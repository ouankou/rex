import pathlib
import subprocess
import sys
import tempfile


def run(command, cwd):
    subprocess.run(command, cwd=cwd, check=True)


probe, flang, clang, specimen = sys.argv[1:]
with tempfile.TemporaryDirectory(prefix="rex_binding_") as temporary:
    root = pathlib.Path(temporary)
    generated = root / "rex_binding_generated.f90"
    run([probe, "-rose:skipfinalCompileStep", "-rose:output", str(generated),
         "-c", specimen], root)
    # Reparse the emitted Fortran as well as the JSON-restored AST. In particular,
    # losing an empty NAME changes the external identity while remaining valid
    # Fortran, so recompilation by itself cannot establish the binding contract.
    reparsed = root / "rex_binding_reparsed.f90"
    run([probe, "-rose:skipfinalCompileStep", "-rose:output", str(reparsed),
         "-c", str(generated)], root)
    driver = root / "rex_binding_driver.c"
    driver.write_text("""extern int rex_bind_default(int);
extern int rex_bound_Name(int);
extern int rex_bind_host(int);
extern int rex_bind_reference(int *);
int main(void) {
  int value = 7;
  return rex_bind_default(7) == 8 && rex_bound_Name(7) == 9 &&
         rex_bind_host(7) == 33 && rex_bind_reference(&value) == 13 ? 0 : 1;
}
""")
    run([clang, "-c", str(driver), "-o", "driver.o"], root)
    for index, source in enumerate([specimen, str(generated), str(reparsed)]):
        executable = root / f"binding_{index}"
        run([flang, source, "driver.o", "-o", str(executable)], root)
        run([str(executable)], root)
