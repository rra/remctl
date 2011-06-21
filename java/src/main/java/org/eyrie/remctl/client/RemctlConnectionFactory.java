package org.eyrie.remctl.client;

import org.apache.commons.pool.BasePoolableObjectFactory;
import org.eyrie.remctl.core.RemctlVersionToken;

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

    /* (non-Javadoc)
     * @see org.apache.commons.pool.BasePoolableObjectFactory#validateObject(java.lang.Object)
     */
    @Override
    public boolean validateObject(Object obj) {
        RemctlConnection connection = (RemctlConnection) obj;
        //FIXME: Can we really send a version token to the server?
        RemctlVersionToken versionToken = new RemctlVersionToken(2);
        connection.writeToken(versionToken);
        //FIXME: what is the expected response?
        connection.readAllTokens();
        return true;

    }

}
