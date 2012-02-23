/*
 * tokens test suite.
 *
 * Written by Russ Allbery <rra@stanford.edu>
 * Copyright 2006, 2007, 2009, 2010, 2012
 *     The Board of Trustees of the Leland Stanford Junior University
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

#include <tests/tap/basic.h>
#include <util/tokens.h>
#include <util/xwrite.h>

/*
 * Windows requires a different function when sending to sockets, but can't
 * return short writes on blocking sockets.
 */
#ifdef _WIN32
# define socket_xwrite(fd, b, s)        send((fd), (b), (s), 0)
#else
# define socket_xwrite(fd, b, s)        xwrite((fd), (b), (s))
#endif

/* A token for testing. */
static const char token[] = { 3, 0, 0, 0, 5, 'h', 'e', 'l', 'l', 'o' };


/*
 * Create a server socket, wait for a connection, and return the connected
 * socket.
 */
static socket_type
create_server(void)
{
    socket_type fd, conn;
    int marker;
    struct sockaddr_in saddr;
    int on = 1;
    const void *onaddr = &on;

    saddr.sin_family = AF_INET;
    saddr.sin_addr.s_addr = htonl(INADDR_LOOPBACK);
    saddr.sin_port = htons(14373);
    fd = socket(AF_INET, SOCK_STREAM, 0);
    if (fd == INVALID_SOCKET)
        sysbail("error creating socket");
    setsockopt(fd, SOL_SOCKET, SO_REUSEADDR, onaddr, sizeof(on));
    if (bind(fd, (struct sockaddr *) &saddr, sizeof(saddr)) < 0)
        sysbail("error binding socket");
    if (listen(fd, 1) < 0)
        sysbail("error listening on socket");
    marker = open("server-ready", O_CREAT | O_TRUNC, 0666);
    if (marker < 0)
        sysbail("cannot create marker file");
    conn = accept(fd, NULL, 0);
    if (conn == INVALID_SOCKET)
        sysbail("error accepting connection");
    return conn;
}


/*
 * Create a client socket, it for a connection, and return the connected
 * socket.
 */
static socket_type
create_client(void)
{
    socket_type fd;
    struct sockaddr_in saddr;
    struct timeval tv;

    saddr.sin_family = AF_INET;
    saddr.sin_addr.s_addr = htonl(INADDR_LOOPBACK);
    saddr.sin_port = htons(14373);
    fd = socket(AF_INET, SOCK_STREAM, 0);
    if (fd == INVALID_SOCKET)
        sysbail("error creating socket");
    alarm(1);
    while (access("server-ready", F_OK) != 0) {
        tv.tv_sec = 0;
        tv.tv_usec = 10000;
        select(0, NULL, NULL, NULL, &tv);
    }
    alarm(0);
    if (connect(fd, (struct sockaddr *) &saddr, sizeof(saddr)) < 0)
        sysbail("error connecting");
    return fd;
}


/*
 * Send a hand-constructed token to a file descriptor.
 */
static void
send_hand_token(socket_type fd)
{
    socket_xwrite(fd, token, sizeof(token));
}


/*
 * Send a token via token_send to a file descriptor.
 */
static void
send_regular_token(socket_type fd)
{
    gss_buffer_desc buffer;

    buffer.value = bmalloc(5);
    memcpy(buffer.value, "hello", 5);
    buffer.length = 5;
    token_send(fd, 3, &buffer, 0);
}


