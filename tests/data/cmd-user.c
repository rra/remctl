/*
 * Small C program to print out the UID and GID it was run as.
 *
 * Written by Russ Allbery <eagle@eyrie.org>
 * Copyright 2012
 *     The Board of Trustees of the Leland Stanford Junior University
 *
 * SPDX-License-Identifier: MIT
 */

#include <config.h>
#include <portable/system.h>


int
main(void)
{
    printf("%lu %lu\n", (unsigned long) getuid(), (unsigned long) getgid());
    exit(0);
}
