
#include "AstConsistencyTests.h"

#include "sage3basic.h"
#include "sageInterface.h"

#include "expressionTreeEqual.h"

#include "inlinerSupport.h"

#include "patternRewrite.h"

#include "pre.h"

#include <iostream>

#include <map>
#include <memory>

#include <vector>

#include "finiteDifferencing.h"

using namespace std;

namespace {
SgExpression *
finiteDifferencingOutermostImplicitConversion(SgExpression *expression) {
  ROSE_ASSERT(expression != nullptr);
  SgExpression *result = expression;
  while (SgCastExp *cast = isSgCastExp(result->get_parent())) {
    cast->validate_semantic_conversion();
    if (cast->cast_type() != SgCastExp::e_implicit_cast) {
      break;
    }
    if (cast->get_operand() != result || result->get_parent() != cast) {
      fprintf(stderr,
              "REX_FINITE_DIFFERENCING_INVARIANT[implicit-conversion]: "
              "expression=%p cast=%p has no exact transparent owner edge\n",
              static_cast<void *>(expression), static_cast<void *>(cast));
      ROSE_ABORT();
    }
    result = cast;
  }
  return result;
}
} // namespace

class FdFindCopiesVisitor : public AstSimpleProcessing {
  SgExpression *target;
  SgFunctionDefinition *function;
  vector<SgExpression *> &copies;

public:
  FdFindCopiesVisitor(SgExpression *target, vector<SgExpression *> &copies)
      : target(target),
        function(SageInterface::getEnclosingFunctionDefinition(target)),
        copies(copies) {
    ROSE_ASSERT(function != nullptr);
  }

  virtual void visit(SgNode *n) {
    if (isSgExpression(n) &&
        SageInterface::getEnclosingFunctionDefinition(n) == function &&
        (n == target || expressionTreeEqual(isSgExpression(n), target))) {
      SgExpression *copy = isSgExpression(n);
      if (std::find(copies.begin(), copies.end(), copy) == copies.end()) {
        copies.push_back(copy);
      }
    }
  }
};

static void
replaceCopiesOfExpression(SgExpression *src, SgExpression *tgt, SgNode *root,
                          vector<SgExpression *> &retiredExpressions,
                          map<SgExpression *, SgExpression *> &published) {
  vector<SgExpression *> copies_of_src;
  FdFindCopiesVisitor(src, copies_of_src).traverse(root, preorder);
  for (unsigned int i = 0; i < copies_of_src.size(); ++i) {
    SgTreeCopy tc;
    SgExpression *copy = isSgExpression(tgt->copy(tc));
    SgExpression *replaced = copies_of_src[i];
    ROSE_ASSERT(copy != nullptr);
    SageInterface::replaceExpression(replaced, copy, true);
    published.emplace(replaced, copy);
    if (replaced != src) {
      retiredExpressions.push_back(replaced);
    }
  }
}

static void
deleteRetiredExpressions(const vector<SgExpression *> &expressions) {
  for (SgExpression *expression : expressions) {
    SageInterface::deleteAST(expression,
                             SageInterface::DeleteAstMode::kRequireIsolated);
  }
}

