#include "sage3basic.h"

#include "rose_config.h"

#include "ModuleBuilder.h"

#include "SageTreeBuilder.h"

#include <algorithm>
#include <charconv>
#include <filesystem>
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

SgProcedureHeaderStatement::fortran_procedure_source_form_enum
FortranProcedureHeaderSourceForm(const SourcePositions &sources) {
  const std::string &path = std::get<0>(sources).path;
  std::string extension = std::filesystem::path(path).extension().string();
  std::transform(extension.begin(), extension.end(), extension.begin(),
                 [](unsigned char ch) { return std::tolower(ch); });
  if (extension == ".mod" || extension == ".rmod" || extension == ".rcmp") {
    return SgProcedureHeaderStatement::
        e_fortran_procedure_source_form_compiler_module_header;
  }
  return SgProcedureHeaderStatement::e_fortran_procedure_source_form_header;
}

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
  std::set<PreprocessingInfo *> retained_infos;
  std::set<PreprocessingInfo *> deleted_infos;
  for (auto it = info_list->begin(); it != info_list->end();) {
    PreprocessingInfo *info = *it;
    if (info != nullptr && deleted_infos.count(info) > 0) {
      it = info_list->erase(it);
      continue;
    }
    if (!IsCommentInfo(info)) {
      if (info != nullptr) {
        retained_infos.insert(info);
      }
      ++it;
      continue;
    }
    const std::string key = BuildCommentKey(info);
    if (seen_keys.count(key) > 0) {
      it = info_list->erase(it);
      if (retained_infos.count(info) == 0 &&
          deleted_infos.insert(info).second) {
        delete info;
      }
      continue;
    }
    seen_keys.insert(key);
    retained_infos.insert(info);
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

  std::set<PreprocessingInfo *> deleted_infos;
  for (auto it = target_list->begin(); it != target_list->end();) {
    PreprocessingInfo *info = *it;
    if (info != nullptr && deleted_infos.count(info) > 0) {
      it = target_list->erase(it);
      continue;
    }
    if (!IsCommentInfo(info)) {
      ++it;
      continue;
    }
    const std::string key = BuildCommentKey(info);
    if (ref_keys.count(key) > 0) {
      it = target_list->erase(it);
      if (std::find(ref_list->begin(), ref_list->end(), info) ==
              ref_list->end() &&
          deleted_infos.insert(info).second) {
        delete info;
      }
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

void requireFreshUnclassifiedSource(SgLocatedNode *node, const char *producer) {
  ASSERT_not_null(node);
  ASSERT_not_null(producer);
  SgExpression *expression = isSgExpression(node);
  Sg_File_Info *operatorInfo =
      expression != nullptr ? expression->get_operatorPosition() : nullptr;
  if (node->get_startOfConstruct() != nullptr ||
      node->get_endOfConstruct() != nullptr || operatorInfo != nullptr) {
    std::cerr << "REX_FLANG_INVARIANT[fresh-source-producer]: " << producer
              << " produced a preclassified " << node->class_name()
              << " instead of one fresh _nfi node\n";
    ROSE_ABORT();
  }
}

void publishTransformationSourceOnce(SgLocatedNode *node,
                                     const char *producer) {
  requireFreshUnclassifiedSource(node, producer);
  SageInterface::setOneSourcePositionForTransformation(node);
  SgExpression *expression = isSgExpression(node);
  if (node->get_startOfConstruct() == nullptr ||
      node->get_endOfConstruct() == nullptr ||
      (expression != nullptr &&
       expression->get_operatorPosition() == nullptr)) {
    std::cerr << "REX_FLANG_INVARIANT[transformation-source-publication]: "
              << producer << " did not publish complete transformation "
              << "source ownership for " << node->class_name() << "\n";
    ROSE_ABORT();
  }
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

PreprocessingInfo::DirectiveType
ToSagePreprocessingDirectiveType(PreprocessingDirectiveKind kind) {
  switch (kind) {
  case PreprocessingDirectiveKind::include:
    return PreprocessingInfo::CpreprocessorIncludeDeclaration;
  case PreprocessingDirectiveKind::include_next:
    return PreprocessingInfo::CpreprocessorIncludeNextDeclaration;
  case PreprocessingDirectiveKind::define:
    return PreprocessingInfo::CpreprocessorDefineDeclaration;
  case PreprocessingDirectiveKind::undef:
    return PreprocessingInfo::CpreprocessorUndefDeclaration;
  case PreprocessingDirectiveKind::ifdef:
    return PreprocessingInfo::CpreprocessorIfdefDeclaration;
  case PreprocessingDirectiveKind::ifndef:
    return PreprocessingInfo::CpreprocessorIfndefDeclaration;
  case PreprocessingDirectiveKind::if_directive:
    return PreprocessingInfo::CpreprocessorIfDeclaration;
  case PreprocessingDirectiveKind::else_directive:
    return PreprocessingInfo::CpreprocessorElseDeclaration;
  case PreprocessingDirectiveKind::elif:
    return PreprocessingInfo::CpreprocessorElifDeclaration;
  case PreprocessingDirectiveKind::endif:
    return PreprocessingInfo::CpreprocessorEndifDeclaration;
  case PreprocessingDirectiveKind::line:
    return PreprocessingInfo::CpreprocessorLineDeclaration;
  case PreprocessingDirectiveKind::pragma:
    return PreprocessingInfo::CpreprocessorPragmaDeclaration;
  case PreprocessingDirectiveKind::error:
    return PreprocessingInfo::CpreprocessorErrorDeclaration;
  case PreprocessingDirectiveKind::warning:
    return PreprocessingInfo::CpreprocessorWarningDeclaration;
  case PreprocessingDirectiveKind::empty:
    return PreprocessingInfo::CpreprocessorEmptyDeclaration;
  case PreprocessingDirectiveKind::ident:
    return PreprocessingInfo::CpreprocessorIdentDeclaration;
  case PreprocessingDirectiveKind::compiler_generated_linemarker:
    return PreprocessingInfo::CpreprocessorCompilerGeneratedLinemarker;
  case PreprocessingDirectiveKind::skipped:
    return PreprocessingInfo::CSkippedToken;
  case PreprocessingDirectiveKind::none:
    break;
  }
  std::cerr << "REX_FLANG_INVARIANT[source-token-directive-kind]: "
               "preprocessing token has no typed directive kind\n";
  ROSE_ABORT();
}

void attachCommentFromToken(SgLocatedNode *node, const Token &token,
                            PreprocessingInfo::RelativePositionType position,
                            const SgSourceFile *source) {
  if (node == nullptr || source == nullptr) {
    std::cerr << "REX_FLANG_INVARIANT[source-token-context]: source token "
                 "has no AST attachment context\n";
    ROSE_ABORT();
  }

  PreprocessingInfo::RelativePositionType adjusted_position = position;
  if (position == PreprocessingInfo::after && isSgBasicBlock(node) != nullptr) {
    adjusted_position = PreprocessingInfo::inside;
  }

  PreprocessingInfo::DirectiveType style;
  std::string spelling;
  switch (token.getTokenType()) {
  case TokenKind::comment:
    if (token.getPreprocessingDirectiveKind() !=
        PreprocessingDirectiveKind::none) {
      std::cerr << "REX_FLANG_INVARIANT[source-token-directive-kind]: comment "
                   "token carries a preprocessing directive kind\n";
      ROSE_ABORT();
    }
    if (!token.hasExactSpelling()) {
      std::cerr << "REX_FLANG_INVARIANT[source-token-spelling]: comment token "
                   "has no exact Flang spelling\n";
      ROSE_ABORT();
    }
    style = GetFortranCommentStyle(source);
    spelling = token.getLexeme();
    break;
  case TokenKind::preprocessing:
    if (!token.hasExactSpelling()) {
      std::cerr << "REX_FLANG_INVARIANT[source-token-spelling]: "
                   "preprocessing token has no exact Flang spelling\n";
      ROSE_ABORT();
    }
    style =
        ToSagePreprocessingDirectiveType(token.getPreprocessingDirectiveKind());
    spelling = token.getLexeme();
    break;
  default:
    std::cerr << "REX_FLANG_INVARIANT[source-token-kind]: unsupported Flang "
                 "source token kind\n";
    ROSE_ABORT();
  }
  if (spelling.empty()) {
    std::cerr << "REX_FLANG_INVARIANT[source-token-spelling]: empty source "
                 "token spelling\n";
    ROSE_ABORT();
  }
  const std::string &filename = token.getPath();
  if (filename.empty()) {
    std::cerr << "REX_FLANG_INVARIANT[source-token-position]: source token "
                 "has no physical path\n";
    ROSE_ABORT();
  }
  int numberOfLines = token.getEndLine() - token.getStartLine() + 1;
  if (token.getStartLine() <= 0 || token.getStartCol() <= 0 ||
      token.getEndLine() < token.getStartLine() || numberOfLines < 1) {
    std::cerr << "REX_FLANG_INVARIANT[source-token-position]: source token "
                 "has an invalid exact range\n";
    ROSE_ABORT();
  }

  PreprocessingInfo *info = new PreprocessingInfo(
      style, spelling, filename, token.getStartLine(), token.getStartCol(),
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

void ValidateResolvedFortranLabelSymbols(SgScopeStatement *labelScope,
                                         const char *boundary) {
  ASSERT_not_null(labelScope);
  ASSERT_not_null(boundary);
  SgSymbolTable *symbolTable = labelScope->get_symbol_table();
  if (symbolTable == nullptr || symbolTable->get_parent() != labelScope) {
    std::cerr << "REX_FLANG_INVARIANT[label-symbol-table]: " << boundary
              << " has no exact label symbol table\n";
    ROSE_ABORT();
  }

  for (SgNode *node : symbolTable->get_symbols()) {
    SgLabelSymbol *labelSymbol = isSgLabelSymbol(node);
    if (labelSymbol == nullptr) {
      continue;
    }
    const int value = labelSymbol->get_numeric_label_value();
    SgStatement *target = labelSymbol->get_fortran_statement();
    if (labelSymbol->get_parent() != symbolTable ||
        !symbolTable->exists(labelSymbol) || value <= 0 || value > 99999 ||
        target == nullptr || isSgNullStatement(target) != nullptr) {
      std::cerr << "REX_FLANG_INVARIANT[unresolved-label]: " << boundary
                << " label " << value
                << " has no exact semantic statement target\n";
      ROSE_ABORT();
    }
  }
}

int ParseFortranNumericLabel(const std::string &label, const char *context) {
  ASSERT_not_null(context);
  int value = 0;
  const char *begin = label.data();
  const char *end = begin + label.size();
  const std::from_chars_result parsed = std::from_chars(begin, end, value);
  if (label.empty() || parsed.ec != std::errc() || parsed.ptr != end ||
      value <= 0 || value > 99999) {
    std::cerr << "REX_FLANG_INVARIANT[numeric-label]: " << context
              << " has invalid Fortran label '" << label << "'\n";
    ROSE_ABORT();
  }
  return value;
}

} // namespace

/// Initialize the global scope and push it onto the scope stack
///
SgGlobal *initialize_global_scope(SgSourceFile *file) {
  if (file == nullptr) {
    std::cerr << "REX_FLANG_INVARIANT[global-output-owner]: null source file\n";
    ROSE_ABORT();
  }

  // Set the default for source position generation to be consistent with other
  // languages (e.g. C/C++).
  SageBuilder::setSourcePositionClassificationMode(
      SageBuilder::e_sourcePositionFrontendConstruction);

  SgGlobal *globalScope = file->get_globalScope();
  ASSERT_not_null(globalScope);
  Sg_File_Info *sourceInfo = file->get_file_info();
  Sg_File_Info *globalStart = globalScope->get_startOfConstruct();
  Sg_File_Info *globalEnd = globalScope->get_endOfConstruct();
  if (file->get_globalScope() != globalScope ||
      globalScope->get_parent() != file || sourceInfo == nullptr ||
      globalStart == nullptr || globalEnd == nullptr ||
      sourceInfo->get_physical_file_id() < 0 ||
      globalStart->get_physical_file_id() !=
          sourceInfo->get_physical_file_id() ||
      globalEnd->get_physical_file_id() != sourceInfo->get_physical_file_id() ||
      sourceInfo->isShared() || globalStart->isShared() ||
      globalEnd->isShared() || globalStart->get_parent() != globalScope ||
      globalEnd->get_parent() != globalScope || globalStart->get_line() != 1 ||
      globalEnd->get_line() != 1 || !globalStart->isOutputInCodeGeneration() ||
      !globalEnd->isOutputInCodeGeneration() ||
      !globalScope->isOutputInCodeGeneration()) {
    std::cerr << "REX_FLANG_INVARIANT[global-output-owner]: source file and "
                 "global scope do not have one exact physical ownership "
                 "chain\n";
    ROSE_ABORT();
  }

  // Fortran is case insensitive
  globalScope->setCaseInsensitive(true);

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

SgVariableDeclaration *BuildUnclassifiedSourceVariableDeclaration(
    const std::string &name, SgType *type, SgExpression *init_expr,
    SgScopeStatement *scope) {
  ASSERT_not_null(type);
  ASSERT_not_null(scope);

  SgInitializer *initializer = nullptr;
  if (init_expr != nullptr) {
    initializer = SageBuilder::buildAssignInitializer_nfi(init_expr, type);
  }

  SgVariableDeclaration *declaration =
      SageBuilder::buildVariableDeclaration_nfi(SgName(name), type, initializer,
                                                scope);
  ASSERT_not_null(declaration);
  if (declaration->get_definingDeclaration() == nullptr) {
    declaration->set_definingDeclaration(declaration);
  }
  if (declaration->get_definingDeclaration() == declaration &&
      declaration->get_firstNondefiningDeclaration() == declaration) {
    declaration->set_firstNondefiningDeclaration(nullptr);
  }

  SgInitializedName *initialized = declaration->get_decl_item(SgName(name));
  SgDeclarationStatement *semanticOwner =
      initialized != nullptr ? initialized->get_declptr() : nullptr;
  SgVariableDefinition *variableDefinition =
      initialized != nullptr ? initialized->get_definition() : nullptr;
  const bool functionValued = isSgFunctionType(type) != nullptr;
  SgVariableSymbol *symbol =
      initialized != nullptr
          ? isSgVariableSymbol(scope->find_symbol_from_declaration(initialized))
          : nullptr;
  if (initialized == nullptr || semanticOwner != declaration ||
      (functionValued ? variableDefinition != nullptr
                      : variableDefinition == nullptr ||
                            variableDefinition->get_vardefn() != initialized ||
                            variableDefinition->get_parent() != initialized) ||
      declaration->get_parent() != nullptr || symbol == nullptr ||
      symbol->get_declaration() != initialized ||
      symbol->get_scope() != scope ||
      initialized->get_parent() != declaration ||
      initialized->get_scope() != scope) {
    std::cerr << "REX_FLANG_INVARIANT[variable-source-construction]: '" << name
              << "' was not assembled as one exact unclassified detached "
                 "source declaration\n";
    ROSE_ABORT();
  }
  return declaration;
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
      auto commentPosition = PreprocessingInfo::before;
      if (token->getStartLine() == pos.getStartLine()) {
        commentPosition = PreprocessingInfo::after;
        // check for a source token following a variable initializer
        if (SgVariableDeclaration *varDecl = isSgVariableDeclaration(stmt)) {
          for (SgInitializedName *name : varDecl->get_variables()) {
            if (SgInitializer *init = name->get_initializer()) {
              PosInfo initPos{init};
              if (initPos.getEndCol() > token->getStartCol()) {
                commentNode = init;
                break;
              }
            }
          }
        }
      }
      if (TRACE_ATTACH_COMMENT) {
        MLOG_TRACE_CXX(MLOG_FRONTEND)
            << "attach source token for: " << commentNode->class_name() << ": "
            << *token << ": " << commentPosition;
      }
      attachCommentFromToken(commentNode, *token, commentPosition, source_);
      tokens_->consumeNextToken();
    }
  } else if (auto expr = isSgEnumVal(node)) {
    const Token *token = nullptr;
    auto commentPosition = PreprocessingInfo::before;
    // try only attaching comments from same line (what about multi-line
    // comments)
    while ((token = tokens_->getNextToken()) &&
           token->getStartLine() == pos.getStartLine()) {
      if (token->getEndCol() == pos.getStartCol()) {
        commentPosition = PreprocessingInfo::after;
      }
      if (TRACE_ATTACH_COMMENT) {
        MLOG_TRACE_CXX(MLOG_FRONTEND)
            << "attach source token for: " << expr->class_name() << ": "
            << *token;
      }
      attachCommentFromToken(expr, *token, commentPosition, source_);
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

void SageTreeBuilder::setExactSourcePosition(SgLocatedNode *node,
                                             const SourcePosition &start,
                                             const SourcePosition &end,
                                             bool attach_comments,
                                             ExactSourceInput input) {
  ASSERT_not_null(node);

  auto positionIsAbsent = [](const SourcePosition &position) {
    return position.path.empty() && position.line == 0 && position.column == 0;
  };
  auto positionIsPartial = [&](const SourcePosition &position) {
    if (positionIsAbsent(position)) {
      return false;
    }
    return position.path.empty() || position.line == 0 || position.column == 0;
  };

  if (positionIsAbsent(start) || positionIsAbsent(end)) {
    std::cerr << "REX_FLANG_INVARIANT[source-position-missing]: "
              << node->class_name()
              << " requires both exact source-range endpoints\n";
    ROSE_ABORT();
  }
  if (positionIsPartial(start) || positionIsPartial(end)) {
    std::cerr << "REX_FLANG_INVARIANT[source-position-partial]: "
              << node->class_name()
              << " has a partially initialized source-range endpoint\n";
    ROSE_ABORT();
  }
  if (start.line < 1 || end.line < 1 || start.column < 1 || end.column < 1) {
    std::cerr << "REX_FLANG_INVARIANT[source-position-invalid]: "
              << node->class_name()
              << " has a non-positive source coordinate\n";
    ROSE_ABORT();
  }

  const std::string normalizedStartPath =
      StringUtility::getAbsolutePathFromRelativePath(start.path);
  const std::string normalizedEndPath =
      StringUtility::getAbsolutePathFromRelativePath(end.path);
  if (normalizedStartPath.empty() || normalizedEndPath.empty() ||
      normalizedStartPath != normalizedEndPath) {
    std::cerr << "REX_FLANG_INVARIANT[source-position-file]: "
              << node->class_name()
              << " source range does not belong to one exact physical file\n";
    ROSE_ABORT();
  }

  const bool ordered = end.line > start.line ||
                       (end.line == start.line && end.column > start.column);
  if (!ordered) {
    std::cerr << "REX_FLANG_INVARIANT[source-position-order]: "
              << node->class_name()
              << " source range is not a non-empty ordered half-open "
                 "interval\n";
    ROSE_ABORT();
  }

  Sg_File_Info *existingStart = node->get_startOfConstruct();
  Sg_File_Info *existingEnd = node->get_endOfConstruct();
  SgExpression *expression = isSgExpression(node);
  Sg_File_Info *existingOperator =
      expression != nullptr ? expression->get_operatorPosition() : nullptr;

  const unsigned int pendingClassification =
      Sg_File_Info::e_output_in_code_generation |
      Sg_File_Info::e_source_position_unavailable_in_frontend;
  auto isPendingExactSource = [&](Sg_File_Info *info) {
    return info != nullptr && info->get_parent() == node && !info->isShared() &&
           info->get_classificationBitField() == pendingClassification &&
           info->get_file_id() == Sg_File_Info::NULL_FILE_ID &&
           info->get_physical_file_id() == Sg_File_Info::NULL_FILE_ID;
  };
  const bool hasPendingExactSource =
      existingStart != nullptr && existingEnd != nullptr &&
      existingStart != existingEnd && isPendingExactSource(existingStart) &&
      isPendingExactSource(existingEnd) &&
      ((expression == nullptr && existingOperator == nullptr &&
        node->get_file_info() == existingStart) ||
       (expression != nullptr && existingOperator != nullptr &&
        existingOperator != existingStart && existingOperator != existingEnd &&
        isPendingExactSource(existingOperator) &&
        node->get_file_info() == existingOperator));
  const bool isFreshSource = existingStart == nullptr &&
                             existingEnd == nullptr &&
                             existingOperator == nullptr;

  auto isExactSemanticPosition = [node](Sg_File_Info *info) {
    return info != nullptr && info->get_parent() == node && !info->isShared() &&
           info->isCompilerGenerated() && info->isFrontendSpecific() &&
           !info->isTransformation() &&
           !info->isSourcePositionUnavailableInFrontend() &&
           info->isOutputInCodeGeneration() &&
           info->get_file_id() == Sg_File_Info::COMPILER_GENERATED_FILE_ID &&
           info->get_physical_file_id() ==
               Sg_File_Info::COMPILER_GENERATED_FILE_ID;
  };
  const bool hasExactSemanticSource =
      existingStart != nullptr && existingEnd != nullptr &&
      existingStart != existingEnd && isExactSemanticPosition(existingStart) &&
      isExactSemanticPosition(existingEnd) &&
      ((expression == nullptr && existingOperator == nullptr &&
        node->get_file_info() == existingStart) ||
       (expression != nullptr && existingOperator != nullptr &&
        existingOperator != existingStart && existingOperator != existingEnd &&
        isExactSemanticPosition(existingOperator) &&
        node->get_file_info() == existingOperator));
  SgVariableDeclaration *pendingFortranVariable = isSgVariableDeclaration(node);
  for (SgNode *owner = node->get_parent();
       pendingFortranVariable == nullptr && owner != nullptr;
       owner = owner->get_parent()) {
    pendingFortranVariable = isSgVariableDeclaration(owner);
    if (pendingFortranVariable == nullptr &&
        isSgInitializedName(owner) == nullptr &&
        isSgVariableDefinition(owner) == nullptr &&
        isSgInitializer(owner) == nullptr) {
      break;
    }
  }
  auto hasExactPendingVariableSemanticScope = [](SgVariableDeclaration *decl) {
    if (decl == nullptr) {
      return false;
    }
    const SgInitializedNamePtrList &names = decl->get_variables();
    if (names.empty()) {
      return false;
    }
    SgScopeStatement *scope = names.front()->get_scope();
    return scope != nullptr &&
           std::all_of(names.begin(), names.end(),
                       [&](SgInitializedName *name) {
                         return name != nullptr && name->get_parent() == decl &&
                                name->get_scope() == scope;
                       });
  };
  const bool isExactPendingFortranVariable =
      input == ExactSourceInput::source_spelled &&
      pendingFortranVariable != nullptr &&
      pendingFortranVariable->get_parent() == nullptr &&
      hasExactPendingVariableSemanticScope(pendingFortranVariable) &&
      (pendingFortranVariable->get_fortran_declaration_origin() ==
           SgVariableDeclaration::e_fortran_source_declaration ||
       pendingFortranVariable->get_fortran_declaration_origin() ==
           SgVariableDeclaration::e_fortran_pending_source_declaration) &&
      hasExactSemanticSource;
  const bool isExactGeneratedSemanticAnchor =
      input == ExactSourceInput::generated_semantic_anchor &&
      hasExactSemanticSource;
  const bool canReplaceSemanticSource =
      isExactPendingFortranVariable || isExactGeneratedSemanticAnchor;
  if (!isFreshSource && !hasPendingExactSource && !canReplaceSemanticSource) {
    std::cerr << "REX_FLANG_INVARIANT[source-position-state]: "
              << node->class_name()
              << " must be fresh or carry one exact typed pending frontend "
                 "source transaction before publication (input="
              << (input == ExactSourceInput::source_spelled
                      ? "source-spelled"
                      : "generated-semantic-anchor")
              << " start=" << (existingStart == nullptr ? "null" : "classified")
              << " end=" << (existingEnd == nullptr ? "null" : "classified")
              << " operator="
              << (expression == nullptr
                      ? "not-applicable"
                      : (existingOperator == nullptr ? "null" : "classified"))
              << ")\n";
    ROSE_ABORT();
  }

  Sg_File_Info *startInfo =
      new Sg_File_Info(normalizedStartPath, start.line, start.column);
  Sg_File_Info *endInfo =
      new Sg_File_Info(normalizedEndPath, end.line, end.column - 1);
  ASSERT_not_null(startInfo);
  ASSERT_not_null(endInfo);
  node->set_startOfConstruct(startInfo);
  node->set_endOfConstruct(endInfo);
  startInfo->set_parent(node);
  endInfo->set_parent(node);
  Sg_File_Info *operatorInfo = nullptr;
  if (expression != nullptr) {
    operatorInfo = new Sg_File_Info(*startInfo);
    ASSERT_not_null(operatorInfo);
    expression->set_operatorPosition(operatorInfo);
    operatorInfo->set_parent(expression);
  }

  if (hasPendingExactSource || canReplaceSemanticSource) {
    for (Sg_File_Info *info : {existingStart, existingEnd, existingOperator}) {
      if (info != nullptr) {
        info->set_parent(nullptr);
        delete info;
      }
    }
  }

  if (node->get_file_info() !=
          (expression != nullptr ? operatorInfo : startInfo) ||
      node->get_startOfConstruct() != startInfo ||
      node->get_endOfConstruct() != endInfo ||
      startInfo->get_parent() != node || endInfo->get_parent() != node ||
      (operatorInfo != nullptr && operatorInfo->get_parent() != node)) {
    std::cerr << "REX_FLANG_INVARIANT[source-position-publication]: "
              << node->class_name()
              << " did not acquire one exact owned source range\n";
    ROSE_ABORT();
  }

  const int nodePhysicalFileId = startInfo->get_physical_file_id();
  if (nodePhysicalFileId < 0 ||
      endInfo->get_physical_file_id() != nodePhysicalFileId ||
      startInfo->isShared() || endInfo->isShared() ||
      (operatorInfo != nullptr &&
       (operatorInfo->get_physical_file_id() != nodePhysicalFileId ||
        operatorInfo->isShared()))) {
    std::cerr << "REX_FLANG_INVARIANT[physical-source-owner]: "
              << node->class_name()
              << " has missing or ambiguous physical source ownership\n";
    ROSE_ABORT();
  }

  if (language_ == LanguageEnum::Fortran && source_ != nullptr) {
    const Sg_File_Info *sourceInfo = source_->get_file_info();
    if (sourceInfo == nullptr || sourceInfo->get_physical_file_id() < 0 ||
        sourceInfo->isShared()) {
      std::cerr << "REX_FLANG_INVARIANT[physical-output-owner]: active "
                   "Fortran source file has missing or ambiguous physical "
                   "output ownership\n";
      ROSE_ABORT();
    }
    if (isPrimaryFortranSourceRange(source_, start, end) &&
        nodePhysicalFileId != sourceInfo->get_physical_file_id()) {
      std::cerr << "REX_FLANG_INVARIANT[physical-output-owner]: "
                << node->class_name()
                << " belongs to the primary Fortran source path but not its "
                   "exact physical output file\n";
      ROSE_ABORT();
    }
  }

  if (language_ == LanguageEnum::Fortran && source_ != nullptr &&
      input == ExactSourceInput::source_spelled &&
      isPrimaryFortranSourceRange(source_, start, end)) {
    for (Sg_File_Info *info :
         {node->get_file_info(), node->get_startOfConstruct(),
          node->get_endOfConstruct(), operatorInfo}) {
      if (info != nullptr) {
        info->setOutputInCodeGeneration();
      }
    }
  }

  // Source-backed nodes own comment attachment. Generated semantic nodes may
  // share the exact source anchor, but must never consume source tokens.
  if (attach_comments && isSgFunctionDefinition(node) == nullptr) {
    if (language_ == LanguageEnum::Fortran) {
      if (SgBasicBlock *block = isSgBasicBlock(node)) {
        if (block->get_statements().empty()) {
          return;
        }
      }
    }
    PosInfo pinfo{start.line, start.column, end.line, end.column};
    attachComments(node, pinfo);
  }
}

void SageTreeBuilder::setSourcePosition(SgLocatedNode *node,
                                        const SourcePosition &start,
                                        const SourcePosition &end) {
  setExactSourcePosition(node, start, end, /*attach_comments=*/true,
                         ExactSourceInput::source_spelled);
  SgAttributeSpecificationStatement *attribute =
      isSgAttributeSpecificationStatement(node);
  SgExprListExp *parameters =
      attribute != nullptr ? attribute->get_parameter_list() : nullptr;
  if (parameters == nullptr) {
    return;
  }
  if (parameters->get_parent() != attribute) {
    std::cerr << "REX_FLANG_INVARIANT[attribute-parameter-source]: "
                 "attribute parameter list has no exact structural owner\n";
    ROSE_ABORT();
  }
  Sg_File_Info *parameterStart = parameters->get_startOfConstruct();
  Sg_File_Info *parameterEnd = parameters->get_endOfConstruct();
  Sg_File_Info *parameterOperator = parameters->get_operatorPosition();
  const bool fresh = parameterStart == nullptr && parameterEnd == nullptr &&
                     parameterOperator == nullptr;
  const bool complete = parameterStart != nullptr && parameterEnd != nullptr &&
                        parameterOperator != nullptr;
  if (fresh && parameters->get_expressions().empty()) {
    SageBuilder::initializeSemanticExpressionSourceProvenance(parameters);
  } else if (!complete) {
    std::cerr << "REX_FLANG_INVARIANT[attribute-parameter-source]: "
                 "nonempty attribute parameter list has no exact source "
                 "interval\n";
    ROSE_ABORT();
  }
}

void SageTreeBuilder::setSourcePosition(SgPragma *pragma,
                                        const SourcePosition &start,
                                        const SourcePosition &end) {
  ASSERT_not_null(pragma);
  if (start.path.empty() || end.path.empty() || start.line < 1 ||
      end.line < 1 || start.column < 1 || end.column < 1 ||
      (end.line < start.line ||
       (end.line == start.line && end.column <= start.column))) {
    std::cerr << "REX_FLANG_INVARIANT[pragma-source-position]: SgPragma has "
                 "an incomplete, invalid, or unordered source interval\n";
    ROSE_ABORT();
  }
  const std::string startPath =
      StringUtility::getAbsolutePathFromRelativePath(start.path);
  const std::string endPath =
      StringUtility::getAbsolutePathFromRelativePath(end.path);
  if (startPath.empty() || startPath != endPath ||
      pragma->get_startOfConstruct() != nullptr ||
      pragma->get_endOfConstruct() != nullptr) {
    std::cerr << "REX_FLANG_INVARIANT[pragma-source-position]: SgPragma "
                 "requires one fresh exact physical source interval\n";
    ROSE_ABORT();
  }

  Sg_File_Info *startInfo =
      new Sg_File_Info(startPath, start.line, start.column);
  Sg_File_Info *endInfo = new Sg_File_Info(endPath, end.line, end.column - 1);
  ASSERT_not_null(startInfo);
  ASSERT_not_null(endInfo);
  pragma->set_startOfConstruct(startInfo);
  pragma->set_endOfConstruct(endInfo);
  startInfo->set_parent(pragma);
  endInfo->set_parent(pragma);

  const int physicalFileId = startInfo->get_physical_file_id();
  if (pragma->get_file_info() != startInfo || physicalFileId < 0 ||
      endInfo->get_physical_file_id() != physicalFileId ||
      startInfo->isShared() || endInfo->isShared()) {
    std::cerr << "REX_FLANG_INVARIANT[pragma-source-position]: SgPragma did "
                 "not acquire one exact owned physical source interval\n";
    ROSE_ABORT();
  }

  if (language_ == LanguageEnum::Fortran && source_ != nullptr &&
      isPrimaryFortranSourceRange(source_, start, end)) {
    const Sg_File_Info *sourceInfo = source_->get_file_info();
    if (sourceInfo == nullptr || sourceInfo->get_physical_file_id() < 0 ||
        sourceInfo->isShared() ||
        physicalFileId != sourceInfo->get_physical_file_id()) {
      std::cerr << "REX_FLANG_INVARIANT[pragma-source-position]: primary "
                   "Fortran pragma is not owned by its exact physical output "
                   "file\n";
      ROSE_ABORT();
    }
    startInfo->setOutputInCodeGeneration();
    endInfo->setOutputInCodeGeneration();
  }
}

void SageTreeBuilder::setGeneratedSourcePosition(
    SgLocatedNode *node, const SourcePosition &start, const SourcePosition &end,
    GeneratedSourceAnchorKind kind) {
  ASSERT_not_null(node);

  bool nodeKindMatches = false;
  switch (kind) {
  case GeneratedSourceAnchorKind::program_canonical_declaration:
    nodeKindMatches = isSgProgramHeaderStatement(node) != nullptr;
    break;
  case GeneratedSourceAnchorKind::procedure_canonical_declaration:
    nodeKindMatches = isSgProcedureHeaderStatement(node) != nullptr;
    break;
  case GeneratedSourceAnchorKind::enum_canonical_declaration:
    nodeKindMatches = isSgEnumDeclaration(node) != nullptr;
    break;
  case GeneratedSourceAnchorKind::use_associated_type_declaration:
  case GeneratedSourceAnchorKind::use_associated_type_canonical_declaration:
    nodeKindMatches = isSgDerivedTypeStatement(node) != nullptr;
    break;
  case GeneratedSourceAnchorKind::use_associated_type_definition:
    nodeKindMatches = isSgClassDefinition(node) != nullptr;
    break;
  case GeneratedSourceAnchorKind::use_associated_object_declaration:
    nodeKindMatches = isSgVariableDeclaration(node) != nullptr;
    break;
  case GeneratedSourceAnchorKind::use_associated_object_name:
    nodeKindMatches = isSgInitializedName(node) != nullptr;
    break;
  case GeneratedSourceAnchorKind::use_associated_variable_definition:
    nodeKindMatches = isSgVariableDefinition(node) != nullptr;
    break;
  case GeneratedSourceAnchorKind::use_associated_procedure_entity_declaration:
    nodeKindMatches = isSgVariableDeclaration(node) != nullptr;
    break;
  case GeneratedSourceAnchorKind::use_associated_procedure_entity_name:
    nodeKindMatches = isSgInitializedName(node) != nullptr;
    break;
  case GeneratedSourceAnchorKind::syntactic_absence_expression: {
    SgNullExpression *absence = isSgNullExpression(node);
    nodeKindMatches = absence != nullptr &&
                      absence->get_role() ==
                          SgNullExpression::e_null_expression_syntactic_absence;
    break;
  }
  }
  if (!nodeKindMatches) {
    std::cerr << "REX_FLANG_INVARIANT[generated-source-kind]: "
              << node->class_name()
              << " is incompatible with its generated semantic source "
                 "anchor kind\n";
    ROSE_ABORT();
  }

  setExactSourcePosition(node, start, end, /*attach_comments=*/false,
                         ExactSourceInput::generated_semantic_anchor);
  for (Sg_File_Info *info :
       {node->get_file_info(), node->get_startOfConstruct(),
        node->get_endOfConstruct()}) {
    if (info == nullptr || info->get_parent() != node) {
      std::cerr << "REX_FLANG_INVARIANT[generated-source-owner]: "
                << node->class_name()
                << " did not publish complete owned source anchors\n";
      ROSE_ABORT();
    }
    info->setCompilerGenerated();
  }
  node->setCompilerGenerated();
  for (Sg_File_Info *info :
       {node->get_file_info(), node->get_startOfConstruct(),
        node->get_endOfConstruct()}) {
    info->setOutputInCodeGeneration();
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

  if (anchor.path.empty() || anchor.line < 1 || anchor.column < 1) {
    std::cerr << "REX_FLANG_INVARIANT[source-anchor-invalid]: "
              << node->class_name()
              << " cannot share an incomplete physical source anchor\n";
    ROSE_ABORT();
  }

  const std::string normalized_anchor_path =
      StringUtility::getAbsolutePathFromRelativePath(anchor.path);
  if (normalized_anchor_path.empty()) {
    std::cerr << "REX_FLANG_INVARIANT[source-anchor-file]: "
              << node->class_name()
              << " has no exact normalized physical source identity\n";
    ROSE_ABORT();
  }

  Sg_File_Info *existing_start = node->get_startOfConstruct();
  Sg_File_Info *existing_end = node->get_endOfConstruct();
  SgExpression *expression = isSgExpression(node);
  Sg_File_Info *existing_operator =
      expression != nullptr ? expression->get_operatorPosition() : nullptr;
  if (existing_start != nullptr || existing_end != nullptr ||
      existing_operator != nullptr) {
    std::cerr << "REX_FLANG_INVARIANT[source-anchor-state]: "
              << node->class_name()
              << " must be a fresh unclassified _nfi node before exact "
                 "source-anchor publication\n";
    ROSE_ABORT();
  }

  Sg_File_Info *start_info =
      new Sg_File_Info(normalized_anchor_path, anchor.line, anchor.column);
  Sg_File_Info *end_info =
      new Sg_File_Info(normalized_anchor_path, anchor.line, anchor.column);
  ASSERT_not_null(start_info);
  ASSERT_not_null(end_info);

  node->set_startOfConstruct(start_info);
  node->set_endOfConstruct(end_info);
  start_info->set_parent(node);
  end_info->set_parent(node);
  Sg_File_Info *operator_info = nullptr;
  if (expression != nullptr) {
    operator_info = new Sg_File_Info(*start_info);
    ASSERT_not_null(operator_info);
    expression->set_operatorPosition(operator_info);
    operator_info->set_parent(expression);
  }
  if (node->get_file_info() !=
          (expression != nullptr ? operator_info : start_info) ||
      node->get_startOfConstruct() != start_info ||
      node->get_endOfConstruct() != end_info ||
      start_info->get_parent() != node || end_info->get_parent() != node ||
      start_info->get_raw_filename() != normalized_anchor_path ||
      end_info->get_raw_filename() != normalized_anchor_path ||
      start_info->get_physical_filename() != normalized_anchor_path ||
      end_info->get_physical_filename() != normalized_anchor_path ||
      (operator_info != nullptr &&
       (operator_info->get_parent() != node ||
        operator_info->get_raw_filename() != normalized_anchor_path ||
        operator_info->get_physical_filename() != normalized_anchor_path))) {
    std::cerr << "REX_FLANG_INVARIANT[source-anchor-owner]: "
              << node->class_name()
              << " did not retain its exact shared physical source anchor\n";
    ROSE_ABORT();
  }
}

void initializeFortranParameterSourceLocations(
    SgFunctionDeclaration *function_decl, const SourcePosition &anchor) {
  ASSERT_not_null(function_decl);

  auto initialize_param_list = [&](SgFunctionParameterList *param_list,
                                   SgFunctionDeclaration *owner) {
    if (param_list == nullptr) {
      std::cerr << "REX_FLANG_INVARIANT[parameter-list]: function '"
                << owner->get_name().str() << "' has no parameter list\n";
      ROSE_ABORT();
    }

    const bool has_file =
        hasConcreteSourceLocation(param_list->get_file_info());
    const bool has_start =
        hasConcreteSourceLocation(param_list->get_startOfConstruct());
    const bool has_end =
        hasConcreteSourceLocation(param_list->get_endOfConstruct());
    if (has_file != has_start || has_file != has_end) {
      std::cerr << "REX_FLANG_INVARIANT[parameter-location]: function '"
                << owner->get_name().str()
                << "' has partially initialized parameter-list location\n";
      ROSE_ABORT();
    }
    if (!has_file) {
      setLocatedNodeSourceAnchor(param_list, anchor);
    }
    if (param_list->get_parent() != owner) {
      std::cerr << "REX_FLANG_INVARIANT[parameter-owner]: function '"
                << owner->get_name().str()
                << "' does not directly own its parameter list\n";
      ROSE_ABORT();
    }

    for (SgInitializedName *arg : param_list->get_args()) {
      if (arg == nullptr) {
        std::cerr << "REX_FLANG_INVARIANT[parameter]: function '"
                  << owner->get_name().str()
                  << "' has a null parameter entry\n";
        ROSE_ABORT();
      }

      const bool arg_has_file = hasConcreteSourceLocation(arg->get_file_info());
      const bool arg_has_start =
          hasConcreteSourceLocation(arg->get_startOfConstruct());
      const bool arg_has_end =
          hasConcreteSourceLocation(arg->get_endOfConstruct());
      if (arg_has_file != arg_has_start || arg_has_file != arg_has_end) {
        std::cerr << "REX_FLANG_INVARIANT[parameter-location]: parameter '"
                  << arg->get_name().str() << "' of function '"
                  << owner->get_name().str()
                  << "' has partially initialized source metadata\n";
        ROSE_ABORT();
      }
      if (!arg_has_file) {
        setLocatedNodeSourceAnchor(arg, anchor);
      }

      if (arg->get_parent() != param_list || arg->get_declptr() != owner) {
        std::cerr << "REX_FLANG_INVARIANT[parameter-owner]: parameter '"
                  << arg->get_name().str() << "' of function '"
                  << owner->get_name().str()
                  << "' lacks exact list and declaration ownership\n";
        ROSE_ABORT();
      }
    }
  };

  SgFunctionParameterList *def_params = function_decl->get_parameterList();
  initialize_param_list(def_params, function_decl);
}

void requireFortranParameterSemanticSurface(const SgInitializedName *parameter,
                                            const char *context) {
  ASSERT_not_null(parameter);
  ASSERT_not_null(context);
  if (parameter->get_type() == nullptr ||
      parameter->get_fortran_source_type() != nullptr ||
      parameter->get_fortran_source_derived_type_symbol() != nullptr ||
      parameter->get_fortran_type_spec() !=
          SgInitializedName::e_fortran_type_spec_default ||
      !parameter->get_fortran_procedure_interface().is_null() ||
      parameter->get_fortran_separate_shape_declaration() != nullptr ||
      parameter->get_fortran_separate_pointer_declaration() != nullptr ||
      parameter->get_cray_pointer_pointee() != nullptr ||
      parameter->get_fortran_cray_pointer_pointee_shape() != nullptr ||
      parameter->get_shapeDeferred()) {
    std::cerr << "REX_FLANG_INVARIANT[procedure-parameter-semantic-surface]: "
              << context << " parameter '" << parameter->get_name().str()
              << "' carries declaration-statement source syntax\n";
    ROSE_ABORT();
  }
}

SgFunctionParameterList *cloneFortranCanonicalParameterList(
    const SgFunctionParameterList *definitionParameters) {
  ASSERT_not_null(definitionParameters);

  SgFunctionParameterList *canonicalParameters =
      SageBuilder::buildFunctionParameterList_nfi();
  ASSERT_not_null(canonicalParameters);

  for (SgInitializedName *definitionArgument :
       definitionParameters->get_args()) {
    if (definitionArgument == nullptr ||
        definitionArgument->get_type() == nullptr) {
      std::cerr << "REX_FLANG_INVARIANT[procedure-canonical-parameters]: "
                   "defining procedure has a null or untyped dummy argument\n";
      ROSE_ABORT();
    }
    if (definitionArgument->get_initializer() != nullptr) {
      std::cerr << "REX_FLANG_INVARIANT[procedure-canonical-parameters]: "
                   "Fortran dummy argument unexpectedly has an initializer\n";
      ROSE_ABORT();
    }

    SgInitializedName *canonicalArgument =
        SageBuilder::buildInitializedName_nfi(definitionArgument->get_name(),
                                              definitionArgument->get_type(),
                                              /*initializer=*/nullptr);
    ASSERT_not_null(canonicalArgument);
    requireFortranParameterSemanticSurface(definitionArgument,
                                           "defining procedure");
    requireFortranParameterSemanticSurface(canonicalArgument,
                                           "canonical declaration");
    canonicalParameters->append_arg(canonicalArgument);
    if (canonicalArgument->get_parent() != canonicalParameters ||
        canonicalArgument == definitionArgument) {
      std::cerr << "REX_FLANG_INVARIANT[procedure-canonical-parameters]: "
                   "canonical dummy argument has no independent list owner\n";
      ROSE_ABORT();
    }
  }

  return canonicalParameters;
}

void requireFortranProcedureCanonicalSource(
    SgProcedureHeaderStatement *canonical, SgScopeStatement *scope,
    const SourcePosition &start, const SourcePosition &end) {
  ASSERT_not_null(canonical);
  ASSERT_not_null(scope);

  const std::string startPath =
      StringUtility::getAbsolutePathFromRelativePath(start.path);
  const std::string endPath =
      StringUtility::getAbsolutePathFromRelativePath(end.path);
  Sg_File_Info *startInfo = canonical->get_startOfConstruct();
  Sg_File_Info *endInfo = canonical->get_endOfConstruct();
  SgAuxiliaryDeclarationList *auxiliary =
      isSgAuxiliaryDeclarationList(canonical->get_parent());
  const int expectedEndColumn = end.column - 1;
  if (startPath.empty() || startPath != endPath || start.line < 1 ||
      start.column < 1 || end.line < 1 || end.column < 1 ||
      startInfo == nullptr || endInfo == nullptr || startInfo == endInfo ||
      canonical->get_file_info() != startInfo ||
      startInfo->get_parent() != canonical ||
      endInfo->get_parent() != canonical ||
      startInfo->get_raw_filename() != startPath ||
      endInfo->get_raw_filename() != endPath ||
      startInfo->get_physical_filename() != startPath ||
      endInfo->get_physical_filename() != endPath ||
      startInfo->get_raw_line() != start.line ||
      startInfo->get_raw_col() != start.column ||
      endInfo->get_raw_line() != end.line ||
      endInfo->get_raw_col() != expectedEndColumn ||
      !startInfo->isCompilerGenerated() || !endInfo->isCompilerGenerated() ||
      startInfo->isTransformation() || endInfo->isTransformation() ||
      startInfo->isShared() || endInfo->isShared() ||
      startInfo->get_physical_file_id() < 0 ||
      endInfo->get_physical_file_id() != startInfo->get_physical_file_id() ||
      auxiliary == nullptr || auxiliary->get_parent() != scope ||
      scope->get_auxiliary_declarations() != auxiliary ||
      canonical->get_scope() != scope ||
      std::count(auxiliary->get_declarations().begin(),
                 auxiliary->get_declarations().end(), canonical) != 1) {
    std::cerr
        << "REX_FLANG_INVARIANT[procedure-canonical-source-owner]: procedure='"
        << canonical->get_name().str()
        << "' has no exact generated source range and auxiliary owner\n";
    ROSE_ABORT();
  }

  SgFunctionParameterList *parameters = canonical->get_parameterList();
  if (parameters == nullptr || parameters->get_parent() != canonical) {
    std::cerr
        << "REX_FLANG_INVARIANT[procedure-canonical-source-owner]: procedure='"
        << canonical->get_name().str()
        << "' has no directly owned canonical parameter list\n";
    ROSE_ABORT();
  }
  auto requireGeneratedAnchor = [&](SgLocatedNode *node) {
    ASSERT_not_null(node);
    Sg_File_Info *nodeStart = node->get_startOfConstruct();
    Sg_File_Info *nodeEnd = node->get_endOfConstruct();
    if (node->get_file_info() != nodeStart || nodeStart == nullptr ||
        nodeEnd == nullptr || nodeStart == nodeEnd ||
        nodeStart->get_parent() != node || nodeEnd->get_parent() != node ||
        nodeStart->get_raw_line() != start.line ||
        nodeStart->get_raw_col() != start.column ||
        nodeEnd->get_raw_line() != start.line ||
        nodeEnd->get_raw_col() != start.column ||
        !nodeStart->isCompilerGenerated() || !nodeEnd->isCompilerGenerated() ||
        nodeStart->isTransformation() || nodeEnd->isTransformation() ||
        nodeStart->isShared() || nodeEnd->isShared() ||
        nodeStart->get_physical_file_id() !=
            startInfo->get_physical_file_id() ||
        nodeEnd->get_physical_file_id() != startInfo->get_physical_file_id()) {
      std::cerr << "REX_FLANG_INVARIANT[procedure-canonical-source-owner]: "
                << node->class_name()
                << " has no exact generated canonical source anchor\n";
      ROSE_ABORT();
    }
  };
  requireGeneratedAnchor(parameters);
  for (SgInitializedName *argument : parameters->get_args()) {
    if (argument == nullptr || argument->get_parent() != parameters) {
      std::cerr << "REX_FLANG_INVARIANT[procedure-canonical-source-owner]: "
                   "procedure='"
                << canonical->get_name().str()
                << "' has a malformed canonical argument owner\n";
      ROSE_ABORT();
    }
    requireGeneratedAnchor(argument);
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
        std::set<PreprocessingInfo *> retained_infos;
        std::set<PreprocessingInfo *> deleted_infos;
        if (AttachedPreprocessingInfoType *first_info =
                first_stmt->getAttachedPreprocessingInfo()) {
          for (PreprocessingInfo *info : *first_info) {
            if (!is_comment_info(info)) {
              continue;
            }
            seen_keys.insert(BuildCommentKey(info));
            retained_infos.insert(info);
          }
        }
        AttachedPreprocessingInfoType to_move;
        AttachedPreprocessingInfoType to_delete;
        for (auto it = info_list->begin(); it != info_list->end();) {
          PreprocessingInfo *info = *it;
          if (info != nullptr && deleted_infos.count(info) > 0) {
            ++it;
            continue;
          }
          if (info != nullptr && is_comment_info(info) &&
              (first_line <= 0 || info->getLineNumber() <= first_line)) {
            const std::string key = BuildCommentKey(info);
            if (seen_keys.count(key) > 0) {
              if (retained_infos.count(info) == 0 &&
                  deleted_infos.insert(info).second) {
                to_delete.push_back(info);
              }
              ++it;
              continue;
            }
            seen_keys.insert(key);
            retained_infos.insert(info);
            to_move.push_back(info);
            ++it;
            continue;
          }
          ++it;
        }
        for (PreprocessingInfo *info : to_delete) {
          delete scope->detachPreprocessingInfo(info);
        }
        for (PreprocessingInfo *info : to_move) {
          scope->detachPreprocessingInfo(info);
          info->setRelativePosition(PreprocessingInfo::before);
        }
        PreprocessingInfo *prev = nullptr;
        for (PreprocessingInfo *info : to_move) {
          if (prev == nullptr) {
            first_stmt->addToAttachedPreprocessingInfo(
                info, PreprocessingInfo::before);
          } else {
            first_stmt->insertToAttachedPreprocessingInfo(info, prev);
          }
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

  if (name.has_value() && name->empty()) {
    std::cerr << "Error: explicit Fortran PROGRAM statement has an empty name"
              << std::endl;
    ROSE_ABORT();
  }
  SgName program_name(name.value_or(std::string{}));

  SgFunctionParameterList *param_list =
      SageBuilder::buildFunctionParameterList_nfi();
  SgFunctionType *function_type =
      SageBuilder::buildFunctionType(SageBuilder::buildVoidType(), param_list);

  SgProgramHeaderStatement *canonical_program_decl =
      new SgProgramHeaderStatement(program_name, function_type,
                                   /*function_def*/ nullptr);
  ASSERT_not_null(canonical_program_decl);
  canonical_program_decl->set_program_statement_kind(
      name.has_value()
          ? SgProgramHeaderStatement::e_explicit_program_statement
          : SgProgramHeaderStatement::e_implicit_program_statement);
  canonical_program_decl->set_scope(scope);
  canonical_program_decl->set_firstNondefiningDeclaration(
      canonical_program_decl);
  canonical_program_decl->set_definingDeclaration(nullptr);

  program_decl = new SgProgramHeaderStatement(program_name, function_type,
                                              /*function_def*/ nullptr);
  ASSERT_not_null(program_decl);
  SageInterface::setParameterList(program_decl, param_list);
  program_decl->set_program_statement_kind(
      name.has_value()
          ? SgProgramHeaderStatement::e_explicit_program_statement
          : SgProgramHeaderStatement::e_implicit_program_statement);

  if (!name.has_value()) {
    const SourcePosition &program_start = std::get<0>(sources);
    if (program_start.line <= 0 || program_start.column <= 0) {
      std::cerr << "REX_FLANG_INVARIANT[implicit-program-symbol-key]: "
                   "implicit PROGRAM has no exact producer source anchor\n";
      ROSE_ABORT();
    }
    const SgName symbol_key("__rex_internal_implicit_program_" +
                            std::to_string(program_start.line) + "_" +
                            std::to_string(program_start.column));
    canonical_program_decl
        ->initialize_fortran_anonymous_program_unit_symbol_key(symbol_key);
    program_decl->initialize_fortran_anonymous_program_unit_symbol_key(
        symbol_key);
  }

  // A source program unit has no source-level prototype, but the Sage function
  // model still requires a distinct canonical declaration for its symbol and
  // declaration chain. Construct that semantic declaration here and give it
  // explicit auxiliary ownership; never make the defining declaration
  // masquerade as its own prototype.
  canonical_program_decl->set_definingDeclaration(program_decl);
  program_decl->set_definingDeclaration(program_decl);
  program_decl->set_firstNondefiningDeclaration(canonical_program_decl);

  program_decl->set_scope(scope);

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
  setGeneratedSourcePosition(
      canonical_program_decl, std::get<0>(sources), std::get<2>(sources),
      GeneratedSourceAnchorKind::program_canonical_declaration);
  setSourcePosition(program_def, std::get<1>(sources), std::get<2>(sources));
  setSourcePosition(program_body, std::get<1>(sources), std::get<2>(sources));
  initializeFortranParameterSourceLocations(program_decl, ps);
  SgFunctionParameterList *canonical_parameters =
      canonical_program_decl->get_parameterList();
  ASSERT_not_null(canonical_parameters);
  setLocatedNodeSourceAnchor(canonical_parameters, ps);
  canonical_parameters->setCompilerGenerated();
  for (Sg_File_Info *info : {canonical_parameters->get_file_info(),
                             canonical_parameters->get_startOfConstruct(),
                             canonical_parameters->get_endOfConstruct()}) {
    if (info == nullptr || info->get_parent() != canonical_parameters) {
      std::cerr << "REX_FLANG_INVARIANT[program-canonical-parameters]: "
                   "canonical PROGRAM parameter list has no owned source "
                   "anchor\n";
      ROSE_ABORT();
    }
    info->setCompilerGenerated();
    info->setOutputInCodeGeneration();
  }
  SageBuilder::attachAuxiliaryDeclaration(scope, canonical_program_decl);
  SgAuxiliaryDeclarationList *canonical_owner =
      isSgAuxiliaryDeclarationList(canonical_program_decl->get_parent());
  if (canonical_owner == nullptr || canonical_owner->get_parent() != scope ||
      scope->get_auxiliary_declarations() != canonical_owner ||
      std::count(canonical_owner->get_declarations().begin(),
                 canonical_owner->get_declarations().end(),
                 canonical_program_decl) != 1) {
    std::cerr << "REX_FLANG_INVARIANT[program-canonical-owner]: PROGRAM '"
              << program_decl->get_name().str()
              << "' canonical declaration has no exact auxiliary owner\n";
    ROSE_ABORT();
  }

  SgName program_symbol_key =
      SageInterface::getFortranProgramUnitSymbolTableKey(program_decl);
  if (scope->symbol_exists(program_symbol_key)) {
    std::cerr << "REX_FLANG_INVARIANT[program-symbol-collision]: PROGRAM '"
              << program_decl->get_name().str()
              << "' collides with an existing global symbol\n";
    ROSE_ABORT();
  }
  SgFunctionSymbol *program_symbol =
      new SgFunctionSymbol(canonical_program_decl);
  scope->insert_symbol(program_symbol_key, program_symbol);
  if (program_symbol->get_declaration() != canonical_program_decl ||
      canonical_program_decl->get_symbol_from_symbol_table() !=
          program_symbol) {
    std::cerr << "REX_FLANG_INVARIANT[program-symbol-chain]: PROGRAM '"
              << program_decl->get_name().str()
              << "' symbol does not own its canonical declaration\n";
    ROSE_ABORT();
  }

  // appendStatement validates a function declaration against the symbol table
  // while publishing its lexical ownership.  The canonical declaration is the
  // symbol basis, so publish that exact semantic identity before exposing the
  // defining PROGRAM declaration to the source statement list.
  SageInterface::appendStatement(program_decl, scope);
  const SgStatementPtrList statements = scope->generateStatementList();
  if (program_decl->get_parent() != scope ||
      program_decl->get_scope() != scope ||
      std::count(statements.begin(), statements.end(), program_decl) != 1) {
    std::cerr << "REX_FLANG_INVARIANT[program-source-owner]: PROGRAM '"
              << program_decl->get_name().str()
              << "' has no exact lexical owner at construction\n";
    ROSE_ABORT();
  }

  // set labels
  if (SageInterface::is_Fortran_language() && labels.size() == 1) {
    SageInterface::setFortranNumericLabel(
        program_decl, ParseFortranNumericLabel(labels.front(), "PROGRAM"),
        SgLabelSymbol::e_start_label_type, /*label_scope=*/program_def);
  }

  ASSERT_require(program_body == SageBuilder::topScopeStack());
  ASSERT_require(program_decl->get_firstNondefiningDeclaration() ==
                 canonical_program_decl);
  ASSERT_require(canonical_program_decl != program_decl);
  ASSERT_require(canonical_program_decl->get_firstNondefiningDeclaration() ==
                 canonical_program_decl);
  ASSERT_require(canonical_program_decl->get_definingDeclaration() ==
                 program_decl);
  ASSERT_require(program_decl->get_definingDeclaration() == program_decl);
  ASSERT_require(canonical_program_decl->get_type() ==
                 program_decl->get_type());
  ASSERT_require(canonical_program_decl->get_parameterList() !=
                 program_decl->get_parameterList());
}

void SageTreeBuilder::Leave(SgProgramHeaderStatement *program_decl) {
  // On exit, this function will have checked that the program declaration is
  // properly connected, cleaned up the scope stack, resolved symbols, and
  // inserted the declaration into its scope.

  MLOG_TRACE_CXX(MLOG_FRONTEND)
      << "SageTreeBuilder::Leave(SgProgramHeaderStatement*) \n";

  ASSERT_not_null(program_decl);
  ValidateResolvedFortranLabelSymbols(program_decl->get_definition(),
                                      "PROGRAM");

  popScopeStack(/*attach_comments*/ true); // program body
  popScopeStack(/*attach_comments*/ true); // program definition

  auto scope = SageBuilder::topScopeStack();

  // The program declaration must go into the global scope
  SgGlobal *global_scope = isSgGlobal(scope);
  ASSERT_not_null(global_scope);

  // The symbol and both physical owners were finalized before body traversal.
  SgName program_symbol_key =
      SageInterface::getFortranProgramUnitSymbolTableKey(program_decl);

  SgProgramHeaderStatement *canonical_program_decl = isSgProgramHeaderStatement(
      program_decl->get_firstNondefiningDeclaration());
  SgAuxiliaryDeclarationList *canonical_owner =
      canonical_program_decl != nullptr
          ? isSgAuxiliaryDeclarationList(canonical_program_decl->get_parent())
          : nullptr;
  if (canonical_program_decl == nullptr ||
      canonical_program_decl == program_decl ||
      canonical_program_decl->get_firstNondefiningDeclaration() !=
          canonical_program_decl ||
      canonical_program_decl->get_definingDeclaration() != program_decl ||
      program_decl->get_definingDeclaration() != program_decl ||
      canonical_program_decl->get_scope() != global_scope ||
      program_decl->get_scope() != global_scope || canonical_owner == nullptr ||
      canonical_owner->get_parent() != global_scope ||
      global_scope->get_auxiliary_declarations() != canonical_owner ||
      std::count(canonical_owner->get_declarations().begin(),
                 canonical_owner->get_declarations().end(),
                 canonical_program_decl) != 1) {
    std::cerr << "REX_FLANG_INVARIANT[program-declaration-chain]: PROGRAM '"
              << program_decl->get_name().str()
              << "' has no distinct canonical and defining declaration chain\n";
    ROSE_ABORT();
  }

  SgFunctionSymbol *symbol =
      global_scope->lookup_function_symbol(program_symbol_key);
  const SgStatementPtrList statements = global_scope->generateStatementList();
  if (symbol == nullptr ||
      symbol->get_declaration() != canonical_program_decl ||
      canonical_program_decl->get_symbol_from_symbol_table() != symbol ||
      program_decl->get_parent() != global_scope ||
      std::count(statements.begin(), statements.end(), program_decl) != 1) {
    std::cerr << "REX_FLANG_INVARIANT[program-symbol-chain]: PROGRAM '"
              << program_decl->get_name().str()
              << "' lost its exact construction-time symbol or source owner\n";
    ROSE_ABORT();
  }

  // Attach any remaining comments
  scope = program_decl->get_definition()->get_body();
  attachComments(scope, /*at_end*/ true);

  DedupAttachedPreprocessingInfo(program_decl);
  if ((language_ == LanguageEnum::Fortran ||
       SageInterface::is_Fortran_language()) &&
      program_decl != nullptr && program_decl->get_parameterList() != nullptr) {
    RemoveDuplicateComments(program_decl->get_parameterList(), program_decl);
  }
}

// Fortran has an end statement which may have an optional name and label
void SageTreeBuilder::setFortranEndProgramStmt(
    SgProgramHeaderStatement *program_decl,
    const std::optional<std::string> &name,
    const std::optional<std::string> &label) {
  ASSERT_not_null(program_decl);

  SgProgramHeaderStatement *canonical_program_decl = isSgProgramHeaderStatement(
      program_decl->get_firstNondefiningDeclaration());
  if (canonical_program_decl == nullptr ||
      canonical_program_decl == program_decl ||
      canonical_program_decl->get_firstNondefiningDeclaration() !=
          canonical_program_decl ||
      canonical_program_decl->get_definingDeclaration() != program_decl ||
      program_decl->get_definingDeclaration() != program_decl ||
      canonical_program_decl->get_program_statement_kind() !=
          program_decl->get_program_statement_kind() ||
      canonical_program_decl->get_name() != program_decl->get_name()) {
    std::cerr << "REX_FLANG_INVARIANT[program-end-chain]: PROGRAM header has "
                 "no exact canonical declaration family\n";
    ROSE_ABORT();
  }

  if (program_decl->get_named_in_end_statement() ||
      !program_decl->get_end_statement_name().getString().empty() ||
      canonical_program_decl->get_named_in_end_statement() ||
      !canonical_program_decl->get_end_statement_name().getString().empty()) {
    std::cerr << "REX_FLANG_INVARIANT[program-end-name]: PROGRAM header "
                 "already has END PROGRAM name metadata\n";
    ROSE_ABORT();
  }

  SgFunctionDefinition *program_def = program_decl->get_definition();
  ASSERT_not_null(program_def);

  if (name) {
    if (name->empty()) {
      std::cerr << "REX_FLANG_INVARIANT[program-end-name]: named END PROGRAM "
                   "has an empty source name\n";
      ROSE_ABORT();
    }
    if (program_decl->get_program_statement_kind() !=
        SgProgramHeaderStatement::e_explicit_program_statement) {
      std::cerr << "REX_FLANG_INVARIANT[program-end-name]: implicit PROGRAM "
                   "has a named END PROGRAM statement\n";
      ROSE_ABORT();
    }
    const SgName endStatementName(*name);
    if (!namesMatch(program_decl->get_name(), endStatementName,
                    /*caseInsensitive=*/true)) {
      std::cerr << "REX_FLANG_INVARIANT[program-end-name]: END PROGRAM name '"
                << endStatementName << "' does not match PROGRAM name '"
                << program_decl->get_name() << "'\n";
      ROSE_ABORT();
    }
    program_decl->set_end_statement_name(endStatementName);
    program_decl->set_named_in_end_statement(true);
    canonical_program_decl->set_end_statement_name(endStatementName);
    canonical_program_decl->set_named_in_end_statement(true);
  }

  if (label) {
    SageInterface::setFortranNumericLabel(
        program_decl, ParseFortranNumericLabel(*label, "END PROGRAM"),
        SgLabelSymbol::e_end_label_type, /*label_scope=*/program_def);
  }

  const bool hasEndStatementName =
      !program_decl->get_end_statement_name().getString().empty();
  if (program_decl->get_named_in_end_statement() != hasEndStatementName ||
      canonical_program_decl->get_named_in_end_statement() !=
          hasEndStatementName ||
      canonical_program_decl->get_end_statement_name() !=
          program_decl->get_end_statement_name()) {
    std::cerr << "REX_FLANG_INVARIANT[program-end-name]: END PROGRAM name "
                 "metadata is inconsistent\n";
    ROSE_ABORT();
  }
}

void SageTreeBuilder::setFortranProcedureDeclarationSourcePosition(
    SgProcedureHeaderStatement *declaration, const SourcePosition &start,
    const SourcePosition &end) {
  ASSERT_not_null(declaration);
  if (declaration->get_definition() != nullptr ||
      declaration->get_parameterList() == nullptr ||
      declaration->get_parameterList()->get_parent() != declaration) {
    std::cerr << "REX_FLANG_INVARIANT[procedure-source-declaration]: "
                 "Fortran procedure declaration has a definition or no "
                 "directly owned parameter list\n";
    ROSE_ABORT();
  }
  setSourcePosition(declaration, start, end);
  initializeFortranParameterSourceLocations(declaration, start);
}

void SageTreeBuilder::attachFortranProcedureCanonical(
    SgProcedureHeaderStatement *canonical, SgScopeStatement *scope,
    const SourcePosition &start, const SourcePosition &end) {
  ASSERT_not_null(canonical);
  ASSERT_not_null(scope);

  SgProcedureHeaderStatement *first = isSgProcedureHeaderStatement(
      canonical->get_firstNondefiningDeclaration());
  SgProcedureHeaderStatement *definition =
      isSgProcedureHeaderStatement(canonical->get_definingDeclaration());
  if (first != canonical || canonical->get_definition() != nullptr ||
      canonical->get_scope() != scope ||
      (definition != nullptr &&
       (definition == canonical ||
        definition->get_firstNondefiningDeclaration() != canonical ||
        definition->get_definingDeclaration() != definition ||
        definition->get_scope() != scope))) {
    std::cerr << "REX_FLANG_INVARIANT[procedure-canonical-chain]: procedure='"
              << canonical->get_name().str()
              << "' has no exact canonical declaration family\n";
    ROSE_ABORT();
  }

  SgFunctionParameterList *canonicalParameters = canonical->get_parameterList();
  if (canonicalParameters == nullptr ||
      canonicalParameters->get_parent() != canonical) {
    std::cerr << "REX_FLANG_INVARIANT[procedure-canonical-parameters]: "
                 "procedure='"
              << canonical->get_name().str()
              << "' does not directly own its canonical parameter list\n";
    ROSE_ABORT();
  }
  if (definition != nullptr) {
    SgFunctionParameterList *definitionParameters =
        definition->get_parameterList();
    if (definitionParameters == nullptr ||
        definitionParameters == canonicalParameters ||
        definitionParameters->get_parent() != definition ||
        definitionParameters->get_args().size() !=
            canonicalParameters->get_args().size()) {
      std::cerr << "REX_FLANG_INVARIANT[procedure-canonical-parameters]: "
                   "procedure='"
                << canonical->get_name().str()
                << "' has shared or structurally inconsistent declaration "
                   "parameter lists\n";
      ROSE_ABORT();
    }
    for (std::size_t index = 0; index < canonicalParameters->get_args().size();
         ++index) {
      SgInitializedName *canonicalArgument =
          canonicalParameters->get_args()[index];
      SgInitializedName *definitionArgument =
          definitionParameters->get_args()[index];
      if (canonicalArgument == nullptr || definitionArgument == nullptr ||
          canonicalArgument == definitionArgument ||
          canonicalArgument->get_parent() != canonicalParameters ||
          definitionArgument->get_parent() != definitionParameters ||
          canonicalArgument->get_name() != definitionArgument->get_name() ||
          canonicalArgument->get_type() != definitionArgument->get_type()) {
        std::cerr << "REX_FLANG_INVARIANT[procedure-canonical-parameters]: "
                     "dummy argument "
                  << index << " of procedure='" << canonical->get_name().str()
                  << "' lacks independent exact declaration ownership\n";
        ROSE_ABORT();
      }
    }
  }

  SgNode *parent = canonical->get_parent();
  if (SgInterfaceBody *interfaceBody = isSgInterfaceBody(parent)) {
    if (interfaceBody->get_functionDeclaration() != canonical ||
        canonical->get_file_info() == nullptr ||
        canonical->get_file_info()->isCompilerGenerated()) {
      std::cerr << "REX_FLANG_INVARIANT[procedure-source-owner]: procedure='"
                << canonical->get_name().str()
                << "' has a malformed INTERFACE-body owner\n";
      ROSE_ABORT();
    }
    return;
  }
  if (SgStatementFunctionStatement *statementFunction =
          isSgStatementFunctionStatement(parent)) {
    if (statementFunction->get_function() != canonical) {
      std::cerr << "REX_FLANG_INVARIANT[procedure-source-owner]: procedure='"
                << canonical->get_name().str()
                << "' has a malformed statement-function owner\n";
      ROSE_ABORT();
    }
    return;
  }
  if (SgScopeStatement *lexicalOwner = isSgScopeStatement(parent)) {
    if (lexicalOwner != scope) {
      std::cerr << "REX_FLANG_INVARIANT[procedure-source-owner]: procedure='"
                << canonical->get_name().str()
                << "' has a lexical parent different from its scope\n";
      ROSE_ABORT();
    }
    if (scope->statementExistsInScope(canonical)) {
      return;
    }
  } else if (parent != nullptr &&
             isSgAuxiliaryDeclarationList(parent) == nullptr) {
    std::cerr << "REX_FLANG_INVARIANT[procedure-canonical-owner]: procedure='"
              << canonical->get_name().str()
              << "' has unsupported hidden owner " << parent->class_name()
              << "\n";
    ROSE_ABORT();
  }

  auto validateAuxiliaryOwner = [&]() {
    SgAuxiliaryDeclarationList *auxiliary =
        isSgAuxiliaryDeclarationList(canonical->get_parent());
    return auxiliary != nullptr && auxiliary->get_parent() == scope &&
           scope->get_auxiliary_declarations() == auxiliary &&
           std::count(auxiliary->get_declarations().begin(),
                      auxiliary->get_declarations().end(), canonical) == 1;
  };

  if (!validateAuxiliaryOwner()) {
    std::cerr << "REX_FLANG_INVARIANT[procedure-canonical-owner]: procedure='"
              << canonical->get_name().str()
              << "' has no exact auxiliary owner\n";
    ROSE_ABORT();
  }

  setGeneratedSourcePosition(
      canonical, start, end,
      GeneratedSourceAnchorKind::procedure_canonical_declaration);

  auto classifyGeneratedPointNode = [&](SgLocatedNode *node) {
    ASSERT_not_null(node);
    setLocatedNodeSourceAnchor(node, start);
    for (Sg_File_Info *info :
         {node->get_file_info(), node->get_startOfConstruct(),
          node->get_endOfConstruct()}) {
      if (info == nullptr || info->get_parent() != node) {
        std::cerr << "REX_FLANG_INVARIANT[procedure-canonical-source-owner]: "
                  << node->class_name()
                  << " has no exact owned source anchor\n";
        ROSE_ABORT();
      }
      info->setCompilerGenerated();
    }
    node->setCompilerGenerated();
    for (Sg_File_Info *info :
         {node->get_file_info(), node->get_startOfConstruct(),
          node->get_endOfConstruct()}) {
      info->setOutputInCodeGeneration();
    }
  };

  classifyGeneratedPointNode(canonicalParameters);
  for (SgInitializedName *argument : canonicalParameters->get_args()) {
    classifyGeneratedPointNode(argument);
  }
  for (Sg_File_Info *info :
       {canonical->get_file_info(), canonical->get_startOfConstruct(),
        canonical->get_endOfConstruct()}) {
    if (info == nullptr || info->get_parent() != canonical ||
        !info->isCompilerGenerated()) {
      std::cerr
          << "REX_FLANG_INVARIANT[procedure-canonical-source-owner]: "
          << canonical->get_name().str()
          << " canonical declaration has no exact generated source anchor\n";
      ROSE_ABORT();
    }
  }
  requireFortranProcedureCanonicalSource(canonical, scope, start, end);
}

void SageTreeBuilder::Enter(SgFunctionParameterList *&param_list,
                            SgScopeStatement *&param_scope,
                            const std::string &function_name,
                            SgType *function_type, bool is_defining_decl,
                            const SourcePositions *defining_sources) {
  MLOG_TRACE_CXX(MLOG_FRONTEND)
      << "SageTreeBuilder::Enter(SgFunctionParameterList*) \n";

  param_list = SageBuilder::buildFunctionParameterList_nfi();
  param_scope = nullptr;

  if (function_type != nullptr) {
    std::cerr << "REX_FLANG_INVARIANT[procedure-construction]: function '"
              << function_name
              << "' requested the removed implicit result-symbol staging "
                 "path\n";
    ROSE_ABORT();
  }

  SgScopeStatement *outer_scope = SageBuilder::topScopeStack();
  ASSERT_not_null(outer_scope);
  if (is_defining_decl) {
    if (defining_sources == nullptr || function_name.empty()) {
      std::cerr << "REX_FLANG_INVARIANT[procedure-construction]: defining "
                   "procedure '"
                << function_name
                << "' has no exact name and source-classified construction "
                   "request\n";
      ROSE_ABORT();
    }
    SgBasicBlock *body = new SgBasicBlock();
    ASSERT_not_null(body);
    SgFunctionDefinition *definition = new SgFunctionDefinition(
        static_cast<SgFunctionDeclaration *>(nullptr), body);
    ASSERT_not_null(definition);
    body->set_parent(definition);
    if (definition->get_parent() != nullptr ||
        definition->get_construction_physical_output_owner() != nullptr ||
        !definition->get_fortran_construction_name().is_null()) {
      std::cerr << "REX_FLANG_INVARIANT[procedure-construction]: defining "
                   "procedure '"
                << function_name
                << "' did not create one fresh detached definition "
                   "transaction\n";
      ROSE_ABORT();
    }
    definition->set_construction_physical_output_owner(outer_scope);
    definition->set_fortran_construction_name(SgName(function_name));
    const SourcePosition &body_begin = std::get<1>(*defining_sources);
    const SourcePosition &procedure_end = std::get<2>(*defining_sources);
    setSourcePosition(definition, body_begin, procedure_end);
    setSourcePosition(body, body_begin, procedure_end);
    param_scope = body;
  } else {
    if (defining_sources != nullptr) {
      std::cerr << "REX_FLANG_INVARIANT[procedure-construction]: "
                   "nondefining procedure '"
                << function_name
                << "' received a defining-body construction request\n";
      ROSE_ABORT();
    }
    param_scope = new SgFunctionParameterScope();
    SageInterface::setSemanticOnlyFrontendSourcePosition(param_scope);
    SgScopeStatement *physical_output_owner = outer_scope;
    if (SgFunctionParameterScope *pending_outer_scope =
            isSgFunctionParameterScope(outer_scope)) {
      if (pending_outer_scope->get_parent() != nullptr ||
          pending_outer_scope->get_construction_physical_output_owner() ==
              nullptr ||
          pending_outer_scope->get_construction_semantic_scope() == nullptr) {
        std::cerr << "REX_FLANG_INVARIANT[procedure-construction]: nested "
                     "nondefining procedure '"
                  << function_name
                  << "' has no exact outer parameter-scope construction "
                     "transaction\n";
        ROSE_ABORT();
      }
      physical_output_owner =
          pending_outer_scope->get_construction_physical_output_owner();
    }
    SageInterface::beginDetachedFunctionParameterScopeConstruction(
        isSgFunctionParameterScope(param_scope), physical_output_owner,
        outer_scope);
  }

  ASSERT_not_null(param_scope);

  if (language_ == LanguageEnum::Fortran ||
      SageInterface::is_language_case_insensitive() ||
      outer_scope->isCaseInsensitive()) {
    param_scope->setCaseInsensitive(true);
    if (SgFunctionDefinition *definition =
            isSgFunctionDefinition(param_scope->get_parent())) {
      definition->setCaseInsensitive(true);
    }
  }

  if (SgFunctionDefinition *definition =
          isSgFunctionDefinition(param_scope->get_parent())) {
    SageBuilder::pushScopeStack(definition);
  } else if (param_scope->get_parent() != nullptr) {
    std::cerr << "REX_FLANG_INVARIANT[procedure-construction]: parameter "
                 "scope has an unexpected physical owner\n";
    ROSE_ABORT();
  }
  SageBuilder::pushScopeStack(param_scope);
}

void SageTreeBuilder::Leave(
    SgFunctionParameterList *param_list, SgScopeStatement *param_scope,
    const std::list<SgVariableSymbol *> &exact_dummy_symbols) {
  MLOG_TRACE_CXX(MLOG_FRONTEND)
      << "SageTreeBuilder::Leave(exact Fortran parameters) \n";

  ASSERT_not_null(param_list);
  ASSERT_not_null(param_scope);
  ASSERT_require(param_scope == SageBuilder::topScopeStack());

  auto requireFreshParameterSource = [](SgInitializedName *parameter) {
    ASSERT_not_null(parameter);
    if (parameter->get_file_info() != nullptr ||
        parameter->get_startOfConstruct() != nullptr ||
        parameter->get_endOfConstruct() != nullptr) {
      std::cerr << "REX_FLANG_INVARIANT[dummy-parameter-source]: parameter '"
                << parameter->get_name().str()
                << "' was source-classified before its procedure declaration "
                   "selected semantic-only or source-spelled provenance\n";
      ROSE_ABORT();
    }
  };

  for (SgVariableSymbol *symbol : exact_dummy_symbols) {
    if (symbol == nullptr) {
      SgInitializedName *label = SageBuilder::buildInitializedName_nfi(
          "*", SgTypeLabel::createType(), /*initializer=*/nullptr);
      ASSERT_not_null(label);
      requireFreshParameterSource(label);
      label->set_scope(param_scope);
      label->set_parent(param_list);
      param_list->append_arg(label);
      continue;
    }

    SgInitializedName *declaration = symbol->get_declaration();
    if (declaration == nullptr || declaration->get_type() == nullptr ||
        declaration->get_scope() != param_scope ||
        symbol->get_scope() != param_scope ||
        param_scope->find_symbol_from_declaration(declaration) != symbol) {
      std::cerr << "REX_FLANG_INVARIANT[dummy-parameter-handoff]: exact "
                   "producer-published dummy symbol is malformed\n";
      ROSE_ABORT();
    }
    SgInitializedName *parameter = SageBuilder::buildInitializedName_nfi(
        declaration->get_name(), declaration->get_type(),
        /*initializer=*/nullptr);
    ASSERT_not_null(parameter);
    requireFreshParameterSource(parameter);
    parameter->get_storageModifier() = declaration->get_storageModifier();
    requireFortranParameterSemanticSurface(parameter, "defining declaration");
    parameter->set_scope(param_scope);
    parameter->set_parent(param_list);
    param_list->append_arg(parameter);
    if (parameter->get_scope() != param_scope ||
        parameter->get_parent() != param_list) {
      std::cerr << "REX_FLANG_INVARIANT[dummy-parameter-handoff]: "
                   "procedure parameter was not published in its exact "
                   "construction scope\n";
      ROSE_ABORT();
    }
  }

  SageBuilder::popScopeStack();
  if (SgFunctionDefinition *definition =
          isSgFunctionDefinition(param_scope->get_parent())) {
    if (definition->get_parent() != nullptr ||
        definition->get_declaration() != nullptr ||
        definition->get_body() != param_scope ||
        definition->get_construction_physical_output_owner() == nullptr ||
        definition->get_fortran_construction_name().getString().empty() ||
        definition != SageBuilder::topScopeStack()) {
      std::cerr << "REX_FLANG_INVARIANT[procedure-construction]: exact "
                   "bottom-up function definition was modified before "
                   "declaration construction\n";
      ROSE_ABORT();
    }
    SageBuilder::popScopeStack();
  } else if (isSgFunctionParameterScope(param_scope) == nullptr ||
             param_scope->get_parent() != nullptr) {
    std::cerr << "REX_FLANG_INVARIANT[procedure-construction]: nondefining "
                 "parameter scope lost its detached transaction ownership\n";
    ROSE_ABORT();
  }
}

void SageTreeBuilder::Enter(SgFunctionDefinition *&function_def) {
  MLOG_TRACE_CXX(MLOG_FRONTEND)
      << "SageTreeBuilder::Enter(SgFunctionDefinition*) \n";

  SgBasicBlock *block = SageBuilder::buildBasicBlock_nfi();

  function_def = new SgFunctionDefinition(block);
  ASSERT_not_null(function_def);
  requireFreshUnclassifiedSource(function_def, "function-definition");

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
    SgFunctionDefinition *exact_definition,
    const LanguageTranslation::FunctionModifierList &modifiers,
    bool is_defining_decl, const SourcePositions &sources,
    std::vector<Rose::builder::Token> &comments,
    SgProcedureHeaderStatement *canonical_nondefining,
    const SageBuilder::FortranBlockDataBuilderIdentity *block_data_identity) {
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
  const bool canonicalWasPredeclared = canonical_nondefining != nullptr;

  if (block_data_identity != nullptr) {
    if (return_type != nullptr) {
      std::cerr << "REX_FLANG_INVARIANT[block-data-return-type]: BLOCK DATA "
                   "was supplied a function return type\n";
      ROSE_ABORT();
    }
    return_type = SageBuilder::buildVoidType();
    subprogram_kind = SgProcedureHeaderStatement::e_block_data_subprogram_kind;
  } else if (return_type == nullptr) {
    return_type = SageBuilder::buildVoidType();
    subprogram_kind = SgProcedureHeaderStatement::e_subroutine_subprogram_kind;
  } else {
    subprogram_kind = SgProcedureHeaderStatement::e_function_subprogram_kind;
  }

  if (is_defining_decl) {
    SgBasicBlock *exact_body =
        exact_definition != nullptr ? exact_definition->get_body() : nullptr;
    const SgName construction_identity =
        block_data_identity != nullptr ? block_data_identity->symbol_table_key
                                       : SgName(name);
    if (exact_definition == nullptr ||
        exact_definition->get_parent() != nullptr ||
        exact_definition->get_declaration() != nullptr ||
        exact_body == nullptr || exact_body->get_parent() != exact_definition ||
        exact_definition->get_construction_physical_output_owner() != scope ||
        exact_definition->get_fortran_construction_name() !=
            construction_identity) {
      std::cerr << "REX_FLANG_INVARIANT[procedure-construction]: defining "
                   "procedure '"
                << name
                << "' did not supply one exact bottom-up definition and "
                   "body\n";
      ROSE_ABORT();
    }
    if (canonical_nondefining == nullptr) {
      SgFunctionParameterList *canonicalParameters =
          cloneFortranCanonicalParameterList(param_list);
      canonical_nondefining = SB::buildNondefiningProcedureHeaderStatement(
          SageBuilder::function_declaration_ownership::
              semanticAuxiliaryPendingExactSource(),
          SgName(name), return_type, canonicalParameters, subprogram_kind,
          SgProcedureHeaderStatement::
              e_fortran_procedure_source_form_semantic_only,
          scope, block_data_identity);
      ASSERT_not_null(canonical_nondefining);
    }
    {
      SgFunctionType *canonical_type = canonical_nondefining->get_type();
      if (canonical_nondefining->get_scope() != scope ||
          canonical_nondefining->get_firstNondefiningDeclaration() !=
              canonical_nondefining ||
          canonical_nondefining->get_definingDeclaration() != nullptr ||
          canonical_type == nullptr ||
          canonical_nondefining->get_subprogram_kind() != subprogram_kind ||
          canonical_nondefining->get_fortran_procedure_source_form() !=
              SgProcedureHeaderStatement::
                  e_fortran_procedure_source_form_semantic_only) {
        std::cerr << "REX_FLANG_INVARIANT[procedure-definition-chain]: "
                     "canonical declaration of '"
                  << name
                  << "' is incompatible with its defining declaration\n";
        ROSE_ABORT();
      }

      SgFunctionParameterList *canonical_params =
          canonical_nondefining->get_parameterList();
      ASSERT_not_null(canonical_params);
      SgInitializedNamePtrList &definition_args = param_list->get_args();
      const SgInitializedNamePtrList &canonical_args =
          canonical_params->get_args();
      if (definition_args.size() != canonical_args.size()) {
        std::cerr << "REX_FLANG_INVARIANT[procedure-definition-signature]: "
                     "definition of '"
                  << name << "' has " << definition_args.size()
                  << " dummy arguments, but its canonical declaration has "
                  << canonical_args.size() << "\n";
        ROSE_ABORT();
      }
      for (std::size_t i = 0; i < definition_args.size(); ++i) {
        SgInitializedName *definition_arg = definition_args[i];
        SgInitializedName *canonical_arg = canonical_args[i];
        if (definition_arg == nullptr || canonical_arg == nullptr ||
            definition_arg->get_name() != canonical_arg->get_name() ||
            canonical_arg->get_type() == nullptr) {
          std::cerr << "REX_FLANG_INVARIANT[procedure-definition-signature]: "
                       "dummy argument "
                    << i << " of '" << name
                    << "' does not match its canonical declaration\n";
          ROSE_ABORT();
        }

        // The internal-procedure prepass and definition pass visit the same
        // Flang semantic declaration.  Kind expressions are AST nodes, so a
        // second visit can otherwise manufacture structurally identical but
        // pointer-distinct SgType nodes.  A declaration chain must share one
        // canonical function signature; reuse its exact parameter types just
        // as the defining-function builder reuses its exact SgFunctionType.
        definition_arg->set_type(canonical_arg->get_type());
      }
      return_type = canonical_type->get_return_type();
      ASSERT_not_null(return_type);

      SgFunctionType *definition_type =
          SageBuilder::buildFunctionType(return_type, param_list);
      if (definition_type != canonical_type) {
        std::cerr << "REX_FLANG_INVARIANT[procedure-definition-signature]: "
                     "definition of '"
                  << name << "' did not reuse its canonical function type"
                  << " (canonical=" << canonical_type
                  << ", definition=" << definition_type
                  << ", canonical-return=" << canonical_type->get_return_type()
                  << ", definition-return="
                  << definition_type->get_return_type()
                  << ", canonical-mangled='"
                  << canonical_type->get_mangled().getString()
                  << "', definition-mangled='"
                  << definition_type->get_mangled().getString() << "')\n";
        for (std::size_t i = 0; i < definition_args.size(); ++i) {
          std::cerr << "  argument[" << i
                    << "] canonical=" << canonical_args[i]->get_type()
                    << " definition=" << definition_args[i]->get_type() << "\n";
        }
        ROSE_ABORT();
      }
      function_decl = SB::buildProcedureHeaderStatementFromExactDefinition(
          SageBuilder::function_declaration_ownership::sourceLexicalIn(scope),
          exact_definition, name.c_str(), return_type, param_list,
          subprogram_kind, FortranProcedureHeaderSourceForm(sources), scope,
          canonical_nondefining, block_data_identity);
    }
    ASSERT_not_null(function_decl);

    function_def = function_decl->get_definition();
    function_body = function_def->get_body();
    ASSERT_not_null(function_def);
    ASSERT_not_null(function_body);
    if (function_def != exact_definition ||
        function_body != exact_definition->get_body() ||
        function_def->get_construction_physical_output_owner() != nullptr ||
        !function_def->get_fortran_construction_name().is_null() ||
        function_def->get_scope() != scope) {
      std::cerr << "REX_FLANG_INVARIANT[procedure-construction]: defining "
                   "procedure '"
                << name << "' did not consume its exact construction tree\n";
      ROSE_ABORT();
    }

    if (language_ == LanguageEnum::Fortran ||
        SageInterface::is_language_case_insensitive() ||
        scope->isCaseInsensitive()) {
      function_def->setCaseInsensitive(true);
      function_body->setCaseInsensitive(true);
    }

    SageBuilder::pushScopeStack(function_def);
    SageBuilder::pushScopeStack(function_body);
  } else {
    if (exact_definition != nullptr) {
      std::cerr << "REX_FLANG_INVARIANT[procedure-construction]: nondefining "
                   "procedure '"
                << name << "' received a function definition\n";
      ROSE_ABORT();
    }
    function_decl = SB::buildNondefiningProcedureHeaderStatement(
        SageBuilder::function_declaration_ownership::semanticAuxiliary(),
        SgName(name), return_type, param_list, subprogram_kind,
        SgProcedureHeaderStatement::
            e_fortran_procedure_source_form_semantic_only,
        scope, block_data_identity);
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
  if (function_def && function_def != exact_definition)
    setSourcePosition(function_def, std::get<1>(sources), std::get<2>(sources));
  if (function_body && function_def != exact_definition)
    setSourcePosition(function_body, std::get<1>(sources),
                      std::get<2>(sources));
  if (exact_definition != nullptr) {
    for (SgLocatedNode *node : {static_cast<SgLocatedNode *>(exact_definition),
                                static_cast<SgLocatedNode *>(function_body)}) {
      if (node->get_startOfConstruct() == nullptr ||
          node->get_endOfConstruct() == nullptr) {
        std::cerr << "REX_FLANG_INVARIANT[procedure-construction]: exact "
                     "definition tree of '"
                  << name << "' lost source classification\n";
        ROSE_ABORT();
      }
    }
  }

  if (is_fortran_language) {
    if (fs.line <= 0 || fs.column <= 0 || fs.path.empty()) {
      std::cerr << "REX_FLANG_INVARIANT[parameter-location]: function '"
                << function_decl->get_name().str()
                << "' has no exact source anchor for its parameters\n";
      ROSE_ABORT();
    }
    initializeFortranParameterSourceLocations(function_decl, fs);
    if (is_defining_decl) {
      SgProcedureHeaderStatement *canonical = isSgProcedureHeaderStatement(
          function_decl->get_firstNondefiningDeclaration());
      if (canonical == nullptr || canonical == function_decl) {
        std::cerr << "REX_FLANG_INVARIANT[procedure-canonical-chain]: "
                     "defining procedure '"
                  << function_decl->get_name().str()
                  << "' has no distinct canonical declaration\n";
        ROSE_ABORT();
      }
      if (canonicalWasPredeclared) {
        requireFortranProcedureCanonicalSource(canonical, scope, fs, fe);
      } else {
        attachFortranProcedureCanonical(canonical, scope, fs, fe);
      }
    }
  } else {
    publishTransformationSourceOnce(function_decl->get_parameterList(),
                                    "non-Fortran-parameter-list");
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
  ASSERT_not_null(param_scope);
  const bool is_defining_decl =
      isSgFunctionParameterScope(param_scope) == nullptr;
  if (is_defining_decl) {
    ValidateResolvedFortranLabelSymbols(function_decl->get_definition(),
                                        "procedure");
    SgBasicBlock *param_block = isSgBasicBlock(param_scope);
    SgBasicBlock *function_body = isSgBasicBlock(SageBuilder::topScopeStack());
    SgFunctionDefinition *definition =
        function_body != nullptr
            ? isSgFunctionDefinition(function_body->get_parent())
            : nullptr;
    if (param_block == nullptr || function_body == nullptr ||
        function_body != param_block || definition == nullptr ||
        definition != function_decl->get_definition() ||
        definition->get_body() != function_body) {
      std::cerr << "REX_FLANG_INVARIANT[param-scope-finalization]: defining "
                   "Fortran procedure '"
                << function_decl->get_name().str()
                << "' was not built directly in its exact function body\n";
      ROSE_ABORT();
    }
    SageBuilder::popScopeStack(); // function body
    SageBuilder::popScopeStack(); // function definition
  } else {
    ASSERT_not_null(isSgFunctionParameterScope(param_scope));
    SageInterface::completeDetachedFunctionParameterScopeConstruction(
        function_decl, isSgFunctionParameterScope(param_scope));
    if (param_scope->get_parent() != function_decl ||
        param_scope->get_scope() != function_decl->get_scope()) {
      std::cerr << "REX_FLANG_INVARIANT[parameter-scope-owner]: nondefining "
                   "procedure '"
                << function_decl->get_name().str()
                << "' did not consume its exact parameter-scope transaction\n";
      ROSE_ABORT();
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
            for (PreprocessingInfo *info : *info_list) {
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
              }
            }
            for (PreprocessingInfo *info : to_move) {
              body->detachPreprocessingInfo(info);
              info->setRelativePosition(PreprocessingInfo::before);
            }
            PreprocessingInfo *prev = nullptr;
            for (PreprocessingInfo *info : to_move) {
              if (prev == nullptr) {
                first_stmt->addToAttachedPreprocessingInfo(
                    info, PreprocessingInfo::before);
              } else {
                first_stmt->insertToAttachedPreprocessingInfo(info, prev);
              }
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
        auto collect_comments =
            [&](SgLocatedNode *owner, AttachedPreprocessingInfoType *info_list,
                AttachedPreprocessingInfoType &out, bool skip_before) {
              if (info_list == nullptr || info_list->empty()) {
                return;
              }
              AttachedPreprocessingInfoType selected;
              for (PreprocessingInfo *info : *info_list) {
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
                    (!skip_before || info->getRelativePosition() !=
                                         PreprocessingInfo::before)) {
                  selected.push_back(info);
                }
              }
              for (PreprocessingInfo *info : selected) {
                owner->detachPreprocessingInfo(info);
                out.push_back(info);
              }
            };
        AttachedPreprocessingInfoType to_move;
        collect_comments(function_decl,
                         function_decl->getAttachedPreprocessingInfo(), to_move,
                         /*skip_before=*/true);
        collect_comments(body, body->getAttachedPreprocessingInfo(), to_move,
                         /*skip_before=*/false);
        for (SgStatement *stmt : body->get_statements()) {
          if (stmt == nullptr) {
            continue;
          }
          collect_comments(stmt, stmt->getAttachedPreprocessingInfo(), to_move,
                           /*skip_before=*/false);
        }
        for (PreprocessingInfo *info : to_move) {
          info->setRelativePosition(PreprocessingInfo::before);
        }
        PreprocessingInfo *prev = nullptr;
        for (PreprocessingInfo *info : to_move) {
          if (prev == nullptr) {
            function_decl->addToAttachedPreprocessingInfo(
                info, PreprocessingInfo::before);
          } else {
            function_decl->insertToAttachedPreprocessingInfo(info, prev);
          }
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

  SgScopeStatement *lexical_scope = SageBuilder::topScopeStack();
  ASSERT_not_null(lexical_scope);
  if (function_decl->get_parent() != lexical_scope ||
      function_decl->get_scope() != lexical_scope ||
      !lexical_scope->statementExistsInScope(function_decl)) {
    std::cerr << "REX_FLANG_INVARIANT[procedure-lexical-owner]: procedure '"
              << function_decl->get_name().str()
              << "' was not source-owned at construction\n";
    ROSE_ABORT();
  }
}

void SageTreeBuilder::Leave(SgFunctionDeclaration *function_decl,
                            SgScopeStatement *param_scope, bool have_end_stmt,
                            SgVariableSymbol *exact_result_symbol) {
  MLOG_TRACE_CXX(MLOG_FRONTEND)
      << "SageTreeBuilder::Leave(SgFunctionDeclaration*) \n";

  Leave(function_decl, param_scope);

  SgProcedureHeaderStatement *procedure =
      isSgProcedureHeaderStatement(function_decl);
  ASSERT_not_null(procedure);
  if (procedure->isFunction()) {
    SgFunctionDefinition *definition = function_decl->get_definition();
    SgBasicBlock *body =
        definition != nullptr ? definition->get_body() : nullptr;
    SgInitializedName *result = exact_result_symbol != nullptr
                                    ? exact_result_symbol->get_declaration()
                                    : nullptr;
    if (body == nullptr || result == nullptr || result->get_type() == nullptr ||
        result->get_scope() != body ||
        exact_result_symbol->get_scope() != body ||
        body->find_symbol_from_declaration(result) != exact_result_symbol ||
        function_decl->get_type() == nullptr ||
        function_decl->get_type()->get_return_type() != result->get_type()) {
      std::cerr << "REX_FLANG_INVARIANT[result-finalization]: function '"
                << function_decl->get_name().str()
                << "' did not receive its exact producer-published result "
                   "symbol and type\n";
      ROSE_ABORT();
    }
    if (SgVariableDeclaration *declaration =
            isSgVariableDeclaration(result->get_parent())) {
      SgAuxiliaryDeclarationList *auxiliary =
          isSgAuxiliaryDeclarationList(declaration->get_parent());
      const bool exact_source_owner = declaration->get_parent() == body;
      const bool exact_auxiliary_owner =
          auxiliary != nullptr && auxiliary->get_parent() == body &&
          body->get_auxiliary_declarations() == auxiliary;
      if (declaration->get_scope() != body ||
          (!exact_source_owner && !exact_auxiliary_owner)) {
        std::cerr << "REX_FLANG_INVARIANT[result-finalization]: result "
                     "declaration of function '"
                  << function_decl->get_name().str()
                  << "' has malformed final ownership\n";
        ROSE_ABORT();
      }
    } else {
      std::cerr << "REX_FLANG_INVARIANT[result-finalization]: function result '"
                << result->get_name().str()
                << "' has no exact variable-declaration owner\n";
      ROSE_ABORT();
    }
    procedure->set_result_name(result);
  } else if (exact_result_symbol != nullptr) {
    std::cerr << "REX_FLANG_INVARIANT[result-finalization]: non-function '"
              << function_decl->get_name().str()
              << "' received a function-result symbol\n";
    ROSE_ABORT();
  }

  if (have_end_stmt) {
    function_decl->set_named_in_end_statement(have_end_stmt);
  }
}

void SageTreeBuilder::Enter(SgDerivedTypeStatement *&derived_type_stmt,
                            const std::string &name) {
  MLOG_TRACE_CXX(MLOG_FRONTEND)
      << "SageTreeBuilder::Enter(SgDerivedTypeStatement* &, ...) \n";

  derived_type_stmt = SageBuilder::buildDerivedTypeStatement(
      SageBuilder::declaration_ownership::sourceLexicalPendingExactSource(),
      name, SageBuilder::topScopeStack());
  requireFreshUnclassifiedSource(derived_type_stmt, "derived-type-statement");

  SgClassDefinition *class_defn = derived_type_stmt->get_definition();
  ASSERT_not_null(class_defn);
  requireFreshUnclassifiedSource(class_defn, "derived-type-definition");
  ASSERT_require(SageBuilder::topScopeStack()->isCaseInsensitive());

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
      name, true, SageBuilder::topScopeStack(),
      SageBuilder::e_namespace_declaration_canonical_generated_lexical, nullptr,
      nullptr, nullptr, std::nullopt);

  SgNamespaceDefinitionStatement *namespace_defn =
      namespace_decl->get_definition();
  ASSERT_not_null(namespace_defn);
  ASSERT_require(SageBuilder::topScopeStack()->isCaseInsensitive());

  // TEMPORARY: fix in SageBuilder
  namespace_defn->setCaseInsensitive(true);
  ASSERT_require(namespace_defn->isCaseInsensitive());

  SageBuilder::pushScopeStack(namespace_defn);
}

void SageTreeBuilder::Leave(SgNamespaceDeclarationStatement *namespace_decl) {
  MLOG_TRACE_CXX(MLOG_FRONTEND)
      << "SageTreeBuilder::Leave(SgNamespaceDeclarationStatement*, ...) \n";

  SageBuilder::popScopeStack(); // namespace definition
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

  assign_op = SageBuilder::buildBinaryExpression_nfi<SgAssignOp>(
      lhs, rhs, lhs->get_type());
  assign_stmt = SageBuilder::buildExprStatement_nfi(assign_op);
}

void SageTreeBuilder::Leave(SgExprStatement *exprStmt,
                            std::vector<std::string> &labels) {
  MLOG_TRACE_CXX(MLOG_FRONTEND)
      << "SageTreeBuilder::Leave(SgExprStatement*) \n";

  SgStatement *stmt = wrapStmtWithLabels(exprStmt, labels);
  SageInterface::appendStatement(stmt, SB::topScopeStack());
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
  cast_expr = SageBuilder::buildTransformationCastExp_nfi(
      cast_operand, conv_type, SgCastExp::e_C_style_cast);
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
      (opt_code) ? *opt_code
                 : SageBuilder::buildNullExpression_nfi(
                       SgNullExpression::e_null_expression_syntactic_absence);
  SgExpression *quiet =
      (opt_quiet) ? *opt_quiet
                  : SageBuilder::buildNullExpression_nfi(
                        SgNullExpression::e_null_expression_syntactic_absence);

  ASSERT_not_null(code);
  control_stmt = new SgProcessControlStatement(code);
  ASSERT_not_null(control_stmt);
  requireFreshUnclassifiedSource(control_stmt, "process-control-statement");

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
      (opt_expr) ? *opt_expr
                 : SageBuilder::buildNullExpression_nfi(
                       SgNullExpression::e_null_expression_syntactic_absence);
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
  doStmt = SB::buildFortranDo_nfi(initialization, bound, increment, body);

  // output "END DO"
  doStmt->set_has_end_statement(true);
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
  requireFreshUnclassifiedSource(print_stmt, "print-statement");
  print_stmt->set_io_statement(SgIOStatement::e_print);

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

  while_stmt = SageBuilder::buildWhileStmt_nfi(condition_stmt, body);
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
  requireFreshUnclassifiedSource(implicit_stmt, "implicit-none-statement");

  if (none_external && none_type) {
    implicit_stmt->set_implicit_spec(
        SgImplicitStatement::e_none_external_and_type);
  } else if (none_external) {
    implicit_stmt->set_implicit_spec(SgImplicitStatement::e_none_external);
  } else if (none_type) {
    implicit_stmt->set_implicit_spec(SgImplicitStatement::e_none_type);
  } else {
    implicit_stmt->set_implicit_spec(SgImplicitStatement::e_none);
  }
}

void SageTreeBuilder::Enter(
    SgImplicitStatement *&implicit_stmt,
    std::list<std::tuple<SgType *, SgSymbol *, FortranImplicitTypeSpecKind,
                         std::list<std::tuple<char, std::optional<char>>>>>
        &implicit_spec_list) {
  MLOG_TRACE_CXX(MLOG_FRONTEND)
      << "SageTreeBuilder::Enter(SgImplicitStatement* &, implicit_spec_list)\n";
  // Implicit with Implicit-Spec

  implicit_stmt = new SgImplicitStatement(false);
  ASSERT_not_null(implicit_stmt);
  implicit_stmt->set_definingDeclaration(implicit_stmt);
  implicit_stmt->set_firstNondefiningDeclaration(implicit_stmt);
  requireFreshUnclassifiedSource(implicit_stmt, "implicit-spec-statement");
  implicit_stmt->set_implicit_spec(
      SgImplicitStatement::e_has_implicit_spec_list);

  SgInitializedNamePtrList &name_list = implicit_stmt->get_variables();
  SgScopeStatement *scope = SageBuilder::topScopeStack();
  ASSERT_not_null(scope);

  // Step through the list of Implicit Specs
  for (std::tuple<SgType *, SgSymbol *, FortranImplicitTypeSpecKind,
                  std::list<std::tuple<char, std::optional<char>>>>
           implicit_spec : implicit_spec_list) {
    SgType *type;
    SgSymbol *sourceDerivedTypeSymbol;
    FortranImplicitTypeSpecKind fortranTypeSpec;
    std::list<std::tuple<char, std::optional<char>>> letter_spec_list;
    std::tie(type, sourceDerivedTypeSymbol, fortranTypeSpec, letter_spec_list) =
        implicit_spec;

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
      publishTransformationSourceOnce(init_name, "implicit-letter-range");
      init_name->set_fortran_source_derived_type_symbol(
          sourceDerivedTypeSymbol);
      switch (fortranTypeSpec) {
      case FortranImplicitTypeSpecKind::intrinsic:
        init_name->set_fortran_type_spec(
            SgInitializedName::e_fortran_type_spec_default);
        break;
      case FortranImplicitTypeSpecKind::type:
        init_name->set_fortran_type_spec(
            SgInitializedName::e_fortran_type_spec_type);
        break;
      case FortranImplicitTypeSpecKind::class_type:
        init_name->set_fortran_type_spec(
            SgInitializedName::e_fortran_type_spec_class);
        break;
      case FortranImplicitTypeSpecKind::type_star:
        init_name->set_fortran_type_spec(
            SgInitializedName::e_fortran_type_spec_type_star);
        break;
      case FortranImplicitTypeSpecKind::class_star:
        init_name->set_fortran_type_spec(
            SgInitializedName::e_fortran_type_spec_class_star);
        break;
      }
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

  module_stmt = SageBuilder::buildModuleStatement(
      SageBuilder::declaration_ownership::sourceLexicalPendingExactSource(),
      name, SageBuilder::topScopeStack());
  requireFreshUnclassifiedSource(module_stmt, "module-statement");

  SgClassDefinition *class_def = module_stmt->get_definition();
  ASSERT_not_null(class_def);
  requireFreshUnclassifiedSource(class_def, "module-definition");

  SageBuilder::pushScopeStack(class_def);
}

void SageTreeBuilder::Leave(SgModuleStatement *module_stmt) {
  MLOG_TRACE_CXX(MLOG_FRONTEND)
      << "SageTreeBuilder::Leave(SgModuleStatement*, ...) \n";
  ASSERT_not_null(module_stmt);

  ValidateResolvedFortranLabelSymbols(module_stmt->get_definition(), "MODULE");

  SageBuilder::popScopeStack(); // class definition
}

void SageTreeBuilder::Enter(SgUseStatement *&use_stmt,
                            const std::string &module_name,
                            const std::string &module_nature) {
  MLOG_TRACE_CXX(MLOG_FRONTEND)
      << "SageTreeBuilder::Enter(SgUseStatement* &, ...)\n";

  use_stmt = new SgUseStatement(module_name, false, module_nature);
  ASSERT_not_null(use_stmt);
  use_stmt->set_definingDeclaration(use_stmt);
  use_stmt->set_firstNondefiningDeclaration(use_stmt);
  requireFreshUnclassifiedSource(use_stmt, "use-statement");

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
  requireFreshUnclassifiedSource(contains_stmt, "contains-statement");
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

  SgScopeStatement *scope = SageBuilder::topScopeStack();
  ASSERT_not_null(scope);
  var_decl =
      BuildUnclassifiedSourceVariableDeclaration(name, type, init_expr, scope);
  if (var_decl->get_variables().size() != 1 ||
      var_decl->get_variables().front() == nullptr) {
    std::cerr << "REX_FLANG_INVARIANT[source-type-publication]: Fortran "
                 "entity has no exact initialized-name owner\n";
    ROSE_ABORT();
  }
  var_decl->get_variables().front()->set_fortran_source_type(type);
  SageBuilder::initializePendingSourceVariableDeclarationProvenance(var_decl);
}

void SageTreeBuilder::Enter(
    SgVariableDeclaration *&var_decl, SgType *base_type,
    std::list<std::tuple<std::string, SgType *, SgType *, SgExpression *>>
        &init_info) {
  MLOG_TRACE_CXX(MLOG_FRONTEND)
      << "SageTreeBuilder::Enter(SgVariableDeclaration* &, std::tuple<...>, "
         "...) \n";

  // Step through list of tuples to create the multi variable declaration
  for (std::list<std::tuple<std::string, SgType *, SgType *,
                            SgExpression *>>::iterator it = init_info.begin();
       it != init_info.end(); ++it) {
    std::string name;
    SgType *semantic_type;
    SgType *source_type;
    SgExpression *init_expr;
    std::tie(name, semantic_type, source_type, init_expr) = *it;

    if (semantic_type == nullptr) {
      std::cerr << "REX_FLANG_INVARIANT[semantic-type-publication]: Fortran "
                   "entity '"
                << name << "' has no exact semantic type\n";
      ROSE_ABORT();
    }
    if (source_type == nullptr) {
      std::cerr << "REX_FLANG_INVARIANT[source-type-publication]: Fortran "
                   "entity '"
                << name << "' has no exact source type\n";
      ROSE_ABORT();
    }

    if (it == init_info.begin()) { // On first pass, call Enter() to create
                                   // variable declaration
      SgScopeStatement *scope = SageBuilder::topScopeStack();
      ASSERT_not_null(scope);
      var_decl = BuildUnclassifiedSourceVariableDeclaration(name, semantic_type,
                                                            init_expr, scope);
      if (var_decl == nullptr || var_decl->get_variables().size() != 1 ||
          var_decl->get_variables().front() == nullptr) {
        std::cerr << "REX_FLANG_INVARIANT[entity-publication]: first Fortran "
                     "entity has no exact initialized-name owner\n";
        ROSE_ABORT();
      }
      var_decl->get_variables().front()->set_fortran_source_type(source_type);
    } else { // On later passes, create new initialized name and append to the
             // var decl
      SgAssignInitializer *init = nullptr;
      if (init_expr) {
        init =
            SageBuilder::buildAssignInitializer_nfi(init_expr, semantic_type);
      }

      SgInitializedName *init_name =
          SageBuilder::buildInitializedName_nfi(name, semantic_type, init);
      ASSERT_not_null(init_name);
      init_name->set_fortran_source_type(source_type);
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
    }
  }
  SageBuilder::initializePendingSourceVariableDeclarationProvenance(var_decl);
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
      var_decl->set_linkage("C");
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
        SgType *source_type = init_name->get_fortran_source_type();
        if (source_type != nullptr && isSgPointerType(source_type) == nullptr) {
          SgPointerType *source_pointer = new SgPointerType(source_type);
          ASSERT_not_null(source_pointer);
          source_pointer->set_fortran_source_syntax(true);
          init_name->set_fortran_source_type(source_pointer);
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

  SgScopeStatement *scope = SageBuilder::topScopeStack();
  SgEnumDeclaration *canonical =
      SageBuilder::buildNondefiningEnumDeclaration_nfi(
          name, false, scope,
          SageBuilder::declaration_ownership::semanticAuxiliary(), nullptr);
  enum_decl = SageBuilder::buildEnumDeclaration_nfi(
      SageBuilder::declaration_ownership::sourceLexicalPendingExactSource(),
      name, false, scope, canonical);
}

void SageTreeBuilder::Leave(SgEnumDeclaration *enum_decl) {
  MLOG_TRACE_CXX(MLOG_FRONTEND)
      << "SageTreeBuilder::Leave(SgEnumDeclaration*) \n";

  SgScopeStatement *scope = SageBuilder::topScopeStack();
  const SgStatementPtrList statements = scope->generateStatementList();
  if (enum_decl->get_parent() != scope || enum_decl->get_scope() != scope ||
      std::count(statements.begin(), statements.end(), enum_decl) != 1) {
    std::cerr << "REX_FLANG_INVARIANT[enum-owner]: defining declaration does "
                 "not have one exact lexical owner\n";
    ROSE_ABORT();
  }
}

void SageTreeBuilder::Enter(SgEnumVal *&enum_val, const std::string &name,
                            SgEnumDeclaration *enum_decl, std::int64_t value,
                            const SourcePositionPair &positions,
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
    // This builder entry point is used only for a source-spelled Flang
    // enumerator.  Construct the value with fresh source state so the parser's
    // exact statement interval is its first and only provenance publication.
    // buildEnumVal_nfi installs the legacy shared-null file identity, which is
    // neither fresh nor an owned pending-source transaction.
    enum_val = new SgEnumVal(value, nondef_decl, name);
    ASSERT_not_null(enum_val);
    setSourcePosition(enum_val, std::get<0>(positions), std::get<1>(positions));
    init_expr = enum_val;
  }

  SgAssignInitializer *initializer =
      SageBuilder::buildAssignInitializer_nfi(init_expr, enum_type);
  SgInitializedName *init_name =
      SageBuilder::buildInitializedName_nfi(name, enum_type, initializer);

  init_name->set_scope(scope);
  init_name->set_declptr(def_decl);
  init_name->set_enum_constant_source_ownership(
      SgInitializedName::e_enum_constant_source_body);
  def_decl->append_enumerator(init_name);

  // Add an associated field symbol to the symbol table
  SgEnumFieldSymbol *enum_field_symbol = new SgEnumFieldSymbol(init_name);
  ASSERT_not_null(enum_field_symbol);
  scope->insert_symbol(name, enum_field_symbol);

  if (enum_type->get_parent() == nullptr) {
    enum_type->set_parent(enum_field_symbol);
  }
}

void SageTreeBuilder::Enter(SgTypedefDeclaration *&type_def,
                            const std::string &name, SgType *type) {
  MLOG_TRACE_CXX(MLOG_FRONTEND)
      << "SageTreeBuilder::Enter(SgTypedefDeclaration*) \n";
  SgScopeStatement *scope = SageBuilder::topScopeStack();

  type_def = SageBuilder::buildTypedefDeclaration_nfi(
      SageBuilder::typedef_declaration_ownership::sourceLexical(),
      SgTypedefDeclaration::e_typedef, name, type, scope);

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

  common_block = new SgCommonBlock();
  ASSERT_not_null(common_block);
  common_block->set_definingDeclaration(common_block);
  common_block->set_firstNondefiningDeclaration(common_block);
  requireFreshUnclassifiedSource(common_block, "common-block-statement");

  SgCommonBlockObjectPtrList &list = common_block->get_block_list();

  for (SgCommonBlockObject *common_block_object : common_block_object_list) {
    if (common_block_object == nullptr ||
        (common_block_object->get_parent() != nullptr &&
         common_block_object->get_parent() != common_block)) {
      std::cerr << "REX_FLANG_INVARIANT[common-block-owner]: COMMON block "
                   "object has no unique declaration owner\n";
      ROSE_ABORT();
    }
    common_block_object->set_parent(common_block);
    list.push_back(common_block_object);
  }
}

void SageTreeBuilder::Leave(SgCommonBlock *common_block) {
  MLOG_TRACE_CXX(MLOG_FRONTEND) << "SageTreeBuilder::Leave(SgCommonBlock*) \n";

  ASSERT_not_null(common_block);
  SageInterface::appendStatement(common_block, SageBuilder::topScopeStack());
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
      std::cerr << "REX_FLANG_INVARIANT[label-scope]: Fortran statement has "
                   "no enclosing program-unit definition\n";
      ROSE_ABORT();
    }
    for (const auto &label : reversed) {
      int labelValue = 0;
      const char *begin = label.data();
      const char *end = begin + label.size();
      const std::from_chars_result parsed =
          std::from_chars(begin, end, labelValue);
      if (label.empty() || parsed.ec != std::errc() || parsed.ptr != end ||
          labelValue <= 0 || labelValue > 99999) {
        std::cerr << "REX_FLANG_INVARIANT[numeric-label]: invalid Fortran "
                     "statement label '"
                  << label << "'\n";
        ROSE_ABORT();
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
SgExpression *buildAddOp_nfi(SgExpression *lhs, SgExpression *rhs,
                             SgType *result_type) {
  return SageBuilder::buildAddOp_nfi(lhs, rhs, result_type);
}

SgExpression *buildAndOp_nfi(SgExpression *lhs, SgExpression *rhs,
                             SgType *result_type) {
  return SageBuilder::buildAndOp_nfi(lhs, rhs, result_type);
}

SgExpression *buildDivideOp_nfi(SgExpression *lhs, SgExpression *rhs,
                                SgType *result_type) {
  return SageBuilder::buildDivideOp_nfi(lhs, rhs, result_type);
}

SgExpression *buildEqualityOp_nfi(SgExpression *lhs, SgExpression *rhs,
                                  SgType *result_type) {
  return SageBuilder::buildEqualityOp_nfi(lhs, rhs, result_type);
}

SgExpression *buildGreaterThanOp_nfi(SgExpression *lhs, SgExpression *rhs,
                                     SgType *result_type) {
  return SageBuilder::buildGreaterThanOp_nfi(lhs, rhs, result_type);
}

SgExpression *buildGreaterOrEqualOp_nfi(SgExpression *lhs, SgExpression *rhs,
                                        SgType *result_type) {
  return SageBuilder::buildGreaterOrEqualOp_nfi(lhs, rhs, result_type);
}

SgExpression *buildMultiplyOp_nfi(SgExpression *lhs, SgExpression *rhs,
                                  SgType *result_type) {
  return SageBuilder::buildMultiplyOp_nfi(lhs, rhs, result_type);
}

SgExpression *buildLessThanOp_nfi(SgExpression *lhs, SgExpression *rhs,
                                  SgType *result_type) {
  return SageBuilder::buildLessThanOp_nfi(lhs, rhs, result_type);
}

SgExpression *buildLessOrEqualOp_nfi(SgExpression *lhs, SgExpression *rhs,
                                     SgType *result_type) {
  return SageBuilder::buildLessOrEqualOp_nfi(lhs, rhs, result_type);
}

SgExpression *buildNotEqualOp_nfi(SgExpression *lhs, SgExpression *rhs,
                                  SgType *result_type) {
  return SageBuilder::buildNotEqualOp_nfi(lhs, rhs, result_type);
}

SgExpression *buildOrOp_nfi(SgExpression *lhs, SgExpression *rhs,
                            SgType *result_type) {
  return SageBuilder::buildOrOp_nfi(lhs, rhs, result_type);
}

SgExpression *buildMinusOp_nfi(SgExpression *i, SgType *result_type,
                               bool is_prefix /* = true */) {
  SgUnaryOp::Sgop_mode mode_enum;

  if (is_prefix) {
    mode_enum = SgUnaryOp::Sgop_mode::prefix;
  } else {
    mode_enum = SgUnaryOp::Sgop_mode::postfix;
  }

  return SageBuilder::buildMinusOp_nfi(i, result_type, mode_enum);
}

SgExpression *buildSubtractOp_nfi(SgExpression *lhs, SgExpression *rhs,
                                  SgType *result_type) {
  return SageBuilder::buildSubtractOp_nfi(lhs, rhs, result_type);
}

// Expressions
//
SgExpression *buildConcatenationOp_nfi(SgExpression *lhs, SgExpression *rhs,
                                       SgType *result_type) {
  return SageBuilder::buildConcatenationOp_nfi(lhs, rhs, result_type);
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
                                  SgType *precision_type,
                                  const std::string &str) {
  ASSERT_not_null(real_value);
  ASSERT_not_null(imaginary_value);
  ASSERT_not_null(precision_type);
  return SageBuilder::buildComplexVal_nfi(real_value, imaginary_value,
                                          precision_type, str);
}

SgExpression *buildSubscriptExpression_nfi(SgExpression *lower_bound,
                                           SgExpression *upper_bound,
                                           SgExpression *stride) {
  return SageBuilder::buildSubscriptExpression_nfi(lower_bound, upper_bound,
                                                   stride);
}

SgPntrArrRefExp *buildPntrArrRefExp_nfi(SgExpression *lhs, SgExpression *rhs,
                                        SgType *result_type) {
  return SageBuilder::buildPntrArrRefExp_nfi(lhs, rhs, result_type);
}

SgExpression *buildAggregateInitializer_nfi(SgExprListExp *initializers,
                                            SgType *type) {
  return SageBuilder::buildAggregateInitializer_nfi(
      initializers, type,
      SgAggregateInitializer::e_aggregate_initializer_source_fortran);
}

SgExpression *buildAsteriskShapeExp_nfi() {
  SgAsteriskShapeExp *shape = new SgAsteriskShapeExp();
  ASSERT_not_null(shape);
  requireFreshUnclassifiedSource(shape, "asterisk-shape-expression");

  return shape;
}

SgExpression *buildAssumedRankExp_nfi() {
  SgAssumedRankExp *shape = new SgAssumedRankExp();
  ASSERT_not_null(shape);
  requireFreshUnclassifiedSource(shape, "assumed-rank-expression");

  return shape;
}

SgExpression *
buildNullExpression_nfi(SgNullExpression::null_expression_role_enum role) {
  return SageBuilder::buildNullExpression_nfi(role);
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
  SgCommonBlockObject *common_block_object = new SgCommonBlockObject();
  ASSERT_not_null(common_block_object);
  common_block_object->set_block_name(name);
  if (expr_list != nullptr) {
    // The expression list is a structural container introduced by the Sage
    // bridge; the source-spelled COMMON objects are its children.  Publish the
    // container's semantic origin at the producer before it is attached to the
    // source-backed COMMON statement.
    publishTransformationSourceOnce(expr_list,
                                    "common-block-object-variable-list");
    common_block_object->set_variable_reference_list(expr_list);
    expr_list->set_parent(common_block_object);
  }
  publishTransformationSourceOnce(common_block_object, "common-block-object");
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

} // namespace SageBuilderCpp17

} // namespace builder
} // namespace Rose
