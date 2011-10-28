package org.eyrie.remctl.core;

import java.io.PrintWriter;
import java.io.StringWriter;

/**
 * Utility functions used by Remctl Client.
 * 
 * <p>
 * Similar functionality can be found in various third party libraries. For the
 * sake of reducing our dependency size, we implement our own version.
 * 
 * @author pradtke
 * 
 */
public class Utils {

	/**
	 * Convert a throwable and stack trace into a String.
	 * 
	 * <p>
	 * Useful for cases when we need to log an exception at a non-error level
	 * </p>
	 * 
	 * @param throwable
	 *            the throwable to convert
	 * @return The thowable stack trace as a string.
	 */
	static public String throwableToString(Throwable throwable) {
		StringWriter stringWriter = new StringWriter();
		PrintWriter printWriter = new PrintWriter(stringWriter);
		throwable.printStackTrace(printWriter);
		return stringWriter.toString();
	}

}
