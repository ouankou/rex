// Tests src/midend/astQuery/{nodeQuery,nameQuery,numberQuery}

#include "rose.h"

#include "nameQuery.h"

#include "nodeQuery.h"

#include "numberQuery.h"

using namespace std;
using namespace Rose;

/** Show an error message about a node. */
static void emit_node_mesg(SgNode *node, const std::string &mesg = "") {
  Sg_File_Info *loc = isSgLocatedNode(node)
                          ? isSgLocatedNode(node)->get_startOfConstruct()
                          : NULL;
  std::string filename = loc ? loc->get_filenameString() : "__UNKNOWN_FILE__";
  int lno = loc ? loc->get_line() : 0;
  int cno = loc ? loc->get_col() : 0;
  std::cerr << filename << ":" << lno << "." << cno << ": "
            << (mesg.empty() ? "error" : mesg) << "\n";
}

/** Checks that all nodes in the list are unique. Emit errors about duplicate
 * items. Returns the number of duplicates. */
static size_t check_unique(const NodeQuerySynthesizedAttributeType &nodes,
                           const std::string &title) {
  std::set<SgNode *> set;
  std::vector<SgNode *> dups;
  for (NodeQuerySynthesizedAttributeType::const_iterator ni = nodes.begin();
       ni != nodes.end(); ++ni) {
    if (!set.insert(*ni).second)
      dups.push_back(*ni);
  }
  if (!dups.empty()) {
    std::cerr << "Duplicate nodes returned for the \"" << title << "\" test:\n";
    for (std::vector<SgNode *>::const_iterator di = dups.begin();
         di != dups.end(); ++di)
      emit_node_mesg(*di, "appears multiple times in list");
  }
  return dups.size();
}

