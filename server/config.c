/*
 * Configuration parsing.
 *
 * These are the functions for parsing the remctld configuration file and
 * checking access.
 *
 * Written by Russ Allbery <eagle@eyrie.org>
 * Based on work by Anton Ushakov
 * Copyright 2015, 2018 Russ Allbery <eagle@eyrie.org>
 * Copyright 2002-2012, 2014
 *     The Board of Trustees of the Leland Stanford Junior University
 * Copyright 2008 Carnegie Mellon University
 * Copyright 2014 IN2P3 Computing Centre - CNRS
 *
 * See LICENSE for licensing terms.
 */

#include <config.h>
#ifdef HAVE_KRB5
# include <portable/krb5.h>
#endif
#include <portable/system.h>

#include <ctype.h>
#include <dirent.h>
#include <errno.h>
#include <grp.h>
#ifdef HAVE_PCRE
# include <pcre.h>
#endif
#include <pwd.h>
#ifdef HAVE_REGCOMP
# include <regex.h>
#endif
#include <sys/stat.h>

#include <server/internal.h>
#include <util/macros.h>
#include <util/messages.h>
#ifdef HAVE_KRB5
# include <util/messages-krb5.h>
#endif
#include <util/vector.h>
#include <util/xmalloc.h>

/*
 * acl_gput_file is currently used only by the test suite to point GPUT at a
 * separate file for testing.  If it becomes available as a configurable
 * parameter, we'll want to do something other than a local static variable
 * for it.
 */
#ifdef HAVE_GPUT
# include <gput.h>
static char *acl_gput_file = NULL;
#endif

/* Maximum length allowed when converting a principal to a local name. */
#define REMCTL_KRB5_LOCALNAME_MAX_LEN \
    (sysconf(_SC_LOGIN_NAME_MAX) < 256 ? 256 : sysconf(_SC_LOGIN_NAME_MAX))

/* Return codes for configuration and ACL parsing. */
enum config_status {
    CONFIG_SUCCESS = 0,
    CONFIG_NOMATCH = -1,
    CONFIG_ERROR   = -2,
    CONFIG_DENY    = -3
};

/* Holds information about parsing configuration options. */
struct config_option {
    const char *name;
    enum config_status (*parse)(struct rule *, char *option, const char *file,
                                size_t lineno);
};

/* Holds information about ACL schemes */
struct acl_scheme {
    const char *name;
    enum config_status (*check)(const struct client *, const char *data,
                                const char *file, size_t lineno);
};

/*
 * The following must match the indexes of these schemes in schemes[].
 * They're used to implement default ACL schemes in particular contexts.
 */
#define ACL_SCHEME_FILE  0
#define ACL_SCHEME_PRINC 1

/* Forward declarations. */
static enum config_status acl_check(const struct client *, const char *entry,
                                    int def_index, const char *file,
                                    size_t lineno);

/*
 * Check a filename for acceptable characters.  Returns true if the file
 * consists solely of [a-zA-Z0-9_-] and false otherwise.
 */
static bool
valid_filename(const char *filename)
{
    const char *p;

    for (p = filename; *p != '\0'; p++) {
        if (*p >= 'A' && *p <= 'Z')
            continue;
        if (*p >= 'a' && *p <= 'z')
            continue;
        if (*p >= '0' && *p <= '9')
            continue;
        if (*p == '_' || *p == '-')
            continue;
        return false;
    }
    return true;
}


/*
 * Process a request for including a file, either for configuration or for
 * ACLs.  Called by read_conf_file and acl_check_file.
 *
 * Takes the file to include, the current file, the line number, the function
 * to call for each included file, and a piece of data to pass to that
 * function.  Handles including either files or directories.  When used for
 * processing ACL files named in the configuration file, the current file and
 * line number will be passed as zero.
 *
 * If the function returns a value less than -1, return its return code.  If
 * the file is recursively included or if there is an error in reading a file
 * or processing an include directory, return CONFIG_ERROR.  Otherwise, return
 * the greatest of all status codes returned by function, or CONFIG_NOMATCH if
 * the file was empty.
 */
static enum config_status
handle_include(const char *included, const char *file, size_t lineno,
               enum config_status (*function)(void *, const char *),
               void *data)
{
    struct stat st;

    /* Sanity checking. */
    if (strcmp(included, file) == 0) {
        warn("%s:%lu: %s recursively included", file, (unsigned long) lineno,
             file);
        return CONFIG_ERROR;
    }
    if (stat(included, &st) < 0) {
        syswarn("%s:%lu: included file %s not found", file,
                (unsigned long) lineno, included);
        return CONFIG_ERROR;
    }

    /*
     * If it's a directory, include everything in the directory whose
     * filenames contain only the allowed characters.  Otherwise, just include
     * the one file.
     */
    if (!S_ISDIR(st.st_mode)) {
        return (*function)(data, included);
    } else {
        DIR *dir;
        struct dirent *entry;
        int status = CONFIG_NOMATCH;
        int last;

        dir = opendir(included);
        if (dir == NULL) {
            syswarn("%s:%lu: included directory %s cannot be opened", file,
                    (unsigned long) lineno, included);
            return CONFIG_ERROR;
        }
        while ((entry = readdir(dir)) != NULL) {
            char *path;

            if (!valid_filename(entry->d_name))
                continue;
            xasprintf(&path, "%s/%s", included, entry->d_name);
            last = (*function)(data, path);
            free(path);
            if (last < -1) {
                closedir(dir);
                return last;
            }
            if (last > status)
                status = last;
        }
        closedir(dir);
        return status;
    }
}


