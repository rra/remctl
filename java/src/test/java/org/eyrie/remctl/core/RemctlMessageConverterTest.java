package org.eyrie.remctl.core;

import static org.junit.Assert.assertEquals;
import static org.mockito.Matchers.any;
import static org.mockito.Matchers.anyInt;
import static org.mockito.Mockito.mock;
import static org.mockito.Mockito.when;

import java.io.IOException;
import java.io.InputStream;

import junit.framework.Assert;

import org.ietf.jgss.GSSContext;
import org.ietf.jgss.GSSException;
import org.ietf.jgss.MessageProp;
import org.junit.Before;
import org.junit.Test;
import org.mockito.invocation.InvocationOnMock;
import org.mockito.stubbing.Answer;
import org.slf4j.Logger;
import org.slf4j.LoggerFactory;

/**
 * Test converting the remctl packets to/from tokens.
 * 
 * The packets are encrypted using GSS context. In testing we use a mock GSS
 * context (that just passes through the byte stream) which lets us test remctl
 * packet to message conversion without requiring GSS
 * 
 * @author pradtke
 * 
 */
public class RemctlMessageConverterTest {

	/**
	 * Allow logging
	 */
	static final Logger logger = LoggerFactory
			.getLogger(RemctlMessageConverterTest.class);

	/**
	 * Class under test
	 */
	RemctlMessageConverter messageConverter;

	/**
	 * A mocked context
	 */
	GSSContext mockContext;

	/**
	 * Return the first argument
	 * 
	 * @author pradtke
	 * 
	 */
	static class ReturnInput implements Answer<byte[]> {

		@Override
		public byte[] answer(InvocationOnMock invocation) throws Throwable {
			return (byte[]) invocation.getArguments()[0];
		}

	}

	@Before
	public void setup() throws GSSException {
		this.mockContext = mock(GSSContext.class);
		this.messageConverter = new RemctlMessageConverter(this.mockContext);
		when(
				this.mockContext.wrap(any(byte[].class), anyInt(), anyInt(),
						any(MessageProp.class))).thenAnswer(new ReturnInput());
	}

	/**
	 * Test handling of IO exceptions
	 * 
	 * @throws Exception
	 *             if exception
	 */
	@Test
	public void testExceptionTranslation() throws Exception {

		InputStream input = mock(InputStream.class);
		when(input.read()).thenThrow(new IOException("connection died"));
		try {
			this.messageConverter.decodeMessage(input);
			Assert.fail("Expected exception");
		} catch (RemctlException e) {
			logger.debug("Caught {}", e);
			assertEquals("connection died", e.getCause().getMessage());
		}

	}

	@Test
	public void testWrite() throws RemctlException {

		// RemctlCommandToken commandToken = new RemctlCommandToken(true,
		// "test",
		// "command");
		//
		// ByteArrayOutputStream byteArrayOutputStream = new
		// ByteArrayOutputStream();
		//
		//
		// this.messageConverter.encodeMessage(byteArrayOutputStream,
		// commandToken);
		//
		//
		// byte[] expectedBytes = { 1, /* flags */
		// 0,0,0,1, /*length */
		// /* payload */
		// 2, /* protocol version */
		// 3, /* message type */
		//
		// }

	}
}
