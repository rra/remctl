package org.eyrie.remctl.client;

import org.slf4j.Logger;
import org.slf4j.LoggerFactory;

/**
 * A base implementation of validation strategy. It checks max life and for
 * unread tokens.
 * 
 * @author pradtke
 * 
 */
public class BaseValidationStrategy implements
        RemctlConnectionValidationStrategy {

    /**
     * default 55 minutes
     */
    long maxLife = 55 * 60 * 10000;

    /**
     * Allow logging
     */
    static final Logger logger = LoggerFactory
            .getLogger(RemctlConnectionFactory.class);

    @Override
    public boolean isValid(RemctlConnection connection) {
        /**
         * Remctl server closes connection after an hour.
         */
        long now = System.currentTimeMillis();
        long elapsedTime = now
                - connection.getConnectionEstablishedTime().getTime();
        if (elapsedTime > this.maxLife) {
            logger.debug("Connection open {} minutes. Marking invalid",
                    this.maxLife);
            return false;
        }

        /**
         * If connection has any unread, stale tokens then mark it invalid.
         */
        if (connection.hasPendingData()) {
            return false;
        }

        try {
            return this.checkConnection(connection);
        } catch (Exception e) {
            //TODO: re-evaluate what level this should be logged.
            logger.info("Error validting connection {}. Marking it invalid", e);
            return false;
        }
    }

    /**
     * An extension point for subclasses that want to add additional validation
     * on top of what is performed by isValid.
     * 
     * <p>
     * Base implementation returns true.
     * </p>
     * 
     * @param connection
     *            the connection to check
     * @return true if additional checks are successful
     * @throws Exception
     *             any exceptions are treated as invalid connection
     */
    boolean checkConnection(RemctlConnection connection) throws Exception {
        return true;
    }

    /**
     * @return the maxLife
     */
    public long getMaxLife() {
        return this.maxLife;
    }

    /**
     * @param maxLife
     *            the maxLife to set
     */
    public void setMaxLife(long maxLife) {
        this.maxLife = maxLife;
    }
}
