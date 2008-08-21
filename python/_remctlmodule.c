/* $Id: _remctlmodule.c,v 1.8 2008/03/08 01:07:35 kula Exp $
 * _remctlmodule.c: a low level interface for Python to remctl
 * 
 * Written by Thomas L. Kula <kula@tproa.net>
 */


#include <Python.h>

#include <errno.h>
#include <stdlib.h>
#include <string.h>
#include <sys/uio.h>
#include <unistd.h>

#include <remctl.h>

#define VERSION "0.4"

    PyObject *
remctlsimple( PyObject *self, PyObject *args )
{
    struct remctl_result    *rr;
    char		    *host;
    short		    port;
    char		    *principal;
    const char		    **command = NULL;
    PyObject		    *listobj = NULL;
    PyObject		    *tmpobj = NULL;
    int			    ll;		    /* List length */
    int			    lc;		    /* List counter */
    PyObject		    *result = NULL;

    if ( PyArg_ParseTuple( args, "sHsO", &host, &port, &principal, &listobj )) {
	/* Now we have to turn listobj into an array of string pointers */

	ll = PyList_Size( listobj );
	/* malloc enough memory to hold ll + 1 char* */

	if (( command = malloc( (ll + 1) * sizeof(char * ))) == NULL ) {
	    return result;
	}
	

	for ( lc = 0; lc < ll; lc++ ) {
	    if (( tmpobj = PyList_GetItem( listobj, lc)) == NULL ) {
		/* tmpobj is a borrowed reference */

		goto end;
	    }

	    if (( command[ lc ] = PyString_AsString( tmpobj )) == NULL ) {
		goto end;
	    }
	}

	command[ ll ] = NULL;

	rr = remctl( host, port, principal, command );

	result = Py_BuildValue( "(ss#s#i)", rr->error,
					    rr->stdout_buf,
					    (int)rr->stdout_len,
					    rr->stderr_buf,
					    (int)rr->stderr_len,
					    rr->status );
	remctl_result_free( rr );

    }


end:
    free( command );
    return( result );
}

    static void
remclose( void *ptr ) 
{
    remctl_close( (struct remctl *) ptr );
}

    PyObject *
remctlnew( PyObject *self, PyObject *args )
{
    PyObject	    *result = NULL;
    struct remctl   *remst = NULL;

    if (( remst = remctl_new()) == NULL ) {
	Py_INCREF( PyExc_Exception );
	return( PyErr_SetFromErrno( PyExc_Exception ));
    }

    result = PyCObject_FromVoidPtr( (void *) remst, remclose );

    return( result );
}

    PyObject *
remctlopen( PyObject *self, PyObject *args )
{
    PyObject		*remobj = NULL;	/* The remctl_new python object */
    char		*host = NULL;
    unsigned short	port = 0;
    char		*principal = NULL;

    struct remctl	*remst = NULL;
    int			retvar = 0;
    PyObject		*result = NULL;

    if ( PyArg_ParseTuple( args, "OsHs", &remobj, &host, &port, &principal )) {
	/*
	 * pull the pointer out of remobj
	 */

	remst = (struct remctl *) PyCObject_AsVoidPtr( remobj );

	retvar = remctl_open( remst, host, port, principal );

	/*
	 * Now turn retvar into a python object
	 */

	result = Py_BuildValue( "i", retvar );
    }

    return( result );
}


    PyObject *
remctlclose( PyObject *self, PyObject *args )
{
    PyObject		*remobj = NULL; /* The remctl_new python object */

    struct remctl	*remst = NULL;
    PyObject		*result = NULL;

    if( PyArg_ParseTuple( args, "O", &remobj)) {
	remst = (struct remctl *) PyCObject_AsVoidPtr( remobj );
	remclose( remst );

	/*
	 * This always succeeds
	 */

	Py_INCREF(Py_None);
	result = Py_None;
    }

    return( result );
}


    PyObject *
remctlerror( PyObject *self, PyObject *args )
{
    PyObject		*remobj = NULL; /* The remctl_new python object */

    struct remctl	*remst = NULL;
    const char		*tmpstr = NULL;
    PyObject		*result = NULL;

    if ( PyArg_ParseTuple( args, "O", &remobj)) {
	remst = (struct remctl *) PyCObject_AsVoidPtr( remobj );
	tmpstr = remctl_error( remst );

	result = Py_BuildValue( "s", tmpstr );
    }

    return( result );
}

    PyObject *
