
#include <algorithm>

#include <cctype>
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

#include "sage3basic.h"

#include "clang-to-dot.hpp"

#include "clang_paths.h"

#include "clang-include-option.h"

#include "ompAstConstruction.h"

#include <clang/Basic/DiagnosticFrontend.h>
#include <clang/Basic/DiagnosticSema.h>
#include <clang/Basic/SourceLocation.h>
#include <clang/Driver/Compilation.h>
#include <clang/Driver/Driver.h>
#include <clang/Driver/Job.h>
#include <clang/Lex/Lexer.h>

#include "rose_config.h"
#include "rose_paths.h"

namespace {

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

struct TokenWithOffset {
  clang::Token token;
  unsigned offset;
};

// Preserve legacy frontend acceptance of explicit member specializations that
// omit the required template<> header.
std::unique_ptr<llvm::MemoryBuffer>
maybeFixMissingTemplateHeader(clang::SourceManager &source_manager,
                              clang::FileID file_id,
                              const clang::LangOptions &lang_opts,
                              std::vector<unsigned> *insert_offsets_out) {
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
    tokens.push_back(
        {token, source_manager.getFileOffset(token.getLocation())});
  }

  if (tokens.empty()) {
    return nullptr;
  }

  auto find_prev_significant = [&](size_t index, size_t *out) -> bool {
    if (out == nullptr) {
      return false;
    }
    for (size_t i = index; i-- > 0;) {
      if (!tokens[i].token.is(clang::tok::comment)) {
        *out = i;
        return true;
      }
    }
    return false;
  };

  auto is_boundary = [](const clang::Token &tok) {
    return tok.isOneOf(clang::tok::semi, clang::tok::l_brace,
                       clang::tok::r_brace);
  };

  auto find_statement_start = [&](size_t index) -> size_t {
    size_t boundary = index;
    while (boundary > 0 && !is_boundary(tokens[boundary - 1].token)) {
      --boundary;
    }
    return boundary;
  };

  auto is_block_brace = [&](size_t index) -> bool {
    size_t prev_index = 0;
    if (!find_prev_significant(index, &prev_index)) {
      return false;
    }
    const clang::Token &prev = tokens[prev_index].token;
    if (prev.isOneOf(clang::tok::kw_do, clang::tok::kw_try,
                     clang::tok::kw_else)) {
      return true;
    }
    size_t start = find_statement_start(index);
    for (size_t i = index; i-- > start;) {
      if (tokens[i].token.isOneOf(clang::tok::r_paren, clang::tok::r_square)) {
        return true;
      }
    }
    return false;
  };

  auto is_namespace_brace = [&](size_t index) -> bool {
    size_t start = find_statement_start(index);
    for (size_t i = start; i < index; ++i) {
      if (tokens[i].token.is(clang::tok::kw_namespace)) {
        return true;
      }
    }
    return false;
  };

  auto is_record_brace = [&](size_t index) -> bool {
    size_t start = find_statement_start(index);
    int angle_depth = 0;
    for (size_t i = start; i < index; ++i) {
      const clang::Token &tok = tokens[i].token;
      if (tok.is(clang::tok::less)) {
        ++angle_depth;
        continue;
      }
      if (tok.is(clang::tok::greater)) {
        if (angle_depth > 0) {
          --angle_depth;
        }
        continue;
      }
      if (tok.is(clang::tok::greatergreater)) {
        if (angle_depth >= 2) {
          angle_depth -= 2;
        }
        continue;
      }
      if (angle_depth != 0) {
        continue;
      }
      if (tok.isOneOf(clang::tok::kw_class, clang::tok::kw_struct,
                      clang::tok::kw_union, clang::tok::kw_enum)) {
        return true;
      }
    }
    return false;
  };

