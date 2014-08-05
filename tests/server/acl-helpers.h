#ifndef REMCTL_TESTS_HELPERS_H
#define REMCTL_TESTS_HELPERS_H 1

#include <util/xmalloc.h>
#include <server/internal.h>

struct request *new_request(const char *user, 
                            const char *command,
                            const char *subcommand,
                            const char **argv);

#define USER_ONLY_REQUEST(user) new_request(user, NULL, NULL, NULL)

#endif
