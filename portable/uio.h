/* $Id$
 *
 * Portability wrapper around <sys/uio.h>.
 *
 * Provides a definition of the iovec struct for platforms that don't have it
 * (primarily Windows).  Currently, the corresponding readv and writev
 * functions are not provided or prototyped here.
 *
 * Written by Russ Allbery <rra@stanford.edu>
 * This work is hereby placed in the public domain by its author.
 */

#ifndef PORTABLE_UIO_H
#define PORTABLE_UIO_H 1

#include <sys/types.h>

#ifdef HAVE_SYS_UIO_H
# include <sys/uio.h>
#else
struct iovec {
    void *iov_base;
    size_t iov_len;
};
#endif

#endif /* !PORTABLE_UIO_H */
