package org.eyrie.remctl.client;

import static org.junit.Assert.assertEquals;

import org.junit.Test;

/**
 * Test case for the factory.
 * 
 * @author pradtke
 * 
 */
public class RemctlClientFactoryTest {

    /**
     * Test specifying the default port.
     */
    @Test
    public void testSettingPort() {
        RemctlClientFactory factory = new RemctlClientFactory();
        RemctlClient client = factory.createClient("bob.example.com");
        assertEquals("Default port expected", 4373, client.getPort());

        // change default port
        factory.setPort(1234);
        client = factory.createClient("dude.dude.ca");
        assertEquals("Updated Default port expected", 1234, client.getPort());

        // check that specify port doesn't get overridden
        client = factory.createClient("dude.dude.ca", 78, null);
        assertEquals("Specified port expected", 78, client.getPort());
    }
}
