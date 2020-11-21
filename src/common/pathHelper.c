#include <pathHelper.h>
#include <limits.h>
#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <dirent.h>
#include <sys/stat.h>
#include <regex.h>
#include <unistd.h>
#include <errno.h>

/**
 * Checks if a path corresponds to a directory
 *
 * @param  pPath Path
 * @return       Result
 *                0 -> Directory exists
 *                1 -> Path is not a directory
 *                2 -> Path doesn exists
 */
int checkPath(const char *pPath){
    DIR *dir = opendir(pPath);
    if(dir){
        closedir(dir);
        return 0;
    }
    else if(errno == ENOTDIR)
        return 1;

    else return 2;
}

void getExecutablePath(char *pArgv[], char *pExecPath){
    char path_save[PATH_MAX];
    char execPath[PATH_MAX];
    char *p;

    if(!(p = strrchr(pArgv[0], '/')))
        getcwd(execPath, sizeof(execPath));
    else{
        *p = '\0';
        getcwd(path_save, sizeof(path_save));
        chdir(pArgv[0]);
        getcwd(execPath, sizeof(execPath));
        chdir(path_save);
    }
    printf("Calculated path is: %s\n", execPath);
    strncpy(pExecPath, execPath, sizeof(execPath));
    printf("Absolute path to executable is: %s\n", pExecPath);
}

int createWorkDir(char *pPath){
    int result = 0;
    int exist = checkPath(pPath);
    if(exist != 0){
        result = mkdir(pPath, 0777);
    }

    return result;
}


/**
 * Reads the name of all directories inside the base directory
 * to determine the next id for the current work directory.
 *
 * @param  pPath Base directory
 * @return       Error code
 */
int findNextDirectoryID(const char* pPath, int *pDirCounter) {
    regex_t regex;
    int ret, id;
    struct dirent* dent;
    DIR* srcdir = opendir(pPath);

    if (srcdir == NULL) {
        printf("Could not open %s\n", pPath);
        return -1;
    }

    ret = regcomp(&regex, "^preHeavyRun_[[:digit:]]+$", REG_EXTENDED);
    if(ret){
        fprintf(stderr, "Could not compile regex\n");
        closedir(srcdir);
        return -2;
    }

    while((dent = readdir(srcdir)) != NULL) {
        struct stat st;
        if (fstatat(dirfd(srcdir), dent->d_name, &st, 0) < 0) {
            perror(dent->d_name);
            continue;
        }

        //Is a file
        if (S_ISDIR(st.st_mode)) {
            ret = regexec(&regex, dent->d_name, 0, NULL, 0);
            if(!ret){
                strtok((char*)dent->d_name, "_");
                char *idStr = strtok(NULL, "_");
                id = atoi(idStr);
                if(id > *pDirCounter) *pDirCounter = id;
            }
        }
    }
    (*pDirCounter)++;
    regfree(&regex);
    closedir(srcdir);
    return 0;
}