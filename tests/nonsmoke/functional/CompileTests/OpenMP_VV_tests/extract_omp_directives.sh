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

    # Fortran directives, including free-form continuation via trailing '&' and
    # following '!$omp&'/'!$ompx&' or plain '&' continuation lines.
    if ($line =~ /^[ \t]*[!cC\*]\$(omp|ompx)\b/i) {
        my $sentinel = lc($1);
        my $segment = $line;
        $segment =~ s/^[ \t]*[!cC\*]\$(?:omp|ompx)\b[ \t]*//i;
        if ($sentinel eq "ompx") {
            $segment = "!\$ompx " . $segment;
        } else {
            $segment = "!\$omp " . $segment;
        }
        $segment =~ s/[ \t]*&[ \t]*$//;
        $segment =~ s/[ \t]+!.*$//;
        my $directive = trim($segment);
        my $continued = ($line =~ /&[ \t]*$/) ? 1 : 0;
        my $join_without_space = ($line =~ /[A-Za-z0-9_]&[ \t]*$/) ? 1 : 0;

        while ($continued && $i + 1 <= $#lines) {
            my $next_raw = $lines[$i + 1];
            my $is_omp_cont = ($next_raw =~ /^[ \t]*[!cC\*]\$(?:omp|ompx)\b/i);
            my $is_amp_cont = ($next_raw =~ /^[ \t]*&/);
            last unless ($is_omp_cont || $is_amp_cont);

            ++$i;
            my $next = $lines[$i];
            my $next_join_without_space =
                ($next =~ /[A-Za-z0-9_]&[ \t]*$/) ? 1 : 0;
            $continued = ($next =~ /&[ \t]*$/) ? 1 : 0;
            if ($is_omp_cont) {
                $next =~ s/^[ \t]*[!cC\*]\$(?:omp|ompx)\b[ \t]*&?[ \t]*//i;
            } else {
                $next =~ s/^[ \t]*&[ \t]*//;
            }
            $next =~ s/[ \t]*&[ \t]*$//;
            $next =~ s/[ \t]+!.*$//;
            $next = trim($next);
            if (length $next) {
                $directive .= ($join_without_space ? "" : " ") . $next;
            }
            $join_without_space = $next_join_without_space;
        }

        $directive = canonicalize_directive($directive);
        push @directives, $directive if length $directive;
    }
}

for my $directive (@directives) {
    print $directive, "\n";
}
PERL
