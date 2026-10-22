/*
 * Unit tests for the type Equivalence check
 * TODO:
 *       All built-in types
 *       Reference types
 *       Named types
 *       Template types
 *       Anonymous types
 *       Modifier types
 *       Modified types
 *       Function types
 *       Opaque types
 *       XXX
 *       What about other language-specific types? They should be covered too.
 *       Fortran types..?
 */
#include <cassert>
#include <memory>
#include <vector>

#include "testSupport.h"

namespace {

struct TypeTestTranslationUnit {
  SgProject *project;
  SgGlobal *global;
};

TypeTestTranslationUnit
buildTypeTestTranslationUnit(const std::string &filename) {
  SgProject *project = new SgProject();
  ROSE_ASSERT(project != nullptr);
  project->get_fileList().clear();
  project->set_compileOnly(true);
  project->get_originalCommandLineArgumentList() = {"c++", "-c"};
  SgSourceFile *source =
      SageBuilder::buildGeneratedSourceFile(filename, project);
  ROSE_ASSERT(source != nullptr);
  ROSE_ASSERT(source->get_project() == project);
  ROSE_ASSERT(source->get_globalScope() != nullptr);
  return {project, source->get_globalScope()};
}

} // namespace

TEST(SageInterfaceTypeEquivalence, HandlesEmptyInput) {
  ::SgTypeInt *int_t1 = SageBuilder::buildIntType();
  bool tcRes = SageInterface::checkTypesAreEqual(int_t1, NULL);
  EXPECT_EQ(tcRes, false);

  tcRes = SageInterface::checkTypesAreEqual(NULL, int_t1);
  EXPECT_EQ(tcRes, false);
}

/*
 * We rely on the build mechanism that SageBuilder provides
 * We construct all (yet a lot have to be added) built-int types
 * and check pairs of the same type
 */
TEST(SageInterfaceTypeEquivalence, BuiltInIntIsEqual) {
  ::SgTypeInt *int_t1 = SageBuilder::buildIntType();
  ::SgTypeInt *int_t2 = SageBuilder::buildIntType();
  bool tcRes = SageInterface::checkTypesAreEqual(int_t1, int_t2);
  EXPECT_EQ(tcRes, true);
}

TEST(SageInterfaceTypeEquivalence, BuiltInShortIsEqual) {
  ::SgTypeShort *short_t1 = SageBuilder::buildShortType();
  ::SgTypeShort *short_t2 = SageBuilder::buildShortType();
  bool tcRes = SageInterface::checkTypesAreEqual(short_t1, short_t2);
  EXPECT_EQ(tcRes, true);
}

TEST(SageInterfaceTypeEquivalence, BuiltInBoolIsEqual) {
  ::SgTypeBool *bool_t1 = SageBuilder::buildBoolType();
  ::SgTypeBool *bool_t2 = SageBuilder::buildBoolType();
  bool tcRes = SageInterface::checkTypesAreEqual(bool_t1, bool_t2);
  EXPECT_EQ(tcRes, true);
}

TEST(SageInterfaceTypeEquivalence, BuiltInVoidIsEqual) {
  ::SgTypeVoid *void_t1 = SageBuilder::buildVoidType();
  ::SgTypeVoid *void_t2 = SageBuilder::buildVoidType();
  bool tcRes = SageInterface::checkTypesAreEqual(void_t1, void_t2);
  EXPECT_EQ(tcRes, true);
}

/* XXX For now we test for structural equivalence, so they are not equal! */
TEST(SageInterfaceTypeEquivalence, BuiltInVolatileIntIntIsEqual) {
  ::SgModifierType *int_t3 =
      SageBuilder::buildVolatileType(SageBuilder::buildIntType());
  ::SgTypeInt *int_t4 = SageBuilder::buildIntType();
  bool tcRes = SageInterface::checkTypesAreEqual(int_t3, int_t4);
  EXPECT_EQ(tcRes, false);
}
/*
 * We construct pairs of different types and check them.
 * The result for each test should of course be false
 */
