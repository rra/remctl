/*
 * An interface between Ruby and libremctl. Implements both the simple and
 * complex forms of the API.
 *
 * Original implementation by Anthony M. Martinez <twopir@nmt.edu>
 *
 * Copyright 2010 Anthony M. Martinez <twopir@nmt.edu>
 *
 * Permission to use, copy, modify, and distribute this software and its
 * documentation for any purpose and without fee is hereby granted, provided
 * that the above copyright notice appear in all copies and that both that
 * copyright notice and this permission notice appear in supporting
 * documentation, and that the name of Anthony M. Martinez not be used in
 * advertising or publicity pertaining to distribution of the software without
 * specific, written prior permission. Anthony M. Martinez makes no
 * representations about the suitability of this software for any purpose. It
 * is provided "as is" without express or implied warranty.
 *
 * THIS SOFTWARE IS PROVIDED "AS IS" AND WITHOUT ANY EXPRESS OR IMPLIED
 * WARRANTIES, INCLUDING, WITHOUT LIMITATION, THE IMPLIED WARRANTIES OF
 * MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE.
 */

#include <ruby.h>

#include <errno.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/uio.h>

#include <remctl.h>

static VALUE cRemctl, cRemctlResult, eRemctlError, eRemctlNotOpen;
/* Since we can't have @ in our names here... */
static ID AAdefault_port, AAdefault_principal, Ahost, Aport, Aprincipal;

/* Stolen from the Python */
const struct {
    enum remctl_output_type type;
    const char *name;
} OUTPUT_TYPE[] = {
    { REMCTL_OUT_OUTPUT, "output" },
    { REMCTL_OUT_STATUS, "status" },
    { REMCTL_OUT_ERROR,  "error"  },
    { REMCTL_OUT_DONE,   "done"   },
    { 0,                 NULL     }
};

#define GET_REMCTL_OR_RAISE(obj, var) do { \
    Data_Get_Struct(obj, struct remctl, var); \
    if(var == NULL) \
        rb_raise(eRemctlNotOpen, "Connection is no longer open."); \
    } while(0)

/* Given a remctl_result pointer, return a Ruby Remctl::Result, unless the
 * remctl call had an error, in which case raise a Remctl::Error
 */
VALUE
rb_remctl_result_new(struct remctl_result *rr)
{
    VALUE result = rb_class_new_instance(0, NULL, cRemctlResult);
    if(rr->error)
        rb_raise(eRemctlError, "%s", rr->error);

    rb_iv_set(result, "@stderr", rb_str_new(rr->stderr_buf, rr->stderr_len));
    rb_iv_set(result, "@stdout", rb_str_new(rr->stdout_buf, rr->stdout_len));
    rb_iv_set(result, "@status", INT2FIX(rr->status));

    remctl_result_free(rr);
    return result;
}

/* call-seq:
 * Remctl.remctl(host, *args)   -> Remctl::Result
 *
 * Place a single Remctl call to the host, using the current default port and
 * principal.
 */
VALUE
rb_remctl_remctl(int argc, VALUE argv[], VALUE self)
{
    VALUE vhost, vport, vprinc, vargs, tmp;
    unsigned int port;
    char *host, *princ;
    const char **args;
    struct remctl_result *rc_res;
    int i, rc_argc;

    /* Take the port and princ from the class instead of demanding that the
     * user specify "nil, nil" so often.
     */
    rb_scan_args(argc, argv, "1*", &vhost, &vargs);

    host   = StringValuePtr(vhost);
    vport  = rb_cvar_get(cRemctl, AAdefault_port);
    vprinc = rb_cvar_get(cRemctl, AAdefault_principal);
    port   = NIL_P(vport)  ? 0    : FIX2UINT(vport);
    princ  = NIL_P(vprinc) ? NULL : StringValuePtr(vprinc);

    /* Convert the remaining arguments to their underlying pointers. */
    rc_argc = RARRAY_LEN(vargs);
    args = ALLOC_N(const char *, rc_argc+1);
    for(i = 0; i < rc_argc; i++) {
        tmp = rb_ary_entry(vargs, i);
        args[i] = StringValuePtr(tmp);
    }
    args[rc_argc] = NULL;

    rc_res = remctl(host, port, princ, args);
    if (rc_res == NULL)
        rb_raise(rb_eNoMemError, "remctl");

    return rb_remctl_result_new(rc_res);
}

VALUE
rb_remctl_result_initialize(VALUE self)
{
    rb_define_attr(cRemctlResult, "stderr", 1, 0);
    rb_define_attr(cRemctlResult, "stdout", 1, 0);
    rb_define_attr(cRemctlResult, "status", 1, 0);
    return Qtrue;
}

/* call-seq:
 * Remctl.default_port  -> 0
 *
 * Return the default port used for a Remctl simple connection or new instance
 * of a complex connection. A value of 0 indicates the default port.
 */
VALUE
rb_remctl_default_port_get(VALUE self)
{
    return rb_cvar_get(cRemctl, AAdefault_port);
}

