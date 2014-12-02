#!/usr/bin/perl -w
# -----------------------------------------------------------------------------

use strict;
use lib ($0 =~ m|^(.*/)| ? $1 : ".");
use LibGsfTest;

my $archive = "test.zip";
&LibGsfTest::junkfile ($archive);

my @files = ('Makefile.am', 'common.supp');

print STDERR "Testing zip write\n";
&test_valgrind ("$gsf createzip $archive @files", 1);
print STDERR "\n";

print STDERR "Testing zip read\n";
&test_valgrind ("$gsf list $archive >/dev/null", 1);
print STDERR "\n";
