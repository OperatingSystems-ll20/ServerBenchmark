#ifndef _CLIENT_H
#define _CLIENT_H

#include <pthread.h>
#include <sys/time.h>

#define MAX_BUFFER 4096

typedef struct ThreadData {
    int _id;
    struct sockaddr_in _serverAddr;
    int _nRequests;
    int _result;
    double _totalTime;
    double _averageTime;
    double _responseTime;
} ThreadData;

static char *_serverIP;
static char *_imagePath;
static int _serverPort;
static int _nThreads;
static int _nCycles;

struct timeval _globalT1, _globalT2;

static int _imageSize;
char *_resultFilePath;
int _imageFP;

pthread_t *_threads;
ThreadData *_threadData;

static int getArgs(const int pArgc, char *pArgv[]);
static FILE *createFile();
static int getResultFilePath();

static void *threadWork(void *pArg);
static void createThreads(struct sockaddr_in *pServerAddress);
static void stopThreads();
static void freeMemory();


#endif