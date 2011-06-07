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

import java.io.IOException;
import java.io.OutputStream;

import org.ietf.jgss.GSSException;

/**
 * Represents a remctl wire token. All protocol tokens inherit from this class,
 * which also provides static factory methods to read tokens from a stream.
 */
public interface RemctlToken {
    /**
     * The maximum size of the data payload of a remctl token as specified by
     * the protocol standard. Each token has a five-byte header (a one byte
     * flags value and a four byte length), followed by the data, and the total
     * token size may not exceed 1MiB, so {@value} is the maximum size of the
     * data payload.
     */
    public static final int maxData = (1024 * 1024) - 5;

    /**
     * The highest protocol version supported by this implementation, currently
     * * {@value} .
     */
    public static final int supportedVersion = 2;

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
    public byte[] write(OutputStream stream)
            throws GSSException, IOException;

}