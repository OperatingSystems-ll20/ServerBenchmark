#include <limits.h>
#include <netinet/in.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/mman.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <sys/wait.h>
#include <unistd.h>

#include <consts.h>
#include <pathHelper.h>
#include <sobelPython.h>
#include <heavy.h>

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
 * @brief Function executed by the child processes
 * 
 * Handles a connection with a client. When the connection
 * ends, the child process terminates its execution.
 * 
 */
static void doWork(){
    int imageSize, transferSize, readSize, writeSize, ret;
    int saveImage = FALSE, connected = TRUE;
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

    //Send a notification to the client
    if(write(_newSocketFD, SERVER_REPLY, strlen(SERVER_REPLY)) != strlen(SERVER_REPLY)){
        printf("*** Process %lu: Error sending reply to client\n", getpid());
        connected = FALSE;
    }

    while(connected){
        imageSize = transferSize = readSize = writeSize = 0;
        bzero(buffer, MAX_BUFFER);
        FILE *receivedImg;
        
        ret = read(_newSocketFD, &imageSize, sizeof(int));
        if(ret < 0){
            printf("*** Process %lu: Error reading from socket\n", getpid());
            break;
        }
        if(imageSize == 0){
            printf("*** Process %lu: Invalid image size received\n", getpid());
            break;
        }
        if(imageSize == -1) break;

        printf("--- Process %lu: Image size received = %d\n", getpid(), imageSize);
        receivedImg = fopen(tmpPath, "w+"); //Unprocessed image
        if(receivedImg == NULL){
            printf("Process %lu: Can't open FILE... Image will not be processed\n", getpid());
            while(transferSize < imageSize){
                do{
                    readSize = read(_newSocketFD, buffer, sizeof(buffer));
                } while(readSize < 0);
                transferSize += readSize;
            }
            break;
        }

        //Receive image and write it to tmp
        while(transferSize < imageSize){
            do{
                readSize = read(_newSocketFD, buffer, sizeof(buffer));
            } while(readSize < 0);
            writeSize = fwrite(buffer, 1, readSize, receivedImg);
            if(readSize != writeSize) printf("Process %lu: Error writing image\n", getpid());
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
        if(write(_newSocketFD, SERVER_REPLY, strlen(SERVER_REPLY)) != strlen(SERVER_REPLY)){
            printf("*** Process %lu: Error sending reply to client\n", getpid());
        }

    }//end while(connected)

    printf("--- Process %lu exiting...\n", getpid());
    exitPython(sobelFunction, saveFunction);
    close(_newSocketFD);
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

    //Create directory: ServerBenchmark/Heavy_Server
    strcat(_currentWorkDir, HEAVY_SERVER_DIR);
    ret = createWorkDir(_currentWorkDir);
    if(ret != 0) return -2;

    //Create directory: ServerBenchmark/Heavy_Server/heavyRun_#
    ret = findNextDirectoryID(_currentWorkDir, &dirCount, DIR_REGEX);
    if(ret != 0) return -3;
    strcat(_currentWorkDir, "/heavyRun_");
    char countStr[32];
    sprintf(countStr, "%d", dirCount);
    strcat(_currentWorkDir, countStr);
    ret = createWorkDir(_currentWorkDir);
    if(ret != 0) return -4;

    return 0; 
}


/**
 * @brief Maps the shared memory used to communicate with 
 * the child processes
 * 
 * The '_processedImages' variable is shared so the child processes
 * know the total amount of images that has been processed.
 * This way they know when stop saving the images to disk.
 * 
 * A mutex is shared between all childs to synchronize the access
 * to the '_processedImages' variable.
 * 
 */
static void createSharedData(){
    int protection = PROT_READ | PROT_WRITE;
    int visibility = MAP_SHARED | MAP_ANONYMOUS;

    _processedImages = (int*)mmap(NULL, sizeof(int), protection, visibility, -1, 0);
    _mutex = mmap(NULL, sizeof(pthread_mutex_t), protection, visibility, -1, 0);

    *_processedImages = 0;

    pthread_mutexattr_init(&_mutexAttr);
    pthread_mutexattr_setpshared(&_mutexAttr, PTHREAD_PROCESS_SHARED);
    pthread_mutex_init(_mutex, &_mutexAttr);
}


/**
 * @brief Unmaps all the shared memory allocated
 * 
 */
static void unmapSharedMem(){
    munmap(_processedImages, sizeof(int));
    munmap(_mutex, sizeof(pthread_mutex_t));
}


/**
 * @brief Forks a new child process
 * 
 * The connection with the client was accepted by the parent
 * process before forking, so the child process inherits the
 * socket descriptor.
 * 
 * @return int Result of fork:
 *      0 -> Never returned (child process)
 *     -1 -> Fork error
 *     <0 -> Pid of the child process
 */
static int createProcess(){
    int pid;
    pid = fork();
    
    if(pid == 0){
        signal(SIGCHLD, SIG_DFL);
        doWork();
        exit(EXIT_SUCCESS);
    }
    else
        return pid;
}

/**
 * @brief Handles the signal SIGCHLD
 * 
 * When a child process terminates its execution a SIGCHLD
 * signal is send to the parent process, where it is catched
 * by this function. Here, the pid of the child process is 
 * reaped from process table (with waitpid), avoiding 
 * zombie processes.
 * 
 * Zombie processes could be avoided using
 *  signal(SIGCHLD, SIG_IGN);
 * instead of this custom signal handler.
 * 
 */
static void childTerminated(){
    pid_t pid;
    while(TRUE){
        pid = waitpid((pid_t)-1, NULL, WNOHANG);
        if(pid == 0) break;
        else if(pid == -1) break;
        else printf("--- Child [%d] terminated\n", pid);
    }
}


/**
 * @brief Main loop in the parent process
 * 
 * The parent process stays in this loop, checking for
 * new client connections or input from command line.
 * 
 * The process is asleep through the pselect() function, 
 * which allows to monitor multiple file descriptors; in
 * this case, monitors the socket binded to the port 9001
 * and the file descriptor of stdin. pselect() also returns
 * when a signal is sended to the process.
 * 
 * When a child process terminates its execution, a SIGCHLD
 * signal is sended to the parent process. Here, pselect()
 * returns a -1 value, setting 'errno' to EINTR.
 * 
 */
static void acceptConnections(){
    int socketFD, ret;
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

    while(!exitLoop){
        FD_ZERO(&readFds);
        FD_SET(STDIN_FILENO, &readFds);
        FD_SET(socketFD, &readFds);
        int available = pselect(socketFD+1, &readFds, NULL, NULL, NULL, NULL);
        if(available < 0){
            switch(errno){
                case EINTR: continue; //Return because of a signal
                default:
                    printf("Error on select\n");
                    close(_newSocketFD);
                    close(socketFD);
                    unmapSharedMem();
                    exit(EXIT_FAILURE);
            }  
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
            _newSocketFD = accept(socketFD, (struct sockaddr *) &clientAddress, &clientLen);
            if (_newSocketFD < 0) {
                printf("Error: Accepting a connection\n");
                exitLoop = TRUE;
                continue;
            }

            ret = createProcess();
            if(ret < 0){
                printf("Error: Cant't fork a new process\n");
                write(_newSocketFD, SERVER_REJECT, strlen(SERVER_REJECT));
                continue;
            }
            close(_newSocketFD); //Close socket in parent process
        }//End if socket

    }//End while

    unmapSharedMem();
    close(_newSocketFD);
    close(socketFD);
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

    signal(SIGCHLD, childTerminated);
    // signal(SIGCHLD, SIG_IGN); //Ignore signal, child is repead automatically
    createSharedData();
    acceptConnections();

    free(_execPath);
    free(_currentWorkDir);

    exit(EXIT_SUCCESS);
}