TEST(SageInterfaceTypeEquivalence, BuiltInIntBoolAreUnequal) {
  ::SgTypeInt *int_t1 = SageBuilder::buildIntType();
  ::SgTypeBool *bool_t2 = SageBuilder::buildBoolType();
  bool tcRes = SageInterface::checkTypesAreEqual(int_t1, bool_t2);
  EXPECT_EQ(tcRes, false);
}

TEST(SageInterfaceTypeEquivalence, BuiltInShortIntAreUnequal) {
  ::SgTypeShort *short_t1 = SageBuilder::buildShortType();
  ::SgTypeInt *int_t2 = SageBuilder::buildIntType();
  bool tcRes = SageInterface::checkTypesAreEqual(short_t1, int_t2);
  EXPECT_EQ(tcRes, false);
}

TEST(SageInterfaceTypeEquivalence, BuiltInIntVoidAreUnequal) {
  ::SgTypeInt *int_t3 = SageBuilder::buildIntType();
  ::SgTypeVoid *void_t2 = SageBuilder::buildVoidType();
  bool tcRes = SageInterface::checkTypesAreEqual(int_t3, void_t2);
  EXPECT_EQ(tcRes, false);
}

TEST(SageInterfaceTypeEquivalence, BuiltInConstIntIntAreUnequal) {
  ::SgModifierType *int_t4 =
      SageBuilder::buildConstType(SageBuilder::buildIntType());
  ::SgTypeInt *int_t5 = SageBuilder::buildIntType();
  bool tcRes = SageInterface::checkTypesAreEqual(int_t4, int_t5);
  EXPECT_EQ(tcRes, false);
}

/*
 * Pointer types to the same base type.
 * A lot of things need to be added here. But this is meant as a first
 * safety net.
 */
TEST(SageInterfaceTypeEquivalence, IntPointerTypesAreEqual) {
  ::SgPointerType *p_1 =
      SageBuilder::buildPointerType(SageBuilder::buildIntType());
  ::SgPointerType *p_2 =
      SageBuilder::buildPointerType(SageBuilder::buildIntType());
  bool tcRes = SageInterface::checkTypesAreEqual(p_1, p_2);
  EXPECT_EQ(tcRes, true);
}

TEST(SageInterfaceTypeEquivalence, VoidPointerTypesAreEqual) {
  ::SgPointerType *p_3 =
      SageBuilder::buildPointerType(SageBuilder::buildVoidType());
  ::SgPointerType *p_4 =
      SageBuilder::buildPointerType(SageBuilder::buildVoidType());
  bool tcRes = SageInterface::checkTypesAreEqual(p_3, p_4);
  EXPECT_EQ(tcRes, true);
}

TEST(SageInterfaceTypeEquivalence, VoidPointerPointerTypesAreEqual) {
  ::SgPointerType *p_3 =
      SageBuilder::buildPointerType(SageBuilder::buildVoidType());
  ::SgPointerType *p_4 =
      SageBuilder::buildPointerType(SageBuilder::buildVoidType());
  ::SgPointerType *p_5 = SageBuilder::buildPointerType(p_3);
  ::SgPointerType *p_6 = SageBuilder::buildPointerType(p_4);
  bool tcRes = SageInterface::checkTypesAreEqual(p_5, p_6);
  EXPECT_EQ(tcRes, true);
}

/* Here we check for structural equivalence, so they are not equal! */
TEST(SageInterfaceTypeEquivalence, RestrictIntPointerIntPointerIsEqual) {
  ::SgModifierType *int_t3 = SageBuilder::buildRestrictType(
      SageBuilder::buildPointerType(SageBuilder::buildIntType()));
  ::SgPointerType *int_t4 =
      SageBuilder::buildPointerType(SageBuilder::buildIntType());
  bool tcRes = SageInterface::checkTypesAreEqual(int_t3, int_t4);
  EXPECT_EQ(tcRes, false);
}

/*
 * We construct pointer types to different base types.
 */
TEST(SageInterfaceTypeEquivalence, IntCharPointerTypesAreUnequal) {
  ::SgPointerType *p_1 =
      SageBuilder::buildPointerType(SageBuilder::buildIntType());
  ::SgPointerType *p_2 =
      SageBuilder::buildPointerType(SageBuilder::buildCharType());
  bool tcRes = SageInterface::checkTypesAreEqual(p_1, p_2);
  EXPECT_EQ(tcRes, false);
}

