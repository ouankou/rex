#!/usr/bin/perl
# DO NOT DISABLE without first checking with a ROSE core developer
my $desc = <<EOF;
All header files found in a source tree should be used somewhere in that source
tree. Unused header files are characteristic of deleted code that should be
removed. The following header files exist but are unused:
EOF

BEGIN {push @INC, $1 if $0 =~ /(.*)\//}
use strict;
use FileLister;
my $warning = " (warning)";	# non-empty means issue warnings rather than errors, but see below

# Create an index of all header files.
# Each key is the base name of the header file;
# Each value is an array of header filenames that map to this key.
my %index;
for my $file (FileLister->new(@ARGV)->all_files()) {
    # utility_functionsImpl.C is misnamed -- it's actually a header file.
    next unless $file =~ /\.(h|hh|hpp)$/ || $file =~ /\/utility_functionsImpl.C$/;
    next if $file =~ /\b(tests|projects|src\/AstNodes)\//;
    my($basename) = $file =~ /([^\/]+)$/;
    my($key) = lc $basename;
    $index{$key} = [] unless exists $index{$key};
    push @{$index{$key}}, $file;
}

# Look for #include statements in all source files and delete the matching entry from %index.
my $files = FileLister->new(@ARGV);
while (my $file = $files->next_file) {
  next unless $file =~ /\.(h|hh|hpp|c|cc|C|cpp|[fF]\w*)$/; # look only at C/C++/Fortran source code
  next if $file =~ /\b(tests|projects|src\/AstNodes)\//; # skip test and project directories
  open my $source, "<", $file or die "$file: cannot open: $!\n";
  local $! = 0;
  while (<$source>) {
    my($path,$name);
    if ((($path,$name) = /^\s*#\s*include\s*["<](.*?)([^\/]*?)[>"]/) || # C/C++
	(($path,$name) = /^\s*include\s*["'](.*?)([^\/]*?)['"]/)) {     # Fortran

      next unless exists $index{lc $name};

      if ($path eq "") {
	delete $index{lc $name}; # just an optimized version of what would happen for the else case.
      } else {
	# Remove header files from the index if the header file base name matches (which it will since
	# that's how the index is organized) and any directories specified in the #include directive
	# also match.
	my @non_matching_headers;
	my @include_parts = reverse split "/", lc($path.$name);
	for my $header (@{$index{lc $name}}) {
	  my @header_parts = reverse split "/", lc $header;
	  my $does_match = 1;
	  for (my $i=0; $does_match && $i<@include_parts && $i<@header_parts; $i++) {
	    $does_match = $include_parts[$i] eq $header_parts[$i];
	  }
	  push @non_matching_headers, $header unless $does_match;
	}
	if (@non_matching_headers) {
	  $index{lc $name} = \@non_matching_headers;
	} else {
	  delete $index{lc $name};
	}
      }
    } elsif (/\b__TH[I]S_HEADER_IS_USED__\b/) { # The [I] is to make sure we don't match this particular occurrence
      # If a header file really is used but just not included in any source code, then you may indicate so in the
      # header by adding a line that matches this pattern.
      my($basename) = $file =~ /([^\/]+)$/;
      delete $index{lc $basename};
    }
  }
  die "$file: cannot read: $!\n" if $!;
  close $source or die "$file: cannot close: $!\n";
}

# Report failures
my @remaining = map {@$_} values %index;

# Max number of permissible unused headers
my $max_unused_headers = 143; # 2023-03-27

$warning = "" if @remaining > $max_unused_headers;

print $desc if @remaining;
print "  $_$warning\n" for sort @remaining;

print "Number of remaining unused header files: " . @remaining . " (max allowed: $max_unused_headers).\n";

exit(@remaining ? ($warning?128:1) : 0);
