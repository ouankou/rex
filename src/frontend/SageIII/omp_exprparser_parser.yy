/* OpenMP C and C++ Grammar */
/* Author: Markus Schordan, 2003 */
/* Modified by Christian Biesinger 2006 for OpenMP 2.0 */
/* Modified by Chunhua Liao for OpenMP 3.0, 2008 */
/* Updated by Chunhua Liao for OpenMP 4.5,  2017 */

/*
To debug bison conflicts, use the following command line in the build tree

/bin/sh ../../../../sourcetree/config/ylwrap
../../../../sourcetree/src/frontend/Sab.h `echo expression_parser.cc | sed -e
s/cc$/hh/ -e s/cpp$/hpp/ -e s/cxx$/hxx/ -e s/c++$/h++/ -e s/c$/h/` y.output
expression_parser.output -- bison -y -d -r state in the build tree
*/
%define api.prefix {omp_exprparser_}
%defines
%define parse.error verbose

%{
/* DQ (2/10/2014): IF is conflicting with template IF. */
#undef IF

#include "ompAstConstruction.h"
#include "sage3basic.h" // Sage Interface and Builders
#include "sageBuilder.h"
#include <algorithm>
#include <assert.h>
#include <cctype>
#include <cerrno>
#include <climits>
#include <cstdlib>
#include <iostream>
#include <limits>
#include <memory>
#include <set>
#include <stdio.h>
#include <vector>

  using namespace OmpSupport;
  using namespace SageInterface;

  /* Parser - BISON */

  /*the scanner function*/
  extern int omp_exprparser_lex();

  /*A customized initialization function for the scanner, str is the string to
   * be scanned.*/
  extern void omp_exprparser_lexer_init(const char *str);
  extern void omp_exprparser_lexer_finish();
  extern bool omp_exprparser_lexer_is_clean();

  extern SgExpression *parseExpression(SgNode *, const char *);
  extern SgExpression *parseArraySectionExpression(SgNode *, const char *);

  static int omp_exprparser_error(const char *);

  // The context node with the pragma annotation being parsed
  //
  // We attach the attribute to the pragma declaration directly for now,
  // A few OpenMP directive does not affect the next structure block
  // This variable is set by the prefix_parser_init() before prefix_parse() is
  // called.
  // Liao
  static SgNode *omp_directive_node;

  static const char *orig_str;

  // The current expression node being generated
  static SgExpression *current_exp = NULL;
  // a flag to indicate if the program is looking forward in the symbol table
  static std::map<std::string, SgVariableSymbol *>
      omp_exprparser_context_variable_symbols;
  static const OpenACCCxxExactSemanticBindings::ExpressionBindings
      *omp_exprparser_openacc_cxx_semantic_bindings = NULL;
  static size_t omp_exprparser_exact_semantic_binding_index = 0;
  static const std::vector<OmpFortranExactSemanticBindings::Binding>
      *omp_exprparser_fortran_exact_semantic_bindings = NULL;
  static size_t omp_exprparser_fortran_exact_semantic_binding_index = 0;
  static const std::vector<OmpExactSubexpressionType>
      *omp_exprparser_exact_subexpression_types = NULL;
  static size_t omp_exprparser_exact_subexpression_type_index = 0;
  static SgScopeStatement *omp_exprparser_fortran_typed_scope = NULL;
  static SgType *omp_exprparser_fortran_default_integer_type = NULL;
  static std::vector<std::unique_ptr<OmpFortranExactSemanticBindings::Binding>>
      omp_exprparser_fortran_typed_scope_bindings;
  static std::set<std::string> omp_exprparser_context_name_expressions;

  // We now follow the OpenMP 4.0 standard's C-style array section syntax:
  // [lower-bound:length] or just [length] the latest variable symbol being
  // parsed, used to help parsing the array dimensions associated with array
  // symbol such as a[0:n][0:m]
  static SgVariableSymbol *array_symbol;
  static SgExpression *lower_exp = NULL;
  static SgExpression *length_exp = NULL;
  // check if the parsed a[][] is an array element access a[i][j] or array
  // section a[lower:length][lower:length]
  //
  static bool arraySection = true;
  // Grammar entry-state flags.  Successful reductions must restore both before
  // the wrapper releases the non-reentrant parser invocation.
  static bool is_ompparser_variable = false;
  static bool is_ompparser_expression = false;

  static void requireIdleOpenMPExpressionParser(const char *operation) {
    OmpSupport::requireOpenMPConversionSession();
    if (operation == NULL || omp_directive_node != NULL || orig_str != NULL ||
        current_exp != NULL || array_symbol != NULL || lower_exp != NULL ||
        length_exp != NULL || !arraySection || is_ompparser_variable ||
        is_ompparser_expression || !omp_exprparser_lexer_is_clean()) {
      std::cerr << "REX_OMP_AST_INVARIANT[expression-parser-lifecycle]: "
                << (operation != NULL ? operation : "<null>")
                << " requires an idle OpenMP expression parser\n";
      ROSE_ABORT();
    }
  }

  static void beginOpenMPExpressionParse(SgNode * directive, const char *str) {
    requireIdleOpenMPExpressionParser("parse entry");
    if (directive == NULL || str == NULL || str[0] == '\0') {
      std::cerr << "REX_OMP_AST_INVARIANT[expression-parser-input]: OpenMP "
                   "expression parse requires a directive and nonempty "
                   "source text\n";
      ROSE_ABORT();
    }
    orig_str = str;
    omp_directive_node = directive;
    omp_exprparser_lexer_init(str);
  }

  static SgExpression *finishOpenMPExpressionParse(int parse_status) {
    if (parse_status != 0 || omp_directive_node == NULL || orig_str == NULL ||
        is_ompparser_variable || is_ompparser_expression) {
      std::cerr << "REX_OMP_AST_INVARIANT[expression-parser-lifecycle]: "
                   "OpenMP expression parser did not complete its exact "
                   "grammar state\n";
      ROSE_ABORT();
    }
    SgExpression *result = current_exp;
    omp_exprparser_lexer_finish();
    omp_directive_node = NULL;
    orig_str = NULL;
    current_exp = NULL;
    array_symbol = NULL;
    lower_exp = NULL;
    length_exp = NULL;
    arraySection = true;
    return result;
  }

  static void requireOpenMPGeneratedExpressionSource(SgExpression * expression,
                                                     const char *producer) {
    Sg_File_Info *primary =
        expression != NULL ? expression->get_file_info() : NULL;
    Sg_File_Info *start =
        expression != NULL ? expression->get_startOfConstruct() : NULL;
    Sg_File_Info *end =
        expression != NULL ? expression->get_endOfConstruct() : NULL;
    Sg_File_Info *operator_position =
        expression != NULL ? expression->get_operatorPosition() : NULL;
    auto exact = [expression](Sg_File_Info *info) {
      return info != NULL && info->get_parent() == expression &&
             info->isTransformation() && info->isOutputInCodeGeneration();
    };
    if (producer == NULL || !exact(primary) || !exact(start) || !exact(end) ||
        !exact(operator_position)) {
      std::cerr << "REX_OMP_AST_INVARIANT[expression-parser-source]: "
                << (producer != NULL ? producer : "<null>")
                << " did not publish one exact generated source identity\n";
      ROSE_ABORT();
    }
  }

  static SgExprListExp *buildOpenMPExpressionList() {
    SgExprListExp *list = SageBuilder::buildExprListExp_nfi();
    SageInterface::setOneSourcePositionForTransformation(list);
    requireOpenMPGeneratedExpressionSource(list,
                                           "parenthesized expression list");
    return list;
  }

  static SgNullExpression *buildOpenMPSyntacticAbsence() {
    SgNullExpression *absence = SageBuilder::buildNullExpression_nfi(
        SgNullExpression::e_null_expression_syntactic_absence);
    SageInterface::setOneSourcePositionForTransformation(absence);
    requireOpenMPGeneratedExpressionSource(absence,
                                           "array-section omitted operand");
    return absence;
  }

  static SgSubscriptExpression *buildOpenMPArraySectionSubscript(
      SgExpression * lower, SgExpression * length,
      SgExpression *stride = NULL) {
    ROSE_ASSERT(lower != NULL);
    if (length == NULL) {
      length = buildOpenMPSyntacticAbsence();
    }
    if (stride == NULL) {
      stride = buildOpenMPSyntacticAbsence();
    }
    SgSubscriptExpression *subscript =
        SageBuilder::buildSubscriptExpression_nfi(lower, length, stride);
    SageInterface::setOneSourcePositionForTransformation(subscript);
    requireOpenMPGeneratedExpressionSource(subscript,
                                           "array-section subscript");
    return subscript;
  }

  static SgValueExp *buildOpenMPIntegerLiteral(const char *source,
                                               bool hexadecimal_token) {
    if (source == NULL || source[0] == '\0') {
      std::cerr << "REX_OMP_AST_INVARIANT[integer-literal]: empty source "
                   "spelling\n";
      ROSE_ABORT();
    }

    const std::string spelling(source);
    auto source_literal = [](SgValueExp *value) {
      ROSE_ASSERT(value != NULL);
      SageInterface::setOneSourcePositionForTransformation(value);
      if (value->get_file_info() == NULL ||
          value->get_startOfConstruct() == NULL ||
          value->get_endOfConstruct() == NULL ||
          value->get_operatorPosition() == NULL ||
          !value->get_file_info()->isTransformation() ||
          !value->get_startOfConstruct()->isTransformation() ||
          !value->get_endOfConstruct()->isTransformation() ||
          !value->get_operatorPosition()->isTransformation()) {
        std::cerr << "REX_OMP_AST_INVARIANT[integer-literal-provenance]: "
                     "expression parser did not construct one exact "
                     "generated source identity\n";
        ROSE_ABORT();
      }
      value->set_literal_spelling_form(SgValueExp::e_literal_source_spelled);
      return value;
    };
    if (SageInterface::is_Fortran_language()) {
      if (hexadecimal_token) {
        std::cerr << "REX_OMP_AST_INVARIANT[integer-literal]: C-style "
                     "hexadecimal literal in a Fortran directive\n";
        ROSE_ABORT();
      }
      errno = 0;
      char *end = NULL;
      const long long value = std::strtoll(source, &end, 10);
      if (errno == ERANGE || end == source || end == NULL || *end != '\0') {
        std::cerr << "REX_OMP_AST_INVARIANT[integer-literal]: invalid or "
                     "out-of-range Fortran integer literal '"
                  << spelling << "'\n";
        ROSE_ABORT();
      }
      if (omp_exprparser_fortran_default_integer_type == NULL ||
          isSgTypeUnknown(omp_exprparser_fortran_default_integer_type) !=
              NULL ||
          isSgTypeDefault(omp_exprparser_fortran_default_integer_type) !=
              NULL) {
        std::cerr << "REX_OMP_AST_INVARIANT[integer-literal]: Fortran "
                     "directive has no producer-owned default INTEGER type\n";
        ROSE_ABORT();
      }
      SgValueExp *literal = NULL;
      if (value >= std::numeric_limits<int>::min() &&
          value <= std::numeric_limits<int>::max()) {
        literal =
            SageBuilder::buildIntVal_nfi(static_cast<int>(value), spelling);
      } else {
        literal = SageBuilder::buildLongLongIntVal_nfi(value, spelling);
      }
      if (literal == NULL || literal->get_literal_type() != NULL) {
        std::cerr << "REX_OMP_AST_INVARIANT[integer-literal]: Fortran "
                     "literal carrier already owns contradictory type "
                     "metadata\n";
        ROSE_ABORT();
      }
      literal->set_literal_type(omp_exprparser_fortran_default_integer_type);
      return source_literal(literal);
    }

    std::size_t numeric_end = hexadecimal_token ? 2 : 0;
    while (
        numeric_end < spelling.size() &&
        (hexadecimal_token
             ? std::isxdigit(static_cast<unsigned char>(spelling[numeric_end]))
             : std::isdigit(
                   static_cast<unsigned char>(spelling[numeric_end])))) {
      ++numeric_end;
    }
    const std::string numeric_spelling = spelling.substr(0, numeric_end);
    std::string suffix = spelling.substr(numeric_end);
    bool suffix_unsigned = false;
    unsigned suffix_long_count = 0;
    for (char character : suffix) {
      character = static_cast<char>(
          std::tolower(static_cast<unsigned char>(character)));
      if (character == 'u') {
        if (suffix_unsigned) {
          std::cerr << "REX_OMP_AST_INVARIANT[integer-literal]: "
                       "duplicate unsigned suffix in '"
                    << spelling << "'\n";
          ROSE_ABORT();
        }
        suffix_unsigned = true;
      } else if (character == 'l') {
        ++suffix_long_count;
      } else {
        std::cerr << "REX_OMP_AST_INVARIANT[integer-literal]: invalid "
                     "suffix in '"
                  << spelling << "'\n";
        ROSE_ABORT();
      }
    }
    if (suffix_long_count > 2 || numeric_spelling.empty()) {
      std::cerr << "REX_OMP_AST_INVARIANT[integer-literal]: invalid suffix "
                   "or numeric spelling in '"
                << spelling << "'\n";
      ROSE_ABORT();
    }

    errno = 0;
    char *end = NULL;
    const unsigned long long value =
        std::strtoull(numeric_spelling.c_str(), &end, 0);
    if (errno == ERANGE || end == numeric_spelling.c_str() || end == NULL ||
        *end != '\0') {
      std::cerr << "REX_OMP_AST_INVARIANT[integer-literal]: invalid or "
                   "out-of-range C/C++ integer literal '"
                << spelling << "'\n";
      ROSE_ABORT();
    }

    const bool nondecimal =
        hexadecimal_token ||
        (numeric_spelling.size() > 1 && numeric_spelling.front() == '0');
    SgProject *project = SageInterface::getProject(omp_directive_node);
    if (project == NULL) {
      std::cerr << "REX_OMP_AST_INVARIANT[integer-literal]: no exact target "
                   "ABI for '"
                << spelling << "'\n";
      ROSE_ABORT();
    }
    enum class Candidate {
      Int,
      UnsignedInt,
      Long,
      UnsignedLong,
      LongLong,
      UnsignedLongLong
    };
    std::vector<Candidate> candidates;
    if (suffix_unsigned) {
      if (suffix_long_count == 0)
        candidates = {Candidate::UnsignedInt, Candidate::UnsignedLong,
                      Candidate::UnsignedLongLong};
      else if (suffix_long_count == 1)
        candidates = {Candidate::UnsignedLong, Candidate::UnsignedLongLong};
      else
        candidates = {Candidate::UnsignedLongLong};
    } else if (suffix_long_count == 0) {
      candidates = nondecimal
                       ? std::vector<Candidate>{Candidate::Int,
                                                Candidate::UnsignedInt,
                                                Candidate::Long,
                                                Candidate::UnsignedLong,
                                                Candidate::LongLong,
                                                Candidate::UnsignedLongLong}
                       : std::vector<Candidate>{Candidate::Int, Candidate::Long,
                                                Candidate::LongLong};
    } else if (suffix_long_count == 1) {
      candidates =
          nondecimal
              ? std::vector<Candidate>{Candidate::Long, Candidate::UnsignedLong,
                                       Candidate::LongLong,
                                       Candidate::UnsignedLongLong}
              : std::vector<Candidate>{Candidate::Long, Candidate::LongLong};
    } else {
      candidates = nondecimal
                       ? std::vector<Candidate>{Candidate::LongLong,
                                                Candidate::UnsignedLongLong}
                       : std::vector<Candidate>{Candidate::LongLong};
    }

    const unsigned long_width = project->get_mode_32_bit() ? 32 : 64;
    auto fits_unsigned = [value](unsigned width) {
      return width == 64 ||
             value <= ((static_cast<unsigned long long>(1) << width) - 1);
    };
    auto fits_signed = [value](unsigned width) {
      return value <=
             (width == 64
                  ? static_cast<unsigned long long>(LLONG_MAX)
                  : (static_cast<unsigned long long>(1) << (width - 1)) - 1);
    };
    for (Candidate candidate : candidates) {
      switch (candidate) {
      case Candidate::Int:
        if (fits_signed(32))
          return source_literal(
              SageBuilder::buildIntVal_nfi(static_cast<int>(value), spelling));
        break;
      case Candidate::UnsignedInt:
        if (fits_unsigned(32))
          return source_literal(SageBuilder::buildUnsignedIntVal_nfi(
              static_cast<unsigned int>(value), spelling));
        break;
      case Candidate::Long:
        if (fits_signed(long_width))
          return source_literal(SageBuilder::buildLongIntVal_nfi(
              static_cast<long>(value), spelling));
        break;
      case Candidate::UnsignedLong:
        if (fits_unsigned(long_width))
          return source_literal(SageBuilder::buildUnsignedLongVal_nfi(
              static_cast<unsigned long>(value), spelling));
        break;
      case Candidate::LongLong:
        if (fits_signed(64))
          return source_literal(SageBuilder::buildLongLongIntVal_nfi(
              static_cast<long long>(value), spelling));
        break;
      case Candidate::UnsignedLongLong:
        return source_literal(
            SageBuilder::buildUnsignedLongLongIntVal_nfi(value, spelling));
      }
    }

    std::cerr << "REX_OMP_AST_INVARIANT[integer-literal]: literal does not "
                 "fit any exact target candidate type: '"
              << spelling << "'\n";
    ROSE_ABORT();
  }

  static bool isKnownVariableSymbolType(SgVariableSymbol * symbol);
  static SgExpression *buildOpenMPIdentifierExpression(const std::string &name,
                                                       SgScopeStatement *scope);

  void omp_exprparser_clear_context_variable_symbols() {
    requireIdleOpenMPExpressionParser("context clear");
    if (omp_exprparser_openacc_cxx_semantic_bindings != NULL ||
        omp_exprparser_fortran_exact_semantic_bindings != NULL ||
        omp_exprparser_exact_subexpression_types != NULL ||
        omp_exprparser_fortran_typed_scope != NULL ||
        !omp_exprparser_fortran_typed_scope_bindings.empty()) {
      std::cerr << "REX_OMP_AST_INVARIANT[exact-semantic-lifecycle]: "
                   "cleared parser context with active exact bindings\n";
      ROSE_ABORT();
    }
    omp_exprparser_context_variable_symbols.clear();
    omp_exprparser_context_name_expressions.clear();
  }

  void omp_exprparser_begin_fortran_exact_semantic_bindings(
      const std::vector<OmpFortranExactSemanticBindings::Binding> *bindings,
      const std::vector<OmpExactSubexpressionType> *subexpressions,
      SgType *default_integer_type) {
    requireIdleOpenMPExpressionParser("Fortran exact-binding installation");
    if (!SageInterface::is_Fortran_language() || bindings == NULL ||
        omp_exprparser_fortran_exact_semantic_bindings != NULL ||
        omp_exprparser_openacc_cxx_semantic_bindings != NULL ||
        omp_exprparser_exact_subexpression_types != NULL ||
        subexpressions == NULL || default_integer_type == NULL ||
        isSgTypeUnknown(default_integer_type) != NULL ||
        isSgTypeDefault(default_integer_type) != NULL ||
        omp_exprparser_fortran_default_integer_type != NULL) {
      std::cerr << "REX_OMP_AST_INVARIANT[fortran-exact-semantic-lifecycle]: "
                   "invalid or nested exact-binding installation\n";
      ROSE_ABORT();
    }
    omp_exprparser_fortran_exact_semantic_bindings = bindings;
    omp_exprparser_fortran_exact_semantic_binding_index = 0;
    omp_exprparser_exact_subexpression_types = subexpressions;
    omp_exprparser_exact_subexpression_type_index = 0;
    omp_exprparser_fortran_default_integer_type = default_integer_type;
  }

  void omp_exprparser_end_fortran_exact_semantic_bindings() {
    requireIdleOpenMPExpressionParser("Fortran exact-binding release");
    if (omp_exprparser_fortran_exact_semantic_bindings == NULL ||
        omp_exprparser_fortran_exact_semantic_binding_index !=
            omp_exprparser_fortran_exact_semantic_bindings->size() ||
        omp_exprparser_exact_subexpression_types == NULL ||
        omp_exprparser_fortran_default_integer_type == NULL ||
        omp_exprparser_exact_subexpression_type_index !=
            omp_exprparser_exact_subexpression_types->size()) {
      std::cerr << "REX_OMP_AST_INVARIANT[fortran-exact-semantic-consumption]: "
                   "expression parser did not consume every exact Fortran "
                   "identifier binding\n";
      ROSE_ABORT();
    }
    omp_exprparser_fortran_exact_semantic_bindings = NULL;
    omp_exprparser_fortran_exact_semantic_binding_index = 0;
    omp_exprparser_exact_subexpression_types = NULL;
    omp_exprparser_exact_subexpression_type_index = 0;
    omp_exprparser_fortran_default_integer_type = NULL;
  }

  void omp_exprparser_begin_fortran_typed_scope_semantics(
      SgScopeStatement * scope, SgType * default_integer_type) {
    requireIdleOpenMPExpressionParser("Fortran typed-scope installation");
    if (!SageInterface::is_Fortran_language() || scope == NULL ||
        omp_exprparser_fortran_typed_scope != NULL ||
        !omp_exprparser_fortran_typed_scope_bindings.empty() ||
        omp_exprparser_fortran_exact_semantic_bindings != NULL ||
        omp_exprparser_openacc_cxx_semantic_bindings != NULL ||
        omp_exprparser_exact_subexpression_types != NULL ||
        default_integer_type == NULL ||
        isSgTypeUnknown(default_integer_type) != NULL ||
        isSgTypeDefault(default_integer_type) != NULL ||
        omp_exprparser_fortran_default_integer_type != NULL) {
      std::cerr << "REX_OMP_AST_INVARIANT[fortran-typed-scope-lifecycle]: "
                   "invalid or nested typed-scope installation\n";
      ROSE_ABORT();
    }
    omp_exprparser_fortran_typed_scope = scope;
    omp_exprparser_fortran_default_integer_type = default_integer_type;
  }

  void omp_exprparser_end_fortran_typed_scope_semantics() {
    requireIdleOpenMPExpressionParser("Fortran typed-scope release");
    if (omp_exprparser_fortran_typed_scope == NULL ||
        omp_exprparser_fortran_default_integer_type == NULL ||
        omp_exprparser_fortran_exact_semantic_bindings != NULL ||
        omp_exprparser_exact_subexpression_types != NULL) {
      std::cerr << "REX_OMP_AST_INVARIANT[fortran-typed-scope-lifecycle]: "
                   "invalid typed-scope release\n";
      ROSE_ABORT();
    }
    omp_exprparser_fortran_typed_scope = NULL;
    omp_exprparser_fortran_default_integer_type = NULL;
    omp_exprparser_fortran_typed_scope_bindings.clear();
  }

  void omp_exprparser_begin_openacc_cxx_semantic_bindings(
      const OpenACCCxxExactSemanticBindings::ExpressionBindings *bindings) {
    requireIdleOpenMPExpressionParser(
        "OpenACC C/C++ exact-binding installation");
    if (SageInterface::is_Fortran_language()) {
      std::cerr << "REX_ACC_AST_INVARIANT[cxx-semantic-lifecycle]: "
                   "OpenACC Clang bindings cannot be installed for Fortran\n";
      ROSE_ABORT();
    }
    if (bindings == NULL || omp_exprparser_openacc_cxx_semantic_bindings != NULL ||
        omp_exprparser_exact_subexpression_types != NULL) {
      std::cerr << "REX_ACC_AST_INVARIANT[cxx-semantic-lifecycle]: "
                   "invalid or nested OpenACC exact-binding installation\n";
      ROSE_ABORT();
    }
    omp_exprparser_openacc_cxx_semantic_bindings = bindings;
    omp_exprparser_exact_semantic_binding_index = 0;
    omp_exprparser_exact_subexpression_types = &bindings->subexpressions();
    omp_exprparser_exact_subexpression_type_index = 0;
  }

  void omp_exprparser_end_openacc_cxx_semantic_bindings() {
    requireIdleOpenMPExpressionParser("OpenACC C/C++ exact-binding release");
    if (omp_exprparser_openacc_cxx_semantic_bindings == NULL ||
        omp_exprparser_exact_semantic_binding_index !=
            omp_exprparser_openacc_cxx_semantic_bindings->identifiers().size() ||
        omp_exprparser_exact_subexpression_types == NULL ||
        omp_exprparser_exact_subexpression_type_index !=
            omp_exprparser_exact_subexpression_types->size()) {
      std::cerr << "REX_ACC_AST_INVARIANT[cxx-semantic-consumption]: "
                   "expression parser did not consume every OpenACC exact "
                   "identifier binding\n";
      ROSE_ABORT();
    }
    omp_exprparser_openacc_cxx_semantic_bindings = NULL;
    omp_exprparser_exact_semantic_binding_index = 0;
    omp_exprparser_exact_subexpression_types = NULL;
    omp_exprparser_exact_subexpression_type_index = 0;
  }

  static const OpenACCCxxExactSemanticBindings::Binding &
  consumeOpenACCCxxExactSemanticBinding(const std::string &spelling) {
    if (omp_exprparser_openacc_cxx_semantic_bindings == NULL ||
        omp_exprparser_exact_semantic_binding_index >=
            omp_exprparser_openacc_cxx_semantic_bindings->identifiers().size()) {
      std::cerr << "REX_ACC_AST_INVARIANT[cxx-semantic-consumption]: no "
                   "producer binding for identifier '"
                << spelling << "'\n";
      ROSE_ABORT();
    }
    const OpenACCCxxExactSemanticBindings::Binding &binding =
        omp_exprparser_openacc_cxx_semantic_bindings
            ->identifiers()[omp_exprparser_exact_semantic_binding_index++];
    if (binding.spelling() != spelling) {
      std::cerr << "REX_ACC_AST_INVARIANT[cxx-semantic-consumption]: "
                   "producer identifier '"
                << binding.spelling() << "' does not match parser identifier '"
                << spelling << "'\n";
      ROSE_ABORT();
    }
    return binding;
  }

  static const OmpFortranExactSemanticBindings::Binding &
  consumeFortranOpenMPExactSemanticBinding(const std::string &spelling) {
    if (omp_exprparser_fortran_typed_scope != NULL) {
      std::string canonical = spelling;
      std::transform(canonical.begin(), canonical.end(), canonical.begin(),
                     [](unsigned char character) {
                       return static_cast<char>(std::tolower(character));
                     });
      const auto local =
          omp_exprparser_context_variable_symbols.find(canonical);
      if (local != omp_exprparser_context_variable_symbols.end()) {
        SgVariableSymbol *symbol = local->second;
        if (symbol == NULL || symbol->get_declaration() == NULL ||
            !isKnownVariableSymbolType(symbol)) {
          std::cerr << "REX_OMP_AST_INVARIANT[fortran-typed-scope-name]: "
                       "directive-local name '"
                    << spelling << "' has no exact typed symbol\n";
          ROSE_ABORT();
        }
        omp_exprparser_fortran_typed_scope_bindings.push_back(
            std::make_unique<OmpFortranExactSemanticBindings::Binding>(
                0, spelling.size(), spelling, spelling,
                OmpFortranExactSemanticBindings::BindingKind::directive_local,
                nullptr, nullptr, symbol->get_type()));
        return *omp_exprparser_fortran_typed_scope_bindings.back();
      }

      SgSymbol *symbol = SageInterface::lookupSymbolInParentScopes(
          SgName(canonical), omp_exprparser_fortran_typed_scope);
      std::set<SgAliasSymbol *> aliases;
      while (SgAliasSymbol *alias = isSgAliasSymbol(symbol)) {
        if (alias->get_alias() == NULL || !aliases.insert(alias).second) {
          std::cerr << "REX_OMP_AST_INVARIANT[fortran-typed-scope-name]: "
                       "name '"
                    << spelling << "' has an invalid Sage alias chain\n";
          ROSE_ABORT();
        }
        symbol = alias->get_alias();
      }
      if (symbol == NULL || (isSgVariableSymbol(symbol) == NULL &&
                             isSgEnumFieldSymbol(symbol) == NULL &&
                             isSgFunctionSymbol(symbol) == NULL)) {
        std::cerr << "REX_OMP_AST_INVARIANT[fortran-typed-scope-name]: "
                     "name '"
                  << spelling
                  << "' has no unique typed value in the directive scope\n";
        ROSE_ABORT();
      }
      omp_exprparser_fortran_typed_scope_bindings.push_back(
          std::make_unique<OmpFortranExactSemanticBindings::Binding>(
              0, spelling.size(), spelling, spelling,
              OmpFortranExactSemanticBindings::BindingKind::value, symbol,
              symbol, nullptr));
      return *omp_exprparser_fortran_typed_scope_bindings.back();
    }
    if (omp_exprparser_fortran_exact_semantic_bindings == NULL ||
        omp_exprparser_fortran_exact_semantic_binding_index >=
            omp_exprparser_fortran_exact_semantic_bindings->size()) {
      std::cerr << "REX_OMP_AST_INVARIANT[fortran-exact-semantic-consumption]: "
                   "no producer binding for identifier '"
                << spelling << "'\n";
      ROSE_ABORT();
    }
    const auto &binding = (*omp_exprparser_fortran_exact_semantic_bindings)
        [omp_exprparser_fortran_exact_semantic_binding_index++];
    if (binding.spelling() != spelling) {
      std::cerr << "REX_OMP_AST_INVARIANT[fortran-exact-semantic-consumption]: "
                   "producer identifier '"
                << binding.spelling() << "' does not match parser identifier '"
                << spelling << "'\n";
      ROSE_ABORT();
    }
    return binding;
  }

  static SgType *consumeOpenMPExactSubexpressionType(
      OmpExactSubexpressionKind expected_kind) {
    if (omp_exprparser_fortran_typed_scope != NULL) {
      std::cerr << "REX_OMP_AST_INVARIANT[fortran-typed-scope-type]: "
                   "REX-owned operation="
                << static_cast<int>(expected_kind)
                << " has no explicit typed-scope result-type rule\n";
      ROSE_ABORT();
    }
    if (expected_kind == OmpExactSubexpressionKind::invalid ||
        omp_exprparser_exact_subexpression_types == NULL ||
        omp_exprparser_exact_subexpression_type_index >=
            omp_exprparser_exact_subexpression_types->size()) {
      std::cerr << "REX_OMP_AST_INVARIANT[exact-subexpression-type]: operation="
                << static_cast<int>(expected_kind)
                << " has no frontend-owned semantic result-type record\n";
      ROSE_ABORT();
    }
    const OmpExactSubexpressionType &record =
        (*omp_exprparser_exact_subexpression_types)
            [omp_exprparser_exact_subexpression_type_index++];
    if (record.kind() != expected_kind || record.resultType() == NULL ||
        isSgTypeUnknown(record.resultType()) != NULL ||
        isSgTypeDefault(record.resultType()) != NULL) {
      std::cerr << "REX_OMP_AST_INVARIANT[exact-subexpression-type]: "
                   "expected operation="
                << static_cast<int>(expected_kind)
                << " but producer supplied operation="
                << static_cast<int>(record.kind())
                << " without one exact semantic result type\n";
      ROSE_ABORT();
    }
    return record.resultType();
  }

  static SgType *buildOpenMPFortranTypedScopeStringLiteralType(
      const std::string &source_literal) {
    if (omp_exprparser_fortran_typed_scope == NULL ||
        source_literal.size() < 2 ||
        source_literal.front() != source_literal.back() ||
        (source_literal.front() != '\'' && source_literal.front() != '"')) {
      std::cerr << "REX_OMP_AST_INVARIANT[fortran-string-literal-type]: "
                   "typed-scope literal has no exact quoted Fortran source "
                   "spelling\n";
      ROSE_ABORT();
    }

    const char delimiter = source_literal.front();
    std::size_t semantic_length = 0;
    for (std::size_t offset = 1; offset + 1 < source_literal.size(); ++offset) {
      if (source_literal[offset] == delimiter &&
          offset + 2 < source_literal.size() &&
          source_literal[offset + 1] == delimiter) {
        ++offset;
      }
      ++semantic_length;
    }
    if (semantic_length >
        static_cast<std::size_t>(std::numeric_limits<int>::max())) {
      std::cerr << "REX_OMP_AST_INVARIANT[fortran-string-literal-type]: "
                   "literal length exceeds the exact Sage selector range\n";
      ROSE_ABORT();
    }

    SgIntVal *length = SageBuilder::buildIntVal_nfi(
        static_cast<int>(semantic_length), std::to_string(semantic_length));
    if (length == NULL) {
      std::cerr << "REX_OMP_AST_INVARIANT[fortran-string-literal-type]: "
                   "cannot construct the exact literal length selector\n";
      ROSE_ABORT();
    }
    SageBuilder::initializeSemanticExpressionSourceProvenance(length);
    SgTypeString *type = SgTypeString::createType(length, NULL);
    if (type == NULL || type->get_lengthExpression() != length ||
        length->get_parent() != type || type->get_fortran_source_syntax()) {
      std::cerr << "REX_OMP_AST_INVARIANT[fortran-string-literal-type]: "
                   "literal did not retain one exact default-kind semantic "
                   "CHARACTER type\n";
      ROSE_ABORT();
    }
    return type;
  }

  static SgType *deriveOpenMPArraySectionResultType(SgExpression * base) {
    if (base == NULL || omp_exprparser_fortran_typed_scope != NULL) {
      std::cerr << "REX_OMP_AST_INVARIANT[array-section-base]: C/C++ array "
                   "section has no exact base expression\n";
      ROSE_ABORT();
    }

    SgType *base_type = base->get_type();
    if (base_type != NULL) {
      base_type = base_type->stripType(
          SgType::STRIP_MODIFIER_TYPE | SgType::STRIP_TYPEDEF_TYPE |
          SgType::STRIP_REFERENCE_TYPE | SgType::STRIP_RVALUE_REFERENCE_TYPE);
    }

    SgType *result_type = NULL;
    if (SgArrayType *array_type = isSgArrayType(base_type)) {
      result_type = array_type->get_base_type();
    } else if (SgPointerType *pointer_type = isSgPointerType(base_type)) {
      result_type = pointer_type->get_base_type();
    } else {
      std::cerr << "REX_OMP_AST_INVARIANT[array-section-base]: expected a "
                   "pointer or array type, got "
                << (base_type != NULL ? base_type->class_name() : "<null>")
                << "\n";
      ROSE_ABORT();
    }

    if (result_type == NULL || isSgTypeUnknown(result_type) != NULL ||
        isSgTypeDefault(result_type) != NULL) {
      std::cerr << "REX_OMP_AST_INVARIANT[array-section-result-type]: "
                   "pointer or array base has no exact element type\n";
      ROSE_ABORT();
    }
    return result_type;
  }

  static SgType *requireOpenMPSageExpressionType(SgExpression * expression,
                                                 const char *operation) {
    SgType *type = expression != NULL ? expression->get_type() : NULL;
    if (operation == NULL || type == NULL ||
        isSgTypeUnknown(type) != NULL || isSgTypeDefault(type) != NULL) {
      std::cerr << "REX_OMP_AST_INVARIANT[sage-expression-type]: "
                << (operation != NULL ? operation : "<null>")
                << " has no exact Sage operand type\n";
      ROSE_ABORT();
    }
    return type;
  }

  static SgType *deriveOpenMPCxxAssignmentResultType(
      OmpExactSubexpressionKind expected_kind, SgExpression * lhs,
      SgExpression * rhs) {
    if (omp_exprparser_exact_subexpression_types != NULL) {
      return consumeOpenMPExactSubexpressionType(expected_kind);
    }
    if (SageInterface::is_Fortran_language()) {
      return consumeOpenMPExactSubexpressionType(expected_kind);
    }
    const bool assignment =
        expected_kind == OmpExactSubexpressionKind::assign ||
        expected_kind == OmpExactSubexpressionKind::rshift_assign ||
        expected_kind == OmpExactSubexpressionKind::lshift_assign ||
        expected_kind == OmpExactSubexpressionKind::add_assign ||
        expected_kind == OmpExactSubexpressionKind::subtract_assign ||
        expected_kind == OmpExactSubexpressionKind::multiply_assign ||
        expected_kind == OmpExactSubexpressionKind::divide_assign ||
        expected_kind == OmpExactSubexpressionKind::modulo_assign ||
        expected_kind == OmpExactSubexpressionKind::bit_and_assign ||
        expected_kind == OmpExactSubexpressionKind::bit_xor_assign ||
        expected_kind == OmpExactSubexpressionKind::bit_or_assign;
    SgType *lhs_type =
        requireOpenMPSageExpressionType(lhs, "C/C++ assignment left operand");
    requireOpenMPSageExpressionType(rhs, "C/C++ assignment right operand");
    if (!assignment) {
      std::cerr << "REX_OMP_AST_INVARIANT[sage-expression-type]: "
                   "non-assignment operation="
                << static_cast<int>(expected_kind)
                << " requested an assignment result type\n";
      ROSE_ABORT();
    }
    return lhs_type;
  }

  static SgType *deriveOpenMPCxxEquivalentOperandResultType(
      OmpExactSubexpressionKind expected_kind, SgExpression * lhs,
      SgExpression * rhs) {
    if (omp_exprparser_exact_subexpression_types != NULL) {
      return consumeOpenMPExactSubexpressionType(expected_kind);
    }
    if (SageInterface::is_Fortran_language()) {
      return consumeOpenMPExactSubexpressionType(expected_kind);
    }
    SgType *lhs_type =
        requireOpenMPSageExpressionType(lhs, "C/C++ binary left operand");
    SgType *rhs_type =
        requireOpenMPSageExpressionType(rhs, "C/C++ binary right operand");
    if (!SageInterface::isEquivalentType(lhs_type, rhs_type)) {
      std::cerr << "REX_OMP_AST_INVARIANT[sage-expression-type]: C/C++ "
                   "operation="
                << static_cast<int>(expected_kind)
                << " requires two equivalent exact Sage operand types\n";
      ROSE_ABORT();
    }
    return lhs_type;
  }

  static SgType *stripOpenMPCxxValueType(SgType * type) {
    return type != NULL
               ? type->stripType(
                     SgType::STRIP_MODIFIER_TYPE |
                     SgType::STRIP_TYPEDEF_TYPE |
                     SgType::STRIP_REFERENCE_TYPE |
                     SgType::STRIP_RVALUE_REFERENCE_TYPE)
               : NULL;
  }

  static bool isOpenMPCxxIntegerType(SgType * type) {
    type = stripOpenMPCxxValueType(type);
    return type != NULL &&
           (type->isIntegerType() || isSgEnumType(type) != NULL);
  }

  static bool isOpenMPCxxArithmeticType(SgType * type) {
    type = stripOpenMPCxxValueType(type);
    return type != NULL &&
           (type->isIntegerType() || type->isFloatType() ||
            isSgEnumType(type) != NULL);
  }

  static bool isOpenMPCxxScalarType(SgType * type) {
    type = stripOpenMPCxxValueType(type);
    return isOpenMPCxxArithmeticType(type) ||
           isSgPointerType(type) != NULL;
  }

  static SgType *deriveOpenMPCxxIntegerBinaryResultType(
      OmpExactSubexpressionKind expected_kind, SgExpression * lhs,
      SgExpression * rhs, bool shift) {
    if (omp_exprparser_exact_subexpression_types != NULL ||
        SageInterface::is_Fortran_language()) {
      return consumeOpenMPExactSubexpressionType(expected_kind);
    }
    SgType *lhs_type =
        requireOpenMPSageExpressionType(lhs, "C/C++ integer left operand");
    SgType *rhs_type =
        requireOpenMPSageExpressionType(rhs, "C/C++ integer right operand");
    if (!isOpenMPCxxIntegerType(lhs_type) ||
        !isOpenMPCxxIntegerType(rhs_type)) {
      std::cerr << "REX_OMP_AST_INVARIANT[sage-expression-type]: C/C++ "
                   "integer operation="
                << static_cast<int>(expected_kind)
                << " has a non-integer operand\n";
      ROSE_ABORT();
    }
    if (shift) {
      return SageInterface::usualArithmeticConversionType(
          lhs_type, SageBuilder::buildIntType(), omp_directive_node);
    }
    return SageInterface::usualArithmeticConversionType(
        lhs_type, rhs_type, omp_directive_node);
  }

  static SgType *deriveOpenMPCxxLogicalBinaryResultType(
      OmpExactSubexpressionKind expected_kind, SgExpression * lhs,
      SgExpression * rhs) {
    if (omp_exprparser_exact_subexpression_types != NULL ||
        SageInterface::is_Fortran_language()) {
      return consumeOpenMPExactSubexpressionType(expected_kind);
    }
    SgType *lhs_type =
        requireOpenMPSageExpressionType(lhs, "C/C++ logical left operand");
    SgType *rhs_type =
        requireOpenMPSageExpressionType(rhs, "C/C++ logical right operand");
    if (!isOpenMPCxxScalarType(lhs_type) ||
        !isOpenMPCxxScalarType(rhs_type)) {
      std::cerr << "REX_OMP_AST_INVARIANT[sage-expression-type]: C/C++ "
                   "logical operation="
                << static_cast<int>(expected_kind)
                << " has a non-scalar operand\n";
      ROSE_ABORT();
    }
    return SageInterface::is_C_language()
               ? static_cast<SgType *>(SageBuilder::buildIntType())
               : static_cast<SgType *>(SageBuilder::buildBoolType());
  }

  static SgType *deriveOpenMPCxxUnaryResultType(
      OmpExactSubexpressionKind expected_kind, SgExpression * operand) {
    if (omp_exprparser_exact_subexpression_types != NULL ||
        SageInterface::is_Fortran_language()) {
      return consumeOpenMPExactSubexpressionType(expected_kind);
    }
    SgType *operand_type =
        requireOpenMPSageExpressionType(operand, "C/C++ unary operand");
    switch (expected_kind) {
    case OmpExactSubexpressionKind::prefix_increment:
    case OmpExactSubexpressionKind::prefix_decrement:
    case OmpExactSubexpressionKind::postfix_increment:
    case OmpExactSubexpressionKind::postfix_decrement:
      if (!isOpenMPCxxArithmeticType(operand_type) &&
          isSgPointerType(stripOpenMPCxxValueType(operand_type)) == NULL) {
        std::cerr << "REX_OMP_AST_INVARIANT[sage-expression-type]: C/C++ "
                     "increment/decrement has a non-scalar operand\n";
        ROSE_ABORT();
      }
      return operand_type;
    case OmpExactSubexpressionKind::unary_plus:
    case OmpExactSubexpressionKind::unary_minus:
      if (!isOpenMPCxxArithmeticType(operand_type)) {
        std::cerr << "REX_OMP_AST_INVARIANT[sage-expression-type]: C/C++ "
                     "unary arithmetic has a non-arithmetic operand\n";
        ROSE_ABORT();
      }
      return SageInterface::usualArithmeticConversionType(
          operand_type, SageBuilder::buildIntType(), omp_directive_node);
    case OmpExactSubexpressionKind::logical_not:
      if (!isOpenMPCxxScalarType(operand_type)) {
        std::cerr << "REX_OMP_AST_INVARIANT[sage-expression-type]: C/C++ "
                     "logical not has a non-scalar operand\n";
        ROSE_ABORT();
      }
      return SageInterface::is_C_language()
                 ? static_cast<SgType *>(SageBuilder::buildIntType())
                 : static_cast<SgType *>(SageBuilder::buildBoolType());
    case OmpExactSubexpressionKind::bit_complement:
      if (!isOpenMPCxxIntegerType(operand_type)) {
        std::cerr << "REX_OMP_AST_INVARIANT[sage-expression-type]: C/C++ "
                     "bit complement has a non-integer operand\n";
        ROSE_ABORT();
      }
      return SageInterface::usualArithmeticConversionType(
          operand_type, SageBuilder::buildIntType(), omp_directive_node);
    case OmpExactSubexpressionKind::address_of:
      return SageBuilder::buildPointerType(operand_type);
    default:
      std::cerr << "REX_OMP_AST_INVARIANT[sage-expression-type]: C/C++ "
                   "unary operation="
                << static_cast<int>(expected_kind)
                << " has no exact REX result-type rule\n";
      ROSE_ABORT();
    }
  }

  static SgType *deriveOpenMPCxxCastToIntResultType(
      OmpExactSubexpressionKind expected_kind) {
    if (omp_exprparser_exact_subexpression_types != NULL ||
        SageInterface::is_Fortran_language()) {
      return consumeOpenMPExactSubexpressionType(expected_kind);
    }
    if (expected_kind != OmpExactSubexpressionKind::cast) {
      std::cerr << "REX_OMP_AST_INVARIANT[sage-expression-type]: C/C++ "
                   "integer cast has the wrong operation kind\n";
      ROSE_ABORT();
    }
    return SageBuilder::buildIntType();
  }

  static SgType *deriveOpenMPCxxSizeofResultType(
      OmpExactSubexpressionKind expected_kind) {
    if (omp_exprparser_exact_subexpression_types != NULL ||
        SageInterface::is_Fortran_language()) {
      return consumeOpenMPExactSubexpressionType(expected_kind);
    }
    if (expected_kind != OmpExactSubexpressionKind::sizeof_expression &&
        expected_kind != OmpExactSubexpressionKind::sizeof_type) {
      std::cerr << "REX_OMP_AST_INVARIANT[sage-expression-type]: C/C++ "
                   "sizeof has the wrong operation kind\n";
      ROSE_ABORT();
    }
    return SageInterface::requireTargetSizeType(omp_directive_node);
  }

  static SgType *deriveOpenMPCxxConditionalResultType(
      SgExpression * condition, SgExpression * true_expression,
      SgExpression * false_expression) {
    if (omp_exprparser_exact_subexpression_types != NULL) {
      return consumeOpenMPExactSubexpressionType(
          OmpExactSubexpressionKind::conditional);
    }
    if (SageInterface::is_Fortran_language()) {
      return consumeOpenMPExactSubexpressionType(
          OmpExactSubexpressionKind::conditional);
    }
    SgType *condition_type = requireOpenMPSageExpressionType(
        condition, "C/C++ conditional condition");
    if (!isOpenMPCxxScalarType(condition_type)) {
      std::cerr << "REX_OMP_AST_INVARIANT[sage-expression-type]: C/C++ "
                   "conditional has a non-scalar condition\n";
      ROSE_ABORT();
    }
    SgType *true_type = requireOpenMPSageExpressionType(
        true_expression, "C/C++ conditional true operand");
    SgType *false_type = requireOpenMPSageExpressionType(
        false_expression, "C/C++ conditional false operand");
    if (isOpenMPCxxArithmeticType(true_type) &&
        isOpenMPCxxArithmeticType(false_type)) {
      return SageInterface::usualArithmeticConversionType(
          true_type, false_type, omp_directive_node);
    }
    return deriveOpenMPCxxEquivalentOperandResultType(
        OmpExactSubexpressionKind::conditional, true_expression,
        false_expression);
  }

  static SgType *deriveOpenMPCxxDereferenceResultType(
      SgExpression * operand) {
    if (omp_exprparser_exact_subexpression_types != NULL) {
      return consumeOpenMPExactSubexpressionType(
          OmpExactSubexpressionKind::dereference);
    }
    if (SageInterface::is_Fortran_language()) {
      return consumeOpenMPExactSubexpressionType(
          OmpExactSubexpressionKind::dereference);
    }
    SgType *operand_type =
        requireOpenMPSageExpressionType(operand, "C/C++ dereference operand");
    operand_type = operand_type->stripType(
        SgType::STRIP_MODIFIER_TYPE | SgType::STRIP_TYPEDEF_TYPE |
        SgType::STRIP_REFERENCE_TYPE | SgType::STRIP_RVALUE_REFERENCE_TYPE);
    SgPointerType *pointer = isSgPointerType(operand_type);
    SgType *result_type =
        pointer != NULL ? pointer->get_base_type() : NULL;
    if (result_type == NULL || isSgTypeUnknown(result_type) != NULL ||
        isSgTypeDefault(result_type) != NULL) {
      std::cerr << "REX_OMP_AST_INVARIANT[sage-expression-type]: C/C++ "
                   "dereference operand is not one exact pointer type\n";
      ROSE_ABORT();
    }
    return result_type;
  }

  static SgType *deriveOpenMPCxxSubscriptResultType(SgExpression * base) {
    if (omp_exprparser_exact_subexpression_types != NULL) {
      return consumeOpenMPExactSubexpressionType(
          OmpExactSubexpressionKind::subscript);
    }
    if (SageInterface::is_Fortran_language()) {
      return consumeOpenMPExactSubexpressionType(
          OmpExactSubexpressionKind::subscript);
    }
    return deriveOpenMPArraySectionResultType(base);
  }

  static SgType *deriveOpenMPCxxStringLiteralType(
      const std::string & literal_value) {
    if (omp_exprparser_exact_subexpression_types != NULL) {
      return consumeOpenMPExactSubexpressionType(
          OmpExactSubexpressionKind::string_literal);
    }
    if (SageInterface::is_Fortran_language() ||
        literal_value.size() >=
            static_cast<std::size_t>(std::numeric_limits<int>::max())) {
      std::cerr << "REX_OMP_AST_INVARIANT[string-literal-type]: C/C++ "
                   "ordinary literal has no representable exact array bound\n";
      ROSE_ABORT();
    }
    SgType *element_type = SageBuilder::buildCharType();
    if (SageInterface::is_Cxx_language()) {
      element_type = SageBuilder::buildConstType(element_type);
    }
    SgIntVal *bound = SageBuilder::buildIntVal_nfi(
        static_cast<int>(literal_value.size() + 1),
        std::to_string(literal_value.size() + 1));
    SageBuilder::initializeSemanticExpressionSourceProvenance(bound);
    SgArrayType *literal_type =
        SageBuilder::buildArrayType(element_type, bound);
    if (literal_type == NULL || literal_type->get_base_type() != element_type ||
        literal_type->get_index() != bound || bound->get_parent() != literal_type) {
      std::cerr << "REX_OMP_AST_INVARIANT[string-literal-type]: C/C++ "
                   "ordinary literal did not retain one exact array type\n";
      ROSE_ABORT();
    }
    return literal_type;
  }

  static SgType *consumeOpenMPExactBinarySubexpressionType(
      OmpExactSubexpressionKind expected_kind, SgExpression * lhs,
      SgExpression * rhs) {
    if (omp_exprparser_exact_subexpression_types != NULL) {
      return consumeOpenMPExactSubexpressionType(expected_kind);
    }
    if (!SageInterface::is_Fortran_language()) {
      SgType *lhs_type = lhs != NULL ? lhs->get_type() : NULL;
      SgType *rhs_type = rhs != NULL ? rhs->get_type() : NULL;
      const bool is_arithmetic =
          expected_kind == OmpExactSubexpressionKind::add ||
          expected_kind == OmpExactSubexpressionKind::subtract ||
          expected_kind == OmpExactSubexpressionKind::multiply ||
          expected_kind == OmpExactSubexpressionKind::divide;
      if (!is_arithmetic || lhs_type == NULL || rhs_type == NULL ||
          isSgTypeUnknown(lhs_type) != NULL ||
          isSgTypeDefault(lhs_type) != NULL ||
          isSgTypeUnknown(rhs_type) != NULL ||
          isSgTypeDefault(rhs_type) != NULL) {
        std::cerr << "REX_OMP_AST_INVARIANT[sage-expression-type]: "
                     "C/C++ OpenMP arithmetic operation="
                  << static_cast<int>(expected_kind)
                  << " does not have two exact arithmetic operand types\n";
        ROSE_ABORT();
      }
      return SageInterface::usualArithmeticConversionType(
          lhs_type, rhs_type, omp_directive_node);
    }
    if (omp_exprparser_fortran_typed_scope == NULL) {
      return consumeOpenMPExactSubexpressionType(expected_kind);
    }
    SgType *lhs_type = lhs != NULL ? lhs->get_type() : NULL;
    SgType *rhs_type = rhs != NULL ? rhs->get_type() : NULL;
    const bool equivalent = lhs_type != NULL && rhs_type != NULL &&
                            SageInterface::isEquivalentType(lhs_type, rhs_type);
    const bool lhs_source_matches_rhs =
        lhs_type != NULL && rhs_type != NULL &&
        SageInterface::fortranSourceTypeMatchesSemanticExpressionType(lhs_type,
                                                                      rhs_type);
    const bool rhs_source_matches_lhs =
        lhs_type != NULL && rhs_type != NULL &&
        SageInterface::fortranSourceTypeMatchesSemanticExpressionType(rhs_type,
                                                                      lhs_type);
    const bool is_arithmetic =
        expected_kind == OmpExactSubexpressionKind::add ||
        expected_kind == OmpExactSubexpressionKind::subtract ||
        expected_kind == OmpExactSubexpressionKind::multiply ||
        expected_kind == OmpExactSubexpressionKind::divide;
    if (!is_arithmetic || lhs_type == NULL || rhs_type == NULL ||
        isSgTypeUnknown(lhs_type) != NULL ||
        isSgTypeDefault(lhs_type) != NULL ||
        isSgTypeUnknown(rhs_type) != NULL ||
        isSgTypeDefault(rhs_type) != NULL ||
        (!equivalent && !lhs_source_matches_rhs && !rhs_source_matches_lhs)) {
      std::cerr << "REX_OMP_AST_INVARIANT[fortran-typed-scope-type]: "
                   "REX-owned binary operation="
                << static_cast<int>(expected_kind)
                << " does not have two compatible typed operands; lhs="
                << (lhs_type != NULL ? lhs_type->class_name()
                                     : std::string("<null>"))
                << " rhs="
                << (rhs_type != NULL ? rhs_type->class_name()
                                     : std::string("<null>"))
                << " equivalent/source-matches=" << equivalent << "/"
                << lhs_source_matches_rhs << "/" << rhs_source_matches_lhs
                << "\n";
      ROSE_ABORT();
    }
    return lhs_source_matches_rhs ? rhs_type : lhs_type;
  }

  static SgType *consumeOpenMPExactComparisonSubexpressionType(
      OmpExactSubexpressionKind expected_kind, SgExpression * lhs,
      SgExpression * rhs) {
    if (omp_exprparser_exact_subexpression_types != NULL) {
      return consumeOpenMPExactSubexpressionType(expected_kind);
    }
    const bool is_comparison =
        expected_kind == OmpExactSubexpressionKind::equal ||
        expected_kind == OmpExactSubexpressionKind::not_equal ||
        expected_kind == OmpExactSubexpressionKind::less ||
        expected_kind == OmpExactSubexpressionKind::greater ||
        expected_kind == OmpExactSubexpressionKind::less_equal ||
        expected_kind == OmpExactSubexpressionKind::greater_equal;
    if (!SageInterface::is_Fortran_language()) {
      SgType *lhs_type = lhs != NULL ? lhs->get_type() : NULL;
      SgType *rhs_type = rhs != NULL ? rhs->get_type() : NULL;
      const bool arithmetic_operands =
          isOpenMPCxxArithmeticType(lhs_type) &&
          isOpenMPCxxArithmeticType(rhs_type);
      const bool compatible_operands =
          arithmetic_operands ||
          (lhs_type != NULL && rhs_type != NULL &&
           SageInterface::isEquivalentType(lhs_type, rhs_type));
      if (!is_comparison || lhs_type == NULL || rhs_type == NULL ||
          isSgTypeUnknown(lhs_type) != NULL ||
          isSgTypeDefault(lhs_type) != NULL ||
          isSgTypeUnknown(rhs_type) != NULL ||
          isSgTypeDefault(rhs_type) != NULL || !compatible_operands) {
        std::cerr << "REX_OMP_AST_INVARIANT[sage-expression-type]: "
                     "C/C++ OpenMP comparison operation="
                  << static_cast<int>(expected_kind)
                  << " does not have two compatible exact Sage operand types\n";
        ROSE_ABORT();
      }
      if (arithmetic_operands) {
        (void)SageInterface::usualArithmeticConversionType(
            lhs_type, rhs_type, omp_directive_node);
      }
      return SageInterface::is_C_language()
                 ? static_cast<SgType *>(SageBuilder::buildIntType())
                 : static_cast<SgType *>(SageBuilder::buildBoolType());
    }
    if (omp_exprparser_fortran_typed_scope == NULL) {
      return consumeOpenMPExactSubexpressionType(expected_kind);
    }
    SgType *lhs_type = lhs != NULL ? lhs->get_type() : NULL;
    SgType *rhs_type = rhs != NULL ? rhs->get_type() : NULL;
    const bool equivalent = lhs_type != NULL && rhs_type != NULL &&
                            SageInterface::isEquivalentType(lhs_type, rhs_type);
    const bool lhs_source_matches_rhs =
        lhs_type != NULL && rhs_type != NULL &&
        SageInterface::fortranSourceTypeMatchesSemanticExpressionType(lhs_type,
                                                                      rhs_type);
    const bool rhs_source_matches_lhs =
        lhs_type != NULL && rhs_type != NULL &&
        SageInterface::fortranSourceTypeMatchesSemanticExpressionType(rhs_type,
                                                                      lhs_type);
    if (!is_comparison || lhs_type == NULL || rhs_type == NULL ||
        isSgTypeUnknown(lhs_type) != NULL ||
        isSgTypeDefault(lhs_type) != NULL ||
        isSgTypeUnknown(rhs_type) != NULL ||
        isSgTypeDefault(rhs_type) != NULL ||
        (!equivalent && !lhs_source_matches_rhs && !rhs_source_matches_lhs)) {
      std::cerr << "REX_OMP_AST_INVARIANT[fortran-typed-scope-type]: "
                   "REX-owned comparison="
                << static_cast<int>(expected_kind)
                << " does not have two compatible typed operands; lhs="
                << (lhs_type != NULL ? lhs_type->class_name()
                                     : std::string("<null>"))
                << " rhs="
                << (rhs_type != NULL ? rhs_type->class_name()
                                     : std::string("<null>"))
                << " equivalent/source-matches=" << equivalent << "/"
                << lhs_source_matches_rhs << "/" << rhs_source_matches_lhs
                << "\n";
      ROSE_ABORT();
    }
    return SageBuilder::buildBoolType();
  }

  void omp_exprparser_add_context_variable_symbol(SgVariableSymbol * symbol) {
    requireIdleOpenMPExpressionParser("context-symbol installation");
    if (symbol == NULL || symbol->get_declaration() == NULL ||
        symbol->get_name().is_null()) {
      std::cerr << "REX_OMP_AST_INVARIANT[context-symbol]: invalid "
                   "directive-local variable symbol\n";
      ROSE_ABORT();
    }
    const std::string name = symbol->get_name().getString();
    if (!omp_exprparser_context_variable_symbols.emplace(name, symbol).second) {
      std::cerr << "REX_OMP_AST_INVARIANT[context-symbol]: duplicate "
                   "directive-local variable '"
                << name << "'\n";
      ROSE_ABORT();
    }
  }

  void omp_exprparser_add_context_name_expression(const std::string &name) {
    requireIdleOpenMPExpressionParser("context-name installation");
    if (name.empty()) {
      std::cerr << "REX_OMP_AST_INVARIANT[context-name]: empty "
                   "directive-local name\n";
      ROSE_ABORT();
    }
    omp_exprparser_context_name_expressions.insert(name);
  }

  static SgExpression *buildOpenACCExactValueExpression(
      const OpenACCCxxExactSemanticBindings::Binding &binding) {
    if (binding.kind() != OpenACCCxxExactSemanticBindings::BindingKind::value ||
        binding.semanticNode() == NULL || binding.symbol() == NULL ||
        binding.symbol()->get_name().getString() != binding.spelling()) {
      std::cerr << "REX_ACC_AST_INVARIANT[cxx-semantic-symbol]: identifier '"
                << binding.spelling()
                << "' has no exact producer-owned Sage value identity\n";
      ROSE_ABORT();
    }
    if (SgVariableSymbol *variable = isSgVariableSymbol(binding.symbol())) {
      return SageBuilder::buildVarRefExp(variable);
    }
    if (SgEnumFieldSymbol *enumerator = isSgEnumFieldSymbol(binding.symbol())) {
      return SageBuilder::buildEnumVal(enumerator);
    }
    if (SgTemplateMemberFunctionSymbol *function =
            isSgTemplateMemberFunctionSymbol(binding.symbol())) {
      return SageBuilder::buildTemplateMemberFunctionRefExp_nfi(function, false,
                                                                false);
    }
    if (SgMemberFunctionSymbol *function =
            isSgMemberFunctionSymbol(binding.symbol())) {
      return SageBuilder::buildMemberFunctionRefExp(function, false, false);
    }
    if (SgTemplateFunctionSymbol *function =
            isSgTemplateFunctionSymbol(binding.symbol())) {
      return SageBuilder::buildTemplateFunctionRefExp_nfi(function);
    }
    if (SgFunctionSymbol *function = isSgFunctionSymbol(binding.symbol())) {
      return SageBuilder::buildFunctionRefExp(function);
    }
    std::cerr << "REX_ACC_AST_INVARIANT[cxx-semantic-symbol]: identifier '"
              << binding.spelling() << "' changed exact Sage symbol kind\n";
    ROSE_ABORT();
  }

  static SgSymbol *unwrapOpenMPSageAlias(SgSymbol *symbol,
                                         const std::string &spelling) {
    std::set<SgAliasSymbol *> aliases;
    while (SgAliasSymbol *alias = isSgAliasSymbol(symbol)) {
      if (alias->get_alias() == NULL || !aliases.insert(alias).second) {
        std::cerr << "REX_OMP_AST_INVARIANT[sage-name-lookup]: identifier '"
                  << spelling << "' has an invalid Sage alias chain\n";
        ROSE_ABORT();
      }
      symbol = alias->get_alias();
    }
    return symbol;
  }

  static SgScopeStatement *canonicalOpenMPLookupScope(
      SgScopeStatement *scope) {
    if (SgNamespaceDefinitionStatement *namespace_definition =
            isSgNamespaceDefinitionStatement(scope)) {
      if (namespace_definition->get_global_definition() == NULL) {
        std::cerr << "REX_OMP_AST_INVARIANT[sage-name-lookup]: namespace "
                     "scope has no canonical definition\n";
        ROSE_ABORT();
      }
      return namespace_definition->get_global_definition();
    }
    return scope;
  }

  static std::vector<SgSymbol *>
  lookupDirectOpenMPSageSymbols(const std::string &name,
                               SgScopeStatement *scope) {
    scope = canonicalOpenMPLookupScope(scope);
    if (name.empty() || scope == NULL || scope->get_symbol_table() == NULL ||
        scope->get_symbol_table()->get_table() == NULL) {
      std::cerr << "REX_OMP_AST_INVARIANT[sage-name-lookup]: direct lookup "
                   "has no exact name, scope, or symbol table\n";
      ROSE_ABORT();
    }

    std::vector<SgSymbol *> result;
    std::set<SgSymbol *> identities;
    const auto range =
        scope->get_symbol_table()->get_table()->equal_range(SgName(name));
    for (auto entry = range.first; entry != range.second; ++entry) {
      SgSymbol *symbol = unwrapOpenMPSageAlias(entry->second, name);
      if (symbol != NULL && identities.insert(symbol).second) {
        result.push_back(symbol);
      }
    }
    return result;
  }

  static std::vector<SgStatement *>
  directOpenMPLexicalStatements(SgScopeStatement *scope) {
    std::vector<SgStatement *> result;
    if (SgGlobal *global = isSgGlobal(scope)) {
      result.assign(global->get_declarations().begin(),
                    global->get_declarations().end());
      return result;
    }
    if (SgNamespaceDefinitionStatement *namespace_definition =
            isSgNamespaceDefinitionStatement(scope)) {
      result.assign(namespace_definition->get_declarations().begin(),
                    namespace_definition->get_declarations().end());
      return result;
    }
    if (SgBasicBlock *block = isSgBasicBlock(scope)) {
      result.assign(block->get_statements().begin(),
                    block->get_statements().end());
      return result;
    }
    return result;
  }

  static SgStatement *
  requireDirectOpenMPLexicalAnchor(SgScopeStatement *scope, SgNode *node,
                                   const char *role) {
    if (scope == NULL || node == NULL || role == NULL) {
      std::cerr << "REX_OMP_AST_INVARIANT[sage-using-visibility]: role="
                << (role != NULL ? role : "<null>")
                << " has no exact scope or node\n";
      ROSE_ABORT();
    }
    std::set<SgNode *> visited;
    SgNode *candidate = node;
    while (candidate != NULL && candidate != scope) {
      if (!visited.insert(candidate).second) {
        std::cerr << "REX_OMP_AST_INVARIANT[sage-using-visibility]: role="
                  << role << " has a cyclic parent chain\n";
        ROSE_ABORT();
      }
      if (candidate->get_parent() == scope) {
        SgStatement *statement = isSgStatement(candidate);
        if (statement == NULL) {
          std::cerr << "REX_OMP_AST_INVARIANT[sage-using-visibility]: role="
                    << role
                    << " reaches its scope through a non-statement owner\n";
          ROSE_ABORT();
        }
        return statement;
      }
      candidate = candidate->get_parent();
    }
    std::cerr << "REX_OMP_AST_INVARIANT[sage-using-visibility]: role=" << role
              << " is not structurally owned by its lookup scope\n";
    ROSE_ABORT();
  }

  static bool openMPUsingDirectivePrecedesCurrentDirective(
      SgUsingDirectiveStatement *using_directive, SgScopeStatement *scope) {
    if (using_directive == NULL || scope == NULL ||
        using_directive->get_scope() != scope) {
      std::cerr << "REX_OMP_AST_INVARIANT[sage-using-visibility]: using "
                   "directive has no exact lookup-scope ownership\n";
      ROSE_ABORT();
    }

    SgNode *visibility_anchor = using_directive;
    if (SgAuxiliaryDeclarationList *auxiliary =
            isSgAuxiliaryDeclarationList(using_directive->get_parent())) {
      SgNamespaceDeclarationStatement *namespace_declaration =
          using_directive->get_namespaceDeclaration();
      if (auxiliary->get_parent() != scope ||
          scope->get_auxiliary_declarations() != auxiliary ||
          std::count(auxiliary->get_declarations().begin(),
                     auxiliary->get_declarations().end(), using_directive) !=
              1 ||
          scope->statementExistsInScope(using_directive) ||
          namespace_declaration == NULL ||
          !namespace_declaration->get_isUnnamedNamespace()) {
        std::cerr << "REX_OMP_AST_INVARIANT[sage-using-visibility]: "
                     "semantic using directive is not one exact implicit "
                     "anonymous-namespace visibility edge\n";
        ROSE_ABORT();
      }
      visibility_anchor = namespace_declaration;
    } else if (using_directive->get_parent() != scope ||
               !scope->statementExistsInScope(using_directive)) {
      std::cerr << "REX_OMP_AST_INVARIANT[sage-using-visibility]: source "
                   "using directive has no exact lexical edge\n";
      ROSE_ABORT();
    }

    const std::vector<SgStatement *> statements =
        directOpenMPLexicalStatements(scope);
    if (statements.empty()) {
      std::cerr << "REX_OMP_AST_INVARIANT[sage-using-visibility]: scope="
                << scope->class_name()
                << " cannot own a C++ using-directive lexical surface\n";
      ROSE_ABORT();
    }
    SgStatement *using_anchor = requireDirectOpenMPLexicalAnchor(
        scope, visibility_anchor, "using-directive");
    SgStatement *directive_anchor = requireDirectOpenMPLexicalAnchor(
        scope, omp_directive_node, "OpenMP-directive");
    const auto using_position =
        std::find(statements.begin(), statements.end(), using_anchor);
    const auto directive_position =
        std::find(statements.begin(), statements.end(), directive_anchor);
    if (using_position == statements.end() ||
        directive_position == statements.end() ||
        std::count(statements.begin(), statements.end(), using_anchor) != 1 ||
        std::count(statements.begin(), statements.end(), directive_anchor) !=
            1) {
      std::cerr << "REX_OMP_AST_INVARIANT[sage-using-visibility]: lookup "
                   "scope has no unique lexical anchor for the using or "
                   "OpenMP directive\n";
      ROSE_ABORT();
    }
    return using_position < directive_position;
  }

  static std::vector<SgUsingDirectiveStatement *>
  directOpenMPSageUsingDirectives(SgScopeStatement *scope) {
    std::vector<SgUsingDirectiveStatement *> result;
    std::set<SgUsingDirectiveStatement *> identities;
    for (SgStatement *statement : directOpenMPLexicalStatements(scope)) {
      if (SgUsingDirectiveStatement *using_directive =
              isSgUsingDirectiveStatement(statement)) {
        if (!identities.insert(using_directive).second) {
          std::cerr << "REX_OMP_AST_INVARIANT[sage-using-visibility]: "
                       "duplicate lexical using-directive identity\n";
          ROSE_ABORT();
        }
        result.push_back(using_directive);
      }
    }
    if (SgAuxiliaryDeclarationList *auxiliary =
            scope->get_auxiliary_declarations()) {
      if (auxiliary->get_parent() != scope) {
        std::cerr << "REX_OMP_AST_INVARIANT[sage-using-visibility]: "
                     "auxiliary declaration list has no exact scope owner\n";
        ROSE_ABORT();
      }
      for (SgDeclarationStatement *declaration :
           auxiliary->get_declarations()) {
        if (SgUsingDirectiveStatement *using_directive =
                isSgUsingDirectiveStatement(declaration)) {
          if (!identities.insert(using_directive).second) {
            std::cerr << "REX_OMP_AST_INVARIANT[sage-using-visibility]: "
                         "using-directive has both lexical and auxiliary "
                         "ownership\n";
            ROSE_ABORT();
          }
          result.push_back(using_directive);
        }
      }
    }
    return result;
  }

  static std::vector<SgSymbol *>
  lookupImportedOpenMPSageSymbols(const std::string &name,
                                  SgScopeStatement *scope,
                                  std::set<SgScopeStatement *> &active_scopes) {
    scope = canonicalOpenMPLookupScope(scope);
    if (scope == NULL || !active_scopes.insert(scope).second) {
      std::cerr << "REX_OMP_AST_INVARIANT[sage-using-visibility]: cyclic "
                   "using-directive namespace graph\n";
      ROSE_ABORT();
    }

    std::vector<SgSymbol *> result;
    std::set<SgSymbol *> identities;
    for (SgUsingDirectiveStatement *using_directive :
         directOpenMPSageUsingDirectives(scope)) {
      if (!openMPUsingDirectivePrecedesCurrentDirective(using_directive,
                                                        scope)) {
        continue;
      }
      SgNamespaceDeclarationStatement *namespace_declaration =
          using_directive->get_namespaceDeclaration();
      SgNamespaceDefinitionStatement *namespace_definition =
          namespace_declaration != NULL ? namespace_declaration->get_definition()
                                        : NULL;
      if (namespace_definition == NULL ||
          namespace_definition->get_global_definition() == NULL) {
        std::cerr << "REX_OMP_AST_INVARIANT[sage-using-visibility]: using "
                     "directive has no exact canonical namespace target\n";
        ROSE_ABORT();
      }
      SgScopeStatement *target_scope =
          namespace_definition->get_global_definition();
      for (SgSymbol *symbol :
           lookupDirectOpenMPSageSymbols(name, target_scope)) {
        if (identities.insert(symbol).second) {
          result.push_back(symbol);
        }
      }
      if (result.empty()) {
        for (SgSymbol *symbol : lookupImportedOpenMPSageSymbols(
                 name, target_scope, active_scopes)) {
          if (identities.insert(symbol).second) {
            result.push_back(symbol);
          }
        }
      }
    }
    active_scopes.erase(scope);
    return result;
  }

  static std::vector<SgSymbol *>
  lookupLexicalOpenMPSageSymbols(const std::string &name,
                                SgScopeStatement *scope) {
    while (scope != NULL) {
      std::vector<SgSymbol *> result =
          lookupDirectOpenMPSageSymbols(name, scope);
      if (result.empty()) {
        std::set<SgScopeStatement *> active_scopes;
        result = lookupImportedOpenMPSageSymbols(name, scope, active_scopes);
      }
      if (!result.empty()) {
        return result;
      }
      if (isSgGlobal(scope) != NULL || scope->get_parent() == NULL) {
        break;
      }
      scope = scope->get_scope();
    }
    return {};
  }

  static SgScopeStatement *
  requireOpenMPQualifierScope(SgSymbol *symbol, const std::string &spelling) {
    symbol = unwrapOpenMPSageAlias(symbol, spelling);
    if (SgNamespaceSymbol *namespace_symbol = isSgNamespaceSymbol(symbol)) {
      SgNamespaceDeclarationStatement *declaration =
          namespace_symbol->get_declaration();
      SgNamespaceDefinitionStatement *definition =
          declaration != NULL ? declaration->get_definition() : NULL;
      if (definition != NULL && definition->get_global_definition() != NULL) {
        return definition->get_global_definition();
      }
    }
    if (SgClassSymbol *class_symbol = isSgClassSymbol(symbol)) {
      SgClassDeclaration *declaration = class_symbol->get_declaration();
      if (declaration != NULL &&
          declaration->get_definingDeclaration() != NULL) {
        declaration =
            isSgClassDeclaration(declaration->get_definingDeclaration());
      }
      if (declaration != NULL && declaration->get_definition() != NULL) {
        return declaration->get_definition();
      }
    }
    std::cerr << "REX_OMP_AST_INVARIANT[sage-name-lookup]: qualifier '"
              << spelling << "' has no exact namespace or class scope\n";
    ROSE_ABORT();
  }

  static bool isOpenMPSageValueSymbol(SgSymbol *symbol) {
    return isSgVariableSymbol(symbol) != NULL ||
           isSgEnumFieldSymbol(symbol) != NULL ||
           isSgTemplateMemberFunctionSymbol(symbol) != NULL ||
           isSgMemberFunctionSymbol(symbol) != NULL ||
           isSgTemplateFunctionSymbol(symbol) != NULL ||
           isSgFunctionSymbol(symbol) != NULL;
  }

  static SgSymbol *requireUniqueOpenMPSageValueSymbol(
      const std::vector<SgSymbol *> &symbols, const std::string &spelling) {
    std::vector<SgSymbol *> values;
    for (SgSymbol *symbol : symbols) {
      symbol = unwrapOpenMPSageAlias(symbol, spelling);
      if (isOpenMPSageValueSymbol(symbol)) {
        values.push_back(symbol);
      }
    }
    if (values.size() != 1) {
      std::cerr << "REX_OMP_AST_INVARIANT[sage-name-lookup]: identifier '"
                << spelling << "' has "
                << (values.empty() ? "no" : "multiple")
                << " exact Sage value identities\n";
      ROSE_ABORT();
    }
    return values.front();
  }

  static SgExpression *buildOpenMPSageValueExpression(
      SgSymbol *symbol, const std::string &spelling) {
    symbol = unwrapOpenMPSageAlias(symbol, spelling);
    if (SgVariableSymbol *variable = isSgVariableSymbol(symbol)) {
      if (!isKnownVariableSymbolType(variable)) {
        std::cerr << "REX_OMP_AST_INVARIANT[sage-name-lookup]: variable '"
                  << spelling << "' has no exact Sage type\n";
        ROSE_ABORT();
      }
      return SageBuilder::buildVarRefExp(variable);
    }
    if (SgEnumFieldSymbol *enumerator = isSgEnumFieldSymbol(symbol)) {
      return SageBuilder::buildEnumVal(enumerator);
    }
    if (SgTemplateMemberFunctionSymbol *function =
            isSgTemplateMemberFunctionSymbol(symbol)) {
      return SageBuilder::buildTemplateMemberFunctionRefExp_nfi(function, false,
                                                                false);
    }
    if (SgMemberFunctionSymbol *function =
            isSgMemberFunctionSymbol(symbol)) {
      return SageBuilder::buildMemberFunctionRefExp(function, false, false);
    }
    if (SgTemplateFunctionSymbol *function =
            isSgTemplateFunctionSymbol(symbol)) {
      return SageBuilder::buildTemplateFunctionRefExp_nfi(function);
    }
    if (SgFunctionSymbol *function = isSgFunctionSymbol(symbol)) {
      return SageBuilder::buildFunctionRefExp(function);
    }
    std::cerr << "REX_OMP_AST_INVARIANT[sage-name-lookup]: identifier '"
              << spelling << "' is not an exact Sage value identity\n";
    ROSE_ABORT();
  }

  static SgExpression *buildFortranOpenMPExactValueExpression(
      const OmpFortranExactSemanticBindings::Binding &binding) {
    using BindingKind = OmpFortranExactSemanticBindings::BindingKind;
    if (binding.kind() == BindingKind::directive_local) {
      const auto local =
          omp_exprparser_context_variable_symbols.find(binding.spelling());
      if (local == omp_exprparser_context_variable_symbols.end() ||
          local->second == NULL || local->second->get_declaration() == NULL ||
          !isKnownVariableSymbolType(local->second) ||
          binding.directiveLocalType() == NULL ||
          local->second->get_declaration()->get_type() !=
              binding.directiveLocalType()) {
        std::cerr << "REX_OMP_AST_INVARIANT[fortran-exact-semantic-local]: "
                     "directive-local identifier '"
                  << binding.spelling()
                  << "' has no exact typed local Sage identity\n";
        ROSE_ABORT();
      }
      return SageBuilder::buildVarRefExp(local->second);
    }
    if (binding.kind() == BindingKind::syntax_name) {
      SgOmpNameExpression *expression =
          new SgOmpNameExpression(binding.spelling());
      SageInterface::setOneSourcePositionForTransformation(expression);
      return expression;
    }
    if (binding.kind() != BindingKind::value ||
        binding.semanticNode() == NULL || binding.symbol() == NULL ||
        binding.semanticNode() != binding.symbol()) {
      std::cerr << "REX_OMP_AST_INVARIANT[fortran-exact-semantic-kind]: "
                   "identifier '"
                << binding.spelling()
                << "' has no exact producer-owned Sage value identity\n";
      ROSE_ABORT();
    }
    if (SgVariableSymbol *variable = isSgVariableSymbol(binding.symbol())) {
      return SageBuilder::buildVarRefExp(variable);
    }
    if (SgEnumFieldSymbol *enumerator = isSgEnumFieldSymbol(binding.symbol())) {
      return SageBuilder::buildEnumVal(enumerator);
    }
    if (SgFunctionSymbol *function = isSgFunctionSymbol(binding.symbol())) {
      return SageBuilder::buildFunctionRefExp(function);
    }
    std::cerr << "REX_OMP_AST_INVARIANT[fortran-exact-semantic-kind]: "
                 "identifier '"
              << binding.spelling() << "' changed exact Sage symbol kind\n";
    ROSE_ABORT();
  }

  static SgExpression *buildOpenMPMemberReference(
      SgExpression * base, const std::string &name, bool through_pointer) {
    ROSE_ASSERT(base != NULL);
    if (name.empty()) {
      std::cerr
          << "REX_OMP_AST_INVARIANT[member-reference]: empty member name\n";
      ROSE_ABORT();
    }

    SgType *owner_type = base->get_type();
    if (owner_type != NULL) {
      owner_type = owner_type->stripType(
          SgType::STRIP_MODIFIER_TYPE | SgType::STRIP_TYPEDEF_TYPE |
          SgType::STRIP_REFERENCE_TYPE | SgType::STRIP_RVALUE_REFERENCE_TYPE);
    }
    if (through_pointer) {
      SgPointerType *pointer_type = isSgPointerType(owner_type);
      if (pointer_type == NULL) {
        std::cerr << "REX_OMP_AST_INVARIANT[member-reference]: '" << name
                  << "' uses -> with a non-pointer base\n";
        ROSE_ABORT();
      }
      owner_type = pointer_type->get_base_type();
      if (owner_type != NULL) {
        owner_type = owner_type->stripType(
            SgType::STRIP_MODIFIER_TYPE | SgType::STRIP_TYPEDEF_TYPE |
            SgType::STRIP_REFERENCE_TYPE | SgType::STRIP_RVALUE_REFERENCE_TYPE);
      }
    }

    SgClassType *class_type = isSgClassType(owner_type);
    SgClassDeclaration *class_decl =
        class_type != NULL ? isSgClassDeclaration(class_type->get_declaration())
                           : NULL;
    if (class_decl != NULL && class_decl->get_definingDeclaration() != NULL) {
      class_decl = isSgClassDeclaration(class_decl->get_definingDeclaration());
    }
    SgClassDefinition *class_def =
        class_decl != NULL ? class_decl->get_definition() : NULL;
    if (class_def == NULL) {
      std::cerr << "REX_OMP_AST_INVARIANT[member-reference]: exact base for '"
                << name << "' has no class definition\n";
      ROSE_ABORT();
    }
    if (SageInterface::is_Fortran_language()) {
      if (omp_exprparser_fortran_typed_scope != NULL) {
        SgVariableSymbol *member = class_def->lookup_variable_symbol(name);
        if (member == NULL || member->get_declaration() == NULL ||
            member->get_declaration()->get_scope() != class_def ||
            !isKnownVariableSymbolType(member)) {
          std::cerr << "REX_OMP_AST_INVARIANT[fortran-typed-scope-member]: '"
                    << name
                    << "' is not one exact typed component of its derived "
                       "base\n";
          ROSE_ABORT();
        }
        return SageBuilder::buildVarRefExp(member);
      }
      const auto &binding = consumeFortranOpenMPExactSemanticBinding(name);
      SgExpression *member = buildFortranOpenMPExactValueExpression(binding);
      SgVarRefExp *reference = isSgVarRefExp(member);
      SgInitializedName *declaration =
          reference != NULL && reference->get_symbol() != NULL
              ? reference->get_symbol()->get_declaration()
              : NULL;
      if (declaration == NULL || declaration->get_scope() != class_def) {
        std::cerr << "REX_OMP_AST_INVARIANT[fortran-exact-semantic-member]: '"
                  << name
                  << "' is not the exact component of its typed derived base\n";
        ROSE_ABORT();
      }
      return member;
    }
    if (omp_exprparser_openacc_cxx_semantic_bindings == NULL) {
      const std::vector<SgSymbol *> symbols =
          lookupDirectOpenMPSageSymbols(name, class_def);
      SgSymbol *symbol =
          requireUniqueOpenMPSageValueSymbol(symbols, name);
      if (isSgVariableSymbol(symbol) == NULL &&
          isSgMemberFunctionSymbol(symbol) == NULL &&
          isSgTemplateMemberFunctionSymbol(symbol) == NULL) {
        std::cerr << "REX_OMP_AST_INVARIANT[member-reference]: exact Sage "
                     "binding '"
                  << name << "' is not a direct class member\n";
        ROSE_ABORT();
      }
      return buildOpenMPSageValueExpression(symbol, name);
    }

    const OpenACCCxxExactSemanticBindings::Binding &binding =
        consumeOpenACCCxxExactSemanticBinding(name);
    if (binding.kind() != OpenACCCxxExactSemanticBindings::BindingKind::value) {
      std::cerr << "REX_ACC_AST_INVARIANT[cxx-semantic-member]: member '"
                << name
                << "' is not an exact producer value\n";
      ROSE_ABORT();
    }
    SgExpression *member = buildOpenACCExactValueExpression(binding);
    if (isSgVarRefExp(member) == NULL &&
        isSgMemberFunctionRefExp(member) == NULL &&
        isSgTemplateMemberFunctionRefExp(member) == NULL) {
      std::cerr << "REX_OMP_AST_INVARIANT[member-reference]: exact binding '"
                << name << "' is not a class member value or function\n";
      ROSE_ABORT();
    }
    return member;
  }

  static SgType *consumeOpenACCExactMemberReferenceType(
      OmpExactSubexpressionKind expected_kind, SgExpression * base,
      SgExpression * member) {
    if (omp_exprparser_exact_subexpression_types != NULL) {
      return consumeOpenMPExactSubexpressionType(expected_kind);
    }
    const bool exact_kind =
        expected_kind == OmpExactSubexpressionKind::member_dot ||
        expected_kind == OmpExactSubexpressionKind::member_arrow;
    SgType *member_type = member != NULL ? member->get_type() : NULL;
    if (!SageInterface::is_Fortran_language()) {
      if (!exact_kind || base == NULL || member == NULL ||
          member_type == NULL || isSgTypeUnknown(member_type) != NULL ||
          isSgTypeDefault(member_type) != NULL ||
          (isSgVarRefExp(member) == NULL &&
           isSgMemberFunctionRefExp(member) == NULL &&
           isSgTemplateMemberFunctionRefExp(member) == NULL)) {
        std::cerr << "REX_OMP_AST_INVARIANT[sage-expression-type]: C/C++ "
                     "OpenMP member access has no exact typed base or member\n";
        ROSE_ABORT();
      }
      return member_type;
    }
    if (omp_exprparser_fortran_typed_scope == NULL) {
      return consumeOpenMPExactSubexpressionType(expected_kind);
    }
    if (!exact_kind || base == NULL || member == NULL || member_type == NULL ||
        isSgTypeUnknown(member_type) != NULL ||
        isSgTypeDefault(member_type) != NULL ||
        (isSgVarRefExp(member) == NULL &&
         isSgMemberFunctionRefExp(member) == NULL &&
         isSgTemplateMemberFunctionRefExp(member) == NULL)) {
      std::cerr << "REX_OMP_AST_INVARIANT[fortran-typed-scope-type]: "
                   "REX-owned member access has no exact typed base, member, "
                   "or operation kind\n";
      ROSE_ABORT();
    }
    return member_type;
  }

  static bool containsFortranArraySectionArgument(SgExprListExp * args) {
    if (args == NULL) {
      return false;
    }
    const SgExpressionPtrList &exprs = args->get_expressions();
    for (SgExpression *expr : exprs) {
      if (isSgSubscriptExpression(expr) != NULL) {
        return true;
      }
    }
    return false;
  }

  static bool isFortranArrayLikeType(SgType * type) {
    if (type == NULL) {
      return false;
    }

    type = type->stripType(
        SgType::STRIP_MODIFIER_TYPE | SgType::STRIP_TYPEDEF_TYPE |
        SgType::STRIP_REFERENCE_TYPE | SgType::STRIP_RVALUE_REFERENCE_TYPE);
    if (isSgArrayType(type) != NULL) {
      return true;
    }

    if (SgPointerType *ptr_type = isSgPointerType(type)) {
      SgType *base_type = ptr_type->get_base_type();
      if (base_type != NULL) {
        base_type = base_type->stripType(
            SgType::STRIP_MODIFIER_TYPE | SgType::STRIP_TYPEDEF_TYPE |
            SgType::STRIP_REFERENCE_TYPE | SgType::STRIP_RVALUE_REFERENCE_TYPE);
        if (isSgArrayType(base_type) != NULL) {
          return true;
        }
      }
    }

    return false;
  }

  static SgVariableSymbol *lookupFortranTypedVariableSymbolFromExpression(
      SgExpression * expr) {
    if (!SageInterface::is_Fortran_language() || expr == NULL) {
      return NULL;
    }

    SgVarRefExp *vref = isSgVarRefExp(expr);
    if (vref == NULL) {
      return NULL;
    }

    if (SgVariableSymbol *symbol = isSgVariableSymbol(vref->get_symbol())) {
      if (!isKnownVariableSymbolType(symbol)) {
        std::cerr << "REX_OMP_AST_INVARIANT[fortran-exact-semantic-type]: "
                     "producer-owned variable identity has no exact type\n";
        ROSE_ABORT();
      }
      return symbol;
    }

    return NULL;
  }

  static bool shouldTreatFortranParenAsArrayRef(SgExpression * base,
                                                SgExprListExp * args) {
    if (!SageInterface::is_Fortran_language() || base == NULL || args == NULL) {
      return false;
    }

    if (containsFortranArraySectionArgument(args)) {
      return true;
    }

    if (SgVariableSymbol *symbol =
            lookupFortranTypedVariableSymbolFromExpression(base)) {
      if (isFortranArrayLikeType(symbol->get_type())) {
        return true;
      }
    }

    return isFortranArrayLikeType(base->get_type());
  }

  static SgType *consumeOpenMPExactArrayReferenceType(
      OmpExactSubexpressionKind expected_kind, SgExpression * base,
      SgExprListExp * args) {
    if (omp_exprparser_fortran_typed_scope == NULL) {
      return consumeOpenMPExactSubexpressionType(expected_kind);
    }
    if (base == NULL || args == NULL ||
        (expected_kind != OmpExactSubexpressionKind::subscript &&
         expected_kind != OmpExactSubexpressionKind::array_section)) {
      std::cerr << "REX_OMP_AST_INVARIANT[fortran-typed-scope-type]: "
                   "REX-owned array reference has no exact base, subscript "
                   "list, or operation kind\n";
      ROSE_ABORT();
    }

    SgType *base_type = base->get_type();
    if (base_type != NULL) {
      base_type = base_type->stripType(
          SgType::STRIP_MODIFIER_TYPE | SgType::STRIP_TYPEDEF_TYPE |
          SgType::STRIP_REFERENCE_TYPE | SgType::STRIP_RVALUE_REFERENCE_TYPE);
    }
    if (SgPointerType *pointer = isSgPointerType(base_type)) {
      base_type = pointer->get_base_type();
      if (base_type != NULL) {
        base_type = base_type->stripType(
            SgType::STRIP_MODIFIER_TYPE | SgType::STRIP_TYPEDEF_TYPE |
            SgType::STRIP_REFERENCE_TYPE | SgType::STRIP_RVALUE_REFERENCE_TYPE);
      }
    }
    SgArrayType *array_type = isSgArrayType(base_type);
    const SgExpressionPtrList &subscripts = args->get_expressions();
    if (array_type == NULL || array_type->get_base_type() == NULL ||
        array_type->get_rank() <= 0 ||
        static_cast<std::size_t>(array_type->get_rank()) != subscripts.size()) {
      std::cerr << "REX_OMP_AST_INVARIANT[fortran-typed-scope-type]: "
                   "REX-owned array reference does not match one exact typed "
                   "array rank\n";
      ROSE_ABORT();
    }

    std::size_t section_rank = 0;
    for (SgExpression *subscript : subscripts) {
      if (subscript == NULL) {
        std::cerr << "REX_OMP_AST_INVARIANT[fortran-typed-scope-type]: "
                     "REX-owned array reference has a null subscript\n";
        ROSE_ABORT();
      }
      if (SgSubscriptExpression *section = isSgSubscriptExpression(subscript)) {
        auto exact_bound = [section](SgExpression *bound) {
          if (bound == NULL || bound->get_parent() != section) {
            return false;
          }
          if (SgNullExpression *absence = isSgNullExpression(bound)) {
            return absence->get_role() ==
                   SgNullExpression::e_null_expression_syntactic_absence;
          }
          SgType *type = bound->get_type();
          return type != NULL && isSgTypeUnknown(type) == NULL &&
                 isSgTypeDefault(type) == NULL;
        };
        if (!exact_bound(section->get_lowerBound()) ||
            !exact_bound(section->get_upperBound()) ||
            !exact_bound(section->get_stride())) {
          std::cerr << "REX_OMP_AST_INVARIANT[fortran-typed-scope-type]: "
                       "REX-owned array section has an incomplete typed "
                       "triplet\n";
          ROSE_ABORT();
        }
        ++section_rank;
      } else {
        SgType *subscript_type = subscript->get_type();
        if (subscript_type == NULL || isSgTypeUnknown(subscript_type) != NULL ||
            isSgTypeDefault(subscript_type) != NULL) {
          std::cerr << "REX_OMP_AST_INVARIANT[fortran-typed-scope-type]: "
                       "REX-owned array reference has an untyped scalar "
                       "subscript\n";
          ROSE_ABORT();
        }
      }
    }
    if ((expected_kind == OmpExactSubexpressionKind::subscript &&
         section_rank != 0) ||
        (expected_kind == OmpExactSubexpressionKind::array_section &&
         section_rank == 0)) {
      std::cerr << "REX_OMP_AST_INVARIANT[fortran-typed-scope-type]: "
                   "REX-owned array operation kind disagrees with its exact "
                   "section rank\n";
      ROSE_ABORT();
    }
    if (section_rank == 0) {
      return array_type->get_base_type();
    }

    SgExprListExp *dimensions = SageBuilder::buildExprListExp_nfi();
    if (dimensions == NULL) {
      std::cerr << "REX_OMP_AST_INVARIANT[fortran-typed-scope-type]: "
                   "cannot construct exact section result dimensions\n";
      ROSE_ABORT();
    }
    for (std::size_t dimension = 0; dimension < section_rank; ++dimension) {
      SgColonShapeExp *shape = SageBuilder::buildColonShapeExp_nfi();
      if (shape == NULL) {
        std::cerr << "REX_OMP_AST_INVARIANT[fortran-typed-scope-type]: "
                     "cannot construct exact section result shape\n";
        ROSE_ABORT();
      }
      SageBuilder::initializeSemanticExpressionSourceProvenance(shape);
      SageInterface::appendExpression(dimensions, shape);
    }
    SageBuilder::initializeSemanticExpressionSourceProvenance(dimensions);
    SgArrayType *result =
        SageBuilder::buildArrayType(array_type->get_base_type(), dimensions);
    if (result == NULL ||
        result->get_rank() != static_cast<int>(section_rank)) {
      std::cerr << "REX_OMP_AST_INVARIANT[fortran-typed-scope-type]: "
                   "section result did not retain its exact typed rank\n";
      ROSE_ABORT();
    }
    return result;
  }

  static SgExpression *buildPostfixParenExpression(SgExpression * base,
                                                   SgExprListExp * args) {
    if (base == NULL || args == NULL) {
      return NULL;
    }

    if (shouldTreatFortranParenAsArrayRef(base, args)) {
      arraySection = containsFortranArraySectionArgument(args);
      // Keep Fortran multi-dimensional subscripts as one list node so
      // unparsing preserves "a(i,j,k)" rather than "a(i)(j)(k)".
      const OmpExactSubexpressionKind operation =
          arraySection ? OmpExactSubexpressionKind::array_section
                       : OmpExactSubexpressionKind::subscript;
      return SageBuilder::buildPntrArrRefExp(
          base, args,
          consumeOpenMPExactArrayReferenceType(operation, base, args));
    }

    arraySection = false;
    if (!SageInterface::is_Fortran_language() &&
        omp_exprparser_exact_subexpression_types == NULL) {
      SgType *function_type = base->get_type();
      if (SgPointerType *pointer = isSgPointerType(function_type)) {
        function_type = pointer->get_base_type();
      }
      SgFunctionType *function = isSgFunctionType(function_type);
      SgType *result_type =
          function != NULL ? function->get_return_type() : NULL;
      if (result_type == NULL || isSgTypeUnknown(result_type) != NULL ||
          isSgTypeDefault(result_type) != NULL) {
        std::cerr << "REX_OMP_AST_INVARIANT[sage-expression-type]: C/C++ "
                     "OpenMP call target has no exact Sage function result "
                     "type\n";
        ROSE_ABORT();
      }
      return SageBuilder::buildFunctionCallExp(base, result_type, args);
    }
    return SageBuilder::buildFunctionCallExp(
        base,
        consumeOpenMPExactSubexpressionType(OmpExactSubexpressionKind::call),
        args);
  }

  // add ompparser var
  static bool addOmpVariable(const char *);
  static bool addOmpVariableExpr(SgExpression *);

  static bool isKnownVariableSymbolType(SgVariableSymbol * symbol) {
    if (symbol == NULL) {
      return false;
    }
    SgType *type = symbol->get_type();
    if (type == NULL) {
      return false;
    }
    type = type->stripType(
        SgType::STRIP_MODIFIER_TYPE | SgType::STRIP_TYPEDEF_TYPE |
        SgType::STRIP_REFERENCE_TYPE | SgType::STRIP_RVALUE_REFERENCE_TYPE);
    return isSgTypeUnknown(type) == NULL;
  }

  static SgClassDeclaration *getOpenMPSemanticEnclosingClassDeclaration() {
    SgFunctionDeclaration *function =
        SageInterface::getEnclosingFunctionDeclaration(omp_directive_node,
                                                       true);
    if (SgMemberFunctionDeclaration *member =
            isSgMemberFunctionDeclaration(function)) {
      SgClassDeclaration *declaration =
          isSgClassDeclaration(member->get_associatedClassDeclaration());
      if (declaration == NULL) {
        std::cerr << "REX_OMP_AST_INVARIANT[member-class]: member function '"
                  << member->get_name()
                  << "' has no exact associated class declaration\n";
        ROSE_ABORT();
      }
      if (SgClassDeclaration *defining =
              isSgClassDeclaration(declaration->get_definingDeclaration())) {
        declaration = defining;
      }
      return declaration;
    }

    SgClassDefinition *definition =
        SageInterface::getEnclosingClassDefinition(omp_directive_node, true);
    return definition != NULL ? definition->get_declaration() : NULL;
  }

  static std::vector<SgSymbol *> lookupOpenMPSageMemberSymbols(
      const std::string & name, SgClassDefinition * definition,
      std::set<SgClassDefinition *> & visited) {
    if (name.empty() || definition == NULL) {
      std::cerr << "REX_OMP_AST_INVARIANT[sage-name-lookup]: member lookup "
                   "has no exact name or class definition\n";
      ROSE_ABORT();
    }
    if (!visited.insert(definition).second) {
      return {};
    }

    std::vector<SgSymbol *> direct =
        lookupDirectOpenMPSageSymbols(name, definition);
    if (!direct.empty()) {
      return direct;
    }

    std::vector<SgSymbol *> inherited;
    std::set<SgSymbol *> identities;
    for (SgBaseClass *base : definition->get_inheritances()) {
      SgClassDeclaration *declaration =
          base != NULL ? isSgClassDeclaration(base->get_base_class()) : NULL;
      if (declaration != NULL &&
          declaration->get_definingDeclaration() != NULL) {
        declaration =
            isSgClassDeclaration(declaration->get_definingDeclaration());
      }
      SgClassDefinition *base_definition =
          declaration != NULL ? declaration->get_definition() : NULL;
      if (base == NULL || declaration == NULL || base_definition == NULL) {
        std::cerr << "REX_OMP_AST_INVARIANT[sage-name-lookup]: class '"
                  << (definition->get_declaration() != NULL
                          ? definition->get_declaration()->get_name().str()
                          : std::string("<unnamed>"))
                  << "' has a malformed base-class identity\n";
        ROSE_ABORT();
      }
      for (SgSymbol *symbol : lookupOpenMPSageMemberSymbols(
               name, base_definition, visited)) {
        symbol = unwrapOpenMPSageAlias(symbol, name);
        if (symbol != NULL && identities.insert(symbol).second) {
          inherited.push_back(symbol);
        }
      }
    }
    return inherited;
  }

  static std::vector<SgSymbol *> lookupOpenMPSageEnclosingMemberSymbols(
      const std::string & name) {
    SgClassDeclaration *declaration =
        getOpenMPSemanticEnclosingClassDeclaration();
    if (declaration == NULL) {
      return {};
    }
    if (declaration->get_definingDeclaration() != NULL) {
      declaration =
          isSgClassDeclaration(declaration->get_definingDeclaration());
    }
    SgClassDefinition *definition =
        declaration != NULL ? declaration->get_definition() : NULL;
    if (definition == NULL) {
      std::cerr << "REX_OMP_AST_INVARIANT[sage-name-lookup]: enclosing class "
                   "has no exact defining declaration\n";
      ROSE_ABORT();
    }
    std::set<SgClassDefinition *> visited;
    return lookupOpenMPSageMemberSymbols(name, definition, visited);
  }

  static SgExpression *buildOpenMPThisExpression() {
    SgClassDeclaration *declaration =
        getOpenMPSemanticEnclosingClassDeclaration();
    SgSymbol *symbol = NULL;
    for (SgClassDeclaration *candidate :
         {declaration,
          declaration != NULL
              ? isSgClassDeclaration(
                    declaration->get_firstNondefiningDeclaration())
              : NULL,
          declaration != NULL
              ? isSgClassDeclaration(declaration->get_definingDeclaration())
              : NULL}) {
      if (candidate != NULL && symbol == NULL) {
        symbol = candidate->get_symbol_from_symbol_table();
      }
    }
    if (symbol == NULL || (isSgClassSymbol(symbol) == NULL &&
                           isSgNonrealSymbol(symbol) == NULL)) {
      std::cerr << "REX_OMP_AST_INVARIANT[this-expression]: OpenMP 'this' "
                   "has no enclosing class symbol\n";
      ROSE_ABORT();
    }
    SgMemberFunctionDeclaration *member = isSgMemberFunctionDeclaration(
        SageInterface::getEnclosingFunctionDeclaration(omp_directive_node,
                                                       true));
    SgMemberFunctionType *member_type =
        member != NULL ? isSgMemberFunctionType(member->get_type()) : NULL;
    SgType *this_base_type = symbol->get_type();
    if (member_type == NULL || this_base_type == NULL) {
      std::cerr << "REX_OMP_AST_INVARIANT[this-result-type]: OpenMP 'this' "
                   "has no exact enclosing member-function type\n";
      ROSE_ABORT();
    }
    if (member_type->isConstFunc() || member_type->isVolatileFunc() ||
        member_type->isRestrictFunc()) {
      SgModifierType *qualified_type = new SgModifierType(this_base_type);
      SgTypeModifier &modifier = qualified_type->get_typeModifier();
      if (member_type->isConstFunc()) {
        modifier.get_constVolatileModifier().setConst();
      }
      if (member_type->isVolatileFunc()) {
        modifier.get_constVolatileModifier().setVolatile();
      }
      if (member_type->isRestrictFunc()) {
        modifier.setRestrict();
      }
      SgModifierType *canonical =
          SgModifierType::insertModifierTypeIntoTypeTable(qualified_type);
      if (canonical != qualified_type) {
        delete qualified_type;
      }
      this_base_type = canonical;
    }
    return SageBuilder::buildThisExp(symbol,
                                     SgPointerType::createType(this_base_type));
  }

  static SgExpression *buildOpenMPIdentifierExpression(
      const std::string &name, SgScopeStatement *scope) {
    if (!SageInterface::is_Fortran_language()) {
      if (omp_exprparser_openacc_cxx_semantic_bindings == NULL) {
        if (name == "this") {
          return buildOpenMPThisExpression();
        }
        const auto local = omp_exprparser_context_variable_symbols.find(name);
        if (local != omp_exprparser_context_variable_symbols.end()) {
          if (local->second == NULL ||
              local->second->get_declaration() == NULL ||
              !isKnownVariableSymbolType(local->second)) {
            std::cerr << "REX_OMP_AST_INVARIANT[sage-name-lookup]: "
                         "directive-local identifier '"
                      << name << "' has no exact typed Sage identity\n";
            ROSE_ABORT();
          }
          return SageBuilder::buildVarRefExp(local->second);
        }
        if (omp_exprparser_context_name_expressions.count(name) != 0 ||
            name == "omp_all_memory" || name == "omp_cur_iteration") {
          SgOmpNameExpression *expression = new SgOmpNameExpression(name);
          SageInterface::setOneSourcePositionForTransformation(expression);
          return expression;
        }

        std::vector<std::string> components;
        const bool explicit_global = name.compare(0, 2, "::") == 0;
        size_t begin = explicit_global ? 2 : 0;
        while (begin <= name.size()) {
          const size_t separator = name.find("::", begin);
          const size_t end =
              separator == std::string::npos ? name.size() : separator;
          if (end == begin) {
            std::cerr << "REX_OMP_AST_INVARIANT[sage-name-lookup]: empty "
                         "component in '"
                      << name << "'\n";
            ROSE_ABORT();
          }
          components.push_back(name.substr(begin, end - begin));
          if (separator == std::string::npos) {
            break;
          }
          begin = separator + 2;
        }
        if (components.empty()) {
          std::cerr << "REX_OMP_AST_INVARIANT[sage-name-lookup]: empty "
                       "identifier spelling\n";
          ROSE_ABORT();
        }

        std::vector<SgSymbol *> symbols =
            lookupLexicalOpenMPSageSymbols(components.front(), scope);
        if (symbols.empty() && components.size() == 1 && !explicit_global) {
          symbols =
              lookupOpenMPSageEnclosingMemberSymbols(components.front());
        }
        for (size_t i = 0; i + 1 < components.size(); ++i) {
          if (symbols.size() != 1) {
            std::cerr << "REX_OMP_AST_INVARIANT[sage-name-lookup]: qualifier '"
                      << components[i]
                      << "' does not have one exact Sage identity\n";
            ROSE_ABORT();
          }
          SgScopeStatement *qualified_scope =
              requireOpenMPQualifierScope(symbols.front(), components[i]);
          symbols =
              lookupDirectOpenMPSageSymbols(components[i + 1], qualified_scope);
        }
        SgSymbol *symbol =
            requireUniqueOpenMPSageValueSymbol(symbols, components.back());
        return buildOpenMPSageValueExpression(symbol, components.back());
      }

      std::vector<std::string> components;
      size_t begin = name.compare(0, 2, "::") == 0 ? 2 : 0;
      while (begin <= name.size()) {
        const size_t separator = name.find("::", begin);
        const size_t end =
            separator == std::string::npos ? name.size() : separator;
        if (end == begin) {
          std::cerr << "REX_ACC_AST_INVARIANT[cxx-semantic-qualifier]: "
                       "empty component in '"
                    << name << "'\n";
          ROSE_ABORT();
        }
        components.push_back(name.substr(begin, end - begin));
        if (separator == std::string::npos) {
          break;
        }
        begin = separator + 2;
      }
      if (components.empty()) {
        std::cerr << "REX_ACC_AST_INVARIANT[cxx-semantic-qualifier]: "
                     "empty identifier spelling\n";
        ROSE_ABORT();
      }

      for (size_t i = 0; i + 1 < components.size(); ++i) {
        const OpenACCCxxExactSemanticBindings::Binding &qualifier =
            consumeOpenACCCxxExactSemanticBinding(components[i]);
        if (qualifier.kind() !=
                OpenACCCxxExactSemanticBindings::BindingKind::qualifier ||
            qualifier.semanticNode() == NULL) {
          std::cerr << "REX_ACC_AST_INVARIANT[cxx-semantic-qualifier]: '"
                    << components[i]
                    << "' has no exact producer-owned qualifier identity\n";
          ROSE_ABORT();
        }
      }

      const std::string &terminal_name = components.back();
      const OpenACCCxxExactSemanticBindings::Binding &binding =
          consumeOpenACCCxxExactSemanticBinding(terminal_name);
      if (components.size() == 1 &&
          binding.kind() ==
              OpenACCCxxExactSemanticBindings::BindingKind::current_this) {
        if (terminal_name != "this") {
          std::cerr << "REX_ACC_AST_INVARIANT[cxx-semantic-this]: "
                       "current-object binding spelling changed to '"
                    << terminal_name << "'\n";
          ROSE_ABORT();
        }
        return buildOpenMPThisExpression();
      }

      SgExpression *expression = buildOpenACCExactValueExpression(binding);
      return expression;
    }

    (void)scope;
    const auto &binding = consumeFortranOpenMPExactSemanticBinding(name);
    return buildFortranOpenMPExactValueExpression(binding);
  }

%}

