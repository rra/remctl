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
public class SimpleRemctlClient {

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

    /**
     * Runs the list of arguments against the remctl server.
     * 
     * @param arguments
     *            The arguments to run. Generally the first argument is the
     *            command, and the rest are arguments for that command.
     * @return The response from the server
     * @throws RuntimeException
     *             if error token is returned or this is an error contacting
     *             server.
     */
    public RemctlResponse execute(String... arguments) {
        RemctlResponse response = this.executeAllowAnyStatus(arguments);
        if (response.getStatus() == null
                        || response.getStatus() != 0) {
            throw new RemctlStatusException(response);
        }
        return response;

    }

    /**
     * Runs the list of arguments against the remctl server.
     * 
     * @param arguments
     *            The arguments to run. Generally the first argument is the
     *            command, and the rest are arguments for that command.
     * @return The response from the server
     * @throws RuntimeException
     *             if error token is returned or this is an error contacting
     *             server.
     */
    public RemctlResponse executeAllowAnyStatus(String... arguments) {
        //        RemctlResponse response = this.execute(arguments);
        //        if (response.getStatus() == null
        //                || response.getStatus() != expectedStatus) {
        //            throw new RuntimeException("Unexpected status. Expected "
        //                    + expectedStatus + ", recieved " + response.getStatus());
        //        }

        RemctlClient remctlClient = new RemctlClient(this.hostname, this.port,
                this.serverPrincipal);

        remctlClient.connect();

        RemctlCommandToken command = new RemctlCommandToken(
                                    false,
                                    arguments);

        remctlClient.writeToken(command);

        //remctl server should auto-close connection since keep alive is false
        return RemctlResponse
                .buildFromTokens(remctlClient.readAllTokens());
    }

}
