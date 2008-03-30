/* $Id$
 *
 * Supporting functions for the Windows socket API.
 *
 * Written by Russ Allbery <rra@stanford.edu>
 * This work is hereby placed in the public domain by its author.
 *
 * Provides supporting functions and wrappers that help with portability to
 * the Windows socket API.
 */

#include <config.h>
#include <system.h>
#include <portable/socket.h>

/*
 * Initializes the Windows socket library.  The returned parameter provides
 * information about the socket library, none of which we care about.  Return
 * true on success and false on failure.
 */
int
socket_init(void)
{
    WSADATA *data;

    if (WSAStartup(MAKEWORD(2,2), &data))
        return 0;
    return 1;
}
