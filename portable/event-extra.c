/*
 * Portability glue functions for libevent
 *
 * This file provides definitions of the interfaces that portable/event.h
 * ensures exist if the function wasn't available in the local libevent
 * library.  Everything in this file will be protected by #ifndef.  If the
 * native libevent library is fully capable, this file will be skipped.
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

#include <config.h>
#include <portable/event.h>
#include <portable/macros.h>
#include <portable/system.h>

/* Used for unused parameters to silence gcc warnings. */
#define UNUSED __attribute__((__unused__))


#ifndef HAVE_BUFFEREVENT_READ_BUFFER
/*
 * Read the data out of a bufferevent into a buffer.  Older versions of
 * libevent only have bufferevent_read, so we have to pull out the buffer and
 * then copy the data.
 */
int
bufferevent_read_buffer(struct bufferevent *bufev, struct evbuffer *buf)
{
    struct evbuffer *source;

    source = bufferevent_get_input(bufev);
    return evbuffer_add_buffer(buf, source);
}
#endif /* !HAVE_BUFFEREVENT_READ_BUFFER */


#ifndef HAVE_BUFFEREVENT_SOCKET_NEW
/*
 * Create a new bufferevent for a socket and register it with the provided
 * base.  Note that options is always ignored, so programs using this backward
 * compatibility layer cannot rely on it.
 */
struct bufferevent *
bufferevent_socket_new(struct event_base *base, evutil_socket_t fd,
                       int options UNUSED)
{
    struct bufferevent *bev;

    bev = bufferevent_new(fd, NULL, NULL, NULL, NULL);
    if (bufferevent_base_set(base, bev) < 0) {
        bufferevent_free(bev);
        return NULL;
    }
    return bev;
}
#endif /* !HAVE_BUFFEREVENT_SOCKET_NEW */


#if !defined(LIBEVENT_VERSION_NUMBER) || LIBEVENT_VERSION_NUMBER < 0x02000100
#    undef evbuffer_drain
/*
 * A fixed version of evbuffer_drain that returns a status code.  Old versions
 * of libevent returned void.
 */
int
evbuffer_drain_fixed(struct evbuffer *ev, size_t length)
{
    fprintf(stderr, "internal draining %lu\n", (unsigned long) length);
    evbuffer_drain(ev, length);
    return 0;
}
#endif /* evbuffer_drain without return code */


#ifndef HAVE_EVENT_NEW
/*
 * Allocate a new event struct and initialize it.  This uses the form that
 * explicitly sets an event base.  If we can't set an event base, return NULL,
 * since that's the only error reporting mechanism we have.
 */
struct event *
event_new(struct event_base *base, evutil_socket_t fd, short what,
          event_callback_fn cb, void *arg)
{
    struct event *ev;

    ev = calloc(1, sizeof(struct event));
    if (ev == NULL)
        return NULL;
    event_set(ev, fd, what, cb, arg);
    if (event_base_set(base, ev) < 0) {
        free(ev);
        return NULL;
    }
    return ev;
}
#endif /* !HAVE_EVENT_NEW */
