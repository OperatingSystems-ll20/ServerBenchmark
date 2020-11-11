#include <stdio.h>
#include <stdlib.h>
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

int main(int argc, char *argv[]){
    _nProcesses = getArgs(argc, argv);
    if(!_nProcesses || _nProcesses < 0) {
        printf("Invalid number of procesess\n");
        exit(1);
    }
    

    return 0;
}