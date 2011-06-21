package org.eyrie.remctl.client;

import static org.junit.Assert.assertEquals;

import org.eyrie.remctl.RemctlErrorException;
import org.eyrie.remctl.RemctlStatusException;
import org.junit.Assert;
import org.junit.Before;
import org.junit.Test;

/**
 * FIXME: this test is highly dependent on stanford infrastructure. We should
 * mock up the dependencies
 * 
 * FIXME: these tests should be run against SimpleRemctlClient and
 * PooledRemctlClient
 * 
 * @author pradtke
 * 
 */
public class RemctlClientIntegrationTest {

    @Before
    /**
     * Setup things before test
     */
    public void setup() {
        System.setProperty("java.security.auth.login.config",
                "gss_jaas.conf");
    }

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
     * 
     * @throws Exception
     *             exception from pool
     */
    @Test
    public void testMultipleExecutes() throws Exception {
        RemctlConnectionFactory factory = new RemctlConnectionFactory();
        factory.setHostname("tools3.stanford.edu");

        RemctlConnectionPool pool = new RemctlConnectionPool(factory);

        PooledRemctlClient remctlClient = new PooledRemctlClient(
                pool);
        this.assertStdError(remctlClient);
        this.assertStdError(remctlClient);

        //clean up
        pool.close();
    }

    /**
     * Execute and test the contents.
     * 
     * @param remctlClient
     *            The remctl client to run execute with.
     */
    private void assertStdError(RemctlClient remctlClient) {
        //note the [31m and [0m make the shell print colors, and don't appear as text when run from the cmdline
        String expectedErr = "[31mTicket.pm: error loading '67': \n[0merror loading '67':  at /usr/share/perl5/Remedy/Ticket.pm line 280\n";

        try {
            remctlClient.execute("ticket", "67");
        } catch (RemctlStatusException statusException) {
            assertEquals("Status is success", Integer.valueOf(-1),
                    statusException.getStatus());

            assertEquals("Stdout should be emtpy", "",
                    statusException.getStdOut());

            assertEquals("Stderr should match", expectedErr,
                    statusException.getStdErr());
        }
    }

    /**
     * Test behavior when error token is encountered
     */
    @Test
    public void testErrorToken() {
        SimpleRemctlClient remctlClient = new SimpleRemctlClient(
                "tools3.stanford.edu");
        try {
            remctlClient.execute("no-such-command");

            Assert.fail("Error tokens should be exceptions");
        } catch (RemctlErrorException e) {
            assertEquals("Code shoud match", 5, e.getErrorCode());
        }

    }
}
