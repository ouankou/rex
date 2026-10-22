
#include "sage3basic.h"

#include <algorithm>
#include <unordered_set>

using namespace std;
using namespace Rose;

// DQ (10/31/2015): Need to define this in a single location instead of in the
// header file included by multiple source files.
MangledNameSupport::setType MangledNameSupport::visitedTemplateDefinitions;
MangledNameSupport::functionSetType
    MangledNameSupport::visitedFunctionDefinitions;

string replaceNonAlphaNum(const string &s) {
  string s_new;
  s_new.reserve(s.size());
  for (unsigned char byte : s) {
    const bool ascii_alnum = (byte >= '0' && byte <= '9') ||
                             (byte >= 'A' && byte <= 'Z') ||
                             (byte >= 'a' && byte <= 'z');
    if (ascii_alnum)
      s_new += static_cast<char>(byte);
    else {
      s_new += '_';
      s_new += std::to_string(byte);
    }
  }
  return s_new;
}

string trimSpaces(const string &s) {
  string::size_type first = 0;
  while (first < s.size() && isspace(static_cast<unsigned char>(s[first])))
    ++first;
  string::size_type last = s.size();
  while (last > first && isspace(static_cast<unsigned char>(s[last - 1])))
    --last;
  return s.substr(first, last - first);
}

static void replaceAllInString(string &source, const string &target,
                               const string &replacement) {
  if (target.empty()) {
    return;
  }

  string::size_type pos = 0;
  while ((pos = source.find(target, pos)) != string::npos) {
    source.replace(pos, target.size(), replacement);
    pos += replacement.size();
  }
}

static string normalizeNameForMangledNameSupport(const string &name) {
  string normalized = trimSpaces(name);
  if (normalized.empty()) {
    return normalized;
  }

  const bool hasAngleBrackets = normalized.find_first_of("<>") != string::npos;
  const bool hasScopeSyntax = normalized.find("::") != string::npos;
  const bool hasColonSyntax = normalized.find(':') != string::npos;
  const bool hasTemplateSeparators = normalized.find(',') != string::npos;
  const bool hasTypeDecorators = normalized.find('&') != string::npos ||
                                 normalized.find('*') != string::npos ||
                                 normalized.find("_-") != string::npos;
  const bool hasLiteralQuotes = normalized.find('"') != string::npos;
  if (!hasAngleBrackets && !hasScopeSyntax && !hasColonSyntax &&
      !hasTemplateSeparators && !hasTypeDecorators && !hasLiteralQuotes) {
    return normalized;
  }

  replaceAllInString(normalized, " ", "");
  replaceAllInString(normalized, "<", "__tas__");
  replaceAllInString(normalized, ">", "__tae__");
  replaceAllInString(normalized, "::", "__scope__");
  replaceAllInString(normalized, ",", "__comma__");
  replaceAllInString(normalized, "&", "__ref__");
  replaceAllInString(normalized, "*", "__ptr__");
  replaceAllInString(normalized, "_-", "__minus__");
  replaceAllInString(normalized, "\"", "__quote__");
  ROSE_ASSERT(normalized.find(':') == string::npos);
  return normalized;
}

string joinMangledQualifiersToString(const string &base, const string &name) {
  string mangled_name(base);
  if (!base.empty() && !name.empty())
    mangled_name += "__scope__";
  mangled_name += name;
  return mangled_name;
}

SgName joinMangledQualifiers(const SgName &base, const SgName &name) {

  string mangled_name =
      joinMangledQualifiersToString(base.getString(), name.getString());
  return SgName(mangled_name.c_str());
}

const SgFunctionDefinition *findRootFunc(const SgScopeStatement *scope) {
  std::unordered_set<const SgScopeStatement *> visited;
  while (scope != nullptr) {
    ROSE_ASSERT(visited.insert(scope).second);
    switch (scope->variantT()) {
    case V_SgFunctionDefinition:
      return isSgFunctionDefinition(scope);

    case V_SgTemplateFunctionDefinition:
      return isSgTemplateFunctionDefinition(scope);

    case V_SgGlobal:
      return nullptr;

    default:
      scope = scope->get_scope();
      ROSE_ASSERT(scope != nullptr);
      break;
    }
  }
  return nullptr;
}

string mangleLocalScopeToString(const SgScopeStatement *scope) {
  ostringstream mangled_name;
  mangled_name.exceptions(std::ios::badbit | std::ios::failbit);
  if (scope) {
    // Find the function definition containing this scope.
    const SgFunctionDefinition *root_func = findRootFunc(scope);
    if (root_func) {
      size_t i = root_func->get_scope_number(scope);
      ROSE_ASSERT(i > 0);
      mangled_name << "__SgSS" << i << "__";
    }
  }
  return mangled_name.str();
}

namespace {

string encodeMangledIdentityComponent(const string &value) {
  static constexpr char digits[] = "0123456789abcdef";
  string encoded;
  encoded.reserve(value.size() * 2);
  for (unsigned char character : value) {
    encoded.push_back(digits[character >> 4]);
    encoded.push_back(digits[character & 0x0f]);
  }
  return encoded;
}

string
mangleFortranProcedureScopeToString(const SgFunctionDefinition *definition) {
  ROSE_ASSERT(definition != nullptr);

  const SgFunctionDeclaration *declaration = definition->get_declaration();
  const SgProcedureHeaderStatement *procedure =
      isSgProcedureHeaderStatement(declaration);
  const SgProgramHeaderStatement *program =
      isSgProgramHeaderStatement(declaration);
  SgScopeStatement *outerScope = nullptr;
  SgName procedureIdentity;

  if (declaration == nullptr) {
    outerScope = definition->get_construction_physical_output_owner();
    procedureIdentity = definition->get_fortran_construction_name();
    if (definition->get_parent() != nullptr || outerScope == nullptr ||
        procedureIdentity.getString().empty() ||
        definition->get_body() == nullptr ||
        definition->get_body()->get_parent() != definition) {
      fprintf(stderr,
              "REX_MANGLING_INVARIANT[fortran-procedure-construction-scope]: "
              "detached definition=%p owner=%p name='%s' has no exact "
              "construction identity\n",
              static_cast<const void *>(definition),
              static_cast<void *>(outerScope),
              procedureIdentity.getString().c_str());
      ROSE_ABORT();
    }
  } else {
    outerScope = declaration->get_scope();
    if ((procedure == nullptr) == (program == nullptr) ||
        definition->get_parent() != declaration ||
        declaration->get_definition() != definition || outerScope == nullptr ||
        definition->get_construction_physical_output_owner() != nullptr ||
        !definition->get_fortran_construction_name().getString().empty()) {
      fprintf(stderr,
              "REX_MANGLING_INVARIANT[fortran-program-unit-scope]: "
              "definition=%p declaration=%p/%s owner=%p retained a malformed "
              "program-unit or construction identity\n",
              static_cast<const void *>(definition),
              static_cast<const void *>(declaration),
              declaration->class_name().c_str(),
              static_cast<void *>(
                  definition->get_construction_physical_output_owner()));
      ROSE_ABORT();
    }
    procedureIdentity =
        SageInterface::getFortranProgramUnitSymbolTableKey(declaration);
  }

  Sg_File_Info *start = definition->get_startOfConstruct();
  Sg_File_Info *end = definition->get_endOfConstruct();
  const bool commonSourceIdentityValid =
      !procedureIdentity.getString().empty() && start != nullptr &&
      end != nullptr && start != end && definition->get_file_info() == start &&
      start->get_parent() == definition && end->get_parent() == definition &&
      start->get_physical_file_id() >= 0 &&
      end->get_physical_file_id() == start->get_physical_file_id() &&
      !start->get_physical_filename().empty() &&
      end->get_physical_filename() == start->get_physical_filename() &&
      !start->isShared() && !end->isShared();
  const bool generatedDefinition = commonSourceIdentityValid &&
                                   start->isTransformation() &&
                                   end->isTransformation();
  const bool generatedIdentityValid =
      generatedDefinition && procedure != nullptr &&
      procedure->get_fortran_procedure_source_form() ==
          SgProcedureHeaderStatement::e_fortran_procedure_source_form_header &&
      start->isOutputInCodeGeneration() && end->isOutputInCodeGeneration() &&
      !start->isCompilerGenerated() && !end->isCompilerGenerated();
  const bool sourceIdentityValid =
      commonSourceIdentityValid && !start->isTransformation() &&
      !end->isTransformation() && !start->isCompilerGenerated() &&
      !end->isCompilerGenerated() && start->get_line() > 0 &&
      start->get_col() > 0;
  if (!generatedIdentityValid && !sourceIdentityValid) {
    fprintf(stderr,
            "REX_MANGLING_INVARIANT[fortran-program-unit-scope-source]: "
            "definition=%p name='%s' start=%p end=%p start-class=%u "
            "end-class=%u physical=%d/%d line=%d:%d has no exact owned "
            "source or generated-output identity\n",
            static_cast<const void *>(definition),
            procedureIdentity.getString().c_str(), static_cast<void *>(start),
            static_cast<void *>(end),
            start != nullptr ? start->get_classificationBitField() : 0,
            end != nullptr ? end->get_classificationBitField() : 0,
            start != nullptr ? start->get_physical_file_id()
                             : Sg_File_Info::BAD_FILE_ID,
            end != nullptr ? end->get_physical_file_id()
                           : Sg_File_Info::BAD_FILE_ID,
            start != nullptr ? start->get_line() : 0,
            start != nullptr ? start->get_col() : 0);
    ROSE_ABORT();
  }

  ostringstream localIdentity;

  localIdentity.exceptions(std::ios::badbit | std::ios::failbit);
  localIdentity << "__fortran_program_unit_scope__"
                << encodeMangledIdentityComponent(procedureIdentity.getString())
                << "__file__"
                << encodeMangledIdentityComponent(
                       start->get_physical_filename())
                << (generatedDefinition ? "__generated__" : "__line__");
  if (!generatedDefinition) {
    localIdentity << start->get_line() << "__column__" << start->get_col();
  }
  return joinMangledQualifiersToString(mangleQualifiersToString(outerScope),
                                       localIdentity.str());
}

string mangleFunctionScopeToString(const SgFunctionDefinition *definition) {
  ROSE_ASSERT(definition != nullptr);

  const SgFunctionDeclaration *declaration = definition->get_declaration();
  const SgFunctionType *functionType =
      declaration != nullptr ? declaration->get_type() : nullptr;
  const SgFunctionParameterTypeList *parameterTypes =
      functionType != nullptr ? functionType->get_argument_list() : nullptr;
  SgScopeStatement *outerScope =
      declaration != nullptr ? declaration->get_scope() : nullptr;
  if (declaration == nullptr || declaration->get_definition() != definition ||
      definition->get_parent() != declaration ||
      definition->get_body() == nullptr ||
      definition->get_body()->get_parent() != definition ||
      functionType == nullptr || parameterTypes == nullptr ||
      outerScope == nullptr || declaration->get_name().getString().empty()) {
    fprintf(
        stderr,
        "REX_MANGLING_INVARIANT[function-scope]: definition=%p "
        "declaration=%p/%s definition-link=%p body=%p type=%p "
        "parameters=%p outer-scope=%p name='%s' has no exact semantic "
        "scope identity\n",
        static_cast<const void *>(definition),
        static_cast<const void *>(declaration),
        declaration != nullptr ? declaration->class_name().c_str() : "<null>",
        static_cast<const void *>(
            declaration != nullptr ? declaration->get_definition() : nullptr),
        static_cast<void *>(definition->get_body()),
        static_cast<const void *>(functionType),
        static_cast<const void *>(parameterTypes),
        static_cast<void *>(outerScope),
        declaration != nullptr ? declaration->get_name().getString().c_str()
                               : "");
    ROSE_ABORT();
  }

  string returnTypeIdentity;
  if (declaration->get_specialFunctionModifier().isConversion()) {
    SgType *returnType = declaration->get_orig_return_type();
    if (returnType == nullptr) {
      fprintf(stderr,
              "REX_MANGLING_INVARIANT[function-scope-conversion]: "
              "definition=%p declaration=%p name='%s' has no return type\n",
              static_cast<const void *>(definition),
              static_cast<const void *>(declaration),
              declaration->get_name().getString().c_str());
      ROSE_ABORT();
    }
    returnTypeIdentity = returnType->get_mangled().getString();
  }

  string functionName = declaration->get_name().getString();
  string templateIdentity;
  if (const SgTemplateInstantiationFunctionDecl *instantiation =
          isSgTemplateInstantiationFunctionDecl(declaration)) {
    functionName = instantiation->get_templateName().getString();
    templateIdentity = mangleTemplateArgsToString(
        instantiation->get_deducedTemplateArguments().begin(),
        instantiation->get_deducedTemplateArguments().end());
  } else if (const SgTemplateInstantiationMemberFunctionDecl *instantiation =
                 isSgTemplateInstantiationMemberFunctionDecl(declaration)) {
    functionName = instantiation->get_templateName().getString();
    templateIdentity = mangleTemplateArgsToString(
        instantiation->get_deducedTemplateArguments().begin(),
        instantiation->get_deducedTemplateArguments().end());
  } else if (const SgTemplateFunctionDeclaration *functionTemplate =
                 isSgTemplateFunctionDeclaration(declaration)) {
    templateIdentity = mangleTemplateArgsToString(
        functionTemplate->get_templateParameters().begin(),
        functionTemplate->get_templateParameters().end());
  } else if (const SgTemplateMemberFunctionDeclaration *functionTemplate =
                 isSgTemplateMemberFunctionDeclaration(declaration)) {
    templateIdentity = mangleTemplateArgsToString(
        functionTemplate->get_templateParameters().begin(),
        functionTemplate->get_templateParameters().end());
  }
  if (functionName.empty()) {
    fprintf(stderr,
            "REX_MANGLING_INVARIANT[function-scope-template-name]: "
            "definition=%p declaration=%p/%s has no template identity\n",
            static_cast<const void *>(definition),
            static_cast<const void *>(declaration),
            declaration->class_name().c_str());
    ROSE_ABORT();
  }

  const string argumentIdentity =
      mangleTypesToString(parameterTypes->get_arguments().begin(),
                          parameterTypes->get_arguments().end());
  ostringstream modifiers;
  modifiers.exceptions(std::ios::badbit | std::ios::failbit);
  if (const SgMemberFunctionType *memberType =
          isSgMemberFunctionType(functionType)) {
    modifiers << (memberType->isConstFunc() ? "c" : "_")
              << (memberType->isVolatileFunc() ? "v" : "_")
              << (memberType->isRestrictFunc() ? "r" : "_")
              << (memberType->isLvalueReferenceFunc() ? "l" : "_")
              << (memberType->isRvalueReferenceFunc() ? "r" : "_");
    if (memberType->isLvalueReferenceFunc() &&
        memberType->isRvalueReferenceFunc()) {
      fprintf(stderr,
              "REX_MANGLING_INVARIANT[function-scope-ref-qualifier]: "
              "definition=%p declaration=%p has both reference qualifiers\n",
              static_cast<const void *>(definition),
              static_cast<const void *>(declaration));
      ROSE_ABORT();
    }
  } else {
    modifiers << "_____";
  }

  ostringstream localIdentity;

  localIdentity.exceptions(std::ios::badbit | std::ios::failbit);
  localIdentity << "__function_scope_name__"
                << encodeMangledIdentityComponent(mangleFunctionNameToString(
                       functionName, returnTypeIdentity))
                << "__template__"
                << encodeMangledIdentityComponent(templateIdentity)
                << "__arguments__"
                << encodeMangledIdentityComponent(argumentIdentity)
                << "__modifiers__" << modifiers.str() << "__translation_unit__"
                << encodeMangledIdentityComponent(
                       mangleTranslationUnitQualifiers(declaration));
  return joinMangledQualifiersToString(mangleQualifiersToString(outerScope),
                                       localIdentity.str());
}

} // namespace

