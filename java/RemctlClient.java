/*
   $Id$

   The java client for a "K5 sysctl" - a service for remote execution of 
   predefined commands. Access is authenticated via GSSAPI Kerberos 5, 
   authorized via aclfiles.

   Written by Anton Ushakov <antonu@stanford.edu>
   Copyright 2002 Board of Trustees, Leland Stanford Jr. University

*/

import javax.security.auth.callback.*;
import javax.security.auth.login.*;
import javax.security.auth.Subject;
import com.sun.security.auth.callback.TextCallbackHandler;
import org.ietf.jgss.*;

import java.net.*;
import java.io.*;
import java.lang.reflect.*;
import java.util.Arrays;
import java.util.ArrayList;
import java.util.Iterator;
import java.nio.ByteBuffer;
import java.security.PrivilegedExceptionAction;

public class RemctlClient {

    private ArrayList byteArgs;
    private String host;
    private String servicePrincipal;

    /** Default remctl server port to use */
    public static final int DEFAULT_PORT = 4444;

    /** Default name to use in the login configuration file */
    public static final String DEFAULT_NAME = "RemctlClient";

    /* Token types */
    private static final int TOKEN_NOOP  =            (1<<0);
    private static final int TOKEN_CONTEXT =          (1<<1);
    private static final int TOKEN_DATA =             (1<<2);
    private static final int TOKEN_MIC =              (1<<3);

    /* Token flags */
    private static final int TOKEN_CONTEXT_NEXT =     (1<<4);
    private static final int TOKEN_SEND_MIC =         (1<<5);

    private String clientIdentity;
    private  String serverIdentity;
    private int    returnCode;
    private String returnMessage;
    private GSSContext context;
    private MessageProp prop;
    private Socket socket;
    private DataInputStream inStream;
    private DataOutputStream outStream;

    /**
     * Main should be invoked as follows:
     * <p>
     * <code>java -Djava.security.auth.login.config=login.conf RemctlClient {host} {args}</code>
     *
     */

    public static void main(String[] args) {

        if (args.length < 3) {
            System.err.println("Usage: java <options> RemctlClient "
                               + " <hostName> <args>");
            System.exit(-1);
        }
        
        try {

            String host = args[0];
            String remargs[] = new String[args.length-1];
            for(int i=1; i<args.length; i++)
                remargs[i-1] = args[i];

            RemctlClient rc = RemctlClient.withLogin(remargs,
                                                     host,
                                                     0,
                                                     null,
                                                     null);
            System.out.print(rc.getReturnMessage());
            System.exit(rc.getReturnCode());
        } catch (Exception e) {
            System.err.println("Error: "+e.getMessage());
            e.printStackTrace();
            System.exit(-1);
        }

    }

    /**
     * Factory method that creates a <code>LogincContext</code>
     * and returns a RemctlClient that executed from within that
     * context.
     *
     * @param args  array of args to pass to remctl server
     * @param host  host name of remctl server. 
     * @param port  port of remctl server. If 0, uses the default
     *              port of 4444 (RemctlClient.DEFAULT_PORT).
     * @param princ Principal to use. If null, uses the default
     *              principal of <code>"host/"+host</code>.
     * @param lcName  Name passed to the <code>LoginContext</code> constructor.
     *              If null, use the default name of "RemctlClient".
     */

    public static RemctlClient withLogin(final String args[],
                                         final String host,
                                         final int port,
                                         final String princ,
                                         final String lcName)
        throws javax.security.auth.login.LoginException,
               java.security.PrivilegedActionException
    {
        final LoginContext lc = 
            new LoginContext(lcName != null ? lcName : "RemctlClient", 
                             new TextCallbackHandler());
        lc.login();
        PrivilegedExceptionAction pea = 
            new PrivilegedExceptionAction() {
                public Object run() 
                    throws GSSException,
                           IOException
                {
                    return new RemctlClient(args, 
                                            host,
                                            port,
                                            princ);
                }
            };
            
        try {
            return (RemctlClient)
                Subject.doAsPrivileged(lc.getSubject(), pea, null);
        } finally {
            lc.logout();
        }
    }

    /**
     * Factory method that creates a <code>LogincContext</code>
     * and returns a RemctlClient that executed from within that
     * context. Uses default values for port/principal/name.
     *
     * @param args  array of args to pass to remctl server
     * @param host  host name of remctl server. 
     *              If null, use the default name of "RemctlClient".
     */
    public static RemctlClient withLogin(final String args[],
                                         final String host)
        throws javax.security.auth.login.LoginException,
               java.security.PrivilegedActionException
    { 
        return withLogin(args, host, 0, null, null);
    }

