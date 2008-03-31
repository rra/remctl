/* $Id$
 *
 * tokens test suite.
 *
 * Written by Russ Allbery <rra@stanford.edu>
 * Copyright 2006, 2007 Board of Trustees, Leland Stanford Jr. University
 *
 * See LICENSE for licensing terms.
 */

#include <config.h>
#include <portable/system.h>
#include <portable/gssapi.h>
#include <portable/socket.h>

#include <fcntl.h>
#include <sys/stat.h>
#include <sys/time.h>
#include <sys/wait.h>

#include <tests/libtest.h>
#include <util/util.h>

/* A token for testing. */
static const char token[] = { 3, 0, 0, 0, 5, 'h', 'e', 'l', 'l', 'o' };


/*
 * Create a server socket, wait for a connection, and return the connected
 * socket.
 */
static int
create_server(void)
{
    int fd, conn, marker;
    struct sockaddr_in saddr;
    int on = 1;

    saddr.sin_family = AF_INET;
    saddr.sin_addr.s_addr = htonl(INADDR_LOOPBACK);
    saddr.sin_port = htons(14444);
    fd = socket(AF_INET, SOCK_STREAM, 0);
    if (fd < 0)
        sysdie("error creating socket");
    setsockopt(fd, SOL_SOCKET, SO_REUSEADDR, (char *) &on, sizeof(on));
    if (bind(fd, (struct sockaddr *) &saddr, sizeof(saddr)) < 0)
        sysdie("error binding socket");
    if (listen(fd, 1) < 0)
        sysdie("error listening on socket");
    marker = open("server-ready", O_CREAT | O_TRUNC, 0666);
    if (marker < 0)
        sysdie("cannot create marker file");
    conn = accept(fd, NULL, 0);
    if (conn < 0)
        sysdie("error accepting connection");
    return conn;
}


/*
 * Create a client socket, it for a connection, and return the connected
 * socket.
 */
static int
create_client(void)
{
    int fd;
    struct sockaddr_in saddr;
    struct timeval tv;

    saddr.sin_family = AF_INET;
    saddr.sin_addr.s_addr = htonl(INADDR_LOOPBACK);
    saddr.sin_port = htons(14444);
    fd = socket(AF_INET, SOCK_STREAM, 0);
    if (fd < 0)
        sysdie("error creating socket");
    alarm(1);
    while (access("server-ready", F_OK) != 0) {
        tv.tv_sec = 0;
        tv.tv_usec = 10000;
        select(0, NULL, NULL, NULL, &tv);
    }
    alarm(0);
    if (connect(fd, (struct sockaddr *) &saddr, sizeof(saddr)) < 0)
        sysdie("error connecting");
    return fd;
}


/*
 * Send a hand-constructed token to a file descriptor.
 */
static void
send_hand_token(int fd)
{
    xwrite(fd, token, sizeof(token));
}


/*
 * Send a token via token_send to a file descriptor.
 */
static void
send_regular_token(int fd)
{
    gss_buffer_desc buffer;

    buffer.value = xmalloc(5);
    memcpy(buffer.value, "hello", 5);
    buffer.length = 5;
    token_send(fd, 3, &buffer);
}


int
main(void)
{
    pid_t child;
    int server, client, status, flags;
    char buffer[20];
    ssize_t length;
    gss_buffer_desc result;

    alarm(2);

    test_init(10);

    unlink("server-ready");
    child = fork();
    if (child < 0)
        sysdie("cannot fork");
    else if (child == 0) {
        server = create_server();
        send_regular_token(server);
        exit(0);
    } else {
        client = create_client();
        length = read(client, buffer, 12);
        ok_int(1, 10, length);
        ok(2, memcmp(buffer, token, 10) == 0);
        waitpid(child, NULL, 0);
    }

    unlink("server-ready");
    child = fork();
    if (child < 0)
        sysdie("cannot fork");
    else if (child == 0) {
        server = create_server();
        send_hand_token(server);
        exit(0);
    } else {
        client = create_client();
        status = token_recv(client, &flags, &result, 5);
        ok_int(3, TOKEN_OK, status);
        ok_int(4, 3, flags);
        ok_int(5, 5, result.length);
        ok(6, memcmp(result.value, "hello", 5) == 0);
        waitpid(child, NULL, 0);
    }

    unlink("server-ready");
    child = fork();
    if (child < 0)
        sysdie("cannot fork");
    else if (child == 0) {
        server = create_server();
        xwrite(server, "\0\0\0\0\1", 5);
        exit(0);
    } else {
        client = create_client();
        status = token_recv(client, &flags, &result, 200);
        ok_int(7, TOKEN_FAIL_INVALID, status);
        waitpid(child, NULL, 0);
    }

    unlink("server-ready");
    child = fork();
    if (child < 0)
        sysdie("cannot fork");
    else if (child == 0) {
        server = create_server();
        send_hand_token(server);
        exit(0);
    } else {
        client = create_client();
        status = token_recv(client, &flags, &result, 4);
        ok_int(8, TOKEN_FAIL_LARGE, status);
        waitpid(child, NULL, 0);
    }

    unlink("server-ready");
    child = fork();
    if (child < 0)
        sysdie("cannot fork");
    else if (child == 0) {
        server = create_server();
        close(server);
        exit(0);
    } else {
        client = create_client();
        status = token_recv(client, &flags, &result, 4);
        ok_int(9, TOKEN_FAIL_EOF, status);
        waitpid(child, NULL, 0);
    }
    unlink("server-ready");

    /* Special test for error handling when sending tokens. */
    server = open("/dev/full", O_RDWR);
    if (server < 0)
        skip(10, "/dev/full not available");
    else {
        result.value = xmalloc(5);
        memcpy(result.value, "hello", 5);
        result.length = 5;
        status = token_send(server, 3, &result);
        free(result.value);
        ok_int(10, TOKEN_FAIL_SOCKET, status);
        close(server);
    }

    return 0;
}