string mangleQualifiersToString(const SgScopeStatement *scope) {
  ROSE_ASSERT(scope != NULL);

  string mangled_name = "";
  if (scope != NULL) {
    switch (scope->variantT()) {
    case V_SgTemplateClassDefinition:

    case V_SgClassDefinition:
    case V_SgTemplateInstantiationDefn: {
      const SgClassDefinition *def = isSgClassDefinition(scope);

      ROSE_ASSERT(def != NULL);
      SgClassDefinition *nonconst_def = const_cast<SgClassDefinition *>(def);
      if (MangledNameSupport::visitedTemplateDefinitions.find(nonconst_def) ==
          MangledNameSupport::visitedTemplateDefinitions.end()) {
        const bool template_instantiation =
            isSgTemplateInstantiationDefn(nonconst_def) != NULL;
        if (template_instantiation)
          MangledNameSupport::visitedTemplateDefinitions.insert(nonconst_def);
        mangled_name = def->get_mangled_name().getString();
        if (template_instantiation)
          MangledNameSupport::visitedTemplateDefinitions.erase(nonconst_def);
      }

      break;
    }

    case V_SgNamespaceDefinitionStatement: {
      const SgNamespaceDefinitionStatement *def =
          isSgNamespaceDefinitionStatement(scope);
      ROSE_ASSERT(def != NULL);
      mangled_name = def->get_mangled_name().getString();
      break;
    }

    case V_SgTemplateFunctionDefinition:
    case V_SgFunctionDefinition: {
      // 'scope' is part of scope for locally defined classes
      const SgFunctionDefinition *def = isSgFunctionDefinition(scope);
      ROSE_ASSERT(def != nullptr);
      if (isSgProcedureHeaderStatement(def->get_declaration()) != nullptr ||
          isSgProgramHeaderStatement(def->get_declaration()) != nullptr ||
          (def->get_declaration() == nullptr &&
           !def->get_fortran_construction_name().getString().empty())) {
        mangled_name = mangleFortranProcedureScopeToString(def);
        break;
      }
      mangled_name = mangleFunctionScopeToString(def);
      break;
    }

    case V_SgRangeBasedForStatement:
    case V_SgAssociateStatement:
    case V_SgCatchOptionStmt:
    case V_SgDoWhileStmt:
    case V_SgForAllStatement:
    case V_SgForStatement:
    case V_SgFortranDo:
    case V_SgIfStmt:
    case V_SgSwitchStatement:
    case V_SgWhileStmt:
    case V_SgBasicBlock: {
      const SgScopeStatement *stmt = isSgScopeStatement(scope);
      ROSE_ASSERT(stmt != NULL);
      string stmt_name = mangleLocalScopeToString(stmt);
      string par_scope_name = mangleQualifiersToString(scope->get_scope());

      mangled_name = joinMangledQualifiersToString(par_scope_name, stmt_name);
      break;
    }
    case V_SgFunctionParameterScope:
    case V_SgGlobal:
      break;
    case V_SgDeclarationScope: {
      const auto *context = isSgDeclarationScope(scope);
      if (context->get_semantic_outer_scope() != nullptr ||
          isSgInitializedName(context->get_parent()) != nullptr)
        mangled_name = mangleQualifiersToString(context->get_scope());
      break;
    }

    default:
      fprintf(stderr,
              "REX_MANGLING_INVARIANT[unsupported-scope]: scope=%p type=%s "
              "semantic-parent=%p has no typed mangling rule\n",
              static_cast<const void *>(scope), scope->class_name().c_str(),
              static_cast<const void *>(scope->get_scope()));
      ROSE_ABORT();
    }
  }

  mangled_name = normalizeNameForMangledNameSupport(mangled_name);

  ROSE_ASSERT(mangled_name.find_first_of("<>") == string::npos);
  ROSE_ASSERT(mangled_name.find(':') == string::npos);

  return mangled_name;
}

SgName mangleQualifiers(const SgScopeStatement *scope) {
  ROSE_ASSERT(scope != NULL);

  ROSE_ASSERT(SgNode::isLiveNode(scope));

  if (scope->get_scope() == NULL) {
    if (isSgGlobal(scope) != NULL) {
      return SgName("");
    }
    fprintf(stderr,
            "REX_MANGLING_INVARIANT[detached-scope]: scope=%p type=%s has no "
            "semantic enclosing scope\n",
            static_cast<const void *>(scope), scope->class_name().c_str());
    const SgNode *ancestor = scope;
    for (size_t depth = 0; ancestor != NULL && depth < 16; ++depth) {
      const SgLocatedNode *located = isSgLocatedNode(ancestor);
      const Sg_File_Info *info =
          located != NULL ? located->get_file_info() : NULL;
      fprintf(stderr,
              "REX_MANGLING_INVARIANT[detached-scope-owner]: depth=%zu "
              "node=%p/%s parent=%p source=(%d:%d:%d)\n",
              depth, static_cast<const void *>(ancestor),
              ancestor->class_name().c_str(),
              static_cast<const void *>(ancestor->get_parent()),
              info != NULL ? info->get_physical_file_id() : -1,
              info != NULL ? info->get_line() : -1,
              info != NULL ? info->get_col() : -1);
      ancestor = ancestor->get_parent();
    }
    ROSE_ABORT();
  }
  ROSE_ASSERT(scope->get_scope() != NULL);

  string s = mangleQualifiersToString(scope);

  return SgName(s.c_str());
}

