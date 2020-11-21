#include <dirent.h>
#include <errno.h>
#include <netinet/in.h>
#include <netdb.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <fcntl.h>
#include <sys/types.h>
#include <unistd.h>
#include <client.h>

/**
 * Checks if a path corresponds to a directory
 *
 * @param  pPath Path
 * @return       Result
 *                0 -> Directory exists
 *                1 -> Path is not a directory
 *                2 -> Path doesn exists
 */
static int checkPath(const char *pPath){
    DIR *dir = opendir(pPath);
    if(dir){
        closedir(dir);
        return 0;
    }
    else if(errno == ENOTDIR)
        return 1;

    else return 2;
}


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

static void *threadWork(void *pArg){
    int socketFD;
    int counter = 0;
    int readSize;
    char buffer[MAX_BUFFER];
    char serverReply[2];
    ThreadData *data = (ThreadData*)pArg;

    socketFD = socket(AF_INET, SOCK_STREAM, 0);
    if (socket < 0) {
        data->_result = -1;
        printf("*** Thread %d: Can't open the socket\n", data->_id);
        return NULL;
    }

    if( connect(socketFD, (struct sockaddr*)&data->_serverAddr, sizeof(data->_serverAddr)) == 0 ){
        while(counter < _nCycles){
            bzero(buffer, MAX_BUFFER);
            off_t offset = 0;

            //Send image size
            // printf("Image size: %d\n", _imageSize);
            if(write(socketFD, (void*)&_imageSize, sizeof(int)) < 0){
                printf("*** Thread %d: Can't write to socket\n", data->_id);
                data->_result = -2;
                break;
            }

            //Send image
            while( (readSize = pread(_imageFP, buffer, MAX_BUFFER-1, offset)) > 0 ){
                while(write(socketFD, buffer, readSize) < 0); //Send
                bzero(buffer, sizeof(buffer));
                offset += readSize;
            }

            read(socketFD, &serverReply, sizeof(serverReply));
            printf("Response of server = %s\n", serverReply);

            counter++;
        }
        int notifyServer = -1;
        write(socketFD, (void*)&notifyServer, sizeof(int));
        close(socketFD);
    }
    else {
        data->_result = -3;
        printf("*** Thread %d: Can't connect to server\n", data->_id);
    }

}

static void createThreads(struct sockaddr_in *pServerAddress){
    _threads = malloc(sizeof(pthread_t)*_nThreads);
    _threadData = malloc(sizeof(ThreadData) * _nThreads);

    for(int i = 0; i < _nThreads; i++){
        _threadData[i]._id = i;
        _threadData[i]._serverAddr = *pServerAddress;
        _threadData[i]._result = 0;
        pthread_create(&_threads[i], NULL, threadWork, (void*)&_threadData[i]);
    }
}

static void stopThreads(){
    for(int i = 0; i < _nThreads; i++){
        pthread_join(_threads[i], NULL);
    }

    
}

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

    for(int i = 0; i < _nThreads; i++){
        if(_threadData[i]._result < 0){
            printf("*** Error: Some threads couldn't connect to the server\n");
            break;
        }
    }

    close(_imageFP);
    freeMemory();

    exit(EXIT_SUCCESS);        
}