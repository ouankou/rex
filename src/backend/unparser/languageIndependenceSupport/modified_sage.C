/* modified_sage.C
 *
 * This C file includes functions that test for operator overloaded functions
 * and helper unparse functions such as unparse_helper and printSpecifier.
 */

// tps (01/14/2010) : Switching from rose.h to sage3.
#include "sage3basic.h"

#include "modified_sage.h"

#include "unparser.h"

// DQ (10/14/2010):  This should only be included by source files that require
// it. This fixed a reported bug which caused conflicts with configure-time
// macros (e.g. PACKAGE_BUGREPORT). Interestingly it must be at the top of the
// list of include files.
#include "rose_config.h"
// DQ (12/31/2005): This is OK if not declared in a header file
using namespace std;
using namespace Rose;

#define OUTPUT_DEBUGGING_FUNCTION_BOUNDARIES 0

namespace {
bool declarations_have_exact_chain_edge(SgDeclarationStatement *lhs,
                                        SgDeclarationStatement *rhs) {
  if (lhs == nullptr || rhs == nullptr) {
    return false;
  }
  return lhs == rhs || lhs->get_firstNondefiningDeclaration() == rhs ||
         lhs->get_definingDeclaration() == rhs ||
         rhs->get_firstNondefiningDeclaration() == lhs ||
         rhs->get_definingDeclaration() == lhs;
}

bool member_specialization_requires_own_header(
    SgTemplateInstantiationMemberFunctionDecl *declaration) {
  ASSERT_not_null(declaration);

  SgClassDeclaration *associatedClass =
      isSgClassDeclaration(declaration->get_associatedClassDeclaration());
  if (associatedClass == nullptr) {
    fprintf(stderr,
            "REX_UNPARSE_INVARIANT[member-specialization-owner]: "
            "declaration=%p name=%s has no exact associated class\n",
            static_cast<void *>(declaration), declaration->get_name().str());
    ROSE_ABORT();
  }

  SgClassDefinition *semanticClassScope =
      isSgClassDefinition(declaration->get_scope());
  SgClassDeclaration *semanticClass =
      semanticClassScope != nullptr ? semanticClassScope->get_declaration()
                                    : nullptr;
  if (!declarations_have_exact_chain_edge(semanticClass, associatedClass)) {
    fprintf(stderr,
            "REX_UNPARSE_INVARIANT[member-specialization-owner]: "
            "declaration=%p name=%s scope=%p does not name associated "
            "class=%p\n",
            static_cast<void *>(declaration), declaration->get_name().str(),
            static_cast<void *>(declaration->get_scope()),
            static_cast<void *>(associatedClass));
    ROSE_ABORT();
  }

  if (SgClassDefinition *lexicalClass =
          isSgClassDefinition(declaration->get_parent())) {
    if (lexicalClass != semanticClassScope ||
        !declarations_have_exact_chain_edge(lexicalClass->get_declaration(),
                                            associatedClass)) {
      SgTemplateInstantiationMemberFunctionDecl *canonical =
          isSgTemplateInstantiationMemberFunctionDecl(
              declaration->get_firstNondefiningDeclaration());
      SgScopeStatement *canonicalScopeOwner =
          canonical != nullptr ? isSgScopeStatement(canonical->get_parent())
                               : nullptr;
      SgAuxiliaryDeclarationList *canonicalAuxiliaryOwner =
          canonical != nullptr
              ? isSgAuxiliaryDeclarationList(canonical->get_parent())
              : nullptr;
      const bool canonicalOwnedBySemanticClass =
          (canonicalScopeOwner == semanticClassScope &&
           std::count(canonicalScopeOwner->getDeclarationList().begin(),
                      canonicalScopeOwner->getDeclarationList().end(),
                      canonical) == 1) ||
          (canonicalAuxiliaryOwner != nullptr &&
           canonicalAuxiliaryOwner->get_parent() == semanticClassScope &&
           semanticClassScope->get_auxiliary_declarations() ==
               canonicalAuxiliaryOwner &&
           std::count(canonicalAuxiliaryOwner->get_declarations().begin(),
                      canonicalAuxiliaryOwner->get_declarations().end(),
                      canonical) == 1);
      const bool exactQualifiedFriendFamily =
          declaration->get_declarationModifier().isFriend() &&
          lexicalClass != semanticClassScope && canonical != nullptr &&
          canonical != declaration &&
          canonical->variantT() == declaration->variantT() &&
          canonical->get_firstNondefiningDeclaration() == canonical &&
          canonical->get_scope() == semanticClassScope &&
          canonical->get_class_scope() == semanticClassScope &&
          declaration->get_firstNondefiningDeclaration() == canonical &&
          declaration->get_definingDeclaration() ==
              canonical->get_definingDeclaration() &&
          declaration->get_class_scope() == semanticClassScope &&
          canonicalOwnedBySemanticClass;
      if (exactQualifiedFriendFamily) {
        // A qualified friend is lexically written in the befriending class but
        // redeclares a member owned by another class.  It is not an in-class
        // specialization definition and therefore never contributes its own
        // `template<>` header.
        return false;
      }
      fprintf(stderr,
              "REX_UNPARSE_INVARIANT[member-specialization-owner]: "
              "in-class declaration=%p name=%s has inconsistent lexical "
              "and semantic class owners\n",
              static_cast<void *>(declaration), declaration->get_name().str());
      ROSE_ABORT();
    }
  }

  for (SgTemplateParameterList *header :
       declaration->get_sourceSpelledTemplateHeaders()) {
    if (header == nullptr || header->get_parent() != declaration) {
      fprintf(stderr,
              "REX_UNPARSE_INVARIANT[member-specialization-source-form]: "
              "declaration=%p name=%s has a null or foreign outer template "
              "header\n",
              static_cast<void *>(declaration), declaration->get_name().str());
      ROSE_ABORT();
    }
    for (SgTemplateParameter *parameter : header->get_args()) {
      if (parameter == nullptr || parameter->get_parent() != header) {
        fprintf(stderr,
                "REX_UNPARSE_INVARIANT[member-specialization-source-form]: "
                "declaration=%p name=%s has a malformed outer template "
                "parameter owner\n",
                static_cast<void *>(declaration),
                declaration->get_name().str());
        ROSE_ABORT();
      }
    }
  }

  const bool isExplicitSpecialization =
      declaration->get_specialization() ==
      SgDeclarationStatement::e_specialization;
  if (!isExplicitSpecialization) {
    return false;
  }

  SgTemplateInstantiationDecl *associatedInstantiation =
      isSgTemplateInstantiationDecl(associatedClass);
  const bool specializesFunctionTemplate =
      declaration->get_templateDeclaration() != nullptr;
  const size_t emptyHeaderCount =
      std::count_if(declaration->get_sourceSpelledTemplateHeaders().begin(),
                    declaration->get_sourceSpelledTemplateHeaders().end(),
                    [](SgTemplateParameterList *header) {
                      return header != nullptr && header->get_args().empty();
                    });
  if (!specializesFunctionTemplate) {
    if (emptyHeaderCount == 0) {
      fprintf(stderr,
              "REX_UNPARSE_INVARIANT[member-specialization-source-form]: "
              "non-template member specialization=%p name=%s requires one "
              "or more exact source-owned empty headers, found headers=%zu "
              "empty=%zu\n",
              static_cast<void *>(declaration), declaration->get_name().str(),
              declaration->get_sourceSpelledTemplateHeaders().size(),
              emptyHeaderCount);
      ROSE_ABORT();
    }

    // The exact source-owned empty header is the sole immediate
    // explicit-specialization surface for an ordinary member.  Its typed
    // specialization role validates the semantics but must not emit a second
    // header.
    return false;
  }

  const bool needsEnclosingClassTemplateHeader =
      specializesFunctionTemplate && associatedInstantiation != nullptr &&
      associatedClass->get_specialization() !=
          SgDeclarationStatement::e_specialization;
  if (needsEnclosingClassTemplateHeader) {
    if (emptyHeaderCount == 0) {
      fprintf(stderr,
              "REX_UNPARSE_INVARIANT[member-specialization-source-form]: "
              "member-function specialization=%p name=%s requires one or "
              "more exact source-owned empty enclosing headers, found "
              "headers=%zu empty=%zu\n",
              static_cast<void *>(declaration), declaration->get_name().str(),
              declaration->get_sourceSpelledTemplateHeaders().size(),
              emptyHeaderCount);
      ROSE_ABORT();
    }
  }

  // sourceSpelledTemplateHeaders contains only preceding enclosing template
  // levels for a member-function template.  Its typed explicit-specialization
  // role owns the immediate `template<>`.
  return true;
}

std::string
exact_implicit_conversion_function_name(SgFunctionDeclaration *declaration) {
  if (declaration == nullptr ||
      !declaration->get_specialFunctionModifier().isConversion()) {
    return "";
  }

  SgName name;
  if (SgTemplateInstantiationFunctionDecl *instantiation =
          isSgTemplateInstantiationFunctionDecl(declaration)) {
    name = instantiation->get_templateName();
  } else if (SgTemplateInstantiationMemberFunctionDecl *instantiation =
                 isSgTemplateInstantiationMemberFunctionDecl(declaration)) {
    name = instantiation->get_templateName();
  } else {
    name = declaration->get_name();
  }

  const std::string function_name = name.getString();
  if (function_name.rfind("operator ", 0) != 0 ||
      function_name.size() == std::string("operator ").size()) {
    fprintf(stderr,
            "REX_UNPARSE_INVARIANT[implicit-conversion-call]: "
            "declaration=%p type=%s has conversion metadata but exact base "
            "name='%s'\n",
            static_cast<void *>(declaration), declaration->class_name().c_str(),
            function_name.c_str());
    ROSE_ABORT();
  }
  return function_name;
}

void validate_implicit_conversion_provenance(SgLocatedNode *node,
                                             const char *role) {
  if (node == nullptr) {
    fprintf(stderr,
            "REX_UNPARSE_INVARIANT[implicit-conversion-call]: missing exact "
            "%s node\n",
            role);
    ROSE_ABORT();
  }
  for (Sg_File_Info *file_info :
       {node->get_file_info(), node->get_startOfConstruct(),
        node->get_endOfConstruct()}) {
    if (file_info == nullptr || !file_info->isCompilerGenerated() ||
        !file_info->isOutputInCodeGeneration()) {
      fprintf(stderr,
              "REX_UNPARSE_INVARIANT[implicit-conversion-call]: exact %s "
              "node lacks synthesized semantic provenance\n",
              role);
      ROSE_ABORT();
    }
  }
}

} // namespace

SgExpression *GetImplicitConversionObject(SgFunctionCallExp *call) {
  if (call == nullptr ||
      call->get_source_syntax() != SgFunctionCallExp::e_implicit_conversion) {
    fprintf(stderr,
            "REX_UNPARSE_INVARIANT[implicit-conversion-call]: expected one "
            "typed implicit-conversion call\n");
    ROSE_ABORT();
  }

  SgExprListExp *arguments = call->get_args();
  if (arguments == nullptr || arguments->get_parent() != call ||
      !arguments->get_expressions().empty() ||
      call->get_uses_operator_syntax() ||
      !call->get_source_user_defined_literal_suffix().getString().empty()) {
    fprintf(stderr,
            "REX_UNPARSE_INVARIANT[implicit-conversion-call]: semantic "
            "conversion must own an empty argument list and no source call "
            "syntax\n");
    ROSE_ABORT();
  }

  SgExpression *callee = call->get_function();
  if (callee == nullptr || callee->get_parent() != call) {
    fprintf(stderr, "REX_UNPARSE_INVARIANT[implicit-conversion-call]: semantic "
                    "conversion has no exactly owned member-access callee\n");
    ROSE_ABORT();
  }

  SgBinaryOp *member_access = isSgDotExp(callee);
  if (member_access == nullptr) {
    member_access = isSgArrowExp(callee);
  }
  if (member_access == nullptr) {
    fprintf(stderr,
            "REX_UNPARSE_INVARIANT[implicit-conversion-call]: semantic "
            "conversion callee is not an exact dot or arrow member access\n");
    ROSE_ABORT();
  }

  SgExpression *object = member_access->get_lhs_operand();
  SgExpression *conversion = member_access->get_rhs_operand();
  SgMemberFunctionDeclaration *conversion_declaration = nullptr;
  bool has_exact_conversion_symbol = false;
  if (SgMemberFunctionRefExp *reference =
          isSgMemberFunctionRefExp(conversion)) {
    conversion_declaration =
        reference->getAssociatedMemberFunctionDeclaration();
    has_exact_conversion_symbol =
        reference->get_symbol() != nullptr &&
        reference->get_symbol()->get_declaration() == conversion_declaration;
  } else if (SgTemplateMemberFunctionRefExp *reference =
                 isSgTemplateMemberFunctionRefExp(conversion)) {
    conversion_declaration =
        reference->getAssociatedMemberFunctionDeclaration();
    SgTemplateMemberFunctionDeclaration *source_template =
        reference->get_symbol() != nullptr
            ? isSgTemplateMemberFunctionDeclaration(
                  reference->get_symbol()->get_declaration())
            : nullptr;
    SgTemplateInstantiationMemberFunctionDecl *semantic_instantiation =
        isSgTemplateInstantiationMemberFunctionDecl(
            reference->get_semantic_member_function_declaration());
    has_exact_conversion_symbol =
        source_template != nullptr && conversion_declaration != nullptr &&
        ((semantic_instantiation != nullptr &&
          conversion_declaration == semantic_instantiation &&
          declarations_have_exact_chain_edge(
              semantic_instantiation->get_templateDeclaration(),
              source_template)) ||
         (semantic_instantiation == nullptr &&
          conversion_declaration == source_template));
  }
  if (object == nullptr || object->get_parent() != member_access ||
      conversion == nullptr || conversion->get_parent() != member_access ||
      conversion_declaration == nullptr || !has_exact_conversion_symbol ||
      exact_implicit_conversion_function_name(conversion_declaration).empty()) {
    fprintf(
        stderr,
        "REX_UNPARSE_INVARIANT[implicit-conversion-call]: member access "
        "does not own one object and one typed conversion-function "
        "reference; object=%p/%s object-parent=%p conversion=%p/%s "
        "conversion-parent=%p declaration=%p/%s exact-symbol=%d\n",
        static_cast<void *>(object),
        object != nullptr ? object->class_name().c_str() : "<null>",
        static_cast<void *>(object != nullptr ? object->get_parent() : nullptr),
        static_cast<void *>(conversion),
        conversion != nullptr ? conversion->class_name().c_str() : "<null>",
        static_cast<void *>(conversion != nullptr ? conversion->get_parent()
                                                  : nullptr),
        static_cast<void *>(conversion_declaration),
        conversion_declaration != nullptr
            ? conversion_declaration->class_name().c_str()
            : "<null>",
        has_exact_conversion_symbol ? 1 : 0);
    ROSE_ABORT();
  }

  validate_implicit_conversion_provenance(call, "call wrapper");
  validate_implicit_conversion_provenance(arguments, "argument-list wrapper");
  validate_implicit_conversion_provenance(member_access,
                                          "member-access wrapper");
  validate_implicit_conversion_provenance(
      conversion, "conversion-function-reference wrapper");
  return object;
}

