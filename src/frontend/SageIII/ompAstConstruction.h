#ifndef _OMP_AST_CONSTRUCTION
#define _OMP_AST_CONSTRUCTION

#include "AstSimpleProcessing.h"

#include "OpenMPIR.h"

#include <cstddef>
#include <map>
#include <memory>
#include <optional>
#include <string>
#include <utility>
#include <vector>

// Ordered operation contract shared by the OpenACC semantic producer and the
// Fortran directive frontend. C/C++ OpenMP has no frontend semantic producer;
// its independent pragma parser derives result types from the completed Sage
// AST.
enum class OmpExactSubexpressionKind {
  invalid,
  conditional,
  assign,
  rshift_assign,
  lshift_assign,
  add_assign,
  subtract_assign,
  multiply_assign,
  divide_assign,
  modulo_assign,
  bit_and_assign,
  bit_xor_assign,
  bit_or_assign,
  logical_or,
  logical_and,
  bit_or,
  bit_xor,
  bit_and,
  equal,
  not_equal,
  less,
  greater,
  less_equal,
  greater_equal,
  rshift,
  lshift,
  add,
  subtract,
  multiply,
  divide,
  modulo,
  cast,
  prefix_increment,
  unary_plus,
  unary_minus,
  logical_not,
  bit_complement,
  address_of,
  sizeof_expression,
  sizeof_type,
  dereference,
  prefix_decrement,
  call,
  subscript,
  array_section,
  member_dot,
  member_arrow,
  postfix_increment,
  postfix_decrement,
  string_literal
};

class OmpExactSubexpressionType {
public:
  OmpExactSubexpressionType(OmpExactSubexpressionKind kind,
                            SgType *result_type);

  OmpExactSubexpressionKind kind() const { return kind_; }
  SgType *resultType() const { return result_type_; }

private:
  const OmpExactSubexpressionKind kind_;
  SgType *const result_type_;
};

class OpenACCCxxExactSemanticBindings {
public:
  enum class BindingKind { invalid, qualifier, value, current_this };

  class Binding {
  public:
    Binding(std::string spelling, BindingKind kind, SgNode *semantic_node,
            SgSymbol *symbol);

    const std::string &spelling() const { return spelling_; }
    BindingKind kind() const { return kind_; }
    SgNode *semanticNode() const { return semantic_node_; }
    SgSymbol *symbol() const { return symbol_; }

  private:
    const std::string spelling_;
    const BindingKind kind_;
    SgNode *const semantic_node_;
    SgSymbol *const symbol_;
  };

  class ExpressionBindings {
  public:
    ExpressionBindings(OpenMPExprParseMode parse_mode, std::string expression,
                       std::vector<Binding> identifiers,
                       std::vector<OmpExactSubexpressionType> subexpressions);

    OpenMPExprParseMode parseMode() const { return parse_mode_; }
    const std::string &expression() const { return expression_; }
    const std::vector<Binding> &identifiers() const { return identifiers_; }
    const std::vector<OmpExactSubexpressionType> &subexpressions() const {
      return subexpressions_;
    }

  private:
    const OpenMPExprParseMode parse_mode_;
    const std::string expression_;
    const std::vector<Binding> identifiers_;
    const std::vector<OmpExactSubexpressionType> subexpressions_;
  };

  using BindingSequence = std::vector<ExpressionBindings>;

  explicit OpenACCCxxExactSemanticBindings(BindingSequence bindings);

  const BindingSequence &bindings() const { return bindings_; }

private:
  BindingSequence bindings_;
};

class OmpFortranExactSemanticBindings {
public:
  enum class Producer { flang_parse_tree, rex_typed_scope };

  enum class BindingKind {
    value,
    common_block,
    directive_local,
    syntax_name,
    directive_local_declaration
  };

  class Binding {
  public:
    Binding(std::size_t source_offset, std::size_t source_size,
            std::string spelling, std::string source_spelling, BindingKind kind,
            SgNode *semantic_node, SgSymbol *symbol,
            SgType *directive_local_type);

