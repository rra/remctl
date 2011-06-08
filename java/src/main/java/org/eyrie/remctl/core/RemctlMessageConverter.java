package org.eyrie.remctl.core;

import java.io.DataInputStream;
import java.io.DataOutputStream;
import java.io.IOException;
import java.io.InputStream;
import java.io.OutputStream;
import java.lang.reflect.Constructor;
import java.util.Arrays;
import java.util.Collections;
import java.util.EnumMap;
import java.util.Map;

import org.eyrie.remctl.RemctlErrorCode;
import org.eyrie.remctl.RemctlErrorException;
import org.eyrie.remctl.RemctlException;
import org.eyrie.remctl.RemctlProtocolException;
import org.ietf.jgss.GSSContext;
import org.ietf.jgss.GSSException;
import org.ietf.jgss.MessageProp;

/**
 * Message encoder turns Remctl messages into Remctl packets.
 * 
 * <p>
 * It uses the GSS context to encode the byte[] representation of a message into
 * the packet payload, and and adds the appropriate protocol bytes (flags,
 * length, etc). For decoding, it decrypts the data payload and converts it into
 * the appropriate message.
 * 
 * @author pradtke
 * 
 */
public class RemctlMessageConverter {

    /**
     * The GSS context used for encrypting/decrypting the payload
     */
    private final GSSContext context;

    /**
     * Create a message converter that uses the context for
     * encrypting/decrypting the payload
     * 
     * @param context
     *            the GSSContext
     */
    public RemctlMessageConverter(GSSContext context) {
        this.context = context;
    }

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
    public static RemctlToken getToken(DataInputStream stream,
            GSSContext context)
            throws IOException, GSSException, RemctlException {
        byte flag = stream.readByte();
        if ((flag & RemctlFlag.TOKEN_PROTOCOL.getValue()) == 0) {
            throw new RemctlProtocolException("Protocol v1 not supported");
        }
        //FIXME: what should this be
        if ((flag & RemctlFlag.TOKEN_PROTOCOL.getValue()) == RemctlFlag.TOKEN_PROTOCOL
                .getValue()) {
            int length = stream.readInt();
            if (length < 0 || length > RemctlToken.maxData) {
                throw new RemctlErrorException(
                        RemctlErrorCode.ERROR_BAD_TOKEN);
            }
            byte token[] = new byte[length];
            stream.readFully(token);
            return parseToken(context, token);
        } else {
            throw new RemctlErrorException(RemctlErrorCode.ERROR_BAD_TOKEN);
        }
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
        } else if (version > RemctlToken.supportedVersion) {
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
                             byte[].class);
            RemctlToken tokenClass = constructor.newInstance(
                    Arrays.copyOfRange(message, 2, message.length));

            return tokenClass;
        } catch (Exception e) {
            // TODO Auto-generated catch block
            e.printStackTrace();
            throw new IllegalStateException("unable parse input", e);
        }

    }

    /**
     * Encrypts the token for use as the payload and adds 'flag' and 'length'
     * headers to create a Remctl packet that is written to output stream.
     * 
     * @param outputStream
     *            The stream to write the message to.
     * @param remctlToken
     *            The token to write
     */
    public void encodeMessage(OutputStream outputStream, RemctlToken remctlToken) {

        try {
            byte[] message = remctlToken.write(null);
            MessageProp prop = new MessageProp(0, true);
            byte[] encryptedToken = this.context.wrap(message, 0,
                    message.length,
                    prop);

            DataOutputStream outStream = new DataOutputStream(outputStream);
            outStream.writeByte(RemctlFlag.TOKEN_DATA.getValue()
                      ^ RemctlFlag.TOKEN_PROTOCOL.getValue());
            outStream.writeInt(encryptedToken.length);
            outStream.write(encryptedToken);
        } catch (Exception e) {
            e.printStackTrace();
            throw new RuntimeException(e);
        }

    }

    /**
     * Decode the RemctlToken from the input stream
     * 
     * @param inputStream
     *            The inputstream
     * @return The token decrypted from the payload of inputstream.
     */
    public RemctlToken decodeMessage(InputStream inputStream) {
        try {
            return getToken(new DataInputStream(inputStream),
                    this.context);
        } catch (Exception e) {
            e.printStackTrace();
            throw new RuntimeException(e);
        }

    }

}
