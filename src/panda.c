/*
  This program is free software: you can redistribute it and/or modify
  it under the terms of the GNU General Public License as published by
  the Free Software Foundation, version 3.

  This program is distributed in the hope that it will be useful,
  but WITHOUT ANY WARRANTY; without even the implied warranty of
  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
  GNU General Public License for more details.

  You should have received a copy of the GNU General Public License
  along with this program.  If not, see <https://www.gnu.org/licenses/>.
*/
// https://github.com/tommy351/ehreader-android/wiki/E-Hentai-JSON-API
// https://curl.se/mail/tracker-2006-12/0022.html
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>
#include <stddef.h>
#include <unistd.h>
#include <ctype.h>
#include <time.h>
#include <errno.h>
#include <sys/stat.h>
#include <curl/curl.h>
#include <limits.h>
#include "curlhelper.h"
#include "mischelp.h"
#include "fixhtmlgarbage.h"
#include "metadatawrite.h"
#include "panda.h"
#include "lose.h"
#include "hooks.h"
#ifdef LOSE32
#define DEFAULTMAXGALLERYSUBDIRLEN WINDOWSDEFAULTSUBDIRLEN
#define DEFAULTIMAGEFILENAMEMAXLEN _MAX_FNAME
#else
#define DEFAULTMAXGALLERYSUBDIRLEN NAME_MAX
#define DEFAULTIMAGEFILENAMEMAXLEN NAME_MAX
#endif

#define IMAGETYPE_GIF 4 // do not change
#define IMAGETYPE_PNG 5
#define IMAGETYPE_JPG 6

char canResetLimit=1;
int maxGallerySubdirLen=DEFAULTMAXGALLERYSUBDIRLEN;
int maxImageFilenameLen=DEFAULTIMAGEFILENAMEMAXLEN; // this includes PARTEXT
int logLevel=1;
int fileDownloadsLeft=-1;
/*
After about 180 galleries, I was banned with a wait time of 2 seconds.
*/
double stdSleepMin=3;
double stdSleepMax=6;

int retryWaitSeconds = 5;
int maxSameServerRetries = 3;
int maxPageIsNotLoadingTries = 5; // max "page is not loading" requests for a single page
int max503Requests = 300;

#define APILINK "https://"SITEDOMAIN"/api.php"

#define USEPARTEXT 1
#define PARTEXT ".p"
// page number, original filename
// supposedly, galleries are 2000 images max
#define INDIVIDUALPAGEFORMAT "%04d_%s"
#define INDIVIDUALPAGEFORMATKEEPFRONT 4 // goes with above format. we need to keep the first 4 chars, always.

char getHtmlMetadata=1;
char getApiMetadata=0;
#define METADATAAPIFILENAME "inforaw.json"
#define METADATAHTMLFILENAME "info.json"
#define COMMENTSFILENAME "peanutgallery.txt"

char chosenDirFormat=0;
char isLoggedIn=1;
char* unavailableList=NULL;
char shouldDownloadComments=0;
char evenmorestrict=0;

sig_atomic_t shutdownRequested=0;

// hooks
int hookVer=-1;
char* onImageDownloadHook=NULL;
char* preLimitResetHook=NULL;
char* postLimitResetHook=NULL;
char* onGalleryDownloadDoneHook=NULL;

#define BANNEDMESSAGE "Your IP address has been temporarily banned for excessive pageloads which indicates that you are using automated mirroring/harvesting software. The ban expires in"
char isBannedPage(const char* a){
	return startswith(a,BANNEDMESSAGE);
}

