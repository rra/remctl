/*
 * @(#)SampleClient.java
 *
 * Copyright 2001-2002 Sun Microsystems, Inc. All Rights Reserved.
 *
 * Redistribution and use in source and binary forms, with or 
 * without modification, are permitted provided that the following 
 * conditions are met:
 * 
 * -Redistributions of source code must retain the above copyright  
 * notice, this  list of conditions and the following disclaimer.
 * 
 * -Redistribution in binary form must reproduct the above copyright 
 * notice, this list of conditions and the following disclaimer in 
 * the documentation and/or other materials provided with the 
 * distribution.
 * 
 * Neither the name of Sun Microsystems, Inc. or the names of 
 * contributors may be used to endorse or promote products derived 
 * from this software without specific prior written permission.
 * 
 * This software is provided "AS IS," without a warranty of any 
 * kind. ALL EXPRESS OR IMPLIED CONDITIONS, REPRESENTATIONS AND 
 * WARRANTIES, INCLUDING ANY IMPLIED WARRANTY OF MERCHANTABILITY, 
 * FITNESS FOR A PARTICULAR PURPOSE OR NON-INFRINGEMENT, ARE HEREBY 
 * EXCLUDED. SUN AND ITS LICENSORS SHALL NOT BE LIABLE FOR ANY 
 * DAMAGES OR LIABILITIES  SUFFERED BY LICENSEE AS A RESULT OF  OR 
 * RELATING TO USE, MODIFICATION OR DISTRIBUTION OF THE SOFTWARE OR 
 * ITS DERIVATIVES. IN NO EVENT WILL SUN OR ITS LICENSORS BE LIABLE 
 * FOR ANY LOST REVENUE, PROFIT OR DATA, OR FOR DIRECT, INDIRECT, 
 * SPECIAL, CONSEQUENTIAL, INCIDENTAL OR PUNITIVE DAMAGES, HOWEVER 
 * CAUSED AND REGARDLESS OF THE THEORY OF LIABILITY, ARISING OUT OF 
 * THE USE OF OR INABILITY TO USE SOFTWARE, EVEN IF SUN HAS BEEN 
 * ADVISED OF THE POSSIBILITY OF SUCH DAMAGES.
 * 
 * You acknowledge that Software is not designed, licensed or 
 * intended for use in the design, construction, operation or 
 * maintenance of any nuclear facility. 
 */

import org.ietf.jgss.*;
import java.net.Socket;
import java.io.IOException;
import java.io.DataInputStream;
import java.io.DataOutputStream;
import java.nio.ByteBuffer;


public class RemctlClient {

/* Token types */
static int TOKEN_NOOP  =            (1<<0);
static int TOKEN_CONTEXT =          (1<<1);
static int TOKEN_DATA =             (1<<2);
static int TOKEN_ERROR =            (1<<3);
static int TOKEN_MIC =              (1<<4);

/* Token flags */
static int TOKEN_CONTEXT_NEXT =     (1<<5);
static int TOKEN_SEND_MIC =         (1<<6);
 

