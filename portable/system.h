/*
 * Standard system includes and portability adjustments.
 *
 * Declarations of routines and variables in the C library.  Including this
 * file is the equivalent of including all of the following headers,
 * portably:
 *
 *     #include <inttypes.h>
 *     #include <limits.h>
 *     #include <stdarg.h>
 *     #include <stdbool.h>
 *     #include <stddef.h>
 *     #include <stdio.h>
 *     #include <stdlib.h>
 *     #include <stdint.h>
 *     #include <string.h>
 *     #include <strings.h>
 *     #include <sys/types.h>
 *     #include <unistd.h>
 *
 * Missing functions are provided via #define or prototyped if available from
 * the portable helper library.  Also provides some standard #defines.
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

#ifndef PORTABLE_SYSTEM_H
#define PORTABLE_SYSTEM_H 1

/* Make sure we have our configuration information. */
#include <config.h>

/* BEGIN_DECL and __attribute__. */
#include <portable/macros.h>

/* A set of standard ANSI C headers.  We don't care about pre-ANSI systems. */
#if HAVE_INTTYPES_H
# include <inttypes.h>
#endif
#include <limits.h>
#include <stdarg.h>
#include <stddef.h>
#if HAVE_STDINT_H
# include <stdint.h>
#endif
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#if HAVE_STRINGS_H
# include <strings.h>
#endif
#include <sys/types.h>
#if HAVE_UNISTD_H
# include <unistd.h>
#endif

/* SCO OpenServer gets int32_t from here. */
#if HAVE_SYS_BITYPES_H
# include <sys/bitypes.h>
#endif

/* Get the bool type. */
#include <portable/stdbool.h>

/* Windows provides snprintf under a different name. */
#ifdef _WIN32
# define snprintf _snprintf
#endif

/* Define sig_atomic_t if it's not available in signal.h. */
#ifndef HAVE_SIG_ATOMIC_T
typedef int sig_atomic_t;
#endif

/* Windows does not define ssize_t. */
#ifndef HAVE_SSIZE_T
typedef ptrdiff_t ssize_t;
#endif

/*
 * POSIX requires that these be defined in <unistd.h>.  If one of them has
 * been defined, all the rest almost certainly have.
 */
#ifndef STDIN_FILENO
# define STDIN_FILENO  0
# define STDOUT_FILENO 1
# define STDERR_FILENO 2
#endif

/*
 * C99 requires va_copy.  Older versions of GCC provide __va_copy.  Per the
 * Autoconf manual, memcpy is a generally portable fallback.
 */
#ifndef va_copy
# ifdef __va_copy
#  define va_copy(d, s) __va_copy((d), (s))
# else
#  define va_copy(d, s) memcpy(&(d), &(s), sizeof(va_list))
# endif
#endif

BEGIN_DECLS

/* Default to a hidden visibility for all portability functions. */
#pragma GCC visibility push(hidden)

/*
 * Provide prototypes for functions not declared in system headers.  Use the
 * HAVE_DECL macros for those functions that may be prototyped but implemented
 * incorrectly or implemented without a prototype.
 */
#if !HAVE_ASPRINTF
extern int asprintf(char **, const char *, ...)
    __attribute__((__format__(printf, 2, 3)));
extern int vasprintf(char **, const char *, va_list);
#endif
#if !HAVE_DECL_SNPRINTF
extern int snprintf(char *, size_t, const char *, ...)
    __attribute__((__format__(printf, 3, 4)));
#endif
#if !HAVE_DECL_VSNPRINTF
extern int vsnprintf(char *, size_t, const char *, va_list);
#endif
#if !HAVE_DAEMON
extern int daemon(int, int);
#endif
#if !HAVE_MKSTEMP
extern int mkstemp(char *);
#endif
#if !HAVE_REALLOCARRAY
extern void *reallocarray(void *, size_t, size_t);
#endif
#if !HAVE_SETENV
extern int setenv(const char *, const char *, int);
#endif
#if !HAVE_STRLCAT
extern size_t strlcat(char *, const char *, size_t);
#endif
#if !HAVE_STRLCPY
extern size_t strlcpy(char *, const char *, size_t);
#endif
#if !HAVE_STRNDUP
extern char *strndup(const char *, size_t);
#endif

/* Undo default visibility change. */
#pragma GCC visibility pop

END_DECLS

#endif /* !PORTABLE_SYSTEM_H */
