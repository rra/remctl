/*
 * 
 * FIXME: remctl token is still part of public API incase some one wants
 * to use the lower level RemctlConnection
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

import java.io.IOException;

import org.ietf.jgss.GSSException;

/**
 * Represents a remctl wire token. All protocol tokens inherit from this class, which also provides static factory
 * methods to read tokens from a stream.
 */
public interface RemctlToken {
    /**
     * The maximum size of the data payload of a remctl token as specified by the protocol standard. Each token has a
     * five-byte header (a one byte flags value and a four byte length), followed by the data, and the total token size
     * may not exceed 1MiB, so {@value} is the maximum size of the data payload.
     */
    int MAX_DATA = (1024 * 1024) - 5;

    /**
     * The highest protocol version supported by this implementation, currently * {@value} .
     */
    int SUPPORTED_VERSION = 2;

    /**
     * Encode the token, encrypting it if necessary, and write it to the provided output stream.
     * 
     * @return The encoded token as a byte array
     * @throws IOException
     *             An error occurred writing the token to the stream
     * @throws GSSException
     *             On errors encrypting the token
     */
    byte[] write() throws GSSException, IOException;

}
