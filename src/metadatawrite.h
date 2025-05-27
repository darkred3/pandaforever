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
#ifndef METADATAWRITE10222020
#define METADATAWRITE10222020
int parsetometadata(char* _html, char* _apiBuff, const char* _outFilename);

#define NEWCOMMENTMARKER "<div class=\"c3\">Posted on"
int parseToComments(const char* _html, const char* _outFilename);
int isBeforeOther(const char* lookatthis, const char* nextcomment);
#endif