static bool isFiniteEvaluationRegion(SgExpression *expression) {
  ROSE_ASSERT(expression != nullptr && expression->get_type() != nullptr);
  if (SageInterface::isVolatileType(expression->get_type()))
    return false;
  for (SgType *type = expression->get_type();;) {
    if (auto *modifier = isSgModifierType(type)) {
      if (modifier->get_typeModifier().isAtomic())
        return false;
      type = modifier->get_base_type();
    } else if (auto *alias = isSgTypedefType(type)) {
      type = alias->get_base_type();
    } else if (auto *reference = isSgReferenceType(type)) {
      type = reference->get_base_type();
    } else if (auto *reference = isSgRvalueReferenceType(type)) {
      type = reference->get_base_type();
    } else {
      break;
    }
    ROSE_ASSERT(type != nullptr);
  }
  if (auto *reference = isSgVarRefExp(expression)) {
    const auto &modifier = reference->get_symbol()
                               ->get_declaration()
                               ->get_declaration()
                               ->get_declarationModifier()
                               .get_typeModifier();
    return !modifier.isAtomic() &&
           !modifier.get_constVolatileModifier().isVolatile();
  }
  if (SgExpression *source = expression->get_originalExpressionTree()) {
    ROSE_ASSERT(source != expression && source->get_parent() == expression);
    return isFiniteEvaluationRegion(source);
  }
  if (isSgValueExp(expression))
    return true;
  if (auto *cast = isSgCastExp(expression)) {
    cast->validate_semantic_conversion();
    const auto conversion = cast->get_semantic_conversion_kind();
    if (conversion == SgCastExp::e_semantic_conversion_Dynamic ||
        conversion == SgCastExp::e_semantic_conversion_UserDefinedConversion ||
        conversion == SgCastExp::e_semantic_conversion_ConstructorConversion)
      return false;
    return isFiniteEvaluationRegion(cast->get_operand());
  }
  if (auto *conditional = isSgConditionalExp(expression))
    return isFiniteEvaluationRegion(conditional->get_conditional_exp()) &&
           isFiniteEvaluationRegion(conditional->get_true_exp()) &&
           isFiniteEvaluationRegion(conditional->get_false_exp());
  if (auto *binary = isSgBinaryOp(expression))
    return isFiniteEvaluationRegion(binary->get_lhs_operand()) &&
           isFiniteEvaluationRegion(binary->get_rhs_operand());
  if (isSgMinusOp(expression) || isSgUnaryAddOp(expression) ||
      isSgNotOp(expression) || isSgBitComplementOp(expression) ||
      isSgPointerDerefExp(expression))
    return isFiniteEvaluationRegion(isSgUnaryOp(expression)->get_operand());
  return false;
}

static bool alwaysComputesFiniteExpression(SgExpression *expression,
                                           SgExpression *target) {
  ROSE_ASSERT(expression != nullptr);
  if (expression == target || expressionTreeEqual(expression, target))
    return true;
  if (auto *cast = isSgCastExp(expression)) {
    cast->validate_semantic_conversion();
    return alwaysComputesFiniteExpression(cast->get_operand(), target);
  }
  if (auto *conditional = isSgConditionalExp(expression)) {
    return alwaysComputesFiniteExpression(conditional->get_conditional_exp(),
                                          target) ||
           (alwaysComputesFiniteExpression(conditional->get_true_exp(),
                                           target) &&
            alwaysComputesFiniteExpression(conditional->get_false_exp(),
                                           target));
  }
  if (auto *binary = isSgBinaryOp(expression)) {
    if (alwaysComputesFiniteExpression(binary->get_lhs_operand(), target))
      return true;
    return !isSgAndOp(binary) && !isSgOrOp(binary) &&
           alwaysComputesFiniteExpression(binary->get_rhs_operand(), target);
  }
  if (isSgMinusOp(expression) || isSgUnaryAddOp(expression) ||
      isSgNotOp(expression) || isSgBitComplementOp(expression) ||
      isSgPointerDerefExp(expression))
    return alwaysComputesFiniteExpression(
        isSgUnaryOp(expression)->get_operand(), target);
  return false;
}

static SgType *finiteUnsignedDomain(SgType *type) {
  ROSE_ASSERT(type != nullptr);
  type = type->stripTypedefsAndModifiers();
  switch (type->variantT()) {
  case V_SgTypeInt:
  case V_SgTypeSignedInt:
  case V_SgTypeUnsignedInt:
    return SageBuilder::buildUnsignedIntType();
  case V_SgTypeLong:
  case V_SgTypeSignedLong:
  case V_SgTypeUnsignedLong:
    return SageBuilder::buildUnsignedLongType();
  case V_SgTypeLongLong:
  case V_SgTypeSignedLongLong:
  case V_SgTypeUnsignedLongLong:
    return SageBuilder::buildUnsignedLongLongType();
  case V_SgTypeSigned128bitInteger:
  case V_SgTypeUnsigned128bitInteger:
    return SageBuilder::buildUnsigned128bitIntegerType();
  default:
    return nullptr;
  }
}