    std::size_t sourceOffset() const { return source_offset_; }
    std::size_t sourceSize() const { return source_size_; }
    const std::string &spelling() const { return spelling_; }
    const std::string &sourceSpelling() const { return source_spelling_; }
    BindingKind kind() const { return kind_; }
    SgNode *semanticNode() const { return semantic_node_; }
    SgSymbol *symbol() const { return symbol_; }
    SgType *directiveLocalType() const { return directive_local_type_; }

  private:
    const std::size_t source_offset_;
    const std::size_t source_size_;
    const std::string spelling_;
    const std::string source_spelling_;
    const BindingKind kind_;
    SgNode *const semantic_node_;
    SgSymbol *const symbol_;
    SgType *const directive_local_type_;
  };

  class ExpressionTypes {
  public:
    ExpressionTypes(std::size_t source_offset, std::size_t source_size,
                    std::string expression,
                    std::vector<OmpExactSubexpressionType> subexpressions);

    std::size_t sourceOffset() const { return source_offset_; }
    std::size_t sourceSize() const { return source_size_; }
    const std::string &expression() const { return expression_; }
    const std::vector<OmpExactSubexpressionType> &subexpressions() const {
      return subexpressions_;
    }

  private:
    const std::size_t source_offset_;
    const std::size_t source_size_;
    const std::string expression_;
    const std::vector<OmpExactSubexpressionType> subexpressions_;
  };

  OmpFortranExactSemanticBindings(Producer producer,
                                  std::string directive_source,
                                  SgType *default_integer_type,
                                  std::vector<Binding> bindings,
                                  std::vector<ExpressionTypes> expressions);

  Producer producer() const { return producer_; }
  const std::string &directiveSource() const { return directive_source_; }
  SgType *defaultIntegerType() const { return default_integer_type_; }
  const std::vector<Binding> &bindings() const { return bindings_; }
  const std::vector<ExpressionTypes> &expressions() const {
    return expressions_;
  }

private:
  Producer producer_;
  std::string directive_source_;
  SgType *default_integer_type_;
  std::vector<Binding> bindings_;
  std::vector<ExpressionTypes> expressions_;
};

struct OpenMPProducerSemanticRecords {
  std::optional<OpenACCCxxExactSemanticBindings> openacc_cxx_semantic_bindings;
  std::optional<OmpFortranExactSemanticBindings>
      fortran_exact_semantic_bindings;

  bool empty() const {
    return !openacc_cxx_semantic_bindings.has_value() &&
           !fortran_exact_semantic_bindings.has_value();
  }
};

namespace OmpSupport {
// The OpenMP directive and expression parsers are generated as non-reentrant
// Flex/Bison parsers.  Every access to their conversion state must therefore
// occur inside one process-wide, stack-owned session.  Constructing a second
// session while one is active is a hard invariant failure, including from a
// different thread.
class OpenMPConversionSession {
public:
  explicit OpenMPConversionSession(SgSourceFile *source_file);
  ~OpenMPConversionSession();

  OpenMPConversionSession(const OpenMPConversionSession &) = delete;
  OpenMPConversionSession &operator=(const OpenMPConversionSession &) = delete;
  OpenMPConversionSession(OpenMPConversionSession &&) = delete;
  OpenMPConversionSession &operator=(OpenMPConversionSession &&) = delete;

private:
  struct Impl;
  std::unique_ptr<Impl> impl_;

