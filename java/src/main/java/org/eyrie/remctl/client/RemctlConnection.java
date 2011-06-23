package org.eyrie.remctl.client;

import java.io.DataInputStream;
import java.io.DataOutputStream;
import java.io.IOException;
import java.net.Socket;
import java.net.UnknownHostException;
import java.security.PrivilegedExceptionAction;
import java.util.ArrayList;
import java.util.Date;
import java.util.List;

import javax.security.auth.Subject;
import javax.security.auth.login.LoginContext;

import org.eyrie.remctl.RemctlException;
import org.eyrie.remctl.core.RemctlErrorToken;
import org.eyrie.remctl.core.RemctlFlag;
import org.eyrie.remctl.core.RemctlMessageConverter;
import org.eyrie.remctl.core.RemctlQuitToken;
import org.eyrie.remctl.core.RemctlStatusToken;
import org.eyrie.remctl.core.RemctlToken;
import org.eyrie.remctl.core.RemctlVersionToken;
import org.ietf.jgss.GSSContext;
import org.ietf.jgss.GSSException;
import org.ietf.jgss.GSSManager;
import org.ietf.jgss.GSSName;
import org.ietf.jgss.Oid;

/**
 * A connection to a remctl control server that allows you to send and recieve
 * remctl tokens.
 * 
 * It is not thread safe.
 * 
 * @author pradtke
 * 
 */
public class RemctlConnection {

    /**
     * Our GSS context
     */
    private GSSContext context;

    /**
     * The server principal
     */
    private String serverPrincipal;

    /**
     * Converts RemctlTokens to/from their wire representations
     */
    RemctlMessageConverter messageConverter;

    /**
     * The hostname to connect to.
     */
    String hostname;

    /**
     * the port to connect to.
     */
    int port = 4373;

    /**
     * Data stream sent from the server
     */
    DataInputStream inStream;

    /**
     * Data stream sent to the server
     */
    DataOutputStream outStream;

    /**
     * Time connection was established
     */
    private Date connectionEstablishedTime;

    /**
     * RemctlClient that will connect to the provide host, on the default port
     * (4373) using the default principal name 'host/canonical_servername'.
     * 
     * @param hostname
     *            the FQDN to connect to.
     */
    public RemctlConnection(String hostname) {
        this(hostname, 0, null);
    }

    /**
     * Create a simple Remctl client
     * 
     * @param hostname
     *            The host to connect to
     * @param port
     *            The port to connect on. If 0, defaults to 4373
     * @param serverPrincipal
     *            The server principal. If null, defaults to
     *            'host/canonical_servername'
     */
    public RemctlConnection(String hostname, int port,
            String serverPrincipal) {
        this.hostname = hostname;
        this.port = port == 0 ? 4373 : port;
        this.serverPrincipal = serverPrincipal;
    }

    /**
     * Send the token to the server
     * 
     * <p>
     * The token will be encrypted and sent.
     * 
     * @param token
     *            The token to send
     */
    public void writeToken(RemctlToken token) {
        this.messageConverter.encodeMessage(this.outStream, token);

    }

    /**
     * Read a token from the server.
     * 
     * @return The next token read from the server
     */
    public RemctlToken readToken() {
        return this.messageConverter
                .decodeMessage(this.inStream);
    }

    /**
     * Read tokens from the server until a Status or Error Token is reached.
     * 
     * @return A list of all tokens (including the ending Status or Error Token)
     *         read from the server.
     */
    public List<RemctlToken> readAllTokens() {
        List<RemctlToken> tokenList = new ArrayList<RemctlToken>();

        while (true) {
            RemctlToken outputToken = this.readToken();
            tokenList.add(outputToken);
            System.out.println("token  " + outputToken);
            if (outputToken instanceof RemctlErrorToken) {
                break;
            } else if (outputToken instanceof RemctlStatusToken) {
                System.out.println("Status done");
                break;
            } else if (outputToken instanceof RemctlVersionToken) {
                //version token is the end of the tokens
                break;
            }
        }

        return tokenList;
    }

    /**
     * Close this connection if open.
     */
    public void close() {
        if (this.isConnected) {
            this.writeToken(new RemctlQuitToken());
            this.isConnected = false;
        }

    }

    /**
     * Indicates if we are already connected.
     */
    private boolean isConnected = false;

    /**
     * Connect to the remctl server and establish the GSS context.
     * 
     * @return true if the client created a new connection, or false if it was
     *         already connected
     */
    public boolean connect() {

        if (this.isConnected)
            return false;

        try {
            this.connectionEstablishedTime = new Date();
            /** Login so can access kerb ticket **/
            LoginContext context = new LoginContext("RemctlClient");
            context.login();
            Subject subject = context.getSubject();
            PrivilegedExceptionAction<Void> pea =
                    new PrivilegedExceptionAction<Void>() {
                        @Override
                        public Void run()
                                throws Exception, IOException {
                            System.out.println("in action");
                            RemctlConnection.this.establishContext();
                            return null;
                        }
                    };

            Subject.doAs(subject, pea);

            this.messageConverter = new RemctlMessageConverter(
                    this.context);

            this.isConnected = true;
            return this.isConnected;
        } catch (Exception e) {
            throw new RuntimeException(e);
        }

    }

