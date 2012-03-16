package org.eyrie.remctl.client;

import org.eyrie.remctl.core.RemctlCommandToken;
import org.eyrie.remctl.core.RemctlException;
import org.slf4j.Logger;
import org.slf4j.LoggerFactory;

/**
 * Remctl Client that uses a pool of connections.
 * 
 * <p>
 * Keep alive is used to keep the connections open after a command is sent
 * </p>
 * 
 * @author pradtke
 * 
 */
public class PooledRemctlClient implements RemctlClient {

    /**
     * Allow logging.
     */
    static final Logger logger = LoggerFactory.getLogger(PooledRemctlClient.class);

    /**
     * Our connection pool.
     */
    private RemctlConnectionPool pool;

    /**
     * Create a client that uses a pool of connections.
     * 
     * @param remctlConnectionPool
     *            The source of our connections
     */
    public PooledRemctlClient(final RemctlConnectionPool remctlConnectionPool) {
        this.pool = remctlConnectionPool;
    }

    @Override
    public RemctlResponse execute(final String... arguments) {
        RemctlResponse response = this.executeAllowAnyStatus(arguments);
        if (response.getStatus() == null || response.getStatus() != 0) {
            throw new RemctlStatusException(response);
        }
        return response;
    }

    @Override
    public RemctlResponse executeAllowAnyStatus(final String... arguments) {
        RemctlCommandToken command = new RemctlCommandToken(true, arguments);

        RemctlConnection remctlClient;
        try {
            remctlClient = this.pool.borrowObject();
        } catch (RemctlException e) {
            // just rethrow
            throw e;
        } catch (Exception e) {
            throw new RemctlException("Unable to get connection from pool", e);
        }

        try {
            remctlClient.writeToken(command);
            // remctl server should auto-close connection since keep alive is
            // false
            return RemctlResponse.buildFromTokens(remctlClient.readAllTokens());
        } finally {
            try {
                this.pool.returnObject(remctlClient);
            } catch (Exception e) {
                logger.debug("Unable to return connection: " + e);
                // ignore exception, since throwing would mask any excpetions
                // thrown in the try block.
            }
        }
    }

    @Override
    public int getPort() {
        try {
            return this.pool.borrowObject().getPort();
        } catch (Exception e) {
            return -1;
        }
    }
}
