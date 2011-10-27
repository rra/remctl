package org.eyrie.remctl.client;


/**
 * Exception thrown when a remctl command returns a non-0 status code. This
 * indicates an error in the command itself, and not with remctl.
 * 
 * <p>
 * If you don't consider a non-0 status codes to be an exceptional condition,
 * then you can use the {@link RemctlClient#executeAllowAnyStatus(String...)}
 * call to return a RemctlResponse with non-0 status codes
 * <p>
 * 
 * @author pradtke
 * 
 */
public class RemctlStatusException extends RuntimeException {

    /**
     * 
     */
    private static final long serialVersionUID = 1L;

    /**
     * The server response
     */
    private final RemctlResponse response;

    /**
     * Constructor for RemctlStatusException
     * 
     * @param response
     *            the server response that caused the exception
     */
    public RemctlStatusException(RemctlResponse response) {
        super("Unexpected status. Expected 0, recieved '"
                + response.getStatus() + "'");
        this.response = response;
    }

    /**
     * @return the server response on stdout
     * @see org.eyrie.remctl.client.RemctlResponse#getStdOut()
     */
    public String getStdOut() {
        return this.response.getStdOut();
    }

    /**
     * @return the server response on stderr
     * @see org.eyrie.remctl.client.RemctlResponse#getStdErr()
     */
    public String getStdErr() {
        return this.response.getStdErr();
    }

    /**
     * @return the status code
     * @see org.eyrie.remctl.client.RemctlResponse#getStatus()
     */
    public Integer getStatus() {
        return this.response.getStatus();
    }

}
