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

package org.eyrie.remctl;

import static org.eyrie.remctl.RemctlErrorCode.ERROR_BAD_TOKEN;

import java.io.ByteArrayInputStream;
import java.io.ByteArrayOutputStream;
import java.io.DataInputStream;
import java.io.DataOutputStream;
import java.io.IOException;
import java.io.OutputStream;
import java.io.UnsupportedEncodingException;
import java.lang.reflect.Constructor;
import java.util.Arrays;
import java.util.Collections;
import java.util.EnumMap;
import java.util.HashMap;
import java.util.Map;

import org.ietf.jgss.GSSContext;
import org.ietf.jgss.GSSException;
import org.ietf.jgss.MessageProp;

/**
 * Holds the flags that may make up the flag octet of a remctl token. These
 * flags should be combined with xor. Only TOKEN_CONTEXT, TOKEN_CONTEXT_NEXT,
 * TOKEN_DATA, and TOKEN_PROTOCOL are used for version 2 packets. The other
 * flags are used only with the legacy version 1 protocol.
 */
enum RemctlFlag {
    /** Sent by client in the first packet at the start of the session. */
    TOKEN_NOOP(0x01),

    /** Used for all tokens during initial context setup. */
    TOKEN_CONTEXT(0x02),

    /** Protocol v1: regular data packet from server or client. */
    TOKEN_DATA(0x04),

    /** Protocol v1: MIC token from server. */
    TOKEN_MIC(0x08),

    /** Sent by client in the first packet at the start of the session. */
    TOKEN_CONTEXT_NEXT(0x10),

    /** Protocol v1: client requests server send a MIC in reply. */
    TOKEN_SEND_MIC(0x20),

    /** Protocol v2: set on all protocol v2 tokens. */
    TOKEN_PROTOCOL(0x40);

    /** The wire representation of this flag. */
    byte value;

    /**
     * Create #RemctlFlag with a particular value.
     * 
     * @param value
     *            Byte value of flag.
     */
    private RemctlFlag(int value) {
        this.value = (byte) value;
    }
}

/**
 * Represents the types of messages that are supported by the remctl protocol.
 * Each message sent by a server or client after the initial GSS-API
 * authentication will be one of these types.
 */
enum RemctlMessageCode {
    /** Command to run (client to server). */
    MESSAGE_COMMAND(1),

    /** Client wants to terminate the session (client to server). */
    MESSAGE_QUIT(2),

    /** Output from command (server to client). */
    MESSAGE_OUTPUT(3),

    /**
     * Exit status of command, marking the end of the command (server to client)
     */
    MESSAGE_STATUS(4),

    /** Error in previous command (server to client). */
    MESSAGE_ERROR(5),

    /** Highest supported protocol version (server to client). */
    MESSAGE_VERSION(6);

    /** The wire representation of this message code. */
    byte value;

    /**
     * Create #RemctlMessageCode with a specific value.
     * 
     * @param value
     *            Byte value of message code.
     */
    private RemctlMessageCode(int value) {
        this.value = (byte) value;
    }

    /** Map of wire byte values to #RemctlMessageCode objects. */
    private static final Map<Byte, RemctlMessageCode> codes;
    static {
        Map<Byte, RemctlMessageCode> m = new HashMap<Byte, RemctlMessageCode>();
        for (RemctlMessageCode c : RemctlMessageCode.class.getEnumConstants()) {
            m.put(c.value, c);
        }
        codes = Collections.unmodifiableMap(m);
    }

    /**
     * Given the wire representation of a message code, return the corresponding
     * enum constant.
     * 
     * @param code
     *            Protocol constant corresponding to a message type.
     * @return The enum constant for that message type or null if that constant
     *         does not correspond to a message type.
     */
    static RemctlMessageCode getCode(int code) {
        if (code < 1 || code > 127)
            return null;
        return codes.get((byte) code);
    }
}

/**
 * Represents a remctl wire token. All protocol tokens inherit from this class,
 * which also provides static factory methods to read tokens from a stream.
 */
abstract class RemctlToken {
    /**
     * The maximum size of the data payload of a remctl token as specified by
     * the protocol standard. Each token has a five-byte header (a one byte
     * flags value and a four byte length), followed by the data, and the total
     * token size may not exceed 1MiB, so {@value} is the maximum size of the
     * data payload.
     */
    static final int maxData = (1024 * 1024) - 5;

    /**
     * The highest protocol version supported by this implementation, currently
     * * {@value} .
     */
    static final int supportedVersion = 2;

    /**
     * Encode the token, encrypting it if necessary, and write it to the
     * provided output stream.
     * 
     * @param stream
     *            Stream to which to write the token
     * @throws IOException
     *             An error occurred writing the token to the stream
     * @throws GSSException
     *             On errors encrypting the token
     */
    abstract void write(OutputStream stream)
            throws GSSException, IOException;