    public static void main(String[] args) 
       throws IOException, GSSException, Exception  {

	// Obtain the command-line arguments and parse the port number
	
	if (args.length < 3) {
	    System.err.println("Usage: java <options> Login SampleClient "
			       + " <remctl_service_name> <hostName> <type> <service> <args>");
	    System.exit(-1);
	}

	String server = args[0];
	String hostName = args[1];
	int port = 4444;

	Socket socket = new Socket(hostName, port);
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
	GSSName serverName = manager.createName(server, null);

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

	context.requestMutualAuth(true);  // Mutual authentication
	context.requestConf(true);  // Will use confidentiality later
	context.requestInteg(true); // Will use integrity later

	// Do the context eastablishment loop

	byte[] token = new byte[0];
	byte flag;
	int errorcode = 0;

	outStream.writeByte(TOKEN_NOOP|TOKEN_CONTEXT_NEXT);
	outStream.writeInt(token.length);
	outStream.write(token);
	outStream.flush();


	while (!context.isEstablished()) {

	    // token is ignored on the first call
	    token = context.initSecContext(token, 0, token.length);

	    // Send a token to the server if one was generated by
	    // initSecContext
	    if (token != null) {
		System.out.println("Will send token of size "
				   + token.length
				   + " from initSecContext.");
                outStream.writeByte(TOKEN_CONTEXT);
		outStream.writeInt(token.length);
		outStream.write(token);
		outStream.flush();
	    }

	    // If the client is done with context establishment
	    // then there will be no more tokens to read in this loop
	    if (!context.isEstablished()) {
		flag = inStream.readByte();
		token = new byte[inStream.readInt()];
		System.out.println("Will read input token of size "
				   + token.length
				   + " for processing by initSecContext");
		inStream.readFully(token);
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

/*
	if (true) {
        context.dispose();
        socket.close();
	System.exit(0);
	}
*/

	ByteBuffer messageBuffer = ByteBuffer.allocate(64000);

	for(int i=2;i<args.length;i++) {
	messageBuffer.putInt(args[i].length());
	messageBuffer.put(args[i].getBytes());
	}

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
	MessageProp prop =  new MessageProp(0, true);

	/*
	 * Encrypt the data and send it across. Integrity protection
	 * is always applied, irrespective of confidentiality
	 * (i.e., encryption).
	 * You can use the same token (byte array) as that used when 
	 * establishing the context.
	 */

	token = context.wrap(messageBytes, 0, messageBytes.length, prop);
	System.out.println("Will send wrap token of size " + token.length);
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
	flag = inStream.readByte(); //TOKEN_MIC
	token = new byte[inStream.readInt()];
	System.out.println("Will read token of size " + token.length);
	inStream.readFully(token);
	context.verifyMIC(token, 0, token.length, 
			  messageBytes, 0, messageBytes.length,
			  prop);
	
	System.out.println("Verified received MIC for message.");




	/* R E S P O N S E */




	    /* 
	     * Read the token. This uses the same token byte array 
	     * as that used during context establishment.
	     */
  	flag = inStream.readByte();

	if ((flag & TOKEN_DATA) != 0 || (flag & TOKEN_ERROR) != 0) {
	    token = new byte[inStream.readInt()];
	    System.out.println("Will read the return token of size " + token.length);
	    inStream.readFully(token);
        }

	    System.out.println("Will unwrap the token\n");
	    byte[] bytes = context.unwrap(token, 0, token.length, prop);

	    messageBuffer = ByteBuffer.allocate(bytes.length);
	    messageBuffer.put(bytes);
	    messageBuffer.rewind();

	    if ((flag & TOKEN_ERROR) != 0) {
		errorcode = messageBuffer.getInt();
		System.out.println("Got error code\n");
	    }

	    byte[] responsebytes = new byte[messageBuffer.getInt()];
	    System.out.println("Will read the return message of size " + responsebytes.length);
	    messageBuffer.get(responsebytes);

	    String msg = new String(responsebytes);
	    System.out.println("Got response message: \n"+ msg);

        /*
	 * Now generate a MIC and send it to the client. This is
	 * just for illustration purposes. The integrity of the
	 * incoming wrapped message is guaranteed irrespective of
	 * the confidentiality (encryption) that was used.
	 */
    
         /*
	  * First reset the QOP of the MessageProp to 0
	  * to ensure the default Quality-of-Protection
	  * is applied.
	  */

    prop.setQOP(0);
    
    token = context.getMIC(bytes, 0, bytes.length, prop);
    
    System.out.println("Will send MIC token of size " 
		       + token.length);
    outStream.writeByte(TOKEN_MIC);
    outStream.writeInt(token.length);
    outStream.write(token);
    outStream.flush();
    
    
    if (errorcode != 0) {
	System.out.println("Error code: "+errorcode);
    }

    System.out.println("Output message: \n"+msg);

    System.out.println("Exiting...");
    
    context.dispose();
	socket.close();
    }
}
