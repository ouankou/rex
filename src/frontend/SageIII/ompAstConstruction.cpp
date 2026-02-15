// Put here code used to construct SgOmp* nodes
// Liao 10/8/2010
#include "ompAstConstruction.h"

#include "astPostProcessing.h"

#include "rose_paths.h"

#include "sage3basic.h"

#include "sageBuilder.h"

#include <algorithm>
#include <cctype>
#include <memory>
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

class AccFortranEndAttribute : public AstAttribute {
public:
  OwnershipPolicy getOwnershipPolicy() const override {
    return CONTAINER_OWNERSHIP;
  }
  AstAttribute *copy() const override {
    return new AccFortranEndAttribute(*this);
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
  std::unordered_map<const OpenMPClause *,
                     std::vector<std::vector<const OmpParsedExpression *>>>
      map_dist_data_policy_nodes;
};

static std::unordered_map<OpenMPDirective *, OmpClauseParseCache>
    g_omp_clause_nodes;

struct PendingCommentedDirectiveRelocation {
  SgLocatedNode *owner = nullptr;
  PreprocessingInfo *info = nullptr;
};

static std::unordered_map<SgPragmaDeclaration *,
                          std::vector<PendingCommentedDirectiveRelocation>>
    g_pending_commented_directive_relocations;

struct OmpExprParseContext {
  SgPragmaDeclaration *pragma_declaration = nullptr;
  OpenMPDirective *directive = nullptr;
  std::vector<std::unique_ptr<OmpParsedExpression>> owned_nodes;
};

OpenMPDirective *ompparser_OpenMPIR;
static bool use_ompparser = false;
static bool use_accparser = false;

void mergeEndClausesToBeginDirective(OpenMPDirective *begin_decl,
                                     OpenMPDirective *end_decl,
                                     OpenMPDirective *end_wrapper);

using namespace std;
using namespace SageInterface;
using namespace SageBuilder;
using namespace OmpSupport;

namespace {
SgExpression *buildOpaqueOpenMPClauseExpression(SgPragmaDeclaration *directive,
                                                const std::string &text);
SgVariableSymbol *extractClauseVariableSymbol(SgNode *node);
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

void collectCommentedDirectiveRelocations(
    SgSourceFile *source_file,
    const std::list<SgPragmaDeclaration *> &pragma_list) {
  g_pending_commented_directive_relocations.clear();
  if (source_file == nullptr) {
    return;
  }

  struct PragmaPosition {
    SgPragmaDeclaration *declaration = nullptr;
    unsigned line = 0;
  };

  std::unordered_map<int, std::vector<PragmaPosition>> pragma_positions_by_file;
  for (SgPragmaDeclaration *pragma_decl : pragma_list) {
    if (pragma_decl == nullptr) {
      continue;
    }
    const Sg_File_Info *file_info = pragma_decl->get_file_info();
    if (file_info == nullptr) {
      continue;
    }

    const unsigned pragma_line = getLocatedNodeLine(pragma_decl);
    if (pragma_line == 0) {
      continue;
    }

    pragma_positions_by_file[file_info->get_file_id()].push_back(
        PragmaPosition{pragma_decl, pragma_line});
  }

  for (auto &entry : pragma_positions_by_file) {
    std::vector<PragmaPosition> &positions = entry.second;
    std::sort(positions.begin(), positions.end(),
              [](const PragmaPosition &lhs, const PragmaPosition &rhs) {
                return lhs.line < rhs.line;
              });
  }

  if (pragma_positions_by_file.empty()) {
    return;
  }

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

    for (PreprocessingInfo *info : *attached) {
      if (!isCommentedOutDirective(info)) {
        continue;
      }
      if (!seen_comments.insert(info).second) {
        continue;
      }

      const int comment_line = info->getLineNumber();
      const int file_id = info->getFileId();
      if (comment_line <= 0) {
        continue;
      }
      auto pragma_positions_it = pragma_positions_by_file.find(file_id);
      if (pragma_positions_it == pragma_positions_by_file.end()) {
        continue;
      }

      const std::vector<PragmaPosition> &positions =
          pragma_positions_it->second;
      if (positions.empty()) {
        continue;
      }

      auto next_pragma_it =
          std::lower_bound(positions.begin(), positions.end(), comment_line,
                           [](const PragmaPosition &position, int line) {
                             return static_cast<int>(position.line) < line;
                           });

      SgPragmaDeclaration *target_pragma = nullptr;
      if (next_pragma_it != positions.end()) {
        target_pragma = next_pragma_it->declaration;
      } else {
        target_pragma = positions.back().declaration;
      }

      if (target_pragma == nullptr) {
        continue;
      }

      g_pending_commented_directive_relocations[target_pragma].push_back(
          PendingCommentedDirectiveRelocation{owner, info});
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
                     const int lhs_line =
                         lhs.info != nullptr ? lhs.info->getLineNumber() : 0;
                     const int rhs_line =
                         rhs.info != nullptr ? rhs.info->getLineNumber() : 0;
                     return lhs_line < rhs_line;
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
        entry.info->getLineNumber() <= static_cast<int>(pragma_line);
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
      SgVariableSymbol *symbol = var_ref->get_symbol();
      ROSE_ASSERT(symbol != nullptr);
      return SageBuilder::buildVarRefExp(symbol);
    }
    return SageInterface::copyExpression(expr);
  }

