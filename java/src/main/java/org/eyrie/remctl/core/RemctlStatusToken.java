package org.eyrie.remctl.core;

import java.io.DataOutputStream;
import java.io.IOException;

/**
 * Represents a remctl status token. This is sent from the server to the client and represents the end of output from a
 * command. It carries the status code of the command, which is zero for success and non-zero for some sort of failure.
 */
public class RemctlStatusToken extends RemctlMessageToken {
    /** Status (exit) code of command. */
    private final int status;

    /**
     * Construct a status token with the given status code.
     * 
     * @param status
     *            The exit status code of the command, which must be between 0 and 255, inclusive.
     * @throws RemctlException
     *             Thrown if the status parameter is out of range.
     */
    public RemctlStatusToken(final int status) throws RemctlException {
        super(2);
        if (status < 0 || status > 255) {
            throw new RemctlException("status " + status + " out of range");
        }
        this.status = status;
    }

    /**
     * Create a Status token from command specific bytes.
     * 
     * @param data
     *            command specific bytes
     * @throws RemctlErrorException
     *             If bytes make an invalid token
     */
    public RemctlStatusToken(final byte[] data) throws RemctlErrorException {
        super(2);
        if (data.length != 1)
            throw new RemctlErrorException(RemctlErrorCode.ERROR_BAD_TOKEN, "Expected size 1, byte length "
                    + data.length);

        this.status = data[0];

    }

    /**
     * Determine the length of this status message token.
     * 
     * @return The length of the wire representation of the data payload of this status token
     */
    @Override
    int length() {
        return 1;
    }

    /**
     * Write the wire representation of this status token to the provided output stream.
     * 
     * @param stream
     *            Output stream to which to write the encoded token
     * @throws IOException
     *             On errors writing to the output stream
     */
    @Override
    void writeData(final DataOutputStream stream) throws IOException {
        stream.writeByte(this.status);
    }

    /**
     * @return the status
     */
    public int getStatus() {
        return this.status;
    }

    /**
     * Check if the status was success
     * 
     * @return true if status was 0, or false for non-zero return codes.
     */
    boolean isSuccessful() {
        return 0 == this.status;
    }

    /*
     * (non-Javadoc)
     * 
     * @see java.lang.Object#toString()
     */
    @Override
    public String toString() {
        return "RemctlStatusToken [status=" + this.status + "]";
    }

    @Override
    RemctlMessageCode getType() {
        return RemctlMessageCode.MESSAGE_STATUS;
    }
}
