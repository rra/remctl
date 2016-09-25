/*
 * Test suite for running commands with sudo.
 *
 * Written by Russ Allbery <eagle@eyrie.org>
 * Copyright 2016 Dropbox, Inc.
 *
 * See LICENSE for licensing terms.
 */

#include <config.h>
#include <portable/event.h>
#include <portable/system.h>

#include <server/internal.h>
#include <tests/tap/basic.h>
#include <util/messages.h>


int
main(void)
{
    struct config *config;
    struct iovec **command;
    struct client *client;

    /* Suppress normal logging. */
    message_handlers_notice(0);

    /* Load the test configuration. */
    if (chdir(getenv("C_TAP_BUILD")) < 0)
        sysbail("can't chdir to C_TAP_BUILD");
    config = server_config_load("data/conf-simple");
    if (config == NULL)
        bail("server_config_load returned NULL");

    /* Create the command we're going to run. */
    command = server_ssh_parse_command("sudo foo bar stdin baz");
    putenv((char *) "REMCTL_USER=test@EXAMPLE.ORG");
    putenv((char *) "SSH_CONNECTION=127.0.0.1 34537 127.0.0.1 4373");
    client = server_ssh_new_client();

    /* Run the command. */
    server_run_command(client, config, command);

    /* Clean up. */
    server_free_command(command);
    server_ssh_free_client(client);
    server_config_free(config);
    libevent_global_shutdown();
    return 0;
}
