package org.eyrie.remctl.core;

import java.io.DataOutputStream;
import java.io.IOException;

import org.eyrie.remctl.RemctlException;
import org.ietf.jgss.GSSContext;

/**
 * Represents a remctl command token. This token is sent by a client to a server
 * to run a command on the server.
 */
public class RemctlCommandToken extends RemctlMessageToken {
    /** Whether the keep-alive flag is set on the message. */
    boolean keepalive;

    /** The arguments of the command. */
    private final String args[];

    /**
     * Construct a command token with the given keep-alive status and command
     * arguments.
     * 
     * @param context
     *            GSS-API context used for encryption
     * @param keepalive
     *            Whether to keep the connection open after completing this
     *            command
     * @param args
     *            Command and arguments
     * @throws RemctlException
     *             Invalid argument to constructor
     */
    RemctlCommandToken(GSSContext context, boolean keepalive, String... args)
            throws RemctlException {
        super(context, 2, RemctlMessageCode.MESSAGE_COMMAND);
        this.keepalive = keepalive;
        this.args = args;
    }

    /**
     * Determine the length of this command message token.
     * 
     * @return The length of the wire representation of the data payload of this
     *         command token
     */
    @Override
    int length() {
        int size = 1 + 1 + 4;
        for (String arg : this.args) {
            size += 4 + arg.length();
        }
        return size;
    }

    /**
     * Write the wire representation of this command token to the provided
     * output stream.
     * 
     * @param output
     *            Output stream to which to write the encoded token
     * @throws IOException
     *             On errors writing to the output stream
     */
    @Override
    protected void writeData(DataOutputStream output) throws IOException {
        output.writeByte(this.keepalive ? 1 : 0);
        output.writeByte(0);
        output.writeInt(this.args.length);
        for (String arg : this.args) {
            //FIXME these calls assume one byte per character, and we should support UTF-8
            output.writeInt(arg.length());
            output.writeBytes(arg);
        }
    }
}