/* $Id$ */
/* Test suite for the high-level remctl library API. */

#include <config.h>
#include <system.h>

#include <krb5.h>
#include <sys/uio.h>
#include <sys/wait.h>

#include <client/remctl.h>
#include <tests/libtest.h>
#include <util/util.h>

/* Set up our Kerberos access and return the principal to use for the remote
   remctl connection, NULL if we couldn't initialize things.  We read the
   principal to use for authentication out of a file and fork kinit to obtain
   a Kerberos ticket cache. */
char *
kerberos_setup(void)
{
    static const char format[] = "kinit -k -K data/test.keytab %s";
    FILE *file;
    char principal[256], *command;
    size_t length;
    int status;

    if (access("../data/test.keytab", F_OK) == 0)
        chdir("..");
    else if (access("tests/data/test.keytab", F_OK) == 0)
        chdir("tests");
    file = fopen("data/test.principal", "r");
    if (file == NULL)
        return NULL;
    if (fgets(principal, sizeof(principal), file) == NULL) {
        fclose(file);
        return NULL;
    }
    fclose(file);
    if (principal[strlen(principal) - 1] != '\n')
        return NULL;
    principal[strlen(principal) - 1] = '\0';
    length = strlen(format) + strlen(principal);
    command = xmalloc(length);
    snprintf(command, length, format, principal);
    putenv("KRB5CCNAME=data/test.cache");
    status = system(command);
    free(command);
    if (status == -1 || WEXITSTATUS(status) != 0)
        return 0;
    return xstrdup(principal);
}


int
main(void)
{
    struct remctl *r;
    int n = 1;
    char *principal;
    const char *test[] = { "test", "test", NULL };
    struct iovec *command;

    test_init(10);

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

        ok(10, remctl("localhost", REMCTL_PORT, principal, test) == NULL);
    }
    unlink("data/test.cache");
    exit(0);
}
