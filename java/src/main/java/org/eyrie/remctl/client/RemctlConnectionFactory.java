package org.eyrie.remctl.client;

import org.apache.commons.pool.BasePoolableObjectFactory;
import org.slf4j.Logger;
import org.slf4j.LoggerFactory;

/**
 * A connection factory for creating RemctlConnections.
 * 
 * <p>
 * A factory compatible with commons-pooling
 * 
 * <p>
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
     * The configuration to use with new connections.
     */
    private Config config;

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
        this(new Config.Builder().withHostname(hostname).build());
    }

    /**
     * Create factory that creates remctl connection based on the configuration settings.
     * 
     * @param config
     *            The config settings to use.
     */
    public RemctlConnectionFactory(final Config config) {
        this.config = config;
    }

    @Override
    public RemctlConnection makeObject() throws Exception {
        if (this.config == null) {
            throw new IllegalStateException("A hostname or Config must be provided to create new RemctlConnection");
        }
        RemctlConnection connection = new RemctlConnection(this.config);
        connection.connect();
        return connection;
    }

    /**
     * Get the hostname that connections are made to.
     * 
     * @return the hostname
     */
    public String getHostname() {
        return this.config == null ? null : this.config.getHostname();
    }

    /**
     * Set the hostname that connections are made to.
     * <p>
     * Use {@link Config} to change configuration options for the connections
     * </p>
     * .
     * 
     * <p>
     * Calling this method, overwrites any existing configuration, and uses the default port and server principal in
     * conjunction with the provided hostname
     * </p>
     * 
     * @param hostname
     *            the hostname to set
     */
    @Deprecated
    public void setHostname(final String hostname) {
        this.config = new Config.Builder().withHostname(hostname).build();
    }

    /**
     * The configuration to use when creating new connections.
     * 
     * @return the config
     */
    public Config getConfig() {
        return this.config;
    }

    /**
     * Set the configuration to use when creating new connection.
     * 
     * @param config
     *            the config to set
     */
    public void setConfig(final Config config) {
        this.config = config;
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
