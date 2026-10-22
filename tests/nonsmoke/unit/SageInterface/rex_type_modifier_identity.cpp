#include "AstNodes/Expression/ExpressionTypeClassification.h"
#include "midend/programTransformation/astInlining/inlinerSupport.h"
#include "midend/programTransformation/constantFolding/constantFolding.h"
#include "midend/programTransformation/partialRedundancyElimination/pre.h"
#include "rose.h"
#include "virtualCFG.h"
#include <gtest/gtest.h>

#include <initializer_list>
#include <memory>
#include <utility>

namespace {
TEST(NamedTypeScopeIdentityDeathTest,
     UsesTypedOwnersAndRejectsMalformedChains) {
  auto *file =
      SageBuilder::buildGeneratedSourceFile("rex_named_type_scope.cpp");
  const auto deleteProject = [](SgProject *project) {
    SageInterface::deleteAST(project);
  };
  std::unique_ptr<SgProject, decltype(deleteProject)> project(
      file->get_project(), deleteProject);
  auto *global = file->get_globalScope();
  const auto makeNamespace = [](const char *name, bool anonymous,
                                SgScopeStatement *scope) {
    return SageBuilder::buildNamespaceDeclaration_nfi(
        name, anonymous, scope,
        SageBuilder::e_namespace_declaration_semantic_auxiliary, nullptr,
        nullptr, nullptr, std::nullopt);
  };
  auto *named = makeNamespace("rex_named", false, global);
  auto *anonymous = makeNamespace("rex_generated_anonymous", true, global);
  auto *nested = makeNamespace("nested", false, anonymous->get_definition());
  auto *record = SageBuilder::buildClassDeclaration(
      SageBuilder::declaration_ownership::semanticAuxiliary(), "RexNamed",
      named->get_definition());
  auto *local = SageBuilder::buildClassDeclaration(
      SageBuilder::declaration_ownership::semanticAuxiliary(), "RexLocal",
      anonymous->get_definition());
  using Rose::AST::namedTypeScopeHasCrossTranslationUnitIdentity;
  EXPECT_TRUE(namedTypeScopeHasCrossTranslationUnitIdentity(global));
  EXPECT_TRUE(
      namedTypeScopeHasCrossTranslationUnitIdentity(named->get_definition()));
  EXPECT_FALSE(anonymous->get_name().is_null());
  EXPECT_FALSE(namedTypeScopeHasCrossTranslationUnitIdentity(
      anonymous->get_definition()));
  EXPECT_FALSE(
      namedTypeScopeHasCrossTranslationUnitIdentity(nested->get_definition()));
  EXPECT_TRUE(
      namedTypeScopeHasCrossTranslationUnitIdentity(record->get_definition()));
  EXPECT_FALSE(
      namedTypeScopeHasCrossTranslationUnitIdentity(local->get_definition()));
  EXPECT_DEATH(
      {
        record->set_scope(record->get_definition());
        (void)namedTypeScopeHasCrossTranslationUnitIdentity(
            record->get_definition());
      },
      "visited.insert\\(scope\\).second");
  EXPECT_DEATH(
      {
        named->get_definition()->set_namespaceDeclaration(nullptr);
        (void)namedTypeScopeHasCrossTranslationUnitIdentity(
            named->get_definition());
      },
      "NULL pointer:.*declaration");
  EXPECT_DEATH((void)namedTypeScopeHasCrossTranslationUnitIdentity(nullptr),
               "scope != nullptr");
}

void requireDistinct(SgType *type, SgType *base) {
  ROSE_ASSERT(!SageInterface::isEquivalentType(type, base));
  ROSE_ASSERT(!SageInterface::isEquivalentType(base, type));
}

void requireSemanticModifier(void (SgTypeModifier::*set)()) {
  auto *base = SageBuilder::buildIntType();
  auto type = std::make_unique<SgModifierType>(base);
  (type->get_typeModifier().*set)();
  ROSE_ASSERT(!type->get_typeModifier().isIdentity());
  requireDistinct(type.get(), base);
}

void initializeParameterIdentity(SgTemplateType &type, unsigned offset) {
  auto location =
      std::make_unique<Sg_File_Info>("rex_type_modifier_identity.cpp", 1, 1);
  const int file = location->get_file_id();
  type.initialize_canonical_source_identity({file, offset, file, offset});
  type.set_template_parameter_depth(0);
  type.set_template_parameter_position(0);
}
} // namespace

TEST(CopyReferences, ConstructsOwnedScopeBeforeResultType) {
  auto *file =
      SageBuilder::buildGeneratedSourceFile("rex_copy_owned_result_type.cpp");
  const auto deleteProject = [](SgProject *project) {
    SageInterface::deleteAST(project);
  };
  std::unique_ptr<SgProject, decltype(deleteProject)> project(
      file->get_project(), deleteProject);
  auto *global = file->get_globalScope();
  auto *body = SageBuilder::buildBasicBlock();
  SageInterface::publishGeneratedSubtreeOutputOwner(body, global);
  body->begin_statement_expression_construction(global);
  auto *record = SageBuilder::buildClassDeclaration(
      SageBuilder::declaration_ownership::sourceLexical(), "Local", body);
  auto *variable = SageBuilder::buildVariableDeclaration(
      "value", record->get_type(), nullptr, body);
  SageInterface::appendStatement(variable, body);
  SageInterface::appendStatement(
      SageBuilder::buildExprStatement(
          SageBuilder::buildVarRefExp(variable->get_variables().front())),
      body);
  auto *expression =
      SageBuilder::buildStatementExpression(body, record->get_type(), global);
  class ScopeFirstCopy : public SgTreeCopy {
    const SgScopeStatement *scope;

  public:
    explicit ScopeFirstCopy(const SgScopeStatement *scope) : scope(scope) {}
    SgNode *copyAst(const SgNode *node) override {
      if (auto *declaration = isSgClassDeclaration(node)) {
        if (declaration->get_scope() == scope) {
          const auto &mapping = get_copiedNodeMap();
          const auto owner = mapping.find(scope);
          EXPECT_NE(owner, mapping.end());
          if (owner != mapping.end())
            EXPECT_NE(owner->second, scope);
        }
      }
      return SgTreeCopy::copyAst(node);
    }
  } bindings(body);
  auto *copy = isSgStatementExpression(expression->copy(bindings));
  const auto &mapping = bindings.get_copiedNodeMap();
  ASSERT_NE(copy, nullptr);
  EXPECT_EQ(copy->get_statement(), mapping.at(body));
  auto *copiedRecord = isSgClassDeclaration(mapping.at(record));
  ASSERT_NE(copiedRecord, nullptr);
  EXPECT_EQ(copiedRecord->get_scope(), copy->get_statement());
  EXPECT_EQ(copy->get_type(), copiedRecord->get_type());
  EXPECT_NE(copy->get_type(), expression->get_type());
  SageInterface::deleteAST(copy);
  EXPECT_TRUE(SgNode::isLiveNode(record));
  SageInterface::deleteAST(expression);
}

TEST(ReturnMutation, PreservesNamedReturnBinding) {
  auto *file = SageBuilder::buildGeneratedSourceFile("rex_return_mutation.cpp");
  const auto deleteProject = [](SgProject *project) {
    SageInterface::deleteAST(project);
  };
  std::unique_ptr<SgProject, decltype(deleteProject)> project(
      file->get_project(), deleteProject);
  auto *global = file->get_globalScope();
  auto *record = SageBuilder::buildClassDeclaration(
      SageBuilder::declaration_ownership::sourceLexical(), "Result", global);
  auto *function = SageBuilder::buildDefiningFunctionDeclaration(
      SageBuilder::function_declaration_ownership::sourceLexical(), "produce",
      record->get_type(), SageBuilder::buildFunctionParameterList_nfi(),
      global);
  auto *body = function->get_definition()->get_body();
  auto *first = SageBuilder::buildVariableDeclaration(
      "first", record->get_type(), nullptr, body);
  auto *second = SageBuilder::buildVariableDeclaration(
      "second", record->get_type(), nullptr, body);
  SageInterface::appendStatement(first, body);
  SageInterface::appendStatement(second, body);
  auto *candidate = first->get_variables().front();
  auto *statement = SageBuilder::buildReturnStmt(
      SageBuilder::buildVarRefExp(candidate), candidate);
  SageInterface::appendStatement(statement, body);
  auto *original = statement->get_expression();
  statement->set_expression(original);
  SageInterface::replaceExpression(original,
                                   SageBuilder::buildVarRefExp(candidate));
  EXPECT_EQ(statement->get_nrvo_candidate(), candidate);
  EXPECT_EQ(statement->get_expression()->get_parent(), statement);
  EXPECT_DEATH(statement->set_expression(nullptr), "is_valid_nrvo_binding");
  EXPECT_DEATH(statement->set_expression(SageBuilder::buildVarRefExp(
                   second->get_variables().front())),
               "is_valid_nrvo_binding");
  EXPECT_DEATH(
      statement->replace_expression(
          statement->get_expression(),
          SageBuilder::buildVarRefExp(second->get_variables().front())),
      "is_valid_nrvo_binding");
  EXPECT_DEATH(statement->set_return_keyword_kind(
                   SgReturnStmt::e_return_keyword_co_return),
               "nrvo_candidate");
}

TEST(AddressEscape, ReferenceLambdaCapture) {
  auto *file =
      SageBuilder::buildGeneratedSourceFile("rex_inliner_capture_escape.cpp");
  const auto deleteProject = [](SgProject *project) {
    SageInterface::deleteAST(project);
  };
  std::unique_ptr<SgProject, decltype(deleteProject)> project(
      file->get_project(), deleteProject);
  auto *function = SageBuilder::buildDefiningFunctionDeclaration(
      SageBuilder::function_declaration_ownership::sourceLexical(), "capture",
      SageBuilder::buildVoidType(),
      SageBuilder::buildFunctionParameterList_nfi(), file->get_globalScope());
  auto *body = function->get_definition()->get_body();
  auto *variable = SageBuilder::buildVariableDeclaration(
      "value", SageBuilder::buildIntType(), nullptr, body);
  SageInterface::appendStatement(variable, body);
  auto *probe = SageBuilder::buildVarRefExp(variable->get_variables().front());
  for (bool by_reference : {false, true}) {
    auto *capture = SageBuilder::buildLambdaCapture_nfi(
        SageBuilder::buildVarRefExp(variable->get_variables().front()),
        SageBuilder::buildVarRefExp(variable->get_variables().front()), nullptr,
        by_reference, false, false, false);
    EXPECT_EQ(hasAddressTaken(probe, capture), by_reference);
    SageInterface::deleteAST(capture);
  }
  auto *identity = SageBuilder::buildCastExp(
      SageBuilder::buildVarRefExp(variable->get_variables().front()),
      SageBuilder::buildIntType(), SgCastExp::e_implicit_cast,
      SgCastExp::e_semantic_conversion_NoOp, SgCastExp::e_value_category_lvalue,
      {}, nullptr);
  auto *address = SageBuilder::buildAddressOfOp(
      identity, SageBuilder::buildPointerType(SageBuilder::buildIntType()),
      SgAddressOfOp::e_address_operand_object);
  EXPECT_TRUE(hasAddressTaken(probe, address));
  SageInterface::deleteAST(address);
  auto *read = SageBuilder::buildCastExp(
      SageBuilder::buildVarRefExp(variable->get_variables().front()),
      SageBuilder::buildIntType(), SgCastExp::e_implicit_cast,
      SgCastExp::e_semantic_conversion_LValueToRValue,
      SgCastExp::e_value_category_prvalue, {}, nullptr);
  auto *copy = SageBuilder::buildCastExp(
      read, SageBuilder::buildIntType(), SgCastExp::e_static_cast,
      SgCastExp::e_semantic_conversion_NoOp,
      SgCastExp::e_value_category_prvalue, {}, SageBuilder::buildIntType());
  auto *write = SageBuilder::buildAssignOp(
      SageBuilder::buildVarRefExp(variable->get_variables().front()),
      SageBuilder::buildIntVal(1), SageBuilder::buildIntType());
  EXPECT_FALSE(isPotentiallyModified(copy, write));
  SageInterface::deleteAST(write);
  SageInterface::appendStatement(SageBuilder::buildExprStatement(copy), body);
  SageInterface::deleteAST(probe);
}

