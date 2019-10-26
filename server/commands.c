/*
 * Running commands.
 *
 * These are the functions for running external commands under remctld and
 * calling the appropriate protocol functions to deal with the output.
 *
 * Written by Russ Allbery <eagle@eyrie.org>
 * Based on work by Anton Ushakov
 * Copyright 2015-2016, 2019 Russ Allbery <eagle@eyrie.org>
 * Copyright 2016 Dropbox, Inc.
 * Copyright 2002-2010, 2012-2014
 *     The Board of Trustees of the Leland Stanford Junior University
 *
 * SPDX-License-Identifier: MIT
 */

#include <config.h>
#include <portable/event.h>
#include <portable/system.h>
#include <portable/uio.h>

#include <fcntl.h>
#include <grp.h>
#include <sys/wait.h>

#include <server/internal.h>
#include <util/fdflag.h>
#include <util/macros.h>
#include <util/messages.h>
#include <util/protocol.h>
#include <util/xmalloc.h>


/*
 * Given a configuration rule, a command, and a subcommand, return true if
 * that command and subcommand match that configuration rule and false
 * otherwise.  Handles the ALL and NULL special cases.
 *
 * Empty commands are not yet supported by the rest of the code, but this
 * function copes in case that changes later.
 */
static bool
line_matches(struct rule *rule, const char *command, const char *subcommand)
{
    bool okay = false;

    if (strcmp(rule->command, "ALL") == 0)
        okay = true;
    if (command != NULL && strcmp(rule->command, command) == 0)
        okay = true;
    if (command == NULL && strcmp(rule->command, "EMPTY") == 0)
        okay = true;
    if (okay) {
        if (strcmp(rule->subcommand, "ALL") == 0)
            return true;
        if (subcommand != NULL && strcmp(rule->subcommand, subcommand) == 0)
            return true;
        if (subcommand == NULL && strcmp(rule->subcommand, "EMPTY") == 0)
            return true;
    }
    return false;
}


/*
 * Look up the matching configuration rule for a command and subcommand.
 * Takes the configuration, a command, and a subcommand to match against
 * Returns the matching config line or NULL if none match.
 */
static struct rule *
find_config_line(struct config *config, char *command, char *subcommand)
{
    size_t i;

    for (i = 0; i < config->count; i++)
        if (line_matches(config->rules[i], command, subcommand))
            return config->rules[i];
    return NULL;
}


/*
 * Find the summary of all commands the user can run against this remctl
 * server.  We do so by checking all configuration lines for any that
 * provide a summary setup that the user can access, then running that
 * line's command with the given summary sub-command.
 *
 * Takes a client object, the user requesting access, and the list of all
 * valid configurations.
 */
static void
server_send_summary(struct client *client, struct config *config)
{
    char *path = NULL;
    char *program;
    const char *subcommand;
    struct rule *rule = NULL;
    size_t i;
    const char **req_argv = NULL;
    bool ok_any = false;
    int status_all = 0;
    struct process process;
    struct evbuffer *output = NULL;

    /* Create a buffer to hold all the output for protocol version one. */
    if (client->protocol == 1) {
        output = evbuffer_new();
        if (output == NULL)
            die("internal error: cannot create output buffer");
    }

    /*
     * Check each line in the config to find any that are "<command> ALL"
     * lines, the user is authorized to run, and which have a summary field
     * given.
     */
    for (i = 0; i < config->count; i++) {
        memset(&process, 0, sizeof(process));
        process.client = client;
        rule = config->rules[i];
        if (!server_config_acl_permit(rule, client))
            continue;
        if (rule->summary == NULL)
            continue;
        ok_any = true;

        /*
         * Get the real program name, and use it as the first argument in
         * argv passed to the command.  Then add the summary command to the
         * argv.  If a subcommand is also specified, add that as an argument
         * after the summary command.
         */
        path = rule->program;
        req_argv = xcalloc(4, sizeof(char *));
        program = strrchr(path, '/');
        if (program == NULL)
            program = path;
        else
            program++;
        req_argv[0] = program;
        req_argv[1] = rule->summary;
        subcommand = rule->subcommand;
        if (strcmp(subcommand, "ALL") == 0 || strcmp(subcommand, "EMPTY") == 0)
            req_argv[2] = NULL;
        else
            req_argv[2] = subcommand;
        req_argv[3] = NULL;

        /* Pass the command off to be executed. */
        process.command = rule->summary;
        process.argv = req_argv;
        process.rule = rule;
        if (server_process_run(&process)) {
            if (client->protocol == 1)
                if (evbuffer_add_buffer(output, process.output) < 0)
                    die("internal error: cannot copy data from output buffer");
            if (process.status != 0)
                status_all = process.status;
        }
        free(req_argv);
    }

    /*
     * Sets the last process status to 0 if all succeeded, or the last failed
     * exit status if any commands gave non-zero.  Return that we had output
     * successfully if any command gave it.
     */
    if (WIFEXITED(status_all))
        status_all = (int) WEXITSTATUS(process.status);
    else
        status_all = -1;
    if (ok_any)
        client->finish(client, output, status_all);
    else {
        notice("summary request from user %s, but no defined summaries",
               client->user);
        client->error(client, ERROR_UNKNOWN_COMMAND, "Unknown command");
    }
    if (output != NULL)
        evbuffer_free(output);
}


