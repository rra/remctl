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
import java.nio.ByteBuffer;


public class RemctlClient {

    public static void main(String[] args) throws Exception  {

        if (args.length < 3) {
            System.err.println("Usage: java <options> RemctlClient "
                               + " <hostName> <type> <service> <args>");
            System.exit(-1);
        }
        
        // Initialize the LoginContext object
        LoginContext lc = null;
        try {
            lc = new LoginContext("RemctlClient", new TextCallbackHandler());
        } catch (LoginException le) {
            System.err.println("Cannot create LoginContext. "
                               + le.getMessage());
            System.exit(-1);
        } catch (SecurityException se) {
            System.err .println("Cannot create LoginContext. "
                                + se.getMessage());
            System.exit(-1);
        }
        

        // Read the keytab, or the credentials cache
        try {
            lc.login();
        } catch (AccountExpiredException aee) {
            System.err.println("Your account has expired.  " +
                               "Please notify your administrator.");
            System.exit(-1);
            
        } catch (CredentialExpiredException cee) {

            System.err.println("Your credentials have expired.");
            System.exit(-1);

        } catch (FailedLoginException fle) {

            System.err.println("Authentication Failed");
            System.exit(-1);

        } catch (Exception e) {

            System.err.println("Unexpected Exception - unable to continue");
            e.printStackTrace();
            System.exit(-1);
        }


        // Run the RemctlAction with the k5 ticket
        try {
            Subject.doAsPrivileged(lc.getSubject(),
                                   new  RemctlAction(args),
                                   null);
        } catch (java.security.PrivilegedActionException pae) {
            pae.printStackTrace();
            System.exit(-1);
        }

        System.exit(0);
    }
}

class RemctlAction implements java.security.PrivilegedExceptionAction {

    String[] origArgs;

    static int MAXBUFFER = 64000;
    /* Token types */
    static int TOKEN_NOOP  =            (1<<0);
    static int TOKEN_CONTEXT =          (1<<1);
    static int TOKEN_DATA =             (1<<2);
    static int TOKEN_MIC =              (1<<3);

    /* Token flags */
    static int TOKEN_CONTEXT_NEXT =     (1<<4);
    static int TOKEN_SEND_MIC =         (1<<5);

    public String clientIdentity;
    public String serverIdentity;
    public int    returnCode;
    public String returnMessage;

    GSSContext context;
    MessageProp prop;
    Socket socket;
    DataInputStream inStream;
    DataOutputStream outStream;
    ByteBuffer messageBuffer;
    String hostName;
    String servicePrincipal;
    int port;


    public RemctlAction(String[] origArgs) {
        returnCode = 0;
        this.origArgs = (String[])origArgs.clone();
    }


    public Object run() throws Exception {

        InetAddress hostAddress = InetAddress.getByName(origArgs[0]);
	hostName = hostAddress.getHostName().toLowerCase();
        servicePrincipal = "host/"+hostName;
        port = 4444;

        messageBuffer = ByteBuffer.allocate(MAXBUFFER);

        String remargs[] = new String[origArgs.length-1];
        for(int i=1; i<origArgs.length; i++)
            remargs[i-1] = origArgs[i];

        int returnCode = process(remargs);

        if (returnCode != 0)
            System.out.println("Error code: "+returnCode);

        System.out.println(returnMessage);
        // successful completion
        return null;

    }

    private void clientEstablishContext() 
        throws GSSException, IOException, RemctlException {

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
            throw new RemctlException("Mutual authentication did not take "+
                                      "place!");
        }

    }


    public int process(String args[])  
        throws GSSException, IOException, RemctlException {

        /* Make the socket: */
        socket =    new Socket(hostName, port);
        inStream =  new DataInputStream(socket.getInputStream());
        outStream = new DataOutputStream(socket.getOutputStream());

        clientEstablishContext();
        processRequest(args);
        int returnCode = processResponse();

        context.dispose();
        socket.close();
        return returnCode;
    }


    private void processRequest(String args[])  
        throws GSSException, IOException, RemctlException {

        /* Make the message buffer */
        messageBuffer.putInt(args.length);
        for(int i=0;i<args.length;i++) {
            messageBuffer.putInt(args[i].length());
            messageBuffer.put(args[i].getBytes());
        }

        /* Extract the raw bytes of the message */
        int length = messageBuffer.position();
        messageBuffer.rewind();
        byte[] messageBytes = new byte[length];
        messageBuffer.get(messageBytes);
        
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

        byte[] token = context.wrap(messageBytes, 0, messageBytes.length, 
                                    prop);
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


    private int processResponse() 
        throws GSSException, IOException, RemctlException {

        byte flag = inStream.readByte();

        if ((flag & TOKEN_DATA) == 0) {
            throw new RemctlException("Wrong token type received, expected " +
                                      "TOKEN_DATA");
        }

        byte[] token = new byte[inStream.readInt()];
        inStream.readFully(token);

        byte[] bytes = context.unwrap(token, 0, token.length, prop);
        messageBuffer = ByteBuffer.allocate(bytes.length);
        messageBuffer.put(bytes);
        messageBuffer.rewind();

        int returnCode = messageBuffer.getInt();

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

        return returnCode;
    
    }
}

/*
**  Local variables:
**  java-basic-offset: 4
**  indent-tabs-mode: nil
**  end:
*/
