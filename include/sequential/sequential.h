#ifndef _SEQUENTIAL_H
#define _SEQUENTIAL_H

#include <pthread.h>
#include <socketFIFO.h>

#define TRUE 1
#define FALSE 0
#define PARENT_SOCKET 0
#define CHILD_SOCKET 1
#define MAX_IMAGES 100
#define MAX_BUFFER 4096
#define PORT 9002

static const char *DIR_REGEX = "^sequentialRun_[[:digit:]]+$";
static const char *SEQUENTIAL_SERVER_DIR = "/Sequential_Server";

char *_execPath;
char *_currentWorkDir;
int _childPID;
int _commSockets[2];
int _pipes[2];
SocketFIFO _socketFIFO;

/** Shared **/
int _childExitLoop;
int *_childDoWork, *_childTerminate;
int *_processedImages;

static void getNewPath(char *pResultPath, char *pImageIdStr);
static int receiveSocket(int pChildSocket, int *pNewSocket);
static void doWork();

static int createDirectories(char *pArgv[]);
static void createSharedData();
static void unmapSharedMem();
static void sendSocket(int pParentSocket, int pSocket);
static int createChildProcess();
static void killChildProcess();
static void closeAllSockets();
static void acceptConnections();

#endif