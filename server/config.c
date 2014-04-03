/*
 * Configuration parsing.
 *
 * These are the functions for parsing the remctld configuration file and
 * checking access.
 *
 * Written by Russ Allbery <eagle@eyrie.org>
 * Based on work by Anton Ushakov
 * Copyright 2002, 2003, 2004, 2005, 2006, 2007, 2008, 2009, 2010, 2012, 2014
 *     The Board of Trustees of the Leland Stanford Junior University
 * Copyright 2008 Carnegie Mellon University
 *
 * See LICENSE for licensing terms.
 */

#include <config.h>
#include <portable/system.h>

#include <ctype.h>
#include <dirent.h>
#include <errno.h>
#ifdef HAVE_PCRE
# include <pcre.h>
#endif
#include <pwd.h>
#ifdef HAVE_REGCOMP
# include <regex.h>
#endif
#include <sys/stat.h>

#ifdef HAVE_REMCTL_UNXGRP_ACL 
# include <sys/types.h>
# include <grp.h>
# include <unistd.h>
# include <portable/krb5.h>
#endif

#include <server/internal.h>
#include <util/macros.h>
#include <util/messages.h>
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

/*
 * maximum length allowed when converting
 * principal to local name
 */
#define REMCTL_KRB5_LOCALNAME_MAX_LEN sysconf(_SC_LOGIN_NAME_MAX) < 256 \
                                      ? 256 \
                                      : sysconf(_SC_LOGIN_NAME_MAX)

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
    enum config_status (*check)(const char *user, const char *data,
                                const char *file, int lineno);
};

/*
 * The following must match the indexes of these schemes in schemes[].
 * They're used to implement default ACL schemes in particular contexts.
 */
#define ACL_SCHEME_FILE  0
#define ACL_SCHEME_PRINC 1

