/*!
 *  \file PreprocIfs.cc
 *
 *  \brief Preprocessor phase to break-up any preprocessing control
 *  structure that may straddle the boundaries of a basic block.
 *
 *  \author Richard Vuduc <richie@llnl.gov>
 */
// tps (01/14/2010) : Switching from rose.h to sage3.
#include "sage3basic.h"

#include "sageInterface.h"

#include <algorithm>

#include <iostream>

#include <list>

#include <string>

#include "ASTtools.hh"

#include "If.hh"

#include "Preprocess.hh"

#include "PreprocessingInfo.hh"

#include "StmtRewrite.hh"

//! Relative positions at which '#if' cases may be inserted.
enum ContextPosType {
  e_beforeBlock,
  e_firstInBlock,
  e_lastInBlock,
  e_beforeLastInBlock
};

//! Insert '#if' directives to close a context.
static void closeContext(const CPreproc::If::Case *, ContextPosType,
                         SgBasicBlock *);

//! Insert '#if' directives to open a context.
static void openContext(const CPreproc::If::Case *, ContextPosType,
                        SgBasicBlock *);

//! Stores a list of preprocessing directives.
typedef Rose_STL_Container<PreprocessingInfo *> PreprocInfoList_t;

// =====================================================================

using namespace std;

// =====================================================================

namespace {
class IfsOwner {
public:
  explicit IfsOwner(CPreproc::Ifs_t &ifs) : ifs_(ifs) {}
  ~IfsOwner() {
    for (CPreproc::If *directive : ifs_) {
      delete directive;
    }
  }

private:
  CPreproc::Ifs_t &ifs_;
};
} // namespace

// =====================================================================

SgBasicBlock *Outliner::Preprocess::transformPreprocIfs(SgBasicBlock *b) {
  ROSE_ASSERT(b && b->get_parent());

  // Determine the '#if' directive context at 'b'.
  CPreproc::Ifs_t ifs;
  IfsOwner ifsOwner(ifs);
  CPreproc::If::Case *top = 0;
  CPreproc::If::Case *bottom = 0;
  CPreproc::findIfDirectiveContext(b, ifs, top, bottom);

  // debugging
  //   CPreproc::dump(ifs);
  //  if(top) top->dump();
  //  if(bottom) bottom->dump();

  if (!top && !bottom) // Not guarded by any '#if'.
    return b;
  // move the source block b to be a nested block inside a shell block b_new
  SgBasicBlock *b_new = ASTtools::transformToBlockShell(b);
  ROSE_ASSERT(b_new);

  const bool target_stays_in_same_context = top != NULL && top == bottom;
  if (!target_stays_in_same_context)
    closeContext(top, e_beforeBlock, b_new);
  openContext(top, e_firstInBlock, b_new);

  closeContext(bottom, e_lastInBlock, b_new);
  if (!target_stays_in_same_context)
    openContext(bottom, e_beforeLastInBlock, b);

  return b_new;
}

// =====================================================================

//! Sanitizes a string for insertion into a C-style comment.
static string makeCommentSafe(const string &s) {
  string s_new(s);
  string::size_type pos;

  while ((pos = s_new.find("/*")) != string::npos)
    s_new.erase(pos, 2);
  while ((pos = s_new.find("*/")) != string::npos)
    s_new.erase(pos, 2);
  while ((pos = s_new.find("*/")) != string::npos)
    s_new.erase(pos, 2);

  return s_new;
}

/*!
 *  \brief Returns the '#if' case of the parent directive of the given
 *  case, if any.
 *
 *  For example, let 'c' be as marked below:
 *
 *    #if A
 *      // ...
 *    #elif B  // Associated with some case, 'b'
 *      // ...
 *      #if C   // Associated with input case 'c'
 *
 *  Then, this routine returns the case 'b'.
 */
static const CPreproc::If::Case *getParentCase(const CPreproc::If::Case *c) {
  const CPreproc::If *c_if = c->getIf();
  ROSE_ASSERT(c_if);
  const CPreproc::If::Case *c_if_parent = c_if->getParent();
  return c_if_parent ? c_if_parent : 0;
}

/*!
 *  \brief Returns the '#if ...' that opens the directive to which
 *  this case belongs.
 *
 *  For example, let 'c' be as marked below:
 *
 *    #if A  // Associated with some case, 'a'
 *      // ...
 *    #elif B
 *      // ...
 *    #elif C   // Associated with input case, 'c'
 *      // ...
 *
 *  Then, this routine returns 'a'.
 *
 *  \returns A non-NULL result, given a non-NULL input case.
 */
static const CPreproc::If::Case *getOpenCase(const CPreproc::If::Case *c) {
  if (c) {
    const CPreproc::If *c_if = c->getIf();
    ROSE_ASSERT(c_if);
    const CPreproc::If::Case *c_open = c_if->firstCase();
    ROSE_ASSERT(c_open);
    return c_open;
  }
  return 0;
}

