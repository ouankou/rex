#!/usr/bin/perl
# DO NOT DISABLE without first checking with a ROSE core developer

my $desc = <<EOF;
Header files should not have a "using namespace" directive in an outer scope
since this causes unexpected behavior for code that uses those headers.

The following files violate this policy.
EOF

BEGIN {push @INC, $1 if $0 =~ /(.*)\//}
use strict;
use FileLister;
use Policies;

my $nfail=0;
my $files = FileLister->new();
while (my $filename = $files->next_file) {
  next if $filename =~ /\b(tests|projects)\//; # skip test and project directories
  # utility_functionsImpl.C is misnamed -- it's actually a header file.
  if (($filename =~ /\.(h|hh|hpp)$/ || $filename =~ /\/utility_functionsImpl.C$/) &&
      !is_disabled($filename)) {
    open my $header, "<", $filename or die "$0: cannot read $filename: $!\n";
    local $! = 0;
    while (<$header>) {
      if (/^\b(using\s+namespace\s+[a-z_A-Z]\w*)/) {
	print $desc unless $nfail++;
	print "  $filename ($1)\n";
	last;
      }
    }
    die "$0: cannot read $filename: $!\n" if $!;
    close $header or die "$0: cannot close $filename: $!\n";
  }
}

exit 1 if $nfail > 0;
exit 0;
