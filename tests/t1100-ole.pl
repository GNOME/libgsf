#!/usr/bin/perl -w
# -----------------------------------------------------------------------------

use strict;
use lib ($0 =~ m|^(.*/)| ? $1 : ".");
use LibGsfTest;

my $archive = &LibGsfTest::invent_junkfile ('test.ole');

print STDERR "Testing ole read/write\n";
my $test_ole = "$top_builddir/tests/test-ole";
my $cmd = "$test_ole $archive";
my $code = system ($cmd);
&LibGsfTest::system_failure ('test-ole', $code) if $code;

print STDERR "Pass\n";
