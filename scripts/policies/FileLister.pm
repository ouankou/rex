package FileLister;
use strict;
use Carp;
use Cwd 'abs_path';
use File::Spec;

sub source_toplevel {
  my $dir = abs_path(".");
  while (defined $dir) {
    return $dir if -f "$dir/.gitmodules";
    my($parent) = $dir =~ m{^(.*)/[^/]+$};
    last unless defined $parent && length($parent) && $parent ne $dir;
    $dir = $parent;
  }
  return undef;
}

sub submodule_directories {
  my($source_dir) = @_;
  my %retval;
  return \%retval unless defined $source_dir;

  my $gitmodules = "$source_dir/.gitmodules";
  open GITMODULES, "<", $gitmodules
      or croak "cannot read $gitmodules: $!";
  local($_);
  local $!;
  while (1) {
    $! = 0;
    $_ = <GITMODULES>;
    croak "cannot read $gitmodules: $!" if $!;
    last unless defined $_;
    next unless /^\s*path\s*=\s*(.*?)\s*$/;
    my $path = $1;
    croak "$gitmodules contains an invalid absolute submodule path: $path"
        if $path =~ m{^/};
    croak "$gitmodules contains an invalid parent-traversing submodule path: $path"
        if grep {$_ eq ".."} split m{/+}, $path;
    my $absolute = abs_path("$source_dir/$path");
    croak "$gitmodules names a missing submodule path: $path"
        unless defined $absolute && -d $absolute;
    croak "$gitmodules repeats submodule path: $path"
        if $retval{$absolute};
    $retval{$absolute} = 1;
  }
  close GITMODULES or croak "cannot close $gitmodules: $!";
  return \%retval;
}

sub new {
  my($cls,@root) = @_;
  my $self = bless {build=>0,               # include build subtrees identified by CMake build markers?
		    thirdparty=>0,          # software not "owned by" ROSE developers
		    generated=>0,           # software that is machine-generated
		    install=>0,             # files in the ROSE install directory
		    recursive=>1,           # recurse into directories; -1=never; 0=only into specified dirs; 1=always
		    pending=>\@root,        # files and directories that are pending
		    gitdir=>git_toplevel(), # absolute name of top-level Git directory if we're inside a Git repo
		    gitfiles=>{},             # current tracked/unignored source paths
		    submodules=>{},          # exact separately-owned source trees from .gitmodules
		   }, $cls;

  $self->{gitfiles} = git_files($self->{gitdir}) if $self->{gitdir};
  my $source_dir = source_toplevel();
  if (!$self->{gitdir} ||
      (defined $source_dir && abs_path($self->{gitdir}) eq $source_dir)) {
    $self->{submodules} = submodule_directories($source_dir);
  }
  
  # Parse switches (usually directly from the command-line)
  while (@root && $root[0]=~/^--?([_a-zA-Z]\w*)(=(.*))?/) {
    my($var,$val) = ($1,$3);
    $val = 1 unless defined $val;
    croak "unknown switch: --$var" unless exists $self->{$var};
    $self->{$var} = $val;
    shift @root;
  }
  shift @root if @root && $root[0] eq "--";

  push @root, "." unless @root;

  # Recurse into directories that are specified on the command-line, but no further.
  if (0==$self->{recursive}) {
    my @expanded;
    for my $dir (@root) {
      push @expanded, $dir;
      if (-d $dir && ! -l $dir) {
        opendir my $entries, $dir or croak "cannot inventory $dir: $!";
        local $! = 0;
        my @entries = readdir $entries;
        croak "cannot inventory $dir: $!" if $!;
        push @expanded, map {"$dir/$_"}
            grep {$_ ne "." && $_ ne ".."} sort @entries;
        closedir $entries or croak "cannot close $dir: $!";
      }
    }
    @root = @expanded;
  }

  $self->{pending} = \@root;
  return $self;
}

# Select worktree or source-archive mode from actual metadata. Do not mistake
# a frozen source archive nested inside another checkout for that checkout.
sub git_toplevel {
  my $dir = abs_path(".");
  while (defined $dir) {
    return $dir if -e "$dir/.git";
    return undef if -f "$dir/.gitmodules";
    my($parent) = $dir =~ m{^(.*)/[^/]+$};
    last unless defined $parent && length($parent) && $parent ne $dir;
    $dir = $parent;
  }
  return undef;
}

