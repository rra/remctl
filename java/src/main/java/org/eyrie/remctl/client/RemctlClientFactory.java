package org.eyrie.remctl.client;

import org.eyrie.remctl.core.Utils;

/**
 * Creates an instance of a RemctlClient.
 * 
 * By using a {@link RemctlClientFactory} to create connections rather then <code>new SimpleRemctlClient</code> client
 * code can inject custom implementations to (primarily) return mocked clients for testing purposes.
 * 
 * @author pradtke
 * 
 */
public class RemctlClientFactory {

    /**
     * What port to use for created clients.
     */
    private int defaultPort = Utils.DEFAULT_PORT;

    /**
     * Creates a client that connects to the given hostname on the default port.
     * 
     * @param hostname
     *            the hostname to connect to.
     * @return A RemctlClient
     */
    public RemctlClient createClient(final String hostname) {
        return this.createClient(hostname, this.defaultPort, null);
    }

    /**
     * Creates a client that connects to the given hostname on the given port.
     * 
     * @param hostname
     *            the hostname to connect to.
     * @param port
     *            the port to connect on.
     * @param servicePrincipal
     *            The server principal to use. If null then use the <code>host/hostname</code> principal
     * @return A RemctlClient
     */
    public RemctlClient createClient(final String hostname, final int port, final String servicePrincipal) {
        return new SimpleRemctlClient(hostname, port, null);
    }

    /**
     * The port used for all created {@link RemctlClient}s.
     * 
     * @return the port
     */
    public int getPort() {
        return this.defaultPort;
    }

    /**
     * Set the port used for all future created clients.
     * 
     * @param port
     *            the port to use.
     */
    public void setPort(final int port) {
        this.defaultPort = port;
    }

}
