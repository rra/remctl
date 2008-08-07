#include <config.h>

#include <errno.h>
#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/types.h>
#include <sys/stat.h>

#include <client/remctl.h>

#ifndef IOV_MAX
# define IOV_MAX UIO_MAXIOV
#endif

#include "php.h"
#include "php_remctl.h"

static int			le_remctl_internal;

static zend_function_entry	remctl_functions[] = {
    ZEND_FE( remctl, NULL )
    ZEND_FE( remctl_new, NULL )
    ZEND_FE( remctl_open, NULL )
    ZEND_FE( remctl_close, NULL )
    ZEND_FE( remctl_command, NULL )
    ZEND_FE( remctl_output, NULL )
    ZEND_FE( remctl_error, NULL )
    { NULL, NULL, NULL }
};

zend_module_entry		remctl_module_entry = {
    STANDARD_MODULE_HEADER,
    PHP_REMCTL_EXTNAME,
    remctl_functions,
    PHP_MINIT( remctl ),
    NULL,
    NULL,
    NULL,
    NULL,
    PHP_REMCTL_VERSION,
    STANDARD_MODULE_PROPERTIES
};

#ifdef COMPILE_DL_REMCTL
ZEND_GET_MODULE( remctl )
#endif /* COMPILE_DL_REMCTL */

    static void
php_remctl_dtor( zend_rsrc_list_entry *rsrc TSRMLS_DC )
{
    struct remctl		*r = ( struct remctl * )rsrc->ptr;

    if ( r ) {
	remctl_close( r );
    }
}

PHP_MINIT_FUNCTION( remctl )
{
    le_remctl_internal = zend_register_list_destructors_ex( php_remctl_dtor,
				NULL, PHP_REMCTL_RES_NAME, module_number );

    return( SUCCESS );
}

ZEND_FUNCTION( remctl )
{
    struct remctl_result	*result = NULL;

    zval			*cmd_array, **data;
    zval			*remres_obj;
    HashTable			*hash;
    HashPosition		pos;

    char			*host;
    char			*principal;
    const char			**command = NULL;
    unsigned short		port;
    int				hlen, plen, count, i;
    int				success = 0;

    if ( zend_parse_parameters( ZEND_NUM_ARGS() TSRMLS_CC,
				"slsa", &host, &hlen, &port,
				&principal, &plen, &cmd_array ) == FAILURE ) {
	zend_error( E_WARNING, "%s: invalid parameters", __FUNCTION__ );
	RETURN_NULL();
    }

    if (( hlen == 0 ) || ( plen == 0 )) {
	zend_error( E_WARNING, "Host and principal must be valid strings\n" );
	RETURN_NULL();
    }

    hash = Z_ARRVAL_P( cmd_array );
    if (( count = zend_hash_num_elements( hash )) < 2 ) {
	zend_error( E_WARNING, "%s: array must contain at least two elements\n",
				__FUNCTION__ );
	RETURN_NULL();
    }

    if (( command = ( const char ** )emalloc(( count + 1 ) *
					sizeof( char * ))) == NULL ) {
	zend_error( E_WARNING, "%s: emalloc failed\n", __FUNCTION__ );
	RETURN_NULL();
    }

    for ( i = 0, zend_hash_internal_pointer_reset_ex( hash, &pos );
		zend_hash_get_current_data_ex( hash, ( void ** )&data,
						   &pos ) == SUCCESS;
		i++, zend_hash_move_forward_ex( hash, &pos )) {
	if ( Z_TYPE_PP( data ) != IS_STRING ) {
	    zend_error( E_WARNING, "%s: array contains non-string value\n",
				__FUNCTION__ );
	    goto remctl_cleanup;
	}

	if ( i >= count ) {
	    zend_error( E_WARNING, "%s: exceeded number of elements!\n",
				__FUNCTION__ );
	    goto remctl_cleanup;
	}

	command[ i ] = estrndup( Z_STRVAL_PP( data ), Z_STRLEN_PP( data ));
	if ( command[ i ] == NULL ) {
	    zend_error( E_WARNING, "%s: estrndup failed\n", __FUNCTION__ );
	    count = i;
	    goto remctl_cleanup;
	}
    }
    /* must NULL-terminate command argv for remctl */
    command[ count ] = NULL;

    if (( result = remctl( host, port, principal, command )) == NULL ) {
	zend_error( E_WARNING, "%s: remctl failed\n", __FUNCTION__ );
	goto remctl_cleanup;
    }

    /*
     * populate an object with remctl results.
     * return_value is defined for us by zend.
     */
    if ( object_init( return_value ) != SUCCESS ) {
	zend_error( E_WARNING, "%s: object_init failed\n", __FUNCTION__ );
	goto remctl_cleanup;
    }
    if ( result->error != NULL ) {
	add_property_string( return_value, "error", result->error, 1 );
    } else {
	add_property_string( return_value, "error", "", 1 );
    }
    add_property_stringl( return_value, "stdout",
			  result->stdout_buf, result->stdout_len, 1 );
    add_property_long( return_value, "stdout_len", result->stdout_len );
    add_property_stringl( return_value, "stderr",
			  result->stderr_buf, result->stderr_len, 1 );
    add_property_long( return_value, "stderr_len", result->stderr_len );
    add_property_long( return_value, "status", result->status );

    success = 1;

remctl_cleanup:
    if ( command != NULL ) {
	for ( i = 0; i < count; i++ ) {
	    efree(( char * )command[ i ] );
	}
	efree( command );
    }
    if ( result != NULL ) {
	remctl_result_free( result );
    }

    if ( !success ) {
	RETURN_NULL();
    }

    /*
     * these functions actually return void. return_value contains
     * the interesting bits. the RETURN_X macros simply change
     * what return_value contains.
     */
}