/*!
 *  \brief Generates PreprocessingInfo objects for use when 'closing'
 *  an '#if' directive context.
 */
static void genCloseDirectives(const CPreproc::If::Case *c,
                               PreprocessingInfo::RelativePositionType pos,
                               PreprocInfoList_t &D) {
  if (c) {
    // Close 'c'.

    // Build a debugging comment so we can see to whom this new
    // '#endif' belongs.
    stringstream s_closer;
    const CPreproc::If::Case *c_open = getOpenCase(c);
    ROSE_ASSERT(c_open);
    if (c_open != c)
      s_closer << c_open->getDirective() << ' ' << c_open->getCondition()
               << " ... ";

    s_closer << c->getDirective() << ' ' << c->getCondition();

    // Build preprocessing info object for this string.
    string closer = "#endif /* " + makeCommentSafe(s_closer.str()) + " */\n";
    PreprocessingInfo *d = new PreprocessingInfo(
        PreprocessingInfo::CpreprocessorEndifDeclaration, closer,
        string("transformation"), 0, 0, 1 /* no. of lines */, pos);
    ROSE_ASSERT(d);
    ROSE_ASSERT(d->get_file_info() != NULL);
    d->get_file_info()->set_physical_file_id(Sg_File_Info::NULL_FILE_ID);

    D.push_back(d);

    // Close all parents.
    genCloseDirectives(getParentCase(c), pos, D);
  }
}

/*!
 *  \brief Generates PreprocessingInfo objects for use when 'opening'
 *  an '#if' directive context.
 */
static void genOpenDirectives(const CPreproc::If::Case *c,
                              PreprocessingInfo::RelativePositionType pos,
                              PreprocInfoList_t &D) {
  if (c) {
    // Open all parents.
    genOpenDirectives(getParentCase(c), pos, D);

    stringstream s_opener;
    const CPreproc::If::Case *c_open = getOpenCase(c);
    ROSE_ASSERT(c_open);
    if (c_open != c)
      s_opener << c_open->getDirective() << ' ' << c_open->getCondition()
               << " ... ";
    s_opener << c->getDirective() << ' ' << c->getCondition();

    string opener = "#if 1 /* " + makeCommentSafe(s_opener.str()) + "*/\n";
    PreprocessingInfo *d = new PreprocessingInfo(
        PreprocessingInfo::CpreprocessorIfDeclaration, opener,
        string("transformation"), 0, 0, 1 /* no. of lines */, pos);

    ROSE_ASSERT(d);
    ROSE_ASSERT(d->get_file_info() != NULL);
    d->get_file_info()->set_physical_file_id(Sg_File_Info::NULL_FILE_ID);

    d->setRelativePosition(pos);
    D.push_back(d);
  }
}

/*!
 *  \brief Interprets (converts) an open/close context's relative
 *  position into a PreprocessingInfo::RelativePositionType position.
 */
static PreprocessingInfo::RelativePositionType getRelPos(ContextPosType pos) {
  switch (pos) {
  case e_beforeBlock:
  case e_firstInBlock:
    return PreprocessingInfo::before;
  case e_lastInBlock:
  case e_beforeLastInBlock:
    return PreprocessingInfo::inside;
  default:
    fprintf(stderr,
            "REX_AST_INVARIANT[outliner-preprocessing-position]: invalid "
            "context position=%d\n",
            static_cast<int>(pos));
    ROSE_ABORT();
  }
}

static void publishDirectives(PreprocInfoList_t &directives,
                              SgLocatedNode *owner) {
  ROSE_ASSERT(owner != NULL);
  for (PreprocessingInfo *directive : directives) {
    ROSE_ASSERT(directive != NULL);
    SageInterface::publishGeneratedPreprocessingInfo(directive, owner);
  }
}

static void attachDirectivesAtBack(PreprocInfoList_t &directives,
                                   SgLocatedNode *owner) {
  publishDirectives(directives, owner);
  for (PreprocessingInfo *directive : directives) {
    owner->attachPreprocessingInfo(
        directive, directive->getRelativePosition(),
        SgLocatedNode::PreprocessingInfoInsertion::back);
  }
  directives.clear();
}

static void attachDirectivesAtFront(PreprocInfoList_t &directives,
                                    SgLocatedNode *owner) {
  publishDirectives(directives, owner);
  for (PreprocInfoList_t::reverse_iterator directive = directives.rbegin();
       directive != directives.rend(); ++directive) {
    owner->attachPreprocessingInfo(
        *directive, (*directive)->getRelativePosition(),
        SgLocatedNode::PreprocessingInfoInsertion::front);
  }
  directives.clear();
}

