#ifndef _PRE_HEAVY_H
#define _PRE_HEAVY_H

#define TRUE 1
#define FALSE 0
#define PARENT_SOCKET 0
#define CHILD_SOCKET 1
#define MAX_IMAGES 100
#define MAX_BUFFER 4096
#define PORT 9000


typedef struct Data {
    int _busy;
    int _doWork;
    int _terminate;
} Data;

char *_execPath;
char *_currentWorkDir;
static int _nProcesses;
int *_childs;

/** Child variables **/
int _childID;
int _exitLoop;

/** Shared **/
Data *_childsData;
pthread_mutex_t *_mutex;
pthread_mutexattr_t _mutexAttr;
int *_processedImages;
int **_commSockets;

static int getArgs(const int pArgc, char *pArgv[]);
static int createDirectories(char *pArgv[]);

/** Child functions **/
static void handleSigTerm();
static void handleSigCont();
static int receiveSocket(int pChildSocket, int *pNewSocket);
static void getNewPath(char *pResultPath, char *pImageIdStr);
static void doWork();

static void sendSocket(int pParentSocket, int pSocket);
static void createSharedData();
static void createProcesses();
static void killProcesses();
static void unmapSharedMem();
static void acceptConnections();



#endif