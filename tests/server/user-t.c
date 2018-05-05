/*
 * Test suite for running commands as a designated user.
 *
 * Written by Russ Allbery <eagle@eyrie.org>
 * Copyright 2018 Russ Allbery <eagle@eyrie.org>
 * Copyright 2012-2014
 *     The Board of Trustees of the Leland Stanford Junior University
 *
 * SPDX-License-Identifier: MIT
 */

#include <config.h>
#include <portable/system.h>

#include <pwd.h>

#include <client/remctl.h>
#include <tests/tap/basic.h>
#include <tests/tap/kerberos.h>
#include <tests/tap/remctl.h>
#include <tests/tap/string.h>


/*
 * Run the remote user command with the given variable and parse the UID and
 * GID values from the server.  Return true on success and false if there was
 * an error.
 */
static bool
test_user(struct remctl *r, const char *subcommand, uid_t *uid, gid_t *gid)
{
    struct remctl_output *output;
    char *old, *end;
    char *data = NULL;
    long value;
    const char *command[] = { "test", NULL, NULL };

    /* Run the command and gather its output. */
    command[1] = subcommand;
    if (!remctl_command(r, command)) {
        diag("remctl error %s", remctl_error(r));
        return false;
    }
    do {
        output = remctl_output(r);
        switch (output->type) {
        case REMCTL_OUT_OUTPUT:
            if (data == NULL)
                data = bstrndup(output->data, output->length);
            else {
                old = data;
                basprintf(&data, "%s%.*s", data, (int) output->length,
                          output->data);
                free(old);
            }
            break;
        case REMCTL_OUT_STATUS:
            if (output->status != 0) {
                free(data);
                diag("test env returned status %d", output->status);
                return false;
            }
            break;
        case REMCTL_OUT_ERROR:
            free(data);
            diag("test env returned error: %.*s", (int) output->length,
                 output->data);
            return false;
        case REMCTL_OUT_DONE:
            free(data);
            diag("unexpected done token");
            return false;
        }
    } while (output->type == REMCTL_OUT_OUTPUT);

    /* If there is no output, fail. */
    if (data == NULL) {
        diag("test env returned no output");
        return false;
    }

    /* We have the output.  Now parse it into UID and GID. */
    data[strlen(data) - 1] = '\0';
    value = strtol(data, &end, 10);
    if (value < 0 || end == data) {
        diag("invalid output: %s", data);
        free(data);
        return false;
    }
    *uid = (uid_t) value;
    value = strtol(end, NULL, 10);
    if (value < 0) {
        diag("invalid output: %s", data);
        free(data);
        return false;
    }
    *gid = (gid_t) value;
    free(data);
    return true;
}


int
main(void)
{
    struct kerberos_config *config;
    uid_t uid = (uid_t) -1;
    gid_t gid = (gid_t) -1;
    struct passwd *pw;
    struct remctl *r;
    char *tmpdir, *confpath, *cmd;
    FILE *conf;

    /* Unless we have Kerberos available, we can't really do anything. */
    config = kerberos_setup(TAP_KRB_NEEDS_KEYTAB);

    /* Determine the UID and GID of the current user. */
    pw = getpwuid(getuid());
    if (pw == NULL)
        skip_all("cannot retrieve UID and GID");
    if (pw->pw_uid == 0)
        skip_all("must run as non-root user");

    /* Write out our temporary configuration file. */
    tmpdir = test_tmpdir();
    basprintf(&confpath, "%s/conf-user", tmpdir);
    cmd = test_file_path("data/cmd-user");
    if (cmd == NULL)
        bail("cannot find tests/data/cmd-user");
    conf = fopen(confpath, "w");
    if (conf == NULL)
        sysbail("cannot create %s", confpath);
    fprintf(conf, "test root %s ANYUSER\n", cmd);
    fprintf(conf, "test user %s user=%s ANYUSER\n", cmd, pw->pw_name);
    fclose(conf);

    /*
     * Now we can start remctl with our temporary configuration file.  We have
     * to start remctld under fakeroot so that it can change users.  This may
     * call skip_all if fakeroot wasn't found during configure.
     */
    remctld_start_fakeroot(config, "tmp/conf-user", NULL);

    plan(6);

    /* Finally, we can actually do some testing. */
    r = remctl_new();
    if (!remctl_open(r, "localhost", 14373, config->principal))
        bail("cannot contact remctld");
    ok(test_user(r, "root", &uid, &gid), "test root command");
    is_int(0, uid, "remctld thinks it's running UID 0");
    is_int(0, gid, "...and GID 0");
    ok(test_user(r, "user", &uid, &gid), "test user command");
    is_int(pw->pw_uid, uid, "Changing UID works");
    is_int(pw->pw_gid, gid, "Changing GID works");

    /* Clean up. */
    remctl_close(r);
    unlink(confpath);
    free(confpath);
    test_file_path_free(cmd);
    test_tmpdir_free(tmpdir);
    return 0;
}
