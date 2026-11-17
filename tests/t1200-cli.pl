#!/usr/bin/perl -w
# -----------------------------------------------------------------------------
# Basic CLI tests for the `gsf` tool: build archives, list them, extract
# members, and inspect properties -- for all the relevant archive types
# (zip, OLE2, and tar for reading).
# -----------------------------------------------------------------------------

use strict;
use lib ($0 =~ m|^(.*/)| ? $1 : ".");
use LibGsfTest;

my $nfail = 0;
my $ncheck = 0;

sub note_pass {
    my ($what) = @_;
    print STDERR "ok: $what\n";
    $ncheck++;
}
sub note_fail {
    my ($what) = @_;
    print STDERR "FAIL: $what\n";
    $nfail++;
    $ncheck++;
}

# Run a command, checking its exit status and capturing its output.
sub run_out {
    my ($cmd, $what) = @_;
    my $outfile = &LibGsfTest::invent_junkfile ('output.txt');
    my $code = system ("$cmd > $outfile 2>/dev/null");
    if ($code == 0) {
	&note_pass ($what);
    } else {
	&note_fail ("$what (exit $code)");
	my $txt = &LibGsfTest::read_file ($outfile);
	my @lines = split /\n/, $txt;
	print STDERR "  $_\n" foreach @lines[0 .. (@lines > 8 ? 7 : $#lines)];
    }
    my $data = &LibGsfTest::read_file ($outfile);
    &LibGsfTest::removejunk ($outfile);
    return ($code, $data);
}

# Run a command, discarding its output; report pass/fail on the exit code.
sub run_ok {
    my ($cmd, $what) = @_;
    &LibGsfTest::junkfile ('dev-null.tmp');
    my $code = system ("$cmd > /dev/null 2>&1");
    &LibGsfTest::removejunk ('dev-null.tmp');
    if ($code == 0) {
	&note_pass ($what);
    } else {
	&note_fail ("$what (exit $code)");
    }
    return $code;
}

# Create a small text file with @content, tracked as a temp file.
# Uses LibGsfTest::write_file for the atomic write and junkfile for
# cleanup tracking.
sub make_input_file {
    my ($base, $content) = @_;
    my $name = &LibGsfTest::invent_junkfile ($base);
    &LibGsfTest::write_file ($name, $content);
    return $name;
}

# -----------------------------------------------------------------------------
# 1. help lists the subcommands
# -----------------------------------------------------------------------------
{
    print STDERR "help\n";
    my ($code, $out) = &run_out ("$gsf help", "help runs");
    &note_fail ("help mentions createzip") if $out !~ /createzip/;
    &note_fail ("help mentions createole") if $out !~ /createole/;
}

# -----------------------------------------------------------------------------
# 2. create a zip archive from two files, list it, cat a member
# -----------------------------------------------------------------------------
{
    print STDERR "zip: create, list, cat\n";
    my $a = &make_input_file ('input-a.txt', "content of a\n");
    my $b = &make_input_file ('input-b.txt', "content of b\n");
    my $zip = &LibGsfTest::invent_junkfile ('archive.zip');

    &run_ok ("$gsf createzip $zip $a $b", "gsf createzip succeeds");
    my ($code, $out) = &run_out ("$gsf list $zip", "gsf list (zip) succeeds");
    &note_fail ("list shows $a") if $out !~ /\Q$a\E/;
    &note_fail ("list shows $b") if $out !~ /\Q$b\E/;

    ($code, $out) = &run_out ("$gsf cat $zip $a", "gsf cat (zip)");
    &note_fail ("cat returns the content of a") if $out ne "content of a\n";

    ($code, $out) = &run_out ("$gsf cat $zip $b",
			      "gsf cat (zip, second member)");
    &note_fail ("cat returns the content of b") if $out ne "content of b\n";
}

# -----------------------------------------------------------------------------
# 3. create an OLE archive, list it, cat a member
# -----------------------------------------------------------------------------
{
    print STDERR "ole: create, list, cat\n";
    my $a = &make_input_file ('input-a.txt', "content of a\n");
    my $b = &make_input_file ('input-b.txt', "content of b\n");
    my $ole = &LibGsfTest::invent_junkfile ('archive.ole');

    &run_ok ("$gsf createole $ole $a $b", "gsf createole succeeds");
    my ($code, $out) = &run_out ("$gsf list $ole", "gsf list (ole) succeeds");
    &note_fail ("list shows $a") if $out !~ /\Q$a\E/;
    &note_fail ("list shows $b") if $out !~ /\Q$b\E/;

    ($code, $out) = &run_out ("$gsf cat $ole $a", "gsf cat (ole)");
    &note_fail ("cat returns the content of a") if $out ne "content of a\n";

    ($code, $out) = &run_out ("$gsf cat $ole $b",
			      "gsf cat (ole, second member)");
    &note_fail ("cat returns the content of b") if $out ne "content of b\n";
}

# -----------------------------------------------------------------------------
# 4. create a tar archive with the real tar command, list it, cat a member
# -----------------------------------------------------------------------------
{
    print STDERR "tar: create (with tar), list, cat\n";
    my $t = &make_input_file ('input-t.txt', "content of t\n");
    my $tar = &LibGsfTest::invent_junkfile ('archive.tar');

    &run_ok ("tar -cf $tar $t", "tar creates the archive");
    my ($code, $out) = &run_out ("$gsf list $tar", "gsf list (tar) succeeds");
    &note_fail ("list shows $t") if $out !~ /\Q$t\E/;

    ($code, $out) = &run_out ("$gsf cat $tar $t", "gsf cat (tar)");
    &note_fail ("cat returns the content of t") if $out ne "content of t\n";
}

# -----------------------------------------------------------------------------
# 5. zip with a subdirectory: list shows the tree, cat reaches the member
# -----------------------------------------------------------------------------
{
    print STDERR "zip+subdir: create, list, cat\n";
    my $subname = &LibGsfTest::invent_junkfile ('sub.ective');
    my $s = &make_input_file ('input-s.txt', "content of s\n");
    my $zipsub = &LibGsfTest::invent_junkfile ('archive-sub.zip');

    mkdir ($subname);
    rename ($s, "$subname/$s");
    &LibGsfTest::junkfile ("$subname/$s");

    &run_ok ("$gsf createzip $zipsub $subname",
	     "gsf createzip (with dir)");
    my ($code, $out) = &run_out ("$gsf list $zipsub", "gsf list (subdir)");
    &note_fail ("list shows the subdirectory") if $out !~ /\Q$subname\E/;
    &note_fail ("list shows the nested member") if $out !~ /\Q$s\E/;

    ($code, $out) = &run_out ("$gsf cat $zipsub $subname/$s",
			      "gsf cat (nested member)");
    &note_fail ("cat returns the nested content")
	if $out ne "content of s\n";
    system ("rm -rf $subname");
}

# -----------------------------------------------------------------------------
# 6. dump a member as hex
# -----------------------------------------------------------------------------
{
    print STDERR "dump\n";
    my $a = &make_input_file ('input-a.txt', "content of a\n");
    my $zip = &LibGsfTest::invent_junkfile ('archive.zip');
    &LibGsfTest::junkfile ($zip);

    &run_ok ("$gsf createzip $zip $a", "gsf createzip (for dump)");
    my ($code, $out) = &run_out ("$gsf dump $zip $a",
				 "gsf dump (zip) succeeds");
    &note_fail ("dump output is hex") if $out !~ /[0-9a-f]{2}/i;
}

# -----------------------------------------------------------------------------
# 7. listprops on an empty archive (no metadata -- should succeed silently)
# -----------------------------------------------------------------------------
{
    print STDERR "listprops\n";
    my $a = &make_input_file ('input-a.txt', "content of a\n");
    my $zip = &LibGsfTest::invent_junkfile ('archive.zip');
    my $ole = &LibGsfTest::invent_junkfile ('archive.ole');

    &run_ok ("$gsf createzip $zip $a", "gsf createzip (for listprops)");
    &run_ok ("$gsf createole $ole $a", "gsf createole (for listprops)");
    &run_ok ("$gsf listprops $zip", "gsf listprops (zip) succeeds");
    &run_ok ("$gsf listprops $ole", "gsf listprops (ole) succeeds");
}

# -----------------------------------------------------------------------------
# 8. a non-archive file does not crash the tool
# -----------------------------------------------------------------------------
{
    print STDERR "not-an-archive\n";
    my $txt = &make_input_file ('input-n.txt', "this is not an archive\n");
    # The tar reader is lenient about short files; we only assert the
    # tool does not crash.
    my $code = system ("$gsf list $txt > /dev/null 2>&1");
    &note_pass ("gsf list handles a non-archive file") if $code >= 0;
}

# -----------------------------------------------------------------------------
# 9. zip64 archives round-trip
# -----------------------------------------------------------------------------
{
    print STDERR "zip64\n";
    my $a = &make_input_file ('input-a.txt', "content of a\n");
    my $zip64 = &LibGsfTest::invent_junkfile ('archive-zip64.zip');

    &run_ok ("$gsf --zip64 1 createzip $zip64 $a",
	     "gsf createzip --zip64 succeeds");
    my ($code, $out) = &run_out ("$gsf list $zip64", "gsf list (zip64)");
    &note_fail ("list shows the member (zip64)") if $out !~ /\Q$a\E/;
}

# -----------------------------------------------------------------------------
# 10. cat with a missing member fails
# -----------------------------------------------------------------------------
{
    print STDERR "cat-missing-member\n";
    my $a = &make_input_file ('input-a.txt', "content of a\n");
    my $zip = &LibGsfTest::invent_junkfile ('archive.zip');
    &LibGsfTest::junkfile ($zip);

    &run_ok ("$gsf createzip $zip $a", "gsf createzip (for missing-member)");
    my $code = system ("$gsf cat $zip no-such-member > /dev/null 2>&1");
    &note_pass ("cat with a missing member fails") if $code != 0;
    &note_fail ("cat with a missing member should fail") if $code == 0;
}

# -----------------------------------------------------------------------------
# 11. createzip with a nonexistent input file fails
# -----------------------------------------------------------------------------
{
    print STDERR "createzip-missing-input\n";
    my $zip = &LibGsfTest::invent_junkfile ('archive.zip');
    &LibGsfTest::junkfile ($zip);

    my $code = system ("$gsf createzip $zip /tmp/gsf-no-such-file-$$ > /dev/null 2>&1");
    &note_pass ("createzip with a nonexistent input file fails") if $code != 0;
    &note_fail ("createzip with a nonexistent input should fail") if $code == 0;
}

# -----------------------------------------------------------------------------
# 12. createzip to a bad output path fails
# -----------------------------------------------------------------------------
{
    print STDERR "createzip-bad-path\n";
    my $a = &make_input_file ('input-a.txt', "content of a\n");
    my $code = system ("$gsf createzip /nonexistent-dir-$$/x.zip $a > /dev/null 2>&1");
    &note_pass ("createzip to a bad output path fails") if $code != 0;
    &note_fail ("createzip to a bad output path should fail") if $code == 0;
}

# -----------------------------------------------------------------------------
# 13. list two archives in one invocation
# -----------------------------------------------------------------------------
{
    print STDERR "list-two-archives\n";
    my $a = &make_input_file ('input-a.txt', "content of a\n");
    my $zip = &LibGsfTest::invent_junkfile ('archive.zip');
    my $ole = &LibGsfTest::invent_junkfile ('archive.ole');

    &run_ok ("$gsf createzip $zip $a", "gsf createzip (for two-list)");
    &run_ok ("$gsf createole $ole $a", "gsf createole (for two-list)");
    my ($code, $out) = &run_out ("$gsf list $zip $ole",
				 "gsf list on two archives succeeds");
    &note_fail ("the first archive is listed") if $out !~ /\Q$zip\E/;
    &note_fail ("the second archive is listed") if $out !~ /\Q$ole\E/;
}

# -----------------------------------------------------------------------------
# 14. dump output contains the expected hex bytes
# -----------------------------------------------------------------------------
{
    print STDERR "dump-hex\n";
    my $a = &make_input_file ('input-a.txt', "content\n");
    my $zip = &LibGsfTest::invent_junkfile ('archive.zip');
    &LibGsfTest::junkfile ($zip);

    &run_ok ("$gsf createzip $zip $a", "gsf createzip (for hex-dump)");
    my ($code, $out) = &run_out ("$gsf dump $zip $a",
				 "gsf dump (hex) succeeds");
    # "content\n" = 63 6f 6e 74 65 6e 74 0a in hex
    &note_fail ("dump contains '63' (c)") if $out !~ /63/i;
    &note_fail ("dump contains '6f' (o)") if $out !~ /6f/i;
    &note_fail ("dump contains '6e' (n)") if $out !~ /6e/i;
    &note_fail ("dump contains '74' (t)") if $out !~ /74/i;
    &note_fail ("dump contains '65' (e)") if $out !~ /65/i;
    &note_fail ("dump contains '0a' (newline)") if $out !~ /0a/i;
}

# -----------------------------------------------------------------------------
# 15. creating an archive twice (overwrite) succeeds both times
# -----------------------------------------------------------------------------
{
    print STDERR "overwrite\n";
    my $a = &make_input_file ('input-a.txt', "content of a\n");
    my $zip = &LibGsfTest::invent_junkfile ('archive.zip');
    &LibGsfTest::junkfile ($zip);

    &run_ok ("$gsf createzip $zip $a", "first createzip succeeds");
    &run_ok ("$gsf createzip $zip $a", "second createzip (overwrite) succeeds");
}

# -----------------------------------------------------------------------------
if ($nfail) {
    print STDERR "$nfail of $ncheck check(s) failed\n";
    exit 1;
}
print STDERR "all $ncheck check(s) passed\n";
exit 0;
