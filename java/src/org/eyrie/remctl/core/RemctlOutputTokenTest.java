package org.eyrie.remctl.core;

import static org.junit.Assert.assertEquals;

import org.eyrie.remctl.RemctlException;
import org.junit.Test;

public class RemctlOutputTokenTest {

    @Test
    public void testBytes() throws RemctlException {
        // -- test no message --
        byte[] message = new byte[] {
                 1, /*  stdout */
                0, 0, 0, 0 /* no length */
        };

        RemctlOutputToken outputToken = new RemctlOutputToken(null, message);

        assertEquals("Stream should match", 1,
                outputToken.getStream());
        assertEquals("Output should have no length", 0,
                outputToken.getOutput().length);

        // -- test with message --
        message = new byte[] {
                 2, /*  stderr */
                0, 0, 0, 3, /* length of 3 */
                66, 111, 111
        };

        outputToken = new RemctlOutputToken(null, message);

        assertEquals("Stream should match", 2,
                outputToken.getStream());
        assertEquals("Output should have expected length", 3,
                outputToken.getOutput().length);

        assertEquals("String output should match", "Boo",
                outputToken.getOutputAsString());
    }

}