string mangleTypesToString(const SgTypePtrList::const_iterator b,
                           const SgTypePtrList::const_iterator e) {
  string mangled_name;
  bool is_first = true;

  for (SgTypePtrList::const_iterator p = b; p != e; ++p) {
    const SgType *type_p = *p;
    ROSE_ASSERT(type_p != NULL);

    if (is_first == true) {
      is_first = false;

      // DQ (5/11/2012): Make the mangled names a little more clear.
      mangled_name += "_";
    } else {
      mangled_name += "__sep__"; // separator between argument types
    }

    ROSE_ASSERT(type_p != NULL);
    ROSE_ASSERT(const_cast<SgType *>(type_p) != NULL);

    // DQ (2/14/2016): Adding support for VLA types.
    //    1) All vla types are equivalent, so no further name mangling is
    //    useful. 2) If we proceed then we will cause endless recursion in the
    //    evaluation
    //       of the scope of the array index variable reference expressions.
    SgName mangled_p = (const_cast<SgType *>(type_p))->get_mangled();
    mangled_name += string(mangled_p.str());
  }

  // DQ (5/11/2012): Make the mangled names a little more clear.
  mangled_name += "_";

  return mangled_name;
}

SgName mangleTypes(const SgTypePtrList::const_iterator b,
                   const SgTypePtrList::const_iterator e) {
  string mangled_name_str = mangleTypesToString(b, e);
  return SgName(mangled_name_str.c_str());
}

string mangleFunctionNameToString(const string &s,
                                  const string &ret_type_name) {
  string s_mangled(trimSpaces(s));

  // Special case: destructor names
  if (s[0] == '~') {
    s_mangled.replace(0, 1, "__dt");
    return s_mangled;
  }

  // Check for overloaded operators
  const string opstr("operator");
  const string::size_type n_opstr = opstr.size();
  const string newstr("new");
  const string newarrstr("new[]");
  const string delstr("delete");
  const string delarrstr("delete[]");

  if (s.substr(0, n_opstr) == opstr) // begins with "operator"
  {
    if (isspace(static_cast<unsigned char>(s[n_opstr]))) {
      if (s.substr(n_opstr + 1) == newstr) // is "operator new"
        s_mangled.replace(n_opstr, newstr.size() + 1, "__nw");
      else if (s.substr(n_opstr + 1) == newarrstr) // "operator new[]"
        s_mangled.replace(n_opstr, newarrstr.size() + 1, "__na");
      else if (s.substr(n_opstr + 1) == delstr) // is "operator delete"
        s_mangled.replace(n_opstr, delstr.size() + 1, "__dl");
      else if (s.substr(n_opstr + 1) == delarrstr) // "operator delete[]"
        s_mangled.replace(n_opstr, delarrstr.size() + 1, "__da");
      else // Cast operators.
        s_mangled.replace(n_opstr, s.size() - n_opstr,
                          "__opB__" + ret_type_name + "__opE__");
    } else // real operator (suffix after the substring "operator ")
    {
      string s_op = s.substr(n_opstr);
      // DQ (2/7/2006): Bug fix for case of function:
      // operator_takes_lvalue_operand() (this test appears in test2005_198.C).
      string s_op_mangled = s_op;
      if (s_op == "->")
        s_op_mangled = "__rf";
      else if (s_op == "->*")
        s_op_mangled = "__rm";
      else if (s_op == "==")
        s_op_mangled = "__eq";
      else if (s_op == "<")
        s_op_mangled = "__lt";
      else if (s_op == ">")
        s_op_mangled = "__gt";
      else if (s_op == "!=")
        s_op_mangled = "__ne";
      else if (s_op == "<=")
        s_op_mangled = "__le";
      else if (s_op == ">=")
        s_op_mangled = "__ge";
      else if (s_op == "+")
        s_op_mangled = "__pl";
      else if (s_op == "-")
        s_op_mangled = "__mi";
      else if (s_op == "*")
        s_op_mangled = "__ml";
      else if (s_op == "/")
        s_op_mangled = "__dv";
      else if (s_op == "%")
        s_op_mangled = "__md";
      else if (s_op == "&&")
        s_op_mangled = "__aa";
      else if (s_op == "!")
        s_op_mangled = "__nt";
      else if (s_op == "||")
        s_op_mangled = "__oo";
      else if (s_op == "^")
        s_op_mangled = "__er";
      else if (s_op == "&")
        s_op_mangled = "__ad";
      else if (s_op == "|")
        s_op_mangled = "__or";
      else if (s_op == ",")
        s_op_mangled = "__cm";
      else if (s_op == "<<")
        s_op_mangled = "__ls";
      else if (s_op == ">>")
        s_op_mangled = "__rs";
      else if (s_op == "--")
        s_op_mangled = "__mm";
      else if (s_op == "++")
        s_op_mangled = "__pp";
      else if (s_op == "~")
        s_op_mangled = "__co";
      else if (s_op == "=")
        s_op_mangled = "__as";
      else if (s_op == "+=")
        s_op_mangled = "__apl";
      else if (s_op == "-=")
        s_op_mangled = "__ami";
      else if (s_op == "&=")
        s_op_mangled = "__aad";
      else if (s_op == "|=")
        s_op_mangled = "__aor";
      else if (s_op == "*=")
        s_op_mangled = "__amu";
      else if (s_op == "/=")
        s_op_mangled = "__adv";
      else if (s_op == "%=")
        s_op_mangled = "__amd";
      else if (s_op == "^=")
        s_op_mangled = "__aer";
      else if (s_op == "<<=")
        s_op_mangled = "__als";
      else if (s_op == ">>=")
        s_op_mangled = "__ars";
      else if (s_op == "()")
        s_op_mangled = "__cl";
      else if (s_op == "[]")
        s_op_mangled = "__xi";
      s_mangled.replace(n_opstr, s_op.size(), s_op_mangled);
    }
  }

  return normalizeNameForMangledNameSupport(s_mangled);
}

SgName mangleFunctionName(const SgName &n, const SgName &ret_type_name) {
  string s_mangled =
      mangleFunctionNameToString(n.getString(), ret_type_name.str());
  SgName n_mangled(s_mangled.c_str());
  return n_mangled;
}

string
mangleTemplateArgsToString(const SgTemplateArgumentPtrList::const_iterator b,
                           const SgTemplateArgumentPtrList::const_iterator e) {
  ostringstream mangled_name;
  mangled_name.exceptions(std::ios::badbit | std::ios::failbit);

  for (SgTemplateArgumentPtrList::const_iterator i = b; i != e; ++i) {
    if (i != b) {
      mangled_name << "__sep__";
    }

    const SgTemplateArgument *arg = *i;
    ROSE_ASSERT(arg != NULL);
    mangled_name << arg->get_mangled_name().str();
  }

  return mangled_name.str();
}

string
mangleTemplateArgsToString(const SgTemplateParameterPtrList::const_iterator b,
                           const SgTemplateParameterPtrList::const_iterator e) {
  ostringstream mangled_name;
  mangled_name.exceptions(std::ios::badbit | std::ios::failbit);

  for (SgTemplateParameterPtrList::const_iterator i = b; i != e; ++i) {
    if (i != b) {
      mangled_name << "__sep__";
    }

    const SgTemplateParameter *param = *i;
    ROSE_ASSERT(param != NULL);
    mangled_name << param->get_mangled_name().str();
  }

  return mangled_name.str();
}

string mangleTemplateToString(const string &templ_name,
                              const SgTemplateArgumentPtrList &templ_args,
                              const SgScopeStatement *scope) {
  if (templ_name.empty() || scope == NULL) {
    fprintf(stderr,
            "REX_AST_INVARIANT[mangle-template-arguments]: template name or "
            "semantic scope is missing (name=%s scope=%p)\n",
            templ_name.c_str(), static_cast<const void *>(scope));
    ROSE_ABORT();
  }

  string args_mangled;
  if (templ_args.empty() == true) {
    args_mangled = "no_templ_args";
  } else {
    args_mangled =
        mangleTemplateArgsToString(templ_args.begin(), templ_args.end());
  }

  // Compute the name qualification, if any.
  string scope_name = mangleQualifiersToString(scope);

  string mangled_template_name = normalizeNameForMangledNameSupport(templ_name);

  // Compute the final mangled name.
  string mangled_name =
      joinMangledQualifiersToString(scope_name, mangled_template_name);

  if (mangled_name.empty()) {
    fprintf(stderr,
            "REX_AST_INVARIANT[mangle-template-arguments]: nonempty template "
            "name=%s and scope=%p produced an empty identity\n",
            templ_name.c_str(), static_cast<const void *>(scope));
    ROSE_ABORT();
  }

  mangled_name += "__tas__" + args_mangled + "__tae__";

  return mangled_name;
}

string mangleTemplateToString(const string &templ_name,
                              const SgTemplateParameterPtrList &templ_params,
                              const SgScopeStatement *scope) {
  if (templ_name.empty() || scope == NULL) {
    fprintf(stderr,
            "REX_AST_INVARIANT[mangle-template-parameters]: template name or "
            "semantic scope is missing (name=%s scope=%p)\n",
            templ_name.c_str(), static_cast<const void *>(scope));
    ROSE_ABORT();
  }

  string params_mangled;
  if (templ_params.empty() == true) {
    params_mangled = "no_templ_params";
  } else {
    params_mangled =
        mangleTemplateArgsToString(templ_params.begin(), templ_params.end());
  }

  // Compute the name qualification, if any.
  string scope_name = mangleQualifiersToString(scope);

  string mangled_template_name = normalizeNameForMangledNameSupport(templ_name);

  // Compute the final mangled name.
  string mangled_name =
      joinMangledQualifiersToString(scope_name, mangled_template_name);

  if (mangled_name.empty()) {
    fprintf(stderr,
            "REX_AST_INVARIANT[mangle-template-parameters]: nonempty template "
            "name=%s and scope=%p produced an empty identity\n",
            templ_name.c_str(), static_cast<const void *>(scope));
    ROSE_ABORT();
  }

  mangled_name += "__tps__" + params_mangled + "__tpe__";

  return mangled_name;
}

SgName mangleTemplate(const SgName &templ_name,
                      const SgTemplateArgumentPtrList &templ_args,
                      const SgScopeStatement *scope) {
  string mangled_name = templ_name.str();
  mangled_name = mangleTemplateToString(mangled_name, templ_args, scope);

  return SgName(mangled_name.c_str());
}

