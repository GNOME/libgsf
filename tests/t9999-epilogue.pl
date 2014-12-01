#!/usr/bin/perl -w
# -----------------------------------------------------------------------------

use strict;
use lib ($0 =~ m|^(.*/)| ? $1 : ".");
use LibGsfTest;

&message ("Warnings about things that might affect tests.");

my $HOME = $ENV{'HOME'};

# ----------------------------------------

print STDERR "Warning: you have a ~/.valgrindrc file that might affect tests.\n"
    if defined ($HOME) && -r "$HOME/.valgrindrc";

# ----------------------------------------

print STDERR "Pass\n";
