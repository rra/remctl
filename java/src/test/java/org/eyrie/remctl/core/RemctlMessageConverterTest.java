package org.eyrie.remctl.core;

import static org.mockito.Matchers.any;
import static org.mockito.Matchers.anyInt;
import static org.mockito.Mockito.mock;
import static org.mockito.Mockito.when;

import org.eyrie.remctl.RemctlException;
import org.ietf.jgss.GSSContext;
import org.ietf.jgss.GSSException;
import org.ietf.jgss.MessageProp;
import org.junit.Before;
import org.junit.Test;
import org.mockito.invocation.InvocationOnMock;
import org.mockito.stubbing.Answer;

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

    RemctlMessageConverter messageConverter;
    GSSContext mockContext;

    /**
     * Return the first argument
     * 
     * @author pradtke
     * 
     */
    class ReturnInput implements Answer<byte[]> {

        @Override
        public byte[] answer(InvocationOnMock invocation) throws Throwable {
            return (byte[]) invocation.getArguments()[0];
        }

    }

    @Before
    public void setup() throws GSSException {
        this.mockContext = mock(GSSContext.class);
        this.messageConverter = new RemctlMessageConverter(this.mockContext);
        when(this.mockContext.wrap(any(byte[].class), anyInt(), anyInt(),
                any(MessageProp.class))).thenAnswer(new ReturnInput());
    }

    @Test
    public void testWrite() throws RemctlException {

        //        RemctlCommandToken commandToken = new RemctlCommandToken(true, "test",
        //                "command");
        //
        //        ByteArrayOutputStream byteArrayOutputStream = new ByteArrayOutputStream();
        //
        //
        //        this.messageConverter.encodeMessage(byteArrayOutputStream, commandToken);
        //        
        //        
        //        byte[] expectedBytes = { 1, /* flags */
        //                0,0,0,1, /*length */
        //                /* payload */
        //                2, /* protocol version */
        //                3, /* message type */
        //                
        //        }

    }
}
