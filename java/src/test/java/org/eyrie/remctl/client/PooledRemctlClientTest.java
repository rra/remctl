package org.eyrie.remctl.client;

import static org.junit.Assert.assertEquals;
import static org.mockito.Matchers.any;
import static org.mockito.Mockito.doThrow;
import static org.mockito.Mockito.mock;
import static org.mockito.Mockito.verify;
import static org.mockito.Mockito.when;

import java.util.ArrayList;

import org.eyrie.remctl.core.RemctlCommandToken;
import org.eyrie.remctl.core.RemctlException;
import org.eyrie.remctl.core.RemctlToken;
import org.junit.Assert;
import org.junit.Before;
import org.junit.Test;
import org.mockito.ArgumentCaptor;

/**
 * Test aspects of a poole remctl client
 * 
 * @author pradtke
 * 
 */
public class PooledRemctlClientTest {

    /**
     * A mock connection
     */
    private RemctlConnection mockConnection;
    /**
     * A mock pool
     */
    private RemctlConnectionPool mockPool;

    /**
     * Create new mocks, and make the pool return the connection on borrow.
     * 
     * @throws Exception
     *             exception during borrow
     */
    @Before
    public void resetMocks() throws Exception {
        this.mockConnection = mock(RemctlConnection.class);
        this.mockPool = mock(RemctlConnectionPool.class);
        when(this.mockPool.borrowObject()).thenReturn(this.mockConnection);
    }

    /**
     * The remclt client must return remctl connections to a pool after use,
     * otherwise we would run out.
     * 
     * @throws Exception
     *             Exception thrown
     * 
     */
    @Test
    public void testConnectionReturned() throws Exception {
        //-------Test return if connection throws an exception
        doThrow(new RemctlException("Test object return with exception")).
                when(this.mockConnection).writeToken(any(RemctlToken.class));

        RemctlClient client = new PooledRemctlClient(this.mockPool);
        try {
            client.execute("test");
            Assert.fail("Excpetion expected");
        } catch (RemctlException e) {
            assertEquals("Test object return with exception", e.getMessage());
            //check that object was returned
            verify(this.mockPool).returnObject(this.mockConnection);
        }

        this.resetMocks();

        //-------Test return if connection doesn't throw an exception
        when(this.mockConnection.readAllTokens()).thenReturn(
                new ArrayList<RemctlToken>());
        client = new PooledRemctlClient(this.mockPool);
        client.executeAllowAnyStatus("test");
        //check that object was returned
        verify(this.mockPool).returnObject(this.mockConnection);
    }

    /**
     * Ensure that all remctl commands are sent with keep alive = true.
     * Otherwise the server closes the connection and the pool is useless
     */
    @Test
    public void testKeepAliveIsSet() {

        RemctlClient client = new PooledRemctlClient(this.mockPool);
        client.executeAllowAnyStatus("test");
        ArgumentCaptor<RemctlCommandToken> argument = ArgumentCaptor
                .forClass(RemctlCommandToken.class);
        verify(this.mockConnection).writeToken(argument.capture());
        assertEquals(true, argument.getValue().isKeepalive());
    }

}
