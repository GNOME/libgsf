#!/usr/bin/perl -w
# -----------------------------------------------------------------------------

use strict;
use lib ($0 =~ m|^(.*/)| ? $1 : ".");
use LibGsfTest;
use FileHandle;

my $noise = 'noise.bin';
&LibGsfTest::junkfile ($noise);
system ("dd", "if=/dev/urandom", "of=$noise", "bs=1048576", "count=10");
&test_zip ('files' => [$noise],
	   'zip-member-tests' => ['!zip64']);