TEST(PartialRedundancy, DistinguishesWrittenStorage) {
  enum class Store {
    nested_other,
    nested_input,
    address_other,
    address_input,
    unknown_pointer,
    reference_alias,
    read_only_call,
    writing_call,
    unknown_call,
    unexposed_call,
    copied_argument_call,
    reference_argument_call,
    unexposed_pointer,
    reference_result
  };
  for (Store store :
       {Store::nested_other, Store::nested_input, Store::address_other,
        Store::address_input, Store::unknown_pointer, Store::reference_alias,
        Store::read_only_call, Store::writing_call, Store::unknown_call,
        Store::unexposed_call, Store::copied_argument_call,
        Store::reference_argument_call, Store::unexposed_pointer,
        Store::reference_result}) {
    SCOPED_TRACE(static_cast<int>(store));
    const bool unknownBody = store == Store::unknown_call ||
                             store == Store::unexposed_call ||
                             store == Store::copied_argument_call ||
                             store == Store::reference_argument_call;
    auto *file = SageBuilder::buildGeneratedSourceFile("rex_pre_storage.cpp");
    const auto deleteProject = [](SgProject *project) {
      SageInterface::deleteAST(project);
    };
    std::unique_ptr<SgProject, decltype(deleteProject)> project(
        file->get_project(), deleteProject);
    auto *integer = SageBuilder::buildIntType();
    auto *pointerType = SageBuilder::buildPointerType(integer);
    SgVariableDeclaration *sharedPointer = nullptr;
    SgFunctionDeclaration *callee = nullptr;
    if (store == Store::read_only_call || store == Store::writing_call ||
        unknownBody) {
      auto *parameters = SageBuilder::buildFunctionParameterList_nfi();
      if (store == Store::read_only_call ||
          store == Store::copied_argument_call ||
          store == Store::reference_argument_call) {
        SgType *parameterType = integer;
        if (store == Store::reference_argument_call)
          parameterType = SageBuilder::buildReferenceType(integer);
        parameters->append_arg(
            SageBuilder::buildInitializedName("value", parameterType, nullptr));
      }
      if (store == Store::writing_call) {
        sharedPointer = SageBuilder::buildVariableDeclaration(
            "shared_pointer", pointerType, nullptr, file->get_globalScope());
        SageInterface::appendStatement(sharedPointer, file->get_globalScope());
      }
      const auto ownership =
          SageBuilder::function_declaration_ownership::sourceLexical();
      callee = unknownBody ? SageBuilder::buildNondefiningFunctionDeclaration(
                                 ownership, "effect", integer, parameters,
                                 file->get_globalScope())
                           : SageBuilder::buildDefiningFunctionDeclaration(
                                 ownership, "effect", integer, parameters,
                                 file->get_globalScope());
      if (!unknownBody) {
        auto *calleeBody = callee->get_definition()->get_body();
        if (sharedPointer != nullptr)
          SageInterface::appendStatement(
              SageBuilder::buildExprStatement(SageBuilder::buildAssignOp(
                  SageBuilder::buildPointerDerefExp(
                      SageBuilder::buildVarRefExp(sharedPointer), integer),
                  SageBuilder::buildIntVal(7), integer)),
              calleeBody);
        SgExpression *value;
        if (store == Store::read_only_call) {
          value = SageBuilder::buildAddOp(
              SageBuilder::buildVarRefExp(parameters->get_args().front()),
              SageBuilder::buildIntVal(1), integer);
        } else {
          value = SageBuilder::buildIntVal(0);
        }
        SageInterface::appendStatement(SageBuilder::buildReturnStmt(value),
                                       calleeBody);
      }
    }
    auto *function = SageBuilder::buildDefiningFunctionDeclaration(
        SageBuilder::function_declaration_ownership::sourceLexical(), "store",
        SageBuilder::buildVoidType(),
        SageBuilder::buildFunctionParameterList_nfi(), file->get_globalScope());
    auto *body = function->get_definition()->get_body();
    const auto declare = [&](const char *name, SgType *type,
                             SgExpression *initial) {
      auto *declaration = SageBuilder::buildVariableDeclaration(
          name, type, SageBuilder::buildAssignInitializer(initial, type), body);
      SageInterface::appendStatement(declaration, body);
      return declaration;
    };
    auto *input = declare("input", integer, SageBuilder::buildIntVal(3));
    auto *other = declare("other", integer, SageBuilder::buildIntVal(4));
    auto *result = declare("result", integer, SageBuilder::buildIntVal(0));
    auto *pointerObject = store == Store::unexposed_pointer ||
                                  store == Store::reference_result ||
                                  store == Store::unexposed_call ||
                                  store == Store::copied_argument_call ||
                                  store == Store::reference_argument_call
                              ? other
                              : input;
    auto *pointer =
        declare("pointer", pointerType,
                SageBuilder::buildAddressOfOp(
                    SageBuilder::buildVarRefExp(pointerObject), pointerType,
                    SgAddressOfOp::e_address_operand_object));
    const auto compute = [&] {
      SageInterface::appendStatement(
          SageBuilder::buildExprStatement(SageBuilder::buildAssignOp(
              SageBuilder::buildVarRefExp(result),
              SageBuilder::buildAddOp(SageBuilder::buildVarRefExp(input),
                                      SageBuilder::buildIntVal(1), integer),
              integer)),
          body);
    };
    SgVariableDeclaration *alias = nullptr;
    if (store == Store::reference_alias || store == Store::reference_result) {
      SgExpression *initial = SageBuilder::buildVarRefExp(input);
      if (store == Store::reference_result)
        initial = SageBuilder::buildPlusAssignOp(
            initial, SageBuilder::buildIntVal(1), integer);
      alias =
          declare("alias", SageBuilder::buildReferenceType(integer), initial);
    }
    const size_t declarations =
        NodeQuery::querySubTree(body, V_SgVariableDeclaration).size();
    if (sharedPointer != nullptr)
      SageInterface::appendStatement(
          SageBuilder::buildExprStatement(SageBuilder::buildAssignOp(
              SageBuilder::buildVarRefExp(sharedPointer),
              SageBuilder::buildVarRefExp(pointer), pointerType)),
          body);
    compute();
    const bool writesOther = store == Store::nested_other ||
                             store == Store::address_other ||
                             store == Store::unexposed_pointer;
    auto *object = writesOther ? other : input;
    SgExpression *target;
    if (callee != nullptr) {
      auto *arguments = SageBuilder::buildExprListExp();
      if (store == Store::read_only_call ||
          store == Store::reference_argument_call)
        arguments->append_expression(SageBuilder::buildVarRefExp(input));
      else if (store == Store::copied_argument_call)
        arguments->append_expression(SageBuilder::buildCastExp(
            SageBuilder::buildVarRefExp(input), integer,
            SgCastExp::e_implicit_cast,
            SgCastExp::e_semantic_conversion_LValueToRValue,
            SgCastExp::e_value_category_prvalue, {}, nullptr));
      target = SageBuilder::buildFunctionCallExp(
          SageBuilder::buildFunctionRefExp(callee), integer, arguments);
    } else if (store == Store::reference_alias ||
               store == Store::reference_result) {
      target = SageBuilder::buildVarRefExp(alias);
    } else if (store == Store::nested_other || store == Store::nested_input) {
      target =
          SageBuilder::buildPlusAssignOp(SageBuilder::buildVarRefExp(object),
                                         SageBuilder::buildIntVal(1), integer);
    } else {
      SgExpression *address;
      if (store == Store::unknown_pointer ||
          store == Store::unexposed_pointer) {
        address = SageBuilder::buildVarRefExp(pointer);
      } else {
        address = SageBuilder::buildAddOp(
            SageBuilder::buildAddressOfOp(
                SageBuilder::buildVarRefExp(object), pointerType,
                SgAddressOfOp::e_address_operand_object),
            SageBuilder::buildIntVal(0), pointerType);
      }
      target = SageBuilder::buildPointerDerefExp(address, integer);
    }
    SageInterface::appendStatement(
        SageBuilder::buildExprStatement(
            callee != nullptr
                ? target
                : SageBuilder::buildPlusAssignOp(
                      target, SageBuilder::buildIntVal(1), integer)),
        body);
    compute();
    legacy::PRE::partialRedundancyEliminationFunction(
        function->get_definition());
    EXPECT_EQ(NodeQuery::querySubTree(body, V_SgVariableDeclaration).size(),
              declarations + (writesOther || store == Store::read_only_call ||
                                      store == Store::unexposed_call ||
                                      store == Store::copied_argument_call
                                  ? 1u
                                  : 0u));
  }
}

TEST(PartialRedundancy, CachesCallOperandPrefix) {
  for (unsigned form = 0; form < 6; ++form) {
    SCOPED_TRACE(form);
    auto *file =
        SageBuilder::buildGeneratedSourceFile("rex_pre_call_prefix.cpp");
    const auto deleteProject = [](SgProject *project) {
      SageInterface::deleteAST(project);
    };
    std::unique_ptr<SgProject, decltype(deleteProject)> project(
        file->get_project(), deleteProject);
    auto *integer = SageBuilder::buildIntType();
    auto *parameters = SageBuilder::buildFunctionParameterList_nfi();
    parameters->append_arg(
        SageBuilder::buildInitializedName("value", integer, nullptr));
    auto *callee = SageBuilder::buildNondefiningFunctionDeclaration(
        SageBuilder::function_declaration_ownership::sourceLexical(), "effect",
        integer, parameters, file->get_globalScope());
    auto *function = SageBuilder::buildDefiningFunctionDeclaration(
        SageBuilder::function_declaration_ownership::sourceLexical(), "prefix",
        SageBuilder::buildVoidType(),
        SageBuilder::buildFunctionParameterList_nfi(), file->get_globalScope());
    auto *body = function->get_definition()->get_body();
    const bool blocked = form == 2 || form == 3;
    auto *input = SageBuilder::buildVariableDeclaration(
        "input", integer,
        SageBuilder::buildAssignInitializer(
            SageBuilder::buildIntVal(blocked ? 0 : 1), integer),
        body);
    SageInterface::appendStatement(input, body);
    const auto divide = [&] {
      return SageBuilder::buildDivideOp(SageBuilder::buildIntVal(42),
                                        SageBuilder::buildVarRefExp(input),
                                        integer);
    };
    const auto call = [&](SgExpression *argument) {
      SgExpression *reference = SageBuilder::buildFunctionRefExp(callee);
      if (form == 3)
        reference = SageBuilder::buildCastExp(
            reference, SageBuilder::buildPointerType(callee->get_type()),
            SgCastExp::e_implicit_cast,
            SgCastExp::e_semantic_conversion_BuiltinFnToFnPtr,
            SgCastExp::e_value_category_prvalue, {}, nullptr);
      return SageBuilder::buildFunctionCallExp(
          reference, integer, SageBuilder::buildExprListExp(argument));
    };
    auto *first = divide();
    SgExpression *argument = SageBuilder::buildAddOp(first, divide(), integer);
    if (form == 1)
      argument = call(argument);
    else if (form == 2)
      argument = SageBuilder::buildCommaOpExp(call(SageBuilder::buildIntVal(0)),
                                              argument, integer);
    auto *operation = call(argument);
    SgForStatement *loop = nullptr;
    SgVariableDeclaration *declaration = nullptr;
    SgDeclarationGroupStatement *group = nullptr;
    SgVariableSymbol *symbol = nullptr;
    SgVariableSymbol *extraSymbol = nullptr;
    if (form >= 4) {
      loop = SageBuilder::buildForStatement(
          SageBuilder::buildForInitStatement(SageBuilder::buildNullStatement()),
          SageBuilder::buildExprStatement(SageBuilder::buildBoolValExp(false)),
          SageBuilder::buildNullExpression(
              SgNullExpression::e_null_expression_syntactic_absence),
          SageBuilder::buildBasicBlock());
      SageInterface::appendStatement(loop, body);
      declaration = SageBuilder::buildVariableDeclaration(
          "value", integer,
          SageBuilder::buildAssignInitializer(operation, integer), loop);
      if (form == 5) {
        group = new SgDeclarationGroupStatement();
        SageInterface::setOneSourcePositionForTransformation(group);
        group->set_scope(loop);
        group->set_source_terminator(
            SgDeclarationGroupStatement::e_source_terminator_file_semicolon);
        auto *extra = SageBuilder::buildVariableDeclaration(
            "extra", integer,
            SageBuilder::buildAssignInitializer(SageBuilder::buildIntVal(1),
                                                integer),
            loop);
        extra->set_parent(nullptr);
        declaration->set_parent(nullptr);
        group->append_declaration(extra);
        group->append_declaration(declaration);
        extraSymbol = SageInterface::getFirstVarSym(extra);
        SageInterface::appendStatement(group, loop->get_for_init_stmt());
      } else {
        SageInterface::appendStatement(declaration, loop->get_for_init_stmt());
      }
      symbol = SageInterface::getFirstVarSym(declaration);
    } else {
      SageInterface::appendStatement(SageBuilder::buildExprStatement(operation),
                                     body);
    }
    EXPECT_EQ(legacy::countComputationsOfExpressionIn(first, operation),
              form == 3 ? 0 : 2);
    const auto declarations =
        NodeQuery::querySubTree(body, V_SgVariableDeclaration).size();
    legacy::PRE::partialRedundancyEliminationFunction(
        function->get_definition());
    EXPECT_EQ(NodeQuery::querySubTree(body, V_SgVariableDeclaration).size(),
              declarations + (blocked ? 0u : 1u));
    if (loop != nullptr) {
      auto *scope = isSgBasicBlock(loop->get_parent());
      ASSERT_NE(scope, nullptr);
      EXPECT_EQ(group != nullptr ? group->get_parent()
                                 : declaration->get_parent(),
                scope);
      EXPECT_EQ(declaration->get_variables().front()->get_scope(), scope);
      EXPECT_EQ(SageInterface::getFirstVarSym(declaration), symbol);
      EXPECT_EQ(symbol->get_parent(), scope->get_symbol_table());
      if (group != nullptr) {
        group->validate();
        EXPECT_EQ(group->get_scope(), scope);
        EXPECT_EQ(extraSymbol->get_parent(), scope->get_symbol_table());
        EXPECT_EQ(extraSymbol->get_declaration()->get_scope(), scope);
      }
      ASSERT_EQ(loop->get_init_stmt().size(), 1u);
      auto *absence = isSgNullStatement(loop->get_init_stmt().front());
      ASSERT_NE(absence, nullptr);
      EXPECT_EQ(absence->get_file_info()->get_physical_file_id(),
                loop->get_file_info()->get_physical_file_id());
    }
  }
}

