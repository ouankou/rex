
#include <algorithm>

#include <cctype>
#include <cstdlib>
#include <cstring>
#include <iostream>

#include <memory>

#include <sstream>

#include <llvm/Support/MemoryBuffer.h>
#include <llvm/Support/Program.h>

#include "clang-frontend-private.hpp"

#include "clang-frontend-utils.hpp"

#include "sage3basic.h"

#include "clang-to-dot.hpp"

#include "clang_paths.h"

#include "ompAstConstruction.h"

#include <clang/Driver/Compilation.h>
#include <clang/Driver/Driver.h>
#include <clang/Driver/Job.h>
#include <clang/Lex/Lexer.h>

#include "rose_config.h"

namespace {

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
  enum class ExceptionMode { Unspecified, Enabled, Disabled };
  ExceptionMode exception_mode = ExceptionMode::Unspecified;
  enum class RttiMode { Unspecified, Enabled, Disabled };
  RttiMode rtti_mode = RttiMode::Unspecified;

  for (int i = 0; i < argc; i++) {
    std::string current_arg(argv[i]);
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
      rtti_mode = RttiMode::Enabled;
    } else if (current_arg == "-fno-rtti") {
      // Treat as backend-only: disabling RTTI breaks C++ AST features.
    } else if (current_arg == "-rex:clang:continue-on-error") {
      continue_on_error = true;
    } else if (current_arg == "-rex:clang:disable-access-control") {
      disable_access_control = true;
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

  // In OpenMP/OpenACC AST-only mode, pragmas are parsed as plain text but
  // conditional blocks guarded by _OPENMP must remain visible. Only inject
  // _OPENMP when explicitly requested via AST-only modes and only if the user
  // has not already provided a value on the command line.
  if (openmp_ast_mode) {
    std::string openmp_define;
    for (const auto &define_value : openmp_define_list) {
      if (define_value == "_OPENMP") {
        openmp_define = "_OPENMP=201511";
        break;
      }
      if (define_value.rfind("_OPENMP=", 0) == 0) {
        std::string value = define_value.substr(strlen("_OPENMP="));
        char *endptr = nullptr;
        long parsed = std::strtol(value.c_str(), &endptr, 10);
        if (endptr == value.c_str() || *endptr != '\0') {
          openmp_define = "_OPENMP=201511";
        } else if (parsed >= 201811) {
          openmp_define = "_OPENMP=201511";
        } else {
          openmp_define = "_OPENMP=" + std::to_string(parsed);
        }
        break;
      }
    }
    if (openmp_define.empty()) {
      openmp_define = "_OPENMP=201511";
    }
    if (std::find(define_list.begin(), define_list.end(), openmp_define) ==
        define_list.end()) {
      define_list.push_back(openmp_define);
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

  if (exception_mode == ExceptionMode::Unspecified &&
      (language == ClangToSageTranslator::CPLUSPLUS ||
       language == ClangToSageTranslator::CUDA)) {
    exception_mode = ExceptionMode::Enabled;
    passthrough_args.push_back("-fexceptions");
  }

  if (rtti_mode == RttiMode::Unspecified &&
      (language == ClangToSageTranslator::CPLUSPLUS ||
       language == ClangToSageTranslator::CUDA)) {
    rtti_mode = RttiMode::Enabled;
  }

  const char *cxx_config_include_dirs_array[] = CXX_INCLUDE_STRING;
  const char *c_config_include_dirs_array[] = C_INCLUDE_STRING;

  std::vector<std::string> cxx_config_include_dirs(
      cxx_config_include_dirs_array,
      cxx_config_include_dirs_array +
          sizeof(cxx_config_include_dirs_array) / sizeof(const char *));
  std::vector<std::string> c_config_include_dirs(
      c_config_include_dirs_array,
      c_config_include_dirs_array +
          sizeof(c_config_include_dirs_array) / sizeof(const char *));

  RoseClangPathRoots clang_paths = resolveRoseClangPaths(driver_argv0);
  std::string builtin_header_root = clang_paths.builtin_header_root;

  dropRelativeIncludeDirs(c_config_include_dirs);
  dropRelativeIncludeDirs(cxx_config_include_dirs);

  sys_dirs_list.push_back(builtin_header_root);

#if LLVM_VERSION_MAJOR >= 21
  auto append_unique_dirs = [](std::vector<std::string> &dest,
                               const std::vector<std::string> &src) {
    for (const auto &entry : src) {
      if (std::find(dest.begin(), dest.end(), entry) == dest.end()) {
        dest.push_back(entry);
      }
    }
  };
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
#endif

  switch (language) {
  case ClangToSageTranslator::C:
    sys_dirs_list.insert(sys_dirs_list.begin(), c_config_include_dirs.begin(),
                         c_config_include_dirs.end());
    inc_list.push_back("clang-builtin-c.h");
    break;
  case ClangToSageTranslator::CPLUSPLUS:
    // Use configuration-driven cxx_config_include_dirs for portability
    // across different platforms, architectures, and compiler versions.
    sys_dirs_list.insert(sys_dirs_list.begin(), cxx_config_include_dirs.begin(),
                         cxx_config_include_dirs.end());
#if LLVM_VERSION_MAJOR >= 21
    // Ensure C system headers are available for libc++/libstdc++ internals
    // that include <stdint.h> and other C headers.
    append_unique_dirs(sys_dirs_list, c_config_include_dirs);
#endif
    inc_list.push_back("clang-builtin-cpp.hpp");
    break;
  case ClangToSageTranslator::CUDA:
    sys_dirs_list.insert(sys_dirs_list.begin(), cxx_config_include_dirs.begin(),
                         cxx_config_include_dirs.end());
#if LLVM_VERSION_MAJOR >= 21
    append_unique_dirs(sys_dirs_list, c_config_include_dirs);
#endif
    inc_list.push_back("clang-builtin-cuda.hpp");
    break;
  case ClangToSageTranslator::OPENCL:
    sys_dirs_list.insert(sys_dirs_list.begin(), c_config_include_dirs.begin(),
                         c_config_include_dirs.end());
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

  // FIXME should be handle by Clang ?
  define_list.push_back("__I__=_Complex_I");

#if LLVM_VERSION_MAJOR >= 21
  // Avoid staged Clang resource headers that cause include_next loops.
  filter_staged_resource_dirs(sys_dirs_list);
#endif

  // If user explicitly provided -D_OPENMP=value on command line, honor it
  // Otherwise, when -fopenmp is passed to Clang (enable_openmp=true), Clang
  // will automatically define _OPENMP with the correct version for its OpenMP
  // runtime. We should NOT override Clang's built-in _OPENMP macro with a
  // hardcoded value.
  // Do not pass _OPENMP to the Clang frontend: OpenMP pragmas are handled as
  // plain text and Clang's omp.h expects OpenMP-enabled parsing semantics.

#if LLVM_VERSION_MAJOR >= 21
  const size_t estimated_argc = 1 + define_list.size() + inc_dirs_list.size() +
                                (sys_dirs_list.size() * 2) +
                                (inc_list.size() * 2) + passthrough_args.size();
#else
  const size_t estimated_argc = 1 + define_list.size() + inc_dirs_list.size() +
                                sys_dirs_list.size() + inc_list.size() +
                                passthrough_args.size();
#endif
  std::vector<std::string> args_storage;
  args_storage.reserve(estimated_argc);
  args_storage.push_back(language_arg);
  for (const auto &define : define_list) {
    args_storage.push_back("-D" + define);
  }
  for (const auto &inc_dir : inc_dirs_list) {
    args_storage.push_back("-I" + inc_dir);
  }
#if LLVM_VERSION_MAJOR >= 21
  for (const auto &sys_dir : sys_dirs_list) {
    args_storage.push_back("-isystem");
    args_storage.push_back(sys_dir);
  }
  for (const auto &inc : inc_list) {
    args_storage.push_back("-include");
    args_storage.push_back(inc);
  }
#else
  for (const auto &sys_dir : sys_dirs_list) {
    args_storage.push_back("-isystem" + sys_dir);
  }
  for (const auto &inc : inc_list) {
    args_storage.push_back("-include" + inc);
  }
#endif
  for (const auto &pass : passthrough_args) {
    args_storage.push_back(pass);
  }

  std::vector<const char *> args;
  args.reserve(args_storage.size());
  for (const auto &arg : args_storage) {
    args.push_back(arg.c_str());
  }
#if LLVM_VERSION_MAJOR >= 21
  if (std::getenv("ROSE_CLANG_DUMP_INCLUDES") != nullptr) {
    std::cerr << "ROSE clang invocation args:\n";
    for (const auto &arg : args_storage) {
      std::cerr << "  " << arg << '\n';
    }
  }
#endif
#if DEBUG_ARGS
  for (size_t index = 0; index < args.size(); ++index) {
    std::cerr << "args[" << index << "] = " << args[index] << std::endl;
  }
#endif

  // 2 - Create a compiler instance

#if LLVM_VERSION_MAJOR >= 21
  auto diag_opts = std::make_shared<clang::DiagnosticOptions>();
#else
  llvm::IntrusiveRefCntPtr<clang::DiagnosticOptions> diag_opts =
      llvm::makeIntrusiveRefCnt<clang::DiagnosticOptions>();
#endif

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
  // outlives the diagnostics engine for both LLVM 20 and LLVM 21+.
  clang::TextDiagnosticPrinter *diag_printer =
#if LLVM_VERSION_MAJOR >= 21
      new clang::TextDiagnosticPrinter(llvm::errs(), *diag_opts);
#else
      new clang::TextDiagnosticPrinter(llvm::errs(), diag_opts.get());
#endif

  // LLVM 20+ API - requires VFS as first parameter
  compiler_instance->createDiagnostics(*vfs, diag_printer, true);

  clang::CompilerInvocation &invocation = compiler_instance->getInvocation();
  const std::string default_triple = llvm::sys::getDefaultTargetTriple();

  // Use Clang's driver to build cc1 arguments so target options (e.g. RISC-V
  // ABI defaults) match the toolchain instead of cc1's soft-float defaults.
  std::string driver_executable;
  bool resolved_clang_driver = false;
  {
    const std::string versioned_clang =
        "clang-" + std::to_string(LLVM_VERSION_MAJOR);
    if (auto clang_path = llvm::sys::findProgramByName(versioned_clang)) {
      driver_executable = clang_path.get();
      resolved_clang_driver = true;
    } else if (auto clang_path = llvm::sys::findProgramByName("clang")) {
      driver_executable = clang_path.get();
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
  if (resolved_clang_driver) {
    driver.ResourceDir = clang::CompilerInvocation::GetResourcesPath(
        driver_executable.c_str(), (void *)&clang_main);
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
  llvm::ArrayRef<const char *> argsArrayRef(cc1_args.data(), cc1_args.size());
  clang::CompilerInvocation::CreateFromArgs(
      invocation, argsArrayRef, compiler_instance->getDiagnostics());
  clang::TargetOptions &target_opts = invocation.getTargetOpts();
  ensureX86BaselineTargetFeatures(target_opts);

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

  // Enable Clang's builtin includes (provides compiler-specific headers like
  // stddef.h)
  headerSearchOpts.UseBuiltinIncludes = true;
  headerSearchOpts.UseStandardSystemIncludes = true;
  headerSearchOpts.UseStandardCXXIncludes = true;

#if LLVM_VERSION_MAJOR >= 21
  auto add_header_path = [&](const std::string &path,
                             clang::frontend::IncludeDirGroup group) {
    auto already_present =
        std::any_of(headerSearchOpts.UserEntries.begin(),
                    headerSearchOpts.UserEntries.end(),
                    [&](const clang::HeaderSearchOptions::Entry &entry) {
                      return entry.Path == path && entry.Group == group;
                    });
    if (!already_present) {
      headerSearchOpts.AddPath(path, group, false, false);
    }
  };

  for (const auto &inc_dir : inc_dirs_list) {
    add_header_path(inc_dir, clang::frontend::Angled);
  }
  for (const auto &sys_dir : sys_dirs_list) {
    add_header_path(sys_dir, clang::frontend::System);
  }

  if (std::getenv("ROSE_CLANG_DUMP_INCLUDES") != nullptr) {
    llvm::errs() << "ROSE clang header search paths:\n";
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
#endif

  // ResourceDir and system include paths are already correctly set by
  // CreateFromArgs() based on the Clang installation and target triple. Do NOT
  // override them with hard-coded paths.

  clang::LangOptions &lang_opts = compiler_instance->getLangOpts();
  std::vector<std::string> lang_specific_includes;
  clang::LangStandard::Kind requested_std = lang_opts.LangStd;
  clang::LangStandard std_info =
      clang::LangStandard::getLangStandardForKind(requested_std);
  clang::Language clang_lang = clang::Language::C;
  bool enable_cuda = false;
  bool enable_opencl = false;

  switch (language) {
  case ClangToSageTranslator::C:
    clang_lang = clang::Language::C;
    break;
  case ClangToSageTranslator::CPLUSPLUS:
    if (!std_info.isCPlusPlus()) {
      requested_std = clang::LangStandard::lang_gnucxx17;
    }
    clang_lang = clang::Language::CXX;
    break;
  case ClangToSageTranslator::CUDA:
    if (!std_info.isCPlusPlus()) {
      requested_std = clang::LangStandard::lang_gnucxx17;
    }
    clang_lang = clang::Language::CUDA;
    enable_cuda = true;
    break;
  case ClangToSageTranslator::OPENCL:
    if (!std_info.isOpenCL()) {
      requested_std = clang::LangStandard::lang_opencl30;
    }
    clang_lang = clang::Language::OpenCL;
    enable_opencl = true;
    break;
  case ClangToSageTranslator::OBJC:
    ROSE_ASSERT(!"Objective-C is not supported by ROSE Compiler.");
  default:
    ROSE_ABORT();
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
    // OpenMP/OpenACC AST-only parsing should tolerate non-standard C code used
    // in legacy tests (e.g., void main, implicit functions). Keep C++ in
    // hosted mode so standard headers (iostream, etc.) remain usable.
    if (language == ClangToSageTranslator::C) {
      lang_opts.Freestanding = 1;
      lang_opts.C99 = 0;
      lang_opts.C11 = 0;
      lang_opts.C17 = 0;
      lang_opts.C23 = 0;
      lang_opts.GNUMode = 1;
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
    if (rtti_mode == RttiMode::Disabled) {
      lang_opts.RTTI = 0;
      lang_opts.RTTIData = 0;
    } else {
      lang_opts.RTTI = 1;
      lang_opts.RTTIData = 1;
    }
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

  // LLVM 20 requires shared_ptr, LLVM 21+ requires reference
#if LLVM_VERSION_MAJOR >= 21
  clang::TargetInfo *target_info = clang::TargetInfo::CreateTargetInfo(
      compiler_instance->getDiagnostics(), invocation.getTargetOpts());
#else
  auto target_options =
      std::make_shared<clang::TargetOptions>(invocation.getTargetOpts());
  clang::TargetInfo *target_info = clang::TargetInfo::CreateTargetInfo(
      compiler_instance->getDiagnostics(), target_options);
#endif
  compiler_instance->setTarget(target_info);

  compiler_instance->createSourceManager(compiler_instance->getFileManager());

  // In LLVM 20, getFileRef returns Expected<FileEntryRef> instead of ErrorOr
  llvm::Expected<clang::FileEntryRef> ret =
      compiler_instance->getFileManager().getFileRef(input_file);
  if (!ret) {
    llvm::errs() << "Error opening file: " << input_file << "\n";
    ROSE_ABORT();
  }
  clang::FileEntryRef input_file_entry = *ret;
  // In LLVM 20, createFileID takes FileEntryRef instead of const FileEntry*
  clang::FileID mainFileID = compiler_instance->getSourceManager().createFileID(
      input_file_entry, clang::SourceLocation(), clang::SrcMgr::C_User);
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
  {
    clang::Preprocessor &PP = compiler_instance->getPreprocessor();
    auto omp_callback_owner = std::make_unique<RoseOpenMPPragmaCallback>(
        compiler_instance->getSourceManager(), PP);
    omp_callback = omp_callback_owner.get();
    PP.addPPCallbacks(std::move(omp_callback_owner));

    auto preprocessor_recorder_owner = std::make_unique<SagePreprocessorRecord>(
        &(compiler_instance->getSourceManager()));
    preprocessor_recorder = preprocessor_recorder_owner.get();
    PP.addPPCallbacks(std::move(preprocessor_recorder_owner));
    PP.addCommentHandler(preprocessor_recorder);
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

  // In LLVM 20, get error count from diagnostics directly
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
        if (info == nullptr) {
          continue;
        }
        if (!info->isCompilerGenerated() || info->isOutputInCodeGeneration()) {
          return decl;
        }
      }
      return nullptr;
    };

    SgStatement *attach_target = find_last_output_stmt(global_scope);
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
        entry.second->setRelativePosition(PreprocessingInfo::after);
        attach_target->addToAttachedPreprocessingInfo(entry.second);
      }
      if (!translator.preprocessor_pop()) {
        break;
      }
    }
  }

  if (global_scope != nullptr) {
    auto info_less = [](const PreprocessingInfo *lhs,
                        const PreprocessingInfo *rhs) -> bool {
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
      return lhs_info->get_col() < rhs_info->get_col();
    };

    auto sort_attached = [&](SgLocatedNode *loc) {
      if (loc == nullptr) {
        return;
      }
      AttachedPreprocessingInfoType *info = loc->getAttachedPreprocessingInfo();
      if (info == nullptr || info->size() < 2) {
        return;
      }
      std::stable_sort(info->begin(), info->end(), info_less);
    };

    Rose_STL_Container<SgNode *> located_nodes =
        NodeQuery::querySubTree(global_scope, V_SgLocatedNode);
    for (SgNode *node : located_nodes) {
      sort_attached(isSgLocatedNode(node));
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
    bool has_omp_or_acc = false;
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
      if (key == "omp" || key == "acc") {
        has_omp_or_acc = true;
        break;
      }
    }
    if (has_omp_or_acc) {
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

      if (!source_file->get_openmp() && !openmp_explicitly_disabled) {
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

        // In LLVM 20, getFileEntryForID still returns const FileEntry*.
        const clang::FileEntry *fileEntry = sm.getFileEntryForID(file_begin);
        std::string file;
        if (fileEntry) {
          // In LLVM 20, FileEntry uses tryGetRealPathName() instead of
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

  auto should_attach_preproc = [](SgLocatedNode *node) -> bool {
    if (node == NULL) {
      return false;
    }
    Sg_File_Info *info = node->get_file_info();
    if (info == NULL) {
      return false;
    }
    if (!info->isCompilerGenerated()) {
      return true;
    }
    return info->isOutputInCodeGeneration();
  };

  bool passed_cursor = *current_pos >= *(inheritedValue->cursor);

  while (passed_cursor) {
    if (inheritedValue->next_to_insert != NULL) {
      if (!should_attach_preproc(loc_node)) {
        return inheritedValue;
      }
      loc_node->addToAttachedPreprocessingInfo(inheritedValue->next_to_insert);
    }
    if (!inheritedValue->advance()) {
      return NULL;
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

  if (requires_hash(directive_type)) {
    size_t first_nonspace = content.find_first_not_of(" \t");
    if (first_nonspace != std::string::npos && content[first_nonspace] != '#') {
      content.insert(first_nonspace, "#");
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

  std::string include_target;
  if (IsAngled) {
    include_target = "<" + FileName.str() + ">";
  } else {
    include_target = "\"" + FileName.str() + "\"";
  }

  std::string include_text = directive + include_target + "\n";

  recordDirective(HashLoc, directive_type, include_text);
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
