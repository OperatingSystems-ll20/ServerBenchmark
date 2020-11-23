#include <arpa/inet.h>
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
#include <sobelPython.h>
#include <pathHelper.h>
#include <sequential.h>

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
 * @brief Custom handle for SIGCONT signal
 * 
 */
static void handleSigCont(){ return;}


/**
 * CHILD FUNCTION
 * 
 * @brief Custom handle for SIGTERM signal
 * 
 */
static void handleSigTerm(){
    _childExitLoop = TRUE;
}


/**
 * @brief Function executed by the only child process
 * 
 * Handles the connections with the clients. 
 * The child process notify the parent process when it
 * finishes handling a connection with a client. The parent 
 * then will check if there are available sockets in the 
 * FIFO queue; if there are, it will send it to the child 
 * and 'awake' it by sending a SIGCONT signal.
 * 
 * If there are no available sockets in the FIFO queue, the
 * child process remains 'asleep'.
 * 
 */
static void doWork(){
    int imageSize, transferSize, readSize, writeSize, ret;
    int saveImage = FALSE, connected = TRUE;
    _childExitLoop = FALSE;
    char buffer[MAX_BUFFER];
    char tmpPath[PATH_MAX];
    char *resultPath = malloc(PATH_MAX);
    char imageIDStr[32];
    strcpy(tmpPath, _currentWorkDir);
    strcat(tmpPath, "/tmp");
    sprintf(imageIDStr, "%d", getpid());
    strcat(tmpPath, imageIDStr);
    strcat(tmpPath, IMAGE_EXTENSION);

    PyObject *sobelFunction, *saveFunction; 
    initPython(&sobelFunction, &saveFunction, _execPath);

    while (!_childExitLoop){
        while(!(*_childDoWork) && !(*_childTerminate)) pause();

        if(*_childDoWork){
            int socket;
            ret = receiveSocket(_commSockets[CHILD_SOCKET], &socket);
            printf("--- New Socket received from parent\n");
            if(ret != 0) {
                printf("*** Process %d: Error receiving new Socket from parent\n", _childPID);
                *_childDoWork = FALSE;
                continue;
            }
            connected = TRUE;
            //Send a notification to the client
            if(write(socket, SERVER_REPLY, strlen(SERVER_REPLY)) != strlen(SERVER_REPLY)){
                printf("*** Process %d: Error sending reply to client\n", _childPID);
                connected = FALSE;
            }

            while(connected){
                imageSize = transferSize = readSize = writeSize = 0;
                bzero(buffer, MAX_BUFFER);
                FILE *receivedImg;
                
                ret = read(socket, &imageSize, sizeof(int));
                if(ret < 0){
                    printf("*** Process %d: Error reading from socket\n", _childPID);
                    break;
                }
                if(imageSize == 0){
                    printf("*** Process %d: Invalid image size received\n", _childPID);
                    break;
                }
                if(imageSize == -1) break;

                //printf("--- Process %d: Image size received = %d\n", _childPID, imageSize);
                receivedImg = fopen(tmpPath, "w+"); //Unprocessed image
                if(receivedImg == NULL){
                    printf("Process %d: Can't open FILE... Image will not be processed\n", _childPID);
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
                    if(readSize != writeSize) printf("Process %d: Error writing image\n", _childPID);
                    transferSize += readSize;
                }
                fclose(receivedImg);

                if(*_processedImages < MAX_IMAGES){
                    saveImage = TRUE;
                    sprintf(imageIDStr, "%d", *_processedImages);
                    (*_processedImages)++;                    
                }
                else {
                    saveImage = FALSE;
                }

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
                    printf("*** Process %d: Error sending reply to client\n", _childPID);
                }

            }//end while(connected)
            *_childDoWork = FALSE;
            close(socket);

            //Notify parent process
            if(write(_pipes[CHILD_SOCKET], "READY", 5) != 5){
                printf("*** Error sending READY message to parent");
            }
            printf("--- Finished handling a client\n");

        }//end if(_childDoWork)

        else if(*_childTerminate == TRUE) _childExitLoop = TRUE;

    }//end while

    printf("--- Process %d exiting...\n", _childPID);
    exitPython(sobelFunction, saveFunction);
    free(_execPath);
    free(_currentWorkDir);
    close(_commSockets[CHILD_SOCKET]);
    close(_pipes[CHILD_SOCKET]);
    exit(EXIT_SUCCESS);
}


