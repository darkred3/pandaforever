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
#ifndef CURLHELPER_09282020
#define CURLHELPER_09282020
#include <curl/curl.h>
int curlDownloadFileAlsoPartToMem(CURL* c, const char* _url, FILE* fp, char* _altDest, int _altDestLen);
char* curlGetMem(CURL* c, const char* _url);
char* curlGetMem_stripNulls(CURL* c, const char* _url);
int curlDownloadFile(CURL* c, const char* _url, FILE* fp);
void curlResetSettings(CURL* c);
char* makePostString(CURL* c, char** _labels, char** _values, int _num);
CURL* newcurl();
extern char* useragentStr;
#endif