    /**
     * Connect and establish the context
     * 
     * @throws UnknownHostException
     *             thrown if host doesn't exist
     * @throws IOException
     *             thrown on IO issues
     * @throws GSSException
     *             thrown on GSS issues
     */
    private void establishContext() throws UnknownHostException, IOException,
            GSSException {
        /**
         * Copied from
         * http://download.oracle.com/javase/1.5.0/docs/guide/security
         * /jgss/tutorials/SampleClient.java
         */
        Socket socket = new Socket(this.hostname, this.port);
        this.inStream =
                new DataInputStream(socket.getInputStream());
        this.outStream =
                new DataOutputStream(socket.getOutputStream());

        System.out.println("Connected to server "
                   + socket.getInetAddress());

        if (this.serverPrincipal == null) {
            String cannonicalName = socket.getInetAddress()
                    .getCanonicalHostName().toLowerCase();
            if (!cannonicalName.equalsIgnoreCase(this.hostname)) {
                System.out.println("Using Canonical server name in principal ("
                        + cannonicalName + ")");
            }
            this.serverPrincipal = "host/" + cannonicalName;
        }

        /*
         * This Oid is used to represent the Kerberos version 5 GSS-API
         * mechanism. It is defined in RFC 1964. We will use this Oid
         * whenever we need to indicate to the GSS-API that it must
         * use Kerberos for some purpose.
         */
        Oid krb5Oid = new Oid("1.2.840.113554.1.2.2");

        GSSManager manager = GSSManager.getInstance();

        /*
         * Create a GSSName out of the server's name. The null
         * indicates that this application does not wish to make
         * any claims about the syntax of this name and that the
         * underlying mechanism should try to parse it as per whatever
         * default syntax it chooses.
         */
        GSSName serverName = manager.createName(this.serverPrincipal, null);

        /*
         * Create a GSSContext for mutual authentication with the
         * server.
         *    - serverName is the GSSName that represents the server.
         *    - krb5Oid is the Oid that represents the mechanism to
         *      use. The client chooses the mechanism to use.
         *    - null is passed in for client credentials
         *    - DEFAULT_LIFETIME lets the mechanism decide how long the
         *      context can remain valid.
         * Note: Passing in null for the credentials asks GSS-API to
         * use the default credentials. This means that the mechanism
         * will look among the credentials stored in the current Subject
         * to find the right kind of credentials that it needs.
         */
        this.context = manager.createContext(serverName,
                        krb5Oid,
                        null,
                        GSSContext.DEFAULT_LIFETIME);

        // Set the desired optional features on the context. The client
        // chooses these options.

        this.context.requestMutualAuth(true); // Mutual authentication
        this.context.requestConf(true); // Will use confidentiality later
        this.context.requestInteg(true); // Will use integrity later

        //Establish as protocol 2
        byte flag = (byte) (RemctlFlag.TOKEN_NOOP.getValue()
                ^ RemctlFlag.TOKEN_CONTEXT_NEXT.getValue()
                ^ RemctlFlag.TOKEN_PROTOCOL.getValue());
        System.out.println("flags " + flag);
        this.outStream.writeByte(flag);
        this.outStream.writeInt(0);
        this.outStream.flush();

        // Do the context eastablishment loop

        byte[] token = new byte[0];

        while (!this.context.isEstablished()) {

            // token is ignored on the first call
            token = this.context.initSecContext(token, 0, token.length);

            // Send a token to the server if one was generated by
            // initSecContext
            if (token != null) {
                System.out.println("Will send token of size "
                        + token.length
                        + " from initSecContext.");
                //outStream.writeByte(RemctlToken.supportedVersion);
                //                RemctlVersionToken versionToken = new RemctlVersionToken(
                //                        context, 2);
                // versionToken.writeData(outStream);
                flag = (byte) (RemctlFlag.TOKEN_CONTEXT.getValue()
                        ^ RemctlFlag.TOKEN_PROTOCOL.getValue());
                System.out.println("flags " + flag);

                this.outStream.writeByte(flag);
                this.outStream.writeInt(token.length);
                this.outStream.write(token);
                this.outStream.flush();
            }

            // If the client is done with context establishment
            // then there will be no more tokens to read in this loop
            if (!this.context.isEstablished()) {
                flag = this.inStream.readByte();
                if ((flag ^ RemctlFlag.TOKEN_PROTOCOL.getValue() ^ RemctlFlag.TOKEN_CONTEXT
                        .getValue()) != 0) {
                    System.out.println("Unexpected token: " + flag);
                }
                token = new byte[this.inStream.readInt()];
                this.inStream.readFully(token);
                //                RemctlToken inputToken = RemctlToken
                //                        .getToken(inStream, context);
                //                token = new byte[inStream.readInt()];
                //                System.out.println("Will read input token of size "
                //                        + token.length
                //                        + " for processing by initSecContext");
                //                inStream.readFully(token);
            }
        }

        System.out.println("Context Established! ");
        System.out.println("Client is " + this.context.getSrcName());
        System.out.println("Server is " + this.context.getTargName());

        /*
         * If mutual authentication did not take place, then only the
         * client was authenticated to the server. Otherwise, both
         * client and server were authenticated to each other.
         */
        if (this.context.getMutualAuthState())
            System.out.println("Mutual authentication took place!");
    }

    /**
     * Return the time the connection was established.
     * 
     * @return the a copy connectionEstablishedTime
     */
    public Date getConnectionEstablishedTime() {
        return new Date(this.connectionEstablishedTime.getTime());
    }

    /**
     * Checks the input stream for incoming data.
     * 
     * <p>
     * Useful for determining if there is unread data buffered on the input
     * stream, prior to sending another command
     * </p>
     * 
     * @return true if there is data that can be read.
     */
    public boolean hasPendingData() {
        try {
            return this.inStream.available() > 0;
        } catch (IOException e) {
            throw new RemctlException("Unable to check for pending data", e);
        }
    }

}
