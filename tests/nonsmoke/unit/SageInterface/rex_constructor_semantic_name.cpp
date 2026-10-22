#include "CallGraph.h"
#include "interproceduralCFG.h"
#include "midend/programTransformation/astInlining/inlinerSupport.h"
#include "rose.h"

#include <algorithm>
#include <cstring>
#include <set>

extern bool virtualInterproceduralControlFlowGraphs;

namespace {
void checkNames(SgProject *project) {
  for (SgFile *file : project->get_fileList()) {
    ROSE_ASSERT(SageInterface::getEnclosingFileNode(file) == file);
    for (SgNode *declaration :
         NodeQuery::querySubTree(file, V_SgClassDeclaration))
      ROSE_ASSERT(SageInterface::getEnclosingFileNode(declaration) == file);
  }
  unsigned constructors = 0, destructors = 0, instantiated = 0;
  std::set<SgFunctionModifier::copy_move_kind_enum> copy_move_roles;
  for (SgNode *node :
       NodeQuery::querySubTree(project, V_SgMemberFunctionDeclaration)) {
    auto *function = isSgMemberFunctionDeclaration(node);
    const bool constructor =
        function->get_specialFunctionModifier().isConstructor();
    const bool destructor =
        function->get_specialFunctionModifier().isDestructor();
    const auto copy_move_kind =
        function->get_functionModifier().getCopyMoveKind();
    auto *first = isSgMemberFunctionDeclaration(
        function->get_firstNondefiningDeclaration());
    ROSE_ASSERT(first != nullptr &&
                first->get_functionModifier().getCopyMoveKind() ==
                    copy_move_kind);
    if (copy_move_kind == SgFunctionModifier::e_copy_assignment ||
        copy_move_kind == SgFunctionModifier::e_move_assignment) {
      ROSE_ASSERT(
          function->get_functionModifier().hasSpecialMemberProperties());
      ROSE_ASSERT(function->get_functionModifier().isTrivialSpecialMember() ==
                  first->get_functionModifier().isTrivialSpecialMember());
      ROSE_ASSERT(
          function->get_functionModifier().isIneligibleSpecialMember() ==
          first->get_functionModifier().isIneligibleSpecialMember());
    }
    auto *scope = isSgClassDefinition(function->get_scope());
    ROSE_ASSERT(scope != nullptr);
    if (scope->get_declaration()->get_name() == "RexElision") {
      if (copy_move_kind != SgFunctionModifier::e_copy_move_none)
        copy_move_roles.insert(copy_move_kind);
      const auto &parameters = function->get_parameterList()->get_args();
      if (constructor || function->get_specialFunctionModifier().isOperator()) {
        ROSE_ASSERT(!parameters.empty());
        auto *type =
            parameters.front()->get_type()->stripTypedefsAndModifiers();
        auto expected = SgFunctionModifier::e_copy_move_none;
        if (isSgReferenceType(type))
          expected = constructor ? SgFunctionModifier::e_copy_constructor
                                 : SgFunctionModifier::e_copy_assignment;
        else if (isSgRvalueReferenceType(type))
          expected = constructor ? SgFunctionModifier::e_move_constructor
                                 : SgFunctionModifier::e_move_assignment;
        ROSE_ASSERT(copy_move_kind == expected);
      }
    }
    if (!constructor && !destructor)
      continue;
    if (constructor) {
      const auto &properties = function->get_functionModifier();
      ROSE_ASSERT(properties.hasSpecialMemberProperties());
      ROSE_ASSERT(first->get_functionModifier().isDefaultConstructor() ==
                  properties.isDefaultConstructor());
      ROSE_ASSERT(first->get_functionModifier().isTrivialSpecialMember() ==
                  properties.isTrivialSpecialMember());
      ROSE_ASSERT(first->get_functionModifier().isIneligibleSpecialMember() ==
                  properties.isIneligibleSpecialMember());
    }
    if (constructor) {
      for (auto *entry : function->get_ctors()) {
        if (entry->get_preinitialization() != SgInitializedName::e_data_member)
          continue;
        auto *target = entry->get_ctor_initializer_field_target();
        ROSE_ASSERT(target != nullptr && target->get_scope() == scope);
        ROSE_ASSERT(SageInterface::isEquivalentType(entry->get_type(),
                                                    target->get_type()));
      }
    }
    auto *owner = scope->get_declaration();
    ROSE_ASSERT(owner != nullptr);
    SgName class_name;
    if (auto *instance = isSgTemplateInstantiationDecl(owner)) {
      class_name = instance->get_templateName();
      ++instantiated;
    } else if (auto *templated = isSgTemplateClassDeclaration(owner)) {
      class_name = templated->get_templateName();
    } else {
      class_name = owner->get_name();
    }
    ROSE_ASSERT(!class_name.is_null());
    SgName function_name = function->get_name();
    if (auto *instance = isSgTemplateInstantiationMemberFunctionDecl(function))
      function_name = instance->get_templateName();
    ROSE_ASSERT(
        function_name ==
        (destructor ? SgName("~" + class_name.getString()) : class_name));
    constructors += constructor;
    destructors += destructor;
  }
  ROSE_ASSERT(constructors > 0 && destructors > 0 && instantiated > 0);
  ROSE_ASSERT(copy_move_roles ==
              (std::set<SgFunctionModifier::copy_move_kind_enum>{
                  SgFunctionModifier::e_copy_constructor,
                  SgFunctionModifier::e_move_constructor,
                  SgFunctionModifier::e_copy_assignment,
                  SgFunctionModifier::e_move_assignment}));
}

void checkElision(SgProject *project) {
  unsigned named_returns = 0;
  SgReturnStmt *previous_return = nullptr;
  for (SgNode *node : NodeQuery::querySubTree(project, V_SgReturnStmt)) {
    auto *statement = isSgReturnStmt(node);
    auto *function = SageInterface::getEnclosingFunctionDeclaration(statement);
    ROSE_ASSERT(function != nullptr);
    if (function->get_name() != "rex_return_copy" &&
        function->get_name() != "rex_return_move")
      continue;
    auto *candidate = statement->get_nrvo_candidate();
    ROSE_ASSERT(candidate != nullptr &&
                candidate->get_scope() ==
                    function->get_definition()->get_body());
    unsigned references = 0;
    for (SgNode *operand :
         NodeQuery::querySubTree(statement->get_expression(), V_SgVarRefExp))
      references +=
          isSgVarRefExp(operand)->get_symbol()->get_declaration() == candidate;
    ROSE_ASSERT(references == 1);
    if (previous_return != nullptr) {
      ROSE_ASSERT(!SgReturnStmt::is_valid_nrvo_binding(
          statement->get_expression(), previous_return->get_nrvo_candidate()));
      ROSE_ASSERT(!SgReturnStmt::is_valid_nrvo_binding(
          previous_return->get_expression(), candidate));
    }
    previous_return = statement;
    ++named_returns;
  }
  ROSE_ASSERT(named_returns == 2);
  auto *file = isSgSourceFile(&project->get_file(0));
  ROSE_ASSERT(file != nullptr);
  SgClassSymbol *symbol =
      file->get_globalScope()->lookup_class_symbol("RexElision");
  ROSE_ASSERT(symbol != nullptr && symbol->get_declaration() != nullptr);
  SgClassType *fixture_type = symbol->get_declaration()->get_type();
  const struct {
    const char *name;
    bool nothrow;
  } exceptionGuarantees[] = {{"RexNoThrowSpecialMembers", true},
                             {"RexThrowingSpecialMembers", false},
                             {"RexImplicitNoThrowSpecialMembers", true},
                             {"RexOverrideNoThrowSpecialMembers", true}};
  for (const auto &expected : exceptionGuarantees) {
    auto *record = file->get_globalScope()->lookup_class_symbol(expected.name);
    ROSE_ASSERT(record != nullptr);
    auto *type = record->get_declaration()->get_type();
    ROSE_ASSERT(SageInterface::HasNoThrowConstructor(type) == expected.nothrow);
    ROSE_ASSERT(SageInterface::HasNoThrowCopy(type) == expected.nothrow);
    ROSE_ASSERT(SageInterface::HasNoThrowAssign(type) == expected.nothrow);
    auto *qualified = SageBuilder::buildConstType(type);
    ROSE_ASSERT(SageInterface::HasNoThrowConstructor(qualified) ==
                expected.nothrow);
    ROSE_ASSERT(SageInterface::HasNoThrowCopy(qualified) == expected.nothrow);
    ROSE_ASSERT(!SageInterface::HasNoThrowAssign(qualified));
  }
  const struct {
    const char *name;
    bool accessible;
    bool trivial;
  } defaults[] = {{"RexImplicitDefault", true, true},
                  {"RexProtectedDefault", false, true},
                  {"RexVirtualDefault", true, false}};
  for (const auto &expected : defaults) {
    auto *record = file->get_globalScope()->lookup_class_symbol(expected.name);
    ROSE_ASSERT(record != nullptr);
    auto *type = record->get_declaration()->get_type();
    ROSE_ASSERT(SageInterface::isDefaultConstructible(type) ==
                expected.accessible);
    ROSE_ASSERT(SageInterface::HasTrivialConstructor(type) == expected.trivial);
    ROSE_ASSERT(SageInterface::IsTrivial(type) == expected.trivial);
  }
  auto *assignment =
      file->get_globalScope()->lookup_class_symbol("RexTrivialAssignment");
  ROSE_ASSERT(assignment != nullptr);
  ROSE_ASSERT(SageInterface::HasTrivialAssign(
      assignment->get_declaration()->get_type()));
  auto *move =
      file->get_globalScope()->lookup_class_symbol("RexNontrivialMove");
  ROSE_ASSERT(move != nullptr);
  ROSE_ASSERT(!SageInterface::IsTrivial(move->get_declaration()->get_type()));
  for (const char *name : {"RexDeletedDestructor", "RexMoveOnly"}) {
    auto *record = file->get_globalScope()->lookup_class_symbol(name);
    ROSE_ASSERT(record != nullptr);
    ROSE_ASSERT(
        SageInterface::IsTrivial(record->get_declaration()->get_type()));
  }
  ROSE_ASSERT(SageInterface::IsTrivial(SageBuilder::buildNullptrType()));
  ROSE_ASSERT(!SageInterface::IsTrivial(SageBuilder::buildVoidType()));
  if (project->get_Cxx20_only()) {
    auto *ineligible =
        file->get_globalScope()->lookup_typedef_symbol("RexIneligibleMove");
    ROSE_ASSERT(ineligible != nullptr);
    ROSE_ASSERT(SageInterface::IsTrivial(
        ineligible->get_declaration()->get_base_type()));
    auto *defaulted =
        file->get_globalScope()->lookup_typedef_symbol("RexIneligibleDefault");
    ROSE_ASSERT(defaulted != nullptr);
    ROSE_ASSERT(!SageInterface::isDefaultConstructible(
        defaulted->get_declaration()->get_base_type()));
    auto *copied =
        file->get_globalScope()->lookup_typedef_symbol("RexIneligibleCopy");
    ROSE_ASSERT(copied != nullptr);
    auto *copy_type = copied->get_declaration()->get_base_type();
    ROSE_ASSERT(SageInterface::HasTrivialCopy(copy_type));
    ROSE_ASSERT(SageInterface::HasTrivialAssign(copy_type));
    ROSE_ASSERT(SageInterface::IsTrivial(copy_type));
    ROSE_ASSERT(SageInterface::isCopyConstructible(copy_type));
    ROSE_ASSERT(SageInterface::isAssignable(copy_type));
    auto *unavailable =
        file->get_globalScope()->lookup_typedef_symbol("RexNoEligibleCopy");
    ROSE_ASSERT(unavailable != nullptr);
    auto *unavailable_type = unavailable->get_declaration()->get_base_type();
    ROSE_ASSERT(!SageInterface::HasTrivialCopy(unavailable_type));
    ROSE_ASSERT(!SageInterface::HasTrivialAssign(unavailable_type));
    ROSE_ASSERT(!SageInterface::isCopyConstructible(unavailable_type));
    ROSE_ASSERT(!SageInterface::isAssignable(unavailable_type));
  }
  ROSE_ASSERT(!SageInterface::HasTrivialCopy(fixture_type));
  ROSE_ASSERT(!SageInterface::HasNoThrowCopy(fixture_type));
  auto *value_symbol = file->get_globalScope()->lookup_class_symbol("RexValue");
  ROSE_ASSERT(value_symbol != nullptr);
  auto *value_type = value_symbol->get_declaration()->get_type();
  ROSE_ASSERT(SageInterface::HasTrivialCopy(value_type));
  ROSE_ASSERT(
      SageInterface::HasTrivialCopy(SageBuilder::buildConstType(value_type)));
  ROSE_ASSERT(SageInterface::HasTrivialCopy(
      SageBuilder::buildRvalueReferenceType(value_type)));
  ROSE_ASSERT(!SageInterface::HasTrivialCopy(SageBuilder::buildVoidType()));
  const auto lambdas = NodeQuery::querySubTree(project, V_SgLambdaExp);
  ROSE_ASSERT(lambdas.size() == 1);
  SgClassDeclaration *closure =
      isSgLambdaExp(lambdas.front())->get_lambda_closure_class();
  ROSE_ASSERT(closure != nullptr && closure->get_type() != fixture_type);
  ROSE_ASSERT(SageInterface::HasTrivialCopy(closure->get_type()));
  ROSE_ASSERT(SageInterface::HasTrivialAssign(closure->get_type()));
  ROSE_ASSERT(!SageInterface::isAssignable(closure->get_type()));
  ROSE_ASSERT(!SageInterface::isDefaultConstructible(closure->get_type()));
  ROSE_ASSERT(!SageInterface::HasTrivialConstructor(closure->get_type()));
  unsigned copies = 0, moves = 0, braced = 0, parenthesized = 0;
  unsigned eligible = 0, closure_constructions = 0;
  for (SgNode *node :
       NodeQuery::querySubTree(project, V_SgConstructorInitializer)) {
    auto *initializer = isSgConstructorInitializer(node);
    if (!initializer->get_is_elidable())
      continue;
    ++eligible;
    initializer->validate_elision();
    ROSE_ASSERT(initializer->get_elision_permitted());
    const auto targets =
        CallTargetSet::solveConstructorInitializer(initializer);
    ROSE_ASSERT(targets.size() == 1);
    ROSE_ASSERT(targets.front() == canonicalFunctionDeclForCallGraph(
                                       initializer->get_declaration()));
    auto *selected_owner =
        isSgClassDefinition(initializer->get_declaration()->get_scope());
    ROSE_ASSERT(selected_owner != nullptr);
    const bool fixture_constructor =
        selected_owner->get_declaration()->get_type() == fixture_type;
    const auto &parameters =
        initializer->get_declaration()->get_parameterList()->get_args();
    ROSE_ASSERT(!parameters.empty());
    SgType *source_type = parameters.front()->get_type()->stripType(
        SgType::STRIP_MODIFIER_TYPE | SgType::STRIP_TYPEDEF_TYPE);
    ROSE_ASSERT(isSgReferenceType(source_type) != nullptr ||
                isSgRvalueReferenceType(source_type) != nullptr);
    if (fixture_constructor) {
      ROSE_ASSERT(parameters.size() == 2);
      ROSE_ASSERT(parameters.back()->get_initializer() != nullptr);
      if (isSgReferenceType(source_type))
        ++copies;
      else
        ++moves;
    } else {
      ROSE_ASSERT(selected_owner->get_declaration()->get_type() ==
                  closure->get_type());
      ++closure_constructions;
    }
    SgExprListExp *arguments = initializer->get_args();
    SgExpression *operand = arguments->get_expressions().front();
    ROSE_ASSERT(hasAddressTaken(operand, initializer));
    std::string source = operand->unparseToString();
    if (initializer->get_is_braced_initialized()) {
      braced += fixture_constructor;
      source = "{" + source + "}";
    } else if (initializer->get_need_parenthesis_after_name()) {
      parenthesized += fixture_constructor;
      source = "(" + source + ")";
    }
    ROSE_ASSERT(initializer->unparseToString() == source);

    using VirtualCFG::CFGNode;
    const CFGNode after_source(arguments, 1);
    const auto outgoing = after_source.outEdges();
    ROSE_ASSERT(outgoing.size() == 2);
    const auto &semantic_arguments = arguments->get_expressions();
    const CFGNode next = semantic_arguments.size() > 1
                             ? semantic_arguments[1]->cfgForBeginning()
                             : CFGNode(initializer, 1);
    bool call = false, bypass = false;
    for (const auto &edge : outgoing) {
      ROSE_ASSERT(edge.source() == after_source);
      call |= edge.target() == next;
      bypass |= edge.target() == initializer->cfgForEnd();
      const auto incoming = edge.target().inEdges();
      ROSE_ASSERT(std::find(incoming.begin(), incoming.end(), edge) !=
                  incoming.end());
    }
    ROSE_ASSERT(call && bypass);
    if (fixture_constructor) {
      ROSE_ASSERT(semantic_arguments.size() == 2);
      auto *default_use = isSgCxxDefaultInitExp(semantic_arguments[1]);
      ROSE_ASSERT(default_use != nullptr &&
                  default_use->get_source_kind() ==
                      SgCxxDefaultInitExp::e_parameter);
      ROSE_ASSERT(SgCxxDefaultInitExp::is_valid_binding(
          initializer->get_declaration(), semantic_arguments, 0));
      const auto after_default = arguments->cfgForEnd().outEdges();
      ROSE_ASSERT(after_default.size() == 1 &&
                  after_default.front().target() == CFGNode(initializer, 1));
    }
  }
  if (project->get_Cxx14_only()) {
    ROSE_ASSERT(copies == 1 && moves == 4);
    ROSE_ASSERT(braced == 1 && parenthesized == 1);
    ROSE_ASSERT(closure_constructions != 0);
  } else {
    ROSE_ASSERT(project->get_Cxx17_only() || project->get_Cxx20_only());
    ROSE_ASSERT(eligible == 0);
  }
}

SgCastExp *anonymousArraySourceCast(SgExpression *source) {
  auto *access = isSgDotExp(source);
  auto *cast =
      access != nullptr ? isSgCastExp(access->get_lhs_operand()) : nullptr;
  if (cast == nullptr || cast->get_cast_type() != SgCastExp::e_static_cast)
    return nullptr;
  auto *reference = isSgRvalueReferenceType(cast->get_source_type());
  auto *type = reference != nullptr ? isSgClassType(reference->get_base_type())
                                    : nullptr;
  auto *declaration =
      type != nullptr ? isSgClassDeclaration(type->get_declaration()) : nullptr;
  if (declaration == nullptr || !declaration->get_isUnNamed())
    return nullptr;
  ROSE_ASSERT(declaration->get_firstNondefiningDeclaration() == declaration);
  ROSE_ASSERT(source->get_originalExpressionTree() == nullptr);
  ROSE_ASSERT(cast->get_originalExpressionTree() == nullptr);
  ROSE_ASSERT(cast->get_type_defining_declaration() == nullptr);
  cast->validate_semantic_conversion();
  return cast;
}

SgVarRefExp *unnamedArraySourceParameter(SgExpression *source) {
  auto *access = isSgDotExp(source);
  auto *reference =
      access != nullptr ? isSgVarRefExp(access->get_lhs_operand()) : nullptr;
  if (reference == nullptr)
    return nullptr;
  ROSE_ASSERT(reference->get_symbol() != nullptr);
  auto *parameter = reference->get_symbol()->get_declaration();
  ROSE_ASSERT(parameter != nullptr);
  if (!parameter->get_name().is_null())
    return nullptr;
  auto *parameters = isSgFunctionParameterList(parameter->get_parent());
  auto *function = parameters != nullptr
                       ? isSgMemberFunctionDeclaration(parameters->get_parent())
                       : nullptr;
  ROSE_ASSERT(function != nullptr && function->get_definition() != nullptr);
  ROSE_ASSERT(function->get_specialFunctionModifier().isConstructor());
  ROSE_ASSERT(function->get_parameterList() == parameters &&
              parameters->get_args().size() == 1 &&
              parameters->get_args().front() == parameter);
  ROSE_ASSERT(parameter->get_declptr() == function &&
              function->get_definingDeclaration() == function &&
              parameter->get_scope() == function->get_definition());
  ROSE_ASSERT(SageInterface::getEnclosingFunctionDeclaration(source) ==
              function);
  ROSE_ASSERT(reference->get_parent() == source &&
              source->get_originalExpressionTree() == nullptr &&
              reference->get_originalExpressionTree() == nullptr);
  return reference;
}

void checkArrayInitializations(SgProject *project) {
  using VirtualCFG::CFGNode;
  const auto loops = NodeQuery::querySubTree(project, V_SgCxxArrayInitLoop);
  ROSE_ASSERT(loops.size() >= 2);
  unsigned nested = 0, constructors = 0, common_references = 0, indices = 0;
  unsigned source_projections = 0;
  bool zero = false, one = false, multiple = false;
  for (SgNode *node : loops) {
    auto *loop = isSgCxxArrayInitLoop(node);
    loop->validate_bindings();
    zero |= loop->get_element_count() == 0;
    one |= loop->get_element_count() == 1;
    multiple |= loop->get_element_count() > 1;
    auto *binding = loop->get_binding();
    auto *source = binding->get_common_source();
    auto *element = loop->get_element_initializer();
    ROSE_ASSERT(binding->get_parent() == loop &&
                source->get_parent() == binding &&
                element->get_parent() == loop);
    ROSE_ASSERT(!loop->isLValue() && !loop->get_lvalue());
    if (source->isLValue()) {
      ROSE_ASSERT(source->isUsedAsLValue());
    }
    if (NodeQuery::querySubTree(source, V_SgCxxArrayInitReference).empty() &&
        anonymousArraySourceCast(source) == nullptr &&
        unnamedArraySourceParameter(source) == nullptr) {
      ROSE_ASSERT(loop->unparseToString() == source->unparseToString());
      ++source_projections;
    }
    nested += isSgCxxArrayInitLoop(element) != nullptr;
    for (SgNode *child :
         NodeQuery::querySubTree(element, V_SgConstructorInitializer)) {
      auto *constructor = isSgConstructorInitializer(child);
      ROSE_ASSERT(constructor->get_declaration() != nullptr);
      ROSE_ASSERT(constructor->get_declaration()
                      ->get_specialFunctionModifier()
                      .isConstructor());
      ROSE_ASSERT(constructor->get_args()->get_parent() == constructor);
      const auto targets =
          CallTargetSet::solveConstructorInitializer(constructor);
      ROSE_ASSERT(targets.size() == 1 &&
                  targets.front() == canonicalFunctionDeclForCallGraph(
                                         constructor->get_declaration()));
      ++constructors;
    }
    for (unsigned index = 0; index <= 4; ++index) {
      CFGNode point(loop, index);
      for (const auto &edge : point.outEdges()) {
        ROSE_ASSERT(edge.source() == point);
        const auto incoming = edge.target().inEdges();
        ROSE_ASSERT(std::find(incoming.begin(), incoming.end(), edge) !=
                    incoming.end());
      }
      for (const auto &edge : point.inEdges()) {
        ROSE_ASSERT(edge.target() == point);
        const auto outgoing = edge.source().outEdges();
        ROSE_ASSERT(std::find(outgoing.begin(), outgoing.end(), edge) !=
                    outgoing.end());
      }
    }
    const auto entry = CFGNode(loop, 0).outEdges();
    ROSE_ASSERT(entry.size() == 1 &&
                entry.front().target() == source->cfgForBeginning());
    const auto common_end = source->cfgForEnd().outEdges();
    ROSE_ASSERT(common_end.size() == 1 &&
                common_end.front().target() == CFGNode(loop, 1));
    const auto first = CFGNode(loop, 1).outEdges();
    ROSE_ASSERT(first.size() == 1);
    ROSE_ASSERT(first.front().target() ==
                CFGNode(loop, loop->get_element_count() == 0 ? 4 : 2));
    const auto first_condition = loop->get_element_count() == 0
                                     ? VirtualCFG::eckFalse
                                     : VirtualCFG::eckTrue;
    ROSE_ASSERT(first.front().condition() == first_condition);
    const auto iteration = CFGNode(loop, 3).outEdges();
    const size_t iteration_edges =
        loop->get_element_count() > 1 ? 2 : loop->get_element_count();
    ROSE_ASSERT(iteration.size() == iteration_edges);
    for (const auto &edge : iteration) {
      ROSE_ASSERT(edge.target() == CFGNode(loop, 2) ||
                  edge.target() == CFGNode(loop, 4));
      const auto condition = edge.target() == CFGNode(loop, 2)
                                 ? VirtualCFG::eckTrue
                                 : VirtualCFG::eckFalse;
      ROSE_ASSERT(edge.condition() == condition);
    }
  }
  for (SgNode *node :
       NodeQuery::querySubTree(project, V_SgCxxArrayInitReference)) {
    auto *reference = isSgCxxArrayInitReference(node);
    reference->validate();
    auto *owner = isSgCxxArrayInitLoop(reference->get_binding()->get_parent());
    ROSE_ASSERT(owner != nullptr);
    ROSE_ASSERT(
        SageInterface::isAncestor(owner->get_element_initializer(), reference));
    const bool lvalue =
        reference->get_value_category() == SgCastExp::e_value_category_lvalue;
    ROSE_ASSERT(reference->isLValue() == lvalue &&
                reference->get_lvalue() == lvalue);
    if (reference->get_reference_kind() ==
        SgCxxArrayInitReference::e_common_value) {
      ROSE_ASSERT(reference->get_value_category() ==
                  reference->get_binding()->get_common_value_category());
      ++common_references;
    } else {
      ROSE_ASSERT(reference->get_reference_kind() ==
                  SgCxxArrayInitReference::e_current_index);
      ROSE_ASSERT(reference->get_value_category() ==
                  SgCastExp::e_value_category_prvalue);
      ++indices;
    }
    ROSE_ASSERT(reference->cfgIndexForEnd() == 0);
    SgTreeCopy isolated_copier;
    auto *isolated =
        isSgCxxArrayInitReference(reference->copy(isolated_copier));
    ROSE_ASSERT(isolated != nullptr && isolated != reference);
    ROSE_ASSERT(isolated->get_binding() == reference->get_binding());
    ROSE_ASSERT(isolated_copier.get_copiedNodeMap().count(
                    reference->get_binding()) == 0);
    SageInterface::deepDelete(isolated);
  }
  ROSE_ASSERT(nested != 0 && constructors != 0 && common_references != 0 &&
              indices != 0);
  ROSE_ASSERT(zero && one && multiple);
  ROSE_ASSERT(source_projections != 0);
}
} // namespace

