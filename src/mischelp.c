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
#include <ctype.h>
#include <string.h>
#include <strings.h>
#include <assert.h>
#include <stdlib.h>
#include <stdio.h>
#include <stdarg.h>
#include <sys/types.h>
#include <unistd.h>
#include <errno.h>
#include <sys/stat.h>
#include <time.h>
#include "fixhtmlgarbage.h"
#include "lose.h"
#include "stolenfunctions.h"
#ifndef LOSE32
#include <pwd.h>
#endif
////////
char disableUnicodeReplace=0;
//////// str help
void removeLineBreak(char* _target){
	int _len = strlen(_target);
	if (!_len){
		return;
	}
	if (_target[_len-1]=='\n'){
		if (_len>=2 && _target[_len-2]=='\r'){
			_target[_len-2]='\0';
		}else{
			_target[_len-1]='\0';
		}
	}
}
char* allocSubstr(const char* _startPosInclusive, const char* _endPosExclusive){
	int _innerLen = _endPosExclusive-_startPosInclusive;
	char* _ret = malloc(_innerLen+1);
	if (!_ret){
		fprintf(stderr,"malloc failed\n");
		return NULL;
	}
	memcpy(_ret,_startPosInclusive,_innerLen);
	_ret[_innerLen]='\0';
	return _ret;
}
// given a start point, look for the first deliminator and return a substring between there and the next deliminator.
char* nabNearDelims(const char* _startPos, char d){
	char* _quotePos = strchr(_startPos,d);
	if (_quotePos==NULL){
		return NULL;
	}
	_quotePos++;
	char* _quoteEndPos = strchr(_quotePos,d);
	if (_quoteEndPos==NULL){
		return NULL;
	}
	return allocSubstr(_quotePos,_quoteEndPos);
}
char* nabQuoted(const char* _startPos){
	return nabNearDelims(_startPos,'"');
}
int isNumStr(const char* s){
	while(1){
		char c=*(s++);
		if (c==0){
			return 1;
		}
		if (!isdigit(c)){
			return 0;
		}
	}
}
char* imageExtensions[] = {".gif",".jpg",".jpeg",".png"};
char* getImageExt(char* s){
	char* _extStart = strrchr(s,'.');
	if (!_extStart){
		return NULL;
	}
	for (int i=0;i<sizeof(imageExtensions)/sizeof(char*);++i){
		if (strcasecmp(_extStart,imageExtensions[i])==0){
			return _extStart;
		}
	}
	return NULL;
}
// includes the dot
int getImageExtLen(char* s){
	char* _ext=getImageExt(s);
	return _ext ? strlen(_ext) : 0;
}
#define SECONDUTF8BYTE(_val) ((_val & 0x80)!=0 && (_val & 0x40)==0)
// if this is a regular char, return the given position
// if this is the middle of a utf-8 char, return the start of that char a byte or two to the left.
static char* getStartOfCharBackwards(char* s){
	while(1){
		if (!SECONDUTF8BYTE(*s)){
			return s;
		}
		s--;
	}
}
static char* getStartOfCharForwards(char* s){
	while(1){
		if (!SECONDUTF8BYTE(*s)){
			return s;
		}
		s++;
	}
}
void capToChars(char* s, size_t _maxChars, int _tailCharsToKeep, char _useEllipsis){
	int _numchars=0;
	while(1){
		char c = *s;
		if (c=='\0'){
			return;
		}
		++_numchars;
		if (_numchars>_maxChars){
			break;
		}
		s=getStartOfCharForwards(s+1);
	}
	char* _cutoffStart=s;
	char* _saveStartPos=s+strlen(s);
	for (int i=0;i<_tailCharsToKeep;++i){
		_saveStartPos=getStartOfCharBackwards(_saveStartPos-1);
		_cutoffStart=getStartOfCharBackwards(_cutoffStart-1);
	}
	if (_useEllipsis && _maxChars-_tailCharsToKeep>3){
		for (int i=0;i<3;++i){
			_cutoffStart=getStartOfCharBackwards(_cutoffStart-1);
		}
		for (int i=0;i<3;++i){
			*(_cutoffStart++)='.';
		}
	}
	memmove(_cutoffStart,_saveStartPos,strlen(_saveStartPos)+1);
}
void capToLen(char* s, size_t _maxLen, int _tailAmountToKeep, char _useEllipsis){
	int _cachedLen = strlen(s);
	if (_cachedLen>_maxLen){
		if (_useEllipsis){
			// this is okay because it's always specified in bytes
			_tailAmountToKeep+=3;
		}
		if (_tailAmountToKeep>0){
			assert(_tailAmountToKeep<=_maxLen);
			char* _maxLenOff=s+_maxLen;
			// find a spot that's not in the middle of a char to shove the end part in
			char* _destCopy=getStartOfCharBackwards(_maxLenOff-_tailAmountToKeep);
			// shove it
			memmove(_destCopy,s+_cachedLen-_tailAmountToKeep,_tailAmountToKeep);
			*(_destCopy+_tailAmountToKeep)='\0';
			if (_useEllipsis){
				// _destCopy is where the end got moved to, and we added three bytes extra to the end
				for (int i=0;i<3;++i){
					*(_destCopy++)='.';
				}
			}
		}else{
			*getStartOfCharBackwards(s+_maxLen)='\0';
		}
	}
}
void capToLenStartEnd(char* s, size_t _maxLen, int _startCharsToKeep, int _endAmountToKeep, char _useEllipsis){
	int _frontChars=0;
	while(1){
		char c=*s;
		if (c=='\0'){ // string is shorter than _startCharsToKeep
			return;
		}
		if (!SECONDUTF8BYTE(c) && (++_frontChars)>_startCharsToKeep){
			#ifdef LOSE32
			capToChars(s,_maxLen-_startCharsToKeep,_endAmountToKeep,_useEllipsis);
			#else
			capToLen(s,_maxLen-_startCharsToKeep,_endAmountToKeep,_useEllipsis);
			#endif
			return;
		}else{
			++s;
		}
	}
}
int startswith(const char* a, const char* b){
	return strncmp(a,b,strlen(b))==0;
}
// a ends with b
int endswith(const char* a, const char* b){
	int _bLen=strlen(b);
	int _aLen=strlen(a);
	if (_aLen>=_bLen){
		return strcmp(a+_aLen-_bLen,b)==0;
	}
	return 0;
}
// 7
// abcde\f
void fixBackslashEscape(char* link){
	for (int _lengthLeft = strlen(link);_lengthLeft>=0;--_lengthLeft,++link){
		char c = *link;
		if (c=='\\'){
			memmove(link,link+1,_lengthLeft); // moves null too
			--_lengthLeft;
		}
	}
}
void fixHtmlLink(char* s){
	fixBackslashEscape(s);
	decode_html_entities_utf8(s,NULL);
}
void fixApiLink(char* s){
	fixBackslashEscape(s);
	fixHtmlLink(s);
}
char* charcat(char* dest, char c){
	int _len = strlen(dest);
	dest[_len++]=c;
	dest[_len]='\0';
	return dest;
}
char* searchCharBackwards(char* str, char c){
	while(*(--str)!=c);
	return str;
}
char* strstrafter(const char* haystack, const char* needle){
	char* _ret = strstr(haystack,needle);
	if (_ret){
		return _ret+strlen(needle);
	}
	return NULL;
}
int printfl(int _shouldDoIt, const char* format, ...){
	if (_shouldDoIt){
		va_list ap;
		va_start(ap, format);
		return vprintf(format,ap);
	}else{
		return 0;
	}
}
static char illegals[] = {'/','<','>',':','"','|','?','*','\\'};
static char* illegalReplacements[] = {"／","＜","＞","：","＂","｜","？","＊","＼"};
static int numIllegal=sizeof(illegals);
static int isIllegal(char c){
	for (int i=0;i<numIllegal;++i){
		if (c==illegals[i]){
			return i+1;
		}
	}
	return 0;
}
char* removeIllegalsIsWindows(char* s, int _isWindows){
	// we only need to the forward slash if not targeting a bad filesystem
	numIllegal=_isWindows ? sizeof(illegals) : 1;
	//
	int _pabloCount=0;
	char* _loopStr=s;
	while (1){
		char c = *(_loopStr++);
		if (c=='\0'){
			break;
		}
		int _isIllegal=isIllegal(c);
		if (_isIllegal){
			++_pabloCount;
		}
	}
	if (_pabloCount==0){
		return strdup(s);
	}
	char* _ret = malloc(strlen(s)+_pabloCount*3+1);
	char* _retPos = _ret;
	_loopStr=s;
	while(1){
		char c = *(_loopStr++);
		int _illegalIndex = isIllegal(c);
		if (_illegalIndex){
			if (disableUnicodeReplace){
				*(_retPos++)=' ';
			}else{
				--_illegalIndex;
				int _utf8Len = strlen(illegalReplacements[_illegalIndex]);
				memcpy(_retPos,illegalReplacements[_illegalIndex],_utf8Len);
				_retPos+=_utf8Len;
			}
		}else{
			*(_retPos++)=c;
			if (c=='\0'){
				break;
			}
		}
	}
	return _ret;
}
char* removeIllegals(char* s){
	return removeIllegalsIsWindows(s,windowsFilesystem);
}
int strodfail(const char* nptr, double* _ret){
	char* _end;
	*_ret = strtod(nptr,&_end);
	return _end==nptr;
}
char* forceEndInSlashMaybe(const char* _str){
	int _len = strlen(_str);
	if (!_len){
		return NULL;
	}
	if (_str[_len-1]!='/'){
		char* _ret = malloc(_len+2);
		memcpy(_ret,_str,_len);
		_ret[_len]='/';
		_ret[_len+1]='\0';
		return _ret;
	}else{
		return NULL;
	}
}
#define DEFAULTCONFIGSUBDIR "/.config/"
char* getConfigFolder(){
	#ifdef LOSE32
	return windowsPathNextToExe("");
	#else
	char* _ret;
	if ((_ret=getenv("XDG_CONFIG_HOME"))==NULL){
		char* _homeDir;
		if ((_homeDir=getenv("HOME"))==NULL) {
			struct passwd* p = getpwuid(getuid());
			if (!p){
				perror(NULL);
				return NULL;
			}
			_homeDir=p->pw_dir;
		}
		_ret = malloc(strlen(_homeDir)+strlen(DEFAULTCONFIGSUBDIR)+1);
		strcpy(_ret,_homeDir);
		strcat(_ret,DEFAULTCONFIGSUBDIR);
		return _ret;
	}else{
		char* _maybeSlash = forceEndInSlashMaybe(_ret);
		return _maybeSlash ? _maybeSlash : strdup(_ret);
	}
	#endif
}
int isWSL(){
	#ifdef LOSE32
	return 1;
	#else
	FILE* fp = fopen("/proc/sys/kernel/osrelease","rb");
	if (!fp){
		return 0;
	}
	char* _line=NULL;
	size_t _lineSize=0;
	if (getline(&_line,&_lineSize,fp)==-1){
		free(_line);
		return 0;
	}
	int _ret=(strcasestr(_line,"WSL")!=NULL || strcasestr(_line,"microsoft")!=NULL);
	free(_line);
	return _ret;
	#endif
}
char* readRemainingToBuff(FILE* infp){
	int _retSize=1;
	int _retUsed=0;
	char* _ret = malloc(_retSize+1);
	errno=0;
	while(1){
		int c = fgetc(infp);
		if (c==EOF){
			if (errno!=0){
				perror(NULL);
			}
			break;
		}
		if (_retUsed>=_retSize){
			_retSize*=2;
			_ret=realloc(_ret,_retSize+1);
		}
		_ret[_retUsed++]=c;
	}
	if (errno!=0){
		free(_ret);
		return NULL;
	}else{
		_ret[_retUsed]='\0';
		return _ret;
	}
}
char* fileToBuff(const char* filename){
	FILE* fp = fopen(filename,"rb");
	if (!fp){
		perror("fileToBuff fopen");
		return NULL;
	}
	char* _ret = readRemainingToBuff(fp);
	fclose(fp);
	return _ret;
}
int buffToFile(const char* _outFilename, const char* buff){
	FILE* fp = fopen(_outFilename,"wb");
	if (!fp){
		perror("buffToFile");
		return 1;
	}
	int _ret=0;
	int _len = strlen(buff);
	if (_len!=0){
		if (fwrite(buff,_len,1,fp)!=1){
			perror(NULL);
			_ret=1;
		}
	}
	fclose(fp);
	return _ret;
}
void timespecFromSec(struct timespec* _ret, double sec){
	_ret->tv_sec=(int)sec;
	_ret->tv_nsec=(sec-(int)sec)*1000000000;
}
double randDouble(double _min, double _max){
	if (_min==_max || _max<_min){
		return _min;
	}
	double _range=_max-_min;
	return (rand()/(double)RAND_MAX)*_range+_min;
}
void easysleep(double seconds){
	struct timespec rqtp;
	timespecFromSec(&rqtp,seconds);
	if (nanosleep(&rqtp,NULL)!=0){
		fprintf(stderr,"nanosleep failed\n");
		perror(NULL);
	}
}
#ifndef LOSE32
int dirExists(const char* _dir){
	struct stat sb;
	return (stat(_dir,&sb)==0 && S_ISDIR(sb.st_mode));
}
#endif
int mkdirGoodSettngs(const char *path){
	return mkdir(path,S_IRUSR | S_IWUSR | S_IXUSR);
}
int isarg(const char* s){
	return startswith(s,"--");
}
int isBlankLine(const char* a){
	while(1){
		char c = *(a++);
		if (!isblank(c)){
			if (c=='\0' || c==0x0a || (c==0x0d && *a==0x0a)){
				return 1;
			}
			return 0;
		}
	}
}
