#ifndef _SOCKETFIFO_H
#define _SOCKETFIFO_H

#define MAX_FIFO_SIZE 200

typedef struct SocketFIFO{
    int *_sockets;
    int size;
    int count;
    int head;
    int tail;
} SocketFIFO;

int initFIFO(SocketFIFO *pFIFO, int pSize);
int pushFIFO(SocketFIFO *pFIFO, int pSocket);
int popFIFO(SocketFIFO *pFIFO);
int emptyFIFO(SocketFIFO *pFIFO);
void freeFIFO(SocketFIFO *pFIFO);


#endif