static SgExpression *finiteCacheValue(SgVariableSymbol *symbol, SgType *type) {
  auto read = [symbol]() { return SageBuilder::buildVarRefExp(symbol); };
  if (type->isUnsignedType())
    return read();
  SgType *domain = symbol->get_type();
  auto *maximum = SageBuilder::buildRshiftOp(
      SageBuilder::buildBitComplementOp(
          SageBuilder::buildCastExp(SageBuilder::buildIntVal(0), domain),
          domain),
      SageBuilder::buildIntVal(1), domain);
  auto *negative = SageBuilder::buildSubtractOp(
      SageBuilder::buildMinusOp(
          SageBuilder::buildCastExp(
              SageBuilder::buildBitComplementOp(read(), domain), type),
          type),
      SageBuilder::buildCastExp(SageBuilder::buildIntVal(1), type), type);
  // Both signed casts are representable, including the minimum signed value.
  return SageBuilder::buildConditionalExp(
      SageBuilder::buildGreaterThanOp(
          read(), maximum,
          SageInterface::getBoolType(symbol->get_declaration())),
      negative, SageBuilder::buildCastExp(read(), type), type);
}

static bool finiteOperandInvariant(SgExpression *operand, SgBasicBlock *root) {
  auto symbols = SageInterface::getSymbolsUsedInExpression(operand);
  for (SgVariableSymbol *symbol : symbols) {
    SgType *type =
        symbol->get_declaration()->get_type()->stripTypedefsAndModifiers();
    if (isSgReferenceType(type) || isSgRvalueReferenceType(type))
      return false;
  }
  return !legacy::anyOfListPotentiallyModifiedIn(symbols, root);
}

