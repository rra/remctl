/*
 * Small C program to print out the UID and GID it was run as.
 *
 * Written by Russ Allbery <rra@stanford.edu>
 * Copyright 2012
 *     The Board of Trustees of the Leland Stanford Junior University
 *
 * See LICENSE for licensing terms.
 */

#include <config.h>
#include <portable/system.h>


int
main(void)
{
    printf("%lu %lu\n", (unsigned long) getuid(), (unsigned long) getgid());
    exit(0);
}