  friend struct OpenMPConversionSessionAccess;
};

class SgVarRefExpVisitor : public AstSimpleProcessing {
private:
  std::vector<SgExpression *> expressions;

public:
  SgVarRefExpVisitor();
  std::vector<SgExpression *> get_expressions();
  void visit(SgNode *node);
};

void processOpenMP(SgSourceFile *sageFilePtr);

void requireOpenMPConversionSession();
void registerOpenACCCxxExactSemanticBindings(
    SgSourceFile *source_file, SgPragmaDeclaration *pragma,
    OpenACCCxxExactSemanticBindings bindings);
void registerOpenMPFortranExactSemanticBindings(
    SgSourceFile *source_file, SgPragmaDeclaration *pragma,
    OmpFortranExactSemanticBindings bindings);
OpenMPProducerSemanticRecords
snapshotOpenMPProducerSemanticRecords(SgPragmaDeclaration *pragma);
void registerOpenMPProducerSemanticRecords(
    SgSourceFile *source_file, SgPragmaDeclaration *pragma,
    OpenMPProducerSemanticRecords records);
void discardOpenMPProducerSemanticRecords(SgSourceFile *source_file);
void beginOpenACCCxxExactSemanticConsumption(SgPragmaDeclaration *pragma);
void beginOpenACCCxxExactSemanticExpression(SgPragmaDeclaration *pragma,
                                            const std::string &expression,
                                            OpenMPExprParseMode parse_mode);
void endOpenACCCxxExactSemanticExpression(SgPragmaDeclaration *pragma);
void finishOpenACCCxxExactSemanticConsumption(SgPragmaDeclaration *pragma);
void beginOpenACCFortranExactSemanticConsumption(SgPragmaDeclaration *pragma);
void beginOpenACCFortranExactSemanticExpression(SgPragmaDeclaration *pragma,
                                                const std::string &expression);
void endOpenACCFortranExactSemanticExpression(SgPragmaDeclaration *pragma);
void consumeOpenACCFortranExactSemanticSyntax(SgPragmaDeclaration *pragma,
                                              const std::string &spelling);
void finishOpenACCFortranExactSemanticConsumption(SgPragmaDeclaration *pragma);
void consumeOpenACCFortranExactSemanticEnd(SgPragmaDeclaration *pragma);
std::vector<SgNode *> &openMPExpressionVariables();
void markOpenMPMergedEndClause(OpenMPClause *clause,
                               const std::string &source_text);
bool isOpenMPMergedEndClause(const OpenMPClause *clause);
const std::string &getOpenMPMergedEndClauseSource(const OpenMPClause *clause);

} // namespace OmpSupport

// Liao, 10/27/2008: parsing OpenMP pragma here
// Handle OpenMP pragmas. This should be called after preprocessing information
// is attached since macro calls may exist within pragmas, Liao, 3/31/2009
extern SgExpression *parseExpression(SgNode *, const char *);
extern SgExpression *parseArraySectionExpression(SgNode *, const char *);

// Fortran OpenMP parser interface
extern bool isFortranPairedDirective(OpenMPDirective *node);

bool checkOpenMPIR(OpenMPDirective *);
SgStatement *
getOpenMPBlockBody(std::pair<SgPragmaDeclaration *, OpenMPDirective *>
                       current_OpenMPIR_to_SageIII);
SgExpression *parseOmpArraySection(SgPragmaDeclaration *directive,
                                   OpenMPClauseKind clause_kind,
                                   std::string expression);
SgExpression *parseOmpExpression(SgPragmaDeclaration *directive,
                                 OpenMPClauseKind clause_kind,
                                 std::string expression);
void buildVariableList(SgOmpVariablesClause *current_omp_clause);
void parseOmpVariable(std::pair<SgPragmaDeclaration *, OpenMPDirective *>
                          current_OpenMPIR_to_SageIII,
                      OpenMPClauseKind clause_kind, std::string expression);

SgOmpParallelStatement *convertOmpParallelStatementFromCombinedDirectives(
    std::pair<SgPragmaDeclaration *, OpenMPDirective *>
        current_OpenMPIR_to_SageIII);
SgStatement *
convertOmpTaskwaitDirective(std::pair<SgPragmaDeclaration *, OpenMPDirective *>
                                current_OpenMPIR_to_SageIII);
SgStatement *
convertOmpRequiresDirective(std::pair<SgPragmaDeclaration *, OpenMPDirective *>
                                current_OpenMPIR_to_SageIII);
SgStatement *
convertNonBodyDirective(std::pair<SgPragmaDeclaration *, OpenMPDirective *>
                            current_OpenMPIR_to_SageIII);
SgStatement *
    convertDirective(std::pair<SgPragmaDeclaration *, OpenMPDirective *>);
SgStatement *convertVariantDirective(
    std::pair<SgPragmaDeclaration *, OpenMPDirective *>);