// Observe operands at required evaluations. Unsigned differences avoid
// speculative source updates and signed overflow in the cache arithmetic.
static void
doFiniteDifferencingOne(SgMultiplyOp *e, SgBasicBlock *root,
                        vector<SgExpression *> &retiredExpressions) {
  SgExpression *cachedValue = e;
  SgType *domain = finiteUnsignedDomain(e->get_type());
  if (domain == nullptr)
    return;
  auto *source = SageInterface::getEnclosingSourceFile(e);
  ROSE_ASSERT(source != nullptr);
  if (source->get_Fortran_only())
    return;
  vector<SgVariableSymbol *> used_symbols =
      SageInterface::getSymbolsUsedInExpression(cachedValue);
  SgBasicBlock *context = nullptr;
  for (SgNode *owner = e->get_parent(); owner != nullptr;
       owner = owner->get_parent()) {
    if (auto *block = isSgBasicBlock(owner)) {
      bool operandsVisible = true;
      for (SgVariableSymbol *symbol : used_symbols) {
        ROSE_ASSERT(symbol != nullptr && symbol->get_declaration() != nullptr);
        SgScopeStatement *scope = symbol->get_declaration()->get_scope();
        ROSE_ASSERT(scope != nullptr);
        operandsVisible &=
            scope == block || SageInterface::isAncestor(scope, block);
      }
      if (operandsVisible)
        context = block;
    }
    if (owner == root)
      break;
  }
  // A cache requires a block in which every operand's declaration is visible.
  if (context == nullptr)
    return;
  root = context;
  SgStatementPtrList &root_stmts = root->get_statements();
  SgNode *firstUse = e;
  while (firstUse->get_parent() != root) {
    firstUse = firstUse->get_parent();
    ROSE_ASSERT(firstUse != nullptr);
  }
  auto i = std::find(root_stmts.begin(), root_stmts.end(), firstUse);
  ROSE_ASSERT(i != root_stmts.end());
  vector<SgExpression *> occurrences;
  FdFindCopiesVisitor(e, occurrences).traverse(root, preorder);
  set<SgExpression *> evaluationRoots;
  for (SgExpression *occurrence : occurrences) {
    SgExpression *evaluationRoot = occurrence;
    while (auto *parent = isSgExpression(evaluationRoot->get_parent())) {
      if (isSgInitializer(parent))
        break;
      evaluationRoot = parent;
    }
    if (!isFiniteEvaluationRegion(evaluationRoot) ||
        !alwaysComputesFiniteExpression(evaluationRoot, e) ||
        legacy::anyOfListPotentiallyModifiedIn(used_symbols, evaluationRoot))
      return;
    evaluationRoots.insert(evaluationRoot);
  }
  bool leftInvariant = finiteOperandInvariant(e->get_lhs_operand(), root);
  bool rightInvariant = finiteOperandInvariant(e->get_rhs_operand(), root);
  if (leftInvariant && rightInvariant)
    rightInvariant = false;
  SgName cachename = "cache_fd__";
  cachename << ++SageInterface::gensym_counter;
  SgVariableDeclaration *cachedecl = SageBuilder::buildVariableDeclaration(
      cachename, domain,
      SageBuilder::buildAssignInitializer(
          SageBuilder::buildCastExp(SageBuilder::buildIntVal(0), domain),
          domain),
      root);
  SgInitializedName *cachevar = cachedecl->get_variables().back();
  ROSE_ASSERT(cachevar);
  cachevar->set_scope(root);
  SageInterface::insertStatementBefore(*i, cachedecl, false);
  SgVariableSymbol *sym = root->lookup_variable_symbol(cachename);
  if (sym == NULL || sym->get_declaration() != cachevar) {
    fprintf(stderr,
            "REX_AST_INVARIANT[finite-differencing-cache-owner]: generated "
            "cache declaration has no exact published symbol\n");
    ROSE_ABORT();
  }
  SgStatement *lastDeclaration = cachedecl;
  auto snapshot = [&](const char *suffix) {
    SgName name = cachename;
    name << suffix;
    auto *declaration = SageBuilder::buildVariableDeclaration(
        name, domain,
        SageBuilder::buildAssignInitializer(
            SageBuilder::buildCastExp(SageBuilder::buildIntVal(0), domain),
            domain),
        root);
    SageInterface::insertStatementAfter(lastDeclaration, declaration, false);
    lastDeclaration = declaration;
    auto *symbol = SageInterface::getFirstVarSym(declaration);
    ROSE_ASSERT(symbol != nullptr);
    return symbol;
  };
  SgVariableSymbol *previousLeft = leftInvariant ? nullptr : snapshot("_lhs");
  SgVariableSymbol *previousRight = rightInvariant ? nullptr : snapshot("_rhs");
  auto *cacheRead = finiteCacheValue(sym, cachedValue->get_type());
  map<SgExpression *, SgExpression *> published;
  replaceCopiesOfExpression(e, cacheRead, root, retiredExpressions, published);
  SageInterface::deleteAST(cacheRead,
                           SageInterface::DeleteAstMode::kRequireIsolated);
  ROSE_ASSERT(cachedValue->get_parent() == nullptr);
  for (SgExpression *evaluationRoot : evaluationRoots) {
    auto replacement = published.find(evaluationRoot);
    if (replacement != published.end())
      evaluationRoot = replacement->second;
    auto operand = [&](SgExpression *expression) {
      auto *copy = isSgExpression(SageInterface::deepCopyNode(expression));
      ROSE_ASSERT(copy != nullptr && copy->get_parent() == nullptr);
      return SageBuilder::buildCastExp(copy, domain);
    };
    auto left = [&]() { return operand(e->get_lhs_operand()); };
    auto right = [&]() { return operand(e->get_rhs_operand()); };
    SgExpression *difference = nullptr;
    if (previousLeft != nullptr) {
      SgExpression *rightFactor;
      if (previousRight != nullptr)
        rightFactor = SageBuilder::buildVarRefExp(previousRight);
      else
        rightFactor = right();
      difference = SageBuilder::buildMultiplyOp(
          SageBuilder::buildSubtractOp(
              left(), SageBuilder::buildVarRefExp(previousLeft), domain),
          rightFactor, domain);
    }
    if (previousRight != nullptr) {
      auto *term = SageBuilder::buildMultiplyOp(
          left(),
          SageBuilder::buildSubtractOp(
              right(), SageBuilder::buildVarRefExp(previousRight), domain),
          domain);
      if (difference != nullptr)
        difference = SageBuilder::buildAddOp(difference, term, domain);
      else
        difference = term;
    }
    ROSE_ASSERT(difference != nullptr);
    // cache += (left-oldLeft)*oldRight + left*(right-oldRight).
    // An invariant operand needs neither a snapshot nor its zero difference.
    SgExpression *update = SageBuilder::buildPlusAssignOp(
        SageBuilder::buildVarRefExp(sym), difference, domain);
    if (previousLeft != nullptr)
      update = SageBuilder::buildCommaOpExp(
          update,
          SageBuilder::buildAssignOp(SageBuilder::buildVarRefExp(previousLeft),
                                     left(), domain),
          domain);
    if (previousRight != nullptr)
      update = SageBuilder::buildCommaOpExp(
          update,
          SageBuilder::buildAssignOp(SageBuilder::buildVarRefExp(previousRight),
                                     right(), domain),
          domain);
    auto *retained =
        isSgExpression(SageInterface::deepCopyNode(evaluationRoot));
    ROSE_ASSERT(retained != nullptr && retained->get_parent() == nullptr);
    SageInterface::replaceExpression(evaluationRoot, retained, true);
    auto *sequenced = SageBuilder::buildCommaOpExp(update, evaluationRoot,
                                                   evaluationRoot->get_type());
    SageInterface::replaceExpression(retained, sequenced, true);
    retiredExpressions.push_back(retained);
  }

  PreprocessingInfo *cacheComment = new PreprocessingInfo(
      PreprocessingInfo::CplusplusStyleComment,
      (string("// Finite differencing: ") + cachename.str() +
       " is an exact generated expression cache")
          .c_str(),
      "Compiler-Generated in Finite Differencing", 0, 0, 0,
      PreprocessingInfo::before);
  ROSE_ASSERT(cacheComment->get_file_info() != NULL);
  cacheComment->get_file_info()->set_physical_file_id(
      Sg_File_Info::NULL_FILE_ID);
  SageInterface::publishGeneratedPreprocessingInfo(cacheComment, cachedecl);
  cachedecl->addToAttachedPreprocessingInfo(cacheComment);

  retiredExpressions.push_back(cachedValue);
}

