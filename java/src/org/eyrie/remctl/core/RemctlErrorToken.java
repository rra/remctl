package org.eyrie.remctl.core;

import java.io.ByteArrayInputStream;
import java.io.DataInputStream;
import java.io.DataOutputStream;
import java.io.IOException;

import org.eyrie.remctl.RemctlErrorCode;
import org.eyrie.remctl.RemctlException;
import org.ietf.jgss.GSSContext;

/**
 * Represents a remctl error token. This is sent from the server to the client
 * and represents an error in a command.
 */
public class RemctlErrorToken extends RemctlMessageToken {

    /** Error code, used by software to identify the error. */
    private final RemctlErrorCode code;

    /** Human-readable description of the error returned by server. */
    private final String message;

    /**
     * Minimum size in octets. 4 for error code, 4 for message length
     */
    static private final int MIN_SIZE = 8;

    /**
     * Construct an error token with the given error code and message.
     * 
     * @param context
     *            GSS-API context used for encryption
     * @param code
     *            The remctl error code
     * @param message
     *            The human-readable error message
     * @throws RemctlException
     *             Invalid arguments to constructor
     */
    RemctlErrorToken(GSSContext context, RemctlErrorCode code, String message)
            throws RemctlException {
        super(context, 2, RemctlMessageCode.MESSAGE_ERROR);
        this.code = code;
        this.message = message;
    }

    /**
     * Construct an error token with the given error code. The error message
     * will be the stock error message for that code.
     * 
     * @param context
     *            GSS-API context used for encryption
     * @param code
     *            The remctl error code
     * @throws RemctlException
     *             Invalid argument to constructor
     */
    RemctlErrorToken(GSSContext context, RemctlErrorCode code)
            throws RemctlException {
        this(context, code, code.description);
    }

    RemctlErrorToken(GSSContext context, byte[] data)
            throws RemctlException {

        super(context, 2, RemctlMessageCode.MESSAGE_OUTPUT);
        if (data.length < MIN_SIZE) {
            throw new IllegalArgumentException(
                    "Command data size is to small. Expected " + MIN_SIZE
                            + ", but was " + data.length);
        }

        DataInputStream stream = new DataInputStream(new ByteArrayInputStream(
                data));
        try {
            int codeInt = stream.readInt();
            this.code = RemctlErrorCode.fromInt(codeInt);
            //TODO: length is signed, so we'll overflow to a negative length for large length, but I think
            //max packet format length is 1,048,576 which easily fits in an int, so this is probably a non issue
            int length = stream.readInt();
            if (length != data.length - 8) {
                throw new IllegalStateException(
                        "Expected length mismatch: Command length is " + length
                                + ", but data length is "
                                + (data.length - MIN_SIZE));
            }
            //            if (length == 0) {
            //                this.message = "";
            //            } else {
            byte[] messageBytes = new byte[length];
            stream.readFully(messageBytes, 0, length);
            this.message = new String(messageBytes,
                        "UTF-8");
            //            }

        } catch (IOException e) {
            throw new IllegalStateException(e);
        }
    }

    /**
     * Determine the length of this error message token.
     * 
     * @return The length of the wire representation of the data payload of this
     *         error token
     */
    @Override
    int length() {
        return 4 + 4 + this.message.length();
    }

    /**
     * Write the wire representation of this error token to the provided output
     * stream.
     * 
     * @param stream
     *            Output stream to which to write the encoded token
     * @throws IOException
     *             On errors writing to the output stream
     */
    @Override
    void writeData(DataOutputStream stream) throws IOException {
        stream.writeInt(this.code.value);
        stream.writeInt(this.message.length());
        stream.writeBytes(this.message);
    }

    /* (non-Javadoc)
     * @see java.lang.Object#toString()
     */
    @Override
    public String toString() {
        return "RemctlErrorToken [code=" + this.code + ", message="
                + this.message + "]";
    }

    /**
     * @return the code
     */
    public RemctlErrorCode getCode() {
        return this.code;
    }

    /**
     * @return the message
     */
    public String getMessage() {
        return this.message;
    }

}