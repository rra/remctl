/*
 * Replacement for a missing strndup.
 *
 * The canonical version of this file is maintained in the rra-c-util package,
 * which can be found at <http://www.eyrie.org/~eagle/software/rra-c-util/>.
 *
 * Written by Russ Allbery <rra@stanford.edu>
 *
 * The authors hereby relinquish any claim to any copyright that they may have
 * in this work, whether granted under contract or by operation of law or
 * international treaty, and hereby commit to the public, at large, that they
 * shall not, at any time in the future, seek to enforce any copyright in this
 * work against any person or entity, or prevent any person or entity from
 * copying, publishing, distributing or creating derivative works of this
 * work.
 */

#include <config.h>
#include <portable/system.h>

#include <errno.h>

/*
 * If we're running the test suite, rename the functions to avoid conflicts
 * with the system versions.
 */
#if TESTING
# undef strndup
# define strndup test_strndup
char *test_strndup(const char *, size_t);
#endif

char *
strndup(const char *s, size_t n)
{
    const char *p;
    size_t length;
    char *copy;

    if (s == NULL) {
        errno = EINVAL;
        return NULL;
    }

    /* Don't assume that the source string is nul-terminated. */
    for (p = s; (size_t) (p - s) < n && *p != '\0'; p++)
        ;
    length = p - s;
    copy = malloc(length + 1);
    if (copy == NULL)
        return NULL;
    memcpy(copy, s, length);
    copy[length] = '\0';
    return copy;
}
