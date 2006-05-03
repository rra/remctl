/* $Id$ */
/* Test suite for the high-level remctl library API. */

#include <config.h>
#include <system.h>

#include <sys/uio.h>

#include <client/remctl.h>
#include <tests/libtest.h>
#include <util/util.h>

int
main(void)
{
    struct remctl *r;
    int n = 1;
    char *principal;
    const char *test[] = { "test", "test", NULL };
    struct iovec *command;
    struct remctl_result *result;

    test_init(11);

    principal = kerberos_setup();
    if (principal == NULL) {
        skip_block(1, 10, "Kerberos tests not configured");
    } else {
        r = remctl_open("localhost", REMCTL_PORT, principal);
        ok(1, r != NULL);
        ok_string(2, "No error", remctl_error(r));
        ok(3, !remctl_command(r, test, 1));
        ok_string(4, "Not yet implemented", remctl_error(r));
        command = xcalloc(2, sizeof(struct iovec));
        command[0].iov_base = "test";
        command[0].iov_len = 4;
        command[1].iov_base = "test";
        command[1].iov_len = 4;
        ok(5, !remctl_commandv(r, command, 2, 1));
        ok_string(6, "Not yet implemented", remctl_error(r));
        ok(7, remctl_output(r) == NULL);
        ok_string(8, "Not yet implemented", remctl_error(r));
        remctl_close(r);
        ok(9, 1);

        result = remctl("localhost", REMCTL_PORT, principal, test);
        ok(10, result != NULL);
        ok_string(11, "Not yet implemented", result->error);
    }
    unlink("data/test.cache");
    exit(0);
}
