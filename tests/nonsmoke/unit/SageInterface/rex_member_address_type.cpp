#include "astJson/sageAstJson.h"
#include "ompAstConstruction.h"
#include "rose.h"
#include <cstdlib>
#include <iterator>
#include <utility>

namespace {
void checkCallResults(SgNode *root) {
  struct CallCase {
    const char *name;
    VariantT type;
    bool lvalue;
    bool parameter;
  };
  const CallCase cases[] = {
      {"rex_call_direct", V_SgReferenceType, true, false},
      {"rex_call_indirect", V_SgReferenceType, true, true},
      {"rex_call_reference", V_SgReferenceType, true, true},
      {"rex_call_operator", V_SgReferenceType, true, false},
      {"rex_call_member", V_SgReferenceType, true, false},
      {"rex_call_xvalue", V_SgRvalueReferenceType, false, true},
      {"rex_call_function", V_SgRvalueReferenceType, true, true},
      {"rex_call_prvalue", V_SgTypeInt, false, false}};
  unsigned checked = 0;
  for (SgNode *node : NodeQuery::querySubTree(root, V_SgFunctionDeclaration)) {
    auto *function = isSgFunctionDeclaration(node);
    if (function->get_definition() == nullptr)
      continue;
    for (const auto &entry : cases) {
      if (function->get_name() != entry.name)
        continue;
      const auto calls = NodeQuery::querySubTree(
          function->get_definition()->get_body(), V_SgFunctionCallExp);
      ROSE_ASSERT(calls.size() == 1);
      auto *call = isSgFunctionCallExp(calls.front());
      auto *expected = function->get_type()->get_return_type();
      ROSE_ASSERT(call->get_type()->stripTypedefsAndModifiers()->variantT() ==
                  entry.type);
      ROSE_ASSERT(SageInterface::isEquivalentType(call->get_type(), expected));
      ROSE_ASSERT(call->isLValue() == entry.lvalue);
      if (entry.parameter) {
        const auto &parameters = function->get_parameterList()->get_args();
        ROSE_ASSERT(parameters.size() == 1);
        auto *fresh = SageBuilder::buildFunctionCallExp(
            SageBuilder::buildVarRefExp(parameters.front()), expected,
            SageBuilder::buildExprListExp());
        ROSE_ASSERT(fresh->isLValue() == entry.lvalue);
        SageInterface::deleteAST(
            fresh, SageInterface::DeleteAstMode::kRequireIsolated);
      }
      ++checked;
    }
  }
  ROSE_ASSERT(checked == std::size(cases));
}

void checkPragmaAddresses(SgSourceFile *file) {
  OmpSupport::OpenMPConversionSession session(file);
  auto *scope = file->get_globalScope();
  for (const auto &[source, array_operand] :
       {std::pair{"expr(RexAddressSpace::pointer_result()[0])", false},
        std::pair{"expr(RexAddressSpace::pointer[0])", false},
        std::pair{"expr(RexAddressSpace::array[0])", true}}) {
    auto *element = isSgPntrArrRefExp(parseExpression(scope, source));
    ROSE_ASSERT(element != nullptr && element->isLValue());
    auto *base = element->get_lhs_operand();
    ROSE_ASSERT(element->isChildUsedAsLValue(base) == array_operand);
    ROSE_ASSERT(base->isUsedAsLValue() == array_operand);
    ROSE_ASSERT(!element->isChildUsedAsLValue(element->get_rhs_operand()));
    if (array_operand) {
      auto *reversed = SageBuilder::buildPntrArrRefExp(
          SageInterface::copyExpression(element->get_rhs_operand()),
          SageInterface::copyExpression(base), element->get_type());
      ROSE_ASSERT(reversed->isLValue());
      ROSE_ASSERT(!reversed->isChildUsedAsLValue(reversed->get_lhs_operand()));
      ROSE_ASSERT(reversed->isChildUsedAsLValue(reversed->get_rhs_operand()));
      ROSE_ASSERT(reversed->get_rhs_operand()->isUsedAsLValue());
      SageInterface::deleteAST(reversed,
                               SageInterface::DeleteAstMode::kRequireIsolated);
    }
    SageInterface::deleteAST(element,
                             SageInterface::DeleteAstMode::kRequireIsolated);
  }
  auto *dereference = isSgPointerDerefExp(
      parseExpression(scope, "expr(*RexAddressSpace::pointer)"));
  ROSE_ASSERT(dereference != nullptr && dereference->isLValue());
  ROSE_ASSERT(!dereference->isChildUsedAsLValue(dereference->get_operand()));
  ROSE_ASSERT(!dereference->get_operand()->isUsedAsLValue());
  SageInterface::deleteAST(dereference,
                           SageInterface::DeleteAstMode::kRequireIsolated);
  for (const auto &[source, form] :
       {std::pair{"expr(&RexAddressSpace::value)",
                  SgAddressOfOp::e_address_operand_qualified_id},
        std::pair{"expr(&RexAddressSpace :: value)",
                  SgAddressOfOp::e_address_operand_qualified_id},
        std::pair{"expr(&(RexAddressSpace::value))",
                  SgAddressOfOp::e_address_operand_object},
        std::pair{"expr(&((RexAddressSpace::value)))",
                  SgAddressOfOp::e_address_operand_object},
        std::pair{"expr(&*(&RexAddressSpace::value))",
                  SgAddressOfOp::e_address_operand_object},
        std::pair{"expr(&RexAddressSpace::reference)",
                  SgAddressOfOp::e_address_operand_qualified_id},
        std::pair{"expr(&(RexAddressSpace::reference))",
                  SgAddressOfOp::e_address_operand_object}}) {
    auto *address = isSgAddressOfOp(parseExpression(scope, source));
    ROSE_ASSERT(address != nullptr &&
                address->get_address_operand_form() == form);
    ROSE_ASSERT(!address->isLValue());
    ROSE_ASSERT(address->isChildUsedAsLValue(address->get_operand()));
    ROSE_ASSERT(address->get_operand()->get_parent() == address);
    ROSE_ASSERT(address->get_type() ==
                SageBuilder::buildPointerType(SageBuilder::buildIntType()));
    auto *copy = isSgAddressOfOp(SageInterface::copyExpression(address));
    ROSE_ASSERT(copy != nullptr && copy->get_address_operand_form() == form &&
                copy->get_operand()->get_parent() == copy);
    ROSE_ASSERT(!copy->isLValue());
    SageInterface::deleteAST({address, copy},
                             SageInterface::DeleteAstMode::kRequireIsolated);
  }
}

void checkMemberPointerIdentity(SgNode *root) {
  SgType *base = nullptr, *derived = nullptr;
  for (SgNode *node : NodeQuery::querySubTree(root, V_SgClassDeclaration)) {
    auto *declaration = isSgClassDeclaration(node);
    if (declaration->get_name() == "RexBase")
      base = declaration->get_type();
    if (declaration->get_name() == "RexDerived")
      derived = declaration->get_type();
  }
  ROSE_ASSERT(base != nullptr && derived != nullptr && base != derived);
  auto *integer = SageBuilder::buildIntType();
  SgPointerMemberType first(integer, base), same(integer, base);
  SgPointerMemberType other(integer, derived);
  SgPointerType ordinary(integer);
  auto check = [](SgType *left, SgType *right, bool equivalent) {
    ROSE_ASSERT(SageInterface::isEquivalentType(left, right) == equivalent);
    ROSE_ASSERT(SageInterface::isEquivalentType(right, left) == equivalent);
    ROSE_ASSERT(SageInterface::cxxSourceTypeMatchesSemanticType(left, right) ==
                equivalent);
    ROSE_ASSERT(SageInterface::cxxSourceTypeMatchesSemanticType(right, left) ==
                equivalent);
  };
  check(&first, &same, true);
  check(&first, &other, false);
  check(&first, &ordinary, false);
  ROSE_ASSERT(!SageInterface::cxxNonTypeTemplateArgumentTypeConversionIsExact(
      &first, &ordinary));
  ROSE_ASSERT(!SageInterface::cxxNonTypeTemplateArgumentTypeConversionIsExact(
      &ordinary, &first));
  ROSE_ASSERT(SageInterface::cxxNonTypeTemplateArgumentTypeConversionIsExact(
      SgTypeNullptr::createType(), &first));
}

void checkAddresses(SgNode *project, bool replace) {
  checkCallResults(project);
  checkMemberPointerIdentity(project);
  unsigned members = 0, objects = 0, dependent = 0, object_accesses = 0;
  for (SgNode *node : NodeQuery::querySubTree(project, V_SgAddressOfOp)) {
    auto *address = isSgAddressOfOp(node);
    ROSE_ASSERT(!address->isLValue());
    auto *reference = isSgVarRefExp(address->get_operand());
    auto *access = isSgArrowExp(address->get_operand());
    auto *field = reference != nullptr ? reference
                  : access != nullptr ? isSgVarRefExp(access->get_rhs_operand())
                                      : nullptr;
    if (field == nullptr || !SageInterface::isDataMemberReference(field))
      continue;
    SgType *type = address->get_type();
    ROSE_ASSERT(type != nullptr);
    const auto form = address->get_address_operand_form();
    ROSE_ASSERT(form == SgAddressOfOp::e_address_operand_object ||
                form == SgAddressOfOp::e_address_operand_qualified_id);
    const bool member = form == SgAddressOfOp::e_address_operand_qualified_id;
    if (auto *member_type = isSgPointerMemberType(type)) {
      ROSE_ASSERT(member);
      ROSE_ASSERT(!SageInterface::containsUnknownType(member_type));
      SgType *unknown = SgTypeUnknown::createType();
      SgPointerMemberType unknown_class(member_type->get_base_type(), unknown);
      SgPointerMemberType unknown_member(unknown,
                                         member_type->get_class_type());
      ROSE_ASSERT(SageInterface::containsUnknownType(&unknown_class));
      ROSE_ASSERT(SageInterface::containsUnknownType(&unknown_member));
      SgMemberFunctionType unknown_function_class(
          SageBuilder::buildIntType(), false, new SgFunctionParameterTypeList(),
          member_type->get_class_type(), 0);
      unknown_function_class.set_class_type(unknown);
      ROSE_ASSERT(SageInterface::containsUnknownType(&unknown_function_class));
    } else if (isSgPointerType(type) != nullptr)
      ROSE_ASSERT(!member);
    else {
      ROSE_ASSERT(isSgTypeDependent(type) != nullptr);
      ++dependent;
    }
    if (reference != nullptr) {
      ROSE_ASSERT(SageInterface::isAddressOfCurrentObjectDataMemberReference(
                      reference) == !member);
    } else {
      ROSE_ASSERT(access != nullptr && !member);
      ROSE_ASSERT(field->get_parent() == access &&
                  access->get_parent() == address);
      ++object_accesses;
    }
    members += member;
    objects += !member;
    if (replace && reference != nullptr) {
      // Build a fresh expression with the exact semantic type and symbol. No
      // source qualifier survives to hide a semantic qualification error.
      auto *fresh = SageBuilder::buildAddressOfOp(
          SageBuilder::buildVarRefExp(reference->get_symbol()), type, form);
      SageInterface::replaceExpression(address, fresh);
      ROSE_ASSERT(fresh->get_type() == type);
      ROSE_ASSERT(SageInterface::isAddressOfCurrentObjectDataMemberReference(
                      isSgVarRefExp(fresh->get_operand())) == !member);
    }
  }
  ROSE_ASSERT(members >= 12 && objects >= 1 && dependent >= 3 &&
              object_accesses >= 1);
}
} // namespace

