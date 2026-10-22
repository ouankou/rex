#include "sage3basic.h"

#include "sageBuilder.h"

#include <iomanip>

#include <iostream>

#include <set>

#include <string>

#include "AstConsistencyTests.h"

#include "RoseAst.h" // using AST Iterator
#include "rose_config.h"

#include "inliner.h"

#include "inlinerSupport.h"

#include "replaceExpressionWithStatement.h"

using namespace std;
using namespace Rose;
using namespace SageInterface;

namespace Inliner {
bool skipHeaders = false;
bool verbose = false; // if set to true, generate debugging information
} // namespace Inliner

namespace {
SgLambdaExp *
lambdaExpressionForClassDeclaration(SgClassDeclaration *classDecl) {
  if (classDecl == nullptr ||
      classDecl->get_class_type() != SgClassDeclaration::e_lambda_closure)
    return nullptr;
  auto *definition = isSgClassDeclaration(classDecl->get_definingDeclaration());
  ROSE_ASSERT(definition != nullptr);
  SgLambdaExp *expression = definition->get_lambda_expression();
  if (expression != nullptr)
    ROSE_ASSERT(expression->get_lambda_closure_class() == definition);
  return expression;
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
  auto *member = isSgMemberFunctionDeclaration(functionDecl);
  return member == nullptr
             ? nullptr
             : lambdaExpressionForClassDeclaration(isSgClassDeclaration(
                   member->get_associatedClassDeclaration()));
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
    SgFunctionDeclaration *function;
    bool unavailableAccess = false;

  public:
    NonPublicMemberAccessTraversal(SgFunctionCallExp *funcall,
                                   SgFunctionDeclaration *function)
        : funcall(funcall), function(function) {}

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
      if (owningClass == nullptr)
        return;
      const auto &access = decl->get_declarationModifier().get_accessModifier();
      auto *context = SageInterface::getEnclosingFunctionDeclaration(node);
      if (context != nullptr)
        context = isSgFunctionDeclaration(context->get_definingDeclaration());
      if (context != function &&
          functionContextHasClassAccess(context, owningClass,
                                        access.isProtected()))
        return;
      if (!callSiteHasClassAccess(funcall, owningClass, access)) {
        unavailableAccess = true;
      }
    }

    bool foundUnavailableAccess() const { return unavailableAccess; }
  };

  NonPublicMemberAccessTraversal traversal(funcall, fundef->get_declaration());
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
    returnAssignmentOperator =
        SageBuilder::buildAssignOp(lhs, rhs, lhs->get_type());
  } else
    returnAssignmentOperator = rhs;

  return returnAssignmentOperator;
}

// Change all return statements in a block of code to assignments to a
// variable and gotos to a given label.  Used internally by the inliner.
class ChangeReturnsToGotosVisitor : public AstSimpleProcessing {
private:
  SgLabelStatement *label;
  SgStatement *generated_body;
  SgExpression *where_to_write_answer;
  bool returns_by_reference;

public:
  ChangeReturnsToGotosVisitor(SgLabelStatement *label,
                              SgStatement *generated_body,
                              SgExpression *where_to_write_answer,
                              bool returns_by_reference)
      : label(label), generated_body(generated_body),
        where_to_write_answer(where_to_write_answer),
        returns_by_reference(returns_by_reference) {}

