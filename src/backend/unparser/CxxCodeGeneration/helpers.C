
#include "unparser.h"

#include "sage3basic.h"

#include "rose_config.h"

#include <cctype>

static std::string validate_asm_name(const std::string &name,
                                     const char *context) {
  if (name.empty()) {
    fprintf(stderr, "REX_UNPARSE_INVARIANT[%s]: empty asm name\n", context);
    ROSE_ABORT();
  }

  for (unsigned char ch : name) {
    if (ch == '"' || ch == '\\' || std::iscntrl(ch)) {
      fprintf(stderr,
              "REX_UNPARSE_INVARIANT[%s]: invalid character in asm name\n",
              context);
      ROSE_ABORT();
    }
  }

  return name;
}

std::string unparse_asm_clobber_name(const std::string &register_name) {
  return validate_asm_name(register_name, "asm-clobber");
}

std::string unparse_asm_label_name(const std::string &label_name) {
  return validate_asm_name(label_name, "asm-label");
}

bool isNonFriendMemberFunctionDeclaration(const SgFunctionDeclaration *decl) {
  const SgMemberFunctionDeclaration *member_decl =
      isSgMemberFunctionDeclaration(const_cast<SgFunctionDeclaration *>(decl));
  return member_decl != nullptr &&
         !member_decl->get_declarationModifier().isFriend();
}
