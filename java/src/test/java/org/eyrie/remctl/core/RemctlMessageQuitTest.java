package org.eyrie.remctl.core;

import static org.junit.Assert.assertEquals;
import static org.junit.Assert.assertNotNull;
import junit.framework.Assert;

import org.eyrie.remctl.RemctlErrorException;
import org.junit.Test;

/**
 * Test conversion to/from streams and back to RemctlMessageQuit tokens
 * 
 * @author pradtke
 * 
 */
public class RemctlMessageQuitTest {

    /**
     * Test that token can be created from bytes. Token has no message body so
     * byte array is empty
     * 
     */
    @Test
    public void testFromBytes() {
        byte[] message = {};

        RemctlQuitToken quitToken = new RemctlQuitToken(message);
        //not really anyhing to test. We are mostly checking that no exceptions
        //are thrown and that the token has a constructor that takes byte[]
        assertNotNull(quitToken);

    }

    /**
     * Test handling of malformed message: invalid lengths, not enough miminum
     * bytes, etc
     */
    @Test
    public void testBadMessageLength() {
        byte[] message = { 0 };

        try {
            new RemctlQuitToken(message);
            Assert.fail("Exception expected");
        } catch (RemctlErrorException e) {
            assertEquals(2, e.getErrorCode());
        }

    }

}