SgName mangleTemplate(const SgName &templ_name,
                      const SgTemplateParameterPtrList &templ_params,
                      const SgScopeStatement *scope) {
  // DQ (10/29/2015): Added assertion.
  ROSE_ASSERT(scope != NULL);

  string mangled_name =
      mangleTemplateToString(templ_name.getString(), templ_params, scope);

  return SgName(mangled_name.c_str());
}

string mangleTemplateFunctionToString(
    const string &templ_name, const SgTemplateArgumentPtrList &templ_args,
    const SgFunctionType *func_type, const SgScopeStatement *scope) {
  ROSE_ASSERT(templ_name.empty() == false);
  ASSERT_not_null(func_type);
  const SgType *ret_type = func_type->get_return_type();
  ASSERT_not_null(ret_type);
  const string type_name = func_type->get_mangled().getString();
  const string ret_type_name = ret_type->get_mangled().getString();

  // This function's name, transformed.
  string func_name = mangleFunctionNameToString(templ_name, ret_type_name);

  // Compute the final mangled name.
  string mangled_name =
      mangleTemplateToString(func_name, templ_args, scope) + "__" + type_name;

  return mangled_name;
}

SgName mangleTemplateFunction(const string &templ_name,
                              const SgTemplateArgumentPtrList &templ_args,
                              const SgFunctionType *func_type,
                              const SgScopeStatement *scope) {
  return mangleTemplateFunctionToString(templ_name, templ_args, func_type,
                                        scope);
}

/*! Mangles a value expression.
 *
 *  This template function is parameterized by a specific Sage III
 *  value type (derived from SgValueExp), and specifically relies on
 *  the 'get_value ()' member function.
 */
template <class SgValueExpType_>
string mangleSgValueExp(const SgValueExpType_ *expr) {
  ASSERT_not_null(expr);

  ostringstream mangled_name;

  mangled_name.exceptions(std::ios::badbit | std::ios::failbit);
  mangled_name << expr->get_value();
  return mangled_name.str();
}

string mangleValueExp(const SgValueExp *expr) {
  ASSERT_not_null(expr);
  string mangled_name;
  switch (expr->variantT()) {
  case V_SgBoolValExp:
    mangled_name = mangleSgValueExp<SgBoolValExp>(isSgBoolValExp(expr));
    break;
  case V_SgCharVal:
    mangled_name = mangleSgValueExp<SgCharVal>(isSgCharVal(expr));
    break;
  case V_SgChar16Val:
    mangled_name = mangleSgValueExp<SgChar16Val>(isSgChar16Val(expr));
    break;
  case V_SgChar32Val:
    mangled_name = mangleSgValueExp<SgChar32Val>(isSgChar32Val(expr));
    break;
  case V_SgDoubleVal:
    mangled_name = mangleSgValueExp<SgDoubleVal>(isSgDoubleVal(expr));
    break;
  case V_SgEnumVal:
    mangled_name = mangleSgValueExp<SgEnumVal>(isSgEnumVal(expr));
    break;
  case V_SgFloatVal:
    mangled_name = mangleSgValueExp<SgFloatVal>(isSgFloatVal(expr));
    break;
  case V_SgIntVal:
    mangled_name = mangleSgValueExp<SgIntVal>(isSgIntVal(expr));
    break;
  case V_SgLongDoubleVal:
    mangled_name = mangleSgValueExp<SgLongDoubleVal>(isSgLongDoubleVal(expr));
    break;
  case V_SgLongIntVal:
    mangled_name = mangleSgValueExp<SgLongIntVal>(isSgLongIntVal(expr));
    break;
  case V_SgInteger128Val:
    mangled_name = mangleSgValueExp<SgInteger128Val>(isSgInteger128Val(expr));
    break;
  case V_SgLongLongIntVal:
    mangled_name = mangleSgValueExp<SgLongLongIntVal>(isSgLongLongIntVal(expr));
    break;
  case V_SgShortVal:
    mangled_name = mangleSgValueExp<SgShortVal>(isSgShortVal(expr));
    break;
  case V_SgStringVal: {
    auto *literal = isSgStringVal(expr);
    std::ostringstream value;
    value.exceptions(std::ios::badbit | std::ios::failbit);
    value << "string_" << static_cast<int>(literal->get_literal_encoding());
    for (uint32_t unit : literal->get_value())
      value << "_" << std::hex << unit;
    mangled_name = value.str();
    break;
  }
  case V_SgUnsignedCharVal:
    mangled_name =
        mangleSgValueExp<SgUnsignedCharVal>(isSgUnsignedCharVal(expr));
    break;
  case V_SgSignedCharVal:
    mangled_name = mangleSgValueExp<SgSignedCharVal>(isSgSignedCharVal(expr));
    break;
  case V_SgUnsignedIntVal:
    mangled_name = mangleSgValueExp<SgUnsignedIntVal>(isSgUnsignedIntVal(expr));
    break;
  case V_SgUnsignedLongLongIntVal:
    mangled_name = mangleSgValueExp<SgUnsignedLongLongIntVal>(
        isSgUnsignedLongLongIntVal(expr));
    break;
  case V_SgUnsignedLongVal:
    mangled_name =
        mangleSgValueExp<SgUnsignedLongVal>(isSgUnsignedLongVal(expr));
    break;
  case V_SgUnsignedShortVal:
    mangled_name =
        mangleSgValueExp<SgUnsignedShortVal>(isSgUnsignedShortVal(expr));
    break;
  case V_SgWcharVal:
    mangled_name = mangleSgValueExp<SgWcharVal>(isSgWcharVal(expr));
    break;
  case V_SgNullptrValExp: {
    mangled_name = "__nullptr";
    break;
  }

  case V_SgTemplateParameterVal: {
    const SgTemplateParameterVal *parameter = isSgTemplateParameterVal(expr);
    const SgType *value_type = parameter->get_type();
    const SgTemplateParameter *owner = parameter->get_template_parameter();
    const SgTemplateParameter *canonical =
        owner->get_canonicalNonTypeParameter();
    const auto &source_identity = canonical->get_canonical_source_identity();
    if (!source_identity.has_value() ||
        !(owner->get_canonical_source_identity() == source_identity)) {
      std::cerr << "REX_AST_INVARIANT[template-parameter-value-mangle]: "
                   "value has no canonical typed parameter identity"
                << std::endl;
      ROSE_ABORT();
    }
    const SgName type_mangle = value_type->get_mangled();
    if (type_mangle.is_null()) {
      std::cerr << "REX_AST_INVARIANT[template-parameter-value-mangle]: "
                   "template parameter value type has no exact mangled name"
                << std::endl;
      ROSE_ABORT();
    }
    mangled_name =
        "__tpv_source_" + std::to_string(source_identity->expansion_file_id) +
        "_" + std::to_string(source_identity->expansion_file_offset) + "_" +
        std::to_string(source_identity->spelling_file_id) + "_" +
        std::to_string(source_identity->spelling_file_offset) + "__pos_" +
        std::to_string(parameter->get_template_parameter_position()) +
        "__type_" + type_mangle.getString() + "__";
    break;
  }

  default:
    std::cerr << "Error! Unhandled case in mangleValueExp() for "
              << expr->sage_class_name() << std::endl;
    ROSE_ABORT(); // Unhandled case.
  }

  return replaceNonAlphaNum(mangled_name);
}

static void mangleUnaryOp(const SgUnaryOp *uop,
                          std::ostringstream &mangled_name,
                          std::string opname) {
  mangled_name << "_b" << opname << "_"
               << mangleExpression(uop->get_operand_i()) << "_e" << opname
               << "_";
}

static void mangleBinaryOp(const SgBinaryOp *binop,
                           std::ostringstream &mangled_name,
                           std::string opname) {
  mangled_name << "_b" << opname << "_"
               << mangleExpression(binop->get_lhs_operand_i()) << "__"
               << mangleExpression(binop->get_rhs_operand_i()) << "_e" << opname
               << "_";
}

static string
mangleReferencedFunctionDeclaration(const SgFunctionDeclaration *declaration,
                                    const char *referenceKind) {
  if (declaration == nullptr || referenceKind == nullptr) {
    std::cerr << "REX_AST_INVARIANT[function-reference-mangle]: function "
                 "reference has no exact declaration or reference kind"
              << std::endl;
    ROSE_ABORT();
  }
  const string qualifiedName = declaration->get_qualified_name().str();
  const SgFunctionType *type = isSgFunctionType(declaration->get_type());
  if (qualifiedName.empty() || type == nullptr ||
      type->get_return_type() == nullptr ||
      type->get_argument_list() == nullptr) {
    std::cerr << "REX_AST_INVARIANT[function-reference-mangle]: "
              << referenceKind << " '" << declaration->get_name()
              << "' has no exact qualified declaration identity and function "
                 "type"
              << std::endl;
    ROSE_ABORT();
  }
  const SgName typeIdentity = type->get_mangled();
  if (typeIdentity.is_null() || typeIdentity.getString().empty()) {
    std::cerr << "REX_AST_INVARIANT[function-reference-mangle]: "
              << referenceKind << " '" << qualifiedName
              << "' has no exact structural function-type identity"
              << std::endl;
    ROSE_ABORT();
  }
  return "_b" + string(referenceKind) + "_name_" +
         replaceNonAlphaNum(qualifiedName) + "__type_" +
         typeIdentity.getString() + "_e" + referenceKind + "_";
}

