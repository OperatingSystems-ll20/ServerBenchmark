#include <stdio.h>
#include <stdlib.h>

#include <socketFIFO.h>

/**
 * @brief Initializes a FIFO of size 'pSize'
 * 
 * This is a circular queue, meaning that the head
 * is moved forward after each pop operation. When
 * the head (or tail) reaches the end of the queue, 
 * if the number of elements currently inside is
 * less than the size of the queue; the head (or tail)
 * will be 'moved' to the start of the queue.
 * 
 * @param pFIFO Pointer to the SocketFIFO structure
 * @param pSize Maximun size of the queue
 * @return int Error code:
 *             0 -> Success
 *            -1 -> Error allocating memory
 *            -2 -> pSize greater than MAX_FIFO_SIZE
 */
int initFIFO(SocketFIFO *pFIFO, int pSize){
    int ret = 0;
    if(pSize < MAX_FIFO_SIZE){
        pFIFO->_sockets = malloc(sizeof(int) * pSize);
        if(pFIFO){
            pFIFO->count = pFIFO->head = pFIFO->tail = 0;
            pFIFO->size = pSize;
        }
        else ret = -1;
    }
    else ret = -2;

    return ret;     
}


/**
 * @brief Push a new element inside the queue
 * 
 * @param pFIFO Pointer to the SockerFIFO structure
 * @param pSocket Socket to insert
 * @return int Error code:
 *             0 -> Success
 *            -1 -> FIFO is full
 */
int pushFIFO(SocketFIFO *pFIFO, int pSocket){
    int ret = 0;
    if(pFIFO->count == pFIFO->size) ret = -1; //FIFO full

    else{
        pFIFO->_sockets[pFIFO->tail] = pSocket;
        pFIFO->count++;
        pFIFO->tail = (pFIFO->tail + 1) % pFIFO->size;
    }
    return ret;
}


/**
 * @brief Pop an element of the FIFO
 * 
 * Because this FIFO queue is made to hold socket 
 * descriptors and these are only positive values. 
 * It is safe to return a negative value to 
 * indicate an error.
 * 
 * @param pFIFO Pointer to the SockerFIFO structure
 * @return int Negative value: FIFO is empty
 *             Positive value: Value
 */
int popFIFO(SocketFIFO *pFIFO){
    int ret = 0;
    if(pFIFO->count == 0) return -1;

    else{
        ret = pFIFO->_sockets[pFIFO->head];
        pFIFO->count--;
        pFIFO->head = (pFIFO->head + 1) % pFIFO->size;
    }
    return ret;
}


/**
 * @brief Checks if the FIFO queue is empty
 * 
 * @param pFIFO Pointer to the SocketFIFO structure
 * @return int 1 if the queue is empty
 *             0 if the queue is not empty
 */
int emptyFIFO(SocketFIFO *pFIFO){
    if(pFIFO->count == 0) return 1;
    else return 0;
}


/**
 * @brief Frees the allocated memory for the FIFO queue
 * 
 * @param pFIFO Pointer to the SocketFIFO structure
 * @return int 
 */
void freeFIFO(SocketFIFO *pFIFO){
    free(pFIFO->_sockets);
}