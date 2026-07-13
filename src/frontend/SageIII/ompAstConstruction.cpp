// Put here code used to construct SgOmp* nodes
// Liao 10/8/2010
#include "ompAstConstruction.h"

#include "OpenACCParser.h"
#include "accAstConstruction.h"

#include "astJson/sageAstJson.h"
#include "astPostProcessing.h"
#include "openMPConstantInteger.h"

#include "rose_paths.h"

#include "sage3basic.h"

#include "sageBuilder.h"

#include <algorithm>
#include <atomic>
#include <cctype>
#include <cstdlib>
#include <cstring>
#include <functional>
#include <limits>
#include <list>
#include <memory>
#include <mutex>
#include <optional>
#include <set>
#include <sstream>
#include <thread>
#include <tuple>
#include <unordered_map>
#include <unordered_set>

extern void omp_exprparser_clear_context_variable_symbols();
extern void
omp_exprparser_add_context_variable_symbol(SgVariableSymbol *symbol);
extern void omp_exprparser_begin_openacc_cxx_semantic_bindings(
    const OpenACCCxxExactSemanticBindings::ExpressionBindings *bindings);
extern void omp_exprparser_end_openacc_cxx_semantic_bindings();
extern void omp_exprparser_begin_fortran_exact_semantic_bindings(
    const std::vector<OmpFortranExactSemanticBindings::Binding> *bindings,
    const std::vector<OmpExactSubexpressionType> *subexpressions,
    SgType *default_integer_type);
extern void omp_exprparser_end_fortran_exact_semantic_bindings();
extern void omp_exprparser_begin_fortran_typed_scope_semantics(
    SgScopeStatement *scope, SgType *default_integer_type);
extern void omp_exprparser_end_fortran_typed_scope_semantics();
extern void omp_exprparser_require_clean_state();

struct NormalizedDeclareMapperData {
  SgOmpClause::omp_declare_mapper_identifier_enum identifier =
      SgOmpClause::e_omp_declare_mapper_identifier_unspecified;
  bool identifier_is_explicit = false;
  std::string user_defined_identifier;
  std::string mapper_type;
  std::string mapper_variable;
};

namespace OmpSupport {
static NormalizedDeclareMapperData
normalizeDeclareMapperData(OpenMPDeclareMapperDirective *mapper_directive);
static bool isSimpleMapperIdentifier(const std::string &value);
} // namespace OmpSupport

struct OmpParsedExpression final : ompparser::HostSemanticNode {
  OpenMPExprParseMode mode = OMP_EXPR_PARSE_none;
  std::string text;
  SgNode *node = nullptr;
  mutable bool consumed = false;
};

struct OmpClauseParseCache {
  bool directive_owns_source_expression_spelling = false;
  std::vector<std::shared_ptr<OmpParsedExpression>> owned_nodes;
  std::vector<const OmpParsedExpression *> directive_expression_nodes;
  std::vector<std::string> threadprivate_source_expression_texts;
  const OmpParsedExpression *declare_mapper_type_node = nullptr;
  const OmpParsedExpression *declare_mapper_variable_node = nullptr;
  SgDeclarationScope *directive_local_scope = nullptr;
  std::unordered_map<const OpenMPClause *,
                     std::vector<const OmpParsedExpression *>>
      clause_expression_nodes;
  std::unordered_map<const OpenMPClause *,
                     std::vector<const OmpParsedExpression *>>
      clause_auxiliary_expression_nodes;
  std::unordered_map<const OpenMPClause *, std::vector<std::string>>
      clause_source_expression_texts;
  std::unordered_map<const OpenMPClause *, std::vector<std::string>>
      clause_source_auxiliary_expression_texts;
  std::unordered_map<const OpenMPClause *,
                     std::vector<std::vector<OpenMPMapClause::DistDataPolicy>>>
      map_dist_data_policies;
  std::unordered_map<const OpenMPClause *,
                     std::vector<std::vector<const OmpParsedExpression *>>>
      map_dist_data_policy_nodes;
};

struct OmpDirectiveParseCacheTree {
  OmpClauseParseCache root;
  std::unordered_map<OpenMPDirective *, OmpClauseParseCache> nested;
};

static SgDeclarationStatement *
getOmpFunctionDirectiveSymbolDeclaration(SgSymbol *symbol);

class ResolvedOmpFunctionDirectiveTarget {
public:
  enum class Kind { declare_simd, declare_variant };

  ResolvedOmpFunctionDirectiveTarget(Kind kind,
                                     SgDeclarationStatement *declaration,
                                     SgSymbol *symbol,
                                     std::size_t semantic_variant_ordinal)
      : kind_(kind), declaration_(declaration), symbol_(symbol),
        semantic_variant_ordinal_(semantic_variant_ordinal) {
    if (declaration_ == nullptr || symbol_ == nullptr ||
        getOmpFunctionDirectiveSymbolDeclaration(symbol_) != declaration_) {
      std::cerr << "REX_OMP_AST_INVARIANT[function-target-construction]: "
                   "target is not one coherent exact declaration and symbol "
                   "identity\n";
      ROSE_ABORT();
    }
  }

  Kind kind() const { return kind_; }
  SgDeclarationStatement *declaration() const { return declaration_; }
  SgSymbol *symbol() const { return symbol_; }
  std::size_t semanticVariantOrdinal() const {
    return semantic_variant_ordinal_;
  }

private:
  Kind kind_;
  SgDeclarationStatement *declaration_;
  SgSymbol *symbol_;
  std::size_t semantic_variant_ordinal_;
};

struct FortranExactSemanticConsumptionState {
  SgPragmaDeclaration *pragma = nullptr;
  const OmpFortranExactSemanticBindings *bindings = nullptr;
  std::vector<bool> binding_consumed;
  std::vector<bool> expression_consumed;
  std::size_t source_cursor = 0;
  std::vector<OmpFortranExactSemanticBindings::Binding> active_bindings;
  bool expression_active = false;
};

struct CxxOpenACCExactSemanticConsumptionState {
  SgPragmaDeclaration *pragma = nullptr;
  const OpenACCCxxExactSemanticBindings *bindings = nullptr;
  std::size_t binding_index = 0;
  bool expression_active = false;
};

namespace OmpSupport {

namespace {
std::atomic<OpenMPConversionSession *> activeOpenMPConversionSession{nullptr};

template <typename Record> struct PendingOpenMPProducerRecord {
  SgSourceFile *source_file = nullptr;
  Record record;
};

struct PendingOpenMPProducerRecords {
  std::mutex mutex;
  std::unordered_map<
      SgPragmaDeclaration *,
      PendingOpenMPProducerRecord<OpenACCCxxExactSemanticBindings>>
      openacc_cxx_semantic_bindings;
  std::unordered_map<
      SgPragmaDeclaration *,
      PendingOpenMPProducerRecord<OmpFortranExactSemanticBindings>>
      fortran_exact_semantic_bindings;
};

PendingOpenMPProducerRecords &pendingOpenMPProducerRecords() {
  static PendingOpenMPProducerRecords records;
  return records;
}

SgSourceFile *requireOpenMPProducerRecordSource(SgPragmaDeclaration *pragma,
                                                const char *contract) {
  SgSourceFile *source_file =
      pragma != nullptr ? SageInterface::getEnclosingSourceFile(pragma)
                        : nullptr;
  if (source_file == nullptr || contract == nullptr) {
    std::cerr << "REX_OMP_AST_INVARIANT[semantic-handoff-registration]: "
              << (contract != nullptr ? contract : "<null>")
              << " record has no pragma owned by one source file\n";
    ROSE_ABORT();
  }
  return source_file;
}

void requireOpenMPProducerRegistrationSource(SgSourceFile *source_file,
                                             SgPragmaDeclaration *pragma,
                                             const char *contract) {
  SgScopeStatement *scope = pragma != nullptr ? pragma->get_scope() : nullptr;
  SgSourceFile *attached_source =
      pragma != nullptr ? SageInterface::getEnclosingSourceFile(pragma)
                        : nullptr;
  if (source_file == nullptr || pragma == nullptr || contract == nullptr ||
      scope == nullptr || pragma->get_parent() != scope ||
      (attached_source != nullptr && attached_source != source_file)) {
    std::cerr << "REX_OMP_AST_INVARIANT[semantic-handoff-registration]: "
              << (contract != nullptr ? contract : "<null>")
              << " record has no coherent producer-supplied source-file and "
                 "pragma-scope ownership\n";
    ROSE_ABORT();
  }
}

template <typename Record>
void transferOpenMPProducerRecordsForSource(
    std::unordered_map<SgPragmaDeclaration *,
                       PendingOpenMPProducerRecord<Record>> &pending,
    std::unordered_map<SgPragmaDeclaration *, Record> &owned,
    SgSourceFile *source_file, const char *contract) {
  for (auto record = pending.begin(); record != pending.end();) {
    if (record->second.source_file == nullptr) {
      std::cerr << "REX_OMP_AST_INVARIANT[semantic-handoff-transfer]: "
                << contract << " record has no producer-supplied source file\n";
      ROSE_ABORT();
    }
    if (record->second.source_file != source_file) {
      ++record;
      continue;
    }
    if (requireOpenMPProducerRecordSource(record->first, contract) !=
        source_file) {
      std::cerr << "REX_OMP_AST_INVARIANT[semantic-handoff-transfer]: "
                << contract
                << " pragma was not attached to its exact producer source "
                   "file before OpenMP conversion\n";
      ROSE_ABORT();
    }
    auto current = record++;
    if (!owned.emplace(current->first, std::move(current->second.record))
             .second) {
      std::cerr << "REX_OMP_AST_INVARIANT[semantic-handoff-transfer]: "
                << contract << " record was transferred more than once\n";
      ROSE_ABORT();
    }
    pending.erase(current);
  }
}
} // namespace

struct OpenMPConversionSession::Impl {
  SgSourceFile *source_file = nullptr;
  std::unordered_map<SgPragmaDeclaration *, ResolvedOmpFunctionDirectiveTarget>
      function_directive_targets;
  std::unordered_map<SgPragmaDeclaration *, OpenACCCxxExactSemanticBindings>
      openacc_cxx_semantic_bindings;
  std::unordered_map<SgPragmaDeclaration *, OmpFortranExactSemanticBindings>
      fortran_exact_semantic_bindings;
  std::map<SgPragmaDeclaration *, OpenMPDirective *> fortran_paired_pragmas;
  std::map<SgPragmaDeclaration *, SgPragmaDeclaration *>
      fortran_explicit_end_pragmas;
  std::map<SgPragmaDeclaration *, SgPragmaDeclaration *>
      cxx_explicit_end_pragmas;
  std::unordered_map<OpenMPDirective *, OmpClauseParseCache> clause_nodes;
  std::unordered_map<const OpenMPClause *, std::string>
      merged_end_clause_sources;
  std::unordered_map<OpenMPDirective *, std::unique_ptr<OpenMPDirective>>
      directive_owners;
  std::list<SgPragmaDeclaration *> pragmas;
  std::vector<std::pair<SgPragmaDeclaration *, OpenMPDirective *>> directives;
  std::vector<SgNode *> expression_variables;
  std::optional<CxxOpenACCExactSemanticConsumptionState>
      openacc_cxx_semantic_consumption;
  std::optional<FortranExactSemanticConsumptionState>
      openacc_fortran_semantic_consumption;
  std::thread::id owner_thread = std::this_thread::get_id();
};

struct OpenMPConversionSessionAccess {
  static auto &get(OpenMPConversionSession &session) {
    if (session.impl_ == nullptr) {
      std::cerr << "REX_OMP_AST_INVARIANT[conversion-session]: active "
                   "OpenMP conversion session has no state\n";
      ROSE_ABORT();
    }
    return *session.impl_;
  }
};

namespace {
OpenMPConversionSession &requireActiveOpenMPConversionSession() {
  OpenMPConversionSession *session =
      activeOpenMPConversionSession.load(std::memory_order_acquire);
  if (session == nullptr) {
    std::cerr << "REX_OMP_AST_INVARIANT[conversion-session]: OpenMP parser "
                 "state accessed without an active conversion session\n";
    ROSE_ABORT();
  }
  auto &state = OpenMPConversionSessionAccess::get(*session);
  if (state.owner_thread != std::this_thread::get_id()) {
    std::cerr << "REX_OMP_AST_INVARIANT[conversion-session]: active OpenMP "
                 "conversion session accessed from a non-owner thread\n";
    ROSE_ABORT();
  }
  return *session;
}

auto &openMPConversionState() {
  return OpenMPConversionSessionAccess::get(
      requireActiveOpenMPConversionSession());
}
} // namespace

OpenMPConversionSession::OpenMPConversionSession(SgSourceFile *source_file)
    : impl_(std::make_unique<Impl>()) {
  if (source_file == nullptr) {
    std::cerr << "REX_OMP_AST_INVARIANT[conversion-session]: OpenMP "
                 "conversion session has no exact source-file owner\n";
    ROSE_ABORT();
  }
  PendingOpenMPProducerRecords &pending = pendingOpenMPProducerRecords();
  std::lock_guard<std::mutex> guard(pending.mutex);
  OpenMPConversionSession *expected = nullptr;
  if (!activeOpenMPConversionSession.compare_exchange_strong(
          expected, this, std::memory_order_acq_rel,
          std::memory_order_acquire)) {
    std::cerr << "REX_OMP_AST_INVARIANT[conversion-session]: nested or "
                 "concurrent OpenMP conversion session activation\n";
    ROSE_ABORT();
  }
  impl_->source_file = source_file;
  transferOpenMPProducerRecordsForSource(pending.openacc_cxx_semantic_bindings,
                                         impl_->openacc_cxx_semantic_bindings,
                                         source_file,
                                         "C/C++-exact-semantic-binding");
  transferOpenMPProducerRecordsForSource(
      pending.fortran_exact_semantic_bindings,
      impl_->fortran_exact_semantic_bindings, source_file,
      "Fortran-exact-semantic-binding");
}

OpenMPConversionSession::~OpenMPConversionSession() {
  if (impl_ == nullptr || impl_->owner_thread != std::this_thread::get_id()) {
    std::cerr << "REX_OMP_AST_INVARIANT[conversion-session]: OpenMP "
                 "conversion session destroyed by a non-owner thread or "
                 "without state\n";
    ROSE_ABORT();
  }
  omp_exprparser_require_clean_state();
  if (!impl_->fortran_paired_pragmas.empty() ||
      !impl_->function_directive_targets.empty() ||
      !impl_->openacc_cxx_semantic_bindings.empty() ||
      !impl_->fortran_exact_semantic_bindings.empty() ||
      !impl_->fortran_explicit_end_pragmas.empty() ||
      !impl_->cxx_explicit_end_pragmas.empty() ||
      !impl_->clause_nodes.empty() ||
      !impl_->merged_end_clause_sources.empty() ||
      !impl_->directive_owners.empty() || !impl_->pragmas.empty() ||
      !impl_->directives.empty() || !impl_->expression_variables.empty() ||
      impl_->openacc_cxx_semantic_consumption.has_value() ||
      impl_->openacc_fortran_semantic_consumption.has_value()) {
    std::cerr << "REX_OMP_AST_INVARIANT[conversion-session]: OpenMP "
                 "conversion session ended with unreleased parser state\n";
    ROSE_ABORT();
  }
  OpenMPConversionSession *expected = this;
  if (!activeOpenMPConversionSession.compare_exchange_strong(
          expected, nullptr, std::memory_order_acq_rel,
          std::memory_order_acquire)) {
    std::cerr << "REX_OMP_AST_INVARIANT[conversion-session]: active OpenMP "
                 "conversion session identity changed before destruction\n";
    ROSE_ABORT();
  }
}

void requireOpenMPConversionSession() {
  static_cast<void>(requireActiveOpenMPConversionSession());
}

void registerOpenACCCxxExactSemanticBindings(
    SgSourceFile *source_file, SgPragmaDeclaration *pragma,
    OpenACCCxxExactSemanticBindings bindings) {
  requireOpenMPProducerRegistrationSource(
      source_file, pragma, "OpenACC-C/C++-exact-semantic-binding");
  PendingOpenMPProducerRecords &pending = pendingOpenMPProducerRecords();
  std::lock_guard<std::mutex> guard(pending.mutex);
  if (activeOpenMPConversionSession.load(std::memory_order_acquire) !=
      nullptr) {
    std::cerr << "REX_ACC_AST_INVARIANT[semantic-handoff-registration]: "
                 "producer attempted to publish during directive conversion\n";
    ROSE_ABORT();
  }
  if (!pending.openacc_cxx_semantic_bindings
           .emplace(
               pragma,
               PendingOpenMPProducerRecord<OpenACCCxxExactSemanticBindings>{
                   source_file, std::move(bindings)})
           .second) {
    std::cerr << "REX_ACC_AST_INVARIANT[semantic-handoff-registration]: "
                 "pragma has duplicate OpenACC C/C++ exact semantic bindings\n";
    ROSE_ABORT();
  }
}

void registerOpenMPFortranExactSemanticBindings(
    SgSourceFile *source_file, SgPragmaDeclaration *pragma,
    OmpFortranExactSemanticBindings bindings) {
  requireOpenMPProducerRegistrationSource(source_file, pragma,
                                          "Fortran-exact-semantic-binding");
  if (bindings.directiveSource().empty()) {
    std::cerr << "REX_OMP_AST_INVARIANT[semantic-handoff-registration]: "
                 "Fortran binding record has no directive source\n";
    ROSE_ABORT();
  }
  PendingOpenMPProducerRecords &pending = pendingOpenMPProducerRecords();
  std::lock_guard<std::mutex> guard(pending.mutex);
  if (activeOpenMPConversionSession.load(std::memory_order_acquire) !=
      nullptr) {
    std::cerr << "REX_OMP_AST_INVARIANT[semantic-handoff-registration]: "
                 "producer attempted to publish during OpenMP conversion\n";
    ROSE_ABORT();
  }
  if (!pending.fortran_exact_semantic_bindings
           .emplace(
               pragma,
               PendingOpenMPProducerRecord<OmpFortranExactSemanticBindings>{
                   source_file, std::move(bindings)})
           .second) {
    std::cerr << "REX_OMP_AST_INVARIANT[semantic-handoff-registration]: "
                 "pragma has duplicate Fortran exact semantic bindings\n";
    ROSE_ABORT();
  }
}

OpenMPProducerSemanticRecords
snapshotOpenMPProducerSemanticRecords(SgPragmaDeclaration *pragma) {
  static_cast<void>(
      requireOpenMPProducerRecordSource(pragma, "semantic-record-snapshot"));
  PendingOpenMPProducerRecords &pending = pendingOpenMPProducerRecords();
  std::lock_guard<std::mutex> guard(pending.mutex);
  OpenMPProducerSemanticRecords result;
  if (auto found = pending.openacc_cxx_semantic_bindings.find(pragma);
      found != pending.openacc_cxx_semantic_bindings.end()) {
    if (found->second.source_file !=
        requireOpenMPProducerRecordSource(pragma,
                                          "C/C++-exact-semantic-binding")) {
      std::cerr << "REX_OMP_AST_INVARIANT[semantic-handoff-snapshot]: "
                   "C/C++ binding source ownership changed\n";
      ROSE_ABORT();
    }
    result.openacc_cxx_semantic_bindings.emplace(found->second.record);
  }
  if (auto found = pending.fortran_exact_semantic_bindings.find(pragma);
      found != pending.fortran_exact_semantic_bindings.end()) {
    if (found->second.source_file !=
        requireOpenMPProducerRecordSource(pragma,
                                          "Fortran-exact-semantic-binding")) {
      std::cerr << "REX_OMP_AST_INVARIANT[semantic-handoff-snapshot]: "
                   "Fortran binding source ownership changed\n";
      ROSE_ABORT();
    }
    result.fortran_exact_semantic_bindings.emplace(found->second.record);
  }
  if (!result.empty() && activeOpenMPConversionSession.load(
                             std::memory_order_acquire) != nullptr) {
    std::cerr << "REX_OMP_AST_INVARIANT[semantic-handoff-snapshot]: "
                 "live producer records cannot be serialized during OpenMP "
                 "conversion\n";
    ROSE_ABORT();
  }
  return result;
}

void registerOpenMPProducerSemanticRecords(
    SgSourceFile *source_file, SgPragmaDeclaration *pragma,
    OpenMPProducerSemanticRecords records) {
  requireOpenMPProducerRegistrationSource(source_file, pragma,
                                          "serialized-semantic-records");
  if (records.empty()) {
    std::cerr << "REX_OMP_AST_INVARIANT[semantic-handoff-registration]: "
                 "serialized producer record set is empty\n";
    ROSE_ABORT();
  }
  if (records.fortran_exact_semantic_bindings.has_value() &&
      records.fortran_exact_semantic_bindings->directiveSource().empty()) {
    std::cerr << "REX_OMP_AST_INVARIANT[semantic-handoff-registration]: "
                 "serialized Fortran binding record has no directive source\n";
    ROSE_ABORT();
  }

  PendingOpenMPProducerRecords &pending = pendingOpenMPProducerRecords();
  std::lock_guard<std::mutex> guard(pending.mutex);
  if (activeOpenMPConversionSession.load(std::memory_order_acquire) !=
      nullptr) {
    std::cerr << "REX_OMP_AST_INVARIANT[semantic-handoff-registration]: "
                 "serialized producer records were published during OpenMP "
                 "conversion\n";
    ROSE_ABORT();
  }
  if ((records.openacc_cxx_semantic_bindings.has_value() &&
       pending.openacc_cxx_semantic_bindings.count(pragma) != 0) ||
      (records.fortran_exact_semantic_bindings.has_value() &&
       pending.fortran_exact_semantic_bindings.count(pragma) != 0)) {
    std::cerr << "REX_OMP_AST_INVARIANT[semantic-handoff-registration]: "
                 "serialized pragma duplicates an invocation-owned producer "
                 "record\n";
    ROSE_ABORT();
  }
  if (records.openacc_cxx_semantic_bindings.has_value()) {
    pending.openacc_cxx_semantic_bindings.emplace(
        pragma,
        PendingOpenMPProducerRecord<OpenACCCxxExactSemanticBindings>{
            source_file, std::move(*records.openacc_cxx_semantic_bindings)});
  }
  if (records.fortran_exact_semantic_bindings.has_value()) {
    pending.fortran_exact_semantic_bindings.emplace(
        pragma,
        PendingOpenMPProducerRecord<OmpFortranExactSemanticBindings>{
            source_file, std::move(*records.fortran_exact_semantic_bindings)});
  }
}

void discardOpenMPProducerSemanticRecords(SgSourceFile *source_file) {
  if (source_file == nullptr) {
    std::cerr << "REX_OMP_AST_INVARIANT[semantic-handoff-discard]: "
                 "producer-record discard has no exact source file\n";
    ROSE_ABORT();
  }
  PendingOpenMPProducerRecords &pending = pendingOpenMPProducerRecords();
  std::lock_guard<std::mutex> guard(pending.mutex);
  auto has_records_for_source = [&](const auto &records, const char *) {
    return std::any_of(records.begin(), records.end(), [&](const auto &record) {
      return record.second.source_file == source_file;
    });
  };
  const bool has_records =
      has_records_for_source(pending.openacc_cxx_semantic_bindings,
                             "C/C++-exact-semantic-binding") ||
      has_records_for_source(pending.fortran_exact_semantic_bindings,
                             "Fortran-exact-semantic-binding");
  if (has_records && activeOpenMPConversionSession.load(
                         std::memory_order_acquire) != nullptr) {
    std::cerr << "REX_OMP_AST_INVARIANT[semantic-handoff-discard]: "
                 "live producer records cannot be discarded during OpenMP "
                 "conversion\n";
    ROSE_ABORT();
  }
  auto discard_for_source = [&](auto &records, const char *contract) {
    for (auto record = records.begin(); record != records.end();) {
      if (record->second.source_file == source_file) {
        if (requireOpenMPProducerRecordSource(record->first, contract) !=
            source_file) {
          std::cerr << "REX_OMP_AST_INVARIANT[semantic-handoff-discard]: "
                    << contract << " source ownership changed\n";
          ROSE_ABORT();
        }
        record = records.erase(record);
      } else {
        ++record;
      }
    }
  };
  discard_for_source(pending.openacc_cxx_semantic_bindings,
                     "C/C++-exact-semantic-binding");
  discard_for_source(pending.fortran_exact_semantic_bindings,
                     "Fortran-exact-semantic-binding");
}

std::vector<SgNode *> &openMPExpressionVariables() {
  return openMPConversionState().expression_variables;
}

void markOpenMPMergedEndClause(OpenMPClause *clause,
                               const std::string &source_text) {
  if (clause == nullptr || source_text.empty() ||
      !openMPConversionState()
           .merged_end_clause_sources.emplace(clause, source_text)
           .second) {
    std::cerr << "REX_OMP_AST_INVARIANT[end-clause-provenance]: cannot "
                 "register a null, empty, or duplicate merged END clause\n";
    ROSE_ABORT();
  }
}

bool isOpenMPMergedEndClause(const OpenMPClause *clause) {
  if (clause == nullptr) {
    std::cerr << "REX_OMP_AST_INVARIANT[end-clause-provenance]: cannot query "
                 "a null clause\n";
    ROSE_ABORT();
  }
  return openMPConversionState().merged_end_clause_sources.count(clause) != 0;
}

const std::string &getOpenMPMergedEndClauseSource(const OpenMPClause *clause) {
  if (clause == nullptr) {
    std::cerr << "REX_OMP_AST_INVARIANT[end-clause-provenance]: cannot query "
                 "a null clause\n";
    ROSE_ABORT();
  }
  auto found = openMPConversionState().merged_end_clause_sources.find(clause);
  if (found == openMPConversionState().merged_end_clause_sources.end() ||
      found->second.empty()) {
    std::cerr << "REX_OMP_AST_INVARIANT[end-clause-provenance]: clause has no "
                 "registered merged END source\n";
    ROSE_ABORT();
  }
  return found->second;
}

static std::map<SgPragmaDeclaration *, OpenMPDirective *> &
openMPFortranPairedPragmas() {
  return openMPConversionState().fortran_paired_pragmas;
}

static std::map<SgPragmaDeclaration *, SgPragmaDeclaration *> &
openMPFortranExplicitEndPragmas() {
  return openMPConversionState().fortran_explicit_end_pragmas;
}

static std::map<SgPragmaDeclaration *, SgPragmaDeclaration *> &
openMPCxxExplicitEndPragmas() {
  return openMPConversionState().cxx_explicit_end_pragmas;
}

static std::unordered_map<OpenMPDirective *, OmpClauseParseCache> &
openMPClauseNodes() {
  return openMPConversionState().clause_nodes;
}

static std::unordered_map<OpenMPDirective *, std::unique_ptr<OpenMPDirective>> &
openMPDirectiveOwners() {
  return openMPConversionState().directive_owners;
}

static std::list<SgPragmaDeclaration *> &openMPPragmas() {
  return openMPConversionState().pragmas;
}

static std::vector<std::pair<SgPragmaDeclaration *, OpenMPDirective *>> &
openMPDirectives() {
  return openMPConversionState().directives;
}

static std::unordered_map<SgPragmaDeclaration *,
                          ResolvedOmpFunctionDirectiveTarget> &
openMPFunctionDirectiveTargets() {
  return openMPConversionState().function_directive_targets;
}

static std::unordered_map<SgPragmaDeclaration *,
                          OpenACCCxxExactSemanticBindings> &
openACCCxxExactSemanticBindings() {
  return openMPConversionState().openacc_cxx_semantic_bindings;
}

static std::unordered_map<SgPragmaDeclaration *,
                          OmpFortranExactSemanticBindings> &
openMPFortranExactSemanticBindings() {
  return openMPConversionState().fortran_exact_semantic_bindings;
}

} // namespace OmpSupport

OpenMPDirective *
retainOpenMPDirective(std::unique_ptr<OpenMPDirective> directive,
                      const char *context) {
  if (directive == nullptr || context == nullptr) {
    std::cerr << "REX_OMP_AST_INVARIANT[directive-ownership]: context="
              << (context != nullptr ? context : "<null>")
              << " cannot retain a null parsed directive\n";
    ROSE_ABORT();
  }
  OpenMPDirective *raw = directive.get();
  const auto inserted =
      OmpSupport::openMPDirectiveOwners().emplace(raw, std::move(directive));
  if (!inserted.second) {
    std::cerr << "REX_OMP_AST_INVARIANT[directive-ownership]: context="
              << context << " attempted to retain a directive twice\n";
    ROSE_ABORT();
  }
  return raw;
}

void releaseOpenMPDirective(OpenMPDirective *directive) {
  if (directive == nullptr) {
    std::cerr << "REX_OMP_AST_INVARIANT[directive-ownership]: cannot release "
                 "a null parsed directive\n";
    ROSE_ABORT();
  }
  const auto owner = OmpSupport::openMPDirectiveOwners().find(directive);
  if (owner == OmpSupport::openMPDirectiveOwners().end()) {
    std::cerr << "REX_OMP_AST_INVARIANT[directive-ownership]: persistent "
                 "directive has no unique owner\n";
    ROSE_ABORT();
  }
  OmpSupport::openMPDirectiveOwners().erase(owner);
}

struct OmpExprParseContext {
  SgPragmaDeclaration *pragma_declaration = nullptr;
  OpenMPDirective *directive = nullptr;
  bool capture_source_text_only = false;
  FortranExactSemanticConsumptionState fortran_semantics;
  SgDeclarationScope *directive_local_scope = nullptr;
  SgType *declare_mapper_type = nullptr;
  SgType *pending_iterator_type = nullptr;
  std::vector<std::shared_ptr<OmpParsedExpression>> owned_nodes;
};

namespace {
std::shared_ptr<const ompparser::HostSemanticNode>
parseOpenMPHostFragment(const ompparser::HostFragment &fragment,
                        OmpExprParseContext *context);
}

class RexOpenMPHostLanguageHooks final : public ompparser::HostLanguageHooks {
public:
  explicit RexOpenMPHostLanguageHooks(OmpExprParseContext *context)
      : context_(context) {
    ROSE_ASSERT(context_ != nullptr);
  }

  std::shared_ptr<const ompparser::HostSemanticNode>
  parse(const ompparser::HostFragment &fragment,
        std::vector<ompparser::Diagnostic> &) const override {
    return parseOpenMPHostFragment(fragment, context_);
  }

  void validate(const OpenMPDirective &,
                std::vector<ompparser::Diagnostic> &) const override {}

private:
  OmpExprParseContext *context_ = nullptr;
};

SgDeclarationScope *
ensureOpenMPDirectiveLocalScope(OmpExprParseContext *context) {
  if (context == nullptr || context->pragma_declaration == nullptr) {
    std::cerr << "REX_OMP_AST_INVARIANT[directive-local-scope]: expression "
                 "callback has no exact pragma owner\n";
    ROSE_ABORT();
  }
  if (context->directive_local_scope != nullptr) {
    return context->directive_local_scope;
  }
  SgScopeStatement *semantic_scope = context->pragma_declaration->get_scope();
  if (semantic_scope == nullptr) {
    std::cerr << "REX_OMP_AST_INVARIANT[directive-local-scope]: pragma has no "
                 "exact semantic scope\n";
    ROSE_ABORT();
  }
  context->directive_local_scope = SageBuilder::buildDeclarationScope();
  if (context->directive_local_scope == nullptr) {
    std::cerr << "REX_OMP_AST_INVARIANT[directive-local-scope]: declaration "
                 "scope construction failed\n";
    ROSE_ABORT();
  }
  context->directive_local_scope->setCaseInsensitive(
      semantic_scope->isCaseInsensitive());
  SageBuilder::attachSemanticDeclarationScope(semantic_scope,
                                              context->directive_local_scope);
  return context->directive_local_scope;
}

SgVariableSymbol *
publishOpenMPDirectiveLocalVariable(OmpExprParseContext *context,
                                    const std::string &name, SgType *type,
                                    const char *contract) {
  if (context == nullptr || contract == nullptr ||
      !OmpSupport::isSimpleMapperIdentifier(name) || type == nullptr ||
      isSgTypeUnknown(type) != nullptr || isSgTypeDefault(type) != nullptr) {
    std::cerr << "REX_OMP_AST_INVARIANT[directive-local-variable]: contract="
              << (contract != nullptr ? contract : "<null>")
              << " has no exact name and type\n";
    ROSE_ABORT();
  }
  SgDeclarationScope *local_scope = ensureOpenMPDirectiveLocalScope(context);
  if (local_scope->lookup_variable_symbol(name) != nullptr) {
    std::cerr << "REX_OMP_AST_INVARIANT[directive-local-variable]: contract="
              << contract << " duplicates local '" << name << "'\n";
    ROSE_ABORT();
  }
  SgVariableDeclaration *declaration =
      SageBuilder::buildSemanticAuxiliaryVariableDeclaration(
          name, type, nullptr, local_scope);
  SgAuxiliaryDeclarationList *auxiliary =
      local_scope->get_auxiliary_declarations();
  SgVariableSymbol *symbol = local_scope->lookup_variable_symbol(name);
  if (declaration == nullptr || auxiliary == nullptr ||
      auxiliary->get_parent() != local_scope ||
      declaration->get_parent() != auxiliary ||
      declaration->get_scope() != local_scope ||
      std::count(auxiliary->get_declarations().begin(),
                 auxiliary->get_declarations().end(), declaration) != 1 ||
      local_scope->statementExistsInScope(declaration) || symbol == nullptr ||
      symbol->get_declaration() == nullptr ||
      symbol->get_declaration()->get_type() != type) {
    std::cerr << "REX_OMP_AST_INVARIANT[directive-local-variable]: contract="
              << contract << " local='" << name
              << "' has no exact semantic declaration and symbol owner\n";
    ROSE_ABORT();
  }
  return symbol;
}

ompparser::ParseOptions makeOpenMPParseOptions(
    OpenMPBaseLang baseLanguage,
    const ompparser::HostLanguageHooks *hostHooks = nullptr) {
  if (baseLanguage != Lang_C && baseLanguage != Lang_Cplusplus &&
      baseLanguage != Lang_Fortran) {
    std::cerr << "REX_OMP_AST_INVARIANT[parse-options]: OpenMP parse requires "
                 "an exact C, C++, or Fortran base language\n";
    ROSE_ABORT();
  }
  ompparser::ParseOptions options;
  switch (baseLanguage) {
  case Lang_C:
    options.language = ompparser::BaseLanguage::C;
    break;
  case Lang_Cplusplus:
    options.language = ompparser::BaseLanguage::CXX;
    break;
  case Lang_Fortran:
    options.language = ompparser::BaseLanguage::Fortran;
    break;
  case Lang_unknown:
    ROSE_ABORT();
  }
  options.extensions = ompparser::ExtensionPolicy::AllowRegistered;
  options.host_hooks = hostHooks;
  return options;
}

std::unique_ptr<OpenMPDirective>
parseOpenMPDirectiveOrAbort(const std::string &text,
                            const ompparser::ParseOptions &options) {
  ompparser::ParseResult result = ompparser::parseDirective(text, options);
  if (!result.success() || result.directive == nullptr ||
      (options.host_hooks != nullptr && !result.context_checks_complete)) {
    std::cerr << "REX_OMP_AST_INVARIANT[parser]: OpenMP parser rejected '"
              << text << "'\n";
    for (const ompparser::Diagnostic &diagnostic : result.diagnostics) {
      std::cerr << "  " << diagnostic.range.begin.line << ":"
                << diagnostic.range.begin.column << ": " << diagnostic.message
                << "\n";
    }
    ROSE_ABORT();
  }
  return std::move(result.directive);
}

void mergeEndClausesToBeginDirective(OpenMPDirective *begin_decl,
                                     OpenMPDirective *end_decl,
                                     OpenMPDirective *end_wrapper,
                                     const std::string &end_source_text);

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
SgOmpNameExpression *buildOpenMPNameExpression(const std::string &text);
SgExpression *buildOpenMPSyntaxTokenExpression(const std::string &text,
                                               OpenMPBaseLang base_lang);
SgVariableSymbol *extractClauseVariableSymbol(SgNode *node);
std::string trimWhitespaceCopy(const std::string &value);

void requireExactOpenMPFileInfoCopy(const Sg_File_Info *source,
                                    const Sg_File_Info *copy,
                                    const SgNode *source_owner,
                                    const SgNode *copy_owner,
                                    const char *position_role) {
  if (source == nullptr || copy == nullptr || source_owner == nullptr ||
      copy_owner == nullptr || position_role == nullptr ||
      source->get_parent() != source_owner ||
      copy->get_parent() != copy_owner || source == copy ||
      source->get_file_id() != copy->get_file_id() ||
      source->get_line() != copy->get_line() ||
      source->get_col() != copy->get_col() ||
      source->get_classificationBitField() !=
          copy->get_classificationBitField() ||
      source->get_physical_file_id() != copy->get_physical_file_id() ||
      source->get_physical_line() != copy->get_physical_line() ||
      source->isOutputInCodeGeneration() != copy->isOutputInCodeGeneration()) {
    std::cerr << "REX_OMP_AST_INVARIANT[expression-copy-source]: "
              << (position_role != nullptr ? position_role : "<null>")
              << " did not preserve one exact source identity\n";
    ROSE_ABORT();
  }
}

SgExpression *copyOpenMPExpressionWithExactProvenance(SgExpression *source) {
  if (source == nullptr) {
    std::cerr << "REX_OMP_AST_INVARIANT[expression-copy-source]: cannot copy "
                 "a null OpenMP expression\n";
    ROSE_ABORT();
  }

  SgTreeCopy copy_help;
  SgExpression *copy = isSgExpression(source->copy(copy_help));
  if (copy == nullptr || copy->get_parent() != nullptr) {
    std::cerr << "REX_OMP_AST_INVARIANT[expression-copy-source]: expression "
                 "copy has no exact detached root\n";
    ROSE_ABORT();
  }

  for (const auto &entry : copy_help.get_copiedNodeMap()) {
    const SgNode *source_node = entry.first;
    SgNode *copy_node = entry.second;
    const SgLocatedNode *source_located =
        isSgLocatedNode(const_cast<SgNode *>(source_node));
    SgLocatedNode *copy_located = isSgLocatedNode(copy_node);
    if ((source_located == nullptr) != (copy_located == nullptr)) {
      std::cerr << "REX_OMP_AST_INVARIANT[expression-copy-source]: copied "
                   "node changed its located-node role\n";
      ROSE_ABORT();
    }
    if (source_located == nullptr) {
      continue;
    }

    requireExactOpenMPFileInfoCopy(source_located->get_file_info(),
                                   copy_located->get_file_info(), source_node,
                                   copy_node, "located primary position");
    requireExactOpenMPFileInfoCopy(source_located->get_startOfConstruct(),
                                   copy_located->get_startOfConstruct(),
                                   source_node, copy_node,
                                   "located start position");
    requireExactOpenMPFileInfoCopy(source_located->get_endOfConstruct(),
                                   copy_located->get_endOfConstruct(),
                                   source_node, copy_node,
                                   "located end position");

    const SgExpression *source_expression =
        isSgExpression(const_cast<SgNode *>(source_node));
    SgExpression *copy_expression = isSgExpression(copy_node);
    if ((source_expression == nullptr) != (copy_expression == nullptr)) {
      std::cerr << "REX_OMP_AST_INVARIANT[expression-copy-source]: copied "
                   "node changed its expression role\n";
      ROSE_ABORT();
    }
    if (source_expression != nullptr) {
      requireExactOpenMPFileInfoCopy(source_expression->get_operatorPosition(),
                                     copy_expression->get_operatorPosition(),
                                     source_node, copy_node,
                                     "expression operator position");
    }
  }
  return copy;
}

SgOmpClause::omp_directive_kind_enum
convertDirectiveKind(OpenMPDirectiveKind kind);
struct OmpListItemIdentity {
  SgVariableSymbol *root = nullptr;
  std::vector<SgVariableSymbol *> member_path;
};

bool buildOmpListItemIdentity(SgNode *node, OmpListItemIdentity &identity);
bool ompListItemIdentitiesConflict(const OmpListItemIdentity &lhs,
                                   const OmpListItemIdentity &rhs);

class OpenACCParseSession {
public:
  struct Entry {
    SgPragmaDeclaration *pragma;
    openacc::Directive directive;
  };

  explicit OpenACCParseSession(SgSourceFile *sourceFile)
      : sourceFile_(sourceFile) {
    if (sourceFile_ == nullptr) {
      std::cerr << "REX_ACC_AST_INVARIANT[parse-session]: null source file\n";
      ROSE_ABORT();
    }
  }

  OpenACCParseSession(const OpenACCParseSession &) = delete;
  OpenACCParseSession &operator=(const OpenACCParseSession &) = delete;
  OpenACCParseSession(OpenACCParseSession &&) = default;
  OpenACCParseSession &operator=(OpenACCParseSession &&) = default;

  void add(SgPragmaDeclaration *pragma, openacc::Directive &&directive) {
    if (pragma == nullptr || getEnclosingSourceFile(pragma) != sourceFile_) {
      std::cerr << "REX_ACC_AST_INVARIANT[parse-session]: directive pragma "
                   "does not belong to the session source file\n";
      ROSE_ABORT();
    }
    if (indices_.find(pragma) != indices_.end()) {
      std::cerr << "REX_ACC_AST_INVARIANT[parse-session]: duplicate pragma "
                   "ownership in one OpenACC parse session\n";
      ROSE_ABORT();
    }
    const std::size_t index = entries_.size();
    entries_.push_back({pragma, std::move(directive)});
    indices_.emplace(pragma, index);
  }

  const openacc::Directive *find(SgPragmaDeclaration *pragma) const {
    const auto found = indices_.find(pragma);
    if (found == indices_.end()) {
      return nullptr;
    }
    if (found->second >= entries_.size() ||
        entries_[found->second].pragma != pragma) {
      std::cerr << "REX_ACC_AST_INVARIANT[parse-session]: corrupt OpenACC "
                   "pragma index\n";
      ROSE_ABORT();
    }
    return &entries_[found->second].directive;
  }

  bool hasConvertibleDirectives() const {
    return std::any_of(
        entries_.begin(), entries_.end(), [](const Entry &entry) {
          return entry.directive.kind() != openacc::DirectiveKind::End;
        });
  }

  void markEndConsumed(SgPragmaDeclaration *pragma) {
    const openacc::Directive *directive = find(pragma);
    if (directive == nullptr ||
        directive->kind() != openacc::DirectiveKind::End) {
      std::cerr << "REX_ACC_AST_INVARIANT[paired-end]: cannot consume a "
                   "non-end OpenACC pragma\n";
      ROSE_ABORT();
    }
    if (!consumedEnds_.insert(pragma).second) {
      std::cerr << "REX_ACC_AST_INVARIANT[paired-end]: OpenACC end marker was "
                   "consumed more than once\n";
      ROSE_ABORT();
    }
    OmpSupport::consumeOpenACCFortranExactSemanticEnd(pragma);
  }

  void requireAllEndsConsumed() const {
    for (const Entry &entry : entries_) {
      if (entry.directive.kind() == openacc::DirectiveKind::End &&
          consumedEnds_.find(entry.pragma) == consumedEnds_.end()) {
        std::cerr << "REX_ACC_AST_INVARIANT[paired-end]: unmatched Fortran "
                     "OpenACC end marker\n";
        ROSE_ABORT();
      }
    }
  }

private:
  SgSourceFile *sourceFile_;
  std::vector<Entry> entries_;
  std::unordered_map<SgPragmaDeclaration *, std::size_t> indices_;
  std::unordered_set<SgPragmaDeclaration *> consumedEnds_;
};

openacc::ParseOptions getOpenACCParseOptions(SgSourceFile *sourceFile) {
  if (sourceFile == nullptr) {
    std::cerr << "REX_ACC_AST_INVARIANT[parse-options]: null source file\n";
    ROSE_ABORT();
  }

  const bool isFortran =
      sourceFile->get_Fortran_only() || sourceFile->get_F77_only() ||
      sourceFile->get_F90_only() || sourceFile->get_F95_only() ||
      sourceFile->get_F2003_only();
  if (isFortran) {
    switch (sourceFile->get_inputFormat()) {
    case SgFile::e_fixed_form_output_format:
      return {openacc::Language::Fortran, openacc::InputForm::FortranFixed};
    case SgFile::e_free_form_output_format:
      return {openacc::Language::Fortran, openacc::InputForm::FortranFree};
    case SgFile::e_unknown_output_format:
      std::cerr << "REX_ACC_AST_INVARIANT[parse-options]: Fortran OpenACC "
                   "source has no exact fixed/free input format\n";
      ROSE_ABORT();
    }
    std::cerr << "REX_ACC_AST_INVARIANT[parse-options]: invalid Fortran "
                 "input-format value\n";
    ROSE_ABORT();
  }

  if (sourceFile->get_Cxx_only() || sourceFile->get_Cuda_only()) {
    return {openacc::Language::Cxx, openacc::InputForm::CPragma};
  }
  if (sourceFile->get_C_only() || sourceFile->get_OpenCL_only()) {
    return {openacc::Language::C, openacc::InputForm::CPragma};
  }
  std::cerr << "REX_ACC_AST_INVARIANT[parse-options]: OpenACC source has no "
               "supported exact base-language identity\n";
  ROSE_ABORT();
}

void parseOpenACCDirectiveOrAbort(OpenACCParseSession &session,
                                  SgPragmaDeclaration *pragma,
                                  const std::string &input,
                                  openacc::ParseOptions options) {
  if (pragma == nullptr || input.empty()) {
    std::cerr << "REX_ACC_AST_INVARIANT[parse]: empty OpenACC parse request\n";
    ROSE_ABORT();
  }

  openacc::ParseResult result = openacc::parseDirective(input, options);
  if (!result.diagnostics.empty()) {
    const openacc::Diagnostic &diagnostic = result.diagnostics.front();
    std::cerr << "REX_ACC_AST_INVARIANT[parse]: diagnostic-code="
              << static_cast<unsigned>(diagnostic.code)
              << " severity=" << static_cast<unsigned>(diagnostic.severity)
              << " at " << diagnostic.range.begin.line << ':'
              << diagnostic.range.begin.column << ": " << diagnostic.message
              << '\n';
    ROSE_ABORT();
  }
  if (!result.directive) {
    std::cerr << "REX_ACC_AST_INVARIANT[parse]: parser produced no typed "
                 "OpenACC directive and no diagnostic\n";
    ROSE_ABORT();
  }
  if (!result.succeeded() || result.directive->language() != options.language ||
      result.directive->inputForm() != options.inputForm) {
    std::cerr << "REX_ACC_AST_INVARIANT[parse]: parser success state or typed "
                 "language/form identity is inconsistent\n";
    ROSE_ABORT();
  }

  validateOpenACCDirectiveForSage(*result.directive);
  session.add(pragma, std::move(*result.directive));
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

static bool isDeclareMapperNumericLiteralContinuation(unsigned char ch) {
  return std::isalnum(ch) || ch == '\'' || ch == '.' || ch == '_';
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

    if (std::isdigit(ch)) {
      const std::string::size_type begin = i++;
      while (i < type_text.size() &&
             isDeclareMapperNumericLiteralContinuation(
                 static_cast<unsigned char>(type_text[i]))) {
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
                                 const std::string &type_text);

static bool splitDeclareMapperTemplateId(
    const std::vector<std::string> &tokens,
    std::vector<std::string> &template_name_tokens,
    std::vector<std::vector<std::string>> &template_argument_tokens) {
  int angle_depth = 0;
  int paren_depth = 0;
  int bracket_depth = 0;
  size_t template_begin = tokens.size();
  size_t template_end = tokens.size();

  for (size_t i = 0; i < tokens.size(); ++i) {
    const std::string &token = tokens[i];
    if (token == "(") {
      ++paren_depth;
    } else if (token == ")") {
      --paren_depth;
    } else if (token == "[") {
      ++bracket_depth;
    } else if (token == "]") {
      --bracket_depth;
    } else if (paren_depth == 0 && bracket_depth == 0 && token == "<") {
      if (angle_depth == 0 && template_begin == tokens.size()) {
        template_begin = i;
      }
      ++angle_depth;
    } else if (paren_depth == 0 && bracket_depth == 0 && token == ">") {
      --angle_depth;
      if (angle_depth == 0) {
        template_end = i;
      }
    }

    if (angle_depth < 0 || paren_depth < 0 || bracket_depth < 0) {
      return false;
    }
  }

  if (angle_depth != 0 || paren_depth != 0 || bracket_depth != 0 ||
      template_begin == tokens.size() || template_end != tokens.size() - 1 ||
      template_begin == 0 || template_begin + 1 >= template_end) {
    return false;
  }

  template_name_tokens.assign(tokens.begin(), tokens.begin() + template_begin);

  std::vector<std::string> current_arg_tokens;
  angle_depth = 0;
  paren_depth = 0;
  bracket_depth = 0;
  for (size_t i = template_begin + 1; i < template_end; ++i) {
    const std::string &token = tokens[i];
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

    if (angle_depth == 0 && paren_depth == 0 && bracket_depth == 0 &&
        token == ",") {
      if (current_arg_tokens.empty()) {
        return false;
      }
      template_argument_tokens.push_back(current_arg_tokens);
      current_arg_tokens.clear();
      continue;
    }

    current_arg_tokens.push_back(token);
  }

  if (angle_depth != 0 || paren_depth != 0 || bracket_depth != 0 ||
      current_arg_tokens.empty()) {
    return false;
  }

  template_argument_tokens.push_back(current_arg_tokens);
  return !template_name_tokens.empty() && !template_argument_tokens.empty();
}

static SgTemplateArgument *
resolveDeclareMapperTemplateArgument(SgPragmaDeclaration *directive,
                                     const std::vector<std::string> &tokens) {
  const std::string arg_text = joinDeclareMapperTypeTokens(tokens);
  if (arg_text.empty()) {
    return nullptr;
  }

  if (SgType *arg_type = resolveDeclareMapperType(directive, arg_text)) {
    return SageBuilder::buildTemplateArgument(arg_type);
  }

  if (SgExpression *arg_expr =
          parseOmpExpression(directive, OMPC_map, arg_text)) {
    return SageBuilder::buildTemplateArgument(arg_expr);
  }

  return nullptr;
}

static bool buildDeclareMapperTemplateArgumentNodes(
    const SgTemplateArgumentPtrList &template_arguments,
    Rose_STL_Container<SgNode *> &argument_nodes) {
  for (SgTemplateArgument *arg : template_arguments) {
    if (arg == nullptr) {
      return false;
    }

    switch (arg->get_argumentType()) {
    case SgTemplateArgument::type_argument:
      if (arg->get_type() == nullptr) {
        return false;
      }
      argument_nodes.push_back(arg->get_type());
      break;

    case SgTemplateArgument::nontype_argument:
      if (arg->get_expression() == nullptr) {
        return false;
      }
      argument_nodes.push_back(
          copyOpenMPExpressionWithExactProvenance(arg->get_expression()));
      break;

    default:
      if (arg->get_templateDeclaration() == nullptr) {
        return false;
      }
      argument_nodes.push_back(arg->get_templateDeclaration());
      break;
    }
  }

  return true;
}

static SgType *
validateDeclareMapperTemplateInstantiationScope(SgNode *context_node,
                                                SgType *type) {
  SgClassType *class_type = isSgClassType(type);
  if (class_type == nullptr) {
    return type;
  }

  SgTemplateInstantiationDecl *decl =
      isSgTemplateInstantiationDecl(class_type->get_declaration());
  if (decl == nullptr) {
    return type;
  }

  SgGlobal *global_scope = context_node != nullptr
                               ? SageInterface::getGlobalScope(context_node)
                               : nullptr;
  if (global_scope == nullptr) {
    std::cerr << "REX_OMP_AST_INVARIANT[declare-mapper-type-scope]: mapper "
                 "context has no global scope\n";
    ROSE_ABORT();
  }

  SgTemplateClassDeclaration *template_decl =
      isSgTemplateClassDeclaration(decl->get_templateDeclaration());
  if (template_decl == nullptr) {
    std::cerr << "REX_OMP_AST_INVARIANT[declare-mapper-template-identity]: "
                 "template instantiation has no exact template declaration\n";
    ROSE_ABORT();
  }

  SgGlobal *template_global = SageInterface::getGlobalScope(template_decl);
  SgGlobal *decl_global = decl->get_scope() != nullptr
                              ? SageInterface::getGlobalScope(decl->get_scope())
                              : nullptr;
  if (template_global != global_scope || decl_global != global_scope) {
    std::cerr << "REX_OMP_AST_INVARIANT[declare-mapper-template-owner]: "
                 "mapper type references a template instantiation from a "
                 "different translation unit\n";
    ROSE_ABORT();
  }

  return type;
}

static SgType *resolveDeclareMapperNamedBaseType(
    SgPragmaDeclaration *directive,
    const std::vector<std::string> &base_name_tokens) {
  if (base_name_tokens.empty()) {
    return nullptr;
  }

  SgScopeStatement *scope =
      directive != nullptr ? directive->get_scope() : nullptr;
  const std::string base_name = joinDeclareMapperTypeTokens(base_name_tokens);

  // Built-in type specifiers are language keywords and therefore have no
  // symbol-table entry. Resolve their complete canonical spellings before
  // attempting named-type lookup.
  if (base_name == "void")
    return SageBuilder::buildVoidType();
  if (base_name == "bool")
    return SageBuilder::buildBoolType();
  if (base_name == "char")
    return SageBuilder::buildCharType();
  if (base_name == "signed char")
    return SageBuilder::buildSignedCharType();
  if (base_name == "unsigned char")
    return SageBuilder::buildUnsignedCharType();
  if (base_name == "short" || base_name == "short int")
    return SageBuilder::buildShortType();
  if (base_name == "signed short" || base_name == "signed short int")
    return SageBuilder::buildSignedShortType();
  if (base_name == "unsigned short" || base_name == "unsigned short int")
    return SageBuilder::buildUnsignedShortType();
  if (base_name == "int")
    return SageBuilder::buildIntType();
  if (base_name == "signed" || base_name == "signed int")
    return SageBuilder::buildSignedIntType();
  if (base_name == "unsigned" || base_name == "unsigned int")
    return SageBuilder::buildUnsignedIntType();
  if (base_name == "long" || base_name == "long int")
    return SageBuilder::buildLongType();
  if (base_name == "signed long" || base_name == "signed long int")
    return SageBuilder::buildSignedLongType();
  if (base_name == "unsigned long" || base_name == "unsigned long int")
    return SageBuilder::buildUnsignedLongType();
  if (base_name == "long long" || base_name == "long long int")
    return SageBuilder::buildLongLongType();
  if (base_name == "signed long long" || base_name == "signed long long int")
    return SageBuilder::buildSignedLongLongType();
  if (base_name == "unsigned long long" ||
      base_name == "unsigned long long int")
    return SageBuilder::buildUnsignedLongLongType();
  if (base_name == "float")
    return SageBuilder::buildFloatType();
  if (base_name == "double")
    return SageBuilder::buildDoubleType();
  if (base_name == "long double")
    return SageBuilder::buildLongDoubleType();
  if (base_name == "wchar_t")
    return SageBuilder::buildWcharType();
  if (base_name == "char8_t")
    return SageBuilder::buildChar8Type();
  if (base_name == "char16_t")
    return SageBuilder::buildChar16Type();
  if (base_name == "char32_t")
    return SageBuilder::buildChar32Type();

  std::vector<std::string> template_name_tokens;
  std::vector<std::vector<std::string>> template_argument_tokens;
  if (!splitDeclareMapperTemplateId(base_name_tokens, template_name_tokens,
                                    template_argument_tokens)) {
    return SageInterface::lookupNamedTypeInParentScopes(base_name, scope);
  }

  SgTemplateArgumentPtrList template_arguments;
  template_arguments.reserve(template_argument_tokens.size());
  for (const std::vector<std::string> &arg_tokens : template_argument_tokens) {
    SgTemplateArgument *arg =
        resolveDeclareMapperTemplateArgument(directive, arg_tokens);
    if (arg == nullptr) {
      return nullptr;
    }
    if (directive != nullptr && arg->get_parent() == nullptr) {
      arg->set_parent(directive);
    }
    template_arguments.push_back(arg);
  }

  auto lookup_class_type = [&](const std::string &name) -> SgType * {
    if (name.empty()) {
      return nullptr;
    }
    if (SgClassSymbol *class_symbol =
            SageInterface::lookupClassSymbolInParentScopes(
                SgName(name), scope, &template_arguments)) {
      return validateDeclareMapperTemplateInstantiationScope(
          directive, class_symbol->get_type());
    }
    return nullptr;
  };

  if (SgType *resolved = lookup_class_type(base_name)) {
    return resolved;
  }

  const std::string template_name =
      joinDeclareMapperTypeTokens(template_name_tokens);
  if (SgType *resolved = lookup_class_type(template_name)) {
    return resolved;
  }

  if (SgTemplateClassSymbol *template_symbol =
          SageInterface::lookupTemplateClassSymbolInParentScopes(
              SgName(template_name), nullptr, nullptr, scope)) {
    if (SgTemplateClassDeclaration *template_decl =
            isSgTemplateClassDeclaration(template_symbol->get_declaration())) {
      Rose_STL_Container<SgNode *> argument_nodes;
      if (buildDeclareMapperTemplateArgumentNodes(template_arguments,
                                                  argument_nodes)) {
        return validateDeclareMapperTemplateInstantiationScope(
            directive,
            SageBuilder::buildClassTemplateType(template_decl, argument_nodes));
      }
    }
  }

  if (SgType *resolved =
          SageInterface::lookupNamedTypeInParentScopes(base_name, scope)) {
    if (SgClassType *class_type = isSgClassType(resolved)) {
      if (isSgTemplateInstantiationDecl(class_type->get_declaration()) ==
          nullptr) {
        return nullptr;
      }
    }
    return validateDeclareMapperTemplateInstantiationScope(directive, resolved);
  }

  return nullptr;
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

  SgType *resolved_type =
      resolveDeclareMapperNamedBaseType(directive, base_name_tokens);
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

std::string requireFortranDirectiveText(SgPragmaDeclaration *pragma,
                                        const std::string &text,
                                        const char *description) {
  if (pragma == nullptr || description == nullptr) {
    std::cerr << "REX_OMP_AST_INVARIANT[fortran-provenance]: null directive "
                 "text request\n";
    ROSE_ABORT();
  }
  if (trimWhitespaceCopy(text).empty()) {
    std::cerr << "REX_OMP_AST_INVARIANT[fortran-provenance]: source pragma "
                 "has no exact "
              << description << "\n";
    ROSE_ABORT();
  }
  return text;
}

std::string fortranDirectiveFamilyName(
    SgPragmaDeclaration::fortran_directive_family_enum family) {
  switch (family) {
  case SgPragmaDeclaration::e_fortran_directive_openmp:
    return "omp";
  case SgPragmaDeclaration::e_fortran_directive_ompx:
    return "ompx";
  case SgPragmaDeclaration::e_fortran_directive_openacc:
    return "acc";
  case SgPragmaDeclaration::e_fortran_directive_cuda:
    return "cuf";
  case SgPragmaDeclaration::e_fortran_directive_none:
    break;
  }
  std::cerr << "REX_OMP_AST_INVARIANT[fortran-provenance]: invalid typed "
               "directive family\n";
  ROSE_ABORT();
}

struct FortranDirectiveGroupView {
  std::string id;
  std::string family;
  SgPragmaDeclaration *primary = nullptr;
  std::vector<SgPragmaDeclaration *> members;
  std::string logical_text;
  std::string cooked_text;
};

std::vector<FortranDirectiveGroupView>
collectFortranDirectiveGroups(SgSourceFile *source_file,
                              const std::string &requested_family) {
  if (source_file == nullptr) {
    std::cerr << "REX_OMP_AST_INVARIANT[fortran-provenance]: null source "
                 "file\n";
    ROSE_ABORT();
  }

  std::vector<FortranDirectiveGroupView> groups;
  std::unordered_map<std::string, size_t> group_positions;
  for (SgNode *node :
       NodeQuery::querySubTree(source_file, V_SgPragmaDeclaration)) {
    SgPragmaDeclaration *pragma = isSgPragmaDeclaration(node);
    ROSE_ASSERT(pragma != nullptr);
    if (getEnclosingSourceFile(pragma) != source_file) {
      continue;
    }
    const SgPragmaDeclaration::fortran_directive_family_enum family_kind =
        pragma->get_fortran_directive_family();
    if (family_kind == SgPragmaDeclaration::e_fortran_directive_none) {
      continue;
    }
    const std::string family = fortranDirectiveFamilyName(family_kind);
    const bool requested = requested_family.empty() ||
                           family == requested_family ||
                           (requested_family == "omp" && family == "ompx");
    if (!requested) {
      continue;
    }
    const std::string &group_id = pragma->get_fortran_directive_group_id();
    const size_t member_count = pragma->get_fortran_directive_member_count();
    const size_t member_index = pragma->get_fortran_directive_member_index();
    if (group_id.empty() || member_count == 0 || member_index >= member_count) {
      std::cerr << "REX_OMP_AST_INVARIANT[fortran-provenance]: invalid "
                   "directive group ownership\n";
      ROSE_ABORT();
    }

    auto [position, inserted] =
        group_positions.emplace(group_id, groups.size());
    if (inserted) {
      groups.push_back(FortranDirectiveGroupView{
          group_id, family, nullptr,
          std::vector<SgPragmaDeclaration *>(member_count, nullptr), "", ""});
    }
    FortranDirectiveGroupView &group = groups[position->second];
    if (group.family != family || group.members.size() != member_count ||
        group.members[member_index] != nullptr) {
      std::cerr << "REX_OMP_AST_INVARIANT[fortran-provenance]: inconsistent "
                   "or duplicate directive group member\n";
      ROSE_ABORT();
    }
    group.members[member_index] = pragma;
    requireFortranDirectiveText(
        pragma, pragma->get_fortran_directive_raw_text(), "raw physical text");

    if (pragma->get_fortran_directive_primary()) {
      if (member_index != 0 || group.primary != nullptr) {
        std::cerr << "REX_OMP_AST_INVARIANT[fortran-provenance]: directive "
                     "group has an invalid or duplicate primary\n";
        ROSE_ABORT();
      }
      group.primary = pragma;
      group.logical_text = requireFortranDirectiveText(
          pragma, pragma->get_fortran_directive_logical_text(),
          "logical source spelling");
      group.cooked_text = requireFortranDirectiveText(
          pragma, pragma->get_fortran_directive_semantic_text(),
          "producer-cooked semantic text");
    } else if (!pragma->get_fortran_directive_logical_text().empty() ||
               !pragma->get_fortran_directive_semantic_text().empty()) {
      std::cerr << "REX_OMP_AST_INVARIANT[fortran-provenance]: nonprimary "
                   "directive member owns primary semantic text\n";
      ROSE_ABORT();
    }
  }

  for (const FortranDirectiveGroupView &group : groups) {
    if (group.primary == nullptr || group.logical_text.empty() ||
        group.cooked_text.empty() ||
        std::find(group.members.begin(), group.members.end(), nullptr) !=
            group.members.end()) {
      std::cerr << "REX_OMP_AST_INVARIANT[fortran-provenance]: incomplete "
                   "directive group\n";
      ROSE_ABORT();
    }
  }
  return groups;
}

std::string getCxxOpenMPDirectiveSourceText(SgPragmaDeclaration *pragma) {
  if (pragma == nullptr) {
    std::cerr << "REX_OMP_AST_INVARIANT[source-text]: null C/C++ pragma\n";
    ROSE_ABORT();
  }

  const std::string &source_text = pragma->get_cxx_source_text();
  if (trimWhitespaceCopy(source_text).empty()) {
    std::cerr << "REX_OMP_AST_INVARIANT[source-text]: C/C++ OpenMP pragma at "
              << pragma->get_startOfConstruct()->get_filenameString() << ":"
              << pragma->get_startOfConstruct()->get_line()
              << " has no exact frontend-captured source spelling\n";
    ROSE_ABORT();
  }
  return source_text;
}

static bool hasUsableSourceLocation(const Sg_File_Info *info) {
  return info != nullptr && info->get_line() > 0 && !info->isTransformation() &&
         !info->isCompilerGenerated() &&
         !info->isSourcePositionUnavailableInFrontend() &&
         info->get_physical_file_id() >= 0 && info->get_physical_line() > 0;
}

static int compareOpenMPSourceLocations(const Sg_File_Info *lhs,
                                        const Sg_File_Info *rhs) {
  if (!hasUsableSourceLocation(lhs) || !hasUsableSourceLocation(rhs) ||
      lhs->get_physical_file_id() != rhs->get_physical_file_id()) {
    std::cerr << "REX_OMP_AST_INVARIANT[preprocessing-location]: cannot "
                 "compare incomplete or cross-file source locations\n";
    ROSE_ABORT();
  }
  if (lhs->get_physical_line() != rhs->get_physical_line()) {
    return lhs->get_physical_line() < rhs->get_physical_line() ? -1 : 1;
  }
  if (lhs->get_col() != rhs->get_col()) {
    return lhs->get_col() < rhs->get_col() ? -1 : 1;
  }
  return 0;
}

static void
moveInterveningPreprocessingInfoToOpenMPBody(SgPragmaDeclaration *pragma,
                                             SgStatement *converted_statement) {
  if (pragma == nullptr || converted_statement == nullptr) {
    std::cerr << "REX_OMP_AST_INVARIANT[preprocessing-boundary]: null pragma "
                 "or converted statement\n";
    ROSE_ABORT();
  }
  SgOmpBodyStatement *body_statement =
      isSgOmpBodyStatement(converted_statement);
  if (body_statement == nullptr) {
    return;
  }
  SgLocatedNode *body = body_statement != nullptr
                            ? isSgLocatedNode(body_statement->get_body())
                            : nullptr;
  if (body == nullptr) {
    std::cerr << "REX_OMP_AST_INVARIANT[preprocessing-boundary]: structured "
                 "directive has no located body\n";
    ROSE_ABORT();
  }

  const Sg_File_Info *pragma_end = pragma->get_endOfConstruct();
  if (!hasUsableSourceLocation(pragma_end)) {
    std::cerr << "REX_OMP_AST_INVARIANT[preprocessing-boundary]: structured "
                 "directive has no exact source end\n";
    ROSE_ABORT();
  }
  while (hasUsableSourceLocation(body->get_startOfConstruct()) &&
         compareOpenMPSourceLocations(pragma_end,
                                      body->get_startOfConstruct()) >= 0) {
    SgOmpBodyStatement *nested = isSgOmpBodyStatement(body);
    SgLocatedNode *nested_body =
        nested != nullptr ? isSgLocatedNode(nested->get_body()) : nullptr;
    if (nested_body != nullptr) {
      body = nested_body;
      continue;
    }

    // A paired Fortran construct with multiple statements owns a generated
    // basic block whose source span is the complete BEGIN/END construct.  That
    // wrapper deliberately begins at the directive, so the first statement is
    // the exact boundary for preprocessing records written after BEGIN.  Do
    // not mistake the structural wrapper's source span for its body boundary.
    if (SgBasicBlock *block = isSgBasicBlock(body)) {
      const SgStatementPtrList &statements = block->get_statements();
      SgStatement *first = statements.empty() ? nullptr : statements.front();
      if (first == nullptr || first->get_parent() != block ||
          first->get_scope() != block) {
        std::cerr << "REX_OMP_AST_INVARIANT[preprocessing-boundary]: "
                     "generated structured body has no exact first "
                     "statement\n";
        ROSE_ABORT();
      }
      body = first;
      continue;
    }
    break;
  }
  const Sg_File_Info *body_start = body->get_startOfConstruct();
  if (!hasUsableSourceLocation(body_start) ||
      compareOpenMPSourceLocations(pragma_end, body_start) >= 0) {
    std::cerr << "REX_OMP_AST_INVARIANT[preprocessing-boundary]: structured "
                 "directive and body have an invalid source order\n";
    ROSE_ABORT();
  }

  AttachedPreprocessingInfoType *pragma_info =
      pragma->get_attachedPreprocessingInfoPtr();
  if (pragma_info == nullptr) {
    return;
  }

  std::vector<PreprocessingInfo *> intervening;
  for (PreprocessingInfo *info : *pragma_info) {
    if (info == nullptr || !hasUsableSourceLocation(info->get_file_info())) {
      std::cerr << "REX_OMP_AST_INVARIANT[preprocessing-owner]: pragma owns "
                   "preprocessing information without an exact source "
                   "location\n";
      ROSE_ABORT();
    }
    if (compareOpenMPSourceLocations(pragma_end, info->get_file_info()) < 0 &&
        compareOpenMPSourceLocations(info->get_file_info(), body_start) < 0) {
      intervening.push_back(info);
    }
  }
  if (intervening.empty()) {
    return;
  }

  for (PreprocessingInfo *info : intervening) {
    pragma->detachPreprocessingInfo(info);
    info->setRelativePosition(PreprocessingInfo::before);
    AttachedPreprocessingInfoType *body_info =
        body->get_attachedPreprocessingInfoPtr();
    AttachedPreprocessingInfoType::iterator position;
    if (body_info != nullptr) {
      position = body_info->begin();
    }
    for (; body_info != nullptr && position != body_info->end(); ++position) {
      PreprocessingInfo *existing = *position;
      if (existing == nullptr ||
          !hasUsableSourceLocation(existing->get_file_info())) {
        std::cerr << "REX_OMP_AST_INVARIANT[preprocessing-owner]: body owns "
                     "preprocessing information without an exact source "
                     "location\n";
        ROSE_ABORT();
      }
      if (compareOpenMPSourceLocations(info->get_file_info(),
                                       existing->get_file_info()) < 0) {
        break;
      }
    }
    if (body_info == nullptr || position == body_info->end()) {
      body->attachPreprocessingInfo(
          info, PreprocessingInfo::before,
          SgLocatedNode::PreprocessingInfoInsertion::back);
    } else {
      body->attachPreprocessingInfoRelative(info, PreprocessingInfo::before,
                                            *position, false);
    }
  }
}

static const Sg_File_Info *
getPreferredLocatedNodeStartInfo(const SgLocatedNode *located) {
  if (located == nullptr) {
    return nullptr;
  }
  return located->get_startOfConstruct();
}

void initializeGeneratedOpenMPStatement(SgStatement *statement) {
  if (statement == nullptr) {
    std::cerr << "REX_OMP_AST_INVARIANT[generated-statement]: null OpenMP "
                 "statement\n";
    ROSE_ABORT();
  }

  SgLocatedNode *located = isSgLocatedNode(statement);
  if (located == nullptr) {
    std::cerr << "REX_OMP_AST_INVARIANT[generated-statement]: OpenMP "
                 "statement is not a located node\n";
    ROSE_ABORT();
  }

  Sg_File_Info *file_info = located->get_file_info();
  Sg_File_Info *start = located->get_startOfConstruct();
  Sg_File_Info *end = located->get_endOfConstruct();
  const bool all_locations_missing =
      file_info == nullptr && start == nullptr && end == nullptr;
  const bool any_location_missing =
      file_info == nullptr || start == nullptr || end == nullptr;
  if (all_locations_missing) {
    setSourcePositionAsTransformation(located);
    file_info = located->get_file_info();
    start = located->get_startOfConstruct();
    end = located->get_endOfConstruct();
  } else if (any_location_missing) {
    std::cerr << "REX_OMP_AST_INVARIANT[generated-statement-location]: "
                 "OpenMP statement has a partially initialized source range\n";
    ROSE_ABORT();
  }

  if (file_info == nullptr || start == nullptr || end == nullptr) {
    std::cerr << "REX_OMP_AST_INVARIANT[generated-statement-location]: "
                 "OpenMP statement has no complete source range\n";
    ROSE_ABORT();
  }

  const bool has_source_anchor = hasUsableSourceLocation(file_info) ||
                                 hasUsableSourceLocation(start) ||
                                 hasUsableSourceLocation(end);
  if (!has_source_anchor) {
    file_info->setTransformation();
    start->setTransformation();
    end->setTransformation();
    located->setTransformation();
  }

  file_info->setOutputInCodeGeneration();
  start->setOutputInCodeGeneration();
  end->setOutputInCodeGeneration();
  located->setOutputInCodeGeneration();
  located->markAsModified();

  if (SgOmpBodyStatement *omp_body = isSgOmpBodyStatement(statement)) {
    SgStatement *body = omp_body->get_body();
    SgLocatedNode *located_body = isSgLocatedNode(body);
    if (body != nullptr &&
        (located_body == nullptr || located_body->get_file_info() == nullptr ||
         located_body->get_startOfConstruct() == nullptr ||
         located_body->get_endOfConstruct() == nullptr)) {
      std::cerr << "REX_OMP_AST_INVARIANT[generated-statement-body]: OpenMP "
                   "body has no complete frontend source range\n";
      ROSE_ABORT();
    }
  }

  statement->markAsModified();
}

static void markOpenMPSourceFileAsModified(SgSourceFile *source_file) {
  if (source_file == nullptr) {
    std::cerr << "REX_OMP_AST_INVARIANT[source-file]: cannot mark a null "
                 "source file as modified\n";
    ROSE_ABORT();
  }

  SgGlobal *global_scope = source_file->get_globalScope();
  if (global_scope == nullptr) {
    std::cerr << "REX_OMP_AST_INVARIANT[source-file]: OpenMP source file has "
                 "no global scope\n";
    ROSE_ABORT();
  }

  if (global_scope->get_file_info() != nullptr) {
    global_scope->get_file_info()->setOutputInCodeGeneration();
  }
  if (global_scope->get_startOfConstruct() != nullptr) {
    global_scope->get_startOfConstruct()->setOutputInCodeGeneration();
  }
  if (global_scope->get_endOfConstruct() != nullptr) {
    global_scope->get_endOfConstruct()->setOutputInCodeGeneration();
  }

  global_scope->setOutputInCodeGeneration();
  global_scope->markAsModified();
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

static SgExpression *cloneOmpVarExprFromNode(SgNode *node) {
  if (SgInitializedName *iname = isSgInitializedName(node)) {
    return SageBuilder::buildVarRefExp(iname);
  }

  SgExpression *expr = isSgExpression(node);
  if (expr == nullptr) {
    return nullptr;
  }

  if (SgVarRefExp *var_ref = isSgVarRefExp(expr)) {
    if (SgVariableSymbol *symbol = var_ref->get_symbol()) {
      SgVarRefExp *clone = SageBuilder::buildVarRefExp(symbol);
      if (SgExpression *original_tree = var_ref->get_originalExpressionTree()) {
        SgExpression *original_copy =
            copyOpenMPExpressionWithExactProvenance(original_tree);
        if (original_copy == nullptr ||
            original_copy->get_parent() != nullptr) {
          std::cerr << "REX_OMP_AST_INVARIANT[source-expression-copy]: "
                       "failed to produce an unowned source expression\n";
          ROSE_ABORT();
        }
        clone->set_originalExpressionTree(original_copy);
        original_copy->set_parent(clone);
      }
      return clone;
    }
  }

  return copyOpenMPExpressionWithExactProvenance(expr);
}

static void appendFlattenedOmpVarExprNodes(SgOmpVariablesClause *clause,
                                           SgNode *node) {
  if (clause == nullptr || node == nullptr) {
    std::cerr << "REX_OMP_AST_INVARIANT[variable-list-input]: null clause or "
                 "variable-list node\n";
    ROSE_ABORT();
  }

  if (SgExprListExp *expr_list = isSgExprListExp(node)) {
    for (SgExpression *expr : expr_list->get_expressions()) {
      appendFlattenedOmpVarExprNodes(clause, expr);
    }
    return;
  }

  if (SgCommaOpExp *comma = isSgCommaOpExp(node)) {
    appendFlattenedOmpVarExprNodes(clause, comma->get_lhs_operand());
    appendFlattenedOmpVarExprNodes(clause, comma->get_rhs_operand());
    return;
  }

  if (SgExpression *expr = cloneOmpVarExprFromNode(node)) {
    if (SgFortranCommonBlockRefExp *common =
            isSgFortranCommonBlockRefExp(expr)) {
      SageInterface::validateFortranCommonBlockRef(common);
    }
    SgExprListExp *variables = clause->get_variables();
    if (variables == nullptr) {
      std::cerr << "REX_OMP_AST_INVARIANT[variable-list-owner]: OpenMP "
                   "variables clause has no owned expression list\n";
      ROSE_ABORT();
    }
    if (variables->get_parent() == nullptr) {
      variables->set_parent(clause);
    } else if (variables->get_parent() != clause) {
      std::cerr << "REX_OMP_AST_INVARIANT[variable-list-owner]: OpenMP "
                   "variables expression list is owned by another node\n";
      ROSE_ABORT();
    }
    variables->get_expressions().push_back(expr);
    expr->set_parent(variables);
    return;
  }

  cerr << "REX_OMP_AST_INVARIANT[variable-list]: unhandled node type "
       << node->class_name() << "\n";
  ROSE_ABORT();
}

void clearOpenMPClauseTemporaryState() { openMPExpressionVariables().clear(); }

const OmpParsedExpression *asParsedExpression(
    const std::shared_ptr<const ompparser::HostSemanticNode> &node) {
  return dynamic_cast<const OmpParsedExpression *>(node.get());
}

const OmpParsedExpression *
requireParsedHostFragment(const ompparser::HostFragment &fragment,
                          const char *contract) {
  const OmpParsedExpression *parsed = asParsedExpression(fragment.semantic);
  if (parsed == nullptr || contract == nullptr ||
      parsed->text != fragment.spelling ||
      parsed->mode != fragment.parse_mode) {
    std::cerr
        << "REX_OMP_AST_INVARIANT["
        << (contract != nullptr ? contract : "host-fragment")
        << "]: parser host fragment has no matching typed semantic node\n";
    ROSE_ABORT();
  }
  return parsed;
}

const OmpParsedExpression *
requireCachedHostFragment(const ompparser::HostFragment &source_fragment,
                          const OmpParsedExpression *cached_fragment,
                          const char *contract) {
  if (cached_fragment == nullptr || contract == nullptr ||
      source_fragment.spelling.empty() ||
      cached_fragment->text != source_fragment.spelling ||
      cached_fragment->mode != source_fragment.parse_mode) {
    std::cerr << "REX_OMP_AST_INVARIANT["
              << (contract != nullptr ? contract : "host-fragment-cache")
              << "]: source fragment and exact reparsed callback cache "
                 "diverge\n";
    ROSE_ABORT();
  }
  return cached_fragment;
}

void requireCachedParsedExpression(const OmpParsedExpression *parsed) {
  if (parsed == nullptr) {
    std::cerr << "REX_OMP_AST_INVARIANT[expression-cache]: null parsed "
                 "expression\n";
    ROSE_ABORT();
  }
  if (parsed->node == nullptr) {
    std::cerr << "REX_OMP_AST_INVARIANT[expression-cache]: parsed expression "
                 "has no semantic AST node\n";
    ROSE_ABORT();
  }
}

SgExpression *consumeParsedExpressionNode(const OmpParsedExpression *parsed) {
  requireCachedParsedExpression(parsed);
  if (parsed->consumed) {
    std::cerr << "REX_OMP_AST_INVARIANT[expression-cache-consumption]: "
                 "cached expression was consumed more than once\n";
    ROSE_ABORT();
  }

  if (SgInitializedName *iname = isSgInitializedName(parsed->node)) {
    parsed->consumed = true;
    return SageBuilder::buildVarRefExp(iname);
  }

  if (SgExpression *expr = isSgExpression(parsed->node)) {
    if (expr->get_parent() != nullptr) {
      std::cerr << "REX_OMP_AST_INVARIANT[expression-cache-consumption]: "
                   "cached expression already has a structural owner\n";
      ROSE_ABORT();
    }
    parsed->consumed = true;
    return expr;
  }

  std::cerr << "REX_OMP_AST_INVARIANT[expression-cache]: unsupported "
               "semantic node type "
            << parsed->node->class_name() << "\n";
  ROSE_ABORT();
}

void parseAndStoreVariableList(const std::string &expr_text,
                               OmpParsedExpression *parsed,
                               SgPragmaDeclaration *pragma_declaration,
                               OpenMPDirective *directive,
                               OpenMPClauseKind clause_kind) {
  ROSE_ASSERT(parsed != nullptr);
  static_cast<void>(directive);
  clearOpenMPClauseTemporaryState();
  parseOmpVariable(std::make_pair(pragma_declaration, directive), clause_kind,
                   expr_text);
  ROSE_ASSERT(!openMPExpressionVariables().empty());
  parsed->node = openMPExpressionVariables().back();
  openMPExpressionVariables().clear();
}

void parseAndStoreArraySection(const std::string &expr_text,
                               OmpParsedExpression *parsed,
                               SgPragmaDeclaration *pragma_declaration,
                               OpenMPDirective *directive,
                               OpenMPClauseKind clause_kind) {
  ROSE_ASSERT(parsed != nullptr);
  clearOpenMPClauseTemporaryState();
  parseOmpArraySection(pragma_declaration, clause_kind, expr_text);
  ROSE_ASSERT(!openMPExpressionVariables().empty());
  parsed->node = openMPExpressionVariables().back();
  openMPExpressionVariables().clear();
}

void parseAndStoreExpression(const std::string &expr_text,
                             OmpParsedExpression *parsed,
                             SgPragmaDeclaration *pragma_declaration,
                             OpenMPDirective *directive,
                             OpenMPClauseKind clause_kind) {
  ROSE_ASSERT(parsed != nullptr);
  static_cast<void>(directive);
  clearOpenMPClauseTemporaryState();
  SgExpression *expression =
      parseOmpExpression(pragma_declaration, clause_kind, expr_text);
  ROSE_ASSERT(expression != nullptr);
  parsed->node = expression;
  openMPExpressionVariables().clear();
}

SgOmpNameExpression *buildOpenMPNameExpression(const std::string &text) {
  const std::string name = trimWhitespaceCopy(text);
  if (name.empty()) {
    std::cerr << "REX_OMP_AST_INVARIANT[name-expression]: empty OpenMP "
                 "grammar identifier\n";
    ROSE_ABORT();
  }
  SgOmpNameExpression *expression = new SgOmpNameExpression(name);
  setOneSourcePositionForTransformation(expression);
  return expression;
}

SgOmpSourceExpression *
buildOpenMPSourceExpression(const std::string &source_spelling);

SgExpression *buildOpenMPSyntaxTokenExpression(const std::string &text,
                                               OpenMPBaseLang base_lang) {
  const std::string token = trimWhitespaceCopy(text);
  if (token.empty()) {
    std::cerr << "REX_OMP_AST_INVARIANT[syntax-token]: empty OpenMP grammar "
                 "token\n";
    ROSE_ABORT();
  }
  const bool double_quoted =
      token.size() >= 2 && token.front() == '"' && token.back() == '"';
  const bool single_quoted =
      token.size() >= 2 && token.front() == '\'' && token.back() == '\'';
  if (double_quoted || single_quoted) {
    if (base_lang != Lang_C && base_lang != Lang_Cplusplus &&
        base_lang != Lang_Fortran) {
      std::cerr << "REX_OMP_AST_INVARIANT[syntax-token]: quoted token has no "
                   "exact source language\n";
      ROSE_ABORT();
    }
    if (base_lang != Lang_Fortran && single_quoted) {
      std::cerr << "REX_OMP_AST_INVARIANT[syntax-token]: C/C++ OpenMP string "
                   "token uses a character-literal delimiter\n";
      ROSE_ABORT();
    }
    return buildOpenMPSourceExpression(token);
  }
  if (OmpSupport::isSimpleMapperIdentifier(token)) {
    return buildOpenMPNameExpression(token);
  }
  return buildOpenMPSourceExpression(token);
}

void initializeOpenMPExactSemanticBindings(OmpExprParseContext *context) {
  ROSE_ASSERT(context != nullptr);
  ROSE_ASSERT(context->pragma_declaration != nullptr);
  ROSE_ASSERT(context->directive != nullptr);
  const OpenMPBaseLang base_language = context->directive->getBaseLang();
  if (base_language == Lang_Fortran) {
    auto record =
        openMPFortranExactSemanticBindings().find(context->pragma_declaration);
    if (record == openMPFortranExactSemanticBindings().end() ||
        record->second.directiveSource().empty()) {
      std::cerr
          << "REX_OMP_AST_INVARIANT[fortran-exact-semantic-binding]: pragma "
             "has no invocation-owned Flang binding record\n";
      ROSE_ABORT();
    }
    context->fortran_semantics.pragma = context->pragma_declaration;
    context->fortran_semantics.bindings = &record->second;
    context->fortran_semantics.binding_consumed.assign(
        context->fortran_semantics.bindings->bindings().size(), false);
    context->fortran_semantics.expression_consumed.assign(
        context->fortran_semantics.bindings->expressions().size(), false);
    context->fortran_semantics.source_cursor = 0;
    if (context->fortran_semantics.bindings->producer() ==
        OmpFortranExactSemanticBindings::Producer::rex_typed_scope) {
      return;
    }
    if (context->fortran_semantics.bindings->producer() !=
        OmpFortranExactSemanticBindings::Producer::flang_parse_tree) {
      std::cerr << "REX_OMP_AST_INVARIANT[fortran-exact-semantic-binding]: "
                   "pragma has an invalid Fortran semantic producer\n";
      ROSE_ABORT();
    }
    auto predefinedAllocatorSpelling =
        [](OpenMPAllocateClauseAllocator allocator) -> const char * {
      switch (allocator) {
      case OMPC_ALLOCATE_ALLOCATOR_default:
        return "omp_default_mem_alloc";
      case OMPC_ALLOCATE_ALLOCATOR_large_cap:
        return "omp_large_cap_mem_alloc";
      case OMPC_ALLOCATE_ALLOCATOR_cons_mem:
        return "omp_const_mem_alloc";
      case OMPC_ALLOCATE_ALLOCATOR_high_bw:
        return "omp_high_bw_mem_alloc";
      case OMPC_ALLOCATE_ALLOCATOR_low_lat:
        return "omp_low_lat_mem_alloc";
      case OMPC_ALLOCATE_ALLOCATOR_cgroup:
        return "omp_cgroup_mem_alloc";
      case OMPC_ALLOCATE_ALLOCATOR_pteam:
        return "omp_pteam_mem_alloc";
      case OMPC_ALLOCATE_ALLOCATOR_thread:
        return "omp_thread_mem_alloc";
      case OMPC_ALLOCATE_ALLOCATOR_unspecified:
      case OMPC_ALLOCATE_ALLOCATOR_user:
        return nullptr;
      default:
        std::cerr << "REX_OMP_AST_INVARIANT[fortran-predefined-allocator]: "
                     "allocate clause has an invalid allocator kind\n";
        ROSE_ABORT();
      }
    };
    auto foldSpelling = [](std::string spelling) {
      std::transform(spelling.begin(), spelling.end(), spelling.begin(),
                     [](unsigned char character) {
                       return static_cast<char>(std::tolower(character));
                     });
      return spelling;
    };

    std::vector<std::string> grammarOwnedAllocators;
    if (const std::vector<OpenMPClause *> *clauses =
            context->directive->findClauses(OMPC_allocate)) {
      for (OpenMPClause *clause : *clauses) {
        OpenMPAllocateClause *allocate =
            dynamic_cast<OpenMPAllocateClause *>(clause);
        if (allocate == nullptr) {
          std::cerr
              << "REX_OMP_AST_INVARIANT[fortran-predefined-allocator]: "
                 "allocate clause list contains the wrong typed IR node\n";
          ROSE_ABORT();
        }
        if (const char *spelling =
                predefinedAllocatorSpelling(allocate->getAllocator())) {
          grammarOwnedAllocators.emplace_back(spelling);
        }
      }
    }

    const auto &bindings = context->fortran_semantics.bindings->bindings();
    const auto &expressions =
        context->fortran_semantics.bindings->expressions();
    auto consumeGrammarOwnedName =
        [&](const std::string &expected,
            OmpFortranExactSemanticBindings::BindingKind expectedKind,
            const char *contract) {
          ROSE_ASSERT(contract != nullptr);
          std::size_t selectedBinding = std::string::npos;
          std::size_t selectedExpression = std::string::npos;
          for (std::size_t index = 0; index < bindings.size(); ++index) {
            const auto &binding = bindings[index];
            if (!context->fortran_semantics.binding_consumed[index] &&
                binding.kind() == expectedKind &&
                foldSpelling(binding.spelling()) == expected) {
              selectedBinding = index;
              break;
            }
          }
          if (selectedBinding == std::string::npos) {
            std::cerr << "REX_OMP_AST_INVARIANT[" << contract
                      << "]: grammar-owned name '" << expected
                      << "' has no exact Flang semantic identity\n";
            ROSE_ABORT();
          }
          const auto &binding = bindings[selectedBinding];
          for (std::size_t index = 0; index < expressions.size(); ++index) {
            const auto &expression = expressions[index];
            if (!context->fortran_semantics.expression_consumed[index] &&
                expression.sourceOffset() == binding.sourceOffset() &&
                expression.sourceSize() == binding.sourceSize() &&
                foldSpelling(expression.expression()) == expected &&
                expression.subexpressions().empty()) {
              if (selectedExpression != std::string::npos) {
                std::cerr << "REX_OMP_AST_INVARIANT[" << contract
                          << "]: one grammar-owned name maps to multiple exact "
                             "Flang expression records\n";
                ROSE_ABORT();
              }
              selectedExpression = index;
            }
          }
          if (selectedExpression == std::string::npos) {
            std::cerr << "REX_OMP_AST_INVARIANT[" << contract
                      << "]: grammar-owned name has no exact scalar Flang "
                         "expression record\n";
            ROSE_ABORT();
          }
          context->fortran_semantics.binding_consumed[selectedBinding] = true;
          context->fortran_semantics.expression_consumed[selectedExpression] =
              true;
        };
    for (const std::string &expected : grammarOwnedAllocators) {
      consumeGrammarOwnedName(
          expected, OmpFortranExactSemanticBindings::BindingKind::value,
          "fortran-predefined-allocator");
    }

    OpenMPDirective *grammarOwnedDirective = context->directive;
    if (context->directive->getKind() == OMPD_end) {
      OpenMPEndDirective *endDirective =
          dynamic_cast<OpenMPEndDirective *>(context->directive);
      if (endDirective == nullptr ||
          endDirective->getPairedDirective() == nullptr) {
        std::cerr << "REX_OMP_AST_INVARIANT[fortran-end-directive]: END "
                     "directive has no exact paired typed IR node\n";
        ROSE_ABORT();
      }
      grammarOwnedDirective = endDirective->getPairedDirective();
    }
    if (grammarOwnedDirective->getKind() == OMPD_critical) {
      OpenMPCriticalDirective *critical =
          dynamic_cast<OpenMPCriticalDirective *>(grammarOwnedDirective);
      if (critical == nullptr) {
        std::cerr << "REX_OMP_AST_INVARIANT[fortran-critical-name]: "
                     "critical directive has the wrong typed IR node\n";
        ROSE_ABORT();
      }
      const std::string criticalName =
          foldSpelling(critical->getCriticalName());
      if (!criticalName.empty()) {
        consumeGrammarOwnedName(
            criticalName,
            OmpFortranExactSemanticBindings::BindingKind::syntax_name,
            "fortran-critical-name");
      }
    }
    return;
  }
  if (base_language != Lang_C && base_language != Lang_Cplusplus) {
    std::cerr << "REX_OMP_AST_INVARIANT[exact-semantic-binding]: directive "
                 "has no exact C, C++, or Fortran language\n";
    ROSE_ABORT();
  }
}

struct SelectedFortranExactSemanticExpression {
  const std::vector<OmpFortranExactSemanticBindings::Binding> *identifiers;
  const std::vector<OmpExactSubexpressionType> *subexpressions;
};

std::string
canonicalFortranExpressionCallbackSpelling(const std::string &spelling) {
  std::string canonical;
  canonical.reserve(spelling.size());
  char quote = '\0';
  for (std::size_t index = 0; index < spelling.size(); ++index) {
    const unsigned char character = static_cast<unsigned char>(spelling[index]);
    if (quote != '\0') {
      canonical.push_back(static_cast<char>(character));
      if (character == static_cast<unsigned char>(quote)) {
        if (index + 1 < spelling.size() && spelling[index + 1] == quote) {
          canonical.push_back(spelling[++index]);
        } else {
          quote = '\0';
        }
      }
      continue;
    }
    if (character == '\'' || character == '"') {
      quote = static_cast<char>(character);
      canonical.push_back(quote);
    } else if (!std::isspace(character)) {
      canonical.push_back(static_cast<char>(std::tolower(character)));
    }
  }
  if (quote != '\0') {
    std::cerr << "REX_OMP_AST_INVARIANT[fortran-exact-expression-callback]: "
                 "expression callback has an unterminated character literal\n";
    ROSE_ABORT();
  }
  return canonical;
}

SelectedFortranExactSemanticExpression
selectFortranExactSemanticExpressionBindings(
    FortranExactSemanticConsumptionState *state, const std::string &expression,
    const char *consumer) {
  if (state == nullptr || state->pragma == nullptr ||
      state->bindings == nullptr || expression.empty() || consumer == nullptr ||
      state->bindings->producer() !=
          OmpFortranExactSemanticBindings::Producer::flang_parse_tree ||
      state->binding_consumed.size() != state->bindings->bindings().size() ||
      state->expression_consumed.size() !=
          state->bindings->expressions().size()) {
    std::cerr
        << "REX_OMP_AST_INVARIANT[fortran-exact-semantic-binding]: invalid "
           "Fortran expression binding request\n";
    ROSE_ABORT();
  }

  const auto &bindings = state->bindings->bindings();
  const auto &expressions = state->bindings->expressions();
  const std::string canonicalCallback =
      canonicalFortranExpressionCallbackSpelling(expression);
  std::size_t selectedExpressionIndex = std::string::npos;
  for (std::size_t index = 0; index < expressions.size(); ++index) {
    if (state->expression_consumed[index]) {
      continue;
    }
    const auto &candidate = expressions[index];
    if (candidate.sourceOffset() < state->source_cursor) {
      std::cerr << "REX_OMP_AST_INVARIANT[fortran-exact-expression-stale]: "
                   "producer expression '"
                << candidate.expression() << "' precedes the next " << consumer
                << " expression callback\n";
      ROSE_ABORT();
    }
    if (canonicalFortranExpressionCallbackSpelling(candidate.expression()) !=
        canonicalCallback) {
      std::cerr << "REX_OMP_AST_INVARIANT[fortran-exact-expression-callback]: "
                   "next producer expression '"
                << candidate.expression() << "' does not match callback '"
                << expression << "'\n";
      ROSE_ABORT();
    }
    selectedExpressionIndex = index;
    break;
  }
  if (selectedExpressionIndex == std::string::npos) {
    std::cerr << "REX_OMP_AST_INVARIANT[fortran-exact-expression-callback]: "
                 "callback expression '"
              << expression << "' has no unconsumed Flang producer record\n";
    ROSE_ABORT();
  }

  const auto &selectedExpression = expressions[selectedExpressionIndex];
  const std::size_t selected = selectedExpression.sourceOffset();
  const std::size_t selectedEnd = selected + selectedExpression.sourceSize();
  std::vector<std::size_t> selectedBindings;
  for (std::size_t index = 0; index < bindings.size(); ++index) {
    const auto &binding = bindings[index];
    const std::size_t bindingEnd =
        binding.sourceOffset() + binding.sourceSize();
    const bool overlaps =
        binding.sourceOffset() < selectedEnd && bindingEnd > selected;
    const bool isContained =
        binding.sourceOffset() >= selected && bindingEnd <= selectedEnd;
    if (overlaps && !isContained) {
      std::cerr << "REX_OMP_AST_INVARIANT[fortran-exact-semantic-range]: "
                   "expression boundary splits identifier '"
                << binding.spelling() << "'\n";
      ROSE_ABORT();
    }
    if (isContained) {
      if (state->binding_consumed[index]) {
        std::cerr << "REX_OMP_AST_INVARIANT[fortran-exact-semantic-duplicate]: "
                     "identifier '"
                  << binding.spelling() << "' was consumed more than once\n";
        ROSE_ABORT();
      }
      selectedBindings.push_back(index);
    }
  }

  for (std::size_t index = 0; index < bindings.size(); ++index) {
    if (!state->binding_consumed[index] &&
        bindings[index].sourceOffset() < selected) {
      std::cerr << "REX_OMP_AST_INVARIANT[fortran-exact-semantic-stale]: "
                   "producer identifier '"
                << bindings[index].spelling()
                << "' was skipped before the next expression callback\n";
      ROSE_ABORT();
    }
  }

  state->active_bindings.clear();
  state->active_bindings.reserve(selectedBindings.size());
  for (std::size_t index : selectedBindings) {
    state->binding_consumed[index] = true;
    state->active_bindings.push_back(bindings[index]);
  }
  state->source_cursor = selectedEnd;
  state->expression_consumed[selectedExpressionIndex] = true;
  return {&state->active_bindings, &selectedExpression.subexpressions()};
}

const OmpFortranExactSemanticBindings::Binding &
consumeFortranExactSemanticNameBinding(
    FortranExactSemanticConsumptionState *state, const std::string &name,
    OmpFortranExactSemanticBindings::BindingKind expectedKind,
    const char *consumer) {
  if (state == nullptr || state->pragma == nullptr ||
      state->bindings == nullptr || name.empty() || consumer == nullptr ||
      state->bindings->producer() !=
          OmpFortranExactSemanticBindings::Producer::flang_parse_tree ||
      state->binding_consumed.size() != state->bindings->bindings().size()) {
    std::cerr << "REX_OMP_AST_INVARIANT[fortran-exact-name-binding]: invalid "
                 "Fortran name binding request\n";
    ROSE_ABORT();
  }

  const std::string canonicalName =
      canonicalFortranExpressionCallbackSpelling(name);
  const auto &bindings = state->bindings->bindings();
  std::size_t selected = std::string::npos;
  for (std::size_t index = 0; index < bindings.size(); ++index) {
    const auto &binding = bindings[index];
    if (!state->binding_consumed[index] && binding.kind() == expectedKind &&
        canonicalFortranExpressionCallbackSpelling(binding.spelling()) ==
            canonicalName) {
      if (selected != std::string::npos) {
        std::cerr << "REX_OMP_AST_INVARIANT[fortran-exact-name-binding]: "
                  << consumer << " name '" << name
                  << "' has multiple unconsumed Flang identities\n";
        ROSE_ABORT();
      }
      selected = index;
    }
  }
  if (selected == std::string::npos) {
    std::cerr << "REX_OMP_AST_INVARIANT[fortran-exact-name-binding]: "
              << consumer << " name '" << name
              << "' has no exact Flang identity\n";
    ROSE_ABORT();
  }

  state->binding_consumed[selected] = true;
  return bindings[selected];
}

void requireCompleteFortranExactSemanticConsumption(
    const FortranExactSemanticConsumptionState *state, const char *consumer) {
  if (state == nullptr || state->pragma == nullptr ||
      state->bindings == nullptr || consumer == nullptr ||
      state->expression_active ||
      state->binding_consumed.size() != state->bindings->bindings().size() ||
      state->expression_consumed.size() !=
          state->bindings->expressions().size()) {
    std::cerr << "REX_OMP_AST_INVARIANT[fortran-exact-semantic-binding]: "
                 "invalid or incomplete Fortran semantic-consumption state\n";
    ROSE_ABORT();
  }
  const auto &bindings = state->bindings->bindings();
  for (std::size_t index = 0; index < bindings.size(); ++index) {
    if (!state->binding_consumed[index]) {
      std::cerr << "REX_OMP_AST_INVARIANT[fortran-exact-semantic-stale]: "
                   "producer identifier '"
                << bindings[index].spelling() << "' was not consumed by the "
                << consumer << " expression callback\n";
      ROSE_ABORT();
    }
  }
  const auto &expressions = state->bindings->expressions();
  for (std::size_t index = 0; index < expressions.size(); ++index) {
    if (!state->expression_consumed[index]) {
      std::cerr << "REX_OMP_AST_INVARIANT[fortran-exact-expression-stale]: "
                   "Flang subexpression-type record '"
                << expressions[index].expression()
                << "' was not consumed by the " << consumer
                << " expression callback\n";
      ROSE_ABORT();
    }
  }
}

void finishOpenMPExactSemanticBindings(OmpExprParseContext *context) {
  ROSE_ASSERT(context != nullptr);
  ROSE_ASSERT(context->directive != nullptr);
  if (context->directive->getBaseLang() == Lang_Fortran) {
    if (context->fortran_semantics.bindings == nullptr) {
      std::cerr
          << "REX_OMP_AST_INVARIANT[fortran-exact-semantic-binding]: parser "
             "has no active Flang binding record\n";
      ROSE_ABORT();
    }
    requireCompleteFortranExactSemanticConsumption(&context->fortran_semantics,
                                                   "OpenMP");
    return;
  }
}

static bool clauseExpressionIsOpenMPGrammarName(OpenMPClauseKind clause_kind) {
  switch (clause_kind) {
  case OMPC_threadset:
  case OMPC_transparent:
    return true;
  default:
    return false;
  }
}

void requireOpenMPNonnegativeConstantInteger(SgExpression *expression,
                                             const std::string &spelling) {
  SgType *type = expression != nullptr ? expression->get_type() : nullptr;
  if (type == nullptr ||
      (!SageInterface::isStrictIntegerType(type) &&
       isSgEnumType(type->stripType(SgType::STRIP_MODIFIER_TYPE |
                                    SgType::STRIP_TYPEDEF_TYPE)) == nullptr)) {
    std::cerr << "REX_OMP_SEMANTIC[variant-score]: score '" << spelling
              << "' is not an integer expression\n";
    ROSE_ABORT();
  }
  if (!Rose::OpenMP::isNonnegativeConstantInteger(expression)) {
    std::cerr << "REX_OMP_SEMANTIC[variant-score]: score '" << spelling
              << "' is not a non-negative constant integer expression\n";
    ROSE_ABORT();
  }
}

std::shared_ptr<const ompparser::HostSemanticNode>
parseOpenMPHostFragment(const ompparser::HostFragment &fragment,
                        OmpExprParseContext *context) {
  const OpenMPClauseKind clause_kind = fragment.clause_kind;
  const OpenMPExprParseMode parse_mode = fragment.parse_mode;
  const char *expression = fragment.spelling.c_str();
  ROSE_ASSERT(context != nullptr);
  ROSE_ASSERT(context->pragma_declaration != nullptr);
  ROSE_ASSERT(context->directive != nullptr);
  if (expression == nullptr) {
    std::cerr << "REX_OMP_AST_INVARIANT[expression-callback]: null source "
                 "expression\n";
    ROSE_ABORT();
  }

  auto parsed = std::make_shared<OmpParsedExpression>();
  parsed->mode = parse_mode;
  parsed->text = expression;
  if (context->capture_source_text_only) {
    context->owned_nodes.push_back(parsed);
    return parsed;
  }

  const bool is_fortran = context->directive->getBaseLang() == Lang_Fortran;
  auto begin_exact = [&]() {
    if (is_fortran) {
      if (context->fortran_semantics.bindings->producer() ==
          OmpFortranExactSemanticBindings::Producer::rex_typed_scope) {
        SgScopeStatement *scope = context->pragma_declaration->get_scope();
        if (scope == nullptr ||
            context->pragma_declaration->get_parent() != scope) {
          std::cerr << "REX_OMP_AST_INVARIANT[fortran-typed-scope]: "
                       "REX-owned directive has no exact lexical Sage scope\n";
          ROSE_ABORT();
        }
        omp_exprparser_begin_fortran_typed_scope_semantics(
            scope, context->fortran_semantics.bindings->defaultIntegerType());
      } else {
        const auto fortranBindings =
            selectFortranExactSemanticExpressionBindings(
                &context->fortran_semantics, parsed->text, "OpenMP");
        omp_exprparser_begin_fortran_exact_semantic_bindings(
            fortranBindings.identifiers, fortranBindings.subexpressions,
            context->fortran_semantics.bindings->defaultIntegerType());
      }
    }
  };
  auto end_exact = [&]() {
    if (is_fortran) {
      if (context->fortran_semantics.bindings->producer() ==
          OmpFortranExactSemanticBindings::Producer::rex_typed_scope) {
        omp_exprparser_end_fortran_typed_scope_semantics();
      } else {
        omp_exprparser_end_fortran_exact_semantic_bindings();
      }
    }
  };

  if (parse_mode == OMP_EXPR_PARSE_openmp_declare_mapper_identifier) {
    const std::string name = trimWhitespaceCopy(parsed->text);
    if (!OmpSupport::isSimpleMapperIdentifier(name)) {
      std::cerr << "REX_OMP_AST_INVARIANT[declare-mapper-identifier]: "
                   "invalid mapper identifier '"
                << parsed->text << "'\n";
      ROSE_ABORT();
    }
    parsed->node = buildOpenMPNameExpression(name);
  } else if (parse_mode == OMP_EXPR_PARSE_openmp_declare_mapper_type) {
    if (context->declare_mapper_type == nullptr ||
        isSgTypeUnknown(context->declare_mapper_type) != nullptr) {
      std::cerr << "REX_OMP_AST_INVARIANT[declare-mapper-type]: callback has "
                   "no producer-validated type\n";
      ROSE_ABORT();
    }
    parsed->node =
        SageBuilder::buildTypeExpression(context->declare_mapper_type);
    setOneSourcePositionForTransformation(isSgLocatedNode(parsed->node));
  } else if (parse_mode == OMP_EXPR_PARSE_openmp_declare_mapper_variable) {
    const std::string name = trimWhitespaceCopy(parsed->text);
    if (!OmpSupport::isSimpleMapperIdentifier(name) ||
        context->directive_local_scope == nullptr) {
      std::cerr << "REX_OMP_AST_INVARIANT[declare-mapper-variable]: callback "
                   "has no valid directive-local variable context\n";
      ROSE_ABORT();
    }
    if (is_fortran) {
      const OmpFortranExactSemanticBindings::Producer producer =
          context->fortran_semantics.bindings->producer();
      if (producer ==
          OmpFortranExactSemanticBindings::Producer::flang_parse_tree) {
        const OmpFortranExactSemanticBindings::Binding &local =
            consumeFortranExactSemanticNameBinding(
                &context->fortran_semantics, name,
                OmpFortranExactSemanticBindings::BindingKind::
                    directive_local_declaration,
                "OpenMP declare mapper variable");
        if (canonicalFortranExpressionCallbackSpelling(local.spelling()) !=
                canonicalFortranExpressionCallbackSpelling(name) ||
            local.directiveLocalType() == nullptr ||
            local.directiveLocalType() != context->declare_mapper_type) {
          std::cerr << "REX_OMP_AST_INVARIANT[declare-mapper-variable]: exact "
                       "Fortran directive-local identity disagrees with the "
                       "typed mapper declaration\n";
          ROSE_ABORT();
        }
      } else if (producer !=
                 OmpFortranExactSemanticBindings::Producer::rex_typed_scope) {
        std::cerr << "REX_OMP_AST_INVARIANT[declare-mapper-variable]: "
                     "Fortran mapper variable has an invalid semantic "
                     "producer\n";
        ROSE_ABORT();
      }
    }
    SgVariableSymbol *symbol =
        context->directive_local_scope->lookup_variable_symbol(name);
    if (symbol == nullptr || symbol->get_declaration() == nullptr ||
        symbol->get_declaration()->get_scope() !=
            context->directive_local_scope ||
        symbol->get_declaration()->get_type() != context->declare_mapper_type) {
      std::cerr << "REX_OMP_AST_INVARIANT[declare-mapper-variable]: exact "
                   "directive-local symbol is missing or has the wrong type\n";
      ROSE_ABORT();
    }
    parsed->node = SageBuilder::buildVarRefExp(symbol);
    setOneSourcePositionForTransformation(isSgLocatedNode(parsed->node));
  } else if (parse_mode == OMP_EXPR_PARSE_openmp_iterator_type) {
    const std::string type_spelling = trimWhitespaceCopy(parsed->text);
    if (context->pending_iterator_type != nullptr) {
      std::cerr << "REX_OMP_AST_INVARIANT[iterator-type]: iterator type '"
                << parsed->text
                << "' arrived before the preceding type acquired a name\n";
      ROSE_ABORT();
    }
    SgType *type =
        resolveDeclareMapperType(context->pragma_declaration, type_spelling);
    if (type == nullptr || isSgTypeUnknown(type) != nullptr ||
        isSgTypeDefault(type) != nullptr) {
      std::cerr << "REX_OMP_AST_INVARIANT[iterator-type]: cannot resolve '"
                << parsed->text << "'\n";
      ROSE_ABORT();
    }
    context->pending_iterator_type = type;
    parsed->node = SageBuilder::buildTypeExpression(type);
    setOneSourcePositionForTransformation(isSgLocatedNode(parsed->node));
  } else if (parse_mode == OMP_EXPR_PARSE_openmp_iterator_name) {
    const std::string name = trimWhitespaceCopy(parsed->text);
    if (!OmpSupport::isSimpleMapperIdentifier(name)) {
      std::cerr << "REX_OMP_AST_INVARIANT[iterator-name]: invalid OpenMP "
                   "iterator identity '"
                << parsed->text << "'\n";
      ROSE_ABORT();
    }

    SgType *directiveLocalType = nullptr;
    if (is_fortran) {
      const OmpFortranExactSemanticBindings::Producer producer =
          context->fortran_semantics.bindings->producer();
      if (producer ==
          OmpFortranExactSemanticBindings::Producer::flang_parse_tree) {
        const OmpFortranExactSemanticBindings::Binding &local =
            consumeFortranExactSemanticNameBinding(
                &context->fortran_semantics, name,
                OmpFortranExactSemanticBindings::BindingKind::
                    directive_local_declaration,
                "OpenMP iterator");
        directiveLocalType = local.directiveLocalType();
        if (local.kind() != OmpFortranExactSemanticBindings::BindingKind::
                                directive_local_declaration ||
            canonicalFortranExpressionCallbackSpelling(local.spelling()) !=
                canonicalFortranExpressionCallbackSpelling(name) ||
            directiveLocalType == nullptr ||
            (context->pending_iterator_type != nullptr &&
             context->pending_iterator_type != directiveLocalType)) {
          std::cerr
              << "REX_OMP_AST_INVARIANT[iterator-name]: Fortran iterator '"
              << name << "' does not match its exact frontend-declared type\n";
          ROSE_ABORT();
        }
      } else if (producer ==
                 OmpFortranExactSemanticBindings::Producer::rex_typed_scope) {
        directiveLocalType =
            context->pending_iterator_type != nullptr
                ? context->pending_iterator_type
                : context->fortran_semantics.bindings->defaultIntegerType();
        if (directiveLocalType == nullptr ||
            isSgTypeUnknown(directiveLocalType) != nullptr ||
            isSgTypeDefault(directiveLocalType) != nullptr) {
          std::cerr << "REX_OMP_AST_INVARIANT[iterator-name]: REX-owned "
                       "Fortran iterator has no exact typed-scope type\n";
          ROSE_ABORT();
        }
      } else {
        std::cerr << "REX_OMP_AST_INVARIANT[iterator-name]: Fortran iterator "
                     "has an invalid semantic producer\n";
        ROSE_ABORT();
      }
    } else {
      directiveLocalType = context->pending_iterator_type != nullptr
                               ? context->pending_iterator_type
                               : SageBuilder::buildIntType();
    }
    SgVariableSymbol *symbol = publishOpenMPDirectiveLocalVariable(
        context, name, directiveLocalType, "iterator");
    omp_exprparser_add_context_variable_symbol(symbol);
    context->pending_iterator_type = nullptr;
    parsed->node = buildOpenMPNameExpression(name);
  } else if (parse_mode == OMP_EXPR_PARSE_expression ||
             parse_mode == OMP_EXPR_PARSE_constant_integer) {
    if (clauseExpressionIsOpenMPGrammarName(clause_kind)) {
      parsed->node = buildOpenMPNameExpression(parsed->text);
    } else {
      begin_exact();
      parseAndStoreExpression(parsed->text, parsed.get(),
                              context->pragma_declaration, context->directive,
                              clause_kind);
      end_exact();
      if (parse_mode == OMP_EXPR_PARSE_constant_integer) {
        requireOpenMPNonnegativeConstantInteger(isSgExpression(parsed->node),
                                                parsed->text);
      }
    }
  } else if (parse_mode == OMP_EXPR_PARSE_variable_list) {
    begin_exact();
    parseAndStoreVariableList(parsed->text, parsed.get(),
                              context->pragma_declaration, context->directive,
                              clause_kind);
    end_exact();
  } else if (parse_mode == OMP_EXPR_PARSE_array_section) {
    begin_exact();
    parseAndStoreArraySection(parsed->text, parsed.get(),
                              context->pragma_declaration, context->directive,
                              clause_kind);
    end_exact();
  } else if (parse_mode == OMP_EXPR_PARSE_openmp_context_name) {
    const std::string token = trimWhitespaceCopy(parsed->text);
    const bool double_quoted =
        token.size() >= 2 && token.front() == '"' && token.back() == '"';
    const bool single_quoted =
        token.size() >= 2 && token.front() == '\'' && token.back() == '\'';
    if (double_quoted || single_quoted) {
      if (context->directive->getBaseLang() != Lang_Fortran && single_quoted) {
        std::cerr << "REX_OMP_AST_INVARIANT[context-name]: C/C++ name-list "
                     "property uses a character-literal delimiter\n";
        ROSE_ABORT();
      }
      parsed->node = buildOpenMPSourceExpression(token);
    } else {
      if (!OmpSupport::isSimpleMapperIdentifier(token)) {
        std::cerr << "REX_OMP_AST_INVARIANT[context-name]: name-list property "
                     "is not one exact OpenMP identifier\n";
        ROSE_ABORT();
      }
      parsed->node = buildOpenMPNameExpression(token);
    }
  } else if (parse_mode == OMP_EXPR_PARSE_openmp_source) {
    parsed->node = buildOpenMPSourceExpression(parsed->text);
  } else if (parse_mode == OMP_EXPR_PARSE_openmp_syntax) {
    // OpenMP grammar-only spellings are validated and owned by ompparser.  A
    // Sage host-language node would give labels and transformation names a
    // false value/declaration identity, so the exact callback record is the
    // typed semantic payload for this mode.
    if (trimWhitespaceCopy(parsed->text).empty()) {
      std::cerr << "REX_OMP_AST_INVARIANT[openmp-syntax]: empty OpenMP "
                   "grammar payload\n";
      ROSE_ABORT();
    }
  } else if (parse_mode == OMP_EXPR_PARSE_verbatim) {
    parsed->node = buildOpenMPSyntaxTokenExpression(
        parsed->text, context->directive->getBaseLang());
  } else {
    std::cerr << "REX_OMP_AST_INVARIANT[expression-callback]: unsupported "
                 "parse mode "
              << static_cast<int>(parse_mode) << "\n";
    ROSE_ABORT();
  }

  if (parsed->node == nullptr && parse_mode != OMP_EXPR_PARSE_openmp_syntax) {
    std::cerr << "REX_OMP_AST_INVARIANT[expression-callback]: semantic parse "
                 "produced no AST node for '"
              << parsed->text << "'\n";
    ROSE_ABORT();
  }

  context->owned_nodes.push_back(parsed);
  return parsed;
}

bool mapperClassHasVirtualBase(
    SgClassDeclaration *declaration,
    std::unordered_set<SgClassDeclaration *> &visited) {
  if (declaration == nullptr) {
    std::cerr << "REX_OMP_AST_INVARIANT[declare-mapper-virtual-base]: null "
                 "mapper class declaration\n";
    ROSE_ABORT();
  }
  SgClassDeclaration *defining =
      isSgClassDeclaration(declaration->get_definingDeclaration());
  if (defining == nullptr || defining->get_definition() == nullptr) {
    std::cerr << "REX_OMP_AST_INVARIANT[declare-mapper-virtual-base]: mapper "
                 "class has no defining declaration and definition\n";
    ROSE_ABORT();
  }
  if (!visited.insert(defining).second) {
    return false;
  }

  for (SgBaseClass *base : defining->get_definition()->get_inheritances()) {
    if (base == nullptr || base->get_baseClassModifier() == nullptr ||
        base->get_base_class() == nullptr) {
      std::cerr << "REX_OMP_AST_INVARIANT[declare-mapper-virtual-base]: "
                   "malformed mapper base-class edge\n";
      ROSE_ABORT();
    }
    if (base->get_baseClassModifier()->isVirtual()) {
      return true;
    }
    if (mapperClassHasVirtualBase(base->get_base_class(), visited)) {
      return true;
    }
  }
  return false;
}

SgDeclarationScope *
buildDeclareMapperLocalScope(SgPragmaDeclaration *pragma_declaration,
                             OpenMPDeclareMapperDirective *mapper_directive,
                             SgType *mapper_type) {
  ROSE_ASSERT(pragma_declaration != nullptr);
  ROSE_ASSERT(mapper_directive != nullptr);
  std::vector<OpenMPClause *> *mapper_clauses =
      mapper_directive->getClausesInOriginalOrder();
  ROSE_ASSERT(mapper_clauses != nullptr);
  if (std::none_of(mapper_clauses->begin(), mapper_clauses->end(),
                   [](OpenMPClause *clause) {
                     return clause != nullptr && clause->getKind() == OMPC_map;
                   })) {
    std::cerr << "REX_OMP_SEMANTIC[declare-mapper-map-clause]: declare mapper "
                 "requires at least one map clause\n";
    ROSE_ABORT();
  }
  const NormalizedDeclareMapperData mapper_data =
      normalizeDeclareMapperData(mapper_directive);
  if (mapper_data.mapper_type.empty() || mapper_data.mapper_variable.empty() ||
      !isSimpleMapperIdentifier(mapper_data.mapper_variable)) {
    std::cerr << "REX_OMP_AST_INVARIANT[declare-mapper-context]: mapper type "
                 "or variable is incomplete\n";
    ROSE_ABORT();
  }
  if (mapper_type == nullptr || isSgTypeUnknown(mapper_type) != nullptr) {
    std::cerr << "REX_OMP_AST_INVARIANT[declare-mapper-context]: producer "
                 "supplied no exact mapper type for '"
              << mapper_data.mapper_type << "'\n";
    ROSE_ABORT();
  }

  switch (mapper_directive->getBaseLang()) {
  case Lang_C:
  case Lang_Cplusplus: {
    SgType *base_type = mapper_type->stripType(SgType::STRIP_MODIFIER_TYPE |
                                               SgType::STRIP_TYPEDEF_TYPE);
    SgClassType *class_type = isSgClassType(base_type);
    if (class_type == nullptr) {
      if (mapper_directive->getBaseLang() == Lang_Cplusplus) {
        std::cerr << "REX_OMP_SEMANTIC[declare-mapper-type]: C++ mapper type "
                     "must be a struct, union, or class type\n";
      } else {
        std::cerr << "REX_OMP_SEMANTIC[declare-mapper-type]: C mapper type "
                     "must be a struct or union type\n";
      }
      ROSE_ABORT();
    }
    if (mapper_directive->getBaseLang() == Lang_Cplusplus) {
      std::unordered_set<SgClassDeclaration *> visited;
      if (mapperClassHasVirtualBase(
              isSgClassDeclaration(class_type->get_declaration()), visited)) {
        std::cerr << "REX_OMP_SEMANTIC[declare-mapper-virtual-base]: C++ "
                     "mapper type must not be derived from a virtual base "
                     "class\n";
        ROSE_ABORT();
      }
    }
    break;
  }
  case Lang_Fortran: {
    SgType *base_type = mapper_type->stripType(SgType::STRIP_MODIFIER_TYPE |
                                               SgType::STRIP_TYPEDEF_TYPE);
    if (isSgClassType(base_type) == nullptr) {
      std::cerr << "REX_OMP_SEMANTIC[declare-mapper-type]: Fortran mapper "
                   "type must be a non-parameterized derived type\n";
      ROSE_ABORT();
    }
    break;
  }
  case Lang_unknown:
    std::cerr << "REX_OMP_AST_INVARIANT[declare-mapper-language]: mapper has "
                 "no exact base-language identity\n";
    ROSE_ABORT();
  }

  SgDeclarationScope *local_scope = SageBuilder::buildDeclarationScope();
  ROSE_ASSERT(local_scope != nullptr);
  SgScopeStatement *directive_scope = pragma_declaration->get_scope();
  if (directive_scope == nullptr) {
    std::cerr << "REX_OMP_AST_INVARIANT[declare-mapper-context]: pragma has "
                 "no semantic scope\n";
    ROSE_ABORT();
  }
  local_scope->setCaseInsensitive(directive_scope->isCaseInsensitive());
  // The mapper statement adopts this scope after the parser has consumed the
  // directive. Keep it detached during parsing so it never has to be removed
  // from a temporary owner. Exact external bindings are supplied separately
  // by OmpExprParseContext; the only symbol published here is the mapper's
  // local variable.
  SgVariableDeclaration *declaration =
      SageBuilder::buildSemanticAuxiliaryVariableDeclaration(
          mapper_data.mapper_variable, mapper_type, nullptr, local_scope);
  ROSE_ASSERT(declaration != nullptr);
  SgAuxiliaryDeclarationList *auxiliary =
      local_scope->get_auxiliary_declarations();
  if (auxiliary == nullptr || auxiliary->get_parent() != local_scope ||
      declaration->get_parent() != auxiliary ||
      std::count(auxiliary->get_declarations().begin(),
                 auxiliary->get_declarations().end(), declaration) != 1 ||
      local_scope->statementExistsInScope(declaration)) {
    std::cerr << "REX_OMP_AST_INVARIANT[declare-mapper-context]: mapper "
                 "variable has no exact semantic auxiliary owner\n";
    ROSE_ABORT();
  }
  SgVariableSymbol *symbol =
      local_scope->lookup_variable_symbol(mapper_data.mapper_variable);
  if (symbol == nullptr || symbol->get_declaration() == nullptr ||
      symbol->get_declaration()->get_type() != mapper_type) {
    std::cerr << "REX_OMP_AST_INVARIANT[declare-mapper-context]: mapper "
                 "variable has no exact typed symbol\n";
    ROSE_ABORT();
  }
  return local_scope;
}

std::string normalizeOpenMPDirectiveForParser(const std::string &text,
                                              OpenMPBaseLang base_lang) {
  std::string normalized = text;
  if (base_lang != Lang_Fortran) {
    return normalized;
  }

  const std::string trimmed = trimWhitespaceCopy(normalized);
  std::string lowered = trimmed;
  std::transform(
      lowered.begin(), lowered.end(), lowered.begin(),
      [](unsigned char ch) { return static_cast<char>(std::tolower(ch)); });
  if (lowered.rfind("#pragma", 0) == 0) {
    const std::string::size_type omp_pos = lowered.find("omp");
    ROSE_ASSERT(omp_pos != std::string::npos);
    return "!$" + trimmed.substr(omp_pos);
  }
  if (lowered.rfind("!$omp", 0) == 0 || lowered.rfind("c$omp", 0) == 0 ||
      lowered.rfind("*$omp", 0) == 0) {
    return trimmed;
  }
  if (lowered.rfind("omp", 0) == 0) {
    return "!$" + trimmed;
  }
  return "!$omp " + trimmed;
}

} // namespace

namespace OmpSupport {

namespace {
void requireCxxOpenACCSemanticPragma(SgPragmaDeclaration *pragma,
                                     const char *operation) {
  SgSourceFile *source = pragma != nullptr
                             ? SageInterface::getEnclosingSourceFile(pragma)
                             : nullptr;
  const bool isCxx = source != nullptr &&
                     (source->get_C_only() || source->get_Cxx_only() ||
                      source->get_Cuda_only() || source->get_OpenCL_only());
  if (pragma == nullptr || operation == nullptr || !isCxx ||
      pragma->get_fortran_directive_family() !=
          SgPragmaDeclaration::e_fortran_directive_none) {
    std::cerr << "REX_ACC_AST_INVARIANT[cxx-semantic-consumption]: "
              << (operation != nullptr ? operation : "<null>")
              << " has no exact C/C++ OpenACC pragma owner\n";
    ROSE_ABORT();
  }
}

void requireFortranOpenACCSemanticPragma(SgPragmaDeclaration *pragma,
                                         const char *operation) {
  SgSourceFile *source = pragma != nullptr
                             ? SageInterface::getEnclosingSourceFile(pragma)
                             : nullptr;
  const bool isFortran =
      source != nullptr && (source->get_Fortran_only() ||
                            source->get_F77_only() || source->get_F90_only() ||
                            source->get_F95_only() || source->get_F2003_only());
  if (pragma == nullptr || operation == nullptr || !isFortran ||
      pragma->get_fortran_directive_family() !=
          SgPragmaDeclaration::e_fortran_directive_openacc) {
    std::cerr << "REX_ACC_AST_INVARIANT[fortran-semantic-consumption]: "
              << (operation != nullptr ? operation : "<null>")
              << " has no exact Fortran OpenACC pragma owner\n";
    ROSE_ABORT();
  }
}
} // namespace

void beginOpenACCCxxExactSemanticConsumption(SgPragmaDeclaration *pragma) {
  requireCxxOpenACCSemanticPragma(pragma, "begin directive");
  auto &active = openMPConversionState().openacc_cxx_semantic_consumption;
  if (active.has_value()) {
    std::cerr << "REX_ACC_AST_INVARIANT[cxx-semantic-consumption]: nested "
                 "OpenACC semantic-consumption transaction\n";
    ROSE_ABORT();
  }
  auto record = openACCCxxExactSemanticBindings().find(pragma);
  if (record == openACCCxxExactSemanticBindings().end()) {
    std::cerr << "REX_ACC_AST_INVARIANT[cxx-semantic-consumption]: OpenACC "
                 "pragma has no invocation-owned C/C++ frontend binding "
                 "record\n";
    ROSE_ABORT();
  }
  active.emplace();
  active->pragma = pragma;
  active->bindings = &record->second;
}

void beginOpenACCCxxExactSemanticExpression(SgPragmaDeclaration *pragma,
                                            const std::string &expression,
                                            OpenMPExprParseMode parse_mode) {
  requireCxxOpenACCSemanticPragma(pragma, "begin expression");
  auto &active = openMPConversionState().openacc_cxx_semantic_consumption;
  if (!active.has_value() || active->pragma != pragma ||
      active->bindings == nullptr || active->expression_active ||
      active->binding_index >= active->bindings->bindings().size()) {
    std::cerr << "REX_ACC_AST_INVARIANT[cxx-semantic-consumption]: OpenACC "
                 "expression has no unique active producer record\n";
    ROSE_ABORT();
  }
  const OpenACCCxxExactSemanticBindings::ExpressionBindings &selected =
      active->bindings->bindings()[active->binding_index++];
  if (selected.parseMode() != parse_mode ||
      selected.expression() != expression) {
    std::cerr << "REX_ACC_AST_INVARIANT[cxx-semantic-consumption]: consumer "
                 "expression does not match its producer record\n";
    ROSE_ABORT();
  }
  omp_exprparser_begin_openacc_cxx_semantic_bindings(&selected);
  active->expression_active = true;
}

void endOpenACCCxxExactSemanticExpression(SgPragmaDeclaration *pragma) {
  requireCxxOpenACCSemanticPragma(pragma, "end expression");
  auto &active = openMPConversionState().openacc_cxx_semantic_consumption;
  if (!active.has_value() || active->pragma != pragma ||
      !active->expression_active) {
    std::cerr << "REX_ACC_AST_INVARIANT[cxx-semantic-consumption]: OpenACC "
                 "expression transaction is unbalanced\n";
    ROSE_ABORT();
  }
  omp_exprparser_end_openacc_cxx_semantic_bindings();
  active->expression_active = false;
}

void finishOpenACCCxxExactSemanticConsumption(SgPragmaDeclaration *pragma) {
  requireCxxOpenACCSemanticPragma(pragma, "finish directive");
  auto &active = openMPConversionState().openacc_cxx_semantic_consumption;
  if (!active.has_value() || active->pragma != pragma ||
      active->bindings == nullptr || active->expression_active ||
      active->binding_index != active->bindings->bindings().size()) {
    std::cerr << "REX_ACC_AST_INVARIANT[cxx-semantic-consumption]: OpenACC "
                 "directive did not consume every exact Clang expression "
                 "record\n";
    ROSE_ABORT();
  }
  if (openACCCxxExactSemanticBindings().erase(pragma) != 1) {
    std::cerr << "REX_ACC_AST_INVARIANT[cxx-semantic-consumption]: completed "
                 "OpenACC directive lost its producer record\n";
    ROSE_ABORT();
  }
  active.reset();
}

void beginOpenACCFortranExactSemanticConsumption(SgPragmaDeclaration *pragma) {
  requireOpenMPConversionSession();
  requireFortranOpenACCSemanticPragma(pragma, "begin directive");
  auto &active = openMPConversionState().openacc_fortran_semantic_consumption;
  if (active.has_value()) {
    std::cerr << "REX_ACC_AST_INVARIANT[fortran-semantic-consumption]: "
                 "nested OpenACC semantic-consumption transaction\n";
    ROSE_ABORT();
  }
  auto record = openMPFortranExactSemanticBindings().find(pragma);
  if (record == openMPFortranExactSemanticBindings().end() ||
      record->second.directiveSource().empty()) {
    std::cerr << "REX_ACC_AST_INVARIANT[fortran-semantic-consumption]: "
                 "OpenACC pragma has no invocation-owned Flang binding "
                 "record\n";
    ROSE_ABORT();
  }
  active.emplace();
  active->pragma = pragma;
  active->bindings = &record->second;
  active->binding_consumed.assign(record->second.bindings().size(), false);
  active->expression_consumed.assign(record->second.expressions().size(),
                                     false);
}

void beginOpenACCFortranExactSemanticExpression(SgPragmaDeclaration *pragma,
                                                const std::string &expression) {
  requireFortranOpenACCSemanticPragma(pragma, "begin expression");
  auto &active = openMPConversionState().openacc_fortran_semantic_consumption;
  if (!active.has_value() || active->pragma != pragma ||
      active->expression_active) {
    std::cerr << "REX_ACC_AST_INVARIANT[fortran-semantic-consumption]: "
                 "OpenACC expression has no unique active directive "
                 "transaction\n";
    ROSE_ABORT();
  }
  const SelectedFortranExactSemanticExpression selected =
      selectFortranExactSemanticExpressionBindings(&*active, expression,
                                                   "OpenACC");
  omp_exprparser_begin_fortran_exact_semantic_bindings(
      selected.identifiers, selected.subexpressions,
      active->bindings->defaultIntegerType());
  active->expression_active = true;
}

void endOpenACCFortranExactSemanticExpression(SgPragmaDeclaration *pragma) {
  requireFortranOpenACCSemanticPragma(pragma, "end expression");
  auto &active = openMPConversionState().openacc_fortran_semantic_consumption;
  if (!active.has_value() || active->pragma != pragma ||
      !active->expression_active) {
    std::cerr << "REX_ACC_AST_INVARIANT[fortran-semantic-consumption]: "
                 "OpenACC expression transaction is unbalanced\n";
    ROSE_ABORT();
  }
  omp_exprparser_end_fortran_exact_semantic_bindings();
  active->expression_active = false;
}

void consumeOpenACCFortranExactSemanticSyntax(SgPragmaDeclaration *pragma,
                                              const std::string &spelling) {
  requireFortranOpenACCSemanticPragma(pragma, "consume syntax");
  auto &active = openMPConversionState().openacc_fortran_semantic_consumption;
  if (!active.has_value() || active->pragma != pragma ||
      active->expression_active) {
    std::cerr << "REX_ACC_AST_INVARIANT[fortran-semantic-consumption]: "
                 "OpenACC syntax has no unique active directive "
                 "transaction\n";
    ROSE_ABORT();
  }
  const SelectedFortranExactSemanticExpression selected =
      selectFortranExactSemanticExpressionBindings(&*active, spelling,
                                                   "OpenACC syntax");
  if (selected.subexpressions == nullptr || !selected.subexpressions->empty()) {
    std::cerr << "REX_ACC_AST_INVARIANT[fortran-semantic-consumption]: "
                 "grammar-owned OpenACC name has semantic expression "
                 "operations\n";
    ROSE_ABORT();
  }
}

void finishOpenACCFortranExactSemanticConsumption(SgPragmaDeclaration *pragma) {
  requireFortranOpenACCSemanticPragma(pragma, "finish directive");
  auto &active = openMPConversionState().openacc_fortran_semantic_consumption;
  if (!active.has_value() || active->pragma != pragma) {
    std::cerr << "REX_ACC_AST_INVARIANT[fortran-semantic-consumption]: "
                 "OpenACC directive transaction is not active\n";
    ROSE_ABORT();
  }
  requireCompleteFortranExactSemanticConsumption(&*active, "OpenACC");
  active.reset();
  if (openMPFortranExactSemanticBindings().erase(pragma) != 1) {
    std::cerr << "REX_ACC_AST_INVARIANT[fortran-semantic-consumption]: "
                 "completed OpenACC directive lost its producer record\n";
    ROSE_ABORT();
  }
}

void consumeOpenACCFortranExactSemanticEnd(SgPragmaDeclaration *pragma) {
  beginOpenACCFortranExactSemanticConsumption(pragma);
  finishOpenACCFortranExactSemanticConsumption(pragma);
}

} // namespace OmpSupport

static SgDeclarationStatement *
getOmpFunctionDirectiveSymbolDeclaration(SgSymbol *symbol) {
  if (SgTemplateMemberFunctionSymbol *typed =
          isSgTemplateMemberFunctionSymbol(symbol)) {
    return typed->get_declaration();
  }
  if (SgTemplateFunctionSymbol *typed = isSgTemplateFunctionSymbol(symbol)) {
    return typed->get_declaration();
  }
  if (SgMemberFunctionSymbol *typed = isSgMemberFunctionSymbol(symbol)) {
    return typed->get_declaration();
  }
  if (SgFunctionSymbol *typed = isSgFunctionSymbol(symbol)) {
    return typed->get_declaration();
  }
  return nullptr;
}

namespace {

bool isExactOpenMPValueSymbol(SgSymbol *symbol) {
  return isSgVariableSymbol(symbol) != nullptr ||
         isSgEnumFieldSymbol(symbol) != nullptr ||
         isSgFunctionSymbol(symbol) != nullptr ||
         isSgTemplateFunctionSymbol(symbol) != nullptr ||
         isSgMemberFunctionSymbol(symbol) != nullptr ||
         isSgTemplateMemberFunctionSymbol(symbol) != nullptr;
}

std::string foldOpenMPFortranSpelling(std::string spelling) {
  std::transform(
      spelling.begin(), spelling.end(), spelling.begin(),
      [](unsigned char ch) { return static_cast<char>(std::tolower(ch)); });
  return spelling;
}

template <class Record>
void requireOrderedDisjointOpenMPSourceRanges(
    const std::vector<Record> &records, const char *record_kind) {
  std::size_t previous_end = 0;
  bool has_previous = false;
  for (const Record &record : records) {
    if (record.sourceSize() == 0 ||
        (has_previous && record.sourceOffset() < previous_end)) {
      std::cerr << "REX_OMP_AST_INVARIANT[fortran-exact-semantic-range]: "
                << record_kind
                << " records contain an empty, duplicate, overlapping, or "
                   "out-of-order source span\n";
      ROSE_ABORT();
    }
    previous_end = record.sourceOffset() + record.sourceSize();
    has_previous = true;
  }
}

} // namespace

OmpExactSubexpressionType::OmpExactSubexpressionType(
    OmpExactSubexpressionKind kind, SgType *result_type)
    : kind_(kind), result_type_(result_type) {
  if (kind <= OmpExactSubexpressionKind::invalid ||
      kind > OmpExactSubexpressionKind::string_literal ||
      result_type == nullptr || isSgTypeUnknown(result_type) != nullptr ||
      isSgTypeDefault(result_type) != nullptr) {
    std::cerr << "REX_OMP_AST_INVARIANT[exact-subexpression-construction]: "
                 "record has no valid operation kind and exact Sage result "
                 "type\n";
    ROSE_ABORT();
  }
}

OpenACCCxxExactSemanticBindings::Binding::Binding(std::string spelling,
                                                  BindingKind kind,
                                                  SgNode *semantic_node,
                                                  SgSymbol *symbol)
    : spelling_(std::move(spelling)), kind_(kind),
      semantic_node_(semantic_node), symbol_(symbol) {
  bool valid = !spelling_.empty();
  switch (kind_) {
  case BindingKind::qualifier:
    valid =
        valid && semantic_node_ != nullptr &&
        (symbol_ == nullptr || symbol_->get_symbol_basis() == semantic_node_);
    break;
  case BindingKind::value:
    valid = valid && semantic_node_ != nullptr && symbol_ != nullptr &&
            isExactOpenMPValueSymbol(symbol_) &&
            symbol_->get_symbol_basis() == semantic_node_ &&
            symbol_->get_name().getString() == spelling_;
    break;
  case BindingKind::current_this:
    valid = valid && spelling_ == "this" && semantic_node_ == nullptr &&
            symbol_ == nullptr;
    break;
  case BindingKind::invalid:
  default:
    valid = false;
    break;
  }
  if (!valid) {
    std::cerr << "REX_ACC_AST_INVARIANT[cxx-semantic-construction]: "
                 "identifier binding has an invalid kind, spelling, or "
                 "node/symbol identity\n";
    ROSE_ABORT();
  }
}

OpenACCCxxExactSemanticBindings::ExpressionBindings::ExpressionBindings(
    OpenMPExprParseMode parse_mode, std::string expression,
    std::vector<Binding> identifiers,
    std::vector<OmpExactSubexpressionType> subexpressions)
    : parse_mode_(parse_mode), expression_(std::move(expression)),
      identifiers_(std::move(identifiers)),
      subexpressions_(std::move(subexpressions)) {
  const bool valid_mode = parse_mode_ == OMP_EXPR_PARSE_expression ||
                          parse_mode_ == OMP_EXPR_PARSE_variable_list;
  if (!valid_mode || expression_.empty()) {
    std::cerr << "REX_ACC_AST_INVARIANT[cxx-expression-construction]: "
                 "expression binding has an invalid mode or source payload\n";
    ROSE_ABORT();
  }
}

OpenACCCxxExactSemanticBindings::OpenACCCxxExactSemanticBindings(
    BindingSequence bindings)
    : bindings_(std::move(bindings)) {}

OmpFortranExactSemanticBindings::Binding::Binding(
    std::size_t source_offset, std::size_t source_size, std::string spelling,
    std::string source_spelling, BindingKind kind, SgNode *semantic_node,
    SgSymbol *symbol, SgType *directive_local_type)
    : source_offset_(source_offset), source_size_(source_size),
      spelling_(std::move(spelling)),
      source_spelling_(std::move(source_spelling)), kind_(kind),
      semantic_node_(semantic_node), symbol_(symbol),
      directive_local_type_(directive_local_type) {
  bool valid = source_size_ != 0 && !spelling_.empty() &&
               !source_spelling_.empty() &&
               foldOpenMPFortranSpelling(source_spelling_) ==
                   foldOpenMPFortranSpelling(spelling_);
  switch (kind_) {
  case BindingKind::value:
    valid = valid && semantic_node_ != nullptr && semantic_node_ == symbol_ &&
            isExactOpenMPValueSymbol(symbol_) &&
            foldOpenMPFortranSpelling(symbol_->get_name().getString()) ==
                foldOpenMPFortranSpelling(spelling_) &&
            directive_local_type_ == nullptr;
    break;
  case BindingKind::common_block:
    valid = valid && isSgCommonBlockObject(semantic_node_) != nullptr &&
            symbol_ == nullptr && directive_local_type_ == nullptr;
    break;
  case BindingKind::directive_local:
  case BindingKind::directive_local_declaration:
    valid = valid && semantic_node_ == nullptr && symbol_ == nullptr &&
            directive_local_type_ != nullptr &&
            isSgTypeUnknown(directive_local_type_) == nullptr &&
            isSgTypeDefault(directive_local_type_) == nullptr;
    break;
  case BindingKind::syntax_name:
    valid = valid && semantic_node_ == nullptr && symbol_ == nullptr &&
            directive_local_type_ == nullptr;
    break;
  default:
    valid = false;
    break;
  }
  if (!valid) {
    std::cerr
        << "REX_OMP_AST_INVARIANT[fortran-exact-binding-construction]: "
           "identifier binding has an invalid kind, source span, spelling, "
           "or node/symbol identity\n";
    ROSE_ABORT();
  }
}

OmpFortranExactSemanticBindings::ExpressionTypes::ExpressionTypes(
    std::size_t source_offset, std::size_t source_size, std::string expression,
    std::vector<OmpExactSubexpressionType> subexpressions)
    : source_offset_(source_offset), source_size_(source_size),
      expression_(std::move(expression)),
      subexpressions_(std::move(subexpressions)) {
  if (source_size_ == 0 || expression_.empty() ||
      expression_.size() != source_size_) {
    std::cerr
        << "REX_OMP_AST_INVARIANT[fortran-exact-expression-construction]: "
           "expression record has an invalid source span or payload\n";
    ROSE_ABORT();
  }
}

OmpFortranExactSemanticBindings::OmpFortranExactSemanticBindings(
    Producer producer, std::string directive_source,
    SgType *default_integer_type, std::vector<Binding> bindings,
    std::vector<ExpressionTypes> expressions)
    : producer_(producer), directive_source_(std::move(directive_source)),
      default_integer_type_(default_integer_type),
      bindings_(std::move(bindings)), expressions_(std::move(expressions)) {
  if (directive_source_.empty() || default_integer_type_ == nullptr ||
      isSgTypeUnknown(default_integer_type_) != nullptr ||
      isSgTypeDefault(default_integer_type_) != nullptr ||
      (producer_ == Producer::rex_typed_scope &&
       (!bindings_.empty() || !expressions_.empty())) ||
      (producer_ != Producer::flang_parse_tree &&
       producer_ != Producer::rex_typed_scope)) {
    std::cerr << "REX_OMP_AST_INVARIANT[fortran-exact-record-construction]: "
                 "binding record has an invalid producer, directive source, "
                 "default INTEGER type, or producer-owned payload\n";
    ROSE_ABORT();
  }
  requireOrderedDisjointOpenMPSourceRanges(bindings_, "identifier");
  requireOrderedDisjointOpenMPSourceRanges(expressions_, "expression");
  for (const Binding &binding : bindings_) {
    if (binding.sourceOffset() > directive_source_.size() ||
        binding.sourceSize() >
            directive_source_.size() - binding.sourceOffset() ||
        directive_source_.compare(binding.sourceOffset(), binding.sourceSize(),
                                  binding.spelling()) != 0) {
      std::cerr << "REX_OMP_AST_INVARIANT[fortran-exact-semantic-range]: "
                   "identifier span does not exactly name its directive "
                   "source payload\n";
      ROSE_ABORT();
    }
  }
  for (const ExpressionTypes &expression : expressions_) {
    if (expression.sourceOffset() > directive_source_.size() ||
        expression.sourceSize() >
            directive_source_.size() - expression.sourceOffset() ||
        directive_source_.compare(expression.sourceOffset(),
                                  expression.sourceSize(),
                                  expression.expression()) != 0) {
      std::cerr << "REX_OMP_AST_INVARIANT[fortran-exact-semantic-range]: "
                   "expression span does not exactly match its directive "
                   "source payload\n";
      ROSE_ABORT();
    }
  }
  for (const Binding &binding : bindings_) {
    std::size_t containing_expressions = 0;
    const std::size_t binding_end =
        binding.sourceOffset() + binding.sourceSize();
    for (const ExpressionTypes &expression : expressions_) {
      const std::size_t expression_end =
          expression.sourceOffset() + expression.sourceSize();
      const bool overlaps = binding.sourceOffset() < expression_end &&
                            binding_end > expression.sourceOffset();
      const bool contained =
          binding.sourceOffset() >= expression.sourceOffset() &&
          binding_end <= expression_end;
      if (overlaps && !contained) {
        std::cerr << "REX_OMP_AST_INVARIANT[fortran-exact-semantic-range]: "
                     "expression boundary splits an identifier span\n";
        ROSE_ABORT();
      }
      containing_expressions += contained ? 1 : 0;
    }
    const std::size_t expectedContainingExpressions =
        binding.kind() == BindingKind::directive_local_declaration ? 0 : 1;
    if (containing_expressions != expectedContainingExpressions) {
      std::cerr << "REX_OMP_AST_INVARIANT[fortran-exact-semantic-range]: "
                   "identifier span has the wrong typed expression "
                   "ownership cardinality\n";
      ROSE_ABORT();
    }
  }
}

namespace {

static SgSymbol *
getOmpFunctionDirectiveReferenceSymbol(SgExpression *reference) {
  if (SgTemplateMemberFunctionRefExp *typed =
          isSgTemplateMemberFunctionRefExp(reference)) {
    return typed->get_symbol();
  }
  if (SgTemplateFunctionRefExp *typed = isSgTemplateFunctionRefExp(reference)) {
    return typed->get_symbol();
  }
  if (SgMemberFunctionRefExp *typed = isSgMemberFunctionRefExp(reference)) {
    return typed->get_symbol();
  }
  if (SgFunctionRefExp *typed = isSgFunctionRefExp(reference)) {
    return typed->get_symbol();
  }
  return nullptr;
}

static SgExpression *buildOmpFunctionDirectiveReference(SgSymbol *symbol) {
  SgExpression *result = nullptr;
  if (SgTemplateMemberFunctionSymbol *typed =
          isSgTemplateMemberFunctionSymbol(symbol)) {
    result =
        SageBuilder::buildTemplateMemberFunctionRefExp_nfi(typed, false, false);
  } else if (SgTemplateFunctionSymbol *typed =
                 isSgTemplateFunctionSymbol(symbol)) {
    result = SageBuilder::buildTemplateFunctionRefExp_nfi(typed);
  } else if (SgMemberFunctionSymbol *typed = isSgMemberFunctionSymbol(symbol)) {
    result = SageBuilder::buildMemberFunctionRefExp_nfi(typed, false, false);
  } else if (SgFunctionSymbol *typed = isSgFunctionSymbol(symbol)) {
    result = SageBuilder::buildFunctionRefExp_nfi(typed);
  }
  if (result != nullptr) {
    setOneSourcePositionForTransformation(result);
  }
  return result;
}

static ResolvedOmpFunctionDirectiveTarget *
requireResolvedOmpFunctionDirectiveTarget(
    SgPragmaDeclaration *pragma_declaration,
    ResolvedOmpFunctionDirectiveTarget::Kind expected_kind,
    const std::string &fortran_explicit_name, const char *contract) {
  if (pragma_declaration == nullptr || contract == nullptr ||
      pragma_declaration->get_scope() == nullptr ||
      pragma_declaration->get_parent() != pragma_declaration->get_scope()) {
    std::cerr << "REX_OMP_AST_INVARIANT["
              << (contract != nullptr ? contract : "function-directive-target")
              << "]: pragma has no exact lexical scope\n";
    ROSE_ABORT();
  }

  auto existing = openMPFunctionDirectiveTargets().find(pragma_declaration);
  if (existing != openMPFunctionDirectiveTargets().end()) {
    if (existing->second.kind() != expected_kind ||
        existing->second.declaration() == nullptr ||
        existing->second.symbol() == nullptr ||
        getOmpFunctionDirectiveSymbolDeclaration(existing->second.symbol()) !=
            existing->second.declaration()) {
      std::cerr << "REX_OMP_AST_INVARIANT[" << contract
                << "]: cached AST-constructor target is incoherent\n";
      ROSE_ABORT();
    }
    return &existing->second;
  }

  SgScopeStatement *scope = pragma_declaration->get_scope();
  SgDeclarationStatement *target_declaration = nullptr;
  SgSymbol *target_symbol = nullptr;
  const bool is_fortran = pragma_declaration->get_fortran_directive_family() ==
                              SgPragmaDeclaration::e_fortran_directive_openmp ||
                          pragma_declaration->get_fortran_directive_family() ==
                              SgPragmaDeclaration::e_fortran_directive_ompx;
  if (is_fortran) {
    if (!fortran_explicit_name.empty()) {
      std::string canonical_name = fortran_explicit_name;
      std::transform(canonical_name.begin(), canonical_name.end(),
                     canonical_name.begin(), [](unsigned char character) {
                       return static_cast<char>(std::tolower(character));
                     });
      target_symbol = SageInterface::lookupSymbolInParentScopes(
          SgName(canonical_name), scope);
      std::set<SgAliasSymbol *> aliases;
      while (SgAliasSymbol *alias = isSgAliasSymbol(target_symbol)) {
        if (alias->get_alias() == nullptr || !aliases.insert(alias).second) {
          std::cerr << "REX_OMP_AST_INVARIANT[" << contract
                    << "]: explicit Fortran procedure has an invalid Sage "
                       "alias chain\n";
          ROSE_ABORT();
        }
        target_symbol = alias->get_alias();
      }
      target_declaration =
          getOmpFunctionDirectiveSymbolDeclaration(target_symbol);
    } else {
      SgFunctionDeclaration *enclosing =
          SageInterface::getEnclosingFunctionDeclaration(pragma_declaration,
                                                         true);
      if (enclosing != nullptr &&
          enclosing->get_firstNondefiningDeclaration() != nullptr) {
        enclosing = isSgFunctionDeclaration(
            enclosing->get_firstNondefiningDeclaration());
      }
      target_declaration = enclosing;
      target_symbol = enclosing != nullptr
                          ? enclosing->get_symbol_from_symbol_table()
                          : nullptr;
    }
  } else {
    if (!fortran_explicit_name.empty()) {
      std::cerr << "REX_OMP_AST_INVARIANT[" << contract
                << "]: C/C++ function directive unexpectedly supplied a "
                   "Fortran explicit target\n";
      ROSE_ABORT();
    }
    bool found_pragma = false;
    for (SgNode *child : scope->get_traversalSuccessorContainer()) {
      if (child == pragma_declaration) {
        if (found_pragma) {
          std::cerr << "REX_OMP_AST_INVARIANT[" << contract
                    << "]: pragma occurs more than once in its lexical scope\n";
          ROSE_ABORT();
        }
        found_pragma = true;
        continue;
      }
      if (!found_pragma || isSgPragmaDeclaration(child) != nullptr) {
        continue;
      }
      target_declaration = isSgDeclarationStatement(child);
      break;
    }
    const bool function_like =
        isSgFunctionDeclaration(target_declaration) != nullptr ||
        isSgTemplateFunctionDeclaration(target_declaration) != nullptr ||
        isSgTemplateMemberFunctionDeclaration(target_declaration) != nullptr;
    if (!found_pragma || !function_like ||
        target_declaration->get_scope() != scope ||
        target_declaration->get_parent() != scope) {
      std::cerr << "REX_OMP_AST_INVARIANT[" << contract
                << "]: next direct lexical declaration is not a function\n";
      ROSE_ABORT();
    }
    target_symbol = target_declaration->get_symbol_from_symbol_table();
    if (target_symbol == nullptr &&
        target_declaration->get_firstNondefiningDeclaration() != nullptr) {
      target_symbol = target_declaration->get_firstNondefiningDeclaration()
                          ->get_symbol_from_symbol_table();
    }
  }

  SgDeclarationStatement *canonical =
      getOmpFunctionDirectiveSymbolDeclaration(target_symbol);
  if (canonical == nullptr || target_declaration == nullptr ||
      (canonical != target_declaration &&
       canonical != target_declaration->get_firstNondefiningDeclaration())) {
    std::cerr << "REX_OMP_AST_INVARIANT[" << contract
              << "]: resolved function has no coherent exact Sage symbol\n";
    ROSE_ABORT();
  }

  std::size_t ordinal = 0;
  for (const auto &entry : openMPFunctionDirectiveTargets()) {
    if (entry.second.kind() == expected_kind &&
        entry.second.symbol() == target_symbol) {
      ++ordinal;
    }
  }
  auto inserted = openMPFunctionDirectiveTargets().emplace(
      pragma_declaration,
      ResolvedOmpFunctionDirectiveTarget(expected_kind, canonical,
                                         target_symbol, ordinal));
  if (!inserted.second) {
    std::cerr << "REX_OMP_AST_INVARIANT[" << contract
              << "]: AST constructor published its target twice\n";
    ROSE_ABORT();
  }
  return &inserted.first->second;
}

using OmpNestedDirectivePair = std::pair<OpenMPDirective *, OpenMPDirective *>;

static void
appendNestedVariantDirectivePairs(OpenMPClause *original_clause,
                                  OpenMPClause *parsed_clause,
                                  std::vector<OmpNestedDirectivePair> &pairs) {
  ROSE_ASSERT(original_clause != nullptr);
  ROSE_ASSERT(parsed_clause != nullptr);
  if (original_clause->getKind() != parsed_clause->getKind()) {
    std::cerr << "REX_OMP_AST_INVARIANT[nested-clause-cache]: clause kinds "
                 "diverge\n";
    ROSE_ABORT();
  }

  auto append_pair = [&](OpenMPDirective *original, OpenMPDirective *parsed,
                         const char *role) {
    if ((original == nullptr) != (parsed == nullptr)) {
      std::cerr << "REX_OMP_AST_INVARIANT[nested-clause-cache]: " << role
                << " presence diverges\n";
      ROSE_ABORT();
    }
    if (original == nullptr) {
      return;
    }
    if (original->getKind() != parsed->getKind()) {
      std::cerr << "REX_OMP_AST_INVARIANT[nested-clause-cache]: " << role
                << " directive kinds diverge\n";
      ROSE_ABORT();
    }
    pairs.emplace_back(original, parsed);
  };

  auto *original_variant = dynamic_cast<OpenMPVariantClause *>(original_clause);
  auto *parsed_variant = dynamic_cast<OpenMPVariantClause *>(parsed_clause);
  if ((original_variant == nullptr) != (parsed_variant == nullptr)) {
    std::cerr << "REX_OMP_AST_INVARIANT[nested-clause-cache]: variant clause "
                 "identity diverges\n";
    ROSE_ABORT();
  }
  if (original_variant != nullptr) {
    const auto &original_sets = original_variant->getTraitSets();
    const auto &parsed_sets = parsed_variant->getTraitSets();
    if (original_sets.size() != parsed_sets.size()) {
      std::cerr << "REX_OMP_AST_INVARIANT[nested-clause-cache]: trait-set "
                   "counts diverge\n";
      ROSE_ABORT();
    }
    for (size_t set_index = 0; set_index < original_sets.size(); ++set_index) {
      const auto &original_set = original_sets[set_index];
      const auto &parsed_set = parsed_sets[set_index];
      if (original_set.kind != parsed_set.kind ||
          original_set.selectors.size() != parsed_set.selectors.size()) {
        std::cerr << "REX_OMP_AST_INVARIANT[nested-clause-cache]: trait-set "
                     "structure diverges\n";
        ROSE_ABORT();
      }
      for (size_t selector_index = 0;
           selector_index < original_set.selectors.size(); ++selector_index) {
        const auto &original_selector = original_set.selectors[selector_index];
        const auto &parsed_selector = parsed_set.selectors[selector_index];
        if (original_selector.kind != parsed_selector.kind) {
          std::cerr
              << "REX_OMP_AST_INVARIANT[nested-clause-cache]: trait-selector "
                 "kinds diverge\n";
          ROSE_ABORT();
        }
        append_pair(original_selector.construct_directive.get(),
                    parsed_selector.construct_directive.get(),
                    "construct selector");
      }
    }
  }

  switch (original_clause->getKind()) {
  case OMPC_when:
    append_pair(
        static_cast<OpenMPWhenClause *>(original_clause)->getVariantDirective(),
        static_cast<OpenMPWhenClause *>(parsed_clause)->getVariantDirective(),
        "when variant");
    break;
  case OMPC_otherwise:
    append_pair(static_cast<OpenMPOtherwiseClause *>(original_clause)
                    ->getVariantDirective(),
                static_cast<OpenMPOtherwiseClause *>(parsed_clause)
                    ->getVariantDirective(),
                "otherwise variant");
    break;
  case OMPC_default: {
    auto *original_default =
        static_cast<OpenMPDefaultClause *>(original_clause);
    auto *parsed_default = static_cast<OpenMPDefaultClause *>(parsed_clause);
    if (original_default->getDefaultClauseKind() !=
        parsed_default->getDefaultClauseKind()) {
      std::cerr << "REX_OMP_AST_INVARIANT[nested-clause-cache]: default "
                   "clause kinds diverge\n";
      ROSE_ABORT();
    }
    append_pair(original_default->getVariantDirective(),
                parsed_default->getVariantDirective(), "default variant");
    break;
  }
  default:
    break;
  }
}

static bool
endDirectiveOwnsCompletePairedDirective(const OpenMPEndDirective *end,
                                        const char *contract) {
  if (end == nullptr || contract == nullptr) {
    std::cerr << "REX_OMP_AST_INVARIANT[end-directive-role]: invalid "
                 "ownership query\n";
    ROSE_ABORT();
  }
  if (end->getPairedDirective() == nullptr) {
    std::cerr << "REX_OMP_AST_INVARIANT[" << contract
              << "]: END directive has no paired typed directive\n";
    ROSE_ABORT();
  }
  return end->getPairedDirectiveRole() == OpenMPPairedDirectiveRole::Complete;
}

static std::vector<const OmpParsedExpression *>
getDirectiveDirectSemanticNodes(OpenMPDirective *directive,
                                const char *contract) {
  ROSE_ASSERT(directive != nullptr);
  std::set<const ompparser::HostFragment *> descendants;
  for (OpenMPClause *clause : *directive->getClausesInOriginalOrder()) {
    ROSE_ASSERT(clause != nullptr);
    clause->visitHostFragments([&](ompparser::HostFragment &fragment) {
      descendants.insert(&fragment);
    });
  }

  std::vector<OmpNestedDirectivePair> nested_directives;
  for (OpenMPClause *clause : *directive->getClausesInOriginalOrder()) {
    appendNestedVariantDirectivePairs(clause, clause, nested_directives);
  }
  if (directive->getKind() == OMPD_end) {
    auto *end = static_cast<OpenMPEndDirective *>(directive);
    if (endDirectiveOwnsCompletePairedDirective(end, contract)) {
      nested_directives.emplace_back(end->getPairedDirective(),
                                     end->getPairedDirective());
    }
  }
  for (const OmpNestedDirectivePair &nested : nested_directives) {
    ROSE_ASSERT(nested.first != nullptr);
    nested.first->visitHostFragments([&](ompparser::HostFragment &fragment) {
      descendants.insert(&fragment);
    });
  }

  if (auto *mapper = dynamic_cast<OpenMPDeclareMapperDirective *>(directive)) {
    descendants.insert(&mapper->getDeclareMapperTypeFragment());
    descendants.insert(&mapper->getDeclareMapperVarFragment());
  }

  std::vector<const OmpParsedExpression *> result;
  directive->visitHostFragments([&](ompparser::HostFragment &fragment) {
    if (descendants.count(&fragment) == 0) {
      result.push_back(requireParsedHostFragment(fragment, contract));
    }
  });
  return result;
}

static std::vector<const OmpParsedExpression *>
getClauseExpressionSemanticNodes(OpenMPClause *clause, const char *contract) {
  ROSE_ASSERT(clause != nullptr);
  std::vector<const OmpParsedExpression *> result;
  result.reserve(clause->getExpressionItems().size());
  for (const OpenMPExpressionItem &item : clause->getExpressionItems()) {
    result.push_back(requireParsedHostFragment(item.fragment, contract));
  }
  return result;
}

static std::vector<const OmpParsedExpression *>
getClauseAuxiliarySemanticNodes(OpenMPClause *clause, const char *contract) {
  ROSE_ASSERT(clause != nullptr);
  std::set<const ompparser::HostFragment *> excluded;
  for (const OpenMPExpressionItem &item : clause->getExpressionItems()) {
    excluded.insert(&item.fragment);
  }

  if (auto *map_clause = dynamic_cast<OpenMPMapClause *>(clause)) {
    excluded.insert(&map_clause->getMapperIdentifierFragment());
    for (const auto &policies : map_clause->getDistDataPolicies()) {
      for (const OpenMPMapClause::DistDataPolicy &policy : policies) {
        excluded.insert(&policy.argument);
      }
    }
  } else if (auto *to_clause = dynamic_cast<OpenMPToClause *>(clause)) {
    excluded.insert(&to_clause->getMapperIdentifierFragment());
  } else if (auto *from_clause = dynamic_cast<OpenMPFromClause *>(clause)) {
    excluded.insert(&from_clause->getMapperIdentifierFragment());
  } else if (auto *allocate_clause =
                 dynamic_cast<OpenMPAllocateClause *>(clause)) {
    excluded.insert(&allocate_clause->getUserDefinedAllocatorFragment());
    excluded.insert(&allocate_clause->getAlignmentFragment());
  }

  std::vector<OmpNestedDirectivePair> nested_directives;
  appendNestedVariantDirectivePairs(clause, clause, nested_directives);
  for (const OmpNestedDirectivePair &nested : nested_directives) {
    ROSE_ASSERT(nested.first != nullptr);
    nested.first->visitHostFragments(
        [&](ompparser::HostFragment &fragment) { excluded.insert(&fragment); });
  }

  std::vector<const OmpParsedExpression *> result;
  clause->visitHostFragments([&](ompparser::HostFragment &fragment) {
    if (excluded.count(&fragment) == 0) {
      result.push_back(requireParsedHostFragment(fragment, contract));
    }
  });
  return result;
}

static bool directiveTreeOwnsNestedDirective(OpenMPDirective *directive) {
  ROSE_ASSERT(directive != nullptr);
  if (directive->getKind() == OMPD_end) {
    auto *end = static_cast<OpenMPEndDirective *>(directive);
    if (endDirectiveOwnsCompletePairedDirective(end,
                                                "nested-directive-query")) {
      return true;
    }
  }
  std::vector<OpenMPClause *> *clauses = directive->getClausesInOriginalOrder();
  ROSE_ASSERT(clauses != nullptr);
  for (OpenMPClause *clause : *clauses) {
    ROSE_ASSERT(clause != nullptr);
    std::vector<OmpNestedDirectivePair> nested;
    appendNestedVariantDirectivePairs(clause, clause, nested);
    if (!nested.empty()) {
      return true;
    }
  }
  return false;
}

OmpDirectiveParseCacheTree parseClauseNodesForDirective(
    SgPragmaDeclaration *pragma_declaration, OpenMPDirective *directive,
    const std::string &directive_text,
    const std::string *source_directive_text = nullptr) {
  OmpDirectiveParseCacheTree cache_tree;
  OmpClauseParseCache &parsed_cache = cache_tree.root;
  if (pragma_declaration == nullptr || directive == nullptr ||
      directive_text.empty()) {
    MLOG_ERROR_C("ompAstConstruction",
                 "OpenMP clause cache requires a declaration, directive, and "
                 "nonempty directive text\n");
    ROSE_ABORT();
  }
  parsed_cache.directive_owns_source_expression_spelling =
      pragma_declaration->get_cxx_top_level_macro_expansion();

  const std::string parse_text = normalizeOpenMPDirectiveForParser(
      directive_text, directive->getBaseLang());

  std::vector<OpenMPClause *> *original_clauses =
      directive->getClausesInOriginalOrder();
  ROSE_ASSERT(original_clauses != nullptr);

  std::vector<OpenMPClause *> semantic_clauses;
  semantic_clauses.reserve(original_clauses->size());

  bool requires_expression_cache = false;
  for (OpenMPClause *clause : *original_clauses) {
    if (clause == nullptr) {
      MLOG_ERROR_C("ompAstConstruction",
                   "OpenMP directive contains a null clause\n");
      ROSE_ABORT();
    }
    if (isOpenMPMergedEndClause(clause)) {
      static_cast<void>(getOpenMPMergedEndClauseSource(clause));
      if (clause->getKind() != OMPC_nowait &&
          clause->getKind() != OMPC_copyprivate) {
        MLOG_ERROR_C("ompAstConstruction",
                     "Unsupported merged OpenMP end-clause kind: %d\n",
                     static_cast<int>(clause->getKind()));
        ROSE_ABORT();
      }
    } else {
      semantic_clauses.push_back(clause);
    }
    clause->visitHostFragments([&](ompparser::HostFragment &fragment) {
      if (!fragment.spelling.empty()) {
        requires_expression_cache = true;
      }
    });
    if (clause->getKind() == OMPC_to) {
      auto *to_clause = static_cast<OpenMPToClause *>(clause);
      if (!to_clause->getMapperIdentifier().empty()) {
        requires_expression_cache = true;
      }
    } else if (clause->getKind() == OMPC_from) {
      auto *from_clause = static_cast<OpenMPFromClause *>(clause);
      if (!from_clause->getMapperIdentifier().empty()) {
        requires_expression_cache = true;
      }
    } else if (clause->getKind() == OMPC_allocate) {
      auto *allocate_clause = static_cast<OpenMPAllocateClause *>(clause);
      if (!allocate_clause->getUserDefinedAllocator().empty() ||
          allocate_clause->hasAlignModifier()) {
        requires_expression_cache = true;
      }
    }
  }

  directive->visitHostFragments([&](ompparser::HostFragment &fragment) {
    if (!fragment.spelling.empty()) {
      requires_expression_cache = true;
    }
  });

  requires_expression_cache =
      requires_expression_cache || directiveTreeOwnsNestedDirective(directive);
  if (!requires_expression_cache) {
    for (OpenMPClause *clause : semantic_clauses) {
      ROSE_ASSERT(clause != nullptr);
      parsed_cache.clause_expression_nodes[clause] = {};
      parsed_cache.clause_auxiliary_expression_nodes[clause] = {};
      parsed_cache.clause_source_expression_texts[clause] = {};
      parsed_cache.clause_source_auxiliary_expression_texts[clause] = {};
    }
    return cache_tree;
  }

  OmpExprParseContext context;
  context.pragma_declaration = pragma_declaration;
  context.directive = directive;

  omp_exprparser_clear_context_variable_symbols();
  initializeOpenMPExactSemanticBindings(&context);
  if ((directive->getKind() == OMPD_declare_simd ||
       directive->getKind() == OMPD_declare_variant) &&
      directive->getBaseLang() != Lang_Fortran) {
    SgNode *target_declaration = nullptr;
    if (directive->getKind() == OMPD_declare_simd) {
      target_declaration =
          requireResolvedOmpFunctionDirectiveTarget(
              pragma_declaration,
              ResolvedOmpFunctionDirectiveTarget::Kind::declare_simd, {},
              "declare-simd-target")
              ->declaration();
    } else {
      target_declaration =
          requireResolvedOmpFunctionDirectiveTarget(
              pragma_declaration,
              ResolvedOmpFunctionDirectiveTarget::Kind::declare_variant, {},
              "declare-variant-target")
              ->declaration();
    }
    SgFunctionDeclaration *function =
        isSgFunctionDeclaration(target_declaration);
    if (function == nullptr || function->get_parameterList() == nullptr) {
      std::cerr << "REX_OMP_AST_INVARIANT[function-directive-target]: exact "
                   "target has no function parameter list\n";
      ROSE_ABORT();
    }
    for (SgInitializedName *parameter :
         function->get_parameterList()->get_args()) {
      SgVariableSymbol *symbol =
          parameter != nullptr
              ? isSgVariableSymbol(parameter->get_symbol_from_symbol_table())
              : nullptr;
      if (symbol == nullptr) {
        std::cerr << "REX_OMP_AST_INVARIANT[function-directive-target]: "
                     "target parameter has no exact Sage variable symbol\n";
        ROSE_ABORT();
      }
      omp_exprparser_add_context_variable_symbol(symbol);
    }
  }
  if (directive->getKind() == OMPD_declare_mapper) {
    auto *declare_mapper =
        static_cast<OpenMPDeclareMapperDirective *>(directive);
    const NormalizedDeclareMapperData mapper_data =
        normalizeDeclareMapperData(declare_mapper);
    context.declare_mapper_type =
        resolveDeclareMapperType(pragma_declaration, mapper_data.mapper_type);
    parsed_cache.directive_local_scope = buildDeclareMapperLocalScope(
        pragma_declaration, declare_mapper, context.declare_mapper_type);
    context.directive_local_scope = parsed_cache.directive_local_scope;
    SgVariableSymbol *mapper_symbol =
        parsed_cache.directive_local_scope->lookup_variable_symbol(
            mapper_data.mapper_variable);
    ROSE_ASSERT(mapper_symbol != nullptr);
    omp_exprparser_add_context_variable_symbol(mapper_symbol);
  }
  RexOpenMPHostLanguageHooks semantic_hooks(&context);
  std::unique_ptr<OpenMPDirective> parsed_directive_owner =
      parseOpenMPDirectiveOrAbort(
          parse_text,
          makeOpenMPParseOptions(directive->getBaseLang(), &semantic_hooks));
  OpenMPDirective *parsed_directive = parsed_directive_owner.get();
  if (context.pending_iterator_type != nullptr) {
    std::cerr << "REX_OMP_AST_INVARIANT[iterator-type]: final iterator type "
                 "has no exact following iterator name\n";
    ROSE_ABORT();
  }
  parsed_cache.directive_local_scope = context.directive_local_scope;
  finishOpenMPExactSemanticBindings(&context);
  omp_exprparser_clear_context_variable_symbols();
  ROSE_ASSERT(parsed_directive->getKind() == directive->getKind());

  parsed_cache.owned_nodes = std::move(context.owned_nodes);

  if (parsed_directive->getKind() == OMPD_declare_mapper) {
    auto *declare_mapper =
        static_cast<OpenMPDeclareMapperDirective *>(parsed_directive);
    const OmpParsedExpression *type_node = requireParsedHostFragment(
        declare_mapper->getDeclareMapperTypeFragment(), "declare-mapper-type");
    const OmpParsedExpression *variable_node =
        requireParsedHostFragment(declare_mapper->getDeclareMapperVarFragment(),
                                  "declare-mapper-variable");
    if (type_node == nullptr || variable_node == nullptr ||
        type_node->mode != OMP_EXPR_PARSE_openmp_declare_mapper_type ||
        type_node->text != declare_mapper->getDeclareMapperType() ||
        isSgTypeExpression(type_node->node) == nullptr ||
        variable_node->mode != OMP_EXPR_PARSE_openmp_declare_mapper_variable ||
        variable_node->text != declare_mapper->getDeclareMapperVar() ||
        isSgVarRefExp(variable_node->node) == nullptr) {
      std::cerr << "REX_OMP_AST_INVARIANT[declare-mapper-payload]: exact "
                   "type/variable callback nodes disagree with parser IR\n";
      ROSE_ABORT();
    }
    parsed_cache.declare_mapper_type_node = type_node;
    parsed_cache.declare_mapper_variable_node = variable_node;
    if (declare_mapper->getIdentifier() !=
            OMPD_DECLARE_MAPPER_IDENTIFIER_user &&
        (!declare_mapper->getUserDefinedIdentifierFragment().spelling.empty() ||
         declare_mapper->getUserDefinedIdentifierFragment().semantic !=
             nullptr)) {
      std::cerr << "REX_OMP_AST_INVARIANT[declare-mapper-identifier]: "
                   "non-user mapper identifier owns a callback node\n";
      ROSE_ABORT();
    }
  }
  parsed_cache.directive_expression_nodes = getDirectiveDirectSemanticNodes(
      parsed_directive, "directive-expression-cache");

  std::vector<OpenMPClause *> *parsed_clauses =
      parsed_directive->getClausesInOriginalOrder();
  ROSE_ASSERT(parsed_clauses != nullptr);
  if (semantic_clauses.size() != parsed_clauses->size()) {
    const size_t original_clause_count = semantic_clauses.size();
    const size_t parsed_clause_count = parsed_clauses->size();
    MLOG_ERROR_C("ompAstConstruction",
                 "OpenMP reparse produced mismatched clause counts: "
                 "original=%zu parsed=%zu text=%s\n",
                 original_clause_count, parsed_clause_count,
                 parse_text.c_str());
    ROSE_ABORT();
  }

  struct BoundTargetUpdateListItem {
    const OpenMPClause *clause = nullptr;
    OmpListItemIdentity identity;
  };
  std::vector<BoundTargetUpdateListItem> target_update_list_items;
  const size_t target_update_motion_clause_count =
      directive->getKind() == OMPD_target_update
          ? static_cast<size_t>(std::count_if(
                semantic_clauses.begin(), semantic_clauses.end(),
                [](OpenMPClause *clause) {
                  return clause != nullptr && (clause->getKind() == OMPC_to ||
                                               clause->getKind() == OMPC_from);
                }))
          : 0;

  SgVariableSymbol *declare_mapper_symbol = nullptr;
  bool declare_mapper_maps_variable = false;
  if (directive->getKind() == OMPD_declare_mapper) {
    ROSE_ASSERT(parsed_cache.directive_local_scope != nullptr);
    const NormalizedDeclareMapperData mapper_data = normalizeDeclareMapperData(
        static_cast<OpenMPDeclareMapperDirective *>(directive));
    declare_mapper_symbol =
        parsed_cache.directive_local_scope->lookup_variable_symbol(
            mapper_data.mapper_variable);
    ROSE_ASSERT(declare_mapper_symbol != nullptr);
  }

  for (size_t index = 0; index < semantic_clauses.size(); ++index) {
    OpenMPClause *original_clause = semantic_clauses[index];
    OpenMPClause *parsed_clause = (*parsed_clauses)[index];
    ROSE_ASSERT(original_clause != nullptr);
    ROSE_ASSERT(parsed_clause != nullptr);
    if (original_clause->getKind() != parsed_clause->getKind()) {
      const int original_clause_kind =
          static_cast<int>(original_clause->getKind());
      const int parsed_clause_kind = static_cast<int>(parsed_clause->getKind());
      MLOG_ERROR_C("ompAstConstruction",
                   "OpenMP reparse produced mismatched clause kind: "
                   "original=%d parsed=%d text=%s\n",
                   original_clause_kind, parsed_clause_kind,
                   parse_text.c_str());
      ROSE_ABORT();
    }

    std::vector<const OmpParsedExpression *> clause_nodes =
        getClauseExpressionSemanticNodes(parsed_clause, "clause-expression");

    if (declare_mapper_symbol != nullptr &&
        original_clause->getKind() == OMPC_map) {
      for (const OmpParsedExpression *parsed : clause_nodes) {
        ROSE_ASSERT(parsed != nullptr && parsed->node != nullptr);
        OmpListItemIdentity identity;
        if (!buildOmpListItemIdentity(parsed->node, identity) ||
            identity.root == nullptr) {
          std::cerr << "REX_OMP_AST_INVARIANT[declare-mapper-map-item]: map "
                       "item has no exact typed list-item identity\n";
          ROSE_ABORT();
        }
        if (identity.root == declare_mapper_symbol) {
          declare_mapper_maps_variable = true;
        }
      }
    }

    if (target_update_motion_clause_count > 1 &&
        (original_clause->getKind() == OMPC_to ||
         original_clause->getKind() == OMPC_from)) {
      for (const OmpParsedExpression *parsed : clause_nodes) {
        ROSE_ASSERT(parsed != nullptr && parsed->node != nullptr);
        BoundTargetUpdateListItem current;
        current.clause = original_clause;
        if (!buildOmpListItemIdentity(parsed->node, current.identity) ||
            current.identity.root == nullptr) {
          std::cerr << "REX_OMP_AST_INVARIANT[target-update-list-item]: data "
                       "motion item has no exact typed identity\n";
          ROSE_ABORT();
        }
        for (const BoundTargetUpdateListItem &previous :
             target_update_list_items) {
          if (previous.clause != current.clause &&
              ompListItemIdentitiesConflict(previous.identity,
                                            current.identity)) {
            std::cerr << "REX_OMP_SEMANTIC[directive-list-item-uniqueness]: "
                         "target update repeats an OpenMP list item across "
                         "clauses\n";
            ROSE_ABORT();
          }
        }
        target_update_list_items.push_back(std::move(current));
      }
    }

    std::vector<const OmpParsedExpression *> clause_auxiliary_nodes =
        getClauseAuxiliarySemanticNodes(parsed_clause, "clause-auxiliary");

    if (original_clause->getKind() == OMPC_map) {
      auto *original_map_clause =
          static_cast<OpenMPMapClause *>(original_clause);
      auto *parsed_map_clause = static_cast<OpenMPMapClause *>(parsed_clause);
      if (!original_map_clause->getMapperIdentifier().empty()) {
        const OmpParsedExpression *mapper_node = requireParsedHostFragment(
            parsed_map_clause->getMapperIdentifierFragment(),
            "map-mapper-identifier");
        if (mapper_node == nullptr) {
          MLOG_ERROR_C("ompAstConstruction",
                       "OpenMP map mapper identifier has no parsed AST "
                       "node\n");
          ROSE_ABORT();
        }
        clause_nodes.push_back(mapper_node);
      }
    } else if (original_clause->getKind() == OMPC_to) {
      auto *original_to_clause = static_cast<OpenMPToClause *>(original_clause);
      auto *parsed_to_clause = static_cast<OpenMPToClause *>(parsed_clause);
      if (!original_to_clause->getMapperIdentifier().empty()) {
        const OmpParsedExpression *mapper_node = requireParsedHostFragment(
            parsed_to_clause->getMapperIdentifierFragment(),
            "to-mapper-identifier");
        if (mapper_node == nullptr) {
          MLOG_ERROR_C("ompAstConstruction",
                       "OpenMP to mapper identifier has no parsed AST node\n");
          ROSE_ABORT();
        }
        clause_nodes.push_back(mapper_node);
      }
    } else if (original_clause->getKind() == OMPC_from) {
      auto *original_from_clause =
          static_cast<OpenMPFromClause *>(original_clause);
      auto *parsed_from_clause = static_cast<OpenMPFromClause *>(parsed_clause);
      if (!original_from_clause->getMapperIdentifier().empty()) {
        const OmpParsedExpression *mapper_node = requireParsedHostFragment(
            parsed_from_clause->getMapperIdentifierFragment(),
            "from-mapper-identifier");
        if (mapper_node == nullptr) {
          MLOG_ERROR_C("ompAstConstruction",
                       "OpenMP from mapper identifier has no parsed AST "
                       "node\n");
          ROSE_ABORT();
        }
        clause_nodes.push_back(mapper_node);
      }
    } else if (original_clause->getKind() == OMPC_allocate) {
      auto *original_allocate_clause =
          static_cast<OpenMPAllocateClause *>(original_clause);
      auto *parsed_allocate_clause =
          static_cast<OpenMPAllocateClause *>(parsed_clause);
      if (!original_allocate_clause->getUserDefinedAllocator().empty()) {
        const OmpParsedExpression *allocator_node = requireParsedHostFragment(
            parsed_allocate_clause->getUserDefinedAllocatorFragment(),
            "allocate-allocator");
        if (allocator_node == nullptr) {
          MLOG_ERROR_C(
              "ompAstConstruction",
              "OpenMP user-defined allocator has no parsed AST node\n");
          ROSE_ABORT();
        }
        clause_nodes.push_back(allocator_node);
      }
      if (original_allocate_clause->hasAlignModifier()) {
        const OmpParsedExpression *alignment_node = requireParsedHostFragment(
            parsed_allocate_clause->getAlignmentFragment(),
            "allocate-alignment");
        if (alignment_node == nullptr) {
          MLOG_ERROR_C("ompAstConstruction",
                       "OpenMP allocate alignment has no parsed AST node\n");
          ROSE_ABORT();
        }
        clause_nodes.push_back(alignment_node);
      }
    }

    parsed_cache.clause_expression_nodes[original_clause] =
        std::move(clause_nodes);
    parsed_cache.clause_auxiliary_expression_nodes[original_clause] =
        std::move(clause_auxiliary_nodes);

    if (original_clause->getKind() == OMPC_map) {
      auto *parsed_map_clause = static_cast<OpenMPMapClause *>(parsed_clause);
      const auto &dist_data_policies = parsed_map_clause->getDistDataPolicies();
      parsed_cache.map_dist_data_policies[original_clause] = dist_data_policies;
      std::vector<std::vector<const OmpParsedExpression *>> policy_nodes;
      policy_nodes.reserve(dist_data_policies.size());
      for (const auto &policies_for_item : dist_data_policies) {
        std::vector<const OmpParsedExpression *> item_nodes;
        item_nodes.reserve(policies_for_item.size());
        for (const auto &policy : policies_for_item) {
          const OmpParsedExpression *argument =
              policy.argument.spelling.empty()
                  ? nullptr
                  : requireParsedHostFragment(policy.argument,
                                              "map-dist-data-policy");
          if (policy.argument.spelling.empty() != (argument == nullptr)) {
            MLOG_ERROR_C("ompAstConstruction",
                         "OpenMP map dist_data policy argument text and AST "
                         "node disagree\n");
            ROSE_ABORT();
          }
          item_nodes.push_back(argument);
        }
        policy_nodes.push_back(std::move(item_nodes));
      }
      parsed_cache.map_dist_data_policy_nodes[original_clause] =
          std::move(policy_nodes);
    }
  }

  if (declare_mapper_symbol != nullptr && !declare_mapper_maps_variable) {
    std::cerr << "REX_OMP_SEMANTIC[declare-mapper-map-clause]: declare mapper "
                 "requires a map clause that maps its variable\n";
    ROSE_ABORT();
  }

  std::function<void(OpenMPDirective *, OpenMPDirective *)>
      populate_nested_cache;
  populate_nested_cache = [&](OpenMPDirective *original_nested,
                              OpenMPDirective *parsed_nested) {
    if (original_nested == nullptr || parsed_nested == nullptr ||
        original_nested == directive ||
        original_nested->getKind() != parsed_nested->getKind()) {
      std::cerr << "REX_OMP_AST_INVARIANT[nested-clause-cache]: malformed "
                   "nested directive pair\n";
      ROSE_ABORT();
    }
    auto inserted =
        cache_tree.nested.emplace(original_nested, OmpClauseParseCache{});
    if (!inserted.second) {
      std::cerr << "REX_OMP_AST_INVARIANT[nested-clause-cache]: nested "
                   "directive was cached more than once\n";
      ROSE_ABORT();
    }
    OmpClauseParseCache &nested_cache = inserted.first->second;
    nested_cache.directive_owns_source_expression_spelling =
        parsed_cache.directive_owns_source_expression_spelling;

    nested_cache.directive_expression_nodes = getDirectiveDirectSemanticNodes(
        parsed_nested, "nested-directive-cache");

    std::vector<OpenMPClause *> original_clauses;
    for (OpenMPClause *clause : *original_nested->getClausesInOriginalOrder()) {
      ROSE_ASSERT(clause != nullptr);
      if (!isOpenMPMergedEndClause(clause)) {
        original_clauses.push_back(clause);
      }
    }
    std::vector<OpenMPClause *> *parsed_clauses =
        parsed_nested->getClausesInOriginalOrder();
    ROSE_ASSERT(parsed_clauses != nullptr);
    if (original_clauses.size() != parsed_clauses->size()) {
      std::cerr << "REX_OMP_AST_INVARIANT[nested-clause-cache]: clause counts "
                   "diverge\n";
      ROSE_ABORT();
    }

    std::vector<OmpNestedDirectivePair> child_pairs;
    for (size_t index = 0; index < original_clauses.size(); ++index) {
      OpenMPClause *original_clause = original_clauses[index];
      OpenMPClause *parsed_clause = (*parsed_clauses)[index];
      ROSE_ASSERT(parsed_clause != nullptr);
      if (original_clause->getKind() != parsed_clause->getKind()) {
        std::cerr << "REX_OMP_AST_INVARIANT[nested-clause-cache]: clause "
                     "kinds diverge\n";
        ROSE_ABORT();
      }

      std::vector<const OmpParsedExpression *> expression_nodes =
          getClauseExpressionSemanticNodes(parsed_clause,
                                           "nested-clause-expression");
      std::vector<const OmpParsedExpression *> auxiliary_nodes =
          getClauseAuxiliarySemanticNodes(parsed_clause,
                                          "nested-clause-auxiliary");

      if (original_clause->getKind() == OMPC_map) {
        auto *original_map = static_cast<OpenMPMapClause *>(original_clause);
        auto *parsed_map = static_cast<OpenMPMapClause *>(parsed_clause);
        if (!original_map->getMapperIdentifier().empty()) {
          const OmpParsedExpression *mapper = requireParsedHostFragment(
              parsed_map->getMapperIdentifierFragment(),
              "nested-map-mapper-identifier");
          if (mapper == nullptr) {
            std::cerr << "REX_OMP_AST_INVARIANT[nested-clause-cache]: map "
                         "mapper has no typed callback node\n";
            ROSE_ABORT();
          }
          expression_nodes.push_back(mapper);
        }
        const auto &policies = parsed_map->getDistDataPolicies();
        nested_cache.map_dist_data_policies[original_clause] = policies;
        std::vector<std::vector<const OmpParsedExpression *>> policy_nodes;
        policy_nodes.reserve(policies.size());
        for (const auto &item_policies : policies) {
          std::vector<const OmpParsedExpression *> item_nodes;
          item_nodes.reserve(item_policies.size());
          for (const auto &policy : item_policies) {
            const OmpParsedExpression *argument =
                policy.argument.spelling.empty()
                    ? nullptr
                    : requireParsedHostFragment(policy.argument,
                                                "nested-map-policy");
            if (policy.argument.spelling.empty() != (argument == nullptr)) {
              std::cerr << "REX_OMP_AST_INVARIANT[nested-clause-cache]: map "
                           "policy text and callback node diverge\n";
              ROSE_ABORT();
            }
            item_nodes.push_back(argument);
          }
          policy_nodes.push_back(std::move(item_nodes));
        }
        nested_cache.map_dist_data_policy_nodes[original_clause] =
            std::move(policy_nodes);
      } else if (original_clause->getKind() == OMPC_to) {
        auto *original_to = static_cast<OpenMPToClause *>(original_clause);
        auto *parsed_to = static_cast<OpenMPToClause *>(parsed_clause);
        if (!original_to->getMapperIdentifier().empty()) {
          const OmpParsedExpression *mapper = requireParsedHostFragment(
              parsed_to->getMapperIdentifierFragment(),
              "nested-to-mapper-identifier");
          if (mapper == nullptr) {
            std::cerr << "REX_OMP_AST_INVARIANT[nested-clause-cache]: to "
                         "mapper has no typed callback node\n";
            ROSE_ABORT();
          }
          expression_nodes.push_back(mapper);
        }
      } else if (original_clause->getKind() == OMPC_from) {
        auto *original_from = static_cast<OpenMPFromClause *>(original_clause);
        auto *parsed_from = static_cast<OpenMPFromClause *>(parsed_clause);
        if (!original_from->getMapperIdentifier().empty()) {
          const OmpParsedExpression *mapper = requireParsedHostFragment(
              parsed_from->getMapperIdentifierFragment(),
              "nested-from-mapper-identifier");
          if (mapper == nullptr) {
            std::cerr << "REX_OMP_AST_INVARIANT[nested-clause-cache]: from "
                         "mapper has no typed callback node\n";
            ROSE_ABORT();
          }
          expression_nodes.push_back(mapper);
        }
      } else if (original_clause->getKind() == OMPC_allocate) {
        auto *original_allocate =
            static_cast<OpenMPAllocateClause *>(original_clause);
        auto *parsed_allocate =
            static_cast<OpenMPAllocateClause *>(parsed_clause);
        if (!original_allocate->getUserDefinedAllocator().empty()) {
          const OmpParsedExpression *allocator = requireParsedHostFragment(
              parsed_allocate->getUserDefinedAllocatorFragment(),
              "nested-allocate-allocator");
          if (allocator == nullptr) {
            std::cerr << "REX_OMP_AST_INVARIANT[nested-clause-cache]: "
                         "allocator has no typed callback node\n";
            ROSE_ABORT();
          }
          expression_nodes.push_back(allocator);
        }
        if (original_allocate->hasAlignModifier()) {
          const OmpParsedExpression *alignment =
              requireParsedHostFragment(parsed_allocate->getAlignmentFragment(),
                                        "nested-allocate-alignment");
          if (alignment == nullptr) {
            std::cerr << "REX_OMP_AST_INVARIANT[nested-clause-cache]: "
                         "alignment has no typed callback node\n";
            ROSE_ABORT();
          }
          expression_nodes.push_back(alignment);
        }
      }

      nested_cache.clause_expression_nodes.emplace(original_clause,
                                                   std::move(expression_nodes));
      nested_cache.clause_auxiliary_expression_nodes.emplace(
          original_clause, std::move(auxiliary_nodes));
      appendNestedVariantDirectivePairs(original_clause, parsed_clause,
                                        child_pairs);
    }

    if (original_nested->getKind() == OMPD_end) {
      auto *original_end = static_cast<OpenMPEndDirective *>(original_nested);
      auto *parsed_end = static_cast<OpenMPEndDirective *>(parsed_nested);
      OpenMPDirective *original_paired = original_end->getPairedDirective();
      OpenMPDirective *parsed_paired = parsed_end->getPairedDirective();
      if (original_paired == nullptr || parsed_paired == nullptr ||
          original_paired->getKind() != parsed_paired->getKind()) {
        std::cerr << "REX_OMP_AST_INVARIANT[nested-clause-cache]: malformed "
                     "end-directive payload\n";
        ROSE_ABORT();
      }
      const bool original_complete = endDirectiveOwnsCompletePairedDirective(
          original_end, "nested-clause-cache");
      const bool parsed_complete = endDirectiveOwnsCompletePairedDirective(
          parsed_end, "nested-clause-cache");
      if (original_complete != parsed_complete) {
        std::cerr << "REX_OMP_AST_INVARIANT[nested-clause-cache]: END "
                     "directive ownership roles diverge\n";
        ROSE_ABORT();
      }
      if (original_complete) {
        child_pairs.emplace_back(original_paired, parsed_paired);
      }
    }
    for (const OmpNestedDirectivePair &child : child_pairs) {
      populate_nested_cache(child.first, child.second);
    }
  };

  std::vector<OmpNestedDirectivePair> nested_pairs;
  for (size_t index = 0; index < semantic_clauses.size(); ++index) {
    appendNestedVariantDirectivePairs(semantic_clauses[index],
                                      (*parsed_clauses)[index], nested_pairs);
  }
  if (directive->getKind() == OMPD_end) {
    auto *original_end = static_cast<OpenMPEndDirective *>(directive);
    auto *parsed_end = static_cast<OpenMPEndDirective *>(parsed_directive);
    OpenMPDirective *original_paired = original_end->getPairedDirective();
    OpenMPDirective *parsed_paired = parsed_end->getPairedDirective();
    if (original_paired == nullptr || parsed_paired == nullptr ||
        original_paired->getKind() != parsed_paired->getKind()) {
      std::cerr << "REX_OMP_AST_INVARIANT[nested-clause-cache]: malformed "
                   "root end-directive payload\n";
      ROSE_ABORT();
    }
    const bool original_complete = endDirectiveOwnsCompletePairedDirective(
        original_end, "nested-clause-cache");
    const bool parsed_complete = endDirectiveOwnsCompletePairedDirective(
        parsed_end, "nested-clause-cache");
    if (original_complete != parsed_complete) {
      std::cerr << "REX_OMP_AST_INVARIANT[nested-clause-cache]: root END "
                   "directive ownership roles diverge\n";
      ROSE_ABORT();
    }
    if (original_complete) {
      nested_pairs.emplace_back(original_paired, parsed_paired);
    }
  }
  for (const OmpNestedDirectivePair &nested : nested_pairs) {
    populate_nested_cache(nested.first, nested.second);
  }

  const bool requires_source_expression_cache =
      source_directive_text != nullptr;
  if (requires_source_expression_cache &&
      !pragma_declaration->get_cxx_top_level_macro_expansion()) {
    OmpExprParseContext source_context;
    source_context.pragma_declaration = pragma_declaration;
    source_context.directive = directive;
    source_context.capture_source_text_only = true;

    const std::string source_parse_text = normalizeOpenMPDirectiveForParser(
        *source_directive_text, directive->getBaseLang());
    RexOpenMPHostLanguageHooks source_hooks(&source_context);
    ompparser::ParseOptions source_parse_options =
        makeOpenMPParseOptions(directive->getBaseLang(), &source_hooks);
    std::unique_ptr<OpenMPDirective> source_directive_owner =
        parseOpenMPDirectiveOrAbort(source_parse_text, source_parse_options);
    OpenMPDirective *source_directive = source_directive_owner.get();
    if (source_directive->getKind() != directive->getKind()) {
      MLOG_ERROR_C("ompAstConstruction",
                   "Exact OpenMP source text cannot be parsed for source "
                   "expression ownership: %s\n",
                   source_directive_text->c_str());
      ROSE_ABORT();
    }

    if (source_directive->getKind() == OMPD_threadprivate) {
      auto *source_threadprivate =
          static_cast<OpenMPThreadprivateDirective *>(source_directive);
      for (const ompparser::HostFragment &fragment :
           source_threadprivate->getThreadprivateList()) {
        const OmpParsedExpression *parsed =
            requireParsedHostFragment(fragment, "threadprivate-source");
        if (parsed == nullptr || parsed->mode != OMP_EXPR_PARSE_variable_list ||
            trimWhitespaceCopy(parsed->text).empty()) {
          std::cerr
              << "REX_OMP_AST_INVARIANT[threadprivate-source]: exact source "
                 "directive has an invalid variable-list callback\n";
          ROSE_ABORT();
        }
        parsed_cache.threadprivate_source_expression_texts.push_back(
            parsed->text);
      }
    }

    std::vector<OpenMPClause *> *source_clauses =
        source_directive->getClausesInOriginalOrder();
    ROSE_ASSERT(source_clauses != nullptr);
    if (source_clauses->size() != semantic_clauses.size()) {
      MLOG_ERROR_C("ompAstConstruction",
                   "Exact OpenMP source text has a different clause count: "
                   "semantic=%zu source=%zu text=%s\n",
                   semantic_clauses.size(), source_clauses->size(),
                   source_directive_text->c_str());
      ROSE_ABORT();
    }

    for (size_t index = 0; index < semantic_clauses.size(); ++index) {
      OpenMPClause *original_clause = semantic_clauses[index];
      OpenMPClause *source_clause = (*source_clauses)[index];
      ROSE_ASSERT(original_clause != nullptr);
      ROSE_ASSERT(source_clause != nullptr);
      if (source_clause->getKind() != original_clause->getKind()) {
        MLOG_ERROR_C("ompAstConstruction",
                     "Exact OpenMP source text has a different clause kind "
                     "at index %zu: semantic=%d source=%d text=%s\n",
                     index, static_cast<int>(original_clause->getKind()),
                     static_cast<int>(source_clause->getKind()),
                     source_directive_text->c_str());
        ROSE_ABORT();
      }

      std::vector<std::string> source_expressions;
      for (const OmpParsedExpression *parsed : getClauseExpressionSemanticNodes(
               source_clause, "source-clause-expression")) {
        if (parsed == nullptr || parsed->text.empty()) {
          MLOG_ERROR_C("ompAstConstruction",
                       "Exact OpenMP source expression callback returned an "
                       "empty node at clause index %zu\n",
                       index);
          ROSE_ABORT();
        }
        // This callback parsed the exact source directive, so its text is
        // already the uniquely owned spelling. Searching the whole directive
        // again is ambiguous when an operand differs from a clause keyword
        // only by case (for example `num_tasks(NUM_TASKS)`).
        source_expressions.push_back(parsed->text);
      }
      std::vector<std::string> source_auxiliary_expressions;
      for (const OmpParsedExpression *parsed : getClauseAuxiliarySemanticNodes(
               source_clause, "source-clause-auxiliary")) {
        if (parsed == nullptr || parsed->text.empty()) {
          MLOG_ERROR_C(
              "ompAstConstruction",
              "Exact OpenMP source auxiliary-expression callback returned "
              "an empty node at clause index %zu\n",
              index);
          ROSE_ABORT();
        }
        source_auxiliary_expressions.push_back(parsed->text);
      }
      parsed_cache.clause_source_expression_texts[original_clause] =
          source_expressions;
      parsed_cache.clause_source_auxiliary_expression_texts[original_clause] =
          source_auxiliary_expressions;
    }

    std::function<void(OpenMPDirective *, OpenMPDirective *)>
        populate_nested_source_cache;
    populate_nested_source_cache = [&](OpenMPDirective *original_nested,
                                       OpenMPDirective *source_nested) {
      auto cache = cache_tree.nested.find(original_nested);
      if (cache == cache_tree.nested.end() || source_nested == nullptr ||
          original_nested->getKind() != source_nested->getKind()) {
        std::cerr << "REX_OMP_AST_INVARIANT[nested-source-cache]: "
                     "missing semantic cache or mismatched directive\n";
        ROSE_ABORT();
      }

      std::vector<OpenMPClause *> original_clauses;
      for (OpenMPClause *clause :
           *original_nested->getClausesInOriginalOrder()) {
        ROSE_ASSERT(clause != nullptr);
        if (!isOpenMPMergedEndClause(clause)) {
          original_clauses.push_back(clause);
        }
      }
      std::vector<OpenMPClause *> *source_clauses =
          source_nested->getClausesInOriginalOrder();
      ROSE_ASSERT(source_clauses != nullptr);
      if (original_clauses.size() != source_clauses->size()) {
        std::cerr << "REX_OMP_AST_INVARIANT[nested-source-cache]: clause "
                     "counts diverge\n";
        ROSE_ABORT();
      }

      std::vector<OmpNestedDirectivePair> child_pairs;
      for (size_t index = 0; index < original_clauses.size(); ++index) {
        OpenMPClause *original_clause = original_clauses[index];
        OpenMPClause *source_clause = (*source_clauses)[index];
        ROSE_ASSERT(source_clause != nullptr);
        if (original_clause->getKind() != source_clause->getKind()) {
          std::cerr << "REX_OMP_AST_INVARIANT[nested-source-cache]: clause "
                       "kinds diverge\n";
          ROSE_ABORT();
        }
        std::vector<std::string> expressions;
        for (const OmpParsedExpression *parsed :
             getClauseExpressionSemanticNodes(
                 source_clause, "nested-source-clause-expression")) {
          if (parsed == nullptr || parsed->text.empty()) {
            std::cerr << "REX_OMP_AST_INVARIANT[nested-source-cache]: "
                         "source callback returned an empty node\n";
            ROSE_ABORT();
          }
          expressions.push_back(parsed->text);
        }
        std::vector<std::string> auxiliary_expressions;
        for (const OmpParsedExpression *parsed :
             getClauseAuxiliarySemanticNodes(
                 source_clause, "nested-source-clause-auxiliary")) {
          if (parsed == nullptr || parsed->text.empty()) {
            std::cerr << "REX_OMP_AST_INVARIANT[nested-source-cache]: "
                         "source auxiliary callback returned an empty "
                         "node\n";
            ROSE_ABORT();
          }
          auxiliary_expressions.push_back(parsed->text);
        }
        cache->second.clause_source_expression_texts.emplace(
            original_clause, std::move(expressions));
        cache->second.clause_source_auxiliary_expression_texts.emplace(
            original_clause, std::move(auxiliary_expressions));
        appendNestedVariantDirectivePairs(original_clause, source_clause,
                                          child_pairs);
      }

      if (original_nested->getKind() == OMPD_end) {
        auto *original_end = static_cast<OpenMPEndDirective *>(original_nested);
        auto *source_end = static_cast<OpenMPEndDirective *>(source_nested);
        OpenMPDirective *original_paired = original_end->getPairedDirective();
        OpenMPDirective *source_paired = source_end->getPairedDirective();
        if (original_paired == nullptr || source_paired == nullptr ||
            original_paired->getKind() != source_paired->getKind()) {
          std::cerr << "REX_OMP_AST_INVARIANT[nested-source-cache]: "
                       "malformed end-directive payload\n";
          ROSE_ABORT();
        }
        const bool original_complete = endDirectiveOwnsCompletePairedDirective(
            original_end, "nested-source-cache");
        const bool source_complete = endDirectiveOwnsCompletePairedDirective(
            source_end, "nested-source-cache");
        if (original_complete != source_complete) {
          std::cerr << "REX_OMP_AST_INVARIANT[nested-source-cache]: END "
                       "directive ownership roles diverge\n";
          ROSE_ABORT();
        }
        if (original_complete) {
          child_pairs.emplace_back(original_paired, source_paired);
        }
      }
      for (const OmpNestedDirectivePair &child : child_pairs) {
        populate_nested_source_cache(child.first, child.second);
      }
    };

    std::vector<OmpNestedDirectivePair> source_nested_pairs;
    for (size_t index = 0; index < semantic_clauses.size(); ++index) {
      appendNestedVariantDirectivePairs(semantic_clauses[index],
                                        (*source_clauses)[index],
                                        source_nested_pairs);
    }
    if (directive->getKind() == OMPD_end) {
      auto *original_end = static_cast<OpenMPEndDirective *>(directive);
      auto *source_end = static_cast<OpenMPEndDirective *>(source_directive);
      OpenMPDirective *original_paired = original_end->getPairedDirective();
      OpenMPDirective *source_paired = source_end->getPairedDirective();
      if (original_paired == nullptr || source_paired == nullptr ||
          original_paired->getKind() != source_paired->getKind()) {
        std::cerr << "REX_OMP_AST_INVARIANT[nested-source-cache]: malformed "
                     "root end-directive payload\n";
        ROSE_ABORT();
      }
      const bool original_complete = endDirectiveOwnsCompletePairedDirective(
          original_end, "nested-source-cache");
      const bool source_complete = endDirectiveOwnsCompletePairedDirective(
          source_end, "nested-source-cache");
      if (original_complete != source_complete) {
        std::cerr << "REX_OMP_AST_INVARIANT[nested-source-cache]: root END "
                     "directive ownership roles diverge\n";
        ROSE_ABORT();
      }
      if (original_complete) {
        source_nested_pairs.emplace_back(original_paired, source_paired);
      }
    }
    for (const OmpNestedDirectivePair &nested : source_nested_pairs) {
      populate_nested_source_cache(nested.first, nested.second);
    }
  }

  return cache_tree;
}

void publishOpenMPDirectiveParseCacheTree(
    OpenMPDirective *root, OmpDirectiveParseCacheTree &&cache_tree,
    const char *context) {
  if (root == nullptr || context == nullptr ||
      cache_tree.nested.find(root) != cache_tree.nested.end() ||
      openMPClauseNodes().find(root) != openMPClauseNodes().end()) {
    std::cerr << "REX_OMP_AST_INVARIANT[clause-cache-publication]: invalid "
              << (context != nullptr ? context : "<null-context>") << " root\n";
    ROSE_ABORT();
  }
  for (const auto &entry : cache_tree.nested) {
    if (entry.first == nullptr ||
        openMPClauseNodes().find(entry.first) != openMPClauseNodes().end()) {
      std::cerr << "REX_OMP_AST_INVARIANT[clause-cache-publication]: "
                << context << " has a null or duplicate nested key\n";
      ROSE_ABORT();
    }
  }

  if (!openMPClauseNodes().emplace(root, std::move(cache_tree.root)).second) {
    std::cerr << "REX_OMP_AST_INVARIANT[clause-cache-publication]: " << context
              << " root insertion failed\n";
    ROSE_ABORT();
  }
  for (auto &entry : cache_tree.nested) {
    if (!openMPClauseNodes()
             .emplace(entry.first, std::move(entry.second))
             .second) {
      std::cerr << "REX_OMP_AST_INVARIANT[clause-cache-publication]: "
                << context << " nested insertion failed\n";
      ROSE_ABORT();
    }
  }
}

void transferMergedEndClauseCaches(OpenMPDirective *begin,
                                   OpenMPDirective *end_payload,
                                   OpenMPDirective *end_wrapper,
                                   const std::string &end_source_text,
                                   OmpDirectiveParseCacheTree &&end_cache_tree,
                                   OmpClauseParseCache &begin_cache) {
  if (begin == nullptr || end_payload == nullptr || end_wrapper == nullptr ||
      end_wrapper->getKind() != OMPD_end || end_source_text.empty()) {
    std::cerr << "REX_OMP_AST_INVARIANT[end-clause-cache-transfer]: malformed "
                 "directive ownership\n";
    ROSE_ABORT();
  }

  auto cache_for = [&](OpenMPDirective *owner) -> OmpClauseParseCache * {
    if (owner == end_wrapper) {
      return &end_cache_tree.root;
    }
    auto nested = end_cache_tree.nested.find(owner);
    if (nested == end_cache_tree.nested.end()) {
      std::cerr << "REX_OMP_AST_INVARIANT[end-clause-cache-transfer]: end "
                   "clause owner has no parser-published cache\n";
      ROSE_ABORT();
    }
    return &nested->second;
  };

  std::set<OpenMPClause *> transferred_sources;
  bool transferred_any = false;
  for (OpenMPClause *merged : *begin->getClausesInOriginalOrder()) {
    ROSE_ASSERT(merged != nullptr);
    if (!isOpenMPMergedEndClause(merged) ||
        getOpenMPMergedEndClauseSource(merged) != end_source_text) {
      continue;
    }

    OpenMPClause *source = nullptr;
    OpenMPDirective *source_owner = nullptr;
    for (OpenMPDirective *candidate_owner : {end_payload, end_wrapper}) {
      const std::vector<OpenMPClause *> *candidates =
          candidate_owner->findClauses(merged->getKind());
      if (candidates == nullptr) {
        continue;
      }
      for (OpenMPClause *candidate : *candidates) {
        if (candidate == nullptr || source != nullptr) {
          std::cerr << "REX_OMP_AST_INVARIANT[end-clause-cache-transfer]: "
                       "merged clause has no unique END producer\n";
          ROSE_ABORT();
        }
        source = candidate;
        source_owner = candidate_owner;
      }
    }
    if (source == nullptr || !transferred_sources.insert(source).second) {
      std::cerr << "REX_OMP_AST_INVARIANT[end-clause-cache-transfer]: merged "
                   "clause has a missing or reused END producer\n";
      ROSE_ABORT();
    }

    OmpClauseParseCache *source_cache = cache_for(source_owner);
    auto semantic = source_cache->clause_expression_nodes.find(source);
    auto auxiliary =
        source_cache->clause_auxiliary_expression_nodes.find(source);
    auto source_text =
        source_cache->clause_source_expression_texts.find(source);
    auto source_auxiliary =
        source_cache->clause_source_auxiliary_expression_texts.find(source);
    if (semantic == source_cache->clause_expression_nodes.end() ||
        auxiliary == source_cache->clause_auxiliary_expression_nodes.end() ||
        source_text == source_cache->clause_source_expression_texts.end() ||
        source_auxiliary ==
            source_cache->clause_source_auxiliary_expression_texts.end() ||
        !begin_cache.clause_expression_nodes.emplace(merged, semantic->second)
             .second ||
        !begin_cache.clause_auxiliary_expression_nodes
             .emplace(merged, auxiliary->second)
             .second ||
        !begin_cache.clause_source_expression_texts
             .emplace(merged, source_text->second)
             .second ||
        !begin_cache.clause_source_auxiliary_expression_texts
             .emplace(merged, source_auxiliary->second)
             .second) {
      std::cerr << "REX_OMP_AST_INVARIANT[end-clause-cache-transfer]: exact "
                   "END cache is missing or begin cache is duplicate\n";
      ROSE_ABORT();
    }
    transferred_any = true;
  }

  bool end_has_transferable_clause = false;
  for (OpenMPDirective *owner : {end_payload, end_wrapper}) {
    for (OpenMPClauseKind kind : {OMPC_nowait, OMPC_copyprivate}) {
      const std::vector<OpenMPClause *> *clauses = owner->findClauses(kind);
      end_has_transferable_clause |= clauses != nullptr && !clauses->empty();
    }
  }
  if (end_has_transferable_clause != transferred_any) {
    std::cerr << "REX_OMP_AST_INVARIANT[end-clause-cache-transfer]: END clause "
                 "and merged begin cache disagree\n";
    ROSE_ABORT();
  }

  for (std::shared_ptr<OmpParsedExpression> &owned :
       end_cache_tree.root.owned_nodes) {
    begin_cache.owned_nodes.push_back(std::move(owned));
  }
}

const OmpClauseParseCache *getClauseParseCache(OpenMPDirective *directive) {
  auto found = openMPClauseNodes().find(directive);
  if (found == openMPClauseNodes().end()) {
    return nullptr;
  }
  return &found->second;
}

SgDeclarationScope *getDirectiveLocalScope(OpenMPDirective *directive) {
  const OmpClauseParseCache *cache = getClauseParseCache(directive);
  return cache != nullptr ? cache->directive_local_scope : nullptr;
}

const std::vector<const OmpParsedExpression *> *
getParsedClauseExpressionNodes(OpenMPDirective *directive,
                               const OpenMPClause *clause) {
  if (directive == nullptr || clause == nullptr) {
    std::cerr << "REX_OMP_AST_INVARIANT[clause-cache]: null directive or "
                 "clause in semantic-expression lookup\n";
    ROSE_ABORT();
  }
  const OmpClauseParseCache *cache = getClauseParseCache(directive);
  if (cache == nullptr) {
    std::cerr << "REX_OMP_AST_INVARIANT[clause-cache]: directive has no "
                 "semantic-expression cache\n";
    ROSE_ABORT();
  }
  auto found = cache->clause_expression_nodes.find(clause);
  if (found != cache->clause_expression_nodes.end()) {
    return &found->second;
  }
  std::cerr << "REX_OMP_AST_INVARIANT[clause-cache]: clause has no exact "
               "semantic-expression cache entry\n";
  ROSE_ABORT();
}

const std::vector<const OmpParsedExpression *> *
getParsedClauseAuxiliaryExpressionNodes(OpenMPDirective *directive,
                                        const OpenMPClause *clause) {
  if (directive == nullptr || clause == nullptr) {
    std::cerr << "REX_OMP_AST_INVARIANT[clause-cache]: null directive or "
                 "clause in auxiliary-expression lookup\n";
    ROSE_ABORT();
  }
  const OmpClauseParseCache *cache = getClauseParseCache(directive);
  if (cache == nullptr) {
    std::cerr << "REX_OMP_AST_INVARIANT[clause-cache]: directive has no "
                 "auxiliary-expression cache\n";
    ROSE_ABORT();
  }
  auto found = cache->clause_auxiliary_expression_nodes.find(clause);
  if (found != cache->clause_auxiliary_expression_nodes.end()) {
    return &found->second;
  }
  std::cerr << "REX_OMP_AST_INVARIANT[clause-cache]: clause has no exact "
               "auxiliary-expression cache entry\n";
  ROSE_ABORT();
}

const std::vector<std::string> *
getClauseSourceExpressionTexts(OpenMPDirective *directive,
                               const OpenMPClause *clause) {
  if (directive == nullptr || clause == nullptr) {
    std::cerr << "REX_OMP_AST_INVARIANT[source-expression-cache]: null "
                 "directive or clause\n";
    ROSE_ABORT();
  }
  const OmpClauseParseCache *cache = getClauseParseCache(directive);
  if (cache == nullptr) {
    std::cerr << "REX_OMP_AST_INVARIANT[source-expression-cache]: directive "
                 "has no semantic clause cache\n";
    ROSE_ABORT();
  }
  auto found = cache->clause_source_expression_texts.find(clause);
  if (found != cache->clause_source_expression_texts.end()) {
    return &found->second;
  }
  std::cerr << "REX_OMP_AST_INVARIANT[source-expression-cache]: clause has "
               "no exact source-expression cache entry\n";
  ROSE_ABORT();
}

const std::vector<std::string> *
getClauseSourceAuxiliaryExpressionTexts(OpenMPDirective *directive,
                                        const OpenMPClause *clause) {
  if (directive == nullptr || clause == nullptr) {
    std::cerr << "REX_OMP_AST_INVARIANT[source-expression-cache]: null "
                 "directive or clause for auxiliary lookup\n";
    ROSE_ABORT();
  }
  const OmpClauseParseCache *cache = getClauseParseCache(directive);
  if (cache == nullptr) {
    std::cerr << "REX_OMP_AST_INVARIANT[source-expression-cache]: directive "
                 "has no semantic auxiliary cache\n";
    ROSE_ABORT();
  }
  auto found = cache->clause_source_auxiliary_expression_texts.find(clause);
  if (found != cache->clause_source_auxiliary_expression_texts.end()) {
    return &found->second;
  }
  std::cerr << "REX_OMP_AST_INVARIANT[source-expression-cache]: clause has "
               "no exact source auxiliary-expression cache entry\n";
  ROSE_ABORT();
}

bool clauseOwnsIndependentSourceExpressionSpelling(OpenMPDirective *directive) {
  if (directive == nullptr) {
    std::cerr << "REX_OMP_AST_INVARIANT[source-expression-owner]: null "
                 "directive\n";
    ROSE_ABORT();
  }
  const OmpClauseParseCache *cache = getClauseParseCache(directive);
  if (cache == nullptr) {
    std::cerr << "REX_OMP_AST_INVARIANT[source-expression-owner]: directive "
                 "has no semantic clause cache\n";
    ROSE_ABORT();
  }
  if (!cache->directive_owns_source_expression_spelling) {
    return true;
  }
  if (!cache->clause_source_expression_texts.empty() ||
      !cache->clause_source_auxiliary_expression_texts.empty()) {
    std::cerr << "REX_OMP_AST_INVARIANT[source-expression-owner]: "
                 "directive-level macro spelling conflicts with clause-level "
                 "source spelling\n";
    ROSE_ABORT();
  }
  return false;
}

std::string
canonicalOpenMPExpressionSequence(const std::vector<std::string> &expressions) {
  std::string canonical;
  for (const std::string &expression : expressions) {
    canonical.push_back('\x1f');
    for (unsigned char ch : expression) {
      if (!std::isspace(ch)) {
        canonical.push_back(static_cast<char>(ch));
      }
    }
  }
  return canonical;
}

std::vector<std::string> parsedExpressionTexts(
    const std::vector<const OmpParsedExpression *> *parsed_nodes,
    OpenMPExprParseMode required_mode) {
  std::vector<std::string> expressions;
  if (parsed_nodes == nullptr) {
    std::cerr << "REX_OMP_AST_INVARIANT[clause-cache]: source spelling has no "
                 "semantic expression sequence\n";
    ROSE_ABORT();
  }
  for (const OmpParsedExpression *parsed : *parsed_nodes) {
    requireCachedParsedExpression(parsed);
    if (parsed->mode == required_mode) {
      expressions.push_back(parsed->text);
    }
  }
  return expressions;
}

SgOmpSourceExpression *
buildOpenMPSourceExpression(const std::string &source_spelling) {
  const std::string spelling = trimWhitespaceCopy(source_spelling);
  if (spelling.empty()) {
    std::cerr << "REX_OMP_AST_INVARIANT[source-expression]: empty source "
                 "spelling\n";
    ROSE_ABORT();
  }
  SgOmpSourceExpression *source = new SgOmpSourceExpression(spelling);
  setOneSourcePositionForTransformation(source);
  return source;
}

void attachOriginalOpenMPExpressionSpelling(
    OpenMPDirective *directive, OpenMPClause *clause,
    const std::vector<const OmpParsedExpression *> *parsed_nodes,
    SgExpression *semantic_expression) {
  if (!clauseOwnsIndependentSourceExpressionSpelling(directive)) {
    return;
  }
  const std::vector<std::string> *source_expressions =
      getClauseSourceExpressionTexts(directive, clause);
  if (source_expressions == nullptr) {
    return;
  }
  if (SgExprListExp *semantic_list = isSgExprListExp(semantic_expression)) {
    const std::vector<std::string> semantic_expressions =
        parsedExpressionTexts(parsed_nodes, OMP_EXPR_PARSE_expression);
    SgExpressionPtrList &elements = semantic_list->get_expressions();
    if (source_expressions->size() != semantic_expressions.size() ||
        source_expressions->size() != elements.size() || elements.empty()) {
      std::cerr << "REX_OMP_AST_INVARIANT[source-expression-list]: source, "
                   "semantic callback, and Sage operand counts diverge: source="
                << source_expressions->size()
                << " semantic=" << semantic_expressions.size()
                << " sage=" << elements.size() << "\n";
      ROSE_ABORT();
    }
    for (size_t index = 0; index < elements.size(); ++index) {
      SgExpression *element = elements[index];
      if (element == nullptr) {
        std::cerr << "REX_OMP_AST_INVARIANT[source-expression-list]: null "
                     "Sage operand\n";
        ROSE_ABORT();
      }
      const std::vector<std::string> source_operand = {
          (*source_expressions)[index]};
      const std::vector<std::string> semantic_operand = {
          semantic_expressions[index]};
      if (canonicalOpenMPExpressionSequence(source_operand) ==
          canonicalOpenMPExpressionSequence(semantic_operand)) {
        continue;
      }
      if (element->get_originalExpressionTree() != nullptr ||
          (isSgValueExp(element) == nullptr &&
           isSgBinaryOp(element) == nullptr &&
           isSgVarRefExp(element) == nullptr &&
           isSgCastExp(element) == nullptr &&
           isSgFunctionRefExp(element) == nullptr)) {
        std::cerr << "REX_OMP_AST_INVARIANT[source-expression-list]: Sage "
                     "operand cannot own one exact original expression\n";
        ROSE_ABORT();
      }
      SgOmpSourceExpression *source =
          buildOpenMPSourceExpression((*source_expressions)[index]);
      element->set_originalExpressionTree(source);
      source->set_parent(element);
    }
    return;
  }
  if (SgStringVal *literal = isSgStringVal(semantic_expression)) {
    if (directive == nullptr) {
      std::cerr << "REX_OMP_AST_INVARIANT[string-literal]: semantic OpenMP "
                   "literal has no exact directive language owner\n";
      ROSE_ABORT();
    }
    char delimiter = '\0';
    switch (directive->getBaseLang()) {
    case Lang_C:
    case Lang_Cplusplus:
      if (literal->get_stringDelimiter() != 0) {
        std::cerr << "REX_OMP_AST_INVARIANT[string-literal]: C/C++ OpenMP "
                     "literal carries Fortran delimiter state\n";
        ROSE_ABORT();
      }
      delimiter = '"';
      break;
    case Lang_Fortran:
      delimiter = literal->get_stringDelimiter();
      if (delimiter != '\'' && delimiter != '"') {
        std::cerr << "REX_OMP_AST_INVARIANT[string-literal]: Fortran OpenMP "
                     "literal has no exact source delimiter\n";
        ROSE_ABORT();
      }
      break;
    default:
      std::cerr << "REX_OMP_AST_INVARIANT[string-literal]: semantic OpenMP "
                   "literal has an invalid directive language\n";
      ROSE_ABORT();
    }
    const std::string semantic_spelling =
        delimiter + literal->get_value() + delimiter;
    if (source_expressions->size() == 1 &&
        trimWhitespaceCopy(source_expressions->front()) == semantic_spelling) {
      return;
    }
  }
  if (SgOmpSourceExpression *source =
          isSgOmpSourceExpression(semantic_expression)) {
    const std::vector<std::string> verbatim_expressions =
        parsedExpressionTexts(parsed_nodes, OMP_EXPR_PARSE_verbatim);
    if (source_expressions->size() != 1 || verbatim_expressions.size() != 1 ||
        trimWhitespaceCopy(source_expressions->front()) !=
            source->get_spelling() ||
        verbatim_expressions.front() != source->get_spelling()) {
      std::cerr << "REX_OMP_AST_INVARIANT[source-expression]: typed source "
                   "expression does not match one exact verbatim callback "
                   "record\n";
      ROSE_ABORT();
    }
    return;
  }
  const std::vector<std::string> semantic_expressions =
      parsedExpressionTexts(parsed_nodes, OMP_EXPR_PARSE_expression);
  if (canonicalOpenMPExpressionSequence(*source_expressions) ==
      canonicalOpenMPExpressionSequence(semantic_expressions)) {
    return;
  }
  if (semantic_expression == nullptr || source_expressions->size() != 1 ||
      semantic_expressions.size() != 1) {
    std::cerr << "REX_OMP_AST_INVARIANT[source-expression]: macro-expanded "
                 "scalar clause does not have one source and one semantic "
                 "expression\n";
    ROSE_ABORT();
  }
  if (semantic_expression->get_originalExpressionTree() != nullptr) {
    std::cerr << "REX_OMP_AST_INVARIANT[source-expression]: semantic "
                 "expression already owns an original expression tree\n";
    ROSE_ABORT();
  }
  if (isSgValueExp(semantic_expression) == nullptr &&
      isSgBinaryOp(semantic_expression) == nullptr &&
      isSgVarRefExp(semantic_expression) == nullptr &&
      isSgCastExp(semantic_expression) == nullptr &&
      isSgFunctionRefExp(semantic_expression) == nullptr) {
    std::cerr << "REX_OMP_AST_INVARIANT[source-expression]: semantic root "
              << semantic_expression->class_name()
              << " cannot own an original expression tree\n";
    ROSE_ABORT();
  }
  SgOmpSourceExpression *source =
      buildOpenMPSourceExpression(source_expressions->front());
  semantic_expression->set_originalExpressionTree(source);
  source->set_parent(semantic_expression);
}

void attachOriginalOpenMPVariableSpelling(
    OpenMPDirective *directive, OpenMPClause *clause,
    const std::vector<const OmpParsedExpression *> *parsed_nodes,
    SgOmpVariablesClause *semantic_clause) {
  ROSE_ASSERT(semantic_clause != nullptr);
  if (!clauseOwnsIndependentSourceExpressionSpelling(directive)) {
    return;
  }
  const std::vector<std::string> *source_expressions =
      getClauseSourceExpressionTexts(directive, clause);
  if (source_expressions == nullptr) {
    return;
  }

  std::vector<std::string> semantic_expressions =
      parsedExpressionTexts(parsed_nodes, OMP_EXPR_PARSE_variable_list);
  const std::vector<std::string> array_sections =
      parsedExpressionTexts(parsed_nodes, OMP_EXPR_PARSE_array_section);
  semantic_expressions.insert(semantic_expressions.end(),
                              array_sections.begin(), array_sections.end());
  if (clause->getKind() == OMPC_depend || clause->getKind() == OMPC_affinity) {
    const std::vector<std::string> expressions =
        parsedExpressionTexts(parsed_nodes, OMP_EXPR_PARSE_expression);
    semantic_expressions.insert(semantic_expressions.end(), expressions.begin(),
                                expressions.end());
  }
  const bool preserve_exact_fortran_spelling =
      directive != nullptr && directive->getBaseLang() == Lang_Fortran;
  if (!preserve_exact_fortran_spelling &&
      canonicalOpenMPExpressionSequence(*source_expressions) ==
          canonicalOpenMPExpressionSequence(semantic_expressions)) {
    return;
  }
  if (source_expressions->empty() || semantic_expressions.empty() ||
      semantic_clause->get_variables() == nullptr ||
      semantic_clause->get_variables()->get_expressions().empty()) {
    std::cerr << "REX_OMP_AST_INVARIANT[source-variables]: macro-expanded "
                 "clause has an empty source or semantic variable list\n";
    ROSE_ABORT();
  }
  if (semantic_clause->get_source_variables() != nullptr ||
      semantic_clause->get_has_source_variables_override()) {
    std::cerr << "REX_OMP_AST_INVARIANT[source-variables]: source variable "
                 "list already attached\n";
    ROSE_ABORT();
  }

  SgExprListExp *source_list = SageBuilder::buildExprListExp();
  ROSE_ASSERT(source_list != nullptr);
  for (const std::string &spelling : *source_expressions) {
    SgOmpSourceExpression *source = buildOpenMPSourceExpression(spelling);
    source_list->append_expression(source);
  }
  semantic_clause->set_source_variables(source_list);
  semantic_clause->set_has_source_variables_override(true);
  source_list->set_parent(semantic_clause);
}

const std::vector<std::vector<const OmpParsedExpression *>> *
getParsedMapDistDataPolicyNodes(OpenMPDirective *directive,
                                const OpenMPClause *clause) {
  if (directive == nullptr || clause == nullptr) {
    std::cerr << "REX_OMP_AST_INVARIANT[map-cache]: null directive or map "
                 "clause in dist_data node lookup\n";
    ROSE_ABORT();
  }
  const OmpClauseParseCache *cache = getClauseParseCache(directive);
  if (cache == nullptr) {
    std::cerr << "REX_OMP_AST_INVARIANT[map-cache]: directive has no semantic "
                 "map cache\n";
    ROSE_ABORT();
  }
  auto found = cache->map_dist_data_policy_nodes.find(clause);
  if (found != cache->map_dist_data_policy_nodes.end()) {
    return &found->second;
  }
  std::cerr << "REX_OMP_AST_INVARIANT[map-cache]: map clause has no exact "
               "dist_data node cache entry\n";
  ROSE_ABORT();
}

const std::vector<std::vector<OpenMPMapClause::DistDataPolicy>> *
getParsedMapDistDataPolicies(OpenMPDirective *directive,
                             const OpenMPClause *clause) {
  if (directive == nullptr || clause == nullptr) {
    std::cerr << "REX_OMP_AST_INVARIANT[map-cache]: null directive or map "
                 "clause in dist_data policy lookup\n";
    ROSE_ABORT();
  }
  const OmpClauseParseCache *cache = getClauseParseCache(directive);
  if (cache == nullptr) {
    std::cerr << "REX_OMP_AST_INVARIANT[map-cache]: directive has no semantic "
                 "map policy cache\n";
    ROSE_ABORT();
  }
  auto found = cache->map_dist_data_policies.find(clause);
  if (found != cache->map_dist_data_policies.end()) {
    return &found->second;
  }
  std::cerr << "REX_OMP_AST_INVARIANT[map-cache]: map clause has no exact "
               "dist_data policy cache entry\n";
  ROSE_ABORT();
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
  case OpenMPMapClause::DIST_DATA_unknown:
    break;
  }
  MLOG_ERROR_C("ompAstConstruction",
               "Unsupported dist_data policy kind in map clause\n");
  ROSE_ABORT();
}

void appendParsedVariableNode(const OmpParsedExpression *parsed) {
  requireCachedParsedExpression(parsed);
  if (parsed->consumed) {
    std::cerr << "REX_OMP_AST_INVARIANT[expression-cache-consumption]: "
                 "cached variable-list item was consumed more than once\n";
    ROSE_ABORT();
  }
  if (isSgInitializedName(parsed->node) == nullptr &&
      isSgExpression(parsed->node) == nullptr) {
    std::cerr << "REX_OMP_AST_INVARIANT[variable-list-item]: cached OpenMP "
                 "list item is neither an initialized name nor an expression\n";
    ROSE_ABORT();
  }
  if (SgExpression *expression = isSgExpression(parsed->node)) {
    if (expression->get_parent() != nullptr) {
      std::cerr << "REX_OMP_AST_INVARIANT[expression-cache-consumption]: "
                   "cached variable-list expression already has a structural "
                   "owner\n";
      ROSE_ABORT();
    }
  }
  parsed->consumed = true;
  // Preserve the complete typed expression. In particular, an array section
  // is structurally owned by the clause variable list; it must never be
  // reduced to a base symbol plus detached lower/length metadata.
  openMPExpressionVariables().push_back(parsed->node);
}

SgExpression *consumeParsedClauseExpression(
    OpenMPClauseKind clause_kind,
    const std::vector<const OmpParsedExpression *> *parsed_nodes,
    size_t &next_index, const std::string &expression_text,
    OpenMPExprParseMode required_mode = OMP_EXPR_PARSE_expression) {
  const std::string expected_text = trimWhitespaceCopy(expression_text);
  if (expected_text.empty() || parsed_nodes == nullptr ||
      next_index >= parsed_nodes->size()) {
    std::cerr << "REX_OMP_AST_INVARIANT[expression-cache-order]: clause "
              << static_cast<int>(clause_kind)
              << " has no exact ordered cache record at item " << next_index
              << "\n";
    ROSE_ABORT();
  }
  const OmpParsedExpression *parsed = (*parsed_nodes)[next_index++];
  requireCachedParsedExpression(parsed);
  // Clause grammar owns leading/trailing separator whitespace, while the host
  // fragment owns every character between those boundaries. Compare the
  // canonical boundary spelling without changing the source-faithful cache.
  if (parsed->mode != required_mode ||
      trimWhitespaceCopy(parsed->text) != expected_text) {
    std::cerr << "REX_OMP_AST_INVARIANT[expression-cache-order]: clause "
              << static_cast<int>(clause_kind) << " item " << next_index - 1
              << " role/text mismatch; expected mode="
              << static_cast<int>(required_mode) << " text='" << expected_text
              << "', cached mode=" << static_cast<int>(parsed->mode)
              << " text='" << parsed->text << "'\n";
    ROSE_ABORT();
  }
  return consumeParsedExpressionNode(parsed);
}

std::string consumeParsedClauseOpenMPSyntax(
    OpenMPClauseKind clause_kind,
    const std::vector<const OmpParsedExpression *> *parsed_nodes,
    size_t &next_index, const std::string &syntax_text) {
  const std::string expected_text = trimWhitespaceCopy(syntax_text);
  if (expected_text.empty() || parsed_nodes == nullptr ||
      next_index >= parsed_nodes->size()) {
    std::cerr << "REX_OMP_AST_INVARIANT[openmp-syntax-cache]: clause "
              << static_cast<int>(clause_kind)
              << " has no exact ordered syntax record at item " << next_index
              << "\n";
    ROSE_ABORT();
  }
  const OmpParsedExpression *parsed = (*parsed_nodes)[next_index++];
  if (parsed == nullptr || parsed->consumed || parsed->node != nullptr ||
      parsed->mode != OMP_EXPR_PARSE_openmp_syntax ||
      parsed->text != expected_text) {
    std::cerr << "REX_OMP_AST_INVARIANT[openmp-syntax-cache]: clause "
              << static_cast<int>(clause_kind) << " item " << next_index - 1
              << " is not one exact unconsumed OpenMP grammar payload\n";
    ROSE_ABORT();
  }
  parsed->consumed = true;
  return expected_text;
}

SgOmpNameExpression *consumeParsedClauseOpenMPName(
    OpenMPClauseKind clause_kind,
    const std::vector<const OmpParsedExpression *> *parsed_nodes,
    const std::string &expected_text) {
  if (expected_text.empty() || parsed_nodes == nullptr) {
    std::cerr << "REX_OMP_AST_INVARIANT[openmp-name-cache]: clause "
              << static_cast<int>(clause_kind)
              << " has no exact OpenMP name callback\n";
    ROSE_ABORT();
  }
  const OmpParsedExpression *selected = nullptr;
  for (const OmpParsedExpression *parsed : *parsed_nodes) {
    if (parsed != nullptr && parsed->mode == OMP_EXPR_PARSE_openmp_syntax &&
        parsed->text == expected_text) {
      if (selected != nullptr) {
        std::cerr << "REX_OMP_AST_INVARIANT[openmp-name-cache]: clause "
                  << static_cast<int>(clause_kind)
                  << " has duplicate exact OpenMP name callbacks\n";
        ROSE_ABORT();
      }
      selected = parsed;
    }
  }
  if (selected == nullptr || selected->consumed || selected->node != nullptr) {
    std::cerr << "REX_OMP_AST_INVARIANT[openmp-name-cache]: clause "
              << static_cast<int>(clause_kind)
              << " name is not one exact unconsumed OpenMP grammar payload\n";
    ROSE_ABORT();
  }
  selected->consumed = true;
  return buildOpenMPNameExpression(expected_text);
}

void requireParsedClauseExpressionsConsumed(
    OpenMPClauseKind clause_kind,
    const std::vector<const OmpParsedExpression *> *parsed_nodes,
    size_t next_index) {
  if (parsed_nodes == nullptr || next_index != parsed_nodes->size()) {
    std::cerr << "REX_OMP_AST_INVARIANT[expression-cache-order]: clause "
              << static_cast<int>(clause_kind) << " consumed " << next_index
              << " of " << (parsed_nodes == nullptr ? 0 : parsed_nodes->size())
              << " exact cache records\n";
    ROSE_ABORT();
  }
}

std::string trimWhitespaceCopy(const std::string &value) {
  const std::string::size_type begin = value.find_first_not_of(" \t\r\n");
  if (begin == std::string::npos) {
    return std::string();
  }
  const std::string::size_type end = value.find_last_not_of(" \t\r\n");
  return value.substr(begin, end - begin + 1);
}

SgOmpClause::omp_directive_kind_enum
convertDirectiveKind(OpenMPDirectiveKind kind) {
  switch (kind) {
  case OMPD_parallel:
    return SgOmpClause::e_omp_directive_kind_parallel;
  case OMPD_for:
    return SgOmpClause::e_omp_directive_kind_for;
  case OMPD_do:
    return SgOmpClause::e_omp_directive_kind_do;
  case OMPD_simd:
    return SgOmpClause::e_omp_directive_kind_simd;
  case OMPD_target:
    return SgOmpClause::e_omp_directive_kind_target;
  case OMPD_teams:
    return SgOmpClause::e_omp_directive_kind_teams;
  case OMPD_distribute:
    return SgOmpClause::e_omp_directive_kind_distribute;
  case OMPD_task:
    return SgOmpClause::e_omp_directive_kind_task;
  case OMPD_taskloop:
    return SgOmpClause::e_omp_directive_kind_taskloop;
  case OMPD_sections:
    return SgOmpClause::e_omp_directive_kind_sections;
  case OMPD_section:
    return SgOmpClause::e_omp_directive_kind_section;
  case OMPD_single:
    return SgOmpClause::e_omp_directive_kind_single;
  case OMPD_master:
    return SgOmpClause::e_omp_directive_kind_master;
  case OMPD_masked:
    return SgOmpClause::e_omp_directive_kind_masked;
  case OMPD_critical:
    return SgOmpClause::e_omp_directive_kind_critical;
  case OMPD_barrier:
    return SgOmpClause::e_omp_directive_kind_barrier;
  case OMPD_taskwait:
    return SgOmpClause::e_omp_directive_kind_taskwait;
  case OMPD_taskgroup:
    return SgOmpClause::e_omp_directive_kind_taskgroup;
  case OMPD_atomic:
    return SgOmpClause::e_omp_directive_kind_atomic;
  case OMPD_flush:
    return SgOmpClause::e_omp_directive_kind_flush;
  case OMPD_ordered:
    return SgOmpClause::e_omp_directive_kind_ordered;
  case OMPD_scan:
    return SgOmpClause::e_omp_directive_kind_scan;
  case OMPD_scope:
    return SgOmpClause::e_omp_directive_kind_scope;
  case OMPD_loop:
    return SgOmpClause::e_omp_directive_kind_loop;
  case OMPD_workshare:
    return SgOmpClause::e_omp_directive_kind_workshare;
  case OMPD_cancel:
    return SgOmpClause::e_omp_directive_kind_cancel;
  case OMPD_metadirective:
    return SgOmpClause::e_omp_directive_kind_metadirective;
  default:
    std::cerr << "REX_OMP_AST_INVARIANT[directive-kind-list]: unsupported "
                 "OpenMP directive kind="
              << static_cast<int>(kind) << "\n";
    ROSE_ABORT();
  }
}

SgOmpNameExpression *
parseMapperIdentifierExpression(OpenMPClauseKind clause_kind,
                                const OmpParsedExpression *parsed,
                                const std::string &raw_identifier_text) {
  if (raw_identifier_text.empty() ||
      !isSimpleMapperIdentifier(raw_identifier_text)) {
    std::cerr << "REX_OMP_AST_INVARIANT[mapper-identifier]: clause "
              << static_cast<int>(clause_kind)
              << " has an invalid mapper identifier token '"
              << raw_identifier_text << "'\n";
    ROSE_ABORT();
  }
  requireCachedParsedExpression(parsed);
  const OpenMPExprParseMode expected_mode =
      clause_kind == OMPC_unknown
          ? OMP_EXPR_PARSE_openmp_declare_mapper_identifier
          : OMP_EXPR_PARSE_verbatim;
  if (parsed->mode != expected_mode || parsed->text != raw_identifier_text) {
    std::cerr << "REX_OMP_AST_INVARIANT[mapper-identifier]: exact cached "
                 "token role or spelling disagrees with parser IR\n";
    ROSE_ABORT();
  }
  SgOmpNameExpression *cached = isSgOmpNameExpression(parsed->node);
  if (cached == nullptr || cached->get_spelling() != raw_identifier_text) {
    std::cerr << "REX_OMP_AST_INVARIANT[mapper-identifier]: cached token is "
                 "not the exact typed OpenMP name\n";
    ROSE_ABORT();
  }
  SgOmpNameExpression *result =
      isSgOmpNameExpression(consumeParsedExpressionNode(parsed));
  if (result == nullptr || result->get_parent() != nullptr) {
    std::cerr << "REX_OMP_AST_INVARIANT[mapper-identifier]: cloned token is "
                 "not an unowned typed OpenMP name\n";
    ROSE_ABORT();
  }
  return result;
}

std::string consumeCriticalDirectiveName(OpenMPDirective *directive) {
  auto *critical = dynamic_cast<OpenMPCriticalDirective *>(directive);
  const OmpClauseParseCache *cache = getClauseParseCache(directive);
  if (critical == nullptr || cache == nullptr) {
    std::cerr << "REX_OMP_AST_INVARIANT[critical-name]: critical directive "
                 "has no exact typed parser cache\n";
    ROSE_ABORT();
  }

  const std::string &name = critical->getCriticalName();
  if (name.empty()) {
    if (!cache->directive_expression_nodes.empty()) {
      std::cerr << "REX_OMP_AST_INVARIANT[critical-name]: unnamed critical "
                   "directive owns a name callback\n";
      ROSE_ABORT();
    }
    return name;
  }
  if (cache->directive_expression_nodes.size() != 1) {
    std::cerr << "REX_OMP_AST_INVARIANT[critical-name]: named critical "
                 "directive has no unique exact name callback\n";
    ROSE_ABORT();
  }
  const OmpParsedExpression *parsed = cache->directive_expression_nodes.front();
  if (parsed == nullptr || parsed->consumed || parsed->node != nullptr ||
      parsed->mode != OMP_EXPR_PARSE_openmp_syntax || parsed->text != name) {
    std::cerr << "REX_OMP_AST_INVARIANT[critical-name]: cached callback is "
                 "not the exact unconsumed OpenMP grammar name\n";
    ROSE_ABORT();
  }
  parsed->consumed = true;
  return name;
}

SgExpression *parseClauseExpressionWithCache(
    OpenMPClauseKind clause_kind,
    const std::vector<const OmpParsedExpression *> *parsed_nodes,
    size_t &next_index, const std::string &expression_text,
    bool prefer_verbatim = false) {
  const OpenMPExprParseMode preferred_mode =
      prefer_verbatim ? OMP_EXPR_PARSE_verbatim : OMP_EXPR_PARSE_expression;
  return consumeParsedClauseExpression(clause_kind, parsed_nodes, next_index,
                                       expression_text, preferred_mode);
}

SgOmpInitModifierList *buildInitModifierList(
    OpenMPClauseKind clause_kind, const OpenMPInitModifierList &ir_modifiers,
    const std::vector<const OmpParsedExpression *> *parsed_nodes,
    size_t &parsed_node_index, const char *contract) {
  if (contract == nullptr) {
    std::cerr << "REX_OMP_AST_INVARIANT[init-modifier-list]: missing "
                 "construction contract\n";
    ROSE_ABORT();
  }
  SgOmpInitModifierList *result = new SgOmpInitModifierList();
  setOneSourcePositionForTransformation(result);
  auto append_modifier = [&](SgOmpClause::omp_init_modifier_kind_enum kind,
                             SgExpression *expression) {
    SgOmpInitModifier *modifier = new SgOmpInitModifier(kind, expression);
    setOneSourcePositionForTransformation(modifier);
    if (expression != nullptr) {
      expression->set_parent(modifier);
    }
    result->get_modifiers().push_back(modifier);
    modifier->set_parent(result);
  };
  auto consume_argument = [&](const OpenMPInitModifier &modifier) {
    if (modifier.argument.spelling.empty() || parsed_nodes == nullptr ||
        parsed_node_index >= parsed_nodes->size()) {
      std::cerr << "REX_OMP_AST_INVARIANT[" << contract
                << "]: modifier argument has no exact cached expression\n";
      ROSE_ABORT();
    }
    return consumeParsedClauseExpression(
        clause_kind, parsed_nodes, parsed_node_index,
        modifier.argument.spelling, modifier.argument.parse_mode);
  };

  for (const OpenMPInitModifier &modifier : ir_modifiers.getModifiers()) {
    switch (modifier.category) {
    case OpenMPInitModifierCategory::DirectiveName:
      switch (modifier.directive_name) {
      case OMPD_depobj:
        append_modifier(SgOmpClause::e_omp_init_modifier_depobj, nullptr);
        break;
      case OMPD_interop:
        append_modifier(SgOmpClause::e_omp_init_modifier_interop, nullptr);
        break;
      default:
        std::cerr << "REX_OMP_AST_INVARIANT[" << contract
                  << "]: unsupported directive-name modifier\n";
        ROSE_ABORT();
      }
      break;
    case OpenMPInitModifierCategory::PreferType:
      append_modifier(SgOmpClause::e_omp_init_modifier_prefer_type,
                      consume_argument(modifier));
      break;
    case OpenMPInitModifierCategory::Depinfo: {
      SgOmpClause::omp_init_modifier_kind_enum kind =
          SgOmpClause::e_omp_init_modifier_unknown;
      switch (modifier.dependence_type) {
      case OMPC_DEPENDENCE_TYPE_in:
        kind = SgOmpClause::e_omp_init_modifier_depinfo_in;
        break;
      case OMPC_DEPENDENCE_TYPE_out:
        kind = SgOmpClause::e_omp_init_modifier_depinfo_out;
        break;
      case OMPC_DEPENDENCE_TYPE_inout:
        kind = SgOmpClause::e_omp_init_modifier_depinfo_inout;
        break;
      case OMPC_DEPENDENCE_TYPE_inoutset:
        kind = SgOmpClause::e_omp_init_modifier_depinfo_inoutset;
        break;
      case OMPC_DEPENDENCE_TYPE_mutexinoutset:
        kind = SgOmpClause::e_omp_init_modifier_depinfo_mutexinoutset;
        break;
      default:
        std::cerr << "REX_OMP_AST_INVARIANT[" << contract
                  << "]: unsupported depinfo modifier\n";
        ROSE_ABORT();
      }
      append_modifier(kind, consume_argument(modifier));
      break;
    }
    case OpenMPInitModifierCategory::InteropType:
      switch (modifier.interop_type) {
      case OMPC_INIT_KIND_target:
        append_modifier(SgOmpClause::e_omp_init_modifier_target, nullptr);
        break;
      case OMPC_INIT_KIND_targetsync:
        append_modifier(SgOmpClause::e_omp_init_modifier_targetsync, nullptr);
        break;
      default:
        std::cerr << "REX_OMP_AST_INVARIANT[" << contract
                  << "]: unsupported interop-type modifier\n";
        ROSE_ABORT();
      }
      break;
    }
  }
  return result;
}

SgOmpIteratorDefinitionPtrList buildClauseIteratorDefinitions(
    const std::vector<const OmpParsedExpression *> *parsed_nodes,
    const std::vector<std::string> *source_expressions,
    const std::vector<OpenMPIterator> &iterators) {
  SgOmpIteratorDefinitionPtrList definitions;

  if (source_expressions != nullptr &&
      (parsed_nodes == nullptr ||
       source_expressions->size() != parsed_nodes->size())) {
    std::cerr << "REX_OMP_AST_INVARIANT[iterator-source]: semantic and source "
                 "iterator-expression counts differ\n";
    ROSE_ABORT();
  }
  std::size_t expression_index = 0;

  auto parse_iterator_expression =
      [&](const ompparser::HostFragment &fragment, bool required,
          OpenMPExprParseMode required_mode) -> SgExpression * {
    const std::string trimmed = trimWhitespaceCopy(fragment.spelling);
    if (trimmed.empty()) {
      if (required) {
        std::cerr << "REX_OMP_AST_INVARIANT[iterator-expression]: missing "
                     "required expression\n";
        ROSE_ABORT();
      }
      return nullptr;
    }
    if (parsed_nodes == nullptr || expression_index >= parsed_nodes->size()) {
      std::cerr << "REX_OMP_AST_INVARIANT[iterator-expression]: iterator "
                   "definition has no matching cached expression\n";
      ROSE_ABORT();
    }
    const OmpParsedExpression *parsed = requireCachedHostFragment(
        fragment, (*parsed_nodes)[expression_index], "iterator-expression");
    requireCachedParsedExpression(parsed);
    if (parsed->mode != required_mode ||
        trimWhitespaceCopy(parsed->text) != trimmed) {
      std::cerr << "REX_OMP_AST_INVARIANT[iterator-expression]: iterator "
                   "definition and cached expression role or spelling "
                   "disagree: '"
                << trimmed << "' versus '" << parsed->text << "'\n";
      ROSE_ABORT();
    }
    SgExpression *expression = consumeParsedExpressionNode(parsed);
    if (source_expressions != nullptr) {
      const std::string source =
          trimWhitespaceCopy((*source_expressions)[expression_index]);
      if (source.empty()) {
        std::cerr << "REX_OMP_AST_INVARIANT[iterator-source]: empty source "
                     "expression\n";
        ROSE_ABORT();
      }
      if (canonicalOpenMPExpressionSequence({source}) !=
          canonicalOpenMPExpressionSequence({trimmed})) {
        if (expression->get_originalExpressionTree() != nullptr) {
          std::cerr << "REX_OMP_AST_INVARIANT[iterator-source]: semantic "
                       "expression already owns source provenance\n";
          ROSE_ABORT();
        }
        SgOmpSourceExpression *original = buildOpenMPSourceExpression(source);
        expression->set_originalExpressionTree(original);
        original->set_parent(expression);
      }
    }
    ++expression_index;
    return expression;
  };

  auto build_iterator_type =
      [&](const ompparser::HostFragment &fragment) -> SgTypeExpression * {
    const std::string trimmed = trimWhitespaceCopy(fragment.spelling);
    if (trimmed.empty()) {
      return nullptr;
    }
    SgExpression *expression = parse_iterator_expression(
        fragment, true, OMP_EXPR_PARSE_openmp_iterator_type);
    SgTypeExpression *type = isSgTypeExpression(expression);
    if (type == nullptr) {
      std::cerr << "REX_OMP_AST_INVARIANT[iterator-type]: cached node for '"
                << trimmed << "' is not a type expression\n";
      ROSE_ABORT();
    }
    return type;
  };

  auto build_iterator_name =
      [&](const ompparser::HostFragment &fragment) -> SgOmpNameExpression * {
    const std::string trimmed = trimWhitespaceCopy(fragment.spelling);
    if (trimmed.empty() || !isSimpleMapperIdentifier(trimmed)) {
      std::cerr << "REX_OMP_AST_INVARIANT[iterator-name]: invalid iterator "
                   "identifier '"
                << trimmed << "'\n";
      ROSE_ABORT();
    }
    SgExpression *expression = parse_iterator_expression(
        fragment, true, OMP_EXPR_PARSE_openmp_iterator_name);
    SgOmpNameExpression *name = isSgOmpNameExpression(expression);
    if (name == nullptr) {
      std::cerr << "REX_OMP_AST_INVARIANT[iterator-name]: cached node for '"
                << trimmed << "' is not an OpenMP identifier\n";
      ROSE_ABORT();
    }
    return name;
  };

  auto parse_iterator_range = [&](const ompparser::HostFragment &fragment,
                                  bool required) -> SgExpression * {
    return parse_iterator_expression(fragment, required,
                                     OMP_EXPR_PARSE_expression);
  };

  for (const OpenMPIterator &iterator_def : iterators) {
    SgTypeExpression *type = build_iterator_type(iterator_def.qualifier);
    SgOmpNameExpression *name = build_iterator_name(iterator_def.variable);
    SgExpression *begin = parse_iterator_range(iterator_def.begin, true);
    SgExpression *end = parse_iterator_range(iterator_def.end, true);
    SgExpression *step = parse_iterator_range(iterator_def.step, false);
    SgOmpIteratorDefinition *definition =
        new SgOmpIteratorDefinition(type, name, begin, end, step);
    if (type != nullptr) {
      type->set_parent(definition);
    }
    name->set_parent(definition);
    begin->set_parent(definition);
    end->set_parent(definition);
    if (step != nullptr) {
      step->set_parent(definition);
    }
    definitions.push_back(definition);
  }

  if (parsed_nodes != nullptr && expression_index != parsed_nodes->size()) {
    std::cerr << "REX_OMP_AST_INVARIANT[iterator-expression]: cached iterator "
                 "expressions were not consumed exactly\n";
    ROSE_ABORT();
  }

  return definitions;
}

void ownClauseIteratorDefinitions(
    SgOmpClause *owner, const SgOmpIteratorDefinitionPtrList &definitions,
    bool required) {
  if (owner == nullptr || required != !definitions.empty()) {
    std::cerr << "REX_OMP_AST_INVARIANT[iterator-owner]: "
              << (owner != nullptr ? owner->class_name()
                                   : std::string("<null-clause>"))
              << " has no required iterator definition\n";
    ROSE_ABORT();
  }
  for (SgOmpIteratorDefinition *definition : definitions) {
    if (definition == nullptr || definition->get_parent() != nullptr) {
      std::cerr << "REX_OMP_AST_INVARIANT[iterator-owner]: "
                << owner->class_name()
                << " has a null or already-owned iterator definition\n";
      ROSE_ABORT();
    }
    if (definition->get_iterator_name() == nullptr ||
        definition->get_begin() == nullptr ||
        definition->get_end() == nullptr ||
        definition->get_iterator_name()->get_spelling().empty() ||
        (definition->get_iterator_type() != nullptr &&
         definition->get_iterator_type()->get_parent() != definition) ||
        definition->get_iterator_name()->get_parent() != definition ||
        definition->get_begin()->get_parent() != definition ||
        definition->get_end()->get_parent() != definition ||
        (definition->get_step() != nullptr &&
         definition->get_step()->get_parent() != definition)) {
      std::cerr << "REX_OMP_AST_INVARIANT[iterator-expression]: "
                << owner->class_name()
                << " has a malformed typed iterator definition\n";
      ROSE_ABORT();
    }
    std::set<SgExpression *> fields = {definition->get_iterator_name(),
                                       definition->get_begin(),
                                       definition->get_end()};
    const size_t required_field_count =
        3 + (definition->get_iterator_type() != nullptr) +
        (definition->get_step() != nullptr);
    if (definition->get_iterator_type() != nullptr) {
      fields.insert(definition->get_iterator_type());
    }
    if (definition->get_step() != nullptr) {
      fields.insert(definition->get_step());
    }
    if (fields.size() != required_field_count) {
      std::cerr << "REX_OMP_AST_INVARIANT[iterator-expression]: "
                << owner->class_name()
                << " aliases one syntax node across iterator roles\n";
      ROSE_ABORT();
    }
    definition->set_parent(owner);
  }
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

bool buildOmpListItemIdentity(SgNode *node, OmpListItemIdentity &identity) {
  if (node == nullptr) {
    return false;
  }

  if (SgInitializedName *initialized_name = isSgInitializedName(node)) {
    SgVariableSymbol *symbol = isSgVariableSymbol(
        initialized_name->search_for_symbol_from_symbol_table());
    if (symbol == nullptr || identity.root != nullptr) {
      return false;
    }
    identity.root = symbol;
    return true;
  }

  if (SgVarRefExp *var_ref = isSgVarRefExp(node)) {
    if (var_ref->get_symbol() == nullptr || identity.root != nullptr) {
      return false;
    }
    identity.root = var_ref->get_symbol();
    return true;
  }

  if (SgPntrArrRefExp *array_ref = isSgPntrArrRefExp(node)) {
    // OpenMP treats an array item and sections/elements derived from that item
    // as the same underlying list-item identity for directive-wide uniqueness.
    return buildOmpListItemIdentity(array_ref->get_lhs_operand(), identity);
  }

  auto append_member = [&](SgExpression *lhs, SgExpression *rhs) {
    if (!buildOmpListItemIdentity(lhs, identity)) {
      return false;
    }
    SgVariableSymbol *member = extractClauseVariableSymbol(rhs);
    if (member == nullptr) {
      return false;
    }
    identity.member_path.push_back(member);
    return true;
  };

  if (SgDotExp *dot = isSgDotExp(node)) {
    return append_member(dot->get_lhs_operand(), dot->get_rhs_operand());
  }
  if (SgArrowExp *arrow = isSgArrowExp(node)) {
    return append_member(arrow->get_lhs_operand(), arrow->get_rhs_operand());
  }
  if (SgCastExp *cast_exp = isSgCastExp(node)) {
    return buildOmpListItemIdentity(cast_exp->get_operand(), identity);
  }
  if (SgUnaryOp *unary_op = isSgUnaryOp(node)) {
    return buildOmpListItemIdentity(unary_op->get_operand(), identity);
  }

  return false;
}

bool ompListItemIdentitiesConflict(const OmpListItemIdentity &lhs,
                                   const OmpListItemIdentity &rhs) {
  ROSE_ASSERT(lhs.root != nullptr && rhs.root != nullptr);
  if (lhs.root != rhs.root) {
    return false;
  }

  const size_t common_length =
      std::min(lhs.member_path.size(), rhs.member_path.size());
  for (size_t index = 0; index < common_length; ++index) {
    if (lhs.member_path[index] != rhs.member_path[index]) {
      return false;
    }
  }

  // Equal paths are the same list item. A proper-prefix path denotes an
  // aggregate item together with one of its parts, which OpenMP also forbids
  // on the same directive.
  return true;
}

} // namespace

// Liao 4/23/2011, special function to copy file info of the original SgPragma
// or Fortran comments
void copyStartFileInfo(SgNode *src, SgNode *dest) {
  ROSE_ASSERT(src && dest);
  // same src and dest, no copy is needed
  if (src == dest)
    return;

  SgLocatedNode *lsrc = isSgLocatedNode(src);
  ROSE_ASSERT(lsrc);
  SgLocatedNode *ldest = isSgLocatedNode(dest);
  ROSE_ASSERT(ldest);
  if (!hasUsableSourceLocation(lsrc->get_startOfConstruct())) {
    std::cerr << "REX_OMP_AST_INVARIANT[source-start]: directive source has "
                 "no exact start location\n";
    ROSE_ABORT();
  }
  // ROSE_ASSERT (lsrc->get_file_info()->isTransformation() == false);
  // already the same, no copy is needed
  if (lsrc->get_startOfConstruct()->get_filenameString() ==
          ldest->get_startOfConstruct()->get_filenameString() &&
      lsrc->get_startOfConstruct()->get_line() ==
          ldest->get_startOfConstruct()->get_line() &&
      lsrc->get_startOfConstruct()->get_col() ==
          ldest->get_startOfConstruct()->get_col())
    return;

  Sg_File_Info *copy = new Sg_File_Info(*(lsrc->get_startOfConstruct()));
  ROSE_ASSERT(copy != NULL);

  // delete old start of construct
  Sg_File_Info *old_info = ldest->get_startOfConstruct();
  Sg_File_Info *old_file_info = ldest->get_file_info();
  if (old_info)
    delete (old_info);
  if (old_file_info != nullptr && old_file_info != old_info) {
    delete old_file_info;
  }

  ldest->set_startOfConstruct(copy);
  ldest->set_file_info(copy);
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
}

// Liao 3/11/2013, special function to copy end file info of the original
// SgPragma or Fortran comments (src) to OpenMP node (dest) If the OpenMP node
// is a body statement, we have to use the body's end file info as the node's
// end file info.
void copyEndFileInfo(SgNode *src, SgNode *dest) {
  ROSE_ASSERT(src && dest);

  if (SgOmpBodyStatement *body_stmt = isSgOmpBodyStatement(dest)) {
    if (body_stmt->get_body() != NULL) {
      src = body_stmt->get_body();
    }
  }

  // same src and dest, no copy is needed
  if (src == dest)
    return;

  SgLocatedNode *lsrc = isSgLocatedNode(src);
  ROSE_ASSERT(lsrc);
  SgLocatedNode *ldest = isSgLocatedNode(dest);
  ROSE_ASSERT(ldest);
  if (!hasUsableSourceLocation(lsrc->get_endOfConstruct())) {
    std::cerr << "REX_OMP_AST_INVARIANT[source-end]: " << lsrc->class_name()
              << " used as the end boundary for " << ldest->class_name()
              << " has no exact end location";
    if (lsrc->get_startOfConstruct() != nullptr) {
      std::cerr << " (start "
                << lsrc->get_startOfConstruct()->get_filenameString() << ":"
                << lsrc->get_startOfConstruct()->get_line() << ":"
                << lsrc->get_startOfConstruct()->get_col() << ")";
    }
    std::cerr << "\n";
    ROSE_ABORT();
  }

  // ROSE_ASSERT (lsrc->get_file_info()->isTransformation() == false);
  // already the same, no copy is needed
  if (lsrc->get_endOfConstruct()->get_filenameString() ==
          ldest->get_endOfConstruct()->get_filenameString() &&
      lsrc->get_endOfConstruct()->get_line() ==
          ldest->get_endOfConstruct()->get_line() &&
      lsrc->get_endOfConstruct()->get_col() ==
          ldest->get_endOfConstruct()->get_col())
    return;

  Sg_File_Info *copy = new Sg_File_Info(*(lsrc->get_endOfConstruct()));
  ROSE_ASSERT(copy != NULL);

  // delete old start of construct
  Sg_File_Info *old_info = ldest->get_endOfConstruct();
  if (old_info)
    delete (old_info);

  ldest->set_endOfConstruct(copy);
  copy->set_parent(ldest);

  if (ldest->get_file_info() != nullptr &&
      ldest->get_endOfConstruct()->isTransformation() !=
          ldest->get_file_info()->isTransformation()) {
    if (ldest->get_endOfConstruct()->isTransformation()) {
      ldest->get_file_info()->setTransformation();
    } else {
      ldest->get_file_info()->unsetTransformation();
    }
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
}

static void
validateConvertedOpenMPStatementLocation(SgPragmaDeclaration *pragma,
                                         SgStatement *statement) {
  if (pragma == nullptr || statement == nullptr) {
    std::cerr << "REX_OMP_AST_INVARIANT[converted-location]: null pragma or "
                 "statement\n";
    ROSE_ABORT();
  }

  SgLocatedNode *located_pragma = isSgLocatedNode(pragma);
  SgLocatedNode *located_statement = isSgLocatedNode(statement);
  if (located_pragma == nullptr || located_statement == nullptr) {
    std::cerr << "REX_OMP_AST_INVARIANT[converted-location]: directive or "
                 "converted statement is not located\n";
    ROSE_ABORT();
  }

  const Sg_File_Info *pragma_start = located_pragma->get_startOfConstruct();
  Sg_File_Info *file_info = located_statement->get_file_info();
  Sg_File_Info *start = located_statement->get_startOfConstruct();
  Sg_File_Info *end = located_statement->get_endOfConstruct();
  if (!hasUsableSourceLocation(pragma_start) ||
      !hasUsableSourceLocation(file_info) || !hasUsableSourceLocation(start) ||
      !hasUsableSourceLocation(end)) {
    std::cerr << "REX_OMP_AST_INVARIANT[converted-location]: converted "
                 "directive has no complete exact source range\n";
    ROSE_ABORT();
  }
  if (start->get_filenameString() != pragma_start->get_filenameString() ||
      start->get_line() != pragma_start->get_line() ||
      start->get_col() != pragma_start->get_col() ||
      file_info->get_filenameString() != start->get_filenameString() ||
      file_info->get_line() != start->get_line() ||
      file_info->get_col() != start->get_col()) {
    std::cerr << "REX_OMP_AST_INVARIANT[converted-location]: converted "
                 "directive does not start at its exact pragma: node="
              << statement->class_name()
              << " pragma=" << pragma_start->get_filenameString() << ':'
              << pragma_start->get_line() << ':' << pragma_start->get_col()
              << " start=" << start->get_filenameString() << ':'
              << start->get_line() << ':' << start->get_col()
              << " file-info=" << file_info->get_filenameString() << ':'
              << file_info->get_line() << ':' << file_info->get_col() << '\n';
    ROSE_ABORT();
  }
  if (start->get_parent() != statement || end->get_parent() != statement ||
      file_info->get_parent() != statement) {
    std::cerr << "REX_OMP_AST_INVARIANT[converted-location]: source file "
                 "information has the wrong structural owner\n";
    ROSE_ABORT();
  }
}

static void
initializeGeneratedOpenMPVariantDirective(SgPragmaDeclaration *pragma,
                                          SgStatement *statement) {
  if (pragma == nullptr || statement == nullptr) {
    std::cerr << "REX_OMP_AST_INVARIANT[variant-directive]: null producer "
                 "pragma or generated nested directive\n";
    ROSE_ABORT();
  }

  setOneSourcePositionForTransformation(statement);

  if (SgOmpBodyStatement *omp_body = isSgOmpBodyStatement(statement)) {
    if (SgStatement *nested_directive = omp_body->get_body()) {
      initializeGeneratedOpenMPVariantDirective(pragma, nested_directive);
      nested_directive->set_parent(statement);
    }
  }

  copyStartFileInfo(pragma, statement);
  copyEndFileInfo(pragma, statement);
  initializeGeneratedOpenMPStatement(statement);
  validateConvertedOpenMPStatementLocation(pragma, statement);
  if (SgLocatedNode *located = isSgLocatedNode(statement)) {
    located->setOutputInCodeGeneration();
  }
}

namespace OmpSupport {
static void releaseOpenMPParseStateForSourceFile(SgSourceFile *source_file) {
  if (source_file == nullptr) {
    std::cerr << "REX_OMP_AST_INVARIANT[parse-state]: cannot release OpenMP "
                 "state for a null source file\n";
    ROSE_ABORT();
  }

  // A conversion session is owned by exactly one processOpenMP invocation.
  // Release its complete observer graph without dereferencing pragma pointers:
  // AST-JSON checkpoints may replace the source tree before this boundary.
  std::unordered_set<OpenMPDirective *> observed_directives;
  std::unordered_set<SgPragmaDeclaration *> observed_pragmas;
  std::size_t expected_cxx_bindings = 0;
  std::size_t expected_fortran_bindings = 0;
  for (const auto &entry : openMPDirectives()) {
    if (entry.first == nullptr || entry.second == nullptr ||
        !observed_directives.insert(entry.second).second ||
        !observed_pragmas.insert(entry.first).second) {
      std::cerr << "REX_OMP_AST_INVARIANT[parse-state]: malformed or "
                   "duplicate persistent OpenMP IR observer\n";
      ROSE_ABORT();
    }
    const OpenMPBaseLang base_language = entry.second->getBaseLang();
    if (base_language == Lang_Fortran) {
      if (openMPFortranExactSemanticBindings().find(entry.first) ==
              openMPFortranExactSemanticBindings().end() ||
          openACCCxxExactSemanticBindings().find(entry.first) !=
              openACCCxxExactSemanticBindings().end()) {
        std::cerr << "REX_OMP_AST_INVARIANT[semantic-handoff-release]: "
                     "Fortran directive does not own exactly one Fortran "
                     "binding record\n";
        ROSE_ABORT();
      }
      ++expected_fortran_bindings;
    } else if (base_language == Lang_C || base_language == Lang_Cplusplus) {
      if (openACCCxxExactSemanticBindings().find(entry.first) !=
              openACCCxxExactSemanticBindings().end() ||
          openMPFortranExactSemanticBindings().find(entry.first) !=
              openMPFortranExactSemanticBindings().end()) {
        std::cerr << "REX_OMP_AST_INVARIANT[semantic-handoff-release]: "
                     "C/C++ OpenMP directive owns a forbidden frontend "
                     "semantic record\n";
        ROSE_ABORT();
      }
    } else {
      std::cerr << "REX_OMP_AST_INVARIANT[semantic-handoff-release]: "
                   "directive has no exact source language\n";
      ROSE_ABORT();
    }
  }
  for (const auto &entry : openMPFortranPairedPragmas()) {
    if (entry.first == nullptr || entry.second == nullptr) {
      std::cerr << "REX_OMP_AST_INVARIANT[parse-state]: malformed Fortran "
                   "pragma/directive observer\n";
      ROSE_ABORT();
    }
    observed_directives.insert(entry.second);
    if (entry.second->getKind() == OMPD_ompx) {
      if (openMPFortranExactSemanticBindings().find(entry.first) !=
              openMPFortranExactSemanticBindings().end() ||
          openACCCxxExactSemanticBindings().find(entry.first) !=
              openACCCxxExactSemanticBindings().end()) {
        std::cerr << "REX_OMP_AST_INVARIANT[semantic-handoff-release]: OMPX "
                     "directive owns a semantic-conversion record\n";
        ROSE_ABORT();
      }
      continue;
    }
    if (!observed_pragmas.insert(entry.first).second) {
      continue;
    }
    if (openMPFortranExactSemanticBindings().find(entry.first) ==
            openMPFortranExactSemanticBindings().end() ||
        openACCCxxExactSemanticBindings().find(entry.first) !=
            openACCCxxExactSemanticBindings().end()) {
      std::cerr << "REX_OMP_AST_INVARIANT[semantic-handoff-release]: "
                   "Fortran end directive does not own exactly one Fortran "
                   "binding record\n";
      ROSE_ABORT();
    }
    ++expected_fortran_bindings;
  }
  for (const auto &entry : openMPFortranExplicitEndPragmas()) {
    if (entry.first == nullptr || entry.second == nullptr) {
      std::cerr << "REX_OMP_AST_INVARIANT[parse-state]: malformed Fortran "
                   "explicit-end observer\n";
      ROSE_ABORT();
    }
  }
  for (const auto &entry : openMPCxxExplicitEndPragmas()) {
    if (entry.first == nullptr || entry.second == nullptr) {
      std::cerr << "REX_OMP_AST_INVARIANT[parse-state]: malformed C/C++ "
                   "explicit-end observer\n";
      ROSE_ABORT();
    }
    if (!observed_pragmas.insert(entry.second).second ||
        openACCCxxExactSemanticBindings().find(entry.second) !=
            openACCCxxExactSemanticBindings().end() ||
        openMPFortranExactSemanticBindings().find(entry.second) !=
            openMPFortranExactSemanticBindings().end()) {
      std::cerr << "REX_OMP_AST_INVARIANT[semantic-handoff-release]: C/C++ "
                   "explicit end is duplicated or owns a forbidden semantic "
                   "record\n";
      ROSE_ABORT();
    }
  }
  for (SgPragmaDeclaration *pragma : openMPPragmas()) {
    if (pragma == nullptr) {
      std::cerr << "REX_OMP_AST_INVARIANT[parse-state]: null persistent "
                   "directive pragma observer\n";
      ROSE_ABORT();
    }
    if (observed_pragmas.find(pragma) != observed_pragmas.end()) {
      continue;
    }
    auto cxx = openACCCxxExactSemanticBindings().find(pragma);
    auto fortran = openMPFortranExactSemanticBindings().find(pragma);
    if (cxx != openACCCxxExactSemanticBindings().end()) {
      if (fortran != openMPFortranExactSemanticBindings().end()) {
        std::cerr << "REX_OMP_AST_INVARIANT[semantic-handoff-release]: "
                     "parse-only OpenACC pragma owns both binding families\n";
        ROSE_ABORT();
      }
      ++expected_cxx_bindings;
      observed_pragmas.insert(pragma);
    } else if (fortran != openMPFortranExactSemanticBindings().end()) {
      ++expected_fortran_bindings;
      observed_pragmas.insert(pragma);
    }
  }
  if (expected_cxx_bindings != openACCCxxExactSemanticBindings().size() ||
      expected_fortran_bindings !=
          openMPFortranExactSemanticBindings().size()) {
    std::cerr << "REX_OMP_AST_INVARIANT[semantic-handoff-release]: "
                 "conversion session contains stale producer records\n";
    ROSE_ABORT();
  }
  for (const auto &entry : openMPClauseNodes()) {
    // Nested variant directives are owned transitively by a retained root, so
    // not every cache key is itself a root key in directive_owners.
    if (entry.first == nullptr) {
      std::cerr << "REX_OMP_AST_INVARIANT[parse-state]: clause cache has a "
                   "null directive identity\n";
      ROSE_ABORT();
    }
  }
  if (!openMPExpressionVariables().empty()) {
    std::cerr << "REX_OMP_AST_INVARIANT[parse-state]: expression parser "
                 "scratch escaped directive conversion\n";
    ROSE_ABORT();
  }

  std::vector<OpenMPDirective *> owned_directives;
  owned_directives.reserve(openMPDirectiveOwners().size());
  for (const auto &entry : openMPDirectiveOwners()) {
    if (entry.first == nullptr || entry.second == nullptr ||
        entry.first != entry.second.get() ||
        observed_directives.find(entry.first) == observed_directives.end()) {
      std::cerr << "REX_OMP_AST_INVARIANT[directive-ownership]: persistent "
                   "directive owner has no exact observer identity\n";
      ROSE_ABORT();
    }
    owned_directives.push_back(entry.first);
  }
  if (owned_directives.size() != observed_directives.size()) {
    std::cerr << "REX_OMP_AST_INVARIANT[directive-ownership]: persistent "
                 "directive observer has no unique owner\n";
    ROSE_ABORT();
  }

  openMPClauseNodes().clear();
  openMPConversionState().merged_end_clause_sources.clear();
  openMPFunctionDirectiveTargets().clear();
  openACCCxxExactSemanticBindings().clear();
  openMPFortranExactSemanticBindings().clear();
  openMPDirectives().clear();
  openMPFortranPairedPragmas().clear();
  openMPFortranExplicitEndPragmas().clear();
  openMPCxxExplicitEndPragmas().clear();
  openMPPragmas().clear();
  for (OpenMPDirective *directive : owned_directives) {
    releaseOpenMPDirective(directive);
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
  return std::isspace(static_cast<unsigned char>(next)) || next == '(' ||
         next == '&';
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
  case OMPD_do_simd:
  case OMPD_parallel_do:
  case OMPD_parallel_do_simd:
  case OMPD_parallel_loop:
  case OMPD_loop:
  case OMPD_target:
  case OMPD_target_parallel_do:
  case OMPD_target_parallel_do_simd:
  case OMPD_target_parallel_for:
  case OMPD_target_parallel_for_simd:
  case OMPD_target_parallel_loop:
  case OMPD_target_simd:
  case OMPD_target_teams_distribute:
  case OMPD_target_teams_distribute_parallel_do:
  case OMPD_target_teams_distribute_parallel_do_simd:
  case OMPD_target_teams_distribute_parallel_for:
  case OMPD_target_teams_distribute_parallel_for_simd:
  case OMPD_target_teams_distribute_simd:
  case OMPD_teams_distribute:
  case OMPD_teams_distribute_parallel_do:
  case OMPD_teams_distribute_parallel_do_simd:
  case OMPD_teams_distribute_parallel_for:
  case OMPD_teams_distribute_parallel_for_simd:
  case OMPD_teams_distribute_simd:
  case OMPD_distribute_parallel_do:
  case OMPD_distribute_parallel_do_simd:
  case OMPD_distribute_parallel_for:
  case OMPD_distribute_parallel_for_simd:
  case OMPD_distribute_simd:
  case OMPD_taskloop:
  case OMPD_taskloop_simd:
  case OMPD_declare_target:
    return true;
  default:
    return false;
  }
}

static bool allowsImplicitOpenMPEnd(OpenMPDirective *directive) {
  if (directive == NULL) {
    std::cerr << "REX_OMP_AST_INVARIANT[paired-end]: null begin directive\n";
    ROSE_ABORT();
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
  // OMPX is an implementation-defined source directive family.  Flang owns
  // its exact pragma surface, while ompparser supplies only the typed family
  // classification needed to keep it out of semantic Sage conversion.
  return directive->getKind() == OMPD_ompx ||
         isOpenMPDirectiveEndMarkerOnly(directive);
}

static bool
isFortranPotentiallyExplicitEndDirective(OpenMPDirective *directive) {
  if (directive == nullptr) {
    return false;
  }

  switch (directive->getKind()) {
  case OMPD_atomic:
    return true;
  default:
    return false;
  }
}

static bool usesFortranDoDirectiveSpelling(OpenMPDirectiveKind kind) {
  switch (kind) {
  case OMPD_parallel_do_simd:
  case OMPD_distribute_parallel_do:
  case OMPD_distribute_parallel_do_simd:
  case OMPD_teams_distribute_parallel_do:
  case OMPD_teams_distribute_parallel_do_simd:
  case OMPD_target_parallel_do:
  case OMPD_target_parallel_do_simd:
  case OMPD_target_teams_distribute_parallel_do:
  case OMPD_target_teams_distribute_parallel_do_simd:
    return true;
  default:
    return false;
  }
}

static void markFortranDoDirectiveSpelling(SgStatement *stmt) {
  if (stmt == nullptr) {
    std::cerr << "REX_OMP_AST_INVARIANT[fortran-spelling]: cannot mark a null "
                 "DO-family statement\n";
    ROSE_ABORT();
  }
  switch (stmt->get_omp_fortran_spelling()) {
  case SgStatement::e_omp_fortran_spelling_not_applicable:
    stmt->set_omp_fortran_spelling(SgStatement::e_omp_fortran_spelling_do);
    return;
  case SgStatement::e_omp_fortran_spelling_do:
    return;
  default:
    std::cerr << "REX_OMP_AST_INVARIANT[fortran-spelling]: statement="
              << stmt->class_name() << " has invalid typed spelling="
              << static_cast<int>(stmt->get_omp_fortran_spelling()) << "\n";
    ROSE_ABORT();
  }
}

static void setExactDirectiveEndKind(SgStatement *stmt,
                                     SgStatement::directive_end_kind_enum kind,
                                     const char *context) {
  if (stmt == nullptr || context == nullptr ||
      kind == SgStatement::e_directive_end_not_applicable) {
    std::cerr << "REX_OMP_AST_INVARIANT[directive-end-kind]: invalid "
              << (context != nullptr ? context : "<null-context>") << "\n";
    ROSE_ABORT();
  }
  const SgStatement::directive_end_kind_enum current =
      stmt->get_directive_end_kind();
  if (current != SgStatement::e_directive_end_not_applicable &&
      current != kind) {
    std::cerr << "REX_OMP_AST_INVARIANT[directive-end-kind]: statement="
              << stmt->class_name()
              << " has conflicting current=" << static_cast<int>(current)
              << " requested=" << static_cast<int>(kind)
              << " context=" << context << "\n";
    ROSE_ABORT();
  }
  stmt->set_directive_end_kind(kind);
}

static std::unique_ptr<OpenMPDirective>
parseOpenMPDirectiveText(const std::string &text) {
  std::string parse_buffer = text;
  trimLeft(parse_buffer);
  if (!startsWithCaseInsensitive(parse_buffer, "!$")) {
    parse_buffer = std::string("!$") + parse_buffer;
  }

  return parseOpenMPDirectiveOrAbort(parse_buffer,
                                     makeOpenMPParseOptions(Lang_Fortran));
}

static void failOpenMPFortranAstConstruction(SgSourceFile *sageFilePtr,
                                             SgPragmaDeclaration *pragmaDecl,
                                             const std::string &directiveText,
                                             const std::string &reason) {
  std::cerr << "Error: failed to build OpenMP AST for Fortran directive";
  if (pragmaDecl != nullptr) {
    if (Sg_File_Info *info = pragmaDecl->get_startOfConstruct()) {
      std::cerr << " at " << info->get_filenameString() << ":"
                << info->get_line();
    }
  }
  std::cerr << ": " << reason << ": " << directiveText << std::endl;

  const int openmp_parse_error = 100;
  if (sageFilePtr != nullptr) {
    sageFilePtr->set_frontendErrorCode(
        std::max(sageFilePtr->get_frontendErrorCode(), openmp_parse_error));
    if (SgProject *project = sageFilePtr->get_project()) {
      project->set_frontendErrorCode(
          std::max(project->get_frontendErrorCode(), openmp_parse_error));
    }
  }

  ROSE_ABORT();
}

static bool parseOpenMPFortranPragmas(SgSourceFile *sageFilePtr) {
  ROSE_ASSERT(sageFilePtr != NULL);
  for (SgNode *node :
       NodeQuery::querySubTree(sageFilePtr, V_SgPragmaDeclaration)) {
    SgPragmaDeclaration *pragma = isSgPragmaDeclaration(node);
    ROSE_ASSERT(pragma != nullptr);
    if (getEnclosingSourceFile(pragma) != sageFilePtr ||
        pragma->get_pragma() == nullptr) {
      continue;
    }
    std::string text = pragma->get_pragma()->get_pragma();
    if (extractFortranOpenMPDirectivePayload(text) &&
        pragma->get_fortran_directive_family() ==
            SgPragmaDeclaration::e_fortran_directive_none) {
      std::cerr << "REX_OMP_AST_INVARIANT[fortran-provenance]: OpenMP "
                   "pragma has no Flang directive group ownership\n";
      ROSE_ABORT();
    }
  }

  std::vector<FortranDirectiveGroupView> directive_groups =
      collectFortranDirectiveGroups(sageFilePtr, "omp");
  std::vector<FortranDirectiveGroupView *> omp_groups;
  for (FortranDirectiveGroupView &group : directive_groups) {
    if (group.family == "omp" || group.family == "ompx") {
      omp_groups.push_back(&group);
    }
  }
  if (omp_groups.empty()) {
    return false;
  }

  for (const FortranDirectiveGroupView *group : omp_groups) {
    ROSE_ASSERT(group != nullptr && group->primary != nullptr);
    const Sg_File_Info *info = getPreferredLocatedNodeStartInfo(group->primary);
    if (info == NULL || info->get_line() <= 0 || info->isTransformation()) {
      std::cerr << "REX_OMP_AST_INVARIANT[fortran-pragma-source]: frontend "
                   "OpenMP pragma has no exact source location\n";
      ROSE_ABORT();
    }
  }

  std::vector<OpenMPDirective *> pairing_list;
  std::vector<std::unique_ptr<OpenMPDirective>> local_directive_owners;
  std::vector<std::pair<SgPragmaDeclaration *, OpenMPDirective *>>
      local_OpenMPIR_list;
  std::unordered_map<OpenMPDirective *, std::string>
      local_pragma_semantic_text_by_ir;
  std::unordered_map<OpenMPDirective *, std::string>
      local_pragma_source_text_by_ir;
  std::unordered_map<OpenMPDirective *, OmpDirectiveParseCacheTree>
      local_parse_cache_trees;
  std::vector<SgPragmaDeclaration *> local_omp_pragma_list;
  std::map<SgPragmaDeclaration *, OpenMPDirective *>
      local_fortran_paired_pragma_dict;
  std::map<SgPragmaDeclaration *, SgPragmaDeclaration *>
      local_fortran_explicit_end_pragma_dict;
  std::vector<SgPragmaDeclaration *> pragmas_to_remove;

  for (const FortranDirectiveGroupView *group : omp_groups) {
    ROSE_ASSERT(group != nullptr && group->primary != nullptr);
    SgPragmaDeclaration *primary = group->primary;
    const std::string &pending_semantic = group->cooked_text;
    const std::string &directive_source_text = group->logical_text;
    std::unique_ptr<OpenMPDirective> directive_owner =
        parseOpenMPDirectiveText(pending_semantic);
    OpenMPDirective *directive = directive_owner.get();

    if (directive->getKind() != OMPD_ompx) {
      OmpDirectiveParseCacheTree cache_tree = parseClauseNodesForDirective(
          primary, directive, pending_semantic, &directive_source_text);
      if (!local_parse_cache_trees.emplace(directive, std::move(cache_tree))
               .second) {
        std::cerr << "REX_OMP_AST_INVARIANT[clause-cache]: Fortran source "
                     "directive was cached more than once\n";
        ROSE_ABORT();
      }
    }

    if (directive->getKind() != OMPD_end &&
        (isFortranPairedDirective(directive) ||
         isFortranPotentiallyExplicitEndDirective(directive))) {
      pairing_list.push_back(directive);
    }
    if (directive->getKind() == OMPD_end) {
      auto *end_wrapper = static_cast<OpenMPEndDirective *>(directive);
      OpenMPDirective *end_directive = end_wrapper->getPairedDirective();
      if (pairing_list.empty()) {
        std::cerr << "REX_OMP_AST_INVARIANT[paired-end]: parsed Fortran "
                     "directive sequence before unmatched end:";
        for (const auto &entry : local_pragma_semantic_text_by_ir) {
          std::cerr << " [" << entry.second << "]";
        }
        std::cerr << "\n";
        failOpenMPFortranAstConstruction(sageFilePtr, primary,
                                         directive_source_text,
                                         "unmatched OpenMP end directive");
      }
      bool matched = false;
      OpenMPDirective *matched_begin_directive = NULL;
      size_t matched_index = 0;
      for (size_t i = pairing_list.size(); i > 0; --i) {
        OpenMPDirective *begin_directive = pairing_list[i - 1];
        if (end_directive->getKind() == begin_directive->getKind()) {
          matched = true;
          matched_begin_directive = begin_directive;
          matched_index = i - 1;
          break;
        }
      }
      if (!matched) {
        failOpenMPFortranAstConstruction(sageFilePtr, primary,
                                         directive_source_text,
                                         "mismatched OpenMP end directive");
      } else {
        bool recorded_pair = false;
        for (const auto &entry : local_fortran_paired_pragma_dict) {
          if (entry.second == matched_begin_directive) {
            if (!local_fortran_explicit_end_pragma_dict
                     .emplace(entry.first, primary)
                     .second) {
              std::cerr << "REX_OMP_AST_INVARIANT[paired-end]: Fortran begin "
                           "directive has multiple explicit ends\n";
              ROSE_ABORT();
            }
            recorded_pair = true;
            break;
          }
        }
        if (!recorded_pair) {
          std::cerr << "REX_OMP_AST_INVARIANT[paired-end]: matched Fortran "
                       "begin directive has no pragma owner\n";
          ROSE_ABORT();
        }
        if (isFortranPotentiallyExplicitEndDirective(matched_begin_directive)) {
          matched_begin_directive->setRequiresExplicitEnd(true);
        }
        auto begin_cache =
            local_parse_cache_trees.find(matched_begin_directive);
        auto end_cache = local_parse_cache_trees.find(directive);
        if (begin_cache == local_parse_cache_trees.end() ||
            end_cache == local_parse_cache_trees.end()) {
          std::cerr << "REX_OMP_AST_INVARIANT[end-clause-cache-transfer]: "
                       "Fortran begin or END cache is missing\n";
          ROSE_ABORT();
        }
        mergeEndClausesToBeginDirective(matched_begin_directive, end_directive,
                                        directive, directive_source_text);
        transferMergedEndClauseCaches(matched_begin_directive, end_directive,
                                      directive, directive_source_text,
                                      std::move(end_cache->second),
                                      begin_cache->second.root);
        local_parse_cache_trees.erase(end_cache);
        pairing_list.erase(pairing_list.begin() + matched_index);
      }
    }

    if (!local_fortran_paired_pragma_dict.emplace(primary, directive).second ||
        !local_pragma_semantic_text_by_ir.emplace(directive, pending_semantic)
             .second ||
        !local_pragma_source_text_by_ir
             .emplace(directive, directive_source_text)
             .second) {
      std::cerr << "REX_OMP_AST_INVARIANT[parse-state]: duplicate Fortran "
                   "pragma or directive identity\n";
      ROSE_ABORT();
    }
    const bool is_end_directive = directive->getKind() == OMPD_end;
    if (!is_end_directive &&
        !shouldSkipOpenMPDirectiveAstConversion(directive)) {
      local_OpenMPIR_list.push_back(std::make_pair(primary, directive));
      local_omp_pragma_list.push_back(primary);
    }
    local_directive_owners.push_back(std::move(directive_owner));

    for (size_t member_index = 1; member_index < group->members.size();
         ++member_index) {
      pragmas_to_remove.push_back(group->members[member_index]);
    }
  }

  for (OpenMPDirective *unmatched_begin : pairing_list) {
    ROSE_ASSERT(unmatched_begin != nullptr);
    if (!allowsImplicitOpenMPEnd(unmatched_begin)) {
      std::cerr << "REX_OMP_AST_INVARIANT[paired-end]: required Fortran "
                   "OpenMP end directive is missing for kind "
                << static_cast<int>(unmatched_begin->getKind()) << "\n";
      ROSE_ABORT();
    }
  }

  for (const auto &entry : local_OpenMPIR_list) {
    openMPDirectives().push_back(entry);
    auto cache_tree = local_parse_cache_trees.find(entry.second);
    if (cache_tree == local_parse_cache_trees.end()) {
      std::cerr << "REX_OMP_AST_INVARIANT[clause-cache]: Fortran source "
                   "directive has no parser-stage cache\n";
      ROSE_ABORT();
    }
    publishOpenMPDirectiveParseCacheTree(entry.second,
                                         std::move(cache_tree->second),
                                         "Fortran source directive");
    local_parse_cache_trees.erase(cache_tree);
  }
  if (!local_parse_cache_trees.empty()) {
    std::cerr << "REX_OMP_AST_INVARIANT[clause-cache]: unpublished Fortran "
                 "parser-stage directive caches remain\n";
    ROSE_ABORT();
  }
  for (std::unique_ptr<OpenMPDirective> &directive_owner :
       local_directive_owners) {
    retainOpenMPDirective(std::move(directive_owner), "Fortran source file");
  }
  for (SgPragmaDeclaration *decl : local_omp_pragma_list) {
    openMPPragmas().push_back(decl);
  }
  for (const auto &entry : local_fortran_paired_pragma_dict) {
    if (!openMPFortranPairedPragmas().emplace(entry).second) {
      std::cerr << "REX_OMP_AST_INVARIANT[parse-state]: Fortran pragma "
                   "already has a persistent directive\n";
      ROSE_ABORT();
    }
  }
  for (const auto &entry : local_fortran_explicit_end_pragma_dict) {
    if (!openMPFortranExplicitEndPragmas().emplace(entry).second) {
      std::cerr << "REX_OMP_AST_INVARIANT[paired-end]: Fortran begin pragma "
                   "already has an explicit end\n";
      ROSE_ABORT();
    }
  }
  for (SgPragmaDeclaration *decl : pragmas_to_remove) {
    removeStatement(decl);
  }
  return true;
}

static bool extractFortranOpenACCDirectivePayload(std::string &text) {
  std::string candidate = text;
  trimLeft(candidate);
  stripFortranDirectiveSentinel(candidate);
  trimLeft(candidate);
  if (!startsWithAccKeyword(candidate)) {
    return false;
  }
  text = candidate;
  return true;
}

static bool parseOpenACCFortranPragmas(SgSourceFile *sageFilePtr,
                                       OpenACCParseSession &session) {
  ROSE_ASSERT(sageFilePtr != nullptr);
  const openacc::ParseOptions options = getOpenACCParseOptions(sageFilePtr);
  if (options.language != openacc::Language::Fortran ||
      (options.inputForm != openacc::InputForm::FortranFree &&
       options.inputForm != openacc::InputForm::FortranFixed)) {
    std::cerr << "REX_ACC_AST_INVARIANT[parse-options]: Fortran parser entry "
                 "received a non-Fortran language/form identity\n";
    ROSE_ABORT();
  }
  for (SgNode *node :
       NodeQuery::querySubTree(sageFilePtr, V_SgPragmaDeclaration)) {
    SgPragmaDeclaration *pragma = isSgPragmaDeclaration(node);
    ROSE_ASSERT(pragma != nullptr);
    if (getEnclosingSourceFile(pragma) != sageFilePtr ||
        pragma->get_pragma() == nullptr) {
      continue;
    }
    std::string text = pragma->get_pragma()->get_pragma();
    if (extractFortranOpenACCDirectivePayload(text) &&
        pragma->get_fortran_directive_family() ==
            SgPragmaDeclaration::e_fortran_directive_none) {
      std::cerr << "REX_ACC_AST_INVARIANT[fortran-provenance]: OpenACC "
                   "pragma has no Flang directive group ownership\n";
      ROSE_ABORT();
    }
  }

  std::vector<FortranDirectiveGroupView> acc_groups =
      collectFortranDirectiveGroups(sageFilePtr, "acc");
  if (acc_groups.empty()) {
    return false;
  }

  std::vector<SgPragmaDeclaration *> continuation_pragmas;
  for (const FortranDirectiveGroupView &group : acc_groups) {
    SgPragmaDeclaration *primary = group.primary;
    ROSE_ASSERT(primary != nullptr);
    const Sg_File_Info *info = getPreferredLocatedNodeStartInfo(primary);
    if (info == nullptr || info->get_line() <= 0 || info->isTransformation()) {
      std::cerr << "REX_ACC_AST_INVARIANT[fortran-pragma-source]: frontend "
                   "OpenACC pragma has no exact source location\n";
      ROSE_ABORT();
    }

    std::string semanticText = group.cooked_text;
    trim(semanticText);
    if (!startsWithAccKeyword(semanticText)) {
      std::cerr << "REX_ACC_AST_INVARIANT[fortran-provenance]: frontend "
                   "semantic directive text has no OpenACC family prefix\n";
      ROSE_ABORT();
    }
    // The frontend owns macro expansion and continuation reconstruction.  The
    // typed parser still receives an explicit envelope matching the exact
    // SgFile fixed/free input-form identity, and is invoked exactly once.
    const std::string parseText = "!$" + semanticText;
    parseOpenACCDirectiveOrAbort(session, primary, parseText, options);
    const openacc::Directive *directive = session.find(primary);
    if (directive == nullptr) {
      std::cerr << "REX_ACC_AST_INVARIANT[parse-session]: parsed Fortran "
                   "OpenACC directive was not retained by its session\n";
      ROSE_ABORT();
    }
    if (directive->kind() != openacc::DirectiveKind::End) {
      openMPPragmas().push_back(primary);
    }
    for (size_t index = 1; index < group.members.size(); ++index) {
      continuation_pragmas.push_back(group.members[index]);
    }
  }
  for (SgPragmaDeclaration *pragma : continuation_pragmas) {
    removeStatement(pragma, false);
  }
  return true;
}

static bool fortranAstUnparserEmitsOpenMPEnd(OpenMPDirectiveKind kind);

static bool isFortranOpenMPPragmaDeclaration(SgPragmaDeclaration *decl) {
  if (decl == NULL || decl->get_pragma() == NULL) {
    return false;
  }

  std::string pragma_text = decl->get_pragma()->get_pragma();
  stripFortranDirectiveSentinel(pragma_text);
  trimLeft(pragma_text);
  return startsWithOpenMPDirectiveKeyword(pragma_text);
}

static void removeFortranOpenMPPragmas(SgSourceFile *sageFilePtr) {
  ROSE_ASSERT(sageFilePtr != NULL);

  std::vector<SgNode *> all_pragmas =
      NodeQuery::querySubTree(sageFilePtr, V_SgPragmaDeclaration);
  for (SgNode *node : all_pragmas) {
    SgPragmaDeclaration *decl = isSgPragmaDeclaration(node);
    ROSE_ASSERT(decl != NULL);
    Sg_File_Info *info = decl->get_file_info();
    if (info == NULL) {
      std::cerr << "REX_OMP_AST_INVARIANT[pragma-cleanup]: Fortran OpenMP "
                   "pragma has no source metadata\n";
      ROSE_ABORT();
    }
    if (info->get_filename() != sageFilePtr->get_file_info()->get_filename() &&
        !info->isTransformation()) {
      continue;
    }
    if (!isFortranOpenMPPragmaDeclaration(decl)) {
      continue;
    }
    auto pair_it = openMPFortranPairedPragmas().find(decl);
    if (pair_it == openMPFortranPairedPragmas().end() ||
        pair_it->second == nullptr) {
      std::cerr << "REX_OMP_AST_INVARIANT[pragma-cleanup]: frontend OpenMP "
                   "pragma was not parsed into a directive\n";
      ROSE_ABORT();
    }
    if (pair_it->second->getKind() == OMPD_ompx) {
      // The exact source-owned pragma is the AST representation of an opaque
      // implementation extension and must remain available to the unparser.
      continue;
    }
    if (pair_it->second->getKind() != OMPD_end) {
      std::cerr << "REX_OMP_AST_INVARIANT[pragma-cleanup]: converted Fortran "
                   "OpenMP begin pragma remains attached to the AST\n";
      ROSE_ABORT();
    }
    OpenMPEndDirective *end_directive =
        static_cast<OpenMPEndDirective *>(pair_it->second);
    OpenMPDirective *end_target = end_directive->getPairedDirective();
    SgPragmaDeclaration *begin_pragma = nullptr;
    for (const auto &explicit_end : openMPFortranExplicitEndPragmas()) {
      if (explicit_end.second != decl) {
        continue;
      }
      if (begin_pragma != nullptr) {
        std::cerr << "REX_OMP_AST_INVARIANT[pragma-cleanup]: Fortran OpenMP "
                     "end pragma has multiple begin owners\n";
        ROSE_ABORT();
      }
      begin_pragma = explicit_end.first;
    }
    auto begin_it = openMPFortranPairedPragmas().find(begin_pragma);
    OpenMPDirective *paired_begin =
        begin_it != openMPFortranPairedPragmas().end() ? begin_it->second
                                                       : nullptr;
    if (end_target == nullptr || begin_pragma == nullptr ||
        paired_begin == nullptr || paired_begin->getKind() == OMPD_end ||
        paired_begin->getKind() != end_target->getKind()) {
      std::cerr << "REX_OMP_AST_INVARIANT[pragma-cleanup]: Fortran OpenMP end "
                   "pragma has no exact, kind-compatible parsed begin owner\n";
      ROSE_ABORT();
    }
    if (!fortranAstUnparserEmitsOpenMPEnd(paired_begin->getKind())) {
      std::cerr << "REX_OMP_AST_INVARIANT[pragma-cleanup]: Sage OpenMP node "
                   "cannot emit the required Fortran end directive\n";
      ROSE_ABORT();
    }
    removeStatement(decl, false);
  }
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

//! Convert OpenMPIR directive-name modifiers to the Sage OpenMP clause enum.
static SgOmpClause::omp_directive_name_modifier_enum
toSgOmpClauseDirectiveNameModifier(OpenMPDirectiveKind modifier) {
  switch (modifier) {
  case OMPD_parallel:
    return SgOmpClause::e_omp_directive_name_modifier_parallel;
  case OMPD_for:
    return SgOmpClause::e_omp_directive_name_modifier_for;
  case OMPD_do:
    return SgOmpClause::e_omp_directive_name_modifier_do;
  case OMPD_distribute:
    return SgOmpClause::e_omp_directive_name_modifier_distribute;
  case OMPD_sections:
    return SgOmpClause::e_omp_directive_name_modifier_sections;
  case OMPD_single:
    return SgOmpClause::e_omp_directive_name_modifier_single;
  case OMPD_scope:
    return SgOmpClause::e_omp_directive_name_modifier_scope;
  case OMPD_target:
    return SgOmpClause::e_omp_directive_name_modifier_target;
  case OMPD_task:
    return SgOmpClause::e_omp_directive_name_modifier_task;
  case OMPD_taskloop:
    return SgOmpClause::e_omp_directive_name_modifier_taskloop;
  case OMPD_teams:
    return SgOmpClause::e_omp_directive_name_modifier_teams;
  case OMPD_unknown:
    return SgOmpClause::e_omp_directive_name_modifier_unspecified;
  default:
    return SgOmpClause::e_omp_directive_name_modifier_unknown;
  }
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
    std::cerr << "REX_OMP_AST_INVARIANT[map-operation]: invalid parser map "
                 "operation="
              << static_cast<int>(at_op) << "\n";
    ROSE_ABORT();
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
    std::cerr << "REX_OMP_AST_INVARIANT[map-modifier]: invalid parser map "
                 "modifier="
              << static_cast<int>(modifier) << "\n";
    ROSE_ABORT();
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
    std::cerr << "REX_OMP_AST_INVARIANT[declare-mapper]: cannot normalize a "
                 "null mapper directive\n";
    ROSE_ABORT();
  }

  mapper_data.identifier =
      toSgOmpClauseDeclareMapperIdentifier(mapper_directive->getIdentifier());
  mapper_data.identifier_is_explicit =
      mapper_directive->hasExplicitIdentifier();
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
  case OMPC_REDUCTION_MODIFIER_original_private: {
    result = SgOmpClause::e_omp_reduction_original_private;
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
toSgOmpClauseReductionIdentifier(OpenMPReductionClauseIdentifier identifier,
                                 OpenMPBaseLang base_lang) {
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
    result = base_lang == Lang_Fortran ? SgOmpClause::e_omp_reduction_and
                                       : SgOmpClause::e_omp_reduction_logand;
    break;
  }
  case OMPC_REDUCTION_IDENTIFIER_logor: // ||
  {
    result = base_lang == Lang_Fortran ? SgOmpClause::e_omp_reduction_or
                                       : SgOmpClause::e_omp_reduction_logor;
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
toSgOmpClauseInReductionIdentifier(OpenMPInReductionClauseIdentifier identifier,
                                   OpenMPBaseLang base_lang) {
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
    result = base_lang == Lang_Fortran
                 ? SgOmpClause::e_omp_in_reduction_identifier_and
                 : SgOmpClause::e_omp_in_reduction_identifier_logand;
    break;
  }
  case OMPC_IN_REDUCTION_IDENTIFIER_logor: // ||
  {
    result = base_lang == Lang_Fortran
                 ? SgOmpClause::e_omp_in_reduction_identifier_or
                 : SgOmpClause::e_omp_in_reduction_identifier_logor;
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
    OpenMPTaskReductionClauseIdentifier identifier, OpenMPBaseLang base_lang) {
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
    result = base_lang == Lang_Fortran
                 ? SgOmpClause::e_omp_task_reduction_identifier_and
                 : SgOmpClause::e_omp_task_reduction_identifier_logand;
    break;
  }
  case OMPC_TASK_REDUCTION_IDENTIFIER_logor: // ||
  {
    result = base_lang == Lang_Fortran
                 ? SgOmpClause::e_omp_task_reduction_identifier_or
                 : SgOmpClause::e_omp_task_reduction_identifier_logor;
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
  case OMPC_USESALLOCATORS_ALLOCATOR_unspecified: {
    result = SgOmpClause::e_omp_uses_allocators_allocator_traits;
    break;
  }
  case OMPC_USESALLOCATORS_ALLOCATOR_unknown: {
    std::cerr << "REX_OMP_AST_INVARIANT[uses-allocators]: parser produced "
                 "an unknown allocator form\n";
    ROSE_ABORT();
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

//! Convert the session's parsed directive pragmas to SgOmpxxx nodes.
void OpenMPIRToSageAST(SgSourceFile *sageFilePtr,
                       const OpenACCParseSession &openaccSession) {
  list<SgPragmaDeclaration *>::reverse_iterator
      iter; // bottom up handling for nested cases
  ROSE_ASSERT(sageFilePtr != NULL);
  const bool isFortran =
      sageFilePtr->get_Fortran_only() || sageFilePtr->get_F77_only() ||
      sageFilePtr->get_F90_only() || sageFilePtr->get_F95_only() ||
      sageFilePtr->get_F2003_only();
  std::map<SgPragmaDeclaration *, OpenMPDirective *> omp_lookup;
  for (const auto &entry : openMPDirectives()) {
    omp_lookup[entry.first] = entry.second;
  }
  for (iter = openMPPragmas().rbegin(); iter != openMPPragmas().rend();
       iter++) {
    // Liao, 11/18/2009
    // It is possible that several source files showing up in a single
    // compilation line We have to check if the pragma declaration's file
    // information matches the current file being processed Otherwise we will
    // process the same pragma declaration multiple times!!
    SgPragmaDeclaration *decl = *iter;
    if (isFortran) {
      SgScopeStatement *parent_scope = isSgScopeStatement(decl->get_parent());
      if (parent_scope == nullptr || decl->get_scope() != parent_scope) {
        std::cerr << "REX_OMP_AST_INVARIANT[pragma-owner]: Fortran directive "
                     "is not directly owned by its declared scope\n";
        ROSE_ABORT();
      }
    }
    if (getEnclosingSourceFile(decl) != sageFilePtr) {
      continue;
    }
    if (decl->get_file_info() == nullptr ||
        sageFilePtr->get_file_info() == nullptr) {
      std::cerr << "REX_OMP_AST_INVARIANT[pragma-file]: directive or its "
                   "enclosing source file has no exact source information\n";
      ROSE_ABORT();
    }
    auto omp_it = omp_lookup.find(decl);
    if (omp_it != omp_lookup.end()) {
      convertDirective(std::make_pair(decl, omp_it->second));
      continue;
    }
    if (const openacc::Directive *directive = openaccSession.find(decl)) {
      convertOpenACCDirective(decl, *directive);
      continue;
    }

    std::cerr << "REX_OMP_AST_INVARIANT[pragma-map]: directive pragma has no "
                 "parsed OpenMP or OpenACC IR owner\n";
    ROSE_ABORT();
  }
}

//! A helper function to ensure a sequence statements either has only one
//! statement
//  or all are put under a single basic block.
//  begin_decl is the begin directive which is immediately in front of the list
//  of statements Return the single statement or the basic block. This function
//  is used to wrap all statement between begin and end Fortran directives into
//  a block, if necessary(more than one statement)
static SgStatement *getNextStatementInSameBasicBlock(SgStatement *stmt);
static const Sg_File_Info *getStatementStartLocation(const SgStatement *stmt);

static SgStatement *
ensureSingleStmtOrBasicBlock(SgPragmaDeclaration *begin_decl,
                             SgPragmaDeclaration *end_decl,
                             const std::vector<SgStatement *> &stmt_vec) {
  ROSE_ASSERT(begin_decl != NULL);
  ROSE_ASSERT(end_decl != NULL);
  SgStatement *result = NULL;
  if (stmt_vec.empty()) {
    return NULL;
  }
  if (stmt_vec.size() == 1) {
    result = stmt_vec[0];
    ROSE_ASSERT(getNextStatementInSameBasicBlock(begin_decl) == result);
  } else {
    result = buildBasicBlock();
    initializeGeneratedOpenMPStatement(result);
    if (stmt_vec.front() == nullptr || stmt_vec.back() == nullptr) {
      std::cerr << "REX_OMP_AST_INVARIANT[paired-body]: null boundary in "
                   "multi-statement directive body\n";
      ROSE_ABORT();
    }
    // The wrapper is a generated representation of the paired construct, not
    // a copied source statement.  Its exact source span is therefore the
    // written begin/end directive pair.  Body endpoints can be semantic
    // statements produced by macro expansion and legitimately have no direct
    // physical source position.
    copyStartFileInfo(begin_decl, result);
    copyEndFileInfo(end_decl, result);
    SgScopeStatement *new_scope = isSgScopeStatement(result);
    ROSE_ASSERT(new_scope != NULL);
    // Have to remove them from their original scope first.
    // Otherwise they will show up twice in the unparsed code: original place
    // and under the new block I tried to merge this into appendStatement() but
    // it broke other transformations I don't want debug
    for (SgStatement *statement : stmt_vec) {
      // This operation moves the exact source statements into the generated
      // structured body.  Source tokens attached to those statements belong
      // to the moved lexical boundary (including opaque !$ompx directives);
      // relocating them to a surrounding statement would sever their order
      // from the body they precede.
      removeStatement(statement, false);
    }
    insertStatementAfter(begin_decl, result, false);
    appendStatementList(stmt_vec, new_scope);
  }
  return result;
}

static SgStatement *getNextStatementInSameBasicBlock(SgStatement *stmt) {
  if (stmt == NULL) {
    std::cerr << "REX_OMP_AST_INVARIANT[fortran-sibling]: null statement\n";
    ROSE_ABORT();
  }

  SgBasicBlock *scope = isSgBasicBlock(stmt->get_parent());
  if (scope == NULL || stmt->get_scope() != scope) {
    std::cerr << "REX_OMP_AST_INVARIANT[fortran-sibling]: directive or body "
                 "statement is not owned by its basic block\n";
    ROSE_ABORT();
  }

  const SgStatementPtrList &statements = scope->get_statements();
  const auto found = std::find(statements.begin(), statements.end(), stmt);
  if (found == statements.end()) {
    std::cerr << "REX_OMP_AST_INVARIANT[fortran-sibling]: statement is not "
                 "present in its owning basic block\n";
    ROSE_ABORT();
  }
  const auto next = std::next(found);
  return next != statements.end() ? *next : NULL;
}

static void merge_Matching_Cxx_Pragma_pairs(SgPragmaDeclaration *decl) {
  if (decl == NULL) {
    std::cerr << "REX_OMP_AST_INVARIANT[cxx-pair]: null begin pragma\n";
    ROSE_ABORT();
  }

  auto end_it = openMPCxxExplicitEndPragmas().find(decl);
  if (end_it == openMPCxxExplicitEndPragmas().end()) {
    std::cerr << "REX_OMP_AST_INVARIANT[cxx-pair]: begin pragma has no "
                 "explicit-end mapping\n";
    ROSE_ABORT();
  }

  SgPragmaDeclaration *end_decl = end_it->second;
  if (end_decl == NULL) {
    std::cerr << "REX_OMP_AST_INVARIANT[cxx-pair]: explicit-end mapping has "
                 "a null end pragma\n";
    ROSE_ABORT();
  }

  std::vector<SgStatement *> affected_stmts;
  SgStatement *next_stmt = getNextStatement(decl);
  while (next_stmt != NULL && next_stmt != end_decl) {
    affected_stmts.push_back(next_stmt);
    next_stmt = getNextStatement(next_stmt);
  }

  ROSE_ASSERT(next_stmt == end_decl);

  SgStatement *merged_body =
      ensureSingleStmtOrBasicBlock(decl, end_decl, affected_stmts);
  if (merged_body == NULL) {
    std::cerr << "REX_OMP_AST_INVARIANT[cxx-pair]: matched begin/end pragmas "
                 "have no structured body\n";
    ROSE_ABORT();
  }

  if (!affected_stmts.empty() && isSgBasicBlock(merged_body) != NULL) {
    copyStartFileInfo(affected_stmts.front(), merged_body);
    copyEndFileInfo(end_decl, merged_body);
  }
}

static void convert_Cxx_Pragma_Pairs(SgSourceFile *sageFilePtr) {
  ROSE_ASSERT(sageFilePtr != NULL);

  for (list<SgPragmaDeclaration *>::reverse_iterator iter =
           openMPPragmas().rbegin();
       iter != openMPPragmas().rend(); ++iter) {
    SgPragmaDeclaration *decl = *iter;
    if (decl == NULL || decl->get_file_info() == NULL) {
      std::cerr << "REX_OMP_AST_INVARIANT[cxx-pair]: parsed pragma has no "
                   "declaration or source metadata\n";
      ROSE_ABORT();
    }
    if (decl->get_file_info()->get_filename() !=
            sageFilePtr->get_file_info()->get_filename() &&
        !(decl->get_file_info()->isTransformation())) {
      continue;
    }
    if (openMPCxxExplicitEndPragmas().find(decl) ==
        openMPCxxExplicitEndPragmas().end()) {
      continue;
    }
    merge_Matching_Cxx_Pragma_pairs(decl);
  }
}

static bool fortranAstUnparserEmitsOpenMPEnd(OpenMPDirectiveKind kind) {
  switch (kind) {
  case OMPD_parallel:
  case OMPD_critical:
  case OMPD_sections:
  case OMPD_master:
  case OMPD_masked:
  case OMPD_ordered:
  case OMPD_workshare:
  case OMPD_single:
  case OMPD_task:
  case OMPD_taskgroup:
  case OMPD_atomic:
  case OMPD_do:
  case OMPD_do_simd:
  case OMPD_parallel_do:
  case OMPD_parallel_do_simd:
  case OMPD_parallel_for:
  case OMPD_parallel_for_simd:
  case OMPD_parallel_sections:
  case OMPD_parallel_workshare:
  case OMPD_parallel_loop:
  case OMPD_loop:
  case OMPD_taskloop:
  case OMPD_taskloop_simd:
  case OMPD_target:
  case OMPD_target_data:
  case OMPD_target_data_composite:
  case OMPD_scope:
  case OMPD_parallel_masked:
  case OMPD_assume:
  case OMPD_taskgraph:
  case OMPD_fuse:
  case OMPD_interchange:
  case OMPD_reverse:
  case OMPD_target_parallel:
  case OMPD_target_parallel_do:
  case OMPD_target_parallel_do_simd:
  case OMPD_target_parallel_for:
  case OMPD_target_parallel_for_simd:
  case OMPD_target_parallel_loop:
  case OMPD_target_simd:
  case OMPD_target_teams:
  case OMPD_target_teams_distribute:
  case OMPD_target_teams_distribute_parallel_do:
  case OMPD_target_teams_distribute_parallel_do_simd:
  case OMPD_target_teams_distribute_parallel_for:
  case OMPD_target_teams_distribute_parallel_for_simd:
  case OMPD_target_teams_distribute_simd:
  case OMPD_target_teams_workdistribute:
  case OMPD_teams:
  case OMPD_teams_distribute:
  case OMPD_teams_distribute_parallel_do:
  case OMPD_teams_distribute_parallel_do_simd:
  case OMPD_teams_distribute_parallel_for:
  case OMPD_teams_distribute_parallel_for_simd:
  case OMPD_teams_distribute_simd:
  case OMPD_distribute_simd:
  case OMPD_distribute_parallel_do:
  case OMPD_distribute_parallel_do_simd:
  case OMPD_distribute_parallel_for:
  case OMPD_distribute_parallel_for_simd:
  case OMPD_parallel_master:
  case OMPD_master_taskloop:
  case OMPD_master_taskloop_simd:
  case OMPD_masked_taskloop:
  case OMPD_masked_taskloop_simd:
  case OMPD_parallel_master_taskloop:
  case OMPD_parallel_master_taskloop_simd:
  case OMPD_workdistribute:
  case OMPD_begin_metadirective:
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
  auto begin_it = openMPFortranPairedPragmas().find(decl);
  ROSE_ASSERT(begin_it != openMPFortranPairedPragmas().end());
  OpenMPDirective *begin_directive = begin_it->second;
  ROSE_ASSERT(begin_directive != NULL);

  std::vector<SgStatement *>
      affected_stmts; // statements which are inside the begin .. end pair

  auto explicit_end_it = openMPFortranExplicitEndPragmas().find(decl);
  if (explicit_end_it != openMPFortranExplicitEndPragmas().end()) {
    end_decl = explicit_end_it->second;
    if (end_decl == NULL) {
      std::cerr << "REX_OMP_AST_INVARIANT[paired-end]: explicit Fortran end "
                   "mapping has a null pragma\n";
      ROSE_ABORT();
    }
  }

  SgStatement *next_stmt = getNextStatementInSameBasicBlock(decl);
  while (next_stmt != NULL) {
    if (end_decl != NULL) {
      if (next_stmt == end_decl) {
        break;
      }
    } else if (SgPragmaDeclaration *candidate_end =
                   isSgPragmaDeclaration(next_stmt)) {
      auto end_it = openMPFortranPairedPragmas().find(candidate_end);
      if (end_it != openMPFortranPairedPragmas().end()) {
        OpenMPDirective *end_ir = end_it->second;
        if (end_ir != NULL && end_ir->getKind() == OMPD_end) {
          OpenMPEndDirective *end_directive =
              static_cast<OpenMPEndDirective *>(end_ir);
          if (end_directive != NULL &&
              end_directive->getPairedDirective() == begin_directive) {
            end_decl = candidate_end;
            break;
          }
        }
      }
    }
    affected_stmts.push_back(next_stmt);
    next_stmt = getNextStatementInSameBasicBlock(next_stmt);
  }

  if (end_decl != NULL && next_stmt != end_decl) {
    std::cerr << "REX_OMP_AST_INVARIANT[paired-order]: matched Fortran begin "
                 "and end directives are not ordered siblings in one basic "
                 "block; begin='"
              << decl->get_pragma()->get_pragma() << "' end='"
              << end_decl->get_pragma()->get_pragma() << "' begin-scope="
              << (decl->get_scope() != nullptr ? decl->get_scope()->class_name()
                                               : std::string("<null>"))
              << " end-scope="
              << (end_decl->get_scope() != nullptr
                      ? end_decl->get_scope()->class_name()
                      : std::string("<null>"))
              << " same-scope="
              << (decl->get_scope() == end_decl->get_scope() ? "true" : "false")
              << " begin-parent="
              << (decl->get_parent() != nullptr
                      ? decl->get_parent()->class_name()
                      : std::string("<null>"))
              << " end-parent="
              << (end_decl->get_parent() != nullptr
                      ? end_decl->get_parent()->class_name()
                      : std::string("<null>"))
              << "\n";
    ROSE_ABORT();
  }

  // End directives are optional for selected Fortran OpenMP constructs.
  if (end_decl == NULL) {
    if (!allowsImplicitOpenMPEnd(begin_directive)) {
      cerr << "merge_Matching_Fortran_Pragma_pairs(): cannot find required end "
              "directive for: "
           << endl;
      cerr << decl->get_pragma()->get_pragma() << endl;
      ROSE_ABORT();
    } else {
      setExactDirectiveEndKind(decl, SgStatement::e_directive_end_implicit,
                               "Fortran OpenMP begin without source END");
      return; // There is nothing further to do if the optional end directives
              // do not exist
    }
  } // end if sanity check

  // at this point, we have found a matching end directive/pragma
  ROSE_ASSERT(end_decl);
  setExactDirectiveEndKind(decl, SgStatement::e_directive_end_explicit,
                           "Fortran OpenMP matched source END");
  SgStatement *merged_body =
      ensureSingleStmtOrBasicBlock(decl, end_decl, affected_stmts);
  if (merged_body == NULL) {
    std::cerr << "REX_OMP_AST_INVARIANT[paired-body]: matched Fortran OpenMP "
                 "directives have no structured block; begin scope="
              << (decl->get_scope() != nullptr ? decl->get_scope()->class_name()
                                               : std::string("<null>"))
              << " begin parent="
              << (decl->get_parent() != nullptr
                      ? decl->get_parent()->class_name()
                      : std::string("<null>"))
              << " end scope="
              << (end_decl->get_scope() != nullptr
                      ? end_decl->get_scope()->class_name()
                      : std::string("<null>"))
              << " end parent="
              << (end_decl->get_parent() != nullptr
                      ? end_decl->get_parent()->class_name()
                      : std::string("<null>"))
              << " same-scope="
              << (decl->get_scope() == end_decl->get_scope() ? "true" : "false")
              << " begin-line="
              << (getStatementStartLocation(decl) != nullptr
                      ? getStatementStartLocation(decl)->get_line()
                      : 0)
              << " end-line="
              << (getStatementStartLocation(end_decl) != nullptr
                      ? getStatementStartLocation(end_decl)->get_line()
                      : 0)
              << "\n";
    if (SgBasicBlock *diagnostic_scope = isSgBasicBlock(decl->get_scope())) {
      for (SgStatement *candidate : diagnostic_scope->get_statements()) {
        const Sg_File_Info *candidate_info =
            candidate != nullptr ? getStatementStartLocation(candidate)
                                 : nullptr;
        std::cerr << "  paired-body candidate line="
                  << (candidate_info != nullptr ? candidate_info->get_line()
                                                : 0)
                  << " class="
                  << (candidate != nullptr ? candidate->class_name()
                                           : std::string("<null>"))
                  << "\n";
      }
    }
    ROSE_ABORT();
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
  for (iter = openMPPragmas().rbegin(); iter != openMPPragmas().rend();
       iter++) {
    // It is possible that several source files showing up in a single
    // compilation line We have to check if the pragma declaration's file
    // information matches the current file being processed Otherwise we will
    // process the same pragma declaration multiple times!!
    SgPragmaDeclaration *decl = *iter;
    if (getEnclosingSourceFile(decl) != sageFilePtr) {
      continue;
    }
    if (decl->get_file_info() == nullptr ||
        sageFilePtr->get_file_info() == nullptr) {
      std::cerr << "REX_OMP_AST_INVARIANT[pragma-file]: Fortran OpenMP "
                   "directive or enclosing source file has no exact source "
                   "information\n";
      ROSE_ABORT();
    }
    const auto directive_it = openMPFortranPairedPragmas().find(decl);
    if (directive_it == openMPFortranPairedPragmas().end() ||
        directive_it->second == nullptr) {
      std::cerr << "REX_OMP_AST_INVARIANT[pragma-map]: Fortran OpenMP pragma "
                   "has no parsed directive\n";
      ROSE_ABORT();
    }
    OpenMPDirective *directive = directive_it->second;
    if (isFortranPairedDirective(directive) ||
        (directive != NULL && directive->getRequiresExplicitEnd())) {
      merge_Matching_Fortran_Pragma_pairs(decl);
    }
  }

} // end convert_Fortran_Pragma_Pairs()

static bool allowsImplicitFortranAccEnd(openacc::DirectiveKind kind) {
  switch (kind) {
  case openacc::DirectiveKind::ParallelLoop:
    return true;
  default:
    return false;
  }
}

static bool hasOpenACCFlagClause(const openacc::GeneralDirective &directive,
                                 openacc::FlagClauseKind kind) {
  return std::any_of(directive.defaultClauses.begin(),
                     directive.defaultClauses.end(),
                     [kind](const openacc::Clause &clause) {
                       const openacc::FlagClause *flag =
                           std::get_if<openacc::FlagClause>(&clause);
                       return flag != nullptr && flag->kind == kind;
                     });
}

static bool isFortranAccPairedDirective(const openacc::Directive &directive) {
  const openacc::GeneralDirective *general =
      std::get_if<openacc::GeneralDirective>(&directive.payload());
  if (general == nullptr) {
    return false;
  }
  switch (general->kind) {
  case openacc::DirectiveKind::Parallel:
  case openacc::DirectiveKind::ParallelLoop:
  case openacc::DirectiveKind::Data:
  case openacc::DirectiveKind::Kernels:
    return true;
  case openacc::DirectiveKind::Atomic:
    return hasOpenACCFlagClause(*general, openacc::FlagClauseKind::Capture);
  default:
    return false;
  }
}

static bool matchesFortranAccEnd(const openacc::Directive &endDirective,
                                 openacc::DirectiveKind beginKind) {
  const openacc::EndDirective *end =
      std::get_if<openacc::EndDirective>(&endDirective.payload());
  if (end == nullptr) {
    return false;
  }
  switch (beginKind) {
  case openacc::DirectiveKind::Atomic:
    return end->kind == openacc::EndDirectiveKind::Atomic;
  case openacc::DirectiveKind::Data:
    return end->kind == openacc::EndDirectiveKind::Data;
  case openacc::DirectiveKind::Kernels:
    return end->kind == openacc::EndDirectiveKind::Kernels;
  case openacc::DirectiveKind::Parallel:
    return end->kind == openacc::EndDirectiveKind::Parallel;
  case openacc::DirectiveKind::ParallelLoop:
    return end->kind == openacc::EndDirectiveKind::ParallelLoop;
  default:
    std::cerr << "REX_ACC_AST_INVARIANT[paired-begin]: unsupported OpenACC "
                 "begin kind reached structured-region matching\n";
    ROSE_ABORT();
  }
}

static const Sg_File_Info *getStatementStartLocation(const SgStatement *stmt) {
  ROSE_ASSERT(stmt != NULL);
  if (const SgLocatedNode *located = isSgLocatedNode(stmt)) {
    if (const Sg_File_Info *info = getPreferredLocatedNodeStartInfo(located)) {
      return info;
    }
  }
  return stmt->get_file_info();
}

void merge_Matching_Fortran_ACC_Pragma_pairs(SgPragmaDeclaration *decl,
                                             OpenACCParseSession &session) {
  if (decl == nullptr) {
    std::cerr << "REX_ACC_AST_INVARIANT[paired-begin]: null begin pragma\n";
    ROSE_ABORT();
  }
  const openacc::Directive *beginDirective = session.find(decl);
  if (beginDirective == nullptr ||
      !isFortranAccPairedDirective(*beginDirective)) {
    std::cerr << "REX_ACC_AST_INVARIANT[paired-begin]: begin pragma has no "
                 "paired typed OpenACC directive\n";
    ROSE_ABORT();
  }
  SgPragmaDeclaration *end_decl = nullptr;
  SgStatement *next_stmt = getNextStatementInSameBasicBlock(decl);
  const openacc::DirectiveKind beginKind = beginDirective->kind();

  std::vector<SgStatement *> affected_stmts;

  while (next_stmt != nullptr) {
    end_decl = isSgPragmaDeclaration(next_stmt);
    if (end_decl != nullptr) {
      const openacc::Directive *endDirective = session.find(end_decl);
      if (endDirective != nullptr &&
          matchesFortranAccEnd(*endDirective, beginKind)) {
        break;
      }
    }
    end_decl = nullptr;
    affected_stmts.push_back(next_stmt);
    next_stmt = getNextStatementInSameBasicBlock(next_stmt);
  }

  if (end_decl == nullptr) {
    if (!allowsImplicitFortranAccEnd(beginKind)) {
      std::cerr << "REX_ACC_AST_INVARIANT[paired-end]: cannot find required "
                   "Fortran OpenACC end directive\n";
      ROSE_ABORT();
    }
    setExactDirectiveEndKind(decl, SgStatement::e_directive_end_implicit,
                             "Fortran OpenACC begin without source END");
    return;
  }

  if (ensureSingleStmtOrBasicBlock(decl, end_decl, affected_stmts) == nullptr) {
    std::cerr << "REX_ACC_AST_INVARIANT[paired-body]: matched OpenACC begin "
                 "and end directives have no structured body\n";
    ROSE_ABORT();
  }

  setExactDirectiveEndKind(decl, SgStatement::e_directive_end_explicit,
                           "Fortran OpenACC matched source END");
  session.markEndConsumed(end_decl);
  removeStatement(end_decl, false);
}

void convert_Fortran_ACC_Pragma_Pairs(SgSourceFile *sageFilePtr,
                                      OpenACCParseSession &session) {
  ROSE_ASSERT(sageFilePtr != NULL);
  list<SgPragmaDeclaration *>::reverse_iterator iter;
  for (iter = openMPPragmas().rbegin(); iter != openMPPragmas().rend();
       iter++) {
    SgPragmaDeclaration *decl = *iter;
    if (getEnclosingSourceFile(decl) != sageFilePtr) {
      continue;
    }
    if (decl->get_file_info() == nullptr ||
        sageFilePtr->get_file_info() == nullptr) {
      std::cerr << "REX_ACC_AST_INVARIANT[pragma-file]: Fortran OpenACC "
                   "directive or enclosing source file has no exact source "
                   "information\n";
      ROSE_ABORT();
    }
    const openacc::Directive *directive = session.find(decl);
    if (directive == nullptr) {
      continue;
    }
    if (isFortranAccPairedDirective(*directive)) {
      merge_Matching_Fortran_ACC_Pragma_pairs(decl, session);
    }
  }
  session.requireAllEndsConsumed();
}

// Liao, 5/31/2009 an entry point for OpenMP related processing
// including parsing, AST construction, and later on translation
void processOpenMP(SgSourceFile *sageFilePtr) {
  // DQ (4/4/2010): This function processes both C/C++ and Fortran code.
  // As a result of the Fortran processing some OMP pragmas will cause
  // transformation (e.g. declaration of private variables will add variables
  // to the local scope).  So this function has side-effects for all languages.

  if (sageFilePtr == NULL) {
    std::cerr << "REX_OMP_AST_INVARIANT[process]: null source file\n";
    ROSE_ABORT();
  }
  if (sageFilePtr->get_openmp_processed()) {
    return;
  }
  std::unique_ptr<OpenMPConversionSession> conversion_session =
      std::make_unique<OpenMPConversionSession>(sageFilePtr);

  auto mark_processed = [&](SgSourceFile *file) {
    if (file == nullptr) {
      std::cerr << "REX_OMP_AST_INVARIANT[process]: cannot mark a null source "
                   "file as processed\n";
      ROSE_ABORT();
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
  OpenACCParseSession openaccSession(sageFilePtr);

  bool parsed_fortran_pragmas = false;
  bool parsed_fortran_acc_pragmas = false;

  // ==================================================================================================================//
  // ====== Stage 1: parse OpenMP directives using ompparser and store the
  // ompparser's OpenMPIR nodes in a map   ======
  // ==================================================================================================================//
  // find all SgPragmaDeclaration nodes within a file, parse OpenMP directives
  // using ompparser, and retain the typed IR in this conversion session.
  // ompparser only parse OpenMP directive/clauses not the expressions that are
  // used by the directives/clauses For Fortran, search comments for OpenMP
  // directives
  if (isFortran) { // use ompparser to process Fortran.
    if (wantsOpenMP) {
      parsed_fortran_pragmas = parseOpenMPFortranPragmas(sageFilePtr);
    }
    if (wantsOpenACC) {
      parsed_fortran_acc_pragmas =
          parseOpenACCFortranPragmas(sageFilePtr, openaccSession);
      if (!parsed_fortran_acc_pragmas && SgProject::get_verbose() > 1) {
        std::cout << "No parser-produced Fortran OpenACC directives found.\n";
      }
    }
  } else { // For C/C++, search pragma declarations for OpenMP directives
    OpenMPBaseLang openmp_base_language = Lang_unknown;
    if (wantsOpenMP || wantsOpenACC) {
      if (sageFilePtr->get_Cxx_only() || sageFilePtr->get_Cuda_only()) {
        openmp_base_language = Lang_Cplusplus;
      } else if (sageFilePtr->get_C_only() || sageFilePtr->get_OpenCL_only()) {
        openmp_base_language = Lang_C;
      } else {
        std::cerr << "REX_DIRECTIVE_AST_INVARIANT[base-language]: C/C++ "
                     "OpenMP/OpenACC source has no exact base-language "
                     "identity\n";
        ROSE_ABORT();
      }
    }
    const openacc::ParseOptions openaccOptions =
        getOpenACCParseOptions(sageFilePtr);
    std::vector<SgNode *> all_pragmas =
        NodeQuery::querySubTree(sageFilePtr, V_SgPragmaDeclaration);
    std::vector<std::pair<SgPragmaDeclaration *, OpenMPDirective *>>
        explicit_end_stack;
    std::vector<SgNode *>::iterator iter;
    for (iter = all_pragmas.begin(); iter != all_pragmas.end(); iter++) {
      SgPragmaDeclaration *pragmaDeclaration = isSgPragmaDeclaration(*iter);
      ROSE_ASSERT(pragmaDeclaration != NULL);
      const std::string preprocessedPragmaString =
          pragmaDeclaration->get_pragma()->get_pragma();
      if (pragmaDeclaration->get_cxx_pragma_payload_kind() ==
          SgPragmaDeclaration::e_cxx_pragma_source_file_only) {
        if (isSgGlobal(pragmaDeclaration->get_scope()) == nullptr ||
            pragmaDeclaration->get_cxx_source_text().empty()) {
          std::cerr << "REX_OMP_AST_INVARIANT[source-file-only-pragma]: "
                       "typed preprocessing-only pragma has no exact global "
                       "source owner or payload\n";
          ROSE_ABORT();
        }
        continue;
      }
      string pragmaString = preprocessedPragmaString;
      istringstream istr(pragmaString);
      std::string key;
      istr >> key;
      if (key == "omp" && wantsOpenMP) {
        const std::string sourcePragmaString =
            getCxxOpenMPDirectiveSourceText(pragmaDeclaration);
        // parse expression
        // Get the object that ompparser IR.
        std::unique_ptr<OpenMPDirective> directive_owner =
            parseOpenMPDirectiveOrAbort(
                pragmaString, makeOpenMPParseOptions(openmp_base_language));
        OpenMPDirective *directive = directive_owner.get();
        if (isOpenMPDirectiveEndMarkerOnly(directive)) {
          OpenMPEndDirective *end_wrapper =
              static_cast<OpenMPEndDirective *>(directive);
          OpenMPDirective *end_directive =
              end_wrapper != NULL ? end_wrapper->getPairedDirective() : NULL;
          if (end_directive == nullptr || explicit_end_stack.empty()) {
            std::cerr << "REX_OMP_AST_INVARIANT[paired-end]: unmatched C/C++ "
                         "OpenMP end directive\n";
            ROSE_ABORT();
          }
          std::pair<SgPragmaDeclaration *, OpenMPDirective *> begin_entry =
              explicit_end_stack.back();
          explicit_end_stack.pop_back();
          if (begin_entry.first == nullptr || begin_entry.second == nullptr ||
              !begin_entry.second->getRequiresExplicitEnd() ||
              begin_entry.second->getKind() != end_directive->getKind()) {
            std::cerr << "REX_OMP_AST_INVARIANT[paired-end]: mismatched C/C++ "
                         "OpenMP begin/end directives\n";
            ROSE_ABORT();
          }
          const std::string end_parse_text =
              std::string("#pragma ") + pragmaString;
          const std::string end_source_text =
              std::string("#pragma ") + sourcePragmaString;
          OmpDirectiveParseCacheTree end_cache = parseClauseNodesForDirective(
              pragmaDeclaration, directive, end_parse_text, &end_source_text);
          auto begin_cache = openMPClauseNodes().find(begin_entry.second);
          if (begin_cache == openMPClauseNodes().end()) {
            MLOG_ERROR_C("ompAstConstruction",
                         "Explicit-end OpenMP begin directive has no "
                         "semantic clause cache\n");
            ROSE_ABORT();
          }
          mergeEndClausesToBeginDirective(begin_entry.second, end_directive,
                                          directive, end_source_text);
          transferMergedEndClauseCaches(
              begin_entry.second, end_directive, directive, end_source_text,
              std::move(end_cache), begin_cache->second);
          if (!openMPCxxExplicitEndPragmas()
                   .emplace(begin_entry.first, pragmaDeclaration)
                   .second) {
            std::cerr << "REX_OMP_AST_INVARIANT[paired-end]: C/C++ begin "
                         "pragma already has an explicit end\n";
            ROSE_ABORT();
          }
          continue;
        }
        if (shouldSkipOpenMPDirectiveAstConversion(directive)) {
          continue;
        }

        if (!checkOpenMPIR(directive)) {
          std::cerr << "REX_OMP_AST_INVARIANT[typed-ir]: parsed C/C++ "
                       "directive failed typed IR validation\n";
          ROSE_ABORT();
        }
        std::string parse_text = std::string("#pragma ") + pragmaString;
        const std::string source_parse_text =
            std::string("#pragma ") + sourcePragmaString;
        OmpDirectiveParseCacheTree clause_cache = parseClauseNodesForDirective(
            pragmaDeclaration, directive, parse_text, &source_parse_text);
        OpenMPDirective *persistent_directive = retainOpenMPDirective(
            std::move(directive_owner), "C/C++ source file");
        ROSE_ASSERT(persistent_directive == directive);
        publishOpenMPDirectiveParseCacheTree(persistent_directive,
                                             std::move(clause_cache),
                                             "C/C++ source directive");
        if (persistent_directive->getRequiresExplicitEnd()) {
          explicit_end_stack.push_back(
              std::make_pair(pragmaDeclaration, persistent_directive));
        }
        openMPPragmas().push_back(pragmaDeclaration);
        openMPDirectives().push_back(
            std::make_pair(pragmaDeclaration, persistent_directive));
      } else if (key == "acc" && wantsOpenACC) {
        const std::string parseText = "#pragma " + pragmaString;
        parseOpenACCDirectiveOrAbort(openaccSession, pragmaDeclaration,
                                     parseText, openaccOptions);
        const openacc::Directive *directive =
            openaccSession.find(pragmaDeclaration);
        if (directive == nullptr ||
            directive->kind() == openacc::DirectiveKind::End) {
          std::cerr << "REX_ACC_AST_INVARIANT[parse-session]: C/C++ OpenACC "
                       "pragma has no convertible typed directive\n";
          ROSE_ABORT();
        }
        openMPPragmas().push_back(pragmaDeclaration);
      }
    } // end for
    if (!explicit_end_stack.empty()) {
      std::cerr << "REX_OMP_AST_INVARIANT[paired-end]: required C/C++ OpenMP "
                   "end directive is missing\n";
      ROSE_ABORT();
    }
  }

  const bool openmpParseOnly =
      wantsOpenMP && sageFilePtr->get_openmp_parse_only();
  const bool openaccParseOnly =
      wantsOpenACC && sageFilePtr->get_openacc_parse_only();
  const bool constructsAst =
      (wantsOpenMP && !sageFilePtr->get_openmp_parse_only()) ||
      (wantsOpenACC && !sageFilePtr->get_openacc_parse_only());
  if ((openmpParseOnly || openaccParseOnly) && constructsAst) {
    std::cerr << "REX_DIRECTIVE_MODE_INVARIANT: parse-only and AST-building "
                 "OpenMP/OpenACC modes cannot be mixed in one source file\n";
    ROSE_ABORT();
  }

  if (openmpParseOnly || openaccParseOnly) {
    if (SgProject::get_verbose() > 1) {
      printf("Stopping after OpenMP/OpenACC directive parsing as requested\n");
    }
    releaseOpenMPParseStateForSourceFile(sageFilePtr);
    mark_processed(sageFilePtr);
    return;
  }

  // Build OpenMP AST nodes based on parsing results
  if (!isFortran) {
    convert_Cxx_Pragma_Pairs(sageFilePtr);
  }

  if (isFortran) {
    if (parsed_fortran_pragmas) {
      convert_Fortran_Pragma_Pairs(sageFilePtr);
    }
    if (parsed_fortran_acc_pragmas) {
      convert_Fortran_ACC_Pragma_Pairs(sageFilePtr, openaccSession);
    }
  }
  if (SgProject::get_verbose() > 1) {
    printf("Calling convert_OpenMP_pragma_to_AST() \n");
  }
  OpenMPIRToSageAST(sageFilePtr, openaccSession);
  if (isFortran && parsed_fortran_pragmas) {
    removeFortranOpenMPPragmas(sageFilePtr);
  }

  if (!isFortran && (!openMPDirectives().empty() ||
                     openaccSession.hasConvertibleDirectives())) {
    markOpenMPSourceFileAsModified(sageFilePtr);
  }
  // stop here if only OpenMP AST construction is requested
  if (sageFilePtr->get_openmp_ast_only()) {
    if (SgProject::get_verbose() > 1) {
      printf("Skipping calls to analyze/lower OpenMP "
             "sageFilePtr->get_openmp_ast_only() = %s \n",
             sageFilePtr->get_openmp_ast_only() ? "true" : "false");
    }
    sageFilePtr = Rose::AstJson::roundTripSourceFile(
        sageFilePtr, Rose::AstJson::Checkpoint::PostOmpConstruction);
    releaseOpenMPParseStateForSourceFile(sageFilePtr);
    mark_processed(sageFilePtr);
    return;
  }

  sageFilePtr = Rose::AstJson::roundTripSourceFile(
      sageFilePtr, Rose::AstJson::Checkpoint::PostOmpConstruction);
  releaseOpenMPParseStateForSourceFile(sageFilePtr);
  conversion_session.reset();

  // Analyze OpenMP AST
  analyze_omp(sageFilePtr);

  // stop here if only OpenMP AST analyzing is requested
  if (sageFilePtr->get_openmp_analyzing()) {
    if (SgProject::get_verbose() > 1) {
      printf("Skipping calls to lower OpenMP "
             "sageFilePtr->get_openmp_analyzing() = %s \n",
             sageFilePtr->get_openmp_analyzing() ? "true" : "false");
    }
    mark_processed(sageFilePtr);
    return;
  }

  lower_omp(sageFilePtr);
  sageFilePtr = Rose::AstJson::roundTripSourceFile(
      sageFilePtr, Rose::AstJson::Checkpoint::PostOmpLowering);
  mark_processed(sageFilePtr);
}

} // namespace OmpSupport

SgStatement *
convertDirective(std::pair<SgPragmaDeclaration *, OpenMPDirective *>
                     current_OpenMPIR_to_SageIII) {
  OpenMPDirectiveKind directive_kind =
      current_OpenMPIR_to_SageIII.second->getKind();
  SgStatement *result = NULL;
  SgStatement *combined_body_preprocessing_source = NULL;

  switch (directive_kind) {
  case OMPD_metadirective:
  case OMPD_begin_metadirective:
  case OMPD_teams:
  case OMPD_atomic:
  case OMPD_do:
  case OMPD_do_simd:
  case OMPD_taskgroup:
  case OMPD_dispatch:
  case OMPD_master:
  case OMPD_masked:
  case OMPD_distribute:
  case OMPD_workdistribute:
  case OMPD_loop:
  case OMPD_taskloop:
  case OMPD_target_parallel_for:
  case OMPD_target_parallel_do:
  case OMPD_target_parallel:
  case OMPD_distribute_simd:
  case OMPD_distribute_parallel_for:
  case OMPD_distribute_parallel_do:
  case OMPD_distribute_parallel_for_simd:
  case OMPD_distribute_parallel_do_simd:
  case OMPD_taskloop_simd:
  case OMPD_target_parallel_for_simd:
  case OMPD_target_parallel_do_simd:
  case OMPD_target_parallel_loop:
  case OMPD_target_simd:
  case OMPD_target_teams:
  case OMPD_target_teams_distribute:
  case OMPD_target_teams_workdistribute:
  case OMPD_target_teams_distribute_simd:
  case OMPD_target_teams_loop:
  case OMPD_target_teams_distribute_parallel_for:
  case OMPD_target_teams_distribute_parallel_do:
  case OMPD_target_teams_distribute_parallel_for_simd:
  case OMPD_target_teams_distribute_parallel_do_simd:
  case OMPD_master_taskloop_simd:
  case OMPD_masked_taskloop_simd:
  case OMPD_parallel_master_taskloop:
  case OMPD_parallel_master_taskloop_simd:
  case OMPD_teams_distribute:
  case OMPD_teams_distribute_simd:
  case OMPD_teams_distribute_parallel_for:
  case OMPD_teams_distribute_parallel_do:
  case OMPD_teams_distribute_parallel_for_simd:
  case OMPD_teams_distribute_parallel_do_simd:
  case OMPD_teams_loop:
  case OMPD_parallel_master:
  case OMPD_master_taskloop:
  case OMPD_masked_taskloop:
  case OMPD_parallel_loop:
  case OMPD_task:
  case OMPD_target_data:
  case OMPD_single:
  case OMPD_for:
  case OMPD_for_simd:
  case OMPD_target:
  case OMPD_critical:
  case OMPD_sections:
  case OMPD_section:
  case OMPD_simd:
  case OMPD_parallel:
  case OMPD_workshare:
  case OMPD_tile:
  case OMPD_target_data_composite:
  case OMPD_scope:
  case OMPD_parallel_masked:
  case OMPD_assume:
  case OMPD_taskgraph:
  case OMPD_fuse:
  case OMPD_interchange:
  case OMPD_reverse:
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
  case OMPD_parallel_do_simd:
  case OMPD_parallel_for:
  case OMPD_parallel_for_simd:
  case OMPD_parallel_sections:
  case OMPD_parallel_workshare: {
    combined_body_preprocessing_source =
        getOpenMPBlockBody(current_OpenMPIR_to_SageIII);
    result = convertCombinedBodyDirective(current_OpenMPIR_to_SageIII);
    break;
  }
  case OMPD_declare_mapper:
  case OMPD_nothing:
  case OMPD_cancellation_point:
  case OMPD_target_update:
  case OMPD_scan:
  case OMPD_target_enter_data:
  case OMPD_target_exit_data:
  case OMPD_depobj:
  case OMPD_cancel:
  case OMPD_error:
  case OMPD_interop:
  case OMPD_begin_declare_target:
  case OMPD_assumes:
  case OMPD_begin_assumes:
  case OMPD_end_assumes:
  case OMPD_end_assume:
  case OMPD_groupprivate: {
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
    std::cerr << "REX_OMP_AST_INVARIANT[ompx-source-ownership]: source-only "
                 "OMPX directive reached semantic Sage AST conversion\n";
    ROSE_ABORT();
  }
  default: {
    std::cerr << "REX_OMP_AST_INVARIANT[directive-conversion]: unsupported "
                 "directive kind "
              << static_cast<int>(directive_kind) << "\n";
    ROSE_ABORT();
  }
  }
  SgPragmaDeclaration *pdecl = current_OpenMPIR_to_SageIII.first;
  if (result == NULL) {
    std::cerr << "REX_OMP_AST_INVARIANT[directive-conversion]: no Sage AST "
                 "node was built for parsed directive kind "
              << static_cast<int>(current_OpenMPIR_to_SageIII.second->getKind())
              << ": " << pdecl->get_pragma()->get_pragma() << "\n";
    ROSE_ABORT();
  }
  setOneSourcePositionForTransformation(result);
  copyStartFileInfo(pdecl, result);
  copyEndFileInfo(pdecl, result);
  initializeGeneratedOpenMPStatement(result);

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
  const SgStatement::directive_end_kind_enum pragma_end_kind =
      pdecl->get_directive_end_kind();
  if (is_fortran_file) {
    switch (pragma_end_kind) {
    case SgStatement::e_directive_end_not_applicable:
    case SgStatement::e_directive_end_implicit:
    case SgStatement::e_directive_end_explicit:
      result->set_directive_end_kind(pragma_end_kind);
      break;
    default:
      std::cerr << "REX_OMP_AST_INVARIANT[directive-end-kind]: Fortran pragma "
                   "has invalid typed end kind="
                << static_cast<int>(pragma_end_kind) << "\n";
      ROSE_ABORT();
    }
    if (current_OpenMPIR_to_SageIII.second->getRequiresExplicitEnd() &&
        pragma_end_kind != SgStatement::e_directive_end_explicit) {
      std::cerr << "REX_OMP_AST_INVARIANT[directive-end-kind]: parsed "
                   "Fortran explicit END has no exact pragma provenance\n";
      ROSE_ABORT();
    }
  } else if (current_OpenMPIR_to_SageIII.second->getRequiresExplicitEnd()) {
    setExactDirectiveEndKind(result, SgStatement::e_directive_end_explicit,
                             "C/C++ OpenMP matched source END");
  }
  if (isSgGlobal(scope) != NULL && isSgDeclarationStatement(result) == NULL) {
    std::cerr << "REX_OMP_AST_INVARIANT[directive-scope]: parsed OpenMP "
                 "directive produced a non-declaration node at global scope\n";
    ROSE_ABORT();
  }

  // Publish the converted statement at the pragma's exact structural and
  // physical-output boundary before transferring any preprocessing records.
  // A detached destination has no valid output identity and must never be used
  // as a temporary preprocessing owner.
  replaceStatement(pdecl, result);
  if (combined_body_preprocessing_source != NULL) {
    movePreprocessingInfo(combined_body_preprocessing_source, result,
                          PreprocessingInfo::before, PreprocessingInfo::after,
                          true);
  }
  moveInterveningPreprocessingInfoToOpenMPBody(pdecl, result);
  movePreprocessingInfo(pdecl, result);
  validateConvertedOpenMPStatementLocation(pdecl, result);

  return result;
}

SgStatement *
convertVariantDirective(std::pair<SgPragmaDeclaration *, OpenMPDirective *>
                            current_OpenMPIR_to_SageIII) {
  SgPragmaDeclaration *pragma_declaration = current_OpenMPIR_to_SageIII.first;
  OpenMPDirective *directive = current_OpenMPIR_to_SageIII.second;
  if (pragma_declaration == nullptr || directive == nullptr ||
      directive->getKind() == OMPD_end) {
    std::cerr << "REX_OMP_AST_INVARIANT[variant-directive]: invalid nested "
                 "directive identity\n";
    ROSE_ABORT();
  }
  if (getClauseParseCache(directive) == nullptr) {
    std::cerr << "REX_OMP_AST_INVARIANT[variant-directive-cache]: nested "
                 "directive has no parser-published semantic cache\n";
    ROSE_ABORT();
  }

  SgStatement *result =
      convertVariantBodyDirective(current_OpenMPIR_to_SageIII);
  if (result == NULL) {
    std::cerr << "REX_OMP_AST_INVARIANT[variant-directive]: unsupported "
                 "nested directive kind "
              << static_cast<int>(directive->getKind()) << " for base language "
              << static_cast<int>(directive->getBaseLang()) << "\n";
    ROSE_ABORT();
  }

  initializeGeneratedOpenMPVariantDirective(current_OpenMPIR_to_SageIII.first,
                                            result);
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
  case OMPD_parallel_do_simd:
  case OMPD_parallel_for:
  case OMPD_parallel_for_simd:
  case OMPD_parallel_sections:
  case OMPD_parallel_workshare: {
    result = convertOmpParallelStatementFromCombinedDirectives(
        current_OpenMPIR_to_SageIII);
    break;
  }
  default: {
    std::cerr << "REX_OMP_AST_INVARIANT[combined-directive]: unsupported "
                 "directive kind "
              << static_cast<int>(directive_kind) << "\n";
    ROSE_ABORT();
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
  const std::vector<const OmpParsedExpression *> *parsed_nodes =
      getParsedClauseExpressionNodes(current_OpenMPIR_to_SageIII.second,
                                     current_omp_clause);
  const std::vector<const OmpParsedExpression *> *auxiliary_nodes =
      getParsedClauseAuxiliaryExpressionNodes(
          current_OpenMPIR_to_SageIII.second, current_omp_clause);
  size_t parsed_node_index = 0;
  size_t auxiliary_node_index = 0;

  auto clone_cached_expression =
      [&](const std::string &text,
          const std::vector<const OmpParsedExpression *> *nodes,
          OpenMPExprParseMode mode = OMP_EXPR_PARSE_expression) {
        size_t *next_index = nullptr;
        if (nodes == parsed_nodes) {
          next_index = &parsed_node_index;
        } else if (nodes == auxiliary_nodes) {
          next_index = &auxiliary_node_index;
        } else {
          std::cerr << "REX_OMP_AST_INVARIANT[modern-clause]: expression "
                       "cursor does not name an exact clause cache\n";
          ROSE_ABORT();
        }
        return consumeParsedClauseExpression(clause_kind, nodes, *next_index,
                                             text, mode);
      };

  switch (clause_kind) {
  case OMPC_nowait: {
    SgOmpNowaitClause *nowait_clause =
        new SgOmpNowaitClause((SgExpression *)NULL);
    SgExpression *nowait_expression = NULL;
    const std::vector<OpenMPExpressionItem> &current_expressions =
        current_omp_clause->getExpressionItems();
    const std::size_t expression_count = current_expressions.size();
    const std::size_t cached_node_count =
        parsed_nodes == nullptr ? 0 : parsed_nodes->size();
    if (expression_count > 1 || cached_node_count != expression_count) {
      std::cerr << "REX_OMP_AST_INVARIANT[nowait]: expected exact 0/1 text, "
                   "and cached-node cardinality; text="
                << expression_count << " cache=" << cached_node_count << '\n';
      ROSE_ABORT();
    }
    if (expression_count == 1) {
      const OmpParsedExpression *parsed = parsed_nodes->front();
      requireCachedParsedExpression(parsed);
      const OpenMPExpressionItem &expression = current_expressions.front();
      if (expression.fragment.spelling.empty() ||
          parsed->text != expression.fragment.spelling ||
          parsed->mode != expression.parse_mode ||
          parsed->mode != OMP_EXPR_PARSE_expression) {
        std::cerr << "REX_OMP_AST_INVARIANT[nowait]: expression text and "
                     "exact cached callback node diverge\n";
        ROSE_ABORT();
      }
      nowait_expression =
          clone_cached_expression(expression.fragment.spelling, parsed_nodes,
                                  OMP_EXPR_PARSE_expression);
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
  case OMPC_self_maps: {
    sg_clause = new SgOmpSelfMapsClause();
    break;
  }
  case OMPC_indirect: {
    sg_clause = new SgOmpIndirectClause();
    break;
  }
  case OMPC_no_openmp: {
    sg_clause = new SgOmpNoOpenmpClause();
    break;
  }
  case OMPC_no_openmp_routines: {
    sg_clause = new SgOmpNoOpenmpRoutinesClause();
    break;
  }
  case OMPC_no_parallelism: {
    sg_clause = new SgOmpNoParallelismClause();
    break;
  }
  case OMPC_at: {
    OpenMPAtClauseKind kind =
        static_cast<OpenMPAtClause *>(current_omp_clause)->getAtKind();
    SgOmpClause::omp_at_kind_enum sage_kind = SgOmpClause::e_omp_at_unknown;
    switch (kind) {
    case OMPC_AT_compilation:
      sage_kind = SgOmpClause::e_omp_at_compilation;
      break;
    case OMPC_AT_execution:
      sage_kind = SgOmpClause::e_omp_at_execution;
      break;
    default:
      std::cerr << "REX_OMP_AST_INVARIANT[at]: unsupported kind "
                << static_cast<int>(kind) << "\n";
      ROSE_ABORT();
    }
    sg_clause = new SgOmpAtClause(sage_kind);
    break;
  }
  case OMPC_severity: {
    OpenMPSeverityClauseKind kind =
        static_cast<OpenMPSeverityClause *>(current_omp_clause)
            ->getSeverityKind();
    SgOmpClause::omp_severity_kind_enum sage_kind =
        SgOmpClause::e_omp_severity_unknown;
    switch (kind) {
    case OMPC_SEVERITY_fatal:
      sage_kind = SgOmpClause::e_omp_severity_fatal;
      break;
    case OMPC_SEVERITY_warning:
      sage_kind = SgOmpClause::e_omp_severity_warning;
      break;
    default:
      std::cerr << "REX_OMP_AST_INVARIANT[severity]: unsupported kind "
                << static_cast<int>(kind) << "\n";
      ROSE_ABORT();
    }
    sg_clause = new SgOmpSeverityClause(sage_kind);
    break;
  }
  case OMPC_doacross: {
    OpenMPDoacrossClause *clause =
        static_cast<OpenMPDoacrossClause *>(current_omp_clause);
    SgOmpClause::omp_doacross_kind_enum sage_kind =
        SgOmpClause::e_omp_doacross_unknown;
    switch (clause->getType()) {
    case OMPC_DOACROSS_TYPE_source:
      sage_kind = SgOmpClause::e_omp_doacross_source;
      break;
    case OMPC_DOACROSS_TYPE_sink:
      sage_kind = SgOmpClause::e_omp_doacross_sink;
      break;
    default:
      std::cerr << "REX_OMP_AST_INVARIANT[doacross]: unsupported kind\n";
      ROSE_ABORT();
    }
    SgExprListExp *expressions = SageBuilder::buildExprListExp();
    if (clause->getType() == OMPC_DOACROSS_TYPE_source) {
      if (clause->hasSourceExpression()) {
        expressions->append_expression(clone_cached_expression(
            clause->getSourceExpression().fragment.spelling, parsed_nodes,
            OMP_EXPR_PARSE_expression));
      }
    } else {
      for (const OpenMPExpressionItem &item : clause->getSinkArgs()) {
        expressions->append_expression(
            clone_cached_expression(item.fragment.spelling, parsed_nodes,
                                    OMP_EXPR_PARSE_variable_list));
      }
    }
    sg_clause = new SgOmpDoacrossClause(sage_kind, expressions);
    expressions->set_parent(sg_clause);
    break;
  }
  case OMPC_otherwise: {
    OpenMPOtherwiseClause *clause =
        static_cast<OpenMPOtherwiseClause *>(current_omp_clause);
    OpenMPDirective *variant = clause->getVariantDirective();
    SgStatement *variant_statement =
        variant == nullptr ? nullptr
                           : convertVariantDirective(std::make_pair(
                                 current_OpenMPIR_to_SageIII.first, variant));
    sg_clause = new SgOmpOtherwiseClause(variant_statement);
    if (variant_statement != nullptr) {
      variant_statement->set_parent(sg_clause);
    }
    break;
  }
  case OMPC_induction: {
    OpenMPInductionClause *clause =
        static_cast<OpenMPInductionClause *>(current_omp_clause);
    SgOmpInductionClause *result = new SgOmpInductionClause();
    clause->visitSpecificationItems(
        [&](OpenMPInductionClause::SpecificationItemKind source_kind,
            const ompparser::HostFragment *source_label,
            const ompparser::HostFragment &source_expression) {
          SgOmpClause::omp_induction_item_kind_enum kind =
              SgOmpClause::e_omp_induction_item_unknown;
          std::string label;
          switch (source_kind) {
          case OpenMPInductionClause::SpecificationItemKind::Step:
            if (source_label != nullptr) {
              std::cerr << "REX_OMP_AST_INVARIANT[induction]: step has an "
                           "unexpected label\n";
              ROSE_ABORT();
            }
            kind = SgOmpClause::e_omp_induction_item_step;
            break;
          case OpenMPInductionClause::SpecificationItemKind::Binding:
            if (source_label == nullptr) {
              std::cerr << "REX_OMP_AST_INVARIANT[induction]: binding has no "
                           "typed label\n";
              ROSE_ABORT();
            }
            kind = SgOmpClause::e_omp_induction_item_binding;
            label = consumeParsedClauseOpenMPSyntax(
                clause_kind, auxiliary_nodes, auxiliary_node_index,
                source_label->spelling);
            if (label.empty()) {
              std::cerr << "REX_OMP_AST_INVARIANT[induction]: binding has an "
                           "empty label\n";
              ROSE_ABORT();
            }
            break;
          case OpenMPInductionClause::SpecificationItemKind::Expression:
            if (source_label != nullptr) {
              std::cerr << "REX_OMP_AST_INVARIANT[induction]: expression has "
                           "an unexpected label\n";
              ROSE_ABORT();
            }
            kind = SgOmpClause::e_omp_induction_item_expression;
            break;
          }
          SgOmpInductionItem *item = new SgOmpInductionItem(
              kind, label,
              clone_cached_expression(source_expression.spelling,
                                      auxiliary_nodes));
          setOneSourcePositionForTransformation(item);
          item->get_expression()->set_parent(item);
          result->get_items().push_back(item);
          item->set_parent(result);
        });
    sg_clause = result;
    break;
  }
  case OMPC_apply: {
    OpenMPApplyClause *clause =
        static_cast<OpenMPApplyClause *>(current_omp_clause);
    std::function<SgOmpApplyClause *(const OpenMPApplyClause *)> build_apply;
    build_apply = [&](const OpenMPApplyClause *source) {
      if (source == nullptr) {
        std::cerr << "REX_OMP_AST_INVARIANT[apply]: null apply clause\n";
        ROSE_ABORT();
      }
      const std::string label =
          source->getLabel().empty()
              ? std::string()
              : consumeParsedClauseOpenMPSyntax(clause_kind, auxiliary_nodes,
                                                auxiliary_node_index,
                                                source->getLabel());
      SgOmpApplyClause *result = new SgOmpApplyClause(label);
      const std::vector<OpenMPApplyClause::ApplyTransform> &source_transforms =
          source->getTransformations();
      if (source_transforms.empty() && result->get_label().empty()) {
        std::cerr << "REX_OMP_AST_INVARIANT[apply]: clause has neither a "
                     "label nor a transformation\n";
        ROSE_ABORT();
      }
      for (size_t transform_index = 0;
           transform_index < source_transforms.size(); ++transform_index) {
        const OpenMPApplyClause::ApplyTransform &transform =
            source_transforms[transform_index];
        SgOmpClause::omp_apply_transform_kind_enum sage_kind =
            SgOmpClause::e_omp_apply_transform_unknown;
        std::string transformation_name;
        SgExpression *argument = nullptr;
        SgOmpApplyClause *nested_apply = nullptr;
        switch (transform.kind) {
        case OMPC_APPLY_TRANSFORM_unroll:
          sage_kind = SgOmpClause::e_omp_apply_transform_unroll;
          break;
        case OMPC_APPLY_TRANSFORM_unroll_partial:
          sage_kind = SgOmpClause::e_omp_apply_transform_unroll_partial;
          break;
        case OMPC_APPLY_TRANSFORM_unroll_full:
          sage_kind = SgOmpClause::e_omp_apply_transform_unroll_full;
          break;
        case OMPC_APPLY_TRANSFORM_reverse:
          sage_kind = SgOmpClause::e_omp_apply_transform_reverse;
          break;
        case OMPC_APPLY_TRANSFORM_interchange:
          sage_kind = SgOmpClause::e_omp_apply_transform_interchange;
          break;
        case OMPC_APPLY_TRANSFORM_nothing:
          sage_kind = SgOmpClause::e_omp_apply_transform_nothing;
          break;
        case OMPC_APPLY_TRANSFORM_tile_sizes:
          sage_kind = SgOmpClause::e_omp_apply_transform_tile_sizes;
          break;
        case OMPC_APPLY_TRANSFORM_apply:
          sage_kind = SgOmpClause::e_omp_apply_transform_nested_apply;
          nested_apply = build_apply(transform.nested_apply.get());
          break;
        case OMPC_APPLY_TRANSFORM_unknown:
          sage_kind = SgOmpClause::e_omp_apply_transform_named;
          transformation_name = consumeParsedClauseOpenMPSyntax(
              clause_kind, auxiliary_nodes, auxiliary_node_index,
              transform.argument.spelling);
          if (transformation_name.empty()) {
            std::cerr << "REX_OMP_AST_INVARIANT[apply]: named transform has "
                         "an empty name\n";
            ROSE_ABORT();
          }
          break;
        default:
          std::cerr << "REX_OMP_AST_INVARIANT[apply]: unsupported transform\n";
          ROSE_ABORT();
        }
        const bool requires_argument =
            sage_kind == SgOmpClause::e_omp_apply_transform_unroll_partial ||
            sage_kind == SgOmpClause::e_omp_apply_transform_tile_sizes;
        if (requires_argument && transform.argument.spelling.empty()) {
          std::cerr << "REX_OMP_AST_INVARIANT[apply]: transform requires an "
                       "argument\n";
          ROSE_ABORT();
        }
        if (!requires_argument &&
            sage_kind != SgOmpClause::e_omp_apply_transform_named &&
            sage_kind != SgOmpClause::e_omp_apply_transform_nested_apply &&
            !transform.argument.spelling.empty()) {
          std::cerr << "REX_OMP_AST_INVARIANT[apply]: transform has an "
                       "unexpected argument\n";
          ROSE_ABORT();
        }
        if (requires_argument) {
          argument = clone_cached_expression(transform.argument.spelling,
                                             auxiliary_nodes);
        }
        SgOmpClause::omp_clause_separator_enum separator =
            SgOmpClause::e_omp_clause_separator_unknown;
        if (transform_index == 0) {
          separator = SgOmpClause::e_omp_clause_separator_none;
        } else {
          switch (transform.separator) {
          case OMPC_CLAUSE_SEP_comma:
            separator = SgOmpClause::e_omp_clause_separator_comma;
            break;
          case OMPC_CLAUSE_SEP_space:
            separator = SgOmpClause::e_omp_clause_separator_space;
            break;
          default:
            std::cerr << "REX_OMP_AST_INVARIANT[apply]: unsupported "
                         "transform separator\n";
            ROSE_ABORT();
          }
        }
        SgOmpApplyTransformation *item = new SgOmpApplyTransformation(
            sage_kind, separator, transformation_name, argument, nested_apply);
        setOneSourcePositionForTransformation(item);
        if (argument != nullptr) {
          argument->set_parent(item);
        }
        if (nested_apply != nullptr) {
          nested_apply->set_parent(item);
        }
        result->get_transformations().push_back(item);
        item->set_parent(result);
      }
      setOneSourcePositionForTransformation(result);
      return result;
    };
    sg_clause = build_apply(clause);
    break;
  }
  case OMPC_init: {
    OpenMPInitClause *clause =
        static_cast<OpenMPInitClause *>(current_omp_clause);
    if (clause->getOperand().empty()) {
      std::cerr << "REX_OMP_AST_INVARIANT[init]: missing operand\n";
      ROSE_ABORT();
    }
    SgOmpInitModifierList *modifier_list =
        buildInitModifierList(clause_kind, clause->getModifiers(),
                              auxiliary_nodes, auxiliary_node_index, "init");
    SgExpression *operand = clone_cached_expression(
        clause->getOperand(), auxiliary_nodes, OMP_EXPR_PARSE_variable_list);
    SgOmpInitClause *result = new SgOmpInitClause(modifier_list, operand);
    modifier_list->set_parent(result);
    operand->set_parent(result);
    sg_clause = result;
    break;
  }
  case OMPC_absent:
  case OMPC_contains: {
    const std::vector<OpenMPDirectiveKind> &directives =
        clause_kind == OMPC_absent
            ? static_cast<OpenMPAbsentClause *>(current_omp_clause)
                  ->getDirectives()
            : static_cast<OpenMPContainsClause *>(current_omp_clause)
                  ->getDirectives();
    SgOmpClause::omp_directive_kind_list typed_directives;
    typed_directives.reserve(directives.size());
    for (OpenMPDirectiveKind directive_kind : directives) {
      typed_directives.push_back(convertDirectiveKind(directive_kind));
    }
    sg_clause = clause_kind == OMPC_absent
                    ? static_cast<SgOmpClause *>(
                          new SgOmpAbsentClause(typed_directives))
                    : static_cast<SgOmpClause *>(
                          new SgOmpContainsClause(typed_directives));
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
    SgExpression *operand = nullptr;
    const std::vector<OpenMPExpressionItem> &expressions =
        current_omp_clause->getExpressionItems();
    if (expressions.size() > 1) {
      std::cerr << "REX_OMP_AST_INVARIANT[destroy]: expected at most one "
                   "operand\n";
      ROSE_ABORT();
    }
    if (!expressions.empty()) {
      if (expressions.front().fragment.spelling.empty()) {
        std::cerr << "REX_OMP_AST_INVARIANT[destroy]: operand text is null\n";
        ROSE_ABORT();
      }
      operand =
          clone_cached_expression(expressions.front().fragment.spelling,
                                  parsed_nodes, OMP_EXPR_PARSE_expression);
    }
    sg_clause = new SgOmpDestroyClause(operand);
    if (operand != nullptr) {
      operand->set_parent(sg_clause);
    }
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
  requireParsedClauseExpressionsConsumed(clause_kind, parsed_nodes,
                                         parsed_node_index);
  requireParsedClauseExpressionsConsumed(clause_kind, auxiliary_nodes,
                                         auxiliary_node_index);
  setOneSourcePositionForTransformation(sg_clause);
  addOmpClause(directive, sg_clause);
  return sg_clause;
}

SgOmpDepobjStatement *buildOmpDepobjStatement(
    const std::pair<SgPragmaDeclaration *, OpenMPDirective *> &current) {
  OpenMPDepobjDirective *directive =
      dynamic_cast<OpenMPDepobjDirective *>(current.second);
  const OmpClauseParseCache *cache = getClauseParseCache(current.second);
  if (current.first == nullptr || directive == nullptr || cache == nullptr ||
      directive->getDepobj().empty() ||
      cache->directive_expression_nodes.size() != 1 ||
      cache->directive_expression_nodes.front() == nullptr ||
      cache->directive_expression_nodes.front()->text !=
          directive->getDepobj()) {
    std::cerr << "REX_OMP_AST_INVARIANT[depobj-expression]: directive has no "
                 "single producer-bound typed operand\n";
    ROSE_ABORT();
  }
  SgExpression *depobj =
      consumeParsedExpressionNode(cache->directive_expression_nodes.front());
  if (depobj == nullptr || depobj->get_type() == nullptr ||
      isSgTypeUnknown(depobj->get_type()) != nullptr ||
      depobj->get_parent() != nullptr) {
    std::cerr << "REX_OMP_AST_INVARIANT[depobj-expression]: operand has no "
                 "exact semantic expression, type, or unique owner\n";
    ROSE_ABORT();
  }
  SgOmpDepobjStatement *result = new SgOmpDepobjStatement(depobj);
  depobj->set_parent(result);
  return result;
}

SgStatement *
convertNonBodyDirective(std::pair<SgPragmaDeclaration *, OpenMPDirective *>
                            current_OpenMPIR_to_SageIII) {

  OpenMPDirectiveKind directive_kind =
      current_OpenMPIR_to_SageIII.second->getKind();
  SgStatement *result = NULL;
  OpenMPClauseKind clause_kind;

  switch (directive_kind) {
  case OMPD_error: {
    result = new SgOmpErrorStatement();
    break;
  }
  case OMPD_interop: {
    result = new SgOmpInteropStatement();
    break;
  }
  case OMPD_begin_declare_target: {
    SgOmpBeginDeclareTargetStatement *begin =
        new SgOmpBeginDeclareTargetStatement();
    begin->set_use_underscore_spelling(
        current_OpenMPIR_to_SageIII.second->getDeclareTargetUnderscore());
    result = begin;
    break;
  }
  case OMPD_assumes: {
    result = new SgOmpAssumesStatement();
    break;
  }
  case OMPD_begin_assumes: {
    result = new SgOmpBeginAssumesStatement();
    break;
  }
  case OMPD_end_assumes: {
    result = new SgOmpEndAssumesStatement();
    break;
  }
  case OMPD_end_assume: {
    result = new SgOmpEndAssumeStatement();
    break;
  }
  case OMPD_groupprivate: {
    OpenMPGroupprivateDirective *groupprivate =
        static_cast<OpenMPGroupprivateDirective *>(
            current_OpenMPIR_to_SageIII.second);
    ROSE_ASSERT(groupprivate != nullptr);
    if (!openMPExpressionVariables().empty()) {
      std::cerr << "REX_OMP_AST_INVARIANT[groupprivate-parser-state]: "
                   "groupprivate conversion started with stale parser state\n";
      ROSE_ABORT();
    }
    const OmpClauseParseCache *cache =
        getClauseParseCache(current_OpenMPIR_to_SageIII.second);
    if (cache == nullptr || cache->directive_expression_nodes.size() !=
                                groupprivate->getGroupprivateList().size()) {
      std::cerr << "REX_OMP_AST_INVARIANT[groupprivate]: directive variable "
                   "cache does not match the semantic list\n";
      ROSE_ABORT();
    }
    SgExprListExp *variables = SageBuilder::buildExprListExp();
    for (const OmpParsedExpression *parsed :
         cache->directive_expression_nodes) {
      SgExpression *variable = consumeParsedExpressionNode(parsed);
      if (variable == nullptr) {
        std::cerr << "REX_OMP_AST_INVARIANT[groupprivate]: cached variable "
                     "has no expression\n";
        ROSE_ABORT();
      }
      variables->append_expression(variable);
    }
    clearOpenMPClauseTemporaryState();
    result = new SgOmpGroupprivateStatement(variables);
    variables->set_parent(result);
    break;
  }
  case OMPD_nothing: {
    result = new SgOmpNothingStatement();
    break;
  }
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

    SgDeclarationScope *mapper_local_scope =
        getDirectiveLocalScope(current_OpenMPIR_to_SageIII.second);
    if (mapper_local_scope == nullptr) {
      std::cerr << "REX_OMP_AST_INVARIANT[declare-mapper-context]: mapper "
                   "has no directive-local declaration scope\n";
      ROSE_ABORT();
    }
    SageBuilder::setNonrealDeclarationScope(sg_mapper, mapper_local_scope);

    const NormalizedDeclareMapperData mapper_data =
        normalizeDeclareMapperData(mapper_directive);
    const OmpClauseParseCache *cache =
        getClauseParseCache(current_OpenMPIR_to_SageIII.second);
    if (cache == nullptr || mapper_data.mapper_type.empty() ||
        mapper_data.mapper_variable.empty() ||
        cache->declare_mapper_type_node == nullptr ||
        cache->declare_mapper_variable_node == nullptr) {
      std::cerr << "REX_OMP_AST_INVARIANT[declare-mapper-payload]: mapper has "
                   "no complete typed callback cache\n";
      ROSE_ABORT();
    }
    sg_mapper->set_identifier(mapper_data.identifier);
    sg_mapper->set_identifier_is_explicit(mapper_data.identifier_is_explicit);

    if (sg_mapper->get_identifier() ==
            SgOmpClause::e_omp_declare_mapper_identifier_user &&
        !mapper_data.user_defined_identifier.empty()) {
      if (cache->directive_expression_nodes.size() != 1) {
        std::cerr << "REX_OMP_AST_INVARIANT[declare-mapper-identifier]: user "
                     "identifier has no unique exact parser token\n";
        ROSE_ABORT();
      }
      SgExpression *user_defined_identifier = parseMapperIdentifierExpression(
          OMPC_unknown, cache->directive_expression_nodes.front(),
          mapper_data.user_defined_identifier);
      sg_mapper->set_user_defined_identifier(user_defined_identifier);
      if (user_defined_identifier != nullptr) {
        setOneSourcePositionForTransformation(user_defined_identifier);
        user_defined_identifier->set_parent(sg_mapper);
      }
    } else {
      if (!cache->directive_expression_nodes.empty()) {
        std::cerr << "REX_OMP_AST_INVARIANT[declare-mapper-identifier]: "
                     "non-user identifier unexpectedly owns parser tokens\n";
        ROSE_ABORT();
      }
    }

    const OmpParsedExpression *parsed_type = cache->declare_mapper_type_node;
    if (parsed_type->mode != OMP_EXPR_PARSE_openmp_declare_mapper_type ||
        parsed_type->text != mapper_data.mapper_type) {
      std::cerr << "REX_OMP_AST_INVARIANT[declare-mapper-type]: cached type "
                   "role or spelling disagrees with parser IR\n";
      ROSE_ABORT();
    }
    SgTypeExpression *mapper_type =
        isSgTypeExpression(consumeParsedExpressionNode(parsed_type));
    if (mapper_type == nullptr ||
        mapper_type->get_represented_type() == nullptr) {
      std::cerr << "REX_OMP_AST_INVARIANT[declare-mapper-type]: cached "
                   "payload is not an exact type expression\n";
      ROSE_ABORT();
    }
    sg_mapper->set_mapper_type(mapper_type);
    mapper_type->set_parent(sg_mapper);

    const OmpParsedExpression *parsed_variable =
        cache->declare_mapper_variable_node;
    if (parsed_variable->mode !=
            OMP_EXPR_PARSE_openmp_declare_mapper_variable ||
        parsed_variable->text != mapper_data.mapper_variable) {
      std::cerr << "REX_OMP_AST_INVARIANT[declare-mapper-variable]: cached "
                   "variable role or spelling disagrees with parser IR\n";
      ROSE_ABORT();
    }
    SgVarRefExp *mapper_variable =
        isSgVarRefExp(consumeParsedExpressionNode(parsed_variable));
    SgVariableSymbol *mapper_symbol =
        mapper_local_scope->lookup_variable_symbol(mapper_data.mapper_variable);
    if (mapper_variable == nullptr || mapper_symbol == nullptr ||
        mapper_variable->get_symbol() != mapper_symbol ||
        mapper_symbol->get_declaration() == nullptr ||
        mapper_symbol->get_declaration()->get_type() !=
            mapper_type->get_represented_type()) {
      std::cerr << "REX_OMP_AST_INVARIANT[declare-mapper-variable]: cached "
                   "variable is not the exact typed directive-local symbol\n";
      ROSE_ABORT();
    }
    sg_mapper->set_mapper_variable(mapper_variable);
    mapper_variable->set_parent(sg_mapper);
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
  case OMPD_scan: {
    result = new SgOmpScanStatement();
    break;
  }
  case OMPD_target_enter_data: {
    result = new SgOmpTargetEnterDataStatement();
    break;
  }
  case OMPD_target_exit_data: {
    result = new SgOmpTargetExitDataStatement();
    break;
  }
  case OMPD_depobj: {
    result = buildOmpDepobjStatement(current_OpenMPIR_to_SageIII);
    break;
  }
  case OMPD_ordered: {
    result = new SgOmpOrderedDependStatement();
    break;
  }
  default: {
    std::cerr << "REX_OMP_AST_INVARIANT[nonbody-directive]: unsupported "
                 "directive kind "
              << static_cast<int>(directive_kind) << "\n";
    ROSE_ABORT();
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
    case OMPC_if:
    case OMPC_message:
    case OMPC_holds:
    case OMPC_use:
    case OMPC_no_openmp_constructs: {
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
    case OMPC_at:
    case OMPC_severity:
    case OMPC_absent:
    case OMPC_contains:
    case OMPC_no_openmp:
    case OMPC_no_openmp_routines:
    case OMPC_no_parallelism:
    case OMPC_init:
    case OMPC_destroy: {
      convertSimpleClause(isSgStatement(result), current_OpenMPIR_to_SageIII,
                          *clause_iter);
      break;
    }
    case OMPC_device_type: {
      SgOmpGroupprivateStatement *groupprivate =
          isSgOmpGroupprivateStatement(result);
      if (groupprivate == nullptr) {
        std::cerr << "REX_OMP_AST_INVARIANT[device-type]: clause on "
                     "unsupported directive\n";
        ROSE_ABORT();
      }
      groupprivate->set_device_type_kind(toSgOmpDeclareTargetDeviceTypeKind(
          static_cast<OpenMPDeviceTypeClause *>(*clause_iter)
              ->getDeviceTypeClauseKind()));
      break;
    }
    case OMPC_depend: {
      convertDependClause(isSgStatement(result), current_OpenMPIR_to_SageIII,
                          *clause_iter);
      break;
    }
    case OMPC_depobj_update: {
      convertDepobjUpdateClause(result, current_OpenMPIR_to_SageIII,
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
    case OMPC_inclusive:
    case OMPC_exclusive: {
      convertClause(isSgStatement(result), current_OpenMPIR_to_SageIII,
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
  SgStatement *body = NULL;
  if (directive_kind != OMPD_end) {
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
  case OMPD_target_data_composite: {
    result = new SgOmpTargetDataCompositeStatement(body);
    break;
  }
  case OMPD_scope: {
    result = new SgOmpScopeStatement(body);
    break;
  }
  case OMPD_parallel_masked: {
    result = new SgOmpParallelMaskedStatement(body);
    break;
  }
  case OMPD_assume: {
    result = new SgOmpAssumeStatement(body);
    break;
  }
  case OMPD_taskgraph: {
    result = new SgOmpTaskgraphStatement(body);
    break;
  }
  case OMPD_fuse: {
    result = new SgOmpFuseStatement(body);
    break;
  }
  case OMPD_interchange: {
    result = new SgOmpInterchangeStatement(body);
    break;
  }
  case OMPD_reverse: {
    result = new SgOmpReverseStatement(body);
    break;
  }
  case OMPD_do: {
    result = new SgOmpDoStatement(NULL, body);
    break;
  }
  case OMPD_do_simd: {
    result = new SgOmpForSimdStatement(NULL, body);
    markFortranDoDirectiveSpelling(result);
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
    result = new SgOmpMaskedStatement(NULL, body);
    break;
  }
  case OMPD_distribute: {
    result = new SgOmpDistributeStatement(NULL, body);
    break;
  }
  case OMPD_workdistribute: {
    result = new SgOmpWorkdistributeStatement(NULL, body);
    break;
  }
  case OMPD_loop: {
    result = new SgOmpLoopStatement(NULL, body);
    break;
  }
  case OMPD_taskloop: {
    result = new SgOmpTaskloopStatement(NULL, body);
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
    const std::string name =
        consumeCriticalDirectiveName(current_OpenMPIR_to_SageIII.second);
    result = new SgOmpCriticalStatement(NULL, body, SgName(name));
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
  case OMPD_metadirective:
  case OMPD_begin_metadirective: {
    result = new SgOmpMetadirectiveStatement(NULL, body);
    isSgOmpMetadirectiveStatement(result)->set_source_form_is_begin(
        directive_kind == OMPD_begin_metadirective);
    break;
  }
  case OMPD_target_parallel_for: {
    result = new SgOmpTargetParallelForStatement(NULL, body);
    break;
  }
  case OMPD_target_parallel_do: {
    result = new SgOmpTargetParallelForStatement(NULL, body);
    markFortranDoDirectiveSpelling(result);
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
  case OMPD_distribute_parallel_do: {
    result = new SgOmpDistributeParallelForStatement(NULL, body);
    markFortranDoDirectiveSpelling(result);
    break;
  }
  case OMPD_distribute_parallel_for_simd: {
    result = new SgOmpDistributeParallelForSimdStatement(NULL, body);
    break;
  }
  case OMPD_distribute_parallel_do_simd: {
    result = new SgOmpDistributeParallelForSimdStatement(NULL, body);
    markFortranDoDirectiveSpelling(result);
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
  case OMPD_target_parallel_do_simd: {
    result = new SgOmpTargetParallelForSimdStatement(NULL, body);
    markFortranDoDirectiveSpelling(result);
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
  case OMPD_target_teams_workdistribute: {
    result = new SgOmpTargetTeamsWorkdistributeStatement(NULL, body);
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
  case OMPD_target_teams_distribute_parallel_do: {
    result = new SgOmpTargetTeamsDistributeParallelForStatement(NULL, body);
    markFortranDoDirectiveSpelling(result);
    break;
  }
  case OMPD_target_teams_distribute_parallel_for_simd: {
    result = new SgOmpTargetTeamsDistributeParallelForSimdStatement(NULL, body);
    break;
  }
  case OMPD_target_teams_distribute_parallel_do_simd: {
    result = new SgOmpTargetTeamsDistributeParallelForSimdStatement(NULL, body);
    markFortranDoDirectiveSpelling(result);
    break;
  }
  case OMPD_master_taskloop_simd: {
    result = new SgOmpMasterTaskloopSimdStatement(NULL, body);
    break;
  }
  case OMPD_masked_taskloop_simd: {
    result = new SgOmpMaskedTaskloopSimdStatement(NULL, body);
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
  case OMPD_teams_distribute_parallel_do: {
    result = new SgOmpTeamsDistributeParallelForStatement(NULL, body);
    markFortranDoDirectiveSpelling(result);
    break;
  }
  case OMPD_teams_distribute_parallel_for_simd: {
    result = new SgOmpTeamsDistributeParallelForSimdStatement(NULL, body);
    break;
  }
  case OMPD_teams_distribute_parallel_do_simd: {
    result = new SgOmpTeamsDistributeParallelForSimdStatement(NULL, body);
    markFortranDoDirectiveSpelling(result);
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
    result = new SgOmpMaskedTaskloopStatement(NULL, body);
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
    std::cerr << "REX_OMP_AST_INVARIANT[body-directive]: unsupported "
                 "directive kind "
              << static_cast<int>(directive_kind) << "\n";
    ROSE_ABORT();
  }
  }
  if (body != NULL) {
    body->set_parent(result);
  }
  if (current_OpenMPIR_to_SageIII.second->getRequiresExplicitEnd()) {
    setExactDirectiveEndKind(result, SgStatement::e_directive_end_explicit,
                             "OpenMP body directive requires source END");
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
    case OMPC_filter:
    case OMPC_partial:
    case OMPC_message:
    case OMPC_graph_id:
    case OMPC_graph_reset:
    case OMPC_transparent:
    case OMPC_threadset:
    case OMPC_safesync:
    case OMPC_looprange:
    case OMPC_no_openmp_constructs:
    case OMPC_holds:
    case OMPC_use: {
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
    case OMPC_full:
    case OMPC_self_maps:
    case OMPC_indirect:
    case OMPC_no_openmp:
    case OMPC_no_openmp_routines:
    case OMPC_no_parallelism:
    case OMPC_severity:
    case OMPC_doacross:
    case OMPC_otherwise:
    case OMPC_induction:
    case OMPC_apply:
    case OMPC_init:
    case OMPC_absent:
    case OMPC_contains: {
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
  SgPragmaDeclaration *pragma = current_OpenMPIR_to_SageIII.first;
  OpenMPDeclareSimdDirective *directive =
      dynamic_cast<OpenMPDeclareSimdDirective *>(
          current_OpenMPIR_to_SageIII.second);
  if (pragma == nullptr || directive == nullptr) {
    std::cerr << "REX_OMP_AST_INVARIANT[declare-simd-target]: invalid pragma "
                 "or directive input\n";
    ROSE_ABORT();
  }
  ResolvedOmpFunctionDirectiveTarget *target =
      requireResolvedOmpFunctionDirectiveTarget(
          pragma, ResolvedOmpFunctionDirectiveTarget::Kind::declare_simd,
          directive->getProcName(), "declare-simd-target");

  const bool function_reference_is_explicit = !directive->getProcName().empty();
  SgExpression *functionReference = nullptr;
  if (function_reference_is_explicit) {
    const OmpClauseParseCache *cache =
        getClauseParseCache(current_OpenMPIR_to_SageIII.second);
    if (cache == nullptr || cache->directive_expression_nodes.size() != 1 ||
        cache->directive_expression_nodes.front() == nullptr) {
      std::cerr << "REX_OMP_AST_INVARIANT[declare-simd-target]: explicit "
                   "procedure has no single producer-bound parsed identity\n";
      ROSE_ABORT();
    }
    SgExpression *parsedReference =
        consumeParsedExpressionNode(cache->directive_expression_nodes.front());
    if (getOmpFunctionDirectiveReferenceSymbol(parsedReference) !=
        target->symbol()) {
      std::cerr << "REX_OMP_AST_INVARIANT[declare-simd-target]: explicit "
                   "procedure identity disagrees with its exact target\n";
      ROSE_ABORT();
    }
    functionReference = parsedReference;
  } else {
    functionReference = buildOmpFunctionDirectiveReference(target->symbol());
  }

  if (functionReference == nullptr) {
    std::cerr << "REX_OMP_AST_INVARIANT[declare-simd-target]: exact symbol "
                 "cannot form a typed function reference\n";
    ROSE_ABORT();
  }
  SgOmpDeclareSimdStatement *result = new SgOmpDeclareSimdStatement(
      functionReference, function_reference_is_explicit,
      target->semanticVariantOrdinal());
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
  SgPragmaDeclaration *pragma = current_OpenMPIR_to_SageIII.first;
  OpenMPDeclareVariantDirective *directive =
      dynamic_cast<OpenMPDeclareVariantDirective *>(
          current_OpenMPIR_to_SageIII.second);
  if (pragma == nullptr || directive == nullptr) {
    std::cerr << "REX_OMP_AST_INVARIANT[declare-variant-target]: invalid "
                 "pragma or directive input\n";
    ROSE_ABORT();
  }
  ResolvedOmpFunctionDirectiveTarget *target =
      requireResolvedOmpFunctionDirectiveTarget(
          pragma, ResolvedOmpFunctionDirectiveTarget::Kind::declare_variant, {},
          "declare-variant-target");

  const OmpClauseParseCache *cache =
      getClauseParseCache(current_OpenMPIR_to_SageIII.second);
  if (cache == nullptr || cache->directive_expression_nodes.size() != 1 ||
      cache->directive_expression_nodes.front() == nullptr) {
    std::cerr << "REX_OMP_AST_INVARIANT[declare-variant]: variant function "
                 "does not have one producer-owned semantic node\n";
    ROSE_ABORT();
  }
  SgExpression *base_function_ref =
      buildOmpFunctionDirectiveReference(target->symbol());
  const bool base_function_reference_is_explicit = false;
  SgExpression *variant_function_ref =
      consumeParsedExpressionNode(cache->directive_expression_nodes.front());
  SgSymbol *variant_symbol =
      getOmpFunctionDirectiveReferenceSymbol(variant_function_ref);
  if (variant_function_ref == nullptr || variant_symbol == nullptr ||
      getOmpFunctionDirectiveSymbolDeclaration(variant_symbol) == nullptr) {
    std::cerr << "REX_OMP_AST_INVARIANT[declare-variant]: variant function "
                 "is not an exact typed function reference\n";
    ROSE_ABORT();
  }

  if (base_function_ref == nullptr) {
    std::cerr << "REX_OMP_AST_INVARIANT[declare-variant-target]: exact base "
                 "symbol cannot form a typed function reference\n";
    ROSE_ABORT();
  }

  SgOmpDeclareVariantStatement *result = new SgOmpDeclareVariantStatement(
      variant_function_ref, base_function_ref,
      base_function_reference_is_explicit, target->semanticVariantOrdinal());
  result->set_firstNondefiningDeclaration(result);

  std::vector<OpenMPClause *> *all_clauses =
      current_OpenMPIR_to_SageIII.second->getClausesInOriginalOrder();
  if (all_clauses == nullptr) {
    std::cerr << "REX_OMP_AST_INVARIANT[declare-variant]: directive has no "
                 "owned clause sequence\n";
    ROSE_ABORT();
  }
  for (OpenMPClause *clause : *all_clauses) {
    if (clause == nullptr) {
      std::cerr << "REX_OMP_AST_INVARIANT[declare-variant]: directive owns a "
                   "null clause\n";
      ROSE_ABORT();
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
  if (current_OpenMPIR_to_SageIII.first == nullptr ||
      current_OpenMPIR_to_SageIII.second == nullptr ||
      current_OpenMPIR_to_SageIII.second->getKind() !=
          OMPD_begin_declare_variant) {
    std::cerr << "REX_OMP_AST_INVARIANT[begin-declare-variant]: invalid "
                 "pragma or directive input\n";
    ROSE_ABORT();
  }
  SgOmpBeginDeclareVariantStatement *result =
      new SgOmpBeginDeclareVariantStatement();
  result->set_firstNondefiningDeclaration(result);

  std::vector<OpenMPClause *> *all_clauses =
      current_OpenMPIR_to_SageIII.second->getClausesInOriginalOrder();
  if (all_clauses == nullptr) {
    std::cerr << "REX_OMP_AST_INVARIANT[begin-declare-variant]: directive has "
                 "no owned clause sequence\n";
    ROSE_ABORT();
  }
  for (OpenMPClause *clause : *all_clauses) {
    if (clause == nullptr) {
      std::cerr << "REX_OMP_AST_INVARIANT[begin-declare-variant]: directive "
                   "owns a null clause\n";
      ROSE_ABORT();
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
  if (current_OpenMPIR_to_SageIII.first == nullptr ||
      current_OpenMPIR_to_SageIII.second == nullptr ||
      current_OpenMPIR_to_SageIII.second->getKind() !=
          OMPD_end_declare_variant) {
    std::cerr << "REX_OMP_AST_INVARIANT[end-declare-variant]: invalid pragma "
                 "or directive input\n";
    ROSE_ABORT();
  }
  SgOmpEndDeclareVariantStatement *result =
      new SgOmpEndDeclareVariantStatement();
  result->set_firstNondefiningDeclaration(result);
  return result;
}

static void
publishOpenMPNonentityDeclarationAccess(SgDeclarationStatement *declaration,
                                        const char *producer) {
  if (declaration == nullptr || producer == nullptr) {
    std::cerr << "REX_OMP_AST_INVARIANT[nonentity-access]: producer="
              << (producer != nullptr ? producer : "<null>")
              << " requires one declaration" << std::endl;
    ROSE_ABORT();
  }
  SgAccessModifier &access =
      declaration->get_declarationModifier().get_accessModifier();
  access.setNotApplicable();
  access.set_is_explicit(false);
  if (!access.isNotApplicable() || access.get_is_explicit()) {
    std::cerr << "REX_OMP_AST_INVARIANT[nonentity-access]: producer="
              << producer << " failed to publish non-entity access"
              << std::endl;
    ROSE_ABORT();
  }
}

// Convert an OpenMPIR Declare Target Directive to a ROSE node
SgStatement *convertOmpDeclareTargetDirective(
    std::pair<SgPragmaDeclaration *, OpenMPDirective *>
        current_OpenMPIR_to_SageIII) {
  SgOmpDeclareTargetStatement *result = new SgOmpDeclareTargetStatement();
  result->set_firstNondefiningDeclaration(result);
  publishOpenMPNonentityDeclarationAccess(result,
                                          "convertOmpDeclareTargetDirective");
  result->set_use_underscore_spelling(
      current_OpenMPIR_to_SageIII.second->getDeclareTargetUnderscore());
  SgOmpClause::omp_when_context_kind_enum device_type_kind =
      SgOmpClause::e_omp_when_context_kind_unknown;

  OpenMPDeclareTargetDirective *declare_target_directive = NULL;
  if (current_OpenMPIR_to_SageIII.second->getKind() == OMPD_declare_target) {
    declare_target_directive = static_cast<OpenMPDeclareTargetDirective *>(
        current_OpenMPIR_to_SageIII.second);
  }
  if (declare_target_directive != NULL) {
    const std::vector<ompparser::HostFragment> &extended_list =
        declare_target_directive->getExtendedList();
    if (!extended_list.empty()) {
      const OmpClauseParseCache *cache =
          getClauseParseCache(current_OpenMPIR_to_SageIII.second);
      if (cache == nullptr ||
          cache->directive_expression_nodes.size() != extended_list.size()) {
        std::cerr << "REX_OMP_AST_INVARIANT[declare-target-extended-list]: "
                     "directive variable cache does not match the semantic "
                     "list\n";
        ROSE_ABORT();
      }

      clearOpenMPClauseTemporaryState();
      for (const OmpParsedExpression *parsed :
           cache->directive_expression_nodes) {
        appendParsedVariableNode(parsed);
      }

      SgExprListExp *explist = buildExprListExp();
      SgOmpToClause *extended_to_clause =
          new SgOmpToClause(explist, SgOmpClause::e_omp_to_kind_unknown);
      buildVariableList(extended_to_clause);
      explist->set_parent(extended_to_clause);
      extended_to_clause->set_declare_target_extended_list(true);
      setOneSourcePositionForTransformation(extended_to_clause);
      result->get_clauses().push_back(extended_to_clause);
      extended_to_clause->set_parent(result);
      clearOpenMPClauseTemporaryState();
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
    case OMPC_link:
    case OMPC_enter:
    case OMPC_local:
      convertClause(isSgStatement(result), current_OpenMPIR_to_SageIII,
                    *clause_iter);
      break;
    case OMPC_indirect:
      convertSimpleClause(isSgStatement(result), current_OpenMPIR_to_SageIII,
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
  publishOpenMPNonentityDeclarationAccess(
      result, "convertOmpEndDeclareTargetDirective");
  result->set_use_underscore_spelling(
      current_OpenMPIR_to_SageIII.second->getDeclareTargetUnderscore());

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
    case OMPC_dynamic_allocators:
    case OMPC_self_maps: {
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
  if (!openMPExpressionVariables().empty()) {
    std::cerr << "REX_OMP_AST_INVARIANT[flush-parser-state]: flush "
                 "conversion started with stale parser state\n";
    ROSE_ABORT();
  }
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
  const std::vector<ompparser::HostFragment> &current_expressions =
      current_ir->getFlushList();
  const OmpClauseParseCache *cache =
      getClauseParseCache(current_OpenMPIR_to_SageIII.second);
  if (cache == nullptr ||
      cache->directive_expression_nodes.size() != current_expressions.size()) {
    std::cerr << "REX_OMP_AST_INVARIANT[flush-variable]: directive variable "
                 "cache does not match the semantic list\n";
    ROSE_ABORT();
  }

  for (const OmpParsedExpression *parsed : cache->directive_expression_nodes) {
    if (SgExpression *expr = consumeParsedExpressionNode(parsed)) {
      statement->append_variable(expr);
    } else {
      std::cerr << "REX_OMP_AST_INVARIANT[flush-variable]: cached variable "
                   "has no expression\n";
      ROSE_ABORT();
    }
  }
  clearOpenMPClauseTemporaryState();
  return statement;
}

// Convert an OpenMPIR Allocate Directive to a ROSE node
SgStatement *
convertOmpAllocateDirective(std::pair<SgPragmaDeclaration *, OpenMPDirective *>
                                current_OpenMPIR_to_SageIII) {
  if (!openMPExpressionVariables().empty()) {
    std::cerr << "REX_OMP_AST_INVARIANT[allocate-parser-state]: allocate "
                 "conversion started with stale parser state\n";
    ROSE_ABORT();
  }
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
    case OMPC_align: {
      convertExpressionClause(isSgStatement(statement),
                              current_OpenMPIR_to_SageIII, *clause_iter);
      break;
    }
    default: {
      convertClause(isSgStatement(statement), current_OpenMPIR_to_SageIII,
                    *clause_iter);
    }
    };
  };
  const std::vector<ompparser::HostFragment> &current_expressions =
      current_ir->getAllocateList();
  const OmpClauseParseCache *cache =
      getClauseParseCache(current_OpenMPIR_to_SageIII.second);
  if (cache == nullptr ||
      cache->directive_expression_nodes.size() != current_expressions.size()) {
    std::cerr << "REX_OMP_AST_INVARIANT[allocate-variable]: directive "
                 "variable cache does not match the semantic list\n";
    ROSE_ABORT();
  }
  for (const OmpParsedExpression *parsed : cache->directive_expression_nodes) {
    if (SgExpression *expr = consumeParsedExpressionNode(parsed)) {
      statement->append_variable(expr);
    } else {
      std::cerr << "REX_OMP_AST_INVARIANT[allocate-variable]: cached variable "
                   "has no expression\n";
      ROSE_ABORT();
    }
  }
  clearOpenMPClauseTemporaryState();
  return statement;
}

// Convert an OpenMPIR Threadprivate Directive to a ROSE node
// Because we have to do some non-standard things, I'm putting this in a
// separate function
static std::string
requireExactFortranThreadprivateSourceName(const std::string &sourceExpression,
                                           const std::string &semanticName,
                                           bool commonBlock) {
  std::string source = trimWhitespaceCopy(sourceExpression);
  if (commonBlock) {
    if (source.size() < 3 || source.front() != '/' || source.back() != '/') {
      std::cerr << "REX_OMP_AST_INVARIANT[threadprivate-source]: semantic "
                   "common block has no exact source designator\n";
      ROSE_ABORT();
    }
    source = trimWhitespaceCopy(source.substr(1, source.size() - 2));
  }
  if (!SageInterface::isValidFortranSourceIdentifier(source) ||
      !SageInterface::isValidFortranSourceIdentifier(semanticName) ||
      toLowerCopy(source) != toLowerCopy(semanticName)) {
    std::cerr << "REX_OMP_AST_INVARIANT[threadprivate-source]: exact source "
                 "name '"
              << source << "' does not identify semantic name '" << semanticName
              << "'\n";
    ROSE_ABORT();
  }
  return source;
}

SgStatement *convertOmpThreadprivateStatement(
    std::pair<SgPragmaDeclaration *, OpenMPDirective *>
        current_OpenMPIR_to_SageIII) {
  if (!openMPExpressionVariables().empty()) {
    std::cerr << "REX_OMP_AST_INVARIANT[threadprivate-parser-state]: "
                 "threadprivate conversion started with stale parser state\n";
    ROSE_ABORT();
  }
  SgOmpThreadprivateStatement *statement = new SgOmpThreadprivateStatement();
  OpenMPThreadprivateDirective *current_ir =
      static_cast<OpenMPThreadprivateDirective *>(
          current_OpenMPIR_to_SageIII.second);

  const std::vector<ompparser::HostFragment> &current_expressions =
      current_ir->getThreadprivateList();
  const OmpClauseParseCache *cache =
      getClauseParseCache(current_OpenMPIR_to_SageIII.second);
  if (cache == nullptr ||
      cache->directive_expression_nodes.size() != current_expressions.size()) {
    std::cerr << "REX_OMP_AST_INVARIANT[threadprivate-variable]: directive "
                 "variable cache does not match the semantic list\n";
    ROSE_ABORT();
  }
  const bool isFortran = current_ir->getBaseLang() == Lang_Fortran;
  if (isFortran && cache->threadprivate_source_expression_texts.size() !=
                       cache->directive_expression_nodes.size()) {
    std::cerr << "REX_OMP_AST_INVARIANT[threadprivate-source]: semantic and "
                 "exact source variable lists have different sizes\n";
    ROSE_ABORT();
  }
  for (size_t index = 0; index < cache->directive_expression_nodes.size();
       ++index) {
    const OmpParsedExpression *parsed =
        cache->directive_expression_nodes[index];
    if (SgExpression *expr = consumeParsedExpressionNode(parsed)) {
      if (isFortran) {
        const std::string &sourceExpression =
            cache->threadprivate_source_expression_texts[index];
        if (SgFortranCommonBlockRefExp *common =
                isSgFortranCommonBlockRefExp(expr)) {
          const std::string exactName =
              requireExactFortranThreadprivateSourceName(
                  sourceExpression, common->get_use_name().getString(),
                  /*commonBlock=*/true);
          common->set_use_name(SgName(exactName));
          SageInterface::validateFortranCommonBlockRef(common);
        } else {
          const std::string exactName =
              requireExactFortranThreadprivateSourceName(sourceExpression,
                                                         parsed->text,
                                                         /*commonBlock=*/false);
          if (exactName != parsed->text) {
            if (expr->get_originalExpressionTree() != nullptr) {
              std::cerr
                  << "REX_OMP_AST_INVARIANT[threadprivate-source]: semantic "
                     "variable already owns source provenance\n";
              ROSE_ABORT();
            }
            SgOmpSourceExpression *source =
                buildOpenMPSourceExpression(exactName);
            expr->set_originalExpressionTree(source);
            source->set_parent(expr);
          }
        }
      }
      statement->get_variables().push_back(expr);
      expr->set_parent(statement);
    } else {
      cerr << "REX_OMP_AST_INVARIANT[threadprivate-variable]: unhandled "
              "threadprivate variable-list node\n";
      ROSE_ABORT();
    }
  }

  statement->set_definingDeclaration(statement);
  openMPExpressionVariables().clear();
  return statement;
}

SgOmpDepobjUpdateClause *
convertDepobjUpdateClause(SgStatement *directive,
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
  addOmpClause(directive, sg_clause);

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
  case OMPC_ATOMIC_DEFAULT_MEM_ORDER_acquire: {
    sg_dv = SgOmpClause::e_omp_atomic_default_mem_order_kind_acquire;
    break;
  }
  case OMPC_ATOMIC_DEFAULT_MEM_ORDER_release: {
    sg_dv = SgOmpClause::e_omp_atomic_default_mem_order_kind_release;
    break;
  }
  case OMPC_ATOMIC_DEFAULT_MEM_ORDER_relaxed: {
    sg_dv = SgOmpClause::e_omp_atomic_default_mem_order_kind_relaxed;
    break;
  }
  default: {
    std::cerr << "REX_OMP_AST_INVARIANT[atomic-default-memory-order]: "
                 "unsupported OpenMP IR kind "
              << static_cast<int>(atomic_default_mem_order_kind) << '\n';
    ROSE_ABORT();
  }
  }; // end switch
  SgOmpAtomicDefaultMemOrderClause *result =
      new SgOmpAtomicDefaultMemOrderClause(sg_dv);
  setOneSourcePositionForTransformation(result);
  addOmpClause(directive, result);
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
      buildOpenMPNameExpression(requirement_text);
  SgOmpExtImplementationDefinedRequirementClause *result =
      new SgOmpExtImplementationDefinedRequirementClause(
          ext_implementation_defined_requirement);
  setOneSourcePositionForTransformation(result);
  addOmpClause(directive, result);
  return result;
}

SgOmpScheduleClause *
convertScheduleClause(SgStatement *directive,
                      std::pair<SgPragmaDeclaration *, OpenMPDirective *>
                          current_OpenMPIR_to_SageIII,
                      OpenMPClause *current_omp_clause) {

  const std::vector<const OmpParsedExpression *> *parsed_nodes =
      getParsedClauseExpressionNodes(current_OpenMPIR_to_SageIII.second,
                                     current_omp_clause);
  const std::vector<OpenMPExpressionItem> &current_expressions =
      current_omp_clause->getExpressionItems();
  if (!current_expressions.empty() &&
      (parsed_nodes == nullptr || parsed_nodes->empty())) {
    std::cerr << "REX_OMP_AST_INVARIANT[schedule]: expressions have no cached "
                 "semantic nodes\n";
    ROSE_ABORT();
  }
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
  size_t parsed_node_index = 0;
  if ((((OpenMPScheduleClause *)current_omp_clause)->getChunkSize()) != "") {
    chunk_size = parseClauseExpressionWithCache(
        current_omp_clause->getKind(), parsed_nodes, parsed_node_index,
        ((OpenMPScheduleClause *)current_omp_clause)->getChunkSize());
  }
  requireParsedClauseExpressionsConsumed(current_omp_clause->getKind(),
                                         parsed_nodes, parsed_node_index);

  SgOmpScheduleClause *result =
      new SgOmpScheduleClause(sg_modifier1, sg_modifier2, sg_kind, chunk_size);
  ROSE_ASSERT(result);
  if (chunk_size != NULL) {
    chunk_size->set_parent(result);
    attachOriginalOpenMPExpressionSpelling(current_OpenMPIR_to_SageIII.second,
                                           current_omp_clause, parsed_nodes,
                                           chunk_size);
  }
  setOneSourcePositionForTransformation(result);
  addOmpClause(directive, result);
  return result;
}

SgOmpDistScheduleClause *
convertDistScheduleClause(SgOmpClauseBodyStatement *clause_body,
                          std::pair<SgPragmaDeclaration *, OpenMPDirective *>
                              current_OpenMPIR_to_SageIII,
                          OpenMPClause *current_omp_clause) {

  const std::vector<const OmpParsedExpression *> *parsed_nodes =
      getParsedClauseExpressionNodes(current_OpenMPIR_to_SageIII.second,
                                     current_omp_clause);

  OpenMPDistScheduleClauseKind kind =
      ((OpenMPDistScheduleClause *)current_omp_clause)->getKind();
  SgOmpClause::omp_dist_schedule_kind_enum sg_kind =
      toSgOmpClauseDistScheduleKind(kind);

  SgExpression *chunk_size = NULL;
  size_t parsed_node_index = 0;
  if ((((OpenMPDistScheduleClause *)current_omp_clause)->getChunkSize()) !=
      "") {
    chunk_size = parseClauseExpressionWithCache(
        current_omp_clause->getKind(), parsed_nodes, parsed_node_index,
        ((OpenMPDistScheduleClause *)current_omp_clause)->getChunkSize());
  }
  requireParsedClauseExpressionsConsumed(current_omp_clause->getKind(),
                                         parsed_nodes, parsed_node_index);

  SgOmpDistScheduleClause *result =
      new SgOmpDistScheduleClause(sg_kind, chunk_size);
  ROSE_ASSERT(result);
  if (chunk_size != NULL) {
    chunk_size->set_parent(result);
    attachOriginalOpenMPExpressionSpelling(current_OpenMPIR_to_SageIII.second,
                                           current_omp_clause, parsed_nodes,
                                           chunk_size);
  }
  setOneSourcePositionForTransformation(result);
  SgOmpClause *sg_clause = result;
  addOmpClause(clause_body, sg_clause);
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
  addOmpClause(clause_body, sg_clause);
  return result;
}

SgOmpUsesAllocatorsClause *
convertUsesAllocatorsClause(SgOmpClauseBodyStatement *clause_body,
                            std::pair<SgPragmaDeclaration *, OpenMPDirective *>
                                current_OpenMPIR_to_SageIII,
                            OpenMPClause *current_omp_clause) {

  SgOmpUsesAllocatorsClause *result = NULL;
  SgOmpUsesAllocatorsDefination *uses_allocators_defination = NULL;
  SgOmpClause::omp_uses_allocators_allocator_enum sg_allocator;
  std::vector<usesAllocatorParameter *> *uses_allocators =
      ((OpenMPUsesAllocatorsClause *)current_omp_clause)
          ->getUsesAllocatorsAllocatorSequence();
  if (uses_allocators == nullptr || uses_allocators->empty()) {
    std::cerr << "REX_OMP_AST_INVARIANT[uses-allocators]: clause requires a "
                 "non-empty allocator sequence\n";
    ROSE_ABORT();
  }
  std::vector<usesAllocatorParameter *>::iterator iter;
  SgOmpUsesAllocatorsDefinationPtrList uses_allocators_definations;
  const std::vector<const OmpParsedExpression *> *parsed_nodes =
      getParsedClauseAuxiliaryExpressionNodes(
          current_OpenMPIR_to_SageIII.second, current_omp_clause);
  size_t parsed_node_index = 0;
  for (iter = uses_allocators->begin(); iter != uses_allocators->end();
       iter++) {
    if (*iter == nullptr) {
      std::cerr << "REX_OMP_AST_INVARIANT[uses-allocators]: allocator "
                   "sequence contains a null entry\n";
      ROSE_ABORT();
    }
    OpenMPUsesAllocatorsClauseAllocator allocator =
        ((usesAllocatorParameter *)(*iter))->getUsesAllocatorsAllocator();
    sg_allocator = toSgOmpClauseUsesAllocatorsAllocator(allocator);
    const std::string allocator_user =
        ((usesAllocatorParameter *)(*iter))->getAllocatorUser();
    const std::string allocator_traits =
        ((usesAllocatorParameter *)(*iter))->getAllocatorTraitsArray();
    const bool is_user =
        sg_allocator ==
        SgOmpClause::e_omp_uses_allocators_allocator_user_defined;
    const bool is_traits =
        sg_allocator == SgOmpClause::e_omp_uses_allocators_allocator_traits;
    if ((is_user && allocator_user.empty()) ||
        (is_traits && (allocator_user.empty() || allocator_traits.empty())) ||
        (!is_user && !is_traits && !allocator_user.empty())) {
      std::cerr << "REX_OMP_AST_INVARIANT[uses-allocators]: allocator kind "
                   "and expression payloads disagree\n";
      ROSE_ABORT();
    }
    SgExpression *allocator_traits_array = NULL;
    if (!allocator_traits.empty()) {
      allocator_traits_array = parseClauseExpressionWithCache(
          current_omp_clause->getKind(), parsed_nodes, parsed_node_index,
          allocator_traits);
      if (allocator_traits_array == nullptr ||
          allocator_traits_array->get_parent() != nullptr) {
        std::cerr << "REX_OMP_AST_INVARIANT[uses-allocators-owner]: traits "
                     "expression is null or already owned\n";
        ROSE_ABORT();
      }
    }

    SgExpression *user_defined_allocator = NULL;
    if (is_user || is_traits) {
      user_defined_allocator = consumeParsedClauseExpression(
          current_omp_clause->getKind(), parsed_nodes, parsed_node_index,
          allocator_user, OMP_EXPR_PARSE_variable_list);
      if (user_defined_allocator == nullptr ||
          user_defined_allocator->get_parent() != nullptr) {
        std::cerr << "REX_OMP_AST_INVARIANT[uses-allocators-owner]: allocator "
                     "expression is null or already owned\n";
        ROSE_ABORT();
      }
    }

    uses_allocators_defination = new SgOmpUsesAllocatorsDefination();
    uses_allocators_defination->set_allocator_traits_array(
        allocator_traits_array);
    if (allocator_traits_array != nullptr) {
      allocator_traits_array->set_parent(uses_allocators_defination);
    }
    uses_allocators_defination->set_allocator(sg_allocator);

    uses_allocators_defination->set_user_defined_allocator(
        user_defined_allocator);
    if (user_defined_allocator != nullptr) {
      user_defined_allocator->set_parent(uses_allocators_defination);
    }
    setOneSourcePositionForTransformation(uses_allocators_defination);
    uses_allocators_definations.push_back(uses_allocators_defination);
  }
  requireParsedClauseExpressionsConsumed(current_omp_clause->getKind(),
                                         parsed_nodes, parsed_node_index);

  result = new SgOmpUsesAllocatorsClause();

  ROSE_ASSERT(result != NULL);
  result->get_uses_allocators_defination() = uses_allocators_definations;
  for (SgOmpUsesAllocatorsDefination *definition :
       result->get_uses_allocators_defination()) {
    ROSE_ASSERT(definition != nullptr);
    definition->set_parent(result);
  }
  setOneSourcePositionForTransformation(result);
  SgOmpClause *sg_clause = result;
  addOmpClause(clause_body, sg_clause);
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
  SgOmpNameExpression *mapper_identifier = NULL;

  const SgOmpClause::omp_map_modifier_enum modifiers[] = {
      sg_modifier1, sg_modifier2, sg_modifier3};
  bool saw_unspecified_modifier = false;
  int mapper_modifier_count = 0;
  int iterator_modifier_count = 0;
  std::set<int> unique_modifiers;
  for (SgOmpClause::omp_map_modifier_enum modifier : modifiers) {
    if (modifier == SgOmpClause::e_omp_map_modifier_unspecified) {
      saw_unspecified_modifier = true;
      continue;
    }
    if (saw_unspecified_modifier ||
        !unique_modifiers.insert(static_cast<int>(modifier)).second) {
      std::cerr << "REX_OMP_AST_INVARIANT[map-modifiers]: map modifiers are "
                   "not a unique contiguous typed sequence\n";
      ROSE_ABORT();
    }
    mapper_modifier_count +=
        modifier == SgOmpClause::e_omp_map_modifier_mapper ? 1 : 0;
    iterator_modifier_count +=
        modifier == SgOmpClause::e_omp_map_modifier_iterator ? 1 : 0;
  }
  if (mapper_modifier_count > 1 || iterator_modifier_count > 1) {
    std::cerr << "REX_OMP_AST_INVARIANT[map-modifiers]: mapper and iterator "
                 "modifiers may each occur at most once\n";
    ROSE_ABORT();
  }
  const bool has_mapper_modifier = mapper_modifier_count == 1;
  const bool has_iterator_modifier = iterator_modifier_count == 1;
  if (has_mapper_modifier != !map_clause->getMapperIdentifier().empty() ||
      has_iterator_modifier != !map_clause->getIterators().empty()) {
    std::cerr << "REX_OMP_AST_INVARIANT[map-payload]: map modifier and typed "
                 "mapper/iterator payloads disagree\n";
    ROSE_ABORT();
  }

  std::vector<SgOmpMapDistDataPolicyPtrList> map_item_policies;

  openMPExpressionVariables().clear();
  const std::vector<const OmpParsedExpression *> *parsed_nodes =
      getParsedClauseExpressionNodes(current_OpenMPIR_to_SageIII.second,
                                     current_omp_clause);
  const std::vector<OpenMPExpressionItem> &current_expressions =
      current_omp_clause->getExpressionItems();
  if (!current_expressions.empty() &&
      (parsed_nodes == nullptr || parsed_nodes->empty())) {
    std::cerr << "REX_OMP_AST_INVARIANT[map]: clause expressions have no "
                 "cached semantic nodes\n";
    ROSE_ABORT();
  }
  if (parsed_nodes != nullptr && !parsed_nodes->empty()) {
    for (const OmpParsedExpression *parsed : *parsed_nodes) {
      requireCachedParsedExpression(parsed);
      if (parsed->mode == OMP_EXPR_PARSE_array_section ||
          parsed->mode == OMP_EXPR_PARSE_variable_list ||
          parsed->mode == OMP_EXPR_PARSE_expression) {
        appendParsedVariableNode(parsed);
      }
    }

    const auto &original_map_policies =
        static_cast<OpenMPMapClause *>(current_omp_clause)
            ->getDistDataPolicies();
    const auto *cached_map_policies = getParsedMapDistDataPolicies(
        current_OpenMPIR_to_SageIII.second, current_omp_clause);
    if (cached_map_policies == nullptr ||
        cached_map_policies->size() != original_map_policies.size()) {
      std::cerr << "REX_OMP_AST_INVARIANT[map]: semantic and cached "
                   "dist_data policy counts disagree\n";
      ROSE_ABORT();
    }
    const auto *policy_nodes = getParsedMapDistDataPolicyNodes(
        current_OpenMPIR_to_SageIII.second, current_omp_clause);
    if (policy_nodes == nullptr ||
        policy_nodes->size() != original_map_policies.size()) {
      std::cerr << "REX_OMP_AST_INVARIANT[map]: dist_data policy AST-node "
                   "counts disagree\n";
      ROSE_ABORT();
    }
    const size_t policy_item_count = original_map_policies.size();
    if (policy_item_count != openMPExpressionVariables().size()) {
      std::cerr << "REX_OMP_AST_INVARIANT[map-item-policy-count]: map "
                   "locators and ordered policy groups differ in size\n";
      ROSE_ABORT();
    }
    map_item_policies.resize(policy_item_count);
    for (size_t item_index = 0; item_index < policy_item_count; ++item_index) {
      const auto &policies_for_item = original_map_policies[item_index];
      const auto &cached_policies_for_item = (*cached_map_policies)[item_index];
      const auto &parsed_policy_nodes = (*policy_nodes)[item_index];
      if (cached_policies_for_item.size() != policies_for_item.size() ||
          parsed_policy_nodes.size() != policies_for_item.size()) {
        std::cerr << "REX_OMP_AST_INVARIANT[map]: per-item dist_data policy "
                     "counts disagree\n";
        ROSE_ABORT();
      }
      for (size_t policy_index = 0; policy_index < policies_for_item.size();
           ++policy_index) {
        const OpenMPMapClause::DistDataPolicy &policy =
            policies_for_item[policy_index];
        const OpenMPMapClause::DistDataPolicy &cached_policy =
            cached_policies_for_item[policy_index];
        if (cached_policy.kind != policy.kind ||
            cached_policy.argument.spelling != policy.argument.spelling) {
          std::cerr << "REX_OMP_AST_INVARIANT[map]: semantic and cached "
                       "dist_data policies disagree\n";
          ROSE_ABORT();
        }
        SgExpression *policy_expression = nullptr;
        if (!policy.argument.spelling.empty()) {
          const OmpParsedExpression *parsed_policy =
              parsed_policy_nodes[policy_index];
          if (parsed_policy == nullptr) {
            std::cerr << "REX_OMP_AST_INVARIANT[map]: dist_data policy "
                         "argument has no cached semantic node\n";
            ROSE_ABORT();
          }
          policy_expression = consumeParsedExpressionNode(parsed_policy);
        } else if (parsed_policy_nodes[policy_index] != nullptr) {
          std::cerr << "REX_OMP_AST_INVARIANT[map]: argument-less dist_data "
                       "policy unexpectedly owns a semantic node\n";
          ROSE_ABORT();
        }
        SgOmpMapDistDataPolicy *typed_policy = new SgOmpMapDistDataPolicy(
            toSgMapDistDataPolicy(policy.kind), policy_expression);
        if (policy_expression != nullptr) {
          policy_expression->set_parent(typed_policy);
        }
        setOneSourcePositionForTransformation(typed_policy);
        map_item_policies[item_index].push_back(typed_policy);
      }
    }
  }
  SgExprListExp *explist = buildExprListExp();

  result = new SgOmpMapClause(explist, sg_type);
  result->set_modifier1(sg_modifier1);
  result->set_modifier2(sg_modifier2);
  result->set_modifier3(sg_modifier3);

  if (!map_clause->getMapperIdentifier().empty()) {
    if (parsed_nodes == nullptr ||
        parsed_nodes->size() != current_expressions.size() + 1) {
      std::cerr << "REX_OMP_AST_INVARIANT[map-mapper-identifier]: map "
                   "locators and exact mapper token have invalid "
                   "cardinality\n";
      ROSE_ABORT();
    }
    mapper_identifier = parseMapperIdentifierExpression(
        current_omp_clause->getKind(),
        (*parsed_nodes)[current_expressions.size()],
        map_clause->getMapperIdentifier());
  } else if (parsed_nodes == nullptr ||
             parsed_nodes->size() != current_expressions.size()) {
    std::cerr << "REX_OMP_AST_INVARIANT[map-mapper-identifier]: mapper-free "
                 "map cache cardinality disagrees with its locators\n";
    ROSE_ABORT();
  }
  result->set_mapper_identifier(mapper_identifier);
  if (mapper_identifier != nullptr) {
    mapper_identifier->set_parent(result);
  }

  ROSE_ASSERT(result != NULL);
  if (openMPExpressionVariables().size() != map_item_policies.size()) {
    std::cerr << "REX_OMP_AST_INVARIANT[map-item-count]: map locators and "
                 "typed policy groups differ in size\n";
    ROSE_ABORT();
  }
  for (size_t item_index = 0; item_index < openMPExpressionVariables().size();
       ++item_index) {
    SgExpression *locator =
        cloneOmpVarExprFromNode(openMPExpressionVariables()[item_index]);
    if (locator == nullptr || locator->get_parent() != nullptr) {
      std::cerr << "REX_OMP_AST_INVARIANT[map-item-owner]: map locator is "
                   "null or already owned\n";
      ROSE_ABORT();
    }
    SgOmpMapItem *item = new SgOmpMapItem(locator);
    locator->set_parent(item);
    item->get_policies() = std::move(map_item_policies[item_index]);
    for (SgOmpMapDistDataPolicy *policy : item->get_policies()) {
      if (policy == nullptr || policy->get_parent() != nullptr) {
        std::cerr << "REX_OMP_AST_INVARIANT[map-item-policy-owner]: map "
                     "policy is null or already owned\n";
        ROSE_ABORT();
      }
      policy->set_parent(item);
    }
    setOneSourcePositionForTransformation(item);
    explist->append_expression(item);
  }
  explist->set_parent(result);
  attachOriginalOpenMPVariableSpelling(current_OpenMPIR_to_SageIII.second,
                                       current_omp_clause, parsed_nodes,
                                       result);
  result->get_iterator_definitions() = buildClauseIteratorDefinitions(
      getParsedClauseAuxiliaryExpressionNodes(
          current_OpenMPIR_to_SageIII.second, current_omp_clause),
      getClauseSourceAuxiliaryExpressionTexts(
          current_OpenMPIR_to_SageIII.second, current_omp_clause),
      map_clause->getIterators());
  ownClauseIteratorDefinitions(result, result->get_iterator_definitions(),
                               has_iterator_modifier);

  setOneSourcePositionForTransformation(result);
  SgOmpClause *sg_clause = result;
  addOmpClause(clause_body, sg_clause);
  openMPExpressionVariables().clear();
  return result;
}

static std::vector<SgOmpClause *>
collectCombinedOmpClauses(SgOmpBodyStatement *outer);
static void recordNewCombinedClauseOrder(
    SgOmpBodyStatement *outer, const std::vector<SgOmpClause *> &before,
    SgOmpClause *returned_clause, OpenMPClause *ir_clause,
    std::size_t ir_clause_index, std::size_t &next_source_order);
static void finalizeCombinedClauseOrder(SgOmpBodyStatement *outer,
                                        std::size_t next_source_order);

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
    isSgOmpBodyStatement(result)->set_source_form_is_combined(true);
    break;
  }
  case OMPD_parallel_for: {
    SgStatement *second_stmt = new SgOmpForStatement(NULL, NULL);
    result = new SgOmpParallelStatement(NULL, second_stmt);
    second_stmt->set_parent(result);
    isSgOmpBodyStatement(result)->set_source_form_is_combined(true);
    break;
  }
  case OMPD_parallel_for_simd: {
    SgStatement *second_stmt = new SgOmpForSimdStatement(NULL, NULL);
    result = new SgOmpParallelStatement(NULL, second_stmt);
    second_stmt->set_parent(result);
    isSgOmpBodyStatement(result)->set_source_form_is_combined(true);
    break;
  }
  case OMPD_parallel_do_simd: {
    SgStatement *second_stmt = new SgOmpForSimdStatement(NULL, NULL);
    markFortranDoDirectiveSpelling(second_stmt);
    result = new SgOmpParallelStatement(NULL, second_stmt);
    second_stmt->set_parent(result);
    isSgOmpBodyStatement(result)->set_source_form_is_combined(true);
    break;
  }
  case OMPD_parallel_sections: {
    SgStatement *second_stmt = new SgOmpSectionsStatement(NULL, NULL);
    result = new SgOmpParallelStatement(NULL, second_stmt);
    second_stmt->set_parent(result);
    isSgOmpBodyStatement(result)->set_source_form_is_combined(true);
    break;
  }
  case OMPD_parallel_workshare: {
    SgStatement *second_stmt = new SgOmpWorkshareStatement(NULL, NULL);
    result = new SgOmpParallelStatement(NULL, second_stmt);
    second_stmt->set_parent(result);
    isSgOmpBodyStatement(result)->set_source_form_is_combined(true);
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
    result = new SgOmpMaskedStatement(NULL, NULL);
    break;
  }
  case OMPD_nothing: {
    result = new SgOmpNothingStatement();
    break;
  }
  case OMPD_distribute: {
    result = new SgOmpDistributeStatement(NULL, NULL);
    break;
  }
  case OMPD_workdistribute: {
    result = new SgOmpWorkdistributeStatement(NULL, NULL);
    break;
  }
  case OMPD_loop: {
    result = new SgOmpLoopStatement(NULL, NULL);
    break;
  }
  case OMPD_scan: {
    result = new SgOmpScanStatement();
    break;
  }
  case OMPD_taskloop: {
    result = new SgOmpTaskloopStatement(NULL, NULL);
    break;
  }
  case OMPD_target_enter_data: {
    result = new SgOmpTargetEnterDataStatement();
    break;
  }
  case OMPD_target_exit_data: {
    result = new SgOmpTargetExitDataStatement();
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
    const std::string name =
        consumeCriticalDirectiveName(current_OpenMPIR_to_SageIII.second);
    result = new SgOmpCriticalStatement(NULL, NULL, SgName(name));
    break;
  }
  case OMPD_depobj: {
    result = buildOmpDepobjStatement(current_OpenMPIR_to_SageIII);
    break;
  }
  case OMPD_metadirective:
  case OMPD_begin_metadirective: {
    result = new SgOmpMetadirectiveStatement(NULL, NULL);
    isSgOmpMetadirectiveStatement(result)->set_source_form_is_begin(
        directive_kind == OMPD_begin_metadirective);
    break;
  }
  case OMPD_target_parallel_for: {
    result = new SgOmpTargetParallelForStatement(NULL, NULL);
    break;
  }
  case OMPD_target_parallel_do: {
    result = new SgOmpTargetParallelForStatement(NULL, NULL);
    markFortranDoDirectiveSpelling(result);
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
  case OMPD_distribute_parallel_do: {
    result = new SgOmpDistributeParallelForStatement(NULL, NULL);
    markFortranDoDirectiveSpelling(result);
    break;
  }
  case OMPD_distribute_parallel_for_simd: {
    result = new SgOmpDistributeParallelForSimdStatement(NULL, NULL);
    break;
  }
  case OMPD_distribute_parallel_do_simd: {
    result = new SgOmpDistributeParallelForSimdStatement(NULL, NULL);
    markFortranDoDirectiveSpelling(result);
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
  case OMPD_target_parallel_do_simd: {
    result = new SgOmpTargetParallelForSimdStatement(NULL, NULL);
    markFortranDoDirectiveSpelling(result);
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
  case OMPD_target_teams_workdistribute: {
    result = new SgOmpTargetTeamsWorkdistributeStatement(NULL, NULL);
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
  case OMPD_target_teams_distribute_parallel_do: {
    result = new SgOmpTargetTeamsDistributeParallelForStatement(NULL, NULL);
    markFortranDoDirectiveSpelling(result);
    break;
  }
  case OMPD_target_teams_distribute_parallel_for_simd: {
    result = new SgOmpTargetTeamsDistributeParallelForSimdStatement(NULL, NULL);
    break;
  }
  case OMPD_target_teams_distribute_parallel_do_simd: {
    result = new SgOmpTargetTeamsDistributeParallelForSimdStatement(NULL, NULL);
    markFortranDoDirectiveSpelling(result);
    break;
  }
  case OMPD_master_taskloop_simd: {
    result = new SgOmpMasterTaskloopSimdStatement(NULL, NULL);
    break;
  }
  case OMPD_masked_taskloop_simd: {
    result = new SgOmpMaskedTaskloopSimdStatement(NULL, NULL);
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
  case OMPD_teams_distribute_parallel_do: {
    result = new SgOmpTeamsDistributeParallelForStatement(NULL, NULL);
    markFortranDoDirectiveSpelling(result);
    break;
  }
  case OMPD_teams_distribute_parallel_for_simd: {
    result = new SgOmpTeamsDistributeParallelForSimdStatement(NULL, NULL);
    break;
  }
  case OMPD_teams_distribute_parallel_do_simd: {
    result = new SgOmpTeamsDistributeParallelForSimdStatement(NULL, NULL);
    markFortranDoDirectiveSpelling(result);
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
    result = new SgOmpMaskedTaskloopStatement(NULL, NULL);
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
    std::cerr << "REX_OMP_AST_INVARIANT[variant-directive]: unsupported "
                 "directive kind "
              << static_cast<int>(directive_kind) << "\n";
    ROSE_ABORT();
  }
  }
  // body->set_parent(result);
  //  extract all the clauses based on the vector of clauses in the original
  //  order
  std::vector<OpenMPClause *> *all_clauses =
      current_OpenMPIR_to_SageIII.second->getClausesInOriginalOrder();
  if (all_clauses == nullptr) {
    std::cerr << "REX_OMP_AST_INVARIANT[variant-directive]: null clause "
                 "sequence\n";
    ROSE_ABORT();
  }
  SgOmpBodyStatement *variant_body = isSgOmpBodyStatement(result);
  const bool is_combined_variant =
      variant_body != nullptr && variant_body->get_source_form_is_combined();
  std::size_t next_source_order = 0;
  std::size_t ir_clause_index = 0;
  std::vector<OpenMPClause *>::iterator clause_iter;
  for (clause_iter = all_clauses->begin(); clause_iter != all_clauses->end();
       clause_iter++) {
    if (*clause_iter == nullptr) {
      std::cerr << "REX_OMP_AST_INVARIANT[variant-directive]: null IR clause\n";
      ROSE_ABORT();
    }
    const std::vector<SgOmpClause *> clauses_before_conversion =
        is_combined_variant ? collectCombinedOmpClauses(variant_body)
                            : std::vector<SgOmpClause *>();
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
    case OMPC_filter:
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
      convertMapClause(result, current_OpenMPIR_to_SageIII, *clause_iter);
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
      convertDependClause(result, current_OpenMPIR_to_SageIII, *clause_iter);
      break;
    }
    case OMPC_affinity: {
      convertAffinityClause(result, current_OpenMPIR_to_SageIII, *clause_iter);
      break;
    }
    case OMPC_depobj_update: {
      convertDepobjUpdateClause(result, current_OpenMPIR_to_SageIII,
                                *clause_iter);
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
    if (is_combined_variant) {
      recordNewCombinedClauseOrder(variant_body, clauses_before_conversion,
                                   nullptr, *clause_iter, ir_clause_index,
                                   next_source_order);
    }
    ++ir_clause_index;
  };

  if (is_combined_variant) {
    finalizeCombinedClauseOrder(variant_body, next_source_order);
  }

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
  if (current_is_fortran) {
    result =
        getNextStatementInSameBasicBlock(current_OpenMPIR_to_SageIII.first);
  } else {
    result = getNextStatement(current_OpenMPIR_to_SageIII.first);
  }
  while (SgPragmaDeclaration *next_pragma = isSgPragmaDeclaration(result)) {
    auto mapped = openMPFortranPairedPragmas().find(next_pragma);
    if (mapped != openMPFortranPairedPragmas().end() &&
        mapped->second != NULL && mapped->second->getKind() == OMPD_end) {
      return NULL;
    }
    // For C/C++, a pragma between a body directive and its statement means we
    // cannot safely identify the structured block; keep the original pragma.
    if (!current_is_fortran) {
      return NULL;
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
  addOmpClause(clause_body, sg_clause);

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
  if (sg_modifier == SgOmpClause::e_omp_allocator_modifier_unknown) {
    std::cerr << "REX_OMP_AST_INVARIANT[allocator]: allocator clause has no "
                 "typed allocator payload\n";
    ROSE_ABORT();
  }
  SgExpression *user_defined_parameter = NULL;
  SgGlobal *global =
      SageInterface::getGlobalScope(current_OpenMPIR_to_SageIII.first);
  const std::vector<const OmpParsedExpression *> *parsed_nodes =
      getParsedClauseAuxiliaryExpressionNodes(
          current_OpenMPIR_to_SageIII.second, current_omp_clause);
  size_t parsed_node_index = 0;
  if (sg_modifier == SgOmpClause::e_omp_allocator_user_defined_modifier) {
    if (((OpenMPAllocatorClause *)current_omp_clause)
            ->getUserDefinedAllocator()
            .empty()) {
      std::cerr << "REX_OMP_AST_INVARIANT[allocator]: user-defined allocator "
                   "has no expression text\n";
      ROSE_ABORT();
    }
    SgExpression *clause_expression = parseClauseExpressionWithCache(
        current_omp_clause->getKind(), parsed_nodes, parsed_node_index,
        ((OpenMPAllocatorClause *)current_omp_clause)
            ->getUserDefinedAllocator());
    user_defined_parameter =
        checkOmpExpressionClause(clause_expression, global, e_allocate);
    if (user_defined_parameter == nullptr ||
        user_defined_parameter->get_parent() != nullptr) {
      std::cerr << "REX_OMP_AST_INVARIANT[allocator-owner]: user-defined "
                   "allocator expression is null or already owned\n";
      ROSE_ABORT();
    }
  } else if (!((OpenMPAllocatorClause *)current_omp_clause)
                  ->getUserDefinedAllocator()
                  .empty()) {
    std::cerr << "REX_OMP_AST_INVARIANT[allocator]: predefined allocator "
                 "unexpectedly carries a user expression\n";
    ROSE_ABORT();
  }
  requireParsedClauseExpressionsConsumed(current_omp_clause->getKind(),
                                         parsed_nodes, parsed_node_index);
  SgOmpAllocatorClause *result =
      new SgOmpAllocatorClause(sg_modifier, user_defined_parameter);
  if (user_defined_parameter != nullptr) {
    user_defined_parameter->set_parent(result);
  }
  setOneSourcePositionForTransformation(result);
  // reconsider the location of following code to attach clause
  SgOmpClause *sg_clause = result;
  addOmpClause(clause_body, sg_clause);

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
  addOmpClause(clause_body, result);

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
    std::cerr << "REX_OMP_AST_INVARIANT[order-kind]: unsupported OpenMP IR "
                 "kind "
              << static_cast<int>(order_kind) << '\n';
    ROSE_ABORT();
  }
  }; // end switch
  SgOmpOrderClause *result = new SgOmpOrderClause(sg_dv, sg_modifier);
  setOneSourcePositionForTransformation(result);

  // reconsider the location of following code to attach clause
  addOmpClause(directive, result);

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
    std::cerr << "REX_OMP_AST_INVARIANT[bind-kind]: unsupported OpenMP IR "
                 "binding kind "
              << static_cast<int>(bind_binding) << '\n';
    ROSE_ABORT();
  }
  }; // end switch
  SgOmpBindClause *result = new SgOmpBindClause(sg_dv);
  setOneSourcePositionForTransformation(result);

  // reconsider the location of following code to attach clause
  SgOmpClause *sg_clause = result;
  addOmpClause(clause_body, sg_clause);

  return result;
}

namespace {
struct ConvertedVariantClauseData {
  SgOmpContextSelectorSetPtrList context_selector_sets;
};

SgOmpClause::omp_context_selector_set_kind_enum
toSageContextSelectorSetKind(OpenMPContextSelectorSequenceKind kind) {
  switch (kind) {
  case OMPC_SELECTOR_user:
    return SgOmpClause::e_omp_context_selector_set_user;
  case OMPC_SELECTOR_construct:
    return SgOmpClause::e_omp_context_selector_set_construct;
  case OMPC_SELECTOR_device:
    return SgOmpClause::e_omp_context_selector_set_device;
  case OMPC_SELECTOR_target_device:
    return SgOmpClause::e_omp_context_selector_set_target_device;
  case OMPC_SELECTOR_implementation:
    return SgOmpClause::e_omp_context_selector_set_implementation;
  }
  std::cerr << "REX_OMP_AST_INVARIANT[variant-set-kind]: unsupported set kind "
            << static_cast<int>(kind) << '\n';
  ROSE_ABORT();
}

SgOmpClause::omp_context_trait_selector_kind_enum
toSageContextTraitSelectorKind(OpenMPContextTraitSelectorKind kind) {
  switch (kind) {
  case OMPC_TRAIT_condition:
    return SgOmpClause::e_omp_context_trait_condition;
  case OMPC_TRAIT_construct:
    return SgOmpClause::e_omp_context_trait_construct;
  case OMPC_TRAIT_kind:
    return SgOmpClause::e_omp_context_trait_kind;
  case OMPC_TRAIT_arch:
    return SgOmpClause::e_omp_context_trait_arch;
  case OMPC_TRAIT_isa:
    return SgOmpClause::e_omp_context_trait_isa;
  case OMPC_TRAIT_device_num:
    return SgOmpClause::e_omp_context_trait_device_num;
  case OMPC_TRAIT_uid:
    return SgOmpClause::e_omp_context_trait_uid;
  case OMPC_TRAIT_vendor:
    return SgOmpClause::e_omp_context_trait_vendor;
  case OMPC_TRAIT_extension:
    return SgOmpClause::e_omp_context_trait_extension;
  case OMPC_TRAIT_requires:
    return SgOmpClause::e_omp_context_trait_requires;
  case OMPC_TRAIT_atomic_default_mem_order:
    return SgOmpClause::e_omp_context_trait_atomic_default_mem_order;
  case OMPC_TRAIT_implementation_user:
    return SgOmpClause::e_omp_context_trait_implementation_user;
  }
  std::cerr << "REX_OMP_AST_INVARIANT[variant-trait-kind]: unsupported trait "
               "kind "
            << static_cast<int>(kind) << '\n';
  ROSE_ABORT();
}

SgOmpClause::omp_when_context_kind_enum
toSageContextKind(OpenMPClauseContextKind kind) {
  switch (kind) {
  case OMPC_CONTEXT_KIND_host:
    return SgOmpClause::e_omp_when_context_kind_host;
  case OMPC_CONTEXT_KIND_nohost:
    return SgOmpClause::e_omp_when_context_kind_nohost;
  case OMPC_CONTEXT_KIND_any:
    return SgOmpClause::e_omp_when_context_kind_any;
  case OMPC_CONTEXT_KIND_cpu:
    return SgOmpClause::e_omp_when_context_kind_cpu;
  case OMPC_CONTEXT_KIND_gpu:
    return SgOmpClause::e_omp_when_context_kind_gpu;
  case OMPC_CONTEXT_KIND_fpga:
    return SgOmpClause::e_omp_when_context_kind_fpga;
  case OMPC_CONTEXT_KIND_unknown:
    break;
  }
  std::cerr << "REX_OMP_AST_INVARIANT[variant-device-kind]: unsupported "
               "context kind "
            << static_cast<int>(kind) << '\n';
  ROSE_ABORT();
}

SgOmpClause::omp_when_context_vendor_enum
toSageContextVendor(OpenMPClauseContextVendor vendor) {
  switch (vendor) {
  case OMPC_CONTEXT_VENDOR_amd:
    return SgOmpClause::e_omp_when_context_vendor_amd;
  case OMPC_CONTEXT_VENDOR_arm:
    return SgOmpClause::e_omp_when_context_vendor_arm;
  case OMPC_CONTEXT_VENDOR_bsc:
    return SgOmpClause::e_omp_when_context_vendor_bsc;
  case OMPC_CONTEXT_VENDOR_cray:
    return SgOmpClause::e_omp_when_context_vendor_cray;
  case OMPC_CONTEXT_VENDOR_fujitsu:
    return SgOmpClause::e_omp_when_context_vendor_fujitsu;
  case OMPC_CONTEXT_VENDOR_gnu:
    return SgOmpClause::e_omp_when_context_vendor_gnu;
  case OMPC_CONTEXT_VENDOR_ibm:
    return SgOmpClause::e_omp_when_context_vendor_ibm;
  case OMPC_CONTEXT_VENDOR_intel:
    return SgOmpClause::e_omp_when_context_vendor_intel;
  case OMPC_CONTEXT_VENDOR_llvm:
    return SgOmpClause::e_omp_when_context_vendor_llvm;
  case OMPC_CONTEXT_VENDOR_nvidia:
    return SgOmpClause::e_omp_when_context_vendor_nvidia;
  case OMPC_CONTEXT_VENDOR_pgi:
    return SgOmpClause::e_omp_when_context_vendor_pgi;
  case OMPC_CONTEXT_VENDOR_ti:
    return SgOmpClause::e_omp_when_context_vendor_ti;
  case OMPC_CONTEXT_VENDOR_user:
    return SgOmpClause::e_omp_when_context_vendor_user;
  case OMPC_CONTEXT_VENDOR_unknown:
    return SgOmpClause::e_omp_when_context_vendor_unknown;
  case OMPC_CONTEXT_VENDOR_unspecified:
    break;
  }
  std::cerr << "REX_OMP_AST_INVARIANT[variant-vendor]: unsupported vendor "
            << static_cast<int>(vendor) << '\n';
  ROSE_ABORT();
}

SgOmpClause::omp_atomic_default_mem_order_kind_enum
toSageAtomicDefaultMemOrderKind(OpenMPAtomicDefaultMemOrderClauseKind kind) {
  switch (kind) {
  case OMPC_ATOMIC_DEFAULT_MEM_ORDER_seq_cst:
    return SgOmpClause::e_omp_atomic_default_mem_order_kind_seq_cst;
  case OMPC_ATOMIC_DEFAULT_MEM_ORDER_acq_rel:
    return SgOmpClause::e_omp_atomic_default_mem_order_kind_acq_rel;
  case OMPC_ATOMIC_DEFAULT_MEM_ORDER_acquire:
    return SgOmpClause::e_omp_atomic_default_mem_order_kind_acquire;
  case OMPC_ATOMIC_DEFAULT_MEM_ORDER_release:
    return SgOmpClause::e_omp_atomic_default_mem_order_kind_release;
  case OMPC_ATOMIC_DEFAULT_MEM_ORDER_relaxed:
    return SgOmpClause::e_omp_atomic_default_mem_order_kind_relaxed;
  case OMPC_ATOMIC_DEFAULT_MEM_ORDER_unknown:
    break;
  }
  std::cerr << "REX_OMP_AST_INVARIANT[variant-atomic-default-mem-order]: "
               "unsupported property "
            << static_cast<int>(kind) << '\n';
  ROSE_ABORT();
}

SgOmpClause::omp_requires_property_kind_enum
toSageRequiresPropertyKind(OpenMPClauseKind kind) {
  switch (kind) {
  case OMPC_reverse_offload:
    return SgOmpClause::e_omp_requires_property_reverse_offload;
  case OMPC_unified_address:
    return SgOmpClause::e_omp_requires_property_unified_address;
  case OMPC_unified_shared_memory:
    return SgOmpClause::e_omp_requires_property_unified_shared_memory;
  case OMPC_dynamic_allocators:
    return SgOmpClause::e_omp_requires_property_dynamic_allocators;
  case OMPC_self_maps:
    return SgOmpClause::e_omp_requires_property_self_maps;
  case OMPC_device_safesync:
    return SgOmpClause::e_omp_requires_property_device_safesync;
  case OMPC_atomic_default_mem_order:
    return SgOmpClause::e_omp_requires_property_atomic_default_mem_order;
  case OMPC_ext_implementation_defined_requirement:
    return SgOmpClause::e_omp_requires_property_implementation_defined;
  default:
    break;
  }
  std::cerr << "REX_OMP_AST_INVARIANT[variant-requires-kind]: unsupported "
               "requires property "
            << static_cast<int>(kind) << '\n';
  ROSE_ABORT();
}

template <typename VariantClauseT>
void applyVariantClauseCommonData(VariantClauseT *result,
                                  const ConvertedVariantClauseData &data) {
  ROSE_ASSERT(result != nullptr);
  if (data.context_selector_sets.empty()) {
    std::cerr << "REX_OMP_AST_INVARIANT[variant-selector-sets]: converted "
                 "selector list is empty\n";
    ROSE_ABORT();
  }
  result->get_context_selector_sets() = data.context_selector_sets;
  for (SgOmpContextSelectorSet *set : result->get_context_selector_sets()) {
    if (set == nullptr) {
      std::cerr << "REX_OMP_AST_INVARIANT[variant-selector-sets]: null set\n";
      ROSE_ABORT();
    }
    set->set_parent(result);
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

  const std::vector<OpenMPVariantClause::TraitSetSelector> &ir_sets =
      variant_clause->getTraitSets();
  if (ir_sets.empty()) {
    std::cerr << "REX_OMP_AST_INVARIANT[variant-selector-sets]: parser IR has "
                 "no selector sets\n";
    ROSE_ABORT();
  }

  const std::vector<const OmpParsedExpression *> *auxiliary_nodes =
      getParsedClauseAuxiliaryExpressionNodes(
          current_OpenMPIR_to_SageIII.second, current_omp_clause);
  ROSE_ASSERT(auxiliary_nodes != nullptr);
  std::size_t auxiliary_index = 0;
  auto consumeAuxiliaryFragment =
      [&](const ompparser::HostFragment &fragment,
          OpenMPExprParseMode expected_mode,
          const char *invariant_name) -> SgExpression * {
    if (fragment.spelling.empty() ||
        auxiliary_index >= auxiliary_nodes->size()) {
      std::cerr << "REX_OMP_AST_INVARIANT[" << invariant_name
                << "]: selector payload has no exact cached callback node\n";
      ROSE_ABORT();
    }
    const OmpParsedExpression *parsed = requireCachedHostFragment(
        fragment, (*auxiliary_nodes)[auxiliary_index], invariant_name);
    if (parsed->node == nullptr || parsed->mode != expected_mode) {
      std::cerr << "REX_OMP_AST_INVARIANT[" << invariant_name
                << "]: selector payload and exact cached callback sequence "
                   "diverge\n";
      ROSE_ABORT();
    }
    ++auxiliary_index;
    return consumeParsedExpressionNode(parsed);
  };
  auto consumeOptionalScore =
      [&](const OpenMPVariantClause::TraitSelector &selector,
          const char *invariant_name) -> SgExpression * {
    if (selector.score.spelling.empty()) {
      if (selector.score.semantic != nullptr) {
        std::cerr << "REX_OMP_AST_INVARIANT[" << invariant_name
                  << "]: absent score owns a parser callback node\n";
        ROSE_ABORT();
      }
      return nullptr;
    }
    return consumeAuxiliaryFragment(
        selector.score, OMP_EXPR_PARSE_constant_integer, invariant_name);
  };

  for (const OpenMPVariantClause::TraitSetSelector &ir_set : ir_sets) {
    SgOmpContextSelectorSet *set =
        new SgOmpContextSelectorSet(toSageContextSelectorSetKind(ir_set.kind));
    setOneSourcePositionForTransformation(set);
    if (ir_set.selectors.empty()) {
      std::cerr << "REX_OMP_AST_INVARIANT[variant-selector-set]: parser IR "
                   "contains an empty set\n";
      ROSE_ABORT();
    }

    for (const OpenMPVariantClause::TraitSelector &ir_selector :
         ir_set.selectors) {
      SgOmpContextSelector *selector = new SgOmpContextSelector(
          toSageContextTraitSelectorKind(ir_selector.kind));
      setOneSourcePositionForTransformation(selector);
      selector->set_implementation_defined_name(
          ir_selector.implementation_defined_name);

      SgExpression *score = consumeOptionalScore(ir_selector, "variant-score");
      selector->set_score(score);
      if (score != nullptr) {
        score->set_parent(selector);
      }

      if (ir_selector.kind == OMPC_TRAIT_construct) {
        if (ir_selector.construct_directive == nullptr ||
            !ir_selector.score.spelling.empty() ||
            ir_selector.score.semantic != nullptr ||
            !ir_selector.properties.empty() ||
            !ir_selector.implementation_defined_name.empty()) {
          std::cerr << "REX_OMP_AST_INVARIANT[variant-construct]: malformed "
                       "construct selector payload\n";
          ROSE_ABORT();
        }
        std::pair<SgPragmaDeclaration *, OpenMPDirective *> paired_construct =
            make_pair(current_OpenMPIR_to_SageIII.first,
                      ir_selector.construct_directive.get());
        SgStatement *converted = convertVariantDirective(paired_construct);
        if (converted == nullptr) {
          std::cerr << "REX_OMP_AST_INVARIANT[variant-construct]: conversion "
                       "returned null\n";
          ROSE_ABORT();
        }
        selector->set_construct_directive(converted);
        converted->set_parent(selector);
      } else if (ir_selector.construct_directive != nullptr) {
        std::cerr << "REX_OMP_AST_INVARIANT[variant-construct]: non-construct "
                     "selector owns a construct directive\n";
        ROSE_ABORT();
      }

      for (const OpenMPVariantClause::TraitProperty &ir_property :
           ir_selector.properties) {
        const bool has_expression = !ir_property.fragment.spelling.empty();
        const bool has_kind = ir_property.context_kind.has_value();
        const bool has_vendor = ir_property.context_vendor.has_value();
        const bool has_atomic =
            ir_property.atomic_default_mem_order.has_value();
        const bool has_requirement = ir_property.requirement != nullptr;
        if (static_cast<int>(has_expression) + static_cast<int>(has_kind) +
                static_cast<int>(has_vendor) + static_cast<int>(has_atomic) +
                static_cast<int>(has_requirement) !=
            1) {
          std::cerr << "REX_OMP_AST_INVARIANT[variant-property]: parser IR "
                       "property does not own exactly one typed payload\n";
          ROSE_ABORT();
        }

        SgOmpContextSelectorProperty *property =
            new SgOmpContextSelectorProperty();
        setOneSourcePositionForTransformation(property);
        if (has_expression) {
          SgExpression *expression = consumeAuxiliaryFragment(
              ir_property.fragment, ir_property.fragment.parse_mode,
              "variant-property-expression");
          property->set_expression(expression);
          expression->set_parent(property);
        } else if (has_kind) {
          property->set_context_kind(
              toSageContextKind(*ir_property.context_kind));
        } else if (has_vendor) {
          property->set_context_vendor(
              toSageContextVendor(*ir_property.context_vendor));
        } else if (has_atomic) {
          property->set_atomic_default_mem_order(
              toSageAtomicDefaultMemOrderKind(
                  *ir_property.atomic_default_mem_order));
        } else {
          const OpenMPClause *requirement = ir_property.requirement.get();
          const OpenMPClauseKind requirement_kind =
              requirement->OpenMPClause::getKind();
          if (ir_selector.kind != OMPC_TRAIT_requires ||
              requirement->getDirectiveKind() != OMPD_requires ||
              !requirement->getConstructionErrors().empty()) {
            std::cerr << "REX_OMP_AST_INVARIANT[variant-requires]: malformed "
                         "typed requirement owner\n";
            ROSE_ABORT();
          }
          property->set_requires_kind(
              toSageRequiresPropertyKind(requirement_kind));
          const auto &requirement_expressions =
              requirement->getExpressionItems();
          switch (requirement_kind) {
          case OMPC_reverse_offload:
          case OMPC_unified_address:
          case OMPC_unified_shared_memory:
          case OMPC_dynamic_allocators:
          case OMPC_self_maps:
          case OMPC_device_safesync:
            if (requirement_expressions.size() > 1) {
              std::cerr
                  << "REX_OMP_AST_INVARIANT[variant-requires]: requirement "
                     "owns more than one logical expression\n";
              ROSE_ABORT();
            }
            if (!requirement_expressions.empty()) {
              const ompparser::HostFragment &fragment =
                  requirement_expressions.front().fragment;
              SgExpression *expression = consumeAuxiliaryFragment(
                  fragment, fragment.parse_mode, "variant-requires-expression");
              property->set_requires_expression(expression);
              expression->set_parent(property);
            }
            break;
          case OMPC_atomic_default_mem_order: {
            const auto *atomic =
                dynamic_cast<const OpenMPAtomicDefaultMemOrderClause *>(
                    requirement);
            if (atomic == nullptr || !requirement_expressions.empty()) {
              std::cerr << "REX_OMP_AST_INVARIANT[variant-requires]: malformed "
                           "atomic_default_mem_order requirement\n";
              ROSE_ABORT();
            }
            property->set_requires_atomic_default_mem_order(
                toSageAtomicDefaultMemOrderKind(atomic->getKind()));
            break;
          }
          case OMPC_ext_implementation_defined_requirement: {
            const auto *extension = dynamic_cast<
                const OpenMPExtImplementationDefinedRequirementClause *>(
                requirement);
            if (extension == nullptr || !requirement_expressions.empty() ||
                extension->getImplementationDefinedRequirement().empty()) {
              std::cerr << "REX_OMP_AST_INVARIANT[variant-requires]: malformed "
                           "implementation-defined requirement\n";
              ROSE_ABORT();
            }
            property->set_requires_extension(
                SgName(extension->getImplementationDefinedRequirement()));
            break;
          }
          default:
            std::cerr << "REX_OMP_AST_INVARIANT[variant-requires]: "
                         "unsupported requirement kind\n";
            ROSE_ABORT();
          }
        }
        selector->get_properties().push_back(property);
        property->set_parent(selector);
      }

      set->get_selectors().push_back(selector);
      selector->set_parent(set);
    }
    data.context_selector_sets.push_back(set);
  }

  if (auxiliary_index != auxiliary_nodes->size()) {
    std::cerr << "REX_OMP_AST_INVARIANT[variant-expression]: exact cached "
                 "callback sequence has unconsumed selector payloads\n";
    ROSE_ABORT();
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
  SgOmpWhenClause *result =
      new SgOmpWhenClause(static_cast<SgStatement *>(nullptr));
  applyVariantClauseCommonData(result, data);
  result->set_variant_directive(variant_directive);

  setOneSourcePositionForTransformation(result);
  if (variant_directive != nullptr) {
    variant_directive->set_parent(result);
  }

  SgOmpClause *sg_clause = result;
  addOmpClause(clause_body, sg_clause);

  return result;
}

SgOmpMatchClause *
convertMatchClause(SgStatement *directive,
                   std::pair<SgPragmaDeclaration *, OpenMPDirective *>
                       current_OpenMPIR_to_SageIII,
                   OpenMPClause *current_omp_clause) {
  ConvertedVariantClauseData data = buildVariantClauseCommonData(
      current_OpenMPIR_to_SageIII, current_omp_clause);
  SgOmpMatchClause *result = new SgOmpMatchClause();
  applyVariantClauseCommonData(result, data);

  setOneSourcePositionForTransformation(result);
  addOmpClause(directive, result);
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
      getParsedClauseAuxiliaryExpressionNodes(
          current_OpenMPIR_to_SageIII.second, current_omp_clause);
  size_t parsed_node_index = 0;

  SgExprListExp *arguments = buildExprListExp();
  if (adjust_clause->getArguments().empty()) {
    std::cerr << "REX_OMP_AST_INVARIANT[adjust-args]: missing parameter list\n";
    ROSE_ABORT();
  }
  for (const ompparser::HostFragment &argument :
       adjust_clause->getArguments()) {
    if (parsed_nodes == nullptr || parsed_node_index >= parsed_nodes->size() ||
        argument.spelling.empty()) {
      std::cerr << "REX_OMP_AST_INVARIANT[adjust-args]: argument fragment and "
                   "cache order diverge\n";
      ROSE_ABORT();
    }
    SgExpression *expression = consumeParsedClauseExpression(
        current_omp_clause->getKind(), parsed_nodes, parsed_node_index,
        argument.spelling, argument.parse_mode);
    ROSE_ASSERT(expression != nullptr);
    arguments->append_expression(expression);
  }

  SgOmpClause::omp_adjust_args_modifier_enum modifier =
      SgOmpClause::e_omp_adjust_args_modifier_unknown;
  switch (adjust_clause->getModifier()) {
  case OMPC_ADJUST_ARGS_need_device_addr:
    modifier = SgOmpClause::e_omp_adjust_args_modifier_need_device_addr;
    break;
  case OMPC_ADJUST_ARGS_need_device_ptr:
    modifier = SgOmpClause::e_omp_adjust_args_modifier_need_device_ptr;
    break;
  case OMPC_ADJUST_ARGS_nothing:
    modifier = SgOmpClause::e_omp_adjust_args_modifier_nothing;
    break;
  case OMPC_ADJUST_ARGS_unknown:
  default:
    std::cerr << "REX_OMP_AST_INVARIANT[adjust-args]: invalid typed modifier\n";
    ROSE_ABORT();
  }

  SgOmpAdjustArgsClause *result =
      new SgOmpAdjustArgsClause(arguments, modifier);
  arguments->set_parent(result);
  requireParsedClauseExpressionsConsumed(current_omp_clause->getKind(),
                                         parsed_nodes, parsed_node_index);

  setOneSourcePositionForTransformation(result);
  addOmpClause(directive, result);
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
      getParsedClauseAuxiliaryExpressionNodes(
          current_OpenMPIR_to_SageIII.second, current_omp_clause);
  size_t parsed_node_index = 0;

  if (append_clause->getOperations().empty()) {
    std::cerr << "REX_OMP_AST_INVARIANT[append-args]: missing operation list\n";
    ROSE_ABORT();
  }
  SgOmpAppendArgsClause *result = new SgOmpAppendArgsClause();
  for (const OpenMPAppendArgsClause::Operation &operation :
       append_clause->getOperations()) {
    if (operation.kind != OMPC_APPEND_ARGS_interop) {
      std::cerr << "REX_OMP_AST_INVARIANT[append-args]: invalid typed "
                   "operation\n";
      ROSE_ABORT();
    }
    SgOmpInitModifierList *modifiers = buildInitModifierList(
        current_omp_clause->getKind(), operation.modifiers, parsed_nodes,
        parsed_node_index, "append-args");
    SgOmpAppendArgsOperation *sage_operation =
        new SgOmpAppendArgsOperation(modifiers);
    setOneSourcePositionForTransformation(sage_operation);
    modifiers->set_parent(sage_operation);
    result->get_interop_operations().push_back(sage_operation);
    sage_operation->set_parent(result);
  }
  requireParsedClauseExpressionsConsumed(current_omp_clause->getKind(),
                                         parsed_nodes, parsed_node_index);

  setOneSourcePositionForTransformation(result);
  addOmpClause(directive, result);
  return result;
}

SgOmpSizesClause *
convertSizesClause(SgStatement *directive,
                   std::pair<SgPragmaDeclaration *, OpenMPDirective *>
                       current_OpenMPIR_to_SageIII,
                   OpenMPClause *current_omp_clause) {
  openMPExpressionVariables().clear();
  OpenMPClauseKind clause_kind = current_omp_clause->getKind();
  SgGlobal *global =
      SageInterface::getGlobalScope(current_OpenMPIR_to_SageIII.first);
  const std::vector<OpenMPExpressionItem> &current_expressions =
      current_omp_clause->getExpressionItems();
  const std::vector<const OmpParsedExpression *> *parsed_nodes =
      getParsedClauseExpressionNodes(current_OpenMPIR_to_SageIII.second,
                                     current_omp_clause);
  size_t parsed_node_index = 0;
  SgExprListExp *explist = buildExprListExp();
  if (!current_expressions.empty()) {
    for (const OpenMPExpressionItem &expression : current_expressions) {
      if (expression.fragment.spelling.empty()) {
        std::cerr << "REX_OMP_AST_INVARIANT[sizes]: empty typed operand\n";
        ROSE_ABORT();
      }
      SgExpression *exp = consumeParsedClauseExpression(
          current_omp_clause->getKind(), parsed_nodes, parsed_node_index,
          expression.fragment.spelling, expression.parse_mode);
      explist->append_expression(exp);
    }
  }
  requireParsedClauseExpressionsConsumed(current_omp_clause->getKind(),
                                         parsed_nodes, parsed_node_index);

  // SgExprListExp* explist = buildExprListExp();
  SgOmpSizesClause *result = new SgOmpSizesClause(explist);
  printf("Sizes Clause added!\n");

  setOneSourcePositionForTransformation(result);
  // buildVariableList(result);
  explist->set_parent(result);
  // reconsider the location of following code to attach clause
  addOmpClause(directive, result);
  openMPExpressionVariables().clear();
  return result;
}

static void attachOmpVariablesClauseToDirective(SgStatement *directive,
                                                OpenMPDirective *omp_directive,
                                                SgOmpVariablesClause *clause) {
  ROSE_ASSERT(directive != NULL);
  ROSE_ASSERT(omp_directive != NULL);
  ROSE_ASSERT(clause != NULL);

  addOmpClause(directive, clause);
}

SgOmpVariablesClause *
convertClause(SgStatement *directive,
              std::pair<SgPragmaDeclaration *, OpenMPDirective *>
                  current_OpenMPIR_to_SageIII,
              OpenMPClause *current_omp_clause) {
  openMPExpressionVariables().clear();
  SgOmpVariablesClause *result = NULL;
  OpenMPClauseKind clause_kind = current_omp_clause->getKind();
  SgGlobal *global =
      SageInterface::getGlobalScope(current_OpenMPIR_to_SageIII.first);
  const std::vector<const OmpParsedExpression *> *parsed_nodes =
      getParsedClauseExpressionNodes(current_OpenMPIR_to_SageIII.second,
                                     current_omp_clause);
  const std::vector<const OmpParsedExpression *> *auxiliary_nodes =
      getParsedClauseAuxiliaryExpressionNodes(
          current_OpenMPIR_to_SageIII.second, current_omp_clause);
  const std::vector<OpenMPExpressionItem> &current_expressions =
      current_omp_clause->getExpressionItems();
  if (!current_expressions.empty() &&
      (parsed_nodes == nullptr || parsed_nodes->empty())) {
    std::cerr << "REX_OMP_AST_INVARIANT[variable-clause]: expressions have "
                 "no cached semantic nodes\n";
    ROSE_ABORT();
  }
  if (parsed_nodes == nullptr ||
      parsed_nodes->size() < current_expressions.size()) {
    std::cerr << "REX_OMP_AST_INVARIANT[variable-clause]: exact cache has "
                 "fewer ordered records than semantic variable items\n";
    ROSE_ABORT();
  }
  for (size_t item_index = 0; item_index < current_expressions.size();
       ++item_index) {
    const OmpParsedExpression *parsed = (*parsed_nodes)[item_index];
    requireCachedParsedExpression(parsed);
    const OpenMPExpressionItem &expression = current_expressions[item_index];
    if (expression.fragment.spelling.empty() ||
        parsed->text != expression.fragment.spelling ||
        parsed->mode != expression.parse_mode ||
        (parsed->mode != OMP_EXPR_PARSE_variable_list &&
         parsed->mode != OMP_EXPR_PARSE_array_section)) {
      std::cerr << "REX_OMP_AST_INVARIANT[variable-clause]: ordered semantic "
                   "item cache role/text mismatch\n";
      ROSE_ABORT();
    }
  }
  if (clause_kind != OMPC_allocate) {
    requireParsedClauseExpressionsConsumed(clause_kind, parsed_nodes,
                                           current_expressions.size());
  }
  if (parsed_nodes != nullptr && !parsed_nodes->empty()) {
    for (const OmpParsedExpression *parsed : *parsed_nodes) {
      requireCachedParsedExpression(parsed);
      if (parsed->mode == OMP_EXPR_PARSE_variable_list ||
          parsed->mode == OMP_EXPR_PARSE_array_section) {
        appendParsedVariableNode(parsed);
      }
    }
  }

  SgExprListExp *explist = buildExprListExp();
  switch (clause_kind) {
  case OMPC_link: {
    result = new SgOmpLinkClause(explist);
    break;
  }
  case OMPC_enter: {
    result = new SgOmpEnterClause(explist);
    break;
  }
  case OMPC_local: {
    result = new SgOmpLocalClause(explist);
    break;
  }
  case OMPC_allocate: {
    size_t parsed_node_index = current_expressions.size();
    OpenMPAllocateClauseAllocator allocate_allocator =
        ((OpenMPAllocateClause *)current_omp_clause)->getAllocator();
    SgOmpClause::omp_allocate_modifier_enum sg_modifier =
        toSgOmpClauseAllocateAllocator(allocate_allocator);
    SgExpression *user_defined_parameter = NULL;
    if (sg_modifier == SgOmpClause::e_omp_allocate_user_defined_modifier) {
      const std::string user_defined_allocator =
          ((OpenMPAllocateClause *)current_omp_clause)
              ->getUserDefinedAllocator();
      SgExpression *clause_expression = consumeParsedClauseExpression(
          current_omp_clause->getKind(), parsed_nodes, parsed_node_index,
          user_defined_allocator, OMP_EXPR_PARSE_expression);
      ROSE_ASSERT(clause_expression != nullptr);
      user_defined_parameter =
          checkOmpExpressionClause(clause_expression, global, e_allocate);
    }
    SgOmpAllocateClause *allocate_clause =
        new SgOmpAllocateClause(explist, sg_modifier, user_defined_parameter);
    if (user_defined_parameter != nullptr) {
      user_defined_parameter->set_parent(allocate_clause);
    }
    allocate_clause->set_uses_allocator_modifier_syntax(
        static_cast<OpenMPAllocateClause *>(current_omp_clause)
            ->usesAllocatorModifierSyntax());
    const std::string &alignment =
        static_cast<OpenMPAllocateClause *>(current_omp_clause)->getAlignment();
    if (!alignment.empty()) {
      SgExpression *alignment_expression = consumeParsedClauseExpression(
          current_omp_clause->getKind(), parsed_nodes, parsed_node_index,
          alignment, OMP_EXPR_PARSE_expression);
      allocate_clause->set_alignment(alignment_expression);
      alignment_expression->set_parent(allocate_clause);
    }
    requireParsedClauseExpressionsConsumed(current_omp_clause->getKind(),
                                           parsed_nodes, parsed_node_index);
    result = allocate_clause;
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
        toSgOmpClauseReductionIdentifier(
            identifier, current_OpenMPIR_to_SageIII.second->getBaseLang());
    SgOmpNameExpression *user_defined_identifier = NULL;
    const std::string user_identifier_text =
        ((OpenMPReductionClause *)current_omp_clause)
            ->getUserDefinedIdentifier();
    if (sg_identifier == SgOmpClause::e_omp_reduction_user_defined_identifier) {
      user_defined_identifier = consumeParsedClauseOpenMPName(
          current_omp_clause->getKind(), auxiliary_nodes, user_identifier_text);
    } else if (!user_identifier_text.empty()) {
      std::cerr << "REX_OMP_AST_INVARIANT[reduction-identifier]: predefined "
                   "identifier owns a user-defined name\n";
      ROSE_ABORT();
    }
    result = new SgOmpReductionClause(explist, sg_modifier, sg_identifier,
                                      user_defined_identifier);
    if (user_defined_identifier != nullptr) {
      user_defined_identifier->set_parent(result);
    }
    printf("Reduction Clause added!\n");
    break;
  }
  case OMPC_in_reduction: {
    OpenMPInReductionClauseIdentifier identifier =
        ((OpenMPInReductionClause *)current_omp_clause)->getIdentifier();
    SgOmpClause::omp_in_reduction_identifier_enum sg_identifier =
        toSgOmpClauseInReductionIdentifier(
            identifier, current_OpenMPIR_to_SageIII.second->getBaseLang());
    SgOmpNameExpression *user_defined_identifier = NULL;
    const std::string user_identifier_text =
        ((OpenMPInReductionClause *)current_omp_clause)
            ->getUserDefinedIdentifier();
    if (sg_identifier ==
        SgOmpClause::e_omp_in_reduction_user_defined_identifier) {
      user_defined_identifier = consumeParsedClauseOpenMPName(
          current_omp_clause->getKind(), auxiliary_nodes, user_identifier_text);
    } else if (!user_identifier_text.empty()) {
      std::cerr << "REX_OMP_AST_INVARIANT[in-reduction-identifier]: "
                   "predefined identifier owns a user-defined name\n";
      ROSE_ABORT();
    }
    result = new SgOmpInReductionClause(explist, sg_identifier,
                                        user_defined_identifier);
    if (user_defined_identifier != nullptr) {
      user_defined_identifier->set_parent(result);
    }
    printf("In_reduction Clause added!\n");
    break;
  }
  case OMPC_task_reduction: {
    OpenMPTaskReductionClauseIdentifier identifier =
        ((OpenMPTaskReductionClause *)current_omp_clause)->getIdentifier();
    SgOmpClause::omp_task_reduction_identifier_enum sg_identifier =
        toSgOmpClauseTaskReductionIdentifier(
            identifier, current_OpenMPIR_to_SageIII.second->getBaseLang());
    SgOmpNameExpression *user_defined_identifier = NULL;
    const std::string user_identifier_text =
        ((OpenMPTaskReductionClause *)current_omp_clause)
            ->getUserDefinedIdentifier();
    if (sg_identifier ==
        SgOmpClause::e_omp_task_reduction_user_defined_identifier) {
      user_defined_identifier = consumeParsedClauseOpenMPName(
          current_omp_clause->getKind(), auxiliary_nodes, user_identifier_text);
    } else if (!user_identifier_text.empty()) {
      std::cerr << "REX_OMP_AST_INVARIANT[task-reduction-identifier]: "
                   "predefined identifier owns a user-defined name\n";
      ROSE_ABORT();
    }
    result = new SgOmpTaskReductionClause(explist, sg_identifier,
                                          user_defined_identifier);
    if (user_defined_identifier != nullptr) {
      user_defined_identifier->set_parent(result);
    }
    printf("Task_reduction Clause added!\n");
    break;
  }
  case OMPC_linear: {
    OpenMPLinearClauseModifier modifier =
        ((OpenMPLinearClause *)current_omp_clause)->getModifier();
    SgOmpClause::omp_linear_modifier_enum sg_modifier =
        toSgOmpClauseLinearModifier(modifier);
    SgExpression *stepExp = NULL;
    const std::vector<const OmpParsedExpression *> *auxiliary_nodes =
        getParsedClauseAuxiliaryExpressionNodes(
            current_OpenMPIR_to_SageIII.second, current_omp_clause);
    size_t auxiliary_node_index = 0;
    if ((((OpenMPLinearClause *)current_omp_clause)->getUserDefinedStep()) !=
        "") {
      stepExp = parseClauseExpressionWithCache(
          current_omp_clause->getKind(), auxiliary_nodes, auxiliary_node_index,
          ((OpenMPLinearClause *)current_omp_clause)->getUserDefinedStep());
    }
    requireParsedClauseExpressionsConsumed(
        current_omp_clause->getKind(), auxiliary_nodes, auxiliary_node_index);
    result = new SgOmpLinearClause(explist, stepExp, sg_modifier);
    if (stepExp != nullptr) {
      stepExp->set_parent(result);
    }
    printf("Linear Clause added!\n");
    break;
  }
  case OMPC_aligned: {
    SgExpression *alignExp = NULL;
    const std::vector<const OmpParsedExpression *> *auxiliary_nodes =
        getParsedClauseAuxiliaryExpressionNodes(
            current_OpenMPIR_to_SageIII.second, current_omp_clause);
    size_t auxiliary_node_index = 0;
    if ((((OpenMPAlignedClause *)current_omp_clause)
             ->getUserDefinedAlignment()) != "") {
      alignExp = parseClauseExpressionWithCache(
          current_omp_clause->getKind(), auxiliary_nodes, auxiliary_node_index,
          ((OpenMPAlignedClause *)current_omp_clause)
              ->getUserDefinedAlignment());
    }
    requireParsedClauseExpressionsConsumed(
        current_omp_clause->getKind(), auxiliary_nodes, auxiliary_node_index);
    result = new SgOmpAlignedClause(explist, alignExp);
    if (alignExp != nullptr) {
      alignExp->set_parent(result);
    }
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
    std::cerr << "REX_OMP_AST_INVARIANT[variable-clause]: unsupported clause "
                 "kind "
              << static_cast<int>(clause_kind) << "\n";
    ROSE_ABORT();
  }
  }
  if (clause_kind == OMPC_firstprivate) {
    OpenMPFirstprivateClause *firstprivate_clause =
        static_cast<OpenMPFirstprivateClause *>(current_omp_clause);
    SgOmpFirstprivateClause *sage_firstprivate =
        isSgOmpFirstprivateClause(result);
    ROSE_ASSERT(sage_firstprivate != nullptr);
    sage_firstprivate->set_saved(firstprivate_clause->isSaved());
    if (firstprivate_clause->hasDirectiveNameModifier()) {
      sage_firstprivate->set_directive_name_modifier(
          toSgOmpClauseDirectiveNameModifier(
              firstprivate_clause->getDirectiveNameModifier()));
    }
  } else if (current_omp_clause->hasDirectiveNameModifier()) {
    result->set_directive_name_modifier(toSgOmpClauseDirectiveNameModifier(
        current_omp_clause->getDirectiveNameModifier()));
  }
  setOneSourcePositionForTransformation(result);
  buildVariableList(result);
  explist->set_parent(result);
  switch (clause_kind) {
  case OMPC_link:
  case OMPC_enter:
  case OMPC_local:
  case OMPC_copyin:
  case OMPC_firstprivate:
  case OMPC_nontemporal:
  case OMPC_inclusive:
  case OMPC_exclusive:
  case OMPC_is_device_ptr:
  case OMPC_use_device_ptr:
  case OMPC_use_device_addr:
  case OMPC_has_device_addr:
  case OMPC_private:
  case OMPC_copyprivate:
  case OMPC_reduction:
  case OMPC_in_reduction:
  case OMPC_task_reduction:
  case OMPC_lastprivate:
  case OMPC_shared:
  case OMPC_uniform:
    attachOriginalOpenMPVariableSpelling(current_OpenMPIR_to_SageIII.second,
                                         current_omp_clause, parsed_nodes,
                                         result);
    break;
  default:
    break;
  }
  attachOmpVariablesClauseToDirective(
      directive, current_OpenMPIR_to_SageIII.second, result);
  openMPExpressionVariables().clear();
  return result;
}

SgOmpToClause *
convertToClause(SgStatement *clause_body,
                std::pair<SgPragmaDeclaration *, OpenMPDirective *>
                    current_OpenMPIR_to_SageIII,
                OpenMPClause *current_omp_clause) {
  SgOmpToClause *result = NULL;
  OpenMPToClauseKind kind = ((OpenMPToClause *)current_omp_clause)->getKind();
  SgOmpClause::omp_to_kind_enum sg_type = toSgOmpClauseToKind(kind);
  SgOmpNameExpression *mapper_identifier = NULL;
  OpenMPToClause *to_clause = static_cast<OpenMPToClause *>(current_omp_clause);
  const bool has_mapper_payload = !to_clause->getMapperIdentifier().empty();
  const bool has_iterator_payload = !to_clause->getIterators().empty();
  const bool requires_mapper = sg_type == SgOmpClause::e_omp_to_kind_mapper;
  const bool requires_iterator = sg_type == SgOmpClause::e_omp_to_kind_iterator;
  if (has_mapper_payload != requires_mapper ||
      has_iterator_payload != requires_iterator) {
    std::cerr << "REX_OMP_AST_INVARIANT[to-payload]: to kind and typed "
                 "mapper/iterator payloads disagree\n";
    ROSE_ABORT();
  }

  openMPExpressionVariables().clear();
  const std::vector<const OmpParsedExpression *> *parsed_nodes =
      getParsedClauseExpressionNodes(current_OpenMPIR_to_SageIII.second,
                                     current_omp_clause);
  const std::vector<OpenMPExpressionItem> &current_expressions =
      current_omp_clause->getExpressionItems();
  if (!current_expressions.empty() &&
      (parsed_nodes == nullptr || parsed_nodes->empty())) {
    std::cerr << "REX_OMP_AST_INVARIANT[to]: expressions have no cached "
                 "semantic nodes\n";
    ROSE_ABORT();
  }
  if (parsed_nodes != nullptr && !parsed_nodes->empty()) {
    for (const OmpParsedExpression *parsed : *parsed_nodes) {
      requireCachedParsedExpression(parsed);
      if (parsed->mode == OMP_EXPR_PARSE_array_section ||
          parsed->mode == OMP_EXPR_PARSE_variable_list) {
        appendParsedVariableNode(parsed);
      }
    }
  }
  SgExprListExp *explist = buildExprListExp();

  result = new SgOmpToClause(explist, sg_type);
  if (requires_mapper) {
    if (parsed_nodes == nullptr ||
        parsed_nodes->size() != current_expressions.size() + 1) {
      std::cerr << "REX_OMP_AST_INVARIANT[to-mapper-owner]: locator and "
                   "mapper-token cache cardinality disagree\n";
      ROSE_ABORT();
    }
    mapper_identifier = parseMapperIdentifierExpression(
        current_omp_clause->getKind(),
        (*parsed_nodes)[current_expressions.size()],
        to_clause->getMapperIdentifier());
    if (mapper_identifier == nullptr ||
        mapper_identifier->get_parent() != nullptr) {
      std::cerr << "REX_OMP_AST_INVARIANT[to-mapper-owner]: mapper identifier "
                   "is null or already owned\n";
      ROSE_ABORT();
    }
  } else if (parsed_nodes == nullptr ||
             parsed_nodes->size() != current_expressions.size()) {
    std::cerr << "REX_OMP_AST_INVARIANT[to-mapper-owner]: mapper-free cache "
                 "cardinality disagrees with its locators\n";
    ROSE_ABORT();
  }
  result->set_mapper_identifier(mapper_identifier);
  if (mapper_identifier != nullptr) {
    mapper_identifier->set_parent(result);
  }
  ROSE_ASSERT(result != NULL);
  buildVariableList(result);
  explist->set_parent(result);
  attachOriginalOpenMPVariableSpelling(current_OpenMPIR_to_SageIII.second,
                                       current_omp_clause, parsed_nodes,
                                       result);
  result->get_iterator_definitions() = buildClauseIteratorDefinitions(
      getParsedClauseAuxiliaryExpressionNodes(
          current_OpenMPIR_to_SageIII.second, current_omp_clause),
      getClauseSourceAuxiliaryExpressionTexts(
          current_OpenMPIR_to_SageIII.second, current_omp_clause),
      to_clause->getIterators());
  ownClauseIteratorDefinitions(result, result->get_iterator_definitions(),
                               requires_iterator);

  setOneSourcePositionForTransformation(result);
  SgOmpClause *sg_clause = result;
  addOmpClause(clause_body, sg_clause);
  openMPExpressionVariables().clear();
  return result;
}

SgOmpFromClause *
convertFromClause(SgStatement *clause_body,
                  std::pair<SgPragmaDeclaration *, OpenMPDirective *>
                      current_OpenMPIR_to_SageIII,
                  OpenMPClause *current_omp_clause) {
  SgOmpFromClause *result = NULL;
  OpenMPFromClauseKind kind =
      ((OpenMPFromClause *)current_omp_clause)->getKind();
  SgOmpClause::omp_from_kind_enum sg_type = toSgOmpClauseFromKind(kind);
  SgOmpNameExpression *mapper_identifier = NULL;
  OpenMPFromClause *from_clause =
      static_cast<OpenMPFromClause *>(current_omp_clause);
  const bool has_mapper_payload = !from_clause->getMapperIdentifier().empty();
  const bool has_iterator_payload = !from_clause->getIterators().empty();
  const bool requires_mapper = sg_type == SgOmpClause::e_omp_from_kind_mapper;
  const bool requires_iterator =
      sg_type == SgOmpClause::e_omp_from_kind_iterator;
  if (has_mapper_payload != requires_mapper ||
      has_iterator_payload != requires_iterator) {
    std::cerr << "REX_OMP_AST_INVARIANT[from-payload]: from kind and typed "
                 "mapper/iterator payloads disagree\n";
    ROSE_ABORT();
  }

  openMPExpressionVariables().clear();
  const std::vector<const OmpParsedExpression *> *parsed_nodes =
      getParsedClauseExpressionNodes(current_OpenMPIR_to_SageIII.second,
                                     current_omp_clause);
  const std::vector<OpenMPExpressionItem> &current_expressions =
      current_omp_clause->getExpressionItems();
  if (!current_expressions.empty() &&
      (parsed_nodes == nullptr || parsed_nodes->empty())) {
    std::cerr << "REX_OMP_AST_INVARIANT[from]: expressions have no cached "
                 "semantic nodes\n";
    ROSE_ABORT();
  }
  if (parsed_nodes != nullptr && !parsed_nodes->empty()) {
    for (const OmpParsedExpression *parsed : *parsed_nodes) {
      requireCachedParsedExpression(parsed);
      if (parsed->mode == OMP_EXPR_PARSE_array_section ||
          parsed->mode == OMP_EXPR_PARSE_variable_list) {
        appendParsedVariableNode(parsed);
      }
    }
  }
  SgExprListExp *explist = buildExprListExp();
  result = new SgOmpFromClause(explist, sg_type);
  if (requires_mapper) {
    if (parsed_nodes == nullptr ||
        parsed_nodes->size() != current_expressions.size() + 1) {
      std::cerr << "REX_OMP_AST_INVARIANT[from-mapper-owner]: locator and "
                   "mapper-token cache cardinality disagree\n";
      ROSE_ABORT();
    }
    mapper_identifier = parseMapperIdentifierExpression(
        current_omp_clause->getKind(),
        (*parsed_nodes)[current_expressions.size()],
        from_clause->getMapperIdentifier());
    if (mapper_identifier == nullptr ||
        mapper_identifier->get_parent() != nullptr) {
      std::cerr
          << "REX_OMP_AST_INVARIANT[from-mapper-owner]: mapper identifier is "
             "null or already owned\n";
      ROSE_ABORT();
    }
  } else if (parsed_nodes == nullptr ||
             parsed_nodes->size() != current_expressions.size()) {
    std::cerr << "REX_OMP_AST_INVARIANT[from-mapper-owner]: mapper-free "
                 "cache cardinality disagrees with its locators\n";
    ROSE_ABORT();
  }
  result->set_mapper_identifier(mapper_identifier);
  if (mapper_identifier != nullptr) {
    mapper_identifier->set_parent(result);
  }
  ROSE_ASSERT(result != NULL);
  buildVariableList(result);
  explist->set_parent(result);
  attachOriginalOpenMPVariableSpelling(current_OpenMPIR_to_SageIII.second,
                                       current_omp_clause, parsed_nodes,
                                       result);
  result->get_iterator_definitions() = buildClauseIteratorDefinitions(
      getParsedClauseAuxiliaryExpressionNodes(
          current_OpenMPIR_to_SageIII.second, current_omp_clause),
      getClauseSourceAuxiliaryExpressionTexts(
          current_OpenMPIR_to_SageIII.second, current_omp_clause),
      from_clause->getIterators());
  ownClauseIteratorDefinitions(result, result->get_iterator_definitions(),
                               requires_iterator);

  setOneSourcePositionForTransformation(result);
  SgOmpClause *sg_clause = result;
  addOmpClause(clause_body, sg_clause);
  openMPExpressionVariables().clear();
  return result;
}

SgOmpDependClause *
convertDependClause(SgStatement *clause_body,
                    std::pair<SgPragmaDeclaration *, OpenMPDirective *>
                        current_OpenMPIR_to_SageIII,
                    OpenMPClause *current_omp_clause) {
  SgOmpDependClause *result = NULL;
  clearOpenMPClauseTemporaryState();

  const std::vector<const OmpParsedExpression *> *parsed_nodes =
      getParsedClauseExpressionNodes(current_OpenMPIR_to_SageIII.second,
                                     current_omp_clause);
  const std::vector<OpenMPExpressionItem> &current_expressions =
      current_omp_clause->getExpressionItems();
  if (!current_expressions.empty() &&
      (parsed_nodes == nullptr || parsed_nodes->empty())) {
    std::cerr << "REX_OMP_AST_INVARIANT[depend]: expressions have no cached "
                 "semantic nodes\n";
    ROSE_ABORT();
  }

  auto *depend_clause = static_cast<OpenMPDependClause *>(current_omp_clause);
  OpenMPDependClauseModifier modifier = depend_clause->getModifier();
  const bool requires_iterator = modifier == OMPC_DEPEND_MODIFIER_iterator;
  if (requires_iterator != !depend_clause->getIterators().empty()) {
    std::cerr << "REX_OMP_AST_INVARIANT[depend-iterator-payload]: depend "
                 "modifier and typed iterator payload disagree\n";
    ROSE_ABORT();
  }
  SgOmpIteratorDefinitionPtrList depend_iterator_definitions;
  if (requires_iterator) {
    depend_iterator_definitions = buildClauseIteratorDefinitions(
        getParsedClauseAuxiliaryExpressionNodes(
            current_OpenMPIR_to_SageIII.second, current_omp_clause),
        getClauseSourceAuxiliaryExpressionTexts(
            current_OpenMPIR_to_SageIII.second, current_omp_clause),
        depend_clause->getIterators());
  }
  SgOmpClause::omp_depend_modifier_enum sg_modifier =
      toSgOmpClauseDependModifier(modifier);
  OpenMPDependClauseType type = depend_clause->getType();
  SgOmpClause::omp_dependence_type_enum sg_type =
      toSgOmpClauseDependenceType(type);
  SgExprListExp *explist = NULL;
  SgExprListExp *sink_vectors = nullptr;
  const size_t depend_expression_count = current_expressions.size();
  if (type != OMPC_DEPENDENCE_TYPE_sink) {
    if (parsed_nodes != nullptr && !parsed_nodes->empty()) {
      for (const OmpParsedExpression *parsed : *parsed_nodes) {
        requireCachedParsedExpression(parsed);
        if (parsed->mode == OMP_EXPR_PARSE_array_section ||
            parsed->mode == OMP_EXPR_PARSE_variable_list) {
          appendParsedVariableNode(parsed);
        } else if (parsed->mode == OMP_EXPR_PARSE_expression) {
          appendParsedVariableNode(parsed);
        }
      }
    }
    explist = buildExprListExp();
  } else if (type == OMPC_DEPENDENCE_TYPE_sink) {
    explist = buildExprListExp();
    sink_vectors = buildExprListExp();
    if (parsed_nodes != nullptr && !parsed_nodes->empty()) {
      for (const OmpParsedExpression *parsed : *parsed_nodes) {
        requireCachedParsedExpression(parsed);
        SgExpression *parsed_expr = consumeParsedExpressionNode(parsed);
        if (parsed_expr == nullptr || parsed_expr->get_parent() != nullptr) {
          std::cerr << "REX_OMP_AST_INVARIANT[depend-sink-owner]: sink "
                       "vector is null or already owned\n";
          ROSE_ABORT();
        }
        sink_vectors->append_expression(parsed_expr);
        if (parsed_expr->get_parent() != sink_vectors) {
          std::cerr << "REX_OMP_AST_INVARIANT[depend-sink-owner]: sink "
                       "vector list did not acquire exact ownership\n";
          ROSE_ABORT();
        }
      }
    }
    if (sink_vectors->get_expressions().empty()) {
      std::cerr << "REX_OMP_AST_INVARIANT[depend-sink-payload]: sink "
                   "dependence has no iteration vector\n";
      ROSE_ABORT();
    }
  }
  result = new SgOmpDependClause(explist, sg_modifier, sg_type);
  ROSE_ASSERT(result != NULL);
  if (type != OMPC_DEPENDENCE_TYPE_sink &&
      type != OMPC_DEPENDENCE_TYPE_source && depend_expression_count > 0) {
    ROSE_ASSERT(!openMPExpressionVariables().empty());
  }
  buildVariableList(result);
  explist->set_parent(result);
  if (type != OMPC_DEPENDENCE_TYPE_sink) {
    attachOriginalOpenMPVariableSpelling(current_OpenMPIR_to_SageIII.second,
                                         current_omp_clause, parsed_nodes,
                                         result);
  }
  result->set_sink_vectors(sink_vectors);
  if (sink_vectors != nullptr) {
    sink_vectors->set_parent(result);
  }
  result->get_iterator_definitions() = std::move(depend_iterator_definitions);
  ownClauseIteratorDefinitions(result, result->get_iterator_definitions(),
                               requires_iterator);
  setOneSourcePositionForTransformation(result);
  SgOmpClause *sg_clause = result;
  // DEPEND is accepted by both structured directives and standalone
  // clause-owning directives such as INTEROP.  Attach through the common
  // typed dispatcher so the structural list wrapper remains the direct owner.
  addOmpClause(clause_body, sg_clause);
  openMPExpressionVariables().clear();
  return result;
}

SgOmpAffinityClause *
convertAffinityClause(SgStatement *clause_body,
                      std::pair<SgPragmaDeclaration *, OpenMPDirective *>
                          current_OpenMPIR_to_SageIII,
                      OpenMPClause *current_omp_clause) {
  SgOmpAffinityClause *result = NULL;

  auto *affinity_clause =
      static_cast<OpenMPAffinityClause *>(current_omp_clause);
  OpenMPAffinityClauseModifier modifier = affinity_clause->getModifier();
  const bool requires_iterator = modifier == OMPC_AFFINITY_MODIFIER_iterator;
  if (requires_iterator != !affinity_clause->getIterators().empty()) {
    std::cerr << "REX_OMP_AST_INVARIANT[affinity-iterator-payload]: affinity "
                 "modifier and typed iterator payload disagree\n";
    ROSE_ABORT();
  }
  const std::vector<const OmpParsedExpression *> *parsed_nodes =
      getParsedClauseExpressionNodes(current_OpenMPIR_to_SageIII.second,
                                     current_omp_clause);
  SgOmpIteratorDefinitionPtrList affinity_iterator_definitions;
  if (requires_iterator) {
    affinity_iterator_definitions = buildClauseIteratorDefinitions(
        getParsedClauseAuxiliaryExpressionNodes(
            current_OpenMPIR_to_SageIII.second, current_omp_clause),
        getClauseSourceAuxiliaryExpressionTexts(
            current_OpenMPIR_to_SageIII.second, current_omp_clause),
        affinity_clause->getIterators());
  }
  SgOmpClause::omp_affinity_modifier_enum sg_modifier =
      toSgOmpClauseAffinityModifier(modifier);

  const std::vector<OpenMPExpressionItem> &current_expressions =
      current_omp_clause->getExpressionItems();
  if (!current_expressions.empty() &&
      (parsed_nodes == nullptr || parsed_nodes->empty())) {
    std::cerr << "REX_OMP_AST_INVARIANT[affinity]: expressions have no "
                 "cached semantic nodes\n";
    ROSE_ABORT();
  }
  if (parsed_nodes != nullptr) {
    for (const OmpParsedExpression *parsed : *parsed_nodes) {
      requireCachedParsedExpression(parsed);
      if (parsed->mode == OMP_EXPR_PARSE_array_section ||
          parsed->mode == OMP_EXPR_PARSE_variable_list ||
          parsed->mode == OMP_EXPR_PARSE_expression) {
        appendParsedVariableNode(parsed);
      }
    }
  }
  SgExprListExp *explist = buildExprListExp();

  result = new SgOmpAffinityClause(explist, sg_modifier);
  ROSE_ASSERT(result != NULL);
  buildVariableList(result);
  explist->set_parent(result);
  attachOriginalOpenMPVariableSpelling(current_OpenMPIR_to_SageIII.second,
                                       current_omp_clause, parsed_nodes,
                                       result);
  result->get_iterator_definitions() = std::move(affinity_iterator_definitions);
  ownClauseIteratorDefinitions(result, result->get_iterator_definitions(),
                               requires_iterator);

  setOneSourcePositionForTransformation(result);
  SgOmpClause *sg_clause = result;
  addOmpClause(clause_body, sg_clause);
  openMPExpressionVariables().clear();
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
  const std::vector<OpenMPExpressionItem> &current_expressions =
      current_omp_clause->getExpressionItems();
  size_t parsed_node_index = 0;
  if (current_expressions.size() > 1 && clause_kind != OMPC_looprange) {
    std::cerr << "REX_OMP_AST_INVARIANT[expression-clause]: expected at most "
                 "one semantic expression\n";
    ROSE_ABORT();
  }
  if (clause_kind == OMPC_looprange) {
    if (parsed_nodes == nullptr ||
        parsed_nodes->size() != current_expressions.size()) {
      std::cerr << "REX_OMP_AST_INVARIANT[looprange]: expression cache does "
                   "not match the source operands\n";
      ROSE_ABORT();
    }
    SgExprListExp *operands = SageBuilder::buildExprListExp();
    for (const OmpParsedExpression *parsed : *parsed_nodes) {
      operands->append_expression(consumeParsedExpressionNode(parsed));
      ++parsed_node_index;
    }
    clause_expression = operands;
  } else if (!current_expressions.empty()) {
    const OpenMPExpressionItem &expression = current_expressions.front();
    if (expression.fragment.spelling.empty()) {
      std::cerr << "REX_OMP_AST_INVARIANT[expression-clause]: empty typed "
                   "expression\n";
      ROSE_ABORT();
    }
    OpenMPExprParseMode required_mode = OMP_EXPR_PARSE_expression;
    if (clause_kind == OMPC_device) {
      if (parsed_nodes == nullptr || parsed_nodes->size() != 1) {
        std::cerr << "REX_OMP_AST_INVARIANT[device-expression]: device clause "
                     "does not own one exact expression callback record\n";
        ROSE_ABORT();
      }
      const OmpParsedExpression *parsed = parsed_nodes->front();
      requireCachedParsedExpression(parsed);
      if (parsed->mode == OMP_EXPR_PARSE_verbatim) {
        SgOmpSourceExpression *wildcard = isSgOmpSourceExpression(parsed->node);
        if (trimWhitespaceCopy(expression.fragment.spelling) != "*" ||
            parsed->text != "*" || wildcard == nullptr ||
            wildcard->get_spelling() != "*") {
          std::cerr << "REX_OMP_AST_INVARIANT[device-expression]: verbatim "
                       "device payload is not the typed wildcard token\n";
          ROSE_ABORT();
        }
        required_mode = OMP_EXPR_PARSE_verbatim;
      } else if (parsed->mode != OMP_EXPR_PARSE_expression) {
        std::cerr << "REX_OMP_AST_INVARIANT[device-expression]: device "
                     "payload has neither expression nor wildcard role\n";
        ROSE_ABORT();
      }
    }
    clause_expression = consumeParsedClauseExpression(
        current_omp_clause->getKind(), parsed_nodes, parsed_node_index,
        expression.fragment.spelling, required_mode);
  }
  requireParsedClauseExpressionsConsumed(current_omp_clause->getKind(),
                                         parsed_nodes, parsed_node_index);

  switch (clause_kind) {
  case OMPC_align: {
    result = new SgOmpAlignClause(clause_expression);
    break;
  }
  case OMPC_message: {
    result = new SgOmpMessageClause(clause_expression);
    break;
  }
  case OMPC_graph_id: {
    result = new SgOmpGraphIdClause(clause_expression);
    break;
  }
  case OMPC_graph_reset: {
    result = new SgOmpGraphResetClause(clause_expression);
    break;
  }
  case OMPC_transparent: {
    result = new SgOmpTransparentClause(clause_expression);
    break;
  }
  case OMPC_threadset: {
    result = new SgOmpThreadsetClause(clause_expression);
    break;
  }
  case OMPC_safesync: {
    result = new SgOmpSafesyncClause(clause_expression);
    break;
  }
  case OMPC_looprange: {
    result = new SgOmpLooprangeClause(clause_expression);
    break;
  }
  case OMPC_no_openmp_constructs: {
    result = new SgOmpNoOpenmpConstructsClause(clause_expression);
    break;
  }
  case OMPC_holds: {
    result = new SgOmpHoldsClause(clause_expression);
    break;
  }
  case OMPC_use: {
    result = new SgOmpUseClause(clause_expression);
    break;
  }
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
  case OMPC_filter: {
    SgExpression *filter_expression =
        checkOmpExpressionClause(clause_expression, global, e_num_threads);
    result = new SgOmpFilterClause(filter_expression);
    printf("Filter Clause added!\n");
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
    std::cerr
        << "REX_OMP_AST_INVARIANT[expression-clause]: unsupported clause kind "
        << static_cast<int>(clause_kind) << "\n";
    ROSE_ABORT();
  }
  }
  setOneSourcePositionForTransformation(result);
  if (result != NULL) {
    if (SgExpression *result_expression = result->get_expression()) {
      result_expression->set_parent(result);
      attachOriginalOpenMPExpressionSpelling(current_OpenMPIR_to_SageIII.second,
                                             current_omp_clause, parsed_nodes,
                                             result_expression);
    }
  }

  // reconsider the location of following code to attach clause
  addOmpClause(directive, result);

  return result;
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

void parseOmpVariable(std::pair<SgPragmaDeclaration *, OpenMPDirective *>
                          current_OpenMPIR_to_SageIII,
                      OpenMPClauseKind clause_kind, std::string expression) {
  static_cast<void>(clause_kind);
  std::string expr_string = std::string() + "varlist " + expression + "\n";
  std::size_t old_size = openMPExpressionVariables().size();
  parseExpression(current_OpenMPIR_to_SageIII.first, expr_string.c_str());
  if (openMPExpressionVariables().size() != old_size) {
    return;
  }
  std::cerr << "REX_OMP_AST_INVARIANT[variable-parse]: failed to build a "
               "semantic AST node for '"
            << expression << "'\n";
  ROSE_ABORT();
}

SgExpression *parseOmpExpression(SgPragmaDeclaration *directive,
                                 OpenMPClauseKind clause_kind,
                                 std::string expression) {
  (void)clause_kind;
  SgSourceFile *source_file = getEnclosingSourceFile(directive);
  const bool is_fortran_source =
      source_file != NULL &&
      (source_file->get_Fortran_only() || source_file->get_F77_only() ||
       source_file->get_F90_only() || source_file->get_F95_only() ||
       source_file->get_F2003_only());
  if (is_fortran_source) {
    bool bool_value = false;
    if (parseFortranBooleanLiteral(expression, bool_value)) {
      return SageBuilder::buildBoolValExp(bool_value);
    }
  }
  std::string expr_string = std::string() + "expr (" + expression + ")\n";
  SgExpression *sg_expression = parseExpression(directive, expr_string.c_str());
  if (sg_expression != NULL) {
    return sg_expression;
  }
  std::cerr << "REX_OMP_AST_INVARIANT[expression-parse]: failed to build a "
               "semantic AST node for '"
            << expression << "'\n";
  ROSE_ABORT();
}

SgExpression *parseOmpArraySection(SgPragmaDeclaration *directive,
                                   OpenMPClauseKind clause_kind,
                                   std::string expression) {
  static_cast<void>(clause_kind);
  std::string expr_string =
      std::string() + "array_section (" + expression + ")\n";
  SgExpression *sg_expression =
      parseArraySectionExpression(directive, expr_string.c_str());
  if (sg_expression != NULL) {
    return sg_expression;
  }
  std::cerr << "REX_OMP_AST_INVARIANT[array-section-parse]: failed to build "
               "a semantic AST node for '"
            << expression << "'\n";
  ROSE_ABORT();
}

void buildVariableList(SgOmpVariablesClause *current_omp_clause) {

  std::vector<SgNode *>::iterator iter;
  for (iter = openMPExpressionVariables().begin();
       iter != openMPExpressionVariables().end(); iter++) {
    appendFlattenedOmpVarExprNodes(current_omp_clause, *iter);
  }
}

static void appendDirectOmpClauses(SgStatement *statement,
                                   std::vector<SgOmpClause *> &clauses) {
  if (statement == nullptr) {
    std::cerr << "REX_OMP_AST_INVARIANT[clause-order]: null directive in "
                 "combined clause collection\n";
    ROSE_ABORT();
  }
  if (isSgOmpClauseBodyStatement(statement) == nullptr &&
      isSgOmpClauseStatement(statement) == nullptr) {
    std::cerr << "REX_OMP_AST_INVARIANT[clause-order]: directive "
              << statement->class_name()
              << " cannot structurally own combined clauses\n";
    ROSE_ABORT();
  }
  SgOmpClauseList *list = getOmpClauseList(statement);
  ROSE_ASSERT(list != nullptr);
  for (SgOmpClause *clause : list->get_clauses()) {
    if (clause == nullptr || clause->get_parent() != list) {
      std::cerr << "REX_OMP_AST_INVARIANT[clause-order]: null or misowned "
                   "combined clause\n";
      ROSE_ABORT();
    }
    clauses.push_back(clause);
  }
}

static std::vector<SgOmpClause *>
collectCombinedOmpClauses(SgOmpBodyStatement *outer) {
  if (outer == nullptr || !outer->get_source_form_is_combined()) {
    std::cerr << "REX_OMP_AST_INVARIANT[clause-order]: directive is not a "
                 "typed combined source form\n";
    ROSE_ABORT();
  }
  SgStatement *nested = outer->get_body();
  if (isSgOmpParallelStatement(outer) == nullptr || nested == nullptr ||
      nested->get_parent() != outer ||
      isSgOmpBodyStatement(nested) == nullptr) {
    std::cerr << "REX_OMP_AST_INVARIANT[clause-order]: malformed combined "
                 "parallel directive shape\n";
    ROSE_ABORT();
  }
  std::vector<SgOmpClause *> clauses;
  appendDirectOmpClauses(outer, clauses);
  appendDirectOmpClauses(nested, clauses);
  std::unordered_set<SgOmpClause *> unique;
  for (SgOmpClause *clause : clauses) {
    if (!unique.insert(clause).second) {
      std::cerr << "REX_OMP_AST_INVARIANT[clause-order]: duplicate combined "
                   "clause identity\n";
      ROSE_ABORT();
    }
  }
  return clauses;
}

static void recordNewCombinedClauseOrder(
    SgOmpBodyStatement *outer, const std::vector<SgOmpClause *> &before,
    SgOmpClause *returned_clause, OpenMPClause *ir_clause,
    std::size_t ir_clause_index, std::size_t &next_source_order) {
  if (ir_clause == nullptr ||
      ir_clause->getClausePosition() != static_cast<int>(ir_clause_index)) {
    std::cerr << "REX_OMP_AST_INVARIANT[clause-order]: combined directive "
                 "clause has no exact source-order identity\n";
    ROSE_ABORT();
  }
  const std::unordered_set<SgOmpClause *> old_clauses(before.begin(),
                                                      before.end());
  const std::vector<SgOmpClause *> after = collectCombinedOmpClauses(outer);
  std::vector<SgOmpClause *> added;
  for (SgOmpClause *clause : after) {
    if (old_clauses.find(clause) == old_clauses.end()) {
      added.push_back(clause);
    }
  }
  if (added.empty() ||
      (returned_clause != nullptr &&
       std::find(added.begin(), added.end(), returned_clause) == added.end())) {
    std::cerr << "REX_OMP_AST_INVARIANT[clause-order]: conversion did not add "
                 "the returned combined clause exactly once\n";
    ROSE_ABORT();
  }
  for (SgOmpClause *clause : added) {
    if (clause->get_combined_source_order().has_value()) {
      std::cerr << "REX_OMP_AST_INVARIANT[clause-order]: combined clause "
                   "already owns source-order provenance\n";
      ROSE_ABORT();
    }
    clause->initialize_combined_source_order(next_source_order++);
  }
}

static void finalizeCombinedClauseOrder(SgOmpBodyStatement *outer,
                                        std::size_t next_source_order) {
  const std::vector<SgOmpClause *> clauses = collectCombinedOmpClauses(outer);
  if (next_source_order != clauses.size()) {
    std::cerr << "REX_OMP_AST_INVARIANT[clause-order]: combined clause "
                 "provenance is incomplete\n";
    ROSE_ABORT();
  }
  std::vector<bool> seen(clauses.size(), false);
  for (SgOmpClause *clause : clauses) {
    const std::optional<std::size_t> &source_order =
        clause->get_combined_source_order();
    if (!source_order.has_value() || *source_order >= clauses.size() ||
        seen[*source_order]) {
      std::cerr << "REX_OMP_AST_INVARIANT[clause-order]: combined clause "
                   "provenance is not unique and contiguous\n";
      ROSE_ABORT();
    }
    seen[*source_order] = true;
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
  case OMPD_parallel_do_simd: {
    second_stmt = new SgOmpForSimdStatement(NULL, body);
    markFortranDoDirectiveSpelling(second_stmt);
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
  initializeGeneratedOpenMPStatement(second_stmt);
  SgOmpParallelStatement *first_stmt =
      new SgOmpParallelStatement(NULL, second_stmt);
  setOneSourcePositionForTransformation(first_stmt);
  copyStartFileInfo(current_OpenMPIR_to_SageIII.first, first_stmt);
  copyEndFileInfo(current_OpenMPIR_to_SageIII.first, first_stmt);
  initializeGeneratedOpenMPStatement(first_stmt);
  second_stmt->set_parent(first_stmt);
  first_stmt->set_source_form_is_combined(true);
  validateConvertedOpenMPStatementLocation(current_OpenMPIR_to_SageIII.first,
                                           first_stmt);

  OpenMPClauseKind clause_kind;
  std::vector<OpenMPClause *> *clause_vector =
      current_OpenMPIR_to_SageIII.second->getClausesInOriginalOrder();
  ROSE_ASSERT(clause_vector != nullptr);

  std::vector<SgOmpClause *> clauses_before_conversion;
  std::size_t ir_clause_index = 0;
  std::size_t next_source_order = 0;
  auto record_clause_order = [&](SgOmpClause *clause,
                                 OpenMPClause *omp_clause) {
    recordNewCombinedClauseOrder(first_stmt, clauses_before_conversion, clause,
                                 omp_clause, ir_clause_index,
                                 next_source_order);
  };

  std::vector<OpenMPClause *>::iterator citer;
  for (citer = clause_vector->begin(); citer != clause_vector->end(); citer++) {
    clauses_before_conversion = collectCombinedOmpClauses(first_stmt);
    if (*citer == nullptr) {
      std::cerr << "REX_OMP_AST_INVARIANT[clause-order]: null combined IR "
                   "clause\n";
      ROSE_ABORT();
    }
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
    case OMPC_parallel:
    case OMPC_induction: {
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
    ++ir_clause_index;
  };
  finalizeCombinedClauseOrder(first_stmt, next_source_order);
  return first_stmt;
}

bool checkOpenMPIR(OpenMPDirective *directive) {

  if (directive == NULL) {
    std::cerr << "REX_OMP_AST_INVARIANT[ir-support]: null OpenMP directive\n";
    ROSE_ABORT();
  };
  OpenMPDirectiveKind directive_kind = directive->getKind();
  switch (directive_kind) {
  case OMPD_atomic:
  case OMPD_target_data_composite:
  case OMPD_begin_declare_target:
  case OMPD_error:
  case OMPD_scope:
  case OMPD_parallel_masked:
  case OMPD_interop:
  case OMPD_assume:
  case OMPD_assumes:
  case OMPD_begin_assumes:
  case OMPD_end_assumes:
  case OMPD_end_assume:
  case OMPD_taskgraph:
  case OMPD_groupprivate:
  case OMPD_fuse:
  case OMPD_interchange:
  case OMPD_reverse:
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
  case OMPD_distribute_parallel_do:
  case OMPD_distribute_parallel_do_simd:
  case OMPD_do:
  case OMPD_flush:
  case OMPD_allocate:
  case OMPD_for:
  case OMPD_for_simd:
  case OMPD_loop:
  case OMPD_master:
  case OMPD_metadirective:
  case OMPD_begin_metadirective:
  case OMPD_masked:
  case OMPD_masked_taskloop:
  case OMPD_masked_taskloop_simd:
  case OMPD_nothing:
  case OMPD_ordered:
  case OMPD_parallel:
  case OMPD_parallel_do:
  case OMPD_parallel_do_simd:
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
  case OMPD_target_parallel_do:
  case OMPD_target_parallel:
  case OMPD_distribute_simd:
  case OMPD_distribute_parallel_for:
  case OMPD_distribute_parallel_for_simd:
  case OMPD_taskloop_simd:
  case OMPD_target_update:
  case OMPD_requires:
  case OMPD_target_parallel_for_simd:
  case OMPD_target_parallel_do_simd:
  case OMPD_target_parallel_loop:
  case OMPD_target_simd:
  case OMPD_target_teams:
  case OMPD_target_teams_distribute:
  case OMPD_target_teams_workdistribute:
  case OMPD_target_teams_distribute_simd:
  case OMPD_target_teams_loop:
  case OMPD_target_teams_distribute_parallel_for:
  case OMPD_target_teams_distribute_parallel_do:
  case OMPD_target_teams_distribute_parallel_for_simd:
  case OMPD_target_teams_distribute_parallel_do_simd:
  case OMPD_master_taskloop_simd:
  case OMPD_parallel_master_taskloop:
  case OMPD_parallel_master_taskloop_simd:
  case OMPD_teams_distribute:
  case OMPD_teams_distribute_simd:
  case OMPD_teams_distribute_parallel_for:
  case OMPD_teams_distribute_parallel_do:
  case OMPD_teams_distribute_parallel_for_simd:
  case OMPD_teams_distribute_parallel_do_simd:
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
  case OMPD_workdistribute:
  case OMPD_workshare:
  case OMPD_tile:
  case OMPD_unroll:
  case OMPD_ompx: {
    break;
  }
  default: {
    std::cerr << "REX_OMP_AST_INVARIANT[ir-support]: unsupported OpenMP "
                 "directive kind "
              << static_cast<int>(directive_kind) << "\n";
    ROSE_ABORT();
  }
  };
  const std::map<OpenMPClauseKind, std::vector<OpenMPClause *>> &clauses =
      directive->getAllClauses();
  for (const auto &entry : clauses) {
    if (entry.second.empty() ||
        std::find(entry.second.begin(), entry.second.end(), nullptr) !=
            entry.second.end()) {
      std::cerr << "REX_OMP_AST_INVARIANT[ir-support]: clause index has an "
                   "empty or null entry\n";
      ROSE_ABORT();
    }
    switch (entry.first) {
    case OMPC_acq_rel:
    case OMPC_align:
    case OMPC_self_maps:
    case OMPC_link:
    case OMPC_at:
    case OMPC_severity:
    case OMPC_message:
    case OMPC_doacross:
    case OMPC_otherwise:
    case OMPC_transparent:
    case OMPC_threadset:
    case OMPC_indirect:
    case OMPC_local:
    case OMPC_safesync:
    case OMPC_induction:
    case OMPC_enter:
    case OMPC_graph_id:
    case OMPC_graph_reset:
    case OMPC_looprange:
    case OMPC_apply:
    case OMPC_no_openmp:
    case OMPC_no_openmp_routines:
    case OMPC_no_parallelism:
    case OMPC_no_openmp_constructs:
    case OMPC_holds:
    case OMPC_use:
    case OMPC_absent:
    case OMPC_contains:
    case OMPC_init:
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
    case OMPC_filter:
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
      std::cerr << "REX_OMP_AST_INVARIANT[ir-support]: unsupported OpenMP "
                   "clause kind "
                << static_cast<int>(entry.first) << "\n";
      ROSE_ABORT();
    }
    }
  }
  return true;
}