remctlcommandv( PyObject *self, PyObject *args )
{
    PyObject		*remobj = NULL; /* The remctl_new python object */
    PyObject		*iovobj = NULL; /* A list of strings */

    struct remctl	*remst = NULL;
    struct iovec	*iov = NULL;
    size_t		count = 0;
    size_t		c;		/* counter */
    int			rr;		/* result from remctl_commandv */
    char		*pickles;
    PyObject		*liststring = NULL;
    PyObject		*result = NULL;

    if ( PyArg_ParseTuple( args, "OO", &remobj, &iovobj )) {
	remst = (struct remctl *) PyCObject_AsVoidPtr( remobj );

	count = PyList_Size( iovobj );
	/*
	 * We have the count of how long the list is. So 
	 * malloc enough space t hold count * sizeof(struct iovec)
	 */

	if ((iov = (struct iovec *) malloc( count * sizeof( struct iovec ))) == NULL ) {
	    /* Malloc failed, all bets off, aiee */
	    return( PyErr_NoMemory());
	}

	for ( c = 0; c < count; c = c + 1 ) {
	    if (( liststring = PyList_GetItem( iovobj, c )) == NULL ) {
		goto cleanup;
	    }
	    
	    if ( PyString_AsStringAndSize( liststring, &pickles, &iov[ c ].iov_len ) == -1 ) {
		goto cleanup;
	    }

	    iov[ c ].iov_base = pickles;
	}
    }

    rr = remctl_commandv( remst, iov, count );
    if ( rr ) {
	Py_INCREF( Py_True );
	result = Py_True;
    } else {
	Py_INCREF( Py_False );
	result = Py_False;
    }


cleanup: 
    if ( iov != NULL ) {
	free( iov );
    }

    return( result );
} 
	
    PyObject *
remctloutput( PyObject *self, PyObject *args )
{
    PyObject		    *remobj = NULL; /* The remctl_new python object */

    struct remctl	    *remst = NULL;
    struct remctl_output    *remout = NULL;
    PyObject		    *result = NULL;

    if ( PyArg_ParseTuple( args, "O", &remobj )) {
	remst = (struct remctl *) PyCObject_AsVoidPtr( remobj );

	if (( remout = remctl_output( remst )) == NULL ) {
	    Py_INCREF( Py_False );
	    return( Py_False );
	}

	result = Py_BuildValue( "is#iii", remout->type,
					  remout->data,
					  remout->length,
					  remout->stream,
					  remout->status,
					  remout->error );
    }

    return( result );
}



PyMethodDef methods[] = {
    { "remctl", remctlsimple, METH_VARARGS},
    { "remctlnew", remctlnew, METH_VARARGS},
    { "remctlopen", remctlopen, METH_VARARGS}, 
    { "remctlclose", remctlclose, METH_VARARGS},
    { "remctlerror", remctlerror, METH_VARARGS},
    { "remctlcommandv", remctlcommandv, METH_VARARGS},
    { "remctloutput", remctloutput, METH_VARARGS},
    {NULL, NULL},
};

    void
init_remctl()
{
    PyObject	    *module = NULL;
    PyObject	    *dict = NULL;
    PyObject	    *tmp = NULL;

    module = Py_InitModule( "_remctl", methods );
    dict = PyModule_GetDict( module );

    tmp = PyInt_FromLong( REMCTL_OUT_OUTPUT );
    PyDict_SetItemString( dict, "REMCTL_OUT_OUTPUT", tmp );
    Py_DECREF( tmp );
    tmp = PyInt_FromLong( REMCTL_OUT_STATUS );
    PyDict_SetItemString( dict, "REMCTL_OUT_STATUS", tmp );
    Py_DECREF( tmp );
    tmp = PyInt_FromLong( REMCTL_OUT_ERROR );
    PyDict_SetItemString( dict, "REMCTL_OUT_ERROR", tmp );
    Py_DECREF( tmp );
    tmp = PyInt_FromLong( REMCTL_OUT_DONE  );
    PyDict_SetItemString( dict, "REMCTL_OUT_DONE", tmp );
    Py_DECREF( tmp );

    tmp = PyString_FromString( "$Id: _remctlmodule.c,v 1.8 2008/03/08 01:07:35 kula Exp $" );
    PyDict_SetItemString( dict, "RCS", tmp );
    Py_DECREF( tmp );
    tmp = PyString_FromString( VERSION );
    PyDict_SetItemString( dict, "VERSION", tmp );
    Py_DECREF( tmp );

}
