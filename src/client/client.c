#include <dirent.h>
#include <errno.h>
#include <fcntl.h>
#include <netinet/in.h>
#include <netdb.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <unistd.h>

#include <consts.h>
#include <pathHelper.h>
#include <client.h>

/**
 * @brief Gets and validates the command line arguments
 * 
 * @param pArgc Main argc
 * @param pArgv Main argv
 * @return int Error code:
 *             0 -> Success
 *            -1 -> IP don't corresponds to a known host
 *            -2 -> Image file doesn't exists
 *            -3 -> Specified path don't corresponds to file
 *            -4 -> Invalid port, number of threads or number of cycles
 */
static int getArgs(const int pArgc, char *pArgv[]){
    int result = 0;
    if(pArgc < 6 || pArgc > 6){
        printf("Usage: ./client <ip> <port> <image> <N-threads> <N-cycles>\n");
    }
    else{
        _serverIP = pArgv[1];
        _serverPort = atoi(pArgv[2]);
        _imagePath = pArgv[3];
        _nThreads = atoi(pArgv[4]);
        _nCycles = atoi(pArgv[5]);

        if(gethostbyname(_serverIP) == NULL){
            printf("*** Error: No such host\n");
            result = -1;
        }

        if(checkPath(_imagePath) == 1){
            FILE *tmp;
            if((tmp = fopen(_imagePath, "rb")) == NULL){
                printf("*** Invalid image\n");
                result = -2;
            }
            else{
                fseek(tmp, 0, SEEK_END);
                _imageSize = ftell(tmp);
            }
            fclose(tmp);
        }
        else{
            printf("*** Invalid image\n");
            result = -3;
        }

        if(_serverPort == 0 || _nThreads == 0 || _nCycles == 0){
            printf("*** Invalid parameters\n");
            result = -4;
        }

        return result;
    }
}


/**
 * @brief Creates the 'Results.txt' file
 * 
 * According to the specified server port, the file
 * 'Results.txt' is created in the corresponding
 * server work directory
 * 
 * @return FILE* File pointer
 */
static FILE *createFile(){
    FILE *fp;
    fp = fopen(_resultFilePath, "a");
    return fp;
}



/**
 * @brief Creates the file path of 'Results.txt'
 * 
 * @return int Error code:
 *             0 -> Success
 *            -1 -> Error allocating memory
 */
static int getResultFilePath(){
    int ret = 0;
    _resultFilePath = malloc(PATH_MAX);
    if(_resultFilePath == NULL) ret = -1;
    else{
        strcpy(_resultFilePath, getenv("HOME"));
        strcat(_resultFilePath, WORK_DIR);
        switch(_serverPort){
            case 9000:
                strcat(_resultFilePath, "/Pre_Heavy_Server");
                break;
            case 9001:
                strcat(_resultFilePath, "/Heavy_Server");
                break;
            case 9002:
                strcat(_resultFilePath, "/Sequential_Server");
                break;
        }
        strcat(_resultFilePath, "/Results.txt");
    }
    return ret;
}



/**
 * @brief Function executed by each thread
 * 
 * The thread open a new socket connection with the server
 * and starts to sends the images.
 * 
 * The first validation done is read the socket to know if
 * the server accept (OK) or reject (REJECTED) the connection.
 * After this, the client starts to sends the images, first 
 * sending the size of the image (in bytes) and then the
 * images itself.
 * 
 * Finally the client reads the socket waiting for the 'OK'
 * response of the server, meaning that it it done processing
 * the image. 
 * 
 * The measurement of response time corresponds to the time
 * that it took to the server to send the first confirmation
 * to the client (OK or REJECTED).
 * 
 * 
 * @param pArg ThreadData structure
 * @return void* 
 */
static void *threadWork(void *pArg){
    int socketFD;
    int counter = 0;
    int readSize;
    int rejected = 0;
    char buffer[MAX_BUFFER];
    char serverReply[32];
    struct timeval t1, t2, t3;
    ThreadData *data = (ThreadData*)pArg;

    socketFD = socket(AF_INET, SOCK_STREAM, 0);
    if (socket < 0) {
        data->_result = -1;
        printf("*** Thread %d: Can't open the socket\n", data->_id);
        return NULL;
    }
    gettimeofday(&t1, NULL); //Start time
    if( connect(socketFD, (struct sockaddr*)&data->_serverAddr, sizeof(data->_serverAddr)) == 0 ){
        
        
        //Receive notification from server
        read(socketFD, &serverReply, sizeof(serverReply));
        if(strcmp(serverReply, "REJECTED") == 0){
            printf("Connection rejected from server\n");
            rejected = 1;
            data->_result = -2;
            close(socketFD);
        }
        gettimeofday(&t3, NULL); //To measure response time

        while(counter < _nCycles && !rejected){
            bzero(buffer, MAX_BUFFER);
            bzero(serverReply, 32);
            off_t offset = 0;

            //Send image size
            // printf("Image size: %d\n", _imageSize);
            if(write(socketFD, (void*)&_imageSize, sizeof(int)) < 0){
                printf("*** Thread %d: Can't write to socket\n", data->_id);
                data->_result = -3;
                break;
            }

            //Send image
            while( (readSize = pread(_imageFP, buffer, MAX_BUFFER-1, offset)) > 0 ){
                while(write(socketFD, buffer, readSize) < 0); //Send
                bzero(buffer, sizeof(buffer));
                offset += readSize;
            }

            bzero(serverReply, 32);
            read(socketFD, &serverReply, sizeof(serverReply));
            if(strcmp(serverReply, "OK") == 0){
                // printf("Response of server = %s\n", serverReply);
                gettimeofday(&t2, NULL);
                data->_nRequests++;
            }           
            counter++;
        }
        if(!rejected){
            int notifyServer = -1;
            write(socketFD, (void*)&notifyServer, sizeof(int));
            close(socketFD);

            gettimeofday(&t2, NULL); //End time
            data->_totalTime = ((t2.tv_usec - t1.tv_usec)*1.0e-6) + (t2.tv_sec - t1.tv_sec);
            data->_averageTime = data->_totalTime / data->_nRequests;
            data->_responseTime = ((t3.tv_usec - t1.tv_usec)*1.0e-6) + (t3.tv_sec - t1.tv_sec);
        }        
    }
    else {
        data->_result = -4;
        printf("*** Thread %d: Can't connect to server\n", data->_id);
    }
}


