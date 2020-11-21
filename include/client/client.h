#ifndef _CLIENT_H
#define _CLIENT_H

#include <pthread.h>

#define MAX_BUFFER 4096

typedef struct ThreadData {
    int _id;
    struct sockaddr_in _serverAddr;
    int _result;
} ThreadData;

static char *_serverIP;
static char *_imagePath;
static int _serverPort;
static int _nThreads;
static int _nCycles;

static int _imageSize;
int _imageFP;

pthread_t *_threads;
ThreadData *_threadData;

static int checkPath(const char *pPath);
static int getArgs(const int pArgc, char *pArgv[]);

static void *threadWork(void *pArg);
static void createThreads(struct sockaddr_in *pServerAddress);
static void stopThreads();
static void freeMemory();


#endif