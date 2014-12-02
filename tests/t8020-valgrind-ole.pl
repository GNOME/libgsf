#!/usr/bin/perl -w
# -----------------------------------------------------------------------------

use strict;
use lib ($0 =~ m|^(.*/)| ? $1 : ".");
use LibGsfTest;

my $archive = "test.ole";
&LibGsfTest::junkfile ($archive);

my @files = ('Makefile.am', 'common.supp');

print STDERR "Testing ole write\n";
&test_valgrind ("$gsf createole $archive @files", 1);
print STDERR "\n";

print STDERR "Testing ole read\n";
&test_valgrind ("$gsf list $archive >/dev/null", 1);
print STDERR "\n";