ZEND_FUNCTION( remctl_new )
{
    struct remctl		*r;

    if (( r = remctl_new()) == NULL ) {
	zend_error( E_WARNING, "%s: remctl_new failed", __FUNCTION__ );
	RETURN_NULL();
    }

    ZEND_REGISTER_RESOURCE( return_value, r, le_remctl_internal );
}

ZEND_FUNCTION( remctl_open )
{
    struct remctl		*r;
    zval			*zrem;
    char			*host, *princ;
    unsigned short		port;
    int				hlen, prlen;

    if ( zend_parse_parameters( ZEND_NUM_ARGS() TSRMLS_CC,
				"rsls", &zrem, &host, &hlen,
				&port, &princ, &prlen ) == FAILURE ) {
	zend_error( E_WARNING, "%s: invalid parameters", __FUNCTION__ );
	RETURN_FALSE;
    }

    ZEND_FETCH_RESOURCE( r, struct remctl *, &zrem, -1,
			 PHP_REMCTL_RES_NAME, le_remctl_internal );

    if ( !remctl_open( r, host, port, princ )) {
	RETURN_FALSE;
    }

    RETURN_TRUE;
}

ZEND_FUNCTION( remctl_command )
{
    struct remctl		*r;
    zval			*zrem, *cmd_array, **data;

    HashTable			*hash;
    HashPosition		pos;

    struct iovec		*cmd_vec = NULL;
    int				i, count;
    int				success = 0;

    if ( zend_parse_parameters( ZEND_NUM_ARGS() TSRMLS_CC,
				"ra", &zrem, &cmd_array ) == FAILURE ) {
	zend_error( E_WARNING, "%s: invalid parameters", __FUNCTION__ );
	RETURN_FALSE;
    }

    ZEND_FETCH_RESOURCE( r, struct remctl *, &zrem, -1,
			 PHP_REMCTL_RES_NAME, le_remctl_internal );

    hash = Z_ARRVAL_P( cmd_array );
    if (( count = zend_hash_num_elements( hash )) < 2 ) {
	zend_error( E_WARNING, "%s: array must contain at least two elements\n",
				__FUNCTION__ );
	RETURN_FALSE;
    }
    if ( count > IOV_MAX ) {
	zend_error( E_WARNING, "%s: array count exceeds IOV_MAX!",
				__FUNCTION__ );
	RETURN_FALSE;
    }

    if (( cmd_vec = ( struct iovec * )emalloc( count *
					sizeof( struct iovec ))) == NULL ) {
	zend_error( E_WARNING, "%s: emalloc failed\n", __FUNCTION__ );
	RETURN_FALSE;
    }

    for ( i = 0, zend_hash_internal_pointer_reset_ex( hash, &pos );
		zend_hash_get_current_data_ex( hash, ( void ** )&data,
						   &pos ) == SUCCESS;
		i++, zend_hash_move_forward_ex( hash, &pos )) {
	if ( Z_TYPE_PP( data ) != IS_STRING ) {
	    zend_error( E_WARNING, "%s: array contains non-string value\n",
				__FUNCTION__ );
	    goto remctl_command_cleanup;
	}

	if ( i >= count ) {
	    zend_error( E_WARNING, "%s: exceeded number of elements!\n",
				__FUNCTION__ );
	    goto remctl_command_cleanup;
	}

	cmd_vec[ i ].iov_base = estrndup( Z_STRVAL_PP( data ),
					  Z_STRLEN_PP( data ));
	if ( cmd_vec[ i ].iov_base == NULL ) {
	    zend_error( E_WARNING, "%s: estrndup failed\n", __FUNCTION__ );
	    count = i;
	    goto remctl_command_cleanup;
	}
	cmd_vec[ i ].iov_len = Z_STRLEN_PP( data );
    }

    if ( !remctl_commandv( r, cmd_vec, count )) {
	/* return false, let script get error from remctl_error */
	goto remctl_command_cleanup;
    }

    success = 1;

remctl_command_cleanup:
    if ( cmd_vec != NULL ) {
	for ( i = 0; i < count; i++ ) {
	    efree(( char * )cmd_vec[ i ].iov_base );
	}
	efree( cmd_vec );
    }

    if ( !success ) {
	RETURN_FALSE;
    }

    RETURN_TRUE;
}

