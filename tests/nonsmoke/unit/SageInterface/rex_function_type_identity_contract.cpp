#include "rose.h"

#include <memory>

namespace {
SgFunctionParameterTypeList *arguments(SgType *type) {
  auto *result = new SgFunctionParameterTypeList();
  result->append_argument(type);
  return result;
}

void releaseUnadopted(SgFunctionParameterTypeList *list) {
  ROSE_ASSERT(list->get_parent() == nullptr);
  const auto positions = list->get_argument_qualification_use_sites();
  delete list;
  for (auto *position : positions)
    ROSE_ASSERT(!SgNode::isLiveNode(position));
}
} // namespace

int main() {
  {
    auto *function = SageBuilder::buildFunctionType(
        SageBuilder::buildVoidType(), arguments(SageBuilder::buildIntType()));
    auto *other = SageBuilder::buildFunctionType(
        SageBuilder::buildVoidType(), arguments(SageBuilder::buildFloatType()));
    SgSymbolTable symbols;
    auto *symbol = new SgFunctionTypeSymbol(function->get_mangled(), function);
    symbols.insert(symbol->get_name(), symbol);
    ROSE_ASSERT(symbols.find(function) == symbol);
    ROSE_ASSERT(symbols.find(other) == nullptr);
    symbols.remove_function_type(symbol->get_name());
    ROSE_ASSERT(symbol->get_parent() == nullptr);
    ROSE_ASSERT(!symbols.exists(symbol));
    ROSE_ASSERT(symbols.find(function) == nullptr);
    symbols.insert(symbol->get_name(), symbol);
    ROSE_ASSERT(symbols.find(function) == symbol);
    symbol->set_name("rex_renamed_function_type");
    symbols.remove(symbol);
    ROSE_ASSERT(symbol->get_parent() == nullptr);
    ROSE_ASSERT(!symbols.exists(symbol));
    symbols.validate_exact_symbol_indexes();
    delete symbol;

    auto scope = std::make_unique<SgGlobal>();
    auto first = std::make_unique<SgFunctionDeclaration>(
        "rex_reindexed_function", function, nullptr, nullptr);
    auto replacement = std::make_unique<SgFunctionDeclaration>(
        "rex_reindexed_function", other, nullptr, nullptr);
    first->set_scope(scope.get());
    replacement->set_scope(scope.get());
    first->set_firstNondefiningDeclaration(first.get());
    replacement->set_firstNondefiningDeclaration(replacement.get());
    auto *binding = new SgFunctionSymbol(first.get());
    auto *bindings = scope->get_symbol_table();
    bindings->insert(first->get_name(), binding);
    ROSE_ASSERT(bindings->find_function(first->get_name(), function) ==
                binding);
    SgFunctionType copiedType(function->get_return_type(),
                              function->get_has_ellipses(),
                              arguments(SageBuilder::buildIntType()));
    ROSE_ASSERT(&copiedType != function);
    ROSE_ASSERT(bindings->find_function(first->get_name(), &copiedType) ==
                binding);
    ROSE_ASSERT(bindings->find_function(first->get_name(), other) == nullptr);
    binding->set_declaration(replacement.get());
    bindings->reindex_symbol_basis(binding, first.get());
    bindings->validate_exact_symbol_indexes();
    ROSE_ASSERT(bindings->find_symbols_with_exact_basis(first.get()).empty());
    ROSE_ASSERT(
        bindings->find_symbols_with_exact_basis(replacement.get()).size() == 1);
    ROSE_ASSERT(bindings->find_function(first->get_name(), function) ==
                nullptr);
    ROSE_ASSERT(bindings->find_function(replacement->get_name(), other) ==
                binding);
    bindings->remove(binding);
    bindings->validate_exact_symbol_indexes();
    ROSE_ASSERT(binding->get_parent() == nullptr);
    delete binding;

    auto duplicate = std::make_unique<SgFunctionDeclaration>(
        "rex_reindexed_function", function, nullptr, nullptr);
    duplicate->set_scope(scope.get());
    duplicate->set_firstNondefiningDeclaration(first.get());
    auto *firstBinding = new SgFunctionSymbol(first.get());
    auto *duplicateBinding = new SgFunctionSymbol(duplicate.get());
    bindings->insert(first->get_name(), firstBinding);
    bindings->insert(duplicate->get_name(), duplicateBinding);
    ROSE_ASSERT(bindings->find_function(first->get_name(), function) ==
                bindings->find_function(first->get_name()));
    bindings->remove(firstBinding);
    bindings->insert(first->get_name(), firstBinding);
    ROSE_ASSERT(bindings->find_function(first->get_name(), function) ==
                bindings->find_function(first->get_name()));
    bindings->remove(firstBinding);
    bindings->remove(duplicateBinding);
    bindings->setCaseInsensitive(true);
    duplicate->set_name("REX_REINDEXED_FUNCTION");
    bindings->insert(first->get_name(), firstBinding);
    bindings->insert(duplicate->get_name(), duplicateBinding);
    ROSE_ASSERT(bindings->find_function(first->get_name(), function) ==
                bindings->find_function(duplicate->get_name(), function));
    ROSE_ASSERT(bindings->find_function(first->get_name(), function) ==
                bindings->find_function(first->get_name()));
    bindings->remove(firstBinding);
    bindings->remove(duplicateBinding);
    bindings->validate_exact_symbol_indexes();
    delete firstBinding;
    delete duplicateBinding;
  }
  auto *table = SgNode::get_globalFunctionTypeTable();
  const size_t initialSize = table->size();
  // Reserve the named-type identities before publishing their declarations.
  // A frontend must be able to intern signatures at this construction step;
  // the enclosing lexical scopes are reserved but not yet attached, so their
  // qualified names cannot be mangled at this construction step.
  auto *firstDeclaration = new SgClassDeclaration(
      Sg_File_Info::generateDefaultFileInfoForTransformationNode(), "same_name",
      SgClassDeclaration::e_class, nullptr, nullptr);
  auto *secondDeclaration = new SgClassDeclaration(
      Sg_File_Info::generateDefaultFileInfoForTransformationNode(), "same_name",
      SgClassDeclaration::e_class, nullptr, nullptr);
  firstDeclaration->set_firstNondefiningDeclaration(firstDeclaration);
  secondDeclaration->set_firstNondefiningDeclaration(secondDeclaration);
  firstDeclaration->set_scope(new SgBasicBlock());
  secondDeclaration->set_scope(new SgBasicBlock());
  auto *firstType = SgClassType::createType(firstDeclaration);
  auto *secondType = SgClassType::createType(secondDeclaration);
  ROSE_ASSERT(firstType->get_declaration() == firstDeclaration);
  ROSE_ASSERT(secondType->get_declaration() == secondDeclaration);
  ROSE_ASSERT(SgClassType::createType(firstDeclaration) == firstType);
  ROSE_ASSERT(SgClassType::createType(secondDeclaration) == secondType);

  auto *voidType = SageBuilder::buildVoidType();
  auto *first = SageBuilder::buildFunctionType(voidType, arguments(firstType));
  auto *sameArguments = arguments(firstType);
  ROSE_ASSERT(SageBuilder::buildFunctionType(voidType, sameArguments) == first);
  ROSE_ASSERT(SageInterface::findFunctionType(voidType, sameArguments) ==
              first);
  releaseUnadopted(sameArguments);
  auto *second =
      SageBuilder::buildFunctionType(voidType, arguments(secondType));
  ROSE_ASSERT(second != first);
  auto *memberArguments = arguments(firstType);
  auto *member = SageBuilder::buildMemberFunctionType(
      voidType, memberArguments, firstType, SgMemberFunctionType::e_const);
  ROSE_ASSERT(SageBuilder::buildMemberFunctionType(
                  voidType, memberArguments, firstType,
                  SgMemberFunctionType::e_const) == member);
  auto *nonConst = SageBuilder::buildMemberFunctionType(
      voidType, memberArguments, firstType, 0);
  auto *otherClass = SageBuilder::buildMemberFunctionType(
      voidType, memberArguments, secondType, SgMemberFunctionType::e_const);
  ROSE_ASSERT(member != nonConst && member != otherClass);
  ROSE_ASSERT(table->size() == initialSize + 5);
  auto *qualified = SageBuilder::buildMemberFunctionType(
      voidType, memberArguments, static_cast<SgType *>(nullptr),
      SgMemberFunctionType::e_const);
  auto *refQualified = SageBuilder::buildMemberFunctionType(
      voidType, memberArguments, static_cast<SgType *>(nullptr),
      SgMemberFunctionType::e_ref_qualifier_lvalue);
  ROSE_ASSERT(qualified != first && qualified != member &&
              qualified != refQualified);
  ROSE_ASSERT(qualified->get_class_type() == nullptr);
  ROSE_ASSERT(!SageInterface::containsUnknownType(qualified));
  ROSE_ASSERT(!SageInterface::containsUnknownType(refQualified));
  ROSE_ASSERT(SageBuilder::buildMemberFunctionType(
                  voidType, memberArguments, static_cast<SgType *>(nullptr),
                  SgMemberFunctionType::e_const) == qualified);
  ROSE_ASSERT(table->size() == initialSize + 7);
  releaseUnadopted(memberArguments);

  auto *functionPointer = SageBuilder::buildPointerType(member);
  auto *reference = SageBuilder::buildRvalueReferenceType(functionPointer);
  ROSE_ASSERT(SageBuilder::buildPointerType(member) == functionPointer);
  ROSE_ASSERT(SageBuilder::buildRvalueReferenceType(functionPointer) ==
              reference);
  ROSE_ASSERT(
      SageBuilder::buildReferenceType(member)->getInternalTypes().size() == 4);
  const auto functionTypes = reference->getInternalTypes();
  ROSE_ASSERT(functionTypes.size() == 5);
  SgType *expectedTypes[] = {reference, functionPointer, member, voidType,
                             firstType};
  for (SgType *type : expectedTypes)
    ROSE_ASSERT(std::count(functionTypes.begin(), functionTypes.end(), type) ==
                1);
  ROSE_ASSERT(qualified->getInternalTypes().size() == 3);
  auto *memberPointer =
      SageBuilder::buildPointerMemberType(firstType, secondType);
  const auto memberTypes = memberPointer->getInternalTypes();
  ROSE_ASSERT(memberTypes.size() == 3);
  ROSE_ASSERT(std::count(memberTypes.begin(), memberTypes.end(), firstType) ==
              1);
  ROSE_ASSERT(std::count(memberTypes.begin(), memberTypes.end(), secondType) ==
              1);

  table->remove_function_type(first);
  auto *query = arguments(firstType);
  ROSE_ASSERT(SageInterface::findFunctionType(voidType, query) == nullptr);
  ROSE_ASSERT(first->get_argument_list()->get_arguments().front() == firstType);
  table->insert_function_type(first);
  ROSE_ASSERT(SageInterface::findFunctionType(voidType, query) == first);
  releaseUnadopted(query);
  {
    auto spaceship = [](int left, int right) {
      auto *dependent = SgTypeDependent::createType();
      return new SgSpaceshipOp(
          SageBuilder::buildFunctionParameterRefExp_nfi(left, 0, dependent),
          SageBuilder::buildFunctionParameterRefExp_nfi(right, 0, dependent),
          dependent);
    };
    auto *first = spaceship(0, 1);
    auto *same = spaceship(0, 1);
    auto *reversed = spaceship(1, 0);
    ROSE_ASSERT(SageInterface::constraintExpressionEquivalence(first, same));
    ROSE_ASSERT(
        !SageInterface::constraintExpressionEquivalence(first, reversed));
    for (auto *expression : {first, same, reversed})
      SageInterface::deleteAST(expression);
  }
  {
    auto firstScope = std::make_unique<SgGlobal>();
    auto secondScope = std::make_unique<SgGlobal>();
    auto memberAccess = [](SgScopeStatement *scope, int parameter,
                           const char *name) {
      auto *type = SageBuilder::buildSemanticNonrealType(name, scope, nullptr);
      auto *declaration = isSgNonrealDecl(type->get_declaration());
      auto *symbol =
          isSgNonrealSymbol(declaration->get_symbol_from_symbol_table());
      auto *receiver = SageBuilder::buildFunctionParameterRefExp_nfi(
          parameter, 0, SgTypeDependent::createType());
      return SageBuilder::buildDotExp_nfi(
          receiver, SageBuilder::buildNonrealRefExp_nfi(symbol),
          SgTypeDependent::createType());
    };
    auto *canonical_reference =
        SageBuilder::buildReferenceType(SageBuilder::buildIntType());
    auto *equivalent_reference =
        new SgReferenceType(SageBuilder::buildIntType());
    ROSE_ASSERT(canonical_reference != equivalent_reference);
    auto *firstParameter = SageBuilder::buildFunctionParameterRefExp_nfi(
        0, 0, canonical_reference);
    auto *sameParameter = SageBuilder::buildFunctionParameterRefExp_nfi(
        0, 0, equivalent_reference);
    auto *otherParameter = SageBuilder::buildFunctionParameterRefExp_nfi(
        0, 0, SageBuilder::buildReferenceType(SageBuilder::buildLongType()));
    ROSE_ASSERT(SageInterface::constraintExpressionEquivalence(firstParameter,
                                                               sameParameter));
    ROSE_ASSERT(!SageInterface::constraintExpressionEquivalence(
        firstParameter, otherParameter));
    for (auto *reference : {firstParameter, sameParameter, otherParameter})
      SageInterface::deleteAST(reference);
    delete equivalent_reference;
    auto *firstMember = memberAccess(firstScope.get(), 0, "member");
    auto *sameMember = memberAccess(secondScope.get(), 0, "member");
    auto *otherReceiver = memberAccess(secondScope.get(), 1, "member");
    auto *otherMember = memberAccess(secondScope.get(), 0, "other");
    ROSE_ASSERT(SageInterface::constraintExpressionEquivalence(firstMember,
                                                               sameMember));
    ROSE_ASSERT(!SageInterface::constraintExpressionEquivalence(firstMember,
                                                                otherReceiver));
    ROSE_ASSERT(!SageInterface::constraintExpressionEquivalence(firstMember,
                                                                otherMember));
    auto *qualifier = SageBuilder::buildSemanticNonrealType(
        "qualifier", secondScope.get(), nullptr);
    auto *qualifierScope =
        qualifier->get_declaration()->get_nonreal_decl_scope();
    auto *qualifiedMember = memberAccess(qualifierScope, 0, "member");
    ROSE_ASSERT(!SageInterface::constraintExpressionEquivalence(
        firstMember, qualifiedMember));
    isSgNonrealRefExp(isSgDotExp(firstMember)->get_rhs_operand())
        ->set_explicit_name_qualification_length(0);
    ROSE_ASSERT(SageInterface::constraintExpressionEquivalence(firstMember,
                                                               sameMember));
    ROSE_ASSERT(!SageInterface::constraintExpressionEquivalence(
        firstMember, qualifiedMember));
    for (auto *expression :
         {firstMember, sameMember, otherReceiver, otherMember, qualifiedMember})
      SageInterface::deleteAST(expression);
  }
  {
    SgTypeModifier object_qualifiers;
    object_qualifiers.setAddressSpace();
    object_qualifiers.set_address_space_value(4);
    auto *integer = SageBuilder::buildIntType();
    auto *pointer = SageBuilder::buildPointerType(integer);
    auto *object = SageBuilder::buildModifierType(pointer, object_qualifiers);
    auto *adjusted = SageBuilder::buildFunctionParameterTypeList(object);
    ROSE_ASSERT(adjusted->get_arguments().front() == pointer);
    ROSE_ASSERT(SageBuilder::buildFunctionType(SageBuilder::buildVoidType(),
                                               adjusted) ==
                SageBuilder::buildFunctionType(SageBuilder::buildVoidType(),
                                               arguments(pointer)));
    ROSE_ASSERT(object->get_typeModifier() == object_qualifiers);
    ROSE_ASSERT(
        SageInterface::cxxParameterTypeMatchesSemanticType(pointer, object));
    ROSE_ASSERT(
        !SageInterface::cxxSourceTypeMatchesSemanticType(pointer, object));

    SgTypeModifier global_qualifiers, local_qualifiers;
    global_qualifiers.setOpenclGlobal();
    local_qualifiers.setOpenclLocal();
    auto *global_type =
        SageBuilder::buildModifierType(integer, global_qualifiers);
    auto *global_pointer = SageBuilder::buildPointerType(global_type);
    auto *local_pointer = SageBuilder::buildPointerType(
        SageBuilder::buildModifierType(integer, local_qualifiers));
    auto *global_object =
        SageBuilder::buildModifierType(global_pointer, object_qualifiers);
    auto *local_object =
        SageBuilder::buildModifierType(local_pointer, object_qualifiers);
    ROSE_ASSERT(SageInterface::cxxParameterTypeMatchesSemanticType(
        global_pointer, global_object));
    ROSE_ASSERT(!SageInterface::cxxParameterTypeMatchesSemanticType(
        global_pointer, local_object));
    auto *array = SageBuilder::buildArrayType(global_type);
    ROSE_ASSERT(SageInterface::cxxParameterTypeMatchesSemanticType(
        array, global_object));
    ROSE_ASSERT(!SageInterface::cxxParameterTypeMatchesSemanticType(
        array, local_object));
  }
  table->clear();
  ROSE_ASSERT(table->size() == 0);
  ROSE_ASSERT(member->get_class_type() == firstType);
  return 0;
}
