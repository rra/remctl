package org.eyrie.remctl;

import static org.junit.Assert.assertEquals;

import org.junit.Assert;
import org.junit.Test;

public class RemctlErrorTokenTests {

    /**
     * Test converting valid bytes into messages
     * 
     * @throws RemctlException
     */
    @Test
    public void testFromBytes() throws RemctlException {

        // -- test no message --
        byte[] message = new byte[] {
                0, 0, 0, 1, /* error code */
                0, 0, 0, 0 /* no message */
        };

        RemctlErrorToken errorToken = new RemctlErrorToken(null, message);

        assertEquals("Error code should match", RemctlErrorCode.ERROR_INTERNAL,
                errorToken.getCode());
        assertEquals("No message expected in token", "",
                errorToken.getMessage());

        // -- test with message --
        message = new byte[] {
                0, 0, 0, 4, /* error code */
                0, 0, 0, 3, /* 3 characters */
                66, 111, 111
        };

        errorToken = new RemctlErrorToken(null, message);

        assertEquals("Error code should match",
                RemctlErrorCode.ERROR_BAD_COMMAND,
                errorToken.getCode());
        assertEquals("Message not expected", "Boo",
                errorToken.getMessage());
    }

    /**
     * Test an unexpected error value in message
     * 
     * @throws RemctlException
     */
    @Test
    public void testUnexpectedErrorToken() throws RemctlException {
        // -- test no message --
        byte[] message = new byte[] {
                1, 0, 0, 0, /* error code 4096*/
                0, 0, 0, 0, /* no message */
        };

        RemctlErrorToken errorToken = new RemctlErrorToken(null, message);

        //FIXME errorToken enum can't handle unknown error code
        assertEquals("Error code should match", 4096,
                errorToken.getCode().value);
        assertEquals("No message expected in token", "",
                errorToken.getMessage());
    }

    /**
     * Test handling of malformed message: invalid lengths, not enough miminum
     * bytes
     * 
     * @throws RemctlException
     */
    @Test
    public void testBadMessageLength() throws RemctlException {

        //---  command length mismatch ---
        byte[] message = new byte[] {
                0, 0, 0, 8, /* error code 8*/
                0, 0, 0, 5, /* expect 5 chars */
                66
        };

        try {
            new RemctlErrorToken(null, message);
            Assert.fail("Exception should have been thrown");
        } catch (IllegalStateException e) {
            assertEquals(
                    "Expected length mismatch: Command length is 5, but data length is 1",
                    e.getMessage());
        }

        //--- not enough bytes ---
        message = new byte[] {
                0, 0, 0, 3, /* error code 8*/
                0, 0, 0 /* missing byte */
        };
        try {
            new RemctlErrorToken(null, message);
            Assert.fail("Exception should have been thrown");
        } catch (IllegalArgumentException e) {
            assertEquals(
                    "Command data size is to small. Expected 8, but was 7",
                    e.getMessage());
        }

    }

}