/*
 * Create the argv we will pass along to a program at a full command
 * request.  This will be created from the full command and arguments given
 * via the remctl client.
 *
 * Takes the command and optional sub-command to run, the config line for this
 * command, the process, and the existing argv from remctl client.  Returns
 * a newly-allocated argv array that the caller is responsible for freeing.
 */
static char **
create_argv_command(struct rule *rule, struct process *process,
                    struct iovec **argv)
{
    size_t count, i, j, stdin_arg;
    char **req_argv = NULL;
    const char *program;

    /* Get ready to assemble the argv of the command. */
    for (count = 0; argv[count] != NULL; count++)
        ;
    if (rule->sudo_user == NULL)
        req_argv = xcalloc(count + 1, sizeof(char *));
    else
        req_argv = xcalloc(count + 5, sizeof(char *));

    /*
     * Without sudo, get the real program name, and use it as the first
     * argument in argv passed to the command.  With sudo, use it as the first
     * argument.  Then build the rest of the argv for the command, splicing
     * out the argument we're passing on stdin (if any).
     */
    if (rule->sudo_user != NULL) {
        req_argv[0] = xstrdup(PATH_SUDO);
        req_argv[1] = xstrdup("-u");
        req_argv[2] = xstrdup(rule->sudo_user);
        req_argv[3] = xstrdup("--");
        req_argv[4] = xstrdup(rule->program);
        j = 5;
    } else {
        program = strrchr(rule->program, '/');
        if (program == NULL)
            program = rule->program;
        else
            program++;
        req_argv[0] = xstrdup(program);
        j = 1;
    }
    if (rule->stdin_arg == -1)
        stdin_arg = count - 1;
    else
        stdin_arg = (size_t) rule->stdin_arg;
    for (i = 1; i < count; i++) {
        const char *data = argv[i]->iov_base;
        size_t length = argv[i]->iov_len;

        if (i == stdin_arg) {
            process->input = evbuffer_new();
            if (process->input == NULL)
                die("internal error: cannot create input buffer");
            if (evbuffer_add(process->input, data, length) < 0)
                die("internal error: cannot add data to input buffer");
            continue;
        }
        if (length == 0)
            req_argv[j] = xstrdup("");
        else
            req_argv[j] = xstrndup(data, length);
        j++;
    }
    req_argv[j] = NULL;
    return req_argv;
}


/*
 * Create the argv we will pass along to a program in response to a help
 * request.  This is fairly simple, created off of the specific command
 * we want help with, along with any sub-command given for specific help.
 *
 * Takes the path of the program to run and the command and optional
 * sub-command to run.  Returns a newly allocated argv array that the caller
 * is responsible for freeing.
 */
static char **
create_argv_help(const char *path, const char *command, const char *subcommand)
{
    char **req_argv = NULL;
    const char *program;

    if (subcommand == NULL)
        req_argv = xcalloc(3, sizeof(char *));
    else
        req_argv = xcalloc(4, sizeof(char *));

    /* The argv to pass along for a help command is very simple. */
    program = strrchr(path, '/');
    if (program == NULL)
        program = path;
    else
        program++;
    req_argv[0] = xstrdup(program);
    req_argv[1] = xstrdup(command);
    if (subcommand == NULL)
        req_argv[2] = NULL;
    else {
        req_argv[2] = xstrdup(subcommand);
        req_argv[3] = NULL;
    }
    return req_argv;
}


/*
 * Process an incoming command.  Check the configuration files and the ACL
 * file, and if appropriate, forks off the command.  Takes the argument vector
 * and the user principal, and a buffer into which to put the output from the
 * executable or any error message.  Returns the exit status of the command.
 *
 * Using the command and the subcommand, the following argument, a lookup in
 * the configuration data structure is done to find the command executable and
 * ACL file.  If the configuration contains an entry for this command with
 * subcommand equal to "ALL", that is a wildcard match for any given
 * subcommand.  The first argument is then replaced with the actual program
 * name to be executed.
 */
int
server_run_command(struct client *client, struct config *config,
                   struct iovec **argv)
{
    char *command = NULL;
    char *subcommand = NULL;
    char *helpsubcommand = NULL;
    struct rule *rule = NULL;
    char **req_argv = NULL;
    size_t i;
    int status = -1;
    bool ok = false;
    bool help = false;
    const char *user = client->user;
    struct process process;

    /* Start with an empty process. */
    memset(&process, 0, sizeof(process));
    process.client = client;