class FindForStatementsVisitor : public AstSimpleProcessing {
  vector<SgForStatement *> &stmts;

public:
  FindForStatementsVisitor(vector<SgForStatement *> &stmts) : stmts(stmts) {}

  virtual void visit(SgNode *n) {
    if (isSgForStatement(n))
      stmts.push_back(isSgForStatement(n));
  }
};

// Move variables declared in a for statement to just outside that statement.
static void moveForDeclaredVariables(SgNode *root) {
  vector<SgForStatement *> for_statements;
  FindForStatementsVisitor(for_statements).traverse(root, preorder);

  for (unsigned int i = 0; i < for_statements.size(); ++i) {
    SgForStatement *stmt = for_statements[i];
    SgForInitStatement *init = stmt->get_for_init_stmt();
    ROSE_ASSERT(init != nullptr && init->get_parent() == stmt);
    SgStatementPtrList &inits = init->get_init_stmt();
    vector<SgVariableDeclaration *> decls;
    bool variableInitializers = true;
    for (SgStatement *statement : inits) {
      if (auto *declaration = isSgVariableDeclaration(statement)) {
        decls.push_back(declaration);
      } else if (auto *group = isSgDeclarationGroupStatement(statement)) {
        group->validate();
        for (SgDeclarationStatement *member : group->get_declarations()) {
          auto *declaration = isSgVariableDeclaration(member);
          if (declaration != nullptr)
            decls.push_back(declaration);
          else
            variableInitializers = false;
        }
      }
    }
    if (decls.empty() || !variableInitializers)
      continue;
    SgStatement *parent = isSgStatement(stmt->get_parent());
    assert(parent);
    SgBasicBlock *bb = SageBuilder::buildBasicBlock_nfi();
    SageInterface::setSourcePositionForTransformation(bb);
    SgStatementPtrList initialStatements = inits;
    for (unsigned int j = 0; j < decls.size(); ++j) {
      for (SgInitializedNamePtrList::iterator k =
               decls[j]->get_variables().begin();
           k != decls[j]->get_variables().end(); ++k) {
        SgInitializedName *name = *k;
        auto *sym = isSgVariableSymbol(name->get_symbol_from_symbol_table());
        ROSE_ASSERT(sym != nullptr && sym->get_declaration() == name);
        SgScopeStatement *old_scope = name->get_scope();
        ROSE_ASSERT(old_scope != nullptr &&
                    old_scope->get_symbol_table()->exists(sym));
        old_scope->remove_symbol(sym);
        bb->insert_symbol(name->get_name(), sym);
        sym->set_parent(bb->get_symbol_table());
        name->set_scope(bb);
      }
    }
    inits.clear();
    init->append_init_stmt(SageBuilder::buildNullStatement());
    SageInterface::setOneSourcePositionForTransformation(init);
    // Replace the original loop while its physical parent relation is still
    // intact, then move the declarations and loop into the new transformation
    // block.  Reparenting the loop before replacement left a transiently
    // malformed tree that legacy fixup happened to repair only in some cases.
    parent->replace_statement(stmt, bb);
    if (bb->get_parent() != parent) {
      fprintf(stderr,
              "REX_FINITE_DIFFERENCING_INVARIANT[loop-wrapper]: parent=%p/%s "
              "loop=%p replacement=%p was not installed exactly once\n",
              static_cast<void *>(parent), parent->class_name().c_str(),
              static_cast<void *>(stmt), static_cast<void *>(bb));
      ROSE_ABORT();
    }
    for (SgStatement *statement : initialStatements) {
      bb->append_statement(statement);
      if (auto *group = isSgDeclarationGroupStatement(statement))
        group->set_scope(bb);
    }
    bb->append_statement(stmt);
    // The original loop retains the exact source-file identity for the
    // replaced surface.  Publish that identity onto the generated wrapper and
    // declarations as one explicit construction transaction; transformation
    // file information alone is deliberately not an output-owner fallback.
    SageInterface::publishGeneratedSubtreeOutputOwner(bb, stmt);
    if (stmt->get_parent() != bb) {
      fprintf(stderr,
              "REX_FINITE_DIFFERENCING_INVARIANT[loop-wrapper]: loop=%p has "
              "no exact generated block owner\n",
              static_cast<void *>(stmt));
      ROSE_ABORT();
    }
  }
}

