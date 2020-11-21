#ifndef _PATHHELPER_H
#define _PATHHELPER_H

#include <limits.h>

int checkPath(const char *pPath);
void getExecutablePath(char *pArgv[], char *pExecPath);
int createWorkDir(char *pPath);
int findNextDirectoryID(const char* pPath, int *pDirCounter);



#endif