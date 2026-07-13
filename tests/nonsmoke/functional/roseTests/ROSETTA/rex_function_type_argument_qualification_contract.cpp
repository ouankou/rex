#include "nonrealQualificationSupport.h"
#include "testSupport.h"

#include <type_traits>
#include <utility>

template <typename T, typename = void>
struct HasAppendArgument : std::false_type {};

template <typename T>
struct HasAppendArgument<
    T, std::void_t<decltype(std::declval<T &>().append_argument(
           std::declval<SgType *>()))>> : std::true_type {};

template <typename T, typename = void>
struct HasInsertArgument : std::false_type {};

template <typename T>
struct HasInsertArgument<
    T, std::void_t<decltype(std::declval<T &>().insert_argument(
           std::declval<size_t>(), std::declval<SgType *>()))>>
    : std::true_type {};

static_assert(
    std::is_same<decltype(std::declval<SgFunctionType &>().get_arguments()),
                 const SgTypePtrList &>::value,
    "function types must not expose their canonical argument list for raw "
    "mutation");
static_assert(
    std::is_same<
        decltype(std::declval<SgFunctionParameterTypeList &>().get_arguments()),
        const SgTypePtrList &>::value,
    "function parameter type lists must only mutate through their atomic "
    "typed APIs");
static_assert(!HasAppendArgument<SgFunctionType>::value,
              "canonical function types must not be mutable after interning");
static_assert(!HasInsertArgument<SgFunctionType>::value,
              "canonical function types must not be mutable after interning");

TEST(FunctionTypeArgumentQualification, AppendCreatesExactOwnedIdentity) {
  SgFunctionParameterTypeList *arguments = new SgFunctionParameterTypeList();
  SgType *type = SageBuilder::buildIntType();

  arguments->append_argument(type);

  ASSERT_EQ(arguments->get_arguments().size(), 1);
  ASSERT_EQ(arguments->get_argument_qualification_use_sites().size(), 1);
  SgFunctionTypeArgument *use_site =
      arguments->get_argument_qualification_use_sites().front();
  ASSERT_NE(use_site, nullptr);
  EXPECT_EQ(use_site->get_type(), type);
  EXPECT_EQ(use_site->get_parent(), arguments);
  EXPECT_FALSE(use_site->get_is_pack_expansion());
  use_site->set_is_pack_expansion(true);
  EXPECT_TRUE(use_site->get_is_pack_expansion());
}

TEST(FunctionTypeArgumentQualification, RepeatedTypesHaveDistinctIdentities) {
  SgFunctionParameterTypeList *arguments = new SgFunctionParameterTypeList();
  SgType *type = SageBuilder::buildIntType();

  arguments->append_argument(type);
  arguments->append_argument(type);

  const SgFunctionTypeArgumentPtrList &use_sites =
      arguments->get_argument_qualification_use_sites();
  ASSERT_EQ(use_sites.size(), 2);
  EXPECT_NE(use_sites[0], use_sites[1]);
  EXPECT_EQ(use_sites[0]->get_type(), type);
  EXPECT_EQ(use_sites[1]->get_type(), type);
  EXPECT_EQ(use_sites[0]->get_parent(), arguments);
  EXPECT_EQ(use_sites[1]->get_parent(), arguments);
}

TEST(FunctionTypeArgumentQualification, InsertKeepsContainersAligned) {
  SgFunctionParameterTypeList *arguments = new SgFunctionParameterTypeList();
  SgType *first = SageBuilder::buildIntType();
  SgType *second = SageBuilder::buildBoolType();

  arguments->append_argument(first);
  arguments->insert_argument(0, second);

  ASSERT_EQ(arguments->get_arguments().size(), 2);
  ASSERT_EQ(arguments->get_argument_qualification_use_sites().size(), 2);
  EXPECT_EQ(arguments->get_arguments()[0], second);
  EXPECT_EQ(arguments->get_arguments()[1], first);
  EXPECT_EQ(arguments->get_argument_qualification_use_sites()[0]->get_type(),
            second);
  EXPECT_EQ(arguments->get_argument_qualification_use_sites()[1]->get_type(),
            first);
}