%locations

    /* The %union declaration specifies the entire collection of possible data
    types for semantic values. these names are used in the %token and %type
    declarations to pick one of the types for a terminal or nonterminal symbol
corresponding C type is union name defaults to YYSTYPE.
*/

%union {
  int itype;
  double ftype;
  const char *stype;
  void *ptype; /* For expressions */
}

        /*Some operators have a suffix 2 to avoid name conflicts with ROSE's
          existing types, We may want to reuse them if it is proper.
          experimental BEGIN END are defined by default, we use TARGET_BEGIN
          TARGET_END instead. Liao*/
%token '(' ')' ',' ':' '+' '*' '-' '&' '!' '~' '^' '|' LOGAND LOGOR
              LOGXOR SHLEFT SHRIGHT PLUSPLUS MINUSMINUS PTR_TO '.' LE_OP2 GE_OP2
                  EQ_OP2 NE_OP2 RIGHT_ASSIGN2 LEFT_ASSIGN2 ADD_ASSIGN2
                      SUB_ASSIGN2 MUL_ASSIGN2 DIV_ASSIGN2 MOD_ASSIGN2
                          AND_ASSIGN2 XOR_ASSIGN2 OR_ASSIGN2 DEPEND IN OUT INOUT
                              MERGEABLE LEXICALERROR IDENTIFIER MIN MAX VARLIST
                                  ARRAY_SECTION STATIC_CAST SIZEOF TYPE_INT
        /*We ignore NEWLINE since we only care about the pragma string , We
           relax the syntax check by allowing it as part of line continuation */
