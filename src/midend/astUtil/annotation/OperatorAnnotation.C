
#include "CommandOptions.h"

#include "OperatorAnnotation.h"

#include "ROSE_ASSERT.h"

DebugLog DebugOperatorAnnotation("-debugopa");

OperatorSideEffectAnnotation *OperatorSideEffectAnnotation::inst = 0;
OperatorAliasAnnotation *OperatorAliasAnnotation::inst = 0;

void OperatorSideEffectAnnotation::release_inst() {
  delete inst;
  inst = nullptr;
}

void OperatorAliasAnnotation::release_inst() {
  delete inst;
  inst = nullptr;
}

static bool
AliasAnnotAnal(AstInterface &fa,
               OperatorAnnotCollection<OperatorAliasDescriptor> &aliasInfo,
               const AstNodePtr &fc, const AstNodePtr &result,
               CollectObject<std::pair<AstNodePtr, int>> &collectalias) {
  AstInterface::AstNodeList args;
  OperatorAliasDescriptor desc;
  if (!aliasInfo.known_operator(fa, fc, &args, &desc, false))
    return false;
  if (desc.get_param_decl().get_params().size() != args.size()) {
    std::cerr << "REX_ANNOTATION_INVARIANT[alias-arity]: parameters="
              << desc.get_param_decl().get_params().size()
              << " arguments=" << args.size() << std::endl;
    ROSE_ABORT();
  }
  ReplaceParams paramMap(fa, desc.get_param_decl(), args);
  if (result != AST_NULL)
    paramMap.add(fa, "result", result);
  int index = 0;
  for (OperatorAliasDescriptor::const_iterator p1 = desc.begin();
       p1 != desc.end(); ++p1, ++index) {
    const NameGroup &cur = *p1;
    for (NameGroup::const_iterator p2 = cur.begin(); p2 != cur.end(); ++p2) {
      std::string varname = *p2;
      AstNodePtr arg = paramMap.find(varname).CodeGen(fa);
      if (arg != AST_NULL) {
        collectalias(std::pair<AstNodePtr, int>(arg, index));
      } else {
        AstNodePtr var = fa.CreateVarRef(varname);
        collectalias(std::pair<AstNodePtr, int>(var, index));
      }
    }
  }
  return true;
}

bool OperatorAliasAnnotation::may_alias(
    AstInterface &fa, const AstNodePtr &fc, const AstNodePtr &result,
    CollectObject<std::pair<AstNodePtr, int>> &collectalias) {
  return AliasAnnotAnal(fa, aliasInfo, fc, result, collectalias);
}

bool OperatorAliasAnnotation::allow_alias(
    AstInterface &fa, const AstNodePtr &fc,
    CollectObject<std::pair<AstNodePtr, int>> &collectalias) {
  return AliasAnnotAnal(fa, allowaliasInfo, fc, AST_NULL, collectalias);
}

#define TEMPLATE_ONLY
#include "TypeAnnotation.C"
template class ReadAnnotCollection<OperatorDeclaration, '{', ';', '}'>;
template class TypeCollection<BoolDescriptor>;
template class TypeAnnotCollection<BoolDescriptor>;
template class TypeCollection<OperatorAliasDescriptor>;
template class TypeCollection<OperatorSideEffectDescriptor>;
template class OperatorAnnotCollection<OperatorSideEffectDescriptor>;
template class OperatorAnnotCollection<OperatorAliasDescriptor>;

#include "AnnotDescriptors.C"
template class WriteContainer<set<NameDescriptor>, ',', '(', ')'>;
template class WriteContainer<vector<NameDescriptor>, ',', '(', ')'>;
template class ReadContainer<SetDescriptor<NameDescriptor, ',', '{', '}'>,
                             NameDescriptor, ',', '{', '}'>;
template class ReadContainer<
    SetDescriptor<SymbolicValDescriptor, ',', '{', '}'>, SymbolicValDescriptor,
    ',', '{', '}'>;
template class WriteContainer<
    vector<CollectPair<TypeDescriptor, NameDescriptor, 0>>, ',', '(', ')'>;
template class WriteContainer<set<SymbolicValDescriptor>, ',', '{', '}'>;

template class ReadContainer<ParameterDeclaration,
                             CollectPair<TypeDescriptor, NameDescriptor, 0>,
                             ',', '(', ')'>;
