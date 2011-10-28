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
	 * @throws RuntimeException
	 *             if error token is returned or this is an error contacting
	 *             server.
	 */
	public RemctlResponse execute(String... arguments);

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
	public RemctlResponse executeAllowAnyStatus(String... arguments);

}