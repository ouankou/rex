// Put here code used to construct SgOmp* nodes
// Liao 10/8/2010
#include "ompAstConstruction.h"

#include "astPostProcessing.h"

#include "rose_paths.h"

#include "sage3basic.h"

#include "sageBuilder.h"

#include <algorithm>
#include <cctype>
#include <cstdlib>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <limits>
#include <memory>
#include <mutex>
#include <sstream>
#include <tuple>
#include <unordered_map>
#include <unordered_set>

// the vector of pairs of OpenACC pragma and accparser IR.
static std::vector<std::pair<SgPragmaDeclaration *, OpenACCDirective *>>
    OpenACCIR_list;
OpenACCDirective *accparser_OpenACCIR;

std::map<SgPragmaDeclaration *, OpenMPDirective *> fortran_paired_pragma_dict;
std::map<SgPragmaDeclaration *, OpenACCDirective *>
    fortran_acc_paired_pragma_dict;

static const char *const kAccFortranEndAttributeName = "acc_fortran_end";
static const char *const kOmpFortranEndAttributeName = "omp_fortran_end";
static const char *const kFortranKeepOpenMPPragmaAttributeName =
    "fortran_keep_openmp_pragma";
static const char *const kOmpCombinedParallelNestedVariantAttrName =
    "omp_combined_parallel_nested_variant";
static const char *const kOmpDeclareTargetExtendedListAttrName =
    "omp_declare_target_extended_list";
static const char *const kFortranOmpSourceTextAttributeName =
    "fortran_omp_source_text";
static const char *const kOmpDirectiveSpellingOverrideAttrName =
    "omp_directive_spelling_override";

class AccFortranEndAttribute : public AstAttribute {
public:
  OwnershipPolicy getOwnershipPolicy() const override {
    return CONTAINER_OWNERSHIP;
  }
  AstAttribute *copy() const override {
    return new AccFortranEndAttribute(*this);
  }
};

class OmpFortranEndAttribute : public AstAttribute {
public:
  OwnershipPolicy getOwnershipPolicy() const override {
    return CONTAINER_OWNERSHIP;
  }
  AstAttribute *copy() const override {
    return new OmpFortranEndAttribute(*this);
  }
};

class OmpDeclareTargetExtendedListAttribute : public AstAttribute {
public:
  OwnershipPolicy getOwnershipPolicy() const override {
    return CONTAINER_OWNERSHIP;
  }
  AstAttribute *copy() const override {
    return new OmpDeclareTargetExtendedListAttribute(*this);
  }
};

class KeepFortranOpenMPPragmaAttribute : public AstAttribute {
public:
  OwnershipPolicy getOwnershipPolicy() const override {
    return CONTAINER_OWNERSHIP;
  }
  AstAttribute *copy() const override {
    return new KeepFortranOpenMPPragmaAttribute(*this);
  }
};

class OmpOwnedIntAttribute : public AstIntAttribute {
public:
  OmpOwnedIntAttribute() : AstIntAttribute() {}
  explicit OmpOwnedIntAttribute(int value) : AstIntAttribute(value) {}
  OmpOwnedIntAttribute(const OmpOwnedIntAttribute &other)
      : AstIntAttribute(other) {}

  OwnershipPolicy getOwnershipPolicy() const override {
    return CONTAINER_OWNERSHIP;
  }

  AstAttribute *copy() const override {
    return new OmpOwnedIntAttribute(*this);
  }
};
std::vector<std::tuple<SgLocatedNode *, PreprocessingInfo *, OpenMPDirective *>>
    fortran_omp_pragma_list;

struct OmpParsedExpression {
  OpenMPExprParseMode mode = OMP_EXPR_PARSE_none;
  std::string text;
  SgNode *node = nullptr;
  std::vector<std::pair<SgExpression *, SgExpression *>> dimensions;
};

struct OmpClauseParseCache {
  std::vector<std::unique_ptr<OmpParsedExpression>> owned_nodes;
  std::unordered_map<const OpenMPClause *,
                     std::vector<const OmpParsedExpression *>>
      clause_expression_nodes;
  std::unordered_map<int, std::vector<const OmpParsedExpression *>>
      clause_expression_nodes_by_position;
  std::unordered_map<const OpenMPClause *,
                     std::vector<std::vector<OpenMPMapClause::DistDataPolicy>>>
      map_dist_data_policies;
  std::unordered_map<int,
                     std::vector<std::vector<OpenMPMapClause::DistDataPolicy>>>
      map_dist_data_policies_by_position;
  std::unordered_map<const OpenMPClause *,
                     std::vector<std::vector<const OmpParsedExpression *>>>
      map_dist_data_policy_nodes;
  std::unordered_map<int, std::vector<std::vector<const OmpParsedExpression *>>>
      map_dist_data_policy_nodes_by_position;
};

static std::unordered_map<OpenMPDirective *, OmpClauseParseCache>
    g_omp_clause_nodes;

struct PendingCommentedDirectiveRelocation {
  SgLocatedNode *owner = nullptr;
  PreprocessingInfo *info = nullptr;
  int source_line = 0;
};

static std::unordered_map<SgPragmaDeclaration *,
                          std::vector<PendingCommentedDirectiveRelocation>>
    g_pending_commented_directive_relocations;
static std::unordered_map<SgPragmaDeclaration *, std::string>
    g_omp_directive_source_text_by_pragma;

struct OmpExprParseContext {
  SgPragmaDeclaration *pragma_declaration = nullptr;
  OpenMPDirective *directive = nullptr;
  const std::string *directive_source_text = nullptr;
  std::vector<std::unique_ptr<OmpParsedExpression>> owned_nodes;
};

OpenMPDirective *ompparser_OpenMPIR;
static bool use_ompparser = false;
static bool use_accparser = false;

void mergeEndClausesToBeginDirective(OpenMPDirective *begin_decl,
                                     OpenMPDirective *end_decl,
                                     OpenMPDirective *end_wrapper);

SgOmpFailClause *
convertFailClause(SgStatement *directive,
                  std::pair<SgPragmaDeclaration *, OpenMPDirective *>
                      current_OpenMPIR_to_SageIII,
                  OpenMPClause *current_omp_clause);

static SgOmpClause::omp_fail_memory_order_kind_enum
toSgOmpClauseFailMemoryOrder(OpenMPFailClauseMemoryOrder memory_order);

using namespace std;
using namespace SageInterface;
using namespace SageBuilder;
using namespace OmpSupport;

namespace {
SgExpression *buildOpaqueOpenMPClauseExpression(SgPragmaDeclaration *directive,
                                                const std::string &text);
bool collectArraySectionDimensions(
    SgExpression *expression,
    std::vector<std::pair<SgExpression *, SgExpression *>> &dimensions);
SgVariableSymbol *extractClauseVariableSymbol(SgNode *node);
SgVariableSymbol *extractDirectArraySectionSymbol(SgNode *node);
std::string trimWhitespaceCopy(const std::string &value);
const OmpParsedExpression *findParsedExpressionByText(
    const std::vector<const OmpParsedExpression *> *parsed_nodes,
    const std::string &expression_text, OpenMPExprParseMode required_mode);

unsigned getLocatedNodeLine(const SgLocatedNode *node) {
  if (node == nullptr) {
    return 0;
  }

  if (const Sg_File_Info *info = node->get_file_info()) {
    if (info->get_line() > 0) {
      return info->get_line();
    }
  }

  if (const Sg_File_Info *info = node->get_startOfConstruct()) {
    return info->get_line();
  }

  return 0;
}

const OmpDeclareVariantRegionInfo *
findDeclareVariantRegionInfo(SgPragmaDeclaration *pragma_declaration) {
  if (pragma_declaration == nullptr) {
    return nullptr;
  }

  SgSourceFile *source_file = getEnclosingSourceFile(pragma_declaration);
  if (source_file == nullptr) {
    return nullptr;
  }

  AstAttribute *attribute =
      source_file->getAttribute(kOmpDeclareVariantRegionsAttributeName);
  auto *regions = dynamic_cast<OmpDeclareVariantRegionsAttribute *>(attribute);
  if (regions == nullptr) {
    return nullptr;
  }

  return regions->findByBeginLine(getLocatedNodeLine(pragma_declaration));
}

SgExpression *
parseDeclareVariantExpression(SgPragmaDeclaration *directive,
                              const std::string &expression_text) {
  const std::string trimmed = trimWhitespaceCopy(expression_text);
  if (trimmed.empty()) {
    return nullptr;
  }

  SgExpression *expr = parseOmpExpression(directive, OMPC_match, trimmed);
  if (expr == nullptr) {
    expr = buildOpaqueOpenMPClauseExpression(directive, trimmed);
  }
  return expr;
}

struct DeclareMapperTypeQualifiers {
  bool is_const = false;
  bool is_volatile = false;
  bool is_restrict = false;
};

enum class DeclareMapperTypeOperatorKind {
  e_pointer,
  e_lvalue_reference,
  e_rvalue_reference
};

struct DeclareMapperTypeOperator {
  DeclareMapperTypeOperatorKind kind = DeclareMapperTypeOperatorKind::e_pointer;
  DeclareMapperTypeQualifiers qualifiers;
};

static bool isDeclareMapperQualifierToken(const std::string &token) {
  return token == "const" || token == "volatile" || token == "restrict" ||
         token == "__restrict" || token == "__restrict__";
}

static bool isDeclareMapperElaboratedTypeKeyword(const std::string &token) {
  return token == "struct" || token == "class" || token == "union" ||
         token == "enum" || token == "typename";
}

static void setDeclareMapperQualifier(DeclareMapperTypeQualifiers &qualifiers,
                                      const std::string &token) {
  if (token == "const") {
    qualifiers.is_const = true;
  } else if (token == "volatile") {
    qualifiers.is_volatile = true;
  } else if (token == "restrict" || token == "__restrict" ||
             token == "__restrict__") {
    qualifiers.is_restrict = true;
  }
}

static bool isDeclareMapperWordToken(const std::string &token) {
  if (token.empty()) {
    return false;
  }

  const unsigned char first = static_cast<unsigned char>(token[0]);
  return std::isalnum(first) || first == '_';
}

static std::vector<std::string>
tokenizeDeclareMapperTypeText(const std::string &type_text) {
  std::vector<std::string> tokens;

  for (std::string::size_type i = 0; i < type_text.size();) {
    const unsigned char ch = static_cast<unsigned char>(type_text[i]);
    if (std::isspace(ch)) {
      ++i;
      continue;
    }

    if (std::isalpha(ch) || ch == '_') {
      const std::string::size_type begin = i++;
      while (i < type_text.size()) {
        const unsigned char next = static_cast<unsigned char>(type_text[i]);
        if (!std::isalnum(next) && next != '_') {
          break;
        }
        ++i;
      }
      tokens.push_back(type_text.substr(begin, i - begin));
      continue;
    }

    if (type_text.compare(i, 2, "::") == 0) {
      tokens.push_back("::");
      i += 2;
      continue;
    }

    if (type_text.compare(i, 2, "&&") == 0) {
      tokens.push_back("&&");
      i += 2;
      continue;
    }

    if (type_text.compare(i, 3, "...") == 0) {
      tokens.push_back("...");
      i += 3;
      continue;
    }

    if (std::strchr("*&,()<>[]", type_text[i]) != nullptr) {
      tokens.push_back(std::string(1, type_text[i]));
      ++i;
      continue;
    }

    tokens.push_back(std::string(1, type_text[i]));
    ++i;
  }

  return tokens;
}

static void appendDeclareMapperTypeTokenSpacing(std::string &result,
                                                const std::string &previous,
                                                const std::string &current) {
  if (previous.empty() || current.empty()) {
    return;
  }

  if (previous == "::" || current == "::") {
    return;
  }

  if (previous == "<" || previous == "(" || previous == "[") {
    return;
  }

  if (current == ">" || current == "," || current == ")" || current == "]" ||
      current == "<") {
    return;
  }

  if (previous == "," || previous == ">" || previous == ")" ||
      previous == "]") {
    result += ' ';
    return;
  }

  if (isDeclareMapperWordToken(previous) && isDeclareMapperWordToken(current)) {
    result += ' ';
    return;
  }
}

static std::string
joinDeclareMapperTypeTokens(const std::vector<std::string> &tokens) {
  std::string result;
  std::string previous;
  for (const std::string &token : tokens) {
    appendDeclareMapperTypeTokenSpacing(result, previous, token);
    result += token;
    previous = token;
  }
  return result;
}

static bool
collectDeclareMapperBaseTypeData(const std::vector<std::string> &tokens,
                                 std::vector<std::string> &base_name,
                                 DeclareMapperTypeQualifiers &base_qualifiers) {
  int angle_depth = 0;
  int paren_depth = 0;
  int bracket_depth = 0;

  for (const std::string &token : tokens) {
    const bool top_level =
        angle_depth == 0 && paren_depth == 0 && bracket_depth == 0;

    if (top_level && base_name.empty() &&
        isDeclareMapperElaboratedTypeKeyword(token)) {
      continue;
    }

    if (top_level && isDeclareMapperQualifierToken(token)) {
      setDeclareMapperQualifier(base_qualifiers, token);
    } else {
      base_name.push_back(token);
    }

    if (token == "(") {
      ++paren_depth;
    } else if (token == ")") {
      --paren_depth;
    } else if (token == "[") {
      ++bracket_depth;
    } else if (token == "]") {
      --bracket_depth;
    } else if (paren_depth == 0 && bracket_depth == 0 && token == "<") {
      ++angle_depth;
    } else if (paren_depth == 0 && bracket_depth == 0 && token == ">") {
      --angle_depth;
    }

    if (angle_depth < 0 || paren_depth < 0 || bracket_depth < 0) {
      return false;
    }
  }

  return angle_depth == 0 && paren_depth == 0 && bracket_depth == 0;
}

static bool parseDeclareMapperTypeOperators(
    const std::vector<std::string> &tokens, size_t start_index,
    std::vector<DeclareMapperTypeOperator> &operators) {
  size_t i = start_index;
  while (i < tokens.size()) {
    DeclareMapperTypeOperator current_operator;
    const std::string &token = tokens[i];
    if (token == "*") {
      current_operator.kind = DeclareMapperTypeOperatorKind::e_pointer;
    } else if (token == "&") {
      current_operator.kind = DeclareMapperTypeOperatorKind::e_lvalue_reference;
    } else if (token == "&&") {
      current_operator.kind = DeclareMapperTypeOperatorKind::e_rvalue_reference;
    } else {
      return false;
    }
    ++i;

    while (i < tokens.size() && isDeclareMapperQualifierToken(tokens[i])) {
      if (current_operator.kind != DeclareMapperTypeOperatorKind::e_pointer) {
        return false;
      }
      setDeclareMapperQualifier(current_operator.qualifiers, tokens[i]);
      ++i;
    }

    operators.push_back(current_operator);
  }

  return true;
}

static SgType *
buildQualifiedDeclareMapperType(SgType *base_type,
                                const DeclareMapperTypeQualifiers &qualifiers) {
  if (base_type == nullptr) {
    return nullptr;
  }

  if (!qualifiers.is_const && !qualifiers.is_volatile &&
      !qualifiers.is_restrict) {
    return base_type;
  }

  SgModifierType *result = new SgModifierType(base_type);
  ROSE_ASSERT(result != nullptr);

  SgTypeModifier &type_modifier = result->get_typeModifier();
  if (qualifiers.is_const) {
    type_modifier.get_constVolatileModifier().setConst();
  }
  if (qualifiers.is_volatile) {
    type_modifier.get_constVolatileModifier().setVolatile();
  }
  if (qualifiers.is_restrict) {
    type_modifier.setRestrict();
  }

  SgModifierType *canonical =
      SgModifierType::insertModifierTypeIntoTypeTable(result);
  if (canonical != result) {
    delete result;
  }
  return canonical;
}

SgType *resolveDeclareMapperType(SgPragmaDeclaration *directive,
                                 const std::string &type_text) {
  const std::string normalized = trimWhitespaceCopy(type_text);
  if (normalized.empty()) {
    return nullptr;
  }

  const std::vector<std::string> tokens =
      tokenizeDeclareMapperTypeText(normalized);
  if (tokens.empty()) {
    return nullptr;
  }

  int angle_depth = 0;
  int paren_depth = 0;
  int bracket_depth = 0;
  size_t declarator_start = tokens.size();
  for (size_t i = 0; i < tokens.size(); ++i) {
    const std::string &token = tokens[i];
    if (angle_depth == 0 && paren_depth == 0 && bracket_depth == 0 &&
        (token == "*" || token == "&" || token == "&&")) {
      declarator_start = i;
      break;
    }

    if (token == "(") {
      ++paren_depth;
    } else if (token == ")") {
      --paren_depth;
    } else if (token == "[") {
      ++bracket_depth;
    } else if (token == "]") {
      --bracket_depth;
    } else if (paren_depth == 0 && bracket_depth == 0 && token == "<") {
      ++angle_depth;
    } else if (paren_depth == 0 && bracket_depth == 0 && token == ">") {
      --angle_depth;
    }

    if (angle_depth < 0 || paren_depth < 0 || bracket_depth < 0) {
      return nullptr;
    }
  }

  if (angle_depth != 0 || paren_depth != 0 || bracket_depth != 0) {
    return nullptr;
  }

  std::vector<std::string> base_name_tokens;
  DeclareMapperTypeQualifiers base_qualifiers;
  if (!collectDeclareMapperBaseTypeData(
          std::vector<std::string>(tokens.begin(),
                                   tokens.begin() + declarator_start),
          base_name_tokens, base_qualifiers)) {
    return nullptr;
  }
  if (base_name_tokens.empty()) {
    return nullptr;
  }

  std::vector<DeclareMapperTypeOperator> operators;
  if (!parseDeclareMapperTypeOperators(tokens, declarator_start, operators)) {
    return nullptr;
  }

  SgScopeStatement *scope =
      directive != nullptr ? directive->get_scope() : NULL;
  SgType *resolved_type = SageInterface::lookupNamedTypeInParentScopes(
      joinDeclareMapperTypeTokens(base_name_tokens), scope);
  if (resolved_type == nullptr) {
    return nullptr;
  }

  resolved_type =
      buildQualifiedDeclareMapperType(resolved_type, base_qualifiers);

  for (const DeclareMapperTypeOperator &current_operator : operators) {
    if (current_operator.kind == DeclareMapperTypeOperatorKind::e_pointer) {
      resolved_type = SageBuilder::buildPointerType(resolved_type);
      resolved_type = buildQualifiedDeclareMapperType(
          resolved_type, current_operator.qualifiers);
    } else if (current_operator.kind ==
               DeclareMapperTypeOperatorKind::e_lvalue_reference) {
      resolved_type = SageBuilder::buildReferenceType(resolved_type);
    } else if (current_operator.kind ==
               DeclareMapperTypeOperatorKind::e_rvalue_reference) {
      resolved_type = SageBuilder::buildRvalueReferenceType(resolved_type);
    }
  }

  return resolved_type;
}

void setDirectiveSpellingOverride(SgStatement *statement,
                                  const std::string &spelling) {
  if (statement == nullptr || spelling.empty()) {
    return;
  }

  AstAttribute *existing =
      statement->getAttribute(kOmpDirectiveSpellingOverrideAttrName);
  if (auto *value = dynamic_cast<AstValueAttribute<std::string> *>(existing)) {
    value->set(spelling);
    return;
  }

  statement->addNewAttribute(kOmpDirectiveSpellingOverrideAttrName,
                             new AstValueAttribute<std::string>(spelling));
}

bool isCommentedOutDirective(const PreprocessingInfo *info) {
  if (info == nullptr) {
    return false;
  }

  const PreprocessingInfo::DirectiveType type = info->getTypeOfDirective();
  if (type != PreprocessingInfo::CplusplusStyleComment &&
      type != PreprocessingInfo::C_StyleComment &&
      type != PreprocessingInfo::FortranStyleComment &&
      type != PreprocessingInfo::F90StyleComment) {
    return false;
  }

  std::string text = info->getString();
  std::transform(text.begin(), text.end(), text.begin(), [](unsigned char ch) {
    return static_cast<char>(std::tolower(ch));
  });

  const std::string::size_type pragma_pos = text.find("#pragma");
  if (pragma_pos == std::string::npos) {
    return false;
  }

  return text.find("omp", pragma_pos) != std::string::npos ||
         text.find("acc", pragma_pos) != std::string::npos;
}

bool startsWithCaseInsensitiveKeyword(const std::string &text, size_t pos,
                                      const char *keyword) {
  const size_t keyword_len = std::strlen(keyword);
  if (pos + keyword_len > text.size()) {
    return false;
  }
  for (size_t i = 0; i < keyword_len; ++i) {
    const unsigned char lhs = static_cast<unsigned char>(text[pos + i]);
    const unsigned char rhs = static_cast<unsigned char>(keyword[i]);
    if (std::tolower(lhs) != std::tolower(rhs)) {
      return false;
    }
  }
  return true;
}

bool extractOpenMPCppPragmaPayload(const std::string &line,
                                   std::string &payload) {
  payload.clear();
  size_t pos = line.find_first_not_of(" \t");
  if (pos == std::string::npos || line[pos] != '#') {
    return false;
  }

  ++pos;
  pos = line.find_first_not_of(" \t", pos);
  if (pos == std::string::npos ||
      !startsWithCaseInsensitiveKeyword(line, pos, "pragma")) {
    return false;
  }
  pos += 6;
  if (pos < line.size() &&
      !std::isspace(static_cast<unsigned char>(line[pos]))) {
    return false;
  }

  pos = line.find_first_not_of(" \t", pos);
  if (pos == std::string::npos ||
      !startsWithCaseInsensitiveKeyword(line, pos, "omp")) {
    return false;
  }

  payload = line.substr(pos);
  return true;
}

std::string getFortranOpenMPDirectiveSourceText(SgPragmaDeclaration *pragma) {
  if (pragma == nullptr) {
    return "";
  }

  if (AstValueAttribute<std::string> *attr =
          dynamic_cast<AstValueAttribute<std::string> *>(
              pragma->getAttribute(kFortranOmpSourceTextAttributeName))) {
    return attr->get();
  }

  if (SgPragma *pragma_text = pragma->get_pragma()) {
    return pragma_text->get_pragma();
  }
  return "";
}

bool endsWithCppLineContinuation(const std::string &line) {
  const size_t end = line.find_last_not_of(" \t\r");
  return end != std::string::npos && line[end] == '\\';
}

std::string stripTrailingCppLineContinuation(const std::string &line) {
  std::string result = line;
  const size_t end = result.find_last_not_of(" \t\r");
  if (end != std::string::npos && result[end] == '\\') {
    result.erase(end);
  }
  return trimWhitespaceCopy(result);
}

std::string getRawOpenMPCppDirectiveText(
    SgPragmaDeclaration *pragma_declaration,
    std::unordered_map<std::string,
                       std::shared_ptr<const std::vector<std::string>>>
        &source_lines_cache,
    std::mutex &source_lines_cache_mutex) {
  if (pragma_declaration == nullptr) {
    return "";
  }

  struct CandidateLocation {
    std::string filename;
    int line = 0;
  };

  std::vector<CandidateLocation> candidates;
  std::vector<int> line_hints;
  auto append_candidate = [&](const std::string &filename, int line) {
    if (filename.empty() || line <= 0) {
      return;
    }
    for (const CandidateLocation &candidate : candidates) {
      if (candidate.filename == filename && candidate.line == line) {
        return;
      }
    }
    candidates.push_back(CandidateLocation{filename, line});
  };
  auto append_line_hint = [&](int line) {
    if (line <= 0) {
      return;
    }
    if (std::find(line_hints.begin(), line_hints.end(), line) !=
        line_hints.end()) {
      return;
    }
    line_hints.push_back(line);
  };
  auto append_info_candidates = [&](const Sg_File_Info *info) {
    if (info == nullptr) {
      return;
    }
    append_line_hint(info->get_physical_line());
    append_line_hint(info->get_line());
    append_candidate(info->get_physical_filename(), info->get_physical_line());
    append_candidate(info->get_filenameString(), info->get_line());
  };

  append_info_candidates(pragma_declaration->get_startOfConstruct());
  append_info_candidates(pragma_declaration->get_file_info());
  if (SgSourceFile *source_file = getEnclosingSourceFile(pragma_declaration)) {
    const std::string source_with_path =
        source_file->get_sourceFileNameWithPath();
    const std::string source_file_name = source_file->getFileName();
    const std::filesystem::path source_dir =
        std::filesystem::path(source_with_path).parent_path();
    for (int line_hint : line_hints) {
      append_candidate(source_with_path, line_hint);
      append_candidate(source_file_name, line_hint);
    }

    // Resolve relative candidate paths against the source file path so pragma
    // lookup does not depend on the current working directory.
    const std::vector<CandidateLocation> original_candidates = candidates;
    for (const CandidateLocation &candidate : original_candidates) {
      append_candidate(source_with_path, candidate.line);
      append_candidate(source_file_name, candidate.line);
      const std::filesystem::path candidate_path(candidate.filename);
      if (!candidate_path.is_absolute() && !source_dir.empty()) {
        append_candidate((source_dir / candidate_path).string(),
                         candidate.line);
      }
    }
  }
  if (candidates.empty()) {
    return "";
  }

  auto build_directive_signature = [](const std::string &payload) {
    std::vector<std::string> tokens;
    std::string current;
    for (char ch : payload) {
      if (std::isalnum(static_cast<unsigned char>(ch)) || ch == '_') {
        current +=
            static_cast<char>(std::tolower(static_cast<unsigned char>(ch)));
      } else if (!current.empty()) {
        tokens.push_back(current);
        current.clear();
      }
    }
    if (!current.empty()) {
      tokens.push_back(current);
    }

    size_t start = 0;
    if (!tokens.empty() && tokens.front() == "omp") {
      start = 1;
    }

    static const std::unordered_set<std::string> omp_keywords = {
        // Directive words.
        "allocate", "assumes", "atomic", "barrier", "begin", "cancel",
        "cancellation", "critical", "data", "declare", "depobj", "distribute",
        "do", "dispatch", "end", "enter", "error", "exit", "flush", "for",
        "interop", "loop", "mapper", "master", "masked", "metadirective",
        "nothing", "ordered", "parallel", "reduction", "requires", "scope",
        "scan", "section", "sections", "simd", "single", "target", "task",
        "taskgroup", "taskloop", "teams", "threadprivate", "tile", "unroll",
        "update", "variant", "workshare",
        // Common clause/modifier words.
        "affinity", "aligned", "allocator", "bind", "capture", "collapse",
        "copyin", "copyprivate", "default", "defaultmap", "depend", "detach",
        "device", "device_type", "dist_schedule", "dist_data", "final",
        "firstprivate", "from", "grainsize", "if", "in_reduction", "inbranch",
        "is_device_ptr", "lastprivate", "linear", "link", "map", "mergeable",
        "nontemporal", "notinbranch", "nowait", "num_tasks", "num_teams",
        "num_threads", "ordered", "partial", "priority", "private", "proc_bind",
        "safelen", "schedule", "seq_cst", "shared", "simdlen", "task_reduction",
        "thread_limit", "to", "tofrom", "uniform", "use_device_addr",
        "use_device_ptr", "uses_allocators", "when", // Extension tokens
        "block", "cyclic", "duplicate"};

    std::string signature;
    for (size_t i = start; i < tokens.size(); ++i) {
      if (omp_keywords.find(tokens[i]) == omp_keywords.end()) {
        continue;
      }
      if (!signature.empty()) {
        signature += " ";
      }
      signature += tokens[i];
    }

    if (signature.empty() && start < tokens.size()) {
      signature = tokens[start];
    }
    return signature;
  };

  const std::string expected_signature =
      build_directive_signature(pragma_declaration->get_pragma()->get_pragma());

  auto get_cached_lines = [&](const std::string &filename)
      -> std::shared_ptr<const std::vector<std::string>> {
    {
      std::lock_guard<std::mutex> lock(source_lines_cache_mutex);
      auto cache_it = source_lines_cache.find(filename);
      if (cache_it != source_lines_cache.end()) {
        return cache_it->second;
      }
    }

    std::ifstream input(filename.c_str());
    if (!input.is_open()) {
      return nullptr;
    }

    std::vector<std::string> lines;
    std::string line;
    while (std::getline(input, line)) {
      if (!line.empty() && line.back() == '\r') {
        line.pop_back();
      }
      lines.push_back(line);
    }
    std::shared_ptr<const std::vector<std::string>> cached_lines =
        std::make_shared<const std::vector<std::string>>(std::move(lines));

    std::lock_guard<std::mutex> lock(source_lines_cache_mutex);
    auto insert_result =
        source_lines_cache.insert(std::make_pair(filename, cached_lines));
    return insert_result.first->second;
  };

  auto extract_directive_text_at = [&](const std::vector<std::string> &lines,
                                       size_t line_index,
                                       std::string &directive_text) -> bool {
    directive_text.clear();
    if (line_index >= lines.size()) {
      return false;
    }

    std::string first_payload;
    if (!extractOpenMPCppPragmaPayload(lines[line_index], first_payload)) {
      return false;
    }

    directive_text = stripTrailingCppLineContinuation(first_payload);
    size_t current_line = line_index;
    while (current_line < lines.size() &&
           endsWithCppLineContinuation(lines[current_line])) {
      ++current_line;
      if (current_line >= lines.size()) {
        break;
      }

      std::string continuation = lines[current_line];
      const size_t continuation_start = continuation.find_first_not_of(" \t");
      if (continuation_start != std::string::npos) {
        continuation.erase(0, continuation_start);
      } else {
        continuation.clear();
      }
      continuation = stripTrailingCppLineContinuation(continuation);
      if (!continuation.empty()) {
        directive_text += " " + continuation;
      }
    }

    directive_text = trimWhitespaceCopy(directive_text);
    return !directive_text.empty();
  };

  for (const CandidateLocation &candidate : candidates) {
    const std::shared_ptr<const std::vector<std::string>> lines =
        get_cached_lines(candidate.filename);
    if (!lines || lines->empty()) {
      continue;
    }

    const size_t line_index = static_cast<size_t>(candidate.line - 1);
    std::string directive_text;
    if (extract_directive_text_at(*lines, line_index, directive_text)) {
      // Prefer exact source location over keyword-signature matching. The
      // normalized pragma text stored in the AST can legitimately drop
      // extension-only fragments (e.g. map dist_data policy details), so
      // strict signature equality here can miss the real source directive.
      return directive_text;
    }

    static const int kLineSearchRadius = 64;
    for (int delta = 1; delta <= kLineSearchRadius; ++delta) {
      if (candidate.line - delta > 0) {
        const size_t before_index =
            static_cast<size_t>(candidate.line - delta - 1);
        if (extract_directive_text_at(*lines, before_index, directive_text)) {
          if (expected_signature.empty() ||
              build_directive_signature(directive_text) == expected_signature) {
            return directive_text;
          }
        }
      }

      const size_t after_index =
          static_cast<size_t>(candidate.line + delta - 1);
      if (extract_directive_text_at(*lines, after_index, directive_text)) {
        if (expected_signature.empty() ||
            build_directive_signature(directive_text) == expected_signature) {
          return directive_text;
        }
      }
    }

    if (!expected_signature.empty()) {
      int best_distance = std::numeric_limits<int>::max();
      std::string best_directive_text;
      for (size_t i = 0; i < lines->size(); ++i) {
        if (!extract_directive_text_at(*lines, i, directive_text)) {
          continue;
        }
        if (build_directive_signature(directive_text) != expected_signature) {
          continue;
        }

        const int directive_line = static_cast<int>(i + 1);
        const int distance = std::abs(directive_line - candidate.line);
        if (distance < best_distance) {
          best_distance = distance;
          best_directive_text = directive_text;
        }
      }
      if (!best_directive_text.empty()) {
        return best_directive_text;
      }
    }
  }

  return "";
}

void initializeGeneratedOpenMPStatement(SgStatement *statement) {
  if (statement == nullptr) {
    return;
  }

  if (SgLocatedNode *located = isSgLocatedNode(statement)) {
    if (located->get_file_info() == nullptr ||
        located->get_startOfConstruct() == nullptr ||
        located->get_endOfConstruct() == nullptr) {
      setSourcePositionAsTransformation(located);
    }
    if (located->get_file_info() != nullptr) {
      located->get_file_info()->setTransformation();
    }
    if (located->get_startOfConstruct() != nullptr) {
      located->get_startOfConstruct()->setTransformation();
    }
    if (located->get_endOfConstruct() != nullptr) {
      located->get_endOfConstruct()->setTransformation();
    }
    located->setTransformation();
    located->setOutputInCodeGeneration();
  }

  if (SgOmpBodyStatement *omp_body = isSgOmpBodyStatement(statement)) {
    SgStatement *body = omp_body->get_body();
    SgLocatedNode *located_body = isSgLocatedNode(body);
    if (located_body != nullptr &&
        (located_body->get_file_info() == nullptr ||
         located_body->get_startOfConstruct() == nullptr ||
         located_body->get_endOfConstruct() == nullptr)) {
      initializeGeneratedOpenMPStatement(body);
    }
  }
}

void collectCommentedDirectiveRelocations(
    SgSourceFile *source_file,
    const std::list<SgPragmaDeclaration *> &pragma_list) {
  g_pending_commented_directive_relocations.clear();
  if (source_file == nullptr) {
    return;
  }

  struct StatementPosition {
    SgStatement *statement = nullptr;
    unsigned line = 0;
  };

  std::unordered_set<SgPragmaDeclaration *> pragma_set;
  std::unordered_map<int, std::vector<StatementPosition>>
      statement_positions_by_file;
  std::unordered_map<std::string, std::vector<StatementPosition>>
      statement_positions_by_filename;

  for (SgPragmaDeclaration *pragma_decl : pragma_list) {
    if (pragma_decl == nullptr) {
      continue;
    }
    pragma_set.insert(pragma_decl);
  }

  std::vector<SgNode *> statements =
      NodeQuery::querySubTree(source_file, V_SgStatement);
  for (SgNode *node : statements) {
    SgStatement *stmt = isSgStatement(node);
    if (stmt == nullptr) {
      continue;
    }
    const Sg_File_Info *file_info = stmt->get_file_info();
    if (file_info == nullptr) {
      continue;
    }

    const unsigned stmt_line = getLocatedNodeLine(isSgLocatedNode(stmt));
    if (stmt_line == 0) {
      continue;
    }

    const int file_id = file_info->get_file_id();
    if (file_id <= 0) {
      int physical_file_id = file_info->get_physical_file_id();
      if (physical_file_id > 0) {
        statement_positions_by_file[physical_file_id].push_back(
            StatementPosition{stmt, stmt_line});
      }
    } else {
      statement_positions_by_file[file_id].push_back(
          StatementPosition{stmt, stmt_line});
    }

    const std::string filename = file_info->get_filenameString();
    if (!filename.empty()) {
      statement_positions_by_filename[filename].push_back(
          StatementPosition{stmt, stmt_line});
    }
  }

  for (auto &entry : statement_positions_by_file) {
    std::vector<StatementPosition> &positions = entry.second;
    std::sort(positions.begin(), positions.end(),
              [](const StatementPosition &lhs, const StatementPosition &rhs) {
                return lhs.line < rhs.line;
              });
  }
  for (auto &entry : statement_positions_by_filename) {
    std::vector<StatementPosition> &positions = entry.second;
    std::sort(positions.begin(), positions.end(),
              [](const StatementPosition &lhs, const StatementPosition &rhs) {
                return lhs.line < rhs.line;
              });
  }

  if (statement_positions_by_file.empty() &&
      statement_positions_by_filename.empty()) {
    return;
  }

  auto detach_from_owner = [](SgLocatedNode *owner, PreprocessingInfo *info) {
    if (owner == nullptr || info == nullptr) {
      return;
    }
    AttachedPreprocessingInfoType *owner_info =
        owner->get_attachedPreprocessingInfoPtr();
    if (owner_info == nullptr) {
      return;
    }
    auto owner_pos = std::find(owner_info->begin(), owner_info->end(), info);
    if (owner_pos != owner_info->end()) {
      owner_info->erase(owner_pos);
    }
  };

  auto attach_to_target = [](SgStatement *target, PreprocessingInfo *info,
                             bool attach_before) {
    if (target == nullptr || info == nullptr) {
      return;
    }

    AttachedPreprocessingInfoType *target_info =
        target->get_attachedPreprocessingInfoPtr();
    if (target_info == nullptr) {
      target_info = new AttachedPreprocessingInfoType;
      target->set_attachedPreprocessingInfoPtr(target_info);
    }

    if (std::find(target_info->begin(), target_info->end(), info) !=
        target_info->end()) {
      return;
    }

    info->setRelativePosition(attach_before ? PreprocessingInfo::before
                                            : PreprocessingInfo::after);
    if (attach_before) {
      auto insert_after_existing_before = std::find_if(
          target_info->begin(), target_info->end(),
          [](PreprocessingInfo *current) {
            return current->getRelativePosition() != PreprocessingInfo::before;
          });
      target_info->insert(insert_after_existing_before, info);
    } else {
      target_info->push_back(info);
    }
  };

  std::unordered_set<PreprocessingInfo *> seen_comments;
  std::vector<SgNode *> located_nodes =
      NodeQuery::querySubTree(source_file, V_SgLocatedNode);
  for (SgNode *node : located_nodes) {
    SgLocatedNode *owner = isSgLocatedNode(node);
    if (owner == nullptr) {
      continue;
    }

    AttachedPreprocessingInfoType *attached =
        owner->get_attachedPreprocessingInfoPtr();
    if (attached == nullptr || attached->empty()) {
      continue;
    }

    AttachedPreprocessingInfoType attached_copy = *attached;
    for (PreprocessingInfo *info : attached_copy) {
      if (!isCommentedOutDirective(info)) {
        continue;
      }
      if (!seen_comments.insert(info).second) {
        continue;
      }

      int comment_line = info->getLineNumber();
      int file_id = info->getFileId();
      std::string comment_filename;

      if (comment_line <= 0) {
        comment_line = static_cast<int>(getLocatedNodeLine(owner));
      }
      if (Sg_File_Info *comment_file_info = info->get_file_info()) {
        if (file_id <= 0) {
          file_id = comment_file_info->get_file_id();
          if (file_id <= 0) {
            int physical_file_id = comment_file_info->get_physical_file_id();
            if (physical_file_id > 0) {
              file_id = physical_file_id;
            }
          }
        }
        comment_filename = comment_file_info->get_filenameString();
      }
      if (file_id <= 0) {
        if (const Sg_File_Info *owner_info = owner->get_file_info()) {
          file_id = owner_info->get_file_id();
          if (file_id <= 0) {
            int physical_file_id = owner_info->get_physical_file_id();
            if (physical_file_id > 0) {
              file_id = physical_file_id;
            }
          }
          if (comment_filename.empty()) {
            comment_filename = owner_info->get_filenameString();
          }
        }
      }
      if (file_id <= 0) {
        if (const Sg_File_Info *source_info = source_file->get_file_info()) {
          file_id = source_info->get_file_id();
          if (file_id <= 0) {
            int physical_file_id = source_info->get_physical_file_id();
            if (physical_file_id > 0) {
              file_id = physical_file_id;
            }
          }
          if (comment_filename.empty()) {
            comment_filename = source_info->get_filenameString();
          }
        }
      }
      if (comment_line <= 0) {
        continue;
      }

      const std::vector<StatementPosition> *positions = nullptr;
      if (!comment_filename.empty()) {
        auto by_name_it =
            statement_positions_by_filename.find(comment_filename);
        if (by_name_it != statement_positions_by_filename.end()) {
          positions = &by_name_it->second;
        }
      }
      if (positions == nullptr && file_id > 0) {
        auto by_id_it = statement_positions_by_file.find(file_id);
        if (by_id_it != statement_positions_by_file.end()) {
          positions = &by_id_it->second;
        }
      }
      if (positions == nullptr && statement_positions_by_filename.size() == 1) {
        positions = &statement_positions_by_filename.begin()->second;
      }
      if (positions == nullptr && statement_positions_by_file.size() == 1) {
        positions = &statement_positions_by_file.begin()->second;
      }
      if (positions == nullptr) {
        continue;
      }
      if (positions->empty()) {
        continue;
      }

      auto next_statement_it =
          std::lower_bound(positions->begin(), positions->end(), comment_line,
                           [](const StatementPosition &position, int line) {
                             return static_cast<int>(position.line) < line;
                           });
      SgStatement *target_stmt = nullptr;
      int target_stmt_line = 0;
      if (next_statement_it != positions->end()) {
        target_stmt = next_statement_it->statement;
        target_stmt_line = static_cast<int>(next_statement_it->line);
      } else {
        const StatementPosition &last_position = positions->back();
        target_stmt = last_position.statement;
        target_stmt_line = static_cast<int>(last_position.line);
      }
      if (target_stmt == nullptr) {
        continue;
      }

      const bool appears_before_target =
          target_stmt_line == 0 || comment_line <= target_stmt_line;
      if (SgPragmaDeclaration *target_pragma =
              isSgPragmaDeclaration(target_stmt)) {
        if (pragma_set.find(target_pragma) != pragma_set.end()) {
          g_pending_commented_directive_relocations[target_pragma].push_back(
              PendingCommentedDirectiveRelocation{owner, info, comment_line});
          continue;
        }
      }

      if (isSgLocatedNode(target_stmt) != owner) {
        detach_from_owner(owner, info);
      }
      attach_to_target(target_stmt, info, appears_before_target);
    }
  }
}

void relocatePendingCommentedDirectivesForPragma(
    SgPragmaDeclaration *pragma_decl, SgStatement *directive_stmt) {
  if (pragma_decl == nullptr || directive_stmt == nullptr) {
    return;
  }

  auto found = g_pending_commented_directive_relocations.find(pragma_decl);
  if (found == g_pending_commented_directive_relocations.end()) {
    return;
  }

  std::vector<PendingCommentedDirectiveRelocation> pending =
      std::move(found->second);
  g_pending_commented_directive_relocations.erase(found);
  if (pending.empty()) {
    return;
  }

  const unsigned pragma_line = getLocatedNodeLine(pragma_decl);
  std::stable_sort(pending.begin(), pending.end(),
                   [](const PendingCommentedDirectiveRelocation &lhs,
                      const PendingCommentedDirectiveRelocation &rhs) {
                     return lhs.source_line < rhs.source_line;
                   });

  AttachedPreprocessingInfoType *directive_info =
      directive_stmt->get_attachedPreprocessingInfoPtr();
  if (directive_info == nullptr) {
    directive_info = new AttachedPreprocessingInfoType;
    directive_stmt->set_attachedPreprocessingInfoPtr(directive_info);
  }

  for (const PendingCommentedDirectiveRelocation &entry : pending) {
    if (entry.info == nullptr) {
      continue;
    }

    if (entry.owner != nullptr) {
      AttachedPreprocessingInfoType *owner_info =
          entry.owner->get_attachedPreprocessingInfoPtr();
      if (owner_info != nullptr) {
        auto owner_pos =
            std::find(owner_info->begin(), owner_info->end(), entry.info);
        if (owner_pos != owner_info->end()) {
          owner_info->erase(owner_pos);
        }
      }
    }

    if (std::find(directive_info->begin(), directive_info->end(), entry.info) !=
        directive_info->end()) {
      continue;
    }

    const bool appears_before_pragma =
        pragma_line == 0 ||
        (entry.source_line > 0 &&
         entry.source_line <= static_cast<int>(pragma_line));
    if (appears_before_pragma) {
      entry.info->setRelativePosition(PreprocessingInfo::before);
      auto insert_after_existing_before = std::find_if(
          directive_info->begin(), directive_info->end(),
          [](PreprocessingInfo *current) {
            return current->getRelativePosition() != PreprocessingInfo::before;
          });
      directive_info->insert(insert_after_existing_before, entry.info);
    } else {
      entry.info->setRelativePosition(PreprocessingInfo::after);
      directive_info->push_back(entry.info);
    }
  }
}

bool shouldParseDeviceExprAsVerbatim(const std::string &expression_text) {
  const std::string trimmed = trimWhitespaceCopy(expression_text);
  if (trimmed.empty()) {
    return false;
  }
  if (trimmed == "*") {
    return true;
  }
  if (trimmed.find('"') != std::string::npos ||
      trimmed.find('\'') != std::string::npos) {
    return true;
  }
  if (trimmed.find(':') != std::string::npos &&
      trimmed.find('?') == std::string::npos) {
    return true;
  }
  return false;
}

SgExpression *buildOmpVarExprFromNode(SgNode *node) {
  if (SgInitializedName *iname = isSgInitializedName(node)) {
    return SageBuilder::buildVarRefExp(iname);
  }
  if (SgExpression *expr = isSgExpression(node)) {
    return expr;
  }
  return nullptr;
}

void clearOpenMPClauseTemporaryState() {
  omp_variable_list.clear();
  array_dimensions.clear();
}

const OmpParsedExpression *asParsedExpression(const void *node) {
  return static_cast<const OmpParsedExpression *>(node);
}

SgExpression *cloneParsedExpressionNode(const OmpParsedExpression *parsed) {
  if (parsed == nullptr) {
    return nullptr;
  }
  if (parsed->node == nullptr) {
    return nullptr;
  }

  if (SgInitializedName *iname = isSgInitializedName(parsed->node)) {
    return SageBuilder::buildVarRefExp(iname);
  }

  if (SgExpression *expr = isSgExpression(parsed->node)) {
    if (SgVarRefExp *var_ref = isSgVarRefExp(expr)) {
      if (SgVariableSymbol *symbol = var_ref->get_symbol()) {
        SgVarRefExp *clone = SageBuilder::buildVarRefExp(symbol);
        if (SgExpression *original_tree =
                var_ref->get_originalExpressionTree()) {
          clone->set_originalExpressionTree(
              SageInterface::copyExpression(original_tree));
        }
        return clone;
      }
      return SageInterface::copyExpression(var_ref);
    }
    return SageInterface::copyExpression(expr);
  }

  return nullptr;
}

void parseAndStoreVariableList(
    const std::string &expr_text, OmpParsedExpression *parsed,
    SgPragmaDeclaration *pragma_declaration, OpenMPDirective *directive,
    OpenMPClauseKind clause_kind,
    const std::string *directive_source_text = nullptr) {
  ROSE_ASSERT(parsed != nullptr);
  clearOpenMPClauseTemporaryState();
  parseOmpVariable(std::make_pair(pragma_declaration, directive), clause_kind,
                   expr_text, directive_source_text);
  ROSE_ASSERT(!omp_variable_list.empty());
  parsed->node = omp_variable_list.back().second;
  parsed->dimensions.clear();
  omp_variable_list.clear();
  array_dimensions.clear();
}

void parseAndStoreArraySection(
    const std::string &expr_text, OmpParsedExpression *parsed,
    SgPragmaDeclaration *pragma_declaration, OpenMPDirective *directive,
    OpenMPClauseKind clause_kind,
    const std::string *directive_source_text = nullptr) {
  ROSE_ASSERT(parsed != nullptr);
  clearOpenMPClauseTemporaryState();
  parseOmpArraySection(pragma_declaration, clause_kind, expr_text,
                       directive_source_text);
  ROSE_ASSERT(!omp_variable_list.empty());
  parsed->node = omp_variable_list.back().second;
  parsed->dimensions.clear();
  if (SgVariableSymbol *symbol =
          extractClauseVariableSymbol(omp_variable_list.back().second)) {
    auto found = array_dimensions.find(symbol);
    if (found != array_dimensions.end()) {
      parsed->dimensions = found->second;
    }
  }
  if (parsed->dimensions.empty()) {
    collectArraySectionDimensions(isSgExpression(parsed->node),
                                  parsed->dimensions);
  }
  omp_variable_list.clear();
  array_dimensions.clear();
}

void parseAndStoreExpression(
    const std::string &expr_text, OmpParsedExpression *parsed,
    SgPragmaDeclaration *pragma_declaration, OpenMPDirective *directive,
    OpenMPClauseKind clause_kind,
    const std::string *directive_source_text = nullptr) {
  ROSE_ASSERT(parsed != nullptr);
  clearOpenMPClauseTemporaryState();
  SgExpression *expression = parseOmpExpression(
      pragma_declaration, clause_kind, expr_text, directive_source_text);
  ROSE_ASSERT(expression != nullptr);
  parsed->node = expression;
  parsed->dimensions.clear();
  if (SgVariableSymbol *symbol = extractClauseVariableSymbol(expression)) {
    auto found = array_dimensions.find(symbol);
    if (found != array_dimensions.end()) {
      parsed->dimensions = found->second;
    }
  }
  if (parsed->dimensions.empty()) {
    collectArraySectionDimensions(expression, parsed->dimensions);
  }
  omp_variable_list.clear();
  array_dimensions.clear();
}

void *parseOpenMPExprCallback(OpenMPDirectiveKind directive_kind,
                              OpenMPClauseKind clause_kind,
                              OpenMPExprParseMode parse_mode,
                              const char *expression, void *user_data) {
  OmpExprParseContext *context = static_cast<OmpExprParseContext *>(user_data);
  ROSE_ASSERT(context != nullptr);
  ROSE_ASSERT(context->pragma_declaration != nullptr);
  ROSE_ASSERT(context->directive != nullptr);
  if (expression == nullptr) {
    return nullptr;
  }

  auto parsed = std::make_unique<OmpParsedExpression>();
  parsed->mode = parse_mode;
  parsed->text = expression;

  (void)directive_kind;

  if (parse_mode == OMP_EXPR_PARSE_expression) {
    if (clause_kind == OMPC_device &&
        shouldParseDeviceExprAsVerbatim(parsed->text)) {
      parsed->node = buildOpaqueOpenMPClauseExpression(
          context->pragma_declaration, trimWhitespaceCopy(parsed->text));
    } else {
      parseAndStoreExpression(parsed->text, parsed.get(),
                              context->pragma_declaration, context->directive,
                              clause_kind, context->directive_source_text);
    }
  } else if (parse_mode == OMP_EXPR_PARSE_variable_list) {
    parseAndStoreVariableList(parsed->text, parsed.get(),
                              context->pragma_declaration, context->directive,
                              clause_kind, context->directive_source_text);
  } else if (parse_mode == OMP_EXPR_PARSE_array_section) {
    parseAndStoreArraySection(parsed->text, parsed.get(),
                              context->pragma_declaration, context->directive,
                              clause_kind, context->directive_source_text);
  } else if (parse_mode == OMP_EXPR_PARSE_verbatim) {
    parsed->node = buildOpaqueOpenMPClauseExpression(
        context->pragma_declaration, parsed->text);
  }

  OmpParsedExpression *raw = parsed.get();
  context->owned_nodes.push_back(std::move(parsed));
  return raw;
}

OmpClauseParseCache
parseClauseNodesForDirective(SgPragmaDeclaration *pragma_declaration,
                             OpenMPDirective *directive,
                             const std::string &directive_text) {
  OmpClauseParseCache parsed_cache;
  if (pragma_declaration == nullptr || directive == nullptr ||
      directive_text.empty()) {
    return parsed_cache;
  }

  std::string parse_text = directive_text;
  if (directive->getBaseLang() == Lang_Fortran) {
    const std::string trimmed = trimWhitespaceCopy(parse_text);
    std::string lowered = trimmed;
    std::transform(
        lowered.begin(), lowered.end(), lowered.begin(),
        [](unsigned char ch) { return static_cast<char>(std::tolower(ch)); });

    if (lowered.rfind("#pragma", 0) == 0) {
      const std::string::size_type omp_pos = lowered.find("omp");
      ROSE_ASSERT(omp_pos != std::string::npos);
      parse_text = "!$" + trimmed.substr(omp_pos);
    } else if (lowered.rfind("!$omp", 0) != 0 &&
               lowered.rfind("c$omp", 0) != 0 &&
               lowered.rfind("*$omp", 0) != 0) {
      if (lowered.rfind("omp", 0) == 0) {
        parse_text = "!$" + trimmed;
      } else {
        parse_text = "!$omp " + trimmed;
      }
    } else {
      parse_text = trimmed;
    }
  }

  std::vector<OpenMPClause *> *original_clauses =
      directive->getClausesInOriginalOrder();
  ROSE_ASSERT(original_clauses != nullptr);

  // Fortran clause expressions are already carried as source-faithful text in
  // the frontend IR. Reparsing into this cache can mis-associate expressions
  // across adjacent directives and corrupt later lowering (e.g., if/num_threads
  // clauses becoming runtime helper names). Keep the C/C++ cache behavior, but
  // bypass this cache path for Fortran.
  if (directive->getBaseLang() == Lang_Fortran) {
    return parsed_cache;
  }

  bool requires_expression_cache = false;
  bool requires_strict_expression_cache = false;
  bool skip_reparse_for_merged_fortran_single = false;
  for (OpenMPClause *clause : *original_clauses) {
    if (clause == nullptr) {
      continue;
    }
    if (!clause->getExpressions()->empty()) {
      requires_expression_cache = true;
    }
    if (clause->getKind() == OMPC_to) {
      auto *to_clause = static_cast<OpenMPToClause *>(clause);
      if (!to_clause->getMapperIdentifier().empty()) {
        requires_expression_cache = true;
        requires_strict_expression_cache = true;
      }
    } else if (clause->getKind() == OMPC_from) {
      auto *from_clause = static_cast<OpenMPFromClause *>(clause);
      if (!from_clause->getMapperIdentifier().empty()) {
        requires_expression_cache = true;
        requires_strict_expression_cache = true;
      }
    } else if (clause->getKind() == OMPC_allocate) {
      auto *allocate_clause = static_cast<OpenMPAllocateClause *>(clause);
      if (!allocate_clause->getUserDefinedAllocator().empty()) {
        requires_expression_cache = true;
        requires_strict_expression_cache = true;
      }
    } else if (directive->getBaseLang() == Lang_Fortran &&
               directive->getKind() == OMPD_single &&
               clause->getKind() == OMPC_copyprivate) {
      skip_reparse_for_merged_fortran_single = true;
    }
  }

  // Best-effort cache for Fortran: do not require strict reparse success.
  if (directive->getBaseLang() == Lang_Fortran) {
    requires_strict_expression_cache = false;
  }

  if (!requires_expression_cache) {
    return parsed_cache;
  }

  if (skip_reparse_for_merged_fortran_single &&
      !requires_strict_expression_cache) {
    return parsed_cache;
  }

  OmpExprParseContext context;
  context.pragma_declaration = pragma_declaration;
  context.directive = directive;
  context.directive_source_text = &directive_text;
  OpenMPDirective *parsed_directive =
      parseOpenMP(parse_text.c_str(), parseOpenMPExprCallback, &context);
  if (parsed_directive == nullptr) {
    if (!requires_strict_expression_cache) {
      return parsed_cache;
    }
    MLOG_ERROR_C("ompAstConstruction",
                 "Failed to reparse OpenMP directive text for cache: %s\n",
                 parse_text.c_str());
    ROSE_ABORT();
  }
  ROSE_ASSERT(parsed_directive->getKind() == directive->getKind());

  parsed_cache.owned_nodes = std::move(context.owned_nodes);

  std::vector<OpenMPClause *> *parsed_clauses =
      parsed_directive->getClausesInOriginalOrder();
  ROSE_ASSERT(parsed_clauses != nullptr);
  if (original_clauses->size() != parsed_clauses->size()) {
    delete parsed_directive;
    if (!requires_strict_expression_cache) {
      return OmpClauseParseCache();
    }
    MLOG_ERROR_C("ompAstConstruction",
                 "OpenMP reparse produced mismatched clause counts: "
                 "original=%zu parsed=%zu text=%s\n",
                 original_clauses->size(), parsed_clauses->size(),
                 parse_text.c_str());
    ROSE_ABORT();
  }

  for (size_t index = 0; index < original_clauses->size(); ++index) {
    OpenMPClause *original_clause = (*original_clauses)[index];
    OpenMPClause *parsed_clause = (*parsed_clauses)[index];
    ROSE_ASSERT(original_clause != nullptr);
    ROSE_ASSERT(parsed_clause != nullptr);
    if (original_clause->getKind() != parsed_clause->getKind()) {
      delete parsed_directive;
      if (!requires_strict_expression_cache) {
        return OmpClauseParseCache();
      }
      MLOG_ERROR_C("ompAstConstruction",
                   "OpenMP reparse produced mismatched clause kind: "
                   "original=%d parsed=%d text=%s\n",
                   static_cast<int>(original_clause->getKind()),
                   static_cast<int>(parsed_clause->getKind()),
                   parse_text.c_str());
      ROSE_ABORT();
    }

    std::vector<const OmpParsedExpression *> clause_nodes;
    const std::vector<const void *> &raw_nodes =
        parsed_clause->getExpressionNodes();
    clause_nodes.reserve(raw_nodes.size() + 1);
    for (const void *raw_node : raw_nodes) {
      if (const OmpParsedExpression *parsed = asParsedExpression(raw_node)) {
        clause_nodes.push_back(parsed);
      }
    }

    if (original_clause->getKind() == OMPC_to) {
      auto *original_to_clause = static_cast<OpenMPToClause *>(original_clause);
      auto *parsed_to_clause = static_cast<OpenMPToClause *>(parsed_clause);
      if (!original_to_clause->getMapperIdentifier().empty()) {
        const OmpParsedExpression *mapper_node =
            asParsedExpression(parsed_to_clause->getMapperIdentifierNode());
        if (mapper_node != nullptr) {
          clause_nodes.push_back(mapper_node);
        }
      }
    } else if (original_clause->getKind() == OMPC_from) {
      auto *original_from_clause =
          static_cast<OpenMPFromClause *>(original_clause);
      auto *parsed_from_clause = static_cast<OpenMPFromClause *>(parsed_clause);
      if (!original_from_clause->getMapperIdentifier().empty()) {
        const OmpParsedExpression *mapper_node =
            asParsedExpression(parsed_from_clause->getMapperIdentifierNode());
        if (mapper_node != nullptr) {
          clause_nodes.push_back(mapper_node);
        }
      }
    } else if (original_clause->getKind() == OMPC_allocate) {
      auto *original_allocate_clause =
          static_cast<OpenMPAllocateClause *>(original_clause);
      auto *parsed_allocate_clause =
          static_cast<OpenMPAllocateClause *>(parsed_clause);
      if (!original_allocate_clause->getUserDefinedAllocator().empty()) {
        const OmpParsedExpression *allocator_node = asParsedExpression(
            parsed_allocate_clause->getUserDefinedAllocatorNode());
        if (allocator_node != nullptr) {
          clause_nodes.push_back(allocator_node);
        }
      }
    }

    parsed_cache.clause_expression_nodes[original_clause] =
        std::move(clause_nodes);
    if (original_clause->getClausePosition() >= 0) {
      parsed_cache.clause_expression_nodes_by_position
          [original_clause->getClausePosition()] =
          parsed_cache.clause_expression_nodes[original_clause];
    }

    if (original_clause->getKind() == OMPC_map) {
      auto *parsed_map_clause = static_cast<OpenMPMapClause *>(parsed_clause);
      const auto &dist_data_policies = parsed_map_clause->getDistDataPolicies();
      parsed_cache.map_dist_data_policies[original_clause] = dist_data_policies;
      if (original_clause->getClausePosition() >= 0) {
        parsed_cache.map_dist_data_policies_by_position
            [original_clause->getClausePosition()] =
            parsed_cache.map_dist_data_policies[original_clause];
      }
      std::vector<std::vector<const OmpParsedExpression *>> policy_nodes;
      policy_nodes.reserve(dist_data_policies.size());
      for (const auto &policies_for_item : dist_data_policies) {
        std::vector<const OmpParsedExpression *> item_nodes;
        item_nodes.reserve(policies_for_item.size());
        for (const auto &policy : policies_for_item) {
          item_nodes.push_back(asParsedExpression(policy.argument_node));
        }
        policy_nodes.push_back(std::move(item_nodes));
      }
      parsed_cache.map_dist_data_policy_nodes[original_clause] =
          std::move(policy_nodes);
      if (original_clause->getClausePosition() >= 0) {
        parsed_cache.map_dist_data_policy_nodes_by_position
            [original_clause->getClausePosition()] =
            parsed_cache.map_dist_data_policy_nodes[original_clause];
      }
    }
  }

  delete parsed_directive;
  return parsed_cache;
}

const OmpClauseParseCache *getClauseParseCache(OpenMPDirective *directive) {
  auto found = g_omp_clause_nodes.find(directive);
  if (found == g_omp_clause_nodes.end()) {
    return nullptr;
  }
  return &found->second;
}

const std::vector<const OmpParsedExpression *> *
getParsedClauseExpressionNodes(OpenMPDirective *directive,
                               const OpenMPClause *clause) {
  const OmpClauseParseCache *cache = getClauseParseCache(directive);
  if (cache == nullptr || clause == nullptr) {
    return nullptr;
  }
  auto found = cache->clause_expression_nodes.find(clause);
  if (found != cache->clause_expression_nodes.end()) {
    return &found->second;
  }

  OpenMPClause *mutable_clause = const_cast<OpenMPClause *>(clause);
  if (mutable_clause != nullptr && mutable_clause->getClausePosition() >= 0) {
    auto by_position = cache->clause_expression_nodes_by_position.find(
        mutable_clause->getClausePosition());
    if (by_position != cache->clause_expression_nodes_by_position.end()) {
      return &by_position->second;
    }
  }
  return nullptr;
}

const std::vector<std::vector<const OmpParsedExpression *>> *
getParsedMapDistDataPolicyNodes(OpenMPDirective *directive,
                                const OpenMPClause *clause) {
  const OmpClauseParseCache *cache = getClauseParseCache(directive);
  if (cache == nullptr || clause == nullptr) {
    return nullptr;
  }
  auto found = cache->map_dist_data_policy_nodes.find(clause);
  if (found != cache->map_dist_data_policy_nodes.end()) {
    return &found->second;
  }

  OpenMPClause *mutable_clause = const_cast<OpenMPClause *>(clause);
  if (mutable_clause != nullptr && mutable_clause->getClausePosition() >= 0) {
    auto by_position = cache->map_dist_data_policy_nodes_by_position.find(
        mutable_clause->getClausePosition());
    if (by_position != cache->map_dist_data_policy_nodes_by_position.end()) {
      return &by_position->second;
    }
  }
  return nullptr;
}

const std::vector<std::vector<OpenMPMapClause::DistDataPolicy>> *
getParsedMapDistDataPolicies(OpenMPDirective *directive,
                             const OpenMPClause *clause) {
  const OmpClauseParseCache *cache = getClauseParseCache(directive);
  if (cache == nullptr || clause == nullptr) {
    return nullptr;
  }
  auto found = cache->map_dist_data_policies.find(clause);
  if (found != cache->map_dist_data_policies.end()) {
    return &found->second;
  }

  OpenMPClause *mutable_clause = const_cast<OpenMPClause *>(clause);
  if (mutable_clause != nullptr && mutable_clause->getClausePosition() >= 0) {
    auto by_position = cache->map_dist_data_policies_by_position.find(
        mutable_clause->getClausePosition());
    if (by_position != cache->map_dist_data_policies_by_position.end()) {
      return &by_position->second;
    }
  }
  return nullptr;
}

SgOmpClause::omp_map_dist_data_enum
toSgMapDistDataPolicy(OpenMPMapClause::DistDataPolicyKind policy_kind) {
  switch (policy_kind) {
  case OpenMPMapClause::DIST_DATA_duplicate:
    return SgOmpClause::e_omp_map_dist_data_duplicate;
  case OpenMPMapClause::DIST_DATA_block:
    return SgOmpClause::e_omp_map_dist_data_block;
  case OpenMPMapClause::DIST_DATA_cyclic:
    return SgOmpClause::e_omp_map_dist_data_cyclic;
  }
  MLOG_ERROR_C("ompAstConstruction",
               "Unsupported dist_data policy kind in map clause\n");
  ROSE_ABORT();
}

void appendParsedVariableNode(const OmpParsedExpression *parsed) {
  ROSE_ASSERT(parsed != nullptr);
  ROSE_ASSERT(parsed->node != nullptr);
  SgNode *node_for_clause = parsed->node;

  if (!parsed->dimensions.empty()) {
    if (SgVariableSymbol *symbol =
            extractDirectArraySectionSymbol(parsed->node)) {
      // Keep map/depend/to/from array-section metadata in array_dimensions by
      // canonicalizing direct array sections (e.g. a[0:n]) to a variable item.
      array_dimensions[symbol] = parsed->dimensions;
      node_for_clause = SageBuilder::buildVarRefExp(symbol);
    } else if (isSgInitializedName(parsed->node) != nullptr ||
               isSgVarRefExp(parsed->node) != nullptr) {
      // Dimensions keyed by symbol are only safe for direct variable items.
      if (SgVariableSymbol *symbol =
              extractClauseVariableSymbol(parsed->node)) {
        array_dimensions[symbol] = parsed->dimensions;
      }
    }
  }

  omp_variable_list.push_back(std::make_pair(parsed->text, node_for_clause));
}

SgExpression *cloneParsedExpressionNodeByText(
    const std::vector<const OmpParsedExpression *> *parsed_nodes,
    const std::string &expression_text,
    OpenMPExprParseMode preferred_mode = OMP_EXPR_PARSE_none) {
  const OmpParsedExpression *parsed =
      findParsedExpressionByText(parsed_nodes, expression_text, preferred_mode);
  if (parsed == nullptr && preferred_mode != OMP_EXPR_PARSE_none) {
    parsed = findParsedExpressionByText(parsed_nodes, expression_text,
                                        OMP_EXPR_PARSE_none);
  }
  if (parsed == nullptr) {
    return nullptr;
  }
  return cloneParsedExpressionNode(parsed);
}

SgExpression *cloneParsedExpressionNodeByApproxText(
    const std::vector<const OmpParsedExpression *> *parsed_nodes,
    const std::string &expression_text,
    OpenMPExprParseMode preferred_mode = OMP_EXPR_PARSE_none) {
  const std::string trimmed = trimWhitespaceCopy(expression_text);
  if (trimmed.empty()) {
    return nullptr;
  }

  if (SgExpression *expr = cloneParsedExpressionNodeByText(
          parsed_nodes, trimmed, preferred_mode)) {
    return expr;
  }

  const bool quoted = trimmed.size() >= 2 &&
                      ((trimmed.front() == '"' && trimmed.back() == '"') ||
                       (trimmed.front() == '\'' && trimmed.back() == '\''));
  if (quoted) {
    const std::string unquoted = trimmed.substr(1, trimmed.size() - 2);
    return cloneParsedExpressionNodeByText(parsed_nodes, unquoted,
                                           preferred_mode);
  }

  return cloneParsedExpressionNodeByText(parsed_nodes, "\"" + trimmed + "\"",
                                         preferred_mode);
}

const OmpParsedExpression *findParsedExpressionByText(
    const std::vector<const OmpParsedExpression *> *parsed_nodes,
    const std::string &expression_text,
    OpenMPExprParseMode required_mode = OMP_EXPR_PARSE_none) {
  if (parsed_nodes == nullptr) {
    return nullptr;
  }

  for (const OmpParsedExpression *parsed : *parsed_nodes) {
    if (parsed == nullptr) {
      continue;
    }
    if (required_mode != OMP_EXPR_PARSE_none && parsed->mode != required_mode) {
      continue;
    }
    if (parsed->text == expression_text) {
      return parsed;
    }
  }

  return nullptr;
}

SgExpression *buildOpaqueOpenMPClauseExpression(SgPragmaDeclaration *directive,
                                                const std::string &text) {
  SgScopeStatement *scope =
      directive != nullptr ? directive->get_scope() : nullptr;
  if (scope == nullptr && directive != nullptr) {
    scope = SageInterface::getScope(directive);
  }
  if (scope == nullptr) {
    SgNode *global_parent = SageInterface::getGlobalScope(directive);
    scope = isSgScopeStatement(global_parent);
  }
  ROSE_ASSERT(scope != nullptr);
  if (directive != nullptr) {
    if (SgSourceFile *source_file = getEnclosingSourceFile(directive)) {
      const bool is_fortran_file =
          source_file->get_Fortran_only() || source_file->get_F77_only() ||
          source_file->get_F90_only() || source_file->get_F95_only() ||
          source_file->get_F2003_only();
      if (is_fortran_file) {
        // Use a dangling var ref so clause entities preserve original source
        // spelling (e.g., A/anArray) independent of Fortran symbol
        // canonicalization.
        return SageBuilder::buildDanglingVarRefExp(text, scope);
      }
    }
  }
  return SageBuilder::buildOpaqueVarRefExp(text, scope);
}

std::string trimWhitespaceCopy(const std::string &value) {
  const std::string::size_type begin = value.find_first_not_of(" \t\r\n");
  if (begin == std::string::npos) {
    return std::string();
  }
  const std::string::size_type end = value.find_last_not_of(" \t\r\n");
  return value.substr(begin, end - begin + 1);
}

std::string normalizeMapperIdentifierText(const std::string &value) {
  std::string identifier = trimWhitespaceCopy(value);
  const std::string::size_type colon = identifier.find(':');
  if (colon != std::string::npos) {
    identifier = trimWhitespaceCopy(identifier.substr(0, colon));
  }
  return identifier;
}

SgExpression *parseMapperIdentifierExpression(
    SgPragmaDeclaration *pragma_declaration, OpenMPClauseKind clause_kind,
    const std::vector<const OmpParsedExpression *> *parsed_nodes,
    const std::string &raw_identifier_text) {
  const std::string identifier =
      normalizeMapperIdentifierText(raw_identifier_text);
  if (identifier.empty()) {
    return nullptr;
  }

  SgExpression *expr = cloneParsedExpressionNodeByApproxText(
      parsed_nodes, identifier, OMP_EXPR_PARSE_expression);
  if (expr == nullptr) {
    expr = parseOmpExpression(pragma_declaration, clause_kind, identifier);
  }
  return expr;
}

SgExpression *parseClauseExpressionWithCache(
    SgPragmaDeclaration *pragma_declaration, OpenMPClauseKind clause_kind,
    const std::vector<const OmpParsedExpression *> *parsed_nodes,
    const std::string &expression_text, bool prefer_verbatim = false) {
  const std::string trimmed = trimWhitespaceCopy(expression_text);
  if (trimmed.empty()) {
    return nullptr;
  }

  SgExpression *expr = cloneParsedExpressionNodeByApproxText(
      parsed_nodes, trimmed, OMP_EXPR_PARSE_expression);
  if (expr != nullptr) {
    return expr;
  }

  if (prefer_verbatim) {
    return buildOpaqueOpenMPClauseExpression(pragma_declaration, trimmed);
  }

  return parseOmpExpression(pragma_declaration, clause_kind, trimmed);
}

template <typename IteratorT>
std::list<std::list<SgExpression *>> buildClauseIteratorDefinitions(
    SgPragmaDeclaration *pragma_declaration, OpenMPClauseKind clause_kind,
    const std::vector<const OmpParsedExpression *> *parsed_nodes,
    const std::vector<IteratorT> &iterators) {
  std::list<std::list<SgExpression *>> definitions;

  auto parse_iterator_expr = [&](const std::string &text,
                                 bool prefer_verbatim) -> SgExpression * {
    const std::string trimmed = trimWhitespaceCopy(text);
    if (trimmed.empty()) {
      return nullptr;
    }
    SgExpression *expr =
        parseClauseExpressionWithCache(pragma_declaration, clause_kind,
                                       parsed_nodes, trimmed, prefer_verbatim);
    if (expr == nullptr) {
      expr = buildOpaqueOpenMPClauseExpression(pragma_declaration, trimmed);
    }
    return expr;
  };

  for (const IteratorT &iterator_def : iterators) {
    std::list<SgExpression *> iterator_expressions;
    iterator_expressions.push_back(
        parse_iterator_expr(iterator_def.qualifier, true));
    iterator_expressions.push_back(
        parse_iterator_expr(iterator_def.var, false));
    iterator_expressions.push_back(
        parse_iterator_expr(iterator_def.begin, false));
    iterator_expressions.push_back(
        parse_iterator_expr(iterator_def.end, false));
    iterator_expressions.push_back(
        parse_iterator_expr(iterator_def.step, false));
    definitions.push_back(iterator_expressions);
  }

  return definitions;
}

bool collectArraySectionDimensions(
    SgExpression *expression,
    std::vector<std::pair<SgExpression *, SgExpression *>> &dimensions) {
  if (expression == nullptr) {
    return true;
  }

  if (SgCastExp *cast_exp = isSgCastExp(expression)) {
    return collectArraySectionDimensions(cast_exp->get_operand(), dimensions);
  }

  if (SgUnaryOp *unary_op = isSgUnaryOp(expression)) {
    return collectArraySectionDimensions(unary_op->get_operand(), dimensions);
  }

  if (SgPntrArrRefExp *array_ref = isSgPntrArrRefExp(expression)) {
    if (!collectArraySectionDimensions(array_ref->get_lhs_operand(),
                                       dimensions)) {
      return false;
    }
    if (SgSubscriptExpression *subscript =
            isSgSubscriptExpression(array_ref->get_rhs_operand())) {
      SgExpression *lower = subscript->get_lowerBound();
      SgExpression *length = subscript->get_upperBound();
      SgExpression *stride = subscript->get_stride();
      if (stride != nullptr) {
        const SgIntVal *int_stride = isSgIntVal(stride);
        if (int_stride == nullptr || int_stride->get_value() != 1) {
          dimensions.clear();
          return false;
        }
      }
      if (lower != nullptr && length != nullptr) {
        dimensions.push_back(std::make_pair(lower, length));
      }
    }
    return true;
  }

  return true;
}

SgVariableSymbol *extractClauseVariableSymbol(SgNode *node) {
  if (SgInitializedName *initialized_name = isSgInitializedName(node)) {
    return isSgVariableSymbol(
        initialized_name->search_for_symbol_from_symbol_table());
  }

  if (SgVarRefExp *var_ref = isSgVarRefExp(node)) {
    return var_ref->get_symbol();
  }

  if (SgPntrArrRefExp *array_ref = isSgPntrArrRefExp(node)) {
    return extractClauseVariableSymbol(array_ref->get_lhs_operand());
  }

  if (SgDotExp *dot = isSgDotExp(node)) {
    return extractClauseVariableSymbol(dot->get_lhs_operand());
  }

  if (SgArrowExp *arrow = isSgArrowExp(node)) {
    return extractClauseVariableSymbol(arrow->get_lhs_operand());
  }

  if (SgCastExp *cast_exp = isSgCastExp(node)) {
    return extractClauseVariableSymbol(cast_exp->get_operand());
  }

  if (SgUnaryOp *unary_op = isSgUnaryOp(node)) {
    return extractClauseVariableSymbol(unary_op->get_operand());
  }

  return nullptr;
}

SgVariableSymbol *extractDirectArraySectionSymbol(SgNode *node) {
  SgExpression *expression = isSgExpression(node);
  if (expression == nullptr) {
    return nullptr;
  }

  bool saw_array_section = false;
  while (expression != nullptr) {
    if (SgCastExp *cast_exp = isSgCastExp(expression)) {
      expression = cast_exp->get_operand();
      continue;
    }

    if (SgUnaryOp *unary_op = isSgUnaryOp(expression)) {
      expression = unary_op->get_operand();
      continue;
    }

    if (SgPntrArrRefExp *array_ref = isSgPntrArrRefExp(expression)) {
      SgExpression *rhs = array_ref->get_rhs_operand();
      bool rhs_has_section = false;
      if (SgSubscriptExpression *subscript = isSgSubscriptExpression(rhs)) {
        if (subscript->get_lowerBound() == nullptr ||
            subscript->get_upperBound() == nullptr) {
          return nullptr;
        }
        if (SgExpression *stride = subscript->get_stride()) {
          const SgIntVal *int_stride = isSgIntVal(stride);
          if (int_stride == nullptr || int_stride->get_value() != 1) {
            return nullptr;
          }
        }
        rhs_has_section = true;
      } else if (SgExprListExp *expr_list = isSgExprListExp(rhs)) {
        const SgExpressionPtrList &items = expr_list->get_expressions();
        for (SgExpression *item : items) {
          if (item == nullptr) {
            return nullptr;
          }
          SgSubscriptExpression *subscript = isSgSubscriptExpression(item);
          if (subscript == nullptr) {
            // Plain index dimensions are allowed for mixed Fortran sections
            // (e.g., a(i,1:n)).
            continue;
          }
          if (subscript->get_lowerBound() == nullptr ||
              subscript->get_upperBound() == nullptr) {
            return nullptr;
          }
          if (SgExpression *stride = subscript->get_stride()) {
            const SgIntVal *int_stride = isSgIntVal(stride);
            if (int_stride == nullptr || int_stride->get_value() != 1) {
              return nullptr;
            }
          }
          rhs_has_section = true;
        }
      } else {
        return nullptr;
      }

      // Reject plain element references (e.g., a(i,j)); this path only
      // canonicalizes direct array sections.
      if (!rhs_has_section) {
        return nullptr;
      }
      saw_array_section = true;
      expression = array_ref->get_lhs_operand();
      continue;
    }

    if (SgVarRefExp *var_ref = isSgVarRefExp(expression)) {
      return saw_array_section ? var_ref->get_symbol() : nullptr;
    }

    // Dot/arrow/member and any other non-direct base expressions are not
    // representable in symbol-keyed array_dimensions/dist_data maps.
    return nullptr;
  }

  return nullptr;
}

} // namespace

// Liao 4/23/2011, special function to copy file info of the original SgPragma
// or Fortran comments
bool copyStartFileInfo(SgNode *src, SgNode *dest) {
  bool result = false;
  ROSE_ASSERT(src && dest);
  // same src and dest, no copy is needed
  if (src == dest)
    return true;

  SgLocatedNode *lsrc = isSgLocatedNode(src);
  ROSE_ASSERT(lsrc);
  SgLocatedNode *ldest = isSgLocatedNode(dest);
  ROSE_ASSERT(ldest);
  // ROSE_ASSERT (lsrc->get_file_info()->isTransformation() == false);
  // already the same, no copy is needed
  if (lsrc->get_startOfConstruct()->get_filenameString() ==
          ldest->get_startOfConstruct()->get_filenameString() &&
      lsrc->get_startOfConstruct()->get_line() ==
          ldest->get_startOfConstruct()->get_line() &&
      lsrc->get_startOfConstruct()->get_col() ==
          ldest->get_startOfConstruct()->get_col())
    return true;

  Sg_File_Info *copy = new Sg_File_Info(*(lsrc->get_startOfConstruct()));
  ROSE_ASSERT(copy != NULL);

  // delete old start of construct
  Sg_File_Info *old_info = ldest->get_startOfConstruct();
  if (old_info)
    delete (old_info);

  ldest->set_startOfConstruct(copy);
  copy->set_parent(ldest);
  //  cout<<"debug: set ldest@"<<ldest <<" with file info @"<< copy <<endl;

  ROSE_ASSERT(lsrc->get_startOfConstruct()->get_filenameString() ==
              ldest->get_startOfConstruct()->get_filenameString());
  ROSE_ASSERT(lsrc->get_startOfConstruct()->get_line() ==
              ldest->get_startOfConstruct()->get_line());
  ROSE_ASSERT(lsrc->get_startOfConstruct()->get_col() ==
              ldest->get_startOfConstruct()->get_col());

  ROSE_ASSERT(lsrc->get_startOfConstruct()->get_filenameString() ==
              ldest->get_file_info()->get_filenameString());
  ROSE_ASSERT(lsrc->get_startOfConstruct()->get_line() ==
              ldest->get_file_info()->get_line());
  ROSE_ASSERT(lsrc->get_startOfConstruct()->get_col() ==
              ldest->get_file_info()->get_col());

  ROSE_ASSERT(ldest->get_file_info() == copy);
  // Adjustment for Fortran, the AST node attaching the Fortran comment will not
  // actual give out the accurate line number for the comment
  if (is_Fortran_language()) {
    int commentLine = ompparser_OpenMPIR->getLine();
    ldest->get_file_info()->set_line(commentLine);
  }

  return result;
}

// Liao 3/11/2013, special function to copy end file info of the original
// SgPragma or Fortran comments (src) to OpenMP node (dest) If the OpenMP node
// is a body statement, we have to use the body's end file info as the node's
// end file info.
bool copyEndFileInfo(SgNode *src, SgNode *dest) {
  bool result = false;
  ROSE_ASSERT(src && dest);

  if (SgOmpBodyStatement *body_stmt = isSgOmpBodyStatement(dest)) {
    if (body_stmt->get_body() != NULL) {
      src = body_stmt->get_body();
    }
  }

  // same src and dest, no copy is needed
  if (src == dest)
    return true;

  SgLocatedNode *lsrc = isSgLocatedNode(src);
  ROSE_ASSERT(lsrc);
  SgLocatedNode *ldest = isSgLocatedNode(dest);
  ROSE_ASSERT(ldest);

  bool expected_transformation = false;
  if (ldest->get_file_info() != nullptr) {
    expected_transformation = ldest->get_file_info()->isTransformation();
  } else if (ldest->get_startOfConstruct() != nullptr) {
    expected_transformation = ldest->get_startOfConstruct()->isTransformation();
  }

  // ROSE_ASSERT (lsrc->get_file_info()->isTransformation() == false);
  // already the same, no copy is needed
  if (lsrc->get_endOfConstruct()->get_filenameString() ==
          ldest->get_endOfConstruct()->get_filenameString() &&
      lsrc->get_endOfConstruct()->get_line() ==
          ldest->get_endOfConstruct()->get_line() &&
      lsrc->get_endOfConstruct()->get_col() ==
          ldest->get_endOfConstruct()->get_col() &&
      ldest->get_endOfConstruct()->isTransformation() ==
          expected_transformation)
    return true;

  Sg_File_Info *copy = new Sg_File_Info(*(lsrc->get_endOfConstruct()));
  ROSE_ASSERT(copy != NULL);

  // delete old start of construct
  Sg_File_Info *old_info = ldest->get_endOfConstruct();
  if (old_info)
    delete (old_info);

  ldest->set_endOfConstruct(copy);
  copy->set_parent(ldest);
  if (expected_transformation) {
    copy->setTransformation();
  } else {
    copy->unsetTransformation();
  }

  ROSE_ASSERT(lsrc->get_endOfConstruct()->get_filenameString() ==
              ldest->get_endOfConstruct()->get_filenameString());
  ROSE_ASSERT(lsrc->get_endOfConstruct()->get_line() ==
              ldest->get_endOfConstruct()->get_line());
  ROSE_ASSERT(lsrc->get_endOfConstruct()->get_col() ==
              ldest->get_endOfConstruct()->get_col());
  ROSE_ASSERT(ldest->get_endOfConstruct() == copy);
  if (ldest->get_file_info() != nullptr) {
    ROSE_ASSERT(ldest->get_endOfConstruct()->isTransformation() ==
                ldest->get_file_info()->isTransformation());
  }

  return result;
}

namespace OmpSupport {
// an internal data structure to avoid redundant AST traversal to find OpenMP
// pragmas
static std::list<SgPragmaDeclaration *> omp_pragma_list;

// the vector of pairs of OpenMP pragma and Ompparser IR.
static std::vector<std::pair<SgPragmaDeclaration *, OpenMPDirective *>>
    OpenMPIR_list;

static void clearClauseParseCacheForSourceFile(SgSourceFile *source_file) {
  if (source_file == nullptr) {
    return;
  }

  std::vector<OpenMPDirective *> directives_to_clear;
  directives_to_clear.reserve(OpenMPIR_list.size());
  for (const auto &entry : OpenMPIR_list) {
    if (entry.first == nullptr || entry.second == nullptr) {
      continue;
    }
    if (getEnclosingSourceFile(entry.first) == source_file) {
      directives_to_clear.push_back(entry.second);
    }
  }

  for (OpenMPDirective *directive : directives_to_clear) {
    g_omp_clause_nodes.erase(directive);
  }
}

static bool belongsToSourceFile(SgNode *node, SgSourceFile *source_file) {
  if (node == nullptr || source_file == nullptr) {
    return false;
  }
  return getEnclosingSourceFile(node) == source_file;
}

static void releaseOpenMPParseStateForSourceFile(SgSourceFile *source_file) {
  if (source_file == nullptr) {
    return;
  }

  clearClauseParseCacheForSourceFile(source_file);

  std::unordered_set<OpenMPDirective *> omp_directives_to_delete;
  std::unordered_set<OpenACCDirective *> acc_directives_to_delete;

  auto track_openmp_directive = [&](OpenMPDirective *directive) {
    if (directive != nullptr) {
      omp_directives_to_delete.insert(directive);
    }
  };
  auto track_openacc_directive = [&](OpenACCDirective *directive) {
    if (directive != nullptr) {
      acc_directives_to_delete.insert(directive);
    }
  };

  for (auto it = OpenMPIR_list.begin(); it != OpenMPIR_list.end();) {
    if (belongsToSourceFile(it->first, source_file)) {
      track_openmp_directive(it->second);
      it = OpenMPIR_list.erase(it);
    } else {
      ++it;
    }
  }

  for (auto it = OpenACCIR_list.begin(); it != OpenACCIR_list.end();) {
    if (belongsToSourceFile(it->first, source_file)) {
      track_openacc_directive(it->second);
      it = OpenACCIR_list.erase(it);
    } else {
      ++it;
    }
  }

  for (auto it = fortran_omp_pragma_list.begin();
       it != fortran_omp_pragma_list.end();) {
    SgLocatedNode *loc = std::get<0>(*it);
    if (belongsToSourceFile(loc, source_file)) {
      track_openmp_directive(std::get<2>(*it));
      it = fortran_omp_pragma_list.erase(it);
    } else {
      ++it;
    }
  }

  for (auto it = fortran_paired_pragma_dict.begin();
       it != fortran_paired_pragma_dict.end();) {
    if (belongsToSourceFile(it->first, source_file)) {
      track_openmp_directive(it->second);
      it = fortran_paired_pragma_dict.erase(it);
    } else {
      ++it;
    }
  }

  for (auto it = fortran_acc_paired_pragma_dict.begin();
       it != fortran_acc_paired_pragma_dict.end();) {
    if (belongsToSourceFile(it->first, source_file)) {
      track_openacc_directive(it->second);
      it = fortran_acc_paired_pragma_dict.erase(it);
    } else {
      ++it;
    }
  }

  for (auto it = omp_pragma_list.begin(); it != omp_pragma_list.end();) {
    if (belongsToSourceFile(*it, source_file)) {
      it = omp_pragma_list.erase(it);
    } else {
      ++it;
    }
  }

  for (auto it = g_pending_commented_directive_relocations.begin();
       it != g_pending_commented_directive_relocations.end();) {
    if (belongsToSourceFile(it->first, source_file)) {
      it = g_pending_commented_directive_relocations.erase(it);
    } else {
      ++it;
    }
  }

  for (auto it = g_omp_directive_source_text_by_pragma.begin();
       it != g_omp_directive_source_text_by_pragma.end();) {
    if (belongsToSourceFile(it->first, source_file)) {
      it = g_omp_directive_source_text_by_pragma.erase(it);
    } else {
      ++it;
    }
  }

  for (OpenMPDirective *directive : omp_directives_to_delete) {
    delete directive;
  }
  for (OpenACCDirective *directive : acc_directives_to_delete) {
    delete directive;
  }
}

static std::string toLowerCopy(const std::string &input) {
  std::string result = input;
  std::transform(
      result.begin(), result.end(), result.begin(),
      [](unsigned char ch) { return static_cast<char>(std::tolower(ch)); });
  return result;
}

static std::string canonicalizeDirectiveKey(const std::string &input) {
  std::string result;
  result.reserve(input.size());
  for (unsigned char ch : input) {
    if (std::isspace(ch)) {
      continue;
    }
    result.push_back(static_cast<char>(std::tolower(ch)));
  }
  return result;
}

static void trimLeft(std::string &text) {
  size_t pos = 0;
  while (pos < text.size() &&
         std::isspace(static_cast<unsigned char>(text[pos]))) {
    ++pos;
  }
  text.erase(0, pos);
}

static void trimRight(std::string &text) {
  while (!text.empty() &&
         std::isspace(static_cast<unsigned char>(text.back()))) {
    text.pop_back();
  }
}

static void trim(std::string &text) {
  trimLeft(text);
  trimRight(text);
}

static void stripFortranComment(std::string &text) {
  size_t pos = text.find('!');
  if (pos != std::string::npos) {
    text.erase(pos);
  }
}

static void stripFortranDirectiveSentinel(std::string &text) {
  trimLeft(text);
  if (text.empty()) {
    return;
  }
  const char marker =
      static_cast<char>(std::tolower(static_cast<unsigned char>(text.front())));
  if (marker == '!' || marker == 'c' || marker == 'd' || marker == '*') {
    size_t next = 1;
    while (next < text.size() &&
           std::isspace(static_cast<unsigned char>(text[next]))) {
      ++next;
    }
    if (next < text.size() && text[next] == '$') {
      text.erase(0, next + 1);
      trimLeft(text);
      return;
    }
  }
  if (text.front() == '$') {
    text.erase(0, 1);
    trimLeft(text);
  }
}

static bool hasFortranLineContinuation(const std::string &text) {
  std::string trimmed = text;
  trimRight(trimmed);
  return !trimmed.empty() && trimmed.back() == '&';
}

static void stripFortranLineContinuation(std::string &text) {
  trimRight(text);
  if (!text.empty() && text.back() == '&') {
    text.pop_back();
    trimRight(text);
  }
}

static bool startsWithCaseInsensitive(const std::string &text,
                                      const std::string &prefix) {
  if (text.size() < prefix.size()) {
    return false;
  }
  for (size_t i = 0; i < prefix.size(); ++i) {
    const unsigned char lhs = static_cast<unsigned char>(text[i]);
    const unsigned char rhs = static_cast<unsigned char>(prefix[i]);
    if (std::tolower(lhs) != std::tolower(rhs)) {
      return false;
    }
  }
  return true;
}

static bool startsWithKeywordBoundaryCaseInsensitive(const std::string &text,
                                                     const char *keyword) {
  const size_t keyword_len = std::strlen(keyword);
  if (text.size() < keyword_len) {
    return false;
  }
  for (size_t i = 0; i < keyword_len; ++i) {
    const unsigned char lhs = static_cast<unsigned char>(text[i]);
    const unsigned char rhs = static_cast<unsigned char>(keyword[i]);
    if (std::tolower(lhs) != std::tolower(rhs)) {
      return false;
    }
  }
  if (text.size() == keyword_len) {
    return true;
  }
  const unsigned char next = static_cast<unsigned char>(text[keyword_len]);
  return !std::isalnum(next) && next != '_';
}

static bool startsWithOpenMPDirectiveKeyword(const std::string &text) {
  return startsWithKeywordBoundaryCaseInsensitive(text, "omp") ||
         startsWithKeywordBoundaryCaseInsensitive(text, "ompx");
}

static size_t findCaseInsensitive(const std::string &haystack,
                                  const std::string &needle, size_t pos) {
  if (needle.empty()) {
    return pos <= haystack.size() ? pos : std::string::npos;
  }
  const std::string lower_haystack = toLowerCopy(haystack);
  const std::string lower_needle = toLowerCopy(needle);
  return lower_haystack.find(lower_needle, pos);
}

static size_t rfindCaseInsensitive(const std::string &haystack,
                                   const std::string &needle, size_t pos) {
  if (needle.empty()) {
    return pos <= haystack.size() ? pos : std::string::npos;
  }
  const std::string lower_haystack = toLowerCopy(haystack);
  const std::string lower_needle = toLowerCopy(needle);
  return lower_haystack.rfind(lower_needle, pos);
}

static bool startsWithAccKeyword(const std::string &text) {
  if (!startsWithCaseInsensitive(text, "acc")) {
    return false;
  }
  if (text.size() == 3) {
    return true;
  }
  const char next = text[3];
  return std::isspace(static_cast<unsigned char>(next)) || next == '(';
}

static bool isFortranAccDirective(const std::string &text) {
  std::string trimmed = text;
  trimLeft(trimmed);
  if (trimmed.empty()) {
    return false;
  }
  const char marker =
      static_cast<char>(std::tolower(static_cast<unsigned char>(trimmed[0])));
  if (marker != '!' && marker != 'c' && marker != 'd' && marker != '*') {
    return false;
  }
  size_t pos = 1;
  while (pos < trimmed.size() &&
         std::isspace(static_cast<unsigned char>(trimmed[pos]))) {
    ++pos;
  }
  if (pos >= trimmed.size() || trimmed[pos] != '$') {
    return false;
  }
  ++pos;
  while (pos < trimmed.size() &&
         std::isspace(static_cast<unsigned char>(trimmed[pos]))) {
    ++pos;
  }
  return startsWithCaseInsensitive(trimmed.substr(pos), "acc");
}

static void normalizeFortranAccSentinel(std::string &buffer) {
  size_t pos = buffer.find_first_not_of(" \t");
  if (pos == std::string::npos) {
    return;
  }
  const char marker =
      static_cast<char>(std::tolower(static_cast<unsigned char>(buffer[pos])));
  if (marker != '!' && marker != 'c' && marker != 'd' && marker != '*') {
    return;
  }
  size_t next = pos + 1;
  while (next < buffer.size() &&
         std::isspace(static_cast<unsigned char>(buffer[next]))) {
    ++next;
  }
  if (next >= buffer.size() || buffer[next] != '$') {
    return;
  }
  if (next > pos + 1) {
    buffer.erase(pos + 1, next - (pos + 1));
  }
  const size_t dollar = pos + 1;
  size_t acc_start = dollar + 1;
  while (acc_start < buffer.size() &&
         std::isspace(static_cast<unsigned char>(buffer[acc_start]))) {
    ++acc_start;
  }
  if (acc_start > dollar + 1) {
    buffer.erase(dollar + 1, acc_start - (dollar + 1));
  }
}

static void removeFortranAccComments(std::string &buffer) {
  size_t pos1;
  size_t pos2;
  size_t pos3 = std::string::npos;

  pos1 = buffer.rfind("!", pos3);
  while (pos1 != std::string::npos) {
    pos2 = rfindCaseInsensitive(buffer, "!$acc", pos3);
    if (pos1 != pos2) {
      buffer.erase(pos1);
    } else {
      if (pos2 >= 1) {
        pos3 = pos2 - 1;
      } else {
        break;
      }
    }
    pos1 = buffer.rfind("!", pos3);
  }
}

static void postProcessMergedAccContinuation(std::string &buffer) {
  removeFortranAccComments(buffer);
  size_t first_pos = buffer.find("&");
  if (first_pos == std::string::npos) {
    return;
  }
  size_t second_pos = findCaseInsensitive(buffer, "$acc", first_pos);
  if (second_pos == std::string::npos) {
    return;
  }
  second_pos += 3;
  size_t last_pos = buffer.find("&", second_pos);
  if (hasFortranLineContinuation(buffer)) {
    size_t next_cont_pos = buffer.rfind("&");
    if (last_pos == next_cont_pos) {
      last_pos = std::string::npos;
    }
  }
  if (last_pos == std::string::npos) {
    last_pos = second_pos;
  }
  buffer.erase(first_pos, last_pos - first_pos + 1);
}

static std::string stripOmpPrefix(std::string text) {
  trimLeft(text);
  if (startsWithKeywordBoundaryCaseInsensitive(text, "ompx")) {
    text.erase(0, 4);
    trimLeft(text);
  } else if (startsWithKeywordBoundaryCaseInsensitive(text, "omp")) {
    text.erase(0, 3);
    trimLeft(text);
  }
  return text;
}

static void stripLeadingContinuation(std::string &text) {
  trimLeft(text);
  if (!text.empty() && text.front() == '&') {
    text.erase(0, 1);
    trimLeft(text);
  }
}

static bool extractFortranOpenMPDirectivePayload(std::string &text) {
  std::string candidate = text;
  trimLeft(candidate);
  stripFortranDirectiveSentinel(candidate);
  trimLeft(candidate);
  if (startsWithOpenMPDirectiveKeyword(candidate)) {
    text = candidate;
    return true;
  }

  // Accept embedded sentinels only from preprocessor-like lines (e.g.
  // "#define X !$omp ..."), not from regular comments that merely mention
  // "!$omp ..." or "!$ompx ...".
  std::string leading = text;
  trimLeft(leading);
  if (leading.empty() || leading.front() != '#') {
    return false;
  }

  size_t marker = findCaseInsensitive(text, "!$ompx", 0);
  if (marker == std::string::npos) {
    marker = findCaseInsensitive(text, "c$ompx", 0);
  }
  if (marker == std::string::npos) {
    marker = findCaseInsensitive(text, "d$ompx", 0);
  }
  if (marker == std::string::npos) {
    marker = findCaseInsensitive(text, "*$ompx", 0);
  }
  if (marker == std::string::npos) {
    marker = findCaseInsensitive(text, "!$omp", 0);
  }
  if (marker == std::string::npos) {
    marker = findCaseInsensitive(text, "c$omp", 0);
  }
  if (marker == std::string::npos) {
    marker = findCaseInsensitive(text, "d$omp", 0);
  }
  if (marker == std::string::npos) {
    marker = findCaseInsensitive(text, "*$omp", 0);
  }
  if (marker == std::string::npos) {
    return false;
  }

  candidate = text.substr(marker);
  stripFortranDirectiveSentinel(candidate);
  trimLeft(candidate);
  if (!startsWithOpenMPDirectiveKeyword(candidate)) {
    return false;
  }

  text = candidate;
  return true;
}

static bool allowsImplicitFortranEnd(OpenMPDirectiveKind kind) {
  switch (kind) {
  case OMPD_parallel:
  case OMPD_do:
  case OMPD_parallel_do:
  case OMPD_parallel_loop:
  case OMPD_target:
  case OMPD_target_simd:
  case OMPD_declare_target:
    return true;
  default:
    return false;
  }
}

static bool allowsImplicitOpenMPEnd(OpenMPDirective *directive) {
  if (directive == NULL) {
    return true;
  }

  if (directive->getRequiresExplicitEnd()) {
    return false;
  }

  if (allowsImplicitFortranEnd(directive->getKind())) {
    return true;
  }

  return !isFortranPairedDirective(directive);
}

static bool isOpenMPDirectiveEndMarkerOnly(OpenMPDirective *directive) {
  if (directive == nullptr || directive->getKind() != OMPD_end) {
    return false;
  }

  OpenMPDirective *paired =
      static_cast<OpenMPEndDirective *>(directive)->getPairedDirective();
  return paired != nullptr && paired->getRequiresExplicitEnd();
}

static bool shouldSkipOpenMPDirectiveAstConversion(OpenMPDirective *directive) {
  if (directive == nullptr) {
    return false;
  }
  return isOpenMPDirectiveEndMarkerOnly(directive);
}

static bool parseOpenMPFortranPragmas(SgSourceFile *sageFilePtr) {
  std::vector<SgNode *> all_pragmas =
      NodeQuery::querySubTree(sageFilePtr, V_SgPragmaDeclaration);
  std::vector<SgPragmaDeclaration *> omp_pragmas;
  for (std::vector<SgNode *>::iterator iter = all_pragmas.begin();
       iter != all_pragmas.end(); ++iter) {
    SgPragmaDeclaration *pragmaDecl = isSgPragmaDeclaration(*iter);
    ROSE_ASSERT(pragmaDecl != NULL);
    if (getEnclosingSourceFile(pragmaDecl) != sageFilePtr) {
      continue;
    }
    std::string pragmaString = pragmaDecl->get_pragma()->get_pragma();
    std::string normalized = pragmaString;
    if (extractFortranOpenMPDirectivePayload(normalized)) {
      omp_pragmas.push_back(pragmaDecl);
    }
  }

  if (omp_pragmas.empty()) {
    return false;
  }
  setLang(Lang_Fortran);
  std::vector<OpenMPDirective *> pairing_list;
  std::vector<std::pair<SgPragmaDeclaration *, OpenMPDirective *>>
      local_OpenMPIR_list;
  std::unordered_map<OpenMPDirective *, std::string> local_pragma_text_by_ir;
  std::vector<SgPragmaDeclaration *> local_omp_pragma_list;
  std::map<SgPragmaDeclaration *, OpenMPDirective *>
      local_fortran_paired_pragma_dict;
  std::vector<SgPragmaDeclaration *> pragmas_to_remove;
  std::vector<SgPragmaDeclaration *> pending_pragmas;
  std::string pending;
  SgPragmaDeclaration *prev_pragma = NULL;
  bool prev_continuation = false;
  auto cleanup_local_directives = [&]() {
    std::unordered_set<OpenMPDirective *> directives_to_delete;
    directives_to_delete.reserve(local_fortran_paired_pragma_dict.size());
    for (const auto &entry : local_fortran_paired_pragma_dict) {
      if (entry.second != NULL) {
        directives_to_delete.insert(entry.second);
      }
    }

    for (OpenMPDirective *directive : directives_to_delete) {
      delete directive;
    }
  };

  for (size_t i = 0; i < omp_pragmas.size(); ++i) {
    SgPragmaDeclaration *pragmaDecl = omp_pragmas[i];
    if (prev_continuation && getNextStatement(prev_pragma) != pragmaDecl) {
      cerr << "error: Fortran OpenMP line continuation is not contiguous\n";
      ROSE_ABORT();
    }

    std::string line = getFortranOpenMPDirectiveSourceText(pragmaDecl);
    std::string cleaned = line;
    if (!extractFortranOpenMPDirectivePayload(cleaned)) {
      prev_pragma = pragmaDecl;
      prev_continuation = false;
      continue;
    }
    stripFortranComment(cleaned);
    trim(cleaned);
    if (cleaned.empty()) {
      prev_pragma = pragmaDecl;
      prev_continuation = false;
      continue;
    }

    bool has_continuation = hasFortranLineContinuation(cleaned);
    stripFortranLineContinuation(cleaned);

    if (pending_pragmas.empty()) {
      pending = cleaned;
      pending_pragmas.push_back(pragmaDecl);
    } else {
      std::string continuation = stripOmpPrefix(cleaned);
      stripLeadingContinuation(continuation);
      pending += continuation;
      pending_pragmas.push_back(pragmaDecl);
    }

    prev_pragma = pragmaDecl;
    prev_continuation = has_continuation;
    if (has_continuation) {
      continue;
    }

    std::string parse_buffer = pending;
    trimLeft(parse_buffer);
    if (!startsWithCaseInsensitive(parse_buffer, "!$")) {
      parse_buffer = std::string("!$") + parse_buffer;
    }

    ompparser_OpenMPIR = parseOpenMP(parse_buffer.c_str(), nullptr, nullptr);
    if (ompparser_OpenMPIR == NULL && parse_buffer != pending) {
      ompparser_OpenMPIR = parseOpenMP(pending.c_str(), nullptr, nullptr);
    }
    if (ompparser_OpenMPIR == NULL) {
      cleanup_local_directives();
      setLang(Lang_unknown);
      return false;
    }

    if (ompparser_OpenMPIR->getKind() != OMPD_end) {
      pairing_list.push_back(ompparser_OpenMPIR);
    }
    if (ompparser_OpenMPIR->getKind() == OMPD_end) {
      if (pairing_list.empty()) {
        cleanup_local_directives();
        delete ompparser_OpenMPIR;
        ompparser_OpenMPIR = NULL;
        setLang(Lang_unknown);
        return false;
      }
      OpenMPDirective *end_directive =
          ((OpenMPEndDirective *)ompparser_OpenMPIR)->getPairedDirective();
      bool matched = false;
      while (!pairing_list.empty()) {
        OpenMPDirective *begin_directive = pairing_list.back();
        if (end_directive->getKind() == begin_directive->getKind()) {
          mergeEndClausesToBeginDirective(begin_directive, end_directive,
                                          ompparser_OpenMPIR);
          ((OpenMPEndDirective *)ompparser_OpenMPIR)
              ->setPairedDirective(begin_directive);
          pairing_list.pop_back();
          matched = true;
          break;
        }
        // Keep searching for the matching begin directive; some intervening
        // directives are not closed by explicit END pragmas.
        pairing_list.pop_back();
      }
      if (!matched) {
        cleanup_local_directives();
        delete ompparser_OpenMPIR;
        ompparser_OpenMPIR = NULL;
        setLang(Lang_unknown);
        return false;
      }
    }

    SgPragmaDeclaration *primary = pending_pragmas.front();
    std::string directive_source_text = pending;
    local_fortran_paired_pragma_dict[primary] = ompparser_OpenMPIR;
    local_pragma_text_by_ir[ompparser_OpenMPIR] = directive_source_text;
    const bool is_end_directive = ompparser_OpenMPIR->getKind() == OMPD_end;
    if (!is_end_directive &&
        !shouldSkipOpenMPDirectiveAstConversion(ompparser_OpenMPIR)) {
      local_OpenMPIR_list.push_back(
          std::make_pair(primary, ompparser_OpenMPIR));
      local_omp_pragma_list.push_back(primary);
    }

    for (size_t j = 1; j < pending_pragmas.size(); ++j) {
      pragmas_to_remove.push_back(pending_pragmas[j]);
    }

    pending_pragmas.clear();
    pending.clear();
  }

  if (!pending_pragmas.empty()) {
    cerr << "error: Fortran OpenMP line continuation is unterminated\n";
    ROSE_ABORT();
  }

  for (const auto &entry : local_OpenMPIR_list) {
    OpenMPIR_list.push_back(entry);
    if (entry.second->getKind() == OMPD_end) {
      continue;
    }
    const auto text_it = local_pragma_text_by_ir.find(entry.second);
    ROSE_ASSERT(text_it != local_pragma_text_by_ir.end());
    g_omp_clause_nodes[entry.second] = parseClauseNodesForDirective(
        entry.first, entry.second, text_it->second);
    g_omp_directive_source_text_by_pragma[entry.first] = text_it->second;
  }
  for (SgPragmaDeclaration *decl : local_omp_pragma_list) {
    omp_pragma_list.push_back(decl);
  }
  for (const auto &entry : local_fortran_paired_pragma_dict) {
    fortran_paired_pragma_dict[entry.first] = entry.second;
  }
  for (SgPragmaDeclaration *decl : pragmas_to_remove) {
    removeStatement(decl);
  }
  setLang(Lang_unknown);
  return true;
}

static bool fortranAstUnparserEmitsOpenMPEnd(OpenMPDirectiveKind kind);

static bool isFortranCommentDirective(PreprocessingInfo *info,
                                      const std::string &keyword) {
  if (info == NULL) {
    return false;
  }

  std::string text = info->getString();
  stripFortranDirectiveSentinel(text);
  trimLeft(text);
  if (startsWithCaseInsensitive(text, keyword)) {
    return true;
  }

  std::string continuation = text;
  stripLeadingContinuation(continuation);
  if (startsWithCaseInsensitive(continuation, keyword)) {
    return true;
  }

  std::string raw = info->getString();
  trimLeft(raw);
  if (!raw.empty() && raw.front() == '&') {
    stripLeadingContinuation(raw);
    if (startsWithCaseInsensitive(raw, keyword)) {
      return true;
    }
  }

  return false;
}

static bool isFortranDirectiveSentinelOnly(const std::string &text) {
  std::string trimmed = text;
  trimLeft(trimmed);
  if (trimmed.empty()) {
    return false;
  }

  const char marker = static_cast<char>(
      std::tolower(static_cast<unsigned char>(trimmed.front())));
  if (marker != '!' && marker != 'c' && marker != 'd' && marker != '*') {
    return false;
  }

  size_t pos = 1;
  while (pos < trimmed.size() &&
         std::isspace(static_cast<unsigned char>(trimmed[pos]))) {
    ++pos;
  }
  if (pos >= trimmed.size() || trimmed[pos] != '$') {
    return false;
  }
  ++pos;
  while (pos < trimmed.size() &&
         std::isspace(static_cast<unsigned char>(trimmed[pos]))) {
    ++pos;
  }
  return pos == trimmed.size();
}

static void removeFortranDirectiveComments(SgSourceFile *sageFilePtr,
                                           const std::string &keyword) {
  ROSE_ASSERT(sageFilePtr != NULL);

  std::vector<SgNode *> loc_nodes =
      NodeQuery::querySubTree(sageFilePtr, V_SgLocatedNode);
  for (SgNode *node : loc_nodes) {
    SgLocatedNode *locNode = isSgLocatedNode(node);
    ROSE_ASSERT(locNode != NULL);

    AttachedPreprocessingInfoType *comments =
        locNode->getAttachedPreprocessingInfo();
    if (comments == NULL || comments->empty()) {
      continue;
    }

    for (AttachedPreprocessingInfoType::iterator iter = comments->begin();
         iter != comments->end();) {
      const bool is_directive = isFortranCommentDirective(*iter, keyword);
      bool is_sentinel_lead = false;
      if (!is_directive &&
          isFortranDirectiveSentinelOnly((*iter)->getString())) {
        AttachedPreprocessingInfoType::iterator next_iter = iter;
        ++next_iter;
        if (next_iter != comments->end() &&
            isFortranCommentDirective(*next_iter, keyword)) {
          is_sentinel_lead = true;
        }
      }
      if (is_directive || is_sentinel_lead) {
        iter = comments->erase(iter);
      } else {
        ++iter;
      }
    }
  }
}

static bool isFortranOpenMPPragmaDeclaration(SgPragmaDeclaration *decl) {
  if (decl == NULL || decl->get_pragma() == NULL) {
    return false;
  }

  std::string pragma_text = decl->get_pragma()->get_pragma();
  stripFortranDirectiveSentinel(pragma_text);
  trimLeft(pragma_text);
  return startsWithOpenMPDirectiveKeyword(pragma_text);
}

static SgPragmaDeclaration *
findFortranPragmaForDirective(OpenMPDirective *directive) {
  if (directive == NULL) {
    return NULL;
  }
  for (const auto &entry : fortran_paired_pragma_dict) {
    if (entry.second == directive) {
      return entry.first;
    }
  }
  return NULL;
}

static void removeFortranOpenMPPragmas(SgSourceFile *sageFilePtr) {
  ROSE_ASSERT(sageFilePtr != NULL);

  std::vector<SgNode *> all_pragmas =
      NodeQuery::querySubTree(sageFilePtr, V_SgPragmaDeclaration);
  for (SgNode *node : all_pragmas) {
    SgPragmaDeclaration *decl = isSgPragmaDeclaration(node);
    if (decl == NULL) {
      continue;
    }
    Sg_File_Info *info = decl->get_file_info();
    if (info == NULL) {
      continue;
    }
    if (info->get_filename() != sageFilePtr->get_file_info()->get_filename() &&
        !info->isTransformation()) {
      continue;
    }
    if (!isFortranOpenMPPragmaDeclaration(decl)) {
      continue;
    }
    if (decl->getAttribute(kFortranKeepOpenMPPragmaAttributeName) != NULL) {
      continue;
    }
    auto pair_it = fortran_paired_pragma_dict.find(decl);
    if (pair_it == fortran_paired_pragma_dict.end()) {
      // Only clean up Fortran OpenMP pragmas that were successfully parsed and
      // paired in this pass. Leave untouched pragmas intact.
      continue;
    }
    if (pair_it != fortran_paired_pragma_dict.end() &&
        pair_it->second != NULL && pair_it->second->getKind() == OMPD_end) {
      OpenMPEndDirective *end_directive =
          static_cast<OpenMPEndDirective *>(pair_it->second);
      OpenMPDirective *paired_begin =
          end_directive != NULL ? end_directive->getPairedDirective() : NULL;
      SgPragmaDeclaration *paired_begin_pragma =
          findFortranPragmaForDirective(paired_begin);
      if (paired_begin_pragma != NULL &&
          paired_begin_pragma->getAttribute(
              kFortranKeepOpenMPPragmaAttributeName) != NULL) {
        continue;
      }
      if (paired_begin != NULL &&
          !fortranAstUnparserEmitsOpenMPEnd(paired_begin->getKind())) {
        continue;
      }
    }
    removeStatement(decl, false);
  }
}

static bool hasFortranOpenMPArtifactsForSourceFile(SgSourceFile *sageFilePtr) {
  if (sageFilePtr == NULL) {
    return false;
  }

  for (const auto &entry : fortran_paired_pragma_dict) {
    if (entry.first != NULL &&
        getEnclosingSourceFile(entry.first) == sageFilePtr) {
      return true;
    }
  }

  for (const auto &entry : fortran_omp_pragma_list) {
    SgLocatedNode *loc = std::get<0>(entry);
    if (loc != NULL && getEnclosingSourceFile(loc) == sageFilePtr) {
      return true;
    }
  }

  return false;
}

// Clause node builders
//----------------------------------------------------------
// Sara Royuela ( Nov 2, 2012 ): Check for clause parameters that can be defined
// in macros This adds support for the use of macro definitions in OpenMP
// clauses We need a traversal over SgExpression to support macros in any
// position of an "assignment_expr" F.i.:   #define THREADS_1 16
//         #define THREADS_2 8
//         int main( int arg, char** argv ) {
//         #pragma omp parallel num_threads( THREADS_1 + THREADS_2 )
//           {}
//         }
SgVarRefExpVisitor::SgVarRefExpVisitor() : expressions() {}

std::vector<SgExpression *> SgVarRefExpVisitor::get_expressions() {
  return expressions;
}

void SgVarRefExpVisitor::visit(SgNode *node) {
  SgExpression *expr = isSgVarRefExp(node);
  if (expr != NULL) {
    expressions.push_back(expr);
  }
}

SgExpression *replace_expression_with_macro_value(std::string define_macro,
                                                  SgExpression *old_exp,
                                                  bool &macro_replaced,
                                                  omp_construct_enum) {
  SgExpression *newExp = old_exp;
  // Parse the macro: we are only interested in macros with the form #define
  // MACRO_NAME MACRO_VALUE, the constant macro
  size_t parenthesis = define_macro.find("(");
  if (parenthesis == string::npos) { // Non function macro, constant macro
    unsigned int macroNameInitPos =
        (unsigned int)(define_macro.find("define")) + 6;
    while (macroNameInitPos < define_macro.size() &&
           define_macro[macroNameInitPos] == ' ')
      macroNameInitPos++;
    unsigned int macroNameEndPos = define_macro.find(" ", macroNameInitPos);
    std::string macroName = define_macro.substr(
        macroNameInitPos, macroNameEndPos - macroNameInitPos);

    if (macroName == isSgVarRefExp(old_exp)
                         ->get_symbol()
                         ->get_name()
                         .getString()) { // Clause is defined in a macro
      size_t comma = define_macro.find(",");
      if (comma == string::npos) // Macros like "#define MACRO_NAME VALUE1,
                                 // VALUE2" are not accepted
      { // We create here an expression with the value of the clause defined in
        // the macro
        unsigned int macroValueInitPos = macroNameEndPos + 1;
        while (macroValueInitPos < define_macro.size() &&
               define_macro[macroValueInitPos] == ' ')
          macroValueInitPos++;
        unsigned int macroValueEndPos = macroValueInitPos;
        while (macroValueEndPos < define_macro.size() &&
               define_macro[macroValueEndPos] != ' ' &&
               define_macro[macroValueEndPos] != '\n')
          macroValueEndPos++;
        std::string macroValue = define_macro.substr(
            macroValueInitPos, macroValueEndPos - macroValueInitPos);

        // Check whether the value is a valid integer
        std::string::const_iterator it = macroValue.begin();
        while (it != macroValue.end() && std::isdigit(*it))
          ++it;
        ROSE_ASSERT(!macroValue.empty() && it == macroValue.end());

        newExp = buildIntVal(atoi(macroValue.c_str()));
        SgNode *parent = old_exp->get_parent();
        bool replaced = false;
        if (parent != NULL && !isSgPragmaDeclaration(parent)) {
          replaceExpression(old_exp, newExp);
          replaced = true;
        }
        macro_replaced = true;
      }
    }
  }
  return newExp;
}

SgExpression *checkOmpExpressionClause(SgExpression *clause_expression,
                                       SgGlobal *global,
                                       omp_construct_enum clause_type) {
  SgExpression *newExp = clause_expression;
  // ordered (n): optional (n)
  if (clause_expression == NULL && clause_type == e_ordered_clause)
    return NULL;
  ROSE_ASSERT(clause_expression != NULL);
  // Keep unknown-type expressions as parsed instead of replacing macro-like
  // identifiers with literal values. Replacing these tokens loses source-level
  // fidelity for OpenMP clauses (e.g., num_threads(MACRO), map(...[0:N])).
  (void)global;
  (void)clause_type;
  return newExp;
}

//! A helper function to convert OpenMPIfClause modifier to SgClause if modifier
static SgOmpClause::omp_if_modifier_enum
toSgOmpClauseIfModifier(OpenMPIfClauseModifier modifier) {
  SgOmpClause::omp_if_modifier_enum result;
  switch (modifier) {
  case OMPC_IF_MODIFIER_parallel: {
    result = SgOmpClause::e_omp_if_parallel;
    break;
  }
  case OMPC_IF_MODIFIER_simd: {
    result = SgOmpClause::e_omp_if_simd;
    break;
  }
  case OMPC_IF_MODIFIER_cancel: {
    result = SgOmpClause::e_omp_if_cancel;
    break;
  }
  case OMPC_IF_MODIFIER_taskloop: {
    result = SgOmpClause::e_omp_if_taskloop;
    break;
  }
  case OMPC_IF_MODIFIER_target_enter_data: {
    result = SgOmpClause::e_omp_if_target_enter_data;
    break;
  }
  case OMPC_IF_MODIFIER_target_exit_data: {
    result = SgOmpClause::e_omp_if_target_exit_data;
    break;
  }
  case OMPC_IF_MODIFIER_task: {
    result = SgOmpClause::e_omp_if_task;
    break;
  }
  case OMPC_IF_MODIFIER_target_data: {
    result = SgOmpClause::e_omp_if_target_data;
    break;
  }
  case OMPC_IF_MODIFIER_target: {
    result = SgOmpClause::e_omp_if_target;
    break;
  }
  case OMPC_IF_MODIFIER_target_update: {
    result = SgOmpClause::e_omp_if_target_update;
    break;
  }
  case OMPC_IF_MODIFIER_unspecified: {
    result = SgOmpClause::e_omp_if_modifier_unknown;
    break;
  }
  default: {
    printf("error: unacceptable omp construct enum for if modifier "
           "conversion:%d\n",
           modifier);
    ROSE_ABORT();
  }
  }
  return result;
}

static SgOmpClause::omp_lastprivate_modifier_enum
toSgOmpClauseLastprivateModifier(OpenMPLastprivateClauseModifier modifier) {
  SgOmpClause::omp_lastprivate_modifier_enum result =
      SgOmpClause::e_omp_lastprivate_modifier_unspecified;
  switch (modifier) {
  case OMPC_LASTPRIVATE_MODIFIER_conditional: {
    result = SgOmpClause::e_omp_lastprivate_conditional;
    break;
  }
  case OMPC_LASTPRIVATE_MODIFIER_unspecified: {
    result = SgOmpClause::e_omp_lastprivate_modifier_unspecified;
    break;
  }
  default: {
    printf("error: unacceptable omp construct enum for lastprivate modifier "
           "conversion:%d\n",
           modifier);
    ROSE_ABORT();
  }
  }
  return result;
}

static SgOmpClause::omp_device_modifier_enum
toSgOmpClauseDeviceModifier(OpenMPDeviceClauseModifier modifier) {
  SgOmpClause::omp_device_modifier_enum result =
      SgOmpClause::e_omp_device_modifier_unspecified;
  switch (modifier) {
  case OMPC_DEVICE_MODIFIER_unspecified: {
    result = SgOmpClause::e_omp_device_modifier_unspecified;
    break;
  }
  case OMPC_DEVICE_MODIFIER_ancestor: {
    result = SgOmpClause::e_omp_device_modifier_ancestor;
    break;
  }
  case OMPC_DEVICE_MODIFIER_device_num: {
    result = SgOmpClause::e_omp_device_modifier_device_num;
    break;
  }
  default: {
    printf("error: unacceptable omp construct enum for device modifier "
           "conversion:%d\n",
           modifier);
    ROSE_ABORT();
  }
  }
  return result;
}

static SgOmpClause::omp_schedule_modifier_enum
toSgOmpClauseScheduleModifier(OpenMPScheduleClauseModifier modifier) {
  SgOmpClause::omp_schedule_modifier_enum result =
      SgOmpClause::e_omp_schedule_modifier_unspecified;
  switch (modifier) {
  case OMPC_SCHEDULE_MODIFIER_unspecified: {
    result = SgOmpClause::e_omp_schedule_modifier_unspecified;
    break;
  }
  case OMPC_SCHEDULE_MODIFIER_monotonic: {
    result = SgOmpClause::e_omp_schedule_modifier_monotonic;
    break;
  }
  case OMPC_SCHEDULE_MODIFIER_nonmonotonic: {
    result = SgOmpClause::e_omp_schedule_modifier_nonmonotonic;
    break;
  }
  case OMPC_SCHEDULE_MODIFIER_simd: {
    result = SgOmpClause::e_omp_schedule_modifier_simd;
    break;
  }
  default: {
    printf("error: unacceptable omp construct enum for schedule modifier "
           "conversion:%d\n",
           modifier);
    ROSE_ABORT();
  }
  }
  return result;
}

static SgOmpClause::omp_schedule_kind_enum
toSgOmpClauseScheduleKind(OpenMPScheduleClauseKind kind) {
  SgOmpClause::omp_schedule_kind_enum result =
      SgOmpClause::e_omp_schedule_kind_unspecified;
  switch (kind) {
  case OMPC_SCHEDULE_KIND_unspecified: {
    result = SgOmpClause::e_omp_schedule_kind_unspecified;
    break;
  }
  case OMPC_SCHEDULE_KIND_static: {
    result = SgOmpClause::e_omp_schedule_kind_static;
    break;
  }
  case OMPC_SCHEDULE_KIND_dynamic: {
    result = SgOmpClause::e_omp_schedule_kind_dynamic;
    break;
  }
  case OMPC_SCHEDULE_KIND_guided: {
    result = SgOmpClause::e_omp_schedule_kind_guided;
    break;
  }
  case OMPC_SCHEDULE_KIND_auto: {
    result = SgOmpClause::e_omp_schedule_kind_auto;
    break;
  }
  case OMPC_SCHEDULE_KIND_runtime: {
    result = SgOmpClause::e_omp_schedule_kind_runtime;
    break;
  }
  default: {
    printf("error: unacceptable omp construct enum for schedule kind "
           "conversion:%d\n",
           kind);
    ROSE_ABORT();
  }
  }
  return result;
}

static SgOmpClause::omp_defaultmap_behavior_enum
toSgOmpClauseDefaultmapBehavior(OpenMPDefaultmapClauseBehavior behavior) {
  SgOmpClause::omp_defaultmap_behavior_enum result =
      SgOmpClause::e_omp_defaultmap_behavior_unspecified;
  switch (behavior) {
  case OMPC_DEFAULTMAP_BEHAVIOR_alloc: {
    result = SgOmpClause::e_omp_defaultmap_behavior_alloc;
    break;
  }
  case OMPC_DEFAULTMAP_BEHAVIOR_to: {
    result = SgOmpClause::e_omp_defaultmap_behavior_to;
    break;
  }
  case OMPC_DEFAULTMAP_BEHAVIOR_from: {
    result = SgOmpClause::e_omp_defaultmap_behavior_from;
    break;
  }
  case OMPC_DEFAULTMAP_BEHAVIOR_tofrom: {
    result = SgOmpClause::e_omp_defaultmap_behavior_tofrom;
    break;
  }
  case OMPC_DEFAULTMAP_BEHAVIOR_firstprivate: {
    result = SgOmpClause::e_omp_defaultmap_behavior_firstprivate;
    break;
  }
  case OMPC_DEFAULTMAP_BEHAVIOR_none: {
    result = SgOmpClause::e_omp_defaultmap_behavior_none;
    break;
  }
  case OMPC_DEFAULTMAP_BEHAVIOR_default: {
    result = SgOmpClause::e_omp_defaultmap_behavior_default;
    break;
  }
  case OMPC_DEFAULTMAP_BEHAVIOR_present: {
    result = SgOmpClause::e_omp_defaultmap_behavior_present;
    break;
  }
  default: {
    printf("error: unacceptable omp construct enum for defaultmap behavior "
           "conversion:%d\n",
           behavior);
    ROSE_ABORT();
  }
  }
  return result;
}

static SgOmpClause::omp_map_operator_enum
toSgOmpClauseMapOperator(OpenMPMapClauseType at_op) {
  SgOmpClause::omp_map_operator_enum result = SgOmpClause::e_omp_map_unknown;
  switch (at_op) {
  case OMPC_MAP_TYPE_unspecified: {
    result = SgOmpClause::e_omp_map_unknown;
    break;
  }
  case OMPC_MAP_TYPE_tofrom: {
    result = SgOmpClause::e_omp_map_tofrom;
    break;
  }
  case OMPC_MAP_TYPE_to: {
    result = SgOmpClause::e_omp_map_to;
    break;
  }
  case OMPC_MAP_TYPE_from: {
    result = SgOmpClause::e_omp_map_from;
    break;
  }
  case OMPC_MAP_TYPE_alloc: {
    result = SgOmpClause::e_omp_map_alloc;
    break;
  }
  case OMPC_MAP_TYPE_storage: {
    result = SgOmpClause::e_omp_map_storage;
    break;
  }
  case OMPC_MAP_TYPE_release: {
    result = SgOmpClause::e_omp_map_release;
    break;
  }
  case OMPC_MAP_TYPE_delete: {
    result = SgOmpClause::e_omp_map_delete;
    break;
  }
  case OMPC_MAP_TYPE_present: {
    result = SgOmpClause::e_omp_map_present;
    break;
  }
  case OMPC_MAP_TYPE_self: {
    result = SgOmpClause::e_omp_map_self;
    break;
  }
  default: {
    result = SgOmpClause::e_omp_map_unknown;
    break;
  }
  }
  return result;
}

static SgOmpClause::omp_map_modifier_enum
toSgOmpClauseMapModifier(OpenMPMapClauseModifier modifier) {
  SgOmpClause::omp_map_modifier_enum result =
      SgOmpClause::e_omp_map_modifier_unspecified;
  switch (modifier) {
  case OMPC_MAP_MODIFIER_always: {
    result = SgOmpClause::e_omp_map_modifier_always;
    break;
  }
  case OMPC_MAP_MODIFIER_close: {
    result = SgOmpClause::e_omp_map_modifier_close;
    break;
  }
  case OMPC_MAP_MODIFIER_present: {
    result = SgOmpClause::e_omp_map_modifier_present;
    break;
  }
  case OMPC_MAP_MODIFIER_self: {
    result = SgOmpClause::e_omp_map_modifier_self;
    break;
  }
  case OMPC_MAP_MODIFIER_mapper: {
    result = SgOmpClause::e_omp_map_modifier_mapper;
    break;
  }
  case OMPC_MAP_MODIFIER_iterator: {
    result = SgOmpClause::e_omp_map_modifier_iterator;
    break;
  }
  case OMPC_MAP_MODIFIER_unspecified: {
    result = SgOmpClause::e_omp_map_modifier_unspecified;
    break;
  }
  default: {
    result = SgOmpClause::e_omp_map_modifier_unspecified;
    break;
  }
  }
  return result;
}

static SgOmpClause::omp_declare_mapper_identifier_enum
toSgOmpClauseDeclareMapperIdentifier(
    OpenMPDeclareMapperDirectiveIdentifier identifier) {
  SgOmpClause::omp_declare_mapper_identifier_enum result =
      SgOmpClause::e_omp_declare_mapper_identifier_unspecified;
  switch (identifier) {
  case OMPD_DECLARE_MAPPER_IDENTIFIER_default: {
    result = SgOmpClause::e_omp_declare_mapper_identifier_default;
    break;
  }
  case OMPD_DECLARE_MAPPER_IDENTIFIER_user: {
    result = SgOmpClause::e_omp_declare_mapper_identifier_user;
    break;
  }
  case OMPD_DECLARE_MAPPER_IDENTIFIER_unspecified: {
    result = SgOmpClause::e_omp_declare_mapper_identifier_unspecified;
    break;
  }
  default: {
    result = SgOmpClause::e_omp_declare_mapper_identifier_unspecified;
    break;
  }
  }
  return result;
}

struct NormalizedDeclareMapperData {
  SgOmpClause::omp_declare_mapper_identifier_enum identifier =
      SgOmpClause::e_omp_declare_mapper_identifier_unspecified;
  std::string user_defined_identifier;
  std::string mapper_type;
  std::string mapper_variable;
};

static size_t findStandaloneColonPosition(const std::string &value) {
  for (size_t i = 0; i < value.size(); ++i) {
    if (value[i] != ':') {
      continue;
    }
    const bool has_prev_colon = i > 0 && value[i - 1] == ':';
    const bool has_next_colon = i + 1 < value.size() && value[i + 1] == ':';
    if (!has_prev_colon && !has_next_colon) {
      return i;
    }
  }

  return std::string::npos;
}

static bool isCaseInsensitiveLiteral(const std::string &value,
                                     const char *literal) {
  if (literal == nullptr) {
    return false;
  }

  const size_t literal_length = std::strlen(literal);
  if (value.size() != literal_length) {
    return false;
  }

  for (size_t i = 0; i < literal_length; ++i) {
    const unsigned char lhs = static_cast<unsigned char>(value[i]);
    const unsigned char rhs = static_cast<unsigned char>(literal[i]);
    if (std::tolower(lhs) != std::tolower(rhs)) {
      return false;
    }
  }

  return true;
}

static bool isSimpleMapperIdentifier(const std::string &value) {
  if (value.empty()) {
    return false;
  }

  const unsigned char first = static_cast<unsigned char>(value.front());
  if (!(std::isalpha(first) || first == '_')) {
    return false;
  }

  for (size_t i = 1; i < value.size(); ++i) {
    const unsigned char ch = static_cast<unsigned char>(value[i]);
    if (!(std::isalnum(ch) || ch == '_')) {
      return false;
    }
  }

  return true;
}

static bool assignDeclareMapperIdentifierFromText(
    const std::string &identifier_text,
    SgOmpClause::omp_declare_mapper_identifier_enum &identifier,
    std::string &user_defined_identifier) {
  const std::string trimmed_identifier = trimWhitespaceCopy(identifier_text);
  if (trimmed_identifier.empty()) {
    return false;
  }

  if (isCaseInsensitiveLiteral(trimmed_identifier, "default")) {
    identifier = SgOmpClause::e_omp_declare_mapper_identifier_default;
    user_defined_identifier.clear();
    return true;
  }

  if (!isSimpleMapperIdentifier(trimmed_identifier)) {
    return false;
  }

  identifier = SgOmpClause::e_omp_declare_mapper_identifier_user;
  user_defined_identifier = trimmed_identifier;
  return true;
}

static bool splitDeclareMapperTypeAndVariable(const std::string &type_var_text,
                                              std::string &type_text,
                                              std::string &variable_text) {
  const std::string trimmed = trimWhitespaceCopy(type_var_text);
  if (trimmed.empty()) {
    return false;
  }

  const size_t fortran_separator_pos = trimmed.find("::");
  if (fortran_separator_pos != std::string::npos) {
    const std::string candidate_type =
        trimWhitespaceCopy(trimmed.substr(0, fortran_separator_pos));
    const std::string candidate_variable =
        trimWhitespaceCopy(trimmed.substr(fortran_separator_pos + 2));
    if (!candidate_type.empty() && !candidate_variable.empty()) {
      type_text = candidate_type;
      variable_text = candidate_variable;
      return true;
    }
  }

  for (size_t i = trimmed.size(); i-- > 0;) {
    const char ch = trimmed[i];
    if (ch != ' ' && ch != '\t' && ch != '*' && ch != '&') {
      continue;
    }

    const std::string candidate_type =
        trimWhitespaceCopy(trimmed.substr(0, i + 1));
    const std::string candidate_variable =
        trimWhitespaceCopy(trimmed.substr(i + 1));
    if (!candidate_type.empty() && !candidate_variable.empty()) {
      type_text = candidate_type;
      variable_text = candidate_variable;
      return true;
    }
  }

  return false;
}

static void
inferDeclareMapperIdentifierFromType(NormalizedDeclareMapperData &mapper_data) {
  if (!(mapper_data.identifier ==
            SgOmpClause::e_omp_declare_mapper_identifier_unspecified ||
        (mapper_data.identifier ==
             SgOmpClause::e_omp_declare_mapper_identifier_user &&
         mapper_data.user_defined_identifier.empty()))) {
    return;
  }

  const size_t colon_pos = findStandaloneColonPosition(mapper_data.mapper_type);
  if (colon_pos == std::string::npos) {
    return;
  }

  const std::string candidate_identifier =
      trimWhitespaceCopy(mapper_data.mapper_type.substr(0, colon_pos));
  const std::string candidate_type =
      trimWhitespaceCopy(mapper_data.mapper_type.substr(colon_pos + 1));
  if (candidate_type.empty()) {
    return;
  }

  if (assignDeclareMapperIdentifierFromText(
          candidate_identifier, mapper_data.identifier,
          mapper_data.user_defined_identifier)) {
    mapper_data.mapper_type = candidate_type;
  }
}

static NormalizedDeclareMapperData
normalizeDeclareMapperData(OpenMPDeclareMapperDirective *mapper_directive) {
  NormalizedDeclareMapperData mapper_data;
  if (mapper_directive == nullptr) {
    return mapper_data;
  }

  mapper_data.identifier =
      toSgOmpClauseDeclareMapperIdentifier(mapper_directive->getIdentifier());
  mapper_data.user_defined_identifier =
      trimWhitespaceCopy(mapper_directive->getUserDefinedIdentifier());
  mapper_data.mapper_type =
      trimWhitespaceCopy(mapper_directive->getDeclareMapperType());
  mapper_data.mapper_variable =
      trimWhitespaceCopy(mapper_directive->getDeclareMapperVar());

  inferDeclareMapperIdentifierFromType(mapper_data);

  if (mapper_data.mapper_type.empty() && !mapper_data.mapper_variable.empty()) {
    std::string split_type;
    std::string split_variable;
    if (splitDeclareMapperTypeAndVariable(mapper_data.mapper_variable,
                                          split_type, split_variable)) {
      mapper_data.mapper_type = split_type;
      mapper_data.mapper_variable = split_variable;
      inferDeclareMapperIdentifierFromType(mapper_data);
    }
  }

  if (mapper_data.mapper_variable.empty() && !mapper_data.mapper_type.empty()) {
    std::string split_type;
    std::string split_variable;
    if (splitDeclareMapperTypeAndVariable(mapper_data.mapper_type, split_type,
                                          split_variable)) {
      mapper_data.mapper_type = split_type;
      mapper_data.mapper_variable = split_variable;
      inferDeclareMapperIdentifierFromType(mapper_data);
    }
  }

  if (mapper_data.identifier ==
          SgOmpClause::e_omp_declare_mapper_identifier_user &&
      mapper_data.user_defined_identifier.empty()) {
    mapper_data.identifier =
        SgOmpClause::e_omp_declare_mapper_identifier_unspecified;
  }

  return mapper_data;
}

static SgOmpClause::omp_defaultmap_category_enum
toSgOmpClauseDefaultmapCategory(OpenMPDefaultmapClauseCategory category) {
  SgOmpClause::omp_defaultmap_category_enum result =
      SgOmpClause::e_omp_defaultmap_category_unspecified;
  switch (category) {
  case OMPC_DEFAULTMAP_CATEGORY_unspecified: {
    result = SgOmpClause::e_omp_defaultmap_category_unspecified;
    break;
  }
  case OMPC_DEFAULTMAP_CATEGORY_scalar: {
    result = SgOmpClause::e_omp_defaultmap_category_scalar;
    break;
  }
  case OMPC_DEFAULTMAP_CATEGORY_aggregate: {
    result = SgOmpClause::e_omp_defaultmap_category_aggregate;
    break;
  }
  case OMPC_DEFAULTMAP_CATEGORY_pointer: {
    result = SgOmpClause::e_omp_defaultmap_category_pointer;
    break;
  }
  case OMPC_DEFAULTMAP_CATEGORY_allocatable: {
    result = SgOmpClause::e_omp_defaultmap_category_allocatable;
    break;
  }
  default: {
    printf("error: unacceptable omp construct enum for defaultmap category "
           "conversion:%d\n",
           category);
    ROSE_ABORT();
  }
  }
  return result;
}

static SgOmpClause::omp_dist_schedule_kind_enum
toSgOmpClauseDistScheduleKind(OpenMPDistScheduleClauseKind kind) {
  SgOmpClause::omp_dist_schedule_kind_enum result =
      SgOmpClause::e_omp_dist_schedule_kind_unspecified;
  switch (kind) {
  case OMPC_DIST_SCHEDULE_KIND_static: {
    result = SgOmpClause::e_omp_dist_schedule_kind_static;
    break;
  }
  default: {
    printf("error: unacceptable omp construct enum for dist_schedule kind "
           "conversion:%d\n",
           kind);
    ROSE_ABORT();
  }
  }
  return result;
}

static SgOmpClause::omp_when_context_kind_enum
toSgOmpDeclareTargetDeviceTypeKind(OpenMPDeviceTypeClauseKind kind) {
  SgOmpClause::omp_when_context_kind_enum result =
      SgOmpClause::e_omp_when_context_kind_unknown;
  switch (kind) {
  case OMPC_DEVICE_TYPE_host: {
    result = SgOmpClause::e_omp_when_context_kind_host;
    break;
  }
  case OMPC_DEVICE_TYPE_nohost: {
    result = SgOmpClause::e_omp_when_context_kind_nohost;
    break;
  }
  case OMPC_DEVICE_TYPE_any: {
    result = SgOmpClause::e_omp_when_context_kind_any;
    break;
  }
  case OMPC_DEVICE_TYPE_unknown: {
    result = SgOmpClause::e_omp_when_context_kind_unknown;
    break;
  }
  default: {
    printf("error: unacceptable omp device_type kind conversion:%d\n", kind);
    ROSE_ABORT();
  }
  }
  return result;
}

static SgOmpClause::omp_linear_modifier_enum
toSgOmpClauseLinearModifier(OpenMPLinearClauseModifier modifier) {
  SgOmpClause::omp_linear_modifier_enum result =
      SgOmpClause::e_omp_linear_modifier_unspecified;
  switch (modifier) {
  case OMPC_LINEAR_MODIFIER_unspecified: {
    result = SgOmpClause::e_omp_linear_modifier_unspecified;
    break;
  }
  case OMPC_LINEAR_MODIFIER_ref: {
    result = SgOmpClause::e_omp_linear_modifier_ref;
    break;
  }
  case OMPC_LINEAR_MODIFIER_val: {
    result = SgOmpClause::e_omp_linear_modifier_val;
    break;
  }
  case OMPC_LINEAR_MODIFIER_uval: {
    result = SgOmpClause::e_omp_linear_modifier_uval;
    break;
  }
  default: {
    printf("error: unacceptable omp construct enum for linear modifier "
           "conversion:%d\n",
           modifier);
    ROSE_ABORT();
  }
  }
  return result;
}

//! A helper function to convert OpenMPIR reduction modifier to SgClause
//! reduction modifier
static SgOmpClause::omp_reduction_modifier_enum
toSgOmpClauseReductionModifier(OpenMPReductionClauseModifier modifier) {
  SgOmpClause::omp_reduction_modifier_enum result;
  switch (modifier) {
  case OMPC_REDUCTION_MODIFIER_inscan: {
    result = SgOmpClause::e_omp_reduction_inscan;
    break;
  }
  case OMPC_REDUCTION_MODIFIER_task: {
    result = SgOmpClause::e_omp_reduction_task;
    break;
  }
  case OMPC_REDUCTION_MODIFIER_default: {
    result = SgOmpClause::e_omp_reduction_default;
    break;
  }
  case OMPC_REDUCTION_MODIFIER_unspecified: {
    result = SgOmpClause::e_omp_reduction_modifier_unknown;
    break;
  }
  default: {
    printf("error: unacceptable omp construct enum for reduction modifier "
           "conversion:%d\n",
           modifier);
    ROSE_ABORT();
  }
  }
  return result;
}

//! A helper function to convert OpenMPIR reduction identifier to SgClause
//! reduction identifier
static SgOmpClause::omp_reduction_identifier_enum
toSgOmpClauseReductionIdentifier(OpenMPReductionClauseIdentifier identifier) {
  SgOmpClause::omp_reduction_identifier_enum result =
      SgOmpClause::e_omp_reduction_unknown;
  switch (identifier) {
  case OMPC_REDUCTION_IDENTIFIER_plus: //+
  {
    result = SgOmpClause::e_omp_reduction_plus;
    break;
  }
  case OMPC_REDUCTION_IDENTIFIER_mul: //*
  {
    result = SgOmpClause::e_omp_reduction_mul;
    break;
  }
  case OMPC_REDUCTION_IDENTIFIER_minus: // -
  {
    result = SgOmpClause::e_omp_reduction_minus;
    break;
  }
    // C/C++ only
  case OMPC_REDUCTION_IDENTIFIER_bitand: // &
  {
    result = SgOmpClause::e_omp_reduction_bitand;
    break;
  }
  case OMPC_REDUCTION_IDENTIFIER_bitor: // |
  {
    result = SgOmpClause::e_omp_reduction_bitor;
    break;
  }
  case OMPC_REDUCTION_IDENTIFIER_bitxor: // ^
  {
    result = SgOmpClause::e_omp_reduction_bitxor;
    break;
  }
  case OMPC_REDUCTION_IDENTIFIER_logand: // &&
  {
    result = SgOmpClause::e_omp_reduction_logand;
    break;
  }
  case OMPC_REDUCTION_IDENTIFIER_logor: // ||
  {
    result = SgOmpClause::e_omp_reduction_logor;
    break;
  }
  case OMPC_REDUCTION_IDENTIFIER_eqv: // fortran .eqv.
  {
    result = SgOmpClause::e_omp_reduction_eqv;
    break;
  }
  case OMPC_REDUCTION_IDENTIFIER_neqv: // fortran .neqv.
  {
    result = SgOmpClause::e_omp_reduction_neqv;
    break;
  }
  case OMPC_REDUCTION_IDENTIFIER_max: {
    result = SgOmpClause::e_omp_reduction_max;
    break;
  }
  case OMPC_REDUCTION_IDENTIFIER_min: {
    result = SgOmpClause::e_omp_reduction_min;
    break;
  }
  case OMPC_REDUCTION_IDENTIFIER_user: {
    result = SgOmpClause::e_omp_reduction_user_defined_identifier;
    break;
  }
  default: {
    printf("error: unacceptable omp construct enum for reduction operator "
           "conversion:%d\n",
           identifier);
    ROSE_ABORT();
    break;
  }
  }
  ROSE_ASSERT(result != SgOmpClause::e_omp_reduction_unknown);
  return result;
}

//! A helper function to convert OpenMPIR reduction identifier to SgClause
//! reduction identifier
static SgOmpClause::omp_in_reduction_identifier_enum
toSgOmpClauseInReductionIdentifier(
    OpenMPInReductionClauseIdentifier identifier) {
  SgOmpClause::omp_in_reduction_identifier_enum result =
      SgOmpClause::e_omp_in_reduction_identifier_unspecified;
  switch (identifier) {
  case OMPC_IN_REDUCTION_IDENTIFIER_plus: //+
  {
    result = SgOmpClause::e_omp_in_reduction_identifier_plus;
    break;
  }
  case OMPC_IN_REDUCTION_IDENTIFIER_mul: //*
  {
    result = SgOmpClause::e_omp_in_reduction_identifier_mul;
    break;
  }
  case OMPC_IN_REDUCTION_IDENTIFIER_minus: // -
  {
    result = SgOmpClause::e_omp_in_reduction_identifier_minus;
    break;
  }
    // C/C++ only
  case OMPC_IN_REDUCTION_IDENTIFIER_bitand: // &
  {
    result = SgOmpClause::e_omp_in_reduction_identifier_bitand;
    break;
  }
  case OMPC_IN_REDUCTION_IDENTIFIER_bitor: // |
  {
    result = SgOmpClause::e_omp_in_reduction_identifier_bitor;
    break;
  }
  case OMPC_IN_REDUCTION_IDENTIFIER_bitxor: // ^
  {
    result = SgOmpClause::e_omp_in_reduction_identifier_bitxor;
    break;
  }
  case OMPC_IN_REDUCTION_IDENTIFIER_logand: // &&
  {
    result = SgOmpClause::e_omp_in_reduction_identifier_logand;
    break;
  }
  case OMPC_IN_REDUCTION_IDENTIFIER_logor: // ||
  {
    result = SgOmpClause::e_omp_in_reduction_identifier_logor;
    break;
  }
  case OMPC_IN_REDUCTION_IDENTIFIER_eqv: // fortran .eqv.
  {
    result = SgOmpClause::e_omp_in_reduction_identifier_eqv;
    break;
  }
  case OMPC_IN_REDUCTION_IDENTIFIER_neqv: // fortran .neqv.
  {
    result = SgOmpClause::e_omp_in_reduction_identifier_neqv;
    break;
  }
  case OMPC_IN_REDUCTION_IDENTIFIER_max: {
    result = SgOmpClause::e_omp_in_reduction_identifier_max;
    break;
  }
  case OMPC_IN_REDUCTION_IDENTIFIER_min: {
    result = SgOmpClause::e_omp_in_reduction_identifier_min;
    break;
  }
  case OMPC_IN_REDUCTION_IDENTIFIER_user: {
    result = SgOmpClause::e_omp_in_reduction_user_defined_identifier;
    break;
  }
  default: {
    printf("error: unacceptable omp construct enum for in_reduction operator "
           "conversion:%d\n",
           identifier);
    ROSE_ABORT();
    break;
  }
  }
  ROSE_ASSERT(result != SgOmpClause::e_omp_in_reduction_identifier_unspecified);
  return result;
}

//! A helper function to convert OpenMPIR reduction identifier to SgClause
//! reduction identifier
static SgOmpClause::omp_task_reduction_identifier_enum
toSgOmpClauseTaskReductionIdentifier(
    OpenMPTaskReductionClauseIdentifier identifier) {
  SgOmpClause::omp_task_reduction_identifier_enum result =
      SgOmpClause::e_omp_task_reduction_identifier_unspecified;
  switch (identifier) {
  case OMPC_TASK_REDUCTION_IDENTIFIER_plus: //+
  {
    result = SgOmpClause::e_omp_task_reduction_identifier_plus;
    break;
  }
  case OMPC_TASK_REDUCTION_IDENTIFIER_mul: //*
  {
    result = SgOmpClause::e_omp_task_reduction_identifier_mul;
    break;
  }
  case OMPC_TASK_REDUCTION_IDENTIFIER_minus: // -
  {
    result = SgOmpClause::e_omp_task_reduction_identifier_minus;
    break;
  }
    // C/C++ only
  case OMPC_TASK_REDUCTION_IDENTIFIER_bitand: // &
  {
    result = SgOmpClause::e_omp_task_reduction_identifier_bitand;
    break;
  }
  case OMPC_TASK_REDUCTION_IDENTIFIER_bitor: // |
  {
    result = SgOmpClause::e_omp_task_reduction_identifier_bitor;
    break;
  }
  case OMPC_TASK_REDUCTION_IDENTIFIER_bitxor: // ^
  {
    result = SgOmpClause::e_omp_task_reduction_identifier_bitxor;
    break;
  }
  case OMPC_TASK_REDUCTION_IDENTIFIER_logand: // &&
  {
    result = SgOmpClause::e_omp_task_reduction_identifier_logand;
    break;
  }
  case OMPC_TASK_REDUCTION_IDENTIFIER_logor: // ||
  {
    result = SgOmpClause::e_omp_task_reduction_identifier_logor;
    break;
  }
  case OMPC_TASK_REDUCTION_IDENTIFIER_eqv: // fortran .eqv.
  {
    result = SgOmpClause::e_omp_task_reduction_identifier_eqv;
    break;
  }
  case OMPC_TASK_REDUCTION_IDENTIFIER_neqv: // fortran .neqv.
  {
    result = SgOmpClause::e_omp_task_reduction_identifier_neqv;
    break;
  }
  case OMPC_TASK_REDUCTION_IDENTIFIER_max: {
    result = SgOmpClause::e_omp_task_reduction_identifier_max;
    break;
  }
  case OMPC_TASK_REDUCTION_IDENTIFIER_min: {
    result = SgOmpClause::e_omp_task_reduction_identifier_min;
    break;
  }
  case OMPC_TASK_REDUCTION_IDENTIFIER_user: {
    result = SgOmpClause::e_omp_task_reduction_user_defined_identifier;
    break;
  }
  default: {
    printf("error: unacceptable omp construct enum for task_reduction operator "
           "conversion:%d\n",
           identifier);
    ROSE_ABORT();
    break;
  }
  }
  ROSE_ASSERT(result !=
              SgOmpClause::e_omp_task_reduction_identifier_unspecified);
  return result;
}

//! A helper function to convert OpenMPIR ALLOCATE allocator to SgClause
//! ALLOCATE modifier
static SgOmpClause::omp_allocate_modifier_enum
toSgOmpClauseAllocateAllocator(OpenMPAllocateClauseAllocator allocator) {
  SgOmpClause::omp_allocate_modifier_enum result;
  switch (allocator) {
  case OMPC_ALLOCATE_ALLOCATOR_default: {
    result = SgOmpClause::e_omp_allocate_default_mem_alloc;
    break;
  }
  case OMPC_ALLOCATE_ALLOCATOR_large_cap: {
    result = SgOmpClause::e_omp_allocate_large_cap_mem_alloc;
    break;
  }
  case OMPC_ALLOCATE_ALLOCATOR_cons_mem: {
    result = SgOmpClause::e_omp_allocate_const_mem_alloc;
    break;
  }
  case OMPC_ALLOCATE_ALLOCATOR_high_bw: {
    result = SgOmpClause::e_omp_allocate_high_bw_mem_alloc;
    break;
  }
  case OMPC_ALLOCATE_ALLOCATOR_low_lat: {
    result = SgOmpClause::e_omp_allocate_low_lat_mem_alloc;
    break;
  }
  case OMPC_ALLOCATE_ALLOCATOR_cgroup: {
    result = SgOmpClause::e_omp_allocate_cgroup_mem_alloc;
    break;
  }
  case OMPC_ALLOCATE_ALLOCATOR_pteam: {
    result = SgOmpClause::e_omp_allocate_pteam_mem_alloc;
    break;
  }
  case OMPC_ALLOCATE_ALLOCATOR_thread: {
    result = SgOmpClause::e_omp_allocate_thread_mem_alloc;
    break;
  }
  case OMPC_ALLOCATE_ALLOCATOR_user: {
    result = SgOmpClause::e_omp_allocate_user_defined_modifier;
    break;
  }
  case OMPC_ALLOCATE_ALLOCATOR_unspecified: {
    result = SgOmpClause::e_omp_allocate_modifier_unknown;
    break;
  }
  default: {
    printf("error: unacceptable omp construct enum for allocate modifier "
           "conversion:%d\n",
           allocator);
    ROSE_ABORT();
    break;
  }
  }

  return result;
}

//! A helper function to convert OpenMPIR ALLOCATOR allocator to SgClause
//! ALLOCATOR modifier
static SgOmpClause::omp_allocator_modifier_enum
toSgOmpClauseAllocatorAllocator(OpenMPAllocatorClauseAllocator allocator) {
  SgOmpClause::omp_allocator_modifier_enum result;
  switch (allocator) {
  case OMPC_ALLOCATOR_ALLOCATOR_default: {
    result = SgOmpClause::e_omp_allocator_default_mem_alloc;
    break;
  }
  case OMPC_ALLOCATOR_ALLOCATOR_large_cap: {
    result = SgOmpClause::e_omp_allocator_large_cap_mem_alloc;
    break;
  }
  case OMPC_ALLOCATOR_ALLOCATOR_cons_mem: {
    result = SgOmpClause::e_omp_allocator_const_mem_alloc;
    break;
  }
  case OMPC_ALLOCATOR_ALLOCATOR_high_bw: {
    result = SgOmpClause::e_omp_allocator_high_bw_mem_alloc;
    break;
  }
  case OMPC_ALLOCATOR_ALLOCATOR_low_lat: {
    result = SgOmpClause::e_omp_allocator_low_lat_mem_alloc;
    break;
  }
  case OMPC_ALLOCATOR_ALLOCATOR_cgroup: {
    result = SgOmpClause::e_omp_allocator_cgroup_mem_alloc;
    break;
  }
  case OMPC_ALLOCATOR_ALLOCATOR_pteam: {
    result = SgOmpClause::e_omp_allocator_pteam_mem_alloc;
    break;
  }
  case OMPC_ALLOCATOR_ALLOCATOR_thread: {
    result = SgOmpClause::e_omp_allocator_thread_mem_alloc;
    break;
  }
  case OMPC_ALLOCATOR_ALLOCATOR_user: {
    result = SgOmpClause::e_omp_allocator_user_defined_modifier;
    break;
  }
  case OMPC_ALLOCATOR_ALLOCATOR_unknown: {
    result = SgOmpClause::e_omp_allocator_modifier_unknown;
    break;
  }
  default: {
    printf("error: unacceptable omp construct enum for allocator modifier "
           "conversion:%d\n",
           allocator);
    ROSE_ABORT();
    break;
  }
  }

  return result;
}

//! A helper function to convert OpenMPIR TO kind to SgClause TO kind
static SgOmpClause::omp_to_kind_enum
toSgOmpClauseToKind(OpenMPToClauseKind kind) {
  SgOmpClause::omp_to_kind_enum result;
  switch (kind) {
  case OMPC_TO_mapper: {
    result = SgOmpClause::e_omp_to_kind_mapper;
    break;
  }
  case OMPC_TO_iterator: {
    result = SgOmpClause::e_omp_to_kind_iterator;
    break;
  }
  case OMPC_TO_present: {
    result = SgOmpClause::e_omp_to_kind_present;
    break;
  }

  case OMPC_TO_unspecified: {
    result = SgOmpClause::e_omp_to_kind_unknown;
    break;
  }
  default: {
    printf("error: unacceptable omp construct enum for TO kind conversion:%d\n",
           kind);
    ROSE_ABORT();
    break;
  }
  }

  return result;
}

//! A helper function to convert OpenMPIR FROM kind to SgClause FROM kind
static SgOmpClause::omp_from_kind_enum
toSgOmpClauseFromKind(OpenMPFromClauseKind kind) {
  SgOmpClause::omp_from_kind_enum result;
  switch (kind) {
  case OMPC_FROM_mapper: {
    result = SgOmpClause::e_omp_from_kind_mapper;
    break;
  }
  case OMPC_FROM_iterator: {
    result = SgOmpClause::e_omp_from_kind_iterator;
    break;
  }
  case OMPC_FROM_present: {
    result = SgOmpClause::e_omp_from_kind_present;
    break;
  }

  case OMPC_FROM_unspecified: {
    result = SgOmpClause::e_omp_from_kind_unknown;
    break;
  }
  default: {
    printf(
        "error: unacceptable omp construct enum for FROM kind conversion:%d\n",
        kind);
    ROSE_ABORT();
    break;
  }
  }

  return result;
}

//! A helper function to convert OpenMPIR uses_allocator allocator to SgClause
//! uses_allocator allocator
static SgOmpClause::omp_uses_allocators_allocator_enum
toSgOmpClauseUsesAllocatorsAllocator(
    OpenMPUsesAllocatorsClauseAllocator allocator) {
  SgOmpClause::omp_uses_allocators_allocator_enum result;
  switch (allocator) {
  case OMPC_USESALLOCATORS_ALLOCATOR_default: {
    result = SgOmpClause::e_omp_uses_allocators_allocator_default_mem_alloc;
    break;
  }
  case OMPC_USESALLOCATORS_ALLOCATOR_large_cap: {
    result = SgOmpClause::e_omp_uses_allocators_allocator_large_cap_mem_alloc;
    break;
  }
  case OMPC_USESALLOCATORS_ALLOCATOR_cons_mem: {
    result = SgOmpClause::e_omp_uses_allocators_allocator_const_mem_alloc;
    break;
  }
  case OMPC_USESALLOCATORS_ALLOCATOR_high_bw: {
    result = SgOmpClause::e_omp_uses_allocators_allocator_high_bw_mem_alloc;
    break;
  }
  case OMPC_USESALLOCATORS_ALLOCATOR_low_lat: {
    result = SgOmpClause::e_omp_uses_allocators_allocator_low_lat_mem_alloc;
    break;
  }
  case OMPC_USESALLOCATORS_ALLOCATOR_cgroup: {
    result = SgOmpClause::e_omp_uses_allocators_allocator_cgroup_mem_alloc;
    break;
  }
  case OMPC_USESALLOCATORS_ALLOCATOR_pteam: {
    result = SgOmpClause::e_omp_uses_allocators_allocator_pteam_mem_alloc;
    break;
  }
  case OMPC_USESALLOCATORS_ALLOCATOR_thread: {
    result = SgOmpClause::e_omp_uses_allocators_allocator_thread_mem_alloc;
    break;
  }
  case OMPC_USESALLOCATORS_ALLOCATOR_user: {
    result = SgOmpClause::e_omp_uses_allocators_allocator_user_defined;
    break;
  }
  case OMPC_USESALLOCATORS_ALLOCATOR_unknown: {
    result = SgOmpClause::e_omp_uses_allocators_allocator_unknown;
    break;
  }
  default: {
    printf("error: unacceptable omp construct enum for allocator modifier "
           "conversion:%d\n",
           allocator);
    ROSE_ABORT();
    break;
  }
  }

  return result;
}

static SgOmpClause::omp_depobj_modifier_enum
toSgOmpClauseDepobjModifierType(OpenMPDepobjUpdateClauseDependeceType type) {
  SgOmpClause::omp_depobj_modifier_enum result =
      SgOmpClause::e_omp_depobj_modifier_unknown;
  switch (type) {
  case OMPC_DEPOBJ_UPDATE_DEPENDENCE_TYPE_in: {
    result = SgOmpClause::e_omp_depobj_modifier_in;
    break;
  }
  case OMPC_DEPOBJ_UPDATE_DEPENDENCE_TYPE_out: {
    result = SgOmpClause::e_omp_depobj_modifier_out;
    break;
  }
  case OMPC_DEPOBJ_UPDATE_DEPENDENCE_TYPE_inout: {
    result = SgOmpClause::e_omp_depobj_modifier_inout;
    break;
  }
  case OMPC_DEPOBJ_UPDATE_DEPENDENCE_TYPE_mutexinoutset: {
    result = SgOmpClause::e_omp_depobj_modifier_mutexinoutset;
    break;
  }
  case OMPC_DEPOBJ_UPDATE_DEPENDENCE_TYPE_depobj: {
    result = SgOmpClause::e_omp_depobj_modifier_depobj;
    break;
  }
  case OMPC_DEPOBJ_UPDATE_DEPENDENCE_TYPE_sink: {
    result = SgOmpClause::e_omp_depobj_modifier_sink;
    break;
  }
  case OMPC_DEPOBJ_UPDATE_DEPENDENCE_TYPE_source: {
    result = SgOmpClause::e_omp_depobj_modifier_source;
    break;
  }
  default: {
    printf("error: unacceptable omp construct enum for dependence type "
           "conversion:%d\n",
           type);
    ROSE_ABORT();
    break;
  }
  }
  return result;
}

static SgOmpClause::omp_dependence_type_enum
toSgOmpClauseDependenceType(OpenMPDependClauseType type) {
  SgOmpClause::omp_dependence_type_enum result =
      SgOmpClause::e_omp_depend_unspecified;
  switch (type) {
  case OMPC_DEPENDENCE_TYPE_in: {
    result = SgOmpClause::e_omp_depend_in;
    break;
  }
  case OMPC_DEPENDENCE_TYPE_out: {
    result = SgOmpClause::e_omp_depend_out;
    break;
  }
  case OMPC_DEPENDENCE_TYPE_inout: {
    result = SgOmpClause::e_omp_depend_inout;
    break;
  }
  case OMPC_DEPENDENCE_TYPE_inoutset: {
    result = SgOmpClause::e_omp_depend_inoutset;
    break;
  }
  case OMPC_DEPENDENCE_TYPE_mutexinoutset: {
    result = SgOmpClause::e_omp_depend_mutexinoutset;
    break;
  }
  case OMPC_DEPENDENCE_TYPE_depobj: {
    result = SgOmpClause::e_omp_depend_depobj;
    break;
  }
  case OMPC_DEPENDENCE_TYPE_source: {
    result = SgOmpClause::e_omp_depend_source;
    break;
  }
  case OMPC_DEPENDENCE_TYPE_sink: {
    result = SgOmpClause::e_omp_depend_sink;
    break;
  }
  default: {
    printf("error: unacceptable omp construct enum for dependence type "
           "conversion:%d\n",
           type);
    ROSE_ABORT();
    break;
  }
  }
  return result;
}

static SgOmpClause::omp_depend_modifier_enum
toSgOmpClauseDependModifier(OpenMPDependClauseModifier modifier) {
  SgOmpClause::omp_depend_modifier_enum result =
      SgOmpClause::e_omp_depend_modifier_unspecified;
  switch (modifier) {
  case OMPC_DEPEND_MODIFIER_unspecified: {
    result = SgOmpClause::e_omp_depend_modifier_unspecified;
    break;
  }
  case OMPC_DEPEND_MODIFIER_iterator: {
    result = SgOmpClause::e_omp_depend_modifier_iterator;
    break;
  }
  default: {
    printf("error: unacceptable omp construct enum for depend modifier "
           "conversion:%d\n",
           modifier);
    ROSE_ABORT();
    break;
  }
  }
  return result;
}

static SgOmpClause::omp_affinity_modifier_enum
toSgOmpClauseAffinityModifier(OpenMPAffinityClauseModifier modifier) {
  SgOmpClause::omp_affinity_modifier_enum result =
      SgOmpClause::e_omp_affinity_modifier_unspecified;
  switch (modifier) {
  case OMPC_AFFINITY_MODIFIER_unspecified: {
    result = SgOmpClause::e_omp_affinity_modifier_unspecified;
    break;
  }
  case OMPC_AFFINITY_MODIFIER_iterator: {
    result = SgOmpClause::e_omp_affinity_modifier_iterator;
    break;
  }
  default: {
    printf("error: unacceptable omp construct enum for affinity modifier "
           "conversion:%d\n",
           modifier);
    ROSE_ABORT();
    break;
  }
  }
  return result;
}

//! Convert omp_pragma_list to SgOmpxxx nodes
void OpenMPIRToSageAST(SgSourceFile *sageFilePtr) {
  list<SgPragmaDeclaration *>::reverse_iterator
      iter; // bottom up handling for nested cases
  ROSE_ASSERT(sageFilePtr != NULL);
  const bool isFortran =
      sageFilePtr->get_Fortran_only() || sageFilePtr->get_F77_only() ||
      sageFilePtr->get_F90_only() || sageFilePtr->get_F95_only() ||
      sageFilePtr->get_F2003_only();
  std::map<SgPragmaDeclaration *, OpenMPDirective *> omp_lookup;
  std::map<SgPragmaDeclaration *, OpenACCDirective *> acc_lookup;
  for (const auto &entry : OpenMPIR_list) {
    omp_lookup[entry.first] = entry.second;
  }
  for (const auto &entry : OpenACCIR_list) {
    acc_lookup[entry.first] = entry.second;
  }
  for (iter = omp_pragma_list.rbegin(); iter != omp_pragma_list.rend();
       iter++) {
    // Liao, 11/18/2009
    // It is possible that several source files showing up in a single
    // compilation line We have to check if the pragma declaration's file
    // information matches the current file being processed Otherwise we will
    // process the same pragma declaration multiple times!!
    SgPragmaDeclaration *decl = *iter;
    if (isFortran) {
      if (SgScopeStatement *parent_scope =
              isSgScopeStatement(decl->get_parent())) {
        if (decl->get_scope() != parent_scope) {
          decl->set_scope(parent_scope);
        }
      }
    }
    // Liao, 2/8/2010
    // Some pragmas are set to "transformation generated" when we fix scopes for
    // some pragma under single statement block e.g if ()
    //      #pragma
    //        do_sth()
    //  will be changed to
    //     if ()
    //     {
    //       #pragma
    //        do_sth()
    //     }
    // So we process a pragma if it is either within the same file or marked as
    // transformation
    if (getEnclosingSourceFile(decl) != sageFilePtr) {
      continue;
    }
    if (!isFortran &&
        decl->get_file_info()->get_filename() !=
            sageFilePtr->get_file_info()->get_filename() &&
        !(decl->get_file_info()->isTransformation()))
      continue;
    auto omp_it = omp_lookup.find(decl);
    if (omp_it != omp_lookup.end()) {
      convertDirective(std::make_pair(decl, omp_it->second));
      continue;
    }
    auto acc_it = acc_lookup.find(decl);
    if (acc_it != acc_lookup.end()) {
      convertOpenACCDirective(std::make_pair(decl, acc_it->second));
      continue;
    }

  } // end for (omp_pragma_list)
}

//! A helper function to ensure a sequence statements either has only one
//! statement
//  or all are put under a single basic block.
//  begin_decl is the begin directive which is immediately in front of the list
//  of statements Return the single statement or the basic block. This function
//  is used to wrap all statement between begin and end Fortran directives into
//  a block, if necessary(more than one statement)
static SgStatement *
ensureSingleStmtOrBasicBlock(SgPragmaDeclaration *begin_decl,
                             const std::vector<SgStatement *> &stmt_vec) {
  ROSE_ASSERT(begin_decl != NULL);
  SgStatement *result = NULL;
  if (stmt_vec.empty()) {
    return NULL;
  }
  if (stmt_vec.size() == 1) {
    result = stmt_vec[0];
    ROSE_ASSERT(getNextStatement(begin_decl) == result);
  } else {
    result = buildBasicBlock();
    SgScopeStatement *new_scope = isSgScopeStatement(result);
    ROSE_ASSERT(new_scope != NULL);
    // Have to remove them from their original scope first.
    // Otherwise they will show up twice in the unparsed code: original place
    // and under the new block I tried to merge this into appendStatement() but
    // it broke other transformations I don't want debug
    for (std::vector<SgStatement *>::const_iterator iter = stmt_vec.begin();
         iter != stmt_vec.end(); iter++)
      removeStatement(*iter);
    for (std::vector<SgStatement *>::const_iterator iter = stmt_vec.begin();
         iter != stmt_vec.end(); iter++)
      (*iter)->set_scope(new_scope);
    appendStatementList(stmt_vec, new_scope);
    insertStatementAfter(begin_decl, result, false);
  }
  return result;
}

static bool fortranAstUnparserEmitsOpenMPEnd(OpenMPDirectiveKind kind) {
  switch (kind) {
  case OMPD_parallel:
  case OMPD_critical:
  case OMPD_sections:
  case OMPD_master:
  case OMPD_ordered:
  case OMPD_workshare:
  case OMPD_single:
  case OMPD_task:
  case OMPD_do:
  case OMPD_parallel_do:
  case OMPD_parallel_loop:
    return true;
  default:
    return false;
  }
}

//! This function will Find a (optional) end pragma for an input pragma (decl)
//  and merge clauses from the end pragma to the beginning pragma
//  statements in between will be put into a basic block if there are more than
//  one statements
void merge_Matching_Fortran_Pragma_pairs(SgPragmaDeclaration *decl) {
  SgPragmaDeclaration *end_decl = NULL;
  SgStatement *next_stmt = getNextStatement(decl);
  auto begin_it = fortran_paired_pragma_dict.find(decl);
  ROSE_ASSERT(begin_it != fortran_paired_pragma_dict.end());
  OpenMPDirective *begin_directive = begin_it->second;
  ROSE_ASSERT(begin_directive != NULL);
  OpenMPDirectiveKind begin_directive_kind = begin_directive->getKind();

  std::vector<SgStatement *>
      affected_stmts; // statements which are inside the begin .. end pair

  // Find possible end directives attached to a pragma declaration
  while (next_stmt != NULL) {
    end_decl = isSgPragmaDeclaration(next_stmt);
    if (end_decl != NULL) {
      auto end_it = fortran_paired_pragma_dict.find(end_decl);
      if (end_it != fortran_paired_pragma_dict.end()) {
        OpenMPDirective *end_ir = end_it->second;
        if (end_ir != NULL && end_ir->getKind() == OMPD_end) {
          OpenMPEndDirective *end_directive =
              static_cast<OpenMPEndDirective *>(end_ir);
          if (end_directive != NULL &&
              end_directive->getPairedDirective() == begin_directive) {
            break;
          }
        }
      }
      end_decl = NULL; // MUST reset to NULL if not a match
    }
    affected_stmts.push_back(next_stmt);
    next_stmt = getNextStatement(next_stmt);
  } // end while

  // End directives are optional for selected Fortran OpenMP constructs.
  if (end_decl == NULL) {
    if (!allowsImplicitOpenMPEnd(begin_directive)) {
      cerr << "merge_Matching_Fortran_Pragma_pairs(): cannot find required end "
              "directive for: "
           << endl;
      cerr << decl->get_pragma()->get_pragma() << endl;
      ROSE_ABORT();
    } else
      return; // There is nothing further to do if the optional end directives
              // do not exist
  } // end if sanity check

  // at this point, we have found a matching end directive/pragma
  ROSE_ASSERT(end_decl);
  if (decl->getAttribute(kOmpFortranEndAttributeName) == NULL) {
    decl->addNewAttribute(kOmpFortranEndAttributeName,
                          new OmpFortranEndAttribute());
  }
  SgStatement *merged_body = ensureSingleStmtOrBasicBlock(decl, affected_stmts);
  if (merged_body == NULL) {
    // Keep both begin/end pragmas unchanged when no associated body statement
    // can be formed from this paired region.
    return;
  }

  // Keep end pragmas until after AST conversion. Post-conversion cleanup can
  // then remove only those end pragmas whose begin directives successfully
  // lowered to SgOmp nodes that emit matching END directives.
} // end merge_Matching_Fortran_Pragma_pairs()

//! This function will
//   x. Find matching OpenMP directive pairs
//      an inside out order is used to handle nested regions
//   x. Put statements in between into a basic block
//   x. Merge clauses from the ending directive to the beginning directives
//  The result is an Fortran OpenMP AST with C/C++ pragmas
//  so we can simply reuse convert_OpenMP_pragma_to_AST() to generate
//  OpenMP AST nodes for Fortran programs
void convert_Fortran_Pragma_Pairs(SgSourceFile *sageFilePtr) {
  ROSE_ASSERT(sageFilePtr != NULL);
  list<SgPragmaDeclaration *>::reverse_iterator
      iter; // bottom up handling for nested cases
  for (iter = omp_pragma_list.rbegin(); iter != omp_pragma_list.rend();
       iter++) {
    // It is possible that several source files showing up in a single
    // compilation line We have to check if the pragma declaration's file
    // information matches the current file being processed Otherwise we will
    // process the same pragma declaration multiple times!!
    SgPragmaDeclaration *decl = *iter;
    // Some pragmas are set to "transformation generated" when we fix scopes for
    // some pragma under single statement block e.g if ()
    //      #pragma
    //        do_sth()
    //  will be changed to
    //     if ()
    //     {
    //       #pragma
    //        do_sth()
    //     }
    // So we process a pragma if it is either within the same file or marked as
    // transformation
    if (decl->get_file_info()->get_filename() !=
            sageFilePtr->get_file_info()->get_filename() &&
        !(decl->get_file_info()->isTransformation()))
      continue;
    if (isFortranPairedDirective(fortran_paired_pragma_dict[decl])) {
      merge_Matching_Fortran_Pragma_pairs(decl);
    }
  } // end for omp_pragma_list

} // end convert_Fortran_Pragma_Pairs()

static bool allowsImplicitFortranAccEnd(OpenACCDirectiveKind kind) {
  switch (kind) {
  case ACCD_parallel_loop:
    return true;
  default:
    return false;
  }
}

static bool isFortranAccPairedDirective(OpenACCDirective *directive) {
  if (directive == NULL) {
    return false;
  }
  switch (directive->getKind()) {
  case ACCD_parallel:
  case ACCD_parallel_loop:
  case ACCD_data:
  case ACCD_kernels:
    return true;
  default:
    return false;
  }
}

void merge_Matching_Fortran_ACC_Pragma_pairs(SgPragmaDeclaration *decl) {
  SgPragmaDeclaration *end_decl = NULL;
  SgStatement *next_stmt = getNextStatement(decl);
  OpenACCDirectiveKind begin_directive_kind =
      fortran_acc_paired_pragma_dict[decl]->getKind();

  std::vector<SgStatement *> affected_stmts;

  while (next_stmt != NULL) {
    end_decl = isSgPragmaDeclaration(next_stmt);
    if (end_decl != NULL) {
      auto end_it = fortran_acc_paired_pragma_dict.find(end_decl);
      if (end_it != fortran_acc_paired_pragma_dict.end()) {
        OpenACCDirective *end_ir = end_it->second;
        if (end_ir != NULL && end_ir->getKind() == ACCD_end) {
          OpenACCEndDirective *end_directive =
              dynamic_cast<OpenACCEndDirective *>(end_ir);
          if (end_directive != NULL &&
              end_directive->getPairedDirective() != NULL &&
              end_directive->getPairedDirective()->getKind() ==
                  begin_directive_kind) {
            break;
          }
        }
      }
    }
    end_decl = NULL;
    affected_stmts.push_back(next_stmt);
    next_stmt = getNextStatement(next_stmt);
  }

  if (end_decl == NULL) {
    if (!allowsImplicitFortranAccEnd(begin_directive_kind)) {
      cerr << "merge_Matching_Fortran_ACC_Pragma_pairs(): cannot find required "
              "end directive for: "
           << endl;
      cerr << decl->get_pragma()->get_pragma() << endl;
      ROSE_ABORT();
    }
    return;
  }

  ROSE_ASSERT(end_decl != NULL);
  ensureSingleStmtOrBasicBlock(decl, affected_stmts);

  decl->setAttribute(kAccFortranEndAttributeName, new AccFortranEndAttribute());

  removeStatement(end_decl);
}

void convert_Fortran_ACC_Pragma_Pairs(SgSourceFile *sageFilePtr) {
  ROSE_ASSERT(sageFilePtr != NULL);
  list<SgPragmaDeclaration *>::reverse_iterator iter;
  for (iter = omp_pragma_list.rbegin(); iter != omp_pragma_list.rend();
       iter++) {
    SgPragmaDeclaration *decl = *iter;
    if (decl->get_file_info()->get_filename() !=
            sageFilePtr->get_file_info()->get_filename() &&
        !(decl->get_file_info()->isTransformation()))
      continue;
    auto acc_it = fortran_acc_paired_pragma_dict.find(decl);
    if (acc_it == fortran_acc_paired_pragma_dict.end()) {
      continue;
    }
    if (isFortranAccPairedDirective(acc_it->second)) {
      merge_Matching_Fortran_ACC_Pragma_pairs(decl);
    }
  }
}

//! Convert OpenMP Fortran comments to pragmas
//  main purpose is to
//     x. Generate pragmas from OpenMPIR and insert them into the right places
//        since the floating comments are very difficult to work with
//        we move them to the fake pragmas to ease later translations.
//        The backend has been extended to unparse the pragma in order to debug
//        this step.
//     x. Enclose affected Fortran statement into a basic block
//     x. Merge clauses from END directives to the begin directive
// This will temporarily introduce C/C++-like AST with pragmas.
// This should be fine since we have SgBasicBlock in Fortran AST also.
//
// The benefit is that pragma-to-AST conversion written for C/C++ can
// be reused for Fortran after this pass.
// Liao 10/18/2010
void convert_Fortran_OMP_Comments_to_Pragmas(SgSourceFile *sageFilePtr) {
  ROSE_ASSERT(sageFilePtr != NULL);
  // step 1: Each OpenMPIR will have a dedicated SgPragmaDeclaration for it

  // we record the last pragma inserted after a statement, if any
  std::map<SgStatement *, SgPragmaDeclaration *> stmt_last_pragma_dict;
  // Track pragmas inserted before a statement to preserve their original order.
  std::map<SgStatement *, SgPragmaDeclaration *> stmt_last_before_pragma_dict;
  std::unordered_set<std::string> seen_requires_directives;
  std::unordered_set<std::string> seen_directive_instances;
  std::unordered_map<OpenMPDirective *, std::string> pragma_text_by_ir;

  std::vector<std::tuple<SgLocatedNode *, PreprocessingInfo *,
                         OpenMPDirective *>>::iterator iter;
  for (iter = fortran_omp_pragma_list.begin();
       iter != fortran_omp_pragma_list.end(); iter++) {
    SgLocatedNode *loc_node = std::get<0>(*iter);
    SgStatement *stmt = isSgStatement(loc_node);
    OpenMPDirective *ompparser_directive_ir = std::get<2>(*iter);
    // TODO verify this assertion is true for Fortran OpenMP comments
    ROSE_ASSERT(stmt != NULL);
    // cout<<"debug at ompAstConstruction.cpp:"<<stmt <<" " <<
    // stmt->getAttachedPreprocessingInfo ()->size() <<endl;
    ROSE_ASSERT(stmt->getAttachedPreprocessingInfo()->size() != 0);
    // So we process the directive if it's anchor node is either within the same
    // file or marked as transformation
    if (stmt->get_file_info()->get_filename() !=
            sageFilePtr->get_file_info()->get_filename() &&
        !(stmt->get_file_info()->isTransformation()))
      continue;
    SgScopeStatement *scope = stmt->get_scope();
    ROSE_ASSERT(scope != NULL);
    // the pragma will have string to ease debugging
    std::string pragma_string =
        ompparser_directive_ir->generatePragmaString("omp ", "", "");
    const bool duplicate_requires =
        ompparser_directive_ir->getKind() == OMPD_requires &&
        !seen_requires_directives
             .insert(canonicalizeDirectiveKey(pragma_string))
             .second;
    SgPragmaDeclaration *p_decl = buildPragmaDeclaration(pragma_string, scope);
    // preserve the original source file info ,TODO complex cases , use real
    // preprocessing info's line information !!
    copyStartFileInfo(loc_node, p_decl);

    PreprocessingInfo *info = std::get<1>(*iter);
    ROSE_ASSERT(info != NULL);
    // We still keep the peprocessingInfo. its line number will be used later to
    // set file info object
    AttachedPreprocessingInfoType *comments =
        stmt->getAttachedPreprocessingInfo();
    ROSE_ASSERT(comments != NULL);
    ROSE_ASSERT(comments->size() != 0);
    AttachedPreprocessingInfoType::iterator m_pos =
        find(comments->begin(), comments->end(), info);
    if (m_pos == comments->end()) {
      cerr << "Cannot find a Fortran comment from a node: " << endl;
      cerr << "The comment is " << info->getString() << endl;
      cerr << "The AST Node is " << stmt->class_name() << endl;
      stmt->get_file_info()->display("debug here");
      AttachedPreprocessingInfoType::iterator i;
      for (i = comments->begin(); i != comments->end(); i++) {
        cerr << (*i)->getString() << endl;
      }
      // cerr<<"The AST Node is at
      // line:"<<stmt->get_file_info().get_line()<<endl;
      ROSE_ASSERT(m_pos != comments->end());
    }

    const int directive_line = info->getLineNumber();
    bool erased_any = false;
    for (AttachedPreprocessingInfoType::iterator c_iter = comments->begin();
         c_iter != comments->end();) {
      PreprocessingInfo *candidate = *c_iter;
      const bool same_comment = candidate == info;
      const bool duplicate_omp_comment =
          candidate != NULL && candidate->getLineNumber() == directive_line &&
          isFortranCommentDirective(candidate, "omp");
      if (same_comment || duplicate_omp_comment) {
        c_iter = comments->erase(c_iter);
        erased_any = true;
      } else {
        ++c_iter;
      }
    }
    ROSE_ASSERT(erased_any);

    PreprocessingInfo::RelativePositionType position =
        info->getRelativePosition();
    std::ostringstream directive_instance_key;
    directive_instance_key << info->getFileId() << ":" << info->getLineNumber()
                           << ":" << canonicalizeDirectiveKey(pragma_string);
    if (!seen_directive_instances.insert(directive_instance_key.str()).second) {
      delete p_decl;
      delete ompparser_directive_ir;
      continue;
    }

    if (duplicate_requires) {
      delete p_decl;
      delete ompparser_directive_ir;
      continue;
    }

    pragma_text_by_ir[ompparser_directive_ir] =
        std::string("#pragma ") + pragma_string;
    if (ompparser_directive_ir->getKind() != OMPD_end) {
      OpenMPIR_list.push_back(std::make_pair(p_decl, ompparser_directive_ir));
      omp_pragma_list.push_back(p_decl);
    }
    fortran_paired_pragma_dict[p_decl] = ompparser_directive_ir;

    // two cases for where to insert the pragma, depending on where the
    // preprocessing info is attached to stmt
    //  1. PreprocessingInfo::before
    //     insert the pragma right before the original Fortran statement
    //  2. PreprocessingInfo::inside
    //      insert it as the last statement within stmt
    if (position == PreprocessingInfo::before) {
      SgPragmaDeclaration *last_before = NULL;
      if (stmt_last_before_pragma_dict.count(stmt)) {
        last_before = stmt_last_before_pragma_dict[stmt];
      }
      // Don't automatically move comments here!
      if (isSgBasicBlock(stmt) &&
          isSgFortranDo(
              stmt->get_parent())) { // special handling for the body of
                                     // SgFortranDo.  The comments will be
                                     // attached before the body But we cannot
                                     // insert the pragma before the body. So we
                                     // prepend it into the body instead
        if (last_before) {
          insertStatementAfter(last_before, p_decl, false);
        } else {
          prependStatement(p_decl, isSgBasicBlock(stmt));
        }
      } else if (isSgFunctionDefinition(stmt->get_parent())) {
        SgFunctionDefinition *def = isSgFunctionDefinition(stmt->get_parent());
        ROSE_ASSERT(def != NULL);
        SgBasicBlock *body = def->get_body();
        ROSE_ASSERT(body != NULL);
        if (last_before) {
          insertStatementAfter(last_before, p_decl, false);
        } else {
          prependStatement(p_decl, body);
        }
      } else if (last_before) {
        insertStatementAfter(last_before, p_decl, false);
      } else {
        insertStatementBefore(stmt, p_decl, false);
      }
      stmt_last_before_pragma_dict[stmt] = p_decl;
    } else if (position == PreprocessingInfo::inside) {
      SgScopeStatement *scope = isSgScopeStatement(stmt);
      ROSE_ASSERT(scope != NULL);
      appendStatement(p_decl, scope);
    } else if (position == PreprocessingInfo::after) {
      if (ompparser_directive_ir->getKind() == OMPD_requires) {
        SgFunctionDefinition *def = getEnclosingFunctionDefinition(stmt);
        if (SgProgramHeaderStatement *program =
                isSgProgramHeaderStatement(stmt))
          def = program->get_definition();

        if (def != NULL) {
          SgBasicBlock *body = def->get_body();
          ROSE_ASSERT(body != NULL);
          SgPragmaDeclaration *last_before = NULL;
          if (stmt_last_before_pragma_dict.count(body))
            last_before = stmt_last_before_pragma_dict[body];
          if (last_before) {
            insertStatementAfter(last_before, p_decl, false);
          } else {
            prependStatement(p_decl, body);
          }
          stmt_last_before_pragma_dict[body] = p_decl;
          continue;
        }
      }

      SgStatement *last = stmt;
      if (stmt_last_pragma_dict.count(stmt))
        last = stmt_last_pragma_dict[stmt];
      // Liao, 3/31/2021
      // It is possible there are several comments attached after a same
      // statement. In this case, we should not just insert each generated
      // pragma right after the statement. We should insert each pragma after
      // the previously inserted pragma to preserve the original order.
      // Otherwise , we will end up with reversed order of pragmas, causing
      // later pragma pair matching problem.

      if (isSgFunctionDefinition(stmt->get_parent())) {
        SgFunctionDefinition *def = isSgFunctionDefinition(stmt->get_parent());
        ROSE_ASSERT(def != NULL);
        SgBasicBlock *body = def->get_body();
        ROSE_ASSERT(body != NULL);
        appendStatement(p_decl, body);
      } else {
        insertStatementAfter(last, p_decl, false);
      }
      stmt_last_pragma_dict[stmt] = p_decl;
    } else {
      cerr << "ompAstConstruction.cpp , illegal "
              "PreprocessingInfo::RelativePositionType:"
           << position << endl;
      ROSE_ABORT();
    }
  } // end for omp_comment_list

  for (const auto &entry : OpenMPIR_list) {
    if (entry.second->getKind() == OMPD_end) {
      continue;
    }
    auto pragma_it = pragma_text_by_ir.find(entry.second);
    if (pragma_it != pragma_text_by_ir.end()) {
      g_omp_clause_nodes[entry.second] = parseClauseNodesForDirective(
          entry.first, entry.second, pragma_it->second);
    }
  }

  convert_Fortran_Pragma_Pairs(sageFilePtr);
} // end convert_Fortran_OMP_Comments_to_Pragmas ()

//! Convert OpenACC Fortran comments to pragmas (no OpenACC AST conversion yet)
void convert_Fortran_ACC_Comments_to_Pragmas(SgSourceFile *sageFilePtr) {
  ROSE_ASSERT(sageFilePtr != NULL);

  struct FortranCommentEntry {
    SgLocatedNode *loc_node;
    PreprocessingInfo *info;
    int file_id;
    int line;
    int column;
    size_t order;
  };

  std::vector<FortranCommentEntry> comment_entries;
  size_t order = 0;
  std::vector<SgNode *> loc_nodes =
      NodeQuery::querySubTree(sageFilePtr, V_SgLocatedNode);
  for (SgNode *node : loc_nodes) {
    SgLocatedNode *locNode = isSgLocatedNode(node);
    ROSE_ASSERT(locNode);
    AttachedPreprocessingInfoType *comments =
        locNode->getAttachedPreprocessingInfo();
    if (!comments) {
      continue;
    }
    for (PreprocessingInfo *pinfo : *comments) {
      if (pinfo->getTypeOfDirective() ==
              PreprocessingInfo::FortranStyleComment ||
          pinfo->getTypeOfDirective() == PreprocessingInfo::F90StyleComment) {
        comment_entries.push_back({locNode, pinfo, pinfo->getFileId(),
                                   pinfo->getLineNumber(),
                                   pinfo->getColumnNumber(), order++});
      }
    }
  }

  std::stable_sort(
      comment_entries.begin(), comment_entries.end(),
      [](const FortranCommentEntry &lhs, const FortranCommentEntry &rhs) {
        if (lhs.file_id != rhs.file_id) {
          return lhs.file_id < rhs.file_id;
        }
        if (lhs.line != rhs.line) {
          return lhs.line < rhs.line;
        }
        if (lhs.column != rhs.column) {
          return lhs.column < rhs.column;
        }
        return lhs.order < rhs.order;
      });

  std::map<SgStatement *, SgPragmaDeclaration *> stmt_last_pragma_dict;
  std::map<SgStatement *, SgPragmaDeclaration *> stmt_last_before_pragma_dict;

  PreprocessingInfo *previnfo = nullptr;
  SgLocatedNode *prev_loc_node = nullptr;
  for (const FortranCommentEntry &entry : comment_entries) {
    SgLocatedNode *locNode = entry.loc_node;
    PreprocessingInfo *pinfo = entry.info;

    if (previnfo != nullptr && prev_loc_node != locNode) {
      previnfo = nullptr;
      prev_loc_node = nullptr;
    }

    std::string buffer = pinfo->getString();
    if (!isFortranAccDirective(buffer)) {
      if (previnfo != nullptr && prev_loc_node == locNode) {
        cerr << "error: Found a non-OpenACC comment after a pending OpenACC "
                "comment with a line continuation\n";
        ROSE_ABORT();
      }
      continue;
    }

    normalizeFortranAccSentinel(buffer);
    removeFortranAccComments(buffer);

    if (previnfo != nullptr && prev_loc_node == locNode) {
      buffer = previnfo->getString() + buffer;
      postProcessMergedAccContinuation(buffer);
      previnfo->setString("");
      previnfo = nullptr;
      prev_loc_node = nullptr;
    }

    pinfo->setString(buffer);

    if (hasFortranLineContinuation(buffer)) {
      previnfo = pinfo;
      prev_loc_node = locNode;
      continue;
    }

    std::string pragma_text = buffer;
    stripFortranDirectiveSentinel(pragma_text);
    stripFortranComment(pragma_text);
    stripFortranLineContinuation(pragma_text);
    trim(pragma_text);
    if (pragma_text.empty()) {
      continue;
    }
    if (!startsWithAccKeyword(pragma_text)) {
      pragma_text = std::string("acc ") + pragma_text;
    }

    SgStatement *stmt = isSgStatement(locNode);
    if (stmt == NULL) {
      stmt = SageInterface::getEnclosingStatement(locNode);
    }
    ROSE_ASSERT(stmt != NULL);
    SgScopeStatement *scope = stmt->get_scope();
    ROSE_ASSERT(scope != NULL);

    SgPragmaDeclaration *p_decl = buildPragmaDeclaration(pragma_text, scope);
    copyStartFileInfo(locNode, p_decl);
    if (Sg_File_Info *info = p_decl->get_file_info()) {
      info->set_line(pinfo->getLineNumber());
      info->set_col(pinfo->getColumnNumber());
    }

    std::string parse_text = std::string("!$") + pragma_text;
    accparser_OpenACCIR = parseOpenACC(parse_text);
    ROSE_ASSERT(accparser_OpenACCIR != NULL);
    use_accparser = checkOpenACCIR(accparser_OpenACCIR);
    ROSE_ASSERT(use_accparser == true);
    if (accparser_OpenACCIR->getKind() != ACCD_end) {
      OpenACCIR_list.push_back(std::make_pair(p_decl, accparser_OpenACCIR));
    }
    fortran_acc_paired_pragma_dict[p_decl] = accparser_OpenACCIR;

    AttachedPreprocessingInfoType *comments =
        stmt->getAttachedPreprocessingInfo();
    ROSE_ASSERT(comments != NULL);
    auto m_pos = find(comments->begin(), comments->end(), pinfo);
    if (m_pos == comments->end()) {
      cerr << "Cannot find a Fortran comment from a node: " << endl;
      cerr << "The comment is " << pinfo->getString() << endl;
      cerr << "The AST Node is " << stmt->class_name() << endl;
      stmt->get_file_info()->display("debug here");
      for (auto *info : *comments) {
        cerr << info->getString() << endl;
      }
      ROSE_ASSERT(m_pos != comments->end());
    }
    comments->erase(m_pos);

    PreprocessingInfo::RelativePositionType position =
        pinfo->getRelativePosition();
    if (position == PreprocessingInfo::before) {
      SgPragmaDeclaration *last_before = NULL;
      if (stmt_last_before_pragma_dict.count(stmt)) {
        last_before = stmt_last_before_pragma_dict[stmt];
      }
      if (isSgBasicBlock(stmt) && isSgFortranDo(stmt->get_parent())) {
        if (last_before) {
          insertStatementAfter(last_before, p_decl, false);
        } else {
          prependStatement(p_decl, isSgBasicBlock(stmt));
        }
      } else if (isSgFunctionDefinition(stmt->get_parent())) {
        SgFunctionDefinition *def = isSgFunctionDefinition(stmt->get_parent());
        ROSE_ASSERT(def != NULL);
        SgBasicBlock *body = def->get_body();
        ROSE_ASSERT(body != NULL);
        if (last_before) {
          insertStatementAfter(last_before, p_decl, false);
        } else {
          prependStatement(p_decl, body);
        }
      } else if (last_before) {
        insertStatementAfter(last_before, p_decl, false);
      } else {
        insertStatementBefore(stmt, p_decl, false);
      }
      stmt_last_before_pragma_dict[stmt] = p_decl;
    } else if (position == PreprocessingInfo::inside) {
      SgScopeStatement *scope = isSgScopeStatement(stmt);
      ROSE_ASSERT(scope != NULL);
      appendStatement(p_decl, scope);
    } else if (position == PreprocessingInfo::after) {
      SgStatement *last = stmt;
      if (stmt_last_pragma_dict.count(stmt)) {
        last = stmt_last_pragma_dict[stmt];
      }
      if (isSgFunctionDefinition(stmt->get_parent())) {
        SgFunctionDefinition *def = isSgFunctionDefinition(stmt->get_parent());
        ROSE_ASSERT(def != NULL);
        SgBasicBlock *body = def->get_body();
        ROSE_ASSERT(body != NULL);
        appendStatement(p_decl, body);
      } else {
        insertStatementAfter(last, p_decl, false);
      }
      stmt_last_pragma_dict[stmt] = p_decl;
    } else {
      cerr << "ompAstConstruction.cpp , illegal "
              "PreprocessingInfo::RelativePositionType:"
           << position << endl;
      ROSE_ABORT();
    }

    omp_pragma_list.push_back(p_decl);
  }
}

// Liao, 5/31/2009 an entry point for OpenMP related processing
// including parsing, AST construction, and later on translation
void processOpenMP(SgSourceFile *sageFilePtr) {
  // DQ (4/4/2010): This function processes both C/C++ and Fortran code.
  // As a result of the Fortran processing some OMP pragmas will cause
  // transformation (e.g. declaration of private variables will add variables
  // to the local scope).  So this function has side-effects for all languages.

  if (sageFilePtr == NULL) {
    return;
  }
  if (sageFilePtr->get_openmp_processed()) {
    return;
  }

  auto mark_processed = [&](SgSourceFile *file) {
    if (file == nullptr) {
      return;
    }
    if (file->get_openmp_processed()) {
      return;
    }
    file->set_openmp_processed(true);
  };

  if (SgProject::get_verbose() > 1) {
    printf("Processing OpenMP directives ... \n");
  }

  const bool wantsOpenMP = sageFilePtr->get_openmp();
  const bool wantsOpenACC = sageFilePtr->get_openacc();

  if (!wantsOpenMP && !wantsOpenACC) {
    if (SgProject::get_verbose() > 1) {
      printf("Stop processing OpenMP/OpenACC directives since none found. \n");
    }
    return;
  }

  if (wantsOpenMP) {
    // Preserve original directive/clause structure instead of normalizing and
    // merging clauses in the parser.
    setNormalizeClauses(false);
  }

  bool isFortran = sageFilePtr->get_Fortran_only() ||
                   sageFilePtr->get_F77_only() || sageFilePtr->get_F90_only() ||
                   sageFilePtr->get_F95_only() || sageFilePtr->get_F2003_only();
  bool parsed_fortran_pragmas = false;

  // ==================================================================================================================//
  // ====== Stage 1: parse OpenMP directives using ompparser and store the
  // ompparser's OpenMPIR nodes in a map   ======
  // ==================================================================================================================//
  // find all SgPragmaDeclaration nodes within a file, parse OpenMP directives
  // using ompparser, and store the ompparser OpenMPIR in a map OpenMPIR_list.
  // ompparser only parse OpenMP directive/clauses not the expressions that are
  // used by the directives/clauses For Fortran, search comments for OpenMP
  // directives
  if (isFortran) { // use ompparser to process Fortran.
    if (wantsOpenMP) {
      parsed_fortran_pragmas = parseOpenMPFortranPragmas(sageFilePtr);
      if (!parsed_fortran_pragmas) {
        parseOpenMPFortran(sageFilePtr);
      }
    }
  } else { // For C/C++, search pragma declarations for OpenMP directives
    std::vector<SgNode *> all_pragmas =
        NodeQuery::querySubTree(sageFilePtr, V_SgPragmaDeclaration);
    std::unordered_map<std::string,
                       std::shared_ptr<const std::vector<std::string>>>
        source_lines_cache;
    std::mutex source_lines_cache_mutex;
    std::vector<SgNode *>::iterator iter;
    for (iter = all_pragmas.begin(); iter != all_pragmas.end(); iter++) {
      SgPragmaDeclaration *pragmaDeclaration = isSgPragmaDeclaration(*iter);
      ROSE_ASSERT(pragmaDeclaration != NULL);
      const std::string preprocessedPragmaString =
          pragmaDeclaration->get_pragma()->get_pragma();
      string pragmaString = preprocessedPragmaString;
      const std::string rawPragmaString = getRawOpenMPCppDirectiveText(
          pragmaDeclaration, source_lines_cache, source_lines_cache_mutex);
      if (!rawPragmaString.empty()) {
        pragmaString = rawPragmaString;
      }
      istringstream istr(pragmaString);
      std::string key;
      istr >> key;
      if (key == "omp" && wantsOpenMP) {
        const std::string preferredPragmaString = rawPragmaString.empty()
                                                      ? preprocessedPragmaString
                                                      : rawPragmaString;
        if (!preferredPragmaString.empty()) {
          g_omp_directive_source_text_by_pragma[pragmaDeclaration] =
              preferredPragmaString;
          if (pragmaDeclaration->get_pragma() != NULL &&
              preferredPragmaString != preprocessedPragmaString) {
            pragmaDeclaration->set_pragma(
                SageBuilder::buildPragma(preferredPragmaString));
          }
        }

        // parse expression
        // Get the object that ompparser IR.
        ompparser_OpenMPIR =
            parseOpenMP(pragmaString.c_str(), nullptr, nullptr);
        if (ompparser_OpenMPIR == NULL &&
            pragmaString != preprocessedPragmaString) {
          pragmaString = preprocessedPragmaString;
          ompparser_OpenMPIR =
              parseOpenMP(pragmaString.c_str(), nullptr, nullptr);
        }
        assert(ompparser_OpenMPIR != NULL);
        if (shouldSkipOpenMPDirectiveAstConversion(ompparser_OpenMPIR)) {
          delete ompparser_OpenMPIR;
          ompparser_OpenMPIR = nullptr;
          continue;
        }

        use_ompparser = checkOpenMPIR(ompparser_OpenMPIR);
        if (!use_ompparser) {
          delete ompparser_OpenMPIR;
          ompparser_OpenMPIR = nullptr;
          continue;
        }
        omp_pragma_list.push_back(pragmaDeclaration);
        OpenMPIR_list.push_back(
            std::make_pair(pragmaDeclaration, ompparser_OpenMPIR));
        std::string parse_text = std::string("#pragma ") + pragmaString;
        if (ompparser_OpenMPIR->getKind() != OMPD_end) {
          g_omp_clause_nodes[ompparser_OpenMPIR] = parseClauseNodesForDirective(
              pragmaDeclaration, ompparser_OpenMPIR, parse_text);
        }
      } else if (key == "acc" && wantsOpenACC) {
        // store them into a buffer, reused by build_OpenMP_AST()
        omp_pragma_list.push_back(pragmaDeclaration);
        // Call parser
        // Get the OpenMP IR converted from the OpenACC IR.
        pragmaString = "#pragma " + pragmaString;
        accparser_OpenACCIR = parseOpenACC(pragmaString);
        assert(accparser_OpenACCIR != NULL);
        use_accparser = checkOpenACCIR(accparser_OpenACCIR);
        assert(use_accparser == true);
        if (accparser_OpenACCIR->getKind() != ACCD_end) {
          OpenACCIR_list.push_back(
              std::make_pair(pragmaDeclaration, accparser_OpenACCIR));
        }
      }
    } // end for
  }

  // stop here if only OpenMP parsing is requested
  if (sageFilePtr->get_openmp_parse_only()) {
    if (SgProject::get_verbose() > 1) {
      printf("Skipping calls to lower OpenMP "
             "sageFilePtr->get_openmp_parse_only() = %s \n",
             sageFilePtr->get_openmp_parse_only() ? "true" : "false");
    }
    releaseOpenMPParseStateForSourceFile(sageFilePtr);
    mark_processed(sageFilePtr);
    return;
  }

  // Build OpenMP AST nodes based on parsing results
  if (!isFortran) {
    collectCommentedDirectiveRelocations(sageFilePtr, omp_pragma_list);
  }

  if (isFortran) {
    if (parsed_fortran_pragmas) {
      removeFortranDirectiveComments(sageFilePtr, "omp");
      convert_Fortran_Pragma_Pairs(sageFilePtr);
    } else {
      convert_Fortran_OMP_Comments_to_Pragmas(
          sageFilePtr); // TODO: need to fix not sure why we still need this
                        // here since Fortran is already parsed before.
    }
    if (wantsOpenACC) {
      convert_Fortran_ACC_Comments_to_Pragmas(sageFilePtr);
      convert_Fortran_ACC_Pragma_Pairs(sageFilePtr);
    }
  }
  if (SgProject::get_verbose() > 1) {
    printf("Calling convert_OpenMP_pragma_to_AST() \n");
  }
  // We can turn this off to debug the convert_Fortran_OMP_Comments_to_Pragmas()
  OpenMPIRToSageAST(sageFilePtr);
  if (isFortran) {
    if (hasFortranOpenMPArtifactsForSourceFile(sageFilePtr)) {
      removeFortranDirectiveComments(sageFilePtr, "omp");
      removeFortranOpenMPPragmas(sageFilePtr);
    }
  }

  // stop here if only OpenMP AST construction is requested
  if (sageFilePtr->get_openmp_ast_only()) {
    if (SgProject::get_verbose() > 1) {
      printf("Skipping calls to analyze/lower OpenMP "
             "sageFilePtr->get_openmp_ast_only() = %s \n",
             sageFilePtr->get_openmp_ast_only() ? "true" : "false");
    }
    releaseOpenMPParseStateForSourceFile(sageFilePtr);
    mark_processed(sageFilePtr);
    return;
  }

  // Analyze OpenMP AST
  analyze_omp(sageFilePtr);

  // stop here if only OpenMP AST analyzing is requested
  if (sageFilePtr->get_openmp_analyzing()) {
    if (SgProject::get_verbose() > 1) {
      printf("Skipping calls to lower OpenMP "
             "sageFilePtr->get_openmp_analyzing() = %s \n",
             sageFilePtr->get_openmp_analyzing() ? "true" : "false");
    }
    releaseOpenMPParseStateForSourceFile(sageFilePtr);
    mark_processed(sageFilePtr);
    return;
  }

  lower_omp(sageFilePtr);
  releaseOpenMPParseStateForSourceFile(sageFilePtr);
  mark_processed(sageFilePtr);
}

} // namespace OmpSupport

SgStatement *
convertDirective(std::pair<SgPragmaDeclaration *, OpenMPDirective *>
                     current_OpenMPIR_to_SageIII) {
  OpenMPDirectiveKind directive_kind =
      current_OpenMPIR_to_SageIII.second->getKind();
  SgStatement *result = NULL;

  switch (directive_kind) {
  case OMPD_metadirective:
  case OMPD_teams:
  case OMPD_atomic:
  case OMPD_do:
  case OMPD_taskgroup:
  case OMPD_dispatch:
  case OMPD_master:
  case OMPD_distribute:
  case OMPD_loop:
  case OMPD_scan:
  case OMPD_taskloop:
  case OMPD_target_enter_data:
  case OMPD_target_exit_data:
  case OMPD_target_parallel_for:
  case OMPD_target_parallel:
  case OMPD_distribute_simd:
  case OMPD_distribute_parallel_for:
  case OMPD_distribute_parallel_for_simd:
  case OMPD_taskloop_simd:
  case OMPD_target_parallel_for_simd:
  case OMPD_target_parallel_loop:
  case OMPD_target_simd:
  case OMPD_target_teams:
  case OMPD_target_teams_distribute:
  case OMPD_target_teams_distribute_simd:
  case OMPD_target_teams_loop:
  case OMPD_target_teams_distribute_parallel_for:
  case OMPD_target_teams_distribute_parallel_for_simd:
  case OMPD_master_taskloop_simd:
  case OMPD_parallel_master_taskloop:
  case OMPD_parallel_master_taskloop_simd:
  case OMPD_teams_distribute:
  case OMPD_teams_distribute_simd:
  case OMPD_teams_distribute_parallel_for:
  case OMPD_teams_distribute_parallel_for_simd:
  case OMPD_teams_loop:
  case OMPD_parallel_master:
  case OMPD_master_taskloop:
  case OMPD_parallel_loop:
  case OMPD_task:
  case OMPD_target_data:
  case OMPD_single:
  case OMPD_for:
  case OMPD_for_simd:
  case OMPD_target:
  case OMPD_critical:
  case OMPD_depobj:
  case OMPD_sections:
  case OMPD_section:
  case OMPD_simd:
  case OMPD_parallel:
  case OMPD_workshare:
  case OMPD_tile:
  case OMPD_unroll: {
    result = convertBodyDirective(current_OpenMPIR_to_SageIII);
    break;
  }
  case OMPD_end: {
    result = convertBodyDirective(current_OpenMPIR_to_SageIII);
    break;
  }
  case OMPD_ordered: {
    if (current_OpenMPIR_to_SageIII.second->getClausesInOriginalOrder()
            ->size() != 0) {
      std::vector<OpenMPClause *> *ordered_clauses =
          current_OpenMPIR_to_SageIII.second->getClausesInOriginalOrder();
      OpenMPClause *clause = *ordered_clauses->begin();
      if (clause->getKind() == OMPC_depend) {
        result = convertNonBodyDirective(current_OpenMPIR_to_SageIII);
        break;
      } else {
        result = convertBodyDirective(current_OpenMPIR_to_SageIII);
        break;
      }
    } else {
      result = convertBodyDirective(current_OpenMPIR_to_SageIII);
      break;
    }
  }
  case OMPD_parallel_do:
  case OMPD_parallel_for:
  case OMPD_parallel_for_simd:
  case OMPD_parallel_sections:
  case OMPD_parallel_workshare: {
    result = convertCombinedBodyDirective(current_OpenMPIR_to_SageIII);
    break;
  }
  case OMPD_declare_mapper:
  case OMPD_cancellation_point:
  case OMPD_target_update:
  case OMPD_cancel: {
    result = convertNonBodyDirective(current_OpenMPIR_to_SageIII);
    break;
  }
  case OMPD_requires: {
    result = convertOmpRequiresDirective(current_OpenMPIR_to_SageIII);
    break;
  }
  case OMPD_taskwait: {
    result = convertOmpTaskwaitDirective(current_OpenMPIR_to_SageIII);
    break;
  }
  case OMPD_barrier: {
    result = new SgOmpBarrierStatement();
    break;
  }
  case OMPD_declare_simd: {
    result = convertOmpDeclareSimdDirective(current_OpenMPIR_to_SageIII);
    break;
  }
  case OMPD_declare_variant: {
    result = convertOmpDeclareVariantDirective(current_OpenMPIR_to_SageIII);
    break;
  }
  case OMPD_begin_declare_variant: {
    result =
        convertOmpBeginDeclareVariantDirective(current_OpenMPIR_to_SageIII);
    break;
  }
  case OMPD_end_declare_variant: {
    result = convertOmpEndDeclareVariantDirective(current_OpenMPIR_to_SageIII);
    break;
  }
  case OMPD_declare_target: {
    result = convertOmpDeclareTargetDirective(current_OpenMPIR_to_SageIII);
    break;
  }
  case OMPD_end_declare_target: {
    result = convertOmpEndDeclareTargetDirective(current_OpenMPIR_to_SageIII);
    break;
  }
  case OMPD_flush: {
    result = convertOmpFlushDirective(current_OpenMPIR_to_SageIII);
    break;
  }
  case OMPD_allocate: {
    result = convertOmpAllocateDirective(current_OpenMPIR_to_SageIII);
    break;
  }
  case OMPD_taskyield: {
    result = new SgOmpTaskyieldStatement();
    break;
  }
  case OMPD_threadprivate: {
    result = convertOmpThreadprivateStatement(current_OpenMPIR_to_SageIII);
    break;
  }
  case OMPD_ompx: {
    // Keep implementation-defined extension directives as source pragmas.
    result = NULL;
    break;
  }
  default: {
    printf("Unknown directive is found.\n");
  }
  }
  SgPragmaDeclaration *pdecl = current_OpenMPIR_to_SageIII.first;
  if (result == NULL) {
    if (SgSourceFile *source_file = getEnclosingSourceFile(pdecl)) {
      const bool is_fortran_file =
          source_file->get_Fortran_only() || source_file->get_F77_only() ||
          source_file->get_F90_only() || source_file->get_F95_only() ||
          source_file->get_F2003_only();
      if (is_fortran_file &&
          pdecl->getAttribute(kFortranKeepOpenMPPragmaAttributeName) == NULL) {
        pdecl->addNewAttribute(kFortranKeepOpenMPPragmaAttributeName,
                               new KeepFortranOpenMPPragmaAttribute());
      }
    }
    // Keep the original pragma when conversion is unavailable and preserve it
    // through Fortran pragma cleanup.
    return pdecl;
  }
  setOneSourcePositionForTransformation(result);
  copyStartFileInfo(pdecl, result);
  copyEndFileInfo(pdecl, result);
  if (SgLocatedNode *located_result = isSgLocatedNode(result)) {
    located_result->setOutputInCodeGeneration();
  }
  if (pdecl->getAttribute(kOmpFortranEndAttributeName) != NULL &&
      result->getAttribute(kOmpFortranEndAttributeName) == NULL) {
    result->addNewAttribute(kOmpFortranEndAttributeName,
                            new OmpFortranEndAttribute());
  }

  //! For C/C++ replace OpenMP pragma declaration with an SgOmpxxStatement
  SgScopeStatement *scope = pdecl->get_scope();
  ROSE_ASSERT(scope != NULL);
  bool is_fortran_file = false;
  if (SgSourceFile *source_file = getEnclosingSourceFile(pdecl)) {
    is_fortran_file =
        source_file->get_Fortran_only() || source_file->get_F77_only() ||
        source_file->get_F90_only() || source_file->get_F95_only() ||
        source_file->get_F2003_only();
  }

  if (isSgGlobal(scope) != NULL && isSgDeclarationStatement(result) == NULL) {
    // Global scope stores declaration statements only. Preserve the original
    // pragma if conversion yields a non-declaration directive form.
    return pdecl;
  }

  if (!is_fortran_file) {
    relocatePendingCommentedDirectivesForPragma(pdecl, result);
    moveUpPreprocessingInfo(result,
                            pdecl); // keep #ifdef etc attached to the pragma
  }
  replaceStatement(pdecl, result);

  return result;
}

SgStatement *
convertVariantDirective(std::pair<SgPragmaDeclaration *, OpenMPDirective *>
                            current_OpenMPIR_to_SageIII) {
  SgStatement *result =
      convertVariantBodyDirective(current_OpenMPIR_to_SageIII);
  if (result == NULL) {
    OpenMPDirective *directive = current_OpenMPIR_to_SageIII.second;
    if (directive == NULL || directive->getKind() == OMPD_end) {
      return NULL;
    }

    // Preserve unsupported variant directives opaquely so declare-variant
    // match/when selectors round-trip instead of silently dropping them.
    auto *opaque_directive = new SgOmpClauseStatement();
    std::string opaque_spelling = trimWhitespaceCopy(directive->toString());
    ROSE_ASSERT(!opaque_spelling.empty());
    setDirectiveSpellingOverride(opaque_directive, opaque_spelling);
    result = opaque_directive;
  }

  setOneSourcePositionForTransformation(result);
  return result;
}

SgOmpBodyStatement *
convertCombinedBodyDirective(std::pair<SgPragmaDeclaration *, OpenMPDirective *>
                                 current_OpenMPIR_to_SageIII) {

  OpenMPDirectiveKind directive_kind =
      current_OpenMPIR_to_SageIII.second->getKind();
  // directives like parallel and for have a following code block beside the
  // pragma itself.
  SgOmpBodyStatement *result = NULL;

  switch (directive_kind) {
  case OMPD_parallel_do:
  case OMPD_parallel_for:
  case OMPD_parallel_for_simd:
  case OMPD_parallel_sections:
  case OMPD_parallel_workshare: {
    result = convertOmpParallelStatementFromCombinedDirectives(
        current_OpenMPIR_to_SageIII);
    break;
  }
  default: {
    printf("Unknown directive is found.\n");
  }
  }
  return result;
}

SgOmpClause *
convertSimpleClause(SgStatement *directive,
                    std::pair<SgPragmaDeclaration *, OpenMPDirective *>
                        current_OpenMPIR_to_SageIII,
                    OpenMPClause *current_omp_clause) {
  SgOmpClause *sg_clause = NULL;
  OpenMPClauseKind clause_kind = current_omp_clause->getKind();
  switch (clause_kind) {
  case OMPC_nowait: {
    SgOmpNowaitClause *nowait_clause =
        new SgOmpNowaitClause((SgExpression *)NULL);
    SgExpression *nowait_expression = NULL;
    const std::vector<const OmpParsedExpression *> *parsed_nodes =
        getParsedClauseExpressionNodes(current_OpenMPIR_to_SageIII.second,
                                       current_omp_clause);
    if (parsed_nodes != nullptr) {
      for (const OmpParsedExpression *parsed : *parsed_nodes) {
        if (parsed == nullptr) {
          continue;
        }
        nowait_expression = cloneParsedExpressionNode(parsed);
        if (nowait_expression != NULL) {
          break;
        }
      }
    }
    if (nowait_expression == NULL) {
      std::vector<const char *> *current_expressions =
          current_omp_clause->getExpressions();
      if (current_expressions != NULL && !current_expressions->empty()) {
        const char *expression_text = current_expressions->front();
        if (expression_text != NULL && expression_text[0] != '\0') {
          nowait_expression = parseOmpExpression(
              current_OpenMPIR_to_SageIII.first, current_omp_clause->getKind(),
              expression_text);
        }
      }
    }
    if (nowait_expression != NULL) {
      SgOmpExpressionClause *expression_clause =
          isSgOmpExpressionClause(nowait_clause);
      ROSE_ASSERT(expression_clause != NULL);
      expression_clause->set_expression(nowait_expression);
      nowait_expression->set_parent(expression_clause);
    }
    sg_clause = nowait_clause;
    break;
  }
  case OMPC_nogroup: {
    sg_clause = new SgOmpNogroupClause();
    break;
  }
  case OMPC_untied: {
    sg_clause = new SgOmpUntiedClause();
    break;
  }
  case OMPC_mergeable: {
    sg_clause = new SgOmpMergeableClause();
    break;
  }
  case OMPC_read: {
    sg_clause = new SgOmpReadClause();
    break;
  }
  case OMPC_reverse_offload: {
    sg_clause = new SgOmpReverseOffloadClause();
    break;
  }
  case OMPC_unified_address: {
    sg_clause = new SgOmpUnifiedAddressClause();
    break;
  }
  case OMPC_unified_shared_memory: {
    sg_clause = new SgOmpUnifiedSharedMemoryClause();
    break;
  }
  case OMPC_dynamic_allocators: {
    sg_clause = new SgOmpDynamicAllocatorsClause();
    break;
  }
  case OMPC_write: {
    sg_clause = new SgOmpWriteClause();
    break;
  }
  case OMPC_threads: {
    sg_clause = new SgOmpThreadsClause();
    break;
  }
  case OMPC_simd: {
    sg_clause = new SgOmpSimdClause();
    break;
  }
  case OMPC_compare: {
    sg_clause = new SgOmpCompareClause();
    break;
  }
  case OMPC_weak: {
    sg_clause = new SgOmpWeakClause();
    break;
  }
  case OMPC_update: {
    sg_clause = new SgOmpUpdateClause();
    break;
  }
  case OMPC_capture: {
    sg_clause = new SgOmpCaptureClause();
    break;
  }
  case OMPC_seq_cst: {
    sg_clause = new SgOmpSeqCstClause();
    break;
  }
  case OMPC_acq_rel: {
    sg_clause = new SgOmpAcqRelClause();
    break;
  }
  case OMPC_release: {
    sg_clause = new SgOmpReleaseClause();
    break;
  }
  case OMPC_acquire: {
    sg_clause = new SgOmpAcquireClause();
    break;
  }
  case OMPC_relaxed: {
    sg_clause = new SgOmpRelaxedClause();
    break;
  }
  case OMPC_destroy: {
    sg_clause = new SgOmpDestroyClause();
    break;
  }
  case OMPC_inbranch: {
    sg_clause = new SgOmpInbranchClause();
    break;
  }
  case OMPC_notinbranch: {
    sg_clause = new SgOmpNotinbranchClause();
    break;
  }
  case OMPC_parallel: {
    sg_clause = new SgOmpParallelClause();
    break;
  }
  case OMPC_sections: {
    sg_clause = new SgOmpSectionsClause();
    break;
  }
  case OMPC_for: {
    sg_clause = new SgOmpForClause();
    break;
  }
  case OMPC_taskgroup: {
    sg_clause = new SgOmpTaskgroupClause();
    break;
  }
  case OMPC_full: {
    sg_clause = new SgOmpFullClause();
    break;
  }
  default: {
    cerr << "error: unknown clause " << endl;
    ROSE_ABORT();
  }
  };
  setOneSourcePositionForTransformation(sg_clause);
  if (current_OpenMPIR_to_SageIII.second->getKind() == OMPD_declare_simd) {
    ((SgOmpDeclareSimdStatement *)directive)
        ->get_clauses()
        .push_back(sg_clause);
  } else if (current_OpenMPIR_to_SageIII.second->getKind() ==
             OMPD_target_update) {
    ((SgOmpTargetUpdateStatement *)directive)
        ->get_clauses()
        .push_back(sg_clause);
  } else if (current_OpenMPIR_to_SageIII.second->getKind() == OMPD_cancel ||
             current_OpenMPIR_to_SageIII.second->getKind() ==
                 OMPD_cancellation_point) {
    addOmpClause(directive, sg_clause);
  } else if (current_OpenMPIR_to_SageIII.second->getKind() == OMPD_requires) {
    ((SgOmpRequiresStatement *)directive)->get_clauses().push_back(sg_clause);
  } else if (current_OpenMPIR_to_SageIII.second->getKind() == OMPD_flush) {
    ((SgOmpFlushStatement *)directive)->get_clauses().push_back(sg_clause);
  } else if (current_OpenMPIR_to_SageIII.second->getKind() == OMPD_taskwait) {
    ((SgOmpTaskwaitStatement *)directive)->get_clauses().push_back(sg_clause);
  } else {
    addOmpClause(directive, sg_clause);
  }
  sg_clause->set_parent(directive);
  return sg_clause;
}

SgStatement *
convertNonBodyDirective(std::pair<SgPragmaDeclaration *, OpenMPDirective *>
                            current_OpenMPIR_to_SageIII) {

  OpenMPDirectiveKind directive_kind =
      current_OpenMPIR_to_SageIII.second->getKind();
  SgStatement *result = NULL;
  OpenMPClauseKind clause_kind;

  switch (directive_kind) {
  case OMPD_cancellation_point: {
    result = new SgOmpCancellationPointStatement();
    break;
  }
  case OMPD_declare_mapper: {
    result = new SgOmpDeclareMapperStatement();
    OpenMPDeclareMapperDirective *mapper_directive =
        static_cast<OpenMPDeclareMapperDirective *>(
            current_OpenMPIR_to_SageIII.second);
    SgOmpDeclareMapperStatement *sg_mapper =
        isSgOmpDeclareMapperStatement(result);
    ROSE_ASSERT(mapper_directive != nullptr);
    ROSE_ASSERT(sg_mapper != nullptr);

    const NormalizedDeclareMapperData mapper_data =
        normalizeDeclareMapperData(mapper_directive);
    sg_mapper->set_identifier(mapper_data.identifier);

    if (sg_mapper->get_identifier() ==
            SgOmpClause::e_omp_declare_mapper_identifier_user &&
        !mapper_data.user_defined_identifier.empty()) {
      SgExpression *user_defined_identifier = parseMapperIdentifierExpression(
          current_OpenMPIR_to_SageIII.first, OMPC_map, nullptr,
          mapper_data.user_defined_identifier);
      sg_mapper->set_user_defined_identifier(user_defined_identifier);
    }

    if (!mapper_data.mapper_type.empty()) {
      SgType *resolved_mapper_type = resolveDeclareMapperType(
          current_OpenMPIR_to_SageIII.first, mapper_data.mapper_type);
      if (resolved_mapper_type == nullptr) {
        MLOG_ERROR_C("ompAstConstruction",
                     "Failed to resolve declare mapper type '%s'.\n",
                     mapper_data.mapper_type.c_str());
        ROSE_ABORT();
      }
      SgExpression *mapper_type =
          SageBuilder::buildTypeExpression(resolved_mapper_type);
      sg_mapper->set_mapper_type(mapper_type);
      mapper_type->set_parent(sg_mapper);
    }

    if (!mapper_data.mapper_variable.empty()) {
      SgExpression *mapper_variable = parseClauseExpressionWithCache(
          current_OpenMPIR_to_SageIII.first, OMPC_map, nullptr,
          mapper_data.mapper_variable);
      sg_mapper->set_mapper_variable(mapper_variable);
      if (mapper_variable != nullptr) {
        mapper_variable->set_parent(sg_mapper);
      }
    }
    break;
  }
  case OMPD_cancel: {
    result = new SgOmpCancelStatement();
    break;
  }
  case OMPD_target_update: {
    result = new SgOmpTargetUpdateStatement();
    break;
  }
  case OMPD_ordered: {
    result = new SgOmpOrderedDependStatement();
    break;
  }
  default: {
    printf("Unknown directive is found.\n");
  }
  }
  if (SgDeclarationStatement *decl = isSgDeclarationStatement(result)) {
    if (decl->get_firstNondefiningDeclaration() == nullptr &&
        decl->get_definingDeclaration() == nullptr) {
      decl->set_firstNondefiningDeclaration(decl);
    }
  }
  // extract all the clauses based on the vector of clauses in the original
  // order
  std::vector<OpenMPClause *> *all_clauses =
      current_OpenMPIR_to_SageIII.second->getClausesInOriginalOrder();
  std::vector<OpenMPClause *>::iterator clause_iter;
  for (clause_iter = all_clauses->begin(); clause_iter != all_clauses->end();
       clause_iter++) {
    clause_kind = (*clause_iter)->getKind();
    switch (clause_kind) {
    case OMPC_device:
    case OMPC_if: {
      convertExpressionClause(isSgStatement(result),
                              current_OpenMPIR_to_SageIII, *clause_iter);
      break;
    }
    case OMPC_parallel:
    case OMPC_sections:
    case OMPC_for:
    case OMPC_nowait:
    case OMPC_reverse_offload:
    case OMPC_unified_address:
    case OMPC_unified_shared_memory:
    case OMPC_dynamic_allocators:
    case OMPC_taskgroup: {
      convertSimpleClause(isSgStatement(result), current_OpenMPIR_to_SageIII,
                          *clause_iter);
      break;
    }
    case OMPC_depend: {
      convertDependClause(isSgStatement(result), current_OpenMPIR_to_SageIII,
                          *clause_iter);
      break;
    }
    case OMPC_map: {
      convertMapClause(isSgStatement(result), current_OpenMPIR_to_SageIII,
                       *clause_iter);
      break;
    }
    case OMPC_to: {
      convertToClause(isSgStatement(result), current_OpenMPIR_to_SageIII,
                      *clause_iter);
      break;
    }
    case OMPC_from: {
      convertFromClause(isSgStatement(result), current_OpenMPIR_to_SageIII,
                        *clause_iter);
      break;
    }
    default: {
      cerr << "error: unknown clause " << endl;
      ROSE_ABORT();
    }
    };
  };
  return result;
}

SgStatement *
convertBodyDirective(std::pair<SgPragmaDeclaration *, OpenMPDirective *>
                         current_OpenMPIR_to_SageIII) {

  OpenMPDirectiveKind directive_kind =
      current_OpenMPIR_to_SageIII.second->getKind();
  auto directive_requires_structured_block = [](OpenMPDirectiveKind kind) {
    switch (kind) {
    case OMPD_target_enter_data:
    case OMPD_target_exit_data:
      return false;
    default:
      return kind != OMPD_end;
    }
  };

  // Most body directives consume the following structured block. Directives
  // like target enter/exit data are standalone and must not steal the next
  // statement as a synthetic body.
  SgStatement *body = NULL;
  if (directive_requires_structured_block(directive_kind)) {
    body = getOpenMPBlockBody(current_OpenMPIR_to_SageIII);
    if (body != NULL) {
      removeStatement(body, false);
    } else {
      return NULL;
    }
  }
  SgStatement *result = NULL;
  OpenMPClauseKind clause_kind;

  switch (directive_kind) {
  case OMPD_do: {
    result = new SgOmpDoStatement(NULL, body);
    break;
  }
  case OMPD_for: {
    result = new SgOmpForStatement(NULL, body);
    break;
  }
  case OMPD_ordered: {
    result = new SgOmpOrderedStatement(NULL, body);
    break;
  }
  case OMPD_parallel: {
    result = new SgOmpParallelStatement(NULL, body);
    break;
  }
  case OMPD_teams: {
    result = new SgOmpTeamsStatement(NULL, body);
    break;
  }
  case OMPD_atomic: {
    result = new SgOmpAtomicStatement(NULL, body);
    break;
  }
  case OMPD_taskgroup: {
    result = new SgOmpTaskgroupStatement(NULL, body);
    break;
  }
  case OMPD_dispatch: {
    result = new SgOmpDispatchStatement(NULL, body);
    break;
  }
  case OMPD_master: {
    result = new SgOmpMasterStatement(NULL, body);
    break;
  }
  case OMPD_masked: {
    result = new SgOmpMasterStatement(NULL, body);
    setDirectiveSpellingOverride(result, "masked");
    break;
  }
  case OMPD_distribute: {
    result = new SgOmpDistributeStatement(NULL, body);
    break;
  }
  case OMPD_loop: {
    result = new SgOmpLoopStatement(NULL, body);
    break;
  }
  case OMPD_scan: {
    result = new SgOmpScanStatement(NULL, body);
    break;
  }
  case OMPD_taskloop: {
    result = new SgOmpTaskloopStatement(NULL, body);
    break;
  }
  case OMPD_target_enter_data: {
    result = new SgOmpTargetEnterDataStatement(NULL, body);
    break;
  }
  case OMPD_target_exit_data: {
    result = new SgOmpTargetExitDataStatement(NULL, body);
    break;
  }
  case OMPD_task: {
    result = new SgOmpTaskStatement(NULL, body);
    break;
  }
  case OMPD_target_data: {
    result = new SgOmpTargetDataStatement(NULL, body);
    break;
  }
  case OMPD_simd: {
    result = new SgOmpSimdStatement(NULL, body);
    break;
  }
  case OMPD_single: {
    result = new SgOmpSingleStatement(NULL, body);
    break;
  }
  case OMPD_for_simd: {
    result = new SgOmpForSimdStatement(NULL, body);
    break;
  }
  case OMPD_target: {
    result = new SgOmpTargetStatement(NULL, body);
    break;
  }
  case OMPD_critical: {
    std::string name =
        ((OpenMPCriticalDirective *)(current_OpenMPIR_to_SageIII.second))
            ->getCriticalName();
    result = new SgOmpCriticalStatement(NULL, body, SgName(name));
    break;
  }
  case OMPD_depobj: {
    std::string name =
        ((OpenMPDepobjDirective *)(current_OpenMPIR_to_SageIII.second))
            ->getDepobj();
    result = new SgOmpDepobjStatement(NULL, body, SgName(name));
    break;
  }
  case OMPD_sections: {
    result = new SgOmpSectionsStatement(NULL, body);
    break;
  }
  case OMPD_section: {
    result = new SgOmpSectionStatement(NULL, body);
    break;
  }
  case OMPD_metadirective: {
    result = new SgOmpMetadirectiveStatement(NULL, body);
    break;
  }
  case OMPD_target_parallel_for: {
    result = new SgOmpTargetParallelForStatement(NULL, body);
    break;
  }
  case OMPD_target_parallel: {
    result = new SgOmpTargetParallelStatement(NULL, body);
    break;
  }
  case OMPD_distribute_simd: {
    result = new SgOmpDistributeSimdStatement(NULL, body);
    break;
  }
  case OMPD_distribute_parallel_for: {
    result = new SgOmpDistributeParallelForStatement(NULL, body);
    break;
  }
  case OMPD_distribute_parallel_for_simd: {
    result = new SgOmpDistributeParallelForSimdStatement(NULL, body);
    break;
  }
  case OMPD_taskloop_simd: {
    result = new SgOmpTaskloopSimdStatement(NULL, body);
    break;
  }
  case OMPD_target_parallel_for_simd: {
    result = new SgOmpTargetParallelForSimdStatement(NULL, body);
    break;
  }
  case OMPD_target_parallel_loop: {
    result = new SgOmpTargetParallelLoopStatement(NULL, body);
    break;
  }
  case OMPD_target_simd: {
    result = new SgOmpTargetSimdStatement(NULL, body);
    break;
  }
  case OMPD_target_teams: {
    result = new SgOmpTargetTeamsStatement(NULL, body);
    break;
  }
  case OMPD_target_teams_distribute: {
    result = new SgOmpTargetTeamsDistributeStatement(NULL, body);
    break;
  }
  case OMPD_target_teams_distribute_simd: {
    result = new SgOmpTargetTeamsDistributeSimdStatement(NULL, body);
    break;
  }
  case OMPD_target_teams_loop: {
    result = new SgOmpTargetTeamsLoopStatement(NULL, body);
    break;
  }
  case OMPD_target_teams_distribute_parallel_for: {
    result = new SgOmpTargetTeamsDistributeParallelForStatement(NULL, body);
    break;
  }
  case OMPD_target_teams_distribute_parallel_for_simd: {
    result = new SgOmpTargetTeamsDistributeParallelForSimdStatement(NULL, body);
    break;
  }
  case OMPD_master_taskloop_simd: {
    result = new SgOmpMasterTaskloopSimdStatement(NULL, body);
    break;
  }
  case OMPD_masked_taskloop_simd: {
    result = new SgOmpMasterTaskloopSimdStatement(NULL, body);
    setDirectiveSpellingOverride(result, "masked taskloop simd");
    break;
  }
  case OMPD_parallel_master_taskloop: {
    result = new SgOmpParallelMasterTaskloopStatement(NULL, body);
    break;
  }
  case OMPD_parallel_master_taskloop_simd: {
    result = new SgOmpParallelMasterTaskloopSimdStatement(NULL, body);
    break;
  }
  case OMPD_teams_distribute: {
    result = new SgOmpTeamsDistributeStatement(NULL, body);
    break;
  }
  case OMPD_teams_distribute_simd: {
    result = new SgOmpTeamsDistributeSimdStatement(NULL, body);
    break;
  }
  case OMPD_teams_distribute_parallel_for: {
    result = new SgOmpTeamsDistributeParallelForStatement(NULL, body);
    break;
  }
  case OMPD_teams_distribute_parallel_for_simd: {
    result = new SgOmpTeamsDistributeParallelForSimdStatement(NULL, body);
    break;
  }
  case OMPD_teams_loop: {
    result = new SgOmpTeamsLoopStatement(NULL, body);
    break;
  }
  case OMPD_parallel_master: {
    result = new SgOmpParallelMasterStatement(NULL, body);
    break;
  }
  case OMPD_master_taskloop: {
    result = new SgOmpMasterTaskloopStatement(NULL, body);
    break;
  }
  case OMPD_masked_taskloop: {
    result = new SgOmpMasterTaskloopStatement(NULL, body);
    setDirectiveSpellingOverride(result, "masked taskloop");
    break;
  }
  case OMPD_parallel_loop: {
    result = new SgOmpParallelLoopStatement(NULL, body);
    break;
  }
  case OMPD_end: {
    return result;
  }
  case OMPD_workshare: {
    result = new SgOmpWorkshareStatement(NULL, body);
    break;
  }
  case OMPD_unroll: {
    result = new SgOmpUnrollStatement(NULL, body);
    break;
  }
  case OMPD_tile: {
    result = new SgOmpTileStatement(NULL, body);
    break;
  }
  default: {
    printf("Unknown directive is found.\n");
  }
  }
  if (body != NULL) {
    body->set_parent(result);
  }
  // extract all the clauses based on the vector of clauses in the original
  // order
  std::vector<OpenMPClause *> *all_clauses =
      current_OpenMPIR_to_SageIII.second->getClausesInOriginalOrder();
  std::vector<OpenMPClause *>::iterator clause_iter;
  for (clause_iter = all_clauses->begin(); clause_iter != all_clauses->end();
       clause_iter++) {
    clause_kind = (*clause_iter)->getKind();
    switch (clause_kind) {
    case OMPC_if:
    case OMPC_num_teams:
    case OMPC_final:
    case OMPC_priority:
    case OMPC_hint:
    case OMPC_safelen:
    case OMPC_simdlen:
    case OMPC_ordered:
    case OMPC_collapse:
    case OMPC_thread_limit:
    case OMPC_device:
    case OMPC_grainsize:
    case OMPC_detach:
    case OMPC_num_tasks:
    case OMPC_num_threads:
    case OMPC_nocontext:
    case OMPC_novariants:
    case OMPC_partial: {
      convertExpressionClause(result, current_OpenMPIR_to_SageIII,
                              *clause_iter);
      break;
    }
    case OMPC_sizes: {
      convertSizesClause(result, current_OpenMPIR_to_SageIII, *clause_iter);
      break;
    }
    case OMPC_default: {
      convertDefaultClause(isSgOmpClauseBodyStatement(result),
                           current_OpenMPIR_to_SageIII, *clause_iter);
      break;
    }
    case OMPC_proc_bind: {
      convertProcBindClause(isSgOmpClauseBodyStatement(result),
                            current_OpenMPIR_to_SageIII, *clause_iter);
      break;
    }
    case OMPC_order: {
      convertOrderClause(result, current_OpenMPIR_to_SageIII, *clause_iter);
      break;
    }
    case OMPC_bind: {
      convertBindClause(isSgOmpClauseBodyStatement(result),
                        current_OpenMPIR_to_SageIII, *clause_iter);
      break;
    }
    case OMPC_when: {
      convertWhenClause(isSgOmpClauseBodyStatement(result),
                        current_OpenMPIR_to_SageIII, *clause_iter);
      break;
    }
    case OMPC_inbranch:
    case OMPC_notinbranch: {
      convertSimpleClause(isSgStatement(result), current_OpenMPIR_to_SageIII,
                          *clause_iter);
      break;
    }
    case OMPC_uses_allocators: {
      convertUsesAllocatorsClause(isSgOmpClauseBodyStatement(result),
                                  current_OpenMPIR_to_SageIII, *clause_iter);
      break;
    }
    case OMPC_read:
    case OMPC_write:
    case OMPC_threads:
    case OMPC_simd:
    case OMPC_compare:
    case OMPC_weak:
    case OMPC_update:
    case OMPC_capture:
    case OMPC_seq_cst:
    case OMPC_acq_rel:
    case OMPC_release:
    case OMPC_acquire:
    case OMPC_relaxed:
    case OMPC_mergeable:
    case OMPC_untied:
    case OMPC_nogroup:
    case OMPC_destroy:
    case OMPC_nowait:
    case OMPC_full: {
      convertSimpleClause(result, current_OpenMPIR_to_SageIII, *clause_iter);
      break;
    }
    case OMPC_fail: {
      convertFailClause(isSgStatement(result), current_OpenMPIR_to_SageIII,
                        *clause_iter);
      break;
    }
    case OMPC_schedule: {
      convertScheduleClause(result, current_OpenMPIR_to_SageIII, *clause_iter);
      break;
    }
    case OMPC_dist_schedule: {
      convertDistScheduleClause(isSgOmpClauseBodyStatement(result),
                                current_OpenMPIR_to_SageIII, *clause_iter);
      break;
    }
    case OMPC_defaultmap: {
      convertDefaultmapClause(isSgOmpClauseBodyStatement(result),
                              current_OpenMPIR_to_SageIII, *clause_iter);
      break;
    }
    case OMPC_map: {
      convertMapClause(isSgOmpClauseBodyStatement(result),
                       current_OpenMPIR_to_SageIII, *clause_iter);
      break;
    }
    case OMPC_depend: {
      convertDependClause(isSgOmpClauseBodyStatement(result),
                          current_OpenMPIR_to_SageIII, *clause_iter);
      break;
    }
    case OMPC_affinity: {
      convertAffinityClause(isSgOmpClauseBodyStatement(result),
                            current_OpenMPIR_to_SageIII, *clause_iter);
      break;
    }
    case OMPC_depobj_update: {
      convertDepobjUpdateClause(isSgOmpClauseBodyStatement(result),
                                current_OpenMPIR_to_SageIII, *clause_iter);
      break;
    }
    default: {
      convertClause(result, current_OpenMPIR_to_SageIII, *clause_iter);
    }
    };
  };

  initializeGeneratedOpenMPStatement(result);
  return result;
}

// Convert an OpenMPIR Declare Simd Directive to a ROSE node
SgStatement *convertOmpDeclareSimdDirective(
    std::pair<SgPragmaDeclaration *, OpenMPDirective *>
        current_OpenMPIR_to_SageIII) {
  SgOmpDeclareSimdStatement *result = new SgOmpDeclareSimdStatement();
  result->set_firstNondefiningDeclaration(result);

  std::vector<OpenMPClause *> *all_clauses =
      current_OpenMPIR_to_SageIII.second->getClausesInOriginalOrder();
  OpenMPClauseKind clause_kind;
  std::vector<OpenMPClause *>::iterator clause_iter;
  for (clause_iter = all_clauses->begin(); clause_iter != all_clauses->end();
       clause_iter++) {
    clause_kind = (*clause_iter)->getKind();
    switch (clause_kind) {
    case OMPC_simdlen: {
      convertExpressionClause(isSgStatement(result),
                              current_OpenMPIR_to_SageIII, *clause_iter);
      break;
    }
    case OMPC_inbranch:
    case OMPC_notinbranch: {
      convertSimpleClause(isSgStatement(result), current_OpenMPIR_to_SageIII,
                          *clause_iter);
      break;
    }
    case OMPC_aligned:
    case OMPC_linear:
    case OMPC_uniform: {
      convertClause(isSgStatement(result), current_OpenMPIR_to_SageIII,
                    *clause_iter);
      break;
    }
    default: {
      convertClause(isSgStatement(result), current_OpenMPIR_to_SageIII,
                    *clause_iter);
    }
    };
  };
  return result;
}

SgStatement *convertOmpDeclareVariantDirective(
    std::pair<SgPragmaDeclaration *, OpenMPDirective *>
        current_OpenMPIR_to_SageIII) {
  SgOmpDeclareVariantStatement *result = new SgOmpDeclareVariantStatement();
  result->set_firstNondefiningDeclaration(result);

  ROSE_ASSERT(current_OpenMPIR_to_SageIII.second != nullptr);
  ROSE_ASSERT(current_OpenMPIR_to_SageIII.second->getKind() ==
              OMPD_declare_variant);

  auto *declare_variant_directive =
      static_cast<OpenMPDeclareVariantDirective *>(
          current_OpenMPIR_to_SageIII.second);
  SgExpression *variant_function_ref = parseDeclareVariantExpression(
      current_OpenMPIR_to_SageIII.first,
      declare_variant_directive->getVariantFuncID());
  result->set_variant_function_ref(variant_function_ref);
  if (variant_function_ref != nullptr) {
    variant_function_ref->set_parent(result);
  }

  std::vector<OpenMPClause *> *all_clauses =
      current_OpenMPIR_to_SageIII.second->getClausesInOriginalOrder();
  for (OpenMPClause *clause : *all_clauses) {
    if (clause == nullptr) {
      continue;
    }

    switch (clause->getKind()) {
    case OMPC_match:
      convertMatchClause(isSgStatement(result), current_OpenMPIR_to_SageIII,
                         clause);
      break;
    case OMPC_adjust_args:
      convertAdjustArgsClause(isSgStatement(result),
                              current_OpenMPIR_to_SageIII, clause);
      break;
    case OMPC_append_args:
      convertAppendArgsClause(isSgStatement(result),
                              current_OpenMPIR_to_SageIII, clause);
      break;
    default:
      convertClause(isSgStatement(result), current_OpenMPIR_to_SageIII, clause);
      break;
    }
  }

  return result;
}

SgStatement *convertOmpBeginDeclareVariantDirective(
    std::pair<SgPragmaDeclaration *, OpenMPDirective *>
        current_OpenMPIR_to_SageIII) {
  SgOmpBeginDeclareVariantStatement *result =
      new SgOmpBeginDeclareVariantStatement();
  result->set_firstNondefiningDeclaration(result);

  if (const OmpDeclareVariantRegionInfo *region =
          findDeclareVariantRegionInfo(current_OpenMPIR_to_SageIII.first)) {
    result->set_captured_region(region->captured_region);
  }

  std::vector<OpenMPClause *> *all_clauses =
      current_OpenMPIR_to_SageIII.second->getClausesInOriginalOrder();
  for (OpenMPClause *clause : *all_clauses) {
    if (clause == nullptr) {
      continue;
    }

    switch (clause->getKind()) {
    case OMPC_match:
      convertMatchClause(isSgStatement(result), current_OpenMPIR_to_SageIII,
                         clause);
      break;
    case OMPC_adjust_args:
      convertAdjustArgsClause(isSgStatement(result),
                              current_OpenMPIR_to_SageIII, clause);
      break;
    case OMPC_append_args:
      convertAppendArgsClause(isSgStatement(result),
                              current_OpenMPIR_to_SageIII, clause);
      break;
    default:
      convertClause(isSgStatement(result), current_OpenMPIR_to_SageIII, clause);
      break;
    }
  }

  return result;
}

SgStatement *convertOmpEndDeclareVariantDirective(
    std::pair<SgPragmaDeclaration *, OpenMPDirective *>
        current_OpenMPIR_to_SageIII) {
  SgOmpEndDeclareVariantStatement *result =
      new SgOmpEndDeclareVariantStatement();
  result->set_firstNondefiningDeclaration(result);
  return result;
}

// Convert an OpenMPIR Declare Target Directive to a ROSE node
SgStatement *convertOmpDeclareTargetDirective(
    std::pair<SgPragmaDeclaration *, OpenMPDirective *>
        current_OpenMPIR_to_SageIII) {
  SgOmpDeclareTargetStatement *result = new SgOmpDeclareTargetStatement();
  result->set_firstNondefiningDeclaration(result);
  SgOmpClause::omp_when_context_kind_enum device_type_kind =
      SgOmpClause::e_omp_when_context_kind_unknown;

  OpenMPDeclareTargetDirective *declare_target_directive = NULL;
  if (current_OpenMPIR_to_SageIII.second->getKind() == OMPD_declare_target) {
    declare_target_directive = static_cast<OpenMPDeclareTargetDirective *>(
        current_OpenMPIR_to_SageIII.second);
  }
  if (declare_target_directive != NULL) {
    std::vector<std::string> *extended_list =
        declare_target_directive->getExtendedList();
    if (extended_list != NULL && !extended_list->empty()) {
      omp_variable_list.clear();
      array_dimensions.clear();

      for (const std::string &item : *extended_list) {
        const std::string trimmed_item = trimWhitespaceCopy(item);
        if (trimmed_item.empty()) {
          continue;
        }
        parseOmpArraySection(current_OpenMPIR_to_SageIII.first, OMPC_to,
                             trimmed_item);
      }

      if (!omp_variable_list.empty()) {
        SgExprListExp *explist = buildExprListExp();
        SgOmpToClause *extended_to_clause =
            new SgOmpToClause(explist, SgOmpClause::e_omp_to_kind_unknown);
        buildVariableList(extended_to_clause);
        explist->set_parent(extended_to_clause);
        extended_to_clause->set_array_dimensions(array_dimensions);
        extended_to_clause->setAttribute(
            kOmpDeclareTargetExtendedListAttrName,
            new OmpDeclareTargetExtendedListAttribute());
        setOneSourcePositionForTransformation(extended_to_clause);
        result->get_clauses().push_back(extended_to_clause);
        extended_to_clause->set_parent(result);
      }

      array_dimensions.clear();
      omp_variable_list.clear();
    }
  }

  std::vector<OpenMPClause *> *all_clauses =
      current_OpenMPIR_to_SageIII.second->getClausesInOriginalOrder();
  OpenMPClauseKind clause_kind;
  std::vector<OpenMPClause *>::iterator clause_iter;
  for (clause_iter = all_clauses->begin(); clause_iter != all_clauses->end();
       clause_iter++) {
    clause_kind = (*clause_iter)->getKind();
    switch (clause_kind) {
    case OMPC_to:
      convertToClause(isSgStatement(result), current_OpenMPIR_to_SageIII,
                      *clause_iter);
      break;
    case OMPC_device_type: {
      device_type_kind = toSgOmpDeclareTargetDeviceTypeKind(
          ((OpenMPDeviceTypeClause *)*clause_iter)->getDeviceTypeClauseKind());
      break;
    }
    default:
      convertClause(isSgStatement(result), current_OpenMPIR_to_SageIII,
                    *clause_iter);
    };
  };
  result->set_device_type_kind(device_type_kind);
  return result;
}

// Convert an OpenMPIR End Declare Target Directive to a ROSE node
SgStatement *convertOmpEndDeclareTargetDirective(
    std::pair<SgPragmaDeclaration *, OpenMPDirective *>
        current_OpenMPIR_to_SageIII) {
  SgOmpEndDeclareTargetStatement *result = new SgOmpEndDeclareTargetStatement();
  result->set_firstNondefiningDeclaration(result);

  return result;
}

SgStatement *
convertOmpRequiresDirective(std::pair<SgPragmaDeclaration *, OpenMPDirective *>
                                current_OpenMPIR_to_SageIII) {
  SgOmpRequiresStatement *result = new SgOmpRequiresStatement();
  result->set_firstNondefiningDeclaration(result);
  std::vector<OpenMPClause *> *all_clauses =
      current_OpenMPIR_to_SageIII.second->getClausesInOriginalOrder();
  OpenMPClauseKind clause_kind;
  std::vector<OpenMPClause *>::iterator clause_iter;
  for (clause_iter = all_clauses->begin(); clause_iter != all_clauses->end();
       clause_iter++) {
    clause_kind = (*clause_iter)->getKind();
    switch (clause_kind) {
    case OMPC_reverse_offload:
    case OMPC_unified_address:
    case OMPC_unified_shared_memory:
    case OMPC_dynamic_allocators: {
      convertSimpleClause(isSgStatement(result), current_OpenMPIR_to_SageIII,
                          *clause_iter);
      break;
    }
    case OMPC_atomic_default_mem_order: {
      convertAtomicDefaultMemOrderClause(
          isSgStatement(result), current_OpenMPIR_to_SageIII, *clause_iter);
      break;
    }
    case OMPC_ext_implementation_defined_requirement: {
      convertExtImplementationDefinedRequirementClause(
          isSgStatement(result), current_OpenMPIR_to_SageIII, *clause_iter);
      break;
    }
    default: {
      convertClause(isSgStatement(result), current_OpenMPIR_to_SageIII,
                    *clause_iter);
    }
    };
  };
  return result;
}

SgStatement *
convertOmpTaskwaitDirective(std::pair<SgPragmaDeclaration *, OpenMPDirective *>
                                current_OpenMPIR_to_SageIII) {
  SgOmpTaskwaitStatement *result = new SgOmpTaskwaitStatement();
  result->set_firstNondefiningDeclaration(result);
  std::vector<OpenMPClause *> *all_clauses =
      current_OpenMPIR_to_SageIII.second->getClausesInOriginalOrder();
  OpenMPClauseKind clause_kind;
  std::vector<OpenMPClause *>::iterator clause_iter;
  for (clause_iter = all_clauses->begin(); clause_iter != all_clauses->end();
       clause_iter++) {
    clause_kind = (*clause_iter)->getKind();
    switch (clause_kind) {
    case OMPC_depend: {
      convertDependClause(isSgStatement(result), current_OpenMPIR_to_SageIII,
                          *clause_iter);
      break;
    }
    case OMPC_nowait: {
      convertSimpleClause(isSgStatement(result), current_OpenMPIR_to_SageIII,
                          *clause_iter);
      break;
    }
    default: {
    }
    };
  };
  return result;
}

// Convert an OpenMPIR Flush Directive to a ROSE node
SgStatement *
convertOmpFlushDirective(std::pair<SgPragmaDeclaration *, OpenMPDirective *>
                             current_OpenMPIR_to_SageIII) {
  SgOmpFlushStatement *statement = new SgOmpFlushStatement();
  OpenMPFlushDirective *current_ir =
      static_cast<OpenMPFlushDirective *>(current_OpenMPIR_to_SageIII.second);
  std::vector<OpenMPClause *> *all_clauses =
      current_OpenMPIR_to_SageIII.second->getClausesInOriginalOrder();
  OpenMPClauseKind clause_kind;
  std::vector<OpenMPClause *>::iterator clause_iter;
  for (clause_iter = all_clauses->begin(); clause_iter != all_clauses->end();
       clause_iter++) {
    clause_kind = (*clause_iter)->getKind();
    switch (clause_kind) {
    case OMPC_seq_cst:
    case OMPC_acq_rel:
    case OMPC_release:
    case OMPC_acquire: {
      convertSimpleClause(isSgStatement(statement), current_OpenMPIR_to_SageIII,
                          *clause_iter);
      break;
    }
    default: {
      convertClause(isSgStatement(statement), current_OpenMPIR_to_SageIII,
                    *clause_iter);
    }
    };
  };
  std::vector<std::string> *current_expressions = current_ir->getFlushList();
  if (current_expressions->size() != 0) {
    std::vector<std::string>::iterator iter;
    for (iter = current_expressions->begin();
         iter != current_expressions->end(); iter++) {
      std::string expr_string = std::string() + "varlist " + *iter + "\n";
      omp_exprparser_parser_init(current_OpenMPIR_to_SageIII.first,
                                 expr_string.c_str());
      omp_exprparser_parse();
    }
  }

  std::vector<std::pair<std::string, SgNode *>>::iterator iter;
  for (iter = omp_variable_list.begin(); iter != omp_variable_list.end();
       iter++) {
    if (SgExpression *expr = buildOmpVarExprFromNode((*iter).second)) {
      statement->get_variables().push_back(expr);
      expr->set_parent(statement);
    } else {
      cerr << "error: unhandled type of variable within a list:"
           << ((*iter).second)->class_name();
    }
  }
  current_expressions->clear();
  omp_variable_list.clear();
  return statement;
}

// Convert an OpenMPIR Allocate Directive to a ROSE node
SgStatement *
convertOmpAllocateDirective(std::pair<SgPragmaDeclaration *, OpenMPDirective *>
                                current_OpenMPIR_to_SageIII) {
  SgOmpAllocateStatement *statement = new SgOmpAllocateStatement();
  OpenMPAllocateDirective *current_ir = static_cast<OpenMPAllocateDirective *>(
      current_OpenMPIR_to_SageIII.second);
  std::vector<OpenMPClause *> *all_clauses =
      current_OpenMPIR_to_SageIII.second->getClausesInOriginalOrder();
  OpenMPClauseKind clause_kind;
  std::vector<OpenMPClause *>::iterator clause_iter;
  for (clause_iter = all_clauses->begin(); clause_iter != all_clauses->end();
       clause_iter++) {
    clause_kind = (*clause_iter)->getKind();
    switch (clause_kind) {
    case OMPC_allocator: {
      convertAllocatorClause(isSgOmpClauseStatement(statement),
                             current_OpenMPIR_to_SageIII, *clause_iter);
      break;
    }
    default: {
      convertClause(isSgStatement(statement), current_OpenMPIR_to_SageIII,
                    *clause_iter);
    }
    };
  };
  const std::vector<std::string> &current_expressions =
      current_ir->getAllocateList();
  if (!current_expressions.empty()) {
    for (const auto &expr : current_expressions) {
      std::string expr_string = std::string() + "varlist " + expr + "\n";
      omp_exprparser_parser_init(current_OpenMPIR_to_SageIII.first,
                                 expr_string.c_str());
      omp_exprparser_parse();
    }
  }

  std::vector<std::pair<std::string, SgNode *>>::iterator iter;
  for (iter = omp_variable_list.begin(); iter != omp_variable_list.end();
       iter++) {
    if (SgExpression *expr = buildOmpVarExprFromNode((*iter).second)) {
      statement->get_variables().push_back(expr);
      expr->set_parent(statement);
    } else {
      cerr << "error: unhandled type of variable within a list:"
           << ((*iter).second)->class_name();
    }
  }
  omp_variable_list.clear();
  return statement;
}

// Convert an OpenMPIR Threadprivate Directive to a ROSE node
// Because we have to do some non-standard things, I'm putting this in a
// separate function
SgStatement *convertOmpThreadprivateStatement(
    std::pair<SgPragmaDeclaration *, OpenMPDirective *>
        current_OpenMPIR_to_SageIII) {
  SgOmpThreadprivateStatement *statement = new SgOmpThreadprivateStatement();
  OpenMPThreadprivateDirective *current_ir =
      static_cast<OpenMPThreadprivateDirective *>(
          current_OpenMPIR_to_SageIII.second);

  const std::vector<std::string> &current_expressions =
      current_ir->getThreadprivateList();
  if (!current_expressions.empty()) {
    for (const auto &expr : current_expressions) {
      std::string expr_string = std::string() + "varlist " + expr + "\n";
      omp_exprparser_parser_init(current_OpenMPIR_to_SageIII.first,
                                 expr_string.c_str());
      omp_exprparser_parse();
    }
  }

  std::vector<std::pair<std::string, SgNode *>>::iterator iter;
  for (iter = omp_variable_list.begin(); iter != omp_variable_list.end();
       iter++) {
    if (SgExpression *expr = buildOmpVarExprFromNode((*iter).second)) {
      statement->get_variables().push_back(expr);
      expr->set_parent(statement);
    } else {
      cerr << "error: unhandled type of variable within a list:"
           << ((*iter).second)->class_name();
    }
  }

  statement->set_definingDeclaration(statement);
  return statement;
}

SgOmpDepobjUpdateClause *
convertDepobjUpdateClause(SgOmpClauseBodyStatement *clause_body,
                          std::pair<SgPragmaDeclaration *, OpenMPDirective *>
                              current_OpenMPIR_to_SageIII,
                          OpenMPClause *current_omp_clause) {

  OpenMPDepobjUpdateClauseDependeceType modifier =
      ((OpenMPDepobjUpdateClause *)current_omp_clause)->getType();
  SgOmpClause::omp_depobj_modifier_enum sg_type =
      toSgOmpClauseDepobjModifierType(modifier);
  SgOmpDepobjUpdateClause *result = new SgOmpDepobjUpdateClause(sg_type);
  ROSE_ASSERT(result);

  setOneSourcePositionForTransformation(result);
  SgOmpClause *sg_clause = result;
  clause_body->get_clauses().push_back(sg_clause);
  sg_clause->set_parent(clause_body);

  return result;
}

SgOmpAtomicDefaultMemOrderClause *convertAtomicDefaultMemOrderClause(
    SgStatement *directive,
    std::pair<SgPragmaDeclaration *, OpenMPDirective *>
        current_OpenMPIR_to_SageIII,
    OpenMPClause *current_omp_clause) {
  OpenMPAtomicDefaultMemOrderClauseKind atomic_default_mem_order_kind =
      ((OpenMPAtomicDefaultMemOrderClause *)current_omp_clause)->getKind();
  SgOmpClause::omp_atomic_default_mem_order_kind_enum sg_dv =
      SgOmpClause::e_omp_atomic_default_mem_order_kind_unspecified;
  switch (atomic_default_mem_order_kind) {
  case OMPC_ATOMIC_DEFAULT_MEM_ORDER_seq_cst: {
    sg_dv = SgOmpClause::e_omp_atomic_default_mem_order_kind_seq_cst;
    break;
  }
  case OMPC_ATOMIC_DEFAULT_MEM_ORDER_acq_rel: {
    sg_dv = SgOmpClause::e_omp_atomic_default_mem_order_kind_acq_rel;
    break;
  }
  case OMPC_ATOMIC_DEFAULT_MEM_ORDER_relaxed: {
    sg_dv = SgOmpClause::e_omp_atomic_default_mem_order_kind_relaxed;
    break;
  }
  default: {
    cerr << "error: buildOmpAtomicDefaultMemOrderClause () Unacceptable "
            "default option from OpenMPIR:"
         << atomic_default_mem_order_kind;
  }
  }; // end switch
  SgOmpAtomicDefaultMemOrderClause *result =
      new SgOmpAtomicDefaultMemOrderClause(sg_dv);
  setOneSourcePositionForTransformation(result);
  ((SgOmpRequiresStatement *)directive)->get_clauses().push_back(result);
  result->set_parent(directive);
  return result;
}

SgOmpFailClause *
convertFailClause(SgStatement *directive,
                  std::pair<SgPragmaDeclaration *, OpenMPDirective *>
                      current_OpenMPIR_to_SageIII,
                  OpenMPClause *current_omp_clause) {
  (void)current_OpenMPIR_to_SageIII;
  OpenMPFailClauseMemoryOrder memory_order =
      ((OpenMPFailClause *)current_omp_clause)->getMemoryOrder();
  SgOmpClause::omp_fail_memory_order_kind_enum sg_memory_order =
      toSgOmpClauseFailMemoryOrder(memory_order);
  SgOmpFailClause *result = new SgOmpFailClause(sg_memory_order);
  ROSE_ASSERT(result != NULL);
  setOneSourcePositionForTransformation(result);
  addOmpClause(directive, result);
  result->set_parent(directive);
  return result;
}

SgOmpExtImplementationDefinedRequirementClause *
convertExtImplementationDefinedRequirementClause(
    SgStatement *directive,
    std::pair<SgPragmaDeclaration *, OpenMPDirective *>
        current_OpenMPIR_to_SageIII,
    OpenMPClause *current_omp_clause) {
  const std::string requirement_text = trimWhitespaceCopy(
      ((OpenMPExtImplementationDefinedRequirementClause *)current_omp_clause)
          ->getImplementationDefinedRequirement());
  SgExpression *ext_implementation_defined_requirement =
      buildOpaqueOpenMPClauseExpression(current_OpenMPIR_to_SageIII.first,
                                        requirement_text);
  SgOmpExtImplementationDefinedRequirementClause *result =
      new SgOmpExtImplementationDefinedRequirementClause(
          ext_implementation_defined_requirement);
  setOneSourcePositionForTransformation(result);
  ((SgOmpRequiresStatement *)directive)->get_clauses().push_back(result);
  result->set_parent(directive);
  return result;
}

SgOmpScheduleClause *
convertScheduleClause(SgStatement *directive,
                      std::pair<SgPragmaDeclaration *, OpenMPDirective *>
                          current_OpenMPIR_to_SageIII,
                      OpenMPClause *current_omp_clause) {

  OpenMPScheduleClauseModifier modifier1 =
      ((OpenMPScheduleClause *)current_omp_clause)->getModifier1();
  SgOmpClause::omp_schedule_modifier_enum sg_modifier1 =
      toSgOmpClauseScheduleModifier(modifier1);
  OpenMPScheduleClauseModifier modifier2 =
      ((OpenMPScheduleClause *)current_omp_clause)->getModifier2();
  SgOmpClause::omp_schedule_modifier_enum sg_modifier2 =
      toSgOmpClauseScheduleModifier(modifier2);
  OpenMPScheduleClauseKind kind =
      ((OpenMPScheduleClause *)current_omp_clause)->getKind();
  SgOmpClause::omp_schedule_kind_enum sg_kind = toSgOmpClauseScheduleKind(kind);

  SgExpression *chunk_size = NULL;
  if ((((OpenMPScheduleClause *)current_omp_clause)->getChunkSize()) != "") {
    chunk_size = parseOmpExpression(
        current_OpenMPIR_to_SageIII.first, current_omp_clause->getKind(),
        ((OpenMPScheduleClause *)current_omp_clause)->getChunkSize());
  }

  SgOmpScheduleClause *result =
      new SgOmpScheduleClause(sg_modifier1, sg_modifier2, sg_kind, chunk_size);
  ROSE_ASSERT(result);
  setOneSourcePositionForTransformation(result);
  addOmpClause(directive, result);
  result->set_parent(directive);
  return result;
}

SgOmpDistScheduleClause *
convertDistScheduleClause(SgOmpClauseBodyStatement *clause_body,
                          std::pair<SgPragmaDeclaration *, OpenMPDirective *>
                              current_OpenMPIR_to_SageIII,
                          OpenMPClause *current_omp_clause) {

  OpenMPDistScheduleClauseKind kind =
      ((OpenMPDistScheduleClause *)current_omp_clause)->getKind();
  SgOmpClause::omp_dist_schedule_kind_enum sg_kind =
      toSgOmpClauseDistScheduleKind(kind);

  SgExpression *chunk_size = NULL;
  if ((((OpenMPDistScheduleClause *)current_omp_clause)->getChunkSize()) !=
      "") {
    chunk_size = parseOmpExpression(
        current_OpenMPIR_to_SageIII.first, current_omp_clause->getKind(),
        ((OpenMPDistScheduleClause *)current_omp_clause)->getChunkSize());
  }

  SgOmpDistScheduleClause *result =
      new SgOmpDistScheduleClause(sg_kind, chunk_size);
  ROSE_ASSERT(result);
  setOneSourcePositionForTransformation(result);
  SgOmpClause *sg_clause = result;
  clause_body->get_clauses().push_back(sg_clause);
  sg_clause->set_parent(clause_body);
  return result;
}

SgOmpDefaultmapClause *
convertDefaultmapClause(SgOmpClauseBodyStatement *clause_body,
                        std::pair<SgPragmaDeclaration *, OpenMPDirective *>
                            current_OpenMPIR_to_SageIII,
                        OpenMPClause *current_omp_clause) {

  OpenMPDefaultmapClauseBehavior behavior =
      ((OpenMPDefaultmapClause *)current_omp_clause)->getBehavior();
  SgOmpClause::omp_defaultmap_behavior_enum sg_behavior =
      toSgOmpClauseDefaultmapBehavior(behavior);

  OpenMPDefaultmapClauseCategory category =
      ((OpenMPDefaultmapClause *)current_omp_clause)->getCategory();
  SgOmpClause::omp_defaultmap_category_enum sg_category =
      toSgOmpClauseDefaultmapCategory(category);

  SgOmpDefaultmapClause *result =
      new SgOmpDefaultmapClause(sg_behavior, sg_category);
  ROSE_ASSERT(result);
  setOneSourcePositionForTransformation(result);
  SgOmpClause *sg_clause = result;
  clause_body->get_clauses().push_back(sg_clause);
  sg_clause->set_parent(clause_body);
  return result;
}

SgOmpUsesAllocatorsClause *
convertUsesAllocatorsClause(SgOmpClauseBodyStatement *clause_body,
                            std::pair<SgPragmaDeclaration *, OpenMPDirective *>
                                current_OpenMPIR_to_SageIII,
                            OpenMPClause *current_omp_clause) {

  // budui, allocator yinggai he array duiyingqilai , yinggai you henduo
  // allocators
  SgOmpUsesAllocatorsClause *result = NULL;
  SgOmpUsesAllocatorsDefination *uses_allocators_defination = NULL;
  SgOmpClause::omp_uses_allocators_allocator_enum sg_allocator;
  SgExpression *user_defined_allocator = NULL;
  SgExpression *clause_expression = NULL;
  std::vector<usesAllocatorParameter *> *uses_allocators =
      ((OpenMPUsesAllocatorsClause *)current_omp_clause)
          ->getUsesAllocatorsAllocatorSequence();
  std::vector<usesAllocatorParameter *>::iterator iter;
  std::list<SgOmpUsesAllocatorsDefination *> uses_allocators_definations;
  for (iter = uses_allocators->begin(); iter != uses_allocators->end();
       iter++) {
    OpenMPUsesAllocatorsClauseAllocator allocator =
        ((usesAllocatorParameter *)(*iter))->getUsesAllocatorsAllocator();
    sg_allocator = toSgOmpClauseUsesAllocatorsAllocator(allocator);
    if (sg_allocator ==
        SgOmpClause::e_omp_uses_allocators_allocator_user_defined) {
      clause_expression = parseOmpExpression(
          current_OpenMPIR_to_SageIII.first, current_omp_clause->getKind(),
          ((usesAllocatorParameter *)(*iter))->getAllocatorUser());
    }

    SgExpression *allocator_traits_array = NULL;
    std::string allocator_array =
        ((usesAllocatorParameter *)(*iter))->getAllocatorTraitsArray();
    if (!allocator_array.empty()) {
      allocator_traits_array =
          parseOmpExpression(current_OpenMPIR_to_SageIII.first,
                             current_omp_clause->getKind(), allocator_array);
    }

    uses_allocators_defination = new SgOmpUsesAllocatorsDefination();
    uses_allocators_defination->set_allocator_traits_array(
        allocator_traits_array);
    uses_allocators_defination->set_allocator(sg_allocator);

    uses_allocators_defination->set_user_defined_allocator(clause_expression);
    uses_allocators_definations.push_back(uses_allocators_defination);
  }

  result = new SgOmpUsesAllocatorsClause();

  ROSE_ASSERT(result != NULL);
  result->set_uses_allocators_defination(uses_allocators_definations);
  setOneSourcePositionForTransformation(result);
  SgOmpClause *sg_clause = result;
  clause_body->get_clauses().push_back(sg_clause);
  sg_clause->set_parent(clause_body);
  return result;
}

SgOmpMapClause *
convertMapClause(SgStatement *clause_body,
                 std::pair<SgPragmaDeclaration *, OpenMPDirective *>
                     current_OpenMPIR_to_SageIII,
                 OpenMPClause *current_omp_clause) {
  SgOmpMapClause *result = NULL;
  OpenMPMapClause *map_clause =
      static_cast<OpenMPMapClause *>(current_omp_clause);
  OpenMPMapClauseType type = map_clause->getType();
  SgOmpClause::omp_map_operator_enum sg_type = toSgOmpClauseMapOperator(type);
  SgOmpClause::omp_map_modifier_enum sg_modifier1 =
      toSgOmpClauseMapModifier(map_clause->getModifier1());
  SgOmpClause::omp_map_modifier_enum sg_modifier2 =
      toSgOmpClauseMapModifier(map_clause->getModifier2());
  SgOmpClause::omp_map_modifier_enum sg_modifier3 =
      toSgOmpClauseMapModifier(map_clause->getModifier3());
  SgExpression *mapper_identifier = NULL;

  std::map<SgSymbol *,
           std::vector<
               std::pair<SgOmpClause::omp_map_dist_data_enum, SgExpression *>>>
      map_dist_data_policies;

  omp_variable_list.clear();
  array_dimensions.clear();
  const std::vector<const OmpParsedExpression *> *parsed_nodes =
      getParsedClauseExpressionNodes(current_OpenMPIR_to_SageIII.second,
                                     current_omp_clause);
  if (parsed_nodes != nullptr && !parsed_nodes->empty()) {
    for (const OmpParsedExpression *parsed : *parsed_nodes) {
      if (parsed == nullptr) {
        continue;
      }
      if (parsed->mode == OMP_EXPR_PARSE_array_section ||
          parsed->mode == OMP_EXPR_PARSE_variable_list) {
        appendParsedVariableNode(parsed);
      }
    }

    const auto &original_map_policies =
        static_cast<OpenMPMapClause *>(current_omp_clause)
            ->getDistDataPolicies();
    const auto *cached_map_policies = getParsedMapDistDataPolicies(
        current_OpenMPIR_to_SageIII.second, current_omp_clause);
    const std::vector<std::vector<OpenMPMapClause::DistDataPolicy>>
        *map_policies = &original_map_policies;
    if (map_policies->empty() && cached_map_policies != nullptr &&
        !cached_map_policies->empty()) {
      map_policies = cached_map_policies;
    }
    const auto *policy_nodes = getParsedMapDistDataPolicyNodes(
        current_OpenMPIR_to_SageIII.second, current_omp_clause);
    const size_t policy_item_count = map_policies->size();
    for (size_t item_index = 0; item_index < policy_item_count; ++item_index) {
      const auto &policies_for_item = (*map_policies)[item_index];
      if (policies_for_item.empty()) {
        continue;
      }
      if (item_index >= omp_variable_list.size()) {
        continue;
      }
      SgNode *mapped_node = omp_variable_list[item_index].second;
      SgVariableSymbol *mapped_symbol =
          extractDirectArraySectionSymbol(mapped_node);
      if (mapped_symbol == nullptr &&
          (isSgInitializedName(mapped_node) != nullptr ||
           isSgVarRefExp(mapped_node) != nullptr)) {
        mapped_symbol = extractClauseVariableSymbol(mapped_node);
      }
      if (mapped_symbol == nullptr) {
        continue;
      }

      std::vector<
          std::pair<SgOmpClause::omp_map_dist_data_enum, SgExpression *>>
          sg_policies;
      const std::vector<const OmpParsedExpression *> *parsed_policy_nodes =
          nullptr;
      if (policy_nodes != nullptr && item_index < policy_nodes->size()) {
        parsed_policy_nodes = &(*policy_nodes)[item_index];
      }

      for (size_t policy_index = 0; policy_index < policies_for_item.size();
           ++policy_index) {
        const OpenMPMapClause::DistDataPolicy &policy =
            policies_for_item[policy_index];
        SgExpression *policy_expression = nullptr;
        if (!policy.argument.empty()) {
          const OmpParsedExpression *parsed_policy = nullptr;
          if (parsed_policy_nodes != nullptr &&
              policy_index < parsed_policy_nodes->size()) {
            parsed_policy = (*parsed_policy_nodes)[policy_index];
          }
          if (parsed_policy != nullptr) {
            policy_expression = cloneParsedExpressionNode(parsed_policy);
          }
          if (policy_expression == nullptr) {
            policy_expression = parseOmpExpression(
                current_OpenMPIR_to_SageIII.first,
                current_omp_clause->getKind(), policy.argument);
          }
        }
        sg_policies.push_back(std::make_pair(toSgMapDistDataPolicy(policy.kind),
                                             policy_expression));
      }

      if (!sg_policies.empty()) {
        map_dist_data_policies[mapped_symbol] = std::move(sg_policies);
      }
    }
  } else {
    std::vector<const char *> *current_expressions =
        current_omp_clause->getExpressions();
    if (!current_expressions->empty()) {
      for (const char *expression : *current_expressions) {
        parseOmpArraySection(current_OpenMPIR_to_SageIII.first,
                             current_omp_clause->getKind(), expression);
      }
    }
  }
  SgExprListExp *explist = buildExprListExp();

  result = new SgOmpMapClause(explist, sg_type);
  result->set_modifier1(sg_modifier1);
  result->set_modifier2(sg_modifier2);
  result->set_modifier3(sg_modifier3);

  if (!map_clause->getMapperIdentifier().empty()) {
    mapper_identifier = parseMapperIdentifierExpression(
        current_OpenMPIR_to_SageIII.first, current_omp_clause->getKind(),
        parsed_nodes, map_clause->getMapperIdentifier());
  }
  result->set_mapper_identifier(mapper_identifier);

  ROSE_ASSERT(result != NULL);
  buildVariableList(result);
  explist->set_parent(result);
  result->set_array_dimensions(array_dimensions);
  result->set_dist_data_policies(map_dist_data_policies);
  result->set_iterator(buildClauseIteratorDefinitions(
      current_OpenMPIR_to_SageIII.first, current_omp_clause->getKind(),
      parsed_nodes, map_clause->getIterators()));

  setOneSourcePositionForTransformation(result);
  SgOmpClause *sg_clause = result;
  addOmpClause(clause_body, sg_clause);
  sg_clause->set_parent(clause_body);
  array_dimensions.clear();
  omp_variable_list.clear();
  return result;
}

SgStatement *
convertVariantBodyDirective(std::pair<SgPragmaDeclaration *, OpenMPDirective *>
                                current_OpenMPIR_to_SageIII) {

  OpenMPDirectiveKind directive_kind =
      current_OpenMPIR_to_SageIII.second->getKind();
  // directives like parallel and for have a following code block beside the
  // pragma itself.
  SgStatement *result = NULL;
  OpenMPClauseKind clause_kind;

  switch (directive_kind) {
  case OMPD_do: {
    result = new SgOmpDoStatement(NULL, NULL);
    break;
  }
  case OMPD_ordered: {
    result = new SgOmpOrderedStatement(NULL, NULL);
    break;
  }
  case OMPD_parallel: {
    result = new SgOmpParallelStatement(NULL, NULL);
    break;
  }
  case OMPD_dispatch: {
    result = new SgOmpDispatchStatement(NULL, NULL);
    break;
  }
  case OMPD_parallel_do: {
    SgStatement *second_stmt = new SgOmpDoStatement(NULL, NULL);
    result = new SgOmpParallelStatement(NULL, second_stmt);
    second_stmt->set_parent(result);
    result->addNewAttribute(
        kOmpCombinedParallelNestedVariantAttrName,
        new AstIntAttribute(static_cast<int>(second_stmt->variantT())));
    break;
  }
  case OMPD_parallel_for: {
    SgStatement *second_stmt = new SgOmpForStatement(NULL, NULL);
    result = new SgOmpParallelStatement(NULL, second_stmt);
    second_stmt->set_parent(result);
    result->addNewAttribute(
        kOmpCombinedParallelNestedVariantAttrName,
        new AstIntAttribute(static_cast<int>(second_stmt->variantT())));
    break;
  }
  case OMPD_parallel_for_simd: {
    SgStatement *second_stmt = new SgOmpForSimdStatement(NULL, NULL);
    result = new SgOmpParallelStatement(NULL, second_stmt);
    second_stmt->set_parent(result);
    result->addNewAttribute(
        kOmpCombinedParallelNestedVariantAttrName,
        new AstIntAttribute(static_cast<int>(second_stmt->variantT())));
    break;
  }
  case OMPD_parallel_sections: {
    SgStatement *second_stmt = new SgOmpSectionsStatement(NULL, NULL);
    result = new SgOmpParallelStatement(NULL, second_stmt);
    second_stmt->set_parent(result);
    result->addNewAttribute(
        kOmpCombinedParallelNestedVariantAttrName,
        new AstIntAttribute(static_cast<int>(second_stmt->variantT())));
    break;
  }
  case OMPD_parallel_workshare: {
    SgStatement *second_stmt = new SgOmpWorkshareStatement(NULL, NULL);
    result = new SgOmpParallelStatement(NULL, second_stmt);
    second_stmt->set_parent(result);
    result->addNewAttribute(
        kOmpCombinedParallelNestedVariantAttrName,
        new AstIntAttribute(static_cast<int>(second_stmt->variantT())));
    break;
  }
  case OMPD_simd: {
    result = new SgOmpSimdStatement(NULL, NULL);
    break;
  }
  case OMPD_teams: {
    result = new SgOmpTeamsStatement(NULL, NULL);
    break;
  }
  case OMPD_atomic: {
    result = new SgOmpAtomicStatement(NULL, NULL);
    break;
  }
  case OMPD_taskgroup: {
    result = new SgOmpTaskgroupStatement(NULL, NULL);
    break;
  }
  case OMPD_master: {
    result = new SgOmpMasterStatement(NULL, NULL);
    break;
  }
  case OMPD_masked: {
    result = new SgOmpMasterStatement(NULL, NULL);
    setDirectiveSpellingOverride(result, "masked");
    break;
  }
  case OMPD_nothing: {
    result = new SgOmpMasterStatement(NULL, NULL);
    setDirectiveSpellingOverride(result, "nothing");
    break;
  }
  case OMPD_distribute: {
    result = new SgOmpDistributeStatement(NULL, NULL);
    break;
  }
  case OMPD_loop: {
    result = new SgOmpLoopStatement(NULL, NULL);
    break;
  }
  case OMPD_scan: {
    result = new SgOmpScanStatement(NULL, NULL);
    break;
  }
  case OMPD_taskloop: {
    result = new SgOmpTaskloopStatement(NULL, NULL);
    break;
  }
  case OMPD_target_enter_data: {
    result = new SgOmpTargetEnterDataStatement(NULL, NULL);
    break;
  }
  case OMPD_target_exit_data: {
    result = new SgOmpTargetExitDataStatement(NULL, NULL);
    break;
  }
  case OMPD_task: {
    result = new SgOmpTaskStatement(NULL, NULL);
    break;
  }
  case OMPD_target_data: {
    result = new SgOmpTargetDataStatement(NULL, NULL);
    break;
  }
  case OMPD_single: {
    result = new SgOmpSingleStatement(NULL, NULL);
    break;
  }
  case OMPD_for: {
    result = new SgOmpForStatement(NULL, NULL);
    break;
  }
  case OMPD_target: {
    result = new SgOmpTargetStatement(NULL, NULL);
    break;
  }
  case OMPD_critical: {
    std::string name =
        ((OpenMPCriticalDirective *)(current_OpenMPIR_to_SageIII.second))
            ->getCriticalName();
    result = new SgOmpCriticalStatement(NULL, NULL, SgName(name));
    break;
  }
  case OMPD_depobj: {
    std::string name =
        ((OpenMPDepobjDirective *)(current_OpenMPIR_to_SageIII.second))
            ->getDepobj();
    result = new SgOmpDepobjStatement(NULL, NULL, SgName(name));
    break;
  }
  case OMPD_metadirective: {
    result = new SgOmpMetadirectiveStatement(NULL, NULL);
    break;
  }
  case OMPD_target_parallel_for: {
    result = new SgOmpTargetParallelForStatement(NULL, NULL);
    break;
  }
  case OMPD_target_parallel: {
    result = new SgOmpTargetParallelStatement(NULL, NULL);
    break;
  }
  case OMPD_distribute_simd: {
    result = new SgOmpDistributeSimdStatement(NULL, NULL);
    break;
  }
  case OMPD_distribute_parallel_for: {
    result = new SgOmpDistributeParallelForStatement(NULL, NULL);
    break;
  }
  case OMPD_distribute_parallel_for_simd: {
    result = new SgOmpDistributeParallelForSimdStatement(NULL, NULL);
    break;
  }
  case OMPD_taskloop_simd: {
    result = new SgOmpTaskloopSimdStatement(NULL, NULL);
    break;
  }
  case OMPD_target_parallel_for_simd: {
    result = new SgOmpTargetParallelForSimdStatement(NULL, NULL);
    break;
  }
  case OMPD_target_parallel_loop: {
    result = new SgOmpTargetParallelLoopStatement(NULL, NULL);
    break;
  }
  case OMPD_target_simd: {
    result = new SgOmpTargetSimdStatement(NULL, NULL);
    break;
  }
  case OMPD_target_teams: {
    result = new SgOmpTargetTeamsStatement(NULL, NULL);
    break;
  }
  case OMPD_target_teams_distribute: {
    result = new SgOmpTargetTeamsDistributeStatement(NULL, NULL);
    break;
  }
  case OMPD_target_teams_distribute_simd: {
    result = new SgOmpTargetTeamsDistributeSimdStatement(NULL, NULL);
    break;
  }
  case OMPD_target_teams_loop: {
    result = new SgOmpTargetTeamsLoopStatement(NULL, NULL);
    break;
  }
  case OMPD_target_teams_distribute_parallel_for: {
    result = new SgOmpTargetTeamsDistributeParallelForStatement(NULL, NULL);
    break;
  }
  case OMPD_target_teams_distribute_parallel_for_simd: {
    result = new SgOmpTargetTeamsDistributeParallelForSimdStatement(NULL, NULL);
    break;
  }
  case OMPD_master_taskloop_simd: {
    result = new SgOmpMasterTaskloopSimdStatement(NULL, NULL);
    break;
  }
  case OMPD_masked_taskloop_simd: {
    result = new SgOmpMasterTaskloopSimdStatement(NULL, NULL);
    setDirectiveSpellingOverride(result, "masked taskloop simd");
    break;
  }
  case OMPD_parallel_master_taskloop: {
    result = new SgOmpParallelMasterTaskloopStatement(NULL, NULL);
    break;
  }
  case OMPD_parallel_master_taskloop_simd: {
    result = new SgOmpParallelMasterTaskloopSimdStatement(NULL, NULL);
    break;
  }
  case OMPD_teams_distribute: {
    result = new SgOmpTeamsDistributeStatement(NULL, NULL);
    break;
  }
  case OMPD_teams_distribute_simd: {
    result = new SgOmpTeamsDistributeSimdStatement(NULL, NULL);
    break;
  }
  case OMPD_teams_distribute_parallel_for: {
    result = new SgOmpTeamsDistributeParallelForStatement(NULL, NULL);
    break;
  }
  case OMPD_teams_distribute_parallel_for_simd: {
    result = new SgOmpTeamsDistributeParallelForSimdStatement(NULL, NULL);
    break;
  }
  case OMPD_teams_loop: {
    result = new SgOmpTeamsLoopStatement(NULL, NULL);
    break;
  }
  case OMPD_parallel_master: {
    result = new SgOmpParallelMasterStatement(NULL, NULL);
    break;
  }
  case OMPD_master_taskloop: {
    result = new SgOmpMasterTaskloopStatement(NULL, NULL);
    break;
  }
  case OMPD_masked_taskloop: {
    result = new SgOmpMasterTaskloopStatement(NULL, NULL);
    setDirectiveSpellingOverride(result, "masked taskloop");
    break;
  }
  case OMPD_parallel_loop: {
    result = new SgOmpParallelLoopStatement(NULL, NULL);
    break;
  }
  case OMPD_end: {
    return result;
  }
  case OMPD_workshare: {
    result = new SgOmpWorkshareStatement(NULL, NULL);
    break;
  }
  default: {
    printf("Unknown directive is found.\n");
  }
  }
  // body->set_parent(result);
  //  extract all the clauses based on the vector of clauses in the original
  //  order
  std::vector<OpenMPClause *> *all_clauses =
      current_OpenMPIR_to_SageIII.second->getClausesInOriginalOrder();
  std::vector<OpenMPClause *>::iterator clause_iter;
  for (clause_iter = all_clauses->begin(); clause_iter != all_clauses->end();
       clause_iter++) {
    clause_kind = (*clause_iter)->getKind();
    switch (clause_kind) {
    case OMPC_if:
    case OMPC_num_teams:
    case OMPC_grainsize:
    case OMPC_detach:
    case OMPC_num_tasks:
    case OMPC_safelen:
    case OMPC_hint:
    case OMPC_simdlen:
    case OMPC_ordered:
    case OMPC_collapse:
    case OMPC_final:
    case OMPC_priority:
    case OMPC_thread_limit:
    case OMPC_device:
    case OMPC_nocontext:
    case OMPC_novariants:
    case OMPC_num_threads: {
      convertExpressionClause(result, current_OpenMPIR_to_SageIII,
                              *clause_iter);
      break;
    }
    case OMPC_default: {
      convertDefaultClause(isSgOmpClauseBodyStatement(result),
                           current_OpenMPIR_to_SageIII, *clause_iter);
      break;
    }
    case OMPC_proc_bind: {
      convertProcBindClause(isSgOmpClauseBodyStatement(result),
                            current_OpenMPIR_to_SageIII, *clause_iter);
      break;
    }
    case OMPC_order: {
      convertOrderClause(result, current_OpenMPIR_to_SageIII, *clause_iter);
      break;
    }
    case OMPC_bind: {
      convertBindClause(isSgOmpClauseBodyStatement(result),
                        current_OpenMPIR_to_SageIII, *clause_iter);
      break;
    }
    case OMPC_when: {
      convertWhenClause(isSgOmpClauseBodyStatement(result),
                        current_OpenMPIR_to_SageIII, *clause_iter);
      break;
    }
    case OMPC_read:
    case OMPC_write:
    case OMPC_threads:
    case OMPC_simd:
    case OMPC_compare:
    case OMPC_weak:
    case OMPC_update:
    case OMPC_capture:
    case OMPC_seq_cst:
    case OMPC_acq_rel:
    case OMPC_release:
    case OMPC_acquire:
    case OMPC_relaxed:
    case OMPC_mergeable:
    case OMPC_untied:
    case OMPC_nogroup:
    case OMPC_destroy:
    case OMPC_nowait:
    case OMPC_full: {
      convertSimpleClause(result, current_OpenMPIR_to_SageIII, *clause_iter);
      break;
    }
    case OMPC_fail: {
      convertFailClause(result, current_OpenMPIR_to_SageIII, *clause_iter);
      break;
    }
    case OMPC_schedule: {
      convertScheduleClause(result, current_OpenMPIR_to_SageIII, *clause_iter);
      break;
    }
    case OMPC_dist_schedule: {
      convertDistScheduleClause(isSgOmpClauseBodyStatement(result),
                                current_OpenMPIR_to_SageIII, *clause_iter);
      break;
    }
    case OMPC_defaultmap: {
      convertDefaultmapClause(isSgOmpClauseBodyStatement(result),
                              current_OpenMPIR_to_SageIII, *clause_iter);
      break;
    }
    case OMPC_map: {
      convertMapClause(isSgOmpClauseBodyStatement(result),
                       current_OpenMPIR_to_SageIII, *clause_iter);
      break;
    }
    case OMPC_to: {
      convertToClause(isSgStatement(result), current_OpenMPIR_to_SageIII,
                      *clause_iter);
      break;
    }
    case OMPC_from: {
      convertFromClause(isSgStatement(result), current_OpenMPIR_to_SageIII,
                        *clause_iter);
      break;
    }
    case OMPC_depend: {
      convertDependClause(isSgOmpClauseBodyStatement(result),
                          current_OpenMPIR_to_SageIII, *clause_iter);
      break;
    }
    case OMPC_affinity: {
      convertAffinityClause(isSgOmpClauseBodyStatement(result),
                            current_OpenMPIR_to_SageIII, *clause_iter);
      break;
    }
    case OMPC_depobj_update: {
      convertDepobjUpdateClause(isSgOmpClauseBodyStatement(result),
                                current_OpenMPIR_to_SageIII, *clause_iter);
      break;
    }
    case OMPC_uses_allocators: {
      convertUsesAllocatorsClause(isSgOmpClauseBodyStatement(result),
                                  current_OpenMPIR_to_SageIII, *clause_iter);
      break;
    }
    default: {
      convertClause(result, current_OpenMPIR_to_SageIII, *clause_iter);
    }
    };
  };

  initializeGeneratedOpenMPStatement(result);
  return result;
}

SgStatement *
getOpenMPBlockBody(std::pair<SgPragmaDeclaration *, OpenMPDirective *>
                       current_OpenMPIR_to_SageIII) {

  SgStatement *result = NULL;
  bool current_is_fortran = false;
  if (SgSourceFile *source_file =
          getEnclosingSourceFile(current_OpenMPIR_to_SageIII.first)) {
    current_is_fortran =
        source_file->get_Fortran_only() || source_file->get_F77_only() ||
        source_file->get_F90_only() || source_file->get_F95_only() ||
        source_file->get_F2003_only();
  }
  result = getNextStatement(current_OpenMPIR_to_SageIII.first);
  while (SgPragmaDeclaration *next_pragma = isSgPragmaDeclaration(result)) {
    auto mapped = fortran_paired_pragma_dict.find(next_pragma);
    if (mapped != fortran_paired_pragma_dict.end() && mapped->second != NULL &&
        mapped->second->getKind() == OMPD_end) {
      return NULL;
    }
    // For C/C++, a pragma between a body directive and its statement means we
    // cannot safely identify the structured block; keep the original pragma.
    if (!current_is_fortran) {
      return NULL;
    }
    // Skip stray/misplaced Fortran OpenMP pragmas so the construct body is
    // the executable statement/loop that follows.
    if (isFortranOpenMPPragmaDeclaration(next_pragma)) {
      result = getNextStatement(next_pragma);
      continue;
    }
    break;
  }
  return result;
}

//! Build SgOmpDefaultClause from OpenMPIR
SgOmpDefaultClause *
convertDefaultClause(SgOmpClauseBodyStatement *clause_body,
                     std::pair<SgPragmaDeclaration *, OpenMPDirective *>
                         current_OpenMPIR_to_SageIII,
                     OpenMPClause *current_omp_clause) {
  OpenMPDefaultClauseKind default_kind =
      ((OpenMPDefaultClause *)current_omp_clause)->getDefaultClauseKind();
  SgOmpClause::omp_default_option_enum sg_dv;
  SgStatement *variant_directive = NULL;
  switch (default_kind) {
  case OMPC_DEFAULT_none: {
    sg_dv = SgOmpClause::e_omp_default_none;
    break;
  }
  case OMPC_DEFAULT_shared: {
    sg_dv = SgOmpClause::e_omp_default_shared;
    break;
  }
  case OMPC_DEFAULT_private: {
    sg_dv = SgOmpClause::e_omp_default_private;
    break;
  }
  case OMPC_DEFAULT_firstprivate: {
    sg_dv = SgOmpClause::e_omp_default_firstprivate;
    break;
  }
  case OMPC_DEFAULT_variant: {
    sg_dv = SgOmpClause::e_omp_default_variant;
    OpenMPDirective *variant_OpenMPIR =
        ((OpenMPDefaultClause *)current_omp_clause)->getVariantDirective();
    std::pair<SgPragmaDeclaration *, OpenMPDirective *>
        paired_variant_OpenMPIR =
            make_pair(current_OpenMPIR_to_SageIII.first, variant_OpenMPIR);
    variant_directive = convertVariantDirective(paired_variant_OpenMPIR);
    break;
  }
  default: {
    cerr << "error: buildOmpDefaultClase() Unacceptable default option from "
            "OpenMPIR:"
         << default_kind;
    ROSE_ABORT();
  }
  }; // end switch
  SgOmpDefaultClause *result = new SgOmpDefaultClause(sg_dv, variant_directive);
  setOneSourcePositionForTransformation(result);

  if (variant_directive != NULL) {
    variant_directive->set_parent(result);
  };

  // reconsider the location of following code to attach clause
  SgOmpClause *sg_clause = result;
  clause_body->get_clauses().push_back(sg_clause);
  sg_clause->set_parent(clause_body);

  return result;
}

//! Build SgOmpAllocatorClause from OpenMPIR
SgOmpAllocatorClause *
convertAllocatorClause(SgOmpClauseStatement *clause_body,
                       std::pair<SgPragmaDeclaration *, OpenMPDirective *>
                           current_OpenMPIR_to_SageIII,
                       OpenMPClause *current_omp_clause) {
  OpenMPAllocatorClauseAllocator allocator =
      ((OpenMPAllocatorClause *)current_omp_clause)->getAllocator();
  SgOmpClause::omp_allocator_modifier_enum sg_modifier =
      toSgOmpClauseAllocatorAllocator(allocator);
  SgExpression *user_defined_parameter = NULL;
  SgGlobal *global =
      SageInterface::getGlobalScope(current_OpenMPIR_to_SageIII.first);
  if (sg_modifier == SgOmpClause::e_omp_allocator_user_defined_modifier) {
    SgExpression *clause_expression = parseOmpExpression(
        current_OpenMPIR_to_SageIII.first, current_omp_clause->getKind(),
        ((OpenMPAllocatorClause *)current_omp_clause)
            ->getUserDefinedAllocator());
    user_defined_parameter =
        checkOmpExpressionClause(clause_expression, global, e_allocate);
  }
  SgOmpAllocatorClause *result =
      new SgOmpAllocatorClause(sg_modifier, user_defined_parameter);
  setOneSourcePositionForTransformation(result);
  // reconsider the location of following code to attach clause
  SgOmpClause *sg_clause = result;
  clause_body->get_clauses().push_back(sg_clause);
  sg_clause->set_parent(clause_body);

  return result;
}

//! Build SgOmpProcBindClause from OpenMPIR
SgOmpProcBindClause *
convertProcBindClause(SgOmpClauseBodyStatement *clause_body,
                      std::pair<SgPragmaDeclaration *, OpenMPDirective *>
                          current_OpenMPIR_to_SageIII,
                      OpenMPClause *current_omp_clause) {
  OpenMPProcBindClauseKind proc_bind_kind =
      ((OpenMPProcBindClause *)current_omp_clause)->getProcBindClauseKind();
  SgOmpClause::omp_proc_bind_policy_enum sg_dv;
  switch (proc_bind_kind) {
  case OMPC_PROC_BIND_close: {
    sg_dv = SgOmpClause::e_omp_proc_bind_policy_close;
    break;
  }
  case OMPC_PROC_BIND_master: {
    sg_dv = SgOmpClause::e_omp_proc_bind_policy_master;
    break;
  }
  case OMPC_PROC_BIND_spread: {
    sg_dv = SgOmpClause::e_omp_proc_bind_policy_spread;
    break;
  }
  default: {
    cerr << "error: buildOmpProcBindClause () Unacceptable default option from "
            "OpenMPIR:"
         << proc_bind_kind;
    ROSE_ABORT();
  }
  }; // end switch
  SgOmpProcBindClause *result = new SgOmpProcBindClause(sg_dv);
  setOneSourcePositionForTransformation(result);

  // reconsider the location of following code to attach clause
  clause_body->get_clauses().push_back(result);
  result->set_parent(clause_body);

  return result;
}

static SgOmpClause::omp_order_modifier_enum
toSgOmpClauseOrderModifier(OpenMPOrderClauseModifier modifier) {
  switch (modifier) {
  case OMPC_ORDER_MODIFIER_reproducible:
    return SgOmpClause::e_omp_order_modifier_reproducible;
  case OMPC_ORDER_MODIFIER_unconstrained:
    return SgOmpClause::e_omp_order_modifier_unconstrained;
  case OMPC_ORDER_MODIFIER_unspecified:
    return SgOmpClause::e_omp_order_modifier_unspecified;
  default:
    cerr << "error: toSgOmpClauseOrderModifier() unsupported modifier: "
         << modifier;
    ROSE_ABORT();
  }
}

static SgOmpClause::omp_grainsize_modifier_enum
toSgOmpClauseGrainsizeModifier(OpenMPGrainsizeClauseModifier modifier) {
  switch (modifier) {
  case OMPC_GRAINSIZE_MODIFIER_strict:
    return SgOmpClause::e_omp_grainsize_modifier_strict;
  case OMPC_GRAINSIZE_MODIFIER_unspecified:
    return SgOmpClause::e_omp_grainsize_modifier_unspecified;
  default:
    cerr << "error: toSgOmpClauseGrainsizeModifier() unsupported modifier: "
         << modifier;
    ROSE_ABORT();
  }
}

static SgOmpClause::omp_num_tasks_modifier_enum
toSgOmpClauseNumTasksModifier(OpenMPNumTasksClauseModifier modifier) {
  switch (modifier) {
  case OMPC_NUM_TASKS_MODIFIER_strict:
    return SgOmpClause::e_omp_num_tasks_modifier_strict;
  case OMPC_NUM_TASKS_MODIFIER_unspecified:
    return SgOmpClause::e_omp_num_tasks_modifier_unspecified;
  default:
    cerr << "error: toSgOmpClauseNumTasksModifier() unsupported modifier: "
         << modifier;
    ROSE_ABORT();
  }
}

static SgOmpClause::omp_fail_memory_order_kind_enum
toSgOmpClauseFailMemoryOrder(OpenMPFailClauseMemoryOrder memory_order) {
  switch (memory_order) {
  case OMPC_FAIL_seq_cst:
    return SgOmpClause::e_omp_fail_memory_order_kind_seq_cst;
  case OMPC_FAIL_acquire:
    return SgOmpClause::e_omp_fail_memory_order_kind_acquire;
  case OMPC_FAIL_relaxed:
    return SgOmpClause::e_omp_fail_memory_order_kind_relaxed;
  case OMPC_FAIL_unknown:
    return SgOmpClause::e_omp_fail_memory_order_kind_unspecified;
  default:
    cerr << "error: toSgOmpClauseFailMemoryOrder() unsupported memory order: "
         << memory_order;
    ROSE_ABORT();
  }
}

SgOmpOrderClause *
convertOrderClause(SgStatement *directive,
                   std::pair<SgPragmaDeclaration *, OpenMPDirective *>
                       current_OpenMPIR_to_SageIII,
                   OpenMPClause *current_omp_clause) {
  OpenMPOrderClauseModifier order_modifier =
      ((OpenMPOrderClause *)current_omp_clause)->getOrderClauseModifier();
  OpenMPOrderClauseKind order_kind =
      ((OpenMPOrderClause *)current_omp_clause)->getOrderClauseKind();
  SgOmpClause::omp_order_modifier_enum sg_modifier =
      toSgOmpClauseOrderModifier(order_modifier);
  SgOmpClause::omp_order_kind_enum sg_dv =
      SgOmpClause::e_omp_order_kind_unspecified;
  switch (order_kind) {
  case OMPC_ORDER_concurrent: {
    sg_dv = SgOmpClause::e_omp_order_kind_concurrent;
    break;
  }
  default: {
    cerr << "error: buildOmpOrderClause () Unacceptable default option from "
            "OpenMPIR:"
         << order_kind;
  }
  }; // end switch
  SgOmpOrderClause *result = new SgOmpOrderClause(sg_dv, sg_modifier);
  setOneSourcePositionForTransformation(result);

  // reconsider the location of following code to attach clause
  if (current_OpenMPIR_to_SageIII.second->getKind() == OMPD_declare_simd) {
    ((SgOmpDeclareSimdStatement *)directive)->get_clauses().push_back(result);
  } else {
    addOmpClause(directive, result);
  }
  result->set_parent(directive);

  return result;
}

SgOmpBindClause *
convertBindClause(SgOmpClauseBodyStatement *clause_body,
                  std::pair<SgPragmaDeclaration *, OpenMPDirective *>
                      current_OpenMPIR_to_SageIII,
                  OpenMPClause *current_omp_clause) {
  OpenMPBindClauseBinding bind_binding =
      ((OpenMPBindClause *)current_omp_clause)->getBindClauseBinding();
  SgOmpClause::omp_bind_binding_enum sg_dv =
      SgOmpClause::e_omp_bind_binding_unspecified;
  switch (bind_binding) {
  case OMPC_BIND_teams: {
    sg_dv = SgOmpClause::e_omp_bind_binding_teams;
    break;
  }
  case OMPC_BIND_parallel: {
    sg_dv = SgOmpClause::e_omp_bind_binding_parallel;
    break;
  }
  case OMPC_BIND_thread: {
    sg_dv = SgOmpClause::e_omp_bind_binding_thread;
    break;
  }
  default: {
    cerr << "error: buildOmpBindClause () Unacceptable default option from "
            "OpenMPIR:"
         << bind_binding;
  }
  }; // end switch
  SgOmpBindClause *result = new SgOmpBindClause(sg_dv);
  setOneSourcePositionForTransformation(result);

  // reconsider the location of following code to attach clause
  SgOmpClause *sg_clause = result;
  clause_body->get_clauses().push_back(sg_clause);
  sg_clause->set_parent(clause_body);

  return result;
}

namespace {
struct ConvertedVariantClauseData {
  SgExpression *user_condition = nullptr;
  SgExpression *user_condition_score = nullptr;
  SgStatementPtrList construct_directives;
  bool target_device_selector = false;
  SgExpression *device_arch = nullptr;
  SgExpression *device_isa = nullptr;
  SgExpression *device_num = nullptr;
  SgOmpClause::omp_when_context_kind_enum device_kind =
      SgOmpClause::e_omp_when_context_kind_unknown;
  SgOmpClause::omp_when_context_vendor_enum implementation_vendor =
      SgOmpClause::e_omp_when_context_vendor_unspecified;
  SgExpression *implementation_user_defined = nullptr;
  SgExpression *implementation_extension = nullptr;
};

template <typename VariantClauseT>
void applyVariantClauseCommonData(VariantClauseT *result,
                                  const ConvertedVariantClauseData &data) {
  ROSE_ASSERT(result != nullptr);

  result->set_user_condition(data.user_condition);
  result->set_user_condition_score(data.user_condition_score);
  result->get_construct_directives() = data.construct_directives;
  result->set_target_device_selector(data.target_device_selector);
  result->set_device_arch(data.device_arch);
  result->set_device_isa(data.device_isa);
  result->set_device_num(data.device_num);
  result->set_device_kind(data.device_kind);
  result->set_implementation_vendor(data.implementation_vendor);
  result->set_implementation_user_defined(data.implementation_user_defined);
  result->set_implementation_extension(data.implementation_extension);

  if (data.user_condition != nullptr) {
    data.user_condition->set_parent(result);
  }
  if (data.user_condition_score != nullptr) {
    data.user_condition_score->set_parent(result);
  }
  if (data.device_arch != nullptr) {
    data.device_arch->set_parent(result);
  }
  if (data.device_isa != nullptr) {
    data.device_isa->set_parent(result);
  }
  if (data.device_num != nullptr) {
    data.device_num->set_parent(result);
  }
  if (data.implementation_user_defined != nullptr) {
    data.implementation_user_defined->set_parent(result);
  }
  if (data.implementation_extension != nullptr) {
    data.implementation_extension->set_parent(result);
  }
  for (SgStatement *construct_directive : data.construct_directives) {
    if (construct_directive != nullptr) {
      construct_directive->set_parent(result);
    }
  }
}

ConvertedVariantClauseData
buildVariantClauseCommonData(std::pair<SgPragmaDeclaration *, OpenMPDirective *>
                                 current_OpenMPIR_to_SageIII,
                             OpenMPClause *current_omp_clause) {
  ConvertedVariantClauseData data;
  auto *variant_clause =
      dynamic_cast<OpenMPVariantClause *>(current_omp_clause);
  ROSE_ASSERT(variant_clause != nullptr);

  const std::vector<const OmpParsedExpression *> *parsed_nodes =
      getParsedClauseExpressionNodes(current_OpenMPIR_to_SageIII.second,
                                     current_omp_clause);

  const std::string user_condition_string =
      variant_clause->getUserCondition()->expression;
  if (!user_condition_string.empty()) {
    data.user_condition = parseClauseExpressionWithCache(
        current_OpenMPIR_to_SageIII.first, current_omp_clause->getKind(),
        parsed_nodes, user_condition_string);
  }

  const std::string user_condition_score_string =
      variant_clause->getUserCondition()->score;
  if (!user_condition_score_string.empty()) {
    data.user_condition_score = parseClauseExpressionWithCache(
        current_OpenMPIR_to_SageIII.first, current_omp_clause->getKind(),
        parsed_nodes, user_condition_score_string);
  }

  const std::string device_arch_string =
      variant_clause->getArchExpression()->expression;
  if (!device_arch_string.empty()) {
    data.device_arch = parseClauseExpressionWithCache(
        current_OpenMPIR_to_SageIII.first, current_omp_clause->getKind(),
        parsed_nodes, device_arch_string, true);
  }

  const std::string device_isa_string =
      variant_clause->getIsaExpression()->expression;
  if (!device_isa_string.empty()) {
    data.device_isa = parseClauseExpressionWithCache(
        current_OpenMPIR_to_SageIII.first, current_omp_clause->getKind(),
        parsed_nodes, device_isa_string, true);
  }

  const std::string device_num_string =
      variant_clause->getDeviceNumExpression()->expression;
  if (!device_num_string.empty()) {
    data.device_num = parseClauseExpressionWithCache(
        current_OpenMPIR_to_SageIII.first, current_omp_clause->getKind(),
        parsed_nodes, device_num_string);
  }

  switch (variant_clause->getContextKind()->second) {
  case OMPC_CONTEXT_KIND_host:
    data.device_kind = SgOmpClause::e_omp_when_context_kind_host;
    break;
  case OMPC_CONTEXT_KIND_nohost:
    data.device_kind = SgOmpClause::e_omp_when_context_kind_nohost;
    break;
  case OMPC_CONTEXT_KIND_any:
    data.device_kind = SgOmpClause::e_omp_when_context_kind_any;
    break;
  case OMPC_CONTEXT_KIND_cpu:
    data.device_kind = SgOmpClause::e_omp_when_context_kind_cpu;
    break;
  case OMPC_CONTEXT_KIND_gpu:
    data.device_kind = SgOmpClause::e_omp_when_context_kind_gpu;
    break;
  case OMPC_CONTEXT_KIND_fpga:
    data.device_kind = SgOmpClause::e_omp_when_context_kind_fpga;
    break;
  default:
    break;
  }

  switch (variant_clause->getImplementationKind()->second) {
  case OMPC_CONTEXT_VENDOR_amd:
    data.implementation_vendor = SgOmpClause::e_omp_when_context_vendor_amd;
    break;
  case OMPC_CONTEXT_VENDOR_arm:
    data.implementation_vendor = SgOmpClause::e_omp_when_context_vendor_arm;
    break;
  case OMPC_CONTEXT_VENDOR_bsc:
    data.implementation_vendor = SgOmpClause::e_omp_when_context_vendor_bsc;
    break;
  case OMPC_CONTEXT_VENDOR_cray:
    data.implementation_vendor = SgOmpClause::e_omp_when_context_vendor_cray;
    break;
  case OMPC_CONTEXT_VENDOR_fujitsu:
    data.implementation_vendor = SgOmpClause::e_omp_when_context_vendor_fujitsu;
    break;
  case OMPC_CONTEXT_VENDOR_gnu:
    data.implementation_vendor = SgOmpClause::e_omp_when_context_vendor_gnu;
    break;
  case OMPC_CONTEXT_VENDOR_ibm:
    data.implementation_vendor = SgOmpClause::e_omp_when_context_vendor_ibm;
    break;
  case OMPC_CONTEXT_VENDOR_intel:
    data.implementation_vendor = SgOmpClause::e_omp_when_context_vendor_intel;
    break;
  case OMPC_CONTEXT_VENDOR_llvm:
    data.implementation_vendor = SgOmpClause::e_omp_when_context_vendor_llvm;
    break;
  case OMPC_CONTEXT_VENDOR_nvidia:
    data.implementation_vendor = SgOmpClause::e_omp_when_context_vendor_nvidia;
    break;
  case OMPC_CONTEXT_VENDOR_pgi:
    data.implementation_vendor = SgOmpClause::e_omp_when_context_vendor_pgi;
    break;
  case OMPC_CONTEXT_VENDOR_ti:
    data.implementation_vendor = SgOmpClause::e_omp_when_context_vendor_ti;
    break;
  case OMPC_CONTEXT_VENDOR_unknown:
    data.implementation_vendor = SgOmpClause::e_omp_when_context_vendor_unknown;
    break;
  default:
    break;
  }

  const std::string implementation_user_defined_string =
      variant_clause->getImplementationExpression()->expression;
  if (!implementation_user_defined_string.empty()) {
    data.implementation_user_defined = parseClauseExpressionWithCache(
        current_OpenMPIR_to_SageIII.first, current_omp_clause->getKind(),
        parsed_nodes, implementation_user_defined_string);
  }

  const std::string implementation_extension_string =
      variant_clause->getExtensionExpression()->expression;
  if (!implementation_extension_string.empty()) {
    data.implementation_extension = parseClauseExpressionWithCache(
        current_OpenMPIR_to_SageIII.first, current_omp_clause->getKind(),
        parsed_nodes, implementation_extension_string);
  }

  data.target_device_selector = variant_clause->getIsTargetDeviceSelector();

  std::vector<std::pair<std::string, OpenMPDirective *>> *construct_directive =
      variant_clause->getConstructDirective();
  if (construct_directive != nullptr) {
    for (const auto &construct_entry : *construct_directive) {
      std::pair<SgPragmaDeclaration *, OpenMPDirective *> paired_construct =
          make_pair(current_OpenMPIR_to_SageIII.first, construct_entry.second);
      SgStatement *converted = convertVariantDirective(paired_construct);
      if (converted != nullptr) {
        data.construct_directives.push_back(converted);
      }
    }
  }

  return data;
}
} // namespace

SgOmpWhenClause *
convertWhenClause(SgOmpClauseBodyStatement *clause_body,
                  std::pair<SgPragmaDeclaration *, OpenMPDirective *>
                      current_OpenMPIR_to_SageIII,
                  OpenMPClause *current_omp_clause) {
  auto *when_clause = static_cast<OpenMPWhenClause *>(current_omp_clause);
  SgStatement *variant_directive = nullptr;
  if (OpenMPDirective *variant_OpenMPIR = when_clause->getVariantDirective()) {
    std::pair<SgPragmaDeclaration *, OpenMPDirective *>
        paired_variant_OpenMPIR =
            make_pair(current_OpenMPIR_to_SageIII.first, variant_OpenMPIR);
    variant_directive = convertVariantDirective(paired_variant_OpenMPIR);
  }

  ConvertedVariantClauseData data = buildVariantClauseCommonData(
      current_OpenMPIR_to_SageIII, current_omp_clause);
  SgOmpWhenClause *result = new SgOmpWhenClause(
      (SgExpression *)NULL, (SgExpression *)NULL, (SgExpression *)NULL,
      (SgExpression *)NULL, SgOmpClause::e_omp_when_context_kind_unknown,
      SgOmpClause::e_omp_when_context_vendor_unspecified, (SgExpression *)NULL,
      (SgExpression *)NULL, (SgStatement *)NULL);
  applyVariantClauseCommonData(result, data);
  result->set_variant_directive(variant_directive);

  setOneSourcePositionForTransformation(result);
  if (variant_directive != nullptr) {
    variant_directive->set_parent(result);
  }

  SgOmpClause *sg_clause = result;
  addOmpClause(clause_body, sg_clause);
  sg_clause->set_parent(clause_body);

  return result;
}

SgOmpMatchClause *
convertMatchClause(SgStatement *directive,
                   std::pair<SgPragmaDeclaration *, OpenMPDirective *>
                       current_OpenMPIR_to_SageIII,
                   OpenMPClause *current_omp_clause) {
  ConvertedVariantClauseData data = buildVariantClauseCommonData(
      current_OpenMPIR_to_SageIII, current_omp_clause);
  SgOmpMatchClause *result = new SgOmpMatchClause(
      (SgExpression *)NULL, (SgExpression *)NULL, (SgExpression *)NULL,
      (SgExpression *)NULL, SgOmpClause::e_omp_when_context_kind_unknown,
      SgOmpClause::e_omp_when_context_vendor_unspecified, (SgExpression *)NULL,
      (SgExpression *)NULL);
  applyVariantClauseCommonData(result, data);

  setOneSourcePositionForTransformation(result);
  addOmpClause(directive, result);
  result->set_parent(directive);
  return result;
}

SgOmpAdjustArgsClause *
convertAdjustArgsClause(SgStatement *directive,
                        std::pair<SgPragmaDeclaration *, OpenMPDirective *>
                            current_OpenMPIR_to_SageIII,
                        OpenMPClause *current_omp_clause) {
  auto *adjust_clause =
      static_cast<OpenMPAdjustArgsClause *>(current_omp_clause);
  const std::vector<const OmpParsedExpression *> *parsed_nodes =
      getParsedClauseExpressionNodes(current_OpenMPIR_to_SageIII.second,
                                     current_omp_clause);

  SgExprListExp *arguments = buildExprListExp();
  for (const std::string &argument : adjust_clause->getArguments()) {
    SgExpression *expression = parseClauseExpressionWithCache(
        current_OpenMPIR_to_SageIII.first, current_omp_clause->getKind(),
        parsed_nodes, argument);
    if (expression == nullptr) {
      expression = buildOpaqueOpenMPClauseExpression(
          current_OpenMPIR_to_SageIII.first, argument);
    }
    ROSE_ASSERT(expression != nullptr);
    arguments->append_expression(expression);
  }

  SgOmpClause::omp_adjust_args_modifier_enum modifier =
      SgOmpClause::e_omp_adjust_args_modifier_unknown;
  if (adjust_clause->getModifier() == OMPC_ADJUST_ARGS_need_device_ptr) {
    modifier = SgOmpClause::e_omp_adjust_args_modifier_need_device_ptr;
  }

  SgOmpAdjustArgsClause *result =
      new SgOmpAdjustArgsClause(arguments, modifier);
  arguments->set_parent(result);

  if (modifier == SgOmpClause::e_omp_adjust_args_modifier_unknown &&
      !adjust_clause->getRawModifier().empty()) {
    SgExpression *user_defined_modifier = parseClauseExpressionWithCache(
        current_OpenMPIR_to_SageIII.first, current_omp_clause->getKind(),
        parsed_nodes, adjust_clause->getRawModifier(), true);
    if (user_defined_modifier == nullptr) {
      user_defined_modifier = buildOpaqueOpenMPClauseExpression(
          current_OpenMPIR_to_SageIII.first, adjust_clause->getRawModifier());
    }
    result->set_user_defined_modifier(user_defined_modifier);
    if (user_defined_modifier != nullptr) {
      user_defined_modifier->set_parent(result);
    }
  }

  setOneSourcePositionForTransformation(result);
  addOmpClause(directive, result);
  result->set_parent(directive);
  return result;
}

SgOmpAppendArgsClause *
convertAppendArgsClause(SgStatement *directive,
                        std::pair<SgPragmaDeclaration *, OpenMPDirective *>
                            current_OpenMPIR_to_SageIII,
                        OpenMPClause *current_omp_clause) {
  auto *append_clause =
      static_cast<OpenMPAppendArgsClause *>(current_omp_clause);
  const std::vector<const OmpParsedExpression *> *parsed_nodes =
      getParsedClauseExpressionNodes(current_OpenMPIR_to_SageIII.second,
                                     current_omp_clause);

  SgExprListExp *arguments = buildExprListExp();
  for (const std::string &argument : append_clause->getArguments()) {
    SgExpression *expression = parseClauseExpressionWithCache(
        current_OpenMPIR_to_SageIII.first, current_omp_clause->getKind(),
        parsed_nodes, argument);
    if (expression == nullptr) {
      expression = buildOpaqueOpenMPClauseExpression(
          current_OpenMPIR_to_SageIII.first, argument);
    }
    ROSE_ASSERT(expression != nullptr);
    arguments->append_expression(expression);
  }

  SgOmpAppendArgsClause *result = new SgOmpAppendArgsClause(arguments);
  arguments->set_parent(result);

  if (!append_clause->getLabel().empty()) {
    SgExpression *label = parseClauseExpressionWithCache(
        current_OpenMPIR_to_SageIII.first, current_omp_clause->getKind(),
        parsed_nodes, append_clause->getLabel(), true);
    if (label == nullptr) {
      label = buildOpaqueOpenMPClauseExpression(
          current_OpenMPIR_to_SageIII.first, append_clause->getLabel());
    }
    result->set_label(label);
    if (label != nullptr) {
      label->set_parent(result);
    }
  }

  setOneSourcePositionForTransformation(result);
  addOmpClause(directive, result);
  result->set_parent(directive);
  return result;
}

SgOmpSizesClause *
convertSizesClause(SgStatement *directive,
                   std::pair<SgPragmaDeclaration *, OpenMPDirective *>
                       current_OpenMPIR_to_SageIII,
                   OpenMPClause *current_omp_clause) {
  omp_variable_list.clear();
  OpenMPClauseKind clause_kind = current_omp_clause->getKind();
  SgGlobal *global =
      SageInterface::getGlobalScope(current_OpenMPIR_to_SageIII.first);
  std::vector<const char *> *current_expressions =
      current_omp_clause->getExpressions();
  SgExprListExp *explist = buildExprListExp();
  if (current_expressions->size() != 0) {
    std::vector<const char *>::iterator iter;
    for (iter = current_expressions->begin();
         iter != current_expressions->end(); iter++) {
      SgExpression *exp =
          parseOmpExpression(current_OpenMPIR_to_SageIII.first,
                             current_omp_clause->getKind(), *iter);
      explist->append_expression(exp);
    }
  }

  // SgExprListExp* explist = buildExprListExp();
  SgOmpSizesClause *result = new SgOmpSizesClause(explist);
  printf("Sizes Clause added!\n");

  setOneSourcePositionForTransformation(result);
  // buildVariableList(result);
  explist->set_parent(result);
  // reconsider the location of following code to attach clause
  if (current_OpenMPIR_to_SageIII.second->getKind() == OMPD_declare_simd) {
    ((SgOmpDeclareSimdStatement *)directive)->get_clauses().push_back(result);
  } else {
    addOmpClause(directive, result);
  }
  result->set_parent(directive);
  omp_variable_list.clear();
  return result;
}

static bool tryMapFortranReductionUserIdentifier(
    const std::string &raw_identifier,
    SgOmpClause::omp_reduction_identifier_enum &sg_identifier);
static bool tryMapFortranInReductionUserIdentifier(
    const std::string &raw_identifier,
    SgOmpClause::omp_in_reduction_identifier_enum &sg_identifier);
static bool tryMapFortranTaskReductionUserIdentifier(
    const std::string &raw_identifier,
    SgOmpClause::omp_task_reduction_identifier_enum &sg_identifier);

SgOmpVariablesClause *
convertClause(SgStatement *directive,
              std::pair<SgPragmaDeclaration *, OpenMPDirective *>
                  current_OpenMPIR_to_SageIII,
              OpenMPClause *current_omp_clause) {
  omp_variable_list.clear();
  array_dimensions.clear();
  SgOmpVariablesClause *result = NULL;
  OpenMPClauseKind clause_kind = current_omp_clause->getKind();
  SgGlobal *global =
      SageInterface::getGlobalScope(current_OpenMPIR_to_SageIII.first);
  const std::vector<const OmpParsedExpression *> *parsed_nodes =
      getParsedClauseExpressionNodes(current_OpenMPIR_to_SageIII.second,
                                     current_omp_clause);
  if (parsed_nodes != nullptr && !parsed_nodes->empty()) {
    for (const OmpParsedExpression *parsed : *parsed_nodes) {
      if (parsed == nullptr) {
        continue;
      }
      if (parsed->mode == OMP_EXPR_PARSE_variable_list ||
          parsed->mode == OMP_EXPR_PARSE_array_section) {
        appendParsedVariableNode(parsed);
      }
    }
  } else {
    std::vector<const char *> *current_expressions =
        current_omp_clause->getExpressions();
    if (!current_expressions->empty()) {
      for (const char *expression : *current_expressions) {
        parseOmpVariable(current_OpenMPIR_to_SageIII,
                         current_omp_clause->getKind(), expression);
      }
    }
  }

  SgExprListExp *explist = buildExprListExp();
  switch (clause_kind) {
  case OMPC_allocate: {
    OpenMPAllocateClauseAllocator allocate_allocator =
        ((OpenMPAllocateClause *)current_omp_clause)->getAllocator();
    SgOmpClause::omp_allocate_modifier_enum sg_modifier =
        toSgOmpClauseAllocateAllocator(allocate_allocator);
    SgExpression *user_defined_parameter = NULL;
    if (sg_modifier == SgOmpClause::e_omp_allocate_user_defined_modifier) {
      const std::string user_defined_allocator =
          ((OpenMPAllocateClause *)current_omp_clause)
              ->getUserDefinedAllocator();
      SgExpression *clause_expression = cloneParsedExpressionNodeByText(
          parsed_nodes, user_defined_allocator, OMP_EXPR_PARSE_expression);
      if (clause_expression == nullptr) {
        clause_expression = parseOmpExpression(
            current_OpenMPIR_to_SageIII.first, current_omp_clause->getKind(),
            user_defined_allocator);
      }
      ROSE_ASSERT(clause_expression != nullptr);
      user_defined_parameter =
          checkOmpExpressionClause(clause_expression, global, e_allocate);
    }
    result =
        new SgOmpAllocateClause(explist, sg_modifier, user_defined_parameter);
    printf("Allocate Clause added!\n");
    break;
  }
  case OMPC_copyin: {
    result = new SgOmpCopyinClause(explist);
    printf("Copyin Clause added!\n");
    break;
  }
  case OMPC_firstprivate: {
    result = new SgOmpFirstprivateClause(explist);
    printf("Firstprivate Clause added!\n");
    break;
  }
  case OMPC_nontemporal: {
    result = new SgOmpNontemporalClause(explist);
    printf("Nontemporal Clause added!\n");
    break;
  }
  case OMPC_inclusive: {
    result = new SgOmpInclusiveClause(explist);
    printf("Inclusive Clause added!\n");
    break;
  }
  case OMPC_exclusive: {
    result = new SgOmpExclusiveClause(explist);
    printf("Exclusive Clause added!\n");
    break;
  }
  case OMPC_is_device_ptr: {
    result = new SgOmpIsDevicePtrClause(explist);
    printf("is_device_ptr Clause added!\n");
    break;
  }
  case OMPC_use_device_ptr: {
    result = new SgOmpUseDevicePtrClause(explist);
    printf("use_device_ptr Clause added!\n");
    break;
  }
  case OMPC_use_device_addr: {
    result = new SgOmpUseDeviceAddrClause(explist);
    printf("use_device_addr Clause added!\n");
    break;
  }
  case OMPC_has_device_addr: {
    result = new SgOmpHasDeviceAddrClause(explist);
    printf("has_device_addr Clause added!\n");
    break;
  }
  case OMPC_private: {
    result = new SgOmpPrivateClause(explist);
    printf("Private Clause added!\n");
    break;
  }
  case OMPC_copyprivate: {
    result = new SgOmpCopyprivateClause(explist);
    printf("Copyprivate Clause added!\n");
    break;
  }
  case OMPC_reduction: {
    OpenMPReductionClauseModifier modifier =
        ((OpenMPReductionClause *)current_omp_clause)->getModifier();
    SgOmpClause::omp_reduction_modifier_enum sg_modifier =
        toSgOmpClauseReductionModifier(modifier);
    OpenMPReductionClauseIdentifier identifier =
        ((OpenMPReductionClause *)current_omp_clause)->getIdentifier();
    SgOmpClause::omp_reduction_identifier_enum sg_identifier =
        toSgOmpClauseReductionIdentifier(identifier);
    SgExpression *user_defined_identifier = NULL;
    const std::string user_identifier_text =
        ((OpenMPReductionClause *)current_omp_clause)
            ->getUserDefinedIdentifier();
    tryMapFortranReductionUserIdentifier(user_identifier_text, sg_identifier);
    if (sg_identifier == SgOmpClause::e_omp_reduction_user_defined_identifier) {
      SgExpression *clause_expression = parseOmpExpression(
          current_OpenMPIR_to_SageIII.first, current_omp_clause->getKind(),
          user_identifier_text);
      user_defined_identifier =
          checkOmpExpressionClause(clause_expression, global, e_reduction);
    }
    result = new SgOmpReductionClause(explist, sg_modifier, sg_identifier,
                                      user_defined_identifier);
    printf("Reduction Clause added!\n");
    break;
  }
  case OMPC_in_reduction: {
    OpenMPInReductionClauseIdentifier identifier =
        ((OpenMPInReductionClause *)current_omp_clause)->getIdentifier();
    SgOmpClause::omp_in_reduction_identifier_enum sg_identifier =
        toSgOmpClauseInReductionIdentifier(identifier);
    SgExpression *user_defined_identifier = NULL;
    const std::string user_identifier_text =
        ((OpenMPInReductionClause *)current_omp_clause)
            ->getUserDefinedIdentifier();
    tryMapFortranInReductionUserIdentifier(user_identifier_text, sg_identifier);
    if (sg_identifier ==
        SgOmpClause::e_omp_in_reduction_user_defined_identifier) {
      SgExpression *clause_expression = parseOmpExpression(
          current_OpenMPIR_to_SageIII.first, current_omp_clause->getKind(),
          user_identifier_text);
      user_defined_identifier =
          checkOmpExpressionClause(clause_expression, global, e_reduction);
    }
    result = new SgOmpInReductionClause(explist, sg_identifier,
                                        user_defined_identifier);
    printf("In_reduction Clause added!\n");
    break;
  }
  case OMPC_task_reduction: {
    OpenMPTaskReductionClauseIdentifier identifier =
        ((OpenMPTaskReductionClause *)current_omp_clause)->getIdentifier();
    SgOmpClause::omp_task_reduction_identifier_enum sg_identifier =
        toSgOmpClauseTaskReductionIdentifier(identifier);
    SgExpression *user_defined_identifier = NULL;
    const std::string user_identifier_text =
        ((OpenMPTaskReductionClause *)current_omp_clause)
            ->getUserDefinedIdentifier();
    tryMapFortranTaskReductionUserIdentifier(user_identifier_text,
                                             sg_identifier);
    if (sg_identifier ==
        SgOmpClause::e_omp_task_reduction_user_defined_identifier) {
      SgExpression *clause_expression = parseOmpExpression(
          current_OpenMPIR_to_SageIII.first, current_omp_clause->getKind(),
          user_identifier_text);
      user_defined_identifier =
          checkOmpExpressionClause(clause_expression, global, e_reduction);
    }
    result = new SgOmpTaskReductionClause(explist, sg_identifier,
                                          user_defined_identifier);
    printf("Task_reduction Clause added!\n");
    break;
  }
  case OMPC_linear: {
    OpenMPLinearClauseModifier modifier =
        ((OpenMPLinearClause *)current_omp_clause)->getModifier();
    SgOmpClause::omp_linear_modifier_enum sg_modifier =
        toSgOmpClauseLinearModifier(modifier);
    SgExpression *stepExp = NULL;
    if ((((OpenMPLinearClause *)current_omp_clause)->getUserDefinedStep()) !=
        "") {
      stepExp = parseOmpExpression(
          current_OpenMPIR_to_SageIII.first, current_omp_clause->getKind(),
          ((OpenMPLinearClause *)current_omp_clause)->getUserDefinedStep());
    }
    result = new SgOmpLinearClause(explist, stepExp, sg_modifier);
    printf("Linear Clause added!\n");
    break;
  }
  case OMPC_aligned: {
    SgExpression *alignExp = NULL;
    if ((((OpenMPAlignedClause *)current_omp_clause)
             ->getUserDefinedAlignment()) != "") {
      alignExp = parseOmpExpression(current_OpenMPIR_to_SageIII.first,
                                    current_omp_clause->getKind(),
                                    ((OpenMPAlignedClause *)current_omp_clause)
                                        ->getUserDefinedAlignment());
    }
    result = new SgOmpAlignedClause(explist, alignExp);
    printf("Aligned Clause added!\n");
    break;
  }
  case OMPC_lastprivate: {
    OpenMPLastprivateClauseModifier modifier =
        ((OpenMPLastprivateClause *)current_omp_clause)->getModifier();
    SgOmpClause::omp_lastprivate_modifier_enum sg_modifier =
        toSgOmpClauseLastprivateModifier(modifier);
    result = new SgOmpLastprivateClause(explist, sg_modifier);
    printf("Lastprivate Clause added!\n");
    break;
  }
  case OMPC_shared: {
    result = new SgOmpSharedClause(explist);
    printf("Shared Clause added!\n");
    break;
  }
  case OMPC_uniform: {
    result = new SgOmpUniformClause(explist);
    printf("Uniform Clause added!\n");
    break;
  }
  default: {
    printf("Unknown Clause!\n");
  }
  }
  setOneSourcePositionForTransformation(result);
  buildVariableList(result);
  explist->set_parent(result);
  // reconsider the location of following code to attach clause
  if (current_OpenMPIR_to_SageIII.second->getKind() == OMPD_declare_simd) {
    ((SgOmpDeclareSimdStatement *)directive)->get_clauses().push_back(result);
  } else {
    addOmpClause(directive, result);
  }
  result->set_parent(directive);
  omp_variable_list.clear();
  return result;
}

SgOmpToClause *
convertToClause(SgStatement *clause_body,
                std::pair<SgPragmaDeclaration *, OpenMPDirective *>
                    current_OpenMPIR_to_SageIII,
                OpenMPClause *current_omp_clause) {
  MLOG_DEBUG_C("ompAstConstruction", "ompparser to clause is ready.\n");
  SgOmpToClause *result = NULL;
  OpenMPToClauseKind kind = ((OpenMPToClause *)current_omp_clause)->getKind();
  SgOmpClause::omp_to_kind_enum sg_type = toSgOmpClauseToKind(kind);
  SgExpression *mapper_identifier = NULL;

  omp_variable_list.clear();
  array_dimensions.clear();
  const std::vector<const OmpParsedExpression *> *parsed_nodes =
      getParsedClauseExpressionNodes(current_OpenMPIR_to_SageIII.second,
                                     current_omp_clause);
  if (parsed_nodes != nullptr && !parsed_nodes->empty()) {
    for (const OmpParsedExpression *parsed : *parsed_nodes) {
      if (parsed == nullptr) {
        continue;
      }
      if (parsed->mode == OMP_EXPR_PARSE_array_section ||
          parsed->mode == OMP_EXPR_PARSE_variable_list) {
        appendParsedVariableNode(parsed);
      }
    }
  } else {
    std::vector<const char *> *current_expressions =
        current_omp_clause->getExpressions();
    if (!current_expressions->empty()) {
      for (const char *expression : *current_expressions) {
        parseOmpArraySection(current_OpenMPIR_to_SageIII.first,
                             current_omp_clause->getKind(), expression);
      }
    }
  }
  SgExprListExp *explist = buildExprListExp();

  result = new SgOmpToClause(explist, sg_type);
  if ((((OpenMPToClause *)current_omp_clause)->getMapperIdentifier()) != "") {
    mapper_identifier = parseMapperIdentifierExpression(
        current_OpenMPIR_to_SageIII.first, current_omp_clause->getKind(),
        parsed_nodes,
        ((OpenMPToClause *)current_omp_clause)->getMapperIdentifier());
    ROSE_ASSERT(mapper_identifier != nullptr);
  }
  result->set_mapper_identifier(mapper_identifier);
  ROSE_ASSERT(result != NULL);
  buildVariableList(result);
  explist->set_parent(result);
  result->set_array_dimensions(array_dimensions);
  result->set_iterator(buildClauseIteratorDefinitions(
      current_OpenMPIR_to_SageIII.first, current_omp_clause->getKind(),
      parsed_nodes,
      static_cast<OpenMPToClause *>(current_omp_clause)->getIterators()));

  setOneSourcePositionForTransformation(result);
  SgOmpClause *sg_clause = result;
  if (current_OpenMPIR_to_SageIII.second->getKind() == OMPD_target_update) {
    ((SgOmpTargetUpdateStatement *)clause_body)
        ->get_clauses()
        .push_back(sg_clause);
  } else if (current_OpenMPIR_to_SageIII.second->getKind() ==
             OMPD_declare_target) {
    ((SgOmpDeclareTargetStatement *)clause_body)
        ->get_clauses()
        .push_back(sg_clause);
  }
  sg_clause->set_parent(clause_body);
  array_dimensions.clear();
  omp_variable_list.clear();
  MLOG_DEBUG_C("ompAstConstruction", "ompparser to clause is added.\n");
  return result;
}

SgOmpFromClause *
convertFromClause(SgStatement *clause_body,
                  std::pair<SgPragmaDeclaration *, OpenMPDirective *>
                      current_OpenMPIR_to_SageIII,
                  OpenMPClause *current_omp_clause) {
  MLOG_DEBUG_C("ompAstConstruction", "ompparser from clause is ready.\n");
  SgOmpFromClause *result = NULL;
  OpenMPFromClauseKind kind =
      ((OpenMPFromClause *)current_omp_clause)->getKind();
  SgOmpClause::omp_from_kind_enum sg_type = toSgOmpClauseFromKind(kind);
  SgExpression *mapper_identifier = NULL;

  omp_variable_list.clear();
  array_dimensions.clear();
  const std::vector<const OmpParsedExpression *> *parsed_nodes =
      getParsedClauseExpressionNodes(current_OpenMPIR_to_SageIII.second,
                                     current_omp_clause);
  if (parsed_nodes != nullptr && !parsed_nodes->empty()) {
    for (const OmpParsedExpression *parsed : *parsed_nodes) {
      if (parsed == nullptr) {
        continue;
      }
      if (parsed->mode == OMP_EXPR_PARSE_array_section ||
          parsed->mode == OMP_EXPR_PARSE_variable_list) {
        appendParsedVariableNode(parsed);
      }
    }
  } else {
    std::vector<const char *> *current_expressions =
        current_omp_clause->getExpressions();
    if (!current_expressions->empty()) {
      for (const char *expression : *current_expressions) {
        parseOmpArraySection(current_OpenMPIR_to_SageIII.first,
                             current_omp_clause->getKind(), expression);
      }
    }
  }
  SgExprListExp *explist = buildExprListExp();
  result = new SgOmpFromClause(explist, sg_type);
  if ((((OpenMPFromClause *)current_omp_clause)->getMapperIdentifier()) != "") {
    mapper_identifier = parseMapperIdentifierExpression(
        current_OpenMPIR_to_SageIII.first, current_omp_clause->getKind(),
        parsed_nodes,
        ((OpenMPFromClause *)current_omp_clause)->getMapperIdentifier());
    ROSE_ASSERT(mapper_identifier != nullptr);
  }
  result->set_mapper_identifier(mapper_identifier);
  ROSE_ASSERT(result != NULL);
  buildVariableList(result);
  explist->set_parent(result);
  result->set_array_dimensions(array_dimensions);
  result->set_iterator(buildClauseIteratorDefinitions(
      current_OpenMPIR_to_SageIII.first, current_omp_clause->getKind(),
      parsed_nodes,
      static_cast<OpenMPFromClause *>(current_omp_clause)->getIterators()));

  setOneSourcePositionForTransformation(result);
  SgOmpClause *sg_clause = result;
  if (current_OpenMPIR_to_SageIII.second->getKind() == OMPD_target_update) {
    ((SgOmpTargetUpdateStatement *)clause_body)
        ->get_clauses()
        .push_back(sg_clause);
  } else if (current_OpenMPIR_to_SageIII.second->getKind() ==
             OMPD_declare_target) {
    ((SgOmpDeclareTargetStatement *)clause_body)
        ->get_clauses()
        .push_back(sg_clause);
  }
  sg_clause->set_parent(clause_body);
  array_dimensions.clear();
  omp_variable_list.clear();
  MLOG_DEBUG_C("ompAstConstruction", "ompparser from clause is added.\n");
  return result;
}

SgOmpDependClause *
convertDependClause(SgStatement *clause_body,
                    std::pair<SgPragmaDeclaration *, OpenMPDirective *>
                        current_OpenMPIR_to_SageIII,
                    OpenMPClause *current_omp_clause) {
  SgOmpDependClause *result = NULL;
  clearOpenMPClauseTemporaryState();

  SgExpression *iterator_type = NULL;
  SgExpression *identifier = NULL;
  SgExpression *begin = NULL;
  SgExpression *end = NULL;
  SgExpression *step = NULL;
  const std::vector<const OmpParsedExpression *> *parsed_nodes =
      getParsedClauseExpressionNodes(current_OpenMPIR_to_SageIII.second,
                                     current_omp_clause);

  auto *depend_clause = static_cast<OpenMPDependClause *>(current_omp_clause);
  OpenMPDependClauseModifier modifier = depend_clause->getModifier();
  std::list<std::list<SgExpression *>> depend_iterators_definition_class;
  if (modifier == OMPC_DEPEND_MODIFIER_iterator) {
    const auto &omp_depend_iterators = depend_clause->getIterators();
    for (const auto &iterator_def : omp_depend_iterators) {
      std::list<SgExpression *> iterator_expressions;
      if (!iterator_def.qualifier.empty()) {
        const OmpParsedExpression *parsed = findParsedExpressionByText(
            parsed_nodes, iterator_def.qualifier, OMP_EXPR_PARSE_expression);
        iterator_type = cloneParsedExpressionNode(parsed);
        if (iterator_type == NULL) {
          iterator_type = parseOmpExpression(current_OpenMPIR_to_SageIII.first,
                                             current_omp_clause->getKind(),
                                             iterator_def.qualifier);
        }
        iterator_expressions.push_back(iterator_type);
      } else {
        iterator_type = NULL;
        iterator_expressions.push_back(iterator_type);
      }
      {
        const OmpParsedExpression *parsed = findParsedExpressionByText(
            parsed_nodes, iterator_def.var, OMP_EXPR_PARSE_expression);
        identifier = cloneParsedExpressionNode(parsed);
        if (identifier == NULL) {
          identifier = parseOmpExpression(current_OpenMPIR_to_SageIII.first,
                                          current_omp_clause->getKind(),
                                          iterator_def.var);
        }
      }
      iterator_expressions.push_back(identifier);
      {
        const OmpParsedExpression *parsed = findParsedExpressionByText(
            parsed_nodes, iterator_def.begin, OMP_EXPR_PARSE_expression);
        begin = cloneParsedExpressionNode(parsed);
        if (begin == NULL) {
          begin = parseOmpExpression(current_OpenMPIR_to_SageIII.first,
                                     current_omp_clause->getKind(),
                                     iterator_def.begin);
        }
      }
      iterator_expressions.push_back(begin);
      {
        const OmpParsedExpression *parsed = findParsedExpressionByText(
            parsed_nodes, iterator_def.end, OMP_EXPR_PARSE_expression);
        end = cloneParsedExpressionNode(parsed);
        if (end == NULL) {
          end = parseOmpExpression(current_OpenMPIR_to_SageIII.first,
                                   current_omp_clause->getKind(),
                                   iterator_def.end);
        }
      }
      iterator_expressions.push_back(end);

      if (!iterator_def.step.empty()) {
        const OmpParsedExpression *parsed = findParsedExpressionByText(
            parsed_nodes, iterator_def.step, OMP_EXPR_PARSE_expression);
        step = cloneParsedExpressionNode(parsed);
        if (step == NULL) {
          step = parseOmpExpression(current_OpenMPIR_to_SageIII.first,
                                    current_omp_clause->getKind(),
                                    iterator_def.step);
        }
        iterator_expressions.push_back(step);
      } else {
        step = NULL;
        iterator_expressions.push_back(step);
      }
      depend_iterators_definition_class.push_back(iterator_expressions);
    }
  }
  SgOmpClause::omp_depend_modifier_enum sg_modifier =
      toSgOmpClauseDependModifier(modifier);
  OpenMPDependClauseType type = depend_clause->getType();
  SgOmpClause::omp_dependence_type_enum sg_type =
      toSgOmpClauseDependenceType(type);
  SgExprListExp *explist = NULL;
  std::list<SgExpression *> vec_list;
  size_t depend_expression_count = 0;
  if (type != OMPC_DEPENDENCE_TYPE_sink) {
    if (parsed_nodes != nullptr && !parsed_nodes->empty()) {
      for (const OmpParsedExpression *parsed : *parsed_nodes) {
        if (parsed == nullptr) {
          continue;
        }
        if (parsed->mode == OMP_EXPR_PARSE_array_section ||
            parsed->mode == OMP_EXPR_PARSE_variable_list) {
          appendParsedVariableNode(parsed);
        } else if (parsed->mode == OMP_EXPR_PARSE_expression &&
                   parsed->node != nullptr) {
          omp_variable_list.push_back(
              std::make_pair(parsed->text, parsed->node));
        }
      }
      depend_expression_count = parsed_nodes->size();
    } else {
      std::vector<const char *> *current_expressions =
          current_omp_clause->getExpressions();
      depend_expression_count = current_expressions->size();
      if (!current_expressions->empty()) {
        for (const char *raw_expression : *current_expressions) {
          ROSE_ASSERT(raw_expression != NULL);
          const std::string expression_text(raw_expression);
          parseOmpArraySection(current_OpenMPIR_to_SageIII.first,
                               current_omp_clause->getKind(), expression_text);
        }
      }
    }
    explist = buildExprListExp();
  } else if (type == OMPC_DEPENDENCE_TYPE_sink) {
    explist = buildExprListExp();
    if (parsed_nodes != nullptr && !parsed_nodes->empty()) {
      for (const OmpParsedExpression *parsed : *parsed_nodes) {
        if (parsed == nullptr) {
          continue;
        }
        SgExpression *parsed_expr = cloneParsedExpressionNode(parsed);
        if (parsed_expr == nullptr && !parsed->text.empty()) {
          parsed_expr =
              parseOmpExpression(current_OpenMPIR_to_SageIII.first,
                                 current_omp_clause->getKind(), parsed->text);
        }
        if (parsed_expr != nullptr) {
          vec_list.push_back(parsed_expr);
        }
      }
    } else {
      std::vector<const char *> *current_expressions =
          current_omp_clause->getExpressions();
      if (!current_expressions->empty()) {
        for (const char *expression : *current_expressions) {
          SgExpression *vec =
              parseOmpExpression(current_OpenMPIR_to_SageIII.first,
                                 current_omp_clause->getKind(), expression);
          vec_list.push_back(vec);
        }
      }
    }
  }
  result = new SgOmpDependClause(explist, sg_modifier, sg_type);
  ROSE_ASSERT(result != NULL);
  if (type != OMPC_DEPENDENCE_TYPE_sink &&
      type != OMPC_DEPENDENCE_TYPE_source && depend_expression_count > 0) {
    ROSE_ASSERT(!omp_variable_list.empty());
  }
  buildVariableList(result);
  if (type != OMPC_DEPENDENCE_TYPE_sink)
    explist->set_parent(result);
  result->set_vec(vec_list);
  result->set_array_dimensions(array_dimensions);
  result->set_iterator(depend_iterators_definition_class);
  setOneSourcePositionForTransformation(result);
  SgOmpClause *sg_clause = result;
  if (current_OpenMPIR_to_SageIII.second->getKind() == OMPD_target_update) {
    ((SgOmpTargetUpdateStatement *)clause_body)
        ->get_clauses()
        .push_back(sg_clause);
  } else if (current_OpenMPIR_to_SageIII.second->getKind() == OMPD_taskwait) {
    ((SgOmpTaskwaitStatement *)clause_body)->get_clauses().push_back(sg_clause);
  } else if (current_OpenMPIR_to_SageIII.second->getKind() == OMPD_ordered) {
    ((SgOmpOrderedDependStatement *)clause_body)
        ->get_clauses()
        .push_back(sg_clause);
  } else {
    ((SgOmpClauseBodyStatement *)clause_body)
        ->get_clauses()
        .push_back(sg_clause);
  }
  sg_clause->set_parent(clause_body);
  array_dimensions.clear();
  omp_variable_list.clear();
  return result;
}

SgOmpAffinityClause *
convertAffinityClause(SgStatement *clause_body,
                      std::pair<SgPragmaDeclaration *, OpenMPDirective *>
                          current_OpenMPIR_to_SageIII,
                      OpenMPClause *current_omp_clause) {
  SgOmpAffinityClause *result = NULL;

  SgExpression *iterator_type = NULL;
  SgExpression *identifier = NULL;
  SgExpression *begin = NULL;
  SgExpression *end = NULL;
  SgExpression *step = NULL;

  auto *affinity_clause =
      static_cast<OpenMPAffinityClause *>(current_omp_clause);
  OpenMPAffinityClauseModifier modifier = affinity_clause->getModifier();
  std::list<std::list<SgExpression *>> affinity_iterators_definition_class;
  if (modifier == OMPC_AFFINITY_MODIFIER_iterator) {
    const auto &omp_affinity_iterators = affinity_clause->getIterators();
    for (const auto &iterator_def : omp_affinity_iterators) {
      std::list<SgExpression *> iterator_expressions;
      if (!iterator_def.qualifier.empty()) {
        iterator_type = parseOmpExpression(current_OpenMPIR_to_SageIII.first,
                                           current_omp_clause->getKind(),
                                           iterator_def.qualifier);
        iterator_expressions.push_back(iterator_type);
      } else {
        iterator_type = NULL;
        iterator_expressions.push_back(iterator_type);
      }
      identifier =
          parseOmpExpression(current_OpenMPIR_to_SageIII.first,
                             current_omp_clause->getKind(), iterator_def.var);
      iterator_expressions.push_back(identifier);
      begin =
          parseOmpExpression(current_OpenMPIR_to_SageIII.first,
                             current_omp_clause->getKind(), iterator_def.begin);
      iterator_expressions.push_back(begin);
      end = parseOmpExpression(current_OpenMPIR_to_SageIII.first,
                               current_omp_clause->getKind(), iterator_def.end);
      iterator_expressions.push_back(end);

      if (!iterator_def.step.empty()) {
        step = parseOmpExpression(current_OpenMPIR_to_SageIII.first,
                                  current_omp_clause->getKind(),
                                  iterator_def.step);
        iterator_expressions.push_back(step);
      } else {
        step = NULL;
        iterator_expressions.push_back(step);
      }
      affinity_iterators_definition_class.push_back(iterator_expressions);
    }
  }
  SgOmpClause::omp_affinity_modifier_enum sg_modifier =
      toSgOmpClauseAffinityModifier(modifier);

  std::vector<const char *> *current_expressions =
      current_omp_clause->getExpressions();
  if (current_expressions->size() != 0) {
    std::vector<const char *>::iterator iter;
    for (iter = current_expressions->begin();
         iter != current_expressions->end(); iter++) {
      parseOmpArraySection(current_OpenMPIR_to_SageIII.first,
                           current_omp_clause->getKind(), *iter);
    }
  }
  SgExprListExp *explist = buildExprListExp();

  result = new SgOmpAffinityClause(explist, sg_modifier);
  ROSE_ASSERT(result != NULL);
  buildVariableList(result);
  explist->set_parent(result);
  result->set_array_dimensions(array_dimensions);
  result->set_iterator(affinity_iterators_definition_class);

  setOneSourcePositionForTransformation(result);
  SgOmpClause *sg_clause = result;
  ((SgOmpClauseBodyStatement *)clause_body)->get_clauses().push_back(sg_clause);
  sg_clause->set_parent(clause_body);
  array_dimensions.clear();
  omp_variable_list.clear();
  return result;
}

SgOmpExpressionClause *
convertExpressionClause(SgStatement *directive,
                        std::pair<SgPragmaDeclaration *, OpenMPDirective *>
                            current_OpenMPIR_to_SageIII,
                        OpenMPClause *current_omp_clause) {
  SgOmpExpressionClause *result = NULL;
  SgExpression *clause_expression = NULL;
  SgGlobal *global =
      SageInterface::getGlobalScope(current_OpenMPIR_to_SageIII.first);
  OpenMPClauseKind clause_kind = current_omp_clause->getKind();
  const std::vector<const OmpParsedExpression *> *parsed_nodes =
      getParsedClauseExpressionNodes(current_OpenMPIR_to_SageIII.second,
                                     current_omp_clause);
  if (parsed_nodes != nullptr && !parsed_nodes->empty()) {
    for (const OmpParsedExpression *parsed : *parsed_nodes) {
      if (parsed == nullptr) {
        continue;
      }
      clause_expression = cloneParsedExpressionNode(parsed);
      if (clause_expression == nullptr && !parsed->text.empty()) {
        clause_expression =
            parseOmpExpression(current_OpenMPIR_to_SageIII.first,
                               current_omp_clause->getKind(), parsed->text);
      }
    }
  } else {
    std::vector<const char *> *current_expressions =
        current_omp_clause->getExpressions();
    if (!current_expressions->empty()) {
      for (const char *expression : *current_expressions) {
        clause_expression =
            parseOmpExpression(current_OpenMPIR_to_SageIII.first,
                               current_omp_clause->getKind(), expression);
      }
    }
  }

  switch (clause_kind) {
  case OMPC_if: {
    OpenMPIfClauseModifier if_modifier =
        ((OpenMPIfClause *)current_omp_clause)->getModifier();
    SgOmpClause::omp_if_modifier_enum sg_modifier =
        toSgOmpClauseIfModifier(if_modifier);
    clause_expression->set_parent(current_OpenMPIR_to_SageIII.first);
    SgExpression *if_expression =
        checkOmpExpressionClause(clause_expression, global, e_num_threads);
    result = new SgOmpIfClause(if_expression, sg_modifier);
    printf("If Clause added!\n");
    break;
  }
  case OMPC_num_threads: {
    SgExpression *num_threads_expression =
        checkOmpExpressionClause(clause_expression, global, e_num_threads);
    result = new SgOmpNumThreadsClause(num_threads_expression);
    printf("Num_threads Clause added!\n");
    break;
  }
  case OMPC_num_teams: {
    SgExpression *num_teams_expression =
        checkOmpExpressionClause(clause_expression, global, e_num_threads);
    result = new SgOmpNumTeamsClause(num_teams_expression);
    printf("Num_teams Clause added!\n");
    break;
  }
  case OMPC_grainsize: {
    SgExpression *grainsize_expression =
        checkOmpExpressionClause(clause_expression, global, e_num_threads);
    OpenMPGrainsizeClauseModifier modifier =
        static_cast<OpenMPGrainsizeClause *>(current_omp_clause)->getModifier();
    result = new SgOmpGrainsizeClause(grainsize_expression,
                                      toSgOmpClauseGrainsizeModifier(modifier));
    printf("Grainsize Clause added!\n");
    break;
  }
  case OMPC_detach: {
    SgExpression *detach_expression =
        checkOmpExpressionClause(clause_expression, global, e_num_threads);
    result = new SgOmpDetachClause(detach_expression);
    printf("Detach Clause added!\n");
    break;
  }
  case OMPC_num_tasks: {
    SgExpression *num_tasks_expression =
        checkOmpExpressionClause(clause_expression, global, e_num_threads);
    OpenMPNumTasksClauseModifier modifier =
        static_cast<OpenMPNumTasksClause *>(current_omp_clause)->getModifier();
    result = new SgOmpNumTasksClause(num_tasks_expression,
                                     toSgOmpClauseNumTasksModifier(modifier));
    printf("Num_tasks Clause added!\n");
    break;
  }
  case OMPC_final: {
    SgExpression *final_expression =
        checkOmpExpressionClause(clause_expression, global, e_num_threads);
    result = new SgOmpFinalClause(final_expression);
    printf("Final Clause added!\n");
    break;
  }
  case OMPC_priority: {
    SgExpression *priority_expression =
        checkOmpExpressionClause(clause_expression, global, e_num_threads);
    result = new SgOmpPriorityClause(priority_expression);
    printf("Priority Clause added!\n");
    break;
  }
  case OMPC_hint: {
    SgExpression *hint_expression =
        checkOmpExpressionClause(clause_expression, global, e_num_threads);
    result = new SgOmpHintClause(hint_expression);
    printf("hint Clause added!\n");
    break;
  }
  case OMPC_safelen: {
    SgExpression *safelen_expression =
        checkOmpExpressionClause(clause_expression, global, e_num_threads);
    result = new SgOmpSafelenClause(safelen_expression);
    printf("Safelen Clause added!\n");
    break;
  }
  case OMPC_simdlen: {
    SgExpression *simdlen_expression =
        checkOmpExpressionClause(clause_expression, global, e_num_threads);
    result = new SgOmpSimdlenClause(simdlen_expression);
    printf("Simdlen Clause added!\n");
    break;
  }
  case OMPC_ordered: {
    SgExpression *ordered_expression =
        checkOmpExpressionClause(clause_expression, global, e_ordered_clause);
    result = new SgOmpOrderedClause(ordered_expression);
    printf("Ordered Clause added!\n");
    break;
  }
  case OMPC_collapse: {
    SgExpression *collapse_expression =
        checkOmpExpressionClause(clause_expression, global, e_num_threads);
    result = new SgOmpCollapseClause(collapse_expression);
    printf("Collapse Clause added!\n");
    break;
  }
  case OMPC_thread_limit: {
    SgExpression *thread_limit_expression =
        checkOmpExpressionClause(clause_expression, global, e_num_threads);
    result = new SgOmpThreadLimitClause(thread_limit_expression);
    printf("Thread_limit Clause added!\n");
    break;
  }
  case OMPC_device: {
    OpenMPDeviceClauseModifier modifier =
        ((OpenMPDeviceClause *)current_omp_clause)->getModifier();
    SgOmpClause::omp_device_modifier_enum sg_modifier =
        toSgOmpClauseDeviceModifier(modifier);
    clause_expression->set_parent(current_OpenMPIR_to_SageIII.first);
    SgExpression *device_expression =
        checkOmpExpressionClause(clause_expression, global, e_num_threads);
    result = new SgOmpDeviceClause(device_expression, sg_modifier);
    printf("Device Clause added!\n");
    break;
  }
  case OMPC_nocontext: {
    SgExpression *nocontext_expression =
        checkOmpExpressionClause(clause_expression, global, e_num_threads);
    result = new SgOmpNocontextClause(nocontext_expression);
    printf("Nocontext Clause added!\n");
    break;
  }
  case OMPC_novariants: {
    SgExpression *novariants_expression =
        checkOmpExpressionClause(clause_expression, global, e_num_threads);
    result = new SgOmpNovariantsClause(novariants_expression);
    printf("Novariants Clause added!\n");
    break;
  }
  case OMPC_partial: {
    SgExpression *partial_expression =
        checkOmpExpressionClause(clause_expression, global, e_num_threads);
    result = new SgOmpPartialClause(partial_expression);
    printf("Partial Clause added!\n");
    break;
  }
  default: {
    printf("Unknown Clause!\n");
  }
  }
  setOneSourcePositionForTransformation(result);
  if (result != NULL) {
    if (SgExpression *result_expression = result->get_expression()) {
      result_expression->set_parent(result);
    }
  }

  // reconsider the location of following code to attach clause
  if (current_OpenMPIR_to_SageIII.second->getKind() == OMPD_declare_simd) {
    ((SgOmpDeclareSimdStatement *)directive)->get_clauses().push_back(result);
  } else if (current_OpenMPIR_to_SageIII.second->getKind() ==
             OMPD_target_update) {
    ((SgOmpTargetUpdateStatement *)directive)->get_clauses().push_back(result);
  } else {
    addOmpClause(directive, result);
  }
  result->set_parent(directive);

  return result;
}

static bool isDirectiveIdentifierCharacter(char c) {
  const unsigned char uc = static_cast<unsigned char>(c);
  return std::isalnum(uc) || c == '_';
}

struct CanonicalDirectiveChar {
  char value = '\0';
  std::size_t begin = 0;
  std::size_t end = 0;
};

static void
appendCanonicalDirectiveToken(std::vector<CanonicalDirectiveChar> &out,
                              const std::string &source, std::size_t begin,
                              std::size_t end, const std::string &token) {
  if (begin >= end || begin >= source.size()) {
    return;
  }
  const std::size_t bounded_end = std::min(end, source.size());
  if (begin >= bounded_end) {
    return;
  }
  for (char ch : token) {
    CanonicalDirectiveChar mapped;
    mapped.value = ch;
    mapped.begin = begin;
    mapped.end = bounded_end;
    out.push_back(mapped);
  }
}

static bool consumeFortranTextOperator(const std::string &text, std::size_t pos,
                                       std::size_t &next_pos,
                                       std::string &canonical) {
  canonical.clear();
  next_pos = pos;
  if (pos >= text.size() || text[pos] != '.') {
    return false;
  }

  struct OperatorSpelling {
    const char *spelling;
    const char *canonical;
  };

  static const OperatorSpelling kOps[] = {
      {".eqv.", "=="}, {".neqv.", "!="}, {".eq.", "=="}, {".ne.", "!="},
      {".ge.", ">="},  {".gt.", ">"},    {".le.", "<="}, {".lt.", "<"},
      {".and.", "&&"}, {".or.", "||"},   {".not.", "!"}, {".xor.", "^"}};

  const std::size_t remaining = text.size() - pos;
  for (const OperatorSpelling &op : kOps) {
    const std::size_t len = std::strlen(op.spelling);
    if (remaining < len) {
      continue;
    }
    bool matches = true;
    for (std::size_t i = 0; i < len; ++i) {
      const unsigned char lhs = static_cast<unsigned char>(text[pos + i]);
      const unsigned char rhs = static_cast<unsigned char>(op.spelling[i]);
      if (std::tolower(lhs) != std::tolower(rhs)) {
        matches = false;
        break;
      }
    }
    if (!matches) {
      continue;
    }
    canonical = op.canonical;
    next_pos = pos + len;
    return true;
  }

  return false;
}

static std::vector<CanonicalDirectiveChar>
canonicalizeDirectiveExpressionForMatch(const std::string &text) {
  std::vector<CanonicalDirectiveChar> canonical;
  canonical.reserve(text.size());

  for (std::size_t i = 0; i < text.size();) {
    const unsigned char uch = static_cast<unsigned char>(text[i]);
    if (std::isspace(uch)) {
      ++i;
      continue;
    }

    std::size_t next_pos = i;
    std::string mapped_token;
    if (consumeFortranTextOperator(text, i, next_pos, mapped_token)) {
      appendCanonicalDirectiveToken(canonical, text, i, next_pos, mapped_token);
      i = next_pos;
      continue;
    }

    if (text.compare(i, 2, "==") == 0) {
      appendCanonicalDirectiveToken(canonical, text, i, i + 2, "==");
      i += 2;
      continue;
    }
    if (text.compare(i, 2, "!=") == 0 || text.compare(i, 2, "/=") == 0) {
      appendCanonicalDirectiveToken(canonical, text, i, i + 2, "!=");
      i += 2;
      continue;
    }
    if (text.compare(i, 2, ">=") == 0) {
      appendCanonicalDirectiveToken(canonical, text, i, i + 2, ">=");
      i += 2;
      continue;
    }
    if (text.compare(i, 2, "<=") == 0) {
      appendCanonicalDirectiveToken(canonical, text, i, i + 2, "<=");
      i += 2;
      continue;
    }
    if (text.compare(i, 2, "&&") == 0) {
      appendCanonicalDirectiveToken(canonical, text, i, i + 2, "&&");
      i += 2;
      continue;
    }
    if (text.compare(i, 2, "||") == 0) {
      appendCanonicalDirectiveToken(canonical, text, i, i + 2, "||");
      i += 2;
      continue;
    }

    CanonicalDirectiveChar mapped;
    mapped.value = static_cast<char>(std::tolower(uch));
    mapped.begin = i;
    mapped.end = i + 1;
    canonical.push_back(mapped);
    ++i;
  }

  return canonical;
}

static std::string
canonicalDirectiveString(const std::vector<CanonicalDirectiveChar> &canonical) {
  std::string value;
  value.reserve(canonical.size());
  for (const CanonicalDirectiveChar &entry : canonical) {
    value.push_back(entry.value);
  }
  return value;
}

static std::string
restoreFortranExpressionSourceCaseFromText(const std::string &source_text,
                                           const std::string &expression) {
  std::string trimmed = trimWhitespaceCopy(expression);
  if (trimmed.empty() || source_text.empty()) {
    return trimmed;
  }
  const std::string lowered_source = toLowerCopy(source_text);
  const std::string lowered_expr = toLowerCopy(trimmed);
  const bool enforce_boundaries =
      isDirectiveIdentifierCharacter(trimmed.front()) &&
      isDirectiveIdentifierCharacter(trimmed.back());

  std::string::size_type pos = 0;
  while ((pos = lowered_source.find(lowered_expr, pos)) != std::string::npos) {
    if (enforce_boundaries) {
      const bool left_ok =
          (pos == 0) || !isDirectiveIdentifierCharacter(source_text[pos - 1]);
      const std::string::size_type end_pos = pos + trimmed.size();
      const bool right_ok =
          (end_pos >= source_text.size()) ||
          !isDirectiveIdentifierCharacter(source_text[end_pos]);
      if (!(left_ok && right_ok)) {
        ++pos;
        continue;
      }
    }
    return source_text.substr(pos, trimmed.size());
  }

  const std::vector<CanonicalDirectiveChar> canonical_source =
      canonicalizeDirectiveExpressionForMatch(source_text);
  const std::vector<CanonicalDirectiveChar> canonical_expr =
      canonicalizeDirectiveExpressionForMatch(trimmed);
  if (canonical_source.empty() || canonical_expr.empty()) {
    return trimmed;
  }

  const std::string normalized_source =
      canonicalDirectiveString(canonical_source);
  const std::string normalized_expr = canonicalDirectiveString(canonical_expr);
  std::string::size_type normalized_pos = 0;
  while ((normalized_pos = normalized_source.find(
              normalized_expr, normalized_pos)) != std::string::npos) {
    const std::size_t normalized_end =
        normalized_pos + normalized_expr.size() - 1;
    if (normalized_end >= canonical_source.size()) {
      break;
    }

    const std::size_t source_begin = canonical_source[normalized_pos].begin;
    const std::size_t source_end = canonical_source[normalized_end].end;
    if (source_begin >= source_end || source_end > source_text.size()) {
      ++normalized_pos;
      continue;
    }

    if (enforce_boundaries) {
      const bool left_ok =
          (source_begin == 0) ||
          !isDirectiveIdentifierCharacter(source_text[source_begin - 1]);
      const bool right_ok =
          (source_end >= source_text.size()) ||
          !isDirectiveIdentifierCharacter(source_text[source_end]);
      if (!(left_ok && right_ok)) {
        ++normalized_pos;
        continue;
      }
    }

    return source_text.substr(source_begin, source_end - source_begin);
  }

  return trimmed;
}

static std::string
restoreFortranExpressionSourceCase(SgPragmaDeclaration *directive,
                                   const std::string &expression) {
  std::string trimmed = trimWhitespaceCopy(expression);
  if (directive == NULL || trimmed.empty()) {
    return trimmed;
  }

  auto text_it = g_omp_directive_source_text_by_pragma.find(directive);
  if (text_it == g_omp_directive_source_text_by_pragma.end()) {
    return trimmed;
  }

  return restoreFortranExpressionSourceCaseFromText(text_it->second, trimmed);
}

static std::string trimAndLowercase(const std::string &text) {
  std::string value = trimWhitespaceCopy(text);
  std::transform(
      value.begin(), value.end(), value.begin(),
      [](unsigned char ch) { return static_cast<char>(std::tolower(ch)); });
  return value;
}

static bool parseFortranBooleanLiteral(const std::string &text, bool &value) {
  std::string lowered = trimAndLowercase(text);
  while (lowered.size() >= 2 && lowered.front() == '(' &&
         lowered.back() == ')') {
    lowered = trimAndLowercase(lowered.substr(1, lowered.size() - 2));
  }
  if (lowered == ".true.") {
    value = true;
    return true;
  }
  if (lowered == ".false.") {
    value = false;
    return true;
  }
  return false;
}

static bool
supportsOriginalExpressionTreeForDirectiveClause(SgExpression *expression) {
  return expression != NULL && (isSgBinaryOp(expression) != NULL ||
                                isSgVarRefExp(expression) != NULL ||
                                isSgPntrArrRefExp(expression) != NULL ||
                                isSgCastExp(expression) != NULL ||
                                isSgFunctionRefExp(expression) != NULL);
}

static void
attachFortranSourceExpressionTree(SgPragmaDeclaration *directive,
                                  SgExpression *parsed_expression,
                                  const std::string &source_expression) {
  if (directive == NULL || parsed_expression == NULL ||
      !supportsOriginalExpressionTreeForDirectiveClause(parsed_expression)) {
    return;
  }

  const std::string trimmed = trimWhitespaceCopy(source_expression);
  if (trimmed.empty()) {
    return;
  }

  SgExpression *source_tree =
      buildOpaqueOpenMPClauseExpression(directive, trimmed);
  if (source_tree == NULL) {
    return;
  }

  parsed_expression->set_originalExpressionTree(source_tree);
}

static void
preserveFortranVariableSourceSpelling(SgPragmaDeclaration *directive,
                                      const std::string &restored_expression) {
  if (directive == NULL || omp_variable_list.empty()) {
    return;
  }

  std::pair<std::string, SgNode *> &entry = omp_variable_list.back();
  entry.first = restored_expression;

  SgExpression *expr = isSgExpression(entry.second);
  if (expr == NULL) {
    if (SgInitializedName *iname = isSgInitializedName(entry.second)) {
      expr = SageBuilder::buildVarRefExp(iname);
      entry.second = expr;
    } else {
      return;
    }
  }

  if (expr->get_originalExpressionTree() == NULL) {
    attachFortranSourceExpressionTree(directive, expr, restored_expression);
  }
}

static std::string
normalizeFortranReductionIdentifierToken(const std::string &raw_identifier) {
  std::string normalized = trimAndLowercase(raw_identifier);
  while (normalized.size() >= 2 && normalized.front() == '(' &&
         normalized.back() == ')') {
    normalized = trimAndLowercase(normalized.substr(1, normalized.size() - 2));
  }
  return normalized;
}

static bool tryMapFortranReductionUserIdentifier(
    const std::string &raw_identifier,
    SgOmpClause::omp_reduction_identifier_enum &sg_identifier) {
  if (sg_identifier != SgOmpClause::e_omp_reduction_user_defined_identifier) {
    return false;
  }

  const std::string lowered =
      normalizeFortranReductionIdentifierToken(raw_identifier);
  if (lowered == ".eqv.") {
    sg_identifier = SgOmpClause::e_omp_reduction_eqv;
    return true;
  }
  if (lowered == ".neqv.") {
    sg_identifier = SgOmpClause::e_omp_reduction_neqv;
    return true;
  }
  if (lowered == ".xor.") {
    sg_identifier = SgOmpClause::e_omp_reduction_bitxor;
    return true;
  }
  return false;
}

static bool tryMapFortranInReductionUserIdentifier(
    const std::string &raw_identifier,
    SgOmpClause::omp_in_reduction_identifier_enum &sg_identifier) {
  if (sg_identifier !=
      SgOmpClause::e_omp_in_reduction_user_defined_identifier) {
    return false;
  }

  const std::string lowered =
      normalizeFortranReductionIdentifierToken(raw_identifier);
  if (lowered == ".eqv.") {
    sg_identifier = SgOmpClause::e_omp_in_reduction_identifier_eqv;
    return true;
  }
  if (lowered == ".neqv.") {
    sg_identifier = SgOmpClause::e_omp_in_reduction_identifier_neqv;
    return true;
  }
  if (lowered == ".xor.") {
    sg_identifier = SgOmpClause::e_omp_in_reduction_identifier_bitxor;
    return true;
  }
  return false;
}

static bool tryMapFortranTaskReductionUserIdentifier(
    const std::string &raw_identifier,
    SgOmpClause::omp_task_reduction_identifier_enum &sg_identifier) {
  if (sg_identifier !=
      SgOmpClause::e_omp_task_reduction_user_defined_identifier) {
    return false;
  }

  const std::string lowered =
      normalizeFortranReductionIdentifierToken(raw_identifier);
  if (lowered == ".eqv.") {
    sg_identifier = SgOmpClause::e_omp_task_reduction_identifier_eqv;
    return true;
  }
  if (lowered == ".neqv.") {
    sg_identifier = SgOmpClause::e_omp_task_reduction_identifier_neqv;
    return true;
  }
  if (lowered == ".xor.") {
    sg_identifier = SgOmpClause::e_omp_task_reduction_identifier_bitxor;
    return true;
  }
  return false;
}

void parseOmpVariable(std::pair<SgPragmaDeclaration *, OpenMPDirective *>
                          current_OpenMPIR_to_SageIII,
                      OpenMPClauseKind clause_kind, std::string expression,
                      const std::string *directive_source_text) {
  // special handling for omp declare simd directive
  // It may have clauses referencing a variable declared in an immediately
  // followed function's parameter list
  bool look_forward = false;
  if (current_OpenMPIR_to_SageIII.second->getKind() == OMPD_declare_simd &&
      (clause_kind == OMPC_linear || clause_kind == OMPC_simdlen ||
       clause_kind == OMPC_aligned || clause_kind == OMPC_uniform)) {
    look_forward = true;
  };
  SgSourceFile *source_file =
      getEnclosingSourceFile(current_OpenMPIR_to_SageIII.first);
  const bool is_fortran_source =
      source_file != NULL &&
      (source_file->get_Fortran_only() || source_file->get_F77_only() ||
       source_file->get_F90_only() || source_file->get_F95_only() ||
       source_file->get_F2003_only());
  const std::string restored_expression =
      is_fortran_source
          ? (directive_source_text != nullptr
                 ? restoreFortranExpressionSourceCaseFromText(
                       *directive_source_text, expression)
                 : restoreFortranExpressionSourceCase(
                       current_OpenMPIR_to_SageIII.first, expression))
          : expression;
  const std::string parse_expression =
      is_fortran_source ? restored_expression : expression;
  std::string expr_string =
      std::string() + "varlist " + parse_expression + "\n";
  std::size_t old_size = omp_variable_list.size();
  parseExpression(current_OpenMPIR_to_SageIII.first, look_forward,
                  expr_string.c_str());
  if (omp_variable_list.size() != old_size) {
    if (is_fortran_source && !omp_variable_list.empty()) {
      preserveFortranVariableSourceSpelling(current_OpenMPIR_to_SageIII.first,
                                            restored_expression);
    }
    return;
  }

  if (is_fortran_source) {
    std::string trimmed = restored_expression;
    SgExpression *opaque = buildOpaqueOpenMPClauseExpression(
        current_OpenMPIR_to_SageIII.first, trimmed);
    omp_variable_list.push_back(std::make_pair(trimmed, opaque));
  }
}

SgExpression *parseOmpExpression(SgPragmaDeclaration *directive,
                                 OpenMPClauseKind clause_kind,
                                 std::string expression,
                                 const std::string *directive_source_text) {
  // special handling for omp declare simd directive
  // It may have clauses referencing a variable declared in an immediately
  // followed function's parameter list
  bool look_forward = false;
  if (isSgOmpDeclareSimdStatement(directive) &&
      (clause_kind == OMPC_linear || clause_kind == OMPC_simdlen ||
       clause_kind == OMPC_aligned || clause_kind == OMPC_uniform)) {
    look_forward = true;
  };
  SgSourceFile *source_file = getEnclosingSourceFile(directive);
  const bool is_fortran_source =
      source_file != NULL &&
      (source_file->get_Fortran_only() || source_file->get_F77_only() ||
       source_file->get_F90_only() || source_file->get_F95_only() ||
       source_file->get_F2003_only());
  const std::string restored_expression =
      is_fortran_source
          ? (directive_source_text != nullptr
                 ? restoreFortranExpressionSourceCaseFromText(
                       *directive_source_text, expression)
                 : restoreFortranExpressionSourceCase(directive, expression))
          : expression;
  const std::string parse_expression =
      is_fortran_source ? restored_expression : expression;
  if (is_fortran_source) {
    bool bool_value = false;
    if (parseFortranBooleanLiteral(restored_expression, bool_value) ||
        parseFortranBooleanLiteral(expression, bool_value)) {
      return SageBuilder::buildBoolValExp(bool_value);
    }
  }
  std::string expr_string = std::string() + "expr (" + parse_expression + ")\n";
  SgExpression *sg_expression =
      parseExpression(directive, look_forward, expr_string.c_str());
  if (sg_expression != NULL) {
    if (is_fortran_source) {
      attachFortranSourceExpressionTree(directive, sg_expression,
                                        restored_expression);
    }
    return sg_expression;
  }

  if (is_fortran_source) {
    return buildOpaqueOpenMPClauseExpression(directive, restored_expression);
  }

  return NULL;
}

SgExpression *parseOmpArraySection(SgPragmaDeclaration *directive,
                                   OpenMPClauseKind clause_kind,
                                   std::string expression,
                                   const std::string *directive_source_text) {
  // special handling for omp declare simd directive
  // It may have clauses referencing a variable declared in an immediately
  // followed function's parameter list
  bool look_forward = false;
  if (isSgOmpDeclareSimdStatement(directive) &&
      (clause_kind == OMPC_linear || clause_kind == OMPC_simdlen ||
       clause_kind == OMPC_aligned || clause_kind == OMPC_uniform)) {
    look_forward = true;
  };
  SgSourceFile *source_file = getEnclosingSourceFile(directive);
  const bool is_fortran_source =
      source_file != NULL &&
      (source_file->get_Fortran_only() || source_file->get_F77_only() ||
       source_file->get_F90_only() || source_file->get_F95_only() ||
       source_file->get_F2003_only());
  const std::string restored_expression =
      directive_source_text != nullptr
          ? restoreFortranExpressionSourceCaseFromText(*directive_source_text,
                                                       expression)
          : restoreFortranExpressionSourceCase(directive, expression);
  const std::string parse_expression =
      is_fortran_source ? restored_expression : expression;
  std::string expr_string =
      std::string() + "array_section (" + parse_expression + ")\n";
  SgExpression *sg_expression =
      parseArraySectionExpression(directive, look_forward, expr_string.c_str());
  if (sg_expression != NULL) {
    if (is_fortran_source && !omp_variable_list.empty()) {
      preserveFortranVariableSourceSpelling(directive, restored_expression);
    }
    return sg_expression;
  }

  if (is_fortran_source) {
    std::string trimmed = restored_expression;
    SgExpression *opaque =
        buildOpaqueOpenMPClauseExpression(directive, trimmed);
    omp_variable_list.push_back(std::make_pair(trimmed, opaque));
    return opaque;
  }

  return NULL;
}

void buildVariableList(SgOmpVariablesClause *current_omp_clause) {

  std::vector<std::pair<std::string, SgNode *>>::iterator iter;
  for (iter = omp_variable_list.begin(); iter != omp_variable_list.end();
       iter++) {
    if (SgExpression *expr = buildOmpVarExprFromNode((*iter).second)) {
      current_omp_clause->get_variables()->get_expressions().push_back(expr);
      expr->set_parent(current_omp_clause);
    } else {
      cerr << "error: unhandled type of variable within a list:"
           << ((*iter).second)->class_name();
    }
  }
}

SgOmpParallelStatement *convertOmpParallelStatementFromCombinedDirectives(
    std::pair<SgPragmaDeclaration *, OpenMPDirective *>
        current_OpenMPIR_to_SageIII) {
  ROSE_ASSERT(current_OpenMPIR_to_SageIII.second != NULL);
  SgStatement *body = getOpenMPBlockBody(current_OpenMPIR_to_SageIII);
  if (body == NULL) {
    return NULL;
  }
  removeStatement(body, false);

  // build the 2nd directive node first
  SgStatement *second_stmt = NULL;
  switch (current_OpenMPIR_to_SageIII.second->getKind()) {
  case OMPD_parallel_do: {
    second_stmt = new SgOmpDoStatement(NULL, body);
    break;
  }
  case OMPD_parallel_for: {
    second_stmt = new SgOmpForStatement(NULL, body);
    break;
  }
  case OMPD_parallel_for_simd: {
    second_stmt = new SgOmpForSimdStatement(NULL, body);
    break;
  }
  case OMPD_parallel_sections: {
    second_stmt = new SgOmpSectionsStatement(NULL, body);
    break;
  }
  case OMPD_parallel_workshare: {
    second_stmt = new SgOmpWorkshareStatement(NULL, body);
    break;
  }
  default: {
    cerr << "error: unacceptable directive type in "
            "convertOmpParallelStatementFromCombinedDirectives() "
         << endl;
    ROSE_ABORT();
  }
  }

  setOneSourcePositionForTransformation(second_stmt);

  ROSE_ASSERT(second_stmt);
  body->set_parent(second_stmt);

  copyStartFileInfo(current_OpenMPIR_to_SageIII.first, second_stmt);
  copyEndFileInfo(current_OpenMPIR_to_SageIII.first, second_stmt);
  if (SgLocatedNode *located_second = isSgLocatedNode(second_stmt)) {
    located_second->setOutputInCodeGeneration();
  }
  SgOmpParallelStatement *first_stmt =
      new SgOmpParallelStatement(NULL, second_stmt);
  setOneSourcePositionForTransformation(first_stmt);
  copyStartFileInfo(current_OpenMPIR_to_SageIII.first, first_stmt);
  copyEndFileInfo(current_OpenMPIR_to_SageIII.first, first_stmt);
  first_stmt->setOutputInCodeGeneration();
  second_stmt->set_parent(first_stmt);
  first_stmt->addNewAttribute(
      kOmpCombinedParallelNestedVariantAttrName,
      new OmpOwnedIntAttribute(static_cast<int>(second_stmt->variantT())));

  static const char *const kOmpClauseOriginalOrderAttrName =
      "omp_clause_original_order";
  OpenMPClauseKind clause_kind;
  std::vector<OpenMPClause *> *clause_vector =
      current_OpenMPIR_to_SageIII.second->getClausesInOriginalOrder();
  int next_fallback_order = 0;

  auto record_clause_order = [&](SgOmpClause *clause,
                                 OpenMPClause *omp_clause) {
    if (clause == NULL ||
        clause->getAttribute(kOmpClauseOriginalOrderAttrName) != NULL) {
      return;
    }

    int order_index = next_fallback_order++;
    if (omp_clause != NULL) {
      const int original_order = omp_clause->getClausePosition();
      if (original_order >= 0) {
        order_index = original_order;
      }
    }

    clause->addNewAttribute(kOmpClauseOriginalOrderAttrName,
                            new OmpOwnedIntAttribute(order_index));
  };

  std::vector<OpenMPClause *>::iterator citer;
  for (citer = clause_vector->begin(); citer != clause_vector->end(); citer++) {
    clause_kind = (*citer)->getKind();
    switch (clause_kind) {
    case OMPC_collapse:
    case OMPC_ordered:
    case OMPC_if:
    case OMPC_safelen:
    case OMPC_simdlen:
    case OMPC_num_threads: {
      SgOmpClause *added_clause = NULL;
      if (clause_kind == OMPC_collapse || clause_kind == OMPC_ordered) {
        added_clause = convertExpressionClause(
            second_stmt, current_OpenMPIR_to_SageIII, *citer);
      } else if (clause_kind == OMPC_safelen || clause_kind == OMPC_simdlen) {
        added_clause = convertExpressionClause(
            second_stmt, current_OpenMPIR_to_SageIII, *citer);
      } else {
        added_clause =
            convertExpressionClause(isSgOmpClauseBodyStatement(first_stmt),
                                    current_OpenMPIR_to_SageIII, *citer);
      };
      record_clause_order(added_clause, *citer);
      break;
    }
    case OMPC_allocate:
    case OMPC_copyin:
    case OMPC_firstprivate:
    case OMPC_lastprivate:
    case OMPC_aligned:
    case OMPC_linear:
    case OMPC_nontemporal:
    case OMPC_private:
    case OMPC_reduction:
    case OMPC_shared:
    case OMPC_uniform: {
      SgOmpClause *added_clause = NULL;
      if (clause_kind == OMPC_shared || clause_kind == OMPC_copyin) {
        added_clause = convertClause(isSgOmpClauseBodyStatement(first_stmt),
                                     current_OpenMPIR_to_SageIII, *citer);
      } else {
        added_clause =
            convertClause(second_stmt, current_OpenMPIR_to_SageIII, *citer);
      };
      record_clause_order(added_clause, *citer);
      break;
    }
    case OMPC_default: {
      SgOmpClause *added_clause =
          convertDefaultClause(isSgOmpClauseBodyStatement(first_stmt),
                               current_OpenMPIR_to_SageIII, *citer);
      record_clause_order(added_clause, *citer);
      break;
    }
    case OMPC_proc_bind: {
      SgOmpClause *added_clause =
          convertProcBindClause(isSgOmpClauseBodyStatement(first_stmt),
                                current_OpenMPIR_to_SageIII, *citer);
      record_clause_order(added_clause, *citer);
      break;
    }
    case OMPC_schedule: {
      SgOmpClause *added_clause = convertScheduleClause(
          second_stmt, current_OpenMPIR_to_SageIII, *citer);
      record_clause_order(added_clause, *citer);
      break;
    }
    case OMPC_order: {
      SgOmpClause *added_clause =
          convertOrderClause(second_stmt, current_OpenMPIR_to_SageIII, *citer);
      record_clause_order(added_clause, *citer);
      break;
    }
    case OMPC_parallel: {
      SgOmpClause *added_clause =
          convertSimpleClause(second_stmt, current_OpenMPIR_to_SageIII, *citer);
      record_clause_order(added_clause, *citer);
      break;
    }
    default: {
      cerr << "error: unacceptable clause for combined parallel for directive"
           << endl;
      ROSE_ABORT();
    }
    };
  };
  movePreprocessingInfo(body, first_stmt, PreprocessingInfo::before,
                        PreprocessingInfo::after, true);
  return first_stmt;
}

bool checkOpenMPIR(OpenMPDirective *directive) {

  if (directive == NULL) {
    return false;
  };
  OpenMPDirectiveKind directive_kind = directive->getKind();
  switch (directive_kind) {
  case OMPD_atomic:
  case OMPD_barrier:
  case OMPD_cancel:
  case OMPD_cancellation_point:
  case OMPD_critical:
  case OMPD_declare_mapper:
  case OMPD_declare_simd:
  case OMPD_declare_variant:
  case OMPD_begin_declare_variant:
  case OMPD_end_declare_variant:
  case OMPD_declare_target:
  case OMPD_end_declare_target:
  case OMPD_depobj:
  case OMPD_dispatch:
  case OMPD_distribute:
  case OMPD_do:
  case OMPD_flush:
  case OMPD_allocate:
  case OMPD_for:
  case OMPD_for_simd:
  case OMPD_loop:
  case OMPD_master:
  case OMPD_metadirective:
  case OMPD_masked:
  case OMPD_masked_taskloop:
  case OMPD_masked_taskloop_simd:
  case OMPD_nothing:
  case OMPD_ordered:
  case OMPD_parallel:
  case OMPD_parallel_do:
  case OMPD_parallel_for:
  case OMPD_parallel_for_simd:
  case OMPD_parallel_sections:
  case OMPD_parallel_workshare:
  case OMPD_scan:
  case OMPD_section:
  case OMPD_sections:
  case OMPD_simd:
  case OMPD_single:
  case OMPD_target:
  case OMPD_target_data:
  case OMPD_target_enter_data:
  case OMPD_target_exit_data:
  case OMPD_target_parallel_for:
  case OMPD_target_parallel:
  case OMPD_distribute_simd:
  case OMPD_distribute_parallel_for:
  case OMPD_distribute_parallel_for_simd:
  case OMPD_taskloop_simd:
  case OMPD_target_update:
  case OMPD_requires:
  case OMPD_target_parallel_for_simd:
  case OMPD_target_parallel_loop:
  case OMPD_target_simd:
  case OMPD_target_teams:
  case OMPD_target_teams_distribute:
  case OMPD_target_teams_distribute_simd:
  case OMPD_target_teams_loop:
  case OMPD_target_teams_distribute_parallel_for:
  case OMPD_target_teams_distribute_parallel_for_simd:
  case OMPD_master_taskloop_simd:
  case OMPD_parallel_master_taskloop:
  case OMPD_parallel_master_taskloop_simd:
  case OMPD_teams_distribute:
  case OMPD_teams_distribute_simd:
  case OMPD_teams_distribute_parallel_for:
  case OMPD_teams_distribute_parallel_for_simd:
  case OMPD_teams_loop:
  case OMPD_parallel_master:
  case OMPD_master_taskloop:
  case OMPD_parallel_loop:
  case OMPD_task:
  case OMPD_taskgroup:
  case OMPD_taskloop:
  case OMPD_taskwait:
  case OMPD_taskyield:
  case OMPD_teams:
  case OMPD_threadprivate:
  case OMPD_workshare:
  case OMPD_tile:
  case OMPD_unroll:
  case OMPD_ompx: {
    break;
  }
  default: {
    return false;
  }
  };
  std::map<OpenMPClauseKind, std::vector<OpenMPClause *> *> *clauses =
      directive->getAllClauses();
  if (clauses != NULL) {
    std::map<OpenMPClauseKind, std::vector<OpenMPClause *> *>::iterator it;
    for (it = clauses->begin(); it != clauses->end(); it++) {
      switch (it->first) {
      case OMPC_acq_rel:
      case OMPC_acquire:
      case OMPC_compare:
      case OMPC_aligned:
      case OMPC_allocate:
      case OMPC_allocator:
      case OMPC_bind:
      case OMPC_to:
      case OMPC_from:
      case OMPC_capture:
      case OMPC_collapse:
      case OMPC_copyin:
      case OMPC_copyprivate:
      case OMPC_default:
      case OMPC_defaultmap:
      case OMPC_depend:
      case OMPC_affinity:
      case OMPC_depobj_update:
      case OMPC_destroy:
      case OMPC_detach:
      case OMPC_device_type:
      case OMPC_device:
      case OMPC_dist_schedule:
      case OMPC_exclusive:
      case OMPC_final:
      case OMPC_firstprivate:
      case OMPC_for:
      case OMPC_grainsize:
      case OMPC_hint:
      case OMPC_if:
      case OMPC_has_device_addr:
      case OMPC_in_reduction:
      case OMPC_inbranch:
      case OMPC_inclusive:
      case OMPC_is_device_ptr:
      case OMPC_lastprivate:
      case OMPC_linear:
      case OMPC_map:
      case OMPC_mergeable:
      case OMPC_nogroup:
      case OMPC_nontemporal:
      case OMPC_nocontext:
      case OMPC_notinbranch:
      case OMPC_nowait:
      case OMPC_novariants:
      case OMPC_num_tasks:
      case OMPC_num_teams:
      case OMPC_num_threads:
      case OMPC_order:
      case OMPC_ordered:
      case OMPC_parallel:
      case OMPC_match:
      case OMPC_adjust_args:
      case OMPC_append_args:
      case OMPC_priority:
      case OMPC_private:
      case OMPC_proc_bind:
      case OMPC_read:
      case OMPC_reverse_offload:
      case OMPC_unified_address:
      case OMPC_unified_shared_memory:
      case OMPC_dynamic_allocators:
      case OMPC_atomic_default_mem_order:
      case OMPC_ext_implementation_defined_requirement:
      case OMPC_fail:
      case OMPC_reduction:
      case OMPC_relaxed:
      case OMPC_release:
      case OMPC_safelen:
      case OMPC_schedule:
      case OMPC_sections:
      case OMPC_seq_cst:
      case OMPC_shared:
      case OMPC_simdlen:
      case OMPC_weak:
      case OMPC_task_reduction:
      case OMPC_taskgroup:
      case OMPC_thread_limit:
      case OMPC_uniform:
      case OMPC_untied:
      case OMPC_update:
      case OMPC_use_device_addr:
      case OMPC_use_device_ptr:
      case OMPC_uses_allocators:
      case OMPC_when:
      case OMPC_threads:
      case OMPC_simd:
      case OMPC_write:
      case OMPC_full:
      case OMPC_partial:
      case OMPC_sizes: {
        break;
      }
      default: {
        return false;
      }
      };
    };
  };
  return true;
}