int main(int argc, char *argv[]) {
  SgProject *project = frontend(argc, argv);
  AstTests::runAllTests(project); // run internal consistency tests on the AST

  size_t nerrors = 0;
  for (const char *text : {"rex_query", "prefix rex_query = rex_value"}) {
    auto *pragma = SageBuilder::buildPragmaDeclaration(text);
    SgName match("rex_query");
    ROSE_ASSERT(NodeQuery::queryNodePragmaDeclarationFromName(pragma, &match) ==
                NodeQuerySynthesizedAttributeType{pragma});
    for (const char *name : {"rex_missing", "rex_value"}) {
      SgName nonmatch(name);
      ROSE_ASSERT(
          NodeQuery::queryNodePragmaDeclarationFromName(pragma, &nonmatch)
              .empty());
    }
    SageInterface::deleteAST(pragma);
  }
  const std::vector<SgNode *> queryInputs = {project, project};
  const auto typedResults = AstQueryNamespace::queryRange(
      queryInputs.begin(), queryInputs.end(),
      [](SgNode *node) { return std::vector<SgProject *>{isSgProject(node)}; });
  ROSE_ASSERT(typedResults == std::vector<SgProject *>({project, project}));
  size_t visited = 0;
  void *sideEffectResult = AstQueryNamespace::queryRange(
      queryInputs.begin(), queryInputs.end(), [&](SgNode *) -> void * {
        ++visited;
        return nullptr;
      });
  ROSE_ASSERT(visited == queryInputs.size() && sideEffectResult == nullptr);
  const NameQuerySynthesizedAttributeType structFields = {"rex_struct_first",
                                                          "rex_struct_second"};
  const NameQuerySynthesizedAttributeType unionFields = {"rex_union_first",
                                                         "rex_union_second"};
  ROSE_ASSERT(NameQuery::querySubTree(project, NameQuery::StructFieldNames) ==
              structFields);
  ROSE_ASSERT(NameQuery::querySubTree(project, NameQuery::UnionFieldNames) ==
              unionFields);
  const auto structDeclarations =
      NodeQuery::querySubTree(project, NodeQuery::StructFields);
  const auto unionDeclarations =
      NodeQuery::querySubTree(project, NodeQuery::UnionFields);
  ROSE_ASSERT(structDeclarations.size() == 2 && unionDeclarations.size() == 2);
  ROSE_ASSERT(
      NameQuery::queryNodeList(structDeclarations, NameQuery::VariableNames) ==
      structFields);
  ROSE_ASSERT(NameQuery::queryNodeList(
                  unionDeclarations, NameQuery::VariableNames) == unionFields);
  std::string separator = std::string(80, '-') + "\n";

  std::cerr << separator
            << "Testing NodeQuery::querySubTree for all SgFunctionDeclaration "
               "nodes\n";
  NodeQuerySynthesizedAttributeType funcDecls =
      NodeQuery::querySubTree(project, V_SgFunctionDeclaration);
  std::cerr << "found " << funcDecls.size() << " function declaration nodes\n";
  nerrors += check_unique(funcDecls, "querySubTree SgFunctionDeclaration");
  for (NodeQuerySynthesizedAttributeType::const_iterator ni = funcDecls.begin();
       ni != funcDecls.end(); ++ni) {
    if (!isSgFunctionDeclaration(*ni)) {
      emit_node_mesg(*ni, "not a function declaration");
      ++nerrors;
    }
  }
  ROSE_ASSERT(0 == nerrors); // optional, to exit early

  bool sawConstructorArguments = false;
  for (SgNode *node :
       NodeQuery::querySubTree(project, V_SgMemberFunctionDeclaration)) {
    auto *declaration = isSgMemberFunctionDeclaration(node);
    if (declaration->get_name().getString() != "Bar" ||
        declaration->get_args().size() != 2)
      continue;
    const auto arguments = NodeQuery::querySolverArguments(node);
    ROSE_ASSERT(arguments.size() == 2);
    ROSE_ASSERT(arguments[0] == declaration->get_args()[0] &&
                arguments[1] == declaration->get_args()[1]);
    ROSE_ASSERT(NodeQuery::querySolverVariableDeclarations(node) == arguments);
    sawConstructorArguments = true;
  }
  ROSE_ASSERT(sawConstructorArguments);

  std::cerr << separator
            << "Testing NodeQuery::queryNodeList for all SgFunctionDeclaration "
               "nodes\n";
  NodeQuerySynthesizedAttributeType funcDecls2 =
      NodeQuery::queryNodeList(funcDecls, V_SgFunctionDeclaration);
  std::cerr << "found " << funcDecls2.size() << " function declaration nodes\n";
  nerrors += check_unique(funcDecls2, "queryNodeList SgFunctionDeclaration");
  for (NodeQuerySynthesizedAttributeType::const_iterator ni =
           funcDecls2.begin();
       ni != funcDecls2.end(); ++ni) {
    if (!isSgFunctionDeclaration(*ni)) {
      emit_node_mesg(*ni, "not a function declaration");
      ++nerrors;
    }
  }
  ROSE_ASSERT(0 == nerrors); // optional, to exit early

  std::cerr << separator
            << "Testing NameQuery::querySubTree for FunctionDeclarationNames\n";
  NameQuerySynthesizedAttributeType funcNames =
      NameQuery::querySubTree(project, NameQuery::FunctionDeclarationNames);
  std::cerr << "found " << funcNames.size() << " function declaration names\n";
  if (funcNames.size() != funcDecls.size()) {
    std::cerr << "number of function declaration names (" << funcNames.size()
              << ")"
              << " does not match number of function declaration nodes ("
              << funcDecls.size() << ")\n";
    ++nerrors;
  }
  ROSE_ASSERT(0 == nerrors); // optional, to exit early

  std::cerr
      << separator
      << "Testing NameQuery::queryNodeList for FunctionDeclarationNames\n";
  NameQuerySynthesizedAttributeType funcNames2 =
      NameQuery::queryNodeList(funcDecls, NameQuery::FunctionDeclarationNames);
  std::cerr << "found " << funcNames2.size() << " function declaration names\n";
  if (funcNames2.size() != funcDecls.size()) {
    std::cerr << "number of function declaration names (" << funcNames2.size()
              << ")"
              << " does not match number of function declaration nodes ("
              << funcDecls.size() << ")\n";
    ++nerrors;
  }
  ROSE_ASSERT(0 == nerrors); // optional, to exit early

  std::cerr
      << separator
      << "Testing NumberQuery::querySubTree for NumberOfArgsInConstructor\n";
  NumberQuerySynthesizedAttributeType ctorArgCounts = NumberQuery::querySubTree(
      project, NumberQuery::NumberOfArgsInConstructor);
  std::cerr << "found " << ctorArgCounts.size() << " results\n";
  if (ctorArgCounts.size() != 1) {
    std::cerr << "should have found only one result\n";
    ++nerrors;
  }
  ROSE_ASSERT(0 == nerrors); // optional, to exit early

  for (SgSizeOfOp *sizeOf :
       {SageBuilder::buildSizeOfOp(SageBuilder::buildIntVal(1),
                                   SageBuilder::buildUnsignedLongType()),
        SageBuilder::buildSizeOfOp(SageBuilder::buildIntType(),
                                   SageBuilder::buildUnsignedLongType())}) {
    const auto operands = NumberQuery::queryNumberOfOperands(sizeOf);
    ROSE_ASSERT(operands.size() == 1 && operands.front() == 1);
    SageInterface::deleteAST(sizeOf);
  }

  SgBasicBlock *callScope = SageBuilder::buildBasicBlock();
  const auto definitions =
      NodeQuery::querySubTree(project, V_SgFunctionDefinition);
  ROSE_ASSERT(!definitions.empty());
  SgFunctionDefinition *definition =
      isSgFunctionDefinition(definitions.front());
  ROSE_ASSERT(definition != nullptr && definition->get_body() != nullptr);
  SageInterface::appendStatement(callScope, definition->get_body());
  SgType *intType = SageBuilder::buildIntType();
  SgFunctionType *functionType = SageBuilder::buildFunctionType(
      intType, SageBuilder::buildFunctionParameterTypeList(intType, intType));
  SgVariableDeclaration *functionPointer =
      SageBuilder::buildVariableDeclaration(
          "rex_query_function_pointer",
          SageBuilder::buildPointerType(functionType), nullptr, callScope);
  SageInterface::appendStatement(functionPointer, callScope);
  SgFunctionCallExp *call = SageBuilder::buildFunctionCallExp(
      SageBuilder::buildVarRefExp(functionPointer), intType,
      SageBuilder::buildExprListExp(SageBuilder::buildIntVal(1),
                                    SageBuilder::buildIntVal(2)));
  const auto arguments =
      NumberQuery::queryNumberOfArgsInParenthesisOperator(call, intType);
  ROSE_ASSERT(NameQuery::queryNameFunctionReferenceNames(call).empty());
  ROSE_ASSERT(arguments.size() == 1 && arguments.front() == 2);
  ROSE_ASSERT(NumberQuery::queryNumberOfArgsInParenthesisOperator(
                  call, SageBuilder::buildDoubleType())
                  .empty());
  ROSE_ASSERT(NumberQuery::queryNumberOfArgsInScalarIndexingOperator(call) ==
              arguments);
  ROSE_ASSERT(NumberQuery::getFunction(
                  NumberQuery::NumberOfArgsInScalarIndexingOperator)(call) ==
              arguments);
  ROSE_ASSERT(NumberQuery::queryNumberOfArgsInParenthesisOperator(
                  call->get_args(), intType) == arguments);
  SageInterface::deleteAST(call);
  SageInterface::removeStatement(callScope);
  SageInterface::deleteAST(callScope);

  bool checkedDirectCall = false;
  for (SgNode *node : funcDecls) {
    auto *declaration = isSgFunctionDeclaration(node);
    if (isSgMemberFunctionDeclaration(declaration) ||
        !declaration->get_args().empty()) {
      continue;
    }
    auto *directCall = SageBuilder::buildFunctionCallExp(
        SageBuilder::buildFunctionRefExp(declaration),
        declaration->get_type()->get_return_type(),
        SageBuilder::buildExprListExp());
    const auto names = NameQuery::queryNameFunctionReferenceNames(directCall);
    ROSE_ASSERT(names.size() == 1 &&
                names.front() == declaration->get_name().getString());
    SageInterface::deleteAST(directCall);
    checkedDirectCall = true;
    break;
  }
  ROSE_ASSERT(checkedDirectCall);

  // It is not necessary to call backend for this test; that functionality is
  // tested elsewhere.
  return nerrors ? 1 : 0;
}
