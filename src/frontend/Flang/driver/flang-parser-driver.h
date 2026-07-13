#ifndef ROSE_FLANG_PARSER_DRIVER_H_
#define ROSE_FLANG_PARSER_DRIVER_H_

#include <string>

class SgSourceFile;

int flang_parser_driver_main(int argc, char *const argv[], SgSourceFile *,
                             const std::string &includeTempDir);

#endif /* ROSE_FLANG_PARSER_DRIVER_H_ */
