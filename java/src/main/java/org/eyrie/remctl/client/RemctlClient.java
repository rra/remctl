package org.eyrie.remctl.client;

/**
 * A client for a remctl server.
 * 
 * <p>
 * Implementations must be thread safe, and handle connection resources
 * (opening, closing, etc)
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
	 *            The arguments to run. Generally the first argument is the
	 *            command, and the rest are arguments for that command.
	 * @return The response from the server
	 * @throws RemctlException
	 *             if error token is returned or this is an error contacting
	 *             server.
	 * @throws RemctlStatusException if the return status is not 0.
	 * @throws RemctlErrorException if error token encountered during processing.
	 */
	RemctlResponse execute(String... arguments);

	/**
	 * Runs the list of arguments against the remctl server.
	 * 
	 * @param arguments
	 *            The arguments to run. Generally the first argument is the
	 *            command, and the rest are arguments for that command.
	 * @return The response from the server
	 * @throws RemctlException
	 *             if error token is returned or this is an error contacting
	 *             server.
	 * @throws RemctlErrorException if error token encountered during processing.
	 */
	RemctlResponse executeAllowAnyStatus(String... arguments);

}