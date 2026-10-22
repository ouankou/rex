#!/usr/bin/perl
use strict;
use warnings;
use Errno qw(ENOENT ENOTDIR);

# Inspect the working source, including new files. Source archives have no Git
# metadata and use an explicit filesystem inventory. A failed Git inventory is
# an error, never evidence that there are no conflicting names (AUD-171).
my %index;

sub record_path {
  my ($path) = @_;
  my $dir = ".";
  for my $entry (split m{/}, $path) {
    $index{$dir}{lc $entry}{$entry} = 1;
    $dir .= "/$entry";
  }
}

sub check_directory;

sub check_git_worktree {
  my ($root, $prefix) = @_;
  open my $git, "-|", "git", "-C", $root,
      "ls-files", "--cached", "--others", "--exclude-standard", "-z"
      or die "cannot start Git source inventory for $root: $!\n";
  my @paths;
  {
    local $/ = "\0";
    local $! = 0;
    while (my $path = <$git>) {
      chomp $path;
      push @paths, $path;
    }
    die "cannot read Git source inventory for $root: $!\n" if $!;
  }
  close $git or die "Git source inventory failed for $root (status $?)\n";
  for my $path (@paths) {
    my $absolute = "$root/$path";
    unless (lstat $absolute) {
      # A tracked file can be deleted in the working tree before being staged.
      next if $! == ENOENT || $! == ENOTDIR;
      die "cannot inspect $absolute: $!\n";
    }
    my $relative = "$prefix$path";
    record_path($relative);
    check_directory($absolute, "$relative/") if -d _ && ! -l $absolute;
  }
}

sub check_directory {
  my ($root, $prefix) = @_;
  if (-e "$root/.git") {
    check_git_worktree($root, $prefix);
    return;
  }
  # Exclude actual CMake build trees, not source names resembling build paths.
  return if -f "$root/CMakeCache.txt" || -d "$root/CMakeFiles";
  opendir my $dir, $root or die "cannot inventory directory $root: $!\n";
  local $! = 0;
  my @entries = readdir $dir;
  die "cannot inventory directory $root: $!\n" if $!;
  closedir $dir or die "cannot close directory $root: $!\n";
  for my $entry (@entries) {
    next if $entry eq "." || $entry eq ".." || $entry eq ".git";
    my $absolute = "$root/$entry";
    lstat $absolute or die "cannot inspect $absolute: $!\n";
    my $relative = "$prefix$entry";
    record_path($relative);
    check_directory($absolute, "$relative/") if -d _ && ! -l $absolute;
  }
}

unless (eval { check_directory(".", ""); 1 }) {
  print STDERR $@;
  # The policy harness reserves statuses >= 128 for warnings. Inventory errors
  # must be fatal even when Git itself returns 128 or the process cannot start.
  exit 2;
}
my $failed = 0;
for my $dir (sort keys %index) {
  for my $key (sort keys %{$index{$dir}}) {
    my @names = sort keys %{$index{$dir}{$key}};
    next unless @names > 1;
    print "Case-insensitive source filename conflicts:\n" unless $failed++;
    print "  $dir (", join(" ", @names), ")\n";
  }
}
exit($failed ? 1 : 0);
