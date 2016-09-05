/*
 * Written by Russ Allbery <rra@stanford.edu>
 * Copyright 2010 Board of Trustees, Leland Stanford Jr. University
 * 
 * See LICENSE for licensing terms.
 */

package org.eyrie.remctl.core;

/**
 * Parent class for exceptions specific to the remctl protocol or API.
 * 
 * @author Russ Allbery &lt;rra@stanford.edu&gt;
 */
public class RemctlException extends RuntimeException {
    /** Object ID for serialization. */
    private static final long serialVersionUID = -4221280841282852331L;

    /**
     * Constructs a <code>RemctlException</code> with the provided detail string.
     * 
     * @param msg
     *            The detail message
     */
    public RemctlException(final String msg) {
        super(msg);
    }

    /**
     * {@link RuntimeException#RuntimeException(String, Throwable)}.
     *
     * @param msg
     *            The detail message
     * @param t
     *            The root cause
     */
    public RemctlException(final String msg, final Throwable t) {
        super(msg, t);
    }
}
