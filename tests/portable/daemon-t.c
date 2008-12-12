/*
 * daemon test suite.
 *
 * Written by Russ Allbery <rra@stanford.edu>
 * Copyright 2008 Board of Trustees, Leland Stanford Jr. University
 *
 * See LICENSE for licensing terms.
 */

#include <config.h>
#include <portable/system.h>

#include <errno.h>
#include <fcntl.h>
#ifdef HAVE_SYS_SELECT_H
# include <sys/select.h>
#endif
#include <sys/stat.h>
#include <sys/time.h>
#include <sys/wait.h>

#include <tests/libtest.h>
#include <util/util.h>

int test_daemon(int, int);


/*
 * Create the sentinel file, used by the child to indicate when it's done.
 */
static void
create_sentinel(void)
{
    int fd;

    fd = open("daemon-sentinel", O_RDWR | O_CREAT, 0666);
    close(fd);
}


/*
 * Wait for a sentinel file to be created.  Returns true if we saw it within
 * the expected length of time, and false otherwise.
 */
static int
wait_sentinel(void)
{
    int count = 20;
    int i;
    struct timeval tv;

    for (i = 0; i < count; i++) {
        if (access("daemon-sentinel", F_OK) == 0) {
            unlink("daemon-sentinel");
            return 1;
        }
        tv.tv_sec = 0;
        tv.tv_usec = 100000;
        select(0, NULL, NULL, NULL, &tv);
    }
    if (access("daemon-sentinel", F_OK) == 0) {
        unlink("daemon-sentinel");
        return 1;
    }
    return 0;
}


int
main(void)
{
    int fd, status;
    pid_t child;
    char start[BUFSIZ], dir[BUFSIZ];

    test_init(9);

    /* Get the current working directory. */
    if (getcwd(start, sizeof(start)) == NULL)
        sysdie("cannot get current working directory");

    /* First, some basic tests. */
    child = fork();
    if (child < 0)
        sysdie("cannot fork");
    else if (child == 0) {
        ok_int(1, 0, daemon(1, 1));
        fd = open("/dev/tty", O_RDONLY);
        ok(2, fd < 0);
        if (getcwd(dir, sizeof(dir)) == NULL)
            ok(3, 0);
        else
            ok_string(3, start, dir);
        create_sentinel();
        exit(42);
    } else {
        if (waitpid(child, &status, 0) < 0)
            sysdie("cannot wait for child");
        ok(4, wait_sentinel());
        ok_int(5, 0, status);
    }

    /* Test chdir. */
    child = fork();
    if (child < 0)
        sysdie("cannot fork");
    else if (child == 0) {
        ok_int(6, 0, daemon(0, 1));
        if (getcwd(dir, sizeof(dir)) == NULL)
            ok(7, 0);
        else
            ok_string(7, "/", dir);
        if (chdir(start) != 0)
            sysdie("cannot chdir to %s", start);
        create_sentinel();
        exit(0);
    } else {
        if (waitpid(child, &status, 0) < 0)
            sysdie("cannot wait for child");
        ok(8, wait_sentinel());
    }

    /* Test close. */
    child = fork();
    if (child < 0)
        sysdie("cannot fork");
    else if (child == 0) {
        daemon(0, 0);
        if (chdir(start) != 0)
            sysdie("cannot chdir to %s", start);
        fprintf(stdout, "not ok 9\n");
        create_sentinel();
        exit(0);
    } else {
        if (waitpid(child, &status, 0) < 0)
            sysdie("cannot wait for child");
        ok(9, wait_sentinel());
    }

    return 0;
}
