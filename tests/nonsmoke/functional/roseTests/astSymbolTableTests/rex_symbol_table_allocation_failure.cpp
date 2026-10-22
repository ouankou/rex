#include "PtrMap.h"
#include "sageAstJsonPrivate.h"

#include <cstddef>
#include <cstdlib>
#include <new>
#include <string>

namespace {
thread_local std::ptrdiff_t allocation_failure_countdown = -1;
#ifdef REX_INJECT_ALLOCATION_FAILURE
constexpr bool inject_allocation_failures = true;
#else
constexpr bool inject_allocation_failures = false;
#endif
} // namespace

#ifdef REX_INJECT_ALLOCATION_FAILURE
void *operator new(std::size_t size) {
  if (allocation_failure_countdown == 0) {
    allocation_failure_countdown = -1;
    throw std::bad_alloc();
  }
  if (allocation_failure_countdown > 0)
    --allocation_failure_countdown;
  if (void *storage = std::malloc(size == 0 ? 1 : size))
    return storage;
  throw std::bad_alloc();
}

void *operator new[](std::size_t size) { return ::operator new(size); }
void operator delete(void *storage) noexcept { std::free(storage); }
void operator delete[](void *storage) noexcept { ::operator delete(storage); }
void operator delete(void *storage, std::size_t) noexcept {
  ::operator delete(storage);
}
void operator delete[](void *storage, std::size_t) noexcept {
  ::operator delete(storage);
}
#endif

