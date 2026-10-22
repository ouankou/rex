import pathlib
import resource
import signal
import subprocess
import sys
import tempfile


def disable_core_dump():
    resource.setrlimit(resource.RLIMIT_CORE, (0, 0))


with tempfile.TemporaryDirectory(prefix="rex_missing_input_") as directory:
    source = pathlib.Path(directory) / "rex_sagebuilder_absent.cpp"
    result = subprocess.run(
        [sys.argv[1], str(source)],
        cwd=directory,
        stdout=subprocess.PIPE,
        stderr=subprocess.STDOUT,
        text=True,
        preexec_fn=disable_core_dump,
        check=False,
    )
    print(result.stdout, end="")
    marker = "REX_SAGEBUILDER_INVARIANT[file-input-unreadable]"
    if result.returncode != -signal.SIGABRT or result.stdout.count(marker) != 1:
        raise RuntimeError("missing input did not fail at the builder boundary")
    if source.exists():
        raise RuntimeError("builder created a dummy input instead of rejecting it")
