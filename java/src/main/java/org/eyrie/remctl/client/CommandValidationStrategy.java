package org.eyrie.remctl.client;

import java.util.List;

import org.eyrie.remctl.core.RemctlCommandToken;
import org.eyrie.remctl.core.RemctlToken;

/**
 * A validation strategy that sends a command to the remclt server and checks
 * the response.
 * 
 * @author pradtke
 * 
 */
public class CommandValidationStrategy extends BaseValidationStrategy {

	/**
	 * default commands is just noop
	 */
	String[] commands = { "noop" };

	/**
	 * Default constructor
	 */
	public CommandValidationStrategy() {

	}

	/**
	 * Create a CommandValidationStrategy that runs the specified commands
	 * 
	 * @param commands
	 *            The commands to run
	 */
	public CommandValidationStrategy(String... commands) {
		this.setCommands(commands);
	}

	@Override
	public boolean checkConnection(RemctlConnection connection) {

		RemctlCommandToken token = new RemctlCommandToken(true, this.commands);
		connection.writeToken(token);
		List<RemctlToken> tokens = connection.readAllTokens();
		RemctlResponse response = RemctlResponse.buildFromTokens(tokens);
		return this.checkResponse(response);
	}

	/**
	 * Check the response for running the configured command.
	 * 
	 * <p>
	 * By default simply checks the status of the response. Subclasses may
	 * override as needed to do perform more complicated checks
	 * </p>
	 * 
	 * @param response
	 *            The response from running the configured command
	 * @return true if the response is expected, false otherwise
	 */
	boolean checkResponse(RemctlResponse response) {
		return response.getStatus() != null && response.getStatus() == 0;
	}

	/**
	 * @return the commands
	 */
	public String[] getCommands() {
		return this.commands;
	}

	/**
	 * @param commands
	 *            the commands to set
	 */
	public void setCommands(String... commands) {
		this.commands = commands;
	}

}