%token <stype> ICONSTANT EXPRESSION ID_EXPRESSION HEXCONSTANT
              STRING_LITERAL FORTRAN_COMMON_BLOCK

        /* nonterminals names, types for semantic values, only for nonterminals
         * representing expressions!! not for clauses with expressions.
         */
%type <ptype> expression assignment_expr conditional_expr logical_or_expr
            logical_and_expr inclusive_or_expr exclusive_or_expr and_expr
                equality_expr relational_expr shift_expr additive_expr
                    multiplicative_expr cast_expr primary_expr unary_expr
                        postfix_expr parenthesized_argument_list
                            parenthesized_argument_list_opt
                                parenthesized_argument_item
                                    fortran_subscript_item

        /* start point for the parsing */
%start openmp_expression

%%

        /* NOTE: We can't use the EXPRESSION lexer token directly. Instead, we
         * have to first call omp_parse_expr, because we parse up to the
         * terminating paren.
         */

        openmp_expression : omp_varlist |
    omp_expression | omp_array_section;

omp_varlist : VARLIST { is_ompparser_variable = true; }
variable_list { is_ompparser_variable = false; };

omp_expression : EXPRESSION { is_ompparser_expression = true; }
'(' expression ')' { is_ompparser_expression = false; };

omp_array_section : ARRAY_SECTION { is_ompparser_expression = true; }
'(' array_section_list ')' { is_ompparser_expression = false; };

