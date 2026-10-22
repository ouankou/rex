#ifndef ROSE_SAGE_TREE_BUILDER_H_
#define ROSE_SAGE_TREE_BUILDER_H_

#include "PosInfo.h"

#include "Tokens.h"

#include "Cxx_Grammar.h"
#include "general_language_translation.h"

#include <cstddef>
#include <cstdint>
#include <optional>

#include <ostream>

#include <tuple>

namespace SageBuilder {
struct FortranBlockDataBuilderIdentity;
}

struct EntityDeclInfo {
  std::string name;
  SgType *semantic_type;
  SgType *source_type;
  SgExpression *initializer;
  SgExpression *procedure_interface = nullptr;
  SgInitializedName *prior_declaration = nullptr;
  SgInitializedName::fortran_type_spec_enum type_spec =
      SgInitializedName::e_fortran_type_spec_default;
  SgSymbol *source_derived_type_symbol = nullptr;
};

namespace Rose {
namespace builder {

enum class ProcessControlKind { stop, error_stop, fail_image, pause };

struct HeaderComments {
  std::optional<PosInfo> range;
  std::vector<Token> tokens;
};

// This is similar to F18 Fortran::parser::SourcePosition
struct SourcePosition {
  std::string path; // replaces Fortran::parser::SourceFile
  int line, column;
  std::optional<std::size_t> cooked_source_order;
  friend std::ostream &operator<<(std::ostream &os, const SourcePosition &sp) {
    os << sp.line << ',' << sp.column;
    return os;
  }
};

struct TraversalContext {
  TraversalContext()
      : type(nullptr), is_initialization(false),
        actual_function_param_scope(nullptr) {}
  SgType *type;
  bool is_initialization;
  SgScopeStatement *actual_function_param_scope;
};

using SourcePositionPair = std::tuple<SourcePosition, SourcePosition>;
using SourcePositions =
    std::tuple<SourcePosition, SourcePosition, SourcePosition>;

SgGlobal *initialize_global_scope(SgSourceFile *file);

class SageTreeBuilder {
public:
  enum class GeneratedSourceAnchorKind {
    program_canonical_declaration,
    procedure_canonical_declaration,
    enum_canonical_declaration,
    use_associated_type_declaration,
    use_associated_type_canonical_declaration,
    use_associated_type_definition,
    use_associated_object_declaration,
    use_associated_object_name,
    use_associated_variable_definition,
    use_associated_procedure_entity_declaration,
    use_associated_procedure_entity_name,
    syntactic_absence_expression
  };

  SageTreeBuilder() : source_{nullptr}, tokens_{std::vector<Token>{}} {}
  SageTreeBuilder(const SageTreeBuilder &) = delete;
  SageTreeBuilder &operator=(const SageTreeBuilder &) = delete;
  SageTreeBuilder(SageTreeBuilder &&) = delete;
  SageTreeBuilder &operator=(SageTreeBuilder &&) = delete;

  // Flang sets the source file before constructing its AST.
  void setSourceFile(SgSourceFile *source) { source_ = source; }
  SgSourceFile *getSourceFile() const { return source_; }
  void setTokens(std::vector<Token> tokens);

  void Enter(SgScopeStatement *&);
  void Leave(SgScopeStatement *);

  void Enter(SgBasicBlock *&);
  void Leave(SgBasicBlock *);

  void Enter(SgProgramHeaderStatement *&, const std::optional<std::string> &,
             std::optional<int>, const SourcePositions &, HeaderComments &);
  void Leave(SgProgramHeaderStatement *);

  void setFortranEndProgramStmt(SgProgramHeaderStatement *,
                                const std::optional<std::string> &,
                                std::optional<int>);
  void attachFortranProcedureCanonical(SgProcedureHeaderStatement *,
                                       SgScopeStatement *,
                                       const SourcePosition &,
                                       const SourcePosition &);
  void
  setFortranProcedureDeclarationSourcePosition(SgProcedureHeaderStatement *,
                                               const SourcePosition &,
                                               const SourcePosition &);

  void Enter(SgFunctionParameterList *&, SgScopeStatement *&,
             const std::string &, bool,
             const SourcePositions *defining_sources = nullptr);
  void Leave(SgFunctionParameterList *, SgScopeStatement *,
             const std::list<SgVariableSymbol *> &);

  void Enter(SgFunctionDeclaration *&, const std::string &, SgType *,
             SgFunctionParameterList *, SgFunctionDefinition *,
             const LanguageTranslation::FunctionModifierList &, bool,
             const SourcePositions &, HeaderComments &, bool from_module_file,
             SgProcedureHeaderStatement *canonical_nondefining = nullptr,
             const SageBuilder::FortranBlockDataBuilderIdentity
                 *block_data_identity = nullptr);
  void Leave(SgFunctionDeclaration *, SgScopeStatement *);
  void Leave(SgFunctionDeclaration *, SgScopeStatement *, bool,
             SgVariableSymbol *exact_result_symbol = nullptr);

  void Enter(SgFunctionDefinition *&);
  void Leave(SgFunctionDefinition *);

  void Enter(SgDerivedTypeStatement *&, const std::string &);
  void Leave(SgDerivedTypeStatement *);

  void Enter(SgVariableDeclaration *&, const std::string &, SgType *,
             SgExpression *);
  void
  Enter(SgVariableDeclaration *&, std::list<EntityDeclInfo> &,
        const std::optional<SgInitializedName::FortranProcedureComponentBinding>
            &binding = std::nullopt);
  void Leave(SgVariableDeclaration *,
             std::list<LanguageTranslation::ExpressionKind> &);

