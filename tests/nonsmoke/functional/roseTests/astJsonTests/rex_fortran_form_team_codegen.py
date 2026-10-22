import os
from pathlib import Path
import subprocess
import sys
import tempfile

driver, compiler, source = map(Path, sys.argv[1:])
with tempfile.TemporaryDirectory(prefix="rex-form-team-") as directory:
    root = Path(directory)
    subprocess.run([str(compiler), "-fcoarray", "-c", str(source),
                    "-o", str(root / "original.o")], cwd=root, check=True)
    for iteration in range(2):
        work = root / str(iteration)
        work.mkdir()
        environment = dict(os.environ, ROSE_TEST_OUTPUT_DIR=str(work),
                           REX_AST_JSON_DIR=str(work / "json"))
        subprocess.run([str(driver), "-rose:skipfinalCompileStep", "-c",
                        str(source)], cwd=work, env=environment, check=True)
        assert list((work / "json").glob("*.json")), "checkpoint did not run"
        source = work / ("rose_" + source.name)
        assert source.is_file()
        subprocess.run([str(compiler), "-fcoarray", "-c", str(source),
                        "-o", str(work / "generated.o")], cwd=work, check=True)
