/*
 * Written by Russ Allbery <rra@stanford.edu>
 * Copyright 2010 Board of Trustees, Leland Stanford Jr. University
 * 
 * See LICENSE for licensing terms.
 */

package org.eyrie.remctl.core;

import java.util.HashMap;
import java.util.Map;

/**
 * Holds the protocol-standardized error codes and English error strings for
 * remctl errors that can be sent via a MESSAGE_ERROR token.
 * 
 * FIXME error codes may be extended, and client must accept them. I don't think
 * that would be possible with enums, since the 'accepted' ones are a compile
 * time constant.
 * 
 * @author Russ Allbery &lt;rra@stanford.edu&gt;
 */
public enum RemctlErrorCode {
    /** Unspecified internal error in the server. */
    ERROR_INTERNAL(1, "Internal server failure"),

    /** Token syntax error or other low-level protocol error. */
    ERROR_BAD_TOKEN(2, "Invalid format in token"),

    /** The message type field in the token is invalid. */
    ERROR_UNKNOWN_MESSAGE(3, "Unknown message type"),

    /** Syntax error specific to the construction of a command token. */
    ERROR_BAD_COMMAND(4, "Invalid command format in token"),

    /** The command is not known to the server. */
    ERROR_UNKNOWN_COMMAND(5, "Unknown command"),

    /** The client is not permitted to run that command. */
    ERROR_ACCESS(6, "Access denied"),

    /** The command has too many arguments, exceeding a server limit. */
    ERROR_TOOMANY_ARGS(7, "Argument count exceeds server limit"),

    /**
     * The size of a single argument or the total size of all arguments exceeds
     * a server limit.
     */
    ERROR_TOOMUCH_DATA(8, "Argument size exceeds server limit");

    /** The wire representation of this error code. */
    public byte value;

    /**
     * The English description for this error code from the protocol
     * specification.
     */
    public String description;

    /**
     * Construct a #RemctlErrorCode with the given protocol value and default
     * error message .
     * 
     * @param value
     *            Error code as sent on the wire
     * @param description
     *            Default error message for this error
     */
    private RemctlErrorCode(int value, String description) {
        this.value = (byte) value;
        this.description = description;
    }

    private static final Map<String, RemctlErrorCode> descriptionToCode = new HashMap<String, RemctlErrorCode>();
    private static final Map<Byte, RemctlErrorCode> byteValueToCode = new HashMap<Byte, RemctlErrorCode>();
    static {
        for (RemctlErrorCode errorCode : values()) {
            descriptionToCode.put(errorCode.description, errorCode);
            byteValueToCode.put(errorCode.value, errorCode);
        }
    }

    /**
     * @return The string representation of a status
     */
    @Override
    public String toString() {
        return super.toString().toLowerCase();
    }

    public static RemctlErrorCode fromByte(byte value) {
        return byteValueToCode.get(value);
    }

    public static RemctlErrorCode fromInt(int value) {
        return fromByte((byte) value);
    }
}