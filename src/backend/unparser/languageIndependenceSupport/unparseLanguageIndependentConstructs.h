
/* Language-independent unparser declarations. */

#ifndef UNPARSER_LANGUAGE_INDEPENDENT_SUPPORT
#define UNPARSER_LANGUAGE_INDEPENDENT_SUPPORT

#include <cstdio>
#include <iomanip>
#include <limits>
#include <locale>
#include <map>
#include <optional>
#include <set>
#include <sstream>
#include <string>

#include "rose_attributes_list.h"

#include "modified_sage.h"

#include "unparser.h"

class SgBasicBlock;
class SgBinaryOp;
class SgClassDefinition;
class SgConstructorInitializer;
class SgDeclarationStatement;
class SgDotExp;
class SgExpression;
class SgFile;
class SgIfStmt;
class SgInitializedName;
class SgLocatedNode;
class SgNamespaceDefinitionStatement;
class SgNamespaceDeclarationStatement;
class SgNode;
class SgOmpClause;
class SgAccClause;
class SgAccExpressionClause;
class SgAccVariablesClause;
class FrontierNode;
class SgSourceFile;
class SgStatement;
class SgThisExp;
class SgUnparse_Info;
class SgValue;
class SgValueExp;

/* support for handling precedence and associativity */
typedef int PrecedenceSpecifier;
#define ROSE_UNPARSER_NO_PRECEDENCE -1
enum AssociativitySpecifier {
  e_assoc_none = 0,
  e_assoc_right,
  e_assoc_left,
  e_assoc_last
};

class Unparser;

// This is a base class for the language dependent parts of the unparser.
class UnparseLanguageIndependentConstructs {
protected:
  Unparser *unp;
  std::string currentOutputFileName;
  std::set<const SgStatement *> statementsWithTokenEmittedLeadingPreprocessing;
  std::set<const SgStatement *> statementsWithTokenEmittedTrailingWhitespace;
  struct LineDirectiveLocation {
    std::string filename;
    unsigned int line;

    bool operator==(const LineDirectiveLocation &other) const {
      return filename == other.filename && line == other.line;
    }
  };
  std::optional<LineDirectiveLocation> previousLineDirectiveLocation;

  bool frontierRequiresPartialTokenUnparse(SgSourceFile *sourceFile,
                                           SgStatement *candidate);

public:
  enum namespace_source_fragment_state_enum {
    e_namespace_source_fragment_complete,
    e_namespace_source_fragment_introducer_only,
    e_namespace_source_fragment_open_only,
    e_namespace_source_fragment_close_only,
    e_namespace_source_fragment_neither
  };

  namespace_source_fragment_state_enum namespaceSourceFragmentState(
      const SgNamespaceDeclarationStatement *declaration,
      const SgUnparse_Info &info) const;

  // DQ (12/6/2014): This type permits specification of what bounds to use in
  // the specifiation of token stream subsequence boundaries.
  enum token_sequence_position_enum_type {
    e_leading_whitespace_start,
    e_leading_whitespace_end,
    e_token_subsequence_start,
    e_token_subsequence_end,
    e_trailing_whitespace_start,
    e_trailing_whitespace_end,
    // DQ (12/31/2014): Added to support the middle subsequence of tokens in the
    // SgIfStmt as a special case.
    e_else_whitespace_start,
    e_else_whitespace_end
  };

  // DQ (6/6/2021): Adding the support to provide offsets to modify the starting
  // and ending token sequence to unparse. Single statement specification of
  // token subsequence. void unparseStatementFromTokenStream (SgStatement* stmt,
  // token_sequence_position_enum_type e_leading_whitespace_start,
  // token_sequence_position_enum_type e_token_subsequence_start); void
  // unparseStatementFromTokenStream (SgStatement* stmt,
  // token_sequence_position_enum_type e_leading_whitespace_start,
  // token_sequence_position_enum_type e_token_subsequence_start,
  // SgUnparse_Info& info);
  void unparseStatementFromTokenStream(
      SgStatement *stmt,
      token_sequence_position_enum_type e_leading_whitespace_start,
      token_sequence_position_enum_type e_token_subsequence_start,
      SgUnparse_Info &info, int start_offset = 0, int end_offset = 0);