bool getOperatorFunctionName(SgExpression *expr, string &func_name) {
  auto exact_function_name = [](SgFunctionDeclaration *declaration,
                                const SgName &symbol_name) -> std::string {
    if (SgTemplateInstantiationMemberFunctionDecl *instantiation =
            isSgTemplateInstantiationMemberFunctionDecl(declaration)) {
      const std::string template_name =
          instantiation->get_templateName().getString();
      if (!template_name.empty()) {
        return template_name;
      }
    }
    if (SgTemplateInstantiationFunctionDecl *instantiation =
            isSgTemplateInstantiationFunctionDecl(declaration)) {
      const std::string template_name =
          instantiation->get_templateName().getString();
      if (!template_name.empty()) {
        return template_name;
      }
    }
    return symbol_name.getString();
  };

  if (SgFunctionRefExp *func_ref = isSgFunctionRefExp(expr)) {
    ASSERT_not_null(func_ref->get_symbol());
    func_name = exact_function_name(func_ref->get_symbol()->get_declaration(),
                                    func_ref->get_symbol()->get_name());
    return true;
  }
  if (SgTemplateFunctionRefExp *func_ref = isSgTemplateFunctionRefExp(expr)) {
    ASSERT_not_null(func_ref->get_symbol());
    func_name = exact_function_name(func_ref->get_symbol()->get_declaration(),
                                    func_ref->get_symbol()->get_name());
    return true;
  }
  if (SgMemberFunctionRefExp *mfunc_ref = isSgMemberFunctionRefExp(expr)) {
    ASSERT_not_null(mfunc_ref->get_symbol());
    func_name = exact_function_name(mfunc_ref->get_symbol()->get_declaration(),
                                    mfunc_ref->get_symbol()->get_name());
    return true;
  }
  if (SgTemplateMemberFunctionRefExp *mfunc_ref =
          isSgTemplateMemberFunctionRefExp(expr)) {
    ASSERT_not_null(mfunc_ref->get_symbol());
    func_name = exact_function_name(mfunc_ref->get_symbol()->get_declaration(),
                                    mfunc_ref->get_symbol()->get_name());
    return true;
  }
  if (SgNonrealRefExp *nr_ref = isSgNonrealRefExp(expr)) {
    ASSERT_not_null(nr_ref->get_symbol());
    func_name = nr_ref->get_symbol()->get_name().str();
    return true;
  }
  return false;
}

Unparse_MOD_SAGE::Unparse_MOD_SAGE(Unparser *unp) : unp(unp) {}

void Unparse_MOD_SAGE::resetActiveExternLinkageBraceStack() {
  activeExternLinkageBraceStack.clear();
}

void Unparse_MOD_SAGE::pushActiveExternLinkageBraceLanguage(
    const std::string &language) {
  ROSE_ASSERT(!language.empty());
  activeExternLinkageBraceStack.push_back(language);
}

void Unparse_MOD_SAGE::popActiveExternLinkageBraceLanguage() {
  ROSE_ASSERT(!activeExternLinkageBraceStack.empty());
  activeExternLinkageBraceStack.pop_back();
}

std::string Unparse_MOD_SAGE::getActiveExternLinkageBraceLanguage() const {
  return activeExternLinkageBraceStack.empty()
             ? std::string()
             : activeExternLinkageBraceStack.back();
}

//-----------------------------------------------------------------------------------
//  void Unparse_MOD_SAGE::isOperator
//
//  General function to test if this expression is an unary or binary operator
//  overloading function
//-----------------------------------------------------------------------------------
bool Unparse_MOD_SAGE::isOperator(SgExpression *expr) {
  ASSERT_not_null(expr);

  if (isBinaryOperator(expr) || isUnaryOperator(expr))
    return true;
  return false;
}

// DQ (8/13/2007): Added by Thomas to refactor unparser.
void Unparse_MOD_SAGE::curprint(std::string str) { unp->cur << str; }

// DQ (8/13/2007): Added by Thomas to refactor unparser.
void Unparse_MOD_SAGE::curprint_newline() { unp->cur.insert_newline(); }

//-----------------------------------------------------------------------------------
//  void Unparse_MOD_SAGE::isBinaryEqualsOperator
//
//  Auxiliary function to test if this expression is a binary operator=
//  overloading function
//-----------------------------------------------------------------------------------
bool Unparse_MOD_SAGE::isBinaryEqualsOperator(SgExpression *expr) {
  string func_name;
  if (!getOperatorFunctionName(expr, func_name))
    return false;

  if (func_name == "operator=")
    return true;
  return false;
}

//-----------------------------------------------------------------------------------
//  void Unparse_MOD_SAGE::isBinaryEqualityOperator
//
//  Auxiliary function to test if this expression is a binary operator==
//  overloading function
//-----------------------------------------------------------------------------------
bool Unparse_MOD_SAGE::isBinaryEqualityOperator(SgExpression *expr) {
  string func_name;
  if (!getOperatorFunctionName(expr, func_name))
    return false;

  if (func_name == "operator==")
    return true;

  return false;
}

//-----------------------------------------------------------------------------------
//  void Unparse_MOD_SAGE::isBinaryInequalityOperator
//
//  Auxiliary function to test if this expression is a binary operator==
//  overloading function
//-----------------------------------------------------------------------------------
bool Unparse_MOD_SAGE::isBinaryInequalityOperator(SgExpression *expr) {
  string func_name;
  if (!getOperatorFunctionName(expr, func_name))
    return false;

  if (func_name == "operator<=" || func_name == "operator>=" ||
      func_name == "operator<" || func_name == "operator>" ||
      func_name == "operator!=" || func_name == "operator<=>") {
    return true;
  }

  return false;
}

//-----------------------------------------------------------------------------------
//  void Unparse_MOD_SAGE::isBinaryArithmeticOperator
//
//  Auxiliary function to test if this expression is a binary operator==
//  overloading function
//-----------------------------------------------------------------------------------
bool Unparse_MOD_SAGE::isBinaryArithmeticOperator(SgExpression *expr) {
  string func_name;
  if (!getOperatorFunctionName(expr, func_name))
    return false;

  if (func_name == "operator+" || func_name == "operator-" ||
      func_name == "operator*" || func_name == "operator/" ||
      func_name == "operator+=" || func_name == "operator-=" ||
      func_name == "operator*=" || func_name == "operator/=") {
    return true;
  }

  return false;
}

//-----------------------------------------------------------------------------------
//  void Unparse_MOD_SAGE::isBinaryParenOperator
//
//  Auxiliary function to test if this expression is a binary operator()
//  overloading function
//-----------------------------------------------------------------------------------
bool Unparse_MOD_SAGE::isBinaryParenOperator(SgExpression *expr) {
  string func_name;
  if (!getOperatorFunctionName(expr, func_name))
    return false;

  if (func_name == "operator()")
    return true;

  return false;
}

//-----------------------------------------------------------------------------------
//  void Unparse_MOD_SAGE::isBinaryBracketOperator
//
//  Auxiliary function to test if this expression is a binary operator[]
//  overloading function
//-----------------------------------------------------------------------------------
bool Unparse_MOD_SAGE::isBinaryBracketOperator(SgExpression *expr) {
  string func_name;
  if (!getOperatorFunctionName(expr, func_name))
    return false;

  if (func_name == "operator[]")
    return true;

  return false;
}

//-----------------------------------------------------------------------------------
//  void Unparse_MOD_SAGE::isBinaryOperator
//
//  Function to test if this expression is a binary operator overloading
//  function
//-----------------------------------------------------------------------------------
bool Unparse_MOD_SAGE::isBinaryOperator(SgExpression *expr) {
  ASSERT_not_null(expr);

  bool isBinaryOperatorResult = false;

  string func_name;
  if (!getOperatorFunctionName(expr, func_name))
    return false;

  if (func_name == "operator+" || func_name == "operator-" ||
      func_name == "operator*" || func_name == "operator/" ||
      func_name == "operator%" || func_name == "operator^" ||
      func_name == "operator&" || func_name == "operator|" ||
      func_name == "operator=" || func_name == "operator<" ||
      func_name == "operator>" || func_name == "operator+=" ||
      func_name == "operator-=" || func_name == "operator*=" ||
      func_name == "operator/=" || func_name == "operator%=" ||
      func_name == "operator^=" || func_name == "operator&=" ||
      func_name == "operator|=" || func_name == "operator<<" ||
      func_name == "operator>>" || func_name == "operator>>=" ||
      func_name == "operator<<=" || func_name == "operator==" ||
      func_name == "operator!=" || func_name == "operator<=" ||
      func_name == "operator>=" || func_name == "operator<=>" ||
      func_name == "operator&&" || func_name == "operator||" ||
      func_name == "operator," || func_name == "operator->*" ||
      func_name == "operator->" || func_name == "operator()" ||
      func_name == "operator[]") {
    // DQ (5/6/2007): Make sure this could not be a unary operator (using new
    // fix for unary operators). if (isUnaryOperatorPlus(expr) ||
    // isUnaryOperatorMinus(expr))
    if (isUnaryOperator(expr) == true)
      isBinaryOperatorResult = false;
    else
      isBinaryOperatorResult = true;
  }

  return isBinaryOperatorResult;
}

//-----------------------------------------------------------------------------------
//  void Unparse_MOD_SAGE::isUnaryIncrementOperator
//
//  Auxiliary function to test if this expression is an unary prefix/postfix
//  operator++ overloading function
//-----------------------------------------------------------------------------------
bool Unparse_MOD_SAGE::isUnaryIncrementOperator(SgExpression *expr) {
  // DQ (5/6/2007): This might be a non-member function and if so we don't
  // handle this case correctly! If it is a non-member function this it will
  // have a single argument ROSE_ASSERT(isSgFunctionRefExp(expr) == NULL);
  ASSERT_not_null(expr);

  SgMemberFunctionRefExp *mfunc_ref = isSgMemberFunctionRefExp(expr);
  if (mfunc_ref != NULL) {
    SgMemberFunctionSymbol *mfunc_sym = mfunc_ref->get_symbol();
    if (mfunc_sym != NULL) {
      SgMemberFunctionDeclaration *mfunc_decl = mfunc_sym->get_declaration();
      if (mfunc_decl != NULL) {
        SgName func_name = mfunc_decl->get_name();
        if (func_name.getString() == "operator++") {
          SgInitializedNamePtrList argList = mfunc_decl->get_args();
          if (argList.size() == 1)
            return true;
        }
      }
    }
  }

  // DQ (2/1/2018): Added to catch case of non-member function unary operator
  else {
    SgFunctionRefExp *func_ref = isSgFunctionRefExp(expr);
    if (func_ref != NULL) {
      SgFunctionSymbol *func_sym = func_ref->get_symbol();
      if (func_sym != NULL) {
        SgFunctionDeclaration *func_decl = func_sym->get_declaration();
        if (func_decl != NULL) {
          SgName func_name = func_decl->get_name();
          if (func_name.getString() == "operator++") {
            SgInitializedNamePtrList argList = func_decl->get_args();
            if (argList.size() == 0)
              return true;
          }
        }
      }
    }
  }

  return false;
}