/**
 * @brief Creates the pthreads that sends requests
 * 
 * Allocates the memory used for the list of threads
 * and the list of ThreadData structures that holds 
 * the data needed for each thread.
 * 
 * This function is used to take the first measurement
 * of the total execution time of the threads.
 * 
 * @param pServerAddress Struct used to open a new socket connection
 */
static void createThreads(struct sockaddr_in *pServerAddress){
    _threads = malloc(sizeof(pthread_t)*_nThreads);
    _threadData = malloc(sizeof(ThreadData) * _nThreads);

    gettimeofday(&_globalT1, NULL);
    for(int i = 0; i < _nThreads; i++){
        _threadData[i]._id = i;
        _threadData[i]._serverAddr = *pServerAddress;
        _threadData[i]._result = 0;
        _threadData[i]._nRequests = 0;
        _threadData[i]._totalTime = 0;
        _threadData[i]._averageTime = 0;
        _threadData[i]._responseTime = 0;
        pthread_create(&_threads[i], NULL, threadWork, (void*)&_threadData[i]);
    }
}


/**
 * @brief Waits for all spawn pthreads to terminate execution
 * 
 * This function is used to take the second measurement of
 * the total execution time of the threads.
 * 
 */
static void stopThreads(){
    for(int i = 0; i < _nThreads; i++){
        pthread_join(_threads[i], NULL);
    }
    gettimeofday(&_globalT2, NULL);
}


/**
 * @brief Dealloactes all the memory used
 * 
 */
static void freeMemory(){
    free(_threads);
    free(_threadData);
}


int main(int argc, char *argv[]){
    int ret = 0;
    ret = getArgs(argc, argv);
    if(ret < 0) exit(EXIT_FAILURE);

    _imageFP = open(_imagePath, O_RDONLY, 0);

    struct sockaddr_in serverAddr;
    struct hostent *server;
    server = gethostbyname(argv[1]);

    bzero((char *)&serverAddr, sizeof(serverAddr));
    serverAddr.sin_family = AF_INET;
    bcopy((char *)server->h_addr, (char *)&serverAddr.sin_addr.s_addr, server->h_length);
    serverAddr.sin_port = htons(_serverPort);

    createThreads(&serverAddr);
    stopThreads();

    //Collect result data
    int totalRequests = 0;
    int goodRequests = 0, badRequests = 0;
    double globalTime = 0;
    double globalAverage = 0;
    double totalResponseTime = 0;
    double averageResponseTime = 0;

    for(int i = 0; i < _nThreads; i++){
        if(_threadData[i]._result < 0) badRequests++;

        else{
            goodRequests++;
            totalRequests += _threadData[i]._nRequests;
            totalResponseTime += _threadData[i]._responseTime;
        }
    }
    
    //Write to file only if at least one thread finished execution correctly
    if(goodRequests != 0){
        globalTime = ((_globalT2.tv_usec - _globalT1.tv_usec)*1.0e-6) 
                    + (_globalT2.tv_sec - _globalT1.tv_sec);
        globalAverage = globalTime / totalRequests;
        averageResponseTime = totalResponseTime / goodRequests;

        getResultFilePath();
        FILE *fp = createFile();
        if(fp){
            fprintf(fp, "%d,%d,%.8f,%.8f,%.8f\n", 
                goodRequests, totalRequests, globalTime, globalAverage, averageResponseTime);
            fclose(fp);
        }
        else{
            printf("Error writing results to file\n");
        }
    }

    if(badRequests > 0 && badRequests != _nThreads) 
        printf("*** Warning: Some threads couldn't connect to the server\n");
    else if(badRequests == _nThreads) 
        printf("*** Error: The threads could not connect with the server\n");
    

    close(_imageFP);
    freeMemory();

    exit(EXIT_SUCCESS);        
}