package org.eyrie.remctl.client;

import org.eyrie.remctl.RemctlStatusException;
import org.eyrie.remctl.core.RemctlCommandToken;

/**
 * A simplified interface for a remctl client.
 * 
 * It is thread safe, and does not use keep alive. Each call to execute opens a
 * new remctl connection.
 * 
 * <p>
 * 
 * TODO: create connection pool form remctlclient as a different class
 * 
 * @author pradtke
 * 
 */
public class SimpleRemctlClient implements RemctlClient {

    /**
     * hostname to connect to
     */
    private final String hostname;

    /**
     * port to connect on
     */
    private final int port;

    /**
     * Server princicpal
     */
    String serverPrincipal;

    /**
     * Create a simple Remctl client that connects to hostname using default
     * port and server prinicpals
     * 
     * @param hostname
     *            The host to connect to.
     */
    public SimpleRemctlClient(String hostname) {
        this(hostname, 0, null);
    }

    /**
     * Create a simple Remctl client
     * 
     * @param hostname
     *            The host to connect to
     * @param port
     *            The port to connect on
     * @param serverPrincipal
     *            The server principal. If null, defaults to 'host/hostname'
     */
    public SimpleRemctlClient(String hostname, int port,
            String serverPrincipal) {
        this.hostname = hostname;
        this.port = port;
        this.serverPrincipal = serverPrincipal;

    }

    /* (non-Javadoc)
     * @see org.eyrie.remctl.client.RemctlClient#execute(java.lang.String)
     */
    @Override
    public RemctlResponse execute(String... arguments) {
        RemctlResponse response = this.executeAllowAnyStatus(arguments);
        if (response.getStatus() == null
                        || response.getStatus() != 0) {
            throw new RemctlStatusException(response);
        }
        return response;

    }

    /* (non-Javadoc)
     * @see org.eyrie.remctl.client.RemctlClient#executeAllowAnyStatus(java.lang.String)
     */
    @Override
    public RemctlResponse executeAllowAnyStatus(String... arguments) {

        //FIXME: we should use our connection factory (which we can get from a 
        // a setter or a constructor). That would allow us to use a mock connection
        // for testing the functionality.
        RemctlConnection remctlConnection = new RemctlConnection(this.hostname,
                this.port,
                this.serverPrincipal);

        remctlConnection.connect();

        RemctlCommandToken command = new RemctlCommandToken(
                                    false,
                                    arguments);

        remctlConnection.writeToken(command);

        //remctl server should auto-close connection since keep alive is false
        return RemctlResponse
                .buildFromTokens(remctlConnection.readAllTokens());
    }

}
