/*
 * Running commands.
 *
 * These are the functions for running external commands under remctld and
 * calling the appropriate protocol functions to deal with the output.
 *
 * Written by Russ Allbery <eagle@eyrie.org>
 * Based on work by Anton Ushakov
 * Copyright 2002, 2003, 2004, 2005, 2006, 2007, 2008, 2009, 2010, 2012, 2013,
 *     2014 The Board of Trustees of the Leland Stanford Junior University
 *
 * See LICENSE for licensing terms.
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
 * Given a configuration line, a command, and a subcommand, return true if
 * that command and subcommand match that configuration line and false
 * otherwise.  Handles the ALL and NULL special cases.
 *
 * Empty commands are not yet supported by the rest of the code, but this
 * function copes in case that changes later.
 */
static bool
line_matches(struct confline *cline, const char *command,
             const char *subcommand)
{
    bool okay = false;

    if (strcmp(cline->command, "ALL") == 0)
        okay = true;
    if (command != NULL && strcmp(cline->command, command) == 0)
        okay = true;
    if (command == NULL && strcmp(cline->command, "EMPTY") == 0)
        okay = true;
    if (okay) {
        if (strcmp(cline->subcommand, "ALL") == 0)
            return true;
        if (subcommand != NULL && strcmp(cline->subcommand, subcommand) == 0)
            return true;
        if (subcommand == NULL && strcmp(cline->subcommand, "EMPTY") == 0)
            return true;
    }
    return false;
}


/*
 * Look up the matching configuration line for a command and subcommand.
 * Takes the configuration, a command, and a subcommand to match against
 * Returns the matching config line or NULL if none match.
 */
static struct confline *
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
server_send_summary(struct client *client, const char *user,
                    struct config *config)
{
    char *path = NULL;
    char *program;
    struct confline *cline = NULL;
    size_t i;
    char **req_argv = NULL;
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
        cline = config->rules[i];
        if (strcmp(cline->subcommand, "ALL") != 0)
            continue;
        if (!server_config_acl_permit(cline, user))
            continue;
        if (cline->summary == NULL)
            continue;
        ok_any = true;