TEST(PartialRedundancy, PreservesConditionalAndUnevaluatedOperands) {
  const auto divide = [] {
    return SageBuilder::buildDivideOp(SageBuilder::buildIntVal(42),
                                      SageBuilder::buildIntVal(2),
                                      SageBuilder::buildIntType());
  };
  for (bool selectedTrue : {false, true}) {
    auto *file = SageBuilder::buildGeneratedSourceFile(
        selectedTrue ? "rex_pre_choice_condition_true.cpp"
                     : "rex_pre_choice_condition_false.cpp");
    const auto deleteProject = [](SgProject *project) {
      SageInterface::deleteAST(project);
    };
    std::unique_ptr<SgProject, decltype(deleteProject)> project(
        file->get_project(), deleteProject);
    auto *integer = SageBuilder::buildIntType();
    auto *function = SageBuilder::buildDefiningFunctionDeclaration(
        SageBuilder::function_declaration_ownership::sourceLexical(), "choice",
        integer, SageBuilder::buildFunctionParameterList_nfi(),
        file->get_globalScope());
    auto *condition = SageBuilder::buildEqualityOp(
        divide(), SageBuilder::buildIntVal(selectedTrue ? 21 : 0),
        SageBuilder::buildBoolType());
    SgExpression *selected =
        SageBuilder::buildAddOp(divide(), divide(), integer);
    SgExpression *discarded = divide();
    auto *choice = SageBuilder::buildChooseExpression_nfi(
        condition, selectedTrue ? selected : discarded,
        selectedTrue ? discarded : selected, integer,
        selectedTrue ? SgChooseExpression::e_choose_selection_true
                     : SgChooseExpression::e_choose_selection_false);
    SageBuilder::initializeSemanticExpressionSourceProvenance(choice);
    auto *body = function->get_definition()->get_body();
    SageInterface::appendStatement(SageBuilder::buildReturnStmt(choice), body);
    legacy::PRE::myControlFlowGraph cfg;
    legacy::PRE::makeCfg(function->get_definition(), cfg);
    legacy::PRE::addEdgeInsertionPoints(cfg);
    std::vector<SgExpression *> retired;
    legacy::PRE::partialRedundancyEliminationOne(
        isSgAddOp(selected)->get_lhs_operand(), body, cfg, retired);
    EXPECT_EQ(NodeQuery::querySubTree(body, V_SgVariableDeclaration).size(),
              1u);
    EXPECT_EQ(NodeQuery::querySubTree(condition, V_SgDivideOp).size(), 1u);
    EXPECT_EQ(NodeQuery::querySubTree(selectedTrue
                                          ? choice->get_false_expression()
                                          : choice->get_true_expression(),
                                      V_SgDivideOp)
                  .size(),
              1u);
    EXPECT_TRUE(NodeQuery::querySubTree(selected, V_SgDivideOp).empty());
    for (SgExpression *expression : retired)
      SageInterface::deleteAST(expression,
                               SageInterface::DeleteAstMode::kRequireIsolated);
  }
  for (bool selectedTrue : {false, true}) {
    auto *probe = divide();
    auto *condition = SageBuilder::buildBoolValExp(selectedTrue);
    const auto selection = selectedTrue
                               ? SgChooseExpression::e_choose_selection_true
                               : SgChooseExpression::e_choose_selection_false;
    auto *choice = SageBuilder::buildChooseExpression_nfi(
        condition,
        SageBuilder::buildAddOp(probe, divide(), SageBuilder::buildIntType()),
        SageBuilder::buildIntVal(0), SageBuilder::buildIntType(), selection);
    SageBuilder::initializeSemanticExpressionSourceProvenance(choice);
    EXPECT_EQ(legacy::countComputationsOfExpressionIn(probe, choice),
              selectedTrue ? 2 : 0);
    EXPECT_EQ(legacy::countComputationsOfExpressionIn(condition, choice), 0);
    SgTreeCopy copying;
    auto *copy = isSgChooseExpression(choice->copy(copying));
    ASSERT_NE(copy, nullptr);
    copy->validate();
    EXPECT_EQ(copy->get_selection_kind(), selection);
    EXPECT_NE(copy->get_condition(), condition);
    SageInterface::deleteAST(copy);
    auto *statement = SageBuilder::buildExprStatement(choice);
    auto *selected = selectedTrue ? choice->get_true_expression()
                                  : choice->get_false_expression();
    auto *discarded = selectedTrue ? choice->get_false_expression()
                                   : choice->get_true_expression();
    const auto verifyEdge = [](VirtualCFG::CFGNode from,
                               VirtualCFG::CFGNode to) {
      const auto outgoing = from.outEdges();
      ASSERT_EQ(outgoing.size(), 1u);
      EXPECT_EQ(outgoing.front().target(), to);
      const auto incoming = to.inEdges();
      ASSERT_EQ(incoming.size(), 1u);
      EXPECT_EQ(incoming.front().source(), from);
    };
    verifyEdge(statement->cfgForBeginning(), choice->cfgForBeginning());
    verifyEdge(choice->cfgForBeginning(), selected->cfgForBeginning());
    verifyEdge(selected->cfgForEnd(), choice->cfgForEnd());
    verifyEdge(choice->cfgForEnd(), statement->cfgForEnd());
    EXPECT_TRUE(condition->cfgForBeginning().inEdges().empty());
    EXPECT_TRUE(condition->cfgForEnd().outEdges().empty());
    EXPECT_TRUE(discarded->cfgForBeginning().inEdges().empty());
    EXPECT_TRUE(discarded->cfgForEnd().outEdges().empty());
    const auto edges = choice->cfgForBeginning().outEdges();
    ASSERT_EQ(edges.size(), 1u);
    const auto &edge = edges.front();
    EXPECT_EQ(edge.condition(), VirtualCFG::eckUnconditional);
    EXPECT_EQ(edge.conditionBasedOn(), nullptr);
    SageInterface::deleteAST(statement);
  }
  {
    auto *parameter = SageBuilder::buildTemplateParameter(
        SgTemplateParameter::nontype_parameter, SageBuilder::buildIntType(),
        SgName("rex_choice"), nullptr,
        SgTemplateParameter::keyword_unspecified);
    auto *condition = SageBuilder::buildTemplateParameterVal(0, parameter);
    auto *choice = SageBuilder::buildChooseExpression_nfi(
        condition, SageBuilder::buildIntVal(2), SageBuilder::buildIntVal(3),
        SageBuilder::buildIntType(),
        SgChooseExpression::e_choose_selection_dependent);
    SageBuilder::initializeSemanticExpressionSourceProvenance(choice);
    auto *statement = SageBuilder::buildExprStatement(choice);
    const auto entry = choice->cfgForBeginning();
    const auto exit = choice->cfgForEnd();
    EXPECT_TRUE(entry.isInteresting());
    const auto outgoing = entry.outEdges();
    const auto incoming = exit.inEdges();
    ASSERT_EQ(outgoing.size(), 2u);
    ASSERT_EQ(incoming.size(), 2u);
    for (unsigned arm = 0; arm < 2; ++arm) {
      auto *expression = arm == 0 ? choice->get_true_expression()
                                  : choice->get_false_expression();
      EXPECT_EQ(outgoing[arm].target(), expression->cfgForBeginning());
      EXPECT_EQ(incoming[arm].source(), expression->cfgForEnd());
      EXPECT_EQ(outgoing[arm].condition(),
                arm == 0 ? VirtualCFG::eckTrue : VirtualCFG::eckFalse);
      EXPECT_EQ(outgoing[arm].conditionBasedOn(), condition);
      const auto before = expression->cfgForBeginning().inEdges();
      const auto after = expression->cfgForEnd().outEdges();
      ASSERT_EQ(before.size(), 1u);
      ASSERT_EQ(after.size(), 1u);
      EXPECT_EQ(before.front().source(), entry);
      EXPECT_EQ(after.front().target(), exit);
    }
    EXPECT_TRUE(condition->cfgForBeginning().inEdges().empty());
    EXPECT_TRUE(condition->cfgForEnd().outEdges().empty());
    SageInterface::deleteAST(statement);
    SageInterface::deleteAST(parameter);
  }
  for (unsigned form = 0; form < 5; ++form) {
    SCOPED_TRACE(form);
    auto *probe = divide();
    auto *body = SageBuilder::buildExprStatement(
        SageBuilder::buildAddOp(probe, divide(), SageBuilder::buildIntType()));
    auto *condition = SageBuilder::buildBoolValExp(false);
    SgExpression *initial = nullptr;
    SgStatement *statement;
    if (form < 3) {
      SgExprStatement *otherwise = nullptr;
      if (form == 1)
        otherwise = SageBuilder::buildExprStatement(divide());
      else if (form == 2)
        otherwise =
            SageBuilder::buildExprStatement(SageBuilder::buildIntVal(0));
      statement = SageBuilder::buildIfStmt(condition, body, otherwise);
    } else if (form == 3) {
      statement = SageBuilder::buildWhileStmt(condition, body);
    } else {
      initial = SageBuilder::buildMultiplyOp(SageBuilder::buildIntVal(7),
                                             SageBuilder::buildIntVal(3),
                                             SageBuilder::buildIntType());
      statement = SageBuilder::buildForStatement(
          SageBuilder::buildForInitStatement(
              SageBuilder::buildExprStatement(initial)),
          SageBuilder::buildExprStatement(condition),
          SageBuilder::buildNullExpression(
              SgNullExpression::e_null_expression_syntactic_absence),
          body);
    }
    EXPECT_EQ(legacy::countComputationsOfExpressionIn(probe, statement),
              form == 1 ? 1 : 0);
    EXPECT_EQ(legacy::countComputationsOfExpressionIn(condition, statement), 1);
    if (initial != nullptr)
      EXPECT_EQ(legacy::countComputationsOfExpressionIn(initial, statement), 1);
    SageInterface::deleteAST(statement);
  }
  for (bool isConsteval : {false, true}) {
    for (bool selectedTrue : {false, true}) {
      SCOPED_TRACE(isConsteval);
      SCOPED_TRACE(selectedTrue);
      auto *probe = divide();
      SgExpression *condition = nullptr;
      SgStatement *conditionStatement = nullptr;
      if (!isConsteval) {
        condition = SageBuilder::buildBoolValExp(selectedTrue);
        conditionStatement = SageBuilder::buildExprStatement(condition);
      }
      auto *statement = SageBuilder::buildIfStmt_nfi(
          conditionStatement,
          SageBuilder::buildBasicBlock(
              SageBuilder::buildExprStatement(SageBuilder::buildAddOp(
                  probe, divide(), SageBuilder::buildIntType()))),
          SageBuilder::buildBasicBlock(
              SageBuilder::buildExprStatement(divide())));
      if (isConsteval) {
        statement->set_if_statement_kind(selectedTrue
                                             ? SgIfStmt::e_if_not_consteval
                                             : SgIfStmt::e_if_consteval);
      } else {
        statement->set_if_statement_kind(SgIfStmt::e_if_constexpr);
        statement->set_if_constexpr_value_known(true);
        statement->set_if_constexpr_value(selectedTrue);
      }
      EXPECT_EQ(legacy::countComputationsOfExpressionIn(probe, statement),
                selectedTrue ? 2 : 1);
      if (condition != nullptr)
        EXPECT_EQ(legacy::countComputationsOfExpressionIn(condition, statement),
                  0);
      SageInterface::deleteAST(statement);
    }
  }
  const char *forms[] = {"and",
                         "or",
                         "conditional",
                         "both_arms",
                         "sizeof",
                         "noexcept",
                         "gnu_conditional",
                         "writing_comma",
                         "nonreturning_comma",
                         "nonreturning_branch"};
  for (unsigned form = 0; form < 10; ++form) {
    SCOPED_TRACE(forms[form]);
    auto *file = SageBuilder::buildGeneratedSourceFile(
        std::string("rex_pre_runtime_evaluation_") + forms[form] + ".cpp");
    file->set_target_size_type(SageBuilder::buildUnsignedLongType());
    const auto deleteProject = [](SgProject *project) {
      SageInterface::deleteAST(project);
    };
    std::unique_ptr<SgProject, decltype(deleteProject)> project(
        file->get_project(), deleteProject);
    auto *integer = SageBuilder::buildIntType();
    SgFunctionDeclaration *stop = nullptr;
    SgVariableDeclaration *takeStop = nullptr;
    if (form >= 8) {
      stop = SageBuilder::buildDefiningFunctionDeclaration(
          SageBuilder::function_declaration_ownership::sourceLexical(),
          "rex_stop", SageBuilder::buildVoidType(),
          SageBuilder::buildFunctionParameterList_nfi(),
          file->get_globalScope());
      auto *stopBody = stop->get_definition()->get_body();
      auto *volatileInteger = SageBuilder::buildVolatileType(integer);
      auto *sink = SageBuilder::buildVariableDeclaration(
          "sink", volatileInteger, nullptr, stopBody);
      SageInterface::appendStatement(sink, stopBody);
      auto *loopBody = SageBuilder::buildBasicBlock();
      SageInterface::appendStatement(
          SageBuilder::buildWhileStmt(SageBuilder::buildBoolValExp(true),
                                      loopBody),
          stopBody);
      SageInterface::appendStatement(
          SageBuilder::buildExprStatement(SageBuilder::buildAssignOp(
              SageBuilder::buildVarRefExp(sink), SageBuilder::buildIntVal(0),
              volatileInteger)),
          loopBody);
    }
    if (form == 9) {
      auto *volatileBool =
          SageBuilder::buildVolatileType(SageBuilder::buildBoolType());
      takeStop = SageBuilder::buildVariableDeclaration(
          "take_stop", volatileBool,
          SageBuilder::buildAssignInitializer(
              SageBuilder::buildBoolValExp(true), volatileBool),
          file->get_globalScope());
      SageInterface::appendStatement(takeStop, file->get_globalScope());
    }
    auto *function = SageBuilder::buildDefiningFunctionDeclaration(
        SageBuilder::function_declaration_ownership::sourceLexical(), "main",
        integer, SageBuilder::buildFunctionParameterList_nfi(),
        file->get_globalScope());
    auto *body = function->get_definition()->get_body();
    auto *divisor = SageBuilder::buildVariableDeclaration(
        "divisor", integer,
        SageBuilder::buildAssignInitializer(
            SageBuilder::buildIntVal(form == 3 || form == 7 ? 1 : 0), integer),
        body);
    SageInterface::appendStatement(divisor, body);
    const auto divide = [&] {
      SgExpression *denominator;
      if (form >= 8)
        denominator = SageBuilder::buildIntVal(0);
      else
        denominator = SageBuilder::buildVarRefExp(divisor);
      return SageBuilder::buildDivideOp(SageBuilder::buildIntVal(42),
                                        denominator, integer);
    };
    auto *first = divide();
    auto *second = divide();
    SgExpression *expression;
    if (form == 3) {
      expression = SageBuilder::buildConditionalExp(
          SageBuilder::buildBoolValExp(false), first, second, integer);
    } else if (form == 7) {
      auto *write =
          SageBuilder::buildAssignOp(SageBuilder::buildVarRefExp(divisor),
                                     SageBuilder::buildIntVal(2), integer);
      expression = SageBuilder::buildCommaOpExp(
          first, SageBuilder::buildCommaOpExp(write, second, integer), integer);
    } else if (form == 9) {
      auto *trueBody = SageBuilder::buildBasicBlock();
      auto *falseBody = SageBuilder::buildBasicBlock();
      SageInterface::appendStatement(
          SageBuilder::buildIfStmt(SageBuilder::buildVarRefExp(takeStop),
                                   trueBody, falseBody),
          body);
      SageInterface::appendStatement(
          SageBuilder::buildExprStatement(SageBuilder::buildFunctionCallExp(
              SageBuilder::buildFunctionRefExp(stop),
              SageBuilder::buildVoidType(), SageBuilder::buildExprListExp())),
          trueBody);
      SageInterface::appendStatement(SageBuilder::buildExprStatement(first),
                                     falseBody);
      expression = second;
    } else {
      auto *sum = SageBuilder::buildAddOp(first, second, integer);
      switch (form) {
      case 0:
        expression =
            SageBuilder::buildAndOp(SageBuilder::buildBoolValExp(false), sum,
                                    SageBuilder::buildBoolType());
        break;
      case 1:
        expression = SageBuilder::buildOrOp(SageBuilder::buildBoolValExp(true),
                                            sum, SageBuilder::buildBoolType());
        break;
      case 2:
        expression = SageBuilder::buildConditionalExp(
            SageBuilder::buildBoolValExp(false), sum,
            SageBuilder::buildIntVal(0), integer);
        break;
      case 4:
        expression =
            SageBuilder::buildSizeOfOp(sum, file->get_target_size_type());
        break;
      case 5:
        expression = SageBuilder::buildNoexceptOp(sum);
        break;
      case 6:
        expression = SageBuilder::buildBinaryConditionalExp(
            SageBuilder::buildIntVal(1), sum, integer);
        break;
      case 8:
        expression = SageBuilder::buildCommaOpExp(
            SageBuilder::buildFunctionCallExp(
                SageBuilder::buildFunctionRefExp(stop),
                SageBuilder::buildVoidType(), SageBuilder::buildExprListExp()),
            sum, integer);
        break;
      default:
        ROSE_ABORT();
      }
    }
    SgExpression *result;
    if (form == 7) {
      result = SageBuilder::buildConditionalExp(
          SageBuilder::buildEqualityOp(expression, SageBuilder::buildIntVal(21),
                                       SageBuilder::buildBoolType()),
          SageBuilder::buildIntVal(0), SageBuilder::buildIntVal(1), integer);
    } else {
      SageInterface::appendStatement(
          SageBuilder::buildExprStatement(expression), body);
      result = SageBuilder::buildIntVal(0);
    }
    SageInterface::appendStatement(SageBuilder::buildReturnStmt(result), body);
    EXPECT_EQ(legacy::countComputationsOfExpressionIn(first, expression),
              form == 3 || form == 9 ? 1
              : form >= 7            ? 2
                                     : 0);
    legacy::PRE::partialRedundancyEliminationFunction(
        function->get_definition());
    // A cache after the nonreturning call is valid; native execution checks
    // placement.
    if (form != 9)
      EXPECT_EQ(NodeQuery::querySubTree(body, V_SgVariableDeclaration).size(),
                1u);
    file->unparse();
  }
}

