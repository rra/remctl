/*  $Id$
**
**  Declarations of routines and variables in the C library.  Including this
**  file is the equivalent of including all of the following headers,
**  portably:
**
**      #include <sys/types.h>
**      #include <stdarg.h>
**      #include <stdio.h>
**      #include <stdlib.h>
**      #include <stddef.h>
**      #include <stdint.h>
**      #include <string.h>
**      #include <unistd.h>
**
**  Missing functions are provided via #define or prototyped if available from
**  the util helper library.  Also provides some standard #defines.
*/

#ifndef SYSTEM_H
#define SYSTEM_H 1

/* Make sure we have our configuration information. */
#include <config.h>

/* A set of standard ANSI C headers.  We don't care about pre-ANSI systems. */
#include <stdarg.h>
#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/types.h>
#include <string.h>
#if HAVE_INTTYPES_H
# include <inttypes.h>
#endif
#if HAVE_STDINT_H
# include <stdint.h>
#endif
#include <unistd.h>

/* SCO OpenServer gets int32_t from here. */
#if HAVE_SYS_BITYPES_H
# include <sys/bitypes.h>
#endif

/* __attribute__ is available in gcc 2.5 and later, but only with gcc 2.7
   could you use the __format__ form of the attributes, which is what we use
   (to avoid confusion with other macros). */
#ifndef __attribute__
# if __GNUC__ < 2 || (__GNUC__ == 2 && __GNUC_MINOR__ < 7)
#  define __attribute__(spec)   /* empty */
# endif
#endif

/* BEGIN_DECLS is used at the beginning of declarations so that C++
   compilers don't mangle their names.  END_DECLS is used at the end. */
#undef BEGIN_DECLS
#undef END_DECLS
#ifdef __cplusplus
# define BEGIN_DECLS    extern "C" {
# define END_DECLS      }
#else
# define BEGIN_DECLS    /* empty */
# define END_DECLS      /* empty */
#endif

BEGIN_DECLS

/* Provide prototypes for functions not declared in system headers.  Use the
   HAVE_DECL macros for those functions that may be prototyped but
   implemented incorrectly or implemented without a prototype. */
#if !HAVE_INET_NTOP
extern const char *     inet_ntop(int, const void *, char *, socklen_t);
#endif
#if !HAVE_ASPRINTF
extern int              asprintf(char **, const char *, ...);
extern int              vasprintf(char **, const char *, va_list);
#endif
#if !HAVE_DECL_SNPRINTF
extern int              snprintf(char *, size_t, const char *, ...)
    __attribute__((__format__(printf, 3, 4)));
#endif
#if !HAVE_DECL_VSNPRINTF
extern int              vsnprintf(char *, size_t, const char *, va_list);
#endif
#if !HAVE_DAEMON
extern int              daemon(int, int);
#endif
#if !HAVE_SETENV
extern int              setenv(const char *, const char *, int);
#endif
#if !HAVE_STRLCAT
extern size_t           strlcat(char *, const char *, size_t);
#endif
#if !HAVE_STRLCPY
extern size_t           strlcpy(char *, const char *, size_t);
#endif

END_DECLS

/* POSIX requires that these be defined in <unistd.h>.  If one of them has
   been defined, all the rest almost certainly have. */
#ifndef STDIN_FILENO
# define STDIN_FILENO   0
# define STDOUT_FILENO  1
# define STDERR_FILENO  2
#endif

/* C99 requires va_copy.  Older versions of GCC provide __va_copy.  Per the
   Autoconf manual, memcpy is a generally portable fallback. */
#ifndef va_copy
# ifdef __va_copy
#  define va_copy(d, s)         __va_copy((d), (s))
# else
#  define va_copy(d, s)         memcpy(&(d), &(s), sizeof(va_list))
# endif
#endif

#endif /* !SYSTEM_H */
