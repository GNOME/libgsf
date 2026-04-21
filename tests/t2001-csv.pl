#!/usr/bin/perl -w
# -----------------------------------------------------------------------------

use strict;
use lib ($0 =~ m|^(.*/)| ? $1 : ".");
use LibGsfTest;

print STDERR "Testing CSV output\n";
my $test_csv = "$top_builddir/tests/test-csv";
my $code = system ($test_csv);
&LibGsfTest::system_failure ('test-csv', $code) if $code;

print STDERR "Pass\n";
