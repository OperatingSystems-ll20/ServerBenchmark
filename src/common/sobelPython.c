#include <stdio.h>
#include <sobelPython.h>


/**
 * 
 * @brief Initialize the embedded python environment
 * 
 * @param pSobelFunc PyObject of 'applySobel' function
 * @param pSaveFunc PyObject of 'saveImg' function
 */
void initPython(PyObject **pSobelFunc, PyObject **pSaveFunc, char *pExecPath){
    setenv("PYTHONPATH",".",1);
    PyObject *moduleString, *module, *dict, *sobelFunction, *saveImgFunction;
    Py_Initialize();

    PyObject* sysPath = PySys_GetObject((char*)"path");
    char pythonDir[PATH_MAX];
    strcpy(pythonDir, pExecPath);
    strcat(pythonDir, "/../python");
    PyObject* programName = PyUnicode_FromString(pythonDir);
    PyList_Append(sysPath, programName);
    Py_DECREF(programName);

    moduleString = PyUnicode_FromString((char*)"sobel");
    module = PyImport_Import(moduleString);

    *pSobelFunc = PyObject_GetAttrString(module, (char*)"applySobel");
    *pSaveFunc = PyObject_GetAttrString(module, (char*)"saveImg");

    Py_DECREF(moduleString);
    Py_DECREF(module);
}


/**
 * @brief Finalizes the embedded python environment
 * 
 * @param pSobel PyObject of 'applySobel' function
 * @param pSave PyObject of 'saveImg' function
 */
void exitPython(PyObject *pSobel, PyObject *pSave){
    Py_DECREF(pSobel); 
    Py_DECREF(pSave);
    Py_FinalizeEx();
}


/**
 * @brief Executes the "applySobel" funtion in python
 * 
 * @param pSobel PyObject of applySobel function
 * @param pImageToProcess Path of the src image
 * @return PyObject Resulting image after sobel filter
 * 
 */
PyObject *processImage(PyObject *pSobel, char *pImageToProcess){
    PyObject *imgResult;
    if(PyCallable_Check(pSobel)){
        PyObject *args = Py_BuildValue("(s)", pImageToProcess);
        imgResult = PyObject_CallObject(pSobel, args);
        PyErr_Print();
        Py_DECREF(args); 
    }
    else {
        printf("Image not processed\n");
        PyErr_Print();
    }
    return imgResult; 
}


/**
 * @brief Calls the python function 'saveImg' to write the resulting
 * image after applying the sobel filter
 * 
 * @param pSaveFunc PyObject of 'saveImg' function
 * @param pImage PyObject with result image
 * @param pPath Path where the image will be written
 */
void saveResultImage(PyObject *pSaveFunc, PyObject *pImage, char *pPath){
    if(PyCallable_Check(pSaveFunc)){
        PyObject *args = Py_BuildValue("Ns", pImage, pPath);
        PyObject_CallObject(pSaveFunc, args);
        Py_DECREF(args);
    }
    else{
        printf("Processed image not saved\n");
        PyErr_Print();
    }
}