    /**
     * Reads a token from a stream, creates a corresponding token object, and
     * returns that object.
     * 
     * @param stream
     *            Stream from which to read the token
     * @param context
     *            GSS-API context for decryption of token
     * @return Token read from the stream
     * @throws IOException
     *             An error occurred reading from the stream
     * @throws GSSException
     *             GSS-API error decoding the token
     * @throws RemctlException
     *             Protocol error in the token read from the stream
     */
    static RemctlToken getToken(DataInputStream stream, GSSContext context)
            throws IOException, GSSException, RemctlException {
        byte flag = stream.readByte();
        if ((flag & RemctlFlag.TOKEN_PROTOCOL.value) == 0) {
            throw new RemctlProtocolException("Protocol v1 not supported");
        }
        //FIXME: what should this be
        if ((flag & RemctlFlag.TOKEN_PROTOCOL.value) == RemctlFlag.TOKEN_PROTOCOL.value) {
            int length = stream.readInt();
            if (length < 0 || length > maxData) {
                throw new RemctlErrorException(ERROR_BAD_TOKEN);
            }
            byte token[] = new byte[length];
            stream.readFully(token);
            return RemctlMessageToken.parseToken(context, token);
        } else {
            throw new RemctlErrorException(ERROR_BAD_TOKEN);
        }
    }
}

/**
 * Represents an encrypted remctl message token. This is the parent class for
 * all remctl tokens except for the initial session establishment tokens. It
 * holds the GSS-API context that should be used to encrypt and decrypt the
 * token and supports a factory method to decrypt a token and then create the
 * appropriate message token type.
 */
class RemctlMessageToken extends RemctlToken {
    /** GSS-API context used to encrypt and decrypt the token. */
    private final GSSContext context;

    /** Protocol version of token. */
    private final byte version;

    /** Type code of message. */
    private final RemctlMessageCode type;

    /** Data payload of token. */
    private byte[] data;

    /** Map of message codes to classes representing that message. */
    private static final Map<RemctlMessageCode, Class<? extends RemctlToken>> messageClasses;
    static {
        EnumMap<RemctlMessageCode, Class<? extends RemctlToken>> m = new EnumMap<RemctlMessageCode, Class<? extends RemctlToken>>(
                RemctlMessageCode.class);
        m.put(RemctlMessageCode.MESSAGE_COMMAND, RemctlCommandToken.class);
        m.put(RemctlMessageCode.MESSAGE_QUIT, RemctlQuitToken.class);
        m.put(RemctlMessageCode.MESSAGE_OUTPUT, RemctlOutputToken.class);
        m.put(RemctlMessageCode.MESSAGE_STATUS, RemctlStatusToken.class);
        m.put(RemctlMessageCode.MESSAGE_ERROR, RemctlErrorToken.class);
        m.put(RemctlMessageCode.MESSAGE_VERSION, RemctlVersionToken.class);
        messageClasses = Collections.unmodifiableMap(m);
    }

    /**
     * Construct a message token with the given GSS-API context, version, and
     * type but no private data. This constructor should be used by child
     * classes to initialize those fields in the token.
     * 
     * @param context
     *            GSS-API context used for encryption
     * @param version
     *            The protocol version required to understand this token
     * @param type
     *            The message type
     * @throws RemctlException
     *             If version is out of range. The version is not checked
     *             against {@link RemctlToken#supportedVersion}.
     */
    RemctlMessageToken(GSSContext context, int version, RemctlMessageCode type)
            throws RemctlException {
        if (version < 2 || version > 127) {
            throw new RemctlProtocolException("Invalid protocol version "
                    + version);
        }
        this.context = context;
        this.version = (byte) version;
        this.type = type;
    }

    /**
     * FIXME We can't force a constructor signature on subclasses. I think we
     * are looking for an AbstractFacotry for 'forcing' a standard way to build
     * things, but for now we will just implement the constructor on all
     * subclasses.
     * 
     * Construct a message token from a byte array. This constructor is mostly
     * useless in <code>RemctlMessageToken</code>. It is here to be overridden
     * by subclasses and is the constructor used to create new classes from data
     * read from a stream.
     * 
     * @param context
     *            GSS-API context used for encryption
     * @param data
     *            The message-specific data in the token
     * @throws RemctlException
     *             The message-specific data is invalid for this type of token
     *             or cannot be parsed
     */
    RemctlMessageToken(GSSContext context, byte[] data) throws RemctlException {
        this.context = context;
        this.version = 2;
        this.type = RemctlMessageCode.MESSAGE_ERROR;
    }