ZEND_FUNCTION( remctl_output )
{
    struct remctl		*r;
    struct remctl_output	*output;
    zval			*zrem;

    int				success = 0;

    if ( zend_parse_parameters( ZEND_NUM_ARGS() TSRMLS_CC,
				"r", &zrem ) == FAILURE ) {
	zend_error( E_WARNING, "%s: invalid parameters", __FUNCTION__ );
	RETURN_NULL();
    }

    ZEND_FETCH_RESOURCE( r, struct remctl *, &zrem, -1,
			 PHP_REMCTL_RES_NAME, le_remctl_internal );

    /*
     * populate an object with remctl results.
     * return_value is defined for us by zend.
     */
    if ( object_init( return_value ) != SUCCESS ) {
	zend_error( E_WARNING, "%s: object_init failed\n", __FUNCTION__ );
	RETURN_NULL();
    }
    
    if (( output = remctl_output( r )) == NULL ) {
	zend_error( E_WARNING, "%s: error reading from server: %s",
				remctl_error( r ));
	RETURN_NULL();
    }

    add_property_bool( return_value, "done", 0 );
    add_property_long( return_value, "stdout_len", 0 );
    add_property_long( return_value, "stderr_len", 0 );

    switch( output->type ) {
    case REMCTL_OUT_OUTPUT:
	add_property_stringl( return_value, "type",
				"output", strlen( "output" ), 1 );
	if ( output->stream == 1 ) {
	    add_property_stringl( return_value, "stdout",
				output->data, output->length, 1 );
	    add_property_long( return_value, "stdout_len", output->length );
	} else if ( output->stream == 2 ) {
	    add_property_stringl( return_value, "stderr",
				output->data, output->length, 1 );
	    add_property_long( return_value, "stderr_len", output->length );
	} else {
	    zend_error( E_WARNING, "%s: unknown output stream %d",
				output->stream );
	    add_property_stringl( return_value, "stderr",
				output->data, output->length, 1 );
	    add_property_long( return_value, "stderr_len", output->length );
	}
	break;

    case REMCTL_OUT_ERROR:
	add_property_stringl( return_value, "type",
				"error", strlen( "error" ), 1 );
	add_property_stringl( return_value, "error",
				output->data, output->length, 1 );
	break;

    case REMCTL_OUT_STATUS:
	add_property_stringl( return_value, "type",
				"status", strlen( "status" ), 1 );
	add_property_long( return_value, "status", output->status );
	break;

    case REMCTL_OUT_DONE:
	add_property_stringl( return_value, "type",
				"done", strlen( "done" ), 1 );
	add_property_bool( return_value, "done", 1 );
	break;
    }

    /* return_value contains something like a remctl_output object */
}

ZEND_FUNCTION( remctl_error )
{
    struct remctl		*r;
    zval			*zrem;

    const char			*error;
    int				success = 0;

    if ( zend_parse_parameters( ZEND_NUM_ARGS() TSRMLS_CC,
				"r", &zrem ) == FAILURE ) {
	zend_error( E_WARNING, "%s: invalid parameters", __FUNCTION__ );
	RETURN_NULL();;
    }

    ZEND_FETCH_RESOURCE( r, struct remctl *, &zrem, -1,
			 PHP_REMCTL_RES_NAME, le_remctl_internal );

    error = remctl_error( r );

    RETURN_STRING(( char * )error, 1 );
}

ZEND_FUNCTION( remctl_close )
{
    struct remctl		*r;
    zval			*zrem;

    if ( zend_parse_parameters( ZEND_NUM_ARGS() TSRMLS_CC,
				"r", &zrem ) == FAILURE ) {
	zend_error( E_WARNING, "%s: invalid parameters", __FUNCTION__ );
	RETURN_FALSE;
    }

    ZEND_FETCH_RESOURCE( r, struct remctl *, &zrem, -1,
			 PHP_REMCTL_RES_NAME, le_remctl_internal );

    /* this delete invokes php_remctl_dtor, which calls remctl_close */
    zend_list_delete( Z_LVAL_P( zrem ));

    RETURN_TRUE;
}
