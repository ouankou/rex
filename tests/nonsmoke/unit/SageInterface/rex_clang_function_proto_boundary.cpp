/* EXT-CLANG-FUNCTION-PROTO-COMPARE: standalone diagnostic, not a REX test
 * target. See https://github.com/ouankou/rex/issues/885 for controls and
 * interpretation.
 */
#include <clang/AST/ASTContext.h>
#include <clang/Tooling/Tooling.h>
#include <new>
#include <valgrind/memcheck.h>
int main(int argc, char **) {
  auto ast = clang::tooling::buildASTFromCode("");
  if (!ast)
    return 2;
  auto &context = ast->getASTContext();
  using Info = clang::FunctionProtoType::ExtProtoInfo;
  alignas(Info) unsigned char storage[sizeof(Info)]{};
  auto &info = *new (storage) Info;
  if (argc > 1)
    VALGRIND_MAKE_MEM_UNDEFINED(&info.ExtParameterInfos,
                                sizeof info.ExtParameterInfos);
  auto type = context.getFunctionType(context.IntTy, {}, info);
  info.~Info();
  return type.isNull();
}