array_section_list : assignment_expr {
  if (!addOmpVariableExpr((SgExpression *)($1)))
    YYABORT;
}
| array_section_list ',' assignment_expr {
  if (!addOmpVariableExpr((SgExpression *)($3)))
    YYABORT;
};

/* Sara Royuela, 04/27/2012
 * Extending grammar to accept conditional expressions, arithmetic and bitwise
 * expressions and member accesses
 */
expression : assignment_expr

                 assignment_expr
    : conditional_expr |
      logical_or_expr | unary_expr '=' assignment_expr {
  current_exp = SageBuilder::buildAssignOp(
      (SgExpression *)($1), (SgExpression *)($3),
      deriveOpenMPCxxAssignmentResultType(
          OmpExactSubexpressionKind::assign, (SgExpression *)($1),
          (SgExpression *)($3)));
  $$ = current_exp;
}
| unary_expr RIGHT_ASSIGN2 assignment_expr {
  current_exp = SageBuilder::buildRshiftAssignOp(
      (SgExpression *)($1), (SgExpression *)($3),
      deriveOpenMPCxxAssignmentResultType(
          OmpExactSubexpressionKind::rshift_assign, (SgExpression *)($1),
          (SgExpression *)($3)));
  $$ = current_exp;
}
| unary_expr LEFT_ASSIGN2 assignment_expr {
  current_exp = SageBuilder::buildLshiftAssignOp(
      (SgExpression *)($1), (SgExpression *)($3),
      deriveOpenMPCxxAssignmentResultType(
          OmpExactSubexpressionKind::lshift_assign, (SgExpression *)($1),
          (SgExpression *)($3)));
  $$ = current_exp;
}
| unary_expr ADD_ASSIGN2 assignment_expr {
  current_exp = SageBuilder::buildPlusAssignOp(
      (SgExpression *)($1), (SgExpression *)($3),
      deriveOpenMPCxxAssignmentResultType(
          OmpExactSubexpressionKind::add_assign, (SgExpression *)($1),
          (SgExpression *)($3)));
  $$ = current_exp;
}
| unary_expr SUB_ASSIGN2 assignment_expr {
  current_exp = SageBuilder::buildMinusAssignOp(
      (SgExpression *)($1), (SgExpression *)($3),
      deriveOpenMPCxxAssignmentResultType(
          OmpExactSubexpressionKind::subtract_assign, (SgExpression *)($1),
          (SgExpression *)($3)));
  $$ = current_exp;
}
| unary_expr MUL_ASSIGN2 assignment_expr {
  current_exp = SageBuilder::buildMultAssignOp(
      (SgExpression *)($1), (SgExpression *)($3),
      deriveOpenMPCxxAssignmentResultType(
          OmpExactSubexpressionKind::multiply_assign, (SgExpression *)($1),
          (SgExpression *)($3)));
  $$ = current_exp;
}
| unary_expr DIV_ASSIGN2 assignment_expr {
  current_exp = SageBuilder::buildDivAssignOp(
      (SgExpression *)($1), (SgExpression *)($3),
      deriveOpenMPCxxAssignmentResultType(
          OmpExactSubexpressionKind::divide_assign, (SgExpression *)($1),
          (SgExpression *)($3)));
  $$ = current_exp;
}
| unary_expr MOD_ASSIGN2 assignment_expr {
  current_exp = SageBuilder::buildModAssignOp(
      (SgExpression *)($1), (SgExpression *)($3),
      deriveOpenMPCxxAssignmentResultType(
          OmpExactSubexpressionKind::modulo_assign, (SgExpression *)($1),
          (SgExpression *)($3)));
  $$ = current_exp;
}
| unary_expr AND_ASSIGN2 assignment_expr {
  current_exp = SageBuilder::buildAndAssignOp(
      (SgExpression *)($1), (SgExpression *)($3),
      deriveOpenMPCxxAssignmentResultType(
          OmpExactSubexpressionKind::bit_and_assign, (SgExpression *)($1),
          (SgExpression *)($3)));
  $$ = current_exp;
}
| unary_expr XOR_ASSIGN2 assignment_expr {
  current_exp = SageBuilder::buildXorAssignOp(
      (SgExpression *)($1), (SgExpression *)($3),
      deriveOpenMPCxxAssignmentResultType(
          OmpExactSubexpressionKind::bit_xor_assign, (SgExpression *)($1),
          (SgExpression *)($3)));
  $$ = current_exp;
}
| unary_expr OR_ASSIGN2 assignment_expr {
  current_exp = SageBuilder::buildIorAssignOp(
      (SgExpression *)($1), (SgExpression *)($3),
      deriveOpenMPCxxAssignmentResultType(
          OmpExactSubexpressionKind::bit_or_assign, (SgExpression *)($1),
          (SgExpression *)($3)));
  $$ = current_exp;
};

