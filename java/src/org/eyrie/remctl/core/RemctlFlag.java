package org.eyrie.remctl.core;

/**
 * Holds the flags that may make up the flag octet of a remctl token. These
 * flags should be combined with xor. Only TOKEN_CONTEXT, TOKEN_CONTEXT_NEXT,
 * TOKEN_DATA, and TOKEN_PROTOCOL are used for version 2 packets. The other
 * flags are used only with the legacy version 1 protocol.
 */
public enum RemctlFlag {
    /** Sent by client in the first packet at the start of the session. */
    TOKEN_NOOP(0x01),

    /** Used for all tokens during initial context setup. */
    TOKEN_CONTEXT(0x02),

    /** Protocol v1: regular data packet from server or client. */
    TOKEN_DATA(0x04),

    /** Protocol v1: MIC token from server. */
    TOKEN_MIC(0x08),

    /** Sent by client in the first packet at the start of the session. */
    TOKEN_CONTEXT_NEXT(0x10),

    /** Protocol v1: client requests server send a MIC in reply. */
    TOKEN_SEND_MIC(0x20),

    /** Protocol v2: set on all protocol v2 tokens. */
    TOKEN_PROTOCOL(0x40);

    /** The wire representation of this flag. */
    byte value;

    /**
     * Create #RemctlFlag with a particular value.
     * 
     * @param value
     *            Byte value of flag.
     */
    private RemctlFlag(int value) {
        this.value = (byte) value;
    }

    /**
     * @return the value
     */
    public byte getValue() {
        return this.value;
    }

}