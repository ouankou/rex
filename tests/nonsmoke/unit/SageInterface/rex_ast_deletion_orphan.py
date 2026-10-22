import signal
import subprocess
import sys

for mode in ("orphan", "type-orphan"):
    result = subprocess.run(sys.argv[1:] + [mode], text=True,
                            stdout=subprocess.PIPE, stderr=subprocess.STDOUT)
    if result.returncode != -signal.SIGABRT or \
            "REX_AST_INVARIANT[isolated-delete-proof]" not in result.stdout:
        raise RuntimeError(
            f"{mode} was not rejected at deletion: {result.returncode}\n{result.stdout}")
