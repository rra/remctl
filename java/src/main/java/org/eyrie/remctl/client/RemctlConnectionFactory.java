package org.eyrie.remctl.client;

import java.util.List;

import org.apache.commons.pool.BasePoolableObjectFactory;
import org.eyrie.remctl.core.RemctlNoopToken;
import org.eyrie.remctl.core.RemctlToken;
import org.eyrie.remctl.core.RemctlVersionToken;
import org.slf4j.Logger;
import org.slf4j.LoggerFactory;

/**
 * A connection factory for creating RemctlConnections.
 * 
 * <p>
 * A factory compatible with commons-pooling
 * 
 * 
 * FIXME: add some logging. Excpetions may be lost, so try-catch our code and
 * log what exceptions we generate
 * 
 * FIXME: add options for validating a remctl connection - remctl connection
 * inputstream.avialble() has bytes, then we're invalid since an earlier client
 * didn;t read all of its tokens. -call quit remctl call on server to keep
 * connection alive/confirm its up
 * 
 * @author pradtke
 * 
 */
public class RemctlConnectionFactory extends BasePoolableObjectFactory {

    /**
     * Allow logging
     */
    static final Logger logger = LoggerFactory
            .getLogger(RemctlConnectionFactory.class);
    /**
     * The hostname to connect to.
     */
    private String hostname;

    /**
     * the port to connect to.
     */
    private int port = 4373;

    /**
     * The server principal
     */
    private String serverPrincipal;

    @Override
    public RemctlConnection makeObject() throws Exception {
        RemctlConnection connection = new RemctlConnection(this.hostname,
                this.port,
                this.serverPrincipal);
        connection.connect();
        return connection;
    }

    /**
     * @return the hostname
     */
    public String getHostname() {
        return this.hostname;
    }

    /**
     * @param hostname
     *            the hostname to set
     */
    public void setHostname(String hostname) {
        this.hostname = hostname;
    }

    /**
     * @return the port
     */
    public int getPort() {
        return this.port;
    }

    /**
     * @param port
     *            the port to set
     */
    public void setPort(int port) {
        this.port = port;
    }

    /**
     * @return the serverPrincipal
     */
    public String getServerPrincipal() {
        return this.serverPrincipal;
    }

    /**
     * @param serverPrincipal
     *            the serverPrincipal to set
     */
    public void setServerPrincipal(String serverPrincipal) {
        this.serverPrincipal = serverPrincipal;
    }

    /* (non-Javadoc)
     * @see org.apache.commons.pool.BasePoolableObjectFactory#destroyObject(java.lang.Object)
     */
    @Override
    public void destroyObject(Object obj) throws Exception {
        RemctlConnection connection = (RemctlConnection) obj;
        connection.close();
    }

    /**
     * Validate the RemctlConnection.
     * 
     * <p>
     * A connection is considered valid if:
     * <ul>
     * <li>A NOOP token results in a response of NOOP or VERSION</li>
     * <li>The connection has been open less than 55 minutes</li>
     * </ul>
     */
    @Override
    public boolean validateObject(Object obj) {
        RemctlConnection connection = (RemctlConnection) obj;

        /**
         * Remctl server closes connection after an hour.
         */
        long now = System.currentTimeMillis();
        long elapsedTime = now
                - connection.getConnectionEstablishedTime().getTime();
        //FIXME: make max life span configurable
        if (elapsedTime > 55 * 60 * 10000) {
            logger.debug("Connection open 55 minutes. Marking invalid");
            return false;
        }

        /**
         * Read any unread, stale tokens from a previous checkout
         */
        while (connection.hasPendingData()) {
            connection.readAllTokens();
        }

        RemctlNoopToken noopToken = new RemctlNoopToken();
        connection.writeToken(noopToken);
        List<RemctlToken> tokens = connection.readAllTokens();
        if (tokens.size() != 1) {
            logger.warn(
                    "Unexpected number of tokens returned in valdate ({}). Marking invalid",
                    tokens.size());
        }

        RemctlToken token = tokens.get(0);
        if (token instanceof RemctlNoopToken) {
            return true;
        } else if (token instanceof RemctlVersionToken) {
            //Server doesn't support protocol version 3. That is OK.
            return true;
        } else {
            logger.warn("Unexpected token returned from NOOP message: {}",
                    token);
        }
        //FIXME: make sure there is no more date in the output stream
        //FIXME: the remctl server closes a connection after an hour (even if active)
        //so we need a way to 'invalidate' connections after an hour.
        return true;

    }

}
