package org.eyrie.remctl.core;

import java.io.ByteArrayOutputStream;
import java.io.DataOutputStream;
import java.io.IOException;
import java.io.OutputStream;
import java.lang.reflect.Constructor;
import java.util.Arrays;
import java.util.Collections;
import java.util.EnumMap;
import java.util.Map;

import org.eyrie.remctl.RemctlException;
import org.eyrie.remctl.RemctlProtocolException;
import org.ietf.jgss.GSSContext;
import org.ietf.jgss.GSSException;
import org.ietf.jgss.MessageProp;

/**
 * Represents an encrypted remctl message token. This is the parent class for
 * all remctl tokens except for the initial session establishment tokens. It
 * holds the GSS-API context that should be used to encrypt and decrypt the
 * token and supports a factory method to decrypt a token and then create the
 * appropriate message token type.
 */
public class RemctlMessageToken extends RemctlToken {
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
    public byte[] write(OutputStream stream) throws GSSException, IOException {
        int length = this.length() + 2;
        ByteArrayOutputStream array = new ByteArrayOutputStream(length);
        DataOutputStream encode = new DataOutputStream(array);
        try {
            encode.writeByte(this.version);
            encode.writeByte(this.type.value);
            this.writeData(encode);

            return array.toByteArray();
        } catch (IOException e) {
            // It should be impossible for writes to a ByteArrayOutputStream
            // to fail, so turn them into runtime exceptions.
            throw new RuntimeException(e);
        }
        //        MessageProp prop = new MessageProp(0, true);
        //        byte[] message = array.toByteArray();
        //        byte[] encryptedToken = this.context.wrap(message, 0, message.length,
        //                prop);

        //DataOutputStream outStream = new DataOutputStream(stream);
        //outStream.writeByte(RemctlFlag.TOKEN_DATA.value
        //            ^ RemctlFlag.TOKEN_PROTOCOL.value);
        //outStream.writeInt(encryptedToken.length);
        //outStream.write(encryptedToken);
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