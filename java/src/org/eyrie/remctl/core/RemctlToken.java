/*
 * remctl token classes.
 * 
 * This file contains the definitions of the classes that represent wire
 * tokens in the remctl protocol.  It is internal to the Java remctl
 * implementation and is not part of the public API.
 *
 * Written by Russ Allbery <rra@stanford.edu>
 * Copyright 2010 Board of Trustees, Leland Stanford Jr. University
 * 
 * See LICENSE for licensing terms.
 */

package org.eyrie.remctl.core;

import static org.eyrie.remctl.RemctlErrorCode.ERROR_BAD_TOKEN;

import java.io.DataInputStream;
import java.io.IOException;
import java.io.OutputStream;
import java.util.Collections;
import java.util.HashMap;
import java.util.Map;

import org.eyrie.remctl.RemctlErrorException;
import org.eyrie.remctl.RemctlException;
import org.eyrie.remctl.RemctlProtocolException;
import org.ietf.jgss.GSSContext;
import org.ietf.jgss.GSSException;

/**
 * Represents the types of messages that are supported by the remctl protocol.
 * Each message sent by a server or client after the initial GSS-API
 * authentication will be one of these types.
 */
enum RemctlMessageCode {
    /** Command to run (client to server). */
    MESSAGE_COMMAND(1),

    /** Client wants to terminate the session (client to server). */
    MESSAGE_QUIT(2),

    /** Output from command (server to client). */
    MESSAGE_OUTPUT(3),

    /**
     * Exit status of command, marking the end of the command (server to client)
     */
    MESSAGE_STATUS(4),

    /** Error in previous command (server to client). */
    MESSAGE_ERROR(5),

    /** Highest supported protocol version (server to client). */
    MESSAGE_VERSION(6);

    /** The wire representation of this message code. */
    byte value;

    /**
     * Create #RemctlMessageCode with a specific value.
     * 
     * @param value
     *            Byte value of message code.
     */
    private RemctlMessageCode(int value) {
        this.value = (byte) value;
    }

    /** Map of wire byte values to #RemctlMessageCode objects. */
    private static final Map<Byte, RemctlMessageCode> codes;
    static {
        Map<Byte, RemctlMessageCode> m = new HashMap<Byte, RemctlMessageCode>();
        for (RemctlMessageCode c : RemctlMessageCode.class.getEnumConstants()) {
            m.put(c.value, c);
        }
        codes = Collections.unmodifiableMap(m);
    }

    /**
     * Given the wire representation of a message code, return the corresponding
     * enum constant.
     * 
     * @param code
     *            Protocol constant corresponding to a message type.
     * @return The enum constant for that message type or null if that constant
     *         does not correspond to a message type.
     */
    static RemctlMessageCode getCode(int code) {
        if (code < 1 || code > 127)
            return null;
        return codes.get((byte) code);
    }
}

/**
 * Represents a remctl wire token. All protocol tokens inherit from this class,
 * which also provides static factory methods to read tokens from a stream.
 */
public abstract class RemctlToken {
    /**
     * The maximum size of the data payload of a remctl token as specified by
     * the protocol standard. Each token has a five-byte header (a one byte
     * flags value and a four byte length), followed by the data, and the total
     * token size may not exceed 1MiB, so {@value} is the maximum size of the
     * data payload.
     */
    static final int maxData = (1024 * 1024) - 5;

    /**
     * The highest protocol version supported by this implementation, currently
     * * {@value} .
     */
    static final int supportedVersion = 2;

    /**
     * Encode the token, encrypting it if necessary, and write it to the
     * provided output stream.
     * 
     * @param stream
     *            Stream to which to write the token
     * @return
     * @throws IOException
     *             An error occurred writing the token to the stream
     * @throws GSSException
     *             On errors encrypting the token
     */
    public abstract byte[] write(OutputStream stream)
            throws GSSException, IOException;

    /**
     * Reads a token from a stream, creates a corresponding token object, and
     * returns that object.
     * 
     * @param stream
     *            Stream from which to read the token
     * @param context
     *            GSS-API context for decryption of token
     * @return Token read from the stream
     * @throws IOException
     *             An error occurred reading from the stream
     * @throws GSSException
     *             GSS-API error decoding the token
     * @throws RemctlException
     *             Protocol error in the token read from the stream
     */
    public static RemctlToken getToken(DataInputStream stream,
            GSSContext context)
            throws IOException, GSSException, RemctlException {
        byte flag = stream.readByte();
        if ((flag & RemctlFlag.TOKEN_PROTOCOL.value) == 0) {
            throw new RemctlProtocolException("Protocol v1 not supported");
        }
        //FIXME: what should this be
        if ((flag & RemctlFlag.TOKEN_PROTOCOL.value) == RemctlFlag.TOKEN_PROTOCOL.value) {
            int length = stream.readInt();
            if (length < 0 || length > maxData) {
                throw new RemctlErrorException(ERROR_BAD_TOKEN);
            }
            byte token[] = new byte[length];
            stream.readFully(token);
            return RemctlMessageToken.parseToken(context, token);
        } else {
            throw new RemctlErrorException(ERROR_BAD_TOKEN);
        }
    }
}