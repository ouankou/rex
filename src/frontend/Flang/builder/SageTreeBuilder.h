#ifndef ROSE_SAGE_TREE_BUILDER_H_
#define ROSE_SAGE_TREE_BUILDER_H_

#include "PosInfo.h"

#include "Tokens.h"

#include "general_language_translation.h"

#include <cstdint>
#include <optional>

#include <sstream>

#include <tuple>

// WARNING: This file has been designed to compile with -std=c++17
// This limits the use of ROSE header files at the moment.
//
class SgBasicBlock;
class SgArithmeticIfStatement;
class SgBreakStmt;
class SgCaseOptionStmt;
class SgCastExp;
class SgCommonBlock;
class SgCommonBlockObject;
class SgContainsStatement;
class SgContinueStmt;
class SgDefaultOptionStmt;
class SgDerivedTypeStatement;
class SgEnumDeclaration;
class SgEnumType;
class SgEnumVal;
class SgExpression;
class SgExprListExp;
class SgExprStatement;
class SgFunctionCallExp;
class SgFunctionDeclaration;
class SgFunctionDefinition;
class SgFunctionParameterList;
class SgFunctionParameterScope;
class SgProcedureHeaderStatement;
class SgGlobal;
class SgGotoStatement;
class SgIfStmt;
class SgImplicitStatement;
class SgInitializedName;
class SgLabelStatement;
class SgLocatedNode;
class SgModuleStatement;
class SgNamedType;
class SgNamespaceDeclarationStatement;
class SgPntrArrRefExp;
class SgPointerType;
class SgPrintStatement;
class SgProcessControlStatement;
class SgProgramHeaderStatement;
class SgReturnStmt;
class SgScopeStatement;
class SgSourceFile;
class SgSwitchStatement;
class SgType;
class SgTypedefDeclaration;
class SgVariableDeclaration;
class SgUseStatement;
class SgVarRefExp;
class SgWhileStmt;

using SgExpressionPtrList = std::vector<SgExpression *>;

namespace SageBuilder {
struct FortranBlockDataBuilderIdentity;
}

namespace Rose {
namespace builder {

enum class FortranImplicitTypeSpecKind {
  intrinsic,
  type,
  class_type,
  type_star,
  class_star
};

// This is similar to F18 Fortran::parser::SourcePosition
struct SourcePosition {
  std::string path; // replaces Fortran::parser::SourceFile
  int line, column;
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

// using SourcePositionPair = std::tuple<SourcePosition, SourcePosition>;
// using SourcePositions    = std::tuple<SourcePosition, SourcePosition,
// SourcePosition>;
using SourcePositionPair = std::tuple<SourcePosition, SourcePosition>;
using SourcePositions =
    std::tuple<SourcePosition, SourcePosition, SourcePosition>;

SgGlobal *initialize_global_scope(SgSourceFile *file);

// Create a builder class that does nothing
class SageTreeBuilderNull {
public:
  // Default action for a sage tree node is to do nothing.
  template <typename T, typename... Options> void Enter(T *&, Options...) {}
  template <typename T> void Leave(T *) {}
};

class SageTreeBuilder {
public:
  enum class LanguageEnum { Fortran };
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

  // C++11: disallow default constructor, ...
  SageTreeBuilder() = delete;
  SageTreeBuilder(const SageTreeBuilder &) = delete;
  SageTreeBuilder &operator=(const SageTreeBuilder &) = delete;
  SageTreeBuilder(SageTreeBuilder &&) = delete;
  SageTreeBuilder &operator=(SageTreeBuilder &&) = delete;

  SageTreeBuilder(SgSourceFile *source, LanguageEnum language,
                  std::istringstream &tokens);
  ~SageTreeBuilder();

  // WARNING: this constructor requires source_ to be set before usage (called
  // by flang main)
  SageTreeBuilder(LanguageEnum language)
      : language_{language}, source_{nullptr} {
    // Sort out how to get token stream from flang
    tokens_ = new TokenStream(iss_empty_);
  }
  void setSourceFile(SgSourceFile *source) { source_ = source; }
  SgSourceFile *getSourceFile() const { return source_; }
  void setTokens(std::vector<Token> tokens);

