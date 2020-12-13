/*
 * remctl PECL extension for PHP 7 and later
 *
 * Provides bindings for PHP very similar to the libremctl library for C or
 * the Net::Remctl bindings for Perl.
 *
 * Originally written by Andrew Mortensen <admorten@umich.edu>
 * Copyright 2016, 2018, 2020 Russ Allbery <eagle@eyrie.org>
 * Copyright 2008 Andrew Mortensen <admorten@umich.edu>
 * Copyright 2008, 2011-2012, 2014
 *     The Board of Trustees of the Leland Stanford Junior University
 *
 * SPDX-License-Identifier: MIT
 */

#include <config.h>

#include <errno.h>
#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <time.h>

#include <client/remctl.h>

#include <php.h>
#include <php_remctl.h>

static int le_remctl_internal;

/* clang-format off */
ZEND_BEGIN_ARG_INFO_EX(arginfo_remctl, 0, 0, 4)
    ZEND_ARG_INFO(0, "host")
    ZEND_ARG_INFO(0, "port")
    ZEND_ARG_INFO(0, "principal")
    ZEND_ARG_INFO(0, "command")
ZEND_END_ARG_INFO()

ZEND_BEGIN_ARG_INFO_EX(arginfo_remctl_new, 0, 0, 0)
ZEND_END_ARG_INFO()

ZEND_BEGIN_ARG_INFO_EX(arginfo_remctl_set_ccache, 0, 0, 2)
    ZEND_ARG_INFO(0, "remctl")
    ZEND_ARG_INFO(0, "path")
ZEND_END_ARG_INFO()

ZEND_BEGIN_ARG_INFO_EX(arginfo_remctl_set_source_ip, 0, 0, 2)
    ZEND_ARG_INFO(0, "remctl")
    ZEND_ARG_INFO(0, "source_ip")
ZEND_END_ARG_INFO()

ZEND_BEGIN_ARG_INFO_EX(arginfo_remctl_set_timeout, 0, 0, 2)
    ZEND_ARG_INFO(0, "remctl")
    ZEND_ARG_INFO(0, "timeout")
ZEND_END_ARG_INFO()

ZEND_BEGIN_ARG_INFO_EX(arginfo_remctl_open, 0, 0, 2)
    ZEND_ARG_INFO(0, "remctl")
    ZEND_ARG_INFO(0, "host")
    ZEND_ARG_INFO(0, "port")
    ZEND_ARG_INFO(0, "principal")
ZEND_END_ARG_INFO()

ZEND_BEGIN_ARG_INFO_EX(arginfo_remctl_close, 0, 0, 2)
    ZEND_ARG_INFO(0, "remctl")
ZEND_END_ARG_INFO()

ZEND_BEGIN_ARG_INFO_EX(arginfo_remctl_command, 0, 0, 2)
    ZEND_ARG_INFO(0, "remctl")
    ZEND_ARG_INFO(0, "command")
ZEND_END_ARG_INFO()

ZEND_BEGIN_ARG_INFO_EX(arginfo_remctl_output, 0, 0, 2)
    ZEND_ARG_INFO(0, "remctl")
ZEND_END_ARG_INFO()

ZEND_BEGIN_ARG_INFO_EX(arginfo_remctl_noop, 0, 0, 2)
    ZEND_ARG_INFO(0, "remctl")
ZEND_END_ARG_INFO()

ZEND_BEGIN_ARG_INFO_EX(arginfo_remctl_error, 0, 0, 2)
    ZEND_ARG_INFO(0, "remctl")
ZEND_END_ARG_INFO()

static zend_function_entry remctl_functions[] = {
    ZEND_FE(remctl,               arginfo_remctl)
    ZEND_FE(remctl_new,           arginfo_remctl_new)
    ZEND_FE(remctl_set_ccache,    arginfo_remctl_set_ccache)
    ZEND_FE(remctl_set_source_ip, arginfo_remctl_set_source_ip)
    ZEND_FE(remctl_set_timeout,   arginfo_remctl_set_timeout)
    ZEND_FE(remctl_open,          arginfo_remctl_open)
    ZEND_FE(remctl_close,         arginfo_remctl_close)
    ZEND_FE(remctl_command,       arginfo_remctl_command)
    ZEND_FE(remctl_output,        arginfo_remctl_output)
    ZEND_FE(remctl_noop,          arginfo_remctl_noop)
    ZEND_FE(remctl_error,         arginfo_remctl_error)
    {NULL, NULL, NULL, 0, 0}
};

zend_module_entry remctl_module_entry = {
    STANDARD_MODULE_HEADER,
    PHP_REMCTL_EXTNAME,
    remctl_functions,
    PHP_MINIT(remctl),
    NULL,
    NULL,
    NULL,
    NULL,
    PHP_REMCTL_VERSION,
    STANDARD_MODULE_PROPERTIES
};
/* clang-format on */

