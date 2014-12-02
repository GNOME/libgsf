#!/usr/bin/perl -w
# -----------------------------------------------------------------------------

use strict;
use lib ($0 =~ m|^(.*/)| ? $1 : ".");
use LibGsfTest;

my $archive = "test.zip";
&LibGsfTest::junkfile ($archive);

my @files = ('Makefile.am', 'common.supp');

print STDERR "Testing zip create\n";
&test_valgrind ("$gsf createzip $archive @files", 1);
print STDERR "\n";

print STDERR "Testing zip list\n";
&test_valgrind ("$gsf list $archive >/dev/null", 1);
print STDERR "\n";

print STDERR "Testing zip listprops\n";
&test_valgrind ("$gsf listprops $archive >/dev/null", 1);
print STDERR "\n";

print STDERR "Testing zip cat\n";
&test_valgrind ("$gsf cat $archive @files >/dev/null", 1);
print STDERR "\n";
