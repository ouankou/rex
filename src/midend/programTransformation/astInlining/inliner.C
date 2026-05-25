
// tps (01/14/2010) : Switching from rose.h to sage3.
#include "sage3basic.h"

#include "sageBuilder.h"

#include <iomanip>

#include <iostream>

#include <set>

#include <string>
// PP(14/10/20) PRE->legacy::PRE #include "pre.h"

#include "AstConsistencyTests.h"

#include "RoseAst.h" // using AST Iterator
#include "rose_config.h"

// DQ (8/1/2005): test use of new static function to create
// Sg_File_Info object that are marked as transformations
#undef SgNULL_FILE
#define SgNULL_FILE Sg_File_Info::generateDefaultFileInfoForTransformationNode()

#include "inliner.h"

#include "inlinerSupport.h"

#include "replaceExpressionWithStatement.h"

using namespace std;
using namespace Rose;
using namespace SageInterface;
// void FixSgTree(SgNode*);
// void FixSgProject(SgProject&);

// a namespace
namespace Inliner {
bool skipHeaders = false;
bool verbose = false; // if set to true, generate debugging information
} // namespace Inliner

namespace {
SgExpression *lambdaCaptureSourceExpression(const SgLambdaCapture *capture) {
  if (capture == NULL) {
    return NULL;
  }

  if (SgExpression *source = capture->get_source_closure_variable()) {
    return source;
  }

  return capture->get_capture_variable();
}

SgLambdaExp *
lambdaExpressionForClassDeclaration(SgClassDeclaration *classDecl) {
  if (classDecl == NULL) {
    return NULL;
  }

  SgClassDeclaration *candidates[] = {
      classDecl,
      isSgClassDeclaration(classDecl->get_firstNondefiningDeclaration()),
      isSgClassDeclaration(classDecl->get_definingDeclaration())};

  for (SgClassDeclaration *candidate : candidates) {
    if (candidate == NULL) {
      continue;
    }

    if (SgLambdaExp *lambdaExp = isSgLambdaExp(candidate->get_parent())) {
      return lambdaExp;
    }
  }

  return NULL;
}

SgType *buildAutoShadowTypeForFormal(SgType *formalType) {
  SgAutoType *autoType = SageBuilder::buildAutoType();
  SgType *referencedType = NULL;
  bool useRvalueReference = false;

  if (SgReferenceType *referenceType = isSgReferenceType(formalType)) {
    referencedType = referenceType->get_base_type();
  } else if (SgRvalueReferenceType *rvalueReferenceType =
                 isSgRvalueReferenceType(formalType)) {
    referencedType = rvalueReferenceType->get_base_type();
    useRvalueReference = true;
  }

  if (referencedType == NULL) {
    return autoType;
  }

  SgType *autoReferentType = autoType;
  if (SageInterface::isConstType(referencedType)) {
    autoReferentType = SageBuilder::buildConstType(autoReferentType);
  }
  if (SageInterface::isVolatileType(referencedType)) {
    autoReferentType = SageBuilder::buildVolatileType(autoReferentType);
  }

  return useRvalueReference
             ? static_cast<SgType *>(
                   SageBuilder::buildRvalueReferenceType(autoReferentType))
             : static_cast<SgType *>(
                   SageBuilder::buildReferenceType(autoReferentType));
}

SgLambdaExp *
lambdaExpressionForFunctionDeclaration(SgFunctionDeclaration *functionDecl) {
  if (functionDecl == NULL) {
    return NULL;
  }

  SgFunctionDeclaration *candidates[] = {
      functionDecl,
      isSgFunctionDeclaration(functionDecl->get_firstNondefiningDeclaration()),
      isSgFunctionDeclaration(functionDecl->get_definingDeclaration())};
  for (SgFunctionDeclaration *candidate : candidates) {
    if (candidate == NULL) {
      continue;
    }

    if (SgLambdaExp *lambdaExp = isSgLambdaExp(candidate->get_parent())) {
      return lambdaExp;
    }
  }

  if (SgMemberFunctionDeclaration *memberDecl =
          isSgMemberFunctionDeclaration(functionDecl)) {
    if (SgClassDeclaration *associatedClass = isSgClassDeclaration(
            memberDecl->get_associatedClassDeclaration())) {
      if (SgLambdaExp *lambdaExp =
              lambdaExpressionForClassDeclaration(associatedClass)) {
        return lambdaExp;
      }
    }

    if (SgClassDefinition *classDef =
            isSgClassDefinition(memberDecl->get_parent())) {
      if (SgLambdaExp *lambdaExp = lambdaExpressionForClassDeclaration(
              classDef->get_declaration())) {
        return lambdaExp;
      }
    }

    if (SgMemberFunctionType *memberType =
            isSgMemberFunctionType(memberDecl->get_type())) {
      if (SgClassType *classType =
              isSgClassType(memberType->get_class_type())) {
        if (SgLambdaExp *lambdaExp = lambdaExpressionForClassDeclaration(
                isSgClassDeclaration(classType->get_declaration()))) {
          return lambdaExp;
        }
      }
    }
  }

  return NULL;
}

SgClassDefinition *classDefinitionForDeclaration(SgClassDeclaration *decl) {
  if (decl == NULL) {
    return NULL;
  }

  SgClassDeclaration *definingDecl =
      isSgClassDeclaration(decl->get_definingDeclaration());
  if (definingDecl != NULL && definingDecl->get_definition() != NULL) {
    return definingDecl->get_definition();
  }

  return decl->get_definition();
}

SgClassDefinition *
classDefinitionForMemberFunction(SgMemberFunctionDeclaration *memberDecl) {
  if (memberDecl == NULL) {
    return NULL;
  }

  if (SgClassDeclaration *classDecl =
          isSgClassDeclaration(memberDecl->get_associatedClassDeclaration())) {
    if (SgClassDefinition *classDef =
            classDefinitionForDeclaration(classDecl)) {
      return classDef;
    }
  }

  if (SgMemberFunctionType *memberType =
          isSgMemberFunctionType(memberDecl->get_type())) {
    if (SgClassType *classType = isSgClassType(memberType->get_class_type())) {
      if (SgClassDeclaration *classDecl =
              isSgClassDeclaration(classType->get_declaration())) {
        if (SgClassDefinition *classDef =
                classDefinitionForDeclaration(classDecl)) {
          return classDef;
        }
      }
    }
  }

  return SageInterface::getEnclosingClassDefinition(memberDecl, true);
}

SgClassDefinition *
classDefinitionForDeclarationContext(SgDeclarationStatement *decl) {
  if (decl == NULL) {
    return NULL;
  }

  if (SgInitializedName *initializedName = isSgInitializedName(decl)) {
    return classDefinitionForDeclarationContext(
        initializedName->get_declaration());
  }

  if (SgClassDefinition *scopeClass = isSgClassDefinition(decl->get_scope())) {
    return scopeClass;
  }

  return SageInterface::getEnclosingClassDefinition(decl, false);
}

bool sameClassDefinition(SgClassDefinition *lhs, SgClassDefinition *rhs) {
  if (lhs == NULL || rhs == NULL) {
    return false;
  }

  SgClassDeclaration *lhsDecl = lhs->get_declaration();
  SgClassDeclaration *rhsDecl = rhs->get_declaration();
  if (lhsDecl != NULL && rhsDecl != NULL) {
    lhsDecl = isSgClassDeclaration(lhsDecl->get_definingDeclaration());
    rhsDecl = isSgClassDeclaration(rhsDecl->get_definingDeclaration());
    if (lhsDecl != NULL && rhsDecl != NULL) {
      return lhsDecl == rhsDecl;
    }
  }

  return lhs == rhs;
}

bool classDerivesFrom(SgClassDefinition *derivedClass,
                      SgClassDefinition *baseClass,
                      std::set<SgClassDefinition *> &visitedClasses) {
  if (derivedClass == NULL || baseClass == NULL ||
      visitedClasses.insert(derivedClass).second == false) {
    return false;
  }

  for (SgBaseClass *baseSpecifier : derivedClass->get_inheritances()) {
    if (baseSpecifier == NULL) {
      continue;
    }

    SgClassDeclaration *baseDecl =
        isSgClassDeclaration(baseSpecifier->get_base_class());
    if (baseDecl == NULL) {
      continue;
    }

    SgClassDefinition *directBaseClass =
        classDefinitionForDeclaration(baseDecl);
    if (sameClassDefinition(directBaseClass, baseClass) ||
        classDerivesFrom(directBaseClass, baseClass, visitedClasses)) {
      return true;
    }
  }

  return false;
}

bool classDerivesFrom(SgClassDefinition *derivedClass,
                      SgClassDefinition *baseClass) {
  std::set<SgClassDefinition *> visitedClasses;
  return classDerivesFrom(derivedClass, baseClass, visitedClasses);
}

bool accessModifierRequiresPrivilegedContext(
    const SgAccessModifier &accessModifier) {
  return accessModifier.isPrivate() || accessModifier.isProtected();
}

SgClassDefinition *
nonPublicClassOwningDeclaration(SgDeclarationStatement *decl) {
  if (decl == NULL) {
    return NULL;
  }

  if (!accessModifierRequiresPrivilegedContext(
          decl->get_declarationModifier().get_accessModifier())) {
    return NULL;
  }

  return classDefinitionForDeclarationContext(decl);
}

bool functionContextHasClassAccess(SgFunctionDeclaration *functionDecl,
                                   SgClassDefinition *classDef,
                                   bool allowDerivedAccess) {
  if (functionDecl == NULL || classDef == NULL) {
    return false;
  }

  SgClassDefinition *functionClassDef = NULL;
  if (SgMemberFunctionDeclaration *memberDecl =
          isSgMemberFunctionDeclaration(functionDecl)) {
    functionClassDef = classDefinitionForMemberFunction(memberDecl);
    if (sameClassDefinition(functionClassDef, classDef)) {
      return true;
    }
  }

  if (allowDerivedAccess && classDerivesFrom(functionClassDef, classDef)) {
    return true;
  }

  if (functionDecl->get_declarationModifier().isFriend()) {
    if (sameClassDefinition(
            SageInterface::getEnclosingClassDefinition(functionDecl, false),
            classDef)) {
      return true;
    }
  }

  if (SgFunctionDeclaration *nondef = isSgFunctionDeclaration(
          functionDecl->get_firstNondefiningDeclaration())) {
    if (nondef != functionDecl &&
        nondef->get_declarationModifier().isFriend()) {
      if (sameClassDefinition(
              SageInterface::getEnclosingClassDefinition(nondef, false),
              classDef)) {
        return true;
      }
    }
  }

  return false;
}

bool callSiteHasClassAccess(SgFunctionCallExp *funcall,
                            SgClassDefinition *classDef,
                            const SgAccessModifier &accessModifier) {
  SgFunctionDeclaration *targetDecl =
      SageInterface::getEnclosingFunctionDeclaration(funcall, false);
  if (targetDecl != NULL) {
    targetDecl = isSgFunctionDeclaration(targetDecl->get_definingDeclaration());
  }

  return functionContextHasClassAccess(targetDecl, classDef,
                                       accessModifier.isProtected());
}

bool functionBodyNeedsUnavailableClassAccess(SgFunctionDefinition *fundef,
                                             SgFunctionCallExp *funcall) {
  class NonPublicMemberAccessTraversal : public AstSimpleProcessing {
    SgFunctionCallExp *funcall;
    bool unavailableAccess = false;

  public:
    explicit NonPublicMemberAccessTraversal(SgFunctionCallExp *funcall)
        : funcall(funcall) {}

    void visit(SgNode *node) override {
      SgDeclarationStatement *decl = NULL;

      if (SgVarRefExp *varRef = isSgVarRefExp(node)) {
        if (SgVariableSymbol *symbol = varRef->get_symbol()) {
          if (SgInitializedName *initializedName = symbol->get_declaration()) {
            decl = initializedName->get_declaration();
          }
        }
      } else if (SgMemberFunctionRefExp *memberFunctionRef =
                     isSgMemberFunctionRefExp(node)) {
        if (SgMemberFunctionSymbol *symbol = memberFunctionRef->get_symbol()) {
          decl = symbol->get_declaration();
        }
      }

      SgClassDefinition *owningClass = nonPublicClassOwningDeclaration(decl);
      if (owningClass != NULL &&
          !callSiteHasClassAccess(
              funcall, owningClass,
              decl->get_declarationModifier().get_accessModifier())) {
        unavailableAccess = true;
      }
    }

    bool foundUnavailableAccess() const { return unavailableAccess; }
  };

  NonPublicMemberAccessTraversal traversal(funcall);
  traversal.traverse(fundef->get_body(), preorder);

  if (SgMemberFunctionDeclaration *memberDecl =
          isSgMemberFunctionDeclaration(fundef->get_declaration())) {
    if (memberDecl->get_specialFunctionModifier().isConstructor()) {
      if (SgCtorInitializerList *ctorInitializers =
              memberDecl->get_CtorInitializerList()) {
        traversal.traverse(ctorInitializers, preorder);
      }
    }
  }

  return traversal.foundUnavailableAccess();
}
} // namespace

