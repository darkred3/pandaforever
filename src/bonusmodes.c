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
#include <string.h>
#include <stdlib.h>
#include <curl/curl.h>
#include <strings.h>
#include "panda.h"
#include "mischelp.h"
#include "metadatawrite.h"
#include "curlhelper.h"
#include "configreader.h"
#include "main.h"
#include "lose.h"
// getmetadata <api/eze> [url 1] [out file 1]
static int getMetadataMode(int argc, char** argv, CURL* c){
	int _ret=0;
	char _isApi = strcmp(argv[0],"api")==0;
	for (int i=1;i<argc;i+=2){
		const char* _link = argv[i];
		const char* _out = argv[i+1];
		char* _apiMetadata = getApiMetadataLink(c,_link);
		if (!_apiMetadata){
			fprintf(stderr,"failed to get metadata for %s\n",_link);
			_ret=1;
			continue;
		}
		if (_isApi){
			_ret|=buffToFile(_out, _apiMetadata);
		}else{
			char* _firstPageHtml = curlGetMem_stripNulls(c,_link);
			if (_firstPageHtml){
				_ret|=parsetometadata(_firstPageHtml,_apiMetadata,_out);
				free(_firstPageHtml);
			}else{
				fprintf(stderr,"failed to get %s\n",_link);
				_ret=1;
			}
		}
		free(_apiMetadata);
	}
	return _ret+1;
}
int easyCommentsToFile(CURL* c, const char* _url, const char* _outFilename){
	int _ret=0;
	char* _fixedUrl1 = trimGalleryUrlGarbage(_url);
	char* _fixedUrl2=generateUrlWithComments(_fixedUrl1);
	char* _html = curlGetMem_stripNulls(c,_fixedUrl2);
	if (_html){
		_ret|=parseToComments(_html,_outFilename);
	}else{
		_ret=1;
	}
	free(_html);
	free(_fixedUrl1);
	free(_fixedUrl2);
	return _ret;
}
// getcomments [url 1] [out file 1]
static int getCommentsMode(int argc, char** argv, CURL* c){
	int _ret=0;
	for (int i=0;i<argc;i+=2){
		easyCommentsToFile(c,argv[i],argv[i+1]);
	}
	return _ret+1;
}
static int resetLimitMode(int argc, char** argv, CURL* c){
	return 1+lowResetLimit(c);
}
char* bonusModeNames[]={"getmetadata","resetlimit","getcomments"};
int (*bonusModeFuncs[])(int,char**,CURL*)={getMetadataMode,resetLimitMode,getCommentsMode};
char bonusRequiresCurl[] = {1,1,1};
int bonusmodes(int argc, char** argv){
	char* _username=NULL;
	char* _password=NULL;
	char* _cookiefile=DEFAULTCOOKIEFILE;
	const char* _optionNames[] = {"username","password","cookiefile"};
	char _optionTypes[] = {CONFIGTYPE_STR,CONFIGTYPE_STR,CONFIGTYPE_STR};
	void* _optionDests[] = {&_username,&_password,&_cookiefile};
	int _listArgIndex;
	for (_listArgIndex=2;_listArgIndex<argc;_listArgIndex+=2){
		if (!isarg(argv[_listArgIndex])){
			break;
		}
		if (parseSingleOption(argv[_listArgIndex]+2,argv[_listArgIndex+1],sizeof(_optionNames)/sizeof(char*),_optionNames,_optionDests,_optionTypes)){
			return 2;
		}
	}
	for (int i=0;i<sizeof(bonusModeNames)/sizeof(char*);++i){
		if (strcasecmp(argv[1],bonusModeNames[i])==0){
			CURL* c = NULL;
			if (bonusRequiresCurl[i]){
				c = newcurl();
				if (exlogin(c,_username,_password,_cookiefile)){
					return 2;
				}
			}
			int _ret=bonusModeFuncs[i](argc-_listArgIndex,argv+_listArgIndex,c);
			if (c){
				curl_easy_cleanup(c);
				curl_global_cleanup();
			}
			return _ret;
		}
	}
	fprintf(stderr,"unknown bonus mode. check your first argument.\n");
	return 2;
}