string mangleExpression(const SgExpression *expr) {
  ostringstream mangled_name;
  mangled_name.exceptions(std::ios::badbit | std::ios::failbit);
  ROSE_ASSERT(expr != NULL);

  const SgValueExp *val = isSgValueExp(expr);
  if (val != NULL) {
    mangled_name << mangleValueExp(val);
  } else {
    switch (expr->variantT()) {
    case V_SgThisExp: {
      const SgName typeIdentity = expr->get_type()->get_mangled();
      ROSE_ASSERT(!typeIdentity.is_null());
      mangled_name << "_bThis_" << typeIdentity.str() << "_eThis_";
      break;
    }
    case V_SgVarRefExp: {
      const SgVarRefExp *e = isSgVarRefExp(expr);
      SgVariableSymbol *vsym = e->get_symbol();
      ROSE_ASSERT(vsym != NULL);
      SgInitializedName *iname = vsym->get_declaration();
      ROSE_ASSERT(iname != NULL);
      SgFunctionParameterScope *parameterScope =
          isSgFunctionParameterScope(iname->get_scope());
      SgRequiresExpr *requiresExpression =
          parameterScope != NULL
              ? isSgRequiresExpr(parameterScope->get_parent())
              : NULL;
      if (requiresExpression != NULL) {
        SgFunctionParameterList *parameters =
            requiresExpression->get_local_parameter_list();
        if (requiresExpression->get_local_parameter_scope() != parameterScope ||
            parameters == NULL ||
            parameters->get_parent() != requiresExpression ||
            iname->get_type() == NULL) {
          std::cerr << "REX_AST_INVARIANT[requires-local-reference-mangle]: "
                       "requires local variable '"
                    << iname->get_name()
                    << "' has no exact parameter scope, list, or type"
                    << std::endl;
          ROSE_ABORT();
        }
        const SgInitializedNamePtrList &parameterList = parameters->get_args();
        const SgInitializedNamePtrList::const_iterator position =
            std::find(parameterList.begin(), parameterList.end(), iname);
        if (position == parameterList.end() ||
            std::find(std::next(position), parameterList.end(), iname) !=
                parameterList.end()) {
          std::cerr << "REX_AST_INVARIANT[requires-local-reference-mangle]: "
                       "requires local variable '"
                    << iname->get_name()
                    << "' is not owned at one exact parameter-list position"
                    << std::endl;
          ROSE_ABORT();
        }
        // A requires-expression local parameter is a bound variable.  Its
        // spelling and enclosing declaration's output qualification are not
        // semantic identity; independently owned copies are alpha-equivalent
        // when the parameter position and exact type agree.
        mangled_name << "__requires_local_pos_"
                     << std::distance(parameterList.begin(), position)
                     << "__type_" << iname->get_type()->get_mangled().str()
                     << "__";
        break;
      }
      if (auto *parameters =
              isSgFunctionParameterList(iname->get_declaration())) {
        // The parameter list binds references before the callable type exists.
        ROSE_ASSERT(iname->get_parent() == parameters &&
                    iname->get_type() != nullptr);
        const auto &arguments = parameters->get_args();
        const auto position =
            std::find(arguments.begin(), arguments.end(), iname);
        ROSE_ASSERT(position != arguments.end() &&
                    std::find(std::next(position), arguments.end(), iname) ==
                        arguments.end());
        mangled_name << "__function_parameter_pos_"
                     << std::distance(arguments.begin(), position) << "__type_"
                     << iname->get_type()->get_mangled().str() << "__";
        break;
      }
      std::string variable_name = iname->get_mangled_name().str();
      if (variable_name.empty()) {
        std::cerr << "REX_AST_INVARIANT[variable-reference-mangle]: variable '"
                  << iname->get_name()
                  << "' has no exact mangled declaration identity" << std::endl;
        ROSE_ABORT();
      }
      mangled_name << replaceNonAlphaNum(variable_name);
      break;
    }
    case V_SgFunctionRefExp: {
      const SgFunctionRefExp *e = isSgFunctionRefExp(expr);
      SgFunctionSymbol *fsym = e->get_symbol_i();
      ROSE_ASSERT(fsym != NULL);
      SgFunctionDeclaration *fdecl = fsym->get_declaration();
      mangled_name << mangleReferencedFunctionDeclaration(fdecl,
                                                          "FunctionRefExp");
      break;
    }
    case V_SgTemplateFunctionRefExp: {
      const SgTemplateFunctionRefExp *e = isSgTemplateFunctionRefExp(expr);
      SgTemplateFunctionSymbol *fsym = e->get_symbol_i();
      ROSE_ASSERT(fsym != NULL);
      SgFunctionDeclaration *fdecl = fsym->get_declaration();
      mangled_name << mangleReferencedFunctionDeclaration(
          fdecl, "TemplateFunctionRefExp");
      break;
    }
    case V_SgMemberFunctionRefExp: {
      const SgMemberFunctionRefExp *e = isSgMemberFunctionRefExp(expr);
      SgMemberFunctionSymbol *fsym = e->get_symbol_i();
      ROSE_ASSERT(fsym != NULL);
      SgMemberFunctionDeclaration *fdecl = fsym->get_declaration();
      mangled_name << mangleReferencedFunctionDeclaration(
          fdecl, "MemberFunctionRefExp");
      break;
    }
    case V_SgTemplateMemberFunctionRefExp: {
      const SgTemplateMemberFunctionRefExp *e =
          isSgTemplateMemberFunctionRefExp(expr);
      SgTemplateMemberFunctionSymbol *fsym = e->get_symbol_i();
      ROSE_ASSERT(fsym != NULL);
      SgMemberFunctionDeclaration *fdecl = fsym->get_declaration();
      mangled_name << mangleReferencedFunctionDeclaration(
          fdecl, "TemplateMemberFunctionRefExp");
      break;
    }
    case V_SgNonrealRefExp: {
      const SgNonrealRefExp *nrref = isSgNonrealRefExp(expr);
      if (nrref->get_resolved_variable_declaration() != nullptr) {
        auto *name = SageInterface::requireResolvedVariableTemplateReference(
            nrref, "mangleExpression");
        mangled_name << name->get_mangled_name().str();
        break;
      }
      SgNonrealSymbol *nrsym = nrref->get_symbol();
      ROSE_ASSERT(nrsym != NULL);
      SgNonrealDecl *nrdecl = nrsym->get_declaration();
      ROSE_ASSERT(nrdecl != NULL);
      if (nrref->get_name_kind() == SgNonrealRefExp::e_destructor_id)
        mangled_name << "_destructor_id_";
      mangled_name << nrdecl->get_mangled_name().str();
      const SgTemplateArgumentPtrList &templateArguments =
          nrref->get_templateArguments();
      if (!templateArguments.empty()) {
        mangled_name << "__bNonrealTemplateArguments_"
                     << mangleTemplateArgsToString(templateArguments.begin(),
                                                   templateArguments.end())
                     << "_eNonrealTemplateArguments__";
      }
      break;
    }

    case V_SgCastExp: {
      const SgCastExp *cast = isSgCastExp(expr);
      ROSE_ASSERT(cast != NULL);
      cast->validate_semantic_conversion();
      SgExpression *op = cast->get_operand_i();
      ROSE_ASSERT(op != NULL);
      SgType *cast_type = cast->get_type();
      ROSE_ASSERT(cast_type != NULL);
      mangled_name << "_bCastExp_surface_"
                   << static_cast<int>(cast->get_cast_type()) << "_semantic_"
                   << static_cast<int>(cast->get_semantic_conversion_kind())
                   << "_category_"
                   << static_cast<int>(cast->get_value_category())
                   << "_base_path_";
      for (SgType *base : cast->get_conversion_base_path()) {
        ROSE_ASSERT(base != nullptr);
        mangled_name << base->get_mangled().str() << "_step_";
      }
      mangled_name << "_operand_" << mangleExpression(op) << "_totype_"
                   << cast_type->get_mangled().str() << "_eCastExp_";
      break;
    }

    case V_SgNotOp:
      mangleUnaryOp((const SgUnaryOp *)expr, mangled_name, "NotOp");
      break;
    case V_SgBitComplementOp:
      mangleUnaryOp((const SgUnaryOp *)expr, mangled_name, "BitComplementOp");
      break;
    case V_SgMinusOp:
      mangleUnaryOp((const SgUnaryOp *)expr, mangled_name, "MinusOp");
      break;
    case V_SgUnaryAddOp:
      mangleUnaryOp((const SgUnaryOp *)expr, mangled_name, "UnaryAddOp");
      break;
    case V_SgAddressOfOp:
      mangleUnaryOp((const SgUnaryOp *)expr, mangled_name, "AddressOfOp");
      break;
    case V_SgPointerDerefExp:
      mangleUnaryOp((const SgUnaryOp *)expr, mangled_name, "PointerDerefExp");
      break;
    case V_SgPlusPlusOp:
      mangleUnaryOp((const SgUnaryOp *)expr, mangled_name, "PlusPlusOp");
      break;
    case V_SgMinusMinusOp:
      mangleUnaryOp((const SgUnaryOp *)expr, mangled_name, "MinusMinusOp");
      break;
    case V_SgVectorElementExp: {
      const auto *access = isSgVectorElementExp(expr);
      ROSE_ASSERT(access != nullptr && !access->get_components().empty());
      mangleUnaryOp(access, mangled_name, "VectorElementExp");
      mangled_name << access->get_is_arrow() << '_'
                   << access->get_selector_kind() << '_';
      for (unsigned component : access->get_components())
        mangled_name << component << '_';
      mangled_name << "end_components_";
      break;
    }

    case V_SgAddOp:
      mangleBinaryOp((const SgBinaryOp *)expr, mangled_name, "AddOp");
      break;
    case V_SgAndOp:
      mangleBinaryOp((const SgBinaryOp *)expr, mangled_name, "AndOp");
      break;
    case V_SgBitAndOp:
      mangleBinaryOp((const SgBinaryOp *)expr, mangled_name, "BitAndOp");
      break;
    case V_SgBitXorOp:
      mangleBinaryOp((const SgBinaryOp *)expr, mangled_name, "BitXorOp");
      break;
    case V_SgBitOrOp:
      mangleBinaryOp((const SgBinaryOp *)expr, mangled_name, "BitOrOp");
      break;
    case V_SgOrOp:
      mangleBinaryOp((const SgBinaryOp *)expr, mangled_name, "OrOp");
      break;
    case V_SgMultiplyOp:
      mangleBinaryOp((const SgBinaryOp *)expr, mangled_name, "MultiplyOp");
      break;
    case V_SgDivideOp:
      mangleBinaryOp((const SgBinaryOp *)expr, mangled_name, "DivideOp");
      break;
    case V_SgEqualityOp:
      mangleBinaryOp((const SgBinaryOp *)expr, mangled_name, "EqualityOp");
      break;
    case V_SgSubtractOp:
      mangleBinaryOp((const SgBinaryOp *)expr, mangled_name, "SubtractOp");
      break;
    case V_SgDotExp:
      mangleBinaryOp((const SgBinaryOp *)expr, mangled_name, "DotExp");
      break;
    case V_SgScopeOp:
      mangleBinaryOp(isSgScopeOp(expr), mangled_name, "ScopeOp");
      break;
    case V_SgModOp:
      mangleBinaryOp((const SgBinaryOp *)expr, mangled_name, "ModOp");
      break;
    case V_SgArrowExp:
      mangleBinaryOp((const SgBinaryOp *)expr, mangled_name, "ArrowExp");
      break;
    case V_SgLessThanOp:
      mangleBinaryOp((const SgBinaryOp *)expr, mangled_name, "LessThanOp");
      break;
    case V_SgLessOrEqualOp:
      mangleBinaryOp((const SgBinaryOp *)expr, mangled_name, "LessOrEqualOp");
      break;
    case V_SgGreaterThanOp:
      mangleBinaryOp((const SgBinaryOp *)expr, mangled_name, "GreaterThanOp");
      break;
    case V_SgGreaterOrEqualOp:
      mangleBinaryOp((const SgBinaryOp *)expr, mangled_name,
                     "GreaterOrEqualOp");
      break;
    case V_SgAssignOp:
      mangleBinaryOp((const SgBinaryOp *)expr, mangled_name, "AssignOp");
      break;
    case V_SgPlusAssignOp:
      mangleBinaryOp((const SgBinaryOp *)expr, mangled_name, "PlusAssignOp");
      break;
    case V_SgMinusAssignOp:
      mangleBinaryOp((const SgBinaryOp *)expr, mangled_name, "MinusAssignOp");
      break;
    case V_SgAndAssignOp:
      mangleBinaryOp((const SgBinaryOp *)expr, mangled_name, "AndAssignOp");
      break;
    case V_SgXorAssignOp:
      mangleBinaryOp((const SgBinaryOp *)expr, mangled_name, "XorAssignOp");
      break;
    case V_SgIorAssignOp:
      mangleBinaryOp((const SgBinaryOp *)expr, mangled_name, "IorAssignOp");
      break;
    case V_SgMultAssignOp:
      mangleBinaryOp((const SgBinaryOp *)expr, mangled_name, "MultAssignOp");
      break;
    case V_SgDivAssignOp:
      mangleBinaryOp((const SgBinaryOp *)expr, mangled_name, "DivAssignOp");
      break;
    case V_SgLshiftAssignOp:
      mangleBinaryOp((const SgBinaryOp *)expr, mangled_name, "LshiftAssignOp");
      break;
    case V_SgRshiftAssignOp:
      mangleBinaryOp((const SgBinaryOp *)expr, mangled_name, "RshiftAssignOp");
      break;
    case V_SgModAssignOp:
      mangleBinaryOp((const SgBinaryOp *)expr, mangled_name, "ModAssignOp");
      break;
    case V_SgNotEqualOp:
      mangleBinaryOp((const SgBinaryOp *)expr, mangled_name, "NotEqualOp");
      break;
    case V_SgRshiftOp:
      mangleBinaryOp((const SgBinaryOp *)expr, mangled_name, "RshiftOp");
      break;
    case V_SgLshiftOp:
      mangleBinaryOp((const SgBinaryOp *)expr, mangled_name, "LshiftOp");
      break;
    case V_SgCommaOpExp:
      mangleBinaryOp((const SgBinaryOp *)expr, mangled_name, "CommaOpExp");
      break;
    case V_SgDotStarOp:
      mangleBinaryOp((const SgBinaryOp *)expr, mangled_name, "DotStarOp");
      break;
    case V_SgArrowStarOp:
      mangleBinaryOp((const SgBinaryOp *)expr, mangled_name, "ArrowStarOp");
      break;

    case V_SgNoexceptOp: {
      const SgNoexceptOp *e = isSgNoexceptOp(expr);
      mangled_name << "_bNoexceptOp_" << mangleExpression(e->get_operand_expr())
                   << "_eNoexceptOp_";
      break;
    }
    case V_SgSimpleRequirement: {
      const SgSimpleRequirement *requirement = isSgSimpleRequirement(expr);
      if (requirement == NULL || requirement->get_expression() == NULL) {
        fprintf(stderr,
                "REX_AST_INVARIANT[mangle-simple-requirement]: malformed "
                "simple requirement=%p\n",
                static_cast<const void *>(requirement));
        ROSE_ABORT();
      }
      mangled_name << "_bSimpleRequirement_"
                   << mangleExpression(requirement->get_expression())
                   << "_eSimpleRequirement_";
      break;
    }
    case V_SgTypeRequirement: {
      const SgTypeRequirement *requirement = isSgTypeRequirement(expr);
      if (requirement == NULL || requirement->get_required_type() == NULL) {
        fprintf(stderr,
                "REX_AST_INVARIANT[mangle-type-requirement]: malformed type "
                "requirement=%p\n",
                static_cast<const void *>(requirement));
        ROSE_ABORT();
      }
      mangled_name << "_bTypeRequirement_"
                   << requirement->get_required_type()->get_mangled().str()
                   << "_eTypeRequirement_";
      break;
    }
    case V_SgCompoundRequirement: {
      const SgCompoundRequirement *requirement = isSgCompoundRequirement(expr);
      if (requirement == NULL || requirement->get_expression() == NULL) {
        fprintf(stderr,
                "REX_AST_INVARIANT[mangle-compound-requirement]: malformed "
                "compound requirement=%p\n",
                static_cast<const void *>(requirement));
        ROSE_ABORT();
      }
      mangled_name << "_bCompoundRequirement_"
                   << mangleExpression(requirement->get_expression())
                   << (requirement->get_noexcept_required() ? "_noexcept_"
                                                            : "_maythrow_");
      if (requirement->get_type_constraint() != NULL) {
        mangled_name << "_typeConstraint_"
                     << mangleExpression(requirement->get_type_constraint());
      } else {
        mangled_name << "_noTypeConstraint_";
      }
      mangled_name << "_eCompoundRequirement_";
      break;
    }
    case V_SgNestedRequirement: {
      const SgNestedRequirement *requirement = isSgNestedRequirement(expr);
      if (requirement == NULL || requirement->get_constraint() == NULL) {
        fprintf(stderr,
                "REX_AST_INVARIANT[mangle-nested-requirement]: malformed "
                "nested requirement=%p\n",
                static_cast<const void *>(requirement));
        ROSE_ABORT();
      }
      mangled_name << "_bNestedRequirement_"
                   << mangleExpression(requirement->get_constraint())
                   << "_eNestedRequirement_";
      break;
    }
    case V_SgRequiresExpr: {
      const SgRequiresExpr *requiresExpression = isSgRequiresExpr(expr);
      if (requiresExpression == NULL ||
          requiresExpression->get_requirements() == NULL ||
          requiresExpression->get_requirements()->get_expressions().empty()) {
        fprintf(stderr,
                "REX_AST_INVARIANT[mangle-requires-expression]: malformed "
                "requires expression=%p\n",
                static_cast<const void *>(requiresExpression));
        ROSE_ABORT();
      }

      mangled_name << "_bRequiresExpression_";
      if (const SgFunctionParameterList *parameters =
              requiresExpression->get_local_parameter_list()) {
        if (auto *written =
                requiresExpression->get_local_parameter_list_syntax()) {
          ROSE_ASSERT(written != parameters &&
                      written->get_parent() == requiresExpression &&
                      written->get_args().size() ==
                          parameters->get_args().size());
          parameters = written;
        }
        mangled_name << "_bLocalParameters_";
        const SgInitializedNamePtrList &parameterList = parameters->get_args();
        for (SgInitializedNamePtrList::const_iterator i = parameterList.begin();
             i != parameterList.end(); ++i) {
          if (i != parameterList.begin()) {
            mangled_name << "__sep__";
          }
          const SgInitializedName *parameter = *i;
          if (parameter == NULL || parameter->get_type() == NULL) {
            fprintf(stderr,
                    "REX_AST_INVARIANT[mangle-requires-parameter]: requires "
                    "expression=%p has malformed local parameter\n",
                    static_cast<const void *>(requiresExpression));
            ROSE_ABORT();
          }
          mangled_name << parameter->get_type()->get_mangled().str() << "_"
                       << replaceNonAlphaNum(parameter->get_name().str());
        }
        mangled_name << "_eLocalParameters_";
      } else {
        mangled_name << "_noLocalParameters_";
      }
      mangled_name << mangleExpression(requiresExpression->get_requirements())
                   << "_eRequiresExpression_";
      break;
    }
    case V_SgSizeOfOp: {
      const SgSizeOfOp *e = isSgSizeOfOp(expr);
      mangled_name << "_bSizeOfOp_" << e->get_operator_kind() << "_";
      if (e->get_operand_expr() != NULL) {
        mangled_name << "_expr_" << mangleExpression(e->get_operand_expr());
      } else if (e->get_operand_type()) {
        mangled_name << "_type_"
                     << e->get_operand_type()->get_mangled().getString();
      } else {
        ROSE_ABORT();
      }
      mangled_name << "_eSizeOfOp_";
      break;
    }
    case V_SgAlignOfOp: {
      const SgAlignOfOp *e = isSgAlignOfOp(expr);
      mangled_name << "_bAlignOfOp_" << e->get_operator_kind() << "_";
      if (e->get_operand_expr() != NULL) {
        mangled_name << "_expr_" << mangleExpression(e->get_operand_expr());
      } else if (e->get_operand_type()) {
        mangled_name << "_type_"
                     << e->get_operand_type()->get_mangled().getString();
      } else {
        ROSE_ABORT();
      }
      mangled_name << "_eAlignOfOp_";
      break;
    }

    case V_SgConditionalExp: {
      const SgConditionalExp *e = isSgConditionalExp(expr);
      e->validate();
      mangled_name << "_bConditionalExp_";
      mangled_name << static_cast<int>(e->get_operator_kind()) << "__";
      mangled_name << mangleExpression(e->get_conditional_exp());
      if (e->get_operator_kind() ==
          SgConditionalExp::e_conditional_operator_standard) {
        mangled_name << "__";
        mangled_name << mangleExpression(e->get_true_exp());
      }
      mangled_name << "__";
      mangled_name << mangleExpression(e->get_false_exp());
      mangled_name << "_eConditionalExp_";
      break;
    }
    case V_SgFunctionCallExp: {
      const SgFunctionCallExp *e = isSgFunctionCallExp(expr);
      std::string mangledFn = mangleExpression(e->get_function());
      std::string mangledArgs = mangleExpression(e->get_args());

      mangled_name << "_bFunctionCallExp_" << mangledFn << "__" << mangledArgs
                   << "_eFunctionCallExp_";
      break;
    }
    case V_SgConstructorInitializer: {
      const SgConstructorInitializer *e = isSgConstructorInitializer(expr);
      SgType *type = e != nullptr ? e->get_type() : nullptr;
      SgExprListExp *arguments = e != nullptr ? e->get_args() : nullptr;
      if (e == nullptr || type == nullptr || isSgTypeUnknown(type) != nullptr ||
          isSgTypeDefault(type) != nullptr || arguments == nullptr ||
          arguments->get_parent() != e) {
        fprintf(stderr,
                "REX_AST_INVARIANT[constructor-initializer-mangle]: "
                "initializer=%p type=%p arguments=%p parent=%p is malformed\n",
                static_cast<const void *>(e), static_cast<void *>(type),
                static_cast<void *>(arguments),
                static_cast<void *>(
                    arguments != nullptr ? arguments->get_parent() : nullptr));
        ROSE_ABORT();
      }

      mangled_name << "_bConstructorInitializer_";
      mangled_name << "_type_" << type->get_mangled().getString();
      mangled_name << "_arguments_" << mangleExpression(arguments);
      if (SgMemberFunctionDeclaration *declaration = e->get_declaration()) {
        mangled_name << "_constructor_"
                     << mangleReferencedFunctionDeclaration(
                            declaration, "ConstructorInitializerDeclaration");
      } else {
        mangled_name << "_noConstructorDeclaration_";
      }
      mangled_name << "_braced_" << (e->get_is_braced_initialized() ? 1 : 0);
      mangled_name << "_eConstructorInitializer_";
      break;
    }
    case V_SgPntrArrRefExp: {
      const SgPntrArrRefExp *e = isSgPntrArrRefExp(expr);
      mangled_name << "_bPntrArrRefExp_"
                   << mangleExpression(e->get_lhs_operand_i()) << "__"
                   << mangleExpression(e->get_rhs_operand_i())
                   << "_ePntrArrRefExp_";
      break;
    }
    case V_SgTypeTraitBuiltinOperator: {
      const SgTypeTraitBuiltinOperator *e = isSgTypeTraitBuiltinOperator(expr);
      mangled_name << "_bTypeTraitBuiltinOperator_"
                   << e->get_name().getString();
      mangled_name << "_bExpressionPtrList_";
      SgExpressionPtrList::const_iterator it;
      const SgExpressionPtrList &args = e->get_args();
      for (it = args.begin(); it != args.end(); it++) {
        if (it != args.begin())
          mangled_name << "__sep__";
        SgTypeExpression *type_operand = isSgTypeExpression(*it);
        SgExpression *eit = *it;
        if (type_operand != nullptr) {
          SgType *represented_type = type_operand->get_represented_type();
          if (represented_type == nullptr ||
              isSgTypeUnknown(represented_type) != nullptr ||
              isSgTypeDefault(represented_type) != nullptr ||
              type_operand->get_parent() != e) {
            fprintf(stderr,
                    "REX_AST_INVARIANT[type-trait-mangling]: builtin=%p "
                    "contains a malformed typed operand occurrence\n",
                    static_cast<const void *>(e));
            ROSE_ABORT();
          }
          mangled_name << "_bTypeOperand_"
                       << represented_type->get_mangled().getString()
                       << "_eTypeOperand_";
        } else if (eit != NULL) {
          mangled_name << mangleExpression(eit);
        } else {
          fprintf(stderr,
                  "REX_AST_INVARIANT[type-trait-mangling]: builtin=%p "
                  "contains a raw type or non-expression argument\n",
                  static_cast<const void *>(e));
          ROSE_ABORT();
        }
      }
      mangled_name << "_eExpressionPtrList_";
      mangled_name << "_eTypeTraitBuiltinOperator_";
      break;
    }
    case V_SgTypeExpression: {
      const SgTypeExpression *type_expression = isSgTypeExpression(expr);
      SgType *represented_type = type_expression->get_represented_type();
      if (represented_type == nullptr ||
          isSgTypeUnknown(represented_type) != nullptr ||
          isSgTypeDefault(represented_type) != nullptr) {
        fprintf(stderr,
                "REX_AST_INVARIANT[type-expression-mangling]: expression=%p "
                "has no exact represented type\n",
                static_cast<const void *>(type_expression));
        ROSE_ABORT();
      }
      mangled_name << "_bTypeExpression_"
                   << represented_type->get_mangled().getString()
                   << "_eTypeExpression_";
      break;
    }
    case V_SgExprListExp: {
      const SgExprListExp *e = isSgExprListExp(expr);
      mangled_name << "_bExprListExp_";
      SgExpressionPtrList::const_iterator it;
      const SgExpressionPtrList &args = e->get_expressions();
      for (it = args.begin(); it != args.end(); it++) {
        if (it != args.begin())
          mangled_name << "__sep__";
        mangled_name << mangleExpression(*it);
      }
      mangled_name << "_eExprListExp_";
      break;
    }
    case V_SgAggregateInitializer: {
      const SgAggregateInitializer *e = isSgAggregateInitializer(expr);
      mangled_name << "_bAggregateInitializer_"
                   << mangleExpression(e->get_initializers())
                   << "_eAggregateInitializer_";
      break;
    }
    case V_SgBracedInitializer: {
      const SgBracedInitializer *e = isSgBracedInitializer(expr);
      mangled_name << "_bBracedInitializer_"
                   << mangleExpression(e->get_initializers())
                   << "_eBracedInitializer_";
      break;
    }
    case V_SgAssignInitializer: {
      const SgAssignInitializer *e = isSgAssignInitializer(expr);
      mangled_name << "_bAssignInitializer_"
                   << mangleExpression(e->get_operand_i())
                   << "_eAssignInitializer_";
      break;
    }
    case V_SgNewExp: {
      const SgNewExp *e = isSgNewExp(expr);
      SgType *specifiedType = e != nullptr ? e->get_specified_type() : nullptr;
      SgExprListExp *placementArguments =
          e != nullptr ? e->get_placement_args() : nullptr;
      SgConstructorInitializer *constructorArguments =
          e != nullptr ? e->get_constructor_args() : nullptr;
      SgExpression *builtinArguments =
          e != nullptr ? e->get_builtin_args() : nullptr;
      const short globalSpecifier =
          e != nullptr ? e->get_need_global_specifier() : -1;
      if (e == nullptr || specifiedType == nullptr ||
          isSgTypeUnknown(specifiedType) != nullptr ||
          isSgTypeDefault(specifiedType) != nullptr ||
          (placementArguments != nullptr &&
           placementArguments->get_parent() != e) ||
          (constructorArguments != nullptr &&
           constructorArguments->get_parent() != e) ||
          (builtinArguments != nullptr &&
           builtinArguments->get_parent() != e) ||
          (e->get_array_bound() != nullptr &&
           e->get_array_bound()->get_parent() != e) ||
          (globalSpecifier != 0 && globalSpecifier != 1)) {
        fprintf(stderr,
                "REX_AST_INVARIANT[new-expression-mangle]: expression=%p "
                "type=%p placement=%p constructor=%p builtin=%p global=%d is "
                "malformed\n",
                static_cast<const void *>(e),
                static_cast<void *>(specifiedType),
                static_cast<void *>(placementArguments),
                static_cast<void *>(constructorArguments),
                static_cast<void *>(builtinArguments),
                static_cast<int>(globalSpecifier));
        ROSE_ABORT();
      }
      mangled_name << "_bNewExpr_type_"
                   << specifiedType->get_mangled().getString() << "_result_"
                   << e->get_type()->get_mangled().getString() << "_bound_"
                   << (e->get_array_bound() != nullptr
                           ? mangleExpression(e->get_array_bound())
                           : "_noArrayBound_");
      mangled_name << "_placement_"
                   << (placementArguments != nullptr
                           ? mangleExpression(placementArguments)
                           : "_noPlacementArguments_");
      mangled_name << "_constructor_"
                   << (constructorArguments != nullptr
                           ? mangleExpression(constructorArguments)
                           : "_noConstructorArguments_");
      mangled_name << "_builtin_"
                   << (builtinArguments != nullptr
                           ? mangleExpression(builtinArguments)
                           : "_noBuiltinArguments_");
      mangled_name << "_global_" << static_cast<int>(globalSpecifier)
                   << "_parenthesized_type_"
                   << (e->get_type_id_is_parenthesized() ? 1 : 0)
                   << "_array_bound_source_form_"
                   << e->get_array_bound_source_form()
                   << "_implicit_allocation_arguments_"
                   << e->get_allocation_implicit_argument_count();
      if (SgFunctionDeclaration *newOperator =
              e->get_newOperatorDeclaration()) {
        mangled_name << "_operator_"
                     << mangleReferencedFunctionDeclaration(
                            newOperator, "NewOperatorDeclaration");
      } else {
        mangled_name << "_implicitOperator_";
      }
      mangled_name << "_eNewExpr_";
      break;
    }
    case V_SgFunctionParameterRefExp: {
      const SgFunctionParameterRefExp *e = isSgFunctionParameterRefExp(expr);
      SgType *parameterType = e->get_type();
      mangled_name << "_bFunctionParameterRefExp_number_"
                   << e->get_parameter_number() << "_levels_"
                   << e->get_parameter_levels_up() << "_type_"
                   << parameterType->get_mangled().getString()
                   << "_eFunctionParameterRefExp_";
      break;
    }
    case V_SgPseudoDestructorRefExp: {
      const SgPseudoDestructorRefExp *e = isSgPseudoDestructorRefExp(expr);
      ASSERT_not_null(e->get_object_type());
      mangled_name << "_bPseudoDestructorRefExp_";
      mangled_name << e->get_object_type()->get_mangled().getString();
      mangled_name << "_ePseudoDestructorRefExp_";
      break;
    }
    case V_SgNullExpression: {
      mangled_name << "_bNullExpr_";
      break;
    }
    case V_SgColonShapeExp:
      mangled_name << "_bFortranColonShape_eFortranColonShape_";
      break;
    case V_SgAsteriskShapeExp:
      mangled_name << "_bFortranAsteriskShape_eFortranAsteriskShape_";
      break;
    case V_SgAssumedRankExp:
      mangled_name << "_bFortranAssumedRank_eFortranAssumedRank_";
      break;
    case V_SgSubscriptExpression: {
      const SgSubscriptExpression *subscript = isSgSubscriptExpression(expr);
      if (subscript == NULL || subscript->get_lowerBound() == NULL ||
          subscript->get_upperBound() == NULL ||
          subscript->get_stride() == NULL) {
        fprintf(stderr,
                "REX_AST_INVARIANT[mangle-fortran-subscript]: malformed "
                "subscript expression=%p\n",
                static_cast<const void *>(subscript));
        ROSE_ABORT();
      }
      mangled_name << "_bFortranSubscript_lower_"
                   << mangleExpression(subscript->get_lowerBound()) << "_upper_"
                   << mangleExpression(subscript->get_upperBound())
                   << "_stride_" << mangleExpression(subscript->get_stride())
                   << "_eFortranSubscript_";
      break;
    }
    case V_SgFoldExpression: {
      const SgFoldExpression *fold = isSgFoldExpression(expr);
      mangled_name << "_bfold_" << mangleExpression(fold->get_operands())
                   << "_op_" << fold->get_operator_token() << "_assoc_"
                   << (fold->get_is_left_associative() ? "left" : "right")
                   << "_efold_";
      break;
    }
    case V_SgPackExpansionExpr: {
      const SgPackExpansionExpr *pack_expansion = isSgPackExpansionExpr(expr);
      ASSERT_not_null(pack_expansion->get_pattern_expression());
      mangled_name << "_bPackExpansionExpr_"
                   << mangleExpression(pack_expansion->get_pattern_expression())
                   << "_ePackExpansionExpr_";
      break;
    }
    case V_SgCxxDefaultInitExp: {
      auto *use = isSgCxxDefaultInitExp(expr);
      use->validate();
      auto *parameter = use->get_source_declaration();
      if (use->get_source_kind() == SgCxxDefaultInitExp::e_member) {
        mangled_name << "_bDefaultMember_" << parameter->get_mangled_name()
                     << "_expression_"
                     << mangleExpression(use->get_expression())
                     << "_eDefaultMember_";
        break;
      }
      auto *list = isSgFunctionParameterList(parameter->get_parent());
      const auto position = std::find(list->get_args().begin(),
                                      list->get_args().end(), parameter);
      auto *owner = isSgFunctionDeclaration(parameter->get_declptr());
      mangled_name << "_bDefaultArgument_" << owner->get_mangled_name()
                   << "_parameter_"
                   << std::distance(list->get_args().begin(), position)
                   << "_expression_" << mangleExpression(use->get_expression())
                   << "_eDefaultArgument_";
      break;
    }
    case V_SgMacroExpansionExp: {
      const SgMacroExpansionExp *macro = isSgMacroExpansionExp(expr);
      mangled_name << mangleExpression(
          macro->get_expanded_expression_checked());
      break;
    }
    default: {
      printf("In mangleExpression: Unsupported expression %p (%s)\n", expr,
             expr ? expr->class_name().c_str() : "");
      ROSE_ABORT();
    }
    }
  }

  return mangled_name.str();
}