class FdFindFunctionsVisitor : public AstSimpleProcessing {
public:
  vector<SgFunctionDefinition *> functions;

  virtual void visit(SgNode *n) {
    if (isSgFunctionDefinition(n)) {
      functions.push_back(isSgFunctionDefinition(n));
    }
  }
};

class FdFindInitnamesVisitor : public AstSimpleProcessing {
public:
  vector<SgInitializedName *> initnames;

  virtual void visit(SgNode *n) {
    if (isSgInitializedName(n)) {
      initnames.push_back(isSgInitializedName(n));
    }
  }
};

class FdFindMultiplicationsVisitor : public AstSimpleProcessing {
public:
  vector<SgMultiplyOp *> exprs;

  virtual void visit(SgNode *n) {
    if (isSgMultiplyOp(n)) {
      exprs.push_back(isSgMultiplyOp(n));
    }
  }
};

namespace {
SgExpression *finiteDifferencingSemanticOperand(SgExpression *expression) {
  ROSE_ASSERT(expression != nullptr);
  while (SgCastExp *cast = isSgCastExp(expression)) {
    cast->validate_semantic_conversion();
    if (cast->cast_type() != SgCastExp::e_implicit_cast) {
      break;
    }
    SgExpression *operand = cast->get_operand();
    if (operand == nullptr || operand->get_parent() != cast) {
      fprintf(stderr,
              "REX_FINITE_DIFFERENCING_INVARIANT[implicit-conversion]: "
              "cast=%p has no exact operand ownership\n",
              static_cast<void *>(cast));
      ROSE_ABORT();
    }
    expression = operand;
  }
  return expression;
}

} // namespace

