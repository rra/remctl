package org.eyrie.remctl;

import static org.junit.Assert.assertEquals;

import org.junit.Test;

public class SimpleRemctlClientTest {

    @Test
    public void testStdOut() {

        SimpleRemctlClient remctlClient = new SimpleRemctlClient(
                "acct-scripts-dev.stanford.edu");

        remctlClient.execute("account-show", "show", "bob");

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
                remctlClient.returnStatus);

        assertEquals("Stdout should match", expectedOutput,
                remctlClient.getStdOutResponse());

        assertEquals("Stderr should be emtpy", "",
                remctlClient.getStdErrResponse());

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
     * Execute and test the contents.
     * 
     * @param remctlClient
     *            The remctl client to run execute with.
     */
    private void assertStdError(SimpleRemctlClient remctlClient) {
        //note the [31m and [0m make the shell print colors, and don't appear as text when run from the cmdline
        String expectedErr = "[31mTicket.pm: error loading '67': \n[0merror loading '67':  at /usr/share/perl5/Remedy/Ticket.pm line 280\n";

        remctlClient.execute("ticket", "67");

        assertEquals("Status is success", Integer.valueOf(-1),
                remctlClient.returnStatus);

        assertEquals("Stdout should be emtpy", "",
                remctlClient.getStdOutResponse());

        assertEquals("Stderr should match", expectedErr,
                remctlClient.getStdErrResponse());
    }

    @Test
    public void testException() {
        SimpleRemctlClient remctlClient = new SimpleRemctlClient(
                "tools3.stanford.edu");
        remctlClient.execute("no-such-command");

        assertEquals("Error token should match",
                RemctlErrorCode.ERROR_UNKNOWN_COMMAND,
                remctlClient.errorToken.getCode());

    }
}
