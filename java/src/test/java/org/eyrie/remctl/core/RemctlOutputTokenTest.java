package org.eyrie.remctl.core;

import static org.junit.Assert.assertEquals;
import junit.framework.Assert;

import org.junit.Test;

/**
 * Test conversion to/from streams and back to RemctlOutputTokenTest tokens
 * 
 * @author pradtke
 * 
 */
public class RemctlOutputTokenTest {

	/**
	 * Test building the token from command specific bytes
	 */
	@Test
	public void testBytes() {
		// -- test no message --
		byte[] message = new byte[] { 1, /* stdout */
		0, 0, 0, 0 /* no length */
		};

		RemctlOutputToken outputToken = new RemctlOutputToken(message);

		assertEquals("Stream should match", 1, outputToken.getStream());
		assertEquals("Output should have no length", 0,
				outputToken.getOutput().length);

		// -- test with message --
		message = new byte[] { 2, /* stderr */
		0, 0, 0, 3, /* length of 3 */
		66, 111, 111 };

		outputToken = new RemctlOutputToken(message);

		assertEquals("Stream should match", 2, outputToken.getStream());
		assertEquals("Output should have expected length", 3,
				outputToken.getOutput().length);

		assertEquals("String output should match", "Boo",
				outputToken.getOutputAsString());
	}

	/**
	 * Test handling of malformed message: invalid lengths, not enough miminum
	 * bytes, etc
	 */
	@Test
	public void testBadMessageLength() {
		byte[] message = {};

		try {
			new RemctlOutputToken(message);
			Assert.fail("Exception expected");
		} catch (RemctlErrorException e) {
			assertEquals(2, e.getErrorCode());
		}

		message = new byte[] { 1 };

		try {
			new RemctlOutputToken(message);
			Assert.fail("Exception expected");
		} catch (RemctlErrorException e) {
			assertEquals(2, e.getErrorCode());
		}

		message = new byte[] { 2, 1 };

		try {
			new RemctlOutputToken(message);
			Assert.fail("Exception expected");
		} catch (RemctlErrorException e) {
			assertEquals(2, e.getErrorCode());
		}

		message = new byte[] { 1, /* stdout */
		0, 0, 0, 0, /* no length */
		1 /* extra byte */
		};

		try {
			new RemctlOutputToken(message);
			Assert.fail("Exception expected");
		} catch (RemctlErrorException e) {
			assertEquals(2, e.getErrorCode());
		}

		message = new byte[] { 1, /* stdout */
		0, 0, 0, 1 /* no length */
		/* missing byte */
		};

		try {
			new RemctlOutputToken(message);
			Assert.fail("Exception expected");
		} catch (RemctlErrorException e) {
			assertEquals(2, e.getErrorCode());
		}

		// -- invalid stream --
		message = new byte[] { 8, /* bad stream */
		0, 0, 0, 0 /* no length */
		};

		try {
			new RemctlOutputToken(message);
			Assert.fail("Exception expected");
		} catch (RemctlException e) {
			assertEquals("invalid stream 8", e.getMessage());
		}

	}

}
