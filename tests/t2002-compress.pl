#!/usr/bin/perl -w
# -----------------------------------------------------------------------------

use strict;
use lib ($0 =~ m|^(.*/)| ? $1 : ".");
use LibGsfTest;

print STDERR "Testing GZip and BZip2 compression/decompression\n";
my $test_compress = "$top_builddir/tests/test-compress";
my $code = system ($test_compress);
&LibGsfTest::system_failure ('test-compress', $code) if $code;

print STDERR "Pass\n";
