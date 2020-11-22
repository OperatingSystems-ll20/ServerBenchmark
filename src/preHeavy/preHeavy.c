#include <arpa/inet.h>
#include <locale.h>
#include <netinet/in.h>
#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/wait.h> 
#include <sys/mman.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h> 

#include <consts.h>
#include <pathHelper.h>
#include <sobelPython.h>
#include <preHeavy.h>

/**
 * @brief Get the number of heavy processes to spawn
 * 
 * @param pArgc  Main argc
 * @param pArgv  Main argv
 * @return int   Number of heavy processes
 */
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

/**
 * @brief Create the directory tree of the current execution
 * 
 * @param pArgv Main argv
 * @return int  Error code
 *              0 -> Success
 *             -1 -> Error creating main directory
 *             -2 -> Error obtaining next directory ID
 *             -3 -> Error creating directory of current execution
 */
static int createDirectories(char *pArgv[]){
    int ret = 0;
    int dirCount = 0;
    _execPath = malloc(PATH_MAX);
    _currentWorkDir = malloc(PATH_MAX);

    getExecutablePath(pArgv, _execPath);

    //Create directory: ServerBenchmark
    strcpy(_currentWorkDir, getenv("HOME"));
    strcat(_currentWorkDir, WORK_DIR);
    ret = createWorkDir(_currentWorkDir);
    if(ret != 0) return -1;

    //Create directory: ServerBenchmark/preHeavyRun_#
    ret = findNextDirectoryID(_currentWorkDir, &dirCount, DIR_REGEX);
    if(ret != 0) return -2;
    strcat(_currentWorkDir, "/preHeavyRun_");
    char countStr[32];
    sprintf(countStr, "%d", dirCount);
    strcat(_currentWorkDir, countStr);
    ret = createWorkDir(_currentWorkDir);
    if(ret != 0) return -3;

    return 0;    
}

/**
 * CHILD FUNCTION
 * 
 * @brief Custom handle for SIGTERM signal
 * 
 */
static void handleSigTerm(){
    _exitLoop = 1;
}

/**
 * CHILD FUNCTION
 * 
 * @brief Custom handle for SIGCONT signal
 * 
 */
static void handleSigCont(){ return;}


/**
 * CHILD FUNCTION
 * 
 * @brief Receive a socket descriptor sended by the 
 * parent process 
 * 
 * @param pChildSocket Comm socket with parent
 * @param pNewSocket Socket received (reference)
 * @return int Error code:
 *             0 -> Success
 *            -1 -> Error receiving socket descriptor
 */
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
    if(recvmsg(pChildSocket, &m, 0) == -1) return -1;
 
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


/**
 * CHILD FUNCTION
 * 
 * @brief Creates a new file name (complete path) where a processed
 * image will be saved
 * 
 * @param pResultPath Resulting path (Reference)
 * @param pImageIdStr Suffix added to the file name
 */
static void getNewPath(char *pResultPath, char *pImageIdStr){
    strcpy(pResultPath, _currentWorkDir);
    strcat(pResultPath, IMAGE_NAME); 
    strcat(pResultPath, pImageIdStr);
    strcat(pResultPath, IMAGE_EXTENSION); 
}


/**
 * CHILD FUNCTION
 * 
 * @brief Main function executed by the child processes.
 * 
 * Handles the communication with a client. Upon awakening to
 * handle a new connection, the process sends an acknowledgement
 * to the client letting it know that the connection was accepted.
 * 
 */