/* call-seq:
 * Remctl.default_port = 0       -> 0
 * Remctl.default_port = 2617    -> 2617
 * Remctl.default_port = 65539   -> ArgumentError
 *
 * Change the default port used for a Remctl simple connection or new instance
 * of a complex connection. A value of 0 indicates the default port. Raises
 * +ArgumentError+ if the port number isn't sane.
 */
VALUE
rb_remctl_default_port_set(VALUE self, VALUE new)
{
    unsigned int port = FIX2UINT(new);
    if(port > 65535)
        rb_raise(rb_eArgError, "Port number %u out of range", port);
    else
        rb_cvar_set(cRemctl, AAdefault_port, new, 0);
    return rb_cvar_get(cRemctl, AAdefault_port);
}

/* call-seq:
 * Remctl.default_principal  -> nil
 *
 * Return the default principal used for a Remctl simple connection or new
 * instance of a complex connection.
 */
VALUE
rb_remctl_default_principal_get(VALUE self)
{
    return rb_cvar_get(cRemctl, AAdefault_principal);
}

/* call-seq:
 * Remctl.default_principal = "root@REALM"  -> "root@REALM"
 *
 * Set the principal used by default for a Remctl simple connection. A value of
 * nil requests the library default.
 */
VALUE
rb_remctl_default_principal_set(VALUE self, VALUE new)
{
    rb_cvar_set(cRemctl, AAdefault_principal, StringValue(new), 0);
    return rb_cvar_get(cRemctl, AAdefault_principal);
}

void
rb_remctl_destroy(void *rc)
{
    if(rc)
        remctl_close(rc);
}

VALUE
rb_remctl_alloc(VALUE klass)
{
    struct remctl *rem = NULL;
    return Data_Wrap_Struct(klass, NULL, rb_remctl_destroy, rem);
}

/* call-seq:
 * rc.close  -> nil
 *
 * Close a Remctl connection. Any further operations (besides reopen) on the
 * object will raise Remctl::NotOpen.
 */
VALUE
rb_remctl_close(VALUE self)
{
    struct remctl *rem;

    GET_REMCTL_OR_RAISE(self, rem);
    remctl_close(rem);
    DATA_PTR(self) = NULL;
    return Qnil;
}

/* Reopen a Remctl connection to the stored host, port, and principal.
 *
 * Raises Remctl::Error if the connection fails.
 */
VALUE
rb_remctl_reopen(VALUE self)
{
    struct remctl *rc;
    VALUE vhost, vport, vprinc;
    char *host, *princ;
    unsigned int port;

    Data_Get_Struct(self, struct remctl, rc);
    if(rc != NULL) {
        remctl_close(rc);
    }
    rc = remctl_new();

    /* This is kinda ugly :( */
    vhost  = rb_ivar_get(self, Ahost);
    vport  = rb_ivar_get(self, Aport);
    vprinc = rb_ivar_get(self, Aprincipal);

    host  = StringValuePtr(vhost);
    port  = NIL_P(vport)  ? 0    : FIX2UINT(vport);
    princ = NIL_P(vprinc) ? NULL : StringValuePtr(vprinc);

    if(!remctl_open(rc, host, port, princ)) {
        rb_raise(eRemctlError, "%s", rb_str_new2(remctl_error(rc)));
    }
    DATA_PTR(self) = rc;
    return self;
}

/* Call a remote command. Returns nil.
 *
 * Raises Remctl::Error in the event of failure, and Remctl::NotOpen if the
 * connection has been closed.
 */
VALUE
rb_remctl_command(int argc, VALUE argv[], VALUE self)
{
    struct remctl *rc;
    struct iovec *iov;
    int i;
    VALUE s;

    GET_REMCTL_OR_RAISE(self, rc);
    iov = ALLOC_N(struct iovec, argc);
    for(i = 0; i < argc; i++) {
        s = StringValue(argv[i]);
        iov[i].iov_base = RSTRING_PTR(s);
        iov[i].iov_len  = RSTRING_LEN(s);
    }

    if(!remctl_commandv(rc, iov, argc)) {
        rb_raise(eRemctlError, "%s", rb_str_new2(remctl_error(rc)));
    }

    return Qnil;
}

VALUE
rb_remctl_type_intern(enum remctl_output_type t)
{
    int i;
    for(i = 0; OUTPUT_TYPE[i].name != NULL; i++) {
        if(OUTPUT_TYPE[i].type == t) {
            return ID2SYM(rb_intern(OUTPUT_TYPE[i].name));
        }
    }
    rb_bug("Fell off the end while looking up remctl output type %d!\n", t);
}

/* call-seq:
 * rc.command -> [type, output, stream, status, error]
 *
 * Retrieve the output tokens from the remote command.
 *
 * Raises Remctl::Error in the event of failure, and Remctl::NotOpen if the
 * connection has been closed.
 */
