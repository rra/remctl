/*  $Id$
**
**  Configuration parsing.
**
**  These are the functions for parsing the remctld configuration file and
**  checking access.
**
**  Written by Russ Allbery <rra@stanford.edu>
**  Based on work by Anton Ushakov
**  Copyright 2002, 2003, 2004, 2005, 2006, 2007
**      Board of Trustees, Leland Stanford Jr. University
**
**  See README for licensing terms.
*/

#include <config.h>
#include <system.h>

#include <ctype.h>
#include <dirent.h>
#include <sys/stat.h>

#include <server/internal.h>
#include <util/util.h>


/*
**  Handles an include request for either read_conf_file or acl_check_file.
**  Takes the vector that represents the include directive, the current file,
**  the line number, the function to call for each included file, and a piece
**  of data to pass to that function.  Handles including either files or
**  directories.
**
**  If the function returns a value less than -1, return its return code.
**  Otherwise, return the greatest of all status codes returned by the
**  functions.  Also returns -2 for any error in processing the include
**  directive.
*/
static int
handle_include(struct vector *line, const char *file, int lineno,
               int (*function)(void *, const char *), void *data)
{
    const char *included = line->strings[1];
    struct stat st;

    /* Validate the directive. */
    if (line->count != 2 || strcmp(line->strings[0], "include") != 0) {
        warn("%s:%d: parse error", file, lineno);
        return -2;
    }
    if (strcmp(included, file) == 0) {
        warn("%s:%d: %s recursively included", file, lineno, file);
        return -2;
    }
    if (stat(included, &st) < 0) {
        warn("%s:%d: included file %s not found", file, lineno, included);
        return -2;
    }

    /* If it's a directory, include everything in the directory that doesn't
       contain a period.  Otherwise, just include the one file. */
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
            if (last == -2) {
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
**  Reads the configuration file and parses every line, populating a data
**  structure that will be traversed on each request to translate a command
**  type into an executable path and ACL file.
**
**  config is populated with the parsed configuration file.  Empty lines and
**  lines beginning with # are ignored.  Each line is divided into fields,
**  separated by spaces.  The fields are defined by struct confline.  Lines
**  ending in backslash are continued on the next line.  config is passed in
**  as a void * so that read_conf_file and acl_check_file can use common
**  include handling code.
**
**  As a special case, include <file> will call read_conf_file recursively to
**  parse an included file (or, if <file> is a directory, every file in that
**  directory that doesn't contain a period).
**
**  Returns 0 on success and -2 on error, reporting an error message.
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

        /* Allow for long lines and continuation lines.  As long as we've
           either filled the buffer or have a line ending in a backslash, we
           keep reading more data.  If we filled the buffer, increase it by
           another 1KB; otherwise, back up and write over the backslash and
           newline. */
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

        /* Skip blank lines or commented-out lines.  Note that because of the
           above logic, comments can be continued on the next line, so be
           careful. */
        p = buffer;
        while (isspace((int) *p))
            p++;
        if (*p == '\0' || *p == '#')
            continue;

        /* We have a valid configuration line.  Do a quick syntax check and
           handle include. */
        line = vector_split_space(buffer, NULL);
        if (line->count < 4) {
            s = handle_include(line, name, lineno, read_conf_file, config);
            if (s < -1)
                goto fail;
            vector_free(line);
            line = NULL;
            continue;
        }

        /* Okay, we have a regular configuration line.  Make sure there's
           space for it in the config struct and stuff the vector into
           place. */
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

        /* Change this to a while vline->string[n] has an "=" in it to support
           multiple x=y options. */
        if (strncmp(line->strings[3], "logmask=", 8) == 0) {
            confline->logmask = cvector_new();
            cvector_split(line->strings[3] + 8, ',', confline->logmask);
            arg_i = 4;
        } else
            arg_i = 3;

        /* One more syntax error possibility here: a line that only has a
           logmask setting but no ACL files. */
        if (line->count <= arg_i) {
            warn("%s:%lu: config parse error", name, (unsigned long) lineno);
            goto fail;
        }

        /* Grab the list of ACL files. */
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
**  Check to see if a given principal is in a given file.  This function is
**  recursive to handle included ACL files and only does a simple check to
**  prevent infinite recursion, so be careful.  The first argument is the user
**  to check, which is passed in as a void * so that acl_check_file and
**  read_conf_file can share common include-handling code.
**
**  Returns 0 if the user is authorized, -1 if they aren't, and -2 on some
**  sort of failure (such as failure to read a file or a syntax error).
*/
static int
acl_check_file(void *data, const char *aclfile)
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

        /* Skip blank lines or commented-out lines and remove trailing
         * whitespace. */
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
        if (strchr(p, ' ') != NULL) {
            line = vector_split_space(buffer, NULL);
            s = handle_include(line, aclfile, lineno, acl_check_file, data);
            vector_free(line);
            line = NULL;
            if (s != -1) {
                fclose(file);
                return s;
            }
        } else if (strcmp(p, user) == 0) {
            fclose(file);
            return 0;
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
**  Load a configuration file.  Returns a newly allocated config struct if
**  successful or NULL on failure, logging an appropriate error message.
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
**  Free the config structure created by calling server_config_load.
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
    }
    free(config->rules);
    free(config);
}


/*
**  Given the confline corresponding to the command and the principal
**  requesting access, see if the command is allowed.  Return true if so,
**  false otherwise.
*/
int
server_config_acl_permit(struct confline *cline, const char *user)
{
    char **acls = cline->acls;
    int status, i;

    if (strcmp(acls[0], "ANYUSER") == 0)
        return 1;
    for (i = 0; acls[i] != NULL; i++) {
        status = acl_check_file((void *) user, acls[i]);
        if (status == 0)
            return 1;
        else if (status < -1)
            return 0;
    }
    return 0;
}