/*
 * Check whether a given string is an option setting.  An option setting must
 * start with a letter and consists of one or more alphanumerics or hyphen (-)
 * followed by an equal sign (=) and at least one additional character.
 */
static bool
is_option(const char *option)
{
    const char *p;

    if (!isalpha((unsigned int) *option))
        return false;
    for (p = option; *p != '\0'; p++) {
        if (*p == '=' && p > option && p[1] != '\0')
            return true;
        if (!isalnum((unsigned int) *p) && *p != '-')
            return false;
    }
    return false;
}


/*
 * Convert a string to a long, validating that the number converts properly.
 * Returns true on success and false on failure.
 */
static bool
convert_number(const char *string, long *result)
{
    char *end;
    long arg;

    errno = 0;
    arg = strtol(string, &end, 10);
    if (errno != 0 || *end != '\0' || arg <= 0)
        return false;
    *result = arg;
    return true;
}


/*
 * Parse the logmask configuration option.  Verifies the listed argument
 * numbers, stores them in the configuration rule struct, and returns
 * CONFIG_SUCCESS on success and CONFIG_ERROR on error.
 */
static enum config_status
option_logmask(struct rule *rule, char *value, const char *name, size_t lineno)
{
    struct cvector *logmask;
    size_t i;
    long mask;

    logmask = cvector_split(value, ',', NULL);
    free(rule->logmask);
    rule->logmask = xcalloc(logmask->count + 1, sizeof(unsigned int));
    for (i = 0; i < logmask->count; i++) {
        if (!convert_number(logmask->strings[i], &mask) || mask < 0) {
            warn("%s:%lu: invalid logmask parameter %s", name,
                 (unsigned long) lineno, logmask->strings[i]);
            cvector_free(logmask);
            free(rule->logmask);
            rule->logmask = NULL;
            return CONFIG_ERROR;
        }
        rule->logmask[i] = (unsigned int) mask;
    }
    rule->logmask[i] = 0;
    cvector_free(logmask);
    return CONFIG_SUCCESS;
}


/*
 * Parse the stdin configuration option.  Verifies the argument number or
 * "last" keyword, stores it in the configuration rule struct, and returns
 * CONFIG_SUCCESS on success and CONFIG_ERROR on error.
 */
static enum config_status
option_stdin(struct rule *rule, char *value, const char *name, size_t lineno)
{
    if (strcmp(value, "last") == 0)
        rule->stdin_arg = -1;
    else if (!convert_number(value, &rule->stdin_arg)) {
        warn("%s:%lu: invalid stdin value %s", name,
             (unsigned long) lineno, value);
        return CONFIG_ERROR;
    }
    return CONFIG_SUCCESS;
}


/*
 * Parse the sudo configuration option.  The value is just stored and passed
 * verbatim to sudo as the -u option.  Returns CONFIG_SUCCESS on success and
 * CONFIG_ERROR on error.
 */
static enum config_status
option_sudo(struct rule *rule, char *value, const char *name UNUSED,
            size_t lineno UNUSED)
{
    rule->sudo_user = value;
    return CONFIG_SUCCESS;
}


/*
 * Parse the user configuration option.  Verifies that the value is either a
 * UID or a username, stores the user in the configuration rule struct, and
 * looks up the UID and primary GID and stores that in the configuration
 * struct as well.  Returns CONFIG_SUCCESS on success and CONFIG_ERROR on
 * error.
 */
static enum config_status
option_user(struct rule *rule, char *value, const char *name, size_t lineno)
{
    struct passwd *pw;
    long uid;

    if (convert_number(value, &uid))
        pw = getpwuid((uid_t) uid);
    else
        pw = getpwnam(value);
    if (pw == NULL) {
        warn("%s:%lu: invalid user value %s", name, (unsigned long) lineno,
             value);
        return CONFIG_ERROR;
    }
    rule->user = xstrdup(pw->pw_name);
    rule->uid = pw->pw_uid;
    rule->gid = pw->pw_gid;
    return CONFIG_SUCCESS;
}


/*
 * Parse the summary configuration option.  Stores the summary option in the
 * configuration rule struct.  Returns CONFIG_SUCCESS on success and
 * CONFIG_ERROR on error.
 */
