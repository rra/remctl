package org.eyrie.remctl.client;

import static org.junit.Assert.assertEquals;
import static org.mockito.Mockito.mock;
import static org.mockito.Mockito.when;

import org.eyrie.remctl.core.Utils;
import org.junit.Test;
import org.junit.runner.RunWith;
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
        when(this.clientFactory.createClient(hostname, Utils.DEFAULT_PORT, null)).thenReturn(client);

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
        when(this.clientFactory.createClient(hostname, Utils.DEFAULT_PORT, null)).thenReturn(client);

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
        when(this.clientFactory.createClient(hostname, 567, "service/some-principal"))
                .thenReturn(client);

        // run
        // expect the run to return the status code from the remctl response
        assertEquals(status,
                this.clientCli.run("-s", "service/some-principal", "-p", "567", hostname, "command", "command-arg"));

    }

}
