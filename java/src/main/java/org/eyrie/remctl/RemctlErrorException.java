/*
 * Written by Russ Allbery <rra@stanford.edu>
 * Copyright 2010 Board of Trustees, Leland Stanford Jr. University
 * 
 * See LICENSE for licensing terms.
 */

package org.eyrie.remctl;

/**
 * An exception representing a remctl protocol error.
 * 
 * @author Russ Allbery &lt;rra@stanford.edu&gt;
 */
public class RemctlErrorException extends RemctlException {
    /** Object ID for serialization. */
    private static final long serialVersionUID = -2935474927873780821L;

    /** Protocol error code corresponding to this exception. */
    private final RemctlErrorCode code;

    /**
     * Constructs a <code>RemctlErrorException</code> from the provided
     * {@link RemctlErrorCode}.
     * 
     * @param code
     *            The RemctlErrorCode constant for the error
     */
    public RemctlErrorException(RemctlErrorCode code) {
        super(code.description);
        this.code = code;
    }

    /**
     * Return a string representation of the <code>RemctlErrorException</code>,
     * which will include the description of the error code (from the protocol
     * specification) and the numeric error code.
     * 
     * @return String representation of the underlying error code
     */
    @Override
    public String getMessage() {
        return this.code.description + " (error " + this.code.value + ")";
    }

    /**
     * Return the specific protocol error code.
     * 
     * @return The RemctlErrorCode constant for the error
     */
    public RemctlErrorCode getErrorCode() {
        return this.code;
    }
}