static void doWork(){
    int imageSize, transferSize, readSize, writeSize, ret;
    int saveImage = FALSE, connected = FALSE;
    char buffer[MAX_BUFFER];
    char tmpPath[PATH_MAX];
    char *resultPath = malloc(PATH_MAX);
    char imageIDStr[32];
    strcpy(tmpPath, _currentWorkDir);
    strcat(tmpPath, "/tmp");
    sprintf(imageIDStr, "%d", _childID);
    strcat(tmpPath, imageIDStr);
    strcat(tmpPath, IMAGE_EXTENSION);

    PyObject *sobelFunction, *saveFunction; 
    initPython(&sobelFunction, &saveFunction, _execPath);

    while (!_exitLoop){
        while(!_childsData[_childID]._doWork && !_childsData[_childID]._terminate) pause();

        //Handle client
        if(_childsData[_childID]._doWork){
            int socket;
            ret = receiveSocket(_commSockets[_childID][CHILD_SOCKET], &socket);
            if(ret != 0) {
                printf("*** Process %d: Error receiving new Socket from parent\n", _childID);
                _childsData[_childID]._doWork = 0;
                continue;
            }
            connected = TRUE;
            //Send a notification to the client
            if(write(socket, SERVER_REPLY, strlen(SERVER_REPLY)) != strlen(SERVER_REPLY)){
                printf("*** Process %d: Error sending reply to client\n", _childID);
                connected = FALSE;
            }
            while(connected){
                imageSize = transferSize = readSize = writeSize = 0;
                bzero(buffer, MAX_BUFFER);
                FILE *receivedImg;
                
                ret = read(socket, &imageSize, sizeof(int));
                if(ret < 0){
                    printf("*** Process %d: Error reading from socket\n", _childID);
                    break;
                }
                if(imageSize == 0){
                    printf("*** Process %d: Invalid image size received\n", _childID);
                    break;
                }
                if(imageSize == -1) break;

                //printf("--- Process %d: Image size received = %d\n", _childID, imageSize);
                receivedImg = fopen(tmpPath, "w+"); //Unprocessed image
                if(receivedImg == NULL){
                    printf("Process %d: Can't open FILE... Image will not be processed\n", _childID);
                    while(transferSize < imageSize){
                        do{
                            readSize = read(socket, buffer, sizeof(buffer));
                        } while(readSize < 0);
                        transferSize += readSize;
                    }
                    break;
                }

                //Receive image and write it to tmp
                while(transferSize < imageSize){
                    do{
                        readSize = read(socket, buffer, sizeof(buffer));
                    } while(readSize < 0);
                    writeSize = fwrite(buffer, 1, readSize, receivedImg);
                    if(readSize != writeSize) printf("Process %d: Error writing image\n", _childID);
                    transferSize += readSize;
                }
                fclose(receivedImg);

                               
                pthread_mutex_lock(_mutex);
                if(*_processedImages < MAX_IMAGES){
                    saveImage = TRUE;
                    sprintf(imageIDStr, "%d", *_processedImages);
                    (*_processedImages)++;                    
                }
                else {
                    saveImage = FALSE;
                }
                pthread_mutex_unlock(_mutex);

                PyObject *imageResult = processImage(sobelFunction, tmpPath);

                if(saveImage) {
                    getNewPath(resultPath, imageIDStr);
                    saveResultImage(saveFunction, imageResult, resultPath);
                    bzero(resultPath, PATH_MAX);
                    bzero(imageIDStr, 32);
                }
                remove(tmpPath); //Delete tmp image

                //Reply to the client
                if(write(socket, SERVER_REPLY, strlen(SERVER_REPLY)) != strlen(SERVER_REPLY)){
                    printf("*** Process %d: Error sending reply to client\n", _childID);
                }
            } 
            _childsData[_childID]._doWork = 0;
            close(socket);
        }

        //If _terminate is TRUE, child must end its execution
        else if(_childsData[_childID]._terminate == TRUE) _exitLoop = TRUE;
        _childsData[_childID]._doWork = 0;
    }
    
    printf("--- Process %d exiting...\n", _childID);
    exitPython(sobelFunction, saveFunction);
    close(_commSockets[_childID][CHILD_SOCKET]);
    exit(0);
}


/**
 * @brief Sends a socket descriptor to one of the child process
 * 
 * @param pChildSocket Socket connected to the child process
 * @param pSocket Socket descriptor to send
 */
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
    dummy[0] = 0;
    ssize_t ret = sendmsg(pChildSocket, &m, 0);
}


/**
 * @brief Maps the shared memory used to communicate with the child processes
 * 
 */
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
    }

    pthread_mutexattr_init(&_mutexAttr);
    pthread_mutexattr_setpshared(&_mutexAttr, PTHREAD_PROCESS_SHARED);
    pthread_mutex_init(_mutex, &_mutexAttr);
}

