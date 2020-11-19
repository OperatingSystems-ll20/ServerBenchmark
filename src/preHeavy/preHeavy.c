#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/wait.h> 
#include <sys/mman.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <pthread.h>
#include <unistd.h> 
#include <preHeavy.h>

static int getArgs(const int pArgc, char *pArgv[]){
    int n = 0;
    if(pArgc < 2 || pArgc > 2){
        printf("Usage: ./preHeavy <number of heavy processes>\n");
    }
    else{
        n = atoi(pArgv[1]);
    }
    return n;
}

static void handleSigTerm(){
    _exitLoop = 1;
}

static void handleSigCont(){}

static void doWork(){
    while (!_exitLoop){
        while(!_childsData[_childID]._doWork && !_childsData[_childID]._terminate) 
            pause();

        // pthread_mutex_lock(_mutex);
        // if(*_processedImages < MAX_IMAGES-1){
        //     (*_processedImages)++;
        //     printf("Process %d incrementing counter. Counter = %d\n", _childID, *_processedImages);
        // }
        // pthread_mutex_unlock(_mutex);

        _childsData[_childID]._doWork = 0;
    }
    
    printf("[son] pid %d with _childID %d exiting...\n", getpid(), _childID);
    exit(0);
}

static void createSharedData(){
    int protection = PROT_READ | PROT_WRITE;
    int visibility = MAP_SHARED | MAP_ANONYMOUS;

    _processedImages = (int*)mmap(NULL, sizeof(int), protection, visibility, -1, 0);
    _mutex = mmap(NULL, sizeof(pthread_mutex_t), protection, visibility, -1, 0);
    _childsData = mmap(NULL, sizeof(Data)*_nProcesses, protection, visibility, -1, 0);

    *_processedImages = 0;

    for(int i = 0; i < _nProcesses; i++){
        _childsData[i]._busy = FALSE;
        _childsData[i]._doWork = FALSE;
        _childsData[i]._terminate = FALSE;
        _childsData[i]._socket = 0;
        // pthread_condattr_init(&_childsData[i]->_condAttr);
        // pthread_condattr_setpshared(&_childsData[i]->_condAttr, PTHREAD_PROCESS_SHARED);
        // pthread_cond_init(&_childsData[i]->_cond, &_childsData[i]->_condAttr);
    }

    pthread_mutexattr_init(&_mutexAttr);
    pthread_mutexattr_setpshared(&_mutexAttr, PTHREAD_PROCESS_SHARED);
    pthread_mutex_init(_mutex, &_mutexAttr);
}


static void createProcesses(){
    _childs = malloc(_nProcesses * sizeof(pid_t));
    for(int i = 0; i < _nProcesses; i++){
        _childID = i;
        _childs[i] = fork();
        if(_childs[i] == 0){
            printf("[son] pid %d with _childID %d\n", getpid(), _childID);
            signal(SIGTERM, handleSigTerm);
            signal(SIGCONT, handleSigCont);
            doWork();
        }
    }
}

static void killProcesses(){
    for(int i = 0; i < _nProcesses; i++){
        _childsData[i]._terminate = 1;
        kill(_childs[i], SIGTERM);
        wait(NULL);
    }
}

static void unmapSharedMem(){
    munmap(_processedImages, sizeof(int));
    munmap(_mutex, sizeof(pthread_mutex_t));
    munmap(_childsData, sizeof(Data)*_nProcesses);
}


static void acceptConnections(){
    int socketFD, newSocketFD, ret;
    socklen_t clientLen;
    int exitLoop = 0;
    struct sockaddr_in serverAddress, clientAddress;
    char input[100];

    fd_set readFds;

    socketFD = socket(AF_INET, SOCK_STREAM, 0);
    if (socket < 0) {
        fprintf(stderr, "Error: Can't open the socket\n");
        exit(-1);
    }

    bzero((char *) &serverAddress, sizeof(serverAddress));
    serverAddress.sin_family = AF_INET;
    serverAddress.sin_addr.s_addr = INADDR_ANY;
    serverAddress.sin_port = htons(PORT);

    if(bind(socketFD, (struct sockaddr *) &serverAddress, sizeof(serverAddress)) < 0){
        fprintf(stderr, "Error: Binding socket\n");
        exit(-1);
    }
    listen(socketFD,5);
    clientLen = sizeof(clientAddress);

    FD_ZERO(&readFds);
    FD_SET(STDIN_FILENO, &readFds);
    FD_SET(socketFD, &readFds);

    while(!exitLoop){
        int available = select(socketFD+1, &readFds, NULL, NULL, NULL);
        if(available < 0){
            printf("Error on select\n");
            killProcesses();
            close(newSocketFD);
            close(socketFD);
            exit(-1);
        }

        else{
            if(FD_ISSET(STDIN_FILENO, &readFds)){
                scanf("%s", input);
                if(strcmp(input, "exit") == 0){
                    printf("Terminating server...\n");
                    break;
                }
            }

            if(FD_ISSET(socketFD, &readFds)){
                newSocketFD = accept(socketFD, (struct sockaddr *) &clientAddress, &clientLen);
                if (newSocketFD < 0) {
                    fprintf(stderr, "Error: Accepting a connection\n");
                    exitLoop = 1;
                    continue;
                }

                for(int i = 0; i < _nProcesses; i++){
                    if(!_childsData[i]._busy){
                        _childsData[i]._socket = newSocketFD;
                        _childsData[i]._doWork = 1;
                        kill(_childs[i], SIGCONT);
                    }
                }
            }
        }
    }

    killProcesses();
    unmapSharedMem();
    close(newSocketFD);
    close(socketFD);
}

int main(int argc, char *argv[]){
    _nProcesses = getArgs(argc, argv);
    if(!_nProcesses || _nProcesses < 0) {
        printf("Invalid number of procesess\n");
        exit(1);
    }

    createSharedData();
    createProcesses();
    acceptConnections();
    
    return 0;
}