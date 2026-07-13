/**
 *  \file Block.cc
 *  \brief Preprocessor phase to convert a statement into an
 *  SgBasicBlock.
 */
// tps (01/14/2010) : Switching from rose.h to sage3.
#include "sage3basic.h"

#include "sageBuilder.h"

#include <iostream>

#include <list>

#include <limits>

#include <string>

#include "ASTtools.hh"

#include "Copy.hh"

#include "Preprocess.hh"

#include "PreprocessingInfo.hh"

#include "StmtRewrite.hh"

// =====================================================================

using namespace std;

// =====================================================================

/*!
 *  If the given initialized name (located in the given scope) has an
 *  initializer, returns a new expression statement equivalent to the
 *  initializer.
 *
 *  \pre 'name' must correspond to a variable declaration in 'scope'.
 *
 *  \returns The new statement, or 0 if none the initialized name does
 *  not have an initializer.
 *
 *  \todo Currently works only for SgAssignInitializers on
 *  non-reference primitive types. SgAggregateInitializers will return
 *  0.
 */
static SgExprStatement *createAssignment(const SgInitializedName *name,
                                         const SgScopeStatement *scope) {
  if (!name)
    return 0;

  const SgAssignInitializer *rhs =
      isSgAssignInitializer(name->get_initializer());
  if (!rhs)
    return 0;

  // Has an assign initializer (rhs). If 'name's' type is a reference,
  // keep the initializer.
  if (isSgReferenceType(name->get_type()))
    return 0;

  const SgType *rhs_type = rhs->get_type();
  if (!rhs_type || ASTtools::isConstObj(name->get_type()) ||
      isSgClassType(rhs_type) ||
      (isSgModifierType(rhs_type) &&
       isSgClassType(isSgModifierType(rhs_type)->get_base_type())))
    return 0;

  // Build RHS
  SgExpression *rhs_op = isSgExpression(ASTtools::deepCopy(rhs->get_operand()));
  ROSE_ASSERT(rhs_op);

  // Build LHS (i.e., variable reference)
  ROSE_ASSERT(scope);
  SgVariableSymbol *v_sym =
      const_cast<SgScopeStatement *>(scope)->lookup_var_symbol(
          name->get_name());
  ROSE_ASSERT(v_sym);
  SgVarRefExp *v = SageBuilder::buildVarRefExp(v_sym);
  ROSE_ASSERT(v);

  // Build assignment expression
  SgAssignOp *assign_op = SageBuilder::buildAssignOp(v, rhs_op, v->get_type());
  ROSE_ASSERT(assign_op);

  // Build expression statement
  SgExprStatement *expr_stmt = SageBuilder::buildExprStatement(assign_op);
  ROSE_ASSERT(expr_stmt);

  // Done
  return expr_stmt;
}

/*!
 *  If the given initialized name has an initializer, create an
 *  expression statement equivalent to the initializer and append that
 *  statement to the target basic block.
 *
 *  \pre 'name' must correspond to a variable declaration in 'scope'.
 *
 *  \returns true if an initializer exists, and false otherwise.
 *
 *  \todo See TODO for \ref createAssignment().
 */
static bool appendAssignment(const SgInitializedName *name,
                             const SgScopeStatement *scope,
                             SgBasicBlock *target) {
  if (target) {
    SgExprStatement *assign = createAssignment(name, scope);
    if (assign) {
      target->append_statement(assign);
      return true;
    }
  }
  return false; // default: no assignment needed
}

