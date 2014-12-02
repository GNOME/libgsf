#!/usr/bin/perl -w
# -----------------------------------------------------------------------------

use strict;
use lib ($0 =~ m|^(.*/)| ? $1 : ".");
use LibGsfTest;

my $archive = "test.tar";
&LibGsfTest::junkfile ($archive);

my @files = ('Makefile.am', 'common.supp');

if (0) {
    print STDERR "Testing tar write\n";
    &test_valgrind ("$gsf createole $archive @files", 1);
    print STDERR "\n";
} else {
    # We don't have the tar write side, so use plain tar
    system ("tar", "cf", $archive, @files);
}

print STDERR "Testing tar read\n";
&test_valgrind ("$gsf list $archive >/dev/null", 1);
print STDERR "\n";
