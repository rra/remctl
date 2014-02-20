package org.eyrie.remctl.client;

import static org.junit.Assert.assertEquals;
import static org.mockito.Matchers.argThat;
import static org.mockito.Mockito.mock;
import static org.mockito.Mockito.when;

import javax.security.auth.login.LoginContext;

import org.junit.Ignore;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.ArgumentMatcher;
import org.mockito.InjectMocks;
import org.mockito.Mock;
import org.mockito.runners.MockitoJUnitRunner;

/**
 * Check the sample CLI app.
 * 
 * @author pradtke
 * 
 */
@RunWith(MockitoJUnitRunner.class)
public class RemctlClientCliTest {

    /** Class under test. */
    @InjectMocks
    private RemctlClientCli clientCli;

    /** Mock dependency. */
    @Mock
    private RemctlClientFactory clientFactory;
    

    /**
     * Running with no args is an error.
     */
    @Test
    public void testNoArgsIsError() {
        assertEquals(1, this.clientCli.run());
    }

    /**
     * Testing with args, but no options.
     */
    @Test
    public void testArgsNoOptions() {
        // setup
        String hostname = "someserver.stanford.edu";
        int status = 5;
        RemctlClient client = mock(RemctlClient.class);
        RemctlResponse response = new RemctlResponse("out", "err", status);
        when(client.executeAllowAnyStatus("command", "command-arg")).thenReturn(response);
        Config config = new Config.Builder().withHostname(hostname).build();
        when(this.clientFactory.createClient(argThat(new IsMatchingConfig(config)))).thenReturn(client);

        // run
        // expect the run to return the status code from the remctl response
        assertEquals(status, this.clientCli.run(hostname, "command", "command-arg"));

    }

    /**
     * Testing with args that resemble options.
     */
    @Test
    public void testArgsThatLookLikeOptions() {
        // setup
        String hostname = "someserver.stanford.edu";
        int status = 5;
        RemctlClient client = mock(RemctlClient.class);
        RemctlResponse response = new RemctlResponse("out", "err", status);
        when(client.executeAllowAnyStatus("command", "-p")).thenReturn(response);
        Config config = new Config.Builder().withHostname(hostname).build();
        when(this.clientFactory.createClient(argThat(new IsMatchingConfig(config)))).thenReturn(client);

        // run
        // expect the run to return the status code from the remctl response
        assertEquals(status, this.clientCli.run(hostname, "command", "-p"));

    }

    /**
     * Testing with args and options.
     */
    @Test
    public void testArgsWithOptions() {
        // setup
        String hostname = "otherserver.stanford.edu";
        int status = 5;
        RemctlClient client = mock(RemctlClient.class);
        RemctlResponse response = new RemctlResponse("out", "err", status);
        when(client.executeAllowAnyStatus("command", "command-arg")).thenReturn(response);
        Config config = new Config.Builder().withHostname(hostname).withPort(567)
                .withServerPrincipal("service/some-principal").build();
        when(this.clientFactory.createClient(argThat(new IsMatchingConfig(config))))
                .thenReturn(client);

        // run
        // expect the run to return the status code from the remctl response
        assertEquals(status,
                this.clientCli.run("-s", "service/some-principal", "-p", "567", hostname, "command", "command-arg"));

    }

    /**
     * Testing with args and option to prompt user for auth.
     */
    @Test
    @Ignore("Prompting user requires valid JAAS config file, which we don't currently have set")
    public void testArgsWithPrompting() {
        // setup
        String hostname = "otherserver.stanford.edu";
        int status = 5;
        RemctlClient client = mock(RemctlClient.class);
        RemctlResponse response = new RemctlResponse("out", "err", status);
        when(client.executeAllowAnyStatus("command", "command-arg")).thenReturn(response);
        LoginContext loginContext = mock(LoginContext.class);
        Config config = new Config.Builder().withHostname(hostname).withLoginContext(loginContext).build();
        when(this.clientFactory.createClient(argThat(new IsMatchingConfig(config)))).thenReturn(client);

        // run
        // expect the run to return the status code from the remctl response
        assertEquals(status, this.clientCli.run("--prompt", hostname, "command", "command-arg"));

    }

    /**
     * Used to stub a match to a config object.
     * @author pradtke
     *
     */
    class IsMatchingConfig extends ArgumentMatcher<Config> {
        
            private Config config;
            
            public IsMatchingConfig(Config config) {
                this.config = config;
            }
            @Override
            public boolean matches(Object arg) {
                Config argument = (Config) arg;
                if (!this.config.getHostname().equals(argument.getHostname())) {
                    return false;
                }
                if (this.config.getPort() != argument.getPort()) {
                    return false;
                }
                
                if(!this.nullSafeEquals(this.config.getServerPrincipal(), argument.getServerPrincipal())) {
                    return false;
                }
                
                // LoginContext is too difficult to compare, so we simply check if both are null or not null
                return this.config.getLoginContext() == argument.getLoginContext() || (this.config.getLoginContext() != null && argument.getLoginContext() != null); 
            }
            
            /**
             * Null safe equals of two objects
             * @param object1
             * @param object2
             * @return true if both are null, or both are equal
             */
            private boolean nullSafeEquals(Object object1, Object object2) {
                if (object1 == object2) {
                    return true;
                }
                if (object1 == null) {
                    return false;
                }
                if (object2 == null) {
                    return false;
                }
                return true;
            }
        }

}