int
main(void)
{
    pid_t child;
    socket_type server, client;
    int status, flags;
    char buffer[20];
    ssize_t length;
    gss_buffer_desc result;

    alarm(20);

    plan(12);
    if (chdir(getenv("BUILD")) < 0)
        sysbail("can't chdir to BUILD");

    unlink("server-ready");
    child = fork();
    if (child < 0)
        sysbail("cannot fork");
    else if (child == 0) {
        server = create_server();
        send_regular_token(server);
        exit(0);
    } else {
        client = create_client();
        length = read(client, buffer, 12);
        is_int(10, length, "received token has correct length");
        ok(memcmp(buffer, token, 10) == 0, "...and correct data");
        waitpid(child, NULL, 0);
    }

    unlink("server-ready");
    child = fork();
    if (child < 0)
        sysbail("cannot fork");
    else if (child == 0) {
        server = create_server();
        send_hand_token(server);
        exit(0);
    } else {
        client = create_client();
        status = token_recv(client, &flags, &result, 5, 0);
        is_int(TOKEN_OK, status, "received hand-rolled token");
        is_int(3, flags, "...with right flags");
        is_int(5, result.length, "...and right length");
        ok(memcmp(result.value, "hello", 5) == 0, "...and right data");
        waitpid(child, NULL, 0);
    }

    /* Send a token with a length of one, but no following data. */
    unlink("server-ready");
    child = fork();
    if (child < 0)
        sysbail("cannot fork");
    else if (child == 0) {
        server = create_server();
        socket_xwrite(server, "\0\0\0\0\1", 5);
        exit(0);
    } else {
        client = create_client();
        status = token_recv(client, &flags, &result, 200, 0);
        is_int(TOKEN_FAIL_EOF, status, "receive invalid token");
        waitpid(child, NULL, 0);
    }

    /* Send a token larger than our token size limit. */
    unlink("server-ready");
    child = fork();
    if (child < 0)
        sysbail("cannot fork");
    else if (child == 0) {
        server = create_server();
        send_hand_token(server);
        exit(0);
    } else {
        client = create_client();
        status = token_recv(client, &flags, &result, 4, 0);
        is_int(TOKEN_FAIL_LARGE, status, "receive too-large token");
        waitpid(child, NULL, 0);
    }

    /* Send EOF when we were expecting a token. */
    unlink("server-ready");
    child = fork();
    if (child < 0)
        sysbail("cannot fork");
    else if (child == 0) {
        server = create_server();
        close(server);
        exit(0);
    } else {
        client = create_client();
        status = token_recv(client, &flags, &result, 4, 0);
        is_int(TOKEN_FAIL_EOF, status, "receive end of file");
        waitpid(child, NULL, 0);
    }

    /*
     * Test a timeout on sending a token.  We have to send a large enough
     * token that the network layer doesn't just buffer it.
     */
    unlink("server-ready");
    child = fork();
    if (child < 0)
        sysbail("cannot fork");
    else if (child == 0) {
        server = create_server();
        sleep(3);
        exit(0);
    } else {
        result.value = bmalloc(512 * 1024);
        memset(result.value, 'a', 512 * 1024);
        result.length = 512 * 1024;
        client = create_client();
        status = token_send(client, 3, &result, 1);
        free(result.value);
        is_int(TOKEN_FAIL_TIMEOUT, status, "can't send due to timeout");
        close(client);
        waitpid(child, NULL, 0);
    }

    /* Test a timeout on receiving a token. */
    unlink("server-ready");
    child = fork();
    if (child < 0)
        sysbail("cannot fork");
    else if (child == 0) {
        server = create_server();
        sleep(3);
        exit(0);
    } else {
        client = create_client();
        status = token_recv(client, &flags, &result, 200, 1);
        is_int(TOKEN_FAIL_TIMEOUT, status, "can't receive due to timeout");
        close(client);
        waitpid(child, NULL, 0);
    }

    /* Special test for error handling when sending tokens. */
    server = open("/dev/full", O_RDWR);
    if (server < 0)
        skip("/dev/full not available");
    else {
        result.value = bmalloc(5);
        memcpy(result.value, "hello", 5);
        result.length = 5;
        status = token_send(server, 3, &result, 0);
        free(result.value);
        is_int(TOKEN_FAIL_SOCKET, status, "can't send due to system error");
        close(server);
    }

    unlink("server-ready");
    return 0;
}
