#!/usr/bin/perl
my $desc = "Library source files must include specific headers instead of rose.h:\n";

BEGIN {push @INC, $1 if $0 =~ /(.*)\//}
use strict;
use FileLister;
use Policies;

@ARGV = "src" if !@ARGV && -d "scripts/policies";

my $nfail = 0;
my $files = FileLister->new(@ARGV);
while (my $filename = $files->next_file) {
    next unless $filename =~ /\.(?:C|cc|cpp|cxx)$/;
    next if $filename =~ /\/utility_functionsImpl\.C$/;
    next if is_disabled($filename);
    open my $source, "<", $filename or die "cannot read $filename: $!\n";
    local $! = 0;
    while (my $line = <$source>) {
        if ($line =~ /^\s*#\s*include\s*["<]rose\.h[>"]/ &&
            $line !~ /\bPOLICY_OK\b/) {
            print $desc unless $nfail++;
            print "  $filename\n";
            last;
        }
    }
    die "cannot read $filename: $!\n" if $!;
    close $source or die "cannot close $filename: $!\n";
}

exit($nfail > 0 ? 1 : 0);
