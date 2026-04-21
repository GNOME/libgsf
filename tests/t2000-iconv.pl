#!/usr/bin/perl -w
# -----------------------------------------------------------------------------

use strict;
use lib ($0 =~ m|^(.*/)| ? $1 : ".");
use LibGsfTest;

print STDERR "Testing iconv output\n";
my $test_iconv = "$top_builddir/tests/test-iconv";
my $code = system ($test_iconv);
&LibGsfTest::system_failure ('test-iconv', $code) if $code;

print STDERR "Pass\n";
