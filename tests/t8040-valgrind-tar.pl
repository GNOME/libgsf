#!/usr/bin/perl -w
# -----------------------------------------------------------------------------

use strict;
use lib ($0 =~ m|^(.*/)| ? $1 : ".");
use LibGsfTest;

my $archive = "test.tar";
&LibGsfTest::junkfile ($archive);

my $NEWS = 'NEWS';
&LibGsfTest::junkfile ($NEWS);
system ("cp", "$topsrc/NEWS", $NEWS);

my @files = ('Makefile', $NEWS);

if (0) {
    print STDERR "Testing tar write\n";
    &test_valgrind ("$gsf createole $archive @files", 1);
    print STDERR "\n";
} else {
    # We don't have the tar write side, so use plain tar
    system ("tar", "cf", $archive, @files);
}

print STDERR "Testing tar list\n";
&test_valgrind ("$gsf list $archive >/dev/null", 1);
print STDERR "\n";

print STDERR "Testing tar listprops\n";
&test_valgrind ("$gsf listprops $archive >/dev/null", 1);
print STDERR "\n";

print STDERR "Testing tar cat\n";
&test_valgrind ("$gsf cat $archive @files >/dev/null", 1);
print STDERR "\n";