SgStatement *
    convertBodyDirective(std::pair<SgPragmaDeclaration *, OpenMPDirective *>);
SgOmpBodyStatement *convertCombinedBodyDirective(
    std::pair<SgPragmaDeclaration *, OpenMPDirective *>);
SgStatement *convertVariantBodyDirective(
    std::pair<SgPragmaDeclaration *, OpenMPDirective *>);
SgStatement *convertOmpDeclareSimdDirective(
    std::pair<SgPragmaDeclaration *, OpenMPDirective *>);
SgStatement *convertOmpDeclareVariantDirective(
    std::pair<SgPragmaDeclaration *, OpenMPDirective *>);
SgStatement *convertOmpBeginDeclareVariantDirective(
    std::pair<SgPragmaDeclaration *, OpenMPDirective *>);
SgStatement *convertOmpEndDeclareVariantDirective(
    std::pair<SgPragmaDeclaration *, OpenMPDirective *>);
SgStatement *convertOmpDeclareTargetDirective(
    std::pair<SgPragmaDeclaration *, OpenMPDirective *>);
SgStatement *convertOmpEndDeclareTargetDirective(
    std::pair<SgPragmaDeclaration *, OpenMPDirective *>);
SgStatement *convertOmpFlushDirective(
    std::pair<SgPragmaDeclaration *, OpenMPDirective *>);
SgStatement *convertOmpAllocateDirective(
    std::pair<SgPragmaDeclaration *, OpenMPDirective *>);
SgStatement *convertOmpThreadprivateStatement(
    std::pair<SgPragmaDeclaration *, OpenMPDirective *>
        current_OpenMPIR_to_SageIII);

SgOmpVariablesClause *
convertClause(SgStatement *directive,
              std::pair<SgPragmaDeclaration *, OpenMPDirective *>
                  current_OpenMPIR_to_SageIII,
              OpenMPClause *current_omp_clause);
SgOmpWhenClause *
convertWhenClause(SgOmpClauseBodyStatement *clause_body,
                  std::pair<SgPragmaDeclaration *, OpenMPDirective *>
                      current_OpenMPIR_to_SageIII,
                  OpenMPClause *current_omp_clause);
SgOmpMatchClause *
convertMatchClause(SgStatement *clause_body,
                   std::pair<SgPragmaDeclaration *, OpenMPDirective *>
                       current_OpenMPIR_to_SageIII,
                   OpenMPClause *current_omp_clause);
SgOmpAdjustArgsClause *
convertAdjustArgsClause(SgStatement *directive,
                        std::pair<SgPragmaDeclaration *, OpenMPDirective *>
                            current_OpenMPIR_to_SageIII,
                        OpenMPClause *current_omp_clause);
SgOmpAppendArgsClause *
convertAppendArgsClause(SgStatement *directive,
                        std::pair<SgPragmaDeclaration *, OpenMPDirective *>
                            current_OpenMPIR_to_SageIII,
                        OpenMPClause *current_omp_clause);
SgOmpBindClause *
convertBindClause(SgOmpClauseBodyStatement *clause_body,
                  std::pair<SgPragmaDeclaration *, OpenMPDirective *>
                      current_OpenMPIR_to_SageIII,
                  OpenMPClause *current_omp_clause);
SgOmpOrderClause *
convertOrderClause(SgStatement *directive,
                   std::pair<SgPragmaDeclaration *, OpenMPDirective *>
                       current_OpenMPIR_to_SageIII,
                   OpenMPClause *current_omp_clause);
SgOmpProcBindClause *
convertProcBindClause(SgOmpClauseBodyStatement *clause_body,
                      std::pair<SgPragmaDeclaration *, OpenMPDirective *>
                          current_OpenMPIR_to_SageIII,
                      OpenMPClause *current_omp_clause);
SgOmpDefaultClause *
convertDefaultClause(SgOmpClauseBodyStatement *clause_body,
                     std::pair<SgPragmaDeclaration *, OpenMPDirective *>
                         current_OpenMPIR_to_SageIII,
                     OpenMPClause *current_omp_clause);
