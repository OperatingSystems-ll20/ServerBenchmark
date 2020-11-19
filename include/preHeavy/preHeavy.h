#ifndef _PRE_HEAVY_H
#define _PRE_HEAVY_H

#define TRUE 1
#define FALSE 0
#define MAX_IMAGES 100
#define PORT 9000

#define MAX(a,b) (((a)>(b))?(a):(b))

typedef struct Data {
    int _busy;
    int _doWork;
    int _terminate;
    int _socket;
    // pthread_condattr_t _condAttr;
    // pthread_cond_t _cond;
} Data;

static int _nProcesses;
int *_childs;

/** Child variables **/
int _childID;
int _exitLoop;

/** Shared **/
Data *_childsData;
int *_processedImages;
pthread_mutex_t *_mutex;
pthread_mutexattr_t _mutexAttr;

static int getArgs(const int pArgc, char *pArgv[]);

/** Child functions **/
static void handleSigTerm();
static void handleSigCont();
static void doWork();

static void createSharedData();
static void createProcesses();
static void killProcesses();
static void unmapSharedMem();
static void acceptConnections();



#endif