bool declarationHasTranslationUnitScope(const SgDeclarationStatement *decl) {
  ROSE_ASSERT(decl != NULL);
  SgNode *declParent = decl->get_parent();

  if (declParent == NULL) {
    const SgFunctionDeclaration *pendingDefining =
        isSgFunctionDeclaration(decl);
    const SgFunctionDeclaration *pendingCanonical =
        pendingDefining != NULL
            ? isSgFunctionDeclaration(
                  pendingDefining->get_firstNondefiningDeclaration())
            : NULL;
    const SgFunctionDefinition *pendingDefinition =
        pendingDefining != NULL ? pendingDefining->get_definition() : NULL;
    const SgAuxiliaryDeclarationList *pendingCanonicalOwner =
        pendingCanonical != NULL
            ? isSgAuxiliaryDeclarationList(pendingCanonical->get_parent())
            : NULL;
    const bool exactPendingRootCopyFamily =
        pendingDefining != NULL && pendingCanonical != NULL &&
        pendingCanonical != pendingDefining && pendingDefinition != NULL &&
        pendingDefining->get_definingDeclaration() == pendingDefining &&
        pendingCanonical->get_firstNondefiningDeclaration() ==
            pendingCanonical &&
        pendingCanonical->get_definingDeclaration() == pendingDefining &&
        pendingDefinition->get_parent() == pendingDefining &&
        pendingDefinition->get_declaration() == pendingDefining &&
        pendingDefining->get_scope() != NULL &&
        pendingCanonical->get_scope() == pendingDefining->get_scope() &&
        pendingCanonicalOwner != NULL &&
        pendingCanonicalOwner->get_parent() == NULL &&
        pendingCanonicalOwner->get_declarations().size() == 1 &&
        pendingCanonicalOwner->get_declarations().front() == pendingCanonical;
    if (!exactPendingRootCopyFamily) {
      fprintf(
          stderr,
          "REX_AST_INVARIANT[mangling-declaration-parent]: "
          "declaration=%p type=%s name=%s has no structural owner\n",
          static_cast<const void *>(decl), decl->class_name().c_str(),
          SageInterface::get_name(const_cast<SgDeclarationStatement *>(decl))
              .c_str());
      ROSE_ABORT();
    }
    declParent = pendingDefining->get_scope();
  }

  if (SgAuxiliaryDeclarationList *auxiliary =
          isSgAuxiliaryDeclarationList(declParent)) {
    SgScopeStatement *semanticOwner =
        isSgScopeStatement(auxiliary->get_parent());
    const SgDeclarationStatementPtrList &declarations =
        auxiliary->get_declarations();
    const SgFunctionDeclaration *pendingCanonical =
        isSgFunctionDeclaration(decl);
    const SgFunctionDeclaration *pendingDefining =
        pendingCanonical != NULL
            ? isSgFunctionDeclaration(
                  pendingCanonical->get_definingDeclaration())
            : NULL;
    const bool exactPendingCopyFamily =
        semanticOwner == NULL && declarations.size() == 1 &&
        declarations.front() == decl && pendingCanonical != NULL &&
        pendingCanonical->get_firstNondefiningDeclaration() ==
            pendingCanonical &&
        pendingDefining != NULL && pendingDefining != pendingCanonical &&
        pendingDefining->get_firstNondefiningDeclaration() ==
            pendingCanonical &&
        pendingDefining->get_definingDeclaration() == pendingDefining &&
        pendingDefining->get_definition() != NULL &&
        pendingDefining->get_definition()->get_declaration() ==
            pendingDefining &&
        pendingCanonical->get_scope() != NULL &&
        pendingDefining->get_scope() == pendingCanonical->get_scope();
    if (exactPendingCopyFamily) {
      semanticOwner = pendingCanonical->get_scope();
    }
    if (semanticOwner == NULL ||
        (!exactPendingCopyFamily &&
         semanticOwner->get_auxiliary_declarations() != auxiliary) ||
        decl->get_scope() != semanticOwner ||
        std::count(declarations.begin(), declarations.end(), decl) != 1) {
      fprintf(stderr,
              "REX_AST_INVARIANT[mangling-auxiliary-owner]: declaration=%p "
              "type=%s auxiliary=%p auxiliary_parent=%p semantic_scope=%p "
              "declaration_count=%zu pending_canonical=%p "
              "canonical_first=%p pending_defining=%p defining_first=%p "
              "defining_self=%p definition=%p definition_declaration=%p "
              "defining_scope=%p exact_pending_copy_family=%d has malformed "
              "semantic ownership\n",
              static_cast<const void *>(decl), decl->class_name().c_str(),
              static_cast<void *>(auxiliary),
              static_cast<void *>(auxiliary->get_parent()),
              static_cast<void *>(decl->get_scope()), declarations.size(),
              static_cast<const void *>(pendingCanonical),
              static_cast<const void *>(
                  pendingCanonical != NULL
                      ? pendingCanonical->get_firstNondefiningDeclaration()
                      : NULL),
              static_cast<const void *>(pendingDefining),
              static_cast<const void *>(
                  pendingDefining != NULL
                      ? pendingDefining->get_firstNondefiningDeclaration()
                      : NULL),
              static_cast<const void *>(
                  pendingDefining != NULL
                      ? pendingDefining->get_definingDeclaration()
                      : NULL),
              static_cast<const void *>(pendingDefining != NULL
                                            ? pendingDefining->get_definition()
                                            : NULL),
              static_cast<const void *>(
                  pendingDefining != NULL &&
                          pendingDefining->get_definition() != NULL
                      ? pendingDefining->get_definition()->get_declaration()
                      : NULL),
              static_cast<const void *>(pendingDefining != NULL
                                            ? pendingDefining->get_scope()
                                            : NULL),
              exactPendingCopyFamily ? 1 : 0);
      ROSE_ABORT();
    }
    declParent = semanticOwner;
  }

  VariantT declParentV = declParent->variantT();

  if (declParentV == V_SgGlobal ||
      declParentV == V_SgNamespaceDefinitionStatement) {
    if (declParentV == V_SgGlobal) {
      if (const auto *function = isSgFunctionDeclaration(decl);
          function != nullptr && function->get_name() == "main")
        return true;
    }

    // If the declaration is static (in the C sense), it will have translation
    // unit scope.
    if (decl->get_declarationModifier().get_storageModifier().isStatic())
      return true;

    // Likewise if the declaration is an inline function.
    if (const SgFunctionDeclaration *fnDecl = isSgFunctionDeclaration(decl)) {
      if (fnDecl->get_functionModifier().isInline())
        return true;
    }
  }

  // Likewise if the declaration is an anonymous namespace
  if (const SgNamespaceDeclarationStatement *nsDecl =
          isSgNamespaceDeclarationStatement(decl)) {
    if (nsDecl->get_isUnnamedNamespace())
      return true;
  }

  return false;
}