namespace {
void requirePointerMapLookupAllocationSafety() {
  int key = 0, missing = 1, value = 2;
  PtrMapWrap<int, int> mapping;
  mapping.InsertMapping(&key, &value);
  const auto &lookup = mapping;
  allocation_failure_countdown = 0;
  ROSE_ASSERT(lookup.Map(&key) == &value);
  ROSE_ASSERT(lookup.Map(&missing) == nullptr);
  mapping.RemoveMapping(&missing);
  mapping.RemoveMapping(&key);
  ROSE_ASSERT(lookup.Map(&key) == nullptr);
  ROSE_ASSERT(mapping.GetIterator().ReachEnd());
  ROSE_ASSERT(allocation_failure_countdown == 0);
  allocation_failure_countdown = -1;
  mapping.InsertMapping(&key, &value);
  auto entries = mapping.GetIterator();
  ROSE_ASSERT(entries.Current() == &key && entries.Current2() == &value);
  ++entries;
  ROSE_ASSERT(entries.ReachEnd());
  mapping.InsertMapping(&key, nullptr);
  ROSE_ASSERT(mapping.GetIterator().ReachEnd());
}

void requireArgumentMetadataCopyAllocationSafety() {
  SgTypeInt integer;
  SgTypeFloat floating;
  SgFunctionParameterTypeList source;
  source.append_argument(&floating);
  auto *original = source.get_argument_qualification_use_sites().front();
  original->set_is_pack_expansion(true);
  original->set_source_type_qualification_present(true);
  original->set_source_type_global_qualification(true);
  original->set_source_type_elaboration_required(true);
  original->get_source_type_qualification_tokens() = {std::string(128, 'q')};
  SgFunctionParameterTypeList target;
  target.append_argument(&integer);
  auto *existing = target.get_argument_qualification_use_sites().front();
  const auto positions_before = SgFunctionTypeArgument::numberOfNodes();
  bool completed = false;
  for (int boundary = 0; boundary != 32 && !completed; ++boundary) {
    allocation_failure_countdown = boundary;
    try {
      target.append_argument(&floating, *original);
      completed = true;
    } catch (const std::bad_alloc &) {
    }
    allocation_failure_countdown = -1;
    target.validate_argument_qualification_use_sites("metadata-copy");
    ROSE_ASSERT(target.get_argument_qualification_use_sites().front() ==
                existing);
    if (!completed) {
      ROSE_ASSERT(target.get_arguments().size() == 1);
      ROSE_ASSERT(SgFunctionTypeArgument::numberOfNodes() == positions_before);
    }
  }
  ROSE_ASSERT(completed && target.get_arguments().size() == 2);
  auto *copy = target.get_argument_qualification_use_sites().back();
  ROSE_ASSERT(copy != original && copy->get_parent() == &target);
  ROSE_ASSERT(copy->get_is_pack_expansion());
  ROSE_ASSERT(copy->get_source_type_qualification_present());
  ROSE_ASSERT(copy->get_source_type_global_qualification());
  ROSE_ASSERT(copy->get_source_type_elaboration_required());
  ROSE_ASSERT(copy->get_source_type_qualification_tokens() ==
              original->get_source_type_qualification_tokens());
  ROSE_ASSERT(original->get_parent() == &source);
}

void requireParameterTypeListBuilderAllocationSafety() {
  SgTypeInt integer;
  const auto lists_before = SgFunctionParameterTypeList::numberOfNodes();
  const auto positions_before = SgFunctionTypeArgument::numberOfNodes();
  std::unique_ptr<SgFunctionParameterTypeList> parameters;
  for (int boundary = 0; boundary != 32 && parameters == nullptr; ++boundary) {
    allocation_failure_countdown = boundary;
    try {
      parameters.reset(SageBuilder::buildFunctionParameterTypeList(&integer));
    } catch (const std::bad_alloc &) {
    }
    allocation_failure_countdown = -1;
    if (parameters == nullptr) {
      ROSE_ASSERT(SgFunctionParameterTypeList::numberOfNodes() == lists_before);
      ROSE_ASSERT(SgFunctionTypeArgument::numberOfNodes() == positions_before);
    }
  }
  ROSE_ASSERT(parameters != nullptr && parameters->get_parent() == nullptr);
  parameters->validate_argument_qualification_use_sites("list-builder");
  parameters.reset();
  ROSE_ASSERT(SgFunctionParameterTypeList::numberOfNodes() == lists_before);
  ROSE_ASSERT(SgFunctionTypeArgument::numberOfNodes() == positions_before);
}

void requireFunctionTypeBuilderAllocationSafety() {
  SgTypeInt integer;
  SgTypeFloat floating;
  auto *table = SgNode::get_globalFunctionTypeTable();
  for (bool member : {false, true}) {
    auto parameters = std::make_unique<SgFunctionParameterTypeList>();
    parameters->append_argument(&integer);
    auto *position = parameters->get_argument_qualification_use_sites().front();
    const auto functions_before = SgFunctionType::numberOfNodes();
    const auto members_before = SgMemberFunctionType::numberOfNodes();
    const auto lists_before = SgFunctionParameterTypeList::numberOfNodes();
    const auto positions_before = SgFunctionTypeArgument::numberOfNodes();
    const auto entries_before = table->size();
    auto build = [&]() -> SgFunctionType * {
      return member
                 ? SageBuilder::buildMemberFunctionType(
                       &floating, parameters.get(),
                       static_cast<SgType *>(nullptr),
                       SgMemberFunctionType::e_const)
                 : SageBuilder::buildFunctionType(&floating, parameters.get());
    };
    SgFunctionType *function = nullptr;
    for (int boundary = 0; boundary != 128 && function == nullptr; ++boundary) {
      allocation_failure_countdown = boundary;
      try {
        function = build();
      } catch (const std::bad_alloc &) {
      }
      allocation_failure_countdown = -1;
      ROSE_ASSERT(SgNode::isLiveNode(parameters.get()));
      parameters->validate_argument_qualification_use_sites("builder-failure");
      ROSE_ASSERT(parameters->get_argument_qualification_use_sites().front() ==
                  position);
      if (function == nullptr) {
        ROSE_ASSERT(parameters->get_parent() == nullptr);
        ROSE_ASSERT(SgFunctionType::numberOfNodes() == functions_before);
        ROSE_ASSERT(SgMemberFunctionType::numberOfNodes() == members_before);
        ROSE_ASSERT(SgFunctionParameterTypeList::numberOfNodes() ==
                    lists_before);
        ROSE_ASSERT(SgFunctionTypeArgument::numberOfNodes() ==
                    positions_before);
        ROSE_ASSERT(table->size() == entries_before);
      }
    }
    ROSE_ASSERT(function != nullptr && build() == function);
    ROSE_ASSERT(function->get_argument_list()->get_parent() == function);
    ROSE_ASSERT(table->size() == entries_before + 1);
    if (member) {
      ROSE_ASSERT(parameters->get_parent() == nullptr);
      ROSE_ASSERT(function->get_argument_list() != parameters.get());
    } else {
      ROSE_ASSERT(function->get_argument_list() == parameters.get());
      parameters.release();
    }
    allocation_failure_countdown = 0;
    delete function;
    allocation_failure_countdown = -1;
    ROSE_ASSERT(SgFunctionType::numberOfNodes() == functions_before);
    ROSE_ASSERT(SgMemberFunctionType::numberOfNodes() == members_before);
    ROSE_ASSERT(table->size() == entries_before);
  }
}

void requireParameterTypeListInsertionSafety() {
  SgTypeInt integer;
  SgTypeFloat floating;
  SgFunctionParameterTypeList parameters;
  parameters.append_argument(&integer);
  auto *original = parameters.get_argument_qualification_use_sites().front();
  const auto positions_before = SgFunctionTypeArgument::numberOfNodes();
  bool completed = false;
  for (int boundary = 0; boundary != 32 && !completed; ++boundary) {
    allocation_failure_countdown = boundary;
    try {
      parameters.insert_argument(0, &floating);
      completed = true;
    } catch (const std::bad_alloc &) {
    }
    allocation_failure_countdown = -1;
    parameters.validate_argument_qualification_use_sites("allocation-failure");
    const auto &arguments = parameters.get_arguments();
    const auto &positions = parameters.get_argument_qualification_use_sites();
    if (completed) {
      ROSE_ASSERT(arguments.size() == 2 && arguments.front() == &floating);
      ROSE_ASSERT(arguments.back() == &integer && positions.back() == original);
      ROSE_ASSERT(SgFunctionTypeArgument::numberOfNodes() ==
                  positions_before + 1);
    } else {
      ROSE_ASSERT(arguments.size() == 1 && arguments.front() == &integer);
      ROSE_ASSERT(positions.size() == 1 && positions.front() == original);
      ROSE_ASSERT(SgFunctionTypeArgument::numberOfNodes() == positions_before);
    }
  }
  ROSE_ASSERT(completed);
}

template <typename Type, typename Factory>
Type *requireTypeFactoryAllocationSafety(Factory factory) {
  auto *symbols = SgNode::get_globalTypeTable()->get_type_table();
  const auto entries_before = symbols->get_table()->size();
  const auto symbols_before = SgFunctionTypeSymbol::numberOfNodes();
  const auto types_before = Type::numberOfNodes();
  Type *type = nullptr;
  for (int boundary = 0; boundary != 128 && type == nullptr; ++boundary) {
    allocation_failure_countdown = boundary;
    try {
      type = factory();
    } catch (const std::bad_alloc &) {
    }
    allocation_failure_countdown = -1;
    if (type != nullptr) {
      ROSE_ASSERT(Type::numberOfNodes() == types_before + 1);
      ROSE_ASSERT(factory() == type);
      ROSE_ASSERT(Type::numberOfNodes() == types_before + 1);
    } else {
      ROSE_ASSERT(Type::numberOfNodes() == types_before);
      ROSE_ASSERT(SgFunctionTypeSymbol::numberOfNodes() == symbols_before);
      ROSE_ASSERT(symbols->get_table()->size() == entries_before);
    }
    symbols->validate_exact_symbol_indexes();
  }
  ROSE_ASSERT(type != nullptr);
  return type;
}

template <typename Type, typename Factory>
void requireKindFactoryAllocationSafety(Factory factory) {
  auto kind = std::make_unique<SgIntVal>(8, "");
  auto *symbols = SgNode::get_globalTypeTable()->get_type_table();
  const auto entries_before = symbols->get_table()->size();
  const auto symbols_before = SgFunctionTypeSymbol::numberOfNodes();
  const auto types_before = Type::numberOfNodes();
  const auto selectors_before = SgIntVal::numberOfNodes();
  Type *type = nullptr;
  for (int boundary = 0; boundary != 128 && type == nullptr; ++boundary) {
    allocation_failure_countdown = boundary;
    try {
      type = factory(kind.get());
    } catch (const std::bad_alloc &) {
    }
    allocation_failure_countdown = -1;
    ROSE_ASSERT(SgNode::isLiveNode(kind.get()));
    if (type == nullptr) {
      ROSE_ASSERT(kind->get_parent() == nullptr);
      ROSE_ASSERT(Type::numberOfNodes() == types_before);
      ROSE_ASSERT(SgFunctionTypeSymbol::numberOfNodes() == symbols_before);
      ROSE_ASSERT(symbols->get_table()->size() == entries_before);
    }
    symbols->validate_exact_symbol_indexes();
  }
  ROSE_ASSERT(type != nullptr && type->get_type_kind() == kind.get());
  ROSE_ASSERT(kind->get_parent() == type);
  kind.release();
  ROSE_ASSERT(Type::numberOfNodes() == types_before + 1);
  ROSE_ASSERT(SgFunctionTypeSymbol::numberOfNodes() == symbols_before + 1);
  ROSE_ASSERT(symbols->get_table()->size() == entries_before + 1);
  ROSE_ASSERT(factory(new SgIntVal(8, "")) == type);
  ROSE_ASSERT(Type::numberOfNodes() == types_before + 1);
  ROSE_ASSERT(SgIntVal::numberOfNodes() == selectors_before);
  ROSE_ASSERT(type->get_type_kind()->get_parent() == type);
}

void requireStringFactoryAllocationSafety() {
  auto *symbols = SgNode::get_globalTypeTable()->get_type_table();
  SgTypeString *canonical = nullptr;
  for (bool cached : {false, true}) {
    auto length = std::unique_ptr<SgAddOp>(SageBuilder::buildAddOp(
        SageBuilder::buildIntVal(37), SageBuilder::buildIntVal(5),
        SageBuilder::buildIntType()));
    auto kind = std::make_unique<SgIntVal>(8, "");
    auto *left = length->get_lhs_operand_i();
    auto *right = length->get_rhs_operand_i();
    const auto entries_before = symbols->get_table()->size();
    const auto symbols_before = SgFunctionTypeSymbol::numberOfNodes();
    const auto types_before = SgTypeString::numberOfNodes();
    const auto integers_before = SgIntVal::numberOfNodes();
    const auto additions_before = SgAddOp::numberOfNodes();
    const auto positions_before = Sg_File_Info::numberOfNodes();
    SgTypeString *type = nullptr;
    for (int boundary = 0; boundary != 256 && type == nullptr; ++boundary) {
      allocation_failure_countdown = boundary;
      try {
        type = SgTypeString::createType(length.get(), kind.get());
      } catch (const std::bad_alloc &) {
      }
      allocation_failure_countdown = -1;
      if (type == nullptr) {
        ROSE_ASSERT(SgNode::isLiveNode(length.get()));
        ROSE_ASSERT(SgNode::isLiveNode(kind.get()));
        ROSE_ASSERT(length->get_parent() == nullptr);
        ROSE_ASSERT(kind->get_parent() == nullptr);
        ROSE_ASSERT(left->get_parent() == length.get());
        ROSE_ASSERT(right->get_parent() == length.get());
        ROSE_ASSERT(SgTypeString::numberOfNodes() == types_before);
        ROSE_ASSERT(SgFunctionTypeSymbol::numberOfNodes() == symbols_before);
        ROSE_ASSERT(symbols->get_table()->size() == entries_before);
        ROSE_ASSERT(SgIntVal::numberOfNodes() == integers_before);
        ROSE_ASSERT(SgAddOp::numberOfNodes() == additions_before);
        ROSE_ASSERT(Sg_File_Info::numberOfNodes() == positions_before);
      }
      symbols->validate_exact_symbol_indexes();
    }
    ROSE_ASSERT(type != nullptr);
    ROSE_ASSERT(SgNode::get_globalTypeTable()->lookup_type(
                    type->get_mangled()) == type);
    if (cached) {
      ROSE_ASSERT(type == canonical);
      ROSE_ASSERT(SgTypeString::numberOfNodes() == types_before);
      ROSE_ASSERT(SgIntVal::numberOfNodes() == integers_before - 3);
      ROSE_ASSERT(SgAddOp::numberOfNodes() == additions_before - 1);
      ROSE_ASSERT(SgFunctionTypeSymbol::numberOfNodes() == symbols_before);
      ROSE_ASSERT(symbols->get_table()->size() == entries_before);
    } else {
      canonical = type;
      ROSE_ASSERT(type->get_lengthExpression() == length.get());
      ROSE_ASSERT(type->get_type_kind() == kind.get());
      ROSE_ASSERT(SgTypeString::numberOfNodes() == types_before + 1);
    }
    length.release();
    kind.release();
    ROSE_ASSERT(type->get_lengthExpression()->get_parent() == type);
    ROSE_ASSERT(type->get_type_kind()->get_parent() == type);
  }
}

void requireStringReaderAllocationSafety() {
  using namespace Rose::AstJson;
  auto *canonical = SgTypeString::createType(SageBuilder::buildIntVal(61),
                                             SageBuilder::buildIntVal(8));
  const auto json = parseJson(rawTypeJson(canonical, {}));
  const auto key = canonical->get_mangled();
  auto *symbols = SgNode::get_globalTypeTable()->get_type_table();
  const auto entries_before = symbols->get_table()->size();
  const auto symbols_before = SgFunctionTypeSymbol::numberOfNodes();
  const auto types_before = SgTypeString::numberOfNodes();
  const auto selectors_before = SgIntVal::numberOfNodes();
  const auto positions_before = Sg_File_Info::numberOfNodes();
  SgType *type = nullptr;
  for (int boundary = 0; boundary != 1024 && type == nullptr; ++boundary) {
    allocation_failure_countdown = boundary;
    try {
      type = typeFromJson(json, {});
    } catch (const std::bad_alloc &) {
    }
    allocation_failure_countdown = -1;
    ROSE_ASSERT(SgTypeString::numberOfNodes() == types_before);
    ROSE_ASSERT(SgIntVal::numberOfNodes() == selectors_before);
    ROSE_ASSERT(Sg_File_Info::numberOfNodes() == positions_before);
    ROSE_ASSERT(SgFunctionTypeSymbol::numberOfNodes() == symbols_before);
    ROSE_ASSERT(symbols->get_table()->size() == entries_before);
    ROSE_ASSERT(SgNode::get_globalTypeTable()->lookup_type(key) == canonical);
    ROSE_ASSERT(canonical->get_lengthExpression()->get_parent() == canonical);
    ROSE_ASSERT(canonical->get_type_kind()->get_parent() == canonical);
    symbols->validate_exact_symbol_indexes();
  }
  ROSE_ASSERT(type == canonical);
}

void requireStringBindingIdentity() {
  auto *first_file =
      SageBuilder::buildGeneratedSourceFile("rex_string_binding_first.cpp");
  auto *second_file =
      SageBuilder::buildGeneratedSourceFile("rex_string_binding_second.cpp");
  auto build_scope = [](SgGlobal *global) {
    auto *function = SageBuilder::buildDefiningFunctionDeclaration(
        SageBuilder::function_declaration_ownership::sourceLexical(),
        "rex_character_binding", SageBuilder::buildVoidType(),
        SageBuilder::buildFunctionParameterList(), global);
    ROSE_ASSERT(function->get_parent() == global &&
                function->get_scope() == global);
    ROSE_ASSERT(global->get_declarations().size() == 1 &&
                global->get_declarations().front() == function);
    return function->get_definition()->get_body();
  };
  auto *first_scope = build_scope(first_file->get_globalScope());
  auto *second_scope = build_scope(second_file->get_globalScope());
  auto *first = SageBuilder::buildVariableDeclaration(
      "rex_character_length", SageBuilder::buildIntType(), nullptr,
      first_scope);
  auto *second = SageBuilder::buildVariableDeclaration(
      "rex_character_length", SageBuilder::buildIntType(), nullptr,
      second_scope);
  first_scope->append_statement(first);
  second_scope->append_statement(second);
  auto *first_name = first->get_variables().front();
  auto *second_name = second->get_variables().front();
  auto *first_type = SgTypeString::createType(
      SageBuilder::buildVarRefExp(first_name), nullptr);
  auto *second_type = SgTypeString::createType(
      SageBuilder::buildVarRefExp(second_name), nullptr);
  ROSE_ASSERT(first_type != second_type);
  ROSE_ASSERT(first_type->get_mangled() == second_type->get_mangled());
  for (auto *type : {first_type, second_type}) {
    auto *reference = isSgVarRefExp(type->get_lengthExpression());
    auto *name = type == first_type ? first_name : second_name;
    ROSE_ASSERT(reference != nullptr && reference->get_parent() == type);
    ROSE_ASSERT(reference->get_symbol()->get_declaration() == name);
    ROSE_ASSERT(SgTypeString::createType(SageBuilder::buildVarRefExp(name),
                                         nullptr) == type);
    auto *symbols = SgNode::get_globalTypeTable()->get_type_table();
    const auto entries_before = symbols->get_table()->size();
    const auto symbols_before = SgFunctionTypeSymbol::numberOfNodes();
    auto *pointer = SgPointerType::createType(type);
    auto *lvalue = SgReferenceType::createType(type);
    auto *rvalue = SgRvalueReferenceType::createType(type);
    ROSE_ASSERT(pointer->get_base_type() == type);
    ROSE_ASSERT(lvalue->get_base_type() == type);
    ROSE_ASSERT(rvalue->get_base_type() == type);
    ROSE_ASSERT(SgPointerType::createType(type) == pointer);
    ROSE_ASSERT(SgReferenceType::createType(type) == lvalue);
    ROSE_ASSERT(SgRvalueReferenceType::createType(type) == rvalue);
    ROSE_ASSERT(type->get_ptr_to() == pointer);
    ROSE_ASSERT(type->get_ref_to() == lvalue);
    ROSE_ASSERT(type->get_rvalue_ref_to() == rvalue);
    for (SgType *derived :
         {static_cast<SgType *>(pointer), static_cast<SgType *>(lvalue),
          static_cast<SgType *>(rvalue)}) {
      ROSE_ASSERT(derived->get_parent() == nullptr);
    }
    ROSE_ASSERT(symbols->get_table()->size() == entries_before);
    ROSE_ASSERT(SgFunctionTypeSymbol::numberOfNodes() == symbols_before);
  }
  SageInterface::deleteAST({first_type, second_type, first_file->get_project(),
                            second_file->get_project()},
                           SageInterface::DeleteAstMode::kRequireIsolated);
}

void requireSemanticTypeFactoryAllocationSafety() {
  auto *symbols = SgNode::get_globalTypeTable()->get_type_table();
  const auto entries_before = symbols->get_table()->size();
  const auto symbols_before = SgFunctionTypeSymbol::numberOfNodes();
  const auto integers_before = SgTypeInt::numberOfNodes();
  auto operand = std::make_unique<SgTypeInt>();
  requireTypeFactoryAllocationSafety<SgUnaryTransformType>([&] {
    return SgUnaryTransformType::createType(
        operand.get(), SgUnaryTransformType::e_add_pointer);
  });
  requireTypeFactoryAllocationSafety<SgPointerType>(
      [&] { return SgPointerType::createType(operand.get()); });
  ROSE_ASSERT(operand->get_ptr_to() != nullptr);
  requireTypeFactoryAllocationSafety<SgReferenceType>(
      [&] { return SgReferenceType::createType(operand.get()); });
  ROSE_ASSERT(operand->get_ref_to() != nullptr);
  requireTypeFactoryAllocationSafety<SgRvalueReferenceType>(
      [&] { return SgRvalueReferenceType::createType(operand.get()); });
  ROSE_ASSERT(operand->get_rvalue_ref_to() != nullptr);
  SgTypeModifier atomic;
  atomic.setAtomic();
  SgTemplateType parameter_type("T");
  parameter_type.set_template_parameter_position(0);
  std::unique_ptr<SgTemplateParameter> parameter(
      SageBuilder::buildTemplateParameter(SgTemplateParameter::type_parameter,
                                          &parameter_type,
                                          SgTemplateParameter::keyword_class));
  auto argument = std::make_unique<SgTemplateArgument>(operand.get(), false);
  SgTemplateParameterPtrList parameters{parameter.get()};
  SgTemplateArgumentPtrList arguments{argument.get()};
  SgModifierType source_modifier(&parameter_type);
  source_modifier.get_typeModifier() = atomic;
  const auto modifier_buckets = SgModifierNodes::numberOfNodes();
  std::vector<SgModifierType *> modifiers{
      requireTypeFactoryAllocationSafety<SgModifierType>([&] {
        return isSgModifierType(
            Rose::Builder::Templates::instantiateNonrealTypes(
                &source_modifier, parameters, arguments, nullptr));
      })};
  for (int address = 0; address != 6; ++address) {
    SgTypeModifier modifier;
    modifier.setAtomic();
    modifier.setAddressSpace();
    modifier.set_address_space_value(address);
    auto candidate = std::make_unique<SgModifierType>(operand.get());
    candidate->get_typeModifier() = modifier;
    const auto buckets = SgModifierNodes::numberOfNodes();
    auto *head = operand->get_modifiers();
    SgModifierType *canonical = nullptr;
    for (int boundary = 0; boundary != 32 && canonical == nullptr; ++boundary) {
      allocation_failure_countdown = boundary;
      try {
        canonical =
            SgModifierType::insertModifierTypeIntoTypeTable(candidate.get());
      } catch (const std::bad_alloc &) {
      }
      allocation_failure_countdown = -1;
      if (canonical == nullptr) {
        ROSE_ASSERT(operand->get_modifiers() == head);
        ROSE_ASSERT(SgModifierNodes::numberOfNodes() == buckets);
        ROSE_ASSERT(candidate->get_parent() == nullptr);
      }
    }
    ROSE_ASSERT(canonical == candidate.get());
    candidate.release();
    auto duplicate = std::make_unique<SgModifierType>(operand.get());
    duplicate->get_typeModifier() = modifier;
    SgModifierType *reused = nullptr;
    allocation_failure_countdown = 0;
    try {
      reused = SgModifierType::insertModifierTypeIntoTypeTable(duplicate.get());
    } catch (const std::bad_alloc &) {
    }
    allocation_failure_countdown = -1;
    if (reused != nullptr)
      duplicate.release();
    ROSE_ASSERT(reused == canonical);
    modifiers.push_back(canonical);
  }
  auto *head = operand->get_modifiers();
  auto *tail = head->get_next();
  ROSE_ASSERT(tail != nullptr && tail->get_next() == nullptr);
  for (size_t index = 0; index != 4; ++index)
    delete modifiers[index];
  ROSE_ASSERT(!SgNode::isLiveNode(head));
  ROSE_ASSERT(operand->get_modifiers() == tail &&
              tail->get_parent() == operand.get());
  for (size_t index = 4; index != modifiers.size(); ++index)
    delete modifiers[index];
  ROSE_ASSERT(operand->get_modifiers() == nullptr);
  ROSE_ASSERT(SgModifierNodes::numberOfNodes() == modifier_buckets);
  auto scope = std::make_unique<SgGlobal>();
  auto declaration = std::make_unique<SgClassDeclaration>(
      nullptr, "rex_fault_member_owner", SgClassDeclaration::e_class, nullptr,
      nullptr);
  declaration->set_firstNondefiningDeclaration(declaration.get());
  declaration->set_scope(scope.get());
  auto *owner = SgClassType::createType(declaration.get());
  requireTypeFactoryAllocationSafety<SgPointerMemberType>(
      [&] { return SgPointerMemberType::createType(operand.get(), owner); });
  argument.reset();
  SageInterface::deleteAST(operand.release(),
                           SageInterface::DeleteAstMode::kRequireIsolated);
  ROSE_ASSERT(SgTypeInt::numberOfNodes() == integers_before);
  declaration->set_type(nullptr);
  delete owner;
  const SgName spelling("__SVInt8_t");
  auto *builtin = requireTypeFactoryAllocationSafety<SgTypeTargetBuiltin>([&] {
    return SgTypeTargetBuiltin::createType(
        spelling, SgTypeTargetBuiltin::e_target_builtin_aarch64);
  });
  SageInterface::deleteAST(builtin,
                           SageInterface::DeleteAstMode::kRequireIsolated);
  const SgName label("rex_fault_label");
  auto *label_type = requireTypeFactoryAllocationSafety<SgTypeLabel>(
      [&] { return SgTypeLabel::createType(label); });
  SageInterface::deleteAST(label_type,
                           SageInterface::DeleteAstMode::kRequireIsolated);
  ROSE_ASSERT(SgFunctionTypeSymbol::numberOfNodes() == symbols_before);
  ROSE_ASSERT(symbols->get_table()->size() == entries_before);
  symbols->validate_exact_symbol_indexes();
}

void requireFunctionTypeCacheRetirement() {
  SgTypeInt integer;
  SgTypeFloat floating;
  auto *arguments = new SgFunctionParameterTypeList();
  arguments->append_argument(&integer);
  SgFunctionType function(&integer, false, arguments);
  auto *pack_arguments = new SgFunctionParameterTypeList();
  pack_arguments->append_argument(&integer);
  pack_arguments->get_argument_qualification_use_sites()
      .front()
      ->set_is_pack_expansion(true);
  SgFunctionType pack_function(&integer, false, pack_arguments);
  auto table = std::make_unique<SgFunctionTypeTable>();
  table->insert_function_type(&function);
  table->insert_function_type(&pack_function);
  bool retired = false;
  allocation_failure_countdown = 0;
  try {
    ROSE_ASSERT(table->lookup_exact_function_type(&integer, arguments, nullptr,
                                                  0) == &function);
    ROSE_ASSERT(table->lookup_exact_function_type(
                    &integer, pack_arguments, nullptr, 0) == &pack_function);
    ROSE_ASSERT(table->lookup_exact_function_type(&floating, arguments, nullptr,
                                                  0) == nullptr);
    table->remove_function_type(&function);
    ROSE_ASSERT(table->lookup_exact_function_type(&integer, arguments, nullptr,
                                                  0) == nullptr);
    ROSE_ASSERT(table->lookup_exact_function_type(
                    &integer, pack_arguments, nullptr, 0) == &pack_function);
    table->remove_function_type(&pack_function);
    retired = true;
  } catch (const std::bad_alloc &) {
  }
  allocation_failure_countdown = -1;
  ROSE_ASSERT(retired && table->size() == 0);
}

void requireSemanticArrayAdmissionSafety() {
  using namespace Rose::AstJson;
  auto *integer = SageBuilder::buildIntType();
  const auto arrays_before = SgArrayType::numberOfNodes();
  bool completed = false;
  for (int boundary = 0; boundary != 16 && !completed; ++boundary) {
    {
      auto shape = std::make_shared<InlineExpressionConstruction>();
      auto *array = static_cast<SgArrayType *>(
          shape->own(new SgArrayType(integer, nullptr)));
      SemanticArrayConstruction construction;
      bool rejected = false;
      allocation_failure_countdown = boundary;
      try {
        construction.take(array, shape);
      } catch (const std::bad_alloc &) {
        rejected = true;
      }
      allocation_failure_countdown = -1;
      ROSE_ASSERT(shape->owns(array));
      if (rejected) {
        ROSE_ASSERT(!construction.owns(array));
      } else {
        ROSE_ASSERT(!inject_allocation_failures || boundary >= 2);
        ROSE_ASSERT(construction.owns(array));
        completed = true;
      }
    }
    ROSE_ASSERT(SgArrayType::numberOfNodes() == arrays_before);
  }
  ROSE_ASSERT(completed);
}

void requireConstructionOwnershipTransfer() {
  using Rose::AstJson::InlineExpressionConstruction;
  auto original_parent = std::make_unique<SgNullStatement>();
  auto borrowed = std::make_unique<SgNullStatement>();
  borrowed->set_parent(original_parent.get());
  const auto statements_before = SgNullStatement::numberOfNodes();
  {
    InlineExpressionConstruction enclosing;
    InlineExpressionConstruction nested;
    auto *first = enclosing.own(new SgNullStatement());
    auto *second = nested.own(new SgNullStatement());
    enclosing.borrow(borrowed.get());
    borrowed->set_parent(first);
    nested.borrow(borrowed.get());
    borrowed->set_parent(second);
    bool transferred = false;
    allocation_failure_countdown = 0;
    try {
      nested.transferTo(enclosing);
      transferred = true;
    } catch (const std::bad_alloc &) {
    }
    allocation_failure_countdown = -1;
    ROSE_ASSERT(transferred && nested.empty());
    ROSE_ASSERT(enclosing.owns(first) && enclosing.owns(second));
    ROSE_ASSERT(borrowed->get_parent() == second);
  }
  ROSE_ASSERT(borrowed->get_parent() == original_parent.get());
  ROSE_ASSERT(SgNullStatement::numberOfNodes() == statements_before);
}

void requireSymbolIndexAllocationSafety() {
  SgTypeInt integer_type;
  auto *type = &integer_type;
  auto existing_name = std::make_unique<SgInitializedName>(
      SgName("rex_existing_basis"), type, nullptr);
  auto new_name = std::make_unique<SgInitializedName>(SgName("rex_new_basis"),
                                                      type, nullptr);
  const SgName existing_key("rex_existing_symbol");
  const SgName new_key("rex_new_symbol_" + std::string(96, 'x'));
  for (bool case_insensitive : {false, true}) {
    for (bool shared_basis : {false, true}) {
      SgSymbolTable table;
      table.setCaseInsensitive(case_insensitive);
      SgVariableSymbol existing(existing_name.get());
      SgVariableSymbol candidate(shared_basis ? existing_name.get()
                                              : new_name.get());
      table.insert(existing_key, &existing);
      const auto basis_index = table.get_symbolBasisExactIndex();
      bool completed = false;
      for (int boundary = 0; boundary != 64; ++boundary) {
        bool rejected = false;
        allocation_failure_countdown = boundary;
        try {
          table.insert(new_key, &candidate);
        } catch (const std::bad_alloc &) {
          rejected = true;
        }
        allocation_failure_countdown = -1;
        ROSE_ASSERT(existing.get_parent() == &table);
        ROSE_ASSERT(table.find_variable(existing_key) == &existing);
        ROSE_ASSERT(table.stored_name_of(&existing) == existing_key);
        if (rejected) {
          ROSE_ASSERT(candidate.get_parent() == nullptr);
          ROSE_ASSERT(!table.exists(&candidate));
          ROSE_ASSERT(table.get_table()->size() == 1);
          ROSE_ASSERT(table.find_variable(new_key) == nullptr);
          ROSE_ASSERT(table.get_symbolBasisExactIndex() == basis_index);
          table.validate_exact_symbol_indexes();
          continue;
        }
        ROSE_ASSERT(!inject_allocation_failures || boundary >= 4);
        ROSE_ASSERT(candidate.get_parent() == &table);
        ROSE_ASSERT(table.find_variable(new_key) == &candidate);
        table.validate_exact_symbol_indexes();
        table.remove(&candidate);
        ROSE_ASSERT(table.get_symbolBasisExactIndex() == basis_index);
        completed = true;
        break;
      }
      ROSE_ASSERT(completed);
      if (shared_basis) {
        candidate.set_declaration(new_name.get());
        table.insert(new_key, &candidate);
      }
      const auto original_index = table.get_symbolBasisExactIndex();
      completed = false;
      for (int boundary = 0; boundary != 64; ++boundary) {
        existing.set_declaration(new_name.get());
        bool rejected = false;
        allocation_failure_countdown = boundary;
        try {
          table.reindex_symbol_basis(&existing, existing_name.get());
        } catch (const std::bad_alloc &) {
          rejected = true;
        }
        allocation_failure_countdown = -1;
        ROSE_ASSERT(existing.get_parent() == &table && table.exists(&existing));
        ROSE_ASSERT(table.find_variable(existing_key) == &existing);
        if (rejected) {
          ROSE_ASSERT(table.get_symbolBasisExactIndex() == original_index);
          existing.set_declaration(existing_name.get());
          table.validate_exact_symbol_indexes();
          continue;
        }
        ROSE_ASSERT(!inject_allocation_failures || boundary >= 1);
        ROSE_ASSERT(
            table.find_symbols_with_exact_basis(existing_name.get()).empty());
        table.validate_exact_symbol_indexes();
        existing.set_declaration(existing_name.get());
        table.reindex_symbol_basis(&existing, new_name.get());
        ROSE_ASSERT(table.get_symbolBasisExactIndex() == original_index);
        completed = true;
        break;
      }
      ROSE_ASSERT(completed);
      if (shared_basis)
        table.remove(&candidate);
      table.remove(&existing);
      table.validate_exact_symbol_indexes();
    }
  }
  SgTypeTable types;
  SgTypeInt new_type;
  auto original_owner = std::make_unique<SgNullStatement>();
  new_type.set_parent(original_owner.get());
  const auto symbols_before = SgFunctionTypeSymbol::numberOfNodes();
  bool completed = false;
  for (int boundary = 0; boundary != 64; ++boundary) {
    bool rejected = false;
    allocation_failure_countdown = boundary;
    try {
      types.insert_type(new_key, &new_type);
    } catch (const std::bad_alloc &) {
      rejected = true;
    }
    allocation_failure_countdown = -1;
    if (rejected) {
      ROSE_ASSERT(new_type.get_parent() == original_owner.get());
      ROSE_ASSERT(SgFunctionTypeSymbol::numberOfNodes() == symbols_before);
      ROSE_ASSERT(types.lookup_type(new_key) == nullptr);
      types.get_type_table()->validate_exact_symbol_indexes();
      continue;
    }
    ROSE_ASSERT(!inject_allocation_failures || boundary >= 4);
    ROSE_ASSERT(types.lookup_type(new_key) == &new_type);
    auto *symbol = isSgFunctionTypeSymbol(new_type.get_parent());
    ROSE_ASSERT(symbol != nullptr && symbol->get_type() == &new_type);
    ROSE_ASSERT(SgFunctionTypeSymbol::numberOfNodes() == symbols_before + 1);
    SgTypeFloat second_type;
    types.insert_type(new_key, &second_type);
    auto *second_symbol = isSgFunctionTypeSymbol(second_type.get_parent());
    ROSE_ASSERT(second_symbol != nullptr && second_symbol != symbol);
    bool removed = false;
    allocation_failure_countdown = 0;
    try {
      types.remove_type(new_key);
      removed = true;
    } catch (const std::bad_alloc &) {
    }
    allocation_failure_countdown = -1;
    ROSE_ASSERT(removed && types.lookup_type(new_key) == nullptr);
    ROSE_ASSERT(symbol->get_parent() == nullptr);
    ROSE_ASSERT(second_symbol->get_parent() == nullptr);
    types.get_type_table()->validate_exact_symbol_indexes();
    new_type.set_parent(original_owner.get());
    second_type.set_parent(nullptr);
    delete symbol;
    delete second_symbol;
    ROSE_ASSERT(SgFunctionTypeSymbol::numberOfNodes() == symbols_before);
    completed = true;
    break;
  }
  ROSE_ASSERT(completed);
}

} // namespace

