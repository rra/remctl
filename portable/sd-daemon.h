/*
 * Portability wrapper around systemd-daemon headers.
 *
 * Currently, only sd_listen_fds and sd_notify are guaranteed to be provided
 * by this interface.  This takes the approach of stubbing out these functions
 * if the libsystemd-daemon library is not available, rather than providing a
 * local implementation, on the grounds that anyone who wants systemd status
 * reporting should be able to build with the systemd libraries.
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

#ifndef PORTABLE_SD_DAEMON_H
#define PORTABLE_SD_DAEMON_H 1

#ifdef HAVE_SD_NOTIFY
# include <systemd/sd-daemon.h>
#else
# define SD_LISTEN_FDS_START 3
# define sd_listen_fds(u) 0
# define sd_notify(u, s)  0
#endif

#endif /* !PORTABLE_SD_DAEMON_H */
