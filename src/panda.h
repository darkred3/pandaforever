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
#ifndef PANDAH10252020
#define PANDAH10252020
#include <signal.h>
#include <curl/curl.h>
#ifdef DISABLESECRETCLUB
#define SITEDOMAIN "e-hentai.org"
#else
#define SITEDOMAIN "exhentai.org"
#endif
#define CONTINUEPAGEFORMAT "https://"SITEDOMAIN"/s/%s/%s-%d"
#define CONTINUEGALLERYFORMAT "https://"SITEDOMAIN"/g/%s/%s"

#define WINDOWSDEFAULTSUBDIRLEN 110

// log level 0 is for the insane
#define LOGLVZERO (logLevel>=0)
#define LOGLVONE (logLevel>=1)
#define LOGLVTWO (logLevel>=2)
#define LOGLVTHREE (logLevel>=3)
extern int logLevel;

extern char* lastContinuePageGid;
extern char* lastContinuePageKey;
extern int lastContinuePageNum;
extern sig_atomic_t shutdownRequested;
extern char isLoggedIn;

extern double stdSleepMin;
extern double stdSleepMax;
extern int retryWaitSeconds;
extern int maxSameServerRetries;
extern int maxPageIsNotLoadingTries;
extern int max503Requests;
extern char canResetLimit;
extern int maxGallerySubdirLen;
extern int maxImageFilenameLen;
extern int logLevel;
extern int fileDownloadsLeft;
extern char getHtmlMetadata;
extern char getApiMetadata;
extern char chosenDirFormat;
extern char* unavailableList;
extern char shouldDownloadComments;
extern char evenmorestrict;

extern int hookVer;
extern char* onImageDownloadHook;
extern char* preLimitResetHook;
extern char* postLimitResetHook;
extern char* onGalleryDownloadDoneHook;

void getGalleryUrlIdKey(const char* _galleryLink, char** _retId, char** _retKey);
int exlogin(CURL* c, const char* _username, const char* _password, const char* _cookieFile);
int nabGallery(CURL* c, const char* _link, const char* _rootOutDir);
char* getApiMetadataLink(CURL* c, const char* _link);
int lowResetLimit(CURL* c);
char* trimGalleryUrlGarbage(const char* _galleryUrl);
char* generateUrlWithComments(const char* _galleryUrl);
char* apiGetMetadata(CURL* c, const char* _gid, const char* _key);
int isGalleryUnavailable(const char* _listingHtml);
void standardSleep();
#endif
