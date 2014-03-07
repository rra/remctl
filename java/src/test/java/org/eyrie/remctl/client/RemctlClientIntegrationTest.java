package org.eyrie.remctl.client;

import static org.junit.Assert.assertEquals;
import static org.junit.Assert.fail;

import org.eyrie.remctl.core.RemctlErrorCode;
import org.eyrie.remctl.core.RemctlErrorException;
import org.junit.Assert;
import org.junit.BeforeClass;
import org.junit.Ignore;
import org.junit.Test;

/**
 * These test assume you have deployed the sample `test-server-script` script and sample remctl config file on a server
 * and will perform integration tests against that script.
 * 
 * <p>
 * FIXME: these tests should be run against SimpleRemctlClient and PooledRemctlClient
 * 
 * <p>
 * On some systems you may to be explicit about the kerberos configuration, or need to use a SOCKS proxy to reach the
 * destination host.
 * 
 * <pre>
 * -DsocksProxyPort=1535  -DsocksProxyHost=localhost -Djava.security.krb5.realm=stanford.edu  -Djava.security.krb5.kdc=krb5auth1.stanford.edu:88
 * </pre>
 * 
 * @author pradtke
 * 
 */
public class RemctlClientIntegrationTest {

    /** The configuration to use in contacting the remctl server. */
    private static Config config;

    /** The command to call in the integration scripts. */
    private String cmdName = "test-server-script";

    /**
     * Setup things before test.
     */
    @BeforeClass
    public static void setup() {
        System.setProperty("java.security.auth.login.config", "gss_jaas.conf");

        // TODO: pull this config from system properties
        config = new Config.Builder().withHostname("venture.stanford.edu").withPort(5555).build();
    }

    /**
     * Test getting a response on stdout.
     */
    @Test
    public void testStdOut() {

        SimpleRemctlClient remctlClient = new SimpleRemctlClient(config);

        // repeat simply echos back the data
        RemctlResponse remctlResponse = remctlClient.execute(this.cmdName, "repeat", "Monkey", "Like", "Mango Man");

        String expectedOutput = "repeat Monkey Like Mango Man\n";

        assertEquals("Status is success", Integer.valueOf(0), remctlResponse.getStatus());
        assertEquals("Stdout should match", expectedOutput, remctlResponse.getStdOut());
        assertEquals("Stderr should be emtpy", "", remctlResponse.getStdErr());

    }

    /**
     * Test a command that prints to stderr and stdout.
     */
    @Test
    public void testMixedStream() {
        SimpleRemctlClient remctlClient = new SimpleRemctlClient(config);
        this.assertMixedStreams(remctlClient);
    }

    /**
     * Test getting a non-0 response.
     */
    @Test
    public void testStatusErr() {
        Integer expectedStatus = 78;

        SimpleRemctlClient remctlClient = new SimpleRemctlClient(config);

        // 'exit-err' simply exists with a status of 78
        RemctlResponse remctlResponse = remctlClient.executeAllowAnyStatus(this.cmdName, "exit-err");

        assertEquals("Status is success", expectedStatus, remctlResponse.getStatus());
        assertEquals("Stdout should  be emtpy", "", remctlResponse.getStdOut());
        assertEquals("Stderr should be emtpy", "", remctlResponse.getStdErr());

        // regular execture throws an exception
        try {
            remctlClient.execute(this.cmdName, "exit-err");
            fail("Exception expected");
        } catch (RemctlStatusException e) {
            assertEquals("Status is success", expectedStatus, e.getStatus());
            assertEquals("Stdout should  be emtpy", "", e.getStdOut());
            assertEquals("Stderr should be emtpy", "", e.getStdErr());
        }

    }

    /**
     * Test that we use the canonical hostname for setting the default server principal.
     */
    @Test
    @Ignore
    public void testHostnameIsAlias() {
        // FIXME: test against an alias for a host
        // hosts real name is tools3.stanford.edu
        SimpleRemctlClient remctlClient = new SimpleRemctlClient("tools.stanford.edu");
        this.assertMixedStreams(remctlClient);
    }

    /**
     * Test using a remctlClinetFactory.
     */
    @Test
    public void testClientFactory() {
        RemctlClientFactory factory = new RemctlClientFactory();
        this.assertMixedStreams(factory.createClient(config));
    }

    /**
     * Confirm client can be used multiple times.
     * 
     * @throws Exception
     *             exception from pool
     */
    @Test
    @Ignore
    public void testMultipleExecutes() throws Exception {
        RemctlConnectionFactory factory = new RemctlConnectionFactory();
        factory.setHostname("tools3.stanford.edu");

        RemctlConnectionPool pool = new RemctlConnectionPool(factory);

        PooledRemctlClient remctlClient = new PooledRemctlClient(pool);
        this.assertMixedStreams(remctlClient);
        this.assertMixedStreams(remctlClient);

        // clean up
        pool.close();
    }

    /**
     * Perform a standard test against a remctl command that prints to stderr and stdout.
     * 
     * @param remctlClient
     *            The remctl client to run execute with.
     */
    private void assertMixedStreams(final RemctlClient remctlClient) {
        RemctlResponse remctlResponse = remctlClient.execute(this.cmdName, "mix-stream");

        String expectedStdOutput = "This is stdout\nThis is more stdout\n";
        String expectedStdErr = "This is stderr\n";

        assertEquals("Status is success", Integer.valueOf(0), remctlResponse.getStatus());
        assertEquals("Stdout should match", expectedStdOutput, remctlResponse.getStdOut());
        assertEquals("Stderr should match", expectedStdErr, remctlResponse.getStdErr());
    }

    /**
     * Test behavior when error token is encountered.
     */
    @Test
    public void testErrorTokenUnknownCommand() {
        SimpleRemctlClient remctlClient = new SimpleRemctlClient(config);
        try {
            remctlClient.execute("no-such-command");

            Assert.fail("Error tokens should be exceptions");
        } catch (RemctlErrorException e) {
            assertEquals("Code shoud match", RemctlErrorCode.ERROR_UNKNOWN_COMMAND.value, e.getErrorCode());
        }

    }

    /**
     * Test behavior when error token is encountered because of acls.
     */
    @Test
    public void testErrorTokenAccess() {
        SimpleRemctlClient remctlClient = new SimpleRemctlClient(config);
        try {
            remctlClient.execute("not-allowed");

            Assert.fail("Error tokens should be exceptions");
        } catch (RemctlErrorException e) {
            assertEquals("Code shoud match", RemctlErrorCode.ERROR_ACCESS.value, e.getErrorCode());
        }

    }

    /**
     * Test enabling command validator.
     */
    @Test
    @Ignore
    public void testWithCommandValidation() {
        // ---setup connection and pool
        RemctlConnectionFactory factory = new RemctlConnectionFactory();
        factory.setHostname("acct-scripts-dev.stanford.edu");
        RemctlConnectionPool pool = new RemctlConnectionPool(factory);

        PooledRemctlClient remctlClient = new PooledRemctlClient(pool);
        CommandValidationStrategy validationStrategy = new CommandValidationStrategy("account-show", "show", "bob");
        factory.setValidationStrategy(validationStrategy);

        RemctlResponse reponse = remctlClient.execute("account-show", "list", "pradtke");
        assertEquals(0, reponse.getStatus().intValue());

    }
}
