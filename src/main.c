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
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>
#include <unistd.h>
#include <signal.h>
#include <errno.h>
#include <curl/curl.h>
#include "curlhelper.h"
#include "panda.h"
#include "mischelp.h"
#include "configreader.h"
#include "lose.h"
#include "metadatawrite.h"
#include "main.h"
#include "bonusmodes.h"
#include "hooks.h"

#define DEFAULTROOTOUTDIR "./"
#define CONFIGFILENAME "pandaforevercfg.txt" // oh well i guess ill keep the extension for windows users

char* postinitHook;
char* listFinishedHook;
char* noerrorsHook;
char* sadEndHook;

//#ifndef LOSE32
//#define ENABLEHOOKCONFIG
//#endif

static void catchExit(int signo){
	(void)signo;
	shutdownRequested=1;
}
int writeContinueList(const char* _destFilename, char* _remainingFileContents){
	FILE* outfp = fopen(_destFilename,"wb");
	if (!outfp){
		perror(_destFilename);
		return 1;
	}
	// if there is a continue page
	if (lastContinuePageNum!=-2){
		if (lastContinuePageNum==-1){ // it's a link to a gallery itself
			fprintf(outfp,CONTINUEGALLERYFORMAT,lastContinuePageGid,lastContinuePageKey);
		}else{
			fprintf(outfp,CONTINUEPAGEFORMAT,lastContinuePageKey,lastContinuePageGid,lastContinuePageNum);
		}
	}
	fputc('\n',outfp);
	// copy rest of the file
	int _ret=0;
	if (fwrite(_remainingFileContents,strlen(_remainingFileContents),1,outfp)!=1){
		_ret=1;
		perror("writeContinueList fwrite");
	}
	fclose(outfp);
	return _ret;
}
const char* propNames[]={"username","password","rootout","cookiefile","log","waitmin","waitmax","galleryidonlyfolder","maxgallerydirlen","retrywaitseconds","maxsameserverretries","maxpageisnotloadingtrys","max503requests","canresetlimit","disableunicodeslash","maximagefilenamelen","disableunicodereplace","wsloverride","outputlist","apimetadata","ezestylemetadata","dirformatid","maxfiledownloads","useragent","unavailablelist","savecomments","evenmorestrict"
	#ifdef ENABLEHOOKCONFIG
	,"hookversion","onImgDownloadHook","preLimitResetHook","postLimitResetHook","onGalleryDoneHook","postinithook","listfinishedhook","happyexithook","sadexithook"
	#endif
};
char propTypes[]={CONFIGTYPE_STR,CONFIGTYPE_STR,CONFIGTYPE_STR,CONFIGTYPE_STR,CONFIGTYPE_INT,CONFIGTYPE_DOUBLE,CONFIGTYPE_DOUBLE,CONFIGTYPE_YN,CONFIGTYPE_INT,CONFIGTYPE_INT,CONFIGTYPE_INT,CONFIGTYPE_INT,CONFIGTYPE_INT,CONFIGTYPE_YN,CONFIGTYPE_YN,CONFIGTYPE_INT,CONFIGTYPE_YN,CONFIGTYPE_YN,CONFIGTYPE_STR,CONFIGTYPE_YN,CONFIGTYPE_YN,CONFIGTYPE_CHAR,CONFIGTYPE_INT,CONFIGTYPE_STR,CONFIGTYPE_STR,CONFIGTYPE_YN,CONFIGTYPE_YN
	#ifdef ENABLEHOOKCONFIG
	,CONFIGTYPE_INT,CONFIGTYPE_STR,CONFIGTYPE_STR,CONFIGTYPE_STR,CONFIGTYPE_STR,CONFIGTYPE_STR,CONFIGTYPE_STR,CONFIGTYPE_STR,CONFIGTYPE_STR
	#endif
};
int validArgsCount = sizeof(propNames)/sizeof(char*);
void printhelp(const char* _exePath){
	const char* _capUnit;
	_capUnit = windowsFilesystem ? "chars" : "bytes";
	const char* _propDescriptions[]={NULL,NULL,"dir path","file path", "-1 to 3","seconds","seconds",NULL,_capUnit,NULL,NULL,NULL,NULL,NULL,NULL,_capUnit,NULL,NULL,NULL,NULL,NULL,"id",NULL,NULL,"path",NULL,NULL
		#ifdef ENABLEHOOKCONFIG
		,"number",NULL,NULL,NULL,NULL,NULL,NULL,NULL,NULL
		#endif
	};
	if (sizeof(_propDescriptions)/sizeof(char*)!=validArgsCount){
		fprintf(stderr,"_propDescriptions wrong size!\n");
		exit(1);
	}
	fprintf(stderr,"%s [OPTIONS...] [URLS...]\n",_exePath);
	for (int i=0;i<validArgsCount;++i){
		const char* _desc;
		if (!(_desc=_propDescriptions[i])){
			if (propTypes[i]==CONFIGTYPE_YN){
				_desc="y/n";
			}else{
				_desc=propNames[i];
			}
		}
		fprintf(stderr,"\t --%s <%s>\n",propNames[i],_desc);
	}
	fprintf(stderr,"\t -i <filename> (must be last option)\n");
	fputs("Options must come before urls.\nrootout's parent directories must exist.\n",stderr);
	#ifdef LOSE32
	fputs("Read manual_windows.txt for more information.\n",stderr);
	#else
	fputs("read the man page for more information.\n",stderr);
	#endif
	char* _fullConfigPath = getConfigFilePath(CONFIGFILENAME);
	if (_fullConfigPath){
		fprintf(stderr,"config file goes at: %s\n",_fullConfigPath);
	}else{
		fprintf(stderr,"failed to get config path\n");
	}
	#if DISABLESECRETCLUB
	#warning disabling secret club
	fputs("DISABLESECRETCLUB\n",stderr);
	#endif
	#ifdef __DATE__
	fprintf(stderr,"<%s>\n",__DATE__);
	#endif
}
void applyWSLLimits(){
	#ifndef LOSE32
	printfl(LOGLVONE,"Warning: WSL enabled. Settings Windows filesystem limitations.\n");
	#endif
	windowsFilesystem=1;
	maxGallerySubdirLen=WINDOWSDEFAULTSUBDIRLEN;
	// NOTE: Be careful setting variables here. This function is executed after the config file is read, so it will overwrite settings.
}
// i want to be stepped on by a cute girl
int main(int argc, char** argv){
	if (argc==1 || (argc==2 && strcasecmp(argv[1],"--help")==0)){
		printhelp(argc>0 ? argv[0] : "name");
		return 1;
	}
	if (argc>=2){
		if (!isarg(argv[1]) && !startswith(argv[1],"http") && !(strcmp(argv[1],"-i")==0)){
			// bonus modes of operation
			int _maybe=bonusmodes(argc,argv);
			if (_maybe){
				return _maybe-1;
			}
		}
	}
	CURL* c = newcurl();
	// additional argument parsing goes here
	int _listArgIndex=1;
	char* _outputList=NULL;
	char* _inUsername=NULL;
	char* _inPassword=NULL;
	char* _rootOutDir=DEFAULTROOTOUTDIR;
	char* _cookieFile=DEFAULTCOOKIEFILE;
	{
		char _disableUnicodeSlash=0;
		signed char _overrideWSL=-1;
		char _galleryIdOnlyFolder=0;
		int _configmaxGallerySubdirLen=-1;
		void* _propDests[]={&_inUsername,&_inPassword,&_rootOutDir,&_cookieFile,&logLevel,&stdSleepMin,&stdSleepMax,&_galleryIdOnlyFolder,&_configmaxGallerySubdirLen,&retryWaitSeconds,&maxSameServerRetries,&maxPageIsNotLoadingTries,&max503Requests,&canResetLimit,&_disableUnicodeSlash,&maxImageFilenameLen,&disableUnicodeReplace,&_overrideWSL,&_outputList,&getApiMetadata,&getHtmlMetadata,&chosenDirFormat,&fileDownloadsLeft,&useragentStr,&unavailableList,&shouldDownloadComments,&evenmorestrict
			#ifdef ENABLEHOOKCONFIG
			,&hookVer,&onImageDownloadHook,&preLimitResetHook,&postLimitResetHook,&onGalleryDownloadDoneHook,&postinitHook,&listFinishedHook,&noerrorsHook,&sadEndHook
			#endif
		};
		if (sizeof(_propDests)/sizeof(void*)!=validArgsCount || sizeof(propTypes)!=validArgsCount){
			fprintf(stderr,"argument array info wrong size!\n");
			return 1;
		}
		{
			char* _fullConfigPath = getConfigFilePath(CONFIGFILENAME);
			if (_fullConfigPath){
				if (access(_fullConfigPath,F_OK)==0){
					printfl(LOGLVONE,"Reading config file...\n");
					readConfig(_fullConfigPath,validArgsCount,propNames,_propDests,propTypes);
				}
				free(_fullConfigPath);
			}else{
				fprintf(stderr,"failed to get config path\n");
			}
		}
		// allow options from command line too
		while(_listArgIndex<argc){
			if (isarg(argv[_listArgIndex])){
				if (strcasecmp(argv[_listArgIndex],"--login")==0){
					_inUsername=argv[_listArgIndex+1];
					_inPassword=argv[_listArgIndex+2];
					_listArgIndex+=3;
					continue;
				}else if (strcasecmp(argv[_listArgIndex],"--waitrange")==0){
					if (strodfail(argv[_listArgIndex+1],&stdSleepMin) || strodfail(argv[_listArgIndex+2],&stdSleepMax)){
						fprintf(stderr,"number parse failed\n");
						return 1;
					}
					if (stdSleepMin<=0 || stdSleepMax<=0){
						fprintf(stderr,"======\nWARNING\n======\nOne of your sleep limits is 0. You will get banned.\n");
					}
					_listArgIndex+=3;
					continue;
				}
				if (parseSingleOption(argv[_listArgIndex]+2,argv[_listArgIndex+1],validArgsCount,propNames,_propDests,propTypes)){
					return 1;
				}else{
					_listArgIndex+=2;
				}
			}else{
				break;
			}
		}
		if (_galleryIdOnlyFolder){
			chosenDirFormat=2;
		}
		if ((_overrideWSL==-1 && isWSL()) || _overrideWSL==1){
			applyWSLLimits();
		}
		// need to put this after in case applyWSLLimits broke the user setting
		if (_configmaxGallerySubdirLen!=-1){
			maxGallerySubdirLen=_configmaxGallerySubdirLen;
		}
		if (_disableUnicodeSlash && !disableUnicodeReplace){
			fprintf(stderr,"Warning: Upgrading disableUnicodeSlash to disableUnicodeReplace\n");
			disableUnicodeReplace=1;
		}
		if (stdSleepMax<stdSleepMin){
			fprintf(stderr,"sleep max less than sleep min\n");
			return 1;
		}
		if ((_inUsername!=NULL || _inPassword!=NULL) && (_inUsername==NULL || _inPassword==NULL)){
			fprintf(stderr,_inUsername ? "Only username provided\n" : "Only password provided\n");
			return 1;
		}
		{
			char* m = forceEndInSlashMaybe(_rootOutDir);
			if (m){
				_rootOutDir=m;
			}
		}
		//
		{ // check for arguments put after the last one
			for (int i=_listArgIndex;i<argc;++i){
				if (isarg(argv[i])){
					fprintf(stderr,"all arguments must come before -i and before input urls. move %s\n",argv[i]);
					exit(1);
				}
			}
		}
		{
			// if hooks, ensure _chosenHookVer
			if (onImageDownloadHook || preLimitResetHook || postLimitResetHook || onGalleryDownloadDoneHook || postinitHook || listFinishedHook || noerrorsHook || sadEndHook){
				if (hookVer==-1){
					fprintf(stderr,"set hookversion if you want to use hooks\n");
					return 1;
				}
			}
		}
		//
		if (!dirExists(_rootOutDir)){
			if (mkdirGoodSettngs(_rootOutDir)){
				fprintf(stderr,"mkdir for root out dir %s\n",_rootOutDir);
				perror(NULL);
				return 1;
			}
		}
	}
	if (exlogin(c,_inUsername,_inPassword,_cookieFile)){
		fprintf(stderr,"failed to get login cookies\n");
		return 1;
	}
	//
	#ifdef LOSE32
	signal(SIGINT,catchExit);
	signal(SIGTERM,catchExit);
	#else
	struct sigaction sa;
	memset(&sa,0,sizeof(struct sigaction));
	sa.sa_handler=catchExit;
	if (sigaction(SIGINT,&sa,NULL) || sigaction(SIGTERM,&sa,NULL)){
		perror("sigaction");
		return 1;
	}
	#endif
	//
	if (runprogramPrintBadRet(postinitHook,NULL,0)){
		return 1;
	}
	//
	int i;
	for (i=_listArgIndex;i<argc;++i){
		if (strcmp(argv[i],"-i")==0){
			++i;
			FILE* fp = fopen(argv[i],"rb");
			if (!fp){
				perror(argv[i]);
				break;
			}
			char _didSadBreak=0;
			char* _line=NULL;
			size_t _size=0;
			while(1){
				errno=0;
				if (getline(&_line,&_size,fp)==-1){
					if (errno){
						perror("getline");
						_didSadBreak=1;
					}
					break;
				}
				if (_line[0]=='#' || isBlankLine(_line)){
					continue;
				}else if (!startswith(_line,"http")){
					fprintf(stderr,"invalid line %s\n",_line);
					_didSadBreak=1;
					break;
				}else{
					removeLineBreak(_line);
					_didSadBreak = (nabGallery(c,_line,_rootOutDir));
					// NOTE: this is a bit of a bad way of doing this
					// on any non-weird error, the continue page will be be put into the lastContinuePageGid variable. so we check that variable.
					// if that variable is not set, then the gallery either downloaded successfully or exited in a weird way.
					// but because we sometimes want to write the continue list in weird ways, there's a bunch of negative
					// number signals with lastContinuePageNum
					if (lastContinuePageGid && _outputList){
						char* _remainingBuff = readRemainingToBuff(fp);
						if (_remainingBuff){
							fclose(fp);
							fp=NULL;
							if (writeContinueList(_outputList,_remainingBuff)){
								fprintf(stderr,"failed to write continue list to %s\n",_outputList);
								_didSadBreak=1;
							}else{
								printfl(LOGLVONE,"Wrote continue list to %s\n",_outputList);
							}
							free(_remainingBuff);
						}else{
							fprintf(stderr,"failed to read the rest of the file to write the continue list.\n");
						}
					}
					if (_didSadBreak){
						break;
					}
				}
			}
			free(_line);
			if (fp){
				fclose(fp);
			}
			if (_didSadBreak){
				break;
			}
			if (runprogramOneArg(listFinishedHook,argv[i],1)){
				break;
			}
		}else{
			if (nabGallery(c,argv[i],_rootOutDir) || shutdownRequested){
				break;
			}
		}
	}
	int _ret=(i!=argc && !shutdownRequested);
	if (!_ret){
		_ret=runprogramPrintBadRet(noerrorsHook,NULL,0);
	}
	if (_ret){
		if (runprogramPrintBadRet(sadEndHook,NULL,0)){
			fprintf(stderr,"just when i thought it couldnt get any sadder\n");
		}
	}
	if (_ret){
		fprintf(stderr,"break due to error\n");
	}else if (i==argc){
		printfl(LOGLVONE,"happy exit\n");
	}
	if (isLoggedIn){
		curl_easy_setopt(c,CURLOPT_COOKIEJAR,_cookieFile);
	}
	curl_easy_cleanup(c);
	curl_global_cleanup();
	return _ret;
}
