#!/usr/bin/perl -w

sub usage {
    print STDERR "Usage: $0 --lname <text> --srcdir path [options] [-- vars]\n";
    print STDERR "  --lname <text>             the name of the library\n";
    print STDERR "  --srcdir path              the path where Makefile.am and headers of the library can be found\n";
    print STDERR "  --outdir path              the path where built binaries can be found\n";
    print STDERR "  --prefix path              the base path where the library will be installed\n";
    print STDERR "  vars                       comma separated text substitutions\n";
    print STDERR "  -h, --help                 show this help message\n";
    exit 0;
}

defined $ARGV[0] or usage;

defined $ENV{'GTK_BASEPATH'} and $prefix = $ENV{'GTK_BASEPATH'};
if (-e 'install.options' and open OPTIONS, 'install.options') {
	my @options = <OPTIONS>;
	$_ = join ' ', @options;
	while (/((?:\"[^\"]*\"|[^\s\"])+)/) {
		my $val = $&;
		$_ = $';
		$val =~ s/\"//g;
		push @ARGV, $val;
	}
	close OPTIONS;
}
@vars = ();

while ($_ = $ARGV[0], defined $_ and /^-/) {
    shift;
    if (/^--$/)						 { push @vars, split ',', shift }
    elsif (/^--lname$/)              { $libname = shift }
    elsif (/^--srcdir$/)             { $srcdir = shift }
    elsif (/^--outdir$/)             { $outdir = shift }
    elsif (/^--prefix$/)             { $prefix = shift }
    elsif (/^--help$/ || /^-h$/)     { usage; }
    else { usage; }
}

(defined $prefix and defined $libname and defined $srcdir) or usage;

use File::Copy;

($am_libname = $libname) =~ s/-[0-9]+$//;
($ms_libname = $am_libname) =~ s/^lib//;
$am_libname =~ s/-/_/g;

open MAKEFILE, "$srcdir/Makefile.am" or die "Cannot open $srcdir/Makefile.am\n";

while (<MAKEFILE>) {
	if (/^\s*${am_libname}(?:include|_include|_la)_HEADERS\s*=\s*([^\\\n]+)?(\\)?$/) {
		do {
			if (defined $1 && $1 ne '#') {
				push @headers, split /[ \t]+/, $1;
			}
		} while (defined $2 && ($_ = <MAKEFILE>) &&
			     (/^\s*([^\\\n]+)?(\\)?$/ || /^((\#))/));
	} elsif (/^\s*${am_libname}(?:include|_include|_la)dir\s*=\s*([^ \t\\\n]+)\s*$/) {
		$includedir = $1;
		$old_includedir = '';
		while ($includedir =~ /\$\([^\)]+\)/ && $includedir ne $old_includedir) {
			$old_includedir = $includedir;
			$includedir =~ s/\$\(includedir\)/$prefix\/include/;
			foreach $var (@vars) {
				($key, $val) = split '=', $var;
				$includedir =~ s/\$\($key\)/$val/;
			}
		}
	}
}

close MAKEFILE;

sub install_dir {
	my $stuff = shift;

	if (!-d $stuff) {
		print "Creating directory $stuff\n";
		$dir = '';
		foreach $part (split /[\/\\]/, $stuff) {
			$dir .= ($dir ne '' ? '/' : '') . $part;
			if (!-d $dir) {
				mkdir($dir, 755) or die "Cannot create directory $dir";
			}
		}
	}
}

sub _install_file {
	my $src = shift;
	my $dst = shift;
	my (@stat1, @stat2);

	!-e $src and die "Source file $src doesn't exist\n";

	if ((!-e $dst ||
		 (-e $dst &&
		  (@stat1 = stat($src)) && (@stat2 = stat($dst)) &&
		  $stat1[9] > $stat2[9]))) {
		print "Installing $src as $dst\n";
		copy($src, $dst) or die "Cannot install file $src as $dst\n";
	}
}

sub install_file {
	my $src = shift;
	my $dst = shift;
	my $dst_dir;

	($dst_dir = $dst) =~ s/[\/\\][^\/\\]+$//;
	$dst_dir ne '' and install_dir($dst_dir);
	_install_file($src, $dst);
}

if (defined $outdir) {
	install_file("$outdir/$libname.dll", "$prefix/bin/$libname.dll");
	install_file("$outdir/$ms_libname.lib", "$prefix/lib/$ms_libname.lib");
}

defined @headers and install_dir($includedir);
foreach $header (@headers) {
	_install_file("$srcdir/$header", "$includedir/$header");
}
