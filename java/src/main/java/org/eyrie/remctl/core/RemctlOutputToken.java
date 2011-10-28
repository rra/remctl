package org.eyrie.remctl.core;

import java.io.ByteArrayInputStream;
import java.io.DataInputStream;
import java.io.DataOutputStream;
import java.io.IOException;
import java.io.UnsupportedEncodingException;

/**
 * Represents a remctl output token. This is sent from the server to the client
 * and represents some of the output from a command. Zero or more output tokens
 * may be sent for a command, followed by a status token that indicates the end
 * of output from the command.
 */
public class RemctlOutputToken extends RemctlMessageToken {
	/** The stream number from which this output came. */
	private final byte stream;

	/** The output from the command. */
	private final byte output[];

	/**
	 * Construct an output token with the given output data.
	 * 
	 * @param stream
	 *            The output stream, either 1 for standard output or 2 for
	 *            standard error
	 * @param output
	 *            The output data
	 * @throws RemctlException
	 *             If the stream is not 1 or 2
	 */
	public RemctlOutputToken(int stream, byte output[]) throws RemctlException {
		super(2);
		if (stream != 1 && stream != 2) {
			throw new RemctlException("invalid stream" + stream);
		}
		this.stream = (byte) stream;
		this.output = output;
	}

	/**
	 * Determine the length of this output message token.
	 * 
	 * @return The length of the wire representation of the data payload of this
	 *         output token
	 */
	@Override
	int length() {
		return 1 + 4 + this.output.length;
	}

	/**
	 * Write the wire representation of this output token to the provided output
	 * stream.
	 * 
	 * @param stream
	 *            Output stream to which to write the encoded token
	 * @throws IOException
	 *             On errors writing to the output stream
	 */
	@Override
	void writeData(DataOutputStream stream) throws IOException {
		stream.writeByte(this.stream);
		stream.writeInt(this.output.length);
		stream.write(this.output);
	}

	/**
	 * Create a Output token from command specific bytes
	 * 
	 * @param data
	 *            command specific bytes
	 * @throws RemctlErrorException
	 *             If bytes make an invalid token
	 */
	public RemctlOutputToken(byte[] data) throws RemctlErrorException {
		super(2);
		if (data.length < 5)
			throw new RemctlErrorException(RemctlErrorCode.ERROR_BAD_TOKEN,
					"Minimum size 5, byte length " + data.length);
		DataInputStream inputStream = new DataInputStream(
				new ByteArrayInputStream(data));
		try {
			this.stream = inputStream.readByte();
			if (this.stream != 1 && this.stream != 2) {
				throw new RemctlException("invalid stream " + this.stream);
			}
			int length = inputStream.readInt();
			if (data.length - 5 != length) {
				throw new RemctlErrorException(RemctlErrorCode.ERROR_BAD_TOKEN,
						"Expected length mismatch: Command length is " + length
								+ ", but data length is " + (data.length - 5));
			}
			this.output = new byte[length];
			inputStream.readFully(this.output, 0, length);
		} catch (IOException e) {
			throw new IllegalStateException(e);
		}
	}

	/**
	 * @return the stream
	 */
	public byte getStream() {
		return this.stream;
	}

	/**
	 * @return the output
	 */
	byte[] getOutput() {
		return this.output;
	}

	/**
	 * Get the byte[] of output as a UTF-8 string
	 * 
	 * @return String representation of the output
	 */
	public String getOutputAsString() {
		try {
			return new String(this.output, "UTF-8");
		} catch (UnsupportedEncodingException e) {
			throw new RemctlException(
					"Unable to convert bytes to string on stream "
							+ this.stream, e);
		}
	}

	/*
	 * (non-Javadoc)
	 * 
	 * @see java.lang.Object#toString()
	 */
	@Override
	public String toString() {
		return "RemctlOutputToken [stream=" + this.stream + ", output size ="
				+ this.output.length + "]";
	}

	@Override
	RemctlMessageCode getType() {
		return RemctlMessageCode.MESSAGE_OUTPUT;
	}

}