  const TokenStream &getTokens() { return *tokens_; }

  const std::map<const std::string, SgLabelStatement *> &getLabels() {
    return labels_;
  }

  // Default action for a sage tree node is to do nothing.
  template <typename T> void Enter(T *&) {}
  template <typename T> void Leave(T *) {}

  void Enter(SgScopeStatement *&);
  void Leave(SgScopeStatement *);

  void Enter(SgBasicBlock *&);
  void Leave(SgBasicBlock *);

  void Enter(SgProgramHeaderStatement *&, const std::optional<std::string> &,
             const std::vector<std::string> &, const SourcePositions &,
             std::vector<Rose::builder::Token> &);
  void Leave(SgProgramHeaderStatement *);

  void setFortranEndProgramStmt(SgProgramHeaderStatement *,
                                const std::optional<std::string> &,
                                const std::optional<std::string> &);
  void attachFortranProcedureCanonical(SgProcedureHeaderStatement *,
                                       SgScopeStatement *,
                                       const SourcePosition &,
                                       const SourcePosition &);
  void
  setFortranProcedureDeclarationSourcePosition(SgProcedureHeaderStatement *,
                                               const SourcePosition &,
                                               const SourcePosition &);

  void Enter(SgFunctionParameterList *&, SgScopeStatement *&,
             const std::string &, SgType *, bool,
             const SourcePositions *defining_sources = nullptr);
  void Leave(SgFunctionParameterList *, SgScopeStatement *,
             const std::list<SgVariableSymbol *> &);

