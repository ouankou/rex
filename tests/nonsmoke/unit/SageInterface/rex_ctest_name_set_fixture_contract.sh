#!/usr/bin/env bash

set -euo pipefail

runner=$1
cmake=$2
ctest=$3
source_dir=$4
binary_dir=$5

source_dir=$(realpath "$source_dir")
binary_dir=$(realpath -m "$binary_dir")

rm -rf "$binary_dir"
"$cmake" -S "$source_dir" -B "$binary_dir"
output=$(python3 "$runner" \
  --test-dir "$binary_dir" \
  --manifest "$source_dir/manifest.txt" \
  --include-regex '^rex_regex_only$' \
  --ctest "$ctest" \
  --jobs 1 \
  --dry-run)

expected="Selected 5 CTest tests"
if [ "$output" != "$expected" ]; then
  echo "unexpected manifest/regex selection result: $output" >&2
  exit 1
fi

"$cmake" -E echo rex_missing >"$binary_dir/missing-manifest.txt"
if python3 "$runner" \
  --test-dir "$binary_dir" \
  --manifest "$binary_dir/missing-manifest.txt" \
  --ctest "$ctest" \
  --jobs 1 \
  --dry-run >"$binary_dir/missing-manifest.log" 2>&1; then
  echo "missing manifest test was accepted" >&2
  exit 1
fi
grep -Fq 'Manifest tests are not registered:' "$binary_dir/missing-manifest.log"

output=$(python3 "$runner" \
  --test-dir "$binary_dir" \
  --include-regex '^rex_memcheck_(ignored|selected)$' \
  --shard-index 1 \
  --shard-count 1 \
  --ctest "$ctest" \
  --jobs 1 \
  --memcheck \
  --dry-run)

expected="Selected 1 CTest tests"
if [ "$output" != "$expected" ]; then
  echo "unexpected MemCheck-registry selection result: $output" >&2
  exit 1
fi

# A later inventory must preserve even an interrupted dashboard's partial
# evidence. CTest's show-only Memcheck mode otherwise overwrites this report.
python3 - "$runner" "$ctest" "$binary_dir" <<'PY'
from pathlib import Path
import subprocess
import sys

runner, ctest, directory = sys.argv[1:]
testing = Path(directory) / "Testing"
tag = (testing / "TAG").read_bytes()
report = testing / tag.decode().splitlines()[0] / "DynamicAnalysis.xml"
evidence = b"retained interrupted Memcheck evidence\n"
report.write_bytes(evidence)
result = subprocess.run([
    sys.executable, runner, "--test-dir", directory, "--ctest", ctest,
    "--jobs", "1", "--memcheck", "--dry-run",
    "--include-regex", "^rex_memcheck_selected$",
], capture_output=True, text=True)
(testing / "rejected-inventory.log").write_text(result.stdout + result.stderr)
assert result.returncode != 0, "Memcheck inventory reused an existing dashboard"
assert "existing evidence" in result.stderr, result.stderr
assert (testing / "TAG").read_bytes() == tag
assert report.read_bytes() == evidence, "Memcheck inventory overwrote evidence"
PY

output=$(python3 "$runner" \
  --test-dir "$binary_dir" \
  --include-regex '^rex_dependency_(producer|consumer)$' \
  --shard-index 2 \
  --shard-count 3 \
  --ctest "$ctest" \
  --jobs 1 \
  --dry-run)

expected="Selected 2 CTest tests"
if [ "$output" != "$expected" ]; then
  echo "unexpected dependency-closed shard result: $output" >&2
  exit 1
fi

# The stable name hash assigns the producer to shard 3.  A raw CTest index
# residue would leave this shard empty because the filtered registry has only
# two direct tests, so this also guards against registration-order sharding.
output=$(python3 "$runner" \
  --test-dir "$binary_dir" \
  --include-regex '^rex_dependency_(producer|consumer)$' \
  --shard-index 3 \
  --shard-count 3 \
  --ctest "$ctest" \
  --jobs 1 \
  --dry-run)

expected="Selected 1 CTest tests"
if [ "$output" != "$expected" ]; then
  echo "unexpected stable name-hash shard result: $output" >&2
  exit 1
fi

python3 "$runner" \
  --test-dir "$binary_dir" \
  --include-regex '^rex_dependency_(producer|consumer)$' \
  --shard-index 2 \
  --shard-count 3 \
  --ctest "$ctest" \
  --jobs 1

python3 - "$runner" "$binary_dir" <<'PY'
from pathlib import Path
import json
import os
import re
import runpy
import subprocess
import sys

runner, directory = sys.argv[1:]
available_cpus = set(sorted(os.sched_getaffinity(0))[:2])
for level, explicit_jobs, expected in (
    (None, None, None),
    ("3", None, 3),
    ("3", "1", 1),
):
    env = dict(os.environ)
    for key in ("CTEST_PARALLEL_LEVEL", "OMP_NUM_THREADS", "OMP_THREAD_LIMIT"):
        env.pop(key, None)
    if level is not None:
        env["CTEST_PARALLEL_LEVEL"] = level
    if expected is None:
        expected = int(subprocess.check_output(
            ["nproc"], env=env, text=True,
            preexec_fn=lambda: os.sched_setaffinity(0, available_cpus)))
    command = [sys.executable, runner, "--test-dir", directory,
               "--include-regex", "^rex_regex_only$"]
    if explicit_jobs is not None:
        command.extend(["--jobs", explicit_jobs])
    result = subprocess.run(
        command, env=env, capture_output=True, text=True,
        preexec_fn=lambda: os.sched_setaffinity(0, available_cpus))
    assert result.returncode == 0, result.stdout + result.stderr
    run_dir = re.search(r"^CTest selection and results: (.+)$",
                        result.stdout, re.MULTILINE)
    assert run_dir is not None, result.stdout
    recorded = json.loads((Path(run_dir[1]) / "command.json").read_text())
    assert recorded[-1] == f"-j{expected}", recorded

verify = runpy.run_path(sys.argv[1])["verify_results"]
report = Path(sys.argv[2]) / "malformed-results.xml"
registry = {"tests": [{"name": "producer"}, {"name": "consumer"}]}
report.write_text('<testsuite><testcase name="producer" status="run"/></testsuite>')
assert "Missing result: consumer" in verify(registry, [report])
report.write_text(
    '<testsuite failures="1"><testcase name="producer" status="run"/>'
    '<testcase name="consumer" status="run"/></testsuite>')
assert any("Unsuccessful JUnit suite" in problem for problem in verify(registry, [report]))
report.write_text(
    '<testsuite><testcase name="producer" status="run"/>'
    '<testcase name="consumer" status="skipped"/></testsuite>')
assert any("Unsuccessful test" in problem for problem in verify(registry, [report]))
PY