SgOmpExpressionClause *
convertExpressionClause(SgStatement *directive,
                        std::pair<SgPragmaDeclaration *, OpenMPDirective *>
                            current_OpenMPIR_to_SageIII,
                        OpenMPClause *current_omp_clause);
SgOmpAllocatorClause *
convertAllocatorClause(SgOmpClauseStatement *clause_body,
                       std::pair<SgPragmaDeclaration *, OpenMPDirective *>
                           current_OpenMPIR_to_SageIII,
                       OpenMPClause *current_omp_clause);
SgOmpDependClause *
convertDependClause(SgStatement *clause_body,
                    std::pair<SgPragmaDeclaration *, OpenMPDirective *>
                        current_OpenMPIR_to_SageIII,
                    OpenMPClause *current_omp_clause);
SgOmpExtImplementationDefinedRequirementClause *
convertExtImplementationDefinedRequirementClause(
    SgStatement *directive,
    std::pair<SgPragmaDeclaration *, OpenMPDirective *>
        current_OpenMPIR_to_SageIII,
    OpenMPClause *current_omp_clause);
SgOmpAtomicDefaultMemOrderClause *convertAtomicDefaultMemOrderClause(
    SgStatement *directive,
    std::pair<SgPragmaDeclaration *, OpenMPDirective *>
        current_OpenMPIR_to_SageIII,
    OpenMPClause *current_omp_clause);
SgOmpDepobjUpdateClause *
convertDepobjUpdateClause(SgStatement *directive,
                          std::pair<SgPragmaDeclaration *, OpenMPDirective *>
                              current_OpenMPIR_to_SageIII,
                          OpenMPClause *current_omp_clause);
SgOmpAffinityClause *
convertAffinityClause(SgStatement *clause_body,
                      std::pair<SgPragmaDeclaration *, OpenMPDirective *>
                          current_OpenMPIR_to_SageIII,
                      OpenMPClause *current_omp_clause);
SgOmpMapClause *
convertMapClause(SgStatement *clause_body,
                 std::pair<SgPragmaDeclaration *, OpenMPDirective *>
                     current_OpenMPIR_to_SageIII,
                 OpenMPClause *current_omp_clause);
SgOmpDefaultmapClause *
convertDefaultmapClause(SgOmpClauseBodyStatement *clause_body,
                        std::pair<SgPragmaDeclaration *, OpenMPDirective *>
                            current_OpenMPIR_to_SageIII,
                        OpenMPClause *current_omp_clause);
SgOmpDistScheduleClause *
convertDistScheduleClause(SgOmpClauseBodyStatement *clause_body,
                          std::pair<SgPragmaDeclaration *, OpenMPDirective *>
                              current_OpenMPIR_to_SageIII,
                          OpenMPClause *current_omp_clause);
SgOmpScheduleClause *
convertScheduleClause(SgStatement *directive,
                      std::pair<SgPragmaDeclaration *, OpenMPDirective *>
                          current_OpenMPIR_to_SageIII,
                      OpenMPClause *current_omp_clause);
SgOmpUsesAllocatorsClause *
convertUsesAllocatorsClause(SgOmpClauseBodyStatement *clause_body,
                            std::pair<SgPragmaDeclaration *, OpenMPDirective *>
                                current_OpenMPIR_to_SageIII,
                            OpenMPClause *current_omp_clause);
SgOmpFromClause *
convertFromClause(SgStatement *clause_body,
                  std::pair<SgPragmaDeclaration *, OpenMPDirective *>
                      current_OpenMPIR_to_SageIII,
                  OpenMPClause *current_omp_clause);
SgOmpSizesClause *
convertSizesClause(SgStatement *directive,
                   std::pair<SgPragmaDeclaration *, OpenMPDirective *>
                       current_OpenMPIR_to_SageIII,
                   OpenMPClause *current_omp_clause);
SgOmpToClause *
convertToClause(SgStatement *clause_body,
                std::pair<SgPragmaDeclaration *, OpenMPDirective *>
                    current_OpenMPIR_to_SageIII,
                OpenMPClause *current_omp_clause);
#endif