//-----------------------------------------------------------------------------------
//  void Unparse_MOD_SAGE::isUnaryDecrementOperator
//
//  Auxiliary function to test if this expression is an unary prefix/postfix
//  operator-- overloading function
//-----------------------------------------------------------------------------------
bool Unparse_MOD_SAGE::isUnaryDecrementOperator(SgExpression *expr) {
  // DQ (5/6/2007): This might be a non-member function and if so we don't
  // handle this case correctly! If it is a non-member function this it will
  // have a single argument ROSE_ASSERT(isSgFunctionRefExp(expr) == NULL);
  ASSERT_not_null(expr);

  SgMemberFunctionRefExp *mfunc_ref = isSgMemberFunctionRefExp(expr);
  if (mfunc_ref != NULL) {
    SgMemberFunctionSymbol *mfunc_sym = mfunc_ref->get_symbol();
    if (mfunc_sym != NULL) {
      SgMemberFunctionDeclaration *mfunc_decl = mfunc_sym->get_declaration();
      if (mfunc_decl != NULL) {
        SgName func_name = mfunc_decl->get_name();
        if (func_name.getString() == "operator--") {
          SgInitializedNamePtrList argList = mfunc_decl->get_args();
          if (argList.size() == 1)
            return true;
        }
      }
    }
  }

  // DQ (2/1/2018): Added to catch case of non-member function unary operator
  else {
    SgFunctionRefExp *func_ref = isSgFunctionRefExp(expr);
    if (func_ref != NULL) {
      SgFunctionSymbol *func_sym = func_ref->get_symbol();
      if (func_sym != NULL) {
        SgFunctionDeclaration *func_decl = func_sym->get_declaration();
        if (func_decl != NULL) {
          SgName func_name = func_decl->get_name();
          if (func_name.getString() == "operator--") {
            SgInitializedNamePtrList argList = func_decl->get_args();
            if (argList.size() == 0)
              return true;
          }
        }
      }
    }
  }

  return false;
}

bool Unparse_MOD_SAGE::isUnaryLiteralOperator(SgExpression *expr) {
  ASSERT_not_null(expr);

  SgMemberFunctionRefExp *mfunc_ref = isSgMemberFunctionRefExp(expr);
  if (mfunc_ref != NULL) {
    SgMemberFunctionSymbol *mfunc_sym = mfunc_ref->get_symbol();
    if (mfunc_sym != NULL) {
      SgMemberFunctionDeclaration *mfunc_decl = mfunc_sym->get_declaration();
      if (mfunc_decl != NULL) {
        SgName func_name = mfunc_decl->get_name();
        std::string s = func_name.getString();
        if (s.find("operator \"\" ", 0) != std::string::npos) {
          return true;
        }
      }
    }
  }

  // DQ (2/12/2019): Added to catch case of non-member function unary operator
  else {
    SgFunctionRefExp *func_ref = isSgFunctionRefExp(expr);
    if (func_ref != NULL) {
      SgFunctionSymbol *func_sym = func_ref->get_symbol();
      if (func_sym != NULL) {
        SgFunctionDeclaration *func_decl = func_sym->get_declaration();
        if (func_decl != NULL) {
          SgName func_name = func_decl->get_name();
          std::string s = func_name.getString();
          if (s.find("operator \"\" ", 0) != std::string::npos) {
            return true;
          }
        }
      }
    }
  }

  return false;
}

//-----------------------------------------------------------------------------------
//  void Unparse_MOD_SAGE::isUnaryOperator
//
//  Function to test if this expression is an unary operator overloading
//  function
//-----------------------------------------------------------------------------------
bool Unparse_MOD_SAGE::isUnaryOperator(SgExpression *expr) {
  SgFunctionRefExp *func_ref = isSgFunctionRefExp(expr);
  SgMemberFunctionRefExp *mfunc_ref = isSgMemberFunctionRefExp(expr);

  // Ignore things that are not functions
  // if (!func_ref && !mfunc_ref)
  if (func_ref == NULL && mfunc_ref == NULL) {
    // DQ (8/27/2007): This really is called for many non-functions reference
    // expressions.
    return false;
  }

  string func_name;
  if (func_ref != NULL) {
    func_name = func_ref->get_symbol()->get_name().str();
  } else {
    func_name = mfunc_ref->get_symbol()->get_name().str();
  }

  // DQ (2/14/2005): Need special test for operator*(), and other unary
  // operators, similar to operator+() and operator-(). Error need to check
  // number of parameters in arg list of operator* to verify it is a unary
  // operators else it could be a binary multiplication operator Maybe also for
  // the binary operator&() (what about operator~()?) Added support for
  // isUnaryDereferenceOperator(), isUnaryAddressOperator(),
  // isUnaryOrOperator(), isUnaryComplementOperator().
  // DQ (2/1/2018): The argument to these functions in the predicate should be
  // "exp" not "mfunc_ref"
  if (isUnaryOperatorPlus(expr) || isUnaryOperatorMinus(expr) ||
      // DQ (2/1/2018): This operator now has a function to support the
      // evaluation of it being unary. func_name == "operator!" ||
      isUnaryNotOperator(expr) ||
      // func_name == "operator*" ||
      isUnaryDereferenceOperator(expr) ||
      // DQ (11/24/2004): Added support for address operator "operator&"
      // func_name == "operator&" ||
      isUnaryAddressOperator(expr) || func_name == "operator--" ||
      func_name == "operator++" ||
      // DQ (2/12/2019): Adding support for C++11 literal operators.
      isUnaryLiteralOperator(expr) ||
      // DQ (2/1/2018): I don't think this operator can exist.
      // isUnaryOrOperator(mfunc_ref) ||
      // func_name == "operator~")
      isUnaryComplementOperator(expr)) {
    return true;
  }

  return false;
}

//-----------------------------------------------------------------------------------
//  void Unparse_MOD_SAGE::isUnaryPostfixOperator
//
//  Auxiliary function to test if this expression is an unary postfix operator
//  overloading function
//-----------------------------------------------------------------------------------
bool Unparse_MOD_SAGE::isUnaryPostfixOperator(SgExpression *expr) {
  // DQ (5/6/2007): This might be a non-member function and if so we don't
  // handle this case correctly! ROSE_ASSERT(isSgFunctionRefExp(expr) == NULL);
  ASSERT_not_null(expr);

  SgMemberFunctionRefExp *mfunc_ref = isSgMemberFunctionRefExp(expr);
  if (mfunc_ref != NULL) {
    SgMemberFunctionSymbol *mfunc_sym = mfunc_ref->get_symbol();
    if (mfunc_sym != NULL) {
      SgMemberFunctionDeclaration *mfunc_decl = mfunc_sym->get_declaration();
      if (mfunc_decl != NULL) {
        SgName func_name = mfunc_decl->get_name();
        if (func_name.getString() == "operator++" ||
            func_name.getString() == "operator--") {
          SgInitializedNamePtrList argList = mfunc_decl->get_args();
          // postfix operators have one argument (0), prefix operators have none
          // ()
          if (argList.size() == 1) {
            return true;
          } else {
            // DQ (2/12/2019): Check if this is a literal operator.
            if (mfunc_decl->get_specialFunctionModifier().isUldOperator() ==
                true) {
              return true;
            }
          }
        }
      }
    }
  }

  // DQ (2/1/2018): Added to catch case of non-member function unary operator
  else {
    SgFunctionRefExp *func_ref = isSgFunctionRefExp(expr);
    if (func_ref != NULL) {
      SgFunctionSymbol *func_sym = func_ref->get_symbol();
      if (func_sym != NULL) {
        SgFunctionDeclaration *func_decl = func_sym->get_declaration();
        if (func_decl != NULL) {
          SgName func_name = func_decl->get_name();
          // if (func_name.getString() == "operator--")
          if (func_name.getString() == "operator++" ||
              func_name.getString() == "operator--") {
            SgInitializedNamePtrList argList = func_decl->get_args();
            if (argList.size() == 2) {
              return true;
            }
          } else {
            // DQ (2/12/2019): Check if this is a literal operator.
            if (func_decl->get_specialFunctionModifier().isUldOperator() ==
                true) {
              return true;
            }
          }
        }
      }
    }
  }

  return false;
}

// QY 6/23/04: added code for precedence of expressions
//-----------------------------------------------------------------------------------
//   void GetOperatorVariant
//
//   Function that returns the expression variant of overloaded operators
//-----------------------------------------------------------------------------------

int GetOperatorVariant(SgExpression *expr) {

  SgFunctionCallExp *func_call = isSgFunctionCallExp(expr);
  if (func_call == NULL) {
    return expr->variantT();
  }

  if (func_call->get_source_syntax() ==
      SgFunctionCallExp::e_implicit_conversion) {
    return V_SgCastExp;
  }

  switch (func_call->get_source_operator_surface()) {
  case SgFunctionCallExp::e_no_operator_surface:
  case SgFunctionCallExp::e_call_operator_surface:
  case SgFunctionCallExp::e_arrow_operator_surface:
  case SgFunctionCallExp::e_user_defined_literal_surface:
    return V_SgFunctionCallExp;
  case SgFunctionCallExp::e_prefix_plus:
    return V_SgUnaryAddOp;
  case SgFunctionCallExp::e_prefix_minus:
    return V_SgMinusOp;
  case SgFunctionCallExp::e_dereference:
    return V_SgPointerDerefExp;
  case SgFunctionCallExp::e_address_of:
    return V_SgAddressOfOp;
  case SgFunctionCallExp::e_logical_not:
    return V_SgNotOp;
  case SgFunctionCallExp::e_bitwise_not:
    return V_SgBitComplementOp;
  case SgFunctionCallExp::e_prefix_increment:
  case SgFunctionCallExp::e_postfix_increment:
    return V_SgPlusPlusOp;
  case SgFunctionCallExp::e_prefix_decrement:
  case SgFunctionCallExp::e_postfix_decrement:
    return V_SgMinusMinusOp;
  case SgFunctionCallExp::e_co_await:
    return V_SgAwaitExpression;
  case SgFunctionCallExp::e_binary_plus:
    return V_SgAddOp;
  case SgFunctionCallExp::e_binary_minus:
    return V_SgSubtractOp;
  case SgFunctionCallExp::e_binary_multiply:
    return V_SgMultiplyOp;
  case SgFunctionCallExp::e_binary_divide:
    return V_SgDivideOp;
  case SgFunctionCallExp::e_binary_remainder:
    return V_SgModOp;
  case SgFunctionCallExp::e_binary_xor:
    return V_SgBitXorOp;
  case SgFunctionCallExp::e_binary_and:
    return V_SgBitAndOp;
  case SgFunctionCallExp::e_binary_or:
    return V_SgBitOrOp;
  case SgFunctionCallExp::e_binary_assign:
    return V_SgAssignOp;
  case SgFunctionCallExp::e_binary_less:
    return V_SgLessThanOp;
  case SgFunctionCallExp::e_binary_greater:
    return V_SgGreaterThanOp;
  case SgFunctionCallExp::e_binary_plus_assign:
    return V_SgPlusAssignOp;
  case SgFunctionCallExp::e_binary_minus_assign:
    return V_SgMinusAssignOp;
  case SgFunctionCallExp::e_binary_multiply_assign:
    return V_SgMultAssignOp;
  case SgFunctionCallExp::e_binary_divide_assign:
    return V_SgDivAssignOp;
  case SgFunctionCallExp::e_binary_remainder_assign:
    return V_SgModAssignOp;
  case SgFunctionCallExp::e_binary_xor_assign:
    return V_SgXorAssignOp;
  case SgFunctionCallExp::e_binary_and_assign:
    return V_SgAndAssignOp;
  case SgFunctionCallExp::e_binary_or_assign:
    return V_SgIorAssignOp;
  case SgFunctionCallExp::e_binary_left_shift:
    return V_SgLshiftOp;
  case SgFunctionCallExp::e_binary_right_shift:
    return V_SgRshiftOp;
  case SgFunctionCallExp::e_binary_left_shift_assign:
    return V_SgLshiftAssignOp;
  case SgFunctionCallExp::e_binary_right_shift_assign:
    return V_SgRshiftAssignOp;
  case SgFunctionCallExp::e_binary_equal:
    return V_SgEqualityOp;
  case SgFunctionCallExp::e_binary_not_equal:
    return V_SgNotEqualOp;
  case SgFunctionCallExp::e_binary_less_equal:
    return V_SgLessOrEqualOp;
  case SgFunctionCallExp::e_binary_greater_equal:
    return V_SgGreaterOrEqualOp;
  case SgFunctionCallExp::e_binary_spaceship:
    return V_SgSpaceshipOp;
  case SgFunctionCallExp::e_binary_logical_and:
    return V_SgAndOp;
  case SgFunctionCallExp::e_binary_logical_or:
    return V_SgOrOp;
  case SgFunctionCallExp::e_binary_comma:
    return V_SgCommaOpExp;
  case SgFunctionCallExp::e_binary_arrow_star:
    return V_SgArrowStarOp;
  case SgFunctionCallExp::e_subscript_operator_surface:
    return V_SgPntrArrRefExp;
  }

  fprintf(stderr,
          "REX_UNPARSE_INVARIANT[operator-source-surface]: call=%p has "
          "invalid surface=%d for precedence\n",
          static_cast<void *>(func_call),
          static_cast<int>(func_call->get_source_operator_surface()));
  ROSE_ABORT();
}

