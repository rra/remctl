
#include <tests/server/acl-helpers.h>

static struct request *srequest = NULL;

struct request *new_request(const char *user, 
                            const char *command,
                            const char *subcommand,
                            const char **argv)
{

    struct request *request = NULL;

    if (srequest == NULL)
        srequest = (struct request *) malloc(sizeof(struct request));

    request = srequest;

    request->user = (char *) user;
    request->command = (char *) command;
    request->subcommand = (char *) subcommand;
    request->argv = (char **) argv;

    return request;
}
