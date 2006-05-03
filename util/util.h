/*  $Id$
**
**  Utility functions.
**
**  This is a variety of utility functions that are used internally by pieces
**  of remctl.  Many of them came originally from INN.
*/

#ifndef UTIL_UTIL_H
#define UTIL_UTIL_H 1

#include <config.h>

#include <stdarg.h>
#include <sys/types.h>

#ifdef HAVE_GSSAPI_H
# include <gssapi.h>
#else
# include <gssapi/gssapi_generic.h>
#endif

/* __attribute__ is available in gcc 2.5 and later, but only with gcc 2.7
   could you use the __format__ form of the attributes, which is what we use
   (to avoid confusion with other macros). */
#ifndef __attribute__
# if __GNUC__ < 2 || (__GNUC__ == 2 && __GNUC_MINOR__ < 7)
#  define __attribute__(spec)   /* empty */
# endif
#endif

/* Used for unused parameters to silence gcc warnings. */
#define UNUSED  __attribute__((__unused__))

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

/* Concatenate NULL-terminated strings into a newly allocated string. */
extern char *concat(const char *first, ...);

/* Given a baes path and a file name, create a newly allocated path string.
   The name will be appended to base with a / between them.  Exceptionally, if
   name begins with a slash, it will be strdup'd and returned as-is. */
extern char *concatpath(const char *base, const char *name);

/* Failure return codes from token_send and token_recv. */
enum token_status {
    TOKEN_OK = 0,
    TOKEN_FAIL_SYSTEM = -1,     /* System call failed, error in errno */
    TOKEN_FAIL_INVALID = -2,    /* Invalid token from remote site */
    TOKEN_FAIL_LARGE = -3       /* Token data exceeds max length. */
};

/* Sending and receiving tokens. */
enum token_status token_send(int fd, gss_buffer_t, int flags);
enum token_status token_recv(int fd, gss_buffer_t, int *flags, size_t max);

/* The reporting functions.  The ones prefaced by "sys" add a colon, a space,
   and the results of strerror(errno) to the output and are intended for
   reporting failures of system calls. */
extern void debug(const char *, ...)
    __attribute__((__format__(printf, 1, 2)));
extern void notice(const char *, ...)
    __attribute__((__format__(printf, 1, 2)));
extern void sysnotice(const char *, ...)
    __attribute__((__format__(printf, 1, 2)));
extern void warn(const char *, ...)
    __attribute__((__format__(printf, 1, 2)));
extern void syswarn(const char *, ...)
    __attribute__((__format__(printf, 1, 2)));
extern void die(const char *, ...)
    __attribute__((__noreturn__, __format__(printf, 1, 2)));
extern void sysdie(const char *, ...)
    __attribute__((__noreturn__, __format__(printf, 1, 2)));

/* Set the handlers for various message functions.  All of these functions
   take a count of the number of handlers and then function pointers for each
   of those handlers.  These functions are not thread-safe; they set global
   variables. */
extern void message_handlers_debug(int count, ...);
extern void message_handlers_notice(int count, ...);
extern void message_handlers_warn(int count, ...);
extern void message_handlers_die(int count, ...);

/* Some useful handlers, intended to be passed to message_handlers_*.  All
   handlers take the length of the formatted message, the format, a variadic
   argument list, and the errno setting if any. */
extern void message_log_stdout(int, const char *, va_list, int);
extern void message_log_stderr(int, const char *, va_list, int);
extern void message_log_syslog_debug(int, const char *, va_list, int);
extern void message_log_syslog_info(int, const char *, va_list, int);
extern void message_log_syslog_notice(int, const char *, va_list, int);
extern void message_log_syslog_warning(int, const char *, va_list, int);
extern void message_log_syslog_err(int, const char *, va_list, int);
extern void message_log_syslog_crit(int, const char *, va_list, int);

/* The type of a message handler. */
typedef void (*message_handler_func)(int, const char *, va_list, int);

/* If non-NULL, called before exit and its return value passed to exit. */
extern int (*message_fatal_cleanup)(void);

/* If non-NULL, prepended (followed by ": ") to all messages printed by either
   message_log_stdout or message_log_stderr. */
extern const char *message_program_name;

struct vector {
    size_t count;
    size_t allocated;
    char **strings;
};

struct cvector {
    size_t count;
    size_t allocated;
    const char **strings;
};

/* Create a new, empty vector. */
struct vector *vector_new(void);
struct cvector *cvector_new(void);

/* Add a string to a vector.  Resizes the vector if necessary. */
void vector_add(struct vector *, const char *string);
void cvector_add(struct cvector *, const char *string);