SgExpression *generateAssignmentMaybe(SgExpression *lhs, SgExpression *rhs) {
  // If lhs is NULL, return rhs without doing an assignment
  // If lhs is not NULL, assign rhs to it
  // Used as a helper in inliner

  SgExpression *returnAssignmentOperator = NULL;
  if (lhs != NULL) {
    returnAssignmentOperator = new SgAssignOp(SgNULL_FILE, lhs, rhs);
    returnAssignmentOperator->set_endOfConstruct(SgNULL_FILE);
  } else
    returnAssignmentOperator = rhs;

  return returnAssignmentOperator;
}

// Change all return statements in a block of code to assignments to a
// variable and gotos to a given label.  Used internally by the inliner.
class ChangeReturnsToGotosVisitor : public AstSimpleProcessing {
private:
  SgLabelStatement *label;
  SgExpression *where_to_write_answer;
  bool returns_by_reference;

public:
  ChangeReturnsToGotosVisitor(SgLabelStatement *label,
                              SgExpression *where_to_write_answer,
                              bool returns_by_reference)
      : label(label), where_to_write_answer(where_to_write_answer),
        returns_by_reference(returns_by_reference) {}

  virtual void visit(SgNode *n) {
    SgReturnStmt *rs = isSgReturnStmt(n);
    if (rs) {
      // std::cout << "Converting return statement " << rs->unparseToString();
      // std::cout << " into possible assignment to " <<
      // where_to_write_answer->unparseToString(); std::cout << " and jump to "
      // << label->get_name().getString() << std::endl;
      SgExpression *return_expr = rs->get_expression();
      SgBasicBlock *block = SageBuilder::buildBasicBlock();
      // printf ("Building IR node #1: new SgBasicBlock = %p \n",block);
      if (return_expr) {
        SgExpression *result_expr = return_expr;
        if (returns_by_reference && where_to_write_answer != NULL &&
            SageInterface::isPointerType(where_to_write_answer->get_type())) {
          result_expr = SageBuilder::buildAddressOfOp(return_expr);
        }

        SgExpression *assignment =
            generateAssignmentMaybe(where_to_write_answer, result_expr);
        if (where_to_write_answer)
          where_to_write_answer->set_parent(assignment);
        if (result_expr != assignment)
          result_expr->set_parent(assignment);
        SgStatement *assign_stmt = SageBuilder::buildExprStatement(assignment);
        SageInterface::appendStatement(assign_stmt, block);
      }

      // block->get_statements().push_back(new SgGotoStatement(SgNULL_FILE,
      // label));
      SgGotoStatement *gotoStatement = new SgGotoStatement(SgNULL_FILE, label);
      gotoStatement->set_endOfConstruct(SgNULL_FILE);
      ROSE_ASSERT(n->get_parent() != NULL);
      SageInterface::appendStatement(gotoStatement, block);
      isSgStatement(n->get_parent())->replace_statement(rs, block);
      block->set_parent(n->get_parent());
      ROSE_ASSERT(gotoStatement->get_parent() != NULL);
    }
  }
};

