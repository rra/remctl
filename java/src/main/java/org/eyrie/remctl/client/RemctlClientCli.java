package org.eyrie.remctl.client;

import java.util.ArrayList;
import java.util.List;

import org.eyrie.remctl.core.Utils;
import org.slf4j.Logger;
import org.slf4j.LoggerFactory;

/**
 * Allows invoking the Java remctl client from the command line.
 * <p>
 * During development this can be run with
 * 
 * <pre>
 * mvn exec:java -Djava.security.auth.login.config=gss_jaas.conf \
 *  -Dexec.mainClass=org.eyrie.remctl.client.RemctlClientCli -Dexec.args="[args]"
 * </pre>
 * 
 * </p>
 * <p>
 * Java and Keberos sometimes don't get along together. Here are some things to try:
 * 
 * <ul>
 * <li>Specify krb5.conf info explicitly with
 * 
 * <pre>
 * -Djava.security.krb5.realm=stanford.edu -Djava.security.krb5.kdc=krb5auth1.stanford.edu:88
 * </pre>
 * 
 * </li>
 * </ul>
 * 
 * @author pradtke
 * 
 */
public class RemctlClientCli {

    /**
     * Allow logging.
     */
    static final Logger logger = LoggerFactory.getLogger(RemctlConnection.class);

    /**
     * Default factory for remctl clients.
     */
    private RemctlClientFactory clientFactory = new RemctlClientFactory();

    /**
     * Main method.
     * 
     * @param args
     *            the arguments to pass to remctl
     */
    public static void main(String[] args) {

        int status = new RemctlClientCli().run(args);
        System.exit(status);
    }

    /**
     * Run the actual instance and return the status code from the remctl command.
     * 
     * @param args
     *            The options and arguments to use
     * @return The status from the remctl command, or 1 if there was an issue.
     */
    public int run(final String... args) {

        int port = Utils.DEFAULT_PORT;
        String serverPrincipal = null;

        List<String> commandArgs = new ArrayList<String>();

        boolean lookForOptions = true;
        for (int i = 0; i < args.length; i++) {
            String arg = args[i];
            if (lookForOptions && arg.startsWith("-")) {
                if (arg.equals("-p")) {
                    port = Integer.parseInt(args[++i]);
                } else if (arg.equals("-s")) {
                    serverPrincipal = args[++i];
                } else {
                    this.printHelp("Unrecognized option " + arg);
                    return 1;
                }
            } else {
                lookForOptions = false;
                commandArgs.add(arg);
            }
        }

        if (commandArgs.isEmpty()) {
            this.printHelp("No command found");
            return 1;
        }

        logger.info("Running {}", commandArgs);

        String hostname = commandArgs.remove(0);

        // FIXME: catch exception and print error code
        RemctlResponse response = this.clientFactory.createClient(hostname, port, serverPrincipal)
                .executeAllowAnyStatus(commandArgs.toArray(new String[0]));

        System.err.print(response.getStdErr());
        System.out.print(response.getStdOut());

        return response.getStatus();
    }

    /**
     * Helper for printing the help message.
     * 
     * @param message
     *            Custome first line.
     */
    private void printHelp(final String message) {
        System.out.println(message);
        // full usage: remctl [-dhv] [-b source-ip] [-p port] [-s service] host
        System.out.println("usage: remctl [-p port] [-s service] host"
                + " command [subcommand [parameters ...]]");
        System.out.println("The Java command line tool is geared towards testing the Remctl library. "
                + "This tool attempts to follow the C version: "
                + "http://www.eyrie.org/~eagle/software/remctl/remctl.html");
    }

}
