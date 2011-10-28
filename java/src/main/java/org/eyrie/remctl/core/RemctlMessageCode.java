package org.eyrie.remctl.core;

import java.util.Collections;
import java.util.HashMap;
import java.util.Map;

/**
 * Represents the types of messages that are supported by the remctl protocol. Each message sent by a server or client
 * after the initial GSS-API authentication will be one of these types.
 */
public enum RemctlMessageCode {
    /** Command to run (client to server). */
    MESSAGE_COMMAND(1),

    /** Client wants to terminate the session (client to server). */
    MESSAGE_QUIT(2),

    /** Output from command (server to client). */
    MESSAGE_OUTPUT(3),

    /**
     * Exit status of command, marking the end of the command (server to client).
     */
    MESSAGE_STATUS(4),

    /** Error in previous command (server to client). */
    MESSAGE_ERROR(5),

    /** Highest supported protocol version (server to client). */
    MESSAGE_VERSION(6),

    /** A NO-OP message for generating traffic on idle connections. (Message 7) */
    MESSAGE_NOOP(7);

    /** The wire representation of this message code. */
    byte value;

    /**
     * Create #RemctlMessageCode with a specific value.
     * 
     * @param value
     *            Byte value of message code.
     */
    private RemctlMessageCode(final int value) {
        this.value = (byte) value;
    }

    /** Map of wire byte values to #RemctlMessageCode objects. */
    private static final Map<Byte, RemctlMessageCode> codes;
    static {
        Map<Byte, RemctlMessageCode> m = new HashMap<Byte, RemctlMessageCode>();
        for (RemctlMessageCode c : RemctlMessageCode.class.getEnumConstants()) {
            m.put(c.value, c);
        }
        codes = Collections.unmodifiableMap(m);
    }

    /**
     * Given the wire representation of a message code, return the corresponding enum constant.
     * 
     * @param code
     *            Protocol constant corresponding to a message type.
     * @return The enum constant for that message type or null if that constant does not correspond to a message type.
     */
    public static RemctlMessageCode getCode(final int code) {
        if (code < 1 || code > 127)
            return null;
        return codes.get((byte) code);
    }
}