# HEAD omits unstaged/new files and may not exist yet. Git supplies the current
# tracked and unignored source set; errors cannot mean an empty source tree.
sub git_files {
  my($root) = @_;
  my %retval;
  open my $git, "-|", "git", "-C", $root,
      "ls-files", "--cached", "--others", "--exclude-standard", "-z"
      or croak "cannot start Git source inventory: $!";
  {
    local $/ = "\0";
    local $! = 0;
    while (my $path = <$git>) {
      chomp $path;
      $retval{$path} = 1;
      while ($path =~ s{/[^/]+$}{}) { $retval{$path} = 1; }
    }
    croak "cannot read Git source inventory: $!" if $!;
  }
  unless (close $git) {
    my $status = $?;
    $? = 0; # Git status 128 is reserved for warnings by the policy protocol.
    croak "Git source inventory failed (status $status)";
  }
  return \%retval;
}


# Return true if the given file or directory (with path) is third-party software.
sub is_3rdparty {
  local($_) = @_;
  return 1 if m(/3rdPartyLibraries(/|$));
  return 0;
}

# Return true if the given file or directory (with path) is machine-generated code.
sub is_generated {
  my $self = shift;
  local($_) = @_;
  if (my $gitdir = $self->{gitdir}) {
    my $absolute = File::Spec->canonpath(File::Spec->rel2abs($_));
    return 0 if $absolute eq $gitdir;
    my($filename) = $absolute =~ m{^\Q$gitdir\E/(.*)};
    croak "source path $_ is outside $gitdir" unless defined $filename;
    return 1 unless exists $self->{gitfiles}{$filename};
  }

  return 1 if m(/\.#); # Emacs temporary file
  return 1 if m(~$); # Emacs backup file
  return 1 if m(/\.git(?:/|$));
  return 0;
}

# Return true if the name is an exact submodule directory or one of its
# descendants.  A Git checkout discovers this ownership boundary from its
# gitlink.  Source archives and Docker build contexts have no .git directory,
# so retain the same boundary from the checked-in .gitmodules contract.
sub is_submodule_content {
  my($self, $name) = @_;
  my $absolute = abs_path($name);
  return 0 unless defined $absolute;
  for my $submodule (keys %{$self->{submodules}}) {
    return 1 if $absolute eq $submodule || $absolute =~ m{^\Q$submodule\E/};
  }
  return 0;
}

# Return true if the given directory (with path) is the ROSE install directory.
sub is_install {
  return 0 unless -d $_[0] && $ENV{prefix} ne "";
  local($_)  = abs_path(@_);
  my $prefix = abs_path($ENV{'prefix'});
  return 1 if $prefix eq $_;
  return 0;
}

# Return the next name, or undef when we reach the end.
sub next_name {
  my($self) = @_;
  my $retval = shift @{$self->{pending}};
  return $retval unless defined $retval;
  if ((!$self->{build}      && (-f "$retval/CMakeCache.txt" || -d "$retval/CMakeFiles")) ||
      (!$self->{thirdparty} && is_3rdparty($retval)) ||
      $self->is_submodule_content($retval) ||
      (!$self->{generated}  && $self->is_generated($retval)) ||
      (!$self->{install}    && is_install($retval)) ||
      ($retval =~ /\/\.(git|svn)$/ && -d $retval) || # no need to ever descend into Git or Subversion meta data
      ($retval =~ /~$/      && ! -d $retval) ||      # editor backup files
      ($retval =~ /\/\.#/)                           # emacs autosave files
     ) {
    return $self->next_name; # skip this name, return the next one
  } elsif ($self->{recursive}>0 && -d $retval && ! -l $retval) {
    opendir my $dir, $retval or croak "cannot inventory $retval: $!";
    local $! = 0;
    my @entries = readdir $dir;
    croak "cannot inventory $retval: $!" if $!;
    closedir $dir or croak "cannot close $retval: $!";
    unshift @{$self->{pending}}, map {"$retval/$_"}
        grep {$_ ne "." && $_ ne ".."} sort @entries;
  }
  return $retval;
}

# Return next name, skipping over directories
sub next_file {
  my($self) = @_;
  while (1) {
    my $retval = $self->next_name;
    return $retval unless defined $retval && -d $retval;
  }
}

# Return the next directory, skipping over non-directories
sub next_dir {
  my($self) = @_;
  while (1) {
    my $retval = $self->next_name;
    return unless defined $retval;
    return $retval if -d $retval;
  }
}

sub all {
  my($self,$code) = @_;
  my(@retval,$entry);
  push @retval, $entry while defined($entry=&{$code});
  return @retval;
}

# Returns all names
sub all_names {
  my($self) = @_;
  return $self->all(sub {$self->next_name});
}

# Return all files (non-directories)
sub all_files {
  my($self) = @_;
  return $self->all(sub {$self->next_file});
}

# Return all directories
sub all_dirs {
  my($self) = @_;
  return $self->all(sub {$self->next_dir});
}

1;