static enum config_status
option_summary(struct rule *rule, char *value, const char *name UNUSED,
               size_t lineno UNUSED)
{
    rule->summary = value;
    return CONFIG_SUCCESS;
}


/*
 * Parse the help configuration option.  Stores the help option in the
 * configuration rule struct.  Returns CONFIG_SUCCESS on success and
 * CONFIG_ERROR on error.
 */
static enum config_status
option_help(struct rule *rule, char *value, const char *name UNUSED,
            size_t lineno UNUSED)
{
    rule->help = value;
    return CONFIG_SUCCESS;
}


/*
 * The table relating configuration option names to functions.
 */
static const struct config_option options[] = {
    { "help",    option_help    },
    { "logmask", option_logmask },
    { "stdin",   option_stdin   },
    { "sudo",    option_sudo    },
    { "summary", option_summary },
    { "user",    option_user    },
    { NULL,      NULL           }
};


/*
 * Parse a configuration option.  This is something after the command but
 * before the ACLs that contains an equal sign.  The configuration option is
 * the part before the equals and the value is the part afterwards.  Takes the
 * configuration rule, the option string, the file name, and the line number,
 * and stores data in the configuration rule struct as needed.
 *
 * Returns CONFIG_SUCCESS on success and CONFIG_ERROR on error, reporting an
 * error message.
 */
static enum config_status
parse_conf_option(struct rule *rule, char *option, const char *name,
                  size_t lineno)
{
    char *end;
    size_t length;
    const struct config_option *handler;

    end = strchr(option, '=');
    if (end == NULL) {
        warn("%s:%lu: invalid option %s", name, (unsigned long) lineno,
             option);
        return CONFIG_ERROR;
    }
    length = end - option;
    for (handler = options; handler->name != NULL; handler++)
        if (strlen(handler->name) == length)
            if (strncmp(handler->name, option, length) == 0)
                return (handler->parse)(rule, end + 1, name, lineno);
    warn("%s:%lu: unknown option %s", name, (unsigned long) lineno, option);
    return CONFIG_ERROR;
}


/*
 * Reads the configuration file and parses every line, populating a data
 * structure that will be traversed on each request to translate a command
 * into an executable path and ACL file.
 *
 * config is populated with the parsed configuration file.  Empty lines and
 * lines beginning with # are ignored.  Each line is divided into fields,
 * separated by spaces.  The fields are defined by struct rule.  Lines
 * ending in backslash are continued on the next line.  config is passed in as
 * a void * so that read_conf_file and acl_check_file can use common include
 * handling code.
 *
 * As a special case, include <file> will call read_conf_file recursively to
 * parse an included file (or, if <file> is a directory, every file in that
 * directory that doesn't contain a period).
 *
 * Returns CONFIG_SUCCESS on success and CONFIG_ERROR on error, reporting an
 * error message.
 */
