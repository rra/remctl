package org.eyrie.remctl.core;

import java.io.ByteArrayInputStream;
import java.io.DataInputStream;
import java.io.DataOutputStream;
import java.io.IOException;
import java.io.UnsupportedEncodingException;

import org.eyrie.remctl.RemctlException;
import org.ietf.jgss.GSSContext;

/**
 * Represents a remctl output token. This is sent from the server to the client
 * and represents some of the output from a command. Zero or more output tokens
 * may be sent for a command, followed by a status token that indicates the end
 * of output from the command.
 */
public class RemctlOutputToken extends RemctlMessageToken {
    /** The stream number from which this output came. */
    private final byte stream;

    /** The output from the command. */
    private final byte output[];

    /**
     * Construct an output token with the given output data.
     * 
     * @param context
     *            GSS-API context used for encryption
     * @param stream
     *            The output stream, either 1 for standard output or 2 for
     *            standard error
     * @param output
     *            The output data
     * @throws RemctlException
     *             If the stream is not 1 or 2
     */
    RemctlOutputToken(GSSContext context, int stream, byte output[])
            throws RemctlException {
        super(context, 2, RemctlMessageCode.MESSAGE_OUTPUT);
        if (stream != 1 && stream != 2) {
            throw new RemctlException("invalid stream" + stream);
        }
        this.stream = (byte) stream;
        this.output = output;
    }

    /**
     * Determine the length of this output message token.
     * 
     * @return The length of the wire representation of the data payload of this
     *         output token
     */
    @Override
    int length() {
        return 1 + 4 + this.output.length;
    }

    /**
     * Write the wire representation of this output token to the provided output
     * stream.
     * 
     * @param stream
     *            Output stream to which to write the encoded token
     * @throws IOException
     *             On errors writing to the output stream
     */
    @Override
    void writeData(DataOutputStream stream) throws IOException {
        stream.writeByte(this.stream);
        stream.writeInt(this.output.length);
        stream.write(this.output);
    }

    RemctlOutputToken(GSSContext context, byte[] data)
            throws RemctlException {
        super(context, 2, RemctlMessageCode.MESSAGE_OUTPUT);
        DataInputStream stream = new DataInputStream(new ByteArrayInputStream(
                data));
        try {
            this.stream = stream.readByte();
            int length = stream.readInt();
            this.output = new byte[length];
            stream.readFully(this.output, 0, length);
        } catch (IOException e) {
            throw new IllegalStateException(e);
        }
    }

    /**
     * @return the stream
     */
    public byte getStream() {
        return this.stream;
    }

    /**
     * @return the output
     */
    byte[] getOutput() {
        return this.output;
    }

    String getOutputAsString() {
        try {
            return new String(this.output, "UTF-8");
        } catch (UnsupportedEncodingException e) {
            // TODO Auto-generated catch block
            e.printStackTrace();
            throw new RuntimeException(e);
        }
    }

    /* (non-Javadoc)
     * @see java.lang.Object#toString()
     */
    @Override
    public String toString() {
        return "RemctlOutputToken [stream=" + this.stream + ", output size ="
                + this.output.length + "]";
    }

}