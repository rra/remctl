package org.eyrie.remctl;

import java.io.DataInputStream;
import java.io.DataOutputStream;
import java.io.IOException;
import java.net.Socket;
import java.net.UnknownHostException;
import java.security.PrivilegedExceptionAction;

import javax.security.auth.Subject;
import javax.security.auth.login.LoginContext;

import org.eyrie.remctl.RemctlTests.RemctlFlag;
import org.ietf.jgss.GSSContext;
import org.ietf.jgss.GSSException;
import org.ietf.jgss.GSSManager;
import org.ietf.jgss.GSSName;
import org.ietf.jgss.Oid;

/**
 * A simplified interface for a remctl client.
 * 
 * @author pradtke
 * 
 */
public class SimpleRemctlClient {

    StringBuilder stdOutResponse = new StringBuilder();
    StringBuilder stdErrResponse = new StringBuilder();

    GSSContext context;

    Integer returnStatus;

    //    String hostname;
    //    String serverPrincipal;
    //    int port;

    String serverPrincipal;
    String hostname;
    int port = 4373;

    DataInputStream inStream;
    DataOutputStream outStream;

    RemctlErrorToken errorToken;

    public SimpleRemctlClient(String hostname) {
        this(hostname, 0, null);
    }

    /**
     * Create a simple Remctl client
     * 
     * @param hostname
     *            The host to connect to
     * @param port
     *            The port to connect on
     * @param serverPrincipal
     *            The server principal. If null, defaults to 'host/hostname'
     * @param clientPrincipal
     *            The client principal
     */
    public SimpleRemctlClient(String hostname, int port,
            String serverPrincipal) {
        this.hostname = hostname;
        this.port = port == 0 ? 4373 : port;
        this.serverPrincipal = serverPrincipal;
    }

    public void execute(String... arguments) {
        try {
            System.setProperty("java.security.auth.login.config",
                    "/Users/pradtke/Desktop/kerberos/remctl/java/gss_jaas.conf");
            LoginContext context = new LoginContext("RemctlClient");
            context.login();
            Subject subject = context.getSubject();
            PrivilegedExceptionAction pea =
                    new PrivilegedExceptionAction() {
                        @Override
                        public Object run()
                                throws Exception, IOException {
                            System.out.println("in action");
                            SimpleRemctlClient.this.establishContext();
                            return null;
                        }
                    };

            Subject.doAs(subject, pea);

            RemctlCommandToken command = new RemctlCommandToken(this.context,
                                false,
                                arguments);

            command.write(this.outStream);

            while (true) {
                RemctlMessageToken outputToken = (RemctlMessageToken) RemctlToken
                                    .getToken(this.inStream, this.context);

                System.out.println("token type " + outputToken.getType());
                System.out.println("token  " + outputToken);
                if (outputToken instanceof RemctlErrorToken) {
                    this.errorToken = (RemctlErrorToken) outputToken;
                    break;
                } else if (outputToken instanceof RemctlOutputToken) {
                    RemctlOutputToken remctlOutputToken = (RemctlOutputToken) outputToken;
                    if (remctlOutputToken.getStream() == 1) {
                        this.stdOutResponse.append(remctlOutputToken
                                            .getOutputAsString());
                    } else {
                        this.stdErrResponse.append(remctlOutputToken
                                            .getOutputAsString());
                    }
                } else if (outputToken instanceof RemctlStatusToken) {
                    System.out.println("Status done");
                    this.returnStatus = ((RemctlStatusToken) outputToken)
                                        .getStatus();
                    break;
                }
            }
        } catch (Exception e) {
            throw new RuntimeException(e);
        }
    }

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
        byte flag = (byte) (RemctlFlag.TOKEN_NOOP.value
                ^ RemctlFlag.TOKEN_CONTEXT_NEXT.value
                ^ RemctlFlag.TOKEN_PROTOCOL.value);
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
                flag = (byte) (RemctlFlag.TOKEN_CONTEXT.value
                        ^ RemctlFlag.TOKEN_PROTOCOL.value);
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
                if ((flag ^ RemctlFlag.TOKEN_PROTOCOL.value ^ RemctlFlag.TOKEN_CONTEXT.value) != 0) {
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
     * @return the stdOutResponse
     */
    String getStdOutResponse() {
        return this.stdOutResponse.toString();
    }

    /**
     * @return the stdErrResponse
     */
    String getStdErrResponse() {
        return this.stdErrResponse.toString();
    }

    /**
     * @return the returnStatus
     */
    Integer getReturnStatus() {
        return this.returnStatus;
    }

}
