/*  $Id$
**
**  Logging, debugging, and error reporting functions.
**
**  This collection of functions facilitate logging, debugging, and error
**  reporting in a flexible manner that can be used by libraries as well as by
**  programs.  The functions are based around the idea of handlers, which take
**  a message and do something appropriate with it.  The program can set the
**  appropriate handlers for all the message reporting functions, and then
**  library code can use them with impunity and know the right thing will
**  happen with the messages.
*/

#ifndef MESSAGES_H
#define MESSAGES_H 1

#include <stdarg.h>

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

#endif /* MESSAGES_H */
