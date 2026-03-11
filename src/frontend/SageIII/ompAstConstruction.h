#ifndef _OMP_AST_CONSTRUCTION
#define _OMP_AST_CONSTRUCTION

#include "AstSimpleProcessing.h"

#include "OpenACCIR.h"

#include "OpenMPIR.h"

#include <string>
#include <utility>
#include <vector>

inline constexpr char kOmpDeclareVariantRegionsAttributeName[] =
    "omp_declare_variant_regions";

struct OmpDeclareVariantRegionInfo {
  unsigned begin_line = 0;
  unsigned end_line = 0;
  std::string captured_region;
};

class OmpDeclareVariantRegionsAttribute : public AstAttribute {
public:
  OmpDeclareVariantRegionsAttribute() = default;

  explicit OmpDeclareVariantRegionsAttribute(
      std::vector<OmpDeclareVariantRegionInfo> regions)
      : regions_(std::move(regions)) {}

  AstAttribute *copy() const override {
    return new OmpDeclareVariantRegionsAttribute(*this);
  }

  OwnershipPolicy getOwnershipPolicy() const override {
    return CONTAINER_OWNERSHIP;
  }

  std::string toString() override { return "omp_declare_variant_regions"; }

  const std::vector<OmpDeclareVariantRegionInfo> &regions() const {
    return regions_;
  }

  void addRegion(OmpDeclareVariantRegionInfo region) {
    regions_.push_back(std::move(region));
  }

  const OmpDeclareVariantRegionInfo *findByBeginLine(unsigned line) const {
    for (const OmpDeclareVariantRegionInfo &region : regions_) {
      if (region.begin_line == line) {
        return &region;
      }
    }
    return nullptr;
  }

private:
  std::vector<OmpDeclareVariantRegionInfo> regions_;
};

namespace OmpSupport {
class SgVarRefExpVisitor : public AstSimpleProcessing {
private:
  std::vector<SgExpression *> expressions;

public:
  SgVarRefExpVisitor();
  std::vector<SgExpression *> get_expressions();
  void visit(SgNode *node);
};

void processOpenMP(SgSourceFile *sageFilePtr);

} // namespace OmpSupport

extern std::vector<std::pair<std::string, SgNode *>> omp_variable_list;
extern std::map<SgSymbol *,
                std::vector<std::pair<SgExpression *, SgExpression *>>>
    array_dimensions;
extern OpenMPDirective *parseOpenMP(const char *, OpenMPExprParseCallback,
                                    void *);
extern OpenACCDirective *parseOpenACC(std::string);

extern bool checkOpenACCIR(OpenACCDirective *);
extern SgStatement *convertOpenACCDirective(
    std::pair<SgPragmaDeclaration *, OpenACCDirective *>);

// Liao, 10/27/2008: parsing OpenMP pragma here
// Handle OpenMP pragmas. This should be called after preprocessing information
// is attached since macro calls may exist within pragmas, Liao, 3/31/2009
extern int omp_exprparser_parse();
extern SgExpression *parseExpression(SgNode *, bool, const char *);
extern SgExpression *parseArraySectionExpression(SgNode *, bool, const char *);
extern void omp_exprparser_parser_init(SgNode *aNode, const char *str);

// Fortran OpenMP parser interface
extern void parseOpenMPFortran(SgSourceFile *);
extern bool isFortranPairedDirective(OpenMPDirective *node);

bool checkOpenMPIR(OpenMPDirective *);
SgStatement *
getOpenMPBlockBody(std::pair<SgPragmaDeclaration *, OpenMPDirective *>
                       current_OpenMPIR_to_SageIII);
SgExpression *
parseOmpArraySection(SgPragmaDeclaration *directive,
                     OpenMPClauseKind clause_kind, std::string expression,
                     const std::string *directive_source_text = nullptr);
SgExpression *
parseOmpExpression(SgPragmaDeclaration *directive, OpenMPClauseKind clause_kind,
                   std::string expression,
                   const std::string *directive_source_text = nullptr);
void buildVariableList(SgOmpVariablesClause *current_omp_clause);
void parseOmpVariable(std::pair<SgPragmaDeclaration *, OpenMPDirective *>
                          current_OpenMPIR_to_SageIII,
                      OpenMPClauseKind clause_kind, std::string expression,
                      const std::string *directive_source_text = nullptr);

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
convertDepobjUpdateClause(SgOmpClauseBodyStatement *clause_body,
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
