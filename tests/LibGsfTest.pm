package LibGsfTest;
use strict;
use Exporter;
use File::Basename qw(fileparse);
use Config;
use XML::Parser;

$| = 1;

@LibGsfTest::ISA = qw (Exporter);
@LibGsfTest::EXPORT = qw(message test_valgrind
			 test_zip
			 $topsrc $top_builddir
			 $gsf $PERL);
@LibGsfTest::EXPORT_OK = qw(junkfile $unzip $zipinfo);

use vars qw($topsrc $top_builddir $PERL $verbose);
use vars qw($gsf $unzip $zipinfo $sevenz);

$PERL = $Config{'perlpath'};
$PERL .= $Config{'_exe'} if $^O ne 'VMS' && $PERL !~ m/$Config{'_exe'}$/i;

$unzip = &find_program ("unzip");
$zipinfo = &find_program ("zipinfo");
$sevenz = &find_program ("7z", 1);

$topsrc = $0;
$topsrc =~ s|/[^/]+$|/..|;
$topsrc =~ s|^\./(.)|$1|;
$topsrc =~ s|/tests/\.\.$||;

$top_builddir = "..";
$gsf = "$top_builddir/tools/gsf";
$verbose = 0;

# -----------------------------------------------------------------------------

my @tempfiles;
END {
    unlink @tempfiles;
}

sub junkfile {
    my ($fn) = @_;
    push @tempfiles, $fn;
}

sub removejunk {
    my ($fn) = @_;
    unlink $fn;

    if (@tempfiles && $fn eq $tempfiles[-1]) {
	scalar (pop @tempfiles);
    }
}

# -----------------------------------------------------------------------------

sub system_failure {
    my ($program,$code) = @_;

    if ($code == -1) {
	die "failed to run $program: $!\n";
    } elsif ($code >> 8) {
	my $sig = $code >> 8;
	die "$program died due to signal $sig\n";
    } else {
	die "$program exited with exit code $code\n";
    }
}

sub read_file {
    my ($fn) = @_;

    local (*FIL);
    open (FIL, $fn) or die "Cannot open $fn: $!\n";
    local $/ = undef;
    my $lines = <FIL>;
    close FIL;

    return $lines;
}

sub write_file {
    my ($fn,$contents) = @_;

    local (*FIL);
    open (FIL, ">$fn.tmp") or die "Cannot create $fn.tmp: $!\n";
    print FIL $contents;
    close FIL;
    rename "$fn.tmp", $fn;
}

sub update_file {
    my ($fn,$contents) = @_;

    my @stat = stat $fn;
    die "Cannot stat $fn: $!\n" unless @stat > 2;

    &write_file ($fn,$contents);

    chmod $stat[2], $fn or
	die "Cannot chmod $fn: $!\n";
}

# Print a string with each line prefixed by "| ".
sub dump_indented {
    my ($txt) = @_;
    return if $txt eq '';
    $txt =~ s/^/| /gm;
    $txt = "$txt\n" unless substr($txt, -1) eq "\n";
    print STDERR $txt;
}

sub find_program {
    my ($p,$qoptional) = @_;

    if ($p =~ m{/}) {
	return $p if -x $p;
    } else {
	my $PATH = exists $ENV{'PATH'} ? $ENV{'PATH'} : '';
	foreach my $dir (split (':', $PATH)) {
	    $dir = '.' if $dir eq '';
	    my $tentative = "$dir/$p";
	    return $tentative if -x $tentative;
	}
    }

    return undef if $qoptional;

    &report_skip ("$p is missing");
}

# -----------------------------------------------------------------------------

sub message {
    my ($message) = @_;
    print "-" x 79, "\n";
    my $me = $0;
    $me =~ s|^.*/||;
    foreach (split (/\n/, $message)) {
	print "$me: $_\n";
    }
}

# -----------------------------------------------------------------------------

sub test_command {
    my ($cmd,$test) = @_;

    my $output = `$cmd 2>&1`;
    my $err = $?;
    die "Failed command: $cmd\n" if $err;

    &dump_indented ($output);
    local $_ = $output;
    if (&$test ($output)) {
	print STDERR "Pass\n";
    } else {
	die "Fail\n";
    }
}

# -----------------------------------------------------------------------------

