package org.eyrie.remctl.core;

import java.io.DataOutputStream;
import java.io.IOException;

/**
 * Represents a remctl quit token. This is sent from the client to the server
 * when it is done issuing commands.
 */
public class RemctlQuitToken extends RemctlMessageToken {
	/**
	 * Construct a quit token.
	 * 
	 */
	public RemctlQuitToken() {
		super(2);
	}

	/**
	 * Construct a quit token from token data.
	 * 
	 * @param data
	 *            Token data (must be empty)
	 * @throws RemctlException
	 *             If the data is not empty
	 */
	public RemctlQuitToken(byte[] data) throws RemctlException {
		super(2);
		if (data.length != 0)
			throw new RemctlErrorException(RemctlErrorCode.ERROR_BAD_TOKEN);
	}

	/**
	 * Determine the length of this quit message token.
	 * 
	 * @return The length of the wire representation of the data payload of this
	 *         quit token
	 */
	@Override
	int length() {
		return 0;
	}

	/**
	 * Write the wire representation of this quit token to the provided output
	 * stream.
	 * 
	 * @param stream
	 *            Output stream to which to write the encoded token
	 * @throws IOException
	 *             On errors writing to the output stream
	 */
	@Override
	void writeData(DataOutputStream stream) throws IOException {
	}

	@Override
	RemctlMessageCode getType() {
		return RemctlMessageCode.MESSAGE_QUIT;
	}
}