static enum config_status
read_conf_file(void *data, const char *name)
{
    struct config *config = data;
    FILE *file;
    char *buffer, *p, *option;
    int bufsize;
    size_t length, count, i, arg_i;
    enum config_status s;
    struct vector *line = NULL;
    struct rule *rule = NULL;
    size_t lineno = 0;
    DIR *dir = NULL;

    bufsize = 1024;
    buffer = xmalloc(bufsize);
    file = fopen(name, "r");
    if (file == NULL) {
        free(buffer);
        syswarn("cannot open config file %s", name);
        return CONFIG_ERROR;
    }
    while (fgets(buffer, bufsize, file) != NULL) {
        length = strlen(buffer);
        if (length == 2 && buffer[length - 1] != '\n') {
            warn("%s:%lu: no final newline", name, (unsigned long) lineno);
            goto fail;
        }
        if (length < 2)
            continue;

        /*
         * Allow for long lines and continuation lines.  As long as we've
         * either filled the buffer or have a line ending in a backslash, we
         * keep reading more data.  If we filled the buffer, increase it by
         * another 1KB; otherwise, back up and write over the backslash and
         * newline.
         */
        p = buffer + length - 2;
        while (length > 2 && (p[1] != '\n' || p[0] == '\\')) {
            if (p[1] != '\n') {
                bufsize += 1024;
                buffer = xrealloc(buffer, bufsize);
            } else {
                length -= 2;
                lineno++;
            }
            if (fgets(buffer + length, bufsize - (int) length, file) == NULL) {
                warn("%s:%lu: no final line or newline", name,
                     (unsigned long) lineno);
                goto fail;
            }
            length = strlen(buffer);
            p = buffer + length - 2;
        }
        if (length > 0)
            buffer[length - 1] = '\0';
        lineno++;

        /*
         * Skip blank lines or commented-out lines.  Note that because of the
         * above logic, comments can be continued on the next line, so be
         * careful.
         */
        p = buffer;
        while (isspace((int) *p))
            p++;
        if (*p == '\0' || *p == '#')
            continue;

        /*
         * We have a valid configuration line.  Do a quick syntax check and
         * handle include.
         */
        line = vector_split_space(buffer, NULL);
        if (line->count == 2 && strcmp(line->strings[0], "include") == 0) {
            s = handle_include(line->strings[1], name, lineno, read_conf_file,
                               config);
            if (s < -1)
                goto fail;
            vector_free(line);
            line = NULL;
            continue;
        } else if (line->count < 4) {
            warn("%s:%lu: parse error", name, (unsigned long) lineno);
            goto fail;
        }

        /*
         * Okay, we have a regular configuration line.  Make sure there's
         * space for it in the config struct and stuff the vector into place.
         */
        if (config->count == config->allocated) {
            size_t size, n;

            n = (config->allocated < 4) ? 4 : config->allocated * 2;
            size = sizeof(struct rule *);
            config->rules = xreallocarray(config->rules, n, size);
            config->allocated = n;
        }
        rule = xcalloc(1, sizeof(struct rule));
        rule->line       = line;
        rule->command    = line->strings[0];
        rule->subcommand = line->strings[1];
        rule->program    = line->strings[2];

        /*
         * Parse config options.
         */
        for (arg_i = 3; arg_i < line->count; arg_i++) {
            option = line->strings[arg_i];
            if (!is_option(option))
                break;
            s = parse_conf_option(rule, option, name, lineno);
            if (s != CONFIG_SUCCESS)
                goto fail;
        }

        /*
         * One more syntax error possibility here: a line that only has
         * settings and no ACL file.
         */
        if (line->count <= arg_i) {
            warn("%s:%lu: config parse error", name, (unsigned long) lineno);
            goto fail;
        }

        /* Grab the metadata and list of ACL files. */
        rule->file = xstrdup(name);
        rule->lineno = lineno;
        count = line->count - arg_i + 1;
        rule->acls = xcalloc(count, sizeof(char *));
        for (i = 0; i < line->count - arg_i; i++)
            rule->acls[i] = line->strings[i + arg_i];
        rule->acls[i] = NULL;

        /* Success.  Put the configuration line in place. */
        config->rules[config->count] = rule;
        config->count++;
        rule = NULL;
        line = NULL;
    }

    /* Free allocated memory and return success. */
    free(buffer);
    fclose(file);
    return 0;

    /* Abort with an error. */
fail:
    if (dir != NULL)
        closedir(dir);
    vector_free(line);
    if (rule != NULL) {
        free(rule->logmask);
        free(rule);
    }
    free(buffer);
    fclose(file);
    return CONFIG_ERROR;
}


/*
 * Check to see if a principal is authorized by a given ACL file.
 *
 * This function is used to handle included ACL files and only does a simple
 * check to prevent infinite recursion, so be careful.  The first argument is
 * the user to check, which is passed in as a void * so that acl_check_file
 * and read_conf_file can share common include-handling code.
 *
 * Returns the result of the first check that returns a result other than
 * CONFIG_NOMATCH, or CONFIG_NOMATCH if no check returns some other value.
 * Also returns CONFIG_ERROR on some sort of failure (such as failure to read
 * a file or a syntax error).
 */
static enum config_status
acl_check_file_internal(void *data, const char *aclfile)
{
    const struct client *client = data;
    FILE *file;
    char buffer[BUFSIZ];
    char *p;
    int lineno;
    enum config_status s;
    size_t length;
    struct vector *line = NULL;

    file = fopen(aclfile, "r");
    if (file == NULL) {
        syswarn("cannot open ACL file %s", aclfile);
        return CONFIG_ERROR;
    }
    lineno = 0;
    while (fgets(buffer, sizeof(buffer), file) != NULL) {
        lineno++;
        length = strlen(buffer);
        if (length >= sizeof(buffer) - 1) {
            warn("%s:%d: ACL file line too long", aclfile, lineno);
            goto fail;
        }

        /*
         * Skip blank lines or commented-out lines and remove trailing
         * whitespace.
         */
        p = buffer + length - 1;
        while (p > buffer && isspace((int) *p))
            p--;
        p[1] = '\0';
        p = buffer;
        while (isspace((int) *p))
            p++;
        if (*p == '\0' || *p == '#')
            continue;

        /* Parse the line. */
        if (strchr(p, ' ') == NULL)
            s = acl_check(client, p, ACL_SCHEME_PRINC, aclfile, lineno);
        else {
            line = vector_split_space(buffer, NULL);
            if (line->count == 2 && strcmp(line->strings[0], "include") == 0) {
                s = acl_check(data, line->strings[1], ACL_SCHEME_FILE,
                              aclfile, lineno);
                vector_free(line);
                line = NULL;
            } else {
                warn("%s:%d: parse error", aclfile, lineno);
                goto fail;
            }
        }
        if (s != CONFIG_NOMATCH) {
            fclose(file);
            return s;
        }
    }
    fclose(file);
    return CONFIG_NOMATCH;

fail:
    vector_free(line);
    if (file != NULL)
        fclose(file);
    return CONFIG_ERROR;
}


