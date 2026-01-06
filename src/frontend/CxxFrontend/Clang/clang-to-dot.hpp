
#ifndef _CLANG_TO_DOT_FRONTEND_HPP_
# define _CLANG_TO_DOT_FRONTEND_HPP_

class SgSourceFile;

// int clang_to_dot_main(int argc, char* argv[], SgSourceFile& sageFile);
int clang_to_dot_main(int argc, char *argv[], const char *driver_argv0);

#endif /* _CLANG_TO_DOT_FRONTEND_HPP_ */
