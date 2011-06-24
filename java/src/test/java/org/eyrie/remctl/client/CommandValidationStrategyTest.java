package org.eyrie.remctl.client;

import static org.junit.Assert.assertArrayEquals;
import static org.junit.Assert.assertFalse;
import static org.junit.Assert.assertTrue;
import static org.mockito.Mockito.mock;
import static org.mockito.Mockito.verify;
import static org.mockito.Mockito.when;

import java.util.Arrays;
import java.util.Date;
import java.util.List;

import org.eyrie.remctl.RemctlErrorCode;
import org.eyrie.remctl.core.RemctlCommandToken;
import org.eyrie.remctl.core.RemctlErrorToken;
import org.eyrie.remctl.core.RemctlOutputToken;
import org.eyrie.remctl.core.RemctlStatusToken;
import org.eyrie.remctl.core.RemctlToken;
import org.junit.Test;
import org.mockito.ArgumentCaptor;

/**
 * Test command validation strategy
 * 
 * @author pradtke
 * 
 */
public class CommandValidationStrategyTest {

    /**
     * A mock connection
     */
    private RemctlConnection mockConnection;

    /**
     * Class under test
     */
    CommandValidationStrategy validationStrategy = new CommandValidationStrategy();

    /**
     * Create a new stubbed connection.
     * 
     * @param minutesSinceCreate
     *            time elapsed since connection created.
     * @param extraData
     *            Extra stale data on the connection
     * @param tokens
     *            tokens returned
     * 
     */
    public void buildMocks(int minutesSinceCreate, boolean extraData,
            RemctlToken... tokens) {
        this.mockConnection = mock(RemctlConnection.class);
        Date created = new Date(System.currentTimeMillis()
                - (minutesSinceCreate * 60 * 100));
        when(this.mockConnection.getConnectionEstablishedTime()).thenReturn(
                created);
        when(this.mockConnection.hasPendingData()).thenReturn(extraData);

        List<RemctlToken> tokenList = Arrays.asList(tokens);
        when(this.mockConnection.readAllTokens()).thenReturn(tokenList);

    }

    /**
     * Confirm inherited validation works.
     * 
     * FIXME: refactor this so that it can be pasted in a strategy and can
     * validate others.
     */
    @Test
    public void inheritedIsValidTest() {
        //test stale token data
        this.buildMocks(0, true);
        assertFalse(this.validationStrategy.isValid(this.mockConnection));

        //test max lifetime
        this.buildMocks(60, false);
        assertFalse(this.validationStrategy.isValid(this.mockConnection));

    }

    /**
     * Test additional isValid implementation.
     */
    @Test
    public void isValidExtensionTest() {
        //no success token
        this.buildMocks(0, false);
        assertFalse(this.validationStrategy.isValid(this.mockConnection));

        //non-0 status
        this.buildMocks(0, false, new RemctlStatusToken(78));
        assertFalse(this.validationStrategy.isValid(this.mockConnection));

        //error token returned
        this.buildMocks(0, false, new RemctlErrorToken(
                RemctlErrorCode.ERROR_ACCESS));
        assertFalse(this.validationStrategy.isValid(this.mockConnection));

        //successful response status
        this.buildMocks(0, false, new RemctlStatusToken(0));
        assertTrue(this.validationStrategy.isValid(this.mockConnection));

        //successful response status
        this.buildMocks(0, false,
                new RemctlOutputToken(1, "test data".getBytes()),
                new RemctlStatusToken(0));
        assertTrue(this.validationStrategy.isValid(this.mockConnection));

    }

    /**
     * Test setting commands
     */
    @Test
    public void testCommandChange() {
        /**
         * Confirm default command
         */
        this.buildMocks(0, false, new RemctlStatusToken(0));
        this.validationStrategy.isValid(this.mockConnection);

        ArgumentCaptor<RemctlCommandToken> argument = ArgumentCaptor
                .forClass(RemctlCommandToken.class);
        verify(this.mockConnection).writeToken(argument.capture());
        assertArrayEquals(new String[] { "noop" }, argument.getValue()
                .getArgs());

        /**
         * Confirm command change works.
         */
        this.buildMocks(0, false, new RemctlStatusToken(0));

        String[] commands = { "hello", "jello" };
        this.validationStrategy.setCommands(commands);
        this.validationStrategy.isValid(this.mockConnection);
        verify(this.mockConnection).writeToken(argument.capture());
        assertArrayEquals(commands, argument.getValue()
                .getArgs());
    }
}
