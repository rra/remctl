package org.eyrie.remctl.core;

import java.io.DataOutputStream;

/**
 * Represents a remctl noop token. This token is sent by a client to a server as
 * a idle connection keep alive.
 * 
 * <p>
 * In version 3 of the prto
 */
public class RemctlNoopToken extends RemctlMessageToken {

	/**
	 * Construct a noop token.
	 * 
	 */
	public RemctlNoopToken() {
		super(3);
	}

	/**
	 * Determine the length of NOOP message token. There is no content.
	 * 
	 * @return 0. NOOP message have no additional data.
	 */
	@Override
	int length() {
		return 0;
	}

	/**
	 * Write the wire representation of this NOOP token to the provided output
	 * stream. This is a noop method, since the token has no data.
	 * 
	 * @param output
	 *            Output stream to which to write the encoded token
	 */
	@Override
	protected void writeData(DataOutputStream output) {
		return;
	}

	@Override
	RemctlMessageCode getType() {
		return RemctlMessageCode.MESSAGE_NOOP;
	}

}