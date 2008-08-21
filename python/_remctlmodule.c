/*
 * Low-level Python interface to remctl.
 *
 * A direct Python equivalent to the libremctl API.  Normally, Python scripts
 * should not use this interface directly; instead, they should use the remctl
 * Python wrapper around this class.
 *
 * Written by Thomas L. Kula <kula@tproa.net>
 * Copyright 2008 Thomas L. Kula <kula@tproa.net>
 * Copyright 2008 Board of Trustees, Leland Stanford Jr. University
 *
 * See LICENSE for licensing terms.
 */

#include <Python.h>

#include <errno.h>
#include <stdlib.h>
#include <string.h>
#include <sys/uio.h>
#include <unistd.h>

#include <remctl.h>

#define VERSION "0.4"

static PyObject *
py_remctl(PyObject *self, PyObject *args)
{
    struct remctl_result *rr;
    char *host = NULL;
    unsigned short port;
    char *principal = NULL;
    const char **command = NULL;
    PyObject *list = NULL;
    PyObject *tmp = NULL;
    int length, i;
    PyObject *result = NULL;

    if (!PyArg_ParseTuple(args, "sHsO", &host, &port, &principal, &list))
        return NULL;

    /*
     * The command is passed as a list object.  For the remctl API, we need to
     * turn it into a NULL-terminated array of pointers.
     */
    length = PyList_Size(list);
    command = malloc((length + 1) * sizeof(char *));
    if (command == NULL)
        return NULL;
    for (i = 0; i < length; i++) {
        tmp = PyList_GetItem(list, i);
        if (tmp == NULL)
            goto end;
        command[i] = PyString_AsString(tmp);
        if (command[i] == NULL)
            goto end;
    }
    command[i] = NULL;

    rr = remctl(host, port, principal, command);
    result = Py_BuildValue("(ss#s#i)", rr->error,
                           rr->stdout_buf, (int) rr->stdout_len,
                           rr->stderr_buf, (int) rr->stderr_buf,
                           rr->status);
    remctl_result_free(rr);

end:
    if (command != NULL)
        free(command);
    return result;
}


/*
 * Called when the Python object is destroyed.  Clean up the underlying
 * libremctl object.
 */
static void
remctl_destruct(void *r)
{
    remctl_close(r);
}


static PyObject *
py_remctl_new(PyObject *self, PyObject *args)
{
    struct remctl *r;

    r = remctl_new();
    if (r == NULL) {
        Py_INCREF(PyExc_Exception);
        PyErr_SetFromErrno(PyExc_Exception);
        return NULL;
    }
    return PyCObject_FromVoidPtr(r, remctl_destruct);
}


static PyObject *
py_remctl_open(PyObject *self, PyObject *args)
{
    PyObject *object = NULL;
    char *host = NULL;
    unsigned short port = 0;
    char *principal = NULL;
    struct remctl *r;
    int status;

    if (!PyArg_ParseTuple(args, "OsHs", &object, &host, &port, &principal))
        return NULL;
    r = PyCObject_AsVoidPtr(object);
    status = remctl_open(r, host, port, principal);
    return Py_BuildValue("i", status);
}


static PyObject *
py_remctl_close(PyObject *self, PyObject *args)
{
    PyObject *object = NULL;
    struct remctl *r;

    if (!PyArg_ParseTuple(args, "O", &object))
        return NULL;
    r = PyCObject_AsVoidPtr(object);
    remctl_close(r);
    Py_INCREF(Py_None);
    return Py_None;
}


static PyObject *
py_remctl_error(PyObject *self, PyObject *args)
{
    PyObject *object = NULL;
    struct remctl *r;

    if (!PyArg_ParseTuple(args, "O", &object))
        return NULL;
    r = PyCObject_AsVoidPtr(object);
    return Py_BuildValue("s", remctl_error(r));
}


static PyObject *
py_remctl_commandv(PyObject *self, PyObject *args)
{
    PyObject *object = NULL;
    PyObject *list = NULL;
    struct remctl *r;
    struct iovec *iov;
    size_t count, i;
    char *string;
    Py_ssize_t length;
    PyObject *element;
    PyObject *result = NULL;

    if (!PyArg_ParseTuple(args, "OO", &object, &list))
        return NULL;
    r = PyCObject_AsVoidPtr(object);

    /*
     * Convert the Python list into an array of struct iovecs, each of which
     * pointing to the elements of the list.
     */
    count = PyList_Size(list);
    iov = malloc(count * sizeof(struct iovec));
    if (iov == NULL) {
        PyErr_NoMemory();
        return NULL;
    }
    for (i = 0; i < count; i++) {
        element = PyList_GetItem(list, i);
        if (element == NULL)
            goto end;
        if (PyString_AsStringAndSize(element, &string, &length) == -1)
            goto end;
        iov[i].iov_base = string;
        iov[i].iov_len = length;
    }

    if (remctl_commandv(r, iov, count)) {
        Py_INCREF(Py_True);
        result = Py_True;
    } else {
        Py_INCREF(Py_False);
        result = Py_False;
    }

end:
    if (iov != NULL)
        free(iov);
    return result;
}


static PyObject *
py_remctl_output(PyObject *self, PyObject *args)
{
    PyObject *object = NULL;
    struct remctl *r;
    struct remctl_output *output;
    PyObject *result;

    if (!PyArg_ParseTuple(args, "O", &object))
        return NULL;
    r = PyCObject_AsVoidPtr(object);
    output = remctl_output(r);
    if (output == NULL) {
        Py_INCREF(Py_False);
        return Py_False;
    }
    result = Py_BuildValue("is#iii", output->type, output->data,
                           output->length, output->stream, output->status,
                           output->error);
    return result;
}


static PyMethodDef methods[] = {
    { "remctl",          py_remctl,          METH_VARARGS },
    { "remctl_new",      py_remctl_new,      METH_VARARGS },
    { "remctl_open",     py_remctl_open,     METH_VARARGS },
    { "remctl_close",    py_remctl_close,    METH_VARARGS },
    { "remctl_error",    py_remctl_error,    METH_VARARGS },
    { "remctl_commandv", py_remctl_commandv, METH_VARARGS },
    { "remctl_output",   py_remctl_output,   METH_VARARGS },
    { NULL,              NULL,               0            },
};


PyMODINIT_FUNC
init_remctl(void)
{
    PyObject *module, *dict, *tmp;

    module = Py_InitModule("_remctl", methods);
    dict = PyModule_GetDict(module);

    tmp = PyInt_FromLong(REMCTL_OUT_OUTPUT);
    PyDict_SetItemString(dict, "REMCTL_OUT_OUTPUT", tmp);
    Py_DECREF(tmp);
    tmp = PyInt_FromLong(REMCTL_OUT_STATUS);
    PyDict_SetItemString(dict, "REMCTL_OUT_STATUS", tmp);
    Py_DECREF(tmp);
    tmp = PyInt_FromLong(REMCTL_OUT_ERROR);
    PyDict_SetItemString(dict, "REMCTL_OUT_ERROR", tmp);
    Py_DECREF(tmp);
    tmp = PyInt_FromLong(REMCTL_OUT_DONE);
    PyDict_SetItemString(dict, "REMCTL_OUT_DONE", tmp);
    Py_DECREF(tmp);

    tmp = PyString_FromString(VERSION);
    PyDict_SetItemString(dict, "VERSION", tmp);
    Py_DECREF(tmp);
}
