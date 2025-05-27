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
extern char windowsFilesystem;
#if defined(_WIN32) || defined(_WIN64)
#define LOSE32
#include <stdio.h>
#include <sys/stat.h>
#include "stolenfunctions.h"
FILE* losedowsfopen(const char* pathname, const char* mode);
int losedowsrename(const char* old, const char* new);
int losedowsremove(const char* _pathorig);
int losedowsmkdir(const char* _pathorig, mode_t mode);
int losedowsdirExists(const char* _dir);
char* windowsGetExeLocation();
char* windowsPathNextToExe(const char* _filename);
#ifndef NOSTUPIDWINDOWSMACROS
#define fopen losedowsfopen
#define rename losedowsrename
#define remove losedowsremove
#define mkdir losedowsmkdir
#define dirExists losedowsdirExists
#endif
#endif
