#!/usr/bin/perl

use strict;
use warnings;
use POSIX 'strftime';
use Socket qw(unpack_sockaddr_in);
my $peer = $ENV{'REMOTE_HOST'};

use POE;
use POE::Wheel::ReadWrite;
use POE::Wheel::SocketFactory;
use Symbol qw(gensym);
use Device::SerialPort;
my $existing = `ps -ef | grep klatsch | grep -v grep`;
if ($existing) {
   my ($pi, $proc, $junk) = split / +/, $existing;
   if ($proc != $$) {
      print "my process: $$\n";
      print "Killing hung process $proc\r\n";
      system ("kill $proc");
      sleep (0.5);
   }
}

# Create session for console (talking to the client)
POE::Session->create (
    inline_states => {
        _start        => \&console_start,
        console_input => \&console_handler,
        _stop         => \&console_stop,
        check_oob     => \&console_check_oob,
        next_snapshot => \&next_snapshot,
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

# Create socket factory for OOB channel
POE::Session->create(
  inline_states => {
    _start => \&setup_sockets,
    on_client_accept => \&oob_client_accept,
    on_server_error  => \&oob_server_error,
    on_client_input  => \&oob_client_input,
    on_client_error  => \&oob_client_error,
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
my $oob_server = undef;
my $oob = undef;
my $oob_connected = undef;

my $eye_state = 0;
my $pic = '';

my $video_frequency = 0;

my $mvm = {}; # Movement parameter model = controller model



sub shut_down {
    $poe_kernel->stop();
}

# ---------------------------------------------------------------------------------------
# Logging
# ---------------------------------------------------------------------------------------
my $log = undef;

sub to_log {
   return unless $log;
   return unless fileno($log);
   my $time = POSIX::strftime('%I:%M:%S ', localtime);
   print $log $time, shift, "\n";
}

# ---------------------------------------------------------------------------------------
# Hardware state variables
# ---------------------------------------------------------------------------------------
my %state_variables;

sub set_state {
   my ($name, $val) = @_;
   return unless defined $val;
   $state_variables{$name} = $val;
   to_log ("$name -> $val");
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
      InputLiteral  => "\x0D\x0A",    # Received line endings.
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

sub handle_wheels_error {
  my $heap = $_[HEAP];
  $console->put("$_[ARG0] error $_[ARG1]: $_[ARG2]");
  $console->put("motor error - crashing");
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
      InputLiteral  => "\x0D\x0A",    # Received line endings.
      OutputLiteral => "\x0D",        # Sent line endings.
    ),
    InputEvent => "incoming_arm",
    ErrorEvent => "arm_error",
  );
  $arm = $heap->{arm_wheel};
}

sub incoming_arm {
  my ($heap, $data) = @_[HEAP, ARG0];
  if ($data =~ /^=/) {
     $data =~ s/^.*=//;
     my ($name, $val) = split / /, $data;
     set_state($name, $val);
  }
  else {
     say(410, $data);
  }
}

sub handle_arm_error {
  my $heap = $_[HEAP];
  $console->put("$_[ARG0] error $_[ARG1]: $_[ARG2]");
  $console->put("arm error - crashing");
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
      InputLiteral  => "\x0D\x0A",    # Received line endings.
      OutputLiteral => "\x0D",        # Sent line endings.
    ),
    InputEvent => "incoming_eye",
    ErrorEvent => "eye_error",
  );
  $eye = $heap->{eye_wheel};
}

sub incoming_eye {
  my ($heap, $data) = @_[HEAP, ARG0];
  $eye_state = 0 unless defined $eye_state;
  if ($eye_state == 0 and $data =~ /^img /) {
     $eye_state = 1;
  } elsif ($eye_state == 1) {
     if ($data =~ /done$/m) {
        $data =~ s/done$//m;
        $pic .= $data;
        $pic =~ s/^\00//;
        $eye_state = 0;
        if ($oob_connected) {
           $oob->put('+' . $pic . '+++done');
           $pic = '';
        } else {
           my $date = POSIX::strftime('%Y-%m-%d-%I%M%S', localtime);
           open (my $fh, ">", "/home/pi/log/$date.jpg");
           binmode $fh;
           print $fh $pic;
           $pic = '';
           to_log ("Image $date.jpg");
        }
     } else {
        $pic .= $data;
        $pic .= "\x0D\x0A";
     }
  } else {
     say(420, $data) unless $data eq 'picture coming';
  }
}

sub handle_eye_error {
  my $heap = $_[HEAP];
  $console->put("$_[ARG0] error $_[ARG1]: $_[ARG2]");
  $console->put("eye error - crashing");
  shut_down();
}

sub next_snapshot {
  my ($heap, $kernel) = @_[HEAP, KERNEL];
  $eye->put('o');
  $kernel->delay(next_snapshot => $video_frequency) if $video_frequency;
}

# ---------------------------------------------------------------------------------------
# OOB data handling
# Largely taken from http://search.cpan.org/dist/POE/lib/POE/Wheel/SocketFactory.pm
# ---------------------------------------------------------------------------------------
sub setup_sockets {
  $_[HEAP]{oob_server} = POE::Wheel::SocketFactory->new(
     SuccessEvent => 'on_client_accept',
     FailureEvent => 'on_server_error',
  );
  $oob_server = $_[HEAP]{oob_server};
}
sub oob_client_accept {
  my $client_socket = $_[ARG0];
  my $io_wheel = POE::Wheel::ReadWrite->new(
    Handle => $client_socket,
    InputEvent => "on_client_input",
    ErrorEvent => "on_client_error",
  );
  $_[HEAP]{client}->{$io_wheel->ID() } = $io_wheel;
  $oob = $io_wheel;
  $oob_connected = 1;
}
sub oob_server_error {
    my ($kernel, $heap) = @_[KERNEL, HEAP];
    $kernel->yield('_stop');
}
sub oob_client_input {
   # The client sends movement parameters on the OOB channel.
   my ($heap, $data) = @_[HEAP, ARG0];
   if ($data =~ /^(.*)=(.*)$/) {
      set_mvm_parm($1, $2);
   } else {
      say (110, $data . "?");
   }
}
sub oob_client_error {
     # Handle client error, including disconnect.
     my $wheel_id = $_[ARG3];
     delete $_[HEAP]{client}{$wheel_id};
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
    my ($heap, $kernel) = @_[HEAP, KERNEL];
    $heap->{console} = POE::Wheel::ReadWrite->new(
        InputHandle  => \*STDIN,
        OutputHandle => \*STDOUT,
        InputEvent   => 'console_input',
        ErrorEvent   => 'console_error',
    );
    $console = $heap->{console};
    say (100, "Welcome to Coffeebot - service is my only joy");
    my $date = POSIX::strftime('%Y-%m-%d-%I%M%S', localtime);
    say (101, "Logging at time $date");
    open($log, '>', "/home/pi/log/$date.txt") or say (500, "Couldn't open log: $!");
    $kernel->yield('check_oob');
}
sub console_check_oob {
    my $kernel = $_[KERNEL];
    say (104, "Checking OOB ready");
    if (not defined $oob_server) {
       $kernel->delay(check_oob => 1);
       return;
    }
    my ($port, $addr) = unpack_sockaddr_in($oob_server->getsockname);
    if ($port) {
       say (105, $port, "Listening for OOB connection");
    } else {
       $kernel->delay(check_oob => 1);
    }
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
        } elsif ($command eq 'f' || $command eq 'b' || $command eq 'p' || $command eq 'l') {
        	$wheels->put($command);
        } elsif ($command eq 'k') {
            $wheels->put('r'); 
        } elsif ($command eq '1' || $command eq '2' || $command eq '3' || $command eq '4') {
        	$arm->put($command);
        } elsif ($command eq 'q' || $command eq 'w' || $command eq 'e' || $command eq 'r') {
        	$arm->put($command);
        } elsif ($command eq 'o') {
            $eye->put($command);
        } elsif ($command eq 'vid') {
            $video_frequency = $pieces[0] || 0.5;
            say (110, "vid frequency $video_frequency");
            $kernel->delay(next_snapshot => $video_frequency) if $video_frequency;
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

# ------------------------------------------------------------------------------
# Movement parameter model
# ------------------------------------------------------------------------------
sub set_mvm_parm {
   my ($key, $val) = @_;
   $mvm->{$key} = $val;
   if ($key eq 'BS' or $key eq 'FS' or $key eq 'PS') {
      $wheels->put("$key=$val");
   } else {
      say (115, "$key -> $val");
   }
}