conditional_expr : logical_or_expr '?' assignment_expr ':' assignment_expr {
  current_exp = SageBuilder::buildConditionalExp(
      (SgExpression *)($1), (SgExpression *)($3), (SgExpression *)($5),
      deriveOpenMPCxxConditionalResultType(
          (SgExpression *)($1), (SgExpression *)($3),
          (SgExpression *)($5)));
  $$ = current_exp;
};

logical_or_expr : logical_and_expr | logical_or_expr LOGOR logical_and_expr {
  current_exp = SageBuilder::buildOrOp(
      (SgExpression *)($1), (SgExpression *)($3),
      deriveOpenMPCxxLogicalBinaryResultType(
          OmpExactSubexpressionKind::logical_or, (SgExpression *)($1),
          (SgExpression *)($3)));
  $$ = current_exp;
};

logical_and_expr : inclusive_or_expr |
                   logical_and_expr LOGAND inclusive_or_expr {
  current_exp = SageBuilder::buildAndOp(
      (SgExpression *)($1), (SgExpression *)($3),
      deriveOpenMPCxxLogicalBinaryResultType(
          OmpExactSubexpressionKind::logical_and, (SgExpression *)($1),
          (SgExpression *)($3)));
  $$ = current_exp;
};

inclusive_or_expr : exclusive_or_expr |
                    inclusive_or_expr '|' exclusive_or_expr {
  current_exp = SageBuilder::buildBitOrOp(
      (SgExpression *)($1), (SgExpression *)($3),
      deriveOpenMPCxxIntegerBinaryResultType(
          OmpExactSubexpressionKind::bit_or, (SgExpression *)($1),
          (SgExpression *)($3), false));
  $$ = current_exp;
};