  // DQ (6/6/2021): Adding the support to provide offsets to modify the starting
  // and ending token sequence to unparse. Two statement specification of token
  // subsequence (required for "else" case in SgIfStmt). void
  // unparseStatementFromTokenStream (SgStatement* stmt_1, SgStatement* stmt_2,
  // token_sequence_position_enum_type e_leading_whitespace_start,
  // token_sequence_position_enum_type e_token_subsequence_start); void
  // unparseStatementFromTokenStream (SgLocatedNode* stmt_1, SgLocatedNode*
  // stmt_2, token_sequence_position_enum_type e_leading_whitespace_start,
  // token_sequence_position_enum_type e_token_subsequence_start); void
  // unparseStatementFromTokenStream (SgLocatedNode* stmt_1, SgLocatedNode*
  // stmt_2, token_sequence_position_enum_type e_leading_whitespace_start,
  //                                       token_sequence_position_enum_type
  //                                       e_token_subsequence_start, bool
  //                                       unparseOnlyWhitespace = false );
  // void unparseStatementFromTokenStream (SgLocatedNode* stmt_1, SgLocatedNode*
  // stmt_2, token_sequence_position_enum_type e_leading_whitespace_start,
  //                                       token_sequence_position_enum_type
  //                                       e_token_subsequence_start,
  //                                       SgUnparse_Info& info, bool
  //                                       unparseOnlyWhitespace = false );
  void unparseStatementFromTokenStream(
      SgLocatedNode *stmt_1, SgLocatedNode *stmt_2,
      token_sequence_position_enum_type e_leading_whitespace_start,
      token_sequence_position_enum_type e_token_subsequence_start,
      SgUnparse_Info &info, bool unparseOnlyWhitespace = false,
      int start_offset = 0, int end_offset = 0);

  // DQ (12/30/2014): Adding debugging information.
  std::string token_sequence_position_name(token_sequence_position_enum_type e);

  enum unparsed_as_enum_type {
    e_unparsed_as_error,
    e_unparsed_as_AST,
    e_unparsed_as_partial_token_sequence,
    e_unparsed_as_token_stream,
    e_unparsed_as_last
  };

  std::string unparsed_as_kind(unparsed_as_enum_type x);

  UnparseLanguageIndependentConstructs(Unparser *unp, std::string fname)
      : unp(unp) {
    currentOutputFileName = fname;
  };

  virtual ~UnparseLanguageIndependentConstructs() {};

  // Where all the language specific statement unparsing is done
  virtual void unparseLanguageSpecificStatement(SgStatement *stmt,
                                                SgUnparse_Info &info) = 0;
  virtual void unparseLanguageSpecificExpression(SgExpression *expr,
                                                 SgUnparse_Info &info) = 0;

  //! Support for unparsing of line directives into generated code to support
  //! debugging
  virtual void unparseLineDirectives(SgStatement *stmt);

  void unparseOneElemConInit(SgConstructorInitializer *con_init,
                             SgUnparse_Info &info);
  //! get the filename from the SgFile Object
  std::string getFileName();

  //! used to support the run_unparser function
  //! (support for #line 42 "filename" when it appears in source code)
  bool statementFromFile(SgStatement *stmt, std::string sourceFilename,
                         SgUnparse_Info &info);
  // bool statementFromFile ( SgStatement* stmt, std::string sourceFilename );

  //! Unparse a statement while tracking extern "C" brace state.
  void unparseStatementWithExternBraceTracking(SgStatement *stmt,
                                               SgUnparse_Info &info,
                                               size_t &extern_brace_depth,
                                               bool &extern_brace_active);

  //! Generate a CPP directive
  void outputDirective(PreprocessingInfo *directive);

  //! counts the number of statements in a basic block
  int num_stmt_in_block(SgBasicBlock *);

