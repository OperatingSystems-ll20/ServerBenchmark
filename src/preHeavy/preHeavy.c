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

static void getExecutablePath(char *pArgv[]){
    char path_save[PATH_MAX];
    // char abs_exe_path[PATH_MAX];
    char *p;

    if(!(p = strrchr(pArgv[0], '/')))
        getcwd(_execPath, sizeof(_execPath));
    else{
        *p = '\0';
        getcwd(path_save, sizeof(path_save));
        chdir(pArgv[0]);
        getcwd(_execPath, sizeof(_execPath));
        chdir(path_save);
    }
    printf("Absolute path to executable is: %s\n", _execPath);
}

static void handleSigTerm(){
    _exitLoop = 1;
}

static void handleSigCont(){ return;}

static int receiveSocket(int pChildSocket, int *pNewSocket){
    struct msghdr m;
    struct cmsghdr *cm;
    struct iovec iov;
    char dummy[100];
    char buf[CMSG_SPACE(sizeof(int))];
    ssize_t readlen;
    int *fdlist;

    iov.iov_base = dummy;
    iov.iov_len = sizeof(dummy);
    memset(&m, 0, sizeof(m));
    m.msg_iov = &iov;
    m.msg_iovlen = 1;
    m.msg_controllen = CMSG_SPACE(sizeof(int));
    m.msg_control = buf;
    readlen = recvmsg(pChildSocket, &m, 0);
    /* Do your error handling here in case recvmsg fails */
    *pNewSocket = -1; /* Default: none was received */
    for (cm = CMSG_FIRSTHDR(&m); cm; cm = CMSG_NXTHDR(&m, cm)) {
        if (cm->cmsg_level == SOL_SOCKET && cm->cmsg_type == SCM_RIGHTS) {
            // nfds = (cm->cmsg_len - CMSG_LEN(0)) / sizeof(int);
            fdlist = (int *)CMSG_DATA(cm);
            *pNewSocket = *fdlist;
            break;
        }
    }

    return 0;
}

PyObject *initPython(){
    setenv("PYTHONPATH",".",1);
    PyObject *moduleString, *module, *dict, *sobelFunction;
    Py_Initialize();

    PyObject* sysPath = PySys_GetObject((char*)"path");
    char pythonDir[PATH_MAX];
    strcpy(pythonDir, _execPath);
    strcat(pythonDir, "/../python");
    PyObject* programName = PyUnicode_FromString(pythonDir);
    PyList_Append(sysPath, programName);
    Py_DECREF(programName);

    moduleString = PyUnicode_FromString((char*)"testSobel");
    module = PyImport_Import(moduleString);

    sobelFunction = PyObject_GetAttrString(module, (char*)"applySobel");

    Py_DECREF(moduleString);
    Py_DECREF(module);

    return sobelFunction;
}

void exitPython(PyObject *pSobel){
    Py_DECREF(pSobel); 
    Py_FinalizeEx();
}

static int processImage(PyObject *pSobel, char *pImageToProcess, char *pImage){

    if(PyCallable_Check(pSobel)){
        printf("Processing image\n");
        PyObject *args = Py_BuildValue("ss", pImageToProcess, pImage);
        PyErr_Print();
        PyObject_CallObject(pSobel, args);
        PyErr_Print();
        Py_DECREF(args); 
    }
    else {
        printf("Image not processed\n");
        PyErr_Print();
    }
     
    return 0; 
}