exclusive_or_expr : and_expr | exclusive_or_expr '^' and_expr {
  current_exp = SageBuilder::buildBitXorOp(
      (SgExpression *)($1), (SgExpression *)($3),
      deriveOpenMPCxxIntegerBinaryResultType(
          OmpExactSubexpressionKind::bit_xor, (SgExpression *)($1),
          (SgExpression *)($3), false));
  $$ = current_exp;
}
| exclusive_or_expr LOGXOR and_expr {
  current_exp = SageBuilder::buildBitXorOp(
      (SgExpression *)($1), (SgExpression *)($3),
      deriveOpenMPCxxIntegerBinaryResultType(
          OmpExactSubexpressionKind::bit_xor, (SgExpression *)($1),
          (SgExpression *)($3), false));
  $$ = current_exp;
};

and_expr : equality_expr | and_expr '&' equality_expr {
  current_exp = SageBuilder::buildBitAndOp(
      (SgExpression *)($1), (SgExpression *)($3),
      deriveOpenMPCxxIntegerBinaryResultType(
          OmpExactSubexpressionKind::bit_and, (SgExpression *)($1),
          (SgExpression *)($3), false));
  $$ = current_exp;
};

equality_expr : relational_expr | equality_expr EQ_OP2 relational_expr {
  current_exp = SageBuilder::buildEqualityOp(
      (SgExpression *)($1), (SgExpression *)($3),
      consumeOpenMPExactComparisonSubexpressionType(
          OmpExactSubexpressionKind::equal, (SgExpression *)($1),
          (SgExpression *)($3)));
  $$ = current_exp;
}
| equality_expr NE_OP2 relational_expr {
  current_exp = SageBuilder::buildNotEqualOp(
      (SgExpression *)($1), (SgExpression *)($3),
      consumeOpenMPExactComparisonSubexpressionType(
          OmpExactSubexpressionKind::not_equal, (SgExpression *)($1),
          (SgExpression *)($3)));
  $$ = current_exp;
};

