/*
 * Configuration parsing.
 *
 * These are the functions for parsing the remctld configuration file and
 * checking access.
 *
 * Written by Russ Allbery <rra@stanford.edu>
 * Based on work by Anton Ushakov
 * Copyright 2002, 2003, 2004, 2005, 2006, 2007, 2008
 *     Board of Trustees, Leland Stanford Jr. University
 *
 * See LICENSE for licensing terms.
 */

#include <config.h>
#include <portable/system.h>

#include <ctype.h>
#include <dirent.h>
#include <sys/stat.h>

#include <server/internal.h>
#include <util/util.h>

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
 * The following must match the indexes of these schemes in schemes[].
 * They're used to implement default ACL schemes in particular contexts.
 */
#define ACL_SCHEME_FILE  0
#define ACL_SCHEME_PRINC 1

/* Forward declarations. */
static int acl_check(const char *user, const char *entry, int def_index,
                     const char *file, int lineno);


/*
 * Process a request for including a file, either for configuration or for
 * ACLs.  Called by read_conf_file and acl_check_file.
 *
 * Takes the vector that represents the include directive, the current file,
 * the line number, the function to call for each included file, and a piece
 * of data to pass to that function.  Handles including either files or
 * directories.  When used for processing ACL files named in the configuration
 * file, the current file and line number will be passed as zero.
 *
 * If the function returns a value less than -1, return its return code.  If
 * the file is recursively included or if there is an error in reading a file
 * or processing an include directory, return -2.  Otherwise, return the
 * greatest of all status codes returned by function.
 */
