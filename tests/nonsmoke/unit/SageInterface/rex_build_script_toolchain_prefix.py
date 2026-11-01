#!/usr/bin/env python3
"""Exercise build-rex with a coherent toolchain under literal path characters."""

import json
import os
from pathlib import Path
import shutil
import subprocess
import sys
import tempfile


build_script, manifest, flang_check = map(Path, sys.argv[1:])
real_cmake = shutil.which("cmake")
assert real_cmake is not None


def executable(path, source):
    path.write_text(source)
    path.chmod(0o755)


with tempfile.TemporaryDirectory(prefix="rex-build-prefix-") as directory:
    root = Path(directory)
    for index, name in enumerate(("plain", "llvm toolchain", 'llvm"toolchain',
                                  "llvm\\toolchain")):
        prefix = root / name
        bindir = prefix / "bin"
        libdir = prefix / "lib"
        bindir.mkdir(parents=True)
        (libdir / "cmake/llvm").mkdir(parents=True)
        (libdir / "cmake/llvm/LLVMConfig.cmake").write_text("# fixture package\n")
        inventory = root / "inventory.txt"
        setup = root / "read-flang.cmake"
        setup.write_text('''include("${REX_FIXTURE_MANIFEST}")
file(WRITE "${REX_FIXTURE_INVENTORY}" "${REX_FLANG_FRONTEND_LIBRARIES}\n${REX_FLANG_FRONTEND_HEADERS}\n")
''')
        subprocess.run([real_cmake, f"-DREX_FIXTURE_MANIFEST={manifest.resolve()}",
                        f"-DREX_FIXTURE_INVENTORY={inventory}", "-P", str(setup)], check=True)
        libraries, headers = inventory.read_text().splitlines()
        for library in libraries.split(";"):
            (libdir / f"lib{library}.a").touch()
        for header in headers.split(";"):
            path = prefix / "include" / header
            path.parent.mkdir(parents=True, exist_ok=True)
            path.touch()
        executable(bindir / "llvm-config", '''#!/bin/bash
if [ -n "${REX_LLVM_PREFIX_STATE_FIXTURE-}" ] &&
   [ -f "$REX_LLVM_PREFIX_STATE_FIXTURE" ]; then
  REX_PREFIX_FIXTURE="$(cat "$REX_LLVM_PREFIX_STATE_FIXTURE")"
fi
case "$1" in
  --version) printf '%s\\n' "${REX_LLVM_VERSION_FIXTURE-23.1.8}" ;;
  --prefix) printf '%s\\n' "$REX_PREFIX_FIXTURE" ;;
  --bindir) printf '%s\\n' "$REX_PREFIX_FIXTURE/bin" ;;
  --libdir) printf '%s\\n' "$REX_PREFIX_FIXTURE/lib" ;;
  *) exit 2 ;;
esac
if [ "$1" = --libdir ] && [ -n "${REX_LLVM_NEXT_PREFIX_FIXTURE-}" ]; then
  printf '%s\\n' "$REX_LLVM_NEXT_PREFIX_FIXTURE" > "$REX_LLVM_PREFIX_STATE_FIXTURE"
fi
if [ "$1" = "${REX_LLVM_FAIL_QUERY_FIXTURE-}" ]; then
  exit 7
fi
''')
        for tool in ("clang", "clang++", "flang"):
            kind = "flang" if tool == "flang" else "clang"
            executable(bindir / tool,
                       f"#!/bin/bash\nprintf '%s\\n' '{kind} version 23.1.8'\n")
        for tool in ("llvm-ar", "llvm-ranlib"):
            executable(bindir / tool, "#!/bin/bash\nexit 0\n")
        executable(bindir / "cmake", '''#!/usr/bin/python3
import json, os, sys
if "-P" in sys.argv:
    os.execv(os.environ["REX_REAL_CMAKE"], [os.environ["REX_REAL_CMAKE"]] + sys.argv[1:])
with open(os.environ["REX_PREFIX_RECORD"], "a") as stream:
    stream.write(json.dumps(sys.argv[1:]) + "\\n")
''')
        work = root / f"project-{index}"
        work.mkdir()
        (work / "CMakeLists.txt").write_text("# fixture project\n")
        (work / "cmake").mkdir()
        shutil.copyfile(manifest, work / "cmake/rex_flang_frontend_manifest.cmake")
        shutil.copyfile(flang_check, work / "cmake/rex_check_flang_installation.cmake")
        retained_input = work / "build" / "retained-input"
        retained_input.parent.mkdir()
        retained_input.write_text("existing build input\n")
        record = work / "commands.jsonl"
        env = dict(os.environ)
        for key in ("LLVM_DIR", "LLVM_CONFIG", "LLVM_ROOT", "FLANG_ROOT",
                    "CC", "CXX", "FC", "LLVM_REQUIRED_MAJOR",
                    "REX_CMAKE_GENERATOR", "REX_LINKER",
                    "REX_LLVM_VERSION_FIXTURE", "REX_LLVM_FAIL_QUERY_FIXTURE",
                    "REX_LLVM_PREFIX_STATE_FIXTURE", "REX_LLVM_NEXT_PREFIX_FIXTURE"):
            env.pop(key, None)
        env.update(LLVM_ROOT=str(prefix), REX_PREFIX_FIXTURE=str(prefix),
                   REX_PREFIX_RECORD=str(record), REX_REAL_CMAKE=real_cmake, NUM_JOBS="2")
        result = subprocess.run(["bash", str(build_script), str(work / "install"),
                                 "Debug"], cwd=work, env=env, text=True,
                                stdout=subprocess.PIPE, stderr=subprocess.STDOUT)
        assert result.returncode == 0, result.stdout
        assert retained_input.read_text() == "existing build input\n"
        commands = [json.loads(line) for line in record.read_text().splitlines()]
        assert len(commands) == 3, commands
        for key, value in (("LLVM_ROOT", prefix), ("Clang_ROOT", prefix),
                           ("FLANG_ROOT", prefix),
                           ("CMAKE_C_COMPILER", bindir / "clang"),
                           ("CMAKE_CXX_COMPILER", bindir / "clang++"),
                           ("CMAKE_Fortran_COMPILER", bindir / "flang")):
            assert f"-D{key}={value}" in commands[0], commands
        assert commands[1] == ["--build", ".", "-j", "2"], commands
        assert commands[2] == ["--install", "."], commands

        if index == 0:
            invocation = ["bash", str(build_script), str(work / "install"), "Debug"]
            # Defaults must use this process's available CPUs, including when
            # Docker passes an empty build-argument override. Explicit
            # NUM_JOBS=2 is checked above with unrestricted CPU affinity.
            available_cpus = set(sorted(os.sched_getaffinity(0))[:2])
            for jobs_override in (None, ""):
                parallel_record = root / f"parallel-{jobs_override is None}.jsonl"
                parallel_env = dict(env, REX_PREFIX_RECORD=str(parallel_record))
                for key in ("NUM_JOBS", "OMP_NUM_THREADS", "OMP_THREAD_LIMIT"):
                    parallel_env.pop(key, None)
                if jobs_override is not None:
                    parallel_env["NUM_JOBS"] = jobs_override
                # A cgroup quota can allow fewer CPUs than the affinity mask.
                expected_jobs = subprocess.check_output(
                    ["nproc"], env=parallel_env, text=True,
                    preexec_fn=lambda: os.sched_setaffinity(0, available_cpus)).strip()
                accepted = subprocess.run(
                    invocation, cwd=work, env=parallel_env, text=True,
                    stdout=subprocess.PIPE, stderr=subprocess.STDOUT,
                    preexec_fn=lambda: os.sched_setaffinity(0, available_cpus))
                assert accepted.returncode == 0, accepted.stdout
                commands = [json.loads(line)
                            for line in parallel_record.read_text().splitlines()]
                assert commands[1] == ["--build", ".", "-j", expected_jobs], commands
            for version in ("23garbage", "23..1", "23.1.8\nprobe error", ""):
                rejected = subprocess.run(
                    invocation, cwd=work,
                    env=dict(env, REX_LLVM_VERSION_FIXTURE=version), text=True,
                    stdout=subprocess.PIPE, stderr=subprocess.STDOUT)
                assert rejected.returncode != 0 and "llvm-config" in rejected.stdout, rejected.stdout
                assert len(record.read_text().splitlines()) == 3
            for query in ("--version", "--bindir", "--prefix", "--libdir"):
                rejected = subprocess.run(
                    invocation, cwd=work,
                    env=dict(env, REX_LLVM_FAIL_QUERY_FIXTURE=query), text=True,
                    stdout=subprocess.PIPE, stderr=subprocess.STDOUT)
                assert rejected.returncode != 0 and "llvm-config query failed" in rejected.stdout, rejected.stdout
                assert len(record.read_text().splitlines()) == 3
            accepted = subprocess.run(
                invocation, cwd=work,
                env=dict(env, REX_LLVM_VERSION_FIXTURE="23.1.8git"), text=True,
                stdout=subprocess.PIPE, stderr=subprocess.STDOUT)
            assert accepted.returncode == 0, accepted.stdout
            assert len(record.read_text().splitlines()) == 6

            nested = bindir / "other-installation" / "clang"
            nested.parent.mkdir()
            external = root / "external-clang"
            for compiler in (nested, external):
                executable(compiler, "#!/bin/sh\nprintf '%s\\n' 'clang version 23.1.8'\n")
                rejected = subprocess.run(
                    invocation, cwd=work, env=dict(env, CC=str(compiler)), text=True,
                    stdout=subprocess.PIPE, stderr=subprocess.STDOUT)
                assert rejected.returncode != 0 and "outside the selected LLVM bindir" in rejected.stdout, rejected.stdout
                assert len(record.read_text().splitlines()) == 6
            directory = bindir / "compiler-directory"
            directory.mkdir()
            rejected = subprocess.run(
                invocation, cwd=work, env=dict(env, CC=str(directory)), text=True,
                stdout=subprocess.PIPE, stderr=subprocess.STDOUT)
            assert rejected.returncode != 0 and "compiler is not executable" in rejected.stdout, rejected.stdout
            assert len(record.read_text().splitlines()) == 6
            alias = bindir / "clang-alias"
            alias.symlink_to(bindir / "clang")
            accepted = subprocess.run(
                invocation, cwd=work, env=dict(env, CC=str(alias)), text=True,
                stdout=subprocess.PIPE, stderr=subprocess.STDOUT)
            assert accepted.returncode == 0, accepted.stdout
            commands = [json.loads(line) for line in record.read_text().splitlines()]
            assert len(commands) == 9
            assert f"-DCMAKE_C_COMPILER={alias}" in commands[6], commands[6]

            bare_record = root / "bare-compiler-names.jsonl"
            accepted = subprocess.run(
                invocation, cwd=work,
                env=dict(env, CC="clang", CXX="clang++", FC="flang",
                         REX_PREFIX_RECORD=str(bare_record)), text=True,
                stdout=subprocess.PIPE, stderr=subprocess.STDOUT)
            assert accepted.returncode == 0, accepted.stdout
            commands = [json.loads(line)
                        for line in bare_record.read_text().splitlines()]
            assert len(commands) == 3, commands
            for language, driver in (("C", "clang"), ("CXX", "clang++"),
                                     ("Fortran", "flang")):
                compiler_option = f"-DCMAKE_{language}_COMPILER={bindir / driver}"
                assert compiler_option in commands[0], commands[0]

            second_prefix = root / "second-toolchain"
            shutil.copytree(prefix, second_prefix)
            changed_record = root / "changed-toolchain.jsonl"
            accepted = subprocess.run(
                invocation, cwd=work,
                env=dict(env, REX_PREFIX_RECORD=str(changed_record),
                         REX_LLVM_PREFIX_STATE_FIXTURE=str(root / "prefix-state"),
                         REX_LLVM_NEXT_PREFIX_FIXTURE=str(second_prefix)), text=True,
                stdout=subprocess.PIPE, stderr=subprocess.STDOUT)
            assert accepted.returncode == 0, accepted.stdout
            commands = [json.loads(line) for line in changed_record.read_text().splitlines()]
            assert len(commands) == 3, commands
            assert f"-DLLVM_ROOT={prefix}" in commands[0], commands[0]
            assert f"-DCMAKE_C_COMPILER={bindir / 'clang'}" in commands[0], commands[0]

            versioned = bindir / "llvm-config-23"
            versioned.symlink_to(bindir / "llvm-config")
            older = root / f"older {versioned} compiler" / "bin"
            older.mkdir(parents=True)
            executable(older / "llvm-config", r'''#!/bin/bash
if [ "$1" = --version ]; then
  printf '%s\n' 22.1.8
else
  exec "$REX_PREFIX_FIXTURE/bin/llvm-config" "$@"
fi
''')
            search_record = root / "path-search.jsonl"
            search_env = dict(env, REX_PREFIX_RECORD=str(search_record),
                              PATH=os.pathsep.join((str(older), str(bindir),
                                                    env["PATH"])))
            search_env.pop("LLVM_ROOT")
            selected = subprocess.run(
                invocation, cwd=work, env=search_env, text=True,
                stdout=subprocess.PIPE, stderr=subprocess.STDOUT)
            assert selected.returncode == 0, selected.stdout
            commands = [json.loads(line)
                        for line in search_record.read_text().splitlines()]
            assert len(commands) == 3, commands
            assert f"-DLLVM_ROOT={prefix}" in commands[0], commands[0]

        # Preflight must use find_library semantics, not accept backup files.
        library = libdir / "libFortranSupport.a"
        command = [real_cmake, f"-DREX_FLANG_ROOT={prefix}", "-P", str(flang_check.resolve())]
        for suffix in (".a.disabled", ".dylib"):
            backup = library.with_suffix(suffix)
            library.rename(backup)
            rejected = subprocess.run(command, text=True, stdout=subprocess.PIPE,
                                      stderr=subprocess.STDOUT)
            assert rejected.returncode != 0 and "libFortranSupport" in rejected.stdout, rejected.stdout
            backup.rename(library)
        header = prefix / "include/flang/Parser/parsing.h"
        header.unlink()
        header.mkdir()
        rejected = subprocess.run(command, text=True, stdout=subprocess.PIPE,
                                  stderr=subprocess.STDOUT)
        assert rejected.returncode != 0 and "include/flang/Parser/parsing.h" in rejected.stdout, rejected.stdout
        header.rmdir()
        header.touch()