/**
 * @brief Create the directory tree of the current execution
 * 
 * @param pArgv Main argv
 * @return int  Error code
 *              0 -> Success
 *             -1 -> Error creating main directory
 *             -2 -> Error creating the server directory
 *             -3 -> Error obtaining next directory ID
 *             -4 -> Error creating directory of current execution
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

    //Create directory: ServerBenchmark/Sequential_Server
    strcat(_currentWorkDir, SEQUENTIAL_SERVER_DIR);
    ret = createWorkDir(_currentWorkDir);
    if(ret != 0) return -2;

    //Create directory: ServerBenchmark/Sequential_Server/sequentialRun_#
    ret = findNextDirectoryID(_currentWorkDir, &dirCount, DIR_REGEX);
    if(ret != 0) return -3;
    strcat(_currentWorkDir, "/sequentialRun_");
    char countStr[32];
    sprintf(countStr, "%d", dirCount);
    strcat(_currentWorkDir, countStr);
    ret = createWorkDir(_currentWorkDir);
    if(ret != 0) return -4;

    return 0; 
}



/**
 * @brief Maps the shared memory used to communicate with the child processes
 * 
 */
static void createSharedData(){
    int protection = PROT_READ | PROT_WRITE;
    int visibility = MAP_SHARED | MAP_ANONYMOUS;

    _childDoWork = (int*)mmap(NULL, sizeof(int), protection, visibility, -1, 0);
    _childTerminate = (int*)mmap(NULL, sizeof(int), protection, visibility, -1, 0);
    _processedImages = (int*)mmap(NULL, sizeof(int), protection, visibility, -1, 0);

    *_processedImages = 0;
    *_childDoWork = FALSE;
    *_childTerminate = FALSE;
}



/**
 * @brief Unmaps all the shared memory allocated
 * 
 */
static void unmapSharedMem(){
    munmap(_childTerminate, sizeof(int));
    munmap(_childDoWork, sizeof(int));
    munmap(_processedImages, sizeof(int));
}



/**
 * @brief Sends a socket descriptor to the child process
 * 
 * @param pChildSocket Socket connected to the child process
 * @param pSocket Socket descriptor to send
 */
static void sendSocket(int pParentSocket, int pSocket){
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
    ssize_t ret = sendmsg(pParentSocket, &m, 0);
}


/**
 * @brief Forks the child process
 * 
 * @return int Result of fork:
 *      0 -> Never returned (child process)
 *     -1 -> Fork error
 *     >0 -> Pid of the child process
 */
static int createChildProcess(){
    int ret = pipe(_pipes);
    if(ret < 0) return -2;

    socketpair(PF_LOCAL, SOCK_STREAM, 0, _commSockets);
    _childPID = fork();
    
    if(_childPID == 0){
        close(_pipes[PARENT_SOCKET]);
        close(_commSockets[PARENT_SOCKET]);
        signal(SIGTERM, handleSigTerm);
        signal(SIGCONT, handleSigCont);
        doWork();
    }
    else{
        close(_pipes[CHILD_SOCKET]);
        close(_commSockets[CHILD_SOCKET]);
        return _childPID;
    }
}


/**
 * @brief Waits for the child process to terminate execution
 * 
 */
static void killChildProcess(){
    *_childTerminate = TRUE;
    kill(_childPID, SIGTERM);
    wait(NULL);
    printf("--- Child [%d] terminated\n", _childPID);
}


/**
 * @brief Close all the sockets in the FIFO queue
 * 
 * In case the server is closed before the child process
 * finished handling all the clients, a 'REJECTED' message 
 * is send to all the clients on hold and their sockets are closed.
 * 
 */
static void closeAllSockets(){
    while(emptyFIFO(&_socketFIFO) == FALSE){
        int socket = popFIFO(&_socketFIFO);
        write(socket, SERVER_REJECT, strlen(SERVER_REJECT));
        close(popFIFO(&_socketFIFO));
    }
}


