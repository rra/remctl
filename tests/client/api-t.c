/* $Id$ */
/* Test suite for the high-level remctl library API. */

#include <config.h>
#include <system.h>

#include <signal.h>
#include <sys/uio.h>
#include <sys/wait.h>

#include <client/remctl.h>
#include <tests/libtest.h>
#include <util/util.h>

/* Spawn a remctl server on port 14444 to use for testing and return the PID
   for later killing. */
static pid_t
spawn_remctl(char *principal)
{
    pid_t child;

    child = fork();
    if (child < 0)
        return child;
    else if (child == 0) {
        putenv("KRB5_KTNAME=data/test.keytab");
        execl("../server/remctld", "remctld", "-m", "-p", "14444", "-s",
              principal, "-f", "data/simple.conf", (char *) 0);
        _exit(1);
    } else
        return child;
}


int
main(void)
{
    struct remctl *r;
    int n = 1;
    char *principal;
    const char *test[] = { "test", "test", NULL };
    struct iovec *command;
    struct remctl_result *result;
    struct remctl_output *output;
    pid_t remctld;

    test_init(30);

    principal = kerberos_setup();
    if (principal == NULL) {
        skip_block(1, 30, "Kerberos tests not configured");
    } else {
        remctld = spawn_remctl(principal);
        if (remctld <= 0)
            die("cannot spawn remctld");
        r = remctl_new();
        ok(1, r != NULL);
        ok_string(2, "No error", remctl_error(r));
        ok(3, remctl_open(r, "localhost", 14444, principal));
        ok_string(4, "No error", remctl_error(r));
        ok(5, remctl_command(r, test, 1));
        ok_string(6, "No error", remctl_error(r));
        output = remctl_output(r);
        ok(7, output != NULL);
        ok_int(8, REMCTL_OUT_OUTPUT, output->type);
        ok_int(9, 12, output->length);
        ok(10, memcmp("hello world\n", output->data, 11) == 0);
        ok_int(11, 1, output->stream);
        output = remctl_output(r);
        ok(12, output != NULL);
        ok_int(13, REMCTL_OUT_STATUS, output->type);
        ok_int(14, 0, output->status);
        command = xcalloc(2, sizeof(struct iovec));
        command[0].iov_base = "test";
        command[0].iov_len = 4;
        command[1].iov_base = "test";
        command[1].iov_len = 4;
        ok(15, remctl_commandv(r, command, 2, 1));
        ok_string(16, "No error", remctl_error(r));
        output = remctl_output(r);
        ok(17, output != NULL);
        ok_int(18, REMCTL_OUT_OUTPUT, output->type);
        ok_int(19, 12, output->length);
        ok(20, memcmp("hello world\n", output->data, 11) == 0);
        ok_int(21, 1, output->stream);
        output = remctl_output(r);
        ok(22, output != NULL);
        ok_int(23, REMCTL_OUT_STATUS, output->type);
        ok_int(24, 0, output->status);
        remctl_close(r);
        ok(25, 1);

        result = remctl("localhost", 14444, principal, test);
        ok(26, result != NULL);
        ok_int(27, 0, result->status);
        ok_int(28, 12, result->stdout_len);
        ok(29, memcmp("hello world\n", result->stdout, 11) == 0);
        ok(30, result->error == NULL);
        kill(remctld, SIGTERM);
        waitpid(remctld, NULL, 0);
    }
    unlink("data/test.cache");
    exit(0);
}