  return nullptr;
}

void parseAndStoreVariableList(const std::string &expr_text,
                               OmpParsedExpression *parsed,
                               SgPragmaDeclaration *pragma_declaration,
                               OpenMPDirective *directive,
                               OpenMPClauseKind clause_kind) {
  ROSE_ASSERT(parsed != nullptr);
  clearOpenMPClauseTemporaryState();
  parseOmpVariable(std::make_pair(pragma_declaration, directive), clause_kind,
                   expr_text);
  ROSE_ASSERT(!omp_variable_list.empty());
  parsed->node = omp_variable_list.back().second;
  parsed->dimensions.clear();
  omp_variable_list.clear();
  array_dimensions.clear();
}

void parseAndStoreArraySection(const std::string &expr_text,
                               OmpParsedExpression *parsed,
                               SgPragmaDeclaration *pragma_declaration,
                               OpenMPDirective *directive,
                               OpenMPClauseKind clause_kind) {
  ROSE_ASSERT(parsed != nullptr);
  clearOpenMPClauseTemporaryState();
  parseOmpArraySection(pragma_declaration, clause_kind, expr_text);
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
  omp_variable_list.clear();
  array_dimensions.clear();
}

void parseAndStoreExpression(const std::string &expr_text,
                             OmpParsedExpression *parsed,
                             SgPragmaDeclaration *pragma_declaration,
                             OpenMPDirective *directive,
                             OpenMPClauseKind clause_kind) {
  ROSE_ASSERT(parsed != nullptr);
  clearOpenMPClauseTemporaryState();
  SgExpression *expression =
      parseOmpExpression(pragma_declaration, clause_kind, expr_text);
  ROSE_ASSERT(expression != nullptr);
  parsed->node = expression;
  parsed->dimensions.clear();
  if (SgVariableSymbol *symbol = extractClauseVariableSymbol(expression)) {
    auto found = array_dimensions.find(symbol);
    if (found != array_dimensions.end()) {
      parsed->dimensions = found->second;
    }
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
                              clause_kind);
    }
  } else if (parse_mode == OMP_EXPR_PARSE_variable_list) {
    parseAndStoreVariableList(parsed->text, parsed.get(),
                              context->pragma_declaration, context->directive,
                              clause_kind);
  } else if (parse_mode == OMP_EXPR_PARSE_array_section) {
    parseAndStoreArraySection(parsed->text, parsed.get(),
                              context->pragma_declaration, context->directive,
                              clause_kind);
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
  ROSE_ASSERT(original_clauses->size() == parsed_clauses->size());

  for (size_t index = 0; index < original_clauses->size(); ++index) {
    OpenMPClause *original_clause = (*original_clauses)[index];
    OpenMPClause *parsed_clause = (*parsed_clauses)[index];
    ROSE_ASSERT(original_clause != nullptr);
    ROSE_ASSERT(parsed_clause != nullptr);
    ROSE_ASSERT(original_clause->getKind() == parsed_clause->getKind());

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
        ROSE_ASSERT(mapper_node != nullptr);
        clause_nodes.push_back(mapper_node);
      }
    } else if (original_clause->getKind() == OMPC_from) {
      auto *original_from_clause =
          static_cast<OpenMPFromClause *>(original_clause);
      auto *parsed_from_clause = static_cast<OpenMPFromClause *>(parsed_clause);
      if (!original_from_clause->getMapperIdentifier().empty()) {
        const OmpParsedExpression *mapper_node =
            asParsedExpression(parsed_from_clause->getMapperIdentifierNode());
        ROSE_ASSERT(mapper_node != nullptr);
        clause_nodes.push_back(mapper_node);
      }
    } else if (original_clause->getKind() == OMPC_allocate) {
      auto *original_allocate_clause =
          static_cast<OpenMPAllocateClause *>(original_clause);
      auto *parsed_allocate_clause =
          static_cast<OpenMPAllocateClause *>(parsed_clause);
      if (!original_allocate_clause->getUserDefinedAllocator().empty()) {
        const OmpParsedExpression *allocator_node = asParsedExpression(
            parsed_allocate_clause->getUserDefinedAllocatorNode());
        ROSE_ASSERT(allocator_node != nullptr);
        clause_nodes.push_back(allocator_node);
      }
    }

    parsed_cache.clause_expression_nodes[original_clause] =
        std::move(clause_nodes);

    if (original_clause->getKind() == OMPC_map) {
      auto *parsed_map_clause = static_cast<OpenMPMapClause *>(parsed_clause);
      const auto &dist_data_policies = parsed_map_clause->getDistDataPolicies();
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
  if (found == cache->clause_expression_nodes.end()) {
    return nullptr;
  }
  return &found->second;
}