        /*
         * Get the real program name, and use it as the first argument in
         * argv passed to the command.  Then add the summary command to the
         * argv and pass off to be executed.
         */
        path = cline->program;
        req_argv = xmalloc(3 * sizeof(char *));
        program = strrchr(path, '/');
        if (program == NULL)
            program = path;
        else
            program++;
        req_argv[0] = program;
        req_argv[1] = cline->summary;
        req_argv[2] = NULL;
        process.command = cline->summary;
        process.argv = req_argv;
        process.config = cline;
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
    if (ok_any) {
        if (client->protocol == 1)
            server_v1_send_output(client, output, status_all);
        else
            server_v2_send_status(client, status_all);
    } else {
        notice("summary request from user %s, but no defined summaries",
               user);
        server_send_error(client, ERROR_UNKNOWN_COMMAND, "Unknown command");
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
create_argv_command(struct confline *cline, struct process *process,
                    struct iovec **argv)
{
    size_t count, i, j, stdin_arg;
    char **req_argv = NULL;
    const char *program;

    /* Get ready to assemble the argv of the command. */
    for (count = 0; argv[count] != NULL; count++)
        ;
    req_argv = xmalloc((count + 1) * sizeof(char *));

    /*
     * Get the real program name, and use it as the first argument in argv
     * passed to the command.  Then build the rest of the argv for the
     * command, splicing out the argument we're passing on stdin (if any).
     */
    program = strrchr(cline->program, '/');
    if (program == NULL)
        program = cline->program;
    else
        program++;
    req_argv[0] = xstrdup(program);
    if (cline->stdin_arg == -1)
        stdin_arg = count - 1;
    else
        stdin_arg = (size_t) cline->stdin_arg;
    for (i = 1, j = 1; i < count; i++) {
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
        req_argv = xmalloc(3 * sizeof(char *));
    else
        req_argv = xmalloc(4 * sizeof(char *));

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
 * executable or any error message.  Returns 0 on success and a negative
 * integer on failure.
 *
 * Using the command and the subcommand, the following argument, a lookup in
 * the conf data structure is done to find the command executable and acl
 * file.  If the conf file, and subsequently the conf data structure contains
 * an entry for this command with subcommand equal to "ALL", that is a
 * wildcard match for any given subcommand.  The first argument is then
 * replaced with the actual program name to be executed.
 *
 * After checking the acl permissions, the process forks and the child execv's
 * the command with pipes arranged to gather output. The parent waits for the
 * return code and gathers stdout and stderr pipes.
 */
void
server_run_command(struct client *client, struct config *config,
                   struct iovec **argv)
{
    char *command = NULL;
    char *subcommand = NULL;
    char *helpsubcommand = NULL;
    struct confline *cline = NULL;
    char **req_argv = NULL;
    size_t i;
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
        server_send_error(client, ERROR_BAD_COMMAND, "Invalid command token");
        goto done;
    }

    /* Neither the command nor the subcommand may ever contain nuls. */
    for (i = 0; argv[i] != NULL && i < 2; i++) {
        if (memchr(argv[i]->iov_base, '\0', argv[i]->iov_len)) {
            notice("%s from user %s contains nul octet",
                   (i == 0) ? "command" : "subcommand", user);
            server_send_error(client, ERROR_BAD_COMMAND,
                              "Invalid command token");
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
    cline = find_config_line(config, command, subcommand);
    if (cline == NULL && strcmp(command, "help") == 0) {

        /* Error if we have more than a command and possible subcommand. */
        if (argv[1] != NULL && argv[2] != NULL && argv[3] != NULL) {
            notice("help command from user %s has more than three arguments",
                   user);
            server_send_error(client, ERROR_TOOMANY_ARGS,
                              "Too many arguments for help command");
        }

        if (subcommand == NULL) {
            server_send_summary(client, user, config);
            goto done;
        } else {
            help = true;
            if (argv[2] != NULL)
                helpsubcommand = xstrndup(argv[2]->iov_base,
                                          argv[2]->iov_len);
            cline = find_config_line(config, subcommand, helpsubcommand);
        }
    }

    /*
     * Arguments may only contain nuls if they're the argument being passed on
     * standard input.
     */
    for (i = 1; argv[i] != NULL; i++) {
        if (cline != NULL) {
            if (help == false && (long) i == cline->stdin_arg)
                continue;
            if (argv[i + 1] == NULL && cline->stdin_arg == -1)
                continue;
        }
        if (memchr(argv[i]->iov_base, '\0', argv[i]->iov_len)) {
            notice("argument %lu from user %s contains nul octet",
                   (unsigned long) i, user);
            server_send_error(client, ERROR_BAD_COMMAND,
                              "Invalid command token");
            goto done;
        }
    }

    /* Log after we look for command so we can get potentially get logmask. */
    server_log_command(argv, cline, user);

    /*
     * Check the command, aclfile, and the authorization of this client to
     * run this command.
     */
    if (cline == NULL) {
        notice("unknown command %s%s%s from user %s", command,
               (subcommand == NULL) ? "" : " ",
               (subcommand == NULL) ? "" : subcommand, user);
        server_send_error(client, ERROR_UNKNOWN_COMMAND, "Unknown command");
        goto done;
    }
    if (!server_config_acl_permit(cline, user)) {
        notice("access denied: user %s, command %s%s%s", user, command,
               (subcommand == NULL) ? "" : " ",
               (subcommand == NULL) ? "" : subcommand);
        server_send_error(client, ERROR_ACCESS, "Access denied");
        goto done;
    }

    /*
     * Check for a specific command help request with the cline and do error
     * checking and arg massaging.
     */
    if (help) {
        if (cline->help == NULL) {
            notice("command %s from user %s has no defined help",
                   command, user);
            server_send_error(client, ERROR_NO_HELP,
                              "No help defined for command");
            goto done;
        } else {
            free(subcommand);
            subcommand = xstrdup(cline->help);
        }
    }

    /* Assemble the argv for the command we're about to run. */
    if (help)
        req_argv = create_argv_help(cline->program, subcommand, helpsubcommand);
    else
        req_argv = create_argv_command(cline, &process, argv);

    /* Now actually execute the program. */
    process.command = command;
    process.argv = req_argv;
    process.config = cline;
    ok = server_process_run(&process);
    if (ok) {
        if (WIFEXITED(process.status))
            process.status = (signed int) WEXITSTATUS(process.status);
        else
            process.status = -1;
        if (client->protocol == 1)
            server_v1_send_output(client, process.output, process.status);
        else
            server_v2_send_status(client, process.status);
    }

 done:
    if (command != NULL)
        free(command);
    if (subcommand != NULL)
        free(subcommand);
    if (helpsubcommand != NULL)
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
