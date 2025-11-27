// A translator to call many SageInterface functions to improve code coverage reported by LCOV.
//
// Initially add analysis interface functions, 
//
// Later to add transformation interface functions
//  SageInterface::initializeSwitchStatement(SgSwitchStatement* switchStatement,SgStatement *item_selector,SgStatement *body)
//  SageInterface::appendStatement(SgStatement *stmt, SgForInitStatement* for_init_stmt)
//   insertStatementListBeforeFirstNonDeclaration()
//   moveToSubdirectory()
//   attachComment()
// by traversing memory pools
//
// Liao, 4/4/2017
#include "rose.h"
#include <iostream>
using namespace std;
using namespace SageInterface;

// used to cover SageInterface::DeclarationSets:: * 
SageInterface::DeclarationSets* decl_set = NULL; 

class RoseVisitor : public ROSE_VisitTraversal
{
  public:
    void visit ( SgNode* node);
};

void RoseVisitor::visit ( SgNode* node)
{
  if (SgDeclarationStatement* decl = isSgDeclarationStatement(node))
  {
    cout<<"calling enclosingNamespaceScope() "<<endl;
    enclosingNamespaceScope( decl);

    //TODO: not working SageInterface::DeclarationSets::getDeclarations ()    
    //      decl_set  = buildDeclarationSets(decl);
    //      decl_set->getDeclarations(decl);


    cout<<"calling SageInterface::generateUniqueNameForUseAsIdentifier_support ( SgDeclarationStatement* declaration ) "<<endl;
    cout<<generateUniqueNameForUseAsIdentifier_support (decl)<<endl;

    // TODO: this fails for some nodes, moved to function declarations 
    // cout<<"calling SageInterface::generateUniqueNameForUseAsIdentifier( SgDeclarationStatement* declaration ) "<<endl;
    // cout<<generateUniqueNameForUseAsIdentifier(decl)<<endl;

    // local function, commented out
    //generateUniqueDeclaration (decl);

  }

  if (SgVarRefExp* varref = isSgVarRefExp(node))
  {
    if (SgArrayType *atype = isSgArrayType (varref->get_type()) )
    {
      get_C_array_dimensions (*atype, *varref);
    }
  }

#if 0 //TODO: Assertion `init_stmt != __null' failed.
  if (SgForStatement* fs = isSgForStatement(node))
  {
    SgVariableSymbol* vs=NULL;
    SgExpression* lb = NULL; 
    SgExpression* up = NULL; 
    SgExpression* st= NULL; 
    getForLoopInformations (fs, vs, lb, up, st);
  }
#endif
  if (SgVariableDeclaration* var_decl = isSgVariableDeclaration(node))
  {
    getFirstVariable (*var_decl);
  }

  if (SgSwitchStatement* sw = isSgSwitchStatement(node))
  {
    cout<<"calling whereAmI() "<<endl;
    whereAmI (sw); // we don't want touch this func for all nodes

    cout<<"calling SageInterface::outputLocalSymbolTables ( SgNode* node ) "<<endl;
    outputLocalSymbolTables (sw);

    // calling some functions within a smaller narrow scope
    setOneSourcePositionForTransformation(sw);
    removeAllOriginalExpressionTrees (sw);
    // TODO: bugging function to fix    
    //    changeBreakStatementsToGotos(sw);
  }

#if 0
  if (SgWhileStmt* sw = isSgWhileStmt(node))
  {
    // Internal function, called by SageInterface::ensureBasicBlockAsBodyOfUpcForAll(SgUpcForAllStatement* fs) only.    
    //    ensureBasicBlock_aux (sw, &SgWhileStmt::get_body, &SgWhileStmt::set_body);
  }
#endif

  // TODO: not sure when SgToken show up in AST
  if (SgToken* stk = isSgToken(node) )
  {
    cout<<"calling SageInterface::get_name ( const SgToken* token ) "<<endl;
    cout<<get_name (stk)<<endl;
  }

  if (isSgLocatedNode(node))
  {
    // cover string getVariantName ( VariantT v )
    // the test harness in Makefile.am will redirect output to rose_inputinterfaceFunctionCoverage.C.passed
    cout<<"calling getVariantName() "<<endl;
    cout<<getVariantName (node->variantT())<<endl;
  }


  if (SgTemplateInstantiationMemberFunctionDecl * temp_decl = isSgTemplateInstantiationMemberFunctionDecl (node))
  {
    cout<<"calling functions operating on SgTemplateInstantiationMemberFunctionDecl()"<<endl;
    //TODO: this function triggers assertion failure.
    // getNonInstantiatonDeclarationForClass (temp_decl);

    templateDefinitionIsInClass (temp_decl);

    // TODO: this function causes assertion failure
    // SgTemplateInstantiationMemberFunctionDecl* copy = buildForwardFunctionDeclaration (temp_decl);
    // prependStatement(copy, temp_decl->get_scope());
  }

  if (SgFunctionDeclaration* func = isSgFunctionDeclaration (node))
  {
    SgScopeStatement* scope = func->get_scope();
    if (scope->containsOnlyDeclarations() == true)
    {
      cout<<"calling isPrototypeInScope() "<<endl;
      isPrototypeInScope (scope, func, func);
    }

    cout<<"calling SageInterface::generateUniqueNameForUseAsIdentifier( SgDeclarationStatement* declaration ) "<<endl;
    cout<<generateUniqueNameForUseAsIdentifier(func)<<endl;

    SgFunctionDeclaration* nondef_decl = isSgFunctionDeclaration(func->get_firstNondefiningDeclaration ());
    // This is a defining declaration
    if (nondef_decl != NULL && nondef_decl != func)
    {
      if (declarationPreceedsDefinition (nondef_decl, func))
        cout<<"calling declarationPreceedsDefinition() returns true."<<endl;

    }

    dumpInfo(func);
    std::set<SgVariableSymbol*> readOnlySymbols; 
    //TODO: assertion failures
    //collectReadOnlySymbols (func, readOnlySymbols);

  }

  if (SgMemberFunctionDeclaration * memfunc = isSgMemberFunctionDeclaration (node))
  {
    // this function only accepts member functions
    if (isOverloaded (memfunc))
      cout<<"calling isOverloaded() returns true"<<endl;
  }
  //TODO: this never should work since we don't use SgC_PreprocessorDirectiveStatement now. 
  if (SgC_PreprocessorDirectiveStatement * pdecl= isSgC_PreprocessorDirectiveStatement(node))
  {
    cout<<"calling get_name(SgC_PreprocessorDirectiveStatement*) "<<endl;
    cout<< get_name(pdecl)<<endl;    
  }

  if (SgExpression* exp = isSgExpression(node))
  {
    cout<<"calling SageInterface::get_name ( const SgType* type ) "<<endl;
    cout<<get_name(exp->get_type()); // we don't want touch this func for all nodes

    getDeclarationOfNamedFunction(exp); 

    //TODO: assert failure for this function
    //if (getInitializerOfExpression (exp)!= NULL)
    //   cout<<"calling SageInterface::getInitializerOfExpression(SgExpression* n) returns something "<<endl;

    if (SgIntVal* iv = isSgIntVal(exp))
    {
      if(isEqualToIntConst (iv, 12345))
      {
        cout<<"calling  SageInterface::isEqualToIntConst(SgExpression* e, int value) return true, IntVal == 12345 "<<endl;
        setOperand (isSgExpression(iv->get_parent()), SageBuilder::buildIntVal(9));
      }
    }

    isConstantTrue (exp);
    isConstantFalse (exp);

    isPostfixOperator (exp);
    isIndexOperator (exp);
#if 0
    if (SgBinaryOp* bop = isSgBinaryOp (exp))
    {
      //TODO: assertion failure
      splitExpressionIntoBasicBlock(bop);
    }
#endif
    if (isSgCharVal(exp) ||
        isSgUnsignedCharVal(exp) ||
        isSgShortVal(exp) ||
        isSgUnsignedShortVal(exp) ||
        isSgUnsignedIntVal(exp) ||
        isSgLongIntVal(exp) ||
        isSgUnsignedLongVal(exp) ||
        isSgLongLongIntVal(exp) ||
        isSgUnsignedLongLongIntVal(exp)
       )
    {
      getIntegerConstantValue (isSgValueExp(exp));
      evaluateConstIntegerExpression (exp);
    }

  }


  if (SgFunctionCallExp* node2= isSgFunctionCallExp(node))
  {
    // TODO: this function has many assertions for unsupported cases. 
    // functionCallExpressionPreceedsDeclarationWhichAssociatesScope(node2);
    isCallToParticularFunction ("test_splitVariableDeclaration",0, node2);

    if (SgFunctionSymbol* fsym = node2->getAssociatedFunctionSymbol ())
    {
      if ((fsym->get_name().getString()) =="test_splitVariableDeclaration" ) 
      {
        SgFunctionDeclaration* fdecl = findFunctionDeclaration (getGlobalScope(node2),"test_splitVariableDeclaration", NULL, true);
        isCallToParticularFunction (fdecl, node2);
      }
    }
  }

  // Extracted from projects/SMTPathFeasibility/utils/replaceExpressionsAndSimplifyExpressions.cpp
  if (SgIfStmt* fixIf= isSgIfStmt(node))
  {
    SgStatement* conditional = fixIf->get_conditional();
    if (isSgExprStatement(conditional)) {
      SgExpression* expr = isSgExprStatement(conditional)->get_expression();
      std::pair<SgVariableDeclaration*, SgExpression*> pr = SageInterface::createTempVariableForExpression(expr,isSgScopeStatement(fixIf),true);
      SgInitializedNamePtrList lptr = pr.first->get_variables();
      //std::cout << "lprt size: " << lptr.size() << std::endl;
      ROSE_ASSERT(lptr.size() <= 1);
      SgVarRefExp* varRef = SageBuilder::buildVarRefExp(pr.first);
      SgIntVal* iv = SageBuilder::buildIntVal(0);
      SgNotEqualOp* nop = SageBuilder::buildNotEqualOp(isSgExpression(varRef),isSgExpression(iv));
      SgExprStatement* ses = SageBuilder::buildExprStatement(isSgExpression(nop));
      SageInterface::replaceStatement(conditional,ses);

      SageInterface::insertStatementBefore(fixIf,pr.first); 
    }
  }

  // scan various input functions to trigger testing
  if (SgFunctionDefinition* node2= isSgFunctionDefinition(node))
  {
    clearScopeNumbers (node2);

    removeConsecutiveLabels (node2);

    removeLabeledGotos (node2);

    splitVariableDeclaration (node2);

    Rose_STL_Container<SgNode*>  currentVarRefList; 
    collectVariableReferencesInArrayTypes (node2, currentVarRefList);
  }

  if (SgScopeStatement* node2= isSgScopeStatement(node))
  {
    //TODO: this function as error 
    // calling SageInterface::getEnclosingScope(SgNode* n, const bool includingSelf/* =false*/)
    //getEnclosingScope (node2);

    hasSimpleChildrenList(node2);
  }

}

