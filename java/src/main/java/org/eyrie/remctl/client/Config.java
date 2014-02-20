package org.eyrie.remctl.client;

import javax.security.auth.login.LoginContext;

import org.eyrie.remctl.core.Utils;

/**
 * Fluid API for setting configuration options that can be used for creating a RemctlConnection or Client.
 * 
 * @author pradtke
 * 
 */
public final class Config {

    /**
     * hostname to connect to.
     */
    private String hostname;

    /**
     * port to connect on.
     */
    private int port = Utils.DEFAULT_PORT;

    /**
     * Server principal.
     */
    private String serverPrincipal;

    /**
     * A custom login context to use when connecting.
     */
    private LoginContext loginContext;

    private Config(final Builder builder) {
        this.hostname = builder.hostname;
        this.port = builder.port;
        this.serverPrincipal = builder.serverPrincipal;
        this.loginContext = builder.loginContext;
    }

    /**
     * @return the hostname
     */
    public String getHostname() {
        return this.hostname;
    }

    /**
     * @return the port
     */
    public int getPort() {
        return this.port;
    }

    /**
     * @return the serverPrincipal
     */
    public String getServerPrincipal() {
        return this.serverPrincipal;
    }

    /**
     * @return the loginContext
     */
    public LoginContext getLoginContext() {
        return this.loginContext;
    }

    /**
     * Simplify building a configuration.
     * 
     * @author pradtke
     * 
     */
    public static class Builder {

        private String hostname;
        private int port;
        private String serverPrincipal;
        private LoginContext loginContext;

        /**
         * The hostname to connect to.
         * 
         * @param hostname
         *            the hostname
         * 
         * @return the builder
         */
        public Builder withHostname(final String hostname) {
            this.hostname = hostname;
            return this;
        }

        /**
         * The port to connect on. Default is 4347
         * 
         * @param port
         *            the port to use
         * @return the builder
         */
        public Builder withPort(final int port) {
            this.port = port;
            return this;
        }

        /**
         * The server principal to use. Defaults to host/servername.
         * 
         * @param serverPrincipal
         *            the server prinicpal to use
         * @return the builder
         */
        public Builder withServerPrincipal(final String serverPrincipal) {
            this.serverPrincipal = serverPrincipal;
            return this;
        }

        /**
         * Set the login context to use when authentication to remctl.
         * 
         * @param loginContext
         *            The context
         * @return the builder
         */
        public Builder withLoginContext(final LoginContext loginContext) {
            this.loginContext = loginContext;
            return this;
        }

        /**
         * Create the configuration.
         * 
         * @return The Remctl config
         */
        public Config build() {
            this.validate();
            return new Config(this);
        }

        /**
         * Validate any required fields.
         */
        private void validate() {
            if (this.hostname == null || this.hostname.length() == 0) {
                throw new IllegalArgumentException("hostname may not be blank");
            }
            if (this.port == 0) {
                this.port = Utils.DEFAULT_PORT;
            }
            if (this.port < 1) {
                throw new IllegalArgumentException("port must be greater than 0 " + this.port);
            }
        }
    }

}