TEST(SageInterfaceTypeEquivalence, VoidIntPointerTypesAreUnequal) {
  ::SgPointerType *p_3 =
      SageBuilder::buildPointerType(SageBuilder::buildVoidType());
  ::SgPointerType *p_4 =
      SageBuilder::buildPointerType(SageBuilder::buildIntType());
  bool tcRes = SageInterface::checkTypesAreEqual(p_3, p_4);
  EXPECT_EQ(tcRes, false);
}

TEST(SageInterfaceTypeEquivalence, VoidIntPointerPointerTypesAreUnequal) {
  ::SgPointerType *p_3 =
      SageBuilder::buildPointerType(SageBuilder::buildVoidType());
  ::SgPointerType *p_4 =
      SageBuilder::buildPointerType(SageBuilder::buildIntType());
  ::SgPointerType *p_5 = SageBuilder::buildPointerType(p_3);
  ::SgPointerType *p_6 = SageBuilder::buildPointerType(p_4);
  bool tcRes = SageInterface::checkTypesAreEqual(p_5, p_6);
  EXPECT_EQ(tcRes, false);
}

TEST(SageInterfaceTypeEquivalence, ConstIntIntPointerTypesAreUnequal) {
  ::SgPointerType *p_7 = SageBuilder::buildPointerType(
      SageBuilder::buildConstType(SageBuilder::buildIntType()));
  ::SgPointerType *p_8 =
      SageBuilder::buildPointerType(SageBuilder::buildIntType());
  bool tcRes = SageInterface::checkTypesAreEqual(p_7, p_8);
  EXPECT_EQ(tcRes, false);
}

/*
 * Reference types
 */
TEST(SageInterfaceTypeEquivalence, ReferenceTypesAreEqual) {
  ::SgReferenceType *r_1 =
      SageBuilder::buildReferenceType(SageBuilder::buildIntType());
  ::SgReferenceType *r_2 =
      SageBuilder::buildReferenceType(SageBuilder::buildIntType());
  bool tcRef = SageInterface::checkTypesAreEqual(r_1, r_2);
  EXPECT_EQ(tcRef, true);
}

TEST(SageInterfaceTypeEquivalence, RvalueReferenceCopiesPreserveEquivalence) {
  auto *integer = SageBuilder::buildIntType();
  auto copy = std::make_unique<SgRvalueReferenceType>(integer);
  EXPECT_TRUE(SageInterface::isEquivalentType(
      copy.get(), SageBuilder::buildRvalueReferenceType(integer)));
  EXPECT_FALSE(SageInterface::isEquivalentType(
      copy.get(), SageBuilder::buildReferenceType(integer)));
  EXPECT_FALSE(SageInterface::isEquivalentType(
      copy.get(),
      SageBuilder::buildRvalueReferenceType(SageBuilder::buildDoubleType())));
}

TEST(SageInterfaceTypeEquivalence, VoidReferenceTypesAreEqual) {
  ::SgReferenceType *r_3 =
      SageBuilder::buildReferenceType(SageBuilder::buildVoidType());
  ::SgReferenceType *r_4 =
      SageBuilder::buildReferenceType(SageBuilder::buildVoidType());
  bool tcRef = SageInterface::checkTypesAreEqual(r_3, r_4);
  EXPECT_EQ(tcRef, true);
}

TEST(SageInterfaceTypeEquivalence, ReferenceTypesAreUnequal) {
  ::SgReferenceType *r_1 =
      SageBuilder::buildReferenceType(SageBuilder::buildIntType());
  ::SgReferenceType *r_2 =
      SageBuilder::buildReferenceType(SageBuilder::buildVoidType());
  bool tcRef = SageInterface::checkTypesAreEqual(r_1, r_2);
  EXPECT_EQ(tcRef, false);
}

