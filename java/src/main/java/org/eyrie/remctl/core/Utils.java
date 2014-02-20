package org.eyrie.remctl.core;

import java.io.PrintWriter;
import java.io.StringWriter;

/**
 * Utility functions used by Remctl Client.
 * 
 * <p>
 * Similar functionality can be found in various third party libraries. For the sake of reducing our dependency size, we
 * implement our own version.
 * 
 * @author pradtke
 * 
 */
public final class Utils {

    /**
     * Utility class doesn't need to be instantiated.
     */
    private Utils() {

    }

    /**
     * Milliseconds in a minute.
     */
    public static final long MILLS_IN_MINUTE = 60 * 1000;

    /**
     * The default remctl port 4373.
     */
    public static final int DEFAULT_PORT = 4373;

    /**
     * The default name to use for the login module in a JAAS config file.
     */
    public static final String LOGIN_MODULE_NAME = "RemctlClient";

    /**
     * Convert a throwable and stack trace into a String.
     * 
     * <p>
     * Useful for cases when we need to log an exception at a non-error level
     * </p>
     * 
     * @param throwable
     *            the throwable to convert
     * @return The throwable stack trace as a string.
     */
    public static String throwableToString(final Throwable throwable) {
        StringWriter stringWriter = new StringWriter();
        PrintWriter printWriter = new PrintWriter(stringWriter);
        throwable.printStackTrace(printWriter);
        return stringWriter.toString();
    }

}