SgExpression *GetFirstOperand(SgExpression *expr) {
  SgFunctionCallExp *func_call = isSgFunctionCallExp(expr);
  if (func_call != NULL) {
    if (func_call->get_source_operator_surface() !=
        SgFunctionCallExp::e_no_operator_surface) {
      if (func_call->get_source_operator_surface() ==
          SgFunctionCallExp::e_user_defined_literal_surface) {
        SgExprListExp *lexical =
            func_call->get_source_user_defined_literal_operands();
        if (lexical == nullptr || lexical->get_parent() != func_call ||
            lexical->get_expressions().empty() ||
            lexical->get_expressions().front() == nullptr ||
            lexical->get_expressions().front()->get_parent() != lexical) {
          fprintf(stderr,
                  "REX_UNPARSE_INVARIANT[user-defined-literal-call]: literal "
                  "has no exact first lexical operand\n");
          ROSE_ABORT();
        }
        return lexical->get_expressions().front();
      }
      if (func_call->get_source_operator_callee_form() ==
          SgFunctionCallExp::e_member_operator_callee) {
        if (SgDotExp *dot = isSgDotExp(func_call->get_function())) {
          return dot->get_lhs_operand();
        }
        if (SgArrowExp *arrow = isSgArrowExp(func_call->get_function())) {
          return arrow->get_lhs_operand();
        }
        fprintf(stderr, "REX_UNPARSE_INVARIANT[operator-source-callee]: member "
                        "operator has no exact first source operand\n");
        ROSE_ABORT();
      }
      if (func_call->get_source_operator_callee_form() ==
          SgFunctionCallExp::e_nonmember_operator_callee) {
        SgExprListExp *arguments = func_call->get_args();
        const SgUnsignedCharList &operand_roles =
            func_call->get_source_operator_operand_roles();
        if (arguments == nullptr ||
            arguments->get_expressions().size() != operand_roles.size()) {
          fprintf(stderr,
                  "REX_UNPARSE_INVARIANT[operator-source-operands]: "
                  "nonmember operator has inconsistent exact operand roles\n");
          ROSE_ABORT();
        }
        auto role = operand_roles.begin();
        for (SgExpression *argument : arguments->get_expressions()) {
          if (*role++ == SgFunctionCallExp::e_source_operator_operand) {
            return argument;
          }
        }
        fprintf(stderr,
                "REX_UNPARSE_INVARIANT[operator-source-operands]: "
                "nonmember operator has no exact first source operand\n");
        ROSE_ABORT();
      }
      fprintf(stderr,
              "REX_UNPARSE_INVARIANT[operator-source-callee]: operator has "
              "invalid exact callee form\n");
      ROSE_ABORT();
    }
    return func_call->get_function();
  } else if (SgPackExpansionExpr *pack_expansion = isSgPackExpansionExpr(expr))
    return pack_expansion->get_pattern_expression();
  else {
    SgUnaryOp *op1 = isSgUnaryOp(expr);
    if (op1 != 0)
      return op1->get_operand();
    else {
      SgBinaryOp *op2 = isSgBinaryOp(expr);
      if (op2 != 0)
        return op2->get_lhs_operand();
    }
  }

  return NULL;
}

//-----------------------------------------------------------------------------------
//  int GetPrecedence
//
//  returns the precedence (1-17) of the expression variants,
//  such that 17 has the highest precedence and 1 has the lowest precedence.
//-----------------------------------------------------------------------------------
int GetPrecedence(int variant) {
  ROSE_ASSERT(!"Deprecated. Use "
               "UnparseLanguageIndependentConstructs::getPrecedence instead");
  return -1;
}

//-----------------------------------------------------------------------------------
//  GetAssociativity
//
//  Function that returns the associativity of the expression variants,
//  -1: left associative; 1: right associative; 0 : not associative/unknown
//-----------------------------------------------------------------------------------
int GetAssociativity(int) {
  ROSE_ASSERT(
      !"Deprecated. Use UnparseLanguageIndependentConstructs::getAssociativity "
       "instead");
  return 0;
}

//-----------------------------------------------------------------------------------
//  void Unparse_MOD_SAGE::PrintStartParen
//
//  Auxiliary function that determines whether "(" should been printed for a
//  binary expression. This function is needed whenever the rhs of the binary
//  expression is an operator= overloaded function.
//-----------------------------------------------------------------------------------
bool Unparse_MOD_SAGE::PrintStartParen(SgExpression *, SgUnparse_Info &) {
  ROSE_ASSERT(
      !"deprecated. use "
       "UnparseLanguageIndependentConstructs::requiresParentheses instead");
  return false;
}

//-----------------------------------------------------------------------------------
//  bool Unparse_MOD_SAGE::RemovePareninExprList
//
//  Auxiliary function to determine whether parenthesis is needed around
//  this expression list. If the list only contains one element and the
//  element is a binary operator whose rhs is an operator(), then parens
//  are removed (return true). Otherwise, return false.
//-----------------------------------------------------------------------------------
bool Unparse_MOD_SAGE::RemovePareninExprList(SgExprListExp *expr_list) {
  ASSERT_not_null(expr_list);
  SgExpressionPtrList::iterator i = expr_list->get_expressions().begin();

  if (i != expr_list->get_expressions().end()) {
    SgFunctionCallExp *func_call = isSgFunctionCallExp(*i);
    i++;
    if (func_call != NULL) {
      SgDotExp *dot_exp = isSgDotExp(func_call->get_function());
      if (dot_exp != NULL) {
        SgBinaryOp *binary_op = isSgBinaryOp(dot_exp);
        if (binary_op != NULL) {
          // check if there is only one expression in the list and this
          // expression contains the member operator() overloaded function
          if (i == expr_list->get_expressions().end() &&
              isBinaryParenOperator(binary_op->get_rhs_operand()))
            return true;
        }
      }
    }
  }
  return false;
}

//-----------------------------------------------------------------------------------
//  void Unparse_MOD_SAGE::isOverloadedArrowOperator
//
//  Auxiliary function to test if this expression is a binary operator-> or
//  operator->* overloading function
//-----------------------------------------------------------------------------------
bool Unparse_MOD_SAGE::isOverloadedArrowOperator(SgExpression *expr) {
  return SageInterface::isOverloadedArrowOperator(expr);
}

bool Unparse_MOD_SAGE::isOverloadedArrowOperatorChain(SgExpression *expr) {
  // DQ (12/11/2004): This function recognizes a subtree which represents the
  // "S* T::operator->()" for some class type "T" and type "S"
  return SageInterface::isOverloadedArrowOperatorChain(expr);
}

//-----------------------------------------------------------------------------------
//  bool Unparse_MOD_SAGE::isIOStreamOperator
//
//  checks if this expression is an iostream overloaded function
//-----------------------------------------------------------------------------------
bool Unparse_MOD_SAGE::isIOStreamOperator(SgExpression *expr) {
  SgFunctionRefExp *func_ref = isSgFunctionRefExp(expr);
  SgMemberFunctionRefExp *mfunc_ref = isSgMemberFunctionRefExp(expr);
  if (!func_ref && !mfunc_ref)
    return false;

  // This results in a FMR in purify since the casting operator for
  // SgName (operator char*) causes a SgName object to be built and
  // then the char* pointer is taken from that temporary char* object
  // (and then the temporary is deleted (goes out of scope) leaving
  // the char* pointing to the string in the deleted object).
  string func_name;
  if (func_ref)
    func_name = func_ref->get_symbol()->get_name().str();
  else
    func_name = mfunc_ref->get_symbol()->get_name().str();

  // check if the function name is "operator<<" or "operator>>"
  if (func_name == "operator<<" || func_name == "operator>>")
    return true;

  return false;
}

//-----------------------------------------------------------------------------------
//  bool Unparse_MOD_SAGE::isCast_ConstCharStar
//
//  auxiliary function to determine if this cast expression is "const char*"
//-----------------------------------------------------------------------------------
bool Unparse_MOD_SAGE::isCast_ConstCharStar(SgType *type) {
  SgPointerType *pointer_type = isSgPointerType(type);
  if (pointer_type != NULL) {
    SgModifierType *modifier_type =
        isSgModifierType(pointer_type->get_base_type());
    if (modifier_type != NULL && modifier_type->get_typeModifier()
                                     .get_constVolatileModifier()
                                     .isConst()) {
      SgTypeChar *char_type = isSgTypeChar(modifier_type->get_base_type());
      if (char_type != NULL)
        return true;
    }
  }

  return false;
}

//-----------------------------------------------------------------------------------
//  bool Unparse_MOD_SAGE::noQualifiedName
//
//  auxiliary function to determine if "::" is printed out
//-----------------------------------------------------------------------------------
bool Unparse_MOD_SAGE::noQualifiedName(SgExpression *expr) {
  SgMemberFunctionRefExp *mfunc_ref = isSgMemberFunctionRefExp(expr);
  if (mfunc_ref != NULL) {
    // DQ (12/11/2004): I'm not so sure that the need_qualifier data member
    // is always set properly in Sage III from legacy frontend, or even in
    // legacy frontend!
    if (mfunc_ref->get_need_qualifier()) {
      // check if this is a iostream operator function and the value of the
      // overload opt is false
      if (!unp->opt.get_overload_opt() && isIOStreamOperator(mfunc_ref))
        ;
      else
        return false;
    }
  }
  return true;
}

//-----------------------------------------------------------------------------------
//  void Unparse_MOD_SAGE::output
//
//  prints out the line number and file information of the node
//-----------------------------------------------------------------------------------
/*
void Unparse_MOD_SAGE::output(SgLocatedNode* node) {
  Sg_File_Info* node_file = node->get_file_info();
  int curprint_line = node_file->getCurprintrentLine();
  char* curprint_file = node_file->getCurprintrentFilename();
  char* p_filename = node_file->get_filename();
  int   p_line = node_file->get_line();
  int   p_col = node_file->get_col();

  int newFile = !curprint_file || !p_filename || (::strcmp(p_filename,
curprint_file) != 0); int newLine = p_line != curprint_line;

  if (!newFile && newLine && (p_line > curprint_line) && ((p_line -
curprint_line) < 4)) { curprint_line = p_line; } else if (p_line && (newFile ||
newLine)) { curprint( "#" << p_line; curprint_line = p_line; if (newFile &&
p_filename) { curprint( " \"" << p_filename << "\""); curprint_file =
p_filename;
    }
  }

}
*/
//-----------------------------------------------------------------------------------
//  void Unparse_MOD_SAGE::directives
//
//  checks the linefile option to determine whether to print line and file info
//  prints out pragmas
//-----------------------------------------------------------------------------------
void Unparse_MOD_SAGE::directives(SgLocatedNode * /*lnode*/) {
  // checks option status before printing line and file info
  if (unp->opt.get_linefile_opt()) {
    /*
              if (lnode->get_file_info())
                 {
                   output(lnode);
                 }
    */
  }

  printf("Unparse_MOD_SAGE::directives(): commented out call to "
         "output_pragma() \n");
  // DQ (8/20/2004): Removed this function (old implementation of pragmas)
  // stringstream out;
  // lnode->output_pragma(out);
  // curprint( out.str();
}

//-----------------------------------------------------------------------------------
//  void Unparse_MOD_SAGE::printSpecifier1
//  void Unparse_MOD_SAGE::printSpecifier2
//  void Unparse_MOD_SAGE::printSpecifier
//
// printSpecifier1 is retained as the first half of the declaration-specifier
// API.  Access labels are not declaration specifiers: an exact
// SgAccessLabelStatement owns and emits each lexical `public:`, `protected:`,
// or `private:` surface.
//-----------------------------------------------------------------------------------
void Unparse_MOD_SAGE::printSpecifier1(SgDeclarationStatement *decl_stmt,
                                       SgUnparse_Info &info) {
  ASSERT_not_null(decl_stmt);
  static_cast<void>(info);
}

