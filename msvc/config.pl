#!/usr/bin/perl -w

sub usage {
    print STDERR "Usage: $0 [options]\n";
    print STDERR "  --conf file                config file, by default config.txt\n";
    print STDERR "  --template file            config.h.in file, by default ../config.h.in\n";
    print STDERR "  --configure file           configure.in file, by default ../configure.in\n";
    print STDERR "  -h, --help                 show this help message\n";
    exit 0;
}

$config = 'config.txt';
$template = '../config.h.in';
$configure = '../configure.in';

while ($_ = $ARGV[0], defined $_ and /^-/) {
    shift;
    last if /^--$/;
    if (/^--conf$/)                  { $config = shift }
    elsif (/^--template$/)           { $template = shift }
    elsif (/^--configure$/)          { $configure = shift }
    elsif (/^--help$/ || /^-h$/)     { usage; }
    else { usage; }
}

open CONFIGURE_IN, $configure or die "Cannot open $configure\n";
@configure_in = <CONFIGURE_IN>;
$configure_in = join("\n", @configure_in);
undef @configure_in;
close CONFIGURE_IN;

$defines{''} = '@';
open CONFIG, $config or die "Cannot open $config\n";
while (<CONFIG>) {
	if (/^([A-Za-z_]\w*)(\([^\)]+\))?(=.*)?$/) {
		my $key = $1;
		my $brace = defined $2 ? $2 : '';
		my $val = undef;
		if (defined $3) {
			my $changed = 1;
			$val = $3;
			while ($changed and $val =~ /\@([\w_]*)\@/) {
				my $temp;
				$changed = (defined $defines{$1} and $temp = $defines{$1} and $val =~ s/\@$1\@/$temp/);
				!$changed and die "\@$1\@ is not defined.\n";
			}
		}
		push @{$predefines{$key}}, ($brace, defined $val ? substr($val, 1) : NULL);
	} elsif (/^\@([\w_]+)\@=(.+)$/) {
		my ($key, $pattern) = ($1, $2);
		if ($configure_in =~ /$2/) {
			$defines{$key} = $1;
		} else {
			die "Cannot extract \@$key\@ from $configure by using pattern /$pattern/.\n";
		}
	} elsif (/^\#([\w_]+)\#=(.*)$/) {
		$defines{$1} = $2;
	}
}
close CONFIG;

open CONFIG_IN, $template or die "Cannot open $template\n";
while (<CONFIG_IN>) {
	if (/^\#undef ([A-Za-z_]\w*)$/) {
		if (defined $predefines{$1}) {
			my @info = @{$predefines{$1}};
			print $info[1] ne NULL ? "#define $1$info[0] $info[1]\n" : "/* #undef $1 */\n";
		} else {
			die "$1 is not defined in $config, please report.";
		}
	}
	else {
		print $_;
	}
}
close CONFIG_IN;
