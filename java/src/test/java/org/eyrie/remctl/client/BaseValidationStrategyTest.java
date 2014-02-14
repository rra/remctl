package org.eyrie.remctl.client;

import static org.junit.Assert.assertFalse;
import static org.junit.Assert.assertTrue;
import static org.mockito.Matchers.any;
import static org.mockito.Mockito.mock;
import static org.mockito.Mockito.spy;
import static org.mockito.Mockito.when;

import java.util.Arrays;
import java.util.Date;
import java.util.List;

import org.eyrie.remctl.core.RemctlToken;
import org.junit.Test;

/**
 * Test the base validation strategy.
 * 
 * @author pradtke
 * 
 */
public class BaseValidationStrategyTest {
	
	/**
	 * Create a new stubbed connection.
	 * 
	 * @param minutesSinceCreate
	 *            time elapsed since connection created.
	 * @param extraData
	 *            Extra stale data on the connection
	 * @param tokens
	 *            tokens returned
	 * @return The stubbed connection
	 * 
	 */
	static public RemctlConnection buildMockConnection(int minutesSinceCreate,
			boolean extraData, RemctlToken... tokens) {
		RemctlConnection mockConnection = mock(RemctlConnection.class);
		Date created = new Date(System.currentTimeMillis()
				- (60L * 1000 * minutesSinceCreate));
		when(mockConnection.getConnectionEstablishedTime()).thenReturn(created);
		when(mockConnection.hasPendingData()).thenReturn(extraData);

		List<RemctlToken> tokenList = Arrays.asList(tokens);
		when(mockConnection.readAllTokens()).thenReturn(tokenList);
		return mockConnection;
	}

	/**
	 * Test that an exception from check connection results in a false during a
	 * validate.
	 * 
	 * @throws Exception
	 *             exception
	 */
	@Test
	public void testHandleException() throws Exception {

		// create a valid connection, but make 'checkConnection' throw an
		// exception
		RemctlConnection mockConnection = buildMockConnection(5, false);
		BaseValidationStrategy strategy = spy(new BaseValidationStrategy());
		when(strategy.checkConnection(any(RemctlConnection.class))).thenThrow(
				new RuntimeException("exception checking"));
		assertFalse(strategy.isValid(mockConnection));

	}

	/**
	 * Test the base validator
	 */
	@Test
	public void testValidate() {
		// check failure conditons
		BaseValidationStrategy strategy = new BaseValidationStrategy();
		testBaseFailedValidations(strategy);

		assertTrue(strategy.isValid(buildMockConnection(0, false)));
		assertTrue(strategy.isValid(buildMockConnection(54, false)));

	}

	/**
	 * Assert the failure conditions for BaseValidationStrategy.
	 * 
	 * <p>
	 * This can be called by subclasses to assert that they didn't
	 * break/override expected functionality in the base validator
	 * 
	 * @param validationStrategy
	 *            The validation strategy to test
	 * 
	 */
	static public void testBaseFailedValidations(
			BaseValidationStrategy validationStrategy) {
		// test stale token data
		assertFalse(validationStrategy.isValid(buildMockConnection(0, true)));

		// test max lifetime
		assertFalse(validationStrategy.isValid(buildMockConnection(60, false)));

	}
}