int main(int argc, char **argv) {
  const bool reject_anonymous_array_source =
      argc > 1 && std::strcmp(argv[1], "--reject-anonymous-array-source") == 0;
  const bool reject_unnamed_array_reference =
      argc > 1 && std::strcmp(argv[1], "--reject-unnamed-array-reference") == 0;
  const bool reject_unnamed_array_source =
      argc > 1 && std::strcmp(argv[1], "--reject-unnamed-array-source") == 0;
  const bool reject_unnamed_array_loop =
      argc > 1 && std::strcmp(argv[1], "--reject-unnamed-array-loop") == 0;
  const bool reject_array_source =
      reject_anonymous_array_source || reject_unnamed_array_reference ||
      reject_unnamed_array_source || reject_unnamed_array_loop;
  if (reject_array_source) {
    std::memmove(argv + 1, argv + 2, (argc - 1) * sizeof(argv[0]));
    --argc;
  }
  SgProject *project = frontend(argc, argv);
  ROSE_ASSERT(project != nullptr);
  if (reject_array_source) {
    for (SgNode *node :
         NodeQuery::querySubTree(project, V_SgCxxArrayInitLoop)) {
      auto *loop = isSgCxxArrayInitLoop(node);
      loop->validate_bindings();
      auto *source = loop->get_binding()->get_common_source();
      SgExpression *rejected = nullptr;
      if (reject_anonymous_array_source) {
        if (anonymousArraySourceCast(source) != nullptr)
          rejected = source;
      } else if (auto *reference = unnamedArraySourceParameter(source)) {
        rejected = reject_unnamed_array_reference ? reference
                   : reject_unnamed_array_loop    ? loop
                                                  : source;
      }
      if (rejected != nullptr) {
        (void)rejected->unparseToString();
        return 1;
      }
    }
    return 2;
  }
  checkNames(project);
  checkElision(project);
  checkArrayInitializations(project);
  std::set<SgNode *> lexical_default_calls;
  unsigned default_parameters = 0;
  for (SgNode *node : NodeQuery::querySubTree(project, V_SgInitializedName)) {
    auto *parameter = isSgInitializedName(node);
    if (isSgFunctionParameterList(parameter->get_parent()) == nullptr ||
        parameter->get_initializer() == nullptr)
      continue;
    ROSE_ASSERT(parameter->cfgIndexForEnd() == 0);
    const auto outgoing = parameter->cfgForBeginning().outEdges();
    ROSE_ASSERT(outgoing.size() == 1 &&
                outgoing.front().target().getNode() == parameter->get_parent());
    for (SgNode *call : NodeQuery::querySubTree(parameter->get_initializer(),
                                                V_SgFunctionCallExp))
      lexical_default_calls.insert(call);
    ++default_parameters;
  }
  ROSE_ASSERT(default_parameters != 0 && !lexical_default_calls.empty());
  unsigned return_edges = 0, interprocedural_returns = 0;
  unsigned interprocedural_destructions = 0;
  unsigned dependent_epilogues = 0;
  for (SgNode *node :
       NodeQuery::querySubTree(project, V_SgFunctionDefinition)) {
    auto *definition = isSgFunctionDefinition(node);
    if (definition->get_destructor_epilogue_dependent()) {
      ROSE_ASSERT(definition->get_destructor_epilogue() == nullptr);
      const VirtualCFG::CFGNode cleanup(definition, 3);
      ROSE_ASSERT(cleanup.isInteresting());
      const auto outgoing = cleanup.outEdges();
      ROSE_ASSERT(outgoing.size() == 1 &&
                  outgoing.front().target() == definition->cfgForEnd());
      const auto incoming = definition->cfgForEnd().inEdges();
      ROSE_ASSERT(incoming.size() == 1 && incoming.front() == outgoing.front());
      ROSE_ASSERT(cleanup.toString().find("Dependent cleanup(") == 0);
      ROSE_ASSERT(definition->cfgForEnd().toString().find("End(") == 0);
      ++dependent_epilogues;
    }
    if (auto *epilogue = definition->get_destructor_epilogue()) {
      StaticCFG::InterproceduralCFG graph(definition);
      for (auto *expression : epilogue->get_expressions()) {
        auto *destruction = isSgCxxSubobjectDestruction(expression);
        ROSE_ASSERT(destruction != nullptr);
        auto *callee = isSgMemberFunctionDeclaration(
            destruction->get_destructor_declaration()->get_definingDeclaration());
        ROSE_ASSERT(callee != nullptr && callee->get_definition() != nullptr);
        VirtualCFG::CFGNode call(destruction, 1);
        const auto outgoing = graph.getOutEdges(graph.toGraphNode(call));
        ROSE_ASSERT(outgoing.size() == 1 &&
                    graph.toCFGPath(outgoing.front()).target() ==
                        callee->get_definition()->cfgForBeginning());
        auto callee_exit = callee->get_definition()->cfgForEnd();
        const auto returns =
            graph.getOutEdges(graph.toGraphNode(callee_exit));
        ROSE_ASSERT(std::any_of(returns.begin(), returns.end(),
                                [&](SgDirectedGraphEdge *edge) {
                                  return graph.toCFGPath(edge).target() ==
                                         destruction->cfgForEnd();
                                }));
        ++interprocedural_destructions;
      }
    }
    if (definition->get_declaration()->get_name() == "rex_return_copy") {
      StaticCFG::InterproceduralCFG graph(definition);
      for (auto *graph_node : graph.getGraph()->computeNodeSet()) {
        for (auto *edge : graph.getOutEdges(graph_node)) {
          const auto &virtual_edge = graph.toCFGPath(edge);
          ROSE_ASSERT(graph.toCFGNode(edge->get_from()) ==
                      virtual_edge.source());
          ROSE_ASSERT(graph.toCFGNode(edge->get_to()) == virtual_edge.target());
          interprocedural_returns +=
              isSgFunctionDefinition(virtual_edge.source().getNode()) !=
                  nullptr &&
              isSgConstructorInitializer(virtual_edge.target().getNode()) !=
                  nullptr;
        }
      }
    }
    for (const auto &edge : definition->cfgForEnd().inEdges()) {
      if (auto *statement = isSgReturnStmt(edge.source().getNode())) {
        ROSE_ASSERT(SageInterface::getEnclosingProcedure(statement) ==
                    definition);
        const auto outgoing = edge.source().outEdges();
        ROSE_ASSERT(std::count(outgoing.begin(), outgoing.end(), edge) == 1);
        ++return_edges;
      }
    }
  }
  ROSE_ASSERT(return_edges != 0);
  ROSE_ASSERT(interprocedural_returns != 0);
  ROSE_ASSERT(interprocedural_destructions != 0);
  ROSE_ASSERT(dependent_epilogues != 0);
  virtualInterproceduralControlFlowGraphs = true;
  ClassHierarchyWrapper hierarchy(project);
  unsigned resolved_calls = 0, resolved_constructors = 0;
  for (SgNode *node : NodeQuery::querySubTree(project, V_SgExpression)) {
    auto *expression = isSgExpression(node);
    if (isSgFunctionCallExp(expression) == nullptr &&
        isSgConstructorInitializer(expression) == nullptr)
      continue;
    Rose_STL_Container<SgFunctionDefinition *> definitions;
    CallTargetSet::getDefinitionsForExpression(expression, &hierarchy,
                                               definitions);
    for (auto *definition : definitions) {
      using VirtualCFG::CFGEdge;
      using VirtualCFG::CFGNode;
      if (lexical_default_calls.count(expression) != 0) {
        const auto incoming = definition->cfgForBeginning().inEdges();
        const CFGEdge edge(
            CFGNode(expression, expression->cfgIndexForEnd() - 1),
            definition->cfgForBeginning());
        ROSE_ASSERT(std::count(incoming.begin(), incoming.end(), edge) == 0);
        continue;
      }
      for (const auto &edge :
           {CFGEdge(CFGNode(expression, expression->cfgIndexForEnd() - 1),
                    definition->cfgForBeginning()),
            CFGEdge(definition->cfgForEnd(), expression->cfgForEnd())}) {
        const auto outgoing = edge.source().outEdges();
        const auto incoming = edge.target().inEdges();
        ROSE_ASSERT(std::count(outgoing.begin(), outgoing.end(), edge) == 1);
        ROSE_ASSERT(std::count(incoming.begin(), incoming.end(), edge) == 1);
      }
      resolved_calls += isSgFunctionCallExp(expression) != nullptr;
      resolved_constructors +=
          isSgConstructorInitializer(expression) != nullptr;
    }
  }
  virtualInterproceduralControlFlowGraphs = false;
  ROSE_ASSERT(resolved_calls != 0 && resolved_constructors != 0);
  SgCxxArrayInitLoop *outer = nullptr;
  for (SgNode *node : NodeQuery::querySubTree(project, V_SgCxxArrayInitLoop)) {
    auto *loop = isSgCxxArrayInitLoop(node);
    if (isSgCxxArrayInitLoop(loop->get_element_initializer()) &&
        NodeQuery::querySubTree(loop->get_binding()->get_common_source(),
                                V_SgCxxArrayInitReference)
            .empty()) {
      outer = loop;
      SgExpression *element = loop->get_element_initializer();
      auto *replacement =
          isSgExpression(SageInterface::deepCopySemanticSubtree(element));
      ROSE_ASSERT(replacement != nullptr && replacement != element);
      SageInterface::replaceExpression(element, replacement);
      loop->validate_bindings();
      break;
    }
  }
  ROSE_ASSERT(outer != nullptr);
  auto *outer_copy =
      isSgCxxArrayInitLoop(SageInterface::deepCopySemanticSubtree(outer));
  ROSE_ASSERT(outer_copy != nullptr && outer_copy != outer);
  outer_copy->validate_bindings();
  ROSE_ASSERT(outer_copy->get_binding() != outer->get_binding());
  ROSE_ASSERT(outer_copy->get_binding()->get_common_source() !=
              outer->get_binding()->get_common_source());
  SageInterface::deepDelete(outer_copy);
  const auto references =
      NodeQuery::querySubTree(outer, V_SgCxxArrayInitReference);
  ROSE_ASSERT(!references.empty());
  auto *reference = isSgCxxArrayInitReference(references.front());
  auto *reference_copy = isSgCxxArrayInitReference(
      SageInterface::deepCopySemanticSubtree(reference));
  ROSE_ASSERT(reference_copy != nullptr && reference_copy != reference &&
              reference_copy->get_binding() == reference->get_binding());
  SageInterface::deepDelete(reference_copy);
  SgTreeCopy copier;
  auto *copy = isSgProject(project->copy(copier));
  ROSE_ASSERT(copy != nullptr && copy != project);
  checkNames(copy);
  checkElision(copy);
  checkArrayInitializations(copy);
  const auto &mapping = copier.get_copiedNodeMap();
  for (SgNode *node : NodeQuery::querySubTree(project, V_SgClassDefinition)) {
    if (isSgTemplateClassDefinition(node))
      continue;
    auto *original = isSgClassDefinition(node)->get_declaration()->get_type();
    auto *copied =
        isSgClassDefinition(mapping.at(node))->get_declaration()->get_type();
    const bool isUnion =
        isSgClassDefinition(node)->get_declaration()->get_class_type() ==
        SgClassDeclaration::e_union;
    ROSE_ASSERT(SageInterface::IsBaseOf(original, original) == !isUnion);
    ROSE_ASSERT(SageInterface::IsBaseOf(copied, copied) == !isUnion);
    ROSE_ASSERT(!SageInterface::IsBaseOf(original, copied));
  }
  for (SgNode *node : NodeQuery::querySubTree(project, V_SgReturnStmt)) {
    auto *original = isSgReturnStmt(node);
    auto *copied = isSgReturnStmt(mapping.at(original));
    auto *candidate = original->get_nrvo_candidate();
    ROSE_ASSERT(copied->get_nrvo_candidate() ==
                (candidate != nullptr ? mapping.at(candidate) : nullptr));
  }
  size_t indexed_fields = 0;
  for (auto *node : NodeQuery::querySubTree(project, V_SgInitializedName)) {
    auto *original = isSgInitializedName(node);
    auto *copied = isSgInitializedName(mapping.at(original));
    ROSE_ASSERT(copied != nullptr && copied != original);
    ROSE_ASSERT(copied->get_cxx_field_index() ==
                original->get_cxx_field_index());
    if (original->get_cxx_field_index())
      ++indexed_fields;
  }
  ROSE_ASSERT(indexed_fields != 0);
  for (SgNode *node :
       NodeQuery::querySubTree(project, V_SgCxxSubobjectDestruction)) {
    auto *original = isSgCxxSubobjectDestruction(node);
    auto *copied = isSgCxxSubobjectDestruction(mapping.at(original));
    ROSE_ASSERT(copied != nullptr && copied != original);
    ROSE_ASSERT(copied->get_destructor_declaration() ==
                mapping.at(original->get_destructor_declaration()));
    if (auto *member = original->get_member_target())
      ROSE_ASSERT(copied->get_member_target() == mapping.at(member));
    if (auto *base = original->get_base_target())
      ROSE_ASSERT(copied->get_base_target() == mapping.at(base));
    copied->validate();
  }
  for (SgNode *node : NodeQuery::querySubTree(project, V_SgCxxArrayInitLoop)) {
    auto *original = isSgCxxArrayInitLoop(node);
    auto *copied = isSgCxxArrayInitLoop(mapping.at(original));
    ROSE_ASSERT(copied != nullptr && copied != original);
    ROSE_ASSERT(copied->get_binding() == mapping.at(original->get_binding()));
    ROSE_ASSERT(copied->get_binding() != original->get_binding());
    ROSE_ASSERT(copied->get_binding()->get_common_value_category() ==
                original->get_binding()->get_common_value_category());
    ROSE_ASSERT(copied->get_binding()->get_common_source() ==
                mapping.at(original->get_binding()->get_common_source()));
    ROSE_ASSERT(copied->get_element_initializer() ==
                mapping.at(original->get_element_initializer()));
    ROSE_ASSERT(copied->get_element_count() == original->get_element_count());
  }
  for (SgNode *node :
       NodeQuery::querySubTree(project, V_SgCxxArrayInitReference)) {
    auto *original = isSgCxxArrayInitReference(node);
    auto *copied = isSgCxxArrayInitReference(mapping.at(original));
    ROSE_ASSERT(copied != nullptr && copied != original);
    ROSE_ASSERT(copied->get_binding() == mapping.at(original->get_binding()));
    ROSE_ASSERT(copied->get_reference_kind() == original->get_reference_kind());
    ROSE_ASSERT(copied->get_value_category() == original->get_value_category());
  }
  for (SgNode *node :
       NodeQuery::querySubTree(project, V_SgConstructorInitializer)) {
    auto *original = isSgConstructorInitializer(node);
    const auto &mapping = copier.get_copiedNodeMap();
    auto *copied = isSgConstructorInitializer(mapping.at(original));
    ROSE_ASSERT(copied != nullptr && copied != original);
    ROSE_ASSERT(copied->get_is_elidable() == original->get_is_elidable());
    ROSE_ASSERT(copied->get_elision_permitted() ==
                original->get_elision_permitted());
    if (original->get_is_elidable()) {
      ROSE_ASSERT(copied->get_args() == mapping.at(original->get_args()));
      ROSE_ASSERT(copied->get_args()->get_expressions().front() ==
                  mapping.at(original->get_args()->get_expressions().front()));
    }
  }
  auto *copied_files = copy->get_fileList_ptr();
  SageInterface::deleteAST(copy,
                           SageInterface::DeleteAstMode::kRequireIsolated);
  ROSE_ASSERT(!SgNode::isLiveNode(copied_files));
  checkNames(project);
  checkElision(project);
  checkArrayInitializations(project);
  project->skipfinalCompileStep(true);
  const int status = backend(project);
  auto *project_files = project->get_fileList_ptr();
  SageInterface::deleteAST(project,
                           SageInterface::DeleteAstMode::kRequireIsolated);
  ROSE_ASSERT(!SgNode::isLiveNode(project_files));
  return status;
}
