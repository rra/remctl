package org.eyrie.remctl.client;

import static org.junit.Assert.assertEquals;

import org.junit.Assert;
import org.junit.Test;

public class SimpleRemctlClientTest {

    /**
     * Test getting a response on std out
     */
    @Test
    public void testStdOut() {

        SimpleRemctlClient remctlClient = new SimpleRemctlClient(
                "acct-scripts-dev.stanford.edu");

        RemctlResponse remctlResponse = remctlClient.execute("account-show",
                "show", "bob");

        String expectedOutput = "Account bob\n"
                +
                "   account key: 1000007311\n"
                +
                "   type:        self\n"
                +
                "   status:      inactive\n"
                +
                "   description: \n"
                +
                "   owner:       person/468c983ee76911d1a32a2436000baa77 -- Edwards, Robert Scott (bob)\n"
                +
                "   service:     kerberos (inactive)\n"
                +
                "       setting: principal = bob\n"
                +
                "Sunetid\n"
                +
                "   entity key:  14780\n"
                +
                "   regid:       17faee88b44052d1564d9ebcbe5a9d52\n"
                +
                "   status:      enabled\n"
                +
                "   identifies:  person/468c983ee76911d1a32a2436000baa77 -- Edwards, Robert Scott (bob)\n";

        assertEquals("Status is success", Integer.valueOf(0),
                remctlResponse.getStatus());

        assertEquals("Stdout should match", expectedOutput,
                remctlResponse.getStdOut());

        assertEquals("Stderr should be emtpy", "",
                remctlResponse.getStdErr());

    }

    /**
     * Test a supported command, but call it with bad arguments so we get output
     * on stderror
     */
    @Test
    public void testStdErr() {
        SimpleRemctlClient remctlClient = new SimpleRemctlClient(
                "tools3.stanford.edu");
        this.assertStdError(remctlClient);

    }

    /**
     * Test that we use the canonical hostname for setting the default server
     * principal.
     */
    @Test
    public void testHostnameIsAlias() {
        //hosts real name is tools3.stanford.edu
        SimpleRemctlClient remctlClient = new SimpleRemctlClient(
                "tools.stanford.edu");
        this.assertStdError(remctlClient);
    }

    /**
     * Confirm client can be used multiple times
     */
    @Test
    public void testMultipleExecutes() {
        SimpleRemctlClient remctlClient = new SimpleRemctlClient(
                "tools3.stanford.edu");
        this.assertStdError(remctlClient);
        this.assertStdError(remctlClient);
    }

    /**
     * Execute and test the contents.
     * 
     * @param remctlClient
     *            The remctl client to run execute with.
     */
    private void assertStdError(SimpleRemctlClient remctlClient) {
        //note the [31m and [0m make the shell print colors, and don't appear as text when run from the cmdline
        String expectedErr = "[31mTicket.pm: error loading '67': \n[0merror loading '67':  at /usr/share/perl5/Remedy/Ticket.pm line 280\n";

        RemctlResponse remctlResponse = remctlClient.execute("ticket", "67");

        assertEquals("Status is success", Integer.valueOf(-1),
                remctlResponse.getStatus());

        assertEquals("Stdout should be emtpy", "",
                remctlResponse.getStdOut());

        assertEquals("Stderr should match", expectedErr,
                remctlResponse.getStdErr());
    }

    @Test
    public void testException() {
        SimpleRemctlClient remctlClient = new SimpleRemctlClient(
                "tools3.stanford.edu");
        remctlClient.execute("no-such-command");

        Assert.fail("Error tokens should be exceptions");

    }
}