/* Resize the array of strings to hold size entries.  Saves reallocation work
   in vector_add if it's known in advance how many entries there will be. */
void vector_resize(struct vector *, size_t size);
void cvector_resize(struct cvector *, size_t size);

/* Reset the number of elements to zero, freeing all of the strings for a
   regular vector, but not freeing the strings array (to cut down on memory
   allocations if the vector will be reused). */
void vector_clear(struct vector *);
void cvector_clear(struct cvector *);

/* Free the vector and all resources allocated for it. */
void vector_free(struct vector *);
void cvector_free(struct cvector *);

/* Split functions build a vector from a string.  vector_split splits on a
   specified character, while vector_split_space splits on any sequence of
   spaces or tabs (not any sequence of whitespace, as just spaces or tabs is
   more useful).  The cvector versions destructively modify the provided
   string in-place to insert nul characters between the strings.  If the
   vector argument is NULL, a new vector is allocated; otherwise, the provided
   one is reused.

   Empty strings will yield zero-length vectors.  Adjacent delimiters are
   treated as a single delimiter by *_split_space, but *not* by *_split, so
   callers of *_split should be prepared for zero-length strings in the
   vector. */
struct vector *vector_split(const char *string, char sep, struct vector *);
struct vector *vector_split_space(const char *string, struct vector *);
struct cvector *cvector_split(char *string, char sep, struct cvector *);
struct cvector *cvector_split_space(char *string, struct cvector *);

/* Build a string from a vector by joining its components together with the
   specified string as separator.  Returns a newly allocated string; caller is
   responsible for freeing. */
char *vector_join(const struct vector *, const char *seperator);
char *cvector_join(const struct cvector *, const char *separator);

/* Exec the given program with the vector as its arguments.  Return behavior
   is the same as execv.  Note the argument order is different than the other
   vector functions (but the same as execv). */
int vector_exec(const char *path, struct vector *);
int cvector_exec(const char *path, struct cvector *);

/* The functions are actually macros so that we can pick up the file and line
   number information for debugging error messages without the user having to
   pass those in every time. */
#define xcalloc(n, size)        x_calloc((n), (size), __FILE__, __LINE__)
#define xmalloc(size)           x_malloc((size), __FILE__, __LINE__)
#define xrealloc(p, size)       x_realloc((p), (size), __FILE__, __LINE__)
#define xstrdup(p)              x_strdup((p), __FILE__, __LINE__)
#define xstrndup(p, size)       x_strndup((p), (size), __FILE__, __LINE__)
#define xvasprintf(p, f, a)     x_vasprintf((p), (f), (a), __FILE__, __LINE__)

/* asprintf is a special case since it takes variable arguments.  If we have
   support for variadic macros, we can still pass in the file and line and
   just need to put them somewhere else in the argument list than last.
   Otherwise, just call x_asprintf directly.  This means that the number of
   arguments x_asprintf takes must vary depending on whether variadic macros
   are supported. */
#ifdef HAVE_C99_VAMACROS
# define xasprintf(p, f, ...) \
    x_asprintf((p), __FILE__, __LINE__, (f), __VA_ARGS__)
#elif HAVE_GNU_VAMACROS
# define xasprintf(p, f, args...) \
    x_asprintf((p), __FILE__, __LINE__, (f), args)
#else
# define xasprintf x_asprintf
#endif

/* Last two arguments are always file and line number.  These are internal
   implementations that should not be called directly.  ISO C99 says that
   identifiers beginning with _ and a lowercase letter are reserved for
   identifiers of file scope, so while the position of libraries in the
   standard isn't clear, it's probably not entirely kosher to use _xmalloc
   here.  Use x_malloc instead. */
extern void *x_calloc(size_t, size_t, const char *, int);
extern void *x_malloc(size_t, const char *, int);
extern void *x_realloc(void *, size_t, const char *, int);
extern char *x_strdup(const char *, const char *, int);
extern char *x_strndup(const char *, size_t, const char *, int);
extern int x_vasprintf(char **, const char *, va_list, const char *, int);

/* asprintf special case. */
#if HAVE_C99_VAMACROS || HAVE_GNU_VAMACROS
extern int x_asprintf(char **, const char *, int, const char *, ...);
#else
extern int x_asprintf(char **, const char *, ...);
#endif

/* Failure handler takes the function, the size, the file, and the line. */
typedef void (*xmalloc_handler_type)(const char *, size_t, const char *, int);

/* The default error handler. */
void xmalloc_fail(const char *, size_t, const char *, int);

/* Assign to this variable to choose a handler other than the default, which
   just calls sysdie. */
extern xmalloc_handler_type xmalloc_error_handler;

END_DECLS

#endif /* UTIL_UTIL_H */
