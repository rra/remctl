/* $Id$ */
/* Test suite for the high-level remctl library API. */

#include <config.h>
#include <system.h>

#include <signal.h>
#ifdef HAVE_SYS_SELECT_H
# include <sys/select.h>
#endif
#include <sys/time.h>
#include <sys/uio.h>
#include <sys/wait.h>

#include <client/remctl.h>
#include <client/internal.h>
#include <tests/libtest.h>
#include <util/util.h>

/* Spawn a remctl server on port 14444 to use for testing and return the PID
   for later killing. */
static pid_t
spawn_remctl(char *principal)
{
    pid_t child;
    struct timeval tv;

    if (access("data/pid", F_OK) == 0)
        if (unlink("data/pid") != 0)
            sysdie("cannot unlink data/pid");
    child = fork();
    if (child < 0)
        return child;
    else if (child == 0) {
        execl("../server/remctld", "remctld", "-m", "-p", "14444", "-s",
              principal, "-P", "data/pid", "-f", "data/simple.conf", "-d",
              (char *) 0);
        _exit(1);
    } else {
        alarm(1);
        while (access("data/pid", F_OK) != 0) {
            tv.tv_sec = 0;
            tv.tv_usec = 10000;
            select(0, NULL, NULL, NULL, &tv);
        }
        alarm(0);
        return child;
    }
}

/* Takes the current test number, the principal, and the protocol version and
   runs a set of tests.  Due to the compatibility layer, we should be able to
   run the same commands regardless of the protocol (we're not testing any of
   the v2-specific features here).  Returns the next test number. */
static int
do_tests(int n, const char *principal, int protocol)
{
    struct remctl *r;
    struct iovec *command;
    struct remctl_output *output;
    const char *test[] = { "test", "test", NULL };

    r = remctl_new();
    ok(n++, r != NULL);
    ok_string(n++, "No error", remctl_error(r));
    r->protocol = protocol;
    ok(n++, remctl_open(r, "localhost", 14444, principal));
    ok_string(n++, "No error", remctl_error(r));
    ok(n++, remctl_command(r, test, 1));
    ok_string(n++, "No error", remctl_error(r));
    output = remctl_output(r);
    ok(n++, output != NULL);
    ok_int(n++, REMCTL_OUT_OUTPUT, output->type);
    ok_int(n++, 12, output->length);
    if (output->data == NULL)
        ok(n++, 0);
    else
        ok(n++, memcmp("hello world\n", output->data, 11) == 0);
    ok_int(n++, 1, output->stream);
    output = remctl_output(r);
    ok(n++, output != NULL);
    ok_int(n++, REMCTL_OUT_STATUS, output->type);
    ok_int(n++, 0, output->status);
    command = xcalloc(2, sizeof(struct iovec));
    command[0].iov_base = (char *) "test";
    command[0].iov_len = 4;
    command[1].iov_base = (char *) "test";
    command[1].iov_len = 4;
    ok(n++, remctl_commandv(r, command, 2, 1));
    ok_string(n++, "No error", remctl_error(r));
    output = remctl_output(r);
    ok(n++, output != NULL);
    ok_int(n++, REMCTL_OUT_OUTPUT, output->type);
    ok_int(n++, 12, output->length);
    if (output->data == NULL)
        ok(n++, 0);
    else
        ok(n++, memcmp("hello world\n", output->data, 11) == 0);
    ok_int(n++, 1, output->stream);
    output = remctl_output(r);
    ok(n++, output != NULL);
    ok_int(n++, REMCTL_OUT_STATUS, output->type);
    ok_int(n++, 0, output->status);
    remctl_close(r);
    ok(n++, 1);

    return n;
}

int
main(void)
{
    char *principal;
    pid_t remctld;
    struct remctl_result *result;
    const char *test[] = { "test", "test", NULL };
    int n;

    test_init(55);

    principal = kerberos_setup();
    if (principal == NULL) {
        skip_block(1, 55, "Kerberos tests not configured");
    } else {
        remctld = spawn_remctl(principal);
        if (remctld <= 0)
            die("cannot spawn remctld");

        n = do_tests(1, principal, 1);
        n = do_tests(n, principal, 2);

        /* We don't have a way of forcing this to use a particular protocol,
           so we always do it via protocol v2.  But if the above worked with
           protocol v1, and this wrapper works with v2, everything should have
           gotten tested. */
        result = remctl("localhost", 14444, principal, test);
        ok(n++, result != NULL);
        ok_int(n++, 0, result->status);
        ok_int(n++, 12, result->stdout_len);
        if (result->stdout == NULL)
            ok(n++, 0);
        else
            ok(n++, memcmp("hello world\n", result->stdout, 11) == 0);
        ok(n++, result->error == NULL);

        kill(remctld, SIGTERM);
        waitpid(remctld, NULL, 0);
    }
    unlink("data/test.cache");
    unlink("data/pid");
    exit(0);
}
