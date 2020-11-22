#include <dirent.h>
#include <errno.h>
#include <limits.h>
#include <regex.h>
#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/stat.h>
#include <unistd.h>

#include <pathHelper.h>


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


/**
 * @brief Obtains the absolute path where the executable is located
 * 
 * @param pArgv Main argv
 * @param pExecPath Executable path (Reference)
 */
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
    strncpy(pExecPath, execPath, sizeof(execPath));
}


/**
 * @brief Creates a directory
 * 
 * If the directory already exists, the
 * operation is ignored
 * 
 * @param pPath Path of the new directory
 * @return int Error code:
 *             0 -> Success
 *            -1 -> Error creating directory
 */
int createWorkDir(char *pPath){
    int result = 0;
    int exist = checkPath(pPath);
    if(exist != 0){
        result = mkdir(pPath, 0777);
    }

    return result;
}


/**
 * Reads the name of all subdirectories inside the given path
 * to determine the next id for the current work directory.
 *
 * @param  pPath Base directory
 * @return       Error code:
 *               0 -> Success
 *              -1 -> Could not open directory in 'pPath'
 *              -2 -> Could not compile regex
 */
int findNextDirectoryID(const char* pPath, int *pDirCounter, const char *pRegex) {
    regex_t regex;
    int ret, id;
    struct dirent* dent;
    DIR* srcdir = opendir(pPath);

    if (srcdir == NULL) {
        printf("Could not open %s\n", pPath);
        return -1;
    }

    ret = regcomp(&regex, pRegex, REG_EXTENDED);
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