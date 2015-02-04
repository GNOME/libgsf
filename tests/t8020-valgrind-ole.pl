#!/usr/bin/perl -w
# -----------------------------------------------------------------------------

use strict;
use lib ($0 =~ m|^(.*/)| ? $1 : ".");
use LibGsfTest;

my $archive = "test.ole";
&LibGsfTest::junkfile ($archive);

my $NEWS = 'NEWS';
&LibGsfTest::junkfile ($NEWS);
system ("cp", "$topsrc/NEWS", $NEWS);

my @files = ('Makefile', $NEWS);

print STDERR "Testing ole create\n";
&test_valgrind ("$gsf createole $archive @files", 1);
print STDERR "\n";

print STDERR "Testing ole list\n";
&test_valgrind ("$gsf list $archive >/dev/null", 1);
print STDERR "\n";

print STDERR "Testing ole listprops\n";
&test_valgrind ("$gsf listprops $archive >/dev/null", 1);
print STDERR "\n";

print STDERR "Testing ole cat\n";
&test_valgrind ("$gsf cat $archive @files >/dev/null", 1);
print STDERR "\n";
