/*
 * Prototypes for message and error reporting (possibly fatal).
 *
 * Copyright 2008, 2010 Board of Trustees, Leland Stanford Jr. University
 * Copyright (c) 2004, 2005, 2006
 *     by Internet Systems Consortium, Inc. ("ISC")
 * Copyright (c) 1991, 1994, 1995, 1996, 1997, 1998, 1999, 2000, 2001,
 *     2002, 2003 by The Internet Software Consortium and Rich Salz
 *
 * See LICENSE for licensing terms.
 */

#ifndef UTIL_MESSAGES_H
#define UTIL_MESSAGES_H 1

#include <config.h>
#include <portable/macros.h>

#include <stdarg.h>

BEGIN_DECLS

/* Default to a hidden visibility for all util functions. */
#pragma GCC visibility push(hidden)

/*
 * The reporting functions.  The ones prefaced by "sys" add a colon, a space,
 * and the results of strerror(errno) to the output and are intended for
 * reporting failures of system calls.
 */
void debug(const char *, ...)
    __attribute__((__nonnull__, __format__(printf, 1, 2)));
void notice(const char *, ...)
    __attribute__((__nonnull__, __format__(printf, 1, 2)));
void sysnotice(const char *, ...)
    __attribute__((__nonnull__, __format__(printf, 1, 2)));
void warn(const char *, ...)
    __attribute__((__nonnull__, __format__(printf, 1, 2)));
void syswarn(const char *, ...)
    __attribute__((__nonnull__, __format__(printf, 1, 2)));
void die(const char *, ...)
    __attribute__((__nonnull__, __noreturn__, __format__(printf, 1, 2)));
void sysdie(const char *, ...)
    __attribute__((__nonnull__, __noreturn__, __format__(printf, 1, 2)));

/*
 * Set the handlers for various message functions.  All of these functions
 * take a count of the number of handlers and then function pointers for each
 * of those handlers.  These functions are not thread-safe; they set global
 * variables.
 */
void message_handlers_debug(int count, ...);
void message_handlers_notice(int count, ...);
void message_handlers_warn(int count, ...);
void message_handlers_die(int count, ...);

/*
 * Some useful handlers, intended to be passed to message_handlers_*.  All
 * handlers take the length of the formatted message, the format, a variadic
 * argument list, and the errno setting if any.
 */
void message_log_stdout(int, const char *, va_list, int)
    __attribute((__nonnull__));
void message_log_stderr(int, const char *, va_list, int)
    __attribute((__nonnull__));
void message_log_syslog_debug(int, const char *, va_list, int)
    __attribute((__nonnull__));
void message_log_syslog_info(int, const char *, va_list, int)
    __attribute((__nonnull__));
void message_log_syslog_notice(int, const char *, va_list, int)
    __attribute((__nonnull__));
void message_log_syslog_warning(int, const char *, va_list, int)
    __attribute((__nonnull__));
void message_log_syslog_err(int, const char *, va_list, int)
    __attribute((__nonnull__));
void message_log_syslog_crit(int, const char *, va_list, int)
    __attribute((__nonnull__));

/* The type of a message handler. */
typedef void (*message_handler_func)(int, const char *, va_list, int);

/* If non-NULL, called before exit and its return value passed to exit. */
extern int (*message_fatal_cleanup)(void);

/*
 * If non-NULL, prepended (followed by ": ") to all messages printed by either
 * message_log_stdout or message_log_stderr.
 */
extern const char *message_program_name;

/* Undo default visibility change. */
#pragma GCC visibility pop

END_DECLS

#endif /* UTIL_MESSAGES_H */
