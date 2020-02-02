/*
 * Supporting functions for the Windows socket API.
 *
 * Provides supporting functions and wrappers that help with portability to
 * the Windows socket API.
 *
 * The canonical version of this file is maintained in the rra-c-util package,
 * which can be found at <https://www.eyrie.org/~eagle/software/rra-c-util/>.
 *
 * Written by Russ Allbery <eagle@eyrie.org>
 * Copyright 2008, 2011
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
#include <portable/socket.h>
#include <portable/system.h>


/*
 * Initializes the Windows socket library.  The returned parameter provides
 * information about the socket library, none of which we care about.  Return
 * true on success and false on failure.
 */
int
socket_init(void)
{
    WSADATA data;

    if (WSAStartup(MAKEWORD(2, 2), &data))
        return 0;
    return 1;
}


/*
 * On Windows, strerror cannot be used for socket errors (or any other errors
 * over sys_nerr).  Try to use FormatMessage with a local static variable
 * instead.
 */
const char *socket_strerror(err)
{
    const char *message = NULL;

    if (err >= sys_nerr) {
        char *p;
        DWORD f = FORMAT_MESSAGE_ALLOCATE_BUFFER | FORMAT_MESSAGE_FROM_SYSTEM
                  | FORMAT_MESSAGE_IGNORE_INSERTS;
        static char *buffer = NULL;

        if (buffer != NULL)
            LocalFree(buffer);
        if (FormatMessage(f, NULL, err, 0, (LPTSTR) &buffer, 0, NULL) != 0) {
            p = strchr(buffer, '\r');
            if (p != NULL)
                *p = '\0';
        }
        message = buffer;
    }
    if (message == NULL)
        message = strerror(err);
    return message;
}
