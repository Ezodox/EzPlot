#ifndef PYUTILS_H__
#define PYUTILS_H__

#include <Python.h>
#include <vector>
using namespace std;

PyObject* vectorToList_Float(const vector<float>& data);

PyObject* vectorToTuple_Float(const vector<float>& data);

PyObject* vectorVectorToTuple_Float(const vector< vector< float > >& data);

vector<float> listTupleToVector_Float(PyObject* incoming);

vector<int> listTupleToVector_Int(PyObject* incoming);

vector<const char*> listTupleToVector_String(PyObject* incoming);

#endif