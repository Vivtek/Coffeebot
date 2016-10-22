#!/usr/bin/perl

use strict;
use warnings;
my $peer = $ENV{'REMOTE_HOST'};

use POE;
use POE::Wheel::ReadWrite;

# Open free port for binary data channel

# Create session for console (talking to the client)
POE::Session->create (
    inline_states => {
        _start        => \&console_start,
        console_input => \&console_handler,
        _stop         => \&console_stop,
    }
);

# Create session on serial port for the visual cortex

# Create session on serial port for the motor ganglion


$poe_kernel->run();
exit 0;

sub shut_down {
    $poe_kernel->stop();
}



# ---------------------------------------------------------------------------------------
# Console handling
# ---------------------------------------------------------------------------------------
my $console = undef;
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
}

sub console_stop {
    delete $_[HEAP]->{console};
}

sub console_handler {
    my ($heap, $kernel, $input, $exception) = @_[HEAP, KERNEL, ARG0, ARG1];
    if (defined $input) {
        my @pieces = split / /, $input;
        my $command = shift @pieces;
        if ($command eq 'quit' || $command eq 'bye' || $command eq 'exit') {
            command_quit ();
        } elsif ($command eq 'something') {
            # command_something($heap, @pieces);
        } else {
            say (500, "Unknown command '$command'");
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