VALUE
rb_remctl_output(VALUE self)
{
    struct remctl *rc;
    struct remctl_output *o;

    GET_REMCTL_OR_RAISE(self, rc);
    if((o = remctl_output(rc)) == NULL) {
        rb_raise(eRemctlError, "%s", rb_str_new2(remctl_error(rc)));
    }
    return rb_ary_new3(5, rb_remctl_type_intern(o->type),
                          rb_str_new(o->data, o->length),
                          INT2FIX(o->stream),
                          INT2FIX(o->status),
                          INT2FIX(o->error));
}

/* call-seq:
 * Remctl.new(host, port=Remctl.default_port, princ=Remctl.default_principal) -> #&lt;Remctl&gt;
 * Remctl.new(host, port, princ) {|rc| ...} -> nil
 *
 * Create and open a new Remctl complex instance to +host+. Raises
 * ArgumentError if the port number is out of range, and Remctl::Error if the
 * underlying library raises one. With a block, yield the instance, and ensure
 * the connection is closed at block exit.
 */
VALUE
rb_remctl_initialize(int argc, VALUE argv[], VALUE self)
{
    VALUE vhost, vport, vprinc, vdefport, vdefprinc;
    unsigned int port;
    char *host, *princ;

    rb_define_attr(cRemctl, "host", 1, 0);
    rb_define_attr(cRemctl, "port", 1, 0);
    rb_define_attr(cRemctl, "principal", 1, 0);

    vdefport  = rb_cvar_get(cRemctl, AAdefault_port);
    vdefprinc = rb_cvar_get(cRemctl, AAdefault_principal);

    rb_scan_args(argc, argv, "12", &vhost, &vport, &vprinc);

    if(NIL_P(vport))  vport  = vdefport;
    if(NIL_P(vprinc)) vprinc = vdefprinc;

    host  = StringValuePtr(vhost);
    port  = NIL_P(vport)  ? 0    : FIX2UINT(vport);
    princ = NIL_P(vprinc) ? NULL : StringValuePtr(vprinc);

    if(port > 65535) {
        rb_raise(rb_eArgError, "Port number %u out of range", port);
    }

    /* Hold these in instance variables for reopen */
    rb_ivar_set(self, Ahost, vhost);
    rb_ivar_set(self, Aport, vport);
    rb_ivar_set(self, Aprincipal, vprinc);
    rb_remctl_reopen(self);
    if(rb_block_given_p()) {
        rb_ensure(rb_yield, self, rb_remctl_close, self);
        return Qnil;
    }
    else {
        return self;
    }
}

/* Ruby interface to the Remctl library for remote Kerberized command
 * execution.
 */
void
Init_remctl()
{
    cRemctl = rb_define_class("Remctl", rb_cObject);
    rb_define_singleton_method(cRemctl, "remctl", rb_remctl_remctl, -1);

    AAdefault_port      = rb_intern("@@default_port");
    AAdefault_principal = rb_intern("@@default_principal");

    Ahost      = rb_intern("@host");
    Aport      = rb_intern("@port");
    Aprincipal = rb_intern("@principal");

    rb_cvar_set(cRemctl, AAdefault_port, UINT2NUM(0), 0);
    rb_cvar_set(cRemctl, AAdefault_principal, Qnil, 0);

    rb_define_singleton_method(cRemctl, "default_port",
            rb_remctl_default_port_get, 0);
    rb_define_singleton_method(cRemctl, "default_port=",
            rb_remctl_default_port_set, 1);
    rb_define_singleton_method(cRemctl, "default_principal",
            rb_remctl_default_principal_get, 0);
    rb_define_singleton_method(cRemctl, "default_principal=",
            rb_remctl_default_principal_set, 1);

    rb_define_alloc_func(cRemctl, rb_remctl_alloc);
    rb_define_method(cRemctl, "initialize", rb_remctl_initialize, -1);
    rb_define_method(cRemctl, "close", rb_remctl_close, 0);
    rb_define_method(cRemctl, "reopen", rb_remctl_reopen, 0);
    rb_define_method(cRemctl, "command", rb_remctl_command, -1);
    rb_define_method(cRemctl, "output", rb_remctl_output, 0);
    
    /* Document-class: Remctl::Result
     *
     * Returned from a simple Remctl.remctl call. Attributes:
     *
     * +stdout+:: String containing the returned standard output
     * +stderr+:: Same, but standard error.
     * +status+:: Fixnum of the command's exit status.
     */
    cRemctlResult = rb_define_class_under(cRemctl, "Result", rb_cObject);
    rb_define_method(cRemctlResult, "initialize", rb_remctl_result_initialize,
            0);

    /* Document-class: Remctl::Error
     *
     * Raised when Remctl has encountered either a protocol or network error.
     * The message comes from the library function +remctl_error+.
     */
    eRemctlError = rb_define_class_under(cRemctl, "Error", rb_eException);

    /* Document-class: Remctl::NotOpen
     *
     * Raised when using the complex interface and the connection has been
     * closed.
     */
    eRemctlNotOpen = rb_define_class_under(cRemctl, "NotOpen", rb_eException);
}