  virtual void visit(SgNode *n) {
    SgReturnStmt *rs = isSgReturnStmt(n);
    if (rs) {
      SgNode *owner = rs;
      while (owner != generated_body &&
             isSgFunctionDefinition(owner) == nullptr) {
        owner = owner->get_parent();
        if (owner == nullptr) {
          fprintf(stderr,
                  "REX_INLINE_INVARIANT[return-generated-body]: return=%p "
                  "does not descend from generated body=%p\n",
                  static_cast<void *>(rs), static_cast<void *>(generated_body));
          ROSE_ABORT();
        }
      }
      if (owner != generated_body) {
        // A lambda or local callable is a distinct return domain even though
        // its definition is structurally nested in the copied function body.
        return;
      }

      SgExpression *return_expr = rs->get_expression();
      if (return_expr != NULL) {
        if (return_expr->get_parent() != rs) {
          fprintf(stderr,
                  "REX_INLINE_INVARIANT[return-expression-owner]: return=%p "
                  "expression=%p parent=%p is not the exact typed child\n",
                  static_cast<void *>(rs), static_cast<void *>(return_expr),
                  static_cast<void *>(return_expr->get_parent()));
          ROSE_ABORT();
        }
        rs->set_expression(NULL);
        return_expr->set_parent(NULL);
      }
      SgBasicBlock *block = SageBuilder::buildBasicBlock();
      // The replacement scope must acquire its physical output identity before
      // generated statements are published into it.  This also detaches the
      // old return in one checked ownership transaction.
      SageInterface::replaceStatement(rs, block);
      if (return_expr) {
        SgExpression *result_expr = return_expr;
        SgExpression *answer_expr =
            where_to_write_answer != NULL
                ? SageInterface::copyExpression(where_to_write_answer)
                : NULL;
        if (returns_by_reference && where_to_write_answer != NULL &&
            SageInterface::isPointerType(where_to_write_answer->get_type())) {
          result_expr = SageBuilder::buildAddressOfOp(
              return_expr, where_to_write_answer->get_type(),
              SgAddressOfOp::e_address_operand_object);
        }

        SgExpression *assignment =
            generateAssignmentMaybe(answer_expr, result_expr);
        SgStatement *assign_stmt = SageBuilder::buildExprStatement(assignment);
        SageInterface::appendStatement(assign_stmt, block);
      }

      SgGotoStatement *gotoStatement = SageBuilder::buildGotoStatement(label);
      SageInterface::appendStatement(gotoStatement, block);
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
  std::vector<SgVariableDeclaration *> pending_declarations;
  bool returns_by_reference;
  SgExpression *where_to_write_answer = NULL;
  bool finalized = false;

public:
  ChangeReturnsToGotosPrevisitor(
      SgLabelStatement *end, SgStatement *body,
      const std::vector<SgVariableDeclaration *> &declarations,
      bool returns_by_reference)
      : end_of_inline_label(end), funbody_copy(body),
        pending_declarations(declarations),
        returns_by_reference(returns_by_reference) {}

  SgStatement *generate(SgExpression *where_to_write_answer) override {
    if (finalized || funbody_copy == NULL ||
        funbody_copy->get_parent() != NULL) {
      fprintf(stderr,
              "REX_INLINE_INVARIANT[generated-body]: body=%p parent=%p "
              "finalized=%d is not one detached unconsumed inline body\n",
              static_cast<void *>(funbody_copy),
              funbody_copy != NULL
                  ? static_cast<void *>(funbody_copy->get_parent())
                  : NULL,
              finalized ? 1 : 0);
      ROSE_ABORT();
    }
    this->where_to_write_answer = where_to_write_answer;
    return funbody_copy;
  }

  void finalizeGeneratedStatement(SgStatement *generatedStatement) override {
    if (finalized || generatedStatement != funbody_copy ||
        funbody_copy->get_parent() == NULL) {
      fprintf(stderr,
              "REX_INLINE_INVARIANT[published-body]: expected=%p actual=%p "
              "parent=%p finalized=%d is not one attached inline body\n",
              static_cast<void *>(funbody_copy),
              static_cast<void *>(generatedStatement),
              funbody_copy != NULL
                  ? static_cast<void *>(funbody_copy->get_parent())
                  : NULL,
              finalized ? 1 : 0);
      ROSE_ABORT();
    }
    finalized = true;
    for (auto declaration = pending_declarations.rbegin();
         declaration != pending_declarations.rend(); ++declaration) {
      SageInterface::prependStatement(*declaration,
                                      isSgScopeStatement(funbody_copy));
    }
    SageInterface::appendStatement(end_of_inline_label,
                                   isSgScopeStatement(funbody_copy));
    ChangeReturnsToGotosVisitor(end_of_inline_label, funbody_copy,
                                where_to_write_answer, returns_by_reference)
        .traverse(funbody_copy, postorder);
  }
};

class ReplaceCaptureVariableVisitor : public AstSimpleProcessing {
public:
  typedef std::map<SgVariableSymbol *, SgVariableSymbol *> captureVarMap;

private:
  const captureVarMap &varMap;
  std::vector<SgBinaryOp *> retired;

public:
  ReplaceCaptureVariableVisitor(const captureVarMap &varMap) : varMap(varMap) {}

  void atTraversalEnd() override {
    for (SgBinaryOp *binaryOp : retired) {
      ROSE_ASSERT(binaryOp->get_parent() == nullptr &&
                  binaryOp->get_rhs_operand() == nullptr);
      SageInterface::deepDelete(binaryOp);
    }
    retired.clear();
  }

  void visit(SgNode *n) override {
    if (isSgDotExp(n) || isSgArrowExp(n)) {
      SgBinaryOp *binaryOp = isSgBinaryOp(n);
      SgExpression *replacement = binaryOp->get_rhs_operand();
      ROSE_ASSERT(replacement != nullptr &&
                  replacement->get_parent() == binaryOp);
      binaryOp->set_rhs_operand(nullptr);
      replacement->set_parent(nullptr);
      SageInterface::replaceExpression(binaryOp, replacement, true);
      retired.push_back(binaryOp);
    }
    if (isSgVarRefExp(n)) {
      SgVarRefExp *vr = isSgVarRefExp(n);
      SgVariableSymbol *sym = vr->get_symbol();
      captureVarMap::const_iterator iter = varMap.find(sym);
      if (iter == varMap.end())
        return;
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
      SgExpression *thisExpression = isSgExpression(n);
      ROSE_ASSERT(thisExpression->get_parent() != NULL);
      SgVarRefExp *vr = SageBuilder::buildVarRefExp(sym);
      SageInterface::replaceExpression(thisExpression, vr);
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

void removeRedundantCopyInConstruction(SgInitializedName *in) {
  ROSE_ASSERT(in);
  SgAssignInitializer *ai = isSgAssignInitializer(in->get_initializer());
  ROSE_ASSERT(ai && ai->get_parent() == in);
  SgInitializer *realInit = isSgInitializer(ai->get_operand());
  ROSE_ASSERT(realInit);
  ROSE_ASSERT(isSgConstructorInitializer(realInit));
  ROSE_ASSERT(ai->release_operand() == realInit);
  in->set_initializer(realInit);
  delete ai;
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
        if (SgAuxiliaryDeclarationList *auxiliary =
                isSgAuxiliaryDeclarationList(loc)) {
          auxiliary->validate_semantic_non_output_role();
          // Auxiliary declarations are semantic dependencies of the copied
          // scope, not descendants of its lexical output surface. Keep the
          // complete container subtree outside the transformation
          // publication transaction.
          return true;
        }
        if (isSuppressedFrontendNode(loc)) {
          return true;
        }

        if (SgCastExp *cast = isSgCastExp(loc);
            cast != NULL &&
            cast->get_cast_type() == SgCastExp::e_implicit_cast) {
          cast->validate_semantic_conversion();
          size_t position_index = 0;
          for (Sg_File_Info *position :
               {cast->get_file_info(), cast->get_startOfConstruct(),
                cast->get_endOfConstruct(), cast->get_operatorPosition()}) {
            if (position == NULL || position->get_parent() != cast ||
                position->isShared() || !position->isCompilerGenerated() ||
                position->isTransformation() ||
                !position->isOutputInCodeGeneration() ||
                !position->isImplicitCast()) {
              fprintf(
                  stderr,
                  "REX_INLINE_INVARIANT[implicit-conversion-provenance]: "
                  "cast=%p position[%zu]=%p parent=%p shared=%d "
                  "generated=%d transformation=%d output=%d implicit=%d "
                  "is not one exact copied semantic wrapper\n",
                  static_cast<void *>(cast), position_index,
                  static_cast<void *>(position),
                  static_cast<void *>(position != NULL ? position->get_parent()
                                                       : NULL),
                  position != NULL && position->isShared() ? 1 : 0,
                  position != NULL && position->isCompilerGenerated() ? 1 : 0,
                  position != NULL && position->isTransformation() ? 1 : 0,
                  position != NULL && position->isOutputInCodeGeneration() ? 1
                                                                           : 0,
                  position != NULL && position->isImplicitCast() ? 1 : 0);
              ROSE_ABORT();
            }
            ++position_index;
          }
          // The implicit conversion is a transparent frontend semantic edge,
          // not a new written surface.  Its operand remains traversable and
          // independently enters the inlined transformation subtree.
          return false;
        }

        // The copied subtree is a new lexical surface.  All three owned source
        // positions must enter the transformation state together; leaving the
        // primary position source-backed makes token mapping treat a copied
        // declaration as an original declaration without a source token
        // interval.
        for (Sg_File_Info *position :
             {loc->get_file_info(), loc->get_startOfConstruct(),
              loc->get_endOfConstruct()}) {
          if (position == NULL) {
            continue;
          }
          if (position->get_parent() != loc) {
            fprintf(stderr,
                    "REX_INLINE_INVARIANT[transformation-provenance]: "
                    "node=%p/%s position=%p has owner=%p\n",
                    static_cast<void *>(loc), loc->class_name().c_str(),
                    static_cast<void *>(position),
                    static_cast<void *>(position->get_parent()));
            ROSE_ABORT();
          }
          position->setTransformation();
          position->setOutputInCodeGeneration();
        }

        if (SgExpression *exp = isSgExpression(loc)) {
          if (exp->get_operatorPosition()) {
            if (exp->get_operatorPosition()->get_parent() != exp) {
              fprintf(stderr,
                      "REX_INLINE_INVARIANT[transformation-provenance]: "
                      "expression=%p/%s operator-position=%p has owner=%p\n",
                      static_cast<void *>(exp), exp->class_name().c_str(),
                      static_cast<void *>(exp->get_operatorPosition()),
                      static_cast<void *>(
                          exp->get_operatorPosition()->get_parent()));
              ROSE_ABORT();
            }
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
bool doInline(SgFunctionCallExp *funcall, bool allowRecursion) {

  if (Inliner::verbose) {
    Sg_File_Info *info_start = funcall->get_startOfConstruct();
    size_t a_start = (size_t)info_start->get_line();
    cout << "Inside doInling() for a function call @ " << a_start << endl;
  }
  if (Inliner::skipHeaders &&
      (funcall->get_file_info()->isCompilerGenerated() ||
       SageInterface::insideHeader(funcall)))
    return false;

  // Handle member function calls like a.foo() or aptr->foo()
  // Walk to its right-hand side to get the member function reference
  // expression.
  SgExpression *funname = funcall->get_function();
  if (SgCastExp *decay = isSgCastExp(funname)) {
    decay->validate_semantic_conversion();
    if (decay->get_cast_type() == SgCastExp::e_implicit_cast &&
        decay->get_semantic_conversion_kind() ==
            SgCastExp::e_semantic_conversion_FunctionToPointerDecay) {
      SgPointerType *resultType = isSgPointerType(decay->get_type());
      SgExpression *sourceFunction = decay->get_operand_i();
      if (resultType == NULL || sourceFunction == NULL ||
          isSgFunctionType(resultType->get_base_type()) == NULL ||
          isSgFunctionType(sourceFunction->get_type()) == NULL) {
        fprintf(stderr,
                "REX_INLINE_INVARIANT[function-pointer-decay]: call=%p "
                "cast=%p does not preserve one exact function identity\n",
                static_cast<void *>(funcall), static_cast<void *>(decay));
        ROSE_ABORT();
      }
      funname = sourceFunction;
    }
  }
  SgExpression *func_ref_exp = isSgFunctionRefExp(funname);
  SgDotExp *dotexp = isSgDotExp(funname);
  SgArrowExp *arrowexp = isSgArrowExp(funname);
  SgExpression *thisptr = 0;
  if (dotexp || arrowexp) {
    func_ref_exp = isSgBinaryOp(funname)->get_rhs_operand();
    if (dotexp) {
      SgExpression *lhs = dotexp->get_lhs_operand();

      SgMemberFunctionRefExp *m_ref_exp =
          isSgMemberFunctionRefExp(func_ref_exp);
      if (m_ref_exp) {
        SgMemberFunctionSymbol *symbol = m_ref_exp->get_symbol();
        ROSE_ASSERT(symbol);
        SgMemberFunctionDeclaration *m_func_decl = symbol->get_declaration();
        ROSE_ASSERT(m_func_decl);
        const SgSpecialFunctionModifier &special =
            m_func_decl->get_specialFunctionModifier();
        if ((special.isOperator() || special.isConversion() ||
             special.isUldOperator()) &&
            lambdaExpressionForFunctionDeclaration(m_func_decl) == nullptr) {
          if (Inliner::verbose) {
            std::cout << "Inline returns false: skip non-lambda operator "
                         "function named:"
                      << m_func_decl->get_qualified_name().getString()
                      << std::endl;
          }
          return false;
        }
      }

      // FIXME -- patch this into p_lvalue
      bool is_lvalue = lhs->get_lvalue();
      if (isSgInitializer(lhs))
        is_lvalue = false;

      if (!is_lvalue) {
        SgAssignInitializer *ai = SageInterface::splitExpression(lhs);
        ROSE_ASSERT(ai);
        SgInitializedName *in = isSgInitializedName(ai->get_parent());
        ROSE_ASSERT(in);
        in->set_auto_decltype(SageBuilder::buildAutoType());
        if (isSgInitializer(ai->get_operand()))
          removeRedundantCopyInConstruction(in);
        lhs = dotexp->get_lhs_operand(); // Should be a var ref now
      }
      SgType *lhsType = lhs->get_type();
      if (lhsType == NULL) {
        fprintf(stderr,
                "REX_INLINE_INVARIANT[member-object-type]: expression=%p/%s "
                "has no exact semantic type\n",
                static_cast<void *>(lhs), lhs->class_name().c_str());
        ROSE_ABORT();
      }
      SgType *thisPointerType = SageBuilder::buildPointerType(lhsType);
      if (thisPointerType == NULL) {
        fprintf(stderr,
                "REX_INLINE_INVARIANT[member-object-type]: expression=%p/%s "
                "type=%p/%s has no exact pointer type\n",
                static_cast<void *>(lhs), lhs->class_name().c_str(),
                static_cast<void *>(lhsType), lhsType->class_name().c_str());
        ROSE_ABORT();
      }
      thisptr = SageBuilder::buildAddressOfOp(
          SageInterface::copyExpression(lhs), thisPointerType,
          SgAddressOfOp::e_address_operand_object);
    } else if (arrowexp) {
      thisptr = SageInterface::copyExpression(arrowexp->get_lhs_operand());
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
  if (fundecl) {
    ROSE_ASSERT(fundecl->get_scope());
    for (SgScopeStatement *scope = fundecl->get_scope(); !isSgGlobal(scope);
         scope = SageInterface::getEnclosingScope(scope)) {
      ROSE_ASSERT(scope);
      if (SgNamespaceDefinitionStatement *namespace_scope =
              isSgNamespaceDefinitionStatement(scope)) {
        SgNamespaceDeclarationStatement *namespace_decl =
            namespace_scope->get_namespaceDeclaration();
        ROSE_ASSERT(namespace_decl && namespace_decl->get_scope());
        if (namespace_decl->get_name() == "std" &&
            isSgGlobal(namespace_decl->get_scope())) {
          if (Inliner::verbose) {
            std::cout << "Inline returns false: skip std function named:"
                      << fundecl->get_qualified_name().getString() << std::endl;
          }
          return false;
        }
      }
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
      my_fundef = my_fundef->get_parent();
      ROSE_ASSERT(my_fundef != NULL);
    }
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
  SgType *thisptrtype = NULL;
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
    thisptrtype = thisptr->get_type();
    const SgSpecialFunctionModifier &specialMod =
        funsym->get_declaration()->get_specialFunctionModifier();
    SgFunctionType *ft = funsym->get_declaration()->get_type();
    ROSE_ASSERT(ft);
    SgMemberFunctionType *mft = isSgMemberFunctionType(ft);
    ROSE_ASSERT(mft);
    SgType *ct = mft->get_class_type();
    if (specialMod.isConstructor()) {
      thisptrtype = SageBuilder::buildPointerType(ct);
    }
    SgClassDeclaration *classDecl =
        isSgClassDeclaration(isSgClassType(ct)->get_declaration());
    ROSE_ASSERT(classDecl);
    if (SgLambdaExp *lambdaExp =
            lambdaExpressionForClassDeclaration(classDecl)) {
      isLambdaMemberFuncCall = true;
      SgLambdaCaptureList *lambdaCaptureList =
          lambdaExp->get_lambda_capture_list();
      SgLambdaCapturePtrList captureList =
          lambdaCaptureList->get_capture_list();
      for (SgLambdaCapture *capture : captureList) {
        // get the capture variable
        ROSE_ASSERT(capture != nullptr &&
                    capture->get_capture_variable() != nullptr &&
                    capture->get_capture_variable()->get_parent() == capture);
        SgVarRefExp *captureVarRef =
            isSgVarRefExp(capture->get_capture_variable());
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
      SgTypeModifier exact_modifier;
      exact_modifier.get_constVolatileModifier() = thiscv;
      thisptrtype = SageBuilder::buildModifierType(thisptrtype, exact_modifier);
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
  std::vector<SgVariableDeclaration *> pendingDeclarations;
  SgTreeCopy tc;
  SgFunctionDefinition *function_copy =
      isSgFunctionDefinition(fundef->copy(tc));

  ROSE_ASSERT(function_copy);

  SgFunctionDeclaration *function_copy_declaration =
      function_copy->get_declaration();
  if (function_copy_declaration == NULL ||
      function_copy_declaration->get_args().size() != params.size()) {
    fprintf(stderr,
            "REX_INLINE_INVARIANT[copied-formals]: copied definition=%p "
            "declaration=%p has %zu formals; source declaration=%p has %zu\n",
            static_cast<void *>(function_copy),
            static_cast<void *>(function_copy_declaration),
            function_copy_declaration != NULL
                ? function_copy_declaration->get_args().size()
                : 0,
            static_cast<void *>(fundecl), params.size());
    ROSE_ABORT();
  }
  SgInitializedNamePtrList copied_params =
      function_copy_declaration->get_args();
  for (size_t i = 0; i < params.size(); ++i) {
    SgCopyHelp::copiedNodeMapTypeIterator copied =
        tc.get_copiedNodeMap().find(params[i]);
    if (copied == tc.get_copiedNodeMap().end() ||
        copied->second != copied_params[i]) {
      fprintf(stderr,
              "REX_INLINE_INVARIANT[copied-formals]: source formal=%p at "
              "index=%zu maps to %p instead of exact copied formal=%p\n",
              static_cast<void *>(params[i]), i,
              static_cast<void *>(copied != tc.get_copiedNodeMap().end()
                                      ? copied->second
                                      : NULL),
              static_cast<void *>(copied_params[i]));
      ROSE_ABORT();
    }
  }

  // SgThisExp's semantic class-symbol edge must be rebound by the central copy
  // transaction.  Inlining does not repair or rehome copied symbols.
  for (SgCopyHelp::copiedNodeMapTypeIterator iter =
           tc.get_copiedNodeMap().begin();
       iter != tc.get_copiedNodeMap().end(); ++iter) {
    const SgThisExp *originalThis = isSgThisExp(iter->first);
    SgThisExp *copiedThis = isSgThisExp(iter->second);
    if (originalThis == NULL) {
      continue;
    }
    SgClassSymbol *originalSymbol = originalThis->get_class_symbol();
    SgClassSymbol *copiedSymbol =
        copiedThis != NULL ? copiedThis->get_class_symbol() : NULL;
    SgSymbolTable *originalTable =
        originalSymbol != NULL ? isSgSymbolTable(originalSymbol->get_parent())
                               : NULL;
    SgScopeStatement *originalScope =
        originalTable != NULL ? isSgScopeStatement(originalTable->get_parent())
                              : NULL;
    SgScopeStatement *expectedScope = originalScope;
    SgCopyHelp::copiedNodeMapTypeIterator copiedScope =
        tc.get_copiedNodeMap().find(originalScope);
    if (copiedScope != tc.get_copiedNodeMap().end() &&
        copiedScope->second != originalScope) {
      expectedScope = isSgScopeStatement(copiedScope->second);
    }
    SgSymbolTable *expectedTable =
        expectedScope != NULL ? expectedScope->get_symbol_table() : NULL;
    if (copiedThis == NULL || originalSymbol == NULL || copiedSymbol == NULL ||
        originalScope == NULL || expectedScope == NULL ||
        expectedTable == NULL || copiedSymbol->get_parent() != expectedTable ||
        !expectedTable->exists(copiedSymbol)) {
      fprintf(stderr,
              "REX_INLINE_INVARIANT[this-symbol]: original=%p symbol=%p "
              "scope=%p copy=%p symbol=%p parent=%p expected-table=%p\n",
              static_cast<const void *>(originalThis),
              static_cast<void *>(originalSymbol),
              static_cast<void *>(originalScope),
              static_cast<void *>(copiedThis),
              static_cast<void *>(copiedSymbol),
              static_cast<void *>(
                  copiedSymbol != NULL ? copiedSymbol->get_parent() : NULL),
              static_cast<void *>(expectedTable));
      ROSE_ABORT();
    }
  }

  SgBasicBlock *funbody_copy = function_copy->get_body();
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

  // Release the copied body for insertion, but retain its complete temporary
  // function family until every reference to copied formals and captures has
  // been rebound below.  Retiring the definition here would leave the copied
  // canonical declaration pointing at a live defining declaration with no
  // definition.
  if (function_copy->get_declaration() != function_copy_declaration ||
      function_copy->get_parent() != function_copy_declaration ||
      function_copy_declaration->get_definition() != function_copy) {
    fprintf(stderr,
            "REX_INLINE_INVARIANT[function-copy-owner]: copied "
            "definition=%p declaration=%p parent=%p definition-edge=%p "
            "does not preserve exact declaration ownership\n",
            static_cast<void *>(function_copy),
            static_cast<void *>(function_copy_declaration),
            static_cast<void *>(function_copy->get_parent()),
            static_cast<void *>(function_copy_declaration->get_definition()));
    ROSE_ABORT();
  }
  if (function_copy->get_body()) {
    ASSERT_require(function_copy->get_body()->get_parent() == function_copy);
    function_copy->get_body()->set_parent(NULL);
    function_copy->set_body(NULL);
  }
  if (funbody_copy->get_parent() != NULL) {
    fprintf(stderr,
            "REX_INLINE_INVARIANT[copied-body-release]: body=%p parent=%p "
            "was not detached from its copied function definition\n",
            static_cast<void *>(funbody_copy),
            static_cast<void *>(funbody_copy->get_parent()));
    ROSE_ABORT();
  }
  if (!isLambdaMemberFuncCall && thisptr != NULL) {
    if (thisptrtype == NULL || thisptr->get_parent() != NULL) {
      fprintf(stderr,
              "REX_INLINE_INVARIANT[this-shadow-producer]: this-expression=%p "
              "parent=%p type=%p is not one detached typed operand\n",
              static_cast<void *>(thisptr),
              static_cast<void *>(thisptr->get_parent()),
              static_cast<void *>(thisptrtype));
      ROSE_ABORT();
    }
    SgAssignInitializer *assignInitializer =
        SageBuilder::buildAssignInitializer(thisptr, thisptrtype);
    thisdecl = SageBuilder::buildVariableDeclaration(
        thisname, thisptrtype, assignInitializer, funbody_copy);
    thisinitname = SageInterface::getFirstInitializedName(thisdecl);
    if (thisinitname == NULL ||
        thisinitname->get_initializer() != assignInitializer) {
      fprintf(stderr,
              "REX_INLINE_INVARIANT[this-shadow-producer]: declaration=%p "
              "has no exact initialized-name and initializer ownership\n",
              static_cast<void *>(thisdecl));
      ROSE_ABORT();
    }
    pendingDeclarations.push_back(thisdecl);
  }

  // In the to-be-inserted function body, create new local variables with
  // distinct non-conflicting names, one per formal argument and having the same
  // type as the formal argument. Initialize those new local variables with the
  // actual arguments.  Also, build a paramMap that maps each formal argument
  // (SgInitializedName) to its corresponding new local variable
  // (SgVariableSymbol).
  ReplaceParameterUseVisitor::paramMapType paramMap;
  SgInitializedNamePtrList::iterator formalIter = copied_params.begin();
  SgExpressionPtrList::iterator actualIter = funargs.begin();
  for (size_t argNumber = 0;
       formalIter != copied_params.end() && actualIter != funargs.end();
       ++argNumber, ++formalIter, ++actualIter) {
    SgInitializedName *formalArg = *formalIter;
    SgExpression *actualArg = *actualIter;

    // Build the new local variable.
    // FIXME[Robb P. Matzke 2014-12-12]: we need a better way to generate a
    // non-conflicting local variable name.
    bool hasLambdaFuncArg = isSgLambdaExp(actualArg) != NULL;
    if (SgClassType *classType = isSgClassType(formalArg->get_typeptr())) {
      SgClassDeclaration *classDeclaration =
          isSgClassDeclaration(classType->get_declaration());
      ROSE_ASSERT(classDeclaration);
      hasLambdaFuncArg =
          hasLambdaFuncArg ||
          lambdaExpressionForClassDeclaration(classDeclaration) != NULL;
    }
    SgVariableDeclaration *vardecl = NULL;
    SgName shadow_name(formalArg->get_name());
    shadow_name << "__" << ++gensym_counter;
    if (hasLambdaFuncArg) {
      SgType *autoShadowType =
          buildAutoShadowTypeForFormal(formalArg->get_type());
      SgAssignInitializer *assignInitializer =
          SageBuilder::buildAssignInitializer(actualArg, autoShadowType);
      ASSERT_not_null(assignInitializer);
      vardecl = SageBuilder::buildVariableDeclaration(
          shadow_name, autoShadowType, assignInitializer, funbody_copy);
      SgInitializedName *vardeclInitializedName =
          vardecl->get_decl_item(shadow_name);
      vardeclInitializedName->set_auto_decltype(autoShadowType);
    } else {
      SgAssignInitializer *assignInitializer =
          SageBuilder::buildAssignInitializer(actualArg, formalArg->get_type());
      ASSERT_not_null(assignInitializer);
      vardecl = SageBuilder::buildVariableDeclaration(
          shadow_name, formalArg->get_type(), assignInitializer, funbody_copy);
    }

    // Insert the new local variable into the (near) beginning of the
    // to-be-inserted function body.  We insert them in the order their
    // corresponding actuals/formals appear, although the C++ standard does not
    // require this order of evaluation.
    SgInitializedName *init = vardecl->get_variables().back();
    pendingDeclarations.push_back(vardecl);
    SgVariableSymbol *sym =
        isSgVariableSymbol(funbody_copy->find_symbol_from_declaration(init));
    if (sym == NULL || sym->get_declaration() != init ||
        sym->get_parent() != funbody_copy->get_symbol_table()) {
      fprintf(stderr,
              "REX_INLINE_INVARIANT[parameter-shadow-symbol]: formal=%p "
              "declaration=%p name=%p symbol=%p has no exact copied-body "
              "publication\n",
              static_cast<void *>(formalArg), static_cast<void *>(vardecl),
              static_cast<void *>(init), static_cast<void *>(sym));
      ROSE_ABORT();
    }
    paramMap[formalArg] = sym;
  }

  // Similarly for "this". We create a local variable in the to-be-inserted
  // function body that will be initialized with the caller's "this".
  if (!isLambdaMemberFuncCall && thisdecl) {
    SgVariableSymbol *thisSym = isSgVariableSymbol(
        funbody_copy->find_symbol_from_declaration(thisinitname));
    if (thisSym == NULL || thisSym->get_declaration() != thisinitname ||
        thisSym->get_parent() != funbody_copy->get_symbol_table()) {
      fprintf(stderr,
              "REX_INLINE_INVARIANT[this-shadow-symbol]: declaration=%p "
              "name=%p symbol=%p has no exact copied-body publication\n",
              static_cast<void *>(thisdecl), static_cast<void *>(thisinitname),
              static_cast<void *>(thisSym));
      ROSE_ABORT();
    }
    ReplaceThisWithRefVisitor(thisSym).traverse(funbody_copy, postorder);
  }
  if (isLambdaMemberFuncCall) {
    ReplaceCaptureVariableVisitor(varMap).traverse(funbody_copy, postorder);
  }
  ReplaceParameterUseVisitor(paramMap).traverse(funbody_copy, postorder);

  // The copied declaration family is a temporary semantic owner for copied
  // formals while references are being rebound.  It must now be completely
  // isolated from the body that will be inserted.  Split its canonical and
  // defining declarations into independently closed roots and require the
  // deletion machinery to prove that no live AST edge still targets either
  // root.
  SgFunctionDeclaration *copied_canonical = isSgFunctionDeclaration(
      function_copy_declaration->get_firstNondefiningDeclaration());
  SgAuxiliaryDeclarationList *copied_canonical_owner =
      copied_canonical != NULL
          ? isSgAuxiliaryDeclarationList(copied_canonical->get_parent())
          : NULL;
  if (copied_canonical == NULL ||
      copied_canonical == function_copy_declaration ||
      copied_canonical_owner == NULL ||
      copied_canonical_owner->get_parent() != NULL ||
      copied_canonical_owner->get_declarations().size() != 1 ||
      copied_canonical_owner->get_declarations().front() != copied_canonical ||
      copied_canonical->get_firstNondefiningDeclaration() != copied_canonical ||
      copied_canonical->get_definingDeclaration() !=
          function_copy_declaration ||
      function_copy_declaration->get_firstNondefiningDeclaration() !=
          copied_canonical ||
      function_copy_declaration->get_definingDeclaration() !=
          function_copy_declaration ||
      function_copy_declaration->get_definition() != function_copy ||
      function_copy->get_declaration() != function_copy_declaration ||
      function_copy->get_parent() != function_copy_declaration ||
      function_copy->get_body() != NULL) {
    fprintf(stderr,
            "REX_INLINE_INVARIANT[function-copy-retirement]: canonical=%p "
            "canonical-owner=%p defining=%p definition=%p is not one exact "
            "detached copied function family\n",
            static_cast<void *>(copied_canonical),
            static_cast<void *>(copied_canonical_owner),
            static_cast<void *>(function_copy_declaration),
            static_cast<void *>(function_copy));
    ROSE_ABORT();
  }

  copied_canonical_owner->get_declarations().clear();
  copied_canonical->set_parent(NULL);
  copied_canonical->set_definingDeclaration(NULL);
  function_copy_declaration->set_firstNondefiningDeclaration(
      function_copy_declaration);
  SageInterface::deleteAST(copied_canonical,
                           SageInterface::DeleteAstMode::kRequireIsolated);
  SageInterface::deleteAST(copied_canonical_owner,
                           SageInterface::DeleteAstMode::kRequireIsolated);
  SageInterface::deleteAST(function_copy_declaration,
                           SageInterface::DeleteAstMode::kRequireIsolated);
  function_copy = NULL;
  function_copy_declaration = NULL;

  SgName end_of_inline_name = "rose_inline_end__";
  end_of_inline_name << ++gensym_counter;
  SgNullStatement *dummyStatement = SageBuilder::buildNullStatement();
  SgLabelStatement *end_of_inline_label = SageBuilder::buildLabelStatement(
      end_of_inline_name, dummyStatement, targetFunction);

  SgLabelSymbol *end_of_inline_label_sym =
      targetFunction->lookup_label_symbol(end_of_inline_name);
  if (end_of_inline_label_sym == NULL ||
      end_of_inline_label_sym->get_declaration() != end_of_inline_label ||
      end_of_inline_label_sym->get_parent() !=
          targetFunction->get_symbol_table()) {
    fprintf(stderr,
            "REX_INLINE_INVARIANT[end-label-symbol]: label=%p symbol=%p "
            "has no exact target-function publication\n",
            static_cast<void *>(end_of_inline_label),
            static_cast<void *>(end_of_inline_label_sym));
    ROSE_ABORT();
  }

  ChangeReturnsToGotosPrevisitor previsitor = ChangeReturnsToGotosPrevisitor(
      end_of_inline_label, funbody_copy, pendingDeclarations,
      SageInterface::isReferenceType(funcall->get_type()));
  replaceExpressionWithStatement(funcall, &previsitor);

#ifdef NDEBUG
  AstTests::runAllTests(SageInterface::getProject());
#endif

  // The copied body has become a new lexical surface at the call site. Publish
  // that producer transition before token mapping sees its declarations;
  // copied source coordinates are provenance only and cannot claim the
  // original function's token intervals.
  markAsTransformation(funbody_copy);

  return true;
}