namespace {
struct SourceExtent {
  std::string filename;
  int first_line = std::numeric_limits<int>::max();
  int last_line = 0;
};

bool isUsableSourceInfo(const Sg_File_Info *info) {
  return info != NULL && info->get_line() > 0 &&
         info->get_filenameString() != "NULL_FILE" &&
         !info->isCompilerGenerated() && !info->isTransformation();
}

void updateSourceExtent(const Sg_File_Info *info, SourceExtent &extent) {
  if (!isUsableSourceInfo(info)) {
    return;
  }

  const std::string filename = info->get_filenameString();
  if (extent.filename.empty()) {
    extent.filename = filename;
  }
  if (extent.filename != filename) {
    return;
  }

  const int line = info->get_line();
  extent.first_line = std::min(extent.first_line, line);
  extent.last_line = std::max(extent.last_line, line);
}

SourceExtent getSourceExtent(SgStatement *statement) {
  SourceExtent extent;
  if (SgLocatedNode *located = isSgLocatedNode(statement)) {
    updateSourceExtent(located->get_file_info(), extent);
    updateSourceExtent(located->get_startOfConstruct(), extent);
    updateSourceExtent(located->get_endOfConstruct(), extent);
  }

  Rose_STL_Container<SgNode *> nodes =
      NodeQuery::querySubTree(statement, V_SgNode);
  for (Rose_STL_Container<SgNode *>::iterator i = nodes.begin();
       i != nodes.end(); ++i) {
    SgLocatedNode *located = isSgLocatedNode(*i);
    if (located == NULL) {
      continue;
    }

    updateSourceExtent(located->get_file_info(), extent);
    updateSourceExtent(located->get_startOfConstruct(), extent);
    updateSourceExtent(located->get_endOfConstruct(), extent);
  }

  return extent;
}

bool hasValidExtent(const SourceExtent &extent) {
  return !extent.filename.empty() && extent.first_line <= extent.last_line;
}

bool isWithinExtent(const PreprocessingInfo *info, const SourceExtent &extent) {
  if (info == NULL || !hasValidExtent(extent)) {
    return false;
  }

  const Sg_File_Info *file_info = info->get_file_info();
  if (!isUsableSourceInfo(file_info) ||
      file_info->get_filenameString() != extent.filename) {
    return false;
  }

  const int line = file_info->get_line();
  return line >= extent.first_line && line <= extent.last_line;
}

void moveParentInsidePreprocInfoToWrapper(SgStatement *statement,
                                          SgStatement *parent,
                                          SgBasicBlock *wrapper) {
  ROSE_ASSERT(statement != NULL);
  ROSE_ASSERT(parent != NULL);
  ROSE_ASSERT(wrapper != NULL);

  AttachedPreprocessingInfoType *parent_info =
      parent->get_attachedPreprocessingInfoPtr();
  if (parent_info == NULL || parent_info->empty()) {
    return;
  }

  const SourceExtent extent = getSourceExtent(statement);
  if (!hasValidExtent(extent)) {
    return;
  }

  AttachedPreprocessingInfoType moved_info;
  int open_if_depth = 0;
  bool moving_if_group = false;

  for (AttachedPreprocessingInfoType::iterator i = parent_info->begin();
       i != parent_info->end(); ++i) {
    PreprocessingInfo *info = *i;
    bool move_info = false;

    if (ASTtools::isPositionInside(info)) {
      move_info = moving_if_group || isWithinExtent(info, extent);

      if (move_info) {
        if (ASTtools::isIfDirectiveBegin(info)) {
          ++open_if_depth;
          moving_if_group = true;
        } else if (ASTtools::isIfDirectiveEnd(info) && moving_if_group) {
          if (open_if_depth > 0) {
            --open_if_depth;
          }
          moving_if_group = open_if_depth > 0;
        }
      }
    }

    if (move_info) {
      moved_info.push_back(info);
    }
  }

  if (moved_info.empty()) {
    return;
  }

  for (PreprocessingInfo *info : moved_info) {
    SageInterface::publishPreprocessingInfoPhysicalOutputOwner(info, wrapper);
    parent->transferPreprocessingInfo(
        info, wrapper, info->getRelativePosition(),
        SgLocatedNode::PreprocessingInfoInsertion::back);
  }
}

} // namespace

SgBasicBlock *Outliner::Preprocess::normalizeVarDecl(SgVariableDeclaration *s) {
  if (!s)
    return 0;

  // Verify at least one variable exists.
  SgInitializedNamePtrList &vars_orig = s->get_variables();
  SgInitializedNamePtrList::iterator i = vars_orig.begin();
  ROSE_ASSERT(i != vars_orig.end());

  // Prepare new basic block to contain initializers.
  SgBasicBlock *assigns_new = SageBuilder::buildBasicBlock();
  SgScopeStatement *s_scope = s->get_scope();
  ROSE_ASSERT(s_scope);

  do {
    if (appendAssignment(*i, s_scope, assigns_new)) {
      SgInitializer *initializer = (*i)->get_initializer();
      (*i)->set_initializer(NULL);
      if (initializer != NULL) {
        SageInterface::deleteAST(initializer);
      }
    }
    ++i;
  } while (i != vars_orig.end());

  // Insert block of assignments after the variable declaration.
  SageInterface::insertStatementAfter(s, assigns_new);
  return assigns_new;
}

/*!
 *  \brief Convert the "plain-old" statement into an SgBasicBlock.
 *  This normalization simplifies outlining of single statements.
 */
SgBasicBlock *Outliner::Preprocess::createBlock(SgStatement *s) {
  SgStatement *s_outline = s;
  if (!isSgBasicBlock(s)) {
    SgBasicBlock *b_new = SageBuilder::buildBasicBlock();
    ROSE_ASSERT(b_new);
    SgStatement *parent = isSgStatement(s->get_parent());
    ROSE_ASSERT(parent);
    SageInterface::replaceStatement(s, b_new);
    moveParentInsidePreprocInfoToWrapper(s, parent, b_new);
    ASTtools::moveUpPreprocInfo(b_new, s);
    // insert s to b_new
    SageInterface::appendStatement(s, b_new);
    s_outline = b_new;
  }
  return isSgBasicBlock(s_outline);
}

// eof
