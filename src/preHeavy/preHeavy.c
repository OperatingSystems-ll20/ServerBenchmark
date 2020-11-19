#include <stdio.h>
#include <stdlib.h>
#include <sys/wait.h> 
#include <sys/mman.h>
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
        pause();
        printf("Child %d waked up!!!\n", _childID);
    }
    
    printf("[son] pid %d with _childID %d exiting...\n", getpid(), _childID);
    exit(0);
}

static void createSharedData(){
    int protection = PROT_READ | PROT_WRITE;
    int visibility = MAP_SHARED | MAP_ANONYMOUS;

    _sharedData = mmap(NULL, sizeof(Data)*_nProcesses, protection, visibility, -1, 0);

    for(int i = 0; i < _nProcesses; i++){
        _sharedData[i]->_busy = FALSE;
        _sharedData[i]->_doWork = FALSE;
        _sharedData[i]->_socket = 0;
        // pthread_condattr_init(&_sharedData[i]->_condAttr);
        // pthread_condattr_setpshared(&_sharedData[i]->_condAttr, PTHREAD_PROCESS_SHARED);
        // pthread_cond_init(&_sharedData[i]->_cond, &_sharedData[i]->_condAttr);
    }

    pthread_mutexattr_init(&_mutexAttr);
    pthread_mutexattr_setpshared(&_mutexAttr, PTHREAD_PROCESS_SHARED);
    pthread_mutex_init(&_mutex, &_mutexAttr);
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

int main(int argc, char *argv[]){
    _nProcesses = getArgs(argc, argv);
    if(!_nProcesses || _nProcesses < 0) {
        printf("Invalid number of procesess\n");
        exit(1);
    }

    // createSharedData();
    createProcesses();

    printf("Sleeping main thread!!!\n");
    sleep(5);
    printf("Waking child 0 one time!!!\n");
    kill(_childs[0], SIGCONT);
    
    printf("Sleeping main thread!!!\n");
    sleep(3);
    printf("Terminating childs!!!\n");
    for(int i = 0; i < _nProcesses; i++){
        kill(_childs[i], SIGTERM);
        wait(NULL);
    } 
    

    return 0;
}