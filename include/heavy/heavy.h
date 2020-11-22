#ifndef _HEAVY_H
#define _HEAVY_H

#include <pthread.h>

#define TRUE 1
#define FALSE 0
#define PARENT_SOCKET 0
#define CHILD_SOCKET 1
#define MAX_IMAGES 100
#define MAX_BUFFER 4096
#define PORT 9001

static const char *DIR_REGEX = "^heavyRun_[[:digit:]]+$";

char *_execPath;
char *_currentWorkDir;
int _newSocketFD;

/** Shared variables **/
pthread_mutex_t *_mutex;
pthread_mutexattr_t _mutexAttr;
int *_processedImages;

/** Child functions **/
static void getNewPath(char *pResultPath, char *pImageIdStr);
static void doWork();

static int createDirectories(char *pArgv[]);
static void createSharedData();
static void unmapSharedMem();
static int createProcess();
static void childTerminated();
static void acceptConnections();



#endif