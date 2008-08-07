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
while ( $output != null && !$output->done ) {
    switch ( $output->type ) {
    case "output":
	if ( $output->stdout_len ) {
	    echo "stdout: $output->stdout";
	}
	if ( $output->stderr_len ) {
	    echo "stderr: $output->stderr";
	}
	break;

    case "error":
	echo "error: $output->error\n";
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
