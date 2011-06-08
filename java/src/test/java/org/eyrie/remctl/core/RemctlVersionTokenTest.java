package org.eyrie.remctl.core;

import static org.junit.Assert.assertEquals;
import junit.framework.Assert;

import org.eyrie.remctl.RemctlErrorCode;
import org.eyrie.remctl.RemctlErrorException;
import org.eyrie.remctl.RemctlException;
import org.junit.Test;

public class RemctlVersionTokenTest {

    /**
     * Test that token can be created from bytes. Token has a single byte for
     * body
     * 
     * @throws RemctlException
     */
    @Test
    public void testFromBytes() throws RemctlException {
        byte[] message = { 9 };

        RemctlVersionToken versionToken = new RemctlVersionToken(null, message);
        assertEquals("version should match", 9,
                versionToken.getHighestSupportedVersion());
    }

    /**
     * Test handling of malformed message: invalid lengths, not enough miminum
     * bytes
     * 
     * @throws RemctlException
     */
    @Test
    public void testBadMessageLength() throws RemctlException {
        byte[] message = {};

        try {
            new RemctlVersionToken(null,
                    message);
            Assert.fail("excpetion expected");
        } catch (RemctlErrorException e) {
            assertEquals(RemctlErrorCode.ERROR_BAD_TOKEN, e.getErrorCode());
        }
        message = new byte[] { 5, 6 };

        try {
            new RemctlVersionToken(null,
                    message);
            Assert.fail("excpetion expected");
        } catch (RemctlErrorException e) {
            assertEquals(RemctlErrorCode.ERROR_BAD_TOKEN, e.getErrorCode());
        }

    }

}
