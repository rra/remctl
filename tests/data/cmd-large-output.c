/*
 * Small C program to output an arbitrary amount of data to standard output
 * and then exit successfully.  Used to verify that we don't truncate data
 * output right before process close.
 *
 * Written by Russ Allbery <eagle@eyrie.org>
 * Copyright 2013
 *     The Board of Trustees of the Leland Stanford Junior University
 *
 * SPDX-License-Identifier: MIT
 */

#include <config.h>
#include <portable/system.h>

#include <util/xwrite.h>


int
main(int argc, char *argv[])
{
    unsigned long length;
    char *data;

    if (argc != 3) {
        fprintf(stderr, "invalid arguments\n");
        exit(1);
    }
    length = strtoul(argv[2], NULL, 10);
    if (length == 0) {
        fprintf(stderr, "invalid data length\n");
        exit(1);
    }
    data = malloc(length);
    memset(data, '1', length);
    xwrite(STDOUT_FILENO, data, length);
    exit(0);
}
