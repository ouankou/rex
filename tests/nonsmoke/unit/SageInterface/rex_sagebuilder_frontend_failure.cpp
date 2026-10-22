#include "rose.h"

int main(int argc, char **argv) {
  if (argc != 2)
    return 2;

  // Missing inputs must fail before construction. For malformed Fortran,
  // Flang returns a diagnostic status that must fail before postprocessing.
  SageBuilder::buildFile(argv[1], "");
  return 0;
}