TEST(SageInterfaceTypeEquivalence,
     VoidPointerReferenceVoidReferenceAreUnequal) {
  ::SgReferenceType *r_3 = SageBuilder::buildReferenceType(
      SageBuilder::buildPointerType(SageBuilder::buildVoidType()));
  ::SgReferenceType *r_4 =
      SageBuilder::buildReferenceType(SageBuilder::buildVoidType());
  bool tcRef = SageInterface::checkTypesAreEqual(r_3, r_4);
  EXPECT_EQ(tcRef, false);
}

/*
 * Array Types
 */
TEST(SageInterfaceTypeEquivalence, IntArrayTypesAreEqual) {
  ::SgArrayType *a_1 = SageBuilder::buildArrayType(SageBuilder::buildIntType(),
                                                   SageBuilder::buildIntVal(3));
  ::SgArrayType *a_2 = SageBuilder::buildArrayType(SageBuilder::buildIntType(),
                                                   SageBuilder::buildIntVal(3));
  bool tcRef = SageInterface::checkTypesAreEqual(a_1, a_2);
  EXPECT_EQ(tcRef, true);
  EXPECT_TRUE(SageInterface::isEquivalentType(a_1, a_2));
}

TEST(SageInterfaceTypeEquivalence, CharArrayIsEqual) {
  ::SgArrayType *a_3 = SageBuilder::buildArrayType(
      SageBuilder::buildCharType(), SageBuilder::buildIntVal(100));
  ::SgArrayType *a_4 = SageBuilder::buildArrayType(
      SageBuilder::buildCharType(), SageBuilder::buildIntVal(100));
  bool tcRef = SageInterface::checkTypesAreEqual(a_3, a_4);
  EXPECT_EQ(tcRef, true);
}

TEST(SageInterfaceTypeEquivalence, CharArrayArrayIsEqual) {
  ::SgArrayType *a_3 = SageBuilder::buildArrayType(
      SageBuilder::buildCharType(), SageBuilder::buildIntVal(100));
  ::SgArrayType *a_4 = SageBuilder::buildArrayType(
      SageBuilder::buildCharType(), SageBuilder::buildIntVal(100));
  ::SgArrayType *a_5 =
      SageBuilder::buildArrayType(a_3, SageBuilder::buildIntVal(100));
  ::SgArrayType *a_6 =
      SageBuilder::buildArrayType(a_4, SageBuilder::buildIntVal(100));
  bool tcRef = SageInterface::checkTypesAreEqual(a_5, a_6);
  EXPECT_EQ(tcRef, true);
}

TEST(SageInterfaceTypeEquivalence, IntCharArrayTypesAreUnequal) {
  ::SgArrayType *a_1 = SageBuilder::buildArrayType(
      SageBuilder::buildCharType(), SageBuilder::buildIntVal(42));
  ::SgArrayType *a_2 = SageBuilder::buildArrayType(
      SageBuilder::buildIntType(), SageBuilder::buildIntVal(42));
  bool tcRef = SageInterface::checkTypesAreEqual(a_1, a_2);
  EXPECT_EQ(tcRef, false);
}

TEST(SageInterfaceTypeEquivalence, IntArrayDifferentDimensionAreUnequal) {
  ::SgArrayType *a_3 = SageBuilder::buildArrayType(
      SageBuilder::buildCharType(), SageBuilder::buildIntVal(42));
  ::SgArrayType *a_4 = SageBuilder::buildArrayType(
      SageBuilder::buildCharType(), SageBuilder::buildIntVal(21));
  bool tcRef = SageInterface::checkTypesAreEqual(a_3, a_4);
  EXPECT_EQ(tcRef, false);
  EXPECT_FALSE(SageInterface::isEquivalentType(a_3, a_4));
  EXPECT_FALSE(SageInterface::isEquivalentType(
      SageBuilder::buildPointerType(a_3), SageBuilder::buildPointerType(a_4)));
}

TEST(SageInterfaceTypeEquivalence, ConstCharArrayCharArrayAreUnequal) {
  ::SgArrayType *a_3 = SageBuilder::buildArrayType(
      SageBuilder::buildCharType(), SageBuilder::buildIntVal(42));
  ::SgArrayType *a_5 = SageBuilder::buildArrayType(
      SageBuilder::buildConstType(SageBuilder::buildCharType()),
      SageBuilder::buildIntVal(42));
  bool tcRef = SageInterface::checkTypesAreEqual(a_5, a_3);
  EXPECT_EQ(tcRef, false);
}