    /**
     * Construct a message token with the given GSS-API context, version, type,
     * and data. This constructor is provided to write pure byte strings as
     * message tokens and is useful for testing. It should generally not be
     * provided by subclasses.
     * 
     * @param context
     *            GSS-API context used for encryption
     * @param version
     *            The protocol version required to understand this token
     * @param type
     *            The message type
     * @param data
     *            The message-specific data in the token
     * @throws RemctlException
     *             If version is out of range. The version is not checked
     *             against {@link RemctlToken#supportedVersion}.
     */
    RemctlMessageToken(GSSContext context, int version, RemctlMessageCode type,
            byte[] data) throws RemctlException {
        this(context, version, type);
        this.data = data;
    }

    /**
     * Encrypt the token and write it to the provided stream.
     * 
     * @param stream
     *            Stream to which to write the token
     * @throws IOException
     *             An error occurred writing the token to the stream
     * @throws GSSException
     *             On errors encrypting the token
     */
    @Override
    void write(OutputStream stream) throws GSSException, IOException {
        int length = this.length() + 2;
        ByteArrayOutputStream array = new ByteArrayOutputStream(length);
        DataOutputStream encode = new DataOutputStream(array);
        try {
            encode.writeByte(this.version);
            encode.writeByte(this.type.value);
            this.writeData(encode);
        } catch (IOException e) {
            // It should be impossible for writes to a ByteArrayOutputStream
            // to fail, so turn them into runtime exceptions.
            throw new RuntimeException(e);
        }
        MessageProp prop = new MessageProp(0, true);
        byte[] message = array.toByteArray();
        byte[] encryptedToken = this.context.wrap(message, 0, message.length,
                prop);

        DataOutputStream outStream = new DataOutputStream(stream);
        outStream.writeByte(RemctlFlag.TOKEN_DATA.value
                      ^ RemctlFlag.TOKEN_PROTOCOL.value);
        outStream.writeInt(encryptedToken.length);
        outStream.write(encryptedToken);
    }

    /**
     * Returns the length of the encoded token. This method is designed to be
     * overridden by child classes of <code>RemctlMessageToken</code> so that
     * the output buffer required for a token can be sized. The fields included
     * in every token are not part of the length.
     * 
     * @return The encoded length of the token
     */
    int length() {
        return this.data.length;
    }

    /**
     * Writes the type-specific data to <code>DataOutputStream</code>. This
     * method is designed to be overridden by child classes of
     * <code>RemctlMessageToken</code> so that child classes don't have to
     * implement the rest of {@link #write(OutputStream)}.
     * 
     * @param stream
     *            Output stream to which to write the token data
     * @throws IOException
     *             On error writing to the stream
     */
    void writeData(DataOutputStream stream) throws IOException {
        stream.write(this.data);
    }

    /**
     * Given a <code>GSSContext</code> and an encrypted token as a byte array,
     * decrypt the token, parse it, construct the appropriate subclass of
     * <code>RemctlMessageToken</code>, and return it.
     * 
     * @param context
     *            GSS-API context to use to decrypt the token
     * @param encryptedToken
     *            An encrypted token
     * @return The decrypted token object
     * @throws GSSException
     *             On errors decrypting the token
     * @throws RemctlException
     *             If the token type is not recognized
     */
    static RemctlToken parseToken(GSSContext context, byte[] encryptedToken)
            throws GSSException, RemctlException {
        MessageProp prop = new MessageProp(0, true);
        byte[] message = context.unwrap(encryptedToken, 0,
                encryptedToken.length, prop);
        byte version = message[0];
        if (version < 1) {
            throw new RemctlProtocolException("Invalid protocol version "
                    + version);
        } else if (version == 1) {
            throw new RemctlProtocolException("Protocol v1 not supported");
        } else if (version > supportedVersion) {
            throw new RemctlProtocolException("Protocol v" + version
                    + "not supported");
        }
        byte code = message[1];
        RemctlMessageCode type = RemctlMessageCode.getCode(code);
        Class<? extends RemctlToken> klass = messageClasses.get(type);
        System.out.println("message class " + klass.getName());
        try {
            // Constructor<?>[] allC = klass.getDeclaredConstructors();
            Constructor<? extends RemctlToken> constructor = klass
                    .getDeclaredConstructor(
                            GSSContext.class, byte[].class);
            RemctlToken tokenClass = constructor.newInstance(context,
                    Arrays.copyOfRange(message, 2, message.length));

            return tokenClass;
        } catch (Exception e) {
            // TODO Auto-generated catch block
            e.printStackTrace();
            throw new IllegalStateException("unable parse input", e);
        }

    }

    /**
     * @return the version
     */
    private byte getVersion() {
        return this.version;
    }

    /**
     * @return the type
     */
    RemctlMessageCode getType() {
        return this.type;
    }
}

/**
 * Represents a remctl command token. This token is sent by a client to a server
 * to run a command on the server.
 */
