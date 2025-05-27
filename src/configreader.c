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
#include <errno.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>
#include <ctype.h>
#include "mischelp.h"
#include "configreader.h"
#include "lose.h"

int parseSingleOption(const char* _key, const char* _val, int _numOptions, const char** _destKeys, void** _destVars, char* _destTypes){
	for (int i=0;i<_numOptions;++i){
		if (strcasecmp(_key,_destKeys[i])==0){
			char _parseFailed=0;
			switch(_destTypes[i]){
				case CONFIGTYPE_INT:
				case CONFIGTYPE_CHAR:
				{
					double _parsed;
					_parseFailed=strodfail(_val,&_parsed);
					if (_destTypes[i]==CONFIGTYPE_CHAR){
						*((char*)_destVars[i])=_parsed;
					}else{
						*((int*)_destVars[i])=_parsed;
					}
					break;
				}
				case CONFIGTYPE_DOUBLE:
					_parseFailed=strodfail(_val,(double*)_destVars[i]);
					break;
				case CONFIGTYPE_STR:
					*((char**)_destVars[i])=strdup(_val);
					break;
				case CONFIGTYPE_YN:
					*((char*)_destVars[i])=(tolower(_val[0])=='y');
					break;
			}
			if (_parseFailed){
				fprintf(stderr,"Key: \"%s\", Value: \"%s\" parse failed\n",_key,_val);
				return 1;
			}
			return 0;
		}
	}
	fprintf(stderr,"Unknown property %s\n",_key);
	return 1;
}
void readConfig(const char* _filename, int _numRead, const char** _destKeys, void** _destVars, char* _destTypes){
	FILE* fp = fopen(_filename,"rb");
	if (!fp){
		fprintf(stderr,"failed to open config file at %s\n",_filename);
		perror(NULL);
		return;
	}
	char* _curLine=NULL;
	size_t _curSize=0;
	while(1){
		if (getline(&_curLine,&_curSize,fp)==-1){
			if (!feof(fp)){
				fprintf(stderr,"getline failed\n");
			}
			break;
		}
		removeLineBreak(_curLine);
		if (_curLine[0]=='#' || _curLine[0]=='\0'){
			continue;
		}
		char* _equalsPos = strchr(_curLine,'=');
		if (!_equalsPos){
			fprintf(stderr,"ignore line \"%s\"\n",_curLine);
			continue;
		}
		if (*(_equalsPos-1)==' ' || *(_equalsPos+1)==' '){
			fprintf(stderr,"do not put spaces on either side of equals. Ignoring line \"%s\"\n",_curLine);
			continue;
		}
		*_equalsPos='\0';
		parseSingleOption(_curLine, _equalsPos+1, _numRead, _destKeys, _destVars, _destTypes);
	}
	free(_curLine);
	fclose(fp);
}
char* getConfigFilePath(const char* _configFilename){
	char* _folderRoot = getConfigFolder();
	if (!_folderRoot){
		return NULL;
	}
	char* _full = malloc(strlen(_folderRoot)+strlen(_configFilename)+1);
	strcpy(_full,_folderRoot);
	strcat(_full,_configFilename);
	free(_folderRoot);
	return _full;
}
