/*
 * Portability wrapper around libevent headers.
 *
 * The canonical version of this file is maintained in the rra-c-util package,
 * which can be found at <http://www.eyrie.org/~eagle/software/rra-c-util/>.
 *
 * Written by Russ Allbery <eagle@eyrie.org>
 *
 * The authors hereby relinquish any claim to any copyright that they may have
 * in this work, whether granted under contract or by operation of law or
 * international treaty, and hereby commit to the public, at large, that they
 * shall not, at any time in the future, seek to enforce any copyright in this
 * work against any person or entity, or prevent any person or entity from
 * copying, publishing, distributing or creating derivative works of this
 * work.
 */

#ifndef PORTABLE_EVENT_H
#define PORTABLE_EVENT_H 1

#include <event2/buffer.h>
#include <event2/bufferevent.h>
#include <event2/event.h>

/* Introduced in 2.1.1-alpha.  Just skip it if we don't have it. */
#ifndef HAVE_LIBEVENT_GLOBAL_SHUTDOWN
# define libevent_global_shutdown() /* empty */
#endif

#endif /* !PORTABLE_SD_DAEMON_H */
