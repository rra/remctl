package org.eyrie.remctl.client;

import org.eyrie.remctl.core.Utils;
import org.slf4j.Logger;
import org.slf4j.LoggerFactory;

/**
 * A base implementation of validation strategy. It checks max life and for unread tokens.
 * 
 * @author pradtke
 * 
 */
public class BaseValidationStrategy implements RemctlConnectionValidationStrategy {

    /**
     * default 55 minutes.
     */
    private static final long DEFAULT_MAX_LIFE = 55 * Utils.MILLS_IN_MINUTE;

    /**
     * default 55 minutes.
     */
    private long maxLife = DEFAULT_MAX_LIFE;

    /**
     * Allow logging.
     */
    static final Logger logger = LoggerFactory.getLogger(BaseValidationStrategy.class);

    @Override
    public final boolean isValid(final RemctlConnection connection) {
        /**
         * Remctl server closes connection after an hour.
         */
        long now = System.currentTimeMillis();
        long elapsedTime = now - connection.getConnectionEstablishedTime().getTime();
        if (elapsedTime > this.maxLife) {
            logger.debug("Connection open {}. Max life {}. Marking invalid", elapsedTime, this.maxLife);
            return false;
        }

        /**
         * If connection has any unread, stale tokens then mark it invalid.
         */
        if (connection.hasPendingData()) {
            logger.debug("Connection has stale pending data. Marking invalid");
            return false;
        }

        try {
            return this.checkConnection(connection);
        } catch (Exception e) {
            if (logger.isInfoEnabled()) {
                logger.info("Error validating connection. Marking it invalid.");
                logger.info("Stack trace {}", Utils.throwableToString(e));
            }
            return false;
        }
    }

    /**
     * An extension point for subclasses that want to add additional validation on top of what is performed by isValid.
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
    boolean checkConnection(final RemctlConnection connection) throws Exception {
        return true;
    }

    /**
     * The maximum connection lifetime in milliseconds.
     * 
     * @return the maxLife
     */
    public long getMaxLife() {
        return this.maxLife;
    }

    /**
     * Set the maximum time a connection should be open.
     * <p>
     * In milliseconds
     * </p>
     * 
     * @param maxLife
     *            the maxLife to set
     */
    public void setMaxLife(final long maxLife) {
        this.maxLife = maxLife;
    }
}