// One curried step of the inlining process.  This class just rearranges
// the parameters and sends them on to ChangeReturnsToGotosVisitor.
class ChangeReturnsToGotosPrevisitor
    : public SageInterface::StatementGenerator {
  SgLabelStatement *end_of_inline_label;
  SgStatement *funbody_copy;
  bool returns_by_reference;

public:
  ChangeReturnsToGotosPrevisitor(SgLabelStatement *end, SgStatement *body,
                                 bool returns_by_reference)
      : end_of_inline_label(end), funbody_copy(body),
        returns_by_reference(returns_by_reference) {}

  virtual SgStatement *generate(SgExpression *where_to_write_answer) {
    ChangeReturnsToGotosVisitor(end_of_inline_label, where_to_write_answer,
                                returns_by_reference)
        .traverse(funbody_copy, postorder);
    return funbody_copy;
  }
};

// Pei-Hung (06/12/20) This will replace the closure symbols to the capture
// symbols
class ReplaceCaptureVariableVisitor : public AstSimpleProcessing {
public:
  // map < closureSymbol, captureSymbol>
  typedef std::map<SgVariableSymbol *, SgVariableSymbol *> captureVarMap;

private:
  const captureVarMap &varMap;

public:
  ReplaceCaptureVariableVisitor(const captureVarMap &varMap) : varMap(varMap) {}

  virtual void visit(SgNode *n) {
    if (isSgDotExp(n) || isSgArrowExp(n)) {
      SgBinaryOp *binaryOp = isSgBinaryOp(n);
      isSgExpression(n->get_parent())
          ->replace_expression(isSgExpression(n), binaryOp->get_rhs_operand());
    }
    if (isSgVarRefExp(n)) {
      SgVarRefExp *vr = isSgVarRefExp(n);
      SgVariableSymbol *sym = vr->get_symbol();
      captureVarMap::const_iterator iter = varMap.find(sym);
      if (iter == varMap.end())
        return; // This is not a parameter use
      // cout <<" replace closure symbol" << endl;
      vr->set_symbol(iter->second);
    }
  }
};

// This class replaces all uses of this to references to a specified
// variable.  Used as part of inlining non-static member functions.
class ReplaceThisWithRefVisitor : public AstSimpleProcessing {
  SgVariableSymbol *sym;

public:
  ReplaceThisWithRefVisitor(SgVariableSymbol *sym) : sym(sym) {}

  virtual void visit(SgNode *n) {
    if (isSgThisExp(n)) {
      SgVarRefExp *vr = new SgVarRefExp(SgNULL_FILE, sym);
      vr->set_endOfConstruct(SgNULL_FILE);
      isSgExpression(n->get_parent())
          ->replace_expression(isSgExpression(n), vr);
    }
  }
};

// This class replaces all variable references to point to new symbols
// based on a map.  It is used to replace references to the parameters
// of an inlined procedure with new variables.
class ReplaceParameterUseVisitor : public AstSimpleProcessing {
public:
  typedef std::map<SgInitializedName *, SgVariableSymbol *> paramMapType;

private:
  const paramMapType &paramMap;

public:
  // constructor accepts the formal-actual parameter mapping
  ReplaceParameterUseVisitor(const paramMapType &paramMap)
      : paramMap(paramMap) {}

  virtual void visit(SgNode *n) {
    SgVarRefExp *vr = isSgVarRefExp(n);
    if (!vr)
      return;
    SgInitializedName *in = vr->get_symbol()->get_declaration();
    paramMapType::const_iterator iter = paramMap.find(in);
    if (iter == paramMap.end())
      return; // This is not a parameter use
    vr->set_symbol(iter->second);
  }
};

// Convert a declaration such as "A x = A(1, 2);" into "A x(1, 2);".  This is
// always (IIRC) safe to do by C++ language rules, even if A has a nontrivial
// copy constructor and/or destructor.
// FIXME (bug in another part of ROSE) -- the output from this routine unparses
// as if no changes had occurred, even though the PDF shows the transformation
// correctly.
void removeRedundantCopyInConstruction(SgInitializedName *in) {
  SgAssignInitializer *ai = isSgAssignInitializer(in->get_initializer());
  ROSE_ASSERT(ai);
  SgInitializer *realInit = isSgInitializer(ai->get_operand());
  ROSE_ASSERT(realInit);
  ROSE_ASSERT(isSgConstructorInitializer(realInit));
  in->set_initializer(realInit);
  realInit->set_parent(in);
  // FIXME -- do we need to delete ai?
}

