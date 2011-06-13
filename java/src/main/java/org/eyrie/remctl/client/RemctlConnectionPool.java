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
 * FIXME: Do we even need this? Using generic object pool may be sufficient.
 */
public class RemctlConnectionPool extends GenericObjectPool {

    /* (non-Javadoc)
     * @see org.apache.commons.pool.impl.GenericObjectPool#borrowObject()
     */
    @Override
    public RemctlConnection borrowObject() throws Exception {
        return (RemctlConnection) super.borrowObject();
    }

}
