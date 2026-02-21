#!/usr/bin/env bash
set -euo pipefail

if [[ $# -ne 2 ]]; then
  echo "usage: $0 <input-source-or-rose-file> <output-file>" >&2
  exit 1
fi

input_file=$1
output_file=$2

if [[ ! -f "$input_file" ]]; then
  : >"$output_file"
  exit 0
fi

perl - "$input_file" >"$output_file" <<'PERL'
use strict;
use warnings;

my $path = shift @ARGV;
open my $fh, "<", $path or die "failed to open '$path': $!";
local $/;
my $text = <$fh>;
close $fh;

$text = "" unless defined $text;
$text =~ s/\r\n/\n/g;

sub trim {
    my ($value) = @_;
    $value =~ s/^\s+//;
    $value =~ s/\s+$//;
    return $value;
}

sub canonicalize_directive {
    my ($directive) = @_;
    $directive =~ s{/\*.*?\*/}{}g;
    $directive =~ s{//.*$}{}g;
    $directive =~ s/[ \t\r]+/ /g;
    $directive = trim($directive);
    $directive =~ s/\s+\(/(/g;
    $directive =~ s/\s*,\s*/,/g;
    $directive =~ s/\(\s*/(/g;
    $directive =~ s/\s*\)/)/g;
    $directive =~ s/\s*\[\s*/[/g;
    $directive =~ s/\s*\]\s*/]/g;
    $directive =~ s/\s*::\s*/::/g;
    $directive =~ s/\s*:\s*/:/g;
    $directive =~ s/\s*([+\-*\/%&|^<>!=?])\s*/$1/g;
    $directive =~ s/[ \t]+/ /g;
    return trim($directive);
}

my @directives;
my @lines = split /\n/, $text, -1;
for (my $i = 0; $i <= $#lines; ++$i) {
    my $line = $lines[$i];

    # C/C++ pragmas, including '\' continuation lines.
    if ($line =~ /^[ \t]*\#\s*pragma[ \t]+omp\b/i) {
        my $directive = $line;
        while ($directive =~ /\\[ \t]*$/ && $i + 1 <= $#lines) {
            $directive =~ s/\\[ \t]*$//;
            ++$i;
            my $next = $lines[$i];
            $next =~ s/^[ \t]+//;
            $directive .= " " . $next;
        }
        $directive =~ s/^[ \t]*\#\s*pragma[ \t]+omp\b/#pragma omp/i;
        $directive = canonicalize_directive($directive);
        push @directives, $directive if length $directive;
        next;
    }

    # OpenMP directives embedded in preprocessor macro definitions.
    # OMPVV Fortran tests use this pattern for module-level `requires`.
    if ($line =~ /^[ \t]*\#\s*define\b.*[!cC\*]\$omp\b/i) {
        my $directive = $line;
        $directive =~ s/^[^!cC\*]*([!cC\*]\$omp\b.*)$/$1/i;
        $directive =~ s/^[ \t]*[!cC\*]\$omp\b[ \t]*/!\$omp /i;
        $directive = canonicalize_directive($directive);
        push @directives, $directive if length $directive;
        next;
    }

    # Fortran directives, including free-form continuation via trailing '&' and
    # following '!$omp&' lines.
    if ($line =~ /^[ \t]*[!cC\*]\$omp\b/i) {
        my $segment = $line;
        $segment =~ s/^[ \t]*[!cC\*]\$omp\b[ \t]*/!\$omp /i;
        $segment =~ s/[ \t]*&[ \t]*$//;
        $segment =~ s/[ \t]+!.*$//;
        my $directive = trim($segment);
        my $continued = ($line =~ /&[ \t]*$/) ? 1 : 0;

        while ($continued && $i + 1 <= $#lines &&
               $lines[$i + 1] =~ /^[ \t]*[!cC\*]\$omp\b/i) {
            ++$i;
            my $next = $lines[$i];
            $continued = ($next =~ /&[ \t]*$/) ? 1 : 0;
            $next =~ s/^[ \t]*[!cC\*]\$omp\b[ \t]*&?[ \t]*//i;
            $next =~ s/[ \t]*&[ \t]*$//;
            $next =~ s/[ \t]+!.*$//;
            $next = trim($next);
            $directive .= " $next" if length $next;
        }

        $directive = canonicalize_directive($directive);
        push @directives, $directive if length $directive;
    }
}

for my $directive (@directives) {
    print $directive, "\n";
}
PERL
