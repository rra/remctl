package org.eyrie.remctl.core;

import java.io.DataOutputStream;
import java.io.IOException;


/**
 * Represents a remctl version token. This is sent from the server to the client
 * after receiving a message with a protocol version higher than what the server
 * supports.
 */
public class RemctlVersionToken extends RemctlMessageToken {
    /** Protocol version stored in the token. */
    private byte highestSupportedVersion;

    /**
     * Construct a version token with the given maximum supported version.
     * 
     * @param version
     *            The maximum supported protocol version
     * @throws RemctlException
     *             If the provided version is not valid
     */
    public RemctlVersionToken(int version)
            throws RemctlException {
        super(2);
        if (version < 2)
            throw new RemctlProtocolException("Invalid protocol version"
                    + version);
    }

    /**
     * Construct a version token from token data.
     * 
     * @param data
     *            Token data to parse for version information
     * @throws RemctlException
     *             If the data is not a valid version number
     */
    RemctlVersionToken(byte[] data) throws RemctlException {
        super(2);
        if (data.length != 1)
            throw new RemctlErrorException(RemctlErrorCode.ERROR_BAD_TOKEN);
        this.highestSupportedVersion = data[0];
    }

    /**
     * Determine the length of this version message token.
     * 
     * @return The length of the wire representation of the data payload of this
     *         version token
     */
    @Override
    int length() {
        return 1;
    }

    /**
     * Write the wire representation of this version token to the provided
     * output stream.
     * 
     * @param stream
     *            Output stream to which to write the encoded token
     * @throws IOException
     *             On errors writing to the output stream
     */
    @Override
    void writeData(DataOutputStream stream) throws IOException {
        stream.writeByte(this.highestSupportedVersion);
    }

    /**
     * @return the highestSupportedVersion
     */
    byte getHighestSupportedVersion() {
        return this.highestSupportedVersion;
    }

    /* (non-Javadoc)
     * @see java.lang.Object#toString()
     */
    @Override
    public String toString() {
        return "RemctlVersionToken [highestSupportedVersion="
                + this.highestSupportedVersion + "]";
    }

    @Override
    RemctlMessageCode getType() {
        return RemctlMessageCode.MESSAGE_VERSION;
    }

}