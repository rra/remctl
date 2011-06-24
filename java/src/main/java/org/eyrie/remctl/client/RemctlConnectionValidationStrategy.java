package org.eyrie.remctl.client;

/**
 * Allow plugable strategies for validation that a RemctlConnection is fit for
 * use.
 * <p>
 * Usually used by RemctlConnectionPool (in conjunction with
 * RemctlConnectionFactory) for validating connections that are in the pool.
 * Actions in isValid will also be performed on idle connections, to keep them
 * alive.
 * </p>
 * 
 * See {@link RemctlConnectionPool#getTestWhileIdle()} and
 * {@link RemctlConnectionPool#getTestOnBorrow()} for configuring when
 * {@link #isValid(RemctlConnection)} will be called, and
 * {@link RemctlConnectionFactory#validateObject(Object)} for where the strategy
 * is plugged in.
 * 
 * @author pradtke
 * 
 */
public interface RemctlConnectionValidationStrategy {

    /**
     * Determines if the connection is still valid for use.
     * <p>
     * When returning true, implementations must leave the connection in a state
     * where it can be re-used
     * </p>
     * 
     * @param connection
     *            The connection to test
     * @return true if the connection is ready for use, false otherwise
     */
    public boolean isValid(RemctlConnection connection);

}
