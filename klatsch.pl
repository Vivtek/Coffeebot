#!/usr/bin/perl

use strict;
use warnings;
my $peer = $ENV{'REMOTE_HOST'};

use POE;
use POE::Wheel::ReadWrite;
use Symbol qw(gensym);
use Device::SerialPort;

# Open free port for binary data channel

# Create session for console (talking to the client)
POE::Session->create (
    inline_states => {
        _start        => \&console_start,
        console_input => \&console_handler,
        _stop         => \&console_stop,
    }
);

# Create session on serial port for the arm controller
POE::Session->create(
  inline_states => {
    _start      => \&setup_arm,
    incoming_arm => \&incoming_arm,
    arm_error   => \&handle_arm_error,
  },
);

# Create session on serial port for the motor ganglion
POE::Session->create(
  inline_states => {
    _start      => \&setup_wheels,
    incoming_wheels => \&incoming_wheels,
    wheels_error   => \&handle_wheels_error,
  },
);

# Create session on serial port for the visual cortex
POE::Session->create(
  inline_states => {
    _start      => \&setup_eye,
    incoming_eye => \&incoming_eye,
    eye_error   => \&handle_eye_error,
  },
);


$poe_kernel->run();
exit 0;

# ---------------------------------------------------------------------------------------
# Things we can talk to
# ---------------------------------------------------------------------------------------
my $console = undef;
my $wheels = undef;
my $arm = undef;
my $eye = undef;

sub shut_down {
    $poe_kernel->stop();
}


# ---------------------------------------------------------------------------------------
# Motor ganglion handler
# ---------------------------------------------------------------------------------------
sub setup_wheels {
  my ($kernel, $heap) = @_[KERNEL, HEAP];

  # Open a serial port, and tie it to a file handle for POE.
  my $handle = gensym();
  my $port = tie(*$handle, "Device::SerialPort", "/dev/wheels");
  die "can't open port: $!" unless $port;
  $port->datatype('raw');
  $port->baudrate(9600);
  $port->databits(8);
  $port->parity("none");
  $port->stopbits(1);
  $port->handshake("rts");
  $port->write_settings();

  $heap->{wheels} = $port;
  $heap->{wheels_wheel} = POE::Wheel::ReadWrite->new(
    Handle => $handle,
    Filter => POE::Filter::Line->new(
      InputLiteral  => "\x0D",    # Received line endings.
      OutputLiteral => "\x0D",        # Sent line endings.
    ),
    InputEvent => "incoming_wheels",
    ErrorEvent => "wheels_error",
  );
  $wheels = $heap->{wheels_wheel};
}

sub incoming_wheels {
  my ($heap, $data) = @_[HEAP, ARG0];
  say(400, $data);
}

sub handle_wheels_errors {
  my $heap = $_[HEAP];
  $console->put("$_[ARG0] error $_[ARG1]: $_[ARG2]");
  $console->put("bye!");
  shut_down();
}

# ---------------------------------------------------------------------------------------
# Arm handler
# ---------------------------------------------------------------------------------------
sub setup_arm {
  my ($kernel, $heap) = @_[KERNEL, HEAP];

  # Open a serial port, and tie it to a file handle for POE.
  my $handle = gensym();
  my $port = tie(*$handle, "Device::SerialPort", "/dev/armncam");
  die "can't open port: $!" unless $port;
  $port->datatype('raw');
  $port->baudrate(9600);
  $port->databits(8);
  $port->parity("none");
  $port->stopbits(1);
  $port->handshake("rts");
  $port->write_settings();

  $heap->{arm} = $port;
  $heap->{arm_wheel} = POE::Wheel::ReadWrite->new(
    Handle => $handle,
    Filter => POE::Filter::Line->new(
      InputLiteral  => "\x0D",    # Received line endings.
      OutputLiteral => "\x0D",        # Sent line endings.
    ),
    InputEvent => "incoming_arm",
    ErrorEvent => "arm_error",
  );
  $arm = $heap->{arm_wheel};
}

