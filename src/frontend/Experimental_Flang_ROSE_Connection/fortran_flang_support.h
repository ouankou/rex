#ifndef ROSE_FORTRAN_FLANG_SUPPORT_
#define ROSE_FORTRAN_FLANG_SUPPORT_

#include <string>

// It would be nice to get rid of this global hack (but the proverbial later)
// I think you should be able to use the argument passed to
// experimental_fortran_main
int experimental_fortran_main(int argc, char *argv[],
                              SgSourceFile *sg_source_file);
void set_flang_include_temp_dir(const std::string &path);

#endif /* ROSE_FORTRAN_FLANG_SUPPORT_ */
