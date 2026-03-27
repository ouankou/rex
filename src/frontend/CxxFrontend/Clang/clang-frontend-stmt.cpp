#include "clang-frontend-private.hpp"
#include "clang-nns-utils.hpp"

#include "clang-to-rose-support.hpp"

#include "rex_coroutine_attributes.h"
#include "sage3basic.h"

#include <algorithm>

#include <array>

#include <cctype>

#include <functional>

#include <iomanip>

#include <limits>

#include <regex>

#include <sstream>

#include <set>

#include <utility>

#include <unordered_set>

#include <vector>

#include <clang/Basic/CharInfo.h>

#include <clang/Basic/OperatorKinds.h>

#include "markLhsValues.h"
#include "sageInterface.h"

static void suppress_unparse_output(SgLocatedNode *n) {
  if (n == nullptr) {
    return;
  }
  if (Sg_File_Info *fi = n->get_file_info()) {
    fi->unsetOutputInCodeGeneration();
  }
  if (Sg_File_Info *fi = n->get_startOfConstruct()) {
    fi->unsetOutputInCodeGeneration();
  }
  if (Sg_File_Info *fi = n->get_endOfConstruct()) {
    fi->unsetOutputInCodeGeneration();
  }
}

#include <clang/AST/APValue.h>

#include <clang/AST/LambdaCapture.h>

#include <clang/Basic/TypeTraits.h>

#include <clang/Lex/Lexer.h>

#include <clang/Lex/LiteralSupport.h>
using llvm::isa; // For LLVM type checking (isa<Type>)

namespace {
static bool
type_has_elaborated_spelling_for_type_operand(const clang::Type *type) {
  if (type == nullptr) {
    return false;
  }

  if (const auto *tag_type = llvm::dyn_cast<clang::TagType>(type)) {
    return tag_type->getKeyword() != clang::ElaboratedTypeKeyword::None;
  }
  if (const auto *typedef_type = llvm::dyn_cast<clang::TypedefType>(type)) {
    return typedef_type->getKeyword() != clang::ElaboratedTypeKeyword::None;
  }
  if (const auto *using_type = llvm::dyn_cast<clang::UsingType>(type)) {
    return using_type->getKeyword() != clang::ElaboratedTypeKeyword::None;
  }
  if (const auto *unresolved_using =
          llvm::dyn_cast<clang::UnresolvedUsingType>(type)) {
    return unresolved_using->getKeyword() != clang::ElaboratedTypeKeyword::None;
  }
  if (const auto *dependent_name =
          llvm::dyn_cast<clang::DependentNameType>(type)) {
    return dependent_name->getKeyword() != clang::ElaboratedTypeKeyword::None;
  }
  if (const auto *template_specialization =
          llvm::dyn_cast<clang::TemplateSpecializationType>(type)) {
    return template_specialization->getKeyword() !=
           clang::ElaboratedTypeKeyword::None;
  }
  if (const auto *deduced_template_specialization =
          llvm::dyn_cast<clang::DeducedTemplateSpecializationType>(type)) {
    return deduced_template_specialization->getKeyword() !=
           clang::ElaboratedTypeKeyword::None;
  }
  if (const auto *injected_class_name =
          llvm::dyn_cast<clang::InjectedClassNameType>(type)) {
    return injected_class_name->getKeyword() !=
           clang::ElaboratedTypeKeyword::None;
  }

  return qualifiedTypeHasQualifier(type);
}

static bool qual_type_contains_elaborated_spelling_for_type_operand(
    clang::QualType qual_type) {
  const clang::Type *type = qual_type.getTypePtrOrNull();
  while (type != nullptr) {
    if (type_has_elaborated_spelling_for_type_operand(type)) {
      return true;
    }

    if (const auto *paren_type = llvm::dyn_cast<clang::ParenType>(type)) {
      qual_type = paren_type->getInnerType();
    } else if (const auto *pointer_type =
                   llvm::dyn_cast<clang::PointerType>(type)) {
      qual_type = pointer_type->getPointeeType();
    } else if (const auto *reference_type =
                   llvm::dyn_cast<clang::ReferenceType>(type)) {
      qual_type = reference_type->getPointeeType();
    } else if (const auto *array_type =
                   llvm::dyn_cast<clang::ArrayType>(type)) {
      qual_type = array_type->getElementType();
    } else if (const auto *attributed_type =
                   llvm::dyn_cast<clang::AttributedType>(type)) {
      qual_type = attributed_type->getModifiedType();
    } else if (const auto *adjusted_type =
                   llvm::dyn_cast<clang::AdjustedType>(type)) {
      qual_type = adjusted_type->getOriginalType();
    } else if (qualifiedTypeHasQualifier(type)) {
      qual_type = qual_type.getCanonicalType();
    } else {
      break;
    }

    type = qual_type.getTypePtrOrNull();
  }

  return false;
}

static bool
scope_supports_statement_list_for_instantiation_ref(SgScopeStatement *scope) {
  if (scope == nullptr) {
    return false;
  }
  switch (scope->variantT()) {
  case V_SgBasicBlock:
  case V_SgCatchOptionStmt:
  case V_SgDoWhileStmt:
  case V_SgForStatement:
  case V_SgRangeBasedForStatement:
  case V_SgTemplateFunctionDefinition:
  case V_SgFunctionDefinition:
  case V_SgSwitchStatement:
  case V_SgWhileStmt:
  case V_SgFortranDo:
    return true;
  default:
    return false;
  }
}

static SgDeclarationStatementPtrList *
get_scope_declaration_list_for_instantiation_ref(SgScopeStatement *scope) {
  if (scope == nullptr) {
    return nullptr;
  }
  if (SgGlobal *global = isSgGlobal(scope)) {
    return &global->get_declarations();
  }
  if (SgNamespaceDefinitionStatement *ns_def =
          isSgNamespaceDefinitionStatement(scope)) {
    return &ns_def->get_declarations();
  }
  if (SgDeclarationScope *decl_scope = isSgDeclarationScope(scope)) {
    return &decl_scope->get_declarations();
  }
  if (SgClassDefinition *class_def = isSgClassDefinition(scope)) {
    return &class_def->get_members();
  }
  if (SgTemplateClassDefinition *template_def =
          isSgTemplateClassDefinition(scope)) {
    return &template_def->get_members();
  }
  if (SgTemplateInstantiationDefn *inst_def =
          isSgTemplateInstantiationDefn(scope)) {
    return &inst_def->get_members();
  }
  if (scope->containsOnlyDeclarations()) {
    return &scope->getDeclarationList();
  }
  return nullptr;
}

static bool is_decl_attached_to_scope_child_list_for_instantiation_ref(
    SgScopeStatement *scope, SgDeclarationStatement *decl) {
  if (scope == nullptr || decl == nullptr) {
    return false;
  }
  if (SgDeclarationStatementPtrList *decls =
          get_scope_declaration_list_for_instantiation_ref(scope)) {
    return std::find(decls->begin(), decls->end(), decl) != decls->end();
  }
  if (!scope_supports_statement_list_for_instantiation_ref(scope)) {
    return false;
  }
  const SgStatementPtrList &stmts = scope->getStatementList();
  return std::find(stmts.begin(), stmts.end(), decl) != stmts.end();
}

static bool detach_decl_from_scope_child_list_for_instantiation_ref(
    SgDeclarationStatement *decl, SgScopeStatement *scope) {
  if (scope == nullptr || decl == nullptr) {
    return false;
  }

  auto erase_all = [&](auto &list) -> bool {
    bool removed = false;
    for (auto it = list.begin(); it != list.end();) {
      if (*it == decl) {
        it = list.erase(it);
        removed = true;
      } else {
        ++it;
      }
    }
    return removed;
  };

  if (SgDeclarationStatementPtrList *decls =
          get_scope_declaration_list_for_instantiation_ref(scope)) {
    return erase_all(*decls);
  }
  if (!scope_supports_statement_list_for_instantiation_ref(scope)) {
    return false;
  }
  return erase_all(scope->getStatementList());
}

static void setCoroutineKeywordAttribute(SgNode *node,
                                         const char *keyword_spelling) {
  if (node == nullptr || keyword_spelling == nullptr) {
    return;
  }
  node->setAttribute(Rose::kCoroutineKeywordAttributeName,
                     new AstValueAttribute<std::string>(keyword_spelling));
}

static bool canLexTrailingToken(clang::SourceLocation loc,
                                clang::SourceManager &sm) {
  if (!loc.isValid() || loc.isMacroID()) {
    return false;
  }

  clang::FileID file_id = sm.getFileID(loc);
  if (file_id.isInvalid() || file_id != sm.getMainFileID()) {
    return false;
  }

  auto buffer = sm.getBufferDataOrNone(file_id);
  if (!buffer) {
    return false;
  }

  unsigned offset = sm.getFileOffset(loc);
  if (offset >= buffer->size()) {
    return false;
  }

  bool invalid = false;
  (void)sm.getCharacterData(loc, &invalid);
  return !invalid;
}

static clang::SourceLocation findMatchingLParen(clang::SourceLocation rparen,
                                                clang::SourceManager &sm) {
  if (!canLexTrailingToken(rparen, sm)) {
    return clang::SourceLocation();
  }

  clang::SourceLocation spell_loc = sm.getSpellingLoc(rparen);
  if (!spell_loc.isValid()) {
    return clang::SourceLocation();
  }

  clang::FileID file_id = sm.getFileID(spell_loc);
  if (file_id.isInvalid()) {
    return clang::SourceLocation();
  }

  auto buffer = sm.getBufferDataOrNone(file_id);
  if (!buffer) {
    return clang::SourceLocation();
  }

  unsigned offset = sm.getFileOffset(spell_loc);
  if (offset >= buffer->size()) {
    return clang::SourceLocation();
  }

  int depth = 1;
  for (unsigned i = offset; i > 0; --i) {
    const char c = (*buffer)[i - 1];
    if (c == ')') {
      ++depth;
    } else if (c == '(') {
      --depth;
      if (depth == 0) {
        return sm.getLocForStartOfFile(file_id).getLocWithOffset(i - 1);
      }
    }
  }

  return clang::SourceLocation();
}

static void
propagateSpecialFunctionModifiers(SgFunctionDeclaration *base_decl,
                                  SgFunctionDeclaration *inst_decl) {
  if (base_decl == nullptr || inst_decl == nullptr) {
    return;
  }

  if (base_decl->get_specialFunctionModifier().isOperator()) {
    inst_decl->get_specialFunctionModifier().setOperator();
  }
  if (base_decl->get_specialFunctionModifier().isUldOperator()) {
    inst_decl->get_specialFunctionModifier().setUldOperator();
  }
}

static std::string
escapeOrdinaryStringLiteralContentsForUnparse(llvm::StringRef contents) {
  std::string escaped;
  escaped.reserve(contents.size());
  bool last_hex_escape = false;

  for (unsigned char ch : contents) {
    switch (ch) {
    case '\\':
      escaped.append("\\\\");
      last_hex_escape = false;
      break;
    case '\n':
      escaped.append("\\n");
      last_hex_escape = false;
      break;
    case '\r':
      escaped.append("\\r");
      last_hex_escape = false;
      break;
    case '"':
      escaped.append("\\\"");
      last_hex_escape = false;
      break;
    default:
      if (!clang::isPrintable(ch) || (last_hex_escape && std::isxdigit(ch))) {
        std::stringstream escape;
        escape << "\\x" << std::uppercase << std::hex << std::setw(2)
               << std::setfill('0') << static_cast<unsigned>(ch);
        escaped.append(escape.str());
        last_hex_escape = true;
      } else {
        escaped.push_back(static_cast<char>(ch));
        last_hex_escape = false;
      }
      break;
    }
  }

  return escaped;
}

static void appendUnicodeEscape(std::string &escaped, uint32_t code_point) {
  std::stringstream escape;
  escape << std::uppercase << std::hex << std::setfill('0');
  if (code_point <= 0xFFFF) {
    escape << "\\u" << std::setw(4) << code_point;
  } else {
    escape << "\\U" << std::setw(8) << code_point;
  }
  escaped.append(escape.str());
}

static void appendHexEscape(std::string &escaped, uint32_t code_unit) {
  std::stringstream escape;
  escape << "\\x" << std::uppercase << std::hex << code_unit;
  escaped.append(escape.str());
}

static void appendOctalEscape(std::string &escaped, uint32_t code_unit) {
  ROSE_ASSERT(code_unit <= 0xFF);
  std::stringstream escape;
  escape << '\\' << std::setfill('0') << std::setw(3) << std::oct << code_unit;
  escaped.append(escape.str());
}

static std::string escapeWideStringLiteralCodeUnitsForUnparse(
    const std::vector<uint32_t> &code_units, bool is_wide, bool is_utf16) {
  std::string escaped;
  bool last_hex_escape = false;

  for (size_t ii = 0; ii < code_units.size(); ++ii) {
    uint32_t content_val = code_units[ii];
    bool use_hex_escape = false;
    bool use_octal_escape = false;

    if (is_utf16 && content_val >= 0xD800 && content_val <= 0xDBFF &&
        ii + 1 < code_units.size()) {
      uint32_t low_surrogate = code_units[ii + 1];
      if (low_surrogate >= 0xDC00 && low_surrogate <= 0xDFFF) {
        content_val =
            0x10000 + (((content_val & 0x3FF) << 10) | (low_surrogate & 0x3FF));
        ++ii;
      }
    }

    if (content_val >= 0xD800 && content_val <= 0xDFFF) {
      use_hex_escape = true;
    } else if (!is_wide && content_val > 0xFF) {
      appendUnicodeEscape(escaped, content_val);
      last_hex_escape = false;
      continue;
    } else if (content_val == 0 && last_hex_escape) {
      use_octal_escape = true;
    } else if (auto mapped =
                   clang::escapeCStyle<clang::EscapeChar::Double>(content_val);
               !mapped.empty()) {
      escaped.append(mapped.str());
      last_hex_escape = false;
      continue;
    } else if (content_val > 0xFF ||
               (last_hex_escape && content_val <= 0xFF &&
                std::isxdigit(static_cast<unsigned char>(content_val)))) {
      use_hex_escape = true;
    } else if (content_val > 0xFF ||
               !clang::isPrintable(static_cast<unsigned char>(content_val))) {
      use_octal_escape = true;
    } else {
      escaped.push_back(static_cast<char>(content_val));
      last_hex_escape = false;
      continue;
    }

    if (use_hex_escape) {
      appendHexEscape(escaped, content_val);
      last_hex_escape = true;
    } else {
      appendOctalEscape(escaped, content_val);
      last_hex_escape = false;
    }
  }

  return escaped;
}

static clang::Token lexStringLiteralToken(const std::string &spelling,
                                          const clang::LangOptions &lang_opts) {
  clang::Token token;
  token.startToken();

  const char *buffer_begin = spelling.data();
  const char *buffer_end = buffer_begin + spelling.size();
  clang::Lexer lexer(clang::SourceLocation(), lang_opts, buffer_begin,
                     buffer_begin, buffer_end);
  lexer.LexFromRawLexer(token);

  ROSE_ASSERT(token.isOneOf(
      clang::tok::string_literal, clang::tok::wide_string_literal,
      clang::tok::utf8_string_literal, clang::tok::utf16_string_literal,
      clang::tok::utf32_string_literal));
  ROSE_ASSERT(token.getLength() == spelling.size());

  clang::Token eof_token;
  eof_token.startToken();
  lexer.LexFromRawLexer(eof_token);
  ROSE_ASSERT(eof_token.is(clang::tok::eof));

  return token;
}

static std::vector<uint32_t>
extractStringLiteralCodeUnits(const clang::StringLiteralParser &parser,
                              const clang::TargetInfo &target) {
  unsigned code_unit_width = 0;
  if (parser.isWide()) {
    code_unit_width = target.getWCharWidth();
  } else if (parser.isUTF16()) {
    code_unit_width = target.getChar16Width();
  } else {
    ROSE_ASSERT(parser.isUTF32());
    code_unit_width = target.getChar32Width();
  }

  ROSE_ASSERT(code_unit_width % 8 == 0);
  unsigned code_unit_bytes = code_unit_width / 8;
  ROSE_ASSERT(code_unit_bytes > 0);
  ROSE_ASSERT(code_unit_bytes <= sizeof(uint32_t));

  llvm::StringRef bytes = parser.GetString();
  ROSE_ASSERT(bytes.size() % code_unit_bytes == 0);

  std::vector<uint32_t> code_units;
  code_units.reserve(bytes.size() / code_unit_bytes);
  bool little_endian = target.isLittleEndian();

  for (size_t offset = 0; offset < bytes.size(); offset += code_unit_bytes) {
    uint32_t code_unit = 0;
    for (unsigned ii = 0; ii < code_unit_bytes; ++ii) {
      unsigned shift = little_endian ? 8 * ii : 8 * (code_unit_bytes - 1 - ii);
      code_unit |=
          static_cast<uint32_t>(static_cast<unsigned char>(bytes[offset + ii]))
          << shift;
    }
    code_units.push_back(code_unit);
  }

  ROSE_ASSERT(parser.GetNumStringChars() == code_units.size());
  return code_units;
}

static std::string
extractStringLiteralValue(const std::string &spelling,
                          const clang::CompilerInstance *compiler_instance) {
  ROSE_ASSERT(compiler_instance != nullptr);

  clang::Token token =
      lexStringLiteralToken(spelling, compiler_instance->getLangOpts());
  std::array<clang::Token, 1> tokens = {token};
  clang::StringLiteralParser parser(
      tokens, compiler_instance->getSourceManager(),
      compiler_instance->getLangOpts(), compiler_instance->getTarget(),
      &compiler_instance->getDiagnostics());
  ROSE_ASSERT(!parser.hadError);

  if (parser.isOrdinary() || parser.isUTF8()) {
    return escapeOrdinaryStringLiteralContentsForUnparse(parser.GetString());
  }

  return escapeWideStringLiteralCodeUnitsForUnparse(
      extractStringLiteralCodeUnits(parser, compiler_instance->getTarget()),
      parser.isWide(), parser.isUTF16());
}

static bool isFloatingLiteralForUdlSpelling(
    const std::string &spelling,
    const clang::CompilerInstance *compiler_instance) {
  ROSE_ASSERT(compiler_instance != nullptr);

  clang::NumericLiteralParser parser(
      spelling, clang::SourceLocation(), compiler_instance->getSourceManager(),
      compiler_instance->getLangOpts(), compiler_instance->getTarget(),
      compiler_instance->getDiagnostics());
  ROSE_ASSERT(!parser.hadError);
  return parser.isFloatingLiteral();
}

static void rejectClangOpenMPStmt(const clang::Stmt *stmt) {
  std::cerr
      << "Error: OpenMP/OpenACC constructs must be handled via pragma capture "
         "and omp/accAstConstructor. Clang OpenMP AST nodes should not be "
         "translated. Ensure -fopenmp is not passed to Clang."
      << std::endl;
  if (stmt != nullptr) {
    stmt->dump();
  }
  ROSE_ABORT();
}

static std::vector<const clang::NamespaceDecl *>
collectNamespaceContexts(clang::DeclContext *context) {
  std::vector<const clang::NamespaceDecl *> namespaces;
  for (clang::DeclContext *ctx = context; ctx != nullptr;
       ctx = ctx->getParent()) {
    if (const clang::NamespaceDecl *ns =
            llvm::dyn_cast<clang::NamespaceDecl>(ctx)) {
      if (!ns->isAnonymousNamespace()) {
        namespaces.push_back(ns);
      }
    }
  }
  std::reverse(namespaces.begin(), namespaces.end());
  return namespaces;
}

static std::vector<std::string>
collectNamespaceNamesFromScope(SgScopeStatement *scope) {
  std::vector<std::string> names;
  for (SgNode *node = scope; node != nullptr; node = node->get_parent()) {
    if (SgNamespaceDefinitionStatement *ns_def =
            isSgNamespaceDefinitionStatement(node)) {
      SgNamespaceDeclarationStatement *ns_decl =
          ns_def->get_namespaceDeclaration();
      if (ns_decl != nullptr && !ns_decl->get_isUnnamedNamespace()) {
        names.push_back(ns_decl->get_name().getString());
      }
    }
  }
  std::reverse(names.begin(), names.end());
  return names;
}

static bool scopeIsWithinNamespaceChain(SgScopeStatement *scope,
                                        const clang::DeclContext *context) {
  if (scope == nullptr || context == nullptr) {
    return false;
  }
  std::vector<const clang::NamespaceDecl *> decl_namespaces =
      collectNamespaceContexts(const_cast<clang::DeclContext *>(context));
  if (decl_namespaces.empty()) {
    return false;
  }
  std::vector<std::string> scope_names = collectNamespaceNamesFromScope(scope);
  if (scope_names.size() < decl_namespaces.size()) {
    return false;
  }
  for (size_t i = 0; i < decl_namespaces.size(); ++i) {
    const clang::NamespaceDecl *ns = decl_namespaces[i];
    if (ns == nullptr) {
      return false;
    }
    if (scope_names[i] != ns->getNameAsString()) {
      return false;
    }
  }
  return true;
}

static clang::NestedNameSpecifier
buildNamespaceQualifierForDeclContext(clang::DeclContext *context,
                                      clang::ASTContext &ast_context) {
  if (context == nullptr) {
    return std::nullopt;
  }
  std::vector<const clang::NamespaceDecl *> namespaces =
      collectNamespaceContexts(context);
  if (namespaces.empty()) {
    return std::nullopt;
  }

  clang::NestedNameSpecifier qualifier = std::nullopt;
  for (const clang::NamespaceDecl *ns : namespaces) {
    if (ns == nullptr || ns->isAnonymousNamespace()) {
      continue;
    }
    qualifier = clang::NestedNameSpecifier(ast_context, ns, qualifier);
  }
  return qualifier;
}

static std::string
buildOverloadedOperatorName(clang::OverloadedOperatorKind op) {
  const char *spelling = clang::getOperatorSpelling(op);
  ROSE_ASSERT(spelling != nullptr);

  std::string result = "operator";
  if (std::isalpha(static_cast<unsigned char>(spelling[0])) ||
      spelling[0] == '_') {
    result += ' ';
  }
  result += spelling;
  return result;
}

static std::string getDeclarationNameAsString(clang::DeclarationName name) {
  if (name.isEmpty()) {
    return "";
  }
  if (const clang::IdentifierInfo *identifier = name.getAsIdentifierInfo()) {
    return identifier->getName().str();
  }
  if (name.getCXXOverloadedOperator() != clang::OO_None) {
    return buildOverloadedOperatorName(name.getCXXOverloadedOperator());
  }
  return name.getAsString();
}

static SgName normalizeOperatorTerminalName(const SgName &name) {
  const std::string value = name.getString();
  if (value.empty()) {
    return name;
  }
  if (value.rfind("operator", 0) == 0) {
    return name;
  }
  bool has_identifier_char = false;
  for (unsigned char ch : value) {
    if (std::isalnum(ch) || ch == '_') {
      has_identifier_char = true;
      break;
    }
  }
  if (!has_identifier_char) {
    return SgName("operator" + value);
  }
  return name;
}

struct ExplicitTemplateArgumentSourceInfo {
  bool has_template_argument_list = false;
  size_t argument_count = 0;
};

static bool hasExplicitEmptyTemplateArgumentList(
    const clang::TemplateArgumentListInfo &arg_info) {
  return arg_info.arguments().empty() && arg_info.getLAngleLoc().isValid() &&
         arg_info.getRAngleLoc().isValid();
}

static ExplicitTemplateArgumentSourceInfo
scanExplicitTemplateArgumentsInText(llvm::StringRef text) {
  ExplicitTemplateArgumentSourceInfo info;

  bool in_args = false;
  int depth = 0;
  bool saw_top_level_arg_token = false;
  bool last_was_comma = false;

  for (size_t i = 0; i < text.size(); ++i) {
    const unsigned char ch = static_cast<unsigned char>(text[i]);
    const char c = static_cast<char>(ch);

    if (!in_args) {
      if (std::isspace(ch)) {
        continue;
      }
      if (c == '<') {
        in_args = true;
        depth = 1;
        saw_top_level_arg_token = false;
        last_was_comma = false;
        continue;
      }
      if (c == '(' || c == '[' || c == ')' || c == ';' || c == '{') {
        break;
      }
      continue;
    }

    if (c == '<') {
      if (depth == 1) {
        saw_top_level_arg_token = true;
        last_was_comma = false;
      }
      ++depth;
      continue;
    }

    if (c == '>') {
      if (depth == 1 && (saw_top_level_arg_token || last_was_comma)) {
        ++info.argument_count;
      }
      --depth;
      if (depth == 0) {
        info.has_template_argument_list = true;
        break;
      }
      if (depth < 0) {
        break;
      }
      last_was_comma = false;
      continue;
    }

    if (depth == 1 && c == ',') {
      // Conservative fallback for malformed spellings.
      ++info.argument_count;
      saw_top_level_arg_token = false;
      last_was_comma = true;
      continue;
    }

    if (depth > 0 && !std::isspace(ch)) {
      if (depth == 1) {
        last_was_comma = false;
      }
      saw_top_level_arg_token = true;
    }
  }

  if (!info.has_template_argument_list) {
    info.argument_count = 0;
  }

  return info;
}

static ExplicitTemplateArgumentSourceInfo
scanExplicitTemplateArgumentsAfterLoc(clang::SourceLocation loc,
                                      clang::SourceManager &sm,
                                      const clang::LangOptions &lang_opts) {
  if (!canLexTrailingToken(loc, sm)) {
    return {};
  }

  clang::SourceLocation spell_loc = sm.getSpellingLoc(loc);
  if (!spell_loc.isValid()) {
    return {};
  }

  clang::FileID file_id = sm.getFileID(spell_loc);
  if (file_id.isInvalid()) {
    return {};
  }

  auto buffer = sm.getBufferDataOrNone(file_id);
  if (!buffer) {
    return {};
  }

  clang::SourceLocation end_loc =
      clang::Lexer::getLocForEndOfToken(spell_loc, 0, sm, lang_opts);
  if (!end_loc.isValid()) {
    return {};
  }

  unsigned offset = sm.getFileOffset(end_loc);
  if (offset >= buffer->size()) {
    return {};
  }

  return scanExplicitTemplateArgumentsInText(buffer->substr(offset));
}

static ExplicitTemplateArgumentSourceInfo
scanExplicitTemplateArgumentsInSourceRange(
    clang::SourceRange range, clang::SourceManager &sm,
    const clang::LangOptions &lang_opts) {
  if (!range.isValid()) {
    return {};
  }

  clang::SourceLocation begin = sm.getSpellingLoc(range.getBegin());
  clang::SourceLocation end = sm.getSpellingLoc(range.getEnd());
  if (!begin.isValid() || !end.isValid()) {
    return {};
  }

  clang::SourceLocation end_loc =
      clang::Lexer::getLocForEndOfToken(end, 0, sm, lang_opts);
  if (!end_loc.isValid()) {
    return {};
  }

  bool invalid = false;
  llvm::StringRef text = clang::Lexer::getSourceText(
      clang::CharSourceRange::getCharRange(begin, end_loc), sm, lang_opts,
      &invalid);
  if (invalid || text.empty()) {
    return {};
  }

  return scanExplicitTemplateArgumentsInText(text);
}

static ExplicitTemplateArgumentSourceInfo
scanExplicitTemplateArgumentsForExprSource(
    clang::SourceRange range, clang::SourceLocation trailing_loc,
    clang::CompilerInstance *compiler_instance) {
  if (compiler_instance == nullptr) {
    return {};
  }

  clang::SourceManager &sm = compiler_instance->getSourceManager();
  const clang::LangOptions &lang_opts = compiler_instance->getLangOpts();

  ExplicitTemplateArgumentSourceInfo source_info =
      scanExplicitTemplateArgumentsInSourceRange(range, sm, lang_opts);
  if (!source_info.has_template_argument_list) {
    source_info =
        scanExplicitTemplateArgumentsAfterLoc(trailing_loc, sm, lang_opts);
  }

  return source_info;
}

static void ensure_function_param_list(SgFunctionDeclaration *decl,
                                       SgFunctionParameterList *fallback) {
  if (decl == nullptr) {
    if (fallback != nullptr && fallback->get_parent() == nullptr) {
      delete fallback;
    }
    return;
  }

  SgFunctionParameterList *params = decl->get_parameterList();
  if (params == nullptr && fallback != nullptr) {
    SageInterface::setParameterList(decl, fallback);
    params = decl->get_parameterList();
  }
  if (params == nullptr) {
    return;
  }
  if (params->get_parent() == nullptr) {
    params->set_parent(decl);
  }
  if (decl->get_parameterList() != params) {
    decl->set_parameterList(params);
  }

  SgScopeStatement *scope = decl->get_scope();
  for (SgInitializedName *param : params->get_args()) {
    if (param == nullptr) {
      continue;
    }
    if (param->get_parent() == nullptr) {
      param->set_parent(params);
    }
    param->set_declptr(decl);
    if (param->get_scope() == nullptr && scope != nullptr) {
      param->set_scope(scope);
    }
  }

  if (SgFunctionParameterList *syntax = decl->get_parameterList_syntax()) {
    if (syntax->get_parent() == nullptr) {
      syntax->set_parent(decl);
    }
  }

  if (fallback != nullptr && params != fallback &&
      fallback->get_parent() == nullptr) {
    delete fallback;
  }
}

clang::SourceRange
extendSourceRangeWithTrailingSemicolon(clang::SourceRange range,
                                       clang::SourceManager &sm,
                                       const clang::LangOptions &lang_opts) {
  if (!range.isValid()) {
    return range;
  }

  clang::SourceLocation end = range.getEnd();
  if (!end.isValid()) {
    return range;
  }
  if (end.isMacroID()) {
    end = sm.getExpansionLoc(end);
  }
  if (!end.isValid()) {
    return range;
  }

  if (!canLexTrailingToken(end, sm)) {
    return range;
  }

  // Lex the next non-whitespace token after the end token.
  clang::SourceLocation after_end =
      clang::Lexer::getLocForEndOfToken(end, 0, sm, lang_opts);
  if (!after_end.isValid()) {
    return range;
  }

  clang::Token tok;
  if (clang::Lexer::getRawToken(after_end, tok, sm, lang_opts,
                                /*IgnoreWhiteSpace=*/true)) {
    return range;
  }
  if (!tok.is(clang::tok::semi)) {
    return range;
  }

  range.setEnd(tok.getLocation());
  return range;
}

std::string getFloatingLiteralSpelling(const clang::FloatingLiteral *literal,
                                       clang::SourceManager &sm,
                                       const clang::LangOptions &lang_opts) {
  if (literal == nullptr) {
    return "";
  }

  clang::SourceLocation loc = literal->getLocation();
  if (!loc.isValid()) {
    return "";
  }

  loc = sm.getSpellingLoc(loc);
  if (loc.isValid()) {
    llvm::SmallString<64> buf;
    bool invalid = false;
    llvm::StringRef spelling =
        clang::Lexer::getSpelling(loc, buf, sm, lang_opts, &invalid);
    if (!invalid && !spelling.empty()) {
      return spelling.str();
    }
  }

  clang::SourceRange range = literal->getSourceRange();
  if (!range.isValid()) {
    return "";
  }

  clang::SourceLocation begin = sm.getSpellingLoc(range.getBegin());
  clang::SourceLocation end = sm.getSpellingLoc(range.getEnd());
  if (!begin.isValid() || !end.isValid()) {
    return "";
  }

  bool invalid = false;
  llvm::StringRef text = clang::Lexer::getSourceText(
      clang::CharSourceRange::getTokenRange(begin, end), sm, lang_opts,
      &invalid);
  if (invalid) {
    return "";
  }

  return text.str();
}

static bool looksLikeIntegerLiteralToken(const std::string &text) {
  size_t i = 0;
  while (i < text.size() &&
         std::isspace(static_cast<unsigned char>(text[i])) != 0) {
    ++i;
  }
  if (i >= text.size()) {
    return false;
  }

  // Integer literal tokens always start with a digit in C/C++.
  return std::isdigit(static_cast<unsigned char>(text[i])) != 0;
}

std::string getIntegerLiteralSpelling(const clang::IntegerLiteral *literal,
                                      clang::SourceManager &sm,
                                      const clang::LangOptions &lang_opts) {
  if (literal == nullptr) {
    return "";
  }

  clang::SourceLocation loc = literal->getLocation();
  if (!loc.isValid()) {
    return "";
  }

  loc = sm.getSpellingLoc(loc);
  if (loc.isValid()) {
    llvm::SmallString<64> buf;
    bool invalid = false;
    llvm::StringRef spelling =
        clang::Lexer::getSpelling(loc, buf, sm, lang_opts, &invalid);
    if (!invalid && !spelling.empty()) {
      std::string token = spelling.str();
      if (looksLikeIntegerLiteralToken(token)) {
        return token;
      }
    }
  }

  clang::SourceRange range = literal->getSourceRange();
  if (!range.isValid()) {
    return "";
  }

  clang::SourceLocation begin = sm.getSpellingLoc(range.getBegin());
  clang::SourceLocation end = sm.getSpellingLoc(range.getEnd());
  if (!begin.isValid() || !end.isValid()) {
    return "";
  }

  bool invalid = false;
  llvm::StringRef text = clang::Lexer::getSourceText(
      clang::CharSourceRange::getTokenRange(begin, end), sm, lang_opts,
      &invalid);
  if (invalid) {
    return "";
  }

  std::string token = text.str();
  if (!looksLikeIntegerLiteralToken(token)) {
    return "";
  }

  return token;
}

SgSymbol *findEnclosingThisSymbol(SgScopeStatement *starting_scope) {
  for (SgNode *node = starting_scope; node != nullptr;
       node = node->get_parent()) {
    SgClassDefinition *class_def = isSgClassDefinition(node);
    if (class_def == nullptr) {
      continue;
    }

    SgClassDeclaration *class_decl = class_def->get_declaration();
    ROSE_ASSERT(class_decl != nullptr);
    SgScopeStatement *decl_scope = class_decl->get_scope();
    ROSE_ASSERT(decl_scope != nullptr);

    if (SgClassSymbol *class_sym =
            SageInterface::lookupClassSymbolInParentScopes(
                class_decl->get_name(), decl_scope)) {
      return class_sym;
    }

    if (SgClassSymbol *class_sym =
            decl_scope->lookup_class_symbol(class_decl->get_name())) {
      return class_sym;
    }

    if (SgSymbol *sym = class_decl->search_for_symbol_from_symbol_table()) {
      if (isSgClassSymbol(sym) != nullptr ||
          isSgNonrealSymbol(sym) != nullptr) {
        return sym;
      }
    }

    break;
  }

  return nullptr;
}

std::string trimWhitespace(const std::string &input) {
  size_t start = 0;
  while (start < input.size() &&
         (input[start] == ' ' || input[start] == '\t')) {
    ++start;
  }
  size_t end = input.size();
  while (end > start && (input[end - 1] == ' ' || input[end - 1] == '\t')) {
    --end;
  }
  return input.substr(start, end - start);
}

std::string resolve_template_parameter_name_from_stmt_scope(
    SgScopeStatement *scope, unsigned depth, unsigned index) {
  return resolveTemplateParameterNameFromSageScopeShared(
      scope, depth, index, normalizeClangTemplateParamName);
}

std::string
resolve_synthetic_template_param_token_in_stmt_scope(std::string token,
                                                     SgScopeStatement *scope) {
  token = normalizeClangTemplateParamName(trimWhitespace(token));
  if (token.empty()) {
    return token;
  }

  unsigned depth = 0;
  unsigned index = 0;
  if (!parseTemplateParamDepthAndIndex(token, &depth, &index)) {
    return token;
  }

  std::string resolved =
      resolve_template_parameter_name_from_stmt_scope(scope, depth, index);
  return resolved.empty() ? token : resolved;
}

std::string
resolve_synthetic_template_param_name_in_stmt_scope(std::string name,
                                                    SgScopeStatement *scope) {
  name = trimWhitespace(name);
  if (name.empty()) {
    return name;
  }

  static const std::string operator_prefix = "operator ";
  if (name.rfind(operator_prefix, 0) == 0) {
    std::string suffix = name.substr(operator_prefix.size());
    suffix =
        resolve_synthetic_template_param_token_in_stmt_scope(suffix, scope);
    return operator_prefix + suffix;
  }

  return resolve_synthetic_template_param_token_in_stmt_scope(name, scope);
}

std::string printNestedNameSpecifier(clang::NestedNameSpecifier specifier,
                                     const clang::ASTContext &context) {
  if (!specifier) {
    return "";
  }
  std::string buffer;
  llvm::raw_string_ostream stream(buffer);
  clang::PrintingPolicy policy(context.getLangOpts());
  policy.SuppressScope = false;
  policy.SuppressTagKeyword = true;
  specifier.print(stream, policy);
  stream.flush();
  return buffer;
}

std::string normalizeQualifierToken(std::string token) {
  token = trimWhitespace(token);
  while (token.rfind("::", 0) == 0) {
    token = token.substr(2);
  }
  while (token.size() >= 2 && token.compare(token.size() - 2, 2, "::") == 0) {
    token.erase(token.size() - 2);
  }
  token = trimWhitespace(token);
  return token;
}

std::string deriveNestedNameSpecifierToken(clang::NestedNameSpecifier specifier,
                                           const clang::ASTContext &context) {
  if (!specifier) {
    return "";
  }

  std::string full_text = printNestedNameSpecifier(specifier, context);
  if (full_text.empty()) {
    return "";
  }

  std::string prefix_text;
  if (clang::NestedNameSpecifier prefix =
          nestedNameSpecifierPrefix(specifier)) {
    prefix_text = printNestedNameSpecifier(prefix, context);
  }

  std::string token = full_text;
  if (!prefix_text.empty()) {
    if (token.rfind(prefix_text, 0) != 0) {
      return "";
    }
    token = token.substr(prefix_text.size());
  }

  return normalizeQualifierToken(token);
}

std::string
getNestedNameSpecifierLocToken(const clang::NestedNameSpecifierLoc &loc,
                               clang::SourceManager &sm,
                               const clang::LangOptions &lang_opts) {
  if (!loc.getNestedNameSpecifier()) {
    return "";
  }

  clang::SourceRange range = loc.getLocalSourceRange();
  if (!range.isValid()) {
    return "";
  }

  clang::SourceLocation begin = sm.getSpellingLoc(range.getBegin());
  clang::SourceLocation end = sm.getSpellingLoc(range.getEnd());
  if (!begin.isValid() || !end.isValid()) {
    return "";
  }

  bool invalid = false;
  llvm::StringRef text = clang::Lexer::getSourceText(
      clang::CharSourceRange::getTokenRange(begin, end), sm, lang_opts,
      &invalid);
  if (invalid || text.empty()) {
    return "";
  }

  return normalizeQualifierToken(text.str());
}

struct ExplicitQualifierInfo {
  int depth = 0;
  bool has_global = false;
  SgStringList tokens;
};

std::string buildExplicitQualifierString(const ExplicitQualifierInfo &info) {
  std::string qualifier;
  if (info.has_global) {
    qualifier = "::";
  }
  for (const std::string &token : info.tokens) {
    if (token.empty()) {
      continue;
    }
    if (!qualifier.empty() && qualifier.back() != ':') {
      qualifier += "::";
    }
    qualifier += token;
    qualifier += "::";
  }
  return qualifier;
}

ExplicitQualifierInfo getExplicitQualifierInfo(
    clang::NestedNameSpecifier qualifier, const clang::ASTContext &context,
    const clang::NestedNameSpecifierLoc *qualifier_loc = nullptr,
    clang::SourceManager *sm = nullptr,
    const clang::LangOptions *lang_opts = nullptr) {
  ExplicitQualifierInfo info;
  std::vector<std::string> reversed_tokens;
  bool can_read_source_tokens =
      qualifier_loc != nullptr && sm != nullptr && lang_opts != nullptr;

  for (clang::NestedNameSpecifier nns = qualifier; nns;
       nns = nestedNameSpecifierPrefix(nns)) {
    if (nns.getKind() == clang::NestedNameSpecifier::Kind::Global) {
      info.has_global = true;
      continue;
    }
    ++info.depth;
    std::string token;
    bool token_from_source = false;
    if (can_read_source_tokens && qualifier_loc != nullptr &&
        qualifier_loc->getNestedNameSpecifier() == nns) {
      token = getNestedNameSpecifierLocToken(*qualifier_loc, *sm, *lang_opts);
      token_from_source = !token.empty();
    }
    if (!token.empty() &&
        nns.getKind() == clang::NestedNameSpecifier::Kind::Type &&
        token.find("::") != std::string::npos) {
      if (std::string local_token =
              deriveNestedNameSpecifierToken(nns, context);
          !local_token.empty()) {
        token = local_token;
        token_from_source = true;
      }
    }
    if (token.empty()) {
      switch (nns.getKind()) {
      case clang::NestedNameSpecifier::Kind::Null:
        break;
      case clang::NestedNameSpecifier::Kind::Global:
        break;
      case clang::NestedNameSpecifier::Kind::Namespace: {
        const clang::NamespaceDecl *ns = nestedNameSpecifierNamespace(nns);
        if (ns != nullptr) {
          if (!ns->isAnonymousNamespace()) {
            token = ns->getNameAsString();
          }
        }
        break;
      }
      case clang::NestedNameSpecifier::Kind::Type: {
        const clang::Type *type = nns.getAsType();
        if (const clang::TypedefType *typedef_type =
                llvm::dyn_cast_or_null<clang::TypedefType>(type)) {
          token = typedef_type->getDecl()->getNameAsString();
        } else if (const clang::UsingType *using_type =
                       llvm::dyn_cast_or_null<clang::UsingType>(type)) {
          clang::UsingShadowDecl *using_shadow = using_type->getDecl();
          if (using_shadow != nullptr) {
            token = using_shadow->getNameAsString();
          }
        } else if (const clang::RecordType *record_type =
                       llvm::dyn_cast_or_null<clang::RecordType>(type)) {
          token = record_type->getDecl()->getNameAsString();
        } else if (const clang::EnumType *enum_type =
                       llvm::dyn_cast_or_null<clang::EnumType>(type)) {
          token = enum_type->getDecl()->getNameAsString();
        } else if (const clang::InjectedClassNameType *injected_type =
                       llvm::dyn_cast_or_null<clang::InjectedClassNameType>(
                           type)) {
          token = injected_type->getDecl()->getNameAsString();
        } else if (type != nullptr) {
          clang::QualType qual_type(type, 0);
          clang::PrintingPolicy policy(context.getLangOpts());
          policy.SuppressScope = true;
          policy.SuppressTagKeyword = true;
          token = qual_type.getAsString(policy);
        }
        break;
      }
      case clang::NestedNameSpecifier::Kind::MicrosoftSuper:
        token = "super";
        break;
      }
    }
    if (token.empty()) {
      if (const clang::NamespaceAliasDecl *alias =
              nestedNameSpecifierNamespaceAlias(nns)) {
        if (alias != nullptr) {
          token = alias->getNameAsString();
        }
      }
    }
    if (token.empty()) {
      token = deriveNestedNameSpecifierToken(nns, context);
      token_from_source = !token.empty();
    }
    if (!token.empty() && !token_from_source &&
        nestedNameSpecifierHasTemplateKeyword(nns)) {
      token = "template " + token;
    }
    if (!token.empty()) {
      reversed_tokens.push_back(token);
    }
  }
  if (!reversed_tokens.empty()) {
    for (auto it = reversed_tokens.rbegin(); it != reversed_tokens.rend();
         ++it) {
      info.tokens.push_back(*it);
    }
  }
  return info;
}

template <typename T>
void setExplicitQualifierOnRef(T *ref, const ExplicitQualifierInfo &info) {
  if (ref == nullptr) {
    return;
  }
  ref->set_explicit_name_qualification_length(info.depth);
  ref->set_explicit_global_qualification(info.has_global);
  if (!info.tokens.empty()) {
    ref->set_explicit_name_qualification_tokens(info.tokens);
  }
}

void setExplicitQualifierOnExpr(SgExpression *expr,
                                const ExplicitQualifierInfo &info) {
  if (expr == nullptr || (info.depth == 0 && !info.has_global)) {
    return;
  }
  if (SgVarRefExp *var_ref = isSgVarRefExp(expr)) {
    setExplicitQualifierOnRef(var_ref, info);
  } else if (SgTemplateMemberFunctionRefExp *tmpl_member =
                 isSgTemplateMemberFunctionRefExp(expr)) {
    setExplicitQualifierOnRef(tmpl_member, info);
  } else if (SgMemberFunctionRefExp *member_ref =
                 isSgMemberFunctionRefExp(expr)) {
    setExplicitQualifierOnRef(member_ref, info);
  } else if (SgTemplateFunctionRefExp *tmpl_func =
                 isSgTemplateFunctionRefExp(expr)) {
    setExplicitQualifierOnRef(tmpl_func, info);
  } else if (SgFunctionRefExp *func_ref = isSgFunctionRefExp(expr)) {
    setExplicitQualifierOnRef(func_ref, info);
  } else if (SgNonrealRefExp *nonreal_ref = isSgNonrealRefExp(expr)) {
    setExplicitQualifierOnRef(nonreal_ref, info);
  } else if (SgEnumVal *enum_val = isSgEnumVal(expr)) {
    setExplicitQualifierOnRef(enum_val, info);
  }
}

void attachExplicitQualifierFromNestedName(
    SgExpression *expr, clang::NestedNameSpecifier qualifier,
    const clang::NestedNameSpecifierLoc *qualifier_loc,
    clang::CompilerInstance *compiler_instance) {
  if (expr == nullptr || !qualifier || compiler_instance == nullptr) {
    return;
  }

  const clang::NestedNameSpecifierLoc *qualifier_loc_ptr = nullptr;
  if (qualifier_loc != nullptr && qualifier_loc->getNestedNameSpecifier()) {
    qualifier_loc_ptr = qualifier_loc;
  }

  const ExplicitQualifierInfo info = getExplicitQualifierInfo(
      qualifier, compiler_instance->getASTContext(), qualifier_loc_ptr,
      &compiler_instance->getSourceManager(),
      &compiler_instance->getLangOpts());
  setExplicitQualifierOnExpr(expr, info);
}

clang::NestedNameSpecifier structuralQualifierForExplicitlyQualifiedNonrealRef(
    clang::NestedNameSpecifier qualifier) {
  if (!qualifier) {
    return qualifier;
  }

  // SgNonrealRefExp unparses from explicit qualifier tokens. Rebuilding the
  // same nested-name chain inside the synthetic nonreal declaration causes the
  // qualifier to be printed twice (e.g. `std::std::foo`).
  return std::nullopt;
}

SgScopeStatement *normalizeNamespaceScope(SgScopeStatement *scope) {
  if (scope == nullptr) {
    return nullptr;
  }

  SgNamespaceDefinitionStatement *ns_def =
      isSgNamespaceDefinitionStatement(scope);
  if (ns_def == nullptr) {
    return scope;
  }

  SgNamespaceDeclarationStatement *ns_decl = ns_def->get_namespaceDeclaration();
  if (ns_decl == nullptr) {
    return scope;
  }

  SgNamespaceDeclarationStatement *first_nondef =
      isSgNamespaceDeclarationStatement(
          ns_decl->get_firstNondefiningDeclaration());
  if (first_nondef == nullptr) {
    return scope;
  }

  SgNamespaceDefinitionStatement *first_def = first_nondef->get_definition();
  return first_def != nullptr ? first_def : scope;
}

template <typename Fn>
static void for_each_unique_function_decl_in_chain(SgFunctionDeclaration *decl,
                                                   Fn &&fn) {
  if (decl == nullptr) {
    return;
  }

  std::array<SgFunctionDeclaration *, 3> candidates = {
      decl, isSgFunctionDeclaration(decl->get_firstNondefiningDeclaration()),
      isSgFunctionDeclaration(decl->get_definingDeclaration())};
  std::array<SgFunctionDeclaration *, 3> visited = {nullptr, nullptr, nullptr};
  size_t visited_count = 0;

  for (SgFunctionDeclaration *candidate : candidates) {
    if (candidate == nullptr) {
      continue;
    }

    bool seen = false;
    for (size_t i = 0; i < visited_count; ++i) {
      if (visited[i] == candidate) {
        seen = true;
        break;
      }
    }
    if (seen) {
      continue;
    }

    visited[visited_count++] = candidate;
    fn(candidate);
  }
}

template <typename TemplateDeclT, typename InstantiationDeclT>
static TemplateDeclT *
recover_instantiation_template_declaration(SgFunctionDeclaration *decl) {
  TemplateDeclT *result = nullptr;

  for_each_unique_function_decl_in_chain(
      decl, [&](SgFunctionDeclaration *candidate_decl) {
        if (result != nullptr) {
          return;
        }

        if (TemplateDeclT *template_decl =
                dynamic_cast<TemplateDeclT *>(candidate_decl)) {
          result = template_decl;
          return;
        }

        if (InstantiationDeclT *inst_decl =
                dynamic_cast<InstantiationDeclT *>(candidate_decl)) {
          if (TemplateDeclT *template_decl = dynamic_cast<TemplateDeclT *>(
                  inst_decl->get_templateDeclaration())) {
            result = template_decl;
            return;
          }
          if (TemplateDeclT *template_decl = dynamic_cast<TemplateDeclT *>(
                  inst_decl->get_specializedTemplateDeclaration())) {
            result = template_decl;
          }
        }
      });

  return result;
}

template <typename InstantiationDeclT, typename TemplateDeclT>
static void apply_template_instantiation_template_links(
    InstantiationDeclT *decl, TemplateDeclT *template_decl,
    const SgName &template_name = SgName()) {
  if (decl == nullptr || template_decl == nullptr) {
    return;
  }

  const SgName resolved_template_name =
      template_name.is_null() ? template_decl->get_name() : template_name;

  for_each_unique_function_decl_in_chain(
      decl, [&](SgFunctionDeclaration *candidate_decl) {
        if (InstantiationDeclT *inst_decl =
                dynamic_cast<InstantiationDeclT *>(candidate_decl)) {
          inst_decl->set_templateDeclaration(template_decl);
          inst_decl->set_specializedTemplateDeclaration(template_decl);
          if (resolved_template_name.is_null() == false) {
            inst_decl->set_templateName(resolved_template_name);
          }
        }
      });
}

static void
mark_synthesized_instantiation_decl_chain_for_ref(SgFunctionDeclaration *decl,
                                                  SgScopeStatement *scope) {
  if (decl == nullptr) {
    return;
  }

  SgScopeStatement *target_scope = normalizeNamespaceScope(scope);
  if (target_scope == nullptr) {
    target_scope = normalizeNamespaceScope(decl->get_scope());
  }

  for_each_unique_function_decl_in_chain(
      decl, [&](SgFunctionDeclaration *candidate_decl) {
        Sg_File_Info *file_info = candidate_decl->get_file_info();
        const bool touches_synthetic_candidate =
            candidate_decl == decl || file_info == nullptr ||
            file_info->isCompilerGenerated();
        if (!touches_synthetic_candidate) {
          return;
        }

        if (target_scope != nullptr &&
            candidate_decl->get_scope() != target_scope) {
          candidate_decl->set_scope(target_scope);
        }

        if (SgScopeStatement *decl_scope = candidate_decl->get_scope()) {
          if (is_decl_attached_to_scope_child_list_for_instantiation_ref(
                  decl_scope, candidate_decl)) {
            detach_decl_from_scope_child_list_for_instantiation_ref(
                candidate_decl, decl_scope);
          }
          candidate_decl->set_parent(decl_scope);
        }

        suppress_unparse_output(candidate_decl);
        if (SgFunctionParameterList *params =
                candidate_decl->get_parameterList()) {
          suppress_unparse_output(params);
          for (SgInitializedName *param : params->get_args()) {
            if (param != nullptr) {
              suppress_unparse_output(param);
            }
          }
        }
      });
}

bool isSkippableLineBetweenPragmaAndStatement(const std::string &trimmed_line) {
  if (trimmed_line.empty()) {
    return true;
  }

  if (trimmed_line.size() >= 2 && trimmed_line[0] == '/' &&
      trimmed_line[1] == '/') {
    return true;
  }
  if (trimmed_line.size() >= 2 && trimmed_line[0] == '/' &&
      trimmed_line[1] == '*') {
    return true;
  }
  if (!trimmed_line.empty() && trimmed_line[0] == '*') {
    return true;
  }
  if (trimmed_line.size() >= 2 &&
      trimmed_line[trimmed_line.size() - 2] == '*' &&
      trimmed_line.back() == '/') {
    return true;
  }

  if (!trimmed_line.empty() && trimmed_line.back() == '\\') {
    return true;
  }

  if (trimmed_line.back() == ':') {
    bool is_case = trimmed_line.rfind("case", 0) == 0;
    bool is_default = trimmed_line.rfind("default", 0) == 0;
    if (!is_case && !is_default) {
      bool valid_label = true;
      for (size_t i = 0; i + 1 < trimmed_line.size(); ++i) {
        char c = trimmed_line[i];
        if (!(std::isalnum(static_cast<unsigned char>(c)) || c == '_')) {
          valid_label = false;
          break;
        }
      }
      if (valid_label) {
        return true;
      }
    }
  }

  return false;
}

bool getLineContent(const clang::SourceManager &source_manager,
                    clang::FileID file_id, unsigned line, std::string &result) {
  if (line == 0) {
    return false;
  }
  clang::SourceLocation line_start =
      source_manager.translateLineCol(file_id, line, 1);
  if (!line_start.isValid()) {
    return false;
  }

  const char *begin = source_manager.getCharacterData(line_start);
  const char *current = begin;
  while (*current != '\n' && *current != '\r' && *current != '\0') {
    ++current;
  }

  result.assign(begin, current - begin);
  return true;
}

std::string extractOpenMPDirective(const std::string &pragma_text) {
  size_t pragma_pos = pragma_text.find("pragma");
  if (pragma_pos == std::string::npos) {
    return std::string();
  }

  size_t hash_pos = pragma_text.find('#');
  size_t spaces_after_hash = 0;
  if (hash_pos != std::string::npos && pragma_pos > hash_pos) {
    spaces_after_hash = pragma_pos - hash_pos - 1;
  }
  size_t after_pragma = pragma_pos + 6; // Skip "pragma"

  if (after_pragma < pragma_text.size() &&
      RoseOpenMPPragmaCallback::isWhitespace(pragma_text[after_pragma])) {
    ++after_pragma;
  }

  return std::string(spaces_after_hash, ' ') + pragma_text.substr(after_pragma);
}

} // namespace

SgExpression *
ClangToSageTranslator::buildFallbackExpression(const clang::Expr *expr) {
  SgType *type = nullptr;
  if (expr != nullptr) {
    type = buildTypeFromQualifiedType(expr->getType());
  }
  return buildFallbackExpression(type);
}

SgExpression *ClangToSageTranslator::buildFallbackExpression(SgType *type) {
  SgExpression *expr = nullptr;
  if (type == nullptr) {
    expr = SageBuilder::buildIntVal(0);
  } else {
    SgType *stripped = type->stripType(SgType::STRIP_MODIFIER_TYPE |
                                       SgType::STRIP_TYPEDEF_TYPE);
    if (isSgTypeNullptr(stripped) != nullptr) {
      expr = SageBuilder::buildNullptrValExp();
    } else if (SgReferenceType *ref_type = isSgReferenceType(stripped)) {
      SgType *base_type = ref_type->get_base_type();
      if (base_type == nullptr) {
        base_type = SageBuilder::buildUnknownType();
      }
      SgType *ptr_type = SageBuilder::buildPointerType(base_type);
      SgExpression *zero = SageBuilder::buildIntVal(0);
      SgExpression *cast = SageBuilder::buildCastExp(zero, ptr_type);
      expr = SageBuilder::buildPointerDerefExp(cast);
    } else if (isSgTypeUnknown(stripped) != nullptr) {
      expr = SageBuilder::buildIntVal(0);
    } else if (SageInterface::isScalarType(stripped) ||
               SageInterface::isPointerType(stripped) ||
               isSgEnumType(stripped)) {
      expr = SageBuilder::buildCastExp(SageBuilder::buildIntVal(0), type);
    } else {
      bool class_unknown = false;
      if (isSgTypedefType(type) == nullptr && isSgClassType(type) == nullptr) {
        class_unknown = true;
      }
      SgExprListExp *args = SageBuilder::buildExprListExp_nfi();
      expr = SageBuilder::buildConstructorInitializer_nfi(
          nullptr, args, type, false, false, false, class_unknown);
    }
  }

  if (expr != nullptr) {
    setCompilerGeneratedFileInfo(expr, false);
  }
  return expr;
}

size_t ClangToSageTranslator::countExplicitTemplateArgumentsFromSource(
    clang::SourceRange range) const {
  if (p_compiler_instance == nullptr) {
    return 0;
  }
  clang::SourceManager &sm = p_compiler_instance->getSourceManager();
  const clang::LangOptions &lang_opts = p_compiler_instance->getLangOpts();
  ExplicitTemplateArgumentSourceInfo source_info =
      scanExplicitTemplateArgumentsInSourceRange(range, sm, lang_opts);
  return source_info.has_template_argument_list ? source_info.argument_count
                                                : 0;
}

void ClangToSageTranslator::applySourceRangeWithTrailingSemicolon(
    SgNode *rose_node, const clang::Stmt *clang_stmt) {
  if (rose_node == nullptr || clang_stmt == nullptr ||
      p_compiler_instance == nullptr) {
    return;
  }

  clang::SourceRange range = clang_stmt->getSourceRange();
  range = extendSourceRangeWithTrailingSemicolon(
      range, p_compiler_instance->getSourceManager(),
      p_compiler_instance->getLangOpts());
  applySourceRange(rose_node, range);
}

SgNonrealRefExp *
ClangToSageTranslator::buildNonrealRefExpFromNestedNameSpecifier(
    clang::NestedNameSpecifier qualifier, SgScopeStatement *scope,
    const SgName &terminalName, bool terminalHasTemplateKeyword,
    const SgTemplateArgumentPtrList *terminalTemplateArgs) {
  const SgName normalized = normalizeOperatorTerminalName(terminalName);
  SgNonrealType *nrtype = buildNonrealTypeFromNestedNameSpecifier(
      qualifier, scope, normalized, terminalTemplateArgs);
  ROSE_ASSERT(nrtype != nullptr);

  SgNonrealDecl *nrdecl = isSgNonrealDecl(nrtype->get_declaration());
  ROSE_ASSERT(nrdecl != nullptr);

  if (terminalHasTemplateKeyword) {
    nrdecl->set_has_template_keyword(true);
  }

  SgNonrealSymbol *sym =
      isSgNonrealSymbol(nrdecl->get_symbol_from_symbol_table());
  ROSE_ASSERT(sym != nullptr);
  SgNonrealRefExp *ref = SageBuilder::buildNonrealRefExp_nfi(sym);
  if (ref != nullptr && terminalTemplateArgs != nullptr &&
      !terminalTemplateArgs->empty()) {
    ref->get_templateArguments() = *terminalTemplateArgs;
    SageBuilder::setTemplateArgumentParents(ref);
  }
  return ref;
}

SgNode *ClangToSageTranslator::Traverse(clang::Stmt *stmt) {
  if (stmt == nullptr)
    return nullptr;

  if (isa<clang::OMPExecutableDirective>(stmt) ||
      isa<clang::ArraySectionExpr>(stmt)) {
    rejectClangOpenMPStmt(stmt);
  }

  std::map<clang::Stmt *, SgNode *>::iterator it =
      p_stmt_translation_map.find(stmt);
  if (it != p_stmt_translation_map.end()) {
    if (SgExpression *expr = isSgExpression(it->second)) {
      // Clang can reuse expression subtrees across template patterns,
      // instantiations, and other lowered wrapper nodes. ROSE expressions
      // must remain a tree with a single owning parent, so cached expressions
      // need copy-on-reuse rather than pointer reuse.
      return SageInterface::copyExpression(expr);
    }
    if (SgStatement *sg_stmt = isSgStatement(it->second)) {
      switch (stmt->getStmtClass()) {
      case clang::Stmt::LabelStmtClass:
      case clang::Stmt::CaseStmtClass:
      case clang::Stmt::DefaultStmtClass:
        // These statements are referenced from elsewhere in the AST (e.g.,
        // goto/switch targets), so lookups must continue to resolve to the
        // original node identity.
        return sg_stmt;
      default:
        break;
      }

      if (sg_stmt->get_parent() != nullptr) {
        // Non-target statements must not be shared across distinct owning
        // contexts such as template instantiations or OpenMP wrapper nodes.
        return SageInterface::copyStatement(sg_stmt);
      }
    }
    return it->second;
  }

  SgNode *result = nullptr;
  bool ret_status = false;

  switch (stmt->getStmtClass()) {
  case clang::Stmt::GCCAsmStmtClass:
    ret_status = VisitGCCAsmStmt((clang::GCCAsmStmt *)stmt, &result);
    ROSE_ASSERT(result != nullptr);
    break;
  case clang::Stmt::MSAsmStmtClass:
    ret_status = VisitMSAsmStmt((clang::MSAsmStmt *)stmt, &result);
    ROSE_ASSERT(result != nullptr);
    break;
  case clang::Stmt::BreakStmtClass:
    ret_status = VisitBreakStmt((clang::BreakStmt *)stmt, &result);
    ROSE_ASSERT(result != nullptr);
    break;
  case clang::Stmt::CapturedStmtClass:
    ret_status = VisitCapturedStmt((clang::CapturedStmt *)stmt, &result);
    ROSE_ASSERT(result != nullptr);
    break;
  case clang::Stmt::CompoundStmtClass:
    ret_status = VisitCompoundStmt((clang::CompoundStmt *)stmt, &result);
    ROSE_ASSERT(result != nullptr);
    break;
  case clang::Stmt::ContinueStmtClass:
    ret_status = VisitContinueStmt((clang::ContinueStmt *)stmt, &result);
    ROSE_ASSERT(result != nullptr);
    break;
  case clang::Stmt::CoreturnStmtClass:
    ret_status = VisitCoreturnStmt((clang::CoreturnStmt *)stmt, &result);
    ROSE_ASSERT(result != nullptr);
    break;
  case clang::Stmt::CoroutineBodyStmtClass:
    ret_status =
        VisitCoroutineBodyStmt((clang::CoroutineBodyStmt *)stmt, &result);
    ROSE_ASSERT(result != nullptr);
    break;
  case clang::Stmt::CXXCatchStmtClass:
    ret_status = VisitCXXCatchStmt((clang::CXXCatchStmt *)stmt, &result);
    ROSE_ASSERT(result != nullptr);
    break;
  case clang::Stmt::CXXForRangeStmtClass:
    ret_status = VisitCXXForRangeStmt((clang::CXXForRangeStmt *)stmt, &result);
    ROSE_ASSERT(result != nullptr);
    break;
  case clang::Stmt::CXXTryStmtClass:
    ret_status = VisitCXXTryStmt((clang::CXXTryStmt *)stmt, &result);
    ROSE_ASSERT(result != nullptr);
    break;
  case clang::Stmt::DeclStmtClass:
    ret_status = VisitDeclStmt((clang::DeclStmt *)stmt, &result);
    ROSE_ASSERT(result != nullptr);
    break;
  case clang::Stmt::DoStmtClass:
    ret_status = VisitDoStmt((clang::DoStmt *)stmt, &result);
    ROSE_ASSERT(result != nullptr);
    break;
  case clang::Stmt::ForStmtClass:
    ret_status = VisitForStmt((clang::ForStmt *)stmt, &result);
    ROSE_ASSERT(result != nullptr);
    break;
  case clang::Stmt::GotoStmtClass:
    ret_status = VisitGotoStmt((clang::GotoStmt *)stmt, &result);
    ROSE_ASSERT(result != nullptr);
    break;
  case clang::Stmt::IfStmtClass:
    ret_status = VisitIfStmt((clang::IfStmt *)stmt, &result);
    ROSE_ASSERT(result != nullptr);
    break;
  case clang::Stmt::IndirectGotoStmtClass:
    ret_status =
        VisitIndirectGotoStmt((clang::IndirectGotoStmt *)stmt, &result);
    ROSE_ASSERT(result != nullptr);
    break;
  case clang::Stmt::MSDependentExistsStmtClass:
    ret_status = VisitMSDependentExistsStmt(
        (clang::MSDependentExistsStmt *)stmt, &result);
    ROSE_ASSERT(result != nullptr);
    break;
  case clang::Stmt::NullStmtClass:
    ret_status = VisitNullStmt((clang::NullStmt *)stmt, &result);
    ROSE_ASSERT(result != nullptr);
    break;
  case clang::Stmt::ReturnStmtClass:
    ret_status = VisitReturnStmt((clang::ReturnStmt *)stmt, &result);
    ROSE_ASSERT(result != nullptr);
    break;
  case clang::Stmt::SEHExceptStmtClass:
    ret_status = VisitSEHExceptStmt((clang::SEHExceptStmt *)stmt, &result);
    ROSE_ASSERT(result != nullptr);
    break;
  case clang::Stmt::SEHFinallyStmtClass:
    ret_status = VisitSEHFinallyStmt((clang::SEHFinallyStmt *)stmt, &result);
    ROSE_ASSERT(result != nullptr);
    break;
  case clang::Stmt::SEHLeaveStmtClass:
    ret_status = VisitSEHLeaveStmt((clang::SEHLeaveStmt *)stmt, &result);
    ROSE_ASSERT(result != nullptr);
    break;
  case clang::Stmt::SEHTryStmtClass:
    ret_status = VisitSEHTryStmt((clang::SEHTryStmt *)stmt, &result);
    ROSE_ASSERT(result != nullptr);
    break;
  case clang::Stmt::CaseStmtClass:
    ret_status = VisitCaseStmt((clang::CaseStmt *)stmt, &result);
    ROSE_ASSERT(result != nullptr);
    break;
  case clang::Stmt::DefaultStmtClass:
    ret_status = VisitDefaultStmt((clang::DefaultStmt *)stmt, &result);
    ROSE_ASSERT(result != nullptr);
    break;
  case clang::Stmt::SwitchStmtClass:
    ret_status = VisitSwitchStmt((clang::SwitchStmt *)stmt, &result);
    ROSE_ASSERT(result != nullptr);
    break;
  case clang::Stmt::AttributedStmtClass:
    ret_status = VisitAttributedStmt((clang::AttributedStmt *)stmt, &result);
    ROSE_ASSERT(result != nullptr);
    break;
  case clang::Stmt::BinaryConditionalOperatorClass:
    ret_status = VisitBinaryConditionalOperator(
        (clang::BinaryConditionalOperator *)stmt, &result);
    ROSE_ASSERT(result != nullptr);
    break;
  case clang::Stmt::ConditionalOperatorClass:
    ret_status =
        VisitConditionalOperator((clang::ConditionalOperator *)stmt, &result);
    ROSE_ASSERT(result != nullptr);
    break;
  case clang::Stmt::AddrLabelExprClass:
    ret_status = VisitAddrLabelExpr((clang::AddrLabelExpr *)stmt, &result);
    ROSE_ASSERT(result != nullptr);
    break;
  case clang::Stmt::ArrayInitIndexExprClass:
    ret_status =
        VisitArrayInitIndexExpr((clang::ArrayInitIndexExpr *)stmt, &result);
    ROSE_ASSERT(result != nullptr);
    break;
  case clang::Stmt::ArrayInitLoopExprClass:
    ret_status =
        VisitArrayInitLoopExpr((clang::ArrayInitLoopExpr *)stmt, &result);
    ROSE_ASSERT(result != nullptr);
    break;
  case clang::Stmt::ArraySubscriptExprClass:
    ret_status =
        VisitArraySubscriptExpr((clang::ArraySubscriptExpr *)stmt, &result);
    ROSE_ASSERT(result != nullptr);
    break;
  case clang::Stmt::ArrayTypeTraitExprClass:
    ret_status =
        VisitArrayTypeTraitExpr((clang::ArrayTypeTraitExpr *)stmt, &result);
    ROSE_ASSERT(result != nullptr);
    break;
  case clang::Stmt::AsTypeExprClass:
    ret_status = VisitAsTypeExpr((clang::AsTypeExpr *)stmt, &result);
    ROSE_ASSERT(result != nullptr);
    break;
  case clang::Stmt::AtomicExprClass:
    ret_status = VisitAtomicExpr((clang::AtomicExpr *)stmt, &result);
    ROSE_ASSERT(result != nullptr);
    break;
  case clang::Stmt::CompoundAssignOperatorClass:
    ret_status = VisitCompoundAssignOperator(
        (clang::CompoundAssignOperator *)stmt, &result);
    ROSE_ASSERT(result != nullptr);
    break;
  case clang::Stmt::BlockExprClass:
    ret_status = VisitBlockExpr((clang::BlockExpr *)stmt, &result);
    ROSE_ASSERT(result != nullptr);
    break;
  case clang::Stmt::CUDAKernelCallExprClass:
    ret_status =
        VisitCUDAKernelCallExpr((clang::CUDAKernelCallExpr *)stmt, &result);
    ROSE_ASSERT(result != nullptr);
    break;
  case clang::Stmt::CXXMemberCallExprClass:
    ret_status =
        VisitCXXMemberCallExpr((clang::CXXMemberCallExpr *)stmt, &result);
    ROSE_ASSERT(result != nullptr);
    break;
  case clang::Stmt::CXXOperatorCallExprClass:
    ret_status =
        VisitCXXOperatorCallExpr((clang::CXXOperatorCallExpr *)stmt, &result);
    ROSE_ASSERT(result != nullptr);
    break;
  case clang::Stmt::UserDefinedLiteralClass:
    ret_status =
        VisitUserDefinedLiteral((clang::UserDefinedLiteral *)stmt, &result);
    ROSE_ASSERT(result != nullptr);
    break;
  case clang::Stmt::BuiltinBitCastExprClass:
    ret_status =
        VisitBuiltinBitCastExpr((clang::BuiltinBitCastExpr *)stmt, &result);
    ROSE_ASSERT(result != nullptr);
    break;
  case clang::Stmt::CStyleCastExprClass:
    ret_status = VisitCStyleCastExpr((clang::CStyleCastExpr *)stmt, &result);
    ROSE_ASSERT(result != nullptr);
    break;
  case clang::Stmt::CXXFunctionalCastExprClass:
    ret_status = VisitCXXFunctionalCastExpr(
        (clang::CXXFunctionalCastExpr *)stmt, &result);
    ROSE_ASSERT(result != nullptr);
    break;
  case clang::Stmt::CXXConstCastExprClass:
    ret_status =
        VisitCXXConstCastExpr((clang::CXXConstCastExpr *)stmt, &result);
    ROSE_ASSERT(result != nullptr);
    break;
  case clang::Stmt::CXXDynamicCastExprClass:
    ret_status =
        VisitCXXDynamicCastExpr((clang::CXXDynamicCastExpr *)stmt, &result);
    ROSE_ASSERT(result != nullptr);
    break;
  case clang::Stmt::CXXReinterpretCastExprClass:
    ret_status = VisitCXXReinterpretCastExpr(
        (clang::CXXReinterpretCastExpr *)stmt, &result);
    ROSE_ASSERT(result != nullptr);
    break;
  case clang::Stmt::CXXStaticCastExprClass:
    ret_status =
        VisitCXXStaticCastExpr((clang::CXXStaticCastExpr *)stmt, &result);
    ROSE_ASSERT(result != nullptr);
    break;
  case clang::Stmt::ImplicitCastExprClass:
    ret_status =
        VisitImplicitCastExpr((clang::ImplicitCastExpr *)stmt, &result);
    ROSE_ASSERT(result != nullptr);
    break;
  case clang::Stmt::CharacterLiteralClass:
    ret_status =
        VisitCharacterLiteral((clang::CharacterLiteral *)stmt, &result);
    ROSE_ASSERT(result != nullptr);
    break;
  case clang::Stmt::ChooseExprClass:
    ret_status = VisitChooseExpr((clang::ChooseExpr *)stmt, &result);
    ROSE_ASSERT(result != nullptr);
    break;
  case clang::Stmt::CompoundLiteralExprClass:
    ret_status =
        VisitCompoundLiteralExpr((clang::CompoundLiteralExpr *)stmt, &result);
    ROSE_ASSERT(result != nullptr);
    break;
  case clang::Stmt::ConceptSpecializationExprClass:
    ret_status = VisitConceptSpecializationExpr(
        (clang::ConceptSpecializationExpr *)stmt, &result);
    ROSE_ASSERT(result != nullptr);
    break;
  case clang::Stmt::ConvertVectorExprClass:
    ret_status =
        VisitConvertVectorExpr((clang::ConvertVectorExpr *)stmt, &result);
    ROSE_ASSERT(result != nullptr);
    break;
  case clang::Stmt::CoawaitExprClass:
    ret_status = VisitCoawaitExpr((clang::CoawaitExpr *)stmt, &result);
    ROSE_ASSERT(result != nullptr);
    break;
  case clang::Stmt::CoyieldExprClass:
    ret_status = VisitCoyieldExpr((clang::CoyieldExpr *)stmt, &result);
    ROSE_ASSERT(result != nullptr);
    break;
  case clang::Stmt::CXXBindTemporaryExprClass:
    ret_status =
        VisitCXXBindTemporaryExpr((clang::CXXBindTemporaryExpr *)stmt, &result);
    ROSE_ASSERT(result != nullptr);
    break;
  case clang::Stmt::CXXBoolLiteralExprClass:
    ret_status =
        VisitCXXBoolLiteralExpr((clang::CXXBoolLiteralExpr *)stmt, &result);
    ROSE_ASSERT(result != nullptr);
    break;
  case clang::Stmt::CXXConstructExprClass:
    ret_status =
        VisitCXXConstructExpr((clang::CXXConstructExpr *)stmt, &result);
    ROSE_ASSERT(result != nullptr);
    break;
  case clang::Stmt::CXXTemporaryObjectExprClass:
    ret_status = VisitCXXTemporaryObjectExpr(
        (clang::CXXTemporaryObjectExpr *)stmt, &result);
    ROSE_ASSERT(result != nullptr);
    break;
  case clang::Stmt::CXXParenListInitExprClass:
    ret_status =
        VisitCXXParenListInitExpr((clang::CXXParenListInitExpr *)stmt, &result);
    ROSE_ASSERT(result != nullptr);
    break;
  case clang::Stmt::CXXDefaultArgExprClass:
    ret_status =
        VisitCXXDefaultArgExpr((clang::CXXDefaultArgExpr *)stmt, &result);
    ROSE_ASSERT(result != nullptr);
    break;
  case clang::Stmt::CXXDefaultInitExprClass:
    ret_status =
        VisitCXXDefaultInitExpr((clang::CXXDefaultInitExpr *)stmt, &result);
    ROSE_ASSERT(result != nullptr);
    break;
  case clang::Stmt::CXXDeleteExprClass:
    ret_status = VisitCXXDeleteExpr((clang::CXXDeleteExpr *)stmt, &result);
    ROSE_ASSERT(result != nullptr);
    break;
  case clang::Stmt::CXXDependentScopeMemberExprClass:
    ret_status = VisitCXXDependentScopeMemberExpr(
        (clang::CXXDependentScopeMemberExpr *)stmt, &result);
    ROSE_ASSERT(result != nullptr);
    break;
  case clang::Stmt::CXXFoldExprClass:
    ret_status = VisitCXXFoldExpr((clang::CXXFoldExpr *)stmt, &result);
    ROSE_ASSERT(result != nullptr);
    break;
  case clang::Stmt::CXXInheritedCtorInitExprClass:
    ret_status = VisitCXXInheritedCtorInitExpr(
        (clang::CXXInheritedCtorInitExpr *)stmt, &result);
    ROSE_ASSERT(result != nullptr);
    break;
  case clang::Stmt::CXXNewExprClass:
    ret_status = VisitCXXNewExpr((clang::CXXNewExpr *)stmt, &result);
    ROSE_ASSERT(result != nullptr);
    break;
  case clang::Stmt::CXXNoexceptExprClass:
    ret_status = VisitCXXNoexceptExpr((clang::CXXNoexceptExpr *)stmt, &result);
    ROSE_ASSERT(result != nullptr);
    break;
  case clang::Stmt::CXXNullPtrLiteralExprClass:
    ret_status = VisitCXXNullPtrLiteralExpr(
        (clang::CXXNullPtrLiteralExpr *)stmt, &result);
    ROSE_ASSERT(result != nullptr);
    break;
  case clang::Stmt::CXXPseudoDestructorExprClass:
    ret_status = VisitCXXPseudoDestructorExpr(
        (clang::CXXPseudoDestructorExpr *)stmt, &result);
    ROSE_ASSERT(result != nullptr);
    break;
  case clang::Stmt::CXXRewrittenBinaryOperatorClass:
    ret_status = VisitCXXRewrittenBinaryOperator(
        (clang::CXXRewrittenBinaryOperator *)stmt, &result);
    ROSE_ASSERT(result != nullptr);
    break;
  case clang::Stmt::CXXScalarValueInitExprClass:
    ret_status = VisitCXXScalarValueInitExpr(
        (clang::CXXScalarValueInitExpr *)stmt, &result);
    ROSE_ASSERT(result != nullptr);
    break;
  case clang::Stmt::CXXStdInitializerListExprClass:
    ret_status = VisitCXXStdInitializerListExpr(
        (clang::CXXStdInitializerListExpr *)stmt, &result);
    ROSE_ASSERT(result != nullptr);
    break;
  case clang::Stmt::CXXThisExprClass:
    ret_status = VisitCXXThisExpr((clang::CXXThisExpr *)stmt, &result);
    ROSE_ASSERT(result != nullptr);
    break;
  case clang::Stmt::CXXThrowExprClass:
    ret_status = VisitCXXThrowExpr((clang::CXXThrowExpr *)stmt, &result);
    ROSE_ASSERT(result != nullptr);
    break;
  case clang::Stmt::CXXTypeidExprClass:
    ret_status = VisitCXXTypeidExpr((clang::CXXTypeidExpr *)stmt, &result);
    ROSE_ASSERT(result != nullptr);
    break;
  case clang::Stmt::CXXUnresolvedConstructExprClass:
    ret_status = VisitCXXUnresolvedConstructExpr(
        (clang::CXXUnresolvedConstructExpr *)stmt, &result);
    ROSE_ASSERT(result != nullptr);
    break;
  case clang::Stmt::CXXUuidofExprClass:
    ret_status = VisitCXXUuidofExpr((clang::CXXUuidofExpr *)stmt, &result);
    ROSE_ASSERT(result != nullptr);
    break;
  case clang::Stmt::DeclRefExprClass:
    ret_status = VisitDeclRefExpr((clang::DeclRefExpr *)stmt, &result);
    ROSE_ASSERT(result != nullptr);
    break;
  case clang::Stmt::DependentCoawaitExprClass:
    ret_status =
        VisitDependentCoawaitExpr((clang::DependentCoawaitExpr *)stmt, &result);
    ROSE_ASSERT(result != nullptr);
    break;
  case clang::Stmt::DependentScopeDeclRefExprClass:
    ret_status = VisitDependentScopeDeclRefExpr(
        (clang::DependentScopeDeclRefExpr *)stmt, &result);
    ROSE_ASSERT(result != nullptr);
    break;
  case clang::Stmt::DesignatedInitExprClass:
    ret_status =
        VisitDesignatedInitExpr((clang::DesignatedInitExpr *)stmt, &result);
    ROSE_ASSERT(result != nullptr);
    break;
  case clang::Stmt::DesignatedInitUpdateExprClass:
    ret_status = VisitDesignatedInitUpdateExpr(
        (clang::DesignatedInitUpdateExpr *)stmt, &result);
    ROSE_ASSERT(result != nullptr);
    break;
  case clang::Stmt::ExpressionTraitExprClass:
    ret_status =
        VisitExpressionTraitExpr((clang::ExpressionTraitExpr *)stmt, &result);
    ROSE_ASSERT(result != nullptr);
    break;
  case clang::Stmt::ExtVectorElementExprClass:
    ret_status =
        VisitExtVectorElementExpr((clang::ExtVectorElementExpr *)stmt, &result);
    ROSE_ASSERT(result != nullptr);
    break;
  case clang::Stmt::FixedPointLiteralClass:
    ret_status =
        VisitFixedPointLiteral((clang::FixedPointLiteral *)stmt, &result);
    ROSE_ASSERT(result != nullptr);
    break;
  case clang::Stmt::FloatingLiteralClass:
    ret_status = VisitFloatingLiteral((clang::FloatingLiteral *)stmt, &result);
    ROSE_ASSERT(result != nullptr);
    break;
  case clang::Stmt::ConstantExprClass:
    ret_status = VisitConstantExpr((clang::ConstantExpr *)stmt, &result);
    ROSE_ASSERT(result != nullptr);
    break;
  case clang::Stmt::ExprWithCleanupsClass:
    ret_status =
        VisitExprWithCleanups((clang::ExprWithCleanups *)stmt, &result);
    ROSE_ASSERT(result != nullptr);
    break;
  case clang::Stmt::FunctionParmPackExprClass:
    ret_status =
        VisitFunctionParmPackExpr((clang::FunctionParmPackExpr *)stmt, &result);
    ROSE_ASSERT(result != nullptr);
    break;
  case clang::Stmt::GenericSelectionExprClass:
    ret_status =
        VisitGenericSelectionExpr((clang::GenericSelectionExpr *)stmt, &result);
    ROSE_ASSERT(result != nullptr);
    break;
  case clang::Stmt::GNUNullExprClass:
    ret_status = VisitGNUNullExpr((clang::GNUNullExpr *)stmt, &result);
    ROSE_ASSERT(result != nullptr);
    break;
  case clang::Stmt::ImaginaryLiteralClass:
    ret_status =
        VisitImaginaryLiteral((clang::ImaginaryLiteral *)stmt, &result);
    ROSE_ASSERT(result != nullptr);
    break;
  case clang::Stmt::ImplicitValueInitExprClass:
    ret_status = VisitImplicitValueInitExpr(
        (clang::ImplicitValueInitExpr *)stmt, &result);
    ROSE_ASSERT(result != nullptr);
    break;
  case clang::Stmt::InitListExprClass:
    ret_status = VisitInitListExpr((clang::InitListExpr *)stmt, &result);
    ROSE_ASSERT(result != nullptr);
    break;
  case clang::Stmt::IntegerLiteralClass:
    ret_status = VisitIntegerLiteral((clang::IntegerLiteral *)stmt, &result);
    ROSE_ASSERT(result != nullptr);
    break;
  case clang::Stmt::LambdaExprClass:
    ret_status = VisitLambdaExpr((clang::LambdaExpr *)stmt, &result);
    ROSE_ASSERT(result != nullptr);
    break;
  case clang::Stmt::MaterializeTemporaryExprClass:
    ret_status = VisitMaterializeTemporaryExpr(
        (clang::MaterializeTemporaryExpr *)stmt, &result);
    ROSE_ASSERT(result != nullptr);
    break;
  case clang::Stmt::MemberExprClass:
    ret_status = VisitMemberExpr((clang::MemberExpr *)stmt, &result);
    ROSE_ASSERT(result != nullptr);
    break;
  case clang::Stmt::MSPropertyRefExprClass:
    ret_status =
        VisitMSPropertyRefExpr((clang::MSPropertyRefExpr *)stmt, &result);
    ROSE_ASSERT(result != nullptr);
    break;
  case clang::Stmt::MSPropertySubscriptExprClass:
    ret_status = VisitMSPropertySubscriptExpr(
        (clang::MSPropertySubscriptExpr *)stmt, &result);
    ROSE_ASSERT(result != nullptr);
    break;
  case clang::Stmt::NoInitExprClass:
    ret_status = VisitNoInitExpr((clang::NoInitExpr *)stmt, &result);
    ROSE_ASSERT(result != nullptr);
    break;
  case clang::Stmt::OffsetOfExprClass:
    ret_status = VisitOffsetOfExpr((clang::OffsetOfExpr *)stmt, &result);
    ROSE_ASSERT(result != nullptr);
    break;
  case clang::Stmt::OpaqueValueExprClass:
    ret_status = VisitOpaqueValueExpr((clang::OpaqueValueExpr *)stmt, &result);
    ROSE_ASSERT(result != nullptr);
    break;
  case clang::Stmt::UnresolvedLookupExprClass:
    ret_status =
        VisitUnresolvedLookupExpr((clang::UnresolvedLookupExpr *)stmt, &result);
    ROSE_ASSERT(result != nullptr);
    break;
  case clang::Stmt::UnresolvedMemberExprClass:
    ret_status =
        VisitUnresolvedMemberExpr((clang::UnresolvedMemberExpr *)stmt, &result);
    ROSE_ASSERT(result != nullptr);
    break;
  case clang::Stmt::PackExpansionExprClass:
    ret_status =
        VisitPackExpansionExpr((clang::PackExpansionExpr *)stmt, &result);
    ROSE_ASSERT(result != nullptr);
    break;
  case clang::Stmt::ParenExprClass:
    ret_status = VisitParenExpr((clang::ParenExpr *)stmt, &result);
    ROSE_ASSERT(result != nullptr);
    break;
  case clang::Stmt::ParenListExprClass:
    ret_status = VisitParenListExpr((clang::ParenListExpr *)stmt, &result);
    ROSE_ASSERT(result != nullptr);
    break;
  case clang::Stmt::PredefinedExprClass:
    ret_status = VisitPredefinedExpr((clang::PredefinedExpr *)stmt, &result);
    ROSE_ASSERT(result != nullptr);
    break;
  case clang::Stmt::PseudoObjectExprClass:
    ret_status =
        VisitPseudoObjectExpr((clang::PseudoObjectExpr *)stmt, &result);
    ROSE_ASSERT(result != nullptr);
    break;
  case clang::Stmt::ShuffleVectorExprClass:
    ret_status =
        VisitShuffleVectorExpr((clang::ShuffleVectorExpr *)stmt, &result);
    ROSE_ASSERT(result != nullptr);
    break;
  case clang::Stmt::SizeOfPackExprClass:
    ret_status = VisitSizeOfPackExpr((clang::SizeOfPackExpr *)stmt, &result);
    ROSE_ASSERT(result != nullptr);
    break;
  case clang::Stmt::SourceLocExprClass:
    ret_status = VisitSourceLocExpr((clang::SourceLocExpr *)stmt, &result);
    ROSE_ASSERT(result != nullptr);
    break;
  case clang::Stmt::StmtExprClass:
    ret_status = VisitStmtExpr((clang::StmtExpr *)stmt, &result);
    ROSE_ASSERT(result != nullptr);
    break;
  case clang::Stmt::StringLiteralClass:
    ret_status = VisitStringLiteral((clang::StringLiteral *)stmt, &result);
    ROSE_ASSERT(result != nullptr);
    break;
  case clang::Stmt::SubstNonTypeTemplateParmPackExprClass:
    ret_status = VisitSubstNonTypeTemplateParmPackExpr(
        (clang::SubstNonTypeTemplateParmPackExpr *)stmt, &result);
    ROSE_ASSERT(result != nullptr);
    break;
  case clang::Stmt::SubstNonTypeTemplateParmExprClass:
    ret_status = VisitSubstNonTypeTemplateParmExpr(
        (clang::SubstNonTypeTemplateParmExpr *)stmt, &result);
    ROSE_ASSERT(result != nullptr);
    break;
  case clang::Stmt::TypeTraitExprClass:
    ret_status = VisitTypeTraitExpr((clang::TypeTraitExpr *)stmt, &result);
    ROSE_ASSERT(result != nullptr);
    break;
  // TypoExpr was removed in LLVM
  // case clang::Stmt::TypoExprClass:
  //     ret_status = VisitTypoExpr((clang::TypoExpr *)stmt, &result);
  //     ROSE_ASSERT(result != nullptr);
  //     break;
  case clang::Stmt::UnaryExprOrTypeTraitExprClass:
    ret_status = VisitUnaryExprOrTypeTraitExpr(
        (clang::UnaryExprOrTypeTraitExpr *)stmt, &result);
    ROSE_ASSERT(result != nullptr);
    break;
  case clang::Stmt::VAArgExprClass:
    ret_status = VisitVAArgExpr((clang::VAArgExpr *)stmt, &result);
    ROSE_ASSERT(result != nullptr);
    break;
  case clang::Stmt::LabelStmtClass:
    ret_status = VisitLabelStmt((clang::LabelStmt *)stmt, &result);
    ROSE_ASSERT(result != nullptr);
    break;
  case clang::Stmt::WhileStmtClass:
    ret_status = VisitWhileStmt((clang::WhileStmt *)stmt, &result);
    ROSE_ASSERT(result != nullptr);
    break;
  case clang::Stmt::UnaryOperatorClass:
    ret_status = VisitUnaryOperator((clang::UnaryOperator *)stmt, &result);
    ROSE_ASSERT(result != nullptr);
    break;
  case clang::Stmt::CallExprClass:
    ret_status = VisitCallExpr((clang::CallExpr *)stmt, &result);
    ROSE_ASSERT(result != nullptr);
    break;
  case clang::Stmt::BinaryOperatorClass:
    ret_status = VisitBinaryOperator((clang::BinaryOperator *)stmt, &result);
    ROSE_ASSERT(result != nullptr);
    break;
  case clang::Stmt::RecoveryExprClass:
    // CLANG FRONTEND FIX: Use a typed fallback expression instead of
    // SgIntVal(42) for RecoveryExpr. Clang creates RecoveryExpr during parse
    // errors or incomplete template instantiations; a compiler-generated
    // fallback keeps translation moving without inventing a real value.
    result = buildFallbackExpression(static_cast<clang::RecoveryExpr *>(stmt));
    // Note: Assertion removed since RecoveryExpr is a valid (though error)
    // state during parsing ROSE_ASSERT(FAIL_FIXME == 0); // There is no concept
    // of recovery expression in ROSE
    break;
  case clang::Stmt::RequiresExprClass:
    ret_status = VisitRequiresExpr((clang::RequiresExpr *)stmt, &result);
    ROSE_ASSERT(result != nullptr);
    break;

  default:
    std::cerr << "Unknown statement kind: " << stmt->getStmtClassName() << " !"
              << std::endl;
    ROSE_ABORT();
  }

  ROSE_ASSERT(result != nullptr);

  p_stmt_translation_map.insert(
      std::pair<clang::Stmt *, SgNode *>(stmt, result));

  return result;
}

/********************/
/* Visit Statements */
/********************/

bool ClangToSageTranslator::VisitStmt(clang::Stmt *stmt, SgNode **node) {
#if DEBUG_VISIT_STMT
  std::cerr << "ClangToSageTranslator::VisitStmt" << std::endl;
#endif

  if (*node == nullptr) {
    std::cerr << "Runtime error: No Sage node associated with the Statement: "
              << stmt->getStmtClassName() << std::endl;
    stmt->dump();
    return false;
  }

  // TODO Is there anything else todo?

  if (isSgLocatedNode(*node) != nullptr &&
      (isSgLocatedNode(*node)->get_file_info() == nullptr ||
       !(isSgLocatedNode(*node)->get_file_info()->isCompilerGenerated()))) {
    clang::SourceRange range = stmt->getSourceRange();

    // Token-stream mapping expects statement extents to cover the full spelled
    // statement, including terminating semicolons when present.
    switch (stmt->getStmtClass()) {
    case clang::Stmt::ReturnStmtClass:
    case clang::Stmt::BreakStmtClass:
    case clang::Stmt::ContinueStmtClass:
    case clang::Stmt::GotoStmtClass:
    case clang::Stmt::IndirectGotoStmtClass:
    case clang::Stmt::DoStmtClass:
      if (p_compiler_instance != nullptr) {
        range = extendSourceRangeWithTrailingSemicolon(
            range, p_compiler_instance->getSourceManager(),
            p_compiler_instance->getLangOpts());
      }
      break;
    default:
      break;
    }

    applySourceRange(*node, range);
  }

  return true;
}

bool ClangToSageTranslator::VisitAsmStmt(clang::AsmStmt *asm_stmt,
                                         SgNode **node) {
#if DEBUG_VISIT_STMT
  std::cerr << "ClangToSageTranslator::VisitAsmStmt" << std::endl;
#endif
  bool res = true;

  ROSE_ASSERT(FAIL_TODO == 0); // TODO
  return VisitStmt(asm_stmt, node) && res;
}

bool ClangToSageTranslator::VisitGCCAsmStmt(clang::GCCAsmStmt *gcc_asm_stmt,
                                            SgNode **node) {
#if DEBUG_VISIT_STMT
  std::cerr << "ClangToSageTranslator::VisitGCCAsmStmt" << std::endl;
#endif
  bool res = true;

  unsigned asmNumInput = gcc_asm_stmt->getNumInputs();
  unsigned asmNumOutput = gcc_asm_stmt->getNumOutputs();
  unsigned asmClobber = gcc_asm_stmt->getNumClobbers();

  // LLVM returns std::string
  std::string AsmString;
  AsmString = gcc_asm_stmt->getAsmString();
#if DEBUG_VISIT_STMT
  std::cerr << "AsmString:" << AsmString << std::endl;
#endif

  SgAsmStmt *asmStmt = SageBuilder::buildAsmStatement(AsmString);
  asmStmt->set_firstNondefiningDeclaration(asmStmt);
  asmStmt->set_definingDeclaration(asmStmt);
  asmStmt->set_parent(SageBuilder::topScopeStack());
  asmStmt->set_useGnuExtendedFormat(true);

  // Pei-Hung (03/22/2022)  The clobber string is available.
  // The implementation adding clobber into ROSE AST is not in place.
  for (unsigned i = 0; i < asmClobber; ++i) {
    std::string clobberStr =
        static_cast<std::string>(gcc_asm_stmt->getClobber(i));
#if DEBUG_VISIT_STMT
    std::cerr << "AsmOp clobber[" << i << "]: " << clobberStr << std::endl;
#endif
    // Pei-Hung "cc" clobber is skipped by the legacy frontend
    if (clobberStr.compare(0, sizeof(clobberStr), "cc") == 0)
      continue;

    SgInitializedName::asm_register_name_enum sageRegisterName =
        get_sgAsmRegister(clobberStr);
    asmStmt->get_clobberRegisterList().push_back(sageRegisterName);
  }

  // Pei-Hung (03/22/2022) use regular expression to check the first modifier, +
  // and =, for ouput Ops. Then the second modifier for both input and output
  // Ops.  The rest is for constraints. regex_match should report 4 matched
  // results:
  // 1. the whole matched string
  // 2. first modifier: =, +, or empty
  // 3. second modifier: empty or &, %, *, #, ?, !
  // 4. The constraint
  std::regex e("([\\=\\+]*)([\\&\\%\\*\\#\\?\\!]*)(.+)",
               std::regex_constants::ECMAScript | std::regex_constants::icase);

  // process output
  for (unsigned i = 0; i < asmNumOutput; ++i) {
    SgNode *tmp_node = Traverse(gcc_asm_stmt->getOutputExpr(i));
    SgExpression *outputExpr = isSgExpression(tmp_node);
    ROSE_ASSERT(outputExpr != nullptr);

    std::string outputConstraintStr =
        static_cast<std::string>(gcc_asm_stmt->getOutputConstraint(i));
// Clang's constraint is equivalent to ROSE's modifier + operand constraints
#if DEBUG_VISIT_STMT
    std::cerr << "AsmOp output constraint[" << i << "]: " << outputConstraintStr
              << std::endl;
#endif

    std::smatch sm;
    std::regex_match(outputConstraintStr, sm, e);
#if DEBUG_VISIT_STMT
    std::cout << "string literal: " << outputConstraintStr << "  with "
              << sm.size() << " matches\n";
    if (sm.size())
      std::cout << "the matches were: ";
    for (unsigned i = 0; i < sm.size(); ++i) {
      std::cout << "[" << sm[i] << "] \n";
    }
    if (sm.size())
      std::cout << std::endl;
#endif

    SgAsmOp::asm_operand_constraint_enum constraint =
        (SgAsmOp::asm_operand_constraint_enum)SgAsmOp::e_any;
    SgAsmOp::asm_operand_modifier_enum modifiers =
        (SgAsmOp::asm_operand_modifier_enum)SgAsmOp::e_unknown;
    SgAsmOp *sageAsmOp = new SgAsmOp(constraint, modifiers, outputExpr);
    outputExpr->set_parent(sageAsmOp);
    sageAsmOp->set_recordRawAsmOperandDescriptions(true);

    // set as an output AsmOp
    sageAsmOp->set_isOutputOperand(true);
    if (llvm::StringRef outputName = gcc_asm_stmt->getOutputName(i);
        !outputName.empty()) {
      sageAsmOp->set_name(outputName.str());
    }

    ROSE_ASSERT(sm.size() == 4);

    unsigned modifierVal = static_cast<int>(modifiers);
    if (!sm[1].str().empty())
      modifierVal += static_cast<int>(get_sgAsmOperandModifier(sm[1].str()));

    if (!sm[2].str().empty())
      modifierVal += static_cast<int>(get_sgAsmOperandModifier(sm[2].str()));

    sageAsmOp->set_modifiers(
        static_cast<SgAsmOp::asm_operand_modifier_enum>(modifierVal));

    // set constraint
    sageAsmOp->set_constraint(get_sgAsmOperandConstraint(sm[3].str()));
    sageAsmOp->set_constraintString(outputConstraintStr);

    Sg_File_Info *start_fi =
        Sg_File_Info::generateDefaultFileInfoForCompilerGeneratedNode();
    start_fi->setCompilerGenerated();
    sageAsmOp->set_startOfConstruct(start_fi);

    Sg_File_Info *end_fi =
        Sg_File_Info::generateDefaultFileInfoForCompilerGeneratedNode();
    end_fi->setCompilerGenerated();
    sageAsmOp->set_endOfConstruct(end_fi);

    asmStmt->get_operands().push_back(sageAsmOp);
    sageAsmOp->set_parent(asmStmt);
  }

  // process input
  for (unsigned i = 0; i < asmNumInput; ++i) {
    SgNode *tmp_node = Traverse(gcc_asm_stmt->getInputExpr(i));
    SgExpression *inputExpr = isSgExpression(tmp_node);
    ROSE_ASSERT(inputExpr != nullptr);

    std::string inputConstraintStr =
        static_cast<std::string>(gcc_asm_stmt->getInputConstraint(i));
// Clang's constraint is equivalent to ROSE's modifier + operand constraints
#if DEBUG_VISIT_STMT
    std::cerr << "AsmOp input constraint[" << i << "]: " << inputConstraintStr
              << std::endl;
#endif

    std::smatch sm;
    std::regex_match(inputConstraintStr, sm, e);
#if DEBUG_VISIT_STMT
    std::cout << "string literal: " << inputConstraintStr << "  with "
              << sm.size() << " matches\n";
    if (sm.size())
      std::cout << "the matches were: ";
    for (unsigned i = 0; i < sm.size(); ++i) {
      std::cout << "[" << sm[i] << "] \n";
    }
    if (sm.size())
      std::cout << std::endl;
#endif

    SgAsmOp::asm_operand_constraint_enum constraint =
        (SgAsmOp::asm_operand_constraint_enum)SgAsmOp::e_any;
    SgAsmOp::asm_operand_modifier_enum modifiers =
        (SgAsmOp::asm_operand_modifier_enum)SgAsmOp::e_unknown;
    SgAsmOp *sageAsmOp = new SgAsmOp(constraint, modifiers, inputExpr);
    inputExpr->set_parent(sageAsmOp);
    sageAsmOp->set_recordRawAsmOperandDescriptions(true);

    // set as an input AsmOp
    sageAsmOp->set_isOutputOperand(false);
    if (llvm::StringRef inputName = gcc_asm_stmt->getInputName(i);
        !inputName.empty()) {
      sageAsmOp->set_name(inputName.str());
    }

    ROSE_ASSERT(sm.size() == 4);

    unsigned modifierVal = static_cast<int>(modifiers);

    // "+" and "=" should not be part of the input AsmOp.  Skip checking sm[1]
    // for the inputs.

    //      if(!sm[1].str().empty())
    //        modifierVal +=
    //        static_cast<int>(get_sgAsmOperandModifier(sm[1].str())); modifiers
    //        &= get_sgAsmOperandModifier(sm[1].str());

    if (!sm[2].str().empty())
      modifierVal += static_cast<int>(get_sgAsmOperandModifier(sm[2].str()));

    sageAsmOp->set_modifiers(
        static_cast<SgAsmOp::asm_operand_modifier_enum>(modifierVal));

    // set constraint
    sageAsmOp->set_constraint(get_sgAsmOperandConstraint(sm[3].str()));
    sageAsmOp->set_constraintString(inputConstraintStr);

    Sg_File_Info *start_fi =
        Sg_File_Info::generateDefaultFileInfoForCompilerGeneratedNode();
    start_fi->setCompilerGenerated();
    sageAsmOp->set_startOfConstruct(start_fi);

    Sg_File_Info *end_fi =
        Sg_File_Info::generateDefaultFileInfoForCompilerGeneratedNode();
    end_fi->setCompilerGenerated();
    sageAsmOp->set_endOfConstruct(end_fi);

    asmStmt->get_operands().push_back(sageAsmOp);
    sageAsmOp->set_parent(asmStmt);
  }
  *node = asmStmt;

  return VisitStmt(gcc_asm_stmt, node) && res;
}

bool ClangToSageTranslator::VisitMSAsmStmt(clang::MSAsmStmt *ms_asm_stmt,
                                           SgNode **node) {
#if DEBUG_VISIT_STMT
  std::cerr << "ClangToSageTranslator::VisitMSAsmStmt" << std::endl;
#endif
  bool res = true;

  return VisitStmt(ms_asm_stmt, node) && res;
}

bool ClangToSageTranslator::VisitBreakStmt(clang::BreakStmt *break_stmt,
                                           SgNode **node) {
#if DEBUG_VISIT_STMT
  std::cerr << "ClangToSageTranslator::VisitBreakStmt" << std::endl;
#endif

  *node = SageBuilder::buildBreakStmt();
  return VisitStmt(break_stmt, node);
}

bool ClangToSageTranslator::VisitCapturedStmt(
    clang::CapturedStmt *captured_stmt, SgNode **node) {
#if DEBUG_VISIT_STMT
  std::cerr << "ClangToSageTranslator::VisitCapturedStmt" << std::endl;
#endif
  bool res = true;

  clang::Stmt *clang_body = captured_stmt->getCapturedStmt();
  SgNode *tmp_stmt = Traverse(clang_body);
  SgStatement *body = isSgStatement(tmp_stmt);

  // If the traversal returned an expression instead of a statement,
  // wrap it in an expression statement (this happens when the captured
  // region contains a single expression like a function call)
  if (body == nullptr && tmp_stmt != nullptr) {
    SgExpression *expr = isSgExpression(tmp_stmt);
    if (expr != nullptr) {
      body = SageBuilder::buildExprStatement(expr);
      if (clang_body != nullptr) {
        applySourceRange(body, clang_body->getSourceRange());
      }
    } else {
      std::cerr << "Runtime error: CapturedStmt child did not translate into "
                   "an SgStatement or SgExpression."
                << std::endl;
      res = false;
    }
  }

  if (body == nullptr) {
    body = SageBuilder::buildNullStatement();
  }

  *node = body;

  return VisitStmt(captured_stmt, node) && res;
}

static bool isStandaloneOpenMPEndDirective(const std::string &directive_text) {
  std::string trimmed = trimWhitespace(directive_text);
  if (trimmed.empty()) {
    return false;
  }

  auto lower_at = [](char c) -> char {
    return static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
  };
  auto starts_with_keyword = [&](const std::string &text,
                                 const std::string &keyword) -> bool {
    if (text.size() < keyword.size()) {
      return false;
    }
    for (size_t i = 0; i < keyword.size(); ++i) {
      if (lower_at(text[i]) != keyword[i]) {
        return false;
      }
    }
    if (text.size() == keyword.size()) {
      return true;
    }
    return std::isspace(static_cast<unsigned char>(text[keyword.size()])) != 0;
  };

  if (!starts_with_keyword(trimmed, "omp")) {
    return false;
  }
  trimmed = trimWhitespace(trimmed.substr(3));
  return starts_with_keyword(trimmed, "end");
}

bool ClangToSageTranslator::collectOpenMPPragmas(
    clang::Stmt *stmt, std::vector<CapturedPragma> &pragmas) {
  pragmas.clear();

  if (p_openmp_pragma_callback == nullptr || stmt == nullptr ||
      p_compiler_instance == nullptr) {
    return false;
  }

  clang::SourceLocation loc = stmt->getBeginLoc();
  if (!loc.isValid()) {
    return false;
  }

  clang::SourceManager &source_manager =
      p_compiler_instance->getSourceManager();
  clang::FileID file_id = source_manager.getFileID(loc);
  unsigned stmt_line = source_manager.getPresumedLineNumber(loc);
  if (stmt_line == 0) {
    return false;
  }

  bool found_any = false;
  std::string pragma_text;

  bool continue_across_multiline_directive = false;

  for (unsigned search_line = stmt_line; search_line > 0; --search_line) {

    if (p_openmp_pragma_callback->getPragmaAtLine(file_id, search_line,
                                                  pragma_text)) {
      bool is_openmp =
          p_openmp_pragma_callback->isOpenMPPragmaAtLine(file_id, search_line);
      if (is_openmp && p_consumed_openmp_lines.count(
                           std::make_pair(file_id, search_line)) != 0) {
        break;
      }
      std::string directive_text = pragma_text;
      if (is_openmp) {
        std::string extracted = extractOpenMPDirective(pragma_text);
        if (extracted.empty()) {
          continue;
        }
        if (isStandaloneOpenMPEndDirective(extracted)) {
          break;
        }
        directive_text = extracted;
        p_consumed_openmp_lines.insert(std::make_pair(file_id, search_line));
      }
      pragmas.push_back({search_line, directive_text, is_openmp});
      found_any = true;
      continue_across_multiline_directive = true;
      continue;
    }

    std::string line_content;
    if (!getLineContent(source_manager, file_id, search_line, line_content)) {
      break;
    }

    std::string trimmed = trimWhitespace(line_content);
    bool is_statement_line = (search_line == stmt_line);

    if (is_statement_line) {
      continue;
    }

    if (trimmed.empty()) {
      continue_across_multiline_directive = false;
      continue;
    }

    // Check if this line is a continuation of a multi-line pragma (ends with
    // backslash) We must check this regardless of whether we've found the
    // pragma yet, because when scanning backwards, we encounter continuation
    // lines BEFORE the pragma line itself
    if (p_openmp_pragma_callback->isContinuationLine(file_id, search_line)) {
      continue;
    }

    if (isSkippableLineBetweenPragmaAndStatement(trimmed)) {
      continue_across_multiline_directive = false;
      continue;
    }

    // Non-empty, non-continuation, non-skippable line - stop scanning
    break;
  }

  if (found_any) {
    std::reverse(pragmas.begin(), pragmas.end());
  }

  return found_any;
}

namespace {
static SgDeclarationStatementPtrList *
getScopeDeclarationListForPragma(SgScopeStatement *scope) {
  if (scope == nullptr) {
    return nullptr;
  }
  if (SgGlobal *global = isSgGlobal(scope)) {
    return &global->get_declarations();
  }
  if (SgNamespaceDefinitionStatement *ns_def =
          isSgNamespaceDefinitionStatement(scope)) {
    return &ns_def->get_declarations();
  }
  if (SgClassDefinition *class_def = isSgClassDefinition(scope)) {
    return &class_def->get_members();
  }
  if (SgTemplateClassDefinition *template_def =
          isSgTemplateClassDefinition(scope)) {
    return &template_def->get_members();
  }
  if (SgTemplateInstantiationDefn *inst_def =
          isSgTemplateInstantiationDefn(scope)) {
    return &inst_def->get_members();
  }
  if (scope->containsOnlyDeclarations()) {
    return &scope->getDeclarationList();
  }
  return nullptr;
}

static unsigned getLineForStatement(const SgStatement *stmt) {
  if (stmt == nullptr) {
    return 0;
  }
  if (Sg_File_Info *info = stmt->get_file_info()) {
    if (info->get_line() > 0) {
      return info->get_line();
    }
  }
  if (Sg_File_Info *info = stmt->get_startOfConstruct()) {
    return info->get_line();
  }
  return 0;
}

static bool filenamesMatch(const std::string &lhs, const std::string &rhs) {
  if (lhs.empty() || rhs.empty()) {
    return true;
  }
  if (lhs == rhs) {
    return true;
  }
  return Rose::StringUtility::stripPathFromFileName(lhs) ==
         Rose::StringUtility::stripPathFromFileName(rhs);
}

static std::string normalizeFilenameForPragma(const std::string &name) {
  if (name.empty()) {
    return name;
  }
  return Rose::StringUtility::stripPathFromFileName(name);
}

static bool isOpenMPPragmaString(const std::string &pragma) {
  std::string trimmed = trimWhitespace(pragma);
  if (trimmed.empty()) {
    return false;
  }
  auto lower_at = [](char c) -> char {
    return static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
  };
  if (trimmed.size() >= 3) {
    std::string prefix;
    prefix.push_back(lower_at(trimmed[0]));
    prefix.push_back(lower_at(trimmed[1]));
    prefix.push_back(lower_at(trimmed[2]));
    return (prefix == "omp" || prefix == "acc");
  }
  return false;
}

static bool getLocatedNodeFilenameAndLine(const SgLocatedNode *node,
                                          std::string &filename,
                                          unsigned &line) {
  if (node == nullptr) {
    return false;
  }
  if (Sg_File_Info *info = node->get_file_info()) {
    if (info->get_line() > 0) {
      line = info->get_line();
      filename = info->get_filenameString();
      return true;
    }
  }
  if (Sg_File_Info *info = node->get_startOfConstruct()) {
    if (info->get_line() > 0) {
      line = info->get_line();
      filename = info->get_filenameString();
      return true;
    }
  }
  return false;
}

static bool
statementBeginsAtLineStart(const clang::SourceManager &source_manager,
                           clang::SourceLocation loc) {
  if (!loc.isValid()) {
    return true;
  }

  clang::FileID file_id = source_manager.getFileID(loc);
  if (file_id.isInvalid()) {
    return true;
  }

  unsigned line = source_manager.getPresumedLineNumber(loc);
  if (line == 0) {
    return true;
  }

  unsigned column = source_manager.getPresumedColumnNumber(loc);
  if (column <= 1) {
    return true;
  }

  std::string line_content;
  if (!getLineContent(source_manager, file_id, line, line_content)) {
    return true;
  }

  unsigned prefix_len = column - 1;
  if (prefix_len > line_content.size()) {
    prefix_len = static_cast<unsigned>(line_content.size());
  }
  for (unsigned i = 0; i < prefix_len; ++i) {
    if (!std::isspace(static_cast<unsigned char>(line_content[i]))) {
      return false;
    }
  }
  return true;
}

} // namespace

void ClangToSageTranslator::appendUnattachedOpenMPPragmas() {
  if (p_openmp_pragma_callback == nullptr || p_compiler_instance == nullptr ||
      p_global_scope == nullptr || p_sage_source_file == nullptr) {
    return;
  }

  const auto &pragma_map = p_openmp_pragma_callback->getPragmaMap();
  if (pragma_map.empty()) {
    return;
  }

  struct PragmaEntry {
    clang::FileID file_id;
    unsigned line;
    std::string directive;
    std::string filename;
  };

  clang::SourceManager &source_manager =
      p_compiler_instance->getSourceManager();

  std::vector<PragmaEntry> entries;
  entries.reserve(pragma_map.size());

  for (const auto &entry : pragma_map) {
    const clang::FileID file_id = entry.first.first;
    const unsigned line = entry.first.second;
    if (!p_openmp_pragma_callback->isOpenMPPragmaAtLine(file_id, line)) {
      continue;
    }
    std::string directive = extractOpenMPDirective(entry.second);
    if (directive.empty()) {
      continue;
    }
    const bool is_standalone_end = isStandaloneOpenMPEndDirective(directive);
    const bool is_consumed = p_consumed_openmp_lines.count(entry.first) != 0;
    if (!is_standalone_end && is_consumed) {
      continue;
    }
    std::string filename;
    if (auto file_entry = source_manager.getFileEntryRefForID(file_id)) {
      filename = file_entry->getName().str();
    } else {
      filename = p_sage_source_file->getFileName();
    }
    entries.push_back({file_id, line, directive, filename});
  }

  if (entries.empty()) {
    return;
  }

  std::set<std::pair<std::string, unsigned>> existing_openmp_lines;
  {
    Rose_STL_Container<SgNode *> pragmas =
        NodeQuery::querySubTree(p_global_scope, V_SgPragmaDeclaration);
    for (SgNode *node : pragmas) {
      SgPragmaDeclaration *pragma_decl = isSgPragmaDeclaration(node);
      if (pragma_decl == nullptr) {
        continue;
      }
      SgPragma *pragma = pragma_decl->get_pragma();
      if (pragma == nullptr) {
        continue;
      }
      if (!isOpenMPPragmaString(pragma->get_pragma())) {
        continue;
      }
      std::string filename;
      unsigned line = 0;
      if (!getLocatedNodeFilenameAndLine(pragma_decl, filename, line)) {
        continue;
      }
      existing_openmp_lines.insert(
          std::make_pair(normalizeFilenameForPragma(filename), line));
    }

    Rose_STL_Container<SgNode *> stmts =
        NodeQuery::querySubTree(p_global_scope, V_SgStatement);
    for (SgNode *node : stmts) {
      SgStatement *stmt = isSgStatement(node);
      if (stmt == nullptr || !SageInterface::isOmpStatement(stmt)) {
        continue;
      }
      std::string filename;
      unsigned line = 0;
      if (!getLocatedNodeFilenameAndLine(stmt, filename, line)) {
        continue;
      }
      existing_openmp_lines.insert(
          std::make_pair(normalizeFilenameForPragma(filename), line));
    }
  }

  std::sort(entries.begin(), entries.end(),
            [](const PragmaEntry &a, const PragmaEntry &b) {
              if (a.filename != b.filename) {
                return a.filename < b.filename;
              }
              return a.line < b.line;
            });

  Rose_STL_Container<SgNode *> scopes =
      NodeQuery::querySubTree(p_global_scope, V_SgScopeStatement);

  auto find_scope_for_line = [&](const std::string &filename,
                                 unsigned line) -> SgScopeStatement * {
    SgScopeStatement *best = p_global_scope;
    unsigned best_span = std::numeric_limits<unsigned>::max();
    for (SgNode *node : scopes) {
      SgScopeStatement *scope = isSgScopeStatement(node);
      if (scope == nullptr) {
        continue;
      }
      Sg_File_Info *start = scope->get_startOfConstruct();
      if (start == nullptr || start->get_line() == 0) {
        continue;
      }
      if (!filenamesMatch(start->get_filenameString(), filename)) {
        continue;
      }
      unsigned start_line = start->get_line();
      unsigned end_line = std::numeric_limits<unsigned>::max();
      if (Sg_File_Info *end = scope->get_endOfConstruct()) {
        if (end->get_line() > 0) {
          end_line = end->get_line();
        }
      }
      if (line < start_line || line > end_line) {
        continue;
      }
      unsigned span = end_line - start_line;
      if (span < best_span) {
        best_span = span;
        best = scope;
      }
    }
    return best;
  };

  auto insert_pragma_in_scope = [&](SgPragmaDeclaration *pragma_decl,
                                    SgScopeStatement *scope,
                                    const std::string &filename,
                                    unsigned line) {
    if (pragma_decl == nullptr || scope == nullptr) {
      return;
    }
    if (SgDeclarationStatementPtrList *decls =
            getScopeDeclarationListForPragma(scope)) {
      auto insert_it = decls->end();
      auto last_same_file_it = decls->end();

      for (auto it = decls->begin(); it != decls->end(); ++it) {
        SgDeclarationStatement *decl = *it;
        if (decl == nullptr) {
          continue;
        }
        std::string decl_filename;
        unsigned decl_line = 0;
        if (!getLocatedNodeFilenameAndLine(decl, decl_filename, decl_line) ||
            decl_line == 0 || !filenamesMatch(decl_filename, filename)) {
          continue;
        }
        if (decl_line > line) {
          insert_it = it;
          break;
        }
        last_same_file_it = it;
      }

      if (insert_it == decls->end()) {
        if (last_same_file_it != decls->end()) {
          insert_it = last_same_file_it;
          ++insert_it;
        } else {
          // Avoid cross-file placement when this scope has no declarations
          // from the pragma's source file.
          insert_it = decls->end();
        }
      }

      decls->insert(insert_it, pragma_decl);
      pragma_decl->set_parent(scope);
      pragma_decl->set_scope(scope);
      return;
    }

    SgStatementPtrList &stmts = scope->getStatementList();
    auto insert_it = stmts.end();
    auto last_same_file_it = stmts.end();

    for (auto it = stmts.begin(); it != stmts.end(); ++it) {
      SgStatement *stmt = *it;
      if (stmt == nullptr) {
        continue;
      }
      std::string stmt_filename;
      unsigned stmt_line = 0;
      if (!getLocatedNodeFilenameAndLine(stmt, stmt_filename, stmt_line) ||
          stmt_line == 0 || !filenamesMatch(stmt_filename, filename)) {
        continue;
      }
      if (stmt_line > line) {
        insert_it = it;
        break;
      }
      last_same_file_it = it;
    }

    if (insert_it == stmts.end()) {
      if (last_same_file_it != stmts.end()) {
        insert_it = last_same_file_it;
        ++insert_it;
      } else {
        // Avoid cross-file placement when this scope has no statements from
        // the pragma's source file.
        insert_it = stmts.end();
      }
    }

    stmts.insert(insert_it, pragma_decl);
    pragma_decl->set_parent(scope);
    pragma_decl->set_scope(scope);
  };

  for (const auto &entry : entries) {
    SgScopeStatement *scope = find_scope_for_line(entry.filename, entry.line);
    if (scope == nullptr) {
      scope = p_global_scope;
    }

    std::string normalized_name = normalizeFilenameForPragma(entry.filename);
    if (!normalized_name.empty() && existing_openmp_lines.count(std::make_pair(
                                        normalized_name, entry.line)) > 0) {
      p_consumed_openmp_lines.insert(std::make_pair(entry.file_id, entry.line));
      continue;
    }

    SgPragmaDeclaration *pragma_decl =
        buildOpenMPPragmaDeclaration(entry.directive, entry.line, scope);
    if (pragma_decl == nullptr) {
      continue;
    }
    insert_pragma_in_scope(pragma_decl, scope, entry.filename, entry.line);
    p_consumed_openmp_lines.insert(std::make_pair(entry.file_id, entry.line));
  }
}

SgPragmaDeclaration *ClangToSageTranslator::buildOpenMPPragmaDeclaration(
    const std::string &directive, unsigned pragma_line,
    SgScopeStatement *scope) {
  if (scope == nullptr || directive.empty()) {
    return nullptr;
  }

  // Don't normalize here - OMP unparser will handle normalization
  SgPragmaDeclaration *pragma_decl =
      SageBuilder::buildPragmaDeclaration(directive, scope);

  // Use actual source file info instead of COMPILER_GENERATED so that
  // attachPreprocessingInfo() can properly attach comments relative to the
  // pragma based on source positions
  std::string filename = p_sage_source_file->getFileName();
  Sg_File_Info *start_fi = new Sg_File_Info(filename, pragma_line, 1);
  Sg_File_Info *end_fi = new Sg_File_Info(filename, pragma_line, 1);

  pragma_decl->set_file_info(start_fi);
  pragma_decl->set_startOfConstruct(start_fi);
  pragma_decl->set_endOfConstruct(end_fi);
  start_fi->set_parent(pragma_decl);
  end_fi->set_parent(pragma_decl);
  pragma_decl->set_parent(scope);

  return pragma_decl;
}

void ClangToSageTranslator::appendOpenMPPragmasBefore(clang::Stmt *stmt,
                                                      SgScopeStatement *scope) {
  if (scope == nullptr) {
    return;
  }

  if (p_compiler_instance != nullptr && stmt != nullptr) {
    clang::SourceManager &source_manager =
        p_compiler_instance->getSourceManager();
    if (!statementBeginsAtLineStart(source_manager, stmt->getBeginLoc())) {
      return;
    }
  }

  std::vector<CapturedPragma> pragmas;
  if (!collectOpenMPPragmas(stmt, pragmas)) {
    return;
  }

  for (const auto &entry : pragmas) {
    SgPragmaDeclaration *pragma_decl = nullptr;
    if (entry.is_openmp) {
      pragma_decl = buildOpenMPPragmaDeclaration(entry.text, entry.line, scope);
    } else {
      // Strip leading "#pragma" if present; SageBuilder re-adds it on unparse.
      std::string pragma_body = entry.text;
      const std::string prefix = "#pragma";
      if (pragma_body.compare(0, prefix.size(), prefix) == 0) {
        pragma_body = trimWhitespace(pragma_body.substr(prefix.size()));
      }
      pragma_decl = SageBuilder::buildPragmaDeclaration(pragma_body, scope);
      if (pragma_decl != nullptr) {
        pragma_decl->set_parent(scope);
      }
    }
    if (pragma_decl == nullptr) {
      continue;
    }

    if (SgBasicBlock *block = isSgBasicBlock(scope)) {
      block->append_statement(pragma_decl);
    } else {
      SageInterface::appendStatement(pragma_decl, scope);
    }
  }
}

SgStatement *
ClangToSageTranslator::wrapStatementWithOpenMPPragmas(clang::Stmt *stmt,
                                                      SgStatement *statement) {
  if (statement == nullptr) {
    return nullptr;
  }

  if (p_compiler_instance != nullptr && stmt != nullptr) {
    clang::SourceManager &source_manager =
        p_compiler_instance->getSourceManager();
    if (!statementBeginsAtLineStart(source_manager, stmt->getBeginLoc())) {
      return statement;
    }
  }

  std::vector<CapturedPragma> pragmas;
  if (!collectOpenMPPragmas(stmt, pragmas)) {
    return statement;
  }

  SgBasicBlock *wrapper_block = SageBuilder::buildBasicBlock();
  setCompilerGeneratedFileInfo(wrapper_block, true);

  for (const auto &entry : pragmas) {
    SgPragmaDeclaration *pragma_decl = nullptr;
    if (entry.is_openmp) {
      pragma_decl =
          buildOpenMPPragmaDeclaration(entry.text, entry.line, wrapper_block);
    } else {
      std::string pragma_body = entry.text;
      const std::string prefix = "#pragma";
      if (pragma_body.compare(0, prefix.size(), prefix) == 0) {
        pragma_body = trimWhitespace(pragma_body.substr(prefix.size()));
      }
      pragma_decl =
          SageBuilder::buildPragmaDeclaration(pragma_body, wrapper_block);
      if (pragma_decl != nullptr) {
        pragma_decl->set_parent(wrapper_block);
      }
    }
    if (pragma_decl != nullptr) {
      wrapper_block->append_statement(pragma_decl);
    }
  }

  wrapper_block->append_statement(statement);
  return wrapper_block;
}

bool ClangToSageTranslator::VisitCompoundStmt(
    clang::CompoundStmt *compound_stmt, SgNode **node) {
#if DEBUG_VISIT_STMT
  std::cerr << "ClangToSageTranslator::VisitCompoundStmt" << std::endl;
#endif

  bool res = true;

  SgBasicBlock *block = SageBuilder::buildBasicBlock();

  block->set_parent(SageBuilder::topScopeStack());

  SageBuilder::pushScopeStack(block);

  std::unordered_set<const clang::Stmt *> seen_clang_stmts;
  std::unordered_set<SgStatement *> seen_sg_stmts;

  clang::CompoundStmt::body_iterator it;
  for (it = compound_stmt->body_begin(); it != compound_stmt->body_end();
       it++) {
    clang::Stmt *child_stmt = *it;

    if (child_stmt != nullptr) {
      if (!seen_clang_stmts.insert(child_stmt).second) {
        std::cerr << "VisitCompoundStmt: duplicate clang stmt child "
                  << child_stmt->getStmtClassName() << "@" << child_stmt
                  << std::endl;
      }
    }

    appendOpenMPPragmasBefore(child_stmt, block);

    SgNode *tmp_node = Traverse(child_stmt);

#if DEBUG_VISIT_STMT
    if (tmp_node != nullptr)
      std::cerr << "In VisitCompoundStmt : child is " << tmp_node->class_name()
                << std::endl;
    else
      std::cerr << "In VisitCompoundStmt : child is nullptr" << std::endl;
#endif

    SgClassDeclaration *class_decl = isSgClassDeclaration(tmp_node);
    if (class_decl != nullptr &&
        (class_decl->get_name() == "" || class_decl->get_isUnNamed()))
      continue;
    SgEnumDeclaration *enum_decl = isSgEnumDeclaration(tmp_node);
    if (enum_decl != nullptr &&
        (enum_decl->get_name() == "" || enum_decl->get_isUnNamed()))
      continue;
#if DEBUG_VISIT_STMT
    else if (enum_decl != nullptr)
      std::cerr << "enum_decl = " << enum_decl
                << " >> name: " << enum_decl->get_name() << std::endl;
#endif

    SgStatement *stmt = isSgStatement(tmp_node);
    SgExpression *expr = isSgExpression(tmp_node);
    if (tmp_node != nullptr && stmt == nullptr && expr == nullptr) {
      std::cerr << "Runtime error: tmp_node != nullptr && stmt == nullptr && "
                   "expr == nullptr"
                << std::endl;
      res = false;
    } else if (stmt != nullptr) {
      if (!seen_sg_stmts.insert(stmt).second) {
        std::cerr << "VisitCompoundStmt: duplicate sg stmt "
                  << stmt->class_name() << "@" << stmt << std::endl;
      }
      block->append_statement(stmt);
    } else if (expr != nullptr) {
      SgExprStatement *expr_stmt = SageBuilder::buildExprStatement(expr);
      applySourceRangeWithTrailingSemicolon(expr_stmt, child_stmt);
      if (!seen_sg_stmts.insert(expr_stmt).second) {
        std::cerr << "VisitCompoundStmt: duplicate sg expr stmt "
                  << expr_stmt->class_name() << "@" << expr_stmt << std::endl;
      }
      block->append_statement(expr_stmt);
    }
  }

  SageBuilder::popScopeStack();

  *node = block;

  return VisitStmt(compound_stmt, node) && res;
}

bool ClangToSageTranslator::VisitContinueStmt(
    clang::ContinueStmt *continue_stmt, SgNode **node) {
#if DEBUG_VISIT_STMT
  std::cerr << "ClangToSageTranslator::VisitContinueStmt" << std::endl;
#endif

  *node = SageBuilder::buildContinueStmt();
  return VisitStmt(continue_stmt, node);
}

bool ClangToSageTranslator::VisitCoreturnStmt(
    clang::CoreturnStmt *core_turn_stmt, SgNode **node) {
#if DEBUG_VISIT_STMT
  std::cerr << "ClangToSageTranslator::VisitCoreturnStmt" << std::endl;
#endif
  bool res = true;

  SgExpression *return_expr = nullptr;
  if (clang::Expr *operand = core_turn_stmt->getOperand()) {
    SgNode *tmp_expr = Traverse(operand);
    return_expr = isSgExpression(tmp_expr);
    if (tmp_expr != nullptr && return_expr == nullptr) {
      std::cerr << "Runtime error: co_return operand did not translate into "
                   "SgExpression"
                << std::endl;
      res = false;
    }
  }

  SgReturnStmt *return_stmt = SageBuilder::buildReturnStmt(return_expr);
  applySourceRangeWithTrailingSemicolon(return_stmt, core_turn_stmt);
  setCoroutineKeywordAttribute(return_stmt, "co_return");
  *node = return_stmt;

  return VisitStmt(core_turn_stmt, node) && res;
}

bool ClangToSageTranslator::VisitCoroutineBodyStmt(
    clang::CoroutineBodyStmt *coroutine_body_stmt, SgNode **node) {
#if DEBUG_VISIT_STMT
  std::cerr << "ClangToSageTranslator::VisitCoroutineBodyStmt" << std::endl;
#endif
  bool res = true;

  SgNode *tmp_body = Traverse(coroutine_body_stmt->getBody());
  SgStatement *body = isSgStatement(tmp_body);
  if (body == nullptr && tmp_body != nullptr) {
    if (SgExpression *expr = isSgExpression(tmp_body)) {
      body = SageBuilder::buildExprStatement(expr);
      applySourceRange(body, coroutine_body_stmt->getBody()->getSourceRange());
    } else {
      std::cerr << "Runtime error: CoroutineBodyStmt body did not translate "
                   "into SgStatement or SgExpression"
                << std::endl;
      res = false;
    }
  }

  if (body == nullptr) {
    body = SageBuilder::buildNullStatement();
  }

  applySourceRange(body, coroutine_body_stmt->getSourceRange());
  *node = body;

  return VisitStmt(coroutine_body_stmt, node) && res;
}

bool ClangToSageTranslator::VisitCXXCatchStmt(
    clang::CXXCatchStmt *cxx_catch_stmt, SgNode **node) {
#if DEBUG_VISIT_STMT
  std::cerr << "ClangToSageTranslator::VisitCXXCatchStmt" << std::endl;
#endif
  bool res = true;

  SgCatchOptionStmt *catch_stmt =
      SageBuilder::buildCatchOptionStmt(nullptr, nullptr);
  // Ensure the catch statement is parented before any scope queries.
  if (catch_stmt->get_parent() == nullptr) {
    catch_stmt->set_parent(SageBuilder::topScopeStack());
  }

  SgScopeStatement *catch_scope = isSgScopeStatement(catch_stmt);
  ROSE_ASSERT(catch_scope != nullptr);
  SageBuilder::pushScopeStack(catch_scope);

  SgVariableDeclaration *condition_decl = nullptr;
  if (clang::VarDecl *exception_decl = cxx_catch_stmt->getExceptionDecl()) {
    SgNode *tmp_condition = Traverse(exception_decl);
    condition_decl = isSgVariableDeclaration(tmp_condition);
    if (tmp_condition != nullptr && condition_decl == nullptr) {
      std::cerr
          << "Runtime error: tmp_condition != nullptr && condition_decl == "
             "nullptr"
          << std::endl;
      res = false;
    }
  } else {
    SgName empty_name("");
    SgType *ellipsis_type = SgTypeEllipse::createType();
    condition_decl = SageBuilder::buildVariableDeclaration_nfi(
        empty_name, ellipsis_type, nullptr, SageBuilder::topScopeStack());
  }

  if (condition_decl != nullptr) {
    SgInitializedName *init_name =
        SageInterface::getFirstInitializedName(condition_decl);
    if (init_name != nullptr && init_name->get_scope() == nullptr) {
      init_name->set_scope(catch_scope);
    }
  }

  SgNode *tmp_body = Traverse(cxx_catch_stmt->getHandlerBlock());
  SgStatement *body_stmt = isSgStatement(tmp_body);
  if (tmp_body != nullptr && body_stmt == nullptr) {
    std::cerr << "Runtime error: tmp_body != nullptr && body_stmt == nullptr"
              << std::endl;
    res = false;
  }

  SageBuilder::popScopeStack();

  catch_stmt->set_condition(condition_decl);
  if (condition_decl != nullptr) {
    condition_decl->set_parent(catch_stmt);
  }

  ROSE_ASSERT(body_stmt != nullptr);
  catch_stmt->set_body(body_stmt);
  body_stmt->set_parent(catch_stmt);

  *node = catch_stmt;
  return VisitStmt(cxx_catch_stmt, node) && res;
}

bool ClangToSageTranslator::VisitCXXForRangeStmt(
    clang::CXXForRangeStmt *cxx_for_range_stmt, SgNode **node) {
#if DEBUG_VISIT_STMT
  std::cerr << "ClangToSageTranslator::VisitCXXForRangeStmt" << std::endl;
#endif
  bool res = true;

  auto translate_for_init_child =
      [&](clang::Stmt *clang_stmt) -> SgStatement * {
    if (clang_stmt == nullptr) {
      return nullptr;
    }

    bool prev_in_for_init = p_in_for_init_translation;
    p_in_for_init_translation = true;
    SgNode *tmp = Traverse(clang_stmt);
    p_in_for_init_translation = prev_in_for_init;

    SgStatement *stmt = isSgStatement(tmp);
    if (stmt == nullptr) {
      if (SgExpression *expr = isSgExpression(tmp)) {
        stmt = SageBuilder::buildExprStatement(expr);
        applySourceRange(stmt, clang_stmt->getSourceRange());
      } else if (tmp != nullptr) {
        std::cerr << "Runtime error: for-range child did not translate to a "
                     "statement ("
                  << tmp->class_name() << ")" << std::endl;
        res = false;
      }
    }

    return stmt;
  };

  SgBasicBlock *wrapper_block = nullptr;
  SgStatement *init_stmt = nullptr;
  if (clang::Stmt *clang_init = cxx_for_range_stmt->getInit()) {
    wrapper_block = SageBuilder::buildBasicBlock_nfi();
    if (wrapper_block != nullptr && wrapper_block->get_parent() == nullptr) {
      wrapper_block->set_parent(SageBuilder::topScopeStack());
    }

    SageBuilder::pushScopeStack(wrapper_block);
    init_stmt = translate_for_init_child(clang_init);
    SageBuilder::popScopeStack();
  }

  SgRangeBasedForStatement *sg_for_stmt =
      SageBuilder::buildRangeBasedForStatement_nfi(
          (SgVariableDeclaration *)nullptr, (SgVariableDeclaration *)nullptr,
          (SgVariableDeclaration *)nullptr, (SgVariableDeclaration *)nullptr,
          (SgExpression *)nullptr, (SgExpression *)nullptr,
          (SgStatement *)nullptr);

  sg_for_stmt->set_parent(
      wrapper_block != nullptr
          ? static_cast<SgNode *>(wrapper_block)
          : static_cast<SgNode *>(SageBuilder::topScopeStack()));
  ROSE_ASSERT(sg_for_stmt->get_parent() != nullptr);
  SageBuilder::pushScopeStack(sg_for_stmt);

  auto translate_var_decl_child =
      [&](clang::Stmt *clang_stmt,
          const char *child_label) -> SgVariableDeclaration * {
    SgStatement *stmt = translate_for_init_child(clang_stmt);
    if (stmt == nullptr) {
      return nullptr;
    }

    SgVariableDeclaration *var_decl = isSgVariableDeclaration(stmt);
    if (var_decl == nullptr) {
      std::cerr << "Runtime error: " << child_label
                << " translated to non-variable declaration statement: "
                << stmt->class_name() << std::endl;
      res = false;
    }

    return var_decl;
  };

  // Root-cause fix: model C++ range-for directly in the AST instead of
  // desugaring to SgForStatement. SgForStatement for-init unparsing merges
  // declarations and rewrites type deduction semantics (e.g. __range/__begin).
  SgVariableDeclaration *range_decl = translate_var_decl_child(
      cxx_for_range_stmt->getRangeStmt(), "CXXForRangeStmt::range");
  SgVariableDeclaration *begin_decl = translate_var_decl_child(
      cxx_for_range_stmt->getBeginStmt(), "CXXForRangeStmt::begin");
  SgVariableDeclaration *end_decl = translate_var_decl_child(
      cxx_for_range_stmt->getEndStmt(), "CXXForRangeStmt::end");
  SgVariableDeclaration *iterator_decl = translate_var_decl_child(
      cxx_for_range_stmt->getLoopVarStmt(), "CXXForRangeStmt::loop variable");

  SgExpression *not_equal_expr = nullptr;
  if (clang::Expr *clang_cond = cxx_for_range_stmt->getCond()) {
    SgNode *tmp_cond = Traverse(clang_cond);
    if (SgExpression *cond_expr = isSgExpression(tmp_cond)) {
      not_equal_expr = cond_expr;
    } else if (SgExprStatement *cond_stmt = isSgExprStatement(tmp_cond)) {
      not_equal_expr = cond_stmt->get_expression();
    } else if (tmp_cond != nullptr) {
      std::cerr << "Runtime error: CXXForRangeStmt cond translated to "
                   "non-expression node: "
                << tmp_cond->class_name() << std::endl;
      res = false;
    }
  }

  SgExpression *increment_expr = nullptr;
  if (clang::Expr *clang_inc = cxx_for_range_stmt->getInc()) {
    SgNode *tmp_inc = Traverse(clang_inc);
    if (SgExpression *inc_expr = isSgExpression(tmp_inc)) {
      increment_expr = inc_expr;
    } else if (SgExprStatement *inc_stmt = isSgExprStatement(tmp_inc)) {
      increment_expr = inc_stmt->get_expression();
    } else if (tmp_inc != nullptr) {
      std::cerr << "Runtime error: CXXForRangeStmt inc translated to "
                   "non-expression node: "
                << tmp_inc->class_name() << std::endl;
      res = false;
    }
  }

  SgNode *tmp_body = cxx_for_range_stmt->getBody()
                         ? Traverse(cxx_for_range_stmt->getBody())
                         : nullptr;
  SgStatement *body = isSgStatement(tmp_body);
  if (body == nullptr) {
    if (SgExpression *body_expr = isSgExpression(tmp_body)) {
      body = SageBuilder::buildExprStatement(body_expr);
      applySourceRange(body, cxx_for_range_stmt->getBody()->getSourceRange());
    } else if (tmp_body != nullptr) {
      std::cerr << "Runtime error: CXXForRangeStmt body translated to "
                   "non-statement node: "
                << tmp_body->class_name() << std::endl;
      res = false;
    }
  }
  if (body != nullptr) {
    body = wrapStatementWithOpenMPPragmas(cxx_for_range_stmt->getBody(), body);
  }

  SageBuilder::popScopeStack();

  auto finalize_decl = [&](SgVariableDeclaration *decl) {
    if (decl == nullptr) {
      return;
    }

    if (decl->get_scope() == nullptr) {
      decl->set_scope(sg_for_stmt);
    }

    for (SgInitializedName *init_name : decl->get_variables()) {
      if (init_name != nullptr && init_name->get_scope() == nullptr) {
        init_name->set_scope(sg_for_stmt);
      }
    }
  };

  finalize_decl(iterator_decl);
  finalize_decl(range_decl);
  finalize_decl(begin_decl);
  finalize_decl(end_decl);

  if (wrapper_block != nullptr && init_stmt != nullptr) {
    if (SgVariableDeclaration *init_decl = isSgVariableDeclaration(init_stmt)) {
      if (init_decl->get_scope() == nullptr) {
        init_decl->set_scope(wrapper_block);
      }

      for (SgInitializedName *init_name : init_decl->get_variables()) {
        if (init_name != nullptr && init_name->get_scope() == nullptr) {
          init_name->set_scope(wrapper_block);
        }
      }
    }

    init_stmt->set_parent(wrapper_block);
  }

  if (iterator_decl != nullptr) {
    iterator_decl->set_parent(sg_for_stmt);
    sg_for_stmt->set_iterator_declaration(iterator_decl);
  }
  if (range_decl != nullptr) {
    range_decl->set_parent(sg_for_stmt);
    sg_for_stmt->set_range_declaration(range_decl);
  }
  if (begin_decl != nullptr) {
    begin_decl->set_parent(sg_for_stmt);
    sg_for_stmt->set_begin_declaration(begin_decl);
  }
  if (end_decl != nullptr) {
    end_decl->set_parent(sg_for_stmt);
    sg_for_stmt->set_end_declaration(end_decl);
  }
  if (not_equal_expr != nullptr) {
    not_equal_expr->set_parent(sg_for_stmt);
    sg_for_stmt->set_not_equal_expression(not_equal_expr);
  }
  if (increment_expr != nullptr) {
    increment_expr->set_parent(sg_for_stmt);
    sg_for_stmt->set_increment_expression(increment_expr);
  }
  if (body != nullptr) {
    body->set_parent(sg_for_stmt);
    sg_for_stmt->set_loop_body(body);
  }

  applySourceRange(sg_for_stmt, cxx_for_range_stmt->getSourceRange());

  // Enforce structural invariants expected by AST tests and CFG support.
  ROSE_ASSERT(sg_for_stmt->get_iterator_declaration() != nullptr);
  ROSE_ASSERT(sg_for_stmt->get_range_declaration() != nullptr);
  ROSE_ASSERT(sg_for_stmt->get_loop_body() != nullptr);

  if (wrapper_block != nullptr) {
    SgStatementPtrList &stmts = wrapper_block->get_statements();
    if (init_stmt != nullptr &&
        std::find(stmts.begin(), stmts.end(), init_stmt) == stmts.end()) {
      stmts.push_back(init_stmt);
    }
    if (std::find(stmts.begin(), stmts.end(), sg_for_stmt) == stmts.end()) {
      stmts.push_back(sg_for_stmt);
    }
    sg_for_stmt->set_parent(wrapper_block);
    applySourceRange(wrapper_block, cxx_for_range_stmt->getSourceRange());
    *node = wrapper_block;
  } else {
    *node = sg_for_stmt;
  }

  return VisitStmt(cxx_for_range_stmt, node) && res;
}

bool ClangToSageTranslator::VisitCXXTryStmt(clang::CXXTryStmt *cxx_try_stmt,
                                            SgNode **node) {
#if DEBUG_VISIT_STMT
  std::cerr << "ClangToSageTranslator::VisitCXXTryStmt" << std::endl;
#endif
  bool res = true;

  SgNode *tmp_body = Traverse(cxx_try_stmt->getTryBlock());
  SgStatement *try_body = isSgStatement(tmp_body);
  if (try_body == nullptr) {
    if (tmp_body != nullptr) {
      std::cerr << "Runtime error: try body translated to non-statement node: "
                << tmp_body->class_name() << std::endl;
    }
    const bool clang_errors =
        p_compiler_instance != nullptr &&
        p_compiler_instance->getDiagnostics().hasErrorOccurred();
    if (clang_errors) {
      std::cerr << "Note: Clang reported errors; inserting empty try block to "
                   "avoid abort."
                << std::endl;
      try_body = SageBuilder::buildBasicBlock_nfi();
      setCompilerGeneratedFileInfo(try_body);
      res = false;
    } else {
      ROSE_ASSERT(try_body != nullptr);
    }
  }

  SgTryStmt *try_stmt = SageBuilder::buildTryStmt(try_body);

  for (unsigned i = 0; i < cxx_try_stmt->getNumHandlers(); ++i) {
    SgNode *tmp_handler = Traverse(cxx_try_stmt->getHandler(i));
    SgCatchOptionStmt *handler_stmt = isSgCatchOptionStmt(tmp_handler);
    if (tmp_handler != nullptr && handler_stmt == nullptr) {
      std::cerr
          << "Runtime error: tmp_handler != nullptr && handler_stmt == nullptr"
          << std::endl;
      res = false;
      continue;
    }
    if (handler_stmt != nullptr) {
      try_stmt->append_catch_statement(handler_stmt);
      handler_stmt->set_trystmt(try_stmt);
    }
  }

  *node = try_stmt;
  return VisitStmt(cxx_try_stmt, node) && res;
}

bool ClangToSageTranslator::VisitDeclStmt(clang::DeclStmt *decl_stmt,
                                          SgNode **node) {
#if DEBUG_VISIT_STMT
  std::cerr << "ClangToSageTranslator::VisitDeclStmt" << std::endl;
#endif

  bool res = true;

  std::vector<clang::Decl *> visible_decls;
  for (clang::Decl *decl : decl_stmt->decls()) {
    if (decl == nullptr) {
      continue;
    }
    if (llvm::isa<clang::BindingDecl>(decl)) {
      continue;
    }
    visible_decls.push_back(decl);
  }

  if (visible_decls.empty()) {
    *node = nullptr;
  } else if (visible_decls.size() == 1) {
    *node = Traverse(visible_decls.front());
#if DEBUG_VISIT_STMT
    printf("In VisitDeclStmt(): *node = %p = %s \n", *node,
           (*node)->class_name().c_str());
#endif
  } else {
    std::vector<SgNode *> tmp_decls;
    // SgDeclarationStatement * decl;
    clang::DeclStmt::decl_iterator it;

    SgScopeStatement *scope = SageBuilder::topScopeStack();

    for (auto visible_it = visible_decls.begin();
         visible_it != visible_decls.end() - 1; ++visible_it) {
      clang::Decl *decl = *visible_it;
      if (decl == nullptr)
        continue;
      SgNode *child = Traverse(decl);

      SgDeclarationStatement *sub_decl_stmt = isSgDeclarationStatement(child);
      if (sub_decl_stmt == nullptr && child != nullptr) {
        std::cerr << "Runtime error: the node produce for a clang::Decl is not "
                     "a SgDeclarationStatement !"
                  << std::endl;
        std::cerr << "    class = " << child->class_name() << std::endl;
        res = false;
        continue;
      }
      if (sub_decl_stmt == nullptr) {
        continue;
      } else if (child != nullptr) {
        if (clang::TagDecl *tagDecl = llvm::dyn_cast<clang::TagDecl>(decl)) {
          if (tagDecl->isEmbeddedInDeclarator() ||
              tagDecl->getTypedefNameForAnonDecl() != nullptr) {
            continue;
          }
        }
      }
      if (scope != nullptr && !p_in_for_init_translation) {
        scope->append_statement(sub_decl_stmt);
        sub_decl_stmt->set_parent(scope);
      }
    }
    // last declaration in scope
    SgNode *lastDecl = Traverse(visible_decls.back());
    SgDeclarationStatement *last_decl_Stmt = isSgDeclarationStatement(lastDecl);
    if (lastDecl != nullptr && last_decl_Stmt == nullptr) {
      std::cerr
          << "Runtime error: lastDecl != nullptr && last_decl_Stmt == nullptr"
          << std::endl;
      res = false;
    }
    *node = last_decl_Stmt;
  }

#if DEBUG_VISIT_STMT
  printf("In VisitDeclStmt(): identify where the parent is not set: *node = %p "
         "= %s \n",
         *node, (*node)->class_name().c_str());
  printf(" --- *node parent = %p \n", (*node)->get_parent());
#endif

  return res;
}

bool ClangToSageTranslator::VisitDoStmt(clang::DoStmt *do_stmt, SgNode **node) {
#if DEBUG_VISIT_STMT
  std::cerr << "ClangToSageTranslator::VisitDoStmt" << std::endl;
#endif

  SgNode *tmp_cond = Traverse(do_stmt->getCond());
  SgExpression *cond = isSgExpression(tmp_cond);
  ROSE_ASSERT(cond != nullptr);

  SgStatement *expr_stmt = SageBuilder::buildExprStatement(cond);
  if (p_compiler_instance != nullptr && do_stmt->getCond() != nullptr) {
    applySourceRange(expr_stmt, do_stmt->getCond()->getSourceRange());
  }

  ROSE_ASSERT(expr_stmt != nullptr);

  SgDoWhileStmt *sg_do_stmt =
      SageBuilder::buildDoWhileStmt_nfi(expr_stmt, nullptr);

  sg_do_stmt->set_parent(SageBuilder::topScopeStack());

  sg_do_stmt->set_condition(expr_stmt);

  cond->set_parent(expr_stmt);
  expr_stmt->set_parent(sg_do_stmt);

  SageBuilder::pushScopeStack(sg_do_stmt);

  SgNode *tmp_body = Traverse(do_stmt->getBody());
  SgStatement *body = isSgStatement(tmp_body);
  SgExpression *expr = isSgExpression(tmp_body);
  if (expr != nullptr) {
    body = SageBuilder::buildExprStatement(expr);
    applySourceRangeWithTrailingSemicolon(body, do_stmt->getBody());
  }
  ROSE_ASSERT(body != nullptr);
  body = wrapStatementWithOpenMPPragmas(do_stmt->getBody(), body);

  body->set_parent(sg_do_stmt);

  SageBuilder::popScopeStack();

  sg_do_stmt->set_body(body);

  *node = sg_do_stmt;

  return VisitStmt(do_stmt, node);
}

bool ClangToSageTranslator::VisitForStmt(clang::ForStmt *for_stmt,
                                         SgNode **node) {
#if DEBUG_VISIT_STMT
  std::cerr << "ClangToSageTranslator::VisitForStmt" << std::endl;
#endif

  bool res = true;

  // DQ (11/28/2020): We have to build the scope first, and then build the rest
  // bottom up.
  SgForStatement *sg_for_stmt = new SgForStatement(
      (SgStatement *)nullptr, (SgExpression *)nullptr, (SgStatement *)nullptr);

#if DEBUG_VISIT_STMT
  printf("In VisitForStmt(): Setting the parent of the sg_for_stmt \n");
#endif

  // DQ (11/28/2020): this is required for test2012_127.c.
  sg_for_stmt->set_parent(SageBuilder::topScopeStack());

  // DQ (11/28/2020): Adding asertion.
  ROSE_ASSERT(sg_for_stmt->get_parent() != nullptr);

  SageBuilder::pushScopeStack(sg_for_stmt);

  // Initialization

  SgForInitStatement *for_init_stmt = nullptr;

  {
    SgStatementPtrList for_init_stmt_list;
    clang::Stmt *clang_init = for_stmt->getInit();
    bool prev_in_for_init = p_in_for_init_translation;
    p_in_for_init_translation = true;

    if (clang::DeclStmt *decl_stmt =
            llvm::dyn_cast_or_null<clang::DeclStmt>(clang_init)) {
      std::set<const clang::TagDecl *> inline_tag_decls;
      auto get_tag_decl_from_type =
          [&](clang::QualType qual_type) -> clang::TagDecl * {
        const clang::Type *type = qual_type.getTypePtr();
        while (type != nullptr) {
          if (qualifiedTypeHasQualifier(type)) {
            qual_type = qual_type.getCanonicalType();
          } else if (auto *ptr = llvm::dyn_cast<clang::PointerType>(type)) {
            qual_type = ptr->getPointeeType();
          } else if (auto *array = llvm::dyn_cast<clang::ArrayType>(type)) {
            qual_type = array->getElementType();
          } else {
            break;
          }
          type = qual_type.getTypePtr();
        }
        if (auto *record_type =
                llvm::dyn_cast_or_null<clang::RecordType>(type)) {
          return record_type->getDecl();
        }
        if (auto *enum_type = llvm::dyn_cast_or_null<clang::EnumType>(type)) {
          return enum_type->getDecl();
        }
        return nullptr;
      };
      std::set<const clang::TagDecl *> tag_decls_in_stmt;
      bool has_declarator_decl = false;
      for (auto it = decl_stmt->decl_begin(); it != decl_stmt->decl_end();
           ++it) {
        if (auto *tag_decl = llvm::dyn_cast<clang::TagDecl>(*it)) {
          tag_decls_in_stmt.insert(tag_decl->getCanonicalDecl());
        }
        if (llvm::isa<clang::DeclaratorDecl>(*it)) {
          has_declarator_decl = true;
        }
      }
      if (!tag_decls_in_stmt.empty()) {
        for (auto it = decl_stmt->decl_begin(); it != decl_stmt->decl_end();
             ++it) {
          if (auto *decl = llvm::dyn_cast<clang::DeclaratorDecl>(*it)) {
            if (clang::TagDecl *tag = get_tag_decl_from_type(decl->getType())) {
              if (tag_decls_in_stmt.find(tag->getCanonicalDecl()) !=
                  tag_decls_in_stmt.end()) {
                inline_tag_decls.insert(tag->getCanonicalDecl());
              }
            }
          }
        }
      }
      if (inline_tag_decls.empty() && has_declarator_decl &&
          !tag_decls_in_stmt.empty()) {
        inline_tag_decls = tag_decls_in_stmt;
      }

      for (auto it = decl_stmt->decl_begin(); it != decl_stmt->decl_end();
           ++it) {
        clang::Decl *decl = *it;
        if (decl == nullptr) {
          continue;
        }
        if (clang::TagDecl *tag_decl = llvm::dyn_cast<clang::TagDecl>(decl)) {
          bool treat_as_embedded =
              tag_decl->isEmbeddedInDeclarator() ||
              tag_decl->getTypedefNameForAnonDecl() != nullptr ||
              inline_tag_decls.find(tag_decl->getCanonicalDecl()) !=
                  inline_tag_decls.end();
          if (treat_as_embedded) {
            p_inline_tag_decls.insert(tag_decl->getCanonicalDecl());
            SgNode *tag_node = Traverse(tag_decl);
            if (SgDeclarationStatement *tag_stmt =
                    isSgDeclarationStatement(tag_node)) {
              ensureDeclInScopeChildListPreserveScope(
                  tag_stmt, sg_for_stmt, "VisitForStmt:inline-tag");
            }
            continue;
          }
        }
        SgNode *child = Traverse(decl);
        SgStatement *stmt = isSgStatement(child);
        if (child != nullptr && stmt == nullptr) {
          std::cerr << "Runtime error: decl in for-init did not translate to "
                       "SgStatement ("
                    << child->class_name() << ")" << std::endl;
          res = false;
          continue;
        }
        if (stmt != nullptr) {
          for_init_stmt_list.push_back(stmt);
        }
      }
    } else if (clang_init != nullptr) {
      SgNode *tmp_init = Traverse(clang_init);
      SgStatement *init_stmt = isSgStatement(tmp_init);
      SgExpression *init_expr = isSgExpression(tmp_init);
      if (tmp_init != nullptr && init_stmt == nullptr && init_expr == nullptr) {
        std::cerr
            << "Runtime error: tmp_init != nullptr && init_stmt == nullptr && "
               "init_expr == nullptr ("
            << tmp_init->class_name() << ")" << std::endl;
        res = false;
      } else if (init_expr != nullptr) {
        init_stmt = SageBuilder::buildExprStatement(init_expr);
        applySourceRange(init_stmt, clang_init->getSourceRange());
      }
      if (init_stmt != nullptr) {
        for_init_stmt_list.push_back(init_stmt);
      }
    }

    p_in_for_init_translation = prev_in_for_init;

    if (for_init_stmt_list.size() > 1) {
      auto canonical_class_decl =
          [](SgClassDeclaration *decl) -> SgClassDeclaration * {
        if (decl == nullptr) {
          return nullptr;
        }
        if (SgClassDeclaration *first =
                isSgClassDeclaration(decl->get_firstNondefiningDeclaration())) {
          return first;
        }
        return decl;
      };
      auto canonical_enum_decl =
          [](SgEnumDeclaration *decl) -> SgEnumDeclaration * {
        if (decl == nullptr) {
          return nullptr;
        }
        if (SgEnumDeclaration *first =
                isSgEnumDeclaration(decl->get_firstNondefiningDeclaration())) {
          return first;
        }
        return decl;
      };

      auto embed_class_decl = [&](SgClassDeclaration *class_decl,
                                  SgVariableDeclaration *var_decl) -> bool {
        if (class_decl == nullptr || var_decl == nullptr) {
          return false;
        }
        if (var_decl->get_variables().empty()) {
          return false;
        }
        SgInitializedName *init_name = var_decl->get_variables().front();
        if (init_name == nullptr || init_name->get_type() == nullptr) {
          return false;
        }
        SgType *base_type = init_name->get_type()->findBaseType();
        SgClassType *class_type = isSgClassType(base_type);
        if (class_type == nullptr) {
          return false;
        }
        SgClassDeclaration *type_decl =
            isSgClassDeclaration(class_type->get_declaration());
        bool match =
            canonical_class_decl(type_decl) == canonical_class_decl(class_decl);
        if (!match && type_decl != nullptr && class_decl != nullptr) {
          if (type_decl->get_name() == class_decl->get_name()) {
            SgScopeStatement *type_scope = type_decl->get_scope();
            SgScopeStatement *class_scope = class_decl->get_scope();
            if (type_scope != nullptr && type_scope == class_scope) {
              match = true;
            }
          }
        }
        if (!match && type_decl != nullptr) {
          if (SgClassDeclaration *type_def =
                  isSgClassDeclaration(type_decl->get_definingDeclaration())) {
            if (canonical_class_decl(type_def) ==
                canonical_class_decl(class_decl)) {
              match = true;
            } else if (class_decl != nullptr &&
                       type_def->get_name() == class_decl->get_name()) {
              SgScopeStatement *type_scope = type_def->get_scope();
              SgScopeStatement *class_scope = class_decl->get_scope();
              if (type_scope != nullptr && type_scope == class_scope) {
                match = true;
              }
            }
          }
        }
        if (!match) {
          return false;
        }

        SgClassDeclaration *def_decl =
            isSgClassDeclaration(class_decl->get_definingDeclaration());
        if (def_decl == nullptr) {
          def_decl = class_decl;
        }
        def_decl->set_parent(var_decl);
        def_decl->set_isAutonomousDeclaration(false);
        suppress_unparse_output(def_decl);
        if (class_decl != def_decl) {
          class_decl->set_isAutonomousDeclaration(false);
          suppress_unparse_output(class_decl);
        }
        var_decl->set_baseTypeDefiningDeclaration(def_decl);
        var_decl->set_variableDeclarationContainsBaseTypeDefiningDeclaration(
            true);
        return true;
      };

      auto embed_enum_decl = [&](SgEnumDeclaration *enum_decl,
                                 SgVariableDeclaration *var_decl) -> bool {
        if (enum_decl == nullptr || var_decl == nullptr) {
          return false;
        }
        if (var_decl->get_variables().empty()) {
          return false;
        }
        SgInitializedName *init_name = var_decl->get_variables().front();
        if (init_name == nullptr || init_name->get_type() == nullptr) {
          return false;
        }
        SgType *base_type = init_name->get_type()->findBaseType();
        SgEnumType *enum_type = isSgEnumType(base_type);
        if (enum_type == nullptr) {
          return false;
        }
        SgEnumDeclaration *type_decl =
            isSgEnumDeclaration(enum_type->get_declaration());
        bool match =
            canonical_enum_decl(type_decl) == canonical_enum_decl(enum_decl);
        if (!match && type_decl != nullptr && enum_decl != nullptr) {
          if (type_decl->get_name() == enum_decl->get_name()) {
            SgScopeStatement *type_scope = type_decl->get_scope();
            SgScopeStatement *enum_scope = enum_decl->get_scope();
            if (type_scope != nullptr && type_scope == enum_scope) {
              match = true;
            }
          }
        }
        if (!match && type_decl != nullptr) {
          if (SgEnumDeclaration *type_def =
                  isSgEnumDeclaration(type_decl->get_definingDeclaration())) {
            if (canonical_enum_decl(type_def) ==
                canonical_enum_decl(enum_decl)) {
              match = true;
            } else if (enum_decl != nullptr &&
                       type_def->get_name() == enum_decl->get_name()) {
              SgScopeStatement *type_scope = type_def->get_scope();
              SgScopeStatement *enum_scope = enum_decl->get_scope();
              if (type_scope != nullptr && type_scope == enum_scope) {
                match = true;
              }
            }
          }
        }
        if (!match) {
          return false;
        }
        SgEnumDeclaration *def_decl =
            isSgEnumDeclaration(enum_decl->get_definingDeclaration());
        if (def_decl == nullptr) {
          def_decl = enum_decl;
        }
        def_decl->set_parent(var_decl);
        def_decl->set_isAutonomousDeclaration(false);
        suppress_unparse_output(def_decl);
        if (enum_decl != def_decl) {
          enum_decl->set_isAutonomousDeclaration(false);
          suppress_unparse_output(enum_decl);
        }
        var_decl->set_baseTypeDefiningDeclaration(def_decl);
        var_decl->set_variableDeclarationContainsBaseTypeDefiningDeclaration(
            true);
        return true;
      };

      SgStatementPtrList filtered;
      filtered.reserve(for_init_stmt_list.size());
      for (size_t i = 0; i < for_init_stmt_list.size(); ++i) {
        SgStatement *stmt = for_init_stmt_list[i];
        if (SgClassDeclaration *class_decl = isSgClassDeclaration(stmt)) {
          if (i + 1 < for_init_stmt_list.size()) {
            if (SgVariableDeclaration *var_decl =
                    isSgVariableDeclaration(for_init_stmt_list[i + 1])) {
              if (embed_class_decl(class_decl, var_decl)) {
                ensureDeclInScopeChildListPreserveScope(
                    class_decl, sg_for_stmt, "VisitForStmt:embedded-class");
                continue;
              }
            }
          }
        } else if (SgEnumDeclaration *enum_decl = isSgEnumDeclaration(stmt)) {
          if (i + 1 < for_init_stmt_list.size()) {
            if (SgVariableDeclaration *var_decl =
                    isSgVariableDeclaration(for_init_stmt_list[i + 1])) {
              if (embed_enum_decl(enum_decl, var_decl)) {
                ensureDeclInScopeChildListPreserveScope(
                    enum_decl, sg_for_stmt, "VisitForStmt:embedded-enum");
                continue;
              }
            }
          }
        }
        filtered.push_back(stmt);
      }
      for_init_stmt_list.swap(filtered);
    }

    if (for_init_stmt_list.size() == 0) {
      SgNullStatement *nullStmt = SageBuilder::buildNullStatement_nfi();
      setCompilerGeneratedFileInfo(nullStmt, false);
      for_init_stmt_list.push_back(nullStmt);
    }

    for_init_stmt = sg_for_stmt->get_for_init_stmt();
    ROSE_ASSERT(for_init_stmt != nullptr);
    for_init_stmt->get_init_stmt().clear();
    for (SgStatement *init_stmt : for_init_stmt_list) {
      if (init_stmt == nullptr) {
        continue;
      }
      for_init_stmt->append_init_stmt(init_stmt);
    }

#if DEBUG_VISIT_STMT
    printf("In VisitForStmt(): for_init_stmt = %p  \n");
#endif

    if (for_stmt->getInit() != nullptr)
      applySourceRange(for_init_stmt, for_stmt->getInit()->getSourceRange());
    else
      setCompilerGeneratedFileInfo(for_init_stmt, false);
    Sg_File_Info *for_init_fi = for_init_stmt->get_file_info();
    if (for_init_stmt->get_startOfConstruct() == nullptr ||
        for_init_fi == nullptr || for_init_fi->isCompilerGenerated()) {
      setCompilerGeneratedFileInfo(for_init_stmt, false);
    }

    // Ensure for-init statements are parented by the SgForInitStatement while
    // keeping their scope on the enclosing for-statement.
    for (SgStatement *init_stmt : for_init_stmt_list) {
      if (init_stmt == nullptr) {
        continue;
      }
      if (SgDeclarationStatement *decl = isSgDeclarationStatement(init_stmt)) {
        if (decl->get_scope() == nullptr) {
          decl->set_scope(sg_for_stmt);
        }
        if (SgVariableDeclaration *var_decl = isSgVariableDeclaration(decl)) {
          for (SgInitializedName *init_name : var_decl->get_variables()) {
            if (init_name != nullptr && init_name->get_scope() == nullptr) {
              init_name->set_scope(sg_for_stmt);
            }
          }
        }
      }
    }
  }

  // Condition

  SgStatement *cond_stmt = nullptr;

  {
    if (clang::DeclStmt *cond_decl = for_stmt->getConditionVariableDeclStmt()) {
      SgNode *tmp_cond = Traverse(cond_decl);
      cond_stmt = isSgStatement(tmp_cond);
      if (tmp_cond != nullptr && cond_stmt == nullptr) {
        std::cerr << "Runtime error: condition decl did not translate to "
                     "SgStatement ("
                  << tmp_cond->class_name() << ")" << std::endl;
        res = false;
      }
      if (cond_stmt != nullptr) {
        applySourceRange(cond_stmt, cond_decl->getSourceRange());
      }
    } else {
      SgNode *tmp_cond = Traverse(for_stmt->getCond());
      SgExpression *cond = isSgExpression(tmp_cond);
      if (tmp_cond != nullptr && cond == nullptr) {
        std::cerr << "Runtime error: tmp_cond != nullptr && cond == nullptr"
                  << std::endl;
        res = false;
      }
      if (cond != nullptr) {
        cond_stmt = SageBuilder::buildExprStatement(cond);
        applySourceRange(cond_stmt, for_stmt->getCond()->getSourceRange());
      } else {
        cond_stmt = SageBuilder::buildNullStatement_nfi();
        setCompilerGeneratedFileInfo(cond_stmt, true);
      }

      if (cond_stmt != nullptr) {
        auto *expr_stmt = isSgExprStatement(cond_stmt);
        if (expr_stmt != nullptr) {
          auto simplifyOperand = [](SgExpression *operand) -> SgExpression * {
            SgExpression *current = operand;
            while (auto cast = isSgCastExp(current)) {
              current = cast->get_operand_i();
            }
            if (isSgVarRefExp(current) != nullptr ||
                isSgIntVal(current) != nullptr ||
                isSgUnsignedIntVal(current) != nullptr ||
                isSgLongLongIntVal(current) != nullptr ||
                isSgUnsignedLongLongIntVal(current) != nullptr) {
              return SageInterface::copyExpression(current);
            }
            return nullptr;
          };

          if (auto *less_than = isSgLessThanOp(expr_stmt->get_expression())) {
            SgExpression *lhs_simplified =
                simplifyOperand(less_than->get_lhs_operand());
            SgExpression *rhs_simplified =
                simplifyOperand(less_than->get_rhs_operand());
            if (lhs_simplified != nullptr && rhs_simplified != nullptr) {
              SgExpression *new_cond =
                  SageBuilder::buildLessThanOp(lhs_simplified, rhs_simplified);
              applySourceRange(new_cond, for_stmt->getCond()->getSourceRange());
              expr_stmt->set_expression(new_cond);
              new_cond->set_parent(expr_stmt);
            }
          }
        }
      }
    }

    if (SgDeclarationStatement *decl = isSgDeclarationStatement(cond_stmt)) {
      SgScopeStatement *cond_scope = isSgScopeStatement(sg_for_stmt);
      if (cond_scope == nullptr) {
        cond_scope = sg_for_stmt->get_scope();
      }
      if (cond_scope == nullptr) {
        cond_scope = SageBuilder::topScopeStack();
      }
      if (cond_scope != nullptr) {
        decl->set_scope(cond_scope);
        if (SgVariableDeclaration *var_decl = isSgVariableDeclaration(decl)) {
          for (SgInitializedName *init_name : var_decl->get_variables()) {
            if (init_name != nullptr) {
              init_name->set_scope(cond_scope);
            }
          }
        }
      }
    }
  }

  // Increment

  SgExpression *inc = nullptr;

  {
    SgNode *tmp_inc = Traverse(for_stmt->getInc());
    inc = isSgExpression(tmp_inc);
    if (tmp_inc != nullptr && inc == nullptr) {
      std::cerr << "Runtime error: tmp_inc != nullptr && inc == nullptr"
                << std::endl;
      res = false;
    }
    if (inc == nullptr) {
      inc = SageBuilder::buildNullExpression_nfi();
      setCompilerGeneratedFileInfo(inc, true);
    }
  }

  // Body

  SgStatement *body = nullptr;

  {
    SgNode *tmp_body = Traverse(for_stmt->getBody());
    body = isSgStatement(tmp_body);
    if (body == nullptr) {
      SgExpression *body_expr = isSgExpression(tmp_body);
      if (body_expr != nullptr) {
        body = SageBuilder::buildExprStatement(body_expr);
        applySourceRange(body, for_stmt->getBody()->getSourceRange());
      }
    }
    if (tmp_body != nullptr && body == nullptr) {
      std::cerr << "Runtime error: tmp_body != nullptr && body == nullptr"
                << std::endl;
      res = false;
    }
    if (body == nullptr) {
      body = SageBuilder::buildNullStatement_nfi();
      setCompilerGeneratedFileInfo(body);
    }
  }
  body = wrapStatementWithOpenMPPragmas(for_stmt->getBody(), body);

  SageBuilder::popScopeStack();

  // Attach sub trees to the for statement

  if (for_init_stmt->get_parent() == nullptr) {
    for_init_stmt->set_parent(sg_for_stmt);
  }

  if (cond_stmt != nullptr) {
    cond_stmt->set_parent(sg_for_stmt);
    sg_for_stmt->set_test(cond_stmt);
  }

  if (inc != nullptr) {
    inc->set_parent(sg_for_stmt);
    sg_for_stmt->set_increment(inc);
  }

  if (body != nullptr) {
    body->set_parent(sg_for_stmt);
    sg_for_stmt->set_loop_body(body);
  }

  // DQ (11/28/2020): Now we want to use the scope that is already on the stack
  // (instead of adding a new one).
  SageBuilder::buildForStatement_nfi(sg_for_stmt, for_init_stmt, cond_stmt, inc,
                                     body);

  // DQ (11/28/2020): Adding asertion.
  ROSE_ASSERT(sg_for_stmt->get_parent() != nullptr);

  *node = sg_for_stmt;

  return VisitStmt(for_stmt, node) && res;
}

bool ClangToSageTranslator::VisitGotoStmt(clang::GotoStmt *goto_stmt,
                                          SgNode **node) {
#if DEBUG_VISIT_STMT
  std::cerr << "ClangToSageTranslator::VisitGotoStmt" << std::endl;
#endif

  bool res = true;

  SgSymbol *tmp_sym = GetSymbolFromSymbolTable(goto_stmt->getLabel());
  SgLabelSymbol *sym = isSgLabelSymbol(tmp_sym);
  if (sym == nullptr) {
    SgNode *tmp_label = Traverse(goto_stmt->getLabel()->getStmt());
    SgLabelStatement *label_stmt = isSgLabelStatement(tmp_label);
    if (label_stmt == nullptr) {
      std::cerr << "Runtime error: Cannot find the symbol for the label: \""
                << goto_stmt->getLabel()->getStmt()->getName() << "\"."
                << std::endl;
      std::cerr << "Runtime Error: Cannot find the label: \""
                << goto_stmt->getLabel()->getStmt()->getName() << "\"."
                << std::endl;
      res = false;
    } else {
      *node = SageBuilder::buildGotoStatement(label_stmt);
    }
  } else {
    *node = SageBuilder::buildGotoStatement(sym->get_declaration());
  }

  /*
      SgNode * tmp_label = Traverse(goto_stmt->getLabel()->getStmt());
      SgLabelStatement * label_stmt = isSgLabelStatement(tmp_label);
      if (label_stmt == nullptr) {
          std::cerr << "Runtime Error: Cannot find the label: \"" <<
     goto_stmt->getLabel()->getStmt()->getName() << "\"." << std::endl; res =
     false;
      }
      else {
          *node = SageBuilder::buildGotoStatement(label_stmt);
      }
  */
  return VisitStmt(goto_stmt, node) && res;
}

bool ClangToSageTranslator::VisitIfStmt(clang::IfStmt *if_stmt, SgNode **node) {
#if DEBUG_VISIT_STMT
  std::cerr << "ClangToSageTranslator::VisitIfStmt" << std::endl;
#endif

  bool res = true;

  // TODO if_stmt->getConditionVariable() appears when a variable is declared in
  // the condition...
  //
  // C++17 if-init statements (if (init; cond)) are represented by getInit().
  // Since SgIfStmt does not directly model init-statements, lower this shape to
  // an equivalent block:
  //   { init; if (cond) ... }
  clang::Stmt *clang_if_init_stmt = if_stmt->getInit();
  SgBasicBlock *if_init_wrapper = nullptr;
  if (clang_if_init_stmt != nullptr) {
    if_init_wrapper = SageBuilder::buildBasicBlock_nfi();
    if_init_wrapper->set_parent(SageBuilder::topScopeStack());
    SageBuilder::pushScopeStack(if_init_wrapper);

    SgNode *tmp_init = Traverse(clang_if_init_stmt);
    SgStatement *sg_init_stmt = isSgStatement(tmp_init);
    SgExpression *sg_init_expr = isSgExpression(tmp_init);
    if (tmp_init == nullptr) {
      MLOG_ERROR_CXX(MLOG_FRONTEND)
          << "Runtime error: if-init translation returned nullptr."
          << std::endl;
      res = false;
    } else if (sg_init_stmt == nullptr && sg_init_expr == nullptr) {
      MLOG_ERROR_CXX(MLOG_FRONTEND)
          << "Runtime error: if-init did not translate to SgStatement or "
             "SgExpression ("
          << tmp_init->class_name() << ")." << std::endl;
      res = false;
    } else if (sg_init_expr != nullptr) {
      sg_init_stmt = SageBuilder::buildExprStatement(sg_init_expr);
      if (sg_init_stmt == nullptr) {
        MLOG_ERROR_CXX(MLOG_FRONTEND)
            << "Runtime error: if-init buildExprStatement failed." << std::endl;
        res = false;
      }
    }

    if (sg_init_stmt == nullptr) {
      sg_init_stmt = SageBuilder::buildNullStatement_nfi();
      setCompilerGeneratedFileInfo(sg_init_stmt, true);
    } else {
      applySourceRange(sg_init_stmt, clang_if_init_stmt->getSourceRange());
    }

    sg_init_stmt->set_parent(if_init_wrapper);
    if_init_wrapper->append_statement(sg_init_stmt);
  }

  *node = SageBuilder::buildIfStmt_nfi(nullptr, nullptr, nullptr);

  // Pei-Hung (04/22/22) Needs to setup parent node before processing the
  // operands. Needed for test2013_55.c and other similar tests
  (*node)->set_parent(SageBuilder::topScopeStack());
  SageBuilder::pushScopeStack(isSgScopeStatement(*node));

  SgStatement *cond_stmt = nullptr;
  if (clang::DeclStmt *cond_decl = if_stmt->getConditionVariableDeclStmt()) {
    SgNode *tmp_cond = Traverse(cond_decl);
    cond_stmt = isSgStatement(tmp_cond);
    if (tmp_cond != nullptr && cond_stmt == nullptr) {
      std::cerr << "Runtime error: condition decl did not translate to "
                   "SgStatement ("
                << tmp_cond->class_name() << ")" << std::endl;
      res = false;
    }
    if (cond_stmt != nullptr) {
      applySourceRange(cond_stmt, cond_decl->getSourceRange());
    }
  } else {
    SgNode *tmp_cond = Traverse(if_stmt->getCond());
    SgExpression *cond_expr = isSgExpression(tmp_cond);
    cond_stmt = SageBuilder::buildExprStatement(cond_expr);
    applySourceRange(cond_stmt, if_stmt->getCond()->getSourceRange());
    if (if_stmt->getRParenLoc().isValid()) {
      clang::SourceRange cond_range = if_stmt->getCond()->getSourceRange();
      if (cond_range.getBegin().isValid()) {
        clang::SourceRange expanded(cond_range.getBegin(),
                                    if_stmt->getRParenLoc());
        applySourceRange(cond_stmt, expanded);
      }
    }
  }

  if (SgDeclarationStatement *decl = isSgDeclarationStatement(cond_stmt)) {
    SgScopeStatement *cond_scope = isSgScopeStatement(*node);
    if (cond_scope == nullptr) {
      cond_scope = SageBuilder::topScopeStack();
    }
    if (cond_scope != nullptr) {
      decl->set_scope(cond_scope);
      if (SgVariableDeclaration *var_decl = isSgVariableDeclaration(decl)) {
        for (SgInitializedName *init_name : var_decl->get_variables()) {
          if (init_name != nullptr) {
            init_name->set_scope(cond_scope);
          }
        }
      }
    }
  }

  if (cond_stmt == nullptr) {
    cond_stmt = SageBuilder::buildNullStatement_nfi();
    setCompilerGeneratedFileInfo(cond_stmt, true);
  }

  SgNode *tmp_then = Traverse(if_stmt->getThen());
  SgStatement *then_stmt = isSgStatement(tmp_then);
  if (then_stmt == nullptr) {
    SgExpression *then_expr = isSgExpression(tmp_then);
    ROSE_ASSERT(then_expr != nullptr);
    then_stmt = SageBuilder::buildExprStatement(then_expr);
  }
  applySourceRange(then_stmt, if_stmt->getThen()->getSourceRange());
  then_stmt = wrapStatementWithOpenMPPragmas(if_stmt->getThen(), then_stmt);

  SgNode *tmp_else = Traverse(if_stmt->getElse());
  SgStatement *else_stmt = isSgStatement(tmp_else);
  if (else_stmt == nullptr) {
    SgExpression *else_expr = isSgExpression(tmp_else);
    if (else_expr != nullptr)
      else_stmt = SageBuilder::buildExprStatement(else_expr);
  }
  if (else_stmt != nullptr) {
    applySourceRange(else_stmt, if_stmt->getElse()->getSourceRange());
    else_stmt = wrapStatementWithOpenMPPragmas(if_stmt->getElse(), else_stmt);
  }

  SageBuilder::popScopeStack();

  cond_stmt->set_parent(*node);
  isSgIfStmt(*node)->set_conditional(cond_stmt);

  then_stmt->set_parent(*node);
  isSgIfStmt(*node)->set_true_body(then_stmt);
  if (else_stmt != nullptr) {
    else_stmt->set_parent(*node);
    isSgIfStmt(*node)->set_false_body(else_stmt);
  }

  if (if_init_wrapper != nullptr) {
    SgStatement *if_statement = isSgStatement(*node);
    ROSE_ASSERT(if_statement != nullptr);
    if_statement->set_parent(if_init_wrapper);
    if_init_wrapper->append_statement(if_statement);
    if (clang_if_init_stmt->getBeginLoc().isValid() &&
        if_stmt->getEndLoc().isValid()) {
      applySourceRange(if_init_wrapper,
                       clang::SourceRange(clang_if_init_stmt->getBeginLoc(),
                                          if_stmt->getEndLoc()));
    }

    SageBuilder::popScopeStack();
    *node = if_init_wrapper;
    // Avoid VisitStmt(if_stmt, node) here: it would overwrite the wrapper's
    // explicit init-to-end source range with if_stmt->getSourceRange().
    return res;
  }

  return VisitStmt(if_stmt, node) && res;
}

bool ClangToSageTranslator::VisitIndirectGotoStmt(
    clang::IndirectGotoStmt *indirect_goto_stmt, SgNode **node) {
#if DEBUG_VISIT_STMT
  std::cerr << "ClangToSageTranslator::VisitIndirectGotoStmt" << std::endl;
#endif
  bool res = true;

  SgNode *tmp_target = Traverse(indirect_goto_stmt->getTarget());
  SgExpression *target_expr = isSgExpression(tmp_target);

  if (target_expr == nullptr) {
    if (tmp_target == nullptr) {
      MLOG_ERROR_CXX(MLOG_FRONTEND)
          << "Runtime error: IndirectGotoStmt target did not translate to a "
             "Sage node."
          << std::endl;
    } else {
      MLOG_ERROR_CXX(MLOG_FRONTEND)
          << "Runtime error: IndirectGotoStmt target did not translate to "
             "SgExpression ("
          << tmp_target->class_name() << ")." << std::endl;
    }
    *node = SageBuilder::buildNullStatement();
    setCompilerGeneratedFileInfo(*node);
    res = false;
  } else {
    SgGotoStatement *sg_goto_stmt =
        SageBuilder::buildGotoStatement_nfi(target_expr);
    ROSE_ASSERT(sg_goto_stmt != nullptr);
    if (target_expr->get_parent() == nullptr) {
      target_expr->set_parent(sg_goto_stmt);
    }

    *node = sg_goto_stmt;
  }

  return VisitStmt(indirect_goto_stmt, node) && res;
}

bool ClangToSageTranslator::VisitMSDependentExistsStmt(
    clang::MSDependentExistsStmt *ms_dependent_exists_stmt, SgNode **node) {
#if DEBUG_VISIT_STMT
  std::cerr << "ClangToSageTranslator::VisitMSDependentExistsStmt" << std::endl;
#endif
  bool res = true;

  ROSE_ASSERT(FAIL_TODO == 0); // TODO

  return VisitStmt(ms_dependent_exists_stmt, node) && res;
}

bool ClangToSageTranslator::VisitNullStmt(clang::NullStmt *null_stmt,
                                          SgNode **node) {
#if DEBUG_VISIT_STMT
  std::cerr << "ClangToSageTranslator::VisitNullStmt" << std::endl;
#endif
  bool res = true;

  *node = SageBuilder::buildNullStatement();

  return VisitStmt(null_stmt, node) && res;
}

bool ClangToSageTranslator::VisitOMPExecutableDirective(
    clang::OMPExecutableDirective *omp_executable_directive, SgNode **node) {
#if DEBUG_VISIT_STMT
  std::cerr << "ClangToSageTranslator::VisitOMPExecutableDirective"
            << std::endl;
#endif
  bool res = true;
  SgStatement *associated_stmt = nullptr;

  // Traverse the body statement
  if (clang::Stmt *clang_associated_stmt =
          omp_executable_directive->getAssociatedStmt()) {
    SgNode *tmp_stmt = Traverse(clang_associated_stmt);
    associated_stmt = isSgStatement(tmp_stmt);
    if (tmp_stmt != nullptr && associated_stmt == nullptr) {
      std::cerr << "Runtime error: associated OpenMP statement did not "
                   "translate into an SgStatement."
                << std::endl;
      res = false;
    }
  }

  SgStatement *target_stmt = associated_stmt;
  if (target_stmt == nullptr) {
    target_stmt = SageBuilder::buildNullStatement();
    target_stmt->set_parent(SageBuilder::topScopeStack());
  }

  // Just return the body statement
  // The parent (VisitCompoundStmt) will handle pragma insertion via
  // appendOpenMPPragmasBefore DO NOT call VisitStmt here - it would apply the
  // directive's source range to the body statement, overwriting the body's
  // correct source info
  *node = target_stmt;
  return res;
}

bool ClangToSageTranslator::VisitOMPAtomicDirective(
    clang::OMPAtomicDirective *omp_atomic_directive, SgNode **node) {
#if DEBUG_VISIT_STMT
  std::cerr << "ClangToSageTranslator::VisitOMPAtomicDirective" << std::endl;
#endif
  bool res = true;

  return VisitOMPExecutableDirective(omp_atomic_directive, node) && res;
}

bool ClangToSageTranslator::VisitOMPBarrierDirective(
    clang::OMPBarrierDirective *omp_barrier_directive, SgNode **node) {
#if DEBUG_VISIT_STMT
  std::cerr << "ClangToSageTranslator::VisitOMPBarrierDirective" << std::endl;
#endif
  bool res = true;

  return VisitOMPExecutableDirective(omp_barrier_directive, node) && res;
}

bool ClangToSageTranslator::VisitOMPCancelDirective(
    clang::OMPCancelDirective *omp_cancel_directive, SgNode **node) {
#if DEBUG_VISIT_STMT
  std::cerr << "ClangToSageTranslator::VisitOMPCancelDirective" << std::endl;
#endif
  bool res = true;

  ROSE_ASSERT(FAIL_TODO == 0); // TODO

  return VisitOMPExecutableDirective(omp_cancel_directive, node) && res;
}

bool ClangToSageTranslator::VisitOMPCancellationPointDirective(
    clang::OMPCancellationPointDirective *omp_cancellation_point_directive,
    SgNode **node) {
#if DEBUG_VISIT_STMT
  std::cerr << "ClangToSageTranslator::VisitOMPCancellationPointDirective"
            << std::endl;
#endif
  bool res = true;

  ROSE_ASSERT(FAIL_TODO == 0); // TODO

  return VisitOMPExecutableDirective(omp_cancellation_point_directive, node) &&
         res;
}

bool ClangToSageTranslator::VisitOMPCriticalDirective(
    clang::OMPCriticalDirective *omp_critical_directive, SgNode **node) {
#if DEBUG_VISIT_STMT
  std::cerr << "ClangToSageTranslator::VisitOMPCriticalDirective" << std::endl;
#endif
  bool res = true;

  ROSE_ASSERT(FAIL_TODO == 0); // TODO

  return VisitOMPExecutableDirective(omp_critical_directive, node) && res;
}

bool ClangToSageTranslator::VisitOMPFlushDirective(
    clang::OMPFlushDirective *omp_flush_directive, SgNode **node) {
#if DEBUG_VISIT_STMT
  std::cerr << "ClangToSageTranslator::VisitOMPFlushDirective" << std::endl;
#endif
  bool res = true;

  ROSE_ASSERT(FAIL_TODO == 0); // TODO

  return VisitOMPExecutableDirective(omp_flush_directive, node) && res;
}

bool ClangToSageTranslator::VisitOMPLoopDirective(
    clang::OMPLoopDirective *omp_loop_directive, SgNode **node) {
#if DEBUG_VISIT_STMT
  std::cerr << "ClangToSageTranslator::VisitOMPLoopDirective" << std::endl;
#endif
  bool res = true;

  return VisitOMPExecutableDirective(omp_loop_directive, node) && res;
}

bool ClangToSageTranslator::VisitOMPDistributeDirective(
    clang::OMPDistributeDirective *omp_distribute_directive, SgNode **node) {
#if DEBUG_VISIT_STMT
  std::cerr << "ClangToSageTranslator::VisitOMPDistributeDirective"
            << std::endl;
#endif
  bool res = true;

  ROSE_ASSERT(FAIL_TODO == 0); // TODO

  return VisitOMPLoopDirective(omp_distribute_directive, node) && res;
}

bool ClangToSageTranslator::VisitOMPDistributeParallelForDirective(
    clang::OMPDistributeParallelForDirective
        *omp_distribute_parallel_for_directive,
    SgNode **node) {
#if DEBUG_VISIT_STMT
  std::cerr << "ClangToSageTranslator::VisitOMPDistributeParallelForDirective"
            << std::endl;
#endif
  bool res = true;

  ROSE_ASSERT(FAIL_TODO == 0); // TODO

  return VisitOMPExecutableDirective(omp_distribute_parallel_for_directive,
                                     node) &&
         res;
}

bool ClangToSageTranslator::VisitOMPDistributeParallelForSimdDirective(
    clang::OMPDistributeParallelForSimdDirective
        *omp_distribute_parallel_for_simd_directive,
    SgNode **node) {
#if DEBUG_VISIT_STMT
  std::cerr
      << "ClangToSageTranslator::VisitOMPDistributeParallelForSimdDirective"
      << std::endl;
#endif
  bool res = true;

  ROSE_ASSERT(FAIL_TODO == 0); // TODO

  return VisitOMPExecutableDirective(omp_distribute_parallel_for_simd_directive,
                                     node) &&
         res;
}

bool ClangToSageTranslator::VisitOMPDistributeSimdDirective(
    clang::OMPDistributeSimdDirective *omp_distribute__simd_directive,
    SgNode **node) {
#if DEBUG_VISIT_STMT
  std::cerr << "ClangToSageTranslator::VisitOMPDistributeSimdDirective"
            << std::endl;
#endif
  bool res = true;

  ROSE_ASSERT(FAIL_TODO == 0); // TODO

  return VisitOMPExecutableDirective(omp_distribute__simd_directive, node) &&
         res;
}

bool ClangToSageTranslator::VisitOMPForDirective(
    clang::OMPForDirective *omp_for_directive, SgNode **node) {
#if DEBUG_VISIT_STMT
  std::cerr << "ClangToSageTranslator::VisitOMPForDirective" << std::endl;
#endif
  bool res = true;

  return VisitOMPLoopDirective(omp_for_directive, node) && res;
}

bool ClangToSageTranslator::VisitOMPForSimdDirective(
    clang::OMPForSimdDirective *omp_for_simd_directive, SgNode **node) {
#if DEBUG_VISIT_STMT
  std::cerr << "ClangToSageTranslator::VisitOMPForSimdDirective" << std::endl;
#endif
  bool res = true;

  ROSE_ASSERT(FAIL_TODO == 0); // TODO

  return VisitOMPLoopDirective(omp_for_simd_directive, node) && res;
}

bool ClangToSageTranslator::VisitOMPParallelForDirective(
    clang::OMPParallelForDirective *omp_parallel_for_directive, SgNode **node) {
#if DEBUG_VISIT_STMT
  std::cerr << "ClangToSageTranslator::VisitOMPParallelForDirective"
            << std::endl;
#endif
  bool res = true;

  ROSE_ASSERT(FAIL_TODO == 0); // TODO

  return VisitOMPLoopDirective(omp_parallel_for_directive, node) && res;
}

bool ClangToSageTranslator::VisitOMPParallelForSimdDirective(
    clang::OMPParallelForSimdDirective *omp_parallel_for_simd_directive,
    SgNode **node) {
#if DEBUG_VISIT_STMT
  std::cerr << "ClangToSageTranslator::VisitOMPParallelForSimdDirective"
            << std::endl;
#endif
  bool res = true;

  ROSE_ASSERT(FAIL_TODO == 0); // TODO

  return VisitOMPLoopDirective(omp_parallel_for_simd_directive, node) && res;
}

bool ClangToSageTranslator::VisitOMPSimdDirective(
    clang::OMPSimdDirective *omp_simd_directive, SgNode **node) {
#if DEBUG_VISIT_STMT
  std::cerr << "ClangToSageTranslator::VisitOMPSimdDirective" << std::endl;
#endif
  bool res = true;

  ROSE_ASSERT(FAIL_TODO == 0); // TODO

  return VisitOMPLoopDirective(omp_simd_directive, node) && res;
}

bool ClangToSageTranslator::VisitOMPTargetParallelForDirective(
    clang::OMPTargetParallelForDirective *omp_target_parallel_for_directive,
    SgNode **node) {
#if DEBUG_VISIT_STMT
  std::cerr << "ClangToSageTranslator::VisitOMPTargetParallelForDirective"
            << std::endl;
#endif
  bool res = true;

  return VisitOMPLoopDirective(omp_target_parallel_for_directive, node) && res;
}

bool ClangToSageTranslator::VisitOMPTargetParallelForSimdDirective(
    clang::OMPTargetParallelForSimdDirective
        *omp_target_parallel_for_simd_directive,
    SgNode **node) {
#if DEBUG_VISIT_STMT
  std::cerr << "ClangToSageTranslator::VisitOMPTargetParallelForSimdDirective"
            << std::endl;
#endif
  bool res = true;

  return VisitOMPLoopDirective(omp_target_parallel_for_simd_directive, node) &&
         res;
}

bool ClangToSageTranslator::VisitOMPTargetSimdDirective(
    clang::OMPTargetSimdDirective *omp_target_simd_directive, SgNode **node) {
#if DEBUG_VISIT_STMT
  std::cerr << "ClangToSageTranslator::VisitOMPTargetSimdDirective"
            << std::endl;
#endif
  bool res = true;

  return VisitOMPLoopDirective(omp_target_simd_directive, node) && res;
}

bool ClangToSageTranslator::VisitOMPTargetTeamsDistributeDirective(
    clang::OMPTargetTeamsDistributeDirective
        *omp_target_teams_distribute_directive,
    SgNode **node) {
#if DEBUG_VISIT_STMT
  std::cerr << "ClangToSageTranslator::VisitOMPTargetTeamsDistributeDirective"
            << std::endl;
#endif
  bool res = true;

  return VisitOMPLoopDirective(omp_target_teams_distribute_directive, node) &&
         res;
}

bool ClangToSageTranslator::VisitOMPTargetTeamsDistributeSimdDirective(
    clang::OMPTargetTeamsDistributeSimdDirective
        *omp_target_teams_distribute_simd_directive,
    SgNode **node) {
#if DEBUG_VISIT_STMT
  std::cerr
      << "ClangToSageTranslator::VisitOMPTargetTeamsDistributeSimdDirective"
      << std::endl;
#endif
  bool res = true;

  return VisitOMPLoopDirective(omp_target_teams_distribute_simd_directive,
                               node) &&
         res;
}

bool ClangToSageTranslator::VisitOMPTaskLoopDirective(
    clang::OMPTaskLoopDirective *omp_task_loop_directive, SgNode **node) {
#if DEBUG_VISIT_STMT
  std::cerr << "ClangToSageTranslator::VisitOMPTaskLoopDirective" << std::endl;
#endif
  bool res = true;

  ROSE_ASSERT(FAIL_TODO == 0); // TODO

  return VisitOMPLoopDirective(omp_task_loop_directive, node) && res;
}

bool ClangToSageTranslator::VisitOMPTaskLoopSimdDirective(
    clang::OMPTaskLoopSimdDirective *omp_task_loop_simd_directive,
    SgNode **node) {
#if DEBUG_VISIT_STMT
  std::cerr << "ClangToSageTranslator::VisitOMPTaskLoopSimdDirective"
            << std::endl;
#endif
  bool res = true;

  ROSE_ASSERT(FAIL_TODO == 0); // TODO

  return VisitOMPLoopDirective(omp_task_loop_simd_directive, node) && res;
}

bool ClangToSageTranslator::VisitOMPMasterDirective(
    clang::OMPMasterDirective *omp_master_directive, SgNode **node) {
#if DEBUG_VISIT_STMT
  std::cerr << "ClangToSageTranslator::VisitOMPMasterDirective" << std::endl;
#endif
  bool res = true;

  ROSE_ASSERT(FAIL_TODO == 0); // TODO

  return VisitOMPExecutableDirective(omp_master_directive, node) && res;
}

bool ClangToSageTranslator::VisitOMPOrderedDirective(
    clang::OMPOrderedDirective *omp_ordered_directive, SgNode **node) {
#if DEBUG_VISIT_STMT
  std::cerr << "ClangToSageTranslator::VisitOMPOrderedDirective" << std::endl;
#endif
  bool res = true;

  ROSE_ASSERT(FAIL_TODO == 0); // TODO

  return VisitOMPExecutableDirective(omp_ordered_directive, node) && res;
}

bool ClangToSageTranslator::VisitOMPParallelDirective(
    clang::OMPParallelDirective *omp_parallel_directive, SgNode **node) {
#if DEBUG_VISIT_STMT
  std::cerr << "ClangToSageTranslator::VisitOMPParallelDirective" << std::endl;
#endif
  bool res = true;

  return VisitOMPExecutableDirective(omp_parallel_directive, node) && res;
}

bool ClangToSageTranslator::VisitOMPParallelSectionsDirective(
    clang::OMPParallelSectionsDirective *omp_parallel_sections_directive,
    SgNode **node) {
#if DEBUG_VISIT_STMT
  std::cerr << "ClangToSageTranslator::VisitOMPParallelSectionsDirective"
            << std::endl;
#endif
  bool res = true;

  ROSE_ASSERT(FAIL_TODO == 0); // TODO

  return VisitOMPExecutableDirective(omp_parallel_sections_directive, node) &&
         res;
}

bool ClangToSageTranslator::VisitReturnStmt(clang::ReturnStmt *return_stmt,
                                            SgNode **node) {
#if DEBUG_VISIT_STMT
  std::cerr << "ClangToSageTranslator::VisitReturnStmt" << std::endl;
#endif

  bool res = true;

  SgNode *tmp_expr = Traverse(return_stmt->getRetValue());
  SgExpression *expr = isSgExpression(tmp_expr);
  if (tmp_expr != nullptr && expr == nullptr) {
    std::cerr << "Runtime error: tmp_expr != nullptr && expr == nullptr"
              << std::endl;
    res = false;
  }
  if (SgConstructorInitializer *ctor_init = isSgConstructorInitializer(expr)) {
    // Return-by-value construction requires an explicit type name in ROSE's
    // constructor-initializer representation.
    ctor_init->set_need_name(true);
    ctor_init->set_need_parenthesis_after_name(true);
    ctor_init->set_is_explicit_cast(true);
  }
  *node = SageBuilder::buildReturnStmt(expr);

  return VisitStmt(return_stmt, node) && res;
}

bool ClangToSageTranslator::VisitSEHExceptStmt(
    clang::SEHExceptStmt *seh_except_stmt, SgNode **node) {
#if DEBUG_VISIT_STMT
  std::cerr << "ClangToSageTranslator::VisitSEHExceptStmt" << std::endl;
#endif
  bool res = true;

  ROSE_ASSERT(FAIL_TODO == 0); // TODO

  return VisitStmt(seh_except_stmt, node) && res;
}

bool ClangToSageTranslator::VisitSEHFinallyStmt(
    clang::SEHFinallyStmt *seh_finally_stmt, SgNode **node) {
#if DEBUG_VISIT_STMT
  std::cerr << "ClangToSageTranslator::VisitSEHFinallyStmt" << std::endl;
#endif
  bool res = true;

  ROSE_ASSERT(FAIL_TODO == 0); // TODO

  return VisitStmt(seh_finally_stmt, node) && res;
}

bool ClangToSageTranslator::VisitSEHLeaveStmt(
    clang::SEHLeaveStmt *seh_leave_stmt, SgNode **node) {
#if DEBUG_VISIT_STMT
  std::cerr << "ClangToSageTranslator::VisitSEHLeaveStmt" << std::endl;
#endif
  bool res = true;

  ROSE_ASSERT(FAIL_TODO == 0); // TODO

  return VisitStmt(seh_leave_stmt, node) && res;
}

bool ClangToSageTranslator::VisitSEHTryStmt(clang::SEHTryStmt *seh_try_stmt,
                                            SgNode **node) {
#if DEBUG_VISIT_STMT
  std::cerr << "ClangToSageTranslator::VisitSEHTryStmt" << std::endl;
#endif
  bool res = true;

  ROSE_ASSERT(FAIL_TODO == 0); // TODO

  return VisitStmt(seh_try_stmt, node) && res;
}

bool ClangToSageTranslator::VisitSwitchCase(clang::SwitchCase *switch_case,
                                            SgNode **node) {
#if DEBUG_VISIT_STMT
  std::cerr << "ClangToSageTranslator::VisitSwitchCase" << std::endl;
#endif
  bool res = true;

  // TODO

  return VisitStmt(switch_case, node) && res;
}

bool ClangToSageTranslator::VisitCaseStmt(clang::CaseStmt *case_stmt,
                                          SgNode **node) {
#if DEBUG_VISIT_STMT
  std::cerr << "ClangToSageTranslator::VisitCaseStmt" << std::endl;
#endif

  SgNode *tmp_stmt = Traverse(case_stmt->getSubStmt());
  SgStatement *stmt = isSgStatement(tmp_stmt);
  SgExpression *expr = isSgExpression(tmp_stmt);
  if (expr != nullptr) {
    stmt = SageBuilder::buildExprStatement(expr);
    applySourceRangeWithTrailingSemicolon(stmt, case_stmt->getSubStmt());
  }
  ROSE_ASSERT(stmt != nullptr);
  stmt = wrapStatementWithOpenMPPragmas(case_stmt->getSubStmt(), stmt);

  SgNode *tmp_lhs = Traverse(case_stmt->getLHS());
  SgExpression *lhs = isSgExpression(tmp_lhs);
  ROSE_ASSERT(lhs != nullptr);

  SgExpression *rhs = nullptr;
  if (case_stmt->getRHS() != nullptr) {
    SgNode *tmp_rhs = Traverse(case_stmt->getRHS());
    rhs = isSgExpression(tmp_rhs);
    ROSE_ASSERT(rhs != nullptr);
  }

  SgCaseOptionStmt *caseOptionStmt =
      SageBuilder::buildCaseOptionStmt_nfi(lhs, stmt);

  if (rhs != nullptr) {
    caseOptionStmt->set_key_range_end(rhs);
  }

  *node = caseOptionStmt;

  return VisitSwitchCase(case_stmt, node);
}

bool ClangToSageTranslator::VisitDefaultStmt(clang::DefaultStmt *default_stmt,
                                             SgNode **node) {
#if DEBUG_VISIT_STMT
  std::cerr << "ClangToSageTranslator::VisitDefaultStmt" << std::endl;
#endif

  SgNode *tmp_stmt = Traverse(default_stmt->getSubStmt());
  SgStatement *stmt = isSgStatement(tmp_stmt);
  SgExpression *expr = isSgExpression(tmp_stmt);
  if (expr != nullptr) {
    stmt = SageBuilder::buildExprStatement(expr);
    applySourceRangeWithTrailingSemicolon(stmt, default_stmt->getSubStmt());
  }
  ROSE_ASSERT(stmt != nullptr);
  stmt = wrapStatementWithOpenMPPragmas(default_stmt->getSubStmt(), stmt);

  *node = SageBuilder::buildDefaultOptionStmt_nfi(stmt);

  return VisitSwitchCase(default_stmt, node);
}

bool ClangToSageTranslator::VisitSwitchStmt(clang::SwitchStmt *switch_stmt,
                                            SgNode **node) {
#if DEBUG_VISIT_STMT
  std::cerr << "ClangToSageTranslator::VisitSwitchStmt" << std::endl;
#endif

  bool res = true;

  clang::Stmt *clang_switch_init_stmt = switch_stmt->getInit();
  // SgSwitchStatement has no dedicated child for a C++17 init-statement.
  // Lower it into an enclosing block so the translated AST remains
  // structurally valid while preserving the initializer's scope for the switch
  // body.
  SgBasicBlock *switch_init_wrapper = nullptr;
  if (clang_switch_init_stmt != nullptr) {
    switch_init_wrapper = SageBuilder::buildBasicBlock_nfi();
    switch_init_wrapper->set_parent(SageBuilder::topScopeStack());
    SageBuilder::pushScopeStack(switch_init_wrapper);
  }

  auto ensure_decl_scope = [](SgStatement *stmt, SgScopeStatement *scope) {
    if (stmt == nullptr || scope == nullptr) {
      return;
    }

    if (SgDeclarationStatement *decl = isSgDeclarationStatement(stmt)) {
      decl->set_scope(scope);
      if (SgVariableDeclaration *var_decl = isSgVariableDeclaration(decl)) {
        for (SgInitializedName *init_name : var_decl->get_variables()) {
          if (init_name != nullptr) {
            init_name->set_scope(scope);
          }
        }
      }
    }
  };

  auto translate_wrapper_stmt = [&](clang::Stmt *clang_stmt) -> SgStatement * {
    if (clang_stmt == nullptr) {
      return nullptr;
    }

    SgNode *tmp_stmt = Traverse(clang_stmt);
    SgStatement *sg_stmt = isSgStatement(tmp_stmt);
    SgExpression *sg_expr = isSgExpression(tmp_stmt);
    if (tmp_stmt == nullptr) {
      return nullptr;
    }
    if (sg_stmt == nullptr && sg_expr == nullptr) {
      MLOG_ERROR_CXX(MLOG_FRONTEND)
          << "Runtime error: switch init did not translate to SgStatement or "
             "SgExpression ("
          << tmp_stmt->class_name() << ")." << std::endl;
      res = false;
      return nullptr;
    }
    if (sg_expr != nullptr) {
      sg_stmt = SageBuilder::buildExprStatement(sg_expr);
    }
    if (sg_stmt == nullptr) {
      return nullptr;
    }

    applySourceRange(sg_stmt, clang_stmt->getSourceRange());
    sg_stmt->set_parent(switch_init_wrapper);
    ensure_decl_scope(sg_stmt, switch_init_wrapper);
    switch_init_wrapper->append_statement(sg_stmt);
    return sg_stmt;
  };

  if (switch_init_wrapper != nullptr && clang_switch_init_stmt != nullptr) {
    SgStatement *sg_init_stmt = translate_wrapper_stmt(clang_switch_init_stmt);
    if (sg_init_stmt == nullptr) {
      sg_init_stmt = SageBuilder::buildNullStatement_nfi();
      setCompilerGeneratedFileInfo(sg_init_stmt, true);
      sg_init_stmt->set_parent(switch_init_wrapper);
      switch_init_wrapper->append_statement(sg_init_stmt);
    }
  }

  SgSwitchStatement *sg_switch_stmt =
      SageBuilder::buildSwitchStatement_nfi(nullptr, nullptr);
  sg_switch_stmt->set_parent(SageBuilder::topScopeStack());

  SageBuilder::pushScopeStack(sg_switch_stmt);

  SgStatement *cond_stmt = nullptr;
  if (clang::DeclStmt *cond_decl =
          switch_stmt->getConditionVariableDeclStmt()) {
    SgNode *tmp_cond = Traverse(cond_decl);
    cond_stmt = isSgStatement(tmp_cond);
    if (tmp_cond != nullptr && cond_stmt == nullptr) {
      std::cerr << "Runtime error: condition decl did not translate to "
                   "SgStatement ("
                << tmp_cond->class_name() << ")" << std::endl;
      res = false;
    }
    if (cond_stmt != nullptr) {
      applySourceRange(cond_stmt, cond_decl->getSourceRange());
    }
  } else {
    SgNode *tmp_cond = Traverse(switch_stmt->getCond());
    SgExpression *cond = isSgExpression(tmp_cond);
    ROSE_ASSERT(cond != nullptr);
    cond_stmt = SageBuilder::buildExprStatement(cond);
    applySourceRange(cond_stmt, switch_stmt->getCond()->getSourceRange());
  }

  if (cond_stmt == nullptr) {
    cond_stmt = SageBuilder::buildNullStatement_nfi();
    setCompilerGeneratedFileInfo(cond_stmt, true);
  }

  if (SgDeclarationStatement *decl = isSgDeclarationStatement(cond_stmt)) {
    SgScopeStatement *cond_scope = isSgScopeStatement(sg_switch_stmt);
    if (cond_scope == nullptr) {
      cond_scope = sg_switch_stmt->get_scope();
    }
    if (cond_scope == nullptr) {
      cond_scope = SageBuilder::topScopeStack();
    }
    if (cond_scope != nullptr) {
      decl->set_scope(cond_scope);
      if (SgVariableDeclaration *var_decl = isSgVariableDeclaration(decl)) {
        for (SgInitializedName *init_name : var_decl->get_variables()) {
          if (init_name != nullptr) {
            init_name->set_scope(cond_scope);
          }
        }
      }
    }
  }

  cond_stmt->set_parent(sg_switch_stmt);
  sg_switch_stmt->set_item_selector(cond_stmt);

  SgNode *tmp_body = Traverse(switch_stmt->getBody());
  SgStatement *body = isSgStatement(tmp_body);
  SgExpression *expr = isSgExpression(tmp_body);
  if (expr != nullptr) {
    body = SageBuilder::buildExprStatement(expr);
    applySourceRangeWithTrailingSemicolon(body, switch_stmt->getBody());
  }
  if (body == nullptr) {
    body = SageBuilder::buildNullStatement_nfi();
    setCompilerGeneratedFileInfo(body, true);
  }

  SageBuilder::popScopeStack();

  sg_switch_stmt->set_body(body);
  // Pei-Hung (07/29/2024) In the case of test2001_14.C, body can be the
  // SgDefaultStmt and the parent needs to be set properly.
  body->set_parent(sg_switch_stmt);

  if (switch_init_wrapper != nullptr) {
    applySourceRange(sg_switch_stmt, switch_stmt->getSourceRange());
    sg_switch_stmt->set_parent(switch_init_wrapper);
    switch_init_wrapper->append_statement(sg_switch_stmt);

    if (clang_switch_init_stmt->getBeginLoc().isValid() &&
        switch_stmt->getEndLoc().isValid()) {
      applySourceRange(switch_init_wrapper,
                       clang::SourceRange(clang_switch_init_stmt->getBeginLoc(),
                                          switch_stmt->getEndLoc()));
    }

    SageBuilder::popScopeStack();
    *node = switch_init_wrapper;
    return res;
  }

  *node = sg_switch_stmt;

  return VisitStmt(switch_stmt, node) && res;
}

bool ClangToSageTranslator::VisitValueStmt(clang::ValueStmt *value_stmt,
                                           SgNode **node) {
#if DEBUG_VISIT_STMT
  std::cerr << "ClangToSageTranslator::VisitValueStmt" << std::endl;
#endif
  bool res = true;

  // DQ (11/28/2020): In test2020_45.c: I think this is the enum field.
  // clang::Expr* expr = value_stmt->getExprStmt();
  // ROSE_ASSERT(expr != nullptr);

  // DQ (11/28/2020): Note that value_stmt->getExprStmt() == value_stmt, but not
  // sure why.

  // DQ (11/28/2020): This was previously commented out, and I think there is
  // nothing to do here. The actual implementation was done in VisitFullExp
  // ROSE_ASSERT(FAIL_TODO == 0); // TODO

  return VisitStmt(value_stmt, node) && res;
}

bool ClangToSageTranslator::VisitAttributedStmt(
    clang::AttributedStmt *attributed_stmt, SgNode **node) {
#if DEBUG_VISIT_STMT
  std::cerr << "ClangToSageTranslator::VisitAttributedStmt" << std::endl;
#endif
  bool res = true;

  // Attributes like [[fallthrough]] wrap an inner statement. We currently drop
  // the attribute and translate the wrapped statement directly.
  clang::Stmt *sub_stmt = attributed_stmt->getSubStmt();
  SgNode *sub_node = Traverse(sub_stmt);
  if (sub_node == nullptr) {
    sub_node = SageBuilder::buildNullStatement();
  }
  *node = sub_node;

  if (SgStatement *sg_stmt = isSgStatement(sub_node)) {
    applySourceRange(sg_stmt, attributed_stmt->getSourceRange());
  }

  return VisitValueStmt(attributed_stmt, node) && res;
}

bool ClangToSageTranslator::VisitExpr(clang::Expr *expr, SgNode **node) {
#if DEBUG_VISIT_STMT
  std::cerr << "ClangToSageTranslator::VisitExpr" << std::endl;
#endif

  // TODO Is there anything to be done? (maybe in relation with typing?)

  return VisitValueStmt(expr, node);
}

bool ClangToSageTranslator::VisitAbstractConditionalOperator(
    clang::AbstractConditionalOperator *abstract_conditional_operator,
    SgNode **node) {
#if DEBUG_VISIT_STMT
  std::cerr << "ClangToSageTranslator::VisitAbstractConditionalOperator"
            << std::endl;
#endif
  bool res = true;

  // TODO

  return VisitStmt(abstract_conditional_operator, node) && res;
}

bool ClangToSageTranslator::VisitBinaryConditionalOperator(
    clang::BinaryConditionalOperator *binary_conditional_operator,
    SgNode **node) {
#if DEBUG_VISIT_STMT
  std::cerr << "ClangToSageTranslator::VisitBinaryConditionalOperator"
            << std::endl;
#endif
  bool res = true;

  SgExpression *common_expr =
      isSgExpression(Traverse(binary_conditional_operator->getCommon()));
  ROSE_ASSERT(common_expr != nullptr);

  SgExpression *false_expr =
      isSgExpression(Traverse(binary_conditional_operator->getFalseExpr()));
  ROSE_ASSERT(false_expr != nullptr);

  SgType *common_type = buildTypeFromQualifiedType(
      binary_conditional_operator->getCommon()->getType());
  ROSE_ASSERT(common_type != nullptr);

  SgType *result_type =
      buildTypeFromQualifiedType(binary_conditional_operator->getType());
  ROSE_ASSERT(result_type != nullptr);

  SgBasicBlock *block = SageBuilder::buildBasicBlock();
  ROSE_ASSERT(block != nullptr);

  const std::string temp_name =
      SageInterface::generateUniqueVariableName(block, "gnu_binary_cond");

  SgExpression *result_expr = nullptr;
  if (binary_conditional_operator->isLValue()) {
    SgType *pointer_common_type = SageBuilder::buildPointerType(common_type);
    SgType *pointer_result_type = SageBuilder::buildPointerType(result_type);
    ROSE_ASSERT(pointer_common_type != nullptr);
    ROSE_ASSERT(pointer_result_type != nullptr);

    SgExpression *common_addr = SageBuilder::buildAddressOfOp(common_expr);
    SgAssignInitializer *initializer =
        SageBuilder::buildAssignInitializer(common_addr);
    SgVariableDeclaration *temp_decl = SageBuilder::buildVariableDeclaration(
        temp_name, pointer_common_type, initializer, block);
    ROSE_ASSERT(temp_decl != nullptr);
    SageInterface::appendStatement(temp_decl, block);

    SgExpression *cond_expr = SageBuilder::buildPointerDerefExp(
        SageBuilder::buildVarRefExp(temp_decl));
    SgExpression *true_expr = SageBuilder::buildVarRefExp(temp_decl);
    SgExpression *false_ptr_expr = SageBuilder::buildAddressOfOp(false_expr);
    SgConditionalExp *pointer_conditional =
        SageBuilder::buildConditionalExp_nfi(
            cond_expr, true_expr, false_ptr_expr, pointer_result_type);
    ROSE_ASSERT(pointer_conditional != nullptr);

    SageInterface::appendStatement(
        SageBuilder::buildExprStatement(pointer_conditional), block);

    SgStatementExpression *statement_expression =
        SageBuilder::buildStatementExpression_nfi(block);
    ROSE_ASSERT(statement_expression != nullptr);
    result_expr = SageBuilder::buildPointerDerefExp(statement_expression);
  } else {
    SgAssignInitializer *initializer =
        SageBuilder::buildAssignInitializer(common_expr);
    SgVariableDeclaration *temp_decl = SageBuilder::buildVariableDeclaration(
        temp_name, common_type, initializer, block);
    ROSE_ASSERT(temp_decl != nullptr);
    SageInterface::appendStatement(temp_decl, block);

    SgExpression *cond_expr = SageBuilder::buildVarRefExp(temp_decl);
    SgExpression *true_expr = SageBuilder::buildVarRefExp(temp_decl);
    SgConditionalExp *conditional = SageBuilder::buildConditionalExp_nfi(
        cond_expr, true_expr, false_expr, result_type);
    ROSE_ASSERT(conditional != nullptr);

    SageInterface::appendStatement(SageBuilder::buildExprStatement(conditional),
                                   block);

    result_expr = SageBuilder::buildStatementExpression_nfi(block);
  }

  ROSE_ASSERT(result_expr != nullptr);
  applySourceRange(result_expr, binary_conditional_operator->getSourceRange());
  *node = result_expr;

  return VisitAbstractConditionalOperator(binary_conditional_operator, node) &&
         res;
}

bool ClangToSageTranslator::VisitConditionalOperator(
    clang::ConditionalOperator *conditional_operator, SgNode **node) {
#if DEBUG_VISIT_STMT
  std::cerr << "ClangToSageTranslator::VisitConditionalOperator" << std::endl;
#endif

  bool res = true;

  SgNode *tmp_cond = Traverse(conditional_operator->getCond());
  SgExpression *cond_expr = isSgExpression(tmp_cond);
  ROSE_ASSERT(cond_expr);
  SgNode *tmp_true = Traverse(conditional_operator->getTrueExpr());
  SgExpression *true_expr = isSgExpression(tmp_true);
  ROSE_ASSERT(true_expr);
  SgNode *tmp_false = Traverse(conditional_operator->getFalseExpr());
  SgExpression *false_expr = isSgExpression(tmp_false);
  ROSE_ASSERT(false_expr);

  SgType *cond_type =
      buildTypeFromQualifiedType(conditional_operator->getType());
  SgConditionalExp *cond_exp = SageBuilder::buildConditionalExp_nfi(
      cond_expr, true_expr, false_expr, cond_type);
  applySourceRange(cond_exp, conditional_operator->getSourceRange());
  *node = cond_exp;

  return VisitAbstractConditionalOperator(conditional_operator, node) && res;
}

bool ClangToSageTranslator::VisitAddrLabelExpr(
    clang::AddrLabelExpr *addr_label_expr, SgNode **node) {
#if DEBUG_VISIT_STMT
  std::cerr << "ClangToSageTranslator::VisitAddrLabelExpr" << std::endl;
#endif
  bool res = true;

  *node = buildFallbackExpression(addr_label_expr);
  ROSE_ASSERT(*node != nullptr);

  clang::LabelDecl *label_decl = addr_label_expr->getLabel();
  if (label_decl == nullptr) {
    MLOG_ERROR_CXX(MLOG_FRONTEND)
        << "Runtime error: AddrLabelExpr has no target label declaration."
        << std::endl;
    res = false;
  } else {
    clang::LabelDecl *lookup_decl =
        llvm::dyn_cast<clang::LabelDecl>(label_decl->getCanonicalDecl());
    if (lookup_decl == nullptr) {
      lookup_decl = label_decl;
    }

    SgLabelSymbol *label_sym =
        isSgLabelSymbol(GetSymbolFromSymbolTable(lookup_decl));
    if (label_sym == nullptr) {
      clang::LabelStmt *clang_label_stmt = lookup_decl->getStmt();
      if (clang_label_stmt == nullptr) {
        MLOG_ERROR_CXX(MLOG_FRONTEND)
            << "Runtime error: Cannot resolve target LabelStmt for "
               "AddrLabelExpr: \""
            << lookup_decl->getNameAsString() << "\"." << std::endl;
        res = false;
      } else {
        SgNode *tmp_label = Traverse(clang_label_stmt);
        SgLabelStatement *sg_label_stmt = isSgLabelStatement(tmp_label);
        if (sg_label_stmt == nullptr) {
          MLOG_ERROR_CXX(MLOG_FRONTEND)
              << "Runtime error: Cannot translate target LabelStmt for "
                 "AddrLabelExpr: \""
              << lookup_decl->getNameAsString() << "\"." << std::endl;
          res = false;
        } else {
          label_sym = isSgLabelSymbol(GetSymbolFromSymbolTable(lookup_decl));
          if (label_sym == nullptr) {
            label_sym =
                isSgLabelSymbol(sg_label_stmt->get_symbol_from_symbol_table());
          }
          if (label_sym == nullptr && sg_label_stmt->get_scope() != nullptr) {
            label_sym = sg_label_stmt->get_scope()->lookup_label_symbol(
                sg_label_stmt->get_name());
          }
        }
      }
    }

    if (label_sym == nullptr) {
      MLOG_ERROR_CXX(MLOG_FRONTEND)
          << "Runtime error: Cannot find label symbol for AddrLabelExpr: \""
          << lookup_decl->getNameAsString() << "\"." << std::endl;
      res = false;
    } else {
      SgLabelRefExp *label_ref = SageBuilder::buildLabelRefExp(label_sym);
      applySourceRange(label_ref, addr_label_expr->getSourceRange());
      *node = label_ref;
    }
  }

  return VisitExpr(addr_label_expr, node) && res;
}

bool ClangToSageTranslator::VisitArrayInitIndexExpr(
    clang::ArrayInitIndexExpr *array_init_index_expr, SgNode **node) {
#if DEBUG_VISIT_STMT
  std::cerr << "ClangToSageTranslator::VisitArrayInitIndexExpr" << std::endl;
#endif
  bool res = true;

  // TODO

  return VisitExpr(array_init_index_expr, node) && res;
}

bool ClangToSageTranslator::VisitArrayInitLoopExpr(
    clang::ArrayInitLoopExpr *array_init_loop_expr, SgNode **node) {
#if DEBUG_VISIT_STMT
  std::cerr << "ClangToSageTranslator::VisitArrayInitLoopExpr" << std::endl;
#endif
  bool res = true;

  if (clang::OpaqueValueExpr *common_expr =
          array_init_loop_expr->getCommonExpr()) {
    SgNode *tmp_common = Traverse(common_expr);
    SgExpression *common = isSgExpression(tmp_common);
    if (tmp_common != nullptr) {
      ROSE_ASSERT(common != nullptr &&
                  "Traversed common expression of ArrayInitLoopExpr must be an "
                  "SgExpression");
      applySourceRange(common, array_init_loop_expr->getSourceRange());
      *node = common;
      return res;
    }
  }

  return VisitExpr(array_init_loop_expr, node) && res;
}

bool ClangToSageTranslator::VisitArraySubscriptExpr(
    clang::ArraySubscriptExpr *array_subscript_expr, SgNode **node) {
#if DEBUG_VISIT_STMT
  std::cerr << "ClangToSageTranslator::VisitArraySubscriptExpr" << std::endl;
#endif

  bool res = true;

  SgNode *tmp_base = Traverse(array_subscript_expr->getBase());
  SgExpression *base = isSgExpression(tmp_base);
  if (tmp_base != nullptr && base == nullptr) {
    std::cerr << "Runtime error: tmp_base != nullptr && base == nullptr"
              << std::endl;
    res = false;
  }
  if (SgCastExp *cast = isSgCastExp(base)) {
    auto pointerInfo = [](SgType *type) -> std::pair<int, SgType *> {
      int depth = 0;
      SgType *current = type;
      while (current != nullptr) {
        current = current->stripTypedefsAndModifiers();
        SgPointerType *ptrType = isSgPointerType(current);
        if (ptrType == nullptr) {
          break;
        }
        ++depth;
        current = ptrType->get_base_type();
      }
      return std::make_pair(depth, current != nullptr
                                       ? current->stripTypedefsAndModifiers()
                                       : nullptr);
    };

    SgType *operandType = cast->get_operand_i()->get_type();
    if (operandType != nullptr) {
      operandType = operandType->stripTypedefsAndModifiers();
      if (SgArrayType *arrayType = isSgArrayType(operandType)) {
        SgType *elementType = arrayType->get_base_type();
        ROSE_ASSERT(elementType != nullptr);
        SgType *targetType = SgPointerType::createType(elementType);
        ROSE_ASSERT(targetType != nullptr);
        if (pointerInfo(cast->get_type()) != pointerInfo(targetType)) {
          cast->set_type(targetType);
        }
      }
    }
  }

  SgNode *tmp_idx = Traverse(array_subscript_expr->getIdx());
  SgExpression *idx = isSgExpression(tmp_idx);
  if (tmp_idx != nullptr && idx == nullptr) {
    std::cerr << "Runtime error: tmp_idx != nullptr && idx == nullptr"
              << std::endl;
    res = false;
  }

  *node = SageBuilder::buildPntrArrRefExp(base, idx);

  return VisitExpr(array_subscript_expr, node) && res;
}

bool ClangToSageTranslator::VisitArrayTypeTraitExpr(
    clang::ArrayTypeTraitExpr *array_type_trait_expr, SgNode **node) {
#if DEBUG_VISIT_STMT
  std::cerr << "ClangToSageTranslator::VisitArrayTypeTraitExpr" << std::endl;
#endif

  // Array type traits map to builtins like __array_rank and __array_extent.
  const char *trait_spelling =
      clang::getTraitSpelling(array_type_trait_expr->getTrait());
  ROSE_ASSERT(trait_spelling != nullptr);

  SgNodePtrList args;
  SgType *queried_type =
      buildTypeFromQualifiedType(array_type_trait_expr->getQueriedType());
  ROSE_ASSERT(queried_type != nullptr);
  args.push_back(queried_type);

  if (clang::Expr *dimension =
          array_type_trait_expr->getDimensionExpression()) {
    SgNode *tmp_dim = Traverse(dimension);
    SgExpression *dim_expr = isSgExpression(tmp_dim);
    ROSE_ASSERT(dim_expr != nullptr);
    args.push_back(dim_expr);
  }

  *node = SageBuilder::buildTypeTraitBuiltinOperator(trait_spelling, args);

  return VisitExpr(array_type_trait_expr, node);
}

bool ClangToSageTranslator::VisitAsTypeExpr(clang::AsTypeExpr *as_type_expr,
                                            SgNode **node) {
#if DEBUG_VISIT_STMT
  std::cerr << "ClangToSageTranslator::VisitAsTypeExpr" << std::endl;
#endif
  bool res = true;

  // TODO

  return VisitExpr(as_type_expr, node) && res;
}

bool ClangToSageTranslator::VisitAtomicExpr(clang::AtomicExpr *atomic_expr,
                                            SgNode **node) {
#if DEBUG_VISIT_STMT
  std::cerr << "ClangToSageTranslator::VisitAtomicExpr" << std::endl;
#endif
  bool res = true;

  // ROOT CAUSE FIX: AtomicExpr represents C11/C++11 atomic operations like:
  // __atomic_load, __atomic_store, __atomic_fetch_add, __atomic_exchange, etc.
  // Build a function call expression to represent the atomic builtin.

  // Traverse all sub-expressions (pointer, values, memory orders, etc.)
  SgExprListExp *args = SageBuilder::buildExprListExp();
  for (unsigned i = 0; i < atomic_expr->getNumSubExprs(); i++) {
    SgNode *tmp_arg = Traverse(atomic_expr->getSubExprs()[i]);
    SgExpression *arg = isSgExpression(tmp_arg);
    if (arg != nullptr) {
      args->append_expression(arg);
    } else if (tmp_arg != nullptr) {
      std::cerr << "Warning: AtomicExpr argument " << i
                << " is not an expression" << std::endl;
      res = false;
    }
  }

  // Build the function call expression with the actual builtin name
  // Get the real builtin name (e.g., "__atomic_load", "__atomic_fetch_add",
  // etc.) so the unparsed code will compile correctly
  std::string builtin_name = atomic_expr->getOpAsString().str();

  // Use the actual return type from Clang, not void - many atomics return
  // values Example: __atomic_fetch_add returns the old value before addition
  SgType *return_type = buildTypeFromQualifiedType(atomic_expr->getType());
  *node = SageBuilder::buildFunctionCallExp(builtin_name, return_type, args,
                                            SageBuilder::topScopeStack());

  return VisitExpr(atomic_expr, node) && res;
}

bool ClangToSageTranslator::VisitBinaryOperator(
    clang::BinaryOperator *binary_operator, SgNode **node) {
#if DEBUG_VISIT_STMT
  std::cerr << "ClangToSageTranslator::VisitBinaryOperator" << std::endl;
#endif

  bool res = true;

  SgNode *tmp_lhs = Traverse(binary_operator->getLHS());
  SgExpression *lhs = isSgExpression(tmp_lhs);
  if (tmp_lhs != nullptr && lhs == nullptr) {
    std::cerr << "Runtime error: tmp_lhs != nullptr && lhs == nullptr"
              << std::endl;
    res = false;
  }

  SgNode *tmp_rhs = Traverse(binary_operator->getRHS());
  SgExpression *rhs = isSgExpression(tmp_rhs);
  if (tmp_rhs != nullptr && rhs == nullptr) {
    std::cerr << "Runtime error: tmp_rhs != nullptr && rhs == nullptr"
              << std::endl;
    res = false;
  }

  switch (binary_operator->getOpcode()) {
  // ROOT CAUSE FIX: Pointer-to-member operators for C++ member function
  // pointers
  case clang::BO_PtrMemD:
    *node = SageBuilder::buildDotStarOp(lhs, rhs);
    break; // obj.*ptr_to_member
  case clang::BO_PtrMemI:
    *node = SageBuilder::buildArrowStarOp(lhs, rhs);
    break; // ptr->*ptr_to_member
  case clang::BO_Mul:
    *node = SageBuilder::buildMultiplyOp(lhs, rhs);
    break;
  case clang::BO_Div:
    *node = SageBuilder::buildDivideOp(lhs, rhs);
    break;
  case clang::BO_Rem:
    *node = SageBuilder::buildModOp(lhs, rhs);
    break;
  case clang::BO_Add:
    *node = SageBuilder::buildAddOp(lhs, rhs);
    break;
  case clang::BO_Sub:
    *node = SageBuilder::buildSubtractOp(lhs, rhs);
    break;
  case clang::BO_Shl:
    *node = SageBuilder::buildLshiftOp(lhs, rhs);
    break;
  case clang::BO_Shr:
    *node = SageBuilder::buildRshiftOp(lhs, rhs);
    break;
  case clang::BO_Cmp:
    *node = SageBuilder::buildSpaceshipOp(lhs, rhs);
    break;
  case clang::BO_LT:
    *node = SageBuilder::buildLessThanOp(lhs, rhs);
    break;
  case clang::BO_GT:
    *node = SageBuilder::buildGreaterThanOp(lhs, rhs);
    break;
  case clang::BO_LE:
    *node = SageBuilder::buildLessOrEqualOp(lhs, rhs);
    break;
  case clang::BO_GE:
    *node = SageBuilder::buildGreaterOrEqualOp(lhs, rhs);
    break;
  case clang::BO_EQ:
    *node = SageBuilder::buildEqualityOp(lhs, rhs);
    break;
  case clang::BO_NE:
    *node = SageBuilder::buildNotEqualOp(lhs, rhs);
    break;
  case clang::BO_And:
    *node = SageBuilder::buildBitAndOp(lhs, rhs);
    break;
  case clang::BO_Xor:
    *node = SageBuilder::buildBitXorOp(lhs, rhs);
    break;
  case clang::BO_Or:
    *node = SageBuilder::buildBitOrOp(lhs, rhs);
    break;
  case clang::BO_LAnd:
    *node = SageBuilder::buildAndOp(lhs, rhs);
    break;
  case clang::BO_LOr:
    *node = SageBuilder::buildOrOp(lhs, rhs);
    break;
  case clang::BO_Assign:
    *node = SageBuilder::buildAssignOp(lhs, rhs);
    break;
  case clang::BO_MulAssign:
    *node = SageBuilder::buildMultAssignOp(lhs, rhs);
    break;
  case clang::BO_DivAssign:
    *node = SageBuilder::buildDivAssignOp(lhs, rhs);
    break;
  case clang::BO_RemAssign:
    *node = SageBuilder::buildModAssignOp(lhs, rhs);
    break;
  case clang::BO_AddAssign:
    *node = SageBuilder::buildPlusAssignOp(lhs, rhs);
    break;
  case clang::BO_SubAssign:
    *node = SageBuilder::buildMinusAssignOp(lhs, rhs);
    break;
  case clang::BO_ShlAssign:
    *node = SageBuilder::buildLshiftAssignOp(lhs, rhs);
    break;
  case clang::BO_ShrAssign:
    *node = SageBuilder::buildRshiftAssignOp(lhs, rhs);
    break;
  case clang::BO_AndAssign:
    *node = SageBuilder::buildAndAssignOp(lhs, rhs);
    break;
  case clang::BO_XorAssign:
    *node = SageBuilder::buildXorAssignOp(lhs, rhs);
    break;
  case clang::BO_OrAssign:
    *node = SageBuilder::buildIorAssignOp(lhs, rhs);
    break;
  case clang::BO_Comma:
    *node = SageBuilder::buildCommaOpExp(lhs, rhs);
    break;
  default:
    std::cerr << "Unknown opcode for binary operator: "
              << binary_operator->getOpcodeStr().str() << std::endl;
    res = false;
  }

  return VisitExpr(binary_operator, node) && res;
}

bool ClangToSageTranslator::VisitCompoundAssignOperator(
    clang::CompoundAssignOperator *compound_assign_operator, SgNode **node) {
#if DEBUG_VISIT_STMT
  std::cerr << "ClangToSageTranslator::VisitCompoundAssignOperator"
            << std::endl;
#endif
  bool res = true;

  // TODO

  return VisitBinaryOperator(compound_assign_operator, node) && res;
}

bool ClangToSageTranslator::VisitBlockExpr(clang::BlockExpr *block_expr,
                                           SgNode **node) {
#if DEBUG_VISIT_STMT
  std::cerr << "ClangToSageTranslator::VisitBlockExpr" << std::endl;
#endif
  bool res = true;

  // TODO

  return VisitExpr(block_expr, node) && res;
}

bool ClangToSageTranslator::VisitCallExpr(clang::CallExpr *call_expr,
                                          SgNode **node) {
#if DEBUG_VISIT_STMT
  std::cerr << "ClangToSageTranslator::VisitCallExpr" << std::endl;
#endif

  bool res = true;

  // Phase C (Issue 115): Queue implicit template instantiations that are
  // referenced from user code. They will be translated after the TU decl pass
  // completes so their bodies can be materialized with resolved references.
  if (clang::FunctionDecl *direct_callee = call_expr->getDirectCallee()) {
    clang::TemplateSpecializationKind kind =
        direct_callee->getTemplateSpecializationKind();
    if (kind == clang::TSK_ImplicitInstantiation ||
        kind == clang::TSK_ExplicitInstantiationDefinition) {
      bool eligible = true;
      if (p_compiler_instance != nullptr) {
        clang::SourceManager &sm = p_compiler_instance->getSourceManager();
        clang::SourceLocation callee_loc = direct_callee->getLocation();
        clang::SourceLocation call_loc = call_expr->getExprLoc();
        const bool call_in_system = !call_loc.isValid() ||
                                    sm.isInSystemHeader(call_loc) ||
                                    sm.isWrittenInBuiltinFile(call_loc);
        if (!callee_loc.isValid()) {
          eligible = false;
        } else if ((sm.isInSystemHeader(callee_loc) ||
                    sm.isWrittenInBuiltinFile(callee_loc)) &&
                   call_in_system) {
          eligible = false;
        }
      }

      if (eligible &&
          p_pending_implicit_function_instantiations_set.insert(direct_callee)
              .second) {
        p_pending_implicit_function_instantiations.push_back(direct_callee);
      }
    }
  }

  SgExpression *expr = nullptr;
  clang::Expr *callee_expr = call_expr->getCallee();
  clang::Expr *callee_base =
      callee_expr != nullptr ? callee_expr->IgnoreParenImpCasts() : nullptr;
  SgNode *tmp_expr = Traverse(callee_expr);
  expr = isSgExpression(tmp_expr);
  if (tmp_expr != nullptr && expr == nullptr) {
    std::cerr << "Runtime error: tmp_expr != nullptr && expr == nullptr"
              << std::endl;
    res = false;
  }

  auto maybe_resolve_unresolved_overload_callee = [&]() {
    clang::UnresolvedLookupExpr *unresolved_lookup =
        llvm::dyn_cast_or_null<clang::UnresolvedLookupExpr>(callee_base);
    if (unresolved_lookup == nullptr) {
      return;
    }
    if (unresolved_lookup->hasExplicitTemplateArgs()) {
      return;
    }
    if (expr != nullptr && isSgNonrealRefExp(expr) == nullptr) {
      return;
    }

    clang::NamedDecl *selected_named_decl = nullptr;
    clang::FunctionDecl *selected_function_decl = nullptr;
    clang::Decl *selected_canonical_decl = nullptr;
    const unsigned call_arg_count = call_expr->getNumArgs();

    for (auto it = unresolved_lookup->decls_begin();
         it != unresolved_lookup->decls_end(); ++it) {
      clang::NamedDecl *named_decl = it.getDecl();
      if (named_decl == nullptr) {
        continue;
      }
      if (clang::UsingShadowDecl *shadow =
              llvm::dyn_cast<clang::UsingShadowDecl>(named_decl)) {
        named_decl = shadow->getTargetDecl();
      }
      if (named_decl == nullptr) {
        continue;
      }

      clang::FunctionDecl *function_decl = nullptr;
      if (clang::FunctionTemplateDecl *template_decl =
              llvm::dyn_cast<clang::FunctionTemplateDecl>(named_decl)) {
        function_decl = template_decl->getTemplatedDecl();
      } else {
        function_decl = llvm::dyn_cast<clang::FunctionDecl>(named_decl);
      }
      if (function_decl == nullptr) {
        return;
      }

      const unsigned min_args = function_decl->getMinRequiredArguments();
      const unsigned max_args = function_decl->getNumParams();
      const bool viable_by_arity =
          call_arg_count >= min_args &&
          (function_decl->isVariadic() || call_arg_count <= max_args);
      if (!viable_by_arity) {
        continue;
      }

      clang::Decl *canonical_decl = function_decl->getCanonicalDecl();
      if (selected_canonical_decl != nullptr &&
          selected_canonical_decl != canonical_decl) {
        selected_named_decl = nullptr;
        selected_function_decl = nullptr;
        selected_canonical_decl = nullptr;
        return;
      }

      selected_named_decl = named_decl;
      selected_function_decl = function_decl;
      selected_canonical_decl = canonical_decl;
    }

    if (selected_named_decl == nullptr) {
      return;
    }

    auto unwrap_directive_decl =
        [](SgDeclarationStatement *decl) -> SgDeclarationStatement * {
      if (SgTemplateInstantiationDirectiveStatement *inst_directive =
              isSgTemplateInstantiationDirectiveStatement(decl)) {
        return inst_directive->get_declaration();
      }
      return decl;
    };

    auto ensure_template_member_symbol =
        [&](SgTemplateMemberFunctionDeclaration *decl)
        -> SgTemplateMemberFunctionSymbol * {
      if (decl == nullptr) {
        return nullptr;
      }
      registerDeclarationSymbol(decl);
      SgTemplateMemberFunctionSymbol *sym = isSgTemplateMemberFunctionSymbol(
          decl->get_symbol_from_symbol_table());
      if (sym != nullptr) {
        return sym;
      }

      SgScopeStatement *decl_scope = decl->get_scope();
      if (decl_scope == nullptr) {
        decl_scope = SageBuilder::topScopeStack();
      }
      if (decl_scope != nullptr) {
        SgTemplateParameterPtrList *params = &decl->get_templateParameters();
        sym = decl_scope->lookup_template_member_function_symbol(
            decl->get_name(), decl->get_type(), params);
      }
      if (sym == nullptr) {
        sym = new SgTemplateMemberFunctionSymbol(decl);
        attachSymbolToScopeOrOrphan(sym, decl_scope);
      }
      return sym;
    };

    auto ensure_template_function_symbol =
        [&](SgTemplateFunctionDeclaration *decl) -> SgTemplateFunctionSymbol * {
      if (decl == nullptr) {
        return nullptr;
      }
      registerDeclarationSymbol(decl);
      SgTemplateFunctionSymbol *sym =
          isSgTemplateFunctionSymbol(decl->get_symbol_from_symbol_table());
      if (sym != nullptr) {
        return sym;
      }

      SgScopeStatement *decl_scope = normalizeNamespaceScope(decl->get_scope());
      if (decl_scope == nullptr) {
        decl_scope = SageBuilder::topScopeStack();
      }
      if (decl_scope != nullptr) {
        SgTemplateParameterPtrList *params = &decl->get_templateParameters();
        sym = decl_scope->lookup_template_function_symbol(
            decl->get_name(), decl->get_type(), params);
      }
      if (sym == nullptr) {
        sym = new SgTemplateFunctionSymbol(decl);
        attachSymbolToScopeOrOrphan(sym, decl_scope);
      }
      return sym;
    };

    auto ensure_member_symbol =
        [&](SgMemberFunctionDeclaration *decl) -> SgMemberFunctionSymbol * {
      if (decl == nullptr) {
        return nullptr;
      }
      registerDeclarationSymbol(decl);
      SgMemberFunctionSymbol *sym =
          isSgMemberFunctionSymbol(decl->get_symbol_from_symbol_table());
      if (sym != nullptr) {
        return sym;
      }

      SgScopeStatement *decl_scope = decl->get_scope();
      if (decl_scope == nullptr) {
        decl_scope = SageBuilder::topScopeStack();
      }
      if (decl_scope != nullptr) {
        sym = decl_scope->lookup_nontemplate_member_function_symbol(
            decl->get_name(), decl->get_type(), nullptr);
      }
      if (sym == nullptr) {
        sym = new SgMemberFunctionSymbol(decl);
        attachSymbolToScopeOrOrphan(sym, decl_scope);
      }
      return sym;
    };

    auto ensure_function_symbol =
        [&](SgFunctionDeclaration *decl) -> SgFunctionSymbol * {
      if (decl == nullptr) {
        return nullptr;
      }
      registerDeclarationSymbol(decl);
      SgFunctionSymbol *sym =
          isSgFunctionSymbol(decl->get_symbol_from_symbol_table());
      if (sym != nullptr) {
        return sym;
      }

      SgScopeStatement *decl_scope = normalizeNamespaceScope(decl->get_scope());
      if (decl_scope == nullptr) {
        decl_scope = SageBuilder::topScopeStack();
      }
      if (decl_scope != nullptr) {
        sym = decl_scope->lookup_nontemplate_function_symbol(
            decl->get_name(), decl->get_type(), nullptr);
        if (sym == nullptr) {
          sym = decl_scope->lookup_function_symbol(decl->get_name(),
                                                   decl->get_type());
        }
      }
      if (sym == nullptr) {
        sym = new SgFunctionSymbol(decl);
        attachSymbolToScopeOrOrphan(sym, decl_scope);
      }
      return sym;
    };

    SgDeclarationStatement *decl_stmt =
        unwrap_directive_decl(lookupSgDeclarationForClangDecl(
            llvm::cast<clang::Decl>(selected_named_decl),
            /*allow_on_demand=*/true));
    if (decl_stmt == nullptr && selected_function_decl != nullptr) {
      decl_stmt = unwrap_directive_decl(lookupSgDeclarationForClangDecl(
          selected_function_decl, /*allow_on_demand=*/true));
    }
    if (decl_stmt == nullptr) {
      return;
    }

    SgExpression *resolved_ref = nullptr;
    if (SgTemplateMemberFunctionDeclaration *tmpl_member_decl =
            isSgTemplateMemberFunctionDeclaration(decl_stmt)) {
      if (SgTemplateMemberFunctionSymbol *sym =
              ensure_template_member_symbol(tmpl_member_decl)) {
        resolved_ref = SageBuilder::buildTemplateMemberFunctionRefExp_nfi(
            sym, false, false);
      }
    } else if (SgTemplateFunctionDeclaration *tmpl_func_decl =
                   isSgTemplateFunctionDeclaration(decl_stmt)) {
      if (SgTemplateFunctionSymbol *sym =
              ensure_template_function_symbol(tmpl_func_decl)) {
        resolved_ref = SageBuilder::buildTemplateFunctionRefExp_nfi(sym);
      }
    } else if (SgMemberFunctionDeclaration *member_decl =
                   isSgMemberFunctionDeclaration(decl_stmt)) {
      if (SgMemberFunctionSymbol *sym = ensure_member_symbol(member_decl)) {
        resolved_ref =
            SageBuilder::buildMemberFunctionRefExp_nfi(sym, false, false);
      }
    } else if (SgFunctionDeclaration *function_decl =
                   isSgFunctionDeclaration(decl_stmt)) {
      if (SgFunctionSymbol *sym = ensure_function_symbol(function_decl)) {
        resolved_ref = SageBuilder::buildFunctionRefExp(sym);
      }
    }

    if (resolved_ref == nullptr) {
      return;
    }

    clang::NestedNameSpecifierLoc qualifier_loc =
        unresolved_lookup->getQualifierLoc();
    const clang::NestedNameSpecifierLoc *qualifier_loc_ptr =
        qualifier_loc.getNestedNameSpecifier() ? &qualifier_loc : nullptr;
    clang::NestedNameSpecifier qualifier = unresolved_lookup->getQualifier();
    if (!qualifier && qualifier_loc_ptr != nullptr) {
      qualifier = qualifier_loc_ptr->getNestedNameSpecifier();
    }
    attachExplicitQualifierFromNestedName(
        resolved_ref, qualifier, qualifier_loc_ptr, p_compiler_instance);
    applySourceRange(resolved_ref, unresolved_lookup->getSourceRange());
    expr = resolved_ref;
  };

  maybe_resolve_unresolved_overload_callee();

  auto maybe_upgrade_template_callee = [&]() {
    if (expr == nullptr) {
      return;
    }

    // Preserve explicitly-qualified source spellings. Replacing these refs with
    // synthesized instantiation symbols can silently drop qualification.
    if (const clang::DeclRefExpr *decl_ref =
            llvm::dyn_cast_or_null<clang::DeclRefExpr>(callee_base)) {
      if (decl_ref->hasQualifier()) {
        return;
      }
    } else if (const clang::MemberExpr *member_ref =
                   llvm::dyn_cast_or_null<clang::MemberExpr>(callee_base)) {
      if (member_ref->hasQualifier()) {
        return;
      }
    }

    clang::FunctionDecl *direct_callee = call_expr->getDirectCallee();
    if (direct_callee == nullptr) {
      return;
    }

    clang::FunctionDecl *template_arg_decl = direct_callee;
    if (clang::DeclRefExpr *decl_ref =
            llvm::dyn_cast_or_null<clang::DeclRefExpr>(callee_base)) {
      if (clang::FunctionDecl *decl_func =
              llvm::dyn_cast<clang::FunctionDecl>(decl_ref->getDecl())) {
        template_arg_decl = decl_func;
      }
    } else if (clang::MemberExpr *member_ref =
                   llvm::dyn_cast_or_null<clang::MemberExpr>(callee_base)) {
      if (clang::FunctionDecl *decl_func = llvm::dyn_cast<clang::FunctionDecl>(
              member_ref->getMemberDecl())) {
        template_arg_decl = decl_func;
      }
    }

    SgExpression *ref_expr = expr;
    SgBinaryOp *member_access = nullptr;
    if (SgDotExp *dot = isSgDotExp(expr)) {
      member_access = dot;
      ref_expr = dot->get_rhs_operand();
    } else if (SgArrowExp *arrow = isSgArrowExp(expr)) {
      member_access = arrow;
      ref_expr = arrow->get_rhs_operand();
    }

    SgFunctionRefExp *func_ref = isSgFunctionRefExp(ref_expr);
    SgMemberFunctionRefExp *member_ref = isSgMemberFunctionRefExp(ref_expr);
    if (func_ref == nullptr && member_ref == nullptr) {
      return;
    }

    SgFunctionSymbol *func_sym =
        func_ref != nullptr ? func_ref->get_symbol() : nullptr;
    SgMemberFunctionSymbol *member_sym =
        member_ref != nullptr ? member_ref->get_symbol() : nullptr;
    SgFunctionDeclaration *ref_decl = nullptr;
    if (member_sym != nullptr) {
      ref_decl = isSgFunctionDeclaration(member_sym->get_declaration());
    } else if (func_sym != nullptr) {
      ref_decl = isSgFunctionDeclaration(func_sym->get_declaration());
    }
    if (ref_decl == nullptr) {
      return;
    }

    bool is_template_ref =
        isSgTemplateFunctionDeclaration(ref_decl) != nullptr ||
        isSgTemplateMemberFunctionDeclaration(ref_decl) != nullptr ||
        isSgTemplateInstantiationFunctionDecl(ref_decl) != nullptr ||
        isSgTemplateInstantiationMemberFunctionDecl(ref_decl) != nullptr;
    if (!is_template_ref) {
      if (SgFunctionDeclaration *first_nondef = isSgFunctionDeclaration(
              ref_decl->get_firstNondefiningDeclaration())) {
        is_template_ref =
            isSgTemplateFunctionDeclaration(first_nondef) != nullptr ||
            isSgTemplateMemberFunctionDeclaration(first_nondef) != nullptr ||
            isSgTemplateInstantiationFunctionDecl(first_nondef) != nullptr ||
            isSgTemplateInstantiationMemberFunctionDecl(first_nondef) !=
                nullptr;
      }
    }
    if (!is_template_ref) {
      return;
    }

    const clang::TemplateArgumentList *clang_args =
        template_arg_decl->getTemplateSpecializationArgs();
    if ((clang_args == nullptr || clang_args->size() == 0) &&
        template_arg_decl != nullptr) {
      if (const clang::FunctionTemplateSpecializationInfo *spec_info =
              template_arg_decl->getTemplateSpecializationInfo()) {
        clang_args = spec_info->TemplateArguments;
      }
    }
    if (clang_args == nullptr || clang_args->size() == 0) {
      return;
    }

    bool has_call_explicit_template_args = false;
    if (const clang::DeclRefExpr *decl_ref =
            llvm::dyn_cast_or_null<clang::DeclRefExpr>(callee_base)) {
      has_call_explicit_template_args = decl_ref->hasExplicitTemplateArgs();
    } else if (const clang::MemberExpr *member_ref =
                   llvm::dyn_cast_or_null<clang::MemberExpr>(callee_base)) {
      has_call_explicit_template_args = member_ref->hasExplicitTemplateArgs();
    }

    clang::TemplateArgumentListInfo explicit_arg_info;
    auto get_explicit_template_arg_info =
        [&](clang::TemplateArgumentListInfo &arg_info) -> bool {
      if (const clang::DeclRefExpr *decl_ref =
              llvm::dyn_cast_or_null<clang::DeclRefExpr>(callee_base)) {
        if (decl_ref->hasExplicitTemplateArgs()) {
          decl_ref->copyTemplateArgumentsInto(arg_info);
          return true;
        }
      }
      if (const clang::MemberExpr *member_ref =
              llvm::dyn_cast_or_null<clang::MemberExpr>(callee_base)) {
        if (member_ref->hasExplicitTemplateArgs()) {
          member_ref->copyTemplateArgumentsInto(arg_info);
          return true;
        }
      }
      if (const clang::ASTTemplateArgumentListInfo *args_as_written =
              template_arg_decl->getTemplateSpecializationArgsAsWritten()) {
        arg_info = clang::TemplateArgumentListInfo(
            args_as_written->getLAngleLoc(), args_as_written->getRAngleLoc());
        for (const clang::TemplateArgumentLoc &loc :
             args_as_written->arguments()) {
          arg_info.addArgument(loc);
        }
        return arg_info.getLAngleLoc().isValid() &&
               arg_info.getRAngleLoc().isValid();
      }
      return false;
    };

    size_t explicit_arg_count = 0;
    size_t call_source_count = 0;
    const bool has_explicit_arg_info =
        get_explicit_template_arg_info(explicit_arg_info);
    bool has_call_explicit_template_id =
        has_call_explicit_template_args || has_explicit_arg_info;
    bool has_explicit_empty_template_id =
        has_explicit_arg_info &&
        hasExplicitEmptyTemplateArgumentList(explicit_arg_info);
    if (has_explicit_arg_info) {
      explicit_arg_count = countExpandedTemplateArguments(explicit_arg_info);
    }

    if (explicit_arg_count == 0 && callee_expr != nullptr &&
        p_compiler_instance != nullptr) {
      clang::SourceManager &sm = p_compiler_instance->getSourceManager();
      const clang::LangOptions &lang_opts = p_compiler_instance->getLangOpts();
      clang::Expr *range_expr =
          callee_base != nullptr ? callee_base : callee_expr;
      ExplicitTemplateArgumentSourceInfo source_info =
          scanExplicitTemplateArgumentsInSourceRange(
              range_expr->getSourceRange(), sm, lang_opts);
      if (!source_info.has_template_argument_list) {
        clang::SourceLocation lparen =
            findMatchingLParen(call_expr->getRParenLoc(), sm);
        if (lparen.isValid()) {
          source_info = scanExplicitTemplateArgumentsInSourceRange(
              clang::SourceRange(range_expr->getBeginLoc(), lparen), sm,
              lang_opts);
        } else {
          source_info = scanExplicitTemplateArgumentsInSourceRange(
              call_expr->getSourceRange(), sm, lang_opts);
        }
      }
      if (source_info.has_template_argument_list) {
        has_call_explicit_template_id = true;
        explicit_arg_count = source_info.argument_count;
        call_source_count = source_info.argument_count;
        has_explicit_empty_template_id = (source_info.argument_count == 0);
      }
    }
    if (!has_call_explicit_template_id) {
      return;
    }

    SgTemplateArgumentPtrList template_args;
    if (has_explicit_arg_info &&
        (explicit_arg_count > 0 || has_explicit_empty_template_id)) {
      template_args = buildTemplateArguments(explicit_arg_info, true);
    }
    if (template_args.empty() && !has_explicit_empty_template_id) {
      if (explicit_arg_count == 0 && has_explicit_arg_info &&
          clang_args->size() != 0) {
        explicit_arg_count = clang_args->size();
      }
      template_args = buildTemplateArguments(*clang_args, explicit_arg_count);
    }
    if (!has_explicit_empty_template_id && explicit_arg_count == 0 &&
        call_source_count > 0 && !template_args.empty()) {
      explicit_arg_count = template_args.size();
    }
    if (!has_explicit_empty_template_id && explicit_arg_count == 0 &&
        has_explicit_arg_info && !template_args.empty()) {
      explicit_arg_count = template_args.size();
    }
    applyExplicitTemplateArgumentFlags(template_args, explicit_arg_count);
    const bool has_explicit_args =
        has_call_explicit_template_id || explicit_arg_count > 0 ||
        has_explicit_arg_info || call_source_count > 0;
    SgTemplateArgumentPtrList *template_args_ptr = &template_args;

    auto sync_function_instantiation_args = [&](SgFunctionDeclaration *decl) {
      auto apply_to_decl = [&](SgFunctionDeclaration *candidate) {
        if (SgTemplateInstantiationFunctionDecl *inst_decl =
                isSgTemplateInstantiationFunctionDecl(candidate)) {
          inst_decl->set_template_argument_list_is_explicit(has_explicit_args);
          SageBuilder::setTemplateArgumentsInDeclaration(inst_decl,
                                                         template_args_ptr);
        }
      };
      apply_to_decl(decl);
      if (decl != nullptr) {
        apply_to_decl(
            isSgFunctionDeclaration(decl->get_firstNondefiningDeclaration()));
        apply_to_decl(isSgFunctionDeclaration(decl->get_definingDeclaration()));
      }
    };

    auto sync_member_instantiation_args =
        [&](SgMemberFunctionDeclaration *decl) {
          auto apply_to_decl = [&](SgMemberFunctionDeclaration *candidate) {
            if (SgTemplateInstantiationMemberFunctionDecl *inst_decl =
                    isSgTemplateInstantiationMemberFunctionDecl(candidate)) {
              inst_decl->set_template_argument_list_is_explicit(
                  has_explicit_args);
              SageBuilder::setTemplateArgumentsInDeclaration(inst_decl,
                                                             template_args_ptr);
            }
          };
          apply_to_decl(decl);
          if (decl != nullptr) {
            apply_to_decl(isSgMemberFunctionDeclaration(
                decl->get_firstNondefiningDeclaration()));
            apply_to_decl(
                isSgMemberFunctionDeclaration(decl->get_definingDeclaration()));
          }
        };

    auto set_compiler_generated_on_instantiation_chain =
        [&](SgFunctionDeclaration *decl) {
          if (decl == nullptr) {
            return;
          }
          for_each_unique_function_decl_in_chain(
              decl, [&](SgFunctionDeclaration *candidate_decl) {
                Sg_File_Info *file_info = candidate_decl->get_file_info();
                const bool touches_synthetic_candidate =
                    candidate_decl == decl || file_info == nullptr ||
                    file_info->isCompilerGenerated();
                if (!touches_synthetic_candidate) {
                  return;
                }

                setCompilerGeneratedFileInfo(candidate_decl);
                if (SgFunctionParameterList *params =
                        candidate_decl->get_parameterList()) {
                  setCompilerGeneratedFileInfo(params);
                  for (SgInitializedName *param : params->get_args()) {
                    if (param != nullptr) {
                      setCompilerGeneratedFileInfo(param);
                    }
                  }
                }
              });
        };

    auto finalize_synthesized_instantiation_decl =
        [&](SgFunctionDeclaration *decl, SgScopeStatement *scope,
            const char *context) {
          if (decl == nullptr) {
            return;
          }
          (void)context;

          SgScopeStatement *target_scope = scope;
          if (target_scope != nullptr) {
            target_scope = normalizeNamespaceScope(target_scope);
          }
          mark_synthesized_instantiation_decl_chain_for_ref(decl, target_scope);
          set_compiler_generated_on_instantiation_chain(decl);

          registerDeclarationSymbol(decl);
          if (decl->get_symbol_from_symbol_table() == nullptr &&
              decl->get_scope() != nullptr) {
            if (SgSymbol *sym = buildSymbolForDeclaration(decl)) {
              attachSymbolToScopeOrOrphan(sym, decl->get_scope());
            }
          }
        };

    auto apply_callee_qualifier = [&](SgExpression *new_ref) {
      if (new_ref == nullptr || p_compiler_instance == nullptr) {
        return;
      }
      if (const clang::DeclRefExpr *decl_ref =
              llvm::dyn_cast_or_null<clang::DeclRefExpr>(callee_base)) {
        if (decl_ref->hasQualifier()) {
          clang::NestedNameSpecifierLoc qualifier_loc =
              decl_ref->getQualifierLoc();
          const clang::NestedNameSpecifierLoc *qualifier_loc_ptr =
              qualifier_loc.getNestedNameSpecifier() ? &qualifier_loc : nullptr;
          clang::NestedNameSpecifier qualifier = decl_ref->getQualifier();
          if (!qualifier && qualifier_loc_ptr != nullptr) {
            qualifier = qualifier_loc_ptr->getNestedNameSpecifier();
          }
          if (qualifier) {
            attachExplicitQualifierFromNestedName(
                new_ref, qualifier, qualifier_loc_ptr, p_compiler_instance);
          }
        }
      } else if (const clang::MemberExpr *member_ref =
                     llvm::dyn_cast_or_null<clang::MemberExpr>(callee_base)) {
        if (member_ref->hasQualifier()) {
          clang::NestedNameSpecifierLoc qualifier_loc =
              member_ref->getQualifierLoc();
          const clang::NestedNameSpecifierLoc *qualifier_loc_ptr =
              qualifier_loc.getNestedNameSpecifier() ? &qualifier_loc : nullptr;
          clang::NestedNameSpecifier qualifier = member_ref->getQualifier();
          if (!qualifier && qualifier_loc_ptr != nullptr) {
            qualifier = qualifier_loc_ptr->getNestedNameSpecifier();
          }
          if (qualifier) {
            attachExplicitQualifierFromNestedName(
                new_ref, qualifier, qualifier_loc_ptr, p_compiler_instance);
          }
        }
      }
    };

    SgSymbol *direct_sym = GetSymbolFromSymbolTable(template_arg_decl);
    if (direct_sym == nullptr ||
        isSgTemplateFunctionSymbol(direct_sym) != nullptr ||
        isSgTemplateMemberFunctionSymbol(direct_sym) != nullptr) {
      TraverseOnDemand(template_arg_decl);
      direct_sym = GetSymbolFromSymbolTable(template_arg_decl);
    }

    auto replace_ref = [&](SgExpression *new_ref) {
      if (new_ref == nullptr) {
        return;
      }
      apply_callee_qualifier(new_ref);
      if (member_access != nullptr) {
        member_access->set_rhs_operand(new_ref);
        new_ref->set_parent(member_access);
      } else {
        expr = new_ref;
      }
    };

    if (SgMemberFunctionSymbol *inst_member_sym =
            isSgMemberFunctionSymbol(direct_sym)) {
      SgMemberFunctionDeclaration *inst_decl =
          isSgMemberFunctionDeclaration(inst_member_sym->get_declaration());
      SgTemplateInstantiationMemberFunctionDecl *inst_tmpl_decl =
          isSgTemplateInstantiationMemberFunctionDecl(inst_decl);
      if (inst_tmpl_decl == nullptr && inst_decl != nullptr) {
        inst_tmpl_decl = isSgTemplateInstantiationMemberFunctionDecl(
            inst_decl->get_firstNondefiningDeclaration());
      }
      if (inst_tmpl_decl != nullptr) {
        sync_member_instantiation_args(
            isSgMemberFunctionDeclaration(inst_tmpl_decl));
        SgExpression *new_ref = SageBuilder::buildMemberFunctionRefExp_nfi(
            inst_member_sym, false, false);
        replace_ref(new_ref);
        return;
      }
    }

    if (SgFunctionSymbol *inst_func_sym = isSgFunctionSymbol(direct_sym)) {
      SgFunctionDeclaration *inst_decl =
          isSgFunctionDeclaration(inst_func_sym->get_declaration());
      SgTemplateInstantiationFunctionDecl *inst_tmpl_decl =
          isSgTemplateInstantiationFunctionDecl(inst_decl);
      if (inst_tmpl_decl == nullptr && inst_decl != nullptr) {
        inst_tmpl_decl = isSgTemplateInstantiationFunctionDecl(
            inst_decl->get_firstNondefiningDeclaration());
      }
      if (inst_tmpl_decl != nullptr) {
        sync_function_instantiation_args(
            isSgFunctionDeclaration(inst_tmpl_decl));
        SgExpression *new_ref = SageBuilder::buildFunctionRefExp(inst_func_sym);
        replace_ref(new_ref);
        return;
      }
    }

    SgScopeStatement *func_scope = nullptr;
    if (member_sym != nullptr) {
      func_scope = member_sym->get_scope();
    } else if (func_sym != nullptr) {
      func_scope = func_sym->get_scope();
    }
    if (func_scope == nullptr) {
      func_scope = SageBuilder::topScopeStack();
    }

    auto try_use_mapped_instantiation = [&]() -> bool {
      SgDeclarationStatement *mapped_decl =
          lookupSgDeclarationForClangDecl(template_arg_decl,
                                          /*allow_on_demand=*/true);
      if (SgTemplateInstantiationDirectiveStatement *inst_directive =
              isSgTemplateInstantiationDirectiveStatement(mapped_decl)) {
        mapped_decl = inst_directive->get_declaration();
      }

      auto resolve_function_symbol =
          [&](SgFunctionDeclaration *candidate_decl) -> SgFunctionSymbol * {
        if (candidate_decl == nullptr ||
            isSgTemplateInstantiationFunctionDecl(candidate_decl) == nullptr) {
          return nullptr;
        }
        sync_function_instantiation_args(candidate_decl);
        registerDeclarationSymbol(candidate_decl);
        SgFunctionSymbol *candidate_sym =
            isSgFunctionSymbol(candidate_decl->get_symbol_from_symbol_table());
        if (candidate_sym == nullptr && func_scope != nullptr) {
          candidate_sym = new SgFunctionSymbol(candidate_decl);
          attachSymbolToScopeOrOrphan(candidate_sym, func_scope);
        }
        return candidate_sym;
      };

      auto resolve_member_symbol =
          [&](SgMemberFunctionDeclaration *candidate_decl)
          -> SgMemberFunctionSymbol * {
        if (candidate_decl == nullptr ||
            isSgTemplateInstantiationMemberFunctionDecl(candidate_decl) ==
                nullptr) {
          return nullptr;
        }
        sync_member_instantiation_args(candidate_decl);
        registerDeclarationSymbol(candidate_decl);
        SgMemberFunctionSymbol *candidate_sym = isSgMemberFunctionSymbol(
            candidate_decl->get_symbol_from_symbol_table());
        if (candidate_sym == nullptr && func_scope != nullptr) {
          candidate_sym = new SgMemberFunctionSymbol(candidate_decl);
          attachSymbolToScopeOrOrphan(candidate_sym, func_scope);
        }
        return candidate_sym;
      };

      if (member_sym != nullptr) {
        if (SgMemberFunctionDeclaration *member_decl =
                isSgMemberFunctionDeclaration(mapped_decl)) {
          SgDeclarationStatement *decls_to_try[] = {
              member_decl, member_decl->get_firstNondefiningDeclaration(),
              member_decl->get_definingDeclaration()};
          for (SgDeclarationStatement *decl_stmt : decls_to_try) {
            if (SgMemberFunctionDeclaration *cand_decl =
                    isSgMemberFunctionDeclaration(decl_stmt)) {
              if (SgMemberFunctionSymbol *candidate_sym =
                      resolve_member_symbol(cand_decl)) {
                SgExpression *new_ref =
                    SageBuilder::buildMemberFunctionRefExp_nfi(candidate_sym,
                                                               false, false);
                replace_ref(new_ref);
                return true;
              }
            }
          }
        }
      } else if (func_sym != nullptr) {
        if (SgFunctionDeclaration *function_decl =
                isSgFunctionDeclaration(mapped_decl)) {
          SgDeclarationStatement *decls_to_try[] = {
              function_decl, function_decl->get_firstNondefiningDeclaration(),
              function_decl->get_definingDeclaration()};
          for (SgDeclarationStatement *decl_stmt : decls_to_try) {
            if (SgFunctionDeclaration *cand_decl =
                    isSgFunctionDeclaration(decl_stmt)) {
              if (SgFunctionSymbol *candidate_sym =
                      resolve_function_symbol(cand_decl)) {
                SgExpression *new_ref =
                    SageBuilder::buildFunctionRefExp(candidate_sym);
                replace_ref(new_ref);
                return true;
              }
            }
          }
        }
      }

      return false;
    };

    if (try_use_mapped_instantiation()) {
      return;
    }

    SgFunctionDeclaration *base_decl = ref_decl;
    SgName template_base_name = ref_decl->get_name();
    if (SgTemplateFunctionDeclaration *tmpl_decl =
            isSgTemplateFunctionDeclaration(ref_decl)) {
      template_base_name = tmpl_decl->get_name();
    } else if (SgTemplateMemberFunctionDeclaration *tmpl_decl =
                   isSgTemplateMemberFunctionDeclaration(ref_decl)) {
      template_base_name = tmpl_decl->get_name();
    } else if (SgTemplateInstantiationFunctionDecl *inst_decl =
                   isSgTemplateInstantiationFunctionDecl(ref_decl)) {
      if (inst_decl->get_templateName().is_null() == false) {
        template_base_name = inst_decl->get_templateName();
      }
    } else if (SgTemplateInstantiationMemberFunctionDecl *inst_decl =
                   isSgTemplateInstantiationMemberFunctionDecl(ref_decl)) {
      if (inst_decl->get_templateName().is_null() == false) {
        template_base_name = inst_decl->get_templateName();
      }
    }

    SgType *lookup_type = ref_decl->get_type();
    if (SgType *clang_type =
            buildTypeFromQualifiedType(template_arg_decl->getType())) {
      if (isSgFunctionType(clang_type) != nullptr ||
          isSgMemberFunctionType(clang_type) != nullptr ||
          lookup_type == nullptr) {
        lookup_type = clang_type;
      }
    }

    if (member_sym != nullptr) {
      SgMemberFunctionType *member_type = isSgMemberFunctionType(lookup_type);
      SgType *ret_type = member_type != nullptr
                             ? member_type->get_return_type()
                             : SageBuilder::buildUnknownType();
      SgFunctionParameterList *param_list =
          (member_type != nullptr &&
           member_type->get_argument_list() != nullptr)
              ? SageBuilder::buildFunctionParameterList_nfi(
                    member_type->get_argument_list())
              : SageBuilder::buildFunctionParameterList_nfi();
      unsigned int functionConstVolatileFlags =
          member_type != nullptr ? member_type->get_mfunc_specifier() : 0;

      SgMemberFunctionSymbol *inst_sym = nullptr;
      if (func_scope != nullptr) {
        SgFunctionSymbol *existing_sym = func_scope->lookup_function_symbol(
            template_base_name, lookup_type, template_args_ptr);
        if (existing_sym == nullptr && lookup_type != nullptr) {
          existing_sym = func_scope->lookup_function_symbol(template_base_name,
                                                            lookup_type);
        }
        if (existing_sym != nullptr) {
          if (isSgTemplateInstantiationMemberFunctionDecl(
                  existing_sym->get_declaration()) != nullptr) {
            inst_sym = isSgMemberFunctionSymbol(existing_sym);
          } else if (SgMemberFunctionDeclaration *first_nondef =
                         isSgMemberFunctionDeclaration(
                             existing_sym->get_declaration()
                                 ->get_firstNondefiningDeclaration())) {
            if (isSgTemplateInstantiationMemberFunctionDecl(first_nondef) !=
                nullptr) {
              inst_sym = isSgMemberFunctionSymbol(existing_sym);
            }
          }
        }
      }

      if (inst_sym == nullptr) {
        SgTemplateInstantiationMemberFunctionDecl *inst_decl =
            isSgTemplateInstantiationMemberFunctionDecl(
                SageBuilder::buildNondefiningMemberFunctionDeclaration(
                    template_base_name, ret_type, param_list, func_scope,
                    functionConstVolatileFlags,
                    /*buildTemplateInstantiation=*/true, template_args_ptr));
        ensure_function_param_list(inst_decl, param_list);
        if (inst_decl != nullptr) {
          sync_member_instantiation_args(inst_decl);
          propagateSpecialFunctionModifiers(base_decl, inst_decl);
          if (SgTemplateMemberFunctionDeclaration *tmpl_decl =
                  recover_instantiation_template_declaration<
                      SgTemplateMemberFunctionDeclaration,
                      SgTemplateInstantiationMemberFunctionDecl>(base_decl)) {
            apply_template_instantiation_template_links(inst_decl, tmpl_decl,
                                                        tmpl_decl->get_name());
          }

          finalize_synthesized_instantiation_decl(
              inst_decl, func_scope, "VisitCallExpr:template-instantiation");

          inst_sym = isSgMemberFunctionSymbol(
              inst_decl->get_symbol_from_symbol_table());
          if (inst_sym == nullptr && func_scope != nullptr) {
            inst_sym = new SgMemberFunctionSymbol(inst_decl);
            attachSymbolToScopeOrOrphan(inst_sym, func_scope);
          }
        }
      } else {
        sync_member_instantiation_args(
            isSgMemberFunctionDeclaration(inst_sym->get_declaration()));
        if (SgMemberFunctionDeclaration *inst_decl =
                isSgMemberFunctionDeclaration(inst_sym->get_declaration())) {
          propagateSpecialFunctionModifiers(base_decl, inst_decl);
          if (SgTemplateInstantiationMemberFunctionDecl *inst_tmpl_decl =
                  isSgTemplateInstantiationMemberFunctionDecl(inst_decl)) {
            if (SgTemplateMemberFunctionDeclaration *tmpl_decl =
                    recover_instantiation_template_declaration<
                        SgTemplateMemberFunctionDeclaration,
                        SgTemplateInstantiationMemberFunctionDecl>(base_decl)) {
              apply_template_instantiation_template_links(
                  inst_tmpl_decl, tmpl_decl, tmpl_decl->get_name());
            }
          }
        }
      }

      if (inst_sym != nullptr) {
        SgExpression *new_ref =
            SageBuilder::buildMemberFunctionRefExp_nfi(inst_sym, false, false);
        replace_ref(new_ref);
      }
    } else if (func_sym != nullptr) {
      SgFunctionType *func_type = isSgFunctionType(lookup_type);
      SgType *ret_type = func_type != nullptr ? func_type->get_return_type()
                                              : SageBuilder::buildUnknownType();
      SgFunctionParameterList *param_list =
          (func_type != nullptr && func_type->get_argument_list() != nullptr)
              ? SageBuilder::buildFunctionParameterList_nfi(
                    func_type->get_argument_list())
              : SageBuilder::buildFunctionParameterList_nfi();

      SgFunctionSymbol *inst_sym = nullptr;
      if (func_scope != nullptr) {
        inst_sym = func_scope->lookup_function_symbol(
            template_base_name, lookup_type, template_args_ptr);
        if (inst_sym == nullptr && lookup_type != nullptr) {
          inst_sym = func_scope->lookup_function_symbol(template_base_name,
                                                        lookup_type);
        }
        if (inst_sym != nullptr) {
          if (isSgTemplateInstantiationFunctionDecl(
                  inst_sym->get_declaration()) == nullptr) {
            if (SgFunctionDeclaration *first_nondef = isSgFunctionDeclaration(
                    inst_sym->get_declaration()
                        ->get_firstNondefiningDeclaration())) {
              if (isSgTemplateInstantiationFunctionDecl(first_nondef) ==
                  nullptr) {
                inst_sym = nullptr;
              }
            } else {
              inst_sym = nullptr;
            }
          }
        }
      }

      if (inst_sym == nullptr) {
        SgTemplateInstantiationFunctionDecl *inst_decl =
            isSgTemplateInstantiationFunctionDecl(
                SageBuilder::buildNondefiningFunctionDeclaration(
                    template_base_name, ret_type, param_list, func_scope,
                    /*buildTemplateInstantiation=*/true, template_args_ptr,
                    SgStorageModifier::e_default,
                    /*forceFreeFunctionScope=*/false));
        ensure_function_param_list(inst_decl, param_list);
        if (inst_decl != nullptr) {
          sync_function_instantiation_args(inst_decl);
          propagateSpecialFunctionModifiers(base_decl, inst_decl);
          if (SgTemplateFunctionDeclaration *tmpl_decl =
                  recover_instantiation_template_declaration<
                      SgTemplateFunctionDeclaration,
                      SgTemplateInstantiationFunctionDecl>(base_decl)) {
            apply_template_instantiation_template_links(inst_decl, tmpl_decl,
                                                        tmpl_decl->get_name());
          }

          finalize_synthesized_instantiation_decl(
              inst_decl, func_scope, "VisitCallExpr:template-instantiation");

          inst_sym =
              isSgFunctionSymbol(inst_decl->get_symbol_from_symbol_table());
          if (inst_sym == nullptr && func_scope != nullptr) {
            inst_sym = new SgFunctionSymbol(inst_decl);
            attachSymbolToScopeOrOrphan(inst_sym, func_scope);
          }
        }
      } else {
        sync_function_instantiation_args(
            isSgFunctionDeclaration(inst_sym->get_declaration()));
        if (SgFunctionDeclaration *inst_decl =
                isSgFunctionDeclaration(inst_sym->get_declaration())) {
          propagateSpecialFunctionModifiers(base_decl, inst_decl);
          if (SgTemplateInstantiationFunctionDecl *inst_tmpl_decl =
                  isSgTemplateInstantiationFunctionDecl(inst_decl)) {
            if (SgTemplateFunctionDeclaration *tmpl_decl =
                    recover_instantiation_template_declaration<
                        SgTemplateFunctionDeclaration,
                        SgTemplateInstantiationFunctionDecl>(base_decl)) {
              apply_template_instantiation_template_links(
                  inst_tmpl_decl, tmpl_decl, tmpl_decl->get_name());
            }
          }
        }
      }

      if (inst_sym != nullptr) {
        SgExpression *new_ref = SageBuilder::buildFunctionRefExp(inst_sym);
        replace_ref(new_ref);
      }
    }
  };

  maybe_upgrade_template_callee();

  SgExprListExp *param_list = SageBuilder::buildExprListExp_nfi();
  applySourceRange(param_list, call_expr->getSourceRange());

  clang::CallExpr::arg_iterator it;
  for (it = call_expr->arg_begin(); it != call_expr->arg_end(); ++it) {
    if (clang::isa<clang::CXXDefaultArgExpr>(*it)) {
      continue;
    }
    SgNode *tmp_expr = Traverse(*it);
    SgExpression *expr = isSgExpression(tmp_expr);
    if (tmp_expr != nullptr && expr == nullptr) {
      std::cerr << "Runtime error: tmp_expr != nullptr && expr == nullptr"
                << std::endl;
      res = false;
      continue;
    }
    param_list->append_expression(expr);
  }

  SgType *call_type = buildTypeFromQualifiedType(call_expr->getType());
  if (clang::CXXMemberCallExpr *member_call =
          llvm::dyn_cast<clang::CXXMemberCallExpr>(call_expr)) {
    clang::CXXMethodDecl *method_decl = member_call->getMethodDecl();
    if (method_decl != nullptr) {
      const clang::CXXRecordDecl *record_decl = method_decl->getParent();
      const clang::ClassTemplateSpecializationDecl *spec_decl = nullptr;
      clang::QualType object_type = member_call->getObjectType();
      if (!object_type.isNull()) {
        object_type = object_type.getNonReferenceType();
        clang::QualType canonical_object_type = object_type.getCanonicalType();
        if (!canonical_object_type.isNull()) {
          if (const clang::RecordType *record_type =
                  canonical_object_type->getAs<clang::RecordType>()) {
            if (const auto *object_spec =
                    llvm::dyn_cast<clang::ClassTemplateSpecializationDecl>(
                        record_type->getDecl())) {
              record_decl = object_spec;
              spec_decl = object_spec;
            }
          }
        }
        if (spec_decl == nullptr) {
          if (const clang::CXXRecordDecl *object_decl =
                  object_type->getAsCXXRecordDecl()) {
            if (const auto *object_spec =
                    llvm::dyn_cast<clang::ClassTemplateSpecializationDecl>(
                        object_decl)) {
              record_decl = object_decl;
              spec_decl = object_spec;
            }
          }
        }
      }
      if (spec_decl == nullptr) {
        spec_decl =
            llvm::dyn_cast_or_null<clang::ClassTemplateSpecializationDecl>(
                record_decl);
      }
      if (spec_decl != nullptr && record_decl != nullptr) {
        if (SgType *member_type = buildSpecializedMemberTypedefReturnType(
                method_decl, spec_decl, record_decl)) {
          call_type = member_type;
        }
      }
    }
  }
  if (call_type == nullptr && expr != nullptr) {
    call_type = expr->get_type();
  }

  auto is_implicit_conversion_call = [&]() -> bool {
    clang::FunctionDecl *direct_callee = call_expr->getDirectCallee();
    if (direct_callee == nullptr ||
        !llvm::isa<clang::CXXConversionDecl>(direct_callee)) {
      return false;
    }

    std::string spelled_call = getSourceText(call_expr->getSourceRange());
    if (spelled_call.empty()) {
      return true;
    }

    return spelled_call.find("operator") == std::string::npos;
  };

  SgFunctionCallExp *call_exp =
      new SgFunctionCallExp(expr, param_list, call_type);
  if (call_exp != nullptr) {
    if (expr != nullptr) {
      expr->set_parent(call_exp);
    }
    if (param_list != nullptr) {
      param_list->set_parent(call_exp);
    }
    SageInterface::setOneSourcePositionNull(call_exp);
    if (is_implicit_conversion_call()) {
      setCompilerGeneratedFileInfo(call_exp, true);
    }
  }
  *node = call_exp;

  return VisitExpr(call_expr, node) && res;
}

bool ClangToSageTranslator::VisitCUDAKernelCallExpr(
    clang::CUDAKernelCallExpr *cuda_kernel_call_expr, SgNode **node) {
#if DEBUG_VISIT_STMT
  std::cerr << "ClangToSageTranslator::VisitCUDAKernelCallExpr" << std::endl;
#endif
  bool res = true;

  auto translate_expr = [&](clang::Expr *expr,
                            const char *label) -> SgExpression * {
    if (expr == nullptr) {
      return nullptr;
    }

    SgNode *tmp_expr = Traverse(expr);
    SgExpression *sg_expr = isSgExpression(tmp_expr);
    if (tmp_expr != nullptr && sg_expr == nullptr) {
      std::cerr << "Runtime error: " << label
                << " translated to a non-expression Sage node" << std::endl;
      res = false;
    }
    return sg_expr;
  };

  SgExpression *kernel =
      translate_expr(cuda_kernel_call_expr->getCallee(), "CUDA kernel callee");

  clang::CallExpr *config_call = cuda_kernel_call_expr->getConfig();
  if (config_call == nullptr) {
    std::cerr << "Runtime error: CUDAKernelCallExpr missing launch "
                 "configuration"
              << std::endl;
    return false;
  }

  auto translate_config_arg = [&](unsigned index,
                                  const char *label) -> SgExpression * {
    if (index >= config_call->getNumArgs()) {
      return nullptr;
    }
    return translate_expr(config_call->getArg(index), label);
  };

  SgExpression *grid =
      translate_config_arg(0, "CUDA launch configuration grid");
  SgExpression *blocks =
      translate_config_arg(1, "CUDA launch configuration blocks");
  SgExpression *shared =
      translate_config_arg(2, "CUDA launch configuration shared memory");
  SgExpression *stream =
      translate_config_arg(3, "CUDA launch configuration stream");

  if (kernel == nullptr || grid == nullptr || blocks == nullptr) {
    return false;
  }

  SgExprListExp *param_list = SageBuilder::buildExprListExp_nfi();
  applySourceRange(param_list, cuda_kernel_call_expr->getSourceRange());

  for (clang::CallExpr::arg_iterator it = cuda_kernel_call_expr->arg_begin();
       it != cuda_kernel_call_expr->arg_end(); ++it) {
    if (clang::isa<clang::CXXDefaultArgExpr>(*it)) {
      continue;
    }

    SgExpression *arg = translate_expr(*it, "CUDA kernel argument");
    if (arg != nullptr) {
      param_list->append_expression(arg);
    }
  }

  SgCudaKernelExecConfig *exec_config =
      SageBuilder::buildCudaKernelExecConfig_nfi(grid, blocks, shared, stream);
  applySourceRange(exec_config, config_call->getSourceRange());

  SgCudaKernelCallExp *kernel_call =
      SageBuilder::buildCudaKernelCallExp_nfi(kernel, param_list, exec_config);
  applySourceRange(kernel_call, cuda_kernel_call_expr->getSourceRange());
  *node = kernel_call;

  return VisitExpr(cuda_kernel_call_expr, node) && res;
}

bool ClangToSageTranslator::VisitCXXMemberCallExpr(
    clang::CXXMemberCallExpr *cxx_member_call_expr, SgNode **node) {
#if DEBUG_VISIT_STMT
  std::cerr << "ClangToSageTranslator::VisitCXXMemberCallExpr" << std::endl;
#endif
  bool res = true;

  // CXXMemberCallExpr represents calls to member functions (e.g., obj.method()
  // or ptr->method()) Delegate to CallExpr handler which will handle function
  // call expression generation
  return VisitCallExpr(cxx_member_call_expr, node) && res;
}

bool ClangToSageTranslator::VisitCXXOperatorCallExpr(
    clang::CXXOperatorCallExpr *cxx_operator_call_expr, SgNode **node) {
#if DEBUG_VISIT_STMT
  std::cerr << "ClangToSageTranslator::VisitCXXOperatorCallExpr" << std::endl;
#endif
  bool res = true;

  auto callee_has_explicit_template_args = [&]() -> bool {
    clang::Expr *callee = cxx_operator_call_expr->getCallee();
    if (callee == nullptr) {
      return false;
    }

    clang::Expr *base = callee->IgnoreParenImpCasts();
    if (clang::OverloadExpr *overload =
            llvm::dyn_cast_or_null<clang::OverloadExpr>(base)) {
      return overload->hasExplicitTemplateArgs();
    }
    if (clang::DeclRefExpr *decl_ref =
            llvm::dyn_cast_or_null<clang::DeclRefExpr>(base)) {
      return decl_ref->hasExplicitTemplateArgs();
    }
    if (clang::MemberExpr *member_expr =
            llvm::dyn_cast_or_null<clang::MemberExpr>(base)) {
      return member_expr->hasExplicitTemplateArgs();
    }
    return false;
  };

  const bool has_explicit_template_args = callee_has_explicit_template_args();

  if (cxx_operator_call_expr->getOperator() == clang::OO_Equal &&
      cxx_operator_call_expr->getNumArgs() >= 2) {
    SgNode *tmp_lhs = Traverse(cxx_operator_call_expr->getArg(0));
    SgExpression *lhs = isSgExpression(tmp_lhs);
    if (tmp_lhs != nullptr && lhs == nullptr) {
      MLOG_ERROR_C(MLOG_FRONTEND,
                   "Runtime error: tmp_lhs != nullptr && lhs == nullptr\n");
      res = false;
    }

    SgNode *tmp_rhs = Traverse(cxx_operator_call_expr->getArg(1));
    SgExpression *rhs = isSgExpression(tmp_rhs);
    if (tmp_rhs != nullptr && rhs == nullptr) {
      MLOG_ERROR_C(MLOG_FRONTEND,
                   "Runtime error: tmp_rhs != nullptr && rhs == nullptr\n");
      res = false;
    }

    if (lhs != nullptr && rhs != nullptr) {
      SgType *lhs_type = lhs->get_type();
      SgType *op_type =
          buildTypeFromQualifiedType(cxx_operator_call_expr->getType());
      if (lhs_type != nullptr && op_type != nullptr && lhs_type == op_type) {
        *node = SageBuilder::buildAssignOp(lhs, rhs);
        return VisitExpr(cxx_operator_call_expr, node) && res;
      }
    }
  }

  auto traverse_operator_arg = [&](unsigned index) -> SgExpression * {
    if (index >= cxx_operator_call_expr->getNumArgs()) {
      return nullptr;
    }

    SgNode *tmp_arg = Traverse(cxx_operator_call_expr->getArg(index));
    SgExpression *arg = isSgExpression(tmp_arg);
    if (tmp_arg != nullptr && arg == nullptr) {
      std::cerr << "Runtime error: tmp_arg != nullptr && arg == nullptr"
                << std::endl;
      res = false;
    }
    return arg;
  };

  auto build_syntax_operator_expr = [&]() -> SgExpression * {
    const unsigned num_args = cxx_operator_call_expr->getNumArgs();
    const clang::OverloadedOperatorKind op =
        cxx_operator_call_expr->getOperator();

    auto unary_arg = [&]() -> SgExpression * {
      return traverse_operator_arg(0);
    };
    auto binary_lhs = [&]() -> SgExpression * {
      return traverse_operator_arg(0);
    };
    auto binary_rhs = [&]() -> SgExpression * {
      return traverse_operator_arg(1);
    };

    switch (op) {
    case clang::OO_Plus:
      if (num_args == 1) {
        return SageBuilder::buildUnaryAddOp(unary_arg());
      }
      if (num_args == 2) {
        return SageBuilder::buildAddOp(binary_lhs(), binary_rhs());
      }
      break;

    case clang::OO_Minus:
      if (num_args == 1) {
        return SageBuilder::buildMinusOp(unary_arg());
      }
      if (num_args == 2) {
        return SageBuilder::buildSubtractOp(binary_lhs(), binary_rhs());
      }
      break;

    case clang::OO_Star:
      if (num_args == 1) {
        return SageBuilder::buildPointerDerefExp(unary_arg());
      }
      if (num_args == 2) {
        return SageBuilder::buildMultiplyOp(binary_lhs(), binary_rhs());
      }
      break;

    case clang::OO_Amp:
      if (num_args == 1) {
        return SageBuilder::buildAddressOfOp(unary_arg());
      }
      if (num_args == 2) {
        return SageBuilder::buildBitAndOp(binary_lhs(), binary_rhs());
      }
      break;

    case clang::OO_Tilde:
      if (num_args == 1) {
        return SageBuilder::buildBitComplementOp(unary_arg());
      }
      break;

    case clang::OO_Exclaim:
      if (num_args == 1) {
        return SageBuilder::buildNotOp(unary_arg());
      }
      break;

    case clang::OO_Slash:
      if (num_args == 2) {
        return SageBuilder::buildDivideOp(binary_lhs(), binary_rhs());
      }
      break;

    case clang::OO_Percent:
      if (num_args == 2) {
        return SageBuilder::buildModOp(binary_lhs(), binary_rhs());
      }
      break;

    case clang::OO_Caret:
      if (num_args == 2) {
        return SageBuilder::buildBitXorOp(binary_lhs(), binary_rhs());
      }
      break;

    case clang::OO_Pipe:
      if (num_args == 2) {
        return SageBuilder::buildBitOrOp(binary_lhs(), binary_rhs());
      }
      break;

    case clang::OO_LessLess:
      if (num_args == 2) {
        return SageBuilder::buildLshiftOp(binary_lhs(), binary_rhs());
      }
      break;

    case clang::OO_GreaterGreater:
      if (num_args == 2) {
        return SageBuilder::buildRshiftOp(binary_lhs(), binary_rhs());
      }
      break;

    case clang::OO_Less:
      if (num_args == 2) {
        return SageBuilder::buildLessThanOp(binary_lhs(), binary_rhs());
      }
      break;

    case clang::OO_Greater:
      if (num_args == 2) {
        return SageBuilder::buildGreaterThanOp(binary_lhs(), binary_rhs());
      }
      break;

    case clang::OO_LessEqual:
      if (num_args == 2) {
        return SageBuilder::buildLessOrEqualOp(binary_lhs(), binary_rhs());
      }
      break;

    case clang::OO_GreaterEqual:
      if (num_args == 2) {
        return SageBuilder::buildGreaterOrEqualOp(binary_lhs(), binary_rhs());
      }
      break;

    case clang::OO_EqualEqual:
      if (num_args == 2) {
        return SageBuilder::buildEqualityOp(binary_lhs(), binary_rhs());
      }
      break;

    case clang::OO_ExclaimEqual:
      if (num_args == 2) {
        return SageBuilder::buildNotEqualOp(binary_lhs(), binary_rhs());
      }
      break;

    case clang::OO_AmpAmp:
      if (num_args == 2) {
        return SageBuilder::buildAndOp(binary_lhs(), binary_rhs());
      }
      break;

    case clang::OO_PipePipe:
      if (num_args == 2) {
        return SageBuilder::buildOrOp(binary_lhs(), binary_rhs());
      }
      break;

    case clang::OO_Comma:
      if (num_args == 2) {
        return SageBuilder::buildCommaOpExp(binary_lhs(), binary_rhs());
      }
      break;

    case clang::OO_Equal:
      if (num_args == 2) {
        return SageBuilder::buildAssignOp(binary_lhs(), binary_rhs());
      }
      break;

    case clang::OO_PlusEqual:
      if (num_args == 2) {
        return SageBuilder::buildPlusAssignOp(binary_lhs(), binary_rhs());
      }
      break;

    case clang::OO_MinusEqual:
      if (num_args == 2) {
        return SageBuilder::buildMinusAssignOp(binary_lhs(), binary_rhs());
      }
      break;

    case clang::OO_StarEqual:
      if (num_args == 2) {
        return SageBuilder::buildMultAssignOp(binary_lhs(), binary_rhs());
      }
      break;

    case clang::OO_SlashEqual:
      if (num_args == 2) {
        return SageBuilder::buildDivAssignOp(binary_lhs(), binary_rhs());
      }
      break;

    case clang::OO_PercentEqual:
      if (num_args == 2) {
        return SageBuilder::buildModAssignOp(binary_lhs(), binary_rhs());
      }
      break;

    case clang::OO_AmpEqual:
      if (num_args == 2) {
        return SageBuilder::buildAndAssignOp(binary_lhs(), binary_rhs());
      }
      break;

    case clang::OO_PipeEqual:
      if (num_args == 2) {
        return SageBuilder::buildIorAssignOp(binary_lhs(), binary_rhs());
      }
      break;

    case clang::OO_CaretEqual:
      if (num_args == 2) {
        return SageBuilder::buildXorAssignOp(binary_lhs(), binary_rhs());
      }
      break;

    case clang::OO_LessLessEqual:
      if (num_args == 2) {
        return SageBuilder::buildLshiftAssignOp(binary_lhs(), binary_rhs());
      }
      break;

    case clang::OO_GreaterGreaterEqual:
      if (num_args == 2) {
        return SageBuilder::buildRshiftAssignOp(binary_lhs(), binary_rhs());
      }
      break;

    case clang::OO_PlusPlus:
      if (num_args == 1) {
        return SageBuilder::buildPlusPlusOp(unary_arg(), SgUnaryOp::prefix);
      }
      if (num_args == 2) {
        return SageBuilder::buildPlusPlusOp(binary_lhs(), SgUnaryOp::postfix);
      }
      break;

    case clang::OO_MinusMinus:
      if (num_args == 1) {
        return SageBuilder::buildMinusMinusOp(unary_arg(), SgUnaryOp::prefix);
      }
      if (num_args == 2) {
        return SageBuilder::buildMinusMinusOp(binary_lhs(), SgUnaryOp::postfix);
      }
      break;

    default:
      break;
    }

    return nullptr;
  };

  if (!has_explicit_template_args &&
      cxx_operator_call_expr->getDirectCallee() == nullptr) {
    clang::Expr *callee = cxx_operator_call_expr->getCallee();
    clang::Expr *base_callee =
        callee != nullptr ? callee->IgnoreParenImpCasts() : nullptr;
    if (llvm::isa<clang::OverloadExpr>(base_callee)) {
      if (SgExpression *syntax_expr = build_syntax_operator_expr()) {
        *node = syntax_expr;
        return VisitExpr(cxx_operator_call_expr, node) && res;
      }
    }
  }

  // C++ overloaded operators (operator+, operator[], etc.) are represented as
  // function calls. For member operators, Clang models the implicit object as
  // the first argument, while ROSE represents it as a dot/arrow expression in
  // the call's callee. Convert member-operator calls into the ROSE form to
  // preserve correct operator arity and unparsing.
  if (clang::FunctionDecl *direct_callee =
          cxx_operator_call_expr->getDirectCallee()) {
    if (llvm::isa<clang::CXXMethodDecl>(direct_callee) &&
        cxx_operator_call_expr->getNumArgs() > 0) {
      // operator[] is special: ROSE's unparser expects the implicit object to
      // remain as the first argument so it can emit `obj[idx]`. Do not convert
      // the implicit object into a dot/arrow callee for this operator.
      if (cxx_operator_call_expr->getOperator() == clang::OO_Subscript) {
        // Prefer the canonical array-ref AST for operator[] so the unparser
        // emits `obj[idx]` reliably (ROSE expects this shape).
        if (cxx_operator_call_expr->getNumArgs() == 2) {
          SgNode *tmp_base = Traverse(cxx_operator_call_expr->getArg(0));
          SgExpression *base = isSgExpression(tmp_base);
          if (tmp_base != nullptr && base == nullptr) {
            std::cerr << "Runtime error: tmp_base != nullptr && base == nullptr"
                      << std::endl;
            res = false;
          }

          SgNode *tmp_idx = Traverse(cxx_operator_call_expr->getArg(1));
          SgExpression *idx = isSgExpression(tmp_idx);
          if (tmp_idx != nullptr && idx == nullptr) {
            std::cerr << "Runtime error: tmp_idx != nullptr && idx == nullptr"
                      << std::endl;
            res = false;
          }

          if (base != nullptr && idx != nullptr) {
            SgType *result_type =
                buildTypeFromQualifiedType(cxx_operator_call_expr->getType());
            SgPntrArrRefExp *arr_ref =
                new SgPntrArrRefExp(base, idx, result_type);
            if (base != nullptr) {
              base->set_parent(arr_ref);
              markLhsValues(arr_ref);
            }
            if (idx != nullptr) {
              idx->set_parent(arr_ref);
            }
            SageInterface::setSourcePosition(arr_ref);
            arr_ref->set_need_paren(false);
            *node = arr_ref;
            return VisitExpr(cxx_operator_call_expr, node) && res;
          }
        }

        SgNode *tmp_decl = nullptr;
        auto it_decl = p_decl_translation_map.find(direct_callee);
        if (it_decl != p_decl_translation_map.end()) {
          tmp_decl = it_decl->second;
        } else {
          tmp_decl = Traverse(direct_callee);
        }

        SgMemberFunctionDeclaration *member_decl =
            isSgMemberFunctionDeclaration(tmp_decl);
        SgMemberFunctionSymbol *member_sym = nullptr;
        if (member_decl != nullptr) {
          member_sym = isSgMemberFunctionSymbol(
              member_decl->get_symbol_from_symbol_table());
          if (member_sym == nullptr) {
            if (SgScopeStatement *decl_scope = member_decl->get_scope()) {
              member_sym =
                  isSgMemberFunctionSymbol(decl_scope->lookup_function_symbol(
                      member_decl->get_name(), member_decl->get_type()));
            }
          }
          if (member_sym == nullptr) {
            member_sym = new SgMemberFunctionSymbol(member_decl);
            if (SgScopeStatement *decl_scope = member_decl->get_scope()) {
              attachSymbolToScopeOrOrphan(member_sym, decl_scope);
            } else {
              attachSymbolToScopeOrOrphan(member_sym, nullptr);
            }
          }
        }

        if (member_sym != nullptr) {
          SgExprListExp *param_list = SageBuilder::buildExprListExp_nfi();
          applySourceRange(param_list,
                           cxx_operator_call_expr->getSourceRange());

          for (unsigned i = 0; i < cxx_operator_call_expr->getNumArgs(); ++i) {
            if (clang::isa<clang::CXXDefaultArgExpr>(
                    cxx_operator_call_expr->getArg(i))) {
              continue;
            }
            SgNode *tmp_arg = Traverse(cxx_operator_call_expr->getArg(i));
            SgExpression *arg = isSgExpression(tmp_arg);
            if (tmp_arg != nullptr && arg == nullptr) {
              std::cerr << "Runtime error: tmp_arg != nullptr && arg == nullptr"
                        << std::endl;
              res = false;
              continue;
            }
            param_list->append_expression(arg);
          }

          SgExpression *callee = SageBuilder::buildFunctionRefExp(member_sym);
          SgFunctionCallExp *call =
              SageBuilder::buildFunctionCallExp_nfi(callee, param_list);
          call->set_uses_operator_syntax(true);
          *node = call;
          return VisitExpr(cxx_operator_call_expr, node) && res;
        }
      }

      SgNode *tmp_base = Traverse(cxx_operator_call_expr->getArg(0));
      SgExpression *base = isSgExpression(tmp_base);
      if (tmp_base != nullptr && base == nullptr) {
        std::cerr << "Runtime error: tmp_base != nullptr && base == nullptr"
                  << std::endl;
        res = false;
      }

      SgNode *tmp_decl = nullptr;
      auto it_decl = p_decl_translation_map.find(direct_callee);
      if (it_decl != p_decl_translation_map.end()) {
        tmp_decl = it_decl->second;
      } else {
        tmp_decl = Traverse(direct_callee);
      }

      SgMemberFunctionDeclaration *member_decl =
          isSgMemberFunctionDeclaration(tmp_decl);
      SgMemberFunctionSymbol *member_sym = nullptr;
      if (member_decl != nullptr) {
        member_sym = isSgMemberFunctionSymbol(
            member_decl->get_symbol_from_symbol_table());
        if (member_sym == nullptr) {
          if (SgScopeStatement *decl_scope = member_decl->get_scope()) {
            member_sym =
                isSgMemberFunctionSymbol(decl_scope->lookup_function_symbol(
                    member_decl->get_name(), member_decl->get_type()));
          }
        }
        if (member_sym == nullptr) {
          member_sym = new SgMemberFunctionSymbol(member_decl);
          if (SgScopeStatement *decl_scope = member_decl->get_scope()) {
            attachSymbolToScopeOrOrphan(member_sym, decl_scope);
          } else {
            attachSymbolToScopeOrOrphan(member_sym, nullptr);
          }
        }
      }

      if (base != nullptr && member_sym != nullptr) {
        SgExpression *member_ref = SageBuilder::buildMemberFunctionRefExp_nfi(
            member_sym, false, false);

        SgExpression *callee = nullptr;
        if (isSgPointerType(base->get_type())) {
          callee = SageBuilder::buildArrowExp(base, member_ref);
        } else {
          callee = SageBuilder::buildDotExp(base, member_ref);
        }

        SgExprListExp *param_list = SageBuilder::buildExprListExp_nfi();
        applySourceRange(param_list, cxx_operator_call_expr->getSourceRange());

        for (unsigned i = 1; i < cxx_operator_call_expr->getNumArgs(); ++i) {
          if (clang::isa<clang::CXXDefaultArgExpr>(
                  cxx_operator_call_expr->getArg(i))) {
            continue;
          }
          SgNode *tmp_arg = Traverse(cxx_operator_call_expr->getArg(i));
          SgExpression *arg = isSgExpression(tmp_arg);
          if (tmp_arg != nullptr && arg == nullptr) {
            std::cerr << "Runtime error: tmp_arg != nullptr && arg == nullptr"
                      << std::endl;
            res = false;
            continue;
          }
          param_list->append_expression(arg);
        }

        SgFunctionCallExp *call =
            SageBuilder::buildFunctionCallExp_nfi(callee, param_list);
        // Use explicit call syntax for member operators once the implicit
        // object is moved into the callee (dot/arrow form).
        call->set_uses_operator_syntax(false);
        *node = call;

        return VisitExpr(cxx_operator_call_expr, node) && res;
      }
    }
  }

  // Non-member overloaded operators are modeled as regular calls where all
  // operands appear as explicit arguments.
  res = VisitCallExpr(cxx_operator_call_expr, node) && res;

  // CLANG FRONTEND FIX: Set uses_operator_syntax flag to true for operator
  // overloads This tells the unparser to generate operator syntax (e.g., a + b)
  // instead of explicit function call syntax (e.g., operator+(a, b))
  if (*node != nullptr && res) {
    SgFunctionCallExp *funcCall = isSgFunctionCallExp(*node);
    if (funcCall != nullptr) {
      SgExpression *callee = funcCall->get_function();
      bool is_member_callee = (isSgMemberAccessExp(callee) != nullptr);
      std::function<bool(SgExpression *)> callee_has_template_args =
          [&](SgExpression *expr) -> bool {
        if (expr == nullptr) {
          return false;
        }
        if (SgDotExp *dot = isSgDotExp(expr)) {
          return callee_has_template_args(dot->get_rhs_operand());
        }
        if (SgArrowExp *arrow = isSgArrowExp(expr)) {
          return callee_has_template_args(arrow->get_rhs_operand());
        }
        if (SgNonrealRefExp *nr_ref = isSgNonrealRefExp(expr)) {
          if (!nr_ref->get_templateArguments().empty()) {
            return true;
          }
          if (SgNonrealSymbol *nr_sym = nr_ref->get_symbol()) {
            if (SgNonrealDecl *nr_decl = nr_sym->get_declaration()) {
              if (!nr_decl->get_tpl_args().empty()) {
                return true;
              }
            }
          }
          return false;
        }

        auto instantiation_has_args = [](SgFunctionDeclaration *decl) -> bool {
          if (decl == nullptr) {
            return false;
          }
          if (SgTemplateInstantiationFunctionDecl *inst =
                  isSgTemplateInstantiationFunctionDecl(decl)) {
            return !inst->get_templateArguments().empty();
          }
          if (SgTemplateInstantiationMemberFunctionDecl *inst =
                  isSgTemplateInstantiationMemberFunctionDecl(decl)) {
            return !inst->get_templateArguments().empty();
          }
          return false;
        };

        if (SgFunctionRefExp *func_ref = isSgFunctionRefExp(expr)) {
          return instantiation_has_args(
              func_ref->get_symbol() ? func_ref->get_symbol()->get_declaration()
                                     : nullptr);
        }
        if (SgTemplateFunctionRefExp *tmpl_ref =
                isSgTemplateFunctionRefExp(expr)) {
          return instantiation_has_args(
              tmpl_ref->get_symbol() ? tmpl_ref->get_symbol()->get_declaration()
                                     : nullptr);
        }
        if (SgMemberFunctionRefExp *mfunc_ref =
                isSgMemberFunctionRefExp(expr)) {
          return instantiation_has_args(
              mfunc_ref->get_symbol()
                  ? mfunc_ref->get_symbol()->get_declaration()
                  : nullptr);
        }
        if (SgTemplateMemberFunctionRefExp *tmpl_mfunc_ref =
                isSgTemplateMemberFunctionRefExp(expr)) {
          return instantiation_has_args(
              tmpl_mfunc_ref->get_symbol()
                  ? tmpl_mfunc_ref->get_symbol()->get_declaration()
                  : nullptr);
        }

        return false;
      };

      bool use_operator_syntax = !is_member_callee;
      if (use_operator_syntax &&
          (has_explicit_template_args || callee_has_template_args(callee)) &&
          cxx_operator_call_expr->getOperator() != clang::OO_Spaceship) {
        use_operator_syntax = false;
      }
      funcCall->set_uses_operator_syntax(use_operator_syntax);
    }
  }

  return res;
}

bool ClangToSageTranslator::VisitUserDefinedLiteral(
    clang::UserDefinedLiteral *user_defined_literal, SgNode **node) {
#if DEBUG_VISIT_STMT
  std::cerr << "ClangToSageTranslator::VisitUserDefinedLiteral" << std::endl;
#endif
  bool res = true;

  // User-defined literals first go through regular CallExpr
  // translation, then raw/template literal operands are normalized here so
  // ROSE preserves correct operand spelling and literal-operator metadata.
  res = VisitCallExpr(user_defined_literal, node) && res;

  SgFunctionCallExp *call = isSgFunctionCallExp(*node);
  if (call == nullptr) {
    return res;
  }

  if (SgTemplateFunctionRefExp *template_ref =
          isSgTemplateFunctionRefExp(call->get_function())) {
    if (template_ref->get_symbol() != nullptr) {
      if (SgFunctionDeclaration *decl =
              template_ref->get_symbol()->get_declaration()) {
        SgFunctionRefExp *function_ref = SageBuilder::buildFunctionRefExp(decl);
        call->set_function(function_ref);
        function_ref->set_parent(call);
      }
    }
  }

  auto mark_literal_operator_decl = [&](SgExpression *callee,
                                        auto &&self) -> void {
    if (callee == nullptr) {
      return;
    }

    auto mark_decl = [](SgFunctionDeclaration *decl) {
      if (decl == nullptr) {
        return;
      }
      decl->get_specialFunctionModifier().setUldOperator();
      if (SgFunctionDeclaration *first_nondef = isSgFunctionDeclaration(
              decl->get_firstNondefiningDeclaration())) {
        first_nondef->get_specialFunctionModifier().setUldOperator();
      }
      if (SgTemplateInstantiationFunctionDecl *inst =
              isSgTemplateInstantiationFunctionDecl(decl)) {
        inst->set_template_argument_list_is_explicit(false);
        inst->get_templateArguments().clear();
      } else if (SgTemplateInstantiationMemberFunctionDecl *inst =
                     isSgTemplateInstantiationMemberFunctionDecl(decl)) {
        inst->set_template_argument_list_is_explicit(false);
        inst->get_templateArguments().clear();
      }
    };

    if (SgFunctionRefExp *func_ref = isSgFunctionRefExp(callee)) {
      mark_decl(func_ref->get_symbol()
                    ? func_ref->get_symbol()->get_declaration()
                    : nullptr);
      return;
    }
    if (SgMemberFunctionRefExp *mfunc_ref = isSgMemberFunctionRefExp(callee)) {
      mark_decl(mfunc_ref->get_symbol()
                    ? mfunc_ref->get_symbol()->get_declaration()
                    : nullptr);
      return;
    }
    if (SgTemplateFunctionRefExp *template_ref =
            isSgTemplateFunctionRefExp(callee)) {
      mark_decl(template_ref->get_symbol()
                    ? template_ref->get_symbol()->get_declaration()
                    : nullptr);
      return;
    }
    if (SgTemplateMemberFunctionRefExp *template_mref =
            isSgTemplateMemberFunctionRefExp(callee)) {
      mark_decl(template_mref->get_symbol()
                    ? template_mref->get_symbol()->get_declaration()
                    : nullptr);
      return;
    }
    if (SgDotExp *dot_exp = isSgDotExp(callee)) {
      self(dot_exp->get_rhs_operand(), self);
      return;
    }
    if (SgArrowExp *arrow_exp = isSgArrowExp(callee)) {
      self(arrow_exp->get_rhs_operand(), self);
      return;
    }
  };
  mark_literal_operator_decl(call->get_function(), mark_literal_operator_decl);

  std::string literal_spelling =
      getSourceText(user_defined_literal->getSourceRange());
  std::string suffix_spelling;
  if (const clang::IdentifierInfo *suffix =
          user_defined_literal->getUDSuffix()) {
    suffix_spelling = suffix->getName().str();
  }
  std::string literal_body = literal_spelling;
  if (!suffix_spelling.empty() &&
      literal_body.size() >= suffix_spelling.size() &&
      literal_body.compare(literal_body.size() - suffix_spelling.size(),
                           suffix_spelling.size(), suffix_spelling) == 0) {
    literal_body.resize(literal_body.size() - suffix_spelling.size());
  }

  auto starts_with = [](const std::string &text, const char *prefix) -> bool {
    return text.rfind(prefix, 0) == 0;
  };

  auto build_literal_operand = [&]() -> SgExpression * {
    auto build_string_literal =
        [&](const std::string &spelling) -> SgExpression * {
      SgStringVal *string_val = SageBuilder::buildStringVal("");
      if (starts_with(spelling, "LR\"") || starts_with(spelling, "L\"")) {
        string_val->set_wcharString(true);
      } else if (starts_with(spelling, "uR\"") ||
                 starts_with(spelling, "u\"")) {
        string_val->set_is16bitString(true);
      } else if (starts_with(spelling, "UR\"") ||
                 starts_with(spelling, "U\"")) {
        string_val->set_is32bitString(true);
      }
      string_val->set_value(
          extractStringLiteralValue(spelling, p_compiler_instance));
      return string_val;
    };

    auto build_char_literal =
        [&](const std::string &spelling) -> SgExpression * {
      if (starts_with(spelling, "L'")) {
        return SageBuilder::buildWcharVal_nfi(0, spelling);
      }
      if (starts_with(spelling, "u'")) {
        return SageBuilder::buildChar16Val_nfi(0, spelling);
      }
      if (starts_with(spelling, "U'")) {
        return SageBuilder::buildChar32Val_nfi(0, spelling);
      }
      return SageBuilder::buildCharVal_nfi(0, spelling);
    };
    auto is_string_literal_spelling = [&](const std::string &spelling) {
      return starts_with(spelling, "u8R\"") || starts_with(spelling, "uR\"") ||
             starts_with(spelling, "UR\"") || starts_with(spelling, "LR\"") ||
             starts_with(spelling, "R\"") || starts_with(spelling, "u8\"") ||
             starts_with(spelling, "u\"") || starts_with(spelling, "U\"") ||
             starts_with(spelling, "L\"") || starts_with(spelling, "\"");
    };

    auto is_character_literal_spelling = [&](const std::string &spelling) {
      return starts_with(spelling, "u'") || starts_with(spelling, "U'") ||
             starts_with(spelling, "L'") || starts_with(spelling, "'");
    };

    clang::UserDefinedLiteral::LiteralOperatorKind kind =
        user_defined_literal->getLiteralOperatorKind();
    switch (kind) {
    case clang::UserDefinedLiteral::LOK_String:
      return build_string_literal(literal_body);
    case clang::UserDefinedLiteral::LOK_Character:
      return build_char_literal(literal_body);
    case clang::UserDefinedLiteral::LOK_Floating:
      return SageBuilder::buildLongDoubleVal_nfi(0.0L, literal_body);
    case clang::UserDefinedLiteral::LOK_Integer:
      return SageBuilder::buildIntVal_nfi(0, literal_body);
    case clang::UserDefinedLiteral::LOK_Raw:
    case clang::UserDefinedLiteral::LOK_Template:
      if (is_string_literal_spelling(literal_body)) {
        return build_string_literal(literal_body);
      }
      if (is_character_literal_spelling(literal_body)) {
        return build_char_literal(literal_body);
      }
      if (isFloatingLiteralForUdlSpelling(literal_body, p_compiler_instance)) {
        return SageBuilder::buildLongDoubleVal_nfi(0.0L, literal_body);
      }
      return SageBuilder::buildIntVal_nfi(0, literal_body);
    }

    ROSE_ABORT();
  };

  auto update_literal_value_string = [&](SgExpression *expr,
                                         const std::string &spelling) {
    if (auto *int_val = isSgIntVal(expr)) {
      int_val->set_valueString(spelling);
    } else if (auto *ll_val = isSgLongLongIntVal(expr)) {
      ll_val->set_valueString(spelling);
    } else if (auto *ull_val = isSgUnsignedLongLongIntVal(expr)) {
      ull_val->set_valueString(spelling);
    } else if (auto *long_val = isSgLongIntVal(expr)) {
      long_val->set_valueString(spelling);
    } else if (auto *ulong_val = isSgUnsignedLongVal(expr)) {
      ulong_val->set_valueString(spelling);
    } else if (auto *uint_val = isSgUnsignedIntVal(expr)) {
      uint_val->set_valueString(spelling);
    } else if (auto *float_val = isSgFloatVal(expr)) {
      float_val->set_valueString(spelling);
    } else if (auto *double_val = isSgDoubleVal(expr)) {
      double_val->set_valueString(spelling);
    } else if (auto *long_double_val = isSgLongDoubleVal(expr)) {
      long_double_val->set_valueString(spelling);
    } else if (auto *char_val = isSgCharVal(expr)) {
      char_val->set_valueString(spelling);
    } else if (auto *wchar_val = isSgWcharVal(expr)) {
      wchar_val->set_valueString(spelling);
    } else if (auto *char16_val = isSgChar16Val(expr)) {
      char16_val->set_valueString(spelling);
    } else if (auto *char32_val = isSgChar32Val(expr)) {
      char32_val->set_valueString(spelling);
    }
  };

  clang::UserDefinedLiteral::LiteralOperatorKind literal_kind =
      user_defined_literal->getLiteralOperatorKind();
  std::string operand_spelling = literal_body;

  if (literal_kind != clang::UserDefinedLiteral::LOK_Raw &&
      literal_kind != clang::UserDefinedLiteral::LOK_Template &&
      user_defined_literal->getCookedLiteral() != nullptr) {
    SgExprListExp *param_list = call->get_args();
    if (param_list != nullptr && !param_list->get_expressions().empty()) {
      SgExpression *operand =
          isSgExpression(Traverse(user_defined_literal->getCookedLiteral()));
      if (operand != nullptr) {
        update_literal_value_string(operand, operand_spelling);
        setCompilerGeneratedFileInfo(operand, true);
        operand->set_parent(param_list);
        param_list->get_expressions().front() = operand;
      } else {
        res = false;
      }
    } else {
      res = false;
    }
  } else {
    SgExpression *operand = build_literal_operand();
    if (operand != nullptr) {
      setCompilerGeneratedFileInfo(operand, true);
    }

    SgExprListExp *param_list = SageBuilder::buildExprListExp_nfi();
    applySourceRange(param_list, user_defined_literal->getSourceRange());
    if (operand != nullptr) {
      param_list->append_expression(operand);
    } else {
      res = false;
    }
    call->set_args(param_list);
    param_list->set_parent(call);
  }

  call->set_uses_operator_syntax(true);

  return res;
}

bool ClangToSageTranslator::VisitCastExpr(clang::CastExpr *cast_expr,
                                          SgNode **node) {
#if DEBUG_VISIT_STMT
  std::cerr << "ClangToSageTranslator::VisitCastExpr" << std::endl;
#endif
  bool res = true;

  // Process the sub-expression being cast
  SgNode *tmp_expr = Traverse(cast_expr->getSubExpr());
  SgExpression *sg_expr = isSgExpression(tmp_expr);
  ROSE_ASSERT(sg_expr != nullptr);

  // Get the target type
  SgType *sg_type = buildTypeFromQualifiedType(cast_expr->getType());

  // Create the cast expression
  *node = SageBuilder::buildCastExp(sg_expr, sg_type);

  return VisitExpr(cast_expr, node) && res;
}

bool ClangToSageTranslator::VisitExplicitCastExpr(
    clang::ExplicitCastExpr *explicit_cast_expr, SgNode **node) {
#if DEBUG_VISIT_STMT
  std::cerr << "ClangToSageTranslator::VisitExplicitCastExpr" << std::endl;
#endif
  bool res = true;

  if (auto *functional =
          llvm::dyn_cast<clang::CXXFunctionalCastExpr>(explicit_cast_expr)) {
    return VisitCXXFunctionalCastExpr(functional, node);
  }

  SgNode *tmp_expr = Traverse(explicit_cast_expr->getSubExpr());
  SgExpression *sg_expr = isSgExpression(tmp_expr);
  ROSE_ASSERT(sg_expr != nullptr);

  auto suppress_nonautonomous_named_type_declaration = [](SgType *rose_type) {
    if (rose_type == nullptr) {
      return;
    }

    unsigned char strip_options =
        SgType::STRIP_MODIFIER_TYPE | SgType::STRIP_REFERENCE_TYPE |
        SgType::STRIP_RVALUE_REFERENCE_TYPE | SgType::STRIP_POINTER_TYPE |
        SgType::STRIP_ARRAY_TYPE;
    SgType *stripped_type = rose_type->stripType(strip_options);

    auto suppress_class_decl = [](SgClassDeclaration *decl) {
      if (decl == nullptr) {
        return;
      }
      decl->set_isAutonomousDeclaration(false);
      suppress_unparse_output(decl);
    };
    auto suppress_enum_decl = [](SgEnumDeclaration *decl) {
      if (decl == nullptr) {
        return;
      }
      decl->set_isAutonomousDeclaration(false);
      suppress_unparse_output(decl);
    };

    if (SgClassType *class_type = isSgClassType(stripped_type)) {
      class_type->set_autonomous_declaration(false);
      if (SgClassDeclaration *class_decl =
              isSgClassDeclaration(class_type->get_declaration())) {
        suppress_class_decl(class_decl);
        suppress_class_decl(isSgClassDeclaration(
            class_decl->get_firstNondefiningDeclaration()));
        suppress_class_decl(
            isSgClassDeclaration(class_decl->get_definingDeclaration()));
      }
      return;
    }

    if (SgEnumType *enum_type = isSgEnumType(stripped_type)) {
      enum_type->set_autonomous_declaration(false);
      if (SgEnumDeclaration *enum_decl =
              isSgEnumDeclaration(enum_type->get_declaration())) {
        suppress_enum_decl(enum_decl);
        suppress_enum_decl(
            isSgEnumDeclaration(enum_decl->get_firstNondefiningDeclaration()));
        suppress_enum_decl(
            isSgEnumDeclaration(enum_decl->get_definingDeclaration()));
      }
    }
  };

  bool cast_requires_elaboration =
      qual_type_contains_elaborated_spelling_for_type_operand(
          explicit_cast_expr->getTypeAsWritten());

  SgType *sg_type = nullptr;
  if (clang::TypeSourceInfo *type_info =
          explicit_cast_expr->getTypeInfoAsWritten()) {
    sg_type = buildTypeFromTypeLoc(type_info->getTypeLoc());
  }
  if (sg_type == nullptr) {
    sg_type =
        buildTypeFromQualifiedType(explicit_cast_expr->getTypeAsWritten());
  }

  SgCastExp::cast_type_enum cast_kind = SgCastExp::e_C_style_cast;
  if (llvm::isa<clang::CXXStaticCastExpr>(explicit_cast_expr)) {
    cast_kind = SgCastExp::e_static_cast;
  } else if (llvm::isa<clang::CXXConstCastExpr>(explicit_cast_expr)) {
    cast_kind = SgCastExp::e_const_cast;
  } else if (llvm::isa<clang::CXXDynamicCastExpr>(explicit_cast_expr)) {
    cast_kind = SgCastExp::e_dynamic_cast;
  } else if (llvm::isa<clang::CXXReinterpretCastExpr>(explicit_cast_expr)) {
    cast_kind = SgCastExp::e_reinterpret_cast;
  }

  SgCastExp *cast_exp = SageBuilder::buildCastExp(sg_expr, sg_type, cast_kind);
  if (cast_requires_elaboration) {
    cast_exp->set_type_elaboration_required(true);
    suppress_nonautonomous_named_type_declaration(sg_type);
  }
  *node = cast_exp;

  return VisitExpr(explicit_cast_expr, node) && res;
}

bool ClangToSageTranslator::VisitBuiltinBitCastExpr(
    clang::BuiltinBitCastExpr *builtin_bit_cast_expr, SgNode **node) {
#if DEBUG_VISIT_STMT
  std::cerr << "ClangToSageTranslator::VisitBuiltinBitCastExpr" << std::endl;
#endif
  bool res = true;

  // TODO

  return VisitExplicitCastExpr(builtin_bit_cast_expr, node) && res;
}

bool ClangToSageTranslator::VisitCStyleCastExpr(
    clang::CStyleCastExpr *c_style_cast, SgNode **node) {
#if DEBUG_VISIT_STMT
  std::cerr << "ClangToSageTranslator::VisitCStyleCastExpr" << std::endl;
#endif

  return VisitExplicitCastExpr(c_style_cast, node);
}

bool ClangToSageTranslator::VisitCXXFunctionalCastExpr(
    clang::CXXFunctionalCastExpr *cxx_functional_cast_expr, SgNode **node) {
#if DEBUG_VISIT_STMT
  std::cerr << "ClangToSageTranslator::VisitCXXFunctionalCastExpr" << std::endl;
#endif
  bool res = true;

  SgExprListExp *args = SageBuilder::buildExprListExp_nfi();
  if (clang::Expr *sub_expr = cxx_functional_cast_expr->getSubExpr()) {
    SgNode *tmp_expr = Traverse(sub_expr);
    if (SgExprListExp *expr_list = isSgExprListExp(tmp_expr)) {
      args = expr_list;
    } else if (SgExpression *expr = isSgExpression(tmp_expr)) {
      args->append_expression(expr);
    }
  }

  auto has_explicit_qualifier = [](clang::TypeLoc type_loc) -> bool {
    while (!type_loc.isNull()) {
      if (qualifiedTypeHasQualifier(type_loc.getTypePtr())) {
        return true;
      }
      if (clang::ParenTypeLoc paren_loc =
              type_loc.getAs<clang::ParenTypeLoc>()) {
        type_loc = paren_loc.getInnerLoc();
        continue;
      }
      if (clang::AttributedTypeLoc attributed_loc =
              type_loc.getAs<clang::AttributedTypeLoc>()) {
        type_loc = attributed_loc.getModifiedLoc();
        continue;
      }
      if (clang::AdjustedTypeLoc adjusted_loc =
              type_loc.getAs<clang::AdjustedTypeLoc>()) {
        type_loc = adjusted_loc.getOriginalLoc();
        continue;
      }
      break;
    }
    return false;
  };

  auto scope_contains_declaration = [](SgScopeStatement *scope,
                                       SgDeclarationStatement *decl) -> bool {
    if (scope == nullptr || decl == nullptr) {
      return false;
    }

    for (SgNode *cursor = scope; cursor != nullptr;
         cursor = cursor->get_parent()) {
      if (cursor == decl) {
        return true;
      }

      if (SgClassDeclaration *class_decl = isSgClassDeclaration(decl)) {
        if (SgClassDefinition *class_def = class_decl->get_definition()) {
          if (cursor == class_def) {
            return true;
          }
        }
      }

      if (SgTemplateClassDeclaration *tmpl_decl =
              isSgTemplateClassDeclaration(decl)) {
        if (SgClassDefinition *tmpl_def = tmpl_decl->get_definition()) {
          if (cursor == tmpl_def) {
            return true;
          }
        }
      }

      if (SgTemplateInstantiationDecl *inst_decl =
              isSgTemplateInstantiationDecl(decl)) {
        if (SgClassDefinition *inst_def = inst_decl->get_definition()) {
          if (cursor == inst_def) {
            return true;
          }
        }
      }
    }

    return false;
  };

  SgType *cast_type = nullptr;
  if (clang::TypeSourceInfo *written_type_info =
          cxx_functional_cast_expr->getTypeInfoAsWritten()) {
    clang::TypeLoc written_loc = written_type_info->getTypeLoc();
    auto qualifier_has_type_component =
        [](clang::NestedNameSpecifier qualifier) -> bool {
      for (clang::NestedNameSpecifier current = qualifier; current;
           current = nestedNameSpecifierPrefix(current)) {
        switch (current.getKind()) {
        case clang::NestedNameSpecifier::Kind::Type:
        case clang::NestedNameSpecifier::Kind::MicrosoftSuper:
          return true;
        default:
          break;
        }
      }
      return false;
    };
    auto qualifier_has_namespace_component =
        [](clang::NestedNameSpecifier qualifier) -> bool {
      for (clang::NestedNameSpecifier current = qualifier; current;
           current = nestedNameSpecifierPrefix(current)) {
        switch (current.getKind()) {
        case clang::NestedNameSpecifier::Kind::Namespace:
        case clang::NestedNameSpecifier::Kind::Global:
          return true;
        default:
          break;
        }
      }
      return false;
    };
    std::function<std::string(const clang::TemplateName &)>
        get_template_name_base =
            [&](const clang::TemplateName &template_name) -> std::string {
      if (clang::TemplateDecl *template_decl =
              template_name.getAsTemplateDecl()) {
        return template_decl->getNameAsString();
      }
      if (const clang::QualifiedTemplateName *qualified =
              template_name.getAsQualifiedTemplateName()) {
        return get_template_name_base(qualified->getUnderlyingTemplate());
      }
      if (const clang::DependentTemplateName *dependent =
              template_name.getAsDependentTemplateName()) {
        clang::IdentifierOrOverloadedOperator base = dependent->getName();
        if (const clang::IdentifierInfo *id = base.getIdentifier()) {
          return id->getName().str();
        }
        return "";
      }
      if (const clang::SubstTemplateTemplateParmStorage *subst =
              template_name.getAsSubstTemplateTemplateParm()) {
        return get_template_name_base(subst->getReplacement());
      }
      if (clang::UsingShadowDecl *using_shadow =
              template_name.getAsUsingShadowDecl()) {
        return using_shadow->getNameAsString();
      }
      return "";
    };
    auto build_explicit_alias_template_cast_type =
        [&](clang::TypeLoc type_loc) -> SgType * {
      while (!type_loc.isNull()) {
        if (clang::ParenTypeLoc paren_loc =
                type_loc.getAs<clang::ParenTypeLoc>()) {
          type_loc = paren_loc.getInnerLoc();
          continue;
        }
        if (clang::AttributedTypeLoc attributed_loc =
                type_loc.getAs<clang::AttributedTypeLoc>()) {
          type_loc = attributed_loc.getModifiedLoc();
          continue;
        }
        if (clang::AdjustedTypeLoc adjusted_loc =
                type_loc.getAs<clang::AdjustedTypeLoc>()) {
          type_loc = adjusted_loc.getOriginalLoc();
          continue;
        }

        clang::TemplateSpecializationTypeLoc spec_loc =
            type_loc.getAs<clang::TemplateSpecializationTypeLoc>();
        if (spec_loc.isNull()) {
          return nullptr;
        }

        const clang::TemplateSpecializationType *spec_type =
            spec_loc.getTypePtr();
        if (spec_type == nullptr || !spec_type->isTypeAlias()) {
          return nullptr;
        }

        clang::TemplateName template_name = spec_type->getTemplateName();
        clang::NestedNameSpecifier qualifier = std::nullopt;
        if (const clang::QualifiedTemplateName *qualified =
                template_name.getAsQualifiedTemplateName()) {
          qualifier = qualified->getQualifier();
        } else if (const clang::DependentTemplateName *dependent =
                       template_name.getAsDependentTemplateName()) {
          qualifier = dependent->getQualifier();
        }

        if (!qualifier || !qualifier_has_namespace_component(qualifier) ||
            qualifier_has_type_component(qualifier)) {
          return nullptr;
        }

        std::string base_name = get_template_name_base(template_name);
        if (base_name.empty()) {
          return nullptr;
        }

        SgTemplateArgumentPtrList template_args;
        for (unsigned i = 0; i < spec_loc.getNumArgs(); ++i) {
          appendTemplateArguments(template_args, spec_loc.getArgLoc(i), true);
        }

        SgScopeStatement *scope = SageBuilder::topScopeStack();
        if (scope == nullptr) {
          scope = getGlobalScope();
        }
        return buildNonrealTypeFromNestedNameSpecifier(
            qualifier, scope, SgName(base_name),
            template_args.empty() ? nullptr : &template_args);
      }
      return nullptr;
    };

    if (has_explicit_qualifier(written_loc)) {
      cast_type = build_explicit_alias_template_cast_type(written_loc);
    }
    if (!has_explicit_qualifier(written_loc)) {
      clang::QualType written_qt = written_type_info->getType();
      if (const clang::TypedefType *written_typedef =
              llvm::dyn_cast_or_null<clang::TypedefType>(
                  written_qt.getTypePtrOrNull())) {
        clang::TypedefNameDecl *typedef_decl = written_typedef->getDecl();
        if (typedef_decl != nullptr) {
          std::string typedef_name = typedef_decl->getNameAsString();
          if (!typedef_name.empty()) {
            clang::DeclContext *decl_ctx = typedef_decl->getDeclContext();
            clang::CXXRecordDecl *record_ctx =
                llvm::dyn_cast_or_null<clang::CXXRecordDecl>(decl_ctx);
            if (record_ctx != nullptr) {
              SgDeclarationStatement *sg_record_decl =
                  lookupSgDeclarationForClangDecl(record_ctx,
                                                  /*allow_on_demand=*/true);
              if (sg_record_decl == nullptr) {
                sg_record_decl = lookupSgDeclarationForClangDecl(
                    record_ctx->getCanonicalDecl(),
                    /*allow_on_demand=*/true);
              }

              if (scope_contains_declaration(SageBuilder::topScopeStack(),
                                             sg_record_decl)) {
                SgScopeStatement *scope = SageBuilder::topScopeStack();
                if (scope == nullptr) {
                  scope = getGlobalScope();
                }
                cast_type = SageBuilder::buildNonrealType(SgName(typedef_name),
                                                          scope, nullptr);
              }
            }
          }
        }
      }
    }
  }

  if (cast_type == nullptr) {
    if (clang::TypeSourceInfo *written_type_info =
            cxx_functional_cast_expr->getTypeInfoAsWritten()) {
      cast_type = buildTypeFromTypeLoc(written_type_info->getTypeLoc());
    }
  }
  if (cast_type == nullptr) {
    cast_type = buildTypeFromQualifiedType(
        cxx_functional_cast_expr->getTypeAsWritten());
  }

  bool class_unknown = false;
  if (cast_type != nullptr) {
    if (isSgTypedefType(cast_type) == nullptr &&
        isSgClassType(cast_type) == nullptr) {
      class_unknown = true;
    }
  } else {
    class_unknown = true;
  }

  SgConstructorInitializer *ctor_init =
      SageBuilder::buildConstructorInitializer_nfi(
          nullptr, // declaration (filled in later by AST fixup if needed)
          args, cast_type,
          true,         // need_name
          false,        // need_qualifier
          false,        // need_parenthesis_after_name
          class_unknown // associated_class_unknown
      );

  ctor_init->set_is_explicit_cast(true);
  ctor_init->set_is_braced_initialized(
      cxx_functional_cast_expr->isListInitialization());

  *node = ctor_init;

  return VisitExpr(cxx_functional_cast_expr, node) && res;
}

bool ClangToSageTranslator::VisitCXXNamedCastExpr(
    clang::CXXNamedCastExpr *cxx_named_cast_expr, SgNode **node) {
#if DEBUG_VISIT_STMT
  std::cerr << "ClangToSageTranslator::VisitCXXNamedCastExpr" << std::endl;
#endif
  bool res = true;

  // TODO

  return VisitExplicitCastExpr(cxx_named_cast_expr, node) && res;
}

bool ClangToSageTranslator::VisitCXXConstCastExpr(
    clang::CXXConstCastExpr *cxx_const_cast_expr, SgNode **node) {
#if DEBUG_VISIT_STMT
  std::cerr << "ClangToSageTranslator::VisitCXXConstCastExpr" << std::endl;
#endif
  bool res = true;

  // TODO

  return VisitCXXNamedCastExpr(cxx_const_cast_expr, node) && res;
}

bool ClangToSageTranslator::VisitCXXDynamicCastExpr(
    clang::CXXDynamicCastExpr *cxx_dynamic_cast_expr, SgNode **node) {
#if DEBUG_VISIT_STMT
  std::cerr << "ClangToSageTranslator::VisitCXXDynamicCastExpr" << std::endl;
#endif
  bool res = true;

  // TODO

  return VisitCXXNamedCastExpr(cxx_dynamic_cast_expr, node) && res;
}

bool ClangToSageTranslator::VisitCXXReinterpretCastExpr(
    clang::CXXReinterpretCastExpr *cxx_reinterpret_cast_expr, SgNode **node) {
#if DEBUG_VISIT_STMT
  std::cerr << "ClangToSageTranslator::VisitCXXReinterpretCastExpr"
            << std::endl;
#endif
  bool res = true;

  // TODO

  return VisitCXXNamedCastExpr(cxx_reinterpret_cast_expr, node) && res;
}

bool ClangToSageTranslator::VisitCXXStaticCastExpr(
    clang::CXXStaticCastExpr *cxx_static_cast_expr, SgNode **node) {
#if DEBUG_VISIT_STMT
  std::cerr << "ClangToSageTranslator::VisitCXXStaticCastExpr" << std::endl;
#endif
  SgNode *tmp_expr = Traverse(cxx_static_cast_expr->getSubExpr());
  SgExpression *expr = isSgExpression(tmp_expr);
  SgType *type = buildTypeFromQualifiedType(cxx_static_cast_expr->getType());
  SgCastExp *res =
      SageBuilder::buildCastExp(expr, type, SgCastExp::e_dynamic_cast);

  *node = res;

  return VisitCXXNamedCastExpr(cxx_static_cast_expr, node);
}

bool ClangToSageTranslator::VisitImplicitCastExpr(
    clang::ImplicitCastExpr *implicit_cast_expr, SgNode **node) {
#if DEBUG_VISIT_STMT
  std::cerr << "ClangToSageTranslator::VisitImplicitCastExpr" << std::endl;
#endif

  SgNode *tmp_expr = Traverse(implicit_cast_expr->getSubExpr());
  SgExpression *expr = isSgExpression(tmp_expr);

  ROSE_ASSERT(expr != nullptr);

  // Materialize implicit scalar-to-bool conversions as compiler-generated cast
  // nodes. This preserves CFG/AST node parity for control-flow conditions while
  // keeping unparsing unchanged (compiler-generated C-style casts are skipped).
  switch (implicit_cast_expr->getCastKind()) {
  case clang::CK_IntegralToBoolean:
  case clang::CK_FloatingToBoolean:
  case clang::CK_PointerToBoolean:
  case clang::CK_MemberPointerToBoolean:
  case clang::CK_IntegralToFloating: {
    SgType *target_type =
        buildTypeFromQualifiedType(implicit_cast_expr->getType());
    if (target_type != nullptr) {
      SgCastExp *cast_expr = SageBuilder::buildCastExp(
          expr, target_type, SgCastExp::e_C_style_cast);
      ROSE_ASSERT(cast_expr != nullptr);
      setCompilerGeneratedFileInfo(cast_expr, true);
      ROSE_ASSERT(cast_expr->get_file_info() != nullptr);
      cast_expr->get_file_info()->setImplicitCast();
      *node = cast_expr;
      return VisitExpr(implicit_cast_expr, node);
    }
    break;
  }
  default:
    break;
  }

  // FIX: Pass through implicit casts without creating SgCastExp nodes
  // Legacy frontend doesn't create explicit cast nodes for implicit casts
  // Creating them breaks parent pointer relationships (e.g., FunctionRefExp
  // parent becomes CastExp instead of FunctionCallExp) This matches the
  // behavior expected by existing ROSE tests

  // LIMITATION: The sub-expression retains its original type (e.g., int stays
  // int even if cast to double). SgExpression types are immutable in ROSE -
  // there is no set_type() method. This matches legacy frontend behavior where
  // implicit casts don't create SgCastExp nodes. Most ROSE analyses handle this
  // correctly. If type correctness is critical for a specific analysis, that
  // analysis should consult the Clang AST context or implement its own type
  // inference.
  *node = expr;

  return VisitExpr(implicit_cast_expr, node);
}

bool ClangToSageTranslator::VisitCharacterLiteral(
    clang::CharacterLiteral *character_literal, SgNode **node) {
#if DEBUG_VISIT_STMT
  std::cerr << "ClangToSageTranslator::VisitCharacterLiteral" << std::endl;
#endif

  *node = SageBuilder::buildCharVal(character_literal->getValue());

  return VisitExpr(character_literal, node);
}

bool ClangToSageTranslator::VisitChooseExpr(clang::ChooseExpr *choose_expr,
                                            SgNode **node) {
#if DEBUG_VISIT_STMT
  std::cerr << "ClangToSageTranslator::VisitChooseExpr" << std::endl;
#endif
  clang::Expr *cond_sub_expr = choose_expr->getCond();
  clang::Expr *true_sub_expr = choose_expr->getLHS();
  clang::Expr *false_sub_expr = choose_expr->getRHS();
  if (cond_sub_expr == nullptr || true_sub_expr == nullptr ||
      false_sub_expr == nullptr) {
    ROSE_ASSERT(!"ChooseExpr has null subexpression");
    return false;
  }

  // __builtin_choose_expr is compile-time conditional selection.
  // For non-dependent conditions, evaluate and translate only the chosen arm.
  if (!choose_expr->isConditionDependent() &&
      !choose_expr->isValueDependent() && p_compiler_instance != nullptr) {
    clang::Expr::EvalResult eval_result;
    if (cond_sub_expr->EvaluateAsInt(eval_result,
                                     p_compiler_instance->getASTContext())) {
      const llvm::APSInt &value = eval_result.Val.getInt();
      clang::Expr *chosen_sub_expr =
          value != 0 ? true_sub_expr : false_sub_expr;
      ROSE_ASSERT(chosen_sub_expr != nullptr);

      *node = Traverse(chosen_sub_expr);
      ROSE_ASSERT(*node != nullptr);
      applySourceRange(*node, choose_expr->getSourceRange());

      return VisitExpr(choose_expr, node);
    }
  }

  SgExpression *cond_expr = isSgExpression(Traverse(cond_sub_expr));
  ROSE_ASSERT(cond_expr != nullptr);

  SgExpression *true_expr = isSgExpression(Traverse(true_sub_expr));
  ROSE_ASSERT(true_expr != nullptr);

  SgExpression *false_expr = isSgExpression(Traverse(false_sub_expr));
  ROSE_ASSERT(false_expr != nullptr);

  // Clang has already computed the final conditional-result type (after
  // standard conversions/promotions). Preserve that exact type in ROSE.
  SgType *result_type = buildTypeFromQualifiedType(choose_expr->getType());
  ROSE_ASSERT(result_type != nullptr);

  SgConditionalExp *cond_exp = SageBuilder::buildConditionalExp_nfi(
      cond_expr, true_expr, false_expr, result_type);
  ROSE_ASSERT(cond_exp != nullptr);
  applySourceRange(cond_exp, choose_expr->getSourceRange());
  *node = cond_exp;

  return VisitExpr(choose_expr, node);
}

bool ClangToSageTranslator::VisitCompoundLiteralExpr(
    clang::CompoundLiteralExpr *compound_literal, SgNode **node) {
#if DEBUG_VISIT_STMT
  std::cerr << "ClangToSageTranslator::VisitCompoundLiteralExpr" << std::endl;
#endif

  SgType *type = buildTypeFromQualifiedType(compound_literal->getType());
  ROSE_ASSERT(type != nullptr);
  SgNode *tmp_node = Traverse(compound_literal->getInitializer());
  SgExprListExp *expr = isSgExprListExp(tmp_node);
  ROSE_ASSERT(expr != nullptr);

  SgAggregateInitializer *initializer =
      SageBuilder::buildAggregateInitializer_nfi(expr, type);

  initializer->set_uses_compound_literal(true);

  SgName name = std::string("compound_literal_") +
                Rose::StringUtility::numberToString(compound_literal);
  SgScopeStatement *scope = SageBuilder::topScopeStack();
  SgVariableDeclaration *var_decl =
      new SgVariableDeclaration(SgName(name), type, initializer);
  var_decl->set_scope(scope);
  var_decl->set_parent(scope);
  var_decl->set_firstNondefiningDeclaration(var_decl);
  var_decl->set_definingDeclaration(var_decl);
  var_decl->set_file_info(
      Sg_File_Info::generateDefaultFileInfoForCompilerGeneratedNode());
  var_decl->set_endOfConstruct(
      Sg_File_Info::generateDefaultFileInfoForCompilerGeneratedNode());
  if (Sg_File_Info *fi = var_decl->get_file_info()) {
    fi->setCompilerGenerated();
    fi->unsetOutputInCodeGeneration();
  }
  if (Sg_File_Info *fi = var_decl->get_endOfConstruct()) {
    fi->setCompilerGenerated();
    fi->unsetOutputInCodeGeneration();
  }

  ROSE_ASSERT(!var_decl->get_variables().empty());
  SgInitializedName *iname = var_decl->get_variables()[0];
  iname->set_scope(scope);
  iname->set_parent(var_decl);
  iname->set_file_info(
      Sg_File_Info::generateDefaultFileInfoForCompilerGeneratedNode());
  iname->set_endOfConstruct(
      Sg_File_Info::generateDefaultFileInfoForCompilerGeneratedNode());
  if (Sg_File_Info *fi = iname->get_file_info()) {
    fi->setCompilerGenerated();
    fi->unsetOutputInCodeGeneration();
  }
  if (Sg_File_Info *fi = iname->get_endOfConstruct()) {
    fi->setCompilerGenerated();
    fi->unsetOutputInCodeGeneration();
  }

  if (initializer != nullptr) {
    initializer->set_parent(iname);
  }

  SgVariableSymbol *vsym = new SgVariableSymbol(iname);
  ROSE_ASSERT(vsym != nullptr);

  if (scope != nullptr) {
    attachSymbolToScopeOrOrphan(vsym, scope);
  } else {
    attachSymbolToScopeOrOrphan(vsym, nullptr);
  }
  var_decl->set_isAssociatedWithDeclarationList(true);

  // REX FIX: Check if the type is defined inside the compound literal
  // If so, mark the declaration as non-autonomous so it gets unparsed
  // correctly
  if (clang::TypeSourceInfo *TInfo = compound_literal->getTypeSourceInfo()) {
    clang::TypeLoc TL = TInfo->getTypeLoc();
    // Drill down to the underlying TagTypeLoc, stripping common wrappers
    // (const qualifiers, parens, attributes, arrays, pointers, elaborations).
    while (true) {
      bool advanced = false;

      // Strip top-level qualifiers
      TL = TL.getUnqualifiedLoc();

      if (auto ParenTL = TL.getAs<clang::ParenTypeLoc>()) {
        TL = ParenTL.getInnerLoc();
        advanced = true;
      } else if (auto AttrTL = TL.getAs<clang::AttributedTypeLoc>()) {
        TL = AttrTL.getModifiedLoc();
        advanced = true;
      } else if (auto AdjTL = TL.getAs<clang::AdjustedTypeLoc>()) {
        TL = AdjTL.getOriginalLoc();
        advanced = true;
      } else if (auto ATL = TL.getAs<clang::ArrayTypeLoc>()) {
        TL = ATL.getElementLoc();
        advanced = true;
      } else if (auto PTL = TL.getAs<clang::PointerTypeLoc>()) {
        TL = PTL.getPointeeLoc();
        advanced = true;
      }

      if (!advanced)
        break;
    }

    if (auto TagTL = TL.getAs<clang::TagTypeLoc>()) {
      if (TagTL.isDefinition()) {
        clang::TagDecl *TD = TagTL.getDecl();
        std::map<clang::Decl *, SgNode *>::iterator it =
            p_decl_translation_map.find(TD);
        if (it != p_decl_translation_map.end()) {
          SgNode *node = it->second;
          if (SgClassDeclaration *classDecl = isSgClassDeclaration(node)) {
            classDecl->set_isAutonomousDeclaration(false);
            if (SgClassDeclaration *defDecl = isSgClassDeclaration(
                    classDecl->get_definingDeclaration())) {
              defDecl->set_isAutonomousDeclaration(false);
              if (!isSgDeclarationStatement(defDecl->get_parent())) {
                defDecl->set_parent(var_decl);
                var_decl->set_baseTypeDefiningDeclaration(defDecl);
                var_decl
                    ->set_variableDeclarationContainsBaseTypeDefiningDeclaration(
                        true);
              }
            }
          } else if (SgEnumDeclaration *enumDecl = isSgEnumDeclaration(node)) {
            enumDecl->set_isAutonomousDeclaration(false);
            if (SgEnumDeclaration *defDecl =
                    isSgEnumDeclaration(enumDecl->get_definingDeclaration())) {
              defDecl->set_isAutonomousDeclaration(false);
              if (!isSgDeclarationStatement(defDecl->get_parent())) {
                defDecl->set_parent(var_decl);
                var_decl->set_baseTypeDefiningDeclaration(defDecl);
                var_decl
                    ->set_variableDeclarationContainsBaseTypeDefiningDeclaration(
                        true);
              }
            }
          }
        }
      }
    }
  }

  *node = SageBuilder::buildCompoundLiteralExp_nfi(vsym);

  return VisitExpr(compound_literal, node);
}

bool ClangToSageTranslator::VisitConceptSpecializationExpr(
    clang::ConceptSpecializationExpr *concept_specialization_expr,
    SgNode **node) {
#if DEBUG_VISIT_STMT
  std::cerr << "ClangToSageTranslator::VisitConceptSpecializationExpr"
            << std::endl;
#endif
  bool res = true;

  SgTemplateArgumentPtrList template_args;
  const clang::ASTTemplateArgumentListInfo *args_written =
      concept_specialization_expr->getTemplateArgsAsWritten();
  if (args_written != nullptr && args_written->getLAngleLoc().isValid() &&
      args_written->getRAngleLoc().isValid()) {
    for (const clang::TemplateArgumentLoc &arg_loc :
         args_written->arguments()) {
      appendTemplateArguments(template_args, arg_loc, true);
    }
  }

  SgNonrealRefExp *ref = nullptr;
  clang::ConceptDecl *concept_decl =
      concept_specialization_expr->getNamedConcept();
  if (concept_decl != nullptr) {
    auto it = p_decl_translation_map.find(concept_decl);
    if (it == p_decl_translation_map.end() &&
        p_decl_translation_in_progress.find(concept_decl) ==
            p_decl_translation_in_progress.end() &&
        p_decl_translation_on_demand.find(concept_decl) ==
            p_decl_translation_on_demand.end()) {
      TraverseOnDemand(concept_decl);
      it = p_decl_translation_map.find(concept_decl);
    }
    if (it != p_decl_translation_map.end()) {
      if (SgNonrealDecl *nrdecl = isSgNonrealDecl(it->second)) {
        if (SgNonrealSymbol *sym =
                isSgNonrealSymbol(nrdecl->get_symbol_from_symbol_table())) {
          ref = SageBuilder::buildNonrealRefExp_nfi(sym);
        }
      }
    }
  }

  if (ref == nullptr) {
    std::string name =
        concept_decl != nullptr ? concept_decl->getNameAsString() : "__concept";
    SgScopeStatement *scope = SageBuilder::topScopeStack();
    if (scope == nullptr) {
      scope = getGlobalScope();
    }
    ref = buildNonrealRefExpFromNestedNameSpecifier(
        std::nullopt, scope, SgName(name), false,
        template_args.empty() ? nullptr : &template_args);
  }

  if (ref != nullptr && !template_args.empty()) {
    ref->get_templateArguments() = template_args;
    SageBuilder::setTemplateArgumentParents(ref);
  }

  if (ref != nullptr && !concept_specialization_expr->isValueDependent()) {
    const clang::ASTConstraintSatisfaction &satisfaction =
        concept_specialization_expr->getSatisfaction();
    ConstraintSatisfactionResult result;
    result.evaluated = true;
    result.contains_errors = satisfaction.ContainsErrors;
    result.satisfied = satisfaction.IsSatisfied && !result.contains_errors;
    attachConstraintSatisfaction(ref, result);
  }

  *node = ref != nullptr ? static_cast<SgNode *>(ref)
                         : static_cast<SgNode *>(buildFallbackExpression(
                               concept_specialization_expr));

  if (SgExpression *expr = isSgExpression(*node)) {
    applySourceRange(expr, concept_specialization_expr->getSourceRange());
  }

  return VisitExpr(concept_specialization_expr, node) && res;
}

bool ClangToSageTranslator::VisitConvertVectorExpr(
    clang::ConvertVectorExpr *convert_vector_expr, SgNode **node) {
#if DEBUG_VISIT_STMT
  std::cerr << "ClangToSageTranslator::VisitConvertVectorExpr" << std::endl;
#endif
  bool res = true;

  // TODO

  return VisitExpr(convert_vector_expr, node) && res;
}

bool ClangToSageTranslator::buildCoroutineAwaitExpression(
    clang::Expr *operand, clang::SourceRange source_range,
    const char *operand_description, SgNode **node) {
  ROSE_ASSERT(node != nullptr);

  bool res = true;
  SgExpression *operand_expr = nullptr;
  if (operand != nullptr) {
    SgNode *tmp_operand = Traverse(operand);
    operand_expr = isSgExpression(tmp_operand);
    if (tmp_operand != nullptr && operand_expr == nullptr) {
      std::cerr << "Runtime error: "
                << (operand_description != nullptr ? operand_description
                                                   : "coroutine operand")
                << " did not translate into SgExpression" << std::endl;
      res = false;
    }
  }

  if (operand_expr == nullptr) {
    operand_expr = SageBuilder::buildNullExpression();
  }

  SgAwaitExpression *await_expr = SageBuilder::buildAwaitExpression_nfi();
  ROSE_ASSERT(await_expr != nullptr);
  await_expr->set_value(operand_expr);
  if (operand_expr->get_parent() == nullptr) {
    operand_expr->set_parent(await_expr);
  }
  applySourceRange(await_expr, source_range);
  *node = await_expr;

  return res;
}

bool ClangToSageTranslator::VisitCoroutineSuspendExpr(
    clang::CoroutineSuspendExpr *coroutine_suspend_expr, SgNode **node) {
#if DEBUG_VISIT_STMT
  std::cerr << "ClangToSageTranslator::VisitCoroutineSuspendExpr" << std::endl;
#endif
  bool res = buildCoroutineAwaitExpression(
      coroutine_suspend_expr->getOperand(),
      coroutine_suspend_expr->getSourceRange(), "coroutine operand", node);

  return VisitExpr(coroutine_suspend_expr, node) && res;
}

bool ClangToSageTranslator::VisitCoawaitExpr(clang::CoawaitExpr *coawait_expr,
                                             SgNode **node) {
#if DEBUG_VISIT_STMT
  std::cerr << "ClangToSageTranslator::VisitCoawaitExpr" << std::endl;
#endif
  bool res = VisitCoroutineSuspendExpr(coawait_expr, node);
  if (res) {
    setCoroutineKeywordAttribute(*node, "co_await");
  }
  return res;
}

bool ClangToSageTranslator::VisitCoyieldExpr(clang::CoyieldExpr *coyield_expr,
                                             SgNode **node) {
#if DEBUG_VISIT_STMT
  std::cerr << "ClangToSageTranslator::VisitCoyieldExpr" << std::endl;
#endif
  bool res = true;

  clang::Expr *operand = coyield_expr->getOperand();
  if (operand != nullptr) {
    if (clang::Expr *stripped = operand->IgnoreParenImpCasts()) {
      if (auto *member_call =
              llvm::dyn_cast<clang::CXXMemberCallExpr>(stripped)) {
        if (member_call->getNumArgs() == 1) {
          if (auto *member_expr = llvm::dyn_cast<clang::MemberExpr>(
                  member_call->getCallee()->IgnoreParenImpCasts())) {
            if (member_expr->getMemberNameInfo().getAsString() ==
                "yield_value") {
              operand = member_call->getArg(0);
            }
          }
        }
      }
    }
  }

  res = buildCoroutineAwaitExpression(operand, coyield_expr->getSourceRange(),
                                      "co_yield operand", node);
  if (res) {
    setCoroutineKeywordAttribute(*node, "co_yield");
  }

  return VisitExpr(coyield_expr, node) && res;
}

bool ClangToSageTranslator::VisitCXXBindTemporaryExpr(
    clang::CXXBindTemporaryExpr *cxx_bind_temporary_expr, SgNode **node) {
#if DEBUG_VISIT_STMT
  std::cerr << "ClangToSageTranslator::VisitCXXBindTemporaryExpr" << std::endl;
#endif
  bool res = true;

  // ROOT CAUSE FIX: CXXBindTemporaryExpr extends lifetime of temporary object
  // ROSE handles temporaries differently - just traverse the subexpression
  clang::Expr *sub_expr = cxx_bind_temporary_expr->getSubExpr();
  if (sub_expr != nullptr) {
    *node = Traverse(sub_expr);
    if (*node == nullptr) {
      return false;
    }
  } else {
    return false;
  }

  return VisitExpr(cxx_bind_temporary_expr, node) && res;
}

bool ClangToSageTranslator::VisitCXXBoolLiteralExpr(
    clang::CXXBoolLiteralExpr *cxx_bool_literal_expr, SgNode **node) {
#if DEBUG_VISIT_STMT
  std::cerr << "ClangToSageTranslator::VisitCXXBoolLiteralExpr" << std::endl;
#endif
  bool res = true;

  // C++ boolean literals (true/false)
  bool value = cxx_bool_literal_expr->getValue();
  *node = SageBuilder::buildBoolValExp(value);
  applySourceRange(*node, cxx_bool_literal_expr->getSourceRange());

  return VisitExpr(cxx_bool_literal_expr, node) && res;
}

bool ClangToSageTranslator::VisitCXXConstructExpr(
    clang::CXXConstructExpr *cxx_construct_expr, SgNode **node) {
#if DEBUG_VISIT_STMT
  std::cerr << "ClangToSageTranslator::VisitCXXConstructExpr" << std::endl;
  // isElidable seems to be related to copy elision:
  // https://en.cppreference.com/w/cpp/language/copy_elision
  std::cerr << "isElidable " << cxx_construct_expr->isElidable() << std::endl;
  std::cerr << "hadMultipleCandidates "
            << cxx_construct_expr->hadMultipleCandidates() << std::endl;
  std::cerr << "isListInitialization "
            << cxx_construct_expr->isListInitialization() << std::endl;
#endif
  bool res = true;

  // Get the constructor being called
  clang::CXXConstructorDecl *ctor_decl = cxx_construct_expr->getConstructor();

  if (ctor_decl != nullptr) {
    // Get the type being constructed
    SgType *constructed_type = nullptr;
    if (clang::CXXTemporaryObjectExpr *temporary_object_expr =
            llvm::dyn_cast<clang::CXXTemporaryObjectExpr>(cxx_construct_expr)) {
      if (clang::TypeSourceInfo *type_info =
              temporary_object_expr->getTypeSourceInfo()) {
        constructed_type = buildTypeFromTypeLoc(type_info->getTypeLoc());
      }
    }
    if (constructed_type == nullptr) {
      constructed_type =
          buildTypeFromQualifiedType(cxx_construct_expr->getType());
    }

    // Build argument list for constructor call
    // Note: Empty argument lists are intentional and valid for default
    // constructors or when all arguments fail traversal (e.g.,
    // template-dependent arguments)
    SgExprListExp *args = SageBuilder::buildExprListExp_nfi();
    applySourceRange(args, cxx_construct_expr->getParenOrBraceRange());

    auto is_std_init_list_arg = [](clang::Expr *arg) {
      clang::Expr *base = arg;
      while (base != nullptr) {
        base = base->IgnoreParenImpCasts();
        if (llvm::isa<clang::CXXStdInitializerListExpr>(base)) {
          return true;
        }
        if (auto *materialize =
                llvm::dyn_cast<clang::MaterializeTemporaryExpr>(base)) {
          base = materialize->getSubExpr();
          continue;
        }
        if (auto *bind = llvm::dyn_cast<clang::CXXBindTemporaryExpr>(base)) {
          base = bind->getSubExpr();
          continue;
        }
        break;
      }
      return false;
    };

    // Traverse constructor arguments
    for (unsigned i = 0; i < cxx_construct_expr->getNumArgs(); ++i) {
      clang::Expr *arg = cxx_construct_expr->getArg(i);
      if (arg != nullptr) {
        // CLANG FRONTEND FIX #19: Skip default arguments
        // CXXDefaultArgExpr represents implicit default arguments that
        // shouldn't appear in the explicit argument list. For example:
        //   std::string str("hello");  // Should have 1 arg, not 2
        // The allocator parameter is a default argument and should be omitted.
        if (clang::isa<clang::CXXDefaultArgExpr>(arg)) {
          continue;
        }

        SgNode *sg_arg = Traverse(arg);
        if (SgExpression *sg_expr = isSgExpression(sg_arg)) {
          if (cxx_construct_expr->isListInitialization() &&
              is_std_init_list_arg(arg)) {
            if (SgAggregateInitializer *agg_init =
                    isSgAggregateInitializer(sg_expr)) {
              agg_init->set_need_explicit_braces(false);
            }
          }
          args->append_expression(sg_expr);
        }
      }
    }

    // Use SgConstructorInitializer to properly represent constructor calls
    // This ensures the expression has the constructed class type, not void

    // Check if the type satisfies SgConstructorInitializer requirements
    // The assertion requires: isSgTypedefType or isSgClassType or
    // associated_class_unknown==true
    bool class_unknown = false;
    if (constructed_type != nullptr) {
      if (isSgTypedefType(constructed_type) == nullptr &&
          isSgClassType(constructed_type) == nullptr) {
        // Type is neither typedef nor class type, set flag to true
        class_unknown = true;
      }
    } else {
      // No type available, set flag to true
      class_unknown = true;
    }

    SgConstructorInitializer *ctor_init =
        SageBuilder::buildConstructorInitializer_nfi(
            nullptr, // declaration (filled in later by AST fixup if needed)
            args, constructed_type,
            false,        // need_name
            false,        // need_qualifier
            false,        // need_parenthesis_after_name
            class_unknown // associated_class_unknown
        );
    if (SgMemberFunctionDeclaration *sg_ctor_decl =
            isSgMemberFunctionDeclaration(Traverse(ctor_decl))) {
      ctor_init->set_declaration(sg_ctor_decl);
    }
    bool isCompilerGenerated = false;
    SgClassDeclaration *enclosingClassDecl = nullptr;
    if (ctor_decl != nullptr) {
      if (clang::CXXRecordDecl *parent_record = ctor_decl->getParent()) {
        enclosingClassDecl = isSgClassDeclaration(Traverse(parent_record));
      }
    }
#if DEBUG_VISIT_STMT
    if (ctor_decl != nullptr) {
      std::cerr << "clang::CXXConstructExpr: is default constructor: "
                << ctor_decl->isDefaultConstructor() << std::endl;
      if (enclosingClassDecl != nullptr) {
        std::cerr << "clang::CXXConstructExpr: is from UnNamed class: "
                  << enclosingClassDecl->get_isUnNamed() << std::endl;
      }
    }
#endif
    if (ctor_decl != nullptr && ctor_decl->isDefaultConstructor() &&
        enclosingClassDecl != nullptr && enclosingClassDecl->get_isUnNamed()) {
      isCompilerGenerated = true;
    }

    if (isCompilerGenerated) {
      setCompilerGeneratedFileInfo(ctor_init, false);
    } else {
      applySourceRange(ctor_init, cxx_construct_expr->getSourceRange());
    }

    // Preserve brace-init vs paren-init. Without this, list-initialization like
    // `T t{};` may be unparsed as `T t;`, which can change semantics (e.g.,
    // value-initialization vs default-initialization).
    ctor_init->set_is_braced_initialized(
        cxx_construct_expr->isListInitialization());

    *node = ctor_init;
  } else {
    // No constructor available, create a fallback expression
    *node = buildFallbackExpression(cxx_construct_expr);
  }

  return VisitExpr(cxx_construct_expr, node) && res;
}

bool ClangToSageTranslator::VisitCXXTemporaryObjectExpr(
    clang::CXXTemporaryObjectExpr *cxx_temporary_object_expr, SgNode **node) {
#if DEBUG_VISIT_STMT
  std::cerr << "ClangToSageTranslator::VisitCXXTemporaryObjectExpr"
            << std::endl;
#endif
  bool res = true;

  res = VisitCXXConstructExpr(cxx_temporary_object_expr, node) && res;
  if (res && *node != nullptr) {
    if (SgConstructorInitializer *ctor_init =
            isSgConstructorInitializer(*node)) {
      // Ensure temporaries print as `Type()` even with no args, since they can
      // be used as implicit objects (e.g., conversion operators).
      ctor_init->set_need_name(true);
      ctor_init->set_need_parenthesis_after_name(true);
    }
  }

  return res;
}

bool ClangToSageTranslator::VisitCXXDefaultArgExpr(
    clang::CXXDefaultArgExpr *cxx_default_arg_expr, SgNode **node) {
#if DEBUG_VISIT_STMT
  std::cerr << "ClangToSageTranslator::VisitCXXDefaultArgExpr" << std::endl;
#endif
  bool res = true;

  // CXXDefaultArgExpr represents use of a default argument in a function call
  // The referenced default argument expression is owned by the parameter
  // declaration. Reusing the same SgNode in both the declaration and call-site
  // would create multiple parents and break CFG invariants. Always deep-copy
  // the translated expression for the call-site use.
  if (cxx_default_arg_expr->getExpr() != nullptr) {
    SgNode *tmp_expr = Traverse(cxx_default_arg_expr->getExpr());
    SgExpression *expr = isSgExpression(tmp_expr);
    if (tmp_expr != nullptr && expr == nullptr) {
      std::cerr << "Runtime error: tmp_expr != nullptr && expr == nullptr"
                << std::endl;
      res = false;
      *node = buildFallbackExpression(cxx_default_arg_expr);
    } else if (expr != nullptr) {
      SgExpression *expr_copy = SageInterface::deepCopy(expr);
      ROSE_ASSERT(expr_copy != nullptr);
      *node = expr_copy;
    } else {
      *node = buildFallbackExpression(cxx_default_arg_expr);
    }
  } else {
    // No expression available, use a fallback expression
    *node = buildFallbackExpression(cxx_default_arg_expr);
  }

  return VisitExpr(cxx_default_arg_expr, node) && res;
}

bool ClangToSageTranslator::VisitCXXDefaultInitExpr(
    clang::CXXDefaultInitExpr *cxx_default_init_expr, SgNode **node) {
#if DEBUG_VISIT_STMT
  std::cerr << "ClangToSageTranslator::VisitCXXDefaultInitExpr" << std::endl;
#endif
  bool res = true;

  clang::Expr *init_expr = cxx_default_init_expr->getExpr();
  ROSE_ASSERT(init_expr != nullptr);

  SgNode *tmp_expr = Traverse(init_expr);
  SgExpression *expr = isSgExpression(tmp_expr);
  ROSE_ASSERT(expr != nullptr);

  SgExpression *expr_copy = SageInterface::deepCopy(expr);
  ROSE_ASSERT(expr_copy != nullptr);
  *node = expr_copy;

  return res;
}

bool ClangToSageTranslator::VisitCXXDeleteExpr(
    clang::CXXDeleteExpr *cxx_delete_expr, SgNode **node) {
#if DEBUG_VISIT_STMT
  std::cerr << "ClangToSageTranslator::VisitCXXDeleteExpr" << std::endl;
#endif
  bool res = true;

  // ROOT CAUSE FIX: CXXDeleteExpr represents the C++ delete operator
  // Examples: delete ptr; or delete[] array;

  // Get the expression being deleted
  clang::Expr *arg_expr = cxx_delete_expr->getArgument();
  SgNode *tmp_arg = arg_expr ? Traverse(arg_expr) : nullptr;
  SgExpression *arg = isSgExpression(tmp_arg);

  if (arg == nullptr) {
    // If we can't get the argument, create a fallback expression
    arg = buildFallbackExpression(arg_expr);
  }

  // Check if this is array delete (delete[]) or single object delete (delete)
  bool is_array = cxx_delete_expr->isArrayForm();

  // Build the delete expression
  *node = SageBuilder::buildDeleteExp(arg, is_array, false, nullptr);

  return VisitExpr(cxx_delete_expr, node) && res;
}

bool ClangToSageTranslator::VisitCXXDependentScopeMemberExpr(
    clang::CXXDependentScopeMemberExpr *cxx_dependent_scope_member_expr,
    SgNode **node) {
#if DEBUG_VISIT_STMT
  std::cerr << "ClangToSageTranslator::VisitCXXDependentScopeMemberExpr"
            << std::endl;
#endif
  // std::cerr << "DEBUG: VisitCXXDependentScopeMemberExpr. Member: " <<
  // cxx_dependent_scope_member_expr->getMember().getAsString() << std::endl;
  bool res = true;

  // CXXDependentScopeMemberExpr represents member access on a
  // template-dependent type (e.g., obj.begin(), obj->data()).

  // Get the member name
  std::string member_name =
      cxx_dependent_scope_member_expr->getMember().getAsString();

  SgScopeStatement *current_scope = SageBuilder::topScopeStack();
  if (current_scope == nullptr) {
    current_scope = getGlobalScope();
  }
  ROSE_ASSERT(current_scope != nullptr);

  member_name = resolve_synthetic_template_param_name_in_stmt_scope(
      member_name, current_scope);

  SgExpression *base_expr = nullptr;
  if (!cxx_dependent_scope_member_expr->isImplicitAccess()) {
    SgNode *tmp_base = Traverse(cxx_dependent_scope_member_expr->getBase());
    base_expr = isSgExpression(tmp_base);
    ROSE_ASSERT(base_expr != nullptr);
  }

  // Try to resolve dependent members when the base type is a known record.
  // This preserves field types for template code like pair<_T1,_T2>::first.
  SgExpression *resolved_member = nullptr;
  clang::QualType base_type = cxx_dependent_scope_member_expr->getBaseType();
  auto resolve_record_decl =
      [](clang::QualType type) -> const clang::CXXRecordDecl * {
    if (type.isNull()) {
      return nullptr;
    }
    const clang::Type *type_ptr = type.getTypePtrOrNull();
    while (type_ptr != nullptr) {
      if (const auto *ref = llvm::dyn_cast<clang::ReferenceType>(type_ptr)) {
        type = ref->getPointeeType();
        type_ptr = type.getTypePtrOrNull();
        continue;
      }
      if (const auto *ptr = llvm::dyn_cast<clang::PointerType>(type_ptr)) {
        type = ptr->getPointeeType();
        type_ptr = type.getTypePtrOrNull();
        continue;
      }
      if (qualifiedTypeHasQualifier(type_ptr)) {
        type = type.getCanonicalType();
        type_ptr = type.getTypePtrOrNull();
        continue;
      }
      break;
    }
    if (type.isNull()) {
      return nullptr;
    }
    if (const clang::CXXRecordDecl *record = type->getAsCXXRecordDecl()) {
      return record;
    }
    if (const auto *spec =
            llvm::dyn_cast<clang::TemplateSpecializationType>(type_ptr)) {
      clang::TemplateDecl *tmpl_decl =
          spec->getTemplateName().getAsTemplateDecl();
      if (auto *class_tmpl =
              llvm::dyn_cast_or_null<clang::ClassTemplateDecl>(tmpl_decl)) {
        return class_tmpl->getTemplatedDecl();
      }
    }
    return nullptr;
  };

  const clang::CXXRecordDecl *record_decl = resolve_record_decl(base_type);
  if (record_decl != nullptr && base_expr != nullptr) {
    const clang::CXXRecordDecl *record_def =
        record_decl->getDefinition() != nullptr ? record_decl->getDefinition()
                                                : record_decl;
    SgScopeStatement *record_scope = resolveScopeFromDeclContext(
        const_cast<clang::CXXRecordDecl *>(record_def),
        SageBuilder::topScopeStack());
    if (record_scope == nullptr && record_def != nullptr) {
      if (p_decl_translation_in_progress.find(
              const_cast<clang::CXXRecordDecl *>(record_def)) ==
          p_decl_translation_in_progress.end()) {
        TraverseOnDemand(const_cast<clang::CXXRecordDecl *>(record_def));
        record_scope = resolveScopeFromDeclContext(
            const_cast<clang::CXXRecordDecl *>(record_def),
            SageBuilder::topScopeStack());
      }
    }

    clang::DeclContextLookupResult lookup =
        record_def->lookup(cxx_dependent_scope_member_expr->getMember());
    for (clang::NamedDecl *member_decl : lookup) {
      if (member_decl == nullptr) {
        continue;
      }
      clang::FieldDecl *field_decl =
          llvm::dyn_cast<clang::FieldDecl>(member_decl);
      if (field_decl == nullptr) {
        continue;
      }

      bool pushed_scope = false;
      if (record_scope != nullptr) {
        SageBuilder::pushScopeStack(record_scope);
        pushed_scope = true;
      }

      SgSymbol *field_sym = GetSymbolFromSymbolTable(field_decl);
      if (field_sym == nullptr &&
          p_decl_translation_in_progress.find(field_decl) ==
              p_decl_translation_in_progress.end()) {
        SgNode *field_node = TraverseOnDemand(field_decl);
        field_sym = GetSymbolFromSymbolTable(field_decl);
        if (field_sym == nullptr) {
          if (SgVariableDeclaration *var_decl =
                  isSgVariableDeclaration(field_node)) {
            if (SgInitializedName *init_name =
                    SageInterface::getFirstInitializedName(var_decl)) {
              field_sym = init_name->search_for_symbol_from_symbol_table();
            }
          }
        }
      }

      if (pushed_scope) {
        SageBuilder::popScopeStack();
      }

      if (SgVariableSymbol *var_sym = isSgVariableSymbol(field_sym)) {
        resolved_member = SageBuilder::buildVarRefExp(var_sym);
        break;
      }
    }
  }

  if (resolved_member != nullptr) {
    if (cxx_dependent_scope_member_expr->getQualifier() &&
        p_compiler_instance != nullptr) {
      clang::NestedNameSpecifierLoc qualifier_loc =
          cxx_dependent_scope_member_expr->getQualifierLoc();
      const clang::NestedNameSpecifierLoc *qualifier_loc_ptr =
          qualifier_loc.getNestedNameSpecifier() ? &qualifier_loc : nullptr;
      attachExplicitQualifierFromNestedName(
          resolved_member, cxx_dependent_scope_member_expr->getQualifier(),
          qualifier_loc_ptr, p_compiler_instance);
    }

    if (base_expr != nullptr) {
      if (cxx_dependent_scope_member_expr->isArrow()) {
        *node = SageBuilder::buildArrowExp(base_expr, resolved_member);
      } else {
        *node = SageBuilder::buildDotExp(base_expr, resolved_member);
      }
    } else {
      *node = resolved_member;
    }

    if (SgExpression *expr = isSgExpression(*node)) {
      applySourceRange(expr, cxx_dependent_scope_member_expr->getSourceRange());
    }

    return VisitExpr(cxx_dependent_scope_member_expr, node) && res;
  }

  SgTemplateArgumentPtrList template_args;
  const SgTemplateArgumentPtrList *template_args_ptr = nullptr;
  if (cxx_dependent_scope_member_expr->hasExplicitTemplateArgs()) {
    clang::TemplateArgumentListInfo arg_info;
    cxx_dependent_scope_member_expr->copyTemplateArgumentsInto(arg_info);
    template_args = buildTemplateArguments(arg_info, true);
    template_args_ptr = &template_args;
  }

  SgNonrealRefExp *member_ref = buildNonrealRefExpFromNestedNameSpecifier(
      structuralQualifierForExplicitlyQualifiedNonrealRef(
          cxx_dependent_scope_member_expr->getQualifier()),
      current_scope, SgName(member_name),
      cxx_dependent_scope_member_expr->hasTemplateKeyword(), template_args_ptr);
  ROSE_ASSERT(member_ref != nullptr);
  clang::NestedNameSpecifierLoc qualifier_loc =
      cxx_dependent_scope_member_expr->getQualifierLoc();
  const clang::NestedNameSpecifierLoc *qualifier_loc_ptr =
      qualifier_loc.getNestedNameSpecifier() ? &qualifier_loc : nullptr;
  attachExplicitQualifierFromNestedName(
      member_ref, cxx_dependent_scope_member_expr->getQualifier(),
      qualifier_loc_ptr, p_compiler_instance);

  if (base_expr != nullptr) {
    if (cxx_dependent_scope_member_expr->isArrow()) {
      *node = SageBuilder::buildArrowExp(base_expr, member_ref);
    } else {
      *node = SageBuilder::buildDotExp(base_expr, member_ref);
    }
  } else {
    *node = member_ref;
  }

  // Set source position
  SgExpression *expr = isSgExpression(*node);
  if (expr != nullptr) {
    applySourceRange(expr, cxx_dependent_scope_member_expr->getSourceRange());
  }

  return VisitExpr(cxx_dependent_scope_member_expr, node) && res;
}

bool ClangToSageTranslator::VisitCXXFoldExpr(clang::CXXFoldExpr *cxx_fold_expr,
                                             SgNode **node) {
#if DEBUG_VISIT_STMT
  std::cerr << "ClangToSageTranslator::VisitCXXFoldExpr" << std::endl;
#endif
  bool res = true;

  // CXXFoldExpr represents C++17 fold expressions like (... && args)
  clang::Expr *lhs = cxx_fold_expr->getLHS();
  clang::Expr *rhs = cxx_fold_expr->getRHS();
  clang::Expr *pattern = cxx_fold_expr->getPattern();

  SgExpression *lhs_expr = nullptr;
  if (lhs != nullptr) {
    lhs_expr = isSgExpression(Traverse(lhs));
  }

  SgExpression *rhs_expr = nullptr;
  if (rhs != nullptr) {
    rhs_expr = isSgExpression(Traverse(rhs));
  }

  SgExpression *pattern_expr = nullptr;
  if (pattern != nullptr) {
    pattern_expr = isSgExpression(Traverse(pattern));
  }

  SgExpression *operands = nullptr;
  if (lhs_expr != nullptr && rhs_expr != nullptr) {
    SgExprListExp *list = SageBuilder::buildExprListExp();
    list->append_expression(lhs_expr);
    list->append_expression(rhs_expr);
    operands = list;
  } else if (lhs_expr != nullptr) {
    operands = lhs_expr;
  } else if (rhs_expr != nullptr) {
    operands = rhs_expr;
  } else if (pattern_expr != nullptr) {
    operands = pattern_expr;
  }

  ROSE_ASSERT(operands != nullptr);

  clang::StringRef op_spelling =
      clang::BinaryOperator::getOpcodeStr(cxx_fold_expr->getOperator());
  std::string op_token = op_spelling.str();

  SgFoldExpression *fold_expr = SageBuilder::buildFoldExpression_nfi(
      operands, op_token, cxx_fold_expr->isLeftFold());
  if (operands->get_parent() == nullptr) {
    operands->set_parent(fold_expr);
  }
  *node = fold_expr;

  return VisitExpr(cxx_fold_expr, node) && res;
}

bool ClangToSageTranslator::VisitCXXInheritedCtorInitExpr(
    clang::CXXInheritedCtorInitExpr *cxx_inherited_ctor_init_expr,
    SgNode **node) {
#if DEBUG_VISIT_STMT
  std::cerr << "ClangToSageTranslator::VisitCXXInheritedCtorInitExpr"
            << std::endl;
#endif
  bool res = true;

  // TODO

  return VisitExpr(cxx_inherited_ctor_init_expr, node) && res;
}

bool ClangToSageTranslator::VisitCXXNewExpr(clang::CXXNewExpr *cxx_new_expr,
                                            SgNode **node) {
#if DEBUG_VISIT_STMT
  std::cerr << "ClangToSageTranslator::VisitCXXNewExpr" << std::endl;
#endif
  bool res = true;
  clang::QualType allocatedType = cxx_new_expr->getAllocatedType();
  SgType *allocated_sg_type = buildTypeFromQualifiedType(allocatedType);
  SgType *new_expr_type = allocated_sg_type;

  auto classTypeUnknown = [allocated_sg_type]() {
    return allocated_sg_type == nullptr ||
           (isSgTypedefType(allocated_sg_type) == nullptr &&
            isSgClassType(allocated_sg_type) == nullptr);
  };

  SgExprListExp *placementArgs = nullptr;
  if (cxx_new_expr->getNumPlacementArgs() > 0) {
    placementArgs = SageBuilder::buildExprListExp();
    for (clang::Expr *placementArg : cxx_new_expr->placement_arguments()) {
      SgNode *tmpArg = Traverse(placementArg);
      if (SgExpression *expr = isSgExpression(tmpArg)) {
        placementArgs->append_expression(expr);
      }
    }
  }

  SgConstructorInitializer *constructor_args = nullptr;
  if (const clang::CXXConstructExpr *construct_expr =
          cxx_new_expr->getConstructExpr()) {
    SgNode *constructorInitializer =
        Traverse(const_cast<clang::CXXConstructExpr *>(construct_expr));
    constructor_args = isSgConstructorInitializer(constructorInitializer);
  }

  if (constructor_args == nullptr && cxx_new_expr->hasInitializer()) {
    clang::Expr *initializer = cxx_new_expr->getInitializer();
    if (initializer != nullptr) {
      SgNode *translated_initializer = Traverse(initializer);
      if (SgConstructorInitializer *ctor_init =
              isSgConstructorInitializer(translated_initializer)) {
        constructor_args = ctor_init;
      } else if (SgExpression *expr = isSgExpression(translated_initializer)) {
        SgExprListExp *args = isSgExprListExp(expr);
        if (args == nullptr) {
          args = SageBuilder::buildExprListExp_nfi();
          args->append_expression(expr);
        }

        constructor_args = SageBuilder::buildConstructorInitializer_nfi(
            nullptr, args, allocated_sg_type,
            false,             // need_name
            false,             // need_qualifier
            false,             // need_parenthesis_after_name
            classTypeUnknown() // associated_class_unknown
        );
        constructor_args->set_is_braced_initialized(
            llvm::isa<clang::InitListExpr>(initializer) ||
            llvm::isa<clang::CXXStdInitializerListExpr>(initializer));
        applySourceRange(constructor_args, initializer->getSourceRange());
      } else if (translated_initializer != nullptr) {
        std::cerr << "Runtime error: CXXNewExpr initializer did not translate "
                     "into SgExpression."
                  << std::endl;
        initializer->dump();
        ROSE_ABORT();
      }
    }
  }

  if (cxx_new_expr->isArray()) {
    SgExpression *array_size = nullptr;
    if (const std::optional<clang::Expr *> size_expr_opt =
            cxx_new_expr->getArraySize()) {
      if (clang::Expr *size_expr = *size_expr_opt) {
        SgNode *translated_size = Traverse(size_expr);
        array_size = isSgExpression(translated_size);
        ROSE_ASSERT(array_size != nullptr);
      }
    }

    new_expr_type = SageBuilder::buildArrayType(allocated_sg_type, array_size);
  }

  SgNode *clangFuncDecl = Traverse(cxx_new_expr->getOperatorNew());
  if (constructor_args != nullptr) {
    // (4/28/23 Pei-Hung) The type name is given through sg_type,
    // SgConstructorInitializer doesn't seem to provide name for unparsing.
    constructor_args->set_need_name(false);
    if (const clang::CXXConstructExpr *construct_expr =
            cxx_new_expr->getConstructExpr()) {
      clangFuncDecl = Traverse(construct_expr->getConstructor());
    }
  }
  SgFunctionDeclaration *sgFuncDecl = isSgFunctionDeclaration(clangFuncDecl);

  SgExpression *builtin_args = nullptr;
  short int need_global_specifier = (short int)cxx_new_expr->isGlobalNew();

  SgNewExp *newExp =
      SageBuilder::buildNewExp(new_expr_type, placementArgs, constructor_args,
                               builtin_args, need_global_specifier, sgFuncDecl);
  *node = newExp;

  return VisitExpr(cxx_new_expr, node) && res;
}

bool ClangToSageTranslator::VisitCXXNoexceptExpr(
    clang::CXXNoexceptExpr *cxx_noexcept_expr, SgNode **node) {
#if DEBUG_VISIT_STMT
  std::cerr << "ClangToSageTranslator::VisitCXXNoexceptExpr" << std::endl;
#endif
  bool res = true;

  SgExpression *operand = nullptr;
  if (clang::Expr *arg = cxx_noexcept_expr->getOperand()) {
    SgNode *tmp = Traverse(arg);
    operand = isSgExpression(tmp);
  }
  if (operand == nullptr) {
    operand = buildFallbackExpression(cxx_noexcept_expr);
  }

  SgNoexceptOp *noexcept_op = SageBuilder::buildNoexceptOp_nfi(operand);
  if (operand->get_parent() == nullptr) {
    operand->set_parent(noexcept_op);
  }
  *node = noexcept_op;

  if (SgExpression *expr = isSgExpression(*node)) {
    applySourceRange(expr, cxx_noexcept_expr->getSourceRange());
  }

  return VisitExpr(cxx_noexcept_expr, node) && res;
}

bool ClangToSageTranslator::VisitRequiresExpr(
    clang::RequiresExpr *requires_expr, SgNode **node) {
#if DEBUG_VISIT_STMT
  std::cerr << "ClangToSageTranslator::VisitRequiresExpr" << std::endl;
#endif
  bool res = true;

  std::string text = getSourceText(requires_expr->getSourceRange());
  if (text.empty()) {
    text = "requires";
  }
  SgRequiresExpr *req_expr = SageBuilder::buildRequiresExpr_nfi(text);
  *node = req_expr;

  if (SgExpression *expr = isSgExpression(*node)) {
    applySourceRange(expr, requires_expr->getSourceRange());
  }

  return VisitExpr(requires_expr, node) && res;
}

bool ClangToSageTranslator::VisitCXXNullPtrLiteralExpr(
    clang::CXXNullPtrLiteralExpr *cxx_null_ptr_literal_expr, SgNode **node) {
#if DEBUG_VISIT_STMT
  std::cerr << "ClangToSageTranslator::VisitCXXNullPtrLiteralExpr" << std::endl;
#endif
  bool res = true;

  // ROOT CAUSE FIX: CXXNullPtrLiteralExpr represents C++11's nullptr literal
  // nullptr is a null pointer constant of type std::nullptr_t
  // In SAGE, we represent it with SgNullptrValExp

  // Create the nullptr value expression without forcing a nonreal
  // std::nullptr_t declaration.
  *node = SageBuilder::buildNullptrValExp();

  return VisitExpr(cxx_null_ptr_literal_expr, node) && res;
}

bool ClangToSageTranslator::VisitCXXPseudoDestructorExpr(
    clang::CXXPseudoDestructorExpr *cxx_pseudo_destructor_expr, SgNode **node) {
#if DEBUG_VISIT_STMT
  std::cerr << "ClangToSageTranslator::VisitCXXPseudoDestructorExpr"
            << std::endl;
#endif
  bool res = true;

  // Clang models pseudo-destructor calls as
  // CallExpr(callee=CXXPseudoDestructorExpr). Build the callee structurally so
  // the enclosing SgFunctionCallExp can emit the call parentheses without
  // duplication.

  SgNode *tmp_base = Traverse(cxx_pseudo_destructor_expr->getBase());
  SgExpression *base = isSgExpression(tmp_base);
  ROSE_ASSERT(base != nullptr);

  SgType *destroyed_type = nullptr;
  if (clang::TypeSourceInfo *type_info =
          cxx_pseudo_destructor_expr->getDestroyedTypeInfo()) {
    destroyed_type = buildTypeFromQualifiedType(type_info->getType());
  } else {
    if (const clang::IdentifierInfo *id =
            cxx_pseudo_destructor_expr->getDestroyedTypeIdentifier()) {
      destroyed_type =
          SageBuilder::buildTemplateType(SgName(id->getName().str()));
    } else {
      clang::QualType qual_type =
          cxx_pseudo_destructor_expr->getDestroyedType();
      if (!qual_type.isNull()) {
        destroyed_type = buildTypeFromQualifiedType(qual_type);
      }
    }
  }
  ROSE_ASSERT(destroyed_type != nullptr);

  Sg_File_Info *file_info =
      Sg_File_Info::generateDefaultFileInfoForCompilerGeneratedNode();
  ROSE_ASSERT(file_info != nullptr);

  SgPseudoDestructorRefExp *pseudo_dtor =
      new SgPseudoDestructorRefExp(file_info, destroyed_type);
  ROSE_ASSERT(pseudo_dtor != nullptr);
  pseudo_dtor->post_construction_initialization();

  clang::SourceLocation tilde_loc = cxx_pseudo_destructor_expr->getTildeLoc();
  clang::SourceLocation name_end_loc;
  if (clang::TypeSourceInfo *type_info =
          cxx_pseudo_destructor_expr->getDestroyedTypeInfo()) {
    name_end_loc = type_info->getTypeLoc().getEndLoc();
  }
  if (tilde_loc.isValid() && name_end_loc.isValid()) {
    applySourceRange(pseudo_dtor, clang::SourceRange(tilde_loc, name_end_loc));
  } else {
    applySourceRange(pseudo_dtor, cxx_pseudo_destructor_expr->getSourceRange());
  }

  SgExpression *callee = nullptr;
  if (cxx_pseudo_destructor_expr->isArrow()) {
    callee =
        SageBuilder::buildBinaryExpression_nfi<SgArrowExp>(base, pseudo_dtor);
  } else {
    callee =
        SageBuilder::buildBinaryExpression_nfi<SgDotExp>(base, pseudo_dtor);
  }
  ROSE_ASSERT(callee != nullptr);
  applySourceRange(callee, cxx_pseudo_destructor_expr->getSourceRange());

  *node = callee;

  return VisitExpr(cxx_pseudo_destructor_expr, node) && res;
}

bool ClangToSageTranslator::VisitCXXRewrittenBinaryOperator(
    clang::CXXRewrittenBinaryOperator *cxx_rewrite_binary_operator,
    SgNode **node) {
#if DEBUG_VISIT_STMT
  std::cerr << "ClangToSageTranslator::VisitCXXRewrittenBinaryOperator"
            << std::endl;
#endif
  bool res = true;

  clang::Expr *semantic_form = cxx_rewrite_binary_operator->getSemanticForm();
  if (semantic_form == nullptr) {
    MLOG_ERROR_C(MLOG_FRONTEND,
                 "Runtime error: CXXRewrittenBinaryOperator has no semantic "
                 "form.\n");
    return false;
  }

  SgNode *tmp_expr = Traverse(semantic_form);
  SgExpression *expr = isSgExpression(tmp_expr);
  if (expr == nullptr) {
    MLOG_ERROR_C(MLOG_FRONTEND,
                 "Runtime error: failed to translate semantic form for "
                 "CXXRewrittenBinaryOperator.\n");
    return false;
  }

  *node = expr;
  applySourceRange(expr, cxx_rewrite_binary_operator->getSourceRange());

  return VisitExpr(cxx_rewrite_binary_operator, node) && res;
}

bool ClangToSageTranslator::VisitCXXScalarValueInitExpr(
    clang::CXXScalarValueInitExpr *cxx_scalar_value_init_expr, SgNode **node) {
#if DEBUG_VISIT_STMT
  std::cerr << "ClangToSageTranslator::VisitCXXScalarValueInitExpr"
            << std::endl;
#endif
  bool res = true;

  // ROOT CAUSE FIX: CXXScalarValueInitExpr represents value initialization of
  // scalar types Examples: int(), char(), double*() - all initialize to
  // zero/null This is equivalent to a cast expression with default
  // initialization

  // Get the type being initialized
  SgType *type =
      buildTypeFromQualifiedType(cxx_scalar_value_init_expr->getType());

  if (type == nullptr) {
    type = SageBuilder::buildVoidType();
  }

  // Create a cast expression with a null operand to represent the default
  // initialization The cast will produce the zero-initialized value of the
  // target type
  *node = SageBuilder::buildCastExp(SageBuilder::buildIntVal(0), type);

  return VisitExpr(cxx_scalar_value_init_expr, node) && res;
}

bool ClangToSageTranslator::VisitCXXStdInitializerListExpr(
    clang::CXXStdInitializerListExpr *cxx_std_initializer_list_expr,
    SgNode **node) {
#if DEBUG_VISIT_STMT
  std::cerr << "ClangToSageTranslator::VisitCXXStdInitializerListExpr"
            << std::endl;
#endif
  bool res = true;

  clang::Expr *sub_expr = cxx_std_initializer_list_expr->getSubExpr();
  if (sub_expr == nullptr) {
    std::cerr << "Runtime error: CXXStdInitializerListExpr has no subexpr."
              << std::endl;
    return false;
  }

  SgNode *tmp_sub = Traverse(sub_expr);
  if (tmp_sub == nullptr) {
    std::cerr << "Runtime error: failed to translate CXXStdInitializerListExpr "
                 "subexpr."
              << std::endl;
    return false;
  }

  SgExprListExp *expr_list = isSgExprListExp(tmp_sub);
  if (expr_list == nullptr) {
    if (SgAggregateInitializer *agg_init = isSgAggregateInitializer(tmp_sub)) {
      *node = agg_init;
      return VisitExpr(cxx_std_initializer_list_expr, node) && res;
    }

    SgExpression *expr = isSgExpression(tmp_sub);
    if (expr == nullptr) {
      std::cerr << "Runtime error: CXXStdInitializerListExpr subexpr is not an "
                   "expression."
                << std::endl;
      return false;
    }

    expr_list = SageBuilder::buildExprListExp_nfi();
    expr_list->append_expression(expr);
  }

  SgType *init_list_type =
      buildTypeFromQualifiedType(cxx_std_initializer_list_expr->getType());
  SgAggregateInitializer *agg_init =
      SageBuilder::buildAggregateInitializer_nfi(expr_list, init_list_type);
  applySourceRange(agg_init, cxx_std_initializer_list_expr->getSourceRange());
  *node = agg_init;

  return VisitExpr(cxx_std_initializer_list_expr, node) && res;
}

bool ClangToSageTranslator::VisitCXXThisExpr(clang::CXXThisExpr *cxx_this_expr,
                                             SgNode **node) {
#if DEBUG_VISIT_STMT
  std::cerr << "ClangToSageTranslator::VisitCXXThisExpr" << std::endl;
#endif
  bool res = true;

  SgSymbol *this_symbol = findEnclosingThisSymbol(SageBuilder::topScopeStack());
  if (this_symbol == nullptr) {
    const clang::Type *this_type_ptr =
        cxx_this_expr->getType().getTypePtrOrNull();
    const clang::PointerType *pointer_type =
        this_type_ptr != nullptr ? this_type_ptr->getAs<clang::PointerType>()
                                 : nullptr;
    const clang::CXXRecordDecl *record_decl = nullptr;
    if (pointer_type != nullptr) {
      record_decl = pointer_type->getPointeeType()->getAsCXXRecordDecl();
    }

    if (record_decl != nullptr) {
      const clang::CXXRecordDecl *canonical_decl =
          record_decl->getCanonicalDecl();
      auto it = p_decl_translation_map.find(
          const_cast<clang::CXXRecordDecl *>(canonical_decl));
      if (it == p_decl_translation_map.end()) {
        it = p_decl_translation_map.find(
            const_cast<clang::CXXRecordDecl *>(record_decl));
      }

      SgClassDeclaration *class_decl = nullptr;
      if (it != p_decl_translation_map.end()) {
        class_decl = isSgClassDeclaration(it->second);
      } else {
        SgNode *tmp_class =
            Traverse(const_cast<clang::CXXRecordDecl *>(canonical_decl));
        class_decl = isSgClassDeclaration(tmp_class);
      }

      if (class_decl != nullptr && class_decl->get_scope() != nullptr) {
        SgScopeStatement *decl_scope = class_decl->get_scope();
        SgName class_name = class_decl->get_name();

        // Prefer the symbol already associated with the declaration. This
        // preserves correct symbol kinds (e.g., SgTemplateClassSymbol for
        // SgTemplateClassDeclaration) and avoids inserting invalid
        // SgClassSymbol entries that trip AstConsistencyTests.
        SgSymbol *class_sym = class_decl->get_symbol_from_symbol_table();
        if (class_sym == nullptr) {
          class_sym = decl_scope->lookup_symbol(class_name);
        }

        if (class_sym == nullptr) {
          if (SgTemplateClassDeclaration *tmpl_decl =
                  isSgTemplateClassDeclaration(class_decl)) {
            class_sym = new SgTemplateClassSymbol(tmpl_decl);
          } else {
            class_sym = new SgClassSymbol(class_decl);
          }
          decl_scope->insert_symbol(class_name, class_sym);
        }

        this_symbol = class_sym;
      }
    }
  }

  ROSE_ASSERT(this_symbol != nullptr);

  SgThisExp *this_exp = SageBuilder::buildThisExp_nfi(this_symbol);
  *node = this_exp;

  return VisitExpr(cxx_this_expr, node) && res;
}

bool ClangToSageTranslator::VisitCXXThrowExpr(
    clang::CXXThrowExpr *cxx_throw_expr, SgNode **node) {
#if DEBUG_VISIT_STMT
  std::cerr << "ClangToSageTranslator::VisitCXXThrowExpr" << std::endl;
#endif
  bool res = true;

  // ROOT CAUSE FIX: CXXThrowExpr represents C++ throw expressions
  // Can be either "throw expr;" or bare "throw;" (rethrow)
  SgExpression *throw_operand = nullptr;
  SgThrowOp::e_throw_kind throw_kind;

  // Check if this is a rethrow (bare "throw;") or throw with expression
  clang::Expr *sub_expr = cxx_throw_expr->getSubExpr();
  if (sub_expr != nullptr) {
    // Regular throw with an expression
    SgNode *tmp_expr = Traverse(sub_expr);
    throw_operand = isSgExpression(tmp_expr);
    if (throw_operand == nullptr) {
      std::cerr << "Error: Failed to convert throw operand expression"
                << std::endl;
      return false;
    }
    throw_kind = SgThrowOp::throw_expression;
  } else {
    // Rethrow (bare "throw;")
    throw_kind = SgThrowOp::rethrow;
  }

  // Build the throw operation
  SgThrowOp *throw_op = SageBuilder::buildThrowOp(throw_operand, throw_kind);
  ROSE_ASSERT(throw_op != nullptr);

  *node = throw_op;

  return VisitExpr(cxx_throw_expr, node) && res;
}

bool ClangToSageTranslator::VisitCXXTypeidExpr(
    clang::CXXTypeidExpr *cxx_typeid_expr, SgNode **node) {
#if DEBUG_VISIT_STMT
  std::cerr << "ClangToSageTranslator::VisitCXXTypeidExpr" << std::endl;
#endif
  bool res = true;

  SgType *expression_type =
      buildTypeFromQualifiedType(cxx_typeid_expr->getType());
  SgReferenceType *ref_type = isSgReferenceType(expression_type);
  SgType *base_type =
      (ref_type != nullptr) ? ref_type->get_base_type() : expression_type;
  if (!SageInterface::isConstType(base_type)) {
    base_type = SageBuilder::buildConstType(base_type);
  }
  if (ref_type == nullptr || base_type != ref_type->get_base_type()) {
    expression_type = SageBuilder::buildReferenceType(base_type);
  }

  if (cxx_typeid_expr->isTypeOperand()) {
    SgType *type = nullptr;
    if (p_compiler_instance != nullptr) {
      type = buildTypeFromQualifiedType(cxx_typeid_expr->getTypeOperand(
          p_compiler_instance->getASTContext()));
    }
    if (type == nullptr) {
      type = SageBuilder::buildUnknownType();
    }
    *node = SageBuilder::buildTypeIdOp(nullptr, type, expression_type);
  } else {
    SgNode *tmp_expr = Traverse(cxx_typeid_expr->getExprOperand());
    SgExpression *expr = isSgExpression(tmp_expr);
    if (expr == nullptr) {
      expr = buildFallbackExpression(cxx_typeid_expr->getExprOperand());
    }
    SgTypeIdOp *typeid_op =
        SageBuilder::buildTypeIdOp(expr, nullptr, expression_type);
    expr->set_parent(typeid_op);
    *node = typeid_op;
  }

  return VisitExpr(cxx_typeid_expr, node) && res;
}

bool ClangToSageTranslator::VisitCXXUnresolvedConstructExpr(
    clang::CXXUnresolvedConstructExpr *cxx_unresolved_construct_expr,
    SgNode **node) {
#if DEBUG_VISIT_STMT
  std::cerr << "ClangToSageTranslator::VisitCXXUnresolvedConstructExpr"
            << std::endl;
#endif
  bool res = true;

  // Template-dependent constructor calls (e.g., T(args) where T is a template
  // parameter) should remain constructor initializers instead of null
  // placeholders.
  SgType *type = buildTypeFromQualifiedType(
      cxx_unresolved_construct_expr->getTypeAsWritten());

  auto scope_contains_declaration = [](SgScopeStatement *scope,
                                       SgDeclarationStatement *decl) -> bool {
    if (scope == nullptr || decl == nullptr) {
      return false;
    }

    for (SgNode *cursor = scope; cursor != nullptr;
         cursor = cursor->get_parent()) {
      if (cursor == decl) {
        return true;
      }

      if (SgClassDeclaration *class_decl = isSgClassDeclaration(decl)) {
        if (SgClassDefinition *class_def = class_decl->get_definition()) {
          if (cursor == class_def) {
            return true;
          }
        }
      }

      if (SgTemplateClassDeclaration *tmpl_decl =
              isSgTemplateClassDeclaration(decl)) {
        if (SgClassDefinition *tmpl_def = tmpl_decl->get_definition()) {
          if (cursor == tmpl_def) {
            return true;
          }
        }
      }

      if (SgTemplateInstantiationDecl *inst_decl =
              isSgTemplateInstantiationDecl(decl)) {
        if (SgClassDefinition *inst_def = inst_decl->get_definition()) {
          if (cursor == inst_def) {
            return true;
          }
        }
      }
    }

    return false;
  };

  clang::QualType written_type_qt =
      cxx_unresolved_construct_expr->getTypeAsWritten();
  bool has_explicit_qualifier = false;
  const clang::Type *written_type_ptr = written_type_qt.getTypePtrOrNull();
  has_explicit_qualifier = qualifiedTypeHasQualifier(written_type_ptr);

  // For unqualified class-member typedef constructors, preserve the
  // source-level spelling to avoid later over-qualification
  // (e.g., `::std::vector::const_iterator(...)`).
  if (!has_explicit_qualifier) {
    if (const clang::TypedefType *written_typedef =
            llvm::dyn_cast_or_null<clang::TypedefType>(written_type_ptr)) {
      clang::TypedefNameDecl *typedef_decl = written_typedef->getDecl();
      if (typedef_decl != nullptr) {
        std::string typedef_name = typedef_decl->getNameAsString();
        if (!typedef_name.empty()) {
          clang::DeclContext *decl_ctx = typedef_decl->getDeclContext();
          clang::CXXRecordDecl *record_ctx =
              llvm::dyn_cast_or_null<clang::CXXRecordDecl>(decl_ctx);
          if (record_ctx != nullptr) {
            SgDeclarationStatement *sg_record_decl =
                lookupSgDeclarationForClangDecl(record_ctx,
                                                /*allow_on_demand=*/true);
            if (sg_record_decl == nullptr) {
              sg_record_decl = lookupSgDeclarationForClangDecl(
                  record_ctx->getCanonicalDecl(),
                  /*allow_on_demand=*/true);
            }
            if (scope_contains_declaration(SageBuilder::topScopeStack(),
                                           sg_record_decl)) {
              SgScopeStatement *scope = SageBuilder::topScopeStack();
              if (scope == nullptr) {
                scope = getGlobalScope();
              }
              type = SageBuilder::buildNonrealType(SgName(typedef_name), scope,
                                                   nullptr);
            }
          }
        }
      }
    }
  }

  // Build expression list for constructor arguments.
  SgExprListExp *args = SageBuilder::buildExprListExp_nfi();
  for (unsigned i = 0; i < cxx_unresolved_construct_expr->getNumArgs(); i++) {
    SgNode *tmp_expr = Traverse(cxx_unresolved_construct_expr->getArg(i));
    SgExpression *arg = isSgExpression(tmp_expr);
    if (arg != nullptr) {
      args->append_expression(arg);
    }
  }

  bool class_unknown = false;
  if (type != nullptr) {
    if (isSgTypedefType(type) == nullptr && isSgClassType(type) == nullptr) {
      class_unknown = true;
    }
  } else {
    class_unknown = true;
  }

  SgConstructorInitializer *ctor_init =
      SageBuilder::buildConstructorInitializer_nfi(
          nullptr, // declaration will be nullptr for unresolved/dependent
                   // constructors
          args, type,
          true,         // need_name
          false,        // need_qualifier
          false,        // need_parenthesis_after_name
          class_unknown // associated_class_unknown for non class-like types
      );

  // Unresolved constructor expressions are source-level cast-like syntax
  // (`T(args)` / `T{args}`) and must keep the type name when unparsing.
  ctor_init->set_is_explicit_cast(true);

  // Preserve list-initialization form for unresolved constructor expressions
  // so template definitions keep braces where required (e.g., aggregate
  // initialization through dependent base packs in C++17).
  ctor_init->set_is_braced_initialized(
      cxx_unresolved_construct_expr->isListInitialization());

  *node = ctor_init;

  return VisitExpr(cxx_unresolved_construct_expr, node) && res;
}

bool ClangToSageTranslator::VisitCXXUuidofExpr(
    clang::CXXUuidofExpr *cxx_uuidof_expr, SgNode **node) {
#if DEBUG_VISIT_STMT
  std::cerr << "ClangToSageTranslator::VisitCXXUuidofExpr" << std::endl;
#endif
  bool res = true;

  // TODO

  return VisitExpr(cxx_uuidof_expr, node) && res;
}

bool ClangToSageTranslator::VisitDeclRefExpr(clang::DeclRefExpr *decl_ref_expr,
                                             SgNode **node) {
  if (clang::NonTypeTemplateParmDecl *non_type_param =
          llvm::dyn_cast<clang::NonTypeTemplateParmDecl>(
              decl_ref_expr->getDecl())) {
    SgDeclarationStatement *owning_template = nullptr;
    if (clang::DeclContext *ctx = non_type_param->getDeclContext()) {
      if (clang::TemplateDecl *template_ctx =
              llvm::dyn_cast<clang::TemplateDecl>(ctx)) {
        auto it = p_decl_translation_map.find(template_ctx);
        if (it != p_decl_translation_map.end()) {
          owning_template = isSgDeclarationStatement(it->second);
        }
      }
    }
    if (owning_template == nullptr) {
      SgScopeStatement *scope = SageBuilder::topScopeStack();
      if (scope != nullptr) {
        SgFunctionDeclaration *enclosing_function =
            SageInterface::getEnclosingNode<SgFunctionDeclaration>(scope, true);
        if (enclosing_function != nullptr) {
          if (SgFunctionDeclaration *first_nondef = isSgFunctionDeclaration(
                  enclosing_function->get_firstNondefiningDeclaration())) {
            enclosing_function = first_nondef;
          }
          SgScopeStatement *function_scope = enclosing_function->get_scope();
          if (SgClassDefinition *class_def =
                  isSgClassDefinition(function_scope)) {
            owning_template =
                isSgTemplateClassDeclaration(class_def->get_declaration());
          } else if (SgTemplateClassDefinition *template_def =
                         isSgTemplateClassDefinition(function_scope)) {
            owning_template =
                isSgTemplateClassDeclaration(template_def->get_declaration());
          }
        }
      }
    }

    unsigned position = non_type_param->getIndex();
    SgTemplateParameter *sg_param =
        translateTemplateParameter(non_type_param, owning_template, position);

    if (sg_param != nullptr) {
      SgTemplateParameterVal *param_val =
          SageBuilder::buildTemplateParameterVal(non_type_param->getIndex());
      std::string canonical_name;
      if (SgInitializedName *init_name = sg_param->get_initializedName()) {
        canonical_name = init_name->get_name().getString();
      } else if (SgTemplateType *template_type =
                     isSgTemplateType(sg_param->get_type())) {
        canonical_name = template_type->get_name().getString();
      } else if (SgNonrealType *nonreal_type =
                     isSgNonrealType(sg_param->get_type())) {
        canonical_name = nonreal_type->get_name().getString();
      }
      if (const clang::Decl *canonical_decl =
              non_type_param->getCanonicalDecl()) {
        if (canonical_name.empty()) {
          if (const clang::NonTypeTemplateParmDecl *canonical_nttp =
                  llvm::dyn_cast<clang::NonTypeTemplateParmDecl>(
                      canonical_decl)) {
            canonical_name = normalizeClangTemplateParamName(
                canonical_nttp->getNameAsString());
          }
        }
      }
      if (canonical_name.empty()) {
        canonical_name =
            normalizeClangTemplateParamName(non_type_param->getNameAsString());
      }
      param_val->set_valueString(canonical_name);
      if (sg_param->get_type() != nullptr) {
        param_val->set_valueType(sg_param->get_type());
      }
      applySourceRange(param_val, decl_ref_expr->getSourceRange());
      *node = param_val;
      return true;
    }
  }

  bool res = true;

  clang::NestedNameSpecifierLoc decl_ref_qualifier_loc =
      decl_ref_expr->getQualifierLoc();
  const clang::NestedNameSpecifierLoc *decl_ref_qualifier_loc_ptr =
      decl_ref_qualifier_loc.getNestedNameSpecifier() ? &decl_ref_qualifier_loc
                                                      : nullptr;
  clang::NestedNameSpecifier decl_ref_qualifier = decl_ref_expr->getQualifier();
  if (!decl_ref_qualifier && decl_ref_qualifier_loc_ptr != nullptr) {
    decl_ref_qualifier = decl_ref_qualifier_loc_ptr->getNestedNameSpecifier();
  }

  auto qualifier_has_type_component =
      [](clang::NestedNameSpecifier qualifier) -> bool {
    for (clang::NestedNameSpecifier nns = qualifier; nns;
         nns = nestedNameSpecifierPrefix(nns)) {
      switch (nns.getKind()) {
      case clang::NestedNameSpecifier::Kind::Type:
      case clang::NestedNameSpecifier::Kind::MicrosoftSuper:
        return true;
      default:
        break;
      }
    }
    return false;
  };

  bool force_global_scope = false;
  if (decl_ref_qualifier && decl_ref_qualifier.getKind() ==
                                clang::NestedNameSpecifier::Kind::Global) {
    force_global_scope = true;
  }
  auto ensure_global_function_decl_symbol =
      [&](clang::FunctionDecl *clang_func) {
        if (clang_func == nullptr) {
          return;
        }
        SgDeclarationStatement *decl_stmt =
            lookupSgDeclarationForClangDecl(clang_func,
                                            /*allow_on_demand=*/true);
        SgFunctionDeclaration *func_decl = isSgFunctionDeclaration(decl_stmt);
        if (func_decl == nullptr) {
          return;
        }
        SgScopeStatement *global_scope =
            SageInterface::getGlobalScope(SageBuilder::topScopeStack());
        if (global_scope != nullptr) {
          func_decl->set_scope(global_scope);
          if (func_decl->get_parent() == nullptr) {
            func_decl->set_parent(global_scope);
          }
        }
        if (func_decl->get_firstNondefiningDeclaration() == nullptr) {
          func_decl->set_firstNondefiningDeclaration(func_decl);
        }
        SgFunctionDeclaration *symbol_decl = func_decl;
        if (SgFunctionDeclaration *first_nondef = isSgFunctionDeclaration(
                func_decl->get_firstNondefiningDeclaration())) {
          symbol_decl = first_nondef;
          if (symbol_decl->get_scope() == nullptr && global_scope != nullptr) {
            symbol_decl->set_scope(global_scope);
          }
          if (symbol_decl->get_parent() == nullptr && global_scope != nullptr) {
            symbol_decl->set_parent(global_scope);
          }
        }
        if (global_scope != nullptr && symbol_decl != nullptr) {
          SgFunctionSymbol *existing_sym =
              global_scope->lookup_nontemplate_function_symbol(
                  symbol_decl->get_name(), symbol_decl->get_type(), nullptr);
          if (existing_sym == nullptr) {
            if (SgSymbol *any_sym = global_scope->lookup_function_symbol(
                    symbol_decl->get_name(), symbol_decl->get_type())) {
              existing_sym = isSgFunctionSymbol(any_sym);
            }
          }
          if (existing_sym == nullptr) {
            SgFunctionSymbol *new_sym = new SgFunctionSymbol(symbol_decl);
            attachSymbolToScopeOrOrphan(new_sym, global_scope);
          } else if (existing_sym->get_declaration() != symbol_decl) {
            // Keep existing symbol but ensure this declaration also has one.
            SgFunctionSymbol *new_sym = new SgFunctionSymbol(symbol_decl);
            attachSymbolToScopeOrOrphan(new_sym, global_scope);
          }
        } else {
          registerDeclarationSymbol(func_decl);
          if (SgFunctionDeclaration *first_nondef = isSgFunctionDeclaration(
                  func_decl->get_firstNondefiningDeclaration())) {
            registerDeclarationSymbol(first_nondef);
          }
        }
      };
  if (force_global_scope) {
    if (clang::FunctionDecl *clang_func =
            llvm::dyn_cast<clang::FunctionDecl>(decl_ref_expr->getDecl())) {
      ensure_global_function_decl_symbol(clang_func);
    }
  }
  auto ensure_function_decl_symbol = [&](SgFunctionDeclaration *decl) {
    if (decl == nullptr) {
      return;
    }
    auto ensure_decl_scope = [&](SgFunctionDeclaration *candidate) {
      if (candidate == nullptr) {
        return;
      }
      SgScopeStatement *resolved_scope = nullptr;
      if (clang::DeclContext *ctx =
              decl_ref_expr->getDecl()->getDeclContext()) {
        resolved_scope =
            resolveScopeFromDeclContext(ctx, SageBuilder::topScopeStack());
      }
      if (force_global_scope) {
        resolved_scope =
            SageInterface::getGlobalScope(SageBuilder::topScopeStack());
      }
      if (resolved_scope != nullptr) {
        resolved_scope = normalizeNamespaceScope(resolved_scope);
      }
      SgScopeStatement *preferred_scope = resolved_scope;
      if (preferred_scope != nullptr) {
        if (SgScopeStatement *parent_scope =
                isSgScopeStatement(candidate->get_parent())) {
          SgNamespaceDefinitionStatement *parent_ns =
              isSgNamespaceDefinitionStatement(parent_scope);
          SgNamespaceDefinitionStatement *resolved_ns =
              isSgNamespaceDefinitionStatement(preferred_scope);
          if (parent_ns != nullptr && resolved_ns != nullptr) {
            SgNamespaceDeclarationStatement *parent_decl =
                parent_ns->get_namespaceDeclaration();
            SgNamespaceDeclarationStatement *resolved_decl =
                resolved_ns->get_namespaceDeclaration();
            if (parent_decl != nullptr && resolved_decl != nullptr) {
              SgDeclarationStatement *parent_first =
                  parent_decl->get_firstNondefiningDeclaration();
              if (parent_first == nullptr) {
                parent_first = parent_decl;
              }
              SgDeclarationStatement *resolved_first =
                  resolved_decl->get_firstNondefiningDeclaration();
              if (resolved_first == nullptr) {
                resolved_first = resolved_decl;
              }
              if (parent_first == resolved_first) {
                preferred_scope = parent_scope;
              }
            }
          }
        }
      }
      if (preferred_scope != nullptr &&
          candidate->get_scope() != preferred_scope) {
        candidate->set_scope(preferred_scope);
      }
      if (candidate->get_scope() == nullptr) {
        candidate->set_scope(
            SageInterface::getGlobalScope(SageBuilder::topScopeStack()));
      }
      if (candidate->get_parent() == nullptr &&
          candidate->get_scope() != nullptr) {
        candidate->set_parent(candidate->get_scope());
      }
    };
    auto ensure_decl = [&](SgFunctionDeclaration *candidate) {
      if (candidate == nullptr) {
        return;
      }
      ensure_decl_scope(candidate);
      if (candidate->get_firstNondefiningDeclaration() == nullptr) {
        candidate->set_firstNondefiningDeclaration(candidate);
      }
      if (candidate->get_symbol_from_symbol_table() == nullptr ||
          candidate->get_declaration_associated_with_symbol() == nullptr) {
        registerDeclarationSymbol(candidate);
      }
      if (candidate->get_symbol_from_symbol_table() == nullptr) {
        SgScopeStatement *scope = candidate->get_scope();
        if (scope == nullptr) {
          scope = SageInterface::getGlobalScope(SageBuilder::topScopeStack());
          candidate->set_scope(scope);
        }
        if (scope != nullptr) {
          if (SgSymbol *sym = buildSymbolForDeclaration(candidate)) {
            attachSymbolToScopeOrOrphan(sym, scope);
          }
        }
      }
    };
    ensure_decl(decl);
    ensure_decl(
        isSgFunctionDeclaration(decl->get_firstNondefiningDeclaration()));
    ensure_decl(isSgFunctionDeclaration(decl->get_definingDeclaration()));
  };
  auto set_compiler_generated_on_instantiation_chain =
      [&](SgFunctionDeclaration *decl) {
        if (decl == nullptr) {
          return;
        }
        for_each_unique_function_decl_in_chain(
            decl, [&](SgFunctionDeclaration *candidate_decl) {
              Sg_File_Info *file_info = candidate_decl->get_file_info();
              const bool touches_synthetic_candidate =
                  candidate_decl == decl || file_info == nullptr ||
                  file_info->isCompilerGenerated();
              if (!touches_synthetic_candidate) {
                return;
              }

              setCompilerGeneratedFileInfo(candidate_decl);
              if (SgFunctionParameterList *params =
                      candidate_decl->get_parameterList()) {
                setCompilerGeneratedFileInfo(params);
                for (SgInitializedName *param : params->get_args()) {
                  if (param != nullptr) {
                    setCompilerGeneratedFileInfo(param);
                  }
                }
              }
            });
      };
  auto finalize_instantiation_decl = [&](SgFunctionDeclaration *decl) {
    if (decl == nullptr) {
      return;
    }
    mark_synthesized_instantiation_decl_chain_for_ref(decl, decl->get_scope());
    set_compiler_generated_on_instantiation_chain(decl);
  };
  auto attach_explicit_qualifier = [&](SgExpression *expr) {
    if (expr == nullptr || decl_ref_expr == nullptr ||
        p_compiler_instance == nullptr) {
      return;
    }
    if (!decl_ref_qualifier) {
      return;
    }
    const ExplicitQualifierInfo info = getExplicitQualifierInfo(
        decl_ref_qualifier, p_compiler_instance->getASTContext(),
        decl_ref_qualifier_loc_ptr, &p_compiler_instance->getSourceManager(),
        &p_compiler_instance->getLangOpts());
    setExplicitQualifierOnExpr(expr, info);
  };

  // Phase C (Issue 115): Queue implicit template instantiations that are
  // referenced from user code. Translation is deferred until the TU pass
  // completes to avoid ordering issues.
  if (clang::FunctionDecl *func_decl =
          llvm::dyn_cast<clang::FunctionDecl>(decl_ref_expr->getDecl())) {
    clang::TemplateSpecializationKind kind =
        func_decl->getTemplateSpecializationKind();
    if (kind == clang::TSK_ImplicitInstantiation ||
        kind == clang::TSK_ExplicitInstantiationDefinition) {
      bool eligible = true;
      if (p_compiler_instance != nullptr) {
        clang::SourceManager &sm = p_compiler_instance->getSourceManager();
        clang::SourceLocation callee_loc = func_decl->getLocation();
        clang::SourceLocation ref_loc = decl_ref_expr->getExprLoc();
        const bool ref_in_system = !ref_loc.isValid() ||
                                   sm.isInSystemHeader(ref_loc) ||
                                   sm.isWrittenInBuiltinFile(ref_loc);
        if (!callee_loc.isValid()) {
          eligible = false;
        } else if ((sm.isInSystemHeader(callee_loc) ||
                    sm.isWrittenInBuiltinFile(callee_loc)) &&
                   ref_in_system) {
          eligible = false;
        }
      }

      if (eligible &&
          p_pending_implicit_function_instantiations_set.insert(func_decl)
              .second) {
        p_pending_implicit_function_instantiations.push_back(func_decl);
      }
    }
  }

  // SgNode * tmp_node = Traverse(decl_ref_expr->getDecl());
  // DONE: Do not use Traverse(...) as the declaration can not be complete
  // (recursive functions)
  //       Instead use SymbolTable from ROSE as the symbol should be ready
  //       (cannot have a reference before the declaration)
  // FIXME: This fix will not work for C++ (methods/fields can be use before
  // they are declared...)
  // FIXME: I feel like it could work now, we will see ....

  SgSymbol *sym = nullptr;
  if (decl_ref_expr->hasQualifier()) {
    if (clang::NestedNameSpecifier qualifier = decl_ref_expr->getQualifier()) {
      if (clang::NamespaceDecl *namespaceDecl =
              const_cast<clang::NamespaceDecl *>(
                  nestedNameSpecifierNamespace(qualifier))) {
        if (SgNamespaceDeclarationStatement *namespaceDeclStmt =
                ensureNamespaceDeclaration(namespaceDecl)) {
          SgNamespaceDefinitionStatement *namespaceDefinition =
              namespaceDeclStmt->get_definition();
          if (namespaceDefinition != nullptr) {
            SageBuilder::pushScopeStack(namespaceDefinition);
            sym = GetSymbolFromSymbolTable(decl_ref_expr->getDecl());
            SageBuilder::popScopeStack();
          }
        }
      } else if (clang::CXXRecordDecl *cxxRecordDecl =
                     qualifier.getAsRecordDecl()) {
        if (SgClassDeclaration *classDecl =
                isSgClassDeclaration(Traverse(cxxRecordDecl))) {
          // Resolve the class definition for qualified lookup, translating
          // on-demand if needed.
          SgClassDefinition *classDef = classDecl->get_definition();
          if (classDef == nullptr) {
            if (SgClassDeclaration *def_decl = isSgClassDeclaration(
                    classDecl->get_definingDeclaration())) {
              classDef = def_decl->get_definition();
            }
          }
          if (classDef == nullptr) {
            if (clang::CXXRecordDecl *definitionDecl =
                    cxxRecordDecl->getDefinition()) {
              SgNode *def_node = TraverseOnDemand(definitionDecl);
              if (SgClassDeclaration *def_decl =
                      isSgClassDeclaration(def_node)) {
                classDef = def_decl->get_definition();
                if (classDef == nullptr) {
                  if (SgClassDeclaration *defining_decl = isSgClassDeclaration(
                          def_decl->get_definingDeclaration())) {
                    classDef = defining_decl->get_definition();
                  }
                }
              } else if (SgClassDefinition *def_def =
                             isSgClassDefinition(def_node)) {
                classDef = def_def;
              }
            }
          }
          ROSE_ASSERT(classDef != nullptr);
          SageBuilder::pushScopeStack(classDef);
          sym = GetSymbolFromSymbolTable(decl_ref_expr->getDecl());
          SageBuilder::popScopeStack();
        }
      } else if (clang::NamespaceAliasDecl *namespaceAliasDecl =
                     const_cast<clang::NamespaceAliasDecl *>(
                         nestedNameSpecifierNamespaceAlias(qualifier))) {
        if (SgNamespaceAliasDeclarationStatement *namespaceAliasDeclStmt =
                isSgNamespaceAliasDeclarationStatement(
                    Traverse(namespaceAliasDecl))) {
          SgNamespaceDeclarationStatement *namespaceDeclStmt =
              namespaceAliasDeclStmt->get_namespaceDeclaration();
          ROSE_ASSERT(namespaceDeclStmt);
          SgNamespaceDefinitionStatement *namespaceDefinition =
              namespaceDeclStmt->get_definition();
          SageBuilder::pushScopeStack(namespaceDefinition);
          sym = GetSymbolFromSymbolTable(decl_ref_expr->getDecl());
          SageBuilder::popScopeStack();
        }
      } else if (qualifier.getKind() ==
                 clang::NestedNameSpecifier::Kind::Global) {
        SgGlobal *globalScope =
            SageInterface::getGlobalScope(SageBuilder::topScopeStack());
        std::string declName = decl_ref_expr->getDecl()->getNameAsString();
        if (clang::FunctionDecl *clang_func =
                llvm::dyn_cast<clang::FunctionDecl>(decl_ref_expr->getDecl())) {
          if (SgFunctionType *func_type = isSgFunctionType(
                  buildTypeFromQualifiedType(clang_func->getType()))) {
            sym = globalScope->lookup_nontemplate_function_symbol(
                declName, func_type, nullptr);
            if (sym == nullptr) {
              sym = globalScope->lookup_function_symbol(declName, func_type);
            }
          }
          if (sym != nullptr &&
              clang_func->getDescribedFunctionTemplate() == nullptr &&
              clang_func->getPrimaryTemplate() == nullptr) {
            if (isSgTemplateFunctionSymbol(sym) != nullptr ||
                isSgTemplateMemberFunctionSymbol(sym) != nullptr) {
              sym = nullptr;
            }
          }
          if (sym != nullptr && isSgFunctionSymbol(sym) == nullptr &&
              isSgTemplateFunctionSymbol(sym) == nullptr &&
              isSgMemberFunctionSymbol(sym) == nullptr) {
            sym = nullptr;
          }
        } else {
          sym = globalScope->lookup_symbol(declName);
        }
        // Global lookup can miss symbols for templated or deferred decls;
        // fall back to later resolution logic.
      }
    }
  }

  if (clang::VarDecl *var_decl =
          llvm::dyn_cast<clang::VarDecl>(decl_ref_expr->getDecl())) {
    if (var_decl->isStaticDataMember()) {
      auto resolve_static_member_scope = [&]() -> SgScopeStatement * {
        clang::DeclContext *ctx = var_decl->getDeclContext();
        if (ctx == nullptr) {
          return nullptr;
        }
        SgScopeStatement *resolved =
            resolveScopeFromDeclContext(ctx, SageBuilder::topScopeStack());
        if (SgClassDeclaration *class_decl = isSgClassDeclaration(resolved)) {
          if (class_decl->get_definition() != nullptr) {
            resolved = class_decl->get_definition();
          }
        }
        if (resolved != nullptr) {
          return resolved;
        }
        if (clang::CXXRecordDecl *record =
                llvm::dyn_cast<clang::CXXRecordDecl>(ctx)) {
          clang::CXXRecordDecl *record_def = record->getDefinition() != nullptr
                                                 ? record->getDefinition()
                                                 : record;
          if (record_def != nullptr) {
            if (SgDeclarationStatement *decl = lookupSgDeclarationForClangDecl(
                    record_def,
                    /*allow_on_demand=*/false)) {
              if (SgClassDeclaration *class_decl = isSgClassDeclaration(decl)) {
                if (class_decl->get_definition() != nullptr) {
                  return class_decl->get_definition();
                }
                return nullptr;
              }
              if (SgClassDefinition *class_def = isSgClassDefinition(decl)) {
                return class_def;
              }
            }
            if (SgClassDefinition *enclosing =
                    SageInterface::getEnclosingClassDefinition(
                        SageBuilder::topScopeStack(), true)) {
              return enclosing;
            }
            if (p_decl_translation_in_progress.find(record_def) ==
                p_decl_translation_in_progress.end()) {
              TraverseOnDemand(record_def);
              resolved = resolveScopeFromDeclContext(
                  record_def, SageBuilder::topScopeStack());
              if (SgClassDeclaration *class_decl =
                      isSgClassDeclaration(resolved)) {
                if (class_decl->get_definition() != nullptr) {
                  resolved = class_decl->get_definition();
                }
              }
              if (resolved != nullptr) {
                return resolved;
              }
            }
          }
        }
        for (auto it = SageBuilder::ScopeStack.rbegin();
             it != SageBuilder::ScopeStack.rend(); ++it) {
          if (SgClassDefinition *class_def = isSgClassDefinition(*it)) {
            return class_def;
          }
        }
        return nullptr;
      };
      SgScopeStatement *member_scope = resolve_static_member_scope();

      bool pushed_scope = false;
      if (member_scope != nullptr) {
        SageBuilder::pushScopeStack(member_scope);
        pushed_scope = true;
      }

      SgSymbol *member_sym = GetSymbolFromSymbolTable(var_decl);
      if (member_sym == nullptr && member_scope != nullptr) {
        if (p_decl_translation_in_progress.find(var_decl) ==
            p_decl_translation_in_progress.end()) {
          if (SgDeclarationStatement *decl_stmt =
                  lookupSgDeclarationForClangDecl(var_decl,
                                                  /*allow_on_demand=*/true)) {
            if (SgVariableDeclaration *sg_var_decl =
                    isSgVariableDeclaration(decl_stmt)) {
              for (SgInitializedName *init_name :
                   sg_var_decl->get_variables()) {
                if (init_name == nullptr) {
                  continue;
                }
                if (init_name->get_scope() == nullptr ||
                    init_name->get_scope() != member_scope) {
                  init_name->set_scope(member_scope);
                }
              }
              registerDeclarationSymbol(sg_var_decl);
            }
          }
        }
        member_sym = GetSymbolFromSymbolTable(var_decl);
      }

      if (pushed_scope) {
        SageBuilder::popScopeStack();
      }

      if (member_sym != nullptr) {
        sym = member_sym;
      }
    }
  }

  if (sym == nullptr) {
    sym = GetSymbolFromSymbolTable(decl_ref_expr->getDecl());
  }

  auto resolve_member_symbol = [&](SgMemberFunctionDeclaration *member_decl)
      -> SgMemberFunctionSymbol * {
    if (member_decl == nullptr) {
      return nullptr;
    }
    SgScopeStatement *decl_scope =
        normalizeNamespaceScope(member_decl->get_scope());
    if (decl_scope == nullptr) {
      decl_scope = SageBuilder::topScopeStack();
    }
    SgMemberFunctionSymbol *member_sym = nullptr;
    if (decl_scope != nullptr) {
      member_sym = decl_scope->lookup_nontemplate_member_function_symbol(
          member_decl->get_name(), member_decl->get_type(), nullptr);
      if (member_sym == nullptr) {
        if (SgFunctionSymbol *wrong_sym = decl_scope->lookup_function_symbol(
                member_decl->get_name(), member_decl->get_type())) {
          decl_scope->remove_symbol(wrong_sym);
          move_symbol_to_orphan_table(wrong_sym);
        }
      }
    }
    if (member_sym == nullptr) {
      member_sym = new SgMemberFunctionSymbol(member_decl);
      if (decl_scope != nullptr) {
        attachSymbolToScopeOrOrphan(member_sym, decl_scope);
      } else {
        attachSymbolToScopeOrOrphan(member_sym, nullptr);
      }
    }
    return member_sym;
  };

  auto resolve_member_declaration_from_symbol =
      [&](SgSymbol *symbol) -> SgMemberFunctionDeclaration * {
    if (symbol == nullptr) {
      return nullptr;
    }
    if (SgMemberFunctionSymbol *member_symbol =
            isSgMemberFunctionSymbol(symbol)) {
      return isSgMemberFunctionDeclaration(member_symbol->get_declaration());
    }
    if (SgTemplateMemberFunctionSymbol *tmpl_member_symbol =
            isSgTemplateMemberFunctionSymbol(symbol)) {
      return isSgMemberFunctionDeclaration(
          tmpl_member_symbol->get_declaration());
    }
    if (SgTemplateFunctionSymbol *tmpl_function_symbol =
            isSgTemplateFunctionSymbol(symbol)) {
      return isSgMemberFunctionDeclaration(
          tmpl_function_symbol->get_declaration());
    }
    if (SgFunctionSymbol *function_symbol = isSgFunctionSymbol(symbol)) {
      return isSgMemberFunctionDeclaration(function_symbol->get_declaration());
    }
    return nullptr;
  };

  auto lookup_translated_member_declaration =
      [&](clang::Decl *lookup_decl) -> SgMemberFunctionDeclaration * {
    if (lookup_decl == nullptr) {
      return nullptr;
    }

    SgDeclarationStatement *decl_stmt =
        lookupSgDeclarationForClangDecl(lookup_decl, /*allow_on_demand=*/true);
    if (decl_stmt == nullptr &&
        p_decl_translation_in_progress.find(lookup_decl) ==
            p_decl_translation_in_progress.end() &&
        p_decl_translation_on_demand.find(lookup_decl) ==
            p_decl_translation_on_demand.end()) {
      decl_stmt = isSgDeclarationStatement(TraverseOnDemand(lookup_decl));
    }
    if (SgTemplateInstantiationDirectiveStatement *inst_directive =
            isSgTemplateInstantiationDirectiveStatement(decl_stmt)) {
      decl_stmt = inst_directive->get_declaration();
    }

    return isSgMemberFunctionDeclaration(decl_stmt);
  };

  auto lookup_template_member_decl =
      [&](clang::Decl *lookup_decl) -> SgTemplateMemberFunctionDeclaration * {
    if (lookup_decl == nullptr) {
      return nullptr;
    }

    SgDeclarationStatement *decl_stmt =
        lookupSgDeclarationForClangDecl(lookup_decl, /*allow_on_demand=*/true);
    if (SgTemplateInstantiationDirectiveStatement *inst_directive =
            isSgTemplateInstantiationDirectiveStatement(decl_stmt)) {
      decl_stmt = inst_directive->get_declaration();
    }

    if (SgTemplateMemberFunctionDeclaration *tmpl_decl =
            isSgTemplateMemberFunctionDeclaration(decl_stmt)) {
      return tmpl_decl;
    }

    return recover_instantiation_template_declaration<
        SgTemplateMemberFunctionDeclaration,
        SgTemplateInstantiationMemberFunctionDecl>(
        isSgFunctionDeclaration(decl_stmt));
  };

  auto lookup_template_function_decl =
      [&](clang::Decl *lookup_decl) -> SgTemplateFunctionDeclaration * {
    if (lookup_decl == nullptr) {
      return nullptr;
    }

    SgDeclarationStatement *decl_stmt =
        lookupSgDeclarationForClangDecl(lookup_decl, /*allow_on_demand=*/true);
    if (SgTemplateInstantiationDirectiveStatement *inst_directive =
            isSgTemplateInstantiationDirectiveStatement(decl_stmt)) {
      decl_stmt = inst_directive->get_declaration();
    }

    if (SgTemplateFunctionDeclaration *tmpl_decl =
            isSgTemplateFunctionDeclaration(decl_stmt)) {
      return tmpl_decl;
    }

    return recover_instantiation_template_declaration<
        SgTemplateFunctionDeclaration, SgTemplateInstantiationFunctionDecl>(
        isSgFunctionDeclaration(decl_stmt));
  };

  auto resolve_member_template_decl =
      [&](SgMemberFunctionDeclaration *member_decl,
          clang::FunctionDecl *clang_func)
      -> SgTemplateMemberFunctionDeclaration * {
    if (SgTemplateMemberFunctionDeclaration *tmpl_decl =
            recover_instantiation_template_declaration<
                SgTemplateMemberFunctionDeclaration,
                SgTemplateInstantiationMemberFunctionDecl>(member_decl)) {
      return tmpl_decl;
    }

    if (clang_func == nullptr) {
      return nullptr;
    }

    if (SgTemplateMemberFunctionDeclaration *tmpl_decl =
            lookup_template_member_decl(clang_func->getPrimaryTemplate())) {
      return tmpl_decl;
    }
    if (SgTemplateMemberFunctionDeclaration *tmpl_decl =
            lookup_template_member_decl(
                clang_func->getDescribedFunctionTemplate())) {
      return tmpl_decl;
    }
    if (SgTemplateMemberFunctionDeclaration *tmpl_decl =
            lookup_template_member_decl(
                clang_func->getInstantiatedFromMemberFunction())) {
      return tmpl_decl;
    }
    return lookup_template_member_decl(
        clang_func->getTemplateInstantiationPattern());
  };

  auto resolve_function_template_decl =
      [&](SgFunctionDeclaration *func_decl,
          clang::FunctionDecl *clang_func) -> SgTemplateFunctionDeclaration * {
    if (SgTemplateFunctionDeclaration *tmpl_decl =
            recover_instantiation_template_declaration<
                SgTemplateFunctionDeclaration,
                SgTemplateInstantiationFunctionDecl>(func_decl)) {
      return tmpl_decl;
    }

    if (clang_func == nullptr) {
      return nullptr;
    }

    if (SgTemplateFunctionDeclaration *tmpl_decl =
            lookup_template_function_decl(clang_func->getPrimaryTemplate())) {
      return tmpl_decl;
    }
    if (SgTemplateFunctionDeclaration *tmpl_decl =
            lookup_template_function_decl(
                clang_func->getDescribedFunctionTemplate())) {
      return tmpl_decl;
    }
    return lookup_template_function_decl(
        clang_func->getTemplateInstantiationPattern());
  };

  if (clang::CXXMethodDecl *clang_method =
          llvm::dyn_cast<clang::CXXMethodDecl>(decl_ref_expr->getDecl())) {
    SgMemberFunctionSymbol *method_sym = isSgMemberFunctionSymbol(sym);
    if (method_sym == nullptr) {
      SgMemberFunctionDeclaration *member_decl =
          resolve_member_declaration_from_symbol(sym);
      if (member_decl == nullptr) {
        SgDeclarationStatement *decl_stmt = lookupSgDeclarationForClangDecl(
            clang_method, /*allow_on_demand=*/true);
        if (SgTemplateInstantiationDirectiveStatement *inst_directive =
                isSgTemplateInstantiationDirectiveStatement(decl_stmt)) {
          decl_stmt = inst_directive->get_declaration();
        }
        member_decl = isSgMemberFunctionDeclaration(decl_stmt);
      }
      if (member_decl == nullptr) {
        if (clang::FunctionDecl *pattern =
                clang_method->getInstantiatedFromMemberFunction()) {
          member_decl = lookup_translated_member_declaration(pattern);
        }
      }
      if (member_decl == nullptr) {
        if (clang::FunctionDecl *pattern =
                clang_method->getTemplateInstantiationPattern()) {
          member_decl = lookup_translated_member_declaration(pattern);
        }
      }
      if (member_decl == nullptr) {
        if (clang::FunctionTemplateDecl *primary =
                clang_method->getPrimaryTemplate()) {
          member_decl = lookup_translated_member_declaration(primary);
          if (member_decl == nullptr) {
            member_decl = lookup_translated_member_declaration(
                primary->getTemplatedDecl());
          }
        }
      }
      if (member_decl == nullptr) {
        if (const clang::FunctionTemplateSpecializationInfo *spec_info =
                clang_method->getTemplateSpecializationInfo()) {
          if (clang::FunctionTemplateDecl *template_decl =
                  spec_info->getTemplate()) {
            member_decl = lookup_translated_member_declaration(template_decl);
            if (member_decl == nullptr) {
              member_decl = lookup_translated_member_declaration(
                  template_decl->getTemplatedDecl());
            }
          }
        }
      }
      method_sym = resolve_member_symbol(member_decl);
    }

    if (method_sym != nullptr) {
      sym = method_sym;
    }
  }

  if (sym != nullptr) {
    if (clang::FunctionDecl *clang_func =
            llvm::dyn_cast<clang::FunctionDecl>(decl_ref_expr->getDecl())) {
      if (isSgTemplateFunctionSymbol(sym) != nullptr &&
          !llvm::isa<clang::CXXMethodDecl>(clang_func) &&
          clang_func->getDescribedFunctionTemplate() == nullptr &&
          clang_func->getPrimaryTemplate() == nullptr) {
        SgNode *tmp_decl = nullptr;
        auto it_decl = p_decl_translation_map.find(clang_func);
        if (it_decl != p_decl_translation_map.end()) {
          tmp_decl = it_decl->second;
        } else if (p_decl_translation_in_progress.find(clang_func) ==
                   p_decl_translation_in_progress.end()) {
          tmp_decl = Traverse(clang_func);
        }

        if (SgFunctionDeclaration *func_decl =
                isSgFunctionDeclaration(tmp_decl)) {
          SgScopeStatement *decl_scope =
              normalizeNamespaceScope(func_decl->get_scope());
          if (decl_scope != nullptr && func_decl->get_scope() != decl_scope) {
            func_decl->set_scope(decl_scope);
            if (SgFunctionDeclaration *first_nondef = isSgFunctionDeclaration(
                    func_decl->get_firstNondefiningDeclaration())) {
              if (first_nondef->get_scope() != decl_scope) {
                first_nondef->set_scope(decl_scope);
              }
            }
            if (SgFunctionDeclaration *def_decl = isSgFunctionDeclaration(
                    func_decl->get_definingDeclaration())) {
              if (def_decl->get_scope() != decl_scope) {
                def_decl->set_scope(decl_scope);
              }
            }
          }

          SgFunctionSymbol *func_sym = nullptr;
          if (decl_scope != nullptr) {
            func_sym = decl_scope->lookup_nontemplate_function_symbol(
                func_decl->get_name(), func_decl->get_type(), nullptr);
          }
          if (func_sym == nullptr && decl_scope != nullptr) {
            func_sym = decl_scope->lookup_function_symbol(
                func_decl->get_name(), func_decl->get_type());
          }
          if (func_sym == nullptr) {
            func_sym = new SgFunctionSymbol(func_decl);
            if (decl_scope != nullptr) {
              attachSymbolToScopeOrOrphan(func_sym, decl_scope);
            } else {
              attachSymbolToScopeOrOrphan(func_sym, nullptr);
            }
          }
          if (func_sym != nullptr) {
            sym = func_sym;
          }
        }
      }
    }
  }

  if (sym == nullptr) {
    // If Clang resolved a templated member call to a concrete FunctionDecl,
    // traversing that declaration can build multiple "foo" declarations that
    // collide (same name/signature) across different template arguments.
    // For references with explicit template arguments, build an explicit
    // template instantiation ref directly instead of traversing the resolved
    // specialization.
    if (decl_ref_expr->hasExplicitTemplateArgs()) {
      if (clang::FunctionDecl *clang_func =
              llvm::dyn_cast<clang::FunctionDecl>(decl_ref_expr->getDecl())) {
        if (clang::CXXMethodDecl *method_decl =
                llvm::dyn_cast<clang::CXXMethodDecl>(clang_func)) {
          clang::TemplateArgumentListInfo arg_info;
          decl_ref_expr->copyTemplateArgumentsInto(arg_info);
          const size_t explicit_arg_count =
              countExpandedTemplateArguments(arg_info);

          clang::CXXRecordDecl *parent_decl = method_decl->getParent();
          SgScopeStatement *class_scope = nullptr;
          if (parent_decl != nullptr) {
            SgNode *parent_node = nullptr;
            std::map<clang::Decl *, SgNode *>::iterator it_decl =
                p_decl_translation_map.find(parent_decl);
            if (it_decl != p_decl_translation_map.end()) {
              parent_node = it_decl->second;
            } else {
              parent_node = Traverse(parent_decl);
            }

            if (SgClassDeclaration *class_decl =
                    isSgClassDeclaration(parent_node)) {
              class_scope = class_decl->get_definition();
            } else if (SgClassDefinition *class_def =
                           isSgClassDefinition(parent_node)) {
              class_scope = class_def;
            } else if (SgTemplateClassDeclaration *template_class_decl =
                           isSgTemplateClassDeclaration(parent_node)) {
              class_scope = template_class_decl->get_definition();
            }
          }

          if (class_scope != nullptr) {
            unsigned int functionConstVolatileFlags = 0;
            if (method_decl->isConst()) {
              functionConstVolatileFlags |= SgMemberFunctionType::e_const;
            }
            if (method_decl->isVolatile()) {
              functionConstVolatileFlags |= SgMemberFunctionType::e_volatile;
            }
            clang::Qualifiers qualifiers = method_decl->getMethodQualifiers();
            if (qualifiers.hasRestrict()) {
              functionConstVolatileFlags |= SgMemberFunctionType::e_restrict;
            }
            switch (method_decl->getRefQualifier()) {
            case clang::RQ_LValue:
              functionConstVolatileFlags |=
                  SgMemberFunctionType::e_ref_qualifier_lvalue;
              break;
            case clang::RQ_RValue:
              functionConstVolatileFlags |=
                  SgMemberFunctionType::e_ref_qualifier_rvalue;
              break;
            case clang::RQ_None:
              break;
            }

            SgType *ret_type =
                buildTypeFromQualifiedType(method_decl->getReturnType());
            if (ret_type == nullptr) {
              ret_type = SageBuilder::buildUnknownType();
            }

            SgFunctionParameterList *param_list = nullptr;
            if (SgFunctionType *func_type = isSgFunctionType(
                    buildTypeFromQualifiedType(method_decl->getType()))) {
              if (func_type->get_argument_list() != nullptr) {
                param_list = SageBuilder::buildFunctionParameterList_nfi(
                    func_type->get_argument_list());
              }
            }
            if (param_list == nullptr) {
              param_list = SageBuilder::buildFunctionParameterList_nfi();
            }

            SgTemplateArgumentPtrList template_args;
            if (const clang::TemplateArgumentList *clang_args =
                    clang_func->getTemplateSpecializationArgs()) {
              if (clang_args->size() != 0) {
                template_args =
                    buildTemplateArguments(*clang_args, explicit_arg_count);
              } else {
                template_args = buildTemplateArguments(arg_info, true);
              }
            } else {
              template_args = buildTemplateArguments(arg_info, true);
            }
            SgTemplateArgumentPtrList *builder_args = &template_args;
            SgTemplateInstantiationMemberFunctionDecl *inst_decl =
                isSgTemplateInstantiationMemberFunctionDecl(
                    SageBuilder::buildNondefiningMemberFunctionDeclaration(
                        SgName(method_decl->getNameAsString()), ret_type,
                        param_list, class_scope, functionConstVolatileFlags,
                        /*buildTemplateInstantiation=*/true, builder_args));

            ensure_function_param_list(inst_decl, param_list);
            if (inst_decl != nullptr) {
              inst_decl->set_template_argument_list_is_explicit(true);
              SageBuilder::setTemplateArgumentsInDeclaration(inst_decl,
                                                             &template_args);

              if (SgTemplateMemberFunctionDeclaration *tmpl_decl =
                      resolve_member_template_decl(nullptr, clang_func)) {
                apply_template_instantiation_template_links(
                    inst_decl, tmpl_decl, tmpl_decl->get_name());
              }

              finalize_instantiation_decl(inst_decl);
              registerDeclarationSymbol(inst_decl);
              if (SgMemberFunctionSymbol *inst_sym = isSgMemberFunctionSymbol(
                      inst_decl->get_symbol_from_symbol_table())) {
                ensure_function_decl_symbol(
                    isSgFunctionDeclaration(inst_sym->get_declaration()));
                SgExpression *ref_exp =
                    SageBuilder::buildMemberFunctionRefExp_nfi(inst_sym, false,
                                                               false);
                attach_explicit_qualifier(ref_exp);
                applySourceRange(ref_exp, decl_ref_expr->getSourceRange());
                *node = ref_exp;
                return VisitExpr(decl_ref_expr, node) && res;
              }
            }
          }
        }
      }
    }

    SgNode *tmp_decl = nullptr;
    SgScopeStatement *decl_scope_hint = nullptr;
    if (clang::DeclContext *ctx = decl_ref_expr->getDecl()->getDeclContext()) {
      decl_scope_hint =
          resolveScopeFromDeclContext(ctx, SageBuilder::topScopeStack());
    }
    if (force_global_scope) {
      decl_scope_hint =
          SageInterface::getGlobalScope(SageBuilder::topScopeStack());
    }

    bool pushed_scope = false;
    if (decl_scope_hint != nullptr &&
        decl_scope_hint != SageBuilder::topScopeStack()) {
      SageBuilder::pushScopeStack(decl_scope_hint);
      pushed_scope = true;
    }

    if (clang::FunctionDecl *clang_func =
            llvm::dyn_cast<clang::FunctionDecl>(decl_ref_expr->getDecl())) {
      if (p_decl_translation_in_progress.find(clang_func) !=
          p_decl_translation_in_progress.end()) {
        if (clang::FunctionTemplateDecl *primary =
                clang_func->getPrimaryTemplate()) {
          tmp_decl = lookupSgDeclarationForClangDecl(primary,
                                                     /*allow_on_demand=*/true);
          if (tmp_decl == nullptr) {
            tmp_decl = lookupSgDeclarationForClangDecl(
                primary->getTemplatedDecl(), /*allow_on_demand=*/true);
          }
        }
        if (tmp_decl == nullptr) {
          if (clang::FunctionDecl *pattern =
                  clang_func->getTemplateInstantiationPattern()) {
            tmp_decl = lookupSgDeclarationForClangDecl(
                pattern, /*allow_on_demand=*/true);
          }
        }
        if (tmp_decl == nullptr) {
          if (const clang::FunctionTemplateSpecializationInfo *spec_info =
                  clang_func->getTemplateSpecializationInfo()) {
            if (clang::FunctionTemplateDecl *template_decl =
                    spec_info->getTemplate()) {
              tmp_decl = lookupSgDeclarationForClangDecl(
                  template_decl, /*allow_on_demand=*/true);
              if (tmp_decl == nullptr) {
                tmp_decl = lookupSgDeclarationForClangDecl(
                    template_decl->getTemplatedDecl(),
                    /*allow_on_demand=*/true);
              }
            }
          }
        }
      }
    }

    if (tmp_decl == nullptr) {
      tmp_decl = Traverse(decl_ref_expr->getDecl());
    }

    if (SgTemplateInstantiationDirectiveStatement *inst_directive =
            isSgTemplateInstantiationDirectiveStatement(tmp_decl)) {
      if (SgDeclarationStatement *inst_decl =
              inst_directive->get_declaration()) {
        tmp_decl = inst_decl;
      }
    }

    if (pushed_scope) {
      SageBuilder::popScopeStack();
    }

    // DQ (11/29/2020): Added assertion.
    ROSE_ASSERT(tmp_decl != nullptr);

#if DEBUG_VISIT_STMT
    printf("tmp_decl = %p = %s \n", tmp_decl, tmp_decl->class_name().c_str());
#endif
    SgInitializedName *initializedName = isSgInitializedName(tmp_decl);
#if DEBUG_VISIT_STMT
    if (initializedName != nullptr) {
      printf("Found SgInitializedName: initializedName->get_name() = %s \n",
             initializedName->get_name().str());
    }
#endif

    if (tmp_decl != nullptr) {
      sym = GetSymbolFromSymbolTable(decl_ref_expr->getDecl());
    }

    // FIXME hack Traverse have added the symbol but we cannot find it
    // (probably: problem with type and function lookup)

    if (sym == nullptr && isSgFunctionDeclaration(tmp_decl) != nullptr) {
      if (SgMemberFunctionDeclaration *member_func_decl =
              isSgMemberFunctionDeclaration(tmp_decl)) {
        SgScopeStatement *decl_scope = member_func_decl->get_scope();
        decl_scope = normalizeNamespaceScope(decl_scope);

        SgSymbol *member_sym = nullptr;
        if (SgTemplateMemberFunctionDeclaration *tmpl_member =
                isSgTemplateMemberFunctionDeclaration(member_func_decl)) {
          SgTemplateParameterPtrList *params =
              &tmpl_member->get_templateParameters();
          if (decl_scope != nullptr) {
            member_sym = decl_scope->lookup_template_member_function_symbol(
                tmpl_member->get_name(), tmpl_member->get_type(), params);
          }
          if (member_sym == nullptr) {
            SgTemplateMemberFunctionSymbol *new_sym =
                new SgTemplateMemberFunctionSymbol(tmpl_member);
            if (decl_scope != nullptr) {
              attachSymbolToScopeOrOrphan(new_sym, decl_scope);
            } else {
              attachSymbolToScopeOrOrphan(new_sym, nullptr);
            }
            member_sym = new_sym;
          }
        } else {
          SgMemberFunctionSymbol *member_func_sym = nullptr;
          if (decl_scope != nullptr) {
            member_func_sym =
                decl_scope->lookup_nontemplate_member_function_symbol(
                    member_func_decl->get_name(), member_func_decl->get_type(),
                    nullptr);
          }
          if (member_func_sym == nullptr && decl_scope != nullptr) {
            if (SgFunctionSymbol *wrong_sym =
                    decl_scope->lookup_function_symbol(
                        member_func_decl->get_name(),
                        member_func_decl->get_type())) {
              decl_scope->remove_symbol(wrong_sym);
              move_symbol_to_orphan_table(wrong_sym);
            }
          }
          if (member_func_sym == nullptr) {
            SgMemberFunctionSymbol *new_sym =
                new SgMemberFunctionSymbol(member_func_decl);
            if (decl_scope != nullptr) {
              attachSymbolToScopeOrOrphan(new_sym, decl_scope);
            } else {
              attachSymbolToScopeOrOrphan(new_sym, nullptr);
            }
            member_func_sym = new_sym;
          }
          member_sym = member_func_sym;
        }
        sym = member_sym;
      } else {
        SgFunctionDeclaration *func_decl = isSgFunctionDeclaration(tmp_decl);
        SgScopeStatement *decl_scope =
            normalizeNamespaceScope(func_decl->get_scope());
        if (decl_scope != nullptr && func_decl->get_scope() != decl_scope) {
          func_decl->set_scope(decl_scope);
          if (SgFunctionDeclaration *first_nondef = isSgFunctionDeclaration(
                  func_decl->get_firstNondefiningDeclaration())) {
            if (first_nondef->get_scope() != decl_scope) {
              first_nondef->set_scope(decl_scope);
            }
          }
          if (SgFunctionDeclaration *def_decl = isSgFunctionDeclaration(
                  func_decl->get_definingDeclaration())) {
            if (def_decl->get_scope() != decl_scope) {
              def_decl->set_scope(decl_scope);
            }
          }
        }

        SgSymbol *func_sym = nullptr;
        if (SgTemplateFunctionDeclaration *tmpl_decl =
                isSgTemplateFunctionDeclaration(func_decl)) {
          SgTemplateParameterPtrList *params =
              &tmpl_decl->get_templateParameters();
          if (decl_scope != nullptr) {
            func_sym = decl_scope->lookup_template_function_symbol(
                tmpl_decl->get_name(), tmpl_decl->get_type(), params);
          }
          if (func_sym == nullptr) {
            SgTemplateFunctionSymbol *new_sym =
                new SgTemplateFunctionSymbol(tmpl_decl);
            if (decl_scope != nullptr) {
              attachSymbolToScopeOrOrphan(new_sym, decl_scope);
            } else {
              attachSymbolToScopeOrOrphan(new_sym, nullptr);
            }
            func_sym = new_sym;
          }
        } else {
          SgFunctionSymbol *non_template_sym = nullptr;
          if (decl_scope != nullptr) {
            non_template_sym = decl_scope->lookup_nontemplate_function_symbol(
                func_decl->get_name(), func_decl->get_type(), nullptr);
            if (non_template_sym == nullptr) {
              non_template_sym = decl_scope->lookup_function_symbol(
                  func_decl->get_name(), func_decl->get_type());
            }
          }
          if (non_template_sym == nullptr) {
            SgFunctionSymbol *new_sym = new SgFunctionSymbol(func_decl);
            if (decl_scope != nullptr) {
              attachSymbolToScopeOrOrphan(new_sym, decl_scope);
            } else {
              attachSymbolToScopeOrOrphan(new_sym, nullptr);
            }
            non_template_sym = new_sym;
          }
          func_sym = non_template_sym;
        }
        sym = func_sym;
      }
    }
    // ROOT CAUSE FIX: Handle SgVariableDeclaration from VisitVarDecl
    // Extract the InitializedName and create symbol if needed
    if (sym == nullptr && isSgVariableDeclaration(tmp_decl) != nullptr) {
      SgVariableDeclaration *var_decl_result =
          isSgVariableDeclaration(tmp_decl);
      if (var_decl_result->get_variables().size() > 0) {
        SgInitializedName *init_name = var_decl_result->get_variables()[0];
        if (init_name != nullptr) {
          // Try to get existing symbol first
          SgScopeStatement *init_scope =
              normalizeNamespaceScope(init_name->get_scope());
          if (init_scope != nullptr) {
            sym = init_scope->lookup_variable_symbol(init_name->get_name());
          }
          // If still not found, create new symbol
          if (sym == nullptr) {
            sym = new SgVariableSymbol(init_name);
            if (init_scope != nullptr) {
              attachSymbolToScopeOrOrphan(sym, init_scope);
            } else {
              attachSymbolToScopeOrOrphan(sym, nullptr);
            }
          }
        }
      }
    }
    // Pei-Hung (04/07/2022) sym can be nullptr in the case for C99 VLA
    if (sym == nullptr && isSgInitializedName(tmp_decl) != nullptr) {
      SgInitializedName *init_name = isSgInitializedName(tmp_decl);

      if (llvm::isa<clang::EnumConstantDecl>(decl_ref_expr->getDecl())) {
        SgScopeStatement *init_scope =
            normalizeNamespaceScope(init_name->get_scope());
        if (init_scope == nullptr) {
          init_scope = normalizeNamespaceScope(SageBuilder::topScopeStack());
        }
        if (init_scope != nullptr) {
          SgEnumFieldSymbol *enum_sym =
              init_scope->lookup_enum_field_symbol(init_name->get_name());
          if (enum_sym == nullptr) {
            enum_sym = new SgEnumFieldSymbol(init_name);
            attachSymbolToScopeOrOrphan(enum_sym, init_scope);
          }
          sym = enum_sym;
        }
      } else {
        sym = new SgVariableSymbol(init_name);
        SgScopeStatement *decl_scope =
            normalizeNamespaceScope(SageBuilder::topScopeStack());
        if (decl_scope != nullptr) {
          attachSymbolToScopeOrOrphan(sym, decl_scope);
        } else {
          attachSymbolToScopeOrOrphan(sym, nullptr);
        }
      }
    }
  }

  if (sym != nullptr) {
    // Not else if it was nullptr we have try to traverse it....
    SgVariableSymbol *var_sym = isSgVariableSymbol(sym);
    SgMemberFunctionSymbol *member_func_sym = isSgMemberFunctionSymbol(sym);
    SgFunctionSymbol *func_sym = isSgFunctionSymbol(sym);
    SgTemplateFunctionSymbol *tmpl_func_sym = isSgTemplateFunctionSymbol(sym);
    SgTemplateMemberFunctionSymbol *tmpl_member_sym =
        isSgTemplateMemberFunctionSymbol(sym);
    SgEnumFieldSymbol *enum_sym = isSgEnumFieldSymbol(sym);

    if (func_sym != nullptr) {
      ensure_function_decl_symbol(
          isSgFunctionDeclaration(func_sym->get_declaration()));
    }
    if (member_func_sym != nullptr) {
      ensure_function_decl_symbol(
          isSgFunctionDeclaration(member_func_sym->get_declaration()));
    }
    if (tmpl_func_sym != nullptr) {
      ensure_function_decl_symbol(
          isSgFunctionDeclaration(tmpl_func_sym->get_declaration()));
    }
    if (tmpl_member_sym != nullptr) {
      ensure_function_decl_symbol(
          isSgFunctionDeclaration(tmpl_member_sym->get_declaration()));
    }

    if (var_sym != nullptr) {
      SgExpression *ref_exp = nullptr;
      const bool explicit_variable_template_ref =
          decl_ref_expr->hasExplicitTemplateArgs() &&
          (llvm::isa<clang::VarTemplateSpecializationDecl>(
               decl_ref_expr->getDecl()) ||
           isSgTemplateVariableSymbol(sym) != nullptr);
      const bool build_nonreal_ref =
          explicit_variable_template_ref ||
          (decl_ref_qualifier &&
           qualifier_has_type_component(decl_ref_qualifier));
      if (build_nonreal_ref) {
        SgTemplateArgumentPtrList template_args;
        const SgTemplateArgumentPtrList *template_args_ptr = nullptr;
        if (decl_ref_expr->hasExplicitTemplateArgs()) {
          clang::TemplateArgumentListInfo arg_info;
          decl_ref_expr->copyTemplateArgumentsInto(arg_info);
          template_args = buildTemplateArguments(arg_info, true);
          if (template_args.empty()) {
            if (clang::VarTemplateSpecializationDecl *spec_decl =
                    llvm::dyn_cast<clang::VarTemplateSpecializationDecl>(
                        decl_ref_expr->getDecl())) {
              const clang::TemplateArgumentList &clang_args =
                  spec_decl->getTemplateArgs();
              for (unsigned i = 0; i < clang_args.size(); ++i) {
                appendTemplateArguments(template_args, clang_args.get(i),
                                        false);
              }
            }
          }
          if (!template_args.empty()) {
            template_args_ptr = &template_args;
          }
        }

        SgScopeStatement *scope = SageBuilder::topScopeStack();
        if (scope == nullptr) {
          scope = getGlobalScope();
        }

        ref_exp = buildNonrealRefExpFromNestedNameSpecifier(
            structuralQualifierForExplicitlyQualifiedNonrealRef(
                decl_ref_qualifier),
            scope, SgName(decl_ref_expr->getDecl()->getNameAsString()),
            decl_ref_expr->hasTemplateKeyword(), template_args_ptr);
        attach_explicit_qualifier(ref_exp);
      }

      if (ref_exp == nullptr) {
        ref_exp = SageBuilder::buildVarRefExp(var_sym);
        attach_explicit_qualifier(ref_exp);
      }

      *node = ref_exp;
    } else {
      SgMemberFunctionSymbol *member_sym = member_func_sym;
      if (member_sym == nullptr) {
        member_sym = resolve_member_symbol(
            resolve_member_declaration_from_symbol(tmpl_member_sym));
      }
      if (member_sym == nullptr) {
        member_sym = resolve_member_symbol(
            resolve_member_declaration_from_symbol(tmpl_func_sym));
      }
      if (member_sym == nullptr) {
        member_sym =
            resolve_member_symbol(resolve_member_declaration_from_symbol(
                static_cast<SgSymbol *>(func_sym)));
      }

      clang::FunctionDecl *clang_func =
          llvm::dyn_cast<clang::FunctionDecl>(decl_ref_expr->getDecl());
      auto get_explicit_template_arg_info =
          [&](clang::TemplateArgumentListInfo &arg_info,
              size_t &explicit_arg_count) -> bool {
        if (!decl_ref_expr->hasExplicitTemplateArgs()) {
          return false;
        }

        decl_ref_expr->copyTemplateArgumentsInto(arg_info);
        explicit_arg_count = countExpandedTemplateArguments(arg_info);
        return true;
      };

      clang::TemplateArgumentListInfo explicit_arg_info;
      size_t explicit_arg_count = 0;
      bool has_explicit_template_args =
          get_explicit_template_arg_info(explicit_arg_info, explicit_arg_count);
      bool has_explicit_empty_template_id =
          has_explicit_template_args &&
          hasExplicitEmptyTemplateArgumentList(explicit_arg_info);
      const clang::TemplateArgumentList *deduced_args =
          clang_func != nullptr ? clang_func->getTemplateSpecializationArgs()
                                : nullptr;
      bool has_deduced_template_args =
          deduced_args != nullptr && deduced_args->size() != 0;
      if (!has_deduced_template_args && clang_func != nullptr) {
        if (const clang::FunctionTemplateSpecializationInfo *spec_info =
                clang_func->getTemplateSpecializationInfo()) {
          deduced_args = spec_info->TemplateArguments;
          has_deduced_template_args =
              deduced_args != nullptr && deduced_args->size() != 0;
        }
      }
      if (has_explicit_template_args || has_deduced_template_args) {
        ExplicitTemplateArgumentSourceInfo source_info =
            scanExplicitTemplateArgumentsForExprSource(
                decl_ref_expr->getSourceRange(), decl_ref_expr->getEndLoc(),
                p_compiler_instance);
        if (source_info.has_template_argument_list) {
          explicit_arg_count = source_info.argument_count;
          has_explicit_template_args = true;
          has_explicit_empty_template_id = (source_info.argument_count == 0);
        }
      }
      const bool is_member_function_decl =
          llvm::isa<clang::CXXMethodDecl>(decl_ref_expr->getDecl());
      // Restrict on-the-fly instantiation synthesis to unqualified explicit
      // template-argument references, except member-function templates where
      // explicit args are carried by instantiation declarations.
      // Qualified namespace/global refs (e.g. `::foo<int>`) must preserve
      // source spelling to keep overload/specialization lookup stable.
      const bool should_build_instantiation =
          has_explicit_template_args &&
          (!decl_ref_expr->hasQualifier() || is_member_function_decl);
      auto build_template_args_for_instantiation =
          [&]() -> SgTemplateArgumentPtrList {
        if (has_explicit_template_args) {
          SgTemplateArgumentPtrList template_args =
              buildTemplateArguments(explicit_arg_info, true);
          if (template_args.empty() && !has_explicit_empty_template_id &&
              clang_func != nullptr && deduced_args != nullptr &&
              deduced_args->size() != 0) {
            template_args =
                buildTemplateArguments(*deduced_args, explicit_arg_count);
          }
          return template_args;
        }

        if (clang_func != nullptr && deduced_args != nullptr &&
            deduced_args->size() != 0) {
          return buildTemplateArguments(*deduced_args, explicit_arg_count);
        }

        return buildTemplateArguments(explicit_arg_info, true);
      };
      auto get_decl_namespace_scope =
          [&](clang::NamedDecl *clang_decl) -> SgScopeStatement * {
        if (clang_decl == nullptr) {
          return nullptr;
        }

        clang::DeclContext *ctx = clang_decl->getDeclContext();
        while (ctx != nullptr && !ctx->isTranslationUnit() &&
               !llvm::isa<clang::NamespaceDecl>(ctx)) {
          ctx = ctx->getParent();
        }

        clang::NamespaceDecl *ns_decl =
            llvm::dyn_cast_or_null<clang::NamespaceDecl>(ctx);
        if (ns_decl == nullptr) {
          return nullptr;
        }

        SgScopeStatement *ns_scope = nullptr;
        auto it_decl = p_decl_translation_map.find(ns_decl);
        if (it_decl != p_decl_translation_map.end()) {
          if (SgNamespaceDeclarationStatement *ns_stmt =
                  isSgNamespaceDeclarationStatement(it_decl->second)) {
            ns_scope = ns_stmt->get_definition();
          } else if (SgNamespaceDefinitionStatement *ns_def =
                         isSgNamespaceDefinitionStatement(it_decl->second)) {
            ns_scope = ns_def;
          }
        }

        if (ns_scope == nullptr) {
          if (SgNamespaceDeclarationStatement *ns_stmt =
                  ensureNamespaceDeclaration(ns_decl)) {
            ns_scope = ns_stmt->get_definition();
          }
        }

        return normalizeNamespaceScope(ns_scope);
      };

      if (member_sym != nullptr) {
        SgMemberFunctionSymbol *ref_member_sym = member_sym;
        if (should_build_instantiation) {
          SgTemplateArgumentPtrList template_args;
          bool template_args_built = false;
          auto ensure_template_args = [&]() -> SgTemplateArgumentPtrList * {
            if (!template_args_built) {
              template_args = build_template_args_for_instantiation();
              template_args_built = true;
            }
            return &template_args;
          };

          SgMemberFunctionDeclaration *member_decl =
              isSgMemberFunctionDeclaration(member_sym->get_declaration());
          SgScopeStatement *func_scope =
              member_decl != nullptr ? member_decl->get_scope() : nullptr;
          if (func_scope == nullptr) {
            func_scope = SageBuilder::topScopeStack();
          }

          SgName template_base_name = member_sym->get_name();
          if (member_decl != nullptr) {
            if (SgTemplateInstantiationMemberFunctionDecl *existing_inst =
                    isSgTemplateInstantiationMemberFunctionDecl(member_decl)) {
              if (existing_inst->get_templateName().is_null() == false) {
                template_base_name = existing_inst->get_templateName();
              }
            } else if (SgTemplateInstantiationMemberFunctionDecl
                           *existing_inst =
                               isSgTemplateInstantiationMemberFunctionDecl(
                                   member_decl
                                       ->get_firstNondefiningDeclaration())) {
              if (existing_inst->get_templateName().is_null() == false) {
                template_base_name = existing_inst->get_templateName();
              }
            }
          }

          bool reused_instantiation = false;
          if (SgTemplateInstantiationMemberFunctionDecl *inst_decl =
                  isSgTemplateInstantiationMemberFunctionDecl(member_decl)) {
            inst_decl->set_template_argument_list_is_explicit(
                has_explicit_template_args);
            SageBuilder::setTemplateArgumentsInDeclaration(
                inst_decl, ensure_template_args());
            if (inst_decl->get_templateName().is_null() &&
                template_base_name.is_null() == false) {
              inst_decl->set_templateName(template_base_name);
            }
            reused_instantiation = true;
          } else if (member_decl != nullptr) {
            if (SgTemplateInstantiationMemberFunctionDecl *inst_decl =
                    isSgTemplateInstantiationMemberFunctionDecl(
                        member_decl->get_firstNondefiningDeclaration())) {
              inst_decl->set_template_argument_list_is_explicit(
                  has_explicit_template_args);
              SageBuilder::setTemplateArgumentsInDeclaration(
                  inst_decl, ensure_template_args());
              if (inst_decl->get_templateName().is_null() &&
                  template_base_name.is_null() == false) {
                inst_decl->set_templateName(template_base_name);
              }
              reused_instantiation = true;
            }
          }

          if (!reused_instantiation) {
            SgType *sym_type = member_sym->get_type();
            if (sym_type == nullptr && member_decl != nullptr) {
              sym_type = member_decl->get_type();
            }

            SgType *lookup_type = sym_type;
            if (clang::FunctionDecl *clang_func =
                    llvm::dyn_cast<clang::FunctionDecl>(
                        decl_ref_expr->getDecl())) {
              if (SgType *clang_type =
                      buildTypeFromQualifiedType(clang_func->getType())) {
                if (isSgMemberFunctionType(clang_type) != nullptr ||
                    lookup_type == nullptr) {
                  lookup_type = clang_type;
                }
              }
            }

            SgFunctionType *func_type = isSgFunctionType(lookup_type);
            SgFunctionSymbol *existing_inst_sym = nullptr;
            if (func_scope != nullptr) {
              existing_inst_sym = func_scope->lookup_function_symbol(
                  template_base_name, lookup_type, ensure_template_args());
              if (existing_inst_sym == nullptr && lookup_type != nullptr) {
                SgFunctionSymbol *fallback_sym =
                    func_scope->lookup_function_symbol(template_base_name,
                                                       lookup_type);
                if (fallback_sym != nullptr) {
                  if (isSgTemplateInstantiationMemberFunctionDecl(
                          fallback_sym->get_declaration()) != nullptr) {
                    existing_inst_sym = fallback_sym;
                  } else if (SgMemberFunctionDeclaration *first_nondef =
                                 isSgMemberFunctionDeclaration(
                                     fallback_sym->get_declaration()
                                         ->get_firstNondefiningDeclaration())) {
                    if (isSgTemplateInstantiationMemberFunctionDecl(
                            first_nondef) != nullptr) {
                      existing_inst_sym = fallback_sym;
                    }
                  }
                }
              }
            }

            SgMemberFunctionSymbol *inst_member_sym =
                isSgMemberFunctionSymbol(existing_inst_sym);
            SgTemplateInstantiationMemberFunctionDecl *inst_decl =
                inst_member_sym != nullptr
                    ? isSgTemplateInstantiationMemberFunctionDecl(
                          inst_member_sym->get_declaration())
                    : nullptr;
            if (inst_decl == nullptr && inst_member_sym != nullptr) {
              if (SgMemberFunctionDeclaration *first_nondef =
                      isSgMemberFunctionDeclaration(
                          inst_member_sym->get_declaration()
                              ->get_firstNondefiningDeclaration())) {
                inst_decl =
                    isSgTemplateInstantiationMemberFunctionDecl(first_nondef);
              }
            }

            if (inst_member_sym != nullptr && inst_decl != nullptr) {
              ensure_function_param_list(inst_decl, nullptr);
              ref_member_sym = inst_member_sym;
              inst_decl->set_template_argument_list_is_explicit(
                  has_explicit_template_args);
              SageBuilder::setTemplateArgumentsInDeclaration(
                  inst_decl, ensure_template_args());
              if (inst_decl->get_templateName().is_null() &&
                  template_base_name.is_null() == false) {
                inst_decl->set_templateName(template_base_name);
              }
              if (SgTemplateMemberFunctionDeclaration *tmpl_decl =
                      resolve_member_template_decl(member_decl, clang_func)) {
                apply_template_instantiation_template_links(
                    inst_decl, tmpl_decl, tmpl_decl->get_name());
              }
            } else {
              SgType *ret_type =
                  func_type != nullptr ? func_type->get_return_type() : nullptr;
              if (ret_type == nullptr) {
                ret_type = SageBuilder::buildUnknownType();
              }

              SgFunctionParameterList *param_list = nullptr;
              if (func_type != nullptr &&
                  func_type->get_argument_list() != nullptr) {
                param_list = SageBuilder::buildFunctionParameterList_nfi(
                    func_type->get_argument_list());
              } else {
                param_list = SageBuilder::buildFunctionParameterList_nfi();
              }

              unsigned int functionConstVolatileFlags = 0;
              if (SgMemberFunctionType *member_type =
                      isSgMemberFunctionType(sym_type)) {
                functionConstVolatileFlags = member_type->get_mfunc_specifier();
              }

              SgTemplateArgumentPtrList *builder_args = ensure_template_args();
              SgTemplateInstantiationMemberFunctionDecl *inst_decl =
                  isSgTemplateInstantiationMemberFunctionDecl(
                      SageBuilder::buildNondefiningMemberFunctionDeclaration(
                          template_base_name, ret_type, param_list, func_scope,
                          functionConstVolatileFlags,
                          /*buildTemplateInstantiation=*/true, builder_args));

              ensure_function_param_list(inst_decl, param_list);
              if (inst_decl != nullptr) {
                inst_decl->set_template_argument_list_is_explicit(
                    has_explicit_template_args);
                SageBuilder::setTemplateArgumentsInDeclaration(
                    inst_decl, ensure_template_args());

                if (SgTemplateMemberFunctionDeclaration *tmpl_decl =
                        resolve_member_template_decl(member_decl, clang_func)) {
                  apply_template_instantiation_template_links(
                      inst_decl, tmpl_decl, tmpl_decl->get_name());
                }

                finalize_instantiation_decl(inst_decl);
                registerDeclarationSymbol(inst_decl);
                SgMemberFunctionSymbol *inst_sym = isSgMemberFunctionSymbol(
                    inst_decl->get_symbol_from_symbol_table());
                if (inst_sym == nullptr) {
                  if (SgMemberFunctionDeclaration *first_nondef =
                          isSgMemberFunctionDeclaration(
                              inst_decl->get_firstNondefiningDeclaration())) {
                    inst_sym = isSgMemberFunctionSymbol(
                        first_nondef->get_symbol_from_symbol_table());
                  }
                }
                if (inst_sym == nullptr && func_scope != nullptr) {
                  SgFunctionSymbol *lookup_sym =
                      func_scope->lookup_function_symbol(
                          template_base_name, lookup_type,
                          ensure_template_args());
                  if (lookup_sym == nullptr && lookup_type != nullptr) {
                    lookup_sym = func_scope->lookup_function_symbol(
                        template_base_name, lookup_type);
                  }
                  if (lookup_sym != nullptr) {
                    if (SgTemplateInstantiationMemberFunctionDecl *lookup_decl =
                            isSgTemplateInstantiationMemberFunctionDecl(
                                lookup_sym->get_declaration())) {
                      inst_sym = isSgMemberFunctionSymbol(lookup_sym);
                    } else if (
                        SgMemberFunctionDeclaration *first_nondef =
                            isSgMemberFunctionDeclaration(
                                lookup_sym->get_declaration()
                                    ->get_firstNondefiningDeclaration())) {
                      if (isSgTemplateInstantiationMemberFunctionDecl(
                              first_nondef) != nullptr) {
                        inst_sym = isSgMemberFunctionSymbol(lookup_sym);
                      }
                    }
                  }
                }
                if (inst_sym == nullptr && func_scope != nullptr) {
                  inst_sym = new SgMemberFunctionSymbol(inst_decl);
                  attachSymbolToScopeOrOrphan(inst_sym, func_scope);
                }
                if (inst_sym != nullptr) {
                  ref_member_sym = inst_sym;
                }
              }
            }
          }
        }

        const bool need_qualifier = decl_ref_expr->hasQualifier();
        SgExpression *ref_exp = SageBuilder::buildMemberFunctionRefExp_nfi(
            ref_member_sym, false, need_qualifier);
        attach_explicit_qualifier(ref_exp);
        ensure_function_decl_symbol(
            isSgFunctionDeclaration(ref_member_sym->get_declaration()));
        *node = ref_exp;
      } else if (func_sym != nullptr) {
        if (!should_build_instantiation && has_explicit_template_args &&
            decl_ref_expr->hasQualifier()) {
          // Preserve qualified explicit template-id spellings (e.g.
          // `::foo<int>`) without forcing synthetic instantiation symbols.
          SgTemplateArgumentPtrList template_args =
              buildTemplateArguments(explicit_arg_info, true);
          const SgTemplateArgumentPtrList *template_args_ptr =
              template_args.empty() ? nullptr : &template_args;
          SgScopeStatement *scope = SageBuilder::topScopeStack();
          if (scope == nullptr) {
            scope = getGlobalScope();
          }
          SgExpression *ref_exp = buildNonrealRefExpFromNestedNameSpecifier(
              structuralQualifierForExplicitlyQualifiedNonrealRef(
                  decl_ref_qualifier),
              scope, SgName(decl_ref_expr->getDecl()->getNameAsString()),
              decl_ref_expr->hasTemplateKeyword(), template_args_ptr);
          attach_explicit_qualifier(ref_exp);
          *node = ref_exp;
          applySourceRange(*node, decl_ref_expr->getSourceRange());
          return VisitExpr(decl_ref_expr, node) && res;
        }

        SgFunctionSymbol *ref_func_sym = func_sym;
        if (should_build_instantiation) {
          SgTemplateArgumentPtrList template_args;
          bool template_args_built = false;
          auto ensure_template_args = [&]() -> SgTemplateArgumentPtrList * {
            if (!template_args_built) {
              template_args = build_template_args_for_instantiation();
              template_args_built = true;
            }
            return &template_args;
          };

          SgScopeStatement *func_scope = func_sym->get_scope();
          if (SgScopeStatement *decl_scope =
                  get_decl_namespace_scope(decl_ref_expr->getFoundDecl())) {
            func_scope = decl_scope;
          } else if (SgScopeStatement *decl_scope = get_decl_namespace_scope(
                         llvm::dyn_cast_or_null<clang::NamedDecl>(
                             decl_ref_expr->getDecl()))) {
            func_scope = decl_scope;
          }
          if (func_scope == nullptr) {
            func_scope = SageBuilder::topScopeStack();
          }

          SgFunctionDeclaration *decl =
              isSgFunctionDeclaration(func_sym->get_declaration());
          SgName template_base_name = func_sym->get_name();
          if (decl != nullptr) {
            if (SgTemplateInstantiationFunctionDecl *existing_inst =
                    isSgTemplateInstantiationFunctionDecl(decl)) {
              if (existing_inst->get_templateName().is_null() == false) {
                template_base_name = existing_inst->get_templateName();
              }
            } else if (SgTemplateInstantiationFunctionDecl *existing_inst =
                           isSgTemplateInstantiationFunctionDecl(
                               decl->get_firstNondefiningDeclaration())) {
              if (existing_inst->get_templateName().is_null() == false) {
                template_base_name = existing_inst->get_templateName();
              }
            }
          }

          bool reused_instantiation = false;
          if (SgTemplateInstantiationFunctionDecl *inst_decl =
                  isSgTemplateInstantiationFunctionDecl(decl)) {
            inst_decl->set_template_argument_list_is_explicit(
                has_explicit_template_args);
            SageBuilder::setTemplateArgumentsInDeclaration(
                inst_decl, ensure_template_args());
            if (inst_decl->get_templateName().is_null() &&
                template_base_name.is_null() == false) {
              inst_decl->set_templateName(template_base_name);
            }
            if (SgTemplateFunctionDeclaration *tmpl_decl =
                    resolve_function_template_decl(decl, clang_func)) {
              apply_template_instantiation_template_links(
                  inst_decl, tmpl_decl, tmpl_decl->get_name());
            }
            reused_instantiation = true;
          } else if (decl != nullptr) {
            if (SgTemplateInstantiationFunctionDecl *inst_decl =
                    isSgTemplateInstantiationFunctionDecl(
                        decl->get_firstNondefiningDeclaration())) {
              inst_decl->set_template_argument_list_is_explicit(
                  has_explicit_template_args);
              SageBuilder::setTemplateArgumentsInDeclaration(
                  inst_decl, ensure_template_args());
              if (inst_decl->get_templateName().is_null() &&
                  template_base_name.is_null() == false) {
                inst_decl->set_templateName(template_base_name);
              }
              if (SgTemplateFunctionDeclaration *tmpl_decl =
                      resolve_function_template_decl(decl, clang_func)) {
                apply_template_instantiation_template_links(
                    inst_decl, tmpl_decl, tmpl_decl->get_name());
              }
              reused_instantiation = true;
            }
          }

          if (!reused_instantiation) {
            SgType *lookup_type = func_sym->get_type();
            if (clang::FunctionDecl *clang_func =
                    llvm::dyn_cast<clang::FunctionDecl>(
                        decl_ref_expr->getDecl())) {
              if (SgType *clang_type =
                      buildTypeFromQualifiedType(clang_func->getType())) {
                if (isSgFunctionType(clang_type) != nullptr ||
                    lookup_type == nullptr) {
                  lookup_type = clang_type;
                }
              }
            }

            SgFunctionType *func_type = isSgFunctionType(lookup_type);
            SgFunctionSymbol *existing_inst_sym = nullptr;
            if (func_scope != nullptr) {
              existing_inst_sym = func_scope->lookup_function_symbol(
                  template_base_name, lookup_type, ensure_template_args());
              if (existing_inst_sym == nullptr && lookup_type != nullptr) {
                SgFunctionSymbol *fallback_sym =
                    func_scope->lookup_function_symbol(template_base_name,
                                                       lookup_type);
                if (fallback_sym != nullptr) {
                  if (isSgTemplateInstantiationFunctionDecl(
                          fallback_sym->get_declaration()) != nullptr) {
                    existing_inst_sym = fallback_sym;
                  } else if (SgFunctionDeclaration *first_nondef =
                                 isSgFunctionDeclaration(
                                     fallback_sym->get_declaration()
                                         ->get_firstNondefiningDeclaration())) {
                    if (isSgTemplateInstantiationFunctionDecl(first_nondef) !=
                        nullptr) {
                      existing_inst_sym = fallback_sym;
                    }
                  }
                }
              }
            }

            SgFunctionSymbol *inst_func_sym =
                isSgFunctionSymbol(existing_inst_sym);
            SgTemplateInstantiationFunctionDecl *inst_decl =
                inst_func_sym != nullptr
                    ? isSgTemplateInstantiationFunctionDecl(
                          inst_func_sym->get_declaration())
                    : nullptr;
            if (inst_decl == nullptr && inst_func_sym != nullptr) {
              if (SgFunctionDeclaration *first_nondef = isSgFunctionDeclaration(
                      inst_func_sym->get_declaration()
                          ->get_firstNondefiningDeclaration())) {
                inst_decl = isSgTemplateInstantiationFunctionDecl(first_nondef);
              }
            }

            if (inst_func_sym != nullptr && inst_decl != nullptr) {
              ensure_function_param_list(inst_decl, nullptr);
              ref_func_sym = inst_func_sym;
              inst_decl->set_template_argument_list_is_explicit(
                  has_explicit_template_args);
              SageBuilder::setTemplateArgumentsInDeclaration(
                  inst_decl, ensure_template_args());
              if (inst_decl->get_templateName().is_null() &&
                  template_base_name.is_null() == false) {
                inst_decl->set_templateName(template_base_name);
              }
              if (SgTemplateFunctionDeclaration *tmpl_decl =
                      resolve_function_template_decl(
                          isSgFunctionDeclaration(func_sym->get_declaration()),
                          clang_func)) {
                apply_template_instantiation_template_links(
                    inst_decl, tmpl_decl, tmpl_decl->get_name());
              }
            } else {
              SgType *ret_type =
                  func_type != nullptr ? func_type->get_return_type() : nullptr;
              if (ret_type == nullptr) {
                ret_type = SageBuilder::buildUnknownType();
              }

              SgFunctionParameterList *param_list = nullptr;
              if (func_type != nullptr &&
                  func_type->get_argument_list() != nullptr) {
                param_list = SageBuilder::buildFunctionParameterList_nfi(
                    func_type->get_argument_list());
              } else {
                param_list = SageBuilder::buildFunctionParameterList_nfi();
              }

              SgTemplateArgumentPtrList *builder_args = ensure_template_args();
              SgTemplateInstantiationFunctionDecl *inst_decl =
                  isSgTemplateInstantiationFunctionDecl(
                      SageBuilder::buildNondefiningFunctionDeclaration(
                          template_base_name, ret_type, param_list, func_scope,
                          /*buildTemplateInstantiation=*/true, builder_args,
                          SgStorageModifier::e_default,
                          /*forceFreeFunctionScope=*/false));

              ensure_function_param_list(inst_decl, param_list);
              if (inst_decl != nullptr) {
                inst_decl->set_template_argument_list_is_explicit(
                    has_explicit_template_args);
                SageBuilder::setTemplateArgumentsInDeclaration(
                    inst_decl, ensure_template_args());

                if (SgTemplateFunctionDeclaration *tmpl_decl =
                        resolve_function_template_decl(
                            isSgFunctionDeclaration(
                                func_sym->get_declaration()),
                            clang_func)) {
                  apply_template_instantiation_template_links(
                      inst_decl, tmpl_decl, tmpl_decl->get_name());
                }

                finalize_instantiation_decl(inst_decl);
                registerDeclarationSymbol(inst_decl);
                SgFunctionSymbol *inst_sym = isSgFunctionSymbol(
                    inst_decl->get_symbol_from_symbol_table());
                if (inst_sym == nullptr) {
                  if (SgFunctionDeclaration *first_nondef =
                          isSgFunctionDeclaration(
                              inst_decl->get_firstNondefiningDeclaration())) {
                    inst_sym = isSgFunctionSymbol(
                        first_nondef->get_symbol_from_symbol_table());
                  }
                }
                if (inst_sym == nullptr && func_scope != nullptr) {
                  inst_sym = func_scope->lookup_function_symbol(
                      template_base_name, lookup_type, ensure_template_args());
                  if (inst_sym == nullptr && lookup_type != nullptr) {
                    inst_sym = func_scope->lookup_function_symbol(
                        template_base_name, lookup_type);
                  }
                  if (inst_sym != nullptr) {
                    if (isSgTemplateInstantiationFunctionDecl(
                            inst_sym->get_declaration()) == nullptr) {
                      if (SgFunctionDeclaration *first_nondef =
                              isSgFunctionDeclaration(
                                  inst_sym->get_declaration()
                                      ->get_firstNondefiningDeclaration())) {
                        if (isSgTemplateInstantiationFunctionDecl(
                                first_nondef) == nullptr) {
                          inst_sym = nullptr;
                        }
                      } else {
                        inst_sym = nullptr;
                      }
                    }
                  }
                }
                if (inst_sym == nullptr && func_scope != nullptr) {
                  inst_sym = new SgFunctionSymbol(inst_decl);
                  attachSymbolToScopeOrOrphan(inst_sym, func_scope);
                }
                if (inst_sym != nullptr) {
                  ref_func_sym = inst_sym;
                }
              }
            }
          }
        }
        SgExpression *ref_exp = SageBuilder::buildFunctionRefExp(ref_func_sym);
        attach_explicit_qualifier(ref_exp);
        ensure_function_decl_symbol(
            isSgFunctionDeclaration(ref_func_sym->get_declaration()));
        *node = ref_exp;
      } else {
        if (enum_sym != nullptr) {
          // ROOT CAUSE FIX: Get enum declaration from the type instead of
          // parent The Clang frontend may not set parent pointers
          // correctly for enum constants But the type is always set
          // correctly to an SgEnumType
          SgInitializedName *init_name = enum_sym->get_declaration();
          SgEnumDeclaration *enum_decl = nullptr;
          long long enum_value = 0;

          if (init_name != nullptr && init_name->get_type() != nullptr) {
            SgEnumType *enum_type = isSgEnumType(init_name->get_type());
            if (enum_type != nullptr) {
              enum_decl = isSgEnumDeclaration(enum_type->get_declaration());
            }
          }

          // Fallback: try getting from parent if type method didn't work
          if (enum_decl == nullptr && init_name != nullptr) {
            enum_decl = isSgEnumDeclaration(init_name->get_parent());
          }

          if (const clang::EnumConstantDecl *enum_const_decl =
                  llvm::dyn_cast<clang::EnumConstantDecl>(
                      decl_ref_expr->getDecl())) {
            enum_value = enum_const_decl->getInitVal().getExtValue();
          }

          ROSE_ASSERT(enum_decl != nullptr);
          SgName name(decl_ref_expr->getNameInfo().getName().getAsString());
          SgExpression *ref_exp =
              SageBuilder::buildEnumVal_nfi(enum_value, enum_decl, name);
          attach_explicit_qualifier(ref_exp);
          *node = ref_exp;
        } else {
          if (sym != nullptr) {
            std::cerr << "Runtime error: Unknown type of symbol for a "
                         "declaration reference."
                      << std::endl;
            std::cerr << "    sym->class_name() = " << sym->class_name()
                      << std::endl;
            ROSE_ABORT();
          }
        }
      }
    }
  } else {
    // ROOT CAUSE FIX: Handle template-dependent and unresolved declarations
    clang::Decl *clang_decl = decl_ref_expr->getDecl();
    std::string decl_name = "unresolved_symbol";

    // Get declaration name and type info for better handling
    if (clang_decl && clang::isa<clang::NamedDecl>(clang_decl)) {
      clang::NamedDecl *named_decl = clang::cast<clang::NamedDecl>(clang_decl);
      decl_name = named_decl->getNameAsString();

      // Log what type of declaration couldn't be resolved
      std::cerr << "Warning: Cannot resolve symbol for "
                << clang_decl->getDeclKindName() << " '" << decl_name
                << "', using placeholder" << std::endl;
    } else {
      std::cerr << "Warning: Cannot resolve symbol for declaration reference, "
                   "using placeholder"
                << std::endl;
    }

    SgScopeStatement *current_scope = SageBuilder::topScopeStack();
    if (current_scope == nullptr) {
      current_scope = getGlobalScope();
    }
    ROSE_ASSERT(current_scope != nullptr);

    SgTemplateArgumentPtrList template_args;
    const SgTemplateArgumentPtrList *template_args_ptr = nullptr;
    if (decl_ref_expr->hasExplicitTemplateArgs()) {
      clang::TemplateArgumentListInfo arg_info;
      decl_ref_expr->copyTemplateArgumentsInto(arg_info);
      template_args = buildTemplateArguments(arg_info, true);
      template_args_ptr = &template_args;
    }

    SgExpression *ref_exp = buildNonrealRefExpFromNestedNameSpecifier(
        structuralQualifierForExplicitlyQualifiedNonrealRef(decl_ref_qualifier),
        current_scope, SgName(decl_name), decl_ref_expr->hasTemplateKeyword(),
        template_args_ptr);
    attach_explicit_qualifier(ref_exp);
    *node = ref_exp;
  }

  applySourceRange(*node, decl_ref_expr->getSourceRange());
  return VisitExpr(decl_ref_expr, node) && res;
}

bool ClangToSageTranslator::VisitDependentCoawaitExpr(
    clang::DependentCoawaitExpr *dependent_coawait_expr, SgNode **node) {
#if DEBUG_VISIT_STMT
  std::cerr << "ClangToSageTranslator::VisitDependentCoawaitExpr" << std::endl;
#endif
  bool res =
      buildCoroutineAwaitExpression(dependent_coawait_expr->getOperand(),
                                    dependent_coawait_expr->getSourceRange(),
                                    "dependent co_await operand", node);
  if (res) {
    setCoroutineKeywordAttribute(*node, "co_await");
  }

  return VisitExpr(dependent_coawait_expr, node) && res;
}

bool ClangToSageTranslator::VisitDependentScopeDeclRefExpr(
    clang::DependentScopeDeclRefExpr *dependent_scope_decl_ref_expr,
    SgNode **node) {
#if DEBUG_VISIT_STMT
  std::cerr << "ClangToSageTranslator::VisitDependentScopeDeclRefExpr"
            << std::endl;
#endif
  bool res = true;

  // DependentScopeDeclRefExpr represents a reference to a declaration that
  // depends on template parameters (e.g., variable references like 'x' or 'y'
  // in template-dependent contexts).

  std::string decl_name =
      dependent_scope_decl_ref_expr->getDeclName().getAsString();

  SgScopeStatement *current_scope = SageBuilder::topScopeStack();
  if (current_scope == nullptr) {
    current_scope = getGlobalScope();
  }
  ROSE_ASSERT(current_scope != nullptr);

  decl_name = resolve_synthetic_template_param_name_in_stmt_scope(
      decl_name, current_scope);

  SgTemplateArgumentPtrList template_args;
  const SgTemplateArgumentPtrList *template_args_ptr = nullptr;
  if (dependent_scope_decl_ref_expr->hasExplicitTemplateArgs()) {
    clang::TemplateArgumentListInfo arg_info;
    dependent_scope_decl_ref_expr->copyTemplateArgumentsInto(arg_info);
    template_args = buildTemplateArguments(arg_info, true);
    template_args_ptr = &template_args;
  }

  clang::NestedNameSpecifierLoc qualifier_loc =
      dependent_scope_decl_ref_expr->getQualifierLoc();
  const clang::NestedNameSpecifierLoc *qualifier_loc_ptr =
      qualifier_loc.getNestedNameSpecifier() ? &qualifier_loc : nullptr;

  clang::NestedNameSpecifier lookup_qualifier =
      dependent_scope_decl_ref_expr->getQualifier();
  if (!lookup_qualifier && qualifier_loc_ptr != nullptr) {
    lookup_qualifier = qualifier_loc_ptr->getNestedNameSpecifier();
  }

  *node = buildNonrealRefExpFromNestedNameSpecifier(
      structuralQualifierForExplicitlyQualifiedNonrealRef(lookup_qualifier),
      current_scope, SgName(decl_name),
      dependent_scope_decl_ref_expr->hasTemplateKeyword(), template_args_ptr);

  // Set source position
  SgExpression *expr = isSgExpression(*node);
  if (expr != nullptr) {
    attachExplicitQualifierFromNestedName(
        expr, lookup_qualifier, qualifier_loc_ptr, p_compiler_instance);
    applySourceRange(expr, dependent_scope_decl_ref_expr->getSourceRange());
  }

  return VisitExpr(dependent_scope_decl_ref_expr, node) && res;
}

// bool
// ClangToSageTranslator::VisitDependentScopeDeclRefExpr(clang::DependentScopeDeclRefExpr
// * dependent_scope_decl_ref_expr);

bool ClangToSageTranslator::VisitDesignatedInitExpr(
    clang::DesignatedInitExpr *designated_init_expr, SgNode **node) {
#if DEBUG_VISIT_STMT
  std::cerr << "ClangToSageTranslator::VisitDesignatedInitExpr" << std::endl;
#endif

  SgInitializer *base_init = nullptr;
  SgDesignatedInitializer *designated_init = nullptr;
  SgExprListExp *expr_list_exp = nullptr;
  {
    SgNode *tmp_expr = Traverse(designated_init_expr->getInit());
    SgExpression *expr = isSgExpression(tmp_expr);
    ROSE_ASSERT(expr != nullptr);
    SgExprListExp *expr_list_exp = isSgExprListExp(expr);
    if (expr_list_exp != nullptr) {
      // FIXME get the type right...
      base_init =
          SageBuilder::buildAggregateInitializer_nfi(expr_list_exp, nullptr);
    } else {
      base_init =
          SageBuilder::buildAssignInitializer_nfi(expr, expr->get_type());
    }
    ROSE_ASSERT(base_init != nullptr);
    applySourceRange(base_init,
                     designated_init_expr->getInit()->getSourceRange());
  }

  /* Pei-Hung (06/10/2022) revision to handle Initializer in test2013_37.c
   *  After calling getSyntacticForm from InitListExpr, the type and
   * multidimensional array hierarchy is missing. This version can construct the
   * array structure but need additional support to grab the type structure from
   *  parent AST node, such as VarDecl.
   */

  auto designatorSize = designated_init_expr->size();

  for (auto it = designatorSize; it > 0; it--) {
    expr_list_exp = SageBuilder::buildExprListExp_nfi();
    SgExpression *expr = nullptr;
    clang::DesignatedInitExpr::Designator *D =
        designated_init_expr->getDesignator(it - 1);
    if (D->isFieldDesignator()) {
      // getField() was renamed to getFieldDecl()
      clang::FieldDecl *field_decl = D->getFieldDecl();
      ROSE_ASSERT(field_decl != nullptr);
      SgSymbol *symbol = GetSymbolFromSymbolTable(field_decl);
      SgVariableSymbol *var_sym = isSgVariableSymbol(symbol);
      if (var_sym == nullptr &&
          p_decl_translation_in_progress.find(field_decl) ==
              p_decl_translation_in_progress.end()) {
        TraverseOnDemand(field_decl);
        symbol = GetSymbolFromSymbolTable(field_decl);
        var_sym = isSgVariableSymbol(symbol);
      }
      if (var_sym == nullptr) {
        SgNode *field_node = nullptr;
        auto it_decl = p_decl_translation_map.find(field_decl);
        if (it_decl != p_decl_translation_map.end()) {
          field_node = it_decl->second;
        } else {
          field_node = Traverse(field_decl);
        }
        SgInitializedName *init_name = isSgInitializedName(field_node);
        if (init_name == nullptr) {
          if (SgVariableDeclaration *var_decl =
                  isSgVariableDeclaration(field_node)) {
            init_name = SageInterface::getFirstInitializedName(var_decl);
          }
        }
        ROSE_ASSERT(init_name != nullptr);
        SgScopeStatement *decl_scope = init_name->get_scope();
        if (decl_scope == nullptr) {
          decl_scope = resolveScopeFromDeclContext(
              field_decl->getDeclContext(), SageBuilder::topScopeStack());
          if (decl_scope != nullptr) {
            init_name->set_scope(decl_scope);
          }
        }
        ROSE_ASSERT(decl_scope != nullptr);
        var_sym = decl_scope->lookup_variable_symbol(init_name->get_name());
        if (var_sym == nullptr) {
          var_sym = new SgVariableSymbol(init_name);
          attachSymbolToScopeOrOrphan(var_sym, decl_scope);
        }
      }
      ROSE_ASSERT(var_sym != nullptr);
      expr = SageBuilder::buildVarRefExp_nfi(var_sym);
    } else if (D->isArrayDesignator()) {
      SgNode *tmp_expr = Traverse(designated_init_expr->getArrayIndex(*D));
      expr = isSgExpression(tmp_expr);
      ROSE_ASSERT(expr != nullptr);

    } else if (D->isArrayRangeDesignator()) {
      SgExpression *lower_bound = isSgExpression(
          Traverse(designated_init_expr->getArrayRangeStart(*D)));
      SgExpression *upper_bound =
          isSgExpression(Traverse(designated_init_expr->getArrayRangeEnd(*D)));
      ROSE_ASSERT(lower_bound != nullptr);
      ROSE_ASSERT(upper_bound != nullptr);

      SgExpression *stride = SageBuilder::buildIntVal(1);
      SgSubscriptExpression *range_designator =
          SageBuilder::buildSubscriptExpression_nfi(lower_bound, upper_bound,
                                                    stride);
      lower_bound->set_parent(range_designator);
      upper_bound->set_parent(range_designator);
      stride->set_parent(range_designator);

      expr = range_designator;
    } else {
      ROSE_ABORT();
    }

    ROSE_ASSERT(expr != nullptr);

    applySourceRange(expr, D->getSourceRange());
    expr->set_parent(expr_list_exp);
    expr_list_exp->append_expression(expr);
    if (it > 1) {
      SgDesignatedInitializer *design_init =
          new SgDesignatedInitializer(expr_list_exp, base_init);
      applySourceRange(design_init,
                       designated_init_expr->getDesignatorsSourceRange());
      design_init->post_construction_initialization();
      SgExprListExp *aggListExp = SageBuilder::buildExprListExp_nfi();
      design_init->set_parent(aggListExp);
      aggListExp->append_expression(design_init);
      SgAggregateInitializer *newAggInit =
          SageBuilder::buildAggregateInitializer_nfi(aggListExp, nullptr);
      expr_list_exp = SageBuilder::buildExprListExp_nfi();
      base_init = newAggInit;
    }
  }

  applySourceRange(expr_list_exp,
                   designated_init_expr->getDesignatorsSourceRange());
  designated_init = new SgDesignatedInitializer(expr_list_exp, base_init);
  designated_init->post_construction_initialization();

  *node = designated_init;

  return VisitExpr(designated_init_expr, node);

  // Pei-Hung (06/10/2022) keep the original implementation which has the array
  // information stored in the list
  /*
      for (auto it=0; it < designatorSize; it++) {
          SgExpression * expr = nullptr;
          clang::DesignatedInitExpr::Designator * D =
  designated_init_expr->getDesignator(it); if (D->isFieldDesignator()) {
              SgSymbol * symbol = GetSymbolFromSymbolTable(D->getField());
              SgVariableSymbol * var_sym = isSgVariableSymbol(symbol);
              ROSE_ASSERT(var_sym != nullptr);
              expr = SageBuilder::buildVarRefExp_nfi(var_sym);
              applySourceRange(expr, D->getSourceRange());
          }
          else if (D->isArrayDesignator()) {
              SgNode * tmp_expr = nullptr;
              if(clang::ConstantExpr::classof(designated_init_expr->getArrayIndex(*D)))
              {
                 clang::FullExpr* fullExpr = (clang::FullExpr*)
  designated_init_expr->getArrayIndex(*D); clang::IntegerLiteral* integerLiteral
  = (clang::IntegerLiteral*) fullExpr->getSubExpr(); tmp_expr =
  SageBuilder::buildUnsignedLongVal((unsigned long)
  integerLiteral->getValue().getSExtValue()); std::cerr << "idx:" <<
  integerLiteral->getValue().getSExtValue() << std::endl;
              }
              else
              {
                 tmp_expr = Traverse(designated_init_expr->getArrayIndex(*D));
              }
              expr = isSgExpression(tmp_expr);
              ROSE_ASSERT(expr != nullptr);
          }
          else if (D->isArrayRangeDesignator()) {
              ROSE_ASSERT(!"I don't believe range designator initializer are
  supported by ROSE...");
          }
          else ROSE_ABORT();

          ROSE_ASSERT(expr != nullptr);

          expr->set_parent(expr_list_exp);
          expr_list_exp->append_expression(expr);
      }

      applySourceRange(expr_list_exp,
  designated_init_expr->getDesignatorsSourceRange());

      SgDesignatedInitializer * design_init = new
  SgDesignatedInitializer(expr_list_exp, init);
      expr_list_exp->set_parent(design_init);
      init->set_parent(design_init);

      *node = design_init;

      return VisitExpr(designated_init_expr, node);
  */
}

bool ClangToSageTranslator::VisitDesignatedInitUpdateExpr(
    clang::DesignatedInitUpdateExpr *designated_init_update, SgNode **node) {
#if DEBUG_VISIT_STMT
  std::cerr << "ClangToSageTranslator::VisitDesignatedInitUpdateExpr"
            << std::endl;
#endif
  bool res = true;

  // TODO

  return VisitExpr(designated_init_update, node) && res;
}

bool ClangToSageTranslator::VisitExpressionTraitExpr(
    clang::ExpressionTraitExpr *expression_trait_expr, SgNode **node) {
#if DEBUG_VISIT_STMT
  std::cerr << "ClangToSageTranslator::VisitExpressionTraitExpr" << std::endl;
#endif
  bool res = true;

  // TODO

  return VisitExpr(expression_trait_expr, node) && res;
}

bool ClangToSageTranslator::VisitExtVectorElementExpr(
    clang::ExtVectorElementExpr *ext_vector_element_expr, SgNode **node) {
#if DEBUG_VISIT_STMT
  std::cerr << "ClangToSageTranslator::VisitExtVectorElementExpr" << std::endl;
#endif

  SgNode *tmp_base = Traverse(ext_vector_element_expr->getBase());
  SgExpression *base = isSgExpression(tmp_base);
#if DEBUG_VISIT_STMT
  // if (base) std::cerr << "DEBUG: VisitExtVectorElementExpr base node type: "
  // << base->class_name() << std::endl; else std::cerr << "DEBUG:
  // VisitExtVectorElementExpr base node is nullptr" << std::endl;
#endif
  ROSE_ASSERT(base != nullptr);

  SgType *type = buildTypeFromQualifiedType(ext_vector_element_expr->getType());

  clang::IdentifierInfo &ident_info = ext_vector_element_expr->getAccessor();
  std::string ident = ident_info.getName().str();

  SgScopeStatement *scope = SageBuilder::ScopeStack.front();
  SgGlobal *global = isSgGlobal(scope);
  ROSE_ASSERT(global != nullptr);

  // Build Manually a SgVarRefExp to have the same Accessor (text version) TODO
  // ExtVectorAccessor and ExtVectorType
  SgInitializedName *init_name = SageBuilder::buildInitializedName(
      ident, SageBuilder::buildVoidType(), nullptr);
  setCompilerGeneratedFileInfo(init_name);
  init_name->set_scope(global);
  SgVariableSymbol *var_symbol = new SgVariableSymbol(init_name);
  attachSymbolToScopeOrOrphan(var_symbol, nullptr);
  SgVarRefExp *pseudo_field = new SgVarRefExp(var_symbol);
  setCompilerGeneratedFileInfo(pseudo_field, true);
  init_name->set_parent(pseudo_field);

  SgExpression *res = nullptr;
  if (ext_vector_element_expr->isArrow())
    res = SageBuilder::buildArrowExp(base, pseudo_field);
  else
    res = SageBuilder::buildDotExp(base, pseudo_field);

  ROSE_ASSERT(res != nullptr);

  *node = res;

  return VisitExpr(ext_vector_element_expr, node);
}

bool ClangToSageTranslator::VisitFixedPointLiteral(
    clang::FixedPointLiteral *fixed_point_literal, SgNode **node) {
#if DEBUG_VISIT_STMT
  std::cerr << "ClangToSageTranslator::VisitFixedPointLiteral" << std::endl;
#endif
  bool res = true;

  // TODO

  return VisitExpr(fixed_point_literal, node) && res;
}

bool ClangToSageTranslator::VisitFloatingLiteral(
    clang::FloatingLiteral *floating_literal, SgNode **node) {
#if DEBUG_VISIT_STMT
  std::cerr << "ClangToSageTranslator::VisitFloatingLiteral" << std::endl;
#endif

  std::string spelling = getFloatingLiteralSpelling(
      floating_literal, p_compiler_instance->getSourceManager(),
      p_compiler_instance->getLangOpts());

  unsigned int precision = llvm::APFloat::semanticsPrecision(
      floating_literal->getValue().getSemantics());
  if (precision == 24) {
    // 32-bit float
    *node = SageBuilder::buildFloatVal(
        floating_literal->getValue().convertToFloat());
  } else if (precision == 53) {
    // 64-bit double
    *node = SageBuilder::buildDoubleVal(
        floating_literal->getValue().convertToDouble());
  } else if (precision == 64) {
    // 80-bit long double
    // Use APFloat conversion to avoid std::stold throwing on extreme literals.
#if defined(HAS_IEE754_FLOAT128)
    long double value =
        static_cast<long double>(floating_literal->getValue().convertToQuad());
#else
    long double value = static_cast<long double>(
        floating_literal->getValue().convertToDouble());
#endif
    *node = SageBuilder::buildLongDoubleVal(value);
  } else if (precision == 113) {
    // 128-bit long double - use double as approximation
    *node = SageBuilder::buildLongDoubleVal(
        floating_literal->getValue().convertToDouble());
  } else if (precision == 11) {
    // 16-bit half precision - use float
    *node = SageBuilder::buildFloatVal(
        floating_literal->getValue().convertToFloat());
  } else {
    // Fallback for other sizes - use double
    std::cerr << "Warning: Unsupported float precision " << precision
              << ", using double" << std::endl;
    *node = SageBuilder::buildDoubleVal(
        floating_literal->getValue().convertToDouble());
  }

  if (!spelling.empty()) {
    if (SgFloatVal *float_val = isSgFloatVal(*node)) {
      float_val->set_valueString(spelling);
    } else if (SgDoubleVal *double_val = isSgDoubleVal(*node)) {
      double_val->set_valueString(spelling);
    } else if (SgLongDoubleVal *long_double_val = isSgLongDoubleVal(*node)) {
      long_double_val->set_valueString(spelling);
    }
  }

  applySourceRange(*node, floating_literal->getSourceRange());
  return VisitExpr(floating_literal, node);
}

bool ClangToSageTranslator::VisitFullExpr(clang::FullExpr *full_expr,
                                          SgNode **node) {
#if DEBUG_VISIT_STMT
  std::cerr << "ClangToSageTranslator::VisitFullExpr" << std::endl;
#endif
  bool res = true;

  SgNode *tmp_expr = Traverse(full_expr->getSubExpr());
  SgExpression *expr = isSgExpression(tmp_expr);

#if DEBUG_VISIT_STMT
  std::cerr << "In VisitFullExpr(): built: expr = " << expr << " = "
            << expr->class_name().c_str() << std::endl;
#endif

  *node = expr;

  // TODO

  return VisitExpr(full_expr, node) && res;
}

bool ClangToSageTranslator::VisitConstantExpr(
    clang::ConstantExpr *constant_expr, SgNode **node) {
#if DEBUG_VISIT_STMT
  std::cerr << "ClangToSageTranslator::VisitConstantExpr" << std::endl;
#endif
  bool res = true;

  // TODO

  return VisitFullExpr(constant_expr, node) && res;
}

bool ClangToSageTranslator::VisitExprWithCleanups(
    clang::ExprWithCleanups *expr_with_cleanups, SgNode **node) {
#if DEBUG_VISIT_STMT
  std::cerr << "ClangToSageTranslator::VisitExprWithCleanups" << std::endl;
#endif
  bool res = true;

  // TODO

  return VisitFullExpr(expr_with_cleanups, node) && res;
}

bool ClangToSageTranslator::VisitFunctionParmPackExpr(
    clang::FunctionParmPackExpr *function_parm_pack_expr, SgNode **node) {
#if DEBUG_VISIT_STMT
  std::cerr << "ClangToSageTranslator::VisitFunctionParmPackExpr" << std::endl;
#endif
  bool res = true;

  // TODO

  return VisitExpr(function_parm_pack_expr, node) && res;
}

bool ClangToSageTranslator::VisitGenericSelectionExpr(
    clang::GenericSelectionExpr *generic_Selection_expr, SgNode **node) {
#if DEBUG_VISIT_STMT
  std::cerr << "ClangToSageTranslator::VisitGenericSelectionExpr" << std::endl;
#endif
  SgNode *result = Traverse(generic_Selection_expr->getResultExpr());
  SgExpression *result_expr = isSgExpression(result);
  ROSE_ASSERT(result_expr != nullptr);

  *node = result_expr;

  return VisitExpr(generic_Selection_expr, node);
}

bool ClangToSageTranslator::VisitGNUNullExpr(clang::GNUNullExpr *gnu_null_expr,
                                             SgNode **node) {
#if DEBUG_VISIT_STMT
  std::cerr << "ClangToSageTranslator::VisitGNUNullExpr" << std::endl;
#endif
  bool res = true;

  // ROOT CAUSE FIX: GNUNullExpr is GNU's __null extension, which represents a
  // null pointer constant It has type long (or long long on 64-bit) but behaves
  // as a null pointer Create an integer literal with value 0, VisitExpr will
  // handle the type
  *node = SageBuilder::buildIntVal(0);

  return VisitExpr(gnu_null_expr, node) && res;
}

bool ClangToSageTranslator::VisitImaginaryLiteral(
    clang::ImaginaryLiteral *imaginary_literal, SgNode **node) {
#if DEBUG_VISIT_STMT
  std::cerr << "ClangToSageTranslator::VisitImaginaryLiteral" << std::endl;
#endif

  SgNode *tmp_imag_val = Traverse(imaginary_literal->getSubExpr());
  SgValueExp *imag_val = isSgValueExp(tmp_imag_val);
  ROSE_ASSERT(imag_val != nullptr);

  SgComplexVal *comp_val =
      new SgComplexVal(nullptr, imag_val, imag_val->get_type(), "");

  *node = comp_val;

  return VisitExpr(imaginary_literal, node);
}

bool ClangToSageTranslator::VisitImplicitValueInitExpr(
    clang::ImplicitValueInitExpr *implicit_value_init_expr, SgNode **node) {
#if DEBUG_VISIT_STMT
  std::cerr << "ClangToSageTranslator::VisitImplicitValueInitExpr" << std::endl;
#endif
  bool res = true;

  SgType *type =
      buildTypeFromQualifiedType(implicit_value_init_expr->getType());
  ROSE_ASSERT(type != nullptr);
  ROSE_ASSERT(isSgReferenceType(type) == nullptr);

  SgType *stripped =
      type->stripType(SgType::STRIP_MODIFIER_TYPE | SgType::STRIP_TYPEDEF_TYPE);
  SgExpression *expr = nullptr;

  if (isSgTypeNullptr(stripped) != nullptr) {
    expr = SageBuilder::buildNullptrValExp();
  } else if (SageInterface::isScalarType(stripped) ||
             SageInterface::isPointerType(stripped) ||
             isSgEnumType(stripped) != nullptr) {
    expr = SageBuilder::buildCastExp(SageBuilder::buildIntVal(0), type);
  } else {
    ROSE_ASSERT(isSgTypeUnknown(stripped) == nullptr);
    bool class_unknown = isSgClassType(stripped) == nullptr;
    SgExprListExp *args = SageBuilder::buildExprListExp_nfi();
    expr = SageBuilder::buildConstructorInitializer_nfi(
        nullptr, args, type,
        false,        // need_name
        false,        // need_qualifier
        false,        // need_parenthesis_after_name
        class_unknown // associated_class_unknown
    );
  }
  ROSE_ASSERT(expr != nullptr);

  *node = expr;

  return VisitExpr(implicit_value_init_expr, node) && res;
}

bool ClangToSageTranslator::VisitInitListExpr(
    clang::InitListExpr *init_list_expr, SgNode **node) {
#if DEBUG_VISIT_STMT
  std::cerr << "ClangToSageTranslator::VisitInitListExpr" << std::endl;
#endif

  // We use the syntactic version of the initializer if it exists
  if (init_list_expr->getSyntacticForm() != nullptr)
    return VisitInitListExpr(init_list_expr->getSyntacticForm(), node);

  SgExprListExp *expr_list_expr = SageBuilder::buildExprListExp_nfi();

  clang::InitListExpr::iterator it;
  for (it = init_list_expr->begin(); it != init_list_expr->end(); it++) {
    SgNode *tmp_expr = Traverse(*it);
    SgExpression *expr = isSgExpression(tmp_expr);
    ROSE_ASSERT(expr != nullptr);

    // Pei-Hung (05/13/2022) the expr can another InitListExpr
    SgExprListExp *child_expr_list_expr = isSgExprListExp(expr);
    SgInitializer *init = nullptr;
    if (child_expr_list_expr != nullptr) {
      SgType *type = expr->get_type();
      init = SageBuilder::buildAggregateInitializer(child_expr_list_expr, type);
    }

    if (init != nullptr) {
      applySourceRange(init, (*it)->getSourceRange());
      expr_list_expr->append_expression(init);
    } else
      expr_list_expr->append_expression(expr);
  }

  *node = expr_list_expr;

  return VisitExpr(init_list_expr, node);
}

bool ClangToSageTranslator::VisitIntegerLiteral(
    clang::IntegerLiteral *integer_literal, SgNode **node) {
#if DEBUG_VISIT_STMT
  std::cerr << "ClangToSageTranslator::VisitIntegerLiteral" << std::endl;
#endif
  SgValueExp *value_exp = nullptr;
  const clang::QualType literal_type = integer_literal->getType();
  const clang::BuiltinType *builtin_type =
      literal_type->getAs<clang::BuiltinType>();
  const llvm::APInt value = integer_literal->getValue();
  std::string spelling = getIntegerLiteralSpelling(
      integer_literal, p_compiler_instance->getSourceManager(),
      p_compiler_instance->getLangOpts());
  bool has_spelling = !spelling.empty();

  if (builtin_type != nullptr) {
    switch (builtin_type->getKind()) {
    case clang::BuiltinType::UChar:
      if (has_spelling) {
        value_exp = SageBuilder::buildUnsignedCharVal_nfi(
            static_cast<unsigned char>(value.getLimitedValue()), spelling);
      } else {
        value_exp = SageBuilder::buildUnsignedCharVal(
            static_cast<unsigned char>(value.getLimitedValue()));
      }
      break;
    case clang::BuiltinType::UShort:
      if (has_spelling) {
        value_exp = SageBuilder::buildUnsignedShortVal_nfi(
            static_cast<unsigned short>(value.getLimitedValue()), spelling);
      } else {
        value_exp = SageBuilder::buildUnsignedShortVal(
            static_cast<unsigned short>(value.getLimitedValue()));
      }
      break;
    case clang::BuiltinType::UInt:
      if (has_spelling) {
        value_exp = SageBuilder::buildUnsignedIntVal_nfi(
            static_cast<unsigned int>(value.getLimitedValue()), spelling);
      } else {
        value_exp = SageBuilder::buildUnsignedIntVal(
            static_cast<unsigned int>(value.getLimitedValue()));
      }
      break;
    case clang::BuiltinType::ULong:
      if (has_spelling) {
        value_exp = SageBuilder::buildUnsignedLongVal_nfi(
            static_cast<unsigned long>(value.getLimitedValue()), spelling);
      } else {
        value_exp = SageBuilder::buildUnsignedLongVal(
            static_cast<unsigned long>(value.getLimitedValue()));
      }
      break;
    case clang::BuiltinType::ULongLong:
      if (has_spelling) {
        value_exp = SageBuilder::buildUnsignedLongLongIntVal_nfi(
            static_cast<unsigned long long>(value.getLimitedValue()), spelling);
      } else {
        value_exp = SageBuilder::buildUnsignedLongLongIntVal(
            static_cast<unsigned long long>(value.getLimitedValue()));
      }
      break;
    case clang::BuiltinType::SChar:
    case clang::BuiltinType::Char_S:
      if (has_spelling) {
        value_exp = SageBuilder::buildCharVal_nfi(
            static_cast<char>(value.getSExtValue()), spelling);
      } else {
        value_exp =
            SageBuilder::buildCharVal(static_cast<char>(value.getSExtValue()));
      }
      break;
    case clang::BuiltinType::Char_U:
      if (has_spelling) {
        value_exp = SageBuilder::buildCharVal_nfi(
            static_cast<char>(value.getZExtValue()), spelling);
      } else {
        value_exp =
            SageBuilder::buildCharVal(static_cast<char>(value.getZExtValue()));
      }
      break;
    case clang::BuiltinType::Short:
      if (has_spelling) {
        value_exp = SageBuilder::buildShortVal_nfi(
            static_cast<short>(value.getSExtValue()), spelling);
      } else {
        value_exp = SageBuilder::buildShortVal(
            static_cast<short>(value.getSExtValue()));
      }
      break;
    case clang::BuiltinType::Int:
      if (has_spelling) {
        value_exp = SageBuilder::buildIntVal_nfi(
            static_cast<int>(value.getSExtValue()), spelling);
      } else {
        value_exp =
            SageBuilder::buildIntVal(static_cast<int>(value.getSExtValue()));
      }
      break;
    case clang::BuiltinType::Long:
      if (has_spelling) {
        value_exp = SageBuilder::buildLongIntVal_nfi(
            static_cast<long>(value.getSExtValue()), spelling);
      } else {
        value_exp = SageBuilder::buildLongIntVal(
            static_cast<long>(value.getSExtValue()));
      }
      break;
    case clang::BuiltinType::LongLong:
      if (has_spelling) {
        value_exp = SageBuilder::buildLongLongIntVal_nfi(
            static_cast<long long>(value.getSExtValue()), spelling);
      } else {
        value_exp = SageBuilder::buildLongLongIntVal(
            static_cast<long long>(value.getSExtValue()));
      }
      break;
    default:
      break;
    }
  }

  if (value_exp == nullptr) {
    if (literal_type->isUnsignedIntegerType()) {
      if (has_spelling) {
        value_exp = SageBuilder::buildUnsignedLongLongIntVal_nfi(
            static_cast<unsigned long long>(value.getLimitedValue()), spelling);
      } else {
        value_exp = SageBuilder::buildUnsignedLongLongIntVal(
            static_cast<unsigned long long>(value.getLimitedValue()));
      }
    } else {
      if (has_spelling) {
        value_exp = SageBuilder::buildLongLongIntVal_nfi(
            static_cast<long long>(value.getSExtValue()), spelling);
      } else {
        value_exp = SageBuilder::buildLongLongIntVal(
            static_cast<long long>(value.getSExtValue()));
      }
    }
  }

  *node = value_exp;

  applySourceRange(*node, integer_literal->getSourceRange());
  return VisitExpr(integer_literal, node);
}

bool ClangToSageTranslator::VisitLambdaExpr(clang::LambdaExpr *lambda_expr,
                                            SgNode **node) {
#if DEBUG_VISIT_STMT
  std::cerr << "ClangToSageTranslator::VisitLambdaExpr" << std::endl;
#endif
  bool res = true;

  // Get the lambda class (closure type) from Clang
  const clang::CXXRecordDecl *clang_lambda_class =
      lambda_expr->getLambdaClass();

  // Get the call operator (operator()) from Clang
  const clang::CXXMethodDecl *clang_call_operator =
      lambda_expr->getCallOperator();

  // Convert Clang lambda class to ROSE class declaration
  SgClassDeclaration *lambda_closure_class = nullptr;
  auto mark_lambda_closure_class_hidden =
      [this](SgClassDeclaration *class_decl) {
        std::unordered_set<SgClassDeclaration *> visited;
        std::vector<SgClassDeclaration *> worklist;
        if (class_decl != nullptr) {
          worklist.push_back(class_decl);
        }

        while (!worklist.empty()) {
          SgClassDeclaration *current = worklist.back();
          worklist.pop_back();
          if (current == nullptr || !visited.insert(current).second) {
            continue;
          }

          current->set_isAutonomousDeclaration(false);
          setCompilerGeneratedFileInfo(current, false);
          suppress_unparse_output(current);
          current->setFrontendSpecific();

          if (SgClassDefinition *class_def = current->get_definition()) {
            setCompilerGeneratedFileInfo(class_def, false);
            suppress_unparse_output(class_def);
            class_def->setFrontendSpecific();
          }

          if (SgClassDeclaration *first_nondef = isSgClassDeclaration(
                  current->get_firstNondefiningDeclaration())) {
            worklist.push_back(first_nondef);
          }
          if (SgClassDeclaration *defining =
                  isSgClassDeclaration(current->get_definingDeclaration())) {
            worklist.push_back(defining);
          }
        }
      };

  if (clang_lambda_class != nullptr) {
    SgNode *tmp_class =
        Traverse(const_cast<clang::CXXRecordDecl *>(clang_lambda_class));
    lambda_closure_class = isSgClassDeclaration(tmp_class);
    if (lambda_closure_class != nullptr) {
      if (SgClassDeclaration *defining_decl = isSgClassDeclaration(
              lambda_closure_class->get_definingDeclaration())) {
        lambda_closure_class = defining_decl;
      }
      mark_lambda_closure_class_hidden(lambda_closure_class);
      if (SgClassDeclaration *first_nondef = isSgClassDeclaration(
              lambda_closure_class->get_firstNondefiningDeclaration())) {
        if (SgScopeStatement *closure_scope =
                lambda_closure_class->get_scope()) {
          first_nondef->set_scope(closure_scope);
        }
        if (SgScopeStatement *first_scope = first_nondef->get_scope()) {
          ensureDeclInScopeChildListPreserveScope(
              first_nondef, first_scope, "VisitLambdaExpr:lambda-first-nondef");

          if (SgBasicBlock *bb = isSgBasicBlock(first_scope)) {
            SgStatementPtrList &stmts = bb->get_statements();
            if (std::find(stmts.begin(), stmts.end(), first_nondef) ==
                stmts.end()) {
              stmts.push_back(first_nondef);
            }
            if (first_nondef->get_parent() != bb) {
              first_nondef->set_parent(bb);
            }
          }
        }
      }
    }
  }

  // Convert Clang call operator to ROSE function declaration
  SgFunctionDeclaration *lambda_function = nullptr;
  if (clang_call_operator != nullptr) {
    SgNode *tmp_func =
        Traverse(const_cast<clang::CXXMethodDecl *>(clang_call_operator));
    lambda_function = isSgFunctionDeclaration(tmp_func);
    if (lambda_function != nullptr) {
      if (SgFunctionDeclaration *defining_decl = isSgFunctionDeclaration(
              lambda_function->get_definingDeclaration())) {
        lambda_function = defining_decl;
      }
    }
    if (lambda_function != nullptr && lambda_function->get_scope() == nullptr &&
        lambda_closure_class != nullptr &&
        lambda_closure_class->get_definition() != nullptr) {
      lambda_function->set_scope(lambda_closure_class->get_definition());
    }
  }

  SgLambdaCaptureList *lambda_capture_list =
      SageBuilder::buildLambdaCaptureList();
  SgScopeStatement *lambda_capture_scope = nullptr;
  if (lambda_function != nullptr) {
    if (SgFunctionDefinition *lambda_def = lambda_function->get_definition()) {
      lambda_capture_scope = lambda_def->get_body();
      if (lambda_capture_scope == nullptr) {
        lambda_capture_scope = lambda_def;
      }
    }
    if (lambda_capture_scope == nullptr) {
      lambda_capture_scope = lambda_function->get_scope();
    }
  }
  if (lambda_capture_scope == nullptr && lambda_closure_class != nullptr) {
    lambda_capture_scope = lambda_closure_class->get_definition();
    if (lambda_capture_scope == nullptr) {
      lambda_capture_scope = lambda_closure_class->get_scope();
    }
  }
  unsigned total_captures = lambda_expr->capture_size();
  unsigned capture_index = 0;
  for (auto capture_it = lambda_expr->capture_begin();
       capture_it != lambda_expr->capture_end();
       ++capture_it, ++capture_index) {
    const clang::LambdaCapture &capture = *capture_it;
    SgExpression *capture_expression = nullptr;
    bool handled_capture = false;

    if (capture.capturesThis()) {
      SgSymbol *this_symbol =
          findEnclosingThisSymbol(SageBuilder::topScopeStack());
      ROSE_ASSERT(this_symbol != nullptr);
      capture_expression = SageBuilder::buildThisExp_nfi(this_symbol);
      handled_capture = true;
    } else if (capture.capturesVariable()) {
      clang::ValueDecl *captured_val = capture.getCapturedVar();
      clang::VarDecl *captured_var =
          llvm::dyn_cast_or_null<clang::VarDecl>(captured_val);
      if (captured_var != nullptr) {
        // Look up the existing symbol instead of traversing again (avoids
        // duplicate decls)
        SgSymbol *captured_symbol = GetSymbolFromSymbolTable(captured_var);
        if (captured_symbol == nullptr) {
          clang::VarDecl *canonical_decl = captured_var->getCanonicalDecl();
          if (canonical_decl != captured_var) {
            captured_symbol = GetSymbolFromSymbolTable(canonical_decl);
          }
        }

        if (SgVariableSymbol *var_symbol =
                isSgVariableSymbol(captured_symbol)) {
          capture_expression = SageBuilder::buildVarRefExp(var_symbol);
          handled_capture = true;
        } else if (captured_var->isInitCapture()) {
          // Init-captures introduce names in lambda-local context, not the
          // enclosing file/block scope.
          SgName capture_name(captured_var->getNameAsString());
          capture_expression = SageBuilder::buildDanglingVarRefExp(
              capture_name, lambda_capture_scope);
          handled_capture = true;
        }
      }
    } else if (capture.capturesVLAType()) {
      // VLA captures are represented on the closure type; nothing to emit in
      // capture list.
      continue;
    }

    if (!handled_capture || capture_expression == nullptr) {
      continue;
    }

    bool is_init_capture = lambda_expr->isInitCapture(&capture);
    clang::LambdaCaptureKind capture_kind = capture.getCaptureKind();
    bool capture_by_reference =
        (capture_kind == clang::LCK_ByRef || capture_kind == clang::LCK_This);

    SgLambdaCapture *sg_capture =
        SageBuilder::buildLambdaCapture(capture_expression, nullptr, nullptr);
    sg_capture->set_capture_by_reference(capture_by_reference);
    sg_capture->set_implicit(capture.isImplicit());
    sg_capture->set_pack_expansion(capture.isPackExpansion());
    if (is_init_capture && capture_index < total_captures) {
      clang::Expr *clang_init_expr =
          lambda_expr->capture_init_begin()[capture_index];
      if (clang_init_expr != nullptr) {
        SgNode *init_node = Traverse(clang_init_expr);
        if (SgExpression *init_expr = isSgExpression(init_node)) {
          sg_capture->set_source_closure_variable(init_expr);
          init_expr->set_parent(sg_capture);
        }
      }
    }

    lambda_capture_list->get_capture_list().push_back(sg_capture);
    sg_capture->set_parent(lambda_capture_list);
  }

  SgLambdaExp *built_lambda = SageBuilder::buildLambdaExp(
      lambda_capture_list, lambda_closure_class, lambda_function);
  ROSE_ASSERT(built_lambda != nullptr);

  clang::LambdaCaptureDefault capture_default =
      lambda_expr->getCaptureDefault();
  bool has_default_capture = (capture_default != clang::LCD_None);
  built_lambda->set_capture_default(has_default_capture);
  built_lambda->set_default_is_by_reference(capture_default ==
                                            clang::LCD_ByRef);
  built_lambda->set_has_parameter_decl(lambda_expr->hasExplicitParameters());
  built_lambda->set_is_mutable(lambda_expr->isMutable());
  built_lambda->set_explicit_return_type(lambda_expr->hasExplicitResultType());

  *node = built_lambda;

  return VisitExpr(lambda_expr, node) && res;
}

bool ClangToSageTranslator::VisitMaterializeTemporaryExpr(
    clang::MaterializeTemporaryExpr *materialize_temporary_expr,
    SgNode **node) {
#if DEBUG_VISIT_STMT
  std::cerr << "ClangToSageTranslator::VisitMaterializeTemporaryExpr"
            << std::endl;
#endif
  bool res = true;

  // MaterializeTemporaryExpr creates a temporary object from a prvalue
  // For now, just traverse the temporary expression itself
  // The temporary materialization is implicit in C++ and doesn't need explicit
  // AST representation in ROSE
  *node = Traverse(materialize_temporary_expr->getSubExpr());

  return VisitExpr(materialize_temporary_expr, node) && res;
}

bool ClangToSageTranslator::VisitMemberExpr(clang::MemberExpr *member_expr,
                                            SgNode **node) {
#if DEBUG_VISIT_STMT
  std::cerr << "ClangToSageTranslator::VisitMemberExpr" << std::endl;
  std::cerr << "MemberExpr::hasQualifier() " << member_expr->hasQualifier()
            << std::endl;
#endif

  bool res = true;

  bool implicit_access = member_expr->isImplicitAccess();
  auto is_instance_member_access = [&](clang::ValueDecl *decl) {
    if (decl == nullptr) {
      return false;
    }
    if (llvm::isa<clang::FieldDecl>(decl)) {
      return true;
    }
    if (clang::CXXMethodDecl *method_decl =
            llvm::dyn_cast<clang::CXXMethodDecl>(decl)) {
      return !method_decl->isStatic();
    }
    return false;
  };

  bool materialize_implicit_base =
      implicit_access && member_expr->hasQualifier() &&
      is_instance_member_access(member_expr->getMemberDecl());

  SgExpression *base = nullptr;
  if (!implicit_access || materialize_implicit_base) {
    SgNode *tmp_base = Traverse(member_expr->getBase());
    base = isSgExpression(tmp_base);
    if (base == nullptr) {
      // std::cerr << "DEBUG: VisitMemberExpr base is nullptr! Base stmt class:
      // "
      // << member_expr->getBase()->getStmtClassName() << std::endl;
    } else {
      // std::cerr << "DEBUG: VisitMemberExpr base type: " << base->class_name()
      // << std::endl;
    }
    ROSE_ASSERT(base != nullptr);
  }

  auto normalize_member_decl_for_lookup =
      [](clang::ValueDecl *decl) -> clang::ValueDecl * {
    if (clang::FunctionDecl *member_func_decl =
            llvm::dyn_cast_or_null<clang::FunctionDecl>(decl)) {
      clang::TemplateSpecializationKind kind =
          member_func_decl->getTemplateSpecializationKind();
      if (kind == clang::TSK_ExplicitInstantiationDeclaration ||
          kind == clang::TSK_ExplicitInstantiationDefinition) {
        if (clang::FunctionDecl *pattern =
                member_func_decl->getTemplateInstantiationPattern()) {
          return pattern;
        }
        if (clang::CXXMethodDecl *member_method =
                llvm::dyn_cast<clang::CXXMethodDecl>(member_func_decl)) {
          if (clang::FunctionDecl *instantiated =
                  member_method->getInstantiatedFromMemberFunction()) {
            return instantiated;
          }
        }
        if (clang::FunctionDecl *first_decl =
                member_func_decl->getFirstDecl()) {
          return first_decl;
        }
      }
    }
    return decl;
  };

  clang::ValueDecl *member_decl =
      normalize_member_decl_for_lookup(member_expr->getMemberDecl());

  SgSymbol *sym = GetSymbolFromSymbolTable(member_decl);

  SgVariableSymbol *var_sym = isSgVariableSymbol(sym);
  SgMemberFunctionSymbol *func_sym = isSgMemberFunctionSymbol(sym);
  SgFunctionSymbol *plain_func_sym =
      isSgFunctionSymbol(sym); // Regular function symbol (not member)
  SgClassSymbol *class_sym = isSgClassSymbol(sym);

  if (clang::CXXMethodDecl *clang_method =
          llvm::dyn_cast<clang::CXXMethodDecl>(member_expr->getMemberDecl())) {
    auto resolve_receiver_member_scope = [&]() -> SgScopeStatement * {
      auto scope_from_type = [&](SgType *type) -> SgScopeStatement * {
        if (type == nullptr) {
          return nullptr;
        }

        SgType *stripped = type->stripType(
            SgType::STRIP_MODIFIER_TYPE | SgType::STRIP_TYPEDEF_TYPE |
            SgType::STRIP_REFERENCE_TYPE | SgType::STRIP_RVALUE_REFERENCE_TYPE);
        if (SgPointerType *ptr_type = isSgPointerType(stripped)) {
          stripped = ptr_type->get_base_type();
          if (stripped != nullptr) {
            stripped = stripped->stripType(SgType::STRIP_MODIFIER_TYPE |
                                           SgType::STRIP_TYPEDEF_TYPE |
                                           SgType::STRIP_REFERENCE_TYPE |
                                           SgType::STRIP_RVALUE_REFERENCE_TYPE);
          }
        }

        SgClassType *class_type = isSgClassType(stripped);
        if (class_type == nullptr) {
          return nullptr;
        }

        SgDeclarationStatement *decl = class_type->get_declaration();
        if (SgTemplateInstantiationDecl *inst_decl =
                isSgTemplateInstantiationDecl(decl)) {
          if (SgTemplateInstantiationDecl *def_decl =
                  isSgTemplateInstantiationDecl(
                      inst_decl->get_definingDeclaration())) {
            if (def_decl->get_definition() != nullptr) {
              return def_decl->get_definition();
            }
          }
          return inst_decl->get_definition();
        }
        if (SgTemplateClassDeclaration *tmpl_decl =
                isSgTemplateClassDeclaration(decl)) {
          if (SgTemplateClassDeclaration *def_decl =
                  isSgTemplateClassDeclaration(
                      tmpl_decl->get_definingDeclaration())) {
            if (def_decl->get_definition() != nullptr) {
              return def_decl->get_definition();
            }
          }
          return tmpl_decl->get_definition();
        }
        if (SgClassDeclaration *class_decl = isSgClassDeclaration(decl)) {
          if (SgClassDeclaration *def_decl =
                  isSgClassDeclaration(class_decl->get_definingDeclaration())) {
            if (def_decl->get_definition() != nullptr) {
              return def_decl->get_definition();
            }
          }
          return class_decl->get_definition();
        }

        return nullptr;
      };

      if (base != nullptr) {
        if (SgScopeStatement *scope = scope_from_type(base->get_type())) {
          return scope;
        }
      }

      if (clang::Expr *clang_base = member_expr->getBase()) {
        if (SgType *base_type =
                buildTypeFromQualifiedType(clang_base->getType())) {
          return scope_from_type(base_type);
        }
      }

      return nullptr;
    };

    auto lookup_template_member_decl =
        [&](clang::Decl *lookup_decl) -> SgTemplateMemberFunctionDeclaration * {
      if (lookup_decl == nullptr) {
        return nullptr;
      }

      SgDeclarationStatement *decl_stmt =
          lookupSgDeclarationForClangDecl(lookup_decl,
                                          /*allow_on_demand=*/true);
      if (SgTemplateInstantiationDirectiveStatement *inst_directive =
              isSgTemplateInstantiationDirectiveStatement(decl_stmt)) {
        decl_stmt = inst_directive->get_declaration();
      }

      if (SgTemplateMemberFunctionDeclaration *tmpl_decl =
              isSgTemplateMemberFunctionDeclaration(decl_stmt)) {
        return tmpl_decl;
      }

      return recover_instantiation_template_declaration<
          SgTemplateMemberFunctionDeclaration,
          SgTemplateInstantiationMemberFunctionDecl>(
          isSgFunctionDeclaration(decl_stmt));
    };

    auto resolve_member_template_decl =
        [&](SgMemberFunctionDeclaration *member_decl)
        -> SgTemplateMemberFunctionDeclaration * {
      if (SgTemplateMemberFunctionDeclaration *tmpl_decl =
              recover_instantiation_template_declaration<
                  SgTemplateMemberFunctionDeclaration,
                  SgTemplateInstantiationMemberFunctionDecl>(member_decl)) {
        return tmpl_decl;
      }

      if (SgTemplateMemberFunctionDeclaration *tmpl_decl =
              lookup_template_member_decl(clang_method->getPrimaryTemplate())) {
        return tmpl_decl;
      }
      if (SgTemplateMemberFunctionDeclaration *tmpl_decl =
              lookup_template_member_decl(
                  clang_method->getDescribedFunctionTemplate())) {
        return tmpl_decl;
      }
      if (SgTemplateMemberFunctionDeclaration *tmpl_decl =
              lookup_template_member_decl(
                  clang_method->getInstantiatedFromMemberFunction())) {
        return tmpl_decl;
      }
      return lookup_template_member_decl(
          clang_method->getTemplateInstantiationPattern());
    };

    auto resolve_receiver_member_declaration =
        [&](SgMemberFunctionDeclaration *member_decl)
        -> SgMemberFunctionDeclaration * {
      SgScopeStatement *receiver_scope = resolve_receiver_member_scope();
      if (receiver_scope == nullptr) {
        return nullptr;
      }

      SgMemberFunctionDeclaration *normalized_member_decl = member_decl;
      if (SgMemberFunctionDeclaration *first_nondef =
              isSgMemberFunctionDeclaration(
                  normalized_member_decl != nullptr
                      ? normalized_member_decl
                            ->get_firstNondefiningDeclaration()
                      : nullptr)) {
        normalized_member_decl = first_nondef;
      }

      if (normalized_member_decl != nullptr &&
          normalized_member_decl->get_scope() == receiver_scope) {
        return normalized_member_decl;
      }

      SgName target_name = normalized_member_decl != nullptr
                               ? normalized_member_decl->get_name()
                               : SgName(clang_method->getNameAsString());
      SgType *target_type = normalized_member_decl != nullptr
                                ? normalized_member_decl->get_type()
                                : nullptr;
      if (target_type == nullptr) {
        target_type = buildTypeFromQualifiedType(clang_method->getType());
      }
      SgTemplateMemberFunctionDeclaration *target_template_decl =
          resolve_member_template_decl(normalized_member_decl);

      SgMemberFunctionDeclaration *template_match = nullptr;
      SgMemberFunctionDeclaration *type_match = nullptr;
      SgMemberFunctionDeclaration *name_match = nullptr;
      size_t name_match_count = 0;

      SgDeclarationStatementPtrList *members =
          get_scope_declaration_list_for_instantiation_ref(receiver_scope);
      if (members == nullptr) {
        return nullptr;
      }

      for (SgDeclarationStatement *member_stmt : *members) {
        SgMemberFunctionDeclaration *candidate =
            isSgMemberFunctionDeclaration(member_stmt);
        if (candidate == nullptr) {
          continue;
        }

        if (SgMemberFunctionDeclaration *first_nondef =
                isSgMemberFunctionDeclaration(
                    candidate->get_firstNondefiningDeclaration())) {
          candidate = first_nondef;
        }

        if (candidate->get_name() != target_name) {
          continue;
        }

        name_match = candidate;
        ++name_match_count;

        if (target_template_decl != nullptr) {
          if (SgTemplateInstantiationMemberFunctionDecl *inst_candidate =
                  isSgTemplateInstantiationMemberFunctionDecl(candidate)) {
            if (inst_candidate->get_templateDeclaration() ==
                target_template_decl) {
              if (target_type == nullptr ||
                  SageInterface::isEquivalentType(candidate->get_type(),
                                                  target_type)) {
                return candidate;
              }
              if (template_match == nullptr) {
                template_match = candidate;
              }
            }
          }
        }

        if (target_type != nullptr && SageInterface::isEquivalentType(
                                          candidate->get_type(), target_type)) {
          if (type_match == nullptr) {
            type_match = candidate;
          }
        }
      }

      if (template_match != nullptr) {
        return template_match;
      }
      if (type_match != nullptr) {
        return type_match;
      }
      if (name_match_count == 1) {
        return name_match;
      }

      return nullptr;
    };

    auto resolve_member_symbol = [&](SgMemberFunctionDeclaration *member_decl)
        -> SgMemberFunctionSymbol * {
      if (member_decl == nullptr) {
        return nullptr;
      }
      SgScopeStatement *decl_scope =
          normalizeNamespaceScope(member_decl->get_scope());
      if (decl_scope == nullptr) {
        decl_scope = SageBuilder::topScopeStack();
      }
      SgMemberFunctionSymbol *member_sym = nullptr;
      if (decl_scope != nullptr) {
        member_sym = decl_scope->lookup_nontemplate_member_function_symbol(
            member_decl->get_name(), member_decl->get_type(), nullptr);
      }
      if (member_sym == nullptr) {
        member_sym = new SgMemberFunctionSymbol(member_decl);
        if (decl_scope != nullptr) {
          attachSymbolToScopeOrOrphan(member_sym, decl_scope);
        } else {
          attachSymbolToScopeOrOrphan(member_sym, nullptr);
        }
      }
      return member_sym;
    };

    if (func_sym == nullptr) {
      SgMemberFunctionDeclaration *member_decl = nullptr;
      if (plain_func_sym != nullptr) {
        member_decl =
            isSgMemberFunctionDeclaration(plain_func_sym->get_declaration());
      }
      if (member_decl == nullptr) {
        SgDeclarationStatement *decl_stmt = lookupSgDeclarationForClangDecl(
            clang_method, /*allow_on_demand=*/true);
        if (SgTemplateInstantiationDirectiveStatement *inst_directive =
                isSgTemplateInstantiationDirectiveStatement(decl_stmt)) {
          decl_stmt = inst_directive->get_declaration();
        }
        member_decl = isSgMemberFunctionDeclaration(decl_stmt);
      }
      if (SgMemberFunctionDeclaration *receiver_member_decl =
              resolve_receiver_member_declaration(member_decl)) {
        member_decl = receiver_member_decl;
      }

      if (SgMemberFunctionSymbol *method_sym =
              resolve_member_symbol(member_decl)) {
        sym = method_sym;
        func_sym = method_sym;
        plain_func_sym = nullptr;
      }
    }
  }

  SgExpression *sg_member_expr = nullptr;

  bool successful_cast = var_sym || func_sym || plain_func_sym || class_sym;
  if (sym != nullptr && !successful_cast) {
    std::cerr << "Runtime error: Unknown type of symbol for a member reference."
              << std::endl;
    std::cerr << "    sym->class_name() = " << sym->class_name() << std::endl;
    res = false;
  } else if (var_sym != nullptr) {
    sg_member_expr = SageBuilder::buildVarRefExp(var_sym);
  } else if (func_sym != nullptr) { // C++ member function
    sg_member_expr = SageBuilder::buildMemberFunctionRefExp_nfi(
        func_sym, false,
        member_expr->hasQualifier());     // FIXME 2nd and 3rd params ?
  } else if (plain_func_sym != nullptr) { // Regular function treated as member
                                          // (e.g., static member or inherited)
    sg_member_expr = SageBuilder::buildFunctionRefExp(plain_func_sym);
  } else if (class_sym != nullptr) {
    SgClassDeclaration *classDecl = class_sym->get_declaration();
    SgClassDeclaration *classDefDecl =
        isSgClassDeclaration(classDecl->get_definition());
    SgType *classType = classDecl->get_type();
    //        if(classDecl->get_isUnNamed())
    {
      SgName varName(generate_name_for_variable(member_expr));
      std::cerr << "build varName:" << varName << std::endl;
      SgVariableDeclaration *var_decl = SageBuilder::buildVariableDeclaration(
          varName, classType, nullptr, SageBuilder::topScopeStack());
      var_decl->set_baseTypeDefiningDeclaration(classDefDecl);
      var_decl->set_variableDeclarationContainsBaseTypeDefiningDeclaration(
          true);
      var_decl->set_parent(SageBuilder::topScopeStack());

      sg_member_expr = SageBuilder::buildVarRefExp(var_decl);
    }
  } else if (sym == nullptr) {
    // Symbol not found - check if member declaration has already been traversed
    // to avoid infinite recursion during template member access
    SgNode *tmp_member = nullptr;

    // First check if already in translation map
    if (llvm::isa<clang::Decl>(member_decl)) {
      std::map<clang::Decl *, SgNode *>::iterator it_decl =
          p_decl_translation_map.find((clang::Decl *)member_decl);
      if (it_decl != p_decl_translation_map.end()) {
        tmp_member = it_decl->second;
      }
    }

    // If not in map, traverse it (but this might fail for template members)
    if (tmp_member == nullptr) {
      tmp_member = Traverse(member_decl);
    }

    // Explicit instantiation declarations may be wrapped in a directive node.
    // Member-expression resolution needs the underlying declaration symbol.
    if (SgTemplateInstantiationDirectiveStatement *inst_directive =
            isSgTemplateInstantiationDirectiveStatement(tmp_member)) {
      if (SgDeclarationStatement *wrapped_decl =
              inst_directive->get_declaration()) {
        tmp_member = wrapped_decl;
      }
    }

#if DEBUG_VISIT_STMT
    if (tmp_member != nullptr) {
      // std::cerr << "DEBUG VisitMemberExpr: Got/traversed member, node type: "
      // << tmp_member->class_name() << std::endl;
    } else {
      // std::cerr << "DEBUG VisitMemberExpr: Member not available (nullptr)" <<
      // std::endl;
    }
#endif
    if (tmp_member != nullptr) {
      // CLANG FRONTEND FIX: Extract symbol from the traversed member
      // declaration.
      //
      // WHY: After successfully traversing the member's declaration above, we
      // need to get its symbol. However, calling GetSymbolFromSymbolTable again
      // here would create infinite recursion when template members reference
      // each other.
      //
      // SOLUTION: Set sym=nullptr to skip the GetSymbolFromSymbolTable call
      // below (around line 4290), and instead extract the symbol directly from
      // the already-constructed SAGE node (tmp_member).
      //
      // This breaks the cycle: GetSymbolFromSymbolTable → VisitMemberExpr →
      // Traverse(member) → GetSymbolFromSymbolTable (AVOIDED by sym=nullptr)
      //
      sym = nullptr; // Skip GetSymbolFromSymbolTable below; extract from
                     // tmp_member instead

      // Extract symbol directly from the traversed node
      if (isSgVariableDeclaration(tmp_member)) {
        SgInitializedName *init_name = SageInterface::getFirstInitializedName(
            isSgVariableDeclaration(tmp_member));
        if (init_name) {
          sym = init_name->search_for_symbol_from_symbol_table();
          if (sym == nullptr) {
            SgScopeStatement *decl_scope = init_name->get_scope();
            if (decl_scope == nullptr) {
              decl_scope = SageBuilder::topScopeStack();
            }
            if (decl_scope != nullptr) {
              SgVariableSymbol *new_sym = new SgVariableSymbol(init_name);
              attachSymbolToScopeOrOrphan(new_sym, decl_scope);
              sym = new_sym;
            }
          }
        }
      } else if (isSgFunctionDeclaration(tmp_member)) {
        SgFunctionDeclaration *func_decl = isSgFunctionDeclaration(tmp_member);
        SgScopeStatement *decl_scope = func_decl->get_scope();
        decl_scope = normalizeNamespaceScope(decl_scope);
        if (decl_scope) {
          // Use type-aware lookup to handle overloaded functions correctly
          SgFunctionType *func_type = func_decl->get_type();
          sym = decl_scope->lookup_function_symbol(func_decl->get_name(),
                                                   func_type);
        }
      }
      if (isSgVariableSymbol(sym)) {
        sg_member_expr = SageBuilder::buildVarRefExp(isSgVariableSymbol(sym));
      } else if (isSgMemberFunctionSymbol(sym)) {
        sg_member_expr = SageBuilder::buildMemberFunctionRefExp_nfi(
            isSgMemberFunctionSymbol(sym), false, false);
      } else if (isSgFunctionSymbol(sym)) {
        // ROOT CAUSE FIX: Handle plain SgFunctionSymbol (not member function
        // symbol) This happens when VisitFunctionDecl creates a regular
        // function declaration
        sg_member_expr =
            SageBuilder::buildFunctionRefExp(isSgFunctionSymbol(sym));
      } else if (isSgInitializedName(tmp_member)) {
        // Create a temporary symbol if we got an initialized name
        SgVariableSymbol *temp_sym =
            new SgVariableSymbol(isSgInitializedName(tmp_member));
        attachSymbolToScopeOrOrphan(temp_sym, nullptr);
        sg_member_expr = SageBuilder::buildVarRefExp(temp_sym);
      }
      // ROOT CAUSE FIX: Handle SgMemberFunctionDeclaration from
      // VisitCXXMethodDecl
      else if (sym == nullptr &&
               isSgMemberFunctionDeclaration(tmp_member) != nullptr) {
        SgMemberFunctionDeclaration *member_func_decl =
            isSgMemberFunctionDeclaration(tmp_member);
        // Try to find existing symbol in the class scope
        SgScopeStatement *decl_scope = member_func_decl->get_scope();
        if (SgTemplateMemberFunctionDeclaration *tmpl_member =
                isSgTemplateMemberFunctionDeclaration(member_func_decl)) {
          SgTemplateParameterPtrList *params =
              &tmpl_member->get_templateParameters();
          if (decl_scope != nullptr) {
            sym = decl_scope->lookup_template_member_function_symbol(
                tmpl_member->get_name(), tmpl_member->get_type(), params);
          }
          if (sym == nullptr) {
            SgTemplateMemberFunctionSymbol *new_func_sym =
                new SgTemplateMemberFunctionSymbol(tmpl_member);
            if (decl_scope != nullptr) {
              attachSymbolToScopeOrOrphan(new_func_sym, decl_scope);
            } else {
              attachSymbolToScopeOrOrphan(new_func_sym, nullptr);
            }
            sym = new_func_sym;
          }
        } else {
          if (decl_scope != nullptr) {
            // Use type-aware lookup to handle overloaded member functions
            // correctly
            SgFunctionType *func_type = member_func_decl->get_type();
            sym = decl_scope->lookup_nontemplate_member_function_symbol(
                member_func_decl->get_name(), func_type, nullptr);
            if (sym == nullptr) {
              sym = decl_scope->lookup_function_symbol(
                  member_func_decl->get_name(), func_type);
            }
          }
          // If still not found, create new member function symbol
          if (sym == nullptr) {
            SgMemberFunctionSymbol *new_func_sym =
                new SgMemberFunctionSymbol(member_func_decl);
            if (decl_scope != nullptr) {
              attachSymbolToScopeOrOrphan(new_func_sym, decl_scope);
            } else {
              attachSymbolToScopeOrOrphan(new_func_sym, nullptr);
            }
            sym = new_func_sym;
          }
        }
        if (isSgMemberFunctionSymbol(sym)) {
          sg_member_expr = SageBuilder::buildMemberFunctionRefExp_nfi(
              isSgMemberFunctionSymbol(sym), false,
              member_expr->hasQualifier());
        }
      }
      // Also handle regular function declarations that might be static members
      else if (sym == nullptr &&
               isSgFunctionDeclaration(tmp_member) != nullptr) {
        SgFunctionDeclaration *func_decl = isSgFunctionDeclaration(tmp_member);
        // Try to find existing symbol
        SgScopeStatement *decl_scope =
            normalizeNamespaceScope(func_decl->get_scope());
        if (decl_scope != nullptr && func_decl->get_scope() != decl_scope) {
          func_decl->set_scope(decl_scope);
          if (SgFunctionDeclaration *first_nondef = isSgFunctionDeclaration(
                  func_decl->get_firstNondefiningDeclaration())) {
            if (first_nondef->get_scope() != decl_scope) {
              first_nondef->set_scope(decl_scope);
            }
          }
          if (SgFunctionDeclaration *def_decl = isSgFunctionDeclaration(
                  func_decl->get_definingDeclaration())) {
            if (def_decl->get_scope() != decl_scope) {
              def_decl->set_scope(decl_scope);
            }
          }
        }
        if (SgTemplateFunctionDeclaration *tmpl_decl =
                isSgTemplateFunctionDeclaration(func_decl)) {
          SgTemplateParameterPtrList *params =
              &tmpl_decl->get_templateParameters();
          if (decl_scope != nullptr) {
            sym = decl_scope->lookup_template_function_symbol(
                tmpl_decl->get_name(), tmpl_decl->get_type(), params);
          }
          if (sym == nullptr) {
            SgTemplateFunctionSymbol *new_func_sym =
                new SgTemplateFunctionSymbol(tmpl_decl);
            if (decl_scope != nullptr) {
              attachSymbolToScopeOrOrphan(new_func_sym, decl_scope);
            } else {
              attachSymbolToScopeOrOrphan(new_func_sym, nullptr);
            }
            sym = new_func_sym;
          }
        } else {
          if (decl_scope != nullptr) {
            // Use type-aware lookup to handle overloaded functions correctly
            SgFunctionType *func_type = func_decl->get_type();
            sym = decl_scope->lookup_nontemplate_function_symbol(
                func_decl->get_name(), func_type, nullptr);
            if (sym == nullptr) {
              sym = decl_scope->lookup_function_symbol(func_decl->get_name(),
                                                       func_type);
            }
          }
          // If not found, create new function symbol
          if (sym == nullptr) {
            SgFunctionSymbol *new_func_sym = new SgFunctionSymbol(func_decl);
            if (decl_scope != nullptr) {
              attachSymbolToScopeOrOrphan(new_func_sym, decl_scope);
            } else {
              attachSymbolToScopeOrOrphan(new_func_sym, nullptr);
            }
            sym = new_func_sym;
          }
        }
        if (isSgMemberFunctionSymbol(sym)) {
          sg_member_expr = SageBuilder::buildMemberFunctionRefExp_nfi(
              isSgMemberFunctionSymbol(sym), false,
              member_expr->hasQualifier());
        } else if (isSgFunctionSymbol(sym)) {
          sg_member_expr =
              SageBuilder::buildFunctionRefExp(isSgFunctionSymbol(sym));
        }
      }
    }

    // If still nullptr, create a placeholder
    if (sg_member_expr == nullptr) {
      auto build_member_var_ref =
          [&](clang::ValueDecl *member_decl) -> SgExpression * {
        if (member_decl == nullptr) {
          return nullptr;
        }
        SgInitializedName *init_name = nullptr;
        auto it_member = p_decl_translation_map.find(member_decl);
        if (it_member != p_decl_translation_map.end()) {
          if (SgVariableDeclaration *var_decl =
                  isSgVariableDeclaration(it_member->second)) {
            init_name = SageInterface::getFirstInitializedName(var_decl);
          } else if (SgInitializedName *init =
                         isSgInitializedName(it_member->second)) {
            init_name = init;
          }
        }
        if (init_name == nullptr) {
          if (clang::RecordDecl *parent = llvm::dyn_cast<clang::RecordDecl>(
                  member_decl->getDeclContext())) {
            SgScopeStatement *scope = resolveScopeFromDeclContext(
                parent, SageBuilder::topScopeStack());
            SgClassDefinition *class_def = isSgClassDefinition(scope);
            if (class_def == nullptr) {
              if (SgClassDeclaration *class_decl =
                      isSgClassDeclaration(scope)) {
                class_def = class_decl->get_definition();
              }
            }
            if (class_def == nullptr && parent->getDefinition() != nullptr) {
              if (SgNode *def_node =
                      TraverseOnDemand(parent->getDefinition())) {
                if (SgClassDeclaration *def_decl =
                        isSgClassDeclaration(def_node)) {
                  class_def = def_decl->get_definition();
                } else if (SgClassDefinition *def_def =
                               isSgClassDefinition(def_node)) {
                  class_def = def_def;
                }
              }
            }
            if (class_def != nullptr) {
              for (SgDeclarationStatement *member : class_def->get_members()) {
                if (SgVariableDeclaration *var_member =
                        isSgVariableDeclaration(member)) {
                  for (SgInitializedName *member_init :
                       var_member->get_variables()) {
                    if (member_init != nullptr &&
                        member_init->get_name().getString() ==
                            member_decl->getNameAsString()) {
                      init_name = member_init;
                      break;
                    }
                  }
                }
                if (init_name != nullptr) {
                  break;
                }
              }
            }
          }
        }
        if (init_name == nullptr) {
          return nullptr;
        }
        SgScopeStatement *var_scope = init_name->get_scope();
        if (var_scope == nullptr) {
          var_scope = SageBuilder::topScopeStack();
        }
        SgVariableSymbol *sym = isSgVariableSymbol(
            init_name->search_for_symbol_from_symbol_table());
        if (sym == nullptr) {
          sym = new SgVariableSymbol(init_name);
          attachSymbolToScopeOrOrphan(sym, var_scope);
        }
        if (sym == nullptr) {
          return nullptr;
        }
        return SageBuilder::buildVarRefExp(sym);
      };

      if (clang::ValueDecl *member_decl = member_expr->getMemberDecl()) {
        if (llvm::isa<clang::FieldDecl>(member_decl) ||
            llvm::isa<clang::VarDecl>(member_decl)) {
          sg_member_expr = build_member_var_ref(member_decl);
        }
      }

      std::string member_name = member_expr->getMemberNameInfo().getAsString();
      clang::ValueDecl *member_decl = member_expr->getMemberDecl();
      if (sg_member_expr == nullptr && member_decl) {
        std::cerr << "Warning: Cannot resolve "
                  << member_decl->getDeclKindName() << " member '"
                  << member_name << "'";
        if (tmp_member != nullptr) {
          std::cerr << " (traversed to " << tmp_member->class_name() << ")";
        } else {
          std::cerr << " (traverse returned nullptr)";
        }
        std::cerr << ", using placeholder" << std::endl;
      } else if (sg_member_expr == nullptr) {
        std::cerr << "Warning: Cannot resolve member '" << member_name
                  << "', using placeholder" << std::endl;
      }
      if (sg_member_expr == nullptr) {
        sg_member_expr = SageBuilder::buildDanglingVarRefExp(
            SgName(member_name), SageBuilder::topScopeStack());
      }
    }
  }

  if (sym != nullptr && func_sym == nullptr) {
    func_sym = isSgMemberFunctionSymbol(sym);
  }

  clang::FunctionDecl *clang_func =
      llvm::dyn_cast<clang::FunctionDecl>(member_expr->getMemberDecl());
  auto get_explicit_template_arg_info =
      [&](clang::TemplateArgumentListInfo &arg_info,
          size_t &explicit_arg_count) -> bool {
    if (!member_expr->hasExplicitTemplateArgs()) {
      return false;
    }

    member_expr->copyTemplateArgumentsInto(arg_info);
    explicit_arg_count = countExpandedTemplateArguments(arg_info);
    return true;
  };

  clang::TemplateArgumentListInfo explicit_arg_info;
  size_t explicit_arg_count = 0;
  bool has_explicit_template_args =
      get_explicit_template_arg_info(explicit_arg_info, explicit_arg_count);
  bool has_explicit_empty_template_id =
      has_explicit_template_args &&
      hasExplicitEmptyTemplateArgumentList(explicit_arg_info);
  const clang::TemplateArgumentList *deduced_args =
      clang_func != nullptr ? clang_func->getTemplateSpecializationArgs()
                            : nullptr;
  bool has_deduced_template_args =
      deduced_args != nullptr && deduced_args->size() != 0;
  if (!has_deduced_template_args && clang_func != nullptr) {
    if (const clang::FunctionTemplateSpecializationInfo *spec_info =
            clang_func->getTemplateSpecializationInfo()) {
      deduced_args = spec_info->TemplateArguments;
      has_deduced_template_args =
          deduced_args != nullptr && deduced_args->size() != 0;
    }
  }
  if (has_explicit_template_args || has_deduced_template_args) {
    ExplicitTemplateArgumentSourceInfo source_info =
        scanExplicitTemplateArgumentsForExprSource(
            member_expr->getSourceRange(), member_expr->getEndLoc(),
            p_compiler_instance);
    if (source_info.has_template_argument_list) {
      explicit_arg_count = source_info.argument_count;
      has_explicit_template_args = true;
      has_explicit_empty_template_id = (source_info.argument_count == 0);
    }
  }
  // Only materialize explicit instantiation refs at use sites when the
  // source explicitly provides template arguments. Building synthetic
  // instantiations for deduced calls mutates shared declaration/type state and
  // can rewrite unrelated qualified references.
  const bool should_build_instantiation = has_explicit_template_args;

  if (should_build_instantiation) {
    SgTemplateArgumentPtrList template_args;
    bool template_args_built = false;
    auto ensure_template_args = [&]() -> SgTemplateArgumentPtrList * {
      if (!template_args_built) {
        if (has_explicit_template_args) {
          template_args = buildTemplateArguments(explicit_arg_info, true);
          if (template_args.empty() && !has_explicit_empty_template_id &&
              deduced_args != nullptr && deduced_args->size() != 0) {
            template_args =
                buildTemplateArguments(*deduced_args, explicit_arg_count);
          }
        } else if (deduced_args != nullptr && deduced_args->size() != 0) {
          template_args =
              buildTemplateArguments(*deduced_args, explicit_arg_count);
        } else {
          template_args = buildTemplateArguments(explicit_arg_info, true);
        }
        template_args_built = true;
      }
      return &template_args;
    };

    auto update_inst_decl =
        [&](SgTemplateInstantiationMemberFunctionDecl *inst_decl,
            const SgName &template_base_name) {
          if (inst_decl == nullptr) {
            return;
          }
          inst_decl->set_template_argument_list_is_explicit(
              has_explicit_template_args);
          SageBuilder::setTemplateArgumentsInDeclaration(
              inst_decl, ensure_template_args());
          if (inst_decl->get_templateName().is_null() == false) {
            return;
          }
          if (template_base_name.is_null() == false) {
            inst_decl->set_templateName(template_base_name);
          }
        };

    auto lookup_template_member_decl =
        [&](clang::Decl *lookup_decl) -> SgTemplateMemberFunctionDeclaration * {
      if (lookup_decl == nullptr) {
        return nullptr;
      }

      SgDeclarationStatement *decl_stmt =
          lookupSgDeclarationForClangDecl(lookup_decl,
                                          /*allow_on_demand=*/true);
      if (SgTemplateInstantiationDirectiveStatement *inst_directive =
              isSgTemplateInstantiationDirectiveStatement(decl_stmt)) {
        decl_stmt = inst_directive->get_declaration();
      }

      if (SgTemplateMemberFunctionDeclaration *tmpl_decl =
              isSgTemplateMemberFunctionDeclaration(decl_stmt)) {
        return tmpl_decl;
      }

      return recover_instantiation_template_declaration<
          SgTemplateMemberFunctionDeclaration,
          SgTemplateInstantiationMemberFunctionDecl>(
          isSgFunctionDeclaration(decl_stmt));
    };

    auto resolve_member_template_decl =
        [&](SgMemberFunctionDeclaration *member_decl)
        -> SgTemplateMemberFunctionDeclaration * {
      if (SgTemplateMemberFunctionDeclaration *tmpl_decl =
              recover_instantiation_template_declaration<
                  SgTemplateMemberFunctionDeclaration,
                  SgTemplateInstantiationMemberFunctionDecl>(member_decl)) {
        return tmpl_decl;
      }

      if (clang_func == nullptr) {
        return nullptr;
      }

      if (SgTemplateMemberFunctionDeclaration *tmpl_decl =
              lookup_template_member_decl(clang_func->getPrimaryTemplate())) {
        return tmpl_decl;
      }
      if (SgTemplateMemberFunctionDeclaration *tmpl_decl =
              lookup_template_member_decl(
                  clang_func->getDescribedFunctionTemplate())) {
        return tmpl_decl;
      }
      if (SgTemplateMemberFunctionDeclaration *tmpl_decl =
              lookup_template_member_decl(
                  clang_func->getInstantiatedFromMemberFunction())) {
        return tmpl_decl;
      }
      return lookup_template_member_decl(
          clang_func->getTemplateInstantiationPattern());
    };

    auto set_compiler_generated_on_instantiation_chain =
        [&](SgFunctionDeclaration *decl) {
          if (decl == nullptr) {
            return;
          }
          for_each_unique_function_decl_in_chain(
              decl, [&](SgFunctionDeclaration *candidate_decl) {
                Sg_File_Info *file_info = candidate_decl->get_file_info();
                const bool touches_synthetic_candidate =
                    candidate_decl == decl || file_info == nullptr ||
                    file_info->isCompilerGenerated();
                if (!touches_synthetic_candidate) {
                  return;
                }

                setCompilerGeneratedFileInfo(candidate_decl);
                if (SgFunctionParameterList *params =
                        candidate_decl->get_parameterList()) {
                  setCompilerGeneratedFileInfo(params);
                  for (SgInitializedName *param : params->get_args()) {
                    if (param != nullptr) {
                      setCompilerGeneratedFileInfo(param);
                    }
                  }
                }
              });
        };

    auto finalize_member_instantiation_decl =
        [&](SgTemplateInstantiationMemberFunctionDecl *decl,
            SgScopeStatement *scope) {
          if (decl == nullptr) {
            return;
          }
          mark_synthesized_instantiation_decl_chain_for_ref(decl, scope);
          set_compiler_generated_on_instantiation_chain(decl);

          registerDeclarationSymbol(decl);
          if (decl->get_symbol_from_symbol_table() == nullptr &&
              decl->get_scope() != nullptr) {
            if (SgSymbol *sym = buildSymbolForDeclaration(decl)) {
              attachSymbolToScopeOrOrphan(sym, decl->get_scope());
            }
          }
        };

    auto ensure_template_instantiation_symbol =
        [&](SgMemberFunctionSymbol *member_symbol) -> SgMemberFunctionSymbol * {
      if (member_symbol == nullptr) {
        return nullptr;
      }

      SgMemberFunctionDeclaration *member_decl =
          isSgMemberFunctionDeclaration(member_symbol->get_declaration());
      if (member_decl == nullptr) {
        return member_symbol;
      }

      SgTemplateInstantiationMemberFunctionDecl *inst_decl =
          isSgTemplateInstantiationMemberFunctionDecl(member_decl);
      if (inst_decl == nullptr) {
        if (SgMemberFunctionDeclaration *first_nondef =
                isSgMemberFunctionDeclaration(
                    member_decl->get_firstNondefiningDeclaration())) {
          inst_decl = isSgTemplateInstantiationMemberFunctionDecl(first_nondef);
        }
      }

      SgName template_base_name = member_decl->get_name();
      if (SgTemplateInstantiationMemberFunctionDecl *existing_inst =
              isSgTemplateInstantiationMemberFunctionDecl(member_decl)) {
        if (existing_inst->get_templateName().is_null() == false) {
          template_base_name = existing_inst->get_templateName();
        }
      } else if (SgTemplateInstantiationMemberFunctionDecl *existing_inst =
                     isSgTemplateInstantiationMemberFunctionDecl(
                         member_decl->get_firstNondefiningDeclaration())) {
        if (existing_inst->get_templateName().is_null() == false) {
          template_base_name = existing_inst->get_templateName();
        }
      }

      if (inst_decl != nullptr) {
        ensure_function_param_list(inst_decl, nullptr);
        update_inst_decl(inst_decl, template_base_name);
        if (SgTemplateMemberFunctionDeclaration *tmpl_decl =
                resolve_member_template_decl(member_decl)) {
          apply_template_instantiation_template_links(inst_decl, tmpl_decl,
                                                      tmpl_decl->get_name());
        }
        return member_symbol;
      }

      SgScopeStatement *func_scope = member_decl->get_scope();
      if (func_scope == nullptr) {
        func_scope = SageBuilder::topScopeStack();
      }

      SgType *sym_type = member_decl->get_type();
      if (clang_func != nullptr) {
        if (SgType *clang_type =
                buildTypeFromQualifiedType(clang_func->getType())) {
          if (isSgMemberFunctionType(clang_type) != nullptr ||
              sym_type == nullptr) {
            sym_type = clang_type;
          }
        }
      }

      SgMemberFunctionType *member_type = isSgMemberFunctionType(sym_type);
      SgType *ret_type = member_type != nullptr
                             ? member_type->get_return_type()
                             : SageBuilder::buildUnknownType();
      SgFunctionParameterList *param_list =
          (member_type != nullptr &&
           member_type->get_argument_list() != nullptr)
              ? SageBuilder::buildFunctionParameterList_nfi(
                    member_type->get_argument_list())
              : SageBuilder::buildFunctionParameterList_nfi();
      unsigned int functionConstVolatileFlags =
          member_type != nullptr ? member_type->get_mfunc_specifier() : 0;

      SgTemplateArgumentPtrList builder_args(*ensure_template_args());
      SgTemplateInstantiationMemberFunctionDecl *new_inst_decl =
          isSgTemplateInstantiationMemberFunctionDecl(
              SageBuilder::buildNondefiningMemberFunctionDeclaration(
                  template_base_name, ret_type, param_list, func_scope,
                  functionConstVolatileFlags,
                  /*buildTemplateInstantiation=*/true, &builder_args));

      ensure_function_param_list(new_inst_decl, param_list);
      if (new_inst_decl == nullptr) {
        return member_symbol;
      }

      update_inst_decl(new_inst_decl, template_base_name);
      if (SgTemplateMemberFunctionDeclaration *tmpl_decl =
              resolve_member_template_decl(member_decl)) {
        apply_template_instantiation_template_links(new_inst_decl, tmpl_decl,
                                                    tmpl_decl->get_name());
      }

      finalize_member_instantiation_decl(new_inst_decl, func_scope);

      SgMemberFunctionSymbol *inst_sym = isSgMemberFunctionSymbol(
          new_inst_decl->get_symbol_from_symbol_table());
      if (inst_sym == nullptr && func_scope != nullptr) {
        inst_sym = new SgMemberFunctionSymbol(new_inst_decl);
        attachSymbolToScopeOrOrphan(inst_sym, func_scope);
      }
      return inst_sym != nullptr ? inst_sym : member_symbol;
    };

    if (func_sym != nullptr) {
      SgMemberFunctionSymbol *inst_sym =
          ensure_template_instantiation_symbol(func_sym);
      if (inst_sym != nullptr && inst_sym != func_sym) {
        sg_member_expr = SageBuilder::buildMemberFunctionRefExp_nfi(
            inst_sym, false, member_expr->hasQualifier());
        func_sym = inst_sym;
      }
    }

    if (clang_func != nullptr && explicit_arg_count == 0 &&
        func_sym != nullptr) {
      if (SgMemberFunctionDeclaration *member_decl =
              isSgMemberFunctionDeclaration(func_sym->get_declaration())) {
        if (SgTemplateInstantiationMemberFunctionDecl *inst_decl =
                isSgTemplateInstantiationMemberFunctionDecl(member_decl)) {
          update_inst_decl(inst_decl, inst_decl->get_templateName());
        }
      }
    }
  }

  ROSE_ASSERT(sg_member_expr != nullptr);

  if (member_expr->hasQualifier() && p_compiler_instance != nullptr) {
    clang::NestedNameSpecifierLoc qualifier_loc =
        member_expr->getQualifierLoc();
    const clang::NestedNameSpecifierLoc *qualifier_loc_ptr =
        qualifier_loc.getNestedNameSpecifier() ? &qualifier_loc : nullptr;
    clang::NestedNameSpecifier qualifier = member_expr->getQualifier();
    if (!qualifier && qualifier_loc_ptr != nullptr) {
      qualifier = qualifier_loc_ptr->getNestedNameSpecifier();
    }
    if (qualifier) {
      attachExplicitQualifierFromNestedName(
          sg_member_expr, qualifier, qualifier_loc_ptr, p_compiler_instance);
    }
  }

  auto is_anonymous_member_ref = [](SgExpression *expr) {
    if (SgVarRefExp *var_ref = isSgVarRefExp(expr)) {
      SgVariableSymbol *symbol = var_ref->get_symbol();
      if (symbol == nullptr) {
        return false;
      }
      std::string member_name = symbol->get_name().getString();
      return member_name.empty() || member_name.rfind("__anonymous_", 0) == 0;
    }
    return false;
  };

  auto strip_anonymous_member_base = [&](SgExpression *expr) {
    SgExpression *current = expr;
    while (current != nullptr) {
      if (SgDotExp *dot = isSgDotExp(current)) {
        if (is_anonymous_member_ref(dot->get_rhs_operand())) {
          current = dot->get_lhs_operand();
          continue;
        }
      }
      if (SgArrowExp *arrow = isSgArrowExp(current)) {
        if (is_anonymous_member_ref(arrow->get_rhs_operand())) {
          current = arrow->get_lhs_operand();
          continue;
        }
      }
      if (is_anonymous_member_ref(current)) {
        // Anonymous aggregate member chains can end at a synthetic anonymous
        // field reference. Treat access to the promoted member as implicit.
        return static_cast<SgExpression *>(nullptr);
      }
      break;
    }
    return current;
  };

  auto base_requires_arrow = [](SgExpression *expr) -> bool {
    if (expr == nullptr) {
      return false;
    }
    SgType *base_type = expr->get_type();
    if (base_type == nullptr) {
      return false;
    }
    base_type = base_type->stripType(SgType::STRIP_TYPEDEF_TYPE |
                                     SgType::STRIP_REFERENCE_TYPE |
                                     SgType::STRIP_RVALUE_REFERENCE_TYPE);
    return isSgPointerType(base_type) != nullptr;
  };

  if (!implicit_access) {
    base = strip_anonymous_member_base(base);
  }

  if (implicit_access && !materialize_implicit_base) {
    *node = sg_member_expr;
  } else if (base == nullptr) {
    *node = sg_member_expr;
  } else if (member_expr->isArrow() || base_requires_arrow(base)) {
    *node = SageBuilder::buildArrowExp(base, sg_member_expr);
  } else {
    *node = SageBuilder::buildDotExp(base, sg_member_expr);
  }

  return VisitExpr(member_expr, node) && res;
}

bool ClangToSageTranslator::VisitMSPropertyRefExpr(
    clang::MSPropertyRefExpr *ms_property_expr, SgNode **node) {
#if DEBUG_VISIT_STMT
  std::cerr << "ClangToSageTranslator::VisitMSPropertyRefExpr" << std::endl;
#endif
  bool res = true;

  SgNode *tmp_base = Traverse(ms_property_expr->getBaseExpr());
  SgExpression *base = isSgExpression(tmp_base);
  if (tmp_base != nullptr && base == nullptr) {
    std::cerr << "Runtime error: MS property base translated to a non-"
                 "expression Sage node"
              << std::endl;
    return false;
  }

  clang::MSPropertyDecl *property_decl = ms_property_expr->getPropertyDecl();
  const std::string property_name =
      property_decl != nullptr ? property_decl->getNameAsString() : "";
  if (property_name.empty()) {
    std::cerr << "Runtime error: MS property reference missing property name"
              << std::endl;
    return false;
  }

  SgExpression *sg_member_expr = nullptr;
  auto it_decl = property_decl != nullptr
                     ? p_decl_translation_map.find(property_decl)
                     : p_decl_translation_map.end();
  if (it_decl != p_decl_translation_map.end()) {
    if (SgVariableDeclaration *var_decl =
            isSgVariableDeclaration(it_decl->second)) {
      if (SgInitializedName *init_name =
              SageInterface::getFirstInitializedName(var_decl)) {
        SgVariableSymbol *sym = isSgVariableSymbol(
            init_name->search_for_symbol_from_symbol_table());
        if (sym == nullptr) {
          SgScopeStatement *scope = init_name->get_scope();
          if (scope == nullptr) {
            scope = SageBuilder::topScopeStack();
          }
          sym = new SgVariableSymbol(init_name);
          attachSymbolToScopeOrOrphan(sym, scope);
        }
        if (sym != nullptr) {
          sg_member_expr = SageBuilder::buildVarRefExp(sym);
        }
      }
    } else if (SgInitializedName *init_name =
                   isSgInitializedName(it_decl->second)) {
      SgVariableSymbol *sym =
          isSgVariableSymbol(init_name->search_for_symbol_from_symbol_table());
      if (sym == nullptr) {
        SgScopeStatement *scope = init_name->get_scope();
        if (scope == nullptr) {
          scope = SageBuilder::topScopeStack();
        }
        sym = new SgVariableSymbol(init_name);
        attachSymbolToScopeOrOrphan(sym, scope);
      }
      if (sym != nullptr) {
        sg_member_expr = SageBuilder::buildVarRefExp(sym);
      }
    }
  }

  if (sg_member_expr == nullptr) {
    sg_member_expr = SageBuilder::buildDanglingVarRefExp(
        SgName(property_name), SageBuilder::topScopeStack());
  }

  if (ms_property_expr->isImplicitAccess() || base == nullptr) {
    *node = sg_member_expr;
  } else if (ms_property_expr->isArrow()) {
    *node = SageBuilder::buildArrowExp(base, sg_member_expr);
  } else {
    *node = SageBuilder::buildDotExp(base, sg_member_expr);
  }

  applySourceRange(*node, ms_property_expr->getSourceRange());

  return VisitExpr(ms_property_expr, node) && res;
}

bool ClangToSageTranslator::VisitMSPropertySubscriptExpr(
    clang::MSPropertySubscriptExpr *ms_property_subscript_expr, SgNode **node) {
#if DEBUG_VISIT_STMT
  std::cerr << "ClangToSageTranslator::VisitMSPropertySubscriptExpr"
            << std::endl;
#endif
  bool res = true;

  SgNode *tmp_base = Traverse(ms_property_subscript_expr->getBase());
  SgExpression *base = isSgExpression(tmp_base);
  if (tmp_base != nullptr && base == nullptr) {
    std::cerr << "Runtime error: MS property subscript base translated to a "
                 "non-expression Sage node"
              << std::endl;
    return false;
  }

  SgNode *tmp_idx = Traverse(ms_property_subscript_expr->getIdx());
  SgExpression *idx = isSgExpression(tmp_idx);
  if (tmp_idx != nullptr && idx == nullptr) {
    std::cerr << "Runtime error: MS property subscript index translated to a "
                 "non-expression Sage node"
              << std::endl;
    return false;
  }

  *node = SageBuilder::buildPntrArrRefExp(base, idx);
  applySourceRange(*node, ms_property_subscript_expr->getSourceRange());

  return VisitExpr(ms_property_subscript_expr, node) && res;
}

bool ClangToSageTranslator::VisitNoInitExpr(clang::NoInitExpr *no_init_expr,
                                            SgNode **node) {
#if DEBUG_VISIT_STMT
  std::cerr << "ClangToSageTranslator::VisitNoInitExpr" << std::endl;
#endif
  bool res = true;

  // TODO

  return VisitExpr(no_init_expr, node) && res;
}

bool ClangToSageTranslator::VisitOffsetOfExpr(
    clang::OffsetOfExpr *offset_of_expr, SgNode **node) {
#if DEBUG_VISIT_STMT
  std::cerr << "ClangToSageTranslator::VisitOffsetOfExpr" << std::endl;
#endif
  bool res = true;

  SgNodePtrList nodePtrList;

  SgType *type = buildTypeFromQualifiedType(
      offset_of_expr->getTypeSourceInfo()->getType());

  nodePtrList.push_back(type);

  SgExpression *topExp = nullptr;

  for (unsigned i = 0, n = offset_of_expr->getNumComponents(); i < n; ++i) {
    clang::OffsetOfNode ON = offset_of_expr->getComponent(i);

    switch (ON.getKind()) {
    case clang::OffsetOfNode::Array: {
      // Array node
      SgExpression *arrayIdx = isSgExpression(
          Traverse(offset_of_expr->getIndexExpr(ON.getArrayExprIndex())));
      SgPntrArrRefExp *pntrArrRefExp =
          SageBuilder::buildPntrArrRefExp(topExp, arrayIdx);
      topExp = isSgExpression(pntrArrRefExp);
      break;
    }
    case clang::OffsetOfNode::Field: {
      // OffsetOfNode still uses getField(), not getFieldDecl()
      SgNode *fieldNode = Traverse(ON.getField());
      SgName fieldName(ON.getFieldName()->getName().str());
      SgVarRefExp *varExp = SageBuilder::buildDanglingVarRefExp(
          fieldName, SageBuilder::topScopeStack());
      if (topExp == nullptr) {
        topExp = isSgExpression(varExp);
      } else {
        SgDotExp *dotExp = SageBuilder::buildDotExp(topExp, varExp);
        topExp = isSgExpression(dotExp);
      }
      break;
    }
    // TODO
    case clang::OffsetOfNode::Identifier: {
      SgName fieldName(ON.getFieldName()->getName().str());
      SgVarRefExp *varExp = SageBuilder::buildDanglingVarRefExp(
          fieldName, SageBuilder::topScopeStack());
      break;
    }
    // TODO
    case clang::OffsetOfNode::Base:
      break;
    }
  }
  nodePtrList.push_back(topExp);

  SgTypeTraitBuiltinOperator *typeTraitBuiltinOperator =
      SageBuilder::buildTypeTraitBuiltinOperator("__builtin_offsetof",
                                                 nodePtrList);

  *node = typeTraitBuiltinOperator;

  return VisitExpr(offset_of_expr, node) && res;
}

bool ClangToSageTranslator::VisitOMPArraySectionExpr(
    clang::ArraySectionExpr *omp_array_section_expr, SgNode **node) {
#if DEBUG_VISIT_STMT
  std::cerr << "ClangToSageTranslator::VisitOMPArraySectionExpr" << std::endl;
#endif
  bool res = true;

  SgExpression *base = nullptr;
  SgExpression *lower = nullptr;
  SgExpression *length = nullptr;
  SgExpression *stride = nullptr;

  if (clang::Expr *base_expr = omp_array_section_expr->getBase()) {
    base = isSgExpression(Traverse(base_expr));
  }
  if (clang::Expr *lower_expr = omp_array_section_expr->getLowerBound()) {
    lower = isSgExpression(Traverse(lower_expr));
  }
  if (clang::Expr *length_expr = omp_array_section_expr->getLength()) {
    length = isSgExpression(Traverse(length_expr));
  }
  if (omp_array_section_expr->isOMPArraySection()) {
    if (clang::Expr *stride_expr = omp_array_section_expr->getStride()) {
      stride = isSgExpression(Traverse(stride_expr));
    }
  }

  if (base != nullptr) {
    if (length == nullptr) {
      length = SageBuilder::buildNullExpression_nfi();
    }
    SgSubscriptExpression *subscript =
        SageBuilder::buildSubscriptExpression_nfi(lower, length, stride);
    if (lower != nullptr) {
      lower->set_parent(subscript);
    }
    if (length != nullptr) {
      length->set_parent(subscript);
    }
    if (stride != nullptr) {
      stride->set_parent(subscript);
    }
    SgPntrArrRefExp *arr_ref = SageBuilder::buildPntrArrRefExp(base, subscript);
    *node = arr_ref;
  } else {
    *node = buildFallbackExpression(omp_array_section_expr);
  }

  if (SgExpression *expr = isSgExpression(*node)) {
    applySourceRange(expr, omp_array_section_expr->getSourceRange());
  }

  return VisitExpr(omp_array_section_expr, node) && res;
}

bool ClangToSageTranslator::VisitOpaqueValueExpr(
    clang::OpaqueValueExpr *opaque_value_expr, SgNode **node) {
#if DEBUG_VISIT_STMT
  std::cerr << "ClangToSageTranslator::VisitOpaqueValueExpr" << std::endl;
#endif
  bool res = true;

  // OpaqueValueExpr is a Clang internal reference to a previously computed
  // value. The same Clang node can appear multiple times inside one enclosing
  // expression (for example in GNU `a ?: b`), but the ROSE AST must remain a
  // tree. Returning the translated source expression directly would reuse the
  // same SgExpression node in multiple parents via the statement translation
  // cache, which later breaks analyses such as CFG consistency checks.
  // Translate the source expression once, then return a deep copy for each
  // OpaqueValueExpr occurrence.

  clang::Expr *source_expr = opaque_value_expr->getSourceExpr();
  if (source_expr) {
    SgExpression *source = isSgExpression(Traverse(source_expr));
    ROSE_ASSERT(source != nullptr);

    SgExpression *copy = SageInterface::copyExpression(source);
    applySourceRange(copy, opaque_value_expr->getSourceRange());
    *node = copy;
  } else {
    // No source expression - OpaqueValueExpr is used as a placeholder
    // Create a fallback expression for ROSE
    // This happens in range-based for loops and other desugared constructs
    *node = buildFallbackExpression(opaque_value_expr);
  }

  return VisitExpr(opaque_value_expr, node) && res;
}

bool ClangToSageTranslator::VisitOverloadExpr(
    clang::OverloadExpr *overload_expr, SgNode **node) {
#if DEBUG_VISIT_STMT
  std::cerr << "ClangToSageTranslator::VisitOverloadExpr" << std::endl;
#endif
  bool res = true;

  if (overload_expr != nullptr) {
    if (*node == nullptr) {
      std::string name = getDeclarationNameAsString(overload_expr->getName());
      SgTemplateArgumentPtrList template_args;
      const SgTemplateArgumentPtrList *template_args_ptr = nullptr;
      if (overload_expr->hasExplicitTemplateArgs()) {
        clang::TemplateArgumentListInfo arg_info(overload_expr->getLAngleLoc(),
                                                 overload_expr->getRAngleLoc());
        overload_expr->copyTemplateArgumentsInto(arg_info);
        template_args = buildTemplateArguments(arg_info, true);
        if (!template_args.empty()) {
          template_args_ptr = &template_args;
        }
      }

      SgScopeStatement *scope = SageBuilder::topScopeStack();
      if (scope == nullptr) {
        scope = getGlobalScope();
      }
      ROSE_ASSERT(scope != nullptr);

      *node = buildNonrealRefExpFromNestedNameSpecifier(
          overload_expr->getQualifier(), scope, SgName(name),
          overload_expr->hasTemplateKeyword(), template_args_ptr);
    }

    if (SgNonrealRefExp *ref = isSgNonrealRefExp(*node)) {
      if (overload_expr->hasExplicitTemplateArgs() &&
          ref->get_templateArguments().empty()) {
        clang::TemplateArgumentListInfo arg_info(overload_expr->getLAngleLoc(),
                                                 overload_expr->getRAngleLoc());
        overload_expr->copyTemplateArgumentsInto(arg_info);
        ref->get_templateArguments() = buildTemplateArguments(arg_info, true);
      }
      SageBuilder::setTemplateArgumentParents(ref);
    }

    for (auto it = overload_expr->decls_begin();
         it != overload_expr->decls_end(); ++it) {
      clang::NamedDecl *named_decl = it.getDecl();
      if (named_decl == nullptr) {
        continue;
      }
      if (clang::UsingShadowDecl *shadow =
              llvm::dyn_cast<clang::UsingShadowDecl>(named_decl)) {
        named_decl = shadow->getTargetDecl();
      }
      clang::Decl *decl = llvm::dyn_cast<clang::Decl>(named_decl);
      if (decl == nullptr) {
        continue;
      }
      TraverseOnDemand(decl);
    }
  }

  return VisitExpr(overload_expr, node) && res;
}

bool ClangToSageTranslator::VisitUnresolvedLookupExpr(
    clang::UnresolvedLookupExpr *unresolved_lookup_expr, SgNode **node) {
#if DEBUG_VISIT_STMT
  std::cerr << "ClangToSageTranslator::VisitUnresolvedLookupExpr" << std::endl;
#endif
  bool res = true;

  // UnresolvedLookupExpr represents a reference to a name that couldn't be
  // resolved during parsing (e.g., template-dependent function names like
  // `foo(t)` that will be resolved during instantiation/ADL).

  std::string function_name =
      getDeclarationNameAsString(unresolved_lookup_expr->getName());

  SgScopeStatement *current_scope = SageBuilder::topScopeStack();
  if (current_scope == nullptr) {
    current_scope = getGlobalScope();
  }
  ROSE_ASSERT(current_scope != nullptr);

  function_name = resolve_synthetic_template_param_name_in_stmt_scope(
      function_name, current_scope);

  clang::NestedNameSpecifierLoc qualifier_loc =
      unresolved_lookup_expr->getQualifierLoc();
  const clang::NestedNameSpecifierLoc *qualifier_loc_ptr =
      qualifier_loc.getNestedNameSpecifier() ? &qualifier_loc : nullptr;

  clang::NestedNameSpecifier lookup_qualifier =
      unresolved_lookup_expr->getQualifier();
  if (!lookup_qualifier && qualifier_loc_ptr != nullptr) {
    lookup_qualifier = qualifier_loc_ptr->getNestedNameSpecifier();
  }

  auto build_single_overload_function_ref = [&]() -> SgExpression * {
    // Preserve explicit qualification (e.g., std::distance) by keeping the
    // call target in nonreal form with qualifier metadata.
    if (lookup_qualifier) {
      return nullptr;
    }

    // An explicit template-id must retain its source spelling. Collapsing it
    // to a plain template symbol loses the written `<...>` argument list.
    if (unresolved_lookup_expr->hasExplicitTemplateArgs()) {
      return nullptr;
    }

    // Keep unresolved ADL/dependent lookups in nonreal form. Even when Clang
    // exposes a single ordinary-lookup candidate, ADL or template-dependent
    // context can still change the selected callee at instantiation time.
    if (unresolved_lookup_expr->requiresADL() ||
        unresolved_lookup_expr->isTypeDependent() ||
        unresolved_lookup_expr->isValueDependent()) {
      return nullptr;
    }

    if (unresolved_lookup_expr->getName().getCXXOverloadedOperator() !=
        clang::OO_None) {
      return nullptr;
    }

    clang::NamedDecl *candidate_decl = nullptr;
    clang::Decl *candidate_canon = nullptr;

    for (auto it = unresolved_lookup_expr->decls_begin();
         it != unresolved_lookup_expr->decls_end(); ++it) {
      clang::NamedDecl *named_decl = it.getDecl();
      if (named_decl == nullptr) {
        continue;
      }
      if (clang::UsingShadowDecl *shadow =
              llvm::dyn_cast<clang::UsingShadowDecl>(named_decl)) {
        named_decl = shadow->getTargetDecl();
      }
      if (named_decl == nullptr) {
        continue;
      }
      clang::Decl *canon_decl = named_decl->getCanonicalDecl();
      if (candidate_canon == nullptr) {
        candidate_canon = canon_decl;
        candidate_decl = named_decl;
        continue;
      }
      if (canon_decl != candidate_canon) {
        return nullptr;
      }
    }

    if (candidate_decl == nullptr) {
      return nullptr;
    }

    if (clang::FunctionTemplateDecl *tmpl_decl =
            llvm::dyn_cast<clang::FunctionTemplateDecl>(candidate_decl)) {
      SgNode *tmpl_node = nullptr;
      auto it_decl = p_decl_translation_map.find(tmpl_decl);
      if (it_decl != p_decl_translation_map.end()) {
        tmpl_node = it_decl->second;
      } else {
        tmpl_node = TraverseOnDemand(tmpl_decl);
      }
      if (SgTemplateFunctionDeclaration *sg_tmpl_decl =
              isSgTemplateFunctionDeclaration(tmpl_node)) {
        if (sg_tmpl_decl->get_symbol_from_symbol_table() == nullptr) {
          registerDeclarationSymbol(sg_tmpl_decl);
        }
        if (SgTemplateFunctionSymbol *sym = isSgTemplateFunctionSymbol(
                sg_tmpl_decl->get_symbol_from_symbol_table())) {
          return SageBuilder::buildTemplateFunctionRefExp_nfi(sym);
        }
      }
      return nullptr;
    }

    if (clang::FunctionDecl *func_decl =
            llvm::dyn_cast<clang::FunctionDecl>(candidate_decl)) {
      if (llvm::isa<clang::CXXMethodDecl>(func_decl)) {
        return nullptr;
      }
      SgNode *func_node = nullptr;
      auto it_decl = p_decl_translation_map.find(func_decl);
      if (it_decl != p_decl_translation_map.end()) {
        func_node = it_decl->second;
      } else {
        func_node = TraverseOnDemand(func_decl);
      }
      if (SgFunctionDeclaration *sg_func_decl =
              isSgFunctionDeclaration(func_node)) {
        if (sg_func_decl->get_symbol_from_symbol_table() == nullptr) {
          registerDeclarationSymbol(sg_func_decl);
        }
        if (SgFunctionSymbol *sym = isSgFunctionSymbol(
                sg_func_decl->get_symbol_from_symbol_table())) {
          return SageBuilder::buildFunctionRefExp(sym);
        }
      }
    }

    return nullptr;
  };

  auto normalize_class_like_scope =
      [](SgScopeStatement *scope) -> SgScopeStatement * {
    if (scope == nullptr) {
      return nullptr;
    }
    if (SgClassDeclaration *class_decl = isSgClassDeclaration(scope)) {
      if (SgClassDefinition *class_def = class_decl->get_definition()) {
        return class_def;
      }
    }
    if (SgTemplateClassDeclaration *tmpl_decl =
            isSgTemplateClassDeclaration(scope)) {
      if (SgClassDefinition *tmpl_def = tmpl_decl->get_definition()) {
        return tmpl_def;
      }
    }
    if (SgTemplateInstantiationDecl *inst_decl =
            isSgTemplateInstantiationDecl(scope)) {
      if (SgClassDefinition *inst_def = inst_decl->get_definition()) {
        return inst_def;
      }
    }
    return scope;
  };

  auto resolve_member_lookup_scope = [&]() -> SgScopeStatement * {
    const clang::CXXRecordDecl *member_record = nullptr;

    for (auto it = unresolved_lookup_expr->decls_begin();
         it != unresolved_lookup_expr->decls_end(); ++it) {
      clang::NamedDecl *named_decl = it.getDecl();
      if (named_decl == nullptr) {
        continue;
      }
      if (clang::UsingShadowDecl *shadow =
              llvm::dyn_cast<clang::UsingShadowDecl>(named_decl)) {
        named_decl = shadow->getTargetDecl();
      }
      if (named_decl == nullptr) {
        continue;
      }

      const clang::CXXMethodDecl *method_decl = nullptr;
      if (const clang::FunctionTemplateDecl *tmpl_decl =
              llvm::dyn_cast<clang::FunctionTemplateDecl>(named_decl)) {
        method_decl =
            llvm::dyn_cast<clang::CXXMethodDecl>(tmpl_decl->getTemplatedDecl());
      } else {
        method_decl = llvm::dyn_cast<clang::CXXMethodDecl>(named_decl);
      }
      if (method_decl == nullptr || method_decl->getParent() == nullptr) {
        return nullptr;
      }

      const clang::CXXRecordDecl *candidate_record = method_decl->getParent();
      if (candidate_record->getDefinition() != nullptr) {
        candidate_record = candidate_record->getDefinition();
      }
      candidate_record = llvm::dyn_cast_or_null<clang::CXXRecordDecl>(
          candidate_record->getCanonicalDecl());
      if (candidate_record == nullptr) {
        return nullptr;
      }

      if (member_record == nullptr) {
        member_record = candidate_record;
      } else if (member_record != candidate_record) {
        return nullptr;
      }
    }

    if (member_record == nullptr) {
      return nullptr;
    }

    return normalize_class_like_scope(resolveScopeFromDeclContext(
        const_cast<clang::CXXRecordDecl *>(member_record), current_scope));
  };

  if (*node == nullptr) {
    if (SgExpression *func_ref = build_single_overload_function_ref()) {
      attachExplicitQualifierFromNestedName(
          func_ref, lookup_qualifier, qualifier_loc_ptr, p_compiler_instance);
      applySourceRange(func_ref, unresolved_lookup_expr->getSourceRange());
      *node = func_ref;
    }
  }

  if (*node != nullptr) {
    return VisitOverloadExpr(unresolved_lookup_expr, node) && res;
  }

  SgTemplateArgumentPtrList template_args;
  const SgTemplateArgumentPtrList *template_args_ptr = nullptr;
  if (unresolved_lookup_expr->hasExplicitTemplateArgs()) {
    clang::TemplateArgumentListInfo arg_info;
    unresolved_lookup_expr->copyTemplateArgumentsInto(arg_info);
    template_args = buildTemplateArguments(arg_info, true);
    template_args_ptr = &template_args;
  }

  SgScopeStatement *lookup_scope = current_scope;
  if (!lookup_qualifier) {
    if (SgScopeStatement *member_scope = resolve_member_lookup_scope()) {
      lookup_scope = member_scope;
    }
  }

  *node = buildNonrealRefExpFromNestedNameSpecifier(
      structuralQualifierForExplicitlyQualifiedNonrealRef(lookup_qualifier),
      lookup_scope, SgName(function_name),
      unresolved_lookup_expr->hasTemplateKeyword(), template_args_ptr);

  // Set source position
  SgExpression *expr = isSgExpression(*node);
  if (expr != nullptr) {
    attachExplicitQualifierFromNestedName(
        expr, lookup_qualifier, qualifier_loc_ptr, p_compiler_instance);
    applySourceRange(expr, unresolved_lookup_expr->getSourceRange());
  }

  return VisitOverloadExpr(unresolved_lookup_expr, node) && res;
}

bool ClangToSageTranslator::VisitUnresolvedMemberExpr(
    clang::UnresolvedMemberExpr *unresolved_member_expr, SgNode **node) {
#if DEBUG_VISIT_STMT
  std::cerr << "ClangToSageTranslator::VisitUnresolvedMemberExpr" << std::endl;
#endif
  bool res = true;

  // ROOT CAUSE FIX: UnresolvedMemberExpr represents a member access expression
  // where the member couldn't be resolved at parse time (typically in
  // template-dependent code) Example: template<typename T> void foo(T t) {
  // t.bar(); }  // 'bar' is unresolved

  // Get the member name
  std::string member_name =
      getDeclarationNameAsString(unresolved_member_expr->getMemberName());

  SgScopeStatement *current_scope = SageBuilder::topScopeStack();
  if (current_scope == nullptr) {
    current_scope = getGlobalScope();
  }
  ROSE_ASSERT(current_scope != nullptr);

  member_name = resolve_synthetic_template_param_name_in_stmt_scope(
      member_name, current_scope);

  // Handle the base expression (the object/pointer being accessed)
  SgExpression *base_expr = nullptr;
  if (!unresolved_member_expr->isImplicitAccess()) {
    SgNode *tmp_base = Traverse(unresolved_member_expr->getBase());
    base_expr = isSgExpression(tmp_base);
    ROSE_ASSERT(base_expr != nullptr);
  }

  SgTemplateArgumentPtrList template_args;
  const SgTemplateArgumentPtrList *template_args_ptr = nullptr;
  if (unresolved_member_expr->hasExplicitTemplateArgs()) {
    clang::TemplateArgumentListInfo arg_info;
    unresolved_member_expr->copyTemplateArgumentsInto(arg_info);
    template_args = buildTemplateArguments(arg_info, true);
    template_args_ptr = &template_args;
  }

  SgNonrealRefExp *member_ref = buildNonrealRefExpFromNestedNameSpecifier(
      structuralQualifierForExplicitlyQualifiedNonrealRef(
          unresolved_member_expr->getQualifier()),
      current_scope, SgName(member_name),
      unresolved_member_expr->hasTemplateKeyword(), template_args_ptr);
  ROSE_ASSERT(member_ref != nullptr);
  clang::NestedNameSpecifierLoc qualifier_loc =
      unresolved_member_expr->getQualifierLoc();
  const clang::NestedNameSpecifierLoc *qualifier_loc_ptr =
      qualifier_loc.getNestedNameSpecifier() ? &qualifier_loc : nullptr;
  attachExplicitQualifierFromNestedName(member_ref,
                                        unresolved_member_expr->getQualifier(),
                                        qualifier_loc_ptr, p_compiler_instance);

  if (base_expr != nullptr) {
    if (unresolved_member_expr->isArrow()) {
      *node = SageBuilder::buildArrowExp(base_expr, member_ref);
    } else {
      *node = SageBuilder::buildDotExp(base_expr, member_ref);
    }
  } else {
    *node = member_ref;
  }

  if (SgExpression *expr = isSgExpression(*node)) {
    applySourceRange(expr, unresolved_member_expr->getSourceRange());
  }

  return VisitOverloadExpr(unresolved_member_expr, node) && res;
}

bool ClangToSageTranslator::VisitPackExpansionExpr(
    clang::PackExpansionExpr *pack_expansion_expr, SgNode **node) {
#if DEBUG_VISIT_STMT
  std::cerr << "ClangToSageTranslator::VisitPackExpansionExpr" << std::endl;
#endif
  bool res = true;

  // Translate pack expansion structurally as an explicit AST node.
  clang::Expr *pattern = pack_expansion_expr->getPattern();
  if (pattern != nullptr) {
    SgNode *tmp_node = Traverse(pattern);
    SgExpression *pattern_expr = isSgExpression(tmp_node);
    if (pattern_expr != nullptr) {
      SgPackExpansionExpr *pack_expr = new SgPackExpansionExpr(pattern_expr);
      if (pattern_expr->get_parent() == nullptr) {
        pattern_expr->set_parent(pack_expr);
      }
      pack_expr->set_need_paren(false);
      applySourceRange(pack_expr, pack_expansion_expr->getSourceRange());
      *node = pack_expr;
      return VisitExpr(pack_expansion_expr, node) && res;
    }
  }

  // Preserve AST validity if the pattern cannot be translated.
  *node = buildFallbackExpression(pack_expansion_expr);

  return VisitExpr(pack_expansion_expr, node) && res;
}

bool ClangToSageTranslator::VisitParenExpr(clang::ParenExpr *paren_expr,
                                           SgNode **node) {
#if DEBUG_VISIT_STMT
  std::cerr << "ClangToSageTranslator::VisitParenExpr" << std::endl;
#endif

  bool res = true;

  SgNode *tmp_subexpr = Traverse(paren_expr->getSubExpr());
  SgExpression *subexpr = isSgExpression(tmp_subexpr);
  if (tmp_subexpr != nullptr && subexpr == nullptr) {
    std::cerr << "Runtime error: tmp_subexpr != nullptr && subexpr == nullptr"
              << std::endl;
    res = false;
  }

  // bypass ParenExpr, their is nothing equivalent in SageIII
  *node = subexpr;

  return VisitExpr(paren_expr, node) && res;
}

bool ClangToSageTranslator::VisitParenListExpr(
    clang::ParenListExpr *paran_list_expr, SgNode **node) {
#if DEBUG_VISIT_STMT
  std::cerr << "ClangToSageTranslator::VisitParenListExpr" << std::endl;
#endif
  bool res = true;

  // ROOT CAUSE FIX: ParenListExpr represents a parenthesized list of
  // expressions like in aggregate initialization: Point p(1, 2); Build an
  // expression list to represent the paren list

  unsigned num_exprs = paran_list_expr->getNumExprs();

  if (num_exprs == 1) {
    // Single expression - just unwrap the parentheses and return the expression
    // directly
    *node = Traverse(paran_list_expr->getExpr(0));
  } else {
    // Multiple expressions - build an ExprListExp
    SgExprListExp *expr_list = SageBuilder::buildExprListExp();
    for (unsigned i = 0; i < num_exprs; i++) {
      SgNode *tmp_expr = Traverse(paran_list_expr->getExpr(i));
      SgExpression *expr = isSgExpression(tmp_expr);
      if (expr != nullptr) {
        expr_list->append_expression(expr);
      } else if (tmp_expr != nullptr) {
        std::cerr << "Warning: ParenListExpr element " << i
                  << " is not an expression" << std::endl;
        res = false;
      }
    }
    *node = expr_list;
  }

  return VisitExpr(paran_list_expr, node) && res;
}

bool ClangToSageTranslator::VisitCXXParenListInitExpr(
    clang::CXXParenListInitExpr *cxx_paren_list_init_expr, SgNode **node) {
#if DEBUG_VISIT_STMT
  std::cerr << "ClangToSageTranslator::VisitCXXParenListInitExpr" << std::endl;
#endif
  bool res = true;

  SgType *constructed_type =
      buildTypeFromQualifiedType(cxx_paren_list_init_expr->getType());

  SgExprListExp *args = SageBuilder::buildExprListExp_nfi();
  for (clang::Expr *clang_arg : cxx_paren_list_init_expr->getInitExprs()) {
    if (clang_arg == nullptr) {
      continue;
    }
    SgNode *tmp_arg = Traverse(clang_arg);
    SgExpression *arg_expr = isSgExpression(tmp_arg);
    if (arg_expr != nullptr) {
      args->append_expression(arg_expr);
    } else if (tmp_arg != nullptr) {
      std::cerr << "Runtime error: CXXParenListInitExpr argument did not "
                   "translate into SgExpression"
                << std::endl;
      res = false;
    }
  }

  bool class_unknown = false;
  if (constructed_type == nullptr ||
      (isSgTypedefType(constructed_type) == nullptr &&
       isSgClassType(constructed_type) == nullptr)) {
    class_unknown = true;
  }

  SgConstructorInitializer *ctor_init =
      SageBuilder::buildConstructorInitializer_nfi(
          nullptr, args, constructed_type,
          false,        // need_name
          false,        // need_qualifier
          false,        // need_parenthesis_after_name
          class_unknown // associated_class_unknown
      );
  applySourceRange(ctor_init, cxx_paren_list_init_expr->getSourceRange());
  *node = ctor_init;

  return VisitExpr(cxx_paren_list_init_expr, node) && res;
}

bool ClangToSageTranslator::VisitPredefinedExpr(
    clang::PredefinedExpr *predefined_expr, SgNode **node) {
#if DEBUG_VISIT_STMT
  std::cerr << "ClangToSageTranslator::VisitPredefinedExpr" << std::endl;
#endif

  // FIXME It's get tricky here: PredefinedExpr represent compiler generateed
  // variables
  //    I choose to attach those variables on demand in the function definition
  //    scope

  // Traverse the scope's stack to find the last function definition:

  SgFunctionDefinition *func_def = nullptr;
  std::list<SgScopeStatement *>::reverse_iterator it =
      SageBuilder::ScopeStack.rbegin();
  while (it != SageBuilder::ScopeStack.rend() && func_def == nullptr) {
    func_def = isSgFunctionDefinition(*it);
    it++;
  }
  ROSE_ASSERT(func_def != nullptr);

  // Determine the name of the variable

  SgName name;

  // (01/29/2020) Pei-Hung: change to getIndentKind.  And this list is
  // incomplete for Clang 9 enum is PredefinedIdentKind with values
  // Func, Function, etc.
  switch (predefined_expr->getIdentKind()) {
  case clang::PredefinedIdentKind::Func:
  case clang::PredefinedIdentKind::FuncDName:
  case clang::PredefinedIdentKind::FuncSig:
  case clang::PredefinedIdentKind::LFuncSig:
    name = "__func__";
    break;
  case clang::PredefinedIdentKind::Function:
  case clang::PredefinedIdentKind::LFunction:
    name = "__FUNCTION__";
    break;
  case clang::PredefinedIdentKind::PrettyFunction:
  case clang::PredefinedIdentKind::PrettyFunctionNoVirtual:
    name = "__PRETTY_FUNCTION__";
    break;
  default:
    name = "__func__";
    break;
  }

  // Retrieve the associate symbol if it exists

  SgVariableSymbol *symbol = func_def->lookup_variable_symbol(name);

  // Else, build a compiler generated initialized name for this variable in the
  // function defintion scope.

  if (symbol == nullptr) {
    SgInitializedName *init_name = SageBuilder::buildInitializedName_nfi(
        name, SageBuilder::buildPointerType(SageBuilder::buildCharType()),
        nullptr);

    init_name->set_parent(func_def);
    init_name->set_scope(func_def);

    Sg_File_Info *start_fi =
        Sg_File_Info::generateDefaultFileInfoForCompilerGeneratedNode();
    start_fi->setCompilerGenerated();
    init_name->set_startOfConstruct(start_fi);

    Sg_File_Info *end_fi =
        Sg_File_Info::generateDefaultFileInfoForCompilerGeneratedNode();
    end_fi->setCompilerGenerated();
    init_name->set_endOfConstruct(end_fi);

    symbol = new SgVariableSymbol(init_name);

    func_def->insert_symbol(name, symbol);
  }
  ROSE_ASSERT(symbol != nullptr);

  // Finally build the variable reference

  *node = SageBuilder::buildVarRefExp_nfi(symbol);

  return VisitExpr(predefined_expr, node);
}

bool ClangToSageTranslator::VisitPseudoObjectExpr(
    clang::PseudoObjectExpr *pseudo_object_expr, SgNode **node) {
#if DEBUG_VISIT_STMT
  std::cerr << "ClangToSageTranslator::VisitPseudoObjectExpr" << std::endl;
#endif
  bool res = true;

  // Prefer the syntactic form so source-level pseudo-object spellings such as
  // CUDA builtins (`blockDim.x`) remain representable in the ROSE AST instead
  // of collapsing into Clang's internal getter calls.
  clang::Expr *preferred_expr = pseudo_object_expr->getSyntacticForm();
  if (preferred_expr == nullptr) {
    preferred_expr = pseudo_object_expr->getResultExpr();
  }
  if (preferred_expr == nullptr &&
      pseudo_object_expr->getNumSemanticExprs() != 0) {
    preferred_expr = pseudo_object_expr->getSemanticExpr(
        pseudo_object_expr->getResultExprIndex() !=
                clang::PseudoObjectExpr::NoResult
            ? pseudo_object_expr->getResultExprIndex()
            : 0);
  }

  if (preferred_expr == nullptr) {
    std::cerr << "Runtime error: PseudoObjectExpr has no translatable form"
              << std::endl;
    return false;
  }

  SgNode *tmp_expr = Traverse(preferred_expr);
  SgExpression *expr = isSgExpression(tmp_expr);
  if (tmp_expr != nullptr && expr == nullptr) {
    std::cerr << "Runtime error: PseudoObjectExpr translated to a non-"
                 "expression Sage node"
              << std::endl;
    return false;
  }

  *node = expr;
  applySourceRange(*node, pseudo_object_expr->getSourceRange());

  return VisitExpr(pseudo_object_expr, node) && res;
}

bool ClangToSageTranslator::VisitShuffleVectorExpr(
    clang::ShuffleVectorExpr *shuffle_vector_expr, SgNode **node) {
#if DEBUG_VISIT_STMT
  std::cerr << "ClangToSageTranslator::VisitShuffleVectorExpr" << std::endl;
#endif
  bool res = true;

  // TODO

  return VisitExpr(shuffle_vector_expr, node) && res;
}

bool ClangToSageTranslator::VisitSizeOfPackExpr(
    clang::SizeOfPackExpr *size_of_pack_expr, SgNode **node) {
#if DEBUG_VISIT_STMT
  std::cerr << "ClangToSageTranslator::VisitSizeOfPackExpr" << std::endl;
#endif
  bool res = true;

  // ROOT CAUSE FIX: sizeof...(Args) returns the compile-time count of pack
  // elements However, for template-dependent packs, the size isn't known until
  // instantiation

  SgExpression *pack_expr = nullptr;
  if (clang::NamedDecl *pack_decl = size_of_pack_expr->getPack()) {
    std::string pack_name = pack_decl->getNameAsString();
    if (!pack_name.empty()) {
      SgScopeStatement *scope = SageBuilder::topScopeStack();
      if (scope == nullptr) {
        scope = getGlobalScope();
      }
      pack_expr = buildNonrealRefExpFromNestedNameSpecifier(
          std::nullopt, scope, SgName(pack_name), false, nullptr);
    }
  }
  if (pack_expr == nullptr) {
    SgScopeStatement *scope = SageBuilder::topScopeStack();
    if (scope == nullptr) {
      scope = getGlobalScope();
    }
    pack_expr = buildNonrealRefExpFromNestedNameSpecifier(
        std::nullopt, scope, SgName("__pack"), false, nullptr);
  }

  SgSizeOfOp *sizeof_op = SageBuilder::buildSizeOfOp_nfi(pack_expr);
  sizeof_op->set_is_sizeof_pack(true);
  if (pack_expr->get_parent() == nullptr) {
    pack_expr->set_parent(sizeof_op);
  }
  *node = sizeof_op;

  return VisitExpr(size_of_pack_expr, node) && res;
}

bool ClangToSageTranslator::VisitSourceLocExpr(
    clang::SourceLocExpr *source_loc_expr, SgNode **node) {
#if DEBUG_VISIT_STMT
  std::cerr << "ClangToSageTranslator::VisitSourceLocExpr" << std::endl;
#endif
  bool res = true;

  // CLANG FRONTEND FIX: SourceLocExpr represents __builtin_FILE(),
  // __builtin_LINE(),
  // __builtin_FUNCTION(), __builtin_COLUMN() - compiler builtins that return
  // source location information at runtime.
  //
  // We must preserve correct types:
  // - __builtin_FILE(), __builtin_FUNCTION(), __builtin_FileName(),
  // __builtin_FuncSig()
  //   return const char* → SgStringVal
  // - __builtin_LINE() and __builtin_COLUMN() return unsigned int → SgIntVal
  //
  // Current implementation returns placeholder values (empty string "" or 0)
  // since extracting actual source location info requires SourceManager
  // integration.

  clang::SourceLocIdentKind kind = source_loc_expr->getIdentKind();

  switch (kind) {
  case clang::SourceLocIdentKind::File:
  case clang::SourceLocIdentKind::FileName:
  case clang::SourceLocIdentKind::Function:
  case clang::SourceLocIdentKind::FuncSig:
    // String-typed builtins: return empty string to preserve type correctness
    // This allows code like `const char *f = __builtin_FILE();` to unparse
    // correctly
    *node = SageBuilder::buildStringVal("");
    break;

  case clang::SourceLocIdentKind::Line:
  case clang::SourceLocIdentKind::Column:
    // Integer-typed builtins: return 0 as placeholder
    *node = SageBuilder::buildIntVal(0);
    break;

  default:
    // Unknown builtin kind (e.g., SourceLocStruct): default to integer 0
    *node = SageBuilder::buildIntVal(0);
    break;
  }

  return VisitExpr(source_loc_expr, node) && res;
}

bool ClangToSageTranslator::VisitStmtExpr(clang::StmtExpr *stmt_expr,
                                          SgNode **node) {
#if DEBUG_VISIT_STMT
  std::cerr << "ClangToSageTranslator::VisitStmtExpr" << std::endl;
#endif

  bool res = true;

  SgNode *tmp_substmt = Traverse(stmt_expr->getSubStmt());
  SgStatement *substmt = isSgStatement(tmp_substmt);
  if (tmp_substmt != nullptr && substmt == nullptr) {
    std::cerr << "Runtime error: tmp_substmt != nullptr && substmt == nullptr"
              << std::endl;
    res = false;
  }

  SgStatementExpression *statement_expression =
      new SgStatementExpression(substmt);
  if (substmt != nullptr) {
    // SgStatementExpression does not parent p_statement in its constructor.
    // GNU statement-expressions need this parent link for valid scope lookup.
    substmt->set_parent(statement_expression);
  }
  *node = statement_expression;

  return VisitExpr(stmt_expr, node) && res;
}

bool ClangToSageTranslator::VisitStringLiteral(
    clang::StringLiteral *string_literal, SgNode **node) {
#if DEBUG_VISIT_STMT
  std::cerr << "ClangToSageTranslator::VisitStringLiteral" << std::endl;
#endif
  bool res = true;

  std::string rawstr = string_literal->getBytes().str();
  std::string newstr;
#if DEBUG_VISIT_STMT
  std::cerr << "In ClangToSageTranslator string_literal length: "
            << string_literal->getLength()
            << " byteLength:" << string_literal->getByteLength() << std::endl;
#endif

  if (string_literal->isWide() || string_literal->isUTF16() ||
      string_literal->isUTF32()) {
    std::vector<uint32_t> code_units;
    code_units.reserve(string_literal->getLength());
    for (unsigned ii = 0; ii < string_literal->getLength(); ++ii) {
      code_units.push_back(string_literal->getCodeUnit(ii));
    }
    newstr = escapeWideStringLiteralCodeUnitsForUnparse(
        code_units, string_literal->isWide(), string_literal->isUTF16());
  } else {
    newstr = escapeOrdinaryStringLiteralContentsForUnparse(rawstr);
  }
  SgStringVal *sgStrVal = SageBuilder::buildStringVal(newstr);

  if (string_literal->isWide())
    sgStrVal->set_wcharString(true);
  if (string_literal->isUTF16())
    sgStrVal->set_is16bitString(true);
  if (string_literal->isUTF32())
    sgStrVal->set_is32bitString(true);
  *node = sgStrVal;

  return VisitExpr(string_literal, node) && res;
}

bool ClangToSageTranslator::VisitSubstNonTypeTemplateParmExpr(
    clang::SubstNonTypeTemplateParmExpr *subst_non_type_template_parm_expr,
    SgNode **node) {
#if DEBUG_VISIT_STMT
  std::cerr << "ClangToSageTranslator::VisitSubstNonTypeTemplateParmExpr"
            << std::endl;
#endif
  bool res = true;

  // SubstNonTypeTemplateParmExpr represents a non-type template parameter that
  // has been substituted with its actual value (e.g., N in array<T,N> being
  // replaced with 1024) Traverse to the replacement expression
  *node = Traverse(subst_non_type_template_parm_expr->getReplacement());

  return VisitExpr(subst_non_type_template_parm_expr, node) && res;
}

bool ClangToSageTranslator::VisitSubstNonTypeTemplateParmPackExpr(
    clang::SubstNonTypeTemplateParmPackExpr
        *subst_non_type_template_parm_pack_expr,
    SgNode **node) {
#if DEBUG_VISIT_STMT
  std::cerr << "ClangToSageTranslator::VisitSubstNonTypeTemplateParmPackExpr"
            << std::endl;
#endif
  bool res = true;

  auto build_expr_from_template_arg =
      [&](const clang::TemplateArgument &arg) -> SgExpression * {
    if (SgTemplateArgument *sg_arg = translateTemplateArgument(arg, false)) {
      if (SgExpression *expr = sg_arg->get_expression()) {
        return expr;
      }
      if (SgInitializedName *init_name = sg_arg->get_initializedName()) {
        return SageBuilder::buildVarRefExp(init_name);
      }
    }
    return nullptr;
  };

  std::function<void(const clang::TemplateArgument &, SgExprListExp *)>
      append_pack_expr;
  append_pack_expr = [&](const clang::TemplateArgument &arg,
                         SgExprListExp *list) {
    if (arg.getKind() == clang::TemplateArgument::Pack) {
      for (const clang::TemplateArgument &pack_arg : arg.pack_elements()) {
        append_pack_expr(pack_arg, list);
      }
      return;
    }
    if (SgExpression *expr = build_expr_from_template_arg(arg)) {
      list->get_expressions().push_back(expr);
      expr->set_parent(list);
    }
  };

  SgExprListExp *pack_list = SageBuilder::buildExprListExp();
  clang::TemplateArgument pack_arg =
      subst_non_type_template_parm_pack_expr->getArgumentPack();
  append_pack_expr(pack_arg, pack_list);

  if (!pack_list->get_expressions().empty()) {
    *node = pack_list;
    return VisitExpr(subst_non_type_template_parm_pack_expr, node) && res;
  }

  *node = buildFallbackExpression(subst_non_type_template_parm_pack_expr);

  return VisitExpr(subst_non_type_template_parm_pack_expr, node) && res;
}

bool ClangToSageTranslator::VisitTypeTraitExpr(clang::TypeTraitExpr *type_trait,
                                               SgNode **node) {
#if DEBUG_VISIT_STMT
  std::cerr << "ClangToSageTranslator::VisitTypeTraitExpr" << std::endl;
#endif
  bool res = true;

  const char *trait_name = clang::getTraitSpelling(type_trait->getTrait());
  if (trait_name == nullptr) {
    trait_name = "__type_trait";
  }

  if (!type_trait->isValueDependent()) {
    bool trait_value = false;
    if (type_trait->isStoredAsBoolean()) {
      trait_value = type_trait->getBoolValue();
    } else {
      const clang::APValue &value = type_trait->getAPValue();
      ROSE_ASSERT(value.isInt());
      trait_value = value.getInt().getBoolValue();
    }

    *node = SageBuilder::buildBoolValExp(trait_value);
  } else {
    SgNodePtrList args;
    for (unsigned i = 0; i < type_trait->getNumArgs(); ++i) {
      if (clang::TypeSourceInfo *arg_info = type_trait->getArg(i)) {
        if (SgType *arg_type =
                buildTypeFromQualifiedType(arg_info->getType())) {
          args.push_back(arg_type);
        }
      }
    }
    ROSE_ASSERT(!args.empty());
    *node = SageBuilder::buildTypeTraitBuiltinOperator(trait_name, args);
  }

  return VisitExpr(type_trait, node) && res;
}

// TypoExpr was removed in LLVM
/*
bool ClangToSageTranslator::VisitTypoExpr(clang::TypoExpr * typo_expr, SgNode **
node) { #if DEBUG_VISIT_STMT std::cerr << "ClangToSageTranslator::VisitTypoExpr"
<< std::endl; #endif bool res = true;

    // TODO

    return VisitExpr(typo_expr, node) && res;
}
*/

bool ClangToSageTranslator::VisitUnaryExprOrTypeTraitExpr(
    clang::UnaryExprOrTypeTraitExpr *unary_expr_or_type_trait_expr,
    SgNode **node) {
#if DEBUG_VISIT_STMT
  std::cerr << "ClangToSageTranslator::VisitUnaryExprOrTypeTraitExpr"
            << std::endl;
#endif

  bool res = true;

  SgExpression *expr = nullptr;
  SgType *type = nullptr;
  bool type_requires_elaboration = false;
  bool has_embedded_tag_declaration_in_type_operand = false;
  clang::TypeSourceInfo *argument_type_info = nullptr;

  auto mark_first_seen_class_type = [&](SgType *rose_type,
                                        const auto &set_definition_flag) {
    clang::QualType argument_qual_type =
        unary_expr_or_type_trait_expr->getArgumentType();
    const clang::Type *argument_type = argument_qual_type.getTypePtrOrNull();
    bool is_complete_defined = false;

    while (argument_type != nullptr) {
      if (const clang::ParenType *paren_type =
              llvm::dyn_cast<clang::ParenType>(argument_type)) {
        argument_qual_type = paren_type->getInnerType();
      } else if (qualifiedTypeHasQualifier(argument_type)) {
        argument_qual_type = argument_qual_type.getCanonicalType();
      } else if (const clang::PointerType *pointer_type =
                     llvm::dyn_cast<clang::PointerType>(argument_type)) {
        argument_qual_type = pointer_type->getPointeeType();
      } else if (const clang::ArrayType *array_type =
                     llvm::dyn_cast<clang::ArrayType>(argument_type)) {
        argument_qual_type = array_type->getElementType();
      } else if (const clang::AttributedType *attributed_type =
                     llvm::dyn_cast<clang::AttributedType>(argument_type)) {
        argument_qual_type = attributed_type->getModifiedType();
      } else if (const clang::AdjustedType *adjusted_type =
                     llvm::dyn_cast<clang::AdjustedType>(argument_type)) {
        argument_qual_type = adjusted_type->getOriginalType();
      } else {
        break;
      }
      argument_type = argument_qual_type.getTypePtrOrNull();
    }

    if (const clang::RecordType *argument_record_type =
            llvm::dyn_cast_or_null<clang::RecordType>(argument_type)) {
      const clang::RecordDecl *record_declaration =
          argument_record_type->getDecl();
      ROSE_ASSERT(record_declaration != nullptr);
      is_complete_defined = record_declaration->isCompleteDefinition();
    }

    if (SgClassType *class_type = isSgClassType(rose_type);
        class_type != nullptr && is_complete_defined) {
      std::map<SgClassType *, bool>::iterator bool_it =
          p_class_type_decl_first_see_in_type.find(class_type);
      ROSE_ASSERT(bool_it != p_class_type_decl_first_see_in_type.end());
      if (bool_it->second) {
        set_definition_flag();
        bool_it->second = false;
      }
    }
  };

  auto suppress_embedded_tag_declaration_in_type_operand =
      [&](clang::TypeSourceInfo *type_info) -> bool {
    if (type_info == nullptr || p_compiler_instance == nullptr) {
      return false;
    }

    clang::SourceManager &sm = p_compiler_instance->getSourceManager();
    auto file_loc = [&](clang::SourceLocation loc) -> clang::SourceLocation {
      if (!loc.isValid()) {
        return clang::SourceLocation();
      }
      if (loc.isMacroID()) {
        loc = sm.getSpellingLoc(loc);
      }
      return sm.getFileLoc(loc);
    };

    clang::SourceRange type_range = type_info->getTypeLoc().getSourceRange();
    clang::SourceLocation range_begin = file_loc(type_range.getBegin());
    clang::SourceLocation range_end = file_loc(type_range.getEnd());
    if (!range_begin.isValid() || !range_end.isValid()) {
      return false;
    }

    clang::QualType current_qual_type = type_info->getType();
    const clang::Type *current_type = current_qual_type.getTypePtrOrNull();
    clang::TagDecl *tag_decl = nullptr;

    while (current_type != nullptr) {
      if (const auto *tag_type = llvm::dyn_cast<clang::TagType>(current_type)) {
        tag_decl = tag_type->getDecl();
        break;
      }
      if (const auto *injected =
              llvm::dyn_cast<clang::InjectedClassNameType>(current_type)) {
        tag_decl = injected->getDecl();
        break;
      }
      if (const auto *record_type =
              llvm::dyn_cast<clang::RecordType>(current_type)) {
        tag_decl = record_type->getDecl();
        break;
      }
      if (const auto *paren_type =
              llvm::dyn_cast<clang::ParenType>(current_type)) {
        current_qual_type = paren_type->getInnerType();
      } else if (const auto *pointer_type =
                     llvm::dyn_cast<clang::PointerType>(current_type)) {
        current_qual_type = pointer_type->getPointeeType();
      } else if (const auto *reference_type =
                     llvm::dyn_cast<clang::ReferenceType>(current_type)) {
        current_qual_type = reference_type->getPointeeType();
      } else if (const auto *array_type =
                     llvm::dyn_cast<clang::ArrayType>(current_type)) {
        current_qual_type = array_type->getElementType();
      } else if (const auto *attributed_type =
                     llvm::dyn_cast<clang::AttributedType>(current_type)) {
        current_qual_type = attributed_type->getModifiedType();
      } else if (const auto *adjusted_type =
                     llvm::dyn_cast<clang::AdjustedType>(current_type)) {
        current_qual_type = adjusted_type->getOriginalType();
      } else if (qualifiedTypeHasQualifier(current_type)) {
        current_qual_type = current_qual_type.getCanonicalType();
      } else {
        break;
      }

      current_type = current_qual_type.getTypePtrOrNull();
    }

    if (tag_decl == nullptr) {
      return false;
    }

    clang::SourceLocation decl_loc = file_loc(tag_decl->getBeginLoc());
    if (!decl_loc.isValid()) {
      return false;
    }
    if (sm.isBeforeInTranslationUnit(decl_loc, range_begin) ||
        sm.isBeforeInTranslationUnit(range_end, decl_loc)) {
      return false;
    }

    auto suppress_class_decl = [&](SgClassDeclaration *decl) {
      if (decl == nullptr) {
        return;
      }
      decl->set_isAutonomousDeclaration(false);
      suppress_unparse_output(decl);
    };
    auto suppress_enum_decl = [&](SgEnumDeclaration *decl) {
      if (decl == nullptr) {
        return;
      }
      decl->set_isAutonomousDeclaration(false);
      suppress_unparse_output(decl);
    };

    if (SgClassDeclaration *class_decl = isSgClassDeclaration(
            lookupSgDeclarationForClangDecl(tag_decl,
                                            /*allow_on_demand=*/true))) {
      suppress_class_decl(class_decl);
      suppress_class_decl(
          isSgClassDeclaration(class_decl->get_firstNondefiningDeclaration()));
      suppress_class_decl(
          isSgClassDeclaration(class_decl->get_definingDeclaration()));
      return true;
    }

    if (SgEnumDeclaration *enum_decl = isSgEnumDeclaration(
            lookupSgDeclarationForClangDecl(tag_decl,
                                            /*allow_on_demand=*/true))) {
      suppress_enum_decl(enum_decl);
      suppress_enum_decl(
          isSgEnumDeclaration(enum_decl->get_firstNondefiningDeclaration()));
      suppress_enum_decl(
          isSgEnumDeclaration(enum_decl->get_definingDeclaration()));
      return true;
    }

    return false;
  };

  auto suppress_nonautonomous_named_type_declaration = [&](SgType *rose_type) {
    if (rose_type == nullptr) {
      return;
    }

    unsigned char strip_options =
        SgType::STRIP_MODIFIER_TYPE | SgType::STRIP_REFERENCE_TYPE |
        SgType::STRIP_RVALUE_REFERENCE_TYPE | SgType::STRIP_POINTER_TYPE |
        SgType::STRIP_ARRAY_TYPE;
    SgType *stripped_type = rose_type->stripType(strip_options);

    auto suppress_class_decl = [&](SgClassDeclaration *decl) {
      if (decl == nullptr) {
        return;
      }
      decl->set_isAutonomousDeclaration(false);
      suppress_unparse_output(decl);
    };
    auto suppress_enum_decl = [&](SgEnumDeclaration *decl) {
      if (decl == nullptr) {
        return;
      }
      decl->set_isAutonomousDeclaration(false);
      suppress_unparse_output(decl);
    };
    auto physical_start_line = [](SgLocatedNode *node) -> int {
      if (node == nullptr) {
        return -1;
      }
      if (node->get_startOfConstruct() != nullptr) {
        return node->get_startOfConstruct()->get_physical_line();
      }
      if (node->get_file_info() != nullptr) {
        return node->get_file_info()->get_physical_line();
      }
      return -1;
    };
    auto suppress_matching_scope_class_decls =
        [&](SgClassDeclaration *seed_decl) {
          if (seed_decl == nullptr || seed_decl->get_scope() == nullptr) {
            return;
          }

          const SgName seed_name = seed_decl->get_name();
          const int seed_line = physical_start_line(seed_decl);
          std::function<void(SgNode *)> visit = [&](SgNode *node) {
            if (node == nullptr) {
              return;
            }

            if (SgClassDeclaration *candidate = isSgClassDeclaration(node)) {
              if (candidate->get_scope() == seed_decl->get_scope() &&
                  candidate->get_name() == seed_name &&
                  physical_start_line(candidate) == seed_line) {
                candidate->set_isAutonomousDeclaration(false);
                suppress_unparse_output(candidate);
              }
            }

            for (SgNode *child : node->get_traversalSuccessorContainer()) {
              visit(child);
            }
          };

          visit(seed_decl->get_scope());
        };
    auto suppress_matching_scope_enum_decls =
        [&](SgEnumDeclaration *seed_decl) {
          if (seed_decl == nullptr || seed_decl->get_scope() == nullptr) {
            return;
          }

          const SgName seed_name = seed_decl->get_name();
          const int seed_line = physical_start_line(seed_decl);
          std::function<void(SgNode *)> visit = [&](SgNode *node) {
            if (node == nullptr) {
              return;
            }

            if (SgEnumDeclaration *candidate = isSgEnumDeclaration(node)) {
              if (candidate->get_scope() == seed_decl->get_scope() &&
                  candidate->get_name() == seed_name &&
                  physical_start_line(candidate) == seed_line) {
                candidate->set_isAutonomousDeclaration(false);
                suppress_unparse_output(candidate);
              }
            }

            for (SgNode *child : node->get_traversalSuccessorContainer()) {
              visit(child);
            }
          };

          visit(seed_decl->get_scope());
        };

    if (SgClassType *class_type = isSgClassType(stripped_type)) {
      class_type->set_autonomous_declaration(false);
      if (SgClassDeclaration *class_decl =
              isSgClassDeclaration(class_type->get_declaration())) {
        suppress_class_decl(class_decl);
        suppress_class_decl(isSgClassDeclaration(
            class_decl->get_firstNondefiningDeclaration()));
        suppress_class_decl(
            isSgClassDeclaration(class_decl->get_definingDeclaration()));
        suppress_matching_scope_class_decls(class_decl);
      }
      return;
    }

    if (SgEnumType *enum_type = isSgEnumType(stripped_type)) {
      enum_type->set_autonomous_declaration(false);
      if (SgEnumDeclaration *enum_decl =
              isSgEnumDeclaration(enum_type->get_declaration())) {
        suppress_enum_decl(enum_decl);
        suppress_enum_decl(
            isSgEnumDeclaration(enum_decl->get_firstNondefiningDeclaration()));
        suppress_enum_decl(
            isSgEnumDeclaration(enum_decl->get_definingDeclaration()));
        suppress_matching_scope_enum_decls(enum_decl);
      }
    }
  };

  if (unary_expr_or_type_trait_expr->isArgumentType()) {
    clang::QualType argument_type =
        unary_expr_or_type_trait_expr->getArgumentType();
    type_requires_elaboration =
        qual_type_contains_elaborated_spelling_for_type_operand(argument_type);

    argument_type_info = unary_expr_or_type_trait_expr->getArgumentTypeInfo();
    if (argument_type_info != nullptr) {
      has_embedded_tag_declaration_in_type_operand =
          suppress_embedded_tag_declaration_in_type_operand(argument_type_info);
      type = buildTypeFromTypeLoc(argument_type_info->getTypeLoc());
    }
    if (type == nullptr) {
      type = buildTypeFromQualifiedType(argument_type);
    }
    if (has_embedded_tag_declaration_in_type_operand) {
      suppress_nonautonomous_named_type_declaration(type);
    } else if (type_requires_elaboration) {
      suppress_nonautonomous_named_type_declaration(type);
    }
  } else {
    SgNode *tmp_expr =
        Traverse(unary_expr_or_type_trait_expr->getArgumentExpr());
    expr = isSgExpression(tmp_expr);

    if (tmp_expr != nullptr && expr == nullptr) {
      std::cerr << "Runtime error: tmp_expr != nullptr && expr == nullptr"
                << std::endl;
      res = false;
    }
  }

  switch (unary_expr_or_type_trait_expr->getKind()) {
  case clang::UETT_SizeOf:
  case clang::UETT_DataSizeOf:
    if (type != nullptr) {
      SgSizeOfOp *sizeof_op = SageBuilder::buildSizeOfOp_nfi(type);
      sizeof_op->set_type_elaboration_required(type_requires_elaboration);

      if (has_embedded_tag_declaration_in_type_operand) {
        sizeof_op->set_sizeOfContainsBaseTypeDefiningDeclaration(true);
      } else {
        mark_first_seen_class_type(type, [&]() {
          sizeof_op->set_sizeOfContainsBaseTypeDefiningDeclaration(true);
        });
      }

      *node = sizeof_op;
    } else if (expr != nullptr) {
      *node = SageBuilder::buildSizeOfOp_nfi(expr);
    } else {
      res = false;
    }
    break;
  case clang::UETT_AlignOf:
  case clang::UETT_PreferredAlignOf:
    if (type != nullptr) {
      SgAlignOfOp *alignof_op = SageBuilder::buildAlignOfOp_nfi(type);
      alignof_op->set_type_elaboration_required(type_requires_elaboration);

      if (has_embedded_tag_declaration_in_type_operand) {
        alignof_op->set_alignOfContainsBaseTypeDefiningDeclaration(true);
      } else {
        mark_first_seen_class_type(type, [&]() {
          alignof_op->set_alignOfContainsBaseTypeDefiningDeclaration(true);
        });
      }

      *node = alignof_op;
    } else if (expr != nullptr) {
      *node = SageBuilder::buildAlignOfOp_nfi(expr);
    } else {
      res = false;
    }
    break;
  case clang::UETT_PtrAuthTypeDiscriminator:
  case clang::UETT_VecStep:
  case clang::UETT_OpenMPRequiredSimdAlign:
  case clang::UETT_VectorElements:
    ROSE_ASSERT(!"UnaryExprOrTypeTrait kind is not supported yet");
  default:
    ROSE_ASSERT(!"Unknown clang::UETT_xx");
  }

  return VisitStmt(unary_expr_or_type_trait_expr, node) && res;
}

bool ClangToSageTranslator::VisitUnaryOperator(
    clang::UnaryOperator *unary_operator, SgNode **node) {
#if DEBUG_VISIT_STMT
  std::cerr << "ClangToSageTranslator::VisitUnaryOperator" << std::endl;
#endif

  bool res = true;

  SgNode *tmp_subexpr = Traverse(unary_operator->getSubExpr());
  SgExpression *subexpr = isSgExpression(tmp_subexpr);
  if (tmp_subexpr != nullptr && subexpr == nullptr) {
    std::cerr << "Runtime error: tmp_subexpr != nullptr && subexpr == nullptr"
              << std::endl;
    res = false;
  }

  switch (unary_operator->getOpcode()) {
  case clang::UO_PostInc:
    *node = SageBuilder::buildPlusPlusOp(subexpr, SgUnaryOp::postfix);
    break;
  case clang::UO_PostDec:
    *node = SageBuilder::buildMinusMinusOp(subexpr, SgUnaryOp::postfix);
    break;
  case clang::UO_PreInc:
    *node = SageBuilder::buildPlusPlusOp(subexpr, SgUnaryOp::prefix);
    break;
  case clang::UO_PreDec:
    *node = SageBuilder::buildMinusMinusOp(subexpr, SgUnaryOp::prefix);
    break;
  case clang::UO_AddrOf:
    *node = SageBuilder::buildAddressOfOp(subexpr);
    break;
  case clang::UO_Deref:
    *node = SageBuilder::buildPointerDerefExp(subexpr);
    break;
  case clang::UO_Plus:
    *node = SageBuilder::buildUnaryAddOp(subexpr);
    break;
  case clang::UO_Minus:
    *node = SageBuilder::buildMinusOp(subexpr);
    break;
  // Def. in Clang: UNARY_OPERATION(Not, "~")
  case clang::UO_Not:
    *node = SageBuilder::buildBitComplementOp(subexpr);
    break;
  // Def. in UNARY_OPERATION(LNot, "!")
  case clang::UO_LNot:
    *node = SageBuilder::buildNotOp(subexpr);
    break;
  case clang::UO_Real:
    *node = SageBuilder::buildImagPartOp(subexpr);
    break;
  case clang::UO_Imag:
    *node = SageBuilder::buildRealPartOp(subexpr);
    break;
  case clang::UO_Extension:
    *node = subexpr;
    break;
  default:
    std::cerr << "Runtime error: Unknown unary operator." << std::endl;
    res = false;
  }

  return VisitExpr(unary_operator, node) && res;
}

bool ClangToSageTranslator::VisitVAArgExpr(clang::VAArgExpr *va_arg_expr,
                                           SgNode **node) {
#if DEBUG_VISIT_STMT
  std::cerr << "ClangToSageTranslator::VisitVAArgExpr" << std::endl;
#endif

  SgNode *tmp_expr = Traverse(va_arg_expr->getSubExpr());
  SgExpression *expr = isSgExpression(tmp_expr);
  ROSE_ASSERT(expr != nullptr);

  SgType *type =
      buildTypeFromQualifiedType(va_arg_expr->getWrittenTypeInfo()->getType());
  ROSE_ASSERT(type != nullptr);

  *node = SageBuilder::buildVarArgOp_nfi(expr, type);

  return VisitExpr(va_arg_expr, node);
}
bool ClangToSageTranslator::VisitLabelStmt(clang::LabelStmt *label_stmt,
                                           SgNode **node) {
#if DEBUG_VISIT_STMT
  std::cerr << "ClangToSageTranslator::VisitLabelStmt" << std::endl;
#endif

  bool res = true;

  SgName name(label_stmt->getName());

  *node = SageBuilder::buildLabelStatement_nfi(name, nullptr,
                                               SageBuilder::topScopeStack());
  SgLabelStatement *sg_label_stmt = isSgLabelStatement(*node);

  SgFunctionDefinition *label_scope = nullptr;
  std::list<SgScopeStatement *>::reverse_iterator it =
      SageBuilder::ScopeStack.rbegin();
  while (it != SageBuilder::ScopeStack.rend() && label_scope == nullptr) {
    label_scope = isSgFunctionDefinition(*it);
    it++;
  }
  if (label_scope == nullptr) {
    std::cerr << "Runtime error: Cannot find a surrounding function definition "
                 "for the label statement: \""
              << name << "\"." << std::endl;
    res = false;
  } else {
    sg_label_stmt->set_scope(label_scope);
    SgLabelSymbol *label_sym = new SgLabelSymbol(sg_label_stmt);
    label_scope->insert_symbol(label_sym->get_name(), label_sym);
  }

  SgNode *tmp_sub_stmt = Traverse(label_stmt->getSubStmt());
  SgStatement *sg_sub_stmt = isSgStatement(tmp_sub_stmt);
  if (sg_sub_stmt == nullptr) {
    SgExpression *sg_sub_expr = isSgExpression(tmp_sub_stmt);
    ROSE_ASSERT(sg_sub_expr != nullptr);
    sg_sub_stmt = SageBuilder::buildExprStatement(sg_sub_expr);
    applySourceRangeWithTrailingSemicolon(sg_sub_stmt,
                                          label_stmt->getSubStmt());
  }

  ROSE_ASSERT(sg_sub_stmt != nullptr);
  sg_sub_stmt =
      wrapStatementWithOpenMPPragmas(label_stmt->getSubStmt(), sg_sub_stmt);

  sg_sub_stmt->set_parent(sg_label_stmt);
  sg_label_stmt->set_statement(sg_sub_stmt);

  return VisitStmt(label_stmt, node) && res;
}

bool ClangToSageTranslator::VisitWhileStmt(clang::WhileStmt *while_stmt,
                                           SgNode **node) {
#if DEBUG_VISIT_STMT
  std::cerr << "ClangToSageTranslator::VisitWhileStmt" << std::endl;
#endif

  bool res = true;

  SgWhileStmt *sg_while_stmt =
      SageBuilder::buildWhileStmt_nfi(nullptr, nullptr);
  sg_while_stmt->set_parent(SageBuilder::topScopeStack());

  SageBuilder::pushScopeStack(sg_while_stmt);

  SgStatement *cond_stmt = nullptr;
  if (clang::DeclStmt *cond_decl = while_stmt->getConditionVariableDeclStmt()) {
    SgNode *tmp_cond = Traverse(cond_decl);
    cond_stmt = isSgStatement(tmp_cond);
    if (tmp_cond != nullptr && cond_stmt == nullptr) {
      std::cerr << "Runtime error: condition decl did not translate to "
                   "SgStatement ("
                << tmp_cond->class_name() << ")" << std::endl;
      res = false;
    }
    if (cond_stmt != nullptr) {
      applySourceRange(cond_stmt, cond_decl->getSourceRange());
    }
  } else {
    SgNode *tmp_cond = Traverse(while_stmt->getCond());
    SgExpression *cond = isSgExpression(tmp_cond);
    ROSE_ASSERT(cond != nullptr);

    cond_stmt = SageBuilder::buildExprStatement(cond);
    if (p_compiler_instance != nullptr && while_stmt->getCond() != nullptr) {
      applySourceRange(cond_stmt, while_stmt->getCond()->getSourceRange());
    }
  }

  if (cond_stmt == nullptr) {
    cond_stmt = SageBuilder::buildNullStatement_nfi();
    setCompilerGeneratedFileInfo(cond_stmt, true);
  }

  if (SgDeclarationStatement *decl = isSgDeclarationStatement(cond_stmt)) {
    SgScopeStatement *cond_scope = isSgScopeStatement(sg_while_stmt);
    if (cond_scope == nullptr) {
      cond_scope = sg_while_stmt->get_scope();
    }
    if (cond_scope == nullptr) {
      cond_scope = SageBuilder::topScopeStack();
    }
    if (cond_scope != nullptr) {
      decl->set_scope(cond_scope);
      if (SgVariableDeclaration *var_decl = isSgVariableDeclaration(decl)) {
        for (SgInitializedName *init_name : var_decl->get_variables()) {
          if (init_name != nullptr) {
            init_name->set_scope(cond_scope);
          }
        }
      }
    }
  }

  cond_stmt->set_parent(sg_while_stmt);
  sg_while_stmt->set_condition(cond_stmt);

  SgNode *tmp_body = Traverse(while_stmt->getBody());
  SgStatement *body = isSgStatement(tmp_body);
  SgExpression *expr = isSgExpression(tmp_body);
  if (expr != nullptr) {
    body = SageBuilder::buildExprStatement(expr);
    applySourceRangeWithTrailingSemicolon(body, while_stmt->getBody());
  }
  ROSE_ASSERT(body != nullptr);
  body = wrapStatementWithOpenMPPragmas(while_stmt->getBody(), body);

  body->set_parent(sg_while_stmt);

  SageBuilder::popScopeStack();

  sg_while_stmt->set_body(body);

  *node = sg_while_stmt;

  return VisitStmt(while_stmt, node) && res;
}
