package org.eyrie.remctl.client;

import org.eyrie.remctl.RemctlErrorCode;
import org.eyrie.remctl.RemctlErrorException;
import org.eyrie.remctl.RemctlProtocolException;
import org.eyrie.remctl.core.RemctlErrorToken;
import org.eyrie.remctl.core.RemctlOutputToken;
import org.eyrie.remctl.core.RemctlStatusToken;
import org.eyrie.remctl.core.RemctlToken;
import org.slf4j.Logger;
import org.slf4j.LoggerFactory;

/**
 * Encapsulates the response from a remctl server into an easy to use format.
 * 
 * @author pradtke
 * 
 */
public class RemctlResponse {

    /**
     * Allow logging
     */
    static final Logger logger = LoggerFactory.getLogger(RemctlResponse.class);

    /**
     * Output Messages on stream 1
     */
    private final String stdOut;
    /**
     * Output Messages on stream 2
     */
    private final String stdErr;

    /**
     * Exit status. Will be null if an error response happened.
     */
    private final Integer returnStatus;

    /**
     * Use factory method
     * 
     * @param stdOut
     *            stdOut string
     * @param stdErr
     *            stdErr string
     * @param returnStatus
     *            The return status (maybe null)
     */
    RemctlResponse(String stdOut, String stdErr, Integer returnStatus) {
        super();
        this.stdOut = stdOut;
        this.stdErr = stdErr;
        this.returnStatus = returnStatus;
    }

    /**
     * Build a response based on a set of remctl tokens
     * 
     * @param responseTokens
     *            The token received from the server
     * @return The remctlReponse
     * @throws RemctlProtocolException
     *             If the list has an unexpected protocol
     * @throws RemctlErrorException
     *             If a token is malformed, or an error token is seen
     */
    static public RemctlResponse buildFromTokens(
            Iterable<RemctlToken> responseTokens) {
        StringBuilder stdOutResponse = new StringBuilder();
        StringBuilder stdErrResponse = new StringBuilder();
        Integer status = null;

        for (RemctlToken outputToken : responseTokens) {

            logger.trace("Processing token {} " + outputToken.getClass());
            if (outputToken instanceof RemctlErrorToken) {
                logger.info("RemctlErrorToken in response {}", outputToken);
                throw new RemctlErrorException((RemctlErrorToken) outputToken);
            } else if (outputToken instanceof RemctlOutputToken) {
                RemctlOutputToken remctlOutputToken = (RemctlOutputToken) outputToken;
                byte stream = remctlOutputToken.getStream();
                if (stream == 1) {
                    stdOutResponse.append(remctlOutputToken
                                            .getOutputAsString());
                } else if (stream == 2) {
                    stdErrResponse.append(remctlOutputToken
                                            .getOutputAsString());
                } else {
                    throw new RemctlErrorException(
                            RemctlErrorCode.ERROR_BAD_TOKEN.value,
                            "Unrecognized stream " +
                                    stream);
                }
            } else if (outputToken instanceof RemctlStatusToken) {
                status = ((RemctlStatusToken) outputToken)
                                        .getStatus();
                break;
            } else {
                throw new RemctlProtocolException(
                            "Unrecognized response token " +
                                    outputToken.getClass());
            }
        }

        return new RemctlResponse(stdOutResponse.toString(),
                stdErrResponse.toString(), status);
    }

    /**
     * @return the stdOutResponse
     */
    public String getStdOut() {
        return this.stdOut;
    }

    /**
     * @return the stdErrResponse
     */
    public String getStdErr() {
        return this.stdErr;
    }

    /**
     * @return the returnStatus
     */
    public Integer getStatus() {
        return this.returnStatus;
    }

}