  std::string resBool(bool val) const;
  // DQ (7/1/2013): This needs to be defined in the header file, else the
  // GNU 4.5 and 4.6 compilers will have undefined references at link time. Note
  // that the older GNU compilers have not had any problems with the previous
  // version with the function definition in the *.C file.
  template <typename T> std::string tostring(T t) const {
    std::ostringstream myStream; // Creates an ostringstream object
    myStream << std::showpoint << t
             << std::flush; // Distinguish integer and floating-point numbers
    return myStream.str(); // Returns the string form of the stringstream object
  }
  template <typename T>
  std::string canonicalFloatingLiteral(T value,
                                       const std::string &suffix) const {
    std::ostringstream stream;
    stream.imbue(std::locale::classic());
    stream << std::defaultfloat
           << std::setprecision(std::numeric_limits<T>::max_digits10) << value;
    if (stream.fail()) {
      fprintf(stderr,
              "REX_UNPARSE_INVARIANT[floating-literal]: cannot produce a "
              "canonical round-trip spelling for a generated value\n");
      ROSE_ABORT();
    }
    std::string spelling = stream.str();
    if (spelling.find_first_of(".eE") == std::string::npos) {
      spelling += ".0";
    }
    return spelling + suffix;
  }
  void requireGeneratedCanonicalLiteralSpelling(const SgValueExp *value) const;
  SgExpression *validatedOriginalExpressionSource(SgExpression *owner,
                                                  const char *consumer) const;
  void curprint(const std::string &str) const;
  void curprintLiteral(const std::string &text) const;
  void printOutComments(SgLocatedNode *locatedNode) const;

  //! This function unparses any attached comments or CPP directives.
  virtual void unparseAttachedPreprocessingInfo(
      SgLocatedNode *stmt, SgUnparse_Info &info,
      PreprocessingInfo::RelativePositionType whereToUnparse);
  bool RemoveArgs(SgExpression *expr);

  //! Support for Fortran numeric labels (can appear on any statement), this is
  //! an empty function for C/C++.
  virtual void unparseStatementNumbers(SgStatement *stmt, SgUnparse_Info &info);

  virtual std::string languageName() const = 0;

  //! unparse expression functions implemented in unparse_expr.C
  virtual void unparseExpression(SgExpression *expr, SgUnparse_Info &info);

  // DQ (3/27/2017): Eliminate Clang warning about hidden virtual function
  // (derived class functions now use the same signature (which eliminates the
  // warning).
  virtual void unparseExprList(SgExpression *expr, SgUnparse_Info &info);

  // virtual void unparseUnaryOperator           (SgExpression* expr, const
  // char* op, SgUnparse_Info& info); virtual void unparseBinaryOperator
  // (SgExpression* expr, const char* op, SgUnparse_Info& info);
  virtual void unparseUnaryExpr(SgExpression *expr, SgUnparse_Info &info);
  virtual void unparseBinaryExpr(SgExpression *expr, SgUnparse_Info &info);

  // DQ (11/10/2005): Added general support for SgValue (so that we could
  // unparse expression trees from constant folding)
  virtual void unparseValue(SgExpression *expr, SgUnparse_Info &info);

  virtual void unparseBoolVal(SgExpression *expr, SgUnparse_Info &info);
  virtual void unparseShortVal(SgExpression *expr, SgUnparse_Info &info);
  virtual void unparseCharVal(SgExpression *expr, SgUnparse_Info &info);
  virtual void unparseSCharVal(SgExpression *expr, SgUnparse_Info &info);
  virtual void unparseUCharVal(SgExpression *expr, SgUnparse_Info &info);
  virtual void unparseWCharVal(SgExpression *expr, SgUnparse_Info &info);

  virtual void unparseChar16Val(SgExpression *expr, SgUnparse_Info &info);
  virtual void unparseChar32Val(SgExpression *expr, SgUnparse_Info &info);