class IsModifiedBadlyVisitor : public AstSimpleProcessing {
  SgInitializedName *initname;
  bool &safe;

public:
  IsModifiedBadlyVisitor(SgInitializedName *initname, bool &safe)
      : initname(initname), safe(safe) {}

  virtual void visit(SgNode *n) {
    SgVarRefExp *vr = isSgVarRefExp(n);
    if (vr && vr->get_symbol()->get_declaration() == initname) {
      SgNode *semanticParent =
          finiteDifferencingOutermostImplicitConversion(vr)->get_parent();
      ROSE_ASSERT(semanticParent != nullptr);
      switch (semanticParent->variantT()) {
      case V_SgReturnStmt:
      case V_SgExprStatement:
      case V_SgIfStmt:
      case V_SgWhileStmt:
      case V_SgDoWhileStmt:
      case V_SgSwitchStatement:
      case V_SgCaseOptionStmt:
      case V_SgForStatement:
      case V_SgForInitStatement:
        break;

      case V_SgPlusPlusOp:
      case V_SgMinusMinusOp:
        break;

      case V_SgAddOp:
      case V_SgSubtractOp:
      case V_SgMinusOp:
      case V_SgUnaryAddOp:
      case V_SgNotOp:
      case V_SgPointerDerefExp:
      case V_SgBitComplementOp:
      case V_SgThrowOp:
      case V_SgEqualityOp:
      case V_SgLessThanOp:
      case V_SgLessOrEqualOp:
      case V_SgGreaterThanOp:
      case V_SgGreaterOrEqualOp:
      case V_SgNotEqualOp:
      case V_SgMultiplyOp:
      case V_SgDivideOp:
      case V_SgIntegerDivideOp:
      case V_SgModOp:
      case V_SgAndOp:
      case V_SgOrOp:
      case V_SgBitAndOp:
      case V_SgBitOrOp:
      case V_SgBitXorOp:
      case V_SgCommaOpExp:
      case V_SgLshiftOp:
      case V_SgRshiftOp:
      case V_SgAssignInitializer:
        break;

      case V_SgExprListExp:
        if (isSgFunctionCallExp(semanticParent->get_parent())) {
          if (isPotentiallyModified(vr, semanticParent->get_parent())) {
            safe = false;
          }
        } else if (isSgConstructorInitializer(semanticParent->get_parent())) {
          safe = false;
        } else {
          cerr << semanticParent->get_parent()->sage_class_name() << endl;
          assert(!"Unknown SgExprListExp case");
        }
        break;

      case V_SgAssignOp:
      case V_SgPlusAssignOp:
      case V_SgMinusAssignOp:
      case V_SgMultAssignOp:
      case V_SgDivAssignOp:
      case V_SgModAssignOp:
      case V_SgAndAssignOp:
      case V_SgIorAssignOp:
      case V_SgXorAssignOp:
      case V_SgLshiftAssignOp:
      case V_SgRshiftAssignOp:
        break;

      default: {
        safe = false;
      } break;
      }
    }
  }
};

