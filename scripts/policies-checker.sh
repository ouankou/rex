#!/bin/bash
#
# This shell script is the harness for running a set of policy-enforcing scripts.  This harness can be run from any location
# in the ROSE source tree at any time during development. We enforce a set of policies on ROSE developers.  Each policy is
# enforced by a single script in the $ROSESRC/scripts/policies directory.
#

# Turn a CamelCase file name into something more friendly
demangle () {
    local s="$1"
    s="${s##*/}" # strip of directory part if any
    s="${s%%.*}" # strip of extensions
    s="$(printf '%s\n' "$s" |sed 's/\([A-Z]\)/ \1/g')" # de-CamelCase
    s="${s# }" # remove leading space from de-CamelCase if necessary
    # EXT-COREUTILS-TR-SETS: use Bash's ASCII conversion directly; GNU tr
    # reports terminal allocations (docs/codebase-audit/external-exceptions.md).
    local LC_ALL=C
    s="${s,,}"
    printf '%s\n' "$s"
}

# EXT-BASH-TERMINAL-COMMAND: return through Bash's normal function cleanup
# before the process exits (docs/codebase-audit/external-exceptions.md).
check_policies () {
    # Find the directory containing the scripts that enforce individual policies
    local mydir="${0%/*}"
    local arg0="${0#$mydir/}"
    local policies_dir="$mydir/policies"
    if [[ ! -d "$policies_dir" ]]; then
        echo "$arg0: cannot find policies directory: $policies_dir" >&2
        return 1
    fi

    # Run each executable file and keep track of exit status:
    #     0         success
    #     128       documented policy warning
    #     all other nonzero statuses: failure (including Perl exceptions/signals)
    local nsuccesses=0
    local nwarnings=0
    local script name status
    local -a command_status
    for script in "$policies_dir"/*; do
        [ "$script" != "${script%'~'}" ] && continue        # editor temp file
        [ "$script" != "${script%.disabled}" ] && continue  # script is disabled
        [ -x "$script" ] || continue
        name="$(demangle "$script")"
        echo "  POLICY  Checking $name policy ($script)"
        "$script" 2>&1 |sed 's/^/    /'
        command_status=("${PIPESTATUS[@]}")
        status=${command_status[0]}
        if [[ ${command_status[1]} != 0 ]]; then
            echo "Cannot capture policy output for $name." >&2
            return 1
        fi
        if [[ $status = 0 ]]; then
            nsuccesses=$((nsuccesses + 1))
        elif [[ $status != 128 ]]; then
            echo "Errors detected by the $name policy. Aborting now."
            return 1
        else
            nwarnings=$((nwarnings + 1))
        fi
        echo
    done

    # Report a completed policy run
    echo "Policies checked:  success=$nsuccesses; warnings=$nwarnings; errors=0"
    return 0
}

check_policies