// DQ (8/15/2020): Adding support for state so that we can avoid nested extern
// "C" specifications. void Unparse_MOD_SAGE::outputExternLinkageSpecifier (
// SgDeclarationStatement* decl_stmt )
void Unparse_MOD_SAGE::outputExternLinkageSpecifier(
    SgDeclarationStatement *decl_stmt, SgUnparse_Info &info) {
  ASSERT_not_null(decl_stmt);

#define DEBUG_EXTERN 0

#if DEBUG_EXTERN
  printf("Inside of outputExternLinkageSpecifier() decl_stmt = %p = %s "
         "decl_stmt->isExternBrace() = %s \n",
         decl_stmt, decl_stmt->class_name().c_str(),
         decl_stmt->isExternBrace() ? "true" : "false");
  printf("   --- decl_stmt->isExternBrace()                                    "
         "        = %s \n",
         decl_stmt->isExternBrace() ? "true" : "false");
  printf("   --- "
         "decl_stmt->get_declarationModifier().get_storageModifier().isExtern()"
         " = %s \n",
         decl_stmt->get_declarationModifier().get_storageModifier().isExtern()
             ? "true"
             : "false");
  printf("   --- decl_stmt->get_linkage().empty()                              "
         "        = %s \n",
         decl_stmt->get_linkage().empty() ? "true" : "false");
  printf("   --- decl_stmt->get_linkage()                                      "
         "        = %s \n",
         decl_stmt->get_linkage().c_str());
  printf("   --- info.get_extern_C_with_braces()                               "
         "        = %s \n",
         info.get_extern_C_with_braces() ? "true" : "false");
  curprint("\n/* Inside of outputExternLinkageSpecifier() */ \n ");
#endif

  // DQ (5/10/2007): Fixed linkage to be a std::string instead of char*
  // if (decl_stmt->get_declarationModifier().get_storageModifier().isExtern()
  // && decl_stmt->get_linkage())
  if (decl_stmt->get_declarationModifier().get_storageModifier().isExtern() &&
      decl_stmt->get_linkage().empty() == false) {
    const std::string active_linkage = getActiveExternLinkageBraceLanguage();
    const bool inside_matching_linkage_braces =
        !active_linkage.empty() && active_linkage == decl_stmt->get_linkage();
    auto declaration_is_in_local_scope =
        [](SgDeclarationStatement *decl) -> bool {
      SgScopeStatement *lexical_scope = isSgScopeStatement(decl->get_parent());
      if (lexical_scope == NULL)
        return false;

      return isSgGlobal(lexical_scope) == NULL &&
             isSgNamespaceDefinitionStatement(lexical_scope) == NULL &&
             isSgClassDefinition(lexical_scope) == NULL;
    };
    const bool local_scope_declaration =
        declaration_is_in_local_scope(decl_stmt);

#if DEBUG_EXTERN
    printf("/* output extern keyword */ \n");
#endif
    if (inside_matching_linkage_braces == false) {
      // curprint( "extern \"" + decl_stmt->get_linkage() + "\" ");
      if (local_scope_declaration == true) {
#if DEBUG_EXTERN
        printf("/* local-scope extern declaration: output extern keyword "
               "only */ \n");
#endif
        if (decl_stmt->get_declarationModifier().isFriend() == false) {
          curprint("extern ");
        }
      } else if (decl_stmt->isExternBrace() == true) {
#if DEBUG_EXTERN
        printf("/* output extern brace */ \n");
#endif
        ROSE_ASSERT(inside_matching_linkage_braces == false);

        // DQ (11/15/2020): This fixes Cxx_tests/test2020_73.C.
        if (decl_stmt->get_declarationModifier().isFriend() == true) {
          // Suppress the extern keyword
#if DEBUG_EXTERN
          printf("/* decl_stmt->get_declarationModifier().isFriend() == true: "
                 "suppress the extern keyword */ \n");
#endif
          // curprint( "/* Suppress the extern keyword */ ");
        } else {
          // DQ (11/12/2020): We can't output the language linkage when the
          // extern declaration is in a function (e.g. SgBasicBlock). DQ
          // (11/12/2020): output the non-brace of extern with linkage.
          // curprint( "extern \"" + decl_stmt->get_linkage() + "\" ");
          // curprint( "/* info.get_extern_C_with_braces() == false &&
          // decl_stmt->isExternBrace() == false */ extern \"" +
          // decl_stmt->get_linkage() + "\" ");
          if (isSgBasicBlock(decl_stmt->get_parent()) != NULL) {
            // DQ (11/12/2020): See Cxx_tests/test2020_70.C for where this is
            // required.
            curprint("extern ");
          } else {
            curprint("extern \"" + decl_stmt->get_linkage() + "\" ");
          }
        }
      } else {
#if DEBUG_EXTERN
        printf("/* info.get_extern_C_with_braces() == false: output extern "
               "keyword only */ \n");
#endif
        curprint("extern \"" + decl_stmt->get_linkage() + "\" ");
      }
    } else {
#if DEBUG_EXTERN
      printf("/* active extern linkage matches declaration linkage: output "
             "extern keyword only */ \n");
#endif
      // DQ (8/17/2020): This is required for test2020_37.C but not for
      // test2020_28.C. curprint( "extern \"" + decl_stmt->get_linkage() + "\"
      // "); curprint( "extern "); curprint( "extern /* testing */ ");

      // DQ (8/18/2020): friend functions cannot use the extern storage
      // specification.
      if (decl_stmt->get_declarationModifier().isFriend() == true) {
        /* Suppress the extern keyword */
#if DEBUG_EXTERN
        printf("/* decl_stmt->get_declarationModifier().isFriend() == true: "
               "suppress the extern keyword */ \n");
#endif
        // curprint( "/* Suppress the extern keyword */ ");
      } else {
#if DEBUG_EXTERN
        printf("/* decl_stmt->get_declarationModifier().isFriend() == false: "
               "output extern keyword only */ \n");
#endif
        // curprint( "extern /* not a friend declaration */ ");
        curprint("extern ");
      }
    }
  }

#if DEBUG_EXTERN
  printf("Leaving outputExternLinkageSpecifier() decl_stmt = %p = %s "
         "decl_stmt->isExternBrace() = %s \n",
         decl_stmt, decl_stmt->class_name().c_str(),
         decl_stmt->isExternBrace() ? "true" : "false");
  printf("   --- info.get_extern_C_with_braces()                               "
         "        = %s \n",
         info.get_extern_C_with_braces() ? "true" : "false");
#endif
}

void Unparse_MOD_SAGE::outputTemplateSpecializationSpecifier(
    SgDeclarationStatement *decl_stmt, SgUnparse_Info &info) {
#define DEBUG_TEMPLATE_SPECIALIZATION 0

#if DEBUG_TEMPLATE_SPECIALIZATION
  curprint(
      "\n/* In outputTemplateSpecializationSpecifier(): TOP of function */ ");
#endif

  auto output_explicit_template_headers = [&](unsigned int count) {
    for (unsigned int i = 0; i < count; ++i) {
      curprint("template<> ");
    }
  };

  // DQ (1/3/2016): Adding support for template variable declarations.
  // TV (03/31/2022): FIXME that is broken: we need to find a predicate for
  // template variable instantiation in the absence of specialized node
  if (isSgTemplateVariableDeclaration(decl_stmt) != NULL) {
    SgTemplateVariableDeclaration *tvdecl =
        (SgTemplateVariableDeclaration *)decl_stmt;

    SgSourceFile *sourcefile = info.get_current_source_file();
    bool unparse_template_from_ast =
        sourcefile != NULL && sourcefile->get_unparse_template_ast();
    unparse_template_from_ast |= tvdecl->get_unparse_template_ast() == true;

    if (!unparse_template_from_ast) {
      const unsigned int header_count =
          tvdecl->get_explicitTemplateSpecializationHeaderCount();
      if (tvdecl->get_specialization() ==
              SgDeclarationStatement::e_specialization &&
          header_count == 0) {
        fprintf(stderr,
                "REX_UNPARSE_INVARIANT[variable-specialization-header]: "
                "template-variable specialization=%p class=%s has no exact "
                "producer-published header count\n",
                static_cast<void *>(tvdecl), tvdecl->class_name().c_str());
        ROSE_ABORT();
      }
      output_explicit_template_headers(header_count);
    }
  }

  auto is_plain_explicit_specialization =
      [](SgDeclarationStatement *decl) -> bool {
    if (decl == NULL) {
      return false;
    }

    if (isSgTemplateInstantiationDecl(decl) != NULL ||
        isSgTemplateInstantiationFunctionDecl(decl) != NULL ||
        isSgTemplateInstantiationMemberFunctionDecl(decl) != NULL ||
        isSgTemplateClassDeclaration(decl) != NULL ||
        isSgTemplateFunctionDeclaration(decl) != NULL ||
        isSgTemplateMemberFunctionDeclaration(decl) != NULL ||
        isSgTemplateVariableDeclaration(decl) != NULL) {
      return false;
    }

    if (SgClassDeclaration *class_decl = isSgClassDeclaration(decl)) {
      return class_decl->get_specialization() ==
             SgDeclarationStatement::e_specialization;
    }

    if (SgFunctionDeclaration *function_decl = isSgFunctionDeclaration(decl)) {
      return function_decl->get_specialization() ==
             SgDeclarationStatement::e_specialization;
    }

    if (SgVariableDeclaration *variable_decl = isSgVariableDeclaration(decl)) {
      return variable_decl->get_specialization() ==
             SgDeclarationStatement::e_specialization;
    }

    return false;
  };

  if ((isSgTemplateInstantiationDecl(decl_stmt) != NULL) ||
      (isSgTemplateInstantiationFunctionDecl(decl_stmt) != NULL) ||
      (isSgTemplateInstantiationMemberFunctionDecl(decl_stmt) != NULL)) {
#if DEBUG_TEMPLATE_SPECIALIZATION
    curprint("\n/* In outputTemplateSpecializationSpecifier(): This is a "
             "template instantiation */ ");
#endif
    if (isSgTemplateInstantiationDirectiveStatement(decl_stmt->get_parent()) !=
        NULL) {
      // Template instantiation directives use "template" instead of
      // "template<>"
#if DEBUG_TEMPLATE_SPECIALIZATION
      curprint("\n/* In outputTemplateSpecializationSpecifier(): This is a "
               "SgTemplateInstantiationDirectiveStatement */ ");
#endif
      curprint("template ");
    } else {
      // Normal case for output of template instantiations (which ROSE puts out
      // as specializations) curprint( "template<> ");
#if DEBUG_TEMPLATE_SPECIALIZATION
      curprint("\n/* In outputTemplateSpecializationSpecifier(): Normal case "
               "for output of template instantiations: " +
               decl_stmt->class_name() + " */ ");
#endif
      // DQ (5/2/2012): If this is a function template instantiation in a class
      // template instantiation then we don't want the "template<>" (error in
      // g++, at least).  See test2012_59.C.
      SgTemplateInstantiationDefn *templateClassInstatiationDefn =
          isSgTemplateInstantiationDefn(decl_stmt->get_parent());
      if (templateClassInstatiationDefn != NULL) {
        // DQ (4/6/2014): This happens when a member function template in
        // embedded in a class template and thus there is not an associated
        // template for the member function separate from the class declaration.
        // It is not rare for many system template libraries (e.g. iostream).
#if DEBUG_TEMPLATE_SPECIALIZATION
        printf("This is a declaration defined in a templated class (suppress "
               "the output of template specialization syntax) \n");
#endif
      } else {
        // DQ (7/6/2015): template member function instantiations defined
        // outside of the template class shoudl not be output with the
        // "template<>" syntax. curprint("template<> ");
        if (SgTemplateInstantiationMemberFunctionDecl *member =
                isSgTemplateInstantiationMemberFunctionDecl(decl_stmt)) {
          if (member_specialization_requires_own_header(member)) {
            curprint("template<> ");
          }
        } else {
#if DEBUG_TEMPLATE_SPECIALIZATION
          curprint("/* This still might require the output of the template<> "
                   "syntax */ ");
#endif
          // DQ (11/27/2015): If this is a friend function then supress the
          // "template<>" syntax (see test2015_123.C). But we have to check the
          // non-defining declaration for the friend function marking.
          // curprint("template<> ");
          SgTemplateInstantiationFunctionDecl
              *nondefiningTemplateInstantiationFunctionDecl =
                  isSgTemplateInstantiationFunctionDecl(
                      decl_stmt->get_firstNondefiningDeclaration());
          if (nondefiningTemplateInstantiationFunctionDecl != NULL) {
            // DQ (1/13/2020): The firstNondefiningDeclaration might not be the
            // friend declaration (it might be another nondefining declaration
            // (see Cxx11_tests/test2020_47.C)). if
            // (nondefiningTemplateInstantiationFunctionDecl->get_declarationModifier().isFriend()
            // == true)
            if (decl_stmt->get_declarationModifier().isFriend() == true) {
#if DEBUG_TEMPLATE_SPECIALIZATION
              printf("Supress the output of the template<> syntax \n");
              curprint("/* Non-Member friend function instantiations cause us "
                       "to supress the output of template<> syntax */ ");
#endif
            } else {
#if DEBUG_TEMPLATE_SPECIALIZATION
              curprint("/* Non-Member (non-friend) function instantiations "
                       "still output template<> syntax */ ");
#endif
              curprint("template<> ");
            }
          } else {
            // DQ (4/11/2019): Check if this is a friend declaration.
            if (decl_stmt->get_declarationModifier().isFriend() == true) {
#if DEBUG_TEMPLATE_SPECIALIZATION
              printf("Supress the output of the template<> syntax for friend "
                     "non-function specializations \n");
              curprint("/* Non-Member friend non-function instantiations cause "
                       "us to supress the output of template<> syntax */ ");
#endif
            } else {
#if DEBUG_TEMPLATE_SPECIALIZATION
              curprint("/* Non function instantiations still output template<> "
                       "syntax */ ");
#endif
              curprint("template<> ");
            }
          }
        }
      }
    }

    // Preprocessor directives can legally appear between the `template<>`
    // specialization specifier and the declaration keyword (e.g., a conditional
    // `#if ... template<> #endif class X<int>;`). Such directives are attached
    // to the declaration as `inside` preprocessing info, but the generic
    // statement driver only emits `before`/`after` lists. Emit the `inside`
    // directives here so conditionals remain balanced and round-trip correctly.
    if (unp != nullptr && unp->u_exprStmt != nullptr) {
      unp->u_exprStmt->unparseAttachedPreprocessingInfo(
          decl_stmt, info, PreprocessingInfo::inside);
    }
  } else if (is_plain_explicit_specialization(decl_stmt)) {
    if (SgVariableDeclaration *variable_decl =
            isSgVariableDeclaration(decl_stmt)) {
      // A source-spelled outer header list is emitted immediately before the
      // variable declaration.  Its empty header already owns the explicit
      // specialization syntax and must not be duplicated here.
      if (variable_decl->get_sourceSpelledTemplateHeaders().empty()) {
        const unsigned int header_count =
            variable_decl->get_explicitTemplateSpecializationHeaderCount();
        if (header_count == 0) {
          fprintf(stderr,
                  "REX_UNPARSE_INVARIANT[variable-specialization-header]: "
                  "explicit specialization=%p class=%s has neither exact "
                  "source-owned headers nor a producer-published header "
                  "count\n",
                  static_cast<void *>(variable_decl),
                  variable_decl->class_name().c_str());
          ROSE_ABORT();
        }
        output_explicit_template_headers(header_count);
      }
    } else if (SgClassDeclaration *class_decl =
                   isSgClassDeclaration(decl_stmt)) {
      // A non-template member-class specialization is a plain
      // SgClassDeclaration whose exact `template<>` levels are source syntax,
      // not a property that can be reconstructed from e_specialization.  The
      // class unparser emits those typed header edges before it calls the
      // generic specifier path.  Synthesizing one more header here duplicates
      // valid source and masks a producer that failed to preserve the syntax.
      const SgTemplateParameterListPtrList &headers =
          class_decl->get_sourceSpelledTemplateHeaders();
      std::size_t emptyHeaderCount = 0;
      for (SgTemplateParameterList *header : headers) {
        if (header == nullptr || header->get_parent() != class_decl) {
          fprintf(stderr,
                  "REX_UNPARSE_INVARIANT[class-specialization-header]: "
                  "explicit specialization=%p has a null or foreign "
                  "source-owned template header\n",
                  static_cast<void *>(class_decl));
          ROSE_ABORT();
        }
        if (header->get_args().empty()) {
          ++emptyHeaderCount;
        }
      }
      if (headers.empty() || emptyHeaderCount == 0) {
        fprintf(stderr,
                "REX_UNPARSE_INVARIANT[class-specialization-header]: "
                "explicit specialization=%p name=%s requires an exact "
                "source-owned empty template header, found headers=%zu "
                "empty=%zu\n",
                static_cast<void *>(class_decl), class_decl->get_name().str(),
                headers.size(), emptyHeaderCount);
        ROSE_ABORT();
      }
    } else {
      // Explicit specializations of non-template declarations nested under
      // class template specializations still require a leading `template<>`.
      output_explicit_template_headers(1);
    }
  }

#if DEBUG_TEMPLATE_SPECIALIZATION
  curprint("\n/* Leaving outputTemplateSpecializationSpecifier() */ ");
#endif
}