// Do a simple form of finite differencing on all functions contained within
// root (which should be a project, file, or function definition).
void simpleIndexFiniteDifferencing(SgNode *root) {
  moveForDeclaredVariables(root);
  SgNode *proj = root;
  while (proj && !isSgProject(proj))
    proj = proj->get_parent();
  ROSE_ASSERT(proj);
  AstTests::runAllTests(isSgProject(proj));
  std::unique_ptr<RewriteRule> algebraicRules(getAlgebraicRules());
  rewrite(algebraicRules.get(), root); // This might modify root
  FdFindFunctionsVisitor ffv;
  ffv.traverse(root, preorder);
  for (unsigned int x = 0; x < ffv.functions.size(); ++x) {
    SgBasicBlock *body = ffv.functions[x]->get_body();
    FdFindInitnamesVisitor fiv;
    fiv.traverse(ffv.functions[x]->get_declaration(), preorder);
    vector<SgInitializedName *> initnames = fiv.initnames;
    set<SgInitializedName *> safe_vars;
    for (vector<SgInitializedName *>::iterator i = initnames.begin();
         i != initnames.end(); ++i) {
      SgInitializedName *initname = *i;
      bool safe = true;
      IsModifiedBadlyVisitor(initname, safe).traverse(body, preorder);
      if (safe)
        safe_vars.insert(initname);
    }
    auto eligibleOperand = [&](SgExpression *operand) {
      operand = finiteDifferencingSemanticOperand(operand);
      if (isSgValueExp(operand))
        return true;
      auto *reference = isSgVarRefExp(operand);
      if (reference == nullptr)
        return false;
      SgInitializedName *declaration =
          reference->get_symbol()->get_declaration();
      SgType *type = declaration->get_type()->stripTypedefsAndModifiers();
      return !isSgReferenceType(type) && !isSgRvalueReferenceType(type) &&
             safe_vars.count(declaration) != 0;
    };
    FdFindMultiplicationsVisitor fmv;
    fmv.traverse(body, postorder);
    vector<SgMultiplyOp *> mult_exprs;
    for (unsigned int i = 0; i < fmv.exprs.size(); ++i) {
      if (SageInterface::getEnclosingFunctionDefinition(fmv.exprs[i]) !=
          ffv.functions[x])
        continue;
      bool alreadyProcessed = false;
      for (SgMultiplyOp *previous : mult_exprs) {
        if (expressionTreeEqual(fmv.exprs[i], previous)) {
          alreadyProcessed = true;
          break;
        }
      }
      if (alreadyProcessed)
        continue;
      if (eligibleOperand(fmv.exprs[i]->get_lhs_operand()) &&
          eligibleOperand(fmv.exprs[i]->get_rhs_operand()) &&
          isFiniteEvaluationRegion(fmv.exprs[i])) {
        mult_exprs.push_back(fmv.exprs[i]);
      }
    }
    vector<SgExpression *> retiredExpressions;
    for (int i = mult_exprs.size() - 1; i >= 0; --i)
      doFiniteDifferencingOne(mult_exprs[i], body, retiredExpressions);
    deleteRetiredExpressions(retiredExpressions);

    SgNode *bodyCopyForRewrite = body;
    std::unique_ptr<RewriteRule> bodyAlgebraicRules(getAlgebraicRules());
    rewrite(bodyAlgebraicRules.get(),
            bodyCopyForRewrite); // This might update bodyCopyForRewrite
    ROSE_ASSERT(isSgBasicBlock(bodyCopyForRewrite));
    body = isSgBasicBlock(bodyCopyForRewrite);
  }
}
