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
#ifndef PANDAFOREVERHOOKSHEADER11042020
#define PANDAFOREVERHOOKSHEADER11042020
int runprogram(const char* _exeName, char** _origArgsArr, int _numArgs);
int runprogramPrintBadRet(const char* _exeName, char** _origArgsArr, int _numArgs);
int runprogramOneArg(const char* _exeName, const char* _arg, char _printBadRet);
#endif