#define postImageDownloadSleep standardSleep
#define postResetLimitSleep standardSleep
#define pandaIsDownSleep quadrupleSleep
void standardSleep(){
	easysleep(randDouble(stdSleepMin,stdSleepMax));
}
void quadrupleSleep(){
	for (int i=0;i<4;++i){
		standardSleep();
	}
}
////
// returns 0 if the login worked
// sets the cookies
int exlogin(CURL* c, const char* _username, const char* _password, const char* _cookieFile){
	isLoggedIn=1;
	if (access(_cookieFile,F_OK)==0){
		if (_username || _password){
			fprintf(stderr,"Warning: ignoring username and password and using cached login cookies at %s\n",_cookieFile);
		}
		curl_easy_setopt(c,CURLOPT_COOKIEFILE,_cookieFile);
		return 0;
	}
	if (!_username || !_password){
		#if DISABLESECRETCLUB
		#warning allowing usage without login. original images may not be downloaded!
		isLoggedIn=0;
		return 0;
		#else
		fprintf(stderr,"need username and password for login\n");
		return 1;
		#endif
	}

	int _ret=0;
	printfl(LOGLVONE,"Logging in\n");
	curl_easy_setopt(c,CURLOPT_REFERER,"https://e-hentai.org/bounce_login.php?b=d&bt=1-1");
	char* _postFields;
	{
		char* _postKeys[] = {"CookieDate","b","bt","UserName","PassWord","ipb_login_submit"};
		char* _postValues[] = {"1","d","1-1",(char*)_username,(char*)_password,"Login!"};
		_postFields=makePostString(c,_postKeys,_postValues,sizeof(_postKeys)/sizeof(char*));
		curl_easy_setopt(c,CURLOPT_POSTFIELDS,_postFields);
	}
	char* _response = curlGetMem_stripNulls(c,"https://forums.e-hentai.org/index.php?act=Login&CODE=01");
	_ret=strstr(_response,"You are now logged in as:")==NULL;
	free(_response);
	free(_postFields);
	{
		struct curl_slist *cookies = NULL;
		CURLcode res = curl_easy_getinfo(c, CURLINFO_COOKIELIST, &cookies);
		if(!res && cookies) {
			// a linked list of cookies in cookie file format
			struct curl_slist *each = cookies;
			while(each) {
				// switch all e-hentai cookies to exhentai cookies
				if (strncmp(each->data,".e-hentai.org",strlen(".e-hentai.org"))==0){
					each->data[2]='x';
					curl_easy_setopt(c, CURLOPT_COOKIELIST, each->data);
					printfl(LOGLVTWO,"%s\n",each->data);
				}
				each = each->next;
			}
			curl_slist_free_all(cookies);
		}
	}
	return _ret;
}
int lowResetLimit(CURL* c){
	curl_easy_setopt(c,CURLOPT_POSTFIELDS,"act=limits&reset=Reset Limit");
	curl_easy_setopt(c,CURLOPT_REFERER,"https://e-hentai.org/home.php");
	char* html = curlGetMem_stripNulls(c,"https://e-hentai.org/home.php");
	if (!html){
		fprintf(stderr,"failed to get home.php\n");
		return 1;
	}
	int _ret=(strstr(html,"Image limit was successfully reset.")==NULL);
	if (_ret){
		if (strstr(html,"Insufficient GP/Credits.")!=NULL){
			fprintf(stderr,"you're broke\n");
		}else{
			fprintf(stderr,"failed to reset limit for unknown reason\n");
		}
	}
	free(html);
	return _ret;
}
int resetLimit(CURL* c){
	{
		int _hookRes = runprogram(preLimitResetHook,NULL,0);
		if (_hookRes==2){
			printfl(LOGLVTWO,"skipping limit reset as requested\n");
			return 0;
		}else if (_hookRes){
			fprintf(stderr,"hook returned %d\n",_hookRes);
			return 1;
		}
	}
	if (!canResetLimit){
		printfl(LOGLVONE,"Limit reset disabled.\n");
		return 1;
	}
	printfl(LOGLVONE,"Resetting limit...");
	int _ret=lowResetLimit(c);
	if (_ret==0){
		printfl(LOGLVONE,"reset limit");
		if (runprogramPrintBadRet(postLimitResetHook,NULL,0)){
			_ret=1;
		}else{
			postResetLimitSleep();
		}
	}
	return _ret;
}
// whatever is at the start of the string doesnt matter. only that is has "s/" somewhere
int getInfoFromUrl(const char* _pageUrl, int* _retPageNum, char** _retGid, char** _retImgKey){
	*_retGid=NULL;
	*_retImgKey=NULL;
	{
		char* _keyStartPos=strstr(_pageUrl,"s/");
		if (!_keyStartPos){
			fprintf(stderr,"failed to find key start pos\n");
			goto errret;
		}
		_keyStartPos+=2;
		char* _keyEndPos=strchr(_keyStartPos,'/');
		if (!_keyEndPos){
			fprintf(stderr,"failed to find end slash for image id\n");
			goto errret;
		}
		*_retImgKey = allocSubstr(_keyStartPos,_keyEndPos);
	}
	{
		// gid
		char* _gidStartPos=strrchr(_pageUrl,'/')+1;
		char* _gidEndPos=strchr(_gidStartPos,'-');
		if (!_gidEndPos){
			fprintf(stderr,"failed to find the dash for the page number\n");
			goto errret;
		}
		*_retGid=allocSubstr(_gidStartPos,_gidEndPos);
		// num
		_gidEndPos++;
		if (!isNumStr(_gidEndPos)){
			fprintf(stderr,"end of url %s (%s) is not a number\n",_pageUrl,_gidEndPos);
			goto errret;
		}
		*_retPageNum=atol(_gidEndPos);
	}
	return 0;
errret:
	free(*_retGid);
	free(*_retImgKey);
	return 1;
}
// the _pageUrl could be like:
// https://exhentai.org/s/somekey/galleryid-1
int getInitialInfoFromPage(CURL* c, const char* _pageUrl, int* _retPageNum, char** _retGid, char** _retImgKey, char** _retShowKey, char** _retGalleryTitle){
	printfl(LOGLVONE,"Getting initial info from %s\n",_pageUrl);
	*_retGid=NULL;
	*_retShowKey=NULL;
	*_retImgKey=NULL;
	if (getInfoFromUrl(_pageUrl,_retPageNum,_retGid,_retImgKey)){
		goto errret;
	}
	{
		char* _html = curlGetMem_stripNulls(c,_pageUrl);
		if (!_html){
			fprintf(stderr,"failed to get html for %s\n",_pageUrl);
			goto errret;
		}
		char* _showkeylabel = strstr(_html,"var showkey=");
		if (!_showkeylabel){
			fprintf(stderr,"failed to find showkey\n");
			free(_html);
			goto errret;
		}
		*_retShowKey = nabQuoted(_showkeylabel);

		char* _i1Pos = strstr(_html,"id=\"i1\"");
		char* _h1Start;
		if (!_i1Pos || (!(_h1Start=strstr(_i1Pos,"<h1>")))){
			fprintf(stderr,"failed to extract gallery title\n");
			free(_html);
			goto errret;
		}
		_h1Start+=4;
		char* _h1End = strstr(_h1Start,"</h1>");
		*_h1End='\0';
		*_retGalleryTitle=strdup(_h1Start);
		decode_html_entities_utf8(*_retGalleryTitle,NULL);
		*_h1End='<';
		free(_html);
	}
	return 0;
errret:
	free(*_retGid);
	free(*_retShowKey);
	free(*_retImgKey);
	*_retGid=NULL;
	*_retShowKey=NULL;
	*_retImgKey=NULL;
	return 1;
}
int isOverLimitSDImageLink(const char* _sdLink){
	return startswith(_sdLink,"https://"SITEDOMAIN"/img/");
}
// return:
// 0 - good
// 1 - bad
// 2 - reset limit please.
int getImageLinksFrom(char* _json, char** _retSdLink, char** _retHdLink, const char* _i3Marker, const char* _sdIdentifier, const char* _hdDelim, char _isApi){
	char* _i3startpos=strstr(_json,_i3Marker);
	if (!_i3startpos){
		fprintf(stderr,"couldn't find %s\n",_i3Marker);
		return 1;
	}
	{
		char* _sdImageLinkStart = strstr(_i3startpos,_sdIdentifier);
		if (!_sdImageLinkStart){
			fprintf(stderr,"SD image link not found\n");
			return 1;
		}
		_sdImageLinkStart+=strlen(_sdIdentifier);
		char* _sdLink=nabQuoted(_sdImageLinkStart);
		if (_isApi){
			fixApiLink(_sdLink);
		}else{
			fixHtmlLink(_sdLink);
		}
		if (isOverLimitSDImageLink(_sdLink)){
			free(_sdLink);
			return 2;
		}
		*_retSdLink=_sdLink;
	}
	// maybe there will be a full res link
	*_retHdLink=NULL;
	if (isLoggedIn){
		char* _hdLinkStart = strstr(_json,"fullimg.php");
		if (_hdLinkStart){
			char* _linkEndPos = strstr(_hdLinkStart,_hdDelim);
			while(*(--_hdLinkStart)!='"');
			_hdLinkStart++;
			*_retHdLink = allocSubstr(_hdLinkStart, _linkEndPos);
			if (_isApi){
				fixApiLink(*_retHdLink);
			}else{
				fixHtmlLink(*_retHdLink);
			}
		}
	}else{
		fprintf(stderr,"Warning: not checking for full image link because not logged in.\n");
	}
	return 0;
}
int getImageLinksFromHtml(char* _html, char** _retSdLink, char** _retHdLink){
	return getImageLinksFrom(_html,_retSdLink,_retHdLink,"id=\"i3\"","img id=\"img\"","\"", 0);
}
#define IDENTIFIERFORSDIMAGE 
int getImageLinksFromApi(char* _json, char** _retSdLink, char** _retHdLink){
	return getImageLinksFrom(_json,_retSdLink,_retHdLink,"\"i3\"","id=\\\"img\\\" src=\\","\\\"", 1);
}
#define PAGEISNOTLOADINGFORMAT "https://"SITEDOMAIN"/s/%s/%s-%d?nl=%s"
char* getPageIsNotLoadingHtml(CURL* c, char* _json, const char* _imageKey, const char* _gid, int _curPageNum){
	char* _nlStartPos = strstr(_json,"return nl(");
	if (!_nlStartPos){
		fprintf(stderr,"notloading javascript call not found\n");
		return NULL;
	}
	char* _nlStr = nabNearDelims(_nlStartPos,'\'');
	char* _urlBuff=malloc(strlen(PAGEISNOTLOADINGFORMAT)+strlen(_imageKey)+strlen(_gid)+10+strlen(_nlStr)+1);
	sprintf(_urlBuff,PAGEISNOTLOADINGFORMAT,_imageKey,_gid,_curPageNum,_nlStr);
	char* _ret = curlGetMem_stripNulls(c,_urlBuff);
	free(_urlBuff);
	free(_nlStr);
	return _ret;
}
// gid (string), page num (int), img key (str), show key (str)
#define APIGETPAGEJSON "{\"method\":\"showpage\",\"gid\":%s,\"page\":%d,\"imgkey\":\"%s\",\"showkey\":\"%s\"}"
char* showpageApiRequest(CURL* c, int _pageNum, char* _imgKey, char* _gid, char* _showKey){
	struct curl_slist* headers=NULL;
	headers=curl_slist_append(headers, "Content-Type: application/json");
	curl_easy_setopt(c,CURLOPT_HTTPHEADER,headers);
	char* _fullPostData = malloc(strlen(APIGETPAGEJSON)+strlen(_gid)+10+strlen(_imgKey)+strlen(_showKey)+1);
	if (!_fullPostData){
		fprintf(stderr,"no mem\n");
		return NULL;
	}
	sprintf(_fullPostData,APIGETPAGEJSON,_gid,_pageNum,_imgKey,_showKey);
	curl_easy_setopt(c,CURLOPT_POSTFIELDS,_fullPostData);
	char* _ret = curlGetMem_stripNulls(c, APILINK);
	curl_slist_free_all(headers);
	free(_fullPostData);
	return _ret;
}