/* Forward declarations. */
static enum config_status acl_check(const char *user, const char *entry,
                                    int def_index, const char *file,
                                    int lineno);

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
handle_include(const char *included, const char *file, int lineno,
               enum config_status (*function)(void *, const char *),
               void *data)
{
    struct stat st;

    /* Sanity checking. */
    if (strcmp(included, file) == 0) {
        warn("%s:%d: %s recursively included", file, lineno, file);
        return CONFIG_ERROR;
    }
    if (stat(included, &st) < 0) {
        syswarn("%s:%d: included file %s not found", file, lineno, included);
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
            syswarn("%s:%d: included directory %s cannot be opened", file,
                 lineno, included);
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
        if (!convert_number(logmask->strings[i], &mask)) {
            warn("%s:%lu: invalid logmask parameter %s", name,
                 (unsigned long) lineno, logmask->strings[i]);
            cvector_free(logmask);
            free(rule->logmask);
            rule->logmask = NULL;
            return CONFIG_ERROR;
        }
        rule->logmask[i] = mask;
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
        pw = getpwuid(uid);
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
    size_t bufsize, length, size, count, i, arg_i;
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
            if (fgets(buffer + length, bufsize - length, file) == NULL) {
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
            if (config->allocated < 4)
                config->allocated = 4;
            else
                config->allocated *= 2;
            size = config->allocated * sizeof(struct rule *);
            config->rules = xrealloc(config->rules, size);
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
        rule->acls = xmalloc(count * sizeof(char *));
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
    const char *user = data;
    FILE *file = NULL;
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
            s = acl_check(user, p, ACL_SCHEME_PRINC, aclfile, lineno);
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
acl_check_file(const char *user, const char *aclfile, const char *file,
               int lineno)
{
    return handle_include(aclfile, file, lineno, acl_check_file_internal,
                          (void *) user);
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
acl_check_princ(const char *user, const char *data, const char *file UNUSED,
                int lineno UNUSED)
{
    return (strcmp(user, data) == 0) ? CONFIG_SUCCESS : CONFIG_NOMATCH;
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
acl_check_deny(const char *user, const char *data, const char *file,
               int lineno)
{
    enum config_status s;

    s = acl_check(user, data, ACL_SCHEME_PRINC, file, lineno);
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
acl_check_gput(const char *user, const char *data, const char *file,
               int lineno)
{
    GPUT *G;
    char *role, *xform, *xform_start, *xform_end;
    enum config_status s;

    xform_start = strchr(data, '[');
    if (xform_start != NULL) {
        xform_end = strchr(xform_start + 1, ']');
        if (xform_end == NULL) {
            warn("%s:%d: missing ] in GPUT specification '%s'", file, lineno,
                 data);
            return CONFIG_ERROR;
        }
        if (xform_end[1] != '\0') {
            warn("%s:%d: invalid GPUT specification '%s'", file, lineno,
                 data);
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
        if (gput_check(G, role, (char *) user, xform, NULL))
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
acl_check_pcre(const char *user, const char *data, const char *file,
               int lineno)
{
    pcre *regex;
    const char *error;
    int offset, status;

    regex = pcre_compile(data, PCRE_NO_AUTO_CAPTURE, &error, &offset, NULL);
    if (regex == NULL) {
        warn("%s:%d: compilation of regex '%s' failed around %d", file,
             lineno, data, offset);
        return CONFIG_ERROR;
    }
    status = pcre_exec(regex, NULL, user, strlen(user), 0, 0, NULL, 0);
    pcre_free(regex);
    switch (status) {
    case 0:
        return CONFIG_SUCCESS;
    case PCRE_ERROR_NOMATCH:
        return CONFIG_NOMATCH;
    default:
        warn("%s:%d: matching with regex '%s' failed with status %d", file,
             lineno, data, status);
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
acl_check_regex(const char *user, const char *data, const char *file,
                int lineno)
{
    regex_t regex;
    char error[BUFSIZ];
    int status;
    enum config_status result;

    memset(&regex, 0, sizeof(regex));
    status = regcomp(&regex, data, REG_EXTENDED | REG_NOSUB);
    if (status != 0) {
        regerror(status, &regex, error, sizeof(error));
        warn("%s:%d: compilation of regex '%s' failed: %s", file, lineno,
             data, error);
        return CONFIG_ERROR;
    }
    status = regexec(&regex, user, 0, NULL, 0);
    switch (status) {
    case 0:
        result = CONFIG_SUCCESS;
        break;
    case REG_NOMATCH:
        result = CONFIG_NOMATCH;
        break;
    default:
        regerror(status, &regex, error, sizeof(error));
        warn("%s:%d: matching with regex '%s' failed: %s", file, lineno,
             data, error);
        result = CONFIG_ERROR;
        break;
    }
    regfree(&regex);
    return result;
}
#endif /* HAVE_REGCOMP */

#ifdef HAVE_REMCTL_UNXGRP_ACL
static enum config_status
acl_check_unxgrp (const char *user, const char *data, const char *file,
                int lineno)
{
    struct group grp;
    struct group *tempgrp = NULL;
    int status = -1;
    enum config_status result = CONFIG_ERROR;
    long buf_len = -1;
    char *buf = NULL;
    char *lname = NULL;
    size_t lsize = REMCTL_KRB5_LOCALNAME_MAX_LEN;
    size_t buf_sz = 1024;

    memset(&grp, 0x0, sizeof(grp));

    buf_len = sysconf(_SC_GETGR_R_SIZE_MAX);
    if (buf_len > 0) {
        buf_sz = (size_t) buf_sz;
    }

    buf = (char *) xmalloc(sizeof(char) * buf_sz);
    memset(buf, 0x0, buf_sz);

    do {
        status = getgrnam_r(data, &grp, buf, buf_sz, &tempgrp);
        if (status != 0 && status != EINTR) {
            warn("%s:%d: resolving unix group '%s' failed with status %d", file,
                 lineno, data, status);
            result = CONFIG_ERROR;
            goto die;
        }
    } while (status == EINTR);

    /* No group matching */
    if (tempgrp == NULL) {
        result = CONFIG_NOMATCH;
    }
    else {
        /* Sanitize and search for match in group members */
        char *cur_member = grp.gr_mem[0];
        int i = 0;
        krb5_context krb_ctx = NULL;
        krb5_principal princ;
        krb5_error_code krb5_code = -1;

        memset(&princ, 0x0, sizeof(princ));

        krb5_code = krb5_init_context(&krb_ctx);
        if (krb5_code != 0) {
            warn("%s:%d: initializing krb5 context failed with status %d", file,
                lineno, krb5_code);
            result = CONFIG_ERROR;
            goto die;
        }

        krb5_code = krb5_parse_name(krb_ctx, user, &princ);
        if (krb5_code != 0) {
            warn("%s:%d: parsing principal %s failed with status %d (%s)", file,
                lineno, user, krb5_code, krb5_get_error_message(krb_ctx, krb5_code));
            result = CONFIG_ERROR;
            goto die;
        }

        lname = (char *) xmalloc(sizeof(char) * lsize);
        memset(lname, 0x0, lsize);

        krb5_code = krb5_aname_to_localname(krb_ctx, princ, lsize, lname);
        if (krb5_code != 0) {
            if (krb5_code == KRB5_LNAME_NOTRANS) {
                result = CONFIG_NOMATCH;
            }
            else {
                warn("%s:%d: converting krb5 principal %s to localname failed with status %d (%s)", file,
                    lineno, user, krb5_code, krb5_get_error_message(krb_ctx, krb5_code));
                result = CONFIG_ERROR;
            }
            goto die;
        }

        /* Check if sanitized user is within group members */
        while (cur_member != NULL && *cur_member != '\0') {
            if (strcmp(cur_member, lname) == 0) {
                result = CONFIG_SUCCESS;
                goto die;
            }
            cur_member = grp.gr_mem[++i];
        }

        result = CONFIG_NOMATCH;
    }

die:
    if (buf != NULL) {
        free(buf);
        buf = NULL;
    }

    if (lname != NULL) {
        free(lname);
        lname = NULL;
    }

    return result;
}
#endif /* HAVE_REMCTL_UNXGRP_ACL */

/*
 * The table relating ACL scheme names to functions.  The first two ACL
 * schemes must remain in their current slots or the index constants set at
 * the top of the file need to change.
 */
static const struct acl_scheme schemes[] = {
    { "file",  acl_check_file  },
    { "princ", acl_check_princ },
    { "deny",  acl_check_deny  },
#ifdef HAVE_GPUT
    { "gput",  acl_check_gput  },
#else
    { "gput",  NULL            },
#endif
#ifdef HAVE_PCRE
    { "pcre",  acl_check_pcre  },
#else
    { "pcre",  NULL            },
#endif
#ifdef HAVE_REGCOMP
    { "regex", acl_check_regex },
#else
    { "regex", NULL            },
#endif
#ifdef HAVE_REMCTL_UNXGRP_ACL
    { "unxgrp", acl_check_unxgrp },
#else
    { "unxgrp", NULL         },
#endif
    { NULL,    NULL            }
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
acl_check(const char *user, const char *entry, int def_index,
          const char *file, int lineno)
{
    const struct acl_scheme *scheme;
    char *prefix;
    const char *data;

    data = strchr(entry, ':');
    if (data != NULL) {
        prefix = xstrndup(entry, data - entry);
        data++;
        for (scheme = schemes; scheme->name != NULL; scheme++)
            if (strcmp(prefix, scheme->name) == 0)
                break;
        if (scheme->name == NULL) {
            warn("%s:%d: invalid ACL scheme '%s'", file, lineno, prefix);
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
        warn("%s:%d: ACL scheme '%s' is not supported", file, lineno,
             scheme->name);
        return CONFIG_ERROR;
    }
    return scheme->check(user, data, file, lineno);
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
 * Given the rule corresponding to the command and the principal
 * requesting access, see if the command is allowed.  Return true if so, false
 * otherwise.
 */
bool
server_config_acl_permit(struct rule *rule, const char *user)
{
    char **acls = rule->acls;
    size_t i;
    enum config_status status;

    if (strcmp(acls[0], "ANYUSER") == 0)
        return true;
    for (i = 0; acls[i] != NULL; i++) {
        status = acl_check(user, acls[i], ACL_SCHEME_FILE, rule->file,
                           rule->lineno);
        if (status == 0)
            return true;
        else if (status < -1)
            return false;
    }
    return false;
}
