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
#ifndef CONFGIREADERH10172020
#define CONFGIREADERH10172020

#define CONFIGTYPE_INT 1
#define CONFIGTYPE_DOUBLE 2
#define CONFIGTYPE_STR 3
#define CONFIGTYPE_CHAR 4
#define CONFIGTYPE_YN 5
void readConfig(const char* _filename, int _numRead, const char** _destKeys, void** _destVars, char* _destTypes);
char* getConfigFilePath(const char* _configFilename);
int parseSingleOption(const char* _key, const char* _val, int _numOptions, const char** _destKeys, void** _destVars, char* _destTypes);

#endif
