"""An analysis must reject a declaration whose symbol was never published."""
import subprocess
import sys

result = subprocess.run([sys.argv[1], "--missing-symbol"], text=True,
                        stdout=subprocess.PIPE, stderr=subprocess.STDOUT)
if result.returncode == 0 or "REX_AST_INVARIANT[outliner-variable-symbol]" not in result.stdout:
    raise RuntimeError(f"missing symbol was not rejected: {result.returncode}\n{result.stdout}")
