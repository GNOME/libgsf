#!/usr/bin/perl -w
# -----------------------------------------------------------------------------

use strict;
use lib ($0 =~ m|^(.*/)| ? $1 : ".");
use LibGsfTest;

&test_zip ('files' => ['Makefile.am', 'common.supp'],
	   'stdio' => 1,
	   'zip-member-tests' => ['zip64']);

&test_zip ('files' => ['Makefile.am', 'common.supp'],
	   'create-options' => ["--zip64=0"],
	   'stdio' => 1,
	   'zip-member-tests' => ['!zip64']);

&test_zip ('files' => ['Makefile.am', 'common.supp'],
	   'create-options' => ["--zip64=1"],
	   'stdio' => 1,
	   'zip-member-tests' => ['zip64']);