void Unparse_MOD_SAGE::printSpecifier2(SgDeclarationStatement *decl_stmt,
                                       SgUnparse_Info &info) {
  // Modern C++ requires linkage before a template specialization specifier.
  // Instantiated functions/member functions and template variables own their
  // linkage spelling in their specialized declaration paths.
  if (isSgTemplateInstantiationFunctionDecl(decl_stmt) == NULL &&
      isSgTemplateInstantiationMemberFunctionDecl(decl_stmt) == NULL &&
      isSgTemplateVariableDeclaration(decl_stmt) == NULL) {
    outputExternLinkageSpecifier(decl_stmt, info);
  }

  outputTemplateSpecializationSpecifier(decl_stmt, info);

  SgFunctionDeclaration *functionDeclaration =
      isSgFunctionDeclaration(decl_stmt);

  // DQ (2/4/2006): Moved output of "friend" keyword inside of test for
  // SgFunctionDeclaration

  // DQ (2/4/2006): Need this case for friend class declarations
  if (decl_stmt->get_declarationModifier().isFriend()) {
    // This assertion fails in test2004_116.C
    // ASSERT_not_null(functionDeclaration);
    if (functionDeclaration == NULL) {
      curprint("friend ");
    }
  }

  if (functionDeclaration != NULL) {
    // curprint("/* printSpecifier2 */ ");

    // DQ (7/26/2014): Added support to output the C11 _Noreturn keyword.
    if (functionDeclaration->get_using_C11_Noreturn_keyword() == true) {
      curprint("_Noreturn ");
    }

    // DQ (8/1/2014): Added support to output the constexpr keyword.
    if (functionDeclaration->get_is_constexpr() == true) {
      curprint("constexpr ");
    }

    // DQ (2/4/2006): Template specialization declarations (forward declaration)
    // can't have some modified output
    bool isDeclarationOfTemplateSpecialization = false;
    SgDeclarationStatement::template_specialization_enum
        specializationEnumValue = functionDeclaration->get_specialization();
    isDeclarationOfTemplateSpecialization =
        (specializationEnumValue == SgDeclarationStatement::e_specialization) ||
        (specializationEnumValue ==
         SgDeclarationStatement::e_partial_specialization);

    // DQ (2/2/2006): friend can't be output for a Template specialization
    // declaration curprint((string("/* isDeclarationOfTemplateSpecialization =
    // ") << ((isDeclarationOfTemplateSpecialization == true) ? string("true") :
    // string("false")) << string(" */ \n ")); printf
    // ("isDeclarationOfTemplateSpecialization = %s
    // \n",isDeclarationOfTemplateSpecialization == true ? "true" : "false");
    if ((decl_stmt->get_declarationModifier().isFriend() == true) &&
        (isDeclarationOfTemplateSpecialization == false)) {
      ASSERT_not_null(decl_stmt->get_parent());
      // DQ (11/28/2015): We need to filter the cases where the function is not
      // output in a class definition. curprint( "friend ");
      if (isSgClassDefinition(decl_stmt->get_parent()) != NULL) {
        curprint("friend ");
      }
    }

    // DQ (2/2/2006): Not sure if virtual can be output when
    // isForwardDeclarationOfTemplateSpecialization == true
    if (functionDeclaration->get_functionModifier().isVirtual()) {

      // DQ (4/11/2019): Only output the "virtual" keyword for functions defined
      // in a class definition. curprint("virtual ");
      SgClassDefinition *classDefinition =
          isSgClassDefinition(functionDeclaration->get_parent());
      if (classDefinition != NULL) {
        // DQ (2/15/2020): It is an error to output "friend virtual" as code.
        // (error: "virtual functions cannot be friends"). See
        // Cxx11_tests/test2020_66.C for an example of where this is currently
        // generated (incorrectly). curprint("virtual ");
        bool isFriend =
            ((decl_stmt->get_declarationModifier().isFriend() == true) &&
             (isDeclarationOfTemplateSpecialization == false));
        if (isFriend == false) {
          curprint("virtual ");
        }
      }
    }

    // if (unp->opt.get_inline_opt())

    // DQ (2/2/2006): Not sure if virtual can be output when
    // isForwardDeclarationOfTemplateSpecialization == true DQ (4/28/2004):
    // output "inline" even for function definitions if
    // (!info.SkipFunctionDefinition())
    if (functionDeclaration->get_functionModifier().isInline()) {
      // DQ (9/25/2013): Check if this is a C file using -std=c89, and if so
      // then unparse "__inline__" instead of "inline". curprint( "inline ");
      SgFile *file = SageInterface::getEnclosingFileNode(functionDeclaration);
      ASSERT_not_null(file);
      if (file->get_C89_only() == true && file->get_C89_gnu_only() == false) {
        // DQ (9/25/2013): This is what is required when using -std=c89 (the
        // default for GNU gcc is -std=gnu89).
        curprint("__inline__ ");
      } else {
        // DQ (6/27/2015): We need this to be output because the
        // isGnuAttributeAlwaysInline() maybe true, but we need to output the
        // "inline" keyword for GNU 4.2.4 compiler (only able to demonstrate the
        // problem on Google protobuffer on RHEL5).
        curprint("inline ");
      }
    }

    // DQ (4/13/2019): We want to output the explicit keyword even when
    // info.SkipFunctionDefinition() == true. DQ (2/2/2006): friend can't be
    // output for a Template specialization declaration if
    // ((!info.SkipFunctionDefinition()) &&
    // functionDeclaration->get_functionModifier().isExplicit()) if (
    // (info.SkipFunctionDefinition() == false) &&
    //      (functionDeclaration->get_functionModifier().isExplicit() == true)
    //      && (isDeclarationOfTemplateSpecialization == false) )
    if ((functionDeclaration->get_functionModifier().isExplicit() == true) &&
        (isDeclarationOfTemplateSpecialization == false)) {
      // DQ (4/13/2019): We can't output the "explicit" keyword for a function
      // outside of it's class. curprint( "explicit "); check that this is a
      // declaration appearing in a class.
      SgClassDefinition *classDefinition =
          isSgClassDefinition(functionDeclaration->get_parent());
      if (classDefinition != NULL ||
          functionDeclaration->get_is_deduction_guide()) {
        curprint("explicit ");
      }
    }

    // TV (04/26/2010): CUDA function modifiers
    if (functionDeclaration->get_functionModifier().isCudaKernel()) {
      curprint("__global__ ");
    }
    if (functionDeclaration->get_functionModifier().isCudaDevice()) {
      curprint("__device__ ");
    }
    if (functionDeclaration->get_functionModifier().isCudaHost()) {
      curprint("__host__ ");
    }

    // TV (05/06/2010): OpenCL function modifiers
    if (functionDeclaration->get_functionModifier().isOpenclKernel()) {
      curprint("__kernel ");
    }
    if (functionDeclaration->get_functionModifier().hasOpenclVecTypeHint()) {
      SgType *openclVecType =
          functionDeclaration->get_functionModifier().get_opencl_vec_type();
      ASSERT_not_null(openclVecType);
      SgUnparse_Info typeInfo(info);
      typeInfo.unset_SkipBaseType();
      typeInfo.set_SkipClassDefinition();
      typeInfo.set_SkipEnumDefinition();
      typeInfo.set_SkipNameQualification();
      curprint("__attribute__((vec_type_hint(");
      if (SgNamedType *namedType = isSgNamedType(openclVecType)) {
        curprint(namedType->get_name().getString());
      } else {
        unp->u_type->unparseType(openclVecType, typeInfo);
      }
      curprint("))) ");
    }
    if (functionDeclaration->get_functionModifier()
            .hasOpenclWorkGroupSizeHint()) {
      const SgFunctionModifier::opencl_work_group_size_t dimensions =
          functionDeclaration->get_functionModifier()
              .get_opencl_work_group_size();
      curprint("__attribute__((work_group_size_hint(" +
               std::to_string(dimensions.x) + ", " +
               std::to_string(dimensions.y) + ", " +
               std::to_string(dimensions.z) + "))) ");
    }
    if (functionDeclaration->get_functionModifier()
            .hasOpenclWorkGroupSizeReq()) {
      const SgFunctionModifier::opencl_work_group_size_t dimensions =
          functionDeclaration->get_functionModifier()
              .get_opencl_work_group_size();
      curprint("__attribute__((reqd_work_group_size(" +
               std::to_string(dimensions.x) + ", " +
               std::to_string(dimensions.y) + ", " +
               std::to_string(dimensions.z) + "))) ");
    }

    // DQ (4/20/2015): Added support for GNU cdecl attribute.
    if (functionDeclaration->get_declarationModifier()
            .get_typeModifier()
            .isGnuAttributeCdecl() == true) {
      curprint("__attribute__((cdecl)) ");
    }
  }

  // DQ (4/25/2004): Removed CC++ specific modifiers
  // if (decl_stmt->isAtomic() && !info.SkipAtomic() ) { curprint( "atomic "); }
  // if (decl_stmt->isGlobalClass() && !info.SkipGlobal() ) { curprint( "global
  // "); }

  // DQ (7/202/2006): The isStatic() function in the SgStorageModifier held by
  // the SgInitializedName object should always be false, and this one held by
  // the SgDeclaration is what is used. printf ("In decl_stmt = %p = %s test the
  // return value of storage.isStatic() = %d = %d (should be boolean value) \n",
  //      decl_stmt,decl_stmt->class_name().c_str(),decl_stmt->get_declarationModifier().get_storageModifier().isStatic(),
  //      decl_stmt->get_declarationModifier().get_storageModifier().get_modifier());

  // This was a bug mistakenly reported by Isaac
  ROSE_ASSERT(decl_stmt->get_declarationModifier()
                  .get_storageModifier()
                  .get_modifier() >= 0);

  if (decl_stmt->get_declarationModifier().get_storageModifier().isStatic()) {
    bool suppress_static_keyword = false;

    // C++ static member functions must not repeat the "static" keyword in
    // out-of-class declarations/definitions (e.g., "int A::f()", not
    // "static int A::f()").
    // NOTE: The Clang frontend preserves `static` only on the in-class
    // declaration (and first nondefining declaration). Other frontends
    // and/or transformations may propagate `static` onto out-of-class
    // member function declarations/definitions; suppressing it here keeps
    // the unparsed C++ correct regardless.
    if (SgMemberFunctionDeclaration *memberFunctionDeclaration =
            isSgMemberFunctionDeclaration(decl_stmt)) {
      SgNode *parent = memberFunctionDeclaration->get_parent();
      if (isSgClassDefinition(parent) == NULL) {
        suppress_static_keyword = true;
      }
    }

    if (suppress_static_keyword == false) {
      curprint("static ");
    }
  }

  // if (unp->opt.get_extern_opt())
  // DQ (5/10/2007): Fixed linkage to be a std::string instead of char*
  // if (decl_stmt->get_declarationModifier().get_storageModifier().isExtern()
  // && !decl_stmt->get_linkage())
  if (decl_stmt->get_declarationModifier().get_storageModifier().isExtern() &&
      decl_stmt->get_linkage().empty() == true) {
    // DQ (7/23/2014): Looking for greater precision in the control of the
    // output of the "extern" keyword.
    ROSE_ASSERT(decl_stmt->get_declarationModifier()
                    .get_storageModifier()
                    .isDefault() == false);

    // DQ (4/11/2019): Don't allow friend and extern together (see
    // Cxx11_tests/test2019_338.C). DQ (1/3/2016): We may have to suppress this
    // for SgTemplateVariableDeclaration IR nodes. curprint("extern ");
    // curprint("/* extern from storageModifier */ extern ");
    // if (isSgTemplateVariableDeclaration(decl_stmt) == NULL)
    if ((decl_stmt->get_declarationModifier().isFriend() == false) &&
        (isSgTemplateVariableDeclaration(decl_stmt) == NULL)) {
      // DQ (9/18/2020): If this is a static variable then Clang does not allow
      // the output of the extern keyword. Check if there is a previous
      // declaration associated with this declaration (e.g. in a namespace as an
      // extern declaration, or in a class as a static variable declaration.
      // curprint( "extern /* Unparse_MOD_SAGE::printSpecifier2() */ ");
      // curprint("extern ");
      bool supress_extern_keyword = false;
      SgVariableDeclaration *variableDeclaration =
          isSgVariableDeclaration(decl_stmt);
      if (variableDeclaration != NULL) {
        SgInitializedName *initializedName =
            SageInterface::getFirstInitializedName(variableDeclaration);
        ROSE_ASSERT(initializedName != NULL);
        SgInitializedName *previous_initializedName =
            initializedName->get_prev_decl_item();
        if (previous_initializedName != NULL) {
          // Only suppress `extern` for out-of-class references to static class
          // members. Ordinary C file-scope redeclarations such as
          // `static int x; extern int x;` must retain `extern`.
          SgVariableDeclaration *previous_variableDeclaration =
              isSgVariableDeclaration(previous_initializedName->get_parent());
          SgScopeStatement *previous_scope =
              previous_variableDeclaration != NULL
                  ? previous_variableDeclaration->get_scope()
                  : NULL;
          const bool previous_decl_is_class_member =
              isSgClassDefinition(previous_scope) != NULL ||
              isSgTemplateClassDefinition(previous_scope) != NULL ||
              isSgTemplateInstantiationDefn(previous_scope) != NULL;
          if (previous_decl_is_class_member &&
              previous_variableDeclaration->get_declarationModifier()
                      .get_storageModifier()
                      .isStatic() == true) {
            supress_extern_keyword = true;
          }
        }
      }

      if (supress_extern_keyword == false) {
        // curprint( "extern /* Unparse_MOD_SAGE::printSpecifier2() */ ");
        curprint("extern ");
      }
    }
  }

  // DQ (12/1/2007): Added support for gnu extension "__thread" (will be
  // available in legacy frontend version > 3.3) But added to support use
  // by Gouchun Shi (UIUC).  Code generation support also added in
  // unparser. This only works on gnu backends and those compatable with
  // gnu (which is a lot of compilers so skip special code to be
  // conditional on the backend).  According to documentation, "__thread"
  // should appear immediately after "extern" or "static" and can not be
  // combined with other storage modifiers.
  if (decl_stmt->get_declarationModifier()
          .get_storageModifier()
          .get_thread_local_storage() == true) {
    curprint("__thread ");
  }

  SgVariableDeclaration *variableDeclaration =
      isSgVariableDeclaration(decl_stmt);
  if (variableDeclaration != NULL) {
    // DQ (8/1/2014): Added support to output the constexpr keyword.
    if (variableDeclaration->get_is_constexpr() == true) {
      curprint("constexpr ");
    }
  }

  if (unp->opt.get_auto_opt()) // checks option status before printing auto (to
                               // prevent redundant use)
  {
    if (decl_stmt->get_declarationModifier().get_storageModifier().isAuto()) {
      curprint("auto ");
    }
  }

  if (decl_stmt->get_declarationModifier().get_storageModifier().isRegister()) {
    curprint("register ");
  }

  if (decl_stmt->get_declarationModifier().get_storageModifier().isMutable()) {
    curprint("mutable ");
  }

  // TV (05/06/2010): CUDA storage modifiers

  if (decl_stmt->get_declarationModifier()
          .get_storageModifier()
          .isCudaGlobal()) {
    curprint("__device__ ");
  }

  if (decl_stmt->get_declarationModifier()
          .get_storageModifier()
          .isCudaConstant()) {
    curprint("__device__ __constant__ ");
  }

  if (decl_stmt->get_declarationModifier()
          .get_storageModifier()
          .isCudaShared()) {
    curprint("__device__ __shared__ ");
  }

  if (decl_stmt->get_declarationModifier()
          .get_storageModifier()
          .isCudaDynamicShared()) {
    curprint("extern __device__ __shared__ ");
  }
}