TEST(PartialRedundancy, AtomicReadsAreNotEquivalent) {
  auto *file = SageBuilder::buildGeneratedSourceFile("rex_pre_atomic.c");
  const auto deleteProject = [](SgProject *project) {
    SageInterface::deleteAST(project);
  };
  std::unique_ptr<SgProject, decltype(deleteProject)> project(
      file->get_project(), deleteProject);
  SgTypeModifier atomic;
  atomic.setAtomic();
  auto *declaration = SageBuilder::buildVariableDeclaration(
      "value",
      SageBuilder::buildModifierType(SageBuilder::buildIntType(), atomic),
      nullptr, file->get_globalScope());
  SageInterface::appendStatement(declaration, file->get_globalScope());
  auto *first = SageBuilder::buildVarRefExp(declaration);
  auto *second = SageBuilder::buildVarRefExp(declaration);
  EXPECT_FALSE(expressionTreeEqual(first, first));
  EXPECT_FALSE(expressionTreeEqual(first, second));
  SageInterface::deleteAST(first);
  SageInterface::deleteAST(second);
}

TEST(PartialRedundancy, FloatingLiteralIdentity) {
  const auto distinguishZeros = [](auto positive, auto negative) {
    EXPECT_TRUE(expressionTreeEqual(positive.get(), positive.get()));
    EXPECT_TRUE(expressionTreeEqual(negative.get(), negative.get()));
    EXPECT_FALSE(expressionTreeEqual(positive.get(), negative.get()));
    EXPECT_FALSE(expressionTreeEqual(negative.get(), positive.get()));
  };
  distinguishZeros(std::make_unique<SgFloatVal>(0.0f, ""),
                   std::make_unique<SgFloatVal>(-0.0f, ""));
  distinguishZeros(std::make_unique<SgDoubleVal>(0.0, ""),
                   std::make_unique<SgDoubleVal>(-0.0, ""));
  distinguishZeros(std::make_unique<SgLongDoubleVal>(0.0L, ""),
                   std::make_unique<SgLongDoubleVal>(-0.0L, ""));
}

TEST(ConstantFolding, RetiresNestedReplacements) {
  auto *integer = SageBuilder::buildIntType();
  const auto values = SgIntVal::numberOfNodes();
  const auto additions = SgAddOp::numberOfNodes();
  const auto initializers = SgAssignInitializer::numberOfNodes();
  for (int form = 0; form < 5; ++form) {
    SCOPED_TRACE(form);
    for (int depth = 1; depth <= 4; ++depth) {
      SCOPED_TRACE(depth);
      SgExpression *expression = SageBuilder::buildIntVal(0);
      for (int level = 0; level < depth; ++level)
        expression = SageBuilder::buildAddOp(
            expression, SageBuilder::buildIntVal(1), integer);
      SgNode *owner = expression;
      if (form == 0)
        owner = SageBuilder::buildAssignInitializer(expression, integer);
      else if (form == 1)
        owner = SageBuilder::buildExprStatement(expression);
      else if (form == 2)
        owner = SageBuilder::buildReturnStmt(expression);
      if (form == 4) {
        auto *folded = isSgIntVal(
            ConstantFolding::returnConstantFoldedValueExpression(expression));
        ASSERT_NE(folded, nullptr);
        EXPECT_EQ(folded->get_value(), depth);
        EXPECT_EQ(folded->get_parent(), nullptr);
        SageInterface::deleteAST(folded);
      } else {
        ConstantFolding::constantFoldingOptimization(owner);
        if (form < 3) {
          auto *folded = isSgIntVal(owner->get_traversalSuccessorByIndex(0));
          EXPECT_NE(folded, nullptr);
          if (folded != nullptr) {
            EXPECT_EQ(folded->get_value(), depth);
            EXPECT_EQ(folded->get_parent(), owner);
            ConstantFolding::constantFoldingOptimization(owner);
            EXPECT_EQ(owner->get_traversalSuccessorByIndex(0), folded);
          }
        }
      }
      SageInterface::deleteAST(owner);
      EXPECT_EQ(SgIntVal::numberOfNodes(), values);
      EXPECT_EQ(SgAddOp::numberOfNodes(), additions);
      EXPECT_EQ(SgAssignInitializer::numberOfNodes(), initializers);
    }
  }
}

TEST(FunctionParameterTypes, PreservesPrototypeAndReferenceUseTypes) {
  auto *integer = SageBuilder::buildIntType();
  const auto signature = [&](bool prototype, SgType *parameter = nullptr) {
    auto arguments = std::make_unique<SgFunctionParameterTypeList>();
    if (parameter)
      arguments->append_argument(parameter);
    auto *type =
        SageBuilder::buildFunctionType(integer, arguments.get(), prototype);
    if (type->get_argument_list() == arguments.get())
      arguments.release();
    return type;
  };
  auto *unprototyped = signature(false);
  auto *empty = signature(true);
  auto *prototype = signature(true, integer);
  EXPECT_NE(unprototyped, empty);
  EXPECT_FALSE(SageInterface::isEquivalentType(unprototyped, empty));
  EXPECT_EQ(signature(false), unprototyped);
  EXPECT_TRUE(
      SageInterface::isFunctionPrototypeRefinement(unprototyped, empty));
  EXPECT_TRUE(
      SageInterface::isFunctionPrototypeRefinement(unprototyped, prototype));
  for (SgType *parameter : std::initializer_list<SgType *>{
           SageBuilder::buildDoubleType(), SageBuilder::buildFloat16Type(),
           SageBuilder::buildBFloat16Type()})
    EXPECT_TRUE(SageInterface::isFunctionPrototypeRefinement(
        unprototyped, signature(true, parameter)));
  for (SgType *parameter : std::initializer_list<SgType *>{
           SageBuilder::buildFloatType(), SageBuilder::buildCharType(),
           SageBuilder::buildShortType()})
    EXPECT_FALSE(SageInterface::isFunctionPrototypeRefinement(
        unprototyped, signature(true, parameter)));
  auto *file = SageBuilder::buildGeneratedSourceFile("rex_reference.c");
  const auto deleteProject = [](SgProject *project) {
    SageInterface::deleteAST(project);
  };
  std::unique_ptr<SgProject, decltype(deleteProject)> project(
      file->get_project(), deleteProject);
  auto *scope = file->get_globalScope();
  auto *first = SageBuilder::buildNondefiningFunctionDeclaration(
      SageBuilder::function_declaration_ownership::sourceLexical()
          .withFunctionType(unprototyped),
      "rex_reference", integer, SageBuilder::buildFunctionParameterList(),
      scope);
  auto *later = SageBuilder::buildDefiningFunctionDeclaration(
      SageBuilder::function_declaration_ownership::sourceLexicalRedeclarationIn(
          scope, first)
          .withFunctionType(prototype),
      "rex_reference", integer,
      SageBuilder::buildFunctionParameterList(
          SageBuilder::buildInitializedName("value", integer)),
      scope);
  {
    SgFunctionSymbol symbol(first);
    auto *before = SageBuilder::buildFunctionRefExp_nfi(&symbol, unprototyped);
    auto *after = SageBuilder::buildFunctionRefExp_nfi(&symbol, prototype);
    SageBuilder::initializeSemanticExpressionSourceProvenance(before);
    symbol.set_declaration(later);
    EXPECT_EQ(before->get_type(), unprototyped);
    EXPECT_EQ(after->get_type(), prototype);
    SgTreeCopy copying;
    auto *copy = isSgFunctionRefExp(before->copy(copying));
    ASSERT_NE(copy, nullptr);
    EXPECT_EQ(copy->get_type(), unprototyped);
    SageInterface::deleteAST({before, after, copy},
                             SageInterface::DeleteAstMode::kRequireIsolated);
  }
}

