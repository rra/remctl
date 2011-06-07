package org.eyrie.remctl;

import static org.junit.Assert.assertEquals;
import static org.junit.Assert.assertFalse;
import static org.junit.Assert.assertTrue;

import org.junit.Test;

public class MessageStatusTests {

    @Test
    public void testFromBytes() throws RemctlException {
        byte[] message = { 0 };

        RemctlStatusToken statusToken = new RemctlStatusToken(null, message);

        assertEquals("Status code should match", 0, statusToken.getStatus());
        assertTrue("Status was successful", statusToken.isSuccessful());

        message = new byte[] { -1 };

        statusToken = new RemctlStatusToken(null, message);

        assertEquals("Status code should match", -1, statusToken.getStatus());
        assertFalse("Status was unsuccessful", statusToken.isSuccessful());

    }

    //FIXME Add test for bad message lengths

}
