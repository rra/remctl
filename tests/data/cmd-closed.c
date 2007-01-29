/*  $Id$
**
**  Small C program to verify that standard input is not closed but returns
**  EOF on any read.
*/

#include <config.h>
#include <system.h>

#include <errno.h>

int
main(void)
{
    char buffer;
    ssize_t count;

    count = read(0, &buffer, 1);
    if (count == 0) {
        printf("Okay");
        exit(0);
    } else if (count > 0) {
        printf("Read %d bytes\n", (int) count);
        exit(1);
    } else {
        printf("Failed with error: %s\n", strerror(errno));
        exit(2);
    }
}