TEST(FunctionTypeArgumentQualification,
     BuilderDerivesVariadicIdentityFromFinalEllipsis) {
  SgType *integer_type = SageBuilder::buildIntType();
  SgType *ellipsis_type = SgTypeEllipse::createType();
  SgFunctionParameterTypeList *first_arguments =
      new SgFunctionParameterTypeList();
  first_arguments->append_argument(integer_type);
  first_arguments->append_argument(ellipsis_type);

  SgFunctionType *first_type = SageBuilder::buildFunctionType(
      SageBuilder::buildVoidType(), first_arguments);

  ASSERT_NE(first_type, nullptr);
  ASSERT_EQ(first_type->get_argument_list(), first_arguments);
  EXPECT_TRUE(first_type->get_has_ellipses());
  ASSERT_EQ(first_type->get_arguments().size(), 2);
  EXPECT_EQ(first_type->get_arguments().back(), ellipsis_type);

  SgFunctionParameterTypeList *equivalent_arguments =
      new SgFunctionParameterTypeList();
  equivalent_arguments->append_argument(integer_type);
  equivalent_arguments->append_argument(ellipsis_type);
  SgFunctionType *equivalent_type = SageBuilder::buildFunctionType(
      SageBuilder::buildVoidType(), equivalent_arguments);

  EXPECT_EQ(equivalent_type, first_type);
  EXPECT_TRUE(equivalent_type->get_has_ellipses());
  EXPECT_EQ(equivalent_arguments->get_parent(), nullptr);
  SageInterface::deleteAST(
      equivalent_arguments,
      SageInterface::DeleteAstMode::kSkipExternalReferences);
}

TEST(FunctionTypeArgumentQualification, BuilderRejectsNonFinalEllipsis) {
  EXPECT_DEATH(
      {
        SgFunctionParameterTypeList *arguments =
            new SgFunctionParameterTypeList();
        arguments->append_argument(SgTypeEllipse::createType());
        arguments->append_argument(SageBuilder::buildIntType());
        (void)SageBuilder::buildFunctionType(SageBuilder::buildVoidType(),
                                             arguments);
      },
      "non-final ellipsis");
}

TEST(NonrealQualification,
     ExplicitEmptySourcePayloadOverridesSynthesizedSemanticChain) {
  SgGlobal *global = new SgGlobal();
  SgNonrealType *outer = SageBuilder::buildSemanticNonrealType(
      SgName("rex_semantic_namespace"), global, nullptr, nullptr);
  ASSERT_NE(outer, nullptr);
  SgNonrealDecl *outer_declaration = isSgNonrealDecl(outer->get_declaration());
  ASSERT_NE(outer_declaration, nullptr);
  SgDeclarationScope *outer_scope = outer_declaration->get_nonreal_decl_scope();
  ASSERT_NE(outer_scope, nullptr);

  SgNonrealType *terminal = SageBuilder::buildSemanticNonrealType(
      SgName("rex_unqualified_template"), outer_scope, nullptr, nullptr);
  ASSERT_NE(terminal, nullptr);
  SgNonrealDecl *terminal_declaration =
      isSgNonrealDecl(terminal->get_declaration());
  ASSERT_NE(terminal_declaration, nullptr);

  terminal_declaration->set_source_name_qualification_present(true);
  terminal_declaration->set_source_name_global_qualification(false);
  terminal_declaration->get_source_name_qualification_tokens().clear();

  EXPECT_FALSE(SageInterface::nonrealTypeCarriesWrittenQualification(terminal));
}

TEST(NonrealQualification,
     WrittenPayloadOverridesUnownedSemanticStructuralChain) {
  SgGlobal *global = new SgGlobal();
  SgNonrealType *outer = SageBuilder::buildSemanticNonrealType(
      SgName("rex_written_namespace"), global, nullptr, nullptr);
  ASSERT_NE(outer, nullptr);
  SgNonrealDecl *outer_declaration = isSgNonrealDecl(outer->get_declaration());
  ASSERT_NE(outer_declaration, nullptr);

  SgNonrealType *terminal = SageBuilder::buildSemanticNonrealType(
      SgName("rex_qualified_template"),
      outer_declaration->get_nonreal_decl_scope(), nullptr, nullptr);
  ASSERT_NE(terminal, nullptr);
  SgNonrealDecl *terminal_declaration =
      isSgNonrealDecl(terminal->get_declaration());
  ASSERT_NE(terminal_declaration, nullptr);

  terminal_declaration->set_source_name_qualification_present(true);
  terminal_declaration->set_source_name_global_qualification(false);
  terminal_declaration->get_source_name_qualification_tokens() = {
      "rex_written_namespace::"};
  EXPECT_TRUE(SageInterface::nonrealTypeCarriesWrittenQualification(terminal));

  terminal_declaration->set_source_name_qualification_present(false);
  terminal_declaration->get_source_name_qualification_tokens().clear();
  EXPECT_FALSE(SageInterface::nonrealTypeCarriesWrittenQualification(terminal));
  EXPECT_TRUE(
      SageInterface::nonrealTypeHasSemanticQualificationChain(terminal));
}
