package org.eyrie.remctl.core;

import static org.junit.Assert.assertEquals;
import static org.junit.Assert.assertFalse;
import static org.junit.Assert.assertTrue;
import junit.framework.Assert;

import org.junit.Test;

/**
 * Test MessageStatusToken conversion
 * 
 * @author pradtke
 * 
 */
public class RemctlMessageStatusTest {

    /**
     * Test building the token from command specific bytes
     */
    @Test
    public void testFromBytes() {
        byte[] message = { 0 };

        RemctlStatusToken statusToken = new RemctlStatusToken(message);

        assertEquals("Status code should match", 0, statusToken.getStatus());
        assertTrue("Status was successful", statusToken.isSuccessful());

        message = new byte[] { -1 };

        statusToken = new RemctlStatusToken(message);

        assertEquals("Status code should match", -1, statusToken.getStatus());
        assertFalse("Status was unsuccessful", statusToken.isSuccessful());

    }

    /**
     * Test handling of malformed message: invalid lengths, not enough miminum
     * bytes, etc
     */
    @Test
    public void testBadMessageLength() {
        byte[] message = {};

        try {
            new RemctlStatusToken(message);
            Assert.fail("Exception expected");
        } catch (RemctlErrorException e) {
            assertEquals(2, e.getErrorCode());
        }

        message = new byte[] { 1, 1 };

        try {
            new RemctlStatusToken(message);
            Assert.fail("Exception expected");
        } catch (RemctlErrorException e) {
            assertEquals(2, e.getErrorCode());
        }

    }
}
