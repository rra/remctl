/*
 * Written by Russ Allbery <rra@stanford.edu>
 * Copyright 2010 Board of Trustees, Leland Stanford Jr. University
 * 
 * See LICENSE for licensing terms.
 */
package org.eyrie.remctl;

import static org.junit.Assert.assertEquals;
import static org.junit.Assert.assertNotNull;
import static org.junit.Assert.assertNull;

import java.io.BufferedInputStream;
import java.io.BufferedOutputStream;
import java.io.DataInputStream;
import java.io.DataOutputStream;
import java.io.IOException;
import java.net.InetAddress;
import java.net.Socket;
import java.security.PrivilegedActionException;
import java.security.PrivilegedExceptionAction;

import javax.security.auth.Subject;
import javax.security.auth.login.LoginContext;
import javax.security.auth.login.LoginException;

import org.eyrie.remctl.core.RemctlFlag;
import org.eyrie.remctl.core.RemctlMessageToken;
import org.eyrie.remctl.core.RemctlToken;
import org.ietf.jgss.GSSContext;
import org.ietf.jgss.GSSManager;
import org.ietf.jgss.GSSName;
import org.ietf.jgss.Oid;
import org.junit.Test;

import edu.stanford.infra.remctl.RemctlClient;

/**
 * JUnit test suite for the org.eyrie.remctl package.
 * 
 * @author Russ Allbery &lt;rra@stanford.edu&gt;
 */
public class RemctlTests {
    /**
     * Test method for {@link RemctlMessageCode#getCode(int)}.
     */
    @Test
    public void testGetCode() {
        assertEquals("MESSAGE_COMMAND", RemctlMessageCode.MESSAGE_COMMAND,
                RemctlMessageCode.getCode(1));
        assertNull("unknown message", RemctlMessageCode.getCode(42));
    }

    /**
     * Test method for {@link RemctlErrorException#getMessage()}.
     */
    @Test
    public void testGetMessage() {
        RemctlErrorException e = new RemctlErrorException(
                RemctlErrorCode.ERROR_INTERNAL);
        assertEquals("ERROR_INTERNAL", "Internal server failure (error 1)",
                e.getMessage());
        e = new RemctlErrorException(RemctlErrorCode.ERROR_TOOMUCH_DATA);
        assertEquals("ERROR_TOOMUCH_DATA",
                "Argument size exceeds server limit (error 8)",
                e.getMessage());
    }

    @Test
    public void testLogin() throws LoginException, PrivilegedActionException {
        System.setProperty("java.security.auth.login.config",
                "/Users/pradtke/Desktop/kerberos/remctl/java/gss_jaas.conf");
        LoginContext context = new LoginContext("RemctlClient");
        context.login();
        Subject subject = context.getSubject();
        assertNotNull(subject);

        System.out.println(subject.getPrincipals());

        PrivilegedExceptionAction pea =
                new PrivilegedExceptionAction() {
                    @Override
                    public Object run()
                            throws Exception, IOException {
                        System.out.println("in action");
                        RemctlTests.this.clientEstablishContext();
                        return null;
                    }
                };

        Subject.doAs(subject, pea);

    }

    String serverPrincipal = "host/acct-scripts-dev.stanford.edu";
    String hostName = "acct-scripts-dev.stanford.edu";
    //        int port = 4373;
    int port = 3456;

