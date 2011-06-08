/*
 * Written by Russ Allbery <rra@stanford.edu>
 * Copyright 2010 Board of Trustees, Leland Stanford Jr. University
 * 
 * See LICENSE for licensing terms.
 */
package org.eyrie.remctl.core;

import static org.junit.Assert.assertEquals;
import static org.junit.Assert.assertNotNull;
import static org.junit.Assert.assertNull;

import java.security.PrivilegedActionException;

import javax.security.auth.Subject;
import javax.security.auth.login.LoginContext;
import javax.security.auth.login.LoginException;

import org.eyrie.remctl.RemctlErrorCode;
import org.eyrie.remctl.RemctlErrorException;
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

    @Test
    public void testLogin() throws LoginException, PrivilegedActionException {
        System.setProperty("java.security.auth.login.config",
                "/Users/pradtke/Desktop/kerberos/remctl/java/gss_jaas.conf");
        LoginContext context = new LoginContext("RemctlClient");
        context.login();
        Subject subject = context.getSubject();
        assertNotNull(subject);

        System.out.println(subject.getPrincipals());

    }

}
