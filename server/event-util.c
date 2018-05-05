/*
 * Utility functions for libevent.
 *
 * Provides some utility functions used with libevent in both remctld and
 * remctl-shell.
 *
 * Written by Russ Allbery <eagle@eyrie.org>
 * Copyright 2016 Dropbox, Inc.
 * Copyright 2014
 *     The Board of Trustees of the Leland Stanford Junior University
 *
 * SPDX-License-Identifier: MIT
 */

#include <config.h>
#include <portable/event.h>
#include <portable/socket.h>

#include <server/internal.h>
#include <util/messages.h>


/*
 * The logging callback for libevent.  We hook this into our message system so
 * that libevent messages are handled the same way as our other internal
 * messages.  This function should be passed to event_set_log_callback at the
 * start of libevent initialization.
 */
void
server_event_log_callback(int severity, const char *message)
{
    switch (severity) {
    case EVENT_LOG_DEBUG:
        debug("%s", message);
        break;
    case EVENT_LOG_MSG:
        notice("%s", message);
        break;
    default:
        warn("%s", message);
        break;
    }
}


/*
 * The fatal callback for libevent.  Convert this to die, so that it's logged
 * the same as our other messages.  This function should be passed to
 * event_set_fatal_callback at the start of libevent initialization.
 */
void
server_event_fatal_callback(int err)
{
    die("fatal libevent error (%d)", err);
}