/*
 * Typedef tests
 */
TEST(SageInterfaceTypeEquivalence, TypedefTypesAreEqual) {
  TypeTestTranslationUnit unit =
      buildTypeTestTranslationUnit("types_are_equal_typedef.cpp");
  ::SgTypedefDeclaration *t_1 = SageBuilder::buildTypedefDeclaration(
      SageBuilder::typedef_declaration_ownership::semanticAuxiliary(),
      SgTypedefDeclaration::e_typedef, "MyInt", SageBuilder::buildIntType(),
      unit.global);
  ::SgTypeInt *t_2 = SageBuilder::buildIntType();
  bool tcRef = SageInterface::checkTypesAreEqual(t_1->get_type(), t_2);
  EXPECT_EQ(tcRef, true);
  delete unit.project;
}

TEST(SageInterfaceTypeEquivalence, TypedefTypesAreUnequal) {
  TypeTestTranslationUnit unit =
      buildTypeTestTranslationUnit("types_are_unequal_typedef.cpp");
  ::SgTypedefDeclaration *t_1 = SageBuilder::buildTypedefDeclaration(
      SageBuilder::typedef_declaration_ownership::semanticAuxiliary(),
      SgTypedefDeclaration::e_typedef, "MyInt", SageBuilder::buildIntType(),
      unit.global);
  ::SgTypeChar *t_2 = SageBuilder::buildCharType();
  bool tcRef = SageInterface::checkTypesAreEqual(t_1->get_type(), t_2);
  EXPECT_EQ(tcRef, false);
  delete unit.project;
}

TEST(SageInterfaceTypeEquivalence, FunctionCategoriesAndQualifiersDiffer) {
  SgType *result = SageBuilder::buildIntType();
  auto *variadic_arguments = new SgFunctionParameterTypeList();
  variadic_arguments->append_argument(SgTypeEllipse::createType());
  std::vector<SgFunctionType *> types{
      new SgFunctionType(result, false, new SgFunctionParameterTypeList()),
      new SgFunctionType(result, true, variadic_arguments),
      new SgMemberFunctionType(result, false, new SgFunctionParameterTypeList(),
                               nullptr, 0),
      new SgMemberFunctionType(result, false, new SgFunctionParameterTypeList(),
                               nullptr, SgMemberFunctionType::e_const),
      new SgMemberFunctionType(result, false, new SgFunctionParameterTypeList(),
                               nullptr, SgMemberFunctionType::e_volatile),
      new SgMemberFunctionType(result, false, new SgFunctionParameterTypeList(),
                               nullptr,
                               SgMemberFunctionType::e_ref_qualifier_lvalue),
      new SgMemberFunctionType(result, false, new SgFunctionParameterTypeList(),
                               nullptr,
                               SgMemberFunctionType::e_ref_qualifier_rvalue)};
  for (size_t left = 0; left < types.size(); ++left) {
    for (size_t right = 0; right < types.size(); ++right) {
      EXPECT_EQ(
          SageInterface::isEquivalentFunctionType(types[left], types[right]),
          left == right);
    }
  }
  auto *equivalent =
      new SgFunctionType(result, false, new SgFunctionParameterTypeList());
  EXPECT_TRUE(
      SageInterface::isEquivalentFunctionType(types.front(), equivalent));
  types.push_back(equivalent);
  for (SgFunctionType *type : types) {
    delete type;
  }
}

TEST(SageInterfaceTypeEquivalence, NonrealNamesInSameScopeAreDistinct) {
  auto unit = buildTypeTestTranslationUnit("rex_nonreal_type_identity.cpp");
  auto *left =
      SageBuilder::buildSemanticNonrealType("Left", unit.global, nullptr);
  auto *right =
      SageBuilder::buildSemanticNonrealType("Right", unit.global, nullptr);
  auto *repeated =
      SageBuilder::buildSemanticNonrealType("Left", unit.global, nullptr);
  EXPECT_TRUE(SageInterface::isEquivalentType(left, repeated));
  EXPECT_FALSE(SageInterface::isEquivalentType(left, right));
  EXPECT_FALSE(SageInterface::isEquivalentType(right, left));
  EXPECT_TRUE(SageInterface::checkTypesAreEqual(left, repeated));
  EXPECT_FALSE(SageInterface::checkTypesAreEqual(left, right));
  delete unit.project;
}