#ifdef COMPILE_DL_REMCTL
ZEND_GET_MODULE(remctl)
#endif


/*
 * Destructor for a remctl object.  Close the underlying connection.
 */
static void
php_remctl_dtor(zend_resource *rsrc)
{
    struct remctl *r = (struct remctl *) rsrc->ptr;

    if (r != NULL)
        remctl_close(r);
}


/*
 * Initialize the module and register the destructor.  Stores the resource
 * number of the module in le_remctl_internal.
 */
PHP_MINIT_FUNCTION(remctl)
{
    le_remctl_internal = zend_register_list_destructors_ex(
        php_remctl_dtor, NULL, PHP_REMCTL_RES_NAME, module_number);
    return SUCCESS;
}


/*
 * The simplified interface.  Make a call and return the results as an
 * object.
 */
ZEND_FUNCTION(remctl)
{
    zval *cmd_array;
    zval *data = NULL;
    HashTable *hash;
    char *host, *principal = NULL;
    const char **command = NULL;
    long port;
    size_t hlen, plen;
    int count, i, status;
    int success = 0;
    struct remctl_result *result = NULL;

    /*
     * Read the arguments (host, port, principal, and command) and check their
     * validity.  Host and command are required, so all arguments must be
     * provided, but an empty string can be passed in as the principal.
     */
    status = zend_parse_parameters(ZEND_NUM_ARGS(), "slsa", &host, &hlen,
                                   &port, &principal, &plen, &cmd_array);
    if (status == FAILURE) {
        zend_error(E_WARNING, "remctl: invalid parameters\n");
        RETURN_NULL();
    }
    if (hlen == 0) {
        zend_error(E_WARNING, "remctl: host must be a valid string\n");
        RETURN_NULL();
    }
    if (plen == 0)
        principal = NULL;
    hash = Z_ARRVAL_P(cmd_array);
    count = zend_hash_num_elements(hash);
    if (count < 1) {
        zend_error(E_WARNING, "remctl: command must not be empty\n");
        RETURN_NULL();
    }

    /*
     * Convert the command array into the char ** needed by libremctl.  This
     * is less than ideal because we make another copy of all of the
     * arguments.  There should be some way to do this without making a copy.
     */
    command = ecalloc(count + 1, sizeof(char *));
    if (command == NULL) {
        zend_error(E_WARNING, "remctl: ecalloc failed\n");
        RETURN_NULL();
    }
    i = 0;
    ZEND_HASH_FOREACH_VAL(hash, data)
    {
        if (Z_TYPE_P(data) != IS_STRING) {
            zend_error(E_WARNING, "remctl: command contains non-string\n");
            goto cleanup;
        }
        if (i >= count) {
            zend_error(E_WARNING, "remctl: internal error: incorrect count\n");
            goto cleanup;
        }
        command[i] = estrndup(Z_STRVAL_P(data), Z_STRLEN_P(data));
        if (command[i] == NULL) {
            zend_error(E_WARNING, "remctl: estrndup failed\n");
            count = i;
            goto cleanup;
        }
        i++;
    }
    ZEND_HASH_FOREACH_END();
    command[count] = NULL;

    /* Run the actual remctl call. */
    result = remctl(host, port, principal, command);
    if (result == NULL) {
        zend_error(E_WARNING, "remctl: %s\n", strerror(errno));
        goto cleanup;
    }

    /*
     * Convert the remctl result to an object.  return_value is defined for us
     * by Zend.
     */
    object_init(return_value);
    if (result->error == NULL)
        add_property_string(return_value, "error", "");
    else
        add_property_string(return_value, "error", result->error);
    add_property_stringl(return_value, "stdout", result->stdout_buf,
                         result->stdout_len);
    add_property_long(return_value, "stdout_len", result->stdout_len);
    add_property_stringl(return_value, "stderr", result->stderr_buf,
                         result->stderr_len);
    add_property_long(return_value, "stderr_len", result->stderr_len);
    add_property_long(return_value, "status", result->status);
    success = 1;

cleanup:
    if (command != NULL) {
        for (i = 0; i < count; i++)
            efree((char *) command[i]);
        efree(command);
    }
    if (result != NULL)
        remctl_result_free(result);
    if (!success)
        RETURN_NULL();
}


/*
 * Now the full interface.  First, the constructor.
 */
ZEND_FUNCTION(remctl_new)
{
    struct remctl *r;

    r = remctl_new();
    if (r == NULL) {
        zend_error(E_WARNING, "remctl_new: %s", strerror(errno));
        RETURN_NULL();
    }
    RETURN_RES(zend_register_resource(r, le_remctl_internal));
}


/*
 * Set the credential cache for subsequent connections with remctl_open.
 */
