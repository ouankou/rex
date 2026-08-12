#include "AstInterface_ROSE.h"
#include "AstUtilInterface.h"
#include "RoseAst.h"
#include "StmtInfoCollect.h"
#include "rose.h"

#include <algorithm>
#include <functional>
#include <iostream>
#include <vector>

namespace {

bool isDirectCallableIdentity(SgNode *node) {
  return isSgFunctionRefExp(node) != nullptr ||
         isSgTemplateFunctionRefExp(node) != nullptr ||
         isSgMemberFunctionRefExp(node) != nullptr ||
         isSgTemplateMemberFunctionRefExp(node) != nullptr ||
         isSgNonrealRefExp(node) != nullptr;
}

SgFunctionDefinition *findTestFunction(SgProject *project,
                                       const std::string &name) {
  RoseAst ast(project);
  for (RoseAst::iterator node = ast.begin(); node != ast.end(); ++node) {
    SgFunctionDefinition *definition = isSgFunctionDefinition(*node);
    if (definition != nullptr &&
        definition->get_declaration()->get_name().getString() == name) {
      return definition;
    }
  }
  return nullptr;
}

SgInitializedName *findVariable(SgNode *root, const std::string &name) {
  RoseAst ast(root);
  for (RoseAst::iterator node = ast.begin(); node != ast.end(); ++node) {
    SgInitializedName *variable = isSgInitializedName(*node);
    if (variable != nullptr && variable->get_name().getString() == name)
      return variable;
  }
  return nullptr;
}

bool contains(const std::vector<AstNodePtr> &nodes, const AstNodePtr &needle) {
  const SgNode *needleNode = AstNodePtrImpl(needle).get_ptr();
  return std::any_of(nodes.begin(), nodes.end(),
                     [needleNode](const AstNodePtr &node) {
                       return AstNodePtrImpl(node).get_ptr() == needleNode;
                     });
}

int fail(const char *message) {
  std::cerr << "rex_side_effect_callable_channels: " << message << std::endl;
  return 1;
}

} // namespace

int main(int argc, char *argv[]) {
  SgProject *project = frontend(argc, argv);
  ROSE_ASSERT(project != nullptr);

  SgFunctionDefinition *testFunction =
      findTestFunction(project, "rex_test_callable_channels");
  if (testFunction == nullptr) {
    return fail("test function definition is missing");
  }

  AstInterfaceImpl implementation(testFunction->get_body());
  AstInterface ast(&implementation);
  StmtSideEffectCollect collect(
      ast, nullptr,
      StmtSideEffectCollect::UnknownFunctionEffectPolicy::ReportOnly);

  std::vector<AstNodePtr> reads;
  std::vector<AstNodePtr> modifications;
  std::vector<AstNodePtr> calls;
  bool sawSyntheticRead = false;
  bool sawSyntheticModification = false;

  std::function<bool(AstNodePtr, AstNodePtr)> readCollect =
      [&reads, &sawSyntheticRead](AstNodePtr node, AstNodePtr) {
        if (node.is_unknown() || node.is_unknown_reference()) {
          sawSyntheticRead = true;
        } else {
          reads.push_back(node);
        }
        return true;
      };
  std::function<bool(AstNodePtr, AstNodePtr)> modificationCollect =
      [&modifications, &sawSyntheticModification](AstNodePtr node, AstNodePtr) {
        if (node.is_unknown() || node.is_unknown_reference()) {
          sawSyntheticModification = true;
        } else {
          modifications.push_back(node);
        }
        return true;
      };
  std::function<bool(AstNodePtr, AstNodePtr)> callCollect =
      [&calls](AstNodePtr callee, AstNodePtr) {
        calls.push_back(callee);
        return true;
      };
  collect.set_read_collect(readCollect);
  collect.set_modify_collect(modificationCollect);
  collect.set_call_collect(callCollect);

  const bool complete = collect(AstNodePtrImpl(testFunction->get_body()));
  if (complete) {
    return fail("missing interprocedural provider was reported as complete");
  }
  if (sawSyntheticRead || sawSyntheticModification) {
    return fail("ReportOnly emitted a synthetic unknown reference");
  }
  if (calls.size() != 2) {
    return fail("expected exactly one direct and one indirect call");
  }

  AstNodePtr directCallee;
  AstNodePtr indirectCallee;
  for (const AstNodePtr &callee : calls) {
    if (isDirectCallableIdentity(AstNodePtrImpl(callee).get_ptr())) {
      if (directCallee != AstNodePtr()) {
        return fail("multiple direct callable identities were collected");
      }
      directCallee = callee;
    } else {
      if (indirectCallee != AstNodePtr()) {
        return fail("multiple indirect callable identities were collected");
      }
      indirectCallee = callee;
    }
  }

  if (directCallee == AstNodePtr() || indirectCallee == AstNodePtr()) {
    return fail("call channel lost a direct or indirect callable identity");
  }
  if (contains(reads, directCallee)) {
    return fail("direct callable identity leaked into the object-read channel");
  }
  if (!contains(reads, indirectCallee)) {
    return fail(
        "indirect callable memory was absent from the object-read channel");
  }

  SgFunctionDefinition *pointerFunction =
      findTestFunction(project, "rex_test_pointer_alias_channel");
  if (pointerFunction == nullptr) {
    return fail("pointer-alias test function definition is missing");
  }
  SgInitializedName *pointer = findVariable(pointerFunction, "pointer");
  if (pointer == nullptr) {
    return fail("pointer declaration is missing");
  }
  bool sawPointerValueRead = false;
  bool aliasedPointerValueToPointee = false;
  std::function<bool(const AstNodePtr &, const AstNodePtr &,
                     AstUtilInterface::OperatorSideEffect)>
      collectComputedEffects =
          [pointer, &sawPointerValueRead, &aliasedPointerValueToPointee](
              const AstNodePtr &node, const AstNodePtr &,
              AstUtilInterface::OperatorSideEffect relation) {
            if (relation != AstUtilInterface::OperatorSideEffect::Read)
              return true;
            SgNode *sageNode = AstNodePtrImpl(node).get_ptr();
            SgVarRefExp *reference = isSgVarRefExp(sageNode);
            if ((reference != nullptr && reference->get_symbol() != nullptr &&
                 reference->get_symbol()->get_declaration() == pointer) ||
                sageNode == pointer)
              sawPointerValueRead = true;
            if (isSgAddressOfOp(sageNode) != nullptr)
              aliasedPointerValueToPointee = true;
            return true;
          };
  AstUtilInterface::ComputeAstSideEffects(pointerFunction,
                                          &collectComputedEffects,
                                          /*add_to_dep_analysis=*/nullptr);
  if (!sawPointerValueRead) {
    return fail("pointer-value read was replaced by its pointee alias");
  }
  if (aliasedPointerValueToPointee) {
    return fail("pointer-value read was reported as address-of pointee");
  }

  return 0;
}