/*
 *
 * We now test function type equivalence
 *
 * It is basically iterating over the types a function takes and
 * returns. The naming convention is
 * Returntype X FunctionArgType1FunctionArgType2Etc
 * so void foo() has type VoidXVoidFunctionType
 *
 */
TEST_F(CleanFunctionTypeTableFixture, VoidXIntAndVoidXIntEqual) {
  ::SgFunctionParameterTypeList *paramList = new SgFunctionParameterTypeList();
  paramList->append_argument(SageBuilder::buildIntType());
  ::SgFunctionType *ft1 =
      SageBuilder::buildFunctionType(SageBuilder::buildVoidType(), paramList);
  ::SgFunctionType *ft2 =
      SageBuilder::buildFunctionType(SageBuilder::buildVoidType(), paramList);
  EXPECT_EQ(SgNode::get_globalFunctionTypeTable()->size(), 1);
  bool tcRef = SageInterface::checkTypesAreEqual(ft1, ft2);
  EXPECT_EQ(tcRef, true);
}

TEST_F(CleanFunctionTypeTableFixture, VoidXIntIntAndVoidXIntIntEqual) {
  ::SgFunctionParameterTypeList *paramList = new SgFunctionParameterTypeList();
  paramList->append_argument(SageBuilder::buildIntType());
  paramList->append_argument(SageBuilder::buildIntType());
  ::SgFunctionType *ft1 =
      SageBuilder::buildFunctionType(SageBuilder::buildVoidType(), paramList);
  ::SgFunctionType *ft2 =
      SageBuilder::buildFunctionType(SageBuilder::buildVoidType(), paramList);
  bool tcRef = SageInterface::checkTypesAreEqual(ft1, ft2);
  EXPECT_EQ(tcRef, true);
}

TEST_F(CleanFunctionTypeTableFixture,
       VoidXCharConstIntAndVoidXCharConstIntEqual) {
  auto *paramList = SageBuilder::buildFunctionParameterTypeList(
      SageBuilder::buildCharType(),
      SageBuilder::buildConstType(SageBuilder::buildIntType()));
  auto *paramList2 = SageBuilder::buildFunctionParameterTypeList(
      SageBuilder::buildCharType(),
      SageBuilder::buildConstType(SageBuilder::buildIntType()));
  ::SgFunctionType *ft1 =
      SageBuilder::buildFunctionType(SageBuilder::buildVoidType(), paramList);
  ::SgFunctionType *ft2 =
      SageBuilder::buildFunctionType(SageBuilder::buildVoidType(), paramList2);
  bool tcRef = SageInterface::checkTypesAreEqual(ft1, ft2);
  EXPECT_EQ(tcRef, true);
  ROSE_ASSERT(paramList2->get_parent() == nullptr);
  delete paramList2;
}

TEST_F(CleanFunctionTypeTableFixture, IntXVoidAndIntXVoidEqual) {
  ::SgFunctionParameterTypeList *paramList = new SgFunctionParameterTypeList();
  paramList->append_argument(SageBuilder::buildVoidType());
  ::SgFunctionType *ft1 =
      SageBuilder::buildFunctionType(SageBuilder::buildIntType(), paramList);
  ::SgFunctionType *ft2 =
      SageBuilder::buildFunctionType(SageBuilder::buildIntType(), paramList);
  bool tcRef = SageInterface::checkTypesAreEqual(ft1, ft2);
  EXPECT_EQ(tcRef, true);
}

/*
 * Function types unequal start
 */

