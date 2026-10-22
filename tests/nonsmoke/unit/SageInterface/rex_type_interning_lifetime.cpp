#include "rose.h"

#include <memory>
#include <string>
#include <vector>

namespace {
void requireMangledNameRetirement() {
  auto live = std::unique_ptr<SgIntVal>(SageBuilder::buildIntVal_nfi(1));
  SageInterface::addMangledNameToCache(live.get(), "rex_live_node");
  auto *retired = SageBuilder::buildIntVal_nfi(2);
  SageInterface::addMangledNameToCache(retired, "rex_retired_node");
  delete retired;
  auto replacement = std::unique_ptr<SgIntVal>(SageBuilder::buildIntVal_nfi(3));
  ROSE_ASSERT(replacement.get() == retired);
  ROSE_ASSERT(
      SageInterface::getMangledNameFromCache(replacement.get()).empty());
  ROSE_ASSERT(SageInterface::getMangledNameFromCache(live.get()) ==
              "rex_live_node");
}

void requireTemplateArgumentTypeRetirement() {
  const auto types = SgTemplateType::numberOfNodes();
  const auto arguments = SgTemplateArgument::numberOfNodes();
  auto *base = new SgTypeInt();
  auto *substitution = new SgTemplateType(SgName("Pack"));
  substitution->set_template_parameter_position(0);
  substitution->set_template_parameter_depth(0);
  substitution->set_is_pack_substitution(true);
  auto *argument = new SgTemplateArgument(base, false);
  argument->set_parent(substitution);
  substitution->get_tpl_args().push_back(argument);
  SageInterface::deleteAST(base,
                           SageInterface::DeleteAstMode::kRequireIsolated);
  ROSE_ASSERT(!SgNode::isLiveNode(base));
  ROSE_ASSERT(!SgNode::isLiveNode(substitution));
  ROSE_ASSERT(!SgNode::isLiveNode(argument));
  ROSE_ASSERT(SgTemplateType::numberOfNodes() == types);
  ROSE_ASSERT(SgTemplateArgument::numberOfNodes() == arguments);
}

void requireFunctionTypeRetirement() {
  auto base = std::make_unique<SgTypeInt>();
  auto *table = SgNode::get_globalFunctionTypeTable();
  const size_t initial = table->size();
  for (bool qualified : {false, true}) {
    auto arguments = std::make_unique<SgFunctionParameterTypeList>();
    arguments->append_argument(base.get());
    SgFunctionType *type =
        qualified
            ? SageBuilder::buildMemberFunctionType(
                  base.get(), arguments.get(), static_cast<SgType *>(nullptr),
                  SgMemberFunctionType::e_const)
            : SageBuilder::buildFunctionType(base.get(), arguments.get());
    if (!qualified)
      arguments.release();
    ROSE_ASSERT(table->size() == initial + 1);
    auto *copy = isSgFunctionType(SageInterface::deepCopy(type));
    ROSE_ASSERT(copy != nullptr && copy != type);
    ROSE_ASSERT(copy->get_return_type() == type->get_return_type());
    ROSE_ASSERT(copy->get_arguments() == type->get_arguments());
    ROSE_ASSERT(copy->get_argument_list() != type->get_argument_list());
    ROSE_ASSERT(copy->get_argument_list()->get_parent() == copy);
    if (qualified) {
      auto *member = isSgMemberFunctionType(copy);
      ROSE_ASSERT(member != nullptr && member->get_class_type() == nullptr);
      ROSE_ASSERT(member->get_mfunc_specifier() ==
                  SgMemberFunctionType::e_const);
    }
    delete copy;
    ROSE_ASSERT(table->size() == initial + 1);
    table->insert_function_type(type);
    ROSE_ASSERT(table->size() == initial + 1);
    delete type;
    ROSE_ASSERT(table->size() == initial);
    for (const auto &entry : table->get_types())
      ROSE_ASSERT(entry.second != type);
  }

  auto arguments = std::make_unique<SgFunctionParameterTypeList>();
  arguments->append_argument(base.get());
  auto type =
      std::make_unique<SgFunctionType>(base.get(), false, arguments.release());
  {
    auto local = std::make_unique<SgFunctionTypeTable>();
    local->insert_function_type(type.get());
    local->clear();
    ROSE_ASSERT(local->size() == 0);
    local->insert_function_type(type.get());
    local->remove_function_type(type.get());
    ROSE_ASSERT(local->size() == 0);
    local->insert_function_type(type.get());
  }
  ROSE_ASSERT(SgNode::isLiveNode(type.get()));
  {
    auto local = std::make_unique<SgFunctionTypeTable>();
    local->insert_function_type(type.get());
    type.reset();
    ROSE_ASSERT(local->size() == 0);
  }
  ROSE_ASSERT(table->size() == initial);
}

void requireDuplicateFunctionOperandRetirement() {
  const auto integers = SgIntVal::numberOfNodes();
  const auto noexcepts = SgNoexceptOp::numberOfNodes();
  auto candidate = [] {
    auto *operand = SageBuilder::buildIntVal_nfi(987654321);
    SageBuilder::initializeSemanticExpressionSourceProvenance(operand);
    auto *expression = SageBuilder::buildNoexceptOp_nfi(operand);
    SageBuilder::initializeSemanticExpressionSourceProvenance(expression);
    return std::make_unique<SgFunctionType>(
        SageBuilder::buildIntType(), false,
        SageBuilder::buildFunctionParameterTypeList(),
        SgFunctionType::e_exception_noexcept_true, expression, SgTypePtrList{},
        std::vector<bool>{}, nullptr, nullptr, false, false, nullptr);
  };
  auto *canonical = SageBuilder::internFunctionType(candidate());
  ROSE_ASSERT(SageBuilder::internFunctionType(candidate()) == canonical);
  ROSE_ASSERT(SgIntVal::numberOfNodes() == integers + 1);
  ROSE_ASSERT(SgNoexceptOp::numberOfNodes() == noexcepts + 1);
  SageInterface::deleteAST(canonical,
                           SageInterface::DeleteAstMode::kRequireIsolated);
  ROSE_ASSERT(SgIntVal::numberOfNodes() == integers);
  ROSE_ASSERT(SgNoexceptOp::numberOfNodes() == noexcepts);
}

template <class Type, class Factory, class Cache>
void requireNativeRetirement(Factory factory, Cache cache) {
  auto base = std::make_unique<SgTypeInt>();
  const auto modifier_buckets = SgModifierNodes::numberOfNodes();
  const auto types = Type::numberOfNodes();
  const auto indices = SgFunctionTypeSymbol::numberOfNodes();
  Type *canonical = factory(base.get());
  ROSE_ASSERT(cache(base.get()) == canonical);
  ROSE_ASSERT(canonical->get_parent() == nullptr);
  ROSE_ASSERT(SgFunctionTypeSymbol::numberOfNodes() == indices);
  {
    auto lexical = std::make_unique<Type>(base.get());
  }
  ROSE_ASSERT(cache(base.get()) == canonical);
  delete canonical;
  ROSE_ASSERT(cache(base.get()) == nullptr);
  ROSE_ASSERT(SgModifierNodes::numberOfNodes() == modifier_buckets);
  ROSE_ASSERT(Type::numberOfNodes() == types);
  ROSE_ASSERT(SgFunctionTypeSymbol::numberOfNodes() == indices);
  {
    std::unique_ptr<Type> replacement(factory(base.get()));
    ROSE_ASSERT(cache(base.get()) == replacement.get());
  }
  ROSE_ASSERT(cache(base.get()) == nullptr);
  ROSE_ASSERT(SgModifierNodes::numberOfNodes() == modifier_buckets);
  ROSE_ASSERT(Type::numberOfNodes() == types);
  std::unique_ptr<Type> last(factory(base.get()));
  base.reset();
  last.reset();
}

template <class Type, class Factory> void requireInterning(Factory factory) {
  Type *canonical = factory();
  ROSE_ASSERT(canonical != nullptr);
  const size_t types = Type::numberOfNodes();
  const size_t selectors = SgIntVal::numberOfNodes();
  const size_t positions = Sg_File_Info::numberOfNodes();
  for (int iteration = 0; iteration != 1000; ++iteration) {
    ROSE_ASSERT(factory() == canonical);
    ROSE_ASSERT(Type::numberOfNodes() == types);
    ROSE_ASSERT(SgIntVal::numberOfNodes() == selectors);
    ROSE_ASSERT(Sg_File_Info::numberOfNodes() == positions);
  }
}

template <class Type, class Factory>
void requireKindInterning(Factory factory) {
  Type *first = factory(SageBuilder::buildIntVal(4));
  Type *second = factory(SageBuilder::buildIntVal(8));
  ROSE_ASSERT(first != nullptr && second != nullptr && first != second);
  ROSE_ASSERT(first->get_type_kind()->get_parent() == first);
  ROSE_ASSERT(second->get_type_kind()->get_parent() == second);
  const size_t types = Type::numberOfNodes();
  const size_t integers = SgTypeInt::numberOfNodes();
  const size_t selectors = SgIntVal::numberOfNodes();
  const size_t positions = Sg_File_Info::numberOfNodes();
  for (int iteration = 0; iteration != 1000; ++iteration) {
    Type *canonical = iteration % 2 == 0 ? first : second;
    SgExpression *selector = canonical->get_type_kind();
    ROSE_ASSERT(factory(SageBuilder::buildIntVal(iteration % 2 == 0 ? 4 : 8)) ==
                canonical);
    ROSE_ASSERT(canonical->get_type_kind() == selector);
    ROSE_ASSERT(selector->get_parent() == canonical);
    ROSE_ASSERT(Type::numberOfNodes() == types);
    ROSE_ASSERT(SgTypeInt::numberOfNodes() == integers);
    ROSE_ASSERT(SgIntVal::numberOfNodes() == selectors);
    ROSE_ASSERT(Sg_File_Info::numberOfNodes() == positions);
  }
}
void requireAliasProjectionRetirement(const char *executable,
                                      const char *source) {
  std::vector<std::string> arguments{executable, "-std=gnu++14", "-c", source};
  std::unique_ptr<SgProject> project(frontend(arguments));
  ROSE_ASSERT(project != nullptr && project->get_frontendErrorCode() == 0);
  SgNonrealDecl *aliases[2]{};
  VariantVector variants(V_SgNonrealDecl);
  for (SgNode *node : NodeQuery::queryMemoryPool(variants)) {
    auto *declaration = isSgNonrealDecl(node);
    auto *type = declaration->get_type();
    if (SageInterface::getProject(declaration) != project.get() ||
        type == nullptr)
      continue;
    SgType *projection = type->get_aliased_type();
    if (isSgDeclType(projection) != nullptr)
      aliases[0] = declaration;
    if (auto *pointer = isSgPointerType(projection))
      if (isSgDeclType(pointer->get_base_type()) != nullptr)
        aliases[1] = declaration;
    if (aliases[0] != nullptr && aliases[1] != nullptr)
      break;
  }
  ROSE_ASSERT(aliases[0] != nullptr && aliases[1] != nullptr);
  const auto decltypes = SgDeclType::numberOfNodes();
  const auto typeofs = SgTypeOfType::numberOfNodes();
  const auto pointers = SgPointerType::numberOfNodes();
  const auto references = SgNonrealRefExp::numberOfNodes();
  const auto file_infos = Sg_File_Info::numberOfNodes();
  for (SgNonrealDecl *alias : aliases) {
    auto *projection = alias->get_type()->get_aliased_type();
    auto *pointer = isSgPointerType(projection);
    SgType *original_occurrence =
        pointer != nullptr ? pointer->get_base_type() : projection;
    for (int iteration = 0; iteration < 4; ++iteration) {
      auto *copy = isSgNonrealDecl(SageInterface::deepCopy(alias));
      ROSE_ASSERT(copy != nullptr && copy != alias);
      ROSE_ASSERT(copy->get_parent() == nullptr);
      SgType *copied_projection = copy->get_type()->get_aliased_type();
      ROSE_ASSERT(copied_projection != projection);
      if (auto *pointer = isSgPointerType(projection)) {
        auto *copied_pointer = isSgPointerType(copied_projection);
        ROSE_ASSERT(copied_pointer != nullptr);
        ROSE_ASSERT(isSgDeclType(copied_pointer->get_base_type()) != nullptr);
        ROSE_ASSERT(copied_pointer->get_base_type() !=
                    pointer->get_base_type());
      } else {
        ROSE_ASSERT(isSgDeclType(copied_projection) != nullptr);
      }
      SageInterface::deleteAST(copy,
                               SageInterface::DeleteAstMode::kRequireIsolated);
      ROSE_ASSERT(SgDeclType::numberOfNodes() == decltypes);
      ROSE_ASSERT(SgTypeOfType::numberOfNodes() == typeofs);
      ROSE_ASSERT(SgPointerType::numberOfNodes() == pointers);
      ROSE_ASSERT(SgNonrealRefExp::numberOfNodes() == references);
      ROSE_ASSERT(Sg_File_Info::numberOfNodes() == file_infos);
      ROSE_ASSERT(SgNode::isLiveNode(alias));
      ROSE_ASSERT(SgNode::isLiveNode(projection));
      ROSE_ASSERT(SgNode::isLiveNode(original_occurrence));
      ROSE_ASSERT(alias->get_type()->get_aliased_type() == projection);
    }
  }
}
} // namespace

