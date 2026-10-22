#!/usr/bin/perl
# DO NOT DISABLE without first checking with a ROSE core developer

my $desc = <<EOF;
The following files must not be included in ROSE public header files because
they pollute the user's global name space with symbols whose names do not begin
with "ROSE", "Rose", or "rose":
   rose.h
   sage3.h
   sage3basic.h
   rose_config.h

An alternative is to include these (except rose.h) in .C files instead since
that would prevent any global symbols from leaking into user programs via
ROSE public header files.

The following header files violate this policy.
EOF


BEGIN {push @INC, $1 if $0 =~ /(.*)\//}
use strict;
use FileLister;
use Policies;

# If no files or directories are specified and the CWD is the top of the ROSE source tree, then check
# only the "src" directory.
@ARGV = "src" if !@ARGV && -d "scripts/policies";

my $nfail=0;
my $files = FileLister->new(@ARGV);
while (my $filename = $files->next_file) {
    # utility_functionsImpl.C is misnamed--it's actually a header file.
    if (($filename=~/\.(h|hh|hpp|code2|macro)$/ || $filename=~/\/utility_functionsImpl.C$/) &&
        !is_disabled($filename)) {
        open my $header, "<", $filename or die "$0: cannot read $filename: $!\n";
        local $! = 0;
	while (<$header>) {
	    if (/^\s*#\s*include\s*["<]((rose|sage3|sage3basic|rose_config)\.h)[>"]/ && !/\bPOLICY_OK\b/) {
		print $desc unless $nfail++;
		printf "  %1s (%1s)\n", $filename, $1;
		last;
	    }
	}
        die "$0: cannot read $filename: $!\n" if $!;
        close $header or die "$0: cannot close $filename: $!\n";
    }
}

exit 1 if $nfail > 0;
exit 0;