TEST(TypeTraits, CRecordMemberQualifications) {
  auto *file = SageBuilder::buildGeneratedSourceFile("rex_traits_c_fields.c");
  const auto deleteProject = [](SgProject *project) {
    SageInterface::deleteAST(project);
  };
  std::unique_ptr<SgProject, decltype(deleteProject)> project(
      file->get_project(), deleteProject);
  ASSERT_EQ(file->get_inputLanguage(), SgFile::e_C_language);
  auto *const_int = SageBuilder::buildConstType(SageBuilder::buildIntType());
  const struct {
    SgType *type;
    bool assignable;
  } cases[] = {
      {SageBuilder::buildArrayType(SageBuilder::buildIntType(),
                                   SageBuilder::buildIntVal(3)),
       true},
      {SageBuilder::buildArrayType(const_int, SageBuilder::buildIntVal(3)),
       false},
      {SageBuilder::buildVolatileType(SageBuilder::buildArrayType(
           SageBuilder::buildIntType(), SageBuilder::buildIntVal(3))),
       true},
      {SageBuilder::buildConstType(SageBuilder::buildArrayType(
           SageBuilder::buildIntType(), SageBuilder::buildIntVal(3))),
       false},
      {SageBuilder::buildPointerType(const_int), true}};
  unsigned index = 0;
  for (const auto &expected : cases) {
    auto *record = SageBuilder::buildStructDeclaration(
        SageBuilder::declaration_ownership::sourceLexical(),
        "RexCRecord" + std::to_string(index), file->get_globalScope());
    auto *field = SageBuilder::buildVariableDeclaration(
        "value", expected.type, nullptr, record->get_definition());
    SageInterface::appendStatement(field, record->get_definition());
    EXPECT_TRUE(SageInterface::isCopyConstructible(record->get_type()));
    EXPECT_EQ(SageInterface::isAssignable(record->get_type()),
              expected.assignable);
    auto *outer = SageBuilder::buildStructDeclaration(
        SageBuilder::declaration_ownership::sourceLexical(),
        "RexCOuter" + std::to_string(index++), file->get_globalScope());
    auto *nested = SageBuilder::buildVariableDeclaration(
        "nested", record->get_type(), nullptr, outer->get_definition());
    SageInterface::appendStatement(nested, outer->get_definition());
    EXPECT_TRUE(SageInterface::isCopyConstructible(outer->get_type()));
    EXPECT_EQ(SageInterface::isAssignable(outer->get_type()),
              expected.assignable);
  }
}

TEST(TypeTraitsDeathTest, MissingCppCopyFamilies) {
  auto *file =
      SageBuilder::buildGeneratedSourceFile("rex_traits_missing_copy.cpp");
  const auto deleteProject = [](SgProject *project) {
    SageInterface::deleteAST(project);
  };
  std::unique_ptr<SgProject, decltype(deleteProject)> project(
      file->get_project(), deleteProject);
  auto *record = SageBuilder::buildClassDeclaration(
      SageBuilder::declaration_ownership::sourceLexical(), "RexMissingCopy",
      file->get_globalScope());
  EXPECT_DEATH(SageInterface::isCopyConstructible(record->get_type()),
               "hasAnyCopyConstructor");
  EXPECT_DEATH(SageInterface::isAssignable(record->get_type()),
               "hasAnyAssignOperator");
}

TEST(VariableConstruction, PublishesInitializerOutputOwner) {
  auto *file = SageBuilder::buildGeneratedSourceFile(
      "rex_default_constructor_owner.cpp");
  const auto deleteProject = [](SgProject *project) {
    SageInterface::deleteAST(project);
  };
  std::unique_ptr<SgProject, decltype(deleteProject)> project(
      file->get_project(), deleteProject);
  auto *scope = file->get_globalScope();
  auto *owner = SageBuilder::buildClassDeclaration(
      SageBuilder::declaration_ownership::sourceLexical(),
      "RexDefaultConstructorOwner", scope);
  auto *constructor = SageBuilder::buildNondefiningMemberFunctionDeclaration(
      SageBuilder::function_declaration_ownership::semanticAuxiliary()
          .withSpecialMemberKind(SageBuilder::function_declaration_ownership::
                                     special_member_kind::constructor)
          .withSpecialMemberProperties(true, true, false),
      owner->get_name(), SageBuilder::buildVoidType(),
      SageBuilder::buildFunctionParameterList_nfi(), owner->get_definition());
  const int output = file->get_file_info()->get_physical_file_id();
  ASSERT_GE(output, 0);
  const auto requireInitializer = [&](SgInitializedName *name,
                                      int expectedOwner) {
    auto *initializer = isSgConstructorInitializer(name->get_initializer());
    ASSERT_NE(initializer, nullptr);
    EXPECT_EQ(initializer->get_declaration(), constructor);
    EXPECT_EQ(initializer->get_parent(), name);
    for (auto *node : NodeQuery::querySubTree(initializer, V_SgExpression)) {
      auto *expression = isSgExpression(node);
      for (auto *position : {expression->get_startOfConstruct(),
                             expression->get_endOfConstruct(),
                             expression->get_operatorPosition()}) {
        ASSERT_NE(position, nullptr);
        EXPECT_EQ(position->get_parent(), expression);
        EXPECT_EQ(position->get_physical_file_id(), expectedOwner);
      }
    }
  };
  for (bool unpublished : {false, true}) {
    auto *initializer = SageBuilder::buildConstructorInitializer(
        constructor, SageBuilder::buildExprListExp(), owner->get_type(), false,
        false, true, false);
    auto *declaration =
        unpublished
            ? SageBuilder::buildUnpublishedVariableDeclaration(
                  "unpublished_object", owner->get_type(), initializer, scope)
            : SageBuilder::buildVariableDeclaration(
                  "published_object", owner->get_type(), initializer, scope);
    auto *name = declaration->get_variables().front();
    EXPECT_EQ(scope->find_symbol_from_declaration(name) != nullptr,
              !unpublished);
    ASSERT_EQ(declaration->get_parent(), nullptr);
    requireInitializer(name, Sg_File_Info::NULL_FILE_ID);
    SageInterface::appendStatement(declaration, scope);
    requireInitializer(name, output);
  }
}

TEST(VariableConstruction, PreservesTemplateLexicalOutputOwner) {
  auto *header =
      SageBuilder::buildGeneratedSourceFile("rex_template_owner.hpp");
  const auto deleteProject = [](SgProject *project) {
    SageInterface::deleteAST(project);
  };
  std::unique_ptr<SgProject, decltype(deleteProject)> project(
      header->get_project(), deleteProject);
  auto *source = SageBuilder::buildGeneratedSourceFile("rex_template_owner.cpp",
                                                       project.get());
  SgTemplateParameterPtrList parameters, definingParameters;
  for (auto *list : {&parameters, &definingParameters})
    list->push_back(SageBuilder::buildTemplateParameter(
        SgTemplateParameter::type_parameter,
        SageBuilder::buildTemplateType("T"),
        SgTemplateParameter::keyword_class));
  SgTemplateArgumentPtrList arguments;
  const SgName name("RexTemplateOwner");
  const auto scopes =
      SageBuilder::template_class_declaration_scopes::fromExactSemanticScope(
          header->get_globalScope());
  auto *canonical = SageBuilder::buildNondefiningTemplateClassDeclaration(
      SageBuilder::declaration_ownership::semanticAuxiliary(), name,
      SgClassDeclaration::e_class, scopes, &parameters, &arguments, &name);
  auto *owner = SageBuilder::buildTemplateClassDeclaration(
      SageBuilder::declaration_ownership::sourceLexical(), name,
      SgClassDeclaration::e_class, scopes, canonical, &parameters,
      &definingParameters, &arguments, &name);
  auto *type = SageBuilder::buildIntType();
  auto *member = SageBuilder::buildTemplateVariableDeclaration(
      SageBuilder::declaration_ownership::sourceLexical(), "value", type,
      nullptr, owner->get_definition(), nullptr, {});
  member->get_declarationModifier().get_storageModifier().setStatic();
  auto *definition = SageBuilder::buildTemplateVariableDeclaration(
      SageBuilder::declaration_ownership::sourceLexicalIn(
          source->get_globalScope()),
      "value", type,
      SageBuilder::buildAssignInitializer(SageBuilder::buildIntVal(7), type),
      owner->get_definition(), nullptr, {}, member->get_variables().front());
  EXPECT_EQ(definition->get_parent(), source->get_globalScope());
  EXPECT_EQ(definition->get_variables().front()->get_scope(),
            owner->get_definition());
  const int output = source->get_file_info()->get_physical_file_id();
  ASSERT_NE(output, header->get_file_info()->get_physical_file_id());
  for (auto *node : NodeQuery::querySubTree(definition, V_SgLocatedNode)) {
    auto *located = isSgLocatedNode(node);
    for (auto *position :
         {located->get_startOfConstruct(), located->get_endOfConstruct()}) {
      ASSERT_NE(position, nullptr);
      EXPECT_EQ(position->get_physical_file_id(), output);
    }
  }
}

TEST(VariableConstruction, KeepsPendingTemplateUnpublished) {
  auto *file =
      SageBuilder::buildGeneratedSourceFile("rex_pending_template.cpp");
  const auto deleteNode = [](SgNode *node) { SageInterface::deleteAST(node); };
  std::unique_ptr<SgProject, decltype(deleteNode)> project(file->get_project(),
                                                           deleteNode);
  auto *scope = file->get_globalScope();
  std::unique_ptr<SgTemplateVariableDeclaration, decltype(deleteNode)>
      declaration(SageBuilder::buildTemplateVariableDeclaration(
                      SageBuilder::declaration_ownership::
                          sourceLexicalPendingExactSource(),
                      "pending_value", SageBuilder::buildIntType(), nullptr,
                      scope, nullptr, {}),
                  deleteNode);
  auto *name = declaration->get_variables().front();
  EXPECT_EQ(declaration->get_parent(), nullptr);
  EXPECT_EQ(declaration->get_startOfConstruct(), nullptr);
  EXPECT_EQ(name->get_scope(), scope);
  EXPECT_EQ(scope->find_symbol_from_declaration(name), nullptr);
}

TEST(TemplateInstantiation, RenamedRedeclarationPreservesFamily) {
  SgTemplateType original("T"), renamed("Renamed");
  initializeParameterIdentity(original, 1);
  initializeParameterIdentity(renamed, 1);
  std::unique_ptr<SgTemplateParameter> parameter(
      SageBuilder::buildTemplateParameter(SgTemplateParameter::type_parameter,
                                          &original,
                                          SgTemplateParameter::keyword_class));
  std::unique_ptr<SgTemplateParameter> redeclaration(
      SageBuilder::buildTemplateParameter(
          SgTemplateParameter::type_parameter, &renamed,
          SgTemplateParameter::keyword_typename));
  auto argument =
      std::make_unique<SgTemplateArgument>(SageBuilder::buildIntType(), false);
  SgTemplateParameterPtrList parameters{parameter.get()};
  SgTemplateArgumentPtrList arguments{argument.get()};
  EXPECT_EQ(Rose::Builder::Templates::instantiateNonrealTypes(
                &renamed, parameters, arguments, nullptr),
            argument->get_type());
}

TEST(TemplateInstantiationDeathTest,
     RejectsUnrelatedOrMissingParameterIdentity) {
  for (int malformed = 0; malformed < 4; ++malformed) {
    ASSERT_DEATH(
        {
          SgTemplateType original("T");
          SgTemplateType other("T");
          initializeParameterIdentity(original, 1);
          initializeParameterIdentity(other, malformed == 0 ? 2 : 1);
          std::unique_ptr<SgTemplateParameter> parameter(
              SageBuilder::buildTemplateParameter(
                  SgTemplateParameter::type_parameter, &original,
                  SgTemplateParameter::keyword_class));
          std::unique_ptr<SgTemplateParameter> otherParameter(
              SageBuilder::buildTemplateParameter(
                  SgTemplateParameter::type_parameter, &other,
                  SgTemplateParameter::keyword_class));
          auto argument = std::make_unique<SgTemplateArgument>(
              SageBuilder::buildIntType(), false);
          SgTemplateParameterPtrList parameters{parameter.get()};
          SgTemplateArgumentPtrList arguments{argument.get()};
          if (malformed == 1)
            other.set_template_parameter(nullptr);
          else if (malformed == 2)
            parameters.clear();
          else if (malformed == 3)
            otherParameter->set_parameterType(
                SgTemplateParameter::nontype_parameter);
          Rose::Builder::Templates::instantiateNonrealTypes(&other, parameters,
                                                            arguments, nullptr);
        },
        "type-parameter-instantiation");
  }
}