relational_expr : shift_expr | relational_expr '<' shift_expr {
  current_exp = SageBuilder::buildLessThanOp(
      (SgExpression *)($1), (SgExpression *)($3),
      consumeOpenMPExactComparisonSubexpressionType(
          OmpExactSubexpressionKind::less, (SgExpression *)($1),
          (SgExpression *)($3)));
  $$ = current_exp;
  // std::cout<<"debug:
  // buildLessThanOp():\n"<<current_exp->unparseToString()<<std::endl;
}
| relational_expr '>' shift_expr {
  current_exp = SageBuilder::buildGreaterThanOp(
      (SgExpression *)($1), (SgExpression *)($3),
      consumeOpenMPExactComparisonSubexpressionType(
          OmpExactSubexpressionKind::greater, (SgExpression *)($1),
          (SgExpression *)($3)));
  $$ = current_exp;
}
| relational_expr LE_OP2 shift_expr {
  current_exp = SageBuilder::buildLessOrEqualOp(
      (SgExpression *)($1), (SgExpression *)($3),
      consumeOpenMPExactComparisonSubexpressionType(
          OmpExactSubexpressionKind::less_equal, (SgExpression *)($1),
          (SgExpression *)($3)));
  $$ = current_exp;
}
| relational_expr GE_OP2 shift_expr {
  current_exp = SageBuilder::buildGreaterOrEqualOp(
      (SgExpression *)($1), (SgExpression *)($3),
      consumeOpenMPExactComparisonSubexpressionType(
          OmpExactSubexpressionKind::greater_equal, (SgExpression *)($1),
          (SgExpression *)($3)));
  $$ = current_exp;
};

shift_expr : additive_expr | shift_expr SHRIGHT additive_expr {
  current_exp = SageBuilder::buildRshiftOp(
      (SgExpression *)($1), (SgExpression *)($3),
      deriveOpenMPCxxIntegerBinaryResultType(
          OmpExactSubexpressionKind::rshift, (SgExpression *)($1),
          (SgExpression *)($3), true));
  $$ = current_exp;
}
| shift_expr SHLEFT additive_expr {
  current_exp = SageBuilder::buildLshiftOp(
      (SgExpression *)($1), (SgExpression *)($3),
      deriveOpenMPCxxIntegerBinaryResultType(
          OmpExactSubexpressionKind::lshift, (SgExpression *)($1),
          (SgExpression *)($3), true));
  $$ = current_exp;
};

additive_expr : multiplicative_expr | additive_expr '+' multiplicative_expr {
  SgExpression *lhs = (SgExpression *)($1);
  SgExpression *rhs = (SgExpression *)($3);
  current_exp =
      SageBuilder::buildAddOp(lhs, rhs,
                              consumeOpenMPExactBinarySubexpressionType(
                                  OmpExactSubexpressionKind::add, lhs, rhs));
  $$ = current_exp;
}
| additive_expr '-' multiplicative_expr {
  SgExpression *lhs = (SgExpression *)($1);
  SgExpression *rhs = (SgExpression *)($3);
  current_exp = SageBuilder::buildSubtractOp(
      lhs, rhs,
      consumeOpenMPExactBinarySubexpressionType(
          OmpExactSubexpressionKind::subtract, lhs, rhs));
  $$ = current_exp;
};

multiplicative_expr : cast_expr | multiplicative_expr '*' cast_expr {
  SgExpression *lhs = (SgExpression *)($1);
  SgExpression *rhs = (SgExpression *)($3);
  current_exp = SageBuilder::buildMultiplyOp(
      lhs, rhs,
      consumeOpenMPExactBinarySubexpressionType(
          OmpExactSubexpressionKind::multiply, lhs, rhs));
  $$ = current_exp;
}
| multiplicative_expr '/' cast_expr {
  SgExpression *lhs = (SgExpression *)($1);
  SgExpression *rhs = (SgExpression *)($3);
  current_exp = SageBuilder::buildDivideOp(
      lhs, rhs,
      consumeOpenMPExactBinarySubexpressionType(
          OmpExactSubexpressionKind::divide, lhs, rhs));
  $$ = current_exp;
}
| multiplicative_expr '%' cast_expr {
  current_exp = SageBuilder::buildModOp(
      (SgExpression *)($1), (SgExpression *)($3),
      deriveOpenMPCxxIntegerBinaryResultType(
          OmpExactSubexpressionKind::modulo, (SgExpression *)($1),
          (SgExpression *)($3), false));
  $$ = current_exp;
};

primary_expr : ICONSTANT {
  SgValueExp *int_val = buildOpenMPIntegerLiteral((const char *)($1), false);
  free(const_cast<char *>($1));
  current_exp = int_val;
  $$ = current_exp;
}
| HEXCONSTANT {
  SgValueExp *int_val = buildOpenMPIntegerLiteral((const char *)($1), true);
  free(const_cast<char *>($1));
  current_exp = int_val;
  $$ = current_exp;
}
| STRING_LITERAL {
  const std::string source_literal((const char *)($1));
  free(const_cast<char *>($1));
  if (source_literal.size() < 2 ||
      source_literal.front() != source_literal.back() ||
      (source_literal.front() != '\'' && source_literal.front() != '"')) {
    std::cerr << "REX_OMP_AST_INVARIANT[string-literal]: lexer "
                 "returned a malformed source literal\n";
    ROSE_ABORT();
  }
  SgStringVal *string_literal = SageBuilder::buildStringVal_nfi(
      source_literal.substr(1, source_literal.size() - 2));
  SgType *literal_type =
      omp_exprparser_fortran_typed_scope != NULL
          ? buildOpenMPFortranTypedScopeStringLiteralType(source_literal)
          : deriveOpenMPCxxStringLiteralType(
                source_literal.substr(1, source_literal.size() - 2));
  if (string_literal->get_literal_type() != NULL) {
    std::cerr << "REX_OMP_AST_INVARIANT[string-literal]: reconstructed "
                 "literal unexpectedly owns a preexisting semantic type\n";
    ROSE_ABORT();
  }
  string_literal->set_literal_type(literal_type);
  if (SageInterface::is_Fortran_language()) {
    string_literal->set_stringDelimiter(source_literal.front());
  } else if (source_literal.front() != '"') {
    std::cerr << "REX_OMP_AST_INVARIANT[string-literal]: C/C++ OpenMP "
                 "expression uses a character-literal delimiter\n";
    ROSE_ABORT();
  } else if (string_literal->get_stringDelimiter() != 0) {
    std::cerr << "REX_OMP_AST_INVARIANT[string-literal]: C/C++ OpenMP "
                 "expression inherited Fortran delimiter state\n";
    ROSE_ABORT();
  }
  string_literal->set_literal_spelling_form(
      SgValueExp::e_literal_source_spelled);
  SageInterface::setOneSourcePositionForTransformation(string_literal);
  if (string_literal->get_type() != literal_type) {
    std::cerr << "REX_OMP_AST_INVARIANT[string-literal]: reconstructed "
                 "literal did not retain the frontend-owned semantic type\n";
    ROSE_ABORT();
  }
  current_exp = string_literal;
  $$ = current_exp;
}
| ID_EXPRESSION {
  SgScopeStatement *scope = SageInterface::getScope(omp_directive_node);
  ROSE_ASSERT(scope != NULL);
  current_exp = buildOpenMPIdentifierExpression((const char *)($1), scope);
  if (current_exp == NULL) {
    if (SageInterface::is_Fortran_language()) {
      std::cerr << "[REX-OMP-UNRESOLVED-FORTRAN-NAME] no symbol for '"
                << (const char *)($1) << "'\n";
      ROSE_ABORT();
    } else {
      std::cerr << "[REX-OMP-UNRESOLVED-CXX-NAME] no symbol for '"
                << (const char *)($1) << "'\n";
      ROSE_ABORT();
    }
  }
  free(const_cast<char *>($1));
  $$ = current_exp;
}
| '(' expression ')' {
  SgExpression *parenthesized = isSgExpression((SgNode *)($2));
  if (parenthesized != NULL) {
    parenthesized->set_need_paren(true);
    current_exp = parenthesized;
  }
  $$ = current_exp;
};

cast_expr : unary_expr {
  current_exp = (SgExpression *)($1);
  $$ = current_exp;
}
| '(' TYPE_INT ')' cast_expr {
  current_exp = SageBuilder::buildCastExp(
      (SgExpression *)($4),
      deriveOpenMPCxxCastToIntResultType(OmpExactSubexpressionKind::cast),
      SgCastExp::e_C_style_cast);
  $$ = current_exp;
};

unary_expr : postfix_expr {
  current_exp = (SgExpression *)($1);
  $$ = current_exp;
}
| PLUSPLUS unary_expr {
  current_exp = SageBuilder::buildPlusPlusOp(
      (SgExpression *)($2),
      deriveOpenMPCxxUnaryResultType(
          OmpExactSubexpressionKind::prefix_increment,
          (SgExpression *)($2)),
      SgUnaryOp::prefix);
  $$ = current_exp;
}
| '+' unary_expr {
  current_exp = SageBuilder::buildUnaryAddOp(
      (SgExpression *)($2),
      deriveOpenMPCxxUnaryResultType(OmpExactSubexpressionKind::unary_plus,
                                     (SgExpression *)($2)));
  $$ = current_exp;
}
| '-' unary_expr {
  current_exp = SageBuilder::buildMinusOp(
      (SgExpression *)($2),
      deriveOpenMPCxxUnaryResultType(OmpExactSubexpressionKind::unary_minus,
                                     (SgExpression *)($2)));
  $$ = current_exp;
}
| '!' unary_expr {
  current_exp = SageBuilder::buildNotOp(
      (SgExpression *)($2),
      deriveOpenMPCxxUnaryResultType(OmpExactSubexpressionKind::logical_not,
                                     (SgExpression *)($2)));
  $$ = current_exp;
}
| '~' unary_expr {
  current_exp = SageBuilder::buildBitComplementOp(
      (SgExpression *)($2),
      deriveOpenMPCxxUnaryResultType(
          OmpExactSubexpressionKind::bit_complement,
          (SgExpression *)($2)));
  $$ = current_exp;
}
| '&' unary_expr {
  current_exp = SageBuilder::buildAddressOfOp(
      (SgExpression *)($2),
      deriveOpenMPCxxUnaryResultType(OmpExactSubexpressionKind::address_of,
                                     (SgExpression *)($2)));
  $$ = current_exp;
}
| STATIC_CAST '<' TYPE_INT '>' '(' expression ')' {
  current_exp = SageBuilder::buildCastExp(
      (SgExpression *)($6),
      deriveOpenMPCxxCastToIntResultType(OmpExactSubexpressionKind::cast),
      SgCastExp::e_static_cast);
  $$ = current_exp;
}
| SIZEOF unary_expr {
  current_exp = SageBuilder::buildSizeOfOp(
      (SgExpression *)($2),
      deriveOpenMPCxxSizeofResultType(
          OmpExactSubexpressionKind::sizeof_expression));
  $$ = current_exp;
}
| SIZEOF '(' TYPE_INT ')' {
  current_exp = SageBuilder::buildSizeOfOp(
      SageBuilder::buildIntType(),
      deriveOpenMPCxxSizeofResultType(OmpExactSubexpressionKind::sizeof_type));
  $$ = current_exp;
}
| '*' unary_expr {
  current_exp = SageBuilder::buildPointerDerefExp(
      (SgExpression *)($2),
      deriveOpenMPCxxDereferenceResultType((SgExpression *)($2)));
  $$ = current_exp;
}
| MINUSMINUS unary_expr {
  current_exp = SageBuilder::buildMinusMinusOp(
      (SgExpression *)($2),
      deriveOpenMPCxxUnaryResultType(
          OmpExactSubexpressionKind::prefix_decrement,
          (SgExpression *)($2)),
      SgUnaryOp::prefix);
  $$ = current_exp;
}

;
parenthesized_argument_list_opt : %empty { $$ = buildOpenMPExpressionList(); }
| parenthesized_argument_list { $$ = $1; };

parenthesized_argument_list : parenthesized_argument_item {
  SgExprListExp *args = buildOpenMPExpressionList();
  args->append_expression((SgExpression *)($1));
  $$ = args;
}
| parenthesized_argument_list ',' parenthesized_argument_item {
  SgExprListExp *args = isSgExprListExp((SgNode *)($1));
  ROSE_ASSERT(args != NULL);
  args->append_expression((SgExpression *)($3));
  $$ = args;
};

parenthesized_argument_item : assignment_expr { $$ = $1; }
| fortran_subscript_item { $$ = $1; };

fortran_subscript_item : expression ':' expression {
  $$ = buildOpenMPArraySectionSubscript((SgExpression *)($1),
                                        (SgExpression *)($3));
}
| expression ':' expression ':' expression {
  $$ = buildOpenMPArraySectionSubscript(
      (SgExpression *)($1), (SgExpression *)($3), (SgExpression *)($5));
}
| ':' expression {
  $$ = buildOpenMPArraySectionSubscript(buildOpenMPSyntacticAbsence(),
                                        (SgExpression *)($2));
}
| ':' expression ':' expression {
  $$ = buildOpenMPArraySectionSubscript(buildOpenMPSyntacticAbsence(),
                                        (SgExpression *)($2),
                                        (SgExpression *)($4));
}
| expression ':' {
  $$ = buildOpenMPArraySectionSubscript((SgExpression *)($1),
                                        buildOpenMPSyntacticAbsence());
}
| expression ':' ':' expression {
  $$ = buildOpenMPArraySectionSubscript((SgExpression *)($1),
                                        buildOpenMPSyntacticAbsence(),
                                        (SgExpression *)($4));
}
| ':' {
  $$ = buildOpenMPArraySectionSubscript(buildOpenMPSyntacticAbsence(),
                                        buildOpenMPSyntacticAbsence());
}
| ':' ':' expression {
  $$ = buildOpenMPArraySectionSubscript(buildOpenMPSyntacticAbsence(),
                                        buildOpenMPSyntacticAbsence(),
                                        (SgExpression *)($3));
};

