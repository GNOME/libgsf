package LibGsfTest;
use strict;
use Exporter;
use File::Basename qw(fileparse);
use Config;
use XML::Parser;

$| = 1;

@LibGsfTest::ISA = qw (Exporter);
@LibGsfTest::EXPORT = qw(message
			 test_zip
			 $topsrc $top_builddir
			 $gsf $PERL);
@LibGsfTest::EXPORT_OK = qw(junkfile $unzip $zipinfo);

use vars qw($topsrc $top_builddir $PERL $verbose);
use vars qw($gsf $unzip $zipinfo);

$PERL = $Config{'perlpath'};
$PERL .= $Config{'_exe'} if $^O ne 'VMS' && $PERL !~ m/$Config{'_exe'}$/i;

$unzip = &find_program ("unzip");
$zipinfo = &find_program ("zipinfo");

$topsrc = $0;
$topsrc =~ s|/[^/]+$|/..|;
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
    my ($p) = @_;

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
	my $suppfile = "$topsrc/test/common.supp";
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
	# &dump_indented ($txt);
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

sub test_zip {
    my (%args) = @_;

    my $pfiles = $args{'files'};

    my $archive = 'test.zip';
    &junkfile ($archive);

    {
	my $cmd = &quotearg ($gsf, "createzip", $archive, @$pfiles);
	print "# $cmd\n";
	my $code = system ("$cmd 2>&1 | sed -e 's/^/| /'");
	&system_failure ($gsf, $code) if $code;
	die "$gsf failed to create the archive $archive\n" unless -e $archive;
    }

    {
	my $cmd = &quotearg ('unzip', '-q', '-t', $archive);
	print "# $cmd\n";
	my $code = system ("$cmd 2>&1 | sed -e 's/^/| /'");
	&system_failure ($gsf, $code) if $code;
    }

    foreach my $file (@$pfiles) {
	my $cmd = &quotearg ('unzip', '-p', $archive, $file);
	print "# $cmd\n";
	my $stored_data = `$cmd`;

	my $cmd = &quotearg ('cat', $file);
	print "# $cmd\n";
	my $original_data = `$cmd`;

	die "Mismatch for member $file\n"
	    unless $stored_data eq $original_data;
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