void Unparse_MOD_SAGE::printSpecifier(SgDeclarationStatement *decl_stmt,
                                      SgUnparse_Info &info) {
  // DQ (4/25/2004):  Old function was redundently represented by two separate
  // functions (call them both from this older function to remove the
  // redundency)
  printSpecifier1(decl_stmt, info);
  printSpecifier2(decl_stmt, info);
}

void Unparse_MOD_SAGE::printAttributes(SgInitializedName *initializedName,
                                       SgUnparse_Info &info) {
  // DQ (2/26/2013): Added support for missing attributes in unparsed code.
  // These are output after the function declaration (and before the body of the
  // function or the closing ";").

  // DQ (9/16/2013): FIXME: __section__, __cleanup__, __init_priority__ are not
  // yet implemented.

  // DQ (9/16/2013): Added support for more GNU attributes.
  if (initializedName->isGnuAttributeUsed() == true) {
    curprint(" __attribute__((used)) ");
  }

  // DQ (9/16/2013): Added support for more GNU attributes.
  if (initializedName->isGnuAttributeUnused() == true) {
    curprint(" __attribute__((unused)) ");
  }

  // DQ (9/16/2013): Added support for more GNU attributes.
  if (initializedName->isGnuAttributeWeak() == true) {
    curprint(" __attribute__((weak)) ");
  }

  // DQ (9/16/2013): Added support for more GNU attributes.
  if (initializedName->isGnuAttributeDeprecated() == true) {
    curprint(" __attribute__((deprecated)) ");
  }

  // DQ (9/16/2013): Added support for more GNU attributes.
  if (initializedName->isGnuAttributeNoCommon() == true) {
    curprint(" __attribute__((noCommon)) ");
  }

  // DQ (9/16/2013): Added support for more GNU attributes.
  if (initializedName->isGnuAttributeTransparentUnion() == true) {
    curprint(" __attribute__((transparent_union)) ");
  }

  // DQ (9/16/2013): Added support for more GNU attributes.
  if (initializedName->isGnuAttributeWeakReference() == true) {
    curprint(" __attribute__((weak_reference)) ");
  }

  // DQ (1/18/2014): Adding support for GNU specific noreturn attribute for
  // variable (only applies to variable that are of function pointer type).
  if (initializedName->isGnuAttributeNoReturn() == true) {
    curprint(" __attribute__((noreturn)) ");
  }

  if (initializedName->isGnuAttributeNoReorder() == true) {
    curprint(" __attribute__((no_reorder)) ");
  }

  if (initializedName->get_gnu_attribute_section_name().size() > 0) {
    curprint(" __attribute__((section(\"");
    curprint(initializedName->get_gnu_attribute_section_name());
    curprint("\"))) ");
  }

  // DQ (3/1/2013): The default value is changed from zero to -1 (and the type
  // was make to be a short (signed) value).
  short alignmentValue = initializedName->get_gnu_attribute_alignment();

  // DQ (7/26/2014): Adding support for _Alignas keyword.
  bool using_Alignas_keyword =
      (initializedName->get_using_C11_Alignas_keyword() == true);

  // if (alignmentValue >= 0)
  if (alignmentValue >= 0 && using_Alignas_keyword == false) {
    // DQ (7/26/2014): Fixed error in using "align" (mistake), changed to
    // "aligned". curprint( " __attribute__((align(N)))"); curprint( "
    // __attribute__((align(");
    curprint(" __attribute__((aligned(");
    curprint(StringUtility::numberToString((int)alignmentValue));
    curprint("))) ");
  }
}

void Unparse_MOD_SAGE::printAttributesForType(SgDeclarationStatement *decl_stmt,
                                              SgUnparse_Info &info) {
  // DQ (12/31/2013): Added support for missing attributes on types within
  // declarations (in unparsed code).

  ASSERT_not_null(decl_stmt);

  SgVariableDeclaration *variableDeclaration =
      isSgVariableDeclaration(decl_stmt);
  if (variableDeclaration != NULL) {
    // DQ (12/18/2013): Added support for output of packed attribute (see
    // test2013_104.c (required after variable) and test2013_113.c (required
    // after type and before variable)).
    if (decl_stmt->get_declarationModifier()
            .get_typeModifier()
            .isGnuAttributePacked() == true) {
      // curprint(" /* from printAttributesForType(SgDeclarationStatement*) */
      // __attribute__((packed))");
      curprint(" __attribute__((packed))");
    }
  }

  // DQ (1/6/2014): Added support for specification of noreturn (function type)
  // attribute. This is one of two place where the attribute may be used (after
  // the function declaration) and after the function pointer function parameter
  // in a function's parameter list.
  SgFunctionDeclaration *functionDeclaration =
      isSgFunctionDeclaration(decl_stmt);
  if (functionDeclaration != NULL) {
    // DQ (7/26/2014): Fixed for better handling of C11 _Noreturn keyword.
    // if
    // (functionDeclaration->get_declarationModifier().get_typeModifier().isGnuAttributeNoReturn()
    // == true)
    if (functionDeclaration->get_declarationModifier()
                .get_typeModifier()
                .isGnuAttributeNoReturn() == true &&
        functionDeclaration->get_using_C11_Noreturn_keyword() == false) {
      curprint(" __attribute__((noreturn))");
    }

    if (functionDeclaration->get_declarationModifier()
            .get_typeModifier()
            .isGnuAttributeConst() == true) {
      curprint(" __attribute__((const))");
    }

    if (functionDeclaration->get_declarationModifier()
            .get_typeModifier()
            .isGnuAttributeWarnUnusedResult() == true) {
      curprint(" __attribute__((warn_unused_result))");
    }

    const SgFunctionModifier::gnu_attribute_parameter_index_list_list_t
        &nonnullParameterIndexLists =
            functionDeclaration->get_functionModifier()
                .get_gnu_attribute_nonnull_parameter_index_lists();
    if (nonnullParameterIndexLists.empty() == false) {
      for (const SgFunctionModifier::gnu_attribute_parameter_index_list_t
               &indices : nonnullParameterIndexLists) {
        curprint(" __attribute__((nonnull");
        if (indices.empty() == false) {
          curprint("(");
          for (size_t i = 0; i < indices.size(); ++i) {
            if (i != 0) {
              curprint(", ");
            }
            curprint(StringUtility::numberToString(indices[i]));
          }
          curprint(")");
        }
        curprint("))");
      }
    } else if (functionDeclaration->get_declarationModifier()
                   .get_typeModifier()
                   .isGnuAttributeNonnull() == true) {
      curprint(" __attribute__((nonnull))");
    }

    // DQ (2/7/2014): attribute set using:
    // decl->get_functionModifier().set_gnu_attribute_named_alias(alias_name);
    if (functionDeclaration->get_functionModifier()
                .get_gnu_attribute_named_alias()
                .empty() == false &&
        functionDeclaration->get_functionModifier()
                .isGnuAttributeWeakReference() == false) {
      string alias = functionDeclaration->get_functionModifier()
                         .get_gnu_attribute_named_alias();
      // curprint(" __attribute__((noreturn))");
      curprint(" __attribute__((alias(\"");
      curprint(alias);
      curprint("\")))");
    }
  }
}

