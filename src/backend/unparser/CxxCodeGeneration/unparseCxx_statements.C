/* unparse_stmt.C
 * Contains functions that unparse statements
 *
 * FORMATTING WILL BE DONE IN TWO WAYS:
 * 1. using the file_info object to get information from line and column number
 *    (for original source code)
 * 2. following a specified format that I have specified with indentations of
 *    length TABINDENT (for transformations)
 *
 * REMEMBER: For types and symbols, we still call the original unparse function
 * defined in sage since they dont have file_info. For expressions,
 * Unparse_ExprStmt::unparse is called, and for statements,
 * Unparse_ExprStmt::unparseStatement is called.
 *
 */
// tps (01/14/2010) : Switching from rose.h to sage3.
#include "rex_coroutine_attributes.h"
#include "sage3basic.h"

#include "unparser.h"

#include <cctype>
#include <functional>
#include <map>

// DQ (8/31/2013):  This should only be included by source files that require
// it. This fixed a reported bug which caused conflicts with configure-time
// macros (e.g. PACKAGE_BUGREPORT). Interestingly it must be at the top of the
// list of include files.
#include "rose_config.h"
// DQ (12/6/2014): Adding support for unparsing from the token stream.
#include "tokenStreamMapping.h"

#define ROSE_TRACK_PROGRESS_OF_ROSE_COMPILING_ROSE 0

// DQ (12/31/2005): This is OK if not declared in a header file
using namespace std;
using namespace Rose;

#define OUTPUT_DEBUGGING_FUNCTION_BOUNDARIES 0
#define OUTPUT_DEBUGGING_FUNCTION_INTERNALS 0

static const char *kInlineTypeOperandStandaloneSuppressAttribute =
    "rex:inline-type-operand-standalone-suppress";
#define OUTPUT_DEBUGGING_UNPARSE_INFO 0

// Output the class name and function names as we unparse (for debugging)
#define OUTPUT_DEBUGGING_CLASS_NAME 0
#define OUTPUT_DEBUGGING_FUNCTION_NAME 0
#define OUTPUT_HIDDEN_LIST_DATA 0

// DQ (4/15/2021): This is required to be set (to one) by default.
#define HIGH_FEDELITY_TOKEN_UNPARSING 1

#define OUTPUT_PLACEHOLDER_COMMENTS_FOR_SUPRESSED_TEMPLATE_IR_NODES 0

// DQ (2/5/2021): Adding debugging support for token-based unparsing.
#define DEBUG_USING_CURPRINT 0

#define DEBUG_TOKEN_STREAM_UNPARSING 0

#define ENABLE_unparsedPartiallyUsingTokenStream 1

namespace {
struct FunctionLikeMacroDirective {
  int line = 0;
  int col = 0;
  std::string name;
  bool is_define = false;
  bool is_function_like = false;
};

using FunctionLikeMacroDirectiveMap =
    std::map<std::string, std::vector<FunctionLikeMacroDirective>>;

bool isMacroIdentifierChar(char ch) {
  return std::isalnum(static_cast<unsigned char>(ch)) != 0 || ch == '_';
}

bool parseMacroDirective(const std::string &directive,
                         PreprocessingInfo::DirectiveType type,
                         std::string &name, bool &is_function_like) {
  const char *keyword = nullptr;
  switch (type) {
  case PreprocessingInfo::CpreprocessorDefineDeclaration:
    keyword = "define";
    break;
  case PreprocessingInfo::CpreprocessorUndefDeclaration:
    keyword = "undef";
    break;
  default:
    return false;
  }

  size_t pos = 0;
  while (pos < directive.size() &&
         std::isspace(static_cast<unsigned char>(directive[pos])) != 0) {
    ++pos;
  }
  if (pos >= directive.size() || directive[pos] != '#') {
    return false;
  }
  ++pos;
  while (pos < directive.size() &&
         std::isspace(static_cast<unsigned char>(directive[pos])) != 0) {
    ++pos;
  }

  const size_t keyword_len = std::strlen(keyword);
  if (directive.compare(pos, keyword_len, keyword) != 0) {
    return false;
  }
  pos += keyword_len;

  while (pos < directive.size() &&
         std::isspace(static_cast<unsigned char>(directive[pos])) != 0) {
    ++pos;
  }

  const size_t start = pos;
  while (pos < directive.size() && isMacroIdentifierChar(directive[pos])) {
    ++pos;
  }
  if (start == pos) {
    return false;
  }

  name = directive.substr(start, pos - start);
  is_function_like =
      type == PreprocessingInfo::CpreprocessorDefineDeclaration &&
      pos < directive.size() && directive[pos] == '(';
  return true;
}

SgType *stripFunctionReturnTypeForInlineDefinitionCheck(SgType *type) {
  if (type == nullptr) {
    return nullptr;
  }

  return type->stripType(SgType::STRIP_MODIFIER_TYPE |
                         SgType::STRIP_REFERENCE_TYPE |
                         SgType::STRIP_RVALUE_REFERENCE_TYPE |
                         SgType::STRIP_POINTER_TYPE | SgType::STRIP_ARRAY_TYPE);
}

bool declarationNeedsInlineFunctionReturnTypeDefinition(
    SgDeclarationStatement *decl) {
  if (decl == nullptr) {
    return false;
  }

  auto is_function_signature_owned_decl =
      [](SgDeclarationStatement *candidate) -> bool {
    if (candidate == nullptr) {
      return false;
    }

    SgNode *parent = candidate->get_parent();
    return isSgFunctionDeclaration(parent) != nullptr ||
           isSgMemberFunctionDeclaration(parent) != nullptr ||
           isSgFunctionParameterScope(parent) != nullptr ||
           isSgDeclarationScope(parent) != nullptr;
  };

  if (SgClassDeclaration *class_decl = isSgClassDeclaration(decl)) {
    if (SgClassDeclaration *first_nondef = isSgClassDeclaration(
            class_decl->get_firstNondefiningDeclaration())) {
      class_decl = first_nondef;
    }
    if (SgClassDeclaration *def_decl =
            isSgClassDeclaration(class_decl->get_definingDeclaration())) {
      if (is_function_signature_owned_decl(def_decl) &&
          (!def_decl->get_isAutonomousDeclaration() ||
           def_decl->get_isUnNamed())) {
        return true;
      }
    }
    return is_function_signature_owned_decl(class_decl) &&
           (!class_decl->get_isAutonomousDeclaration() ||
            class_decl->get_isUnNamed());
  }

  if (SgEnumDeclaration *enum_decl = isSgEnumDeclaration(decl)) {
    if (SgEnumDeclaration *first_nondef =
            isSgEnumDeclaration(enum_decl->get_firstNondefiningDeclaration())) {
      enum_decl = first_nondef;
    }
    if (SgEnumDeclaration *def_decl =
            isSgEnumDeclaration(enum_decl->get_definingDeclaration())) {
      if (is_function_signature_owned_decl(def_decl) &&
          (!def_decl->get_isAutonomousDeclaration() ||
           def_decl->get_isUnNamed())) {
        return true;
      }
    }
    return is_function_signature_owned_decl(enum_decl) &&
           (!enum_decl->get_isAutonomousDeclaration() ||
            enum_decl->get_isUnNamed());
  }

  return false;
}

bool scopeHasTransformedDeclarations(SgScopeStatement *scope) {
  if (scope == nullptr) {
    return false;
  }

  SgDeclarationStatementPtrList *decls = nullptr;
  if (SgGlobal *global = isSgGlobal(scope)) {
    decls = &global->get_declarations();
  } else if (SgNamespaceDefinitionStatement *ns_def =
                 isSgNamespaceDefinitionStatement(scope)) {
    decls = &ns_def->get_declarations();
  } else if (SgDeclarationScope *decl_scope = isSgDeclarationScope(scope)) {
    decls = &decl_scope->get_declarations();
  }

  if (decls == nullptr) {
    return false;
  }

  for (SgDeclarationStatement *decl : *decls) {
    if (decl == nullptr) {
      continue;
    }
    if (decl->isTransformation() || decl->get_containsTransformation()) {
      return true;
    }
    if (SgClassDeclaration *class_decl = isSgClassDeclaration(decl)) {
      if (SgClassDefinition *class_def = class_decl->get_definition()) {
        if (class_def->isTransformation() ||
            class_def->get_containsTransformation()) {
          return true;
        }
      }
    }
  }

  return false;
}

bool isWithinTransformedDeclarationScope(SgNode *node) {
  for (SgNode *cursor = node; cursor != nullptr;
       cursor = cursor->get_parent()) {
    SgScopeStatement *scope = isSgScopeStatement(cursor);
    if (scope == nullptr) {
      continue;
    }
    if (scopeHasTransformedDeclarations(scope)) {
      return true;
    }
  }

  return false;
}

bool functionReturnTypeNeedsInlineDefinition(SgType *type) {
  SgType *stripped_type = stripFunctionReturnTypeForInlineDefinitionCheck(type);
  if (stripped_type == nullptr) {
    return false;
  }

  if (SgClassType *class_type = isSgClassType(stripped_type)) {
    return declarationNeedsInlineFunctionReturnTypeDefinition(
        class_type->get_declaration());
  }

  if (SgEnumType *enum_type = isSgEnumType(stripped_type)) {
    return declarationNeedsInlineFunctionReturnTypeDefinition(
        enum_type->get_declaration());
  }

  return false;
}

bool parameterTypeNeedsInlineDefinition(SgType *type) {
  return functionReturnTypeNeedsInlineDefinition(type);
}

void enableInlineDefinitionForFunctionReturnTypeIfNeeded(SgUnparse_Info &info,
                                                         SgType *type) {
  if (!functionReturnTypeNeedsInlineDefinition(type)) {
    return;
  }

  info.unset_SkipClassDefinition();
  info.unset_SkipEnumDefinition();
  info.unset_SkipClassSpecifier();
}

const FunctionLikeMacroDirectiveMap &
getFunctionLikeMacroDirectivesForSourceFile(SgSourceFile *source_file) {
  static std::map<SgSourceFile *, FunctionLikeMacroDirectiveMap> cache;

  ROSE_ASSERT(source_file != nullptr);

  std::map<SgSourceFile *, FunctionLikeMacroDirectiveMap>::iterator cached =
      cache.find(source_file);
  if (cached != cache.end()) {
    return cached->second;
  }

  FunctionLikeMacroDirectiveMap directives;
  if (ROSEAttributesListContainerPtr container =
          source_file->get_preprocessorDirectivesAndCommentsList()) {
    for (const auto &entry : container->getList()) {
      const std::string &filename = entry.first;
      ROSEAttributesList *list = entry.second;
      if (list == nullptr) {
        continue;
      }

      std::vector<FunctionLikeMacroDirective> &file_directives =
          directives[filename];
      for (PreprocessingInfo *info : list->getList()) {
        if (info == nullptr) {
          continue;
        }

        std::string macro_name;
        bool is_function_like = false;
        if (!parseMacroDirective(info->getString(), info->getTypeOfDirective(),
                                 macro_name, is_function_like)) {
          continue;
        }

        Sg_File_Info *file_info = info->get_file_info();
        file_directives.push_back(FunctionLikeMacroDirective{
            file_info != nullptr ? file_info->get_line()
                                 : info->getLineNumber(),
            file_info != nullptr ? file_info->get_col()
                                 : info->getColumnNumber(),
            macro_name,
            info->getTypeOfDirective() ==
                PreprocessingInfo::CpreprocessorDefineDeclaration,
            is_function_like});
      }

      std::stable_sort(file_directives.begin(), file_directives.end(),
                       [](const FunctionLikeMacroDirective &lhs,
                          const FunctionLikeMacroDirective &rhs) {
                         if (lhs.line != rhs.line) {
                           return lhs.line < rhs.line;
                         }
                         return lhs.col < rhs.col;
                       });
    }
  }

  return cache.emplace(source_file, std::move(directives)).first->second;
}

bool isFunctionLikeMacroDefinedAt(
    const FunctionLikeMacroDirectiveMap &directives,
    const std::string &filename, int line, int col, const std::string &name) {
  if (line <= 0 || name.empty()) {
    return false;
  }

  FunctionLikeMacroDirectiveMap::const_iterator file_it =
      directives.find(filename);
  if (file_it == directives.end()) {
    return false;
  }

  bool is_defined = false;
  bool is_function_like = false;
  for (const FunctionLikeMacroDirective &directive : file_it->second) {
    if (directive.line > line ||
        (directive.line == line && directive.col >= col)) {
      break;
    }

    if (directive.name != name) {
      continue;
    }

    if (directive.is_define) {
      is_defined = true;
      is_function_like = directive.is_function_like;
    } else {
      is_defined = false;
      is_function_like = false;
    }
  }

  return is_defined && is_function_like;
}

bool shouldParenthesizeFunctionNameForMacro(
    SgFunctionDeclaration *funcdecl_stmt) {
  if (funcdecl_stmt == nullptr || funcdecl_stmt->get_file_info() == nullptr ||
      funcdecl_stmt->get_file_info()->isCompilerGenerated()) {
    return false;
  }

  if (funcdecl_stmt->get_name().is_null() ||
      funcdecl_stmt->get_name().getString().empty()) {
    return false;
  }

  if (funcdecl_stmt->get_name_qualification_length() > 0 ||
      funcdecl_stmt->get_global_qualification_required()) {
    return false;
  }

  SgSourceFile *source_file =
      SageInterface::getEnclosingSourceFile(funcdecl_stmt, true);
  if (source_file == nullptr) {
    return false;
  }

  const FunctionLikeMacroDirectiveMap &directives =
      getFunctionLikeMacroDirectivesForSourceFile(source_file);

  Sg_File_Info *start = funcdecl_stmt->get_startOfConstruct();
  const int line = start != nullptr
                       ? start->get_line()
                       : funcdecl_stmt->get_file_info()->get_line();
  const int col = start != nullptr ? start->get_col()
                                   : funcdecl_stmt->get_file_info()->get_col();

  return isFunctionLikeMacroDefinedAt(
      directives, funcdecl_stmt->get_file_info()->get_filenameString(), line,
      col, funcdecl_stmt->get_name().getString());
}

SgTryStmt *getFunctionTryStmt(SgFunctionDefinition *definition) {
  if (definition == NULL) {
    return NULL;
  }
  SgBasicBlock *body = definition->get_body();
  if (body == NULL) {
    return NULL;
  }
  const SgStatementPtrList &statements = body->get_statements();
  if (statements.size() != 1) {
    return NULL;
  }
  SgTryStmt *try_stmt = isSgTryStmt(statements.front());
  if (try_stmt == NULL) {
    return NULL;
  }
  if (try_stmt->get_is_function_try_block() == false) {
    return NULL;
  }
  return try_stmt;
}

std::string trimRequiresClauseText(const std::string &text) {
  size_t begin = 0;
  while (begin < text.size() &&
         std::isspace(static_cast<unsigned char>(text[begin]))) {
    ++begin;
  }

  size_t end = text.size();
  while (end > begin &&
         std::isspace(static_cast<unsigned char>(text[end - 1]))) {
    --end;
  }

  return text.substr(begin, end - begin);
}

void unparseRequiresClauseExpression(Unparse_ExprStmt *expr_unparser,
                                     SgExpression *requires_clause,
                                     SgUnparse_Info &info);

bool isParenthesizedRequiresClauseText(const std::string &text) {
  std::string trimmed = trimRequiresClauseText(text);
  if (trimmed.size() < 2 || trimmed.front() != '(' || trimmed.back() != ')') {
    return false;
  }

  int depth = 0;
  for (size_t i = 0; i < trimmed.size(); ++i) {
    if (trimmed[i] == '(') {
      ++depth;
    } else if (trimmed[i] == ')') {
      --depth;
      if (depth == 0 && i + 1 != trimmed.size()) {
        return false;
      }
      if (depth < 0) {
        return false;
      }
    }
  }

  return depth == 0;
}

bool isNormalizedClassTemplateMemberFunction(
    const SgTemplateMemberFunctionDeclaration *decl) {
  return decl != NULL && decl->get_marked_as_frontend_normalization() &&
         decl->get_associatedClassDeclaration() != NULL;
}

using TemplateParamSubstitution = std::map<std::string, std::string>;

std::string getTemplateParameterName(const SgTemplateParameter *param) {
  if (param == NULL) {
    return "";
  }

  if (SgInitializedName *init_name = param->get_initializedName()) {
    return init_name->get_name().getString();
  }
  if (SgTemplateType *template_type = isSgTemplateType(param->get_type())) {
    return template_type->get_name().getString();
  }
  if (SgTemplateDeclaration *template_decl =
          isSgTemplateDeclaration(param->get_templateDeclaration())) {
    return template_decl->get_name().getString();
  }

  return "";
}

std::string substituteTemplateParameterName(
    const std::string &name, const TemplateParamSubstitution *substitutions) {
  if (substitutions == NULL || name.empty()) {
    return name;
  }

  TemplateParamSubstitution::const_iterator found = substitutions->find(name);
  if (found == substitutions->end() || found->second.empty()) {
    return name;
  }

  return found->second;
}

TemplateParamSubstitution buildTemplateParameterSubstitutionMap(
    const SgTemplateParameterPtrList &canonical_params,
    const SgTemplateParameterPtrList &override_params) {
  TemplateParamSubstitution substitutions;
  const size_t pair_count =
      std::min(canonical_params.size(), override_params.size());

  for (size_t i = 0; i < pair_count; ++i) {
    std::string canonical_name = getTemplateParameterName(canonical_params[i]);
    std::string override_name = getTemplateParameterName(override_params[i]);
    if (!canonical_name.empty() && !override_name.empty()) {
      substitutions[canonical_name] = override_name;
    }
  }

  return substitutions;
}

std::string buildTemplateParameterArgumentList(
    const SgTemplateParameterPtrList &templateParameterList,
    const TemplateParamSubstitution *substitutions = NULL) {
  std::string args = "<";
  bool need_separator = false;

  for (SgTemplateParameter *param : templateParameterList) {
    if (param == NULL) {
      continue;
    }

    std::string argument_name;
    bool is_pack = param->get_is_parameter_pack();
    if (SgInitializedName *init_name = param->get_initializedName()) {
      argument_name = init_name->get_name().str();
      is_pack = is_pack || init_name->get_is_parameter_pack() ||
                init_name->get_is_pack_element();
    }

    if (argument_name.empty()) {
      if (SgTemplateType *template_type = isSgTemplateType(param->get_type())) {
        argument_name = template_type->get_name().str();
        is_pack = is_pack || template_type->get_packed();
      }
    }

    if (argument_name.empty()) {
      if (SgTemplateDeclaration *template_decl =
              isSgTemplateDeclaration(param->get_templateDeclaration())) {
        argument_name = template_decl->get_name().str();
      }
    }

    if (argument_name.empty()) {
      continue;
    }
    argument_name =
        substituteTemplateParameterName(argument_name, substitutions);

    if (need_separator) {
      args += ",";
    }

    bool has_pack_suffix =
        argument_name.size() >= 3 &&
        argument_name.compare(argument_name.size() - 3, 3, "...") == 0;
    args += argument_name;
    if (is_pack && !has_pack_suffix) {
      args += "...";
    }
    need_separator = true;
  }

  if (!need_separator) {
    return "";
  }

  args += ">";
  return args;
}

std::string buildTemplateArgumentListString(
    const SgTemplateArgumentPtrList &templateArgumentList) {
  return globalUnparseToString(&templateArgumentList, NULL);
}

std::string buildSubstitutedExpressionString(
    SgExpression *expr, const TemplateParamSubstitution *substitutions);
std::string
buildSubstitutedTypeString(SgType *type,
                           const TemplateParamSubstitution *substitutions);

std::string buildSubstitutedTemplateArgumentString(
    SgTemplateArgument *arg, const TemplateParamSubstitution *substitutions) {
  if (arg == NULL) {
    return "";
  }

  switch (arg->get_argumentType()) {
  case SgTemplateArgument::type_argument:
  case SgTemplateArgument::template_template_argument:
    return buildSubstitutedTypeString(arg->get_type(), substitutions);

  case SgTemplateArgument::nontype_argument:
    return buildSubstitutedExpressionString(arg->get_expression(),
                                            substitutions);

  default:
    return globalUnparseToString(arg, NULL);
  }
}

std::string buildSubstitutedTemplateArgumentListString(
    const SgTemplateArgumentPtrList &templateArgumentList,
    const TemplateParamSubstitution *substitutions) {
  std::string args = "<";
  bool need_separator = false;

  for (SgTemplateArgument *arg : templateArgumentList) {
    std::string argument =
        buildSubstitutedTemplateArgumentString(arg, substitutions);
    if (argument.empty()) {
      continue;
    }

    if (need_separator) {
      args += ",";
    }
    args += argument;
    need_separator = true;
  }

  if (!need_separator) {
    return "";
  }

  args += ">";
  return args;
}

std::string buildSubstitutedExpressionString(
    SgExpression *expr, const TemplateParamSubstitution *substitutions) {
  if (expr == NULL) {
    return "";
  }

  if (SgVarRefExp *var_ref = isSgVarRefExp(expr)) {
    if (SgVariableSymbol *symbol = var_ref->get_symbol()) {
      return substituteTemplateParameterName(symbol->get_name().getString(),
                                             substitutions);
    }
  }
  if (SgNonrealRefExp *nonreal_ref = isSgNonrealRefExp(expr)) {
    if (SgNonrealSymbol *symbol = nonreal_ref->get_symbol()) {
      return substituteTemplateParameterName(symbol->get_name().getString(),
                                             substitutions);
    }
  }

  return globalUnparseToString(expr, NULL);
}

std::string
buildSubstitutedTypeString(SgType *type,
                           const TemplateParamSubstitution *substitutions) {
  if (type == NULL) {
    return "";
  }

  if (SgModifierType *modifier_type = isSgModifierType(type)) {
    return buildSubstitutedTypeString(modifier_type->get_base_type(),
                                      substitutions);
  }
  if (SgTypedefType *typedef_type = isSgTypedefType(type)) {
    return buildSubstitutedTypeString(typedef_type->get_base_type(),
                                      substitutions);
  }
  if (SgPointerType *pointer_type = isSgPointerType(type)) {
    return buildSubstitutedTypeString(pointer_type->get_base_type(),
                                      substitutions) +
           " *";
  }
  if (SgReferenceType *reference_type = isSgReferenceType(type)) {
    return buildSubstitutedTypeString(reference_type->get_base_type(),
                                      substitutions) +
           " &";
  }
  if (SgRvalueReferenceType *reference_type = isSgRvalueReferenceType(type)) {
    return buildSubstitutedTypeString(reference_type->get_base_type(),
                                      substitutions) +
           " &&";
  }
  if (SgArrayType *array_type = isSgArrayType(type)) {
    std::string result =
        buildSubstitutedTypeString(array_type->get_base_type(), substitutions);
    result += "[";
    if (array_type->get_index() != NULL) {
      result += buildSubstitutedExpressionString(array_type->get_index(),
                                                 substitutions);
    }
    result += "]";
    return result;
  }
  if (SgTemplateType *template_type = isSgTemplateType(type)) {
    std::string result = substituteTemplateParameterName(
        template_type->get_name().getString(), substitutions);
    if (!template_type->get_tpl_args().empty()) {
      result += buildSubstitutedTemplateArgumentListString(
          template_type->get_tpl_args(), substitutions);
    }
    if (template_type->get_packed()) {
      result += "...";
    }
    return result;
  }
  if (SgNonrealType *nonreal_type = isSgNonrealType(type)) {
    SgNonrealDecl *nonreal_decl =
        isSgNonrealDecl(nonreal_type->get_declaration());
    std::string result;

    if (nonreal_decl != NULL &&
        nonreal_decl->get_templateDeclaration() == NULL) {
      SgDeclarationScope *decl_scope =
          isSgDeclarationScope(nonreal_decl->get_parent());
      SgNonrealDecl *parent_nonreal =
          decl_scope != NULL ? isSgNonrealDecl(decl_scope->get_parent()) : NULL;
      if (parent_nonreal != NULL) {
        result += buildSubstitutedTypeString(parent_nonreal->get_type(),
                                             substitutions);
        result += "::";
      } else if (nonreal_decl->get_has_global_qualifier()) {
        result += "::";
      }
    }

    result += substituteTemplateParameterName(
        nonreal_type->get_name().getString(), substitutions);
    if (nonreal_decl != NULL && (!nonreal_decl->get_tpl_args().empty() ||
                                 nonreal_decl->get_is_nonreal_template())) {
      if (nonreal_decl->get_tpl_args().empty()) {
        result += "<>";
      } else {
        result += buildSubstitutedTemplateArgumentListString(
            nonreal_decl->get_tpl_args(), substitutions);
      }
    }
    return result;
  }
  if (SgClassType *class_type = isSgClassType(type)) {
    SgClassDeclaration *class_decl =
        isSgClassDeclaration(class_type->get_declaration());
    if (SgTemplateInstantiationDecl *inst_decl =
            isSgTemplateInstantiationDecl(class_decl)) {
      std::string name = inst_decl->get_templateName().getString();
      if (name.empty()) {
        name = inst_decl->get_name().getString();
      }
      return name + buildSubstitutedTemplateArgumentListString(
                        inst_decl->get_templateArguments(), substitutions);
    }
    if (class_decl != NULL) {
      return class_decl->get_name().getString();
    }
  }

  return globalUnparseToString(type, NULL);
}

std::string buildTemplateParameterListString(
    const SgTemplateParameterPtrList &templateParameterList,
    const TemplateParamSubstitution *substitutions = NULL) {
  std::string result =
      buildTemplateParameterArgumentList(templateParameterList, substitutions);
  if (!result.empty()) {
    return result;
  }

  return globalUnparseToString(&templateParameterList, NULL);
}

std::string buildClassLikeQualifierSegment(
    SgDeclarationStatement *decl,
    const SgTemplateParameterPtrList *overrideTemplateParameters = NULL,
    const TemplateParamSubstitution *substitutions = NULL) {
  if (decl == NULL) {
    return "";
  }

  if (SgTemplateClassDeclaration *template_decl =
          isSgTemplateClassDeclaration(decl)) {
    std::string name = template_decl->get_templateName().getString();
    if (name.empty()) {
      name = template_decl->get_name().getString();
    }

    const SgTemplateArgumentPtrList &specialization_args =
        template_decl->get_templateSpecializationArguments();
    if (!specialization_args.empty() &&
        template_decl->get_specialization() !=
            SgDeclarationStatement::e_no_specialization) {
      if (substitutions != NULL && !substitutions->empty()) {
        return name + buildSubstitutedTemplateArgumentListString(
                          specialization_args, substitutions);
      }
      return name + buildTemplateArgumentListString(specialization_args);
    }

    if (overrideTemplateParameters != NULL &&
        overrideTemplateParameters->empty() == false) {
      std::string args =
          buildTemplateParameterListString(*overrideTemplateParameters);
      if (!args.empty()) {
        return name + args;
      }
    }

    if (substitutions != NULL && !substitutions->empty() &&
        !template_decl->get_templateParameters().empty()) {
      std::string args = buildTemplateParameterListString(
          template_decl->get_templateParameters(), substitutions);
      if (!args.empty()) {
        return name + args;
      }
    }

    if (!template_decl->get_templateParameters().empty()) {
      std::string args = buildTemplateParameterListString(
          template_decl->get_templateParameters());
      if (!args.empty()) {
        return name + args;
      }
    }

    return name;
  }

  if (SgTemplateInstantiationDecl *inst_decl =
          isSgTemplateInstantiationDecl(decl)) {
    std::string name = inst_decl->get_templateName().getString();
    if (name.empty()) {
      name = inst_decl->get_name().getString();
    }
    if (!inst_decl->get_templateArguments().empty()) {
      if (substitutions != NULL && !substitutions->empty()) {
        return name + buildSubstitutedTemplateArgumentListString(
                          inst_decl->get_templateArguments(), substitutions);
      }
      return name + buildTemplateArgumentListString(
                        inst_decl->get_templateArguments());
    }
    return name;
  }

  if (SgClassDeclaration *class_decl = isSgClassDeclaration(decl)) {
    return class_decl->get_name().getString();
  }

  if (SgNamespaceDeclarationStatement *ns_decl =
          isSgNamespaceDeclarationStatement(decl)) {
    return ns_decl->get_name().getString();
  }

  return "";
}

std::vector<SgDeclarationStatement *>
collectQualificationDeclarationChain(SgDeclarationStatement *associated_decl,
                                     SgScopeStatement *fallback_scope) {
  std::vector<SgDeclarationStatement *> chain;

  auto append_decl = [&](SgDeclarationStatement *decl) {
    if (decl != NULL &&
        std::find(chain.begin(), chain.end(), decl) == chain.end()) {
      chain.push_back(decl);
    }
  };

  append_decl(associated_decl);

  SgNode *cursor = associated_decl != NULL
                       ? associated_decl->get_scope()
                       : static_cast<SgNode *>(fallback_scope);
  while (cursor != NULL) {
    if (SgClassDefinition *class_def = isSgClassDefinition(cursor)) {
      append_decl(class_def->get_declaration());
    } else if (SgTemplateClassDefinition *tpl_class_def =
                   isSgTemplateClassDefinition(cursor)) {
      append_decl(tpl_class_def->get_declaration());
    } else if (SgTemplateInstantiationDefn *inst_def =
                   isSgTemplateInstantiationDefn(cursor)) {
      append_decl(inst_def->get_declaration());
    } else if (SgNamespaceDefinitionStatement *ns_def =
                   isSgNamespaceDefinitionStatement(cursor)) {
      append_decl(ns_def->get_namespaceDeclaration());
    }
    cursor = cursor->get_parent();
  }

  return chain;
}

std::vector<SgTemplateClassDeclaration *>
collectTemplateClassChain(SgDeclarationStatement *associated_decl,
                          SgScopeStatement *fallback_scope) {
  std::vector<SgTemplateClassDeclaration *> chain;

  auto append_template_decl = [&](SgDeclarationStatement *decl) {
    SgTemplateClassDeclaration *template_decl =
        isSgTemplateClassDeclaration(decl);
    if (template_decl != NULL &&
        std::find(chain.begin(), chain.end(), template_decl) == chain.end()) {
      chain.push_back(template_decl);
    }
  };

  append_template_decl(associated_decl);

  SgNode *cursor = associated_decl != NULL
                       ? associated_decl->get_scope()
                       : static_cast<SgNode *>(fallback_scope);
  while (cursor != NULL) {
    if (SgClassDefinition *class_def = isSgClassDefinition(cursor)) {
      append_template_decl(class_def->get_declaration());
    } else if (SgTemplateClassDefinition *tpl_class_def =
                   isSgTemplateClassDefinition(cursor)) {
      append_template_decl(tpl_class_def->get_declaration());
    } else if (SgTemplateInstantiationDefn *inst_def =
                   isSgTemplateInstantiationDefn(cursor)) {
      append_template_decl(inst_def->get_declaration());
    }
    cursor = cursor->get_parent();
  }

  return chain;
}

std::string buildNormalizedTemplateMemberFunctionQualifier(
    SgTemplateMemberFunctionDeclaration *decl) {
  ASSERT_not_null(decl);

  SgDeclarationStatement *associated_decl =
      decl->get_associatedClassDeclaration();
  if (associated_decl == NULL) {
    return decl->get_qualified_name_prefix().str();
  }

  std::vector<SgDeclarationStatement *> chain =
      collectQualificationDeclarationChain(associated_decl, decl->get_scope());
  if (chain.empty()) {
    return decl->get_qualified_name_prefix().str();
  }

  std::string qualifier;
  TemplateParamSubstitution substitutions;
  for (auto it = chain.rbegin(); it != chain.rend(); ++it) {
    SgDeclarationStatement *segment_decl = *it;
    const SgTemplateParameterPtrList *override_params = NULL;
    const TemplateParamSubstitution *segment_substitutions = NULL;
    if (segment_decl == associated_decl &&
        decl->get_templateParameters().empty() == false) {
      if (SgTemplateClassDeclaration *template_decl =
              isSgTemplateClassDeclaration(segment_decl)) {
        substitutions = buildTemplateParameterSubstitutionMap(
            template_decl->get_templateParameters(),
            decl->get_templateParameters());
        if (!substitutions.empty()) {
          segment_substitutions = &substitutions;
        }
        if (template_decl->get_templateSpecializationArguments().empty()) {
          override_params = &decl->get_templateParameters();
        }
      } else if (SgTemplateInstantiationDecl *inst_decl =
                     isSgTemplateInstantiationDecl(segment_decl)) {
        if (SgTemplateClassDeclaration *template_decl =
                isSgTemplateClassDeclaration(
                    inst_decl->get_templateDeclaration())) {
          substitutions = buildTemplateParameterSubstitutionMap(
              template_decl->get_templateParameters(),
              decl->get_templateParameters());
          if (!substitutions.empty()) {
            segment_substitutions = &substitutions;
          }
        }
      }
    }

    std::string segment = buildClassLikeQualifierSegment(
        segment_decl, override_params, segment_substitutions);
    if (segment.empty()) {
      continue;
    }
    qualifier += segment;
    qualifier += "::";
  }

  if (decl->get_global_qualification_required()) {
    qualifier = "::" + qualifier;
  }

  return qualifier;
}

void unparseTrailingRequiresClauseIfPresent(
    Unparse_ExprStmt *expr_unparser, SgFunctionDeclaration *funcdecl_stmt,
    SgUnparse_Info &info) {
  ASSERT_not_null(expr_unparser);
  ASSERT_not_null(funcdecl_stmt);

  if (SgExpression *requires_clause =
          funcdecl_stmt->get_trailingRequiresClause()) {
    SgUnparse_Info rinfo(info);
    rinfo.set_SkipClassDefinition();
    rinfo.set_SkipEnumDefinition();
    expr_unparser->curprint(" requires ");
    unparseRequiresClauseExpression(expr_unparser, requires_clause, rinfo);
  }
}

void unparseRequiresClauseExpression(Unparse_ExprStmt *expr_unparser,
                                     SgExpression *requires_clause,
                                     SgUnparse_Info &info) {
  ASSERT_not_null(expr_unparser);
  ASSERT_not_null(requires_clause);

  bool needs_grouping = true;
  if (SgRequiresExpr *req = isSgRequiresExpr(requires_clause)) {
    needs_grouping =
        !isParenthesizedRequiresClauseText(req->get_expressionString());
  }
  if (needs_grouping) {
    expr_unparser->curprint("(");
  }
  expr_unparser->unparseExpression(requires_clause, info);
  if (needs_grouping) {
    expr_unparser->curprint(")");
  }
}

bool requiresTrailingReturnTypeSyntax(const SgType *return_type) {
  const SgDeclType *decl_type = isSgDeclType(return_type);
  if (decl_type == NULL) {
    return false;
  }

  const SgExpression *base_expression = decl_type->get_base_expression();
  if (base_expression == NULL) {
    return false;
  }

  // Function return types spelled as decltype(...) are safest to preserve
  // using trailing return syntax ("auto f(...) -> decltype(...)"), especially
  // when the expression references function parameters that are not in scope
  // for prefix return-type position.
  return true;
}

bool isBaseOrDelegatingCtorPreinitializer(const SgInitializedName *ctor_init) {
  if (ctor_init == NULL) {
    return false;
  }

  switch (ctor_init->get_preinitialization()) {
  case SgInitializedName::e_virtual_base_class:
  case SgInitializedName::e_nonvirtual_base_class:
  case SgInitializedName::e_delegation_constructor:
    return true;

  default:
    return false;
  }
}

void unparseCtorPreinitializerDesignator(Unparse_ExprStmt *expr_unparser,
                                         Unparser *backend_unparser,
                                         SgInitializedName *ctor_init,
                                         SgUnparse_Info &type_info) {
  ASSERT_not_null(expr_unparser);
  ASSERT_not_null(backend_unparser);
  ASSERT_not_null(ctor_init);

  SgInitializer *designator_initializer = ctor_init->get_initializer();
  SgConstructorInitializer *ctor_initializer =
      isSgConstructorInitializer(designator_initializer);
  if (ctor_initializer == NULL) {
    if (SgAssignInitializer *assign_initializer =
            isSgAssignInitializer(designator_initializer)) {
      ctor_initializer =
          isSgConstructorInitializer(assign_initializer->get_operand());
    }
  }
  if (isBaseOrDelegatingCtorPreinitializer(ctor_init)) {
    // The preinitializer designator names the declared base/delegating target,
    // not the nested constructor-expression type. Prefer the initialized
    // entity's type so we preserve source-written qualifiers and template args.
    SgType *target_type = ctor_init->get_type();
    if (target_type == NULL && ctor_initializer != NULL) {
      target_type = ctor_initializer->get_type();
    }

    if (target_type != NULL) {
      SgUnparse_Info designator_info(type_info);
      designator_info.set_SkipClassDefinition();
      designator_info.set_SkipEnumDefinition();
      designator_info.set_reference_node_for_qualification(NULL);
      designator_info.set_SkipClassSpecifier();
      backend_unparser->u_type->unparseType(target_type, designator_info);
      return;
    }
  }

  SgName nameQualifier = ctor_init->get_qualified_name_prefix();
  if (!nameQualifier.is_null()) {
    expr_unparser->curprint(nameQualifier.str());
  }
  expr_unparser->curprint(ctor_init->get_name().str());
}

SgClassDeclaration *
enclosing_class_declaration_from_scope(SgScopeStatement *scope) {
  if (scope == NULL) {
    return NULL;
  }

  if (SgClassDefinition *class_def = isSgClassDefinition(scope)) {
    return class_def->get_declaration();
  }

  if (SgTemplateClassDefinition *template_def =
          isSgTemplateClassDefinition(scope)) {
    return template_def->get_declaration();
  }

  if (SgTemplateInstantiationDefn *inst_def =
          isSgTemplateInstantiationDefn(scope)) {
    return isSgClassDeclaration(inst_def->get_declaration());
  }

  return NULL;
}

size_t count_template_instantiation_class_chain_levels(
    SgClassDeclaration *class_decl) {
  size_t level_count = 0;
  std::set<SgClassDeclaration *> visited_classes;
  for (SgClassDeclaration *current = class_decl;
       current != NULL && visited_classes.insert(current).second;) {
    if (isSgTemplateInstantiationDecl(current) != NULL) {
      ++level_count;
    }

    current = enclosing_class_declaration_from_scope(current->get_scope());
  }

  return level_count;
}

size_t count_enclosing_template_instantiation_levels_for_class_specialization(
    SgTemplateInstantiationDecl *inst_decl) {
  if (inst_decl == NULL) {
    return 0;
  }

  SgTemplateClassDeclaration *template_decl =
      inst_decl->get_templateDeclaration();
  if (template_decl == NULL) {
    return 0;
  }

  size_t level_count = 0;
  std::set<SgScopeStatement *> visited_scopes;
  for (SgScopeStatement *scope = template_decl->get_scope();
       scope != NULL && visited_scopes.insert(scope).second;
       scope = scope->get_scope()) {
    if (isSgTemplateInstantiationDefn(scope) != NULL) {
      ++level_count;
    }
  }

  return level_count;
}
} // namespace

Unparse_ExprStmt::Unparse_ExprStmt(Unparser *unp, std::string fname)
    : UnparseLanguageIndependentConstructs(unp, fname) {
  // Nothing to do here!
}

Unparse_ExprStmt::~Unparse_ExprStmt() {
  // Nothing to do here!
}

void Unparse_ExprStmt::resetTemplateParameterEmissionState() {
  emitted_default_template_args_.clear();
}

void Unparse_ExprStmt::unparseFunctionTryBlock(SgTryStmt *try_stmt,
                                               SgUnparse_Info &ninfo) {
  ASSERT_not_null(try_stmt);
  SgStatement *try_body = try_stmt->get_body();
  ASSERT_not_null(try_body);

  unp->cur.format(try_body, ninfo, FORMAT_BEFORE_NESTED_STATEMENT);
  unparseStatement(try_body, ninfo);
  unp->cur.format(try_body, ninfo, FORMAT_AFTER_NESTED_STATEMENT);

  for (SgStatement *catch_stmt : try_stmt->get_catch_statement_seq()) {
    unparseStatement(catch_stmt, ninfo);
  }
}

string UnparseLanguageIndependentConstructs::token_sequence_position_name(
    UnparseLanguageIndependentConstructs::token_sequence_position_enum_type e) {
  string s;
  switch (e) {
  case e_leading_whitespace_start:
    s = "e_leading_whitespace_start";
    break;
  case e_leading_whitespace_end:
    s = "e_leading_whitespace_end";
    break;
  case e_token_subsequence_start:
    s = "e_token_subsequence_start";
    break;
  case e_token_subsequence_end:
    s = "e_token_subsequence_end";
    break;
  case e_trailing_whitespace_start:
    s = "e_trailing_whitespace_start";
    break;
  case e_trailing_whitespace_end:
    s = "e_trailing_whitespace_end";
    break;

    // DQ (12/31/2014): Added to support the middle subsequence of tokens in the
    // SgIfStmt as a special case.
  case e_else_whitespace_start:
    s = "e_else_whitespace_start";
    break;
  case e_else_whitespace_end:
    s = "e_else_whitespace_end";
    break;

  default: {
    printf("default reached in switch: value = %d \n", e);
  }
  }

  return s;
}

// DQ (6/6/2021): Adding the support to provide offsets to modify the starting
// and ending token sequence to unparse. void
// UnparseLanguageIndependentConstructs::unparseStatementFromTokenStream (
// SgStatement* stmt,
// UnparseLanguageIndependentConstructs::token_sequence_position_enum_type
// e_token_sequence_position_start,
// UnparseLanguageIndependentConstructs::token_sequence_position_enum_type
// e_token_sequence_position_end)
void UnparseLanguageIndependentConstructs::unparseStatementFromTokenStream(
    SgStatement *stmt,
    UnparseLanguageIndependentConstructs::token_sequence_position_enum_type
        e_token_sequence_position_start,
    UnparseLanguageIndependentConstructs::token_sequence_position_enum_type
        e_token_sequence_position_end,
    SgUnparse_Info &info, int start_offset, int end_offset) {
#if DEBUG_TOKEN_STREAM_UNPARSING
  printf("unparseStatementFromTokenStream(stmt = %p = %s): \n", stmt,
         stmt->class_name().c_str());
  printf("   --- stmt: filename = %s \n", stmt->getFilenameString().c_str());
  printf("   --- e_token_sequence_position_start = %d = %s \n",
         e_token_sequence_position_start,
         token_sequence_position_name(e_token_sequence_position_start).c_str());
  printf("   --- e_token_sequence_position_end   = %d = %s \n",
         e_token_sequence_position_end,
         token_sequence_position_name(e_token_sequence_position_end).c_str());
#endif
#if DEBUG_USING_CURPRINT
  curprint(
      string("\n/* In unparseStatementFromTokenStream(stmt,start,end,info): "
             "stmt = ") +
      stmt->class_name() +
      " get_containsTransformationToSurroundingWhitespace = " +
      string(stmt->get_containsTransformationToSurroundingWhitespace()
                 ? "true"
                 : "false") +
      " */\n");
#endif

#if DEBUG_USING_CURPRINT
  if (SgProject::get_verbose() >= 0) {
    string s = "\n/* Unparse a partial token sequence: 1 stmt: stmt = " +
               stmt->class_name() + " */\n";
    curprint(s);
  }
#endif

  // DQ (6/6/2021): Adding the support to provide offsets to modify the starting
  // and ending token sequence to unparse. DQ (5/30/2021): We do want to
  // uniformally call this function (no exception for SgGlobal. The exception
  // here is that if the last token of the file is C/C++ syntax (e.g. "}") then
  // we don't want to output anything.
  // unparseStatementFromTokenStream(stmt,stmt,e_token_sequence_position_start,e_token_sequence_position_end,info);
  unparseStatementFromTokenStream(stmt, stmt, e_token_sequence_position_start,
                                  e_token_sequence_position_end, info,
                                  start_offset, end_offset);

#if DEBUG_TOKEN_STREAM_UNPARSING
  printf("Leaving unparseStatementFromTokenStream(stmt,start,end,info) \n");
#endif
#if DEBUG_USING_CURPRINT
  // curprint("\n/* Leaving
  // unparseStatementFromTokenStream(stmt,start,end,info): */ \n");
  string s =
      string("\n/* Leaving "
             "unparseStatementFromTokenStream(stmt,start,end,info): stmt = ") +
      stmt->class_name() + " */ \n";
  curprint(s);
#endif
}

// DQ (6/6/2021): Adding the support to provide offsets to modify the starting
// and ending token sequence to unparse. void
// UnparseLanguageIndependentConstructs::unparseStatementFromTokenStream (
// // SgStatement* stmt_1, SgStatement* stmt_2,
//    SgLocatedNode* stmt_1, SgLocatedNode* stmt_2,
//    UnparseLanguageIndependentConstructs::token_sequence_position_enum_type
//    e_token_sequence_position_start,
//    UnparseLanguageIndependentConstructs::token_sequence_position_enum_type
//    e_token_sequence_position_end, bool unparseOnlyWhitespace )
void UnparseLanguageIndependentConstructs::unparseStatementFromTokenStream(
    SgLocatedNode *stmt_1, SgLocatedNode *stmt_2,
    UnparseLanguageIndependentConstructs::token_sequence_position_enum_type
        e_token_sequence_position_start,
    UnparseLanguageIndependentConstructs::token_sequence_position_enum_type
        e_token_sequence_position_end,
    SgUnparse_Info &info, bool unparseOnlyWhitespace, int /*start_offset*/,
    int end_offset) {
  // unparseStatementFromTokenStream (stmt, e_leading_whitespace_start,
  // e_token_subsequence_start); Check for the leading token stream for this
  // statement.  Unparse it if the previous statement was unparsed as a token
  // stream.

#if DEBUG_TOKEN_STREAM_UNPARSING
  printf("In unparseStatementFromTokenStream(stmt_1=%p=%s,stmt_2=%p=%s): \n",
         stmt_1, stmt_1->class_name().c_str(), stmt_2,
         stmt_2->class_name().c_str());
  printf("   --- e_token_sequence_position_start = %d = %s \n",
         e_token_sequence_position_start,
         token_sequence_position_name(e_token_sequence_position_start).c_str());
  printf("   --- e_token_sequence_position_end   = %d = %s \n",
         e_token_sequence_position_end,
         token_sequence_position_name(e_token_sequence_position_end).c_str());
  printf("   --- unparseOnlyWhitespace = %s \n",
         unparseOnlyWhitespace ? "true" : "false");
  printf("   --- start_offset                    = %d \n", start_offset);
  printf("   --- end_offset                      = %d \n", end_offset);

  SgDeclarationStatement *declarationStatement_1 =
      isSgDeclarationStatement(stmt_1);
  if (declarationStatement_1 != NULL) {
    printf("   --- declarationStatement_1->get_firstNondefiningDeclaration() = "
           "%p \n",
           declarationStatement_1->get_firstNondefiningDeclaration());
    printf("   --- declarationStatement_1->get_definingDeclaration()         = "
           "%p \n",
           declarationStatement_1->get_definingDeclaration());
  }

  SgDeclarationStatement *declarationStatement_2 =
      isSgDeclarationStatement(stmt_2);
  if (declarationStatement_2 != NULL) {
    printf("   --- declarationStatement_2->get_firstNondefiningDeclaration() = "
           "%p \n",
           declarationStatement_2->get_firstNondefiningDeclaration());
    printf("   --- declarationStatement_2->get_definingDeclaration()         = "
           "%p \n",
           declarationStatement_2->get_definingDeclaration());
  }

  printf("   --- stmt_1->get_file_info()->get_filenameString() = %s \n",
         stmt_1->get_file_info()->get_filenameString().c_str());
  printf("   --- stmt_2->get_file_info()->get_filenameString() = %s \n",
         stmt_2->get_file_info()->get_filenameString().c_str());
#endif

#if DEBUG_USING_CURPRINT
  curprint(
      "\n/* In unparseStatementFromTokenStream(stmt,stmt,start,end,info,bool): "
      "*/");
  string s1 = string("\n/* --- stmt_1 = ") + stmt_1->class_name().c_str() +
              " " + Rose::StringUtility::numberToString(stmt_1) + " */";
  curprint(s1);
  string s2 = string("\n/* --- stmt_2 = ") + stmt_2->class_name().c_str() +
              " " + Rose::StringUtility::numberToString(stmt_2) + " */";
  curprint(s2);
  curprint(string("\n/* --- stmt_1: "
                  "get_containsTransformationToSurroundingWhitespace = ") +
           string(stmt_1->get_containsTransformationToSurroundingWhitespace()
                      ? "true"
                      : "false") +
           " */");
  curprint(string("\n/* --- stmt_2: "
                  "get_containsTransformationToSurroundingWhitespace = ") +
           string(stmt_2->get_containsTransformationToSurroundingWhitespace()
                      ? "true"
                      : "false") +
           " */");
#endif

  if (SgProject::get_verbose() > 0) {
    // Avoid redundant output from unparseStatementFromTokenStream() taking a
    // single SgStatement.
    if (stmt_1 != stmt_2) {
      string s = "/* Unparse a partial token sequence: 2 stmt: stmt_1 = " +
                 stmt_1->class_name() + " stmt_2 = " + stmt_2->class_name() +
                 " */ ";
      curprint(s);
    }
  }

  // DQ (10/27/2018): This is the wrong source file when we are unparsing header
  // files (which share the global scope but for which this function will
  // traverse paranter pointest to find the translation units SgSourceFile
  // (which is that of the input source code and not the associated header
  // file). SgSourceFile* sourceFile =
  // isSgSourceFile(SageInterface::getEnclosingFileNode(stmt_1));
  SgSourceFile *sourceFile = info.get_current_source_file();
  ASSERT_not_null(sourceFile);

  // DQ (12/26/2018): Moved this to the outer function scopw so that we can
  // assert that any element index is less then the tokenVectorSize.
  SgTokenPtrList &tokenVector = sourceFile->get_token_list();
  int tokenVectorSize = tokenVector.size();

  // Note: that there is a single global map that is accessed from the
  // get_tokenSubsequenceMap() function, not one per SgSourceFile. I am not
  // clear if this is an issue for the header file unparsing using the token
  // steam, I think that since the map is based on keys that are IR node
  // pointers, they are all unique. DQ (9/28/2018): There is not one map per
  // file to simplify debugging (and maybe fix some subtle problems).
  std::map<SgNode *, TokenStreamSequenceToNodeMapping *>
      &tokenStreamSequenceMap = sourceFile->get_tokenSubsequenceMap();

  // TokenStreamSequenceToNodeMapping* tokenSubsequence_1 =
  // tokenStreamSequenceMap[stmt_1]; ASSERT_not_null(tokenSubsequence_1);
  TokenStreamSequenceToNodeMapping *tokenSubsequence_1 = NULL;

  // DQ (10/4/2018): This is a essential test to avoid pointers to default
  // constructed objects from appearing accedentally in the STL map.
  if (tokenStreamSequenceMap.find(stmt_1) == tokenStreamSequenceMap.end()) {
  } else {
    tokenSubsequence_1 = tokenStreamSequenceMap[stmt_1];
  }

  // DQ (9/25/2018): I think this is an issue for new IR nodes added to the AST
  // (such a SgIncludeDirective IR nodes within the header file unparsing
  // support). However there is a current bug where the global scope for include
  // files are missing.
  if (tokenSubsequence_1 == NULL) {
    // DQ (11/4/2018): This is not an error when using the unparse to header
    // files with the token-based unparsing.
  }

  // DQ (10/4/2018): Move this to be initialized later so that we can understnad
  // how a bug is happening between where the tokenStreamSequenceMap is accessed
  // and where it is tested below. TokenStreamSequenceToNodeMapping*
  // tokenSubsequence_2 = tokenStreamSequenceMap[stmt_2];
  TokenStreamSequenceToNodeMapping *tokenSubsequence_2 = NULL;

  // tokenSubsequence_2 = tokenStreamSequenceMap[stmt_2];
  // DQ (10/4/2018): This is a essential test to avoid pointers to default
  // constructed objects from appearing accedentally in the STL map.
  if (tokenStreamSequenceMap.find(stmt_2) == tokenStreamSequenceMap.end()) {
  } else {
    // DQ (10/26/2018): Bug fix: I think this was a cut and paste error.
    // tokenSubsequence_2 = tokenStreamSequenceMap[stmt_1];
    tokenSubsequence_2 = tokenStreamSequenceMap[stmt_2];
  }

  // DQ (9/25/2018): I think this is an issue for new IR nodes added to the AST
  // (such a SgIncludeDirective IR nodes within the header file unparsing
  // support.
  if (tokenSubsequence_2 == NULL) {
  }

  // DQ (12/10/2014): The mapping for stmt_2 might not exist, e.g. if it was
  // added as part of a transformation. in this case then there is no associated
  // token stream to output. ASSERT_not_null(tokenSubsequence_2); if
  // (tokenSubsequence_1 != NULL && tokenSubsequence_2 != NULL)
  if (tokenSubsequence_1 != NULL && tokenSubsequence_2 != NULL &&
      tokenSubsequence_1->token_subsequence_start != -1 &&
      tokenSubsequence_2->token_subsequence_start != -1) {
    // DQ (3/22/2021): This fails for test_20_2019.cpp in the codeSegregation
    // regression tests.
    if (tokenSubsequence_1->token_subsequence_start == -1) {
      printf("tokenSubsequence_1->token_subsequence_start = %d \n",
             tokenSubsequence_1->token_subsequence_start);
      printf("tokenSubsequence_2->token_subsequence_start = %d \n",
             tokenSubsequence_2->token_subsequence_start);
    }
    ROSE_ASSERT(tokenSubsequence_1->token_subsequence_start != -1);
    ROSE_ASSERT(tokenSubsequence_2->token_subsequence_start != -1);

#if DEBUG_USING_CURPRINT
    curprint("\n/* In unparseStatementFromTokenStream(): tokenSubsequence_1 != "
             "NULL && tokenSubsequence_2 != NULL */ \n");
#endif

    // This is correct for the SgFunctionDefinition IR node.
    // int start =
    // functionDefinition_tokenSubsequence->leading_whitespace_start; int end =
    // functionDefinition_tokenSubsequence->token_subsequence_start;

    int start = 0;
    int end = 0;

    bool start_reset_because_requestion_position_was_not_defined = false;

    switch (e_token_sequence_position_start) {
    case e_leading_whitespace_start:
      start = tokenSubsequence_1->leading_whitespace_start;

      // DQ (12/26/2018): We have to make sure that we stay in bounds of the
      // number of tokens in the loken list.
      ROSE_ASSERT(start < tokenVectorSize);
      break;

    case e_leading_whitespace_end:
      start = tokenSubsequence_1->leading_whitespace_end;
      ROSE_ASSERT(start >= 0);

      // DQ (12/26/2018): We have to make sure that we stay in bounds of the
      // number of tokens in the loken list.
      ROSE_ASSERT(start < tokenVectorSize);
      break;

    case e_token_subsequence_start:
      start = tokenSubsequence_1->token_subsequence_start;
      ROSE_ASSERT(start >= 0);

      // DQ (12/26/2018): We have to make sure that we stay in bounds of the
      // number of tokens in the loken list.
      ROSE_ASSERT(start < tokenVectorSize);
      break;

    case e_token_subsequence_end:
      start = tokenSubsequence_1->token_subsequence_end;
      ROSE_ASSERT(start >= 0);

      // DQ (12/26/2018): We have to make sure that we stay in bounds of the
      // number of tokens in the loken list.
      ROSE_ASSERT(start < tokenVectorSize);
      break;

    case e_trailing_whitespace_start:
      start = tokenSubsequence_1->trailing_whitespace_start;
      // DQ (6/6/2021): We want to handle the following:
      // unparseStatementFromTokenStream (test_stmt, body,
      // e_trailing_whitespace_start, e_leading_whitespace_end, info); DQ
      // (6/2/2021): Avoid adjustments to the token bounds. DQ (12/10/2014):
      // Note that white space is not always available.
      if (start == -1) {
#if DEBUG_USING_CURPRINT
        curprint("\n/* In unparseStatementFromTokenStream(): reset start to "
                 "tokenSubsequence_1->token_subsequence_end + 1 */ \n");
#endif
        start = tokenSubsequence_1->token_subsequence_end + 1;
        start_reset_because_requestion_position_was_not_defined = true;
      }
      ROSE_ASSERT(start >= 0);
      // DQ (12/26/2018): If this is out of bounds then we have to set it to not
      // available.
      if (start >= tokenVectorSize) {
#if DEBUG_USING_CURPRINT
        curprint("\n/* In unparseStatementFromTokenStream(): reset start to "
                 "tokenVectorSize - 1 */ \n");
#endif
        // start = -1;
        start = tokenVectorSize - 1;

        // Unclear if this should also be set.
        start_reset_because_requestion_position_was_not_defined = true;
      }

      // DQ (12/26/2018): We have to make sure that we stay in bounds of the
      // number of tokens in the loken list.
      ROSE_ASSERT(start < tokenVectorSize);
      break;

    case e_trailing_whitespace_end:
      start = tokenSubsequence_1->trailing_whitespace_end;
      // DQ (12/26/2018): We have to make sure that we stay in bounds of the
      // number of tokens in the loken list.
      ROSE_ASSERT(start < tokenVectorSize);
      break;

    default: {
      printf("Default reached in unparseStatementFromTokenStream(): "
             "e_token_sequence_position_start = %d \n",
             e_token_sequence_position_start);
      ROSE_ABORT();
    }
    }

    if (stmt_1 == stmt_2 &&
        e_token_sequence_position_start == e_token_sequence_position_end) {
      // DQ (1/24/2015): If the token sequence was not defined then we don't
      // want to output any tokens. This fixes cases where we have multiple
      // blocks closed using "}}" (see test2015_96.C). This should trigger a
      // single token to be output. end = start + 1;
#if DEBUG_USING_CURPRINT
      curprint("\n/* In unparseStatementFromTokenStream(): stmt_1 == stmt_2 && "
               "e_token_sequence_position_start == "
               "e_token_sequence_position_end */ \n");
#endif
#if DEBUG_TOKEN_STREAM_UNPARSING || 0
      printf("(stmt_1 == stmt_2 && e_token_sequence_position_start == "
             "e_token_sequence_position_end) == true \n");
      printf("   --- start_reset_because_requestion_position_was_not_defined = "
             "%s \n",
             start_reset_because_requestion_position_was_not_defined ? "true"
                                                                     : "false");
#endif
      if (start_reset_because_requestion_position_was_not_defined == false) {
        // DQ (5/30/2021): We will output the last token depending on if it is
        // syntax (e.g. closing "}"). end = start + 1;

        // printf ("start = %d end = %d \n",start,end);

        ROSE_ASSERT(start < tokenVectorSize);
        if ((start == tokenVectorSize - 1) &&
            tokenVector[start]->get_classification_code() ==
                ROSE_token_ids::C_CXX_SYNTAX) {
#if DEBUG_USING_CURPRINT
          curprint(
              "\n/* In unparseStatementFromTokenStream(): stmt_1 == stmt_2 && "
              "start == end: last token IS syntax (end = start) */ \n");
#endif
          end = start;
        } else {
#if DEBUG_USING_CURPRINT
          curprint(
              "\n/* In unparseStatementFromTokenStream(): stmt_1 == stmt_2 && "
              "start == end: last token is NOT syntax (end = start + 1) */ \n");
#endif
          // DQ (6/3/2021): We want to be precise, and now we uniformally output
          // the ending token. end = start;
          end = start;
        }

        // DQ (12/27/2018): avoid out of range access.
        if (end == tokenVectorSize) {
          // DQ (12/27/2018): This case is special to the last token in the file
          // that is unparsed with its own call to
          // unparseStatementFromTokenStream().
        } else {
          ROSE_ASSERT(end < tokenVectorSize);
        }
      } else {
        // This is a value that will prevent any output of tokens.
        end = start;

        ROSE_ASSERT(end < tokenVectorSize);
      }

      // DQ (12/26/2018): We have to make sure that we stay in bounds of the
      // number of tokens in the loken list.
      if (end >= tokenVectorSize) {
        // DQ (12/27/2018): This is allowed for the last token of the file
        // (which is a special case). printf ("Error: start = %d end = %d
        // tokenVectorSize = %d \n",start,end,tokenVectorSize);
      }

      // DQ (12/27/2018): Modified assertion to test all but the case of end ==
      // tokenVectorSize (above). ROSE_ASSERT(end < tokenVectorSize);
    } else {
#if DEBUG_USING_CURPRINT
      curprint("\n/* In unparseStatementFromTokenStream(): (stmt_1 == stmt_2 "
               "&& e_token_sequence_position_start == "
               "e_token_sequence_position_end) == false */ \n");
#endif
#if DEBUG_TOKEN_STREAM_UNPARSING
      printf("(stmt_1 == stmt_2 && e_token_sequence_position_start == "
             "e_token_sequence_position_end) == false \n");
#endif
      switch (e_token_sequence_position_end) {
        // DQ (12/28/2014): We need to accound for where the leading and
        // trailing token streams might not be available.
      case e_leading_whitespace_start:

        // DQ (6/6/2021): We need the offset so that when the next statement is
        // unparsed, we can always unparse the the full leading whitespace. end
        // = tokenSubsequence_2->leading_whitespace_start;
        end = tokenSubsequence_2->leading_whitespace_start + end_offset;
        // DQ (6/2/2021): Avoid adjustments to the token bounds.
        // DQ (12/28/2014): Note that white space is not always available.
        if (end == -1) {
#if DEBUG_USING_CURPRINT
          curprint("\n/* In unparseStatementFromTokenStream(): reset end to "
                   "tokenSubsequence_2->token_subsequence_start */ \n");
#endif
          // DQ (6/6/2021): If there is no whitespace, then we don't use the
          // offset. end = tokenSubsequence_2->token_subsequence_start; end =
          // tokenSubsequence_2->token_subsequence_start - 1; end =
          // (tokenSubsequence_2->token_subsequence_start - 1) + end_offset;
          end = tokenSubsequence_2->token_subsequence_start - 1;
        }
        ROSE_ASSERT(end >= 0);

        // DQ (12/26/2018): We have to make sure that we stay in bounds of the
        // number of tokens in the loken list.
        ROSE_ASSERT(end < tokenVectorSize);
        break;

      case e_leading_whitespace_end:
        end = tokenSubsequence_2->leading_whitespace_end;
        // DQ (6/6/2021): We want to handle the following:
        // unparseStatementFromTokenStream (test_stmt, body,
        // e_trailing_whitespace_start, e_leading_whitespace_end, info); DQ
        // (6/2/2021): Avoid adjustments to the token bounds. DQ (12/30/2014):
        // Note that white space is not always available.
        if (end == -1) {
#if DEBUG_USING_CURPRINT
          curprint("\n/* In unparseStatementFromTokenStream(): reset end to "
                   "tokenSubsequence_2->token_subsequence_start */ \n");
#endif
          // end = tokenSubsequence_2->token_subsequence_start;
          // end = tokenSubsequence_2->token_subsequence_start - 1;
          if (tokenSubsequence_2->token_subsequence_start > 0) {
            end = tokenSubsequence_2->token_subsequence_start - 1;
          } else {
            end = tokenSubsequence_2->token_subsequence_start;
          }
        }
        ROSE_ASSERT(end >= 0);
        // DQ (12/26/2018): We have to make sure that we stay in bounds of the
        // number of tokens in the loken list.
        ROSE_ASSERT(end < tokenVectorSize);
        break;

      case e_token_subsequence_start:
        end = tokenSubsequence_2->token_subsequence_start;
        ROSE_ASSERT(end >= 0);

        // DQ (12/26/2018): We have to make sure that we stay in bounds of the
        // number of tokens in the loken list.
        ROSE_ASSERT(end < tokenVectorSize);
        break;

      case e_token_subsequence_end:
        end = tokenSubsequence_2->token_subsequence_end;
        ROSE_ASSERT(end >= 0);

        // DQ (12/26/2018): We have to make sure that we stay in bounds of the
        // number of tokens in the loken list.
        ROSE_ASSERT(end < tokenVectorSize);
        break;

      case e_trailing_whitespace_start:
        end = tokenSubsequence_2->trailing_whitespace_start;

        // DQ (12/26/2018): We have to make sure that we stay in bounds of the
        // number of tokens in the loken list.
        ROSE_ASSERT(end < tokenVectorSize);
        break;

      case e_trailing_whitespace_end:
        end = tokenSubsequence_2->trailing_whitespace_end;

        // DQ (12/26/2018): We have to make sure that we stay in bounds of the
        // number of tokens in the loken list.
        ROSE_ASSERT(end < tokenVectorSize);
        break;
      default: {
        printf("Default reached in unparseStatementFromTokenStream(): "
               "e_token_sequence_position_end = %d \n",
               e_token_sequence_position_end);
        ROSE_ABORT();
      }
      }
    }

#if DEBUG_TOKEN_STREAM_UNPARSING || 0
    printf("unparseStatementFromTokenStream(): Iterate from start = %d to end "
           "= %d \n",
           start, end);
#endif
#if DEBUG_USING_CURPRINT
    curprint(string("\n/* In unparseStatementFromTokenStream(stmt_1,stmt_2): "
                    "Iterate from start = ") +
             StringUtility::numberToString(start) +
             " to end = " + StringUtility::numberToString(end) + " */ \n");
#endif

    // DQ (6/2/2021): Avoid adjustments to the token bounds (the value of -1 is
    // an acceptable value and it means that there are no associated tokens).
    // ROSE_ASSERT(start >= 0);

    // DQ (12/26/2018): This is now declared in the function body scope.
    // SgTokenPtrList & tokenVector = sourceFile->get_token_list();
    // int tokenVectorSize = tokenVector.size();

    // DQ (11/13/2018): This assertion will fail where a class declaration and
    // the end of the file has no white-space (see test9 of UnparseHeaders_tests
    // demo_error_Simple.h for an example).  This will have to be fixed later.
    // DQ (11/12/2018): Try to comment out this assertion (leave the output
    // message while we evaluate this). DQ (1/10/2015): I think we can assert
    // this now that we no longer call this function to output the trailing
    // whitespace of the SgGlobalScope.
    if (!(start < tokenVectorSize && end <= tokenVectorSize)) {
      printf("Error: tokenVectorSize = %d start = %d end = %d \n",
             tokenVectorSize, start, end);
    }
    ROSE_ASSERT(start < tokenVectorSize && end <= tokenVectorSize);

    // DQ (6/2/2021): Avoid adjustments to the token bounds (if they are set to
    // -1, then in means that there are no associated tokens). DQ (12/27/2018):
    // It appears that we allow the "end" to be equal to "tokenVectorSize".
    // Could this be an error? if (start < tokenVectorSize && end <=
    // tokenVectorSize) if ( ((start != -1) && (end != -1)) && (start <
    // tokenVectorSize && end <= tokenVectorSize))
    if (((start >= 0) && (end >= 0)) &&
        (start < tokenVectorSize && end <= tokenVectorSize)) {
      // We don't want to unparse the token at the end.
      // DQ (11/4/2015): Adding support to optionally only unparse the
      // associated whitespace with any region of a statement. This is used when
      // we want to unparse the leading whitespace of a statement as part of a
      // transformation, yet we need to ONLY unparse the spaces and CR's.
      if (unparseOnlyWhitespace == true) {
        // If this is whitespace with embedded comments (which we consider to be
        // in the leading a trailing whitespace for each statement), then we
        // only want to use the non-whitespace that is at the end of the leading
        // whitespace for the statement.

        SgTokenPtrList whitespaceTokens;
#if DEBUG_USING_CURPRINT
        curprint("/* (unparseOnlyWhitespace == true): */ \n");
#endif
        // We don't want to unparse the token at the end.
        int j = end - 1;

        bool firstCarriageReturn = false;
        bool still_is_whitespace = true;
        while ((j >= start) && (still_is_whitespace == true)) {
          // DQ (1/10/2014): Make sure that we don't use data that is
          // unavailable.
          ROSE_ASSERT(j < (int)tokenVector.size());

#if DEBUG_TOKEN_STREAM_UNPARSING
          printf("possible whitespace: start = %d j = %d \n", start, j);
#endif
#if DEBUG_TOKEN_STREAM_UNPARSING
          printf(
              "iterate j=end-1 to j >= start: unparseStatementFromTokenStream: "
              "Output tokenVector[j=%d]->get_lexeme_string() = %s \n",
              j, tokenVector[j]->get_lexeme_string().c_str());
#endif
          if (tokenVector[j]->get_classification_code() ==
              ROSE_token_ids::C_CXX_WHITESPACE) {
            // outputString << tokenVector[j]->get_lexeme_string();
            whitespaceTokens.push_back(tokenVector[j]);
#if DEBUG_USING_CURPRINT
            curprint(string("\n/* In unparseStatementFromTokenStream(): "
                            "whitespaceTokens = ") +
                     tokenVector[j]->get_lexeme_string() + " */ \n");
#endif
          } else {
            still_is_whitespace = false;
          }

          // DQ (11/20/2015): Note that to avoid extra lines in the output we
          // only want to the whitespace up to the first CR.
          firstCarriageReturn = tokenVector[j]->isCarriageReturn();
          if (firstCarriageReturn == true) {
            still_is_whitespace = false;
          }

          j--;
        }

        // Output the whitespace tokens in the reverse order.
#if DEBUG_TOKEN_STREAM_UNPARSING
        printf("whitespaceTokens.size() = %zu \n", whitespaceTokens.size());
#endif
        SgTokenPtrList::reverse_iterator m = whitespaceTokens.rbegin();
        while (m != whitespaceTokens.rend()) {
          // Print token (whitespace).
#if HIGH_FEDELITY_TOKEN_UNPARSING
          *(unp->get_output_stream().output_stream())
              << (*m)->get_lexeme_string();
#else
          // Note that this will interprete line endings which is not going to
          // provide the precise token based output.
          curprint((*m)->get_lexeme_string());
#endif
          m++;
        }
      } else {
#if DEBUG_USING_CURPRINT
        curprint("/* (unparseOnlyWhitespace == false): */ \n");
#endif
        // DQ (6/2/2021): We do need to unparse the end since it is where the
        // ";" is located in a class declaration (see test_153.cpp in code
        // segregation tests). DQ (12/27/2018): Now that we enforce uniformally
        // that the end is in bounds of the token vector, we DO want to unparse
        // the end. It seems that we can't handle this issue this way. We don't
        // want to unparse the token at the end. for (int j = start; j < end;
        // j++) for (int j = start; j < end; j++)
        for (int j = start; j <= end; j++) {
          // DQ (1/10/2014): Make sure that we don't use data that is
          // unavailable.
          ROSE_ASSERT(j < (int)tokenVector.size());

#if DEBUG_USING_CURPRINT && DEBUG_TOKEN_STREAM_UNPARSING
          curprint(string("\n/* In unparseStatementFromTokenStream(): "
                          "non-whitespaceTokens = ") +
                   tokenVector[j]->get_lexeme_string() + " */ \n");
#endif
#if DEBUG_TOKEN_STREAM_UNPARSING
          printf("iterate j=start to j < end: unparseStatementFromTokenStream: "
                 "Output tokenVector[j=%d]->get_lexeme_string() = %s \n",
                 j, tokenVector[j]->get_lexeme_string().c_str());
#endif
#if HIGH_FEDELITY_TOKEN_UNPARSING
          *(unp->get_output_stream().output_stream())
              << tokenVector[j]->get_lexeme_string();
#else
          // Note that this will interprete line endings which is not going to
          // provide the precise token based output.
          curprint(tokenVector[j]->get_lexeme_string());
#endif
        }
      }
    } else {
      // Start and/or end are out of range, acceptable when there are no
      // assocviated tokens to unparse.
#if DEBUG_USING_CURPRINT
      curprint("\n/* Start and/or end are out of range, which is acceptable "
               "when there are no associated tokens to unparse */ \n");
#endif
    }
  } else {
    // DQ (12/30/2014): This will likely cause an error since some subsequence
    // of the token stream will not be unparsed.
#if DEBUG_USING_CURPRINT
    curprint("\n/* ERROR: unparseStatementFromTokenStream(): This will likely "
             "cause an error since some subsequence of the token stream will "
             "not be unparsed */ \n");
#endif
  }

#if DEBUG_USING_CURPRINT
  curprint(
      "\n/* Leaving "
      "unparseStatementFromTokenStream(stmt,stmt,start,end,info,bool): */ \n");
#endif
}

void Unparse_ExprStmt::unparseFunctionParameterDeclaration(
    SgFunctionDeclaration *funcdecl_stmt, SgInitializedName *initializedName,
    bool outputParameterDeclaration, SgUnparse_Info &info) {
  ASSERT_not_null(funcdecl_stmt);

  ASSERT_not_null(initializedName);
  SgName tmp_name = initializedName->get_name();
  SgInitializer *tmp_init = initializedName->get_initializer();
  SgType *tmp_type = initializedName->get_type();

  // DQ (9/7/2014): These should have been setup to be the same.
  ROSE_ASSERT(info.SkipClassDefinition() == info.SkipEnumDefinition());

  // DQ (8/9/2013): refactored to support additional refactoring to seperate out
  // code to unparse SgInitializedName.
  bool oldStyleDefinition =
      funcdecl_stmt->get_oldStyleDefinition() && !funcdecl_stmt->isForward();

  // printf ("In unparseFunctionParameterDeclaration(): Argument name = %s \n",
  //      (tmp_name.str() != NULL) ? tmp_name.str() : "NULL NAME");

  // initializedName.get_storageModifier().display("New storage modifiers in
  // unparseFunctionParameterDeclaration()");

  SgStorageModifier &storage = initializedName->get_storageModifier();
  if (storage.isExtern()) {
    curprint("extern ");
  }

  // DQ (7/202/2006): The isStatic() function in the SgStorageModifier held by
  // the SgInitializedName object should always be false. This is because the
  // static-ness of a variable is held by the SgVariableDeclaration (and the
  // SgStorageModified help in the SgDeclarationModifier). printf ("In
  // initializedName = %p test the return value of storage.isStatic() = %d = %d
  // (should be boolean value)
  // \n",initializedName,storage.isStatic(),storage.get_modifier());
  ROSE_ASSERT(storage.isStatic() == false);

  // This was a bug mistakenly reported by Isaac
  ROSE_ASSERT(storage.get_modifier() >= 0);

  if (storage.isStatic()) {
    curprint("static ");
  }

  if (storage.isAuto()) {
    // DQ (4/30/2004): Auto is a default which is to be supressed
    // in C old-style parameters and not really ever needed anyway?
    // curprint( "auto ");
  }

  if (storage.isRegister()) {
    // curprint( "register ");
    if ((oldStyleDefinition == false) || (outputParameterDeclaration == true)) {
      curprint("register ");
    }
  }

  if (storage.isMutable()) {
    curprint("mutable ");
  }

  if (storage.isTypedef()) {
    curprint("typedef ");
  }

  if (storage.isAsm()) {
    // DQ (2/6/2014): Fix to support GNU gcc.
    // curprint("asm ");
    curprint("__asm__ ");
  }

  // TV (05/06/2010): CUDA storage modifiers
  if (storage.isCudaGlobal()) {
    curprint("__device__ ");
  }

  if (storage.isCudaConstant()) {
    curprint("__device__ __constant__ ");
  }

  if (storage.isCudaShared()) {
    curprint("__device__ __shared__ ");
  }

  if (storage.isCudaDynamicShared()) {
    curprint("extern __device__ __shared__ ");
  }

  // Error checking, if we are using old style C function parameters, then I
  // hope this is not C++ code!
  if (oldStyleDefinition == true) {
    if (SageInterface::is_Cxx_language() == true) {
      printf("Warning: Mixing old style C function parameters with C++ is "
             "maybe not well defined \n");
    }
    ROSE_ASSERT(SageInterface::is_Cxx_language() == false);
  }

  if ((oldStyleDefinition == false) || (outputParameterDeclaration == true)) {
    // output the type name for each argument
    if (tmp_type != NULL) {
      SgUnparse_Info ninfo(info);
      // DQ (2/3/2019): In the case of function parameters, the member pointer
      // types need an extra parenthesis. This might just apply to arrays of
      // SgMemberPointerType.
      SgPointerMemberType *pointerToMemberType =
          isSgPointerMemberType(tmp_type);
      if (pointerToMemberType != NULL) {
        ninfo.set_inArgList();
      }
      // Decide inline parameter-type definitions from the final named-type
      // declaration rather than trusting SgInitializedName::needs_definitions
      // for class/enum types. That flag can be copied through declaration
      // cloning long after the type declaration has been canonicalized, which
      // leads to ordinary namespace-scope enums/classes being emitted inline in
      // parameter lists.
      SgType *inline_check_type =
          stripFunctionReturnTypeForInlineDefinitionCheck(tmp_type);
      bool needs_inline_type_definition = false;
      if (isSgClassType(inline_check_type) != nullptr ||
          isSgEnumType(inline_check_type) != nullptr) {
        needs_inline_type_definition =
            parameterTypeNeedsInlineDefinition(tmp_type);
      } else {
        needs_inline_type_definition = initializedName->get_needs_definitions();
      }
      if (needs_inline_type_definition) {
        if (ninfo.SkipClassDefinition()) {
          ninfo.unset_SkipClassDefinition();
        }
        if (ninfo.SkipEnumDefinition()) {
          ninfo.unset_SkipEnumDefinition();
        }
      }
      // DQ (5/5/2013): Refactored code used here and in the
      // unparseTemplateArgument().
      unp->u_type->outputType<SgInitializedName>(initializedName, tmp_type,
                                                 ninfo);

      // DQ (2/3/2019): In the case of function parameters, the member pointer
      // types need an extra parenthesis.
      if (pointerToMemberType != NULL) {
        ninfo.unset_inArgList();
      }
    } else {
      curprint(tmp_name.str()); // for ... case
    }
  } else {
    curprint(tmp_name.str()); // for ... case
  }

  SgUnparse_Info ninfo3(info);
  ninfo3.unset_inArgList();

  // DQ (4/27/2013): We now have better support in ROSE to know when to output
  // the default arguments, so we don't want to use this mechanism above.  So
  // now we always output the default arguments for function parameters in a
  // function declaration if they are defined in the AST. It is up to the
  // specification in the AST to have them in the correct locations, consistant
  // with the source code.
  bool outputInitializer = true;

  // Add an initializer if it exists
  if (outputInitializer == true && tmp_init != NULL) {
    // Cong (6/28/2011): When unparsing an initializer for a function parameter,
    // we should add a space before '='. Or else, foo(const int& = 1) will be
    // unparsed to foo(const int&=1) which contains an operator '&=", which is
    // incorrect.
    curprint(" = ");
    unp->u_exprStmt->unparseExpression(tmp_init, ninfo3);
  }

  // DQ (1/7/2014): Adding support for GNU specific noreturn attribute for
  // function parameters (only applies to parameters that are of function
  // pointer type).
  if (initializedName->isGnuAttributeNoReturn() == true) {
    curprint(" __attribute__((noreturn))");
  }
}

static bool
hasUnsafeParameterPreprocessingInfo(SgInitializedName *initializedName) {
  if (initializedName == nullptr) {
    return false;
  }

  AttachedPreprocessingInfoType *infos =
      initializedName->getAttachedPreprocessingInfo();
  if (infos == nullptr) {
    return false;
  }

  for (PreprocessingInfo *info : *infos) {
    if (info == nullptr) {
      continue;
    }

    switch (info->getTypeOfDirective()) {
    case PreprocessingInfo::C_StyleComment:
    case PreprocessingInfo::CplusplusStyleComment:
    case PreprocessingInfo::FortranStyleComment:
    case PreprocessingInfo::F90StyleComment:
    case PreprocessingInfo::CpreprocessorBlankLine:
      break;

    default:
      return true;
    }
  }

  return false;
}

void Unparse_ExprStmt::unparseFunctionArgs(SgFunctionDeclaration *funcdecl_stmt,
                                           SgUnparse_Info &info) {
  ASSERT_not_null(funcdecl_stmt);

  // DQ (9/7/2014): These should have been setup to be the same.
  ROSE_ASSERT(info.SkipClassDefinition() == info.SkipEnumDefinition());

  // DQ (1/18/2014): This is a better implementation than setting the source
  // position info on the function parameters.  See test2014_35.c for an example
  // that requires this solution using a new data member.
  if (funcdecl_stmt->get_prototypeIsWithoutParameters() == true) {
    return;
  }

  bool outputFunctionParameters = true;

  SgInitializedNamePtrList::iterator p = funcdecl_stmt->get_args().begin();

  // DQ (4/13/2018): I want to initialize this iterator, but it is not clear
  // what to initialize it to...
  SgInitializedNamePtrList::iterator p_syntax =
      funcdecl_stmt->get_args().begin();
  bool use_param_syntax = false;
  // PP (9/19/25): * get_type_syntax_is_available() is not tied to
  // get_parameterList_syntax()
  //                 in some parse_secondary_declaration flows
  //                 get_type_syntax_is_available() only implies
  //                 get_type_syntax() and not get_parameterList_syntax().
  //               * For recording auto types in returns, only type_syntax is
  //               used,
  //                 but not parameterList_syntax.
  //               => check for get_parameterList_syntax() directly.
  if (SgFunctionParameterList *paramSyntax =
          funcdecl_stmt->get_parameterList_syntax()) {
    use_param_syntax = true;
    p_syntax = paramSyntax->get_args().begin();
  }

  while (p != funcdecl_stmt->get_args().end()) {
    // Liao 11/9/2010,
    // Skip duplicated unparsing of the attached information for C function
    // arguments declared in old style. They usually should be unparsed when
    // unparsing the arguments which are outside of the parameter list are
    // outside of the parameter list See example code:
    // tests/nonsmoke/functional/CompileTests/C_tests/test2010_10.c
    if (funcdecl_stmt->get_oldStyleDefinition() == false &&
        !hasUnsafeParameterPreprocessingInfo(*p)) {
      unparseAttachedPreprocessingInfo(*p, info, PreprocessingInfo::before);
    }
    // DQ (1/17/2014): Adding support in C to output function prototypes without
    // function parameters. unparseFunctionParameterDeclaration
    // (funcdecl_stmt,*p,false,info); if (outputFunctionParameters == true)
    if ((outputFunctionParameters == true) ||
        (funcdecl_stmt->get_oldStyleDefinition() == true)) {
      // DQ (4/13/2018): If we have saved the original syntax then use it, else
      // use the default (which is matching the defining function declaration).
      // unparseFunctionParameterDeclaration (funcdecl_stmt,*p,false,info);
      if (use_param_syntax) {
        // DQ (4/13/2018): One question would be are we using the correct name
        // qualification for any type referenced.
        unparseFunctionParameterDeclaration(funcdecl_stmt, *p_syntax, false,
                                            info);

      } else {
        unparseFunctionParameterDeclaration(funcdecl_stmt, *p, false, info);
      }
    }

    // Move to the next argument
    p++;

    // DQ (4/13/2018): Increment the type syntax iterator in unison.
    if (use_param_syntax) {
      p_syntax++;
    }

    // Check if this is the last argument (output a "," separator if not)
    if (p != funcdecl_stmt->get_args().end()) {
      curprint(",");
    }
  }
}

#define DEBUG_unparse_helper 0

//!  prints out the function parameters in a function declaration or function
//  call. For now, all parameters are printed on one line since there is no
//!  file information for each parameter.
void Unparse_ExprStmt::unparse_helper(SgFunctionDeclaration *funcdecl_stmt,
                                      SgUnparse_Info &info) {
  ASSERT_not_null(funcdecl_stmt);
#if DEBUG_unparse_helper
  printf("In unparse_helper():\n");
  printf("  funcdecl_stmt = %p = %s \n", funcdecl_stmt,
         funcdecl_stmt->class_name().c_str());
  printf("    ->get_name() = %s\n", funcdecl_stmt->get_name().str());
  printf("    ->get_firstNondefiningDeclaration() = %p \n",
         funcdecl_stmt->get_firstNondefiningDeclaration());
  printf("    ->get_definingDeclaration()         = %p \n",
         funcdecl_stmt->get_definingDeclaration());
  printf("    ->decl_mod.isFriend() = %s\n",
         funcdecl_stmt->get_declarationModifier().isFriend() ? "true"
                                                             : "false");
#endif
  bool is_friend = funcdecl_stmt->get_declarationModifier().isFriend();
  bool is_1st_decl =
      funcdecl_stmt == funcdecl_stmt->get_firstNondefiningDeclaration();
  bool has_qualifier = funcdecl_stmt->get_name_qualification_length() > 0 ||
                       funcdecl_stmt->get_global_qualification_required();

  bool force_global_friend_qualifier = false;
  if (is_friend) {
    SgClassDefinition *friend_class_def =
        isSgClassDefinition(funcdecl_stmt->get_parent());
    if (friend_class_def == nullptr &&
        isSgFunctionDeclaration(
            funcdecl_stmt->get_firstNondefiningDeclaration()) != nullptr) {
      friend_class_def = isSgClassDefinition(
          funcdecl_stmt->get_firstNondefiningDeclaration()->get_parent());
    }
    if (friend_class_def == nullptr &&
        isSgFunctionDeclaration(funcdecl_stmt->get_definingDeclaration()) !=
            nullptr) {
      friend_class_def = isSgClassDefinition(
          funcdecl_stmt->get_definingDeclaration()->get_parent());
    }

    if (friend_class_def != nullptr) {
      SgScopeStatement *friend_enclosing_scope =
          SageInterface::enclosingNamespaceScope(
              friend_class_def->get_declaration());
      if (friend_enclosing_scope == nullptr) {
        friend_enclosing_scope = friend_class_def->get_scope();
      }
      SgScopeStatement *friend_decl_scope = funcdecl_stmt->get_scope();
      force_global_friend_qualifier =
          friend_decl_scope != nullptr && friend_enclosing_scope != nullptr &&
          isSgGlobal(friend_decl_scope) != nullptr &&
          isSgGlobal(friend_enclosing_scope) == nullptr;
    }
  }

  bool need_qualifier = !(is_friend && is_1st_decl) || has_qualifier ||
                        force_global_friend_qualifier;
  if (need_qualifier) {
    std::string nameQualifier;
    if (SgTemplateMemberFunctionDeclaration *template_member_decl =
            isSgTemplateMemberFunctionDeclaration(funcdecl_stmt)) {
      if (isNormalizedClassTemplateMemberFunction(template_member_decl) &&
          template_member_decl->get_parent() !=
              template_member_decl->get_scope()) {
        nameQualifier = buildNormalizedTemplateMemberFunctionQualifier(
            template_member_decl);
      }
    }
    if (nameQualifier.empty()) {
      nameQualifier = funcdecl_stmt->get_qualified_name_prefix().str();
    }
    if (is_friend) {
      if (SgTemplateInstantiationFunctionDecl *friend_template_inst =
              isSgTemplateInstantiationFunctionDecl(funcdecl_stmt)) {
        if (SgTemplateFunctionDeclaration *primary_template =
                friend_template_inst->get_templateDeclaration()) {
          SgScopeStatement *template_scope = primary_template->get_scope();
          SgClassDefinition *friend_class_def =
              isSgClassDefinition(funcdecl_stmt->get_parent());
          if (friend_class_def != nullptr && template_scope != nullptr &&
              isSgGlobal(template_scope) != nullptr) {
            SgScopeStatement *lexical_namespace =
                SageInterface::enclosingNamespaceScope(
                    friend_class_def->get_declaration());
            if (lexical_namespace != nullptr &&
                isSgGlobal(lexical_namespace) == nullptr) {
              nameQualifier.clear();
              force_global_friend_qualifier = false;
            }
          }
        }
      }
    }
#if DEBUG_unparse_helper
    printf("  nameQualifier = %s\n", nameQualifier.c_str());
#endif
    if (force_global_friend_qualifier) {
      curprint("::");
    } else {
      curprint(nameQualifier);
    }
  }

  SgTemplateInstantiationFunctionDecl *tpl_fdecl =
      isSgTemplateInstantiationFunctionDecl(funcdecl_stmt);
  const bool parenthesize_function_name =
      tpl_fdecl == NULL &&
      shouldParenthesizeFunctionNameForMacro(funcdecl_stmt);
  if (parenthesize_function_name) {
    curprint("(");
  }
  if (tpl_fdecl != NULL) {
#if DEBUG_unparse_helper
    printf("  tpl_fdecl->get_templateName() = %s \n",
           tpl_fdecl->get_templateName().str());
#endif
    unp->u_exprStmt->unparseTemplateFunctionName(tpl_fdecl, info);
  } else {
    curprint(funcdecl_stmt->get_name().str());
  }
  if (parenthesize_function_name) {
    curprint(")");
  }

  ROSE_ASSERT(info.SkipClassDefinition() == info.SkipEnumDefinition());

  SgUnparse_Info ninfo2(info);
  ninfo2.set_inArgList();
  ninfo2.set_SkipClassDefinition();
  ninfo2.set_SkipEnumDefinition();
  ROSE_ASSERT(ninfo2.SkipClassDefinition() == ninfo2.SkipEnumDefinition());

  curprint("(");
  unparseFunctionArgs(funcdecl_stmt, ninfo2);
  curprint(")");

  if (funcdecl_stmt->get_oldStyleDefinition() && !funcdecl_stmt->isForward()) {
    // Output old-style C (K&R) parameter declarations only for definitions.
    // Forward declarations must stay in prototype form.
    SgInitializedNamePtrList::iterator p = funcdecl_stmt->get_args().begin();
    if (p != funcdecl_stmt->get_args().end()) {
      unp->u_sage->curprint_newline();
    }

    while (p != funcdecl_stmt->get_args().end()) {
      if (!hasUnsafeParameterPreprocessingInfo(*p)) {
        unparseAttachedPreprocessingInfo(*p, info, PreprocessingInfo::before);
      }
      unparseFunctionParameterDeclaration(funcdecl_stmt, *p, true, ninfo2);
      curprint(";");

      unp->u_sage->curprint_newline();

      p++;
    }
  }

#if DEBUG_unparse_helper
  printf("Leaving unparse_helper()\n");
#endif
}

void Unparse_ExprStmt::unparseLanguageSpecificStatement(SgStatement *stmt,
                                                        SgUnparse_Info &info) {
  // This function unparses the language specific parse not handled by the base
  // class unparseStatement() member function

  ASSERT_not_null(stmt);

  // curprint("In unparseLanguageSpecificStatement()");

#if DEBUG_USING_CURPRINT
  curprint(
      string(
          "\n/* Top of unparseLanguageSpecificStatement (Unparse_ExprStmt) ") +
      stmt->class_name() + " */\n");
#endif

#if DEBUG_USING_CURPRINT && 0
  ASSERT_not_null(stmt->get_startOfConstruct());
  // ASSERT_not_null(stmt->getAttachedPreprocessingInfo());
  int numberOfComments = -1;
  if (stmt->getAttachedPreprocessingInfo() != NULL)
    numberOfComments = stmt->getAttachedPreprocessingInfo()->size();
  curprint(string("/* startOfConstruct: file = ") +
           stmt->get_startOfConstruct()->get_filenameString() +
           " raw filename = " +
           stmt->get_startOfConstruct()->get_raw_filename() + " raw line = " +
           StringUtility::numberToString(
               stmt->get_startOfConstruct()->get_raw_line()) +
           " raw column = " +
           StringUtility::numberToString(
               stmt->get_startOfConstruct()->get_raw_col()) +
           " #comments = " + StringUtility::numberToString(numberOfComments) +
           " */\n");
#endif

  // DQ (1/9/2014): These should have been setup to be the same.
  ROSE_ASSERT(info.SkipClassDefinition() == info.SkipEnumDefinition());

#if ROSE_TRACK_PROGRESS_OF_ROSE_COMPILING_ROSE || 0
  printf("In unparseLanguageSpecificStatement(): file = %s line = %d \n",
         stmt->get_startOfConstruct()->get_filenameString().c_str(),
         stmt->get_startOfConstruct()->get_line());
#endif

  // DQ (12/16/2008): Added support for unparsing statements around C++ specific
  // statements unparseAttachedPreprocessingInfo(stmt, info,
  // PreprocessingInfo::before);

  // DQ (12/26/2007): Moved from language independent handling to C/C++ specific
  // handling because we don't want it to appear in the Fortran code generation.
  // DQ (added comments) this is where the new lines are introduced before
  // statements. unp->cur.format(stmt, info, FORMAT_BEFORE_STMT); if
  // (info.unparsedPartiallyUsingTokenStream() == false)
  bool saved_unparsedPartiallyUsingTokenStream =
      info.unparsedPartiallyUsingTokenStream();
  if (saved_unparsedPartiallyUsingTokenStream == false) {
    // DQ (11/14/2015): If we are unparsing statements in a SgBasicBlock, then
    // we want to know if the SgBasicBlock is being unparsed using the
    // partial_token_sequence so that we can supress the formatting that adds a
    // CR to the start of the current statement being unparsed.
    bool parentStatementListBeingUnparsedUsingPartialTokenSequence =
        info.parentStatementListBeingUnparsedUsingPartialTokenSequence();
    if (parentStatementListBeingUnparsedUsingPartialTokenSequence == true) {
      // ROSE_ASSERT(false);
    } else {
      unp->cur.format(stmt, info, FORMAT_BEFORE_STMT);
    }
  }

#if DEBUG_USING_CURPRINT
  curprint("/* In Unparse_ExprStmt::unparseLanguageSpecificStatement(): "
           "Selecting an unparse function */");
#endif

  switch (stmt->variantT()) {
    // DQ (8/14/2007): Need to move the C and C++ specific unparse member
    // functions from the base class to this function.

    // scope
    // case V_SgGlobal:                 unparseGlobalStmt(stmt, info); break;
    // case V_SgScopeStatement:         unparseScopeStmt(stmt, info); break;

    // pragmas
    // case V_SgPragmaDeclaration:      unparsePragmaDeclStmt(stmt, info);
    // break; scope case V_SgGlobal:                 unparseGlobalStmt(stmt,
    // info); break;
    //        case V_SgScopeStatement:         unparseScopeStmt (stmt, info);
    //        break;

    // program units
    // case V_SgModuleStatement:          unparseModuleStmt (stmt, info); break;
    // case V_SgProgramHeaderStatement:   unparseProgHdrStmt(stmt, info); break;
    // case V_SgProcedureHeaderStatement: unparseProcHdrStmt(stmt, info); break;

    // declarations
    // case V_SgInterfaceStatement:     unparseInterfaceStmt(stmt, info); break;
    // case V_SgCommonBlock:            unparseCommonBlock  (stmt, info); break;
  case V_SgVariableDeclaration:
    unparseVarDeclStmt(stmt, info);
    break;
  case V_SgVariableDefinition:
    unparseVarDefnStmt(stmt, info);
    break;
    // case V_SgParameterStatement:     unparseParamDeclStmt(stmt, info); break;
    // case V_SgUseStatement:           unparseUseStmt      (stmt, info); break;

    // executable statements, control flow
  case V_SgBasicBlock:
    unparseBasicBlockStmt(stmt, info);
    break;
  case V_SgIfStmt:
    unparseIfStmt(stmt, info);
    break;
    // case V_SgFortranDo:              unparseDoStmt         (stmt, info);
    // break;
  case V_SgWhileStmt:
    unparseWhileStmt(stmt, info);
    break;
  case V_SgSwitchStatement:
    unparseSwitchStmt(stmt, info);
    break;
  case V_SgCaseOptionStmt:
    unparseCaseStmt(stmt, info);
    break;
  case V_SgDefaultOptionStmt:
    unparseDefaultStmt(stmt, info);
    break;
  case V_SgBreakStmt:
    unparseBreakStmt(stmt, info);
    break;
  case V_SgLabelStatement:
    unparseLabelStmt(stmt, info);
    break;
  case V_SgGotoStatement:
    unparseGotoStmt(stmt, info);
    break;
    // case V_SgStopOrPauseStatement:   unparseStopOrPauseStmt(stmt, info);
    // break;
  case V_SgReturnStmt:
    unparseReturnStmt(stmt, info);
    break;

    // executable statements, IO
    // case V_SgIOStatement:            unparseIOStmt    (stmt, info); break;
    // case V_SgIOControlStatement:     unparseIOCtrlStmt(stmt, info); break;

    // pragmas
  case V_SgPragmaDeclaration:
    unparsePragmaDeclStmt(stmt, info);
    break;

    // DQ (3/22/2019): Adding EmptyDeclaration to support addition of comments
    // and CPP directives that will permit token-based unparsing to work with
    // greater precision. For example, used to add an include directive with
    // greater precision to the global scope and permit the unparsing via the
    // token stream to be used as well.
  case V_SgEmptyDeclaration:
    unparseEmptyDeclaration(stmt, info);
    break;

    // case DECL_STMT:          unparseDeclStmt(stmt, info);         break;
    // case SCOPE_STMT:         unparseScopeStmt(stmt, info);        break;
    //        case V_SgFunctionTypeTable:      unparseFuncTblStmt(stmt, info);
    //        break;
    // case GLOBAL_STMT:        unparseGlobalStmt(stmt, info);       break;
    // case V_SgBasicBlock:             unparseBasicBlockStmt(stmt, info);
    // break; case IF_STMT:            unparseIfStmt(stmt, info); break;

  case V_SgForStatement:
    unparseForStmt(stmt, info);
    break;

    // DQ (3/26/2018): Adding support for C++11 IR node (previously missed).
  case V_SgRangeBasedForStatement:
    unparseRangeBasedForStmt(stmt, info);
    break;

  case V_SgFunctionDeclaration:
    unparseFuncDeclStmt(stmt, info);
    break;
  case V_SgTemplateFunctionDefinition:
    unparseTemplateFunctionDefnStmt(stmt, info);
    break;
  case V_SgFunctionDefinition:
    unparseFuncDefnStmt(stmt, info);
    break;
  case V_SgMemberFunctionDeclaration:
    unparseMFuncDeclStmt(stmt, info);
    break;
    // case VAR_DECL_STMT:      unparseVarDeclStmt(stmt, info);      break;
    // case VAR_DEFN_STMT:      unparseVarDefnStmt(stmt, info);      break;
  case V_SgClassDeclaration:
    unparseClassDeclStmt(stmt, info);
    break;
  case V_SgClassDefinition:
    unparseClassDefnStmt(stmt, info);
    break;
  case V_SgEnumDeclaration:
    unparseEnumDeclStmt(stmt, info);
    break;
  case V_SgExprStatement:
    unparseExprStmt(stmt, info);
    break;
    // case LABEL_STMT:         unparseLabelStmt(stmt, info);        break;
    // case WHILE_STMT:         unparseWhileStmt(stmt, info);        break;
  case V_SgDoWhileStmt:
    unparseDoWhileStmt(stmt, info);
    break;
    // case SWITCH_STMT:        unparseSwitchStmt(stmt, info);       break;
    // case CASE_STMT:          unparseCaseStmt(stmt, info);         break;
  case V_SgTryStmt:
    unparseTryStmt(stmt, info);
    break;
  case V_SgCatchOptionStmt:
    unparseCatchStmt(stmt, info);
    break;
    // case DEFAULT_STMT:       unparseDefaultStmt(stmt, info);      break;
    // case BREAK_STMT:         unparseBreakStmt(stmt, info);        break;
  case V_SgContinueStmt:
    unparseContinueStmt(stmt, info);
    break;
    // case RETURN_STMT:        unparseReturnStmt(stmt, info);       break;
    // case GOTO_STMT:          unparseGotoStmt(stmt, info);         break;
  case V_SgAsmStmt:
    unparseAsmStmt(stmt, info);
    break;
    // case SPAWN_STMT:         unparseSpawnStmt(stmt, info);        break;
  case V_SgTypedefDeclaration:
    unparseTypeDefStmt(stmt, info);
    break;
  case V_SgTemplateDeclaration:
    unparseTemplateDeclStmt(stmt, info);
    break;

    // DQ (6/11/2011): Added support for new template IR nodes.
    // case V_SgTemplateClassDeclaration: unparseTemplateDeclStmt(stmt, info);
    // break; case V_SgTemplateFunctionDeclaration:
    // unparseTemplateDeclStmt(stmt, info); break; case
    // V_SgTemplateMemberFunctionDeclaration: unparseTemplateDeclStmt(stmt,
    // info); break; case V_SgTemplateVariableDeclaration:
    // unparseTemplateDeclStmt(stmt, info); break;

    // DQ (12/26/2011): New design for template declarations (no longer derived
    // from StTemplateDeclaration).
  case V_SgTemplateClassDeclaration:
    unparseTemplateClassDeclStmt(stmt, info);
    break;
  case V_SgTemplateClassDefinition:
    unparseTemplateClassDefnStmt(stmt, info);
    break;
  case V_SgTemplateFunctionDeclaration:
    unparseTemplateFunctionDeclStmt(stmt, info);
    break;
  case V_SgTemplateMemberFunctionDeclaration:
    unparseTemplateMemberFunctionDeclStmt(stmt, info);
    break;
  case V_SgTemplateVariableDeclaration:
    unparseTemplateVariableDeclStmt(stmt, info);
    break;

  case V_SgTemplateInstantiationDecl:
    unparseTemplateInstantiationDeclStmt(stmt, info);
    break;
  case V_SgTemplateInstantiationFunctionDecl:
    unparseTemplateInstantiationFunctionDeclStmt(stmt, info);
    break;
  case V_SgTemplateInstantiationMemberFunctionDecl:
    unparseTemplateInstantiationMemberFunctionDeclStmt(stmt, info);
    break;
  case V_SgTemplateInstantiationDirectiveStatement:
    unparseTemplateInstantiationDirectiveStmt(stmt, info);
    break;

    // Template instantiation typedefs can be compiler-generated (e.g., alias
    // template instantiations) and may not correspond to a valid source-level
    // typedef declaration. Skip those; otherwise fall back to typedef unparse.
  case V_SgTemplateInstantiationTypedefDeclaration: {
    SgTemplateInstantiationTypedefDeclaration *inst =
        isSgTemplateInstantiationTypedefDeclaration(stmt);
    ASSERT_not_null(inst);
    const Sg_File_Info *file_info = inst->get_file_info();
    if (file_info != nullptr && file_info->isCompilerGenerated()) {
      break;
    }
    unparseTypeDefStmt(inst, info);
    break;
  }

  case V_SgForInitStatement:
    unparseForInitStmt(stmt, info);
    break;

    // Comments could be attached to these statements
  case V_SgCatchStatementSeq:     // CATCH_STATEMENT_SEQ:
  case V_SgFunctionParameterList: // FUNCTION_PARAMETER_LIST:
  case V_SgCtorInitializerList:   // CTOR_INITIALIZER_LIST:
#if PRINT_DEVELOPER_WARNINGS
    printf("Ignore these newly implemented cases (case of %s) \n",
           stmt->sage_class_name());
    printf("WARNING: These cases must be implemented so that comments attached "
           "to them can be processed \n");
#endif
    // ROSE_ABORT();
    break;

  case V_SgNamespaceDeclarationStatement:
    unparseNamespaceDeclarationStatement(stmt, info);
    break;
  case V_SgNamespaceDefinitionStatement:
    unparseNamespaceDefinitionStatement(stmt, info);
    break;
  case V_SgNamespaceAliasDeclarationStatement:
    unparseNamespaceAliasDeclarationStatement(stmt, info);
    break;
  case V_SgUsingDirectiveStatement:
    unparseUsingDirectiveStatement(stmt, info);
    break;
  case V_SgUsingDeclarationStatement:
    unparseUsingDeclarationStatement(stmt, info);
    break;

    // DQ (3/2/2005): Added support for unparsing template class definitions.
    // This is the case: TEMPLATE_INST_DEFN_STMT
  case V_SgTemplateInstantiationDefn:
    unparseClassDefnStmt(stmt, info);
    break;

    // case V_SgNullStatement:                      unparseNullStatement(stmt,
    // info); break;

    // Liao, 5/31/2009, add OpenMP support, TODO refactor some code to language
    // independent part
  case V_SgOmpForStatement:
    unparseOmpForStatement(stmt, info);
    break;
  case V_SgOmpForSimdStatement:
    unparseOmpForSimdStatement(stmt, info);
    break;

    // DQ (7/25/2014): Adding support for C11 static assertions.
  case V_SgStaticAssertionDeclaration:
    unparseStaticAssertionDeclaration(stmt, info);
    break;

    // DQ 11/3/2014): Adding C++11 templated typedef declaration support.
  case V_SgTemplateTypedefDeclaration:
    unparseTemplateTypedefDeclaration(stmt, info);
    break;

  case V_SgNonrealDecl:
    unparseNonrealDecl(stmt, info);
    break;

  case V_SgDeclarationScope:
    // Nonreal scope container for template parameters; no direct code output.
    break;

  default: {
    printf("CxxCodeGeneration_locatedNode::unparseLanguageSpecificStatement: "
           "Error: No handler for %s (variant: %d)\n",
           stmt->sage_class_name(), stmt->variantT());
    ROSE_ABORT();
  }
  }

  // DQ (12/16/2008): Added support for unparsing statements around C++ specific
  // statements unparseAttachedPreprocessingInfo(stmt, info,
  // PreprocessingInfo::after);

#if DEBUG_USING_CURPRINT
  curprint(
      string("\n/* Leaving of unparseLanguageSpecificStatement() stmt = ") +
      stmt->class_name() + " */ \n");
#endif

  // DQ (1/9/2014): These should have been setup to be the same.
  ROSE_ASSERT(info.SkipClassDefinition() == info.SkipEnumDefinition());

#if DEBUG_USING_CURPRINT
  curprint(
      string(
          "\n/* Leaving unparseLanguageSpecificStatement (Unparse_ExprStmt) ") +
      stmt->class_name() + " */\n");
#endif
}

void Unparse_ExprStmt::unparseNamespaceDeclarationStatement(
    SgStatement *stmt, SgUnparse_Info &info) {
  // There is a SgNamespaceDefinition, but it is not unparsed except through the
  // SgNamespaceDeclaration
  SgNamespaceDeclarationStatement *namespaceDeclaration =
      isSgNamespaceDeclarationStatement(stmt);
  ASSERT_not_null(namespaceDeclaration);

  // DQ (12/17/2014): Test for if we have unparsed partially using the token
  // stream. If so then we don't want to unparse this syntax, if not then we
  // require this syntax.
  bool saved_unparsedPartiallyUsingTokenStream =
      info.unparsedPartiallyUsingTokenStream();
  if (saved_unparsedPartiallyUsingTokenStream == false) {
    // DQ (8/12/2014): Adding support for inlined namespaces (C++11 support).
    if (namespaceDeclaration->get_isInlinedNamespace() == true) {
      curprint("inline ");
    }

    curprint("namespace ");

    if (!namespaceDeclaration->get_isUnnamedNamespace()) {
      SgName name = namespaceDeclaration->get_name();
      curprint(name.str());
    }
  } else {
    SgNamespaceDefinitionStatement *namespaceDefinition =
        namespaceDeclaration->get_definition();
    ASSERT_not_null(namespaceDefinition);

    if (namespaceDefinition != NULL) {
      // I think that we may need to tigger the output of the opening "{" here,
      // and then loop over the declarations in the class definition explicitly.
      unparseStatementFromTokenStream(stmt, namespaceDefinition,
                                      e_token_subsequence_start,
                                      e_token_subsequence_start, info);

      SgUnparse_Info ninfo(info);

      SgDeclarationStatementPtrList &statementList =
          namespaceDefinition->get_declarations();
      SgDeclarationStatementPtrList::iterator statementIterator =
          statementList.begin();
      while (statementIterator != statementList.end()) {
        unparseStatement(*statementIterator, ninfo);
        statementIterator++;
      }
      unparseStatementFromTokenStream(namespaceDefinition, stmt,
                                      e_token_subsequence_end,
                                      e_token_subsequence_end, info);
    } else {
      // We need to handle the case of a function prototype.
      printf("We need to handle the case of a namespace declaration prototype "
             "that has been modified in partial way (not clear that this is "
             "common) \n");
      printf("Exiting as a test! \n");
      ROSE_ABORT();
    }
  }

  if (saved_unparsedPartiallyUsingTokenStream == false) {
    // DQ (8/6/2012): test2010_24.C causes the new namespace alias support to
    // generate namespaceDeclaration->get_definition() == NULL. I don't know yet
    // if this is reasonable, so output a warning for now.
    // unparseStatement(namespaceDeclaration->get_definition(),info);
    if (namespaceDeclaration->get_definition() != NULL) {
      // DQ (8/19/2014): If we unparse the SgNamespaceDeclarationStatement, then
      // we mean to unparse the SgNamespaceDefinition as well. test2014_110.C
      // demonstrates where the SgNamespaceDefinition has the wrong source
      // position (from a header file) and thus is not unparsed (filterd by the
      // logic in unparseStatement()).  So try to call the correct unparse
      // function directly.
      // unparseStatement(namespaceDeclaration->get_definition(),info);
      unparseNamespaceDefinitionStatement(
          namespaceDeclaration->get_definition(), info);
    } else {
      printf("WARNING: I think we were expecting a definition associated with "
             "this SgNamespaceDeclarationStatement \n");
    }
  }

  // DQ (5/20/2013): I think this is the wrong assertion, see test2013_170.C.
  // Basically, line directives might make the logical file name so that this
  // declaration would not be output, but we want to output the declaration and
  // set isOutputInCodeGeneration() == false to support the output of the
  // declaration. ROSE_ASSERT(stmt->get_file_info()->isCompilerGenerated() ==
  // true || stmt->get_file_info()->isOutputInCodeGeneration() == false);
}

void Unparse_ExprStmt::unparseNamespaceDefinitionStatement(
    SgStatement *stmt, SgUnparse_Info &info) {
  ASSERT_not_null(stmt);
  SgNamespaceDefinitionStatement *namespaceDefinition =
      isSgNamespaceDefinitionStatement(stmt);
  ASSERT_not_null(namespaceDefinition);

#if OUTPUT_HIDDEN_LIST_DATA
  outputHiddenListData(namespaceDefinition);
#endif

  SgUnparse_Info ninfo(info);

  // DQ (11/6/2004): Added support for saving current namespace!
  ASSERT_not_null(namespaceDefinition->get_namespaceDeclaration());
  SgNamespaceDeclarationStatement *saved_namespace =
      ninfo.get_current_namespace();

  // DQ (6/13/2007): Set to null before resetting to non-null value
  ninfo.set_current_namespace(NULL);
  ninfo.set_current_namespace(namespaceDefinition->get_namespaceDeclaration());

  // DQ (12/17/2014): Test for if we have unparsed partially using the token
  // stream. If so then we don't want to unparse this syntax, if not then we
  // require this syntax.
  bool saved_unparsedPartiallyUsingTokenStream =
      info.unparsedPartiallyUsingTokenStream();
  if (saved_unparsedPartiallyUsingTokenStream == false) {
    unp->cur.format(namespaceDefinition, info, FORMAT_BEFORE_BASIC_BLOCK2);
    curprint("{");
    unp->cur.format(namespaceDefinition, info, FORMAT_AFTER_BASIC_BLOCK2);
  } else {
    unparseStatementFromTokenStream(stmt, e_token_subsequence_start,
                                    e_token_subsequence_start, info);
  }

  SgStatement *last_stmt = NULL;

  // unparse all the declarations
  SgDeclarationStatementPtrList &statementList =
      namespaceDefinition->get_declarations();
  SgDeclarationStatementPtrList::iterator statementIterator =
      statementList.begin();
  size_t extern_brace_depth = 0;
  bool extern_brace_active = ninfo.get_extern_C_with_braces();
  while (statementIterator != statementList.end()) {
    SgStatement *currentStatement = *statementIterator;
    ASSERT_not_null(currentStatement);

    // DQ (11/6/2004): use ninfo instead of info for nested declarations in
    // namespace
    unparseStatementWithExternBraceTracking(
        currentStatement, ninfo, extern_brace_depth, extern_brace_active);

    // DQ (12/18/2014): Save the last statement so that we can use the
    // trailing token stream if using the token-based unparsing.
    last_stmt = currentStatement;

    // Go to the next statement
    statementIterator++;
  }

  if (saved_unparsedPartiallyUsingTokenStream == false) {
    // DQ (3/17/2005): This helps handle cases such as void foo () { #include
    // "constant_code.h" }
    unparseAttachedPreprocessingInfo(namespaceDefinition, info,
                                     PreprocessingInfo::inside);

    unp->cur.format(namespaceDefinition, info, FORMAT_BEFORE_BASIC_BLOCK2);
    curprint("}\n");
    unp->cur.format(namespaceDefinition, info, FORMAT_AFTER_BASIC_BLOCK2);
  } else {

    // unparseStatementFromTokenStream (stmt, e_token_subsequence_end,
    // e_token_subsequence_end);
    if (last_stmt != NULL) {
      // Unparse the trailing white space of the last statement.
      unparseStatementFromTokenStream(last_stmt, stmt,
                                      e_trailing_whitespace_start,
                                      e_token_subsequence_end, info);
      // Unparse the final "}" for the SgNamespaceDefinitionStatement.
      unparseStatementFromTokenStream(stmt, e_token_subsequence_end,
                                      e_token_subsequence_end, info);
    } else {
      unparseStatementFromTokenStream(stmt, e_token_subsequence_end,
                                      e_token_subsequence_end, info);
    }
  }

  // DQ (11/3/2007): Since "ninfo" will go out of scope shortly, this is not
  // significant. DQ (6/13/2007): Set to null before resetting to non-null value
  // DQ (11/6/2004): Added support for saving current namespace!
  ninfo.set_current_namespace(NULL);
  ninfo.set_current_namespace(saved_namespace);
}

void Unparse_ExprStmt::unparseNamespaceAliasDeclarationStatement(
    SgStatement *stmt, SgUnparse_Info &info) {
  SgNamespaceAliasDeclarationStatement *namespaceAliasDeclaration =
      isSgNamespaceAliasDeclarationStatement(stmt);
  ASSERT_not_null(namespaceAliasDeclaration);

  curprint("\nnamespace ");
  curprint(namespaceAliasDeclaration->get_name().str());
  curprint(" = ");
  ASSERT_not_null(namespaceAliasDeclaration->get_namespaceDeclaration());

  // DQ (7/8/2014): Support for new name qualification.
  SgUnparse_Info tmp_info(info);
  tmp_info.set_name_qualification_length(
      namespaceAliasDeclaration->get_name_qualification_length());
  tmp_info.set_global_qualification_required(
      namespaceAliasDeclaration->get_global_qualification_required());

  // DQ (7/8/2014): We store the information about the name qualification in the
  // reference to the namespace, and not in the namespace.  This is so that
  // multiple references to the namespace can be supported using different
  // levels of qualification.
  SgName nameQualifier = namespaceAliasDeclaration->get_qualified_name_prefix();

  curprint(nameQualifier);

  // DQ (4/9/2018): Added support for aliases of namespace alias namespaces.
  // curprint (
  // namespaceAliasDeclaration->get_namespaceDeclaration()->get_name().str());
  if (namespaceAliasDeclaration->get_is_alias_for_another_namespace_alias() ==
      false) {
    curprint(namespaceAliasDeclaration->get_namespaceDeclaration()
                 ->get_name()
                 .str());
  } else {
    // DQ (4/9/2018): This is the case of an alis to a namespace alias (see
    // Cxx_tests/test2018_26.C).
    curprint(namespaceAliasDeclaration->get_namespaceAliasDeclaration()
                 ->get_name()
                 .str());
  }

  curprint(string(";\n"));
}

void Unparse_ExprStmt::unparseUsingDirectiveStatement(SgStatement *stmt,
                                                      SgUnparse_Info &info) {
  SgUsingDirectiveStatement *usingDirective = isSgUsingDirectiveStatement(stmt);
  ASSERT_not_null(usingDirective);
  ASSERT_not_null(usingDirective->get_namespaceDeclaration());

  // Anonymous namespaces are emitted as `namespace { ... }` and do not need
  // a synthetic `using namespace __anonymous_namespace_...;` in generated code.
  if (usingDirective->get_namespaceDeclaration()->get_isUnnamedNamespace()) {
    return;
  }

  // DQ (8/26/2004): This should be "using namespace" instead of just "using"
  curprint(string("\nusing namespace "));

  // DQ (5/12/2011): Support for new name qualification.
  SgUnparse_Info tmp_info(info);
  tmp_info.set_name_qualification_length(
      usingDirective->get_name_qualification_length());
  tmp_info.set_global_qualification_required(
      usingDirective->get_global_qualification_required());

  // DQ (6/7/2007): Compute the name qualification separately.
  // curprint ( usingDirective->get_namespaceDeclaration()->get_name().str();
  // curprint (
  // usingDirective->get_namespaceDeclaration()->get_qualified_name().str();

  // DQ (5/12/2011): We store the information about the name qualification in
  // the reference to the namespace, and not in the namespace.  This is so that
  // multiple references to the namespace can be supported using different
  // levels of qualification. SgName nameQualifier =
  // unp->u_name->generateNameQualifier(
  // usingDirective->get_namespaceDeclaration() , info ); SgName nameQualifier =
  // unp->u_name->generateNameQualifier(
  // usingDirective->get_namespaceDeclaration() , tmp_info );
  SgName nameQualifier = usingDirective->get_qualified_name_prefix();

  // printf ("In unparseUsingDirectiveStatement(): nameQualifier = %s
  // \n",nameQualifier.str());
  curprint(nameQualifier);

  curprint(usingDirective->get_namespaceDeclaration()->get_name().str());

  curprint(string(";\n"));
}

void Unparse_ExprStmt::unparseUsingDeclarationStatement(SgStatement *stmt,
                                                        SgUnparse_Info &info) {
  SgUsingDeclarationStatement *usingDeclaration =
      isSgUsingDeclarationStatement(stmt);
  ASSERT_not_null(usingDeclaration);

  // DQ (1/30/2019): This code is required for the output of the access
  // specifier (public, protected, private) and applies only within classes. Use
  // get_parent() instead of get_scope() since we are looking for the structural
  // position of the declaration (is it is a class).
  SgClassDefinition *classDefinition =
      isSgClassDefinition(usingDeclaration->get_parent());
  if (classDefinition != NULL) {
    // Don't output an access specifier in this is a struct or union!
    // printf ("Don't output an access specifier in this is a struct or union!
    // \n");

    // DQ and PC (6/1/2006): Added Peter's suggested fixes to support unparsing
    // fully qualified names (supporting auto-documentation). if
    // (classDefinition->get_declaration()->get_class_type() ==
    // SgClassDeclaration::e_class)
    if (classDefinition->get_declaration()->get_class_type() ==
            SgClassDeclaration::e_class &&
        !info.skipCheckAccess())
      info.set_CheckAccess();
    // inClass = true;
    // inCname =
    // isSgClassDefinition(vardecl_stmt->get_parent())->get_declaration()->get_name();
  }

  // DQ (1/30/2019): Adding support to output the access specifier when we are
  // in a class definition. info.set_CheckAccess();
  unp->u_sage->printSpecifier(usingDeclaration, info);
  info.unset_CheckAccess();

  curprint(string("\nusing "));

  // DQ (9/11/2004): We only save the declaration and get the name by unparsing
  // the declaration Might have to setup info1 to only output the name that we
  // want!
  SgUnparse_Info info1(info);
  // info1.unset_CheckAccess();
  // info1.set_PrintName();
  info1.unset_isWithType();

  // DQ (7/21/2005): Either one or the other of these are valid. A using
  // declaration can have either a reference to a declaration
  // (SgDeclarationStatement) or a variable or enum file name
  // (SgInitializedName).
  SgDeclarationStatement *declarationStatement =
      usingDeclaration->get_declaration();
  SgInitializedName *initializedName = usingDeclaration->get_initializedName();
  auto inheriting_ctor_terminal_name =
      [&](const std::string &raw_name) -> std::string {
    if (usingDeclaration->get_is_inheriting_constructor() != true) {
      return raw_name;
    }
    std::string trimmed_name = Rose::StringUtility::trim(raw_name);
    size_t template_args_pos = trimmed_name.find('<');
    if (template_args_pos != std::string::npos) {
      trimmed_name = trimmed_name.substr(0, template_args_pos);
    }
    return trimmed_name;
  };

  // Enforce that only one is a vaild pointer
  ROSE_ASSERT(declarationStatement != NULL || initializedName != NULL);
  ROSE_ASSERT(declarationStatement == NULL || initializedName == NULL);

  // printf ("In unparseUsingDeclarationStatement(): declarationStatement = %s
  // \n",declarationStatement->sage_class_name());
  // unparseStatement(declarationStatement,info1);

  // DQ (5/12/2011): Support for new name qualification.
  SgUnparse_Info tmp_info1(info);
  tmp_info1.set_name_qualification_length(
      usingDeclaration->get_name_qualification_length());
  tmp_info1.set_global_qualification_required(
      usingDeclaration->get_global_qualification_required());

  if (initializedName != NULL) {
    // DQ (5/12/2011): Support for new name qualification.
    // DQ (6/5/2011): This case is demonstrated by test2005_114.C.
    SgName nameQualifier = usingDeclaration->get_qualified_name_prefix();

    curprint(nameQualifier.str());
    curprint(inheriting_ctor_terminal_name(initializedName->get_name().str()));
  }

  if (declarationStatement != NULL) {
    SgName nameQualifier = usingDeclaration->get_qualified_name_prefix();
    curprint(nameQualifier.str());

    // Handle the different sorts of declarations explicitly since the existing
    // unparse functions for declarations are not setup for what the using
    // declaration unparser requires.
    switch (declarationStatement->variantT()) {
    case V_SgVariableDeclaration: {
      // DQ (7/21/2005): Now that we have added support for SgUsingDeclarations
      // to reference a SgDeclarationStatment or a SgInitializedName we could
      // have the SgVariableDeclaration be implemented to more precisely
      // reference the variable directly instead of the declaration where the
      // variable was defined.

      // get the name of the variable in the declaration
      SgVariableDeclaration *variableDeclaration =
          isSgVariableDeclaration(declarationStatement);
      ASSERT_not_null(variableDeclaration);
      SgInitializedNamePtrList &variableList =
          variableDeclaration->get_variables();
      // using directives must be issued separately for each variable!
      ROSE_ASSERT(variableList.size() == 1);
      SgInitializedName *initializedName = *(variableList.begin());
      unparseAttachedPreprocessingInfo(initializedName, info,
                                       PreprocessingInfo::before);
      ASSERT_not_null(initializedName);
      SgName variableName = initializedName->get_name();
      curprint(variableName.str());
      break;
    }

    case V_SgVariableDefinition: {
      // DQ (6/18/2006): Associated declaration can be a SgVariableDefinition,
      // get the name of the variable using the variable definition
      SgVariableDefinition *variableDefinition =
          isSgVariableDefinition(declarationStatement);
      ASSERT_not_null(variableDefinition);
      SgInitializedName *initializedName = variableDefinition->get_vardefn();
      ASSERT_not_null(initializedName);
      SgName variableName = initializedName->get_name();
      curprint(variableName.str());
      break;
    }

    case V_SgNamespaceDeclarationStatement: {
      SgNamespaceDeclarationStatement *namespaceDeclaration =
          isSgNamespaceDeclarationStatement(declarationStatement);
      ASSERT_not_null(namespaceDeclaration);
      SgName namespaceName = namespaceDeclaration->get_name();
      curprint(namespaceName.str());
      break;
    }

    case V_SgFunctionDeclaration: {
      SgFunctionDeclaration *functionDeclaration =
          isSgFunctionDeclaration(declarationStatement);
      ASSERT_not_null(functionDeclaration);
      SgName functionName = functionDeclaration->get_name();
      curprint(functionName.str());
      break;
    }

    case V_SgTemplateInstantiationMemberFunctionDecl:
    case V_SgMemberFunctionDeclaration: {
      SgMemberFunctionDeclaration *memberFunctionDeclaration =
          isSgMemberFunctionDeclaration(declarationStatement);
      ASSERT_not_null(memberFunctionDeclaration);
      SgName memberFunctionName = memberFunctionDeclaration->get_name();
      std::string using_name =
          inheriting_ctor_terminal_name(memberFunctionName.str());
      curprint(using_name);
      break;
    }

    case V_SgClassDeclaration: {
      SgClassDeclaration *classDeclaration =
          isSgClassDeclaration(declarationStatement);
      ASSERT_not_null(classDeclaration);
      SgName className = classDeclaration->get_name();
      curprint(inheriting_ctor_terminal_name(className.str()));
      break;
    }

    case V_SgTypedefDeclaration: {
      SgTypedefDeclaration *typedefDeclaration =
          isSgTypedefDeclaration(declarationStatement);
      ASSERT_not_null(typedefDeclaration);
      SgName typedefName = typedefDeclaration->get_name();
      curprint(typedefName.str());
      break;
    }

      // DQ (12/29/2011): Added more template support for declarations.
      // I don't know if this case has to be separated out from case
      // V_SgClassDeclaration
    case V_SgTemplateClassDeclaration: {
      SgTemplateClassDeclaration *templateDeclaration =
          isSgTemplateClassDeclaration(declarationStatement);
      ASSERT_not_null(templateDeclaration);
      SgName templateName = templateDeclaration->get_name();
      curprint(inheriting_ctor_terminal_name(templateName.str()));
      break;
    }

      // DQ (12/29/2011): Added more template support for declarations.
      // I don't know if these case have to be separated out from case
      // V_SgFunctionDeclaration
    case V_SgTemplateFunctionDeclaration: {
      SgTemplateFunctionDeclaration *templateDeclaration =
          isSgTemplateFunctionDeclaration(declarationStatement);
      if (templateDeclaration != NULL) {
        SgName templateName = templateDeclaration->get_name();
        std::string using_name =
            inheriting_ctor_terminal_name(templateName.str());
        curprint(using_name);
      }
      break;
    }

    case V_SgTemplateMemberFunctionDeclaration: {
      SgTemplateMemberFunctionDeclaration *templateDeclaration =
          isSgTemplateMemberFunctionDeclaration(declarationStatement);
      if (templateDeclaration != NULL) {
        SgName templateName = templateDeclaration->get_name();
        std::string using_name =
            inheriting_ctor_terminal_name(templateName.str());
        curprint(using_name);
      }
      break;
    }

      // DQ (6/11/2011): Added support for new template IR nodes.
    case V_SgTemplateDeclaration: {
      // DQ (9/12/2004): This function outputs the default template name which
      // is not correct, we need to get more information out of the frontend
      // about the template name if we are to get this correct in the future.
      // This could (and likely will) cause generated code to not compile, but I
      // will worry about that after we can compile Kull.
      SgTemplateDeclaration *templateDeclaration =
          isSgTemplateDeclaration(declarationStatement);
      ASSERT_not_null(templateDeclaration);
      SgName templateName = templateDeclaration->get_name();
      curprint(templateName.str());
      break;
    }

      // DQ (5/22/2007): Added support for enum types in using declaration
      // (test2007_50.C).
    case V_SgEnumDeclaration: {
      SgEnumDeclaration *enumDeclaration =
          isSgEnumDeclaration(declarationStatement);
      ASSERT_not_null(enumDeclaration);
      SgName enumName = enumDeclaration->get_name();
      curprint(enumName.str());
      break;
    }

      // DQ (3/8/2017): Added support for SgTemplateTypedefDeclaration IR nodes
      // in using declaration (Cxx11_tests/test20017_03.C).
    case V_SgTemplateTypedefDeclaration: {
      SgTemplateTypedefDeclaration *templateTypedefDeclaration =
          isSgTemplateTypedefDeclaration(declarationStatement);
      ASSERT_not_null(templateTypedefDeclaration);
      SgName name = templateTypedefDeclaration->get_name();
      curprint(name.str());
      break;
    }

    default: {
      printf("Default reached in unparseUsingDeclarationStatement(): case is "
             "not implemented for %s \n",
             declarationStatement->sage_class_name());
      ROSE_ABORT();
    }
    }
  }

  curprint(string(";\n"));
}

void Unparse_ExprStmt::unparseTemplateInstantiationDirectiveStmt(
    SgStatement *stmt, SgUnparse_Info &info) {
  // DQ (4/16/2005): Added support for explicit template instatination
  // directives
  SgTemplateInstantiationDirectiveStatement *templateInstantiationDirective =
      isSgTemplateInstantiationDirectiveStatement(stmt);
  ASSERT_not_null(templateInstantiationDirective);

  SgDeclarationStatement *declarationStatement =
      templateInstantiationDirective->get_declaration();
  ASSERT_not_null(declarationStatement);

  // curprint ( string("template ";

  // DQ (8/2/2014): Added support for C++ directive to surpress template
  // instantiation.
  if (templateInstantiationDirective->get_do_not_instantiate() == true) {
    // syntax for C++11 "do not instantiate" directive.
    curprint("extern ");
  }

  ASSERT_not_null(declarationStatement->get_file_info());
  // declarationStatement->get_file_info()->display("Location of
  // SgTemplateInstantiationDirectiveStatement \n");

  // unparseStatement(declaration,info);
  switch (declarationStatement->variantT()) {
  case V_SgTemplateInstantiationDecl: {
    SgClassDeclaration *classDeclaration =
        isSgClassDeclaration(declarationStatement);
    ASSERT_not_null(classDeclaration);

    unparseClassDeclStmt(classDeclaration, info);
    break;
  }

  case V_SgTemplateInstantiationFunctionDecl: {
    // printf ("Unparsing of SgTemplateInstantiationFunctionDecl in
    // unparseTemplateInstantiationDirectiveStmt ... \n"); ROSE_ASSERT(false);
    SgFunctionDeclaration *functionDeclaration =
        isSgFunctionDeclaration(declarationStatement);
    ASSERT_not_null(functionDeclaration);
    // DQ (8/29/2005): "template" keyword now output by
    // Unparse_ExprStmt::outputTemplateSpecializationSpecifier() curprint (
    // string("template ";
    SgUnparse_Info ninfo(info);
    ninfo.set_SkipFunctionDefinition();
    if (functionDeclaration->isForward() == false) {
      ninfo.set_AddSemiColonAfterDeclaration();
    }
    unparseFuncDeclStmt(functionDeclaration, ninfo);
    break;
  }

  case V_SgTemplateInstantiationMemberFunctionDecl: {
    SgMemberFunctionDeclaration *memberFunctionDeclaration =
        isSgMemberFunctionDeclaration(declarationStatement);
    ASSERT_not_null(memberFunctionDeclaration);

    SgTemplateInstantiationMemberFunctionDecl *template_member_inst =
        isSgTemplateInstantiationMemberFunctionDecl(memberFunctionDeclaration);
    ASSERT_not_null(template_member_inst);

    const Sg_File_Info *member_file_info =
        template_member_inst->get_file_info();
    const Sg_File_Info *directive_file_info =
        templateInstantiationDirective->get_file_info();
    const bool directive_wraps_original_member_declaration =
        template_member_inst->isSpecialization() == false &&
        member_file_info != NULL && directive_file_info != NULL &&
        member_file_info->isSourcePositionUnavailableInFrontend() == false &&
        (member_file_info->get_filenameString() !=
             directive_file_info->get_filenameString() ||
         member_file_info->get_line() != directive_file_info->get_line() ||
         member_file_info->get_col() != directive_file_info->get_col());
    if (directive_wraps_original_member_declaration) {
      return;
    }

    // Explicit instantiation directives remain directives regardless of
    // whether the instantiated declaration is itself still a template at the
    // ROSE level.  Emit the member declaration unconditionally and let the
    // directive parent drive the leading `template` / `extern template`
    // syntax.
    SgUnparse_Info ninfo(info);
    ninfo.set_SkipFunctionDefinition();
    if (memberFunctionDeclaration->isForward() == false) {
      ninfo.set_AddSemiColonAfterDeclaration();
    }
    unparseMFuncDeclStmt(memberFunctionDeclaration, ninfo);
    break;
  }

  case V_SgVariableDeclaration: {
    printf("Unparsing of SgVariableDeclaration in "
           "unparseTemplateInstantiationDirectiveStmt not implemented \n");

    SgVariableDeclaration *variableDeclaration =
        isSgVariableDeclaration(declarationStatement);
    ASSERT_not_null(variableDeclaration);

    unparseVarDeclStmt(variableDeclaration, info);
    break;
  }

    // DQ (8/13/2005): Added this case because it comes up in compiling KULL
    // (KULL/src/transport/CommonMC/Particle/mcapm.cc)
  case V_SgMemberFunctionDeclaration: {
    // DQ (8/31/2005): This should be an error now!  Template instantiations
    // never generate a SgMemberFunctionDeclaration and always generate a
    // SgTemplateInstantiationMemberFunctionDecl
    printf("Error: SgMemberFunctionDeclaration case found in "
           "unparseTemplateInstantiationDirectiveStmt ... (exiting) \n");
    ROSE_ABORT();
    break;
  }

    // DQ (2/2/2018): Added case for currently unimplemented unparsing support
    // for template variable declarations.
  case V_SgTemplateVariableDeclaration: {
    // printf ("Unparsing of SgTemplateVariableDeclaration in
    // unparseTemplateInstantiationDirectiveStmt not implemented \n");
    // ROSE_ASSERT(false);

    SgTemplateVariableDeclaration *variableDeclaration =
        isSgTemplateVariableDeclaration(declarationStatement);
    ASSERT_not_null(variableDeclaration);

    unparseTemplateVariableDeclStmt(variableDeclaration, info);
    break;
  }

  default: {
    printf("Error: default reached in switch (declarationStatement = %s) \n",
           declarationStatement->class_name().c_str());
    ROSE_ABORT();
  }
  }
}

void Unparse_ExprStmt::unparseTemplateInstantiationDeclStmt(
    SgStatement *stmt, SgUnparse_Info &info) {
  // DQ (2/29/2004): New function to support templates
  SgTemplateInstantiationDecl *templateInstantiationDeclaration =
      isSgTemplateInstantiationDecl(stmt);
  ASSERT_not_null(templateInstantiationDeclaration);

  SgClassDeclaration *classDeclaration =
      isSgClassDeclaration(templateInstantiationDeclaration);
  ASSERT_not_null(classDeclaration);

#if OUTPUT_DEBUGGING_CLASS_NAME || 0
  printf(
      "Inside of unparseTemplateInstantiationDeclStmt() stmt = %p/%p name = %s "
      " templateName = %s transformed = %s/%s prototype = %s "
      "compiler-generated = %s compiler-generated and marked for output = %s "
      "\n",
      classDeclaration, templateInstantiationDeclaration,
      templateInstantiationDeclaration->get_name().str(),
      templateInstantiationDeclaration->get_templateName().str(),
      isTransformed(templateInstantiationDeclaration) ? "true" : "false",
      (templateInstantiationDeclaration->get_file_info()->isTransformation() ==
       true)
          ? "true"
          : "false",
      (templateInstantiationDeclaration->get_definition() == NULL) ? "true"
                                                                   : "false",
      (templateInstantiationDeclaration->get_file_info()
           ->isCompilerGenerated() == true)
          ? "true"
          : "false",
      (templateInstantiationDeclaration->get_file_info()
           ->isCompilerGeneratedNodeToBeUnparsed() == true)
          ? "true"
          : "false");
#endif

  // Call the unparse function for a class declaration
  // If the template has not been modified then don't output the template
  // specialization (force the backend compiler to handle the template
  // specialization and instantiation).

  // DQ (8/2/2012): I think we don't need to always outoput the template
  // instantiation. However, we do want to test this so that we make sure that
  // when we do we can. Test code test2012_155.C demonstrates that we need to
  // output the template arguments with qualification.  I'm not sure how this
  // has been missed before. The fix will be to build up the name from the
  // template name and template arguments so that we can support the use of any
  // qualified names that might be associated with these.

  bool outputClassTemplateInstantiation = true;

  if (isTransformed(templateInstantiationDeclaration) == true) {
    // If the template has been transformed then we have to output the special
    // version of the template as a template specialization.

    // If a class template has been modified then we need to make sure that all
    // the
    //      static data members, and
    //      member functions
    // are instantiated (on the next pass through the prelinker). The process
    // should involve a call to the frontend helper:
    //      static void set_instantiation_required_for_template_class_members
    //      (a_type_ptr class_type)
    // I am not currently sure how to make this happen, but it should involve
    // the *.ti files (I guess).

    // DQ (8/19/2005): If transformed then always output the template
    // instantiation (no matter where it is from)
    outputClassTemplateInstantiation = true;
  } else {
    if (templateInstantiationDeclaration->get_file_info()
            ->isOutputInCodeGeneration() == true) {
#if PRINT_DEVELOPER_WARNINGS || 0
      printf("In unparseTemplateInstantiationDeclStmt(): Class template is "
             "marked for output in the current source file. \n");
#endif
      outputClassTemplateInstantiation = true;
    } else {
#if PRINT_DEVELOPER_WARNINGS || 0
      printf("In unparseTemplateInstantiationDeclStmt(): Class template is NOT "
             "marked for output in the current source file. \n");
#endif
    }
  }

  if (outputClassTemplateInstantiation == true) {
    // DQ (5/8/2004): Make this an explicit specialization (using the newer C++
    // syntax to support this) DQ (3/2/2005): Uncommented output of "template
    // <>"

    // DQ (9/1/2005): This is a temporary fix to handle a bug in g++ (3.3.x
    // and 3.4.x) which requires class specialization declared in a namespace to
    // be output in a namespace instead of with the alternative proper namespace
    // name qualifiers. Note: If this is in a nested namespace then it might be
    // that this will not work (since namespace names can't be name qualified).
    // A loop over the nested namespaces might be required to handle this case,
    // better yet would be to transform the AST in a special pass to fix this up
    // to handle backend compiler limitations (as we currently do for other
    // backend compiler bugs).
    SgNamespaceDefinitionStatement *namespaceDefinition =
        isSgNamespaceDefinitionStatement(classDeclaration->get_scope());
    bool locatedInNamespace = (namespaceDefinition != NULL);

    // DQ (8/2/2012): Set this to be always false for the updated frontend work
    // (see test2004_112.C).
    locatedInNamespace = false;

    // printf ("locatedInNamespace = %s \n",locatedInNamespace ? "true" :
    // "false");
    if (locatedInNamespace == true) {
      string namespaceName =
          namespaceDefinition->get_namespaceDeclaration()->get_name().str();
      // curprint ( string("namespace std /* temporary fix for g++ bug in
      // namespace name qualification */ \n   {";
      curprint(string("namespace ") + namespaceName +
               " /* temporary fix for g++ bug in namespace name qualification "
               "*/ \n   {");
    }
    // curprint ( string("\n /* unparseTemplateInstantiationDeclStmt */ ";
    // DQ (8/29/2005): This is now output by the
    // Unparse_ExprStmt::outputTemplateSpecializationSpecifier() member function
    // curprint ( string("\ntemplate <> \n";
    unparseClassDeclStmt(classDeclaration, info);

    if (locatedInNamespace == true) {
      curprint("   }");
    }
  } else {
    // If not transformed then we only want to output the template (and usually
    // only the name of the template specialization) in variable declarations
    // and the like. These locations control the output of the template
    // specialization explicitly through the SgUnparse_Info object (default for
    // outputClassTemplateName is false).
    if (info.outputClassTemplateName() == true) {
      // DQ (9/8/2004):
      // The unparseClassDeclStmt does much more that what we require and is not
      // setup to handle templates well, it generally works well if the template
      // name required does not need any qualification (e.g.
      // std::templateName<int>).  Thus don't use the unparseClassDeclStmt and
      // just output the qualified template name.
      // unparseClassDeclStmt(classDeclaration,info);

      // Output the qualified template name
      curprint(templateInstantiationDeclaration->get_qualified_name().str());
    }
  }
}

void Unparse_ExprStmt::unparseTemplateInstantiationFunctionDeclStmt(
    SgStatement *stmt, SgUnparse_Info &info) {
  // DQ (6/8/2005): If this is an inlined function, we need to make sure that
  // the function has not been used anywhere before where we output it here.

  // DQ (3/24/2004): New function to support templates
  SgTemplateInstantiationFunctionDecl
      *templateInstantiationFunctionDeclaration =
          isSgTemplateInstantiationFunctionDecl(stmt);
  ASSERT_not_null(templateInstantiationFunctionDeclaration);
  ASSERT_not_null(templateInstantiationFunctionDeclaration->get_file_info());

  SgFunctionDeclaration *functionDeclaration =
      isSgFunctionDeclaration(templateInstantiationFunctionDeclaration);

  ASSERT_not_null(functionDeclaration);

#if OUTPUT_DEBUGGING_FUNCTION_NAME || 0
  printf("In unparseTemplateInstantiationFunctionDeclStmt() name = %s "
         "(qualified_name = %s)  transformed = %s prototype = %s static = %s "
         "friend = %s compiler generated = %s transformed = %s output = %s \n",
         // templateInstantiationFunctionDeclaration->get_name().str(),
         templateInstantiationFunctionDeclaration->get_name().str(),
         templateInstantiationFunctionDeclaration->get_qualified_name().str(),
         isTransformed(templateInstantiationFunctionDeclaration) ? "true"
                                                                 : "false",
         (templateInstantiationFunctionDeclaration->get_definition() == NULL)
             ? "true"
             : "false",
         (templateInstantiationFunctionDeclaration->get_declarationModifier()
              .get_storageModifier()
              .isStatic() == true)
             ? "true"
             : "false",
         (templateInstantiationFunctionDeclaration->get_declarationModifier()
              .isFriend() == true)
             ? "true"
             : "false",
         (templateInstantiationFunctionDeclaration->get_file_info()
              ->isCompilerGenerated() == true)
             ? "true"
             : "false",
         (templateInstantiationFunctionDeclaration->get_file_info()
              ->isTransformation() == true)
             ? "true"
             : "false",
         (templateInstantiationFunctionDeclaration->get_file_info()
              ->isOutputInCodeGeneration() == true)
             ? "true"
             : "false");
#endif

  bool outputInstantiatedTemplateFunction = false;
  if (isTransformed(templateInstantiationFunctionDeclaration) == true) {
    // DQ (5/16/2005): This is an attempt to remove explicit declarations of
    // specializations which are preventing template instantiations of function
    // definitions within the prelinking process.
    bool skipforwardDeclarationOfTemplateSpecialization =
        (templateInstantiationFunctionDeclaration->get_file_info()
             ->isCompilerGenerated() == true) &&
        (templateInstantiationFunctionDeclaration->get_definition() == NULL) &&
        (templateInstantiationFunctionDeclaration->get_definingDeclaration() ==
         NULL);
    if (skipforwardDeclarationOfTemplateSpecialization == true) {
      // This is a compiler generated forward function declaration of a template
      // instatiation, so skip it!
#if OUTPUT_PLACEHOLDER_COMMENTS_FOR_SUPRESSED_TEMPLATE_IR_NODES || 0
      curprint("\n/* Skipping output of compiler generated forward function "
               "declaration of a template specialization */");
#endif
#if PRINT_DEVELOPER_WARNINGS || 0
      printf("This is a compiler generated forward function declaration of a "
             "template instatiation, so skip it! \n");
      curprint(string("\n/* Skipping output of compiler generated forward "
                      "function declaration of a template specialization */"));
#endif
      return;
    }

    // TV (10/15/18): this is an issue when forcing ROSE to unparse the template
    // instantiation in Kripke::Kernel

#if PRINT_DEVELOPER_WARNINGS || 0
    curprint(string(
        "\n/* In unparseTemplateInstantiationFunctionDeclStmt(): part of "
        "transformation - output the template function declaration */ \n "));
#endif
    outputInstantiatedTemplateFunction = true;
  } else {
    // Also output the template member function declaration the template
    // declaration appears in the source file.
    string currentFileName = getFileName();
    // DQ (5/2/2012): If the template declaration is not available then it is
    // likely that is does not exist and so we will need to output the
    // instantiation. The problem with this is that we actually build a template
    // declaration in this case but it is not represented by a string (so until
    // we abandon the string use of the template declaration in the unparsing we
    // can't take advantage of this).
    // ASSERT_not_null(templateInstantiationFunctionDeclaration->get_templateDeclaration());
    if (templateInstantiationFunctionDeclaration->get_templateDeclaration() !=
        NULL) {
      ASSERT_not_null(
          templateInstantiationFunctionDeclaration->get_templateDeclaration()
              ->get_file_info());
      ASSERT_not_null(
          templateInstantiationFunctionDeclaration->get_templateDeclaration()
              ->get_file_info()
              ->get_filename());
      string declarationFileName =
          templateInstantiationFunctionDeclaration->get_templateDeclaration()
              ->get_file_info()
              ->get_filename();
      // if ( declarationFileName == currentFileName )
      // if ( declarationFileName == currentFileName &&
      // templateInstantiationMemberFunctionDeclaration->get_file_info()->isOutputInCodeGeneration()
      // == true)

      // DQ (7/21/2012): We have already made the decission to output this
      // function declaration (before we called this function). and in the case
      // of an explicit template specialization the souce position would have a
      // valid location with isOutputInCodeGeneration() set to false (since this
      // would not be compiler generated) see test2012_132.C for an example.

      // if (
      // templateInstantiationFunctionDeclaration->get_file_info()->isCompilerGenerated()
      // == false )
      if (templateInstantiationFunctionDeclaration->get_file_info()
                  ->isCompilerGenerated() == false &&
          templateInstantiationFunctionDeclaration->get_file_info()
                  ->isSourcePositionUnavailableInFrontend() == false) {
        // DQ (8/2/2012): If this is not compiler generated then is was in the
        // original source code ans we have to out put it in the generated code.
        outputInstantiatedTemplateFunction = true;
      } else {
        // DQ (8/2/2012): Else it was compiler generated and we have to
        // explicitly ask if we have seperately determined that it should be
        // output.
        // We only look at the isOutputInCodeGeneration() if this is a compiler
        // generated function.
        if (templateInstantiationFunctionDeclaration->get_file_info()
                ->isOutputInCodeGeneration() == true) {
#if PRINT_DEVELOPER_WARNINGS
          curprint(
              string("\n/* In unparseTemplateInstantiationFunctionDeclStmt(): "
                     "output the template function declaration */ \n "));
#endif
          outputInstantiatedTemplateFunction = true;
        } else {
          // curprint ( string("\n/* In
          // unparseTemplateInstantiationFunctionDeclStmt(): skip output of
          // template function declaration */ \n ";
#if PRINT_DEVELOPER_WARNINGS
          curprint(
              string("/* Skipped output of template function declaration (name "
                     "= ") +
              templateInstantiationFunctionDeclaration->get_qualified_name()
                  .str() +
              ") */ \n");
#endif
        }
      }
    } else {
      // DQ (5/2/2012): We need to output the template instantiation in some
      // cases (see documentation above).
      printf("Note: the template function declaration is not available (this "
             "happens for declarations in templated classes): so output the "
             "template instantiation. stmt = %p = %s \n",
             stmt, stmt->class_name().c_str());
      outputInstantiatedTemplateFunction = true;
    }
  }

  if (outputInstantiatedTemplateFunction == true) {
    // const SgTemplateArgumentPtrList& templateArgListPtr =
    // templateInstantiationFunctionDeclaration->get_templateArguments();

    // Now output the function declaration
    unparseFuncDeclStmt(functionDeclaration, info);
  }
}

void Unparse_ExprStmt::unparseTemplateInstantiationMemberFunctionDeclStmt(
    SgStatement *stmt, SgUnparse_Info &info) {
  // Rules for output of member templates functions:
  //  1) When we unparse the template declaration as a string the frontend
  //  removes the member
  //     function definitions so we are forced to output all template member
  //     functions.
  //  2) If the member function is specified outside of the class then we don't
  //  have to
  //     explicitly output the instantiation.

  // DQ (3/24/2004): New function to support templates
  SgTemplateInstantiationMemberFunctionDecl
      *templateInstantiationMemberFunctionDeclaration =
          isSgTemplateInstantiationMemberFunctionDecl(stmt);
  ASSERT_not_null(templateInstantiationMemberFunctionDeclaration);

  // #if OUTPUT_DEBUGGING_FUNCTION_NAME

  // DQ (6/1/2005): Use this case when PROTOTYPE_INSTANTIATIONS_IN_IL is true in
  // the legacy frontend's host_envir.h
  bool outputMemberFunctionTemplateInstantiation = false;
  if (isTransformed(templateInstantiationMemberFunctionDeclaration) == true) {
    // Always output the template member function declaration if they are
    // transformed.
    SgDeclarationStatement *definingDeclaration =
        templateInstantiationMemberFunctionDeclaration
            ->get_definingDeclaration();
    // ASSERT_not_null(definingDeclaration);
    SgMemberFunctionDeclaration *memberFunctionDeclaration =
        (definingDeclaration == NULL)
            ? NULL
            : isSgMemberFunctionDeclaration(definingDeclaration);
    // ASSERT_not_null(memberFunctionDeclaration);
    // SgTemplateDeclaration* templateDeclaration =
    // templateInstantiationMemberFunctionDeclaration->get_templateDeclaration();
    // ASSERT_not_null(templateDeclaration);

    bool hasDefinition = (memberFunctionDeclaration != NULL &&
                          memberFunctionDeclaration->get_definition() != NULL);
    if (hasDefinition == true) {
      outputMemberFunctionTemplateInstantiation = true;
    } else {
#if OUTPUT_PLACEHOLDER_COMMENTS_FOR_SUPRESSED_TEMPLATE_IR_NODES
      curprint(" /* function has no definition, so skip output */ ");
#endif
    }
  } else {
    // Also output the template member function declaration the template
    // declaration appears in the source file.
    string currentFileName = getFileName();

    if (templateInstantiationMemberFunctionDeclaration
            ->get_templateDeclaration() == NULL) {
      // DQ (4/6/2014): This happens when a member function template in embedded
      // in a class template and thus there is not an associated template for
      // the member function separate from the class declaration.  It is not
      // rare for many system template libraries (e.g. iostream).
    }

    // DQ (8/19/2005): We only have to test for if the template instantiation
    // is marked for output!
    // DQ (8/17/2005): We have to additionally mark the member function
    // instantiation for output since the correct specialization would
    // disqualify the member function for output! The rules are simple:
    //    1) The member function must be defined in the current source file
    //       (else it will be defined in a header file and we need not output
    //       it explicitly (since we handle only non-transformed template
    //       instantiations in this branch).  And,
    //    2) The member function must be marked for output to avoid the output
    //       of the member function in the case of a template specialiazation.
    // if ( declarationFileName == currentFileName )
    // if ( declarationFileName == currentFileName &&
    // templateInstantiationMemberFunctionDeclaration->get_file_info()->isOutputInCodeGeneration()
    // == true)
    if (SgTemplateInstantiationDecl *associated_class_instantiation =
            isSgTemplateInstantiationDecl(isSgClassDeclaration(
                templateInstantiationMemberFunctionDeclaration
                    ->get_associatedClassDeclaration()));
        associated_class_instantiation != NULL &&
        templateInstantiationMemberFunctionDeclaration->isSpecialization() ==
            false &&
        associated_class_instantiation->get_file_info() != NULL &&
        associated_class_instantiation->get_file_info()
                ->isOutputInCodeGeneration() == true &&
        templateInstantiationMemberFunctionDeclaration->get_file_info() !=
            NULL &&
        (templateInstantiationMemberFunctionDeclaration->get_file_info()
                 ->isCompilerGenerated() == true ||
         templateInstantiationMemberFunctionDeclaration->get_file_info()
                 ->isSourcePositionUnavailableInFrontend() == true)) {
      outputMemberFunctionTemplateInstantiation = false;
    } else if (templateInstantiationMemberFunctionDeclaration->get_file_info()
                   ->isOutputInCodeGeneration() == true) {
      outputMemberFunctionTemplateInstantiation = true;
    } else {
      // DQ (5/22/2013): Added to support output of non-template member
      // functions with valid source position (e.g. the constructor in
      // test2013_176.C).
      if (templateInstantiationMemberFunctionDeclaration->get_file_info()
              ->get_file_id() >= 0) {
        outputMemberFunctionTemplateInstantiation = true;
      }
    }
  }

  if (outputMemberFunctionTemplateInstantiation == true) {
    SgMemberFunctionDeclaration *memberFunctionDeclaration =
        isSgMemberFunctionDeclaration(
            templateInstantiationMemberFunctionDeclaration);
    ASSERT_not_null(memberFunctionDeclaration);

    // DQ (3/3/2005): Commented out since it was a problem in test2004_36.C
    // DQ (5/8/2004): Make this an explicit specialization (using the newer C++
    // syntax to support this) curprint ( string("template <> \n";
    // ASSERT_not_null(templateInstantiationMemberFunctionDeclaration->get_templateArguments());
    // if
    // (templateInstantiationMemberFunctionDeclaration->get_templateArguments()->size()
    // > 0)
    if (templateInstantiationMemberFunctionDeclaration->isSpecialization() ==
        true) {
      if ((templateInstantiationMemberFunctionDeclaration->get_file_info()
               ->isCompilerGenerated() == true) &&
          (templateInstantiationMemberFunctionDeclaration->isForward() ==
           true)) {
        // This is a ROSE generated forward declaration of a ROSE specialized
        // member function (required). It is built in
        // ROSE/src/roseSupport/templateSupport.C void
        // fixupInstantiatedTemplates ( SgProject* project ). The forward
        // declaration is placed directly after the template declaration so that
        // no uses of the function can exist prior to its declaration.  Output a
        // message into the gnerated source code identifying this
        // transformation.
#if PRINT_DEVELOPER_WARNINGS || 0
        curprint(string("\n/* ROSE generated forward declaration of the ROSE "
                        "generated member template specialization */"));
#endif
      } else {
        // This is the ROSE generated template specialization for the template
        // member function (required to be defined since the function is used
        // (called)).  This function is defined at the end of file and may be
        // defined there because a forward declaration for the specialization
        // was output directly after the template declaration (before any use of
        // the function could have been made ???).
#if PRINT_DEVELOPER_WARNINGS || 0
        curprint(
            string("\n/* ROSE generated member template specialization */"));
#endif
      }

      // DQ (8/27/2005): This might be required for g++ 3.4.x and optional for
      // g++ 3.3.x DQ (8/19/2005): It is incorrect when used for non-template
      // member functions on templated classes defined in the class Output the
      // syntax for template specialization (appears to be largely optional (at
      // least with GNU g++) curprint ( string("\ntemplate <> ";
    }

    // DQ (8/29/2005): This is now output by the
    // Unparse_ExprStmt::outputTemplateSpecializationSpecifier() member function
    // curprint ( string("\ntemplate <> ";
    unparseMFuncDeclStmt(memberFunctionDeclaration, info);
  } else {
#if PRINT_DEVELOPER_WARNINGS
    curprint(
        string("/* Skipped output of template member function declaration "
               "(name = ") +
        templateInstantiationMemberFunctionDeclaration->get_qualified_name()
            .str() +
        ") */ \n");
#endif
  }
}

void Unparse_ExprStmt::unparsePragmaDeclStmt(SgStatement *stmt,
                                             SgUnparse_Info &) {
  SgPragmaDeclaration *pragmaDeclaration = isSgPragmaDeclaration(stmt);
  ASSERT_not_null(pragmaDeclaration);

  SgPragma *pragma = pragmaDeclaration->get_pragma();
  ASSERT_not_null(pragma);

  // Request from Boyanna at ANL:
  // DQ (6/22/2006): Start all pragmas at the start of the line.  Since these
  // are handled as IR nodes (#pragma is part of the C and C++ grammar
  // afterall)they are indented for as any other sort of statements.  I have
  // added a CR to put the pragma at the start of the next line.  A better
  // solution might be to have the indent mechanism look ahead to see any
  // upcoming SgPragmaDeclarations so that the indentation (insertion of extra
  // spaces) could be skipped.  This would avoid the insertion of empty lines in
  // the generated code. curprint ( string("#pragma " + pragma->get_pragma() +
  // "\n";

  string pragmaString = pragma->get_pragma();
  string identSubstring = "ident";

  // Test for and ident string (which has some special quoting rules that apply
  // to some compilers)
  if (pragmaString.substr(0, identSubstring.size()) == identSubstring) {
    curprint(string("\n#pragma ident \"") +
             pragmaString.substr(identSubstring.size() + 1) + "\"\n");
  } else {
    curprint(string("\n#pragma ") + pragma->get_pragma() + "\n");
  }

  // printf ("Output the pragma = %s \n",pragma->get_pragma());
  // ROSE_ASSERT (0);
}

void Unparse_ExprStmt::unparseEmptyDeclaration(SgStatement *stmt,
                                               SgUnparse_Info & /*info*/) {
  SgEmptyDeclaration *emptyDeclaration = isSgEmptyDeclaration(stmt);
  ASSERT_not_null(emptyDeclaration);

  // DQ (10/31/2020): This is already called in the unparseStatement() function.
  // DQ (10/31/2020): We need to unparse any associaated comments and CPP
  // directives since this is use with SageInterface::replaceStatement() to
  // preserve comments of removed nodes (since that does not work in the trivial
  // case of remvoving the last statement in a file.
  // unparseAttachedPreprocessingInfo(stmt, info, PreprocessingInfo::before);

  // Nothing to unparse for this case, comment and CPP directives should have
  // been unparsed before getting to this point.

  // DQ (10/31/2020): This is already called in the unparseStatement() function.
  // DQ (10/31/2020): We need to unparse any associaated comments and CPP
  // directives since this is use with SageInterface::replaceStatement() to
  // preserve comments of removed nodes (since that does not work in the trivial
  // case of remvoving the last statement in a file.
  // unparseAttachedPreprocessingInfo(stmt, info, PreprocessingInfo::after);
}

void Unparse_ExprStmt::unparseBasicBlockStmt(SgStatement *stmt,
                                             SgUnparse_Info &info) {
  SgBasicBlock *basic_stmt = isSgBasicBlock(stmt);
  ASSERT_not_null(basic_stmt);

#define DEBUG_BASIC_BLOCK 0

  // unparseAttachedPreprocessingInfo(basic_stmt, info,
  // PreprocessingInfo::before);

#if DEBUG_BASIC_BLOCK || 0
  printf("In unparseBasicBlock (stmt = %p) \n", stmt);
  curprint("/* In unparseBasicBlock */");
#endif

  // DQ (12/15/2014): Debugging, support to detect where this is changed between
  // the top of the block and the bottom of the block.
  bool saved_top_unparsedPartiallyUsingTokenStream =
      info.unparsedPartiallyUsingTokenStream();

  // DQ (12/16/2014): The value of info.unparsedPartiallyUsingTokenStream() is
  // used as a sort of global state, we want it to be consistant within the
  // processing of this function. SgUnparse_Info ninfo(info);
  bool saved_unparsedPartiallyUsingTokenStream =
      info.unparsedPartiallyUsingTokenStream();

#if DEBUG_BASIC_BLOCK
  printf("In unparseBasicBlock (stmt = %p) "
         "saved_unparsedPartiallyUsingTokenStream = %s \n",
         basic_stmt,
         saved_unparsedPartiallyUsingTokenStream ? "true" : "false");
#endif

  // DQ (12/5/2014): Test for if we have unparsed partially using the token
  // stream. If so then we don't want to unparse this syntax, if not then we
  // require this syntax. if (info.unparsedPartiallyUsingTokenStream() == false)
  if (saved_unparsedPartiallyUsingTokenStream == false) {
#if DEBUG_USING_CURPRINT
    curprint("\n/* unparse start of SgBasicBlock: "
             "saved_unparsedPartiallyUsingTokenStream == false: output opening "
             "{ */");
#endif

    unp->cur.format(basic_stmt, info, FORMAT_BEFORE_BASIC_BLOCK1);
    // curprint ( string("{"));
    if (SgProject::get_verbose() > 0)
      curprint("/* syntax from AST */ {");
    else
      curprint("{");

    unp->cur.format(basic_stmt, info, FORMAT_AFTER_BASIC_BLOCK1);
  } else {
#if DEBUG_USING_CURPRINT
    curprint(
        "\n/* unparse start of SgBasicBlock: "
        "saved_unparsedPartiallyUsingTokenStream == true: output opening { */");
#endif
    // Unparse the tokens from the end of the basic block "{".
    // DQ (6/2/2021): Comment out the opening "{".
    // DQ (1/14/2015): We need to unparse syntax instead of the initial token,
    // because this can be a macro expansion (see
    // tests/nonsmoke/functional/roseTests/astInterfaceTests/inputmoveDeclarationToInnermostScope_test2015_57.C).
    // unparseStatementFromTokenStream (stmt, e_leading_whitespace_start,
    // e_token_subsequence_start); unparseStatementFromTokenStream (stmt,
    // e_token_subsequence_start, e_token_subsequence_start); curprint("{");
    unparseStatementFromTokenStream(stmt, e_token_subsequence_start,
                                    e_token_subsequence_start, info);
#if DEBUG_BASIC_BLOCK
    curprint("/* unparse start of SgBasicBlock */");
#endif
  }

  if (basic_stmt->get_asm_function_body().empty() == false) {
#if DEBUG_BASIC_BLOCK
    curprint("/* unparse asm function body of SgBasicBlock */");
#endif
    // This is an asm function body.
    curprint(basic_stmt->get_asm_function_body());

    // Make sure this is a function definition.
    ASSERT_not_null(isSgFunctionDefinition(basic_stmt->get_parent()));
  }

  // DQ (1/9/2007): This is useful for understanding which blocks are marked as
  // compiler generated. curprint ( string(" /* block compiler generated = " +
  // (basic_stmt->get_startOfConstruct()->isCompilerGenerated() ? "true" :
  // "false") + " */ \n ";

  // curprint ( string(" /* block size = " + basic_stmt->get_statements().size()
  // + " */ \n ";

  // printf ("block scope = %p = %s
  // \n",basic_stmt,basic_stmt->class_name().c_str());
  // basic_stmt->get_file_info()->display("basic_stmt block scope: debug");

#if OUTPUT_HIDDEN_LIST_DATA
  outputHiddenListData(basic_stmt);
#endif

#if DEBUG_BASIC_BLOCK
  // DQ (1/7/2015): The funcationality to output the trailing tokens of the last
  // statement is implemented in the unparseStatementFromTokenStream() function.
  SgStatement *last_stmt = NULL;
#endif

  // DQ (10/31/2018): We need to get the current source file from the
  // SgUnparseInfo object instead of computing it through the chain of parent
  // pointers in the translation unit.  This is essential for header file
  // processing since the header file will have a copy of the trnslation unit's
  // global scope. DQ (9/28/2018): We need to get the SgSourceFile so that we
  // can use the correct map from the map of maps in the modified implementation
  // that supports multiple files for token based unparsing. SgSourceFile*
  // sourceFile = TransformationSupport::getSourceFile(basic_stmt);
  SgSourceFile *sourceFile = info.get_current_source_file();

  // DQ (10/31/2018): This is not always non-null (e.g. when used with the
  // unparseToString() function). ASSERT_not_null(sourceFile);

  // DQ (9/28/2018): Older code.
  // SgStatementPtrList::iterator representativeStatementForWhitespace =
  // sourceFile->get_representativeWhitespaceStatementMap()[basic_stmt];

  // DQ (9/28/2018): We need to get the SgSourceFile so that we can use the
  // correct map from the map of maps in the modified implementation that
  // supports multiple files for token based unparsing.
  SgStatement *representativeStatementForWhitespace = NULL;
  // if
  // (SgSourceFile::get_representativeWhitespaceStatementMap().find(basic_stmt)
  // != SgSourceFile::get_representativeWhitespaceStatementMap().end()) if
  // (sourceFile->get_representativeWhitespaceStatementMap().find(basic_stmt) !=
  // sourceFile->get_representativeWhitespaceStatementMap().end())
  if (sourceFile != NULL &&
      sourceFile->get_representativeWhitespaceStatementMap().find(basic_stmt) !=
          sourceFile->get_representativeWhitespaceStatementMap().end()) {
    // representativeStatementForWhitespace =
    // SgSourceFile::get_representativeWhitespaceStatementMap()[basic_stmt];
    representativeStatementForWhitespace =
        sourceFile->get_representativeWhitespaceStatementMap()[basic_stmt];
    ASSERT_not_null(representativeStatementForWhitespace);
  }

  // DQ (11/15/2015): if this is on because it is from an inherited SgBasicBlock
  // then turn off the flag to control formatting. I don't like this method of
  // handling the inherited attribute, and perhaps this poitn to why this
  // formatting should be controled using a different mechanism (though other
  // mechanisms had there problems in thinking them through).
  info.unset_parentStatementListBeingUnparsedUsingPartialTokenSequence();

  SgStatementPtrList::iterator p = basic_stmt->get_statements().begin();
  while (p != basic_stmt->get_statements().end()) {
    ASSERT_not_null((*p));

#if DEBUG_BASIC_BLOCK
    printf("In unparseBasicBlock (block = %p) statement = %p = %s "
           "saved_unparsedPartiallyUsingTokenStream = %s \n",
           basic_stmt, *p, (*p)->class_name().c_str(),
           saved_unparsedPartiallyUsingTokenStream ? "true" : "false");
#endif
#if DEBUG_BASIC_BLOCK && 0
    curprint("/* LOOP: START unparse statement in SgBasicBlock */");
#endif

    SgUnparse_Info local_info(info);

    // DQ (11/4/2015): Adding in the leading white space of the first statement
    // (whatever statement is first).
    if (saved_unparsedPartiallyUsingTokenStream == true) {
      // DQ (11/12/2015): We don't want to do this for just the first statement.
      // if (p == basic_stmt->get_statements().begin())
      {
        // We want to output the whitespace of the first statement, but the
        // first statement may have been moved. But we can at least output the
        // leading white space for whateve is currently the first statement.
        // Unfortunately this can cause problems if this is more than just
        // whitespace (e.g. "#if 1"). So we need to check if this is only
        // whitespace and then we can unparse it.  This would be best handled by
        // adding this feature to the unparseStatementFromTokenStream() function
        // (I think).

        local_info
            .set_parentStatementListBeingUnparsedUsingPartialTokenSequence();

        // curprint("\n");
        bool statement_is_transformation = (*p)->isTransformation();
#if DEBUG_BASIC_BLOCK || 0
        printf("statement is: %p = %s isTransformation() = %s \n", (*p),
               (*p)->class_name().c_str(),
               (*p)->isTransformation() ? "true" : "false");
        string s = statement_is_transformation ? "true" : "false";
#endif
        if (statement_is_transformation == true) {
          // An additional issue is that we should implement
          // unparseOnlyWhitespace support to only unparse the trailing
          // whitespace tokens instead of all the tokens except for
          // non-whitespace).
          // DQ (11/20/2015): This implementation uses a previously prepared map
          // of representative statements in the scope so that we can support
          // the use of the whitespace from these statements when unparsing
          // statements in the current scope that are transformations.
          SgStatement *q = representativeStatementForWhitespace;
          if (q != NULL) {
            // Found a statement in the basic block that we can use to represent
            // representative whitespace.
            bool unparseOnlyWhitespace = true;
            unparseStatementFromTokenStream(q, q, e_leading_whitespace_start,
                                            e_token_subsequence_start, info,
                                            unparseOnlyWhitespace);
          } else {
            // This is the backup plan if there was no identified statement
            // associated with the current scope.
            curprint("\n");
          }
        }
#if DEBUG_BASIC_BLOCK || 0
        curprint("/* unparse leading white space of first statement: END */");
#endif
      }
    }

#if DEBUG_BASIC_BLOCK || 0
    curprint("/* calling unparseStatement(): START */");
#endif
    // unparseStatement((*p), info);
    unparseStatement((*p), local_info);

#if DEBUG_BASIC_BLOCK || 0
    curprint("/* calling unparseStatement(): END */");
#endif

#if DEBUG_BASIC_BLOCK || 0
    curprint("/* LOOP: END unparse statement in SgBasicBlock */");
#endif
    // DQ (12/6/2014): Save the last statement so that we can use the trailing
    // token stream if using the token-based unparsing. last_stmt = *p;

    p++;
  }

#if DEBUG_BASIC_BLOCK || 0
  printf(
      "Inside of Unparse_ExprStmt::unparseBasicBlockStmt: output comment \n");
  curprint("/* Inside of Unparse_ExprStmt::unparseBasicBlockStmt: output "
           "comment */");
#endif

#if DEBUG_BASIC_BLOCK
  printf("Inside of Unparse_ExprStmt::unparseBasicBlockStmt: "
         "saved_unparsedPartiallyUsingTokenStream = %s \n",
         saved_unparsedPartiallyUsingTokenStream ? "true" : "false");
#endif

  // DQ (12/16/2014): This should be controled by the
  // saved_unparsedPartiallyUsingTokenStream value. DQ (3/17/2005): This helps
  // handle cases such as void foo () { #include "constant_code.h" }
  // unparseAttachedPreprocessingInfo(basic_stmt, info,
  // PreprocessingInfo::inside);

  if (saved_unparsedPartiallyUsingTokenStream == false) {
    // DQ (3/17/2005): This helps handle cases such as void foo () { #include
    // "constant_code.h" }

#if DEBUG_BASIC_BLOCK || 0
    printf("Calling unparseAttachedPreprocessingInfo(): INSIDE: basic_stmt = "
           "%p = %s \n",
           basic_stmt, basic_stmt->class_name().c_str());
    printOutComments(basic_stmt);
    printf("In unparseBasicBlockStmt(): info.SkipFunctionDefinition() = %s \n",
           info.SkipFunctionDefinition() ? "true" : "false");
#endif

    unparseAttachedPreprocessingInfo(basic_stmt, info,
                                     PreprocessingInfo::inside);
  }

#if DEBUG_BASIC_BLOCK
  printf("DONE: Inside of Unparse_ExprStmt::unparseBasicBlockStmt: output "
         "comment \n");
  curprint("/* DONE: Inside of Unparse_ExprStmt::unparseBasicBlockStmt: output "
           "comment */");
#endif

#if DEBUG_BASIC_BLOCK
  printf("unparse end of SgBasicBlock: "
         "info.unparsedPartiallyUsingTokenStream() = %s last_stmt = %p \n",
         info.unparsedPartiallyUsingTokenStream() ? "true" : "false",
         last_stmt);
  if (last_stmt != NULL) {
    printf("   --- last_stmt = %p = %s \n", last_stmt,
           last_stmt->class_name().c_str());
  }
#endif

  // DQ (12/15/2014): Debugging, support to detect where this is changed between
  // the top of the block and the bottom of the block.
  bool saved_bottom_unparsedPartiallyUsingTokenStream =
      info.unparsedPartiallyUsingTokenStream();
  // ROSE_ASSERT(saved_top_unparsedPartiallyUsingTokenStream ==
  // saved_bottom_unparsedPartiallyUsingTokenStream);
  if (saved_top_unparsedPartiallyUsingTokenStream !=
      saved_bottom_unparsedPartiallyUsingTokenStream) {
#if DEBUG_BASIC_BLOCK
    printf("WARNING: value of info.unparsedPartiallyUsingTokenStream() changed "
           "within SgBasicBlock \n");
#endif
  }

#if DEBUG_BASIC_BLOCK
  curprint("/* unparse end of SgBasicBlock */");
#endif
#if DEBUG_USING_CURPRINT
  curprint("\n/* unparse end of SgBasicBlock: output closing } */");
#endif

  // DQ (12/5/2014): Test for if we have unparsed partially using the token
  // stream. If so then we don't want to unparse this syntax, if not then we
  // require this syntax. if (info.unparsedPartiallyUsingTokenStream() == false)
  if (saved_unparsedPartiallyUsingTokenStream == false) {
    unp->cur.format(basic_stmt, info, FORMAT_BEFORE_BASIC_BLOCK2);

#if DEBUG_USING_CURPRINT
    curprint("\n/* unparse end of SgBasicBlock: "
             "saved_unparsedPartiallyUsingTokenStream == false: output closing "
             "} */");
#endif
    // curprint ( string("}"));
    if (SgProject::get_verbose() > 0)
      curprint("/* syntax from AST */ }");
    else
      curprint("}");

    unp->cur.format(basic_stmt, info, FORMAT_AFTER_BASIC_BLOCK2);
  } else {
#if DEBUG_BASIC_BLOCK
    printf("unparse last token in SgBasicBlock \n");
    curprint("/* unparse last token in SgBasicBlock */");
#endif

#if DEBUG_USING_CURPRINT
    curprint(
        "\n/* unparse end of SgBasicBlock: "
        "saved_unparsedPartiallyUsingTokenStream == true: output closing } */");
#endif
    // DQ (1/14/2015): We need to unparse syntax instead of the initial token,
    // because this can be a macro expansion (see
    // tests/nonsmoke/functional/roseTests/astInterfaceTests/inputmoveDeclarationToInnermostScope_test2015_57.C).
    // unparseStatementFromTokenStream (stmt, e_token_subsequence_end,
    // e_token_subsequence_end);

#if DEBUG_BASIC_BLOCK
    // DQ (5/25/2021): I think this might be a mistake to output anything here,
    // since the token stream's closing brace should be output.
    printf("Should we be skipping output of closing } in "
           "unparseBasicBlockStmt() \n");
#endif

#if DEBUG_USING_CURPRINT || 0
    curprint("\n/* unparseBasicBlock(): unparse closing } (skipping) */\n");
#endif
    // Unparse the tokens from the end of the basic block "{".
    // unparseStatementFromTokenStream (stmt, function_body,
    // e_token_subsequence_start, e_token_subsequence_start, info);
    // unparseStatementFromTokenStream (stmt, function_body,
    // e_token_subsequence_start, e_leading_whitespace_end, info);
    // unparseStatementFromTokenStream (stmt, function_definition,
    // e_token_subsequence_start, e_leading_whitespace_end, info);
    // unparseStatementFromTokenStream (stmt, class_definition,
    // e_token_subsequence_end, e_leading_whitespace_end, info);
    // unparseStatementFromTokenStream (class_definition, stmt,
    // e_token_subsequence_end, e_token_subsequence_end, info);

    // DQ (6/2/2021): We need to unparse the closing "}" from the SgBasicBlock
    // (even when used in a SgFunctionDeclaration). DQ (5/29/2021): This appear
    // that it might be the best solution, namely to let the output of the
    // trailing white-space also output the last token for what would close off
    // the SgBasicBlock. If we are only unparsing a "}" then it does not make
    // much different if we out put it directly or from the token stream unless
    // macros are at play. curprint("}");
    unparseStatementFromTokenStream(stmt, e_token_subsequence_end,
                                    e_token_subsequence_end, info);
  }

#if DEBUG_USING_CURPRINT
  curprint("\n/* Leaving unparseBasicBlock */");
#endif
#if DEBUG_BASIC_BLOCK || 0
  printf("Leaving unparseBasicBlock (stmt = %p) \n", stmt);
  curprint("/* Leaving unparseBasicBlock */");
#endif
}

// Determine how many "else {}"'s an outer if that has an else clause needs to
// prevent dangling if problems
static size_t countElsesNeededToPreventDangling(SgStatement *s) {
  // The basic rule here is that anything that has a defined end marker
  // (i.e., cannot end with an unmatched if statement) returns 0, everything
  // else (except if) gets the correct number of elses from its body
  switch (s->variantT()) {
  case V_SgCaseOptionStmt:
    return countElsesNeededToPreventDangling(isSgCaseOptionStmt(s)->get_body());
  case V_SgCatchStatementSeq: {
    SgCatchStatementSeq *cs = isSgCatchStatementSeq(s);
    const SgStatementPtrList &seq = cs->get_catch_statement_seq();
    ROSE_ASSERT(!seq.empty());
    return countElsesNeededToPreventDangling(seq.back());
  }
  case V_SgDefaultOptionStmt:
    return countElsesNeededToPreventDangling(isSgCaseOptionStmt(s)->get_body());
  case V_SgLabelStatement:
    return countElsesNeededToPreventDangling(
        isSgLabelStatement(s)->get_statement());
  case V_SgCatchOptionStmt:
    return countElsesNeededToPreventDangling(
        isSgCatchOptionStmt(s)->get_body());
  case V_SgForStatement:
    return countElsesNeededToPreventDangling(
        isSgForStatement(s)->get_loop_body());
  case V_SgIfStmt: {
    SgIfStmt *ifs = isSgIfStmt(s);
    if (ifs->get_false_body() != NULL) {
      return 0;
    } else {
      return countElsesNeededToPreventDangling(ifs->get_true_body()) + 1;
    }
  }
  case V_SgWhileStmt:
    return countElsesNeededToPreventDangling(isSgWhileStmt(s)->get_body());
  case V_SgSwitchStatement:
    ROSE_ASSERT(isSgBasicBlock(isSgSwitchStatement(s)->get_body()));
    return 0;

  default:
    return 0;
  }
}

void Unparse_ExprStmt::unparseIfStmt(SgStatement *stmt, SgUnparse_Info &info) {
  // DQ (12/13/2005): I don't like this implementation with the while loop...

  bool saved_unparsedPartiallyUsingTokenStream =
      info.unparsedPartiallyUsingTokenStream();

  SgIfStmt *if_stmt = isSgIfStmt(stmt);
  assert(if_stmt != NULL);

  while (if_stmt != NULL) {
    SgStatement *tmp_stmt = NULL;

    // DQ (12/6/2014): Test for if we have unparsed partially using the token
    // stream. If so then we don't want to unparse this syntax, if not then we
    // require this syntax. curprint ( string("if (")); if
    // (info.unparsedPartiallyUsingTokenStream() == false)
    if (saved_unparsedPartiallyUsingTokenStream == false) {
      curprint("if (");

      SgUnparse_Info testInfo(info);
      testInfo.set_SkipSemiColon();
      testInfo.set_inConditional();
      // info.set_inConditional();
      if ((tmp_stmt = if_stmt->get_conditional())) {
        // Unparse using base class function so we get any required comments and
        // CPP directives. unparseStatement(tmp_stmt, testInfo);
        UnparseLanguageIndependentConstructs::unparseStatement(tmp_stmt,
                                                               testInfo);
      }
      testInfo.unset_inConditional();
      // curprint ( string(") "));
      if (SgProject::get_verbose() > 0)
        curprint("/* syntax from AST */ ) ");
      else if (isSgBasicBlock(if_stmt->get_true_body()) != nullptr)
        curprint(") ");
      else
        curprint(")");

      // DQ (9/24/2020): Adding support to unparse attached pragmas.
      unparsePragmaAttribute(if_stmt);
    } else {
      // DQ (12/9/2014): Adding more support for unparsing using the token
      // stream.
      SgStatement *true_body = if_stmt->get_true_body();
      if (true_body != NULL) {
        // DQ (1/18/2015): With the denormalization of SgBasicBlock in the
        // SgIfStmt false body we have to use the computed if_stmt and not the
        // outer stmt. unparseStatementFromTokenStream (stmt, true_body,
        // e_leading_whitespace_start, e_token_subsequence_start);
        // unparseStatementFromTokenStream (stmt, true_body,
        // e_token_subsequence_start, e_token_subsequence_start);
        // unparseStatementFromTokenStream (stmt, true_body,
        // e_token_subsequence_start, e_leading_whitespace_start);
        unparseStatementFromTokenStream(if_stmt, true_body,
                                        e_token_subsequence_start,
                                        e_leading_whitespace_start, info);
      }
    }

    if ((tmp_stmt = if_stmt->get_true_body())) {
      // DQ (12/6/2014): Test for if we have unparsed partially using the token
      // stream. If so then we don't want to unparse this syntax, if not then we
      // require this syntax. unp->cur.format(tmp_stmt, info,
      // FORMAT_BEFORE_NESTED_STATEMENT); if
      // (info.unparsedPartiallyUsingTokenStream() == false)
      if (saved_unparsedPartiallyUsingTokenStream == false) {
        unp->cur.format(tmp_stmt, info, FORMAT_BEFORE_NESTED_STATEMENT);
      }

      // Unparse using base class function so we get any required comments and
      // CPP directives. unparseStatement(tmp_stmt, info);
      UnparseLanguageIndependentConstructs::unparseStatement(tmp_stmt, info);

      // DQ (12/6/2014): Test for if we have unparsed partially using the token
      // stream. If so then we don't want to unparse this syntax, if not then we
      // require this syntax. unp->cur.format(tmp_stmt, info,
      // FORMAT_AFTER_NESTED_STATEMENT); if
      // (info.unparsedPartiallyUsingTokenStream() == false)
      if (saved_unparsedPartiallyUsingTokenStream == false) {
        unp->cur.format(tmp_stmt, info, FORMAT_AFTER_NESTED_STATEMENT);
      }
    }

    if ((tmp_stmt = if_stmt->get_false_body())) {
      size_t elsesNeededForInnerIfs =
          countElsesNeededToPreventDangling(if_stmt->get_true_body());
      for (size_t i = 0; i < elsesNeededForInnerIfs; ++i) {
        curprint(string(" else {}")); // Ensure this else does not match an
                                      // inner if statement
      }

      // unp->cur.format(if_stmt, info, FORMAT_BEFORE_STMT);
      // curprint ( string("else "));
      // if (info.unparsedPartiallyUsingTokenStream() == false)
      if (saved_unparsedPartiallyUsingTokenStream == false) {
        unp->cur.format(if_stmt, info, FORMAT_BEFORE_STMT);
        // curprint ( string("else "));
        if (SgProject::get_verbose() > 0) {
          curprint("/* syntax from AST (part 1) */ else ");
        } else {
          // DQ (1/4/2015): Remove trailing space to avoid redundant output of
          // whitespace in token unparsing. We actually need to extra space to
          // avoid unparsing "elseif" by mistake (likely we can address this
          // detail later). curprint(" else ");
          curprint(" else ");
        }
      } else {
        // DQ (12/9/2014): Adding more support for unparsing using the token
        // stream.
        SgStatement *true_body = if_stmt->get_true_body();
        SgStatement *false_body = if_stmt->get_false_body();
        if (true_body != NULL && false_body != NULL) {
          // DQ (1/4/2015): If the false body is a transformation then the token
          // sequence will not exist and the unparseStatementFromTokenStream()
          // function will not output any token sequence.
          // unparseStatementFromTokenStream (true_body, false_body,
          // e_trailing_whitespace_start, e_token_subsequence_start);
          if (SgProject::get_verbose() > 0) {
            curprint("/* syntax from AST (part 2) */ else ");
          } else {
            // curprint(" else ");
            // printf ("In unparseIfStmt(): Output the else part between the
            // true and false cases of the if statement \n");
            // unparseStatementFromTokenStream (true_body, false_body,
            // e_trailing_whitespace_start, e_token_subsequence_start);
            // unparseStatementFromTokenStream (true_body, true_body,
            // e_trailing_whitespace_start, e_trailing_whitespace_end);

            // We might need to check that there are whitespace tokens assocated
            // with the trailing whitespace before the else. Also if the false
            // block is a transformation then we need to output a CR or a space.
            if (true_body->isTransformation() == true ||
                false_body->isTransformation() == true) {
              curprint(" else ");
            } else {
              // unparseStatementFromTokenStream (false_body, false_body,
              // e_leading_whitespace_start, e_leading_whitespace_end);
              // unparseStatementFromTokenStream (true_body, false_body,
              // e_trailing_whitespace_start, e_leading_whitespace_start);
              // unparseStatementFromTokenStream (true_body, false_body,
              // e_trailing_whitespace_start, e_leading_whitespace_start);
              unparseStatementFromTokenStream(true_body, false_body,
                                              e_trailing_whitespace_start,
                                              e_leading_whitespace_start, info);
            }
          }
        }
      }
      if_stmt = isSgIfStmt(tmp_stmt);
      if (if_stmt == NULL) {
        // unp->cur.format(tmp_stmt, info, FORMAT_BEFORE_NESTED_STATEMENT);
        // if (info.unparsedPartiallyUsingTokenStream() == false)
        if (saved_unparsedPartiallyUsingTokenStream == false) {
          unp->cur.format(tmp_stmt, info, FORMAT_BEFORE_NESTED_STATEMENT);
        }
        // Unparse using base class function so we get any required comments and
        // CPP directives. unparseStatement(tmp_stmt, info);
        UnparseLanguageIndependentConstructs::unparseStatement(tmp_stmt, info);
        // if (info.unparsedPartiallyUsingTokenStream() == false)
        if (saved_unparsedPartiallyUsingTokenStream == false) {
          unp->cur.format(tmp_stmt, info, FORMAT_AFTER_NESTED_STATEMENT);
        }
      }
    } else {
      if_stmt = NULL;
    }

    // DQ (12/16/2008): Need to process any associated CPP directives and
    // comments
    if (if_stmt != NULL) {
      // At this point if_stmt is a nested if statement in the true and false
      // branch of the original if statement.
      if (saved_unparsedPartiallyUsingTokenStream == true) {
        // DQ (7/2/2021): I think this should be unparsed via the
        // unparseStatement called fro the false body. New code where we unparse
        // the whitespace between the else and the nested if statement.
        // unparseStatementFromTokenStream (false_body, false_body,
        // e_leading_whitespace_start, e_token_subsequence_start);
        // unparseStatementFromTokenStream (if_stmt, e_leading_whitespace_start,
        // e_token_subsequence_start, info); unparseStatementFromTokenStream
        // (if_stmt, e_leading_whitespace_start, e_leading_whitespace_end,
        // info);
      } else {
        // original code if we are not unparsing from the token stream.
        unparseAttachedPreprocessingInfo(if_stmt, info,
                                         PreprocessingInfo::before);
      }
    }
  }
}

// DQ (8/13/2007): This is no longer used, I think, however it might be required
// for the legacy array optimizer.

//--------------------------------------------------------------------------------
//  void Unparse_ExprStmt::unparseWhereStmt
//
//  This special function unparses where and elsewhere statements. Where
//  statements are actually represented as for statements in the Sage program
//  tree. Thus, the type of the where_stmt is SgForStatement. The part that
//  we are interested in unparsing is in the initializer statement of the
//  for statement. In particular, we want to unparse the arguments of the
//  rhs of the initializer. The rhs should be a function call expression.
//  The same applies for elsewhere statements.
//--------------------------------------------------------------------------------
void Unparse_ExprStmt::unparseWhereStmt(SgStatement *stmt,
                                        SgUnparse_Info &info) {
  SgForStatement *where_stmt = isSgForStatement(stmt);
  ASSERT_not_null(where_stmt);

  printf("In Unparse_ExprStmt::unparseWhereStmt() \n");

  SgStatement *tmp_stmt;
  // DQ (4/7/2001) we don't want the unparser to depend on the array grammar
  // (so comment this out and introduce the array "where" statment in some other
  // way)
  curprint(string("elsewhere ("));

  SgUnparse_Info newinfo(info);
  newinfo.set_SkipSemiColon();
  // if(where_stmt->get_init_stmt() != NULL ) {
  if (where_stmt->get_init_stmt().size() > 0) {
    SgStatementPtrList::iterator i = where_stmt->get_init_stmt().begin();
    if ((*i) != NULL && (*i)->variant() == EXPR_STMT) {
      SgExprStatement *pExprStmt = isSgExprStatement(*i);
      // SgAssignOp* pAssignOp = isSgAssignOp(pExprStmt->get_the_expr());
      SgAssignOp *pAssignOp = isSgAssignOp(pExprStmt->get_expression());
      if (pAssignOp != NULL) {
        SgFunctionCallExp *pFunctionCallExp =
            isSgFunctionCallExp(pAssignOp->get_rhs_operand());
        if (pFunctionCallExp != NULL) {
          if (pFunctionCallExp->get_args()) {
            SgExpressionPtrList &list =
                pFunctionCallExp->get_args()->get_expressions();
            SgExpressionPtrList::iterator arg = list.begin();
            while (arg != list.end()) {
              unparseExpression((*arg), newinfo);
              arg++;
              if (arg != list.end()) {
                curprint(string(","));
              }
            }
          }
        } // pFunctionCallExp != NULL
      } // pAssignOp != NULL
    } //(*i).irep() != NULL && (*i).irep()->variant() == EXPR_STMT
  } // where_stmt->get_init_stmt() != NULL

  curprint(string(")"));

  if ((tmp_stmt = where_stmt->get_loop_body())) {
    unparseStatement(tmp_stmt, info);
  } else {
    if (!info.SkipSemiColon()) {
      curprint(string(";"));
    }
  }
}

void Unparse_ExprStmt::unparseForInitStmt(SgStatement *stmt,
                                          SgUnparse_Info &info) {
  // DQ (7/11/2004): Added to simplify debugging for everyone (requested by
  // Willcock)

  SgForInitStatement *forInitStmt = isSgForInitStatement(stmt);
  ASSERT_not_null(forInitStmt);

  SgStatementPtrList::iterator i = forInitStmt->get_init_stmt().begin();

  // DQ (12/8/2004): Build a new info object so that we can supress the
  // unparsing of the base type once the first variable has been unparsed.
  SgUnparse_Info newinfo(info);

  while (i != forInitStmt->get_init_stmt().end()) {

    unparseStatement(*i, newinfo);
    i++;

    // After unparsing the first variable declaration with the type
    // we want to unparse the rest without the base type.
    newinfo.set_SkipBaseType();

    if (i != forInitStmt->get_init_stmt().end()) {
      curprint(string(", "));
    }
  }

  // DQ (11/4/2015): Change the unparsing semantics to for loop initializer
  // statement to exclude the " " after the ";" so that we can more faithfully
  // represent the unparsed code when using the token-based unparsing.
  // curprint("; ");
  curprint(";");
}

void Unparse_ExprStmt::unparseForStmt(SgStatement *stmt, SgUnparse_Info &info) {
  // printf ("Unparse for loop \n");
  SgForStatement *for_stmt = isSgForStatement(stmt);
  ASSERT_not_null(for_stmt);

  bool saved_unparsedPartiallyUsingTokenStream =
      info.unparsedPartiallyUsingTokenStream();

#define DEBUG_FOR_STMT 0

#if DEBUG_FOR_STMT
  printf("In unparseForStmt(): saved_unparsedPartiallyUsingTokenStream = %s \n",
         saved_unparsedPartiallyUsingTokenStream ? "true" : "false");
  curprint("/* Top of unparseForStmt */");
#endif

  // curprint ( string("for ("));
  // DQ (12/5/2014): Test for if we have unparsed partially using the token
  // stream. If so then we don't want to unparse this syntax, if not then we
  // require this syntax. if (info.unparsedPartiallyUsingTokenStream() == false)
  if (saved_unparsedPartiallyUsingTokenStream == false) {
#if DEBUG_FOR_STMT
    printf("In unparseForStmt(): unparse the \"for (\" directly (not using the "
           "token stream) \n");
#endif
    curprint("for (");
  } else {
    // unparseStatementFromTokenStream (stmt, e_leading_whitespace_start,
    // e_token_subsequence_start);

    SgStatement *tmp_stmt = for_stmt->get_for_init_stmt();

    // Not yet clear how to handle case where tmp_stmt == NULL.
    ASSERT_not_null(tmp_stmt);
#if DEBUG_FOR_STMT
    curprint("/* unparse start of SgForStatement */");
    printf("In unparseForStmt(): unparse from token stream from start of "
           "SgForStatement to for loop initializer \n");
#endif

    // DQ (6/5/2021): The problem here is that if the for_init_stmt does not
    // have any leading whitespace the call to unparseStatementFromTokenStream()
    // will not output anything. However, I have now modified the
    // unparseStatementFromTokenStream() function to use
    // e_token_subsequence_start - 1 when e_leading_whitespace_start is note
    // defined (value == -1). More of these sorts of modifications should be
    // possible. unparseStatementFromTokenStream (for_stmt, tmp_stmt,
    // e_token_subsequence_start, e_token_subsequence_start);
    unparseStatementFromTokenStream(for_stmt, tmp_stmt,
                                    e_token_subsequence_start,
                                    e_leading_whitespace_start, info);
#if DEBUG_FOR_STMT
    curprint("/* DONE: unparse start of SgForStatement */");
#endif
  }

  // if (saved_unparsedPartiallyUsingTokenStream == false)
  {
    SgUnparse_Info newinfo(info);
    newinfo.set_SkipSemiColon();
    newinfo.set_inConditional(); // set to prevent printing line and file
                                 // information

#if DEBUG_FOR_STMT
    printf("In unparseForStmt(): unparse the for loop initializer \n");
#endif

    // curprint(" /* initializer */ ");
    SgStatement *tmp_stmt = for_stmt->get_for_init_stmt();
    // curprint(" /* initializer: " + tmp_stmt->class_name() + " */ ");
    // ASSERT_not_null(tmp_stmt);
    // Milind Chabbi (9/5/2013): if the for init statement is NULL or not
    // unparsed, we should output a semicolon.
    if (tmp_stmt == NULL ||
        !statementFromFile(tmp_stmt, getFileName(), newinfo)) {
      curprint("; ");
    } else {
#if DEBUG_FOR_STMT
      curprint("/* Unparse the for_init_stmt */\n ");
      printf("In unparseForStmt(): unparse the for loop initializer (by "
             "calling unparseStatement()) \n");
#endif
      unparseStatement(tmp_stmt, newinfo);

      if (saved_unparsedPartiallyUsingTokenStream == false) {
        // DQ (11/4/2015): Change the unparsing semantics to for loop
        // initializer statement to exclude the " " after the ";" so that we can
        // more faithfully represent the unparsed code when using the
        // token-based unparsing.
        curprint(" ");
      }

#if DEBUG_FOR_STMT
      curprint("/* DONE: Unparse the for_init_stmt */\n ");
#endif
    }
    newinfo.unset_inConditional();

#if DEBUG_FOR_STMT
    printf("In unparseForStmt(): unparse the for loop test expression (can be "
           "simple declaration statement) \n");
#endif

    // DQ (12/13/2005): New code for handling the test (which could be a
    // declaration!)
#if DEBUG_FOR_STMT
    printf("Output the test in the for statement format "
           "newinfo.inConditional() = %s \n",
           newinfo.inConditional() ? "true" : "false");
    curprint(" /* test */ ");
#endif
    SgStatement *test_stmt = for_stmt->get_test();
    ASSERT_not_null(test_stmt);
    // if ( test_stmt != NULL )
    SgUnparse_Info testinfo(info);

    // DQ (11/2/2015): Change the unparsing semantics to treat the test as a
    // statement instead of a conditional.  This should make it processing more
    // uniform and independent of if the test statement is unparsed using either
    // the AST or the token stream. testinfo.set_SkipSemiColon();
    // testinfo.set_inConditional();

    // DQ (11/2/2015): With the new change to not set SkipSemiColon and
    // inConditional unparse info fields, we have to explicitly specify that we
    // want to skip class elaboration as well (SkipClassSpecifier).  See
    // test2015_110.C and other older test codes.  Additionally, we need to make
    // this as a conditional so that it will be unparsed as "type var = value"
    // instead of "type var(value)" in the case of a class constructor
    // initialization call. This is because the syntax required for C++ in a
    // condition is that of a simple declaration which is a part of C++ syntax
    // not directly supported in ROSE for simplicity.
    testinfo.set_SkipClassSpecifier();
    testinfo.set_inConditional();

#if DEBUG_FOR_STMT
    printf("Output the test in the for statement format "
           "testinfo.inConditional() = %s \n",
           testinfo.inConditional() ? "true" : "false");
#endif
    unparseStatement(test_stmt, testinfo);
#if DEBUG_FOR_STMT
    printf("In unparseForStatement(): saved_unparsedPartiallyUsingTokenStream "
           "= %s \n",
           saved_unparsedPartiallyUsingTokenStream ? "true" : "false");
    printf("In unparseForStatement(): test_stmt->isTransformation()           "
           "= %s \n",
           test_stmt->isTransformation() ? "true" : "false");
#endif
    // DQ (4/6/2015): If the test is a transformation, then we have to output
    // the semi-colon directly (see inliner tutorial test).
    if (saved_unparsedPartiallyUsingTokenStream == true &&
        test_stmt->isTransformation() == true) {
      // ROSE_ASSERT(test_stmt->isTransformation() == true);
      // curprint (" /* output semi-colon at end of test */ ");

      // DQ (11/2/2015): Change the unparsing semantics to treat the test as a
      // statement instead of a conditional.  This should make it processing
      // more uniform and independe of if the test statement is unparsed using
      // either the AST or the token stream. curprint (";");
    }

    if (saved_unparsedPartiallyUsingTokenStream == false) {
      // DQ (11/2/2015): Change the unparsing semantics to treat the test as a
      // statement instead of a conditional.  This should make it processing
      // more uniform and independe of if the test statement is unparsed using
      // either the AST or the token stream.  However, we want to add a space
      // after the test and before the increment to make the generated code
      // better looking.
      curprint(" ");
    }

#if DEBUG_FOR_STMT
    printf("In unparseForStmt(): unparse the for loop increment expression \n");
#endif

    // DQ (10/14/2015): If the test was unparsed from the AST then we need to
    // unparse the increment from the AST, but it the test was unparsed as parrt
    // of a partial token stream unparse of the SgForStatement, then we can
    // unparse the increment from the token stream (as a continuation of the use
    // of the token stream in the test). DQ (12/5/2014): Test for if we have
    // unparsed partially using the token stream. If so then we don't want to
    // unparse this syntax, if not then we require this syntax. if
    // (info.unparsedPartiallyUsingTokenStream() == false) if
    // (saved_unparsedPartiallyUsingTokenStream == false) if
    // (saved_unparsedPartiallyUsingTokenStream == false &&
    // test_stmt->isTransformation() == true) if (test_stmt->isTransformation()
    // == true) if (saved_unparsedPartiallyUsingTokenStream == false ||
    // test_stmt->isTransformation() == false)
    // if (saved_unparsedPartiallyUsingTokenStream == false)
    // if (saved_unparsedPartiallyUsingTokenStream == false ||
    // test_stmt->isTransformation() == false) if (test_stmt->isTransformation()
    // == true) if (saved_unparsedPartiallyUsingTokenStream == false &&
    // test_stmt->isTransformation() == true) if (test_stmt->isTransformation()
    // == true) if (saved_unparsedPartiallyUsingTokenStream == false) if (
    // (saved_unparsedPartiallyUsingTokenStream == false ||
    // test_stmt->isTransformation() == false) )
    if (saved_unparsedPartiallyUsingTokenStream == false) {
      // curprint (" /* output semi-colon before increment */ ");

      // DQ (11/2/2015): Change the unparsing semantics to treat the test as a
      // statement instead of a conditional.  This should make it processing
      // more uniform and independe of if the test statement is unparsed using
      // either the AST or the token stream. curprint("; ");
    }

    if (saved_unparsedPartiallyUsingTokenStream == false) {
      SgExpression *increment_expr = for_stmt->get_increment();
      ASSERT_not_null(increment_expr);
      if (increment_expr != NULL)
        unparseExpression(increment_expr, info);
      curprint(") ");

      // DQ (9/24/2020): Adding support to unparse attached pragmas.
      unparsePragmaAttribute(for_stmt);
    } else {
      // DQ (12/15/2014): Note that the increment expression is not a Statement,
      // so it will be unparsed in the token stream of the body (assuming it is
      // unparsed via the token stream).  Note clear how to look ahead to check
      // this or make the unparsing of the increment expression conditional upon
      // this.
      // SgStatement *tmp_stmt = for_stmt->get_for_init_stmt();
      SgStatement *body = for_stmt->get_loop_body();

      // Not yet clear how to handle case where tmp_stmt == NULL.
      ASSERT_not_null(test_stmt);
      ASSERT_not_null(body);

      // If this is compiler generated this this must be handled similarly as to
      // the SgIfStmt with compiler generated body.
      ROSE_ASSERT(body->isCompilerGenerated() == false);

#if DEBUG_FOR_STMT
      curprint("/* unparse increment expression in SgForStatement header */");
#endif
      // DQ (12/16/2014): When a SgBasicBlock has been substituted for the
      // loop_body then there is not associated token stream (see
      // test2014_14.C). In this case it is better to use the start and end of
      // the trailing whitespace subsequence. unparseStatementFromTokenStream
      // (test_stmt, body, e_trailing_whitespace_start,
      // e_token_subsequence_start); unparseStatementFromTokenStream (test_stmt,
      // e_trailing_whitespace_start, e_trailing_whitespace_end, info);
      // curprint("/* syntax from partial token unparse */ )");
      // SgStatement* loopBody = for_stmt->get_loop_body();
      // unparseStatementFromTokenStream (test_stmt, loopBody,
      // e_trailing_whitespace_end, e_leading_whitespace_start);

      // DQ (6/6/2021): Note that this will unparse the original expression
      // using the token stream, if there was a transformation then it should be
      // unparsed from the AST (as an expression).
      // unparseStatementFromTokenStream (test_stmt, body,
      // e_trailing_whitespace_start, e_leading_whitespace_start, info);
      bool unparseOnlyWhitespace = false;
      int start_offset = 0;
      int end_offset = -1;
      unparseStatementFromTokenStream(
          test_stmt, body, e_trailing_whitespace_start,
          e_leading_whitespace_start, info, unparseOnlyWhitespace, start_offset,
          end_offset);
      // DQ (6/6/2021): This is no longer needed (and causes an error in the
      // generated code). curprint(")");
    }

    // Added support to output the header without the body to support the
    // addition of more context in the prefix used with the AST Rewrite
    // Mechanism. if ( (tmp_stmt = for_stmt->get_loop_body()) )

    SgStatement *loopBody = for_stmt->get_loop_body();
    ASSERT_not_null(loopBody);
    // printf ("loopBody = %p         = %s
    // \n",loopBody,loopBody->class_name().c_str()); printf
    // ("info.SkipBasicBlock() = %s \n",info.SkipBasicBlock() ? "true" :
    // "false");

    // if ( (tmp_stmt = for_stmt->get_loop_body()) && !info.SkipBasicBlock())
    if ((loopBody != NULL) && !info.SkipBasicBlock()) {
#if DEBUG_FOR_STMT
      printf("Unparse the for loop body \n");
      curprint("/* Unparse the for loop body */ ");
#endif
      // unparseStatement(tmp_stmt, info);

      unp->cur.format(loopBody, info, FORMAT_BEFORE_NESTED_STATEMENT);
      unparseStatement(loopBody, info);
      unp->cur.format(loopBody, info, FORMAT_AFTER_NESTED_STATEMENT);
#if DEBUG_FOR_STMT
      curprint("/* DONE: Unparse the for loop body */ ");
#endif
    } else {
      // printf ("No for loop body to unparse! \n");
      // curprint ( string("\n/* No for loop body to unparse! */ \n";
      if (!info.SkipSemiColon()) {
        curprint(string(";"));
      }
    }
  }
}

void Unparse_ExprStmt::unparseRangeBasedForStmt(SgStatement *stmt,
                                                SgUnparse_Info &info) {
  // printf ("Unparse range-based for loop \n");
  SgRangeBasedForStatement *for_stmt = isSgRangeBasedForStatement(stmt);
  ASSERT_not_null(for_stmt);

#define DEBUG_RANGE_BASED_FOR_STMT 0

#if DEBUG_RANGE_BASED_FOR_STMT
  bool saved_unparsedPartiallyUsingTokenStream =
      info.unparsedPartiallyUsingTokenStream();

  printf("In unparseRangeBasedForStmt(): "
         "saved_unparsedPartiallyUsingTokenStream = %s \n",
         saved_unparsedPartiallyUsingTokenStream ? "true" : "false");
  curprint("/* Top of unparseForStmt */");
#endif

  // printf ("ERROR: Range-based For statement unparseRangeBasedForStmt() not
  // implemented \n");

  curprint("for ( ");

  SgVariableDeclaration *interator_declaration =
      for_stmt->get_iterator_declaration();
  ASSERT_not_null(interator_declaration);

  SgUnparse_Info ninfo(info);

  // Need to suppress the output of the semicolon in the output of the variable
  // declaration.
  ninfo.set_SkipSemiColon();
  ninfo.set_SkipInitializer();

  unparseStatement(interator_declaration, ninfo);

  curprint(" : ");

  // SgVarRefExp* range_variable = for_stmt->range_variable_reference();
  // ASSERT_not_null(range_variable);
  SgExpression *range_expression = for_stmt->range_expression();
  ASSERT_not_null(range_expression);

  unparseExpression(range_expression, info);

  curprint(" )");

  SgStatement *loopBody = for_stmt->get_loop_body();
  ASSERT_not_null(loopBody);
  // printf ("loopBody = %p         = %s
  // \n",loopBody,loopBody->class_name().c_str()); printf
  // ("info.SkipBasicBlock() = %s \n",info.SkipBasicBlock() ? "true" : "false");

  if ((loopBody != NULL) && !info.SkipBasicBlock()) {
#if DEBUG_RANGE_BASED_FOR_STMT
    printf("Unparse the for loop body \n");
    curprint("/* Unparse the for loop body */ ");
#endif
    unp->cur.format(loopBody, info, FORMAT_BEFORE_NESTED_STATEMENT);
    unparseStatement(loopBody, info);
    unp->cur.format(loopBody, info, FORMAT_AFTER_NESTED_STATEMENT);

#if DEBUG_RANGE_BASED_FOR_STMT
    curprint("/* DONE: Unparse the range-based for loop body */ ");
#endif
  } else {
    // printf ("No range-based for loop body to unparse! \n");
    // curprint ( string("\n/* No range-based for loop body to unparse! */ \n";
    if (!info.SkipSemiColon()) {
      curprint(string(";"));
    }
  }
}

void Unparse_ExprStmt::unparseExceptionSpecification(
    const SgTypePtrList &exceptionSpecifierList, SgUnparse_Info &info) {
  // DQ (6/27/2006): Added support for throw modifier and its exception
  // specification lists

  curprint(string(" throw("));
  if (!exceptionSpecifierList.empty()) {
    SgTypePtrList::const_iterator i = exceptionSpecifierList.begin();
    while (i != exceptionSpecifierList.end()) {
      // Handle class type as a special case to make sure the names are always
      // output (see test2004_91.C). unparseType(*i,info); printf ("Note: Type
      // found in function throw specifier type = %p = %s
      // \n",*i,i->class_name().c_str());

      ASSERT_not_null(*i);
      // DQ (6/2/2011): Added support for name qualification.
      info.set_reference_node_for_qualification(*i);
      ASSERT_not_null(info.get_reference_node_for_qualification());

      unp->u_type->unparseType(*i, info);

      // DQ (6/2/2011): Since we are not using a new SgUnparse_Info object,
      // clear the reference node for name qualification after it has been used.
      info.set_reference_node_for_qualification(NULL);
      i++;
      if (i != exceptionSpecifierList.end())
        curprint(string(","));
    }
  } else {
    // There was no exception specification list of types
  }
  curprint(string(")"));
}

// DQ (11/7/2007): Make this a more general function so that we can use it for
// the unparsing of SgClassDeclaration objects too. void fixupScopeInUnparseInfo
// ( SgUnparse_Info& ninfo , SgFunctionDeclaration* functionDeclaration )
void fixupScopeInUnparseInfo(SgUnparse_Info &ninfo,
                             SgDeclarationStatement *declarationStatement) {
  // DQ (11/3/2007): This resets the current scope stored in the SgUnparse_Info
  // object so that the name qualification will work properly. It used to be
  // that this would be the scope of the caller (which for unparsing the
  // function prototype would be the outer scope (OK), but for function
  // definitions would be the scope of the function definition (VERY BAD).
  // Because of a previous bug (just fixed) in the SgStatement::get_scope()
  // function, the scope of the SgFunctionDefinition would be set to the parent
  // of the SgFunctionDeclaration (structural) instead of the scope of the
  // SgFunctionDeclaration (semantics). It is the perfect example of two bugs
  // working together to be almost always correct :-).  Note that "ninfo" will
  // go out of scope, so we don't have to reset it at the end of this function.
  // Note that that it is FROM this scope that the name qualification is
  // computed, so this is structural, not semantic.

  // DQ (11/9/2007): If we want to force the use of qualified names then don't
  // reset the internal scope (required for new ROSE documentation generator).
  if (ninfo.forceQualifiedNames() == false) {
    SgScopeStatement *currentScope =
        isSgScopeStatement(declarationStatement->get_parent());

    if (currentScope == NULL) {
      // printf ("In fixupScopeInUnparseInfo(): declarationStatement = %p = %s =
      // %s
      // \n",declarationStatement,declarationStatement->class_name().c_str(),SageInterface::get_name(declarationStatement).c_str());
      SgNode *parentOfFunctionDeclaration = declarationStatement->get_parent();
      ASSERT_not_null(parentOfFunctionDeclaration);

      switch (parentOfFunctionDeclaration->variantT()) {
        // This is one way that the funcdecl_stmt->get_parent() can not be a
        // SgScopeStatement (there might be a few more!)
      case V_SgTemplateInstantiationDirectiveStatement: {
        SgTemplateInstantiationDirectiveStatement *directive =
            isSgTemplateInstantiationDirectiveStatement(
                parentOfFunctionDeclaration);
        ASSERT_not_null(directive);
        // currentScope =
        // isSgScopeStatement(funcdecl_stmt->get_parent()->get_parent());
        currentScope = directive->get_scope();
        break;
      }

      case V_SgVariableDeclaration: {
        SgVariableDeclaration *variableDeclaration =
            isSgVariableDeclaration(parentOfFunctionDeclaration);
        ASSERT_not_null(variableDeclaration);
        // currentScope =
        // isSgScopeStatement(funcdecl_stmt->get_parent()->get_parent());
        currentScope = variableDeclaration->get_scope();
        break;
      }

        // DQ (6/19/2012): Added case to support test2012_103.C.
      case V_SgForInitStatement: {
        SgForInitStatement *forInitDeclaration =
            isSgForInitStatement(parentOfFunctionDeclaration);
        ASSERT_not_null(forInitDeclaration);
        // currentScope =
        // isSgScopeStatement(funcdecl_stmt->get_parent()->get_parent());
        currentScope = forInitDeclaration->get_scope();
        ASSERT_not_null(currentScope);
        break;
      }

        // DQ (2/16/2014): The SystemC example (in systemc_tests) demonstrates
        // where this case must be handled. I think it should be the scope of
        // the SgTypedefDeclaration.
      case V_SgTypedefDeclaration: {
        SgTypedefDeclaration *declaration =
            isSgTypedefDeclaration(parentOfFunctionDeclaration);
        ASSERT_not_null(declaration);
        currentScope = declaration->get_scope();
        break;
      }

      case V_SgLambdaExp: {
        // This happens when calling unparseToString on the function declaration
        // associated with a lambda
        printf("WARNING: In fixupScopeInUnparseInfo: Case of a lambda "
               "expression !!!\n");
        SgLambdaExp *lambda = isSgLambdaExp(parentOfFunctionDeclaration);
        ASSERT_not_null(lambda);
        currentScope =
            SageInterface::getEnclosingStatement(lambda)->get_scope();
        break;
      }

      default: {
        printf("Error: default reached in evaluation of function declaration "
               "structural location parentOfFunctionDeclaration = %s \n",
               parentOfFunctionDeclaration->class_name().c_str());
        printf("     declarationStatement = %p = %s = %s \n",
               declarationStatement, declarationStatement->class_name().c_str(),
               SageInterface::get_name(declarationStatement).c_str());
        declarationStatement->get_startOfConstruct()->display(
            "default reached: debug");
        ROSE_ABORT();
      }
      }
    }

    // printf ("In fixupScopeInUnparseInfo(): currentScope = %p = %s
    // \n",currentScope,currentScope->class_name().c_str());
    ASSERT_not_null(currentScope);

    ninfo.set_current_scope(currentScope);
  }

  // printf ("Set current scope (stored in ninfo): currentScope = %p = %s
  // \n",currentScope,currentScope->class_name().c_str());
}

#define DEBUG_unparseFuncDeclStmt 0

void Unparse_ExprStmt::unparseFuncDeclStmt(SgStatement *stmt,
                                           SgUnparse_Info &info) {
#if DEBUG_unparseFuncDeclStmt
  printf("Enter Unparse_ExprStmt::unparseFuncDeclStmt\n");
  printf("  stmt = %p = %s\n", stmt, stmt->class_name().c_str());
#endif
  SgFunctionDeclaration *funcdecl_stmt = isSgFunctionDeclaration(stmt);
  ASSERT_not_null(funcdecl_stmt);

#if DEBUG_unparseFuncDeclStmt
  printf("  ->isFriend         = %s \n",
         funcdecl_stmt->get_declarationModifier().isFriend() ? "true"
                                                             : "false");
  printf("  ->isForward()      = %s \n",
         funcdecl_stmt->isForward() ? "true" : "false");
  printf("  ->get_definition() = %s \n",
         funcdecl_stmt->get_definition() ? "true" : "false");
  printf("info.SkipFunctionDefinition()   = %s \n",
         info.SkipFunctionDefinition() ? "true" : "false");
#endif

#if ENABLE_unparsedPartiallyUsingTokenStream
  // DQ (10/26/2018): We might not need this code now that I have fixed a cut
  // and paste error in the latest debugging of the
  // unparseStatementFromTokenStream() function. DQ (10/25/2018): Test for if we
  // have unparsed partially using the token stream. If so then we don't want to
  // unparse this syntax, if not then we require this syntax.
  bool saved_unparsedPartiallyUsingTokenStream =
      info.unparsedPartiallyUsingTokenStream();
  if (saved_unparsedPartiallyUsingTokenStream &&
      isWithinTransformedDeclarationScope(funcdecl_stmt)) {
    saved_unparsedPartiallyUsingTokenStream = false;
    info.unset_unparsedPartiallyUsingTokenStream();
  }
  if (saved_unparsedPartiallyUsingTokenStream == true) {
    SgFunctionDefinition *function_definition = funcdecl_stmt->get_definition();
    ASSERT_not_null(function_definition);
    SgStatement *function_body = function_definition->get_body();
    ASSERT_not_null(function_body);

    SgFunctionParameterList *function_parameter_list =
        funcdecl_stmt->get_parameterList();
    ASSERT_not_null(function_parameter_list);

    if (function_body != NULL) {
      unparseStatementFromTokenStream(stmt, function_parameter_list,
                                      e_token_subsequence_start,
                                      e_token_subsequence_end, info);
      unparseStatementFromTokenStream(function_definition,
                                      e_leading_whitespace_start,
                                      e_leading_whitespace_end, info);
      SgUnparse_Info ninfo(info);
      unparseStatement(function_body, info);
    } else {
      // We need to handle the case of a function prototype.
      printf("We need to handle the case of a function prototype \n");
      ROSE_ABORT();
    }

    return;
  }
#endif

  // DQ (1/19/2014): Adding support for attributes that must be prefixed to the
  // function declarations (e.g. "__attribute__((regnum(3)))"). It is output
  // here for non-defining declarations, but in unparseFuncDefnStmt() function
  // for the attribute to be associated with the defining declaration.
  // unp->u_sage->printPrefixAttributes(funcdecl_stmt,info);
  if (funcdecl_stmt->isForward() == true) {
    unp->u_sage->printPrefixAttributes(funcdecl_stmt, info);
  }

  SgUnparse_Info ninfo(info);

  fixupScopeInUnparseInfo(ninfo, funcdecl_stmt);
  const bool is_deduction_guide = funcdecl_stmt->get_is_deduction_guide();

  if ((funcdecl_stmt->isForward() == false) &&
      (funcdecl_stmt->get_definition() != NULL) &&
      (info.SkipFunctionDefinition() == false)) {
    unparseStatement(funcdecl_stmt->get_definition(), info);
  } else {
    SgClassDefinition *cdefn = isSgClassDefinition(funcdecl_stmt->get_parent());
    if (cdefn && cdefn->get_declaration()->get_class_type() ==
                     SgClassDeclaration::e_class) {
      ninfo.set_CheckAccess();
    }

    ninfo.set_SkipClassDefinition();
    ninfo.set_SkipEnumDefinition();

    SgStorageModifier &storage =
        funcdecl_stmt->get_declarationModifier().get_storageModifier();
    if (storage.isAsm() == true) {
      curprint("__asm__ ");
    }

    unp->u_sage->printSpecifier(funcdecl_stmt, ninfo);

    unparseAttachedPreprocessingInfo(stmt, info, PreprocessingInfo::inside);
    ninfo.unset_CheckAccess();
    info.set_access_attribute(ninfo.get_access_attribute());

    // get_orig_return_type queries return type from type_syntax(), if
    // available,
    //   and from type() otherwise.
    SgType *rtype = funcdecl_stmt->get_orig_return_type();
    bool use_trailing_return_type_syntax =
        !is_deduction_guide && requiresTrailingReturnTypeSyntax(rtype);

    if (funcdecl_stmt->get_name().getString() == "foo") {
      std::string filename;
      if (Sg_File_Info *fi = funcdecl_stmt->get_file_info()) {
        filename = fi->get_filenameString();
      }
      if (filename.find("test2011_43.C") != std::string::npos) {
      }
    }

    SgUnparse_Info ninfo_for_type(ninfo);
    if (funcdecl_stmt->get_requiresNameQualificationOnReturnType() == true) {
      ninfo_for_type.set_requiresGlobalNameQualification();
    }
    enableInlineDefinitionForFunctionReturnTypeIfNeeded(ninfo_for_type, rtype);

    ninfo_for_type.set_reference_node_for_qualification(funcdecl_stmt);
    ASSERT_not_null(ninfo_for_type.get_reference_node_for_qualification());
    ninfo_for_type.set_declstatement_ptr(funcdecl_stmt);

    ninfo_for_type.set_name_qualification_length(
        funcdecl_stmt->get_name_qualification_length_for_return_type());
    ninfo_for_type.set_global_qualification_required(
        funcdecl_stmt->get_global_qualification_required_for_return_type());
    ninfo_for_type.set_type_elaboration_required(
        funcdecl_stmt->get_type_elaboration_required_for_return_type());

    if (!is_deduction_guide) {
      if (use_trailing_return_type_syntax) {
        curprint("auto ");
      } else {
        ninfo_for_type.set_isTypeFirstPart();
        unp->u_type->unparseType(rtype, ninfo_for_type);
      }
    }

    if (funcdecl_stmt->isForward() == false) {
      unp->u_sage->printAttributes(funcdecl_stmt, info);
    }
    ninfo.set_declstatement_ptr(NULL);
    ninfo.set_declstatement_ptr(funcdecl_stmt);

    if (funcdecl_stmt->get_firstNondefiningDeclaration() != NULL) {
      SgFunctionDeclaration *firstNondefiningFunction = isSgFunctionDeclaration(
          funcdecl_stmt->get_firstNondefiningDeclaration());
      ASSERT_not_null(firstNondefiningFunction);
      ASSERT_not_null(
          firstNondefiningFunction->get_firstNondefiningDeclaration());
      ASSERT_not_null(
          SageInterface::getEnclosingSourceFile(funcdecl_stmt, true));
      ASSERT_not_null(SageInterface::getEnclosingSourceFile(
          firstNondefiningFunction, true));
    }

    unparse_helper(funcdecl_stmt, ninfo);

    ninfo.set_declstatement_ptr(NULL);
    if (is_deduction_guide || use_trailing_return_type_syntax) {
      curprint(" -> ");
      SgUnparse_Info trailing_type_info(ninfo_for_type);
      trailing_type_info.set_isTypeFirstPart();
      unp->u_type->unparseType(rtype, trailing_type_info);
      trailing_type_info.set_isTypeSecondPart();
      unp->u_type->unparseType(rtype, trailing_type_info);
    } else {
      ninfo.set_isTypeSecondPart();
      unp->u_type->unparseType(rtype, ninfo);
    }

    if (funcdecl_stmt->get_declarationModifier().isThrow()) {
      const SgTypePtrList &exceptionSpecifierList =
          funcdecl_stmt->get_exceptionSpecification();
      info.set_reference_node_for_qualification(funcdecl_stmt);
      unparseExceptionSpecification(exceptionSpecifierList, info);
      info.set_reference_node_for_qualification(NULL);
    }

    if (funcdecl_stmt->get_asm_name().empty() == false) {
      curprint(" __asm__ (\"");
      curprint(funcdecl_stmt->get_asm_name());
      curprint(string("\")"));
    }

    unparseTrailingRequiresClauseIfPresent(this, funcdecl_stmt, info);

    const bool is_function_try_block =
        info.SkipFunctionDefinition() && !funcdecl_stmt->isForward() &&
        getFunctionTryStmt(funcdecl_stmt->get_definition()) != NULL;
    if (is_function_try_block) {
      curprint(" try");
    }

    if (funcdecl_stmt->isForward() && !ninfo.SkipSemiColon()) {
      unp->u_sage->printAttributes(funcdecl_stmt, info);
      unp->u_sage->printAttributesForType(funcdecl_stmt, info);

      curprint(";");
    }
  }

  if (info.AddSemiColonAfterDeclaration()) {
    curprint(";");
  }

#if DEBUG_unparseFuncDeclStmt
  printf("Leave Unparse_ExprStmt::unparseFuncDeclStmt\n");
#endif
}

void Unparse_ExprStmt::unparseTemplateFunctionDefnStmt(SgStatement *stmt_,
                                                       SgUnparse_Info &info) {
  SgTemplateFunctionDefinition *stmt = isSgTemplateFunctionDefinition(stmt_);
  assert(stmt != NULL);

  // DQ (10/27/2020): This can't be commented out since it is required for the
  // conditional below.
  // #ifndef NDEBUG
  // SgStatement *declstmt =
  // isSgTemplateFunctionDeclaration(stmt->get_declaration());
  // SgDeclarationStatement *declstmt =
  // isSgTemplateFunctionDeclaration(stmt->get_declaration());
  SgFunctionDeclaration *declstmt =
      isSgTemplateFunctionDeclaration(stmt->get_declaration());
  assert(declstmt != NULL);
  // #endif

  // unparseTemplateFunctionDeclStmt(declstmt, info); // we should not go back
  // to parent declaration and unparse it. bad logic and cause recursion.

  SgSourceFile *sourcefile = info.get_current_source_file();
  // DQ (10/27/2020): Added support to activate unparsing from the AST on a
  // declaration by declaration basis. if (sourcefile != NULL &&
  // sourcefile->get_unparse_template_ast() == true)
  if ((sourcefile != NULL && sourcefile->get_unparse_template_ast() == true) ||
      (declstmt->get_unparse_template_ast() == true)) {
    // Liao, 12/15/2016
    //  We should only unparse the definition, not going back to parent node to
    //  unparse the entire declaration including the header.
    SgFunctionDefinition *funcdefn_stmt = stmt;
    ASSERT_not_null(funcdefn_stmt);

#if OUTPUT_HIDDEN_LIST_DATA
    outputHiddenListData(funcdefn_stmt);
#endif

    // Unparse any comments of directives attached to the
    // SgFunctionParameterList
    ASSERT_not_null(funcdefn_stmt->get_declaration());
    if (funcdefn_stmt->get_declaration()->get_parameterList() != NULL) {
      unparseAttachedPreprocessingInfo(
          funcdefn_stmt->get_declaration()->get_parameterList(), info,
          PreprocessingInfo::before);
    }

    info.set_SkipFunctionDefinition();
    //     SgStatement *declstmt = funcdefn_stmt->get_declaration();

    // DQ (1/19/2014): Adding gnu attribute prefix support.
    ASSERT_not_null(funcdefn_stmt->get_declaration());

    unp->u_sage->printPrefixAttributes(funcdefn_stmt->get_declaration(), info);

    // DQ (3/24/2004): Need to permit SgMemberFunctionDecl and
    // SgTemplateInstantiationMemberFunctionDecl if (declstmt->variant() ==
    // MFUNC_DECL_STMT)

    // DQ (5/8/2004): Any generated specialization needed to use the
    // C++ syntax for explicit specification of specializations.
    // if (isSgTemplateInstantiationMemberFunctionDecl(declstmt) != NULL)
    //      curprint ( string("template<> ";

    // DQ (10/11/2006): As part of new implementation of qualified names we now
    // default to the generation of all qualified names unless they are skipped.
    // info.set_SkipQualifiedNames();

    // DQ (10/15/2006): Mark that we are unparsing a function declaration (or
    // member function declaration) this will help us know when to trim the "::"
    // prefix from the name qualiciation.  The "::" global scope qualifier is
    // not used in function declarations, but is used for function calls.
    info.set_declstatement_ptr(NULL);
    info.set_declstatement_ptr(funcdefn_stmt->get_declaration());

    // curprint ("/* Inside of
    // Unparse_ExprStmt::unparseTemplateFunctionDefnStmt: DONE calling
    // unparseMFuncDeclStmt or unparseFuncDeclStmt */ ");

    // DQ (10/15/2006): Also un-mark that we are unparsing a function
    // declaration (or member function declaration)
    info.set_declstatement_ptr(NULL);

    // DQ (10/11/2006): As part of new implementation of qualified names we now
    // default to the generation of all qualified names unless they are skipped.
    // info.unset_SkipQualifiedNames();

    info.unset_SkipFunctionDefinition();
    SgUnparse_Info ninfo(info);

    // DQ (10/20/2012): Ouput the comments and CPP directives on the function
    // definition. Note must be outside of SkipFunctionDefinition to be output.
    unparseAttachedPreprocessingInfo(funcdefn_stmt, info,
                                     PreprocessingInfo::before);

    // now the body of the function
    if (funcdefn_stmt->get_body()) {
      if (SgTryStmt *try_stmt = getFunctionTryStmt(funcdefn_stmt)) {
        unparseFunctionTryBlock(try_stmt, ninfo);
      } else {
        unparseStatement(funcdefn_stmt->get_body(), ninfo);
      }
    } else {
      curprint("{}");

      // DQ (9/22/2004): I think this is an error!
      printf("Error: Should be an error to not have a function body in the AST "
             "\n");
      ROSE_ABORT();
    }

    // DQ (10/20/2012): Not clear if this is in the correct location (shouldn't
    // it be BEFORE the function body?). Unparse any comments of directives
    // attached to the SgFunctionParameterList
    unparseAttachedPreprocessingInfo(
        funcdefn_stmt->get_declaration()->get_parameterList(), info,
        PreprocessingInfo::after);

    // DQ (10/20/2012): Ouput the comments and CPP directives on the function
    // definition.
    unparseAttachedPreprocessingInfo(funcdefn_stmt, info,
                                     PreprocessingInfo::after);
  }
}

// NOTE: Bug in Sage: No file information provided for FuncDeclStmt.
void Unparse_ExprStmt::unparseFuncDefnStmt(SgStatement *stmt,
                                           SgUnparse_Info &info) {

  SgFunctionDefinition *funcdefn_stmt = isSgFunctionDefinition(stmt);
  ASSERT_not_null(funcdefn_stmt);

#if OUTPUT_HIDDEN_LIST_DATA
  outputHiddenListData(funcdefn_stmt);
#endif

  // Unparse any comments of directives attached to the SgFunctionParameterList
  ASSERT_not_null(funcdefn_stmt->get_declaration());
  if (funcdefn_stmt->get_declaration()->get_parameterList() != NULL) {
    unparseAttachedPreprocessingInfo(
        funcdefn_stmt->get_declaration()->get_parameterList(), info,
        PreprocessingInfo::before);
  }

  info.set_SkipFunctionDefinition();
  SgStatement *declstmt = funcdefn_stmt->get_declaration();
  const std::string trace_func_name =
      isSgFunctionDeclaration(declstmt) != nullptr
          ? isSgFunctionDeclaration(declstmt)->get_name().getString()
          : std::string();
  const bool trace_func = trace_func_name == "__push_heap" ||
                          trace_func_name == "__copy_streambufs_eof" ||
                          trace_func_name == "imbue" ||
                          trace_func_name == "init" ||
                          trace_func_name == "sentry";

  // DQ (1/19/2014): Adding gnu attribute prefix support.
  ASSERT_not_null(funcdefn_stmt->get_declaration());

  unp->u_sage->printPrefixAttributes(funcdefn_stmt->get_declaration(), info);

  // DQ (3/24/2004): Need to permit SgMemberFunctionDecl and
  // SgTemplateInstantiationMemberFunctionDecl if (declstmt->variant() ==
  // MFUNC_DECL_STMT)

  // DQ (5/8/2004): Any generated specialization needed to use the
  // C++ syntax for explicit specification of specializations.
  // if (isSgTemplateInstantiationMemberFunctionDecl(declstmt) != NULL)
  //      curprint ( string("template<> ";

  // DQ (10/11/2006): As part of new implementation of qualified names we now
  // default to the generation of all qualified names unless they are skipped.
  // info.set_SkipQualifiedNames();

  // DQ (10/15/2006): Mark that we are unparsing a function declaration (or
  // member function declaration) this will help us know when to trim the "::"
  // prefix from the name qualiciation.  The "::" global scope qualifier is not
  // used in function declarations, but is used for function calls.
  info.set_declstatement_ptr(NULL);
  info.set_declstatement_ptr(funcdefn_stmt->get_declaration());

  // DQ (12/5/2014): Test for if we have unparsed partially using the token
  // stream. If so then we don't want to unparse this syntax, if not then we
  // require this syntax. if (info.unparsedPartiallyUsingTokenStream() == false)
  bool saved_unparsedPartiallyUsingTokenStream =
      info.unparsedPartiallyUsingTokenStream();
  if (saved_unparsedPartiallyUsingTokenStream &&
      isWithinTransformedDeclarationScope(funcdefn_stmt)) {
    saved_unparsedPartiallyUsingTokenStream = false;
    info.unset_unparsedPartiallyUsingTokenStream();
  }
  if (trace_func) {
    std::cerr << "TRACE funcdef mode def=" << funcdefn_stmt
              << " name=" << trace_func_name << " scopeTransformed="
              << (isWithinTransformedDeclarationScope(funcdefn_stmt) ? 1 : 0)
              << " partial="
              << (saved_unparsedPartiallyUsingTokenStream ? 1 : 0) << " scope="
              << (funcdefn_stmt->get_scope() != nullptr
                      ? funcdefn_stmt->get_scope()->class_name()
                      : std::string("<null>"))
              << " parent="
              << (funcdefn_stmt->get_parent() != nullptr
                      ? funcdefn_stmt->get_parent()->class_name()
                      : std::string("<null>"))
              << std::endl;
  }
  if (saved_unparsedPartiallyUsingTokenStream == false) {
    if (isSgMemberFunctionDeclaration(declstmt)) {
      unparseMFuncDeclStmt(declstmt, info);
    } else {
      unparseFuncDeclStmt(declstmt, info);
    }
  } else {
    // DQ (12/6/2014): Unparse the equivalent tokens instead.

    // unparseStatementFromTokenStream (SgStatement* stmt,
    // token_sequence_position_enum_type e_leading_whitespace_start,
    // token_sequence_position_enum_type e_token_subsequence_start)
    // unparseStatementFromTokenStream (declstmt, e_leading_whitespace_start,
    // e_token_subsequence_start); unparseStatementFromTokenStream (stmt,
    // e_leading_whitespace_start, e_token_subsequence_start);
    // unparseStatementFromTokenStream (declstmt, stmt,
    // e_token_subsequence_start, e_leading_whitespace_end);
    unparseStatementFromTokenStream(declstmt, stmt, e_token_subsequence_start,
                                    e_token_subsequence_start, info);
  }

  // curprint ("/* Inside of Unparse_ExprStmt::unparseFuncDefnStmt: DONE calling
  // unparseMFuncDeclStmt or unparseFuncDeclStmt */ ");

  // DQ (10/15/2006): Also un-mark that we are unparsing a function declaration
  // (or member function declaration)
  info.set_declstatement_ptr(NULL);

  // DQ (10/11/2006): As part of new implementation of qualified names we now
  // default to the generation of all qualified names unless they are skipped.
  // info.unset_SkipQualifiedNames();

  info.unset_SkipFunctionDefinition();
  SgUnparse_Info ninfo(info);

  // DQ (10/20/2012): Ouput the comments and CPP directives on the function
  // definition. Note must be outside of SkipFunctionDefinition to be output.
  unparseAttachedPreprocessingInfo(funcdefn_stmt, info,
                                   PreprocessingInfo::before);

  // now the body of the function
  if (funcdefn_stmt->get_body()) {
    if (SgTryStmt *try_stmt = getFunctionTryStmt(funcdefn_stmt)) {
      unparseFunctionTryBlock(try_stmt, ninfo);
    } else {
      unparseStatement(funcdefn_stmt->get_body(), ninfo);
    }
  } else {
    curprint("{}");

    // DQ (9/22/2004): I think this is an error!
    printf(
        "Error: Should be an error to not have a function body in the AST \n");
    ROSE_ABORT();
  }

  // DQ (10/20/2012): Not clear if this is in the correct location (shouldn't it
  // be BEFORE the function body?). Unparse any comments of directives attached
  // to the SgFunctionParameterList
  unparseAttachedPreprocessingInfo(
      funcdefn_stmt->get_declaration()->get_parameterList(), info,
      PreprocessingInfo::after);

  // DQ (10/20/2012): Ouput the comments and CPP directives on the function
  // definition.
  unparseAttachedPreprocessingInfo(funcdefn_stmt, info,
                                   PreprocessingInfo::after);
}

string Unparse_ExprStmt::trimGlobalScopeQualifier(string qualifiedName) {
  //! DQ (10/12/2006): Support for qualified names (function names can't have
  //! global scope specifier in GNU, or so it seems).

  // DQ (10/11/2006): Now that we use fully qualified names in most places we
  // need this editing to remove the leading global qualifier (once again!).

  // DQ (8/25/2005): This is the case where we previously named the global scope
  // as "::" within name qualification.  This was done to handle test2005_144.C
  // but it broke test2004_80.C So we have moved to an explicit marking of IR
  // nodes using global scope qualification (since it clearly seems to be
  // required). For member functions we need to remove the leading "::" since
  // GNU g++ can't always handle it for member functions
  string s = qualifiedName;
  size_t subStringLocationOfScopeQualifier = s.find("::");
  // printf ("Location of member function substring = %d
  // \n",subStringLocationOfScopeQualifier);
  if (subStringLocationOfScopeQualifier == 0) {
    // printf ("Found global scope qualifier at start of function or member
    // function name qualification \n");
    s.replace(s.find("::"), 2, "");

    // reset the string in scopename!
    qualifiedName = s.c_str();
  }

  return qualifiedName;
}

void Unparse_ExprStmt::unparseReturnType(SgFunctionDeclaration *funcdecl_stmt,
                                         SgType *&rtype,
                                         SgUnparse_Info &ninfo) {
  // DQ (9/7/2014): Refactored this code so we could call it from the template
  // member and non-member function declaration unparse function. Note that we
  // pass a reference to the return type so that we can call unparseType a
  // second time to unparse the second part (not yet refactored, since it is
  // much simpler).

  SgClassDefinition *parent_class =
      isSgClassDefinition(funcdecl_stmt->get_parent());

  // This is a test for if the member function is structurally in the class
  // where it is defined. printf ("parent_class = %p mfuncdecl_stmt->get_scope()
  // = %p \n",parent_class,mfuncdecl_stmt->get_scope());

  // DQ (11/5/2007): This test is not good enough (does not handle case of
  // nested classes and the definition of member function outside of the nested
  // class and inside of another class. if (parent_class)
  if (parent_class == funcdecl_stmt->get_scope()) {
    // JJW 10-23-2007 This member function is declared inside the
    // class, so its name should never be qualified

    // printf ("mfuncdecl_stmt->get_declarationModifier().isFriend() = %s
    // \n",mfuncdecl_stmt->get_declarationModifier().isFriend() ? "true" :
    // "false");
    if (funcdecl_stmt->get_declarationModifier().isFriend() == false) {
      // printf ("Setting SkipQualifiedNames (this is a member function located
      // in its own class) \n");
      ninfo.set_SkipQualifiedNames();
    }
  }

  ninfo.set_SkipClassDefinition();
  ninfo.set_SkipEnumDefinition();

  // DQ (6/10/2007): set the declaration pointer so that the name qualification
  // can see if this is the declaration (so that exceptions to qualification can
  // be tracked).
  ninfo.set_declstatement_ptr(NULL);
  ninfo.set_declstatement_ptr(funcdecl_stmt);

  // if (!(mfuncdecl_stmt->isConstructor() || mfuncdecl_stmt->isDestructor() ||
  // mfuncdecl_stmt->isConversion()))
  if (!(funcdecl_stmt->get_specialFunctionModifier().isConstructor() ||
        funcdecl_stmt->get_specialFunctionModifier().isDestructor() ||
        funcdecl_stmt->get_specialFunctionModifier().isConversion())) {
    rtype = funcdecl_stmt->get_orig_return_type();
    ASSERT_not_null(rtype);

    bool use_trailing_return_type_syntax =
        requiresTrailingReturnTypeSyntax(rtype);
    if (use_trailing_return_type_syntax) {
      curprint("auto ");
    } else {
      ninfo.set_isTypeFirstPart();
      ninfo.set_SkipClassSpecifier();

      SgUnparse_Info ninfo_for_type(ninfo);
      ninfo_for_type.unset_SkipQualifiedNames();
      enableInlineDefinitionForFunctionReturnTypeIfNeeded(ninfo_for_type,
                                                          rtype);

      // DQ (6/10/2007): set the declaration pointer so that the name
      // qualification can see if this is the declaration (so that exceptions to
      // qualification can be tracked).
      ASSERT_not_null(ninfo_for_type.get_declstatement_ptr());

      // DQ (12/20/2006): This is used to specify global qualification
      // separately from the more general name qualification mechanism.  Note
      // that SgVariableDeclarations don't use the
      // requiresGlobalNameQualificationOnType on the SgInitializedNames in
      // their list since the SgVariableDeclaration IR nodes is marked directly.
      // curprint ( string("\n/*
      // funcdecl_stmt->get_requiresNameQualificationOnReturnType() = " +
      // (mfuncdecl_stmt->get_requiresNameQualificationOnReturnType() ? "true" :
      // "false") + " */ \n";
      if (funcdecl_stmt->get_requiresNameQualificationOnReturnType() == true)
      // if (funcdecl_stmt->get_requiresNameQualificationOnReturnType() == true
      // || isSgNonrealType(rtype->stripType()))
      {
        // Output the name qualification for the type in the variable
        // declaration. But we have to do so after any modifiers are output, so
        // in unp->u_type->unparseType(). printf ("In
        // Unparse_ExprStmt::unparseMemberFunctionDeclaration(): This return
        // type requires a global qualifier \n");

        // Note that general qualification of types is separated from the use of
        // globl qualification.
        ninfo_for_type.set_requiresGlobalNameQualification();
      }

      // DQ (5/30/2011): Added support for name qualification.
      ninfo_for_type.set_reference_node_for_qualification(funcdecl_stmt);
      ASSERT_not_null(ninfo_for_type.get_reference_node_for_qualification());

      ninfo_for_type.set_name_qualification_length(
          funcdecl_stmt->get_name_qualification_length_for_return_type());
      ninfo_for_type.set_global_qualification_required(
          funcdecl_stmt->get_global_qualification_required_for_return_type());
      ninfo_for_type.set_type_elaboration_required(
          funcdecl_stmt->get_type_elaboration_required_for_return_type());

      // unp->u_type->unparseType(rtype, ninfo);
      unp->u_type->unparseType(rtype, ninfo_for_type);

      ninfo.unset_SkipClassSpecifier();
    }

    // printf ("In unparser: DONE with NOT a constructor, destructor or
    // conversion operator \n");
  } else {
    // DQ (9/17/2004): What can we assume about the return type of a
    // constructor, destructor, or conversion operator?
    if (funcdecl_stmt->get_orig_return_type() == NULL) {
      printf("funcdecl_stmt->get_orig_return_type() == NULL funcdecl_stmt = %p "
             "= %s = %s \n",
             funcdecl_stmt, funcdecl_stmt->class_name().c_str(),
             funcdecl_stmt->get_name().str());
    }

    ASSERT_not_null(funcdecl_stmt->get_orig_return_type());
    ASSERT_not_null(funcdecl_stmt->get_type()->get_return_type());
  }
}

#define DEBUG_unparseMFuncDeclStmt 0

void Unparse_ExprStmt::unparseMFuncDeclStmt(SgStatement *stmt,
                                            SgUnparse_Info &info) {
  SgMemberFunctionDeclaration *mfuncdecl_stmt =
      isSgMemberFunctionDeclaration(stmt);
  ASSERT_not_null(mfuncdecl_stmt);

  const std::string trace_mfunc_name = mfuncdecl_stmt->get_name().getString();
  const bool trace_mfunc =
      trace_mfunc_name == "operator++" || trace_mfunc_name == "good" ||
      trace_mfunc_name == "imbue" || trace_mfunc_name == "init" ||
      trace_mfunc_name == "sentry";

#if DEBUG_unparseMFuncDeclStmt
  printf("Enter Unparse_ExprStmt::unparseMFuncDeclStmt\n");
  printf("  stmt = %p = %s\n", stmt, stmt->class_name().c_str());
#endif

#if ENABLE_unparsedPartiallyUsingTokenStream
  // DQ (10/26/2018): We might not need this code now that I have fixed a cut
  // and paste error in the latest debugging of the
  // unparseStatementFromTokenStream() function. DQ (10/25/2018): Test for if we
  // have unparsed partially using the token stream. If so then we don't want to
  // unparse this syntax, if not then we require this syntax.
  bool saved_unparsedPartiallyUsingTokenStream =
      info.unparsedPartiallyUsingTokenStream();
  if (saved_unparsedPartiallyUsingTokenStream &&
      isWithinTransformedDeclarationScope(mfuncdecl_stmt)) {
    saved_unparsedPartiallyUsingTokenStream = false;
    info.unset_unparsedPartiallyUsingTokenStream();
  }
  if (trace_mfunc) {
    std::cerr << "TRACE mfunc mode decl=" << mfuncdecl_stmt
              << " name=" << trace_mfunc_name
              << " forward=" << (mfuncdecl_stmt->isForward() ? 1 : 0)
              << " scopeTransformed="
              << (isWithinTransformedDeclarationScope(mfuncdecl_stmt) ? 1 : 0)
              << " partial="
              << (saved_unparsedPartiallyUsingTokenStream ? 1 : 0) << " scope="
              << (mfuncdecl_stmt->get_scope() != nullptr
                      ? mfuncdecl_stmt->get_scope()->class_name()
                      : std::string("<null>"))
              << " parent="
              << (mfuncdecl_stmt->get_parent() != nullptr
                      ? mfuncdecl_stmt->get_parent()->class_name()
                      : std::string("<null>"))
              << std::endl;
  }
  if (saved_unparsedPartiallyUsingTokenStream == true) {
    SgFunctionDefinition *function_definition =
        mfuncdecl_stmt->get_definition();
    ASSERT_not_null(function_definition);
    SgStatement *function_body = function_definition->get_body();
    ASSERT_not_null(function_body);

    SgFunctionParameterList *function_parameter_list =
        mfuncdecl_stmt->get_parameterList();
    ASSERT_not_null(function_parameter_list);

    if (function_body != NULL) {
      // Unparse the tokens from the start of the function declaration to just
      // before the opening "{".

      // DQ (6/5/2021): Note that if the function_definition leading whitespace
      // is not available (e.g. does not exist) then nothing will be output.  So
      // we have to break this up into two steps.
      unparseStatementFromTokenStream(stmt, function_parameter_list,
                                      e_token_subsequence_start,
                                      e_token_subsequence_end, info);

      // DQ (6/5/2021): If there is any leading whitespace it is associated with
      // the function definition, and not the body (SgBasicBlock).
      unparseStatementFromTokenStream(function_definition,
                                      e_leading_whitespace_start,
                                      e_leading_whitespace_end, info);

      SgUnparse_Info ninfo(info);
      unparseStatement(function_body, info);
    } else {
      printf("We need to handle the case of a function prototype \n");
      ROSE_ABORT();
    }
    return;
  }
#endif

  // DQ (12/3/2007): This causes a bug in the output of access level (public,
  // protected, private) because the inforamtion change in ninfo is not
  // propogated to info. DQ (11/3/2007): Moved construction of ninfo to start of
  // function!
  SgUnparse_Info ninfo(info);

  fixupScopeInUnparseInfo(ninfo, mfuncdecl_stmt);

  auto unparse_enclosing_template_headers = [&]() {
    if (mfuncdecl_stmt->get_parent() == mfuncdecl_stmt->get_scope()) {
      return;
    }
    if (isSgTemplateInstantiationDirectiveStatement(
            mfuncdecl_stmt->get_parent()) != NULL) {
      return;
    }
    if (SgTemplateInstantiationMemberFunctionDecl *inst =
            isSgTemplateInstantiationMemberFunctionDecl(mfuncdecl_stmt)) {
      if (inst->isSpecialization()) {
        SgClassDeclaration *assoc_class = isSgClassDeclaration(
            mfuncdecl_stmt->get_associatedClassDeclaration());
        size_t template_id_count =
            count_template_instantiation_class_chain_levels(assoc_class);
        if (template_id_count == 0) {
          return;
        }

        // The declaration-level specialization specifier emitted via
        // printSpecifier contributes one `template<>`. Emit only the remaining
        // headers required by enclosing specialized class scopes.
        size_t extra_headers =
            template_id_count > 0 ? template_id_count - 1 : 0;

        for (size_t i = 0; i < extra_headers; ++i) {
          curprint("template<>");
          curprint("\n");
        }
        return;
      }
    }
    SgClassDeclaration *assoc_class =
        isSgClassDeclaration(mfuncdecl_stmt->get_associatedClassDeclaration());
    if (assoc_class == NULL) {
      return;
    }

    if (SgTemplateInstantiationDecl *assoc_inst =
            isSgTemplateInstantiationDecl(assoc_class)) {
      if (assoc_inst->isSpecialization()) {
        size_t enclosing_template_ids =
            count_enclosing_template_instantiation_levels_for_class_specialization(
                assoc_inst);
        for (size_t i = 0; i < enclosing_template_ids; ++i) {
          curprint("template<>");
          curprint("\n");
        }
        return;
      }
    }

    std::vector<SgTemplateClassDeclaration *> template_chain;
    auto add_template = [&](SgTemplateClassDeclaration *tmpl) {
      if (tmpl == NULL) {
        return;
      }
      for (SgTemplateClassDeclaration *existing : template_chain) {
        if (existing == tmpl) {
          return;
        }
      }
      template_chain.push_back(tmpl);
    };

    for (SgNode *node = assoc_class; node != NULL; node = node->get_parent()) {
      add_template(isSgTemplateClassDeclaration(node));
      if (SgTemplateInstantiationDecl *inst =
              isSgTemplateInstantiationDecl(node)) {
        add_template(
            isSgTemplateClassDeclaration(inst->get_templateDeclaration()));
      }
    }

    for (auto it = template_chain.rbegin(); it != template_chain.rend(); ++it) {
      SgTemplateClassDeclaration *tmpl = *it;
      if (tmpl->get_templateParameters().empty()) {
        continue;
      }
      curprint("template ");
      SgTemplateParameterPtrList tlist = tmpl->get_templateParameters();
      Unparse_ExprStmt::unparseTemplateParameterList(tlist, info, true);
      curprint("\n");
    }
  };

  // Unparse any comments of directives attached to the SgCtorInitializerList
  if (mfuncdecl_stmt->get_CtorInitializerList() != NULL) {
    unparseAttachedPreprocessingInfo(mfuncdecl_stmt->get_CtorInitializerList(),
                                     ninfo, PreprocessingInfo::before);
  }

  auto const &mfuncdecl_mod = mfuncdecl_stmt->get_functionModifier();
  bool isDefaultedOrDeletedMemberFunction =
      mfuncdecl_mod.isMarkedDefault() || mfuncdecl_mod.isMarkedDelete();
  SgFunctionDefinition *mfuncdefn = mfuncdecl_stmt->get_definition();
#if DEBUG_unparseMFuncDeclStmt
  printf("  mfuncdecl_stmt->isForward()        = %s\n",
         mfuncdecl_stmt->isForward() ? "true" : "false");
  printf("  info.SkipFunctionDefinition()      = %s\n",
         info.SkipFunctionDefinition() ? "true" : "false");
  printf("  isDefaultedOrDeletedMemberFunction = %s\n",
         isDefaultedOrDeletedMemberFunction ? "true" : "false");
  printf("  mfuncdecl_stmt->get_definition()   = %p = %s\n", mfuncdefn,
         mfuncdefn ? mfuncdefn->class_name().c_str() : "");
#endif

  // DQ (4/13/2019): If this is a defaulted constructor, then we don't want to
  // unparse the body, so we want to treat it the same as a forward declaration.
  if (!mfuncdecl_stmt->isForward() && mfuncdefn &&
      !ninfo.SkipFunctionDefinition() &&
      isDefaultedOrDeletedMemberFunction == false) {
    // Class-member access control applies to the declaration itself, not to
    // local declarations nested inside the function body.
    SgUnparse_Info body_info(info);
    body_info.unset_CheckAccess();
    unparseStatement(mfuncdecl_stmt->get_definition(), body_info);
  } else {
    ASSERT_not_null(mfuncdecl_stmt->get_parent());
    // Access-specifier emission must be recomputed for each declaration.
    // Otherwise a prior in-class declaration can leak `CheckAccess` into
    // out-of-class member declarations and print invalid `public:`/`private:`.
    info.unset_CheckAccess();
    SgClassDefinition *parent_class =
        isSgClassDefinition(mfuncdecl_stmt->get_parent());
    if (parent_class &&
        parent_class->get_declaration()->get_class_type() ==
            SgClassDeclaration::e_class &&
        !info.skipCheckAccess()) {
      info.set_CheckAccess();
    }

    unparse_enclosing_template_headers();

    unp->u_sage->printSpecifier1(mfuncdecl_stmt, info);

    unp->u_sage->printSpecifier2(mfuncdecl_stmt, info);
    info.unset_CheckAccess();

    SgType *rtype = NULL;
    unparseReturnType(mfuncdecl_stmt, rtype, ninfo);
    ASSERT_not_null(mfuncdecl_stmt);

    ninfo.set_name_qualification_length(
        mfuncdecl_stmt->get_name_qualification_length());
    ninfo.set_global_qualification_required(
        mfuncdecl_stmt->get_global_qualification_required());

    SgName nameQualifier = mfuncdecl_stmt->get_qualified_name_prefix();
    curprint(nameQualifier.str());

    // DQ (4/2/2018): Adding support for alternative and more
    // sophisticated handling of the function name (e.g. with template
    // arguments correctly qualified, etc.).
    if (isSgTemplateInstantiationMemberFunctionDecl(mfuncdecl_stmt) != NULL) {
      unp->u_exprStmt->unparseTemplateMemberFunctionName(
          isSgTemplateInstantiationMemberFunctionDecl(mfuncdecl_stmt), ninfo);
    } else {
      curprint(mfuncdecl_stmt->get_name().str());
    }

    SgUnparse_Info ninfo2(info);
    ninfo2.set_SkipClassDefinition();
    ninfo2.set_SkipEnumDefinition();
    ninfo2.set_inArgList();
    ninfo2.set_declstatement_ptr(NULL);
    ninfo2.set_declstatement_ptr(mfuncdecl_stmt);

    curprint(string("("));
    unparseFunctionArgs(mfuncdecl_stmt, ninfo2);
    curprint(string(")"));

    if (rtype != NULL) {
      if (requiresTrailingReturnTypeSyntax(rtype)) {
        curprint(" -> ");
        SgUnparse_Info trailing_type_info(ninfo);
        trailing_type_info.set_isTypeFirstPart();
        unp->u_type->unparseType(rtype, trailing_type_info);
        trailing_type_info.set_isTypeSecondPart();
        unp->u_type->unparseType(rtype, trailing_type_info);
      } else {
        SgUnparse_Info ninfo3(ninfo);
        ninfo3.set_isTypeSecondPart();
        unp->u_type->unparseType(rtype, ninfo3);
      }
    }

    unparseTrailingFunctionModifiers(mfuncdecl_stmt, info);

    const bool is_function_try_block =
        info.SkipFunctionDefinition() && !mfuncdecl_stmt->isForward() &&
        getFunctionTryStmt(mfuncdecl_stmt->get_definition()) != NULL;
    if (is_function_try_block) {
      curprint(" try");
    }

    const SgInitializedNamePtrList *ctor_inits_ptr =
        &mfuncdecl_stmt->get_ctors();
    if (ctor_inits_ptr->empty()) {
      if (SgMemberFunctionDeclaration *def_decl = isSgMemberFunctionDeclaration(
              mfuncdecl_stmt->get_definingDeclaration())) {
        if (def_decl != mfuncdecl_stmt && !def_decl->get_ctors().empty()) {
          ctor_inits_ptr = &def_decl->get_ctors();
        }
      }
    }
    auto const &ctor_inits = *ctor_inits_ptr;
    if ((mfuncdecl_stmt->isForward() && !info.SkipSemiColon()) ||
        isDefaultedOrDeletedMemberFunction) {
      curprint(";");

    } else if (!ctor_inits.empty()) {
      auto it_ctor_init = ctor_inits.begin();
      auto const first = it_ctor_init;
#if DEBUG_unparseMFuncDeclStmt
      printf("  Preinitialization list:\n");
#endif
      curprint(" : ");
      while (it_ctor_init != ctor_inits.end()) {
        SgInitializedName *ctor_init = *it_ctor_init;
        ASSERT_not_null(ctor_init);
        if (it_ctor_init != first) {
          curprint(", ");
        }
        it_ctor_init++;

        unparseAttachedPreprocessingInfo(ctor_init, info,
                                         PreprocessingInfo::before);

        SgName nameQualifier = ctor_init->get_qualified_name_prefix();
#if DEBUG_unparseMFuncDeclStmt
        printf("   - element name = %s nameQualifier = %s \n",
               ctor_init->get_name().str(),
               nameQualifier.is_null() ? "NULL" : nameQualifier.str());
#endif
        unparseCtorPreinitializerDesignator(this, unp, ctor_init, ninfo2);

        SgExpression *initializer = ctor_init->get_initializer();
        if (initializer != NULL) {
          // not used: SgAggregateInitializer   * aggr_init =
          // isSgAggregateInitializer(initializer);
          SgConstructorInitializer *ctor_init =
              isSgConstructorInitializer(initializer);
          bool output_parenthesis = ctor_init == nullptr;

#if DEBUG_unparseMFuncDeclStmt
          bool compiler_generated =
              initializer->get_startOfConstruct()->isCompilerGenerated();
          printf("     initializer = %p = %s\n", initializer,
                 initializer->class_name().c_str());
          printf("     output_parenthesis = %s\n",
                 output_parenthesis ? "true" : "false");
          printf("     compiler_generated = %s\n",
                 compiler_generated ? "true" : "false");
#endif
          if (output_parenthesis)
            curprint(string("("));

          info.set_reference_node_for_qualification(initializer);
          unparseExpression(initializer, ninfo2);
          info.set_reference_node_for_qualification(NULL);

          if (output_parenthesis)
            curprint(string(")"));
        }
      }
    }
  }

  // DQ (1/23/03) Added option to support rewrite mechanism (generation of
  // declarations)
  if (info.AddSemiColonAfterDeclaration()) {
    curprint(string(";"));
  }

  // Unparse any comments of directives attached to the SgCtorInitializerList
  if (mfuncdecl_stmt->get_CtorInitializerList() != NULL) {
    unparseAttachedPreprocessingInfo(mfuncdecl_stmt->get_CtorInitializerList(),
                                     info, PreprocessingInfo::after);
  }

#if DEBUG_unparseMFuncDeclStmt
  printf("Leaving Unparse_ExprStmt::unparseMFuncDeclStmt(stmt = %p = %s) \n",
         stmt, stmt->class_name().c_str());
#endif
}

void Unparse_ExprStmt::unparseTrailingFunctionModifiers(
    SgMemberFunctionDeclaration *mfuncdecl_stmt, SgUnparse_Info &info) {
  // DQ (9/9/2014): Refactored support for function modifiers.
  bool outputRestrictKeyword = false;
  SgMemberFunctionType *mftype =
      isSgMemberFunctionType(mfuncdecl_stmt->get_type());

  // DQ (9/9/2014): Note this was using info where it was refactored from and
  // ninfo is passed to this function.
  if (!info.SkipFunctionQualifier() && mftype) {
    if (mftype->isConstFunc()) {
      curprint(" const");
    }
    if (mftype->isVolatileFunc()) {
      curprint(" volatile");
    }

    // DQ (12/11/2012): Added support for restrict (we want this to be more
    // uniform with "const" and "volatile" modifier handling).
    if (mftype->isRestrictFunc()) {
      outputRestrictKeyword = true;

      // DQ (12/11/2012): Make sure that this way of specifing the restrict
      // keyword is set.
      ROSE_ASSERT(mfuncdecl_stmt->get_declarationModifier()
                      .get_typeModifier()
                      .isRestrict() == true);

      // curprint ( string(" restrict"));
    }

    // DQ (1/11/2020): Adding support for lvalue reference member function
    // modifiers.
    if (mftype->isLvalueReferenceFunc()) {
      curprint(" &");
    }

    // DQ (1/11/2020): Adding support for rvalue reference member function
    // modifiers.
    if (mftype->isRvalueReferenceFunc()) {
      curprint(" &&");
    }
  }

  // DQ (12/11/2012): Avoid redundant output of the restrict keyword.
  // DQ (4/28/2004): Added support for restrict modifier
  // if
  // (mfuncdecl_stmt->get_declarationModifier().get_typeModifier().isRestrict())
  if (mfuncdecl_stmt->get_declarationModifier()
          .get_typeModifier()
          .isRestrict() &&
      (outputRestrictKeyword == false)) {
    outputRestrictKeyword = true;
    // DQ (12/11/2012): Error checking.
    if (mftype != NULL) {
      ROSE_ASSERT(mftype->isRestrictFunc() == true);
    }

    // curprint ( string(" restrict"));
  }

  // DQ (12/11/2012): We have two ways of setting the specification of the
  // restrict keyword, but we only want to output the keyword once. This make
  // this code less sensative to which way it is specified and enforces that
  // both ways are set. At the moment there are two ways that a member function
  // is marked as restrict:
  //    1) Via it's function type modifier (const-volatile modifier)
  //    2) The declaration modifier's const-volatile modifier.
  // It does not appear that the "restrict" keyword modifies the type of the
  // function (g++ does not allow overloading on restrict, for example). Thus if
  // it is not a part of the type then it should be a part of the declaration
  // modifier and not in the SgMemberFunctionType. So maybe we should remove it
  // from the SgMemberFunctionType?  I am not clear on this design point at
  // present, so we have forced both to be set consistantly (and this is handled
  // in the SageBuilder interface), plus a consistancy test in the AST
  // consistancy tests. The reason it is in the type modifier held by the
  // declaration modifier is because it is not a prat of the function type
  // (formally). But the reason it is a part of the type modifier is because it
  // is used for function parameter types.  This design point is less than
  // elegant and I'm not clear on what would make this simpler.  For the moment
  // we have focused on making it consistant across the two ways it can be
  // represented (and fixing the SageBuilder Interface to set it consistantly).
  if (outputRestrictKeyword == true) {
    curprint(Unparse_Type::unparseRestrictKeyword());
  }

  // DQ (4/28/2004): Added support for throw modifier
  if (mfuncdecl_stmt->get_declarationModifier().isThrow()) {
    // Unparse SgThrow
    // unparseThrowExp(mfuncdecl_stmt->get_throwExpression,info);
    // printf ("Incomplete implementation of throw specifier on function \n");
    // curprint ( string(" throw( /* from unparseTrailingFunctionModifiers()
    // type list output not implemented */ )";
    const SgTypePtrList &exceptionSpecifierList =
        mfuncdecl_stmt->get_exceptionSpecification();
    // unparseExceptionSpecification(exceptionSpecifierList,ninfo);
    unparseExceptionSpecification(exceptionSpecifierList, info);
  }

  if (SgExpression *requires_clause =
          mfuncdecl_stmt->get_trailingRequiresClause()) {
    SgUnparse_Info rinfo(info);
    rinfo.set_SkipClassDefinition();
    rinfo.set_SkipEnumDefinition();
    curprint(" requires ");
    unparseRequiresClauseExpression(this, requires_clause, rinfo);
  }

  // if (mfuncdecl_stmt->isPure())
  if (mfuncdecl_stmt->get_functionModifier().isPureVirtual()) {
    // DQ (1/22/2013): Supress the output of the pure virtual syntax if this is
    // the defining declaration (see test2013_26.C). curprint ( string(" = 0"));
    if (mfuncdecl_stmt != mfuncdecl_stmt->get_definingDeclaration()) {
      curprint(" = 0");
    }
  }

  // DQ (7/9/2022): This can only be output for member function declarations
  // defined in the class. DQ (8/11/2014): Added support for final keyword
  // unparsing.
  if (mfuncdecl_stmt->get_declarationModifier().isFinal() == true) {
    // DQ (2/12/2019): Testing, final can't be used on prototypes (I think).
    // curprint(" /* output from test 1 */ ");
    // curprint(" final");
    // DQ (7/10/2022): "final" is not a keyword, but it can only be used like a
    // keyword with member function declarations inside of the associated class
    // definition. curprint(" override");
    SgClassDefinition *parentClassDefinition =
        isSgClassDefinition(mfuncdecl_stmt->get_parent());
    if (parentClassDefinition != NULL &&
        mfuncdecl_stmt->get_scope() == parentClassDefinition) {
      curprint(" final");
    }
  }

  // DQ (7/9/2022): This can only be output for member function declarations
  // defined in the class. DQ (8/11/2014): Added support for final keyword
  // unparsing.
  if (mfuncdecl_stmt->get_declarationModifier().isOverride() == true) {
    // DQ (7/10/2022): "override" is not a keyword, but it can only be used like
    // a keyword with member function declarations inside of the associated
    // class definition. curprint(" override");
    SgClassDefinition *parentClassDefinition =
        isSgClassDefinition(mfuncdecl_stmt->get_parent());
    if (parentClassDefinition != NULL &&
        mfuncdecl_stmt->get_scope() == parentClassDefinition) {
      curprint(" override");
    }
  }

  // DQ (4/13/2019): Added support for default keyword unparsing.
  if (mfuncdecl_stmt->get_functionModifier().isMarkedDefault() == true) {
    curprint(" = default");
  }

  // DQ (4/13/2019): Added support for delete keyword unparsing.
  if (mfuncdecl_stmt->get_functionModifier().isMarkedDelete() == true) {
    curprint(" = delete");
  }
}

void Unparse_ExprStmt::unparseVarDefnStmt(SgStatement *stmt,
                                          SgUnparse_Info &info) {
  SgVariableDefinition *vardefn_stmt = isSgVariableDefinition(stmt);
  ASSERT_not_null(vardefn_stmt);

  // DQ: (9/17/2003)
  // Although I have not seen it in any of our tests of ROSE the
  // SgVariableDefinition does appear to be used in the declaration of bit
  // fields!  Note the comment at the end of the unparseVarDeclStmt() function
  // where the bit field is unparsed! Though it appears that the
  // unparseVarDefnStmt is not required to the unparsing of the bit field, so
  // this function is never called!

  // DQ (2/3/2007): However, for the ODR check in the AST merge we require
  // something to be generated for everything that could be shared.  So we
  // should unparse something, perhaps the variable declaration?

  // DQ (1/20/2014): This has been changed to be a SgValueExp (required).  Plus
  // as a generated value expression we include the expression from which the
  // value was generated.  This is important where this is a constant expression
  // generated from sizes of machine dependent types. SgUnsignedLongVal
  // *bitfield = vardefn_stmt->get_bitfield();
  SgExpression *bitfield = vardefn_stmt->get_bitfield();
  if (bitfield != NULL) {
    curprint(string(":"));
    unparseExpression(bitfield, info);
  }
}

void Unparse_ExprStmt::initializeDeclarationsFromParent(
    SgDeclarationStatement *declarationStatement, SgClassDefinition *&cdefn,
    SgNamespaceDefinitionStatement *&namespaceDefn, int /*debugSupport*/) {
  // DQ (11/18/2004): Now that we store the scope explicitly we don't have to
  // interprete the parent pointer!
  ASSERT_not_null(declarationStatement);
  SgScopeStatement *parentScope = declarationStatement->get_scope();
  ASSERT_not_null(parentScope);

  cdefn = isSgClassDefinition(parentScope);
  namespaceDefn = isSgNamespaceDefinitionStatement(parentScope);
}

void Unparse_ExprStmt::unparseClassDeclStmt(SgStatement *stmt,
                                            SgUnparse_Info &info) {
  SgClassDeclaration *classdecl_stmt = isSgClassDeclaration(stmt);
  ASSERT_not_null(classdecl_stmt);

  const std::string trace_class_name = classdecl_stmt->get_name().getString();
  const bool trace_class_decl =
      trace_class_name == "ios_base" || trace_class_name == "__gconv_info" ||
      trace_class_name == "codecvt_base" || trace_class_name == "_Deque_impl" ||
      trace_class_name == "PP_entry" || trace_class_name == "Frame" ||
      trace_class_name == "Record";
  if (trace_class_decl) {
    std::cerr << "TRACE unparseClassDeclStmt decl=" << classdecl_stmt
              << " forward=" << (classdecl_stmt->isForward() ? 1 : 0)
              << " hasDef="
              << (classdecl_stmt->get_definition() != NULL ? 1 : 0)
              << " skipClassDef=" << (info.SkipClassDefinition() ? 1 : 0)
              << " transformed=" << (classdecl_stmt->isTransformation() ? 1 : 0)
              << " parent="
              << (classdecl_stmt->get_parent() != NULL
                      ? classdecl_stmt->get_parent()->class_name()
                      : std::string("<null>"))
              << " scope="
              << (classdecl_stmt->get_scope() != NULL
                      ? classdecl_stmt->get_scope()->class_name()
                      : std::string("<null>"))
              << std::endl;
  }

  if (!info.inArgList() &&
      info.get_reference_node_for_qualification() == NULL &&
      classdecl_stmt->attributeExists(
          kInlineTypeOperandStandaloneSuppressAttribute)) {
    return;
  }

#define DEBUG_UNPARSE_CLASS_DECLARATION 0

#if DEBUG_USING_CURPRINT
  curprint("/* Inside of Unparse_ExprStmt::unparseClassDeclStmt() */ \n");
#endif

  // info.display("Inside of unparseClassDeclStmt");

#if DEBUG_UNPARSE_CLASS_DECLARATION
  printf("At top of unparseClassDeclStmt name = %s \n",
         classdecl_stmt->get_name().str());
#endif
#if DEBUG_UNPARSE_CLASS_DECLARATION
  printf("In Unparse_ExprStmt::unparseClassDeclStmt(): classdecl_stmt = %p "
         "isForward() = %s info.SkipClassDefinition() = %s name = %s \n",
         classdecl_stmt,
         (classdecl_stmt->isForward() == true) ? "true" : "false",
         (info.SkipClassDefinition() == true) ? "true" : "false",
         classdecl_stmt->get_name().str());
#endif

  // DQ (6/2/2021): Adding support for partial token sequence unparsing.
  SgClassDefinition *class_definition = classdecl_stmt->get_definition();
  bool saved_unparsedPartiallyUsingTokenStream =
      info.unparsedPartiallyUsingTokenStream();
  if (saved_unparsedPartiallyUsingTokenStream &&
      (classdecl_stmt->isTransformation() ||
       (class_definition != NULL && class_definition->isTransformation()))) {
    // Partial token-sequence mode can be inherited from an enclosing frontier
    // statement. Reused/transformed class declarations must unparse from the
    // AST instead; otherwise the nested class-declaration path consumes the
    // token-fragment mode directly and can degrade a defining declaration back
    // into a forward declaration.
    saved_unparsedPartiallyUsingTokenStream = false;
  }
  if (trace_class_decl) {
    std::cerr << "TRACE unparseClassDeclStmt mode decl=" << classdecl_stmt
              << " partialTokens="
              << (saved_unparsedPartiallyUsingTokenStream ? 1 : 0)
              << " declTrans=" << (classdecl_stmt->isTransformation() ? 1 : 0)
              << " defTrans="
              << (class_definition != NULL &&
                          class_definition->isTransformation()
                      ? 1
                      : 0)
              << " skipBasicBlock=" << (info.SkipBasicBlock() ? 1 : 0)
              << std::endl;
  }
  if (saved_unparsedPartiallyUsingTokenStream == true) {
    // unparseStatementFromTokenStream (stmt, e_token_subsequence_start,
    // e_token_subsequence_start); unparseStatementFromTokenStream (stmt,
    // e_token_subsequence_start, e_token_subsequence_end);
    ASSERT_not_null(class_definition);

    // unparseStatementFromTokenStream (stmt, function_body,
    // e_token_subsequence_start, e_token_subsequence_start, info);
    // unparseStatementFromTokenStream (stmt, function_body,
    // e_token_subsequence_start, e_leading_whitespace_end, info);
    // unparseStatementFromTokenStream (stmt, function_definition,
    // e_token_subsequence_start, e_leading_whitespace_end, info);

    if (class_definition != NULL) {
      // Unparse the tokens from the start of the function declaration to just
      // befor the opening "{". unparseStatementFromTokenStream (stmt,
      // function_body, e_token_subsequence_start, e_token_subsequence_start,
      // info); unparseStatementFromTokenStream (stmt, function_body,
      // e_token_subsequence_start, e_leading_whitespace_end, info);
      // unparseStatementFromTokenStream (stmt, function_definition,
      // e_token_subsequence_start, e_leading_whitespace_end, info);
      // I think that we may need to tigger the output of the opening "{" here,
      // and then loop over the declarations in the class definition explicitly.
      // unparseStatementFromTokenStream (stmt, class_definition,
      // e_token_subsequence_start, e_leading_whitespace_end, info);
      unparseStatementFromTokenStream(stmt, class_definition,
                                      e_token_subsequence_start,
                                      e_token_subsequence_start, info);

      SgUnparse_Info ninfo(info);
      // unparseStatement(class_definition, info);

      SgDeclarationStatementPtrList::iterator pp =
          class_definition->get_members().begin();

      while (pp != class_definition->get_members().end()) {
        unparseStatement((*pp), ninfo);

        SgStatement *previousStatement = *pp;

        pp++;

        // DQ (6/4/2021): Test for the last statement, and unparse it's trailing
        // whitespace (if it is available).
        if (pp == class_definition->get_members().end()) {
          ROSE_ASSERT(previousStatement != NULL);
          unparseStatementFromTokenStream(previousStatement,
                                          e_trailing_whitespace_start,
                                          e_trailing_whitespace_end, info);
        }
      }
      // Unparse the tokens from the start of the function declaration to just
      // befor the opening "{". unparseStatementFromTokenStream (stmt,
      // function_body, e_token_subsequence_start, e_token_subsequence_start,
      // info); unparseStatementFromTokenStream (stmt, function_body,
      // e_token_subsequence_start, e_leading_whitespace_end, info);
      // unparseStatementFromTokenStream (stmt, function_definition,
      // e_token_subsequence_start, e_leading_whitespace_end, info);
      // unparseStatementFromTokenStream (stmt, class_definition,
      // e_token_subsequence_end, e_leading_whitespace_end, info);
      unparseStatementFromTokenStream(class_definition, stmt,
                                      e_token_subsequence_end,
                                      e_token_subsequence_end, info);
    } else {
      // We need to handle the case of a function prototype.
      printf("We need to handle the case of a class declaration prototype that "
             "has been modified in partial way (not clear that this is common) "
             "\n");
      printf("Exiting as a test! \n");
      ROSE_ABORT();
    }
  }

  if (saved_unparsedPartiallyUsingTokenStream == false) {
    // DQ (11/7/2007): Fixup the SgUnparse_Info object to store the correct
    // scope.
    SgUnparse_Info class_info(info);
    fixupScopeInUnparseInfo(class_info, classdecl_stmt);

    auto unparse_enclosing_template_headers = [&]() {
      if (classdecl_stmt->get_parent() == classdecl_stmt->get_scope()) {
        return;
      }

      if (SgTemplateInstantiationDecl *inst_decl =
              isSgTemplateInstantiationDecl(classdecl_stmt)) {
        if (inst_decl->isSpecialization()) {
          size_t enclosing_template_ids =
              count_enclosing_template_instantiation_levels_for_class_specialization(
                  inst_decl);
          for (size_t i = 0; i < enclosing_template_ids; ++i) {
            curprint("template<>");
            curprint("\n");
          }
          return;
        }
      }

      std::vector<SgTemplateClassDeclaration *> template_chain;
      auto add_template = [&](SgTemplateClassDeclaration *tmpl) {
        if (tmpl == NULL) {
          return;
        }
        for (SgTemplateClassDeclaration *existing : template_chain) {
          if (existing == tmpl) {
            return;
          }
        }
        template_chain.push_back(tmpl);
      };

      std::set<SgScopeStatement *> visited_scopes;
      for (SgScopeStatement *scope = classdecl_stmt->get_scope();
           scope != NULL && visited_scopes.insert(scope).second;
           scope = scope->get_scope()) {
        if (SgClassDefinition *class_def = isSgClassDefinition(scope)) {
          add_template(
              isSgTemplateClassDeclaration(class_def->get_declaration()));
          continue;
        }
        if (SgTemplateClassDefinition *template_def =
                isSgTemplateClassDefinition(scope)) {
          add_template(template_def->get_declaration());
          continue;
        }
        if (SgTemplateInstantiationDefn *inst_def =
                isSgTemplateInstantiationDefn(scope)) {
          if (SgTemplateInstantiationDecl *inst_decl =
                  isSgTemplateInstantiationDecl(inst_def->get_declaration())) {
            add_template(isSgTemplateClassDeclaration(
                inst_decl->get_templateDeclaration()));
          }
        }
      }

      for (auto it = template_chain.rbegin(); it != template_chain.rend();
           ++it) {
        SgTemplateClassDeclaration *tmpl = *it;
        if (tmpl->get_templateParameters().empty()) {
          continue;
        }
        curprint("template ");
        SgTemplateParameterPtrList params = tmpl->get_templateParameters();
        Unparse_ExprStmt::unparseTemplateParameterList(params, info, true);
        curprint("\n");
      }
    };

    SgTemplateInstantiationDecl *inst_decl =
        isSgTemplateInstantiationDecl(classdecl_stmt);
    bool needs_enclosing_template_headers =
        !info.SkipClassDefinition() &&
        classdecl_stmt->get_definition() != NULL &&
        classdecl_stmt->get_parent() != classdecl_stmt->get_scope() &&
        isSgTemplateClassDeclaration(classdecl_stmt) == NULL &&
        inst_decl == NULL;
    bool needs_specialization_enclosing_headers =
        !info.SkipClassDefinition() && inst_decl != NULL &&
        inst_decl->isSpecialization() &&
        classdecl_stmt->get_parent() != classdecl_stmt->get_scope();
    if (needs_enclosing_template_headers ||
        needs_specialization_enclosing_headers) {
      unparse_enclosing_template_headers();
    }

    if (!classdecl_stmt->isForward() && classdecl_stmt->get_definition() &&
        !info.SkipClassDefinition()) {
      if (trace_class_decl) {
        std::cerr << "TRACE unparseClassDeclStmt recurse-def decl="
                  << classdecl_stmt << " def=" << class_definition
                  << " nestedPartial="
                  << (class_info.unparsedPartiallyUsingTokenStream() ? 1 : 0)
                  << std::endl;
      }
      SgUnparse_Info ninfox(class_info);
      ninfox.unset_SkipSemiColon();
      // Class definition emission is controlled by SkipClassDefinition, not by
      // SkipBasicBlock from an outer statement/token-stream context. If a
      // class declaration is selected for full AST unparsing, leaking
      // SkipBasicBlock here collapses the defining declaration into a forward
      // declaration by suppressing the class body inside unparseClassDefnStmt.
      if (ninfox.SkipBasicBlock()) {
        ninfox.unset_SkipBasicBlock();
      }
      if (ninfox.unparsedPartiallyUsingTokenStream() &&
          (classdecl_stmt->isTransformation() ||
           (class_definition != NULL &&
            class_definition->isTransformation()))) {
        ninfox.unset_unparsedPartiallyUsingTokenStream();
      }

      // DQ (6/13/2007): Set to null before resetting to non-null value
      ninfox.set_declstatement_ptr(NULL);
      ninfox.set_declstatement_ptr(classdecl_stmt);

      // printf ("Calling unparseStatement(classdecl_stmt->get_definition(),
      // ninfox); for %s \n",classdecl_stmt->get_name().str());
      unparseStatement(classdecl_stmt->get_definition(), ninfox);

      if (!info.SkipSemiColon()) {
        curprint(";");
      }
    } else {
      if (!info.inEmbeddedDecl()) {
        SgUnparse_Info ninfo(class_info);
        if (classdecl_stmt->get_parent() == NULL) {
          printf("classdecl_stmt->isForward() = %s \n",
                 (classdecl_stmt->isForward() == true) ? "true" : "false");
        }

        // DQ (5/20/2006): This is false within "stdio.h"
        if (classdecl_stmt->get_parent() == NULL) {
          classdecl_stmt->get_file_info()->display(
              "In Unparse_ExprStmt::unparseClassDeclStmt(): "
              "classdecl_stmt->get_parent() == NULL");
        }
        // ASSERT_not_null(classdecl_stmt->get_parent());
        SgClassDefinition *cdefn =
            isSgClassDefinition(classdecl_stmt->get_parent());

        if (cdefn && cdefn->get_declaration()->get_class_type() ==
                         SgClassDeclaration::e_class) {
          ninfo.set_CheckAccess();
        }

        // DQ (8/19/2004): Removed functions using old attribute mechanism (old
        // CC++ mechanism) printf ("Commented out
        // get_suppress_global(classdecl_stmt) \n"); if
        // (get_suppress_global(classdecl_stmt))
        //      ninfo.set_SkipGlobal(); //attributes.h
        // printDebugInfo("entering unp->u_sage->printSpecifier", true);
        unp->u_sage->printSpecifier(classdecl_stmt, ninfo);
        info.set_access_attribute(ninfo.get_access_attribute());
      }

      info.unset_inEmbeddedDecl();
      if (!info.SkipClassSpecifier()) {
        const bool use_class_keyword_for_template_instantiation =
            isSgTemplateInstantiationDecl(classdecl_stmt) != NULL &&
            classdecl_stmt->get_definition() == NULL &&
            (classdecl_stmt->get_parent() == classdecl_stmt->get_scope() ||
             isSgTemplateInstantiationDirectiveStatement(
                 classdecl_stmt->get_parent()) != NULL);
        switch (classdecl_stmt->get_class_type()) {
        case SgClassDeclaration::e_class: {
          curprint("class ");
          break;
        }
        case SgClassDeclaration::e_struct: {
          curprint(use_class_keyword_for_template_instantiation ? "class "
                                                                : "struct ");
          break;
        }
        case SgClassDeclaration::e_union: {
          curprint("union ");
          break;
        }

          // DQ (4/17/2007): Added this enum value to the switch cases.
        case SgClassDeclaration::e_template_parameter: {
          // skip type elaboration here.
          curprint(" ");
          break;
        }

          // DQ (4/17/2007): Added this enum value to the switch cases.
        default: {
          printf("Error: default reached in unparseClassDeclStmt() \n");
          ROSE_ABORT();
        }
        }
      }

      /* have to make sure if it needs qualifier or not */

      SgName nm = classdecl_stmt->get_name();

      // DQ (8/19/2014): Adding code to output the template instantiation with
      // template arguments processed to support name qualification.
      SgTemplateInstantiationDecl *templateInstantiation =
          isSgTemplateInstantiationDecl(classdecl_stmt);
      if (templateInstantiation != NULL) {
        nm = templateInstantiation->get_name();
      }

      // DQ (7/20/2011): Test compilation without these functions.

      // DQ (7/28/2012): This is the original code (I think it is what we really
      // want, but we need to test this. DQ (6/5/2011): Newest refactored
      // support for name qualification.
      SgName nameQualifier;
      if (classdecl_stmt->isForward() ||
          classdecl_stmt->get_parent() != classdecl_stmt->get_scope()) {
        nameQualifier = classdecl_stmt->get_qualified_name_prefix();
      }

      // DQ (6/9/2013): Further restrict this to the special case of un-named
      // unions. bool isAnonymousName =
      // (string(classdecl_stmt->get_name()).substr(0,14) == "__anonymous_0x")
      // != string::npos); bool isAnonymousName =
      // (string(classdecl_stmt->get_name()).substr(0,14) == "__anonymous_0x");
      bool isAnonymousName =
          hasGeneratedAnonymousNamePrefix(classdecl_stmt->get_name()) &&
          (classdecl_stmt->get_class_type() == SgClassDeclaration::e_union);
      bool allowUnnamedInternalName =
          info.PrintName() &&
          (isSgTypedefDeclaration(classdecl_stmt->get_parent()) != NULL ||
           isSgVariableDeclaration(classdecl_stmt->get_parent()) != NULL);
      // DQ (8/19/2014): Adding code to output the template instantiation with
      // template arguments processed to support name qualification.
      if (templateInstantiation != NULL) {
        // DQ (4/13/2019): Make this conditional upon the setting of
        // info.SkipNameQualification() curprint (nameQualifier);
        if (info.SkipNameQualification() == false) {
          curprint(nameQualifier);
          // curprint ("/* conditional output of name qualification */");
        }

        // DQ (4/13/2019): Turn this off before processing the rest of the
        // template instantiation which man contain template arguments that
        // require name qualification.
        info.unset_SkipNameQualification();

        unparseTemplateName(templateInstantiation, info);
      } else {
        // DQ (11/21/2021): I think we can skip the name of the enum here for
        // where this is used in the typedef as a anonymous type.
        // curprint(enum_stmt->get_name() + " ");
        // printf ("We could skip the name of the enum here ... \n");
        // if (info.PrintName() == true)
        // if (isAnonymousName == false && classdecl_stmt->get_isUnNamed() ==
        // false) if (isAnonymousName == false &&
        // classdecl_stmt->get_isUnNamed() == false && info.PrintName() == true)
        if (isAnonymousName == false &&
            classdecl_stmt->get_isUnNamed() == false) {
          curprint(nameQualifier.str());
          curprint(classdecl_stmt->get_name() + " ");
        } else {
          // Keep synthesized names available for internal symbol-table use, but
          // only surface them when an unnamed declaration is embedded in a
          // typedef or variable declaration that needs a disambiguating type
          // name.
          if (allowUnnamedInternalName) {
            curprint(classdecl_stmt->get_name() + " ");
          }
        }
      }

      // DQ (2/12/2019): The "final" keyword can ounly be output on the defining
      // declaration (at least for GNU g++ version 5.1). It is however
      // consistant in ROSE that it be marked uniformally within the defining
      // and nondefining declaration. DQ (8/11/2014): Added support for final
      // keyword unparsing. if
      // (classdecl_stmt->get_declarationModifier().isFinal() == true)
      if ((classdecl_stmt->isForward() == false) &&
          (classdecl_stmt->get_declarationModifier().isFinal() == true)) {
        // DQ (2/12/2019): Testing, final can't be used on prototypes (I think).
        // curprint(" /* output from test 2 */ ");
        curprint("final ");
      }

      {
        // DQ (6/2/2021): Original code.
        if (classdecl_stmt->isForward() && !info.SkipSemiColon()) {
          curprint(";");

          if (classdecl_stmt->isExternBrace()) {
          }
        }
      }
    }

    // DQ (6/3/2021): Closing brace for if
    // (saved_unparsedPartiallyUsingTokenStream == false) above.
  }

#if DEBUG_USING_CURPRINT
  curprint("/* Leaving unparseClassDeclStmt */ \n");
#endif
}

void Unparse_ExprStmt::unparseClassInheritanceList(
    SgClassDefinition *classdefn_stmt, SgUnparse_Info &ninfo) {
  // print out the class inheritance

#define DEBUG_UNPARSE_CLASS_INHERITANCE 0

#if DEBUG_UNPARSE_CLASS_INHERITANCE
  printf("Inside of unparseClassInheritanceList \n");
  curprint("/* Inside of unparseClassInheritanceList */ \n");
#endif

  SgBaseClassPtrList::iterator p = classdefn_stmt->get_inheritances().begin();
  if (p != classdefn_stmt->get_inheritances().end()) {
    curprint(string(": "));

    std::function<void(SgNonrealDecl *, const SgName &, SgUnparse_Info &)>
        unparse_nonreal_base_decl = [&](SgNonrealDecl *nr_decl,
                                        const SgName &base_name_qualifier,
                                        SgUnparse_Info &base_info) {
          ASSERT_not_null(nr_decl);

          bool has_nonreal_parent = false;
          if (SgDeclarationScope *parent_scope =
                  isSgDeclarationScope(nr_decl->get_parent())) {
            if (SgNonrealDecl *parent_decl =
                    isSgNonrealDecl(parent_scope->get_parent())) {
              has_nonreal_parent = true;
              unparse_nonreal_base_decl(parent_decl, SgName(), base_info);
              curprint("::");
            }
          }

          if (!has_nonreal_parent) {
            if (nr_decl->get_has_global_qualifier()) {
              curprint("::");
            }
            if (!base_name_qualifier.is_null()) {
              curprint(base_name_qualifier.str());
            }
          } else if (nr_decl->get_has_template_keyword()) {
            curprint("template ");
          }

          curprint(nr_decl->get_name().str());

          SgTemplateArgumentPtrList &tpl_args = nr_decl->get_tpl_args();
          if (tpl_args.size() > 0 || nr_decl->get_is_nonreal_template()) {
            if (tpl_args.empty()) {
              curprint("<>");
            } else {
              SgTemplateArgumentPtrList explicit_args = tpl_args;
              SgUnparse_Info ninfo(base_info);
              ninfo.set_SkipClassDefinition();
              ninfo.set_SkipEnumDefinition();
              ninfo.set_SkipClassSpecifier();
              unp->u_exprStmt->unparseTemplateArgumentList(explicit_args,
                                                           ninfo);
            }
          }
        };

    // DQ (5/9/2011): This loop structure should be rewritten...
    while (true) {
      SgBaseClass *bcls = *p;
      ASSERT_not_null(bcls);

      SgBaseClassModifier &baseClassModifier = *(bcls->get_baseClassModifier());

      if (baseClassModifier.isVirtual()) {
        curprint(string("virtual "));
      }

      if (baseClassModifier.get_accessModifier().isPublic()) {
        curprint(string("public "));
      }
      if (baseClassModifier.get_accessModifier().isPrivate()) {
        curprint(string("private "));
      }
      if (baseClassModifier.get_accessModifier().isProtected()) {
        curprint(string("protected "));
      }

      // DQ (5/12/2011): This might have to be a qualified name...
      SgUnparse_Info tmp_ninfo(ninfo);
      tmp_ninfo.set_name_qualification_length(
          bcls->get_name_qualification_length());
      tmp_ninfo.set_global_qualification_required(
          bcls->get_global_qualification_required());

      SgName nameQualifier = bcls->get_qualified_name_prefix();
      bool is_pack_expansion_base = bcls->get_pack_expansion();

      SgNonrealBaseClass *nr_bcls = isSgNonrealBaseClass(bcls);
      if (nr_bcls != NULL) {
        SgNonrealDecl *nr_decl = nr_bcls->get_base_class_nonreal();
        ASSERT_not_null(nr_decl);
        unparse_nonreal_base_decl(nr_decl, nameQualifier, tmp_ninfo);
      } else {
        SgClassDeclaration *tmp_decl = bcls->get_base_class();
        ASSERT_not_null(tmp_decl);
        SgTemplateInstantiationDecl *templateInstantiationDeclaration =
            isSgTemplateInstantiationDecl(tmp_decl);
        if (templateInstantiationDeclaration != NULL) {
          SgNode *nodeReferenceToClass = *p;
          if (nodeReferenceToClass != NULL) {
            auto const /* iterator */ i =
                SgNode::get_globalTypeNameMap().find(nodeReferenceToClass);
            if (i != SgNode::get_globalTypeNameMap().end()) {
              string classNameString = i->second.c_str();
              curprint(nameQualifier.str());
              curprint(classNameString);
            } else {
              SgUnparse_Info ninfo2(ninfo);
              SgBaseClass *baseClass = *p;
              ASSERT_not_null(baseClass);
              curprint(nameQualifier.str());
              ninfo2.set_reference_node_for_qualification(baseClass);
              unparseTemplateName(templateInstantiationDeclaration, ninfo2);
            }
          } else {
            SgUnparse_Info ninfo2(ninfo);
            SgBaseClass *baseClass = *p;
            ASSERT_not_null(baseClass);
            curprint(nameQualifier.str());
            ninfo2.set_reference_node_for_qualification(baseClass);
            unparseTemplateName(templateInstantiationDeclaration, ninfo2);
          }
        } else {
          curprint(nameQualifier.str());
          curprint(tmp_decl->get_name().str());
        }
      }
      if (is_pack_expansion_base) {
        curprint("...");
      }

      p++;

      if (p != classdefn_stmt->get_inheritances().end()) {
        curprint(string(","));
      } else {
        break;
      }
    }
  }
}

void Unparse_ExprStmt::unparseClassDefnStmt(SgStatement *stmt,
                                            SgUnparse_Info &info) {

#define DEBUG_UNPARSE_CLASS_DEFINITION 0

#if DEBUG_UNPARSE_CLASS_DEFINITION
  printf("In unparseClassDefnStmt() \n");
  curprint("/* In unparseClassDefnStmt() */ \n");
#endif

  SgClassDefinition *classdefn_stmt = isSgClassDefinition(stmt);
  ASSERT_not_null(classdefn_stmt);

  if (SgClassDeclaration *class_decl = classdefn_stmt->get_declaration()) {
    std::string name = class_decl->get_name().getString();
    if (name == "ios_base" || name == "__gconv_info" || name == "_Deque_base" ||
        name == "ParmParse") {
      std::cerr << "TRACE unparseClassDefnStmt def=" << classdefn_stmt
                << " decl=" << class_decl << " name=" << name
                << " skipBasicBlock=" << (info.SkipBasicBlock() ? 1 : 0)
                << " skipClassDef=" << (info.SkipClassDefinition() ? 1 : 0)
                << " partialTokens="
                << (info.unparsedPartiallyUsingTokenStream() ? 1 : 0)
                << " members="
                << static_cast<long long>(classdefn_stmt->get_members().size())
                << " parent="
                << (classdefn_stmt->get_parent() != NULL
                        ? classdefn_stmt->get_parent()->class_name()
                        : std::string("<null>"))
                << std::endl;
      if (name == "_Deque_base" || name == "ParmParse") {
        for (SgDeclarationStatement *member : classdefn_stmt->get_members()) {
          if (SgClassDeclaration *member_class = isSgClassDeclaration(member)) {
            std::cerr
                << "TRACE unparseClassDefnStmt member name="
                << member_class->get_name().getString()
                << " decl=" << member_class
                << " forward=" << (member_class->isForward() ? 1 : 0)
                << " hasDef="
                << (member_class->get_definition() != NULL ? 1 : 0)
                << " output="
                << (member_class->get_file_info() != NULL &&
                            member_class->get_file_info()
                                ->isOutputInCodeGeneration()
                        ? 1
                        : 0)
                << " compilerGen="
                << (member_class->get_file_info() != NULL &&
                            member_class->get_file_info()->isCompilerGenerated()
                        ? 1
                        : 0)
                << std::endl;
          }
        }
      }
    }
  }

  // DQ (5/28/2021): Adding support for partial token sequence unparsing.
  bool saved_unparsedPartiallyUsingTokenStream =
      info.unparsedPartiallyUsingTokenStream();
  if (saved_unparsedPartiallyUsingTokenStream == true) {
    // SgStatement* body = while_stmt->get_body();
    // curprint("/* partial token sequence SgWhileStmt */ ");
    // SgStatement* condition = while_stmt->get_condition();
    // unparseStatementFromTokenStream (stmt, condition,
    // e_token_subsequence_start, e_token_subsequence_start, info);
  } else {
    // unp->cur.format(namespaceDefinition, info, FORMAT_BEFORE_BASIC_BLOCK2);
    // curprint("/* from AST SgWhileStmt */ while(");
    // curprint("while(");
    // unp->cur.format(namespaceDefinition, info, FORMAT_AFTER_BASIC_BLOCK2);
  }

#if OUTPUT_HIDDEN_LIST_DATA
  outputHiddenListData(classdefn_stmt);
#endif

  SgUnparse_Info ninfo(info);

  // curprint ( string("/* Print out class declaration */ \n";

  ninfo.set_SkipClassDefinition();

  // DQ (9/9/2016): Added to conform to unifor testing that these are always
  // equal.
  ninfo.set_SkipEnumDefinition();

  // DQ (10/13/2006): test2004_133.C demonstrates where we need to unparse
  // qualified names for class definitions (defining declaration). DQ
  // (10/11/2006): Don't generate qualified names for the class name of a
  // defining declaration ninfo.set_SkipQualifiedNames();

  // DQ (7/19/2003) skip the output of the semicolon
  ninfo.set_SkipSemiColon();

#if DEBUG_USING_CURPRINT
  curprint(
      "\n/* In unparseClassDefnStmt(): calling unparseClassDeclStmt() */ \n");
#endif

  // printf ("Calling unparseClassDeclStmt = %p isForward = %s from
  // unparseClassDefnStmt = %p \n",
  //      classdefn_stmt->get_declaration(),(classdefn_stmt->get_declaration()->isForward()
  //      == true) ? "true" : "false",classdefn_stmt);
  ASSERT_not_null(classdefn_stmt->get_declaration());
  unparseClassDeclStmt(classdefn_stmt->get_declaration(), ninfo);

#if DEBUG_USING_CURPRINT
  curprint("\n/* In unparseClassDefnStmt(): DONE: calling "
           "unparseClassDeclStmt() */ \n");
#endif

  // DQ (7/19/2003) unset the specification to skip the output of the semicolon
  ninfo.unset_SkipSemiColon();

  // DQ (10/11/2006): Don't generate qualified names for the class name of a
  // defining declaration ninfo.unset_SkipQualifiedNames();

  ninfo.unset_SkipClassDefinition();

  // DQ (9/9/2016): Added to conform to unifor testing that these are always
  // equal.
  ninfo.unset_SkipEnumDefinition();

  // curprint("/* END: Print out class declaration */ \n");

  SgNamedType *saved_context = ninfo.get_current_context();

  // DQ (11/29/2004): The use of a primary and secondary declaration casue two
  // SgClassType nodes to be generated (which should be fixed) since this is
  // compared to another SgClassType within the generateQualifiedName() function
  // we have to get the the type from the non-defining declaration uniformally.
  // Same way each time so that the pointer test will be meaningful.
  // ninfo.set_current_context(classdefn_stmt->get_declaration()->get_type());
  ASSERT_not_null(
      classdefn_stmt->get_declaration()->get_firstNondefiningDeclaration());
  SgClassDeclaration *classDeclaration = isSgClassDeclaration(
      classdefn_stmt->get_declaration()->get_firstNondefiningDeclaration());
  ASSERT_not_null(classDeclaration->get_type());

  // DQ (6/13/2007): Set to null before resetting to non-null value
  // ninfo.set_current_context(classdefn_stmt->get_declaration()->get_firstNondefiningDeclaration()->get_type());
  ninfo.set_current_context(NULL);
  ninfo.set_current_context(classDeclaration->get_type());

#if DEBUG_UNPARSE_CLASS_DEFINITION
  printf("In unparseClassDefnStmt(): Print out inheritance \n");
  curprint("/* Print out inheritance */ \n");
#endif

  // DQ (1/8/2020): Refactors the output of base classes so that it can be
  // supported in the unparseClassDefnStmt() and unparseClassType() functions.
  unparseClassInheritanceList(classdefn_stmt, ninfo);

#if DEBUG_UNPARSE_CLASS_DEFINITION
  // curprint ( string("\n/* After specification of base classes unparse the
  // declaration body */ \n";
  printf("After specification of base classes unparse the declaration body  "
         "info.SkipBasicBlock() = %s \n",
         (info.SkipBasicBlock() == true) ? "true" : "false");
#endif

  // DQ (9/28/2004): Turn this back on as the only way to prevent this from
  // being unparsed! DQ (11/22/2003): Control unparsing of the {} part of the
  // definition if ( !info.SkipBasicBlock() )
  if (info.SkipBasicBlock() == false) {
    // curprint ( string("\n/* Unparsing class definition within
    // unparseClassDefnStmt */ \n";

    // DQ (6/14/2006): Add packing pragma support (explicitly set the packing
    // alignment to the default, part of packing pragma normalization).
    unsigned int packingAlignment = classdefn_stmt->get_packingAlignment();
    if (packingAlignment != 0) {
      curprint(string("\n#pragma pack(") +
               StringUtility::numberToString(packingAlignment) + string(")"));
    }

    ninfo.set_isUnsetAccess();
    unp->cur.format(classdefn_stmt, info, FORMAT_BEFORE_BASIC_BLOCK1);
    curprint(string("{"));
    unp->cur.format(classdefn_stmt, info, FORMAT_AFTER_BASIC_BLOCK1);

    // DQ (2/25/2016): Adding support for specification of AstUnparseAttribute
    // for placement inside of a SgClassDefinition. Note that this does not
    // replace any existing declarations in the class definition.
    AstUnparseAttribute *unparseAttribute = dynamic_cast<AstUnparseAttribute *>(
        classdefn_stmt->getAttribute(AstUnparseAttribute::markerName));
    if (unparseAttribute != NULL) {
      // Note that in most cases unparseLanguageSpecificStatement() will be
      // called, some formatting via "unp->cur.format(stmt, info,
      // FORMAT_BEFORE_STMT);" may be done.  This can cause extra CRs to be
      // inserted (which only looks bad).  Not clear now to best clean this up.
      string code = unparseAttribute->toString(AstUnparseAttribute::e_inside);
      curprint(code);
    }

    SgDeclarationStatementPtrList::iterator pp =
        classdefn_stmt->get_members().begin();

    while (pp != classdefn_stmt->get_members().end()) {
#if DEBUG_UNPARSE_CLASS_DEFINITION
      printf("In unparseClassDefnStmt(): "
             "(*pp)->get_declarationModifier().get_accessModifier()."
             "isProtected() = %s \n",
             (*pp)->get_declarationModifier().get_accessModifier().isProtected()
                 ? "true"
                 : "false");
#endif
      unparseStatement((*pp), ninfo);
      pp++;
    }

    // DQ (3/17/2005): This helps handle cases such as class foo { #include
    // "constant_code.h" }
    ASSERT_not_null(classdefn_stmt->get_startOfConstruct());
    ASSERT_not_null(classdefn_stmt->get_endOfConstruct());

    unparseAttachedPreprocessingInfo(classdefn_stmt, info,
                                     PreprocessingInfo::inside);

    // DQ (5/28/2021): Fixing the unparseClassDefnStmt() function to support
    // partial unparsing from the token stream. unp->cur.format(classdefn_stmt,
    // info, FORMAT_BEFORE_BASIC_BLOCK2); curprint ( string("}"));
    if (saved_unparsedPartiallyUsingTokenStream == false) {
#if DEBUG_USING_CURPRINT
      curprint("\n/* saved_unparsedPartiallyUsingTokenStream == false */\n");
#endif
      unp->cur.format(classdefn_stmt, info, FORMAT_BEFORE_BASIC_BLOCK2);
      curprint(string("}"));
    } else {
      // This should be addressed using the closing part of the token stream, I
      // think.
      curprint(
          "\n/* closing the class definition from the token stream?? */\n");
    }

    // DQ (6/14/2006): Add packing pragma support (reset the packing
    // alignment to the default, part of packing pragma normalization).
    if (packingAlignment != 0) {
      curprint(string("\n#pragma pack()"));
    }

    unp->cur.format(classdefn_stmt, info, FORMAT_AFTER_BASIC_BLOCK2);
  }

  // DQ (6/13/2007): Set to null before resetting to non-null value
  ninfo.set_current_context(NULL);
  ninfo.set_current_context(saved_context);

  unparseTypeAttributes(classdefn_stmt->get_declaration());

#if DEBUG_USING_CURPRINT
  curprint("/* Leaving unparseClassDefnStmt */ \n");
#endif
}

void Unparse_ExprStmt::unparseTypeAttributes(
    SgDeclarationStatement *declaration) {
  // DQ (10/4/2012): Added support for transparent unions.
  ASSERT_not_null(declaration);

  bool isGnuAttributeTransparentUnion = declaration->get_declarationModifier()
                                            .get_typeModifier()
                                            .isGnuAttributeTransparentUnion();

  // If this came from a type then declaration is the first nondefining
  // declaration (see test2012_10_4.c).
  bool definingDeclaration_isGnuAttributeTransparentUnion = false;
  if (declaration->get_definingDeclaration() != NULL)
    definingDeclaration_isGnuAttributeTransparentUnion =
        declaration->get_definingDeclaration()
            ->get_declarationModifier()
            .get_typeModifier()
            .isGnuAttributeTransparentUnion();

  if (definingDeclaration_isGnuAttributeTransparentUnion == true)
    isGnuAttributeTransparentUnion = true;

  // This should only be set for unions.
  if (isGnuAttributeTransparentUnion == true) {
    curprint(" __attribute__((__transparent_union__))");
  }

  // DQ (1/3/2014): Added support for packing attribute.
  if (declaration->get_declarationModifier()
          .get_typeModifier()
          .isGnuAttributePacked() == true) {
    // curprint(" /* from unparseTypeAttributes(SgDeclarationStatement*) */
    // __attribute__((packed))");
    curprint(" __attribute__((packed))");
  }

  // DQ (7/24/2014): Added support to unparse the alignment attribute.
  short alignmentValue = declaration->get_declarationModifier()
                             .get_typeModifier()
                             .get_gnu_attribute_alignment();

  // DQ (7/24/2014): The default value is changed from zero to -1 (and the type
  // was make to be a short (signed) value).
  if (alignmentValue >= 0) {
    // DQ (7/27/2014): Fixed attribute to have correct spelling.
    // curprint(" __attribute__((align(N)))");
    // curprint(" __attribute__((align(");
    curprint(" __attribute__((aligned(");
    curprint(StringUtility::numberToString((int)alignmentValue));
    curprint(")))");
  }
}

void Unparse_ExprStmt::unparseEnumDeclStmt(SgStatement *stmt,
                                           SgUnparse_Info &info) {
  SgEnumDeclaration *enum_stmt = isSgEnumDeclaration(stmt);
  ASSERT_not_null(enum_stmt);

  // info.display("Called inside of unparseEnumDeclStmt()");

#if DEBUG_USING_CURPRINT
  curprint("\n/* Inside of Unparse_ExprStmt::unparseEnumDeclStmt() */ \n");
#endif

  string enum_string = "enum ";

  // DQ (8/12/2014): Adding support for C++11 scoped enums (syntax is "enum
  // class ").
  if (enum_stmt->get_isScopedEnum() == true) {
    enum_string += "class ";
  }

  // Check if this enum declaration appears imbedded within another declaration
  if (!info.inEmbeddedDecl()) {
    // This is the more common declaration of an enum with the definition
    // attached.
    // If this is part of a class definition then get the access information
    SgClassDefinition *cdefn = isSgClassDefinition(enum_stmt->get_parent());
    if (cdefn && cdefn->get_declaration()->get_class_type() ==
                     SgClassDeclaration::e_class) {
      info.set_CheckAccess();
    }
    // printDebugInfo("entering unp->u_sage->printSpecifier", true);
    unp->u_sage->printSpecifier(enum_stmt, info);
    info.unset_CheckAccess();

    // DQ (2/14/2019): Adding name qualification support.
    // curprint(enum_string + enum_stmt->get_name().str() + " ");
    curprint(enum_string);

    // DQ (5/12/2011): This might have to be a qualified name...
    SgUnparse_Info ninfo(info);
    ninfo.set_name_qualification_length(
        enum_stmt->get_name_qualification_length());
    ninfo.set_global_qualification_required(
        enum_stmt->get_global_qualification_required());

    ASSERT_not_null(enum_stmt);

    SgName nameQualifier = enum_stmt->get_qualified_name_prefix();

    // DQ (11/21/2021): I think we can skip the name of the enum here for where
    // this is used in the typedef as a anonymous type.
    // curprint(enum_stmt->get_name() + " ");
    // printf ("We could skip the name of the enum here ... \n");
    bool isAnonymousName =
        hasGeneratedAnonymousNamePrefix(enum_stmt->get_name());
    if (isAnonymousName == false) {
      curprint(nameQualifier.str());
      curprint(enum_stmt->get_name() + " ");
    } else {
      if (info.PrintName() == true) {
        curprint(nameQualifier.str());
        curprint(enum_stmt->get_name() + " ");
      }
    }
  } else {
    // DQ (2/14/2019): Test if this branch is ever taken.
    printf("Exiting as a test (need to know if this branch is ever taken) \n");
    ROSE_ABORT();

    // This is a declaration of an enum appearing within another declaration
    // (e.g. function declaration as a return type).
    SgClassDefinition *cdefn = NULL;

    // DQ (9/9/2004): Put this message in place at least for now!
    printf("Need logic to handle enums defined in namespaces and which require "
           "qualification \n");

    // ck if defined within a var decl
    int v = GLOBAL_STMT;
    // SgStatement *cparent=enum_stmt->get_parent();
    SgStatement *cparent = isSgStatement(enum_stmt->get_parent());
    v = cparent->variant();
    if (v == VAR_DECL_STMT || v == TYPEDEF_STMT)
      cdefn = isSgClassDefinition(cparent->get_parent());
    else if (v == CLASS_DEFN_STMT)
      cdefn = isSgClassDefinition(cparent);

    if (cdefn) {
      SgNamedType *ptype = isSgNamedType(cdefn->get_declaration()->get_type());
      if (!ptype || (info.get_current_context() == ptype)) {
        // curprint( string("enum " ) +  enum_stmt->get_name().str() + " ");
        curprint(enum_string + enum_stmt->get_name().str() + " ");
      } else {
        // add qualifier of current types to the name
        SgName nm = cdefn->get_declaration()->get_qualified_name();
        if (!nm.is_null()) {
          curprint(string(nm.str()) + "::" + enum_stmt->get_name().str() + " ");
        } else {
          // curprint( string("enum " ) + enum_stmt->get_name().str() + " ");
          curprint(enum_string + enum_stmt->get_name().str() + " ");
        }
      }
    } else {
      // curprint ( string("enum " ) + enum_stmt->get_name().str() + " ");
      curprint(enum_string + enum_stmt->get_name().str() + " ");
    }
  }

  // DQ (8/12/2014): Adding support for C++11 base type specification syntax.
  if (enum_stmt->get_field_type() != NULL) {
    curprint(" : ");

    // Make a new SgUnparse_Info object.
    SgUnparse_Info ninfo(info);
    unp->u_type->unparseType(enum_stmt->get_field_type(), ninfo);
  }

  // DQ (6/26/2005): Support for empty enum declarations!
  if (enum_stmt == enum_stmt->get_definingDeclaration()) {
    curprint("{");
  }

  // if (!info.SkipDefinition()
  if (!info.SkipEnumDefinition()
      /* [BRN] 4/19/2002 --  part of the fix in unparsing var decl including
         enum definition */
      || enum_stmt->get_embedded()) {
    SgUnparse_Info ninfo(info);
    ninfo.set_inEnumDecl();
    SgInitializer *tmp_init = NULL;
    SgName tmp_name;

    // TODO wrap into a function and to be called by all
    SgInitializedNamePtrList::iterator p = enum_stmt->get_enumerators().begin();
    SgInitializedNamePtrList::iterator p_last =
        enum_stmt->get_enumerators().end();

    // Guard against decrementing an invalid iterator
    if (p != p_last)
      p_last--;

#if DEBUG_USING_CURPRINT
    curprint("\n/* In Unparse_ExprStmt::unparseEnumDeclStmt(): output the "
             "enumerators */ \n");
#endif

    // DQ (2/5/2021): Note that token-based unparsing is not supported for
    // partial token sequence output. DQ (2/5/2021): I think this should be a
    // while loop...
    for (; p != enum_stmt->get_enumerators().end(); p++) {
      // Liao, 5/14/2009
      // enumerators may come from another included file
      // have to tell if it matches the current declaration's file before
      // unparsing it!! See test case:
      // tests/nonsmoke/functional/CompileTests/C_test/test2009_05.c
      // TODO: still need work on mixed cases: part of elements are in the
      // original file and others are from a header
      SgInitializedName *field = *p;
      ASSERT_not_null(field);
      bool isInSameFile = (field->get_file_info()->get_filename() ==
                           enum_stmt->get_file_info()->get_filename());
      if (isInSameFile) {
#if DEBUG_USING_CURPRINT
        curprint("\n/* In Unparse_ExprStmt::unparseEnumDeclStmt(): output "
                 "enumerator (isInSameFile == true) */ \n");
#endif
        unparseAttachedPreprocessingInfo(field, info,
                                         PreprocessingInfo::before);
        // unparse the element
        ASSERT_not_null((*p));
        tmp_name = (*p)->get_name();
        tmp_init = (*p)->get_initializer();
        curprint(tmp_name.str());
        if (tmp_init != NULL) {
          curprint("=");
          unparseExpression(tmp_init, ninfo);
        }

        // if (p != (enum_stmt->get_enumerators().end()))
        if (p != p_last) {
          curprint(",");
        }

      } // end same file
    } // end for

    if (enum_stmt->get_enumerators().size() != 0) {
      // DQ (3/17/2005): This helps handle cases such as void foo () { #include
      // "constant_code.h" }
      unparseAttachedPreprocessingInfo(enum_stmt, info,
                                       PreprocessingInfo::inside);
    }
    /* [BRN] 4/19/2002 -- part of fix in unparsing var decl including enum
     * definition */
    if (enum_stmt->get_embedded()) {
      curprint(" ");
    }
    enum_stmt->set_embedded(false);
    /* [BRN] end */
  } /* if */

  // DQ (6/26/2005): Support for empty enum declarations!
  if (enum_stmt == enum_stmt->get_definingDeclaration()) {
    curprint("} ");
  }

  // DQ (6/26/2005): Moved to location after output of closing "}" from enum
  // definition
  if (!info.SkipSemiColon()) {
    curprint(";");
    if (enum_stmt->isExternBrace()) {
    }
  }

#if DEBUG_USING_CURPRINT
  curprint("\n/* Leaving unparseEnumDeclStmt() */ \n");
#endif
}

void Unparse_ExprStmt::unparseExprStmt(SgStatement *stmt,
                                       SgUnparse_Info &info) {
  SgExprStatement *expr_stmt = isSgExprStatement(stmt);
  ASSERT_not_null(expr_stmt);

  SgUnparse_Info newinfo(info);

  // DQ (5/9/2015): Added assertion.
  ASSERT_not_null(expr_stmt->get_expression());

  // Expressions are another place where a class definition should NEVER be
  // unparsed DQ (5/23/2007): Note that statement expressions can have class
  // definition (so they are exceptions, see test2007_51.C).
  newinfo.set_SkipClassDefinition();

  // DQ (1/9/2014): We have to make the handling of enum definitions consistant
  // with that of class definitions.
  newinfo.set_SkipEnumDefinition();

  // DQ (1/9/2014): These should have been setup to be the same.
  ROSE_ASSERT(newinfo.SkipClassDefinition() == newinfo.SkipEnumDefinition());

  // if (expr_stmt->get_the_expr())
  if (expr_stmt->get_expression()) {
    // printDebugInfo(getSgVariant(expr_stmt->get_the_expr()->variant()), true);
    // unparseExpression(expr_stmt->get_the_expr(), newinfo);
    unparseExpression(expr_stmt->get_expression(), newinfo);
  } else {
    ROSE_ABORT();
  }

  if (newinfo.inVarDecl()) {
    curprint(",");
  } else {
    // DQ (11/2/2015): This is part of a change to support uniformity in how for
    // statement tests are unparsed. if (!newinfo.inConditional() &&
    // !newinfo.SkipSemiColon()) if (newinfo.SkipSemiColon() == false)
    if (newinfo.SkipSemiColon() == false) {
      // DQ (11/2/2015): Add a space to match previous behavior (and tests using
      // diff). No, I don't like this. curprint(";");
      curprint(";");
    }
  }
}

void Unparse_ExprStmt::unparseLabelStmt(SgStatement *stmt,
                                        SgUnparse_Info &info) {
  SgLabelStatement *label_stmt = isSgLabelStatement(stmt);
  ASSERT_not_null(label_stmt);

  curprint(string(label_stmt->get_label().str()) + ":");

  // DQ (3/15/2006): Remove the "/* empty statement */" comment (leave the ";").
  // DQ (10/20/2005): Unparse an empty statement with each label
  // curprint ( string(" /* empty statement */ ;";

  // DQ (3/18/2006): I don't think we need this and if we do then we need an
  // example of where we need it. test2005_164.C demonstrates that we need the
  // ";" if the label is the last statment in the block. The frontend is more
  // accepting and does not require a ";" for a label appearing at the end of
  // the block, but g++ is particular on this subtle point.  So we should make
  // the unparser figure this out. curprint ( string(" ;";

  ASSERT_not_null(label_stmt->get_parent());

  // DQ (10/25/2007): Modified handling of labels so that they explicitly
  // include a scope set to the SgFunctionDefinition. SgLabelSymbol objects are
  // also now placed into the SgFunctionDefinition's symbol table.
  // SgScopeStatement* scope = label_stmt->get_scope();
  SgScopeStatement *scope = isSgScopeStatement(label_stmt->get_parent());
  // ASSERT_not_null(scope);

  // SgStatementPtrList & statementList = scope->getStatementList();
  SgStatementPtrList *statementList = NULL;

  bool allocatedStatementList = false;
  if (scope != NULL) {
    // In C and C++, labels can't be places where only declarations are allowed
    // so we know to get the Statement list instead of the declaration list.
    ROSE_ASSERT(scope->containsOnlyDeclarations() == false);

    // DQ (10/27/2012): This is called as part of processing test2012_104.c.  So
    // I guess we have to support it...
    SgIfStmt *ifStatement = isSgIfStmt(scope);

    if (ifStatement != NULL) {
      // The test2012_116.c and the smaller test2012_117.c demonstate and
      // example of a label attached to one side of a conditional.
      statementList = new SgStatementPtrList();
      ASSERT_not_null(statementList);

      allocatedStatementList = true;

      statementList->push_back(ifStatement->get_true_body());
      statementList->push_back(ifStatement->get_false_body());
    } else {
      statementList = &(scope->getStatementList());
    }
  } else {
    // This is going to be the trivial case of only the label statement itself
    // in the statementList.
    statementList = new SgStatementPtrList();
    ASSERT_not_null(statementList);

    allocatedStatementList = true;

    statementList->push_back(label_stmt);
  }

  ASSERT_not_null(statementList);

  // Find the label in the parent scope
  SgStatementPtrList::iterator positionOfLabel =
      find(statementList->begin(), statementList->end(), label_stmt);

  // Verify that we found the label in the scope
  // ROSE_ASSERT(positionOfLabel != SgStatementPtrList::npos);
  if (positionOfLabel == statementList->end()) {
  } else {
    // ROSE_ASSERT(positionOfLabel != statementList.end());
    ROSE_ASSERT(*positionOfLabel == label_stmt);
    // DQ (10/27/2012): I think it is a bug to increment positionOfLabel.
    // Increment past the label to see what is next (but don't count
    // SgNullStatements) positionOfLabel++;

    while ((positionOfLabel != statementList->end()) &&
           ((*positionOfLabel)->variantT() == V_SgNullStatement)) {
      positionOfLabel++;
    }

    // If we are at the end (just past the last statement) then we need the ";"
    if (positionOfLabel == statementList->end()) {
      curprint(string(";"));
    }
  }

  // If this is memory that we allocated on the heap then call delete.
  if (allocatedStatementList == true) {
    delete statementList;
    statementList = NULL;
  }

  // DQ (1/14/2013): However, we want it to be unparsed by the function
  // unparsing the statement list (typically SgBasicBlock) instead of here. The
  // statement associated with the SgLabelStatement was not previously being
  // inserted into the list of the current scope and this is a problem for the
  // data flow analysis (bug reported by Robb).  Also we don't want to have a
  // mechanism for hidding statements behind SgLabelStatements in general, so it
  // makes more sense to not unparse the associated statement here.

  // DQ (1/6/2018): Turn this back on since we handled labels as compound
  // statements now, at least where they are processed in switch statements
  // (which will have to be made uniform). DQ (10/27/2012): Unparse the
  // associated statement to the label. Note that in the edg33 version of ROSE
  // this was always a SgNullStatement, this is corrected in the design with the
  // edg4x work.
  if (label_stmt->get_statement() != NULL) {
    unparseStatement(label_stmt->get_statement(), info);
  }
}

void Unparse_ExprStmt::unparsePragmaAttribute(SgScopeStatement *scope_stmt) {
  ROSE_ASSERT(scope_stmt != NULL);
  if (scope_stmt->get_pragma() != NULL) {
    SgPragma *pragma = scope_stmt->get_pragma();
    string text_string = pragma->get_pragma();
    curprint("\n#pragma " + text_string + "\n");
  }
}

void Unparse_ExprStmt::unparseWhileStmt(SgStatement *stmt,
                                        SgUnparse_Info &info) {
  SgWhileStmt *while_stmt = isSgWhileStmt(stmt);
  ASSERT_not_null(while_stmt);

  // DQ (12/17/2014): Test for if we have unparsed partially using the token
  // stream. If so then we don't want to unparse this syntax, if not then we
  // require this syntax. curprint("while(");
  bool saved_unparsedPartiallyUsingTokenStream =
      info.unparsedPartiallyUsingTokenStream();
  if (saved_unparsedPartiallyUsingTokenStream == false) {
    // unp->cur.format(namespaceDefinition, info, FORMAT_BEFORE_BASIC_BLOCK2);
    // curprint("/* from AST SgWhileStmt */ while(");
    curprint("while(");
    // unp->cur.format(namespaceDefinition, info, FORMAT_AFTER_BASIC_BLOCK2);
  } else {
    // SgStatement* body = while_stmt->get_body();
    // curprint("/* partial token sequence SgWhileStmt */ ");
    SgStatement *condition = while_stmt->get_condition();
    unparseStatementFromTokenStream(stmt, condition, e_token_subsequence_start,
                                    e_token_subsequence_start, info);
  }

  // DQ (10/19/2012): We now want to have more control over where ";" is output.
  // See test2012_47.c for an example of there this can't be explicitly handled
  // for all parts of a conditional. In this case we call unset_SkipSemiColon()
  // in SgClassDefinition so that they will be output properly there.
  // Build a specific SgUnparse_Info to support the conditional.
  // info.set_inConditional();
  // info.set_inConditional();
  // unparseStatement(while_stmt->get_condition(), info);
  // info.unset_inConditional();

  SgUnparse_Info ninfo(info);
  ninfo.set_inConditional();
  ninfo.set_SkipSemiColon();
  unparseStatement(while_stmt->get_condition(), ninfo);

  // curprint(")");
  if (saved_unparsedPartiallyUsingTokenStream == false) {
    curprint(")");
    unparsePragmaAttribute(while_stmt);

    if (while_stmt->get_body()) {
      unp->cur.format(while_stmt->get_body(), info,
                      FORMAT_BEFORE_NESTED_STATEMENT);
      unparseStatement(while_stmt->get_body(), info);
      unp->cur.format(while_stmt->get_body(), info,
                      FORMAT_AFTER_NESTED_STATEMENT);
    } else {
      if (!info.SkipSemiColon()) {
        curprint(";");
      }
    }
  } else {
    SgStatement *condition = while_stmt->get_condition();
    SgStatement *body = while_stmt->get_body();

    ASSERT_not_null(condition);
    ASSERT_not_null(body);

    // unparseStatementFromTokenStream (condition, body,
    // e_trailing_whitespace_start, e_token_subsequence_start);
    // unparseStatementFromTokenStream (condition, body,
    // e_trailing_whitespace_start, e_leading_whitespace_start);
    unparseStatementFromTokenStream(condition, e_trailing_whitespace_start,
                                    e_trailing_whitespace_end, info);

    // Output syntax explicitly.
    curprint(")");

    unparseStatement(while_stmt->get_body(), info);
  }
}

void Unparse_ExprStmt::unparseDoWhileStmt(SgStatement *stmt,
                                          SgUnparse_Info &info) {
  SgDoWhileStmt *dowhile_stmt = isSgDoWhileStmt(stmt);
  ASSERT_not_null(dowhile_stmt);

  curprint("do ");

  unp->cur.format(dowhile_stmt->get_body(), info,
                  FORMAT_BEFORE_NESTED_STATEMENT);
  unparseStatement(dowhile_stmt->get_body(), info);
  unp->cur.format(dowhile_stmt->get_body(), info,
                  FORMAT_AFTER_NESTED_STATEMENT);

  // curprint( string("while " ) + "(");
  curprint("while (");

  SgUnparse_Info ninfo(info);
  ninfo.set_inConditional();

  // DQ (11/2/2015): Skip output of ";" in conditional.
  ninfo.set_SkipSemiColon();

  // we need to keep the properties of the prevnode (The next prevnode will set
  // the line back to where "do" was printed) SgLocatedNode* tempnode =
  // prevnode;

  unparseStatement(dowhile_stmt->get_condition(), ninfo);

  // Note that unseting this flag is not significant.
  ninfo.unset_inConditional();

  curprint(")");

  // DQ (9/24/2020): Adding support to unparse attached pragmas.
  unparsePragmaAttribute(dowhile_stmt);

  if (!info.SkipSemiColon()) {
    curprint(";");
  }
}

void Unparse_ExprStmt::unparseSwitchStmt(SgStatement *stmt,
                                         SgUnparse_Info &info) {
  SgSwitchStatement *switch_stmt = isSgSwitchStatement(stmt);

  ASSERT_not_null(switch_stmt);

  bool saved_unparsedPartiallyUsingTokenStream =
      info.unparsedPartiallyUsingTokenStream();

  // DQ (12/28/2014): Test for if we have unparsed partially using the token
  // stream.
  if (saved_unparsedPartiallyUsingTokenStream == false) {
    curprint("switch(");
  } else {

    // SgStatement* item_selector = switch_stmt->get_item_selector();
    // unparseStatementFromTokenStream (stmt, item_selector,
    // e_token_subsequence_start, e_leading_whitespace_start, info);
    // unparseStatementFromTokenStream (stmt, item_selector,
    // e_token_subsequence_start, e_leading_whitespace_start, info);

    SgStatement *item_selector = switch_stmt->get_item_selector();
    // unparseStatementFromTokenStream (stmt, item_selector,
    // e_token_subsequence_start, e_trailing_whitespace_end, info);
    unparseStatementFromTokenStream(stmt, item_selector,
                                    e_token_subsequence_start,
                                    e_token_subsequence_end, info);

    // If there is whitespace here it will be output.
    unparseStatementFromTokenStream(item_selector, e_trailing_whitespace_start,
                                    e_trailing_whitespace_end, info);

    // DQ (6/4/2021): Note that we need to output a single token of syntax,
    // since there is no reference to this in the token stream mapping.
    curprint(")");

    // SgStatement* switch_body = switch_stmt->get_body();

    // DQ (6/4/2021): The leading whitespace will be output with the body.
    // unparseStatementFromTokenStream (stmt, switch_body,
    // e_token_subsequence_start, e_leading_whitespace_end, info);

    unparseStatement(switch_stmt->get_body(), info);
  }

  if (saved_unparsedPartiallyUsingTokenStream == false) {
    SgUnparse_Info ninfo(info);
    ninfo.set_SkipSemiColon();
    ninfo.set_inConditional();
    unparseStatement(switch_stmt->get_item_selector(), ninfo);

    if (saved_unparsedPartiallyUsingTokenStream == false) {
      curprint(")");

      // DQ (9/23/2020): Adding support to unparse attached pragmas.
      unparsePragmaAttribute(switch_stmt);
    } else {
      SgStatement *item_selector = switch_stmt->get_item_selector();
      SgStatement *switch_body = switch_stmt->get_body();

      unparseStatementFromTokenStream(item_selector, switch_body,
                                      e_trailing_whitespace_start,
                                      e_leading_whitespace_start, info);
    }

    // DQ (11/5/2003): Support for skipping basic block added to support
    //                 prefix generation for AST Rewrite Mechanism
    // if(switch_stmt->get_body())
    if ((switch_stmt->get_body() != NULL) && !info.SkipBasicBlock()) {
      unparseStatement(switch_stmt->get_body(), info);
    }

    // DQ (6/4/2021): end of if (saved_unparsedPartiallyUsingTokenStream ==
    // false) (above)
  }
}

void Unparse_ExprStmt::unparseCaseStmt(SgStatement *stmt,
                                       SgUnparse_Info &info) {
  SgCaseOptionStmt *case_stmt = isSgCaseOptionStmt(stmt);
  ASSERT_not_null(case_stmt);

  bool saved_unparsedPartiallyUsingTokenStream =
      info.unparsedPartiallyUsingTokenStream();

  // DQ (12/28/2014): Test for if we have unparsed partially using the token
  // stream.
  if (saved_unparsedPartiallyUsingTokenStream == false) {
    curprint("case ");

    unparseExpression(case_stmt->get_key(), info);

    // DQ (1/31/2014): Adding support for gnu case range extension.
    if (case_stmt->get_key_range_end() != NULL) {
      // Note that the spaces on each side of the "..." are required to avoid
      // interpretation of the case range as a floating point number by the gnu
      // parser.
      curprint(" ... ");
      unparseExpression(case_stmt->get_key_range_end(), info);
    }

    curprint(":");
  } else {
    SgStatement *case_body = case_stmt->get_body();
    // unparseStatementFromTokenStream (case_stmt, case_body,
    // e_token_subsequence_start, e_leading_whitespace_start);
    if (case_body->isCompilerGenerated() == true) {
      SgBasicBlock *basicBlock = isSgBasicBlock(case_body);
      ASSERT_not_null(basicBlock);

      // DQ (3/28/2017): Eliminate warning about unused variable from Clang.
      // SgStatementPtrList::iterator first =
      // basicBlock->get_statements().begin();

      // DQ (1/22/2015): Since there is not token information inside of a
      // compiler-generated SgBasicBlock, we have to output the whole
      // SgCaseOptionStmt as it's associated tokens.
      // unparseStatementFromTokenStream (case_stmt, e_token_subsequence_start,
      // e_token_subsequence_end);
      unparseStatementFromTokenStream(case_stmt, e_token_subsequence_start,
                                      e_trailing_whitespace_start, info);
    } else {
      unparseStatementFromTokenStream(case_stmt, case_body,
                                      e_token_subsequence_start,
                                      e_leading_whitespace_start, info);
    }
  }

  // DQ (1/3/2018): Put back this original behavior, because the case option
  // statment must be a compound statement (just like a label statement, see
  // test2017_20.c). DQ (12/20/2017): Comment this out to experiment with
  // alternative support for switch (part of new duff's device support). At the
  // very least, commenting this out permis the cases to be adjusted to have
  // defined bodies later (if that ultimately makes sense).
  // if(case_stmt->get_body())
  // if ( (case_stmt->get_body() != NULL) && !info.SkipBasicBlock())
  if ((case_stmt->get_body() != NULL) && !info.SkipBasicBlock()) {
    unparseStatement(case_stmt->get_body(), info);
  }
}

void Unparse_ExprStmt::unparseTryStmt(SgStatement *stmt, SgUnparse_Info &info) {
  SgTryStmt *try_stmt = isSgTryStmt(stmt);
  ASSERT_not_null(try_stmt);

  curprint(string("try "));

  unp->cur.format(try_stmt->get_body(), info, FORMAT_BEFORE_NESTED_STATEMENT);
  unparseStatement(try_stmt->get_body(), info);
  unp->cur.format(try_stmt->get_body(), info, FORMAT_AFTER_NESTED_STATEMENT);

  SgStatementPtrList::iterator i = try_stmt->get_catch_statement_seq().begin();
  while (i != try_stmt->get_catch_statement_seq().end()) {
    unparseStatement(*i, info);
    i++;
  }
}

void Unparse_ExprStmt::unparseCatchStmt(SgStatement *stmt,
                                        SgUnparse_Info &info) {
  SgCatchOptionStmt *catch_statement = isSgCatchOptionStmt(stmt);
  ASSERT_not_null(catch_statement);

  curprint(string("catch ") + "(");
  if (catch_statement->get_condition()) {
    SgUnparse_Info ninfo(info);
    ninfo.set_inVarDecl();
    // DQ (5/6/2004): this does not unparse correctly if the ";" is included
    ninfo.set_SkipSemiColon();
    ninfo.set_SkipClassSpecifier();
    unparseStatement(catch_statement->get_condition(), ninfo);
  }

  curprint(string(")"));
  // if (catch_statement->get_condition() == NULL) prevnode = catch_statement;

  unp->cur.format(catch_statement->get_body(), info,
                  FORMAT_BEFORE_NESTED_STATEMENT);
  unparseStatement(catch_statement->get_body(), info);
  unp->cur.format(catch_statement->get_body(), info,
                  FORMAT_AFTER_NESTED_STATEMENT);
}

void Unparse_ExprStmt::unparseDefaultStmt(SgStatement *stmt,
                                          SgUnparse_Info &info) {
  SgDefaultOptionStmt *default_stmt = isSgDefaultOptionStmt(stmt);
  ASSERT_not_null(default_stmt);

  bool saved_unparsedPartiallyUsingTokenStream =
      info.unparsedPartiallyUsingTokenStream();

  // DQ (12/28/2014): Test for if we have unparsed partially using the token
  // stream.
  if (saved_unparsedPartiallyUsingTokenStream == false) {
    curprint("default:");
  } else {
    SgStatement *default_body = default_stmt->get_body();
    unparseStatementFromTokenStream(default_stmt, default_body,
                                    e_token_subsequence_start,
                                    e_leading_whitespace_start, info);
  }

  // DQ (1/3/2018): Put back this original behavior, because the case option
  // statment must be a compound statement (just like a label statement, see
  // test2017_20.c). DQ (12/20/2017): Comment this out to experiment with
  // alternative support for switch (part of new duff's device support). At the
  // very least, commenting this out permis the cases to be adjusted to have
  // defined bodies later (if that ultimately makes sense).
  // if(default_stmt->get_body())
  if ((default_stmt->get_body() != NULL) && !info.SkipBasicBlock()) {
    unparseStatement(default_stmt->get_body(), info);
  }
}

void Unparse_ExprStmt::unparseBreakStmt(SgStatement *stmt, SgUnparse_Info &) {
  SgBreakStmt *break_stmt = isSgBreakStmt(stmt);
  ASSERT_not_null(break_stmt);

  curprint("break;");
}

void Unparse_ExprStmt::unparseContinueStmt(SgStatement *stmt,
                                           SgUnparse_Info &) {
  SgContinueStmt *continue_stmt = isSgContinueStmt(stmt);
  ASSERT_not_null(continue_stmt);

  curprint("continue; ");
}

void Unparse_ExprStmt::unparseReturnStmt(SgStatement *stmt,
                                         SgUnparse_Info &info) {
  SgReturnStmt *return_stmt = isSgReturnStmt(stmt);
  ASSERT_not_null(return_stmt);

  std::string keyword = "return";
  if (AstValueAttribute<std::string> *keyword_attr =
          dynamic_cast<AstValueAttribute<std::string> *>(
              return_stmt->getAttribute(
                  Rose::kCoroutineKeywordAttributeName))) {
    keyword = keyword_attr->get();
  }
  curprint(keyword);

  SgUnparse_Info ninfo(info);

  // DQ (6/4/2011): Set this in case the initializer is an expression that
  // requires name qualification (e.g. SgConstructorInitializer).  See
  // test2005_42.C for an example.
  // ninfo.set_reference_node_for_qualification(return_stmt);

  if (SgExpression *return_expr = return_stmt->get_expression();
      return_expr != nullptr && isSgNullExpression(return_expr) == nullptr) {
    curprint(" ");

    // DQ (2/8/2019): Restricting output of definitions in the return statement.
    ninfo.set_SkipDefinition();

    // DQ (2/8/2019): Double check that these are all set.
    ROSE_ASSERT(ninfo.SkipClassDefinition() == true);
    ROSE_ASSERT(ninfo.SkipEnumDefinition() == true);
    ROSE_ASSERT(ninfo.SkipDefinition() == true);

    unparseExpression(return_expr, ninfo);
  }

  if (!ninfo.SkipSemiColon()) {
    curprint(";");
  }
}

void Unparse_ExprStmt::unparseGotoStmt(SgStatement *stmt,
                                       SgUnparse_Info &info) {
  SgGotoStatement *goto_stmt = isSgGotoStatement(stmt);
  ASSERT_not_null(goto_stmt);

  if (goto_stmt->get_label() != NULL) {
    // DQ (11/22/2017): Original code.
    curprint(string("goto ") + goto_stmt->get_label()->get_label().str());

  } else {
    // DQ (11/22/2017): Added suport for GNU extension for computed goto.
    curprint("goto *");
    SgExpression *selector_expression = goto_stmt->get_selector_expression();
    ASSERT_not_null(selector_expression);

    SgUnparse_Info ninfo(info);
    unparseExpression(selector_expression, ninfo);
  }

  if (!info.SkipSemiColon()) {
    curprint(string(";"));
  }
}

static bool isOutputAsmOperand(SgAsmOp *asmOp) {
  // There are two way of evaluating if an SgAsmOp is an output operand,
  // depending of if we are using the specific mechanism that knows
  // records register details or the more general mechanism that records
  // the registers as strings.  The string based mechanism lack precision
  // and would require parsing to retrive the instruction details, but it
  // is instruction set independent.  The more precise mechanism records
  // the specific register codes and could in the future be interpreted
  // by tooling that understands ISA-specific register details.

  return (asmOp->get_recordRawAsmOperandDescriptions() == true)
             ? (asmOp->get_isOutputOperand() == true)
             : (asmOp->get_modifiers() & SgAsmOp::e_output);
}

static std::string asm_escapeString(const std::string &s) {
  // DQ (2/4/2014): We need a special version of this function for unparsing the
  // asm strings. The version of escapeString in util will expand '\' to be '\\'
  // and this should not be done to the "\n" and "\t" substrings.

  std::string result;
  for (size_t i = 0; i < s.length(); ++i) {
    switch (s[i]) {
    case '"':
      result += "\\\"";
      break;
    case '\a':
      result += "\\a";
      break;
    case '\f':
      result += "\\f";
      break;
    case '\n':
      result += "\\n";
      break;
    case '\r':
      result += "\\r";
      break;
    case '\t':
      result += "\\t";
      break;
    case '\v':
      result += "\\v";
      break;
    default:
      if (isprint(s[i])) {
        result.push_back(s[i]);
      } else {
        std::ostringstream stream;
        stream << '\\';
        stream << std::setw(3) << std::setfill('0') << std::oct
               << (unsigned)(unsigned char)(s[i]);
        result += stream.str();
      }
      break;
    }
  }

  return result;
}

void Unparse_ExprStmt::unparseAsmStmt(SgStatement *stmt, SgUnparse_Info &info) {
  // This function is called as part of the handling of the C "asm"
  // statement.  The "asm" statement supports inline assembly in C.
  // These sorts of statements are not common in most user code
  // (except embedded code), but are common in many system header files.

  SgAsmStmt *asm_stmt = isSgAsmStmt(stmt);
  ASSERT_not_null(asm_stmt);

#define ASM_DEBUGGING 0

#if ASM_DEBUGGING
  printf("In unparseAsmStmt(): stmt = %p = %s \n", stmt,
         stmt->class_name().c_str());
#endif

  // DQ (5/23/2015): The p_skip_unparse_asm_commands data member has been
  // changed to be a static data member so that we can support ASM statments
  // hidden in AST islands (AST subtrees hidden in types and thus not connected
  // to the AST (since types are shared).  The use of the typeof operator in
  // conjunction with the GNU statemnet expression can permit this
  // configuration.

  // DQ (5/19/2015): Note that sourceFile will be NULL in the case where the asm
  // statement is in a GNU statement expression in a typeof operator. Not clear
  // yet what to do about this case. See test2015_141.c for an example of this.
  // One solution might be to make the skip_unparse_asm_commands variable a
  // static data member. SgSourceFile* sourceFile =
  // TransformationSupport::getSourceFile(stmt); ASSERT_not_null(sourceFile);

  // DQ (1/10/2009): The C language ASM statements are providing significant
  // trouble, they are frequently machine specific and we are compiling then on
  // architectures for which they were not designed.  This option allows then to
  // be read, constructed in the AST to support analysis but not unparsed in the
  // code given to the backend compiler, since this can fail. (See test2007_20.C
  // from Linux Kernel for an example). if
  // (sourceFile->get_skip_unparse_asm_commands() == true)
  if (SgSourceFile::get_skip_unparse_asm_commands() == true) {
    // This is a case were we skip the unparsing of the C language ASM
    // statements, because while we can read then into the AST to support
    // analysis, we can not always output them correctly. This subject requires
    // additional work.

    printf(
        "Warning: Sorry, C language ASM statement skipped (parsed and built in "
        "AST, but not output by the code generation phase (unparser)) \n");

    // DQ (10/25/2012): This needs to be made a bit more sophisticated (tailored
    // to the type of associated scope in the source code). DQ (10/23/2012): If
    // we skip the ASM statment then include an expression statement to take
    // it's place. curprint ( "/* C language ASM statement skipped (in code
    // generation phase) */ "); curprint ( "/* C language ASM statement skipped
    // (in code generation phase) */ \"compiled using
    // -rose:skip_unparse_asm_commands\"; ");
    SgScopeStatement *associatedScope = stmt->get_scope();
    ASSERT_not_null(associatedScope);

    if (associatedScope->containsOnlyDeclarations() == true) {
      // Output a simple string.
      curprint(
          "/* C language ASM statement skipped (in code generation phase) */ ");
    } else {
      // We might need a expression statement to properly close off another
      // statement (e.g. a "if" statement).
      curprint("/* C language ASM statement skipped (in code generation phase) "
               "*/ \"compiled using -rose:skip_unparse_asm_commands\"; ");
    }

    return;
  }

  // Output the "asm" keyword.
  // DQ (8/31/2013): We have to output either "__asm__" or "asm".

// DQ (2/25/2014): Note that the 4.2.4 compiler will define both
// BACKEND_C_COMPILER_SUPPORTS_ASM and
// BACKEND_C_COMPILER_SUPPORTS_UNDERSCORE_ASM So we need to use another macro
// BACKEND_C_COMPILER_SUPPORTS_LONG_STRING_ASM that will work uniformally on
// both 4.4.7 and 4.2.4 versions of the GNU compiler.  This is truely strange
// behavior. DQ (2/25/2014): This is the new support for use of "asm" or
// "__asm__" (which should maybe be refactored). #if
// (defined(BACKEND_C_COMPILER_SUPPORTS_ASM) &&
// defined(BACKEND_C_COMPILER_SUPPORTS_UNDERSCORE_ASM))
//    #error "Error: BACKEND_C_COMPILER_SUPPORTS_ASM and
//    BACKEND_C_COMPILER_SUPPORTS_UNDERSCORE_ASM are both defined!"
// #endif
#if (defined(BACKEND_C_COMPILER_SUPPORTS_LONG_STRING_ASM) &&                   \
     defined(BACKEND_C_COMPILER_SUPPORTS_UNDERSCORE_ASM))
  // DQ (2/26/2014): Allow the CMake tests to pass for now.
#warning                                                                       \
    "Warning: BACKEND_C_COMPILER_SUPPORTS_LONG_STRING_ASM and BACKEND_C_COMPILER_SUPPORTS_UNDERSCORE_ASM are both defined!"
#endif

// #ifdef BACKEND_C_COMPILER_SUPPORTS_ASM
#ifdef BACKEND_C_COMPILER_SUPPORTS_LONG_STRING_ASM
  curprint("asm ");
#else
#ifdef BACKEND_C_COMPILER_SUPPORTS_UNDERSCORE_ASM
  curprint("__asm__ ");
#else
#warning                                                                       \
    "Warning: either BACKEND_C_COMPILER_SUPPORTS_LONG_STRING_ASM or BACKEND_C_COMPILER_SUPPORTS_UNDERSCORE_ASM should be defined (but not both)!"

  // DQ (2/26/2014): Allow the default behavior on CMake build systems to use
  // the GNU compiler specific version or "__asm__".
  curprint("__asm__ ");
#endif
#endif

  curprint("(");

  // DQ (7/22/2006): This IR node has been changed to have a list of SgAsmOp IR
  // nodes unparseExpression(asm_stmt->get_expr(), info);

  // printf ("unparsing asm statement = %ld
  // \n",asm_stmt->get_operands().size()); Process the asm template (always the
  // first operand)
  string asmTemplate = asm_stmt->get_assemblyCode();

#if ASM_DEBUGGING
  printf("In unparseAsmStmt(): asmTemplate.length()      = %" PRIuPTR " \n",
         (size_t)asmTemplate.length());
#endif

#if ASM_DEBUGGING
  printf("In unparseAsmStmt(): asmTemplate               = %s \n",
         asmTemplate.c_str());
  printf("In unparseAsmStmt(): escapeString(asmTemplate) = %s \n",
         asm_escapeString(asmTemplate).c_str());
#endif

  // DQ (2/4/2014): We don't want to escape this string (see test2014_83.c,
  // test2014_84.c, and test2014_85.c).
  curprint("\"" + asm_escapeString(asmTemplate) + "\"");
  // curprint("\"" + asmTemplate + "\"");

#if ASM_DEBUGGING
  printf("In unparseAsmStmt(): asm_stmt->get_useGnuExtendedFormat() = %s \n",
         asm_stmt->get_useGnuExtendedFormat() ? "true" : "false");
#endif

  if (asm_stmt->get_useGnuExtendedFormat()) {
    size_t numOutputOperands = 0;
    size_t numInputOperands = 0;

    // Count the number of input vs. output operands
    for (SgExpressionPtrList::const_iterator i =
             asm_stmt->get_operands().begin();
         i != asm_stmt->get_operands().end(); ++i) {
      SgAsmOp *asmOp = isSgAsmOp(*i);
      ROSE_ASSERT(asmOp);
#if ASM_DEBUGGING
      printf("asmOp->get_modifiers() = %d SgAsmOp::e_output = %d "
             "asmOp->get_isOutputOperand() = %s \n",
             (int)asmOp->get_modifiers(), (int)SgAsmOp::e_output,
             asmOp->get_isOutputOperand() ? "true" : "false");
      printf("asmOp->get_recordRawAsmOperandDescriptions() = %s \n",
             asmOp->get_recordRawAsmOperandDescriptions() ? "true" : "false");
#endif
      // if (asmOp->get_modifiers() & SgAsmOp::e_output)
      // if ( (asmOp->get_modifiers() & SgAsmOp::e_output) ||
      // (asmOp->get_isOutputOperand() == true) )
      if (isOutputAsmOperand(asmOp) == true) {
        ++numOutputOperands;
#if ASM_DEBUGGING
        printf("Marking as an output operand! \n");
#endif
      } else {
        ++numInputOperands;
#if ASM_DEBUGGING
        printf("Marking as an input operand! \n");
#endif
      }
    }

    size_t numClobbers = asm_stmt->get_clobberRegisterList().size();

#if ASM_DEBUGGING
    printf("In unparseAsmStmt(): numClobbers = %" PRIuPTR " \n", numClobbers);
#endif

#if ASM_DEBUGGING
    printf("In unparseAsmStmt(): numOutputOperands = %" PRIuPTR
           " numInputOperands = %" PRIuPTR " numClobbers = %" PRIuPTR " \n",
           numOutputOperands, numInputOperands, numClobbers);
#endif

    // DQ (2/4/2014): Adding initializer (to make me feel better about this
    // code).
    bool first = false;
    if (numInputOperands == 0 && numOutputOperands == 0 && numClobbers == 0) {
#if ASM_DEBUGGING
      printf("In unparseAsmStmt(): (numInputOperands == 0 && numOutputOperands "
             "== 0 && numClobbers == 0): goto donePrintingConstraints \n");
#endif
      // DQ (9/14/2013): Output required if we branch to label (see
      // test2013_72.c).
      curprint(" :: "); // Start of output operands

      goto donePrintingConstraints;
    }
    curprint(" : "); // Start of output operands

#if ASM_DEBUGGING
    curprint(" /* asm output operands */ "); // Debugging output
#endif

    // Record if this is the first operand so that we can surpress the ","
    first = true;
    for (SgExpressionPtrList::const_iterator i =
             asm_stmt->get_operands().begin();
         i != asm_stmt->get_operands().end(); ++i) {
      SgAsmOp *asmOp = isSgAsmOp(*i);
      ASSERT_not_null(asmOp);
      // if (asmOp->get_modifiers() & SgAsmOp::e_output)
      // if ( (asmOp->get_modifiers() & SgAsmOp::e_output) ||
      // (asmOp->get_isOutputOperand() == true) )
      if (isOutputAsmOperand(asmOp) == true) {
        if (!first)
          curprint(", ");
        first = false;
        unparseExpression(asmOp, info);
      }
    }

    if (numInputOperands == 0 && numClobbers == 0) {
#if ASM_DEBUGGING
      printf("In unparseAsmStmt(): (numInputOperands == 0 && numClobbers == "
             "0): goto donePrintingConstraints \n");
#endif
      // DQ (9/14/2013): Output required if we branch to label (see
      // test2013_72.c, but this is not a good example). curprint(" : "); //
      // Start of output operands

      goto donePrintingConstraints;
    }
    curprint(" : "); // Start of input operands
#if ASM_DEBUGGING
    curprint(" /* asm input operands */ "); // Debugging output
#endif
    first = true;
    for (SgExpressionPtrList::const_iterator i =
             asm_stmt->get_operands().begin();
         i != asm_stmt->get_operands().end(); ++i) {
      SgAsmOp *asmOp = isSgAsmOp(*i);
      ASSERT_not_null(asmOp);
      // if (!(asmOp->get_modifiers() & SgAsmOp::e_output))
      if (isOutputAsmOperand(asmOp) == false) {
        if (!first)
          curprint(", ");
        first = false;
        unparseExpression(asmOp, info);
      }
    }

    if (numClobbers == 0) {
#if ASM_DEBUGGING
      printf("In unparseAsmStmt(): (numClobbers == 0): goto "
             "donePrintingConstraints \n");
#endif
      // DQ (9/14/2013): Output required if we branch to label (see
      // test2013_72.c, but this is not a good example). curprint(" : "); //
      // Start of output operands

      goto donePrintingConstraints;
    }

    curprint(" : "); // Start of clobbers

#if ASM_DEBUGGING
    curprint(" /* asm clobbers */ "); // Debugging output
#endif
    first = true;
    for (SgAsmStmt::AsmRegisterNameList::const_iterator i =
             asm_stmt->get_clobberRegisterList().begin();
         i != asm_stmt->get_clobberRegisterList().end(); ++i) {
      if (!first)
        curprint(", ");
      first = false;
      curprint("\"" + unparse_register_name(*i) + "\"");
    }

  donePrintingConstraints: {}

#if ASM_DEBUGGING
    printf("In unparseAsmStmt(): base of conditional block: "
           "asm_stmt->get_useGnuExtendedFormat() = %s \n",
           asm_stmt->get_useGnuExtendedFormat() ? "true" : "false");
#endif
  }

  curprint(string(")"));

  if (!info.SkipSemiColon()) {
    curprint(string(";"));
  }

#if ASM_DEBUGGING
  printf("Leaving unparseAsmStmt(): stmt = %p = %s \n", stmt,
         stmt->class_name().c_str());
#endif
}

// DQ 11/3/2014): Adding C++11 templated typedef declaration support.
void Unparse_ExprStmt::unparseTemplateTypedefDeclaration(SgStatement *stmt,
                                                         SgUnparse_Info &info) {
  SgTemplateTypedefDeclaration *templateTypedef_stmt =
      isSgTemplateTypedefDeclaration(stmt);
  ASSERT_not_null(templateTypedef_stmt);

#define DEBUG_TEMPLATE_TYPEDEF 0

#if DEBUG_TEMPLATE_TYPEDEF
  printf("In unparseTemplateTypeDefStmt() = %p \n", templateTypedef_stmt);
#endif
#if DEBUG_TEMPLATE_TYPEDEF
  curprint(" /* Calling "
           "unparseTemplateDeclarationStatment_support<"
           "SgTemplateTypedefDeclaration>() */ ");
#endif

  // DQ (2/19/2019): The support for unparsing a SgTemplateTypedefDeclaration is
  // different enough that we always use the specialized path here.

  // Unparse_ExprStmt::unparseTemplateParameterList( const
  // SgTemplateParameterPtrList & templateParameterList, SgUnparse_Info& info,
  // bool is_template_header) bool is_template_header = false;
  // unparseTemplateParameterList(templateTypedef_stmt->get_templateParameters(),info,is_template_header);
  unparseTemplateHeader<SgTemplateTypedefDeclaration>(templateTypedef_stmt,
                                                      info);

  // DQ (2/19/2019): I think this is how we detect the template parameters.
  if (templateTypedef_stmt->get_templateParameters().empty() == false) {
    curprint(" /* unparse template paremeters */ ");
  }

  // DQ (2/19/2019): Not clear that I want the extra "\n".
  curprint("\nusing ");

#if DEBUG_TEMPLATE_TYPEDEF
  printf("In unparseTemplateTypeDefStmt(): templateTypedef_stmt->get_name() = "
         "%s \n",
         templateTypedef_stmt->get_name().str());
#endif

  curprint(templateTypedef_stmt->get_name().str());

  curprint(" = ");

  SgType *base_type = templateTypedef_stmt->get_base_type();
  ASSERT_not_null(base_type);

#if DEBUG_TEMPLATE_TYPEDEF
  printf("In unparseTemplateTypeDefStmt(): base_type = %p = %s \n", base_type,
         base_type->class_name().c_str());
#endif

  SgUnparse_Info ninfo(info);

#if DEBUG_TEMPLATE_TYPEDEF
  printf("In unparseTemplateTypeDefStmt(): "
         "templateTypedef_stmt->get_declaration() = %p \n",
         templateTypedef_stmt->get_declaration());
#endif
#if DEBUG_TEMPLATE_TYPEDEF
  printf("In unparseTemplateTypeDefStmt(): set "
         "reference_node_for_qualification: templateTypedef_stmt = %p = %s \n",
         templateTypedef_stmt, templateTypedef_stmt->class_name().c_str());
#endif
  ninfo.set_reference_node_for_qualification(templateTypedef_stmt);

  // DQ (2/19/2019): Cxx_tests/test2019_153.C demonstrates that a class
  // declaration can be define in the C++11 SgTemplateTypedefDeclaration.
  // ROSE_ASSERT(templateTypedef_stmt->get_declaration() == NULL);

  if (templateTypedef_stmt->get_declaration() == NULL) {
    ninfo.set_SkipClassDefinition();
    ninfo.set_SkipEnumDefinition();
  }

  unp->u_type->unparseType(base_type, ninfo);

  curprint(";");

#if DEBUG_TEMPLATE_TYPEDEF
  printf("Leaving unparseTemplateTypeDefStmt() = %p \n", templateTypedef_stmt);
#endif
}

void Unparse_ExprStmt::unparseNonrealDecl(SgStatement *stmt,
                                          SgUnparse_Info &info) {
  SgNonrealDecl *nrdecl = isSgNonrealDecl(stmt);
  ASSERT_not_null(nrdecl);

  if (nrdecl->get_is_concept()) {
    const SgTemplateParameterPtrList &params = nrdecl->get_tpl_params();
    if (!params.empty()) {
      curprint("template ");
      Unparse_ExprStmt::unparseTemplateParameterList(params, info, true);
      curprint("\n");
    }

    curprint("concept ");
    curprint(nrdecl->get_name().str());

    curprint(" = ");
    if (SgExpression *constraint = nrdecl->get_conceptConstraint()) {
      SgUnparse_Info constraint_info(info);
      constraint_info.set_SkipClassDefinition();
      constraint_info.set_SkipEnumDefinition();
      unparseExpression(constraint, constraint_info);
    } else {
      curprint("true");
    }

    if (!info.SkipSemiColon()) {
      curprint(";");
    }
    return;
  }

  printf("WARNING: Asked to unparse a non-real declaration!\n");
  curprint(nrdecl->get_name().str());
}

void Unparse_ExprStmt::unparseTypeDefStmt(SgStatement *stmt,
                                          SgUnparse_Info &info) {
  SgTypedefDeclaration *typedef_stmt = isSgTypedefDeclaration(stmt);
  ASSERT_not_null(typedef_stmt);

#define DEBUG_TYPEDEF_DECLARATIONS 0

#if DEBUG_TYPEDEF_DECLARATIONS
  printf("TOP of unp->u_type->unparseTypeDefStmt() = %p \n", typedef_stmt);
  typedef_stmt->get_file_info()->display(
      "In unp->u_type->unparseTypeDefStmt(): debug");
#endif
#if DEBUG_TYPEDEF_DECLARATIONS
  curprint("\n /* In unp->u_type->unparseTypeDefStmt() */ \n");
#endif

  // DQ (10/5/2004): This is the explicitly set boolean value which indicates
  // that a class declaration is buried inside the current variable declaration
  // (e.g. struct A { int x; } a;).  In this case we have to output the base
  // type with its definition.
  bool outputTypeDefinition =
      typedef_stmt->get_typedefBaseTypeContainsDefiningDeclaration();
  if (outputTypeDefinition == true && (info.SkipClassDefinition() == true ||
                                       info.SkipEnumDefinition() == true)) {
    outputTypeDefinition = false;
  }

#if DEBUG_TYPEDEF_DECLARATIONS
  printf(
      "In unparseTypeDefStmt(): typedef_stmt = %p outputTypeDefinition = %s \n",
      typedef_stmt, (outputTypeDefinition == true) ? "true" : "false");
#endif

  if (!info.inEmbeddedDecl()) {
#if OUTPUT_DEBUGGING_FUNCTION_INTERNALS || 0
    curprint("\n/* NOT an embeddedDeclaration */ \n");
#endif
    SgClassDefinition *cdefn = isSgClassDefinition(typedef_stmt->get_parent());
    if (cdefn && cdefn->get_declaration()->get_class_type() ==
                     SgClassDeclaration::e_class)
      info.set_CheckAccess();
    // printDebugInfo("entering unp->u_sage->printSpecifier", true);
    unp->u_sage->printSpecifier(typedef_stmt, info);
    info.unset_CheckAccess();
  } else {
#if OUTPUT_DEBUGGING_FUNCTION_INTERNALS || 0
    curprint("\n/* Found an embeddedDeclaration */ \n");
#endif
  }

  SgUnparse_Info ninfo(info);

  // DQ (10/10/2006): Do output any qualified names (particularly for
  // non-defining declarations). ninfo.set_forceQualifiedNames();

  // DQ (10/5/2004): This controls the unparsing of the class definition
  // when unparsing the type within this variable declaration.
  if (outputTypeDefinition == true) {
    // printf ("Output the full definition as a basis for the typedef base type
    // \n"); DQ (10/5/2004): If this is a defining declaration then make sure
    // that we don't skip the definition
    ROSE_ASSERT(ninfo.SkipClassDefinition() == false);

    // DQ (12/22/2005): Enum definition should be handled here as well
    ROSE_ASSERT(ninfo.SkipEnumDefinition() == false);

    // DQ (10/14/2006): As part of new implementation of qualified names we now
    // default to the generation of all qualified names unless they are skipped.
    ninfo.set_SkipQualifiedNames();
    // curprint ( string("\n/* Case of typedefs for outputTypeDefinition == true
    // */\n ";
  } else {
    // printf ("Skip output of the full definition as a basis for the typedef
    // base type \n"); DQ (10/5/2004): If this is a non-defining declaration
    // then skip the definition
    ninfo.set_SkipClassDefinition();
    ROSE_ASSERT(ninfo.SkipClassDefinition() == true);

    // DQ (12/22/2005): Enum definition should be handled here as well
    ninfo.set_SkipEnumDefinition();
    ROSE_ASSERT(ninfo.SkipEnumDefinition() == true);

    // DQ (10/14/2006): Force output any qualified names (particularly for
    // non-defining declarations). This is a special case for types of variable
    // declarations. ninfo.set_forceQualifiedNames(); curprint ( string("\n/*
    // Case of typedefs, should we forceQualifiedNames -- outputTypeDefinition
    // == false  */\n ";
  }

  if (typedef_stmt->get_typedef_type() == SgTypedefDeclaration::e_using) {
    SgType *btype = typedef_stmt->get_base_type();
    ASSERT_not_null(btype);

    curprint("using ");
    curprint(typedef_stmt->get_name().str());
    curprint(" = ");

    SgUnparse_Info ninfo_for_type(ninfo);
    ninfo_for_type.set_declstatement_ptr(typedef_stmt);
    if (typedef_stmt->get_requiresGlobalNameQualificationOnType() == true) {
      ninfo_for_type.set_requiresGlobalNameQualification();
    }

    SgDeclarationStatement *declaration = typedef_stmt->get_declaration();
    if (declaration != NULL && isSgEnumDeclaration(declaration) != NULL &&
        isSgTemplateInstantiationDecl(declaration) == NULL) {
      ninfo_for_type.set_reference_node_for_qualification(declaration);
    } else {
      // Qualify using-alias base types relative to alias declaration context.
      // This preserves required owner qualifiers for nested types.
      ninfo_for_type.set_reference_node_for_qualification(typedef_stmt);
    }

    ninfo_for_type.set_name_qualification_length(
        typedef_stmt->get_name_qualification_length_for_base_type());
    ninfo_for_type.set_global_qualification_required(
        typedef_stmt->get_global_qualification_required_for_base_type());
    // Alias declarations should preserve canonical spelling without forcing
    // struct/class elaboration keywords.
    ninfo_for_type.set_type_elaboration_required(false);
    ninfo_for_type.set_SkipClassSpecifier();

    unp->u_type->unparseType(btype, ninfo_for_type);

    if (outputTypeDefinition == true) {
      unparseTypeAttributes(typedef_stmt);
    }

    unp->u_sage->printAttributes(typedef_stmt, info);

    if (!info.SkipSemiColon()) {
      curprint(";");
    }

    return;
  }

  // Note that typedefs of function pointers and member function pointers
  // are quite different from ordinary typedefs and so should be handled
  // separately.

  // First look for a pointer to a function
  SgPointerType *pointerToType = isSgPointerType(typedef_stmt->get_base_type());

  SgFunctionType *functionType = NULL;
  if (pointerToType != NULL)
    functionType = isSgFunctionType(pointerToType->get_base_type());

  // DQ (2/3/2019): Adding support to unparse SgPointerMemberType with
  // parenthesis now that they have been discovered to not be required in the
  // unparse type code for SgPointerMemberType.  See Cxx11_tests/test2019_77.C.
  SgPointerMemberType *pointerToMemberType =
      isSgPointerMemberType(typedef_stmt->get_base_type());

  // DQ (9/15/2004): Added to support typedefs of member pointers
  SgMemberFunctionType *pointerToMemberFunctionType =
      isSgMemberFunctionType(typedef_stmt->get_base_type());

  // DQ (2/14/2019): Adding name qualification support for C++11 enum
  // declarations in typedef types.
  SgEnumType *enumType = isSgEnumType(typedef_stmt->get_base_type());
  if (enumType != NULL) {
    SgDeclarationStatement *declarationReference =
        typedef_stmt->get_declaration();
    ASSERT_not_null(declarationReference);
    SgEnumDeclaration *enumDeclaration =
        isSgEnumDeclaration(declarationReference);
    ASSERT_not_null(enumDeclaration);
    SgEnumDeclaration *definingEnumDeclaration =
        isSgEnumDeclaration(enumDeclaration->get_definingDeclaration());

#if DEBUG_TYPEDEF_DECLARATIONS
    printf("enumDeclaration->get_name() = %s \n",
           enumDeclaration->get_name().str());
#endif
#if DEBUG_TYPEDEF_DECLARATIONS
    curprint(string("\n/* In unparseTypeDefStmt: enum name                     "
                    "             = ") +
             enumDeclaration->get_name().str() + " */ \n ");
    curprint(string("\n/* In unparseTypeDefStmt: enumDeclaration != "
                    "definingEnumDeclaration = ") +
             ((enumDeclaration != definingEnumDeclaration) ? "true" : "false") +
             " */ \n ");
#endif
    // DQ (2/20/2019): If this is not the defining declaration referenced
    // through the declarationReference then use the usual method for name
    // qualification via the base type.
    if (enumDeclaration != definingEnumDeclaration) {
#if DEBUG_TYPEDEF_DECLARATIONS
      printf("internal declarationReference != definingEnumDeclaration: so use "
             "the name qualification via the base type \n");
#endif
      enumType = NULL;
    } else {
      // If this is the defining declaration, then if it is an anonymous class
      // then output as a type.
#if DEBUG_TYPEDEF_DECLARATIONS
      printf("enumDeclaration->get_name() = %s \n",
             enumDeclaration->get_name().str());
#endif
      bool isAnonymousName =
          hasGeneratedAnonymousNamePrefix(enumDeclaration->get_name());
      if (isAnonymousName == true) {
#if DEBUG_TYPEDEF_DECLARATIONS
        printf("internal declarationReference == definingEnumDeclaration: "
               "isAnonymousName == true: so output enum declaration via the "
               "base type \n");
#endif
        enumType = NULL;
      }
    }
  }

  // DQ (2/18/2019): Adding name qualification support for class declarations in
  // typedef types (declared in other scopes).
  SgClassType *classType = isSgClassType(typedef_stmt->get_base_type());
  if (classType != NULL) {
    SgDeclarationStatement *declarationReference =
        typedef_stmt->get_declaration();
    ASSERT_not_null(declarationReference);
    SgClassDeclaration *classDeclaration =
        isSgClassDeclaration(declarationReference);
    ASSERT_not_null(classDeclaration);
    SgClassDeclaration *definingClassDeclaration =
        isSgClassDeclaration(classDeclaration->get_definingDeclaration());

#if DEBUG_TYPEDEF_DECLARATIONS
    printf("classDeclaration->get_name() = %s \n",
           classDeclaration->get_name().str());
#endif
#if DEBUG_TYPEDEF_DECLARATIONS
    curprint(string("\n/* In unparseTypeDefStmt: class name                    "
                    "               = ") +
             classDeclaration->get_name().str() + " */ \n ");
    curprint(
        string("\n/* In unparseTypeDefStmt: classDeclaration != "
               "definingClassDeclaration = ") +
        ((classDeclaration != definingClassDeclaration) ? "true" : "false") +
        " */ \n ");
#endif
    // DQ (2/20/2019): If this is not the defining declaration referenced
    // through the declarationReference then use the usual method for name
    // qualification via the base type.
    if (classDeclaration != definingClassDeclaration) {
#if DEBUG_TYPEDEF_DECLARATIONS
      printf("internal declarationReference != definingClassDeclaration: so "
             "use the name qualification via the base type \n");
#endif
      classType = NULL;
    } else {
      // If this is the defining declaration, then if it is an anonymous class
      // then output as a type.
#if DEBUG_TYPEDEF_DECLARATIONS
      printf("classDeclaration->get_name() = %s \n",
             classDeclaration->get_name().str());
#endif
      bool isAnonymousName =
          hasGeneratedAnonymousNamePrefix(classDeclaration->get_name());
      if (isAnonymousName == true) {
#if DEBUG_TYPEDEF_DECLARATIONS
        printf("internal declarationReference == definingClassDeclaration: "
               "isAnonymousName == true: so output class declaration via the "
               "base type \n");
#endif
        classType = NULL;
      }
    }
  }

#if DEBUG_TYPEDEF_DECLARATIONS
  printf("In unp->u_type->unparseTypedef: functionType                = %p \n",
         functionType);
  printf("In unp->u_type->unparseTypedef: pointerToMemberType         = %p \n",
         pointerToMemberType);
  printf("In unp->u_type->unparseTypedef: pointerToMemberFunctionType = %p \n",
         pointerToMemberFunctionType);
  printf("In unp->u_type->unparseTypedef: enumType                    = %p \n",
         enumType);
  printf("In unp->u_type->unparseTypedef: classType                   = %p \n",
         classType);
#endif

  // DQ (9/22/2004): It is not clear why we need to handle this case with
  // special code. We are only putting out the return type of the function type
  // (for functions or member functions). It seems that the reason we handle
  // function pointers separately is that typedefs of non function pointers
  // could include the complexity of class declarations with definitions and
  // separating the code for function pointers allows for easier debugging. When
  // typedefs of defining class declarations is fixed we might be able to unify
  // these separate cases.

  // DQ (2/14/2019): Adding name qualification support for C++11 enum
  // declarations in typedef types. DQ (2/3/2019): See if this is a better
  // branch for handling the SgPointerMemberType. This handles pointers to
  // functions and member function (but not pointers to members!) if (
  // (functionType != NULL) || (pointerToMemberType != NULL) ) if (
  // (functionType != NULL) || (pointerToMemberFunctionType != NULL) ) if (
  // (functionType != NULL) || (pointerToMemberFunctionType != NULL) ||
  // (pointerToMemberType != NULL) ) if ( (functionType != NULL) ||
  // (pointerToMemberFunctionType != NULL) || (enumType != NULL) )

  // DQ (2/20/2019): Removing the use of classType allows the name qualification
  // to be handled properly thorugh the type. But I need to check why this was
  // added in the first place (what breaks). if ( (functionType != NULL) ||
  // (pointerToMemberFunctionType != NULL) || (enumType != NULL) || (classType
  // != NULL)) if ( (functionType != NULL) || (pointerToMemberFunctionType !=
  // NULL) || (enumType != NULL) ) if ( (functionType != NULL) ||
  // (pointerToMemberFunctionType != NULL) || (enumType != NULL) || (classType
  // != NULL)) if ( (functionType != NULL) || (pointerToMemberFunctionType !=
  // NULL) || (enumType != NULL) )
  if ((functionType != NULL) || (pointerToMemberFunctionType != NULL) ||
      (enumType != NULL) || (classType != NULL)) {
    // Newly implemented case of typedefs for function and member function
    // pointers
#if DEBUG_TYPEDEF_DECLARATIONS
    printf("In unparseTypeDefStmt(): case of typedefs for function and member "
           "function pointers \n");
#endif
#if OUTPUT_DEBUGGING_FUNCTION_INTERNALS || 0
    curprint("\n/* Case of typedefs for function and member function pointers "
             "*/ \n");
#endif
    ninfo.set_SkipFunctionQualifier();
    curprint("typedef ");

    // Specify that only the first part of the type shold be unparsed
    // (this will permit the introduction of the name into the member
    // function pointer declaration)
    ninfo.set_isTypeFirstPart();

#if OUTPUT_DEBUGGING_UNPARSE_INFO || 0
    curprint(string("\n/* ") +
             ninfo.displayString("After return Type now output the base type "
                                 "(first part then second part)") +
             " */ \n");
#endif

    // The base type contains the function po9inter type
    SgType *btype = typedef_stmt->get_base_type();

// #if OUTPUT_DEBUGGING_FUNCTION_INTERNALS
#if DEBUG_TYPEDEF_DECLARATIONS
    curprint("\n/* Output base type (first part) (for functionType or "
             "pointerToMemberFunctionType) */ \n");
#endif

#if DEBUG_TYPEDEF_DECLARATIONS
    printf("In unparseTypeDefStmt(): btype = %p = %s \n", btype,
           btype->class_name().c_str());
#endif

    // DQ (1/10/2007): Set the current declaration statement so that if required
    // we can do context dependent searches of the AST to determine if name
    // qualification is required. This is done now for the case of function and
    // member function typedefs.
    SgUnparse_Info ninfo_for_type(ninfo);
    ninfo_for_type.set_declstatement_ptr(typedef_stmt);

    // DQ (2/18/2019): The name qualification support sets the name
    // qualification for enums declarations using the enum declaration, so the
    // typedef declaration is not appropriate to use with
    // set_reference_node_for_qualification().
    SgDeclarationStatement *declaration = typedef_stmt->get_declaration();
    if (declaration != NULL && isSgEnumDeclaration(declaration) != NULL &&
        isSgTemplateInstantiationDecl(declaration) == NULL) {
#if DEBUG_TYPEDEF_DECLARATIONS
      printf("Found enum declaration in typedef declaration; using it as "
             "reference node for qualification.\n");
#endif
      ninfo_for_type.set_reference_node_for_qualification(declaration);
    } else {
      // Qualify typedef base types relative to the typedef declaration context.
      // This preserves required global qualification when local declarations
      // shadow class names (e.g., typedef ::A::B inside a function-local A).
      ninfo_for_type.set_reference_node_for_qualification(typedef_stmt);
    }

    // DQ (12/27/2019): To support multiple files, we need to set the declartion
    // to be used in unparsing the definig declaration in the type (when it is
    // present). DQ (12/26/2019): Adding support for unparsing of defining
    // declarations in types used across multiple translation units (multiple
    // files). See Cxx11_tests/test2019_520a.C and test2019_520b.C.
    SgDeclarationStatement *tmp_associatedDefiningDeclaration =
        typedef_stmt->get_baseTypeDefiningDeclaration();
    if (tmp_associatedDefiningDeclaration != NULL) {
      ASSERT_not_null(declaration);

      if (SgEnumDeclaration *enumDeclaration =
              isSgEnumDeclaration(tmp_associatedDefiningDeclaration)) {
        // Enum types select their defining declaration via
        // declaration_of_context rather than useAlternativeDefiningDeclaration.
        ninfo_for_type.set_declaration_of_context(enumDeclaration);
      } else {
        ninfo_for_type.set_useAlternativeDefiningDeclaration();

        // Make sure this is not is use.
        // ROSE_ASSERT(ninfo_for_type.get_declstatement_ptr() == NULL);
        // ROSE_ASSERT(ninfo_for_type.get_declstatement_associated_with_type()
        // == NULL);
        // ninfo_for_type.set_declaration(tmp_associatedDefiningDeclaration);
        // ninfo_for_type.set_declstatement_ptr(tmp_associatedDefiningDeclaration);
        ninfo_for_type.set_declstatement_associated_with_type(
            tmp_associatedDefiningDeclaration);
      }
    }

#if DEBUG_TYPEDEF_DECLARATIONS
    curprint("\n/* Output base type (first part) */ \n");
#endif
    // Only pass the ninfo_for_type to support name qualification of the base
    // type. unp->u_type->unparseType(btype, ninfo);
    unp->u_type->unparseType(btype, ninfo_for_type);

// #if OUTPUT_DEBUGGING_FUNCTION_INTERNALS
#if DEBUG_TYPEDEF_DECLARATIONS
    curprint("\n/* Done: Output base type (first part) */ \n");
#endif
    curprint(typedef_stmt->get_name().str());

    // Now unparse the second part of the typedef
// #if OUTPUT_DEBUGGING_FUNCTION_INTERNALS
#if DEBUG_TYPEDEF_DECLARATIONS
    curprint("\n/* Output base type (second part) */ \n");
#endif

    ninfo.set_isTypeSecondPart();

    // DQ (1/2/2020): We need to use settings for ( info.inTypedefDecl() and
    // info.inArgList()) the same as used in unparsing the first part of the
    // type. unp->u_type->unparseType(btype, ninfo);
    ninfo_for_type.set_isTypeSecondPart();

    // DQ (1/2/2020): This is required since it is used in unparsing the first
    // part of the type and causes the "(" to be unparsed, and we require this
    // to be set so that the same logic will triger the ")" to be unparsed.
    ninfo_for_type.set_inTypedefDecl();
    // DQ (1/2/2020): I think we can assert this.
    ROSE_ASSERT(ninfo_for_type.isTypeFirstPart() == false);

    unp->u_type->unparseType(btype, ninfo_for_type);

// #if OUTPUT_DEBUGGING_FUNCTION_INTERNALS
#if DEBUG_TYPEDEF_DECLARATIONS
    curprint("\n/* Done: Output base type (second part) */ \n");
#endif

  } else {
    // previously implemented case of unparsing the typedef does not handle
    // function pointers properly (so they are handled explicitly above!)
#if DEBUG_TYPEDEF_DECLARATIONS
    printf("Not a typedef for a function type or member function type \n");
#endif

    ninfo.set_SkipFunctionQualifier();
    curprint("typedef ");

    ninfo.set_SkipSemiColon();
    SgType *btype = typedef_stmt->get_base_type();

    // DQ (2/3/2019): Make the unparse_info object so that the unparseType() can
    // output the required parenthesis when the base type is a pointer to
    // member.
    if (pointerToMemberType != NULL) {
      ninfo.set_inTypedefDecl();
    }

    ninfo.set_isTypeFirstPart();

    // DQ (10/11/2006): As part of new implementation of qualified names we now
    // default to the generation of all qualified names unless they are skipped.
    // ninfo.set_SkipQualifiedNames();
    // curprint ( string("\n/* Commented out call to
    // ninfo.set_SkipQualifiedNames() */\n ";

    // printf ("Before first part of base type (type = %p = %s)
    // \n",btype,btype->sage_class_name()); ninfo.display ("Before first part of
    // type in unp->u_type->unparseTypeDefStmt()");

    SgUnparse_Info ninfo_for_type(ninfo);

    // DQ (1/10/2007): Set the current declaration statement so that if required
    // we can do context dependent searches of the AST to determine if name
    // qualification is required.
    ninfo_for_type.set_declstatement_ptr(NULL);
    ninfo_for_type.set_declstatement_ptr(typedef_stmt);

    if (typedef_stmt->get_requiresGlobalNameQualificationOnType() == true) {
#if DEBUG_TYPEDEF_DECLARATIONS
      printf("In Unparse_ExprStmt::unp->u_type->unparseTypedefStmt(): This "
             "base type requires a global qualifier \n");
      curprint("\n/* This base type requires a global qualifier, calling "
               "set_requiresGlobalNameQualification() */ \n");
#endif
      // ninfo_for_type.set_forceQualifiedNames();
      ninfo_for_type.set_requiresGlobalNameQualification();
    }

    // curprint ( string("\n/* unp->u_type->unparseTypeDefStmt: Before first
    // part of type */ \n";
// #if OUTPUT_DEBUGGING_FUNCTION_INTERNALS
#if DEBUG_TYPEDEF_DECLARATIONS
    curprint("\n/* Output base type (first part) (Not a typedef for a function "
             "type or member function type) */ \n");
#endif

    // DQ (5/30/2011): Added support for name qualification.
    ninfo_for_type.set_reference_node_for_qualification(typedef_stmt);
    ASSERT_not_null(ninfo_for_type.get_reference_node_for_qualification());

    // DQ (5/14/2011): Added support for newer name qualification
    // implementation.
    ninfo_for_type.set_name_qualification_length(
        typedef_stmt->get_name_qualification_length_for_base_type());
    ninfo_for_type.set_global_qualification_required(
        typedef_stmt->get_global_qualification_required_for_base_type());
    ninfo_for_type.set_type_elaboration_required(
        typedef_stmt->get_type_elaboration_required_for_base_type());

    // DQ (1/2/2020): Added to define symetry in handling.
    // ninfo.set_inTypedefDecl();
    ninfo_for_type.set_inTypedefDecl();

    // DQ (11/21/2021): When there is a declaration of the base type then we
    // can't unparse the declaration by going through the base type because
    // types are shared and in the case of supporting multiple files we will
    // unparse the declaration from the other file.

    if (outputTypeDefinition == true) {
      // DQ (11/21/2021): Fixing bug reported by Jim Leek, Markus, and part of
      // work with Liao. Get the defining class declaration and output it
      // directly, instead of through the shared type which can only refer to
      // one file (half the time the wrong file) and for which the body of unt e
      // class declaration will not match the current file and will not be
      // unparsed.  Note that this is not an issue of supporting shared
      // declarations across two or more files, since this is before merge, and
      // thus there is a defining declaration for the class in each file. This
      // detail is a requirement for the support of multiple source files
      // specified on the command line only.

#if DEBUG_TYPEDEF_DECLARATIONS
      printf("Adding support for outputTypeDefinition == true for multiple "
             "files \n");
#endif
      SgDeclarationStatement *declaration = typedef_stmt->get_declaration();
      ROSE_ASSERT(declaration != NULL);
      // SgScopeStatement* scope = NULL;

      switch (declaration->variantT()) {
      case V_SgClassDeclaration: {
        SgClassDeclaration *classDeclaration =
            isSgClassDeclaration(declaration);
        ROSE_ASSERT(classDeclaration != NULL);
        // SgClassDefinition* classDefinition =
        // classDeclaration->get_definition(); ROSE_ASSERT(classDefinition !=
        // NULL); scope = classDefinition; ROSE_ASSERT(scope != NULL);
        if (typedef_stmt->get_isAssociatedWithDeclarationList() == true) {
          // DQ (8/2/2012): Make this consistant with the design for the
          // variable declarations. This is an alternative to permit the
          // unparsing of the type to control the name output for types. But it
          // would have to be uniform that all the pieces of the first part of
          // the type would have to be output.  E.g. "*" in "*X".
          // ninfo.set_PrintName();
          ninfo_for_type.set_PrintName();
        } else {
          // ninfo.unset_PrintName();
          ninfo_for_type.unset_PrintName();
        }
        // unparseStatement(scope,ninfo);
        // unparseStatement(classDeclaration,ninfo);

        ninfo_for_type.set_declaration_of_context(classDeclaration);
        unp->u_type->unparseType(btype, ninfo_for_type);
        break;
      }

      case V_SgEnumDeclaration: {
        // DQ (11/21/2021): Note that we can't handle these two cases the same
        // way.
        SgEnumDeclaration *enumDeclaration = isSgEnumDeclaration(declaration);
        ROSE_ASSERT(enumDeclaration != NULL);
        // DQ (11/21/2021): Not clear if this case is well tests.
        if (typedef_stmt->get_isAssociatedWithDeclarationList() == true) {
          // DQ (11/21/2021): Need to also support the case of multiple names
          // being used. ninfo.set_PrintName();
          ninfo_for_type.set_PrintName();
        } else {
          // ninfo.unset_PrintName();
          ninfo_for_type.unset_PrintName();
        }
        // ninfo.set_SkipSemiColon();
        // unparseEnumDeclStmt(enumDeclaration, ninfo);
        ninfo_for_type.set_declaration_of_context(enumDeclaration);
        unp->u_type->unparseType(btype, ninfo_for_type);
        break;
      }

      default: {
        printf(
            "Default reached in identification of declaration in typedef \n");
        ROSE_ASSERT(false);
      }
      };
    } else {
      // DQ (7/28/2012): This is similar to code in the variable declaration
      // unparser function and so might be refactored. DQ (7/28/2012): If this
      // is a declaration associated with a declaration list from a previous
      // (the last statement) typedef then output the name if that declaration
      // had an un-named type (class or enum).
#if DEBUG_TYPEDEF_DECLARATIONS
      printf("In unparseTypedefStmt(): "
             "typedef_stmt->get_isAssociatedWithDeclarationList() = %s \n",
             typedef_stmt->get_isAssociatedWithDeclarationList() ? "true"
                                                                 : "false");
#endif
      if (typedef_stmt->get_isAssociatedWithDeclarationList() == true) {
        // DQ (8/2/2012): Make this consistant with the design for the variable
        // declarations. This is an alternative to permit the unparsing of the
        // type to control the name output for types. But it would have to be
        // uniform that all the pieces of the first part of the type would have
        // to be output.  E.g. "*" in "*X".
        ninfo_for_type.set_PrintName();
        unp->u_type->unparseType(btype, ninfo_for_type);
      } else {
        // DQ (7/28/2012): Output the type if this is not associated with a
        // declaration list from a previous declaration.
        // unp->u_type->unparseType(btype, ninfo);
#if DEBUG_TYPEDEF_DECLARATIONS
        printf("In unparseTypedefStmt(): (first part): btype = %p = %s \n",
               btype, btype->class_name().c_str());
#endif
// #if OUTPUT_DEBUGGING_FUNCTION_INTERNALS
#if DEBUG_TYPEDEF_DECLARATIONS
        curprint("\n/* Output a non function pointer typedef (Not a typedef "
                 "for a function type or member function type) */ \n");
#endif

        // DQ (5/7/2013): Using ninfo allows test2013_156.C to work.
        // unp->u_type->unparseType(btype, ninfo_for_type);
        // unp->u_type->unparseType(btype, ninfo);
        unp->u_type->unparseType(btype, ninfo_for_type);
      }
    }

// #if OUTPUT_DEBUGGING_FUNCTION_INTERNALS
#if DEBUG_TYPEDEF_DECLARATIONS
    curprint("\n/* Done: Output base type (first part) */ \n");
#endif

    // curprint ( string("\n/* unp->u_type->unparseTypeDefStmt: After first part
    // of type */ \n"; printf ("After first part of type \n");

    // DQ (10/11/2006): As part of new implementation of qualified names we now
    // default to the generation of all qualified names unless they are skipped.
    // ninfo.unset_SkipQualifiedNames();

    // DQ (10/7/2004): Moved the output of the name to before the output of the
    // second part of the type to handle the case of "typedef A* A_Type[10];"
    // (see test2004_104.C).

#if DEBUG_TYPEDEF_DECLARATIONS
    curprint("\n/* Output typedef name */ \n");
#endif
    // The name of the type (X, in the following example) has to appear after
    // the declaration. Example: struct { int a; } X;
    curprint(typedef_stmt->get_name().str());
    // curprint(string("/* before 2nd part */ ") +
    // typedef_stmt->get_name().str());

    ninfo.set_isTypeSecondPart();

    // printf ("Before 2nd part of type \n");
    // curprint ( string("\n/* unp->u_type->unparseTypeDefStmt: Before second
    // part of type */ \n";
// #if OUTPUT_DEBUGGING_FUNCTION_INTERNALS
#if DEBUG_TYPEDEF_DECLARATIONS
    curprint("\n/* Output base type (second part) */ \n");
#endif

    // DQ (1/2/2020): This is required since it is used in unparsing the first
    // part of the type and causes the "(" to be unparsed, and we require this
    // to be set so that the same logic will triger the ")" to be unparsed.
    ninfo.set_inTypedefDecl();
    // DQ (1/2/2020): I think we can assert this.
    ROSE_ASSERT(ninfo.isTypeFirstPart() == false);

    unp->u_type->unparseType(btype, ninfo);
    // unp->u_type->unparseType(btype, ninfo_for_type);
    // unp->u_type->unparseType(btype, ninfo_for_type);

// #if OUTPUT_DEBUGGING_FUNCTION_INTERNALS
#if DEBUG_TYPEDEF_DECLARATIONS
    curprint("\n/* Done: Output base type (second part) */ \n");
#endif
    // curprint ( string("\n/* unp->u_type->unparseTypeDefStmt: After second
    // part of type */ \n"; printf ("After 2nd part of type \n");

    // DQ (2/3/2019): Unset this to avoid use outside of typedef unparsing.
    ninfo.unset_inTypedefDecl();
  }

#if DEBUG_TYPEDEF_DECLARATIONS
  printf("In unparseTypedefStmt(): outputTypeDefinition = %s \n",
         outputTypeDefinition ? "true" : "false");
#endif
  if (outputTypeDefinition == true) {
    unparseTypeAttributes(typedef_stmt);
  }

  // DQ (2/26/2013): Output any attributes.
  unp->u_sage->printAttributes(typedef_stmt, info);

  if (!info.SkipSemiColon()) {
    curprint(";");
  }

  // info.display ("At base of unp->u_type->unparseTypeDefStmt()");
#if DEBUG_TYPEDEF_DECLARATIONS
  printf("Leaving unparseTypedefStmt() \n");
  curprint("/* Leaving unparseTypedefStmt */ \n");
#endif
}

void Unparse_ExprStmt::unparseTemplateDeclStmt(SgStatement *stmt,
                                               SgUnparse_Info &info) {
  // This function is called for the processing of template declarations
  // (SgTemplateDeclaration). However a newer design as of 11/19/2011 is using
  // separate derived classes (IR nodes) from SgTemplateDeclaration to represent
  // SgTemplateClassDeclaration (and template functions, template member
  // functions, etc).  This work is part of the processing of the template
  // declarations into their own AST (different from legacy template support).

  SgTemplateDeclaration *template_stmt = isSgTemplateDeclaration(stmt);
  ASSERT_not_null(template_stmt);

  // printf ("In unparseTemplateDeclStmt(template_stmt = %p) \n",template_stmt);
  // template_stmt->get_declarationModifier().display("In
  // unparseTemplateDeclStmt()");

  // DQ (11/20/2011): Detect derived classes that should not be used in the new
  // frontend support in ROSE.
  if (isSgTemplateClassDeclaration(stmt) != NULL) {
  }

  // SgUnparse_Info ninfo(info);

  // Check to see if this is an object defined within a class
  ASSERT_not_null(template_stmt->get_parent());
  SgClassDefinition *cdefn = isSgClassDefinition(template_stmt->get_parent());
  if (cdefn != NULL) {
    if (cdefn->get_declaration()->get_class_type() ==
        SgClassDeclaration::e_class) {
      info.set_CheckAccess();
    }
  }

  // SgUnparse_Info saved_ninfo(ninfo);
  // this call has been moved below, after we indent
  // unp->u_sage->printSpecifier2(vardecl_stmt, ninfo);

  // Setup the SgUnparse_Info object for this statement
  // ninfo.unset_CheckAccess();
  // info.set_access_attribute(ninfo.get_access_attribute());

  // info.display("In unparseTemplateDeclStmt()");

  // Output access modifiers
  unp->u_sage->printSpecifier1(template_stmt, info);

  // printf ("template_stmt->get_string().str() = %s
  // \n",template_stmt->get_string().str());

  // DQ (1/21/2004): Use the string class to simplify the previous version of
  // the code
  string templateString = template_stmt->get_string().str();

  // DQ (4/29/2004): Added support for "export" keyword (not supported by g++
  // yet)
  if (template_stmt->get_declarationModifier().isExport())
    curprint(string("export "));

  // printf ("template_stmt->get_template_kind() = %d
  // \n",template_stmt->get_template_kind());
  switch (template_stmt->get_template_kind()) {
  case SgTemplateDeclaration::e_template_class:
  case SgTemplateDeclaration::e_template_m_class:
  case SgTemplateDeclaration::e_template_function:
  case SgTemplateDeclaration::e_template_m_function:
  case SgTemplateDeclaration::e_template_m_data:
  case SgTemplateDeclaration::e_template_variable: {
    // printf ("debugging 64 bit bug: templateString = %s
    // \n",templateString.c_str());
    if (templateString.empty() == true) {
      // DQ (12/22/2006): This is typically a template member class (see
      // test2004_128.C and test2004_138.C). It is not clear to me why the names
      // are missing, though perhaps they have not been computed yet (until the
      // templated clas is instantiated)!

      // DQ (11/27/2011): Uncommented for debugging new frontend connection.
      printf("Warning: templateString name is empty in "
             "Unparse_ExprStmt::unparseTemplateDeclStmt() \n");
      printf("     template_stmt->get_template_kind() = %d \n",
             template_stmt->get_template_kind());
    }
    // ROSE_ASSERT(templateString.empty() == false);

    curprint(string("\n") + templateString);
    break;
  }

  case SgTemplateDeclaration::e_template_none:
    // printf ("Do we need this extra \";\"? \n");
    // curprint ( templateString + ";";
    printf("Error: SgTemplateDeclaration::e_template_none found (not sure what "
           "to do here) \n");
    ROSE_ABORT();
    break;

  default:
    printf("Error: default reached \n");
    ROSE_ABORT();
  }
}

void Unparse_ExprStmt::unparseTemplateClassDefnStmt(SgStatement *stmt_,
                                                    SgUnparse_Info &info) {
  SgTemplateClassDefinition *stmt = isSgTemplateClassDefinition(stmt_);
  assert(stmt != NULL);
  unparseTemplateClassDeclStmt(stmt->get_declaration(), info);
}

void Unparse_ExprStmt::unparseTemplateClassDeclStmt(SgStatement *stmt,
                                                    SgUnparse_Info &info) {
  unparseTemplateDeclarationStatment_support<SgTemplateClassDeclaration>(stmt,
                                                                         info);
}

void Unparse_ExprStmt::unparseTemplateFunctionDeclStmt(SgStatement *stmt,
                                                       SgUnparse_Info &info) {
  // DQ (8/6/2012): Unparse the associated comments.
  // We can't unparse comments in the templae declarations until we stop using
  // saved string form of the template declaration.  This will be done in a
  // later version of the release of the new template support.  In the mean time
  // we have to supress attaching CPP directives to the inside of template
  // declarations.

  unparseTemplateDeclarationStatment_support<SgTemplateFunctionDeclaration>(
      stmt, info);
}

void Unparse_ExprStmt::unparseTemplateMemberFunctionDeclStmt(
    SgStatement *stmt, SgUnparse_Info &info) {

  unparseTemplateDeclarationStatment_support<
      SgTemplateMemberFunctionDeclaration>(stmt, info);

  // DQ (5/28/2019): If there are any attached CPP directives then unparse them.
  // This will cause then to be output twice.
  // unparseAttachedPreprocessingInfo(stmt, info, PreprocessingInfo::after);
}

void Unparse_ExprStmt::unparseTemplateVariableDeclStmt(SgStatement *stmt,
                                                       SgUnparse_Info &info) {
  // DQ (1/3/2016): Present this function and the associated
  // SgTemplateVariableDeclaration IR node is being used for both the template
  // and the instanatiation of variables.  We might want to have an IR node
  // specific to template variable instantition.

  unparseTemplateDeclarationStatment_support<SgTemplateVariableDeclaration>(
      stmt, info);
}

#define DEBUG_unparseTemplateHeader 0

template <class T>
void Unparse_ExprStmt::unparseTemplateHeader(T *decl, SgUnparse_Info &info) {
#if DEBUG_unparseTemplateHeader
  printf("In unparseTemplateHeader(decl = %p = %s) \n", decl,
         decl->class_name().c_str());
#endif
  resetTemplateParameterEmissionState();

  SgTemplateParameterPtrList tlist;
  for (SgTemplateParameter *param : decl->get_templateParameters()) {
    if (!SageInterface::isAbbreviatedFunctionTemplateParameter(param)) {
      tlist.push_back(param);
    }
  }

  if (!tlist.empty()) {
    curprint("template ");
    SgUnparse_Info tinfo(info);
    tinfo.set_declstatement_ptr(NULL);
    tinfo.set_declstatement_ptr(decl);
    Unparse_ExprStmt::unparseTemplateParameterList(tlist, tinfo, true);
    if (SgExpression *requires_clause = decl->get_requiresClause()) {
      curprint("requires ");
      SgUnparse_Info rinfo(info);
      rinfo.set_SkipClassDefinition();
      rinfo.set_SkipEnumDefinition();
      unparseRequiresClauseExpression(this, requires_clause, rinfo);
      curprint(" ");
    }
    curprint("\n");
  } else {
    bool is_explicit_specialization = false;
    if (SgTemplateClassDeclaration *class_decl =
            isSgTemplateClassDeclaration(decl)) {
      is_explicit_specialization = class_decl->get_specialization() ==
                                   SgDeclarationStatement::e_specialization;
    } else if (SgTemplateFunctionDeclaration *func_decl =
                   isSgTemplateFunctionDeclaration(decl)) {
      is_explicit_specialization = func_decl->get_specialization() ==
                                   SgDeclarationStatement::e_specialization;
    } else if (SgTemplateMemberFunctionDeclaration *member_decl =
                   isSgTemplateMemberFunctionDeclaration(decl)) {
      is_explicit_specialization = member_decl->get_specialization() ==
                                   SgDeclarationStatement::e_specialization;
    } else if (SgTemplateVariableDeclaration *var_decl =
                   isSgTemplateVariableDeclaration(decl)) {
      is_explicit_specialization = var_decl->get_specialization() ==
                                   SgDeclarationStatement::e_specialization;
    }

    if (is_explicit_specialization) {
      curprint("template <>\n");
    }
  }
}

std::string replaceString(std::string subject, const std::string &search,
                          const std::string &replace) {
  size_t pos = 0;
  while ((pos = subject.find(search, pos)) != std::string::npos) {
    subject.replace(pos, search.length(), replace);
    pos += replace.length();
  }

  return subject;
}

template <class T>
void Unparse_ExprStmt::unparseTemplateDeclarationStatment_support(
    SgStatement *stmt, SgUnparse_Info &info) {
  ASSERT_not_null(stmt);

  T *template_stmt = dynamic_cast<T *>(stmt);
  ASSERT_not_null(template_stmt);

  // DQ (1/28/2013): This helps handle cases such as "#if 1 void foo () #endif {
  // }"
  unparseAttachedPreprocessingInfo(template_stmt, info,
                                   PreprocessingInfo::inside);

  // Check to see if this is an object defined within a class
  ASSERT_not_null(template_stmt->get_parent());
  SgClassDefinition *cdefn = isSgClassDefinition(template_stmt->get_parent());
  if (cdefn != NULL && cdefn->get_declaration()->get_class_type() ==
                           SgClassDeclaration::e_class) {
    info.set_CheckAccess();
  }

  // Output access modifiers
  unp->u_sage->printSpecifier1(template_stmt, info);

  // DQ (4/29/2004): Added support for "export" keyword (not supported by g++
  // yet)
  if (template_stmt->get_declarationModifier().isExport()) {
    curprint(string("export "));
  }

  // Five cases to consider: TODO this a template function, don't we already
  // know that?

  SgTemplateClassDeclaration *templateClassDeclaration =
      isSgTemplateClassDeclaration(stmt);
  SgTemplateFunctionDeclaration *templateFunctionDeclaration =
      isSgTemplateFunctionDeclaration(stmt);
  SgTemplateMemberFunctionDeclaration *templateMemberFunctionDeclaration =
      isSgTemplateMemberFunctionDeclaration(stmt);
  SgTemplateVariableDeclaration *templateVariableDeclaration =
      isSgTemplateVariableDeclaration(stmt);
  SgTemplateTypedefDeclaration *templateTypedefDeclaration =
      isSgTemplateTypedefDeclaration(stmt);

  ROSE_ASSERT(templateClassDeclaration != nullptr ||
              templateFunctionDeclaration != nullptr ||
              templateMemberFunctionDeclaration != nullptr ||
              templateVariableDeclaration != nullptr ||
              templateTypedefDeclaration != nullptr);

  // Unparsing template from the AST can be controlled at the sourcefile or
  // declaration level. The declaration-level flag is set on the shared
  // template declaration base and applies to all template declaration kinds.

  SgSourceFile *sourcefile = info.get_current_source_file();
  bool unparse_template_from_ast =
      sourcefile != NULL && sourcefile->get_unparse_template_ast();
  unparse_template_from_ast |=
      template_stmt->get_unparse_template_ast() == true;

  auto unparse_member_function_ctor_initializers =
      [&](SgMemberFunctionDeclaration *member_function,
          SgUnparse_Info &decl_info) {
        if (member_function == NULL) {
          return;
        }
        if (member_function->isForward()) {
          return;
        }

        const SgInitializedNamePtrList *ctor_inits_ptr =
            &member_function->get_ctors();
        if (ctor_inits_ptr->empty()) {
          if (SgMemberFunctionDeclaration *def_decl =
                  isSgMemberFunctionDeclaration(
                      member_function->get_definingDeclaration())) {
            if (def_decl != member_function && !def_decl->get_ctors().empty()) {
              ctor_inits_ptr = &def_decl->get_ctors();
            }
          }
        }

        auto const &ctor_inits = *ctor_inits_ptr;
        if (ctor_inits.empty()) {
          return;
        }

        SgUnparse_Info init_info(decl_info);
        init_info.set_SkipClassDefinition();
        init_info.set_SkipEnumDefinition();
        init_info.set_inArgList();
        init_info.set_declstatement_ptr(NULL);
        init_info.set_declstatement_ptr(member_function);

        auto it_ctor_init = ctor_inits.begin();
        auto const first = it_ctor_init;

        curprint(" : ");
        while (it_ctor_init != ctor_inits.end()) {
          SgInitializedName *ctor_init = *it_ctor_init;
          ASSERT_not_null(ctor_init);
          if (it_ctor_init != first) {
            curprint(", ");
          }
          ++it_ctor_init;

          unparseAttachedPreprocessingInfo(ctor_init, decl_info,
                                           PreprocessingInfo::before);

          unparseCtorPreinitializerDesignator(this, unp, ctor_init, init_info);

          SgExpression *initializer = ctor_init->get_initializer();
          if (initializer == NULL) {
            continue;
          }

          SgConstructorInitializer *ctor_initializer =
              isSgConstructorInitializer(initializer);
          bool output_parenthesis = ctor_initializer == nullptr;
          if (output_parenthesis) {
            curprint(string("("));
          }

          decl_info.set_reference_node_for_qualification(initializer);
          unparseExpression(initializer, init_info);
          decl_info.set_reference_node_for_qualification(NULL);

          if (output_parenthesis) {
            curprint(string(")"));
          }
        }
      };

  auto template_parameter_lists_match =
      [](const SgTemplateParameterPtrList &lhs,
         const SgTemplateParameterPtrList &rhs) -> bool {
    if (lhs.size() != rhs.size()) {
      return false;
    }
    for (size_t i = 0; i < lhs.size(); ++i) {
      SgTemplateParameter *lhs_param = lhs[i];
      SgTemplateParameter *rhs_param = rhs[i];
      if (lhs_param == nullptr || rhs_param == nullptr) {
        if (lhs_param != rhs_param) {
          return false;
        }
        continue;
      }
      if (lhs_param->get_parameterType() != rhs_param->get_parameterType()) {
        return false;
      }
      if (lhs_param->get_is_parameter_pack() !=
          rhs_param->get_is_parameter_pack()) {
        return false;
      }

      auto parameter_name = [](SgTemplateParameter *param) -> std::string {
        if (param == nullptr) {
          return std::string();
        }
        if (SgTemplateType *template_type =
                isSgTemplateType(param->get_type())) {
          std::string name = template_type->get_name().getString();
          if (!name.empty()) {
            return name;
          }
        }
        if (SgInitializedName *init_name = param->get_initializedName()) {
          std::string name = init_name->get_name().getString();
          if (!name.empty()) {
            return name;
          }
        }
        return std::string();
      };

      if (parameter_name(lhs_param) != parameter_name(rhs_param)) {
        return false;
      }
    }
    return true;
  };

  if (unparse_template_from_ast) {
    SgTemplateClassDeclaration *assoc_tpl_class_decl = nullptr;
    SgDeclarationStatement *associated_decl = nullptr;
    if (templateMemberFunctionDeclaration) {
      associated_decl =
          templateMemberFunctionDeclaration->get_associatedClassDeclaration();
      assoc_tpl_class_decl = isSgTemplateClassDeclaration(associated_decl);
    } else if (templateVariableDeclaration) {
      ROSE_ASSERT(templateVariableDeclaration->get_variables().size() == 1);
      auto *iname = templateVariableDeclaration->get_variables()[0];
      associated_decl =
          iname->get_scope()
              ? isSgDeclarationStatement(iname->get_scope()->get_parent())
              : nullptr;
      assoc_tpl_class_decl = isSgTemplateClassDeclaration(associated_decl);
    } else if (templateClassDeclaration || templateTypedefDeclaration) {
      associated_decl = template_stmt->get_scope()
                            ? isSgDeclarationStatement(
                                  template_stmt->get_scope()->get_parent())
                            : nullptr;
      assoc_tpl_class_decl = isSgTemplateClassDeclaration(associated_decl);
    }
    //     std::cout << "assoc_tpl_class_decl = " << std::hex <<
    //     assoc_tpl_class_decl << " : " << (assoc_tpl_class_decl ?
    //     assoc_tpl_class_decl->class_name() : "") << std::endl;

    SgNode *parent = stmt->get_parent();
    const bool parent_is_class_like_definition =
        isSgClassDefinition(parent) != nullptr ||
        isSgTemplateClassDefinition(parent) != nullptr ||
        isSgTemplateInstantiationDefn(parent) != nullptr;
    if (!parent_is_class_like_definition) {
      std::vector<SgTemplateClassDeclaration *> assoc_tpl_chain =
          collectTemplateClassChain(associated_decl,
                                    template_stmt->get_scope());
      if (templateMemberFunctionDeclaration != nullptr &&
          !assoc_tpl_chain.empty() &&
          (isNormalizedClassTemplateMemberFunction(
               templateMemberFunctionDeclaration) ||
           template_parameter_lists_match(
               assoc_tpl_chain.front()->get_templateParameters(),
               templateMemberFunctionDeclaration->get_templateParameters()))) {
        assoc_tpl_chain.erase(assoc_tpl_chain.begin());
      }
      for (auto it = assoc_tpl_chain.rbegin(); it != assoc_tpl_chain.rend();
           ++it) {
        unparseTemplateHeader(*it, info);
      }
    }

    unparseTemplateHeader(template_stmt, info);

    SgUnparse_Info ninfo(info);

    if (templateClassDeclaration != NULL) {
      ninfo.unset_SkipSemiColon();
      ninfo.set_declstatement_ptr(NULL);
      ninfo.set_declstatement_ptr(templateClassDeclaration);

      // Match non-template class declaration output so friend/specifier
      // modifiers attached to template declarations are preserved.
      unp->u_sage->printSpecifier2(templateClassDeclaration, ninfo);

      SgClassDefinition *class_defn =
          templateClassDeclaration->get_definition();
      if (class_defn != NULL) {
        unparseClassDefnStmt(templateClassDeclaration->get_definition(), ninfo);
      } else {
        SgClassDeclaration::class_types class_type =
            templateClassDeclaration->get_class_type();
        switch (class_type) {
        case SgClassDeclaration::e_class:
          curprint("class ");
          break;
        case SgClassDeclaration::e_struct:
          curprint("struct ");
          break;
        case SgClassDeclaration::e_union:
          curprint("union ");
          break;
        case SgClassDeclaration::e_template_parameter:
          curprint(" ");
          break;
        default: {
          printf("Error: default reached in unparseClassDeclStmt() \n");
          ROSE_ABORT();
        }
        }

        SgName class_name = templateClassDeclaration->get_name();
        curprint(class_name.getString().c_str());
      }

      ninfo.set_declstatement_ptr(NULL);

      if (!info.SkipSemiColon())
        curprint(";");

    } else if (templateFunctionDeclaration != NULL ||
               templateMemberFunctionDeclaration != NULL) {
      SgFunctionDeclaration *functionDeclaration =
          (SgFunctionDeclaration *)stmt;

      // Match non-template function declarator output (constexpr/friend/etc.).
      unp->u_sage->printSpecifier2(functionDeclaration, ninfo);

      const bool is_deduction_guide =
          functionDeclaration->get_is_deduction_guide();
      SgType *rtype = functionDeclaration->get_type()->get_return_type();
      if (!is_deduction_guide) {
        unparseReturnType(functionDeclaration, rtype, ninfo);
      }

      ninfo.unset_SkipSemiColon();
      ninfo.set_declstatement_ptr(NULL);
      ninfo.set_declstatement_ptr(functionDeclaration);

      unparse_helper(functionDeclaration, ninfo);

      ninfo.set_declstatement_ptr(NULL);

      if (rtype != NULL) {
        if (is_deduction_guide || requiresTrailingReturnTypeSyntax(rtype)) {
          curprint(" -> ");
          SgUnparse_Info trailing_type_info(ninfo);
          trailing_type_info.set_isTypeFirstPart();
          unp->u_type->unparseType(rtype, trailing_type_info);
          trailing_type_info.set_isTypeSecondPart();
          unp->u_type->unparseType(rtype, trailing_type_info);
        } else {
          SgUnparse_Info ninfo3(ninfo);
          ninfo3.set_isTypeSecondPart();
          unp->u_type->unparseType(rtype, ninfo3);
        }
      }

      if (templateMemberFunctionDeclaration != NULL) {
        unparseTrailingFunctionModifiers(templateMemberFunctionDeclaration,
                                         ninfo);
        unparse_member_function_ctor_initializers(
            templateMemberFunctionDeclaration, ninfo);
      } else {
        unparseTrailingRequiresClauseIfPresent(this, functionDeclaration,
                                               ninfo);
      }

      SgFunctionDefinition *functionDefn =
          functionDeclaration->get_definition();
      if (functionDefn != NULL) {
        SgBasicBlock *body = functionDefn->get_body();
        SgUnparse_Info body_info(info);
        body_info.unset_CheckAccess();
        unparseStatement(body, body_info);
      }

      if (functionDefn == NULL && !info.SkipSemiColon())
        curprint(";");

    } else if (templateVariableDeclaration != NULL) {
      unparseVarDeclStmt(templateVariableDeclaration, info);
    } else {
      printf("Error: unexpected node variant: %s\n",
             stmt->class_name().c_str());
      ROSE_ABORT();
    }
    curprint("\n");
  } else if (templateVariableDeclaration != NULL) {
    // DQ (1/4/2016): Note that this is the case of a instantiation for a
    // template variable, we don't yet have a concept like this in the ROSE IR.
    // It would be just a SgVariableDeclaration, I think; however we isolate it
    // out as a SgTemplateVariabelDeclaration and just don't have an associated
    // string for the template and it is put into both the instantiation of the
    // template class and the global scope.  We supress it from being output in
    // the global scope (since that does not compile and is somewhat redundant
    // with output in the class, except that it is what would be required for a
    // non-template class).
    if (templateVariableDeclaration->get_string().is_null() == true) {
      // DQ (1/4/2016): Only output the variable declaration that is NOT in the
      // global scope (where it exists).
      // unparseVarDeclStmt(templateVariableDeclaration,info);
      if (isSgGlobal(templateVariableDeclaration->get_parent()) == NULL) {
        unparseVarDeclStmt(templateVariableDeclaration, info);
      }
    }
  } else {
    // Unparsing done from saved string
    string templateString = template_stmt->get_string().str();

    // Substitute " decltype" with " __decltype". Only if we are not using C++11
    // or later version of C++.
    if (sourcefile != NULL && sourcefile->get_Cxx11_only() == false &&
        sourcefile->get_Cxx14_only() == false) {
      templateString =
          replaceString(templateString, " decltype", " __decltype");
    }

    // Substitute " __ALIGNOF__" with " __alignof__"
    templateString =
        replaceString(templateString, " __ALIGNOF__", " __alignof__");

    // DQ (8/10/2020): Adding support to denormalize the C++ attributes.
    templateString = replaceString(templateString, "[ [", "[[");
    templateString = replaceString(templateString, "] ]", "]]");

    bool string_represents_function_body =
        (templateFunctionDeclaration != NULL &&
         templateFunctionDeclaration->get_string_represents_function_body()) ||
        (templateMemberFunctionDeclaration != NULL &&
         templateMemberFunctionDeclaration
             ->get_string_represents_function_body());

    if (string_represents_function_body == true) {
      // DQ (9/7/2014): This is the special case (to output template member and
      // non-member function declarations after normalization to move then out
      // of a template class declaration.
      SgFunctionDeclaration *functionDeclaration =
          isSgFunctionDeclaration(template_stmt);
      ASSERT_not_null(functionDeclaration);
      ROSE_ASSERT(functionDeclaration->isNormalizedTemplateFunction());

      ASSERT_not_null(templateMemberFunctionDeclaration);

      // TV (10/08/2018): ROSE-1392 uses template unparsing from the AST.
      SgDeclarationStatement *assoc_decl =
          templateMemberFunctionDeclaration->get_associatedClassDeclaration();
      SgTemplateClassDeclaration *assoc_tpl_class_decl =
          isSgTemplateClassDeclaration(assoc_decl);

      SgNode *parent = templateMemberFunctionDeclaration->get_parent();
      const bool parent_is_class_like_definition =
          isSgClassDefinition(parent) != nullptr ||
          isSgTemplateClassDefinition(parent) != nullptr ||
          isSgTemplateInstantiationDefn(parent) != nullptr;

      if (assoc_tpl_class_decl != NULL && !parent_is_class_like_definition) {
        std::vector<SgTemplateClassDeclaration *> assoc_tpl_chain =
            collectTemplateClassChain(
                assoc_decl, templateMemberFunctionDeclaration->get_scope());
        if (!assoc_tpl_chain.empty() &&
            (isNormalizedClassTemplateMemberFunction(
                 templateMemberFunctionDeclaration) ||
             template_parameter_lists_match(
                 assoc_tpl_chain.front()->get_templateParameters(),
                 templateMemberFunctionDeclaration
                     ->get_templateParameters()))) {
          assoc_tpl_chain.erase(assoc_tpl_chain.begin());
        }
        for (auto it = assoc_tpl_chain.rbegin(); it != assoc_tpl_chain.rend();
             ++it) {
          unparseTemplateHeader(*it, info);
        }
      }

      unparseTemplateHeader(templateMemberFunctionDeclaration, info);

      SgUnparse_Info ninfo(info);

      // Match non-template function declarator output (constexpr/friend/etc.).
      unp->u_sage->printSpecifier2(functionDeclaration, ninfo);

      SgType *rtype = NULL;
      unparseReturnType(functionDeclaration, rtype, ninfo);

      ninfo.set_declstatement_ptr(NULL);
      ninfo.set_declstatement_ptr(functionDeclaration);

      unparse_helper(functionDeclaration, ninfo);

      ninfo.set_declstatement_ptr(NULL);

      if (rtype != NULL) {
        if (requiresTrailingReturnTypeSyntax(rtype)) {
          curprint(" -> ");
          SgUnparse_Info trailing_type_info(ninfo);
          trailing_type_info.set_isTypeFirstPart();
          unp->u_type->unparseType(rtype, trailing_type_info);
          trailing_type_info.set_isTypeSecondPart();
          unp->u_type->unparseType(rtype, trailing_type_info);
        } else {
          SgUnparse_Info ninfo3(ninfo);
          ninfo3.set_isTypeSecondPart();
          unp->u_type->unparseType(rtype, ninfo3);
        }
      }

      unparseTrailingFunctionModifiers(templateMemberFunctionDeclaration,
                                       ninfo);
      unparse_member_function_ctor_initializers(
          templateMemberFunctionDeclaration, ninfo);

      curprint(string("\n") + templateString + string("\n"));
    } else {
#if OUTPUT_PLACEHOLDER_COMMENTS_FOR_SUPRESSED_TEMPLATE_IR_NODES
      // DQ (4/5/2018): For debugging, output something so that we know why
      // nothing is output.
      if (templateString.size() == 0) {
        curprint(" /* Output the templateString: templateString.size() = " +
                 StringUtility::numberToString(templateString.size()) + " */ ");
      }
#endif
      curprint(string("\n") + templateString);
    }
  }
}

// OpenMP support
void Unparse_ExprStmt::unparseOmpPrefix(SgUnparse_Info &) {
  curprint(string("#pragma omp "));
}

// OpenACC support
void Unparse_ExprStmt::unparseAccPrefix(SgUnparse_Info &) {
  curprint(string("#pragma acc "));
}

void Unparse_ExprStmt::unparseOmpForStatement(SgStatement *stmt,
                                              SgUnparse_Info &info) {
  ASSERT_not_null(stmt);
  SgOmpForStatement *f_stmt = isSgOmpForStatement(stmt);
  ASSERT_not_null(f_stmt);
  const int saved_linewrap = unp->cur.get_linewrap();
  unp->cur.set_linewrap(-1);

  unparseOmpDirectivePrefixAndName(stmt, info);

  unparseOmpBeginDirectiveClauses(stmt, info);
  // TODO a better way to new line? and add indentation
  curprint(string("\n"));

  SgUnparse_Info ninfo(info);
  if (f_stmt->get_body()) {
    unparseStatement(f_stmt->get_body(), ninfo);
  } else {
    cerr << "Error: empty body for:" << stmt->class_name() << " is not allowed!"
         << endl;
    ROSE_ABORT();
  }

  unp->cur.set_linewrap(saved_linewrap);
}

void Unparse_ExprStmt::unparseOmpForSimdStatement(SgStatement *stmt,
                                                  SgUnparse_Info &info) {
  ASSERT_not_null(stmt);
  SgOmpForSimdStatement *f_stmt = isSgOmpForSimdStatement(stmt);
  ASSERT_not_null(f_stmt);
  const int saved_linewrap = unp->cur.get_linewrap();
  unp->cur.set_linewrap(-1);

  unparseOmpDirectivePrefixAndName(stmt, info);

  unparseOmpBeginDirectiveClauses(stmt, info);
  // TODO a better way to new line? and add indentation
  curprint(string("\n"));

  SgUnparse_Info ninfo(info);
  if (f_stmt->get_body()) {
    unparseStatement(f_stmt->get_body(), ninfo);
  } else {
    cerr << "Error: empty body for:" << stmt->class_name() << " is not allowed!"
         << endl;
    ROSE_ABORT();
  }

  unp->cur.set_linewrap(saved_linewrap);
}

void Unparse_ExprStmt::unparseOmpBeginDirectiveClauses(SgStatement *stmt,
                                                       SgUnparse_Info &info) {
  ASSERT_not_null(stmt);
  // optional clauses
  SgOmpClauseBodyStatement *bodystmt = isSgOmpClauseBodyStatement(stmt);
  SgOmpDeclareSimdStatement *simdstmt = isSgOmpDeclareSimdStatement(stmt);
  SgOmpDeclareVariantStatement *declarevariantstmt =
      isSgOmpDeclareVariantStatement(stmt);
  SgOmpBeginDeclareVariantStatement *begindeclarevariantstmt =
      isSgOmpBeginDeclareVariantStatement(stmt);
  SgOmpDeclareMapperStatement *mapperstmt = isSgOmpDeclareMapperStatement(stmt);
  SgOmpDeclareTargetStatement *declaretargetstmt =
      isSgOmpDeclareTargetStatement(stmt);
  SgOmpTaskwaitStatement *taskwaitstmt = isSgOmpTaskwaitStatement(stmt);
  SgOmpClauseStatement *clausestmt = isSgOmpClauseStatement(stmt);
  SgOmpRequiresStatement *requiresstmt = isSgOmpRequiresStatement(stmt);
  const SgOmpClausePtrList *clause_ptr_list = nullptr;
  if (bodystmt != nullptr) {
    clause_ptr_list = &bodystmt->get_clauses();
  } else if (simdstmt != nullptr) {
    clause_ptr_list = &simdstmt->get_clauses();
  } else if (declarevariantstmt != nullptr) {
    clause_ptr_list = &declarevariantstmt->get_clauses();
  } else if (begindeclarevariantstmt != nullptr) {
    clause_ptr_list = &begindeclarevariantstmt->get_clauses();
  } else if (mapperstmt != nullptr) {
    clause_ptr_list = &mapperstmt->get_clauses();
  } else if (declaretargetstmt != nullptr) {
    clause_ptr_list = &declaretargetstmt->get_clauses();
  } else if (taskwaitstmt != nullptr) {
    clause_ptr_list = &taskwaitstmt->get_clauses();
  } else if (clausestmt != nullptr) {
    clause_ptr_list = &clausestmt->get_clauses();
  } else if (requiresstmt != nullptr) {
    clause_ptr_list = &requiresstmt->get_clauses();
  }
  if (mapperstmt != nullptr) {
    curprint(string("("));
    switch (mapperstmt->get_identifier()) {
    case SgOmpClause::e_omp_declare_mapper_identifier_default:
      curprint(string("default : "));
      break;
    case SgOmpClause::e_omp_declare_mapper_identifier_user:
      if (mapperstmt->get_user_defined_identifier() != nullptr) {
        unparseExpression(mapperstmt->get_user_defined_identifier(), info);
      }
      curprint(string(" : "));
      break;
    case SgOmpClause::e_omp_declare_mapper_identifier_unspecified:
      break;
    default:
      ROSE_ABORT();
    }

    bool mapper_type_needs_variable_separator = false;
    if (mapperstmt->get_mapper_type() != nullptr) {
      if (SgTypeExpression *type_expr =
              isSgTypeExpression(mapperstmt->get_mapper_type())) {
        if (SgClassType *class_type = isSgClassType(type_expr->get_type())) {
          curprint(string("class "));
          if (SgClassDeclaration *class_decl =
                  isSgClassDeclaration(class_type->get_declaration())) {
            if (isSgGlobal(class_decl->get_scope()) != nullptr) {
              curprint(string("::"));
            }
            if (SgTemplateInstantiationDecl *inst_decl =
                    isSgTemplateInstantiationDecl(class_decl)) {
              SgUnparse_Info ninfo(info);
              if (ninfo.isTypeFirstPart()) {
                ninfo.unset_isTypeFirstPart();
              }
              if (ninfo.isTypeSecondPart()) {
                ninfo.unset_isTypeSecondPart();
              }
              unp->u_exprStmt->unparseTemplateName(inst_decl, ninfo);
            } else {
              curprint(class_type->get_name().getString());
            }
          } else {
            curprint(class_type->get_name().getString());
          }
          mapper_type_needs_variable_separator = true;
        } else {
          unparseExpression(mapperstmt->get_mapper_type(), info);
          mapper_type_needs_variable_separator = true;
        }
      } else {
        unparseExpression(mapperstmt->get_mapper_type(), info);
        mapper_type_needs_variable_separator = true;
      }
    }
    if (mapperstmt->get_mapper_variable() != nullptr) {
      if (mapperstmt->get_mapper_type() != nullptr &&
          mapper_type_needs_variable_separator) {
        curprint(string(" "));
      }
      unparseExpression(mapperstmt->get_mapper_variable(), info);
    }
    curprint(string(")"));
  }

  if (clause_ptr_list != nullptr && !clause_ptr_list->empty()) {
    for (SgOmpClause *c_clause : *clause_ptr_list) {
      unparseOmpClause(c_clause, info);
    }
  }
  if (declaretargetstmt != nullptr) {
    const SgOmpClause::omp_when_context_kind_enum device_type_kind =
        declaretargetstmt->get_device_type_kind();
    if (device_type_kind != SgOmpClause::e_omp_when_context_kind_unknown) {
      curprint(string(" device_type("));
      switch (device_type_kind) {
      case SgOmpClause::e_omp_when_context_kind_host:
        curprint(string("host"));
        break;
      case SgOmpClause::e_omp_when_context_kind_nohost:
        curprint(string("nohost"));
        break;
      case SgOmpClause::e_omp_when_context_kind_any:
        curprint(string("any"));
        break;
      default:
        ROSE_ABORT();
      }
      curprint(string(")"));
    }
  }
}

void Unparse_ExprStmt::unparseAccBeginDirectiveClauses(SgStatement *stmt,
                                                       SgUnparse_Info &info) {
  ASSERT_not_null(stmt);
  const SgAccClausePtrList *clause_ptr_list = nullptr;
  if (SgAccClauseBodyStatement *bodystmt = isSgAccClauseBodyStatement(stmt)) {
    clause_ptr_list = &bodystmt->get_clauses();
  } else if (SgAccClauseStatement *clausestmt = isSgAccClauseStatement(stmt)) {
    clause_ptr_list = &clausestmt->get_clauses();
  }
  if (clause_ptr_list != nullptr) {
    for (SgAccClause *c_clause : *clause_ptr_list) {
      unparseAccClause(c_clause, info);
    }
  }
}

void Unparse_ExprStmt::unparseStaticAssertionDeclaration(SgStatement *stmt,
                                                         SgUnparse_Info &info) {
  // DQ (7/25/2014): Adding support for C11 static assertions.

  // For C11 this whould be unparsed as "_Static_assert", but for C++ it should
  // be unparsed as "static_assert".
  SgStaticAssertionDeclaration *staticAssertionDeclaration =
      isSgStaticAssertionDeclaration(stmt);
  ASSERT_not_null(staticAssertionDeclaration);

  // DQ (4/29/2017): This is the C11 syntax, and for C++11 we need the
  // alternative syntax ("static_assert"). curprint("_Static_assert(");
  if (SageInterface::is_Cxx_language() == true) {
    // This must be C++11 (or later).
    curprint("static_assert(");
  } else {
    // This must be C11 (or later).
    curprint("_Static_assert(");
  }

  unparseExpression(staticAssertionDeclaration->get_condition(), info);
  curprint(",\"");
  curprint(staticAssertionDeclaration->get_string_literal());
  curprint("\");");
}

// EOF