  std::vector<bool> inside_block(tokens.size(), false);
  std::vector<bool> inside_record(tokens.size(), false);
  enum class BraceKind { kBlock, kRecord, kOther };
  std::vector<BraceKind> brace_stack;
  brace_stack.reserve(8);
  int block_depth = 0;
  int record_depth = 0;
  for (size_t i = 0; i < tokens.size(); ++i) {
    inside_block[i] = block_depth > 0;
    inside_record[i] = record_depth > 0;
    if (tokens[i].token.is(clang::tok::l_brace)) {
      BraceKind kind = BraceKind::kOther;
      if (is_namespace_brace(i)) {
        kind = BraceKind::kOther;
      } else if (is_record_brace(i)) {
        kind = BraceKind::kRecord;
        ++record_depth;
      } else if (is_block_brace(i)) {
        kind = BraceKind::kBlock;
        ++block_depth;
      }
      brace_stack.push_back(kind);
    } else if (tokens[i].token.is(clang::tok::r_brace)) {
      if (!brace_stack.empty()) {
        BraceKind kind = brace_stack.back();
        if (kind == BraceKind::kBlock) {
          --block_depth;
        } else if (kind == BraceKind::kRecord) {
          --record_depth;
        }
        brace_stack.pop_back();
      }
    }
  }
  auto is_identifier_like = [](const clang::Token &tok) {
    return tok.isOneOf(clang::tok::identifier, clang::tok::raw_identifier);
  };
  auto is_template_keyword = [&](const clang::Token &tok) -> bool {
    if (tok.is(clang::tok::kw_template)) {
      return true;
    }
    if (!is_identifier_like(tok)) {
      return false;
    }
    std::string spelling =
        clang::Lexer::getSpelling(tok, source_manager, lang_opts);
    return spelling == "template";
  };

  std::vector<unsigned> insert_offsets;
  insert_offsets.reserve(8);

  for (size_t i = 1; i < tokens.size(); ++i) {
    if (!tokens[i].token.is(clang::tok::coloncolon)) {
      continue;
    }
    if (!tokens[i - 1].token.isOneOf(clang::tok::greater,
                                     clang::tok::greatergreater)) {
      continue;
    }
    if (inside_block[i] || inside_record[i]) {
      continue;
    }

    bool has_template_id = false;
    for (size_t j = i; j-- > 0;) {
      if (is_boundary(tokens[j].token)) {
        break;
      }
      if (tokens[j].token.is(clang::tok::less)) {
        has_template_id = true;
        break;
      }
    }
    if (!has_template_id) {
      continue;
    }

    size_t name_index = i + 1;
    if (name_index >= tokens.size()) {
      continue;
    }
    if (tokens[name_index].token.is(clang::tok::tilde)) {
      ++name_index;
      if (name_index >= tokens.size()) {
        continue;
      }
    }
    if (!(is_identifier_like(tokens[name_index].token) ||
          tokens[name_index].token.is(clang::tok::kw_operator))) {
      continue;
    }

    bool matched = false;
    bool saw_extra_identifier = false;
    for (size_t k = name_index + 1; k < tokens.size(); ++k) {
      if (is_identifier_like(tokens[k].token)) {
        saw_extra_identifier = true;
        break;
      }
      if (tokens[k].token.is(clang::tok::l_paren)) {
        matched = !saw_extra_identifier;
        break;
      }
      if (is_boundary(tokens[k].token)) {
        break;
      }
    }
    if (!matched) {
      continue;
    }

    size_t boundary = i;
    while (boundary > 0 && !is_boundary(tokens[boundary - 1].token)) {
      --boundary;
    }

    bool has_assignment_or_return = false;
    for (size_t j = boundary; j < i; ++j) {
      if (tokens[j].token.isOneOf(clang::tok::equal, clang::tok::kw_return)) {
        has_assignment_or_return = true;
        break;
      }
    }
    if (has_assignment_or_return) {
      continue;
    }

    bool has_template_keyword = false;
    for (size_t j = boundary; j < i; ++j) {
      if (is_template_keyword(tokens[j].token)) {
        has_template_keyword = true;
        break;
      }
    }
    if (has_template_keyword) {
      continue;
    }

    insert_offsets.push_back(tokens[boundary].offset);
  }