  // Unparsing strings is language dependent.
  virtual void unparseStringVal(SgExpression *expr, SgUnparse_Info &info) = 0;
  virtual void unparseUShortVal(SgExpression *expr, SgUnparse_Info &info);
  virtual void unparseEnumVal(SgExpression *expr, SgUnparse_Info &info);
  virtual void unparseIntVal(SgExpression *expr, SgUnparse_Info &info);
  virtual void unparseUIntVal(SgExpression *expr, SgUnparse_Info &info);
  virtual void unparseLongIntVal(SgExpression *expr, SgUnparse_Info &info);
  virtual void unparseLongLongIntVal(SgExpression *expr, SgUnparse_Info &info);
  virtual void unparseULongLongIntVal(SgExpression *expr, SgUnparse_Info &info);
  virtual void unparseULongIntVal(SgExpression *expr, SgUnparse_Info &info);
  virtual void unparseFloatVal(SgExpression *expr, SgUnparse_Info &info);
  virtual void unparseDoubleVal(SgExpression *expr, SgUnparse_Info &info);
  virtual void unparseLongDoubleVal(SgExpression *expr, SgUnparse_Info &info);
  virtual void unparseComplexVal(SgExpression *expr, SgUnparse_Info &info);

  // DQ (7/31/2014): Adding support for C++11 nullptr const value expressions.
  virtual void unparseNullptrVal(SgExpression *expr, SgUnparse_Info &info);

  virtual void unparseNullExpression(SgExpression *expr, SgUnparse_Info &info);

  //! unparse statement functions implemented in unparse_stmt.C
  virtual void unparseStatement(SgStatement *stmt, SgUnparse_Info &info);
  virtual void unparseGlobalStmt(SgStatement *stmt, SgUnparse_Info &info);
  virtual void unparseFuncTblStmt(SgStatement *stmt, SgUnparse_Info &info);

  virtual void unparseNullStatement(SgStatement *stmt, SgUnparse_Info &info);

  virtual void unparseIncludeDirectiveStatement(SgStatement *stmt,
                                                SgUnparse_Info &info);
  virtual void unparseDefineDirectiveStatement(SgStatement *stmt,
                                               SgUnparse_Info &info);
  virtual void unparseUndefDirectiveStatement(SgStatement *stmt,
                                              SgUnparse_Info &info);
  virtual void unparseIfdefDirectiveStatement(SgStatement *stmt,
                                              SgUnparse_Info &info);
  virtual void unparseIfndefDirectiveStatement(SgStatement *stmt,
                                               SgUnparse_Info &info);
  virtual void unparseDeadIfDirectiveStatement(SgStatement *stmt,
                                               SgUnparse_Info &info);
  virtual void unparseIfDirectiveStatement(SgStatement *stmt,
                                           SgUnparse_Info &info);
  virtual void unparseElseDirectiveStatement(SgStatement *stmt,
                                             SgUnparse_Info &info);
  virtual void unparseElseifDirectiveStatement(SgStatement *stmt,
                                               SgUnparse_Info &info);
  virtual void unparseEndifDirectiveStatement(SgStatement *stmt,
                                              SgUnparse_Info &info);
  virtual void unparseLineDirectiveStatement(SgStatement *stmt,
                                             SgUnparse_Info &info);
  virtual void unparseWarningDirectiveStatement(SgStatement *stmt,
                                                SgUnparse_Info &info);
  virtual void unparseErrorDirectiveStatement(SgStatement *stmt,
                                              SgUnparse_Info &info);
  virtual void unparseEmptyDirectiveStatement(SgStatement *stmt,
                                              SgUnparse_Info &info);
  virtual void unparseIdentDirectiveStatement(SgStatement *stmt,
                                              SgUnparse_Info &info);
  virtual void unparseIncludeNextDirectiveStatement(SgStatement *stmt,
                                                    SgUnparse_Info &info);
  virtual void unparseLinemarkerDirectiveStatement(SgStatement *stmt,
                                                   SgUnparse_Info &info);
  virtual void unparseClinkageStartStatement(SgStatement *stmt,
                                             SgUnparse_Info &info);
  virtual void unparseClinkageEndStatement(SgStatement *stmt,
                                           SgUnparse_Info &info);

