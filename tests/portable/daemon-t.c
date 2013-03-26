/*
 * daemon test suite.
 *
 * The canonical version of this file is maintained in the rra-c-util package,
 * which can be found at <http://www.eyrie.org/~eagle/software/rra-c-util/>.
 *
 * Written by Russ Allbery <rra@stanford.edu>
 *
 * The authors hereby relinquish any claim to any copyright that they may have
 * in this work, whether granted under contract or by operation of law or
 * international treaty, and hereby commit to the public, at large, that they
 * shall not, at any time in the future, seek to enforce any copyright in this
 * work against any person or entity, or prevent any person or entity from
 * copying, publishing, distributing or creating derivative works of this
 * work.
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

#include <tests/tap/basic.h>

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

    plan(9);

    /* Get the current working directory. */
    if (getcwd(start, sizeof(start)) == NULL)
        bail("cannot get current working directory");

    /* First, some basic tests. */
    child = fork();
    if (child < 0)
        sysbail("cannot fork");
    else if (child == 0) {
        is_int(0, test_daemon(1, 1), "daemon(1, 1)");
        fd = open("/dev/tty", O_RDONLY);
        ok(fd < 0, "...no tty");
        is_string(start, getcwd(dir, sizeof(dir)), "...in same directory");
        create_sentinel();
        exit(42);
    } else {
        if (waitpid(child, &status, 0) < 0)
            bail("cannot wait for child: %s", strerror(errno));
        testnum += 3;
        ok(wait_sentinel(), "...child exited");
        is_int(0, status, "...successfully");
    }

    /* Test chdir. */
    child = fork();
    if (child < 0)
        sysbail("cannot fork");
    else if (child == 0) {
        is_int(0, test_daemon(0, 1), "daemon(0, 1)");
        is_string("/", getcwd(dir, sizeof(dir)), "...now in /");
        if (chdir(start) != 0)
            sysbail("cannot chdir to %s", start);
        create_sentinel();
        exit(0);
    } else {
        if (waitpid(child, &status, 0) < 0)
            sysbail("cannot wait for child");
        testnum += 2;
        ok(wait_sentinel(), "...child exited");
    }

    /* Test close. */
    child = fork();
    if (child < 0)
        sysbail("cannot fork");
    else if (child == 0) {
        if (test_daemon(0, 0) != 0)
            sysbail("daemon failed");
        if (chdir(start) != 0)
            sysbail("cannot chdir to %s", start);
        ok(0, "output from child that should be hidden");
        create_sentinel();
        exit(0);
    } else {
        if (waitpid(child, &status, 0) < 0)
            sysbail("cannot wait for child");
        ok(wait_sentinel(), "daemon(0, 0)");
    }

    return 0;
}