std::string
mangleTranslationUnitQualifiers(const SgDeclarationStatement *decl) {
  // Use the owning file ID for declarations selected as translation-unit local.
  if (declarationHasTranslationUnitScope(decl) == true) {
    // Uses global (or enclosing) scope as reference because forward declaration
    // can be compiler generated
    SgLocatedNode *lnode_ref = SageInterface::getGlobalScope(decl);
    if (lnode_ref == NULL) {
      lnode_ref = SageInterface::getEnclosingScope(
          const_cast<SgDeclarationStatement *>(decl));
    }
    ROSE_ASSERT(lnode_ref != NULL);

    int file_id = lnode_ref->get_file_info()->get_file_id();
    if (const auto *function = isSgFunctionDeclaration(decl);
        function != nullptr && function->get_name() == "main" && file_id < 0) {
      fprintf(stderr,
              "REX_MANGLING_INVARIANT[main-translation-unit]: declaration=%p "
              "global scope has no real source file ID (file ID %d)\n",
              static_cast<const void *>(decl), file_id);
      ROSE_ABORT();
    }
    switch (file_id) {
    case Sg_File_Info::COPY_FILE_ID:
      return "_COPY_FILE_ID_";
    case Sg_File_Info::NULL_FILE_ID:
      return "_NULL_FILE_ID_";
    case Sg_File_Info::TRANSFORMATION_FILE_ID:
      return "_TRANSFORMATION_FILE_ID_";
    case Sg_File_Info::COMPILER_GENERATED_FILE_ID:
      return "_COMPILER_GENERATED_FILE_ID_";
    case Sg_File_Info::COMPILER_GENERATED_MARKED_FOR_OUTPUT_FILE_ID:
      return "_COMPILER_GENERATED_MARKED_FOR_OUTPUT_FILE_ID_";
    case Sg_File_Info::BAD_FILE_ID:
      return "_BAD_FILE_ID_";
    default: {
      ROSE_ASSERT(file_id >= 0);
      return "_file_id_" + StringUtility::numberToString(file_id) + "_";
    }
    }
  } else {
    return "";
  }
}