/*
 * The ACL check operation for the file method.  Takes the user to check, the
 * ACL file or directory name, and the referencing file name and line number.
 *
 * Conceptually, this returns CONFIG_SUCCESS if the user is authorized,
 * CONFIG_NOMATCH if they aren't, CONFIG_ERROR on some sort of failure, and
 * CONFIG_DENY for an explicit deny.  What actually happens is the result of
 * the interplay between handle_include and acl_check_file_internal:
 *
 * - For each file, return the first result other than CONFIG_NOMATCH
 *   (indicating no match), or CONFIG_NOMATCH if there is no other result.
 *
 * - Return the first result from any file less than CONFIG_NOMATCH,
 *   indicating a failure or an explicit deny.
 *
 * - If there is no result less than CONFIG_NOMATCH, return the largest
 *   remaining result, which should be CONFIG_SUCCESS or CONFIG_NOMATCH.
 */
static enum config_status
acl_check_file(const struct client *client, const char *aclfile,
               const char *file, size_t lineno)
{
    return handle_include(aclfile, file, lineno, acl_check_file_internal,
                          (void *) client);
}


/*
 * The ACL check operation for the princ method.  Takes the user to check, the
 * principal name we are checking against, and the referencing file name and
 * line number.
 *
 * Returns CONFIG_SUCCESS if the user is authorized, or CONFIG_NOMATCH if they
 * aren't.
 */
static enum config_status
acl_check_princ(const struct client *client, const char *data,
                const char *file UNUSED, size_t lineno UNUSED)
{
    return (strcmp(client->user, data) == 0) ? CONFIG_SUCCESS : CONFIG_NOMATCH;
}


/*
 * The ACL check operation for the anyuser method.  Takes the client
 * information, the principal name we are checking against, and the
 * referencing file name and line number.
 *
 * Returns CONFIG_SUCCESS if the user is authorized, CONFIG_NOMATCH if they
 * aren't, and CONFIG_ERROR for an invalid argument to the anyuser ACL.
 */
static enum config_status
acl_check_anyuser(const struct client *client, const char *data,
                  const char *file, size_t lineno)
{
    if (strcmp(data, "auth") == 0)
        return client->anonymous ? CONFIG_NOMATCH : CONFIG_SUCCESS;
    else if (strcmp(data, "anonymous") == 0)
        return CONFIG_SUCCESS;
    else {
        warn("%s:%lu: invalid ACL value 'anyuser:%s'", file,
             (unsigned long) lineno, data);
        return CONFIG_ERROR;
    }
}


/*
 * The ACL check operation for the deny method.  Takes the user to check, the
 * scheme:method we are checking against, and the referencing file name and
 * line number.
 *
 * This one is a little unusual:
 *
 * - If the recursive check matches (status CONFIG_SUCCESS), it returns
 *   CONFIG_DENY.  This is treated by handle_include and
 *   acl_check_file_internal as an error condition, and causes processing to
 *   be stopped immediately, without doing further checks as would be done for
 *   a normal CONFIG_NOMATCH "no match" return.
 *
 * - If the recursive check does not match (status CONFIG_NOMATCH), it returns
 *   CONFIG_NOMATCH, which indicates "no match".  This allows processing to
 *   continue without either granting or denying access.
 *
 * - If the recursive check returns CONFIG_DENY, that is treated as a forced
 *   deny from a recursive call to acl_check_deny, and is returned as
 *   CONFIG_NOMATCH, indicating "no match".
 *
 * Any other result indicates a processing error and is returned as-is.
 */
static enum config_status
acl_check_deny(const struct client *client, const char *data,
               const char *file, size_t lineno)
{
    enum config_status s;

    s = acl_check(client, data, ACL_SCHEME_PRINC, file, lineno);
    switch (s) {
    case CONFIG_SUCCESS: return CONFIG_DENY;
    case CONFIG_NOMATCH: return CONFIG_NOMATCH;
    case CONFIG_DENY:    return CONFIG_NOMATCH;
    case CONFIG_ERROR:   return CONFIG_ERROR;
    default:             return s;
    }
}


/*
 * Sets the GPUT ACL file.  Currently, this function is only used by the test
 * suite.
 */
#ifdef HAVE_GPUT
void
server_config_set_gput_file(char *file)
{
    acl_gput_file = file;
}
#else
void
server_config_set_gput_file(char *file UNUSED)
{
    return;
}
#endif


