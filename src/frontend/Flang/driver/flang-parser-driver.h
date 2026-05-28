#ifndef ROSE_FLANG_PARSER_DRIVER_H_
#define ROSE_FLANG_PARSER_DRIVER_H_

class SgSourceFile;

int flang_parser_driver_main(int argc, char *const argv[], SgSourceFile *);
void flang_parser_driver_set_include_tmpdir(const char *path);

#endif /* ROSE_FLANG_PARSER_DRIVER_H_ */