    /*
     * We need at least one argument.  This is also rejected earlier when
     * parsing the command and checking argc, but may as well be sure.
     */
    if (argv[0] == NULL) {
        notice("empty command from user %s", user);
        client->error(client, ERROR_BAD_COMMAND, "Invalid command token");
        goto done;
    }

    /* Neither the command nor the subcommand may ever contain nuls. */
    for (i = 0; i < 2 && argv[i] != NULL; i++) {
        if (memchr(argv[i]->iov_base, '\0', argv[i]->iov_len)) {
            notice("%s from user %s contains nul octet",
                   (i == 0) ? "command" : "subcommand", user);
            client->error(client, ERROR_BAD_COMMAND, "Invalid command token");
            goto done;
        }
    }

    /* We need the command and subcommand as nul-terminated strings. */
    command = xstrndup(argv[0]->iov_base, argv[0]->iov_len);
    if (argv[1] != NULL)
        subcommand = xstrndup(argv[1]->iov_base, argv[1]->iov_len);

    /*
     * Find the program path we need to run.  If we find no matching command
     * at first and the command is a help command, then we either dispatch
     * to the summary command if no specific help was requested, or if a
     * specific help command was listed, check for that in the configuration
     * instead.
     */
    rule = find_config_line(config, command, subcommand);
    if (rule == NULL && strcmp(command, "help") == 0) {

        /* Error if we have more than a command and possible subcommand. */
        if (argv[1] != NULL && argv[2] != NULL && argv[3] != NULL) {
            notice("help command from user %s has more than three arguments",
                   user);
            client->error(client, ERROR_TOOMANY_ARGS,
                          "Too many arguments for help command");
        }

        if (subcommand == NULL) {
            server_send_summary(client, config);
            goto done;
        } else {
            help = true;
            if (argv[2] != NULL)
                helpsubcommand = xstrndup(argv[2]->iov_base,
                                          argv[2]->iov_len);
            rule = find_config_line(config, subcommand, helpsubcommand);
        }
    }

    /*
     * Arguments may only contain nuls if they're the argument being passed on
     * standard input.
     */
    for (i = 1; argv[i] != NULL; i++) {
        if (rule != NULL) {
            if (help == false && (long) i == rule->stdin_arg)
                continue;
            if (argv[i + 1] == NULL && rule->stdin_arg == -1)
                continue;
        }
        if (memchr(argv[i]->iov_base, '\0', argv[i]->iov_len)) {
            notice("argument %lu from user %s contains nul octet",
                   (unsigned long) i, user);
            client->error(client, ERROR_BAD_COMMAND, "Invalid command token");
            goto done;
        }
    }

    /* Log after we look for command so we can get potentially get logmask. */
    server_log_command(argv, rule, user);

    /*
     * Check the command, aclfile, and the authorization of this client to
     * run this command.
     */
    if (rule == NULL) {
        notice("unknown command %s%s%s from user %s", command,
               (subcommand == NULL) ? "" : " ",
               (subcommand == NULL) ? "" : subcommand, user);
        client->error(client, ERROR_UNKNOWN_COMMAND, "Unknown command");
        goto done;
    }
    if (!server_config_acl_permit(rule, client)) {
        notice("access denied: user %s, command %s%s%s", user, command,
               (subcommand == NULL) ? "" : " ",
               (subcommand == NULL) ? "" : subcommand);
        client->error(client, ERROR_ACCESS, "Access denied");
        goto done;
    }

    /*
     * Check for a specific command help request with the rule and do error
     * checking and arg massaging.
     */
    if (help) {
        if (rule->help == NULL) {
            notice("command %s from user %s has no defined help",
                   command, user);
            client->error(client, ERROR_NO_HELP,
                          "No help defined for command");
            goto done;
        } else {
            free(subcommand);
            subcommand = xstrdup(rule->help);
        }
        req_argv = create_argv_help(rule->program, subcommand, helpsubcommand);
    } else {
        req_argv = create_argv_command(rule, &process, argv);
    }

    /* Now actually execute the program. */
    process.command = command;
    process.argv = (const char **) req_argv;
    process.rule = rule;
    ok = server_process_run(&process);
    if (ok) {
        if (WIFEXITED(process.status))
            process.status = (signed int) WEXITSTATUS(process.status);
        else
            process.status = -1;
        client->finish(client, process.output, process.status);
    }
    status = process.status;

 done:
    free(command);
    free(subcommand);
    free(helpsubcommand);
    if (req_argv != NULL) {
        for (i = 0; req_argv[i] != NULL; i++)
            free(req_argv[i]);
        free(req_argv);
    }
    if (process.input != NULL)
        evbuffer_free(process.input);
    if (process.output != NULL)
        evbuffer_free(process.output);
    return status;
}


/*
 * Free a command, represented as a NULL-terminated array of pointers to iovec
 * structs.
 */
void
server_free_command(struct iovec **command)
{
    struct iovec **arg;

    for (arg = command; *arg != NULL; arg++) {
        if ((*arg)->iov_base != NULL)
            free((*arg)->iov_base);
        free(*arg);
    }
    free(command);
}