/*
 * The ACL check operation for the gput method.  Takes the user to check, the
 * GPUT group name (and optional transform) we are checking against, and the
 * referencing file name and line number.
 *
 * The syntax of the data is "group" or "group[xform]".
 *
 * Returns CONFIG_SUCCESS if the user is authorized, CONFIG_NOMATCH if they
 * aren't, and CONFIG_ERROR on some sort of failure (such as failure to read a
 * file or a syntax error).
 */
#ifdef HAVE_GPUT
static enum config_status
acl_check_gput(const struct client *client, const char *data,
               const char *file, size_t lineno)
{
    GPUT *G;
    char *role, *xform, *xform_start, *xform_end;
    enum config_status s;

    xform_start = strchr(data, '[');
    if (xform_start != NULL) {
        xform_end = strchr(xform_start + 1, ']');
        if (xform_end == NULL) {
            warn("%s:%lu: missing ] in GPUT specification '%s'", file,
                 (unsigned long) lineno, data);
            return CONFIG_ERROR;
        }
        if (xform_end[1] != '\0') {
            warn("%s:%lu: invalid GPUT specification '%s'", file,
                 (unsigned long) lineno, data);
            return CONFIG_ERROR;
        }
        role = xstrndup(data, xform_start - data);
        xform = xstrndup(xform_start + 1, xform_end - (xform_start + 1));
    } else {
        role = (char *) data;
        xform = NULL;
    }

    /*
     * Sigh; apparently I wasn't flexible enough in GPUT error reporting.  You
     * can direct diagnostics to a file descriptor, but there's not much else
     * you can do with them.  In a future GPUT version, I'll make it possible
     * to have diagnostics reported via a callback.
     */
    G = gput_open(acl_gput_file, NULL);
    if (G == NULL)
        s = CONFIG_ERROR;
    else {
        if (gput_check(G, role, client->user, xform, NULL))
            s = CONFIG_SUCCESS;
        else
            s = CONFIG_NOMATCH;
        gput_close(G);
    }
    if (xform_start) {
        free(role);
        free(xform);
    }
    return s;
}
#endif /* HAVE_GPUT */


/*
 * The ACL check operation for PCRE matches.  Takes the user to check, the
 * regular expression, and the referencing file name and line number.  This
 * can be used to do things like allow only host principals and deny everyone
 * else.
 */
#ifdef HAVE_PCRE
static enum config_status
acl_check_pcre(const struct client *client, const char *data,
               const char *file, size_t lineno)
{
    pcre *regex;
    const char *error;
    const char *user = client->user;
    int offset, status;

    regex = pcre_compile(data, PCRE_NO_AUTO_CAPTURE, &error, &offset, NULL);
    if (regex == NULL) {
        warn("%s:%lu: compilation of regex '%s' failed around %d", file,
             (unsigned long) lineno, data, offset);
        return CONFIG_ERROR;
    }
    status = pcre_exec(regex, NULL, user, (int) strlen(user), 0, 0, NULL, 0);
    pcre_free(regex);
    switch (status) {
    case 0:
        return CONFIG_SUCCESS;
    case PCRE_ERROR_NOMATCH:
        return CONFIG_NOMATCH;
    default:
        warn("%s:%lu: matching with regex '%s' failed with status %d", file,
             (unsigned long) lineno, data, status);
        return CONFIG_ERROR;
    }
}
#endif /* HAVE_PCRE */


/*
 * The ACL check operation for POSIX regex matches.  Takes the user to check,
 * the regular expression, and the referencing file name and line number.
 * This can be used to do things like allow only host principals and deny
 * everyone else.
 */
#ifdef HAVE_REGCOMP
static enum config_status
acl_check_regex(const struct client *client, const char *data,
                const char *file, size_t lineno)
{
    regex_t regex;
    char error[BUFSIZ];
    int status;
    enum config_status result;

    memset(&regex, 0, sizeof(regex));
    status = regcomp(&regex, data, REG_EXTENDED | REG_NOSUB);
    if (status != 0) {
        regerror(status, &regex, error, sizeof(error));
        warn("%s:%lu: compilation of regex '%s' failed: %s", file,
             (unsigned long) lineno, data, error);
        return CONFIG_ERROR;
    }
    status = regexec(&regex, client->user, 0, NULL, 0);
    switch (status) {
    case 0:
        result = CONFIG_SUCCESS;
        break;
    case REG_NOMATCH:
        result = CONFIG_NOMATCH;
        break;
    default:
        regerror(status, &regex, error, sizeof(error));
        warn("%s:%lu: matching with regex '%s' failed: %s", file,
             (unsigned long) lineno, data, error);
        result = CONFIG_ERROR;
        break;
    }
    regfree(&regex);
    return result;
}
#endif /* HAVE_REGCOMP */


#if defined(HAVE_KRB5) && defined(HAVE_GETGRNAM_R)

/*
 * Convert the user (a Kerberos principal name) to a local username for group
 * lookups.  Returns true on success and false on an error other than there
 * being no local equivalent of the Kerberos principal.  Stores the local
 * equivalent username, or NULL if there is none, in the localname parameter.
 */
