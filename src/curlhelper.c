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
#include <stddef.h>
#include <unistd.h>
#include <assert.h>
#include <curl/curl.h>
#include "lose.h"

#ifdef LOSE32
#define CERTFILENAME "ca-certificates.crt"
static char* cachedCertPos=NULL;
char* getCertPos(){
	if (cachedCertPos){
		return cachedCertPos;
	}
	return (cachedCertPos=windowsPathNextToExe(CERTFILENAME));
}
#endif

// very sad that this is global so we cant have multiple curl
// if only there was a way to get curl's error buffer given its object
char curlErrorBuff[CURL_ERROR_SIZE];
char* useragentStr="Mozilla/5.0 (Windows NT 10.0; Win64; x64) AppleWebKit/537.36 (KHTML, like Gecko) Chrome/85.0.4183.102 Safari/537.36";

// does not clear cookies
void curlResetSettings(CURL* c){
	curl_easy_reset(c);
	curl_easy_setopt(c,CURLOPT_USERAGENT,useragentStr);
	curl_easy_setopt(c,CURLOPT_REFERER,"https://exhentai.org");
	curl_easy_setopt(c,CURLOPT_ERRORBUFFER, curlErrorBuff);
	curl_easy_setopt(c,CURLOPT_CONNECTTIMEOUT, 15);
	#ifdef LOSE32
	// todo - have not tried this with anything but 7-bit ascii. maybe if I convert utf-16 to utf-8, it will work for this function?
	curl_easy_setopt(c, CURLOPT_CAINFO, getCertPos());
	#endif
}
int curlDownloadFile(CURL* c, const char* _url, FILE* fp){
	curl_easy_setopt(c,CURLOPT_URL,_url);
	curl_easy_setopt(c,CURLOPT_WRITEFUNCTION,fwrite);
	curl_easy_setopt(c,CURLOPT_WRITEDATA,fp);
	CURLcode res = curl_easy_perform(c);
	curlResetSettings(c);
	if (res!=CURLE_OK){
		fprintf(stderr,"failed to get %s\n",_url);
		fprintf(stderr,"%s\n",curlErrorBuff);
	}
	return res;
}
struct curlDownloadMem {
	char* buff;
	size_t size;
	size_t used;
};
static size_t writeMemoryCallback(void* contents, size_t _uselessvar, size_t bytes, void* data){
	struct curlDownloadMem* m = data;
	if (m->used+bytes>m->size){
		size_t _newSize=m->size;
		do{
			_newSize=(_newSize+1)*2;
		}while(_newSize<m->used+bytes);
		m->size=_newSize;
		m->buff=realloc(m->buff,m->size+1);
		if (!m->buff){
			fprintf(stderr,"realloc failed\n");
			return 0;
		}
	}
	memcpy(m->buff+m->used,contents,bytes);
	m->used+=bytes;
	return bytes;
}
struct curlDownloadMem curlGetMem_withSize(CURL* c, const char* _url){
	curl_easy_setopt(c,CURLOPT_URL,_url);
	struct curlDownloadMem holdmem;
	holdmem.size=0;
	holdmem.used=0;
	holdmem.buff=NULL;
	curl_easy_setopt(c,CURLOPT_WRITEFUNCTION,writeMemoryCallback);
	curl_easy_setopt(c,CURLOPT_WRITEDATA,&holdmem);
	CURLcode res = curl_easy_perform(c);
	curlResetSettings(c);
	if (res!=CURLE_OK){
		fprintf(stderr,"failed to get %s\n",_url);
		fprintf(stderr,"%s\n",curlErrorBuff);
		free(holdmem.buff);
		holdmem.buff=NULL;
		return holdmem;
	}
	if (holdmem.used==0){
		holdmem.buff=calloc(1,1);
	}else{
		holdmem.buff[holdmem.used]='\0';
	}
	return holdmem;
}
char* curlGetMem(CURL* c, const char* _url){
	return curlGetMem_withSize(c,_url).buff;
}
char* curlGetMem_stripNulls(CURL* c, const char* _url){
	struct curlDownloadMem dlres = curlGetMem_withSize(c,_url);
	if (dlres.buff==NULL){
		return NULL;
	}
	char* a=dlres.buff;
	char* endpos=dlres.buff+dlres.used;
	while((a=strchr(a,'\0')) && a<endpos){
		*a=' ';
	}
	return dlres.buff;
}
struct fpcharpair{
	FILE* fp;
	char* altDest;
	int altDestLeft;
};
static size_t curlFileAndMemCallback(void* contents, size_t _uselessvar, size_t bytes, void* data){
	struct fpcharpair* p = data;
	if (p->altDestLeft!=0){
		int _min = p->altDestLeft<bytes ? p->altDestLeft : bytes;
		memcpy(p->altDest,contents,_min);
		p->altDest+=_min;
		p->altDestLeft-=_min;
	}
	return fwrite(contents,_uselessvar,bytes,p->fp);
}
// the first _altDestLen of this download will also go to _altDest in addition to the file.
int curlDownloadFileAlsoPartToMem(CURL* c, const char* _url, FILE* fp, char* _altDest, int _altDestLen){
	struct fpcharpair p;
	p.fp=fp;
	p.altDest=_altDest;
	p.altDestLeft=_altDestLen;
	//
	curl_easy_setopt(c,CURLOPT_URL,_url);
	curl_easy_setopt(c,CURLOPT_WRITEFUNCTION,curlFileAndMemCallback);
	curl_easy_setopt(c,CURLOPT_WRITEDATA,&p);
	CURLcode res = curl_easy_perform(c);
	curlResetSettings(c);
	if (res!=CURLE_OK){
		fprintf(stderr,"failed to get %s\n",_url);
		fprintf(stderr,"%s\n",curlErrorBuff);
	}
	return res;
}
char* makePostString(CURL* c, char** _labels, char** _values, int _num){
	int _valueTotalLen=0;
	int _labelTotalLen=0;
	char* _escaped[_num];
	for (int i=0;i<_num;++i){
		_escaped[i]=curl_easy_escape(c,_values[i],0);
		_valueTotalLen+=strlen(_escaped[i]);
		_labelTotalLen+=strlen(_labels[i]);
	}
	char* _full = malloc(_labelTotalLen+_num+_valueTotalLen+(_num-1)+1);
	char* _fullPos=_full;
	for (int i=0;i<_num;++i){
		if (i!=0){
			*(_fullPos++)='&';
		}
		strcpy(_fullPos,_labels[i]);
		_fullPos+=strlen(_fullPos);
		*(_fullPos++)='=';
		strcpy(_fullPos,_escaped[i]);
		_fullPos+=strlen(_fullPos);
	}
	for (int i=0;i<_num;++i){
		curl_free(_escaped[i]);
	}
	return _full;
}
CURL* newcurl(){
	curl_global_init(CURL_GLOBAL_ALL);
	CURL* c = curl_easy_init();
	curlResetSettings(c);
	curl_easy_setopt(c,CURLOPT_COOKIEFILE,"");
	return c;
}
