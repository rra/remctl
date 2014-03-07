package org.eyrie.remctl.client;

import org.eyrie.remctl.core.RemctlCommandToken;

/**
 * A simple implementation of a remctl client.
 * 
 * <p>
 * It is thread safe, and does not use keep alive. Each call to execute opens a new remctl connection.
 * </p>
 * 
 * For better performance, consider using {@link PooledRemctlClient}.
 * 
 * 
 * @author pradtke
 * 
 */
public class SimpleRemctlClient implements RemctlClient {

    /**
     * The configuration to use when connecting.
     */
    private final Config config;

    /**
     * Create a SimpleRemctlClient that connects to hostname using default port and server principal.
     * 
     * @param hostname
     *            The host to connect to.
     */
    public SimpleRemctlClient(final String hostname) {
        this(hostname, 0, null);
    }

    /**
     * Create a SimpleRemctlClient with the given connection information.
     * 
     * @param hostname
     *            The host to connect to
     * @param port
     *            The port to connect on
     * @param serverPrincipal
     *            The server principal. If null, defaults to 'host/hostname'
     */
    public SimpleRemctlClient(final String hostname, final int port, final String serverPrincipal) {
        this(new Config.Builder().withHostname(hostname).withPort(port).withServerPrincipal(serverPrincipal).build());
    }

    /**
     * Create a new RemctlClient based on the configuration settings.
     * 
     * @param config
     *            The config settings to use.
     */
    public SimpleRemctlClient(final Config config) {
        this.config = config;
    }

    /*
     * (non-Javadoc)
     * 
     * @see org.eyrie.remctl.client.RemctlClient#execute(java.lang.String)
     */
    @Override
    public RemctlResponse execute(final String... arguments) {
        RemctlResponse response = this.executeAllowAnyStatus(arguments);
        if (response.getStatus() == null || response.getStatus() != 0) {
            throw new RemctlStatusException(response);
        }
        return response;

    }

    /*
     * (non-Javadoc)
     * 
     * @see org.eyrie.remctl.client.RemctlClient#executeAllowAnyStatus(java.lang. String)
     */
    @Override
    public RemctlResponse executeAllowAnyStatus(final String... arguments) {

        // FIXME: we should use our connection factory (which we can get from a
        // a setter or a constructor). That would allow us to use a mock
        // connection
        // for testing the functionality.
        RemctlConnection remctlConnection = new RemctlConnection(this.config);
        remctlConnection.connect();

        RemctlCommandToken command = new RemctlCommandToken(false, arguments);

        remctlConnection.writeToken(command);

        // remctl server should auto-close connection since keep alive is false
        return RemctlResponse.buildFromTokens(remctlConnection.readAllTokens());
    }

    @Override
    public int getPort() {
        return this.config.getPort();
    }

}
