#include "testSupport.h"

#include <limits>

TEST_F(CleanFunctionTypeTableFixture, GetsConstructed) {
  SgFunctionTypeTable *ftt = SgNode::get_globalFunctionTypeTable();
  ASSERT_EQ(isNull(ftt), false);
}

TEST(GlobalTypeTableTest, EmptyTypeTableCanPrintItself) {
  SgFunctionTypeTable *ftt = SgNode::get_globalFunctionTypeTable();
  ftt->print_functypetable();
  SUCCEED();
}

TEST_F(CleanFunctionTypeTableFixture, ConstructFunctionTypeIncreasesSizeByOne) {
  SgFunctionTypeTable *ftt = SgNode::get_globalFunctionTypeTable();
  ASSERT_EQ(isNull(ftt), false);
  EXPECT_EQ(ftt->size(), 0);
  SgFunctionParameterTypeList *params = new SgFunctionParameterTypeList();
  params->append_argument(SageBuilder::buildIntType());
  SgFunctionType *ft =
      SageBuilder::buildFunctionType(SageBuilder::buildVoidType(), params);
  EXPECT_EQ(ftt->size(), 1);
}

TEST_F(CleanFunctionTypeTableFixture,
       ConstructFunctionTypeTwiceIncreasesSizeByOne) {
  SgFunctionTypeTable *ftt = SgNode::get_globalFunctionTypeTable();
  ASSERT_EQ(isNull(ftt), false);
  EXPECT_EQ(ftt->size(), 0);
  SgFunctionParameterTypeList *params = new SgFunctionParameterTypeList();
  params->append_argument(SageBuilder::buildIntType());
  SgFunctionType *ft =
      SageBuilder::buildFunctionType(SageBuilder::buildVoidType(), params);
  EXPECT_EQ(ftt->size(), 1);
  SgFunctionType *ft2 =
      SageBuilder::buildFunctionType(SageBuilder::buildVoidType(), params);
  EXPECT_EQ(ftt->size(), 1);
  EXPECT_EQ(ft, ft2);
}

TEST_F(CleanFunctionTypeTableFixture,
       ConstructFunctionTypeWithTwoArgumentsTwiceIncreasesSizeByOne) {
  SgFunctionTypeTable *ftt = SgNode::get_globalFunctionTypeTable();
  ASSERT_EQ(isNull(ftt), false);
  EXPECT_EQ(ftt->size(), 0);
  SgFunctionParameterTypeList *params = new SgFunctionParameterTypeList();
  params->append_argument(SageBuilder::buildIntType());
  params->append_argument(SageBuilder::buildIntType());
  SgFunctionType *ft =
      SageBuilder::buildFunctionType(SageBuilder::buildVoidType(), params);
  EXPECT_EQ(ftt->size(), 1);
  SgFunctionType *ft2 =
      SageBuilder::buildFunctionType(SageBuilder::buildVoidType(), params);
  EXPECT_EQ(ftt->size(), 1);
  EXPECT_EQ(ft, ft2);
}

TEST_F(CleanFunctionTypeTableFixture,
       ConstructTwoDifferentFunctionsIncreasesSizeByTwo) {
  SgFunctionTypeTable *ftt = SgNode::get_globalFunctionTypeTable();
  ASSERT_EQ(isNull(ftt), false);
  EXPECT_EQ(ftt->size(), 0);
  SgFunctionParameterTypeList *params = new SgFunctionParameterTypeList();
  params->append_argument(SageBuilder::buildIntType());
  params->append_argument(SageBuilder::buildIntType());
  SgFunctionType *ft =
      SageBuilder::buildFunctionType(SageBuilder::buildVoidType(), params);
  EXPECT_EQ(ftt->size(), 1);
  SgFunctionParameterTypeList *params2 = new SgFunctionParameterTypeList();
  params2->append_argument(SageBuilder::buildIntType());
  SgFunctionType *ft2 =
      SageBuilder::buildFunctionType(SageBuilder::buildVoidType(), params2);
  EXPECT_EQ(ftt->size(), 2);
}

TEST(SymbolTableTest, DefaultConstructorWorks) {
  SgSymbolTable *p = new SgSymbolTable();
  ASSERT_EQ(isNull(p), false);
  EXPECT_EQ(is<SgSymbolTable>(p), true);
}

TEST(SymbolTableTest, DefaultConstructorLeadsToEmptySymbolSet) {
  SgSymbolTable *p = new SgSymbolTable();
  ASSERT_EQ(isNull(p), false);
  EXPECT_EQ(p->size(), 0);
}

TEST(SymbolTableTest, LookUpOfNameInEmptyTableReturnsFalseNullOrZero) {
  SgSymbolTable *p = new SgSymbolTable();
  const SgName foo("foo");
  EXPECT_EQ(p->exists(foo), false);
  EXPECT_EQ(isNull(p->find_variable(foo)), true);
  EXPECT_EQ(isNull(p->find_class(foo)), true);
  EXPECT_EQ(isNull(p->find_function(foo)), true);
  EXPECT_EQ(isNull(p->find_function_type(foo)), true);
  EXPECT_EQ(isNull(p->find_typedef(foo)), true);
  EXPECT_EQ(isNull(p->find_enum(foo)), true);
  EXPECT_EQ(isNull(p->find_enum_field(foo)), true);
  EXPECT_EQ(isNull(p->find_label(foo)), true);
  EXPECT_EQ(isNull(p->find_namespace(foo)), true);
  EXPECT_EQ(p->count(foo), 0);
  EXPECT_EQ(p->count_aliases(foo), 0);
  EXPECT_EQ(p->get_symbols().size(), 0);
}

