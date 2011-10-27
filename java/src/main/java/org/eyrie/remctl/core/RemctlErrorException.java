/*
 * Written by Russ Allbery <rra@stanford.edu>
 * Copyright 2010 Board of Trustees, Leland Stanford Jr. University
 * 
 * See LICENSE for licensing terms.
 */

package org.eyrie.remctl.core;


/**
 * An exception representing a remctl protocol error, or error token
 * 
 * @author Russ Allbery &lt;rra@stanford.edu&gt;
 */
public class RemctlErrorException extends RemctlException {
    /** Object ID for serialization. */
    private static final long serialVersionUID = -2935474927873780821L;

    /** the error code **/
    private final byte code;

    /** the error message **/
    private final String message;

    /**
     * Constructs a <code>RemctlErrorException</code> from the provided
     * {@link RemctlErrorCode}.
     * 
     * @param code
     *            The RemctlErrorCode constant for the error
     */
    public RemctlErrorException(RemctlErrorCode code) {
        super(code.description);
        this.code = code.value;
        this.message = code.description;
    }

    /**
     * Constructs a <code>RemctlErrorException</code> from the provided
     * {@link RemctlErrorToken}.
     * 
     * @param token
     *            The RemctlErrorToken of the error.
     */
    public RemctlErrorException(RemctlErrorToken token) {
        super(token.getMessage());
        this.code = token.getCode().value;
        this.message = token.getMessage();

    }

    /**
     * Constructs a <code>RemctlErrorException</code> from the provided the code
     * and message
     * 
     * @param code
     *            The code for the error
     * @param message
     *            The message
     */
    public RemctlErrorException(int code, String message) {
        super(message);
        this.code = (byte) code;
        this.message = message;

    }

    /**
     * Constructs a <code>RemctlErrorException</code> from the provided
     * {@link RemctlErrorCode}, but using the alternate error message
     * 
     * @param code
     *            The RemctlErrorCode constant for the error
     * @param message
     *            The alternate error message
     */
    public RemctlErrorException(RemctlErrorCode code, String message) {
        this(code.value, message);
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
        return this.message + " (error " + this.code + ")";
    }

    /**
     * Return the specific protocol error code.
     * 
     * @return The error code
     */
    public byte getErrorCode() {
        return this.code;
    }
}