void Unparse_MOD_SAGE::printAttributes(SgDeclarationStatement *decl_stmt,
                                       SgUnparse_Info &info) {
  // DQ (2/26/2013): Added support for missing attributes in unparsed code.
  // These are output after the function declaration (and before the body of the
  // function or the closing ";").

  ASSERT_not_null(decl_stmt);

  if (decl_stmt->get_declarationModifier().isThrow() == true) {
    // DQ (2/26/2013): This is output as part of the function type unparsing
    // (since it is a part of the type system). curprint( " throw()");
  }

  short alignmentValue = decl_stmt->get_declarationModifier()
                             .get_typeModifier()
                             .get_gnu_attribute_alignment();

  // DQ (3/1/2013): The default value is changed from zero to -1 (and the type
  // was make to be a short (signed) value).
  if (alignmentValue >= 0) {
    // DQ (7/26/2014): Fixed error in using "align" (mistake), changed to
    // "aligned". curprint(" __attribute__((align(N)))"); curprint("
    // __attribute__((align(");
    curprint(" __attribute__((aligned(");
    curprint(StringUtility::numberToString((int)alignmentValue));
    curprint(")))");
  }

  SgVariableDeclaration *variableDeclaration =
      isSgVariableDeclaration(decl_stmt);
  if (variableDeclaration != NULL) {
    // DQ (12/18/2013): Added support for output of packed attribute (see
    // test2013_104.c). DQ (12/31/2013): Note that we need to look at the
    // SgInitializedName in the variable declaration, since we use the type
    // modifier on the declaration to set the attributes for the type (not the
    // variable).
    SgInitializedName *initializedName =
        SageInterface::getFirstInitializedName(variableDeclaration);
    ASSERT_not_null(initializedName);
    initializedName->isGnuAttributePacked();
    if (initializedName->isGnuAttributePacked() == true) {
      // curprint(" /* from printAttributes(SgDeclarationStatement*) triggered
      // from SgInitializedName */ __attribute__((packed))");
      curprint(" __attribute__((packed)) ");
    }
  }

  SgFunctionDeclaration *functionDeclaration =
      isSgFunctionDeclaration(decl_stmt);
  if (functionDeclaration != NULL) {

    // DQ (2/26/2013): Added noinline attribute code generation.
    if (functionDeclaration->get_functionModifier()
            .isGnuAttributeConstructor() == true) {
      curprint(" __attribute__((constructor)) ");
    }

    if (functionDeclaration->get_functionModifier()
            .isGnuAttributeDestructor() == true) {
      curprint(" __attribute__((destructor)) ");
    }

    if (functionDeclaration->get_functionModifier().isGnuAttributePure() ==
        true) {
      curprint(" __attribute__((pure)) ");
    }

    if (functionDeclaration->get_functionModifier().isGnuAttributeWeak() ==
        true) {
      curprint(" __attribute__((weak)) ");
    }

    if (functionDeclaration->get_functionModifier().isGnuAttributeUnused() ==
        true) {
      curprint(" __attribute__((unused)) ");
    }

    if (functionDeclaration->get_functionModifier().isGnuAttributeUsed() ==
        true) {
      curprint(" __attribute__((used)) ");
    }
    if (functionDeclaration->get_functionModifier()
            .isGnuAttributeDeprecated() == true) {
      curprint(" __attribute__((deprecated)) ");
    }

    if (functionDeclaration->get_functionModifier().isGnuAttributeMalloc() ==
        true) {
      curprint(" __attribute__((malloc)) ");
    }

    if (functionDeclaration->get_functionModifier().isGnuAttributeNaked() ==
        true) {
      curprint(" __attribute__((naked)) ");
    }

    if (functionDeclaration->get_functionModifier()
            .isGnuAttributeNoInstrumentFunction() == true) {
      curprint(" __attribute__((no_instrument_function)) ");
    }

    if (functionDeclaration->get_functionModifier()
            .isGnuAttributeNoCheckMemoryUsage() == true) {
      curprint(" __attribute__((no_check_memory_usage)) ");
    }

    if (functionDeclaration->get_functionModifier().isGnuAttributeNoInline() ==
        true) {
      curprint(" __attribute__((noinline)) ");
    }

    if (functionDeclaration->get_functionModifier()
            .isGnuAttributeAlwaysInline() == true) {
      // GNU attribute identifiers accept their reserved `__name__` spelling.
      // Use it for reconstructed syntax so an application macro named
      // `always_inline` cannot rewrite the attribute payload itself.
      curprint(" __attribute__((__always_inline__)) ");
    }

    if (functionDeclaration->get_functionModifier().isGnuAttributeNoThrow() ==
        true) {
      curprint(" __attribute__((no_throw)) ");
    }

    if (functionDeclaration->get_functionModifier()
            .isGnuAttributeWeakReference() == true) {
      const string alias = functionDeclaration->get_functionModifier()
                               .get_gnu_attribute_named_alias();
      if (alias.empty()) {
        curprint(" __attribute__((weakref)) ");
      } else {
        curprint(" __attribute__((weakref(\"");
        curprint(alias);
        curprint("\"))) ");
      }
    }
  }

  // A class declaration emits these in its decl-specifier sequence, between
  // the class-key and name.  Printing them here after the name produces an
  // ill-formed class definition (`struct Name __attribute__(...) {}`).
  if (isSgClassDeclaration(decl_stmt) == nullptr) {
    printGnuVisibilityAttributes(decl_stmt, /*leading_space=*/true);
  }
}

void Unparse_MOD_SAGE::printGnuVisibilityAttributes(
    SgDeclarationStatement *decl_stmt, bool leading_space) {
  ASSERT_not_null(decl_stmt);
  bool emitted = false;
  auto printVisibilityAttribute =
      [&](SgDeclarationModifier::gnu_declaration_visibility_enum visibility,
          const char *attributeName, const char *diagnosticKind) {
        ASSERT_not_null(attributeName);
        ASSERT_not_null(diagnosticKind);
        if (visibility == SgDeclarationModifier::e_unspecified_visibility) {
          return;
        }
        string spelling;
        switch (visibility) {
        case SgDeclarationModifier::e_unknown_visibility:
          fprintf(stderr,
                  "REX_UNPARSE_INVARIANT[gnu-visibility]: declaration has "
                  "unknown %s\n",
                  diagnosticKind);
          ROSE_ABORT();
        case SgDeclarationModifier::e_error_visibility:
          fprintf(stderr,
                  "REX_UNPARSE_INVARIANT[gnu-visibility]: declaration "
                  "has error %s\n",
                  diagnosticKind);
          ROSE_ABORT();
        case SgDeclarationModifier::e_unspecified_visibility:
          fprintf(stderr,
                  "REX_UNPARSE_INVARIANT[gnu-visibility]: internal visibility "
                  "dispatch received unspecified visibility\n");
          ROSE_ABORT();
        case SgDeclarationModifier::e_hidden_visibility:
          spelling = "hidden";
          break;
        case SgDeclarationModifier::e_protected_visibility:
          spelling = "protected";
          break;
        case SgDeclarationModifier::e_internal_visibility:
          spelling = "internal";
          break;
        case SgDeclarationModifier::e_default_visibility:
          spelling = "default";
          break;
        default:
          fprintf(stderr,
                  "REX_UNPARSE_INVARIANT[gnu-visibility]: declaration has "
                  "invalid %s=%d\n",
                  diagnosticKind, static_cast<int>(visibility));
          ROSE_ABORT();
        }
        if (leading_space || emitted) {
          curprint(" ");
        }
        curprint("__attribute__((");
        curprint(attributeName);
        curprint("(\"");
        curprint(spelling);
        curprint("\"))) ");
        emitted = true;
      };

  const SgDeclarationModifier &declarationModifier =
      decl_stmt->get_declarationModifier();
  printVisibilityAttribute(declarationModifier.get_gnu_attribute_visibility(),
                           "visibility", "visibility");
  printVisibilityAttribute(declarationModifier.get_gnu_type_visibility(),
                           "type_visibility", "type visibility");
}

void Unparse_MOD_SAGE::printPrefixAttributes(SgDeclarationStatement *decl_stmt,
                                             SgUnparse_Info &info) {
  SgFunctionDeclaration *functionDeclaration =
      isSgFunctionDeclaration(decl_stmt);
  if (functionDeclaration != NULL) {
    // DQ (1/19/2014): Add support for prefix attributes.
    // DQ (1/19/2014): Adding support for gnu attribute regnum to support use in
    // Valgrind application.
    int gnu_regparm_value = functionDeclaration->get_gnu_regparm_attribute();

    // DQ (5/27/2015): Note that zero is a ligitimate value to use, so the
    // default should be -1. if (gnu_regparm_value > 0)
    if (gnu_regparm_value >= 0) {
      string s = StringUtility::numberToString(gnu_regparm_value);
      curprint(" __attribute__((regparm(");
      curprint(s);

      // Add trailing space since this is for a prefixed attribute.
      curprint("))) ");
    }
  }
}

void Unparse_MOD_SAGE::setupColorCodes(
    vector<pair<bool, std::string>> &stateVector) {
  ROSE_ASSERT(stateVector.empty());
  stateVector.push_back(pair<bool, std::string>(false, string("red")));
  stateVector.push_back(pair<bool, std::string>(false, string("orange")));
  stateVector.push_back(pair<bool, std::string>(false, string("blue")));
  stateVector.push_back(pair<bool, std::string>(false, string("green")));
  stateVector.push_back(pair<bool, std::string>(false, string("yellow")));
}

void Unparse_MOD_SAGE::printColorCodes(
    SgNode *node, bool openState,
    vector<pair<bool, std::string>> &stateVector) {
  // This function is part of test to embed color codes (or other information)
  // into the unparsed output.

  SgLocatedNode *locatedNode = isSgLocatedNode(node);

  // printf ("stateVector.size() = %ld \n",stateVector.size());

  // The implementation below addresses entries through index four.
  ROSE_ASSERT(stateVector.size() >= 5);
  if (activeColorCodeStates.empty()) {
    activeColorCodeStates.assign(stateVector.size(), false);
  }
  ROSE_ASSERT(activeColorCodeStates.size() == stateVector.size());
  // There are a few IR nodes for which we want to skip any colorization because
  // it would color the whole file or parts too large to be meaningful.
  if (isSgGlobal(node) != NULL) {
    return;
  }

  if (openState == true && locatedNode != NULL) {
    Sg_File_Info *startOfConstruct = locatedNode->get_startOfConstruct();
    Sg_File_Info *endOfConstruct = locatedNode->get_endOfConstruct();

    // turn the color codes on
    if (startOfConstruct == NULL) {
      // Only the nesting level that activates a session color may emit it.
      if (activeColorCodeStates[0] == false) {
        stateVector[0].first = true;

        // Keep the session state active until this nesting level closes.
        activeColorCodeStates[0] = true;
      }
    }

    if (startOfConstruct != NULL) {
      if (startOfConstruct->ok() == false) {
        if (activeColorCodeStates[1] == false) {
          stateVector[1].first = true;
          activeColorCodeStates[1] = true;
        }
      }
    }

    if (endOfConstruct == NULL) {
      if (activeColorCodeStates[0] == false) {
        stateVector[0].first = true;
        activeColorCodeStates[0] = true;
      }
    }

    if (endOfConstruct != NULL) {
      if (activeColorCodeStates[1] == false) {
        if (endOfConstruct->ok() == false) {
          stateVector[1].first = true;
          activeColorCodeStates[1] = true;
        }
      }
    }

    if (startOfConstruct != NULL && endOfConstruct != NULL) {
      // Tests for consistancy of compiler generated IR nodes
      if (startOfConstruct->isCompilerGenerated() == true) {
        if (activeColorCodeStates[4] == false) {
          stateVector[4].first = true;
          activeColorCodeStates[4] = true;
        }
      }

      if (endOfConstruct->isCompilerGenerated() == true) {
        if (activeColorCodeStates[4] == false) {
          stateVector[4].first = true;
          activeColorCodeStates[4] = true;
        }
      }

      if (startOfConstruct->isCompilerGenerated() !=
          endOfConstruct->isCompilerGenerated()) {
        if (activeColorCodeStates[0] == false) {
          stateVector[0].first = true;
          activeColorCodeStates[0] = true;
        }
      }
    }

    // turn the color code on
    for (unsigned int i = 0; i < stateVector.size(); i++) {
      // If the local stat has be set then we can output the embedded color
      // code.
      if (stateVector[i].first == true) {
        curprint(" /* colorCode:" + stateVector[i].second + ":on */ ");
      }
    }
  }

  if (openState == false && locatedNode != NULL) {
    // turn the color code off
    // for (unsigned int i = 0; i < stateVector.size(); i++)
    int size = stateVector.size();
    for (int i = size - 1; i >= 0; i--) {
      // Since we only turn on those states that were not previously set, we can
      // turn off all local states.
      if (stateVector[i].first == true) {
        curprint(" /* colorCode:" + stateVector[i].second + ":off */ ");

        ROSE_ASSERT(activeColorCodeStates[i]);
        activeColorCodeStates[i] = false;
      }
    }
  }
}
