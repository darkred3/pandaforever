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
#include <time.h>
#include "mischelp.h"
#include "fixhtmlgarbage.h"
#include "panda.h"
#include "lose.h"
#include "metadatawrite.h"

#define UNIQUEKEY "pndafrvr"
#define UNIQUEKEYVER "4"
#define ENABLEEXTENSIONS 1

int fputcr(int c, FILE* stream){
	return fputc(c,stream)==EOF;
}
static int writeindent(FILE* fp, int numindent){
	for (int i=0;i<numindent;++i){
		if (fputcr('\t',fp)){
			perror(NULL);
			return 1;
		}
	}
	return 0;
}
static int fwriterawstr(const char* ptr, FILE* stream){
	if (fwrite(ptr,strlen(ptr),1,stream)!=1){
		perror(NULL);
		return 1;
	}
	return 0;
}
static int _writeEscapedStr(const char* str, FILE* fp){
	int _ret=0;
	while(1){
		unsigned char c = *(str++);
		if (c<=0x1f){
			if (c=='\0'){
				break;
			}
			if (c==0x0A){
				c='n';
				goto putbackslash;
			}
			fprintf(fp,"\\u%04X",c);
			continue;
		}
		if (c=='\\' || c=='"'){
		putbackslash:
			_ret|=fputcr('\\',fp);
		}
		_ret|=fputcr(c,fp);
	}
	return _ret;
}
static void plusToSpace(char* str){
	while(1){
		char c = *(str++);
		if (c=='\0'){
			break;
		}else if (c=='+'){
			*(str-1)=' ';
		}
	}
}
static int writeJsonString(const char* str, FILE* fp){
	if (fputcr('"',fp)){
		return 1;
	}
	char* _writeable = strdup(str);
	decode_html_entities_utf8(_writeable,NULL);
	plusToSpace(_writeable);
	if (_writeEscapedStr(_writeable,fp)){
		free(_writeable);
		return 1;
	}
	free(_writeable);
	return fputcr('"',fp);
}
static int writeStringPairElem(FILE* fp, const char* _key, const char* _val, int _indentLevel, int _comma){
	if (writeindent(fp,_indentLevel)
		|| writeJsonString(_key,fp)
		|| fwriterawstr(": ",fp)
		|| writeJsonString(_val,fp)
		|| (_comma && fputcr(',',fp))
		|| fputcr('\n',fp)){
		return 1;
	}
	return 0;
}
static int writeeolprimitive(const char* _str, FILE* fp){
	return (fwriterawstr(_str,fp) || fwriterawstr(",\n",fp));
}
static int writePrimativePair(FILE* fp, const char* _key, const char* _val, int _indentLevel, int _comma){
	return (writeindent(fp,_indentLevel)
			|| writeJsonString(_key,fp)
			|| fwriterawstr(": ",fp)
			|| fwriterawstr(_val,fp)
			|| (_comma && fputcr(',',fp))
			|| fputcr('\n',fp));
}
static int writenumber(int _val, int _iseol, FILE* fp){
	fprintf(fp, _iseol ? "%d,\n" : "%d\n", _val);
	return 0;
}
static int writeblockend(int _numIndent, int _comma, FILE* fp){
	return (writeindent(fp,_numIndent)
			|| fputcr('}',fp)
			|| (_comma && fputcr(',',fp))
			|| fputcr('\n',fp));
}
static int writeGidToken(const char* _galleryUrl, int _indentLevel, int _comma, FILE* fp){
	int _ret=0;
	char* _id;
	char* _key;
	getGalleryUrlIdKey(_galleryUrl,&_id,&_key);
	_ret|=writeindent(fp,_indentLevel);
	_ret|=fwriterawstr("\"gid\": ",fp);
	_ret|=writeeolprimitive(_id,fp);
	//
	_ret|=writeStringPairElem(fp,"token",_key,_indentLevel,_comma);
	free(_id);
	free(_key);
	return _ret;
}
static int writeDate(int* _nums, int _indentation, int _trailingComma, FILE* fp){
	int _ret=0;
	_ret|=writeindent(fp,_indentation);
	_ret|=fwriterawstr("\"upload_date\": [\n",fp);
	for (int i=0;i<6;++i){
		_ret|=writeindent(fp,_indentation+1);
		writenumber(_nums[i],i!=5,fp);
	}
	_ret|=writeindent(fp,_indentation);
	_ret|=fwriterawstr("]",fp);
	if (_trailingComma){
		_ret|=fputcr(',',fp);
	}
	_ret|=fputcr('\n',fp);
	return _ret;
}
static int writeDateFromStr(const char* _dateStart, int _indentation, int _trailingComma, FILE* fp){
	int _nums[6];
	_nums[5]=0; // second number. is unknown.
	sscanf(_dateStart,"%d-%d-%d %d:%d",&(_nums[0]),&(_nums[1]),&(_nums[2]),&(_nums[3]),&(_nums[4]));
	return writeDate(_nums,_indentation,_trailingComma,fp);
}
#define TITLEMARKER "<h1 id=\"gn\">"
#define TITLEJPMARKER "<h1 id=\"gj\">"
#define UPLOADERMARKER "<div id=\"gdn\""
#define CATEGORYMARKER "<div id=\"gdc\">"
#define TAGMARKER "hentai.org/tag/"
#define LANGUAGEMARKERA "<td class=\"gdt1\">Language:</td>"
#define LANGUAGEMARKERB "<td class=\"gdt2\">"
#define ISTRANSLATEDMARKER ">TR</span>"
#define DATEMARKER ">Posted:</td><td class=\"gdt2\">"
#define JSONSTARTSTUFF "\"gallery_info\": {\n"
#define PARENTMARKER ">Parent:</td><td class=\"gdt2\"><a href=\""
#define NEWERVERSIONSMARKER "<div id=\"gnd\">"
#define REPORTGALLERYMARKER "?report=select\">Report Gallery</a>"
#define PAGECOUNTMARKER "Length:</td><td class=\"gdt2\">"
#define APIPOSTEDMARKER "\"posted\":\""
#define APIRATINGMARKER "\"rating\":\""
#define APIFILESIZEMARKER "\"filesize\":"
#define FAVORITEMARKER "<td class=\"gdt2\" id=\"favcount\">"
#define DISOWNEDUPLOADERNAME "(Disowned)"
int parsetometadata(char* _html, char* _apiBuff, const char* _outFilename){
	char* _titleStart = strstrafter(_html,TITLEMARKER);
	if (!_titleStart){
		fprintf(stderr,"%s not found\n",TITLEMARKER);
		return 1;
	}
	FILE* fp = fopen(_outFilename,"wb");
	if (!fp || fwriterawstr("{\n",fp) || writeindent(fp,1) || fwriterawstr(JSONSTARTSTUFF,fp)){
		perror(_outFilename);
		return 1;
	}
	int _ret=0;
	// title, title_original
	{
		char* _titleEnd = strstr(_titleStart,"</h1>");
		*_titleEnd='\0';
		_ret|=writeStringPairElem(fp,"title",_titleStart,2,1);
		*_titleEnd='a';
		_titleStart=strstr(_html,TITLEJPMARKER)+strlen(TITLEJPMARKER);
		_titleEnd=strstr(_titleStart,"</h1>");
		*_titleEnd='\0';
		_ret|=writeStringPairElem(fp,"title_original",_titleStart,2,1);
		*_titleEnd='a';
	}
	// uploader
	{
		char* _uploaderEnd=NULL;
		char* _uploaderStart=strstrafter(_html,UPLOADERMARKER);

		_uploaderEnd = strstr(_uploaderStart,"</a>");
		if (strstr(_uploaderStart,"</div>")<_uploaderEnd){
			_uploaderEnd=NULL;
			_uploaderStart=DISOWNEDUPLOADERNAME;
		}else{
			_uploaderStart=searchCharBackwards(_uploaderEnd,'>')+1;
			*_uploaderEnd='\0';
		}
		_ret|=writeStringPairElem(fp,"uploader",_uploaderStart,2,1);
		if (_uploaderEnd){
			*_uploaderEnd='a';
		}
	}
	// category
	{
		char* _categoryStart=strstrafter(_html,CATEGORYMARKER);
		char* _categoryEnd=strstr(_categoryStart,"</div>");
		_categoryStart=searchCharBackwards(_categoryEnd,'>')+1;
		*_categoryEnd='\0';
		_ret|=writeStringPairElem(fp,"category",_categoryStart,2,1);
		*_categoryEnd='a';
	}
	// get tags:
	{
		char* _commentsStartPos=strstr(_html,NEWCOMMENTMARKER);
		_ret|=writeindent(fp,2);
		_ret|=fwriterawstr("\"tags\": {\n",fp);
		char* _lastPrefix=NULL;
		const char* _nextSearchPos=_html;
		while(1){
			_nextSearchPos=strstrafter(_nextSearchPos,TAGMARKER);
			if (!_nextSearchPos || !isBeforeOther(_nextSearchPos, _commentsStartPos)){
				break;
			}
			char* _tagEnd=strchr(_nextSearchPos,'"');
			*_tagEnd='\0';

			if (_lastPrefix==NULL || !startswith(_nextSearchPos,_lastPrefix)){
				char _needToEndLast=(_lastPrefix!=NULL);
				char* _namespaceEnd=strchr(_nextSearchPos,':');
				const char* _namespaceStart;
				if (_namespaceEnd){
					*_namespaceEnd='\0';
					_namespaceStart=_nextSearchPos;
				newnamespace:
					if (_lastPrefix){
						_ret|=fputcr('\n',fp);
					}
					free(_lastPrefix);
					_lastPrefix=strdup(_namespaceStart);
					if (_needToEndLast){
						_ret|=writeindent(fp,3);
						_ret|=fwriterawstr("],\n",fp);
					}
					_ret|=writeindent(fp,3);
					_ret|=writeJsonString(_namespaceStart,fp);
					_ret|=fwriterawstr(": [\n",fp);
					if (_namespaceEnd){
						*_namespaceEnd=':';
						_nextSearchPos=_namespaceEnd+1;
					}
				}else{
					if (!_lastPrefix || strcmp(_lastPrefix,"misc")!=0){
						_namespaceStart="misc";
						goto newnamespace;
					}
					goto samemiscnamespace;
				}
			}else{
				_nextSearchPos+=strlen(_lastPrefix)+1;
			samemiscnamespace:
				_ret|=fwriterawstr(",\n",fp);
			}
			_ret|=writeindent(fp,4);
			_ret|=writeJsonString(_nextSearchPos,fp);
			*_tagEnd='a';
			_nextSearchPos=_tagEnd;
		}
		_ret|=fputcr('\n',fp);
		if (_lastPrefix){
			_ret|=writeindent(fp,3);
			_ret|=fwriterawstr("]\n",fp);
		}
		_ret|=writeblockend(2, 1, fp);
		free(_lastPrefix);
	}
	// lang
	{
		char* _langStart=strstr(_html,LANGUAGEMARKERA);
		_langStart=strstrafter(_langStart,LANGUAGEMARKERB);
		char* _langEnd=strchr(_langStart,' ');
		*_langEnd='\0';
		_ret|=writeStringPairElem(fp,"language",_langStart,2,1);
		*_langEnd='a';
	}
	// translated <true/false>
	writePrimativePair(fp,"translated",strstr(_html,ISTRANSLATEDMARKER)!=NULL ? "true" : "false",2,1);
	// upload data
	{
		if (_apiBuff){ // get date from metadata
			time_t _unixTime;
			char* _timeStringStart=strstrafter(_apiBuff,APIPOSTEDMARKER);
			char* _timeStringEnd=strchr(_timeStringStart,'"');
			*_timeStringEnd='\0';
			_unixTime=atol(_timeStringStart);
			*_timeStringEnd='"';
			struct tm _time;
			gmtime_r(&_unixTime,&_time);
			int _nums[6] = {0,0,_time.tm_mday,_time.tm_hour,_time.tm_min,_time.tm_sec};
			_nums[0]=_time.tm_year+1900;
			_nums[1]=_time.tm_mon+1;
			_ret|=writeDate(_nums,2,1,fp);
		}else{
			char* _dateStart=strstrafter(_html,DATEMARKER);
			_ret|=writeDateFromStr(_dateStart,2,1,fp);
		}
	}
	// source block
	{
		_ret|=writeindent(fp,2);
		_ret|=fwriterawstr("\"source\": {\n",fp);

		_ret|=writeindent(fp,3);
		_ret|=fwriterawstr("\"site\": \"",fp);
		_ret|=fwrite(SITEDOMAIN,strlen(SITEDOMAIN)-4,1,fp)!=1;
		_ret|=fwriterawstr("\",\n",fp);

		{
			char* _galleryUrlEnd=strstr(_html,REPORTGALLERYMARKER);
			char* _galleryUrlStart=searchCharBackwards(_galleryUrlEnd,'"')+1;
			*_galleryUrlEnd='\0';
			_ret|=writeGidToken(_galleryUrlStart,3,1,fp);
			*_galleryUrlStart='a';
		}

		_ret|=writeindent(fp,3);
		char* _parentStart=strstrafter(_html,PARENTMARKER);
		if (_parentStart){
			_ret|=fwriterawstr("\"parent_gallery\": {\n",fp);
			
			char* _urlEnd=strchr(_parentStart,'"');
			*_urlEnd='\0';
			// the exresurrect script writes the id here as a string. but the previous gid as a number. i declare that inconsistency stupid.
			_ret|=writeGidToken(_parentStart,4,0,fp);
			*_urlEnd='a';

			_ret|=writeblockend(3,1,fp);
		}else{
			_ret|=fwriterawstr("\"parent_gallery\": null,\n",fp);
		}

		char* _newVersionsStart=strstrafter(_html,NEWERVERSIONSMARKER);
		if (_newVersionsStart){
			char* _newVersionsEnd = strstr(_newVersionsStart,"</div>");

			_ret|=writeindent(fp,3);
			_ret|=fwriterawstr("\"newer_version\": [",fp);

			char _isFirst=1;
			char* _linkStart=_newVersionsStart;
			while(1){
				_linkStart=strstrafter(_linkStart,"<a href=\"");
				if (!_linkStart || _linkStart>=_newVersionsEnd){
					break;
				}

				if (!_isFirst){
					_ret|=fputcr(',',fp);
				}
				_ret|=fputcr('\n',fp);
				
				_ret|=writeindent(fp,4);
				_ret|=fwriterawstr("{\n",fp);

				_ret|=writeindent(fp,5);
				_ret|=fwriterawstr("\"gallery\": {\n",fp);

				char* _linkEnd=strchr(_linkStart,'"');
				*_linkEnd='\0';
				_ret|=writeGidToken(_linkStart,6,0,fp);
				*_linkEnd='a';
				_linkStart=_linkEnd+2;
				_ret|=writeindent(fp,5);
				_ret|=fwriterawstr("},\n",fp);
				// title
				char* _titleEnd=strstr(_linkStart,"</a>");
				*_titleEnd='\0';
				_ret|=writeStringPairElem(fp, "name", _linkStart, 5, 1);
				*_titleEnd='a';
				_linkStart=_titleEnd;
				// date
				char* _dateStart=strstrafter(_html,", added ");
				_ret|=writeDateFromStr(_dateStart,5,0,fp);
				
				_ret|=writeindent(fp,4);
				_ret|=fputcr('}',fp);
			}
			_ret|=fputcr('\n',fp);
			_ret|=writeindent(fp,3);
			_ret|=fwriterawstr("]\n",fp);
			
		}else{
			_ret|=writeindent(fp,3);
			_ret|=fwriterawstr("\"newer_version\": []\n",fp);
		}
		_ret|=writeblockend(2,1,fp);
		// add a key that notifies a parser that this is a variant of the eze format.
		_ret|=writePrimativePair(fp,UNIQUEKEY,UNIQUEKEYVER,2,ENABLEEXTENSIONS);
		#if ENABLEEXTENSIONS == 1
		// pagecount
		char* _pageCountStart=strstrafter(_html,PAGECOUNTMARKER);
		char* _pageCountEnd=strstr(_pageCountStart," pages");
		*_pageCountEnd='\0';
		_ret|=writePrimativePair(fp,"filecount",_pageCountStart,2,1);
		*_pageCountEnd='a';
		// num favorites
		{
			char* _favoriteStart=strstrafter(_html,FAVORITEMARKER);
			char* _favoriteEnd=strchr(_favoriteStart,'<');
			*_favoriteEnd='\0';
			char* _timesMarker = strstr(_favoriteStart," times");
			char* _numStr;
			if (!_timesMarker){
				if (strcmp(_favoriteStart,"Once")==0){
					_numStr="1";
				}else if (strcmp(_favoriteStart,"Never")==0){
					_numStr="0";
				}else{
					fprintf(stderr,"unknown %s\n",_favoriteStart);
					_ret=1;
					_numStr="0";
				}
			}else{
				_numStr=_favoriteStart;
				*_timesMarker='\0';
			}
			*_favoriteEnd='a';
			_ret|=writePrimativePair(fp,"favorites",_numStr,2,_apiBuff!=NULL);
			if (_timesMarker){
				*_timesMarker='a';
			}
		}
		if (_apiBuff){
			// rating
			// actually, this can be obtained from the html. oh well.
			char* _ratingStart=strstrafter(_apiBuff,APIRATINGMARKER);
			char* _ratingEnd=strchr(_ratingStart,'"');
			*_ratingEnd='\0';
			_ret|=writePrimativePair(fp,"rating",_ratingStart,2,1);
			*_ratingEnd='a';
			// filesize
			char* _filesizeStart=strstrafter(_apiBuff,APIFILESIZEMARKER);
			char* _filesizeEnd=strchr(_filesizeStart,',');
			*_filesizeEnd='\0';
			_ret|=writePrimativePair(fp,"filesize",_filesizeStart,2,0);
			*_filesizeEnd='a';
		}
		#endif
	}
	_ret|=writeblockend(1,0,fp);
	_ret|=writeblockend(0,0,fp);
	_ret|=fputcr('\n',fp); // posix text file
	fclose(fp);
	return _ret;
}