class DebugVisitor : public ROSE_VisitTraversal {
public:
    void visit(SgNode* node) override {
        if (SgFunctionDeclaration* decl = isSgFunctionDeclaration(node)) {
            if (decl->get_name() == "test_template_template") {
                std::cout << "Found SgFunctionDeclaration: " << decl << std::endl;
                printInfo(decl);
                // Force unparsing to check output
                decl->get_file_info()->setOutputInCodeGeneration(); // Force output
                SgUnparse_Info info;
                info.set_SkipComments();
                // info.set_SkipPreprocessorInfo(); // Not available
                std::cout << "Unparsed: " << globalUnparseToString(decl, &info) << std::endl;
                // printInfo(decl);
            }
        }
    }

    void printInfo(SgDeclarationStatement* decl) {
        std::cout << "  isCompilerGenerated: " << decl->get_file_info()->isCompilerGenerated() << std::endl;
        std::cout << "  isOutputInCodeGeneration: " << decl->get_file_info()->isOutputInCodeGeneration() << std::endl;
        std::cout << "  isTransformation: " << decl->get_file_info()->isTransformation() << std::endl;
        std::cout << "  filename: " << decl->get_file_info()->get_filename() << std::endl;
        std::cout << "  parent: " << decl->get_parent()->class_name() << std::endl;
    }
};

int main(int argc, char** argv) {
    SgProject* project = frontend(argc, argv);
    DebugVisitor v;
    SgFunctionDeclaration::traverseMemoryPoolNodes(v);
    return backend(project);
}

