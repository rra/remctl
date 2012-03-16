package org.eyrie.remctl.client;

import org.eyrie.remctl.core.RemctlErrorException;
import org.eyrie.remctl.core.RemctlException;
import org.eyrie.remctl.core.Utils;

/**
 * A client for a remctl server.
 * 
 * <p>
 * Implementations must be thread safe, and handle connection resources (opening, closing, etc)
 * </p>
 * 
 * @author pradtke
 * 
 */
public interface RemctlClient {

    /**
     * Runs the list of arguments against the remctl server.
     * 
     * @param arguments
     *            The arguments to run. Generally the first argument is the command, and the rest are arguments for that
     *            command.
     * @return The response from the server
     * @throws RemctlException
     *             if error token is returned or this is an error contacting server.
     * @throws RemctlStatusException
     *             if the return status is not 0.
     * @throws RemctlErrorException
     *             if error token encountered during processing.
     */
    RemctlResponse execute(String... arguments);

    /**
     * Runs the list of arguments against the remctl server.
     * 
     * @param arguments
     *            The arguments to run. Generally the first argument is the command, and the rest are arguments for that
     *            command.
     * @return The response from the server
     * @throws RemctlException
     *             if error token is returned or this is an error contacting server.
     * @throws RemctlErrorException
     *             if error token encountered during processing.
     */
    RemctlResponse executeAllowAnyStatus(String... arguments);

    /**
     * Return the port that the connection is on, or would be on when execute is called.
     * 
     * @return the port to connect on, 0 if not specified ( the default ({@link Utils#DEFAULT_PORT}) port would be used)
     *         or -1 if it can't be determined.
     */
    int getPort();

}