TEST(TypeEquivalence, ModifierCacheCopyOwnership) {
  auto original = std::make_unique<SgModifierNodes>();
  auto *tail = new SgModifierNodes();
  tail->set_parent(original.get());
  original->set_next(tail);
  SgTreeCopy copier;
  std::unique_ptr<SgModifierNodes> copied(
      isSgModifierNodes(original->copy(copier)));
  ROSE_ASSERT(copied != nullptr);
  SgModifierNodes *copied_tail = copied->get_next();
  ROSE_ASSERT(copied_tail != nullptr && copied_tail != tail);
  ROSE_ASSERT(copied_tail->get_parent() == copied.get());
  copied.reset();
  ROSE_ASSERT(!SgNode::isLiveNode(copied_tail));
  ROSE_ASSERT(SgNode::isLiveNode(tail));
  original.reset();
  ROSE_ASSERT(!SgNode::isLiveNode(tail));
}

TEST(TypeEquivalence, Modifiers) {
  auto *base = SageBuilder::buildIntType();
  auto empty = std::make_unique<SgModifierType>(base);
  ROSE_ASSERT(empty->get_typeModifier().isIdentity());
  ROSE_ASSERT(SageInterface::isEquivalentType(empty.get(), base));
  ROSE_ASSERT(SageInterface::isEquivalentType(base, empty.get()));

  for (auto set :
       {&SgTypeModifier::setRestrict, &SgTypeModifier::setAtomic,
        &SgTypeModifier::setVectorType, &SgTypeModifier::setExtendedVectorType,
        &SgTypeModifier::setAddressSpace, &SgTypeModifier::setOpenclGlobal,
        &SgTypeModifier::setOpenclLocal, &SgTypeModifier::setOpenclConstant})
    requireSemanticModifier(set);

  auto four = std::make_unique<SgModifierType>(base);
  auto eight = std::make_unique<SgModifierType>(base);
  four->get_typeModifier().setVectorType();
  eight->get_typeModifier().setVectorType();
  four->get_typeModifier().set_vector_size(4);
  eight->get_typeModifier().set_vector_size(8);
  requireDistinct(four.get(), eight.get());
  requireDistinct(four.get(), base);
  ROSE_ASSERT(four->get_mangled() != eight->get_mangled());
  auto extended = std::make_unique<SgModifierType>(base);
  extended->get_typeModifier() = four->get_typeModifier();
  extended->get_typeModifier().setExtendedVectorType();
  ROSE_ASSERT(extended->get_typeModifier().isVectorType());
  requireDistinct(four.get(), extended.get());
  ROSE_ASSERT(four->get_mangled() != extended->get_mangled());
  ROSE_ASSERT(
      SageBuilder::buildModifierType(base, four->get_typeModifier()) !=
      SageBuilder::buildModifierType(base, extended->get_typeModifier()));
  SgTypeModifier copied(extended->get_typeModifier());
  ROSE_ASSERT(copied == extended->get_typeModifier());
  copied.unsetVectorType();
  ROSE_ASSERT(!copied.isVectorType() && !copied.isExtendedVectorType());

  auto first = std::make_unique<SgModifierType>(base);
  auto second = std::make_unique<SgModifierType>(base);
  first->get_typeModifier().setAddressSpace();
  second->get_typeModifier().setAddressSpace();
  first->get_typeModifier().set_address_space_value(1);
  second->get_typeModifier().set_address_space_value(2);
  requireDistinct(first.get(), second.get());
  requireDistinct(first.get(), base);
  ROSE_ASSERT(first->get_mangled() != second->get_mangled());
  ROSE_ASSERT(first->get_mangled() != base->get_mangled());

  auto global = std::make_unique<SgModifierType>(base);
  auto local = std::make_unique<SgModifierType>(base);
  auto constant = std::make_unique<SgModifierType>(base);
  global->get_typeModifier().setOpenclGlobal();
  local->get_typeModifier().setOpenclLocal();
  constant->get_typeModifier().setOpenclConstant();
  ROSE_ASSERT(global->get_mangled() != local->get_mangled());
  ROSE_ASSERT(global->get_mangled() != constant->get_mangled());
  ROSE_ASSERT(local->get_mangled() != constant->get_mangled());
  ROSE_ASSERT(global->get_mangled() != base->get_mangled());
}

TEST(TypeEquivalence, ModifierCachePreservesEditState) {
  for (bool modified : {false, true}) {
    auto base = std::make_unique<SgTypeInt>();
    base->set_isModified(modified);
    const auto sequence = base->get_lastMutationSequence();
    SgTypeModifier modifier;
    modifier.setAtomic();
    std::unique_ptr<SgModifierType> type(
        SageBuilder::buildModifierType(base.get(), modifier));
    ROSE_ASSERT(type->get_base_type() == base.get());
    ROSE_ASSERT(base->get_isModified() == modified);
    ROSE_ASSERT(base->get_lastMutationSequence() == sequence);
    ROSE_ASSERT(SageBuilder::buildModifierType(base.get(), modifier) ==
                type.get());
  }
}

TEST(TypeEquivalence, ModifierResetRestoresDefault) {
  SgTypeModifier modifier;
  modifier.setAddressSpace();
  modifier.set_address_space_value(7);
  modifier.setVectorType();
  modifier.set_vector_size(4);
  modifier.setGnuAttributePacked();
  modifier.set_gnu_attribute_alignment(0);
  modifier.set_gnu_attribute_sentinel(2);
  modifier.get_constVolatileModifier().setConst();
  modifier.get_elaboratedTypeModifier().setClass();

  modifier.reset();
  ROSE_ASSERT(modifier == SgTypeModifier());
}

TEST(TypeEquivalence, OtherModifierState) {
  SgFunctionModifier function;
  const SgFunctionModifier default_function;
  function.setMarkedDelete();
  ROSE_ASSERT(!(function == default_function));
  function.reset();
  function.set_gnu_attribute_named_alias("target");
  ROSE_ASSERT(!(function == default_function));
  function.set_opencl_work_group_size({2, 3, 4});
  function.reset();
  ROSE_ASSERT(function == default_function);

  SgPointerType first_hint(SageBuilder::buildIntType());
  SgPointerType second_hint(SageBuilder::buildIntType());
  SgFunctionModifier first_function;
  SgFunctionModifier second_function;
  first_function.setOpenclVecTypeHint();
  second_function.setOpenclVecTypeHint();
  first_function.set_opencl_vec_type(&first_hint);
  second_function.set_opencl_vec_type(&second_hint);
  ROSE_ASSERT(first_function == second_function);

  SgSpecialFunctionModifier special;
  const SgSpecialFunctionModifier default_special;
  special.setUldOperator();
  ROSE_ASSERT(!(special == default_special));
  special.reset();
  ROSE_ASSERT(special == default_special);

  SgDeclarationModifier declaration;
  const SgDeclarationModifier default_declaration;
  declaration.setOverride();
  ROSE_ASSERT(!(declaration == default_declaration));
  declaration.get_typeModifier().setAddressSpace();
  declaration.reset();
  ROSE_ASSERT(declaration == default_declaration);

  SgStorageModifier storage;
  const SgStorageModifier default_storage;
  storage.set_thread_local_storage(true);
  ROSE_ASSERT(!(storage == default_storage));
  storage.reset();
  ROSE_ASSERT(storage == default_storage);

  SgAccessModifier access;
  const SgAccessModifier default_access;
  access.set_is_explicit(true);
  ROSE_ASSERT(!(access == default_access));
  access.reset();
  ROSE_ASSERT(access == default_access);
}

TEST(TypeEquivalence, RepeatedPointerComparisonsReleaseRecursionDepth) {
  SgPointerType first(SageBuilder::buildIntType());
  SgPointerType second(SageBuilder::buildIntType());
  for (int iteration = 0; iteration < 400; ++iteration)
    ASSERT_TRUE(SageInterface::isEquivalentType(&first, &second));
}

TEST(TypeEquivalenceDeathTest, CyclicPointersReachRecursionLimit) {
  ASSERT_DEATH(
      {
        SgPointerType left(SageBuilder::buildIntType());
        SgPointerType right(SageBuilder::buildIntType());
        left.set_base_type(&left);
        right.set_base_type(&right);
        SageInterface::isEquivalentType(&left, &right);
      },
      "recursive limit exceeded");
}

TEST(TypeValidationDeathTest, CyclicTypeFailsAtValidation) {
  ASSERT_DEATH(
      {
        SgPointerType pointer(SageBuilder::buildIntType());
        pointer.set_base_type(&pointer);
        SageInterface::containsUnknownType(&pointer);
      },
      "unknown-type-cycle");
}

TEST(TypeValidationDeathTest, ModifierCacheRejectsMalformedOwnership) {
  for (bool cycle : {false, true}) {
    ASSERT_DEATH(
        {
          SgTypeInt base;
          SgTypeModifier modifier;
          modifier.setAtomic();
          SageBuilder::buildModifierType(&base, modifier);
          auto *bucket = base.get_modifiers();
          if (cycle)
            bucket->set_next(bucket);
          else
            bucket->set_parent(nullptr);
          SageBuilder::buildModifierType(&base, modifier);
        },
        "modifier-type-canonical-owner");
  }
}

TEST(TypeValidationDeathTest, PrivateFunctionRejectsUnknownResult) {
  ASSERT_DEATH(
      {
        SgFunctionType function(new SgTypeUnknown(), false,
                                new SgFunctionParameterTypeList());
      },
      "p_return_type");
}

TEST(FunctionSignatureCopy, PreservesExternalAndMappedExceptionSources) {
  auto *signature = SageBuilder::buildFunctionType(
      SageBuilder::buildVoidType(),
      SageBuilder::buildFunctionParameterTypeList());
  auto source = std::make_unique<SgFunctionDeclaration>(
      "rex_exception_source", signature, nullptr, nullptr);
  auto target = std::make_unique<SgFunctionDeclaration>(
      "rex_exception_target", signature, nullptr, nullptr);
  SgFunctionType specification(signature->get_return_type(), false,
                               SageBuilder::buildFunctionParameterTypeList(),
                               SgFunctionType::e_exception_uninstantiated,
                               nullptr, {}, {}, source.get(), source.get(),
                               false, false, nullptr);
  for (bool mapped : {false, true}) {
    SgTreeCopy bindings;
    if (mapped)
      bindings.insertCopiedNodePair(source.get(), target.get());
    auto copy = SageBuilder::copyFunctionSignature(
        &specification, signature->get_return_type(),
        std::make_unique<SgFunctionParameterTypeList>(), nullptr, 0, bindings,
        nullptr);
    EXPECT_EQ(copy->get_exception_specification_source(),
              mapped ? target.get() : source.get());
    EXPECT_EQ(copy->get_exception_specification_pattern(),
              mapped ? target.get() : source.get());
  }
}

TEST(TypeEquivalence, ConstructorExpressionsPreserveTypesAndArguments) {
  auto *scope = new SgGlobal();
  auto *type =
      SageBuilder::buildSemanticNonrealType("rex_constructor", scope, nullptr);
  auto *sameType =
      SageBuilder::buildSemanticNonrealType("rex_constructor", scope, nullptr);
  auto *otherType =
      SageBuilder::buildSemanticNonrealType("rex_other", scope, nullptr);
  ASSERT_TRUE(SageInterface::isEquivalentType(type, sameType));
  const auto construct = [](SgType *type, int value) {
    auto *arguments = SageBuilder::buildExprListExp_nfi();
    arguments->append_expression(SageBuilder::buildIntVal_nfi(value));
    return SageBuilder::buildConstructorInitializer_nfi(
        nullptr, arguments, type, false, false, true, true);
  };
  auto *first = construct(type, 1);
  auto *same = construct(sameType, 1);
  auto *other = construct(otherType, 1);
  auto *otherArgument = construct(sameType, 2);
  EXPECT_TRUE(SageInterface::constraintExpressionEquivalence(first, same));
  EXPECT_FALSE(SageInterface::constraintExpressionEquivalence(first, other));
  EXPECT_FALSE(
      SageInterface::constraintExpressionEquivalence(first, otherArgument));
  for (auto *expression : {first, same, other, otherArgument})
    SageInterface::deleteAST(expression);
  SageInterface::deleteAST(scope);
}

TEST(TypeEquivalence, TemplateArgumentsUseSemanticReferenceTypes) {
  SgRvalueReferenceType first(SageBuilder::buildIntType());
  SgRvalueReferenceType same(SageBuilder::buildIntType());
  SgRvalueReferenceType other(SageBuilder::buildLongType());
  auto firstArgument = std::make_unique<SgTemplateArgument>(&first, false);
  auto sameArgument = std::make_unique<SgTemplateArgument>(&same, false);
  auto otherArgument = std::make_unique<SgTemplateArgument>(&other, false);
  EXPECT_TRUE(SageInterface::templateArgumentEquivalence(firstArgument.get(),
                                                         sameArgument.get()));
  EXPECT_FALSE(SageInterface::templateArgumentEquivalence(firstArgument.get(),
                                                          otherArgument.get()));
}