char* lastContinuePageGid=NULL;
char* lastContinuePageKey=NULL;
int lastContinuePageNum;
void resetLastContinuePage(){
	if (lastContinuePageGid){
		free(lastContinuePageGid);
	}
	if (lastContinuePageKey){
		free(lastContinuePageKey);
	}
	lastContinuePageGid=NULL;
	lastContinuePageKey=NULL;
}
void printContinuePage(const char* _imageKey, const char* _gid, int _curPageNum){
	resetLastContinuePage();
	lastContinuePageNum=_curPageNum;
	lastContinuePageGid=strdup(_gid);
	lastContinuePageKey=strdup(_imageKey);
	printfl(LOGLVZERO,"Link for the next page: ");
	printfl(LOGLVZERO,CONTINUEPAGEFORMAT,_imageKey,_gid,_curPageNum);
	printfl(LOGLVZERO,"\n");
}
void setContinuePageToGallery(const char* _link){
	resetLastContinuePage();
	lastContinuePageNum=-1;
	getGalleryUrlIdKey(_link,&lastContinuePageGid,&lastContinuePageKey);
}
void setContinuePageToGalleryIsDone(const char* _link){
	resetLastContinuePage();
	lastContinuePageNum=-2;
	lastContinuePageGid=strdup("oppai");
}
static char* imageMagics[] = {"GIF","\xFF\xD8","\x89\x50\x4e\x47\x0d\x0a\x1a\x0a"};
static char imageMagicsTypes[] = {IMAGETYPE_GIF,IMAGETYPE_JPG,IMAGETYPE_PNG};
// 0 - not an image
// >0: one of IMAGETYPE_xxx constants
int getImageDataType(const unsigned char* b){
	for (int i=0;i<sizeof(imageMagics)/sizeof(char*);++i){
		if (memcmp(b,imageMagics[i],strlen(imageMagics[i]))==0){
			return imageMagicsTypes[i];
		}
	}
	return 0;
}
const char* imageTypeToExt(int _imageType){
	switch(_imageType){
		case IMAGETYPE_GIF:
			return ".gif";
		case IMAGETYPE_PNG:
			return ".png";
		case IMAGETYPE_JPG:
			return ".jpg";
	}
	return NULL;
}
// fix this file path to have the proper image extension
// .p extension must already be stripped
char* adjustPathToImageType(char* _inPath, int _imageType){
	int _extOffset;
	char* _curExt = getImageExt(_inPath);
	const char* _expectedExt = imageTypeToExt(_imageType);
	if (_curExt){
		if (strcasecmp(_curExt,_expectedExt)==0){
			return NULL;
		}
		if (strcasecmp(_curExt,".jpeg")==0 && _imageType==IMAGETYPE_JPG){
			return NULL;
		}
		_extOffset=_curExt-_inPath;
	}else{
		_extOffset=strlen(_inPath);
	}
	printfl(LOGLVTHREE,"need to change ext to %s\n",_expectedExt);
	char* _ret = malloc(strlen(_inPath)+4+1);
	memcpy(_ret,_inPath,_extOffset);
	strcpy(_ret+_extOffset,_expectedExt);
	return _ret;
}
#define LIMITREACHEDIMAGERET "\xEF\xBB\xBFYou have exceeded your image viewing limits."
#define LIMITREACHEDIMAGERETALT "You have exceeded your image viewing limits. Note tha"
int isLimitReachedFile(char* _partBuff){
	int _partBuffLen=strlen(LIMITREACHEDIMAGERET);
	return (memcmp(_partBuff,LIMITREACHEDIMAGERET,_partBuffLen)==0
			|| memcmp(_partBuff,LIMITREACHEDIMAGERETALT,_partBuffLen)==0);
}
int isGalleryUnavailable(const char* _listingHtml){
	return (strstr(_listingHtml,"This gallery has been removed or is unavailable.")!=NULL
			|| strstr(_listingHtml,"This gallery is unavailable due to a copyright claim")!=NULL);
}
int appendLineToFile(const char* _destFilename, const char* _appendLine){
	FILE* fp = fopen(_destFilename,"ab");
	if (!fp){
		perror(NULL);
		return 1;
	}
	int _ret=0;
	if (fwrite(_appendLine,strlen(_appendLine),1,fp)!=1 || fputc('\n',fp)==EOF){
		_ret=1;
		perror("appendLineToFile write");
	}
	if (fclose(fp)){
		perror(NULL);
		_ret=1;
	}
	return _ret;
}
/*
0 - (should not happen anymore)
1 - error, should quit
2 - limit reached
3 - max retries used
--------
4 to 6: IMAGETYPE_XXX
*/
int downloadPage(CURL* c, const char* _fullOutFilename, char* _sdLink, char* _hdLink){
	FILE* fp = fopen(_fullOutFilename,"wb");
	if (!fp){
		fprintf(stderr,"failed to open %s for writing\n",_fullOutFilename);
		return 1;
	}else{
		int _partBuffLen=strlen(LIMITREACHEDIMAGERET);
		char _partBuff[_partBuffLen];
		char* _downloadLink = (_hdLink!=NULL ? _hdLink : _sdLink);
		#ifdef LOSE32
		printfl(LOGLVONE,"%s\n",_downloadLink);
		#else
		printfl(LOGLVONE,"%s -> %s\n",_downloadLink,_fullOutFilename);
		#endif
		int _numTries=0;
	retrysame:
		{
			curl_easy_setopt(c, CURLOPT_FOLLOWLOCATION, 1);  // required for fullimg.php
			int _downloadRet=curlDownloadFileAlsoPartToMem(c,_downloadLink,fp,_partBuff,_partBuffLen);
			fclose(fp);
			if (_downloadRet!=CURLE_OK){
				fprintf(stderr,"failed to download file\n");
				if (shutdownRequested){
					return 1;
				}else if (_downloadRet==CURLE_WRITE_ERROR){
					return 1;
				}else{
				doaretry:
					if ((_numTries++)==maxSameServerRetries){
						return 3;
					}else{
						printfl(LOGLVONE,"Retrying in %d seconds...\n",retryWaitSeconds);
						sleep(retryWaitSeconds);
						printfl(LOGLVTWO,"Retrying...\n");
						fp = fopen(_fullOutFilename,"wb");
						if (fp){
							goto retrysame;
						}else{
							fprintf(stderr,"[for retry] failed to open %s for writing\n",_fullOutFilename);
							return 1;
						}
					}
				}
			}else{
				int _isBad=0;
				if (isLimitReachedFile(_partBuff)){
					puts("limit reached by downloaded file");
					_isBad=2; // Limit reached. Caller will fix it.
				}else if (isBannedPage(_partBuff)){
					fprintf(stderr,"You're banned.\n");
					_isBad=1;
				}
				if (_isBad){
					if (remove(_fullOutFilename)){
						fprintf(stderr,"image limit reached, but failed to delete %s\n",_fullOutFilename);
						perror(NULL);
					}
					return _isBad;
				}
				int _downloadedType = getImageDataType((const unsigned char*)_partBuff);
				if (_downloadedType==0){
					fprintf(stderr,"The file downloaded was not an image. Error?\n");
					goto doaretry;
				}
				return _downloadedType;
			}
		}
	}
	return 0;
}
// -1 on fail
int getPageNumberFromApi(const char* _json){
	char* _numStart = strstr(_json,"\"p\":");
	if (!_numStart){ // this is the first place it fails if bad return
		return -1;
	}
	_numStart+=4;
	char* _endPos=strchr(_numStart,',');
	*_endPos='\0';
	int _apiRetPageNum=atoi(_numStart);
	*_endPos=',';
	return _apiRetPageNum;
}
// _imageKey should be malloc and will be freed before return.
// _curPageNum is 1 based
int chainPageDownloads(CURL* c, int _curPageNum, char* _gid, char* _imageKey, char* _showKey, const char* _rootOutDir){
	int _ret=0;
	char* _json=NULL;
	int _notLoadingRequests=0;
	int _done503Requests=0;
	while(1){
		if (shutdownRequested){
			printContinuePage(_imageKey,_gid,_curPageNum);
			_ret=1;
			break;
		}
		free(_json);
		_json = showpageApiRequest(c,_curPageNum,_imageKey,_gid,_showKey);
		if (!_json){
			fprintf(stderr,"api request failed\n");
			printContinuePage(_imageKey,_gid,_curPageNum);
			goto err;
		}
		printfl(LOGLVTHREE,"====\n%s\n===\n",_json);
		if (strcmp(_json,"Page out of range.")==0){ // maybe this check is unnecessary
			fputs(_json,stderr);
			fputc('\n',stderr);
			goto err;
		}else if (isBannedPage(_json)){
			fprintf(stderr,"This is the saddest moment ever.\n");
			fputs(_json,stderr);
			fputc('\n',stderr);
			printContinuePage(_imageKey,_gid,_curPageNum);
			goto err;
		}
		// what is this page number, according to the api?
		{
			int _apiRetPageNum=getPageNumberFromApi(_json);
			if (_apiRetPageNum==-1){
				if (strstr(_json,"Error 503 Backend fetch failed")!=NULL && (++_done503Requests)!=max503Requests){
					printfl(LOGLVONE,"panda is 503. Waiting...\n");
					pandaIsDownSleep();
					continue;
				}else{
					fprintf(stderr,"failed to find page number element\n");
					printContinuePage(_imageKey,_gid,_curPageNum);
					goto err;
				}
			}
			if (_apiRetPageNum!=_curPageNum){
				fprintf(stderr,"_apiRetPageNum and _curPageNum desync detected! api:%d to local:%d\n",_apiRetPageNum,_curPageNum);
				goto err;
			}
			_done503Requests=0;
		}
		// first, download the image for this page
		{
			char* _sdLink;
			char* _hdLink;
			int _getImageRet = getImageLinksFromApi(_json,&_sdLink,&_hdLink);
			if (_getImageRet==2){
				if (resetLimit(c)){
					printContinuePage(_imageKey,_gid,_curPageNum);
					goto err;
				}else{
					continue;
				}
			}else if (_getImageRet==1){
				goto err;
			}
			printfl(LOGLVTWO,"SDlink is \"%s\"\n",_sdLink);
			if (_hdLink){
				printfl(LOGLVTWO,"hd link is \"%s\"\n",_hdLink);
			}else{
				printfl(LOGLVTWO,"no fullimg.php\n");
			}
			char* _fullOutFilename;
			{
				char* _originalFilename = strrchr(_sdLink,'/')+1;
				int _extLen = getImageExtLen(_originalFilename);
				if (_extLen==0){
					fprintf(stderr,"filename does not have extension. \"%s\" from \"%s\"\n",_originalFilename,_sdLink);
					free(_fullOutFilename);
					goto err;
				}
				char* _bonusExt="";
				#if USEPARTEXT == 1
				_bonusExt=PARTEXT;
				#endif
				_fullOutFilename = malloc(strlen(_rootOutDir)+1+strlen(INDIVIDUALPAGEFORMAT)+10+strlen(_originalFilename)+strlen(_bonusExt)+1);
				if (!_fullOutFilename){
					fprintf(stderr,"no mem for dest filename\n");
					goto err;
				}
				strcpy(_fullOutFilename,_rootOutDir);
				*(_fullOutFilename+strlen(_rootOutDir))='/';
				sprintf(_fullOutFilename+strlen(_rootOutDir)+1,INDIVIDUALPAGEFORMAT,_curPageNum,_originalFilename);
				strcat(_fullOutFilename,_bonusExt);
				_extLen+=strlen(_bonusExt);
				if (maxImageFilenameLen!=0){
					capToLenStartEnd(_fullOutFilename+strlen(_rootOutDir)+1, maxImageFilenameLen, INDIVIDUALPAGEFORMATKEEPFRONT, _extLen, 1);
				}
			}
		retrywithnewlink:
			{
				int _downloadRet=downloadPage(c,_fullOutFilename,_sdLink,_hdLink);
				free(_sdLink);
				free(_hdLink);
				if (_downloadRet>=4){ // rename successful download
					_notLoadingRequests=0;
					// the string to use the fix function on.
					// this may or may not be the current filename
					char* _fixTarget=_fullOutFilename;
					if (USEPARTEXT){
						_fixTarget=strdup(_fullOutFilename);
						_fixTarget[strlen(_fixTarget)-strlen(PARTEXT)]='\0';
					}else{
						_fixTarget=_fullOutFilename;
					}
					char* _finalFixed = adjustPathToImageType(_fixTarget, _downloadRet);
					if (!_finalFixed && USEPARTEXT){
						_finalFixed=_fixTarget; // we still need to rename it to get rid of the part extension
					}
					if (_finalFixed){
						if (rename(_fullOutFilename,_finalFixed)){
							fprintf(stderr,"rename %s to %s\n",_fullOutFilename,_finalFixed);
							perror(NULL);
							_downloadRet=1;
						}else{
							if (runprogramOneArg(onImageDownloadHook,_finalFixed,1)){
								_downloadRet=1;
							}
						}
						free(_finalFixed);
					}else{
						if (runprogramOneArg(onImageDownloadHook,_fullOutFilename,1)){
							_downloadRet=1;
						}
					}
					if (USEPARTEXT && _fixTarget!=_finalFixed){
						free(_fixTarget);
					}
					postImageDownloadSleep();
				}
				if (_downloadRet==3){ // retry with new server
					if ((_notLoadingRequests++)==maxPageIsNotLoadingTries){
						fprintf(stderr,"Too many \"image not loading\" requests. Giving up.\n");
						_downloadRet=1;
					}else{
						printfl(LOGLVONE,"\"Click here if the image fails loading\"...\n");
						char* _notLoadingHtml = getPageIsNotLoadingHtml(c,_json,_imageKey,_gid,_curPageNum);
						if (!_notLoadingHtml){
							fprintf(stderr,"failed to get \"page is not loading\" html\n");
							_downloadRet=1;
						}else{
							int _imageGetRes=getImageLinksFromHtml(_notLoadingHtml,&_sdLink,&_hdLink);
							free(_notLoadingHtml);
							if (_imageGetRes){
								_sdLink=NULL;
								_hdLink=NULL;
								_downloadRet=_imageGetRes; // this may be set to 2 to reset the limit
							}else{
								goto retrywithnewlink;
							}
						}
					}
				}
				free(_fullOutFilename);
				if (_downloadRet==2){ // image limit was reached
					if (resetLimit(c)){
						_downloadRet=1;
					}else{
						continue;
					}
				}
				if (_downloadRet==1){ // error
					printContinuePage(_imageKey,_gid,_curPageNum);
					goto err;
				}
			}
		}

		// get the info for the next page
		free(_imageKey);
		_imageKey=NULL;
		{
			// in the i3 line, there's a javascript function like this:
			// return load_image(2, '<key of second image>')
			char* _i3startpos=strstr(_json,"\"i3\"");
			if (!_i3startpos){
				fprintf(stderr,"couldn't find i3\n");
				goto err;
			}
			char* _loadImageCall=strstr(_i3startpos,"load_image(");
			if (!_loadImageCall){
				fprintf(stderr,"couldnt find load_image\n");
				goto err;
			}
			// check the index inside of the load_image call
			// if it's the same as this current page, that means we're on the last page
			{
				char* _numStart=strchr(_loadImageCall,'(');
				_numStart++;
				char* _numEnd=strchr(_numStart,',');
				*_numEnd='\0';
				int _nextPageNum = atoi(_numStart);
				*_numEnd=',';
				if (_nextPageNum==_curPageNum){
					printfl(LOGLVONE,"This is the last page.\n");
					goto ret;
				}
			}
			char* _nextImageKey = nabNearDelims(_loadImageCall,'\'');
			if (!_nextImageKey){
				fprintf(stderr,"couldn't find next image key\n");
				goto err;
			}
			_imageKey=_nextImageKey;
			++_curPageNum;
		}
		if (fileDownloadsLeft!=-1){
			if ((--fileDownloadsLeft)==0){
				printfl(LOGLVONE,"max file downloads reached\n");
				shutdownRequested=1;
			}
		}
	}
	goto ret;
err:
	_ret=1;
ret:
	free(_json);
	free(_imageKey);
	return _ret;
}
char* generateUrlWithComments(const char* _galleryUrl){
	char* _showCommentsUrl = malloc(strlen(_galleryUrl)+6);
	strcpy(_showCommentsUrl,_galleryUrl);
	strcat(_showCommentsUrl,"?hc=1");
	return _showCommentsUrl;
}
int getInitialPage(CURL* c, const char* _galleryUrl, char** _retFirstPageLink, char** _retHtml){
	*_retHtml=NULL;
	printfl(LOGLVONE,"Getting first page of %s...\n",_galleryUrl);
	int _ret;
	*_retFirstPageLink=NULL;
	char* _html;
	if (shouldDownloadComments){
		char* _realUrl=generateUrlWithComments(_galleryUrl);
		_html = curlGetMem_stripNulls(c,_realUrl);
		free(_realUrl);
	}else{
		_html = curlGetMem_stripNulls(c,_galleryUrl);
	}
	if (!_html){
		fprintf(stderr,"failed to get %s\n",_galleryUrl);
		return 1;
	}
	*_retHtml=_html;
	if (isBannedPage(_html)){
		fprintf(stderr,"%s\n",_html);
		goto errret;
	}
	char* _urlStartPos = strstr(_html,"https://"SITEDOMAIN"/s/");
	if (!_urlStartPos){
		fprintf(stderr,"failed to find an https://"SITEDOMAIN"/s/ link\n");
		if (strlen(_html)==0){
			fprintf(stderr,"white page. your ip address may be banned from exhentai. check the igneous cookie.\n");
		}
		goto errret;
	}
	char* _urlEndPos=strchr(_urlStartPos,'"');
	char* _link=allocSubstr(_urlStartPos,_urlEndPos);
	if (!endswith(_link,"-1")){
		fprintf(stderr,"found page link %s isn't to the first page\n",_link);
		free(_link);
		goto errret;
	}
	*_retFirstPageLink=_link;
	_ret=0;
	return _ret;
errret:
	return 1;
}
// gid (string), key (string)
#define APIGETMETADATA "{\"method\": \"gdata\", \"gidlist\": [ [ %s, \"%s\"] ], \"namespace\": 1}"
char* apiGetMetadata(CURL* c, const char* _gid, const char* _key){
	char* _ret=NULL;
	struct curl_slist* headers=NULL;
	headers=curl_slist_append(headers, "Content-Type: application/json");
	curl_easy_setopt(c,CURLOPT_HTTPHEADER,headers);
	char* _fullPostData = malloc(strlen(APIGETMETADATA)+strlen(_gid)+strlen(_key)+1);
	if (!_fullPostData){
		fprintf(stderr,"no mem\n");
		goto err;
	}
	sprintf(_fullPostData,APIGETMETADATA,_gid,_key);
	curl_easy_setopt(c,CURLOPT_POSTFIELDS,_fullPostData);
	_ret=curlGetMem_stripNulls(c, APILINK);
	if (!_ret){
		fprintf(stderr,"get metadata api reqeust failed\n");
	}
err:
	curl_slist_free_all(headers);
	free(_fullPostData);
	return _ret;
}
char* getApiMetadataLink(CURL* c, const char* _link){
	char* _id;
	char* _key;
	getGalleryUrlIdKey(_link,&_id,&_key);
	char* _apiMetadata = apiGetMetadata(c, _id, _key);
	free(_key);
	free(_id);
	return _apiMetadata;
}
void getGalleryUrlIdKey(const char* _galleryLink, char** _retId, char** _retKey){
	char* _gStart = strstr(_galleryLink,"/g/")+3;
	char* _gEnd = strchr(_gStart,'/');
	*_retId = allocSubstr(_gStart,_gEnd);

	char* _keyStart=_gEnd+1;
	char* _keyEnd=strchr(_keyStart,'/');
	if (!_keyEnd){
		*_retKey=strdup(_keyStart);
	}else{
		*_retKey=allocSubstr(_keyStart,_keyEnd);
	}
}
char* genMetadataOutFilename(const char* _filename, const char* _outDir){
	char* _fullPath = malloc(strlen(_outDir)+1+strlen(_filename)+1);
	strcpy(_fullPath,_outDir);
	charcat(_fullPath,'/');
	strcat(_fullPath,_filename);
	return _fullPath;
}

