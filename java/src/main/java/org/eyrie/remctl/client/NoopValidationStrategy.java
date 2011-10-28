package org.eyrie.remctl.client;

import java.util.List;

import org.eyrie.remctl.core.RemctlNoopToken;
import org.eyrie.remctl.core.RemctlToken;
import org.eyrie.remctl.core.RemctlVersionToken;

/**
 * Experimental validation strategy that sends protocol 3 NOOP messages.
 * 
 * @author pradtke
 * 
 */
public class NoopValidationStrategy extends BaseValidationStrategy {

    /**
     * Sends a RemctlNoopToken token and check the response.
     * 
     * @param connection
     *            The connection to check
     * @return true if {@link RemctlNoopToken} was returned from server.
     */
    @Override
    public boolean checkConnection(final RemctlConnection connection) {

        RemctlNoopToken noopToken = new RemctlNoopToken();
        connection.writeToken(noopToken);
        List<RemctlToken> tokens = connection.readAllTokens();
        if (tokens.size() != 1) {
            logger.warn("Unexpected number of tokens returned in valdate ({}). Marking invalid", tokens.size());
        }

        RemctlToken token = tokens.get(0);
        if (token instanceof RemctlNoopToken) {
            return true;
        } else if (token instanceof RemctlVersionToken) {
            // Server doesn't support protocol version 3. That is OK.
            return true;
        } else {
            logger.warn("Unexpected token returned from NOOP message: {}", token);
        }
        return false;

    }
}
