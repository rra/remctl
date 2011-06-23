package org.eyrie.remctl.client;

import org.apache.commons.pool.impl.GenericObjectPool;

/**
 * A pool of remctl connections.
 * 
 * <p>
 * All commands sent using the pool should be sent with keep alive=true to
 * prevent the server from closing the connection. A server may close a
 * connection after a set amount of time, so set your max idle timeouts to be
 * less that the server timeout
 * </p>
 * 
 */
public class RemctlConnectionPool extends GenericObjectPool {

    /**
     * Creates a RemctlConnection pool with some intelligent defaults settings.
     * 
     * <p>
     * <ul>
     * <li>maxWait: 10 seconds</li>
     * <li>minEvictableIdleTimeMillis: 10 minutes</li>
     * <li>testOnBorrow: true</li>
     * <li>testWhileIdle: true</li>
     * <li>timeBetweenEvictionRunsMillis: 2 minutes</li>
     * </ul>
     * 
     * @param connectionFactory
     *            The RemctlConnectionFactory that will create new connections,
     *            and validate existing ones.
     */
    public RemctlConnectionPool(RemctlConnectionFactory connectionFactory) {
        super(connectionFactory);

        this.setMaxWait(10 * 1000);
        this.setMinEvictableIdleTimeMillis(10 * 60 * 1000);
        this.setTestOnBorrow(true);
        this.setTestWhileIdle(true);
        this.setTimeBetweenEvictionRunsMillis(2 * 60 * 1000);
    }

    /* (non-Javadoc)
     * @see org.apache.commons.pool.impl.GenericObjectPool#borrowObject()
     */
    @Override
    public RemctlConnection borrowObject() throws Exception {
        return (RemctlConnection) super.borrowObject();
    }

}