    public void clientEstablishContext() throws Exception, IOException {

        /**
         * Copied from
         * http://download.oracle.com/javase/1.5.0/docs/guide/security
         * /jgss/tutorials/SampleClient.java
         */
        Socket socket = new Socket(this.hostName, this.port);
        DataInputStream inStream =
                new DataInputStream(socket.getInputStream());
        DataOutputStream outStream =
                new DataOutputStream(socket.getOutputStream());

        System.out.println("Connected to server "
                   + socket.getInetAddress());

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
        GSSContext context = manager.createContext(serverName,
                        krb5Oid,
                        null,
                        GSSContext.DEFAULT_LIFETIME);

        // Set the desired optional features on the context. The client
        // chooses these options.

        context.requestMutualAuth(true); // Mutual authentication
        context.requestConf(true); // Will use confidentiality later
        context.requestInteg(true); // Will use integrity later

        //Establish as protocol 2
        byte flag = (byte) (RemctlFlag.TOKEN_NOOP.value
                ^ RemctlFlag.TOKEN_CONTEXT_NEXT.value
                ^ RemctlFlag.TOKEN_PROTOCOL.value);
        System.out.println("flags " + flag);
        outStream.writeByte(flag);
        outStream.writeInt(0);
        outStream.flush();

        // Do the context eastablishment loop

        byte[] token = new byte[0];

        while (!context.isEstablished()) {

            // token is ignored on the first call
            token = context.initSecContext(token, 0, token.length);

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

                outStream.writeByte(flag);
                outStream.writeInt(token.length);
                outStream.write(token);
                outStream.flush();
            }

            // If the client is done with context establishment
            // then there will be no more tokens to read in this loop
            if (!context.isEstablished()) {
                flag = inStream.readByte();
                if ((flag ^ RemctlFlag.TOKEN_PROTOCOL.value ^ RemctlFlag.TOKEN_CONTEXT.value) != 0) {
                    System.out.println("Unexpected token: " + flag);
                }
                token = new byte[inStream.readInt()];
                inStream.readFully(token);
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
        System.out.println("Client is " + context.getSrcName());
        System.out.println("Server is " + context.getTargName());

        /*
         * If mutual authentication did not take place, then only the
         * client was authenticated to the server. Otherwise, both
         * client and server were authenticated to each other.
         */
        if (context.getMutualAuthState())
            System.out.println("Mutual authentication took place!");

        //send a command

        RemctlCommandToken command = new RemctlCommandToken(context, false,
                "account-show", "show", "pradtke");

        command.write(outStream);

        while (true) {
            RemctlMessageToken outputToken = (RemctlMessageToken) RemctlToken
                    .getToken(inStream, context);

            System.out.println("token type " + outputToken.getType());
            System.out.println("token  " + outputToken);
            if (outputToken instanceof RemctlErrorToken) {
                RemctlErrorCode returnedCode = ((RemctlErrorToken) outputToken)
                        .getCode();
                assertEquals(RemctlErrorCode.ERROR_ACCESS, returnedCode);
                assertEquals("Access denied",
                        ((RemctlErrorToken) outputToken).getMessage());
                break;
            } else if (outputToken instanceof RemctlOutputToken) {
                RemctlOutputToken remctlOutputToken = (RemctlOutputToken) outputToken;
                System.out.print(remctlOutputToken.getOutputAsString());
            } else if (outputToken instanceof RemctlStatusToken) {
                System.out.println("Status done");
                break;
            }
        }

    }

    @Test
    public void testRemctlOld() throws Exception {
        System.setProperty("java.security.auth.login.config",
                "/Users/pradtke/Desktop/kerberos/remctl/java/gss_jaas.conf");
        LoginContext context = new LoginContext("RemctlClient");
        context.login();
        Subject subject = context.getSubject();
        assertNotNull(subject);

        System.out.println(subject.getPrincipals());

        RemctlClient remctlClient = new RemctlClient(new String[] { "puppet",
                "status" }, this.hostName, this.port, this.serverPrincipal,
                subject);

    }

    @Test
    public void testAccessDenied() throws IOException {
        String host = "venture.stanford.edu";
        InetAddress hostAddress = InetAddress.getByName(host);
        String hostName = hostAddress.getCanonicalHostName().toLowerCase();
        final Socket socket = new Socket(hostName, 4373);
        final DataInputStream inStream = new DataInputStream(
                    new BufferedInputStream(socket.getInputStream(), 8192));
        final DataOutputStream outStream = new DataOutputStream(
                    new BufferedOutputStream(socket.getOutputStream(), 8192));
    }

}
