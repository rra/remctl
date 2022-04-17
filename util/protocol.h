/*
 * Internal remctl protocol constants.
 *
 * Various constants and values used throughout the remctl source.  This
 * should eventually move into a public header file.
 *
 * Written by Russ Allbery <eagle@eyrie.org>
 * Based on prior work by Anton Ushakov
 * Copyright 2020, 2022 Russ Allbery <eagle@eyrie.org>
 * Copyright 2002-2011, 2014
 *     The Board of Trustees of the Leland Stanford Junior University
 *
 * SPDX-License-Identifier: MIT
 */

#ifndef UTIL_PROTOCOL_H
#define UTIL_PROTOCOL_H 1

/* Message types. */
/* clang-format off */
enum message_types {
    MESSAGE_COMMAND = 1,
    MESSAGE_QUIT    = 2,
    MESSAGE_OUTPUT  = 3,
    MESSAGE_STATUS  = 4,
    MESSAGE_ERROR   = 5,
    MESSAGE_VERSION = 6,
    MESSAGE_NOOP    = 7
};
/* clang-format on */

/* Maximum lengths from the protocol specification of tokens and data. */
#define TOKEN_MAX_LENGTH    (1024 * 1024)
#define TOKEN_MAX_DATA      (64 * 1024)

/*
 * Maximum data payload for a MESSAGE_OUTPUT message, which is TOKEN_MAX_DATA
 * minus the overhead for MESSAGE_OUTPUT labeling.  This is slightly different
 * in protocol one, which used a different message format.
 */
#define TOKEN_MAX_OUTPUT    (TOKEN_MAX_DATA - 1 - 1 - 1 - 4)
#define TOKEN_MAX_OUTPUT_V1 (TOKEN_MAX_DATA - 4 - 4)

/* Windows uses this for something else. */
#ifdef _WIN32
#    undef ERROR_BAD_COMMAND
#endif

/* Error codes. */
/* clang-format off */
enum error_codes {
    ERROR_INTERNAL           = 1, /* Internal server failure. */
    ERROR_BAD_TOKEN          = 2, /* Invalid format in token. */
    ERROR_UNKNOWN_MESSAGE    = 3, /* Unknown message type. */
    ERROR_BAD_COMMAND        = 4, /* Invalid command format in token. */
    ERROR_UNKNOWN_COMMAND    = 5, /* Unknown command. */
    ERROR_ACCESS             = 6, /* Access denied. */
    ERROR_TOOMANY_ARGS       = 7, /* Argument count exceeds server limit. */
    ERROR_TOOMUCH_DATA       = 8, /* Argument size exceeds server limit. */
    ERROR_UNEXPECTED_MESSAGE = 9, /* Message type not valid now. */
    ERROR_NO_HELP            = 10 /* No help defined for this command. */
};
/* clang-format on */

#endif /* UTIL_PROTOCOL_H */