postfix_expr : primary_expr {
  arraySection = false;
  current_exp = (SgExpression *)($1);
  $$ = current_exp;
}
| postfix_expr '(' parenthesized_argument_list_opt ')' {
  current_exp =
      buildPostfixParenExpression((SgExpression *)($1), (SgExprListExp *)($3));
  $$ = current_exp;
}
| postfix_expr '[' expression ']' {
  arraySection = false;
  current_exp = SageBuilder::buildPntrArrRefExp(
      (SgExpression *)($1), (SgExpression *)($3),
      deriveOpenMPCxxSubscriptResultType((SgExpression *)($1)));
  $$ = current_exp;
}
| postfix_expr '[' expression ':' expression ']' {
  arraySection = true; // array section expression
                       // postfix_expr should be ID_EXPRESSION
  if (array_symbol == NULL) {
    if (SgVarRefExp *vref = isSgVarRefExp((SgExpression *)($1))) {
      array_symbol = isSgVariableSymbol(vref->get_symbol());
    }
  }
  lower_exp = (SgExpression *)($3);
  length_exp = (SgExpression *)($5);
  if (array_symbol != NULL) {
    SgType *t = array_symbol->get_type();
    bool isPointer = (isSgPointerType(t) != NULL);
    bool isArray = (isSgArrayType(t) != NULL);
    if (!isPointer && !isArray) {
      std::cerr << "REX_OMP_AST_INVARIANT[array-section-base]: "
                   "expected a pointer or array type, got "
                << t->class_name() << "\n";
      ROSE_ABORT();
    }
  }
  assert(lower_exp && length_exp);
  SgSubscriptExpression *subscript =
      buildOpenMPArraySectionSubscript(lower_exp, length_exp);
  current_exp = SageBuilder::buildPntrArrRefExp(
      (SgExpression *)($1), subscript,
      deriveOpenMPArraySectionResultType((SgExpression *)($1)));
  $$ = current_exp;
}
| postfix_expr '[' expression ':' expression ':' expression ']' {
  arraySection = true; // array section expression with explicit stride
  if (array_symbol == NULL) {
    if (SgVarRefExp *vref = isSgVarRefExp((SgExpression *)($1))) {
      array_symbol = isSgVariableSymbol(vref->get_symbol());
    }
  }
  lower_exp = (SgExpression *)($3);
  length_exp = (SgExpression *)($5);
  SgExpression *stride_exp = (SgExpression *)($7);
  assert(lower_exp && length_exp && stride_exp);
  SgSubscriptExpression *subscript =
      buildOpenMPArraySectionSubscript(lower_exp, length_exp, stride_exp);
  current_exp = SageBuilder::buildPntrArrRefExp(
      (SgExpression *)($1), subscript,
      deriveOpenMPArraySectionResultType((SgExpression *)($1)));
  $$ = current_exp;
}
| postfix_expr '[' ':' expression ']' {
  arraySection = true; // array section expression with omitted lower bound
  if (array_symbol == NULL) {
    if (SgVarRefExp *vref = isSgVarRefExp((SgExpression *)($1))) {
      array_symbol = isSgVariableSymbol(vref->get_symbol());
    }
  }
  lower_exp = buildOpenMPSyntacticAbsence();
  length_exp = (SgExpression *)($4);
  if (array_symbol != NULL) {
    SgType *t = array_symbol->get_type();
    bool isPointer = (isSgPointerType(t) != NULL);
    bool isArray = (isSgArrayType(t) != NULL);
    if (!isPointer && !isArray) {
      std::cerr << "REX_OMP_AST_INVARIANT[array-section-base]: "
                   "expected a pointer or array type, got "
                << t->class_name() << "\n";
      ROSE_ABORT();
    }
  }
  assert(lower_exp && length_exp);
  SgSubscriptExpression *subscript =
      buildOpenMPArraySectionSubscript(lower_exp, length_exp);
  current_exp = SageBuilder::buildPntrArrRefExp(
      (SgExpression *)($1), subscript,
      deriveOpenMPArraySectionResultType((SgExpression *)($1)));
  $$ = current_exp;
}
| postfix_expr '[' ':' ']' {
  arraySection =
      true; // array section expression with omitted lower bound and length
  if (array_symbol == NULL) {
    if (SgVarRefExp *vref = isSgVarRefExp((SgExpression *)($1))) {
      array_symbol = isSgVariableSymbol(vref->get_symbol());
    }
  }
  lower_exp = buildOpenMPSyntacticAbsence();
  length_exp = buildOpenMPSyntacticAbsence();
  assert(lower_exp && length_exp);
  SgSubscriptExpression *subscript =
      buildOpenMPArraySectionSubscript(lower_exp, length_exp);
  current_exp = SageBuilder::buildPntrArrRefExp(
      (SgExpression *)($1), subscript,
      deriveOpenMPArraySectionResultType((SgExpression *)($1)));
  $$ = current_exp;
}
| postfix_expr '[' ':' expression ':' expression ']' {
  arraySection = true; // array section expression with omitted lower bound
  if (array_symbol == NULL) {
    if (SgVarRefExp *vref = isSgVarRefExp((SgExpression *)($1))) {
      array_symbol = isSgVariableSymbol(vref->get_symbol());
    }
  }
  lower_exp = buildOpenMPSyntacticAbsence();
  length_exp = (SgExpression *)($4);
  SgExpression *stride_exp = (SgExpression *)($6);
  assert(lower_exp && length_exp && stride_exp);
  SgSubscriptExpression *subscript =
      buildOpenMPArraySectionSubscript(lower_exp, length_exp, stride_exp);
  current_exp = SageBuilder::buildPntrArrRefExp(
      (SgExpression *)($1), subscript,
      deriveOpenMPArraySectionResultType((SgExpression *)($1)));
  $$ = current_exp;
}
| postfix_expr '[' ':' ':' expression ']' {
  arraySection = true; // array section expression with omitted lower bound and
                       // length, explicit stride
  if (array_symbol == NULL) {
    if (SgVarRefExp *vref = isSgVarRefExp((SgExpression *)($1))) {
      array_symbol = isSgVariableSymbol(vref->get_symbol());
    }
  }
  lower_exp = buildOpenMPSyntacticAbsence();
  length_exp = buildOpenMPSyntacticAbsence();
  SgExpression *stride_exp = (SgExpression *)($5);
  assert(lower_exp && length_exp && stride_exp);
  SgSubscriptExpression *subscript =
      buildOpenMPArraySectionSubscript(lower_exp, length_exp, stride_exp);
  current_exp = SageBuilder::buildPntrArrRefExp(
      (SgExpression *)($1), subscript,
      deriveOpenMPArraySectionResultType((SgExpression *)($1)));
  $$ = current_exp;
}
| postfix_expr '[' expression ':' ']' {
  arraySection = true; // array section expression with omitted length
  if (array_symbol == NULL) {
    if (SgVarRefExp *vref = isSgVarRefExp((SgExpression *)($1))) {
      array_symbol = isSgVariableSymbol(vref->get_symbol());
    }
  }
  lower_exp = (SgExpression *)($3);
  length_exp = buildOpenMPSyntacticAbsence();
  assert(lower_exp && length_exp);
  SgSubscriptExpression *subscript =
      buildOpenMPArraySectionSubscript(lower_exp, length_exp);
  current_exp = SageBuilder::buildPntrArrRefExp(
      (SgExpression *)($1), subscript,
      deriveOpenMPArraySectionResultType((SgExpression *)($1)));
  $$ = current_exp;
}
| postfix_expr '[' expression ':' ':' expression ']' {
  arraySection =
      true; // array section expression with omitted length and explicit stride
  if (array_symbol == NULL) {
    if (SgVarRefExp *vref = isSgVarRefExp((SgExpression *)($1))) {
      array_symbol = isSgVariableSymbol(vref->get_symbol());
    }
  }
  lower_exp = (SgExpression *)($3);
  length_exp = buildOpenMPSyntacticAbsence();
  SgExpression *stride_exp = (SgExpression *)($6);
  assert(lower_exp && length_exp && stride_exp);
  SgSubscriptExpression *subscript =
      buildOpenMPArraySectionSubscript(lower_exp, length_exp, stride_exp);
  current_exp = SageBuilder::buildPntrArrRefExp(
      (SgExpression *)($1), subscript,
      deriveOpenMPArraySectionResultType((SgExpression *)($1)));
  $$ = current_exp;
}
| postfix_expr '.' ID_EXPRESSION {
  SgExpression *base = (SgExpression *)($1);
  SgExpression *member =
      buildOpenMPMemberReference(base, (const char *)($3), false);
  free(const_cast<char *>($3));
  current_exp = SageBuilder::buildDotExp(
      base, member,
      consumeOpenACCExactMemberReferenceType(
          OmpExactSubexpressionKind::member_dot, base, member));
  $$ = current_exp;
}
| postfix_expr PTR_TO ID_EXPRESSION {
  SgExpression *base = (SgExpression *)($1);
  SgExpression *member =
      buildOpenMPMemberReference(base, (const char *)($3), true);
  free(const_cast<char *>($3));
  current_exp = SageBuilder::buildArrowExp(
      base, member,
      consumeOpenACCExactMemberReferenceType(
          OmpExactSubexpressionKind::member_arrow, base, member));
  $$ = current_exp;
}
| postfix_expr PLUSPLUS {
  current_exp = SageBuilder::buildPlusPlusOp(
      (SgExpression *)($1),
      deriveOpenMPCxxUnaryResultType(
          OmpExactSubexpressionKind::postfix_increment,
          (SgExpression *)($1)),
      SgUnaryOp::postfix);
  $$ = current_exp;
}
| postfix_expr MINUSMINUS {
  current_exp = SageBuilder::buildMinusMinusOp(
      (SgExpression *)($1),
      deriveOpenMPCxxUnaryResultType(
          OmpExactSubexpressionKind::postfix_decrement,
          (SgExpression *)($1)),
      SgUnaryOp::postfix);
  $$ = current_exp;
};

/* ----------------------end for parsing expressions ------------------*/

/*  in C
variable-list : identifier
              | variable-list , identifier
*/

/* in C++ (we use the C++ version) */
variable_list : assignment_expr {
  if (!addOmpVariableExpr((SgExpression *)($1)))
    YYABORT;
}
| variable_list ',' assignment_expr {
  if (!addOmpVariableExpr((SgExpression *)($3)))
    YYABORT;
}
| FORTRAN_COMMON_BLOCK {
  if (!SageInterface::is_Fortran_language()) {
    std::cerr << "REX_OMP_AST_INVARIANT[common-block]: "
                 "Fortran common-block syntax in a non-Fortran directive\n";
    ROSE_ABORT();
  }
  if (!addOmpVariable((const char *)($1)))
    YYABORT;
  free(const_cast<char *>($1));
}
| variable_list ',' FORTRAN_COMMON_BLOCK {
  if (!SageInterface::is_Fortran_language()) {
    std::cerr << "REX_OMP_AST_INVARIANT[common-block]: "
                 "Fortran common-block syntax in a non-Fortran directive\n";
    ROSE_ABORT();
  }
  if (!addOmpVariable((const char *)($3)))
    YYABORT;
  free(const_cast<char *>($3));
}

%%

    int yyerror(const char *s) {
  SgLocatedNode *lnode = isSgLocatedNode(omp_directive_node);
  if (lnode == NULL || lnode->get_file_info() == NULL) {
    std::cerr << "REX_OMP_AST_INVARIANT[expression-parser-error]: parse "
                 "failure has no exact located directive owner\n";
    ROSE_ABORT();
  }
  MLOG_ERROR_C("omp_exprparser", "Error when parsing pragma at line %d: %s\n",
               lnode->get_file_info()->get_line(), orig_str ? orig_str : "");
  MLOG_ERROR_C("omp_exprparser", "%s\n", s ? s : "unknown parse error");
  ROSE_ABORT();
  return 0; // we want to the program to stop on error
}

// Preserve a common-block designator as one typed directive operand. Expanding
// /block/ to its members changes both the source surface and the OpenMP/OpenACC
// semantic entity, and a process-wide memory-pool search can bind another
// source file's block.
static void ofs_add_block_reference(const char *spelling) {
  if (spelling == NULL) {
    std::cerr << "REX_OMP_AST_INVARIANT[common-block-spelling]: null "
                 "Fortran common-block designator\n";
    ROSE_ABORT();
  }
  const std::string designator(spelling);
  if (designator.size() < 3 || designator.front() != '/' ||
      designator.back() != '/') {
    std::cerr << "REX_OMP_AST_INVARIANT[common-block-spelling]: malformed "
                 "Fortran common-block designator '"
              << designator << "'\n";
    ROSE_ABORT();
  }

  const std::string innerSpelling = designator.substr(1, designator.size() - 2);
  SgCommonBlockObject *common_block = NULL;
  SgName use_name(innerSpelling);
  if (omp_exprparser_fortran_typed_scope != NULL) {
    common_block = SageInterface::lookupFortranCommonBlockObject(
        use_name, omp_exprparser_fortran_typed_scope);
  } else {
    const auto &binding =
        consumeFortranOpenMPExactSemanticBinding(innerSpelling);
    if (binding.kind() !=
            OmpFortranExactSemanticBindings::BindingKind::common_block ||
        binding.semanticNode() == NULL || binding.symbol() != NULL) {
      std::cerr << "REX_OMP_AST_INVARIANT[fortran-exact-common-block]: /"
                << innerSpelling
                << "/ has no exact producer-owned Sage common-block "
                   "identity\n";
      ROSE_ABORT();
    }
    common_block = isSgCommonBlockObject(binding.semanticNode());
    use_name = SgName(binding.sourceSpelling());
    if (common_block == NULL) {
      std::cerr << "REX_OMP_AST_INVARIANT[fortran-exact-common-block]: /"
                << innerSpelling << "/ changed exact Sage node kind\n";
      ROSE_ABORT();
    }
  }
  SgFortranCommonBlockRefExp *reference =
      new SgFortranCommonBlockRefExp(use_name, common_block);
  SageInterface::setOneSourcePositionForTransformation(reference);
  SageInterface::validateFortranCommonBlockRef(reference);
  OmpSupport::openMPExpressionVariables().push_back(reference);
}

static bool addOmpVariable(const char *var) {

  if (var == NULL || var[0] == '\0') {
    std::cerr << "REX_OMP_AST_INVARIANT[variable-spelling]: empty "
                 "directive variable spelling\n";
    ROSE_ABORT();
  }

  // if the leading symbol is '/', it is a block name in Fortran
  if (var[0] == '/') {
    if (!SageInterface::is_Fortran_language()) {
      std::cerr << "REX_OMP_AST_INVARIANT[common-block]: common-block "
                   "designator reached the C/C++ expression parser\n";
      ROSE_ABORT();
    }
    ofs_add_block_reference(var);
    return true;
  }
  std::cerr << "REX_OMP_AST_INVARIANT[exact-variable]: raw identifier '" << var
            << "' bypassed the exact semantic expression binding\n";
  ROSE_ABORT();
}

static bool addOmpVariableExpr(SgExpression *expr) {
  if (expr == NULL) {
    array_symbol = NULL;
    return false;
  }

  if (SgVarRefExp *vref = isSgVarRefExp(expr)) {
    if (SgVariableSymbol *sym = isSgVariableSymbol(vref->get_symbol())) {
      if (SageInterface::is_Fortran_language()) {
        OmpSupport::openMPExpressionVariables().push_back(expr);
        array_symbol = sym;
        return true;
      }
      std::string name = sym->get_name().getString();
      SgInitializedName *declaration = sym->get_declaration();
      if (name.empty() || declaration == NULL ||
          !isKnownVariableSymbolType(sym)) {
        std::cerr << "REX_OMP_AST_INVARIANT[exact-variable]: exact "
                     "C/C++ clause binding has no typed declaration\n";
        ROSE_ABORT();
      }
      if (expr->get_parent() != NULL) {
        std::cerr << "REX_OMP_AST_INVARIANT[exact-variable]: parser "
                     "temporary already has a structural owner\n";
        ROSE_ABORT();
      }
      if (vref->get_originalExpressionTree() != NULL ||
          !vref->get_traversalSuccessorContainer().empty()) {
        std::cerr << "REX_OMP_AST_INVARIANT[exact-variable]: parser variable "
                     "temporary is not an isolated semantic leaf\n";
        ROSE_ABORT();
      }
      delete vref;
      if (SgNode::isLiveNode(expr)) {
        std::cerr << "REX_OMP_AST_INVARIANT[exact-variable]: parser "
                     "temporary remained live after exact identity "
                     "extraction\n";
        ROSE_ABORT();
      }
      OmpSupport::openMPExpressionVariables().push_back(declaration);
      array_symbol = sym;
      return true;
    }
  }

  if (SgFunctionRefExp *fref = isSgFunctionRefExp(expr)) {
    if (SgFunctionSymbol *sym = fref->get_symbol()) {
      std::string name = sym->get_name().getString();
      if (!name.empty()) {
        OmpSupport::openMPExpressionVariables().push_back(expr);
        array_symbol = NULL;
        return true;
      }
    }
  }

  OmpSupport::openMPExpressionVariables().push_back(expr);
  array_symbol = NULL;
  return true;
}

SgExpression *parseArraySectionExpression(SgNode *directive, const char *str) {
  beginOpenMPExpressionParse(directive, str);
  SgExpression *result = finishOpenMPExpressionParse(omp_exprparser_parse());
  if (result == NULL) {
    std::cerr << "REX_OMP_AST_INVARIANT[expression-parser-result]: array "
                 "section parse produced no expression\n";
    ROSE_ABORT();
  }
  return result;
}

SgExpression *parseExpression(SgNode *directive, const char *str) {
  beginOpenMPExpressionParse(directive, str);
  return finishOpenMPExpressionParse(omp_exprparser_parse());
}

void omp_exprparser_require_clean_state() {
  requireIdleOpenMPExpressionParser("session teardown");
  if (omp_exprparser_openacc_cxx_semantic_bindings != NULL ||
      omp_exprparser_exact_semantic_binding_index != 0 ||
      omp_exprparser_fortran_exact_semantic_bindings != NULL ||
      omp_exprparser_fortran_exact_semantic_binding_index != 0 ||
      omp_exprparser_exact_subexpression_types != NULL ||
      omp_exprparser_exact_subexpression_type_index != 0 ||
      omp_exprparser_fortran_typed_scope != NULL ||
      omp_exprparser_fortran_default_integer_type != NULL ||
      !omp_exprparser_fortran_typed_scope_bindings.empty() ||
      !omp_exprparser_context_variable_symbols.empty() ||
      !omp_exprparser_context_name_expressions.empty()) {
    std::cerr << "REX_OMP_AST_INVARIANT[expression-parser-lifecycle]: "
                 "OpenMP conversion session ended with borrowed parser "
                 "context\n";
    ROSE_ABORT();
  }
}