TEST(SymbolTableTest, InsertNameWithNullSymbolAborts) {
  SgSymbolTable *p = new SgSymbolTable();
  const SgName foo("foo");
  EXPECT_EQ(p->size(), 0);
  EXPECT_DEATH(
      p->insert(foo, NULL),
      "REX_AST_INVARIANT\\[symbol-table-insert\\]: symbol table cannot insert "
      "null symbol for name foo");
}

TEST(SymbolTableTest, InsertSymbolWithoutDeclarationAborts) {
  SgSymbolTable table;
  SgVariableSymbol symbol(nullptr);
  EXPECT_DEATH(table.insert("foo", &symbol),
               "get_declaration\\(\\) != nullptr");
}

TEST(SymbolTableTest, MissingDeclarationCannotSupplyName) {
  SgVariableSymbol symbol(nullptr);
  EXPECT_DEATH(symbol.get_name(), "get_declaration\\(\\) != nullptr");
}

TEST(SourcePrototypeQuery, RejectsCyclicWrapperChain) {
  SgTypeInt integer;
  SgPointerType first(&integer);
  SgPointerType second(&first);
  EXPECT_FALSE(first.has_source_prototype());
  EXPECT_FALSE(second.has_source_prototype());
  first.set_base_type(&second);
  EXPECT_DEATH(first.has_source_prototype(), "has_source_prototype");
}

TEST(FileInfo, RejectsUnregisteredLogicalIdAtMutation) {
  Sg_File_Info *position = new Sg_File_Info("rex_file_info_owner.cpp", 1, 2);
  EXPECT_DEATH(position->set_file_id(std::numeric_limits<int>::max()),
               "file-info-registration");
  EXPECT_DEATH(position->set_file_id(Sg_File_Info::BAD_FILE_ID - 1),
               "BAD_FILE_ID");
  delete position;
}

TEST(FileInfo, RejectsUnregisteredPhysicalIdAtMutation) {
  Sg_File_Info *position = new Sg_File_Info("rex_file_info_owner.cpp", 1, 2);
  EXPECT_DEATH(position->set_physical_file_id(std::numeric_limits<int>::max()),
               "");
  EXPECT_DEATH(position->set_physical_file_id(Sg_File_Info::BAD_FILE_ID - 1),
               "BAD_FILE_ID");
  for (int id :
       {Sg_File_Info::COPY_FILE_ID, Sg_File_Info::TRANSFORMATION_FILE_ID,
        Sg_File_Info::COMPILER_GENERATED_FILE_ID,
        Sg_File_Info::COMPILER_GENERATED_MARKED_FOR_OUTPUT_FILE_ID,
        Sg_File_Info::BAD_FILE_ID}) {
    EXPECT_DEATH(position->set_physical_file_id(id),
                 "file-info-physical-owner");
  }
  for (int id : {Sg_File_Info::COPY_FILE_ID, Sg_File_Info::BAD_FILE_ID}) {
    EXPECT_DEATH((void)new Sg_File_Info(id, 1, 2), "file-info-physical-owner");
  }
  delete position;
}

TEST(FileInfo, RejectsCopyOfUnclassifiedPhysicalOwner) {
  Sg_File_Info *source = new Sg_File_Info("rex_file_info_owner.cpp", 1, 2);
  Sg_File_Info *destination = new Sg_File_Info(*source);
  source->setCompilerGenerated();
  source->set_physical_file_id(Sg_File_Info::COMPILER_GENERATED_FILE_ID);
  source->unsetCompilerGenerated();
  EXPECT_DEATH((void)new Sg_File_Info(*source), "file-info-physical-owner");
  EXPECT_DEATH(*destination = *source, "file-info-physical-owner");
  source->setCompilerGenerated();
  delete destination;
  delete source;
}

TEST(FileInfo, RegisteredLogicalAndPhysicalOwnersRemainIndependent) {
  Sg_File_Info *position = new Sg_File_Info("rex_file_info_owner.cpp", 1, 2);
  const int physical = position->get_physical_file_id();
  const int logical =
      Sg_File_Info::addFilenameToMap("rex_file_info_logical.cpp");
  position->set_file_id(logical);
  EXPECT_EQ(position->get_file_id(), logical);
  EXPECT_EQ(position->get_physical_file_id(), physical);
  position->set_physical_file_id(logical);
  EXPECT_EQ(position->get_physical_file_id(), logical);
  position->setCompilerGenerated();
  EXPECT_EQ(position->get_file_id(), Sg_File_Info::COMPILER_GENERATED_FILE_ID);
  EXPECT_EQ(position->get_physical_file_id(), logical);
  position->setFrontendSpecific();
  EXPECT_EQ(position->get_file_id(), logical);
  position->setTransformation();
  EXPECT_EQ(position->get_file_id(), Sg_File_Info::TRANSFORMATION_FILE_ID);
  EXPECT_EQ(position->get_physical_file_id(), logical);
  position->set_physical_file_id(Sg_File_Info::COMPILER_GENERATED_FILE_ID);
  EXPECT_EQ(position->get_physical_file_id(),
            Sg_File_Info::COMPILER_GENERATED_FILE_ID);
  position->set_physical_file_id(Sg_File_Info::TRANSFORMATION_FILE_ID);
  EXPECT_EQ(position->get_physical_file_id(),
            Sg_File_Info::TRANSFORMATION_FILE_ID);
  delete position;
}
