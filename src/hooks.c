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
/*
return 0 will always mean OK
return 1 will always mean fatal error
any other return values may be interpreted as special messages depending on the hook type
	they are noted below

the following are subject to change:
* the number of arguments passed to a hook program. The ones passed now will not change, but more may be added in the future.
* the interpretation of return values other than 0, 1, and special ones already defined.

Hooks list
(If no special return interpretations or arguments listed, assume none)
----
on init
on image finsihed downloading:
	Arguments: image path
on gallery finsihed downloading:
	Arguments: image path
pre image limit reset:
	Special return interpretations:
		2: continue, skip limit reset
post image limit reset
on list finished
	Arguments: list path
exit with no errors
on sad end
	note: this will run if the "exit with no errors hook fails

the outputlist will be written if any of the following hooks fail:
	on image finished downloading
	pre image limit reset
	post image limit reset
i was thinking about writing the outputlist if "on gallery finished downloading" fails, but what is it supposed to do?
write the entire gallery link to the outputlist? write the gallery's last page link to the outputlist? skip that gallery link and write the rest?
i don't know. i think if this hook fails, it's unexpected and should be handled manually anyway.
why? because post-gallery hook is likely for processing a finished gallery. verifying it, zipping it, whatever. those dont require internet so they will only fail in very strange cases.

 */
#include "lose.h"
#ifndef LOSE32
#include <stdio.h>
#include <sys/types.h>
#include <unistd.h>
#include <sys/wait.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include "mischelp.h"
#include "panda.h"

// runs the child, waits for it to exit, and returns its return
// returns 1 if failed before launch
int runprogram(const char* _exeName, char** _origArgsArr, int _numArgs){
	if (!_exeName){
		return 0;
	}
	printfl(LOGLVTWO,"Running hook\n");
	if (LOGLVTHREE){
		printfl(LOGLVTHREE,_exeName);
		for (int i=0;i<_numArgs;++i){
			printfl(LOGLVTHREE,"%s\n",_origArgsArr[i]);
		}
	}
	pid_t _childPid=fork();
	if (_childPid==0){
		char* argv[2+_numArgs];
		argv[0]=(char*)_exeName;
		if (_numArgs!=0){
			memcpy(&(argv[1]),_origArgsArr,sizeof(char*)*_numArgs);
		}
		argv[_numArgs+1]=NULL;
		execvp(_exeName,argv);
		perror(NULL);
		exit(1);
	}else if (_childPid==-1){
		perror("fork");
		return 1;
	}
	errno=0;
	int wstatus=0;
	if (waitpid(_childPid,&wstatus,0)!=_childPid){
		perror("waitpid");
		return 1;
	}
	// wife exited
	if (WIFEXITED(wstatus)){
		return WEXITSTATUS(wstatus);
	}
	fprintf(stderr,"child did not exit normally. waitpid gave: %d\n",wstatus);
	return 1;
}
int runprogramOneArg(const char* _exeName, const char* _arg, char _printBadRet){
	int _ret=runprogram(_exeName,(char**)&_arg,1);
	if (_printBadRet && _ret){
		fprintf(stderr,"runprogram returned %d\n",_ret);
	}
	return _ret;
}
int runprogramPrintBadRet(const char* _exeName, char** _origArgsArr, int _numArgs){
	int _ret=runprogram(_exeName,_origArgsArr,_numArgs);
	if (_ret){
		fprintf(stderr,"runprogram returned %d\n",_ret);
	}
	return _ret;
}
#else
int runprogram(const char* _exeName, char** _origArgsArr, int _numArgs){
	if (_exeName){
		fprintf(stderr,"too lazy to test hooks on windows so i disabled them\n");
		return 1;
	}
	return 0;
}
int runprogramOneArg(const char* _exeName, const char* _arg, char _printBadRet){
	return runprogram(NULL,NULL,0);
}
int runprogramPrintBadRet(const char* _exeName, char** _origArgsArr, int _numArgs){
	return runprogram(NULL,NULL,0);
}
#endif