TEST_F(CleanFunctionTypeTableFixture, IntXVoidAndConstIntXVoidUnequal) {
  ::SgFunctionParameterTypeList *paramList = new SgFunctionParameterTypeList();
  paramList->append_argument(SageBuilder::buildVoidType());
  auto *paramList2 = new SgFunctionParameterTypeList();
  paramList2->append_argument(SageBuilder::buildVoidType());
  ::SgFunctionType *ft1 =
      SageBuilder::buildFunctionType(SageBuilder::buildIntType(), paramList);
  ::SgFunctionType *ft2 = SageBuilder::buildFunctionType(
      SageBuilder::buildConstType(SageBuilder::buildIntType()), paramList2);
  bool tcRef = SageInterface::checkTypesAreEqual(ft1, ft2);
  EXPECT_EQ(tcRef, false);
}

TEST_F(CleanFunctionTypeTableFixture, VoidXVoidAndIntXVoidUnequal) {
  ::SgFunctionParameterTypeList *paramList = new SgFunctionParameterTypeList();
  paramList->append_argument(SageBuilder::buildVoidType());
  auto *paramList2 = new SgFunctionParameterTypeList();
  paramList2->append_argument(SageBuilder::buildVoidType());
  ::SgFunctionType *ft1 =
      SageBuilder::buildFunctionType(SageBuilder::buildVoidType(), paramList);
  ::SgFunctionType *ft2 =
      SageBuilder::buildFunctionType(SageBuilder::buildIntType(), paramList2);
  bool tcRef = SageInterface::checkTypesAreEqual(ft1, ft2);
  EXPECT_EQ(tcRef, false);
}
TEST_F(CleanFunctionTypeTableFixture, VoidXIntIntAndVoidXIntUnequal) {
  SgFunctionTypeTable *ftt = SgNode::get_globalFunctionTypeTable();

  ::SgFunctionParameterTypeList *paramList = new SgFunctionParameterTypeList();
  paramList->append_argument(SageBuilder::buildIntType());
  paramList->append_argument(SageBuilder::buildIntType());
  EXPECT_EQ(paramList->get_arguments().size(), 2);

  ::SgFunctionParameterTypeList *paramList2 = new SgFunctionParameterTypeList();
  paramList2->append_argument(SageBuilder::buildIntType());
  EXPECT_EQ(paramList2->get_arguments().size(), 1);

  ::SgFunctionType *ft1 =
      SageBuilder::buildFunctionType(SageBuilder::buildBoolType(), paramList);
  ::SgFunctionType *ft2 =
      SageBuilder::buildFunctionType(SageBuilder::buildBoolType(), paramList2);

  EXPECT_EQ(ft1->get_arguments().size(), 2);
  EXPECT_EQ(ft2->get_arguments().size(), 1);

  EXPECT_EQ(ft1->get_argument_list()->get_arguments().size(), 2);
  EXPECT_EQ(ft2->get_argument_list()->get_arguments().size(), 1);

  bool tcRef = SageInterface::checkTypesAreEqual(ft1, ft2);
  EXPECT_EQ(tcRef, false);
}

TEST_F(CleanFunctionTypeTableFixture, VoidXIntIntAndVoidXIntConstIntEqual) {
  ::SgFunctionParameterTypeList *paramList = new SgFunctionParameterTypeList();
  paramList->append_argument(SageBuilder::buildIntType());
  paramList->append_argument(SageBuilder::buildIntType());
  SgType *declaredConstInt =
      SageBuilder::buildConstType(SageBuilder::buildIntType());
  auto *paramList2 = SageBuilder::buildFunctionParameterTypeList(
      SageBuilder::buildIntType(), declaredConstInt);
  EXPECT_TRUE(SageInterface::isConstType(declaredConstInt));
  EXPECT_EQ(paramList2->get_arguments().size(), 2);
  EXPECT_EQ(paramList2->get_arguments()[1], SageBuilder::buildIntType());
  ::SgFunctionType *ft1 =
      SageBuilder::buildFunctionType(SageBuilder::buildVoidType(), paramList);
  ::SgFunctionType *ft2 =
      SageBuilder::buildFunctionType(SageBuilder::buildVoidType(), paramList2);
  bool tcRef = SageInterface::checkTypesAreEqual(ft1, ft2);
  EXPECT_EQ(tcRef, true);
}