class RemctlCommandToken extends RemctlMessageToken {
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

/**
 * Represents a remctl output token. This is sent from the server to the client
 * and represents some of the output from a command. Zero or more output tokens
 * may be sent for a command, followed by a status token that indicates the end
 * of output from the command.
 */
class RemctlOutputToken extends RemctlMessageToken {
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
    byte getStream() {
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

/**
 * Represents a remctl status token. This is sent from the server to the client
 * and represents the end of output from a command. It carries the status code
 * of the command, which is zero for success and non-zero for some sort of
 * failure.
 */
class RemctlStatusToken extends RemctlMessageToken {
    /** Status (exit) code of command. */
    private final int status;

    /**
     * Construct a status token with the given status code.
     * 
     * @param context
     *            GSS-API context used for encryption
     * @param status
     *            The exit status code of the command, which must be between 0
     *            and 255, inclusive.
     * @throws RemctlException
     *             Thrown if the status parameter is out of range.
     */
    RemctlStatusToken(GSSContext context, int status)
            throws RemctlException {
        super(context, 2, RemctlMessageCode.MESSAGE_STATUS);
        if (status < 0 || status > 255) {
            throw new RemctlException("status " + status + " out of range");
        }
        this.status = status;
    }

    RemctlStatusToken(GSSContext context, byte[] data)
            throws RemctlException {
        super(context, 2, RemctlMessageCode.MESSAGE_OUTPUT);

        //FIXME: validate data length
        this.status = data[0];

    }

    /**
     * Determine the length of this status message token.
     * 
     * @return The length of the wire representation of the data payload of this
     *         status token
     */
    @Override
    int length() {
        return 1;
    }

    /**
     * Write the wire representation of this status token to the provided output
     * stream.
     * 
     * @param stream
     *            Output stream to which to write the encoded token
     * @throws IOException
     *             On errors writing to the output stream
     */
    @Override
    void writeData(DataOutputStream stream) throws IOException {
        stream.writeByte(this.status);
    }

    /**
     * @return the status
     */
    int getStatus() {
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

    /* (non-Javadoc)
     * @see java.lang.Object#toString()
     */
    @Override
    public String toString() {
        return "RemctlStatusToken [status=" + this.status + "]";
    }
}

/**
 * Represents a remctl error token. This is sent from the server to the client
 * and represents an error in a command.
 */
class RemctlErrorToken extends RemctlMessageToken {

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

/**
 * Represents a remctl quit token. This is sent from the client to the server
 * when it is done issuing commands.
 */
class RemctlQuitToken extends RemctlMessageToken {
    /**
     * Construct a quit token.
     * 
     * @param context
     *            GSS-API context used for encryption
     * @throws RemctlException
     *             Invalid argument to constructor
     */
    RemctlQuitToken(GSSContext context) throws RemctlException {
        super(context, 2, RemctlMessageCode.MESSAGE_QUIT);
    }

    /**
     * Construct a quit token from token data.
     * 
     * @param context
     *            GSS-API context used for encryption.
     * @param data
     *            Token data (must be empty)
     * @throws RemctlException
     *             If the data is not empty
     */
    RemctlQuitToken(GSSContext context, byte[] data) throws RemctlException {
        super(context, data);
        if (data.length != 0)
            throw new RemctlErrorException(RemctlErrorCode.ERROR_BAD_TOKEN);
    }

    /**
     * Determine the length of this quit message token.
     * 
     * @return The length of the wire representation of the data payload of this
     *         quit token
     */
    @Override
    int length() {
        return 0;
    }

    /**
     * Write the wire representation of this quit token to the provided output
     * stream.
     * 
     * @param stream
     *            Output stream to which to write the encoded token
     * @throws IOException
     *             On errors writing to the output stream
     */
    @Override
    void writeData(DataOutputStream stream) throws IOException {
    }
}

/**
 * Represents a remctl version token. This is sent from the server to the client
 * after receiving a message with a protocol version higher than what the server
 * supports.
 */
class RemctlVersionToken extends RemctlMessageToken {
    /** Protocol version stored in the token. */
    private byte highestSupportedVersion;

    /**
     * Construct a version token with the given maximum supported version.
     * 
     * @param context
     *            GSS-API context used for encryption
     * @param version
     *            The maximum supported protocol version
     * @throws RemctlException
     *             If the provided version is not valid
     */
    RemctlVersionToken(GSSContext context, int version)
            throws RemctlException {
        super(context, 2, RemctlMessageCode.MESSAGE_VERSION);
        if (version < 2)
            throw new RemctlProtocolException("Invalid protocol version"
                    + version);
    }

    /**
     * Construct a version token from token data.
     * 
     * @param context
     *            GSS-API context used for encryption.
     * @param data
     *            Token data to parse for version information
     * @throws RemctlException
     *             If the data is not a valid version number
     */
    RemctlVersionToken(GSSContext context, byte[] data) throws RemctlException {
        super(context, data);
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

}