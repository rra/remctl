/*  $Id$
**
**  Some utility routines for writing tests.
*/

#ifndef LIBTEST_H
#define LIBTEST_H 1

#include <config.h>

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

/* Used for iterating through arrays.  ARRAY_SIZE returns the number of
   elements in the array (useful for a < upper bound in a for loop) and
   ARRAY_END returns a pointer to the element past the end (ISO C99 makes it
   legal to refer to such a pointer as long as it's never dereferenced). */
#define ARRAY_SIZE(array)       (sizeof(array) / sizeof((array)[0]))
#define ARRAY_END(array)        (&(array)[ARRAY_SIZE(array)])

/* A global buffer into which errors_capture stores errors. */
extern char *errors;

BEGIN_DECLS

void ok(int n, int success);
void ok_int(int n, int wanted, int seen);
void ok_double(int n, double wanted, double seen);
void ok_string(int n, const char *wanted, const char *seen);
void skip(int n, const char *reason);

/* Report the same status on, or skip, the next count tests. */
void ok_block(int n, int count, int success);
void skip_block(int n, int count, const char *reason);

/* Print out the number of tests and set standard output to line buffered. */
void test_init(int count);

/* Turn on capturing of errors with errors_capture.  Errors reported by warn
   will be stored in the global errors variable.  Turn this off again with
   errors_uncapture.  Caller is responsible for freeing errors when done. */
void errors_capture(void);
void errors_uncapture(void);

/* Set up Kerberos, returning the test principal if we were successful and
   false otherwise. */
char *kerberos_setup(void);

/* Spawn an external remctld process and return the PID. */
pid_t spawn_remctld(const char *principal);

END_DECLS

#endif /* LIBTEST_H */