    /**
     * Invoke Remctl on the specified host, with the specified args.
     * If no exceptions are thrown, you would then normally check
     * the return code with <code>getReturnCode()</code>, and the
     * returned message with <code>getReturnMessage()</code>.
     * <p>
     * Should only be called within a login context that has the
     * proper Kerberos credentials. The factory methods <code>withLogin</code>
     * can be used when a single RemctlCLient object is needed. If
     * multiple RemctlClient objects are needed, then the application
     * should itself set up the login context, invoke Subject.doAsPrivileged,
     * and then create multiple RemctlObjects from within that context.
     *
     * @param args  array of args to pass to remctl server
     * @param host  host name of remctl server. 
     * @param port  port of remctl server. If 0, uses the default
     *              port of 4444 (RemctlClient.DEFAULT_PORT).
     * @param servicePrincipal Principal to use. If null, uses the default
     *              principal of <code>"host/"+host</code>.
     */
    public RemctlClient(String args[], 
                        String host, 
                        int port, 
                        String servicePrincipal) 
        throws GSSException, IOException 
    {
        this.host = host;
        this.servicePrincipal = servicePrincipal;
        this.byteArgs = new ArrayList();

        for (int i=0; i < args.length; i++) {
            byteArgs.add(args[i].getBytes());
        }

        InetAddress hostAddress = InetAddress.getByName(host);
	String hostName = hostAddress.getCanonicalHostName().toLowerCase();
        if (this.servicePrincipal == null) 
            this.servicePrincipal = "host/"+hostName;

        /* Make the socket: */
        socket =    new Socket(hostName, port != 0 ? port : DEFAULT_PORT);
        inStream =  new DataInputStream(
                    new BufferedInputStream(socket.getInputStream(),8192));
        outStream = new DataOutputStream(
                    new BufferedOutputStream(socket.getOutputStream(),8192));
        clientEstablishContext();
        processRequest();
        processResponse();
        context.dispose();
        socket.close();
    }

    /**
     * Returns the returnCode from the remote remctl execution. For
     * example, if you invoked a Perl script that ended with a
     * an "exit 1", this method would return 1. 
     *
     * @return the return code from the remote remctl execution.
     * 
     */

    public int getReturnCode() {
        return returnCode;
    }

    /**
     * Returns the output from the remote remctl execution. For
     * example, if you invoked a Perl script whose only output
     * was "hello world\n", this method would return "hello world\n".
     *
     * @return the output from the remote remctl execution.
     * 
     */
    public String getReturnMessage() {
        return returnMessage;
    }

    /**
     * Returns the client's Kerberos principal name.
     *
     * @return Returns the client's Kerberos principal name.
     *
     */
    public String getClientIdentity() {
        return clientIdentity;
    }

    /**
     * Returns the server's Kerberos principal name.
     *
     * @return Returns the server's Kerberos principal name.
     *
     */
    public String getServerIdentity() {
        return serverIdentity;
    }

    private void clientEstablishContext() 
        throws GSSException, IOException {

        /*
         * This Oid is used to represent the Kerberos version 5 GSS-API
         * mechanism. It is defined in RFC 1964. We will use this Oid
         * whenever we need to indicate to the GSS-API that it must
         * use Kerberos for some purpose.
         */
        Oid krb5Oid = new Oid("1.2.840.113554.1.2.2");

        GSSManager manager = GSSManager.getInstance();

        /*
         * Create a GSSName out of the service name. The null
         * indicates that this application does not wish to make
         * any claims about the syntax of this name and that the
         * underlying mechanism should try to parse it as per whatever
         * default syntax it chooses.
         */
        GSSName serverName = manager.createName(servicePrincipal, null);

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
        context = manager.createContext(serverName,
                                        krb5Oid,
                                        null,
                                        GSSContext.DEFAULT_LIFETIME);

        // Set the optional features on the context.
        context.requestMutualAuth(true);  // Mutual authentication
        context.requestConf(true);  // Will use confidentiality later
        context.requestInteg(true); // Will use integrity later


        byte[] token = new byte[0];

        // Initialize the context establishment 
        outStream.writeByte(TOKEN_NOOP|TOKEN_CONTEXT_NEXT);
        outStream.writeInt(token.length);
        outStream.write(token);
        outStream.flush();

        // Do the context establishment loop
        while (!context.isEstablished()) {

            // token is ignored on the first call
            token = context.initSecContext(token, 0, token.length);

            // Send a token to the server if one was generated by
            // initSecContext
            if (token != null) {
                outStream.writeByte(TOKEN_CONTEXT);
                outStream.writeInt(token.length);
                outStream.write(token);
                outStream.flush();
            }

            // If the client is done with context establishment
            // then there will be no more tokens to read in this loop
            if (!context.isEstablished()) {
                inStream.readByte(); // flag
                token = new byte[inStream.readInt()];
                inStream.readFully(token);
            }
        }
        
        clientIdentity = context.getSrcName().toString();
        serverIdentity = context.getTargName().toString();

        /*
         * If mutual authentication did not take place, then only the
         * client was authenticated to the server. Otherwise, both
         * client and server were authenticated to each other.
         */
        if (! context.getMutualAuthState()) {
            throw new IOException("remctl: no mutal authentication");
        }

    }