static bool
user_to_localname(const char *user, char **localname)
{
    krb5_context ctx = NULL;
    krb5_error_code code;
    krb5_principal princ = NULL;
    char buffer[BUFSIZ];

    /* Initialize the result. */
    *localname = NULL;

    /*
     * Unfortunately, we don't keep a Kerberos context around and have to
     * create a new one with each invocation.
     */
    code = krb5_init_context(&ctx);
    if (code != 0) {
        warn_krb5(ctx, code, "cannot create Kerberos context");
        return false;
    }

    /* Convert the user to a principal and find the local name. */
    code = krb5_parse_name(ctx, user, &princ);
    if (code != 0) {
        warn_krb5(ctx, code, "cannot parse principal %s", user);
        goto fail;
    }
    code = krb5_aname_to_localname(ctx, princ, sizeof(buffer), buffer);

    /*
     * Distinguish between no result with no error, a result (where we want to
     * make a copy), and an error.  Then free memory and return.
     */
    switch (code) {
    case KRB5_LNAME_NOTRANS:
    case KRB5_NO_LOCALNAME:
        /* No result.  Do nothing. */
        break;
    case 0:
        *localname = xstrdup(buffer);
        break;
    default:
        warn_krb5(ctx, code, "conversion of %s to local name failed", user);
        goto fail;
    }
    krb5_free_principal(ctx, princ);
    krb5_free_context(ctx);
    return true;

fail:
    if (princ != NULL)
        krb5_free_principal(ctx, princ);
    krb5_free_context(ctx);
    return false;
}


/*
 * Look up a group with getgrnam_r, which provides better error reporting than
 * the normal getrnam function but requires buffer handling.  Returns either
 * CONFIG_SUCCESS or CONFIG_ERROR.  On success, sets the provided group and
 * buffer pointers if the group was found.  The caller should free the buffer
 * when done with the results.  On failure to find the group, but no error,
 * returns success but with the buffer and group pointers set to NULL.  On
 * error, returns CONFIG_ERROR and sets errno.
 */
static enum config_status
acl_getgrnam(const char *group, struct group **grp, char **buffer)
{
    long buflen;
    int status, oerrno;
    struct group *result;
    enum config_status retval;

    /* Initialize the return values. */
    *grp = NULL;
    *buffer = NULL;

    /* Determine the initial size of the buffer for getgrnam_r. */
    buflen = sysconf(_SC_GETGR_R_SIZE_MAX);
    if (buflen <= 0)
        buflen = BUFSIZ;
    *buffer = xmalloc(buflen);

    /*
     * Get the group information, retrying on EINTR or ERANGE.  On ERANGE,
     * double the size of the buffer and try again.  This will exhaust memory
     * if the system call just keeps returning ERANGE; hopefully no system
     * will have that bug.
     */
    *grp = xmalloc(sizeof(struct group));
    do {
        status = getgrnam_r(group, *grp, *buffer, buflen, &result);
        if (status == ERANGE) {
            buflen *= 2;
            *buffer = xrealloc(*buffer, buflen);
        } else if (status != 0 && status != EINTR) {
            errno = status;
            retval = CONFIG_ERROR;
            goto fail;
        }
    } while (status == EINTR || status == ERANGE);

    /* If not found, free all the memory and return success with NULL. */
    if (result == NULL) {
        retval = CONFIG_SUCCESS;
        goto fail;
    }

    /* Otherwise, return success. */
    return CONFIG_SUCCESS;

fail:
    oerrno = errno;
    free(*buffer);
    *buffer = NULL;
    free(*grp);
    *grp = NULL;
    errno = oerrno;
    return retval;
}


/*
 * The ACL check operation for UNIX local group membership.  Takes the user to
 * check, the group of which they have to be a member, and the referencing
 * file name and line number.
 */
static enum config_status
acl_check_localgroup(const struct client *client, const char *group,
                     const char *file, size_t lineno)
{
    struct passwd *pw;
    struct group *gr = NULL;
    char *grbuffer = NULL;
    size_t i;
    char *localname = NULL;
    enum config_status result;

    /* Look up the group membership. */
    result = acl_getgrnam(group, &gr, &grbuffer);
    if (result != CONFIG_SUCCESS) {
        syswarn("%s:%lu: retrieving membership of localgroup %s failed", file,
                (unsigned long) lineno, group);
        return result;
    }
    if (gr == NULL)
        return CONFIG_NOMATCH;

    /*
     * Convert the principal to a local name.  Return no match if it doesn't
     * convert.
     */
    if (!user_to_localname(client->user, &localname)) {
        result = CONFIG_ERROR;
        goto done;
    }
    if (localname == NULL) {
        result = CONFIG_NOMATCH;
        goto done;
    }

    /* Look up the local user.  If they don't exist, return no match. */
    pw = getpwnam(localname);
    if (pw == NULL) {
        result = CONFIG_NOMATCH;
        goto done;
    }

    /* Check if the user's primary group is the desired group. */
    if (gr->gr_gid == pw->pw_gid) {
        result = CONFIG_SUCCESS;
        goto done;
    }

    /* Otherwise, check if the user is one of the other group members. */
    result = CONFIG_NOMATCH;
    for (i = 0; gr->gr_mem[i] != NULL; i++)
        if (strcmp(localname, gr->gr_mem[i]) == 0) {
            result = CONFIG_SUCCESS;
            break;
        }

done:
    free(gr);
    free(grbuffer);
    free(localname);
    return result;
}