int main(int argc, char **argv) {
  auto *project = frontend(argc, argv);
  ROSE_ASSERT(project != nullptr);
  auto *file = isSgSourceFile(&project->get_file(0));
  ROSE_ASSERT(file != nullptr);
  ROSE_ASSERT(file->get_isCommandLineInputSource() &&
              !file->get_isGeneratedSource());
  checkPragmaAddresses(file);
  checkAddresses(project, true);
  SgTreeCopy copier;
  auto *copy = isSgProject(project->copy(copier));
  ROSE_ASSERT(copy != nullptr && copy != project);
  checkAddresses(copy, false);
  copy->skipfinalCompileStep(true);
  const int copy_status = backend(copy);
  ROSE_ASSERT(copy_status == 0);
  auto *copied_files = copy->get_fileList_ptr();
  SageInterface::deleteAST(copy,
                           SageInterface::DeleteAstMode::kRequireIsolated);
  ROSE_ASSERT(!SgNode::isLiveNode(copied_files));
  checkAddresses(project, false);
  Rose::AstJson::Options options;
  options.outputDirectory = "rex_member_address_json";
  ROSE_ASSERT(setenv("REX_AST_JSON_CHECKPOINT", "pre-omp-construction", 1) ==
              0);
  auto *restored = Rose::AstJson::roundTripSourceFile(
      file, Rose::AstJson::Checkpoint::PreOmpConstruction, options);
  ROSE_ASSERT(restored != nullptr && restored != file);
  ROSE_ASSERT(restored->get_isCommandLineInputSource() &&
              !restored->get_isGeneratedSource());
  checkAddresses(restored, false);
  project->skipfinalCompileStep(true);
  const int status = backend(project);
  auto *project_files = project->get_fileList_ptr();
  SageInterface::deleteAST(project,
                           SageInterface::DeleteAstMode::kRequireIsolated);
  ROSE_ASSERT(!SgNode::isLiveNode(project_files));
  return status;
}