ZEND_FUNCTION(remctl_set_ccache)
{
    struct remctl *r;
    zval *zrem;
    char *ccache;
    size_t clen;
    int status;

    status =
        zend_parse_parameters(ZEND_NUM_ARGS(), "rs", &zrem, &ccache, &clen);
    if (status == FAILURE) {
        zend_error(E_WARNING, "remctl_set_ccache: invalid parameters\n");
        RETURN_FALSE;
    }
    r = zend_fetch_resource(Z_RES_P(zrem), PHP_REMCTL_RES_NAME,
                            le_remctl_internal);
    if (!remctl_set_ccache(r, ccache))
        RETURN_FALSE;
    RETURN_TRUE;
}


/*
 * Set the source IP for subsequent connections with remctl_open.
 */
ZEND_FUNCTION(remctl_set_source_ip)
{
    struct remctl *r;
    zval *zrem;
    char *source;
    size_t slen;
    int status;

    status =
        zend_parse_parameters(ZEND_NUM_ARGS(), "rs", &zrem, &source, &slen);
    if (status == FAILURE) {
        zend_error(E_WARNING, "remctl_set_source_ip: invalid parameters\n");
        RETURN_FALSE;
    }
    r = zend_fetch_resource(Z_RES_P(zrem), PHP_REMCTL_RES_NAME,
                            le_remctl_internal);
    if (!remctl_set_source_ip(r, source))
        RETURN_FALSE;
    RETURN_TRUE;
}


/*
 * Set the timeout for subsequent operations.
 */
ZEND_FUNCTION(remctl_set_timeout)
{
    struct remctl *r;
    zval *zrem;
    zend_long timeout;
    int status;

    status = zend_parse_parameters(ZEND_NUM_ARGS(), "rl", &zrem, &timeout);
    if (status == FAILURE) {
        zend_error(E_WARNING, "remctl_set_timeout: invalid parameters\n");
        RETURN_FALSE;
    }
    r = zend_fetch_resource(Z_RES_P(zrem), PHP_REMCTL_RES_NAME,
                            le_remctl_internal);
    if (!remctl_set_timeout(r, timeout))
        RETURN_FALSE;
    RETURN_TRUE;
}


/*
 * Open a connection to the remote host.  Only the host parameter is required;
 * the rest are optional.  PHP may require something be passed in for
 * principal, but the empty string is taken to mean "use the library default."
 */
ZEND_FUNCTION(remctl_open)
{
    struct remctl *r;
    zval *zrem;
    char *host;
    char *principal = NULL;
    zend_long port = 0;
    size_t hlen, plen;
    int status;

    /* Parse and verify arguments. */
    status = zend_parse_parameters(ZEND_NUM_ARGS(), "rs|ls", &zrem, &host,
                                   &hlen, &port, &principal, &plen);
    if (status == FAILURE) {
        zend_error(E_WARNING, "remctl_open: invalid parameters\n");
        RETURN_FALSE;
    }
    if (plen == 0)
        principal = NULL;
    r = zend_fetch_resource(Z_RES_P(zrem), PHP_REMCTL_RES_NAME,
                            le_remctl_internal);

    /* Now we have all the arguments and can do the real work. */
    if (!remctl_open(r, host, port, principal))
        RETURN_FALSE;
    RETURN_TRUE;
}


/*
 * Send a command to the remote server.
 */
ZEND_FUNCTION(remctl_command)
{
    struct remctl *r;
    zval *zrem, *cmd_array;
    zval *data = NULL;
    HashTable *hash;
    struct iovec *cmd_vec = NULL;
    int i, count, status;
    int success = 0;

    /* Parse and verify arguments. */
    status = zend_parse_parameters(ZEND_NUM_ARGS(), "ra", &zrem, &cmd_array);
    if (status == FAILURE) {
        zend_error(E_WARNING, "remctl_command: invalid parameters\n");
        RETURN_FALSE;
    }
    r = zend_fetch_resource(Z_RES_P(zrem), PHP_REMCTL_RES_NAME,
                            le_remctl_internal);
    hash = Z_ARRVAL_P(cmd_array);
    count = zend_hash_num_elements(hash);
    if (count < 1) {
        zend_error(E_WARNING, "remctl_command: command must not be empty\n");
        RETURN_NULL();
    }

    /*
     * Transform the PHP array into an array of struct iovec.  This is less
     * than ideal because it makes another copy of all of the data.  There
     * should be some way to do this without copying.
     */
    cmd_vec = ecalloc(count, sizeof(struct iovec));
    if (cmd_vec == NULL) {
        zend_error(E_WARNING, "remctl_command: ecalloc failed\n");
        RETURN_FALSE;
    }
    i = 0;
    ZEND_HASH_FOREACH_VAL(hash, data)
    {
        if (Z_TYPE_P(data) != IS_STRING) {
            zend_error(E_WARNING,
                       "remctl_command: command contains non-string\n");
            goto cleanup;
        }
        if (i >= count) {
            zend_error(E_WARNING,
                       "remctl_command: internal error: incorrect count\n");
            goto cleanup;
        }
        cmd_vec[i].iov_base = emalloc(Z_STRLEN_P(data) + 1);
        if (cmd_vec[i].iov_base == NULL) {
            zend_error(E_WARNING, "remctl_command: emalloc failed\n");
            count = i;
            goto cleanup;
        }
        cmd_vec[i].iov_len = Z_STRLEN_P(data);
        memcpy(cmd_vec[i].iov_base, Z_STRVAL_P(data), cmd_vec[i].iov_len);
        i++;
    }
    ZEND_HASH_FOREACH_END();

    /* Finally, we can do the work. */
    if (!remctl_commandv(r, cmd_vec, count))
        goto cleanup;
    success = 1;

cleanup:
    if (cmd_vec != NULL) {
        for (i = 0; i < count; i++)
            efree(cmd_vec[i].iov_base);
        efree(cmd_vec);
    }
    if (!success)
        RETURN_FALSE;
    RETURN_TRUE;
}