//Child function
static void doWork(){
    int imageSize, transferSize, readSize, writeSize, ret;
    int process = FALSE;
    char buffer[MAX_BUFFER];
    char counterStr[32];
    char *reply = "OK";
    PyObject *sobelFunction = initPython();

    while (!_exitLoop){
        while(!_childsData[_childID]._doWork && !_childsData[_childID]._terminate) pause();

        //Receive image
        if(_childsData[_childID]._doWork){
            int socket;
            ret = receiveSocket(_commSockets[_childID][CHILD_SOCKET], &socket);
            if(ret != 0) {
                printf("*** Process %d: Error receiving new Socket from parent\n", _childID);
                _childsData[_childID]._doWork = 0;
                continue;
            }

            while(TRUE){
                imageSize = transferSize = readSize = writeSize = 0;
                bzero(buffer, MAX_BUFFER);
                FILE *receivedImg;
                char path[MAX_BUFFER];

                ret = read(socket, &imageSize, sizeof(int));
                if(ret < 0){
                    printf("*** Process %d: Error reading from socket\n", _childID);
                    // close(socket);
                    break;
                }
                if(imageSize == 0){
                    printf("*** Process %d: Invalid image size received\n", _childID);
                    // close(socket);
                    break;
                }
                if(imageSize == -1) break;

                printf("Process %d: Image size received = %d\n", _childID, imageSize);
                strcpy(path, getenv("HOME"));
                strcat(path, "/Escritorio/serverImage.jpg");
                receivedImg = fopen(path, "w+");
                if(receivedImg == NULL){
                    printf("Process %d: Can't open FILE... Image will not be processed\n", _childID);
                    while(transferSize < imageSize){
                        do{
                            readSize = read(socket, buffer, sizeof(buffer));
                        } while(readSize < 0);
                        transferSize += readSize;
                    }
                }

                while(transferSize < imageSize){
                    do{
                        readSize = read(socket, buffer, sizeof(buffer));
                    } while(readSize < 0);
                    writeSize = fwrite(buffer, 1, readSize, receivedImg);
                    if(readSize != writeSize) printf("Process %d: Error writing image\n", _childID);
                    transferSize += readSize;
                }
                fclose(receivedImg);

                char resultPath[PATH_MAX];
                strcpy(resultPath, getenv("HOME"));
                strcat(resultPath, "/Escritorio/received");
                pthread_mutex_lock(_mutex);
                if(*_processedImages < MAX_IMAGES-1){
                    process = TRUE;
                    sprintf(counterStr, "%d", *_processedImages);
                    (*_processedImages)++;                    
                }
                else {
                    process = FALSE;
                }
                strcat(resultPath, counterStr);
                strcat(resultPath, ".jpg");
                pthread_mutex_unlock(_mutex);

                if(process){
                    processImage(sobelFunction, path, resultPath);
                    printf("To process: %s\n", path);
                    printf("Result: %s\n", resultPath);
                }
                
                remove(path);

                if(write(socket, reply, strlen(reply)) != strlen(reply)){
                    printf("*** Process %d: Error sending reply to client\n", _childID);
                }
            } 
            _childsData[_childID]._doWork = 0;
            close(socket);
            // printf("Terminated comm with client\n");

        }

        _childsData[_childID]._doWork = 0;
    }
    
    printf("Process %d exiting...\n", _childID);
    exitPython(sobelFunction);
    // close(_commSockets[_childID][CHILD_SOCKET]);
    exit(0);
}

static void sendSocket(int pChildSocket, int pSocket){
    struct msghdr m;
    struct cmsghdr *cm;
    struct iovec iov;
    char buf[CMSG_SPACE(sizeof(int))];
    char dummy[2];    

    memset(&m, 0, sizeof(m));
    m.msg_controllen = CMSG_SPACE(sizeof(int));
    m.msg_control = &buf;
    memset(m.msg_control, 0, m.msg_controllen);
    cm = CMSG_FIRSTHDR(&m);
    cm->cmsg_level = SOL_SOCKET;
    cm->cmsg_type = SCM_RIGHTS;
    cm->cmsg_len = CMSG_LEN(sizeof(int));
    *((int *)CMSG_DATA(cm)) = pSocket;
    m.msg_iov = &iov;
    m.msg_iovlen = 1;
    iov.iov_base = dummy;
    iov.iov_len = 1;
    dummy[0] = 0;   /* doesn't matter what data we send */
    sendmsg(pChildSocket, &m, 0);
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
    _commSockets = malloc(sizeof(int*) * _nProcesses);
    for(int i = 0; i < _nProcesses; i++){
        _commSockets[i] = malloc(sizeof(int) * 2);
    }

    for(int i = 0; i < _nProcesses; i++){
        _childID = i;
        socketpair(PF_LOCAL, SOCK_STREAM, 0, _commSockets[_childID]);
        _childs[i] = fork();
        if(_childs[i] == 0){
            printf("[son] pid %d with _childID %d\n", getpid(), _childID);
            close(_commSockets[_childID][PARENT_SOCKET]);
            signal(SIGTERM, handleSigTerm);
            signal(SIGCONT, handleSigCont);
            doWork();
        }
        else{
            close(_commSockets[_childID][CHILD_SOCKET]);
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

    while(!exitLoop){
        FD_ZERO(&readFds);
        FD_SET(STDIN_FILENO, &readFds);
        FD_SET(socketFD, &readFds);
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
                printf("TEST INPUT\n");
                fgets(input, sizeof(input), stdin);
                if(strncmp(input, "exit", 4) == 0){
                    printf("Terminating server...\n");
                    exitLoop = 1;
                }
            }

            if(FD_ISSET(socketFD, &readFds)){
                printf("Test\n");
                newSocketFD = accept(socketFD, (struct sockaddr *) &clientAddress, &clientLen);
               
                if (newSocketFD < 0) {
                    fprintf(stderr, "Error: Accepting a connection\n");
                    exitLoop = 1;
                    continue;
                }

                for(int i = 0; i < _nProcesses; i++){
                    if(_childsData[i]._doWork == FALSE){
                        sendSocket(_commSockets[i][PARENT_SOCKET], newSocketFD);
                        _childsData[i]._doWork = TRUE;
                        kill(_childs[i], SIGCONT);
                        break;
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

    getExecutablePath(argv);
    createSharedData();
    createProcesses();
    acceptConnections();

    for(int i = 0; i < _nProcesses; i++){
        close(_commSockets[i][PARENT_SOCKET]);
        free(_commSockets[i]);
    }
    free(_commSockets);
    
    return 0;
}