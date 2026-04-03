
#include <algorithm>

#include <cctype>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <functional>
#include <iostream>

#include <map>

#include <memory>

#include <set>

#include <sstream>
#include <vector>

#include <llvm/Support/FileSystem.h>
#include <llvm/Support/MemoryBuffer.h>
#include <llvm/Support/Path.h>
#include <llvm/Support/Program.h>

#include "clang-frontend-private.hpp"

#include "clang-frontend-utils.hpp"

#include "FileHelper.h"
#include "sage3basic.h"

#include "clang-to-dot.hpp"

#include "clang_paths.h"

#include "clang-include-option.h"

#include "ompAstConstruction.h"

#include <clang/Basic/DiagnosticFrontend.h>
#include <clang/Basic/DiagnosticLex.h>
#include <clang/Basic/DiagnosticSema.h>
#include <clang/Basic/SourceLocation.h>
#include <clang/Driver/Compilation.h>
#include <clang/Driver/Driver.h>
#include <clang/Driver/Job.h>
#include <clang/Lex/Lexer.h>

#include "rose_config.h"
#include "rose_paths.h"

namespace {

class RoseDiagnosticConsumer : public clang::DiagnosticConsumer {
  std::unique_ptr<clang::DiagnosticConsumer> delegate_;
  SagePreprocessorRecord *preprocessor_recorder_ = nullptr;

public:
  RoseDiagnosticConsumer(std::unique_ptr<clang::DiagnosticConsumer> delegate,
                         SagePreprocessorRecord *preprocessor_recorder)
      : delegate_(std::move(delegate)),
        preprocessor_recorder_(preprocessor_recorder) {}

  void BeginSourceFile(const clang::LangOptions &LangOpts,
                       const clang::Preprocessor *PP = nullptr) override {
    if (delegate_ != nullptr) {
      delegate_->BeginSourceFile(LangOpts, PP);
    }
  }

  void EndSourceFile() override {
    if (delegate_ != nullptr) {
      delegate_->EndSourceFile();
    }
  }

  void finish() override {
    if (delegate_ != nullptr) {
      delegate_->finish();
    }
  }

  void clear() override {
    clang::DiagnosticConsumer::clear();
    if (delegate_ != nullptr) {
      delegate_->clear();
    }
  }

  bool IncludeInDiagnosticCounts() const override {
    return delegate_ == nullptr
               ? DiagnosticConsumer::IncludeInDiagnosticCounts()
               : delegate_->IncludeInDiagnosticCounts();
  }

  void HandleDiagnostic(clang::DiagnosticsEngine::Level DiagLevel,
                        const clang::Diagnostic &Info) override {
    clang::DiagnosticConsumer::HandleDiagnostic(DiagLevel, Info);

    if (preprocessor_recorder_ != nullptr && Info.hasSourceManager()) {
      PreprocessingInfo::DirectiveType directive_type;
      bool should_record = true;
      switch (Info.getID()) {
      case clang::diag::pp_hash_warning:
        directive_type = PreprocessingInfo::CpreprocessorWarningDeclaration;
        break;
      case clang::diag::err_pp_hash_error:
        directive_type = PreprocessingInfo::CpreprocessorErrorDeclaration;
        break;
      default:
        should_record = false;
        break;
      }

      if (should_record) {
        preprocessor_recorder_->recordSourceDirective(Info.getLocation(),
                                                      directive_type);
      }
    }

    if (delegate_ != nullptr) {
      delegate_->HandleDiagnostic(DiagLevel, Info);
    }
  }
};

const char *
builtinPreincludeForLanguage(ClangToSageTranslator::Language language) {
  switch (language) {
  case ClangToSageTranslator::C:
    return "clang-builtin-c.h";
  case ClangToSageTranslator::CPLUSPLUS:
    return "clang-builtin-cpp.hpp";
  case ClangToSageTranslator::CUDA:
    return "clang-builtin-cuda.hpp";
  case ClangToSageTranslator::OPENCL:
    return "clang-builtin-opencl.h";
  case ClangToSageTranslator::OBJC:
  case ClangToSageTranslator::unknown:
    return nullptr;
  default:
    ROSE_ABORT();
  }
}

bool hasConfiguredPreinclude(const std::vector<std::string> &includes,
                             llvm::StringRef required_include) {
  for (const auto &include : includes) {
    llvm::StringRef include_ref(include);
    if (include_ref == required_include ||
        llvm::sys::path::filename(include_ref) == required_include) {
      return true;
    }
  }
  return false;
}

void assertRequiredPreincludeConfigured(
    const std::vector<std::string> &includes,
    ClangToSageTranslator::Language language, const char *stage) {
  const char *required_include = builtinPreincludeForLanguage(language);
  if (required_include == nullptr) {
    return;
  }
  if (hasConfiguredPreinclude(includes, required_include)) {
    return;
  }
  llvm::errs() << "ROSE Clang frontend invariant violation: required "
                  "preinclude '"
               << required_include << "' missing during " << stage << ".\n";
  ROSE_ABORT();
}

bool isRoseInternalOption(const std::string &arg) {
  return arg.rfind("-rose:", 0) == 0 || arg.rfind("--rose:", 0) == 0;
}

int countRoseOptionTrailingArguments(const std::string &arg) {
  if (arg.find('=') != std::string::npos) {
    return 0;
  }

  std::string canonical_arg = arg;
  if (canonical_arg.rfind("--rose:", 0) == 0) {
    canonical_arg.erase(0, 1);
  }

  if (!CommandlineProcessing::isOptionTakingSecondParameter(canonical_arg)) {
    return 0;
  }

  return CommandlineProcessing::isOptionTakingThirdParameter(canonical_arg) ? 2
                                                                            : 1;
}

struct TokenWithOffset {
  clang::Token token;
  unsigned offset;
};

bool sourceFileContainsPhysicalLineSplice(llvm::StringRef input_file) {
  llvm::ErrorOr<std::unique_ptr<llvm::MemoryBuffer>> buffer_or =
      llvm::MemoryBuffer::getFile(input_file);
  if (!buffer_or) {
    return false;
  }

  llvm::StringRef source_text = (*buffer_or)->getBuffer();
  auto has_line_ending_after = [&](size_t offset) {
    if (offset >= source_text.size()) {
      return false;
    }
    if (source_text[offset] == '\n') {
      return true;
    }
    return offset + 1 < source_text.size() && source_text[offset] == '\r' &&
           source_text[offset + 1] == '\n';
  };

  auto starts_preprocessor_directive = [&](size_t line_start) {
    size_t i = line_start;
    while (i < source_text.size()) {
      char current = source_text[i];
      if (current == ' ' || current == '\t' || current == '\f' ||
          current == '\v') {
        ++i;
        continue;
      }
      break;
    }

    if (i >= source_text.size()) {
      return false;
    }

    if (source_text[i] == '#') {
      return true;
    }

    return i + 2 < source_text.size() && source_text[i] == '?' &&
           source_text[i + 1] == '?' && source_text[i + 2] == '=';
  };

  bool directive_line = starts_preprocessor_directive(0);
  for (size_t i = 0; i < source_text.size();) {
    if (source_text[i] == '\n') {
      ++i;
      directive_line = starts_preprocessor_directive(i);
      continue;
    }

    if (source_text[i] == '\r' && i + 1 < source_text.size() &&
        source_text[i + 1] == '\n') {
      i += 2;
      directive_line = starts_preprocessor_directive(i);
      continue;
    }

    if (source_text[i] == '\\') {
      if (has_line_ending_after(i + 1)) {
        if (!directive_line) {
          return true;
        }
        i += source_text[i + 1] == '\n' ? 2 : 3;
        continue;
      }
      ++i;
      continue;
    }

    if (source_text[i] == '?' && i + 2 < source_text.size() &&
        source_text[i + 1] == '?' && source_text[i + 2] == '/' &&
        has_line_ending_after(i + 3)) {
      if (!directive_line) {
        return true;
      }
      i += source_text[i + 3] == '\n' ? 4 : 5;
      continue;
    }

    ++i;
  }

  return false;
}

bool hasUnsafePreprocessingInfo(SgLocatedNode *node) {
  if (node == NULL) {
    return false;
  }

  AttachedPreprocessingInfoType *infos = node->getAttachedPreprocessingInfo();
  if (infos == NULL) {
    return false;
  }

  for (PreprocessingInfo *info : *infos) {
    if (info == NULL) {
      continue;
    }

    switch (info->getTypeOfDirective()) {
    case PreprocessingInfo::C_StyleComment:
    case PreprocessingInfo::CplusplusStyleComment:
    case PreprocessingInfo::FortranStyleComment:
    case PreprocessingInfo::F90StyleComment:
    case PreprocessingInfo::CpreprocessorBlankLine:
      break;

    default:
      return true;
    }
  }

  return false;
}

static bool isBeforeSourcePosition(int lhs_line, int lhs_col, int rhs_line,
                                   int rhs_col) {
  return lhs_line < rhs_line || (lhs_line == rhs_line && lhs_col < rhs_col);
}

static bool isUnsafeDirectiveForRoundtrip(const PreprocessingInfo *info) {
  if (info == NULL) {
    return false;
  }

  switch (info->getTypeOfDirective()) {
  case PreprocessingInfo::C_StyleComment:
  case PreprocessingInfo::CplusplusStyleComment:
  case PreprocessingInfo::FortranStyleComment:
  case PreprocessingInfo::F90StyleComment:
  case PreprocessingInfo::CpreprocessorBlankLine:
    return false;

  default:
    return true;
  }
}

static bool functionDefinitionHasUnsafeDeclaratorPreprocessingInfo(
    SgFunctionDeclaration *decl) {
  if (decl == NULL) {
    return false;
  }

  SgFunctionDefinition *definition = decl->get_definition();
  SgBasicBlock *body = definition != NULL ? definition->get_body() : NULL;
  Sg_File_Info *decl_info = decl->get_file_info();
  Sg_File_Info *body_info = body != NULL ? body->get_startOfConstruct() : NULL;
  if (definition == NULL || body_info == NULL || body_info->get_line() <= 0 ||
      body_info->get_col() <= 0) {
    return hasUnsafePreprocessingInfo(definition);
  }

  AttachedPreprocessingInfoType *infos =
      definition->getAttachedPreprocessingInfo();
  if (infos == NULL) {
    return false;
  }

  const bool have_decl_start = decl_info != NULL && decl_info->get_line() > 0 &&
                               decl_info->get_col() > 0;
  const std::string decl_filename =
      decl_info != NULL ? decl_info->get_filenameString() : std::string();
  const std::string body_filename = body_info->get_filenameString();
  const int body_line = body_info->get_line();
  const int body_col = body_info->get_col();

  for (PreprocessingInfo *info : *infos) {
    if (!isUnsafeDirectiveForRoundtrip(info)) {
      continue;
    }

    const int info_line = info->getLineNumber();
    const int info_col = info->getColumnNumber();
    if (info_line <= 0 || info_col <= 0) {
      return true;
    }

    const std::string info_filename = info->getFilename();
    if (!info_filename.empty() && !body_filename.empty() &&
        info_filename != body_filename &&
        (decl_filename.empty() || info_filename != decl_filename)) {
      continue;
    }

    if (have_decl_start &&
        isBeforeSourcePosition(info_line, info_col, decl_info->get_line(),
                               decl_info->get_col())) {
      continue;
    }

    if (isBeforeSourcePosition(info_line, info_col, body_line, body_col)) {
      return true;
    }
  }

  return false;
}

bool functionDeclRequiresExactRoundtrip(SgFunctionDeclaration *decl) {
  if (decl == NULL) {
    return false;
  }

  if (decl->get_oldStyleDefinition() && !decl->isForward()) {
    return true;
  }

  if (hasUnsafePreprocessingInfo(decl) ||
      functionDefinitionHasUnsafeDeclaratorPreprocessingInfo(decl) ||
      hasUnsafePreprocessingInfo(decl->get_parameterList()) ||
      hasUnsafePreprocessingInfo(decl->get_parameterList_syntax())) {
    return true;
  }

  auto params_require_roundtrip = [](SgFunctionParameterList *params) {
    if (params == NULL) {
      return false;
    }

    for (SgInitializedName *param : params->get_args()) {
      if (hasUnsafePreprocessingInfo(param)) {
        return true;
      }
    }

    return false;
  };

  return params_require_roundtrip(decl->get_parameterList()) ||
         params_require_roundtrip(decl->get_parameterList_syntax());
}

bool fileRequiresExactFunctionDeclaratorRoundtrip(SgGlobal *global_scope) {
  if (global_scope == NULL) {
    return false;
  }

  class FunctionDeclaratorRoundtripTraversal : public AstSimpleProcessing {
  public:
    bool found = false;

    void visit(SgNode *node) override {
      if (found) {
        return;
      }

      if (functionDeclRequiresExactRoundtrip(isSgFunctionDeclaration(node))) {
        found = true;
      }
    }
  };

  FunctionDeclaratorRoundtripTraversal traversal;
  traversal.traverse(global_scope, preorder);
  return traversal.found;
}

// Clang rejects `_Alignas/alignas` when it appears after a record
// definition, e.g. `struct S { ... } _Alignas(16) x;`, even though this form
// is accepted by GCC and heavily used in legacy ROSE tests. Normalize those
// declarations by moving the alignment specifier sequence in front of the
// record keyword before parsing.
std::unique_ptr<llvm::MemoryBuffer>
maybeNormalizePostRecordAlignas(clang::SourceManager &source_manager,
                                clang::FileID file_id,
                                const clang::LangOptions &lang_opts) {
  auto buffer_or = source_manager.getBufferOrNone(file_id);
  if (!buffer_or) {
    return nullptr;
  }

  clang::Lexer lexer(file_id, *buffer_or, source_manager, lang_opts);
  std::vector<TokenWithOffset> tokens;
  tokens.reserve(256);

  clang::Token token;
  while (true) {
    lexer.LexFromRawLexer(token);
    if (token.is(clang::tok::eof)) {
      break;
    }
    if (source_manager.getFileID(token.getLocation()) != file_id) {
      continue;
    }
    if (token.is(clang::tok::unknown)) {
      std::string spelling =
          clang::Lexer::getSpelling(token, source_manager, lang_opts);
      if (!spelling.empty() &&
          std::all_of(spelling.begin(), spelling.end(),
                      [](unsigned char c) { return std::isspace(c) != 0; })) {
        // Raw lexer exposes whitespace as tok::unknown. Ignore pure whitespace
        // so alignment-specifier detection is not blocked by spacing/newlines.
        continue;
      }
    }
    tokens.push_back(
        {token, source_manager.getFileOffset(token.getLocation())});
  }

  if (tokens.empty()) {
    return nullptr;
  }

  llvm::StringRef source_text = buffer_or->getBuffer();
  auto token_spelling = [&](const clang::Token &tok) {
    return clang::Lexer::getSpelling(tok, source_manager, lang_opts);
  };
  auto token_is_spelling = [&](const clang::Token &tok, llvm::StringRef text) {
    return tok.isOneOf(clang::tok::identifier, clang::tok::raw_identifier) &&
           token_spelling(tok) == text;
  };
  auto next_significant = [&](size_t index) {
    while (index < tokens.size() &&
           tokens[index].token.is(clang::tok::comment)) {
      ++index;
    }
    return index;
  };
  auto is_record_keyword = [&](size_t index) {
    if (index >= tokens.size()) {
      return false;
    }
    const clang::Token &tok = tokens[index].token;
    return tok.isOneOf(clang::tok::kw_struct, clang::tok::kw_union,
                       clang::tok::kw_enum) ||
           token_is_spelling(tok, "struct") ||
           token_is_spelling(tok, "union") || token_is_spelling(tok, "enum");
  };
  auto is_alignas_start = [&](size_t index) {
    if (index >= tokens.size()) {
      return false;
    }
    const clang::Token &tok = tokens[index].token;
    if (tok.is(clang::tok::kw__Alignas)) {
      return true;
    }
    return token_is_spelling(tok, "alignas") ||
           token_is_spelling(tok, "_Alignas");
  };
  auto token_end_offset = [&](size_t index) {
    return tokens[index].offset + tokens[index].token.getLength();
  };

  struct AlignasRewrite {
    unsigned insert_offset;
    unsigned remove_begin;
    unsigned remove_end;
    std::string moved_text;
  };
  std::vector<AlignasRewrite> rewrites;
  rewrites.reserve(4);

  for (size_t i = 0; i < tokens.size(); ++i) {
    if (!is_record_keyword(i)) {
      continue;
    }

    size_t l_brace = tokens.size();
    int paren_depth = 0;
    for (size_t j = i + 1; j < tokens.size(); ++j) {
      const clang::Token &candidate = tokens[j].token;
      if (candidate.is(clang::tok::comment)) {
        continue;
      }
      if (candidate.is(clang::tok::l_paren)) {
        ++paren_depth;
        continue;
      }
      if (candidate.is(clang::tok::r_paren)) {
        if (paren_depth > 0) {
          --paren_depth;
        }
        continue;
      }
      if (paren_depth > 0) {
        continue;
      }
      if (candidate.is(clang::tok::l_brace)) {
        l_brace = j;
        break;
      }
      if (candidate.isOneOf(clang::tok::semi, clang::tok::comma,
                            clang::tok::equal, clang::tok::colon)) {
        break;
      }
    }
    if (l_brace == tokens.size()) {
      continue;
    }

    int brace_depth = 1;
    size_t r_brace = tokens.size();
    for (size_t j = l_brace + 1; j < tokens.size(); ++j) {
      const clang::Token &candidate = tokens[j].token;
      if (candidate.is(clang::tok::comment)) {
        continue;
      }
      if (candidate.is(clang::tok::l_brace)) {
        ++brace_depth;
      } else if (candidate.is(clang::tok::r_brace)) {
        --brace_depth;
        if (brace_depth == 0) {
          r_brace = j;
          break;
        }
      }
    }
    if (r_brace == tokens.size()) {
      continue;
    }

    size_t align_start = next_significant(r_brace + 1);
    if (!is_alignas_start(align_start)) {
      continue;
    }

    size_t scan = align_start;
    size_t align_end = tokens.size();
    bool parse_ok = true;
    while (scan < tokens.size() && is_alignas_start(scan)) {
      size_t l_paren = next_significant(scan + 1);
      if (l_paren >= tokens.size() ||
          !tokens[l_paren].token.is(clang::tok::l_paren)) {
        parse_ok = false;
        break;
      }
      int align_paren_depth = 1;
      size_t r_paren = tokens.size();
      for (size_t cursor = l_paren + 1; cursor < tokens.size(); ++cursor) {
        const clang::Token &paren_tok = tokens[cursor].token;
        if (paren_tok.is(clang::tok::comment)) {
          continue;
        }
        if (paren_tok.is(clang::tok::l_paren)) {
          ++align_paren_depth;
        } else if (paren_tok.is(clang::tok::r_paren)) {
          --align_paren_depth;
          if (align_paren_depth == 0) {
            r_paren = cursor;
            break;
          }
        }
      }
      if (r_paren == tokens.size()) {
        parse_ok = false;
        break;
      }
      align_end = r_paren;
      scan = next_significant(r_paren + 1);
    }
    if (!parse_ok || align_end == tokens.size()) {
      continue;
    }

    if (scan >= tokens.size()) {
      continue;
    }
    const clang::Token &after_align = tokens[scan].token;
    if (!after_align.isOneOf(clang::tok::identifier, clang::tok::raw_identifier,
                             clang::tok::semi, clang::tok::star,
                             clang::tok::l_paren, clang::tok::l_square,
                             clang::tok::kw___attribute) &&
        !token_is_spelling(after_align, "__attribute__")) {
      continue;
    }

    unsigned remove_begin = tokens[align_start].offset;
    unsigned remove_end = token_end_offset(align_end);
    rewrites.push_back(
        {tokens[i].offset, remove_begin, remove_end,
         source_text.substr(remove_begin, remove_end - remove_begin).str()});
  }

  if (rewrites.empty()) {
    return nullptr;
  }

  struct EraseRange {
    unsigned begin;
    unsigned end;
  };
  std::vector<EraseRange> erase_ranges;
  erase_ranges.reserve(rewrites.size());
  std::vector<std::pair<unsigned, std::string>> insertions;
  insertions.reserve(rewrites.size());
  size_t inserted_bytes = 0;
  for (const AlignasRewrite &rewrite : rewrites) {
    erase_ranges.push_back({rewrite.remove_begin, rewrite.remove_end});
    insertions.emplace_back(rewrite.insert_offset, rewrite.moved_text + " ");
    inserted_bytes += insertions.back().second.size();
  }

  std::sort(erase_ranges.begin(), erase_ranges.end(),
            [](const EraseRange &lhs, const EraseRange &rhs) {
              return lhs.begin < rhs.begin;
            });
  std::stable_sort(
      insertions.begin(), insertions.end(),
      [](const auto &lhs, const auto &rhs) { return lhs.first < rhs.first; });

  unsigned previous_end = 0;
  for (size_t idx = 0; idx < erase_ranges.size(); ++idx) {
    const EraseRange &range = erase_ranges[idx];
    if (range.begin > range.end) {
      return nullptr;
    }
    if (idx != 0 && range.begin < previous_end) {
      return nullptr;
    }
    previous_end = range.end;
  }

  std::string updated;
  updated.reserve(source_text.size() + inserted_bytes);
  size_t cursor = 0;
  size_t erase_idx = 0;
  size_t insert_idx = 0;
  auto append_insertions_at = [&](size_t offset) {
    while (insert_idx < insertions.size() &&
           static_cast<size_t>(insertions[insert_idx].first) == offset) {
      updated += insertions[insert_idx].second;
      ++insert_idx;
    }
  };

  while (cursor < source_text.size()) {
    append_insertions_at(cursor);
    if (erase_idx < erase_ranges.size() &&
        cursor == static_cast<size_t>(erase_ranges[erase_idx].begin)) {
      cursor = erase_ranges[erase_idx].end;
      ++erase_idx;
      continue;
    }
    updated.push_back(source_text[cursor]);
    ++cursor;
  }

  append_insertions_at(source_text.size());
  while (insert_idx < insertions.size()) {
    if (insertions[insert_idx].first <= source_text.size()) {
      updated += insertions[insert_idx].second;
    }
    ++insert_idx;
  }

  return llvm::MemoryBuffer::getMemBufferCopy(updated,
                                              buffer_or->getBufferIdentifier());
}

struct SuppressedIncludeDirective {
  size_t hash_offset;
  size_t line_start;
  size_t line_end;
  PreprocessingInfo::DirectiveType directive_type;
  std::string text;
};

static bool isDirectiveLine(llvm::StringRef line, size_t *hash_col) {
  size_t pos = 0;
  while (pos < line.size() &&
         (line[pos] == ' ' || line[pos] == '\t' || line[pos] == '\f')) {
    ++pos;
  }
  if (pos >= line.size() || line[pos] != '#') {
    return false;
  }
  if (hash_col != nullptr) {
    *hash_col = pos;
  }
  return true;
}

static bool parseIncludeDirective(llvm::StringRef line, size_t hash_col,
                                  PreprocessingInfo::DirectiveType *type_out,
                                  std::string *text_out) {
  size_t pos = hash_col + 1;
  while (pos < line.size() &&
         (line[pos] == ' ' || line[pos] == '\t' || line[pos] == '\f')) {
    ++pos;
  }
  auto is_word_boundary = [&](size_t idx) -> bool {
    if (idx >= line.size()) {
      return true;
    }
    char c = line[idx];
    return c == ' ' || c == '\t' || c == '\f' || c == '<' || c == '"' ||
           c == '\r';
  };
  PreprocessingInfo::DirectiveType directive_type =
      PreprocessingInfo::CpreprocessorIncludeDeclaration;
  if (line.substr(pos).starts_with("include_next")) {
    if (!is_word_boundary(pos + strlen("include_next"))) {
      return false;
    }
    directive_type = PreprocessingInfo::CpreprocessorIncludeNextDeclaration;
  } else if (line.substr(pos).starts_with("include")) {
    if (!is_word_boundary(pos + strlen("include"))) {
      return false;
    }
    directive_type = PreprocessingInfo::CpreprocessorIncludeDeclaration;
  } else {
    return false;
  }
  if (type_out != nullptr) {
    *type_out = directive_type;
  }
  if (text_out != nullptr) {
    std::string text = line.str();
    while (!text.empty() && (text.back() == ' ' || text.back() == '\t' ||
                             text.back() == '\f' || text.back() == '\r')) {
      text.pop_back();
    }
    *text_out = std::move(text);
  }
  return true;
}

static bool containsIdentifierReference(llvm::StringRef text,
                                        llvm::StringRef identifier) {
  if (identifier.empty()) {
    return false;
  }

  auto is_identifier_char = [](char c) {
    return (c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') ||
           (c >= '0' && c <= '9') || c == '_';
  };

  size_t offset = 0;
  while (true) {
    offset = text.find(identifier, offset);
    if (offset == llvm::StringRef::npos) {
      return false;
    }

    const bool boundary_before =
        offset == 0 || !is_identifier_char(text[offset - 1]);
    const size_t end_offset = offset + identifier.size();
    const bool boundary_after =
        end_offset >= text.size() || !is_identifier_char(text[end_offset]);
    if (boundary_before && boundary_after) {
      return true;
    }

    offset = end_offset;
  }
}

static bool sourceFileContainsBackendConditional(llvm::StringRef input_file) {
  llvm::ErrorOr<std::unique_ptr<llvm::MemoryBuffer>> buffer_or =
      llvm::MemoryBuffer::getFile(input_file);
  if (!buffer_or) {
    return false;
  }

  llvm::StringRef source_text = (*buffer_or)->getBuffer();
  size_t line_start = 0;
  while (line_start < source_text.size()) {
    size_t line_end = source_text.find_first_of("\r\n", line_start);
    if (line_end == llvm::StringRef::npos) {
      line_end = source_text.size();
    }

    llvm::StringRef line = source_text.slice(line_start, line_end);
    size_t hash_col = 0;
    if (isDirectiveLine(line, &hash_col)) {
      llvm::StringRef directive = line.drop_front(hash_col + 1).ltrim(" \t\f");
      if ((directive.consume_front("if") || directive.consume_front("ifdef") ||
           directive.consume_front("ifndef") ||
           directive.consume_front("elif")) &&
          containsIdentifierReference(directive, "USE_ROSE_BACKEND")) {
        return true;
      }
    }

    line_start = line_end;
    if (line_start < source_text.size()) {
      if (source_text[line_start] == '\r' &&
          line_start + 1 < source_text.size() &&
          source_text[line_start + 1] == '\n') {
        line_start += 2;
      } else {
        ++line_start;
      }
    }
  }

  return false;
}

static void skipDirectiveWhitespace(llvm::StringRef line, size_t *pos) {
  while (*pos < line.size() && (line[*pos] == ' ' || line[*pos] == '\t' ||
                                line[*pos] == '\f' || line[*pos] == '\r')) {
    ++(*pos);
  }
}

static bool consumeDirectiveWord(llvm::StringRef line, size_t *pos,
                                 llvm::StringRef word) {
  skipDirectiveWhitespace(line, pos);
  if (!line.substr(*pos).starts_with(word)) {
    return false;
  }
  const size_t end = *pos + word.size();
  if (end < line.size()) {
    char boundary = line[end];
    if (boundary != ' ' && boundary != '\t' && boundary != '\f' &&
        boundary != '\r') {
      return false;
    }
  }
  *pos = end;
  return true;
}

static bool parseOpenMPDeclareVariantBoundary(llvm::StringRef line,
                                              bool *is_begin, bool *is_end) {
  size_t hash_col = 0;
  if (!isDirectiveLine(line, &hash_col)) {
    return false;
  }
  size_t pos = hash_col + 1;
  if (!consumeDirectiveWord(line, &pos, "pragma")) {
    return false;
  }
  if (!consumeDirectiveWord(line, &pos, "omp")) {
    return false;
  }

  bool begin = false;
  bool end = false;
  if (consumeDirectiveWord(line, &pos, "begin")) {
    begin = true;
  } else if (consumeDirectiveWord(line, &pos, "end")) {
    end = true;
  } else {
    return false;
  }

  if (!consumeDirectiveWord(line, &pos, "declare")) {
    return false;
  }
  if (!consumeDirectiveWord(line, &pos, "variant")) {
    return false;
  }

  if (is_begin != nullptr) {
    *is_begin = begin;
  }
  if (is_end != nullptr) {
    *is_end = end;
  }
  return true;
}

struct OpenMPDeclareVariantRegionCapture {
  size_t body_begin_offset = 0;
  size_t end_directive_offset = 0;
  unsigned begin_line = 0;
  unsigned end_line = 0;
};

struct OpenMPDeclareVariantRewriteResult {
  std::unique_ptr<llvm::MemoryBuffer> rewritten_buffer;
  std::vector<OmpDeclareVariantRegionInfo> regions;
};

static bool lineEndsWithDirectiveContinuation(llvm::StringRef line) {
  while (!line.empty() && (line.back() == ' ' || line.back() == '\t' ||
                           line.back() == '\f' || line.back() == '\r')) {
    line = line.drop_back();
  }
  return !line.empty() && line.back() == '\\';
}

static bool collectLogicalDirectiveText(llvm::StringRef content,
                                        size_t line_start, size_t line_end,
                                        size_t *after_directive_out,
                                        unsigned *line_count_out,
                                        std::string *directive_text_out) {
  if (after_directive_out == nullptr || line_count_out == nullptr ||
      directive_text_out == nullptr) {
    return false;
  }

  llvm::StringRef line = content.slice(line_start, line_end);
  size_t hash_col = 0;
  if (!isDirectiveLine(line, &hash_col)) {
    return false;
  }

  directive_text_out->clear();
  size_t current_line_start = line_start;
  size_t current_line_end = line_end;
  unsigned consumed_lines = 0;

  while (true) {
    llvm::StringRef current_line =
        content.slice(current_line_start, current_line_end);
    size_t effective_end = current_line.size();
    bool has_continuation = lineEndsWithDirectiveContinuation(current_line);
    if (has_continuation) {
      while (effective_end > 0 && (current_line[effective_end - 1] == ' ' ||
                                   current_line[effective_end - 1] == '\t' ||
                                   current_line[effective_end - 1] == '\f' ||
                                   current_line[effective_end - 1] == '\r')) {
        --effective_end;
      }
      if (effective_end > 0 && current_line[effective_end - 1] == '\\') {
        --effective_end;
      }
      while (effective_end > 0 && (current_line[effective_end - 1] == ' ' ||
                                   current_line[effective_end - 1] == '\t' ||
                                   current_line[effective_end - 1] == '\f' ||
                                   current_line[effective_end - 1] == '\r')) {
        --effective_end;
      }
    } else {
      while (effective_end > 0 && (current_line[effective_end - 1] == ' ' ||
                                   current_line[effective_end - 1] == '\t' ||
                                   current_line[effective_end - 1] == '\f' ||
                                   current_line[effective_end - 1] == '\r')) {
        --effective_end;
      }
    }

    if (!directive_text_out->empty()) {
      directive_text_out->push_back(' ');
    }
    directive_text_out->append(current_line.substr(0, effective_end).str());
    ++consumed_lines;

    size_t next_pos = current_line_end;
    if (next_pos >= content.size()) {
      *after_directive_out = next_pos;
      *line_count_out = consumed_lines;
      return true;
    }
    if (content[next_pos] == '\r' && next_pos + 1 < content.size() &&
        content[next_pos + 1] == '\n') {
      next_pos += 2;
    } else {
      next_pos += 1;
    }

    if (!has_continuation) {
      *after_directive_out = next_pos;
      *line_count_out = consumed_lines;
      return true;
    }

    current_line_start = next_pos;
    current_line_end = content.find_first_of("\r\n", current_line_start);
    if (current_line_end == llvm::StringRef::npos) {
      current_line_end = content.size();
    }
  }
}

OpenMPDeclareVariantRewriteResult
captureAndElideOpenMPDeclareVariantRegions(clang::SourceManager &source_manager,
                                           clang::FileID file_id) {
  OpenMPDeclareVariantRewriteResult result;
  auto buffer_or = source_manager.getBufferOrNone(file_id);
  if (!buffer_or) {
    return result;
  }

  llvm::StringRef content = buffer_or->getBuffer();
  if (!content.contains("declare variant")) {
    return result;
  }

  std::vector<OpenMPDeclareVariantRegionCapture> captures;
  captures.reserve(8);
  int variant_depth = 0;
  OpenMPDeclareVariantRegionCapture current_region;
  size_t pos = 0;
  unsigned line_number = 1;
  while (pos < content.size()) {
    size_t line_end = content.find_first_of("\r\n", pos);
    if (line_end == llvm::StringRef::npos) {
      line_end = content.size();
    }

    std::string directive_text;
    size_t after_directive = pos;
    unsigned consumed_lines = 1;
    bool is_begin = false;
    bool is_end = false;
    if (collectLogicalDirectiveText(content, pos, line_end, &after_directive,
                                    &consumed_lines, &directive_text) &&
        parseOpenMPDeclareVariantBoundary(directive_text, &is_begin, &is_end)) {
      if (is_begin) {
        if (variant_depth == 0) {
          current_region.body_begin_offset = after_directive;
          current_region.begin_line = line_number;
        }
        ++variant_depth;
      } else if (variant_depth > 0) {
        --variant_depth;
        if (variant_depth == 0) {
          current_region.end_directive_offset = pos;
          current_region.end_line = line_number;
          captures.push_back(current_region);
          OmpDeclareVariantRegionInfo region;
          region.begin_line = current_region.begin_line;
          region.end_line = current_region.end_line;
          if (current_region.body_begin_offset < pos) {
            region.captured_region =
                content
                    .substr(current_region.body_begin_offset,
                            pos - current_region.body_begin_offset)
                    .str();
          }
          result.regions.push_back(std::move(region));
        }
      }

      pos = after_directive;
      line_number += consumed_lines;
      continue;
    }

    if (line_end < content.size()) {
      if (content[line_end] == '\r' && line_end + 1 < content.size() &&
          content[line_end + 1] == '\n') {
        pos = line_end + 2;
      } else {
        pos = line_end + 1;
      }
      ++line_number;
    } else {
      pos = line_end;
    }
  }

  if (result.regions.empty()) {
    return result;
  }

  std::string updated = content.str();
  bool modified = false;
  for (const OpenMPDeclareVariantRegionCapture &capture : captures) {
    if (capture.body_begin_offset >= capture.end_directive_offset) {
      continue;
    }
    modified = true;
    for (size_t offset = capture.body_begin_offset;
         offset < capture.end_directive_offset; ++offset) {
      if (updated[offset] != '\n' && updated[offset] != '\r') {
        updated[offset] = ' ';
      }
    }
  }

  if (modified) {
    result.rewritten_buffer = llvm::MemoryBuffer::getMemBufferCopy(
        updated, buffer_or->getBufferIdentifier());
  }
  return result;
}

std::unique_ptr<llvm::MemoryBuffer> maybeSuppressMisplacedIncludes(
    clang::SourceManager &source_manager, clang::FileID file_id,
    std::vector<SuppressedIncludeDirective> *suppressed_out) {
  if (suppressed_out == nullptr) {
    return nullptr;
  }
  auto buffer_or = source_manager.getBufferOrNone(file_id);
  if (!buffer_or) {
    return nullptr;
  }
  llvm::StringRef content = buffer_or->getBuffer();
  if (content.empty()) {
    return nullptr;
  }

  bool pending_body = false;
  std::vector<SuppressedIncludeDirective> pending_includes;
  std::vector<SuppressedIncludeDirective> suppressed;
  pending_includes.reserve(4);

  bool in_block_comment = false;
  bool in_line_comment = false;
  bool in_string = false;
  bool in_char = false;
  bool escape = false;
  int brace_depth = 0;
  int paren_depth = 0;

  auto commit_pending = [&]() {
    if (!pending_includes.empty()) {
      suppressed.insert(suppressed.end(), pending_includes.begin(),
                        pending_includes.end());
      pending_includes.clear();
    }
    pending_body = false;
  };

  size_t pos = 0;
  while (pos < content.size()) {
    size_t line_end = content.find_first_of("\r\n", pos);
    if (line_end == llvm::StringRef::npos) {
      line_end = content.size();
    }
    size_t next_pos = line_end;
    if (next_pos < content.size()) {
      if (content[next_pos] == '\r' && next_pos + 1 < content.size() &&
          content[next_pos + 1] == '\n') {
        next_pos += 2;
      } else {
        next_pos += 1;
      }
    }

    llvm::StringRef line = content.slice(pos, line_end);
    size_t hash_col = 0;
    bool is_directive = isDirectiveLine(line, &hash_col);
    if (is_directive) {
      PreprocessingInfo::DirectiveType directive_type;
      std::string directive_text;
      if (parseIncludeDirective(line, hash_col, &directive_type,
                                &directive_text) &&
          pending_body) {
        SuppressedIncludeDirective entry;
        entry.hash_offset = pos + hash_col;
        entry.line_start = pos;
        entry.line_end = line_end;
        entry.directive_type = directive_type;
        entry.text = std::move(directive_text);
        pending_includes.push_back(std::move(entry));
      }
      in_line_comment = false;
      pos = next_pos;
      continue;
    }

    for (size_t i = pos; i < line_end; ++i) {
      char c = content[i];
      if (in_line_comment) {
        continue;
      }
      if (in_block_comment) {
        if (c == '*' && i + 1 < line_end && content[i + 1] == '/') {
          in_block_comment = false;
          ++i;
        }
        continue;
      }
      if (in_string) {
        if (escape) {
          escape = false;
        } else if (c == '\\') {
          escape = true;
        } else if (c == '"') {
          in_string = false;
        }
        continue;
      }
      if (in_char) {
        if (escape) {
          escape = false;
        } else if (c == '\\') {
          escape = true;
        } else if (c == '\'') {
          in_char = false;
        }
        continue;
      }

      if (c == '/' && i + 1 < line_end && content[i + 1] == '/') {
        in_line_comment = true;
        continue;
      }
      if (c == '/' && i + 1 < line_end && content[i + 1] == '*') {
        in_block_comment = true;
        ++i;
        continue;
      }
      if (c == '"') {
        in_string = true;
        escape = false;
        continue;
      }
      if (c == '\'') {
        in_char = true;
        escape = false;
        continue;
      }
      if (c == '(') {
        ++paren_depth;
        continue;
      }
      if (c == ')') {
        if (paren_depth > 0) {
          --paren_depth;
        }
        if (paren_depth == 0 && brace_depth == 0) {
          pending_body = true;
          pending_includes.clear();
        }
        continue;
      }
      if (c == '{') {
        if (brace_depth == 0 && pending_body) {
          commit_pending();
        }
        ++brace_depth;
        continue;
      }
      if (c == '}') {
        if (brace_depth > 0) {
          --brace_depth;
        }
        continue;
      }
      if (c == ';') {
        if (brace_depth == 0 && pending_body) {
          pending_body = false;
          pending_includes.clear();
        }
        continue;
      }
    }
    in_line_comment = false;
    pos = next_pos;
  }

  if (suppressed.empty()) {
    return nullptr;
  }
  *suppressed_out = suppressed;

  std::string updated = content.str();
  for (const auto &entry : suppressed) {
    for (size_t i = entry.line_start; i < entry.line_end; ++i) {
      char &ch = updated[i];
      if (ch != '\n' && ch != '\r') {
        ch = ' ';
      }
    }
  }

  return llvm::MemoryBuffer::getMemBufferCopy(updated,
                                              buffer_or->getBufferIdentifier());
}

} // namespace

// DQ (11/28/2020): Use this for testing the DOT graph generator.
#define EXIT_AFTER_BUILDING_DOT_FILE 0

int clang_main(int argc, char **argv, SgSourceFile &sageFile,
               const char *driver_argv0) {

  // Let individual template declarations opt into AST unparsing when source
  // coverage is unavailable. Source-backed primary templates in the user file
  // should continue to come from the original source so conditional regions do
  // not duplicate their declarations.
  sageFile.set_unparse_template_ast(false);
  // printf ("sageFile.get_clang_il_to_graphviz() = %s
  // \n",sageFile.get_clang_il_to_graphviz() ? "true" : "false");

  // DQ (11/27/2020): Use the -rose:clang_il_to_graphviz option to comntrol the
  // use of the Clang Dot generator.
#if EXIT_AFTER_BUILDING_DOT_FILE
  if (true)
#else
  if (sageFile.get_clang_il_to_graphviz() == true)
#endif
  {
    // DQ (10/23/2020): Calling clang-to-dot generator (I don't think this
    // modifies the argv list).
    int clang_to_dot_status = clang_to_dot_main(argc, argv, driver_argv0);
    if (clang_to_dot_status != 0) {
      printf("Error in generation of dot file of Clang IR: returing from top "
             "of clang_main(): clang_to_dot_status = %d \n",
             clang_to_dot_status);
      return clang_to_dot_status;
    } else {
    }

#if EXIT_AFTER_BUILDING_DOT_FILE
    return 0;
#endif
  }

  // 0 - Analyse Cmd Line

  std::vector<std::string> sys_dirs_list;
  std::vector<std::string> inc_dirs_list;
  std::vector<std::string> define_list;
  std::vector<std::string> openmp_define_list;
  std::vector<std::string> inc_list;
  std::string input_file;
  std::vector<std::string> passthrough_args;
  bool enable_openmp = false;
  bool enable_openmp_simd = false;
  bool disable_openmp_via_flag = false;
  bool continue_on_error = false;
  bool disable_access_control = false;
  bool delayed_template_parsing_compat = false;
  bool respect_rtti_flags = false;
  enum class ExceptionMode { Unspecified, Enabled, Disabled };
  ExceptionMode exception_mode = ExceptionMode::Unspecified;
  bool saw_explicit_rtti_flag = false;
  bool explicit_rtti_enabled = true;

  for (int i = 0; i < argc; i++) {
    std::string current_arg(argv[i]);
    Rose::Cmdline::IncludeOptionParseResult include_parse_result =
        Rose::Cmdline::IncludeOptionParseResult::NotIncludeOption;
    if (isRoseInternalOption(current_arg)) {
      // ROSE-only options are consumed by ROSE command-line processing and
      // must not be forwarded to Clang.
      const int trailing_arg_count =
          countRoseOptionTrailingArguments(current_arg);
      for (int consumed = 0; consumed < trailing_arg_count && i + 1 < argc;
           ++consumed) {
        ++i;
      }
      continue;
    }

    if (current_arg.find("-I") == 0) {
      if (current_arg.length() > 2) {
        inc_dirs_list.push_back(current_arg.substr(2));
      } else {
        i++;
        if (i < argc)
          inc_dirs_list.push_back(current_arg);
        else
          break;
      }
    } else if (current_arg == "-isystem") {
      ++i;
      if (i < argc) {
        sys_dirs_list.push_back(argv[i]);
      } else {
        break;
      }
    } else if (current_arg.rfind("-isystem", 0) == 0) {
      if (current_arg.size() > 8 && current_arg[8] != '-') {
        sys_dirs_list.push_back(current_arg.substr(8));
      }
    } else if (current_arg.find("-D") == 0) {
      std::string define_value;
      if (current_arg.length() > 2) {
        define_value = current_arg.substr(2);
      } else {
        ++i;
        if (i < argc)
          define_value = argv[i];
        else
          break;
      }
      const bool is_openmp_define = (define_value == "_OPENMP") ||
                                    (define_value.rfind("_OPENMP=", 0) == 0);
      if (is_openmp_define)
        openmp_define_list.push_back(define_value);
      else
        define_list.push_back(define_value);
    }
    // Note: -fopenmp is processed by ROSE's command line processor before
    // reaching here ROSE sets sageFile.set_openmp(true) and related flags
    // automatically We don't need to parse it again here
    else if (current_arg.find("-c") == 0) {
    } else if (current_arg == "-std") {
      passthrough_args.push_back(current_arg);
      ++i;
      if (i < argc)
        passthrough_args.push_back(argv[i]);
      else
        break;
    } else if ((include_parse_result =
                    Rose::Cmdline::normalizeAndAppendIncludeOption(
                        current_arg, i, argc,
                        [argv](int arg_index) {
                          return std::string(argv[arg_index]);
                        },
                        passthrough_args)) !=
               Rose::Cmdline::IncludeOptionParseResult::NotIncludeOption) {
      if (include_parse_result ==
          Rose::Cmdline::IncludeOptionParseResult::MissingArgument) {
        break;
      }
    } else if (current_arg.rfind("-std=", 0) == 0) {
      passthrough_args.push_back(current_arg);
    } else if (current_arg.find("-o") == 0) {
      if (current_arg.length() == 2) {
        i++;
        if (i >= argc)
          break;
      }
    } else if (current_arg.rfind("-fopenmp", 0) == 0) {
      // Don't pass -fopenmp to Clang - REX captures OpenMP pragmas as plain
      // text
      bool explicitly_disabled = false;
      if (current_arg.size() > 8 && current_arg[8] == '=') {
        std::string value = current_arg.substr(9);
        std::string lower_value = value;
        std::transform(
            lower_value.begin(), lower_value.end(), lower_value.begin(),
            [](unsigned char c) { return static_cast<char>(std::tolower(c)); });
        if (lower_value == "0" || lower_value == "false" ||
            lower_value == "disabled") {
          explicitly_disabled = true;
        }
      }

      if (explicitly_disabled) {
        disable_openmp_via_flag = true;
        enable_openmp = false;
      } else if (!disable_openmp_via_flag) {
        enable_openmp = true;
      }
    } else if (current_arg == "-fopenmp-simd") {
      enable_openmp_simd = true;
      enable_openmp =
          true; // SIMD is a subset of OpenMP, enable full pragma capture
    } else if (current_arg == "-fexceptions" ||
               current_arg == "-fcxx-exceptions") {
      exception_mode = ExceptionMode::Enabled;
      passthrough_args.push_back(current_arg);
    } else if (current_arg == "-fno-exceptions" ||
               current_arg == "-fno-cxx-exceptions") {
      // Treat as backend-only: the Clang frontend must see exceptions enabled
      // to build a complete C++ AST, and -fno-exceptions is not a cc1 flag.
    } else if (current_arg == "-frtti") {
      saw_explicit_rtti_flag = true;
      explicit_rtti_enabled = true;
    } else if (current_arg == "-fno-rtti") {
      saw_explicit_rtti_flag = true;
      explicit_rtti_enabled = false;
      // By default this is backend-only: disabling RTTI in the frontend breaks
      // C++ AST features. Use -rex:clang:respect-rtti-flags to opt in.
    } else if (current_arg == "-rex:clang:continue-on-error") {
      continue_on_error = true;
    } else if (current_arg == "-rex:clang:disable-access-control") {
      disable_access_control = true;
    } else if (current_arg == "-rex:clang:delayed-template-parsing") {
      delayed_template_parsing_compat = true;
    } else if (current_arg == "-rex:clang:respect-rtti-flags") {
      respect_rtti_flags = true;
    } else if (!current_arg.empty() && current_arg[0] == '-') {
      // TODO -include
#if DEBUG_ARGS
      std::cerr << "argv[" << i << "] = " << current_arg
                << " preserved as passthrough option." << std::endl;
#endif
      passthrough_args.push_back(current_arg);
    } else {
#if DEBUG_ARGS
      std::cerr << "argv[" << i << "] = " << current_arg
                << " is neither define or include dir. Use it as input file."
                << std::endl;
#endif
      input_file = current_arg;
    }
  }

  // Detect if this is a secondary parse during lowering/outlining
  // Check if this file is being re-parsed (project already has a file with same
  // source)
  const bool openmp_ast_mode =
      (sageFile.get_openmp() &&
       (sageFile.get_openmp_ast_only() || sageFile.get_openmp_parse_only())) ||
      (sageFile.get_openacc() &&
       (sageFile.get_openacc_ast_only() || sageFile.get_openacc_parse_only()));
  bool is_secondary_parse = false;
  if (sageFile.get_parent() != NULL) {
    SgProject *project = isSgProject(sageFile.get_parent()->get_parent());
    if (project != NULL) {
      // Check if project already has a file with the same source filename
      // This indicates we're in a secondary parse (e.g., during
      // outlining/lowering)
      SgFilePtrList &file_list = project->get_fileList();

      // Normalize input_file to absolute path for comparison
      std::unique_ptr<char, decltype(&free)> abs_input(
          realpath(input_file.c_str(), NULL), &free);
      std::string normalized_input =
          abs_input ? std::string(abs_input.get()) : input_file;

      for (SgFilePtrList::iterator it = file_list.begin();
           it != file_list.end(); ++it) {
        SgSourceFile *existing_file = isSgSourceFile(*it);
        if (existing_file != NULL && existing_file != &sageFile) {
          std::string existing_filename =
              existing_file->get_sourceFileNameWithPath();

          // Normalize existing filename to absolute path
          std::unique_ptr<char, decltype(&free)> abs_existing(
              realpath(existing_filename.c_str(), NULL), &free);
          std::string normalized_existing =
              abs_existing ? std::string(abs_existing.get())
                           : existing_filename;

          // Use exact path comparison only (no substring matching)
          if (normalized_existing == normalized_input) {
            is_secondary_parse = true;
            break;
          }
        }
      }
    }
  }

  // Enable pragma capture if ROSE has OpenMP enabled (don't pass flag to Clang)
  if (sageFile.get_openmp() && !enable_openmp && !disable_openmp_via_flag &&
      !is_secondary_parse) {
    enable_openmp = true;
  }

  if (sageFile.get_openmp_parse_only() && !enable_openmp_simd &&
      !is_secondary_parse) {
    enable_openmp_simd = true;
    enable_openmp = true; // SIMD requires pragma capture
  }

  // Preserve explicit user-provided _OPENMP defines.
  // Do not synthesize _OPENMP automatically in pragma-driven OpenMP mode:
  // that mode intentionally avoids -fopenmp, and forcing _OPENMP can expose
  // header branches that require compiler-side OpenMP semantic handling.
  for (const auto &define_value : openmp_define_list) {
    if (define_value.empty()) {
      continue;
    }
    if (std::find(define_list.begin(), define_list.end(), define_value) ==
        define_list.end()) {
      define_list.push_back(define_value);
    }
  }

  ClangToSageTranslator::Language language = ClangToSageTranslator::unknown;

  size_t last_period = input_file.find_last_of(".");
  std::string extention(input_file.substr(last_period + 1));

  if (extention == "c") {
    language = ClangToSageTranslator::C;
  } else if (extention == "C" || extention == "cxx" || extention == "cpp" ||
             extention == "cc") {
    language = ClangToSageTranslator::CPLUSPLUS;
  } else if (extention == "objc") {
    language = ClangToSageTranslator::OBJC;
  } else if (extention == "cu") {
    language = ClangToSageTranslator::CUDA;
  } else if (extention == "ocl" || extention == "cl") {
    language = ClangToSageTranslator::OPENCL;
  }

  if (language == ClangToSageTranslator::C && sageFile.get_Cxx_only()) {
    language = ClangToSageTranslator::CPLUSPLUS;
  } else if (language == ClangToSageTranslator::CPLUSPLUS &&
             sageFile.get_C_only()) {
    language = ClangToSageTranslator::C;
  }

  ROSE_ASSERT(language != ClangToSageTranslator::unknown);

  std::string language_arg;
  switch (language) {
  case ClangToSageTranslator::C:
    language_arg = "-xc";
    break;
  case ClangToSageTranslator::CPLUSPLUS:
    language_arg = "-xc++";
    break;
  case ClangToSageTranslator::CUDA:
    language_arg = "-xcuda";
    break;
  case ClangToSageTranslator::OPENCL:
    language_arg = "-xcl";
    break;
  case ClangToSageTranslator::OBJC:
    language_arg = "-xobjective-c";
    break;
  default:
    ROSE_ABORT();
  }

  auto is_cxx17_or_later = [](SgFile::standard_enum std) {
    switch (std) {
    case SgFile::e_cxx17_standard:
    case SgFile::e_cxx20_standard:
    case SgFile::e_cxx23_standard:
    case SgFile::e_cxx26_standard:
      return true;
    default:
      return false;
    }
  };
  bool relax_register_diag = false;
  bool relax_dynamic_exception_diag = false;
  auto has_passthrough_flag = [&](const std::string &flag) {
    return std::find(passthrough_args.begin(), passthrough_args.end(), flag) !=
           passthrough_args.end();
  };
  auto add_passthrough_flag_if_missing = [&](const std::string &flag) {
    if (!has_passthrough_flag(flag)) {
      passthrough_args.push_back(flag);
    }
  };
  auto has_passthrough_optimization_flag = [&]() {
    return std::any_of(passthrough_args.begin(), passthrough_args.end(),
                       [](const std::string &arg) {
                         return arg.size() >= 2 && arg[0] == '-' &&
                                arg[1] == 'O';
                       });
  };

  if (language == ClangToSageTranslator::C ||
      language == ClangToSageTranslator::CPLUSPLUS ||
      language == ClangToSageTranslator::CUDA) {
    // Frontend parsing should only see the generic ROSE marker.
    // USE_ROSE_BACKEND is reserved for backend compiler invocations.
    add_passthrough_flag_if_missing("-DUSE_ROSE");
  }

  if ((language == ClangToSageTranslator::CPLUSPLUS ||
       language == ClangToSageTranslator::CUDA) &&
      is_cxx17_or_later(sageFile.get_standard())) {
    // Allow legacy C++ constructs that were removed in C++17 but appear in
    // older ROSE/REX tests.
    if (!has_passthrough_flag("-Werror=register")) {
      add_passthrough_flag_if_missing("-Wno-error=register");
      add_passthrough_flag_if_missing("-Wno-register");
      relax_register_diag = true;
    }
    if (!has_passthrough_flag("-Werror=dynamic-exception-spec")) {
      add_passthrough_flag_if_missing("-Wno-error=dynamic-exception-spec");
      add_passthrough_flag_if_missing("-Wno-dynamic-exception-spec");
      relax_dynamic_exception_diag = true;
    }
  }

  if (exception_mode == ExceptionMode::Unspecified &&
      (language == ClangToSageTranslator::CPLUSPLUS ||
       language == ClangToSageTranslator::CUDA)) {
    exception_mode = ExceptionMode::Enabled;
    passthrough_args.push_back("-fexceptions");
  }

  if (sageFile.get_optimization() && !has_passthrough_optimization_flag()) {
    // ROSE records optimization as a boolean and may strip the original `-O*`
    // option before the Clang frontend sees argv. Synthesize a frontend
    // optimization level so Clang defines `__OPTIMIZE__` and the parsed AST
    // matches the requested mode.
    passthrough_args.push_back("-O1");
  }

  bool frontend_rtti_enabled = true;
  if ((language == ClangToSageTranslator::CPLUSPLUS ||
       language == ClangToSageTranslator::CUDA) &&
      respect_rtti_flags && saw_explicit_rtti_flag) {
    frontend_rtti_enabled = explicit_rtti_enabled;
  }

  if (delayed_template_parsing_compat &&
      (language == ClangToSageTranslator::CPLUSPLUS ||
       language == ClangToSageTranslator::CUDA) &&
      !has_passthrough_flag("-fno-delayed-template-parsing")) {
    // Keep modern Clang behavior by default; enable delayed parsing only when
    // compatibility mode is explicitly requested.
    add_passthrough_flag_if_missing("-fdelayed-template-parsing");
  }

  RoseClangPathRoots clang_paths = resolveRoseClangPaths(driver_argv0);
  std::string builtin_header_root = clang_paths.builtin_header_root;

  sys_dirs_list.push_back(builtin_header_root);

  auto is_staged_clang_resource_dir = [](const std::string &path) -> bool {
    if (path.find("_HEADERS") == std::string::npos ||
        path.find("clang") == std::string::npos) {
      return false;
    }
    std::string candidate = path;
    if (!candidate.empty() && candidate.back() != '/') {
      candidate += '/';
    }
    candidate += "stdint.h";
    std::unique_ptr<char, decltype(&free)> resolved(
        realpath(candidate.c_str(), NULL), &free);
    if (!resolved) {
      return false;
    }
    return std::string(resolved.get()).find("/lib/clang/") != std::string::npos;
  };
  auto filter_staged_resource_dirs = [&](std::vector<std::string> &dirs) {
    dirs.erase(std::remove_if(dirs.begin(), dirs.end(),
                              [&](const std::string &path) {
                                return is_staged_clang_resource_dir(path);
                              }),
               dirs.end());
  };

  switch (language) {
  case ClangToSageTranslator::C:
    inc_list.push_back("clang-builtin-c.h");
    break;
  case ClangToSageTranslator::CPLUSPLUS:
    inc_list.push_back("clang-builtin-cpp.hpp");
    break;
  case ClangToSageTranslator::CUDA:
    inc_list.push_back("clang-builtin-cuda.hpp");
    break;
  case ClangToSageTranslator::OPENCL:
    inc_list.push_back("clang-builtin-opencl.h");
    break;
  case ClangToSageTranslator::OBJC: {
    printf("Objective C langauge support is not available in ROSE \n");
    ROSE_ABORT();
  }
  default: {
    printf("Default reached in switch(language) support \n");
    ROSE_ABORT();
  }
  }

  // Keep REX OpenMP/OpenACC extension APIs visible during frontend parsing.
  if (sageFile.get_openmp() || openmp_ast_mode || sageFile.get_openacc()) {
    inc_list.push_back("clang-builtin-openmp-compat.h");
#ifdef LLVM_OPENMP_INCLUDE_PATH
    if (std::strlen(LLVM_OPENMP_INCLUDE_PATH) != 0) {
      // Prefer including the configured LLVM omp.h by absolute path from the
      // compatibility wrapper to avoid mixing incompatible Clang resource
      // header directories in the active include search.
      define_list.push_back(std::string("ROSE_LLVM_OPENMP_HEADER_FILE=\"") +
                            LLVM_OPENMP_INCLUDE_PATH + "/omp.h\"");
    }
#endif
  }

  assertRequiredPreincludeConfigured(inc_list, language,
                                     "driver argument construction");

  // Avoid staged Clang resource headers that cause include_next loops.
  filter_staged_resource_dirs(sys_dirs_list);

  // Build cc1-style argument list for the in-process Clang invocation.

  const size_t estimated_argc = 1 + define_list.size() + inc_dirs_list.size() +
                                (sys_dirs_list.size() * 2) +
                                (inc_list.size() * 2) + passthrough_args.size();
  std::vector<std::string> args_storage;
  args_storage.reserve(estimated_argc);
  args_storage.push_back(language_arg);
  for (const auto &define : define_list) {
    args_storage.push_back("-D" + define);
  }
  for (const auto &inc_dir : inc_dirs_list) {
    args_storage.push_back("-I" + inc_dir);
  }
  for (const auto &sys_dir : sys_dirs_list) {
    args_storage.push_back("-isystem");
    args_storage.push_back(sys_dir);
  }
  for (const auto &inc : inc_list) {
    args_storage.push_back("-include");
    args_storage.push_back(inc);
  }
  for (const auto &pass : passthrough_args) {
    args_storage.push_back(pass);
  }

  std::vector<const char *> args;
  args.reserve(args_storage.size());
  for (const auto &arg : args_storage) {
    args.push_back(arg.c_str());
  }
  if (std::getenv("ROSE_CLANG_DUMP_INCLUDES") != nullptr) {
    std::cerr << "ROSE clang invocation args:\n";
    for (const auto &arg : args_storage) {
      std::cerr << "  " << arg << '\n';
    }
  }
#if DEBUG_ARGS
  for (size_t index = 0; index < args.size(); ++index) {
    std::cerr << "args[" << index << "] = " << args[index] << std::endl;
  }
#endif

  // 2 - Create a compiler instance

  auto diag_opts = std::make_shared<clang::DiagnosticOptions>();

  auto compiler_instance = std::make_unique<clang::CompilerInstance>();

  // Create diagnostics with instance-specific physical filesystem (avoids
  // global singleton double-free) Use createPhysicalFileSystem() instead of
  // getRealFileSystem() to avoid sharing the global singleton.
  // getRealFileSystem() returns a static IntrusiveRefCntPtr that libLLVM.so's
  // global destructor also tries to clean up, causing double-free.
  // createPhysicalFileSystem() creates a new instance that CompilerInstance can
  // safely own and destroy.
  //
  // Persist the VFS IntrusiveRefCntPtr so it lives beyond createDiagnostics()
  // call. Otherwise the temporary unique_ptr is destroyed immediately, leaving
  // a dangling reference.
  llvm::IntrusiveRefCntPtr<llvm::vfs::FileSystem> vfs =
      llvm::vfs::createPhysicalFileSystem();
  compiler_instance->setVirtualFileSystem(vfs);

  // TextDiagnosticPrinter keeps a reference to DiagnosticOptions; ensure it
  // outlives the diagnostics engine for LLVM 22.
  clang::TextDiagnosticPrinter *diag_printer =
      new clang::TextDiagnosticPrinter(llvm::errs(), *diag_opts);

  // LLVM 22 returns a DiagnosticsEngine; wire it into the CompilerInstance.
  compiler_instance->setDiagnostics(clang::CompilerInstance::createDiagnostics(
      *vfs, *diag_opts, diag_printer, true));

  clang::CompilerInvocation &invocation = compiler_instance->getInvocation();
  const std::string default_triple = llvm::sys::getDefaultTargetTriple();

  // Use Clang's driver to build cc1 arguments so target options (e.g. RISC-V
  // ABI defaults) match the toolchain instead of cc1's soft-float defaults.
  std::string driver_executable;
  bool resolved_clang_driver = false;
  auto resource_dir_has_header = [](const std::string &resource_dir,
                                    llvm::StringRef header) -> bool {
    if (resource_dir.empty()) {
      return false;
    }
    llvm::SmallString<256> path(resource_dir);
    llvm::sys::path::append(path, "include", header);
    return llvm::sys::fs::exists(path);
  };
  auto normalize_llvm_root = [](const std::string &llvm_dir_in) -> std::string {
    std::string llvm_dir = llvm_dir_in;
    if (!llvm_dir.empty() && llvm_dir.back() == '/') {
      llvm_dir.pop_back();
    }
    if (llvm_dir.empty()) {
      return llvm_dir;
    }
    const std::string cmake_suffix = "/lib/cmake/llvm";
    const std::string cmake_suffix_version =
        "/lib/cmake/llvm-" + std::to_string(LLVM_VERSION_MAJOR);
    llvm::StringRef dir_ref(llvm_dir);
    if (dir_ref.ends_with(cmake_suffix) ||
        dir_ref.ends_with(cmake_suffix_version)) {
      llvm::StringRef root_ref = llvm::sys::path::parent_path(dir_ref);
      root_ref = llvm::sys::path::parent_path(root_ref);
      root_ref = llvm::sys::path::parent_path(root_ref);
      return root_ref.str();
    }
    return llvm_dir;
  };
  std::string llvm_root;
  if (const char *llvm_dir_env = std::getenv("LLVM_DIR")) {
    llvm_root = normalize_llvm_root(llvm_dir_env);
  }
  auto find_clang_in_root = [&](const std::string &root) -> std::string {
    if (root.empty()) {
      return std::string();
    }
    std::string candidate =
        root + "/bin/clang-" + std::to_string(LLVM_VERSION_MAJOR);
    if (llvm::sys::fs::exists(candidate)) {
      return candidate;
    }
    candidate = root + "/bin/clang";
    if (llvm::sys::fs::exists(candidate)) {
      return candidate;
    }
    return std::string();
  };
  std::string clang_driver_path;
  {
    const std::string versioned_clang =
        "clang-" + std::to_string(LLVM_VERSION_MAJOR);
    clang_driver_path = find_clang_in_root(llvm_root);
    if (clang_driver_path.empty()) {
      if (auto clang_path = llvm::sys::findProgramByName(versioned_clang)) {
        clang_driver_path = clang_path.get();
      } else if (auto clang_path = llvm::sys::findProgramByName("clang")) {
        clang_driver_path = clang_path.get();
      }
    }
    if (!clang_driver_path.empty()) {
      driver_executable = clang_driver_path;
      resolved_clang_driver = true;
    } else if (driver_argv0 && *driver_argv0 != '\0') {
      driver_executable = driver_argv0;
    } else {
      driver_executable = "clang";
    }
  }

  std::vector<const char *> driver_args;
  driver_args.reserve(args_storage.size() + 2);
  driver_args.push_back(driver_executable.c_str());
  for (const auto &arg : args_storage) {
    driver_args.push_back(arg.c_str());
  }
  if (!input_file.empty()) {
    driver_args.push_back(input_file.c_str());
  }

  clang::driver::Driver driver(driver_executable, default_triple,
                               compiler_instance->getDiagnostics());
  driver.setCheckInputsExist(false);
  driver.setTitle("clang");
  std::string resource_dir_candidate;
  if (!clang_driver_path.empty()) {
    resource_dir_candidate =
        clang::GetResourcesPath(clang_driver_path.c_str(), (void *)&clang_main);
  } else if (!llvm_root.empty()) {
    const std::string llvm_fallback =
        llvm_root + "/bin/clang-" + std::to_string(LLVM_VERSION_MAJOR);
    resource_dir_candidate =
        clang::GetResourcesPath(llvm_fallback.c_str(), (void *)&clang_main);
  } else if (resolved_clang_driver) {
    resource_dir_candidate =
        clang::GetResourcesPath(driver_executable.c_str(), (void *)&clang_main);
  }
  if (!resource_dir_candidate.empty() &&
      resource_dir_has_header(resource_dir_candidate, "stddef.h")) {
    driver.ResourceDir = resource_dir_candidate;
  }

  std::unique_ptr<clang::driver::Compilation> compilation(
      driver.BuildCompilation(driver_args));
  if (!compilation) {
    llvm::errs() << "Failed to build Clang driver compilation for frontend.\n";
    ROSE_ABORT();
  }

  const clang::driver::Command *cc1_command = nullptr;
  for (const auto &job : compilation->getJobs()) {
    const auto *command = llvm::dyn_cast<clang::driver::Command>(&job);
    if (!command) {
      continue;
    }
    const auto &command_args = command->getArguments();
    for (const char *arg : command_args) {
      if (arg && std::strcmp(arg, "-cc1") == 0) {
        cc1_command = command;
        break;
      }
    }
    if (cc1_command) {
      break;
    }
  }

  if (!cc1_command) {
    llvm::errs()
        << "Failed to locate Clang cc1 command for frontend invocation.\n";
    ROSE_ABORT();
  }

  const auto &cc1_args = cc1_command->getArguments();
  std::vector<std::string> cc1_args_storage;
  cc1_args_storage.reserve(cc1_args.size());
  for (const char *arg : cc1_args) {
    if (arg) {
      cc1_args_storage.emplace_back(arg);
    } else {
      cc1_args_storage.emplace_back();
    }
  }

  auto canonical_path = [](const std::string &path) -> std::string {
    if (path.empty()) {
      return path;
    }
    llvm::SmallString<256> resolved;
    if (!llvm::sys::fs::real_path(path, resolved)) {
      return resolved.str().str();
    }
    return path;
  };

  std::string active_resource_include_dir;
  if (!driver.ResourceDir.empty()) {
    llvm::SmallString<256> include_dir(driver.ResourceDir);
    llvm::sys::path::append(include_dir, "include");
    active_resource_include_dir = canonical_path(include_dir.str().str());
  }
  auto is_clang_resource_include_dir = [&](const std::string &dir) -> bool {
    if (dir.empty()) {
      return false;
    }
    const std::string canonical_dir = canonical_path(dir);
    if (canonical_dir.find("/lib/clang/") == std::string::npos) {
      return false;
    }
    return llvm::sys::path::filename(canonical_dir) == "include";
  };
  auto should_drop_mismatched_clang_resource_include =
      [&](const std::string &dir) -> bool {
    if (active_resource_include_dir.empty()) {
      return false;
    }
    if (!is_clang_resource_include_dir(dir)) {
      return false;
    }
    return canonical_path(dir) != active_resource_include_dir;
  };

  // Drop mismatched Clang resource include directories (e.g., a different LLVM
  // major's <...>/lib/clang/<ver>/include) to avoid include_next guard
  // collisions in builtin headers such as stdint.h.
  std::vector<std::string> filtered_cc1_args_storage;
  filtered_cc1_args_storage.reserve(cc1_args_storage.size());
  auto is_split_system_include_flag = [](llvm::StringRef flag) {
    return flag == "-isystem" || flag == "-internal-isystem" ||
           flag == "-internal-externc-isystem" || flag == "-c-isystem" ||
           flag == "-cxx-isystem";
  };
  auto should_drop_prefixed_system_include = [&](llvm::StringRef flag) -> bool {
    auto drop_prefixed = [&](const char *prefix) -> bool {
      const std::size_t prefix_len = std::strlen(prefix);
      if (!flag.starts_with(prefix) || flag.size() <= prefix_len) {
        return false;
      }
      std::string dir = flag.substr(prefix_len).str();
      if (!dir.empty() && dir[0] == '=') {
        dir.erase(dir.begin());
      }
      return should_drop_mismatched_clang_resource_include(dir);
    };
    return drop_prefixed("-isystem") || drop_prefixed("-internal-isystem") ||
           drop_prefixed("-internal-externc-isystem") ||
           drop_prefixed("-c-isystem") || drop_prefixed("-cxx-isystem");
  };
  for (std::size_t i = 0; i < cc1_args_storage.size(); ++i) {
    llvm::StringRef flag(cc1_args_storage[i]);
    if (is_split_system_include_flag(flag)) {
      if (i + 1 < cc1_args_storage.size() &&
          should_drop_mismatched_clang_resource_include(
              cc1_args_storage[i + 1])) {
        ++i;
        continue;
      }
      filtered_cc1_args_storage.push_back(cc1_args_storage[i]);
      if (i + 1 < cc1_args_storage.size()) {
        filtered_cc1_args_storage.push_back(cc1_args_storage[i + 1]);
        ++i;
      }
      continue;
    }
    if (should_drop_prefixed_system_include(flag)) {
      continue;
    }
    filtered_cc1_args_storage.push_back(cc1_args_storage[i]);
  }
  cc1_args_storage.swap(filtered_cc1_args_storage);

  std::set<std::string> cc1_system_include_dirs;
  auto record_system_include = [&](const std::string &dir) {
    if (dir.empty()) {
      return;
    }
    cc1_system_include_dirs.insert(canonical_path(dir));
  };
  auto parse_system_include_flag = [&](const std::string &flag,
                                       std::size_t index) -> bool {
    auto consume_prefixed = [&](const char *prefix) -> bool {
      const std::size_t prefix_len = std::strlen(prefix);
      if (flag.rfind(prefix, 0) != 0 || flag.size() <= prefix_len) {
        return false;
      }
      std::string suffix = flag.substr(prefix_len);
      if (!suffix.empty() && suffix[0] == '=') {
        suffix.erase(suffix.begin());
      }
      if (!suffix.empty()) {
        record_system_include(suffix);
      }
      return true;
    };

    if (flag == "-isystem" || flag == "-internal-isystem" ||
        flag == "-internal-externc-isystem" || flag == "-c-isystem" ||
        flag == "-cxx-isystem") {
      if (index + 1 < cc1_args_storage.size()) {
        record_system_include(cc1_args_storage[index + 1]);
      }
      return true;
    }
    return consume_prefixed("-isystem") ||
           consume_prefixed("-internal-isystem") ||
           consume_prefixed("-internal-externc-isystem") ||
           consume_prefixed("-c-isystem") || consume_prefixed("-cxx-isystem");
  };
  for (std::size_t i = 0; i < cc1_args_storage.size(); ++i) {
    parse_system_include_flag(cc1_args_storage[i], i);
  }

  // OpenMP parsing in REX is pragma-driven (without forwarding -fopenmp to
  // Clang), so explicitly add a compatibility wrapper include dir (contains
  // omp.h wrapper). The wrapper injects the configured omp.h by absolute path.
  if (sageFile.get_openmp() && !disable_openmp_via_flag) {
    std::string openmp_compat_include_dir;
    const std::vector<std::string> openmp_compat_candidates = {
        ROSE_BUILD_CLANG_INCLUDE_STAGING_DIR + "/openmp-compat",
        ROSE_SOURCE_TREE + "/src/frontend/CxxFrontend/Clang/openmp-compat",
        ROSE_INSTALL_PREFIX + "/" + ROSE_INSTALL_CLANG_INCLUDE_DIR +
            "/openmp-compat"};

    for (const auto &candidate : openmp_compat_candidates) {
      if (candidate.empty()) {
        continue;
      }
      std::string normalized_candidate = canonical_path(candidate);
      llvm::SmallString<256> compat_header(normalized_candidate);
      llvm::sys::path::append(compat_header, "omp.h");
      if (llvm::sys::fs::exists(compat_header)) {
        openmp_compat_include_dir = normalized_candidate;
        break;
      }
    }
    if (openmp_compat_include_dir.empty()) {
      llvm::errs()
          << "REX OpenMP frontend cannot find compatibility omp.h wrapper.\n";
      ROSE_ABORT();
    }
    if (cc1_system_include_dirs.count(openmp_compat_include_dir) == 0) {
      // Force wrapper precedence over Clang resource/system omp.h. The wrapper
      // temporarily hides _OPENMP before include_next <omp.h>.
      cc1_args_storage.push_back("-I");
      cc1_args_storage.push_back(openmp_compat_include_dir);
      cc1_system_include_dirs.insert(openmp_compat_include_dir);
    }

    std::string llvm_openmp_include_dir;
#ifdef LLVM_OPENMP_INCLUDE_PATH
    llvm_openmp_include_dir = canonical_path(LLVM_OPENMP_INCLUDE_PATH);
#endif
    if (llvm_openmp_include_dir.empty()) {
      llvm::errs()
          << "REX OpenMP frontend requires LLVM_OPENMP_INCLUDE_PATH to be "
             "configured.\n";
      ROSE_ABORT();
    }

    llvm::SmallString<256> llvm_openmp_header(llvm_openmp_include_dir);
    llvm::sys::path::append(llvm_openmp_header, "omp.h");
    if (!llvm::sys::fs::exists(llvm_openmp_header)) {
      llvm::errs() << "REX OpenMP frontend cannot find LLVM omp.h at: "
                   << llvm_openmp_header << "\n";
      ROSE_ABORT();
    }

    if (!should_drop_mismatched_clang_resource_include(
            llvm_openmp_include_dir) &&
        cc1_system_include_dirs.count(llvm_openmp_include_dir) == 0) {
      cc1_args_storage.push_back("-internal-isystem");
      cc1_args_storage.push_back(llvm_openmp_include_dir);
      cc1_system_include_dirs.insert(llvm_openmp_include_dir);
    }
  }

  std::vector<const char *> cc1_args_cstr;
  cc1_args_cstr.reserve(cc1_args_storage.size());
  for (const auto &arg : cc1_args_storage) {
    cc1_args_cstr.push_back(arg.c_str());
  }
  llvm::ArrayRef<const char *> argsArrayRef(cc1_args_cstr.data(),
                                            cc1_args_cstr.size());
  bool invocation_ok = clang::CompilerInvocation::CreateFromArgs(
      invocation, argsArrayRef, compiler_instance->getDiagnostics());
  if (!invocation_ok) {
    llvm::errs() << "Failed to create Clang invocation from cc1 arguments.\n";
    ROSE_ABORT();
  }
  clang::TargetOptions &target_opts = invocation.getTargetOpts();
  ensureX86BaselineTargetFeatures(target_opts);

  clang::DiagnosticsEngine &diags = compiler_instance->getDiagnostics();
  if ((language == ClangToSageTranslator::CPLUSPLUS ||
       language == ClangToSageTranslator::CUDA) &&
      is_cxx17_or_later(sageFile.get_standard())) {
    if (relax_register_diag) {
      diags.setSeverityForGroup(clang::diag::Flavor::WarningOrError, "register",
                                clang::diag::Severity::Warning);
      diags.setDiagnosticGroupWarningAsError("register", false);
    }
    if (relax_dynamic_exception_diag) {
      diags.setSeverityForGroup(clang::diag::Flavor::WarningOrError,
                                "dynamic-exception-spec",
                                clang::diag::Severity::Warning);
      diags.setDiagnosticGroupWarningAsError("dynamic-exception-spec", false);
    }
  }
  // LLVM 22 rejects remapping these hard alignment errors into warnings.
  // Preserve the frontend's native severity and record valid alignments from
  // the translated AST instead of trying to force permissive parsing here.

  std::string invocation_triple = target_opts.Triple;
  if (invocation_triple.empty()) {
    invocation_triple = default_triple;
    target_opts.Triple = invocation_triple;
  }
  const llvm::Triple target_triple(invocation_triple);

  // CLANG FRONTEND FIX: Configure header search paths properly
  // WHY: Default paths may point to incorrect locations (e.g.,
  // /usr/include/newlib/ on ARM) CLANG FRONTEND FIX #22: Use Clang's automatic
  // header detection instead of hard-coded paths This ensures portability
  // across different systems, LLVM versions, and distributions.
  //
  // The CompilerInvocation::CreateFromArgs() call above already sets up correct
  // paths:
  // - ResourceDir is auto-detected from the Clang installation
  // - System include paths are computed by Clang's Driver based on the target
  // triple
  // - No need to override with hard-coded paths that break on other systems
  //
  // We only need to enable the relevant flags to use Clang's built-in
  // detection:
  clang::HeaderSearchOptions &headerSearchOpts =
      invocation.getHeaderSearchOpts();

  // Ensure Clang's builtin and system include paths are active for LLVM 22 so
  // the resource-dir headers are always available.
  headerSearchOpts.UseBuiltinIncludes = true;
  headerSearchOpts.UseStandardSystemIncludes = true;
  headerSearchOpts.UseStandardCXXIncludes = true;

  if (headerSearchOpts.ResourceDir.empty()) {
    if (!driver.ResourceDir.empty()) {
      headerSearchOpts.ResourceDir = driver.ResourceDir;
    } else {
      headerSearchOpts.ResourceDir = clang::GetResourcesPath(
          driver_executable.c_str(), (void *)&clang_main);
    }
  } else if (!resource_dir_has_header(headerSearchOpts.ResourceDir,
                                      "stddef.h")) {
    if (!driver.ResourceDir.empty() &&
        resource_dir_has_header(driver.ResourceDir, "stddef.h")) {
      headerSearchOpts.ResourceDir = driver.ResourceDir;
    } else if (!resource_dir_candidate.empty() &&
               resource_dir_has_header(resource_dir_candidate, "stddef.h")) {
      headerSearchOpts.ResourceDir = resource_dir_candidate;
    }
  }

  if (std::getenv("ROSE_CLANG_DUMP_INCLUDES") != nullptr) {
    llvm::errs() << "ROSE clang header search paths:\n";
    llvm::errs() << "  [driver-executable] " << driver_executable << "\n";
    llvm::errs() << "  [driver-resolved] "
                 << (resolved_clang_driver ? "yes" : "no") << "\n";
    if (const char *llvm_dir_env = std::getenv("LLVM_DIR")) {
      llvm::errs() << "  [env LLVM_DIR] " << llvm_dir_env << "\n";
    }
    llvm::errs() << "  [resource-dir] " << headerSearchOpts.ResourceDir << "\n";
    llvm::errs() << "  [builtin-includes] "
                 << (headerSearchOpts.UseBuiltinIncludes ? "on" : "off")
                 << "\n";
    llvm::errs() << "  [standard-system-includes] "
                 << (headerSearchOpts.UseStandardSystemIncludes ? "on" : "off")
                 << "\n";
    llvm::errs() << "  [standard-cxx-includes] "
                 << (headerSearchOpts.UseStandardCXXIncludes ? "on" : "off")
                 << "\n";
    for (const auto &entry : headerSearchOpts.UserEntries) {
      llvm::StringRef group_name = "user";
      if (entry.Group == clang::frontend::System) {
        group_name = "system";
      } else if (entry.Group == clang::frontend::CXXSystem) {
        group_name = "cxx-system";
      } else if (entry.Group == clang::frontend::CSystem) {
        group_name = "c-system";
      }
      llvm::errs() << "  [" << group_name << "] " << entry.Path << "\n";
    }
  }

  // ResourceDir and system include paths are already correctly set by
  // CreateFromArgs() based on the Clang installation and target triple. Do NOT
  // override them with hard-coded paths.

  clang::LangOptions &lang_opts = compiler_instance->getLangOpts();
  std::vector<std::string> lang_specific_includes;
  clang::Language clang_lang = clang::Language::C;
  bool enable_cuda = false;
  bool enable_opencl = false;

  auto parse_requested_standard_from_cc1_args =
      [&](const std::vector<std::string> &cc1_args) {
        clang::LangStandard::Kind parsed_std =
            clang::LangStandard::lang_unspecified;
        for (std::size_t i = 0; i < cc1_args.size(); ++i) {
          llvm::StringRef flag(cc1_args[i]);
          llvm::StringRef standard_name;
          if (flag == "-std" && (i + 1) < cc1_args.size()) {
            standard_name = cc1_args[i + 1];
          } else if (flag.starts_with("-std=")) {
            standard_name = flag.drop_front(std::strlen("-std="));
          }
          if (standard_name.empty()) {
            continue;
          }
          clang::LangStandard::Kind candidate =
              clang::LangStandard::getLangKind(standard_name);
          if (candidate != clang::LangStandard::lang_unspecified) {
            parsed_std = candidate;
          }
        }
        return parsed_std;
      };

  switch (language) {
  case ClangToSageTranslator::C:
    clang_lang = clang::Language::C;
    break;
  case ClangToSageTranslator::CPLUSPLUS:
    clang_lang = clang::Language::CXX;
    break;
  case ClangToSageTranslator::CUDA:
    clang_lang = clang::Language::CUDA;
    enable_cuda = true;
    break;
  case ClangToSageTranslator::OPENCL:
    clang_lang = clang::Language::OpenCL;
    enable_opencl = true;
    break;
  case ClangToSageTranslator::OBJC:
    ROSE_ASSERT(!"Objective-C is not supported by ROSE Compiler.");
  default:
    ROSE_ABORT();
  }

  clang::LangStandard::Kind requested_std =
      parse_requested_standard_from_cc1_args(cc1_args_storage);
  if (requested_std == clang::LangStandard::lang_unspecified) {
    requested_std =
        clang::getDefaultLanguageStandard(clang_lang, target_triple);
  }

  clang::LangStandard std_info =
      clang::LangStandard::getLangStandardForKind(requested_std);
  switch (language) {
  case ClangToSageTranslator::CPLUSPLUS:
  case ClangToSageTranslator::CUDA:
    if (!std_info.isCPlusPlus()) {
      requested_std = clang::LangStandard::lang_gnucxx17;
    }
    break;
  case ClangToSageTranslator::OPENCL:
    if (!std_info.isOpenCL()) {
      requested_std = clang::LangStandard::lang_opencl30;
    }
    break;
  default:
    break;
  }

  clang::InputKind input_kind(clang_lang);
  clang::FrontendOptions &fe_opts = invocation.getFrontendOpts();
  fe_opts.Inputs.clear();
  fe_opts.Inputs.emplace_back(input_file, input_kind);

  clang::LangOptions::setLangDefaults(lang_opts, clang_lang, target_triple,
                                      lang_specific_includes, requested_std);
  if (lang_opts.GNUCVersion == 0) {
    // Clang's driver normally sets a GCC compatibility version; mirror that
    // here so GNU built-in macros (e.g., __GNUC__) are defined.
    lang_opts.GNUCVersion = 40201; // GCC 4.2.1 (Clang default)
  }
  if (openmp_ast_mode) {
    // OpenMP/OpenACC AST-only parsing should preserve the same language
    // standard selected for normal frontend parsing. Do not force legacy C89
    // semantics here; users who do not pass -std should get Clang's default.
    // Keep C++ in hosted mode so standard headers (iostream, etc.) remain
    // usable.
    if (language == ClangToSageTranslator::C) {
      lang_opts.Freestanding = 1;
    }
  }

  if (language == ClangToSageTranslator::CPLUSPLUS) {
    ROSE_ASSERT(lang_opts.CPlusPlus &&
                "Expected C++ mode after setting language defaults");
    lang_opts.Bool = 1;
  }

  if (language == ClangToSageTranslator::CPLUSPLUS ||
      language == ClangToSageTranslator::CUDA) {
    if (exception_mode == ExceptionMode::Disabled) {
      lang_opts.CXXExceptions = 0;
      lang_opts.Exceptions = 0;
    } else {
      lang_opts.CXXExceptions = 1;
      lang_opts.Exceptions = 1;
    }
  }

  if (language == ClangToSageTranslator::CPLUSPLUS ||
      language == ClangToSageTranslator::CUDA) {
    lang_opts.RTTI = frontend_rtti_enabled ? 1 : 0;
    lang_opts.RTTIData = frontend_rtti_enabled ? 1 : 0;
  }

  if (enable_cuda) {
    lang_opts.CUDA = 1;
  }
  if (enable_opencl) {
    lang_opts.OpenCL = 1;
  }
  if (language == ClangToSageTranslator::CPLUSPLUS ||
      language == ClangToSageTranslator::CUDA) {
    if (disable_access_control) {
      lang_opts.AccessControl = 0;
    }
  }

  // Now create file manager with FileSystemOptions from the parsed invocation
  compiler_instance->createFileManager();

  clang::PreprocessorOptions &pp_opts =
      compiler_instance->getInvocation().getPreprocessorOpts();
  pp_opts.UsePredefines = true;
  // NOTE: OpenMP/OpenACC AST-only mode may apply additional preprocessing
  // normalization later to preserve malformed-but-intentional test inputs.
  if (!lang_specific_includes.empty()) {
    pp_opts.Includes.insert(pp_opts.Includes.end(),
                            lang_specific_includes.begin(),
                            lang_specific_includes.end());
  }
  assertRequiredPreincludeConfigured(pp_opts.Includes, language,
                                     "compiler invocation setup");

  // LLVM requires reference
  clang::TargetInfo *target_info = clang::TargetInfo::CreateTargetInfo(
      compiler_instance->getDiagnostics(), invocation.getTargetOpts());
  compiler_instance->setTarget(target_info);

  compiler_instance->createSourceManager();

  // getFileRef returns Expected<FileEntryRef> instead of ErrorOr
  llvm::Expected<clang::FileEntryRef> ret =
      compiler_instance->getFileManager().getFileRef(input_file);
  if (!ret) {
    llvm::errs() << "Error opening file: " << input_file << "\n";
    ROSE_ABORT();
  }
  clang::FileEntryRef input_file_entry = *ret;
  // createFileID takes FileEntryRef instead of const FileEntry*
  clang::FileID mainFileID = compiler_instance->getSourceManager().createFileID(
      input_file_entry, clang::SourceLocation(), clang::SrcMgr::C_User);

  const bool is_c_roundtrip_language = language == ClangToSageTranslator::C;
  const bool is_c_family_roundtrip_language =
      is_c_roundtrip_language || language == ClangToSageTranslator::CPLUSPLUS ||
      language == ClangToSageTranslator::CUDA;
  const bool needs_exact_line_splice_roundtrip =
      is_c_family_roundtrip_language &&
      sourceFileContainsPhysicalLineSplice(input_file);

  if (is_c_family_roundtrip_language && !openmp_ast_mode &&
      !sageFile.get_openmp_lowering() &&
      (sageFile.get_skipfinalCompileStep() ||
       needs_exact_line_splice_roundtrip)) {
    // Preserve original spelling for workflows that compare round-tripped
    // sources directly, and for untouched inputs that depend on physical
    // line-splice semantics that the AST alone cannot reconstruct.
    sageFile.set_unparse_tokens(true);
  }

  bool normalized_post_record_alignas = false;
  std::vector<SuppressedIncludeDirective> suppressed_includes;
  if (language == ClangToSageTranslator::C) {
    if (auto fixed_buffer = maybeNormalizePostRecordAlignas(
            compiler_instance->getSourceManager(), mainFileID, lang_opts)) {
      compiler_instance->getSourceManager().overrideFileContents(
          input_file_entry, std::move(fixed_buffer));
      // The token stream no longer matches the on-disk source; unparse from
      // the AST to preserve normalized C11 alignment declarations.
      sageFile.set_unparse_tokens(false);
      normalized_post_record_alignas = true;
    }
  }
  if (openmp_ast_mode) {
    OpenMPDeclareVariantRewriteResult declare_variant_rewrite =
        captureAndElideOpenMPDeclareVariantRegions(
            compiler_instance->getSourceManager(), mainFileID);
    if (!declare_variant_rewrite.regions.empty()) {
      sageFile.addNewAttribute(kOmpDeclareVariantRegionsAttributeName,
                               new OmpDeclareVariantRegionsAttribute(
                                   std::move(declare_variant_rewrite.regions)));
    }
    if (declare_variant_rewrite.rewritten_buffer) {
      compiler_instance->getSourceManager().overrideFileContents(
          input_file_entry,
          std::move(declare_variant_rewrite.rewritten_buffer));
      // The Clang-visible buffer now elides declare-variant bodies while the
      // Sage AST preserves the original opaque region text for unparsing.
      sageFile.set_unparse_tokens(false);
    }
    if (auto fixed_buffer = maybeSuppressMisplacedIncludes(
            compiler_instance->getSourceManager(), mainFileID,
            &suppressed_includes)) {
      compiler_instance->getSourceManager().overrideFileContents(
          input_file_entry, std::move(fixed_buffer));
      // The token stream no longer reflects the on-disk source. Force
      // AST-based unparsing so preprocessing info can be reinserted.
      sageFile.set_unparse_tokens(false);
    }
  }

  compiler_instance->getSourceManager().setMainFileID(mainFileID);

  if (!compiler_instance->hasPreprocessor())
    compiler_instance->createPreprocessor(clang::TU_Complete);

  // Register pragma and preprocessor callbacks (OpenMP and includes).
  RoseOpenMPPragmaCallback *omp_callback = nullptr;
  SagePreprocessorRecord *preprocessor_recorder = nullptr;
  std::unique_ptr<SagePreprocessorRecord> secondary_preprocessor_recorder;
  {
    clang::Preprocessor &PP = compiler_instance->getPreprocessor();
    auto omp_callback_owner = std::make_unique<RoseOpenMPPragmaCallback>(
        compiler_instance->getSourceManager(), PP);
    omp_callback = omp_callback_owner.get();
    PP.addPPCallbacks(std::move(omp_callback_owner));

    auto preprocessor_recorder_owner = std::make_unique<SagePreprocessorRecord>(
        &(compiler_instance->getSourceManager()), &PP);
    preprocessor_recorder = preprocessor_recorder_owner.get();
    if (!is_secondary_parse) {
      PP.addPPCallbacks(std::move(preprocessor_recorder_owner));
      PP.addCommentHandler(preprocessor_recorder);
    } else {
      // Secondary parses should not contribute another directive/comment stream
      // to the already-annotated primary AST.
      secondary_preprocessor_recorder = std::move(preprocessor_recorder_owner);
    }
  }
  if (!is_secondary_parse && preprocessor_recorder != nullptr) {
    clang::DiagnosticsEngine &diags = compiler_instance->getDiagnostics();
    std::unique_ptr<clang::DiagnosticConsumer> existing_client =
        diags.takeClient();
    diags.setClient(new RoseDiagnosticConsumer(std::move(existing_client),
                                               preprocessor_recorder),
                    true);
  }
  if (preprocessor_recorder != nullptr && !suppressed_includes.empty()) {
    clang::SourceLocation file_start =
        compiler_instance->getSourceManager().getLocForStartOfFile(mainFileID);
    for (const auto &entry : suppressed_includes) {
      clang::SourceLocation loc =
          file_start.getLocWithOffset(entry.hash_offset);
      preprocessor_recorder->recordInjectedDirective(loc, entry.directive_type,
                                                     entry.text);
    }
  }

  if (!compiler_instance->hasASTContext())
    compiler_instance->createASTContext();

  compiler_instance->getPreprocessor().getBuiltinInfo().initializeBuiltins(
      compiler_instance->getPreprocessor().getIdentifierTable(), lang_opts);

  auto translator_ptr = std::make_unique<ClangToSageTranslator>(
      compiler_instance.get(), language, &sageFile, preprocessor_recorder);
  ClangToSageTranslator *translator = translator_ptr.get();

  // Pass pragma callback to translator
  if (omp_callback) {
    translator->setOpenMPPragmaCallback(omp_callback);
  }

  compiler_instance->setASTConsumer(std::move(translator_ptr));

  if (!compiler_instance->hasSema())
    compiler_instance->createSema(clang::TU_Complete, NULL);

  ROSE_ASSERT(compiler_instance->hasDiagnostics());
  ROSE_ASSERT(compiler_instance->hasTarget());
  ROSE_ASSERT(compiler_instance->hasFileManager());
  ROSE_ASSERT(compiler_instance->hasSourceManager());
  ROSE_ASSERT(compiler_instance->hasPreprocessor());
  ROSE_ASSERT(compiler_instance->hasASTContext());
  ROSE_ASSERT(compiler_instance->hasSema());

  // 3 - Translate

  //  printf ("Calling clang::ParseAST()\n");

  struct SourcePositionModeGuard {
    SageBuilder::SourcePositionClassification saved;
    explicit SourcePositionModeGuard(
        SageBuilder::SourcePositionClassification mode)
        : saved(SageBuilder::getSourcePositionClassificationMode()) {
      SageBuilder::setSourcePositionClassificationMode(mode);
    }
    ~SourcePositionModeGuard() {
      SageBuilder::setSourcePositionClassificationMode(saved);
    }
  };

  // Ensure frontend-created nodes do not start as transformations.
  SourcePositionModeGuard source_position_guard(
      SageBuilder::e_sourcePositionFrontendConstruction);

  compiler_instance->getDiagnosticClient().BeginSourceFile(
      compiler_instance->getLangOpts(),
      &(compiler_instance->getPreprocessor()));
  if (language == ClangToSageTranslator::CPLUSPLUS) {
    clang::IdentifierInfo &builtin_id =
        compiler_instance->getPreprocessor().getIdentifierTable().get(
            "__builtin_clzll");
    ROSE_ASSERT(builtin_id.getBuiltinID() !=
                    static_cast<unsigned>(clang::Builtin::NotBuiltin) &&
                "Expected Clang builtins to be initialised for C++ mode");
  }
  clang::ParseAST(compiler_instance->getPreprocessor(), translator,
                  compiler_instance->getASTContext());
  compiler_instance->getDiagnosticClient().EndSourceFile();

  // get error count from diagnostics directly
  unsigned numErrors = compiler_instance->getDiagnostics().getNumErrors();
  if (numErrors > 0) {
    printf("Clang found %d diagnostic errors during parsing\n", numErrors);
  }

  if (is_c_roundtrip_language && preprocessor_recorder != nullptr &&
      preprocessor_recorder->sawSelfReferentialMacroExpansion() &&
      !openmp_ast_mode && !sageFile.get_openmp_lowering()) {
    // Preserve the original tokens for active self-referential macros whose
    // normalized AST spelling would otherwise be re-expanded on output.
    sageFile.set_unparse_tokens(true);
  }

  if (sageFile.get_unparse_tokens() && is_c_family_roundtrip_language &&
      preprocessor_recorder != nullptr &&
      preprocessor_recorder->sawSelfReferentialMacroExpansion() &&
      sourceFileContainsBackendConditional(input_file) && !openmp_ast_mode &&
      !sageFile.get_openmp_lowering()) {
    // USE_ROSE_BACKEND is only defined during the backend compile. If the
    // source toggles self-referential macros inside backend-only conditionals,
    // replaying the frontend token stream preserves macro spellings that are
    // intentionally undefined during recompilation. Prefer AST-based unparsing
    // so the generated file emits the expanded expression instead.
    sageFile.set_unparse_tokens(false);
  }

  SgGlobal *global_scope = translator->getGlobalScope();

  // 4 - Attach to the file

  if (sageFile.get_globalScope() != NULL) {
    SgGlobal *old_global_scope = sageFile.get_globalScope();
    sageFile.set_globalScope(nullptr);
    old_global_scope->set_parent(nullptr);
    SageInterface::deleteAST(old_global_scope);
    auto map_it = Rose::tokenSubsequenceMapOfMapsBySourceFile.find(&sageFile);
    if (map_it != Rose::tokenSubsequenceMapOfMapsBySourceFile.end() &&
        map_it->second != NULL) {
      // Clear stale token mappings from the previous AST to avoid
      // dangling SgNode* references on re-parse.
      map_it->second->clear();
    }
  }

  sageFile.set_globalScope(global_scope);

  // Check if OpenMP was enabled by ROSE's command line processor
  // ROSE processes -fopenmp before clang_main is called and sets these flags
  if (sageFile.get_openmp()) {
    // By default, ROSE sets parse_only mode. For Clang frontend, we want to
    // default to ast_only. Only override if no explicit processing flag was set
    // by user
    bool has_explicit_processing_flag = sageFile.get_openmp_ast_only() ||
                                        sageFile.get_openmp_lowering() ||
                                        sageFile.get_openmp_analyzing();

    if (!has_explicit_processing_flag && sageFile.get_openmp_parse_only()) {
      sageFile.set_openmp_parse_only(false);
      sageFile.set_openmp_ast_only(true);
    }
  }

  if (openmp_ast_mode) {
    // OpenMP/OpenACC AST-only workflows are pragma-normalization passes;
    // skip backend compilation to avoid errors from legacy test inputs.
    sageFile.set_skipfinalCompileStep(true);
  }

  // Parent relationship already set up during global scope creation

  std::string file_name(input_file);

  Sg_File_Info *start_fi = new Sg_File_Info(file_name, 0, 0);
  Sg_File_Info *end_fi = new Sg_File_Info(file_name, 0, 0);

  global_scope->set_startOfConstruct(start_fi);

  global_scope->set_endOfConstruct(end_fi);

  // 6 - Finish the AST (fixup phase)
  //
  // Error-containing parses can leave template/linkage structures incomplete.
  // If Clang still produced an AST, finish ROSE post-processing on that partial
  // tree so explicit continue-on-error workflows can inspect it. Diagnostic
  // errors still remain a frontend failure unless the user asked to continue.
  const bool canRecoverWithConstructedAst = global_scope != NULL;
  const bool proceedAfterDiagnosticErrors =
      (numErrors == 0) || continue_on_error || canRecoverWithConstructedAst;
  if (proceedAfterDiagnosticErrors) {
    finishSageAST(*translator);
    translator->materializeApplicationHeaderDecls();
  }

  const bool needs_exact_function_declarator_roundtrip =
      language == ClangToSageTranslator::C && !sageFile.get_openmp() &&
      !sageFile.get_openacc() &&
      fileRequiresExactFunctionDeclaratorRoundtrip(global_scope);
  if (needs_exact_function_declarator_roundtrip) {
    // Some C declarators depend on preprocessing structure that the AST cannot
    // spell canonically without duplication. Preserve the original token
    // spelling for those untouched round-trip inputs.
    sageFile.set_unparse_tokens(true);
  }

  if (normalized_post_record_alignas && global_scope != NULL) {
    // Parsing consumed an in-memory normalized buffer for C compatibility.
    // Force AST-based
    // unparsing so output reflects the normalized form instead of raw on-disk
    // tokens.
    sageFile.set_unparse_tokens(false);
    global_scope->setTransformation();
    global_scope->set_isModified(true);
    if (Sg_File_Info *scope_info = global_scope->get_file_info()) {
      scope_info->setTransformation();
      scope_info->setOutputInCodeGeneration();
    }
  }

  // 7 - OpenMP Processing
  //
  // NOTE: finishSageAST() enables OpenMP processing when OpenMP/OpenACC
  // pragmas are present so the standard secondary pass can convert them.

  // 8 - Cleanup LLVM objects
  //
  // Now that we use createPhysicalFileSystem() instead of
  // getRealFileSystem(), the CompilerInstance owns its own VFS instance
  // rather than sharing the global singleton. This means we can safely delete
  // the CompilerInstance without causing double-free errors.
  //
  // The translator is owned by CompilerInstance via unique_ptr<ASTConsumer>,
  // so it will also be destroyed when CompilerInstance is deleted. The ROSE
  // AST (global_scope, etc.) persists in sageFile and is NOT owned by the
  // translator, so it remains valid after cleanup.

  // REX: By default, treat Clang diagnostic errors as fatal. Translation-only
  // workflows that already requested -rose:skipfinalCompileStep can still
  // succeed when Clang built a recoverable AST, because those tests are asking
  // ROSE to analyze/unparse rather than produce backend object code.
  if (global_scope == NULL) {
    printf("Error: Failed to build AST - global_scope is NULL\n");
    return (numErrors > 0) ? numErrors : 1; // Failure - no AST
  }

  const bool translation_only_recovery =
      canRecoverWithConstructedAst && sageFile.get_skipfinalCompileStep() &&
      language == ClangToSageTranslator::CUDA;

  if (numErrors > 0) {
    if (continue_on_error || translation_only_recovery) {
      sageFile.set_skipfinalCompileStep(true);
      printf("Note: Proceeding despite %d Clang diagnostic error(s) because "
             "AST was successfully constructed",
             numErrors);
      if (translation_only_recovery && !continue_on_error) {
        printf(" and translation-only processing was requested");
      }
      printf("\n");
      return 0; // Success - AST was built
    }
    sageFile.set_skipfinalCompileStep(true);
    printf("Error: Clang reported %d diagnostic error(s); refusing to run "
           "backend",
           numErrors);
    if (canRecoverWithConstructedAst) {
      printf(" even though AST construction succeeded");
    }
    printf(" (use -rex:clang:continue-on-error to override)\n");
    return numErrors;
  }

  return 0; // Success - AST was built
}

void finishSageAST(ClangToSageTranslator &translator) {
  SgGlobal *global_scope = translator.getGlobalScope();

  // Normalize function-symbol bindings before postprocessing that relies on
  // declaration scope symbol tables (e.g., template-name reset).
  translator.repairMissingFunctionSymbols();

  // Insert captured pragmas that were not attached during statement
  // translation (e.g., standalone directives or file-scope pragmas).
  translator.appendUnattachedPragmas();

  // 1 - Label Statements: Move sub-statement after the label statement.

  // Pei-Hung (05/13/2022) This step is no longer needed as subStatement is
  // properly handled.
  /*
      std::vector<SgLabelStatement *> label_stmts =
     SageInterface::querySubTree<SgLabelStatement>(global_scope,
     V_SgLabelStatement); std::vector<SgLabelStatement *>::iterator
     label_stmt_it; for (label_stmt_it = label_stmts.begin(); label_stmt_it !=
     label_stmts.end(); label_stmt_it++) { SgLabelStatement* labelStmt =
     isSgLabelStatement(*label_stmt_it); SgStatement * sub_stmt =
     labelStmt->get_statement(); if (!isSgNullStatement(sub_stmt)) {
              SgNullStatement * null_stmt =
     SageBuilder::buildNullStatement_nfi();
              translator.setCompilerGeneratedFileInfo(null_stmt);
              labelStmt->set_statement(null_stmt);
              null_stmt->set_parent(labelStmt);
              SageInterface::insertStatementAfter(labelStmt, sub_stmt);
          }
      }
  */
  // 2 - Place Preprocessor informations

  translator.sortPreprocessorList();
  if (translator.preprocessor_list_size() > 0) {
    NextPreprocessorToInsert npp(translator);
    std::pair<Sg_File_Info *, PreprocessingInfo *> top =
        translator.preprocessor_top();
    npp.cursor = top.first;
    npp.next_to_insert = top.second;
    npp.candidat = NULL;

    PreprocessorInserter preprocessor_inserter;
    preprocessor_inserter.traverse(global_scope, &npp);
  }

  if (translator.preprocessor_list_size() > 0 && global_scope != nullptr) {
    Sg_File_Info *main_scope_info = global_scope->get_file_info();
    const int main_file_id =
        main_scope_info != nullptr ? main_scope_info->get_file_id() : -1;
    const std::string main_filename =
        main_scope_info != nullptr ? main_scope_info->get_filenameString()
                                   : std::string();
    auto is_from_main_file = [&](Sg_File_Info *info) -> bool {
      if (info == nullptr || info->get_line() <= 0) {
        return false;
      }
      if (main_file_id >= 0 && info->get_file_id() == main_file_id) {
        return true;
      }
      if (!main_filename.empty() && !info->get_filenameString().empty()) {
        return info->get_filenameString() == main_filename;
      }
      return false;
    };

    auto find_last_output_stmt = [&](SgGlobal *scope) -> SgStatement * {
      if (scope == nullptr) {
        return nullptr;
      }
      SgDeclarationStatementPtrList &decls = scope->get_declarations();
      for (auto it = decls.rbegin(); it != decls.rend(); ++it) {
        SgDeclarationStatement *decl = *it;
        if (decl == nullptr) {
          continue;
        }
        Sg_File_Info *info = decl->get_file_info();
        if (info == nullptr || !is_from_main_file(info)) {
          continue;
        }
        if (!info->isCompilerGenerated() || info->isOutputInCodeGeneration()) {
          return decl;
        }
      }
      for (auto it = decls.rbegin(); it != decls.rend(); ++it) {
        SgDeclarationStatement *decl = *it;
        if (decl == nullptr) {
          continue;
        }
        Sg_File_Info *info = decl->get_file_info();
        if (info == nullptr) {
          continue;
        }
        if (!info->isCompilerGenerated() || info->isOutputInCodeGeneration()) {
          return decl;
        }
      }
      return nullptr;
    };
    auto find_first_output_stmt = [&](SgGlobal *scope) -> SgStatement * {
      if (scope == nullptr) {
        return nullptr;
      }
      SgDeclarationStatementPtrList &decls = scope->get_declarations();
      for (SgDeclarationStatement *decl : decls) {
        if (decl == nullptr) {
          continue;
        }
        Sg_File_Info *info = decl->get_file_info();
        if (info == nullptr || !is_from_main_file(info)) {
          continue;
        }
        if (!info->isCompilerGenerated() || info->isOutputInCodeGeneration()) {
          return decl;
        }
      }
      for (SgDeclarationStatement *decl : decls) {
        if (decl == nullptr) {
          continue;
        }
        Sg_File_Info *info = decl->get_file_info();
        if (info == nullptr) {
          continue;
        }
        if (!info->isCompilerGenerated() || info->isOutputInCodeGeneration()) {
          return decl;
        }
      }
      return nullptr;
    };
    auto is_include_directive = [](const PreprocessingInfo *info) -> bool {
      if (info == nullptr) {
        return false;
      }
      PreprocessingInfo::DirectiveType type = info->getTypeOfDirective();
      return type == PreprocessingInfo::CpreprocessorIncludeDeclaration ||
             type == PreprocessingInfo::CpreprocessorIncludeNextDeclaration;
    };
    auto is_same_file = [](Sg_File_Info *a, Sg_File_Info *b) -> bool {
      if (a == nullptr || b == nullptr) {
        return false;
      }
      if (a->get_file_id() == b->get_file_id()) {
        return true;
      }
      if (!a->get_filenameString().empty() &&
          !b->get_filenameString().empty()) {
        return a->get_filenameString() == b->get_filenameString();
      }
      return false;
    };
    auto location_leq = [](Sg_File_Info *lhs, Sg_File_Info *rhs) -> bool {
      if (lhs == nullptr || rhs == nullptr) {
        return false;
      }
      if (lhs->get_line() != rhs->get_line()) {
        return lhs->get_line() < rhs->get_line();
      }
      return lhs->get_col() <= rhs->get_col();
    };
    auto node_start = [](SgLocatedNode *node) -> Sg_File_Info * {
      if (node == nullptr) {
        return nullptr;
      }
      Sg_File_Info *info = node->get_file_info();
      if (info != nullptr && info->get_line() > 0) {
        return info;
      }
      Sg_File_Info *start = node->get_startOfConstruct();
      if (start != nullptr && start->get_line() > 0) {
        return start;
      }
      return node->get_file_info();
    };
    auto node_end = [](SgLocatedNode *node) -> Sg_File_Info * {
      if (node == nullptr) {
        return nullptr;
      }
      Sg_File_Info *end = node->get_endOfConstruct();
      if (end != nullptr && end->get_line() > 0) {
        return end;
      }
      return node->get_file_info();
    };
    auto cursor_inside_node = [&](SgLocatedNode *node,
                                  Sg_File_Info *cursor) -> bool {
      if (node == nullptr || cursor == nullptr) {
        return false;
      }
      Sg_File_Info *start = node_start(node);
      Sg_File_Info *end = node_end(node);
      if (start == nullptr || end == nullptr) {
        return false;
      }
      if (!is_same_file(start, cursor)) {
        return false;
      }
      if (!is_same_file(end, cursor)) {
        end = start;
      }
      if (!location_leq(start, end)) {
        std::swap(start, end);
      }
      if (start->get_line() == end->get_line() &&
          start->get_col() == end->get_col()) {
        // Some frontend nodes only carry a start location. Treat these as
        // spanning the rest of their source line so trailing line comments
        // anchor to the statement they follow instead of the next declaration.
        if (!is_same_file(start, cursor) ||
            cursor->get_line() != start->get_line()) {
          return false;
        }
        return cursor->get_col() >= start->get_col();
      }
      return location_leq(start, cursor) && location_leq(cursor, end);
    };
    std::function<SgLocatedNode *(SgLocatedNode *, Sg_File_Info *)>
        find_deepest_anchor;
    find_deepest_anchor = [&](SgLocatedNode *node,
                              Sg_File_Info *cursor) -> SgLocatedNode * {
      if (SgFunctionDeclaration *func_decl = isSgFunctionDeclaration(node)) {
        if (SgFunctionDefinition *defn = func_decl->get_definition()) {
          SgLocatedNode *defn_node = isSgLocatedNode(defn);
          if (cursor_inside_node(defn_node, cursor)) {
            if (SgLocatedNode *nested =
                    find_deepest_anchor(defn_node, cursor)) {
              return nested;
            }
            return defn_node;
          }
          if (SgBasicBlock *body = defn->get_body()) {
            SgLocatedNode *body_node = isSgLocatedNode(body);
            if (cursor_inside_node(body_node, cursor)) {
              if (SgLocatedNode *nested =
                      find_deepest_anchor(body_node, cursor)) {
                return nested;
              }
              return body_node;
            }
          }
        }
      }

      if (!cursor_inside_node(node, cursor)) {
        return nullptr;
      }

      auto descend_stmt_list =
          [&](const SgStatementPtrList &stmts) -> SgLocatedNode * {
        for (SgStatement *stmt : stmts) {
          if (SgLocatedNode *nested =
                  find_deepest_anchor(isSgLocatedNode(stmt), cursor)) {
            return nested;
          }
        }
        return nullptr;
      };
      auto descend_decl_list =
          [&](const SgDeclarationStatementPtrList &decls) -> SgLocatedNode * {
        for (SgDeclarationStatement *decl : decls) {
          if (SgLocatedNode *nested =
                  find_deepest_anchor(isSgLocatedNode(decl), cursor)) {
            return nested;
          }
        }
        return nullptr;
      };

      if (SgFunctionDefinition *defn = isSgFunctionDefinition(node)) {
        if (SgBasicBlock *body = defn->get_body()) {
          if (SgLocatedNode *nested =
                  find_deepest_anchor(isSgLocatedNode(body), cursor)) {
            return nested;
          }
        }
      }
      if (SgBasicBlock *block = isSgBasicBlock(node)) {
        if (SgLocatedNode *nested =
                descend_stmt_list(block->get_statements())) {
          return nested;
        }
      }
      if (SgIfStmt *if_stmt = isSgIfStmt(node)) {
        if (SgLocatedNode *nested = find_deepest_anchor(
                isSgLocatedNode(if_stmt->get_true_body()), cursor)) {
          return nested;
        }
        if (SgLocatedNode *nested = find_deepest_anchor(
                isSgLocatedNode(if_stmt->get_false_body()), cursor)) {
          return nested;
        }
      }
      if (SgSwitchStatement *switch_stmt = isSgSwitchStatement(node)) {
        if (SgLocatedNode *nested = find_deepest_anchor(
                isSgLocatedNode(switch_stmt->get_body()), cursor)) {
          return nested;
        }
      }
      if (SgCaseOptionStmt *case_stmt = isSgCaseOptionStmt(node)) {
        if (SgLocatedNode *nested = find_deepest_anchor(
                isSgLocatedNode(case_stmt->get_body()), cursor)) {
          return nested;
        }
      }
      if (SgDefaultOptionStmt *default_stmt = isSgDefaultOptionStmt(node)) {
        if (SgLocatedNode *nested = find_deepest_anchor(
                isSgLocatedNode(default_stmt->get_body()), cursor)) {
          return nested;
        }
      }
      if (SgNamespaceDeclarationStatement *ns_decl =
              isSgNamespaceDeclarationStatement(node)) {
        if (SgNamespaceDefinitionStatement *ns_def =
                ns_decl->get_definition()) {
          if (SgLocatedNode *nested =
                  find_deepest_anchor(isSgLocatedNode(ns_def), cursor)) {
            return nested;
          }
        }
      }
      if (SgNamespaceDefinitionStatement *ns_def =
              isSgNamespaceDefinitionStatement(node)) {
        if (SgLocatedNode *nested =
                descend_decl_list(ns_def->get_declarations())) {
          return nested;
        }
      }
      if (SgClassDeclaration *class_decl = isSgClassDeclaration(node)) {
        if (SgClassDefinition *class_def = class_decl->get_definition()) {
          if (SgLocatedNode *nested =
                  find_deepest_anchor(isSgLocatedNode(class_def), cursor)) {
            return nested;
          }
        }
      }
      if (SgTemplateClassDeclaration *class_decl =
              isSgTemplateClassDeclaration(node)) {
        if (SgTemplateClassDefinition *class_def =
                isSgTemplateClassDefinition(class_decl->get_definition())) {
          if (SgLocatedNode *nested =
                  find_deepest_anchor(isSgLocatedNode(class_def), cursor)) {
            return nested;
          }
        }
      }
      if (SgClassDefinition *class_def = isSgClassDefinition(node)) {
        if (SgLocatedNode *nested =
                descend_decl_list(class_def->get_members())) {
          return nested;
        }
      }
      if (SgTemplateClassDefinition *class_def =
              isSgTemplateClassDefinition(node)) {
        if (SgLocatedNode *nested =
                descend_decl_list(class_def->get_members())) {
          return nested;
        }
      }

      return node;
    };
    auto find_anchor_for_cursor = [&](Sg_File_Info *cursor) -> SgLocatedNode * {
      if (cursor == nullptr || global_scope == nullptr) {
        return nullptr;
      }
      for (SgDeclarationStatement *decl : global_scope->get_declarations()) {
        if (SgLocatedNode *anchor =
                find_deepest_anchor(isSgLocatedNode(decl), cursor)) {
          return anchor;
        }
      }
      return nullptr;
    };
    auto is_comment_directive = [](const PreprocessingInfo *info) -> bool {
      if (info == nullptr) {
        return false;
      }
      PreprocessingInfo::DirectiveType type = info->getTypeOfDirective();
      return type == PreprocessingInfo::C_StyleComment ||
             type == PreprocessingInfo::CplusplusStyleComment;
    };
    auto find_sibling_anchor_after_cursor =
        [&](SgLocatedNode *anchor, Sg_File_Info *cursor) -> SgLocatedNode * {
      if (anchor == nullptr || cursor == nullptr) {
        return nullptr;
      }

      auto better_anchor = [&](SgLocatedNode *lhs, SgLocatedNode *rhs) -> bool {
        if (lhs == nullptr) {
          return false;
        }
        if (rhs == nullptr) {
          return true;
        }
        return location_leq(node_start(lhs), node_start(rhs));
      };

      auto consider = [&](SgLocatedNode *candidate, SgLocatedNode *&best) {
        if (candidate == nullptr) {
          return;
        }
        Sg_File_Info *candidate_start = node_start(candidate);
        if (candidate_start == nullptr || candidate_start->get_line() <= 0) {
          return;
        }
        if (!is_same_file(candidate_start, cursor)) {
          return;
        }
        if (!location_leq(cursor, candidate_start)) {
          return;
        }
        if (better_anchor(candidate, best)) {
          best = candidate;
        }
      };

      SgLocatedNode *best = nullptr;
      SgNode *parent = anchor->get_parent();
      if (SgBasicBlock *block = isSgBasicBlock(parent)) {
        for (SgStatement *stmt : block->get_statements()) {
          consider(isSgLocatedNode(stmt), best);
        }
      } else if (SgGlobal *scope = isSgGlobal(parent)) {
        for (SgDeclarationStatement *decl : scope->get_declarations()) {
          consider(isSgLocatedNode(decl), best);
        }
      } else if (SgNamespaceDefinitionStatement *scope =
                     isSgNamespaceDefinitionStatement(parent)) {
        for (SgDeclarationStatement *decl : scope->get_declarations()) {
          consider(isSgLocatedNode(decl), best);
        }
      } else if (SgClassDefinition *scope = isSgClassDefinition(parent)) {
        for (SgDeclarationStatement *decl : scope->get_members()) {
          consider(isSgLocatedNode(decl), best);
        }
      } else if (SgTemplateClassDefinition *scope =
                     isSgTemplateClassDefinition(parent)) {
        for (SgDeclarationStatement *decl : scope->get_members()) {
          consider(isSgLocatedNode(decl), best);
        }
      }
      return best;
    };

    SgStatement *attach_target = find_last_output_stmt(global_scope);
    SgStatement *first_output_stmt = find_first_output_stmt(global_scope);
    auto find_output_stmt_before_cursor =
        [&](Sg_File_Info *cursor) -> SgStatement * {
      if (cursor == nullptr || cursor->get_line() <= 0 ||
          global_scope == nullptr) {
        return nullptr;
      }

      SgStatement *candidate = nullptr;
      for (SgDeclarationStatement *decl : global_scope->get_declarations()) {
        if (decl == nullptr) {
          continue;
        }
        Sg_File_Info *info = decl->get_file_info();
        if (info == nullptr || info->get_line() <= 0) {
          continue;
        }
        if (!is_from_main_file(info)) {
          continue;
        }
        if (info->isCompilerGenerated() && !info->isOutputInCodeGeneration()) {
          continue;
        }
        if (info->get_line() <= cursor->get_line()) {
          candidate = decl;
          continue;
        }
        break;
      }
      return candidate;
    };
    if (attach_target == nullptr) {
      attach_target = global_scope;
    }

    while (translator.preprocessor_list_size() > 0) {
      std::pair<Sg_File_Info *, PreprocessingInfo *> entry =
          translator.preprocessor_top();
      if (entry.second != nullptr) {
        Sg_File_Info *cursor = entry.first;
        const bool is_include = is_include_directive(entry.second);
        SgLocatedNode *anchor = nullptr;
        bool has_forced_anchor = false;
        PreprocessingInfo::RelativePositionType forced_relative_position =
            PreprocessingInfo::after;

        if (!is_include && cursor != nullptr) {
          if (SgLocatedNode *first_loc = isSgLocatedNode(first_output_stmt)) {
            if (Sg_File_Info *first_start = node_start(first_loc);
                first_start != nullptr && first_start->get_line() > 0 &&
                is_same_file(first_start, cursor) &&
                location_leq(cursor, first_start) &&
                !location_leq(first_start, cursor)) {
              anchor = first_loc;
              has_forced_anchor = true;
              forced_relative_position = PreprocessingInfo::before;
            }
          }

          if (!has_forced_anchor) {
            if (SgLocatedNode *last_loc = isSgLocatedNode(attach_target)) {
              if (Sg_File_Info *last_end = node_end(last_loc);
                  last_end != nullptr && last_end->get_line() > 0 &&
                  is_same_file(last_end, cursor) &&
                  location_leq(last_end, cursor) &&
                  !location_leq(cursor, last_end)) {
                anchor = last_loc;
                has_forced_anchor = true;
                forced_relative_position = PreprocessingInfo::after;
              }
            }
          }
        }

        if (!has_forced_anchor) {
          anchor = find_anchor_for_cursor(cursor);
        }
        if (anchor != nullptr) {
          if (has_forced_anchor) {
            entry.second->setRelativePosition(forced_relative_position);
            anchor->addToAttachedPreprocessingInfo(entry.second);
            goto inserted_preproc;
          }
          if (is_comment_directive(entry.second) && entry.first != nullptr) {
            Sg_File_Info *anchor_start = node_start(anchor);
            if (anchor_start != nullptr && anchor_start->get_line() > 0 &&
                is_same_file(anchor_start, entry.first) &&
                location_leq(entry.first, anchor_start) &&
                !location_leq(anchor_start, entry.first)) {
              if (SgLocatedNode *better_anchor =
                      find_sibling_anchor_after_cursor(anchor, entry.first)) {
                anchor = better_anchor;
              }
            }
          }

          Sg_File_Info *start = node_start(anchor);
          Sg_File_Info *end = node_end(anchor);
          if (isSgBasicBlock(anchor) != nullptr ||
              isSgClassDefinition(anchor) != nullptr ||
              isSgTemplateClassDefinition(anchor) != nullptr ||
              isSgNamespaceDefinitionStatement(anchor) != nullptr) {
            entry.second->setRelativePosition(PreprocessingInfo::inside);
          } else if (isSgNamespaceDeclarationStatement(anchor) != nullptr ||
                     isSgClassDeclaration(anchor) != nullptr ||
                     isSgTemplateClassDeclaration(anchor) != nullptr) {
            if (start != nullptr && end != nullptr &&
                cursor_inside_node(anchor, entry.first) &&
                !location_leq(entry.first, start) &&
                !location_leq(end, entry.first)) {
              entry.second->setRelativePosition(PreprocessingInfo::inside);
            } else if (start != nullptr && location_leq(entry.first, start)) {
              entry.second->setRelativePosition(PreprocessingInfo::before);
            } else {
              entry.second->setRelativePosition(PreprocessingInfo::after);
            }
          } else if (start != nullptr && location_leq(entry.first, start)) {
            entry.second->setRelativePosition(PreprocessingInfo::before);
          } else {
            entry.second->setRelativePosition(PreprocessingInfo::after);
          }
          anchor->addToAttachedPreprocessingInfo(entry.second);
        } else {
          if (is_include_directive(entry.second)) {
            SgStatement *before_stmt =
                find_output_stmt_before_cursor(entry.first);
            if (before_stmt != nullptr) {
              entry.second->setRelativePosition(PreprocessingInfo::after);
              before_stmt->addToAttachedPreprocessingInfo(entry.second);
            } else if (first_output_stmt != nullptr) {
              entry.second->setRelativePosition(PreprocessingInfo::before);
              first_output_stmt->addToAttachedPreprocessingInfo(entry.second);
            } else {
              entry.second->setRelativePosition(PreprocessingInfo::after);
              attach_target->addToAttachedPreprocessingInfo(entry.second);
            }
          } else {
            entry.second->setRelativePosition(PreprocessingInfo::after);
            attach_target->addToAttachedPreprocessingInfo(entry.second);
          }
        }
      }
    inserted_preproc:
      if (!translator.preprocessor_pop()) {
        break;
      }
    }
  }

  if (global_scope != nullptr) {
    auto get_effective_info = [](SgLocatedNode *node) -> Sg_File_Info * {
      if (node == nullptr) {
        return nullptr;
      }
      if (Sg_File_Info *info = node->get_startOfConstruct()) {
        if (info->get_line() > 0) {
          return info;
        }
      }
      if (Sg_File_Info *info = node->get_file_info()) {
        if (info->get_line() > 0) {
          return info;
        }
      }
      return node->get_endOfConstruct();
    };

    auto same_file = [](const Sg_File_Info *lhs, const Sg_File_Info *rhs) {
      if (lhs == nullptr || rhs == nullptr) {
        return false;
      }
      if (lhs->get_file_id() == rhs->get_file_id()) {
        return true;
      }
      return !lhs->get_filenameString().empty() &&
             lhs->get_filenameString() == rhs->get_filenameString();
    };

    auto source_before_or_equal = [](const Sg_File_Info *lhs,
                                     const Sg_File_Info *rhs) {
      if (lhs == nullptr || rhs == nullptr) {
        return false;
      }
      if (lhs->get_line() != rhs->get_line()) {
        return lhs->get_line() < rhs->get_line();
      }
      return lhs->get_col() <= rhs->get_col();
    };

    auto is_class_conditional_directive =
        [](PreprocessingInfo::DirectiveType type) {
          return type == PreprocessingInfo::CpreprocessorIfDeclaration ||
                 type == PreprocessingInfo::CpreprocessorIfdefDeclaration ||
                 type == PreprocessingInfo::CpreprocessorIfndefDeclaration ||
                 type == PreprocessingInfo::CpreprocessorElifDeclaration ||
                 type == PreprocessingInfo::CpreprocessorElseDeclaration ||
                 type == PreprocessingInfo::CpreprocessorEndifDeclaration;
        };
    auto is_class_conditional_payload =
        [&](PreprocessingInfo::DirectiveType type) {
          return is_class_conditional_directive(type) ||
                 type == PreprocessingInfo::CpreprocessorDeadIfDeclaration ||
                 type == PreprocessingInfo::CSkippedToken;
        };

    auto relocate_class_conditionals = [&](SgLocatedNode *class_anchor,
                                           const SgDeclarationStatementPtrList
                                               &members,
                                           SgLocatedNode *class_decl_owner) {
      if (class_anchor == nullptr || members.empty()) {
        return;
      }

      Sg_File_Info *class_start = class_anchor->get_startOfConstruct();
      Sg_File_Info *class_end = class_anchor->get_endOfConstruct();
      if (class_start == nullptr || class_end == nullptr ||
          class_start->get_line() <= 0 || class_end->get_line() <= 0) {
        return;
      }
      if (!same_file(class_start, class_end)) {
        return;
      }

      struct MemberAnchor {
        SgDeclarationStatement *decl = nullptr;
        Sg_File_Info *info = nullptr;
      };
      std::vector<MemberAnchor> member_anchors;
      member_anchors.reserve(members.size());
      for (SgDeclarationStatement *member : members) {
        SgLocatedNode *member_loc = isSgLocatedNode(member);
        if (member_loc == nullptr) {
          continue;
        }
        Sg_File_Info *member_info = get_effective_info(member_loc);
        if (member_info == nullptr || member_info->get_line() <= 0) {
          continue;
        }
        if (member_info->isCompilerGenerated() &&
            !member_info->isOutputInCodeGeneration()) {
          continue;
        }
        if (!same_file(member_info, class_start)) {
          continue;
        }
        member_anchors.push_back({member, member_info});
      }

      std::stable_sort(member_anchors.begin(), member_anchors.end(),
                       [&](const MemberAnchor &lhs, const MemberAnchor &rhs) {
                         if (lhs.info == nullptr || rhs.info == nullptr) {
                           return lhs.info < rhs.info;
                         }
                         if (lhs.info->get_line() != rhs.info->get_line()) {
                           return lhs.info->get_line() < rhs.info->get_line();
                         }
                         if (lhs.info->get_col() != rhs.info->get_col()) {
                           return lhs.info->get_col() < rhs.info->get_col();
                         }
                         return lhs.decl < rhs.decl;
                       });

      if (member_anchors.empty()) {
        return;
      }

      std::vector<SgLocatedNode *> directive_owners;
      directive_owners.reserve(members.size() + 2);
      directive_owners.push_back(class_anchor);
      if (class_decl_owner != nullptr && class_decl_owner != class_anchor) {
        directive_owners.push_back(class_decl_owner);
      }
      for (SgDeclarationStatement *member : members) {
        if (member == nullptr) {
          continue;
        }
        // Nested classes are relocated in their own pass; keep outer-class
        // relocation focused on direct non-class members.
        if (isSgClassDeclaration(member) != nullptr ||
            isSgTemplateClassDeclaration(member) != nullptr) {
          continue;
        }
        if (SgLocatedNode *member_loc = isSgLocatedNode(member)) {
          directive_owners.push_back(member_loc);
        }
      }

      struct PendingMove {
        PreprocessingInfo *info = nullptr;
        SgLocatedNode *anchor = nullptr;
        PreprocessingInfo::RelativePositionType relative_position =
            PreprocessingInfo::after;
      };

      std::vector<PendingMove> moves;

      auto pick_member_before =
          [&](const Sg_File_Info *cursor) -> SgDeclarationStatement * {
        SgDeclarationStatement *best = nullptr;
        Sg_File_Info *best_info = nullptr;
        for (const MemberAnchor &entry : member_anchors) {
          if (!source_before_or_equal(entry.info, cursor)) {
            continue;
          }
          if (best_info == nullptr ||
              source_before_or_equal(best_info, entry.info)) {
            best = entry.decl;
            best_info = entry.info;
          }
        }
        return best;
      };

      auto pick_member_after =
          [&](const Sg_File_Info *cursor) -> SgDeclarationStatement * {
        SgDeclarationStatement *best = nullptr;
        Sg_File_Info *best_info = nullptr;
        for (const MemberAnchor &entry : member_anchors) {
          if (source_before_or_equal(entry.info, cursor)) {
            continue;
          }
          if (best_info == nullptr ||
              source_before_or_equal(entry.info, best_info)) {
            best = entry.decl;
            best_info = entry.info;
          }
        }
        return best;
      };

      auto member_containing_directive =
          [&](const Sg_File_Info *cursor) -> SgDeclarationStatement * {
        if (cursor == nullptr || cursor->get_line() <= 0) {
          return nullptr;
        }
        for (size_t i = 0; i < member_anchors.size(); ++i) {
          const MemberAnchor &entry = member_anchors[i];
          SgLocatedNode *member_loc = isSgLocatedNode(entry.decl);
          if (member_loc == nullptr) {
            continue;
          }
          Sg_File_Info *member_start = get_effective_info(member_loc);
          Sg_File_Info *member_end = member_loc->get_endOfConstruct();
          if (member_start == nullptr || member_end == nullptr ||
              member_start->get_line() <= 0 || member_end->get_line() <= 0) {
            continue;
          }
          if (!same_file(member_start, cursor) ||
              !same_file(member_end, cursor)) {
            continue;
          }
          if (source_before_or_equal(member_start, cursor) &&
              source_before_or_equal(cursor, member_end)) {
            return entry.decl;
          }

          SgFunctionDeclaration *func_decl =
              isSgFunctionDeclaration(entry.decl);
          if (func_decl == nullptr || func_decl->get_definition() == nullptr ||
              func_decl->get_definition()->get_body() == nullptr) {
            continue;
          }

          SgBasicBlock *body = func_decl->get_definition()->get_body();
          Sg_File_Info *decl_start = get_effective_info(func_decl);
          Sg_File_Info *body_start = get_effective_info(body);
          Sg_File_Info *body_end = body->get_endOfConstruct();
          if (decl_start == nullptr || body_start == nullptr ||
              body_end == nullptr || decl_start->get_line() <= 0 ||
              body_start->get_line() <= 0 || body_end->get_line() <= 0 ||
              !same_file(decl_start, cursor) ||
              !same_file(body_start, cursor) || !same_file(body_end, cursor)) {
            continue;
          }

          if (!source_before_or_equal(cursor, decl_start) &&
              source_before_or_equal(cursor, body_start) &&
              !source_before_or_equal(body_start, cursor)) {
            return entry.decl;
          }

          if (!source_before_or_equal(cursor, body_start) &&
              !source_before_or_equal(body_end, cursor)) {
            return entry.decl;
          }
        }
        return nullptr;
      };

      for (SgLocatedNode *owner : directive_owners) {
        if (owner == nullptr) {
          continue;
        }
        AttachedPreprocessingInfoType *attached =
            owner->getAttachedPreprocessingInfo();
        if (attached == nullptr || attached->empty()) {
          continue;
        }

        for (auto it = attached->begin(); it != attached->end();) {
          PreprocessingInfo *info = *it;
          if (info == nullptr ||
              !is_class_conditional_payload(info->getTypeOfDirective())) {
            ++it;
            continue;
          }

          Sg_File_Info *info_fi = info->get_file_info();
          if (info_fi == nullptr || info_fi->get_line() <= 0) {
            ++it;
            continue;
          }
          if (!same_file(info_fi, class_start)) {
            ++it;
            continue;
          }
          SgLocatedNode *anchor = nullptr;
          PreprocessingInfo::RelativePositionType relative_position =
              PreprocessingInfo::after;
          if (SgDeclarationStatement *member =
                  member_containing_directive(info_fi)) {
            if (SgFunctionDeclaration *func_decl =
                    isSgFunctionDeclaration(member)) {
              if (SgFunctionDefinition *func_def =
                      func_decl->get_definition()) {
                SgBasicBlock *body = func_def->get_body();
                Sg_File_Info *decl_start = get_effective_info(func_decl);
                Sg_File_Info *body_start = get_effective_info(body);
                Sg_File_Info *body_end =
                    body != nullptr ? body->get_endOfConstruct() : nullptr;
                if (decl_start != nullptr && body_start != nullptr &&
                    body_end != nullptr && decl_start->get_line() > 0 &&
                    body_start->get_line() > 0 && body_end->get_line() > 0 &&
                    same_file(decl_start, info_fi) &&
                    same_file(body_start, info_fi) &&
                    same_file(body_end, info_fi)) {
                  if (!source_before_or_equal(info_fi, decl_start) &&
                      source_before_or_equal(info_fi, body_start) &&
                      !source_before_or_equal(body_start, info_fi)) {
                    anchor = func_def;
                    relative_position = PreprocessingInfo::before;
                  } else if (source_before_or_equal(body_end, info_fi) &&
                             !source_before_or_equal(info_fi, body_end)) {
                    anchor = func_def;
                    relative_position = PreprocessingInfo::after;
                  } else if (!source_before_or_equal(info_fi, body_start) &&
                             !source_before_or_equal(body_end, info_fi)) {
                    anchor = body;
                    relative_position = PreprocessingInfo::inside;
                  }
                }
              }
            }

            if (anchor == nullptr) {
              ++it;
              continue;
            }
          } else {
            const bool inside_class_range =
                source_before_or_equal(class_start, info_fi) &&
                source_before_or_equal(info_fi, class_end);
            if (!inside_class_range) {
              ++it;
              continue;
            }

            SgDeclarationStatement *member_before = pick_member_before(info_fi);
            SgDeclarationStatement *member_after = pick_member_after(info_fi);

            PreprocessingInfo::DirectiveType type = info->getTypeOfDirective();
            if (type == PreprocessingInfo::CpreprocessorElseDeclaration ||
                type == PreprocessingInfo::CpreprocessorElifDeclaration) {
              if (member_after != nullptr) {
                anchor = member_after;
                relative_position = PreprocessingInfo::before;
              } else {
                anchor = member_before;
                relative_position = PreprocessingInfo::after;
              }
            } else if (type ==
                       PreprocessingInfo::CpreprocessorEndifDeclaration) {
              if (member_after != nullptr) {
                anchor = member_after;
                relative_position = PreprocessingInfo::before;
              } else if (member_before != nullptr) {
                anchor = member_before;
                relative_position = PreprocessingInfo::after;
              }
            } else {
              if (member_after != nullptr) {
                anchor = member_after;
                relative_position = PreprocessingInfo::before;
              } else {
                anchor = member_before;
                relative_position = PreprocessingInfo::after;
              }
            }

            if (anchor == nullptr) {
              ++it;
              continue;
            }
          }
          moves.push_back({info, anchor, relative_position});
          it = attached->erase(it);
        }
      }

      if (moves.empty()) {
        return;
      }

      std::stable_sort(
          moves.begin(), moves.end(),
          [](const PendingMove &lhs, const PendingMove &rhs) {
            Sg_File_Info *lhs_info =
                lhs.info != nullptr ? lhs.info->get_file_info() : nullptr;
            Sg_File_Info *rhs_info =
                rhs.info != nullptr ? rhs.info->get_file_info() : nullptr;
            if (lhs_info == nullptr || rhs_info == nullptr) {
              return lhs_info < rhs_info;
            }
            if (lhs_info->get_line() != rhs_info->get_line()) {
              return lhs_info->get_line() < rhs_info->get_line();
            }
            if (lhs_info->get_col() != rhs_info->get_col()) {
              return lhs_info->get_col() < rhs_info->get_col();
            }
            return lhs.info < rhs.info;
          });

      for (const PendingMove &move : moves) {
        if (move.info == nullptr || move.anchor == nullptr) {
          continue;
        }
        move.info->setRelativePosition(move.relative_position);
        move.anchor->addToAttachedPreprocessingInfo(move.info);
      }
    };

    Rose_STL_Container<SgNode *> class_definition_nodes =
        NodeQuery::querySubTree(global_scope, V_SgClassDefinition);

    Rose_STL_Container<SgNode *> template_class_definition_nodes =
        NodeQuery::querySubTree(global_scope, V_SgTemplateClassDefinition);

    for (SgNode *node : class_definition_nodes) {
      SgClassDefinition *def = isSgClassDefinition(node);
      if (def == nullptr) {
        continue;
      }
      relocate_class_conditionals(def, def->get_members(),
                                  def->get_declaration());
    }

    for (SgNode *node : template_class_definition_nodes) {
      SgTemplateClassDefinition *def = isSgTemplateClassDefinition(node);
      if (def == nullptr) {
        continue;
      }
      relocate_class_conditionals(def, def->get_members(),
                                  def->get_declaration());
    }

    auto is_conditional_directive = [](PreprocessingInfo::DirectiveType type) {
      return type == PreprocessingInfo::CpreprocessorIfDeclaration ||
             type == PreprocessingInfo::CpreprocessorIfdefDeclaration ||
             type == PreprocessingInfo::CpreprocessorIfndefDeclaration ||
             type == PreprocessingInfo::CpreprocessorElifDeclaration ||
             type == PreprocessingInfo::CpreprocessorElseDeclaration ||
             type == PreprocessingInfo::CpreprocessorEndifDeclaration;
    };

    auto relocate_scope_conditionals =
        [&](SgLocatedNode *owner,
            const SgDeclarationStatementPtrList &declarations) {
          if (owner == nullptr || declarations.empty()) {
            return;
          }

          AttachedPreprocessingInfoType *attached =
              owner->getAttachedPreprocessingInfo();
          if (attached == nullptr || attached->empty()) {
            return;
          }

          Sg_File_Info *owner_info = get_effective_info(owner);
          Sg_File_Info *owner_end = owner->get_endOfConstruct();
          if (owner_info == nullptr || owner_end == nullptr ||
              owner_info->get_line() <= 0 || owner_end->get_line() <= 0) {
            return;
          }
          if (!same_file(owner_info, owner_end)) {
            owner_end = owner_info;
          }
          if (!source_before_or_equal(owner_info, owner_end)) {
            std::swap(owner_info, owner_end);
          }

          struct DeclAnchor {
            SgDeclarationStatement *decl = nullptr;
            Sg_File_Info *info = nullptr;
          };

          std::vector<DeclAnchor> anchors;
          anchors.reserve(declarations.size());
          for (SgDeclarationStatement *decl : declarations) {
            if (decl == nullptr) {
              continue;
            }
            SgLocatedNode *decl_loc = isSgLocatedNode(decl);
            if (decl_loc == nullptr) {
              continue;
            }
            Sg_File_Info *decl_info = get_effective_info(decl_loc);
            if (decl_info == nullptr || decl_info->get_line() <= 0) {
              continue;
            }
            if (decl_info->isCompilerGenerated() &&
                !decl_info->isOutputInCodeGeneration()) {
              continue;
            }
            if (!same_file(owner_info, decl_info)) {
              continue;
            }
            anchors.push_back({decl, decl_info});
          }

          if (anchors.empty()) {
            return;
          }

          auto pick_before =
              [&](const Sg_File_Info *cursor) -> SgDeclarationStatement * {
            SgDeclarationStatement *best = nullptr;
            Sg_File_Info *best_info = nullptr;
            for (const DeclAnchor &entry : anchors) {
              if (!source_before_or_equal(entry.info, cursor)) {
                continue;
              }
              if (best_info == nullptr ||
                  source_before_or_equal(best_info, entry.info)) {
                best = entry.decl;
                best_info = entry.info;
              }
            }
            return best;
          };

          auto pick_after =
              [&](const Sg_File_Info *cursor) -> SgDeclarationStatement * {
            SgDeclarationStatement *best = nullptr;
            Sg_File_Info *best_info = nullptr;
            for (const DeclAnchor &entry : anchors) {
              if (source_before_or_equal(entry.info, cursor)) {
                continue;
              }
              if (best_info == nullptr ||
                  source_before_or_equal(entry.info, best_info)) {
                best = entry.decl;
                best_info = entry.info;
              }
            }
            return best;
          };

          auto directive_inside_declaration =
              [&](const Sg_File_Info *cursor) -> bool {
            if (cursor == nullptr || cursor->get_line() <= 0) {
              return false;
            }
            for (const DeclAnchor &entry : anchors) {
              SgLocatedNode *decl_loc = isSgLocatedNode(entry.decl);
              if (decl_loc == nullptr) {
                continue;
              }
              Sg_File_Info *decl_start = get_effective_info(decl_loc);
              Sg_File_Info *decl_end = decl_loc->get_endOfConstruct();
              if (decl_start == nullptr || decl_end == nullptr ||
                  decl_start->get_line() <= 0 || decl_end->get_line() <= 0) {
                continue;
              }
              if (!same_file(decl_start, cursor) ||
                  !same_file(decl_end, cursor)) {
                continue;
              }
              if (source_before_or_equal(decl_start, cursor) &&
                  source_before_or_equal(cursor, decl_end)) {
                return true;
              }
            }
            return false;
          };

          struct PendingMove {
            PreprocessingInfo *info = nullptr;
            SgDeclarationStatement *anchor = nullptr;
            PreprocessingInfo::RelativePositionType relative_position =
                PreprocessingInfo::after;
          };

          std::vector<PendingMove> moves;
          for (auto it = attached->begin(); it != attached->end();) {
            PreprocessingInfo *info = *it;
            if (info == nullptr ||
                !is_conditional_directive(info->getTypeOfDirective())) {
              ++it;
              continue;
            }

            Sg_File_Info *directive_info = info->get_file_info();
            if (directive_info == nullptr || directive_info->get_line() <= 0) {
              ++it;
              continue;
            }
            if (!same_file(owner_info, directive_info)) {
              ++it;
              continue;
            }
            const bool directive_inside_owner =
                source_before_or_equal(owner_info, directive_info) &&
                source_before_or_equal(directive_info, owner_end);
            if (!directive_inside_owner) {
              ++it;
              continue;
            }
            if (directive_inside_declaration(directive_info)) {
              ++it;
              continue;
            }

            SgDeclarationStatement *before = pick_before(directive_info);
            SgDeclarationStatement *after = pick_after(directive_info);

            SgDeclarationStatement *anchor = nullptr;
            PreprocessingInfo::RelativePositionType relative_position =
                PreprocessingInfo::after;

            PreprocessingInfo::DirectiveType type = info->getTypeOfDirective();
            if (type == PreprocessingInfo::CpreprocessorIfDeclaration ||
                type == PreprocessingInfo::CpreprocessorIfdefDeclaration ||
                type == PreprocessingInfo::CpreprocessorIfndefDeclaration) {
              if (after != nullptr) {
                anchor = after;
                relative_position = PreprocessingInfo::before;
              } else if (before != nullptr) {
                anchor = before;
                relative_position = PreprocessingInfo::after;
              }
            } else {
              if (before != nullptr) {
                anchor = before;
                relative_position = PreprocessingInfo::after;
              } else if (after != nullptr) {
                anchor = after;
                relative_position = PreprocessingInfo::before;
              }
            }

            if (anchor == nullptr) {
              ++it;
              continue;
            }

            moves.push_back({info, anchor, relative_position});
            it = attached->erase(it);
          }

          if (moves.empty()) {
            return;
          }

          std::stable_sort(
              moves.begin(), moves.end(),
              [](const PendingMove &lhs, const PendingMove &rhs) {
                Sg_File_Info *lhs_info =
                    lhs.info != nullptr ? lhs.info->get_file_info() : nullptr;
                Sg_File_Info *rhs_info =
                    rhs.info != nullptr ? rhs.info->get_file_info() : nullptr;
                if (lhs_info == nullptr || rhs_info == nullptr) {
                  return lhs_info < rhs_info;
                }
                if (lhs_info->get_line() != rhs_info->get_line()) {
                  return lhs_info->get_line() < rhs_info->get_line();
                }
                if (lhs_info->get_col() != rhs_info->get_col()) {
                  return lhs_info->get_col() < rhs_info->get_col();
                }
                return lhs.info < rhs.info;
              });

          for (const PendingMove &move : moves) {
            if (move.info == nullptr || move.anchor == nullptr) {
              continue;
            }
            move.info->setRelativePosition(move.relative_position);
            move.anchor->addToAttachedPreprocessingInfo(move.info);
          }
        };
    auto is_opening_conditional_directive =
        [](PreprocessingInfo::DirectiveType type) {
          return type == PreprocessingInfo::CpreprocessorIfDeclaration ||
                 type == PreprocessingInfo::CpreprocessorIfdefDeclaration ||
                 type == PreprocessingInfo::CpreprocessorIfndefDeclaration;
        };
    auto is_structural_statement_scope = [](SgLocatedNode *node) {
      return node != nullptr &&
             (isSgBasicBlock(node) != nullptr || isSgIfStmt(node) != nullptr ||
              isSgSwitchStatement(node) != nullptr ||
              isSgCaseOptionStmt(node) != nullptr ||
              isSgDefaultOptionStmt(node) != nullptr ||
              isSgForStatement(node) != nullptr ||
              isSgRangeBasedForStatement(node) != nullptr ||
              isSgWhileStmt(node) != nullptr ||
              isSgDoWhileStmt(node) != nullptr ||
              isSgCatchOptionStmt(node) != nullptr);
    };
    auto find_best_scope_in_statement_subtree =
        [&](SgStatement *root, Sg_File_Info *cursor,
            bool opening) -> SgLocatedNode * {
      if (root == nullptr || cursor == nullptr || cursor->get_line() <= 0) {
        return nullptr;
      }

      Rose_STL_Container<SgNode *> nodes =
          NodeQuery::querySubTree(root, V_SgLocatedNode);
      SgLocatedNode *best = nullptr;
      Sg_File_Info *best_start = nullptr;
      Sg_File_Info *best_end = nullptr;

      for (SgNode *node : nodes) {
        SgLocatedNode *candidate = isSgLocatedNode(node);
        if (candidate == nullptr || !is_structural_statement_scope(candidate)) {
          continue;
        }

        Sg_File_Info *start = get_effective_info(candidate);
        Sg_File_Info *end = candidate->get_endOfConstruct();
        if (start == nullptr || end == nullptr || start->get_line() <= 0 ||
            end->get_line() <= 0) {
          continue;
        }
        if (!same_file(start, cursor) || !same_file(end, cursor)) {
          continue;
        }
        if (!source_before_or_equal(start, end)) {
          std::swap(start, end);
        }

        bool matches = false;
        if (opening) {
          matches = source_before_or_equal(start, cursor) &&
                    source_before_or_equal(cursor, end);
        } else {
          matches = source_before_or_equal(end, cursor);
        }
        if (!matches) {
          continue;
        }

        if (best == nullptr) {
          best = candidate;
          best_start = start;
          best_end = end;
          continue;
        }

        const int candidate_span = end->get_line() - start->get_line();
        const int best_span = best_end->get_line() - best_start->get_line();
        if (candidate_span < best_span ||
            (candidate_span == best_span &&
             source_before_or_equal(best_start, start))) {
          best = candidate;
          best_start = start;
          best_end = end;
        }
      }

      return best;
    };
    auto relocate_basic_block_conditionals = [&](SgBasicBlock *block) {
      if (block == nullptr) {
        return;
      }

      SgStatementPtrList &statements = block->get_statements();
      if (statements.size() < 2) {
        return;
      }

      for (size_t i = 1; i < statements.size(); ++i) {
        SgStatement *previous = statements[i - 1];
        SgStatement *current = statements[i];
        SgLocatedNode *current_loc = isSgLocatedNode(current);
        if (previous == nullptr || current_loc == nullptr) {
          continue;
        }

        AttachedPreprocessingInfoType *attached =
            current_loc->getAttachedPreprocessingInfo();
        if (attached == nullptr || attached->empty()) {
          continue;
        }

        Sg_File_Info *current_start = get_effective_info(current_loc);
        if (current_start == nullptr || current_start->get_line() <= 0) {
          continue;
        }

        struct PendingMove {
          PreprocessingInfo *info = nullptr;
          SgLocatedNode *anchor = nullptr;
          PreprocessingInfo::RelativePositionType relative_position =
              PreprocessingInfo::after;
        };

        std::vector<PendingMove> moves;
        for (auto it = attached->begin(); it != attached->end();) {
          PreprocessingInfo *info = *it;
          if (info == nullptr ||
              info->getRelativePosition() != PreprocessingInfo::before ||
              !is_conditional_directive(info->getTypeOfDirective())) {
            ++it;
            continue;
          }

          Sg_File_Info *directive_info = info->get_file_info();
          if (directive_info == nullptr || directive_info->get_line() <= 0 ||
              !same_file(current_start, directive_info) ||
              !source_before_or_equal(directive_info, current_start) ||
              source_before_or_equal(current_start, directive_info)) {
            ++it;
            continue;
          }

          const bool opening =
              is_opening_conditional_directive(info->getTypeOfDirective());
          SgLocatedNode *anchor = find_best_scope_in_statement_subtree(
              previous, directive_info, opening);
          if (anchor == nullptr) {
            ++it;
            continue;
          }

          moves.push_back(
              {info, anchor,
               opening ? PreprocessingInfo::inside : PreprocessingInfo::after});
          it = attached->erase(it);
        }

        if (moves.empty()) {
          continue;
        }

        std::stable_sort(
            moves.begin(), moves.end(),
            [](const PendingMove &lhs, const PendingMove &rhs) {
              Sg_File_Info *lhs_info =
                  lhs.info != nullptr ? lhs.info->get_file_info() : nullptr;
              Sg_File_Info *rhs_info =
                  rhs.info != nullptr ? rhs.info->get_file_info() : nullptr;
              if (lhs_info == nullptr || rhs_info == nullptr) {
                return lhs_info < rhs_info;
              }
              if (lhs_info->get_line() != rhs_info->get_line()) {
                return lhs_info->get_line() < rhs_info->get_line();
              }
              if (lhs_info->get_col() != rhs_info->get_col()) {
                return lhs_info->get_col() < rhs_info->get_col();
              }
              return lhs.info < rhs.info;
            });

        for (const PendingMove &move : moves) {
          if (move.info == nullptr || move.anchor == nullptr) {
            continue;
          }
          move.info->setRelativePosition(move.relative_position);
          move.anchor->addToAttachedPreprocessingInfo(move.info);
        }
      }
    };

    // Keep preprocessing attachment conservative. The generic relocation passes
    // above were moving balanced `#if/#else/#endif` structure away from the
    // declaration/member it lexically encloses, which in turn caused the
    // unparser to duplicate active-branch source from raw gaps.
  }

  if (global_scope != nullptr) {
    auto is_pp_conditional_directive =
        [](PreprocessingInfo::DirectiveType type) {
          return type == PreprocessingInfo::CpreprocessorIfDeclaration ||
                 type == PreprocessingInfo::CpreprocessorIfdefDeclaration ||
                 type == PreprocessingInfo::CpreprocessorIfndefDeclaration ||
                 type == PreprocessingInfo::CpreprocessorElifDeclaration ||
                 type == PreprocessingInfo::CpreprocessorElseDeclaration ||
                 type == PreprocessingInfo::CpreprocessorEndifDeclaration;
        };

    auto conditional_source_less = [](const PreprocessingInfo *lhs,
                                      const PreprocessingInfo *rhs) {
      if (lhs == nullptr || rhs == nullptr) {
        return lhs < rhs;
      }
      Sg_File_Info *lhs_info = lhs->get_file_info();
      Sg_File_Info *rhs_info = rhs->get_file_info();
      if (lhs_info == nullptr || rhs_info == nullptr) {
        return lhs_info < rhs_info;
      }
      if (lhs_info->get_line() != rhs_info->get_line()) {
        return lhs_info->get_line() < rhs_info->get_line();
      }
      if (lhs_info->get_col() != rhs_info->get_col()) {
        return lhs_info->get_col() < rhs_info->get_col();
      }
      return lhs < rhs;
    };

    auto same_source_file = [](const Sg_File_Info *lhs,
                               const Sg_File_Info *rhs) {
      if (lhs == nullptr || rhs == nullptr) {
        return false;
      }
      if (lhs->get_file_id() == rhs->get_file_id()) {
        return true;
      }
      return !lhs->get_filenameString().empty() &&
             lhs->get_filenameString() == rhs->get_filenameString();
    };

    auto source_before_or_equal = [&](const Sg_File_Info *lhs,
                                      const Sg_File_Info *rhs) {
      if (lhs == nullptr || rhs == nullptr || !same_source_file(lhs, rhs)) {
        return false;
      }
      if (lhs->get_line() != rhs->get_line()) {
        return lhs->get_line() < rhs->get_line();
      }
      return lhs->get_col() <= rhs->get_col();
    };

    auto same_source_position = [&](const Sg_File_Info *lhs,
                                    const Sg_File_Info *rhs) {
      return lhs != nullptr && rhs != nullptr && same_source_file(lhs, rhs) &&
             lhs->get_line() == rhs->get_line() &&
             lhs->get_col() == rhs->get_col();
    };

    auto effective_node_start = [](SgLocatedNode *node) -> Sg_File_Info * {
      if (node == nullptr) {
        return nullptr;
      }
      Sg_File_Info *start = node->get_startOfConstruct();
      if (start != nullptr && start->get_line() > 0) {
        return start;
      }
      start = node->get_file_info();
      if (start != nullptr && start->get_line() > 0) {
        return start;
      }
      return node->get_endOfConstruct();
    };

    auto preprocessing_line = [](const PreprocessingInfo *info) {
      if (info == nullptr) {
        return 0;
      }
      Sg_File_Info *file_info = info->get_file_info();
      if (file_info != nullptr && file_info->get_line() > 0) {
        return file_info->get_line();
      }
      return info->getLineNumber();
    };

    auto preprocessing_col = [](const PreprocessingInfo *info) {
      if (info == nullptr) {
        return 0;
      }
      Sg_File_Info *file_info = info->get_file_info();
      if (file_info != nullptr && file_info->get_col() > 0) {
        return file_info->get_col();
      }
      return info->getColumnNumber();
    };

    auto preprocessing_same_file = [&](const PreprocessingInfo *info,
                                       const Sg_File_Info *anchor) {
      if (info == nullptr || anchor == nullptr) {
        return false;
      }
      Sg_File_Info *file_info = info->get_file_info();
      if (file_info != nullptr && file_info->get_line() > 0 &&
          same_source_file(file_info, anchor)) {
        return true;
      }
      if (info->getFileId() >= 0 &&
          anchor->get_file_id() == info->getFileId()) {
        return true;
      }
      const std::string filename = info->getFilename();
      return !filename.empty() && filename == anchor->get_filenameString();
    };

    auto preprocessing_before_or_equal = [&](const PreprocessingInfo *info,
                                             const Sg_File_Info *anchor) {
      if (info == nullptr || anchor == nullptr ||
          !preprocessing_same_file(info, anchor) || anchor->get_line() <= 0) {
        return false;
      }
      const int line = preprocessing_line(info);
      const int col = preprocessing_col(info);
      if (line <= 0) {
        return false;
      }
      if (line != anchor->get_line()) {
        return line < anchor->get_line();
      }
      return col <= anchor->get_col();
    };

    auto preprocessing_same_position = [&](const PreprocessingInfo *info,
                                           const Sg_File_Info *anchor) {
      return info != nullptr && anchor != nullptr &&
             preprocessing_same_file(info, anchor) &&
             preprocessing_line(info) == anchor->get_line() &&
             preprocessing_col(info) == anchor->get_col();
    };

    auto is_opening_pp_conditional = [](PreprocessingInfo::DirectiveType type) {
      return type == PreprocessingInfo::CpreprocessorIfDeclaration ||
             type == PreprocessingInfo::CpreprocessorIfdefDeclaration ||
             type == PreprocessingInfo::CpreprocessorIfndefDeclaration;
    };

    auto is_comment_like_pp = [](const PreprocessingInfo *info) {
      if (info == nullptr) {
        return false;
      }
      switch (info->getTypeOfDirective()) {
      case PreprocessingInfo::C_StyleComment:
      case PreprocessingInfo::CplusplusStyleComment:
      case PreprocessingInfo::FortranStyleComment:
      case PreprocessingInfo::F90StyleComment:
      case PreprocessingInfo::CpreprocessorBlankLine:
        return true;
      default:
        return false;
      }
    };

    auto is_relocatable_block_pp = [&](const PreprocessingInfo *info) {
      if (info == nullptr) {
        return false;
      }
      return is_pp_conditional_directive(info->getTypeOfDirective()) ||
             info->getTypeOfDirective() == PreprocessingInfo::CSkippedToken ||
             is_comment_like_pp(info);
    };

    auto is_include_directive_type = [](PreprocessingInfo::DirectiveType type) {
      return type == PreprocessingInfo::CpreprocessorIncludeDeclaration ||
             type == PreprocessingInfo::CpreprocessorIncludeNextDeclaration;
    };

    auto relocate_function_body_leading_conditionals =
        [&](SgFunctionDefinition *function_definition) {
          if (function_definition == nullptr) {
            return;
          }

          SgBasicBlock *body = function_definition->get_body();
          if (body == nullptr) {
            return;
          }

          AttachedPreprocessingInfoType *attached =
              body->getAttachedPreprocessingInfo();
          if (attached == nullptr || attached->empty()) {
            return;
          }

          Sg_File_Info *body_start = effective_node_start(body);
          if (body_start == nullptr || body_start->get_line() <= 0) {
            return;
          }

          std::vector<PreprocessingInfo *> moves;
          for (auto it = attached->begin(); it != attached->end();) {
            PreprocessingInfo *info = *it;
            if (info == nullptr ||
                info->getRelativePosition() != PreprocessingInfo::before ||
                !is_pp_conditional_directive(info->getTypeOfDirective()) ||
                preprocessing_line(info) <= 0 ||
                !preprocessing_same_file(info, body_start) ||
                !preprocessing_before_or_equal(info, body_start) ||
                preprocessing_same_position(info, body_start)) {
              ++it;
              continue;
            }

            moves.push_back(info);
            it = attached->erase(it);
          }

          if (moves.empty()) {
            return;
          }

          std::stable_sort(moves.begin(), moves.end(), conditional_source_less);
          for (PreprocessingInfo *info : moves) {
            if (info == nullptr) {
              continue;
            }
            info->setRelativePosition(PreprocessingInfo::before);
            function_definition->addToAttachedPreprocessingInfo(info);
          }
        };

    auto select_previous_expression_anchor =
        [&](SgExpression *root, const Sg_File_Info *cursor) -> SgExpression * {
      if (root == nullptr || cursor == nullptr || cursor->get_line() <= 0) {
        return nullptr;
      }

      Rose_STL_Container<SgNode *> expressions =
          NodeQuery::querySubTree(root, V_SgExpression);
      SgExpression *best = nullptr;
      Sg_File_Info *best_start = nullptr;
      Sg_File_Info *best_end = nullptr;

      for (SgNode *node : expressions) {
        SgExpression *candidate = isSgExpression(node);
        SgLocatedNode *located_candidate = isSgLocatedNode(candidate);
        if (located_candidate == nullptr) {
          continue;
        }

        Sg_File_Info *start = effective_node_start(located_candidate);
        Sg_File_Info *end = located_candidate->get_endOfConstruct();
        if (start == nullptr || end == nullptr || start->get_line() <= 0 ||
            end->get_line() <= 0 || !same_source_file(start, cursor) ||
            !same_source_file(end, cursor) ||
            !source_before_or_equal(end, cursor)) {
          continue;
        }

        const bool better_end =
            best_end == nullptr || (!source_before_or_equal(end, best_end) &&
                                    source_before_or_equal(best_end, end));
        const bool same_end = same_source_position(end, best_end);
        const bool earlier_start = best_start != nullptr &&
                                   source_before_or_equal(start, best_start) &&
                                   !same_source_position(start, best_start);
        if (better_end || (same_end && earlier_start)) {
          best = candidate;
          best_start = start;
          best_end = end;
        }
      }

      return best;
    };

    auto select_next_expression_anchor =
        [&](SgExpression *root, const Sg_File_Info *cursor) -> SgExpression * {
      if (root == nullptr || cursor == nullptr || cursor->get_line() <= 0) {
        return nullptr;
      }

      Rose_STL_Container<SgNode *> expressions =
          NodeQuery::querySubTree(root, V_SgExpression);
      SgExpression *best = nullptr;
      Sg_File_Info *best_start = nullptr;
      Sg_File_Info *best_end = nullptr;

      for (SgNode *node : expressions) {
        SgExpression *candidate = isSgExpression(node);
        SgLocatedNode *located_candidate = isSgLocatedNode(candidate);
        if (located_candidate == nullptr) {
          continue;
        }

        Sg_File_Info *start = effective_node_start(located_candidate);
        Sg_File_Info *end = located_candidate->get_endOfConstruct();
        if (start == nullptr || end == nullptr || start->get_line() <= 0 ||
            end->get_line() <= 0 || !same_source_file(start, cursor) ||
            !same_source_file(end, cursor) ||
            !source_before_or_equal(cursor, start)) {
          continue;
        }

        const bool earlier_start =
            best_start == nullptr ||
            (!source_before_or_equal(best_start, start) &&
             source_before_or_equal(start, best_start));
        const bool same_start = same_source_position(start, best_start);
        const bool later_end = best_end != nullptr &&
                               source_before_or_equal(best_end, end) &&
                               !same_source_position(end, best_end);
        if (earlier_start || (same_start && later_end)) {
          best = candidate;
          best_start = start;
          best_end = end;
        }
      }

      return best;
    };

    auto relocate_enum_conditionals = [&](SgEnumDeclaration *enum_decl) {
      if (enum_decl == nullptr) {
        return;
      }

      AttachedPreprocessingInfoType *attached =
          enum_decl->getAttachedPreprocessingInfo();
      if (attached == nullptr || attached->empty()) {
        return;
      }

      Sg_File_Info *enum_start = effective_node_start(enum_decl);
      Sg_File_Info *enum_end = enum_decl->get_endOfConstruct();
      if (enum_start == nullptr || enum_end == nullptr ||
          enum_start->get_line() <= 0 || enum_end->get_line() <= 0 ||
          !same_source_file(enum_start, enum_end)) {
        return;
      }

      struct PendingMove {
        PreprocessingInfo *info = nullptr;
        SgLocatedNode *anchor = nullptr;
        PreprocessingInfo::RelativePositionType relative_position =
            PreprocessingInfo::after;
      };

      std::vector<PendingMove> moves;
      for (auto it = attached->begin(); it != attached->end();) {
        PreprocessingInfo *info = *it;
        Sg_File_Info *directive_info =
            info != nullptr ? info->get_file_info() : nullptr;
        if (info == nullptr ||
            !is_pp_conditional_directive(info->getTypeOfDirective()) ||
            preprocessing_line(info) <= 0 ||
            !preprocessing_same_file(info, enum_start) ||
            !source_before_or_equal(enum_start, enum_end) ||
            preprocessing_before_or_equal(info, enum_start) ||
            !preprocessing_before_or_equal(info, enum_end)) {
          ++it;
          continue;
        }

        SgLocatedNode *anchor = nullptr;
        PreprocessingInfo::RelativePositionType relative_position =
            PreprocessingInfo::after;

        for (SgInitializedName *enumerator : enum_decl->get_enumerators()) {
          if (enumerator == nullptr) {
            continue;
          }

          Sg_File_Info *enumerator_start = enumerator->get_startOfConstruct();
          Sg_File_Info *enumerator_end = enumerator->get_endOfConstruct();
          if (enumerator_start == nullptr || enumerator_end == nullptr ||
              enumerator_start->get_line() <= 0 ||
              enumerator_end->get_line() <= 0 ||
              !preprocessing_same_file(info, enumerator_start) ||
              !preprocessing_same_file(info, enumerator_end) ||
              !source_before_or_equal(enumerator_start, enumerator_end) ||
              preprocessing_before_or_equal(info, enumerator_start) ||
              !preprocessing_before_or_equal(info, enumerator_end)) {
            continue;
          }

          bool directive_inside_initializer = false;
          if (SgExpression *initializer = enumerator->get_initializer()) {
            SgLocatedNode *located_initializer = isSgLocatedNode(initializer);
            Sg_File_Info *init_start =
                effective_node_start(located_initializer);
            Sg_File_Info *init_end =
                located_initializer != nullptr
                    ? located_initializer->get_endOfConstruct()
                    : nullptr;
            if (init_start != nullptr && init_end != nullptr &&
                init_start->get_line() > 0 && init_end->get_line() > 0 &&
                preprocessing_same_file(info, init_start) &&
                preprocessing_same_file(info, init_end) &&
                preprocessing_before_or_equal(info, init_end) &&
                !preprocessing_before_or_equal(info, init_start) &&
                directive_info != nullptr && directive_info->get_line() > 0 &&
                same_source_file(init_start, directive_info)) {
              directive_inside_initializer = true;
              SgExpression *previous_expr = select_previous_expression_anchor(
                  initializer, directive_info);
              SgExpression *next_expr =
                  select_next_expression_anchor(initializer, directive_info);

              if (is_opening_pp_conditional(info->getTypeOfDirective())) {
                if (previous_expr != nullptr) {
                  anchor = isSgLocatedNode(previous_expr);
                  relative_position = PreprocessingInfo::after;
                } else if (next_expr != nullptr) {
                  anchor = isSgLocatedNode(next_expr);
                  relative_position = PreprocessingInfo::before;
                }
              } else {
                if (previous_expr != nullptr) {
                  anchor = isSgLocatedNode(previous_expr);
                  relative_position = PreprocessingInfo::after;
                } else if (next_expr != nullptr) {
                  anchor = isSgLocatedNode(next_expr);
                  relative_position = PreprocessingInfo::before;
                }
              }
            }
          }

          if (anchor == nullptr && directive_inside_initializer) {
            continue;
          }

          if (anchor == nullptr) {
            anchor = enumerator;
            relative_position =
                is_opening_pp_conditional(info->getTypeOfDirective())
                    ? PreprocessingInfo::before
                    : PreprocessingInfo::after;
          }
          break;
        }

        if (anchor == nullptr) {
          ++it;
          continue;
        }

        moves.push_back({info, anchor, relative_position});
        it = attached->erase(it);
      }

      if (moves.empty()) {
        return;
      }

      std::stable_sort(moves.begin(), moves.end(),
                       [&](const PendingMove &lhs, const PendingMove &rhs) {
                         return conditional_source_less(lhs.info, rhs.info);
                       });

      for (const PendingMove &move : moves) {
        if (move.info == nullptr || move.anchor == nullptr) {
          continue;
        }
        move.info->setRelativePosition(move.relative_position);
        move.anchor->addToAttachedPreprocessingInfo(move.info);
      }
    };

    auto relocate_variable_initializer_directives =
        [&](SgVariableDeclaration *variable_declaration) {
          if (variable_declaration == nullptr) {
            return;
          }

          AttachedPreprocessingInfoType *attached =
              variable_declaration->getAttachedPreprocessingInfo();
          if (attached == nullptr || attached->empty()) {
            return;
          }

          struct PendingMove {
            PreprocessingInfo *info = nullptr;
            SgLocatedNode *anchor = nullptr;
            PreprocessingInfo::RelativePositionType relative_position =
                PreprocessingInfo::before;
          };

          std::vector<PendingMove> moves;
          for (auto it = attached->begin(); it != attached->end();) {
            PreprocessingInfo *info = *it;
            if (info == nullptr ||
                !is_include_directive_type(info->getTypeOfDirective()) ||
                preprocessing_line(info) <= 0) {
              ++it;
              continue;
            }

            SgLocatedNode *anchor = nullptr;
            for (SgInitializedName *variable :
                 variable_declaration->get_variables()) {
              if (variable == nullptr) {
                continue;
              }
              SgLocatedNode *initializer =
                  isSgLocatedNode(variable->get_initializer());
              Sg_File_Info *init_start = effective_node_start(initializer);
              Sg_File_Info *init_end = initializer != nullptr
                                           ? initializer->get_endOfConstruct()
                                           : nullptr;
              if (init_start == nullptr || init_end == nullptr ||
                  init_start->get_line() <= 0 || init_end->get_line() <= 0 ||
                  !preprocessing_same_file(info, init_start) ||
                  !preprocessing_same_file(info, init_end) ||
                  !preprocessing_before_or_equal(info, init_end) ||
                  preprocessing_before_or_equal(info, init_start)) {
                continue;
              }
              anchor = initializer;
              break;
            }

            if (anchor == nullptr) {
              ++it;
              continue;
            }

            moves.push_back({info, anchor, PreprocessingInfo::before});
            it = attached->erase(it);
          }

          if (moves.empty()) {
            return;
          }

          std::stable_sort(moves.begin(), moves.end(),
                           [&](const PendingMove &lhs, const PendingMove &rhs) {
                             return conditional_source_less(lhs.info, rhs.info);
                           });

          for (const PendingMove &move : moves) {
            if (move.info == nullptr || move.anchor == nullptr) {
              continue;
            }
            move.info->setRelativePosition(move.relative_position);
            move.anchor->addToAttachedPreprocessingInfo(move.info);
          }
        };

    auto relocate_basic_block_inside_preprocessing = [&](SgBasicBlock *block) {
      if (block == nullptr) {
        return;
      }

      AttachedPreprocessingInfoType *attached =
          block->getAttachedPreprocessingInfo();
      if (attached == nullptr || attached->empty()) {
        return;
      }

      Sg_File_Info *block_start = effective_node_start(block);
      Sg_File_Info *block_end = block->get_endOfConstruct();
      if (block_start == nullptr || block_end == nullptr ||
          block_start->get_line() <= 0 || block_end->get_line() <= 0 ||
          !same_source_file(block_start, block_end)) {
        return;
      }

      struct StatementAnchor {
        SgStatement *statement = nullptr;
        Sg_File_Info *start = nullptr;
        Sg_File_Info *end = nullptr;
      };

      std::vector<StatementAnchor> anchors;
      anchors.reserve(block->get_statements().size());
      for (SgStatement *statement : block->get_statements()) {
        SgLocatedNode *located_statement = isSgLocatedNode(statement);
        Sg_File_Info *statement_start = effective_node_start(located_statement);
        Sg_File_Info *statement_end =
            located_statement != nullptr
                ? located_statement->get_endOfConstruct()
                : nullptr;
        if (statement_start == nullptr || statement_end == nullptr ||
            statement_start->get_line() <= 0 ||
            statement_end->get_line() <= 0 ||
            !same_source_file(statement_start, block_start) ||
            !same_source_file(statement_end, block_start)) {
          continue;
        }
        anchors.push_back({statement, statement_start, statement_end});
      }

      if (anchors.empty()) {
        return;
      }

      auto pick_previous_statement =
          [&](const PreprocessingInfo *info) -> SgStatement * {
        SgStatement *best = nullptr;
        Sg_File_Info *best_start = nullptr;
        Sg_File_Info *best_end = nullptr;
        for (const StatementAnchor &entry : anchors) {
          if (!preprocessing_before_or_equal(info, entry.end)) {
            continue;
          }
          const bool later_start =
              best_start == nullptr ||
              (!source_before_or_equal(entry.start, best_start) &&
               source_before_or_equal(best_start, entry.start));
          const bool same_start = same_source_position(entry.start, best_start);
          const bool earlier_end =
              best_end != nullptr &&
              source_before_or_equal(entry.end, best_end) &&
              !same_source_position(entry.end, best_end);
          if (later_start || (same_start && earlier_end)) {
            best = entry.statement;
            best_start = entry.start;
            best_end = entry.end;
          }
        }
        return best;
      };

      auto pick_next_statement =
          [&](const PreprocessingInfo *info) -> SgStatement * {
        SgStatement *best = nullptr;
        Sg_File_Info *best_start = nullptr;
        Sg_File_Info *best_end = nullptr;
        for (const StatementAnchor &entry : anchors) {
          if (!preprocessing_same_file(info, entry.start) ||
              !preprocessing_before_or_equal(info, entry.start)) {
            continue;
          }
          const bool earlier_start =
              best_start == nullptr ||
              (!source_before_or_equal(best_start, entry.start) &&
               source_before_or_equal(entry.start, best_start));
          const bool same_start = same_source_position(entry.start, best_start);
          const bool later_end = best_end != nullptr &&
                                 source_before_or_equal(best_end, entry.end) &&
                                 !same_source_position(entry.end, best_end);
          if (earlier_start || (same_start && later_end)) {
            best = entry.statement;
            best_start = entry.start;
            best_end = entry.end;
          }
        }
        return best;
      };

      struct PendingMove {
        PreprocessingInfo *info = nullptr;
        SgLocatedNode *anchor = nullptr;
        PreprocessingInfo::RelativePositionType relative_position =
            PreprocessingInfo::after;
      };

      std::vector<PendingMove> moves;
      for (auto it = attached->begin(); it != attached->end();) {
        PreprocessingInfo *info = *it;
        if (info == nullptr ||
            info->getRelativePosition() != PreprocessingInfo::inside ||
            !is_relocatable_block_pp(info) || preprocessing_line(info) <= 0 ||
            !preprocessing_same_file(info, block_start) ||
            !preprocessing_before_or_equal(info, block_end) ||
            preprocessing_before_or_equal(info, block_start)) {
          ++it;
          continue;
        }

        SgStatement *previous = pick_previous_statement(info);
        SgStatement *next = pick_next_statement(info);

        SgLocatedNode *anchor = nullptr;
        PreprocessingInfo::RelativePositionType relative_position =
            PreprocessingInfo::after;
        if (is_opening_pp_conditional(info->getTypeOfDirective()) ||
            is_comment_like_pp(info)) {
          if (next != nullptr) {
            anchor = isSgLocatedNode(next);
            relative_position = PreprocessingInfo::before;
          } else if (previous != nullptr) {
            anchor = isSgLocatedNode(previous);
            relative_position = PreprocessingInfo::after;
          }
        } else {
          if (previous != nullptr) {
            anchor = isSgLocatedNode(previous);
            relative_position = PreprocessingInfo::after;
          } else if (next != nullptr) {
            anchor = isSgLocatedNode(next);
            relative_position = PreprocessingInfo::before;
          }
        }

        if (anchor == nullptr) {
          ++it;
          continue;
        }

        moves.push_back({info, anchor, relative_position});
        it = attached->erase(it);
      }

      if (moves.empty()) {
        return;
      }

      std::stable_sort(moves.begin(), moves.end(),
                       [&](const PendingMove &lhs, const PendingMove &rhs) {
                         return conditional_source_less(lhs.info, rhs.info);
                       });

      for (const PendingMove &move : moves) {
        if (move.info == nullptr || move.anchor == nullptr) {
          continue;
        }
        move.info->setRelativePosition(move.relative_position);
        move.anchor->addToAttachedPreprocessingInfo(move.info);
      }
    };

    Rose_STL_Container<SgNode *> enum_nodes =
        NodeQuery::querySubTree(global_scope, V_SgEnumDeclaration);
    for (SgNode *node : enum_nodes) {
      relocate_enum_conditionals(isSgEnumDeclaration(node));
    }

    // Keep late preprocessing relocation conservative. Earlier attachment and
    // scope/class relocation passes already preserve inactive branches; these
    // extra moves were broad enough to drag `#if` structure out of the
    // declaration/expression it belongs to, which regressed strict C roundtrip
    // cases such as conditional local declarations, enum initializers, and
    // anonymous aggregate fields.

    auto reorder_attached_conditionals = [&](SgLocatedNode *node) {
      if (node == nullptr) {
        return;
      }
      AttachedPreprocessingInfoType *attached =
          node->getAttachedPreprocessingInfo();
      if (attached == nullptr || attached->size() < 2) {
        return;
      }

      auto reorder_for_position =
          [&](PreprocessingInfo::RelativePositionType rel_pos) {
            std::vector<size_t> slots;
            std::vector<PreprocessingInfo *> directives;

            for (size_t i = 0; i < attached->size(); ++i) {
              PreprocessingInfo *info = (*attached)[i];
              if (info == nullptr || info->getRelativePosition() != rel_pos ||
                  !is_pp_conditional_directive(info->getTypeOfDirective())) {
                continue;
              }
              Sg_File_Info *fi = info->get_file_info();
              if (fi == nullptr || fi->get_line() <= 0) {
                continue;
              }
              slots.push_back(i);
              directives.push_back(info);
            }

            if (directives.size() < 2) {
              return;
            }

            std::stable_sort(directives.begin(), directives.end(),
                             conditional_source_less);

            for (size_t i = 0; i < slots.size(); ++i) {
              (*attached)[slots[i]] = directives[i];
            }
          };

      reorder_for_position(PreprocessingInfo::before);
      reorder_for_position(PreprocessingInfo::inside);
      reorder_for_position(PreprocessingInfo::after);
    };

    reorder_attached_conditionals(global_scope);
    Rose_STL_Container<SgNode *> located_nodes =
        NodeQuery::querySubTree(global_scope, V_SgLocatedNode);
    for (SgNode *node : located_nodes) {
      reorder_attached_conditionals(isSgLocatedNode(node));
    }

    struct ConditionalRange {
      Sg_File_Info *begin = nullptr;
      Sg_File_Info *end = nullptr;
      bool suppress_directives = false;
    };

    auto source_file_key = [](const Sg_File_Info *info) -> std::string {
      if (info == nullptr) {
        return std::string();
      }
      const std::string &filename = info->get_filenameString();
      if (!filename.empty()) {
        return filename;
      }
      return std::string("#") + std::to_string(info->get_file_id());
    };

    std::vector<PreprocessingInfo *> recorded_conditionals;
    std::unordered_set<PreprocessingInfo *> seen_conditionals;
    auto collect_recorded_conditionals = [&](SgLocatedNode *node) {
      if (node == nullptr) {
        return;
      }
      AttachedPreprocessingInfoType *attached =
          node->getAttachedPreprocessingInfo();
      if (attached == nullptr) {
        return;
      }
      for (PreprocessingInfo *info : *attached) {
        if (info == nullptr ||
            !is_pp_conditional_directive(info->getTypeOfDirective()) ||
            !seen_conditionals.insert(info).second) {
          continue;
        }
        Sg_File_Info *fi = info->get_file_info();
        if (fi == nullptr || fi->get_line() <= 0) {
          continue;
        }
        recorded_conditionals.push_back(info);
      }
    };

    collect_recorded_conditionals(global_scope);
    for (SgNode *node : located_nodes) {
      collect_recorded_conditionals(isSgLocatedNode(node));
    }

    std::map<std::string, std::vector<ConditionalRange>> conditional_ranges;
    if (!recorded_conditionals.empty()) {
      std::stable_sort(recorded_conditionals.begin(),
                       recorded_conditionals.end(), conditional_source_less);

      std::map<std::string, std::vector<PreprocessingInfo *>> open_stack;
      for (PreprocessingInfo *info : recorded_conditionals) {
        if (info == nullptr) {
          continue;
        }
        Sg_File_Info *fi = info->get_file_info();
        if (fi == nullptr || fi->get_line() <= 0) {
          continue;
        }

        const std::string file_key = source_file_key(fi);
        if (file_key.empty()) {
          continue;
        }

        PreprocessingInfo::DirectiveType type = info->getTypeOfDirective();
        if (type == PreprocessingInfo::CpreprocessorIfDeclaration ||
            type == PreprocessingInfo::CpreprocessorIfdefDeclaration ||
            type == PreprocessingInfo::CpreprocessorIfndefDeclaration) {
          open_stack[file_key].push_back(info);
          continue;
        }

        if (type != PreprocessingInfo::CpreprocessorEndifDeclaration) {
          continue;
        }

        auto stack_it = open_stack.find(file_key);
        if (stack_it == open_stack.end() || stack_it->second.empty()) {
          continue;
        }

        PreprocessingInfo *open_info = stack_it->second.back();
        stack_it->second.pop_back();
        if (open_info == nullptr) {
          continue;
        }

        Sg_File_Info *open_fi = open_info->get_file_info();
        if (open_fi == nullptr || open_fi->get_line() <= 0 ||
            !same_source_file(open_fi, fi)) {
          continue;
        }

        conditional_ranges[file_key].push_back({open_fi, fi, false});
      }
    }

    auto suppress_located_output = [](SgLocatedNode *node) {
      if (node == nullptr) {
        return;
      }
      if (Sg_File_Info *fi = node->get_file_info()) {
        fi->unsetOutputInCodeGeneration();
      }
      if (Sg_File_Info *fi = node->get_startOfConstruct()) {
        fi->unsetOutputInCodeGeneration();
      }
      if (Sg_File_Info *fi = node->get_endOfConstruct()) {
        fi->unsetOutputInCodeGeneration();
      }
    };

    auto node_is_spelled_within_conditional = [&](SgLocatedNode *node) -> bool {
      if (node == nullptr) {
        return false;
      }

      Sg_File_Info *start = effective_node_start(node);
      Sg_File_Info *end = node->get_endOfConstruct();
      if (start == nullptr || end == nullptr || start->get_line() <= 0 ||
          end->get_line() <= 0 || !same_source_file(start, end)) {
        return false;
      }

      const std::string file_key = source_file_key(start);
      auto ranges_it = conditional_ranges.find(file_key);
      if (file_key.empty() || ranges_it == conditional_ranges.end()) {
        return false;
      }

      for (const ConditionalRange &range : ranges_it->second) {
        if (range.begin == nullptr || range.end == nullptr ||
            !same_source_file(start, range.begin) ||
            !same_source_file(end, range.end)) {
          continue;
        }
        if (source_before_or_equal(range.begin, start) &&
            source_before_or_equal(end, range.end)) {
          return true;
        }
      }

      return false;
    };

    auto mark_conditional_ranges_for_node = [&](SgLocatedNode *node) {
      if (node == nullptr) {
        return;
      }

      Sg_File_Info *start = effective_node_start(node);
      Sg_File_Info *end = node->get_endOfConstruct();
      if (start == nullptr || end == nullptr || start->get_line() <= 0 ||
          end->get_line() <= 0 || !same_source_file(start, end)) {
        return;
      }

      const std::string file_key = source_file_key(start);
      auto ranges_it = conditional_ranges.find(file_key);
      if (file_key.empty() || ranges_it == conditional_ranges.end()) {
        return;
      }

      for (ConditionalRange &range : ranges_it->second) {
        if (range.begin == nullptr || range.end == nullptr ||
            !same_source_file(start, range.begin) ||
            !same_source_file(end, range.end)) {
          continue;
        }
        if (source_before_or_equal(range.begin, start) &&
            source_before_or_equal(end, range.end)) {
          range.suppress_directives = true;
        }
      }
    };

    Rose_STL_Container<SgNode *> variable_declarations =
        NodeQuery::querySubTree(global_scope, V_SgVariableDeclaration);
    for (SgNode *node : variable_declarations) {
      SgVariableDeclaration *var_decl = isSgVariableDeclaration(node);
      if (var_decl == nullptr ||
          !node_is_spelled_within_conditional(var_decl)) {
        continue;
      }

      suppress_located_output(var_decl);
      mark_conditional_ranges_for_node(var_decl);
      if (SgVariableDefinition *def = var_decl->get_definition()) {
        suppress_located_output(def);
      }
      for (SgInitializedName *init_name : var_decl->get_variables()) {
        suppress_located_output(init_name);
        if (SgVariableDefinition *declptr = isSgVariableDefinition(
                init_name != nullptr ? init_name->get_declptr() : nullptr)) {
          suppress_located_output(declptr);
        }
      }
    }

    Rose_STL_Container<SgNode *> function_declarations =
        NodeQuery::querySubTree(global_scope, V_SgFunctionDeclaration);
    auto is_function_declarator_leading_payload =
        [](PreprocessingInfo::DirectiveType type) {
          return type == PreprocessingInfo::CpreprocessorIfDeclaration ||
                 type == PreprocessingInfo::CpreprocessorIfdefDeclaration ||
                 type == PreprocessingInfo::CpreprocessorIfndefDeclaration ||
                 type == PreprocessingInfo::CpreprocessorElseDeclaration ||
                 type == PreprocessingInfo::CpreprocessorElifDeclaration ||
                 type == PreprocessingInfo::CpreprocessorDeadIfDeclaration ||
                 type == PreprocessingInfo::CSkippedToken;
        };
    auto reanchor_function_prefix_conditionals_from_body =
        [&](SgFunctionDefinition *definition, SgBasicBlock *body,
            const Sg_File_Info *decl_start, const Sg_File_Info *body_start) {
          if (definition == nullptr || body == nullptr ||
              decl_start == nullptr || body_start == nullptr ||
              decl_start->get_line() <= 0 || body_start->get_line() <= 0 ||
              !same_source_file(decl_start, body_start)) {
            return;
          }

          AttachedPreprocessingInfoType *attached =
              body->getAttachedPreprocessingInfo();
          if (attached == nullptr || attached->empty()) {
            return;
          }

          for (auto it = attached->begin(); it != attached->end();) {
            PreprocessingInfo *info = *it;
            if (info == nullptr) {
              ++it;
              continue;
            }

            PreprocessingInfo::DirectiveType type = info->getTypeOfDirective();
            const bool declaration_prefix_conditional_payload =
                is_pp_conditional_directive(type) ||
                type == PreprocessingInfo::CpreprocessorDeadIfDeclaration ||
                type == PreprocessingInfo::CSkippedToken;
            if (!declaration_prefix_conditional_payload ||
                !preprocessing_same_file(info, decl_start) ||
                preprocessing_before_or_equal(info, decl_start) ||
                !preprocessing_before_or_equal(info, body_start) ||
                preprocessing_same_position(info, body_start)) {
              ++it;
              continue;
            }

            info->setRelativePosition(PreprocessingInfo::before);
            definition->addToAttachedPreprocessingInfo(info);
            it = attached->erase(it);
          }
        };
    auto owner_has_function_prefix_conditionals =
        [&](SgLocatedNode *node, const Sg_File_Info *decl_start,
            const Sg_File_Info *body_start) {
          if (node == nullptr || decl_start == nullptr ||
              body_start == nullptr || decl_start->get_line() <= 0 ||
              body_start->get_line() <= 0 ||
              !same_source_file(decl_start, body_start)) {
            return false;
          }

          AttachedPreprocessingInfoType *attached =
              node->getAttachedPreprocessingInfo();
          if (attached == nullptr || attached->empty()) {
            return false;
          }

          for (PreprocessingInfo *info : *attached) {
            if (info == nullptr) {
              continue;
            }

            PreprocessingInfo::DirectiveType type = info->getTypeOfDirective();
            const bool declaration_prefix_conditional_payload =
                is_pp_conditional_directive(type) ||
                type == PreprocessingInfo::CpreprocessorDeadIfDeclaration ||
                type == PreprocessingInfo::CSkippedToken;
            if (!declaration_prefix_conditional_payload ||
                !preprocessing_same_file(info, decl_start) ||
                !preprocessing_before_or_equal(info, body_start) ||
                preprocessing_same_position(info, body_start)) {
              continue;
            }

            if (!preprocessing_before_or_equal(info, decl_start)) {
              return true;
            }

            Sg_File_Info *info_fi = info->get_file_info();
            if (info_fi == nullptr ||
                (info_fi->get_line() == decl_start->get_line() &&
                 info_fi->get_col() == decl_start->get_col())) {
              continue;
            }

            if (is_function_declarator_leading_payload(type)) {
              return true;
            }
          }

          return false;
        };
    auto prune_function_prefix_conditionals =
        [&](SgLocatedNode *node, const Sg_File_Info *decl_start,
            const Sg_File_Info *body_start) {
          if (node == nullptr || decl_start == nullptr ||
              body_start == nullptr || decl_start->get_line() <= 0 ||
              body_start->get_line() <= 0 ||
              !same_source_file(decl_start, body_start)) {
            return;
          }

          AttachedPreprocessingInfoType *attached =
              node->getAttachedPreprocessingInfo();
          if (attached == nullptr || attached->empty()) {
            return;
          }

          for (auto it = attached->begin(); it != attached->end();) {
            PreprocessingInfo *info = *it;
            if (info == nullptr) {
              ++it;
              continue;
            }

            PreprocessingInfo::DirectiveType type = info->getTypeOfDirective();
            const bool declaration_prefix_conditional_payload =
                is_pp_conditional_directive(type) ||
                type == PreprocessingInfo::CpreprocessorDeadIfDeclaration ||
                type == PreprocessingInfo::CSkippedToken;
            if (!declaration_prefix_conditional_payload ||
                !preprocessing_same_file(info, decl_start) ||
                preprocessing_before_or_equal(info, decl_start) ||
                !preprocessing_before_or_equal(info, body_start) ||
                preprocessing_same_position(info, body_start)) {
              ++it;
              continue;
            }

            it = attached->erase(it);
          }
        };

    for (SgNode *node : function_declarations) {
      SgFunctionDeclaration *func_decl = isSgFunctionDeclaration(node);
      if (func_decl == nullptr) {
        continue;
      }

      SgFunctionDefinition *definition = func_decl->get_definition();
      if (definition == nullptr || definition->get_body() == nullptr) {
        continue;
      }

      Sg_File_Info *decl_start = effective_node_start(func_decl);
      Sg_File_Info *body_start = effective_node_start(definition->get_body());
      if (decl_start == nullptr || body_start == nullptr ||
          decl_start->get_line() <= 0 || body_start->get_line() <= 0 ||
          !same_source_file(decl_start, body_start) ||
          !source_before_or_equal(decl_start, body_start)) {
        continue;
      }

      reanchor_function_prefix_conditionals_from_body(
          definition, definition->get_body(), decl_start, body_start);

      const bool declaration_side_still_owns_prefix_conditionals =
          owner_has_function_prefix_conditionals(func_decl, decl_start,
                                                 body_start) ||
          owner_has_function_prefix_conditionals(func_decl->get_parameterList(),
                                                 decl_start, body_start) ||
          owner_has_function_prefix_conditionals(
              func_decl->get_parameterList_syntax(), decl_start, body_start) ||
          owner_has_function_prefix_conditionals(definition, decl_start,
                                                 body_start) ||
          owner_has_function_prefix_conditionals(definition->get_body(),
                                                 decl_start, body_start);

      const std::string file_key = source_file_key(decl_start);
      auto ranges_it = conditional_ranges.find(file_key);
      if (file_key.empty() || ranges_it == conditional_ranges.end()) {
        continue;
      }

      if (!declaration_side_still_owns_prefix_conditionals) {
        for (ConditionalRange &range : ranges_it->second) {
          if (range.begin == nullptr || range.end == nullptr ||
              !same_source_file(range.begin, decl_start) ||
              !same_source_file(range.end, body_start)) {
            continue;
          }
          if (source_before_or_equal(decl_start, range.begin) &&
              source_before_or_equal(range.end, body_start)) {
            range.suppress_directives = true;
            auto extend_end_of_construct = [&](SgLocatedNode *node) {
              if (node == nullptr) {
                return;
              }
              Sg_File_Info *node_end = node->get_endOfConstruct();
              if (node_end != nullptr && node_end->get_line() > 0 &&
                  !same_source_file(node_end, body_start)) {
                return;
              }
              if (node_end == nullptr || node_end->get_line() <= 0 ||
                  source_before_or_equal(node_end, body_start)) {
                Sg_File_Info *new_end = new Sg_File_Info(*body_start);
                new_end->set_parent(node);
                node->set_endOfConstruct(new_end);
              }
            };

            extend_end_of_construct(func_decl);
            extend_end_of_construct(func_decl->get_parameterList());
            extend_end_of_construct(func_decl->get_parameterList_syntax());
          }
        }
      }

      if (!declaration_side_still_owns_prefix_conditionals) {
        prune_function_prefix_conditionals(func_decl, decl_start, body_start);
        prune_function_prefix_conditionals(func_decl->get_parameterList(),
                                           decl_start, body_start);
        prune_function_prefix_conditionals(
            func_decl->get_parameterList_syntax(), decl_start, body_start);
        prune_function_prefix_conditionals(definition, decl_start, body_start);
        prune_function_prefix_conditionals(definition->get_body(), decl_start,
                                           body_start);
      }
    }

    auto directive_in_suppressed_variable_range = [&](PreprocessingInfo *info) {
      if (info == nullptr) {
        return false;
      }

      PreprocessingInfo::DirectiveType type = info->getTypeOfDirective();
      const bool declaration_only_conditional_payload =
          is_pp_conditional_directive(type) ||
          type == PreprocessingInfo::CpreprocessorDeadIfDeclaration ||
          type == PreprocessingInfo::CSkippedToken;
      if (!declaration_only_conditional_payload) {
        return false;
      }

      Sg_File_Info *fi = info->get_file_info();
      if (fi == nullptr || fi->get_line() <= 0) {
        return false;
      }

      const std::string file_key = source_file_key(fi);
      auto ranges_it = conditional_ranges.find(file_key);
      if (file_key.empty() || ranges_it == conditional_ranges.end()) {
        return false;
      }

      for (const ConditionalRange &range : ranges_it->second) {
        if (!range.suppress_directives || range.begin == nullptr ||
            range.end == nullptr || !same_source_file(fi, range.begin) ||
            !same_source_file(fi, range.end)) {
          continue;
        }
        if (source_before_or_equal(range.begin, fi) &&
            source_before_or_equal(fi, range.end)) {
          return true;
        }
      }

      return false;
    };

    auto prune_suppressed_conditionals = [&](SgLocatedNode *node) {
      if (node == nullptr) {
        return;
      }
      AttachedPreprocessingInfoType *attached =
          node->getAttachedPreprocessingInfo();
      if (attached == nullptr || attached->empty()) {
        return;
      }
      for (PreprocessingInfo *info : *attached) {
        if (!directive_in_suppressed_variable_range(info) || info == nullptr) {
          continue;
        }
        if (Sg_File_Info *fi = info->get_file_info()) {
          fi->unsetOutputInCodeGeneration();
        }
      }
    };

    prune_suppressed_conditionals(global_scope);
    for (SgNode *node : located_nodes) {
      prune_suppressed_conditionals(isSgLocatedNode(node));
    }
  }

  if (global_scope != nullptr) {
    auto is_include_directive = [](const PreprocessingInfo *info) -> bool {
      if (info == nullptr) {
        return false;
      }
      PreprocessingInfo::DirectiveType type = info->getTypeOfDirective();
      return type == PreprocessingInfo::CpreprocessorIncludeDeclaration ||
             type == PreprocessingInfo::CpreprocessorIncludeNextDeclaration;
    };
    auto is_same_file = [](const Sg_File_Info *a,
                           const Sg_File_Info *b) -> bool {
      if (a == nullptr || b == nullptr) {
        return false;
      }
      if (a->get_file_id() == b->get_file_id()) {
        return true;
      }
      if (!a->get_filenameString().empty() &&
          !b->get_filenameString().empty()) {
        return a->get_filenameString() == b->get_filenameString();
      }
      return false;
    };
    auto effective_info = [](SgLocatedNode *node) -> Sg_File_Info * {
      if (node == nullptr) {
        return nullptr;
      }
      Sg_File_Info *info = node->get_startOfConstruct();
      if (info != nullptr && info->get_line() > 0) {
        return info;
      }
      info = node->get_file_info();
      if (info != nullptr && info->get_line() > 0) {
        return info;
      }
      return node->get_endOfConstruct();
    };
    Sg_File_Info *global_main_info = global_scope->get_file_info();
    auto is_main_file_info = [&](Sg_File_Info *info) -> bool {
      if (global_main_info == nullptr || info == nullptr ||
          info->get_line() <= 0) {
        return false;
      }
      return is_same_file(info, global_main_info);
    };
    auto first_output_stmt = [&]() -> SgStatement * {
      SgDeclarationStatementPtrList &decls = global_scope->get_declarations();
      for (SgDeclarationStatement *decl : decls) {
        if (decl == nullptr) {
          continue;
        }
        Sg_File_Info *info = effective_info(decl);
        if (!is_main_file_info(info)) {
          continue;
        }
        if (!info->isCompilerGenerated() || info->isOutputInCodeGeneration()) {
          return decl;
        }
      }
      for (SgDeclarationStatement *decl : decls) {
        if (decl == nullptr) {
          continue;
        }
        Sg_File_Info *info = effective_info(decl);
        if (info == nullptr || info->get_line() <= 0) {
          continue;
        }
        if (!info->isCompilerGenerated() || info->isOutputInCodeGeneration()) {
          return decl;
        }
      }
      return nullptr;
    }();

    Sg_File_Info *first_output_info = effective_info(first_output_stmt);
    std::vector<std::pair<Sg_File_Info *, PreprocessingInfo *>>
        includes_to_move;
    Rose_STL_Container<SgNode *> located_nodes =
        NodeQuery::querySubTree(global_scope, V_SgLocatedNode);
    for (SgNode *node : located_nodes) {
      SgLocatedNode *loc = isSgLocatedNode(node);
      if (loc == nullptr) {
        continue;
      }
      AttachedPreprocessingInfoType *attached =
          loc->getAttachedPreprocessingInfo();
      if (attached == nullptr || attached->empty()) {
        continue;
      }
      Sg_File_Info *owner_info = effective_info(loc);
      for (auto it = attached->begin(); it != attached->end();) {
        PreprocessingInfo *pp = *it;
        if (!is_include_directive(pp)) {
          ++it;
          continue;
        }
        Sg_File_Info *pp_info = pp->get_file_info();
        Sg_File_Info *main_source_info =
            global_main_info != nullptr ? global_main_info : first_output_info;
        bool same_main_file = main_source_info != nullptr &&
                              is_same_file(pp_info, main_source_info);
        bool should_hoist_before_first_output =
            same_main_file && pp_info != nullptr &&
            first_output_info != nullptr && pp_info->get_line() > 0 &&
            first_output_info->get_line() > 0 &&
            pp_info->get_line() < first_output_info->get_line();
        if (should_hoist_before_first_output) {
          includes_to_move.emplace_back(pp_info, pp);
          it = attached->erase(it);
          continue;
        }
        ++it;
      }
    }

    if (first_output_stmt != nullptr && !includes_to_move.empty()) {
      auto by_location =
          [](const std::pair<Sg_File_Info *, PreprocessingInfo *> &lhs,
             const std::pair<Sg_File_Info *, PreprocessingInfo *> &rhs) {
            const Sg_File_Info *lhs_info = lhs.first;
            const Sg_File_Info *rhs_info = rhs.first;
            if (lhs_info == nullptr || rhs_info == nullptr) {
              return lhs_info < rhs_info;
            }
            if (lhs_info->get_line() != rhs_info->get_line()) {
              return lhs_info->get_line() < rhs_info->get_line();
            }
            return lhs_info->get_col() < rhs_info->get_col();
          };
      std::stable_sort(includes_to_move.begin(), includes_to_move.end(),
                       by_location);
      for (const auto &entry : includes_to_move) {
        if (entry.second == nullptr) {
          continue;
        }
        entry.second->setRelativePosition(PreprocessingInfo::before);
        first_output_stmt->addToAttachedPreprocessingInfo(entry.second);
      }

      auto preproc_by_location = [](const PreprocessingInfo *lhs,
                                    const PreprocessingInfo *rhs) {
        if (lhs == nullptr || rhs == nullptr) {
          return lhs < rhs;
        }
        const Sg_File_Info *lhs_info = lhs->get_file_info();
        const Sg_File_Info *rhs_info = rhs->get_file_info();
        if (lhs_info == nullptr || rhs_info == nullptr) {
          return lhs_info < rhs_info;
        }
        if (lhs_info->get_line() != rhs_info->get_line()) {
          return lhs_info->get_line() < rhs_info->get_line();
        }
        if (lhs_info->get_col() != rhs_info->get_col()) {
          return lhs_info->get_col() < rhs_info->get_col();
        }
        return lhs < rhs;
      };

      if (AttachedPreprocessingInfoType *attached =
              first_output_stmt->getAttachedPreprocessingInfo()) {
        std::stable_sort(attached->begin(), attached->end(),
                         preproc_by_location);
      }
    }

    SgDeclarationStatementPtrList &global_decls =
        global_scope->get_declarations();
    auto source_location_leq = [](const Sg_File_Info *lhs,
                                  const Sg_File_Info *rhs) {
      if (lhs == nullptr || rhs == nullptr) {
        return false;
      }
      if (lhs->get_line() != rhs->get_line()) {
        return lhs->get_line() < rhs->get_line();
      }
      return lhs->get_col() <= rhs->get_col();
    };

    std::vector<SgTemplateInstantiationDirectiveStatement *> inst_directives;
    inst_directives.reserve(global_decls.size());
    for (SgDeclarationStatement *decl : global_decls) {
      if (SgTemplateInstantiationDirectiveStatement *directive =
              isSgTemplateInstantiationDirectiveStatement(decl)) {
        inst_directives.push_back(directive);
      }
    }

    for (SgTemplateInstantiationDirectiveStatement *directive :
         inst_directives) {
      if (directive == nullptr) {
        continue;
      }
      Sg_File_Info *directive_info = effective_info(directive);
      if (!is_main_file_info(directive_info)) {
        continue;
      }

      std::size_t directive_index = global_decls.size();
      for (std::size_t i = 0; i < global_decls.size(); ++i) {
        if (global_decls[i] == directive) {
          directive_index = i;
          break;
        }
      }
      if (directive_index == global_decls.size()) {
        continue;
      }

      std::size_t last_prior_index = global_decls.size();
      bool has_prior_after_directive = false;
      for (std::size_t i = 0; i < global_decls.size(); ++i) {
        if (i == directive_index) {
          continue;
        }
        SgDeclarationStatement *existing = global_decls[i];
        Sg_File_Info *existing_info = effective_info(isSgLocatedNode(existing));
        if (existing_info == nullptr || existing_info->get_line() <= 0) {
          continue;
        }
        if (!is_same_file(existing_info, directive_info)) {
          continue;
        }
        if (!source_location_leq(existing_info, directive_info)) {
          continue;
        }
        last_prior_index = i;
        if (i > directive_index) {
          has_prior_after_directive = true;
        }
      }

      if (!has_prior_after_directive) {
        continue;
      }

      SgDeclarationStatement *directive_decl = global_decls[directive_index];
      global_decls.erase(global_decls.begin() + directive_index);

      if (last_prior_index > directive_index) {
        --last_prior_index;
      }

      std::size_t insert_index = 0;
      if (last_prior_index != global_decls.size()) {
        insert_index = last_prior_index + 1;
      } else {
        insert_index = global_decls.size();
        for (std::size_t i = 0; i < global_decls.size(); ++i) {
          Sg_File_Info *existing_info =
              effective_info(isSgLocatedNode(global_decls[i]));
          if (existing_info == nullptr || existing_info->get_line() <= 0) {
            continue;
          }
          if (!is_same_file(existing_info, directive_info)) {
            continue;
          }
          insert_index = i;
          break;
        }
      }

      global_decls.insert(global_decls.begin() + insert_index, directive_decl);
    }
  }

  if (global_scope != nullptr) {
    auto declaration_outputs_code = [](SgFunctionDeclaration *decl) -> bool {
      if (decl == nullptr) {
        return false;
      }
      Sg_File_Info *info = decl->get_startOfConstruct();
      if (info == nullptr || info->get_line() <= 0) {
        info = decl->get_file_info();
      }
      if (info == nullptr) {
        return false;
      }
      return !info->isCompilerGenerated() || info->isOutputInCodeGeneration();
    };
    auto location_less = [](SgFunctionDeclaration *lhs,
                            SgFunctionDeclaration *rhs) -> bool {
      if (lhs == nullptr || rhs == nullptr) {
        return lhs < rhs;
      }
      auto effective_info = [](SgFunctionDeclaration *decl) -> Sg_File_Info * {
        if (decl == nullptr) {
          return nullptr;
        }
        Sg_File_Info *info = decl->get_startOfConstruct();
        if (info != nullptr && info->get_line() > 0) {
          return info;
        }
        return decl->get_file_info();
      };
      Sg_File_Info *lhs_info = effective_info(lhs);
      Sg_File_Info *rhs_info = effective_info(rhs);
      if (lhs_info == nullptr || rhs_info == nullptr) {
        return lhs < rhs;
      }
      if (lhs_info->get_line() != rhs_info->get_line()) {
        return lhs_info->get_line() < rhs_info->get_line();
      }
      if (lhs_info->get_col() != rhs_info->get_col()) {
        return lhs_info->get_col() < rhs_info->get_col();
      }
      return lhs < rhs;
    };

    Rose_STL_Container<SgNode *> function_nodes =
        NodeQuery::querySubTree(global_scope, V_SgFunctionDeclaration);
    std::vector<SgFunctionDeclaration *> ordered_function_decls;
    ordered_function_decls.reserve(function_nodes.size());
    for (SgNode *node : function_nodes) {
      SgFunctionDeclaration *decl = isSgFunctionDeclaration(node);
      if (decl == nullptr) {
        continue;
      }
      if (!declaration_outputs_code(decl)) {
        continue;
      }
      if (decl->get_parameterList() == nullptr) {
        continue;
      }
      ordered_function_decls.push_back(decl);
    }
    std::stable_sort(ordered_function_decls.begin(),
                     ordered_function_decls.end(), location_less);

    std::map<SgFunctionDeclaration *, std::vector<bool>> seen_defaults_by_chain;
    for (SgFunctionDeclaration *decl : ordered_function_decls) {
      if (decl == nullptr) {
        continue;
      }

      SgFunctionDeclaration *chain_key =
          isSgFunctionDeclaration(decl->get_firstNondefiningDeclaration());
      if (chain_key == nullptr) {
        chain_key = decl;
      }

      SgFunctionParameterList *params = decl->get_parameterList();
      if (params == nullptr) {
        continue;
      }
      SgInitializedNamePtrList &args = params->get_args();
      std::vector<bool> &seen_defaults = seen_defaults_by_chain[chain_key];
      if (seen_defaults.size() < args.size()) {
        seen_defaults.resize(args.size(), false);
      }

      for (std::size_t i = 0; i < args.size(); ++i) {
        SgInitializedName *arg = args[i];
        if (arg == nullptr) {
          continue;
        }
        SgInitializer *initializer = arg->get_initializer();
        if (initializer == nullptr) {
          continue;
        }
        if (seen_defaults[i]) {
          arg->set_initializer(nullptr);
          if (initializer->get_parent() == arg) {
            initializer->set_parent(nullptr);
          }
        } else {
          seen_defaults[i] = true;
        }
      }
    }
  }

  if (global_scope != nullptr) {
    auto fix_arg_parents = [](SgNode *owner, SgTemplateArgumentPtrList &args) {
      if (owner == nullptr) {
        return;
      }
      for (SgTemplateArgument *arg : args) {
        if (arg == nullptr) {
          continue;
        }
        SgNode *parent = arg->get_parent();
        if (parent == nullptr || isSgScopeStatement(parent) != nullptr ||
            isSgFile(parent) != nullptr) {
          arg->set_parent(owner);
        }
      }
    };

    Rose_STL_Container<SgNode *> decl_nodes =
        NodeQuery::querySubTree(global_scope, V_SgDeclarationStatement);
    for (SgNode *node : decl_nodes) {
      SgDeclarationStatement *decl = isSgDeclarationStatement(node);
      if (decl == nullptr) {
        continue;
      }
      SgDeclarationStatement *parent_decl =
          decl->get_firstNondefiningDeclaration();
      if (parent_decl == nullptr) {
        parent_decl = decl;
      }

      if (SgTemplateInstantiationDecl *inst =
              isSgTemplateInstantiationDecl(decl)) {
        fix_arg_parents(parent_decl, inst->get_templateArguments());
      } else if (SgTemplateInstantiationFunctionDecl *inst_func =
                     isSgTemplateInstantiationFunctionDecl(decl)) {
        fix_arg_parents(parent_decl, inst_func->get_templateArguments());
      } else if (SgTemplateInstantiationMemberFunctionDecl *inst_mem =
                     isSgTemplateInstantiationMemberFunctionDecl(decl)) {
        fix_arg_parents(parent_decl, inst_mem->get_templateArguments());
      } else if (SgTemplateInstantiationTypedefDeclaration *inst_typedef =
                     isSgTemplateInstantiationTypedefDeclaration(decl)) {
        fix_arg_parents(parent_decl, inst_typedef->get_templateArguments());
      } else if (SgTemplateClassDeclaration *tmpl_class =
                     isSgTemplateClassDeclaration(decl)) {
        fix_arg_parents(parent_decl,
                        tmpl_class->get_templateSpecializationArguments());
      } else if (SgTemplateFunctionDeclaration *tmpl_func =
                     isSgTemplateFunctionDeclaration(decl)) {
        fix_arg_parents(parent_decl,
                        tmpl_func->get_templateSpecializationArguments());
      } else if (SgTemplateMemberFunctionDeclaration *tmpl_mem =
                     isSgTemplateMemberFunctionDeclaration(decl)) {
        fix_arg_parents(parent_decl,
                        tmpl_mem->get_templateSpecializationArguments());
      } else if (SgTemplateVariableDeclaration *tmpl_var =
                     isSgTemplateVariableDeclaration(decl)) {
        fix_arg_parents(parent_decl,
                        tmpl_var->get_templateSpecializationArguments());
      } else if (SgTemplateTypedefDeclaration *tmpl_typedef =
                     isSgTemplateTypedefDeclaration(decl)) {
        fix_arg_parents(parent_decl,
                        tmpl_typedef->get_templateSpecializationArguments());
      } else if (SgNonrealDecl *nonreal = isSgNonrealDecl(decl)) {
        fix_arg_parents(parent_decl, nonreal->get_tpl_args());
      }
    }

    Rose_STL_Container<SgNode *> tmpl_types =
        NodeQuery::querySubTree(global_scope, V_SgTemplateType);
    for (SgNode *node : tmpl_types) {
      SgTemplateType *tmpl_type = isSgTemplateType(node);
      if (tmpl_type != nullptr) {
        fix_arg_parents(tmpl_type, tmpl_type->get_tpl_args());
        fix_arg_parents(tmpl_type, tmpl_type->get_part_spec_tpl_args());
      }
    }
  }

  if (global_scope != nullptr) {
    auto effective_info = [](SgLocatedNode *node) -> Sg_File_Info * {
      if (node == nullptr) {
        return nullptr;
      }
      if (Sg_File_Info *info = node->get_startOfConstruct();
          info != nullptr && info->get_line() > 0) {
        return info;
      }
      if (Sg_File_Info *info = node->get_file_info();
          info != nullptr && info->get_line() > 0) {
        return info;
      }
      return node->get_endOfConstruct();
    };
    auto same_file = [](const Sg_File_Info *lhs, const Sg_File_Info *rhs) {
      if (lhs == nullptr || rhs == nullptr) {
        return false;
      }
      if (lhs->get_file_id() == rhs->get_file_id()) {
        return true;
      }
      if (!lhs->get_filenameString().empty() &&
          !rhs->get_filenameString().empty()) {
        return lhs->get_filenameString() == rhs->get_filenameString();
      }
      return false;
    };
    auto source_before = [](const Sg_File_Info *lhs, const Sg_File_Info *rhs) {
      if (lhs == nullptr || rhs == nullptr) {
        return false;
      }
      if (lhs->get_line() != rhs->get_line()) {
        return lhs->get_line() < rhs->get_line();
      }
      return lhs->get_col() < rhs->get_col();
    };
    auto preproc_by_location = [](const PreprocessingInfo *lhs,
                                  const PreprocessingInfo *rhs) {
      if (lhs == nullptr || rhs == nullptr) {
        return lhs < rhs;
      }
      const Sg_File_Info *lhs_info = lhs->get_file_info();
      const Sg_File_Info *rhs_info = rhs->get_file_info();
      if (lhs_info == nullptr || rhs_info == nullptr) {
        return lhs_info < rhs_info;
      }
      if (lhs_info->get_line() != rhs_info->get_line()) {
        return lhs_info->get_line() < rhs_info->get_line();
      }
      if (lhs_info->get_col() != rhs_info->get_col()) {
        return lhs_info->get_col() < rhs_info->get_col();
      }
      return lhs < rhs;
    };

    Rose_STL_Container<SgNode *> function_nodes =
        NodeQuery::querySubTree(global_scope, V_SgFunctionDeclaration);
    for (SgNode *node : function_nodes) {
      SgFunctionDeclaration *decl = isSgFunctionDeclaration(node);
      if (decl == nullptr) {
        continue;
      }

      Sg_File_Info *decl_start = effective_info(decl);
      if (decl_start == nullptr || decl_start->get_line() <= 0) {
        continue;
      }

      std::vector<PreprocessingInfo *> moves;
      Rose_STL_Container<SgNode *> owned_nodes =
          NodeQuery::querySubTree(decl, V_SgLocatedNode);
      for (SgNode *owned : owned_nodes) {
        SgLocatedNode *owner = isSgLocatedNode(owned);
        if (owner == nullptr || owner == decl) {
          continue;
        }

        AttachedPreprocessingInfoType *attached =
            owner->getAttachedPreprocessingInfo();
        if (attached == nullptr || attached->empty()) {
          continue;
        }

        for (auto it = attached->begin(); it != attached->end();) {
          PreprocessingInfo *info = *it;
          Sg_File_Info *info_fi =
              info != nullptr ? info->get_file_info() : nullptr;
          if (info_fi == nullptr || info_fi->get_line() <= 0 ||
              !same_file(info_fi, decl_start) ||
              !source_before(info_fi, decl_start)) {
            ++it;
            continue;
          }

          info->setRelativePosition(PreprocessingInfo::before);
          moves.push_back(info);
          it = attached->erase(it);
        }
      }

      if (moves.empty()) {
        continue;
      }

      for (PreprocessingInfo *info : moves) {
        decl->addToAttachedPreprocessingInfo(info);
      }

      if (AttachedPreprocessingInfoType *decl_attached =
              decl->getAttachedPreprocessingInfo()) {
        std::stable_sort(decl_attached->begin(), decl_attached->end(),
                         preproc_by_location);
      }
    }
  }

  SgSourceFile *source_file = isSgSourceFile(global_scope->get_parent());
  if (source_file != nullptr && !source_file->get_openmp_processed()) {
    bool has_omp = false;
    bool has_acc = false;
    Rose_STL_Container<SgNode *> pragmas =
        NodeQuery::querySubTree(global_scope, V_SgPragmaDeclaration);
    for (SgNode *pragma_node : pragmas) {
      SgPragmaDeclaration *pragma_decl = isSgPragmaDeclaration(pragma_node);
      if (pragma_decl == nullptr) {
        continue;
      }
      std::string pragma_text = pragma_decl->get_pragma()->get_pragma();
      std::istringstream istr(pragma_text);
      std::string key;
      istr >> key;
      if (key == "omp") {
        has_omp = true;
      } else if (key == "acc") {
        has_acc = true;
      }
      if (has_omp && has_acc) {
        break;
      }
    }
    if (has_omp || has_acc) {
      bool openmp_explicitly_disabled = false;
      const std::vector<std::string> &args =
          source_file->get_originalCommandLineArgumentList();
      for (const std::string &arg : args) {
        if (arg.rfind("-fopenmp=", 0) == 0) {
          std::string value = arg.substr(9);
          std::transform(value.begin(), value.end(), value.begin(),
                         [](unsigned char c) {
                           return static_cast<char>(std::tolower(c));
                         });
          if (value == "0" || value == "false" || value == "disabled") {
            openmp_explicitly_disabled = true;
            break;
          }
        }
      }

      if (has_omp && !source_file->get_openmp() &&
          !openmp_explicitly_disabled) {
        source_file->set_openmp(true);
        bool has_explicit_processing_flag =
            source_file->get_openmp_ast_only() ||
            source_file->get_openmp_lowering() ||
            source_file->get_openmp_analyzing();
        if (!has_explicit_processing_flag &&
            source_file->get_openmp_parse_only()) {
          source_file->set_openmp_parse_only(false);
          source_file->set_openmp_ast_only(true);
        }
      }

      if (has_acc && !source_file->get_openacc()) {
        source_file->set_openacc(true);
        if (!source_file->get_openacc_ast_only()) {
          source_file->set_openacc_parse_only(false);
          source_file->set_openacc_ast_only(true);
        }
      }
    }
  }

  if (global_scope != nullptr) {
    auto has_real_source_file_info = [](Sg_File_Info *fi) -> bool {
      return fi != nullptr && fi->get_line() > 0 &&
             !fi->get_filenameString().empty() &&
             fi->get_filenameString() != "NULL_FILE" &&
             !fi->isCompilerGenerated() && !fi->isFrontendSpecific() &&
             !fi->isSourcePositionUnavailableInFrontend();
    };
    auto located_has_real_source = [&](SgLocatedNode *node) -> bool {
      return node != nullptr &&
             (has_real_source_file_info(node->get_startOfConstruct()) ||
              has_real_source_file_info(node->get_file_info()) ||
              has_real_source_file_info(node->get_endOfConstruct()));
    };
    auto clone_file_info = [](const Sg_File_Info *prototype,
                              SgLocatedNode *parent,
                              bool output_enabled) -> Sg_File_Info * {
      if (prototype == nullptr) {
        return nullptr;
      }
      Sg_File_Info *copy = new Sg_File_Info(*prototype);
      copy->set_parent(parent);
      if (output_enabled) {
        copy->setOutputInCodeGeneration();
      }
      return copy;
    };
    auto find_class_anchor =
        [&](SgClassDeclaration *class_decl) -> SgLocatedNode * {
      if (class_decl == nullptr) {
        return nullptr;
      }

      SgClassDeclaration *def_decl =
          isSgClassDeclaration(class_decl->get_definingDeclaration());
      if (def_decl == nullptr) {
        def_decl = class_decl;
      }

      if (located_has_real_source(def_decl)) {
        return def_decl;
      }

      SgClassDefinition *class_def = def_decl->get_definition();
      if (located_has_real_source(class_def)) {
        return class_def;
      }
      if (class_def == nullptr) {
        return nullptr;
      }

      if (SgClassDeclaration *definition_decl =
              isSgClassDeclaration(class_def->get_declaration())) {
        if (definition_decl != def_decl &&
            located_has_real_source(definition_decl)) {
          return definition_decl;
        }
      }

      auto first_source_backed_from_list =
          [&](const SgDeclarationStatementPtrList &list) -> SgLocatedNode * {
        for (SgDeclarationStatement *member : list) {
          if (SgLocatedNode *located = isSgLocatedNode(member)) {
            if (located_has_real_source(located)) {
              return located;
            }
          }
        }
        return nullptr;
      };

      if (SgLocatedNode *member_anchor =
              first_source_backed_from_list(class_def->get_members())) {
        return member_anchor;
      }
      if (SgLocatedNode *member_anchor =
              first_source_backed_from_list(class_def->getDeclarationList())) {
        return member_anchor;
      }

      Rose_STL_Container<SgNode *> nested_decl_nodes =
          NodeQuery::querySubTree(class_def, V_SgDeclarationStatement);
      for (SgNode *node : nested_decl_nodes) {
        if (SgLocatedNode *located = isSgLocatedNode(node)) {
          if (located != def_decl && located_has_real_source(located)) {
            return located;
          }
        }
      }

      return nullptr;
    };
    auto sync_class_decl_source_from_anchor = [&](SgClassDeclaration *decl) {
      if (decl == nullptr || located_has_real_source(decl)) {
        return;
      }
      SgLocatedNode *anchor = find_class_anchor(decl);
      if (anchor == nullptr) {
        return;
      }

      const bool output_enabled =
          (decl->get_file_info() != nullptr &&
           decl->get_file_info()->isOutputInCodeGeneration()) ||
          (decl->get_startOfConstruct() != nullptr &&
           decl->get_startOfConstruct()->isOutputInCodeGeneration()) ||
          (decl->get_endOfConstruct() != nullptr &&
           decl->get_endOfConstruct()->isOutputInCodeGeneration());

      const Sg_File_Info *anchor_start =
          has_real_source_file_info(anchor->get_startOfConstruct())
              ? anchor->get_startOfConstruct()
              : anchor->get_file_info();
      const Sg_File_Info *anchor_end =
          has_real_source_file_info(anchor->get_endOfConstruct())
              ? anchor->get_endOfConstruct()
              : anchor_start;
      if (anchor_start == nullptr || anchor_end == nullptr) {
        return;
      }

      if (Sg_File_Info *start_copy =
              clone_file_info(anchor_start, decl, output_enabled)) {
        decl->set_startOfConstruct(start_copy);
      }
      if (Sg_File_Info *end_copy =
              clone_file_info(anchor_end, decl, output_enabled)) {
        decl->set_endOfConstruct(end_copy);
      }
      if (Sg_File_Info *file_copy =
              clone_file_info(anchor_start, decl, output_enabled)) {
        decl->set_file_info(file_copy);
      }
    };

    Rose_STL_Container<SgNode *> class_nodes =
        NodeQuery::querySubTree(global_scope, V_SgClassDeclaration);
    for (SgNode *node : class_nodes) {
      if (SgClassDeclaration *decl = isSgClassDeclaration(node)) {
        if (!decl->isForward()) {
          sync_class_decl_source_from_anchor(decl);
        }
      }
    }

    auto suppress_decl_output = [](SgLocatedNode *node) {
      if (node == nullptr) {
        return;
      }
      if (Sg_File_Info *fi = node->get_file_info()) {
        fi->unsetOutputInCodeGeneration();
      }
      if (Sg_File_Info *fi = node->get_startOfConstruct()) {
        fi->unsetOutputInCodeGeneration();
      }
      if (Sg_File_Info *fi = node->get_endOfConstruct()) {
        fi->unsetOutputInCodeGeneration();
      }
    };
    auto detach_decl_from_scope_lists = [](SgDeclarationStatement *decl) {
      if (decl == nullptr) {
        return;
      }
      auto detach_from_list = [&](SgDeclarationStatementPtrList &list) {
        for (auto it = list.begin(); it != list.end();) {
          if (*it == decl) {
            it = list.erase(it);
          } else {
            ++it;
          }
        }
      };
      if (SgScopeStatement *parent_scope =
              isSgScopeStatement(decl->get_parent())) {
        if (SgGlobal *global = isSgGlobal(parent_scope)) {
          detach_from_list(global->get_declarations());
        } else if (SgNamespaceDefinitionStatement *ns_def =
                       isSgNamespaceDefinitionStatement(parent_scope)) {
          detach_from_list(ns_def->get_declarations());
        } else if (SgDeclarationScope *decl_scope =
                       isSgDeclarationScope(parent_scope)) {
          detach_from_list(decl_scope->get_declarations());
        }
      }
      if (SgScopeStatement *scope = decl->get_scope()) {
        if (scope != isSgScopeStatement(decl->get_parent())) {
          if (SgGlobal *global = isSgGlobal(scope)) {
            detach_from_list(global->get_declarations());
          } else if (SgNamespaceDefinitionStatement *ns_def =
                         isSgNamespaceDefinitionStatement(scope)) {
            detach_from_list(ns_def->get_declarations());
          } else if (SgDeclarationScope *decl_scope =
                         isSgDeclarationScope(scope)) {
            detach_from_list(decl_scope->get_declarations());
          }
        }
      }
    };

    Rose_STL_Container<SgNode *> typedef_nodes =
        NodeQuery::querySubTree(global_scope, V_SgTypedefDeclaration);
    for (SgNode *node : typedef_nodes) {
      SgTypedefDeclaration *typedef_decl = isSgTypedefDeclaration(node);
      if (typedef_decl == nullptr ||
          !typedef_decl->get_typedefBaseTypeContainsDefiningDeclaration()) {
        continue;
      }

      SgClassDeclaration *class_def_decl =
          isSgClassDeclaration(typedef_decl->get_declaration());
      if (class_def_decl == nullptr) {
        continue;
      }

      detach_decl_from_scope_lists(class_def_decl);
      class_def_decl->set_parent(typedef_decl);
      class_def_decl->set_isAutonomousDeclaration(false);
      suppress_decl_output(class_def_decl);

      if (SgClassDefinition *class_def = class_def_decl->get_definition()) {
        class_def->set_parent(class_def_decl);
        suppress_decl_output(class_def);
      }
    }
  }
}

SgGlobal *ClangToSageTranslator::getGlobalScope() const {
  return p_global_scope;
}

ClangToSageTranslator::ClangToSageTranslator(
    clang::CompilerInstance *compiler_instance, Language language_,
    SgSourceFile *sage_source_file,
    SagePreprocessorRecord *preprocessor_recorder)
    : clang::ASTConsumer(), p_decl_translation_map(), p_stmt_translation_map(),
      p_type_translation_map(), p_template_decl_cache(),
      p_template_inst_cache(), p_global_scope(NULL),
      p_class_type_decl_first_see_in_type(),
      p_enum_type_decl_first_see_in_type(),
      p_compiler_instance(compiler_instance),
      p_sage_preprocessor_recorder(preprocessor_recorder),
      p_sage_source_file(sage_source_file), language(language_),
      p_openmp_pragma_callback(nullptr) {
  ROSE_ASSERT(p_sage_preprocessor_recorder != nullptr);
}

ClangToSageTranslator::~ClangToSageTranslator() {}

/* (protected) Helper methods */

namespace {

void setFileInfosWithParent(SgLocatedNode *located_node, Sg_File_Info *start_fi,
                            Sg_File_Info *end_fi) {
  ROSE_ASSERT(located_node != NULL);
  ROSE_ASSERT(start_fi != NULL);
  ROSE_ASSERT(end_fi != NULL);

  located_node->set_startOfConstruct(start_fi);
  located_node->set_endOfConstruct(end_fi);

  start_fi->set_parent(located_node);
  end_fi->set_parent(located_node);
}

void setFileInfosWithParent(SgInitializedName *init_name,
                            Sg_File_Info *start_fi, Sg_File_Info *end_fi) {
  ROSE_ASSERT(init_name != NULL);
  ROSE_ASSERT(start_fi != NULL);
  ROSE_ASSERT(end_fi != NULL);

  init_name->set_startOfConstruct(start_fi);
  init_name->set_endOfConstruct(end_fi);

  start_fi->set_parent(init_name);
  end_fi->set_parent(init_name);
}

} // namespace

void ClangToSageTranslator::applySourceRange(SgNode *node,
                                             clang::SourceRange source_range) {
  SgLocatedNode *located_node = isSgLocatedNode(node);
  SgInitializedName *init_name = isSgInitializedName(node);

#if DEBUG_SOURCE_LOCATION
  std::cerr << "Set File_Info for " << node << " of type " << node->class_name()
            << std::endl;
#endif

  if (located_node == NULL && init_name == NULL) {
    // ROOT CAUSE FIX: Some generated nodes (like SgVarRefExp placeholders for
    // unresolved members) may not be located nodes. Just skip them instead of
    // fatal error.
#if DEBUG_SOURCE_LOCATION
    std::cerr << "Warning: Skipping source range application for non-located "
                 "node of type "
              << node->class_name() << std::endl;
#endif
    return;
  } else {
    if (located_node != NULL) {
      Sg_File_Info *fi = located_node->get_startOfConstruct();
      if (fi != NULL)
        delete fi;
      fi = located_node->get_endOfConstruct();
      if (fi != NULL)
        delete fi;
    } else {
      if (init_name != NULL) {
        Sg_File_Info *fi = init_name->get_startOfConstruct();
        if (fi != NULL)
          delete fi;
        fi = init_name->get_endOfConstruct();
        if (fi != NULL)
          delete fi;
      }
    }
  }

  Sg_File_Info *start_fi = NULL;
  Sg_File_Info *end_fi = NULL;
  unsigned long long translation_unit_order = 0;
  bool has_translation_unit_order = false;

  if (source_range.isValid()) {
    clang::SourceLocation begin = source_range.getBegin();
    clang::SourceLocation end = source_range.getEnd();

    if (begin.isValid() && end.isValid()) {
      clang::SourceManager &sm = p_compiler_instance->getSourceManager();
      const clang::LangOptions &lang_opts = p_compiler_instance->getLangOpts();
      auto move_to_line_end =
          [&](clang::SourceLocation loc) -> clang::SourceLocation {
        if (!loc.isValid() || !loc.isFileID()) {
          return loc;
        }
        bool invalid = false;
        const char *cursor = sm.getCharacterData(loc, &invalid);
        if (invalid || cursor == nullptr) {
          return loc;
        }

        unsigned offset = 0;
        while (cursor[offset] != '\0' && cursor[offset] != '\n' &&
               cursor[offset] != '\r') {
          ++offset;
        }

        if (offset == 0) {
          return loc;
        }

        return loc.getLocWithOffset(static_cast<int>(offset) - 1);
      };
      auto map_to_ancestor_file =
          [&](clang::SourceLocation loc, clang::FileID target_file,
              bool extend_to_line_end) -> clang::SourceLocation {
        if (!loc.isValid() || target_file.isInvalid()) {
          return clang::SourceLocation();
        }

        clang::SourceLocation current = loc;
        if (current.isMacroID()) {
          current = sm.getExpansionLoc(current);
        }
        if (!current.isValid()) {
          return clang::SourceLocation();
        }

        clang::FileID current_file = sm.getFileID(current);
        while (!current_file.isInvalid() && current_file != target_file) {
          clang::SourceLocation include_loc = sm.getIncludeLoc(current_file);
          if (!include_loc.isValid()) {
            return clang::SourceLocation();
          }
          current = sm.getExpansionLoc(include_loc);
          if (!current.isValid()) {
            return clang::SourceLocation();
          }
          current_file = sm.getFileID(current);
        }

        if (current_file != target_file) {
          return clang::SourceLocation();
        }

        return extend_to_line_end ? move_to_line_end(current) : current;
      };

      clang::SourceLocation spelling_begin = sm.getSpellingLoc(begin);
      clang::SourceLocation spelling_end = sm.getSpellingLoc(end);
      clang::SourceLocation expansion_begin = sm.getExpansionLoc(begin);
      clang::SourceLocation expansion_end = sm.getExpansionLoc(end);

      const bool begin_is_macro = begin.isMacroID();
      const bool end_is_macro = end.isMacroID();
      const bool begin_needs_expansion =
          begin_is_macro ||
          (expansion_begin.isValid() &&
           expansion_begin.getRawEncoding() != begin.getRawEncoding());
      const bool end_needs_expansion =
          end_is_macro ||
          (expansion_end.isValid() &&
           expansion_end.getRawEncoding() != end.getRawEncoding());

      if (begin_needs_expansion && expansion_begin.isValid()) {
#if DEBUG_SOURCE_LOCATION
        std::cerr << "\tDump SourceLocation begin as it is a MacroID: ";
        begin.dump(sm);
        std::cerr << std::endl;
#endif
        begin = expansion_begin;
      }

      if (end_needs_expansion && expansion_end.isValid()) {
#if DEBUG_SOURCE_LOCATION
        std::cerr << "\tDump SourceLocation end as it is a MacroID: ";
        end.dump(sm);
        std::cerr << std::endl;
#endif
        end = expansion_end;
      }

      ROSE_ASSERT(begin.isValid());
      ROSE_ASSERT(end.isValid());

      translation_unit_order =
          static_cast<unsigned long long>(begin.getRawEncoding());
      has_translation_unit_order = true;

      clang::FileID file_begin = sm.getFileID(begin);
      clang::FileID file_end = sm.getFileID(end);
      bool normalized_cross_file_end = false;

      if (!sm.isWrittenInMainFile(begin) &&
          sm.isWrittenInMainFile(spelling_begin)) {
        begin = spelling_begin;
        end = spelling_end;
        file_begin = sm.getFileID(begin);
        file_end = sm.getFileID(end);
      }

      if (!file_begin.isInvalid() && !file_end.isInvalid() &&
          file_begin != file_end) {
        if (clang::SourceLocation mapped_end =
                map_to_ancestor_file(end, file_begin, true);
            mapped_end.isValid()) {
          end = mapped_end;
          file_end = sm.getFileID(end);
          normalized_cross_file_end = true;
        }
      }

      if (!file_begin.isInvalid() && !file_end.isInvalid() &&
          file_begin != file_end) {
        end = begin;
        file_end = file_begin;
        normalized_cross_file_end = false;
      }

      if (!file_begin.isInvalid() && !file_end.isInvalid()) {
        auto end_buffer = sm.getBufferDataOrNone(file_end);
        const bool in_main_file = file_begin == sm.getMainFileID();
        bool inv_begin_line = false;
        bool inv_begin_col = false;
        bool inv_end_line = false;
        bool inv_end_col = false;

        unsigned ls = sm.getSpellingLineNumber(begin, &inv_begin_line);
        unsigned cs = sm.getSpellingColumnNumber(begin, &inv_begin_col);

        // Token-stream mapping expects end-of-construct to be
        // token-accurate. Clang SourceRange ends are often at the
        // *start* of the last token; convert to the last character
        // of that token.
        clang::SourceLocation end_for_fi = end;
        bool can_lex_end = end_for_fi.isFileID() && in_main_file &&
                           end_buffer && !normalized_cross_file_end;
        if (can_lex_end) {
          unsigned end_offset = sm.getFileOffset(end_for_fi);
          if (end_offset >= end_buffer->size()) {
            can_lex_end = false;
          }
        }
        if (can_lex_end) {
          bool invalid_char_data = false;
          (void)sm.getCharacterData(end_for_fi, &invalid_char_data);
          if (invalid_char_data) {
            can_lex_end = false;
          }
        }
        if (can_lex_end) {
          clang::SourceLocation after_token =
              clang::Lexer::getLocForEndOfToken(end_for_fi, 0, sm, lang_opts);
          if (after_token.isValid() &&
              after_token.getRawEncoding() != end_for_fi.getRawEncoding()) {
            clang::SourceLocation last_char = after_token.getLocWithOffset(-1);
            if (last_char.isValid()) {
              end_for_fi = last_char;
            }
          }
        }

        unsigned le = sm.getSpellingLineNumber(end_for_fi, &inv_end_line);
        unsigned ce = sm.getSpellingColumnNumber(end_for_fi, &inv_end_col);

        if (inv_begin_line || inv_begin_col || inv_end_line || inv_end_col) {
          ROSE_ASSERT(!"Should not happen as everything have been "
                       "check before...");
        }

        // getFileEntryForID still returns const FileEntry*.
        const clang::FileEntry *fileEntry = sm.getFileEntryForID(file_begin);
        std::string file;
        if (fileEntry) {
          // FileEntry uses tryGetRealPathName() instead of
          // getName().
          file = fileEntry->tryGetRealPathName().str();
        }
        if (file.empty()) {
          file = sm.getFilename(begin).str();
        }
        if ((file.empty() || file == "<built-in>") &&
            sm.isWrittenInMainFile(begin)) {
          const clang::FileEntry *main_entry =
              sm.getFileEntryForID(sm.getMainFileID());
          if (main_entry) {
            file = main_entry->tryGetRealPathName().str();
          }
        }
        if (file.empty()) {
          clang::PresumedLoc ploc = sm.getPresumedLoc(begin);
          if (ploc.isValid()) {
            file = ploc.getFilename();
          }
        }

        if (!file.empty()) {
          // start_fi = new Sg_File_Info(file, ls, cs);
          // end_fi   = new Sg_File_Info(file, le, ce);
          // Mark nodes for code generation only when they originate
          // from the main file being compiled. This preserves
          // ROSE's default behavior of not inlining declarations
          // from included headers into the generated output file
          // (e.g. copyAST_copytest2007_40), while still allowing
          // template default-argument logic to reason about which
          // declarations will be unparsed in the main file (Issue
          // 69).
          if (file.find("clang-builtin-c.h") != std::string::npos) {
            start_fi = new Sg_File_Info(file, ls, cs);
            end_fi = new Sg_File_Info(file, le, ce);

            // DQ (11/29/2020): This is not doing what I had hoped
            // it would do. I think the solution is to use the
            // -DSKIP_ROSE_BUILTIN_DECLARATIONS option, but that is
            // not working as I expected either.  Time to go home.
            // start_fi =
            // Sg_File_Info::generateDefaultFileInfoForCompilerGeneratedNode();
            // end_fi   =
            // Sg_File_Info::generateDefaultFileInfoForCompilerGeneratedNode();
            start_fi->set_classificationBitField(
                Sg_File_Info::e_frontend_specific);
            end_fi->set_classificationBitField(
                Sg_File_Info::e_frontend_specific);
            if (in_main_file) {
              start_fi->setOutputInCodeGeneration();
              end_fi->setOutputInCodeGeneration();
            }
          } else {
            start_fi = new Sg_File_Info(file, ls, cs);
            end_fi = new Sg_File_Info(file, le, ce);
            if (in_main_file) {
              start_fi->setOutputInCodeGeneration();
              end_fi->setOutputInCodeGeneration();
            }
          }

          if (begin_is_macro || end_is_macro) {
            auto spelling_stays_in_logical_file =
                [&](const clang::SourceLocation &logical_loc,
                    const clang::SourceLocation &spelling_loc) -> bool {
              if (!logical_loc.isValid() || !spelling_loc.isValid()) {
                return false;
              }
              clang::FileID logical_id = sm.getFileID(logical_loc);
              clang::FileID spelling_id = sm.getFileID(spelling_loc);
              return !logical_id.isInvalid() && logical_id == spelling_id;
            };
            auto set_physical_info = [&](Sg_File_Info *fi,
                                         const clang::SourceLocation &loc) {
              if (fi == NULL || !loc.isValid())
                return;
              clang::FileID spelling_id = sm.getFileID(loc);
              if (!spelling_id.isInvalid()) {
                const clang::FileEntry *spelling_entry =
                    sm.getFileEntryForID(spelling_id);
                if (spelling_entry) {
                  std::string spelling_file =
                      spelling_entry->tryGetRealPathName().str();
                  if (!spelling_file.empty())
                    fi->set_physical_filename(spelling_file);
                }
              }
              bool inv_line = false;
              unsigned spelling_line = sm.getSpellingLineNumber(loc, &inv_line);
              if (!inv_line && spelling_line > 0)
                fi->set_physical_line(spelling_line);
            };

            // Preserve spelling locations only when the macro spelling remains
            // in the same physical file as the logical construct. Cross-file
            // spellings such as C's bool from stdbool.h otherwise cause
            // token-stream mapping to anchor declarations to the macro
            // definition instead of the source declaration.
            if (begin_is_macro &&
                spelling_stays_in_logical_file(begin, spelling_begin)) {
              set_physical_info(start_fi, spelling_begin);
            }
            if (end_is_macro &&
                spelling_stays_in_logical_file(end, spelling_end)) {
              set_physical_info(end_fi, spelling_end);
            }
          }

#if DEBUG_SOURCE_LOCATION
          std::cerr << "\tCreate FI for node in " << file << ":" << ls << ":"
                    << cs << std::endl;
#endif
        }
#if DEBUG_SOURCE_LOCATION
        else {
          std::cerr << "\tDump SourceLocation for \"Invalid FileID\": "
                    << std::endl
                    << "\t";
          begin.dump(p_compiler_instance->getSourceManager());
          std::cerr << std::endl << "\t";
          end.dump(p_compiler_instance->getSourceManager());
          std::cerr << std::endl;
        }
#endif
      }
    }
  }

  if (start_fi == NULL && end_fi == NULL) {
    start_fi = Sg_File_Info::generateDefaultFileInfoForCompilerGeneratedNode();
    end_fi = Sg_File_Info::generateDefaultFileInfoForCompilerGeneratedNode();

    start_fi->setCompilerGenerated();
    end_fi->setCompilerGenerated();
#if DEBUG_SOURCE_LOCATION
    std::cerr << "Create FI for compiler generated node" << std::endl;
#endif
  } else {
    if (start_fi == NULL || end_fi == NULL) {
      ROSE_ASSERT(!"start_fi == NULL xor end_fi == NULL");
    }
  }

  if (located_node != NULL) {
    setFileInfosWithParent(located_node, start_fi, end_fi);

    if (SgMemberFunctionDeclaration *member_decl =
            isSgMemberFunctionDeclaration(located_node)) {
      if (SgCtorInitializerList *ctor_list =
              member_decl->get_CtorInitializerList()) {
        if (ctor_list->get_startOfConstruct() == NULL ||
            ctor_list->get_endOfConstruct() == NULL) {
          Sg_File_Info *ctor_start = nullptr;
          Sg_File_Info *ctor_end = nullptr;
          if (member_decl->get_startOfConstruct() != NULL) {
            ctor_start = new Sg_File_Info(*member_decl->get_startOfConstruct());
          }
          if (member_decl->get_endOfConstruct() != NULL) {
            ctor_end = new Sg_File_Info(*member_decl->get_endOfConstruct());
          }
          if (ctor_start != nullptr && ctor_end != nullptr) {
            setFileInfosWithParent(ctor_list, ctor_start, ctor_end);
          } else {
            delete ctor_start;
            delete ctor_end;
          }
        }
      }
    }

    // Pei-Hung (09/29/2022) SgExpression::get_file_info() checks and
    // returns get_operatorPosition(), so ensure operatorPosition is set.
    SgExpression *expr = isSgExpression(located_node);
    if (expr != NULL) {
      if (expr->get_operatorPosition() == NULL) {
        expr->set_operatorPosition(start_fi);
      } else {
        // CFE FIX: If operatorPosition was already created by
        // setSourcePositionToDefault, update it to match the real source
        // location.
        delete expr->get_operatorPosition();
        Sg_File_Info *op_fi = new Sg_File_Info(*start_fi);
        expr->set_operatorPosition(op_fi);
        op_fi->set_parent(expr);
      }
    }
  } else {
    if (init_name != NULL) {
      setFileInfosWithParent(init_name, start_fi, end_fi);
    }
  }

  if (node != nullptr && has_translation_unit_order) {
    node->setAttribute(
        kTranslationUnitOrderAttributeName,
        new TranslationUnitOrderAttribute(translation_unit_order));
  }
}

void ClangToSageTranslator::setCompilerGeneratedFileInfo(SgNode *node,
                                                         bool to_be_unparse) {
  Sg_File_Info *start_fi =
      Sg_File_Info::generateDefaultFileInfoForCompilerGeneratedNode();
  Sg_File_Info *end_fi =
      Sg_File_Info::generateDefaultFileInfoForCompilerGeneratedNode();

  start_fi->setCompilerGenerated();
  end_fi->setCompilerGenerated();
  start_fi->unsetOutputInCodeGeneration();
  end_fi->unsetOutputInCodeGeneration();

  if (to_be_unparse) {
    start_fi->setOutputInCodeGeneration();
    end_fi->setOutputInCodeGeneration();
  }

  ROSE_ASSERT(start_fi != NULL && end_fi != NULL);

#if DEBUG_SOURCE_LOCATION
  std::cerr << "Set File_Info for " << node << " of type " << node->class_name()
            << std::endl;
#endif

  SgLocatedNode *located_node = isSgLocatedNode(node);
  SgInitializedName *init_name = isSgInitializedName(node);

  if (located_node == NULL && init_name == NULL) {
    std::cerr << "Consistency error: try to set a Sage node which is not a "
                 "SgLocatedNode or a SgInitializedName as compiler generated"
              << std::endl;
    exit(-1);
  } else if (located_node != NULL) {
    Sg_File_Info *fi = located_node->get_startOfConstruct();
    if (fi != NULL)
      delete fi;
    fi = located_node->get_endOfConstruct();
    if (fi != NULL)
      delete fi;

    setFileInfosWithParent(located_node, start_fi, end_fi);

    // Pei-Hung (07/12/2023) Ensure SgExpression file info is set so
    // get_file_info() doesn't return null for compiler-generated nodes.
    if (SgExpression *expr = isSgExpression(located_node)) {
      Sg_File_Info *expr_fi = expr->get_file_info();
      if (expr_fi != NULL && expr_fi != start_fi && expr_fi != end_fi) {
        delete expr_fi;
      }
      expr->set_file_info(start_fi);
    }

    // CFE FIX: If operatorPosition exists, update it to match
    // compiler-generated flags
    SgExpression *expr = isSgExpression(located_node);
    if (expr != NULL && expr->get_operatorPosition() != NULL) {
      delete expr->get_operatorPosition();
      Sg_File_Info *op_fi =
          Sg_File_Info::generateDefaultFileInfoForCompilerGeneratedNode();
      op_fi->setCompilerGenerated();
      op_fi->unsetOutputInCodeGeneration();
      if (to_be_unparse) {
        op_fi->setOutputInCodeGeneration();
      }
      expr->set_operatorPosition(op_fi);
      op_fi->set_parent(expr);
    }
  } else if (init_name != NULL) {
    Sg_File_Info *fi = init_name->get_startOfConstruct();
    if (fi != NULL)
      delete fi;
    fi = init_name->get_endOfConstruct();
    if (fi != NULL)
      delete fi;

    setFileInfosWithParent(init_name, start_fi, end_fi);
  }
}

SgExpression *
ClangToSageTranslator::prepareExpressionForAttachment(SgExpression *expr) {
  if (expr == nullptr) {
    return nullptr;
  }

  // The statement translation cache stores the first translated SgExpression
  // for a Clang Expr. That first translation can still be detached until some
  // owning node adopts it. Deep-copying a detached cached tree preserves the
  // null-parent root in every clone and triggers fixupCopy_scopes warnings.
  // Let the first owner adopt the original tree, then deep-copy only once the
  // cached translation is already attached somewhere in the AST.
  if (expr->get_parent() == nullptr) {
    return expr;
  }

  return SageInterface::copyExpression(expr);
}

/* Overload of ASTConsumer::HandleTranslationUnit, it is the "entry point" */

void ClangToSageTranslator::HandleTranslationUnit(
    clang::ASTContext &ast_context) {
  Traverse(ast_context.getTranslationUnitDecl());
}

/* Preprocessor Stack */

std::pair<Sg_File_Info *, PreprocessingInfo *>
ClangToSageTranslator::preprocessor_top() {
  ROSE_ASSERT(p_sage_preprocessor_recorder != nullptr);
  return p_sage_preprocessor_recorder->top();
}

bool ClangToSageTranslator::preprocessor_pop() {
  ROSE_ASSERT(p_sage_preprocessor_recorder != nullptr);
  return p_sage_preprocessor_recorder->pop();
}

size_t ClangToSageTranslator::preprocessor_list_size() {
  ROSE_ASSERT(p_sage_preprocessor_recorder != nullptr);
  return p_sage_preprocessor_recorder->size();
}

void ClangToSageTranslator::sortPreprocessorList() {
  if (p_sage_preprocessor_recorder != nullptr) {
    p_sage_preprocessor_recorder->sortRecordedDirectives();
  }
}

std::string
ClangToSageTranslator::getSourceText(clang::SourceRange range) const {
  if (p_compiler_instance == nullptr || !range.isValid()) {
    return std::string();
  }
  clang::SourceManager &sm = p_compiler_instance->getSourceManager();
  const clang::LangOptions &lang_opts = p_compiler_instance->getLangOpts();

  clang::SourceLocation begin = range.getBegin();
  clang::SourceLocation end = range.getEnd();
  if (begin.isMacroID()) {
    begin = sm.getSpellingLoc(begin);
  }
  if (end.isMacroID()) {
    end = sm.getSpellingLoc(end);
  }
  if (!begin.isValid() || !end.isValid()) {
    return std::string();
  }

  clang::CharSourceRange char_range =
      clang::CharSourceRange::getTokenRange(begin, end);
  bool invalid = false;
  llvm::StringRef text =
      clang::Lexer::getSourceText(char_range, sm, lang_opts, &invalid);
  if (invalid) {
    return std::string();
  }
  return text.str();
}

// struct NextPreprocessorToInsert

// NextPreprocessorToInsert::NextPreprocessorToInsert(ClangToSageTranslator &
// translator_) :
NextPreprocessorToInsert::NextPreprocessorToInsert(
    ClangToSageTranslator &translator_)
    : cursor(NULL), candidat(NULL), next_to_insert(NULL),
      translator(translator_) {}

bool NextPreprocessorToInsert::advance() {
  if (!translator.preprocessor_pop()) {
    cursor = NULL;
    next_to_insert = NULL;
    candidat = NULL;
    return false;
  }

  std::pair<Sg_File_Info *, PreprocessingInfo *> next =
      translator.preprocessor_top();
  cursor = next.first;
  next_to_insert = next.second;
  candidat = NULL;
  return true;
}

// class

NextPreprocessorToInsert *PreprocessorInserter::evaluateInheritedAttribute(
    SgNode *astNode, NextPreprocessorToInsert *inheritedValue) {
  // Guard against null after final preprocessor insertion
  if (inheritedValue == NULL)
    return NULL;
  if (inheritedValue->cursor == NULL)
    return NULL;

  SgLocatedNode *loc_node = isSgLocatedNode(astNode);
  if (loc_node == NULL)
    return inheritedValue;

  Sg_File_Info *current_pos = loc_node->get_startOfConstruct();
  if (current_pos == NULL) {
    return inheritedValue;
  }

  auto get_effective_file_info = [](SgLocatedNode *node) -> Sg_File_Info * {
    if (node == nullptr) {
      return nullptr;
    }
    auto has_real_line = [](Sg_File_Info *info) -> bool {
      return info != nullptr && info->get_line() > 0;
    };
    if (Sg_File_Info *info = node->get_file_info(); has_real_line(info)) {
      return info;
    }
    if (Sg_File_Info *info = node->get_startOfConstruct();
        has_real_line(info)) {
      return info;
    }
    if (Sg_File_Info *info = node->get_endOfConstruct(); has_real_line(info)) {
      return info;
    }
    if (Sg_File_Info *info = node->get_file_info()) {
      return info;
    }
    if (Sg_File_Info *info = node->get_startOfConstruct()) {
      return info;
    }
    return node->get_endOfConstruct();
  };
  auto should_attach_preproc = [&](SgLocatedNode *node) -> bool {
    // Keep in sync with SgLocatedNode::addToAttachedPreprocessingInfo() hard
    // restrictions: these node kinds are not valid preprocessing anchors.
    // SgFunctionParameterList must also be excluded: top-level directives that
    // precede a function declaration belong to the enclosing declaration, not
    // inside the declarator's parameter-list syntax.
    if (isSgTypedefSeq(node) != nullptr ||
        isSgCatchStatementSeq(node) != nullptr ||
        isSgCtorInitializerList(node) != nullptr ||
        isSgFunctionParameterList(node) != nullptr) {
      return false;
    }

    Sg_File_Info *info = get_effective_file_info(node);
    if (info == nullptr) {
      return false;
    }
    if (!info->isCompilerGenerated()) {
      return true;
    }
    return info->isOutputInCodeGeneration();
  };
  // Non-statement nodes may appear earlier in traversal order than later
  // directives/comments. Limit those attachments to the node's own lexical
  // range so conditionals do not migrate into preceding expressions.
  auto should_attach_preproc_target =
      [&](SgLocatedNode *node, Sg_File_Info *directive_info) -> bool {
    auto same_file = [](Sg_File_Info *lhs, Sg_File_Info *rhs) -> bool {
      if (lhs == nullptr || rhs == nullptr) {
        return false;
      }
      if (lhs->get_file_id() == rhs->get_file_id()) {
        return true;
      }
      if (!lhs->get_filenameString().empty() &&
          !rhs->get_filenameString().empty()) {
        return lhs->get_filenameString() == rhs->get_filenameString();
      }
      return false;
    };
    auto location_leq_local = [](Sg_File_Info *lhs, Sg_File_Info *rhs) -> bool {
      if (lhs == nullptr || rhs == nullptr) {
        return false;
      }
      if (lhs->get_line() != rhs->get_line()) {
        return lhs->get_line() < rhs->get_line();
      }
      return lhs->get_col() <= rhs->get_col();
    };

    if (node == nullptr) {
      return false;
    }

    auto has_user_anchor_info = [](SgLocatedNode *n) -> bool {
      auto is_user = [](Sg_File_Info *fi) -> bool {
        return fi != nullptr && fi->get_line() > 0 &&
               !fi->isCompilerGenerated();
      };
      return is_user(n->get_file_info()) ||
             is_user(n->get_startOfConstruct()) ||
             is_user(n->get_endOfConstruct());
    };

    const bool is_structural_scope =
        isSgBasicBlock(node) != nullptr ||
        isSgClassDefinition(node) != nullptr ||
        isSgTemplateClassDefinition(node) != nullptr ||
        isSgNamespaceDefinitionStatement(node) != nullptr ||
        isSgClassDeclaration(node) != nullptr ||
        isSgNamespaceDeclarationStatement(node) != nullptr ||
        isSgIfStmt(node) != nullptr || isSgSwitchStatement(node) != nullptr ||
        isSgCaseOptionStmt(node) != nullptr ||
        isSgDefaultOptionStmt(node) != nullptr ||
        isSgForStatement(node) != nullptr ||
        isSgRangeBasedForStatement(node) != nullptr ||
        isSgWhileStmt(node) != nullptr || isSgDoWhileStmt(node) != nullptr ||
        isSgCatchOptionStmt(node) != nullptr;

    const bool has_user_info = has_user_anchor_info(node);
    const bool primary_compiler_generated =
        node->get_file_info() != nullptr &&
        node->get_file_info()->isCompilerGenerated();
    if ((primary_compiler_generated || !has_user_info) &&
        !is_structural_scope) {
      return false;
    }

    if (directive_info == nullptr) {
      return has_user_info ? should_attach_preproc(node) : is_structural_scope;
    }

    Sg_File_Info *info = get_effective_file_info(node);
    if (info == nullptr || info->get_line() <= 0) {
      return false;
    }
    if (!same_file(info, directive_info)) {
      return false;
    }

    if (has_user_info && should_attach_preproc(node)) {
      if (isSgStatement(node) != nullptr) {
        return true;
      }

      Sg_File_Info *start = node->get_startOfConstruct();
      if (start == nullptr || start->get_line() <= 0) {
        start = info;
      }
      Sg_File_Info *end = node->get_endOfConstruct();
      if (start == nullptr || end == nullptr || start->get_line() <= 0) {
        return false;
      }
      if (!same_file(end, directive_info)) {
        end = start;
      }
      if (!location_leq_local(start, end)) {
        std::swap(start, end);
      }
      return location_leq_local(start, directive_info) &&
             location_leq_local(directive_info, end);
    }

    return is_structural_scope;
  };
  auto is_include_directive = [](const PreprocessingInfo *info) -> bool {
    if (info == nullptr) {
      return false;
    }
    PreprocessingInfo::DirectiveType type = info->getTypeOfDirective();
    return type == PreprocessingInfo::CpreprocessorIncludeDeclaration ||
           type == PreprocessingInfo::CpreprocessorIncludeNextDeclaration;
  };
  auto is_comment_directive = [](const PreprocessingInfo *info) -> bool {
    if (info == nullptr) {
      return false;
    }
    PreprocessingInfo::DirectiveType type = info->getTypeOfDirective();
    return type == PreprocessingInfo::C_StyleComment ||
           type == PreprocessingInfo::CplusplusStyleComment ||
           type == PreprocessingInfo::FortranStyleComment ||
           type == PreprocessingInfo::F90StyleComment;
  };
  auto promote_include_target =
      [&](SgLocatedNode *node,
          PreprocessingInfo *directive) -> SgLocatedNode * {
    if (node == nullptr || !is_include_directive(directive)) {
      return node;
    }
    Sg_File_Info *directive_info = directive->get_file_info();
    if (directive_info == nullptr) {
      return node;
    }

    SgLocatedNode *best = node;
    for (SgNode *cursor = node; cursor != nullptr;
         cursor = cursor->get_parent()) {
      SgLocatedNode *located = isSgLocatedNode(cursor);
      if (located == nullptr || isSgStatement(located) == nullptr) {
        continue;
      }
      Sg_File_Info *located_info = get_effective_file_info(located);
      if (located_info == nullptr || located_info->get_line() == 0) {
        continue;
      }
      if (!directive_info->isSameFile(located_info)) {
        continue;
      }
      if (!should_attach_preproc(located)) {
        continue;
      }
      best = located;
      break;
    }

    return best;
  };
  auto is_same_file = [](Sg_File_Info *a, Sg_File_Info *b) -> bool {
    if (a == nullptr || b == nullptr) {
      return false;
    }
    if (a->get_file_id() == b->get_file_id()) {
      return true;
    }
    if (!a->get_filenameString().empty() && !b->get_filenameString().empty()) {
      return a->get_filenameString() == b->get_filenameString();
    }
    return false;
  };
  auto should_attach_include_target =
      [&](SgLocatedNode *node, Sg_File_Info *directive_info) -> bool {
    if (node == nullptr) {
      return false;
    }

    // Includes should anchor to lexical source nodes, not synthesized
    // declarations. Otherwise directives can disappear when they are attached
    // to nodes suppressed in unparsing.
    auto has_user_anchor_info = [](SgLocatedNode *n) -> bool {
      auto is_user = [](Sg_File_Info *fi) -> bool {
        return fi != nullptr && fi->get_line() > 0 &&
               !fi->isCompilerGenerated();
      };
      return is_user(n->get_file_info()) ||
             is_user(n->get_startOfConstruct()) ||
             is_user(n->get_endOfConstruct());
    };

    bool is_structural_scope =
        isSgBasicBlock(node) != nullptr ||
        isSgClassDefinition(node) != nullptr ||
        isSgTemplateClassDefinition(node) != nullptr ||
        isSgNamespaceDefinitionStatement(node) != nullptr ||
        isSgClassDeclaration(node) != nullptr ||
        isSgNamespaceDeclarationStatement(node) != nullptr ||
        isSgIfStmt(node) != nullptr || isSgSwitchStatement(node) != nullptr ||
        isSgCaseOptionStmt(node) != nullptr ||
        isSgDefaultOptionStmt(node) != nullptr;

    const bool has_user_info = has_user_anchor_info(node);
    const bool primary_compiler_generated =
        node->get_file_info() != nullptr &&
        node->get_file_info()->isCompilerGenerated();
    if ((primary_compiler_generated || !has_user_info) &&
        !is_structural_scope) {
      return false;
    }

    Sg_File_Info *info = get_effective_file_info(node);
    if (directive_info != nullptr) {
      if (info == nullptr || info->get_line() <= 0) {
        return false;
      }
      if (!is_same_file(info, directive_info)) {
        return false;
      }
    }
    if (has_user_info && should_attach_preproc(node)) {
      return true;
    }

    // Some structural scopes that correspond to real source braces are marked
    // compiler-generated in ROSE. We still need them as valid anchors for
    // #include placement inside empty blocks/namespaces/classes.
    if (!is_structural_scope) {
      return false;
    }

    info = get_effective_file_info(node);
    if (info == nullptr || info->get_line() <= 0) {
      return false;
    }
    if (directive_info != nullptr && !is_same_file(info, directive_info)) {
      return false;
    }
    return true;
  };
  auto location_leq = [](Sg_File_Info *lhs, Sg_File_Info *rhs) -> bool {
    if (lhs == nullptr || rhs == nullptr) {
      return false;
    }
    if (lhs->get_line() != rhs->get_line()) {
      return lhs->get_line() < rhs->get_line();
    }
    return lhs->get_col() <= rhs->get_col();
  };
  auto directive_inside_node = [&](SgLocatedNode *node,
                                   Sg_File_Info *directive_info) -> bool {
    if (node == nullptr || directive_info == nullptr) {
      return false;
    }
    Sg_File_Info *start = node->get_startOfConstruct();
    if (start == nullptr || start->get_line() <= 0) {
      start = node->get_file_info();
    }
    if (start == nullptr || start->get_line() <= 0) {
      start = get_effective_file_info(node);
    }
    if (start == nullptr) {
      return false;
    }
    Sg_File_Info *end = node->get_endOfConstruct();
    if (end == nullptr || end->get_line() <= 0) {
      end = start;
    }

    if (!is_same_file(start, directive_info)) {
      return false;
    }
    if (!is_same_file(end, directive_info)) {
      end = start;
    }
    if (!location_leq(start, end)) {
      std::swap(start, end);
    }

    return location_leq(start, directive_info) &&
           location_leq(directive_info, end);
  };
  auto directive_inside_class_members_span =
      [&](SgLocatedNode *class_anchor,
          const SgDeclarationStatementPtrList &members,
          Sg_File_Info *directive_info) -> bool {
    if (class_anchor == nullptr || directive_info == nullptr) {
      return false;
    }

    Sg_File_Info *class_start = class_anchor->get_startOfConstruct();
    if (class_start == nullptr || class_start->get_line() <= 0) {
      class_start = get_effective_file_info(class_anchor);
    }
    if (class_start == nullptr || class_start->get_line() <= 0) {
      return false;
    }
    if (!is_same_file(class_start, directive_info)) {
      return false;
    }

    int max_member_end_line = -1;
    int max_member_end_col = -1;
    for (SgDeclarationStatement *member : members) {
      SgLocatedNode *member_node = isSgLocatedNode(member);
      if (member_node == nullptr || !should_attach_preproc(member_node)) {
        continue;
      }
      Sg_File_Info *member_end = member_node->get_endOfConstruct();
      if (member_end == nullptr || member_end->get_line() <= 0) {
        member_end = get_effective_file_info(member_node);
      }
      if (member_end == nullptr || member_end->get_line() <= 0) {
        continue;
      }
      if (!is_same_file(member_end, directive_info)) {
        continue;
      }
      if (member_end->get_line() > max_member_end_line ||
          (member_end->get_line() == max_member_end_line &&
           member_end->get_col() > max_member_end_col)) {
        max_member_end_line = member_end->get_line();
        max_member_end_col = member_end->get_col();
      }
    }

    if (max_member_end_line < 0) {
      return directive_inside_node(class_anchor, directive_info);
    }

    // Allow a small slack beyond the last member to cover the class closing
    // brace/semicolon, but avoid pulling directives that appear after the
    // class (e.g. trailing file-level conditionals) back into the class.
    constexpr int kEndSlackLines = 4;
    if (directive_info->get_line() > max_member_end_line + kEndSlackLines) {
      return false;
    }

    return location_leq(class_start, directive_info);
  };
  std::function<SgLocatedNode *(SgLocatedNode *, Sg_File_Info *)>
      include_target_for_inside;
  include_target_for_inside =
      [&](SgLocatedNode *node,
          Sg_File_Info *directive_info) -> SgLocatedNode * {
    if (node == nullptr || directive_info == nullptr) {
      return node;
    }

    auto choose_inside = [&](SgLocatedNode *candidate) -> SgLocatedNode * {
      if (candidate == nullptr) {
        return nullptr;
      }
      if (!should_attach_include_target(candidate, directive_info)) {
        return nullptr;
      }
      if (!directive_inside_node(candidate, directive_info)) {
        return nullptr;
      }
      return candidate;
    };

    auto descend_statements =
        [&](const SgStatementPtrList &stmts) -> SgLocatedNode * {
      for (SgStatement *stmt : stmts) {
        SgLocatedNode *candidate = isSgLocatedNode(stmt);
        if (candidate == nullptr) {
          continue;
        }
        if (!directive_inside_node(candidate, directive_info)) {
          continue;
        }
        if (SgLocatedNode *target =
                include_target_for_inside(candidate, directive_info)) {
          return target;
        }
        return candidate;
      }
      return nullptr;
    };

    auto descend_declarations =
        [&](const SgDeclarationStatementPtrList &decls) -> SgLocatedNode * {
      for (SgDeclarationStatement *decl : decls) {
        SgLocatedNode *candidate = isSgLocatedNode(decl);
        if (candidate == nullptr) {
          continue;
        }
        if (!directive_inside_node(candidate, directive_info)) {
          continue;
        }
        if (SgLocatedNode *target =
                include_target_for_inside(candidate, directive_info)) {
          return target;
        }
        return candidate;
      }
      return nullptr;
    };

    if (SgBasicBlock *block = isSgBasicBlock(node)) {
      if (SgLocatedNode *target = descend_statements(block->get_statements())) {
        return target;
      }
    }
    if (SgNamespaceDefinitionStatement *ns_def =
            isSgNamespaceDefinitionStatement(node)) {
      if (SgLocatedNode *target =
              descend_declarations(ns_def->get_declarations())) {
        return target;
      }
    }
    if (SgClassDefinition *class_def = isSgClassDefinition(node)) {
      if (SgLocatedNode *target =
              descend_declarations(class_def->get_members())) {
        return target;
      }
    }
    if (SgTemplateClassDefinition *class_def =
            isSgTemplateClassDefinition(node)) {
      if (SgLocatedNode *target =
              descend_declarations(class_def->get_members())) {
        return target;
      }
    }

    if (SgFunctionDeclaration *func_decl = isSgFunctionDeclaration(node)) {
      if (SgFunctionDefinition *defn = func_decl->get_definition()) {
        if (SgBasicBlock *body = defn->get_body()) {
          if (SgLocatedNode *target =
                  include_target_for_inside(body, directive_info)) {
            return target;
          }
          if (SgLocatedNode *target = choose_inside(body)) {
            return target;
          }
        }
        if (SgLocatedNode *target =
                include_target_for_inside(defn, directive_info)) {
          return target;
        }
        if (SgLocatedNode *target = choose_inside(defn)) {
          return target;
        }
      }
    }
    if (SgIfStmt *if_stmt = isSgIfStmt(node)) {
      if (SgLocatedNode *target = include_target_for_inside(
              isSgLocatedNode(if_stmt->get_true_body()), directive_info)) {
        return target;
      }
      if (SgLocatedNode *target =
              choose_inside(isSgLocatedNode(if_stmt->get_true_body()))) {
        return target;
      }
      if (SgLocatedNode *target = include_target_for_inside(
              isSgLocatedNode(if_stmt->get_false_body()), directive_info)) {
        return target;
      }
      if (SgLocatedNode *target =
              choose_inside(isSgLocatedNode(if_stmt->get_false_body()))) {
        return target;
      }
    }
    if (SgForStatement *for_stmt = isSgForStatement(node)) {
      if (SgLocatedNode *target = include_target_for_inside(
              isSgLocatedNode(for_stmt->get_loop_body()), directive_info)) {
        return target;
      }
      if (SgLocatedNode *target =
              choose_inside(isSgLocatedNode(for_stmt->get_loop_body()))) {
        return target;
      }
    }
    if (SgRangeBasedForStatement *for_stmt = isSgRangeBasedForStatement(node)) {
      if (SgLocatedNode *target = include_target_for_inside(
              isSgLocatedNode(for_stmt->get_loop_body()), directive_info)) {
        return target;
      }
      if (SgLocatedNode *target =
              choose_inside(isSgLocatedNode(for_stmt->get_loop_body()))) {
        return target;
      }
    }
    if (SgWhileStmt *while_stmt = isSgWhileStmt(node)) {
      if (SgLocatedNode *target = include_target_for_inside(
              isSgLocatedNode(while_stmt->get_body()), directive_info)) {
        return target;
      }
      if (SgLocatedNode *target =
              choose_inside(isSgLocatedNode(while_stmt->get_body()))) {
        return target;
      }
    }
    if (SgDoWhileStmt *do_stmt = isSgDoWhileStmt(node)) {
      if (SgLocatedNode *target = include_target_for_inside(
              isSgLocatedNode(do_stmt->get_body()), directive_info)) {
        return target;
      }
      if (SgLocatedNode *target =
              choose_inside(isSgLocatedNode(do_stmt->get_body()))) {
        return target;
      }
    }
    if (SgSwitchStatement *switch_stmt = isSgSwitchStatement(node)) {
      if (SgLocatedNode *target = include_target_for_inside(
              isSgLocatedNode(switch_stmt->get_body()), directive_info)) {
        return target;
      }
      if (SgLocatedNode *target =
              choose_inside(isSgLocatedNode(switch_stmt->get_body()))) {
        return target;
      }
    }
    if (SgCaseOptionStmt *case_stmt = isSgCaseOptionStmt(node)) {
      if (SgLocatedNode *target = include_target_for_inside(
              isSgLocatedNode(case_stmt->get_body()), directive_info)) {
        return target;
      }
      if (SgLocatedNode *target =
              choose_inside(isSgLocatedNode(case_stmt->get_body()))) {
        return target;
      }
    }
    if (SgDefaultOptionStmt *default_stmt = isSgDefaultOptionStmt(node)) {
      if (SgLocatedNode *target = include_target_for_inside(
              isSgLocatedNode(default_stmt->get_body()), directive_info)) {
        return target;
      }
      if (SgLocatedNode *target =
              choose_inside(isSgLocatedNode(default_stmt->get_body()))) {
        return target;
      }
    }
    if (SgCatchOptionStmt *catch_stmt = isSgCatchOptionStmt(node)) {
      if (SgLocatedNode *target = include_target_for_inside(
              isSgLocatedNode(catch_stmt->get_body()), directive_info)) {
        return target;
      }
      if (SgLocatedNode *target =
              choose_inside(isSgLocatedNode(catch_stmt->get_body()))) {
        return target;
      }
    }
    if (SgNamespaceDeclarationStatement *ns_decl =
            isSgNamespaceDeclarationStatement(node)) {
      if (SgNamespaceDefinitionStatement *ns_def = ns_decl->get_definition()) {
        if (SgLocatedNode *target =
                include_target_for_inside(ns_def, directive_info)) {
          return target;
        }
        if (SgLocatedNode *target = choose_inside(ns_def)) {
          return target;
        }
      }
    }
    if (SgClassDeclaration *class_decl = isSgClassDeclaration(node)) {
      if (SgClassDefinition *class_def = class_decl->get_definition()) {
        if (SgLocatedNode *target =
                include_target_for_inside(class_def, directive_info)) {
          return target;
        }
        if (SgLocatedNode *target = choose_inside(class_def)) {
          return target;
        }
      }
    }
    if (SgTemplateClassDeclaration *class_decl =
            isSgTemplateClassDeclaration(node)) {
      if (SgTemplateClassDefinition *class_def =
              isSgTemplateClassDefinition(class_decl->get_definition())) {
        if (SgLocatedNode *target =
                include_target_for_inside(class_def, directive_info)) {
          return target;
        }
        if (SgLocatedNode *target = choose_inside(class_def)) {
          return target;
        }
      }
    }

    return choose_inside(node);
  };
  auto find_prior_include_anchor =
      [&](SgLocatedNode *node,
          Sg_File_Info *directive_info) -> SgLocatedNode * {
    if (node == nullptr || directive_info == nullptr) {
      return nullptr;
    }

    auto choose_best = [&](SgLocatedNode *candidate,
                           SgLocatedNode *best) -> SgLocatedNode * {
      if (candidate == nullptr) {
        return best;
      }
      if (!should_attach_include_target(candidate, directive_info)) {
        return best;
      }
      if (!directive_inside_node(candidate, directive_info)) {
        return best;
      }
      return candidate;
    };

    SgLocatedNode *best = nullptr;
    SgNode *parent = node->get_parent();
    if (SgBasicBlock *block = isSgBasicBlock(parent)) {
      for (SgStatement *stmt : block->get_statements()) {
        if (stmt == node) {
          break;
        }
        best = choose_best(isSgLocatedNode(stmt), best);
      }
    } else if (SgNamespaceDefinitionStatement *ns_def =
                   isSgNamespaceDefinitionStatement(parent)) {
      for (SgDeclarationStatement *decl : ns_def->get_declarations()) {
        if (decl == node) {
          break;
        }
        best = choose_best(isSgLocatedNode(decl), best);
      }
    } else if (SgGlobal *global = isSgGlobal(parent)) {
      for (SgDeclarationStatement *decl : global->get_declarations()) {
        if (decl == node) {
          break;
        }
        best = choose_best(isSgLocatedNode(decl), best);
      }
    } else if (SgClassDefinition *class_def = isSgClassDefinition(parent)) {
      for (SgDeclarationStatement *decl : class_def->get_members()) {
        if (decl == node) {
          break;
        }
        best = choose_best(isSgLocatedNode(decl), best);
      }
    } else if (SgTemplateClassDefinition *class_def =
                   isSgTemplateClassDefinition(parent)) {
      for (SgDeclarationStatement *decl : class_def->get_members()) {
        if (decl == node) {
          break;
        }
        best = choose_best(isSgLocatedNode(decl), best);
      }
    }

    if (best == nullptr) {
      best = choose_best(isSgLocatedNode(parent), best);
    }
    return best;
  };

  auto find_global_include_anchor =
      [&](Sg_File_Info *directive_info) -> SgLocatedNode * {
    SgGlobal *global_scope = inheritedValue->translator.getGlobalScope();
    if (directive_info == nullptr || global_scope == nullptr) {
      return nullptr;
    }
    for (SgDeclarationStatement *decl : global_scope->get_declarations()) {
      SgLocatedNode *candidate = isSgLocatedNode(decl);
      if (candidate == nullptr) {
        continue;
      }
      if (!should_attach_include_target(candidate, directive_info)) {
        continue;
      }
      Sg_File_Info *candidate_info = get_effective_file_info(candidate);
      if (candidate_info == nullptr || candidate_info->get_line() <= 0) {
        continue;
      }
      if (!is_same_file(candidate_info, directive_info)) {
        continue;
      }
      if (candidate->get_file_info() != nullptr &&
          candidate->get_file_info()->isCompilerGenerated()) {
        continue;
      }
      return candidate;
    }
    return nullptr;
  };
  auto is_conditional_directive = [](PreprocessingInfo::DirectiveType type) {
    return type == PreprocessingInfo::CpreprocessorIfDeclaration ||
           type == PreprocessingInfo::CpreprocessorIfdefDeclaration ||
           type == PreprocessingInfo::CpreprocessorIfndefDeclaration ||
           type == PreprocessingInfo::CpreprocessorElifDeclaration ||
           type == PreprocessingInfo::CpreprocessorElseDeclaration ||
           type == PreprocessingInfo::CpreprocessorEndifDeclaration;
  };
  auto is_gap_preserved_directive = [&](PreprocessingInfo::DirectiveType type) {
    // Preserve only trailing conditional structure across statement gaps.
    // Opening directives must stay anchored on the following declaration or
    // scope so later rewrites cannot strand an `#endif` without its `#if`.
    return type == PreprocessingInfo::CpreprocessorDeadIfDeclaration ||
           type == PreprocessingInfo::CpreprocessorElifDeclaration ||
           type == PreprocessingInfo::CpreprocessorElseDeclaration ||
           type == PreprocessingInfo::CpreprocessorEndifDeclaration;
  };
  auto is_opening_conditional_directive =
      [](PreprocessingInfo::DirectiveType type) {
        return type == PreprocessingInfo::CpreprocessorIfDeclaration ||
               type == PreprocessingInfo::CpreprocessorIfdefDeclaration ||
               type == PreprocessingInfo::CpreprocessorIfndefDeclaration;
      };
  auto is_structural_conditional_scope = [](SgLocatedNode *node) {
    return node != nullptr &&
           (isSgBasicBlock(node) != nullptr || isSgIfStmt(node) != nullptr ||
            isSgSwitchStatement(node) != nullptr ||
            isSgCaseOptionStmt(node) != nullptr ||
            isSgDefaultOptionStmt(node) != nullptr ||
            isSgForStatement(node) != nullptr ||
            isSgRangeBasedForStatement(node) != nullptr ||
            isSgWhileStmt(node) != nullptr ||
            isSgDoWhileStmt(node) != nullptr ||
            isSgCatchOptionStmt(node) != nullptr);
  };
  auto find_structural_conditional_anchor =
      [&](SgLocatedNode *candidate, SgLocatedNode *current_node,
          Sg_File_Info *directive_info,
          PreprocessingInfo::DirectiveType directive_type,
          PreprocessingInfo::RelativePositionType &relative_position)
      -> SgLocatedNode * {
    if (candidate == nullptr || current_node == nullptr ||
        directive_info == nullptr || current_pos == nullptr ||
        !is_conditional_directive(directive_type)) {
      return nullptr;
    }

    const bool is_opening = is_opening_conditional_directive(directive_type);
    for (SgNode *cursor_node = candidate; cursor_node != nullptr;
         cursor_node = cursor_node->get_parent()) {
      SgLocatedNode *anchor = isSgLocatedNode(cursor_node);
      if (anchor == nullptr || !is_structural_conditional_scope(anchor) ||
          !should_attach_preproc_target(anchor, directive_info)) {
        continue;
      }
      if (anchor == current_node ||
          SageInterface::isAncestor(anchor, current_node)) {
        continue;
      }

      Sg_File_Info *start = anchor->get_startOfConstruct();
      if (start == nullptr || start->get_line() <= 0) {
        start = get_effective_file_info(anchor);
      }
      Sg_File_Info *end = anchor->get_endOfConstruct();
      if (end == nullptr || end->get_line() <= 0) {
        end = start;
      }
      if (start == nullptr || end == nullptr || start->get_line() <= 0) {
        continue;
      }
      if (!is_same_file(start, directive_info)) {
        continue;
      }
      if (!is_same_file(end, directive_info)) {
        end = start;
      }
      if (!location_leq(start, end)) {
        std::swap(start, end);
      }

      if (is_opening) {
        if (location_leq(start, directive_info) &&
            location_leq(directive_info, end)) {
          relative_position = PreprocessingInfo::inside;
          return anchor;
        }
      } else {
        if (location_leq(end, directive_info) &&
            location_leq(directive_info, current_pos) &&
            !location_leq(current_pos, directive_info)) {
          relative_position = PreprocessingInfo::after;
          return anchor;
        }
      }
    }

    return nullptr;
  };
  auto find_gap_anchor_after_candidate =
      [&](SgLocatedNode *candidate, SgLocatedNode *current_node,
          Sg_File_Info *directive_info) -> SgLocatedNode * {
    if (candidate == nullptr || current_node == nullptr ||
        directive_info == nullptr || current_pos == nullptr) {
      return nullptr;
    }

    auto get_effective_end = [&](SgLocatedNode *node) -> Sg_File_Info * {
      if (node == nullptr) {
        return nullptr;
      }
      if (Sg_File_Info *end = node->get_endOfConstruct()) {
        if (end->get_line() > 0) {
          return end;
        }
      }
      return get_effective_file_info(node);
    };

    SgLocatedNode *fallback_anchor = nullptr;
    for (SgNode *cursor_node = candidate; cursor_node != nullptr;
         cursor_node = cursor_node->get_parent()) {
      SgLocatedNode *anchor = isSgLocatedNode(cursor_node);
      if (anchor == nullptr || anchor == current_node ||
          SageInterface::isAncestor(anchor, current_node) ||
          !should_attach_preproc_target(anchor, directive_info)) {
        continue;
      }

      Sg_File_Info *anchor_end = get_effective_end(anchor);
      if (anchor_end == nullptr || anchor_end->get_line() <= 0 ||
          !is_same_file(anchor_end, directive_info) ||
          !is_same_file(current_pos, directive_info)) {
        continue;
      }

      const bool after_anchor = location_leq(anchor_end, directive_info) &&
                                !location_leq(directive_info, anchor_end);
      const bool before_current = location_leq(directive_info, current_pos) &&
                                  !location_leq(current_pos, directive_info);
      if (after_anchor && before_current) {
        if (fallback_anchor == nullptr) {
          fallback_anchor = anchor;
        }
        if (SgStatement *anchor_stmt = isSgStatement(anchor)) {
          auto next_sibling_statement = [](SgStatement *stmt) -> SgStatement * {
            if (stmt == nullptr) {
              return nullptr;
            }
            if (SgBasicBlock *bb = isSgBasicBlock(stmt->get_parent())) {
              const SgStatementPtrList &stmts = bb->get_statements();
              for (size_t i = 0; i < stmts.size(); ++i) {
                if (stmts[i] == stmt) {
                  return (i + 1 < stmts.size()) ? stmts[i + 1] : nullptr;
                }
              }
              return nullptr;
            }
            if (SgGlobal *global = isSgGlobal(stmt->get_parent())) {
              const SgDeclarationStatementPtrList &decls =
                  global->get_declarations();
              for (size_t i = 0; i < decls.size(); ++i) {
                if (decls[i] == stmt) {
                  return (i + 1 < decls.size()) ? decls[i + 1] : nullptr;
                }
              }
              return nullptr;
            }
            if (SgNamespaceDefinitionStatement *ns_def =
                    isSgNamespaceDefinitionStatement(stmt->get_parent())) {
              const SgDeclarationStatementPtrList &decls =
                  ns_def->get_declarations();
              for (size_t i = 0; i < decls.size(); ++i) {
                if (decls[i] == stmt) {
                  return (i + 1 < decls.size()) ? decls[i + 1] : nullptr;
                }
              }
              return nullptr;
            }
            if (SgClassDefinition *class_def =
                    isSgClassDefinition(stmt->get_parent())) {
              const SgDeclarationStatementPtrList &members =
                  class_def->get_members();
              for (size_t i = 0; i < members.size(); ++i) {
                if (members[i] == stmt) {
                  return (i + 1 < members.size()) ? members[i + 1] : nullptr;
                }
              }
              return nullptr;
            }
            if (SgTemplateClassDefinition *class_def =
                    isSgTemplateClassDefinition(stmt->get_parent())) {
              const SgDeclarationStatementPtrList &members =
                  class_def->get_members();
              for (size_t i = 0; i < members.size(); ++i) {
                if (members[i] == stmt) {
                  return (i + 1 < members.size()) ? members[i + 1] : nullptr;
                }
              }
              return nullptr;
            }
            return nullptr;
          };

          if (SgStatement *next_stmt = next_sibling_statement(anchor_stmt)) {
            if (next_stmt == current_node ||
                SageInterface::isAncestor(next_stmt, current_node)) {
              return anchor;
            }
          }
        }
      }
    }

    return fallback_anchor;
  };

  if (inheritedValue->cursor != nullptr &&
      *current_pos <= *(inheritedValue->cursor)) {
    bool is_include = is_include_directive(inheritedValue->next_to_insert);
    bool record_candidate = false;
    if (is_include) {
      record_candidate =
          should_attach_include_target(loc_node, inheritedValue->cursor);
    } else {
      record_candidate =
          should_attach_preproc_target(loc_node, inheritedValue->cursor);
    }
    if (record_candidate) {
      inheritedValue->candidat = loc_node;
    }
  }

  bool passed_cursor = *current_pos >= *(inheritedValue->cursor);

  while (passed_cursor) {
    if (inheritedValue->next_to_insert != NULL) {
      SgLocatedNode *attach_node = loc_node;
      PreprocessingInfo::DirectiveType directive_type =
          inheritedValue->next_to_insert->getTypeOfDirective();
      attach_node =
          promote_include_target(attach_node, inheritedValue->next_to_insert);
      if (is_include_directive(inheritedValue->next_to_insert) &&
          inheritedValue->cursor != nullptr &&
          inheritedValue->candidat != nullptr &&
          inheritedValue->candidat != loc_node &&
          should_attach_include_target(inheritedValue->candidat,
                                       inheritedValue->cursor)) {
        if (directive_inside_node(inheritedValue->candidat,
                                  inheritedValue->cursor)) {
          attach_node = inheritedValue->candidat;
        }
      }
      if (!is_include_directive(inheritedValue->next_to_insert) &&
          inheritedValue->cursor != nullptr &&
          inheritedValue->candidat != nullptr &&
          inheritedValue->candidat != loc_node) {
        PreprocessingInfo::RelativePositionType relative_position =
            inheritedValue->next_to_insert->getRelativePosition();
        if (SgLocatedNode *boundary_anchor = find_structural_conditional_anchor(
                inheritedValue->candidat, loc_node, inheritedValue->cursor,
                directive_type, relative_position)) {
          attach_node = boundary_anchor;
          inheritedValue->next_to_insert->setRelativePosition(
              relative_position);
        }
      }
      if (!is_include_directive(inheritedValue->next_to_insert) &&
          inheritedValue->cursor != nullptr &&
          inheritedValue->candidat != nullptr &&
          inheritedValue->candidat != loc_node &&
          is_gap_preserved_directive(directive_type)) {
        if (SgLocatedNode *gap_anchor = find_gap_anchor_after_candidate(
                inheritedValue->candidat, loc_node, inheritedValue->cursor)) {
          attach_node = gap_anchor;
          inheritedValue->next_to_insert->setRelativePosition(
              PreprocessingInfo::after);
        }
      }
      if (is_include_directive(inheritedValue->next_to_insert) &&
          inheritedValue->cursor != nullptr) {
        if (!directive_inside_node(attach_node, inheritedValue->cursor)) {
          if (SgLocatedNode *prior = find_prior_include_anchor(
                  attach_node, inheritedValue->cursor)) {
            attach_node = prior;
          }
        }
        if (SgLocatedNode *inside_target = include_target_for_inside(
                attach_node, inheritedValue->cursor)) {
          attach_node = inside_target;
        }
        if (isSgBasicBlock(attach_node) != nullptr ||
            isSgClassDefinition(attach_node) != nullptr ||
            isSgTemplateClassDefinition(attach_node) != nullptr ||
            isSgNamespaceDefinitionStatement(attach_node) != nullptr) {
          inheritedValue->next_to_insert->setRelativePosition(
              PreprocessingInfo::inside);
        } else if (isSgNamespaceDeclarationStatement(attach_node) != nullptr ||
                   isSgClassDeclaration(attach_node) != nullptr ||
                   isSgTemplateClassDeclaration(attach_node) != nullptr) {
          Sg_File_Info *start = attach_node->get_startOfConstruct();
          Sg_File_Info *end = attach_node->get_endOfConstruct();
          if (start != nullptr && end != nullptr &&
              directive_inside_node(attach_node, inheritedValue->cursor) &&
              location_leq(start, inheritedValue->cursor) &&
              location_leq(inheritedValue->cursor, end) &&
              !location_leq(inheritedValue->cursor, start) &&
              !location_leq(end, inheritedValue->cursor)) {
            inheritedValue->next_to_insert->setRelativePosition(
                PreprocessingInfo::inside);
          }
        }
      }
      const bool is_class_conditional_directive =
          directive_type == PreprocessingInfo::CpreprocessorEndifDeclaration ||
          directive_type == PreprocessingInfo::CpreprocessorElseDeclaration ||
          directive_type == PreprocessingInfo::CpreprocessorElifDeclaration;
      if (is_class_conditional_directive && inheritedValue->cursor != nullptr) {
        auto find_last_member_before_cursor =
            [&](const SgDeclarationStatementPtrList &members)
            -> SgLocatedNode * {
          SgLocatedNode *last = nullptr;
          Sg_File_Info *last_start = nullptr;
          for (SgDeclarationStatement *member : members) {
            SgLocatedNode *candidate = isSgLocatedNode(member);
            if (candidate == nullptr || !should_attach_preproc(candidate)) {
              continue;
            }
            Sg_File_Info *start = candidate->get_startOfConstruct();
            if (start == nullptr || start->get_line() <= 0) {
              start = get_effective_file_info(candidate);
            }
            if (start == nullptr || start->get_line() <= 0) {
              continue;
            }
            if (!is_same_file(start, inheritedValue->cursor)) {
              continue;
            }
            if (!location_leq(start, inheritedValue->cursor)) {
              continue;
            }
            if (last_start == nullptr || location_leq(last_start, start)) {
              last = candidate;
              last_start = start;
            }
          }
          return last;
        };

        auto find_first_member_after_cursor =
            [&](const SgDeclarationStatementPtrList &members)
            -> SgLocatedNode * {
          SgLocatedNode *first = nullptr;
          Sg_File_Info *first_start = nullptr;
          for (SgDeclarationStatement *member : members) {
            SgLocatedNode *candidate = isSgLocatedNode(member);
            if (candidate == nullptr || !should_attach_preproc(candidate)) {
              continue;
            }
            Sg_File_Info *start = candidate->get_startOfConstruct();
            if (start == nullptr || start->get_line() <= 0) {
              start = get_effective_file_info(candidate);
            }
            if (start == nullptr || start->get_line() <= 0) {
              continue;
            }
            if (!is_same_file(start, inheritedValue->cursor)) {
              continue;
            }
            if (!location_leq(inheritedValue->cursor, start) ||
                location_leq(start, inheritedValue->cursor)) {
              continue;
            }
            if (first_start == nullptr || location_leq(start, first_start)) {
              first = candidate;
              first_start = start;
            }
          }
          return first;
        };

        auto resolve_class_anchor =
            [&](SgLocatedNode *node, SgLocatedNode *&anchor_out,
                const SgDeclarationStatementPtrList *&members_out) {
              for (SgNode *cursor = node; cursor != nullptr;
                   cursor = cursor->get_parent()) {
                if (SgClassDeclaration *class_decl =
                        isSgClassDeclaration(cursor)) {
                  if (SgClassDefinition *def = class_decl->get_definition()) {
                    anchor_out = def;
                    members_out = &def->get_members();
                    return;
                  }
                }
                if (SgTemplateClassDeclaration *template_class_decl =
                        isSgTemplateClassDeclaration(cursor)) {
                  if (SgTemplateClassDefinition *def =
                          isSgTemplateClassDefinition(
                              template_class_decl->get_definition())) {
                    anchor_out = def;
                    members_out = &def->get_members();
                    return;
                  }
                }
                if (SgClassDefinition *def = isSgClassDefinition(cursor)) {
                  anchor_out = def;
                  members_out = &def->get_members();
                  return;
                }
                if (SgTemplateClassDefinition *def =
                        isSgTemplateClassDefinition(cursor)) {
                  anchor_out = def;
                  members_out = &def->get_members();
                  return;
                }
              }
            };

        SgLocatedNode *class_anchor = nullptr;
        const SgDeclarationStatementPtrList *members = nullptr;
        resolve_class_anchor(attach_node, class_anchor, members);
        if ((class_anchor == nullptr || members == nullptr) &&
            inheritedValue->candidat != nullptr) {
          resolve_class_anchor(inheritedValue->candidat, class_anchor, members);
        }

        if ((class_anchor == nullptr || members == nullptr) &&
            inheritedValue->cursor != nullptr) {
          SgGlobal *global_scope = inheritedValue->translator.getGlobalScope();
          if (global_scope != nullptr) {
            SgLocatedNode *best_anchor = nullptr;
            const SgDeclarationStatementPtrList *best_members = nullptr;
            int best_span = -1;

            auto consider_class =
                [&](SgLocatedNode *candidate_anchor,
                    const SgDeclarationStatementPtrList &candidate_members) {
                  if (candidate_anchor == nullptr) {
                    return;
                  }
                  if (!directive_inside_node(candidate_anchor,
                                             inheritedValue->cursor)) {
                    return;
                  }
                  Sg_File_Info *start =
                      candidate_anchor->get_startOfConstruct();
                  Sg_File_Info *end = candidate_anchor->get_endOfConstruct();
                  if (start == nullptr || end == nullptr ||
                      start->get_line() <= 0 || end->get_line() <= 0) {
                    return;
                  }
                  int span = end->get_line() - start->get_line();
                  if (best_span < 0 || span < best_span) {
                    best_anchor = candidate_anchor;
                    best_members = &candidate_members;
                    best_span = span;
                  }
                };

            Rose_STL_Container<SgNode *> class_nodes =
                NodeQuery::querySubTree(global_scope, V_SgClassDeclaration);
            for (SgNode *node : class_nodes) {
              SgClassDeclaration *class_decl = isSgClassDeclaration(node);
              if (class_decl == nullptr) {
                continue;
              }
              if (SgClassDefinition *def = class_decl->get_definition()) {
                consider_class(def, def->get_members());
              }
            }

            Rose_STL_Container<SgNode *> template_class_nodes =
                NodeQuery::querySubTree(global_scope,
                                        V_SgTemplateClassDeclaration);
            for (SgNode *node : template_class_nodes) {
              SgTemplateClassDeclaration *class_decl =
                  isSgTemplateClassDeclaration(node);
              if (class_decl == nullptr) {
                continue;
              }
              if (SgTemplateClassDefinition *def = isSgTemplateClassDefinition(
                      class_decl->get_definition())) {
                consider_class(def, def->get_members());
              }
            }

            if (best_anchor != nullptr && best_members != nullptr) {
              class_anchor = best_anchor;
              members = best_members;
            }
          }
        }

        if (class_anchor != nullptr && members != nullptr &&
            directive_inside_class_members_span(class_anchor, *members,
                                                inheritedValue->cursor)) {
          // Class-level conditionals should be anchored between class members,
          // but directives nested inside member declarations must stay inside
          // that declaration body.
          bool cursor_inside_member = false;
          for (SgDeclarationStatement *member : *members) {
            SgLocatedNode *member_node = isSgLocatedNode(member);
            if (member_node == nullptr || !should_attach_preproc(member_node)) {
              continue;
            }
            if (directive_inside_node(member_node, inheritedValue->cursor)) {
              cursor_inside_member = true;
              break;
            }
          }

          if (!cursor_inside_member) {
            SgLocatedNode *member_before =
                find_last_member_before_cursor(*members);
            if (directive_type ==
                PreprocessingInfo::CpreprocessorEndifDeclaration) {
              if (member_before != nullptr) {
                attach_node = member_before;
                inheritedValue->next_to_insert->setRelativePosition(
                    PreprocessingInfo::after);
              }
            } else {
              if (member_before != nullptr) {
                attach_node = member_before;
                inheritedValue->next_to_insert->setRelativePosition(
                    PreprocessingInfo::after);
              } else if (SgLocatedNode *member_after =
                             find_first_member_after_cursor(*members)) {
                attach_node = member_after;
                inheritedValue->next_to_insert->setRelativePosition(
                    PreprocessingInfo::before);
              }
            }
          }
        }
      }
      if (!is_include_directive(inheritedValue->next_to_insert) &&
          is_comment_directive(inheritedValue->next_to_insert) &&
          inheritedValue->cursor != nullptr &&
          inheritedValue->candidat != nullptr &&
          inheritedValue->candidat != loc_node) {
        SgLocatedNode *candidate_node = inheritedValue->candidat;
        Sg_File_Info *candidate_end = candidate_node->get_endOfConstruct();
        if (candidate_end == nullptr || candidate_end->get_line() <= 0) {
          candidate_end = get_effective_file_info(candidate_node);
        }
        if (candidate_end != nullptr && candidate_end->get_line() > 0 &&
            is_same_file(candidate_end, inheritedValue->cursor) &&
            candidate_end->get_line() == inheritedValue->cursor->get_line() &&
            candidate_end->get_col() > 0 &&
            candidate_end->get_col() < inheritedValue->cursor->get_col()) {
          attach_node = candidate_node;
          inheritedValue->next_to_insert->setRelativePosition(
              PreprocessingInfo::after);
        }
      }
      bool is_include = is_include_directive(inheritedValue->next_to_insert);
      if (is_include && inheritedValue->cursor != nullptr &&
          attach_node != nullptr && attach_node->get_file_info() != nullptr &&
          attach_node->get_file_info()->isCompilerGenerated()) {
        if (SgLocatedNode *global_anchor =
                find_global_include_anchor(inheritedValue->cursor)) {
          attach_node = global_anchor;
        }
      }
      bool can_attach = is_include ? should_attach_include_target(
                                         attach_node, inheritedValue->cursor)
                                   : should_attach_preproc_target(
                                         attach_node, inheritedValue->cursor);
      if (!can_attach) {
        return inheritedValue;
      }
      attach_node->addToAttachedPreprocessingInfo(
          inheritedValue->next_to_insert);
    }
    if (!inheritedValue->advance()) {
      return NULL;
    }

    if (inheritedValue->cursor != nullptr &&
        *current_pos <= *(inheritedValue->cursor)) {
      bool next_is_include =
          is_include_directive(inheritedValue->next_to_insert);
      bool record_candidate = false;
      if (next_is_include) {
        record_candidate =
            should_attach_include_target(loc_node, inheritedValue->cursor);
      } else {
        record_candidate =
            should_attach_preproc_target(loc_node, inheritedValue->cursor);
      }
      if (record_candidate) {
        inheritedValue->candidat = loc_node;
      }
    }

    passed_cursor = *current_pos >= *(inheritedValue->cursor);
  }

  return inheritedValue;
}

// class SagePreprocessorRecord

namespace {

bool tokenSpellsIdentifier(const clang::Token &token,
                           llvm::StringRef identifier) {
  if (!token.isAnyIdentifier()) {
    return false;
  }

  if (token.is(clang::tok::raw_identifier)) {
    return token.getRawIdentifier() == identifier;
  }

  const clang::IdentifierInfo *token_identifier = token.getIdentifierInfo();
  return token_identifier != nullptr &&
         token_identifier->getName() == identifier;
}

bool macroDefinitionIsSelfReferential(const clang::MacroInfo *macro_info,
                                      llvm::StringRef macro_name) {
  if (macro_info == nullptr || macro_name.empty()) {
    return false;
  }

  for (const clang::Token &replacement_token : macro_info->tokens()) {
    if (tokenSpellsIdentifier(replacement_token, macro_name)) {
      return true;
    }
  }

  return false;
}

bool isStructuralConditionalDirective(
    PreprocessingInfo::DirectiveType directive_type) {
  switch (directive_type) {
  case PreprocessingInfo::CpreprocessorIfDeclaration:
  case PreprocessingInfo::CpreprocessorIfdefDeclaration:
  case PreprocessingInfo::CpreprocessorIfndefDeclaration:
  case PreprocessingInfo::CpreprocessorElifDeclaration:
  case PreprocessingInfo::CpreprocessorElseDeclaration:
  case PreprocessingInfo::CpreprocessorEndifDeclaration:
    return true;
  default:
    return false;
  }
}

bool translateRecordedOffset(clang::SourceManager *source_manager,
                             clang::FileID file_id,
                             const Sg_File_Info *file_info, unsigned &offset) {
  if (source_manager == nullptr || file_info == nullptr || !file_id.isValid() ||
      file_info->get_line() <= 0 || file_info->get_col() <= 0) {
    return false;
  }

  clang::SourceLocation loc = source_manager->translateLineCol(
      file_id, file_info->get_line(), file_info->get_col());
  if (!loc.isValid()) {
    return false;
  }

  offset = source_manager->getFileOffset(loc);
  return true;
}

unsigned lineDirectiveEndOffset(clang::SourceManager *source_manager,
                                clang::FileID file_id, unsigned begin_offset) {
  if (source_manager == nullptr || !file_id.isValid()) {
    return begin_offset;
  }

  auto buffer = source_manager->getBufferDataOrNone(file_id);
  if (!buffer || begin_offset >= buffer->size()) {
    return begin_offset;
  }

  unsigned end_offset = begin_offset;
  while (end_offset < buffer->size() && (*buffer)[end_offset] != '\n' &&
         (*buffer)[end_offset] != '\r') {
    ++end_offset;
  }

  if (end_offset < buffer->size()) {
    if ((*buffer)[end_offset] == '\r' && end_offset + 1 < buffer->size() &&
        (*buffer)[end_offset + 1] == '\n') {
      end_offset += 2;
    } else {
      ++end_offset;
    }
  }

  return end_offset;
}

} // namespace

SagePreprocessorRecord::SagePreprocessorRecord(
    clang::SourceManager *source_manager, clang::Preprocessor *preprocessor)
    : p_source_manager(source_manager), p_preprocessor(preprocessor),
      p_preprocessor_record_list(), p_preprocessor_record_list_sorted(true),
      p_application_file_paths(), p_self_referential_macros(),
      p_saw_self_referential_macro_expansion(false) {}

void SagePreprocessorRecord::sortRecordedDirectives() {
  if (p_preprocessor_record_list_sorted || p_preprocessor_record_list.empty()) {
    return;
  }
  auto by_location =
      [](const std::pair<Sg_File_Info *, PreprocessingInfo *> &lhs,
         const std::pair<Sg_File_Info *, PreprocessingInfo *> &rhs) {
        const Sg_File_Info *lhs_info = lhs.first;
        const Sg_File_Info *rhs_info = rhs.first;
        if (lhs_info == nullptr || rhs_info == nullptr) {
          return lhs_info < rhs_info;
        }
        if (lhs_info->get_line() != rhs_info->get_line()) {
          return lhs_info->get_line() < rhs_info->get_line();
        }
        return lhs_info->get_col() < rhs_info->get_col();
      };

  std::stable_sort(p_preprocessor_record_list.begin(),
                   p_preprocessor_record_list.end(), by_location);
  p_preprocessor_record_list_sorted = true;
}

bool SagePreprocessorRecord::isApplicationHeaderPath(
    const std::string &path) const {
  if (path.empty()) {
    return false;
  }

  return p_application_file_paths.find(FileHelper::normalizePathIfPossible(
             path)) != p_application_file_paths.end();
}

bool SagePreprocessorRecord::shouldRecordDirective(
    clang::SourceLocation loc) const {
  if (p_source_manager == nullptr || !loc.isValid()) {
    return false;
  }
  clang::SourceLocation resolved = loc;
  if (resolved.isMacroID()) {
    resolved = p_source_manager->getSpellingLoc(resolved);
  }
  if (!resolved.isValid()) {
    return false;
  }
  return p_source_manager->isWrittenInMainFile(resolved);
}

std::string SagePreprocessorRecord::getFilenameForLocation(
    clang::SourceLocation loc) const {
  if (p_source_manager == nullptr || !loc.isValid()) {
    return std::string();
  }
  clang::SourceLocation resolved = loc;
  if (resolved.isMacroID()) {
    resolved = p_source_manager->getSpellingLoc(resolved);
  }
  clang::FileID file_id = p_source_manager->getFileID(resolved);
  std::string file;
  const clang::FileEntry *fileEntry =
      p_source_manager->getFileEntryForID(file_id);
  if (fileEntry) {
    file = fileEntry->tryGetRealPathName().str();
  }
  if (file.empty()) {
    file = p_source_manager->getFilename(resolved).str();
  }
  if ((file.empty() || file == "<built-in>") &&
      p_source_manager->isWrittenInMainFile(resolved)) {
    const clang::FileEntry *main_entry =
        p_source_manager->getFileEntryForID(p_source_manager->getMainFileID());
    if (main_entry) {
      file = main_entry->tryGetRealPathName().str();
    }
  }
  if (file.empty()) {
    clang::PresumedLoc ploc = p_source_manager->getPresumedLoc(resolved);
    if (ploc.isValid()) {
      file = ploc.getFilename();
    }
  }
  return file;
}

std::string
SagePreprocessorRecord::collectDirectiveText(clang::SourceLocation loc) const {
  if (p_source_manager == nullptr || !loc.isValid()) {
    return std::string();
  }
  clang::SourceLocation resolved = loc;
  if (resolved.isMacroID()) {
    resolved = p_source_manager->getSpellingLoc(resolved);
  }
  if (!resolved.isValid()) {
    return std::string();
  }

  const char *current = p_source_manager->getCharacterData(resolved);
  if (current == nullptr) {
    return std::string();
  }

  std::string text;
  while (current != nullptr) {
    const char *line_end = current;
    while (*line_end != '\n' && *line_end != '\r' && *line_end != '\0') {
      ++line_end;
    }

    const char *back = line_end;
    while (back > current &&
           std::isspace(static_cast<unsigned char>(*(back - 1)))) {
      --back;
    }
    bool has_continuation = (back > current && *(back - 1) == '\\');

    if (has_continuation) {
      const char *effective_end = back - 1;
      while (effective_end > current &&
             std::isspace(static_cast<unsigned char>(*(effective_end - 1)))) {
        --effective_end;
      }
      text.append(current, effective_end - current);
      text.push_back(' ');
    } else {
      text.append(current, line_end - current);
    }

    if (*line_end == '\0') {
      break;
    }

    if (*line_end == '\r' && *(line_end + 1) == '\n') {
      current = line_end + 2;
    } else {
      current = line_end + 1;
    }

    if (!has_continuation) {
      break;
    }
  }

  while (!text.empty() &&
         std::isspace(static_cast<unsigned char>(text.back()))) {
    text.pop_back();
  }

  return text;
}

void SagePreprocessorRecord::recordDirective(
    clang::SourceLocation loc, PreprocessingInfo::DirectiveType directive_type,
    const std::string &text) {
  if (!shouldRecordDirective(loc)) {
    return;
  }

  auto directive_requires_hash = [](PreprocessingInfo::DirectiveType type) {
    switch (type) {
    case PreprocessingInfo::CpreprocessorIncludeDeclaration:
    case PreprocessingInfo::CpreprocessorIncludeNextDeclaration:
    case PreprocessingInfo::CpreprocessorIfdefDeclaration:
    case PreprocessingInfo::CpreprocessorIfndefDeclaration:
    case PreprocessingInfo::CpreprocessorIfDeclaration:
    case PreprocessingInfo::CpreprocessorDeadIfDeclaration:
    case PreprocessingInfo::CpreprocessorElseDeclaration:
    case PreprocessingInfo::CpreprocessorElifDeclaration:
    case PreprocessingInfo::CpreprocessorEndifDeclaration:
    case PreprocessingInfo::CpreprocessorLineDeclaration:
    case PreprocessingInfo::CpreprocessorWarningDeclaration:
    case PreprocessingInfo::CpreprocessorErrorDeclaration:
    case PreprocessingInfo::CpreprocessorEmptyDeclaration:
    case PreprocessingInfo::CpreprocessorDefineDeclaration:
    case PreprocessingInfo::CpreprocessorUndefDeclaration:
    case PreprocessingInfo::CpreprocessorIdentDeclaration:
      return true;
    default:
      return false;
    }
  };

  clang::SourceLocation resolved = loc;
  if (resolved.isMacroID()) {
    resolved = p_source_manager->getSpellingLoc(resolved);
  }
  if (!resolved.isValid()) {
    return;
  }

  clang::SourceLocation file_loc = p_source_manager->getFileLoc(resolved);
  clang::FileID file_id = file_loc.isValid()
                              ? p_source_manager->getFileID(file_loc)
                              : clang::FileID();
  if (directive_requires_hash(directive_type) && file_loc.isValid() &&
      !file_id.isInvalid()) {
    if (auto buffer = p_source_manager->getBufferDataOrNone(file_id)) {
      unsigned hash_offset = p_source_manager->getFileOffset(file_loc);
      while (hash_offset > 0 && (*buffer)[hash_offset - 1] != '\n' &&
             (*buffer)[hash_offset - 1] != '\r') {
        --hash_offset;
      }
      while (hash_offset < buffer->size() && ((*buffer)[hash_offset] == ' ' ||
                                              (*buffer)[hash_offset] == '\t')) {
        ++hash_offset;
      }
      if (hash_offset < buffer->size() && (*buffer)[hash_offset] == '#') {
        file_loc =
            p_source_manager->getLocForStartOfFile(file_id).getLocWithOffset(
                hash_offset);
      }
    }
  }
  unsigned directive_offset = (file_loc.isValid() && file_id.isValid())
                                  ? p_source_manager->getFileOffset(file_loc)
                                  : 0;
  bool inside_skipped_range = false;

  // If we already recorded a whole skipped inactive range (CSkippedToken) for a
  // section of the file, suppress any other directive/comment records that land
  // inside that preserved text to avoid duplicates during unparsing.
  if (directive_type != PreprocessingInfo::CSkippedToken &&
      p_source_manager != nullptr && !p_skipped_ranges.empty()) {
    if (file_id.isValid()) {
      for (const SkippedFileRange &range : p_skipped_ranges) {
        if (!range.file_id.isValid() || range.file_id != file_id) {
          continue;
        }
        if (range.begin_offset <= directive_offset &&
            directive_offset < range.end_offset) {
          inside_skipped_range = true;
          break;
        }
      }
      if (inside_skipped_range &&
          !isStructuralConditionalDirective(directive_type)) {
        return;
      }
    }
  }

  clang::SourceLocation info_loc = file_loc.isValid() ? file_loc : resolved;
  bool inv_begin_line = false;
  bool inv_begin_col = false;
  unsigned ls =
      p_source_manager->getSpellingLineNumber(info_loc, &inv_begin_line);
  unsigned cs =
      p_source_manager->getSpellingColumnNumber(info_loc, &inv_begin_col);
  (void)inv_begin_line;
  (void)inv_begin_col;

  std::string file = getFilenameForLocation(resolved);
  std::string content = text;
  if (!content.empty() && content.back() != '\n') {
    content.push_back('\n');
  }

  if (inside_skipped_range && file_id.isValid() &&
      isStructuralConditionalDirective(directive_type)) {
    unsigned directive_end_offset =
        lineDirectiveEndOffset(p_source_manager, file_id, directive_offset);
    for (auto it = p_preprocessor_record_list.begin();
         it != p_preprocessor_record_list.end();) {
      Sg_File_Info *existing_info = it->first;
      PreprocessingInfo *existing_pp = it->second;
      if (existing_info == nullptr || existing_pp == nullptr ||
          existing_pp->getTypeOfDirective() !=
              PreprocessingInfo::CSkippedToken) {
        ++it;
        continue;
      }

      unsigned skipped_begin_offset = 0;
      if (!translateRecordedOffset(p_source_manager, file_id, existing_info,
                                   skipped_begin_offset)) {
        ++it;
        continue;
      }
      unsigned skipped_end_offset =
          skipped_begin_offset + existing_pp->getString().size();
      if (directive_end_offset <= skipped_begin_offset ||
          skipped_end_offset <= directive_offset) {
        ++it;
        continue;
      }

      const std::string skipped_text = existing_pp->getString();
      std::vector<std::pair<unsigned, std::string>> replacements;
      if (skipped_begin_offset < directive_offset) {
        replacements.emplace_back(
            skipped_begin_offset,
            skipped_text.substr(0, directive_offset - skipped_begin_offset));
      }
      if (directive_end_offset < skipped_end_offset) {
        replacements.emplace_back(
            directive_end_offset,
            skipped_text.substr(directive_end_offset - skipped_begin_offset));
      }

      delete existing_info;
      delete existing_pp;
      it = p_preprocessor_record_list.erase(it);

      for (const auto &replacement : replacements) {
        if (replacement.second.empty()) {
          continue;
        }
        clang::SourceLocation replacement_loc =
            p_source_manager->getLocForStartOfFile(file_id).getLocWithOffset(
                replacement.first);
        bool replacement_inv_line = false;
        bool replacement_inv_col = false;
        unsigned replacement_line = p_source_manager->getSpellingLineNumber(
            replacement_loc, &replacement_inv_line);
        unsigned replacement_col = p_source_manager->getSpellingColumnNumber(
            replacement_loc, &replacement_inv_col);
        (void)replacement_inv_line;
        (void)replacement_inv_col;
        Sg_File_Info *replacement_info =
            new Sg_File_Info(file, replacement_line, replacement_col);
        PreprocessingInfo *replacement_pp = new PreprocessingInfo(
            PreprocessingInfo::CSkippedToken, replacement.second, file,
            replacement_line, replacement_col, 0, PreprocessingInfo::before);
        p_preprocessor_record_list.emplace_back(replacement_info,
                                                replacement_pp);
      }
    }
  }

  auto is_comment_directive = [](PreprocessingInfo::DirectiveType type) {
    switch (type) {
    case PreprocessingInfo::C_StyleComment:
    case PreprocessingInfo::CplusplusStyleComment:
    case PreprocessingInfo::FortranStyleComment:
    case PreprocessingInfo::F90StyleComment:
      return true;
    default:
      return false;
    }
  };
  auto same_record_location = [&](const Sg_File_Info *existing_info) {
    if (existing_info == nullptr) {
      return false;
    }
    if (existing_info->get_line() != static_cast<int>(ls)) {
      return false;
    }
    const std::string existing_file = existing_info->get_filenameString();
    if (!existing_file.empty() && !file.empty() && existing_file != file) {
      return false;
    }
    return true;
  };

  if (directive_requires_hash(directive_type)) {
    size_t first_nonspace = content.find_first_not_of(" \t");
    if (first_nonspace != std::string::npos && content[first_nonspace] != '#') {
      content.insert(first_nonspace, "#");
    }
  }

  // Clang can report the same main-file comment more than once in some
  // preprocessing flows. Keep only exact duplicates at the same source point.
  for (const auto &existing : p_preprocessor_record_list) {
    Sg_File_Info *existing_info = existing.first;
    PreprocessingInfo *existing_pp = existing.second;
    if (existing_info == nullptr || existing_pp == nullptr) {
      continue;
    }
    if (existing_pp->getTypeOfDirective() != directive_type) {
      continue;
    }
    if (!same_record_location(existing_info) ||
        existing_info->get_col() != static_cast<int>(cs)) {
      continue;
    }
    if (existing_pp->getString() == content) {
      return;
    }
  }

  // A trailing comment on the same source line as a preprocessor directive
  // should stay embedded in that directive text, not as a separate comment
  // record.
  if (is_comment_directive(directive_type)) {
    for (const auto &existing : p_preprocessor_record_list) {
      Sg_File_Info *existing_info = existing.first;
      PreprocessingInfo *existing_pp = existing.second;
      if (existing_info == nullptr || existing_pp == nullptr) {
        continue;
      }
      if (!same_record_location(existing_info)) {
        continue;
      }
      if (!is_comment_directive(existing_pp->getTypeOfDirective())) {
        return;
      }
    }
  } else {
    for (auto it = p_preprocessor_record_list.begin();
         it != p_preprocessor_record_list.end();) {
      Sg_File_Info *existing_info = it->first;
      PreprocessingInfo *existing_pp = it->second;
      if (existing_info == nullptr || existing_pp == nullptr ||
          !same_record_location(existing_info) ||
          !is_comment_directive(existing_pp->getTypeOfDirective())) {
        ++it;
        continue;
      }
      delete existing_info;
      delete existing_pp;
      it = p_preprocessor_record_list.erase(it);
    }
  }

  Sg_File_Info *file_info = new Sg_File_Info(file, ls, cs);
  PreprocessingInfo *preproc_info = new PreprocessingInfo(
      directive_type, content, file, ls, cs, 0, PreprocessingInfo::before);

  p_preprocessor_record_list.push_back(
      std::pair<Sg_File_Info *, PreprocessingInfo *>(file_info, preproc_info));
  p_preprocessor_record_list_sorted = false;
}

void SagePreprocessorRecord::recordInjectedDirective(
    clang::SourceLocation loc, PreprocessingInfo::DirectiveType directive_type,
    const std::string &text) {
  recordDirective(loc, directive_type, text);
}

void SagePreprocessorRecord::recordSourceDirective(
    clang::SourceLocation loc,
    PreprocessingInfo::DirectiveType directive_type) {
  if (!shouldRecordDirective(loc)) {
    return;
  }
  std::string text = collectDirectiveText(loc);
  if (!text.empty()) {
    recordDirective(loc, directive_type, text);
  }
}

void SagePreprocessorRecord::InclusionDirective(
    clang::SourceLocation HashLoc, const clang::Token &IncludeTok,
    llvm::StringRef FileName, bool IsAngled,
    clang::CharSourceRange FilenameRange, clang::OptionalFileEntryRef File,
    llvm::StringRef SearchPath, llvm::StringRef RelativePath,
    const clang::Module *SuggestedModule, bool ModuleImported,
    clang::SrcMgr::CharacteristicKind FileType) {
  (void)FilenameRange;
  (void)File;
  (void)SearchPath;
  (void)RelativePath;
  (void)SuggestedModule;
  (void)ModuleImported;
  (void)FileType;

  auto normalize_path = [](const std::string &path) {
    return path.empty() ? std::string()
                        : FileHelper::normalizePathIfPossible(path);
  };

  auto included_path = [&]() {
    std::string path;
    if (File) {
      const clang::FileEntry &file_entry = File->getFileEntry();
      path = file_entry.tryGetRealPathName().str();
      if (path.empty()) {
        path = File->getName().str();
      }
    }
    if (path.empty()) {
      if (llvm::sys::path::is_absolute(FileName)) {
        path = FileName.str();
      } else if (!SearchPath.empty()) {
        llvm::SmallString<512> joined(SearchPath);
        llvm::sys::path::append(joined, FileName);
        path = joined.str().str();
      } else {
        path = FileName.str();
      }
    }
    return normalize_path(path);
  }();

  clang::SourceLocation resolved_hash = HashLoc;
  if (resolved_hash.isMacroID()) {
    resolved_hash = p_source_manager->getSpellingLoc(resolved_hash);
  }
  const bool including_main_file =
      resolved_hash.isValid() && p_source_manager != nullptr &&
      p_source_manager->isWrittenInMainFile(resolved_hash);
  const std::string including_path =
      normalize_path(getFilenameForLocation(HashLoc));
  const bool including_application_file =
      including_main_file || isApplicationHeaderPath(including_path);
  if (including_application_file && !IsAngled && !included_path.empty()) {
    p_application_file_paths.insert(included_path);
  }

  if (!shouldRecordDirective(HashLoc)) {
    return;
  }

  PreprocessingInfo::DirectiveType directive_type =
      PreprocessingInfo::CpreprocessorIncludeDeclaration;
  std::string directive = "#include ";
  clang::tok::PPKeywordKind pp_kind = clang::tok::pp_not_keyword;
  if (const clang::IdentifierInfo *ident = IncludeTok.getIdentifierInfo()) {
    pp_kind = ident->getPPKeywordID();
  }
  if (pp_kind == clang::tok::pp_include_next) {
    directive_type = PreprocessingInfo::CpreprocessorIncludeNextDeclaration;
    directive = "#include_next ";
  }

  // Preserve original include spelling (including trailing comments) when
  // available from the main-file source buffer.
  std::string include_text = collectDirectiveText(HashLoc);
  if (include_text.empty()) {
    std::string include_target;
    if (IsAngled) {
      include_target = "<" + FileName.str() + ">";
    } else {
      include_target = "\"" + FileName.str() + "\"";
    }
    include_text = directive + include_target;
  }

  recordDirective(HashLoc, directive_type, include_text);
}

bool SagePreprocessorRecord::FileNotFound(llvm::StringRef FileName) {
  // REX: Tolerate missing headers to keep parsing error-tolerant and closer to
  // legacy frontend behavior for large test suites.
  if (SgProject::get_verbose() > 0) {
    std::cerr << "[WARN] [ROSE] [frontend] Skipping missing header: "
              << FileName.str() << std::endl;
  }
  return true;
}

void SagePreprocessorRecord::EndOfMainFile() {}

void SagePreprocessorRecord::Ident(clang::SourceLocation Loc,
                                   llvm::StringRef Str) {
  (void)Loc;
  (void)Str;
}

void SagePreprocessorRecord::PragmaComment(clang::SourceLocation Loc,
                                           const clang::IdentifierInfo *Kind,
                                           llvm::StringRef Str) {
  (void)Loc;
  (void)Kind;
  (void)Str;
}

void SagePreprocessorRecord::PragmaMessage(
    clang::SourceLocation Loc, llvm::StringRef Namespace,
    clang::PPCallbacks::PragmaMessageKind Kind, llvm::StringRef Str) {
  (void)Loc;
  (void)Namespace;
  (void)Kind;
  (void)Str;
}

void SagePreprocessorRecord::PragmaDiagnosticPush(clang::SourceLocation Loc,
                                                  llvm::StringRef Namespace) {
  (void)Loc;
  (void)Namespace;
}

void SagePreprocessorRecord::PragmaDiagnosticPop(clang::SourceLocation Loc,
                                                 llvm::StringRef Namespace) {
  (void)Loc;
  (void)Namespace;
}

void SagePreprocessorRecord::PragmaDiagnostic(clang::SourceLocation Loc,
                                              llvm::StringRef Namespace,
                                              clang::diag::Severity Severity,
                                              llvm::StringRef Str) {
  (void)Loc;
  (void)Namespace;
  (void)Severity;
  (void)Str;
}

void SagePreprocessorRecord::MacroExpands(const clang::Token &MacroNameTok,
                                          const clang::MacroDefinition &MD,
                                          clang::SourceRange Range,
                                          const clang::MacroArgs *Args) {
  (void)MD;
  (void)Range;
  (void)Args;
  if (const clang::IdentifierInfo *ident = MacroNameTok.getIdentifierInfo()) {
    if (p_self_referential_macros.find(ident->getName().str()) !=
        p_self_referential_macros.end()) {
      p_saw_self_referential_macro_expansion = true;
    }
  }
}

bool SagePreprocessorRecord::HandleComment(clang::Preprocessor &PP,
                                           clang::SourceRange Comment) {
  clang::SourceLocation loc = Comment.getBegin();
  if (!shouldRecordDirective(loc)) {
    return false;
  }

  std::string text;
  if (p_source_manager != nullptr && Comment.isValid()) {
    clang::CharSourceRange char_range =
        clang::CharSourceRange::getTokenRange(Comment);
    bool invalid = false;
    llvm::StringRef source_text = clang::Lexer::getSourceText(
        char_range, *p_source_manager, PP.getLangOpts(), &invalid);
    if (!invalid) {
      text = source_text.str();
    }
  }
  if (text.empty()) {
    text = collectDirectiveText(loc);
  }
  if (text.empty()) {
    return false;
  }
  if (text.rfind("//", 0) == 0) {
    size_t eol = text.find_first_of("\r\n");
    if (eol != std::string::npos) {
      text = text.substr(0, eol);
    }
  } else if (text.rfind("/*", 0) == 0) {
    size_t end_comment = text.rfind("*/");
    if (end_comment != std::string::npos) {
      text = text.substr(0, end_comment + 2);
    }
  }

  PreprocessingInfo::DirectiveType type = PreprocessingInfo::C_StyleComment;
  llvm::StringRef text_ref(text);
  if (text_ref.starts_with("//")) {
    type = PreprocessingInfo::CplusplusStyleComment;
  } else if (text_ref.starts_with("/*")) {
    type = PreprocessingInfo::C_StyleComment;
  }

  recordDirective(loc, type, text);
  return false;
}

void SagePreprocessorRecord::MacroDefined(const clang::Token &MacroNameTok,
                                          const clang::MacroDirective *MD) {
  clang::SourceLocation loc = MacroNameTok.getLocation();
  const clang::MacroInfo *macro_info = nullptr;
  std::string macro_name;
  if (const clang::IdentifierInfo *ident = MacroNameTok.getIdentifierInfo()) {
    macro_name = ident->getName().str();
  }
  if (MD != nullptr) {
    macro_info = MD->getMacroInfo();
    if (macro_info != nullptr) {
      if (macro_info->getDefinitionLoc().isValid()) {
        loc = macro_info->getDefinitionLoc();
      }
    }
  }
  const bool should_record = shouldRecordDirective(loc);
  std::string text;
  if (should_record) {
    text = collectDirectiveText(loc);
    if (!text.empty()) {
      size_t first = text.find_first_not_of(" \t\r\n");
      if (first != std::string::npos && text[first] != '#') {
        const std::string keyword = "define";
        if (text.compare(first, keyword.size(), keyword) == 0) {
          text = "#" + text.substr(first);
        } else {
          text = "#define " + text.substr(first);
        }
      }
    }
  }
  if (should_record && text.empty()) {
    std::string name_for_text = macro_name;
    if (name_for_text.empty()) {
      name_for_text = "__macro";
    }
    text = "#define " + name_for_text;
  }

  if (!macro_name.empty()) {
    if (macroDefinitionIsSelfReferential(macro_info, macro_name)) {
      p_self_referential_macros.insert(macro_name);
    } else {
      p_self_referential_macros.erase(macro_name);
    }
  }
  if (should_record) {
    recordDirective(loc, PreprocessingInfo::CpreprocessorDefineDeclaration,
                    text);
  }
}

void SagePreprocessorRecord::MacroUndefined(
    const clang::Token &MacroNameTok, const clang::MacroDefinition &MD,
    const clang::MacroDirective *Undef) {
  (void)MD;
  if (const clang::IdentifierInfo *ident = MacroNameTok.getIdentifierInfo()) {
    p_self_referential_macros.erase(ident->getName().str());
  }
  clang::SourceLocation loc = MacroNameTok.getLocation();
  if (Undef != nullptr && Undef->getLocation().isValid()) {
    loc = Undef->getLocation();
  }
  const bool should_record = shouldRecordDirective(loc);
  std::string text;
  if (should_record) {
    text = collectDirectiveText(loc);
    if (!text.empty()) {
      size_t first = text.find_first_not_of(" \t\r\n");
      if (first != std::string::npos && text[first] != '#') {
        const std::string keyword = "undef";
        if (text.compare(first, keyword.size(), keyword) == 0) {
          text = "#" + text.substr(first);
        } else {
          text = "#undef " + text.substr(first);
        }
      }
    }
  }
  if (should_record && text.empty()) {
    std::string name;
    if (const clang::IdentifierInfo *ident = MacroNameTok.getIdentifierInfo()) {
      name = ident->getName().str();
    }
    if (name.empty()) {
      name = "__macro";
    }
    text = "#undef " + name;
  }
  if (should_record) {
    recordDirective(loc, PreprocessingInfo::CpreprocessorUndefDeclaration,
                    text);
  }
}

void SagePreprocessorRecord::Defined(const clang::Token &MacroNameTok,
                                     const clang::MacroDefinition &MD,
                                     clang::SourceRange Range) {
  (void)MacroNameTok;
  (void)MD;
  (void)Range;
}

void SagePreprocessorRecord::SourceRangeSkipped(
    clang::SourceRange Range, clang::SourceLocation EndifLoc) {
  (void)EndifLoc;
  if (p_source_manager == nullptr || p_preprocessor == nullptr ||
      !Range.isValid()) {
    return;
  }

  const clang::LangOptions &lang_opts = p_preprocessor->getLangOpts();
  clang::SourceLocation begin = Range.getBegin();
  clang::SourceLocation end = Range.getEnd();
  if (begin.isMacroID()) {
    begin = p_source_manager->getSpellingLoc(begin);
  }
  if (end.isMacroID()) {
    end = p_source_manager->getSpellingLoc(end);
  }
  if (!begin.isValid() || !end.isValid() || !shouldRecordDirective(begin)) {
    return;
  }

  clang::SourceLocation end_of_token =
      clang::Lexer::getLocForEndOfToken(end, 0, *p_source_manager, lang_opts);
  if (end_of_token.isValid()) {
    end = end_of_token;
  }

  clang::CharSourceRange char_range =
      clang::CharSourceRange::getCharRange(begin, end);

  // Preserve the full text of the skipped range so AST-based unparsing can
  // round-trip inactive branches (e.g., `#ifdef` blocks used by tests).
  //
  // Avoid `Lexer::getSourceText()` here because the range can be represented
  // in a variety of forms; use file offsets directly when possible.
  clang::CharSourceRange file_range =
      clang::Lexer::makeFileCharRange(char_range, *p_source_manager, lang_opts);
  clang::SourceLocation file_begin = begin;
  clang::SourceLocation file_end = end;
  if (!file_range.isInvalid()) {
    file_begin = file_range.getBegin();
    file_end = file_range.getEnd();
  } else {
    file_begin = p_source_manager->getFileLoc(begin);
    file_end = p_source_manager->getFileLoc(end);
  }

  if (file_begin.isValid() && file_end.isValid()) {
    clang::FileID file_id = p_source_manager->getFileID(file_begin);
    clang::FileID file_id_end = p_source_manager->getFileID(file_end);
    if (file_id.isValid() && file_id == file_id_end) {
      if (clang::SourceLocation after_token = clang::Lexer::getLocForEndOfToken(
              file_end, 0, *p_source_manager, lang_opts);
          after_token.isValid()) {
        file_end = after_token;
      }

      auto buffer = p_source_manager->getBufferDataOrNone(file_id);
      if (buffer) {
        unsigned begin_offset = p_source_manager->getFileOffset(file_begin);
        unsigned end_offset = p_source_manager->getFileOffset(file_end);
        if (begin_offset < end_offset && end_offset <= buffer->size()) {
          llvm::StringRef slice =
              buffer->substr(begin_offset, end_offset - begin_offset);
          if (!slice.empty()) {
            p_skipped_ranges.push_back({file_id, begin_offset, end_offset});

            // Prune entries owned by the inactive body, but keep structural
            // conditionals as separate attachments so token ordering preserves
            // `#elif`/`#else`/`#endif` instead of collapsing them into one
            // skipped blob.
            const std::string skipped_file = getFilenameForLocation(file_begin);
            std::vector<std::pair<unsigned, unsigned>> preserved_spans;
            for (auto it = p_preprocessor_record_list.begin();
                 it != p_preprocessor_record_list.end();) {
              Sg_File_Info *existing_info = it->first;
              PreprocessingInfo *existing_pp = it->second;
              if (existing_info == nullptr || existing_pp == nullptr) {
                ++it;
                continue;
              }
              if (!skipped_file.empty()) {
                const std::string existing_file =
                    existing_info->get_filenameString();
                if (!existing_file.empty() && existing_file != skipped_file) {
                  ++it;
                  continue;
                }
              }
              clang::SourceLocation existing_loc =
                  p_source_manager->translateLineCol(file_id,
                                                     existing_info->get_line(),
                                                     existing_info->get_col());
              if (!existing_loc.isValid()) {
                ++it;
                continue;
              }
              unsigned existing_offset =
                  p_source_manager->getFileOffset(existing_loc);
              if (begin_offset <= existing_offset &&
                  existing_offset < end_offset) {
                if (isStructuralConditionalDirective(
                        existing_pp->getTypeOfDirective())) {
                  unsigned span_end = lineDirectiveEndOffset(
                      p_source_manager, file_id, existing_offset);
                  preserved_spans.emplace_back(existing_offset,
                                               std::min(span_end, end_offset));
                  ++it;
                  continue;
                }
                delete existing_info;
                delete existing_pp;
                it = p_preprocessor_record_list.erase(it);
                continue;
              }
              ++it;
            }

            std::sort(preserved_spans.begin(), preserved_spans.end());
            std::vector<std::pair<unsigned, unsigned>> merged_spans;
            for (const auto &span : preserved_spans) {
              if (span.first >= span.second) {
                continue;
              }
              if (merged_spans.empty() ||
                  merged_spans.back().second < span.first) {
                merged_spans.push_back(span);
              } else {
                merged_spans.back().second =
                    std::max(merged_spans.back().second, span.second);
              }
            }

            auto record_skipped_gap = [&](unsigned gap_begin,
                                          unsigned gap_end) {
              if (gap_begin >= gap_end) {
                return;
              }
              llvm::StringRef gap_slice =
                  buffer->substr(gap_begin, gap_end - gap_begin);
              if (gap_slice.empty()) {
                return;
              }
              clang::SourceLocation gap_loc =
                  p_source_manager->getLocForStartOfFile(file_id)
                      .getLocWithOffset(gap_begin);
              recordDirective(gap_loc, PreprocessingInfo::CSkippedToken,
                              gap_slice.str());
            };

            unsigned gap_begin = begin_offset;
            for (const auto &span : merged_spans) {
              record_skipped_gap(gap_begin, span.first);
              gap_begin = std::max(gap_begin, span.second);
            }
            record_skipped_gap(gap_begin, end_offset);
            return;
          }
        }
      }
    }
  }

  begin = file_begin;
  end = file_end;
  if (!begin.isValid() || !end.isValid() ||
      !p_source_manager->isBeforeInTranslationUnit(begin, end)) {
    return;
  }

  clang::FileID file_id = p_source_manager->getFileID(begin);
  if (!file_id.isValid()) {
    return;
  }

  clang::SourceLocation cursor = begin;
  unsigned current_line = 0;
  bool line_has_content = false;
  bool expecting_undef = false;
  clang::SourceLocation hash_loc;

  auto is_identifier_named_undef = [&](const clang::Token &token) -> bool {
    if (!token.isAnyIdentifier()) {
      return false;
    }
    if (token.is(clang::tok::identifier)) {
      const clang::IdentifierInfo *ident = token.getIdentifierInfo();
      return ident != nullptr && ident->getName() == "undef";
    }
    bool invalid_spelling = false;
    std::string spelling = clang::Lexer::getSpelling(
        token, *p_source_manager, lang_opts, &invalid_spelling);
    return !invalid_spelling && spelling == "undef";
  };

  while (cursor.isValid() &&
         p_source_manager->isBeforeInTranslationUnit(cursor, end)) {
    clang::Token token;
    if (clang::Lexer::getRawToken(cursor, token, *p_source_manager, lang_opts,
                                  /*IgnoreWhiteSpace=*/true)) {
      break;
    }

    if (token.is(clang::tok::eof) || token.getLength() == 0) {
      break;
    }

    clang::SourceLocation token_loc = token.getLocation();
    if (!token_loc.isValid() ||
        !p_source_manager->isBeforeInTranslationUnit(token_loc, end)) {
      break;
    }

    clang::FileID token_file_id = p_source_manager->getFileID(token_loc);
    if (token_file_id != file_id) {
      cursor = token_loc.getLocWithOffset(token.getLength());
      continue;
    }

    bool invalid_line = false;
    unsigned token_line =
        p_source_manager->getSpellingLineNumber(token_loc, &invalid_line);
    if (invalid_line || token_line == 0) {
      cursor = token_loc.getLocWithOffset(token.getLength());
      continue;
    }

    if (token_line != current_line) {
      current_line = token_line;
      line_has_content = false;
      expecting_undef = false;
      hash_loc = clang::SourceLocation();
    }

    if (token.is(clang::tok::comment)) {
      cursor = token_loc.getLocWithOffset(token.getLength());
      continue;
    }

    if (!line_has_content) {
      line_has_content = true;
      if (token.is(clang::tok::hash)) {
        expecting_undef = true;
        hash_loc = token_loc;
      }
    } else if (expecting_undef) {
      if (is_identifier_named_undef(token)) {
        std::string directive_text = collectDirectiveText(hash_loc);
        if (!directive_text.empty()) {
          recordDirective(hash_loc,
                          PreprocessingInfo::CpreprocessorUndefDeclaration,
                          directive_text);
        }
      }
      expecting_undef = false;
    }

    cursor = token_loc.getLocWithOffset(token.getLength());
  }
}

void SagePreprocessorRecord::If(
    clang::SourceLocation Loc, clang::SourceRange ConditionRange,
    clang::PPCallbacks::ConditionValueKind ConditionValue) {
  (void)ConditionRange;
  (void)ConditionValue;
  if (!shouldRecordDirective(Loc)) {
    return;
  }
  std::string text = collectDirectiveText(Loc);
  if (text.empty()) {
    text = "#if 0";
  }
  recordDirective(Loc, PreprocessingInfo::CpreprocessorIfDeclaration, text);
}

void SagePreprocessorRecord::Elif(
    clang::SourceLocation Loc, clang::SourceRange ConditionRange,
    clang::PPCallbacks::ConditionValueKind ConditionValue,
    clang::SourceLocation IfLoc) {
  (void)ConditionRange;
  (void)ConditionValue;
  (void)IfLoc;
  if (!shouldRecordDirective(Loc)) {
    return;
  }
  std::string text = collectDirectiveText(Loc);
  if (text.empty()) {
    text = "#elif 0";
  }
  recordDirective(Loc, PreprocessingInfo::CpreprocessorElifDeclaration, text);
}

void SagePreprocessorRecord::Ifdef(clang::SourceLocation Loc,
                                   const clang::Token &MacroNameTok,
                                   const clang::MacroDefinition &MD) {
  (void)MD;
  if (!shouldRecordDirective(Loc)) {
    return;
  }
  std::string text = collectDirectiveText(Loc);
  if (text.empty()) {
    std::string name;
    if (const clang::IdentifierInfo *ident = MacroNameTok.getIdentifierInfo()) {
      name = ident->getName().str();
    }
    if (name.empty()) {
      name = "__macro";
    }
    text = "#ifdef " + name;
  }
  recordDirective(Loc, PreprocessingInfo::CpreprocessorIfdefDeclaration, text);
}

void SagePreprocessorRecord::Ifndef(clang::SourceLocation Loc,
                                    const clang::Token &MacroNameTok,
                                    const clang::MacroDefinition &MD) {
  (void)MD;
  if (!shouldRecordDirective(Loc)) {
    return;
  }
  std::string text = collectDirectiveText(Loc);
  if (text.empty()) {
    std::string name;
    if (const clang::IdentifierInfo *ident = MacroNameTok.getIdentifierInfo()) {
      name = ident->getName().str();
    }
    if (name.empty()) {
      name = "__macro";
    }
    text = "#ifndef " + name;
  }
  recordDirective(Loc, PreprocessingInfo::CpreprocessorIfndefDeclaration, text);
}

void SagePreprocessorRecord::Else(clang::SourceLocation Loc,
                                  clang::SourceLocation IfLoc) {
  (void)IfLoc;
  if (!shouldRecordDirective(Loc)) {
    return;
  }
  std::string text = collectDirectiveText(Loc);
  if (text.empty()) {
    text = "#else";
  }
  recordDirective(Loc, PreprocessingInfo::CpreprocessorElseDeclaration, text);
}

void SagePreprocessorRecord::Endif(clang::SourceLocation Loc,
                                   clang::SourceLocation IfLoc) {
  (void)IfLoc;
  if (!shouldRecordDirective(Loc)) {
    return;
  }
  std::string text = collectDirectiveText(Loc);
  if (text.empty()) {
    text = "#endif";
  }
  recordDirective(Loc, PreprocessingInfo::CpreprocessorEndifDeclaration, text);
}

std::pair<Sg_File_Info *, PreprocessingInfo *> SagePreprocessorRecord::top() {
  sortRecordedDirectives();
  return p_preprocessor_record_list.front();
}

bool SagePreprocessorRecord::pop() {
  p_preprocessor_record_list.erase(p_preprocessor_record_list.begin());
  return !p_preprocessor_record_list.empty();
}
