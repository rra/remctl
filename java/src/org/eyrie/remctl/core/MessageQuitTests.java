package org.eyrie.remctl.core;

import static org.junit.Assert.assertNotNull;

import org.eyrie.remctl.RemctlException;
import org.junit.Test;

/**
 * Test conversion to/from streams and back to RemctlMessageQuit tokens
 * 
 * @author pradtke
 * 
 */
public class MessageQuitTests {

    /**
     * Test that token can be created from bytes. Token has no message body so
     * byte array is empty
     * 
     * @throws RemctlException
     */
    @Test
    public void testFromBytes() throws RemctlException {
        byte[] message = {};

        RemctlQuitToken quitToken = new RemctlQuitToken(null, message);
        //not really anyhing to test. We are mostly checking that no exceptions
        //are thrown and that the token has a constructor that takes byte[]
        assertNotNull(quitToken);
    }

}
