package org.eyrie.remctl;

import java.io.DataInputStream;
import java.io.DataOutputStream;
import java.io.InputStream;
import java.io.OutputStream;

import org.eyrie.remctl.core.RemctlFlag;
import org.eyrie.remctl.core.RemctlToken;
import org.ietf.jgss.GSSContext;
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

    private final GSSContext context;

    public RemctlMessageConverter(GSSContext context) {
        this.context = context;
    }

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

    public RemctlToken decodeMessage(InputStream inputStream) {
        try {
            return RemctlToken.getToken(new DataInputStream(inputStream),
                    this.context);
        } catch (Exception e) {
            e.printStackTrace();
            throw new RuntimeException(e);
        }

    }

}
