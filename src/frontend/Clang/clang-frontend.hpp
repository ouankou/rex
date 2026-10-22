
#ifndef _CLANG_FRONTEND_HPP_
#define _CLANG_FRONTEND_HPP_

class SgSourceFile;

int clang_main(int argc, char *argv[], SgSourceFile &sageFile,
               const char *driver_argv0);

#endif /* _CLANG_FRONTEND_HPP_ */
