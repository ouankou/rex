#include "SgGraphTemplate.h"
#include "graphProcessing.h"
#include "staticCFG.h"
using namespace std;
using namespace Rose;
typedef myGraph CFGforT;
class visitorTraversal : public SgGraphTraversal<CFGforT> {
public:
  vector<string> sss;
  set<vector<string>> sssv;
  void analyzePath(std::vector<VertexID> &pth);
  SgIncidenceDirectedGraph *g;
  myGraph *orig;
  StaticCFG::CFG *cfg;
  std::vector<std::vector<string>> paths;
};
void visitorTraversal::analyzePath(std::vector<VertexID> &pathR) {
  std::vector<string> path;
  for (unsigned int j = 0; j < pathR.size(); j++) {
    SgGraphNode *R = orig->getGraphNode()[pathR[j]];
    CFGNode cf = cfg->toCFGNode(R);
    string str = cf.toString();
    str.erase(std::remove(str.begin(), str.end(), '\n'), str.end());
    path.push_back(str);
  }
  paths.push_back(path);
  // ROSE_ASSERT(sssv.find(path) != sssv.end());
}
int main(int argc, char *argv[]) {
  SgProject *proj = frontend(argc, argv);
  ROSE_ASSERT(proj != NULL);
  SgFunctionDeclaration *mainDefDecl = SageInterface::findMain(proj);
  SgFunctionDefinition *mainDef = mainDefDecl->get_definition();
  visitorTraversal *vis = new visitorTraversal();
  StaticCFG::CFG cfg(mainDef);
  stringstream ss;
  string fileName = StringUtility::stripPathFromFileName(
      mainDef->get_file_info()->get_filenameString());
  string dotFileName1 =
      fileName + "." + mainDef->get_declaration()->get_name() + ".dot";
  cfgToDot(mainDef, dotFileName1);
  SgIncidenceDirectedGraph *g = cfg.getGraph();
  auto mg = instantiateGraph(g, cfg);
  vis->orig = mg.get();
  vis->cfg = &cfg;
  std::set<std::vector<string>> sssv;
  std::vector<string> sss;
  sss.push_back("Start(::main)0x709f7c633010<SgFunctionDefinition> @line=1, "
                "col=1 :idx=0");
  sss.push_back("main_parameter_list_0x709f7ca34010<SgFunctionParameterList> "
                "@line=1, col=9 :idx=0");
  sss.push_back("After parameters(::main)0x709f7c633010<SgFunctionDefinition> "
                "@line=1, col=1 :idx=1");
  sss.push_back(
      "After pre-initialization(::main)0x709f7c633010<SgFunctionDefinition> "
      "@line=1, col=1 :idx=2");
  sss.push_back(
      "0x709f7c6cc1680x709f7c6cc168<SgBasicBlock> @line=1, col=12 :idx=0");
  sss.push_back("_variable_declaration_m0x709f7cf3f710<SgVariableDeclaration> "
                "@line=2, col=5 :idx=0");
  sss.push_back("initialized_name_m0x709f7ce4bf90<SgInitializedName> m :idx=0");
  sss.push_back("_variable_declaration_m0x709f7cf3f710<SgVariableDeclaration> "
                "@line=2, col=5 :idx=1");
  sss.push_back(
      "0x709f7c6cc1680x709f7c6cc168<SgBasicBlock> @line=1, col=12 :idx=1");
  sss.push_back(
      "0x709f7c5960100x709f7c596010<SgForStatement> @line=3, col=5 :idx=0");
  sss.push_back("SgForInitStatement0x709f7c55f010<SgForInitStatement> @line=3, "
                "col=10 :idx=0");
  sss.push_back("_variable_declaration_i0x709f7cf3f9f0<SgVariableDeclaration> "
                "@line=3, col=10 :idx=0");
  sss.push_back("initialized_name_i0x709f7ce4c180<SgInitializedName> i :idx=0");
  sss.push_back("SgAssignInitializer_undef_name0x709f7c52c010<"
                "SgAssignInitializer> @line=3, col=18 :idx=0");
  sss.push_back(
      "integer_value_exp_00x709f7cbf7090<SgIntVal> @line=3, col=18 :idx=0");
  sss.push_back(
      "integer_value_exp_00x709f7cbf7090<SgIntVal> @line=3, col=18 :idx=1");
  sss.push_back("SgAssignInitializer_undef_name0x709f7c52c010<"
                "SgAssignInitializer> @line=3, col=18 :idx=1");
  sss.push_back("initialized_name_i0x709f7ce4c180<SgInitializedName> i :idx=1");
  sss.push_back("_variable_declaration_i0x709f7cf3f9f0<SgVariableDeclaration> "
                "@line=3, col=10 :idx=1");
  sss.push_back("SgForInitStatement0x709f7c55f010<SgForInitStatement> @line=3, "
                "col=10 :idx=1");
  sss.push_back(
      "0x709f7c5960100x709f7c596010<SgForStatement> @line=3, col=5 :idx=1");
  sss.push_back(
      "SgExprStatement0x709f7c483010<SgExprStatement> @line=3, col=21 :idx=0");
  sss.push_back("SgLessThanOp_undef_name0x709f7c4b2080<SgLessThanOp> @line=3, "
                "col=21 :idx=0");
  sss.push_back(
      "var_ref_of_i0x709f7c4e9098<SgVarRefExp> @line=3, col=21 :idx=0");
  sss.push_back("SgLessThanOp_undef_name0x709f7c4b2080<SgLessThanOp> @line=3, "
                "col=21 :idx=1");
  sss.push_back(
      "integer_value_exp_50x709f7cbf7190<SgIntVal> @line=3, col=25 :idx=0");
  sss.push_back(
      "integer_value_exp_50x709f7cbf7190<SgIntVal> @line=3, col=25 :idx=1");
  sss.push_back("SgLessThanOp_undef_name0x709f7c4b2080<SgLessThanOp> @line=3, "
                "col=21 :idx=2");
  sss.push_back(
      "SgExprStatement0x709f7c483010<SgExprStatement> @line=3, col=21 :idx=1");
  sss.push_back(
      "0x709f7c5960100x709f7c596010<SgForStatement> @line=3, col=5 :idx=2");
  sss.push_back(
      "0x709f7c5960100x709f7c596010<SgForStatement> @line=3, col=5 :idx=4");
  sss.push_back(
      "0x709f7c6cc1680x709f7c6cc168<SgBasicBlock> @line=1, col=12 :idx=2");
  sss.push_back(
      "SgReturnStmt0x709f7c421010<SgReturnStmt> @line=16, col=5 :idx=0");
  sss.push_back(
      "integer_value_exp_00x709f7cbf7690<SgIntVal> @line=16, col=12 :idx=0");
  sss.push_back(
      "integer_value_exp_00x709f7cbf7690<SgIntVal> @line=16, col=12 :idx=1");
  sss.push_back(
      "SgReturnStmt0x709f7c421010<SgReturnStmt> @line=16, col=5 :idx=1");
  sss.push_back(
      "End(::main)0x709f7c633010<SgFunctionDefinition> @line=1, col=1 :idx=3");
  sssv.insert(sss);
  sss.clear();
  sss.push_back("Start(::main)0x709f7c633010<SgFunctionDefinition> @line=1, "
                "col=1 :idx=0");
  sss.push_back("main_parameter_list_0x709f7ca34010<SgFunctionParameterList> "
                "@line=1, col=9 :idx=0");
  sss.push_back("After parameters(::main)0x709f7c633010<SgFunctionDefinition> "
                "@line=1, col=1 :idx=1");
  sss.push_back(
      "After pre-initialization(::main)0x709f7c633010<SgFunctionDefinition> "
      "@line=1, col=1 :idx=2");
  sss.push_back(
      "0x709f7c6cc1680x709f7c6cc168<SgBasicBlock> @line=1, col=12 :idx=0");
  sss.push_back("_variable_declaration_m0x709f7cf3f710<SgVariableDeclaration> "
                "@line=2, col=5 :idx=0");
  sss.push_back("initialized_name_m0x709f7ce4bf90<SgInitializedName> m :idx=0");
  sss.push_back("_variable_declaration_m0x709f7cf3f710<SgVariableDeclaration> "
                "@line=2, col=5 :idx=1");
  sss.push_back(
      "0x709f7c6cc1680x709f7c6cc168<SgBasicBlock> @line=1, col=12 :idx=1");
  sss.push_back(
      "0x709f7c5960100x709f7c596010<SgForStatement> @line=3, col=5 :idx=0");
  sss.push_back("SgForInitStatement0x709f7c55f010<SgForInitStatement> @line=3, "
                "col=10 :idx=0");
  sss.push_back("_variable_declaration_i0x709f7cf3f9f0<SgVariableDeclaration> "
                "@line=3, col=10 :idx=0");
  sss.push_back("initialized_name_i0x709f7ce4c180<SgInitializedName> i :idx=0");
  sss.push_back("SgAssignInitializer_undef_name0x709f7c52c010<"
                "SgAssignInitializer> @line=3, col=18 :idx=0");
  sss.push_back(
      "integer_value_exp_00x709f7cbf7090<SgIntVal> @line=3, col=18 :idx=0");
  sss.push_back(
      "integer_value_exp_00x709f7cbf7090<SgIntVal> @line=3, col=18 :idx=1");
  sss.push_back("SgAssignInitializer_undef_name0x709f7c52c010<"
                "SgAssignInitializer> @line=3, col=18 :idx=1");
  sss.push_back("initialized_name_i0x709f7ce4c180<SgInitializedName> i :idx=1");
  sss.push_back("_variable_declaration_i0x709f7cf3f9f0<SgVariableDeclaration> "
                "@line=3, col=10 :idx=1");
  sss.push_back("SgForInitStatement0x709f7c55f010<SgForInitStatement> @line=3, "
                "col=10 :idx=1");
  sss.push_back(
      "0x709f7c5960100x709f7c596010<SgForStatement> @line=3, col=5 :idx=1");
  sss.push_back(
      "SgExprStatement0x709f7c483010<SgExprStatement> @line=3, col=21 :idx=0");
  sss.push_back("SgLessThanOp_undef_name0x709f7c4b2080<SgLessThanOp> @line=3, "
                "col=21 :idx=0");
  sss.push_back(
      "var_ref_of_i0x709f7c4e9098<SgVarRefExp> @line=3, col=21 :idx=0");
  sss.push_back("SgLessThanOp_undef_name0x709f7c4b2080<SgLessThanOp> @line=3, "
                "col=21 :idx=1");
  sss.push_back(
      "integer_value_exp_50x709f7cbf7190<SgIntVal> @line=3, col=25 :idx=0");
  sss.push_back(
      "integer_value_exp_50x709f7cbf7190<SgIntVal> @line=3, col=25 :idx=1");
  sss.push_back("SgLessThanOp_undef_name0x709f7c4b2080<SgLessThanOp> @line=3, "
                "col=21 :idx=2");
  sss.push_back(
      "SgExprStatement0x709f7c483010<SgExprStatement> @line=3, col=21 :idx=1");
  sss.push_back(
      "0x709f7c5960100x709f7c596010<SgForStatement> @line=3, col=5 :idx=2");
  sss.push_back(
      "0x709f7c6cc2c00x709f7c6cc2c0<SgBasicBlock> @line=3, col=33 :idx=0");
  sss.push_back(
      "0x709f7c5961500x709f7c596150<SgForStatement> @line=4, col=9 :idx=0");
  sss.push_back("SgForInitStatement0x709f7c55f080<SgForInitStatement> @line=4, "
                "col=14 :idx=0");
  sss.push_back("_variable_declaration_j0x709f7cf3fcd0<SgVariableDeclaration> "
                "@line=4, col=14 :idx=0");
  sss.push_back("initialized_name_j0x709f7ce4c370<SgInitializedName> j :idx=0");
  sss.push_back("SgAssignInitializer_undef_name0x709f7c52c078<"
                "SgAssignInitializer> @line=4, col=22 :idx=0");
  sss.push_back(
      "integer_value_exp_00x709f7cbf7210<SgIntVal> @line=4, col=22 :idx=0");
  sss.push_back(
      "integer_value_exp_00x709f7cbf7210<SgIntVal> @line=4, col=22 :idx=1");
  sss.push_back("SgAssignInitializer_undef_name0x709f7c52c078<"
                "SgAssignInitializer> @line=4, col=22 :idx=1");
  sss.push_back("initialized_name_j0x709f7ce4c370<SgInitializedName> j :idx=1");
  sss.push_back("_variable_declaration_j0x709f7cf3fcd0<SgVariableDeclaration> "
                "@line=4, col=14 :idx=1");
  sss.push_back("SgForInitStatement0x709f7c55f080<SgForInitStatement> @line=4, "
                "col=14 :idx=1");
  sss.push_back(
      "0x709f7c5961500x709f7c596150<SgForStatement> @line=4, col=9 :idx=1");
  sss.push_back(
      "SgExprStatement0x709f7c483070<SgExprStatement> @line=4, col=25 :idx=0");
  sss.push_back("SgLessThanOp_undef_name0x709f7c4b2160<SgLessThanOp> @line=4, "
                "col=25 :idx=0");
  sss.push_back(
      "var_ref_of_j0x709f7c4e9230<SgVarRefExp> @line=4, col=25 :idx=0");
  sss.push_back("SgLessThanOp_undef_name0x709f7c4b2160<SgLessThanOp> @line=4, "
                "col=25 :idx=1");
  sss.push_back(
      "integer_value_exp_50x709f7cbf7310<SgIntVal> @line=4, col=29 :idx=0");
  sss.push_back(
      "integer_value_exp_50x709f7cbf7310<SgIntVal> @line=4, col=29 :idx=1");
  sss.push_back("SgLessThanOp_undef_name0x709f7c4b2160<SgLessThanOp> @line=4, "
                "col=25 :idx=2");
  sss.push_back(
      "SgExprStatement0x709f7c483070<SgExprStatement> @line=4, col=25 :idx=1");
  sss.push_back(
      "0x709f7c5961500x709f7c596150<SgForStatement> @line=4, col=9 :idx=2");
  sss.push_back(
      "0x709f7c6cc4180x709f7c6cc418<SgBasicBlock> @line=4, col=37 :idx=0");
  sss.push_back(
      "0x709f7c5962900x709f7c596290<SgForStatement> @line=5, col=13 :idx=0");
  sss.push_back("SgForInitStatement0x709f7c55f0f0<SgForInitStatement> @line=5, "
                "col=18 :idx=0");
  sss.push_back("_variable_declaration_k0x709f7cf3ffb0<SgVariableDeclaration> "
                "@line=5, col=18 :idx=0");
  sss.push_back("initialized_name_k0x709f7ce4c560<SgInitializedName> k :idx=0");
  sss.push_back("SgAssignInitializer_undef_name0x709f7c52c0e0<"
                "SgAssignInitializer> @line=5, col=26 :idx=0");
  sss.push_back(
      "integer_value_exp_00x709f7cbf7390<SgIntVal> @line=5, col=26 :idx=0");
  sss.push_back(
      "integer_value_exp_00x709f7cbf7390<SgIntVal> @line=5, col=26 :idx=1");
  sss.push_back("SgAssignInitializer_undef_name0x709f7c52c0e0<"
                "SgAssignInitializer> @line=5, col=26 :idx=1");
  sss.push_back("initialized_name_k0x709f7ce4c560<SgInitializedName> k :idx=1");
  sss.push_back("_variable_declaration_k0x709f7cf3ffb0<SgVariableDeclaration> "
                "@line=5, col=18 :idx=1");
  sss.push_back("SgForInitStatement0x709f7c55f0f0<SgForInitStatement> @line=5, "
                "col=18 :idx=1");
  sss.push_back(
      "0x709f7c5962900x709f7c596290<SgForStatement> @line=5, col=13 :idx=1");
  sss.push_back(
      "SgExprStatement0x709f7c4830d0<SgExprStatement> @line=5, col=29 :idx=0");
  sss.push_back("SgLessThanOp_undef_name0x709f7c4b2240<SgLessThanOp> @line=5, "
                "col=29 :idx=0");
  sss.push_back(
      "var_ref_of_k0x709f7c4e93c8<SgVarRefExp> @line=5, col=29 :idx=0");
  sss.push_back("SgLessThanOp_undef_name0x709f7c4b2240<SgLessThanOp> @line=5, "
                "col=29 :idx=1");
  sss.push_back(
      "integer_value_exp_50x709f7cbf7490<SgIntVal> @line=5, col=33 :idx=0");
  sss.push_back(
      "integer_value_exp_50x709f7cbf7490<SgIntVal> @line=5, col=33 :idx=1");
  sss.push_back("SgLessThanOp_undef_name0x709f7c4b2240<SgLessThanOp> @line=5, "
                "col=29 :idx=2");
  sss.push_back(
      "SgExprStatement0x709f7c4830d0<SgExprStatement> @line=5, col=29 :idx=1");
  sss.push_back(
      "0x709f7c5962900x709f7c596290<SgForStatement> @line=5, col=13 :idx=2");
  sss.push_back(
      "0x709f7c6cc5700x709f7c6cc570<SgBasicBlock> @line=5, col=41 :idx=0");
  sss.push_back(
      "0x709f7c5963d00x709f7c5963d0<SgForStatement> @line=6, col=17 :idx=0");
  sss.push_back("SgForInitStatement0x709f7c55f160<SgForInitStatement> @line=6, "
                "col=22 :idx=0");
  sss.push_back("_variable_declaration_l0x709f7cf40290<SgVariableDeclaration> "
                "@line=6, col=22 :idx=0");
  sss.push_back("initialized_name_l0x709f7ce4c750<SgInitializedName> l :idx=0");
  sss.push_back("SgAssignInitializer_undef_name0x709f7c52c148<"
                "SgAssignInitializer> @line=6, col=30 :idx=0");
  sss.push_back(
      "integer_value_exp_00x709f7cbf7510<SgIntVal> @line=6, col=30 :idx=0");
  sss.push_back(
      "integer_value_exp_00x709f7cbf7510<SgIntVal> @line=6, col=30 :idx=1");
  sss.push_back("SgAssignInitializer_undef_name0x709f7c52c148<"
                "SgAssignInitializer> @line=6, col=30 :idx=1");
  sss.push_back("initialized_name_l0x709f7ce4c750<SgInitializedName> l :idx=1");
  sss.push_back("_variable_declaration_l0x709f7cf40290<SgVariableDeclaration> "
                "@line=6, col=22 :idx=1");
  sss.push_back("SgForInitStatement0x709f7c55f160<SgForInitStatement> @line=6, "
                "col=22 :idx=1");
  sss.push_back(
      "0x709f7c5963d00x709f7c5963d0<SgForStatement> @line=6, col=17 :idx=1");
  sss.push_back(
      "SgExprStatement0x709f7c483130<SgExprStatement> @line=6, col=33 :idx=0");
  sss.push_back("SgLessThanOp_undef_name0x709f7c4b2320<SgLessThanOp> @line=6, "
                "col=33 :idx=0");
  sss.push_back(
      "var_ref_of_l0x709f7c4e9560<SgVarRefExp> @line=6, col=33 :idx=0");
  sss.push_back("SgLessThanOp_undef_name0x709f7c4b2320<SgLessThanOp> @line=6, "
                "col=33 :idx=1");
  sss.push_back(
      "integer_value_exp_50x709f7cbf7610<SgIntVal> @line=6, col=37 :idx=0");
  sss.push_back(
      "integer_value_exp_50x709f7cbf7610<SgIntVal> @line=6, col=37 :idx=1");
  sss.push_back("SgLessThanOp_undef_name0x709f7c4b2320<SgLessThanOp> @line=6, "
                "col=33 :idx=2");
  sss.push_back(
      "SgExprStatement0x709f7c483130<SgExprStatement> @line=6, col=33 :idx=1");
  sss.push_back(
      "0x709f7c5963d00x709f7c5963d0<SgForStatement> @line=6, col=17 :idx=2");
  sss.push_back(
      "0x709f7c6cc6c80x709f7c6cc6c8<SgBasicBlock> @line=6, col=45 :idx=0");
  sss.push_back(
      "SgExprStatement0x709f7c483190<SgExprStatement> @line=9, col=29 :idx=0");
  sss.push_back("SgPlusPlusOp_undef_name0x709f7c4501b0<SgPlusPlusOp> @line=9, "
                "col=29 :idx=0");
  sss.push_back(
      "var_ref_of_m0x709f7c4e9670<SgVarRefExp> @line=9, col=29 :idx=0");
  sss.push_back("SgPlusPlusOp_undef_name0x709f7c4501b0<SgPlusPlusOp> @line=9, "
                "col=29 :idx=1");
  sss.push_back(
      "SgExprStatement0x709f7c483190<SgExprStatement> @line=9, col=29 :idx=1");
  sss.push_back(
      "0x709f7c6cc6c80x709f7c6cc6c8<SgBasicBlock> @line=6, col=45 :idx=1");
  sss.push_back(
      "0x709f7c5963d00x709f7c5963d0<SgForStatement> @line=6, col=17 :idx=3");
  sss.push_back("SgPlusPlusOp_undef_name0x709f7c450148<SgPlusPlusOp> @line=6, "
                "col=40 :idx=0");
  sss.push_back(
      "var_ref_of_l0x709f7c4e95e8<SgVarRefExp> @line=6, col=40 :idx=0");
  sss.push_back("SgPlusPlusOp_undef_name0x709f7c450148<SgPlusPlusOp> @line=6, "
                "col=40 :idx=1");
  sss.push_back(
      "0x709f7c5963d00x709f7c5963d0<SgForStatement> @line=6, col=17 :idx=1");
  sss.push_back(
      "SgExprStatement0x709f7c483130<SgExprStatement> @line=6, col=33 :idx=0");
  sss.push_back("SgLessThanOp_undef_name0x709f7c4b2320<SgLessThanOp> @line=6, "
                "col=33 :idx=0");
  sss.push_back(
      "var_ref_of_l0x709f7c4e9560<SgVarRefExp> @line=6, col=33 :idx=0");
  sss.push_back("SgLessThanOp_undef_name0x709f7c4b2320<SgLessThanOp> @line=6, "
                "col=33 :idx=1");
  sss.push_back(
      "integer_value_exp_50x709f7cbf7610<SgIntVal> @line=6, col=37 :idx=0");
  sss.push_back(
      "integer_value_exp_50x709f7cbf7610<SgIntVal> @line=6, col=37 :idx=1");
  sss.push_back("SgLessThanOp_undef_name0x709f7c4b2320<SgLessThanOp> @line=6, "
                "col=33 :idx=2");
  sss.push_back(
      "SgExprStatement0x709f7c483130<SgExprStatement> @line=6, col=33 :idx=1");
  sss.push_back(
      "0x709f7c5963d00x709f7c5963d0<SgForStatement> @line=6, col=17 :idx=2");
  sss.push_back(
      "0x709f7c5963d00x709f7c5963d0<SgForStatement> @line=6, col=17 :idx=4");
  sss.push_back(
      "0x709f7c6cc5700x709f7c6cc570<SgBasicBlock> @line=5, col=41 :idx=1");
  sss.push_back(
      "0x709f7c5962900x709f7c596290<SgForStatement> @line=5, col=13 :idx=3");
  sss.push_back("SgPlusPlusOp_undef_name0x709f7c4500e0<SgPlusPlusOp> @line=5, "
                "col=36 :idx=0");
  sss.push_back(
      "var_ref_of_k0x709f7c4e9450<SgVarRefExp> @line=5, col=36 :idx=0");
  sss.push_back("SgPlusPlusOp_undef_name0x709f7c4500e0<SgPlusPlusOp> @line=5, "
                "col=36 :idx=1");
  sss.push_back(
      "0x709f7c5962900x709f7c596290<SgForStatement> @line=5, col=13 :idx=1");
  sss.push_back(
      "SgExprStatement0x709f7c4830d0<SgExprStatement> @line=5, col=29 :idx=0");
  sss.push_back("SgLessThanOp_undef_name0x709f7c4b2240<SgLessThanOp> @line=5, "
                "col=29 :idx=0");
  sss.push_back(
      "var_ref_of_k0x709f7c4e93c8<SgVarRefExp> @line=5, col=29 :idx=0");
  sss.push_back("SgLessThanOp_undef_name0x709f7c4b2240<SgLessThanOp> @line=5, "
                "col=29 :idx=1");
  sss.push_back(
      "integer_value_exp_50x709f7cbf7490<SgIntVal> @line=5, col=33 :idx=0");
  sss.push_back(
      "integer_value_exp_50x709f7cbf7490<SgIntVal> @line=5, col=33 :idx=1");
  sss.push_back("SgLessThanOp_undef_name0x709f7c4b2240<SgLessThanOp> @line=5, "
                "col=29 :idx=2");
  sss.push_back(
      "SgExprStatement0x709f7c4830d0<SgExprStatement> @line=5, col=29 :idx=1");
  sss.push_back(
      "0x709f7c5962900x709f7c596290<SgForStatement> @line=5, col=13 :idx=2");
  sss.push_back(
      "0x709f7c5962900x709f7c596290<SgForStatement> @line=5, col=13 :idx=4");
  sss.push_back(
      "0x709f7c6cc4180x709f7c6cc418<SgBasicBlock> @line=4, col=37 :idx=1");
  sss.push_back(
      "0x709f7c5961500x709f7c596150<SgForStatement> @line=4, col=9 :idx=3");
  sss.push_back("SgPlusPlusOp_undef_name0x709f7c450078<SgPlusPlusOp> @line=4, "
                "col=32 :idx=0");
  sss.push_back(
      "var_ref_of_j0x709f7c4e92b8<SgVarRefExp> @line=4, col=32 :idx=0");
  sss.push_back("SgPlusPlusOp_undef_name0x709f7c450078<SgPlusPlusOp> @line=4, "
                "col=32 :idx=1");
  sss.push_back(
      "0x709f7c5961500x709f7c596150<SgForStatement> @line=4, col=9 :idx=1");
  sss.push_back(
      "SgExprStatement0x709f7c483070<SgExprStatement> @line=4, col=25 :idx=0");
  sss.push_back("SgLessThanOp_undef_name0x709f7c4b2160<SgLessThanOp> @line=4, "
                "col=25 :idx=0");
  sss.push_back(
      "var_ref_of_j0x709f7c4e9230<SgVarRefExp> @line=4, col=25 :idx=0");
  sss.push_back("SgLessThanOp_undef_name0x709f7c4b2160<SgLessThanOp> @line=4, "
                "col=25 :idx=1");
  sss.push_back(
      "integer_value_exp_50x709f7cbf7310<SgIntVal> @line=4, col=29 :idx=0");
  sss.push_back(
      "integer_value_exp_50x709f7cbf7310<SgIntVal> @line=4, col=29 :idx=1");
  sss.push_back("SgLessThanOp_undef_name0x709f7c4b2160<SgLessThanOp> @line=4, "
                "col=25 :idx=2");
  sss.push_back(
      "SgExprStatement0x709f7c483070<SgExprStatement> @line=4, col=25 :idx=1");
  sss.push_back(
      "0x709f7c5961500x709f7c596150<SgForStatement> @line=4, col=9 :idx=2");
  sss.push_back(
      "0x709f7c5961500x709f7c596150<SgForStatement> @line=4, col=9 :idx=4");
  sss.push_back(
      "0x709f7c6cc2c00x709f7c6cc2c0<SgBasicBlock> @line=3, col=33 :idx=1");
  sss.push_back(
      "0x709f7c5960100x709f7c596010<SgForStatement> @line=3, col=5 :idx=3");
  sss.push_back("SgPlusPlusOp_undef_name0x709f7c450010<SgPlusPlusOp> @line=3, "
                "col=28 :idx=0");
  sss.push_back(
      "var_ref_of_i0x709f7c4e9120<SgVarRefExp> @line=3, col=28 :idx=0");
  sss.push_back("SgPlusPlusOp_undef_name0x709f7c450010<SgPlusPlusOp> @line=3, "
                "col=28 :idx=1");
  sss.push_back(
      "0x709f7c5960100x709f7c596010<SgForStatement> @line=3, col=5 :idx=1");
  sss.push_back(
      "SgExprStatement0x709f7c483010<SgExprStatement> @line=3, col=21 :idx=0");
  sss.push_back("SgLessThanOp_undef_name0x709f7c4b2080<SgLessThanOp> @line=3, "
                "col=21 :idx=0");
  sss.push_back(
      "var_ref_of_i0x709f7c4e9098<SgVarRefExp> @line=3, col=21 :idx=0");
  sss.push_back("SgLessThanOp_undef_name0x709f7c4b2080<SgLessThanOp> @line=3, "
                "col=21 :idx=1");
  sss.push_back(
      "integer_value_exp_50x709f7cbf7190<SgIntVal> @line=3, col=25 :idx=0");
  sss.push_back(
      "integer_value_exp_50x709f7cbf7190<SgIntVal> @line=3, col=25 :idx=1");
  sss.push_back("SgLessThanOp_undef_name0x709f7c4b2080<SgLessThanOp> @line=3, "
                "col=21 :idx=2");
  sss.push_back(
      "SgExprStatement0x709f7c483010<SgExprStatement> @line=3, col=21 :idx=1");
  sss.push_back(
      "0x709f7c5960100x709f7c596010<SgForStatement> @line=3, col=5 :idx=2");
  sss.push_back(
      "0x709f7c5960100x709f7c596010<SgForStatement> @line=3, col=5 :idx=4");
  sss.push_back(
      "0x709f7c6cc1680x709f7c6cc168<SgBasicBlock> @line=1, col=12 :idx=2");
  sss.push_back(
      "SgReturnStmt0x709f7c421010<SgReturnStmt> @line=16, col=5 :idx=0");
  sss.push_back(
      "integer_value_exp_00x709f7cbf7690<SgIntVal> @line=16, col=12 :idx=0");
  sss.push_back(
      "integer_value_exp_00x709f7cbf7690<SgIntVal> @line=16, col=12 :idx=1");
  sss.push_back(
      "SgReturnStmt0x709f7c421010<SgReturnStmt> @line=16, col=5 :idx=1");
  sss.push_back(
      "End(::main)0x709f7c633010<SgFunctionDefinition> @line=1, col=1 :idx=3");
  sssv.insert(sss);
  sss.clear();
  sss.push_back("Start(::main)0x709f7c633010<SgFunctionDefinition> @line=1, "
                "col=1 :idx=0");
  sss.push_back("main_parameter_list_0x709f7ca34010<SgFunctionParameterList> "
                "@line=1, col=9 :idx=0");
  sss.push_back("After parameters(::main)0x709f7c633010<SgFunctionDefinition> "
                "@line=1, col=1 :idx=1");
  sss.push_back(
      "After pre-initialization(::main)0x709f7c633010<SgFunctionDefinition> "
      "@line=1, col=1 :idx=2");
  sss.push_back(
      "0x709f7c6cc1680x709f7c6cc168<SgBasicBlock> @line=1, col=12 :idx=0");
  sss.push_back("_variable_declaration_m0x709f7cf3f710<SgVariableDeclaration> "
                "@line=2, col=5 :idx=0");
  sss.push_back("initialized_name_m0x709f7ce4bf90<SgInitializedName> m :idx=0");
  sss.push_back("_variable_declaration_m0x709f7cf3f710<SgVariableDeclaration> "
                "@line=2, col=5 :idx=1");
  sss.push_back(
      "0x709f7c6cc1680x709f7c6cc168<SgBasicBlock> @line=1, col=12 :idx=1");
  sss.push_back(
      "0x709f7c5960100x709f7c596010<SgForStatement> @line=3, col=5 :idx=0");
  sss.push_back("SgForInitStatement0x709f7c55f010<SgForInitStatement> @line=3, "
                "col=10 :idx=0");
  sss.push_back("_variable_declaration_i0x709f7cf3f9f0<SgVariableDeclaration> "
                "@line=3, col=10 :idx=0");
  sss.push_back("initialized_name_i0x709f7ce4c180<SgInitializedName> i :idx=0");
  sss.push_back("SgAssignInitializer_undef_name0x709f7c52c010<"
                "SgAssignInitializer> @line=3, col=18 :idx=0");
  sss.push_back(
      "integer_value_exp_00x709f7cbf7090<SgIntVal> @line=3, col=18 :idx=0");
  sss.push_back(
      "integer_value_exp_00x709f7cbf7090<SgIntVal> @line=3, col=18 :idx=1");
  sss.push_back("SgAssignInitializer_undef_name0x709f7c52c010<"
                "SgAssignInitializer> @line=3, col=18 :idx=1");
  sss.push_back("initialized_name_i0x709f7ce4c180<SgInitializedName> i :idx=1");
  sss.push_back("_variable_declaration_i0x709f7cf3f9f0<SgVariableDeclaration> "
                "@line=3, col=10 :idx=1");
  sss.push_back("SgForInitStatement0x709f7c55f010<SgForInitStatement> @line=3, "
                "col=10 :idx=1");
  sss.push_back(
      "0x709f7c5960100x709f7c596010<SgForStatement> @line=3, col=5 :idx=1");
  sss.push_back(
      "SgExprStatement0x709f7c483010<SgExprStatement> @line=3, col=21 :idx=0");
  sss.push_back("SgLessThanOp_undef_name0x709f7c4b2080<SgLessThanOp> @line=3, "
                "col=21 :idx=0");
  sss.push_back(
      "var_ref_of_i0x709f7c4e9098<SgVarRefExp> @line=3, col=21 :idx=0");
  sss.push_back("SgLessThanOp_undef_name0x709f7c4b2080<SgLessThanOp> @line=3, "
                "col=21 :idx=1");
  sss.push_back(
      "integer_value_exp_50x709f7cbf7190<SgIntVal> @line=3, col=25 :idx=0");
  sss.push_back(
      "integer_value_exp_50x709f7cbf7190<SgIntVal> @line=3, col=25 :idx=1");
  sss.push_back("SgLessThanOp_undef_name0x709f7c4b2080<SgLessThanOp> @line=3, "
                "col=21 :idx=2");
  sss.push_back(
      "SgExprStatement0x709f7c483010<SgExprStatement> @line=3, col=21 :idx=1");
  sss.push_back(
      "0x709f7c5960100x709f7c596010<SgForStatement> @line=3, col=5 :idx=2");
  sss.push_back(
      "0x709f7c6cc2c00x709f7c6cc2c0<SgBasicBlock> @line=3, col=33 :idx=0");
  sss.push_back(
      "0x709f7c5961500x709f7c596150<SgForStatement> @line=4, col=9 :idx=0");
  sss.push_back("SgForInitStatement0x709f7c55f080<SgForInitStatement> @line=4, "
                "col=14 :idx=0");
  sss.push_back("_variable_declaration_j0x709f7cf3fcd0<SgVariableDeclaration> "
                "@line=4, col=14 :idx=0");
  sss.push_back("initialized_name_j0x709f7ce4c370<SgInitializedName> j :idx=0");
  sss.push_back("SgAssignInitializer_undef_name0x709f7c52c078<"
                "SgAssignInitializer> @line=4, col=22 :idx=0");
  sss.push_back(
      "integer_value_exp_00x709f7cbf7210<SgIntVal> @line=4, col=22 :idx=0");
  sss.push_back(
      "integer_value_exp_00x709f7cbf7210<SgIntVal> @line=4, col=22 :idx=1");
  sss.push_back("SgAssignInitializer_undef_name0x709f7c52c078<"
                "SgAssignInitializer> @line=4, col=22 :idx=1");
  sss.push_back("initialized_name_j0x709f7ce4c370<SgInitializedName> j :idx=1");
  sss.push_back("_variable_declaration_j0x709f7cf3fcd0<SgVariableDeclaration> "
                "@line=4, col=14 :idx=1");
  sss.push_back("SgForInitStatement0x709f7c55f080<SgForInitStatement> @line=4, "
                "col=14 :idx=1");
  sss.push_back(
      "0x709f7c5961500x709f7c596150<SgForStatement> @line=4, col=9 :idx=1");
  sss.push_back(
      "SgExprStatement0x709f7c483070<SgExprStatement> @line=4, col=25 :idx=0");
  sss.push_back("SgLessThanOp_undef_name0x709f7c4b2160<SgLessThanOp> @line=4, "
                "col=25 :idx=0");
  sss.push_back(
      "var_ref_of_j0x709f7c4e9230<SgVarRefExp> @line=4, col=25 :idx=0");
  sss.push_back("SgLessThanOp_undef_name0x709f7c4b2160<SgLessThanOp> @line=4, "
                "col=25 :idx=1");
  sss.push_back(
      "integer_value_exp_50x709f7cbf7310<SgIntVal> @line=4, col=29 :idx=0");
  sss.push_back(
      "integer_value_exp_50x709f7cbf7310<SgIntVal> @line=4, col=29 :idx=1");
  sss.push_back("SgLessThanOp_undef_name0x709f7c4b2160<SgLessThanOp> @line=4, "
                "col=25 :idx=2");
  sss.push_back(
      "SgExprStatement0x709f7c483070<SgExprStatement> @line=4, col=25 :idx=1");
  sss.push_back(
      "0x709f7c5961500x709f7c596150<SgForStatement> @line=4, col=9 :idx=2");
  sss.push_back(
      "0x709f7c6cc4180x709f7c6cc418<SgBasicBlock> @line=4, col=37 :idx=0");
  sss.push_back(
      "0x709f7c5962900x709f7c596290<SgForStatement> @line=5, col=13 :idx=0");
  sss.push_back("SgForInitStatement0x709f7c55f0f0<SgForInitStatement> @line=5, "
                "col=18 :idx=0");
  sss.push_back("_variable_declaration_k0x709f7cf3ffb0<SgVariableDeclaration> "
                "@line=5, col=18 :idx=0");
  sss.push_back("initialized_name_k0x709f7ce4c560<SgInitializedName> k :idx=0");
  sss.push_back("SgAssignInitializer_undef_name0x709f7c52c0e0<"
                "SgAssignInitializer> @line=5, col=26 :idx=0");
  sss.push_back(
      "integer_value_exp_00x709f7cbf7390<SgIntVal> @line=5, col=26 :idx=0");
  sss.push_back(
      "integer_value_exp_00x709f7cbf7390<SgIntVal> @line=5, col=26 :idx=1");
  sss.push_back("SgAssignInitializer_undef_name0x709f7c52c0e0<"
                "SgAssignInitializer> @line=5, col=26 :idx=1");
  sss.push_back("initialized_name_k0x709f7ce4c560<SgInitializedName> k :idx=1");
  sss.push_back("_variable_declaration_k0x709f7cf3ffb0<SgVariableDeclaration> "
                "@line=5, col=18 :idx=1");
  sss.push_back("SgForInitStatement0x709f7c55f0f0<SgForInitStatement> @line=5, "
                "col=18 :idx=1");
  sss.push_back(
      "0x709f7c5962900x709f7c596290<SgForStatement> @line=5, col=13 :idx=1");
  sss.push_back(
      "SgExprStatement0x709f7c4830d0<SgExprStatement> @line=5, col=29 :idx=0");
  sss.push_back("SgLessThanOp_undef_name0x709f7c4b2240<SgLessThanOp> @line=5, "
                "col=29 :idx=0");
  sss.push_back(
      "var_ref_of_k0x709f7c4e93c8<SgVarRefExp> @line=5, col=29 :idx=0");
  sss.push_back("SgLessThanOp_undef_name0x709f7c4b2240<SgLessThanOp> @line=5, "
                "col=29 :idx=1");
  sss.push_back(
      "integer_value_exp_50x709f7cbf7490<SgIntVal> @line=5, col=33 :idx=0");
  sss.push_back(
      "integer_value_exp_50x709f7cbf7490<SgIntVal> @line=5, col=33 :idx=1");
  sss.push_back("SgLessThanOp_undef_name0x709f7c4b2240<SgLessThanOp> @line=5, "
                "col=29 :idx=2");
  sss.push_back(
      "SgExprStatement0x709f7c4830d0<SgExprStatement> @line=5, col=29 :idx=1");
  sss.push_back(
      "0x709f7c5962900x709f7c596290<SgForStatement> @line=5, col=13 :idx=2");
  sss.push_back(
      "0x709f7c6cc5700x709f7c6cc570<SgBasicBlock> @line=5, col=41 :idx=0");
  sss.push_back(
      "0x709f7c5963d00x709f7c5963d0<SgForStatement> @line=6, col=17 :idx=0");
  sss.push_back("SgForInitStatement0x709f7c55f160<SgForInitStatement> @line=6, "
                "col=22 :idx=0");
  sss.push_back("_variable_declaration_l0x709f7cf40290<SgVariableDeclaration> "
                "@line=6, col=22 :idx=0");
  sss.push_back("initialized_name_l0x709f7ce4c750<SgInitializedName> l :idx=0");
  sss.push_back("SgAssignInitializer_undef_name0x709f7c52c148<"
                "SgAssignInitializer> @line=6, col=30 :idx=0");
  sss.push_back(
      "integer_value_exp_00x709f7cbf7510<SgIntVal> @line=6, col=30 :idx=0");
  sss.push_back(
      "integer_value_exp_00x709f7cbf7510<SgIntVal> @line=6, col=30 :idx=1");
  sss.push_back("SgAssignInitializer_undef_name0x709f7c52c148<"
                "SgAssignInitializer> @line=6, col=30 :idx=1");
  sss.push_back("initialized_name_l0x709f7ce4c750<SgInitializedName> l :idx=1");
  sss.push_back("_variable_declaration_l0x709f7cf40290<SgVariableDeclaration> "
                "@line=6, col=22 :idx=1");
  sss.push_back("SgForInitStatement0x709f7c55f160<SgForInitStatement> @line=6, "
                "col=22 :idx=1");
  sss.push_back(
      "0x709f7c5963d00x709f7c5963d0<SgForStatement> @line=6, col=17 :idx=1");
  sss.push_back(
      "SgExprStatement0x709f7c483130<SgExprStatement> @line=6, col=33 :idx=0");
  sss.push_back("SgLessThanOp_undef_name0x709f7c4b2320<SgLessThanOp> @line=6, "
                "col=33 :idx=0");
  sss.push_back(
      "var_ref_of_l0x709f7c4e9560<SgVarRefExp> @line=6, col=33 :idx=0");
  sss.push_back("SgLessThanOp_undef_name0x709f7c4b2320<SgLessThanOp> @line=6, "
                "col=33 :idx=1");
  sss.push_back(
      "integer_value_exp_50x709f7cbf7610<SgIntVal> @line=6, col=37 :idx=0");
  sss.push_back(
      "integer_value_exp_50x709f7cbf7610<SgIntVal> @line=6, col=37 :idx=1");
  sss.push_back("SgLessThanOp_undef_name0x709f7c4b2320<SgLessThanOp> @line=6, "
                "col=33 :idx=2");
  sss.push_back(
      "SgExprStatement0x709f7c483130<SgExprStatement> @line=6, col=33 :idx=1");
  sss.push_back(
      "0x709f7c5963d00x709f7c5963d0<SgForStatement> @line=6, col=17 :idx=2");
  sss.push_back(
      "0x709f7c5963d00x709f7c5963d0<SgForStatement> @line=6, col=17 :idx=4");
  sss.push_back(
      "0x709f7c6cc5700x709f7c6cc570<SgBasicBlock> @line=5, col=41 :idx=1");
  sss.push_back(
      "0x709f7c5962900x709f7c596290<SgForStatement> @line=5, col=13 :idx=3");
  sss.push_back("SgPlusPlusOp_undef_name0x709f7c4500e0<SgPlusPlusOp> @line=5, "
                "col=36 :idx=0");
  sss.push_back(
      "var_ref_of_k0x709f7c4e9450<SgVarRefExp> @line=5, col=36 :idx=0");
  sss.push_back("SgPlusPlusOp_undef_name0x709f7c4500e0<SgPlusPlusOp> @line=5, "
                "col=36 :idx=1");
  sss.push_back(
      "0x709f7c5962900x709f7c596290<SgForStatement> @line=5, col=13 :idx=1");
  sss.push_back(
      "SgExprStatement0x709f7c4830d0<SgExprStatement> @line=5, col=29 :idx=0");
  sss.push_back("SgLessThanOp_undef_name0x709f7c4b2240<SgLessThanOp> @line=5, "
                "col=29 :idx=0");
  sss.push_back(
      "var_ref_of_k0x709f7c4e93c8<SgVarRefExp> @line=5, col=29 :idx=0");
  sss.push_back("SgLessThanOp_undef_name0x709f7c4b2240<SgLessThanOp> @line=5, "
                "col=29 :idx=1");
  sss.push_back(
      "integer_value_exp_50x709f7cbf7490<SgIntVal> @line=5, col=33 :idx=0");
  sss.push_back(
      "integer_value_exp_50x709f7cbf7490<SgIntVal> @line=5, col=33 :idx=1");
  sss.push_back("SgLessThanOp_undef_name0x709f7c4b2240<SgLessThanOp> @line=5, "
                "col=29 :idx=2");
  sss.push_back(
      "SgExprStatement0x709f7c4830d0<SgExprStatement> @line=5, col=29 :idx=1");
  sss.push_back(
      "0x709f7c5962900x709f7c596290<SgForStatement> @line=5, col=13 :idx=2");
  sss.push_back(
      "0x709f7c5962900x709f7c596290<SgForStatement> @line=5, col=13 :idx=4");
  sss.push_back(
      "0x709f7c6cc4180x709f7c6cc418<SgBasicBlock> @line=4, col=37 :idx=1");
  sss.push_back(
      "0x709f7c5961500x709f7c596150<SgForStatement> @line=4, col=9 :idx=3");
  sss.push_back("SgPlusPlusOp_undef_name0x709f7c450078<SgPlusPlusOp> @line=4, "
                "col=32 :idx=0");
  sss.push_back(
      "var_ref_of_j0x709f7c4e92b8<SgVarRefExp> @line=4, col=32 :idx=0");
  sss.push_back("SgPlusPlusOp_undef_name0x709f7c450078<SgPlusPlusOp> @line=4, "
                "col=32 :idx=1");
  sss.push_back(
      "0x709f7c5961500x709f7c596150<SgForStatement> @line=4, col=9 :idx=1");
  sss.push_back(
      "SgExprStatement0x709f7c483070<SgExprStatement> @line=4, col=25 :idx=0");
  sss.push_back("SgLessThanOp_undef_name0x709f7c4b2160<SgLessThanOp> @line=4, "
                "col=25 :idx=0");
  sss.push_back(
      "var_ref_of_j0x709f7c4e9230<SgVarRefExp> @line=4, col=25 :idx=0");
  sss.push_back("SgLessThanOp_undef_name0x709f7c4b2160<SgLessThanOp> @line=4, "
                "col=25 :idx=1");
  sss.push_back(
      "integer_value_exp_50x709f7cbf7310<SgIntVal> @line=4, col=29 :idx=0");
  sss.push_back(
      "integer_value_exp_50x709f7cbf7310<SgIntVal> @line=4, col=29 :idx=1");
  sss.push_back("SgLessThanOp_undef_name0x709f7c4b2160<SgLessThanOp> @line=4, "
                "col=25 :idx=2");
  sss.push_back(
      "SgExprStatement0x709f7c483070<SgExprStatement> @line=4, col=25 :idx=1");
  sss.push_back(
      "0x709f7c5961500x709f7c596150<SgForStatement> @line=4, col=9 :idx=2");
  sss.push_back(
      "0x709f7c5961500x709f7c596150<SgForStatement> @line=4, col=9 :idx=4");
  sss.push_back(
      "0x709f7c6cc2c00x709f7c6cc2c0<SgBasicBlock> @line=3, col=33 :idx=1");
  sss.push_back(
      "0x709f7c5960100x709f7c596010<SgForStatement> @line=3, col=5 :idx=3");
  sss.push_back("SgPlusPlusOp_undef_name0x709f7c450010<SgPlusPlusOp> @line=3, "
                "col=28 :idx=0");
  sss.push_back(
      "var_ref_of_i0x709f7c4e9120<SgVarRefExp> @line=3, col=28 :idx=0");
  sss.push_back("SgPlusPlusOp_undef_name0x709f7c450010<SgPlusPlusOp> @line=3, "
                "col=28 :idx=1");
  sss.push_back(
      "0x709f7c5960100x709f7c596010<SgForStatement> @line=3, col=5 :idx=1");
  sss.push_back(
      "SgExprStatement0x709f7c483010<SgExprStatement> @line=3, col=21 :idx=0");
  sss.push_back("SgLessThanOp_undef_name0x709f7c4b2080<SgLessThanOp> @line=3, "
                "col=21 :idx=0");
  sss.push_back(
      "var_ref_of_i0x709f7c4e9098<SgVarRefExp> @line=3, col=21 :idx=0");
  sss.push_back("SgLessThanOp_undef_name0x709f7c4b2080<SgLessThanOp> @line=3, "
                "col=21 :idx=1");
  sss.push_back(
      "integer_value_exp_50x709f7cbf7190<SgIntVal> @line=3, col=25 :idx=0");
  sss.push_back(
      "integer_value_exp_50x709f7cbf7190<SgIntVal> @line=3, col=25 :idx=1");
  sss.push_back("SgLessThanOp_undef_name0x709f7c4b2080<SgLessThanOp> @line=3, "
                "col=21 :idx=2");
  sss.push_back(
      "SgExprStatement0x709f7c483010<SgExprStatement> @line=3, col=21 :idx=1");
  sss.push_back(
      "0x709f7c5960100x709f7c596010<SgForStatement> @line=3, col=5 :idx=2");
  sss.push_back(
      "0x709f7c5960100x709f7c596010<SgForStatement> @line=3, col=5 :idx=4");
  sss.push_back(
      "0x709f7c6cc1680x709f7c6cc168<SgBasicBlock> @line=1, col=12 :idx=2");
  sss.push_back(
      "SgReturnStmt0x709f7c421010<SgReturnStmt> @line=16, col=5 :idx=0");
  sss.push_back(
      "integer_value_exp_00x709f7cbf7690<SgIntVal> @line=16, col=12 :idx=0");
  sss.push_back(
      "integer_value_exp_00x709f7cbf7690<SgIntVal> @line=16, col=12 :idx=1");
  sss.push_back(
      "SgReturnStmt0x709f7c421010<SgReturnStmt> @line=16, col=5 :idx=1");
  sss.push_back(
      "End(::main)0x709f7c633010<SgFunctionDefinition> @line=1, col=1 :idx=3");
  sssv.insert(sss);
  sss.clear();
  sss.push_back("Start(::main)0x709f7c633010<SgFunctionDefinition> @line=1, "
                "col=1 :idx=0");
  sss.push_back("main_parameter_list_0x709f7ca34010<SgFunctionParameterList> "
                "@line=1, col=9 :idx=0");
  sss.push_back("After parameters(::main)0x709f7c633010<SgFunctionDefinition> "
                "@line=1, col=1 :idx=1");
  sss.push_back(
      "After pre-initialization(::main)0x709f7c633010<SgFunctionDefinition> "
      "@line=1, col=1 :idx=2");
  sss.push_back(
      "0x709f7c6cc1680x709f7c6cc168<SgBasicBlock> @line=1, col=12 :idx=0");
  sss.push_back("_variable_declaration_m0x709f7cf3f710<SgVariableDeclaration> "
                "@line=2, col=5 :idx=0");
  sss.push_back("initialized_name_m0x709f7ce4bf90<SgInitializedName> m :idx=0");
  sss.push_back("_variable_declaration_m0x709f7cf3f710<SgVariableDeclaration> "
                "@line=2, col=5 :idx=1");
  sss.push_back(
      "0x709f7c6cc1680x709f7c6cc168<SgBasicBlock> @line=1, col=12 :idx=1");
  sss.push_back(
      "0x709f7c5960100x709f7c596010<SgForStatement> @line=3, col=5 :idx=0");
  sss.push_back("SgForInitStatement0x709f7c55f010<SgForInitStatement> @line=3, "
                "col=10 :idx=0");
  sss.push_back("_variable_declaration_i0x709f7cf3f9f0<SgVariableDeclaration> "
                "@line=3, col=10 :idx=0");
  sss.push_back("initialized_name_i0x709f7ce4c180<SgInitializedName> i :idx=0");
  sss.push_back("SgAssignInitializer_undef_name0x709f7c52c010<"
                "SgAssignInitializer> @line=3, col=18 :idx=0");
  sss.push_back(
      "integer_value_exp_00x709f7cbf7090<SgIntVal> @line=3, col=18 :idx=0");
  sss.push_back(
      "integer_value_exp_00x709f7cbf7090<SgIntVal> @line=3, col=18 :idx=1");
  sss.push_back("SgAssignInitializer_undef_name0x709f7c52c010<"
                "SgAssignInitializer> @line=3, col=18 :idx=1");
  sss.push_back("initialized_name_i0x709f7ce4c180<SgInitializedName> i :idx=1");
  sss.push_back("_variable_declaration_i0x709f7cf3f9f0<SgVariableDeclaration> "
                "@line=3, col=10 :idx=1");
  sss.push_back("SgForInitStatement0x709f7c55f010<SgForInitStatement> @line=3, "
                "col=10 :idx=1");
  sss.push_back(
      "0x709f7c5960100x709f7c596010<SgForStatement> @line=3, col=5 :idx=1");
  sss.push_back(
      "SgExprStatement0x709f7c483010<SgExprStatement> @line=3, col=21 :idx=0");
  sss.push_back("SgLessThanOp_undef_name0x709f7c4b2080<SgLessThanOp> @line=3, "
                "col=21 :idx=0");
  sss.push_back(
      "var_ref_of_i0x709f7c4e9098<SgVarRefExp> @line=3, col=21 :idx=0");
  sss.push_back("SgLessThanOp_undef_name0x709f7c4b2080<SgLessThanOp> @line=3, "
                "col=21 :idx=1");
  sss.push_back(
      "integer_value_exp_50x709f7cbf7190<SgIntVal> @line=3, col=25 :idx=0");
  sss.push_back(
      "integer_value_exp_50x709f7cbf7190<SgIntVal> @line=3, col=25 :idx=1");
  sss.push_back("SgLessThanOp_undef_name0x709f7c4b2080<SgLessThanOp> @line=3, "
                "col=21 :idx=2");
  sss.push_back(
      "SgExprStatement0x709f7c483010<SgExprStatement> @line=3, col=21 :idx=1");
  sss.push_back(
      "0x709f7c5960100x709f7c596010<SgForStatement> @line=3, col=5 :idx=2");
  sss.push_back(
      "0x709f7c6cc2c00x709f7c6cc2c0<SgBasicBlock> @line=3, col=33 :idx=0");
  sss.push_back(
      "0x709f7c5961500x709f7c596150<SgForStatement> @line=4, col=9 :idx=0");
  sss.push_back("SgForInitStatement0x709f7c55f080<SgForInitStatement> @line=4, "
                "col=14 :idx=0");
  sss.push_back("_variable_declaration_j0x709f7cf3fcd0<SgVariableDeclaration> "
                "@line=4, col=14 :idx=0");
  sss.push_back("initialized_name_j0x709f7ce4c370<SgInitializedName> j :idx=0");
  sss.push_back("SgAssignInitializer_undef_name0x709f7c52c078<"
                "SgAssignInitializer> @line=4, col=22 :idx=0");
  sss.push_back(
      "integer_value_exp_00x709f7cbf7210<SgIntVal> @line=4, col=22 :idx=0");
  sss.push_back(
      "integer_value_exp_00x709f7cbf7210<SgIntVal> @line=4, col=22 :idx=1");
  sss.push_back("SgAssignInitializer_undef_name0x709f7c52c078<"
                "SgAssignInitializer> @line=4, col=22 :idx=1");
  sss.push_back("initialized_name_j0x709f7ce4c370<SgInitializedName> j :idx=1");
  sss.push_back("_variable_declaration_j0x709f7cf3fcd0<SgVariableDeclaration> "
                "@line=4, col=14 :idx=1");
  sss.push_back("SgForInitStatement0x709f7c55f080<SgForInitStatement> @line=4, "
                "col=14 :idx=1");
  sss.push_back(
      "0x709f7c5961500x709f7c596150<SgForStatement> @line=4, col=9 :idx=1");
  sss.push_back(
      "SgExprStatement0x709f7c483070<SgExprStatement> @line=4, col=25 :idx=0");
  sss.push_back("SgLessThanOp_undef_name0x709f7c4b2160<SgLessThanOp> @line=4, "
                "col=25 :idx=0");
  sss.push_back(
      "var_ref_of_j0x709f7c4e9230<SgVarRefExp> @line=4, col=25 :idx=0");
  sss.push_back("SgLessThanOp_undef_name0x709f7c4b2160<SgLessThanOp> @line=4, "
                "col=25 :idx=1");
  sss.push_back(
      "integer_value_exp_50x709f7cbf7310<SgIntVal> @line=4, col=29 :idx=0");
  sss.push_back(
      "integer_value_exp_50x709f7cbf7310<SgIntVal> @line=4, col=29 :idx=1");
  sss.push_back("SgLessThanOp_undef_name0x709f7c4b2160<SgLessThanOp> @line=4, "
                "col=25 :idx=2");
  sss.push_back(
      "SgExprStatement0x709f7c483070<SgExprStatement> @line=4, col=25 :idx=1");
  sss.push_back(
      "0x709f7c5961500x709f7c596150<SgForStatement> @line=4, col=9 :idx=2");
  sss.push_back(
      "0x709f7c6cc4180x709f7c6cc418<SgBasicBlock> @line=4, col=37 :idx=0");
  sss.push_back(
      "0x709f7c5962900x709f7c596290<SgForStatement> @line=5, col=13 :idx=0");
  sss.push_back("SgForInitStatement0x709f7c55f0f0<SgForInitStatement> @line=5, "
                "col=18 :idx=0");
  sss.push_back("_variable_declaration_k0x709f7cf3ffb0<SgVariableDeclaration> "
                "@line=5, col=18 :idx=0");
  sss.push_back("initialized_name_k0x709f7ce4c560<SgInitializedName> k :idx=0");
  sss.push_back("SgAssignInitializer_undef_name0x709f7c52c0e0<"
                "SgAssignInitializer> @line=5, col=26 :idx=0");
  sss.push_back(
      "integer_value_exp_00x709f7cbf7390<SgIntVal> @line=5, col=26 :idx=0");
  sss.push_back(
      "integer_value_exp_00x709f7cbf7390<SgIntVal> @line=5, col=26 :idx=1");
  sss.push_back("SgAssignInitializer_undef_name0x709f7c52c0e0<"
                "SgAssignInitializer> @line=5, col=26 :idx=1");
  sss.push_back("initialized_name_k0x709f7ce4c560<SgInitializedName> k :idx=1");
  sss.push_back("_variable_declaration_k0x709f7cf3ffb0<SgVariableDeclaration> "
                "@line=5, col=18 :idx=1");
  sss.push_back("SgForInitStatement0x709f7c55f0f0<SgForInitStatement> @line=5, "
                "col=18 :idx=1");
  sss.push_back(
      "0x709f7c5962900x709f7c596290<SgForStatement> @line=5, col=13 :idx=1");
  sss.push_back(
      "SgExprStatement0x709f7c4830d0<SgExprStatement> @line=5, col=29 :idx=0");
  sss.push_back("SgLessThanOp_undef_name0x709f7c4b2240<SgLessThanOp> @line=5, "
                "col=29 :idx=0");
  sss.push_back(
      "var_ref_of_k0x709f7c4e93c8<SgVarRefExp> @line=5, col=29 :idx=0");
  sss.push_back("SgLessThanOp_undef_name0x709f7c4b2240<SgLessThanOp> @line=5, "
                "col=29 :idx=1");
  sss.push_back(
      "integer_value_exp_50x709f7cbf7490<SgIntVal> @line=5, col=33 :idx=0");
  sss.push_back(
      "integer_value_exp_50x709f7cbf7490<SgIntVal> @line=5, col=33 :idx=1");
  sss.push_back("SgLessThanOp_undef_name0x709f7c4b2240<SgLessThanOp> @line=5, "
                "col=29 :idx=2");
  sss.push_back(
      "SgExprStatement0x709f7c4830d0<SgExprStatement> @line=5, col=29 :idx=1");
  sss.push_back(
      "0x709f7c5962900x709f7c596290<SgForStatement> @line=5, col=13 :idx=2");
  sss.push_back(
      "0x709f7c5962900x709f7c596290<SgForStatement> @line=5, col=13 :idx=4");
  sss.push_back(
      "0x709f7c6cc4180x709f7c6cc418<SgBasicBlock> @line=4, col=37 :idx=1");
  sss.push_back(
      "0x709f7c5961500x709f7c596150<SgForStatement> @line=4, col=9 :idx=3");
  sss.push_back("SgPlusPlusOp_undef_name0x709f7c450078<SgPlusPlusOp> @line=4, "
                "col=32 :idx=0");
  sss.push_back(
      "var_ref_of_j0x709f7c4e92b8<SgVarRefExp> @line=4, col=32 :idx=0");
  sss.push_back("SgPlusPlusOp_undef_name0x709f7c450078<SgPlusPlusOp> @line=4, "
                "col=32 :idx=1");
  sss.push_back(
      "0x709f7c5961500x709f7c596150<SgForStatement> @line=4, col=9 :idx=1");
  sss.push_back(
      "SgExprStatement0x709f7c483070<SgExprStatement> @line=4, col=25 :idx=0");
  sss.push_back("SgLessThanOp_undef_name0x709f7c4b2160<SgLessThanOp> @line=4, "
                "col=25 :idx=0");
  sss.push_back(
      "var_ref_of_j0x709f7c4e9230<SgVarRefExp> @line=4, col=25 :idx=0");
  sss.push_back("SgLessThanOp_undef_name0x709f7c4b2160<SgLessThanOp> @line=4, "
                "col=25 :idx=1");
  sss.push_back(
      "integer_value_exp_50x709f7cbf7310<SgIntVal> @line=4, col=29 :idx=0");
  sss.push_back(
      "integer_value_exp_50x709f7cbf7310<SgIntVal> @line=4, col=29 :idx=1");
  sss.push_back("SgLessThanOp_undef_name0x709f7c4b2160<SgLessThanOp> @line=4, "
                "col=25 :idx=2");
  sss.push_back(
      "SgExprStatement0x709f7c483070<SgExprStatement> @line=4, col=25 :idx=1");
  sss.push_back(
      "0x709f7c5961500x709f7c596150<SgForStatement> @line=4, col=9 :idx=2");
  sss.push_back(
      "0x709f7c5961500x709f7c596150<SgForStatement> @line=4, col=9 :idx=4");
  sss.push_back(
      "0x709f7c6cc2c00x709f7c6cc2c0<SgBasicBlock> @line=3, col=33 :idx=1");
  sss.push_back(
      "0x709f7c5960100x709f7c596010<SgForStatement> @line=3, col=5 :idx=3");
  sss.push_back("SgPlusPlusOp_undef_name0x709f7c450010<SgPlusPlusOp> @line=3, "
                "col=28 :idx=0");
  sss.push_back(
      "var_ref_of_i0x709f7c4e9120<SgVarRefExp> @line=3, col=28 :idx=0");
  sss.push_back("SgPlusPlusOp_undef_name0x709f7c450010<SgPlusPlusOp> @line=3, "
                "col=28 :idx=1");
  sss.push_back(
      "0x709f7c5960100x709f7c596010<SgForStatement> @line=3, col=5 :idx=1");
  sss.push_back(
      "SgExprStatement0x709f7c483010<SgExprStatement> @line=3, col=21 :idx=0");
  sss.push_back("SgLessThanOp_undef_name0x709f7c4b2080<SgLessThanOp> @line=3, "
                "col=21 :idx=0");
  sss.push_back(
      "var_ref_of_i0x709f7c4e9098<SgVarRefExp> @line=3, col=21 :idx=0");
  sss.push_back("SgLessThanOp_undef_name0x709f7c4b2080<SgLessThanOp> @line=3, "
                "col=21 :idx=1");
  sss.push_back(
      "integer_value_exp_50x709f7cbf7190<SgIntVal> @line=3, col=25 :idx=0");
  sss.push_back(
      "integer_value_exp_50x709f7cbf7190<SgIntVal> @line=3, col=25 :idx=1");
  sss.push_back("SgLessThanOp_undef_name0x709f7c4b2080<SgLessThanOp> @line=3, "
                "col=21 :idx=2");
  sss.push_back(
      "SgExprStatement0x709f7c483010<SgExprStatement> @line=3, col=21 :idx=1");
  sss.push_back(
      "0x709f7c5960100x709f7c596010<SgForStatement> @line=3, col=5 :idx=2");
  sss.push_back(
      "0x709f7c5960100x709f7c596010<SgForStatement> @line=3, col=5 :idx=4");
  sss.push_back(
      "0x709f7c6cc1680x709f7c6cc168<SgBasicBlock> @line=1, col=12 :idx=2");
  sss.push_back(
      "SgReturnStmt0x709f7c421010<SgReturnStmt> @line=16, col=5 :idx=0");
  sss.push_back(
      "integer_value_exp_00x709f7cbf7690<SgIntVal> @line=16, col=12 :idx=0");
  sss.push_back(
      "integer_value_exp_00x709f7cbf7690<SgIntVal> @line=16, col=12 :idx=1");
  sss.push_back(
      "SgReturnStmt0x709f7c421010<SgReturnStmt> @line=16, col=5 :idx=1");
  sss.push_back(
      "End(::main)0x709f7c633010<SgFunctionDefinition> @line=1, col=1 :idx=3");
  sssv.insert(sss);
  sss.clear();
  sss.push_back("Start(::main)0x709f7c633010<SgFunctionDefinition> @line=1, "
                "col=1 :idx=0");
  sss.push_back("main_parameter_list_0x709f7ca34010<SgFunctionParameterList> "
                "@line=1, col=9 :idx=0");
  sss.push_back("After parameters(::main)0x709f7c633010<SgFunctionDefinition> "
                "@line=1, col=1 :idx=1");
  sss.push_back(
      "After pre-initialization(::main)0x709f7c633010<SgFunctionDefinition> "
      "@line=1, col=1 :idx=2");
  sss.push_back(
      "0x709f7c6cc1680x709f7c6cc168<SgBasicBlock> @line=1, col=12 :idx=0");
  sss.push_back("_variable_declaration_m0x709f7cf3f710<SgVariableDeclaration> "
                "@line=2, col=5 :idx=0");
  sss.push_back("initialized_name_m0x709f7ce4bf90<SgInitializedName> m :idx=0");
  sss.push_back("_variable_declaration_m0x709f7cf3f710<SgVariableDeclaration> "
                "@line=2, col=5 :idx=1");
  sss.push_back(
      "0x709f7c6cc1680x709f7c6cc168<SgBasicBlock> @line=1, col=12 :idx=1");
  sss.push_back(
      "0x709f7c5960100x709f7c596010<SgForStatement> @line=3, col=5 :idx=0");
  sss.push_back("SgForInitStatement0x709f7c55f010<SgForInitStatement> @line=3, "
                "col=10 :idx=0");
  sss.push_back("_variable_declaration_i0x709f7cf3f9f0<SgVariableDeclaration> "
                "@line=3, col=10 :idx=0");
  sss.push_back("initialized_name_i0x709f7ce4c180<SgInitializedName> i :idx=0");
  sss.push_back("SgAssignInitializer_undef_name0x709f7c52c010<"
                "SgAssignInitializer> @line=3, col=18 :idx=0");
  sss.push_back(
      "integer_value_exp_00x709f7cbf7090<SgIntVal> @line=3, col=18 :idx=0");
  sss.push_back(
      "integer_value_exp_00x709f7cbf7090<SgIntVal> @line=3, col=18 :idx=1");
  sss.push_back("SgAssignInitializer_undef_name0x709f7c52c010<"
                "SgAssignInitializer> @line=3, col=18 :idx=1");
  sss.push_back("initialized_name_i0x709f7ce4c180<SgInitializedName> i :idx=1");
  sss.push_back("_variable_declaration_i0x709f7cf3f9f0<SgVariableDeclaration> "
                "@line=3, col=10 :idx=1");
  sss.push_back("SgForInitStatement0x709f7c55f010<SgForInitStatement> @line=3, "
                "col=10 :idx=1");
  sss.push_back(
      "0x709f7c5960100x709f7c596010<SgForStatement> @line=3, col=5 :idx=1");
  sss.push_back(
      "SgExprStatement0x709f7c483010<SgExprStatement> @line=3, col=21 :idx=0");
  sss.push_back("SgLessThanOp_undef_name0x709f7c4b2080<SgLessThanOp> @line=3, "
                "col=21 :idx=0");
  sss.push_back(
      "var_ref_of_i0x709f7c4e9098<SgVarRefExp> @line=3, col=21 :idx=0");
  sss.push_back("SgLessThanOp_undef_name0x709f7c4b2080<SgLessThanOp> @line=3, "
                "col=21 :idx=1");
  sss.push_back(
      "integer_value_exp_50x709f7cbf7190<SgIntVal> @line=3, col=25 :idx=0");
  sss.push_back(
      "integer_value_exp_50x709f7cbf7190<SgIntVal> @line=3, col=25 :idx=1");
  sss.push_back("SgLessThanOp_undef_name0x709f7c4b2080<SgLessThanOp> @line=3, "
                "col=21 :idx=2");
  sss.push_back(
      "SgExprStatement0x709f7c483010<SgExprStatement> @line=3, col=21 :idx=1");
  sss.push_back(
      "0x709f7c5960100x709f7c596010<SgForStatement> @line=3, col=5 :idx=2");
  sss.push_back(
      "0x709f7c6cc2c00x709f7c6cc2c0<SgBasicBlock> @line=3, col=33 :idx=0");
  sss.push_back(
      "0x709f7c5961500x709f7c596150<SgForStatement> @line=4, col=9 :idx=0");
  sss.push_back("SgForInitStatement0x709f7c55f080<SgForInitStatement> @line=4, "
                "col=14 :idx=0");
  sss.push_back("_variable_declaration_j0x709f7cf3fcd0<SgVariableDeclaration> "
                "@line=4, col=14 :idx=0");
  sss.push_back("initialized_name_j0x709f7ce4c370<SgInitializedName> j :idx=0");
  sss.push_back("SgAssignInitializer_undef_name0x709f7c52c078<"
                "SgAssignInitializer> @line=4, col=22 :idx=0");
  sss.push_back(
      "integer_value_exp_00x709f7cbf7210<SgIntVal> @line=4, col=22 :idx=0");
  sss.push_back(
      "integer_value_exp_00x709f7cbf7210<SgIntVal> @line=4, col=22 :idx=1");
  sss.push_back("SgAssignInitializer_undef_name0x709f7c52c078<"
                "SgAssignInitializer> @line=4, col=22 :idx=1");
  sss.push_back("initialized_name_j0x709f7ce4c370<SgInitializedName> j :idx=1");
  sss.push_back("_variable_declaration_j0x709f7cf3fcd0<SgVariableDeclaration> "
                "@line=4, col=14 :idx=1");
  sss.push_back("SgForInitStatement0x709f7c55f080<SgForInitStatement> @line=4, "
                "col=14 :idx=1");
  sss.push_back(
      "0x709f7c5961500x709f7c596150<SgForStatement> @line=4, col=9 :idx=1");
  sss.push_back(
      "SgExprStatement0x709f7c483070<SgExprStatement> @line=4, col=25 :idx=0");
  sss.push_back("SgLessThanOp_undef_name0x709f7c4b2160<SgLessThanOp> @line=4, "
                "col=25 :idx=0");
  sss.push_back(
      "var_ref_of_j0x709f7c4e9230<SgVarRefExp> @line=4, col=25 :idx=0");
  sss.push_back("SgLessThanOp_undef_name0x709f7c4b2160<SgLessThanOp> @line=4, "
                "col=25 :idx=1");
  sss.push_back(
      "integer_value_exp_50x709f7cbf7310<SgIntVal> @line=4, col=29 :idx=0");
  sss.push_back(
      "integer_value_exp_50x709f7cbf7310<SgIntVal> @line=4, col=29 :idx=1");
  sss.push_back("SgLessThanOp_undef_name0x709f7c4b2160<SgLessThanOp> @line=4, "
                "col=25 :idx=2");
  sss.push_back(
      "SgExprStatement0x709f7c483070<SgExprStatement> @line=4, col=25 :idx=1");
  sss.push_back(
      "0x709f7c5961500x709f7c596150<SgForStatement> @line=4, col=9 :idx=2");
  sss.push_back(
      "0x709f7c5961500x709f7c596150<SgForStatement> @line=4, col=9 :idx=4");
  sss.push_back(
      "0x709f7c6cc2c00x709f7c6cc2c0<SgBasicBlock> @line=3, col=33 :idx=1");
  sss.push_back(
      "0x709f7c5960100x709f7c596010<SgForStatement> @line=3, col=5 :idx=3");
  sss.push_back("SgPlusPlusOp_undef_name0x709f7c450010<SgPlusPlusOp> @line=3, "
                "col=28 :idx=0");
  sss.push_back(
      "var_ref_of_i0x709f7c4e9120<SgVarRefExp> @line=3, col=28 :idx=0");
  sss.push_back("SgPlusPlusOp_undef_name0x709f7c450010<SgPlusPlusOp> @line=3, "
                "col=28 :idx=1");
  sss.push_back(
      "0x709f7c5960100x709f7c596010<SgForStatement> @line=3, col=5 :idx=1");
  sss.push_back(
      "SgExprStatement0x709f7c483010<SgExprStatement> @line=3, col=21 :idx=0");
  sss.push_back("SgLessThanOp_undef_name0x709f7c4b2080<SgLessThanOp> @line=3, "
                "col=21 :idx=0");
  sss.push_back(
      "var_ref_of_i0x709f7c4e9098<SgVarRefExp> @line=3, col=21 :idx=0");
  sss.push_back("SgLessThanOp_undef_name0x709f7c4b2080<SgLessThanOp> @line=3, "
                "col=21 :idx=1");
  sss.push_back(
      "integer_value_exp_50x709f7cbf7190<SgIntVal> @line=3, col=25 :idx=0");
  sss.push_back(
      "integer_value_exp_50x709f7cbf7190<SgIntVal> @line=3, col=25 :idx=1");
  sss.push_back("SgLessThanOp_undef_name0x709f7c4b2080<SgLessThanOp> @line=3, "
                "col=21 :idx=2");
  sss.push_back(
      "SgExprStatement0x709f7c483010<SgExprStatement> @line=3, col=21 :idx=1");
  sss.push_back(
      "0x709f7c5960100x709f7c596010<SgForStatement> @line=3, col=5 :idx=2");
  sss.push_back(
      "0x709f7c5960100x709f7c596010<SgForStatement> @line=3, col=5 :idx=4");
  sss.push_back(
      "0x709f7c6cc1680x709f7c6cc168<SgBasicBlock> @line=1, col=12 :idx=2");
  sss.push_back(
      "SgReturnStmt0x709f7c421010<SgReturnStmt> @line=16, col=5 :idx=0");
  sss.push_back(
      "integer_value_exp_00x709f7cbf7690<SgIntVal> @line=16, col=12 :idx=0");
  sss.push_back(
      "integer_value_exp_00x709f7cbf7690<SgIntVal> @line=16, col=12 :idx=1");
  sss.push_back(
      "SgReturnStmt0x709f7c421010<SgReturnStmt> @line=16, col=5 :idx=1");
  sss.push_back(
      "End(::main)0x709f7c633010<SgFunctionDefinition> @line=1, col=1 :idx=3");
  sssv.insert(sss);
  sss.clear();
  vis->sssv = sssv;
  vis->constructPathAnalyzer(mg.get(), true, 0, 0, true);
  ROSE_ASSERT(vis->sssv.size() == vis->paths.size());
  std::cout << "finished" << std::endl;
  std::cout << " paths: " << vis->paths.size() << std::endl;
  delete vis;
}
