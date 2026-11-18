#!/usr/bin/perl -w
# -----------------------------------------------------------------------------

use strict;
use lib ($0 =~ m|^(.*/)| ? $1 : ".");
use LibGsfTest;

my $NEWS = &LibGsfTest::invent_junkfile ('NEWS.txt');
system ("cp", "$topsrc/NEWS", $NEWS);

&test_zip ('files' => ['Makefile', $NEWS],
	   'zip-member-tests' => ['!zip64']);
