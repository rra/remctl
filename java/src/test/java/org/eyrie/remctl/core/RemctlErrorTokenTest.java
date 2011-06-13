package org.eyrie.remctl.core;

import static org.junit.Assert.assertEquals;

import org.eyrie.remctl.RemctlErrorCode;
import org.eyrie.remctl.RemctlErrorException;
import org.junit.Assert;
import org.junit.Ignore;
import org.junit.Test;

/**
 * Test RemctlErrorToken conversion
 * 
 * @author pradtke
 * 
 */
public class RemctlErrorTokenTest {

    /**
     * Test converting valid application bytes into messages
     * 
     */
    @Test
    public void testFromBytes() {

        // -- test no message --
        byte[] message = new byte[] {
                0, 0, 0, 1, /* error code */
                0, 0, 0, 0 /* no message */
        };

        RemctlErrorToken errorToken = new RemctlErrorToken(message);

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

        errorToken = new RemctlErrorToken(message);

        assertEquals("Error code should match",
                RemctlErrorCode.ERROR_BAD_COMMAND,
                errorToken.getCode());
        assertEquals("Message not expected", "Boo",
                errorToken.getMessage());
    }

    /**
     * Test an unexpected error value in message
     * 
     */
    @Test
    @Ignore("Alternate error codes not supported yet")
    public void testUnexpectedErrorToken() {
        // -- test no message --
        byte[] message = new byte[] {
                1, 0, 0, 0, /* error code 4096*/
                0, 0, 0, 0, /* no message */
        };

        RemctlErrorToken errorToken = new RemctlErrorToken(message);

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
     */
    @Test
    public void testBadMessageLength() {

        //---  command length mismatch ---
        byte[] message = new byte[] {
                0, 0, 0, 8, /* error code 8*/
                0, 0, 0, 5, /* expect 5 chars */
                66
        };

        try {
            new RemctlErrorToken(message);
            Assert.fail("Exception should have been thrown");
        } catch (RemctlErrorException e) {
            assertEquals(2, e.getErrorCode());
        }

        //--- not enough bytes ---
        message = new byte[] {
                0, 0, 0, 3, /* error code 8*/
                0, 0, 0 /* missing byte */
        };
        try {
            new RemctlErrorToken(message);
            Assert.fail("Exception should have been thrown");
        } catch (RemctlErrorException e) {
            assertEquals(2, e.getErrorCode());
        }

    }

}