sub test_valgrind {
    my ($cmd,$uselibtool) = @_;

    local (%ENV) = %ENV;
    $ENV{'G_DEBUG'} .= ':gc-friendly:resident-modules';
    $ENV{'G_SLICE'} = 'always-malloc';
    delete $ENV{'VALGRIND_OPTS'};

    my $outfile = 'valgrind.log';
    unlink $outfile;
    die "Cannot remove $outfile.\n" if -f $outfile;
    &junkfile ($outfile);

    my $valhelp = `valgrind --help 2>&1`;
    &report_skip ("Valgrind is not available") unless defined $valhelp;
    die "Problem running valgrind.\n" unless $valhelp =~ /log-file/;

    my $valvers = `valgrind --version`;
    die "Problem running valgrind.\n"
	unless $valvers =~ /^valgrind-(\d+)\.(\d+)\.(\d+)/;
    $valvers = $1 * 10000 + $2 * 100 + $3;
    &report_skip ("Valgrind is too old") unless $valvers >= 30500;

    $cmd = "--gen-suppressions=all $cmd";

    {
	my $suppfile = "$topsrc/tests/common.supp";
	&report_skip ("file $suppfile does not exist") unless -r $suppfile;
	$cmd = "--suppressions=$suppfile $cmd" if -r $suppfile;
    }

    {
	my $suppfile = $0;
	$suppfile =~ s/\.pl$/.supp/;
	$cmd = "--suppressions=$suppfile $cmd" if -r $suppfile;
    }

    # $cmd = "--show-reachable=yes $cmd";
    $cmd = "--show-below-main=yes $cmd";
    $cmd = "--leak-check=full $cmd";
    $cmd = "--num-callers=20 $cmd";
    $cmd = "--track-fds=yes $cmd";
    if ($valhelp =~ /--log-file-exactly=/) {
	$cmd = "--log-file-exactly=$outfile $cmd";
    } else {
	$cmd = "--log-file=$outfile $cmd";
    }
    $cmd = "valgrind $cmd";
    $cmd = "../libtool --mode=execute $cmd" if $uselibtool;

    my $code = system ($cmd);
    &system_failure ('valgrind', $code) if $code;

    my $txt = &read_file ($outfile);
    &removejunk ($outfile);
    my $errors = ($txt =~ /ERROR\s+SUMMARY:\s*(\d+)\s+errors?/i)
	? $1
	: -1;
    if ($errors == 0) {
	&dump_indented ($txt) if $verbose;
	print STDERR "Pass\n";
	return;
    }

    &dump_indented ($txt);
    die "Fail\n";
}

# -----------------------------------------------------------------------------

sub quotearg {
    return join (' ', map { &quotearg1 ($_) } @_);
}

sub quotearg1 {
    my ($arg) = @_;

    return "''" if $arg eq '';
    my $res = '';
    while ($arg ne '') {
	if ($arg =~ m!^([-=/._a-zA-Z0-9]+)!) {
	    $res .= $1;
	    $arg = substr ($arg, length $1);
	} else {
	    $res .= "\\" . substr ($arg, 0, 1);
	    $arg = substr ($arg, 1);
	}
    }
    return $res;
}

# -----------------------------------------------------------------------------

sub zipinfo_callback {
    my ($archive) = @_;

    my @result = ();

    my $entry = undef;
    foreach (`$zipinfo -v $archive`) {
	print STDERR "| $_" if $verbose;

	if (/^Central directory entry #\d+:$/) {
	    push @result, $entry if defined $entry;
	    $entry = {};
	    next;
	}

	if ($entry && /^\s*- A subfield with ID 0x0001 \(PKWARE 64-bit sizes\)/) {
	    $entry->{'zip64'} = 1;
	    next;
	}

	if ($entry && /^\s*- A subfield with ID 0x4949 /) {
	    $entry->{'ignore'} = 1;
	    next;
	}

	if ($entry && /^  *(\S.*\S):\s*(\S.*)$/) {
	    my $field = $1;
	    my $val = $2;
	    $val =~ s/ (bytes|characters)$//;
	    $entry->{$field} = $val;
	    next;
	}

	if ($entry && keys %$entry == 0 && /^  (.*)$/) {
	    $entry->{'name'} = $1;
	    next;
	}
    }
    push @result, $entry if defined $entry;

    return (undef,\@result);
}

