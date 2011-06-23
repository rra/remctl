package org.eyrie.remctl.core;

import static org.junit.Assert.assertEquals;
import junit.framework.Assert;

import org.eyrie.remctl.RemctlErrorCode;
import org.eyrie.remctl.RemctlErrorException;
import org.junit.Test;

/**
 * Test conversion to/from streams and back to RemctlVersionTokenTest tokens
 * 
 * @author pradtke
 * 
 */
public class RemctlVersionTokenTest {

    /**
     * Test that token can be created from bytes. Token has a single byte for
     * body
     * 
     */
    @Test
    public void testFromBytes() {
        byte[] message = { 9 };

        RemctlVersionToken versionToken = new RemctlVersionToken(message);
        assertEquals("version should match", 9,
                versionToken.getHighestSupportedVersion());
    }

    /**
     * Test handling of malformed message: invalid lengths, not enough miminum
     * bytes
     * 
     */
    @Test
    public void testBadMessageLength() {
        byte[] message = {};

        try {
            new RemctlVersionToken(
                    message);
            Assert.fail("exception expected");
        } catch (RemctlErrorException e) {
            assertEquals(RemctlErrorCode.ERROR_BAD_TOKEN.value,
                    e.getErrorCode());
        }
        message = new byte[] { 5, 6 };

        try {
            new RemctlVersionToken(
                    message);
            Assert.fail("exception expected");
        } catch (RemctlErrorException e) {
            assertEquals(RemctlErrorCode.ERROR_BAD_TOKEN.value,
                    e.getErrorCode());
        }

    }

}
