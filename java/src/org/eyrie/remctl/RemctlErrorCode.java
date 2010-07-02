/*
 * Written by Russ Allbery <rra@stanford.edu>
 * Copyright 2010 Board of Trustees, Leland Stanford Jr. University
 * 
 * See LICENSE for licensing terms.
 */

package org.eyrie.remctl;

/**
 * Holds the protocol-standardized error codes and English error strings for
 * remctl errors that can be sent via a MESSAGE_ERROR token.
 * 
 * @author Russ Allbery &lt;rra@stanford.edu&gt;
 */
public enum RemctlErrorCode {
    ERROR_INTERNAL(1, "Internal server failure"),
    ERROR_BAD_TOKEN(2, "Invalid format in token"),
    ERROR_UNKNOWN_MESSAGE(3, "Unknown message type"),
    ERROR_BAD_COMMAND(4, "Invalid command format in token"),
    ERROR_UNKNOWN_COMMAND(5, "Unknown command"),
    ERROR_ACCESS(6, "Access denied"),
    ERROR_TOOMANY_ARGS(7, "Argument count exceeds server limit"),
    ERROR_TOOMUCH_DATA(8, "Argument size exceeds server limit");
    
    /** The wire representation of this error code. */
    byte value;
    
    /**
     * The English description for this error code from the protocol
     * specification.
     */
    String description;

    private RemctlErrorCode(int value, String description) {
        this.value = (byte)value;
        this.description = description;
    }
}