#endif /* HAVE_KRB5 && HAVE_GETGRNAM_R */


/*
 * The table relating ACL scheme names to functions.  The first two ACL
 * schemes must remain in their current slots or the index constants set at
 * the top of the file need to change.
 */
static const struct acl_scheme schemes[] = {
    { "file",       acl_check_file       },
    { "princ",      acl_check_princ      },
    { "anyuser",    acl_check_anyuser    },
    { "deny",       acl_check_deny       },
#ifdef HAVE_GPUT
    { "gput",       acl_check_gput       },
#else
    { "gput",       NULL                 },
#endif
#if defined(HAVE_KRB5) && defined(HAVE_GETGRNAM_R)
    { "localgroup", acl_check_localgroup },
#else
    { "localgroup", NULL                 },
#endif
#ifdef HAVE_PCRE
    { "pcre",       acl_check_pcre       },
#else
    { "pcre",       NULL                 },
#endif
#ifdef HAVE_REGCOMP
    { "regex",      acl_check_regex      },
#else
    { "regex",      NULL                 },
#endif
    { NULL,         NULL                 }
};


/*
 * The access control check switch.  Takes the user to check, the ACL entry,
 * default scheme index, and referencing file name and line number.
 *
 * Returns CONFIG_SUCCESS if the user is authorized, CONFIG_NOMATCH if they
 * aren't, CONFIG_ERROR on some sort of failure (such as failure to read a
 * file or a syntax error), and CONFIG_DENY for an explicit deny.
 */
static enum config_status
acl_check(const struct client *client, const char *entry, int def_index,
          const char *file, size_t lineno)
{
    const struct acl_scheme *scheme;
    char *prefix;
    const char *data;

    /* First, check for ANYUSER and map it to anyuser:auth. */
    if (strcmp(entry, "ANYUSER") == 0)
        entry = "anyuser:auth";

    /* Parse the ACL entry for the scheme and dispatch. */
    data = strchr(entry, ':');
    if (data != NULL) {
        prefix = xstrndup(entry, data - entry);
        data++;
        for (scheme = schemes; scheme->name != NULL; scheme++)
            if (strcmp(prefix, scheme->name) == 0)
                break;
        if (scheme->name == NULL) {
            warn("%s:%lu: invalid ACL scheme '%s'", file,
                 (unsigned long) lineno, prefix);
            free(prefix);
            return CONFIG_ERROR;
        }
        free(prefix);
    } else {
        /* Use the default scheme. */
        scheme = schemes + def_index;
        data = entry;
    }
    if (scheme->check == NULL) {
        warn("%s:%lu: ACL scheme '%s' is not supported", file,
             (unsigned long) lineno, scheme->name);
        return CONFIG_ERROR;
    }
    return scheme->check(client, data, file, lineno);
}


/*
 * Load a configuration file.  Returns a newly allocated config struct if
 * successful or NULL on failure, logging an appropriate error message.
 */
struct config *
server_config_load(const char *file)
{
    struct config *config;

    /* Read the configuration file. */
    config = xcalloc(1, sizeof(struct config));
    if (read_conf_file(config, file) != 0) {
        server_config_free(config);
        return NULL;
    }
    return config;
}


/*
 * Free the config structure created by calling server_config_load.
 */
void
server_config_free(struct config *config)
{
    struct rule *rule;
    size_t i;

    for (i = 0; i < config->count; i++) {
        rule = config->rules[i];
        free(rule->logmask);
        free(rule->user);
        free(rule->acls);
        vector_free(rule->line);
        free(rule->file);
        free(rule);
    }
    free(config->rules);
    free(config);
}


/*
 * Given the rule corresponding to the command and the struct representing a
 * client connection, see if the command is allowed.  Return true if so, false
 * otherwise.
 */
bool
server_config_acl_permit(const struct rule *rule, const struct client *client)
{
    char **acls = rule->acls;
    size_t i;
    enum config_status status;

    for (i = 0; acls[i] != NULL; i++) {
        status = acl_check(client, acls[i], ACL_SCHEME_FILE, rule->file,
                           rule->lineno);
        if (status == 0)
            return true;
        else if (status < -1)
            return false;
    }
    return false;
}