static int
handle_include(const char *included, const char *file, int lineno,
               int (*function)(void *, const char *), void *data)
{
    struct stat st;

    /* Sanity checking. */
    if (strcmp(included, file) == 0) {
        warn("%s:%d: %s recursively included", file, lineno, file);
        return -2;
    }
    if (stat(included, &st) < 0) {
        warn("%s:%d: included file %s not found", file, lineno, included);
        return -2;
    }

    /*
     * If it's a directory, include everything in the directory that doesn't
     * contain a period.  Otherwise, just include the one file.
     */
    if (!S_ISDIR(st.st_mode)) {
        return (*function)(data, included);
    } else {
        DIR *dir;
        struct dirent *entry;
        int status = -1;
        int last;

        dir = opendir(included);
        while ((entry = readdir(dir)) != NULL) {
            char *path;
            size_t length;

            if (strchr(entry->d_name, '.') != NULL)
                continue;
            length = strlen(included) + 1 + strlen(entry->d_name) + 1;
            path = xmalloc(length);
            snprintf(path, length, "%s/%s", included, entry->d_name);
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
 * Reads the configuration file and parses every line, populating a data
 * structure that will be traversed on each request to translate a command
 * type into an executable path and ACL file.
 *
 * config is populated with the parsed configuration file.  Empty lines and
 * lines beginning with # are ignored.  Each line is divided into fields,
 * separated by spaces.  The fields are defined by struct confline.  Lines
 * ending in backslash are continued on the next line.  config is passed in
 * as a void * so that read_conf_file and acl_check_file can use common
 * include handling code.
 *
 * As a special case, include <file> will call read_conf_file recursively to
 * parse an included file (or, if <file> is a directory, every file in that
 * directory that doesn't contain a period).
 *
 * Returns 0 on success and -2 on error, reporting an error message.
 */
static int
read_conf_file(void *data, const char *name)
{
    struct config *config = data;
    FILE *file;
    char *buffer, *p;
    size_t bufsize, length, size, count, i, arg_i;
    int s;
    struct vector *line = NULL;
    struct confline *confline = NULL;
    size_t lineno = 0;
    DIR *dir = NULL;

    bufsize = 1024;
    buffer = xmalloc(bufsize);
    file = fopen(name, "r");
    if (file == NULL) {
        free(buffer);
        syswarn("cannot open config file %s", name);
        return -2;
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
            warn("%s:%d: parse error", name, lineno);
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
            size = config->allocated * sizeof(struct confline *);
            config->rules = xrealloc(config->rules, size);
        }
        confline = xcalloc(1, sizeof(struct confline));
        confline->line    = line;
        confline->type    = line->strings[0];
        confline->service = line->strings[1];
        confline->program = line->strings[2];

        /*
         * Change this to a while vline->string[n] has an "=" in it to support
         * multiple x=y options.
         */
        if (strncmp(line->strings[3], "logmask=", 8) == 0) {
            confline->logmask = cvector_new();
            cvector_split(line->strings[3] + 8, ',', confline->logmask);
            arg_i = 4;
        } else
            arg_i = 3;

        /*
         * One more syntax error possibility here: a line that only has a
         * logmask setting but no ACL files.
         */
        if (line->count <= arg_i) {
            warn("%s:%lu: config parse error", name, (unsigned long) lineno);
            goto fail;
        }

        /* Grab the metadata and list of ACL files. */
        confline->file = xstrdup(name);
        confline->lineno = lineno;
        count = line->count - arg_i + 1;
        confline->acls = xmalloc(count * sizeof(char *));
        for (i = 0; i < line->count - arg_i; i++)
            confline->acls[i] = line->strings[i + arg_i];
        confline->acls[i] = NULL;

        /* Success.  Put the configuration line in place. */
        config->rules[config->count] = confline;
        config->count++;
        confline = NULL;
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
    if (line != NULL)
        vector_free(line);
    if (confline != NULL) {
        if (confline->logmask != NULL)
            cvector_free(confline->logmask);
        free(confline);
    }
    free(buffer);
    fclose(file);
    return -2;
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
 * -1, or -1 if no check returns some other value.  Also returns -2 on
 * some sort of failure (such as failure to read a file or a syntax error).
 */
static int
acl_check_file_internal(void *data, const char *aclfile)
{
    const char *user = data;
    FILE *file = NULL;
    char buffer[BUFSIZ];
    char *p;
    int lineno, s;
    size_t length;
    struct vector *line = NULL;

    file = fopen(aclfile, "r");
    if (file == NULL) {
        syswarn("cannot open ACL file %s", aclfile);
        return -2;
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
        while (isspace((int) *p))
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
        if (s != -1) {
            fclose(file);
            return s;
        }
    }
    return -1;

fail:
    if (line != NULL)
        vector_free(line);
    if (file != NULL)
        fclose(file);
    return -2;
}


/*
 * The ACL check operation for the file method.  Takes the user to check, the
 * ACL file or directory name, and the referencing file name and line number.
 *
 * Conceptually, this returns 0 if the user is authorized, -1 if they aren't,
 * -2 on some sort of failure, and -3 for an explicit deny.  What actually
 * happens is the result of the interplay between handle_include and
 * acl_check_file_internal:
 *
 * - For each file, return the first result other than -1 (indicating no
 *   match), or -1 if there is no other result.
 *
 * - Return the first result from any file less than -1, indicating a failure
 *   or an explicit deny.
 *
 * - If there is no result less than -1, return the largest remaining result,
 *   which should be 0 (match) or -1 (no match).
 */
static int
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
 * Returns 0 if the user is authorized, or -1 if they aren't.
 */
static int
acl_check_princ(const char *user, const char *data, const char *file UNUSED,
                int lineno UNUSED)
{
    return (strcmp(user, data) == 0) ? 0 : -1;
}


/*
 * The ACL check operation for the deny method.  Takes the user to check, the
 * scheme:method we are checking against, and the referencing file name and
 * line number.
 *
 * This one is a little unusual:
 *
 * - If the recursive check matches (status 0), it returns -3.  This is
 *   treated by handle_include and acl_check_file_internal as an error
 *   condition, and causes processing to be stopped immediately, without doing
 *   further checks as would be done for a normal -1 "no match" return.
 *
 * - If the recursive check does not match (status -1), it returns -1, which
 *   indicates "no match".  This allows processing to continue without either
 *   granting or denying access.
 *
 * - If the recursive check returns -3, that is treated as a forced deny from
 *   a recursive call to acl_check_deny, and is returned as -1, indicating "no
 *   match".
 *
 * Any other result indicates a processing error and is returned as-is.
 */
static int
acl_check_deny(const char *user, const char *data, const char *file,
               int lineno)
{
    int s;

    s = acl_check(user, data, ACL_SCHEME_PRINC, file, lineno);
    switch (s) {
    case 0:  return -3;         /* match       => deny */
    case -1: return -1;         /* no match    => keep going */
    case -3: return -1;         /* forced deny => keep going */
    default: return s;          /* Anything else is returned as is. */
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
 * Returns 0 if the user is authorized, -1 if they aren't, and -2 on some sort
 * of failure (such as failure to read a file or a syntax error).
 */
#ifdef HAVE_GPUT
static int
acl_check_gput(const char *user, const char *data, const char *file,
               int lineno)
{
    GPUT *G;
    char *role, *xform, *xform_start, *xform_end;
    int s;

    xform_start = strchr(data, '[');
    if (xform_start != NULL) {
        xform_end = strchr(xform_start + 1, ']');
        if (xform_end == NULL) {
            warn("%s:%d: missing ] in GPUT specification '%s'", file, lineno,
                 data);
            return -2;
        }
        if (xform_end[1] != '\0') {
            warn("%s:%d: invalid GPUT specification '%s'", file, lineno,
                 data);
            return -2;
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
        s = -2;
    else {
        if (gput_check(G, role, (char *) user, xform, NULL))
            s = 0;
        else
            s = -1;
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
 * The table relating ACL scheme names to files.  The first two ACL schemes
 * must remain in their current slots or the index constants set at the top of
 * the file need to change.
 */
static const struct acl_scheme const schemes[] = {
    { "file",  acl_check_file  },
    { "princ", acl_check_princ },
    { "deny",  acl_check_deny  },
#ifdef HAVE_GPUT
    { "gput",  acl_check_gput  },
#else
    { "gput",  NULL            },
#endif
    { NULL,    NULL            }
};


/*
 * The access control check switch.  Takes the user to check, the ACL entry,
 * default scheme index, and referencing file name and line number.
 *
 * Returns 0 if the user is authorized, -1 if they aren't, -2 on some sort of
 * failure (such as failure to read a file or a syntax error), and -3 for an
 * explicit deny.
 */
static int
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
            return -2;
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
        return -2;
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
        free(config);
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
    struct confline *rule;
    size_t i;

    for (i = 0; i < config->count; i++) {
        rule = config->rules[i];
        if (rule->logmask != NULL)
            cvector_free(rule->logmask);
        if (rule->acls != NULL)
            free(rule->acls);
        if (rule->line != NULL)
            vector_free(rule->line);
        if (rule->file != NULL)
            free(rule->file);
    }
    free(config->rules);
    free(config);
}


/*
 * Given the confline corresponding to the command and the principal
 * requesting access, see if the command is allowed.  Return true if so, false
 * otherwise.
 */
bool
server_config_acl_permit(struct confline *cline, const char *user)
{
    char **acls = cline->acls;
    int status, i;

    if (strcmp(acls[0], "ANYUSER") == 0)
        return true;
    for (i = 0; acls[i] != NULL; i++) {
        status = acl_check(user, acls[i], ACL_SCHEME_FILE, cline->file,
                           cline->lineno);
        if (status == 0)
            return true;
        else if (status < -1)
            return false;
    }
    return false;
}