TEST(FunctionSignatureCopyDeathTest, RejectsUnconstructedExceptionDependency) {
  for (bool pattern : {false, true}) {
    ASSERT_DEATH(
        {
          auto *signature = SageBuilder::buildFunctionType(
              SageBuilder::buildVoidType(),
              SageBuilder::buildFunctionParameterTypeList());
          auto source = std::make_unique<SgFunctionDeclaration>(
              "rex_exception_source", signature, nullptr, nullptr);
          auto primary = std::make_unique<SgFunctionDeclaration>(
              "rex_exception_primary", signature, nullptr, nullptr);
          SgFunctionType specification(
              signature->get_return_type(), false,
              SageBuilder::buildFunctionParameterTypeList(),
              SgFunctionType::e_exception_uninstantiated, nullptr, {}, {},
              source.get(), primary.get(), false, false, nullptr);
          SgTreeCopy bindings;
          SgCopyHelp::ConstructionScope construction(
              bindings, pattern ? primary.get() : source.get());
          SageBuilder::copyFunctionSignature(
              &specification, signature->get_return_type(),
              std::make_unique<SgFunctionParameterTypeList>(), nullptr, 0,
              bindings, nullptr);
        },
        "target");
  }
}

TEST(CopyReferences, WaitsForDependencyUnderCompletedCopiedOwner) {
  SgExpression *original = SageBuilder::buildIntVal(1);
  auto *source =
      SageBuilder::buildBasicBlock(SageBuilder::buildExprStatement(original));
  auto *target = SageBuilder::buildBasicBlock();
  auto *copied = SageBuilder::buildIntVal(1);
  SgTreeCopy bindings;
  bindings.insertCopiedNodePair(source, target);
  SgExpression *reference = nullptr;
  bindings.bindCopiedReference(reference, original);
  EXPECT_EQ(reference, nullptr);
  bindings.insertCopiedNodePair(original, copied);
  EXPECT_EQ(reference, copied);
  bindings.requireBoundCopiedReferences();
  SageInterface::deleteAST(source);
  SageInterface::deleteAST(target);
  SageInterface::deleteAST(copied);
}

TEST(CopyReferencesDeathTest, RejectsMissingDependencyUnderCompletedOwner) {
  ASSERT_DEATH(
      {
        SgExpression *original = SageBuilder::buildIntVal(1);
        auto *source = SageBuilder::buildBasicBlock(
            SageBuilder::buildExprStatement(original));
        auto *target = SageBuilder::buildBasicBlock();
        SgTreeCopy bindings;
        bindings.insertCopiedNodePair(source, target);
        SgExpression *reference = nullptr;
        bindings.bindCopiedReference(reference, original);
        bindings.requireBoundCopiedReferences();
      },
      "unconstructed-reference");
}

TEST(ProjectMutationState, PreservesUserAttributeStorage) {
  auto *file = SageBuilder::buildGeneratedSourceFile(
      "rex_project_mutation_attributes.cpp");
  auto *project = file->get_project();
  auto *scope = file->get_globalScope();
  auto *note = new AstIntAttribute(42);
  project->addNewAttribute("rex_project_note", note);
  auto *callGraphState = &project->get_callGraphState();
  auto *declaration =
      SageBuilder::buildPragmaDeclaration("rex_mutation_target", scope);
  SageInterface::appendStatement(declaration, scope);
  SageInterface::removeStatement(declaration);
  EXPECT_EQ(project->get_attributeMechanism()->size(), 1);
  EXPECT_EQ(project->getAttribute("rex_project_note"), note);
  EXPECT_TRUE(scope->get_declarations().empty());
  SageInterface::deleteAST(declaration);
  auto *copy = SageInterface::deepCopy(project);
  EXPECT_NE(&copy->get_mutationState(), &project->get_mutationState());
  EXPECT_NE(&copy->get_callGraphState(), callGraphState);
  EXPECT_NE(copy->getAttribute("rex_project_note"), note);
  EXPECT_EQ(copy->numberOfAttributes(), 1);
  project->removeAttribute("rex_project_note");
  EXPECT_EQ(project->numberOfAttributes(), 0);
  SageInterface::deleteAST(copy);
  SageInterface::deleteAST(project);
}

TEST(CopyReferences, ConstructsDeclarationChainReferenceBeforePublication) {
  auto *previousFile =
      SageBuilder::buildGeneratedSourceFile("rex_copy_chain_previous.cpp");
  auto *project = previousFile->get_project();
  auto *previousScope = previousFile->get_globalScope();
  auto *sourceScope = SageBuilder::buildGeneratedSourceFile(
                          "rex_copy_chain_source.cpp", project)
                          ->get_globalScope();
  auto *targetScope = SageBuilder::buildGeneratedSourceFile(
                          "rex_copy_chain_target.cpp", project)
                          ->get_globalScope();
  auto declaration = [](SgGlobal *scope) {
    auto *result = SageBuilder::buildVariableDeclaration(
        "rex_copy_chain", SageBuilder::buildIntType(), nullptr, scope);
    SageInterface::appendStatement(result, scope);
    return result;
  };
  auto *previousDeclaration = declaration(previousScope);
  auto *sourceDeclaration = declaration(sourceScope);
  auto *copiedPreviousDeclaration = declaration(targetScope);
  auto *previous = previousDeclaration->get_variables().front();
  auto *source = sourceDeclaration->get_variables().front();
  auto *copiedPrevious = copiedPreviousDeclaration->get_variables().front();
  source->set_prev_decl_item(previous);
  SgTreeCopy bindings;
  SgCopyHelp::ConstructionScope construction(bindings, previous);
  bindings.incrementDepth();
  auto *copiedDeclaration =
      isSgVariableDeclaration(sourceDeclaration->copy(bindings));
  ASSERT_NE(copiedDeclaration, nullptr);
  auto *copy = copiedDeclaration->get_variables().front();
  EXPECT_EQ(copy->get_prev_decl_item(), nullptr);
  bindings.insertCopiedNodePair(previous, copiedPrevious);
  ASSERT_EQ(copy->get_prev_decl_item(), copiedPrevious);
  bindings.decrementDepth();
  bindings.requireBoundCopiedReferences();
  SageInterface::deleteAST(copiedDeclaration);
  SageInterface::deleteAST(project);
}

TEST(FunctionParameterTypes, PreserveObjectsAndPointeeQualifiers) {
  auto *integer = SageBuilder::buildIntType();
  for (int qualifier = 0; qualifier != 3; ++qualifier) {
    SgPointerType restrictedBase(integer);
    SgType *base = integer;
    if (qualifier == 2)
      base = &restrictedBase;
    SgModifierType qualified(base);
    auto &modifier = qualified.get_typeModifier();
    if (qualifier == 0)
      modifier.get_constVolatileModifier().setConst();
    else if (qualifier == 1)
      modifier.get_constVolatileModifier().setVolatile();
    else
      modifier.setRestrict();
    const SgTypeModifier original = modifier;
    auto value = std::make_unique<SgIntVal>(0, "");
    std::unique_ptr<SgCastExp> expression(new SgCastExp(
        value.get(), &qualified, SgUnaryOp::prefix, SgCastExp::e_C_style_cast,
        SgCastExp::e_semantic_conversion_NoOp,
        SgCastExp::e_value_category_prvalue, {}, &qualified));
    auto expressions = std::make_unique<SgExprListExp>();
    expressions->append_expression(expression.get());
    std::unique_ptr<SgFunctionParameterTypeList> explicitTypes(
        SageBuilder::buildFunctionParameterTypeList(&qualified));
    std::unique_ptr<SgFunctionParameterTypeList> expressionTypes(
        SageBuilder::buildFunctionParameterTypeList(expressions.get()));
    std::unique_ptr<SgFunctionParameterTypeList> noFileInfoTypes(
        SageBuilder::buildFunctionParameterTypeList_nfi(expressions.get()));
    for (auto *types :
         {explicitTypes.get(), expressionTypes.get(), noFileInfoTypes.get()})
      EXPECT_EQ(types->get_arguments(), SgTypePtrList{base});
    EXPECT_EQ(expression->get_type(), &qualified);
    EXPECT_TRUE(modifier == original);
    SgPointerType pointer(&qualified);
    std::unique_ptr<SgFunctionParameterTypeList> pointerTypes(
        SageBuilder::buildFunctionParameterTypeList(&pointer));
    EXPECT_EQ(pointerTypes->get_arguments(), SgTypePtrList{&pointer});
  }
}

TEST(FunctionParameterTypes, ArrayQualificationsApplyToElements) {
  auto *integer = SageBuilder::buildIntType();
  SgArrayType array(integer, new SgIntVal(8, ""));
  for (int qualifier = 1; qualifier != 4; ++qualifier) {
    SgModifierType source(&array), element(integer);
    for (auto *type : {&source, &element}) {
      auto &cv = type->get_typeModifier().get_constVolatileModifier();
      if (qualifier & 1)
        cv.setConst();
      if (qualifier & 2)
        cv.setVolatile();
    }
    SgArrayType semantic(&element, new SgIntVal(8, ""));
    SgArrayType different(&element, new SgIntVal(9, ""));
    SgArrayType nestedSource(&source, new SgIntVal(9, ""));
    SgArrayType nestedSemantic(&semantic, new SgIntVal(9, ""));
    SgPointerType sourcePointer(&source), semanticPointer(&semantic);
    SgPointerType adjusted(&element), unqualified(integer);
    for (auto [left, right] :
         {std::pair<SgType *, SgType *>{&source, &semantic},
          {&nestedSource, &nestedSemantic},
          {&sourcePointer, &semanticPointer}}) {
      EXPECT_TRUE(SageInterface::cxxSourceTypeMatchesSemanticType(left, right));
      EXPECT_TRUE(SageInterface::cxxSourceTypeMatchesSemanticType(right, left));
    }
    EXPECT_TRUE(
        SageInterface::cxxParameterTypeMatchesSemanticType(&source, &adjusted));
    EXPECT_FALSE(SageInterface::cxxParameterTypeMatchesSemanticType(
        &source, &unqualified));
    EXPECT_FALSE(
        SageInterface::cxxSourceTypeMatchesSemanticType(&source, &array));
    EXPECT_FALSE(
        SageInterface::cxxSourceTypeMatchesSemanticType(&source, &different));
  }
}

TEST(SourceTypeIdentity, OnlyOmittedOuterArrayBoundsMayBeCompleted) {
  auto *integer = SageBuilder::buildIntType();
  SgArrayType incomplete(integer, nullptr);
  SgArrayType complete(integer, new SgIntVal(2, ""));
  SgArrayType different(integer, new SgIntVal(3, ""));
  SgArrayType floating(SageBuilder::buildFloatType(), nullptr);
  SgModifierType constant(integer);
  constant.get_typeModifier().get_constVolatileModifier().setConst();
  SgArrayType qualified(&constant, nullptr);
  SgArrayType qualifiedComplete(&constant, new SgIntVal(2, ""));
  SgModifierType qualifiedSource(&incomplete);
  qualifiedSource.get_typeModifier().get_constVolatileModifier().setConst();
  SgArrayType nestedSource(&incomplete, new SgIntVal(4, ""));
  SgArrayType nestedSemantic(&complete, new SgIntVal(4, ""));
  SgPointerType adjusted(&complete);
  SgPointerType incompletePointer(&incomplete);
  SgReferenceType incompleteReference(&incomplete),
      completeReference(&complete);
  EXPECT_TRUE(
      SageInterface::cxxSourceTypeMatchesObjectType(&incomplete, &complete));
  EXPECT_TRUE(SageInterface::cxxSourceTypeMatchesObjectType(
      &qualifiedSource, &qualifiedComplete));
  EXPECT_FALSE(
      SageInterface::cxxSourceTypeMatchesSemanticType(&incomplete, &complete));
  EXPECT_FALSE(
      SageInterface::cxxSourceTypeMatchesObjectType(&complete, &incomplete));
  EXPECT_FALSE(
      SageInterface::cxxSourceTypeMatchesObjectType(&different, &complete));
  EXPECT_FALSE(
      SageInterface::cxxSourceTypeMatchesObjectType(&floating, &complete));
  EXPECT_FALSE(
      SageInterface::cxxSourceTypeMatchesObjectType(&qualified, &complete));
  EXPECT_FALSE(SageInterface::cxxSourceTypeMatchesObjectType(&nestedSource,
                                                             &nestedSemantic));
  EXPECT_FALSE(SageInterface::cxxSourceTypeMatchesObjectType(&incompletePointer,
                                                             &adjusted));
  EXPECT_FALSE(SageInterface::cxxParameterTypeMatchesSemanticType(
      &incompleteReference, &completeReference));
  EXPECT_FALSE(SageInterface::cxxParameterTypeMatchesSemanticType(&nestedSource,
                                                                  &adjusted));
}