/**
 * @brief Creates all the child processes using fork.
 * 
 * Before forking, a socket pair is created to stablish
 * a channel of communication with each child process, 
 * through which the socket descriptors are send.
 * 
 */
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
            // printf("[son] pid %d with _childID %d\n", getpid(), _childID);
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


/**
 * @brief Kill all the child processes
 * 
 * The flag '_terminate' of a child process is set to TRUE
 * and then it is 'awakened' sending a SIGTERM signal. Upon
 * checking the state of this flag, the process will exit its
 * loop and finish execution.
 * 
 * This process is done for all the processes.
 * 
 */
static void killProcesses(){
    for(int i = 0; i < _nProcesses; i++){
        _childsData[i]._terminate = 1;
        kill(_childs[i], SIGTERM);
        wait(NULL);
    }
}


/**
 * @brief Unmaps all the shared memory allocated
 * 
 */
static void unmapSharedMem(){
    munmap(_processedImages, sizeof(int));
    munmap(_mutex, sizeof(pthread_mutex_t));
    munmap(_childsData, sizeof(Data)*_nProcesses);
}


/**
 * @brief Main loop of parent process
 * 
 * The parent process stays in this loop, checking for
 * new client connections or input from command line.
 * 
 * The process is asleep through the select() function, 
 * which allows to monitor multiple file descriptors; in
 * this case, monitors the socket binded to the port 9000
 * and the file descriptor of stdin.
 * 
 * If there aren't any incoming connections through the socket
 * or no input detected from command line, the process is asleep.
 * 
 * If the input received from stdin is 'exit', the server finishes
 * its execution.
 * 
 * In the case that there no more process available to handle connections,
 * the servers starts to 'reject' connections by sending an acknowledgement
 * to the client and then closing the socket.
 * 
 */
static void acceptConnections(){
    int socketFD, newSocketFD, ret;
    int assigned = FALSE;
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
            exit(EXIT_FAILURE);
        }

        else{
            if(FD_ISSET(STDIN_FILENO, &readFds)){
                fgets(input, sizeof(input), stdin);
                if(strncmp(input, "exit", 4) == 0){
                    printf("Terminating server...\n");
                    exitLoop = TRUE;
                    continue;
                }
            }

            if(FD_ISSET(socketFD, &readFds)){
                newSocketFD = accept(socketFD, (struct sockaddr *) &clientAddress, &clientLen);
                if (newSocketFD < 0) {
                    printf("Error: Accepting a connection\n");
                    exitLoop = TRUE;
                    continue;
                }

                //Connection is assigned to the first available process
                for(int i = 0; i < _nProcesses; i++){
                    if(_childsData[i]._doWork == FALSE){
                        sendSocket(_commSockets[i][PARENT_SOCKET], newSocketFD);
                        _childsData[i]._doWork = TRUE;
                        kill(_childs[i], SIGCONT);
                        assigned = TRUE;
                        break;
                    }
                }

                //Reject connection if all processes are busy
                if(!assigned) {
                    write(newSocketFD, SERVER_REJECT, strlen(SERVER_REJECT));
                    close(newSocketFD);
                }
                assigned = FALSE;
            }
        }
    }

    killProcesses();
    unmapSharedMem();
    close(newSocketFD);
    close(socketFD);
}

int main(int argc, char *argv[]){
    int ret = 0;
    _nProcesses = getArgs(argc, argv);
    if(!_nProcesses || _nProcesses < 0) {
        printf("Invalid number of procesess\n");
        exit(1);
    }

    ret = createDirectories(argv);
    if(ret != 0){
        printf("*** Error creating directories\n");
        free(_execPath);
        free(_currentWorkDir);
        exit(EXIT_FAILURE);
    }
    printf("--- Work directory: %s\n\n", _currentWorkDir);

    createSharedData();
    createProcesses();
    acceptConnections();

    //Free allocated memory
    for(int i = 0; i < _nProcesses; i++){
        close(_commSockets[i][PARENT_SOCKET]);
        free(_commSockets[i]);
    }
    free(_commSockets);
    free(_execPath);
    free(_currentWorkDir);
    
    return 0;
}