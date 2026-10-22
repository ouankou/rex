#ifndef _CLANG_TO_DOT_FRONTEND_HPP_
#define _CLANG_TO_DOT_FRONTEND_HPP_

#include <string>

namespace clang {
class CompilerInstance;
}

// Render the same completed, diagnostic-free AST used by the Sage frontend.
void writeClangAstGraph(clang::CompilerInstance &compiler,
                        const std::string &filename);

#endif
