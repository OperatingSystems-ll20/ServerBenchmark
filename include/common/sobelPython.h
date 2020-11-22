#ifndef _SOBELPYTHON_H
#define _SOBELPYTHON_H

#include <python3.7m/Python.h>

void initPython(PyObject **pSobelFunc, PyObject **pSaveFunc, char *pExecPath);
void exitPython(PyObject *pSobel, PyObject *pSave);
PyObject *processImage(PyObject *pSobel, char *pImageToProcess);
void saveResultImage(PyObject *pSaveFunc, PyObject *pImage, char *pPath);


#endif