#!/usr/bin/perl
# DO NOT DISABLE without first checking with a ROSE core developer
my $desc = <<EOF;
ROSE developers predominantly use Unix-like systems for development. These
systems use the ASCII LF (line feed, '\\n') character for line termination.
Some other operating systems insert a CR (carriage return, '\\r') in front of
the LF.  Insertion of the extra CR causes problems for revision management
software and some Unix-based text editors which developers then have to work
around and/or correct.  To convert a file from any mixture of CR+LF or
LF-only line endings to LF-only line endings, use:
  * the unix command "dos2unix"
  * sed -i 's/\r$//' <filename>
  * vim 7.0 and later commands ':update', ':setlocal ff=unix'.
  * in XEmacs use the "M-x set-buffer-file-coding-system RET" and
    enter "undecided-unix".
The following source files have CR+LF line termination:
EOF
BEGIN {push @INC, $1 if $0 =~ /(.*)\//}

use strict;
use FileLister;
use Policies;

my $nfail=0;
my $files = FileLister->new();
while (my $filename = $files->next_file) {
  next if $filename =~ /\.(pdf|rtf|vimsession)$/;
  next if is_disabled($filename);

  my $is_text = -T $filename;
  die "$filename: cannot classify: $!\n" unless defined $is_text;
  if ($is_text) {
    open my $source, "<", $filename or die "$filename: cannot open: $!\n";
    my ($n_lines, $n_crlf) = (0, 0);
    local $! = 0;
    while (<$source>) {
      ++$n_lines;
      ++$n_crlf if /\r\n/;
    }
    die "$filename: cannot read: $!\n" if $!;
    close $source or die "$filename: cannot close: $!\n";
    if ($n_crlf) {
      print $desc unless $nfail++;
      printf "  %6d (%4d%%) lines in %1s\n", $n_crlf, 100*($n_crlf/$n_lines), $filename;
    }
  }
}
exit($nfail>0 ? 1 : 0);
