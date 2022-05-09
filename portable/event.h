/*
 * Portability wrapper around libevent headers.
 *
 * The canonical version of this file is maintained in the rra-c-util package,
 * which can be found at <https://www.eyrie.org/~eagle/software/rra-c-util/>.
 *
 * Written by Russ Allbery <eagle@eyrie.org>
 * Copyright 2014
 *     The Board of Trustees of the Leland Stanford Junior University
 *
 * Copying and distribution of this file, with or without modification, are
 * permitted in any medium without royalty provided the copyright notice and
 * this notice are preserved.  This file is offered as-is, without any
 * warranty.
 *
 * SPDX-License-Identifier: FSFAP
 */

#ifndef PORTABLE_EVENT_H
#define PORTABLE_EVENT_H 1

#ifdef HAVE_EVENT2_EVENT_H
#    include <event2/buffer.h>
#    include <event2/bufferevent.h>
#    include <event2/event.h>
#else
#    include <event.h>
#endif

#include <portable/socket.h> /* socket_type */

/* Introduced in 2.0.1-alpha. */
#ifndef BEV_EVENT_EOF
#    define BEV_EVENT_EOF     EVBUFFER_EOF
#    define BEV_EVENT_ERROR   EVBUFFER_ERROR
#    define BEV_EVENT_READING EVBUFFER_READ
#    define BEV_EVENT_WRITING EVBUFFER_WRITE
#    define BEV_EVENT_TIMEOUT EVBUFFER_TIMEOUT
#endif

/* Introduced in 2.0.19-stable. */
#ifndef EVENT_LOG_DEBUG
#    define EVENT_LOG_DEBUG _EVENT_LOG_DEBUG
#    define EVENT_LOG_MSG   _EVENT_LOG_MSG
#    define EVENT_LOG_WARN  _EVENT_LOG_WARN
#    define EVENT_LOG_ERR   _EVENT_LOG_ERR
#endif

BEGIN_DECLS

/* Default to a hidden visibility for all portability functions. */
#pragma GCC visibility push(hidden)

/* Introduced in 2.0.2-alpha. */
#ifndef HAVE_BUFFEREVENT_DATA_CB
typedef evbuffercb bufferevent_data_cb;
#endif
#ifndef HAVE_BUFFEREVENT_EVENT_CB
typedef everrorcb bufferevent_event_cb;
#endif

/* Introduced in 2.0.1-alpha. */
#ifndef HAVE_EVUTIL_SOCKET_T
typedef socket_type evutil_socket_t;
#endif

/* Introduced in 2.0.1-alpha. */
#ifndef HAVE_EVENT_CALLBACK_FN
typedef void (*event_callback_fn)(evutil_socket_t, short, void *);
#endif

/* Introduced in 2.0.1-alpha. */
#ifndef HAVE_BUFFEREVENT_GET_INPUT
#    define bufferevent_get_input(bev) EVBUFFER_INPUT(bev)
#endif

/* Introduced in 2.0.1-alpha. */
#ifndef HAVE_BUFFEREVENT_READ_BUFFER
int bufferevent_read_buffer(struct bufferevent *, struct evbuffer *);
#endif

/*
 * Introduced in 2.0.1-alpha.  Note that options are silently ignored with
 * older versions and therefore cannot be relied on in code that has to be
 * portable.
 */
#ifndef HAVE_BUFFEREVENT_SOCKET_NEW
struct bufferevent *bufferevent_socket_new(struct event_base *,
                                           evutil_socket_t, int);
#endif

/* Introduced in 2.1.1-alpha.  Just skip it if we don't have it. */
#ifndef HAVE_LIBEVENT_GLOBAL_SHUTDOWN
#    define libevent_global_shutdown() /* empty */
#endif

/*
 * evbuffer_drain returns 0 or -1 with 2.x, but 1.4.13-stable declares this as
 * a void function.  Convert the older version to one that always returns
 * success.  This is hard to probe in Autoconf, so assume this change happened
 * in 2.0.1-alpha when evbuffers were rewritten to not expose internals and to
 * support more backing types.
 */
#if !defined(LIBEVENT_VERSION_NUMBER) || LIBEVENT_VERSION_NUMBER < 0x02000100
#    define evbuffer_drain(buf, len) evbuffer_drain_fixed((buf), (len))
#endif

/* Introduced in 2.0.1-alpha. */
#ifndef HAVE_EVBUFFER_GET_LENGTH
#    define evbuffer_get_length(buf) EVBUFFER_LENGTH(buf)
#endif

/* Introduced in 2.0.1-alpha. */
#ifndef HAVE_EVENT_FREE
#    define event_free(event) free(event)
#endif

/* Introduced in 2.0.1-alpha. */
#ifndef HAVE_EVENT_NEW
struct event *event_new(struct event_base *, evutil_socket_t, short,
                        event_callback_fn, void *);
#endif

/*
 * Introduced in 2.0.3-alpha.  Prior to this version, we just don't get to
 * have fatal callbacks.
 */
#ifndef HAVE_EVENT_SET_FATAL_CALLBACK
#    define event_set_fatal_callback(cb) /* empty */
#endif

/* Introduced in 2.0.1-alpha. */
#ifndef evsignal_new
#    define evsignal_new(base, signum, callback, arg) \
        event_new((base), (signum), EV_SIGNAL | EV_PERSIST, (callback), (arg))
#endif

/* Undo default visibility change. */
#pragma GCC visibility pop

END_DECLS

#endif /* !PORTABLE_EVENT_H */