  // Liao 10/20/2010 common unparsing support for OpenMP AST
  virtual void unparseOmpPrefix(SgUnparse_Info &info); // = 0;
  virtual void unparseOmpDefaultClause(SgOmpClause *clause,
                                       SgUnparse_Info &info);
  virtual void unparseOmpAllocatorClause(SgOmpClause *clause,
                                         SgUnparse_Info &info);
  virtual void unparseOmpProcBindClause(SgOmpClause *clause,
                                        SgUnparse_Info &info);
  virtual void unparseOmpOrderClause(SgOmpClause *clause, SgUnparse_Info &info);
  virtual void unparseOmpBindClause(SgOmpClause *clause, SgUnparse_Info &info);
  virtual void unparseOmpWhenClause(SgOmpClause *clause, SgUnparse_Info &info);
  virtual void unparseOmpMatchClause(SgOmpClause *clause, SgUnparse_Info &info);
  virtual void unparseOmpAdjustArgsClause(SgOmpClause *clause,
                                          SgUnparse_Info &info);
  virtual void unparseOmpAppendArgsClause(SgOmpClause *clause,
                                          SgUnparse_Info &info);
  virtual void unparseOmpAtomicClause(SgOmpClause *clause,
                                      SgUnparse_Info &info);
  virtual void unparseOmpScheduleClause(SgOmpClause *clause,
                                        SgUnparse_Info &info);
  virtual void unparseOmpDistScheduleClause(SgOmpClause *clause,
                                            SgUnparse_Info &info);
  virtual void unparseOmpDefaultmapClause(SgOmpClause *clause,
                                          SgUnparse_Info &info);
  virtual void unparseOmpUsesAllocatorsClause(SgOmpClause *clause,
                                              SgUnparse_Info &info);
  virtual void unparseOmpVariablesClause(SgOmpClause *clause,
                                         SgUnparse_Info &info);
  virtual void unparseOmpVariablesComplexClause(SgOmpClause *clause,
                                                SgUnparse_Info &info);
  virtual void unparseOmpExpressionClause(SgOmpClause *clause,
                                          SgUnparse_Info &info);
  virtual void unparseOmpDirectiveKindClause(SgOmpClause *clause,
                                             SgUnparse_Info &info);
  virtual void unparseOmpDepobjUpdateClause(SgOmpClause *clause,
                                            SgUnparse_Info &info);
  virtual void unparseOmpClause(SgOmpClause *clause, SgUnparse_Info &info);
  virtual void unparseOmpAtomicDefaultMemOrderClause(SgOmpClause *clause,
                                                     SgUnparse_Info &info);
  virtual void unparseOmpSimpleStatement(SgStatement *stmt,
                                         SgUnparse_Info &info);
  virtual void unparseOmpThreadprivateStatement(SgStatement *stmt,
                                                SgUnparse_Info &info);
  virtual void unparseOmpFlushStatement(SgStatement *stmt,
                                        SgUnparse_Info &info);
  virtual void unparseOmpAllocateStatement(SgStatement *stmt,
                                           SgUnparse_Info &info);
  virtual void unparseOmpDeclareSimdStatement(SgStatement *stmt,
                                              SgUnparse_Info &info);
  virtual void unparseOmpDeclareVariantStatement(SgStatement *stmt,
                                                 SgUnparse_Info &info);
  virtual void unparseOmpBeginDeclareVariantStatement(SgStatement *stmt,
                                                      SgUnparse_Info &info);

  // This is necessary since some clauses should only appear with the begin part
  // of a directive
  virtual void unparseOmpDirectivePrefixAndName(SgStatement *stmt,
                                                SgUnparse_Info &info);
  virtual void unparseOmpEndDirectivePrefixAndName(SgStatement *stmt,
                                                   SgUnparse_Info &info);
  virtual void unparseOmpBeginDirectiveClauses(SgStatement *stmt,
                                               SgUnparse_Info &info);
  virtual void unparseOmpEndDirectiveClauses(SgStatement *stmt,
                                             SgUnparse_Info &info);
  virtual void unparseOmpGenericStatement(SgStatement *stmt,
                                          SgUnparse_Info &info);