// Mark AST as being a transformation
static void markAsTransformation(SgNode *ast) {
  struct FixFileInfo : AstTopDownProcessing<bool> {
    static bool isSuppressedFrontendInfo(Sg_File_Info *info) {
      return info != NULL && info->isFrontendSpecific() &&
             !info->isOutputInCodeGeneration();
    }

    static bool isSuppressedFrontendNode(SgLocatedNode *node) {
      return node != NULL &&
             (isSuppressedFrontendInfo(node->get_file_info()) ||
              isSuppressedFrontendInfo(node->get_startOfConstruct()) ||
              isSuppressedFrontendInfo(node->get_endOfConstruct()));
    }

    bool evaluateInheritedAttribute(SgNode *node, bool suppressOutput) {
      if (suppressOutput) {
        return true;
      }

      if (SgLocatedNode *loc = isSgLocatedNode(node)) {
        if (isSuppressedFrontendNode(loc)) {
          return true;
        }

        // DQ (3/1/2015): This is now being caught in the DOT file generation,
        // so I think we need to use this better version. DQ (4/14/2014): This
        // should be a more complete version to set all of the Sg_File_Info
        // objects on a SgLocatedNode.
        if (loc->get_startOfConstruct()) {
          loc->get_startOfConstruct()->setTransformation();
          loc->get_startOfConstruct()->setOutputInCodeGeneration();
        }

        if (loc->get_endOfConstruct()) {
          loc->get_endOfConstruct()->setTransformation();
          loc->get_endOfConstruct()->setOutputInCodeGeneration();
        }

        if (SgExpression *exp = isSgExpression(loc)) {
          if (exp->get_operatorPosition()) {
            exp->get_operatorPosition()->setTransformation();
            exp->get_operatorPosition()->setOutputInCodeGeneration();
          }
        }
      }

      return false;
    }
  };
  FixFileInfo().traverse(ast, false);
}
// Main inliner code.  Accepts a function call as a parameter, and inlines
// only that single function call.  Returns true if it succeeded, and false
// otherwise.  The function call must be to a named function, static member
// function, or non-virtual non-static member function, and the function
// must be known (not through a function pointer or member function
// pointer).  Also, the body of the function must already be visible.
// Recursive procedures are handled properly (when allowRecursion is set), by
// inlining one copy of the procedure into itself.  Any other restrictions on
// what can be inlined are bugs in the inliner code.
bool doInline(SgFunctionCallExp *funcall, bool allowRecursion) {

  if (Inliner::verbose) {
    Sg_File_Info *info_start = funcall->get_startOfConstruct();
    size_t a_start = (size_t)info_start->get_line();
    cout << "Inside doInling() for a function call @ " << a_start << endl;
    // funcall->get_file_info()->display();
  }
  if (Inliner::skipHeaders) {
    // Liao 1/23/2018. we ignore function calls within header files, which are
    // not unparsed by ROSE.
    string filename = funcall->get_file_info()->get_filename();
    string suffix = StringUtility ::fileNameSuffix(filename);
    // vector.tcc: This is an internal header file, included by other library
    // headers
    if (suffix == "h" || suffix == "hpp" || suffix == "hh" || suffix == "H" ||
        suffix == "hxx" || suffix == "h++" || suffix == "tcc")
      return false;

    // also check if it is compiler generated, mostly template instantiations.
    // They are not from user code.
    if (funcall->get_file_info()->isCompilerGenerated())
      return false;
    // check if the file is within compiler header staging directories
    if (insideSystemHeader(funcall))
      return false;
  }

  // Handle member function calls like a.foo() or aptr->foo()
  // Walk to its right-hand side to get the member function reference
  // expression.
  SgExpression *funname = funcall->get_function();
  SgExpression *func_ref_exp = isSgFunctionRefExp(funname);
  SgDotExp *dotexp = isSgDotExp(funname);
  SgArrowExp *arrowexp = isSgArrowExp(funname);
  SgExpression *thisptr = 0;
  if (dotexp || arrowexp) {
    func_ref_exp = isSgBinaryOp(funname)->get_rhs_operand();
    if (dotexp) {
      SgExpression *lhs = dotexp->get_lhs_operand();

      // Skip operator overloading functions for now.
      SgExpression *rhs = dotexp->get_rhs_operand();
      // TODO: refactored into SageInterface: isOperatorOverloading()
      SgMemberFunctionRefExp *m_ref_exp = isSgMemberFunctionRefExp(rhs);
      if (m_ref_exp) {
        SgMemberFunctionDeclaration *m_func_decl =
            m_ref_exp->get_symbol()->get_declaration();
        if (m_func_decl) {
          string q_name = m_func_decl->get_qualified_name().getString();
          string::size_type pos = q_name.find("::operator", 0);
          const bool isLambdaOperator =
              lambdaExpressionForFunctionDeclaration(m_func_decl) != NULL;

          if (pos != string::npos && isLambdaOperator == false) {
            //                  if (Inliner::verbose)
            std::cout << "Inline returns false: skip non-anonymous operator "
                         "function named:"
                      << q_name << std::endl;
            return false;
          }
        }
      }

      // FIXME -- patch this into p_lvalue
      bool is_lvalue = lhs->get_lvalue();
      if (isSgInitializer(lhs))
        is_lvalue = false;

      if (!is_lvalue) {
        SgAssignInitializer *ai = SageInterface::splitExpression(lhs);
        // ROSE_ASSERT (isSgInitializer(ai->get_operand())); // it can be
        // SgVarRefExp
        SgInitializedName *in = isSgInitializedName(ai->get_parent());
        in->set_auto_decltype(SageBuilder::buildAutoType());
        ROSE_ASSERT(in);
        if (isSgInitializer(ai->get_operand()))
          removeRedundantCopyInConstruction(in);
        lhs = dotexp->get_lhs_operand(); // Should be a var ref now
      }
      thisptr = new SgAddressOfOp(SgNULL_FILE, deepCopy(lhs));
    } else if (arrowexp) {
      thisptr = arrowexp->get_lhs_operand();
    } else {
      ROSE_ABORT();
    }
  }

  if (!func_ref_exp) {
    if (Inliner::verbose) {
      std::cout << "Inline returns false: not a call to a named function for "
                   "SgFunctionCallExp*"
                << funcall << std::endl;
    }
    return false; // Probably a call through a fun ptr
  }

  SgFunctionSymbol *funsym = 0;
  if (isSgFunctionRefExp(func_ref_exp))
    funsym = isSgFunctionRefExp(func_ref_exp)->get_symbol();
  else if (isSgMemberFunctionRefExp(func_ref_exp))
    funsym = isSgMemberFunctionRefExp(func_ref_exp)->get_symbol();
  else // template member function is not supported yet
  {
    cerr << "doInline() unhandled function reference type:"
         << func_ref_exp->class_name() << endl;
    // assert (false);
    return false;
  }

  assert(funsym);
  if (isSgMemberFunctionSymbol(funsym) && isSgMemberFunctionSymbol(funsym)
                                              ->get_declaration()
                                              ->get_functionModifier()
                                              .isVirtual()) {
    if (Inliner::verbose)
      std::cout
          << "Inline returns false: cannot inline virtual member functions"
          << std::endl;
    return false;
  }

  SgFunctionDeclaration *fundecl = funsym->get_declaration();
  fundecl = fundecl
                ? isSgFunctionDeclaration(fundecl->get_definingDeclaration())
                : NULL;
  // check the qualified name of the function to be inlined: skip std::xx
  // functions
  if (fundecl) {
    string q_name = fundecl->get_qualified_name().getString();
    string::size_type pos = q_name.find("::std::", 0);
    if (pos == 0) {
      if (Inliner::verbose)
        std::cout << "Inline returns false: skip std function named:" << q_name
                  << std::endl;
      return false;
    }
  }

  SgFunctionDefinition *fundef = fundecl ? fundecl->get_definition() : NULL;
  if (!fundef) {
    if (Inliner::verbose)
      std::cout << "Inline returns false: no function definition is visible"
                << std::endl;
    return false; // No definition of the function is visible
  }

  if (functionBodyNeedsUnavailableClassAccess(fundef, funcall)) {
    if (Inliner::verbose)
      std::cout << "Inline returns false: inlined body would require "
                   "non-public class access not available at the call site"
                << std::endl;
    return false;
  }

  // check for direct recursion call
  // TODO: handle indirect recursive calls: funcA-> funcB , funcB->funcA
  // Need to build a call graph to answer this question.
  if (!allowRecursion) {
    SgNode *my_fundef = funcall;
    // find enclosing function definition of the call site
    while (my_fundef && !isSgFunctionDefinition(my_fundef)) {
      // printf ("Before reset: my_fundef = %p = %s
      // \n",my_fundef,my_fundef->class_name().c_str());
      my_fundef = my_fundef->get_parent();
      ROSE_ASSERT(my_fundef != NULL);
      // printf ("After reset: my_fundef = %p = %s
      // \n",my_fundef,my_fundef->class_name().c_str());
    }
    // printf ("After reset: my_fundef = %p = %s
    // \n",my_fundef,my_fundef->class_name().c_str());
    assert(isSgFunctionDefinition(my_fundef));
    if (isSgFunctionDefinition(my_fundef) == fundef) {
      if (Inliner::verbose)
        std::cout << "Inline failed: trying to inline a procedure into itself"
                  << std::endl;
      return false;
    }
  }

  SgVariableDeclaration *thisdecl = 0;
  SgName thisname("this__");
  thisname << ++gensym_counter;
  SgInitializedName *thisinitname = 0;
  // Pei-Hung (06/12/20) Need to check if this is a lambda function call
  bool isLambdaMemberFuncCall = false;
  ReplaceCaptureVariableVisitor::captureVarMap varMap;
  // create a new variable declaration for member function call :
  //   TYPE*  this__ =  thisPtr; ??
  // static member functions cannot access this->data (non-static data). That is
  // why we check non-static for thisptr case.
  if (isSgMemberFunctionSymbol(funsym) &&
      !fundecl->get_declarationModifier().get_storageModifier().isStatic()) {
    assert(thisptr != NULL);
    SgType *thisptrtype = thisptr->get_type();
    const SgSpecialFunctionModifier &specialMod =
        funsym->get_declaration()->get_specialFunctionModifier();
    SgFunctionType *ft = funsym->get_declaration()->get_type();
    ROSE_ASSERT(ft);
    SgMemberFunctionType *mft = isSgMemberFunctionType(ft);
    ROSE_ASSERT(mft);
    SgType *ct = mft->get_class_type();
    if (specialMod.isConstructor()) {
      thisptrtype = new SgPointerType(ct);
    }
    // Pei-Hung (06/12/20) check if the parent of SgClassDeclaration is a
    // SgLambdaExp
    SgClassDeclaration *classDecl =
        isSgClassDeclaration(isSgClassType(ct)->get_declaration());
    ROSE_ASSERT(classDecl);
    if (SgLambdaExp *lambdaExp =
            lambdaExpressionForClassDeclaration(classDecl)) {
      // Pei-Hung (06/12/20) If this is a lambda function call, we try to skip
      // the class declaration.
      isLambdaMemberFuncCall = true;
      // cout << "There is a lambda class" << endl;
      SgLambdaCaptureList *lambdaCaptureList =
          lambdaExp->get_lambda_capture_list();
      SgLambdaCapturePtrList captureList =
          lambdaCaptureList->get_capture_list();
      for (SgLambdaCapture *capture : captureList) {
        // get the capture variable
        SgVarRefExp *captureVarRef =
            isSgVarRefExp(lambdaCaptureSourceExpression(capture));
        if (captureVarRef == NULL) {
          continue;
        }
        SgVariableSymbol *captureVarSym = captureVarRef->get_symbol();
        // get the closure variable
        SgVarRefExp *closureVarRef =
            isSgVarRefExp(capture->get_closure_variable());
        if (closureVarRef == NULL) {
          continue;
        }
        SgVariableSymbol *closureVarSym = closureVarRef->get_symbol();
        // Mapping closure and capture.
        varMap[closureVarSym] = captureVarSym;
      }
    } else {
      SgConstVolatileModifier &thiscv = fundecl->get_declarationModifier()
                                            .get_typeModifier()
                                            .get_constVolatileModifier();
      // if (thiscv.isConst() || thiscv.isVolatile()) { FIXME
      thisptrtype = new SgModifierType(thisptrtype);
      isSgModifierType(thisptrtype)
          ->get_typeModifier()
          .get_constVolatileModifier() = thiscv;
      // }
      // cout << thisptrtype->unparseToString() << " --- " << thiscv.isConst()
      // << " " << thiscv.isVolatile() << endl;
      SgAssignInitializer *assignInitializer =
          new SgAssignInitializer(SgNULL_FILE, thisptr);
      assignInitializer->set_endOfConstruct(SgNULL_FILE);
      thisdecl = new SgVariableDeclaration(SgNULL_FILE, thisname, thisptrtype,
                                           assignInitializer);
      thisdecl->set_endOfConstruct(SgNULL_FILE);
      thisdecl->get_definition()->set_endOfConstruct(SgNULL_FILE);
      thisdecl->set_definingDeclaration(thisdecl);

      thisinitname = (thisdecl->get_variables()).back();
      // thisinitname = lastElementOfContainer(thisdecl->get_variables());
      //  thisinitname->set_endOfConstruct(SgNULL_FILE);
      assignInitializer->set_parent(thisinitname);
      markAsTransformation(assignInitializer);

      // printf ("Built new SgVariableDeclaration #1 = %p \n",thisdecl);

      // DQ (6/23/2006): New test
      ROSE_ASSERT(assignInitializer->get_parent() != NULL);
    }
  }

  // Get the list of actual argument expressions from the function call, which
  // we'll later use to initialize new local variables in the inlined code.  We
  // need to detach the actual arguments from the AST here since we'll be
  // reattaching them below (otherwise we would violate the invariant that the
  // AST is a tree). PP (10/14/20) PRE -> legacy::PRE was: SgFunctionDefinition*
  // targetFunction = PRE::getFunctionDefinition(funcall);
  SgFunctionDefinition *targetFunction =
      SageInterface::getEnclosingFunctionDefinition(
          funcall, false /* do not include self */);
  SgExpressionPtrList funargs = funcall->get_args()->get_expressions();
  funcall->get_args()->get_expressions().clear();
  for (SgExpression *actual : funargs)
    actual->set_parent(NULL);

  // Make a copy of the to-be-inlined function so we're not modifying and
  // (re)inserting the original.
  SgBasicBlock *funbody_raw = fundef->get_body();
  SgInitializedNamePtrList &params = fundecl->get_args();
  std::vector<SgInitializedName *> inits;
  SgTreeCopy tc;
  SgFunctionDefinition *function_copy =
      isSgFunctionDefinition(fundef->copy(tc));

  // Pei-Hung (07/15/2020) the SgClassSymbol for the copied SgthisExp is
  // associated with original symbol table.  This should better be fixed in the
  // deep copy function.  This should serve as a tentative fix only.
  for (SgCopyHelp::copiedNodeMapTypeIterator iter =
           tc.get_copiedNodeMap().begin();
       iter != tc.get_copiedNodeMap().end(); iter++) {
    SgThisExp *thisexp_raw = isSgThisExp(const_cast<SgNode *>(iter->first));
    if (thisexp_raw != NULL) {
      SgThisExp *thisexp_copy = isSgThisExp(iter->second);
      SgClassSymbol *classsym_raw = thisexp_raw->get_class_symbol();
      SgClassSymbol *classsym_copy = thisexp_copy->get_class_symbol();
      // both SgClassSymbols point to the same symbol table
      if (classsym_raw->get_parent() == classsym_copy->get_parent()) {
        SgSymbolTable *symtable_raw =
            isSgSymbolTable(classsym_raw->get_parent());
        ROSE_ASSERT(symtable_raw);
        SgScopeStatement *parentscope =
            isSgScopeStatement(symtable_raw->get_parent());
        // Use the copy stack to look for the scope this symbol should stay
        if (tc.get_copiedNodeMap().find(parentscope) !=
            tc.get_copiedNodeMap().end()) {
          SgSymbolTable *newsymtable =
              isSgScopeStatement(
                  tc.get_copiedNodeMap().find(parentscope)->second)
                  ->get_symbol_table();
          classsym_copy->set_parent(newsymtable);
          newsymtable->insert(classsym_copy->get_name(), classsym_copy);
          // std::cout << symtable_raw << " scope :" <<
          // tc.get_copiedNodeMap().find(parentscope)->first << ":" <<
          // tc.get_copiedNodeMap().find(parentscope)->second << std::endl;
          // std::cout << "copy stack :" << iter->first<< ":" << iter->second <<
          // std::endl;
        }
      }
    }
  }

  ROSE_ASSERT(function_copy);
  SgBasicBlock *funbody_copy = function_copy->get_body();

  class ClearCopiedScopeSymbolTables : public AstSimpleProcessing {
  public:
    void visit(SgNode *node) override {
      if (SgScopeStatement *scope = isSgScopeStatement(node)) {
        if (SgSymbolTable *symbolTable = scope->get_symbol_table()) {
          symbolTable->get_table()->clear();
        }
      }
    }
  };

  ClearCopiedScopeSymbolTables().traverse(funbody_copy, preorder);
  funbody_raw->fixupCopy_symbols(funbody_copy, tc);

  auto repairScopesLeavingFunctionCopy = [funbody_copy, &tc](SgNode *root) {
    std::map<const SgDeclarationStatement *, SgDeclarationStatement *>
        copiedDeclarationByOriginal;
    std::map<SgDeclarationStatement *, const SgDeclarationStatement *>
        originalDeclarationByCopy;

    for (SgCopyHelp::copiedNodeMapTypeIterator it =
             tc.get_copiedNodeMap().begin();
         it != tc.get_copiedNodeMap().end(); ++it) {
      const SgDeclarationStatement *originalDecl =
          isSgDeclarationStatement(const_cast<SgNode *>(it->first));
      SgDeclarationStatement *copiedDecl = isSgDeclarationStatement(it->second);
      if (originalDecl == NULL || copiedDecl == NULL) {
        continue;
      }

      copiedDeclarationByOriginal[originalDecl] = copiedDecl;
      originalDeclarationByCopy[copiedDecl] = originalDecl;
    }

    class RepairCopiedScopes : public AstSimpleProcessing {
      SgBasicBlock *retainedBody;
      const std::map<const SgDeclarationStatement *, SgDeclarationStatement *>
          &copiedDeclarationByOriginal;
      const std::map<SgDeclarationStatement *, const SgDeclarationStatement *>
          &originalDeclarationByCopy;

      static bool nodeIsRetained(SgNode *node, SgBasicBlock *retainedBody) {
        for (SgNode *current = node; current != NULL;
             current = current->get_parent()) {
          if (current == retainedBody) {
            return true;
          }
        }

        return false;
      }

      static bool scopeIsRetained(SgScopeStatement *scope,
                                  SgBasicBlock *retainedBody) {
        for (SgNode *node = scope; node != NULL; node = node->get_parent()) {
          if (node == retainedBody) {
            return true;
          }
        }

        return false;
      }

      SgDeclarationStatement *
      findRetainedCopy(const SgDeclarationStatement *originalDecl) const {
        if (originalDecl == NULL) {
          return NULL;
        }

        std::map<const SgDeclarationStatement *,
                 SgDeclarationStatement *>::const_iterator found =
            copiedDeclarationByOriginal.find(originalDecl);
        if (found == copiedDeclarationByOriginal.end() ||
            nodeIsRetained(found->second, retainedBody) == false) {
          return NULL;
        }

        return found->second;
      }

      static SgScopeStatement *findStructuralScope(SgNode *node) {
        for (SgNode *parent = node != NULL ? node->get_parent() : NULL;
             parent != NULL; parent = parent->get_parent()) {
          if (SgScopeStatement *scope = isSgScopeStatement(parent)) {
            return scope;
          }
        }

        return NULL;
      }

      void repairDeclarationChain(SgDeclarationStatement *declaration) const {
        SgDeclarationStatement *currentDefining =
            declaration->get_definingDeclaration();
        SgDeclarationStatement *currentFirstNondefining =
            declaration->get_firstNondefiningDeclaration();
        const bool definingLeavesRetainedBody =
            currentDefining != NULL &&
            nodeIsRetained(currentDefining, retainedBody) == false;
        const bool firstLeavesRetainedBody =
            currentFirstNondefining != NULL &&
            nodeIsRetained(currentFirstNondefining, retainedBody) == false;

        if (!definingLeavesRetainedBody && !firstLeavesRetainedBody) {
          return;
        }

        SgDeclarationStatement *replacementDefining = NULL;
        SgDeclarationStatement *replacementFirstNondefining = NULL;

        std::map<SgDeclarationStatement *,
                 const SgDeclarationStatement *>::const_iterator original =
            originalDeclarationByCopy.find(declaration);
        if (original != originalDeclarationByCopy.end()) {
          const SgDeclarationStatement *originalDecl = original->second;
          const SgDeclarationStatement *originalDefining =
              originalDecl->get_definingDeclaration();
          const SgDeclarationStatement *originalFirstNondefining =
              originalDecl->get_firstNondefiningDeclaration();

          replacementDefining = findRetainedCopy(originalDefining);
          replacementFirstNondefining =
              findRetainedCopy(originalFirstNondefining);

          if (originalDefining == NULL) {
            replacementDefining = NULL;
          } else if (replacementDefining == NULL &&
                     originalDefining == originalDecl) {
            replacementDefining = declaration;
          }

          if (originalFirstNondefining == NULL) {
            replacementFirstNondefining = NULL;
          } else if (replacementFirstNondefining == NULL &&
                     originalFirstNondefining == originalDecl) {
            replacementFirstNondefining = declaration;
          }
        }

        if (definingLeavesRetainedBody && replacementDefining == NULL) {
          replacementDefining = declaration;
        }

        if (firstLeavesRetainedBody && replacementFirstNondefining == NULL) {
          replacementFirstNondefining =
              replacementDefining != NULL ? replacementDefining : declaration;
        }

        if (replacementDefining != NULL &&
            currentDefining != replacementDefining) {
          declaration->set_definingDeclaration(replacementDefining);
        }

        if (replacementFirstNondefining != NULL &&
            currentFirstNondefining != replacementFirstNondefining) {
          declaration->set_firstNondefiningDeclaration(
              replacementFirstNondefining);
        }
      }

    public:
      RepairCopiedScopes(
          SgBasicBlock *retainedBody,
          const std::map<const SgDeclarationStatement *,
                         SgDeclarationStatement *> &copiedDeclarationByOriginal,
          const std::map<SgDeclarationStatement *,
                         const SgDeclarationStatement *>
              &originalDeclarationByCopy)
          : retainedBody(retainedBody),
            copiedDeclarationByOriginal(copiedDeclarationByOriginal),
            originalDeclarationByCopy(originalDeclarationByCopy) {}

      void visit(SgNode *node) override {
        if (SgInitializedName *initializedName = isSgInitializedName(node)) {
          if (initializedName->get_scope() != NULL &&
              scopeIsRetained(initializedName->get_scope(), retainedBody) ==
                  false) {
            SgScopeStatement *replacementScope =
                findStructuralScope(initializedName);
            if (replacementScope == NULL) {
              replacementScope = retainedBody;
            }

            initializedName->set_scope(replacementScope);
          }
        }

        if (SgDeclarationStatement *declaration =
                isSgDeclarationStatement(node)) {
          repairDeclarationChain(declaration);

          const bool isLambdaClosureClass =
              lambdaExpressionForClassDeclaration(
                  isSgClassDeclaration(declaration)) != NULL;
          if (declaration->hasExplicitScope() == true &&
              ((declaration->get_scope() != NULL &&
                scopeIsRetained(declaration->get_scope(), retainedBody) ==
                    false) ||
               isLambdaClosureClass)) {
            SgScopeStatement *replacementScope =
                findStructuralScope(declaration);
            if (replacementScope == NULL) {
              replacementScope = retainedBody;
            }

            declaration->set_scope(replacementScope);
          }
        }
      }
    };

    RepairCopiedScopes repair(funbody_copy, copiedDeclarationByOriginal,
                              originalDeclarationByCopy);
    repair.traverse(root, preorder);
  };
  // rename labels in an inlined function definition. goto statements to them
  // will be updated.
  renameLabels(funbody_copy, targetFunction);

  // print more information in case the following assertion fails
  if (funbody_raw->get_symbol_table()->size() !=
      funbody_copy->get_symbol_table()->size()) {
    cerr << "funbody_raw symbol table size: "
         << funbody_raw->get_symbol_table()->size() << endl;
    cerr << "funbody_copy symbol table size: "
         << funbody_copy->get_symbol_table()->size() << endl;
    SgSymbolTable *rawSymTable = funbody_raw->get_symbol_table();
    std::set<SgNode *> rawSymbolList = rawSymTable->get_symbols();
    for (std::set<SgNode *>::iterator i = rawSymbolList.begin();
         i != rawSymbolList.end(); ++i) {
      SgSymbol *sym = isSgSymbol(*i);
      cout << " raw symbol name = " << sym->get_name() << endl;
    }
    SgSymbolTable *copySymTable = funbody_copy->get_symbol_table();
    std::set<SgNode *> copySymbolList = copySymTable->get_symbols();
    for (std::set<SgNode *>::iterator i = copySymbolList.begin();
         i != copySymbolList.end(); ++i) {
      SgSymbol *sym = isSgSymbol(*i);
      cout << " copy symbol name = " << sym->get_name() << endl;
    }
  }
  ASSERT_require(funbody_raw->get_symbol_table()->size() ==
                 funbody_copy->get_symbol_table()->size());

  // We don't need to keep the copied SgFunctionDefinition now that the labels
  // in it have been moved to the target function (having it in the memory pool
  // confuses the AST tests), but we must not delete the formal argument list or
  // the body because we need them below.
  if (function_copy->get_declaration()) {
    ASSERT_require(function_copy->get_declaration()->get_parent() ==
                   function_copy);
    function_copy->get_declaration()->set_parent(NULL);
    function_copy->set_declaration(NULL);
  }
  if (function_copy->get_body()) {
    ASSERT_require(function_copy->get_body()->get_parent() == function_copy);
    function_copy->get_body()->set_parent(NULL);
    function_copy->set_body(NULL);
  }
  repairScopesLeavingFunctionCopy(funbody_copy);
  delete function_copy;
  function_copy = NULL;
  funbody_copy->set_parent(SageInterface::getScope(funcall));

  // In the to-be-inserted function body, create new local variables with
  // distinct non-conflicting names, one per formal argument and having the same
  // type as the formal argument. Initialize those new local variables with the
  // actual arguments.  Also, build a paramMap that maps each formal argument
  // (SgInitializedName) to its corresponding new local variable
  // (SgVariableSymbol).
  ReplaceParameterUseVisitor::paramMapType paramMap;
  SgInitializedNamePtrList::iterator formalIter = params.begin();
  SgExpressionPtrList::iterator actualIter = funargs.begin();
  for (size_t argNumber = 0;
       formalIter != params.end() && actualIter != funargs.end();
       ++argNumber, ++formalIter, ++actualIter) {
    SgInitializedName *formalArg = *formalIter;
    SgExpression *actualArg = *actualIter;

    // Build the new local variable.
    // FIXME[Robb P. Matzke 2014-12-12]: we need a better way to generate a
    // non-conflicting local variable name
    // SgAssignInitializer* initializer = NULL;
    SgInitializer *initializer = NULL;
    // Pei-Hung (06/12/20): need to check if the argument is a class defined for
    // lambda
    SgClassDeclaration *classdecl = NULL;
    bool hasLambdaFuncArg = isSgLambdaExp(actualArg) != NULL;
    if (isSgClassType(formalArg->get_typeptr())) {
      SgClassType *classtype = isSgClassType(formalArg->get_typeptr());
      classdecl = isSgClassDeclaration(classtype->get_declaration());
      ROSE_ASSERT(classdecl);
      hasLambdaFuncArg = hasLambdaFuncArg ||
                         lambdaExpressionForClassDeclaration(classdecl) != NULL;
    }
    SgVariableDeclaration *vardecl = NULL;
    SgName shadow_name(formalArg->get_name());
    shadow_name << "__" << ++gensym_counter;
    int newStmtCount = 0;
    // Pei-Hung (06/12/20) this will create functor for the inlined code.
    // turn off this by default; turn it on for experimental usage
    bool retrieveFunctor = false;
    if (retrieveFunctor && hasLambdaFuncArg) {
      // cout << "new class name = " << shadow_name << endl;
      // Get lambda function, class declaration, and others
      SgLambdaExp *lambdaExp = isSgLambdaExp(classdecl->get_parent());
      // SgClassDeclaration* defingingclassdecl  =
      // isSgClassDeclaration(classdecl->get_definingDeclaration());
      SgMemberFunctionDeclaration *lambdaFunc =
          isSgMemberFunctionDeclaration(lambdaExp->get_lambda_function());
      SgLambdaCaptureList *lambdaCaptureList =
          lambdaExp->get_lambda_capture_list();
      SgLambdaCapturePtrList captureList =
          lambdaCaptureList->get_capture_list();

      // Create new copy of class
      SgMemberFunctionDeclaration *lambdaFuncDefCopy =
          isSgMemberFunctionDeclaration(SageInterface::deepCopy(lambdaFunc));
      // These should be replaced by buildClassDeclarationStatement_nfi if it
      // can be compiled properly.
      SgClassDeclaration *lambdaFuncClassCopy = new SgClassDeclaration(
          shadow_name, SgClassDeclaration::e_class, NULL, NULL);
      lambdaFuncClassCopy->set_firstNondefiningDeclaration(lambdaFuncClassCopy);
      lambdaFuncClassCopy->set_parent(funbody_copy);
      lambdaFuncClassCopy->set_scope(funbody_copy);
      (void)SgClassType::createType(lambdaFuncClassCopy);
      setOneSourcePositionForTransformation(lambdaFuncClassCopy);
      SgClassDefinition *lambdaClassDef = SageBuilder::buildClassDefinition();

      SgClassDeclaration *definingLambdaClassDecl = new SgClassDeclaration(
          shadow_name, SgClassDeclaration::e_class, NULL, lambdaClassDef);
      lambdaClassDef->set_declaration(definingLambdaClassDecl);
      definingLambdaClassDecl->set_parent(funbody_copy);
      definingLambdaClassDecl->set_scope(funbody_copy);
      lambdaFuncClassCopy->set_definingDeclaration(definingLambdaClassDecl);
      setOneSourcePositionForTransformation(definingLambdaClassDecl);
      definingLambdaClassDecl->set_definingDeclaration(definingLambdaClassDecl);
      definingLambdaClassDecl->set_firstNondefiningDeclaration(
          lambdaFuncClassCopy);
      definingLambdaClassDecl->set_type(lambdaFuncClassCopy->get_type());
      lambdaFuncClassCopy->setForward();
      // fixStructDeclaration(definingLambdaClassDecl,funbody_copy);

      // namae the class to be the variable name used for template function
      // argument
      lambdaFuncClassCopy->set_name(shadow_name);
      definingLambdaClassDecl->set_name(shadow_name);

      lambdaFuncClassCopy->set_explicit_anonymous(false);
      definingLambdaClassDecl->set_explicit_anonymous(false);

      lambdaFuncClassCopy->set_isAutonomousDeclaration(false);
      definingLambdaClassDecl->set_isAutonomousDeclaration(false);

      // cout << lambdaFuncClassCopy->get_name() << ":" << lambdaFuncClassCopy
      // << ":" << lambdaFuncClassCopy->get_explicit_anonymous() << ":" <<
      // lambdaFuncClassCopy->get_isAutonomousDeclaration() << endl; cout <<
      // lambdaFuncClassCopy->get_parent() << ":" << classdecl->get_parent()<<
      // endl; cout << lambdaFuncClassCopy->get_type() << ":" <<
      // classdecl->get_type()<< endl;

      // Insert the class definition to expose the class details.
      lambdaClassDef->append_member(lambdaFuncDefCopy);

      // adding capture list
      SgFunctionParameterList *captureParamList =
          SageBuilder::buildFunctionParameterList();
      SgCtorInitializerList *closureList =
          SageBuilder::buildCtorInitializerList_nfi();
      closureList->set_definingDeclaration(closureList);
      // prepare member functon parameter list for constructor initializer
      SgExprListExp *memberFuncArgList = SageBuilder::buildExprListExp_nfi();
      for (SgLambdaCapture *capture : captureList) {
        // capture list
        SgVarRefExp *captureVarRef =
            isSgVarRefExp(capture->get_capture_variable());
        ROSE_ASSERT(captureVarRef);
        SgVariableSymbol *captureVarSym = captureVarRef->get_symbol();
        SgName localVarName(captureVarSym->get_name());
        localVarName << "__" << ++gensym_counter;
        // cout << "capture list:"<< localVarName << endl;
        SgInitializedName *captureInitializedName =
            SageBuilder::buildInitializedName(localVarName,
                                              captureVarSym->get_type());
        captureParamList->append_arg(captureInitializedName);
        captureInitializedName->set_parent(captureParamList);
        captureInitializedName->set_scope(lambdaClassDef);

        // closure list
        SgVarRefExp *closureVarRef =
            isSgVarRefExp(capture->get_closure_variable());
        ROSE_ASSERT(closureVarRef);
        SgVariableSymbol *closureVarSym = closureVarRef->get_symbol();
        SgName closureNmae = closureVarSym->get_name();
        // cout << "closure list:"<< closureNmae << endl;
        //  build local private variable declaration for the closure variable
        SgVariableDeclaration *closureVarDel =
            SageBuilder::buildVariableDeclaration(
                closureNmae, closureVarSym->get_type(), NULL, lambdaClassDef);
        closureVarDel->get_declarationModifier()
            .get_accessModifier()
            .setPrivate();

        SgVarRefExp *closureAssignVarExp =
            SageBuilder::buildVarRefExp(captureInitializedName, funbody_copy);
        SgAssignInitializer *assignInitilizer =
            SageBuilder::buildAssignInitializer(closureAssignVarExp,
                                                closureVarSym->get_type());
        SgInitializedName *closuredName = SageBuilder::buildInitializedName(
            closureNmae, closureVarSym->get_type(), assignInitilizer);
        closureList->append_ctor_initializer(closuredName);
        closuredName->set_parent(closureList);
        closuredName->set_scope(lambdaClassDef);
        lambdaClassDef->append_member(closureVarDel);

        // Add parameter for onstructor initializer
        SgVarRefExp *constructInitializerParam =
            SageBuilder::buildVarRefExp(closureVarSym);
        memberFuncArgList->append_expression(constructInitializerParam);
      }
      // Build constructor with member intialization
      SgMemberFunctionDeclaration *selfDefiningFunctionDecl =
          SageBuilder::buildDefiningMemberFunctionDeclaration(
              shadow_name, SageBuilder::buildVoidType(), captureParamList,
              lambdaClassDef);
      SageInterface::setCtorInitializerList(selfDefiningFunctionDecl,
                                            closureList);
      selfDefiningFunctionDecl->set_associatedClassDeclaration(
          definingLambdaClassDecl);
      // set constructor type to avoid return type being unparsed
      selfDefiningFunctionDecl->get_specialFunctionModifier().setConstructor();
      lambdaClassDef->append_member(selfDefiningFunctionDecl);
      funbody_copy->get_statements().insert(
          funbody_copy->get_statements().begin() + argNumber,
          definingLambdaClassDecl);
      newStmtCount++;

      // Build variable declaration for the new class/
      SgConstructorInitializer *constructorInitializer =
          SageBuilder::buildConstructorInitializer(
              selfDefiningFunctionDecl, memberFuncArgList,
              SageBuilder::buildVoidType(), false, false, false, false);
      ASSERT_not_null(constructorInitializer);
      initializer = isSgInitializer(constructorInitializer);
      SgName init_construct_name(formalArg->get_name());
      init_construct_name << "__" << ++gensym_counter;
      vardecl = SageBuilder::buildVariableDeclaration(
          init_construct_name, definingLambdaClassDecl->get_type(), initializer,
          funbody_copy);
    } else if (hasLambdaFuncArg) {
      // SgLambdaExp* lambdaExp = isSgLambdaExp(classdecl->get_parent());
      // SgClassDeclaration* defingingclassdecl  =
      // isSgClassDeclaration(classdecl->get_definingDeclaration());
      // SgMemberFunctionDeclaration* lambdaFunc =
      // isSgMemberFunctionDeclaration(lambdaExp->get_lambda_function());
      SgAssignInitializer *assignInitializer = new SgAssignInitializer(
          SgNULL_FILE, actualArg, formalArg->get_type());
      ASSERT_not_null(assignInitializer);
      initializer = isSgInitializer(assignInitializer);
      SgType *autoShadowType =
          buildAutoShadowTypeForFormal(formalArg->get_type());
      vardecl = new SgVariableDeclaration(SgNULL_FILE, shadow_name,
                                          autoShadowType, initializer);
      SgInitializedName *vardeclInitializedName =
          vardecl->get_decl_item(shadow_name);
      vardeclInitializedName->set_auto_decltype(autoShadowType);
    } else {
      SgAssignInitializer *assignInitializer = new SgAssignInitializer(
          SgNULL_FILE, actualArg, formalArg->get_type());
      ASSERT_not_null(assignInitializer);
      initializer = isSgInitializer(assignInitializer);
      vardecl = new SgVariableDeclaration(SgNULL_FILE, shadow_name,
                                          formalArg->get_type(), initializer);
    }
    initializer->set_endOfConstruct(SgNULL_FILE);
    vardecl->set_definingDeclaration(vardecl);
    vardecl->set_endOfConstruct(SgNULL_FILE);
    vardecl->get_definition()->set_endOfConstruct(SgNULL_FILE);
    vardecl->set_parent(funbody_copy);

    // Insert the new local variable into the (near) beginning of the
    // to-be-inserted function body.  We insert them in the order their
    // corresponding actuals/formals appear, although the C++ standard does not
    // require this order of evaluation.
    SgInitializedName *init = vardecl->get_variables().back();
    inits.push_back(init);
    initializer->set_parent(init);
    init->set_scope(funbody_copy);
    funbody_copy->get_statements().insert(
        funbody_copy->get_statements().begin() + argNumber + newStmtCount,
        vardecl);
    SgVariableSymbol *sym = new SgVariableSymbol(init);
    paramMap[formalArg] = sym;
    funbody_copy->insert_symbol(shadow_name, sym);
    sym->set_parent(funbody_copy->get_symbol_table());
  }

  // Similarly for "this". We create a local variable in the to-be-inserted
  // function body that will be initialized with the caller's "this".
  if (!isLambdaMemberFuncCall && thisdecl) {
    thisdecl->set_parent(funbody_copy);
    thisinitname->set_scope(funbody_copy);
    funbody_copy->get_statements().insert(
        funbody_copy->get_statements().begin(), thisdecl);
    SgVariableSymbol *thisSym = new SgVariableSymbol(thisinitname);
    funbody_copy->insert_symbol(thisname, thisSym);
    thisSym->set_parent(funbody_copy->get_symbol_table());
    ReplaceThisWithRefVisitor(thisSym).traverse(funbody_copy, postorder);
  }
  if (isLambdaMemberFuncCall) {
    ReplaceCaptureVariableVisitor(varMap).traverse(funbody_copy, postorder);
  }
  ReplaceParameterUseVisitor(paramMap).traverse(funbody_copy, postorder);

  SgName end_of_inline_name = "rose_inline_end__";
  end_of_inline_name << ++gensym_counter;
  SgLabelStatement *end_of_inline_label =
      new SgLabelStatement(SgNULL_FILE, end_of_inline_name);
  end_of_inline_label->set_endOfConstruct(SgNULL_FILE);

  funbody_copy->append_statement(end_of_inline_label);
  end_of_inline_label->set_scope(targetFunction);
  SgLabelSymbol *end_of_inline_label_sym =
      new SgLabelSymbol(end_of_inline_label);
  end_of_inline_label_sym->set_parent(targetFunction->get_symbol_table());
  targetFunction->get_symbol_table()->insert(end_of_inline_label->get_name(),
                                             end_of_inline_label_sym);

  // To ensure that there is some statement after the label
  SgExprStatement *dummyStatement =
      SageBuilder::buildExprStatement(SageBuilder::buildNullExpression());
  dummyStatement->set_endOfConstruct(SgNULL_FILE);
  funbody_copy->append_statement(dummyStatement);
  dummyStatement->set_parent(funbody_copy);

  ChangeReturnsToGotosPrevisitor previsitor = ChangeReturnsToGotosPrevisitor(
      end_of_inline_label, funbody_copy,
      SageInterface::isReferenceType(funcall->get_type()));
  replaceExpressionWithStatement(funcall, &previsitor);

  // Make sure the AST is consistent. To save time, we'll just fix things that
  // we know can go wrong. For instance, the SgAsmExpression.p_lvalue data
  // member is required to be true for certain operators and is set to false in
  // other situations. Since we've introduced new expressions into the AST we
  // need to adjust their p_lvalue according to the operators where they were
  // inserted.
  markLhsValues(targetFunction);
#ifdef NDEBUG
  AstTests::runAllTests(SageInterface::getProject());
#endif

  // DQ (4/7/2015): This fixes something I was required to fix over the weekend
  // and which is fixed more directly, I think. Mark the things we insert as
  // being transformations so they get inserted into the output by backend()
  markAsTransformation(funbody_copy);

  return true;
}