    private void processRequest() throws GSSException, IOException {

        /* determine size of buffer we need */
        int messageLength = 4; /* number of args */
        for (Iterator it = byteArgs.iterator(); it.hasNext();) {
            /* add 4 for the length, then the actual length */
            byte[]  bytes = (byte[]) it.next();
            messageLength += 4+bytes.length;
        }

        /* Make the message buffer */
        ByteBuffer messageBuffer = ByteBuffer.allocate(messageLength);
        messageBuffer.putInt(byteArgs.size());
        for (Iterator it = byteArgs.iterator(); it.hasNext(); ) {
            byte[]  bytes = (byte[]) it.next();
            messageBuffer.putInt(bytes.length);
            messageBuffer.put(bytes);
        }

        /* Extract the raw bytes of the message */
        byte[] messageBytes = messageBuffer.array(); /* doesn't copy */
        
        /*
         * The first MessageProp argument is 0 to request
         * the default Quality-of-Protection.
         * The second argument is true to request
         * privacy (encryption of the message).
         */
        prop =  new MessageProp(0, true);

        /*
         * Encrypt the data and send it across. Integrity protection
         * is always applied, irrespective of confidentiality
         * (i.e., encryption).
         * You can use the same token (byte array) as that used when 
         * establishing the context.
         */

        byte[] token = context.wrap(messageBytes, 0, 
                                    messageBytes.length, prop);
        outStream.writeByte(TOKEN_DATA | TOKEN_SEND_MIC);
        outStream.writeInt(token.length);
        outStream.write(token);
        outStream.flush();

        /*
         * Now we will allow the server to decrypt the message,
         * calculate a MIC on the decrypted message and send it back
         * to us for verification. This is unnecessary, but done here
         * for illustration.
         */
        inStream.readByte(); // flag
        token = new byte[inStream.readInt()];
        inStream.readFully(token);
        context.verifyMIC(token, 0, token.length, 
                          messageBytes, 0, messageBytes.length,
                          prop);
        
    }

    private void processResponse() throws GSSException, IOException {

        byte flag = inStream.readByte();

        if ((flag & TOKEN_DATA) == 0) {
            throw new IOException("remctl: Wrong token type received, " +
                                  "expected TOKEN_DATA");
        }

        byte[] token = new byte[inStream.readInt()];
        inStream.readFully(token);

        byte[] bytes = context.unwrap(token, 0, token.length, prop);
        ByteBuffer messageBuffer = ByteBuffer.allocate(bytes.length);
        messageBuffer.put(bytes);
        messageBuffer.rewind();

        returnCode = messageBuffer.getInt();

        byte[] responsebytes = new byte[messageBuffer.getInt()];
        messageBuffer.get(responsebytes);

        returnMessage = new String(responsebytes);

        /*
         * First reset the QOP of the MessageProp to 0
         * to ensure the default Quality-of-Protection
         * is applied.
         */
        prop.setQOP(0);
    
        token = context.getMIC(bytes, 0, bytes.length, prop);
    
        outStream.writeByte(TOKEN_MIC);
        outStream.writeInt(token.length);
        outStream.write(token);
        outStream.flush();
    }
}

/*
**  Local variables:
**  java-basic-offset: 4
**  indent-tabs-mode: nil
**  end:
*/