int main() {
  requirePointerMapLookupAllocationSafety();
  requireSymbolIndexAllocationSafety();
  requireArgumentMetadataCopyAllocationSafety();
  requireFunctionTypeBuilderAllocationSafety();
  requireParameterTypeListBuilderAllocationSafety();
  requireParameterTypeListInsertionSafety();
  requireConstructionOwnershipTransfer();
  requireSemanticArrayAdmissionSafety();
  requireFunctionTypeCacheRetirement();
  SageBuilder::buildIntType();
  requireKindFactoryAllocationSafety<SgTypeBool>(
      [](SgExpression *kind) { return SgTypeBool::createType(kind); });
  requireKindFactoryAllocationSafety<SgTypeInt>(
      [](SgExpression *kind) { return SgTypeInt::createType(0, kind); });
  requireKindFactoryAllocationSafety<SgTypeUnsignedInt>(
      [](SgExpression *kind) { return SgTypeUnsignedInt::createType(kind); });
  requireKindFactoryAllocationSafety<SgTypeFloat>(
      [](SgExpression *kind) { return SgTypeFloat::createType(kind); });
  requireStringFactoryAllocationSafety();
  requireStringReaderAllocationSafety();
  requireStringBindingIdentity();
  requireSemanticTypeFactoryAllocationSafety();
}
