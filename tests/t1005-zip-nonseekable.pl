#!/usr/bin/perl -w
# -----------------------------------------------------------------------------

use strict;
use lib ($0 =~ m|^(.*/)| ? $1 : ".");
use LibGsfTest;

my $NEWS = 'NEWS';
&LibGsfTest::junkfile ($NEWS);
system ("cp", "$topsrc/NEWS", $NEWS);

&test_zip ('files' => ['Makefile', $NEWS],
	   'stdio' => 1,
	   'zip-member-tests' => ['zip64']);

&test_zip ('files' => ['Makefile', $NEWS],
	   'create-options' => ["--zip64=0"],
	   'stdio' => 1,
	   'zip-member-tests' => ['!zip64']);

&test_zip ('files' => ['Makefile', $NEWS],
	   'create-options' => ["--zip64=1"],
	   'stdio' => 1,
	   'zip-member-tests' => ['zip64']);
