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
#define NOSTUPIDWINDOWSMACROS
#include "lose.h"

char windowsFilesystem=0;

#ifdef LOSE32
#include <stdlib.h>
#include <stdio.h>
#include <unistd.h>
#include <errno.h>
#include <string.h>
#include <iconv.h>
#include <windows.h>

char* windowsGetExeLocation(){
	int _outBuffBytes=262;
	while(1){
		char* _pathDest=malloc(_outBuffBytes);
		GetModuleFileNameA(NULL,_pathDest,_outBuffBytes/sizeof(TCHAR));
		if (GetLastError()==ERROR_INSUFFICIENT_BUFFER){
			free(_pathDest);
			_outBuffBytes*=2;
		}else{
			return _pathDest;
		}
	}
}
char* windowsPathNextToExe(const char* _filename){
	char* _exeLocation=windowsGetExeLocation();
	for (int i=strlen(_exeLocation)-1;;--i){
		if (_exeLocation[i]=='\\'){
			_exeLocation[i+1]='\0';
			char* _ret=malloc(strlen(_exeLocation)+strlen(_filename)+1);
			strcpy(_ret,_exeLocation);
			strcat(_ret,_filename);
			free(_exeLocation);
			return _ret;
		}
	}
}

#define ENCODINGCANFAIL 0
iconv_t textEncoder;
static wchar_t* utf8ToWindows(const char* _sourceTextBla){
	char* _sourceText = (char*)_sourceTextBla;
	iconv(textEncoder,NULL,NULL,NULL,NULL); // reset it
	size_t _inBytes=strlen(_sourceText);
	size_t _outBuffSize=_inBytes;
	char* _outBuff=malloc(_outBuffSize+2);
	char* _movingOutBuff=_outBuff; // to calculate how much has been used
	size_t _outBytesFree=_outBuffSize;
	while(1){
		if (_inBytes==0){
			if (_sourceText){
				_sourceText=NULL; // make the final text with *inbuf equal to NULL
			}else{
				break;
			}
		}
		errno=0;
		if (iconv(textEncoder,&_sourceText,&_inBytes,&_movingOutBuff,&_outBytesFree)==-1){
			if (errno==E2BIG){
				size_t _usedBytes=_outBuffSize-_outBytesFree;
				// get an additional _outBuffSize bytes
				_outBytesFree=_outBytesFree+_outBuffSize;
				_outBuffSize*=2;
				// realloc the buffer and move the moving thing accordingly
				_outBuff=realloc(_outBuff,_outBuffSize+2);
				_movingOutBuff=_outBuff+_usedBytes;
				continue;
			}else{
				perror(NULL);
				fprintf(stderr,"Failed to convert encoding\n");
				#if ENCODINGCANFAIL == 0
				exit(1);
				#else
				free(_outBuff); // return what was given
				goto noencoding;
				#endif
			}
		}
	}
	_movingOutBuff[0]='\0';
	_movingOutBuff[1]='\0';
	return (wchar_t*)_outBuff;
}
static char _encoderInitialized=0;
wchar_t* makeWindowsString(const char* _sourceText){
	if (!_encoderInitialized){
		textEncoder = iconv_open("UTF-16le","UTF-8");
		_encoderInitialized=1;
	}
	return utf8ToWindows(_sourceText);
}
FILE* losedowsfopen(const char* pathname, const char* mode){
	wchar_t* _stupidpath = makeWindowsString(pathname);
	wchar_t* _stupidMode = makeWindowsString(mode);
	FILE* _ret = _wfopen(_stupidpath,_stupidMode);
	free(_stupidpath);
	free(_stupidMode);
	return _ret;
}
int losedowsrename(const char* old, const char* new){
	wchar_t* _oldfixed = makeWindowsString(old);
	wchar_t* _newfixed = makeWindowsString(new);
	int _ret = _wrename(_oldfixed,_newfixed);
	free(_oldfixed);
	free(_newfixed);
	return _ret;
}
int losedowsremove(const char* _pathorig){
	wchar_t* _pathfixed = makeWindowsString(_pathorig);
	int _ret = _wremove(_pathfixed);
	free(_pathfixed);
	return _ret;
}
int losedowsmkdir(const char* _pathorig, mode_t mode){
	wchar_t* _pathfixed = makeWindowsString(_pathorig);
	int _ret = _wmkdir(_pathfixed);
	free(_pathfixed);
	return _ret;
}
int losedowsdirExists(const char* _dir){
	int _inDirLen=strlen(_dir);
	char* _noSlashEnd;
	if (_dir[_inDirLen-1]=='/'){ // windows is sad if the dir ends in a slash
		_noSlashEnd=strdup(_dir);
		_noSlashEnd[_inDirLen-1]='\0';
	}else{
		_noSlashEnd=NULL;
	}
	wchar_t* _pathfixed = makeWindowsString(_noSlashEnd ? _noSlashEnd : _dir);
	struct _stat32 sb;
	int ret=(_wstat(_pathfixed,&sb)==0 && (_S_IFDIR & sb.st_mode));
	free(_pathfixed);
	if (_noSlashEnd){
		free(_noSlashEnd);
	}
	return ret;
}
#endif
