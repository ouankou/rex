#include "sage3basic.h"

#include "AstInterface_ROSE.h"

#include "CPPAstInterface.h"

bool CPPAstInterface ::IsMemberAccess(const AstNodePtr &_s, AstNodePtr *obj,
                                      std::string *field) {
  SgNode *s = AstNodePtrImpl(_s).get_ptr();
  if (isSgDotExp(s) == nullptr && isSgArrowExp(s) == nullptr)
    return false;
  auto *member = isSgBinaryOp(s);
  SgExpression *selector = member->get_rhs_operand();
  ASSERT_not_null(selector);
  ASSERT_not_null(member->get_lhs_operand());
  ROSE_ASSERT(selector->get_parent() == member &&
              member->get_lhs_operand()->get_parent() == member);
  (void)selector->get_type();
  SgSymbol *symbol = nullptr;
  if (auto *variable = isSgVarRefExp(selector)) {
    symbol = variable->get_symbol();
  } else if (auto *reference = isSgNonrealRefExp(selector)) {
    if (reference->get_resolved_function_declaration() != nullptr ||
        reference->get_semantic_role() == SgNonrealRefExp::e_dependent_callable)
      return false;
    symbol = reference->get_symbol();
  } else {
    return false;
  }
  ASSERT_not_null(symbol);
  ASSERT_not_null(symbol->get_symbol_basis());
  ROSE_ASSERT(!symbol->get_name().is_null());
  if (obj != nullptr)
    *obj = member->get_lhs_operand();
  if (field != nullptr)
    *field = symbol->get_name().getString();
  return true;
}

AstNodePtr CPPAstInterface::CreateFunctionCall(const AstNodePtr &func,
                                               const AstNodeList &args) {
  return AstNodePtrImpl(impl->CreateFunctionCall(AstNodePtrImpl(func).get_ptr(),
                                                 args.begin(), args.end()));
}

bool CPPAstInterface ::IsMemberFunctionCall(const AstNodePtr &_s,
                                            AstNodePtr *obj, std::string *func,
                                            AstNodePtr *access,
                                            AstNodeList *_args) {
  SgNode *s = AstNodePtrImpl(_s).get_ptr();
  SgNode *f;
  AstInterface::AstNodeList args;
  if (!impl->IsFunctionCall(s, &f, &args)) {
    return false;
  }
  if (isSgFunctionRefExp(f) == nullptr &&
      isSgTemplateFunctionRefExp(f) == nullptr &&
      isSgMemberFunctionRefExp(f) == nullptr &&
      isSgTemplateMemberFunctionRefExp(f) == nullptr) {
    return false;
  }
  SgFunctionDeclaration *called =
      SgFunctionCallExp::getAssociatedFunctionDeclarationForCallee(
          isSgExpression(f));
  ASSERT_not_null(called);
  auto *declaration = isSgMemberFunctionDeclaration(called);
  if (declaration == nullptr)
    return false;
  const bool hasObjectArgument =
      isSgMemberFunctionRefExp(f) != nullptr ||
      isSgTemplateMemberFunctionRefExp(f) != nullptr ||
      isSgDotExp(f->get_parent()) != nullptr ||
      isSgArrowExp(f->get_parent()) != nullptr;
  if (hasObjectArgument)
    ROSE_ASSERT(!args.empty());
  if (access != nullptr) {
    SgNode *callee = f;
    while (isSgExpression(callee->get_parent()) != nullptr &&
           isSgFunctionCallExp(callee->get_parent()) == nullptr)
      callee = callee->get_parent();
    auto *call = isSgFunctionCallExp(callee->get_parent());
    ROSE_ASSERT(call != nullptr && call->get_function() == callee);
    *access = callee;
  }
  if (func != nullptr) {
    auto *instantiation =
        isSgTemplateInstantiationMemberFunctionDecl(declaration);
    *func = (instantiation != nullptr ? instantiation->get_templateName()
                                      : declaration->get_name())
                .getString();
  }
  const bool isStatic =
      declaration->get_declarationModifier().get_storageModifier().isStatic();
  ROSE_ASSERT(isStatic || hasObjectArgument);
  if (obj != nullptr)
    *obj = isStatic ? AST_NULL : args.front();
  if (hasObjectArgument)
    args.erase(args.begin());
  if (_args != nullptr)
    *_args = args;
  return true;
}
