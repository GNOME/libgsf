#!/usr/bin/perl -w
# -----------------------------------------------------------------------------

use strict;
use lib ($0 =~ m|^(.*/)| ? $1 : ".");
use LibGsfTest;
use FileHandle;

my $aaaa = 'aaaa.txt';
{
    &LibGsfTest::junkfile ($aaaa);
    my $f = new FileHandle ($aaaa, 'w') or
	die "Failed to create $aaaa: $!\n";
    print $f "a"x10000000;
}

&test_zip ('files' => [$aaaa],
	   'zip-member-tests' => ['!zip64']);
