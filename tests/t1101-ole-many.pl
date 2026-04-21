#!/usr/bin/perl -w
# -----------------------------------------------------------------------------

use strict;
use lib ($0 =~ m|^(.*/)| ? $1 : ".");
use LibGsfTest;

my $archive = "test-many.ole";
&LibGsfTest::junkfile ($archive);

print STDERR "Testing ole many streams (50000)\n";
my $test_ole = "$top_builddir/tests/test-ole";
my $cmd = "$test_ole $archive 50000";
my $code = system ($cmd);
&LibGsfTest::system_failure ('test-ole', $code) if $code;

print STDERR "Pass\n";