/*!
 *  \brief Inserts preprocessing info at the
 *  'PreprocessingInfo::inside' position of the given target
 *  statement.
 */
static void insertMiddle(PreprocInfoList_t &D, SgStatement *s) {
  publishDirectives(D, s);
  AttachedPreprocessingInfoType *info = s->getAttachedPreprocessingInfo();
  PreprocessingInfo *anchor = NULL;
  if (info != NULL) {
    for (PreprocessingInfo *candidate : *info) {
      if (candidate->getRelativePosition() != PreprocessingInfo::before) {
        anchor = candidate;
        break;
      }
    }
  }

  for (PreprocessingInfo *directive : D) {
    if (anchor != NULL) {
      s->attachPreprocessingInfoRelative(
          directive, directive->getRelativePosition(), anchor, false);
    } else {
      s->attachPreprocessingInfo(
          directive, directive->getRelativePosition(),
          SgLocatedNode::PreprocessingInfoInsertion::back);
    }
  }
  D.clear();
}

//! Returns 'true' if the statement deliberately contributes no lexical output.
static bool isNonOutputStatement(const SgStatement *statement) {
  ROSE_ASSERT(statement != NULL);
  const Sg_File_Info *info = statement->get_file_info();
  if (info == NULL) {
    fprintf(stderr,
            "REX_OUTLINER_INVARIANT[preprocessing-owner]: first statement "
            "has no source information\n");
    ROSE_ABORT();
  }
  return !info->isOutputInCodeGeneration();
}

/*!
 *  \brief Inserts the given preprocessing info at the front of the
 *  first statement in b.
 *
 *  \pre b is non-NULL and has at least 1 statement in it.
 */
static void prependAtFirstStatement(PreprocInfoList_t &D, SgBasicBlock *b) {
  ROSE_ASSERT(b && !b->get_statements().empty());

  SgStatementPtrList &stmts = b->get_statements();
  SgStatement *s = *(stmts.begin());
  ROSE_ASSERT(s);

  if (isNonOutputStatement(s)) {
    // A non-output semantic child cannot own emitted preprocessing.  The
    // enclosing lexical block is the exact owner of the beginning-of-block
    // surface; no fabricated null statement is needed.
    insertMiddle(D, b);
  } else { // Attach at the front of preprocessing info at 's'.
    // DQ (9/26/2007): Moved from std::list to std::vector uniformly in ROSE.
    //   printf ("Commentout out front_inserter since it is unavailable in
    //   std::vector \n");
    // copy (D.rbegin (), D.rend (), front_inserter (*ASTtools::createInfoList
    // (s)));

    // Liao (10/3/2007), append elements for a vector
    attachDirectivesAtFront(D, s);
  }
}

//! Inserts preprocessing info objects into a basic block.
static void insertContext(PreprocInfoList_t &D, ContextPosType pos,
                          SgBasicBlock *b) {
  if (b == nullptr) {
    fprintf(stderr, "REX_AST_INVARIANT[outliner-preprocessing-owner]: context "
                    "insertion requires a basic block\n");
    ROSE_ABORT();
  }
  if (D.empty())
    return;

  switch (pos) {
  case e_beforeBlock: {
    // DQ (9/26/2007): Moved from std::list to std::vector uniformly in ROSE.
    //   printf ("Commentout out front_inserter since it is unavailable in
    //   std::vector \n");
    // copy (D.rbegin (), D.rend (), front_inserter (*ASTtools::createInfoList
    // (b)));

    // Liao (10/3/2007), append elements for a vector
    attachDirectivesAtFront(D, b);

    break;
  }

  case e_lastInBlock:
    attachDirectivesAtBack(D, b);
    break;
  case e_firstInBlock: {
    if (b->get_statements().empty())
      insertContext(D, e_beforeLastInBlock, b);
    else
      prependAtFirstStatement(D, b);
  } break;
  case e_beforeLastInBlock:
    insertMiddle(D, b);
    break;
  default:
    fprintf(stderr,
            "REX_AST_INVARIANT[outliner-preprocessing-position]: invalid "
            "context position=%d\n",
            static_cast<int>(pos));
    ROSE_ABORT();
  }
}

// =====================================================================

static void closeContext(const CPreproc::If::Case *context, ContextPosType pos,
                         SgBasicBlock *b) {
  // New directives.
  PreprocInfoList_t new_dirs;
  genCloseDirectives(context, getRelPos(pos), new_dirs);
  insertContext(new_dirs, pos, b);
}

static void openContext(const CPreproc::If::Case *context, ContextPosType pos,
                        SgBasicBlock *b) {
  // New directives.
  PreprocInfoList_t new_dirs;
  genOpenDirectives(context, getRelPos(pos), new_dirs);
  insertContext(new_dirs, pos, b);
}

// eof