sub test_zip {
    my (%args) = @_;

    $args{'create-arg'} = 'createzip';
    $args{'ext'} = 'zip';
    $args{'archive-tester'} = $sevenz ? [$sevenz, 't'] : [$unzip, '-q', '-t'];
    $args{'independent-cat'} = [$unzip, '-p'];
    $args{'infofunc'} = \&zipinfo_callback;

    foreach my $test (@{$args{'zip-member-tests'} || []}) {
	$args{'member-tests'} ||= [];

	if ($test eq 'zip64') {
	    push @{$args{'member-tests'}},
	    sub {
		my ($member) = @_;
		my $name = $member->{'name'};
		die "Member $name should have been zip64\n" unless $member->{'zip64'};
	    };
	    next;
	}

	if ($test eq '!zip64') {
	    push @{$args{'member-tests'}},
	    sub {
		my ($member) = @_;
		my $name = $member->{'name'};
		die "Member $name should not be zip64\n" if $member->{'zip64'};
	    };
	    next;
	}

	if ($test eq '!ignore') {
	    push @{$args{'member-tests'}},
	    sub {
		my ($member) = @_;
		my $name = $member->{'name'};
		die "Member $name should not use ignore extension\n" if $member->{'ignore'};
	    };
	    next;
	}
    }

    &test_archive (\%args);
}

# -----------------------------------------------------------------------------

sub test_archive {
    my ($pargs) = @_;

    my $pfiles = $pargs->{'files'};
    my $ext = $pargs->{'ext'};
    my $tester = $pargs->{'archive-tester'};
    my $independent_cat = $pargs->{'independent-cat'};
    my $member_tests = $pargs->{'member-tests'};
    my $infofunc = $pargs->{'infofunc'};

    my $archive = "test.$ext";
    &junkfile ($archive);

    {
	my $gsfcmd = $pargs->{'create-arg'};
	my $gsfopts = $pargs->{'create-options'} || [];
	my $stdio = $pargs->{'stdio'};
	my $cmd = &quotearg ($gsf, $gsfcmd, @$gsfopts,
			     ($stdio ? "-" : $archive),
			     @$pfiles);
	print STDERR "# $cmd\n";
	my $code =
	    $stdio
	    ? system ("($cmd | cat >$archive) 2>&1 | sed -e 's/^/| /'")
	    : system ("$cmd 2>&1 | sed -e 's/^/| /'");
	&system_failure ($gsf, $code) if $code;
	die "$gsf failed to create the archive $archive\n" unless -e $archive;
    }

    if ($tester) {
	my $cmd = &quotearg (@$tester, $archive);
	print STDERR "# $cmd\n";
	my $code = system ("$cmd 2>&1 | sed -e 's/^/| /'");
	&system_failure ($tester->[0], $code) if $code;
    }

    foreach my $file (@$pfiles) {
	my $cmd = &quotearg ('cat', $file);
	print STDERR "# $cmd\n";
	my $original_data = `$cmd`;

	# Match stored data using external extractor if we have one
	if ($independent_cat) {
	    my $cmd = &quotearg (@$independent_cat, $archive, $file);
	    print STDERR "# $cmd\n";
	    my $stored_data = `$cmd`;

	    die "Mismatch for member $file\n"
		unless $stored_data eq $original_data;
	}

	# Match stored data using our own extractor
	{
	    my $cmd = &quotearg ($gsf, 'cat', $archive, $file);
	    print STDERR "# $cmd\n";
	    my $stored_data = `$cmd`;

	    die "Mismatch for member $file\n"
		unless $stored_data eq $original_data;
	}

	print STDERR "# Member $file matched.\n";
    }

    if ($infofunc) {
	my ($ainfo,$minfo) = &$infofunc ($archive);

	foreach my $test (@$member_tests) {
	    foreach my $member (@$minfo) {
		&$test ($member);
	    }
	}
    }
}

# -----------------------------------------------------------------------------

sub report_skip {
    my ($txt) = @_;

    print "SKIP -- $txt\n";
    # 77 is magic for automake
    exit 77;
}

# -----------------------------------------------------------------------------
# Setup a consistent environment

&report_skip ("all tests skipped") if exists $ENV{'LIBGSF_SKIP_TESTS'};

delete $ENV{'G_SLICE'};
$ENV{'G_DEBUG'} = 'fatal_criticals';

delete $ENV{'LANG'};
delete $ENV{'LANGUAGE'};
foreach (keys %ENV) { delete $ENV{$_} if /^LC_/; }
$ENV{'LC_ALL'} = 'C';

# libgsf listens for this
delete $ENV{'WINDOWS_LANGUAGE'};

if (@ARGV && $ARGV[0] eq '--verbose') {
    $verbose = 1;
    scalar shift @ARGV;
}

1;
