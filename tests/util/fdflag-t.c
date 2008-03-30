/* $Id$ */
/* fdflag test suite. */

/* Written by Russ Allbery <rra@stanford.edu>
   Copyright 2008 Board of Trustees, Leland Stanford Jr. University
   See README for licensing terms. */

#include <config.h>
#include <system.h>
#include <portable/socket.h>

#include <errno.h>
#include <sys/wait.h>

#include <tests/libtest.h>
#include <util/util.h>

int
main(void)
{
    int master, data, out1, out2;
    socklen_t size;
    ssize_t status;
    struct sockaddr_in sin;
    pid_t child;
    char buffer[] = "D";

    test_init(8);

    /* Parent will create the socket first to get the port number. */
    memset(&sin, '\0', sizeof(sin));
    sin.sin_family = AF_INET;
    master = socket(AF_INET, SOCK_STREAM, 0);
    if (master == -1)
        sysdie("socket creation failed");
    if (bind(master, (struct sockaddr *) &sin, sizeof(sin)) < 0)
        sysdie("bind failed");
    size = sizeof(sin);
    if (getsockname(master, (struct sockaddr *) &sin, &size) < 0)
        sysdie("getsockname failed");
    if (listen(master, 1) < 0)
        sysdie("listen failed");

    /* Duplicate standard output to test close-on-exec. */
    out1 = 8;
    out2 = 9;
    if (dup2(fileno(stdout), out1) < 0)
        sysdie("cannot dup stdout to fd 8");
    if (dup2(fileno(stdout), out2) < 0)
        sysdie("cannot dup stdout to fd 9");
    ok(1, fdflag_close_exec(out1, true));
    ok(2, fdflag_close_exec(out2, true));
    ok(3, fdflag_close_exec(out2, false));

    /* Fork, child closes the open socket and then tries to connect, parent
       calls listen() and accept() on it.  Parent will then set the socket
       non-blocking and try to read from it to see what happens, then write
       to the socket and close it, triggering the child close and exit.

       Before the child exits, it will exec a shell that will print "no" to
       the duplicate of stdout that the parent created and then the ok to
       regular stdout. */
    child = fork();
    if (child < 0) {
        sysdie("fork failed");
    } else if (child != 0) {
        size = sizeof(sin);
        data = accept(master, (struct sockaddr *) &sin, &size);
        close(master);
        if (data < 0)
            sysdie("accept failed");
        ok(4, fdflag_nonblocking(data, true));
        status = read(data, buffer, sizeof(buffer));
        ok_int(5, -1, status);
        ok_int(6, EAGAIN, errno);
        write(data, buffer, sizeof(buffer));
        close(data);
    } else {
        data = socket(AF_INET, SOCK_STREAM, 0);
        if (data < 0)
            sysdie("child socket failed");
        if (connect(data, (struct sockaddr *) &sin, sizeof(sin)) < 0)
            sysdie("child connect failed");
        read(data, buffer, sizeof(buffer));
        fclose(stderr);
        execlp("sh", "sh", "-c",
               "printf 'not ' >&8; echo ok 7; echo 'ok 8' >&9", (char *) 0);
        sysdie("exec failed");
    }
    waitpid(child, NULL, 0);
    exit(0);
}
