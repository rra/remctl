package org.eyrie.remctl.client;

import org.apache.commons.pool.BasePoolableObjectFactory;
import org.eyrie.remctl.core.Utils;
import org.slf4j.Logger;
import org.slf4j.LoggerFactory;

/**
 * A connection factory for creating RemctlConnections.
 * 
 * <p>
 * A factory compatible with commons-pooling
 * 
 * 
 * FIXME: add some logging. Excpetions may be lost, so try-catch our code and log what exceptions we generate
 * 
 * @author pradtke
 * 
 */
public class RemctlConnectionFactory extends BasePoolableObjectFactory {

    /**
     * Allow logging.
     */
    static final Logger logger = LoggerFactory.getLogger(RemctlConnectionFactory.class);
    /**
     * The hostname to connect to.
     */
    private String hostname;

    /**
     * the port to connect to.
     */
    private int port = Utils.DEFAULT_PORT;

    /**
     * The server principal.
     */
    private String serverPrincipal;

    /**
     * Set a default validation strategy.
     */
    private RemctlConnectionValidationStrategy validationStrategy = new BaseValidationStrategy();

    /**
     * Default constructor. Setter methods should be used for configuration.
     */
    public RemctlConnectionFactory() {

    }

    /**
     * Create a RemctlConnectionFactory that creates connections connected to hostname on the default port.
     * 
     * @param hostname
     *            the hostname connections should connect to.
     */
    public RemctlConnectionFactory(final String hostname) {
        this.hostname = hostname;
    }

    @Override
    public RemctlConnection makeObject() throws Exception {
        RemctlConnection connection = new RemctlConnection(this.hostname, this.port, this.serverPrincipal);
        connection.connect();
        return connection;
    }

    /**
     * Get the hostname that connections are made to.
     * 
     * @return the hostname
     */
    public String getHostname() {
        return this.hostname;
    }

    /**
     * Set the hostname that connections are made to.
     * 
     * @param hostname
     *            the hostname to set
     */
    public void setHostname(final String hostname) {
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
    public void setPort(final int port) {
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
    public void setServerPrincipal(final String serverPrincipal) {
        this.serverPrincipal = serverPrincipal;
    }

    /*
     * (non-Javadoc)
     * 
     * @see org.apache.commons.pool.BasePoolableObjectFactory#destroyObject(java. lang.Object)
     */
    @Override
    public void destroyObject(final Object obj) throws Exception {
        RemctlConnection connection = (RemctlConnection) obj;
        connection.close();
    }

    /**
     * Validate the RemctlConnection using the <code>validationStrategy</code>.
     * 
     * @param obj
     *            the object to validate.
     * @return true if object if valid, false otherwise
     * 
     */
    @Override
    public boolean validateObject(final Object obj) {
        RemctlConnection connection = (RemctlConnection) obj;
        return this.validationStrategy.isValid(connection);

    }

    /**
     * @param validationStrategy
     *            the validationStrategy to set
     */
    public void setValidationStrategy(final RemctlConnectionValidationStrategy validationStrategy) {
        this.validationStrategy = validationStrategy;
    }

    /**
     * @return the validationStrategy
     */
    public RemctlConnectionValidationStrategy getValidationStrategy() {
        return this.validationStrategy;
    }

}
