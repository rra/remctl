package org.eyrie.remctl;

import org.eyrie.remctl.client.RemctlClient;
import org.eyrie.remctl.core.RemctlCommandToken;
import org.eyrie.remctl.core.RemctlErrorToken;
import org.eyrie.remctl.core.RemctlOutputToken;
import org.eyrie.remctl.core.RemctlStatusToken;
import org.eyrie.remctl.core.RemctlToken;
import org.ietf.jgss.GSSContext;

/**
 * A simplified interface for a remctl client.
 * 
 * @author pradtke
 * 
 */
public class SimpleRemctlClient {

    StringBuilder stdOutResponse = new StringBuilder();
    StringBuilder stdErrResponse = new StringBuilder();

    GSSContext context;

    Integer returnStatus;

    RemctlErrorToken errorToken;

    RemctlClient remctlClient;

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
     * @param clientPrincipal
     *            The client principal
     */
    public SimpleRemctlClient(String hostname, int port,
            String serverPrincipal) {
        this.remctlClient = new RemctlClient(hostname, port, serverPrincipal);
    }

    /**
     * Runs the list of arguments against the remctl server.
     * 
     * @param arguments
     *            The arguments to run. Generally the first argument is the
     *            command, and the rest are arguments for that command.
     */
    public void execute(String... arguments) {

        this.remctlClient.connect();

        RemctlCommandToken command = null;
        try {
            command = new RemctlCommandToken(
                                    false,
                                    arguments);
        } catch (RemctlException e) {
            // TODO Auto-generated catch block
            e.printStackTrace();
        }

        this.remctlClient.writeToken(command);
        for (RemctlToken outputToken : this.remctlClient.readAllTokens()) {

            System.out.println("token  " + outputToken);
            if (outputToken instanceof RemctlErrorToken) {
                this.errorToken = (RemctlErrorToken) outputToken;
                break;
            } else if (outputToken instanceof RemctlOutputToken) {
                RemctlOutputToken remctlOutputToken = (RemctlOutputToken) outputToken;
                if (remctlOutputToken.getStream() == 1) {
                    this.stdOutResponse.append(remctlOutputToken
                                            .getOutputAsString());
                } else {
                    this.stdErrResponse.append(remctlOutputToken
                                            .getOutputAsString());
                }
            } else if (outputToken instanceof RemctlStatusToken) {
                System.out.println("Status done");
                this.returnStatus = ((RemctlStatusToken) outputToken)
                                        .getStatus();
                break;
            }
        }
    }

    /**
     * @return the stdOutResponse
     */
    String getStdOutResponse() {
        return this.stdOutResponse.toString();
    }

    /**
     * @return the stdErrResponse
     */
    String getStdErrResponse() {
        return this.stdErrResponse.toString();
    }

    /**
     * @return the returnStatus
     */
    Integer getReturnStatus() {
        return this.returnStatus;
    }

}