  std::sort(insert_offsets.begin(), insert_offsets.end());
  insert_offsets.erase(
      std::unique(insert_offsets.begin(), insert_offsets.end()),
      insert_offsets.end());
  if (insert_offsets_out != nullptr) {
    *insert_offsets_out = insert_offsets;
  }

  if (insert_offsets.empty()) {
    return nullptr;
  }

  std::string updated = buffer_or->getBuffer().str();
  const std::string insertion = "template<> ";
  size_t shift = 0;
  for (unsigned offset : insert_offsets) {
    updated.insert(offset + shift, insertion);
    shift += insertion.size();
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

  // CLANG FRONTEND FIX: Enable template unparsing from AST
  // The Clang frontend doesn't save template strings like the legacy frontend
  // did, so we must unparse templates from the AST instead of from saved
  // strings. This ensures template declarations like "template <class T>" are
  // output correctly.
  sageFile.set_unparse_template_ast(true);
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
  if (!continue_on_error && openmp_ast_mode) {
    // OpenMP/OpenACC AST-only parsing should tolerate frontend errors so pragma
    // processing and unparse can proceed on partially recovered ASTs.
    continue_on_error = true;
  }
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

  if (language == ClangToSageTranslator::C ||
      language == ClangToSageTranslator::CPLUSPLUS ||
      language == ClangToSageTranslator::CUDA) {
    // Keep frontend macro state aligned with backend compilation.
    add_passthrough_flag_if_missing("-DUSE_ROSE");
    add_passthrough_flag_if_missing("-DUSE_ROSE_BACKEND");
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

  // FIXME should be handle by Clang ?
  define_list.push_back("__I__=_Complex_I");

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

  // TextDiagnosticPrinter keeps a reference to DiagnosticOptions; ensure it
  // outlives the diagnostics engine for LLVM 21.
  clang::TextDiagnosticPrinter *diag_printer =
      new clang::TextDiagnosticPrinter(llvm::errs(), *diag_opts);

  // LLVM API - requires VFS as first parameter
  compiler_instance->createDiagnostics(*vfs, diag_printer, true);

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
    resource_dir_candidate = clang::CompilerInvocation::GetResourcesPath(
        clang_driver_path.c_str(), (void *)&clang_main);
  } else if (!llvm_root.empty()) {
    const std::string llvm_fallback =
        llvm_root + "/bin/clang-" + std::to_string(LLVM_VERSION_MAJOR);
    resource_dir_candidate = clang::CompilerInvocation::GetResourcesPath(
        llvm_fallback.c_str(), (void *)&clang_main);
  } else if (resolved_clang_driver) {
    resource_dir_candidate = clang::CompilerInvocation::GetResourcesPath(
        driver_executable.c_str(), (void *)&clang_main);
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
  diags.setSeverity(clang::diag::err_alignment_not_power_of_two,
                    clang::diag::Severity::Warning, clang::SourceLocation());
  diags.setSeverity(clang::diag::err_fe_invalid_alignment,
                    clang::diag::Severity::Warning, clang::SourceLocation());

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

  // Ensure Clang's builtin and system include paths are active for both
  // LLVM 21 so the resource-dir headers are always available.
  headerSearchOpts.UseBuiltinIncludes = true;
  headerSearchOpts.UseStandardSystemIncludes = true;
  headerSearchOpts.UseStandardCXXIncludes = true;

  if (headerSearchOpts.ResourceDir.empty()) {
    if (!driver.ResourceDir.empty()) {
      headerSearchOpts.ResourceDir = driver.ResourceDir;
    } else {
      headerSearchOpts.ResourceDir =
          clang::CompilerInvocation::GetResourcesPath(driver_executable.c_str(),
                                                      (void *)&clang_main);
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
  compiler_instance->createFileManager(vfs);

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

  compiler_instance->createSourceManager(compiler_instance->getFileManager());

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

  if ((language == ClangToSageTranslator::C ||
       language == ClangToSageTranslator::CPLUSPLUS ||
       language == ClangToSageTranslator::CUDA) &&
      sageFile.get_skipfinalCompileStep() && !openmp_ast_mode &&
      !sageFile.get_openmp_lowering()) {
    // Preserve original spelling for no-backend-compile roundtrip workflows
    // (e.g., cmp-based translator tests), unless we intentionally rewrite the
    // in-memory source buffer below.
    sageFile.set_unparse_tokens(true);
  }

  std::vector<SuppressedIncludeDirective> suppressed_includes;
  if (language == ClangToSageTranslator::CPLUSPLUS ||
      language == ClangToSageTranslator::CUDA) {
    std::vector<unsigned> missing_template_offsets;
    if (auto fixed_buffer = maybeFixMissingTemplateHeader(
            compiler_instance->getSourceManager(), mainFileID, lang_opts,
            &missing_template_offsets)) {
      compiler_instance->getSourceManager().overrideFileContents(
          input_file_entry, std::move(fixed_buffer));
      // The token stream no longer matches the on-disk source; unparse from
      // the AST to preserve the normalized template<> specialization headers.
      sageFile.set_unparse_tokens(false);
    }
    if (!missing_template_offsets.empty()) {
      std::string file_name = input_file;
      if (auto entry =
              compiler_instance->getSourceManager().getFileEntryRefForID(
                  mainFileID)) {
        file_name = entry->getName().str();
      }
      sageFile.addNewAttribute(
          kMissingTemplateHeaderFixupAttributeName,
          new MissingTemplateHeaderFixupAttribute(
              std::move(file_name), std::move(missing_template_offsets)));
    }
  }
  if (openmp_ast_mode) {
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
        &(compiler_instance->getSourceManager()));
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

  finishSageAST(*translator);

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

  // REX: By default, treat Clang diagnostic errors as fatal.
  // Use -rex:clang:continue-on-error to allow backend to run with a partial
  // AST.
  if (global_scope == NULL) {
    printf("Error: Failed to build AST - global_scope is NULL\n");
    return (numErrors > 0) ? numErrors : 1; // Failure - no AST
  }

  if (numErrors > 0) {
    if (continue_on_error) {
      sageFile.set_skipfinalCompileStep(true);
      printf("Note: Proceeding to backend despite %d Clang diagnostic "
             "error(s) because AST was successfully constructed\n",
             numErrors);
      return 0; // Success - AST was built
    }
    printf("Error: Clang reported %d diagnostic error(s); refusing to run "
           "backend (use -rex:clang:continue-on-error to override)\n",
           numErrors);
    return numErrors;
  }

  return 0; // Success - AST was built
}

void finishSageAST(ClangToSageTranslator &translator) {
  SgGlobal *global_scope = translator.getGlobalScope();

  // Normalize function-symbol bindings before postprocessing that relies on
  // declaration scope symbol tables (e.g., template-name reset).
  translator.repairMissingFunctionSymbols();

  // Insert OpenMP/OpenACC pragmas that were not attached to statements
  // (e.g., standalone directives like cancel or file-scope declare simd).
  translator.appendUnattachedOpenMPPragmas();

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
      return location_leq(start, cursor) && location_leq(cursor, end);
    };
    std::function<SgLocatedNode *(SgLocatedNode *, Sg_File_Info *)>
        find_deepest_anchor;
    find_deepest_anchor = [&](SgLocatedNode *node,
                              Sg_File_Info *cursor) -> SgLocatedNode * {
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

      if (SgFunctionDeclaration *func_decl = isSgFunctionDeclaration(node)) {
        if (SgFunctionDefinition *defn = func_decl->get_definition()) {
          if (SgLocatedNode *nested =
                  find_deepest_anchor(isSgLocatedNode(defn), cursor)) {
            return nested;
          }
          if (SgBasicBlock *body = defn->get_body()) {
            if (SgLocatedNode *nested =
                    find_deepest_anchor(isSgLocatedNode(body), cursor)) {
              return nested;
            }
          }
        }
      }
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
      SageBuilder::pushScopeStack(global_scope);
      SgEmptyDeclaration *empty_decl = SageBuilder::buildEmptyDeclaration();
      SageBuilder::popScopeStack();
      if (empty_decl != nullptr) {
        global_scope->append_statement(empty_decl);
        attach_target = empty_decl;
      }
    }
    if (attach_target == nullptr) {
      attach_target = global_scope;
    }

    while (translator.preprocessor_list_size() > 0) {
      std::pair<Sg_File_Info *, PreprocessingInfo *> entry =
          translator.preprocessor_top();
      if (entry.second != nullptr) {
        SgLocatedNode *anchor = find_anchor_for_cursor(entry.first);
        if (anchor != nullptr) {
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
        SgDeclarationStatement *anchor = nullptr;
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

      auto directive_inside_member = [&](const Sg_File_Info *cursor) -> bool {
        if (cursor == nullptr || cursor->get_line() <= 0) {
          return false;
        }
        for (const MemberAnchor &entry : member_anchors) {
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
            return true;
          }
        }
        return false;
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
              !is_class_conditional_directive(info->getTypeOfDirective())) {
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
          if (directive_inside_member(info_fi)) {
            ++it;
            continue;
          }

          const bool inside_class_range =
              source_before_or_equal(class_start, info_fi) &&
              source_before_or_equal(info_fi, class_end);
          if (!inside_class_range) {
            ++it;
            continue;
          }

          SgDeclarationStatement *member_before = pick_member_before(info_fi);
          SgDeclarationStatement *member_after = pick_member_after(info_fi);

          SgDeclarationStatement *anchor = nullptr;
          PreprocessingInfo::RelativePositionType relative_position =
              PreprocessingInfo::after;
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
          } else if (type == PreprocessingInfo::CpreprocessorEndifDeclaration) {
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

    Rose_STL_Container<SgNode *> class_nodes =
        NodeQuery::querySubTree(global_scope, V_SgClassDeclaration);
    for (SgNode *node : class_nodes) {
      SgClassDeclaration *class_decl = isSgClassDeclaration(node);
      if (class_decl == nullptr) {
        continue;
      }
      SgClassDefinition *class_def = class_decl->get_definition();
      if (class_def == nullptr) {
        continue;
      }
      relocate_class_conditionals(class_def, class_def->get_members(),
                                  class_decl);
    }

    Rose_STL_Container<SgNode *> template_class_nodes =
        NodeQuery::querySubTree(global_scope, V_SgTemplateClassDeclaration);
    for (SgNode *node : template_class_nodes) {
      SgTemplateClassDeclaration *class_decl =
          isSgTemplateClassDeclaration(node);
      if (class_decl == nullptr) {
        continue;
      }
      SgTemplateClassDefinition *class_def =
          isSgTemplateClassDefinition(class_decl->get_definition());
      if (class_def == nullptr) {
        continue;
      }
      relocate_class_conditionals(class_def, class_def->get_members(),
                                  class_decl);
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
          if (owner_info == nullptr || owner_info->get_line() <= 0) {
            return;
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

    Rose_STL_Container<SgNode *> namespace_nodes = NodeQuery::querySubTree(
        global_scope, V_SgNamespaceDeclarationStatement);
    for (SgNode *node : namespace_nodes) {
      SgNamespaceDeclarationStatement *ns_decl =
          isSgNamespaceDeclarationStatement(node);
      if (ns_decl == nullptr) {
        continue;
      }
      SgNamespaceDefinitionStatement *ns_def = ns_decl->get_definition();
      if (ns_def == nullptr) {
        continue;
      }
      relocate_scope_conditionals(ns_decl, ns_def->get_declarations());
      relocate_scope_conditionals(ns_def, ns_def->get_declarations());
    }

    for (SgNode *node : class_nodes) {
      SgClassDeclaration *class_decl = isSgClassDeclaration(node);
      if (class_decl == nullptr) {
        continue;
      }
      SgClassDefinition *class_def = class_decl->get_definition();
      if (class_def == nullptr) {
        continue;
      }
      relocate_scope_conditionals(class_decl, class_def->get_members());
      relocate_scope_conditionals(class_def, class_def->get_members());
    }

    for (SgNode *node : template_class_nodes) {
      SgTemplateClassDeclaration *class_decl =
          isSgTemplateClassDeclaration(node);
      if (class_decl == nullptr) {
        continue;
      }
      SgTemplateClassDefinition *class_def =
          isSgTemplateClassDefinition(class_decl->get_definition());
      if (class_def == nullptr) {
        continue;
      }
      relocate_scope_conditionals(class_decl, class_def->get_members());
      relocate_scope_conditionals(class_def, class_def->get_members());
    }
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

  if (source_range.isValid()) {
    clang::SourceLocation begin = source_range.getBegin();
    clang::SourceLocation end = source_range.getEnd();

    if (begin.isValid() && end.isValid()) {
      clang::SourceManager &sm = p_compiler_instance->getSourceManager();
      const clang::LangOptions &lang_opts = p_compiler_instance->getLangOpts();

      bool begin_is_macro = begin.isMacroID();
      bool end_is_macro = end.isMacroID();
      clang::SourceLocation spelling_begin = sm.getSpellingLoc(begin);
      clang::SourceLocation spelling_end = sm.getSpellingLoc(end);

      if (begin_is_macro) {
#if DEBUG_SOURCE_LOCATION
        std::cerr << "\tDump SourceLocation begin as it is a MacroID: ";
        begin.dump(sm);
        std::cerr << std::endl;
#endif
        begin = sm.getExpansionLoc(begin);
        ROSE_ASSERT(begin.isValid());
      }

      if (end_is_macro) {
#if DEBUG_SOURCE_LOCATION
        std::cerr << "\tDump SourceLocation end as it is a MacroID: ";
        end.dump(sm);
        std::cerr << std::endl;
#endif
        end = sm.getExpansionLoc(end);
        ROSE_ASSERT(end.isValid());
      }

      clang::FileID file_begin = sm.getFileID(begin);
      clang::FileID file_end = sm.getFileID(end);

      if (!sm.isWrittenInMainFile(begin) &&
          sm.isWrittenInMainFile(spelling_begin)) {
        begin = spelling_begin;
        end = spelling_end;
        file_begin = sm.getFileID(begin);
        file_end = sm.getFileID(end);
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
        bool can_lex_end = end_for_fi.isFileID() && in_main_file && end_buffer;
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

            set_physical_info(start_fi, spelling_begin);
            set_physical_info(end_fi, spelling_end);
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
}

void ClangToSageTranslator::setCompilerGeneratedFileInfo(SgNode *node,
                                                         bool to_be_unparse) {
  Sg_File_Info *start_fi =
      Sg_File_Info::generateDefaultFileInfoForCompilerGeneratedNode();
  Sg_File_Info *end_fi =
      Sg_File_Info::generateDefaultFileInfoForCompilerGeneratedNode();

  start_fi->setCompilerGenerated();
  end_fi->setCompilerGenerated();

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
    if (isSgTypedefSeq(node) != nullptr ||
        isSgCatchStatementSeq(node) != nullptr ||
        isSgCtorInitializerList(node) != nullptr) {
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
  auto is_include_directive = [](const PreprocessingInfo *info) -> bool {
    if (info == nullptr) {
      return false;
    }
    PreprocessingInfo::DirectiveType type = info->getTypeOfDirective();
    return type == PreprocessingInfo::CpreprocessorIncludeDeclaration ||
           type == PreprocessingInfo::CpreprocessorIncludeNextDeclaration;
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

  if (inheritedValue->cursor != nullptr &&
      *current_pos <= *(inheritedValue->cursor)) {
    bool is_include = is_include_directive(inheritedValue->next_to_insert);
    bool record_candidate = false;
    if (is_include) {
      record_candidate =
          should_attach_include_target(loc_node, inheritedValue->cursor);
    } else {
      record_candidate = should_attach_preproc(loc_node);
    }
    if (record_candidate) {
      inheritedValue->candidat = loc_node;
    }
  }

  bool passed_cursor = *current_pos >= *(inheritedValue->cursor);

  while (passed_cursor) {
    if (inheritedValue->next_to_insert != NULL) {
      SgLocatedNode *attach_node = loc_node;
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
      PreprocessingInfo::DirectiveType directive_type =
          inheritedValue->next_to_insert->getTypeOfDirective();
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
            directive_inside_node(class_anchor, inheritedValue->cursor)) {
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
                                   : should_attach_preproc(attach_node);
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
        record_candidate = should_attach_preproc(loc_node);
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

SagePreprocessorRecord::SagePreprocessorRecord(
    clang::SourceManager *source_manager)
    : p_source_manager(source_manager), p_preprocessor_record_list(),
      p_preprocessor_record_list_sorted(true) {}

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

  clang::SourceLocation resolved = loc;
  if (resolved.isMacroID()) {
    resolved = p_source_manager->getSpellingLoc(resolved);
  }
  if (!resolved.isValid()) {
    return;
  }

  bool inv_begin_line = false;
  bool inv_begin_col = false;
  unsigned ls =
      p_source_manager->getSpellingLineNumber(resolved, &inv_begin_line);
  unsigned cs =
      p_source_manager->getSpellingColumnNumber(resolved, &inv_begin_col);
  (void)inv_begin_line;
  (void)inv_begin_col;

  std::string file = getFilenameForLocation(resolved);
  std::string content = text;
  if (!content.empty() && content.back() != '\n') {
    content.push_back('\n');
  }

  auto requires_hash = [](PreprocessingInfo::DirectiveType type) -> bool {
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

  if (requires_hash(directive_type)) {
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
  (void)MacroNameTok;
  (void)MD;
  (void)Range;
  (void)Args;
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
  if (MD != nullptr) {
    if (const clang::MacroInfo *info = MD->getMacroInfo()) {
      if (info->getDefinitionLoc().isValid()) {
        loc = info->getDefinitionLoc();
      }
    }
  }
  std::string text = collectDirectiveText(loc);
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
  if (text.empty()) {
    std::string name;
    if (const clang::IdentifierInfo *ident = MacroNameTok.getIdentifierInfo()) {
      name = ident->getName().str();
    }
    if (name.empty()) {
      name = "__macro";
    }
    text = "#define " + name;
  }
  recordDirective(loc, PreprocessingInfo::CpreprocessorDefineDeclaration, text);
}

void SagePreprocessorRecord::MacroUndefined(
    const clang::Token &MacroNameTok, const clang::MacroDefinition &MD,
    const clang::MacroDirective *Undef) {
  (void)MD;
  clang::SourceLocation loc = MacroNameTok.getLocation();
  if (Undef != nullptr && Undef->getLocation().isValid()) {
    loc = Undef->getLocation();
  }
  std::string text = collectDirectiveText(loc);
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
  if (text.empty()) {
    std::string name;
    if (const clang::IdentifierInfo *ident = MacroNameTok.getIdentifierInfo()) {
      name = ident->getName().str();
    }
    if (name.empty()) {
      name = "__macro";
    }
    text = "#undef " + name;
  }
  recordDirective(loc, PreprocessingInfo::CpreprocessorUndefDeclaration, text);
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
  (void)Range;
  (void)EndifLoc;
}

void SagePreprocessorRecord::If(
    clang::SourceLocation Loc, clang::SourceRange ConditionRange,
    clang::PPCallbacks::ConditionValueKind ConditionValue) {
  (void)ConditionRange;
  (void)ConditionValue;
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
  std::string text = collectDirectiveText(Loc);
  if (text.empty()) {
    text = "#else";
  }
  recordDirective(Loc, PreprocessingInfo::CpreprocessorElseDeclaration, text);
}

void SagePreprocessorRecord::Endif(clang::SourceLocation Loc,
                                   clang::SourceLocation IfLoc) {
  (void)IfLoc;
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