  void Enter(SgEnumDeclaration *&, const std::string &);
  void Leave(SgEnumDeclaration *);

  // Statements
  //
  void Enter(SgExprStatement *&, SgExpression *&,
             const std::vector<SgExpression *> &);
  void Leave(SgExprStatement *, std::optional<int>);

  void Enter(SgContainsStatement *&);
  void Leave(SgContainsStatement *);

  void Enter(SgContinueStmt *&);
  void Leave(SgContinueStmt *, std::optional<int>);
  void Enter(SgBreakStmt *&);
  void Leave(SgBreakStmt *, std::optional<int>);

  void Enter(SgFortranContinueStmt *&);
  void Leave(SgFortranContinueStmt *, std::optional<int>);

  void Leave(SgGotoStatement *, std::optional<int>);

  void Enter(SgIfStmt *&, SgExpression *, SgBasicBlock *, bool is_ifthen,
             bool has_end_stmt, bool is_else_if);
  void Leave(SgIfStmt *);
  void Leave(SgIfStmt *, std::optional<int>);
  void Leave(SgArithmeticIfStatement *, std::optional<int>);

  void Enter(SgProcessControlStatement *&, ProcessControlKind,
             const std::optional<SgExpression *> &,
             const std::optional<SgExpression *> &quiet = std::nullopt);
  void Leave(SgProcessControlStatement *, std::optional<int>);

  void Enter(SgReturnStmt *&, const std::optional<SgExpression *> &);
  void Leave(SgReturnStmt *, std::optional<int>);

  void Enter(SgFortranDo *&, SgExpression *init = nullptr,
             SgExpression *bound = nullptr, SgExpression *increment = nullptr);
  void Leave(SgFortranDo *);

  void Leave(SgPrintStatement *, std::optional<int>);
  void Leave(SgFormatStatement *, std::optional<int>);

  void Enter(SgWhileStmt *&, SgExpression *);
  void Leave(SgWhileStmt *);

  void Enter(SgModuleStatement *&, const std::string &);
  void Leave(SgModuleStatement *);

  void Leave(SgUseStatement *);

  // Fortran specific nodes
  //
  void Enter(SgCommonBlock *&, std::list<SgCommonBlockObject *> &);
  void Leave(SgCommonBlock *common_block);

private:
  enum class ExactSourceInput {
    source_spelled,
    generated_semantic_anchor,
  };

  SgSourceFile *source_;
  TokenStream tokens_;
  TraversalContext context_;

  void setFortranNumericLabel(SgStatement *stmt, std::optional<int> label);

  void setExactSourcePosition(SgLocatedNode *node, const SourcePosition &start,
                              const SourcePosition &end, bool attach_comments,
                              ExactSourceInput input);

public:
  const TraversalContext &get_context(void) { return context_; }
  void setContext(SgType *type) { context_.type = type; }
  void setActualFunctionParameterScope(SgScopeStatement *scope) {
    context_.actual_function_param_scope = scope;
  }

  void setInitializationContext(bool flag) {
    context_.is_initialization = flag;
  }
  bool isInitializationContext() { return context_.is_initialization; }

  void attachComments(SgLocatedNode *node, bool at_end = false);
  void attachComments(SgLocatedNode *node, const PosInfo &pos,
                      bool at_end = false);
  void attachComments(SgLocatedNode *node, HeaderComments &comments);
  void attachRemainingComments(SgLocatedNode *node);
  void captureHeaderComments(HeaderComments &comments, const PosInfo &range);
  void setSourcePosition(SgLocatedNode *node, const SourcePosition &start,
                         const SourcePosition &end);
  void setSourcePosition(SgPragma *pragma, const SourcePosition &start,
                         const SourcePosition &end);
  void setGeneratedSourcePosition(SgLocatedNode *node,
                                  const SourcePosition &start,
                                  const SourcePosition &end,
                                  GeneratedSourceAnchorKind kind);

  SgScopeStatement *popScopeStack(bool attach_comments = false);

  // Helper function
  bool
  list_contains(const std::list<LanguageTranslation::FunctionModifier> &lst,
                const LanguageTranslation::FunctionModifier &item) {
    return (std::find(lst.begin(), lst.end(), item) != lst.end());
  }
};

namespace SageBuilderCpp17 {

// Operators
SgExpression *buildMinusOp_nfi(SgExpression *i, SgType *result_type,
                               bool is_prefix = true);

// Expressions
SgExpression *buildSubscriptExpression_nfi(SgExpression *lower_bound,
                                           SgExpression *upper_bound,
                                           SgExpression *stride);
SgPntrArrRefExp *buildPntrArrRefExp_nfi(
    SgExpression *lhs, SgExpression *rhs, SgType *result_type,
    std::optional<long long> result_size = std::nullopt,
    const std::vector<std::optional<long long>> &strides = {});
SgExpression *buildAsteriskShapeExp_nfi();
SgExpression *buildAssumedRankExp_nfi();
SgExpression *
buildNullExpression_nfi(SgNullExpression::null_expression_role_enum role);
SgExprListExp *buildExprListExp_nfi(const std::list<SgExpression *> &);

// Other
SgCommonBlockObject *buildCommonBlockObject(std::string name = "",
                                            SgExprListExp *expr_list = nullptr);
// Non builder helper functions
void set_need_paren(SgExpression *&expr);

} // namespace SageBuilderCpp17
} // namespace builder
} // namespace Rose

#endif // ROSE_SAGE_TREE_BUILDER_H_