/*
 * Get an output token from the server and return it as an object.
 */
ZEND_FUNCTION(remctl_output)
{
    struct remctl *r;
    struct remctl_output *output;
    zval *zrem;
    int status;

    /* Parse and verify arguments. */
    status = zend_parse_parameters(ZEND_NUM_ARGS(), "r", &zrem);
    if (status == FAILURE) {
        zend_error(E_WARNING, "remctl_output: invalid parameters\n");
        RETURN_NULL();
    }
    r = zend_fetch_resource(Z_RES_P(zrem), PHP_REMCTL_RES_NAME,
                            le_remctl_internal);

    /* Get the output token. */
    output = remctl_output(r);
    if (output == NULL)
        RETURN_NULL();

    /*
     * Populate an object with the output results.  return_value is defined
     * for us by Zend.
     */
    object_init(return_value);
    switch (output->type) {
    case REMCTL_OUT_OUTPUT:
        add_property_string(return_value, "type", "output");
        add_property_stringl(return_value, "data", output->data,
                             output->length);
        add_property_long(return_value, "stream", output->stream);
        break;
    case REMCTL_OUT_ERROR:
        add_property_string(return_value, "type", "error");
        add_property_stringl(return_value, "data", output->data,
                             output->length);
        add_property_long(return_value, "error", output->error);
        break;
    case REMCTL_OUT_STATUS:
        add_property_string(return_value, "type", "status");
        add_property_long(return_value, "status", output->status);
        break;
    case REMCTL_OUT_DONE:
        add_property_string(return_value, "type", "done");
        break;
    }
}


/*
 * Sends a NOOP message to the server and reads the reply.
 */
ZEND_FUNCTION(remctl_noop)
{
    struct remctl *r;
    zval *zrem;
    int status;

    status = zend_parse_parameters(ZEND_NUM_ARGS(), "r", &zrem);
    if (status == FAILURE) {
        zend_error(E_WARNING, "remctl_noop: invalid parameters\n");
        RETURN_FALSE;
    }
    r = zend_fetch_resource(Z_RES_P(zrem), PHP_REMCTL_RES_NAME,
                            le_remctl_internal);
    if (!remctl_noop(r))
        RETURN_FALSE;
    RETURN_TRUE;
}


/*
 * Returns the error message from a previously failed remctl call.
 */
ZEND_FUNCTION(remctl_error)
{
    struct remctl *r;
    zval *zrem;
    const char *error;
    int status;

    /* Parse and verify arguments. */
    status = zend_parse_parameters(ZEND_NUM_ARGS(), "r", &zrem);
    if (status == FAILURE) {
        zend_error(E_WARNING, "remctl_error: invalid parameters\n");
        RETURN_NULL();
    }
    r = zend_fetch_resource(Z_RES_P(zrem), PHP_REMCTL_RES_NAME,
                            le_remctl_internal);

    /* Do the work. */
    error = remctl_error(r);
    RETURN_STRING((char *) error);
}


/*
 * Close the connection.  This isn't strictly necessary since the destructor
 * will close the connection for us, but it's part of the interface.
 */
ZEND_FUNCTION(remctl_close)
{
    zval *zrem;
    int status;

    /* Parse and verify arguments. */
    status = zend_parse_parameters(ZEND_NUM_ARGS(), "r", &zrem);
    if (status == FAILURE) {
        zend_error(E_WARNING, "remctl_error: invalid parameters\n");
        RETURN_NULL();
    }

    /* This delete invokes php_remctl_dtor, which calls remctl_close. */
    zend_list_delete(Z_RES_P(zrem));
    RETURN_TRUE;
}