  void Enter(SgFunctionDeclaration *&, const std::string &, SgType *,
             SgFunctionParameterList *, SgFunctionDefinition *,
             const LanguageTranslation::FunctionModifierList &, bool,
             const SourcePositions &, std::vector<Rose::builder::Token> &,
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
  void Leave(SgDerivedTypeStatement *,
             std::list<LanguageTranslation::ExpressionKind> &);

  void Enter(SgVariableDeclaration *&, const std::string &, SgType *,
             SgExpression *);
  void Enter(
      SgVariableDeclaration *&, SgType *,
      std::list<std::tuple<std::string, SgType *, SgType *, SgExpression *>> &);
  void Leave(SgVariableDeclaration *);
  void Leave(SgVariableDeclaration *,
             std::list<LanguageTranslation::ExpressionKind> &);

  void Enter(SgEnumDeclaration *&, const std::string &);
  void Leave(SgEnumDeclaration *);

  void Enter(SgEnumVal *&, const std::string &, SgEnumDeclaration *,
             std::int64_t, const SourcePositionPair &,
             SgCastExp *cast = nullptr);

  void Enter(SgTypedefDeclaration *&, const std::string &, SgType *);
  void Leave(SgTypedefDeclaration *);

  // Statements
  //
  void Enter(SgNamespaceDeclarationStatement *&, const std::string &,
             const SourcePositionPair &);
  void Leave(SgNamespaceDeclarationStatement *);

  void Enter(SgExprStatement *&, SgExpression *&,
             const std::vector<SgExpression *> &);
  void Leave(SgExprStatement *, std::vector<std::string> &);

  void Enter(SgContainsStatement *&);
  void Leave(SgContainsStatement *);

  void Enter(SgContinueStmt *&);
  void Leave(SgContinueStmt *, const std::vector<std::string> &);
  void Enter(SgBreakStmt *&);
  void Leave(SgBreakStmt *, const std::vector<std::string> &);

  void Enter(SgFortranContinueStmt *&);
  void Leave(SgFortranContinueStmt *, const std::vector<std::string> &);

  void Leave(SgGotoStatement *, const std::vector<std::string> &);

  void Enter(SgIfStmt *&, SgExpression *, SgBasicBlock *, SgBasicBlock *,
             std::vector<Rose::builder::Token> &, bool is_ifthen = false,
             bool has_end_stmt = false, bool is_else_if = false);
  void Leave(SgIfStmt *);
  void Leave(SgIfStmt *, const std::vector<std::string> &);
  void Leave(SgArithmeticIfStatement *, const std::vector<std::string> &);

  void Enter(SgLabelStatement *&, const std::string &label);
  void Leave(SgLabelStatement *, const std::vector<std::string> &);

  void Enter(SgProcessControlStatement *&, const std::string &,
             const std::optional<SgExpression *> &,
             const std::optional<SgExpression *> &quiet = std::nullopt);
  void Leave(SgProcessControlStatement *, const std::vector<std::string> &);

  void Enter(SgSwitchStatement *&, SgExpression *, const SourcePositionPair &);
  void Leave(SgSwitchStatement *);

  void Enter(SgReturnStmt *&, const std::optional<SgExpression *> &);
  void Leave(SgReturnStmt *);
  void Leave(SgReturnStmt *, const std::vector<std::string> &);

  void Enter(SgCaseOptionStmt *&, SgExprListExp *);
  void Leave(SgCaseOptionStmt *);

  void Enter(SgDefaultOptionStmt *&);
  void Leave(SgDefaultOptionStmt *);

  void Enter(SgFortranDo *&, SgExpression *init = nullptr,
             SgExpression *bound = nullptr, SgExpression *increment = nullptr);
  void Leave(SgFortranDo *);

  void Enter(SgPrintStatement *&, SgExpression *, std::list<SgExpression *> &);
  void Leave(SgPrintStatement *);
  void Leave(SgPrintStatement *, const std::vector<std::string> &);
  void Leave(SgFormatStatement *, const std::vector<std::string> &);

  void Enter(SgWhileStmt *&, SgExpression *);
  void Leave(SgWhileStmt *, bool has_end_do_stmt = false);

  void Enter(SgImplicitStatement *&implicit_stmt, bool none_external = false,
             bool none_type = false);
  void
  Enter(SgImplicitStatement *&,
        std::list<std::tuple<SgType *, SgSymbol *, FortranImplicitTypeSpecKind,
                             std::list<std::tuple<char, std::optional<char>>>>>
            &);
  void Leave(SgImplicitStatement *);

  void Enter(SgModuleStatement *&, const std::string &);
  void Leave(SgModuleStatement *);

  void Enter(SgUseStatement *&, const std::string &, const std::string &);
  void Leave(SgUseStatement *);

  // Expressions
  //
  void Enter(SgCastExp *&, const std::string &name, SgExpression *cast_operand);

  // Fortran specific nodes
  //
  void Enter(SgCommonBlock *&, std::list<SgCommonBlockObject *> &);
  void Leave(SgCommonBlock *common_block);

private:
  enum class ExactSourceInput {
    source_spelled,
    generated_semantic_anchor,
  };

  LanguageEnum language_;
  SgSourceFile *source_;
  TokenStream *tokens_;
  TraversalContext context_;
  std::istringstream iss_empty_{};
  std::map<const std::string, SgLabelStatement *> labels_;

  SgStatement *wrapStmtWithLabels(SgStatement *stmt,
                                  const std::vector<std::string> &labels);

  void setExactSourcePosition(SgLocatedNode *node, const SourcePosition &start,
                              const SourcePosition &end, bool attach_comments,
                              ExactSourceInput input);

public:
  bool is_Fortran_language() { return (language_ == LanguageEnum::Fortran); }

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
  void attachComments(SgLocatedNode *node, const std::vector<Token> &tokens,
                      bool at_end = false);
  void attachComments(SgLocatedNode *node, std::vector<Token> &tokens,
                      const PosInfo &pos);
  void attachComments(SgExpressionPtrList const &list);
  void attachRemainingComments(SgLocatedNode *node);
  void consumePrecedingComments(std::vector<Token> &tokens, const PosInfo &pos);
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

// Temporary wrappers for SageInterface functions (needed until ROSE builds with
// C++17)
//
namespace SageBuilderCpp17 {

// Types
SgType *buildBoolType();
SgType *buildIntType();
SgType *buildFloatType();
SgType *buildCharType();
SgType *buildDoubleType();
SgType *buildComplexType(SgType *base_type = nullptr);
SgType *buildBoolType(SgExpression *kind_expr);
SgType *buildIntType(SgExpression *kind_expr);
SgType *buildFloatType(SgExpression *kind_expr);
SgType *buildStringType(SgExpression *stringLengthExpression);
SgType *buildArrayType(SgType *base_type,
                       std::list<SgExpression *> &explicit_shape_list);

// SgBasicBlock
SgBasicBlock *buildBasicBlock_nfi();
void pushScopeStack(SgBasicBlock *stmt);
void popScopeStack();

// Operators
SgExpression *buildAddOp_nfi(SgExpression *lhs, SgExpression *rhs,
                             SgType *result_type);
SgExpression *buildAndOp_nfi(SgExpression *lhs, SgExpression *rhs,
                             SgType *result_type);
SgExpression *buildConcatenationOp_nfi(SgExpression *lhs, SgExpression *rhs,
                                       SgType *result_type);
SgExpression *buildDivideOp_nfi(SgExpression *lhs, SgExpression *rhs,
                                SgType *result_type);
SgExpression *buildEqualityOp_nfi(SgExpression *lhs, SgExpression *rhs,
                                  SgType *result_type);
SgExpression *buildLessThanOp_nfi(SgExpression *lhs, SgExpression *rhs,
                                  SgType *result_type);
SgExpression *buildLessOrEqualOp_nfi(SgExpression *lhs, SgExpression *rhs,
                                     SgType *result_type);
SgExpression *buildGreaterThanOp_nfi(SgExpression *lhs, SgExpression *rhs,
                                     SgType *result_type);
SgExpression *buildGreaterOrEqualOp_nfi(SgExpression *lhs, SgExpression *rhs,
                                        SgType *result_type);
SgExpression *buildMultiplyOp_nfi(SgExpression *lhs, SgExpression *rhs,
                                  SgType *result_type);
SgExpression *buildNotEqualOp_nfi(SgExpression *lhs, SgExpression *rhs,
                                  SgType *result_type);
SgExpression *buildOrOp_nfi(SgExpression *lhs, SgExpression *rhs,
                            SgType *result_type);
SgExpression *buildMinusOp_nfi(SgExpression *i, SgType *result_type,
                               bool is_prefix = true);

// Expressions
SgExpression *buildBoolValExp_nfi(bool value);
SgExpression *buildIntVal_nfi(int);
SgExpression *buildStringVal_nfi(std::string);
SgExpression *buildFloatVal_nfi(const std::string &);
SgExpression *buildComplexVal_nfi(SgExpression *real_value,
                                  SgExpression *imaginary_value,
                                  SgType *precision_type,
                                  const std::string &str);
SgExpression *buildExprListExp_nfi();
SgExpression *buildSubtractOp_nfi(SgExpression *lhs, SgExpression *rhs,
                                  SgType *result_type);
SgExpression *buildSubscriptExpression_nfi(SgExpression *lower_bound,
                                           SgExpression *upper_bound,
                                           SgExpression *stride);
SgPntrArrRefExp *buildPntrArrRefExp_nfi(SgExpression *lhs, SgExpression *rhs,
                                        SgType *result_type);
SgExpression *buildAggregateInitializer_nfi(SgExprListExp *initializers,
                                            SgType *type = nullptr);
SgExpression *buildAsteriskShapeExp_nfi();
SgExpression *buildAssumedRankExp_nfi();
SgExpression *
buildNullExpression_nfi(SgNullExpression::null_expression_role_enum role);
SgExpression *buildFunctionCallExp(SgFunctionCallExp *);
SgExprListExp *buildExprListExp_nfi(const std::list<SgExpression *> &);

// Other
SgCommonBlockObject *buildCommonBlockObject(std::string name = "",
                                            SgExprListExp *expr_list = nullptr);
// Non builder helper functions
void set_false_body(SgIfStmt *&if_stmt, SgBasicBlock *false_body);
void set_need_paren(SgExpression *&expr);

} // namespace SageBuilderCpp17
} // namespace builder
} // namespace Rose

#endif // ROSE_SAGE_TREE_BUILDER_H_
