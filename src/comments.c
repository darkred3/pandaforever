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
#include "mischelp.h"
#include "panda.h"
#include "fixhtmlgarbage.h"
#include "lose.h"
#include "metadatawrite.h"

// stands for "fwrite str did fail"
static int fwritestrd(const char* ptr, FILE* stream){
	if (fwrite(ptr,strlen(ptr),1,stream)!=1){
		if (strlen(ptr)==0){
			return 0;
		}
		perror("fwritestrd");
		return 1;
	}
	return 0;
}
static int fwritehtmlstrd(char* ptr, FILE* stream){
	char* _usable=strdup(ptr);
	int _numBytes = decode_html_entities_utf8(_usable, NULL);
	int _ret=fwrite(_usable,_numBytes,1,stream)!=1;
	if (_ret){
		if (_numBytes==0){
			_ret=0;
		}else{
			perror("fwritehtmlstrd");
		}
	}
	free(_usable);
	return _ret;
}
int fputcr(int c, FILE* stream);
static int writetodelim(char** _curPos, const char* _delim, FILE* fp){
	char* _delimSpot=strstr(*_curPos,_delim);
	*_delimSpot='\0';
	int _ret=fwritehtmlstrd(*_curPos, fp);
	*_delimSpot=_delim[0];
	*_curPos=_delimSpot+strlen(_delim);
	return _ret;
}
int isBeforeOther(const char* lookatthis, const char* nextcomment){
	return lookatthis<nextcomment || !nextcomment;
}
const char* tagsToReplace[] =   {"<strong>","</strong>","<em>","</em>","<span style=\"text-decoration:underline;\">","</span>","<del>","</del>"};
const char* tagReplacements[] = {"[b]","[/b]","[i]","[/i]","[u]","[/u]","[s]","[/s]"};
#define PANDAFOREVERCOMMENTSVERSION "3"
#define SCOREMARKER "'none'\">Score <span"
#define UPLOADERCOMMENTMARKER "<a name=\"ulcomment\"></a>Uploader Comment</div>"
#define EDITEDMARKER "<div class=\"c8\">Last edited on <strong>"
#define COMMENTBODYSTART "<div class=\"c6\" id=\"comment_"
int parseToComments(const char* _html, const char* _outFilename){
	FILE* fp = fopen(_outFilename,"wb");
	if (!fp){
		perror("parseToComments fopen");
		return 1;
	}
	int _ret=0;
	_ret|=fwritestrd("pndafrvr: "PANDAFOREVERCOMMENTSVERSION"\n",fp);

	char* _nextLastEditedMarker=strstrafter(_html,EDITEDMARKER);
	char* _nextCommentStart=strstr(_html,NEWCOMMENTMARKER);
	while(_nextCommentStart){
		char* _curPos=_nextCommentStart+strlen("<div class=\"c3\">");
		_nextCommentStart=strstr(_nextCommentStart+1,NEWCOMMENTMARKER);
		{ // posted on...   + [username]
			char* _headerEndPos=strstr(_curPos,"</div>");
			char* _postedOnEnd=strstr(_curPos,"&nbsp");
			if (isBeforeOther(_headerEndPos,_postedOnEnd)){
				// there is no username here. Just the "posted on" date.
				_ret|=writetodelim(&_curPos,"</div>",fp);
			}else{ // username exists
				_ret|=writetodelim(&_curPos,"&nbsp",fp);
				// username
				_curPos=strchr(_curPos,'>')+1;
				_ret|=writetodelim(&_curPos,"<",fp);
			}
		}
		// score/uploader comment
		_ret|=fputcr('\n',fp);
		char* _scoreMarkerStart=strstrafter(_curPos,SCOREMARKER);
		if (_scoreMarkerStart && isBeforeOther(_scoreMarkerStart,_nextCommentStart)){
			_curPos=strchr(_scoreMarkerStart,'>')+1;
			_ret|=fwritestrd("Score ",fp);
			_ret|=writetodelim(&_curPos, "<", fp);
		}else{
			char* _ulComment = strstr(_curPos,UPLOADERCOMMENTMARKER);
			if (_ulComment && isBeforeOther(_ulComment,_nextCommentStart)){
				_ret|=fwritestrd("(Uploader comment)",fp);
			}else{
				fprintf(stderr,"cant find score or uploader comment marker\n");
				_ret=1;
			}
		}
		_ret|=fputcr('\n',fp);
		// look for "last edited" message
		if (_nextLastEditedMarker){
			if (isBeforeOther(_nextLastEditedMarker,_nextCommentStart)){
				_ret|=fwritestrd("Last edited on ",fp);
				// it's for this comment. put it in.
				_ret|=writetodelim(&_nextLastEditedMarker,"</strong>",fp);
				_ret|=fputcr('\n',fp);
				_nextLastEditedMarker=strstrafter(_nextLastEditedMarker,EDITEDMARKER);
			}
		}
		// write comment body
		{
			_curPos=strchr(strstr(_curPos,COMMENTBODYSTART),'>')+1;

			_ret|=fputcr('\t',fp);
			char* _commentPos=_curPos;
			while(1){
				char c = *(_commentPos++);
				switch(c){
					case '<':
					{
						if (startswith(_commentPos,"br")){
							_ret|=fputcr('\n',fp);
							_ret|=fputcr('\t',fp);
							_commentPos=strchr(_commentPos,'>')+1; // jump to tag end
						}else if (startswith(_commentPos,"a href")){
							// links cannot have bbcode inside of them.
							//  [url=http://aaa][i]sometext[/i][/url] doesnt work
							_commentPos=strchr(_commentPos,'"')+1; // go inside the url
							char* _linkpos=_commentPos;
							char* _linkendPos=strchr(_linkpos,'"'); // chop the end
							*_linkendPos='\0';
							_ret|=fwritestrd(_linkpos,fp); // write url

							// attempt to save the content that has the link around it.
							// <a> content in here </a>
							// example link with text label: https://exhentai.org/g/2151347/b61464ad01/
							//  (the goal is to get this)
							// example link with image inside: https://exhentai.org/g/2259222/ed42195eb6/
							//  (the image wont be saved)
							char* _contentstart=strchr(_linkendPos+1,'>')+1;
							char* nexttagstartpos=strchr(_contentstart,'<');

							// 1. if there wasn't another tag inside the link.
							//    this means the contents is text
							// 2. the contents aren't empty
							if (startswith(nexttagstartpos,"</a>") &&
								nexttagstartpos!=_contentstart){
								// 3. the content isn't the default, which is just
								//    a text copy of the original link.
								*nexttagstartpos='\0';
								if (strcmp(_contentstart,_linkpos)!=0){
									_ret|=fputcr('(',fp);
									_ret|=fwritehtmlstrd(_contentstart,fp);
									_ret|=fputcr(')',fp);
								}
								*nexttagstartpos='<';
							}
							*_linkendPos='"'; // fix temp termination
							_commentPos=strstrafter(_commentPos,"</a>"); // jump to end.
						}else if (startswith(_commentPos,"img src=")){ // skip images
							_commentPos=strstrafter(_commentPos,"/>");
						}else if (startswith(_commentPos,"div id=\"spa\"")){ // advertisement?
							_commentPos=strstrafter(_commentPos,"</div>");
						}else if (startswith(_commentPos,"/div>")){ // the end of the comment
							goto commentdone;
						}else{
							int _numReplace=sizeof(tagsToReplace)/sizeof(char*);
							int j;
							for (j=0;j<_numReplace;++j){
								if (startswith(_commentPos,tagsToReplace[j]+1)){
									_ret|=fwritestrd(tagReplacements[j],fp);
									_commentPos+=strlen(tagsToReplace[j])-1;
									break;
								}
							}
							if (j==_numReplace){
								if (*_commentPos!='/'){
									fprintf(stderr,"unknown html tag %s in comments\nquitting.",_commentPos);
									fclose(fp);
									return 1;
								}
								_ret|=fputcr(c,fp);
							}
						}
					}
						break;
					case '&':
					{
						--_commentPos;
						char _parseDest[10];
						char* _moveableParseDest=_parseDest;
						if (parse_entity(_commentPos, &_moveableParseDest, (const char**)&_commentPos)){
							*_moveableParseDest='\0';
							_ret|=fwritestrd(_parseDest, fp);
						}else{
							fprintf(stderr,"failed to parse at %s\n",_commentPos-1);
						}
					}
						break;
					default:
						_ret|=fputcr(c,fp);
						break;
				}
			}
		commentdone:
			_ret|=fputcr('\n',fp);
		}
	}
	fclose(fp);
	return _ret;
}