sub incoming_arm {
  my ($heap, $data) = @_[HEAP, ARG0];
  say(410, $data);
}

sub handle_arm_errors {
  my $heap = $_[HEAP];
  $console->put("$_[ARG0] error $_[ARG1]: $_[ARG2]");
  $console->put("bye!");
  shut_down();
}


# ---------------------------------------------------------------------------------------
# Visual cortex handler
# ---------------------------------------------------------------------------------------
sub setup_eye {
  my ($kernel, $heap) = @_[KERNEL, HEAP];

  # Open a serial port, and tie it to a file handle for POE.
  my $handle = gensym();
  my $port = tie(*$handle, "Device::SerialPort", "/dev/eye");
  die "can't open port: $!" unless $port;
  $port->datatype('raw');
  $port->baudrate(115200);
  $port->databits(8);
  $port->parity("none");
  $port->stopbits(1);
  $port->handshake("rts");
  $port->write_settings();

  $heap->{eye} = $port;
  $heap->{eye_wheel} = POE::Wheel::ReadWrite->new(
    Handle => $handle,
    Filter => POE::Filter::Line->new(
      InputLiteral  => "\x0D",    # Received line endings.
      OutputLiteral => "\x0D",        # Sent line endings.
    ),
    InputEvent => "incoming_eye",
    ErrorEvent => "eye_error",
  );
  $eye = $heap->{eye_wheel};
}

sub incoming_eye {
  my ($heap, $data) = @_[HEAP, ARG0];
  say(420, $data);
}

sub handle_eye_errors {
  my $heap = $_[HEAP];
  $console->put("$_[ARG0] error $_[ARG1]: $_[ARG2]");
  $console->put("bye!");
  shut_down();
}


# ---------------------------------------------------------------------------------------
# Console handling
# ---------------------------------------------------------------------------------------
sub say {
    return unless $console;
    my ($num, $data, $text) = @_;
    if (defined $text) {
        $console->put ("$num [$data] $text");
    } else {
        $console->put ("$num $data");
    }
    $console->flush();
}

sub console_start {
    my ($heap) = $_[HEAP];
    $heap->{console} = POE::Wheel::ReadWrite->new(
        InputHandle  => \*STDIN,
        OutputHandle => \*STDOUT,
        InputEvent   => 'console_input',
        ErrorEvent   => 'console_error',
    );
    $console = $heap->{console};
    say (100, "Welcome to Coffeebot - service is my only joy");
    say (101, "Running as " . getpwuid( $< ));
}

sub console_stop {
    delete $_[HEAP]->{console};
}

sub console_handler {
    my ($heap, $kernel, $input, $exception) = @_[HEAP, KERNEL, ARG0, ARG1];
    if (defined $input) {
        my @pieces = split / /, $input;
        my $command = '';
        $command = shift @pieces if @pieces;
        if ($command eq 'quit' || $command eq 'bye' || $command eq 'exit') {
            command_quit ();
        } elsif ($command eq 'x') {
            $wheels->put('x');
            $arm->put('x');
        } elsif ($command eq 'f' || $command eq 'b') {
        	$wheels->put($command);
        } elsif ($command eq '1' || $command eq '2' || $command eq '3' || $command eq '4') {
        	$arm->put($command);
        } elsif ($command eq 'q' || $command eq 'w' || $command eq 'e' || $command eq 'r') {
        	$arm->put($command);
        } elsif ($command eq 'something') {
            # command_something($heap, @pieces);
        } elsif ($command) {
            say (500, "Unknown command '$command'");
        } else {
        	say (100, "Joy");
        }
    }
}

sub console_error {
    my ($kernel, $heap) = @_[KERNEL, HEAP];
    $kernel->yield('_stop');
}

sub command_quit {
    say (999, "Only joy");
    shut_down();
}