TEST(SourceTypeIdentity,
     CompoundLiteralPreservesNestedQualificationsAndBounds) {
  auto *integer = SageBuilder::buildIntType();
  SgModifierType constant(integer);
  constant.get_typeModifier().get_constVolatileModifier().setConst();
  SgPointerType pointer(integer), qualifiedPointer(&constant);
  SgArrayType complete(integer, new SgIntVal(2, ""));
  SgArrayType different(integer, new SgIntVal(3, ""));
  SgArrayType incomplete(integer, nullptr);
  EXPECT_TRUE(SageInterface::cxxCompoundLiteralInitializerTypeMatchesObjectType(
      integer, &constant));
  EXPECT_FALSE(
      SageInterface::cxxCompoundLiteralInitializerTypeMatchesObjectType(
          SageBuilder::buildFloatType(), &constant));
  EXPECT_FALSE(
      SageInterface::cxxCompoundLiteralInitializerTypeMatchesObjectType(
          &pointer, &qualifiedPointer));
  EXPECT_TRUE(SageInterface::cxxCompoundLiteralInitializerTypeMatchesObjectType(
      &complete, &complete));
  EXPECT_FALSE(
      SageInterface::cxxCompoundLiteralInitializerTypeMatchesObjectType(
          &complete, &different));
  EXPECT_FALSE(
      SageInterface::cxxCompoundLiteralInitializerTypeMatchesObjectType(
          &incomplete, &complete));
}

TEST(TypeValidationDeathTest, CanonicalFunctionRejectsQualifiedArguments) {
  for (bool member : {false, true}) {
    for (int qualifier = 0; qualifier != 3; ++qualifier) {
      ASSERT_DEATH(
          {
            SgPointerType pointer(SageBuilder::buildIntType());
            SgType *base = SageBuilder::buildIntType();
            if (qualifier == 2)
              base = &pointer;
            SgModifierType qualified(base);
            if (qualifier == 0)
              qualified.get_typeModifier()
                  .get_constVolatileModifier()
                  .setConst();
            else if (qualifier == 1)
              qualified.get_typeModifier()
                  .get_constVolatileModifier()
                  .setVolatile();
            else
              qualified.get_typeModifier().setRestrict();
            SgFunctionParameterTypeList arguments;
            arguments.append_argument(&qualified);
            if (member)
              SageBuilder::buildMemberFunctionType(
                  SageBuilder::buildVoidType(), &arguments,
                  static_cast<SgType *>(nullptr),
                  SgMemberFunctionType::e_const);
            else
              SageBuilder::buildFunctionType(SageBuilder::buildVoidType(),
                                             &arguments);
          },
          "top-level qualifiers");
    }
  }
}

TEST(TypeValidationDeathTest, PrivateMemberFunctionRejectsUnknownOwner) {
  ASSERT_DEATH(
      {
        SgMemberFunctionType function(SageBuilder::buildVoidType(), false,
                                      new SgFunctionParameterTypeList(),
                                      new SgTypeUnknown());
      },
      "p_class_type");
}

TEST(TypeValidationDeathTest, PrivateFunctionRejectsContradictoryEllipsis) {
  ASSERT_DEATH(
      {
        SgFunctionType function(SageBuilder::buildVoidType(), true,
                                new SgFunctionParameterTypeList());
      },
      "p_has_ellipses");
}

TEST(TypeValidationDeathTest, PrivateFunctionRejectsNonfinalEllipsis) {
  ASSERT_DEATH(
      {
        auto *arguments = new SgFunctionParameterTypeList();
        arguments->append_argument(SgTypeEllipse::createType());
        arguments->append_argument(SageBuilder::buildIntType());
        SgFunctionType function(SageBuilder::buildVoidType(), false, arguments);
      },
      "has_ellipses");
}

TEST(TypeValidationDeathTest, PrivateFunctionRejectsDetachedArgumentPosition) {
  ASSERT_DEATH(
      {
        auto *arguments = new SgFunctionParameterTypeList();
        arguments->append_argument(SageBuilder::buildIntType());
        arguments->get_argument_qualification_use_sites().front()->set_parent(
            nullptr);
        SgFunctionType function(SageBuilder::buildVoidType(), false, arguments);
      },
      "function-argument-qualification-use-site");
}

TEST(CFGScopes, ExcludesPrototypeParameters) {
  auto *file =
      SageBuilder::buildGeneratedSourceFile("rex_cfg_prototype_lifetime.cpp");
  const auto deleteProject = [](SgProject *project) {
    SageInterface::deleteAST(project);
  };
  std::unique_ptr<SgProject, decltype(deleteProject)> project(
      file->get_project(), deleteProject);
  auto *integer = SageBuilder::buildIntType();
  auto *parameter = SageBuilder::buildInitializedName("active", integer);
  auto *parameters = SageBuilder::buildFunctionParameterList_nfi();
  parameters->append_arg(parameter);
  auto *function = SageBuilder::buildDefiningFunctionDeclaration(
      SageBuilder::function_declaration_ownership::sourceLexical(), "caller",
      integer, parameters, file->get_globalScope());
  auto *definition = function->get_definition();
  auto *body = definition->get_body();
  auto *local =
      SageBuilder::buildVariableDeclaration("local", integer, nullptr, body);
  SageInterface::appendStatement(local, body);
  auto *prototypeParameter =
      SageBuilder::buildInitializedName("inactive", integer);
  auto *prototypeParameters = SageBuilder::buildFunctionParameterList_nfi();
  prototypeParameters->append_arg(prototypeParameter);
  SageBuilder::buildNondefiningFunctionDeclaration(
      SageBuilder::function_declaration_ownership::sourceLexicalIn(body),
      "callee", integer, prototypeParameters, file->get_globalScope());
  auto *statement = SageBuilder::buildReturnStmt(SageBuilder::buildIntVal(0));
  SageInterface::appendStatement(statement, body);
  const std::vector<SgInitializedName *> expected{
      local->get_variables().front(), parameter};
  const auto jumps = statement->cfgForEnd().outEdges();
  ASSERT_EQ(jumps.size(), 1u);
  EXPECT_EQ(jumps.front().target(), definition->cfgForEnd());
  EXPECT_EQ(jumps.front().scopesBeingExited(), expected);
  const auto fallthrough = body->cfgForEnd().outEdges();
  ASSERT_EQ(fallthrough.size(), 1u);
  EXPECT_EQ(fallthrough.front().target(), definition->cfgForEnd());
  EXPECT_EQ(fallthrough.front().scopesBeingExited(), expected);
}

TEST(DestructorConstructionDeathTest, ValidatesBaseHierarchyAndOrder) {
  enum class Case {
    direct,
    swapped_direct,
    foreign,
    wrong_role,
    indirect_virtual,
    cycle,
    swapped_virtual,
    nested_virtual,
    spurious_indirect,
    cached_indirect,
    swapped_members,
    missing_member_index,
    omitted_direct,
    omitted_virtual,
    omitted_member,
    foreign_signature_owner
  };
  const auto construct = [](Case form) {
    auto *file =
        SageBuilder::buildGeneratedSourceFile("rex_destructor_base_order.cpp");
    const auto deleteProject = [](SgProject *project) {
      SageInterface::deleteAST(project);
    };
    std::unique_ptr<SgProject, decltype(deleteProject)> project(
        file->get_project(), deleteProject);
    auto *global = file->get_globalScope();
    const auto ownership =
        SageBuilder::function_declaration_ownership::semanticAuxiliary()
            .withSpecialMemberKind(SageBuilder::function_declaration_ownership::
                                       special_member_kind::destructor)
            .withDestructorProperties(
                SgFunctionModifier::e_destructor_selected_nontrivial);
    std::vector<SgClassDeclaration *> bases;
    std::vector<SgMemberFunctionDeclaration *> destructors;
    for (const char *name : {"First", "Second", "Foreign"}) {
      auto *base = SageBuilder::buildClassDeclaration(
          SageBuilder::declaration_ownership::sourceLexical(), name, global);
      bases.push_back(base);
      destructors.push_back(
          SageBuilder::buildNondefiningMemberFunctionDeclaration(
              ownership, SgName("~" + base->get_name().getString()),
              SageBuilder::buildVoidType(),
              SageBuilder::buildFunctionParameterList_nfi(),
              base->get_definition()));
    }
    if (form == Case::foreign_signature_owner) {
      auto *destructor = destructors[0];
      auto *signature = destructor->get_type();
      EXPECT_TRUE(SgExpression::is_valid_bound_temporary_destructor(
          bases[0]->get_type(), destructor));
      destructor->set_type(destructors[2]->get_type());
      EXPECT_EQ(destructor->get_scope(), bases[0]->get_definition());
      EXPECT_FALSE(SgExpression::is_valid_bound_temporary_destructor(
          bases[2]->get_type(), destructor));
      EXPECT_FALSE(SgCxxSubobjectDestruction::is_valid_target(
          destructor, nullptr, bases[2],
          SgCxxSubobjectDestruction::e_nonvirtual_base,
          SageBuilder::buildVoidType()));
      destructor->set_type(signature);
      return;
    }
    auto *owner = SageBuilder::buildClassDeclaration(
        SageBuilder::declaration_ownership::sourceLexical(), "Owner", global);
    auto *scope = owner->get_definition();
    const bool indirect =
        form == Case::indirect_virtual || form == Case::swapped_virtual ||
        form == Case::nested_virtual || form == Case::cached_indirect ||
        form == Case::omitted_virtual;
    SageBuilder::buildBaseClass(bases[0], bases[0]->get_type(),
                                indirect ? bases[1]->get_definition() : scope,
                                indirect, true);
    SageBuilder::buildBaseClass(bases[1], bases[1]->get_type(), scope, false,
                                true);
    if (form == Case::cycle)
      SageBuilder::buildBaseClass(owner, owner->get_type(),
                                  bases[0]->get_definition(), false, true);
    if (form == Case::swapped_virtual || form == Case::nested_virtual)
      SageBuilder::buildBaseClass(bases[2], bases[2]->get_type(),
                                  bases[0]->get_definition(), true, true);
    if (form == Case::spurious_indirect || form == Case::cached_indirect) {
      auto *target = bases[form == Case::spurious_indirect ? 2 : 0];
      SageBuilder::buildBaseClass(target, target->get_type(), scope, true,
                                  false);
    }
    std::vector<SgInitializedName *> fields;
    for (unsigned int index = 0; index != 2; ++index) {
      auto *field = SageBuilder::buildVariableDeclaration(
          index == 0 ? "first" : "second", bases[0]->get_type(), nullptr,
          scope);
      auto *name = field->get_variables().front();
      name->set_cxx_field_index(index);
      SageInterface::appendStatement(field, scope);
      fields.push_back(name);
    }
    auto *destructor = SageBuilder::buildDefiningMemberFunctionDeclaration(
        ownership, "~Owner", SageBuilder::buildVoidType(),
        SageBuilder::buildFunctionParameterList_nfi(), scope);
    auto *epilogue = SageBuilder::buildExprListExp_nfi();
    using Step = SgCxxSubobjectDestruction;
    if (form == Case::missing_member_index)
      fields[0]->set_cxx_field_index(std::nullopt);
    for (unsigned int position = 0; position != 2; ++position) {
      const auto index =
          form == Case::swapped_members ? position : 1 - position;
      if (form == Case::omitted_member && index == 0)
        continue;
      epilogue->append_expression(new Step(destructors[0], fields[index],
                                           nullptr, Step::e_member,
                                           SageBuilder::buildVoidType()));
    }
    const auto append = [&](size_t index, Step::target_kind_enum kind) {
      if (index == 0 &&
          (form == Case::omitted_direct || form == Case::omitted_virtual))
        return;
      epilogue->append_expression(new SgCxxSubobjectDestruction(
          destructors[index], nullptr, bases[index], kind,
          SageBuilder::buildVoidType()));
    };
    if (form == Case::swapped_direct) {
      append(0, Step::e_nonvirtual_base);
      append(1, Step::e_nonvirtual_base);
    } else {
      append(1, Step::e_nonvirtual_base);
      if (form == Case::swapped_virtual)
        append(2, Step::e_virtual_base);
      append(form == Case::foreign ? 2 : 0, indirect || form == Case::wrong_role
                                                ? Step::e_virtual_base
                                                : Step::e_nonvirtual_base);
      if (form == Case::nested_virtual || form == Case::spurious_indirect)
        append(2, Step::e_virtual_base);
    }
    destructor->get_definition()->adopt_destructor_epilogue(epilogue, fields);
    EXPECT_EQ(epilogue->get_parent(), destructor->get_definition());
  };
  construct(Case::direct);
  construct(Case::indirect_virtual);
  construct(Case::nested_virtual);
  construct(Case::cached_indirect);
  construct(Case::foreign_signature_owner);
  EXPECT_DEATH(construct(Case::swapped_direct), "direct_order");
  EXPECT_DEATH(construct(Case::foreign), "direct_order");
  EXPECT_DEATH(construct(Case::wrong_role), "virtual_order");
  EXPECT_DEATH(construct(Case::cycle), "state->second");
  EXPECT_DEATH(construct(Case::swapped_virtual), "virtual_order");
  EXPECT_DEATH(construct(Case::spurious_indirect), "virtual_order");
  EXPECT_DEATH(construct(Case::swapped_members), "previous_member");
  EXPECT_DEATH(construct(Case::missing_member_index), "is_valid_target");
  EXPECT_DEATH(construct(Case::omitted_direct), "destructor_selected_trivial");
  EXPECT_DEATH(construct(Case::omitted_virtual), "destructor_selected_trivial");
  EXPECT_DEATH(construct(Case::omitted_member),
               "has_complete_member_destruction");
}
