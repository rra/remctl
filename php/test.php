# PHP remctl examples

# load the remctl module
<? dl('remctl.so');

if ( !extension_loaded( 'remctl' )) {
    echo "Failed to load remctl module.\n";
    exit( 2 );
}

# simple remctl interface
$args = array( 'type', 'service', 'php-remctl', 'notify' );
$result = remctl( 'server.remctl.edu', 0, 'remctl/server', $args );
if ( $result->stdout_len ) {
    echo "stdout: $result->stdout";
}
if ( $result->stderr_len ) {
    echo "stderr: $result->stderr";
}
if ( $result->error ) {
    echo "remctl failed: $result->error\n";
    exit( 2 );
}
echo "status: $result->status\n";


# advanced remctl interface, for more direct control over the session.
if (( $rem = remctl_new()) == null ) {
    echo "remctl_new failed\n";
    exit( 2 );
}
if ( !remctl_open( $rem, 'server.remctl.edu', 0, 'remctl/server' )) {
    echo "remctl_open failed: " . remctl_error( $rem ) . "\n";
    exit( 2 );
}
$args = array( 'type', 'service', 'php-remctl', 'notify' );
if ( !remctl_command( $rem, $args )) {
    echo "remctl_command failed: " . remctl_error( $rem ) . "\n";
    exit( 2 );
}
$output = remctl_output( $rem );
while ($output != null && $output->type != "done") {
    switch ( $output->type ) {
    case "output":
	if ($output->stream == 1) {
	    echo "stdout: $output->data";
	} elseif ($output->stream == 2) {
	    echo "stderr: $output->data";
	}
	break;

    case "error":
	echo "error: $output->error ($output->data)\n";
	break;

    case "status":
	echo "status: $output->status\n";
	break;

    default:
	echo "unknown output type \"$output->type\"";
    }

    $output = remctl_output( $rem );
}
if ( $output == null ) {
    echo "remctl_output failed: " . remctl_error( $rem ) . "\n";
}
remctl_close( $rem );
?>
