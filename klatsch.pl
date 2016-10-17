#!/usr/bin/perl

$|=1;
$peer = $ENV{'REMOTE_HOST'};
respond (100, "Welcome to Coffeebot. Service is my only joy.");

sub respond {
    my ($num, $data, $text) = @_;
    if (defined $text) {
        print "$num [$data] $text\r\n";
    } else {
        print "$num $data\r\n";
    }
}

while (<>) {
    s/[\n\r]+$//;
    my @pieces = split;
    my $command = shift @pieces;
    last if $command eq 'quit';
    last if $command eq 'bye';
    last if $command eq 'exit';

    respond (500, "Unknown command $command");
}
