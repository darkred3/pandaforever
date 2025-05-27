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
#ifndef MISCHELP_09282020
#define MISCHELP_09282020
char* allocSubstr(const char* _startPosInclusive, const char* _endPosExclusive);
char* nabNearDelims(const char* _startPos, char d);
char* nabQuoted(const char* _startPos);
int isNumStr(const char* s);
int getImageExtLen(char* s);
void capToLen(char* s, size_t _maxLen, int _tailAmountToKeep, char _useEllipsis);
int startswith(const char* a, const char* b);
int endswith(const char* a, const char* b);
void fixHtmlLink(char* s);
void fixApiLink(char* s);
char* charcat(char* dest, char c);
char* searchCharBackwards(char* str, char c);
int printfl(int _shouldDoIt, const char* format, ...);
int strodfail(const char* nptr, double* _ret);
void capToChars(char* s, size_t _maxChars, int _tailCharsToKeep, char _useEllipsis);
void removeLineBreak(char* _target);
char* forceEndInSlashMaybe(const char* _str);
char* getConfigFolder();
void capToLenStartEnd(char* s, size_t _maxLen, int _startCharsToKeep, int _endAmountToKeep, char _useEllipsis);
char* getImageExt(char* s);
int isWSL();
char* removeIllegals(char* s);
char* strstrafter(const char* haystack, const char* needle);
char* readRemainingToBuff(FILE* infp);
char* fileToBuff(const char* filename);
int buffToFile(const char* _outFilename, const char* buff);
int dirExists(const char* _dir);
int mkdirGoodSettngs(const char *path);
int isarg(const char* s);
int isBlankLine(const char* a);
char* removeIllegalsIsWindows(char* s, int _isWindows);
void easysleep(double seconds);
double randDouble(double _min, double _max);
extern char disableUnicodeReplace;
#endif
