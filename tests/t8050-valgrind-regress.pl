#!/usr/bin/perl -w
# -----------------------------------------------------------------------------
# Valgrind-based regression tests for libgsf defects that can only be observed
# by exercising the code under a memory checker (leaks, over-reads/
# over-writes, use-after-free, fd leaks, and the null-dereference on closing
# a zip with an unwritten member).
#
# The tested programs deliberately exercise the broken code paths, so this
# test FAILS until the corresponding bugs are fixed.

use strict;
use lib ($0 =~ m|^(.*/)| ? $1 : ".");
use LibGsfTest;

$ENV{'G_DEBUG'} = 'gc-friendly:resident-modules';
$ENV{'G_SLICE'} = 'always-malloc';
delete $ENV{'VALGRIND_OPTS'};

my $suppfile = "$topsrc/tests/common.supp";
&LibGsfTest::report_skip ("file $suppfile does not exist") unless -r $suppfile;

my @tests = (
    # out-of-bounds write in GsfOutputMemory::vprintf after seek past EOF
    'test-regress-outmem-printf',
    # GsfXMLOut holds its sink without a reference (UAF)
    'test-regress-xmlout-uaf',
    # infile lookup leaks / borrowed reference
    'test-regress-infile-leak',
    # FILE* leak in gsf_input_stdio_new_FILE non-regular path
    'test-regress-stdio',
    # null dereference when closing a zip with an unwritten member
    # (expected to crash until fixed -- hence the tolerant runner below)
    'test-regress-outzip-open',
    # gzip output: failed setup must not re-run deflateInit2 (leak)
    'test-regress-gzip-setup',
    # tar header with several bad numeric fields: first GError leaked
    'test-regress-tar-badhdr',
    # CSV quoting decodes fields past their end / invalid UTF-8
    'test-regress-csv',
    # ODF metadata reader: name pending when parsing aborts
    'test-regress-meta',
);

my $nfail = 0;
foreach my $t (@tests) {
    my $outfile = &LibGsfTest::invent_junkfile ('valgrind.log');
    # Run through libtool so the build-tree library is used instead of the
    # system one.
    my $cmd = "../libtool --mode=execute valgrind --gen-suppressions=all"
	. " --suppressions=$suppfile"
	. " --show-below-main=yes --leak-check=full --num-callers=20"
	. " --track-fds=yes --log-file=$outfile ./$t";

    print STDERR "Running $t under valgrind\n";
    system ($cmd);

    my $txt = &LibGsfTest::read_file ($outfile);
    &LibGsfTest::removejunk ($outfile);
    my $errors = ($txt =~ /ERROR\s+SUMMARY:\s*(\d+)\s+errors?/i) ? $1 : -1;
    if ($errors == 0) {
	print STDERR "Pass\n";
    } else {
	# Print the interesting part of the valgrind report for the log.
	$txt =~ s/\{\n.*?\n\}\n//gs;	# drop generated suppression blocks
	my @lines = split /\n/, $txt;
	print STDERR "  $_\n" foreach @lines[0 .. (@lines > 40 ? 39 : $#lines)];
	print STDERR "Fail ($errors error(s))\n";
	$nfail++;
    }
}

if ($nfail) {
    print STDERR "$nfail valgrind regression test(s) failed\n";
    exit 1;
}
print STDERR "All valgrind regression tests passed\n";
exit 0;
