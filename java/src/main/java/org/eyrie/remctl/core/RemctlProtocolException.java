/*
 * Written by Russ Allbery <rra@stanford.edu>
 * Copyright 2010 Board of Trustees, Leland Stanford Jr. University
 * 
 * See LICENSE for licensing terms.
 */

package org.eyrie.remctl.core;


/**
 * Parent class for protocol exceptions, thrown if we encounter a token from an
 * unknown or unsupported version of the protocol.
 * 
 * @author Russ Allbery &lt;rra@stanford.edu&gt;
 */
public class RemctlProtocolException extends RemctlException {
    /** Object ID for serialization. */
    private static final long serialVersionUID = 4505549867268445774L;

    /**
     * Constructs a <code>RemctlProtocolException</code> with the specified
     * detail message.
     * 
     * @param msg
     *            The detail message
     */
    public RemctlProtocolException(String msg) {
        super(msg);
    }
}
