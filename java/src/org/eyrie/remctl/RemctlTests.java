/*
 * Written by Russ Allbery <rra@stanford.edu>
 * Copyright 2010 Board of Trustees, Leland Stanford Jr. University
 * 
 * See LICENSE for licensing terms.
 */
package org.eyrie.remctl;

import static org.junit.Assert.assertEquals;
import static org.junit.Assert.assertNull;

import org.junit.Test;

/**
 * JUnit test suite for the org.eyrie.remctl package.
 * 
 * @author Russ Allbery &lt;rra@stanford.edu&gt;
 */
public class RemctlTests {
    /**
     * Test method for {@link RemctlMessageCode#getCode(int)}.
     */
    @Test
    public void testGetCode() {
        assertEquals("MESSAGE_COMMAND", RemctlMessageCode.MESSAGE_COMMAND,
                RemctlMessageCode.getCode(1));
        assertNull("unknown message", RemctlMessageCode.getCode(42));
    }

    /**
     * Test method for {@link RemctlErrorException#getMessage()}.
     */
    @Test
    public void testGetMessage() {
        RemctlErrorException e = new RemctlErrorException(
                RemctlErrorCode.ERROR_INTERNAL);
        assertEquals("ERROR_INTERNAL", "Internal server failure (error 1)",
                e.getMessage());
        e = new RemctlErrorException(RemctlErrorCode.ERROR_TOOMUCH_DATA);
        assertEquals("ERROR_TOOMUCH_DATA",
                "Argument size exceeds server limit (error 8)",
                e.getMessage());
    }
}
