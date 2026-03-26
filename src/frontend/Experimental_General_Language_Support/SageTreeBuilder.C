#include "sage3basic.h"

#include "rose_config.h"

#include "ModuleBuilder.h"

#include "SageTreeBuilder.h"

#include <iostream>

#include <set>
#include <sstream>

#include <utility>

constexpr bool TRACE_ATTACH_COMMENT = false;

namespace Rose {
namespace builder {

using namespace LanguageTranslation;

namespace SB = SageBuilder;
namespace SI = SageInterface;

namespace {
PreprocessingInfo::DirectiveType
GetFortranCommentStyle(const SgSourceFile *source);

bool IsCommentInfo(const PreprocessingInfo *info) {
  if (info == nullptr) {
    return false;
  }
  const auto type = info->getTypeOfDirective();
  return type == PreprocessingInfo::FortranStyleComment ||
         type == PreprocessingInfo::F90StyleComment ||
         type == PreprocessingInfo::C_StyleComment ||
         type == PreprocessingInfo::CplusplusStyleComment;
}

std::string BuildCommentKey(const PreprocessingInfo *info) {
  if (info == nullptr) {
    return {};
  }
  std::string key =
      std::to_string(static_cast<int>(info->getTypeOfDirective())) + ":" +
      std::to_string(static_cast<int>(info->getRelativePosition())) + ":" +
      info->getString();
  if (info->getLineNumber() > 0) {
    key = std::to_string(info->getLineNumber()) + ":" + key;
  }
  return key;
}

void DedupAttachedPreprocessingInfo(SgLocatedNode *node) {
  if (node == nullptr) {
    return;
  }
  AttachedPreprocessingInfoType *info_list =
      node->getAttachedPreprocessingInfo();
  if (info_list == nullptr || info_list->empty()) {
    return;
  }

  std::set<std::string> seen_keys;
  for (auto it = info_list->begin(); it != info_list->end();) {
    PreprocessingInfo *info = *it;
    if (!IsCommentInfo(info)) {
      ++it;
      continue;
    }
    const std::string key = BuildCommentKey(info);
    if (seen_keys.count(key) > 0) {
      it = info_list->erase(it);
      continue;
    }
    seen_keys.insert(key);
    ++it;
  }
}

void RemoveDuplicateComments(SgLocatedNode *target, SgLocatedNode *reference) {
  if (target == nullptr || reference == nullptr) {
    return;
  }
  AttachedPreprocessingInfoType *target_list =
      target->getAttachedPreprocessingInfo();
  if (target_list == nullptr || target_list->empty()) {
    return;
  }
  const AttachedPreprocessingInfoType *ref_list =
      reference->getAttachedPreprocessingInfo();
  if (ref_list == nullptr || ref_list->empty()) {
    return;
  }

  std::set<std::string> ref_keys;
  for (const PreprocessingInfo *info : *ref_list) {
    if (!IsCommentInfo(info)) {
      continue;
    }
    ref_keys.insert(BuildCommentKey(info));
  }

  for (auto it = target_list->begin(); it != target_list->end();) {
    PreprocessingInfo *info = *it;
    if (!IsCommentInfo(info)) {
      ++it;
      continue;
    }
    const std::string key = BuildCommentKey(info);
    if (ref_keys.count(key) > 0) {
      it = target_list->erase(it);
      continue;
    }
    ++it;
  }
}

std::string formatLocatedNode(const SgLocatedNode *node) {
  if (node == nullptr) {
    return {};
  }
  const Sg_File_Info *info = node->get_startOfConstruct();
  if (info == nullptr) {
    return {};
  }
  std::ostringstream out;
  const std::string &file = info->get_filenameString();
  if (!file.empty()) {
    out << file;
  }
  if (info->get_line() > 0) {
    out << ":" << info->get_line();
    if (info->get_col() > 0) {
      out << ":" << info->get_col();
    }
  }
  return out.str();
}

std::string resolveCommentFilename(const SgSourceFile *source) {
  if (source == nullptr) {
    return {};
  }
  if (source->get_file_info() != nullptr) {
    const std::string file_info_name =
        source->get_file_info()->get_filenameString();
    if (!file_info_name.empty()) {
      return file_info_name;
    }
  }
  std::string filename = source->get_sourceFileNameWithPath();
  if (filename.empty()) {
    filename = source->getFileName();
  }
  return filename;
}

std::string resolvePrimarySourceFilename(const SgSourceFile *source) {
  if (source == nullptr) {
    return {};
  }
  std::string filename = source->get_sourceFileNameWithPath();
  if (filename.empty()) {
    filename = source->getFileName();
  }
  if (filename.empty()) {
    return {};
  }
  return StringUtility::getAbsolutePathFromRelativePath(filename);
}

bool isPrimaryFortranSourceRange(const SgSourceFile *source,
                                 const SourcePosition &start,
                                 const SourcePosition &end) {
  const std::string primaryPath = resolvePrimarySourceFilename(source);
  if (primaryPath.empty()) {
    return true;
  }
  return StringUtility::getAbsolutePathFromRelativePath(start.path) ==
             primaryPath &&
         StringUtility::getAbsolutePathFromRelativePath(end.path) ==
             primaryPath;
}

bool isInSourceFileForComments(const SgLocatedNode *node,
                               const SgSourceFile *source) {
  if (node == nullptr || source == nullptr) {
    return true;
  }
  const Sg_File_Info *node_info = node->get_file_info();
  const Sg_File_Info *source_info = source->get_file_info();
  if (node_info == nullptr || source_info == nullptr) {
    return true;
  }
  if (node_info->get_physical_file_id() == Sg_File_Info::NULL_FILE_ID ||
      node_info->get_physical_file_id() == Sg_File_Info::COPY_FILE_ID) {
    return true;
  }
  return node_info->get_physical_file_id() ==
         source_info->get_physical_file_id();
}

std::string buildFortranCommentText(PreprocessingInfo::DirectiveType style,
                                    const std::string &content) {
  switch (style) {
  case PreprocessingInfo::FortranStyleComment:
    return "      C " + content;
  case PreprocessingInfo::F90StyleComment:
    return "!" + content;
  default:
    return content;
  }
}

void attachCommentFromToken(SgLocatedNode *node, const Token &token,
                            PreprocessingInfo::RelativePositionType position,
                            const SgSourceFile *source) {
  if (node == nullptr || source == nullptr) {
    return;
  }

  PreprocessingInfo::RelativePositionType adjusted_position = position;
  if (position == PreprocessingInfo::after && isSgBasicBlock(node) != nullptr) {
    adjusted_position = PreprocessingInfo::inside;
  }

  const PreprocessingInfo::DirectiveType style = GetFortranCommentStyle(source);
  const std::string comment = buildFortranCommentText(style, token.getLexeme());
  const std::string filename = resolveCommentFilename(source);
  int numberOfLines = token.getEndLine() - token.getStartLine() + 1;
  if (numberOfLines < 1) {
    numberOfLines = 1;
  }

  PreprocessingInfo *info = new PreprocessingInfo(
      style, comment, filename, token.getStartLine(), token.getStartCol(),
      numberOfLines, adjusted_position);
  ROSE_ASSERT(info != nullptr);
  node->addToAttachedPreprocessingInfo(info);
}

PreprocessingInfo::DirectiveType
GetFortranCommentStyle(const SgSourceFile *source) {
  if (source == nullptr) {
    return PreprocessingInfo::FortranStyleComment;
  }
  if (source->get_inputFormat() == SgFile::e_fixed_form_output_format) {
    return PreprocessingInfo::FortranStyleComment;
  }
  if (source->get_inputFormat() == SgFile::e_unknown_output_format &&
      source->get_F77_only()) {
    return PreprocessingInfo::FortranStyleComment;
  }
  return PreprocessingInfo::F90StyleComment;
}

bool namesMatch(const SgName &left, const SgName &right, bool caseInsensitive) {
  if (!caseInsensitive) {
    return left == right;
  }
  return StringUtility::convertToLowerCase(left.str()) ==
         StringUtility::convertToLowerCase(right.str());
}

SgInitializedName *findInitializedNameInStatements(SgScopeStatement *scope,
                                                   const std::string &name,
                                                   bool caseInsensitive) {
  SgBasicBlock *block = isSgBasicBlock(scope);
  if (block == nullptr) {
    return nullptr;
  }

  const SgName target(name);
  const SgStatementPtrList &stmts = block->get_statements();
  for (SgStatement *stmt : stmts) {
    SgVariableDeclaration *var_decl = isSgVariableDeclaration(stmt);
    if (var_decl == nullptr) {
      continue;
    }
    for (SgInitializedName *init_name : var_decl->get_variables()) {
      if (init_name == nullptr) {
        continue;
      }
      if (namesMatch(init_name->get_name(), target, caseInsensitive)) {
        return init_name;
      }
    }
  }
  return nullptr;
}

SgInitializedName *findInitializedNameInScope(SgScopeStatement *scope,
                                              const std::string &name,
                                              bool caseInsensitive) {
  if (scope == nullptr) {
    return nullptr;
  }

  if (SgInitializedName *init_name =
          findInitializedNameInStatements(scope, name, caseInsensitive)) {
    return init_name;
  }

  const SgName target(name);
  for (SgStatement *stmt : scope->generateStatementList()) {
    SgVariableDeclaration *var_decl = isSgVariableDeclaration(stmt);
    if (var_decl == nullptr) {
      continue;
    }
    for (SgInitializedName *init_name : var_decl->get_variables()) {
      if (init_name == nullptr) {
        continue;
      }
      if (namesMatch(init_name->get_name(), target, caseInsensitive)) {
        return init_name;
      }
    }
  }
  return nullptr;
}

constexpr const char *kFortranImplicitDeclAttr =
    "rose_fortran_implicit_declaration";

class FortranImplicitDeclAttribute : public AstAttribute {
public:
  AstAttribute *copy() const override {
    return new FortranImplicitDeclAttribute();
  }
  OwnershipPolicy getOwnershipPolicy() const override {
    return CONTAINER_OWNERSHIP;
  }
};

bool IsFortranSpecificationStatement(const SgStatement *stmt) {
  return isSgDeclarationStatement(stmt) || isSgUseStatement(stmt) ||
         isSgImplicitStatement(stmt) ||
         isSgAttributeSpecificationStatement(stmt);
}

SgType *StripModifierType(SgType *type) {
  while (auto *modifier = isSgModifierType(type)) {
    type = modifier->get_base_type();
  }
  return type;
}

SgType *StripPointerType(SgType *type) {
  type = StripModifierType(type);
  if (auto *pointer = isSgPointerType(type)) {
    type = pointer->get_base_type();
  }
  return StripModifierType(type);
}

bool IsFunctionLikeType(SgType *type) {
  type = StripPointerType(type);
  return isSgFunctionType(type) != nullptr ||
         isSgMemberFunctionType(type) != nullptr;
}

SgScopeStatement *FindFortranImplicitDeclScope(SgScopeStatement *scope) {
  if (scope == nullptr) {
    return nullptr;
  }

  if (SgFunctionDefinition *funcDef =
          SageInterface::getEnclosingFunctionDefinition(scope, true)) {
    if (SgBasicBlock *body = funcDef->get_body()) {
      return body;
    }
  }

  if (SgBasicBlock *block = isSgBasicBlock(scope)) {
    SgNode *parent = block->get_parent();
    if (parent != nullptr && isSgScopeStatement(parent) != nullptr &&
        isSgFunctionDefinition(parent) == nullptr) {
      return block;
    }
  }

  if (SgModuleStatement *moduleStmt =
          SageInterface::getEnclosingModuleStatement(scope, true)) {
    if (SgClassDefinition *moduleDef = moduleStmt->get_definition()) {
      return moduleDef;
    }
  }

  return scope;
}

bool HasFortranImplicitNone(SgScopeStatement *scope) {
  SgScopeStatement *implicit_scope = FindFortranImplicitDeclScope(scope);
  if (implicit_scope == nullptr) {
    return false;
  }
  for (SgStatement *stmt : implicit_scope->generateStatementList()) {
    SgImplicitStatement *implicit_stmt = isSgImplicitStatement(stmt);
    if (implicit_stmt == nullptr) {
      continue;
    }
    if (implicit_stmt->get_implicit_none()) {
      return true;
    }
    switch (implicit_stmt->get_implicit_spec()) {
    case SgImplicitStatement::e_none:
    case SgImplicitStatement::e_none_external:
    case SgImplicitStatement::e_none_type:
    case SgImplicitStatement::e_none_external_and_type:
      return true;
    default:
      break;
    }
  }
  return false;
}

void InsertFortranImplicitDeclaration(SgVariableDeclaration *decl,
                                      SgScopeStatement *scope) {
  ASSERT_not_null(decl);
  ASSERT_not_null(scope);

  for (SgStatement *stmt : scope->generateStatementList()) {
    if (!IsFortranSpecificationStatement(stmt)) {
      SageInterface::insertStatementBefore(stmt, decl);
      return;
    }
  }
  SageInterface::appendStatement(decl, scope);
}
} // namespace

/// Initialize the global scope and push it onto the scope stack
///
SgGlobal *initialize_global_scope(SgSourceFile *file) {
  // Set the default for source position generation to be consistent with other
  // languages (e.g. C/C++).
  SageBuilder::setSourcePositionClassificationMode(
      SageBuilder::e_sourcePositionFrontendConstruction);

  SgGlobal *globalScope = file->get_globalScope();
  ASSERT_not_null(globalScope);
  ASSERT_not_null(globalScope->get_parent());

  // Fortran is case insensitive
  globalScope->setCaseInsensitive(true);

  ASSERT_not_null(globalScope->get_endOfConstruct());
  ASSERT_not_null(globalScope->get_startOfConstruct());

  // Not sure why this isn't set at construction
  globalScope->get_startOfConstruct()->set_line(1);
  globalScope->get_endOfConstruct()->set_line(1);

  SageBuilder::pushScopeStack(globalScope);

  return globalScope;
}

void SageTreeBuilder::attachComments(SgLocatedNode *node, bool at_end) {
  PosInfo pos{node};
  attachComments(node, pos, at_end);
}

void SageTreeBuilder::attachComments(SgExpressionPtrList const &list) {
  for (auto expr : list) {
    PosInfo exprPos{expr};
    auto commentToken = tokens_->getNextToken();

    // May have problems with multi-line expressions, currently biased to
    // comments following the expression
    if (commentToken && exprPos.getEndLine() == commentToken->getStartLine()) {
      auto commentPosition = PreprocessingInfo::after;
      if (exprPos.getStartCol() >= commentToken->getEndCol()) {
        commentPosition = PreprocessingInfo::before;
      }
      attachCommentFromToken(expr, *commentToken, commentPosition, source_);
      tokens_->consumeNextToken();
    }
  }
}

SgVariableDeclaration *BuildFunctionTypeVarDecl(const std::string &name,
                                                SgType *type,
                                                SgExpression *init_expr,
                                                SgScopeStatement *scope) {
  ASSERT_not_null(type);
  ASSERT_not_null(scope);

  SgName var_name(name);
  SgInitializer *var_init = nullptr;
  if (init_expr != nullptr) {
    var_init = SageBuilder::buildAssignInitializer_nfi(init_expr, type);
  }

  SgVariableDeclaration *var_decl =
      new SgVariableDeclaration(var_name, type, var_init);
  ASSERT_not_null(var_decl);
  var_decl->set_firstNondefiningDeclaration(var_decl);
  var_decl->set_definingDeclaration(var_decl);
  SageInterface::setSourcePosition(var_decl);

  if (var_init != nullptr) {
    SageInterface::setSourcePosition(var_init);
  }

  SgInitializedName *init_name = var_decl->get_decl_item(var_name);
  ASSERT_not_null(init_name);

  if (init_name->get_declptr() == nullptr) {
    SgVariableDefinition *var_def =
        new SgVariableDefinition(init_name, var_init);
    ASSERT_not_null(var_def);
    var_def->set_vardefn(init_name);
    var_def->set_parent(init_name);
    init_name->set_declptr(var_def);
    if (var_def->get_file_info() == nullptr) {
      var_def->set_file_info(
          Sg_File_Info::generateDefaultFileInfoForCompilerGeneratedNode());
    }
  }

  init_name->set_scope(scope);
  init_name->set_parent(var_decl);

  SageInterface::fixVariableDeclaration(var_decl, scope);
  SageInterface::appendStatement(var_decl, scope);

  return var_decl;
}

void SageTreeBuilder::attachComments(SgLocatedNode *node, const PosInfo &pos,
                                     bool at_end) {
  const bool is_fortran_language = (language_ == LanguageEnum::Fortran) ||
                                   SageInterface::is_Fortran_language();
  if (is_fortran_language && !isInSourceFileForComments(node, source_)) {
    return;
  }
  // Attach comments at end of a statement or expression
  if (at_end && (isSgStatement(node) || isSgExpression(node))) {
    const Token *token = nullptr;

    // If a scope, some comments should be attached to last statement in scope
    SgStatement *last{nullptr};
    SgStatement *first{nullptr};
    if (auto scope = isSgScopeStatement(node)) {
      last = scope->lastStatement();
      first = scope->firstStatement();
      if (!is_fortran_language && last == nullptr &&
          isSgBasicBlock(scope) != nullptr) {
        if (auto parent_stmt = isSgStatement(scope->get_parent())) {
          last = parent_stmt;
        }
      }
    }

    int first_start_line = pos.getStartLine();
    if (is_fortran_language) {
      if (auto scope = isSgScopeStatement(node)) {
        int best_line = -1;
        SgStatement *best_stmt = nullptr;
        auto consider_stmt = [&](SgStatement *stmt) {
          if (stmt == nullptr) {
            return;
          }
          if (!isInSourceFileForComments(stmt, source_)) {
            return;
          }
          PosInfo stmt_pos{stmt};
          int line = stmt_pos.getStartLine();
          if (line > 0 && (best_line < 0 || line < best_line)) {
            best_line = line;
            best_stmt = stmt;
          }
        };
        const SgStatementPtrList stmts = scope->generateStatementList();
        for (SgStatement *stmt : stmts) {
          consider_stmt(stmt);
        }
        if (best_stmt != nullptr) {
          first = best_stmt;
          first_start_line = best_line;
        }
      }
    }
    if (first != nullptr && first_start_line <= 0) {
      PosInfo first_pos{first};
      if (first_pos.getStartLine() > 0) {
        first_start_line = first_pos.getStartLine();
      }
    }

    int leading_line = first_start_line;
    if (pos.getStartLine() > leading_line) {
      leading_line = pos.getStartLine();
    }

    int end_line = pos.getEndLine();
    if (end_line <= 0 && last != nullptr) {
      PosInfo last_pos{last};
      if (last_pos.getEndLine() > 0) {
        end_line = last_pos.getEndLine();
      }
    }
    if (end_line <= 0 && first != nullptr) {
      PosInfo first_pos{first};
      if (first_pos.getEndLine() > 0) {
        end_line = first_pos.getEndLine();
      }
    }
    if (end_line <= 0) {
      end_line = pos.getStartLine();
    }

    while ((token = tokens_->getNextToken()) &&
           token->getStartLine() <= end_line) {
      if (is_fortran_language && first &&
          token->getStartLine() <= leading_line) {
        if (TRACE_ATTACH_COMMENT) {
          MLOG_TRACE_CXX(MLOG_FRONTEND)
              << "attach leading comment to first stmt: " << first->class_name()
              << ": " << *token;
        }
        if (!isInSourceFileForComments(first, source_)) {
          break;
        }
        attachCommentFromToken(first, *token, PreprocessingInfo::before,
                               source_);
      } else if (last &&
                 (token->getEndLine() < end_line ||
                  (is_fortran_language && token->getEndLine() == end_line))) {
        if (TRACE_ATTACH_COMMENT) {
          MLOG_TRACE_CXX(MLOG_FRONTEND)
              << "attach end comment to last stmt: " << last->class_name()
              << ": " << *token;
        }
        if (!isInSourceFileForComments(last, source_)) {
          break;
        }
        attachCommentFromToken(last, *token, PreprocessingInfo::after, source_);
      } else {
        if (TRACE_ATTACH_COMMENT)
          MLOG_TRACE_CXX(MLOG_FRONTEND)
              << "---> attach end comment to: " << node->class_name() << ": "
              << *token;
        if (!isInSourceFileForComments(node, source_)) {
          break;
        }
        attachCommentFromToken(node, *token, PreprocessingInfo::after, source_);
      }
      tokens_->consumeNextToken();
    }
    return;
  }

  if (isSgScopeStatement(node)) {
    const Token *token = nullptr;
    SgStatement *first_stmt = nullptr;
    if (is_fortran_language) {
      if (auto scope = isSgScopeStatement(node)) {
        int best_line = -1;
        SgStatement *best_stmt = nullptr;
        auto consider_stmt = [&](SgStatement *stmt) {
          if (stmt == nullptr) {
            return;
          }
          if (!isInSourceFileForComments(stmt, source_)) {
            return;
          }
          PosInfo stmt_pos{stmt};
          int line = stmt_pos.getStartLine();
          if (line > 0 && (best_line < 0 || line < best_line)) {
            best_line = line;
            best_stmt = stmt;
          }
        };
        const SgStatementPtrList stmts = scope->generateStatementList();
        for (SgStatement *stmt : stmts) {
          consider_stmt(stmt);
        }
        if (best_stmt != nullptr) {
          first_stmt = best_stmt;
        }
      }
    }
    // Comments before scoping unit
    while ((token = tokens_->getNextToken()) &&
           token->getStartLine() < pos.getStartLine()) {
      if (TRACE_ATTACH_COMMENT) {
        MLOG_TRACE_CXX(MLOG_FRONTEND)
            << "attach comment before scoping unit: " << *token;
      }
      if (is_fortran_language && first_stmt != nullptr) {
        if (!isInSourceFileForComments(first_stmt, source_)) {
          break;
        }
        attachCommentFromToken(first_stmt, *token, PreprocessingInfo::before,
                               source_);
      } else {
        if (!isInSourceFileForComments(node, source_)) {
          break;
        }
        attachCommentFromToken(node, *token, PreprocessingInfo::before,
                               source_);
      }
      tokens_->consumeNextToken();
    }
    return;
  }

  if (SgStatement *stmt = isSgStatement(node)) {
    const Token *token = nullptr;
    while ((token = tokens_->getNextToken()) &&
           token->getStartLine() <= pos.getStartLine()) {
      SgLocatedNode *commentNode{stmt};
      if (token->getTokenType() == TokenKind::comment) {
        auto commentPosition = PreprocessingInfo::before;
        if (token->getStartLine() == pos.getStartLine()) {
          commentPosition = PreprocessingInfo::after;
          // check for comment following a variable initializer
          if (SgVariableDeclaration *varDecl = isSgVariableDeclaration(stmt)) {
            for (SgInitializedName *name : varDecl->get_variables()) {
              if (SgInitializer *init = name->get_initializer()) {
                PosInfo initPos{init};
                if (initPos.getEndCol() > token->getStartCol()) {
                  // attach comment after this variable initializer
                  commentNode = init;
                  break;
                }
              }
            }
          }
        }
        if (TRACE_ATTACH_COMMENT) {
          MLOG_TRACE_CXX(MLOG_FRONTEND)
              << "attach comment for: " << commentNode->class_name() << ": "
              << *token << ": " << commentPosition;
        }
        attachCommentFromToken(commentNode, *token, commentPosition, source_);
      }
      tokens_->consumeNextToken();
    }
  } else if (auto expr = isSgEnumVal(node)) {
    const Token *token = nullptr;
    auto commentPosition = PreprocessingInfo::before;
    // try only attaching comments from same line (what about multi-line
    // comments)
    while ((token = tokens_->getNextToken()) &&
           token->getStartLine() == pos.getStartLine()) {
      if (token->getTokenType() == TokenKind::comment) {
        if (token->getEndCol() == pos.getStartCol()) {
          commentPosition = PreprocessingInfo::after;
        }
        if (TRACE_ATTACH_COMMENT) {
          MLOG_TRACE_CXX(MLOG_FRONTEND)
              << "attach comment for: " << expr->class_name() << ": " << *token;
        }
        attachCommentFromToken(expr, *token, commentPosition, source_);
      }
      tokens_->consumeNextToken();
    }
  }

  else {
    // Additional expressions?
    if (TRACE_ATTACH_COMMENT) {
      MLOG_WARN_CXX(MLOG_FRONTEND)
          << "SageTreeBuilder::attachComment: not adding node "
          << node->class_name() << "\n";
    }
  }
}

/** Attach comments from a vector */
void SageTreeBuilder::attachComments(SgLocatedNode *node,
                                     const std::vector<Token> &tokens,
                                     bool at_end) {
  const bool is_fortran_language = (language_ == LanguageEnum::Fortran) ||
                                   SageInterface::is_Fortran_language();
  if (is_fortran_language && !isInSourceFileForComments(node, source_)) {
    return;
  }
  auto commentPosition{PreprocessingInfo::before};
  if (at_end) {
    commentPosition = PreprocessingInfo::after;
  }

  for (auto token : tokens) {
    if (TRACE_ATTACH_COMMENT) {
      MLOG_TRACE_CXX(MLOG_FRONTEND)
          << "attach comment to: " << node->class_name() << ": " << token
          << ": pos: " << commentPosition;
    }
    attachCommentFromToken(node, token, commentPosition, source_);
  }
}

/** Conditionally attach comments from a vector */
void SageTreeBuilder::attachComments(SgLocatedNode *node,
                                     std::vector<Token> &tokens,
                                     const PosInfo &pos) {
  const bool is_fortran_language = (language_ == LanguageEnum::Fortran) ||
                                   SageInterface::is_Fortran_language();
  if (is_fortran_language && !isInSourceFileForComments(node, source_)) {
    return;
  }
  SgStatement *first_stmt{nullptr};
  int first_stmt_line = pos.getStartLine();
  if (is_fortran_language) {
    if (auto scope = isSgScopeStatement(node)) {
      int best_line = -1;
      SgStatement *best_stmt = nullptr;
      auto consider_stmt = [&](SgStatement *stmt) {
        if (stmt == nullptr) {
          return;
        }
        if (!isInSourceFileForComments(stmt, source_)) {
          return;
        }
        PosInfo stmt_pos{stmt};
        int line = stmt_pos.getStartLine();
        if (line > 0 && (best_line < 0 || line < best_line)) {
          best_line = line;
          best_stmt = stmt;
        }
      };
      const SgStatementPtrList stmts = scope->generateStatementList();
      for (SgStatement *stmt : stmts) {
        consider_stmt(stmt);
      }
      if (best_stmt != nullptr) {
        first_stmt = best_stmt;
        first_stmt_line = best_line;
      }
    }
  }
  if (first_stmt != nullptr && first_stmt_line <= 0) {
    PosInfo first_pos{first_stmt};
    if (first_pos.getStartLine() > 0) {
      first_stmt_line = first_pos.getStartLine();
    }
  }

  int count{0};
  for (auto token : tokens) {
    if (token.getStartLine() <= pos.getStartLine()) {
      if (TRACE_ATTACH_COMMENT) {
        MLOG_TRACE_CXX(MLOG_FRONTEND)
            << "attach comment for: " << node->class_name() << ": " << token;
      }
      if (is_fortran_language && first_stmt &&
          token.getStartLine() <= first_stmt_line) {
        if (!isInSourceFileForComments(first_stmt, source_)) {
          break;
        }
        attachCommentFromToken(first_stmt, token, PreprocessingInfo::before,
                               source_);
      } else {
        if (!isInSourceFileForComments(node, source_)) {
          break;
        }
        attachCommentFromToken(node, token, PreprocessingInfo::before, source_);
      }
      count += 1;
    }
  }
  if (count > 0)
    tokens.erase(tokens.begin(), tokens.begin() + count);
}

/** Attach any left over comments to end of node */
void SageTreeBuilder::attachRemainingComments(SgLocatedNode *node) {
  const bool is_fortran_language = (language_ == LanguageEnum::Fortran) ||
                                   SageInterface::is_Fortran_language();
  if (is_fortran_language && !isInSourceFileForComments(node, source_)) {
    return;
  }
  const Token *token = nullptr;
  while ((token = tokens_->getNextToken())) {
    if (TRACE_ATTACH_COMMENT) {
      MLOG_TRACE_CXX(MLOG_FRONTEND)
          << "attach comment for: " << node->class_name() << ": " << *token;
    }
    attachCommentFromToken(node, *token, PreprocessingInfo::after, source_);
    tokens_->consumeNextToken();
  }
}

/** Move comments preceding @pos to a vector */
void SageTreeBuilder::consumePrecedingComments(std::vector<Token> &tokens,
                                               const PosInfo &pos) {
  const Token *token = nullptr;
  while ((token = tokens_->getNextToken()) &&
         token->getStartLine() <= pos.getStartLine()) {
    tokens.push_back(*token);
    tokens_->consumeNextToken();
  }
}

/** Pop the scope stack and conditionally @attach_comments associated with end
 * of scope */
SgScopeStatement *SageTreeBuilder::popScopeStack(bool attach_comments) {
  auto scope = SageBuilder::topScopeStack();
  if (attach_comments) {
    attachComments(scope, PosInfo{scope}, /*at_end*/ true);
  }
  SageBuilder::popScopeStack();
  return scope;
}

void SageTreeBuilder::setSourcePosition(SgLocatedNode *node,
                                        const SourcePosition &start,
                                        const SourcePosition &end) {
  ASSERT_not_null(node);

  const bool missing_source_position = start.path.empty() || end.path.empty() ||
                                       start.line <= 0 || end.line <= 0 ||
                                       start.column <= 0 || end.column <= 0;
  if (missing_source_position) {
    // Flang can produce nodes without concrete source coordinates.
    // Keep these nodes printable by using transformation file-info
    // instead of marking source unavailable.
    SageInterface::setSourcePositionAsTransformation(node);
    if (node->get_file_info() != nullptr) {
      node->get_file_info()->setTransformation();
      node->get_file_info()->setOutputInCodeGeneration();
    }
    if (node->get_startOfConstruct() != nullptr) {
      node->get_startOfConstruct()->setTransformation();
      node->get_startOfConstruct()->setOutputInCodeGeneration();
    }
    if (node->get_endOfConstruct() != nullptr) {
      node->get_endOfConstruct()->setTransformation();
      node->get_endOfConstruct()->setOutputInCodeGeneration();
    }
    node->setTransformation();
    node->setOutputInCodeGeneration();
    return;
  }

  // SageBuilder may have been used and it builds FileInfo
  if (node->get_startOfConstruct() != nullptr) {
    delete node->get_startOfConstruct();
    node->set_startOfConstruct(nullptr);
  }
  if (node->get_endOfConstruct() != nullptr) {
    delete node->get_endOfConstruct();
    node->set_endOfConstruct(nullptr);
  }

  node->set_startOfConstruct(
      new Sg_File_Info(start.path, start.line, start.column));
  node->get_startOfConstruct()->set_parent(node);

  node->set_endOfConstruct(new Sg_File_Info(
      end.path, end.line, end.column - 1)); // ROSE end is inclusive
  node->get_endOfConstruct()->set_parent(node);

  SageInterface::setSourcePosition(node);

  if (language_ == LanguageEnum::Fortran && source_ != nullptr &&
      source_->get_requires_C_preprocessor() &&
      isPrimaryFortranSourceRange(source_, start, end) &&
      isSgStatement(node) != nullptr) {
    if (node->get_startOfConstruct() != nullptr) {
      node->get_startOfConstruct()->setOutputInCodeGeneration();
    }
    if (node->get_endOfConstruct() != nullptr) {
      node->get_endOfConstruct()->setOutputInCodeGeneration();
    }
  }

  // and attach comments if they exist
  if (isSgFunctionDefinition(node) == nullptr) {
    PosInfo pinfo{start.line, start.column, end.line, end.column};
    attachComments(node, pinfo);
  }
}

namespace {
bool hasConcreteSourceLocation(const Sg_File_Info *info) {
  return info != nullptr && !info->get_filenameString().empty() &&
         info->get_filenameString() != "NULL_FILE" && info->get_line() > 0 &&
         info->get_col() > 0 && !info->isSourcePositionUnavailableInFrontend();
}

void setLocatedNodeSourceAnchor(SgLocatedNode *node,
                                const SourcePosition &anchor) {
  ASSERT_not_null(node);

  Sg_File_Info *old_start = node->get_startOfConstruct();
  Sg_File_Info *old_file_info = node->get_file_info();
  Sg_File_Info *old_end = node->get_endOfConstruct();

  if (old_start != nullptr) {
    delete old_start;
  }
  if (old_file_info != nullptr && old_file_info != old_start) {
    delete old_file_info;
  }
  if (old_end != nullptr) {
    delete old_end;
  }

  Sg_File_Info *start_info =
      new Sg_File_Info(anchor.path, anchor.line, anchor.column);
  Sg_File_Info *end_info =
      new Sg_File_Info(anchor.path, anchor.line, anchor.column);
  ASSERT_not_null(start_info);
  ASSERT_not_null(end_info);

  node->set_startOfConstruct(start_info);
  node->set_file_info(start_info);
  node->set_endOfConstruct(end_info);
  start_info->set_parent(node);
  end_info->set_parent(node);
}

void ensureFortranParameterSourceLocations(SgFunctionDeclaration *function_decl,
                                           const SourcePosition &anchor) {
  ASSERT_not_null(function_decl);

  auto repair_param_list = [&](SgFunctionParameterList *param_list,
                               SgFunctionDeclaration *owner) {
    if (param_list == nullptr) {
      return;
    }

    if (!hasConcreteSourceLocation(param_list->get_file_info()) ||
        !hasConcreteSourceLocation(param_list->get_startOfConstruct()) ||
        !hasConcreteSourceLocation(param_list->get_endOfConstruct())) {
      setLocatedNodeSourceAnchor(param_list, anchor);
    }

    for (SgInitializedName *arg : param_list->get_args()) {
      if (arg == nullptr) {
        continue;
      }

      if (!hasConcreteSourceLocation(arg->get_file_info()) ||
          !hasConcreteSourceLocation(arg->get_startOfConstruct()) ||
          !hasConcreteSourceLocation(arg->get_endOfConstruct())) {
        setLocatedNodeSourceAnchor(arg, anchor);
      }

      if (arg->get_parent() == nullptr) {
        arg->set_parent(param_list);
      }
      if (owner != nullptr && arg->get_declptr() == nullptr) {
        arg->set_declptr(owner);
      }
    }
  };

  SgFunctionParameterList *def_params = function_decl->get_parameterList();
  repair_param_list(def_params, function_decl);

  SgFunctionDeclaration *first_nondef =
      isSgFunctionDeclaration(function_decl->get_firstNondefiningDeclaration());
  if (first_nondef != nullptr && first_nondef != function_decl &&
      first_nondef->get_parameterList() != def_params) {
    repair_param_list(first_nondef->get_parameterList(), first_nondef);
  }
}
} // namespace

/// Constructor
///
SageTreeBuilder::SageTreeBuilder(SgSourceFile *source, LanguageEnum language,
                                 std::istringstream &tokens)
    : language_{language}, source_{source} {
  tokens_ = new TokenStream(tokens);
}

SageTreeBuilder::~SageTreeBuilder() {
  delete tokens_;
  tokens_ = nullptr;
}

void SageTreeBuilder::setTokens(std::vector<Token> tokens) {
  delete tokens_;
  tokens_ = new TokenStream(std::move(tokens));
}

void SageTreeBuilder::Enter(SgScopeStatement *&scope) {
  MLOG_TRACE_CXX(MLOG_FRONTEND)
      << "SageTreeBuilder::Enter(SgScopeStatement* &) \n";

  scope = isSgGlobal(SageBuilder::topScopeStack());
  ASSERT_not_null(scope);
}

void SageTreeBuilder::Leave(SgScopeStatement *scope) {
  MLOG_TRACE_CXX(MLOG_FRONTEND)
      << "SageTreeBuilder::Leave(SgScopeStatement*) \n";

  scope = isSgGlobal(SageBuilder::topScopeStack());
  ASSERT_not_null(scope);

  // Clear any dangling forward references
  if (!forward_var_refs_.empty()) {
    auto it = forward_var_refs_.begin();
    while (it != forward_var_refs_.end()) {
      if (SgFunctionSymbol *func_sym =
              SageInterface::lookupFunctionSymbolInParentScopes(it->first,
                                                                scope)) {
        SgVarRefExp *prev_var_ref = it->second;
        SgVariableSymbol *prev_var_sym = prev_var_ref->get_symbol();
        ASSERT_not_null(prev_var_sym);

        SgNode *prev_parent = prev_var_ref->get_parent();

        // There may be more options but only three are known so far
        if (isSgUnaryOp(prev_parent) || isSgBinaryOp(prev_parent) ||
            isSgExprStatement(prev_parent)) {
          SgExprListExp *params = SageBuilder::buildExprListExp_nfi();
          SgFunctionCallExp *func_call =
              SageBuilder::buildFunctionCallExp(func_sym, params);
          func_call->set_parent(prev_parent);

          if (SgExprStatement *expr_stmt = isSgExprStatement(prev_parent)) {
            expr_stmt->set_expression(func_call);
          } else if (SgUnaryOp *unary_op = isSgUnaryOp(prev_parent)) {
            SgVarRefExp *var_ref = isSgVarRefExp(unary_op->get_operand());
            if (var_ref == prev_var_ref) {
              unary_op->set_operand(func_call);
            }
            ASSERT_require(var_ref == prev_var_ref);
          } else if (SgBinaryOp *bin_op = isSgBinaryOp(prev_parent)) {
            // Is this left or right operand
            SgVarRefExp *var_ref = isSgVarRefExp(bin_op->get_rhs_operand());
            if (var_ref == prev_var_ref) {
              bin_op->set_rhs_operand(func_call);
            } else if ((var_ref = isSgVarRefExp(bin_op->get_lhs_operand()))) {
              bin_op->set_lhs_operand(func_call);
            }
            ASSERT_require(var_ref == prev_var_ref);
          }

          // The dangling variable reference has been fixed
          it = forward_var_refs_.erase(it);

          // Detach the placeholder symbol from the scope and leave cleanup to
          // the normal AST lifecycle.
          SgScopeStatement *prev_scope = prev_var_sym->get_scope();
          ASSERT_not_null(prev_scope);
          if (prev_scope->symbol_exists(prev_var_sym)) {
            prev_scope->remove_symbol(prev_var_sym);
          }
          prev_var_ref->set_parent(nullptr);
        } else {
          // Unexpected previous parent node
          MLOG_WARN_CXX(MLOG_FRONTEND) << "{" << it->first << ": " << it->second
                                       << " parent is " << prev_parent << "}\n";
          it++;
        }
      } else {
        it++;
      }
    }
  }

  // Some forward references can't be resolved until the global scope is reached
  if (!forward_var_refs_.empty() && isSgGlobal(scope)) {
    MLOG_WARN_CXX(MLOG_FRONTEND)
        << "map for forward variable references is not empty, size is "
        << forward_var_refs_.size() << "\n";
    forward_var_refs_.clear();
  }
  if (!forward_type_refs_.empty() && isSgGlobal(scope)) {
    MLOG_WARN_CXX(MLOG_FRONTEND)
        << "map for forward type references is not empty, size is "
        << forward_type_refs_.size() << "\n";
    forward_type_refs_.clear();
  }

  // Attaching any remaining comments
  attachRemainingComments(scope);
  if ((language_ == LanguageEnum::Fortran ||
       SageInterface::is_Fortran_language()) &&
      isSgGlobal(scope) != nullptr) {
    SgStatement *first_stmt = nullptr;
    int first_line = -1;
    const SgDeclarationStatementPtrList &decls = scope->getDeclarationList();
    for (SgDeclarationStatement *decl : decls) {
      if (decl == nullptr) {
        continue;
      }
      PosInfo stmt_pos{decl};
      int line = stmt_pos.getStartLine();
      if (line > 0 && (first_line < 0 || line < first_line)) {
        first_line = line;
        first_stmt = decl;
      }
    }
    if (first_stmt == nullptr && !decls.empty()) {
      first_stmt = decls.front();
    }
    if (first_line <= 0 && first_stmt != nullptr) {
      PosInfo first_pos{first_stmt};
      if (first_pos.getStartLine() > 0) {
        first_line = first_pos.getStartLine();
      }
    }
    if (first_stmt != nullptr) {
      AttachedPreprocessingInfoType *info_list =
          scope->getAttachedPreprocessingInfo();
      if (info_list != nullptr && !info_list->empty()) {
        auto is_comment_info = [](const PreprocessingInfo *info) {
          if (info == nullptr) {
            return false;
          }
          const auto type = info->getTypeOfDirective();
          return type == PreprocessingInfo::FortranStyleComment ||
                 type == PreprocessingInfo::F90StyleComment ||
                 type == PreprocessingInfo::C_StyleComment ||
                 type == PreprocessingInfo::CplusplusStyleComment;
        };
        std::set<std::string> seen_keys;
        if (AttachedPreprocessingInfoType *first_info =
                first_stmt->getAttachedPreprocessingInfo()) {
          for (const PreprocessingInfo *info : *first_info) {
            if (!is_comment_info(info)) {
              continue;
            }
            seen_keys.insert(BuildCommentKey(info));
          }
        }
        AttachedPreprocessingInfoType to_move;
        for (auto it = info_list->begin(); it != info_list->end();) {
          PreprocessingInfo *info = *it;
          if (info != nullptr && is_comment_info(info) &&
              (first_line <= 0 || info->getLineNumber() <= first_line)) {
            const std::string key = BuildCommentKey(info);
            if (seen_keys.count(key) > 0) {
              it = info_list->erase(it);
              continue;
            }
            seen_keys.insert(key);
            to_move.push_back(info);
            it = info_list->erase(it);
            continue;
          }
          ++it;
        }
        PreprocessingInfo *prev = nullptr;
        for (PreprocessingInfo *info : to_move) {
          if (prev == nullptr) {
            first_stmt->addToAttachedPreprocessingInfo(
                info, PreprocessingInfo::before);
          } else {
            first_stmt->insertToAttachedPreprocessingInfo(info, prev);
          }
          info->setRelativePosition(PreprocessingInfo::before);
          prev = info;
        }
        if (auto *func_decl = isSgFunctionDeclaration(first_stmt)) {
          DedupAttachedPreprocessingInfo(func_decl);
          if (func_decl->get_parameterList() != nullptr) {
            RemoveDuplicateComments(func_decl->get_parameterList(), func_decl);
          }
        }
      }
    }
  }
}

void SageTreeBuilder::Enter(SgBasicBlock *&block) {
  MLOG_TRACE_CXX(MLOG_FRONTEND) << "SageTreeBuilder::Enter(SgBasicBlock* &)\n";

  // Set the parent (at least temporarily) so that symbols can be traced.
  block = SageBuilder::buildBasicBlock_nfi(SageBuilder::topScopeStack());
  block->set_scope(SageBuilder::topScopeStack());

  // Append now (before Leave is called) so that symbol lookup will work
  SageInterface::appendStatement(block, SageBuilder::topScopeStack());
  SageBuilder::pushScopeStack(block);
}

void SageTreeBuilder::Leave(SgBasicBlock *block) {
  MLOG_TRACE_CXX(MLOG_FRONTEND) << "SageTreeBuilder::Leave(SgBasicBlock*) \n";
  SageBuilder::popScopeStack(); // this basic block
}

void SageTreeBuilder::Enter(SgProgramHeaderStatement *&program_decl,
                            const std::optional<std::string> &name,
                            const std::vector<std::string> &labels,
                            const SourcePositions &sources,
                            std::vector<Rose::builder::Token> &comments) {
  MLOG_TRACE_CXX(MLOG_FRONTEND)
      << "SageTreeBuilder::Enter(SgProgramHeaderStatement* &, ...) "
      << std::get<0>(sources) << ":" << std::get<1>(sources) << ":"
      << std::get<2>(sources) << "\n";

  SgScopeStatement *scope = SageBuilder::topScopeStack();

  ASSERT_not_null(scope);
  ASSERT_require(scope->variantT() == V_SgGlobal);

  SgName program_name(name.value_or(ROSE_IMPLICIT_FORTRAN_PROGRAM_NAME));

  SgFunctionParameterList *param_list =
      SageBuilder::buildFunctionParameterList_nfi();
  SgFunctionType *function_type =
      SageBuilder::buildFunctionType(SageBuilder::buildVoidType(), param_list);

  program_decl = new SgProgramHeaderStatement(program_name, function_type,
                                              /*function_def*/ nullptr);
  ASSERT_not_null(program_decl);

  // A Fortran program has no non-defining declaration (assume same for other
  // languages)
  program_decl->set_definingDeclaration(program_decl);

  program_decl->set_scope(scope);
  program_decl->set_parent(scope);
  param_list->set_parent(program_decl);

  SgBasicBlock *program_body = new SgBasicBlock();
  SgFunctionDefinition *program_def =
      new SgFunctionDefinition(program_decl, program_body);

  if (language_ == LanguageEnum::Fortran ||
      SageInterface::is_language_case_insensitive()) {
    program_body->setCaseInsensitive(true);
    program_def->setCaseInsensitive(true);
  }

  ASSERT_require(SageBuilder::topScopeStack()->isCaseInsensitive());
  SageBuilder::pushScopeStack(program_def);
  ASSERT_require(SageBuilder::topScopeStack()->isCaseInsensitive());
  SageBuilder::pushScopeStack(program_body);

  program_body->set_parent(program_def);
  program_def->set_parent(program_decl);

  // set source position and attach comments (order important as comments may be
  // added as side effect)
  const SourcePosition &ps = std::get<0>(sources);
  const SourcePosition &bs = std::get<1>(sources);
  const SourcePosition &pe = std::get<2>(sources);
  const bool is_fortran_language = (language_ == LanguageEnum::Fortran) ||
                                   SageInterface::is_Fortran_language();
  attachComments(program_decl, comments,
                 PosInfo{ps.line, ps.column, pe.line, pe.column});
  if (!is_fortran_language) {
    attachComments(program_body, comments,
                   PosInfo{bs.line, bs.column, pe.line, pe.column});
  }

  setSourcePosition(program_decl, std::get<0>(sources), std::get<2>(sources));
  setSourcePosition(program_def, std::get<1>(sources), std::get<2>(sources));
  setSourcePosition(program_body, std::get<1>(sources), std::get<2>(sources));
  SageInterface::setSourcePosition(program_decl->get_parameterList());

  // set labels
  if (SageInterface::is_Fortran_language() && labels.size() == 1) {
    SageInterface::setFortranNumericLabel(
        program_decl, atoi(labels.front().c_str()),
        SgLabelSymbol::e_start_label_type, /*label_scope=*/program_def);
  }

  // If there is no program name then there is no ProgramStmt (this probably
  // needs to be marked somehow?)
  if (!name) {
    MLOG_WARN_CXX(MLOG_FRONTEND)
        << "no ProgramStmt in the Fortran MainProgram\n";
  }

  ASSERT_require(program_body == SageBuilder::topScopeStack());
  ASSERT_require(program_decl->get_firstNondefiningDeclaration() == nullptr);
}

void SageTreeBuilder::Leave(SgProgramHeaderStatement *program_decl) {
  // On exit, this function will have checked that the program declaration is
  // properly connected, cleaned up the scope stack, resolved symbols, and
  // inserted the declaration into its scope.

  MLOG_TRACE_CXX(MLOG_FRONTEND)
      << "SageTreeBuilder::Leave(SgProgramHeaderStatement*) \n";

  popScopeStack(/*attach_comments*/ true); // program body
  popScopeStack(/*attach_comments*/ true); // program definition

  auto scope = SageBuilder::topScopeStack();

  // The program declaration must go into the global scope
  SgGlobal *global_scope = isSgGlobal(scope);
  ASSERT_not_null(global_scope);

  // A symbol using this name should not already exist
  SgName program_name = program_decl->get_name();
  ASSERT_require(!global_scope->symbol_exists(program_name));

  // Add a symbol to the symbol table in the global scope
  SgFunctionSymbol *symbol = new SgFunctionSymbol(program_decl);
  global_scope->insert_symbol(program_name, symbol);

  // Attach any remaining comments
  scope = program_decl->get_definition()->get_body();
  attachComments(scope, /*at_end*/ true);

  DedupAttachedPreprocessingInfo(program_decl);
  if ((language_ == LanguageEnum::Fortran ||
       SageInterface::is_Fortran_language()) &&
      program_decl != nullptr && program_decl->get_parameterList() != nullptr) {
    RemoveDuplicateComments(program_decl->get_parameterList(), program_decl);
  }

  SageInterface::appendStatement(program_decl, global_scope);
}

// Fortran has an end statement which may have an optional name and label
void SageTreeBuilder::setFortranEndProgramStmt(
    SgProgramHeaderStatement *program_decl,
    const std::optional<std::string> &name,
    const std::optional<std::string> &label) {
  ASSERT_not_null(program_decl);

  SgFunctionDefinition *program_def = program_decl->get_definition();
  ASSERT_not_null(program_def);

  if (label) {
    SageInterface::setFortranNumericLabel(program_decl, atoi(label->c_str()),
                                          SgLabelSymbol::e_end_label_type,
                                          /*label_scope=*/program_def);
  }

  if (name) {
    program_decl->set_named_in_end_statement(true);
  }
}

void SageTreeBuilder::Enter(SgFunctionParameterList *&param_list,
                            SgScopeStatement *&param_scope,
                            const std::string &function_name,
                            SgType *function_type, bool is_defining_decl) {
  MLOG_TRACE_CXX(MLOG_FRONTEND)
      << "SageTreeBuilder::Enter(SgFunctionParameterList*) \n";

  param_list = SageBuilder::buildFunctionParameterList_nfi();
  param_scope = nullptr;

  // If this is a defining declaration (has a function body) then an
  // SgBasicBlock must be created to temporarily store declarations needed to
  // build the types of the initialized names in the parameter list. These
  // declarations are transferred to the function definition scope during later
  // processing: Leave(SgFunctionDeclaration*).
  //
  if (is_defining_decl) {
    param_scope = new SgBasicBlock();
  } else {
    param_scope = new SgFunctionParameterScope();
  }

  ASSERT_not_null(param_scope);
  SageInterface::setSourcePosition(param_scope);

  // The parameter scope must be attached so that symbol lookups can happen
  ASSERT_require(param_scope->get_parent() == nullptr);
  param_scope->set_parent(SageBuilder::topScopeStack());

  if (language_ == LanguageEnum::Fortran ||
      SageInterface::is_language_case_insensitive() ||
      SageBuilder::topScopeStack()->isCaseInsensitive()) {
    param_scope->setCaseInsensitive(true);
  }

  // Build the initialized name and symbol for the function result. It is needed
  // because in Fortran the function name is used as a variable to set the
  // return result value. The initialized name will need to be transferred to
  // the function definition scope later.
  //
  if (function_type) {
    SgInitializedName *result_name = SageBuilder::buildInitializedName_nfi(
        function_name, function_type, /*initializer*/ nullptr);
    SageInterface::setSourcePosition(result_name);
    result_name->set_scope(param_scope);
    result_name->set_parent(param_scope);
    SgVariableSymbol *result_symbol = new SgVariableSymbol(result_name);
    param_scope->insert_symbol(result_name->get_name(), result_symbol);
  }

  SageBuilder::pushScopeStack(param_scope);
}

void SageTreeBuilder::Leave(SgFunctionParameterList *param_list,
                            SgScopeStatement *param_scope,
                            const std::list<FormalParameter> &param_name_list) {
  MLOG_TRACE_CXX(MLOG_FRONTEND)
      << "SageTreeBuilder::Leave(SgFunctionParameterList*) \n";

  ASSERT_not_null(param_list);
  ASSERT_not_null(param_scope);

  // Sanity check
  ASSERT_require(param_scope == SageBuilder::topScopeStack());

  // Populate the function parameter list from declarations in the parameter
  // block
  for (const FormalParameter &param : param_name_list) {
    SgVariableSymbol *symbol =
        SageInterface::lookupVariableSymbolInParentScopes(param.name,
                                                          param_scope);

    if (symbol == nullptr) {
      MLOG_ERROR_CXX(MLOG_FRONTEND)
          << "SageTreeBuilder::Leave(SgFunctionParameterList*) - symbol lookup "
             "failed for name "
          << param.name;
      ASSERT_not_null(symbol);
    }

    // Create a new initialized name for the parameter list
    SgInitializedName *init_name = symbol->get_declaration();
    SgType *type = init_name->get_type();
    SgInitializedName *new_init_name = SageBuilder::buildInitializedName_nfi(
        param.name, type, /*initializer*/ nullptr);
    SageInterface::setSourcePosition(new_init_name);

    param_list->append_arg(new_init_name);

    if (param.output) {
      init_name->get_storageModifier().setMutable();
      new_init_name->get_storageModifier().setMutable();
    }
  }

  SageBuilder::popScopeStack(); // remove parameter scope from the stack
}

void SageTreeBuilder::Leave(SgFunctionParameterList *param_list,
                            SgScopeStatement *param_scope,
                            const std::list<std::string> &dummy_arg_name_list) {
  MLOG_TRACE_CXX(MLOG_FRONTEND)
      << "SageTreeBuilder::Leave(SgFunctionParameterList* for Fortran) \n";

  ASSERT_not_null(param_scope);

  for (const std::string &name : dummy_arg_name_list) {
    if (name.empty()) {
      continue;
    }
    if (name == "*") {
      SgInitializedName *labelInit = SageBuilder::buildInitializedName_nfi(
          name, SgTypeLabel::createType(), /*initializer*/ nullptr);
      ASSERT_not_null(labelInit);
      SageInterface::setSourcePosition(labelInit);
      labelInit->set_scope(param_scope);
      param_list->append_arg(labelInit);
      labelInit->set_parent(param_list);

      if (param_scope->lookup_label_symbol(name) == nullptr) {
        SgLabelSymbol *labelSymbol = new SgLabelSymbol(labelInit);
        param_scope->insert_symbol(name, labelSymbol);
      }
      continue;
    }

    // TODO: deal with fortran functions when the dummy argument is not declared
    // and implicitly typed.
    const bool case_insensitive =
        SageInterface::is_language_case_insensitive() ||
        param_scope->isCaseInsensitive();
    SgVariableSymbol *symbol =
        SageInterface::lookupVariableSymbolInParentScopes(name, param_scope);
    SgInitializedName *decl_init = nullptr;
    if (symbol == nullptr) {
      decl_init =
          findInitializedNameInStatements(param_scope, name, case_insensitive);
      if (decl_init != nullptr) {
        symbol = isSgVariableSymbol(decl_init->get_symbol_from_symbol_table());
        if (symbol == nullptr) {
          if (SgVariableDeclaration *varDecl =
                  isSgVariableDeclaration(decl_init->get_parent())) {
            SageInterface::fixVariableDeclaration(varDecl, param_scope);
          }
          symbol = SageInterface::lookupVariableSymbolInParentScopes(
              name, param_scope);
        }
      }
    }
    if (symbol == nullptr && decl_init == nullptr) {
      SgType *implicitType = SageBuilder::buildFortranImplicitType(name);
      SgVariableDeclaration *varDecl =
          SageBuilder::buildVariableDeclaration_nfi(name, implicitType,
                                                    /*initializer*/ nullptr,
                                                    param_scope);
      ASSERT_not_null(varDecl);
      SageInterface::setSourcePosition(varDecl);
      SageInterface::appendStatement(varDecl, param_scope);
      symbol =
          SageInterface::lookupVariableSymbolInParentScopes(name, param_scope);
    }
    SgInitializedName *init_name =
        symbol != nullptr ? symbol->get_declaration() : decl_init;
    ASSERT_not_null(init_name);
    SgType *type = init_name->get_type();
    SgInitializedName *new_init_name = SageBuilder::buildInitializedName_nfi(
        name, type, /*initializer*/ nullptr);
    ASSERT_not_null(new_init_name);
    SageInterface::setSourcePosition(new_init_name);
    new_init_name->get_storageModifier() = init_name->get_storageModifier();
    param_list->append_arg(new_init_name);
  }

  SageBuilder::popScopeStack(); // remove parameter scope from the stack
}

void SageTreeBuilder::Enter(SgFunctionDefinition *&function_def) {
  MLOG_TRACE_CXX(MLOG_FRONTEND)
      << "SageTreeBuilder::Enter(SgFunctionDefinition*) \n";

  SgBasicBlock *block = SageBuilder::buildBasicBlock_nfi();

  function_def = new SgFunctionDefinition(block);
  ASSERT_not_null(function_def);
  SageInterface::setSourcePosition(function_def);

  if (language_ == LanguageEnum::Fortran ||
      SageInterface::is_language_case_insensitive() ||
      SageBuilder::topScopeStack()->isCaseInsensitive()) {
    function_def->setCaseInsensitive(true);
    block->setCaseInsensitive(true);
  }

  ASSERT_require(SageBuilder::topScopeStack()->isCaseInsensitive());
  SageBuilder::pushScopeStack(function_def);
}

void SageTreeBuilder::Leave(SgFunctionDefinition *function_def) {
  MLOG_TRACE_CXX(MLOG_FRONTEND)
      << "SageTreeBuilder::Leave(SgFunctionDefinition*) \n";
  // don't pop the scope stack here as the function declaration will need it on
  // enter
}

void SageTreeBuilder::Enter(
    SgFunctionDeclaration *&function_decl, const std::string &name,
    SgType *return_type, SgFunctionParameterList *param_list,
    const LanguageTranslation::FunctionModifierList &modifiers,
    bool is_defining_decl, const SourcePositions &sources,
    std::vector<Rose::builder::Token> &comments) {
  MLOG_TRACE_CXX(MLOG_FRONTEND)
      << "SageTreeBuilder::Enter(SgFunctionDeclaration* &, ...) "
      << std::get<0>(sources) << ":" << std::get<1>(sources) << ":"
      << std::get<2>(sources) << "\n";

  SgFunctionDefinition *function_def = nullptr;
  SgBasicBlock *function_body = nullptr;
  SgProcedureHeaderStatement::subprogram_kind_enum subprogram_kind;

  function_decl = nullptr;

  SgScopeStatement *scope = SageBuilder::topScopeStack();
  ASSERT_not_null(scope);

  if (return_type == nullptr) {
    return_type = SageBuilder::buildVoidType();
    subprogram_kind = SgProcedureHeaderStatement::e_subroutine_subprogram_kind;
  } else {
    subprogram_kind = SgProcedureHeaderStatement::e_function_subprogram_kind;
  }

  if (is_defining_decl) {
    function_decl = SB::buildProcedureHeaderStatement(
        SgName(name), return_type, param_list, subprogram_kind, scope);
    ASSERT_not_null(function_decl);

    function_def = function_decl->get_definition();
    function_body = function_def->get_body();
    ASSERT_not_null(function_def);
    ASSERT_not_null(function_body);

    if (language_ == LanguageEnum::Fortran ||
        SageInterface::is_language_case_insensitive() ||
        scope->isCaseInsensitive()) {
      function_def->setCaseInsensitive(true);
      function_body->setCaseInsensitive(true);
    }

    SageBuilder::pushScopeStack(function_def);
    SageBuilder::pushScopeStack(function_body);
  } else {
    function_decl = SB::buildNondefiningProcedureHeaderStatement(
        SgName(name), return_type, param_list, subprogram_kind, scope);
  }
  ASSERT_not_null(function_decl);

  // set source position and attach comments (order important, from list first,
  // decl before body)
  const SourcePosition &fs = std::get<0>(sources);
  const SourcePosition &bs = std::get<1>(sources);
  const SourcePosition &fe = std::get<2>(sources);
  const bool is_fortran_language = (language_ == LanguageEnum::Fortran) ||
                                   SageInterface::is_Fortran_language();
  attachComments(function_decl, comments,
                 PosInfo{fs.line, fs.column, fe.line, fe.column});
  if (!is_fortran_language) {
    attachComments(function_body, comments,
                   PosInfo{bs.line, bs.column, fe.line, fe.column});
  }

  if (function_decl)
    setSourcePosition(function_decl, std::get<0>(sources),
                      std::get<2>(sources));
  if (function_def)
    setSourcePosition(function_def, std::get<1>(sources), std::get<2>(sources));
  if (function_body)
    setSourcePosition(function_body, std::get<1>(sources),
                      std::get<2>(sources));

  if (is_fortran_language && fs.line > 0 && fs.column > 0 && !fs.path.empty()) {
    ensureFortranParameterSourceLocations(function_decl, fs);
  } else {
    SageInterface::setSourcePosition(function_decl->get_parameterList());
  }

  if (list_contains(modifiers, e_function_modifier_recursive))
    function_decl->get_functionModifier().setRecursive();

  if (list_contains(modifiers, e_function_modifier_pure))
    function_decl->get_functionModifier().setPure();
  if (list_contains(modifiers, e_function_modifier_elemental))
    function_decl->get_functionModifier().setElemental();
}

void SageTreeBuilder::Leave(SgFunctionDeclaration *function_decl,
                            SgScopeStatement *param_scope) {
  MLOG_TRACE_CXX(MLOG_FRONTEND)
      << "SageTreeBuilder::Leave(SgFunctionDeclaration*) \n";

  ASSERT_not_null(function_decl);

  SgName function_name = function_decl->get_name();
  SgVariableSymbol *result_symbol =
      param_scope->lookup_variable_symbol(function_decl->get_name());
  bool is_defining_decl = (isSgFunctionParameterScope(param_scope) == nullptr);
  bool skip_param_scope_transfer =
      is_defining_decl && param_scope != nullptr &&
      param_scope->getAttribute(kFlangParamScopeTransferredAttr) != nullptr;
  if (result_symbol == nullptr && is_defining_decl) {
    if (SgBasicBlock *function_body =
            isSgBasicBlock(SageBuilder::topScopeStack())) {
      result_symbol = function_body->lookup_variable_symbol(function_name);
    }
  }
  const bool force_case_insensitive =
      (language_ == LanguageEnum::Fortran) ||
      SageInterface::is_language_case_insensitive();

  auto ensure_symbols_for_block = [&](SgBasicBlock *block) {
    if (block == nullptr) {
      return;
    }
    const bool fortran_like = (language_ == LanguageEnum::Fortran) ||
                              SageInterface::is_language_case_insensitive() ||
                              block->isCaseInsensitive();
    if (!fortran_like) {
      return;
    }

    SgSymbolTable *symtab = block->get_symbol_table();
    for (SgStatement *stmt : block->get_statements()) {
      SgVariableDeclaration *var_decl = isSgVariableDeclaration(stmt);
      if (var_decl == nullptr) {
        continue;
      }
      SageInterface::fixVariableDeclaration(var_decl, block);
      for (SgInitializedName *init_name : var_decl->get_variables()) {
        if (init_name == nullptr) {
          continue;
        }
        if (init_name->get_scope() != block) {
          init_name->set_scope(block);
        }
        if (symtab != nullptr && symtab->find(init_name) == nullptr) {
          SgVariableSymbol *var_sym = new SgVariableSymbol(init_name);
          block->insert_symbol(init_name->get_name(), var_sym);
        }
      }
    }
  };

  auto fix_initnames_from_param_scope = [&](SgScopeStatement *target_scope,
                                            SgScopeStatement *old_scope) {
    if (target_scope == nullptr || old_scope == nullptr) {
      return;
    }
    SgSymbolTable *symtab = target_scope->get_symbol_table();
    if (symtab == nullptr) {
      return;
    }
    std::set<SgNode *> symbols = symtab->get_symbols();
    for (SgNode *symNode : symbols) {
      SgVariableSymbol *varSym = isSgVariableSymbol(symNode);
      if (varSym == nullptr) {
        continue;
      }
      SgInitializedName *initName = varSym->get_declaration();
      if (initName == nullptr) {
        continue;
      }
      if (initName->get_scope() == old_scope) {
        initName->set_scope(target_scope);
      }
      if (initName->get_parent() == old_scope) {
        initName->set_parent(target_scope);
      }
      if (varSym->get_parent() == old_scope) {
        varSym->set_parent(target_scope);
      }
    }
  };

  // If this is a defining declaration then the function body has to be moved
  // from the temporary parameter scope (param_scope is a SgBasicBlock*)
  if (is_defining_decl) {
    SgBasicBlock *function_body = isSgBasicBlock(SageBuilder::topScopeStack());
    ASSERT_not_null(function_body);

    if (!skip_param_scope_transfer) {
      // Move all of the statements temporarily stored in param_scope into the
      // scope of the function body
      if (SgBasicBlock *param_block = isSgBasicBlock(param_scope)) {
        bool has_statements = !param_block->get_statements().empty();
        bool has_symbols = false;
        if (SgSymbolTable *symtab = param_block->get_symbol_table()) {
          has_symbols = !symtab->get_symbols().empty();
        }
        if (has_statements || has_symbols) {
          SageInterface::ensureCaseInsensitiveSymbolTable(
              param_block, force_case_insensitive);
          SageInterface::ensureCaseInsensitiveSymbolTable(
              function_body, force_case_insensitive);
          ensure_symbols_for_block(param_block);
          SageInterface::moveStatementsBetweenBlocks(param_block,
                                                     function_body);
          SageInterface::transferSymbols(param_block, function_body);
        }
      }

      // Any symbols that originated in the parameter scope but were not tied to
      // statements must be rebound before deleting the temporary scope.
      fix_initnames_from_param_scope(function_body, param_scope);

      if (param_scope != nullptr) {
        Rose_STL_Container<SgNode *> init_nodes =
            NodeQuery::querySubTree(function_decl, V_SgInitializedName);
        for (SgNode *node : init_nodes) {
          SgInitializedName *init_name = isSgInitializedName(node);
          if (init_name == nullptr) {
            continue;
          }
          if (init_name->get_scope() == param_scope) {
            init_name->set_scope(function_body);
          }
          if (init_name->get_parent() == param_scope) {
            init_name->set_parent(function_body);
          }
        }
      }

      // Transfer any label symbols into the function definition scope before
      // deleting the parameter scope.
      if (isSgBasicBlock(param_scope)) {
        SgFunctionDefinition *function_def = function_decl->get_definition();
        ASSERT_not_null(function_def);
        auto transfer_label_symbols = [&](SgScopeStatement *from_scope) {
          if (from_scope == nullptr) {
            return;
          }
          SgSymbolTable *symtab = from_scope->get_symbol_table();
          if (symtab == nullptr) {
            return;
          }
          std::set<SgNode *> symbols = symtab->get_symbols();
          for (SgNode *symNode : symbols) {
            SgLabelSymbol *labelSym = isSgLabelSymbol(symNode);
            if (labelSym == nullptr) {
              continue;
            }
            from_scope->remove_symbol(labelSym);
            if (function_def->lookup_label_symbol(labelSym->get_name()) ==
                nullptr) {
              function_def->insert_symbol(labelSym->get_name(), labelSym);
            }
            if (SgLabelStatement *labelStmt = labelSym->get_declaration()) {
              labelStmt->set_scope(function_def);
            }
          }
        };
        transfer_label_symbols(param_scope);
        transfer_label_symbols(function_body);
      }

      // Re-parent any function type symbols that still point at the temporary
      // parameter scope (or its symbol table) before deletion.
      VariantVector variants;
      variants.push_back(V_SgFunctionTypeSymbol);
      Rose_STL_Container<SgNode *> symbols =
          NodeQuery::queryMemoryPool(variants);
      for (SgNode *node : symbols) {
        SgFunctionTypeSymbol *symbol = isSgFunctionTypeSymbol(node);
        if (symbol == nullptr) {
          continue;
        }

        SgSymbolTable *target_table = nullptr;
        SgType *type = symbol->get_type();
        if (isSgFunctionType(type) != nullptr ||
            isSgMemberFunctionType(type) != nullptr) {
          SgFunctionTypeTable *func_table =
              SgNode::get_globalFunctionTypeTable();
          if (func_table != nullptr) {
            target_table = func_table->get_function_type_table();
          }
        } else {
          SgTypeTable *type_table = SgNode::get_globalTypeTable();
          if (type_table != nullptr) {
            target_table = type_table->get_type_table();
          }
        }

        if (target_table != nullptr) {
          if (!target_table->exists(symbol)) {
            target_table->insert(symbol->get_name(), symbol);
          }
          if (symbol->get_parent() != target_table) {
            symbol->set_parent(target_table);
          }
        }
      }
    }

    // Connect the result SgInitializedName initially created in param_scope
    // into the scope of the function body
    if (result_symbol) {
      SgProcedureHeaderStatement *proc_decl =
          isSgProcedureHeaderStatement(function_decl);
      SgInitializedName *result_name =
          isSgInitializedName(result_symbol->get_declaration());
      ASSERT_not_null(proc_decl);
      ASSERT_not_null(result_name);

      proc_decl->set_result_name(result_name);
      if (!(language_ == LanguageEnum::Fortran &&
            isSgVariableDeclaration(result_name->get_parent()) != nullptr)) {
        result_name->set_parent(function_decl);
      }
      result_name->set_scope(function_body);
      if (language_ == LanguageEnum::Fortran &&
          isSgVariableDeclaration(result_name->get_parent()) == nullptr) {
        for (SgStatement *stmt : function_body->get_statements()) {
          SgVariableDeclaration *var_decl = isSgVariableDeclaration(stmt);
          if (var_decl == nullptr) {
            continue;
          }
          for (SgInitializedName *init_name : var_decl->get_variables()) {
            if (init_name == result_name) {
              result_name->set_parent(var_decl);
              break;
            }
          }
          if (isSgVariableDeclaration(result_name->get_parent()) != nullptr) {
            break;
          }
        }
      }
      if (function_body->lookup_variable_symbol(function_name) == nullptr) {
        function_body->insert_symbol(function_name, result_symbol);
      }
      ASSERT_not_null(function_body->lookup_symbol(function_name));
    }

    // Keep the original parent so memory-pool diagnostics can still walk any
    // compiler-generated placeholders that remain tied to the temporary scope.

    SageBuilder::popScopeStack(); // function body
    SageBuilder::popScopeStack(); // function definition
  } // is_def_decl
  else {
    ASSERT_not_null(isSgFunctionParameterScope(param_scope));
    ASSERT_require(function_decl->get_functionParameterScope() == nullptr);
    function_decl->set_functionParameterScope(
        isSgFunctionParameterScope(param_scope));

    if (result_symbol) {
      SgProcedureHeaderStatement *proc_decl =
          isSgProcedureHeaderStatement(function_decl);
      SgInitializedName *result_name =
          isSgInitializedName(result_symbol->get_declaration());
      ASSERT_not_null(proc_decl);
      ASSERT_not_null(result_name);

      proc_decl->set_result_name(result_name);
      if (!(language_ == LanguageEnum::Fortran &&
            isSgVariableDeclaration(result_name->get_parent()) != nullptr)) {
        result_name->set_parent(function_decl);
      }
    }
  }

  if (is_defining_decl) {
    // Attach any remaining comments
    auto scope = function_decl->get_definition()->get_body();
    if ((language_ == LanguageEnum::Fortran ||
         SageInterface::is_Fortran_language()) &&
        scope != nullptr) {
      if (tokens_ != nullptr) {
        Sg_File_Info *decl_info = function_decl->get_startOfConstruct();
        const int decl_line = decl_info != nullptr ? decl_info->get_line() : -1;
        if (decl_line > 0) {
          const Token *token = nullptr;
          while ((token = tokens_->getNextToken()) &&
                 token->getStartLine() < decl_line) {
            attachCommentFromToken(function_decl, *token,
                                   PreprocessingInfo::before, source_);
            tokens_->consumeNextToken();
          }
        }
      }
      if (SgBasicBlock *body = isSgBasicBlock(scope)) {
        SgStatement *first_stmt = nullptr;
        int best_line = -1;
        const SgStatementPtrList &stmts = body->getStatementList();
        for (SgStatement *stmt : stmts) {
          if (stmt == nullptr) {
            continue;
          }
          PosInfo stmt_pos{stmt};
          int line = stmt_pos.getStartLine();
          if (line > 0 && (best_line < 0 || line < best_line)) {
            best_line = line;
            first_stmt = stmt;
          }
        }
        if (first_stmt == nullptr && !stmts.empty()) {
          first_stmt = stmts.front();
        }
        int first_stmt_line = best_line;
        if (first_stmt_line <= 0 && first_stmt != nullptr) {
          PosInfo first_pos{first_stmt};
          if (first_pos.getStartLine() > 0) {
            first_stmt_line = first_pos.getStartLine();
          }
        }
        if (first_stmt != nullptr) {
          AttachedPreprocessingInfoType *info_list =
              body->getAttachedPreprocessingInfo();
          if (info_list != nullptr && !info_list->empty()) {
            AttachedPreprocessingInfoType to_move;
            for (auto it = info_list->begin(); it != info_list->end();) {
              PreprocessingInfo *info = *it;
              if (info != nullptr &&
                  (info->getTypeOfDirective() ==
                       PreprocessingInfo::FortranStyleComment ||
                   info->getTypeOfDirective() ==
                       PreprocessingInfo::F90StyleComment ||
                   info->getTypeOfDirective() ==
                       PreprocessingInfo::C_StyleComment ||
                   info->getTypeOfDirective() ==
                       PreprocessingInfo::CplusplusStyleComment) &&
                  (first_stmt_line <= 0 ||
                   info->getLineNumber() <= first_stmt_line)) {
                to_move.push_back(info);
                it = info_list->erase(it);
                continue;
              }
              ++it;
            }
            PreprocessingInfo *prev = nullptr;
            for (PreprocessingInfo *info : to_move) {
              if (prev == nullptr) {
                first_stmt->addToAttachedPreprocessingInfo(
                    info, PreprocessingInfo::before);
              } else {
                first_stmt->insertToAttachedPreprocessingInfo(info, prev);
              }
              info->setRelativePosition(PreprocessingInfo::before);
              prev = info;
            }
          }
        }
      }
    }
    attachComments(scope, /*at_end*/ true);
    if ((language_ == LanguageEnum::Fortran ||
         SageInterface::is_Fortran_language()) &&
        function_decl != nullptr) {
      Sg_File_Info *decl_info = function_decl->get_startOfConstruct();
      const int decl_line = decl_info != nullptr ? decl_info->get_line() : -1;
      SgBasicBlock *body = function_decl->get_definition()->get_body();
      if (decl_line > 0 && body != nullptr) {
        auto collect_comments = [&](AttachedPreprocessingInfoType *info_list,
                                    AttachedPreprocessingInfoType &out,
                                    bool skip_before) {
          if (info_list == nullptr || info_list->empty()) {
            return;
          }
          for (auto it = info_list->begin(); it != info_list->end();) {
            PreprocessingInfo *info = *it;
            if (info != nullptr &&
                (info->getTypeOfDirective() ==
                     PreprocessingInfo::FortranStyleComment ||
                 info->getTypeOfDirective() ==
                     PreprocessingInfo::F90StyleComment ||
                 info->getTypeOfDirective() ==
                     PreprocessingInfo::C_StyleComment ||
                 info->getTypeOfDirective() ==
                     PreprocessingInfo::CplusplusStyleComment) &&
                info->getLineNumber() > 0 &&
                info->getLineNumber() < decl_line &&
                (!skip_before ||
                 info->getRelativePosition() != PreprocessingInfo::before)) {
              out.push_back(info);
              it = info_list->erase(it);
              continue;
            }
            ++it;
          }
        };
        AttachedPreprocessingInfoType to_move;
        collect_comments(function_decl->getAttachedPreprocessingInfo(), to_move,
                         /*skip_before=*/true);
        collect_comments(body->getAttachedPreprocessingInfo(), to_move,
                         /*skip_before=*/false);
        for (SgStatement *stmt : body->get_statements()) {
          if (stmt == nullptr) {
            continue;
          }
          collect_comments(stmt->getAttachedPreprocessingInfo(), to_move,
                           /*skip_before=*/false);
        }
        PreprocessingInfo *prev = nullptr;
        for (PreprocessingInfo *info : to_move) {
          if (prev == nullptr) {
            function_decl->addToAttachedPreprocessingInfo(
                info, PreprocessingInfo::before);
          } else {
            function_decl->insertToAttachedPreprocessingInfo(info, prev);
          }
          info->setRelativePosition(PreprocessingInfo::before);
          prev = info;
        }
      }
    }
  }

  DedupAttachedPreprocessingInfo(function_decl);
  if ((language_ == LanguageEnum::Fortran ||
       SageInterface::is_Fortran_language()) &&
      function_decl != nullptr &&
      function_decl->get_parameterList() != nullptr) {
    RemoveDuplicateComments(function_decl->get_parameterList(), function_decl);
  }
  if ((language_ == LanguageEnum::Fortran ||
       SageInterface::is_Fortran_language()) &&
      function_decl != nullptr && function_decl->get_definition() != nullptr) {
    RemoveDuplicateComments(function_decl->get_definition(), function_decl);
  }

  // Finished using the map for labels
  labels_.clear();

  SageInterface::appendStatement(function_decl, SageBuilder::topScopeStack());
}

void SageTreeBuilder::Leave(SgFunctionDeclaration *function_decl,
                            SgScopeStatement *param_scope, bool have_end_stmt,
                            const std::string &result_name /* = "" */) {
  MLOG_TRACE_CXX(MLOG_FRONTEND)
      << "SageTreeBuilder::Leave(SgFunctionDeclaration*) \n";

  SgVariableSymbol *result_symbol = nullptr;
  if (!result_name.empty() && param_scope != nullptr) {
    result_symbol = param_scope->lookup_variable_symbol(result_name);
  }

  // Call more generic leave for SgFunctionDeclaration, will move declarations
  // out of param_scope into the body of the function declaration and will set
  // the result name as name of the function
  Leave(function_decl, param_scope);

  // If result is named, get symbol and init name of the result to set it for
  // the function declaration
  if (!result_name.empty()) {
    // Get symbol and associated initialized name
    SgFunctionDefinition *func_def = function_decl->get_definition();
    ASSERT_not_null(func_def);
    SgBasicBlock *body = func_def->get_body();
    ASSERT_not_null(body);
    const bool case_insensitive =
        (language_ == LanguageEnum::Fortran) ||
        SageInterface::is_language_case_insensitive() ||
        body->isCaseInsensitive();
    if (result_symbol == nullptr) {
      result_symbol =
          SageInterface::lookupVariableSymbolInParentScopes(result_name, body);
    }
    if (result_symbol == nullptr) {
      SgInitializedName *decl_init =
          findInitializedNameInStatements(body, result_name, case_insensitive);
      if (decl_init != nullptr) {
        result_symbol =
            isSgVariableSymbol(decl_init->get_symbol_from_symbol_table());
        if (result_symbol == nullptr) {
          SageInterface::rebuildSymbolTable(body);
          result_symbol = SageInterface::lookupVariableSymbolInParentScopes(
              result_name, body);
        }
      }
    }
    if (result_symbol == nullptr) {
      SgType *result_type = function_decl->get_type()->get_return_type();
      if (result_type == nullptr) {
        result_type = SageBuilder::buildFortranImplicitType(result_name);
      }
      SageBuilderCpp17::fixUndeclaredResultName(result_name, body, result_type);
      result_symbol = body->lookup_variable_symbol(result_name);
    }
    ASSERT_not_null(result_symbol);
    SgInitializedName *init_name = result_symbol->get_declaration();
    ASSERT_not_null(init_name);

    SgProcedureHeaderStatement *proc_header_stmt =
        isSgProcedureHeaderStatement(function_decl);
    ASSERT_not_null(proc_header_stmt);

    // If result is named but not declared, need to fix up initialized name
    // created earlier for it
    SgNode *parent = init_name->get_parent();
    if (parent == nullptr || isSgScopeStatement(parent) != nullptr) {
      init_name->set_parent(proc_header_stmt);
    }
    if (init_name->get_scope() != body) {
      init_name->set_scope(body);
    }
    if (body->lookup_variable_symbol(result_name) == nullptr) {
      SageInterface::rebuildSymbolTable(body);
    }

    // Reset the result name to the correct initialized name
    proc_header_stmt->set_result_name(init_name);
  }

  if (language_ == LanguageEnum::Fortran &&
      function_decl->get_functionModifier().isRecursive()) {
    SgProcedureHeaderStatement *proc_header_stmt =
        isSgProcedureHeaderStatement(function_decl);
    if (proc_header_stmt != nullptr && proc_header_stmt->isFunction()) {
      SgInitializedName *result_init = proc_header_stmt->get_result_name();
      if (result_init != nullptr) {
        const bool case_insensitive =
            SageInterface::is_language_case_insensitive() ||
            (function_decl->get_definition() != nullptr &&
             function_decl->get_definition()->get_body() != nullptr &&
             function_decl->get_definition()->get_body()->isCaseInsensitive());
        const SgName function_name = proc_header_stmt->get_name();
        if (namesMatch(result_init->get_name(), function_name,
                       case_insensitive)) {
          SgFunctionDefinition *func_def = function_decl->get_definition();
          SgBasicBlock *body =
              func_def != nullptr ? func_def->get_body() : nullptr;
          SgScopeStatement *result_scope = body;
          if (result_scope == nullptr) {
            result_scope = function_decl->get_functionParameterScope();
          }
          if (result_scope != nullptr) {
            const std::string base = function_name.str();
            std::string new_name = base + "_result";
            auto has_conflict = [&](const std::string &name) {
              if (result_scope->lookup_variable_symbol(name) != nullptr) {
                return true;
              }
              return findInitializedNameInScope(result_scope, name,
                                                case_insensitive) != nullptr;
            };
            if (has_conflict(new_name)) {
              for (int i = 1; i < 10000; ++i) {
                const std::string candidate =
                    base + "_result" + std::to_string(i);
                if (!has_conflict(candidate)) {
                  new_name = candidate;
                  break;
                }
              }
            }

            SgVariableSymbol *result_symbol =
                isSgVariableSymbol(result_init->get_symbol_from_symbol_table());
            if (result_symbol == nullptr) {
              result_symbol = isSgVariableSymbol(
                  result_init->search_for_symbol_from_symbol_table());
            }
            if (result_symbol == nullptr) {
              result_symbol = new SgVariableSymbol(result_init);
            }

            auto remove_from_scope = [&](SgScopeStatement *scope) {
              if (scope == nullptr) {
                return;
              }
              SgSymbolTable *symtab = scope->get_symbol_table();
              if (symtab == nullptr) {
                return;
              }
              if (symtab->exists(result_symbol)) {
                scope->remove_symbol(result_symbol);
              }
            };
            remove_from_scope(body);
            remove_from_scope(function_decl->get_functionParameterScope());

            result_init->set_name(SgName(new_name));
            result_init->set_scope(result_scope);
            if (result_scope->lookup_variable_symbol(new_name) == nullptr) {
              result_scope->insert_symbol(new_name, result_symbol);
            }
            proc_header_stmt->set_result_name(result_init);
          }
        }
      }
    }
  }

  // Set named end statement if needed
  if (have_end_stmt) {
    function_decl->set_named_in_end_statement(have_end_stmt);
  }
}

void SageTreeBuilder::Enter(SgDerivedTypeStatement *&derived_type_stmt,
                            const std::string &name) {
  MLOG_TRACE_CXX(MLOG_FRONTEND)
      << "SageTreeBuilder::Enter(SgDerivedTypeStatement* &, ...) \n";

  derived_type_stmt = SageBuilder::buildDerivedTypeStatement(
      name, SageBuilder::topScopeStack());

  SgClassDefinition *class_defn = derived_type_stmt->get_definition();
  ASSERT_not_null(class_defn);
  ASSERT_require(SageBuilder::topScopeStack()->isCaseInsensitive());

  // Append now (before Leave is called) so that symbol lookup will work
  SageInterface::appendStatement(derived_type_stmt,
                                 SageBuilder::topScopeStack());
  SageBuilder::pushScopeStack(class_defn);
}

void SageTreeBuilder::Leave(SgDerivedTypeStatement *derived_type_stmt) {
  MLOG_TRACE_CXX(MLOG_FRONTEND)
      << "SageTreeBuilder::Leave(SgDerivedTypeStatement*) \n";
  SageBuilder::popScopeStack(); // class definition
}

void SageTreeBuilder::Leave(
    SgDerivedTypeStatement *derived_type_stmt,
    std::list<LanguageTranslation::ExpressionKind> &modifier_enum_list) {
  MLOG_TRACE_CXX(MLOG_FRONTEND)
      << "SageTreeBuilder::Leave(SgDerivedTypeStatement*) with modifiers \n";

  for (LanguageTranslation::ExpressionKind modifier_enum : modifier_enum_list) {
    switch (modifier_enum) {
    case LanguageTranslation::ExpressionKind::e_access_modifier_public:
      derived_type_stmt->get_declarationModifier()
          .get_accessModifier()
          .setPublic();
      break;
    case LanguageTranslation::ExpressionKind::e_access_modifier_private:
      derived_type_stmt->get_declarationModifier()
          .get_accessModifier()
          .setPrivate();
      break;
    case LanguageTranslation::ExpressionKind::e_type_modifier_abstract:
      derived_type_stmt->get_declarationModifier()
          .get_typeModifier()
          .setAbstract();
      break;
    case LanguageTranslation::ExpressionKind::e_type_modifier_bind_c:
      derived_type_stmt->get_declarationModifier().get_typeModifier().setBind();
      break;
    default:
      break;
    }
  }

  Leave(derived_type_stmt);
}

// Statements
//

void SageTreeBuilder::Enter(SgNamespaceDeclarationStatement *&namespace_decl,
                            const std::string &name,
                            const SourcePositionPair &positions) {
  MLOG_TRACE_CXX(MLOG_FRONTEND)
      << "SageTreeBuilder::Enter(SgNamespaceDeclarationStatement* &, ...) \n";

  // Build a namespace to contain the module
  namespace_decl = SageBuilder::buildNamespaceDeclaration_nfi(
      name, true, SageBuilder::topScopeStack());
  SageInterface::setSourcePosition(namespace_decl);

  SgNamespaceDefinitionStatement *namespace_defn =
      namespace_decl->get_definition();
  ASSERT_not_null(namespace_defn);
  ASSERT_require(SageBuilder::topScopeStack()->isCaseInsensitive());

  // TEMPORARY: fix in SageBuilder
  namespace_defn->setCaseInsensitive(true);
  ASSERT_require(namespace_defn->isCaseInsensitive());

  // Append before push (so that symbol lookup will work)
  SageInterface::appendStatement(namespace_decl, SageBuilder::topScopeStack());
  SageBuilder::pushScopeStack(namespace_defn);
}

void SageTreeBuilder::Leave(SgNamespaceDeclarationStatement *namespace_decl) {
  MLOG_TRACE_CXX(MLOG_FRONTEND)
      << "SageTreeBuilder::Leave(SgNamespaceDeclarationStatement*, ...) \n";

  SageBuilder::popScopeStack(); // namespace definition
}

void SageTreeBuilder::Enter(SgExprStatement *&proc_call_stmt,
                            const std::string &proc_name,
                            SgExprListExp *param_list,
                            const std::string &abort_phrase) {
  MLOG_TRACE_CXX(MLOG_FRONTEND)
      << "SageTreeBuilder::Enter(SgExprStatement* &, ...) \n";

  SgFunctionCallExp *proc_call_exp;

  // I think entering an expression is a little awkward (what about leave an
  // expression, maybe ok)
  Enter(proc_call_exp, proc_name, param_list);

  // TODO: AbortPhrase handling for the frontend.
  proc_call_stmt = SageBuilder::buildExprStatement_nfi(proc_call_exp);
}

void SageTreeBuilder::Enter(SgExprStatement *&assign_stmt, SgExpression *&rhs,
                            const std::vector<SgExpression *> &vars) {
  MLOG_TRACE_CXX(MLOG_FRONTEND)
      << "SageTreeBuilder::Enter(SgExprStatement* &, ...) \n";

  SgExpression *lhs{nullptr};
  SgAssignOp *assign_op{nullptr};

  // Some languages may allow more than one variable in an assignment statement
  if (vars.size() == 1) {
    lhs = vars[0];
  } else if (vars.size() > 1) {
    lhs = SageBuilder::buildExprListExp(vars);
  }
  ASSERT_not_null(lhs);

  assign_op = SageBuilder::buildBinaryExpression_nfi<SgAssignOp>(lhs, rhs);
  assign_stmt = SageBuilder::buildExprStatement_nfi(assign_op);
}

void SageTreeBuilder::Leave(SgExprStatement *exprStmt,
                            std::vector<std::string> &labels) {
  MLOG_TRACE_CXX(MLOG_FRONTEND)
      << "SageTreeBuilder::Leave(SgExprStatement*) \n";

  SgStatement *stmt = wrapStmtWithLabels(exprStmt, labels);
  SageInterface::appendStatement(stmt, SB::topScopeStack());
}

void SageTreeBuilder::Enter(SgFunctionCallExp *&func_call,
                            const std::string &name, SgExprListExp *params) {
  MLOG_TRACE_CXX(MLOG_FRONTEND)
      << "SageTreeBuilder::Enter(SgFunctionCallExp* &, ...) \n";

  func_call = nullptr;

  // Function calls are ambiguous with arrays in Fortran (and type casts in some
  // languages). Start out by assuming it's a function call if another symbol
  // doesn't exist.

  SgFunctionSymbol *func_symbol =
      SageInterface::lookupFunctionSymbolInParentScopes(
          name, SageBuilder::topScopeStack());

  if (func_symbol == nullptr) {
    SgSymbol *symbol = SageInterface::lookupSymbolInParentScopes(
        name, SageBuilder::topScopeStack());
    if (symbol) {
      if (auto *var_sym = isSgVariableSymbol(symbol)) {
        SgType *var_type = var_sym->get_type();
        if (IsFunctionLikeType(var_type)) {
          SgVarRefExp *var_ref = SageBuilder::buildVarRefExp_nfi(var_sym);
          ASSERT_not_null(var_ref);
          func_call = SageBuilder::buildFunctionCallExp_nfi(var_ref, params);
          ASSERT_not_null(func_call);
          SageInterface::setSourcePosition(func_call);
          return;
        }
        if (SageInterface::is_Fortran_language()) {
          // Fortran allows procedure dummy arguments and externals that may
          // not yet be typed as functions. Promote the symbol to a function
          // type so the call expression stays consistent.
          if (SgInitializedName *init_name = var_sym->get_declaration()) {
            SgFunctionParameterTypeList *param_types =
                SageBuilder::buildFunctionParameterTypeList();
            SgType *return_type = SageBuilder::buildVoidType();
            SgFunctionType *proc_type =
                SageBuilder::buildFunctionType(return_type, param_types);
            init_name->set_type(proc_type);
          }

          SgVarRefExp *var_ref = SageBuilder::buildVarRefExp_nfi(var_sym);
          ASSERT_not_null(var_ref);
          func_call = SageBuilder::buildFunctionCallExp_nfi(var_ref, params);
          ASSERT_not_null(func_call);
          SageInterface::setSourcePosition(func_call);
          return;
        }
      }
      if (isInitializationContext()) {
        return;
      }
      // There is a symbol (but not a function symbol), punt and let variable
      // handling take care of it.
      return;
    } else if (isInitializationContext()) {
      return;
    } else {
      // Assume a void return type.
      SgType *return_type = SageBuilder::buildVoidType();
      if (SageInterface::is_Fortran_language()) {
        SgScopeStatement *decl_scope =
            FindFortranImplicitDeclScope(SageBuilder::topScopeStack());
        if (decl_scope == nullptr) {
          decl_scope = SageBuilder::topScopeStack();
        }
        ASSERT_not_null(decl_scope);

        SgFunctionSymbol *placeholder_sym =
            decl_scope->lookup_function_symbol(SgName(name));
        if (placeholder_sym == nullptr) {
          SgFunctionParameterList *param_list =
              SageBuilder::buildFunctionParameterList_nfi();
          SageBuilder::buildNondefiningProcedureHeaderStatement(
              SgName(name), return_type, param_list,
              SgProcedureHeaderStatement::e_subroutine_subprogram_kind,
              decl_scope);
          placeholder_sym = decl_scope->lookup_function_symbol(SgName(name));
        }

        if (placeholder_sym != nullptr) {
          func_call =
              SageBuilder::buildFunctionCallExp(placeholder_sym, params);
        } else {
          func_call = SB::buildFunctionCallExp(SgName(name), return_type,
                                               params, decl_scope);
        }
      } else {
        func_call = SB::buildFunctionCallExp(SgName(name), return_type, params,
                                             SageBuilder::topScopeStack());
      }
    }
  } else {
    func_call = SageBuilder::buildFunctionCallExp(func_symbol, params);
  }

  ASSERT_not_null(func_call);
  SageInterface::setSourcePosition(func_call);
}

void SageTreeBuilder::Enter(SgCastExp *&cast_expr, const std::string &name,
                            SgExpression *cast_operand) {
  MLOG_TRACE_CXX(MLOG_FRONTEND)
      << "SageTreeBuilder::Enter(SgCastExp* &, ...) \n";

  SgSymbol *symbol = SageInterface::lookupSymbolInParentScopes(
      name, SageBuilder::topScopeStack());

  if (isSgTypedefSymbol(symbol) == nullptr &&
      isSgEnumSymbol(symbol) == nullptr) {
    MLOG_ERROR_CXX(MLOG_FRONTEND)
        << "UNIMPLEMENTED: SageTreeBuilder::Enter(SgCastExp* ...) for name "
        << name;
    ROSE_ABORT();
  }

  SgType *conv_type = symbol->get_type();
  cast_expr = SageBuilder::buildCastExp_nfi(cast_operand, conv_type,
                                            SgCastExp::e_default);
}

void SageTreeBuilder::Enter(SgPntrArrRefExp *&array_ref,
                            const std::string &name, SgExprListExp *subscripts,
                            SgExprListExp *cosubscripts) {
  MLOG_TRACE_CXX(MLOG_FRONTEND)
      << "SageTreeBuilder::Enter(SgPntrArrRefExp* &, ...) \n";

  SgVarRefExp *var_ref = nullptr;
  Enter(var_ref, name, false);
  Leave(var_ref);

  // No cosubscripts for now
  array_ref = SageBuilder::buildPntrArrRefExp_nfi(var_ref, subscripts);
}

void SageTreeBuilder::Enter(SgVarRefExp *&var_ref, const std::string &name,
                            bool compiler_generate) {
  MLOG_TRACE_CXX(MLOG_FRONTEND)
      << "SageTreeBuilder::Enter(SgVarRefExp* &, ...) \n";

  SgVariableSymbol *var_sym = SageInterface::lookupVariableSymbolInParentScopes(
      name, SageBuilder::topScopeStack());
  if (!var_sym && compiler_generate) {
    SgVariableDeclaration *var_decl;

    SgType *type = SageBuilder::buildIntType();

    // Build variable declaration for the control letter
    Enter(var_decl, name, type, nullptr);
    Leave(var_decl);

    var_sym = SageInterface::lookupVariableSymbolInParentScopes(
        name, SageBuilder::topScopeStack());
  }
  ASSERT_not_null(var_sym);

  var_ref = SageBuilder::buildVarRefExp_nfi(var_sym);
}

void SageTreeBuilder::Enter(SgIfStmt *&if_stmt, SgExpression *conditional,
                            SgBasicBlock *true_body, SgBasicBlock *false_body,
                            std::vector<Rose::builder::Token> &comments,
                            bool is_ifthen, bool has_end_stmt,
                            bool is_else_if) {
  MLOG_TRACE_CXX(MLOG_FRONTEND)
      << "SageTreeBuilder::Enter(SgIfStmt* &, ...) \n";

  ASSERT_not_null(conditional);
  ASSERT_not_null(true_body);

  SgStatement *conditional_stmt =
      SageBuilder::buildExprStatement_nfi(conditional);
  if_stmt =
      SageBuilder::buildIfStmt_nfi(conditional_stmt, true_body, false_body);

  if (is_ifthen) {
    if_stmt->set_use_then_keyword(true);
  }
  if (has_end_stmt) {
    if_stmt->set_has_end_statement(true);
  }
  if (is_else_if) {
    if_stmt->set_is_else_if_statement(true);
  }

  attachComments(if_stmt, comments);
}

void SageTreeBuilder::Leave(SgIfStmt *if_stmt) {
  MLOG_TRACE_CXX(MLOG_FRONTEND) << "SageTreeBuilder::Leave(SgIfStmt*) \n";

  ASSERT_not_null(if_stmt);
  SageInterface::appendStatement(if_stmt, SageBuilder::topScopeStack());
}

void SageTreeBuilder::Leave(SgIfStmt *if_stmt,
                            const std::vector<std::string> &labels) {
  MLOG_TRACE_CXX(MLOG_FRONTEND) << "SageTreeBuilder::Leave(SgIfStmt*, ...)\n";

  ASSERT_not_null(if_stmt);
  SgStatement *stmt = wrapStmtWithLabels(if_stmt, labels);
  SageInterface::appendStatement(stmt, SageBuilder::topScopeStack());
}

void SageTreeBuilder::Leave(SgArithmeticIfStatement *if_stmt,
                            const std::vector<std::string> &labels) {
  MLOG_TRACE_CXX(MLOG_FRONTEND)
      << "SageTreeBuilder::Leave(SgArithmeticIfStatement*, ...) \n";

  ASSERT_not_null(if_stmt);
  SgStatement *stmt = wrapStmtWithLabels(if_stmt, labels);
  SageInterface::appendStatement(stmt, SageBuilder::topScopeStack());
}

void SageTreeBuilder::Enter(SgContinueStmt *&continueStmt) {
  MLOG_TRACE_CXX(MLOG_FRONTEND)
      << "SageTreeBuilder::Enter(SgContinueStmt*, ...)\n";

  continueStmt = SB::buildContinueStmt_nfi();
}

void SageTreeBuilder::Leave(SgContinueStmt *continueStmt,
                            const std::vector<std::string> &labels) {
  MLOG_TRACE_CXX(MLOG_FRONTEND)
      << "SageTreeBuilder::Leave(SgContinueStmt*, ...)\n";

  // Append final label statement, if there are labels, otherwise
  // stmt==continueStmt
  SgStatement *stmt = wrapStmtWithLabels(continueStmt, labels);
  SageInterface::appendStatement(stmt, SB::topScopeStack());
}

void SageTreeBuilder::Enter(SgBreakStmt *&breakStmt) {
  MLOG_TRACE_CXX(MLOG_FRONTEND)
      << "SageTreeBuilder::Enter(SgBreakStmt*, ...)\n";

  breakStmt = SB::buildBreakStmt_nfi();
}

void SageTreeBuilder::Leave(SgBreakStmt *breakStmt,
                            const std::vector<std::string> &labels) {
  MLOG_TRACE_CXX(MLOG_FRONTEND)
      << "SageTreeBuilder::Leave(SgBreakStmt*, ...)\n";

  SgStatement *stmt = wrapStmtWithLabels(breakStmt, labels);
  SageInterface::appendStatement(stmt, SB::topScopeStack());
}

void SageTreeBuilder::Enter(SgFortranContinueStmt *&continueStmt) {
  MLOG_TRACE_CXX(MLOG_FRONTEND)
      << "SageTreeBuilder::Enter(SgFortranContinueStmt*, ...)\n";

  continueStmt = SB::buildFortranContinueStmt_nfi();
}

void SageTreeBuilder::Leave(SgFortranContinueStmt *continueStmt,
                            const std::vector<std::string> &labels) {
  MLOG_TRACE_CXX(MLOG_FRONTEND)
      << "SageTreeBuilder::Leave(SgFortranContinueStmt*, ...)\n";

  // Append final label statement, if there are labels, otherwise
  // stmt==continueStmt
  SgStatement *stmt = wrapStmtWithLabels(continueStmt, labels);
  SageInterface::appendStatement(stmt, SB::topScopeStack());
}

void SageTreeBuilder::Enter(SgGotoStatement *&gotoStmt,
                            const std::string &label) {
  MLOG_TRACE_CXX(MLOG_FRONTEND)
      << "SageTreeBuilder::Enter(SgGotoStatement*, ...)\n";

  if (SageInterface::is_Fortran_language()) {
    SgScopeStatement *currentScope = SB::topScopeStack();
    ASSERT_not_null(currentScope);
    SgScopeStatement *labelScope =
        SageInterface::getEnclosingFunctionDefinition(currentScope,
                                                      /*includingSelf*/ true);
    if (labelScope == nullptr) {
      labelScope = SageInterface::getEnclosingScope(currentScope, true);
    }
    ASSERT_not_null(labelScope);
    const int labelValue = std::atoi(label.c_str());
    ROSE_ASSERT(labelValue > 0);
    SgName labelName(label);
    SgLabelSymbol *labelSymbol = labelScope->lookup_label_symbol(labelName);
    if (labelSymbol == nullptr) {
      labelSymbol = new SgLabelSymbol(static_cast<SgLabelStatement *>(nullptr));
      ROSE_ASSERT(labelSymbol != nullptr);
      labelSymbol->set_numeric_label_value(labelValue);
      SgNullStatement *placeholder = SageBuilder::buildNullStatement();
      ROSE_ASSERT(placeholder != nullptr);
      placeholder->set_parent(labelScope);
      labelSymbol->set_fortran_statement(placeholder);
      labelScope->insert_symbol(labelName, labelSymbol);
    }
    SgLabelRefExp *labelRef = SB::buildLabelRefExp(labelSymbol);
    ASSERT_not_null(labelRef);
    gotoStmt = new SgGotoStatement(static_cast<SgLabelStatement *>(nullptr));
    ASSERT_not_null(gotoStmt);
    gotoStmt->set_label_expression(labelRef);
    labelRef->set_parent(gotoStmt);
    return;
  }

  SgLabelStatement *labelStmt{nullptr};
  gotoStmt = nullptr;

  // Ensure a label statement exists for the statement to goto
  if (labels_.find(label) != labels_.end()) {
    labelStmt = labels_[label];
  } else {
    // Build a temporary placeholder
    labelStmt =
        SB::buildLabelStatement_nfi(label, nullptr, SB::topScopeStack());
    labels_[label] = labelStmt;
  }

  ASSERT_not_null(labelStmt);
  gotoStmt = SB::buildGotoStatement_nfi(labelStmt);
}

void SageTreeBuilder::Leave(SgGotoStatement *gotoStmt,
                            const std::vector<std::string> &labels) {
  MLOG_TRACE_CXX(MLOG_FRONTEND)
      << "SageTreeBuilder::Leave(SgGotoStatement*, ...)\n";

  // Append final label statement (if there are labels, otherwise
  // stmt==gotoStmt)
  SgStatement *stmt = wrapStmtWithLabels(gotoStmt, labels);
  SageInterface::appendStatement(stmt, SB::topScopeStack());
}

void SageTreeBuilder::Enter(SgLabelStatement *&labelStmt,
                            const std::string &label) {
  MLOG_TRACE_CXX(MLOG_FRONTEND)
      << "SageTreeBuilder::Enter(SgLabelStatement*, ...)\n";

  labelStmt = nullptr;
  SgScopeStatement *currentScope = SB::topScopeStack();
  ASSERT_not_null(currentScope);
  SgScopeStatement *labelScope = currentScope;
  if (SageInterface::is_Fortran_language()) {
    if (SgScopeStatement *funcScope =
            SageInterface::getEnclosingFunctionDefinition(
                currentScope,
                /*includingSelf*/ true)) {
      labelScope = funcScope;
    }
  }

  // Perhaps a label statement already exists
  if (labels_.find(label) != labels_.end()) {
    labelStmt = labels_[label];
  } else {
    labelStmt = nullptr;
  }
  if (labelStmt != nullptr && SageInterface::is_Fortran_language()) {
    SgScopeStatement *existingScope = labelStmt->get_scope();
    SgScopeStatement *existingLabelScope = existingScope;
    if (existingScope != nullptr) {
      if (SgScopeStatement *funcScope =
              SageInterface::getEnclosingFunctionDefinition(
                  existingScope, /*includingSelf*/ true)) {
        existingLabelScope = funcScope;
      }
    }
    if (existingLabelScope != labelScope) {
      labelStmt = nullptr;
    }
  }
  if (labelStmt == nullptr) {
    // Build a temporary placeholder in the current label scope.
    labelStmt = SB::buildLabelStatement_nfi(label, nullptr, labelScope);
    labels_[label] = labelStmt;
  }
  ASSERT_not_null(labelStmt);
}

void SageTreeBuilder::Leave(SgLabelStatement *labelStmt,
                            const std::vector<std::string> &labels) {
  MLOG_TRACE_CXX(MLOG_FRONTEND)
      << "SageTreeBuilder::Leave(SgGotoStatement*, ...)\n";

  // Append final label statement (if there are labels, otherwise
  // stmt==labelStmt)
  SgStatement *stmt = wrapStmtWithLabels(labelStmt, labels);
  SageInterface::appendStatement(stmt, SB::topScopeStack());
}

void SageTreeBuilder::Enter(SgProcessControlStatement *&control_stmt,
                            const std::string &stmt_kind,
                            const std::optional<SgExpression *> &opt_code,
                            const std::optional<SgExpression *> &opt_quiet) {
  MLOG_TRACE_CXX(MLOG_FRONTEND)
      << "SageTreeBuilder::Enter(SgProcessControlStatement* &, ...) \n";

  SgExpression *code =
      (opt_code) ? *opt_code : SageBuilder::buildNullExpression_nfi();
  SgExpression *quiet =
      (opt_quiet) ? *opt_quiet : SageBuilder::buildNullExpression_nfi();

  ASSERT_not_null(code);
  control_stmt = new SgProcessControlStatement(code);
  ASSERT_not_null(control_stmt);
  SageInterface::setSourcePosition(control_stmt);

  ASSERT_not_null(quiet);
  control_stmt->set_quiet(quiet);

  if (stmt_kind == "abort") {
    control_stmt->set_control_kind(SgProcessControlStatement::e_abort);
  } else if (stmt_kind == "error_stop") {
    control_stmt->set_control_kind(SgProcessControlStatement::e_error_stop);
  } else if (stmt_kind == "exit") {
    control_stmt->set_control_kind(SgProcessControlStatement::e_exit);
  } else if (stmt_kind == "fail_image") {
    control_stmt->set_control_kind(SgProcessControlStatement::e_fail_image);
  } else if (stmt_kind == "pause") {
    control_stmt->set_control_kind(SgProcessControlStatement::e_pause);
  } else if (stmt_kind == "stop") {
    control_stmt->set_control_kind(SgProcessControlStatement::e_stop);
  } else {
    MLOG_FATAL_CXX(MLOG_FRONTEND)
        << "SageTreeBuilder::Enter(SgProcessControlStatement* &, ...): "
           "incorrect statement kind\n";
    ROSE_ABORT();
  }

  code->set_parent(control_stmt);
  quiet->set_parent(control_stmt);
}

void SageTreeBuilder::Leave(SgProcessControlStatement *controlStmt,
                            const std::vector<std::string> &labels) {
  MLOG_TRACE_CXX(MLOG_FRONTEND)
      << "SageTreeBuilder::Leave(SgProcessControlStatement*, ...) \n";
  ASSERT_not_null(controlStmt);

  // Append final label statement (if there are labels, otherwise
  // stmt==controlStmt)
  SgStatement *stmt = wrapStmtWithLabels(controlStmt, labels);
  SageInterface::appendStatement(stmt, SB::topScopeStack());
}

void SageTreeBuilder::Enter(SgSwitchStatement *&switch_stmt,
                            SgExpression *selector,
                            const SourcePositionPair &sources) {
  MLOG_TRACE_CXX(MLOG_FRONTEND)
      << "SageTreeBuilder::Enter(SgSwitchStatement* &, ...) \n";

  ASSERT_not_null(selector);
  SgExprStatement *selector_stmt =
      SageBuilder::buildExprStatement_nfi(selector);
  SgBasicBlock *body = SageBuilder::buildBasicBlock_nfi();
  body->set_scope(SageBuilder::topScopeStack());

  switch_stmt = SageBuilder::buildSwitchStatement_nfi(selector_stmt, body);

  // Append before push (so that symbol lookup will work)
  SageInterface::appendStatement(switch_stmt, SageBuilder::topScopeStack());
  SageBuilder::pushScopeStack(body);
}

void SageTreeBuilder::Leave(SgSwitchStatement *switch_stmt) {
  MLOG_TRACE_CXX(MLOG_FRONTEND)
      << "SageTreeBuilder::Leave(SgSwitchStatement*, ...) \n";
  ASSERT_not_null(switch_stmt);

  SageBuilder::popScopeStack(); // switch statement body
}

void SageTreeBuilder::Enter(SgReturnStmt *&return_stmt,
                            const std::optional<SgExpression *> &opt_expr) {
  MLOG_TRACE_CXX(MLOG_FRONTEND)
      << "SageTreeBuilder::Enter(SgReturnStmt* &, ...) \n";

  SgExpression *return_expr =
      (opt_expr) ? *opt_expr : SageBuilder::buildNullExpression_nfi();
  ASSERT_not_null(return_expr);

  return_stmt = SageBuilder::buildReturnStmt_nfi(return_expr);
}

void SageTreeBuilder::Leave(SgReturnStmt *return_stmt) {
  MLOG_TRACE_CXX(MLOG_FRONTEND)
      << "SageTreeBuilder::Leave(SgReturnStmt*, ...) \n";
  ASSERT_not_null(return_stmt);

  SageInterface::appendStatement(return_stmt, SageBuilder::topScopeStack());
}

void SageTreeBuilder::Leave(SgReturnStmt *return_stmt,
                            const std::vector<std::string> &labels) {
  MLOG_TRACE_CXX(MLOG_FRONTEND)
      << "SageTreeBuilder::Leave(SgReturnStmt*, ...) with labels \n";
  ASSERT_not_null(return_stmt);

  SgStatement *stmt = wrapStmtWithLabels(return_stmt, labels);
  SageInterface::appendStatement(stmt, SageBuilder::topScopeStack());
}

void SageTreeBuilder::Enter(SgCaseOptionStmt *&case_option_stmt,
                            SgExprListExp *key) {
  MLOG_TRACE_CXX(MLOG_FRONTEND)
      << "SageTreeBuilder::Enter(SgCaseOptionStmt* &, ...) \n";
  ASSERT_not_null(key);

  SgBasicBlock *body = SageBuilder::buildBasicBlock_nfi();
  body->set_scope(SageBuilder::topScopeStack());
  case_option_stmt = SageBuilder::buildCaseOptionStmt_nfi(key, body);

  // Append before push (so that symbol lookup will work)
  SageInterface::appendStatement(case_option_stmt,
                                 SageBuilder::topScopeStack());
  SageBuilder::pushScopeStack(body);
}

void SageTreeBuilder::Leave(SgCaseOptionStmt *case_option_stmt) {
  MLOG_TRACE_CXX(MLOG_FRONTEND)
      << "SageTreeBuilder::Leave(SgCaseOptionStmt*, ...) \n";
  ASSERT_not_null(case_option_stmt);

  SageBuilder::popScopeStack(); // case_option_stmt body
}

void SageTreeBuilder::Enter(SgDefaultOptionStmt *&default_option_stmt) {
  MLOG_TRACE_CXX(MLOG_FRONTEND)
      << "SageTreeBuilder::Enter(SgDefautlOptionStmt* &, ...) \n";

  SgBasicBlock *body = SageBuilder::buildBasicBlock_nfi();
  body->set_scope(SageBuilder::topScopeStack());
  default_option_stmt = SageBuilder::buildDefaultOptionStmt(body);

  // Append before push (so that symbol lookup will work)
  SageInterface::appendStatement(default_option_stmt,
                                 SageBuilder::topScopeStack());
  SageBuilder::pushScopeStack(body);
}

void SageTreeBuilder::Leave(SgDefaultOptionStmt *default_option_stmt) {
  MLOG_TRACE_CXX(MLOG_FRONTEND)
      << "SageTreeBuilder::Leave(SgDefautlOptionStmt*, ...) \n";
  ASSERT_not_null(default_option_stmt);

  SageBuilder::popScopeStack(); // default_option_stmt body
}

void SageTreeBuilder::Enter(SgFortranDo *&doStmt, SgExpression *initialization,
                            SgExpression *bound, SgExpression *increment) {
  MLOG_TRACE_CXX(MLOG_FRONTEND)
      << "SageTreeBuilder::Enter(SgDoWhileStmt* &, ...) \n";

  auto body = SageBuilder::buildBasicBlock_nfi();
  body->set_scope(SageBuilder::topScopeStack());
  doStmt = SB::buildFortranDo_nfi(initialization, bound, increment, body);

  // output "END DO"
  doStmt->set_has_end_statement(true);

  // Append before push (so that symbol lookup will work)
  SageInterface::appendStatement(doStmt, SageBuilder::topScopeStack());
  SageBuilder::pushScopeStack(body);
}

void SageTreeBuilder::Leave(SgFortranDo *doStmt) {
  MLOG_TRACE_CXX(MLOG_FRONTEND)
      << "SageTreeBuilder::Leave(SgFortranDo*, ...) \n";
  ASSERT_not_null(doStmt);

  attachComments(doStmt, PosInfo{doStmt}, /*at_end*/ true);
  SageBuilder::popScopeStack(); // do statement body
}

void SageTreeBuilder::Enter(SgPrintStatement *&print_stmt, SgExpression *format,
                            std::list<SgExpression *> &expr_list) {
  MLOG_TRACE_CXX(MLOG_FRONTEND)
      << "SageTreeBuilder::Enter(SgPrintStmt* &, ...) \n";

  ASSERT_not_null(format);

  print_stmt = new SgPrintStatement();
  ASSERT_not_null(print_stmt);
  SageInterface::setSourcePosition(print_stmt);

  print_stmt->set_format(format);
  format->set_parent(print_stmt);

  SgExprListExp *io_stmt_list =
      SageBuilderCpp17::buildExprListExp_nfi(expr_list);
  io_stmt_list->set_parent(print_stmt);
  for (SgExpression *expr : io_stmt_list->get_expressions()) {
    if (expr != nullptr) {
      expr->set_parent(io_stmt_list);
    }
  }
  print_stmt->set_io_stmt_list(io_stmt_list);
}

void SageTreeBuilder::Leave(SgPrintStatement *print_stmt) {
  MLOG_TRACE_CXX(MLOG_FRONTEND)
      << "SageTreeBuilder::Leave(SgPrintStmt*, ...) \n";
  ASSERT_not_null(print_stmt);

  SageInterface::appendStatement(print_stmt, SageBuilder::topScopeStack());
}

void SageTreeBuilder::Leave(SgPrintStatement *print_stmt,
                            const std::vector<std::string> &labels) {
  MLOG_TRACE_CXX(MLOG_FRONTEND)
      << "SageTreeBuilder::Leave(SgPrintStmt*, ...) \n";
  ASSERT_not_null(print_stmt);

  SgStatement *stmt = wrapStmtWithLabels(print_stmt, labels);
  SageInterface::appendStatement(stmt, SageBuilder::topScopeStack());
}

void SageTreeBuilder::Leave(SgFormatStatement *format_stmt,
                            const std::vector<std::string> &labels) {
  MLOG_TRACE_CXX(MLOG_FRONTEND)
      << "SageTreeBuilder::Leave(SgFormatStatement*, ...) \n";
  ASSERT_not_null(format_stmt);

  SgStatement *stmt = wrapStmtWithLabels(format_stmt, labels);
  SageInterface::appendStatement(stmt, SageBuilder::topScopeStack());
}

void SageTreeBuilder::Enter(SgWhileStmt *&while_stmt, SgExpression *condition) {
  MLOG_TRACE_CXX(MLOG_FRONTEND)
      << "SageTreeBuilder::Enter(SgWhileStmt* &, ...) \n";
  ASSERT_not_null(condition);

  SgExprStatement *condition_stmt =
      SageBuilder::buildExprStatement_nfi(condition);
  SgBasicBlock *body = SageBuilder::buildBasicBlock_nfi();
  body->set_scope(SageBuilder::topScopeStack());

  while_stmt = SageBuilder::buildWhileStmt_nfi(condition_stmt, body);

  // Append before push (so that symbol lookup will work)
  SageInterface::appendStatement(while_stmt, SageBuilder::topScopeStack());
  SageBuilder::pushScopeStack(body);
}

void SageTreeBuilder::Leave(SgWhileStmt *while_stmt, bool has_end_do_stmt) {
  MLOG_TRACE_CXX(MLOG_FRONTEND)
      << "SageTreeBuilder::Leave(SgWhileStmt*, ...) \n";
  ASSERT_not_null(while_stmt);

  // The default value of has_end_do_stmt is false so if true,
  // then the language supports it and it needs to be set.
  if (has_end_do_stmt) {
    while_stmt->set_has_end_statement(true);
  }

  SageBuilder::popScopeStack(); // while statement body
}

void SageTreeBuilder::Enter(SgImplicitStatement *&implicit_stmt,
                            bool none_external, bool none_type) {
  MLOG_TRACE_CXX(MLOG_FRONTEND) << "SageTreeBuilder::Enter(SgImplicitStatement*"
                                   " &, bool none_external, bool none_type)\n";
  // Implicit None

  implicit_stmt = new SgImplicitStatement(true /* implicit none*/);
  ASSERT_not_null(implicit_stmt);
  implicit_stmt->set_definingDeclaration(implicit_stmt);
  implicit_stmt->set_firstNondefiningDeclaration(implicit_stmt);
  SageInterface::setSourcePosition(implicit_stmt);

  if (none_external && none_type) {
    implicit_stmt->set_implicit_spec(
        SgImplicitStatement::e_none_external_and_type);
  } else if (none_external) {
    implicit_stmt->set_implicit_spec(SgImplicitStatement::e_none_external);
  } else if (none_type) {
    implicit_stmt->set_implicit_spec(SgImplicitStatement::e_none_type);
  }
}

void SageTreeBuilder::Enter(
    SgImplicitStatement *&implicit_stmt,
    std::list<
        std::tuple<SgType *, std::list<std::tuple<char, std::optional<char>>>>>
        &implicit_spec_list) {
  MLOG_TRACE_CXX(MLOG_FRONTEND)
      << "SageTreeBuilder::Enter(SgImplicitStatement* &, implicit_spec_list)\n";
  // Implicit with Implicit-Spec

  implicit_stmt = new SgImplicitStatement(false);
  ASSERT_not_null(implicit_stmt);
  implicit_stmt->set_definingDeclaration(implicit_stmt);
  implicit_stmt->set_firstNondefiningDeclaration(implicit_stmt);
  SageInterface::setSourcePosition(implicit_stmt);
  implicit_stmt->set_implicit_spec(
      SgImplicitStatement::e_has_implicit_spec_list);

  SgInitializedNamePtrList &name_list = implicit_stmt->get_variables();
  SgScopeStatement *scope = SageBuilder::topScopeStack();
  ASSERT_not_null(scope);

  // Step through the list of Implicit Specs
  for (std::tuple<SgType *, std::list<std::tuple<char, std::optional<char>>>>
           implicit_spec : implicit_spec_list) {
    SgType *type;
    std::list<std::tuple<char, std::optional<char>>> letter_spec_list;
    std::tie(type, letter_spec_list) = implicit_spec;

    // Traverse the list of letter specs
    for (std::tuple<char, std::optional<char>> letter_spec : letter_spec_list) {
      char first;
      std::optional<char> second;
      std::tie(first, second) = letter_spec;
      if (first == '\0') {
        continue;
      }

      std::string name(1, first);
      if (second) {
        name += "-";
        name.push_back(*second);
      }

      SgInitializedName *init_name = SageBuilder::buildInitializedName_nfi(
          name, type, /*initializer*/ nullptr);
      ASSERT_not_null(init_name);
      SageInterface::setSourcePosition(init_name);
      init_name->set_scope(scope);
      init_name->set_parent(implicit_stmt);
      name_list.push_back(init_name);
    }
  }
}

void SageTreeBuilder::Leave(SgImplicitStatement *implicit_stmt) {
  MLOG_TRACE_CXX(MLOG_FRONTEND)
      << "SageTreeBuilder::Leave(SgImplicitStatement*, ...) \n";
  ASSERT_not_null(implicit_stmt);

  SageInterface::appendStatement(implicit_stmt, SageBuilder::topScopeStack());
}

void SageTreeBuilder::Enter(SgModuleStatement *&module_stmt,
                            const std::string &name) {
  MLOG_TRACE_CXX(MLOG_FRONTEND)
      << "SageTreeBuilder::Enter(SgModuleStatement* &, ...)\n";

  module_stmt =
      SageBuilder::buildModuleStatement(name, SageBuilder::topScopeStack());

  SgClassDefinition *class_def = module_stmt->get_definition();
  ASSERT_not_null(class_def);

  // Append now (before Leave is called) so that symbol lookup will work
  SageInterface::appendStatement(module_stmt, SageBuilder::topScopeStack());
  SageBuilder::pushScopeStack(class_def);
}

void SageTreeBuilder::Leave(SgModuleStatement *module_stmt) {
  MLOG_TRACE_CXX(MLOG_FRONTEND)
      << "SageTreeBuilder::Leave(SgModuleStatement*, ...) \n";
  ASSERT_not_null(module_stmt);

  SageBuilder::popScopeStack(); // class definition
}

void SageTreeBuilder::Enter(SgUseStatement *&use_stmt,
                            const std::string &module_name,
                            const std::string &module_nature) {
  MLOG_TRACE_CXX(MLOG_FRONTEND)
      << "SageTreeBuilder::Enter(SgUseStatement* &, ...)\n";

  use_stmt = new SgUseStatement(module_name, false, module_nature);
  ASSERT_not_null(use_stmt);
  SageInterface::setSourcePosition(use_stmt);

  SgClassSymbol *module_symbol =
      SageInterface::lookupClassSymbolInParentScopes(module_name);
  ASSERT_not_null(module_symbol);

  SgClassDeclaration *decl = module_symbol->get_declaration();
  ASSERT_not_null(decl);

  SgModuleStatement *module_stmt = isSgModuleStatement(decl);
  ASSERT_not_null(module_stmt);

  use_stmt->set_module(module_stmt);
}

void SageTreeBuilder::Leave(SgUseStatement *use_stmt) {
  MLOG_TRACE_CXX(MLOG_FRONTEND)
      << "SageTreeBuilder::Leave(SgUseStatement*, ...) \n";
  ASSERT_not_null(use_stmt);

  SageInterface::appendStatement(use_stmt, SageBuilder::topScopeStack());
}

void SageTreeBuilder::Enter(SgContainsStatement *&contains_stmt) {
  MLOG_TRACE_CXX(MLOG_FRONTEND)
      << "SageTreeBuilder::Enter(SgContainsStatement* &, ...)\n";

  contains_stmt = new SgContainsStatement();
  ASSERT_not_null(contains_stmt);
  contains_stmt->set_definingDeclaration(contains_stmt);
  contains_stmt->set_firstNondefiningDeclaration(contains_stmt);
  SageInterface::setSourcePosition(contains_stmt);
}

void SageTreeBuilder::Leave(SgContainsStatement *contains_stmt) {
  MLOG_TRACE_CXX(MLOG_FRONTEND)
      << "SageTreeBuilder::Leave(SgContainsStatement*, ...) \n";
  ASSERT_not_null(contains_stmt);

  SageInterface::appendStatement(contains_stmt, SageBuilder::topScopeStack());
}

void SageTreeBuilder::Enter(SgVariableDeclaration *&var_decl,
                            const std::string &name, SgType *type,
                            SgExpression *init_expr) {
  MLOG_TRACE_CXX(MLOG_FRONTEND)
      << "SageTreeBuilder::Enter(SgVariableDeclaration* &, ...) \n";

  ASSERT_not_null(type);

  if (SageInterface::is_Fortran_language() &&
      (isSgFunctionType(type) != nullptr ||
       isSgMemberFunctionType(type) != nullptr)) {
    var_decl = BuildFunctionTypeVarDecl(name, type, init_expr,
                                        SageBuilder::topScopeStack());
    return;
  }

  SgName var_name = name;
  SgInitializer *var_init = nullptr;

  if (init_expr) {
    var_init = SageBuilder::buildAssignInitializer_nfi(init_expr, type);
  }

  // Reset pointer base-type name so the base type can be replaced when it has
  // been declared
  if (SgPointerType *pointer = isSgPointerType(type)) {
    if (SgTypeUnknown *unknown = isSgTypeUnknown(pointer->get_base_type())) {
      // Reset the type name to the variable name. This allows the variable
      // symbol for name to be found from the forward_type_refs_ map of
      // pointers.
      unknown->set_type_name(name);
    }
  }

  var_decl = SB::buildVariableDeclaration_nfi(var_name, type, var_init,
                                              SB::topScopeStack());

  if (var_decl->get_definingDeclaration() == nullptr) {
    var_decl->set_definingDeclaration(var_decl);
  }

  SgVariableDefinition *var_def = var_decl->get_definition();
  ASSERT_not_null(var_def);

  SgInitializedName *init_name = var_decl->get_decl_item(var_name);
  ASSERT_not_null(init_name);

  SgDeclarationStatement *decl_ptr = init_name->get_declptr();
  ASSERT_not_null(decl_ptr);
  ASSERT_require(decl_ptr == var_def);

  SgInitializedName *var_defn = var_def->get_vardefn();
  ASSERT_not_null(var_defn);
  ASSERT_require(var_defn == init_name);

  SI::appendStatement(var_decl, SB::topScopeStack());

  // Look for a symbol previously implicitly declared and fix the variable
  // reference
  if (forward_var_refs_.find(name) != forward_var_refs_.end()) {
    if (SgVariableSymbol *var_sym =
            SI::lookupVariableSymbolInParentScopes(name)) {
      auto range = forward_var_refs_.equal_range(name);
      for (auto it = range.first; it != range.second; ++it) {
        SgVarRefExp *prev_var_ref = it->second;
        SgVariableSymbol *prev_var_sym = prev_var_ref->get_symbol();
        ASSERT_not_null(prev_var_sym);

        SgInitializedName *prev_init_name = prev_var_sym->get_declaration();
        ASSERT_require(prev_init_name->get_name() == init_name->get_name());

        // Reset the symbol for the variable reference to the symbol for the
        // explicit variable declaration
        prev_var_ref->set_symbol(var_sym);

        // Detach the placeholder symbol from the scope and leave cleanup to
        // the normal AST lifecycle.
        SgScopeStatement *prev_scope = prev_var_sym->get_scope();
        ASSERT_not_null(prev_scope);
        if (prev_scope->symbol_exists(prev_var_sym)) {
          prev_scope->remove_symbol(prev_var_sym);
        }
      }
      // Remove all variable refs associated with name
      forward_var_refs_.erase(name);
    }
  }
}

void SageTreeBuilder::Enter(
    SgVariableDeclaration *&var_decl, SgType *base_type,
    std::list<std::tuple<std::string, SgType *, SgExpression *>> &init_info) {
  MLOG_TRACE_CXX(MLOG_FRONTEND)
      << "SageTreeBuilder::Enter(SgVariableDeclaration* &, std::tuple<...>, "
         "...) \n";

  // Step through list of tuples to create the multi variable declaration
  for (std::list<std::tuple<std::string, SgType *, SgExpression *>>::iterator
           it = init_info.begin();
       it != init_info.end(); ++it) {
    std::string name;
    SgType *type;
    SgExpression *init_expr;
    std::tie(name, type, init_expr) = *it;

    if (!type) {
      type = base_type;
    }

    if (it == init_info.begin()) { // On first pass, call Enter() to create
                                   // variable declaration
      Enter(var_decl, name, type, init_expr);
    } else { // On later passes, create new initialized name and append to the
             // var decl
      SgAssignInitializer *init = nullptr;
      if (init_expr) {
        init = SageBuilder::buildAssignInitializer_nfi(init_expr, type);
      }

      SgInitializedName *init_name =
          SageBuilder::buildInitializedName_nfi(name, type, init);
      var_decl->append_variable(init_name, init);
      init_name->set_declptr(var_decl);
      init_name->set_parent(var_decl);
      init_name->set_scope(SageBuilder::topScopeStack());
      if (init != nullptr) {
        init->set_parent(init_name);
      }

      // A symbol for the variable also has to be created
      SgVariableSymbol *var_sym = new SgVariableSymbol(init_name);
      ASSERT_not_null(var_sym);
      SgScopeStatement *scope = SageBuilder::topScopeStack();
      ASSERT_not_null(scope);
      scope->insert_symbol(SgName(name), var_sym);

      // Fix any dangling variable references from prior implicit use.
      if (forward_var_refs_.find(name) != forward_var_refs_.end()) {
        auto range = forward_var_refs_.equal_range(name);
        for (auto it = range.first; it != range.second; ++it) {
          SgVarRefExp *prev_var_ref = it->second;
          SgVariableSymbol *prev_var_sym = prev_var_ref->get_symbol();
          ASSERT_not_null(prev_var_sym);

          SgInitializedName *prev_init_name = prev_var_sym->get_declaration();
          ASSERT_require(prev_init_name->get_name() == init_name->get_name());

          prev_var_ref->set_symbol(var_sym);

          // Detach the placeholder symbol from the scope and leave cleanup to
          // the normal AST lifecycle.
          SgScopeStatement *prev_scope = prev_var_sym->get_scope();
          ASSERT_not_null(prev_scope);
          if (prev_scope->symbol_exists(prev_var_sym)) {
            prev_scope->remove_symbol(prev_var_sym);
          }
        }
        forward_var_refs_.erase(name);
      }
    }
  }
}

void SageTreeBuilder::Leave(SgVariableDeclaration *var_decl) {
  MLOG_TRACE_CXX(MLOG_FRONTEND)
      << "SageTreeBuilder::Leave(SgVariableDeclaration*) \n";
}

void SageTreeBuilder::Leave(
    SgVariableDeclaration *var_decl,
    std::list<LanguageTranslation::ExpressionKind> &modifier_enum_list) {
  MLOG_TRACE_CXX(MLOG_FRONTEND)
      << "SageTreeBuilder::Leave(SgVariableDeclaration*) with modifiers \n";

  auto apply_const_to_decl_types = [](SgVariableDeclaration *decl) {
    if (decl == nullptr) {
      return;
    }
    for (SgInitializedName *init_name : decl->get_variables()) {
      if (init_name == nullptr) {
        continue;
      }
      SgType *type = init_name->get_type();
      if (type != nullptr && !SageInterface::isConstType(type)) {
        init_name->set_type(SageBuilder::buildConstType(type));
      }
    }
  };

  for (LanguageTranslation::ExpressionKind modifier_enum : modifier_enum_list) {
    switch (modifier_enum) {
    case LanguageTranslation::ExpressionKind::e_type_modifier_intent_in: {
      var_decl->get_declarationModifier().get_typeModifier().setIntent_in();
      break;
    }
    case LanguageTranslation::ExpressionKind::e_type_modifier_intent_out: {
      var_decl->get_declarationModifier().get_typeModifier().setIntent_out();
      break;
    }
    case LanguageTranslation::ExpressionKind::e_type_modifier_intent_inout: {
      var_decl->get_declarationModifier().get_typeModifier().setIntent_inout();
      break;
    }
    case LanguageTranslation::ExpressionKind::e_type_modifier_bind_c: {
      var_decl->get_declarationModifier().setBind();
      break;
    }
    case LanguageTranslation::ExpressionKind::e_type_modifier_parameter: {
      var_decl->get_declarationModifier()
          .get_typeModifier()
          .get_constVolatileModifier()
          .setConst();
      apply_const_to_decl_types(var_decl);
      break;
    }
    case LanguageTranslation::ExpressionKind::e_type_modifier_const: {
      var_decl->get_declarationModifier()
          .get_typeModifier()
          .get_constVolatileModifier()
          .setConst();
      apply_const_to_decl_types(var_decl);
      break;
    }
    case LanguageTranslation::ExpressionKind::e_type_modifier_allocatable: {
      var_decl->get_declarationModifier().get_typeModifier().setAllocatable();
      break;
    }
    case LanguageTranslation::ExpressionKind::e_type_modifier_asynchronous: {
      var_decl->get_declarationModifier().get_typeModifier().setAsynchronous();
      break;
    }
    case LanguageTranslation::ExpressionKind::e_type_modifier_intrinsic: {
      var_decl->get_declarationModifier().get_typeModifier().setIntrinsic();
      break;
    }
    case LanguageTranslation::ExpressionKind::e_type_modifier_optional: {
      var_decl->get_declarationModifier().get_typeModifier().setOptional();
      break;
    }
    case LanguageTranslation::ExpressionKind::e_type_modifier_save: {
      var_decl->get_declarationModifier().get_typeModifier().setSave();
      break;
    }
    case LanguageTranslation::ExpressionKind::e_type_modifier_target: {
      var_decl->get_declarationModifier().get_typeModifier().setTarget();
      break;
    }
    case LanguageTranslation::ExpressionKind::e_type_modifier_volatile: {
      var_decl->get_declarationModifier()
          .get_typeModifier()
          .get_constVolatileModifier()
          .setVolatile();
      break;
    }
    case LanguageTranslation::ExpressionKind::e_param_binding_value: {
      var_decl->get_declarationModifier().get_typeModifier().setValue();
      break;
    }
    case LanguageTranslation::ExpressionKind::e_storage_modifier_contiguous: {
      var_decl->get_declarationModifier().get_storageModifier().setContiguous();
      break;
    }
    case LanguageTranslation::ExpressionKind::e_storage_modifier_external: {
      var_decl->get_declarationModifier().get_storageModifier().setExtern();
      break;
    }
    case LanguageTranslation::ExpressionKind::
        e_storage_modifier_cuda_constant: {
      var_decl->get_declarationModifier()
          .get_storageModifier()
          .setCudaConstant();
      break;
    }
    case LanguageTranslation::ExpressionKind::e_storage_modifier_cuda_device: {
      var_decl->get_declarationModifier()
          .get_storageModifier()
          .setCudaDeviceMemory();
      break;
    }
    case LanguageTranslation::ExpressionKind::e_storage_modifier_cuda_managed: {
      var_decl->get_declarationModifier()
          .get_storageModifier()
          .setCudaManaged();
      break;
    }
    case LanguageTranslation::ExpressionKind::e_storage_modifier_cuda_pinned: {
      var_decl->get_declarationModifier().get_storageModifier().setCudaPinned();
      break;
    }
    case LanguageTranslation::ExpressionKind::e_storage_modifier_cuda_shared: {
      var_decl->get_declarationModifier().get_storageModifier().setCudaShared();
      break;
    }
    case LanguageTranslation::ExpressionKind::e_storage_modifier_cuda_texture: {
      var_decl->get_declarationModifier()
          .get_storageModifier()
          .setCudaTexture();
      break;
    }
    case LanguageTranslation::ExpressionKind::e_storage_modifier_cuda_unified: {
      var_decl->get_declarationModifier()
          .get_storageModifier()
          .setCudaUnified();
      break;
    }
    case LanguageTranslation::ExpressionKind::e_access_modifier_public: {
      var_decl->get_declarationModifier().get_accessModifier().setPublic();
      break;
    }
    case LanguageTranslation::ExpressionKind::e_access_modifier_private: {
      var_decl->get_declarationModifier().get_accessModifier().setPrivate();
      break;
    }
    case LanguageTranslation::ExpressionKind::e_type_modifier_pointer: {
      for (SgInitializedName *init_name : var_decl->get_variables()) {
        SgType *type = init_name->get_type();
        if (type != nullptr && isSgPointerType(type) == nullptr) {
          init_name->set_type(SageBuilder::buildPointerType(type));
        }
      }
      break;
    }
    case LanguageTranslation::ExpressionKind::e_type_modifier_protected: {
      for (SgInitializedName *init_name : var_decl->get_variables()) {
        init_name->set_protected_declaration(true);
      }
      break;
    }
    default:
      break;
    }
  }

  Leave(var_decl);
}

void SageTreeBuilder::Enter(SgEnumDeclaration *&enum_decl,
                            const std::string &name) {
  MLOG_TRACE_CXX(MLOG_FRONTEND)
      << "SageTreeBuilder::Enter(SgEnumDeclaration* &, ...) \n";

  enum_decl =
      SageBuilder::buildEnumDeclaration_nfi(name, SageBuilder::topScopeStack());
}

void SageTreeBuilder::Leave(SgEnumDeclaration *enum_decl) {
  MLOG_TRACE_CXX(MLOG_FRONTEND)
      << "SageTreeBuilder::Leave(SgEnumDeclaration*) \n";

  SageInterface::appendStatement(enum_decl, SageBuilder::topScopeStack());
}

void SageTreeBuilder::Enter(SgEnumVal *&enum_val, const std::string &name,
                            SgEnumDeclaration *enum_decl, int value,
                            SgCastExp *cast) {
  MLOG_TRACE_CXX(MLOG_FRONTEND) << "SageTreeBuilder::Enter(SgEnumVal*) \n";

  ASSERT_not_null(enum_decl);
  SgEnumType *enum_type = enum_decl->get_type();
  SgScopeStatement *scope = enum_decl->get_scope();

  SgEnumDeclaration *def_decl =
      isSgEnumDeclaration(enum_decl->get_definingDeclaration());
  ASSERT_not_null(def_decl);
  SgEnumDeclaration *nondef_decl =
      isSgEnumDeclaration(enum_decl->get_firstNondefiningDeclaration());
  ASSERT_not_null(nondef_decl);

  // There doesn't have to be an SgEnumVal, there shall be an SgInitializedName
  SgExpression *init_expr = nullptr;
  if (cast) {
    init_expr = cast;
  } else {
    enum_val = SageBuilder::buildEnumVal_nfi(value, nondef_decl, name);
    init_expr = enum_val;
  }

  SgAssignInitializer *initializer =
      SageBuilder::buildAssignInitializer_nfi(init_expr, enum_type);
  SgInitializedName *init_name =
      SageBuilder::buildInitializedName_nfi(name, enum_type, initializer);

  def_decl->get_enumerators().push_back(init_name);
  init_name->set_scope(scope);
  init_name->set_declptr(def_decl);
  init_name->set_parent(def_decl);

  // Add an associated field symbol to the symbol table
  SgEnumFieldSymbol *enum_field_symbol = new SgEnumFieldSymbol(init_name);
  ASSERT_not_null(enum_field_symbol);
  scope->insert_symbol(name, enum_field_symbol);

  if (enum_type->get_parent() == nullptr) {
    enum_type->set_parent(enum_field_symbol);
  }

  // Also add enum alias to global scope as StatusConstants are globally visible
  auto global_scope = SI::getGlobalScope(scope);
  auto alias_sym = new SgAliasSymbol(enum_field_symbol);
  ASSERT_not_null(global_scope);
  ASSERT_not_null(alias_sym);
  global_scope->insert_symbol(name, alias_sym);
}

void SageTreeBuilder::Enter(SgTypedefDeclaration *&type_def,
                            const std::string &name, SgType *type) {
  MLOG_TRACE_CXX(MLOG_FRONTEND)
      << "SageTreeBuilder::Enter(SgTypedefDeclaration*) \n";
  SgScopeStatement *scope = SageBuilder::topScopeStack();

  type_def = SageBuilder::buildTypedefDeclaration_nfi(name, type, scope, false);

  // These things should be setup properly in SageBuilder?
  SgTypedefSymbol *symbol =
      SageInterface::lookupTypedefSymbolInParentScopes(name, scope);
  ASSERT_not_null(symbol);
  SgTypedefType *typedef_type = type_def->get_type();
  ASSERT_not_null(typedef_type);

  type_def->set_base_type(type);
  type_def->set_parent_scope(symbol);
  typedef_type->set_parent_scope(symbol);
  ASSERT_not_null(type_def->get_parent_scope());
  ASSERT_not_null(typedef_type->get_parent_scope());

  // Fix forward type references
  reset_forward_type_refs(name, type_def->get_type());

  SageInterface::appendStatement(type_def, SageBuilder::topScopeStack());
}

void SageTreeBuilder::Leave(SgTypedefDeclaration *type_def) {
  MLOG_TRACE_CXX(MLOG_FRONTEND)
      << "SageTreeBuilder::Leave(SgTypedefDeclaration*) \n";
}

// Fortran specific nodes

void SageTreeBuilder::Enter(
    SgCommonBlock *&common_block,
    std::list<SgCommonBlockObject *> &common_block_object_list) {
  MLOG_TRACE_CXX(MLOG_FRONTEND)
      << "SageTreeBuilder::Enter(SgCommonBlock* &, ...) \n";

  common_block = SageBuilder::buildCommonBlock();
  SageInterface::setSourcePosition(common_block);

  SgCommonBlockObjectPtrList &list = common_block->get_block_list();

  for (SgCommonBlockObject *common_block_object : common_block_object_list) {
    list.push_back(common_block_object);
  }
}

void SageTreeBuilder::Leave(SgCommonBlock *common_block) {
  MLOG_TRACE_CXX(MLOG_FRONTEND) << "SageTreeBuilder::Leave(SgCommonBlock*) \n";

  ASSERT_not_null(common_block);
  SageInterface::appendStatement(common_block, SageBuilder::topScopeStack());
}

// Some languages allow implicitly declared variables but require there to
// be an explicit declaration at some point (unlike Fortran). This builder
// function manages name and symbol information so that the variable reference
// can be cleaned/fixed up when the explicit declaration is seen.
SgVarRefExp *SageTreeBuilder::buildVarRefExp_nfi(const std::string &name) {
  SgVarRefExp *var_ref =
      SageBuilder::buildVarRefExp(name, SageBuilder::topScopeStack());
  ASSERT_not_null(var_ref);
  SageInterface::setSourcePosition(var_ref);

  if (SageInterface::lookupSymbolInParentScopes(name) == nullptr) {
    forward_var_refs_.insert({name, var_ref});
  }
  return var_ref;
}

// Some languages allow pointers to types which haven't been declared yet. This
// builder function manages type name and symbol information so that the pointer
// variable reference can be cleaned/fixed up when the explicit type declaration
// is seen.
SgPointerType *
SageTreeBuilder::buildPointerType(const std::string &base_type_name,
                                  SgType *base_type) {
  SgPointerType *type = nullptr;

  if (base_type == nullptr) {
    // Constructors are used here rather than SageBuilder functions because
    // these types will be replaced (and deleted) once the actual base type is
    // declared.
    SgTypeUnknown *unknown = new SgTypeUnknown();
    ASSERT_not_null(unknown);
    unknown->set_type_name(base_type_name);

    type = new SgPointerType(unknown);
    ASSERT_not_null(type);

    forward_type_refs_.insert(std::make_pair(base_type_name, type));
  } else {
    type = SageBuilder::buildPointerType(base_type);
  }
  ASSERT_not_null(type);

  return type;
}

void SageTreeBuilder::reset_forward_type_refs(const std::string &type_name,
                                              SgNamedType *type) {
  auto range = forward_type_refs_.equal_range(type_name);

  bool present = false;
  for (auto pair = range.first; pair != range.second; pair++) {
    present = true;
    SgPointerType *ptr = pair->second;
    ASSERT_not_null(ptr);

    // The placeholder
    SgTypeUnknown *unknown = isSgTypeUnknown(ptr->get_base_type());
    ASSERT_not_null(unknown);

    // The type name have been replaced by the variable name by this point
    const std::string &var_name = unknown->get_type_name();
    SgVariableSymbol *var_sym =
        SageInterface::lookupVariableSymbolInParentScopes(var_name);
    ASSERT_not_null(var_sym);

    SgInitializedName *init_name = var_sym->get_declaration();
    ASSERT_not_null(init_name);

    SgPointerType *new_pointer = SageBuilder::buildPointerType(type);
    init_name->set_type(new_pointer);

    // Leave placeholder types alive; AST nodes are memory-pooled.
  }

  // Remove the type name from the multimap
  if (present) {
    forward_type_refs_.erase(type_name);
  }
}

SgStatement *
SageTreeBuilder::wrapStmtWithLabels(SgStatement *stmt,
                                    const std::vector<std::string> &labels) {
  ASSERT_not_null(stmt);

  // Order of the labels need to be reversed to come out right
  std::vector<std::string> reversed = labels;
  auto lbegin = reversed.begin();
  auto lend = reversed.end();
  std::reverse(lbegin, lend);

  // Statements may have a label(s), wrap the statement with its label(s)
  if (SageInterface::is_Fortran_language()) {
    SgScopeStatement *labelScope =
        SageInterface::getEnclosingFunctionDefinition(SB::topScopeStack(),
                                                      /*includingSelf*/ true);
    if (labelScope == nullptr) {
      labelScope = SB::topScopeStack();
    }
    ASSERT_not_null(labelScope);
    for (const auto &label : reversed) {
      const int labelValue = std::atoi(label.c_str());
      if (labelValue <= 0) {
        continue;
      }
      SageInterface::setFortranNumericLabel(
          stmt, labelValue, SgLabelSymbol::e_start_label_type, labelScope);
    }
    return stmt;
  }

  for (auto label : reversed) {
    // A label statement may already exist for this label, e.g., from a
    // placeholder created previously for an SgGotoStatement, for example,
    // check.
    SgLabelStatement *labelStmt{nullptr};
    if (labels_.find(label) != labels_.end()) {
      labelStmt = labels_[label];
    } else {
      labelStmt = SB::buildLabelStatement_nfi(label, stmt, SB::topScopeStack());
      labels_[label] = labelStmt;
    }

    if (labelStmt && labelStmt->get_statement() == nullptr) {
      // Found a placeholder label statement
      labelStmt->set_statement(stmt);
      stmt->set_parent(labelStmt);
    }

    if (SageInterface::is_Fortran_language()) {
      SgScopeStatement *labelScope =
          SageInterface::getEnclosingFunctionDefinition(SB::topScopeStack());
      if (labelScope == nullptr) {
        labelScope = SB::topScopeStack();
      }
      ASSERT_not_null(labelScope);
      SgLabelSymbol *labelSymbol =
          labelScope->lookup_label_symbol(labelStmt->get_label());
      if (labelSymbol == nullptr) {
        labelSymbol = new SgLabelSymbol(labelStmt);
        labelScope->insert_symbol(labelSymbol->get_name(), labelSymbol);
      }
      if (labelSymbol->get_fortran_statement() == nullptr) {
        labelSymbol->set_fortran_statement(stmt);
      } else if (labelSymbol->get_fortran_statement() != stmt) {
        std::cerr << "Duplicate Fortran label " << label << "\n";
        ROSE_ABORT();
      }
      const int labelValue = std::atoi(label.c_str());
      ROSE_ASSERT(labelValue > 0);
      const int numericValue = labelSymbol->get_numeric_label_value();
      if (numericValue <= 0) {
        labelSymbol->set_numeric_label_value(labelValue);
      } else if (numericValue != labelValue) {
        const std::string location = formatLocatedNode(stmt);
        std::cerr << "Mismatched Fortran label value for " << label
                  << " (symbol=" << numericValue << ", statement=" << labelValue
                  << ")";
        if (!location.empty()) {
          std::cerr << " at " << location;
        }
        std::cerr << "\n";
        ROSE_ABORT();
      }
    }

    stmt = labelStmt;
  }
  ASSERT_not_null(stmt);

  return stmt;
}

// Temporary wrappers for SageInterface functions (needed until ROSE builds with
// C++17)
//
namespace SageBuilderCpp17 {

// Types
//
SgType *buildBoolType() { return SageBuilder::buildBoolType(); }

SgType *buildIntType() { return SageBuilder::buildIntType(); }

SgType *buildFloatType() { return SageBuilder::buildFloatType(); }

SgType *buildCharType() { return SageBuilder::buildCharType(); }

SgType *buildDoubleType() { return SageBuilder::buildDoubleType(); }

SgType *buildComplexType(SgType *base_type) {
  return SageBuilder::buildComplexType(base_type);
}

SgType *buildBoolType(SgExpression *kind_expr) {
  return SageBuilder::buildBoolType(kind_expr);
}

SgType *buildIntType(SgExpression *kind_expr) {
  return SageBuilder::buildIntType(kind_expr);
}

SgType *buildFloatType(SgExpression *kind_expr) {
  return SageBuilder::buildFloatType(kind_expr);
}

SgType *buildStringType(SgExpression *stringLengthExpression) {
  return SageBuilder::buildStringType(stringLengthExpression);
}

SgType *buildArrayType(SgType *base_type,
                       std::list<SgExpression *> &explicit_shape_list) {
  SgExprListExp *dim_info = SageBuilder::buildExprListExp_nfi();

  for (SgExpression *expr : explicit_shape_list) {
    ASSERT_not_null(expr);
    SageInterface::appendExpression(dim_info, expr);
  }

  return SageBuilder::buildArrayType(base_type, dim_info);
}

// SgBasicBlock
//

SgBasicBlock *buildBasicBlock_nfi() {
  return SageBuilder::buildBasicBlock_nfi();
}

void pushScopeStack(SgBasicBlock *stmt) { SageBuilder::pushScopeStack(stmt); }

void popScopeStack() { SageBuilder::popScopeStack(); }

// Operators
//
SgExpression *buildAddOp_nfi(SgExpression *lhs, SgExpression *rhs) {
  return SageBuilder::buildAddOp_nfi(lhs, rhs);
}

SgExpression *buildAndOp_nfi(SgExpression *lhs, SgExpression *rhs) {
  return SageBuilder::buildAndOp_nfi(lhs, rhs);
}

SgExpression *buildDivideOp_nfi(SgExpression *lhs, SgExpression *rhs) {
  return SageBuilder::buildDivideOp_nfi(lhs, rhs);
}

SgExpression *buildEqualityOp_nfi(SgExpression *lhs, SgExpression *rhs) {
  return SageBuilder::buildEqualityOp_nfi(lhs, rhs);
}

SgExpression *buildGreaterThanOp_nfi(SgExpression *lhs, SgExpression *rhs) {
  return SageBuilder::buildGreaterThanOp_nfi(lhs, rhs);
}

SgExpression *buildGreaterOrEqualOp_nfi(SgExpression *lhs, SgExpression *rhs) {
  return SageBuilder::buildGreaterOrEqualOp_nfi(lhs, rhs);
}

SgExpression *buildMultiplyOp_nfi(SgExpression *lhs, SgExpression *rhs) {
  return SageBuilder::buildMultiplyOp_nfi(lhs, rhs);
}

SgExpression *buildLessThanOp_nfi(SgExpression *lhs, SgExpression *rhs) {
  return SageBuilder::buildLessThanOp_nfi(lhs, rhs);
}

SgExpression *buildLessOrEqualOp_nfi(SgExpression *lhs, SgExpression *rhs) {
  return SageBuilder::buildLessOrEqualOp_nfi(lhs, rhs);
}

SgExpression *buildNotEqualOp_nfi(SgExpression *lhs, SgExpression *rhs) {
  return SageBuilder::buildNotEqualOp_nfi(lhs, rhs);
}

SgExpression *buildOrOp_nfi(SgExpression *lhs, SgExpression *rhs) {
  return SageBuilder::buildOrOp_nfi(lhs, rhs);
}

SgExpression *buildMinusOp_nfi(SgExpression *i, bool is_prefix /* = true */) {
  SgUnaryOp::Sgop_mode mode_enum;

  if (is_prefix) {
    mode_enum = SgUnaryOp::Sgop_mode::prefix;
  } else {
    mode_enum = SgUnaryOp::Sgop_mode::postfix;
  }

  return SageBuilder::buildMinusOp_nfi(i, mode_enum);
}

SgExpression *buildSubtractOp_nfi(SgExpression *lhs, SgExpression *rhs) {
  return SageBuilder::buildSubtractOp_nfi(lhs, rhs);
}

// Expressions
//
SgExpression *buildConcatenationOp_nfi(SgExpression *lhs, SgExpression *rhs) {
  return SageBuilder::buildConcatenationOp_nfi(lhs, rhs);
}

SgExpression *buildExprListExp_nfi() {
  return SageBuilder::buildExprListExp_nfi();
}

SgExpression *buildBoolValExp_nfi(bool value) {
  return SageBuilder::buildBoolValExp_nfi(value);
}

SgExpression *buildIntVal_nfi(int value = 0) {
  return SageBuilder::buildIntVal_nfi(value);
}

SgExpression *buildStringVal_nfi(std::string value) {
  return SageBuilder::buildStringVal_nfi(value);
}

SgExpression *buildFloatVal_nfi(const std::string &str) {
  return SageBuilder::buildFloatVal_nfi(str);
}

SgExpression *buildComplexVal_nfi(SgExpression *real_value,
                                  SgExpression *imaginary_value,
                                  const std::string &str) {
  SgValueExp *real = isSgValueExp(real_value);
  SgValueExp *imaginary = isSgValueExp(imaginary_value);

  ASSERT_not_null(real);
  ASSERT_not_null(imaginary);

  return SageBuilder::buildComplexVal_nfi(real, imaginary, str);
}

SgExpression *buildVarRefExp_nfi(std::string &name, SgScopeStatement *scope,
                                 bool allow_implicit) {
  if (scope == nullptr) {
    scope = SageBuilder::topScopeStack();
  }
  ASSERT_not_null(scope);

  bool allow_implicit_decl = allow_implicit;
  if (SageInterface::is_Fortran_language() && allow_implicit_decl) {
    if (HasFortranImplicitNone(scope)) {
      allow_implicit_decl = false;
    }
  }

  auto fixup_existing_decl = [&](SgScopeStatement *decl_scope) -> bool {
    if (decl_scope == nullptr) {
      return false;
    }
    const bool case_insensitive =
        SageInterface::is_language_case_insensitive() ||
        decl_scope->isCaseInsensitive();
    SgInitializedName *existing =
        findInitializedNameInScope(decl_scope, name, case_insensitive);
    if (existing == nullptr) {
      return false;
    }
    SgVariableSymbol *sym =
        isSgVariableSymbol(existing->get_symbol_from_symbol_table());
    if (sym == nullptr) {
      sym = isSgVariableSymbol(existing->search_for_symbol_from_symbol_table());
    }
    if (sym == nullptr) {
      sym = new SgVariableSymbol(existing);
    }
    SgScopeStatement *sym_scope = sym->get_scope();
    if (sym_scope != nullptr && sym_scope != decl_scope) {
      sym_scope->remove_symbol(sym);
    }
    if (decl_scope->lookup_variable_symbol(existing->get_name()) == nullptr) {
      decl_scope->insert_symbol(existing->get_name(), sym);
    }
    if (existing->get_scope() != decl_scope) {
      existing->set_scope(decl_scope);
    }
    return true;
  };

  SgScopeStatement *implicit_scope = nullptr;
  if (SageInterface::is_Fortran_language()) {
    implicit_scope = FindFortranImplicitDeclScope(scope);
    if (implicit_scope == nullptr) {
      implicit_scope = scope;
    }
  }

  auto resolve_fortran_symbol = [&]() -> SgVariableSymbol * {
    SgVariableSymbol *sym =
        SageInterface::lookupVariableSymbolInParentScopes(name, scope);
    if (sym == nullptr && implicit_scope != nullptr) {
      sym = implicit_scope->lookup_variable_symbol(name);
    }
    return sym;
  };

  if (SageInterface::is_Fortran_language() &&
      SageInterface::lookupVariableSymbolInParentScopes(name, scope) ==
          nullptr) {
    fixup_existing_decl(implicit_scope);
    fixup_existing_decl(scope);
  }

  SgVariableSymbol *resolved_sym = nullptr;
  if (SageInterface::is_Fortran_language()) {
    resolved_sym = resolve_fortran_symbol();
    if (resolved_sym != nullptr) {
      SgVarRefExp *var_ref = SageBuilder::buildVarRefExp_nfi(resolved_sym);
      ASSERT_not_null(var_ref);
      SageInterface::setSourcePosition(var_ref);
      return var_ref;
    }
  }

  if (SageInterface::is_Fortran_language() && allow_implicit_decl) {
    if (SageInterface::lookupVariableSymbolInParentScopes(name, scope) ==
        nullptr) {
      SgType *implicit_type = SageBuilder::buildFortranImplicitType(name);
      SgVariableDeclaration *var_decl =
          SageBuilder::buildVariableDeclaration_nfi(name, implicit_type,
                                                    /*initializer*/ nullptr,
                                                    implicit_scope);
      ASSERT_not_null(var_decl);
      SageInterface::setSourcePosition(var_decl);
      var_decl->addNewAttribute(kFortranImplicitDeclAttr,
                                new FortranImplicitDeclAttribute());
      InsertFortranImplicitDeclaration(var_decl, implicit_scope);
      if (implicit_scope != nullptr) {
        resolved_sym = implicit_scope->lookup_variable_symbol(name);
      }
      if (resolved_sym != nullptr) {
        SgVarRefExp *var_ref = SageBuilder::buildVarRefExp_nfi(resolved_sym);
        ASSERT_not_null(var_ref);
        SageInterface::setSourcePosition(var_ref);
        return var_ref;
      }
    }
  }

  SgVarRefExp *var_ref = nullptr;
  if (SageInterface::is_Fortran_language() && !allow_implicit_decl &&
      SageInterface::lookupVariableSymbolInParentScopes(name, scope) ==
          nullptr) {
    SgGlobal *global_scope = SageInterface::getGlobalScope(scope);
    ASSERT_not_null(global_scope);
    var_ref = SageBuilder::buildDanglingVarRefExp(SgName(name), global_scope);
  } else {
    var_ref = SageBuilder::buildVarRefExp(name, scope);
  }
  ASSERT_not_null(var_ref);
  SageInterface::setSourcePosition(var_ref);

  return var_ref;
}

SgExpression *buildSubscriptExpression_nfi(SgExpression *lower_bound,
                                           SgExpression *upper_bound,
                                           SgExpression *stride) {
  return SageBuilder::buildSubscriptExpression_nfi(lower_bound, upper_bound,
                                                   stride);
}

SgExpression *buildPntrArrRefExp_nfi(SgExpression *lhs, SgExpression *rhs) {
  return SageBuilder::buildPntrArrRefExp_nfi(lhs, rhs);
}

SgExpression *buildAggregateInitializer_nfi(SgExprListExp *initializers,
                                            SgType *type) {
  return SageBuilder::buildAggregateInitializer_nfi(initializers, type);
}

SgExpression *buildAsteriskShapeExp_nfi() {
  SgAsteriskShapeExp *shape = new SgAsteriskShapeExp();
  ASSERT_not_null(shape);
  SageInterface::setSourcePosition(shape);

  return shape;
}

SgExpression *buildAssumedRankExp_nfi() {
  SgAssumedRankExp *shape = new SgAssumedRankExp();
  ASSERT_not_null(shape);
  SageInterface::setSourcePosition(shape);

  return shape;
}

SgExpression *buildNullExpression_nfi() {
  return SageBuilder::buildNullExpression_nfi();
}

SgExpression *buildFunctionCallExp(SgFunctionCallExp *func_call) {
  return func_call;
}

SgExprListExp *buildExprListExp_nfi(const std::list<SgExpression *> &list) {
  SgExprListExp *expr_list = SageBuilder::buildExprListExp_nfi();

  for (SgExpression *expr : list) {
    ASSERT_not_null(expr);
    SageInterface::appendExpression(expr_list, expr);
  }
  return expr_list;
}

SgCommonBlockObject *buildCommonBlockObject(std::string name,
                                            SgExprListExp *expr_list) {
  SgCommonBlockObject *common_block_object =
      SageBuilder::buildCommonBlockObject(name, expr_list);
  SageInterface::setSourcePosition(common_block_object);
  return common_block_object;
}

void set_false_body(SgIfStmt *&if_stmt, SgBasicBlock *false_body) {
  ASSERT_not_null(if_stmt);
  if_stmt->set_false_body(false_body);
}

void set_need_paren(SgExpression *&expr) {
  ASSERT_not_null(expr);
  expr->set_need_paren(true);
}

void fixUndeclaredResultName(const std::string &result_name,
                             SgScopeStatement *scope, SgType *result_type) {
  // This function should only be called if there is no symbol and there is a
  // result type
  SgVariableSymbol *symbol = nullptr;
  if (scope != nullptr) {
    symbol = scope->lookup_variable_symbol(result_name);
  }
  ASSERT_require(symbol == nullptr);
  ASSERT_not_null(result_type);

  const bool case_insensitive =
      SageInterface::is_language_case_insensitive() ||
      (scope != nullptr && scope->isCaseInsensitive());
  if (SgBasicBlock *block = isSgBasicBlock(scope)) {
    if (SgInitializedName *existing = findInitializedNameInStatements(
            block, result_name, case_insensitive)) {
      SgVariableSymbol *existing_sym =
          isSgVariableSymbol(existing->get_symbol_from_symbol_table());
      if (existing_sym == nullptr) {
        existing_sym =
            isSgVariableSymbol(existing->search_for_symbol_from_symbol_table());
      }
      if (existing_sym == nullptr) {
        existing_sym = new SgVariableSymbol(existing);
      }
      SgScopeStatement *sym_scope = existing_sym->get_scope();
      if (sym_scope != block) {
        if (sym_scope != nullptr) {
          sym_scope->remove_symbol(existing_sym);
        }
      }
      if (block->lookup_variable_symbol(existing->get_name()) == nullptr) {
        block->insert_symbol(existing->get_name(), existing_sym);
      }
      existing->set_scope(block);
      return;
    }

    SgVariableDeclaration *var_decl = SageBuilder::buildVariableDeclaration_nfi(
        result_name, result_type, /*initializer*/ nullptr, block);
    ASSERT_not_null(var_decl);
    SageInterface::setSourcePosition(var_decl);
    SageInterface::appendStatement(var_decl, block);
    return;
  }

  SgInitializedName *init_name = SageBuilder::buildInitializedName_nfi(
      result_name, result_type, /*initializer*/ nullptr);
  SageInterface::setSourcePosition(init_name);
  init_name->set_scope(scope);
  init_name->set_parent(scope);
  SgVariableSymbol *result_symbol = new SgVariableSymbol(init_name);
  ASSERT_not_null(result_symbol);
  scope->insert_symbol(result_name, result_symbol);
}

SgFunctionRefExp *buildIntrinsicFunctionRefExp_nfi(const std::string &name,
                                                   SgScopeStatement *scope) {
  SgFunctionRefExp *func_ref = nullptr;

  // assumes Fortran for now
  SgFunctionSymbol *symbol =
      SageInterface::lookupFunctionSymbolInParentScopes(name, scope);

  if (symbol) {
  } else {
    // Look for intrinsic name
    if (name == "num_images") {
      // TODO
      MLOG_WARN_CXX(MLOG_FRONTEND)
          << "need to build a function reference to num_images\n";
    }
  }

  return func_ref;
}

SgFunctionCallExp *buildIntrinsicFunctionCallExp_nfi(const std::string &name,
                                                     SgExprListExp *params,
                                                     SgScopeStatement *scope) {
  SgType *return_type = nullptr;
  SgFunctionCallExp *func_call = nullptr;

  if (!params) {
    params = SageBuilder::buildExprListExp_nfi();
  }
  if (!scope) {
    scope = SageBuilder::topScopeStack();
  }
  ASSERT_not_null(params);
  ASSERT_not_null(scope);

  // Create a return type based on the intrinsic name
  if (name == "num_images") {
    return_type = SageBuilder::buildIntType();
  } else {
    return_type = SageBuilder::buildVoidType();
  }

  if (return_type) {
    func_call = SageBuilder::buildFunctionCallExp(SgName(name), return_type,
                                                  params, scope);
    ASSERT_not_null(func_call);
    SageInterface::setSourcePosition(func_call);
  }

  return func_call;
}

} // namespace SageBuilderCpp17

} // namespace builder
} // namespace Rose