const std::vector<std::vector<const OmpParsedExpression *>> *
getParsedMapDistDataPolicyNodes(OpenMPDirective *directive,
                                const OpenMPClause *clause) {
  const OmpClauseParseCache *cache = getClauseParseCache(directive);
  if (cache == nullptr || clause == nullptr) {
    return nullptr;
  }
  auto found = cache->map_dist_data_policy_nodes.find(clause);
  if (found == cache->map_dist_data_policy_nodes.end()) {
    return nullptr;
  }
  return &found->second;
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
  omp_variable_list.push_back(std::make_pair(parsed->text, parsed->node));
  if (SgVariableSymbol *symbol = extractClauseVariableSymbol(parsed->node)) {
    if (!parsed->dimensions.empty()) {
      array_dimensions[symbol] = parsed->dimensions;
    }
  }
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
  if (lsrc->get_startOfConstruct()->get_filename() ==
          ldest->get_startOfConstruct()->get_filename() &&
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

  ROSE_ASSERT(lsrc->get_startOfConstruct()->get_filename() ==
              ldest->get_startOfConstruct()->get_filename());
  ROSE_ASSERT(lsrc->get_startOfConstruct()->get_line() ==
              ldest->get_startOfConstruct()->get_line());
  ROSE_ASSERT(lsrc->get_startOfConstruct()->get_col() ==
              ldest->get_startOfConstruct()->get_col());

  ROSE_ASSERT(lsrc->get_startOfConstruct()->get_filename() ==
              ldest->get_file_info()->get_filename());
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

  if (isSgOmpBodyStatement(dest))
    src = isSgOmpBodyStatement(dest)->get_body();

  // same src and dest, no copy is needed
  if (src == dest)
    return true;

  SgLocatedNode *lsrc = isSgLocatedNode(src);
  ROSE_ASSERT(lsrc);
  SgLocatedNode *ldest = isSgLocatedNode(dest);
  ROSE_ASSERT(ldest);
  // ROSE_ASSERT (lsrc->get_file_info()->isTransformation() == false);
  // already the same, no copy is needed
  if (lsrc->get_endOfConstruct()->get_filename() ==
          ldest->get_endOfConstruct()->get_filename() &&
      lsrc->get_endOfConstruct()->get_line() ==
          ldest->get_endOfConstruct()->get_line() &&
      lsrc->get_endOfConstruct()->get_col() ==
          ldest->get_endOfConstruct()->get_col())
    return true;

  Sg_File_Info *copy = new Sg_File_Info(*(lsrc->get_endOfConstruct()));
  ROSE_ASSERT(copy != NULL);

  // delete old start of construct
  Sg_File_Info *old_info = ldest->get_endOfConstruct();
  if (old_info)
    delete (old_info);

  ldest->set_endOfConstruct(copy);
  copy->set_parent(ldest);

  ROSE_ASSERT(lsrc->get_endOfConstruct()->get_filename() ==
              ldest->get_endOfConstruct()->get_filename());
  ROSE_ASSERT(lsrc->get_endOfConstruct()->get_line() ==
              ldest->get_endOfConstruct()->get_line());
  ROSE_ASSERT(lsrc->get_endOfConstruct()->get_col() ==
              ldest->get_endOfConstruct()->get_col());
  ROSE_ASSERT(ldest->get_endOfConstruct() == copy);

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

static std::string toLowerCopy(const std::string &input) {
  std::string result = input;
  std::transform(
      result.begin(), result.end(), result.begin(),
      [](unsigned char ch) { return static_cast<char>(std::tolower(ch)); });
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
  if (startsWithCaseInsensitive(text, "omp")) {
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

static bool allowsImplicitFortranEnd(OpenMPDirectiveKind kind) {
  switch (kind) {
  case OMPD_parallel:
  case OMPD_do:
  case OMPD_parallel_do:
  case OMPD_parallel_loop:
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

  return allowsImplicitFortranEnd(directive->getKind());
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

  if (directive->getRequiresExplicitEnd()) {
    return true;
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
    std::string pragmaString = pragmaDecl->get_pragma()->get_pragma();
    std::string normalized = pragmaString;
    stripFortranDirectiveSentinel(normalized);
    trim(normalized);
    std::istringstream istr(normalized);
    std::string key;
    istr >> key;
    if (toLowerCopy(key) == "omp") {
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

  for (size_t i = 0; i < omp_pragmas.size(); ++i) {
    SgPragmaDeclaration *pragmaDecl = omp_pragmas[i];
    if (prev_continuation && getNextStatement(prev_pragma) != pragmaDecl) {
      cerr << "error: Fortran OpenMP line continuation is not contiguous\n";
      ROSE_ABORT();
    }

    std::string line = pragmaDecl->get_pragma()->get_pragma();
    std::string cleaned = line;
    stripFortranDirectiveSentinel(cleaned);
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

    ompparser_OpenMPIR = parseOpenMP(pending.c_str(), nullptr, nullptr);
    if (ompparser_OpenMPIR == NULL) {
      for (const auto &entry : local_OpenMPIR_list) {
        delete entry.second;
      }
      setLang(Lang_unknown);
      return false;
    }

    if (isFortranPairedDirective(ompparser_OpenMPIR)) {
      pairing_list.push_back(ompparser_OpenMPIR);
    }
    if (ompparser_OpenMPIR->getKind() == OMPD_end) {
      if (pairing_list.empty()) {
        cerr << "error: unmatched OpenMP end directive\n";
        ROSE_ABORT();
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
        if (!allowsImplicitOpenMPEnd(begin_directive)) {
          ROSE_ASSERT(end_directive->getKind() == begin_directive->getKind());
        }
        pairing_list.pop_back();
      }
      if (!matched) {
        cerr << "error: unmatched OpenMP end directive\n";
        ROSE_ABORT();
      }
    }

    SgPragmaDeclaration *primary = pending_pragmas.front();
    local_fortran_paired_pragma_dict[primary] = ompparser_OpenMPIR;
    local_pragma_text_by_ir[ompparser_OpenMPIR] = pending;
    if (!shouldSkipOpenMPDirectiveAstConversion(ompparser_OpenMPIR)) {
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
  bool returnNewExpression = false;
  if (isSgTypeUnknown(clause_expression->get_type())) {
    SgVarRefExpVisitor v;
    v.traverse(clause_expression, preorder);
    std::vector<SgExpression *> expressions = v.get_expressions();
    if (!expressions.empty()) {
      if (expressions.size() == 1) { // create the new expression and return it
        // otherwise, replace the expression and return the original, which is
        // now modified
        returnNewExpression = true;
      }

      bool macroReplaced;
      SgDeclarationStatementPtrList &declarations = global->get_declarations();
      while (!expressions.empty()) {
        macroReplaced = false;
        SgExpression *oldExp = expressions.back();
        for (SgDeclarationStatementPtrList::iterator declIt =
                 declarations.begin();
             declIt != declarations.end() && !macroReplaced; ++declIt) {
          SgDeclarationStatement *declaration = *declIt;
          AttachedPreprocessingInfoType *preprocInfo =
              declaration->getAttachedPreprocessingInfo();
          if (preprocInfo !=
              NULL) { // There is preprocessed info attached to the current node
            for (AttachedPreprocessingInfoType::iterator infoIt =
                     preprocInfo->begin();
                 infoIt != preprocInfo->end() && !macroReplaced; infoIt++) {
              if ((*infoIt)->getTypeOfDirective() ==
                  PreprocessingInfo::CpreprocessorDefineDeclaration) {
                newExp = replace_expression_with_macro_value(
                    (*infoIt)->getString(), oldExp, macroReplaced, clause_type);
              }
            }
          }
        }

        // When a macro is defined in a header without any statement, the
        // preprocessed information is attached to the SgFile
        if (!macroReplaced) {
          SgProject *project = SageInterface::getProject();
          int nFiles = project->numberOfFiles();
          for (int fileIt = 0; fileIt < nFiles && !macroReplaced; fileIt++) {
            SgFile &file = project->get_file(fileIt);
            ROSEAttributesListContainerPtr filePreprocInfo =
                file.get_preprocessorDirectivesAndCommentsList();
            if (filePreprocInfo != NULL) {
              std::map<std::string, ROSEAttributesList *> preprocInfoMap =
                  filePreprocInfo->getList();
              for (std::map<std::string, ROSEAttributesList *>::iterator mapIt =
                       preprocInfoMap.begin();
                   mapIt != preprocInfoMap.end() && !macroReplaced; mapIt++) {
                std::vector<PreprocessingInfo *> preprocInfoList =
                    mapIt->second->getList();
                for (std::vector<PreprocessingInfo *>::iterator infoIt =
                         preprocInfoList.begin();
                     infoIt != preprocInfoList.end() && !macroReplaced;
                     infoIt++) {
                  if ((*infoIt)->getTypeOfDirective() ==
                      PreprocessingInfo::CpreprocessorDefineDeclaration) {
                    newExp = replace_expression_with_macro_value(
                        (*infoIt)->getString(), oldExp, macroReplaced,
                        clause_type);
                  }
                }
              }
            }
          }
        }

        expressions.pop_back();
      }
    } else {
      printf("error in checkOmpExpressionClause(): no expression found in an "
             "expression clause\n");
      ROSE_ABORT();
    }
  }

  return (returnNewExpression ? newExp : clause_expression);
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
  case OMPC_MAP_TYPE_tofrom:
  case OMPC_MAP_TYPE_unspecified: {
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
  default: {
    // printf("error: unacceptable omp construct enum for map operator
    // conversion:%s\n", OmpSupport::toString(at_op).c_str());
    ROSE_ABORT();
    break;
  }
  }
  ROSE_ASSERT(result != SgOmpClause::e_omp_map_unknown);
  return result;
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
  ROSE_ASSERT(stmt_vec.size() > 0);
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

//! This function will Find a (optional) end pragma for an input pragma (decl)
//  and merge clauses from the end pragma to the beginning pragma
//  statements in between will be put into a basic block if there are more than
//  one statements
void merge_Matching_Fortran_Pragma_pairs(SgPragmaDeclaration *decl) {
  SgPragmaDeclaration *end_decl = NULL;
  SgStatement *next_stmt = getNextStatement(decl);
  OpenMPDirectiveKind begin_directive_kind =
      fortran_paired_pragma_dict[decl]->getKind();

  std::vector<SgStatement *>
      affected_stmts; // statements which are inside the begin .. end pair

  // Find possible end directives attached to a pragma declaration
  while (next_stmt != NULL) {
    end_decl = isSgPragmaDeclaration(next_stmt);
    if ((end_decl) &&
        (((OpenMPEndDirective *)fortran_paired_pragma_dict[end_decl])
             ->getPairedDirective()) == fortran_paired_pragma_dict[decl])
      break;
    else
      end_decl = NULL; // MUST reset to NULL if not a match
    affected_stmts.push_back(next_stmt);
    next_stmt = getNextStatement(next_stmt);
  } // end while

  // mandatory end directives for most begin directives, except for two cases:
  // !$omp end do
  // !$omp end parallel do
  if (end_decl == NULL) {
    if ((begin_directive_kind != OMPD_parallel) &&
        (begin_directive_kind != OMPD_do) &&
        (begin_directive_kind != OMPD_parallel_do) &&
        (begin_directive_kind != OMPD_parallel_loop)) {
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
  ensureSingleStmtOrBasicBlock(decl, affected_stmts);

  // SgBasicBlock is not unparsed in Fortran
  //
  // To ensure the unparsed Fortran code is correct for debugging
  // -rose:openmp:ast_only
  //  after converting Fortran comments to Pragmas.
  // x.  We should not tweak the original text for the pragmas.
  // x.  We should not remove the end pragma declaration since SgBasicBlock is
  // not unparsed. In the end , the pragmas don't matter too much, the OpenMPIR
  // attached to them are used to guide translations.
  removeStatement(end_decl);
  // we should save those useless end pragmas to a list
  // and remove them as one of the first steps in OpenMP lowering for Fortran
  // omp_end_pragma_list.push_back(end_decl);
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
    pragma_text_by_ir[ompparser_directive_ir] =
        std::string("#pragma ") + pragma_string;
    SgPragmaDeclaration *p_decl = buildPragmaDeclaration(pragma_string, scope);
    // preserve the original source file info ,TODO complex cases , use real
    // preprocessing info's line information !!
    copyStartFileInfo(loc_node, p_decl);

    if (ompparser_directive_ir->getKind() != OMPD_end) {
      OpenMPIR_list.push_back(std::make_pair(p_decl, ompparser_directive_ir));
      omp_pragma_list.push_back(p_decl);
    }
    fortran_paired_pragma_dict[p_decl] = ompparser_directive_ir;

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
    comments->erase(m_pos);

    // two cases for where to insert the pragma, depending on where the
    // preprocessing info is attached to stmt
    //  1. PreprocessingInfo::before
    //     insert the pragma right before the original Fortran statement
    //  2. PreprocessingInfo::inside
    //      insert it as the last statement within stmt
    PreprocessingInfo::RelativePositionType position =
        info->getRelativePosition();
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
    std::vector<SgNode *>::iterator iter;
    for (iter = all_pragmas.begin(); iter != all_pragmas.end(); iter++) {
      SgPragmaDeclaration *pragmaDeclaration = isSgPragmaDeclaration(*iter);
      ROSE_ASSERT(pragmaDeclaration != NULL);
      string pragmaString = pragmaDeclaration->get_pragma()->get_pragma();
      istringstream istr(pragmaString);
      std::string key;
      istr >> key;
      if (key == "omp" && wantsOpenMP) {
        // parse expression
        // Get the object that ompparser IR.
        ompparser_OpenMPIR =
            parseOpenMP(pragmaString.c_str(), nullptr, nullptr);
        assert(ompparser_OpenMPIR != NULL);
        if (shouldSkipOpenMPDirectiveAstConversion(ompparser_OpenMPIR)) {
          delete ompparser_OpenMPIR;
          ompparser_OpenMPIR = nullptr;
          continue;
        }

        use_ompparser = checkOpenMPIR(ompparser_OpenMPIR);
        assert(use_ompparser == true);
        omp_pragma_list.push_back(pragmaDeclaration);
        OpenMPIR_list.push_back(
            std::make_pair(pragmaDeclaration, ompparser_OpenMPIR));
        std::string parse_text = std::string("#pragma ") + pragmaString;
        if (ompparser_OpenMPIR->getKind() != OMPD_end) {
          g_omp_clause_nodes[ompparser_OpenMPIR] = parseClauseNodesForDirective(
              pragmaDeclaration, ompparser_OpenMPIR, parse_text);
        }
      } else if (key == "acc") {
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
    clearClauseParseCacheForSourceFile(sageFilePtr);
    mark_processed(sageFilePtr);
    return;
  }

  // Build OpenMP AST nodes based on parsing results
  if (!isFortran) {
    collectCommentedDirectiveRelocations(sageFilePtr, omp_pragma_list);
  }

  if (isFortran) {
    if (parsed_fortran_pragmas) {
      convert_Fortran_Pragma_Pairs(sageFilePtr);
    } else {
      convert_Fortran_OMP_Comments_to_Pragmas(
          sageFilePtr); // TODO: need to fix not sure why we still need this
                        // here since Fortran is already parsed before.
    }
    convert_Fortran_ACC_Comments_to_Pragmas(sageFilePtr);
    convert_Fortran_ACC_Pragma_Pairs(sageFilePtr);
  }
  if (SgProject::get_verbose() > 1) {
    printf("Calling convert_OpenMP_pragma_to_AST() \n");
  }
  // We can turn this off to debug the convert_Fortran_OMP_Comments_to_Pragmas()
  OpenMPIRToSageAST(sageFilePtr);

  // stop here if only OpenMP AST construction is requested
  if (sageFilePtr->get_openmp_ast_only()) {
    if (SgProject::get_verbose() > 1) {
      printf("Skipping calls to analyze/lower OpenMP "
             "sageFilePtr->get_openmp_ast_only() = %s \n",
             sageFilePtr->get_openmp_ast_only() ? "true" : "false");
    }
    clearClauseParseCacheForSourceFile(sageFilePtr);
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
    clearClauseParseCacheForSourceFile(sageFilePtr);
    mark_processed(sageFilePtr);
    return;
  }

  lower_omp(sageFilePtr);
  clearClauseParseCacheForSourceFile(sageFilePtr);
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
  default: {
    printf("Unknown directive is found.\n");
  }
  }
  setOneSourcePositionForTransformation(result);
  SgPragmaDeclaration *pdecl = current_OpenMPIR_to_SageIII.first;
  copyStartFileInfo(pdecl, result);
  copyEndFileInfo(pdecl, result);
  if (SgLocatedNode *located_result = isSgLocatedNode(result)) {
    located_result->setTransformation();
    located_result->setOutputInCodeGeneration();
  }

  //! For C/C++ replace OpenMP pragma declaration with an SgOmpxxStatement
  SgScopeStatement *scope = pdecl->get_scope();
  ROSE_ASSERT(scope != NULL);
  relocatePendingCommentedDirectivesForPragma(pdecl, result);
  moveUpPreprocessingInfo(result,
                          pdecl); // keep #ifdef etc attached to the pragma
  replaceStatement(pdecl, result);

  return result;
}

SgStatement *
convertVariantDirective(std::pair<SgPragmaDeclaration *, OpenMPDirective *>
                            current_OpenMPIR_to_SageIII) {
  OpenMPDirectiveKind directive_kind =
      current_OpenMPIR_to_SageIII.second->getKind();
  SgStatement *result = NULL;

  switch (directive_kind) {
  case OMPD_parallel: {
    result = convertVariantBodyDirective(current_OpenMPIR_to_SageIII);
    break;
  }
  default: {
    printf("Unknown directive is found.\n");
  }
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
    sg_clause = new SgOmpNowaitClause();
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
  // directives like parallel and for have a following code block beside the
  // pragma itself.
  SgStatement *body = getOpenMPBlockBody(current_OpenMPIR_to_SageIII);
  removeStatement(body, false);
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
  case OMPD_master: {
    result = new SgOmpMasterStatement(NULL, body);
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
  body->set_parent(result);
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

// Convert an OpenMPIR Declare Target Directive to a ROSE node
SgStatement *convertOmpDeclareTargetDirective(
    std::pair<SgPragmaDeclaration *, OpenMPDirective *>
        current_OpenMPIR_to_SageIII) {
  SgOmpDeclareTargetStatement *result = new SgOmpDeclareTargetStatement();
  result->set_firstNondefiningDeclaration(result);

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
    default:
      convertClause(isSgStatement(result), current_OpenMPIR_to_SageIII,
                    *clause_iter);
    };
  };
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
convertMapClause(SgOmpClauseBodyStatement *clause_body,
                 std::pair<SgPragmaDeclaration *, OpenMPDirective *>
                     current_OpenMPIR_to_SageIII,
                 OpenMPClause *current_omp_clause) {
  SgOmpMapClause *result = NULL;
  OpenMPMapClauseType type = ((OpenMPMapClause *)current_omp_clause)->getType();
  SgOmpClause::omp_map_operator_enum sg_type = toSgOmpClauseMapOperator(type);

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

    const auto &map_policies =
        static_cast<OpenMPMapClause *>(current_omp_clause)
            ->getDistDataPolicies();
    const auto *policy_nodes = getParsedMapDistDataPolicyNodes(
        current_OpenMPIR_to_SageIII.second, current_omp_clause);
    const size_t policy_item_count = map_policies.size();
    for (size_t item_index = 0; item_index < policy_item_count; ++item_index) {
      if (item_index >= omp_variable_list.size()) {
        MLOG_ERROR_C("ompAstConstruction",
                     "Map clause parse node count mismatch with variables\n");
        ROSE_ABORT();
      }
      SgVariableSymbol *mapped_symbol =
          extractClauseVariableSymbol(omp_variable_list[item_index].second);
      if (mapped_symbol == nullptr) {
        MLOG_ERROR_C("ompAstConstruction",
                     "Unable to resolve map symbol for dist_data policy\n");
        ROSE_ABORT();
      }

      std::vector<
          std::pair<SgOmpClause::omp_map_dist_data_enum, SgExpression *>>
          sg_policies;
      const auto &policies_for_item = map_policies[item_index];
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
  ROSE_ASSERT(result != NULL);
  buildVariableList(result);
  explist->set_parent(result);
  result->set_array_dimensions(array_dimensions);
  result->set_dist_data_policies(map_dist_data_policies);

  setOneSourcePositionForTransformation(result);
  SgOmpClause *sg_clause = result;
  clause_body->get_clauses().push_back(sg_clause);
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
    default: {
      convertClause(result, current_OpenMPIR_to_SageIII, *clause_iter);
    }
    };
  };

  return result;
}

SgStatement *
getOpenMPBlockBody(std::pair<SgPragmaDeclaration *, OpenMPDirective *>
                       current_OpenMPIR_to_SageIII) {

  SgStatement *result = NULL;
  result = getNextStatement(current_OpenMPIR_to_SageIII.first);
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

SgOmpOrderClause *
convertOrderClause(SgStatement *directive,
                   std::pair<SgPragmaDeclaration *, OpenMPDirective *>
                       current_OpenMPIR_to_SageIII,
                   OpenMPClause *current_omp_clause) {
  OpenMPOrderClauseKind order_kind =
      ((OpenMPOrderClause *)current_omp_clause)->getOrderClauseKind();
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
  SgOmpOrderClause *result = new SgOmpOrderClause(sg_dv);
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

SgOmpWhenClause *
convertWhenClause(SgOmpClauseBodyStatement *clause_body,
                  std::pair<SgPragmaDeclaration *, OpenMPDirective *>
                      current_OpenMPIR_to_SageIII,
                  OpenMPClause *current_omp_clause) {
  printf("when clause is coming.\n");
  SgStatement *variant_directive = NULL;
  auto *when_clause = static_cast<OpenMPWhenClause *>(current_omp_clause);
  OpenMPDirective *variant_OpenMPIR = when_clause->getVariantDirective();
  if (variant_OpenMPIR) {
    std::pair<SgPragmaDeclaration *, OpenMPDirective *>
        paired_variant_OpenMPIR =
            make_pair(current_OpenMPIR_to_SageIII.first, variant_OpenMPIR);
    variant_directive = convertVariantDirective(paired_variant_OpenMPIR);
  };

  SgExpression *user_condition = NULL;
  std::string user_condition_string =
      when_clause->getUserCondition()->expression;
  if (user_condition_string.size()) {
    user_condition = parseOmpExpression(current_OpenMPIR_to_SageIII.first,
                                        current_omp_clause->getKind(),
                                        user_condition_string.c_str());
  };
  SgExpression *user_condition_score = NULL;
  std::string user_condition_score_string =
      when_clause->getUserCondition()->score;
  if (user_condition_score_string.size()) {
    user_condition_score = parseOmpExpression(
        current_OpenMPIR_to_SageIII.first, current_omp_clause->getKind(),
        user_condition_score_string.c_str());
  };

  SgExpression *device_arch = NULL;
  std::string device_arch_string = when_clause->getArchExpression()->expression;
  if (device_arch_string.size()) {
    device_arch = parseOmpExpression(current_OpenMPIR_to_SageIII.first,
                                     current_omp_clause->getKind(),
                                     device_arch_string.c_str());
  };

  SgExpression *device_isa = NULL;
  std::string device_isa_string = when_clause->getIsaExpression()->expression;
  if (device_isa_string.size()) {
    device_isa = parseOmpExpression(current_OpenMPIR_to_SageIII.first,
                                    current_omp_clause->getKind(),
                                    device_isa_string.c_str());
  };

  SgOmpClause::omp_when_context_kind_enum sg_device_kind =
      SgOmpClause::e_omp_when_context_kind_unknown;
  OpenMPClauseContextKind device_kind = when_clause->getContextKind()->second;
  switch (device_kind) {
  case OMPC_CONTEXT_KIND_host: {
    sg_device_kind = SgOmpClause::e_omp_when_context_kind_host;
    break;
  }
  case OMPC_CONTEXT_KIND_nohost: {
    sg_device_kind = SgOmpClause::e_omp_when_context_kind_nohost;
    break;
  }
  case OMPC_CONTEXT_KIND_any: {
    sg_device_kind = SgOmpClause::e_omp_when_context_kind_any;
    break;
  }
  case OMPC_CONTEXT_KIND_cpu: {
    sg_device_kind = SgOmpClause::e_omp_when_context_kind_cpu;
    break;
  }
  case OMPC_CONTEXT_KIND_gpu: {
    sg_device_kind = SgOmpClause::e_omp_when_context_kind_gpu;
    break;
  }
  case OMPC_CONTEXT_KIND_fpga: {
    sg_device_kind = SgOmpClause::e_omp_when_context_kind_fpga;
    break;
  }
  default: {
    ;
  }
  };
  SgOmpClause::omp_when_context_vendor_enum sg_implementation_vendor =
      SgOmpClause::e_omp_when_context_vendor_unspecified;
  OpenMPClauseContextVendor implementation_vendor =
      when_clause->getImplementationKind()->second;
  switch (implementation_vendor) {
  case OMPC_CONTEXT_VENDOR_amd: {
    sg_implementation_vendor = SgOmpClause::e_omp_when_context_vendor_amd;
    break;
  }
  case OMPC_CONTEXT_VENDOR_arm: {
    sg_implementation_vendor = SgOmpClause::e_omp_when_context_vendor_arm;
    break;
  }
  case OMPC_CONTEXT_VENDOR_bsc: {
    sg_implementation_vendor = SgOmpClause::e_omp_when_context_vendor_bsc;
    break;
  }
  case OMPC_CONTEXT_VENDOR_cray: {
    sg_implementation_vendor = SgOmpClause::e_omp_when_context_vendor_cray;
    break;
  }
  case OMPC_CONTEXT_VENDOR_fujitsu: {
    sg_implementation_vendor = SgOmpClause::e_omp_when_context_vendor_fujitsu;
    break;
  }
  case OMPC_CONTEXT_VENDOR_gnu: {
    sg_implementation_vendor = SgOmpClause::e_omp_when_context_vendor_gnu;
    break;
  }
  case OMPC_CONTEXT_VENDOR_ibm: {
    sg_implementation_vendor = SgOmpClause::e_omp_when_context_vendor_ibm;
    break;
  }
  case OMPC_CONTEXT_VENDOR_intel: {
    sg_implementation_vendor = SgOmpClause::e_omp_when_context_vendor_intel;
    break;
  }
  case OMPC_CONTEXT_VENDOR_llvm: {
    sg_implementation_vendor = SgOmpClause::e_omp_when_context_vendor_llvm;
    break;
  }
  case OMPC_CONTEXT_VENDOR_pgi: {
    sg_implementation_vendor = SgOmpClause::e_omp_when_context_vendor_pgi;
    break;
  }
  case OMPC_CONTEXT_VENDOR_ti: {
    sg_implementation_vendor = SgOmpClause::e_omp_when_context_vendor_ti;
    break;
  }
  case OMPC_CONTEXT_VENDOR_unknown: {
    sg_implementation_vendor = SgOmpClause::e_omp_when_context_vendor_unknown;
    break;
  }
  default: {
    ;
  }
  };

  SgExpression *implementation_user_defined = NULL;
  std::string implementation_user_defined_string =
      when_clause->getImplementationExpression()->expression;
  if (implementation_user_defined_string.size()) {
    implementation_user_defined = parseOmpExpression(
        current_OpenMPIR_to_SageIII.first, current_omp_clause->getKind(),
        implementation_user_defined_string.c_str());
  };

  SgExpression *implementation_extension = NULL;
  std::string implementation_extension_string =
      when_clause->getExtensionExpression()->expression;
  if (implementation_extension_string.size()) {
    implementation_extension = parseOmpExpression(
        current_OpenMPIR_to_SageIII.first, current_omp_clause->getKind(),
        implementation_extension_string.c_str());
  };

  SgOmpWhenClause *result = new SgOmpWhenClause(
      user_condition, user_condition_score, device_arch, device_isa,
      sg_device_kind, sg_implementation_vendor, implementation_user_defined,
      implementation_extension, variant_directive);
  std::vector<std::pair<std::string, OpenMPDirective *>> *construct_directive =
      when_clause->getConstructDirective();
  if (construct_directive->size()) {
    std::list<SgStatement *> sg_construct_directives;
    SgStatement *sg_construct_directive = NULL;
    for (unsigned int i = 0; i < construct_directive->size(); i++) {
      std::pair<SgPragmaDeclaration *, OpenMPDirective *>
          paired_construct_OpenMPIR =
              make_pair(current_OpenMPIR_to_SageIII.first,
                        construct_directive->at(i).second);
      sg_construct_directive =
          convertVariantDirective(paired_construct_OpenMPIR);
      sg_construct_directives.push_back(sg_construct_directive);
    };
    result->set_construct_directives(sg_construct_directives);
  };

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
    if (sg_identifier == SgOmpClause::e_omp_reduction_user_defined_identifier) {
      SgExpression *clause_expression = parseOmpExpression(
          current_OpenMPIR_to_SageIII.first, current_omp_clause->getKind(),
          ((OpenMPReductionClause *)current_omp_clause)
              ->getUserDefinedIdentifier());
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
    if (sg_identifier ==
        SgOmpClause::e_omp_in_reduction_user_defined_identifier) {
      SgExpression *clause_expression = parseOmpExpression(
          current_OpenMPIR_to_SageIII.first, current_omp_clause->getKind(),
          ((OpenMPInReductionClause *)current_omp_clause)
              ->getUserDefinedIdentifier());
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
    if (sg_identifier ==
        SgOmpClause::e_omp_task_reduction_user_defined_identifier) {
      SgExpression *clause_expression = parseOmpExpression(
          current_OpenMPIR_to_SageIII.first, current_omp_clause->getKind(),
          ((OpenMPTaskReductionClause *)current_omp_clause)
              ->getUserDefinedIdentifier());
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
    mapper_identifier = cloneParsedExpressionNodeByText(
        parsed_nodes,
        ((OpenMPToClause *)current_omp_clause)->getMapperIdentifier(),
        OMP_EXPR_PARSE_expression);
    ROSE_ASSERT(mapper_identifier != nullptr);
  }
  result->set_mapper_identifier(mapper_identifier);
  ROSE_ASSERT(result != NULL);
  buildVariableList(result);
  explist->set_parent(result);
  result->set_array_dimensions(array_dimensions);

  setOneSourcePositionForTransformation(result);
  SgOmpClause *sg_clause = result;
  if (current_OpenMPIR_to_SageIII.second->getKind() == OMPD_target_update) {
    ((SgOmpTargetUpdateStatement *)clause_body)
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
    mapper_identifier = cloneParsedExpressionNodeByText(
        parsed_nodes,
        ((OpenMPFromClause *)current_omp_clause)->getMapperIdentifier(),
        OMP_EXPR_PARSE_expression);
    ROSE_ASSERT(mapper_identifier != nullptr);
  }
  result->set_mapper_identifier(mapper_identifier);
  ROSE_ASSERT(result != NULL);
  buildVariableList(result);
  explist->set_parent(result);
  result->set_array_dimensions(array_dimensions);

  setOneSourcePositionForTransformation(result);
  SgOmpClause *sg_clause = result;
  if (current_OpenMPIR_to_SageIII.second->getKind() == OMPD_target_update) {
    ((SgOmpTargetUpdateStatement *)clause_body)
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
    clearOpenMPClauseTemporaryState();
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
    result = new SgOmpGrainsizeClause(grainsize_expression);
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
    result = new SgOmpNumTasksClause(num_tasks_expression);
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

void parseOmpVariable(std::pair<SgPragmaDeclaration *, OpenMPDirective *>
                          current_OpenMPIR_to_SageIII,
                      OpenMPClauseKind clause_kind, std::string expression) {
  // special handling for omp declare simd directive
  // It may have clauses referencing a variable declared in an immediately
  // followed function's parameter list
  bool look_forward = false;
  if (current_OpenMPIR_to_SageIII.second->getKind() == OMPD_declare_simd &&
      (clause_kind == OMPC_linear || clause_kind == OMPC_simdlen ||
       clause_kind == OMPC_aligned || clause_kind == OMPC_uniform)) {
    look_forward = true;
  };
  std::string expr_string = std::string() + "varlist " + expression + "\n";
  parseExpression(current_OpenMPIR_to_SageIII.first, look_forward,
                  expr_string.c_str());
}

SgExpression *parseOmpExpression(SgPragmaDeclaration *directive,
                                 OpenMPClauseKind clause_kind,
                                 std::string expression) {
  // special handling for omp declare simd directive
  // It may have clauses referencing a variable declared in an immediately
  // followed function's parameter list
  bool look_forward = false;
  if (isSgOmpDeclareSimdStatement(directive) &&
      (clause_kind == OMPC_linear || clause_kind == OMPC_simdlen ||
       clause_kind == OMPC_aligned || clause_kind == OMPC_uniform)) {
    look_forward = true;
  };
  std::string expr_string = std::string() + "expr (" + expression + ")\n";
  SgExpression *sg_expression =
      parseExpression(directive, look_forward, expr_string.c_str());

  return sg_expression;
}

SgExpression *parseOmpArraySection(SgPragmaDeclaration *directive,
                                   OpenMPClauseKind clause_kind,
                                   std::string expression) {
  // special handling for omp declare simd directive
  // It may have clauses referencing a variable declared in an immediately
  // followed function's parameter list
  bool look_forward = false;
  if (isSgOmpDeclareSimdStatement(directive) &&
      (clause_kind == OMPC_linear || clause_kind == OMPC_simdlen ||
       clause_kind == OMPC_aligned || clause_kind == OMPC_uniform)) {
    look_forward = true;
  };
  std::string expr_string =
      std::string() + "array_section (" + expression + ")\n";
  SgExpression *sg_expression =
      parseArraySectionExpression(directive, look_forward, expr_string.c_str());

  return sg_expression;
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
  removeStatement(body, false);
  ROSE_ASSERT(body != NULL);

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
    located_second->setTransformation();
    located_second->setOutputInCodeGeneration();
  }
  SgOmpParallelStatement *first_stmt =
      new SgOmpParallelStatement(NULL, second_stmt);
  setOneSourcePositionForTransformation(first_stmt);
  copyStartFileInfo(current_OpenMPIR_to_SageIII.first, first_stmt);
  copyEndFileInfo(current_OpenMPIR_to_SageIII.first, first_stmt);
  first_stmt->setTransformation();
  first_stmt->setOutputInCodeGeneration();
  second_stmt->set_parent(first_stmt);

  OpenMPClauseKind clause_kind;
  std::vector<OpenMPClause *> *clause_vector =
      current_OpenMPIR_to_SageIII.second->getClausesInOriginalOrder();
  std::vector<OpenMPClause *>::iterator citer;
  for (citer = clause_vector->begin(); citer != clause_vector->end(); citer++) {
    clause_kind = (*citer)->getKind();
    switch (clause_kind) {
    case OMPC_collapse:
    case OMPC_ordered:
    case OMPC_if:
    case OMPC_num_threads: {
      if (clause_kind == OMPC_collapse || clause_kind == OMPC_ordered) {
        convertExpressionClause(second_stmt, current_OpenMPIR_to_SageIII,
                                *citer);
      } else {
        convertExpressionClause(isSgOmpClauseBodyStatement(first_stmt),
                                current_OpenMPIR_to_SageIII, *citer);
      };
      break;
    }
    case OMPC_allocate:
    case OMPC_copyin:
    case OMPC_firstprivate:
    case OMPC_lastprivate:
    case OMPC_linear:
    case OMPC_private:
    case OMPC_reduction:
    case OMPC_shared:
    case OMPC_uniform: {
      if (clause_kind == OMPC_shared || clause_kind == OMPC_copyin) {
        convertClause(isSgOmpClauseBodyStatement(first_stmt),
                      current_OpenMPIR_to_SageIII, *citer);
      } else {
        convertClause(second_stmt, current_OpenMPIR_to_SageIII, *citer);
      };
      break;
    }
    case OMPC_default: {
      convertDefaultClause(isSgOmpClauseBodyStatement(first_stmt),
                           current_OpenMPIR_to_SageIII, *citer);
      break;
    }
    case OMPC_proc_bind: {
      convertProcBindClause(isSgOmpClauseBodyStatement(first_stmt),
                            current_OpenMPIR_to_SageIII, *citer);
      break;
    }
    case OMPC_schedule: {
      convertScheduleClause(second_stmt, current_OpenMPIR_to_SageIII, *citer);
      break;
    }
    case OMPC_parallel: {
      convertSimpleClause(second_stmt, current_OpenMPIR_to_SageIII, *citer);
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
  case OMPD_declare_target:
  case OMPD_end_declare_target:
  case OMPD_depobj:
  case OMPD_distribute:
  case OMPD_do:
  case OMPD_flush:
  case OMPD_allocate:
  case OMPD_for:
  case OMPD_for_simd:
  case OMPD_loop:
  case OMPD_master:
  case OMPD_metadirective:
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
  case OMPD_unroll: {
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
      case OMPC_device:
      case OMPC_dist_schedule:
      case OMPC_exclusive:
      case OMPC_final:
      case OMPC_firstprivate:
      case OMPC_for:
      case OMPC_grainsize:
      case OMPC_hint:
      case OMPC_if:
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
      case OMPC_notinbranch:
      case OMPC_nowait:
      case OMPC_num_tasks:
      case OMPC_num_teams:
      case OMPC_num_threads:
      case OMPC_order:
      case OMPC_ordered:
      case OMPC_parallel:
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
      case OMPC_reduction:
      case OMPC_relaxed:
      case OMPC_release:
      case OMPC_safelen:
      case OMPC_schedule:
      case OMPC_sections:
      case OMPC_seq_cst:
      case OMPC_shared:
      case OMPC_simdlen:
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