/**
 * @brief Main loop in the parent process
 * 
 * The parent process stays in this loop, waiting for:
 *      * New client connections
 *      * Input from command line
 *      * Messages from child process
 * If none of the previous events occur, the process stays
 * 'asleep'.
 * 
 * When a new connection from a client is detected, it is
 * accepted and the socket is pushed into the FIFO queue.
 * If the child process is available, it will send the next
 * socket inside the FIFO queue.
 * 
 * The command is checked for the 'exit' message, to terminate
 * the execution of the server.
 * 
 * If a message from the child process is detected, it means
 * that it finished a connection with a client. So a new socket
 * from the FIFO queue is sended to it.
 * 
 */
static void acceptConnections(){
    int socketFD, newSocketFD, ret;
    int assigned = FALSE, exitLoop = FALSE;
    socklen_t clientLen;
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

    int maxSocket = (_pipes[PARENT_SOCKET] > socketFD) ? _pipes[PARENT_SOCKET] : socketFD;

    while(!exitLoop){
        FD_ZERO(&readFds);
        FD_SET(STDIN_FILENO, &readFds);
        FD_SET(_pipes[PARENT_SOCKET], &readFds);
        FD_SET(socketFD, &readFds);
        int available = select(maxSocket+1, &readFds, NULL, NULL, NULL);
        if(available < 0){
            printf("Error on select\n");
            killChildProcess();
            closeAllSockets();
            close(socketFD);
            close(newSocketFD);
            unmapSharedMem();
            exit(EXIT_FAILURE);
        }

        if(FD_ISSET(STDIN_FILENO, &readFds)){
            fgets(input, sizeof(input), stdin);
            if(strncmp(input, "exit", 4) == 0){
                printf("Terminating server...\n");
                exitLoop = TRUE;
                continue;
            }
        }//End if stdin

        if(FD_ISSET(socketFD, &readFds)){
            newSocketFD = accept(socketFD, (struct sockaddr *) &clientAddress, &clientLen);
            if (newSocketFD < 0) {
                printf("Error: Accepting a connection\n");
                exitLoop = TRUE;
                continue;
            }

            //Insert socket in FIFO  
            ret = pushFIFO(&_socketFIFO, newSocketFD);    
            if(ret < 0){
                printf("FIFO full... rejecting new connections\n");
                write(newSocketFD, SERVER_REJECT, strlen(SERVER_REJECT));
                close(newSocketFD);
            }

            if(*_childDoWork == FALSE){
                if(!emptyFIFO(&_socketFIFO)){
                    int socket = popFIFO(&_socketFIFO);
                    sendSocket(_commSockets[PARENT_SOCKET], socket);
                    *_childDoWork = TRUE;
                    kill(_childPID, SIGCONT);
                }
            }
        }//End if socket

        if(FD_ISSET(_pipes[PARENT_SOCKET], &readFds)){
            char buff[32];
            read(_pipes[PARENT_SOCKET], buff, 32);
            if(strncmp(buff, "READY", 5) == 0 && *_childDoWork == FALSE){
                if(!emptyFIFO(&_socketFIFO)){
                    int socket = popFIFO(&_socketFIFO);
                    sendSocket(_commSockets[PARENT_SOCKET], socket);
                    *_childDoWork = TRUE;
                    kill(_childPID, SIGCONT);
                }     
            }
               
        }//End if commSocket

    }//end while(!exitLoop)

    killChildProcess();
    unmapSharedMem();
    closeAllSockets();
    close(newSocketFD);
    close(socketFD);
    close(_commSockets[PARENT_SOCKET]);
    close(_pipes[PARENT_SOCKET]);
}


int main(int argc, char *argv[]){
    int ret = 0;

    ret = createDirectories(argv);
    if(ret != 0){
        printf("*** Error creating directories\n");
        free(_execPath);
        free(_currentWorkDir);
        exit(EXIT_FAILURE);
    }
    printf("--- Work directory: %s\n\n", _currentWorkDir);

    createSharedData();
    createChildProcess();
    initFIFO(&_socketFIFO, 100);
    acceptConnections();

    free(_execPath);
    free(_currentWorkDir);
    freeFIFO(&_socketFIFO);

    exit(EXIT_SUCCESS);
}

