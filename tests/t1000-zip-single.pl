#!/usr/bin/perl -w
# -----------------------------------------------------------------------------

use strict;
use lib ($0 =~ m|^(.*/)| ? $1 : ".");
use LibGsfTest;

&test_zip ('files' => ["Makefile"],
	   'zip-member-tests' => ['!zip64']);