int main(int argc, char **argv) {
  ROSE_ASSERT(argc == 2);
  requireMangledNameRetirement();
  requireTemplateArgumentTypeRetirement();
  requireNativeRetirement<SgUnaryTransformType>(
      [](SgType *base) {
        return SgUnaryTransformType::createType(
            base, SgUnaryTransformType::e_add_pointer);
      },
      [](SgType *base) {
        return SgUnaryTransformType::findCanonicalSemanticType(
            base, SgUnaryTransformType::e_add_pointer);
      });
  requireFunctionTypeRetirement();
  requireDuplicateFunctionOperandRetirement();
  requireNativeRetirement<SgPointerType>(
      [](SgType *base) { return SgPointerType::createType(base); },
      [](SgType *base) { return base->get_ptr_to(); });
  requireNativeRetirement<SgReferenceType>(
      [](SgType *base) { return SgReferenceType::createType(base); },
      [](SgType *base) { return base->get_ref_to(); });
  requireNativeRetirement<SgRvalueReferenceType>(
      [](SgType *base) { return SgRvalueReferenceType::createType(base); },
      [](SgType *base) { return base->get_rvalue_ref_to(); });
  requireNativeRetirement<SgModifierType>(
      [](SgType *base) {
        auto *candidate = new SgModifierType(base);
        candidate->get_typeModifier().setAtomic();
        return SgModifierType::insertModifierTypeIntoTypeTable(candidate);
      },
      [](SgType *base) {
        auto *bucket = base->get_modifiers();
        return bucket != nullptr ? bucket->get_nodes().front() : nullptr;
      });
  requireInterning<SgTypeVoid>([] { return SageBuilder::buildVoidType(); });
  requireInterning<SgTypeBool>([] { return SageBuilder::buildBoolType(); });
  requireInterning<SgTypeChar>([] { return SageBuilder::buildCharType(); });
  requireInterning<SgTypeShort>([] { return SageBuilder::buildShortType(); });
  requireInterning<SgTypeInt>([] { return SageBuilder::buildIntType(); });
  requireInterning<SgTypeUnsignedInt>(
      [] { return SageBuilder::buildUnsignedIntType(); });
  requireInterning<SgTypeLong>([] { return SageBuilder::buildLongType(); });
  requireInterning<SgTypeFloat>([] { return SageBuilder::buildFloatType(); });
  requireInterning<SgTypeDouble>([] { return SageBuilder::buildDoubleType(); });
  requireInterning<SgTypeLongDouble>(
      [] { return SageBuilder::buildLongDoubleType(); });
  requireInterning<SgTypeTargetBuiltin>([] {
    return SageBuilder::buildTargetBuiltinType(
        SgName("image2d_t"), SgTypeTargetBuiltin::e_target_builtin_opencl);
  });
  requireInterning<SgModifierType>([] {
    auto *candidate = new SgModifierType(SageBuilder::buildIntType());
    candidate->get_typeModifier().get_constVolatileModifier().setConst();
    return SgModifierType::insertModifierTypeIntoTypeTable(candidate);
  });
  requireKindInterning<SgTypeBool>(
      [](SgExpression *kind) { return SageBuilder::buildBoolType(kind); });
  requireKindInterning<SgTypeInt>(
      [](SgExpression *kind) { return SageBuilder::buildIntType(kind); });
  requireKindInterning<SgTypeUnsignedInt>([](SgExpression *kind) {
    return SageBuilder::buildUnsignedIntType(kind);
  });
  requireKindInterning<SgTypeFloat>(
      [](SgExpression *kind) { return SageBuilder::buildFloatType(kind); });
  for (int length : {0, 12}) {
    requireInterning<SgTypeString>([=] {
      return SgTypeString::createType(SageBuilder::buildIntVal(length),
                                      nullptr);
    });
  }
  requireKindInterning<SgTypeString>([](SgExpression *kind) {
    return SgTypeString::createType(SageBuilder::buildIntVal(12), kind);
  });
  requireAliasProjectionRetirement(argv[0], argv[1]);
  return 0;
}
