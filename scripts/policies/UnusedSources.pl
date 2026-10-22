#!/usr/bin/perl
# DO NOT DISABLE without first checking with a ROSE core developer

# Number of failures allowed
# ONLY DECREASE THIS VALUE!
my $limit = 383; # 2023-03-31 Robb Matzke

my $desc = <<EOF;
Source files should be referenced by CMake targets or installation rules.
Review the following candidates for removal or missing build integration.
This checker matches filenames, so its results require source review.
The current allowance is $limit; reduce it as unused sources are removed.
EOF

BEGIN {push @INC, $1 if $0 =~ /(.*)\//}
use strict;
use FileLister;
my $warning = " (warning)";	# non-empty means issue warnings rather than errors, but see below

# Create an index of all source files.
# Each key is the base name of the file;
# Each value is an array of filenames that map to this key.
my %index;
for my $file (FileLister->new(@ARGV)->all_files()) {
    next unless $file =~ /\.(h|hh|hpp|c|C|cpp)$/;
    next if $file =~ /\b(tests|projects|src\/AstNodes)\//;
    my($basename) = $file =~ /([^\/]+)$/;
    my($key) = lc $basename;
    $index{$key} = [] unless exists $index{$key};
    push @{$index{$key}}, $file;
}

# Remove filenames referenced by CMake from the candidate index.
my $files = FileLister->new(@ARGV);
while (my $file = $files->next_file) {
  next unless $file =~ /\/CMakeLists\.txt$/;
  next if $file =~ /\b(tests|projects|src\/AstNodes)\//; # skip test and project directories
  open my $source, "<", $file or die "$file: cannot open: $!\n";
  local $! = 0;
  while (<$source>) {
    s/#.*//;
    s/([-+a-zA-Z_0-9\.]+)/delete $index{lc $1}/ge;
  }
  die "$file: cannot read: $!\n" if $!;
  close $source or die "$file: cannot close: $!\n";
}

# Report failures
my @remaining = map {@$_} values %index;
$warning = "" if @remaining > $limit;
print $desc if @remaining;
print "  $_$warning\n" for sort @remaining;

print "Number of remaining unused source files (limit=$limit): " . @remaining . ".\n";


exit(@remaining ? ($warning?128:1) : 0);