  // OpenACC support
  virtual void unparseAccPrefix(SgUnparse_Info &info);
  virtual void unparseAccClause(SgAccClause *clause, SgUnparse_Info &info);
  virtual void unparseAccExpressionClause(SgAccExpressionClause *clause,
                                          SgUnparse_Info &info);
  virtual void unparseAccVariablesClause(SgAccVariablesClause *clause,
                                         SgUnparse_Info &info);
  virtual void unparseAccDirectivePrefixAndName(SgStatement *stmt,
                                                SgUnparse_Info &info);
  virtual void unparseAccBeginDirectiveClauses(SgStatement *stmt,
                                               SgUnparse_Info &info);
  virtual void unparseAccGenericStatement(SgStatement *stmt,
                                          SgUnparse_Info &info);

  virtual void unparseMapDistDataPoliciesToString(
      const SgOmpMapDistDataPolicyPtrList &policies, SgUnparse_Info &info);

  bool isTransformed(SgStatement *stmt);
  void markGeneratedFile() const;

  // DQ (10/14/2004): Supporting function shared by unparseClassDecl and
  // unparseClassType
  void initializeDeclarationsFromParent(
      SgDeclarationStatement *declarationStatement, SgClassDefinition *&cdefn,
      SgNamespaceDefinitionStatement *&namespaceDefn, int debugSupport = 0);

  // Support for language-independent precedence
  static constexpr PrecedenceSpecifier additiveOperatorPrecedence() {
    return 13;
  }
  virtual bool requiresParentheses(SgExpression *expr, SgUnparse_Info &info);
  virtual PrecedenceSpecifier getPrecedence(SgExpression *exp);
  virtual AssociativitySpecifier getAssociativity(SgExpression *exp);

  // DQ (4/14/2013): Added to support the mixed use of both overloaded operator
  // names and operator syntax.
  bool isRequiredOperator(SgBinaryOp *binary_op,
                          bool current_function_call_uses_operator_syntax,
                          bool parent_function_call_uses_operator_syntax);

  // DQ (10/29/2013): Adding support to unparse statements using the token
  // stream. int unparseStatementFromTokenStream(SgSourceFile* sourceFile,
  // SgStatement* stmt);

  // DQ (6/6/2021): I think this is not needed (was modified to be the version
  // of the function below).
  int unparseStatementFromTokenStream(
      SgSourceFile *sourceFile, SgStatement *stmt, SgUnparse_Info &info,
      bool &lastStatementOfGlobalScopeUnparsedUsingTokenStream);

  // DQ (11/4/2014): Unparse a partial sequence of tokens up to the next AST
  // node.
  bool canPartiallyReplayStatementTokens(SgSourceFile *sourceFile,
                                         SgStatement *stmt);

  bool canBeUnparsedFromTokenStream(SgSourceFile *sourceFile,
                                    SgStatement *stmt);

  // DQ (11/30/2013): Adding support to suppress redundant unparsing of CPP
  // directives and comments. bool
  // isTransitionFromTokenUnparsingToASTunparsing(SgStatement* statement);

  // DQ (9/3/2014): Adding support to supress output of SgThisExp as part of
  // support for C++11 lambda functions code generation.
  bool suppressImplicitObjectAccess(SgExpression *expr);

  // DQ (11/13/2014): Detect when to unparse the leading and trailing edge
  // tokens for attached CPP directives and comments.
  bool unparseAttachedPreprocessingInfoUsingTokenStream(
      SgLocatedNode *stmt, SgUnparse_Info &info,
      PreprocessingInfo::RelativePositionType whereToUnparse);

  friend void unparse_alignas(SgInitializedName *decl_item,
                              class Unparse_ExprStmt &unparser,
                              SgUnparse_Info &info);
};

#endif