int downloadMetadata(CURL* c, const char* _fullOutDir, const char* _link, char* _firstPageHtml){
	if (getApiMetadata || getHtmlMetadata){
		char* _apiMetadata = getApiMetadataLink(c,_link);
		if (!_apiMetadata){
			return 1;
		}
		int _ret=0;
		if (!_ret && getApiMetadata){
			char* _fullPath = genMetadataOutFilename(METADATAAPIFILENAME,_fullOutDir);
			_ret|=buffToFile(_fullPath, _apiMetadata);
			free(_fullPath);
		}
		if (!_ret && getHtmlMetadata){
			char* _fullPath = genMetadataOutFilename(METADATAHTMLFILENAME,_fullOutDir);
			_ret|=parsetometadata(_firstPageHtml,_apiMetadata,_fullPath);
			free(_fullPath);
		}
		free(_apiMetadata);
		return _ret;
	}
	return 0;
}
// DO NOT CHANGE THESE ID NUMBERS
char* formatGalleryFolder(const char* _rootOutDir, char* _galleryName, const char* _gid){
	if (chosenDirFormat==0 || chosenDirFormat==1){
		// 0: id goes at end
		// 1: id goes at start
		const char* _format = (chosenDirFormat==0) ? "%s [%s]" : "[%s] %s";
		char* _safeGalleryName = removeIllegals(_galleryName);
		char* _fullOutDir = malloc(strlen(_rootOutDir)+strlen(_format)+1+strlen(_safeGalleryName)+strlen(_gid)+1+1);
		strcpy(_fullOutDir,_rootOutDir);
		if (chosenDirFormat==0){
			sprintf(_fullOutDir+strlen(_rootOutDir),_format,_safeGalleryName,_gid);
		}else{
			sprintf(_fullOutDir+strlen(_rootOutDir),_format,_gid,_safeGalleryName);
		}
		free(_safeGalleryName);
		// cap output directory length
		{
			int _galleryIdBytes=strlen(_gid)+2;
			char* _capTarget=_fullOutDir+strlen(_rootOutDir);
			if (chosenDirFormat==0){ // end cap
				capToLenStartEnd(_capTarget,maxGallerySubdirLen,0,_galleryIdBytes,1);
			}else{ // start cap
				capToLenStartEnd(_capTarget,maxGallerySubdirLen,_galleryIdBytes,0,1);
			}
		}
		charcat(_fullOutDir,'/');
		return _fullOutDir;
	}else if (chosenDirFormat==2){ // gallery id only folder
		char* _fullOutDir = malloc(strlen(_rootOutDir)+1+strlen(_gid)+1);
		strcpy(_fullOutDir,_rootOutDir);
		charcat(_fullOutDir,'/');
		strcat(_fullOutDir,_gid);
		charcat(_fullOutDir,'/');
		return _fullOutDir;
	}else{
		return NULL;
	}
}
// the passed url may have some garbage appended to the end
char* trimGalleryUrlGarbage(const char* _galleryUrl){
	if (strstr(_galleryUrl,"/g/")==NULL){
		return strdup(_galleryUrl);
	}
	char* _fixedUrl = malloc(strlen(_galleryUrl)+3);
	int _origCharsCopy;
	char* _garbageStartPos=strrchr(_galleryUrl,'?'); // the passed url may have some garbage appended to the end
	if (_garbageStartPos){
		_origCharsCopy=_garbageStartPos-_galleryUrl;
	}else{
		_origCharsCopy=strlen(_galleryUrl);
	}
	memcpy(_fixedUrl,_galleryUrl,_origCharsCopy);
	_fixedUrl[_origCharsCopy]='\0';
	if (_fixedUrl[_origCharsCopy-1]!='/'){
		charcat(_fixedUrl,'/');
	}
	return _fixedUrl;
}
int nabGalleryNoUrlFix(CURL* c, const char* _link, const char* _rootOutDir){
	resetLastContinuePage();
	char* _firstPageHtml=NULL;
	char* _firstPageLink;
	if (!strstr(_link,SITEDOMAIN)){
		fprintf(stderr,"link does not contain \""SITEDOMAIN"\" and therefore must be invalid:\n");
		fprintf(stderr,"%s\n",_link);
		return 1;
	}else if (strstr(_link,"/s/")!=NULL){ // individual page
		_firstPageLink=strdup(_link);
	}else if (strstr(_link,"/g/")!=NULL){
		if (getInitialPage(c,_link,&_firstPageLink,&_firstPageHtml)){
			int _isRemoved=isGalleryUnavailable(_firstPageHtml);
			free(_firstPageHtml);
			_firstPageHtml=NULL;
			if (_isRemoved){
				if (!unavailableList){
					fprintf(stderr,"%s is unavailable. Quitting because unavailableList not specified.\n",_link);
					setContinuePageToGallery(_link);
					return 1;
				}
				printfl(LOGLVONE,"Adding %s to unavailable gallery list...\n",_link);
				return appendLineToFile(unavailableList,_link);
			}
			setContinuePageToGallery(_link);
		}
		standardSleep();
	}else{
		fprintf(stderr,"don't know what %s is.\n",_link);
		return 1;
	}
	if (!_firstPageLink){
		free(_firstPageHtml);
		return 1;
	}
	int _ret=0;
	int _pageNum;
	char* _gid;
	char* _imgKey;
	char* _showKey;
	char* _galleryName;
	if (!getInitialInfoFromPage(c,_firstPageLink, &_pageNum, &_gid, &_imgKey, &_showKey, &_galleryName)){
		char _shouldNabMetadata=(strstr(_link,"/g/")!=NULL);
		printfl(LOGLVTWO,"%s;%s;%s;%s;%d\n",_gid,_imgKey,_showKey,_galleryName,_pageNum);

		char* _fullOutDir=formatGalleryFolder(_rootOutDir,_galleryName,_gid);
		if (!dirExists(_fullOutDir)){
			if (mkdirGoodSettngs(_fullOutDir)){
				fprintf(stderr,"mdkir %s\n",_fullOutDir);
				perror(NULL);
				_ret=1;
			}else{
				if (!_shouldNabMetadata){
					fprintf(stderr,"warning: downloading to new directory. give the gallery url to download the metadata\n");
				}
			}
		}
		free(_galleryName);
		
		if (_ret==0){
			if (_shouldNabMetadata){
				if (shouldDownloadComments){
					char* _commentsOutFilename = genMetadataOutFilename(COMMENTSFILENAME,_fullOutDir);
					_ret|=parseToComments(_firstPageHtml,_commentsOutFilename);
					free(_commentsOutFilename);
				}
				if (_ret){
					fprintf(stderr,"comments write failed\n");
				}else{
					_ret|=downloadMetadata(c,_fullOutDir,_link,_firstPageHtml);
					if (_ret){
						fprintf(stderr,"metadata download failed\n");
					}
				}
			}
			free(_firstPageHtml);
			if (_ret==0){
				_ret=chainPageDownloads(c, _pageNum, _gid, _imgKey, _showKey,_fullOutDir);
				if (_ret==0){
					if (runprogramOneArg(onGalleryDownloadDoneHook,_fullOutDir,1)){
						_ret=1;
					}
				}
				free(_fullOutDir);
				free(_gid);
				free(_showKey);
			}
		}
	}else{
		free(_firstPageHtml);
		fprintf(stderr,"failed to getInitialInfoFromPage\n");
		_ret=1;
	}
	free(_firstPageLink);
	return _ret;
}
int nabGallery(CURL* c, const char* _link, const char* _rootOutDir){
	char* _fixedLink=trimGalleryUrlGarbage(_link);
	int _ret=nabGalleryNoUrlFix(c,_fixedLink,_rootOutDir);
	free(_fixedLink);
	return _ret;
}
