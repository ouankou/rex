/* unparseFortran_statements.C
 *
 * Code to unparse Sage/Fortran statement nodes.
 *
 */

#include "sage3basic.h"

#include "unparser.h"

#include <cctype>
#include <limits>
#include <sstream>

// DQ (10/14/2010):  This should only be included by source files that require
// it. This fixed a reported bug which caused conflicts with configure-time
// macros (e.g. PACKAGE_BUGREPORT). Interestingly it must be at the top of the
// list of include files.
#include "rose_config.h"

using namespace std;
using namespace Rose;

#ifdef ROSE_FLANG_FRONTEND
constexpr bool flangParser{true};
constexpr bool keywordsAreUpperCase{false}; // should be a command-line option
#else
constexpr bool flangParser{false};
constexpr bool keywordsAreUpperCase{true};
#endif

bool fortranSeparateStatementOwnsArrayShape(
    const SgInitializedName *initializedName);
bool fortranSeparatePointerStatementOwnsPointerAttribute(
    const SgInitializedName *initializedName);

namespace {
struct FortranTypeSurface {
  SgType *base = nullptr;
  size_t ordinaryRank = 0;
  bool pointer = false;
};

FortranTypeSurface decomposeFortranTypeSurface(SgType *type) {
  FortranTypeSurface result;
  while (type != nullptr) {
    if (SgModifierType *modifier = isSgModifierType(type)) {
      type = modifier->get_base_type();
      continue;
    }
    if (SgPointerType *pointer = isSgPointerType(type)) {
      result.pointer = true;
      type = pointer->get_base_type();
      continue;
    }
    if (SgArrayType *array = isSgArrayType(type)) {
      if (!array->get_isCoArray()) {
        result.ordinaryRank += array->get_rank();
      }
      type = array->get_base_type();
      continue;
    }
    result.base = type;
    return result;
  }
  return result;
}

bool exactFortranRedeclarationIdentity(const SgInitializedName *source,
                                       const SgInitializedName *canonical) {
  std::unordered_set<const SgInitializedName *> visited;
  for (const SgInitializedName *current = source; current != nullptr;
       current = current->get_prev_decl_item()) {
    if (!visited.insert(current).second) {
      std::cerr << "REX_UNPARSE_INVARIANT[fortran-redeclaration-chain]: "
                   "variable="
                << source->get_name()
                << " has a cyclic initialized-name chain\n";
      ROSE_ABORT();
    }
    if (current == canonical) {
      return true;
    }
  }
  return false;
}

bool fortranSourceTypeMatchesSemanticType(
    const SgInitializedName *initializedName) {
  ASSERT_not_null(initializedName);
  SgType *source = initializedName->get_fortran_source_type();
  SgType *semantic = initializedName->get_type();
  if (source != nullptr && source == semantic) {
    SgVariableDeclaration *declaration =
        isSgVariableDeclaration(initializedName->get_parent());
    auto is_transformation_position = [](const Sg_File_Info *position,
                                         const SgNode *owner) {
      return position != nullptr && position->get_parent() == owner &&
             position->isTransformation() && !position->isCompilerGenerated() &&
             !position->isFrontendSpecific() &&
             !position->isSourcePositionUnavailableInFrontend() &&
             position->isOutputInCodeGeneration();
    };
    const bool exact_generated_source_surface =
        declaration != nullptr &&
        declaration->get_fortran_declaration_origin() ==
            SgVariableDeclaration::e_fortran_source_declaration &&
        !semantic->get_fortran_source_syntax() &&
        is_transformation_position(declaration->get_file_info(), declaration) &&
        is_transformation_position(declaration->get_startOfConstruct(),
                                   declaration) &&
        is_transformation_position(declaration->get_endOfConstruct(),
                                   declaration) &&
        is_transformation_position(initializedName->get_file_info(),
                                   initializedName) &&
        is_transformation_position(initializedName->get_startOfConstruct(),
                                   initializedName) &&
        is_transformation_position(initializedName->get_endOfConstruct(),
                                   initializedName);
    return exact_generated_source_surface ||
           SageInterface::fortranSourceTypeMatchesSemanticType(source,
                                                               semantic);
  }
  if (isSgTypeCrayPointer(source) != nullptr) {
    const FortranTypeSurface semanticSurface =
        decomposeFortranTypeSurface(semantic);
    return source->get_fortran_source_syntax() &&
           semanticSurface.base != nullptr &&
           (isSgTypeInt(semanticSurface.base) != nullptr ||
            isSgTypeSignedInt(semanticSurface.base) != nullptr) &&
           initializedName->get_cray_pointer_pointee() != nullptr;
  }

  const FortranTypeSurface sourceSurface = decomposeFortranTypeSurface(source);
  const FortranTypeSurface semanticSurface =
      decomposeFortranTypeSurface(semantic);
  const bool separateStatementOwnsShape =
      fortranSeparateStatementOwnsArrayShape(initializedName);
  const bool separateStatementOwnsPointer =
      fortranSeparatePointerStatementOwnsPointerAttribute(initializedName);
  if (const SgFunctionType *semanticProcedure =
          isSgFunctionType(semanticSurface.base)) {
    const SgFunctionType *sourceProcedure =
        isSgFunctionType(sourceSurface.base);
    const bool exactProcedureSource =
        sourceProcedure != nullptr
            ? sourceProcedure == semanticProcedure
            : (sourceSurface.base != nullptr &&
               semanticProcedure->get_return_type() != nullptr &&
               SageInterface::fortranSourceTypeMatchesSemanticType(
                   sourceSurface.base, semanticProcedure->get_return_type()));
    return sourceSurface.base != nullptr &&
           (sourceSurface.pointer == semanticSurface.pointer ||
            (separateStatementOwnsPointer && !sourceSurface.pointer &&
             semanticSurface.pointer)) &&
           sourceSurface.ordinaryRank == 0 &&
           semanticSurface.ordinaryRank == 0 && exactProcedureSource;
  }
  return sourceSurface.base != nullptr && semanticSurface.base != nullptr &&
         (sourceSurface.pointer == semanticSurface.pointer ||
          (separateStatementOwnsPointer && !sourceSurface.pointer &&
           semanticSurface.pointer)) &&
         (sourceSurface.ordinaryRank == semanticSurface.ordinaryRank ||
          (separateStatementOwnsShape && sourceSurface.ordinaryRank == 0 &&
           semanticSurface.ordinaryRank > 0)) &&
         SageInterface::fortranSourceTypeMatchesSemanticType(
             sourceSurface.base, semanticSurface.base);
}

const SgOmpClausePtrList &
requiredFortranOmpClauses(SgStatement *owner, SgOmpClauseList *clause_list) {
  if (owner == nullptr || clause_list == nullptr ||
      clause_list->get_parent() != owner) {
    fprintf(stderr,
            "REX_UNPARSE_INVARIANT[openmp-clause-list]: Fortran statement "
            "type=%s has no exact clause-list owner\n",
            owner != nullptr ? owner->class_name().c_str() : "<null>");
    ROSE_ABORT();
  }
  return clause_list->get_clauses();
}

bool isValidFortranIdentifier(const SgName &name) {
  const std::string text = name.getString();
  if (text.empty() || !std::isalpha(static_cast<unsigned char>(text.front()))) {
    return false;
  }
  return std::all_of(text.begin() + 1, text.end(), [](unsigned char ch) {
    return std::isalnum(ch) || ch == '_';
  });
}

bool isValidFortranSubmoduleParent(const SgName &name) {
  const std::string text = name.getString();
  const size_t separator = text.find(':');
  if (separator == std::string::npos) {
    return isValidFortranIdentifier(name);
  }
  if (separator == 0 || separator + 1 >= text.size() ||
      text.find(':', separator + 1) != std::string::npos) {
    return false;
  }
  return isValidFortranIdentifier(SgName(text.substr(0, separator))) &&
         isValidFortranIdentifier(SgName(text.substr(separator + 1)));
}

void requireFortranSourceDeclarationSurface(
    SgVariableDeclaration *declaration) {
  ASSERT_not_null(declaration);
  switch (declaration->get_fortran_declaration_origin()) {
  case SgVariableDeclaration::e_fortran_source_declaration:
    for (SgInitializedName *name : declaration->get_variables()) {
      if (name == nullptr || name->get_type() == nullptr ||
          name->get_fortran_source_type() == nullptr ||
          !fortranSourceTypeMatchesSemanticType(name)) {
        SgType *semanticType = name != nullptr ? name->get_type() : nullptr;
        SgType *sourceType =
            name != nullptr ? name->get_fortran_source_type() : nullptr;
        const FortranTypeSurface semanticSurface =
            decomposeFortranTypeSurface(semanticType);
        const FortranTypeSurface sourceSurface =
            decomposeFortranTypeSurface(sourceType);
        std::cerr << "REX_UNPARSE_INVARIANT[fortran-source-type]: source "
                     "declaration has a missing or contradictory "
                     "semantic/source type pair"
                  << std::endl;
        std::cerr << "REX_UNPARSE_DETAIL[fortran-source-type]: declaration="
                  << declaration << " name=" << name << " ('"
                  << (name != nullptr ? name->get_name().getString()
                                      : std::string("<null>"))
                  << "') semantic=" << semanticType << "/"
                  << (semanticType != nullptr ? semanticType->class_name()
                                              : std::string("<null>"))
                  << " source=" << sourceType << "/"
                  << (sourceType != nullptr ? sourceType->class_name()
                                            : std::string("<null>"))
                  << " semantic-base=" << semanticSurface.base << "/"
                  << (semanticSurface.base != nullptr
                          ? semanticSurface.base->class_name()
                          : std::string("<null>"))
                  << " source-base=" << sourceSurface.base << "/"
                  << (sourceSurface.base != nullptr
                          ? sourceSurface.base->class_name()
                          : std::string("<null>"))
                  << " semantic-rank=" << semanticSurface.ordinaryRank
                  << " source-rank=" << sourceSurface.ordinaryRank
                  << " semantic-pointer=" << semanticSurface.pointer
                  << " source-pointer=" << sourceSurface.pointer << std::endl;
        ROSE_ABORT();
      }
    }
    return;
  case SgVariableDeclaration::e_fortran_semantic_only_declaration:
    std::cerr << "REX_UNPARSE_INVARIANT[fortran-declaration-origin]: "
                 "semantic-only declaration reached a source-emission list"
              << std::endl;
    ROSE_ABORT();
  case SgVariableDeclaration::e_fortran_pending_source_declaration:
    std::cerr << "REX_UNPARSE_INVARIANT[fortran-declaration-origin]: pending "
                 "source declaration reached the unparser"
              << std::endl;
    ROSE_ABORT();
  default:
    std::cerr << "REX_UNPARSE_INVARIANT[fortran-declaration-origin]: invalid "
                 "value="
              << static_cast<int>(declaration->get_fortran_declaration_origin())
              << std::endl;
    ROSE_ABORT();
  }
}

bool isFixedFortranOutput(const Unparser *unparser) {
  ASSERT_not_null(unparser);
  ASSERT_not_null(unparser->currentFile);
  switch (unparser->currentFile->get_outputFormat()) {
  case SgFile::e_fixed_form_output_format:
    return true;
  case SgFile::e_free_form_output_format:
    return false;
  default:
    std::cerr << "Error: Fortran output format must be resolved before "
                 "unparsing"
              << std::endl;
    ROSE_ABORT();
  }
}

bool hasExplicitProgramStatement(const SgProgramHeaderStatement *program) {
  ASSERT_not_null(program);
  switch (program->get_program_statement_kind()) {
  case SgProgramHeaderStatement::e_explicit_program_statement:
    return true;
  case SgProgramHeaderStatement::e_implicit_program_statement:
    return false;
  default:
    std::cerr << "Error: Fortran main program has no PROGRAM-statement "
                 "metadata"
              << std::endl;
    ROSE_ABORT();
  }
}

bool blockDataHasName(const SgProcedureHeaderStatement *procedure) {
  ASSERT_not_null(procedure);
  switch (procedure->get_block_data_name_kind()) {
  case SgProcedureHeaderStatement::e_named_block_data:
    return true;
  case SgProcedureHeaderStatement::e_unnamed_block_data:
    return false;
  default:
    std::cerr << "Error: BLOCK DATA has no source-name metadata" << std::endl;
    ROSE_ABORT();
  }
}
} // namespace

// Unparse language keywords
void FortranCodeGeneration_locatedNode::curprint_keyword(
    const std::string &keyword, SgUnparse_Info &info) {
  if (keywordsAreUpperCase) {
    // The default construction used below
    curprint(keyword);
  } else {
    std::string lowered{keyword};
    transform(lowered.begin(), lowered.end(), lowered.begin(), ::tolower);
    curprint(lowered);
  }
}

inline bool namesMatch(const string &x, const string &y) {
  // This function checks a case insensitive match of x against y.
  // This is required because Fortran is case insensitive.

  size_t x_length = x.length();
  size_t y_length = y.length();
  ROSE_ASSERT(x_length > 0 && y_length > 0);
  return (x_length == y_length)
             ? strncasecmp(x.c_str(), y.c_str(), x_length) == 0
             : false;
}

FortranCodeGeneration_locatedNode::FortranCodeGeneration_locatedNode(
    Unparser *unp, std::string fname)
    : UnparseLanguageIndependentConstructs(unp, fname) {
  // Nothing to do here!
}

FortranCodeGeneration_locatedNode::~FortranCodeGeneration_locatedNode() {
  // Nothing to do here!
}

void FortranCodeGeneration_locatedNode::unparseStatementNumbersSupport(
    SgLabelRefExp *numeric_label_exp, SgUnparse_Info &info) {
  // This is a supporting function for the unparseStatementNumbers, but can be
  // called directly for statments in the IR that can have botha starting yntax
  // and an ending syntax, both of which can be labeled. See test2007_01.f90 for
  // an example of the SgProgramHeaderStatement used this way.

  // In fixed format all labels must appear within columns 1-5 (where column 1
  // is the first column) and the 6th column is for the line continuation
  // character (any character, I think).
  const int NumericLabelIndentation = 6;

  if (info.SkipFormatting()) {
    return;
  }

  const bool fixedFormat = isFixedFortranOutput(unp);

  if (numeric_label_exp != nullptr) {
    SgLabelSymbol *numeric_label_symbol = numeric_label_exp->get_symbol();
    int numeric_label = numeric_label_symbol->get_numeric_label_value();

    ASSERT_require(numeric_label >= -1);

    // DQ (12/24/2007): I think that this value is an error in all versions of
    // Fortran
    ROSE_ASSERT(numeric_label != 0);

    // If it is greater than zero then output the value converted to a string.
    if (numeric_label >= 0) {
      // A label exists in the source code
      string numeric_label_string =
          StringUtility::numberToString(numeric_label);

      // append an extra blank to separate the lable from other code (if
      // fixedFormat == true then this puts a blank into column 6 as required
      // for this to be a code statement).
      numeric_label_string += " ";

      if (fixedFormat) {
        // Now indent the statement so that it will appear uniform (just for
        // fun!)
        int spacing = numeric_label_string.size();
        while (spacing < NumericLabelIndentation) {
          // prepend the extra blanks to right justify the numeric labels
          // (we have to fill the space anyway and this makes them look nice).
          numeric_label_string = " " + numeric_label_string;
          spacing++;
        }
      }

      curprint(numeric_label_string);
    } else {
      if (fixedFormat) {
        // if fixed format then output 6 blanks
        curprint("      ");
      }
    }
  } else {
    if (fixedFormat) {
      // if fixed format then output 6 blanks
      curprint("      ");
    }
  }
}

void FortranCodeGeneration_locatedNode::unparseStatementNumbers(
    SgStatement *stmt, SgUnparse_Info &info) {
  // This is a virtual function (called by the
  // UnparseLanguageIndependentConstructs::unparseStatement() member function).

  // DQ (11/29/2008): If this is a CPP directive then don't output statement
  // number or the white space for then in fixed format mode.
  if (isSgC_PreprocessorDirectiveStatement(stmt) != nullptr) {
    return;
  }
  if (flangParser && isSgLabelStatement(stmt) != nullptr) {
    // Label statements handle their own fixed/free-form alignment.
    return;
  }

  // This fixes a formatting problem, an aspect fo which was reported by Liao
  // 12/28/2007).
  if (isSgGlobal(stmt) != nullptr || isSgBasicBlock(stmt) != nullptr) {
    // Skip any formatting since these don't result in statements that are
    // output!
  } else {
    SgProgramHeaderStatement *program_header = isSgProgramHeaderStatement(stmt);
    if (program_header != nullptr) {
      if (hasExplicitProgramStatement(program_header)) {
        // If this is a program name that will be output then format the start
        // of the output (in case there is a label or this is fixed format).
        unparseStatementNumbersSupport(stmt->get_numeric_label(), info);
      }
    } else {
      // This is a Fortran specific case (different from use of SgLabelStatement
      // in C/C++).
      unparseStatementNumbersSupport(stmt->get_numeric_label(), info);
    }
  }
}

void FortranCodeGeneration_locatedNode::unparseLanguageSpecificStatement(
    SgStatement *stmt, SgUnparse_Info &info) {
  // This function unparses the language specific parse not handled by the base
  // class unparseStatement() member function
  ASSERT_not_null(stmt);

  switch (stmt->variantT()) {
    // program units
    // case V_SgModuleStatement:            unparseModuleStmt(stmt, info);
    // break;
  case V_SgProgramHeaderStatement:
    unparseProgHdrStmt(stmt, info);
    break;
  case V_SgProcedureHeaderStatement:
    unparseProcHdrStmt(stmt, info);
    break;

    // declarations
  case V_SgInterfaceStatement:
    unparseInterfaceStmt(stmt, info);
    break;
  case V_SgCommonBlock:
    unparseCommonBlock(stmt, info);
    break;
  case V_SgVariableDeclaration:
    unparseVarDeclStmt(stmt, info);
    break;
  case V_SgVariableDefinition:
    unparseVarDefnStmt(stmt, info);
    break;
  case V_SgEnumDeclaration:
    unparseEnumDeclStmt(stmt, info);
    break;
  case V_SgParameterStatement:
    unparseParamDeclStmt(stmt, info);
    break;
  case V_SgUseStatement:
    unparseUseStmt(stmt, info);
    break;

  case V_SgDerivedTypeStatement:
    unparseClassDeclStmt_derivedType(stmt, info);
    break;
  case V_SgModuleStatement:
    unparseClassDeclStmt_module(stmt, info);
    break;

  case V_SgClassDefinition:
    unparseClassDefnStmt(stmt, info);
    break;

    // executable statements, control flow
  case V_SgBasicBlock:
    unparseBasicBlockStmt(stmt, info);
    break;
  case V_SgIfStmt:
    unparseIfStmt(stmt, info);
    break;
  case V_SgFortranDo:
    unparseDoStmt(stmt, info);
    break;
  case V_SgSwitchStatement:
    unparseSwitchStmt(stmt, info);
    break;
  case V_SgCaseOptionStmt:
    unparseCaseStmt(stmt, info);
    break;
  case V_SgDefaultOptionStmt:
    unparseDefaultStmt(stmt, info);
    break;
  case V_SgProcessControlStatement:
    unparseProcessControlStmt(stmt, info);
    break;

    // These are derived from SgIOStatement
  case V_SgPrintStatement:
    unparsePrintStatement(stmt, info);
    break;
  case V_SgReadStatement:
    unparseReadStatement(stmt, info);
    break;
  case V_SgWriteStatement:
    unparseWriteStatement(stmt, info);
    break;
  case V_SgOpenStatement:
    unparseOpenStatement(stmt, info);
    break;
  case V_SgCloseStatement:
    unparseCloseStatement(stmt, info);
    break;
  case V_SgInquireStatement:
    unparseInquireStatement(stmt, info);
    break;
  case V_SgFlushStatement:
    unparseFlushStatement(stmt, info);
    break;
  case V_SgRewindStatement:
    unparseRewindStatement(stmt, info);
    break;
  case V_SgBackspaceStatement:
    unparseBackspaceStatement(stmt, info);
    break;
  case V_SgEndfileStatement:
    unparseEndfileStatement(stmt, info);
    break;
  case V_SgWaitStatement:
    unparseWaitStatement(stmt, info);
    break;

    // These are derived from SgImageControlStatement
  case V_SgSyncAllStatement:
    unparseSyncAllStatement(stmt, info);
    break;
  case V_SgSyncImagesStatement:
    unparseSyncImagesStatement(stmt, info);
    break;
  case V_SgSyncMemoryStatement:
    unparseSyncMemoryStatement(stmt, info);
    break;
  case V_SgSyncTeamStatement:
    unparseSyncTeamStatement(stmt, info);
    break;
  case V_SgLockStatement:
    unparseLockStatement(stmt, info);
    break;
  case V_SgUnlockStatement:
    unparseUnlockStatement(stmt, info);
    break;

  case V_SgAssociateStatement:
    unparseAssociateStatement(stmt, info);
    break;

  case V_SgFunctionDefinition:
    unparseFuncDefnStmt(stmt, info);
    break;
  case V_SgExprStatement:
    unparseExprStmt(stmt, info);
    break;

  case V_SgImplicitStatement:
    unparseImplicitStmt(stmt, info);
    break;
  case V_SgBlockDataStatement:
    unparseBlockDataStmt(stmt, info);
    break;
  case V_SgStatementFunctionStatement:
    unparseStatementFunctionStmt(stmt, info);
    break;
  case V_SgWhereStatement:
    unparseWhereStmt(stmt, info);
    break;
  case V_SgElseWhereStatement:
    unparseElseWhereStmt(stmt, info);
    break;
  case V_SgNullifyStatement:
    unparseNullifyStmt(stmt, info);
    break;
  case V_SgEquivalenceStatement:
    unparseEquivalenceStmt(stmt, info);
    break;
  case V_SgArithmeticIfStatement:
    unparseArithmeticIfStmt(stmt, info);
    break;
  case V_SgAssignStatement:
    unparseAssignStmt(stmt, info);
    break;
  case V_SgComputedGotoStatement:
    unparseComputedGotoStmt(stmt, info);
    break;
  case V_SgAssignedGotoStatement:
    unparseAssignedGotoStmt(stmt, info);
    break;

    // DQ (11/16/2007): This is unparsed as a CONTINUE statement
  case V_SgLabelStatement:
    unparseLabelStmt(isSgLabelStatement(stmt), info);
    break;

    // DQ (11/16/2007): This is a "DO WHILE" statement
  case V_SgWhileStmt:
    unparseWhileStmt(stmt, info);
    break;

    // DQ (11/17/2007): This is unparsed as a Fortran EXIT statement
  case V_SgBreakStmt:
    unparseBreakStmt(stmt, info);
    break;

    // This is unparsed as a Fortran CYCLE statement
  case V_SgContinueStmt:
    unparseContinueStmt(isSgContinueStmt(stmt), info);
    break;
  case V_SgFortranContinueStmt:
    unparseFortranContinueStmt(isSgFortranContinueStmt(stmt), info);
    break;

  case V_SgAttributeSpecificationStatement:
    unparseAttributeSpecificationStatement(stmt, info);
    break;
  case V_SgNamelistStatement:
    unparseNamelistStatement(stmt, info);
    break;
  case V_SgReturnStmt:
    unparseReturnStmt(stmt, info);
    break;
  case V_SgImportStatement:
    unparseImportStatement(stmt, info);
    break;
  case V_SgFormatStatement:
    unparseFormatStatement(stmt, info);
    break;
  case V_SgGotoStatement:
    unparseGotoStmt(isSgGotoStatement(stmt), info);
    break;

  case V_SgForAllStatement:
    unparseForAllStatement(stmt, info);
    break;

  case V_SgContainsStatement:
    unparseContainsStatement(stmt, info);
    break;
  case V_SgEntryStatement:
    unparseEntryStatement(stmt, info);
    break;
  case V_SgFortranIncludeLine:
    unparseFortranIncludeLine(stmt, info);
    break;
  case V_SgAllocateStatement:
    unparseAllocateStatement(stmt, info);
    break;
  case V_SgDeallocateStatement:
    unparseDeallocateStatement(stmt, info);
    break;

  case V_SgCAFWithTeamStatement:
    unparseWithTeamStatement(stmt, info);
    break;

    // Language independent code generation (placed in base class)
    // scope
    // case V_SgGlobal:                     unparseGlobalStmt(stmt, info);
    // break; case V_SgScopeStatement:             unparseScopeStmt(stmt, info);
    // break; case V_SgWhileStmt:                  unparseWhileStmt(stmt, info);
    // break; executable statements, other case V_SgExprStatement:
    // unparseExprStmt(stmt, info); break;
    //  Liao 10/18/2010, I turn on the pragma unparsing here to help debugging
    //  OpenMP programs , where OpenMP directive comments are used to generate
    //  C/C++-like pragmas internally. Those pragmas later are used to reuse
    //  large portion of OpenMP AST construction of C/C++
    // pragmas
  case V_SgPragmaDeclaration:
    unparsePragmaDeclStmt(stmt, info);
    break;
  // Liao 10/21/2010, Fortran-only OpenMP handling
  case V_SgOmpDoStatement:
    unparseOmpDoStatement(stmt, info);
    break;
  case V_SgFunctionParameterList:
    // Parameter lists are unparsed as part of the procedure header.
    break;
  default: {
    printf(
        "FortranCodeGeneration_locatedNode::unparseLanguageSpecificStatement: "
        "Error: No unparse function for %s (variant: %d)\n",
        stmt->sage_class_name(), stmt->variantT());
    ROSE_ABORT();
  }
  }
}

void FortranCodeGeneration_locatedNode::unparseFortranIncludeLine(
    SgStatement *stmt, SgUnparse_Info &info) {
  // This is support for the language specific include mechanism.
  if (info.outputFortranModFile()) // rmod file expands the include file but
                                   // does not contain the include statement
    return;
  SgFortranIncludeLine *includeLine = isSgFortranIncludeLine(stmt);
  ASSERT_not_null(includeLine);

  const string includeFileName = includeLine->get_filename();
  if (includeFileName.empty()) {
    std::cerr << "Error: Fortran INCLUDE statement has an empty filename"
              << std::endl;
    ROSE_ABORT();
  }

  curprint("include ");

  curprint("\"");
  curprint(includeFileName);
  curprint("\"");

  unp->cur.insert_newline(1);
}

void FortranCodeGeneration_locatedNode::unparseEntryStatement(
    SgStatement *stmt, SgUnparse_Info &info) {
  // This is much like a function declaration inside of an existing function

  SgEntryStatement *entryStatement = isSgEntryStatement(stmt);
  ASSERT_not_null(entryStatement);
  if (entryStatement->get_name().is_null()) {
    std::cerr << "Error: Fortran ENTRY statement has no name" << std::endl;
    ROSE_ABORT();
  }

  curprint("entry ");
  curprint(entryStatement->get_name());

  curprint("(");
  unparseFunctionArgs(entryStatement, info);
  curprint(")");

  // Unparse the result(<name>) suffix if present
  if (entryStatement->get_result_name() != nullptr) {
    curprint(" result(");
    curprint(entryStatement->get_result_name()->get_name());
    curprint(")");
  }

  unp->cur.insert_newline(1);
}

void FortranCodeGeneration_locatedNode::unparseContainsStatement(
    SgStatement *, SgUnparse_Info &) {
  curprint("CONTAINS");
  unp->cur.insert_newline(1);
}

void FortranCodeGeneration_locatedNode::unparseNamelistStatement(
    SgStatement *stmt, SgUnparse_Info &) {
  SgNamelistStatement *namelistStatement = isSgNamelistStatement(stmt);
  ASSERT_not_null(namelistStatement);

  SgNameGroupPtrList &groupList = namelistStatement->get_group_list();
  if (groupList.empty()) {
    std::cerr << "Error: Fortran NAMELIST statement has no groups" << std::endl;
    ROSE_ABORT();
  }
  for (SgNameGroup *nameGroup : groupList) {
    if (nameGroup == nullptr || nameGroup->get_group_name().empty() ||
        nameGroup->get_name_list().empty()) {
      std::cerr << "Error: malformed group in Fortran NAMELIST statement"
                << std::endl;
      ROSE_ABORT();
    }
  }

  curprint("namelist ");

  SgNameGroupPtrList::iterator i = groupList.begin();
  while (i != groupList.end()) {
    SgNameGroup *nameGroup = *i;
    curprint("/" + nameGroup->get_group_name() + "/ ");
    SgStringList &nameList = nameGroup->get_name_list();
    SgStringList::iterator j = nameList.begin();
    while (j != nameList.end()) {
      curprint(*j);
      j++;
      if (j != nameList.end()) {
        curprint(",");
      }
    }

    i++;

    // Put a little space before the next group name (it there are multiple
    // groups specified)
    if (i != groupList.end()) {
      curprint(" ");
    }
  }

  unp->cur.insert_newline(1);
}

void FortranCodeGeneration_locatedNode::unparseFormatItemList(
    SgFormatItemList *formatItemList, SgUnparse_Info &info) {
  SgFormatItemPtrList &formatList = formatItemList->get_format_item_list();
  SgFormatItemPtrList::iterator i = formatList.begin();
  while (i != formatList.end()) {
    SgFormatItem *formatItem = *i;
    ASSERT_not_null(formatItem);

    // The default value is "-1" so zero should be an invalid value
    int repeat_specification = formatItem->get_repeat_specification();
    ROSE_ASSERT(repeat_specification != 0);

    // Valid values are > 0
    if (repeat_specification > 0) {
      string stringValue = StringUtility::numberToString(repeat_specification);
      curprint(stringValue);
      curprint(" ");
    }

    if (formatItem->get_data() != nullptr) {
      SgStringVal *stringValue = isSgStringVal(formatItem->get_data());
      ASSERT_not_null(stringValue);

      string str;
      const bool usesSingleQuotes = stringValue->get_usesSingleQuotes();
      const bool usesDoubleQuotes = stringValue->get_usesDoubleQuotes();
      const char delimiter = stringValue->get_stringDelimiter();
      const bool isHollerith = !usesSingleQuotes && !usesDoubleQuotes &&
                               (delimiter == 'H' || delimiter == 'h');
      const bool isUnquoted =
          !usesSingleQuotes && !usesDoubleQuotes && delimiter == 0;
      if ((!isUnquoted && !isHollerith && !usesSingleQuotes &&
           !usesDoubleQuotes) ||
          (usesSingleQuotes && delimiter != '\'') ||
          (usesDoubleQuotes && delimiter != '"') ||
          (usesSingleQuotes && usesDoubleQuotes)) {
        std::cerr << "REX_UNPARSE_INVARIANT[fortran-format-string]: format "
                     "descriptor has contradictory typed delimiter ownership"
                  << std::endl;
        ROSE_ABORT();
      }
      if (usesSingleQuotes) {
        unp->emitFortranCharacterLiteral(stringValue->get_value(), '\'');
      } else if (usesDoubleQuotes) {
        unp->emitFortranCharacterLiteral(stringValue->get_value(), '"');
      } else if (isHollerith) {
        str = StringUtility::numberToString(stringValue->get_value().size()) +
              delimiter + stringValue->get_value();
        curprintLiteral(str);
      } else {
        str = stringValue->get_value();
        curprintLiteral(str);
      }

    } else {
      if (formatItem->get_format_item_list() != nullptr) {
        curprint("(");
        unparseFormatItemList(formatItem->get_format_item_list(), info);
        curprint(")");
      } else {
        std::cerr << "Error: malformed SgFormatItem has neither data nor a "
                     "nested item list"
                  << std::endl;
        ROSE_ABORT();
      }
    }

    i++;

    if (i != formatList.end()) {
      curprint(",");
    }
  }
}

void FortranCodeGeneration_locatedNode::unparseFormatStatement(
    SgStatement *stmt, SgUnparse_Info &info) {
  // Note that we use a SgStringVal in the SgFormatItem to hold a string which
  // is not really interpreted as a literal in the Fortram grammar (I think).

  SgFormatStatement *formatStatement = isSgFormatStatement(stmt);
  ASSERT_not_null(formatStatement);
  ASSERT_not_null(formatStatement->get_format_item_list());
  curprint("format ( ");

  SgFormatItemList *formatItemList = formatStatement->get_format_item_list();
  unparseFormatItemList(formatItemList, info);
  curprint(" )");

  unp->cur.insert_newline(1);
}

void FortranCodeGeneration_locatedNode::unparseImportStatement(
    SgStatement *stmt, SgUnparse_Info &info) {
  SgImportStatement *importStatement = isSgImportStatement(stmt);
  ASSERT_not_null(importStatement);
  const SgStringList &importList = importStatement->get_import_list();
  for (const std::string &name : importList) {
    if (name.empty()) {
      std::cerr
          << "[REX-UNPARSER-MALFORMED-IMPORT-NAME] IMPORT name is empty\n";
      ROSE_ABORT();
    }
  }

  curprint("import ");
  if (!importList.empty()) {
    curprint(":: ");
  }

  bool needComma = false;
  for (const std::string &name : importList) {
    if (needComma) {
      curprint(", ");
    }
    curprint(name);
    needComma = true;
  }

  unp->cur.insert_newline(1);
}

namespace {
SgArrayType *findOrdinaryFortranArrayType(SgType *type) {
  while (type != nullptr) {
    if (SgModifierType *modifier = isSgModifierType(type)) {
      type = modifier->get_base_type();
      continue;
    }
    if (SgPointerType *pointer = isSgPointerType(type)) {
      type = pointer->get_base_type();
      continue;
    }
    if (SgArrayType *array = isSgArrayType(type)) {
      if (!array->get_isCoArray()) {
        return array;
      }
      type = array->get_base_type();
      continue;
    }
    return nullptr;
  }
  return nullptr;
}
} // namespace

bool fortranAttributeStatementOwnsArrayShape(
    const SgInitializedName *initializedName) {
  ASSERT_not_null(initializedName);
  SgAttributeSpecificationStatement *dimension =
      isSgAttributeSpecificationStatement(
          initializedName->get_fortran_separate_shape_declaration());
  if (dimension == nullptr) {
    return false;
  }
  const auto kind = dimension->get_attribute_kind();
  if ((kind != SgAttributeSpecificationStatement::e_dimensionStatement &&
       kind != SgAttributeSpecificationStatement::e_allocatableStatement &&
       kind != SgAttributeSpecificationStatement::e_pointerStatement) ||
      findOrdinaryFortranArrayType(initializedName->get_type()) == nullptr) {
    std::cerr << "REX_UNPARSE_INVARIANT[fortran-attribute-shape-source]: "
                 "variable="
              << initializedName->get_name()
              << " has an attribute shape owner but no exact ordinary array "
                 "type and supported attribute statement\n";
    ROSE_ABORT();
  }

  SgExprListExp *parameters = dimension->get_parameter_list();
  if (parameters == nullptr || parameters->get_parent() != dimension ||
      parameters->get_expressions().empty()) {
    std::cerr << "REX_UNPARSE_INVARIANT[fortran-attribute-shape-source]: "
                 "variable="
              << initializedName->get_name()
              << " points to an attribute statement without a nonempty owned "
                 "parameter list\n";
    ROSE_ABORT();
  }

  size_t matchingItems = 0;
  for (SgExpression *item : parameters->get_expressions()) {
    SgPntrArrRefExp *arrayReference = isSgPntrArrRefExp(item);
    if (arrayReference == nullptr) {
      SgVarRefExp *scalarReference = isSgVarRefExp(item);
      SgVariableSymbol *scalarSymbol =
          scalarReference != nullptr ? scalarReference->get_symbol() : nullptr;
      SgInitializedName *scalarDeclaration =
          scalarSymbol != nullptr ? scalarSymbol->get_declaration() : nullptr;
      const bool scalarOwnedByAttribute =
          scalarDeclaration != nullptr &&
          scalarDeclaration->get_fortran_separate_shape_declaration() !=
              dimension &&
          (kind == SgAttributeSpecificationStatement::e_allocatableStatement ||
           (kind == SgAttributeSpecificationStatement::e_pointerStatement &&
            scalarDeclaration->get_fortran_separate_pointer_declaration() ==
                dimension));
      if (!scalarOwnedByAttribute || item == nullptr ||
          item->get_parent() != parameters || scalarReference == nullptr ||
          scalarSymbol == nullptr || scalarDeclaration == nullptr ||
          scalarDeclaration->get_scope() != scalarSymbol->get_scope()) {
        std::cerr << "REX_UNPARSE_INVARIANT[fortran-attribute-shape-source]: "
                     "attribute statement contains a malformed typed scalar "
                     "declarator\n";
        ROSE_ABORT();
      }
      continue;
    }
    SgVarRefExp *variableReference =
        isSgVarRefExp(arrayReference->get_lhs_operand());
    SgVariableSymbol *symbol = variableReference != nullptr
                                   ? variableReference->get_symbol()
                                   : nullptr;
    SgExprListExp *shape = isSgExprListExp(arrayReference->get_rhs_operand());
    if (arrayReference->get_parent() != parameters ||
        variableReference == nullptr || symbol == nullptr || shape == nullptr ||
        variableReference->get_parent() != arrayReference ||
        shape->get_parent() != arrayReference ||
        shape->get_expressions().empty()) {
      std::cerr << "REX_UNPARSE_INVARIANT[fortran-attribute-shape-source]: "
                   "attribute statement contains a malformed typed array "
                   "declarator\n";
      ROSE_ABORT();
    }
    if (symbol->get_declaration() == initializedName) {
      ++matchingItems;
    }
  }
  if (matchingItems != 1) {
    std::cerr << "REX_UNPARSE_INVARIANT[fortran-attribute-shape-source]: "
                 "variable="
              << initializedName->get_name()
              << " is not owned by exactly one item in its linked attribute "
                 "statement\n";
    ROSE_ABORT();
  }
  return true;
}

bool fortranSeparatePointerStatementOwnsPointerAttribute(
    const SgInitializedName *initializedName) {
  ASSERT_not_null(initializedName);
  SgStatement *owner =
      initializedName->get_fortran_separate_pointer_declaration();
  if (owner == nullptr) {
    return false;
  }
  SgAttributeSpecificationStatement *pointerStatement =
      isSgAttributeSpecificationStatement(owner);
  SgExprListExp *parameters = pointerStatement != nullptr
                                  ? pointerStatement->get_parameter_list()
                                  : nullptr;
  const FortranTypeSurface source =
      decomposeFortranTypeSurface(initializedName->get_fortran_source_type());
  const FortranTypeSurface semantic =
      decomposeFortranTypeSurface(initializedName->get_type());
  SgVariableDeclaration *variableDeclaration =
      isSgVariableDeclaration(initializedName->get_parent());
  const bool semanticOnlySource =
      variableDeclaration != nullptr &&
      variableDeclaration->get_fortran_declaration_origin() ==
          SgVariableDeclaration::e_fortran_semantic_only_declaration &&
      initializedName->get_fortran_source_type() == nullptr;
  if (pointerStatement == nullptr ||
      pointerStatement->get_attribute_kind() !=
          SgAttributeSpecificationStatement::e_pointerStatement ||
      pointerStatement->get_scope() == nullptr ||
      pointerStatement->get_scope() != initializedName->get_scope() ||
      parameters == nullptr || parameters->get_parent() != pointerStatement ||
      parameters->get_expressions().empty() ||
      (!semanticOnlySource && source.base == nullptr) ||
      semantic.base == nullptr || source.pointer || !semantic.pointer) {
    std::cerr << "REX_UNPARSE_INVARIANT[fortran-pointer-source]: variable="
              << initializedName->get_name()
              << " has no exact separate POINTER statement and semantic "
                 "pointer wrapper\n";
    ROSE_ABORT();
  }

  size_t matchingItems = 0;
  for (SgExpression *item : parameters->get_expressions()) {
    SgPntrArrRefExp *arrayReference = isSgPntrArrRefExp(item);
    SgVarRefExp *variableReference =
        arrayReference != nullptr
            ? isSgVarRefExp(arrayReference->get_lhs_operand())
            : isSgVarRefExp(item);
    SgVariableSymbol *symbol = variableReference != nullptr
                                   ? variableReference->get_symbol()
                                   : nullptr;
    SgInitializedName *declaration =
        symbol != nullptr ? symbol->get_declaration() : nullptr;
    if (item == nullptr || item->get_parent() != parameters ||
        variableReference == nullptr || symbol == nullptr ||
        declaration == nullptr ||
        declaration->get_fortran_separate_pointer_declaration() !=
            pointerStatement) {
      std::cerr << "REX_UNPARSE_INVARIANT[fortran-pointer-source]: POINTER "
                   "statement contains a malformed or unowned typed entity\n";
      ROSE_ABORT();
    }
    if (arrayReference != nullptr) {
      SgExprListExp *shape = isSgExprListExp(arrayReference->get_rhs_operand());
      if (variableReference->get_parent() != arrayReference ||
          shape == nullptr || shape->get_parent() != arrayReference ||
          shape->get_expressions().empty() ||
          declaration->get_fortran_separate_shape_declaration() !=
              pointerStatement ||
          !fortranAttributeStatementOwnsArrayShape(declaration)) {
        std::cerr
            << "REX_UNPARSE_INVARIANT[fortran-pointer-source]: shaped POINTER "
               "entity has no exact deferred-shape source ownership\n";
        ROSE_ABORT();
      }
    }
    if (exactFortranRedeclarationIdentity(initializedName, declaration)) {
      ++matchingItems;
    }
  }
  if (matchingItems != 1) {
    std::cerr << "REX_UNPARSE_INVARIANT[fortran-pointer-source]: variable="
              << initializedName->get_name()
              << " is not owned by exactly one item in its linked POINTER "
                 "statement\n";
    ROSE_ABORT();
  }
  return true;
}

bool fortranCommonStatementOwnsArrayShape(
    const SgInitializedName *initializedName) {
  ASSERT_not_null(initializedName);
  SgCommonBlock *common = isSgCommonBlock(
      initializedName->get_fortran_separate_shape_declaration());
  if (common == nullptr) {
    return false;
  }
  if (findOrdinaryFortranArrayType(initializedName->get_type()) == nullptr ||
      common->get_block_list().empty()) {
    std::cerr << "REX_UNPARSE_INVARIANT[fortran-common-shape-source]: "
                 "variable="
              << initializedName->get_name()
              << " has a COMMON shape owner but no exact ordinary array type "
                 "and COMMON statement\n";
    ROSE_ABORT();
  }

  size_t matchingItems = 0;
  for (SgCommonBlockObject *block : common->get_block_list()) {
    SgExprListExp *objects =
        block != nullptr ? block->get_variable_reference_list() : nullptr;
    if (block == nullptr || block->get_parent() != common ||
        objects == nullptr || objects->get_parent() != block ||
        objects->get_expressions().empty()) {
      std::cerr << "REX_UNPARSE_INVARIANT[fortran-common-shape-source]: "
                   "COMMON statement contains a malformed typed object list\n";
      ROSE_ABORT();
    }
    for (SgExpression *object : objects->get_expressions()) {
      SgPntrArrRefExp *arrayReference = isSgPntrArrRefExp(object);
      if (arrayReference == nullptr) {
        if (isSgVarRefExp(object) == nullptr ||
            object->get_parent() != objects) {
          std::cerr
              << "REX_UNPARSE_INVARIANT[fortran-common-shape-source]: "
                 "COMMON object has no exact typed scalar or array surface\n";
          ROSE_ABORT();
        }
        continue;
      }
      SgVarRefExp *variableReference =
          isSgVarRefExp(arrayReference->get_lhs_operand());
      SgVariableSymbol *symbol = variableReference != nullptr
                                     ? variableReference->get_symbol()
                                     : nullptr;
      SgExprListExp *shape = isSgExprListExp(arrayReference->get_rhs_operand());
      if (arrayReference->get_parent() != objects ||
          variableReference == nullptr || symbol == nullptr ||
          shape == nullptr || shape->get_expressions().empty()) {
        std::cerr << "REX_UNPARSE_INVARIANT[fortran-common-shape-source]: "
                     "COMMON statement contains a malformed typed array "
                     "declarator\n";
        ROSE_ABORT();
      }
      if (symbol->get_declaration() == initializedName) {
        ++matchingItems;
      }
    }
  }
  if (matchingItems != 1) {
    std::cerr << "REX_UNPARSE_INVARIANT[fortran-common-shape-source]: "
                 "variable="
              << initializedName->get_name()
              << " is not owned by exactly one shaped item in its linked "
                 "COMMON statement\n";
    ROSE_ABORT();
  }
  return true;
}

bool fortranCrayPointerStatementOwnsArrayShape(
    const SgInitializedName *initializedName) {
  ASSERT_not_null(initializedName);
  SgVariableDeclaration *pointerDeclaration = isSgVariableDeclaration(
      initializedName->get_fortran_separate_shape_declaration());
  if (pointerDeclaration == nullptr) {
    return false;
  }
  SgArrayType *semanticArray =
      findOrdinaryFortranArrayType(initializedName->get_type());
  SgScopeStatement *scope = initializedName->get_scope();
  if (semanticArray == nullptr || semanticArray->get_rank() <= 0 ||
      scope == nullptr || pointerDeclaration->get_scope() != scope ||
      pointerDeclaration->get_fortran_declaration_origin() !=
          SgVariableDeclaration::e_fortran_source_declaration ||
      pointerDeclaration->get_variables().size() != 1) {
    std::cerr << "REX_UNPARSE_INVARIANT[fortran-cray-pointer-shape-source]: "
                 "pointee="
              << initializedName->get_name()
              << " has no exact shaped Cray POINTER owner\n";
    ROSE_ABORT();
  }

  SgInitializedName *pointer = pointerDeclaration->get_variables().front();
  SgExprListExp *shape = pointer != nullptr
                             ? pointer->get_fortran_cray_pointer_pointee_shape()
                             : nullptr;
  if (pointer == nullptr || pointer->get_parent() != pointerDeclaration ||
      pointer->get_scope() != scope ||
      isSgTypeCrayPointer(pointer->get_fortran_source_type()) == nullptr ||
      pointer->get_cray_pointer_pointee() != initializedName ||
      shape == nullptr || shape->get_parent() != pointer ||
      shape->get_expressions().empty() ||
      shape->get_expressions().size() !=
          static_cast<std::size_t>(semanticArray->get_rank()) ||
      !initializedName->get_shapeDeferred()) {
    std::cerr << "REX_UNPARSE_INVARIANT[fortran-cray-pointer-shape-source]: "
                 "pointee="
              << initializedName->get_name()
              << " is not owned by one exact typed Cray POINTER array "
                 "declarator\n";
    ROSE_ABORT();
  }
  return true;
}

bool fortranSeparateStatementOwnsArrayShape(
    const SgInitializedName *initializedName) {
  ASSERT_not_null(initializedName);
  SgStatement *owner =
      initializedName->get_fortran_separate_shape_declaration();
  if (owner == nullptr) {
    return false;
  }
  if (isSgAttributeSpecificationStatement(owner) != nullptr) {
    return fortranAttributeStatementOwnsArrayShape(initializedName);
  }
  if (isSgCommonBlock(owner) != nullptr) {
    return fortranCommonStatementOwnsArrayShape(initializedName);
  }
  if (isSgVariableDeclaration(owner) != nullptr) {
    return fortranCrayPointerStatementOwnsArrayShape(initializedName);
  }
  std::cerr << "REX_UNPARSE_INVARIANT[fortran-separate-shape-source]: "
               "variable="
            << initializedName->get_name()
            << " has an unsupported separate-shape statement owner\n";
  ROSE_ABORT();
}

void FortranCodeGeneration_locatedNode::unparseAttributeSpecificationStatement(
    SgStatement *stmt, SgUnparse_Info &info) {
  SgAttributeSpecificationStatement *attributeSpecificationStatement =
      isSgAttributeSpecificationStatement(stmt);
  ASSERT_not_null(attributeSpecificationStatement);

  string name;
  switch (attributeSpecificationStatement->get_attribute_kind()) {
  case SgAttributeSpecificationStatement::e_unknown_attribute_spec:
    std::cerr << "Error: unknown SgAttributeSpecificationStatement kind"
              << std::endl;
    ROSE_ABORT();
  case SgAttributeSpecificationStatement::e_accessStatement_private:
    name = "private";
    break;
  case SgAttributeSpecificationStatement::e_accessStatement_public:
    name = "public";
    break;
  case SgAttributeSpecificationStatement::e_allocatableStatement:
    name = "allocatable";
    break;
  case SgAttributeSpecificationStatement::e_asynchronousStatement:
    name = "asynchronous";
    break;
  case SgAttributeSpecificationStatement::e_bindStatement:
    name = "bind";
    break;
  case SgAttributeSpecificationStatement::e_dataStatement:
    name = "data";
    break;
  case SgAttributeSpecificationStatement::e_dimensionStatement:
    name = "dimension";
    break;
  case SgAttributeSpecificationStatement::e_externalStatement:
    name = "external";
    break;
  case SgAttributeSpecificationStatement::e_intentStatement:
    name = "intent";
    break;
  case SgAttributeSpecificationStatement::e_intrinsicStatement:
    name = "intrinsic";
    break;
  case SgAttributeSpecificationStatement::e_optionalStatement:
    name = "optional";
    break;
  case SgAttributeSpecificationStatement::e_parameterStatement:
    name = "parameter";
    break;
  case SgAttributeSpecificationStatement::e_pointerStatement:
    name = "pointer";
    break;
  case SgAttributeSpecificationStatement::e_protectedStatement:
    name = "protected";
    break;
  case SgAttributeSpecificationStatement::e_saveStatement:
    name = "save";
    break;
  case SgAttributeSpecificationStatement::e_targetStatement:
    name = "target";
    break;
  case SgAttributeSpecificationStatement::e_valueStatement:
    name = "value";
    break;
  case SgAttributeSpecificationStatement::e_volatileStatement:
    name = "volatile";
    break;
  case SgAttributeSpecificationStatement::e_last_attribute_spec:
    std::cerr << "Error: sentinel SgAttributeSpecificationStatement kind"
              << std::endl;
    ROSE_ABORT();

  default: {
    printf("Error: default reached %d \n",
           attributeSpecificationStatement->get_attribute_kind());
    ROSE_ABORT();
  }
  }

  curprint(name);

  if (attributeSpecificationStatement->get_attribute_kind() ==
      SgAttributeSpecificationStatement::e_intentStatement) {
    // This define is copied from OFP actionEnum.h This needs to be better
    // handled later (using a proper enum type).
#define IntentSpecBase 600
    const int IN = IntentSpecBase + 0;
    const int OUT = IntentSpecBase + 1;
    const int INOUT = IntentSpecBase + 2;

    string intentString;
    switch (attributeSpecificationStatement->get_intent()) {
    case IN:
      intentString = "in";
      break;
    case OUT:
      intentString = "out";
      break;
    case INOUT:
      intentString = "inout";
      break;

    default: {
      printf("Error: default reached "
             "attributeSpecificationStatement->get_intent() = %d \n",
             attributeSpecificationStatement->get_intent());
      ROSE_ABORT();
    }
    }

    curprint("(" + intentString + ")");
  }

  // The parameter statement is a bit different from the other attribute
  // statements (perhaps enough for it to be it's own IR node.
  if (attributeSpecificationStatement->get_attribute_kind() ==
      SgAttributeSpecificationStatement::e_parameterStatement) {
    ASSERT_not_null(attributeSpecificationStatement->get_parameter_list());

    curprint("(");
    unparseExpression(attributeSpecificationStatement->get_parameter_list(),
                      info);
    curprint(")");
  }

  if (attributeSpecificationStatement->get_attribute_kind() ==
      SgAttributeSpecificationStatement::e_bindStatement) {
    ASSERT_not_null(attributeSpecificationStatement->get_bind_list());
    ROSE_ASSERT(
        attributeSpecificationStatement->get_declarationModifier().isBind());
    if (attributeSpecificationStatement->get_linkage().empty()) {
      std::cerr << "Error: Fortran BIND statement has no language binding spec"
                << std::endl;
      ROSE_ABORT();
    }

    curprint("(");
    curprint(attributeSpecificationStatement->get_linkage());
    if (attributeSpecificationStatement->get_binding_label().empty() == false) {
      curprint(",NAME=\"");
      curprint(attributeSpecificationStatement->get_binding_label());
      curprint("\"");
    }
    if (attributeSpecificationStatement->get_binding_cdefined()) {
      curprint(",CDEFINED");
    }
    curprint(")");
  }

  const bool is_access_statement =
      attributeSpecificationStatement->get_attribute_kind() ==
          SgAttributeSpecificationStatement::e_accessStatement_private ||
      attributeSpecificationStatement->get_attribute_kind() ==
          SgAttributeSpecificationStatement::e_accessStatement_public;
  const bool has_named_access_targets =
      is_access_statement &&
      !attributeSpecificationStatement->get_name_list().empty();

  if ((attributeSpecificationStatement->get_attribute_kind() !=
       SgAttributeSpecificationStatement::e_parameterStatement) &&
      (attributeSpecificationStatement->get_attribute_kind() !=
       SgAttributeSpecificationStatement::e_dataStatement) &&
      (((!is_access_statement) &&
        attributeSpecificationStatement->get_parameter_list() != nullptr) ||
       has_named_access_targets)) {
    // The parameter and data statement do not use "::" in their syntax
    curprint(" :: ");
  } else {
    // Need a space to prevent variables from being too close to the keywords
    // (e.g. "privatei" should be "private i").
    curprint(" ");
  }

  if (attributeSpecificationStatement->get_attribute_kind() ==
      SgAttributeSpecificationStatement::e_bindStatement) {
    ASSERT_not_null(attributeSpecificationStatement->get_bind_list());

    unparseExpression(attributeSpecificationStatement->get_bind_list(), info);
  }

  if (attributeSpecificationStatement->get_attribute_kind() ==
      SgAttributeSpecificationStatement::e_allocatableStatement) {
    ASSERT_not_null(attributeSpecificationStatement->get_parameter_list());

    unparseExpression(attributeSpecificationStatement->get_parameter_list(),
                      info);
  }

  if (attributeSpecificationStatement->get_attribute_kind() ==
      SgAttributeSpecificationStatement::e_externalStatement) {
    // for this case the functions need to be output just as names without the
    // "()"
    ASSERT_not_null(attributeSpecificationStatement->get_parameter_list());

    SgExpressionPtrList &functionNameList =
        attributeSpecificationStatement->get_parameter_list()
            ->get_expressions();
    SgExpressionPtrList::iterator i = functionNameList.begin();
    while (i != functionNameList.end()) {
      SgName name;
      if (SgFunctionRefExp *functionRefExp = isSgFunctionRefExp(*i)) {
        SgFunctionSymbol *symbol = functionRefExp->get_symbol();
        SgFunctionDeclaration *declaration =
            symbol != nullptr ? symbol->get_declaration() : nullptr;
        if (declaration == nullptr || declaration->get_type() == nullptr ||
            isSgFunctionType(declaration->get_type()) == nullptr) {
          std::cerr << "REX_UNPARSE_INVARIANT[fortran-external-entity]: "
                       "EXTERNAL function reference has no complete "
                       "procedure symbol and type\n";
          ROSE_ABORT();
        }
        name = symbol->get_name();
      } else if (SgVarRefExp *variableRefExp = isSgVarRefExp(*i)) {
        SgVariableSymbol *symbol = variableRefExp->get_symbol();
        SgInitializedName *declaration =
            symbol != nullptr ? symbol->get_declaration() : nullptr;
        SgType *type =
            declaration != nullptr ? declaration->get_type() : nullptr;
        SgType *procedureType =
            type != nullptr ? type->stripType(SgType::STRIP_MODIFIER_TYPE |
                                              SgType::STRIP_POINTER_TYPE)
                            : nullptr;
        if (declaration == nullptr || procedureType == nullptr ||
            isSgFunctionType(procedureType) == nullptr) {
          std::cerr << "REX_UNPARSE_INVARIANT[fortran-external-entity]: "
                       "EXTERNAL variable reference is not a complete typed "
                       "procedure entity\n";
          ROSE_ABORT();
        }
        name = symbol->get_name();
      } else {
        std::cerr << "REX_UNPARSE_INVARIANT[fortran-external-entity]: "
                     "EXTERNAL list contains neither a function nor a typed "
                     "procedure-entity reference\n";
        ROSE_ABORT();
      }
      if (name.is_null()) {
        std::cerr << "REX_UNPARSE_INVARIANT[fortran-external-entity]: "
                     "EXTERNAL procedure entity has no exact name\n";
        ROSE_ABORT();
      }
      curprint(name);

      i++;

      if (i != functionNameList.end())
        curprint(", ");
    }
  }

  if (attributeSpecificationStatement->get_attribute_kind() ==
      SgAttributeSpecificationStatement::e_dataStatement) {
    curprint(" ");

    SgDataStatementGroupPtrList &dataStatementGroupList =
        attributeSpecificationStatement->get_data_statement_group_list();
    SgDataStatementGroupPtrList::iterator i_group =
        dataStatementGroupList.begin();
    while (i_group != dataStatementGroupList.end()) {
      SgDataStatementObjectPtrList &dataStatementObjectList =
          (*i_group)->get_object_list();
      SgDataStatementObjectPtrList::iterator i_object =
          dataStatementObjectList.begin();

      while (i_object != dataStatementObjectList.end()) {
        unparseExpression((*i_object)->get_variableReference_list(), info);
        i_object++;
        if (i_object != dataStatementObjectList.end()) {
          curprint(", ");
        }
      }

      // Now output the data values
      curprint("/");
      SgDataStatementValuePtrList &dataStatementValueList =
          (*i_group)->get_value_list();
      SgDataStatementValuePtrList::iterator i_value =
          dataStatementValueList.begin();
      while (i_value != dataStatementValueList.end()) {
        SgDataStatementValue::data_statement_value_enum value_kind =
            (*i_value)->get_data_initialization_format();
        switch (value_kind) {
        case SgDataStatementValue::e_unknown:
        case SgDataStatementValue::e_default: {
          printf(
              "Error: value_kind == e_unknown or e_default value_kind = %d \n",
              value_kind);
          ROSE_ABORT();
        }

        case SgDataStatementValue::e_explict_list: {
          unparseExpression((*i_value)->get_initializer_list(), info);
          break;
        }

        case SgDataStatementValue::e_implicit_list: {
          ASSERT_require(
              (*i_value)->get_initializer_list()->get_expressions().empty());

          SgExpression *repeatExpression = (*i_value)->get_repeat_expression();
          ASSERT_not_null(repeatExpression);
          SgExpression *constantExpression =
              (*i_value)->get_constant_expression();
          ASSERT_not_null(constantExpression);

          unparseExpression(repeatExpression, info);
          curprint(" * ");
          unparseExpression(constantExpression, info);
          break;
        }

        case SgDataStatementValue::e_implied_do: {
          std::cerr << "Error: SgDataStatementValue implied-do has no "
                       "structured unparser representation"
                    << std::endl;
          ROSE_ABORT();
        }

        default: {
          printf("Error: default reached value_kind = %d \n", value_kind);
          ROSE_ABORT();
        }
        }

        i_value++;
        if (i_value != dataStatementValueList.end()) {
          curprint(", ");
        }
      }
      curprint("/");

      i_group++;
      if (i_group != dataStatementGroupList.end()) {
        curprint(", ");
      }
    }
  }

  if (attributeSpecificationStatement->get_attribute_kind() ==
      SgAttributeSpecificationStatement::e_dimensionStatement) {
    SgExprListExp *parameters =
        attributeSpecificationStatement->get_parameter_list();
    if (parameters == nullptr ||
        parameters->get_parent() != attributeSpecificationStatement ||
        parameters->get_expressions().empty()) {
      std::cerr << "REX_UNPARSE_INVARIANT[fortran-dimension-source]: "
                   "DIMENSION statement has no nonempty owned parameter list\n";
      ROSE_ABORT();
    }

    const SgExpressionPtrList &parameterList = parameters->get_expressions();
    for (size_t i = 0; i < parameterList.size(); ++i) {
      SgPntrArrRefExp *arrayReference = isSgPntrArrRefExp(parameterList[i]);
      SgVarRefExp *variableReference =
          arrayReference != nullptr
              ? isSgVarRefExp(arrayReference->get_lhs_operand())
              : nullptr;
      SgVariableSymbol *symbol = variableReference != nullptr
                                     ? variableReference->get_symbol()
                                     : nullptr;
      SgInitializedName *initializedName =
          symbol != nullptr ? symbol->get_declaration() : nullptr;
      if (arrayReference == nullptr ||
          arrayReference->get_parent() != parameters ||
          initializedName == nullptr ||
          initializedName->get_fortran_separate_shape_declaration() !=
              attributeSpecificationStatement ||
          !fortranAttributeStatementOwnsArrayShape(initializedName)) {
        std::cerr << "REX_UNPARSE_INVARIANT[fortran-dimension-source]: "
                     "DIMENSION item does not have an exact variable-to-"
                     "statement ownership edge\n";
        ROSE_ABORT();
      }
      if (i != 0) {
        curprint(", ");
      }
      unparseExpression(arrayReference, info);
    }
  }

  if (attributeSpecificationStatement->get_attribute_kind() ==
      SgAttributeSpecificationStatement::e_pointerStatement) {
    SgExprListExp *parameters =
        attributeSpecificationStatement->get_parameter_list();
    if (parameters == nullptr ||
        parameters->get_parent() != attributeSpecificationStatement ||
        parameters->get_expressions().empty()) {
      std::cerr << "REX_UNPARSE_INVARIANT[fortran-pointer-source]: POINTER "
                   "statement has no nonempty owned parameter list\n";
      ROSE_ABORT();
    }
    const SgExpressionPtrList &items = parameters->get_expressions();
    for (size_t index = 0; index < items.size(); ++index) {
      SgPntrArrRefExp *arrayReference = isSgPntrArrRefExp(items[index]);
      SgVarRefExp *variableReference =
          arrayReference != nullptr
              ? isSgVarRefExp(arrayReference->get_lhs_operand())
              : isSgVarRefExp(items[index]);
      SgVariableSymbol *symbol = variableReference != nullptr
                                     ? variableReference->get_symbol()
                                     : nullptr;
      SgInitializedName *initializedName =
          symbol != nullptr ? symbol->get_declaration() : nullptr;
      if (items[index] == nullptr || items[index]->get_parent() != parameters ||
          initializedName == nullptr ||
          initializedName->get_fortran_separate_pointer_declaration() !=
              attributeSpecificationStatement ||
          !fortranSeparatePointerStatementOwnsPointerAttribute(
              initializedName)) {
        std::cerr << "REX_UNPARSE_INVARIANT[fortran-pointer-source]: POINTER "
                     "item has no exact variable-to-statement ownership edge\n";
        ROSE_ABORT();
      }
      if (index != 0) {
        curprint(", ");
      }
      unparseExpression(items[index], info);
    }
  }

  const SgStringList &localList =
      attributeSpecificationStatement->get_name_list();

  // We need to recognize the commonblockobject in the list
  Rose_STL_Container<SgNode *> commonBlockList = NodeQuery::querySubTree(
      attributeSpecificationStatement->get_scope(), V_SgCommonBlockObject);

  SgStringList::const_iterator i = localList.begin();
  string outputName = "";
  while (i != localList.end()) {
    outputName = *i;
    for (Rose_STL_Container<SgNode *>::iterator j = commonBlockList.begin();
         j != commonBlockList.end(); j++) {
      SgCommonBlockObject *commonBlockObject = isSgCommonBlockObject(*j);
      ROSE_ASSERT(commonBlockObject);
      string blockName = commonBlockObject->get_block_name();
      if (namesMatch(blockName, outputName)) {
        outputName = "/" + outputName + "/";
        break;
      }
    }

    curprint(outputName);

    i++;
    if (i != localList.end()) {
      curprint(", ");
    }
  }

  unp->cur.insert_newline(1);
}

void FortranCodeGeneration_locatedNode::unparseImplicitStmt(
    SgStatement *stmt, SgUnparse_Info &info) {
  SgImplicitStatement *implicitStatement = isSgImplicitStatement(stmt);

  if (implicitStatement->get_implicit_none()) {
    curprint_keyword("IMPLICIT", info);
    curprint(" ");
    curprint_keyword("NONE", info);
    curprint(" ");

    switch (implicitStatement->get_implicit_spec()) {
    case SgImplicitStatement::e_none:
      break;
    case SgImplicitStatement::e_none_external:
      curprint(" (EXTERNAL)");
      break;
    case SgImplicitStatement::e_none_type:
      curprint(" (TYPE)");
      break;
    case SgImplicitStatement::e_none_external_and_type:
      curprint(" (EXTERNAL, TYPE)");
      break;
    default:
      std::cerr << "Error: invalid IMPLICIT NONE specifier in Fortran AST"
                << std::endl;
      ROSE_ABORT();
    }
  } else {
    // This is a range such as "DOUBLE PRECISION (D-E)" or a singleton such as
    // "COMPLEX (C)"

    SgInitializedNamePtrList &nameList = implicitStatement->get_variables();
    if (nameList.empty()) {
      std::cerr << "Error: non-NONE SgImplicitStatement has no type ranges"
                << std::endl;
      ROSE_ABORT();
    } else {
      ROSE_ASSERT(nameList.empty() == false);

      curprint_keyword("IMPLICIT", info);
      curprint(" ");

      SgInitializedNamePtrList::iterator i = nameList.begin();
      // DQ (12/2/2010): New code to handle implicit statements.
      while (i != nameList.end()) {
        SgInitializedName *implicitTypeName = *i;
        ASSERT_not_null(implicitTypeName);

        SgUnparse_Info typeInfo(info);
        typeInfo.set_reference_node_for_qualification(implicitTypeName);
        unp->u_fortran_type->unparseType(implicitTypeName->get_type(),
                                         typeInfo);
        curprint("(");
        curprint(implicitTypeName->get_name().str());
        curprint(")");

        i++;
        if (i != nameList.end())
          curprint(",");
      }
    }
  }

  unp->cur.insert_newline(1);
}

void FortranCodeGeneration_locatedNode::unparseBlockDataStmt(SgStatement *,
                                                             SgUnparse_Info &) {
  std::cerr << "Error: legacy SgBlockDataStatement cannot represent a complete "
               "BLOCK DATA program unit; use SgProcedureHeaderStatement"
            << std::endl;
  ROSE_ABORT();
}

void FortranCodeGeneration_locatedNode::unparseStatementFunctionStmt(
    SgStatement *stmt, SgUnparse_Info &info) {
  SgStatementFunctionStatement *stmtFunc = isSgStatementFunctionStatement(stmt);
  ASSERT_not_null(stmtFunc);

  SgFunctionDeclaration *funcDecl = stmtFunc->get_function();
  ASSERT_not_null(funcDecl);

  curprint(funcDecl->get_name().str());
  curprint("(");

  SgFunctionParameterList *params = funcDecl->get_parameterList();
  bool needComma = false;
  if (params != nullptr) {
    for (SgInitializedName *arg : params->get_args()) {
      if (arg == nullptr) {
        std::cerr << "Error: null argument in Fortran statement function"
                  << std::endl;
        ROSE_ABORT();
      }
      if (needComma) {
        curprint(", ");
      }
      curprint(arg->get_name().str());
      needComma = true;
    }
  }
  curprint(") = ");

  SgExpression *expr = stmtFunc->get_expression();
  ASSERT_not_null(expr);
  unparseExpression(expr, info);

  unp->cur.insert_newline(1);
}

void FortranCodeGeneration_locatedNode::unparseWhereStmt(SgStatement *stmt,
                                                         SgUnparse_Info &info) {
  // Currently the simple "where (a) b = 0" is unparsed as "where (a) b = 0
  // endwhere"

  SgWhereStatement *whereStatement = isSgWhereStatement(stmt);
  ASSERT_not_null(whereStatement);

  if (whereStatement->get_string_label().empty() == false) {
    // Output the string label
    curprint(whereStatement->get_string_label() + ": ");
  }

  curprint("WHERE (");
  unparseExpression(whereStatement->get_condition(), info);
  curprint(") ");

  bool output_endwhere = whereStatement->get_has_end_statement();

  if (output_endwhere) {
    ASSERT_not_null(whereStatement->get_body());
    unparseStatement(whereStatement->get_body(), info);
  } else {
    SgStatementPtrList &statementList =
        whereStatement->get_body()->get_statements();
    ROSE_ASSERT(statementList.size() == 1);
    SgStatement *statement = *(statementList.begin());
    ASSERT_not_null(statement);
    SgUnparse_Info info_without_formating(info);
    info_without_formating.set_SkipFormatting();
    unparseStatement(statement, info_without_formating);
  }

  SgElseWhereStatement *elsewhereStatement = whereStatement->get_elsewhere();
  if (elsewhereStatement != nullptr) {
    if (output_endwhere) {
      unparseStatement(elsewhereStatement, info);
    } else {
      // Output the statement on the same line as the "else"
      SgStatementPtrList &statementList =
          elsewhereStatement->get_body()->get_statements();
      ROSE_ASSERT(statementList.size() == 1);
      SgStatement *statement = *(statementList.begin());
      ASSERT_not_null(statement);
      SgUnparse_Info info_without_formating(info);
      info_without_formating.set_SkipFormatting();
      unparseStatement(statement, info_without_formating);
    }
  }

  // The end where statement can have a label
  if (output_endwhere) {
    unparseStatementNumbersSupport(whereStatement->get_end_numeric_label(),
                                   info);
    curprint("END WHERE");
    if (whereStatement->get_string_label().empty() == false) {
      // Output the string label
      curprint(" " + whereStatement->get_string_label());
    }
  }

  unp->cur.insert_newline(1);
}

void FortranCodeGeneration_locatedNode::unparseElseWhereStmt(
    SgStatement *stmt, SgUnparse_Info &info) {
  SgElseWhereStatement *elseWhereStatement = isSgElseWhereStatement(stmt);
  ASSERT_not_null(elseWhereStatement);

  curprint("ELSEWHERE ");

  ASSERT_not_null(elseWhereStatement->get_condition());

  // Only unparse the "()" if there is a valid elsewhere mask.
  if (isSgNullExpression(elseWhereStatement->get_condition()) == nullptr) {
    curprint("(");
    unp->u_exprStmt->unparseExpression(elseWhereStatement->get_condition(),
                                       info);
    curprint(")");
  }

  ASSERT_not_null(elseWhereStatement->get_body());
  unparseStatement(elseWhereStatement->get_body(), info);

  SgElseWhereStatement *nested_elseWhereStatement =
      elseWhereStatement->get_elsewhere();
  if (nested_elseWhereStatement != nullptr) {
    unparseStatement(nested_elseWhereStatement, info);
  }
}

void FortranCodeGeneration_locatedNode::unparseNullifyStmt(
    SgStatement *stmt, SgUnparse_Info &info) {
  curprint("NULLIFY ");
  curprint("(");
  SgExprListExp *dlist = (isSgNullifyStatement(stmt))->get_pointer_list();
  SgExpressionPtrList::iterator i = dlist->get_expressions().begin();
  while (i != dlist->get_expressions().end()) {
    unparseExpression(*i, info);
    i++;

    if (i != dlist->get_expressions().end()) {
      curprint(", ");
    }
  }

  curprint(")");
  unp->cur.insert_newline(1);
}

void FortranCodeGeneration_locatedNode::unparseEquivalenceStmt(
    SgStatement *stmt, SgUnparse_Info &info) {
  // This organization is as a SgExprListExp of SgExprListExp of SgExpression
  // objects. This we can represent: "equivalence (i,j), (k,l,m,n)"

  SgEquivalenceStatement *equivalenceStatement = isSgEquivalenceStatement(stmt);

  curprint("equivalence ");

  ASSERT_not_null(equivalenceStatement->get_equivalence_set_list());

  SgExpressionPtrList &expressionList =
      equivalenceStatement->get_equivalence_set_list()->get_expressions();
  SgExpressionPtrList::iterator i = expressionList.begin();
  while (i != expressionList.end()) {
    curprint("( ");
    unparseExpression(*i, info);
    curprint(" )");

    i++;

    if (i != expressionList.end()) {
      curprint(", ");
    }
  }

  unp->cur.insert_newline(1);
}

void FortranCodeGeneration_locatedNode::unparseLabel(SgLabelRefExp *exp) {
  ASSERT_not_null(exp);
  SgLabelSymbol *symbol = exp->get_symbol();
  ASSERT_not_null(symbol);

  // DQ (12/24/2007): Every numeric label should have been associated with a
  // statement!
  ASSERT_not_null(symbol->get_fortran_statement());
  int numericLabel = symbol->get_numeric_label_value();

  curprint(StringUtility::numberToString(numericLabel));
}

void FortranCodeGeneration_locatedNode::unparseArithmeticIfStmt(
    SgStatement *stmt, SgUnparse_Info &info) {
  SgArithmeticIfStatement *arithmeticIf = isSgArithmeticIfStatement(stmt);
  ASSERT_not_null(arithmeticIf);
  ROSE_ASSERT(arithmeticIf->get_conditional());

  // condition
  curprint("IF (");
  info.set_inConditional();

  // DQ (8/15/2007): In C the condiion is a statment, and in Fortran the
  // condition is an expression! We might want to fix this by having an IR node
  // to represent the Fortran "if" statement.
  SgExpression *expression = isSgExpression(arithmeticIf->get_conditional());
  unparseExpression(expression, info);

  info.unset_inConditional();
  curprint(") ");

  unparseLabel(arithmeticIf->get_less_label());
  curprint(",");
  unparseLabel(arithmeticIf->get_equal_label());
  curprint(",");
  unparseLabel(arithmeticIf->get_greater_label());

  unp->cur.insert_newline(1);
}

void FortranCodeGeneration_locatedNode::unparseAssignStmt(
    SgStatement *stmt, SgUnparse_Info &info) {
  SgAssignStatement *assignStmt = isSgAssignStatement(stmt);
  ASSERT_not_null(assignStmt);

  curprint("ASSIGN ");
  unparseLabel(assignStmt->get_label());
  curprint(" TO ");
  unparseExpression(assignStmt->get_value(), info);
  unp->cur.insert_newline(1);
}

void FortranCodeGeneration_locatedNode::unparseComputedGotoStmt(
    SgStatement *stmt, SgUnparse_Info &info) {
  SgComputedGotoStatement *computedGoto = isSgComputedGotoStatement(stmt);

  curprint("GOTO (");

  ASSERT_not_null(computedGoto->get_labelList());
  SgExpressionPtrList &labelList =
      computedGoto->get_labelList()->get_expressions();

  int size = labelList.size();
  for (int i = 0; i < size; i++) {
    SgLabelRefExp *labelRefExp = isSgLabelRefExp(labelList[i]);
    ASSERT_not_null(labelRefExp);

    SgLabelSymbol *labelSymbol = labelRefExp->get_symbol();

    // DQ (12/24/2007): Every numeric label should have been associated with a
    // statement!
    ASSERT_not_null(labelSymbol->get_fortran_statement());
    int numericLabel = labelSymbol->get_numeric_label_value();

    ROSE_ASSERT(numericLabel >= 0);
    string numericLabelString = StringUtility::numberToString(numericLabel);
    curprint(numericLabelString);

    if (i < size - 1) {
      curprint(", ");
    }
  }

  curprint(" ) ");

  unparseExpression(computedGoto->get_label_index(), info);
  unp->cur.insert_newline(1);
}

void FortranCodeGeneration_locatedNode::unparseAssignedGotoStmt(
    SgStatement *stmt, SgUnparse_Info &info) {
  SgAssignedGotoStatement *assignedGoto = isSgAssignedGotoStatement(stmt);
  ASSERT_not_null(assignedGoto);

  SgExprListExp *targets = assignedGoto->get_targets();
  ASSERT_not_null(targets);
  SgExpressionPtrList &exprs = targets->get_expressions();
  ROSE_ASSERT(!exprs.empty());

  curprint("GO TO ");
  unparseExpression(exprs.front(), info);

  if (exprs.size() > 1) {
    curprint(", (");
    for (auto it = std::next(exprs.begin()); it != exprs.end(); ++it) {
      SgLabelRefExp *labelRef = isSgLabelRefExp(*it);
      ASSERT_not_null(labelRef);
      unparseLabel(labelRef);
      if (std::next(it) != exprs.end()) {
        curprint(", ");
      }
    }
    curprint(")");
  }

  unp->cur.insert_newline(1);
}

//----------------------------------------------------------------------------
//  FortranCodeGeneration_locatedNode::<program units>
//----------------------------------------------------------------------------

void FortranCodeGeneration_locatedNode::unparseModuleStmt(
    SgStatement *stmt, SgUnparse_Info &info) {
  // Sage node corresponds to Fortran module

  SgModuleStatement *mod = isSgModuleStatement(stmt);
  ROSE_ASSERT(mod);
  if (mod->get_name().is_null()) {
    std::cerr << "Error: Fortran MODULE or SUBMODULE has no name" << std::endl;
    ROSE_ABORT();
  }

  const SgName &parentName = mod->get_fortran_submodule_parent();
  bool isSubmodule = false;
  switch (mod->get_fortran_module_kind()) {
  case SgModuleStatement::e_fortran_submodule:
    isSubmodule = true;
    if (!isValidFortranSubmoduleParent(parentName)) {
      std::cerr << "REX_UNPARSE_INVARIANT[fortran-submodule-parent]: invalid "
                   "parent='"
                << parentName.getString() << "'" << std::endl;
      ROSE_ABORT();
    }
    curprint("SUBMODULE (");
    curprint(parentName.getString());
    curprint(") ");
    break;
  case SgModuleStatement::e_fortran_module:
    if (!parentName.is_null()) {
      std::cerr << "REX_UNPARSE_INVARIANT[fortran-module-kind]: MODULE has "
                   "unexpected submodule parent='"
                << parentName.getString() << "'" << std::endl;
      ROSE_ABORT();
    }
    curprint("MODULE ");
    break;
  default:
    std::cerr << "REX_UNPARSE_INVARIANT[fortran-module-kind]: invalid value="
              << static_cast<int>(mod->get_fortran_module_kind()) << std::endl;
    ROSE_ABORT();
  }
  curprint(mod->get_name().str());

  // body
  ASSERT_not_null(mod->get_definition());
  SgUnparse_Info ninfo(info);
  unparseStatement(mod->get_definition(), ninfo);

  unparseStatementNumbersSupport(mod->get_end_numeric_label(), info);

  if (isSubmodule) {
    curprint("END SUBMODULE");
    if (!mod->get_name().getString().empty()) {
      curprint(" ");
      curprint(mod->get_name().str());
    }
  } else {
    curprint("END MODULE");
  }
  ASSERT_not_null(unp);
  unp->cur.insert_newline(1);
}

void FortranCodeGeneration_locatedNode::unparseProgHdrStmt(
    SgStatement *stmt, SgUnparse_Info &info) {
  // Sage node corresponds to Fortran program

  SgProgramHeaderStatement *proghdr = isSgProgramHeaderStatement(stmt);
  ASSERT_not_null(proghdr);
  const bool lacksSourceName =
      SageInterface::isFortranProgramUnitWithoutSourceName(proghdr);
  const bool hasProgramStatement = hasExplicitProgramStatement(proghdr);
  if (hasProgramStatement == lacksSourceName) {
    std::cerr << "REX_UNPARSE_INVARIANT[program-source-name]: PROGRAM "
                 "statement metadata disagrees with public source-name "
                 "presence\n";
    ROSE_ABORT();
  }
  if (hasProgramStatement && !isValidFortranIdentifier(proghdr->get_name())) {
    std::cerr << "Error: explicit Fortran PROGRAM has an invalid source name"
              << std::endl;
    ROSE_ABORT();
  }
  if (proghdr->isForward() || proghdr->get_definition() == nullptr) {
    std::cerr << "Error: Fortran PROGRAM header must own a definition"
              << std::endl;
    ROSE_ABORT();
  }
  const SgName endStatementName = proghdr->get_end_statement_name();
  const bool hasEndStatementName = !endStatementName.getString().empty();
  if (proghdr->get_named_in_end_statement() != hasEndStatementName) {
    std::cerr << "REX_UNPARSE_INVARIANT[program-end-name]: END PROGRAM name "
                 "metadata is inconsistent\n";
    ROSE_ABORT();
  }
  if (hasEndStatementName && !hasProgramStatement) {
    std::cerr << "REX_UNPARSE_INVARIANT[program-end-name]: implicit Fortran "
                 "main program has a named END PROGRAM\n";
    ROSE_ABORT();
  }
  if (hasEndStatementName &&
      (!isValidFortranIdentifier(endStatementName) ||
       !namesMatch(proghdr->get_name().str(), endStatementName.str()))) {
    std::cerr << "REX_UNPARSE_INVARIANT[program-end-name]: END PROGRAM name '"
              << endStatementName << "' is invalid or does not match PROGRAM "
              << "name '" << proghdr->get_name() << "'\n";
    ROSE_ABORT();
  }

  if (!proghdr->isForward() && proghdr->get_definition() != nullptr &&
      !info.SkipFunctionDefinition()) {
    // Output the function declaration with definition

    // The unparsing of the definition will cause the unparsing of the
    // declaration (with SgUnparse_Info flags set to just unparse a forward
    // declaration!)
    SgUnparse_Info ninfo(info);

    // To avoid end of statement formatting (added CR's) we call the
    // unparseFuncDefnStmt directly
    unparseFuncDefnStmt(proghdr->get_definition(), ninfo);

    unparseStatementNumbersSupport(proghdr->get_end_numeric_label(), info);

    if (hasProgramStatement) {
      // The "END" has just been output by the unparsing of the
      // SgFunctionDefinition so just add "PROGRAM <name>".
      curprint("END PROGRAM");
      if (hasEndStatementName) {
        curprint(" ");
        curprint(endStatementName.str());
      }

      // Output 2 new lines to better separate functions visually in the output
      unp->cur.insert_newline(1);
      unp->cur.insert_newline(2); // FMZ
    } else {
      // And "end" is always required even if the program-stmt is not explicitly
      // used.
      curprint("END");

      // Added to fix problem reported by Liao (email 12/28/2007).
      unp->cur.insert_newline(1);
    }
  } else {
    // Output only the header syntax while the definition is being emitted by
    // the enclosing SgFunctionDefinition traversal.
    if (hasProgramStatement) {
      curprint("PROGRAM ");
      curprint(proghdr->get_name().str());
    }

    // Output 1 new line so that new statements will appear on their own line
    // after the SgProgramHeaderStatement declaration.
    unp->cur.insert_newline(1);
  }
}

//----------------------------------------------------------------------------
//  FortranCodeGeneration_locatedNode::<declarations>
//----------------------------------------------------------------------------

void FortranCodeGeneration_locatedNode::unparseInterfaceStmt(
    SgStatement *stmt, SgUnparse_Info &info) {
  // Sage node corresponds to Fortran interface statement
  SgInterfaceStatement *interfaceStatement = isSgInterfaceStatement(stmt);
  ASSERT_not_null(interfaceStatement);

  for (SgInterfaceBody *body : interfaceStatement->get_interface_body_list()) {
    if (body == nullptr ||
        (body->get_use_function_name() &&
         body->get_function_name().is_null()) ||
        (!body->get_use_function_name() &&
         body->get_functionDeclaration() == nullptr)) {
      std::cerr << "Error: malformed entry in Fortran INTERFACE body"
                << std::endl;
      ROSE_ABORT();
    }
  }

  string nm = interfaceStatement->get_name().str();
  curprint("INTERFACE ");

  switch (interfaceStatement->get_generic_spec()) {
  case SgInterfaceStatement::e_named_interface_type: {
    if (nm.empty()) {
      std::cerr << "Error: named Fortran INTERFACE has no name" << std::endl;
      ROSE_ABORT();
    }
    curprint(nm);
    break;
  }
  case SgInterfaceStatement::e_operator_interface_type: {
    if (nm.empty()) {
      std::cerr << "Error: operator Fortran INTERFACE has no operator name"
                << std::endl;
      ROSE_ABORT();
    }
    curprint("operator(");
    curprint(nm);
    curprint(")");
    break;
  }
  case SgInterfaceStatement::e_assignment_interface_type: {
    if (nm.empty()) {
      std::cerr << "Error: assignment Fortran INTERFACE has no generic name"
                << std::endl;
      ROSE_ABORT();
    }
    curprint("assignment(");
    curprint(nm);
    curprint(")");
    break;
  }
  case SgInterfaceStatement::e_unnamed_interface_type: {
    // Nothing to do for this case!
    break;
  }
  default: {
    printf("Error: value of interfaceStatement->get_generic_spec() = %d \n",
           interfaceStatement->get_generic_spec());
    ROSE_ABORT();
  }
  }

  unp->cur.insert_newline(1);

  for (size_t i = 0; i < interfaceStatement->get_interface_body_list().size();
       i++) {
    bool outputFunctionName = interfaceStatement->get_interface_body_list()[i]
                                  ->get_use_function_name();
    SgName functionName =
        interfaceStatement->get_interface_body_list()[i]->get_function_name();
    SgFunctionDeclaration *functionDeclaration =
        interfaceStatement->get_interface_body_list()[i]
            ->get_functionDeclaration();

    if (outputFunctionName) {
      curprint("MODULE PROCEDURE ");
      curprint(functionName.str());
      unp->cur.insert_newline(1);
    } else if (functionDeclaration != nullptr) {
      SgProcedureHeaderStatement *procHeader =
          isSgProcedureHeaderStatement(functionDeclaration);
      if (procHeader != nullptr && procHeader->get_definition() == nullptr) {
        SgUnparse_Info ninfo(info);
        // INTERFACE bodies use a direct procedure-header dispatch so their
        // specification declarations can be emitted explicitly below.  That
        // bypasses the common statement dispatcher; preserve its fixed-form
        // label-field contract here before writing the header.
        unparseStatementNumbers(procHeader, ninfo);
        unparseProcHdrStmt(procHeader, ninfo);

        if (SgFunctionParameterScope *paramScope =
                procHeader->get_functionParameterScope()) {
          for (SgStatement *specStmt : paramScope->generateStatementList()) {
            if (auto *varDecl = isSgVariableDeclaration(specStmt)) {
              requireFortranSourceDeclarationSurface(varDecl);
            }
            unparseStatement(specStmt, info);
          }
        }

        string endKind;
        if (procHeader->isFunction()) {
          endKind = " FUNCTION";
        } else if (procHeader->isSubroutine()) {
          endKind = "SUBROUTINE";
        } else if (procHeader->isBlockData()) {
          endKind = "BLOCK DATA";
        } else {
          std::cerr << "Error: INTERFACE body procedure has unknown Fortran "
                       "subprogram kind"
                    << std::endl;
          ROSE_ABORT();
        }
        unparseStatementNumbersSupport(procHeader->get_end_numeric_label(),
                                       info);
        curprint("END " + endKind);
        if (procHeader->get_named_in_end_statement()) {
          curprint(" ");
          curprint(procHeader->get_name().str());
        }
        unp->cur.insert_newline(1);
      } else {
        unparseStatement(functionDeclaration, info);
      }
    }
  }

  unparseStatementNumbersSupport(interfaceStatement->get_end_numeric_label(),
                                 info);

  curprint("END INTERFACE ");

  ASSERT_not_null(unp);
  unp->cur.insert_newline(1);
}

void FortranCodeGeneration_locatedNode::unparseCommonBlock(
    SgStatement *stmt, SgUnparse_Info &info) {
  SgCommonBlock *commonBlock = isSgCommonBlock(stmt);
  ASSERT_not_null(commonBlock);

  SgScopeStatement *parentScope = isSgScopeStatement(commonBlock->get_parent());
  ASSERT_not_null(parentScope);

  curprint("COMMON ");

  SgCommonBlockObjectPtrList &blockList = commonBlock->get_block_list();
  if (blockList.empty()) {
    std::cerr << "Error: Fortran COMMON statement has no block objects"
              << std::endl;
    ROSE_ABORT();
  }
  for (SgCommonBlockObject *block : blockList) {
    ASSERT_not_null(block);
    ASSERT_not_null(block->get_variable_reference_list());
  }
  SgCommonBlockObjectPtrList::iterator i = blockList.begin();
  while (i != blockList.end()) {
    curprint("/");
    curprint((*i)->get_block_name());
    curprint("/");
    // Pei-Hung (07/30/2020) if declaration stmt is not available, the type
    // attribute has to be unparsed
    SgExprListExp *expr_list =
        isSgExprListExp((*i)->get_variable_reference_list());
    ASSERT_not_null(expr_list);
    SgExpressionPtrList::iterator iexp = expr_list->get_expressions().begin();

    if (iexp != expr_list->get_expressions().end()) {
      while (true) {
        SgVarRefExp *varRef = isSgVarRefExp(*iexp);
        if (varRef != nullptr) {
          SgVariableSymbol *varSym = isSgVariableSymbol(varRef->get_symbol());
          ASSERT_not_null(varSym);
          SgInitializedName *initName =
              isSgInitializedName(varSym->get_declaration());
          ASSERT_not_null(initName);
          SgVariableDeclaration *varDecl =
              isSgVariableDeclaration(initName->get_parent());
          ASSERT_not_null(varDecl);
          SgType *type = initName->get_typeptr();
          ASSERT_not_null(type);
          if (isSgBasicBlock(parentScope) != nullptr) {
            SgBasicBlock *basicBlock = isSgBasicBlock(parentScope);
            ASSERT_not_null(basicBlock);
            SgStatementPtrList &stmtList = basicBlock->get_statements();
            if (std::find(stmtList.begin(), stmtList.end(), varDecl) ==
                stmtList.end()) {
              unparseExpression(*iexp, info);
              // third argument has to be false to have the attribute unparsed
              // to individual variable
              unparseEntityTypeAttr(type, info, false);
            } else
              unparseExpression(*iexp, info);
          } else if (isSgGlobal(parentScope) != nullptr) {
            SgGlobal *globalScope = isSgGlobal(parentScope);
            ASSERT_not_null(globalScope);
            SgDeclarationStatementPtrList &stmtList =
                globalScope->get_declarations();
            if (std::find(stmtList.begin(), stmtList.end(), varDecl) ==
                stmtList.end()) {
              unparseExpression(*iexp, info);
              // third argument has to be false to have the attribute unparsed
              // to individual variable
              unparseEntityTypeAttr(type, info, false);
            } else
              unparseExpression(*iexp, info);
          } else
            unparseExpression(*iexp, info);
        } else
          unparseExpression(*iexp, info);
        iexp++;
        if (iexp != expr_list->get_expressions().end()) {
          curprint(",");
        } else {
          break;
        }
      }
    }

    i++;
    if (i != blockList.end()) {
      curprint(", ");
    }
  }

  unp->cur.insert_newline(1);
}

void FortranCodeGeneration_locatedNode::unparseEnumDeclStmt(
    SgStatement *stmt, SgUnparse_Info &info) {
  SgEnumDeclaration *enumDecl = isSgEnumDeclaration(stmt);
  ASSERT_not_null(enumDecl);
  enumDecl->validate_enumerator_source_ownership();
  if (enumDecl->get_definingDeclaration() != enumDecl) {
    std::cerr << "REX_UNPARSER_INVARIANT[fortran-enum]: statement list "
                 "contains a nondefining ENUM declaration\n";
    ROSE_ABORT();
  }
  SgEnumDeclaration *firstNondef =
      isSgEnumDeclaration(enumDecl->get_firstNondefiningDeclaration());
  if (firstNondef == nullptr ||
      firstNondef->get_definingDeclaration() != enumDecl ||
      firstNondef->get_scope() != enumDecl->get_scope()) {
    std::cerr << "REX_UNPARSER_INVARIANT[fortran-enum]: malformed ENUM "
                 "declaration chain\n";
    ROSE_ABORT();
  }
  if (enumDecl->get_enumerators().empty()) {
    std::cerr << "REX_UNPARSER_INVARIANT[fortran-enum]: ENUM declaration has "
                 "no enumerators\n";
    ROSE_ABORT();
  }

  curprint("ENUM, BIND(C)");
  unp->cur.insert_newline(1);
  for (SgInitializedName *enumerator : enumDecl->get_enumerators()) {
    if (enumerator == nullptr || enumerator->get_parent() != enumDecl ||
        enumerator->get_scope() != enumDecl->get_scope() ||
        enumerator->get_name().is_null()) {
      std::cerr << "REX_UNPARSER_INVARIANT[fortran-enum]: malformed "
                   "enumerator ownership or name\n";
      ROSE_ABORT();
    }
    SgAssignInitializer *initializer =
        isSgAssignInitializer(enumerator->get_initializer());
    SgEnumVal *value = initializer != nullptr
                           ? isSgEnumVal(initializer->get_operand_i())
                           : nullptr;
    SgEnumDeclaration *valueDecl =
        value != nullptr ? value->get_declaration() : nullptr;
    if (initializer == nullptr || value == nullptr || valueDecl == nullptr ||
        valueDecl->get_definingDeclaration() != enumDecl ||
        value->get_name() != enumerator->get_name()) {
      std::cerr << "REX_UNPARSER_INVARIANT[fortran-enum]: enumerator '"
                << enumerator->get_name()
                << "' has no exact semantic integer initializer\n";
      ROSE_ABORT();
    }

    unparseAttachedPreprocessingInfo(enumerator, info,
                                     PreprocessingInfo::before);
    curprint("ENUMERATOR :: ");
    curprint(enumerator->get_name().str());
    curprint(" = ");
    curprint(std::to_string(value->get_value()));
    unparseAttachedPreprocessingInfo(enumerator, info,
                                     PreprocessingInfo::after);
    unp->cur.insert_newline(1);
  }
  unparseAttachedPreprocessingInfo(enumDecl, info, PreprocessingInfo::inside);
  curprint("END ENUM");
  unp->cur.insert_newline(1);
}

void FortranCodeGeneration_locatedNode::unparseVarDeclStmt(
    SgStatement *stmt, SgUnparse_Info &info) {
  // Sage node corresponds to Fortran variable declaration

  SgVariableDeclaration *vardecl = isSgVariableDeclaration(stmt);
  ASSERT_not_null(vardecl);
  requireFortranSourceDeclarationSurface(vardecl);
  if (vardecl->get_variables().empty()) {
    std::cerr << "Error: Fortran variable declaration has no entities"
              << std::endl;
    ROSE_ABORT();
  }
  for (SgInitializedName *variable : vardecl->get_variables()) {
    ASSERT_not_null(variable);
    if (variable->get_name().is_null()) {
      std::cerr << "Error: unnamed entity in Fortran variable declaration"
                << std::endl;
      ROSE_ABORT();
    }
  }

  // In Fortran we should never have to deal with a type declaration
  // inside a variable declaration (e.g. struct A { int x; } a;)
  ROSE_ASSERT(vardecl->get_baseTypeDefiningDeclaration() == nullptr);

  // Build a new SgUnparse_Info object to represent formatting options
  // for this statement
  SgUnparse_Info ninfo(info);

  // FIXME: we may need to do something analagous for modules?
  // Check to see if this is an object defined within a class

  SgName inCname;
  ROSE_ASSERT(vardecl->get_parent());
  SgClassDefinition *cdefn = isSgClassDefinition(vardecl->get_parent());
  if (cdefn) {
    inCname = cdefn->get_declaration()->get_name();
    if (cdefn->get_declaration()->get_class_type() ==
        SgClassDeclaration::e_class)
      ninfo.set_CheckAccess();
  }

  // Save the input information
  SgUnparse_Info saved_ninfo(ninfo);

  // Setup the SgUnparse_Info object for this statement
  ninfo.unset_CheckAccess();
  info.set_access_attribute(ninfo.get_access_attribute());
  SgInitializedNamePtrList::iterator p = vardecl->get_variables().begin();
  unparseVarDecl(vardecl, *p, ninfo);
  ninfo.set_SkipBaseType();
  p++;
  while (p != vardecl->get_variables().end()) {
    curprint(", ");
    unparseVarDecl(vardecl, *p, ninfo);
    p++;
  }

  unp->cur.insert_newline(1);
}

void FortranCodeGeneration_locatedNode::unparseVarDefnStmt(SgStatement *stmt,
                                                           SgUnparse_Info &) {
  ASSERT_not_null(isSgVariableDefinition(stmt));
  std::cerr << "Error: SgVariableDefinition has no Fortran source spelling"
            << std::endl;
  ROSE_ABORT();
}

void FortranCodeGeneration_locatedNode::unparseParamDeclStmt(SgStatement *,
                                                             SgUnparse_Info &) {
  std::cerr << "Error: SgParameterStatement must not reach the Fortran "
               "unparser"
            << std::endl;
  ROSE_ABORT();
}

void FortranCodeGeneration_locatedNode::unparseUseStmt(SgStatement *stmt,
                                                       SgUnparse_Info &) {
  // Sage node corresponds to Fortran use statement

  SgUseStatement *useStmt = isSgUseStatement(stmt);
  ASSERT_not_null(useStmt);
  if (useStmt->get_name().is_null()) {
    std::cerr << "Error: Fortran USE statement has no module name" << std::endl;
    ROSE_ABORT();
  }
  for (SgRenamePair *renamePair : useStmt->get_rename_list()) {
    if (renamePair == nullptr || renamePair->get_use_name().is_null() ||
        (renamePair->isRename() && renamePair->get_local_name().is_null())) {
      std::cerr << "Error: malformed rename entry in Fortran USE statement"
                << std::endl;
      ROSE_ABORT();
    }
  }

  curprint("USE ");

  // Pei-Hung (03/09/21) added unparsing for module nature (intrinsic or
  // non_intrinsic)
  std::string nature = useStmt->get_module_nature();
  if (nature != "") {
    curprint(", " + nature + " :: ");
  }

  curprint(useStmt->get_name().str());

  if (useStmt->get_only_option()) {
    // FMZ: move comma here
    curprint(", ");
    curprint("ONLY : ");
  }

  int listSize = useStmt->get_rename_list().size();
  if (listSize > 0 &&
      !useStmt->get_only_option()) // need to print a comma and a space
    curprint(", ");
  for (int i = 0; i < listSize; i++) {
    SgRenamePair *renamePair = useStmt->get_rename_list()[i];
    ASSERT_not_null(renamePair);

    if (renamePair->isRename()) {
      SgName local_name = renamePair->get_local_name();
      SgName use_name = renamePair->get_use_name();
      curprint(local_name);
      curprint(" => ");
      curprint(use_name);
    } else {
      SgName use_name = renamePair->get_use_name();
      curprint(use_name);
    }

    if (i < listSize - 1)
      curprint(" , ");
  }

  unp->cur.insert_newline(1);
}

//----------------------------------------------------------------------------
//  FortranCodeGeneration_locatedNode::<executable statements, control flow>
//----------------------------------------------------------------------------

void FortranCodeGeneration_locatedNode::unparseBasicBlockStmt(
    SgStatement *bb, SgUnparse_Info &info) {
  SgBasicBlock *block = isSgBasicBlock(bb);
  ASSERT_not_null(block);

  const bool isFortranBlock = block->get_is_fortran_block_construct();
  const std::string blockName = block->get_fortran_block_construct_name();
  if (!isFortranBlock && !blockName.empty()) {
    std::cerr << "REX_UNPARSE_INVARIANT[fortran-block-role]: ordinary basic "
                 "block owns a Fortran construct name\n";
    ROSE_ABORT();
  }
  if (isFortranBlock) {
    SgScopeStatement *semanticScope = block->get_scope();
    SgNode *structuralOwner = block->get_parent();
    const bool directScopeOwner =
        structuralOwner == semanticScope && semanticScope != nullptr &&
        semanticScope != block && semanticScope->statementExistsInScope(block);
    SgOmpBodyStatement *ompOwner = isSgOmpBodyStatement(structuralOwner);
    const bool exactOpenMPBodyOwner =
        ompOwner != nullptr && ompOwner->get_body() == block &&
        semanticScope != nullptr && semanticScope != block &&
        ompOwner->get_parent() == semanticScope &&
        semanticScope->statementExistsInScope(ompOwner);
    if (!directScopeOwner && !exactOpenMPBodyOwner) {
      std::cerr << "REX_UNPARSE_INVARIANT[fortran-block-owner]: Fortran "
                   "BLOCK has no exact lexical owner\n";
      ROSE_ABORT();
    }
    if (!blockName.empty()) {
      curprint(blockName + ": ");
    }
    curprint("BLOCK");
    unp->cur.insert_newline(1);
  }

  // space here is required to get "else if" blocks formatted correctly (at
  // least).
  unp->cur.format(block, info, FORMAT_BEFORE_BASIC_BLOCK1);

  for (auto stmt : block->get_statements()) {
    ASSERT_not_null(stmt);
    if (auto *varDecl = isSgVariableDeclaration(stmt)) {
      requireFortranSourceDeclarationSurface(varDecl);
    }
    // FMZ: for module file, only output the variable declarations (not
    // definitions) Pei-Hung (05/23/2019) Need to add SgUseStatement,
    // SgimplicitStatement and SgDerivedTypeStatement into rmod file
    if (!info.outputFortranModFile() ||
        stmt->variantT() == V_SgVariableDeclaration ||
        stmt->variantT() ==
            V_SgAttributeSpecificationStatement // DXN (02/07/2012): unparse
                                                // attribute statements also
        || stmt->variantT() == V_SgUseStatement ||
        stmt->variantT() == V_SgImplicitStatement ||
        stmt->variantT() == V_SgDerivedTypeStatement) {
      unparseStatement(stmt, info);
    }
  }

  // Liao (10/14/2010): This helps handle cases such as
  //    c$OMP END PARALLEL
  //          END
  unparseAttachedPreprocessingInfo(block, info, PreprocessingInfo::inside);

  if (isFortranBlock) {
    unparseStatementNumbersSupport(block->get_fortran_block_end_numeric_label(),
                                   info);
    curprint("END BLOCK");
    if (!blockName.empty()) {
      curprint(" " + blockName);
    }
    unp->cur.insert_newline(1);
  }
}

bool hasCStyleElseIfConstruction(SgIfStmt *parentIfStatement) {
  // Rasmussen(7/17/2018): Check for C style AST else-if construction.
  // The C style else-if AST doesn't have an SgBasicBlock immediately
  // preceding the SgIfStmt representing the else-if clause; the SgIfStmt
  // itself is the false branch.
  //
  SgIfStmt *else_if_stmt = isSgIfStmt(parentIfStatement->get_false_body());

  return (else_if_stmt != nullptr);
}

SgIfStmt *getElseIfStatement(SgIfStmt *parentIfStatement) {
  // This returns the elseif statement in a SgIfStmt object, else returns NULL.

  SgIfStmt *childIfStatement{nullptr};

  SgBasicBlock *falseBlock =
      isSgBasicBlock(parentIfStatement->get_false_body());

  // Rasmussen (7/23/2018): Simplification of logic allowed because usage of
  // is_else_if_statement was fixed in frontend.  Previously
  // is_else_if_statement was not used in the unparser and this confused users
  // at NCAR when attempting transformations.
  //
  if (falseBlock != nullptr) {
    if (falseBlock->get_statements().empty() == false) {
      childIfStatement = isSgIfStmt(*(falseBlock->get_statements().begin()));
      if (childIfStatement != nullptr) {
        if (childIfStatement->get_is_else_if_statement() == false) {
          childIfStatement = nullptr;
        }
      }
    }
  }

  // Rasmussen (7/23/2018): This branch added for the experimental Fortran
  // parser where the AST was designed to follow the C if statement.  For the
  // new design there is no SgBasicBlock immediately preceding the SgIfStmt (the
  // SgIfStmt is the false branch).
  //
  SgIfStmt *else_if_stmt = isSgIfStmt(parentIfStatement->get_false_body());
  if (else_if_stmt != nullptr) {
    childIfStatement = else_if_stmt;
  }

  return childIfStatement;
}

void FortranCodeGeneration_locatedNode::unparseIfStmt(SgStatement *stmt,
                                                      SgUnparse_Info &info) {
  // Sage node corresponding to Fortran 'if'
  //

  // Rasmussen(7/23/2018): Modified much of the unparsing of if statements and
  // if constructs because:
  //   1. The AST was modified to follow the C-style AST for the experimental
  //   branch.
  //   2. Original Fortran frontend AST construction also moved towards C by
  //   using NULL for false body.
  //   3. ELSE was not unparsed if the else-block was empty (no reason not to
  //   unparse it).
  //   4. A bug was fixed in unparsing the if-construct label name
  //   (label_string).
  //
  SgIfStmt *if_stmt = isSgIfStmt(stmt);
  ASSERT_not_null(if_stmt);
  ROSE_ASSERT(if_stmt->get_conditional());
  ROSE_ASSERT(if_stmt->get_true_body());

  // Output the if-construct-name string (if present) if this is an else-if
  // branch
  if (if_stmt->get_string_label().empty() == false &&
      if_stmt->get_is_else_if_statement() == false) {
    curprint(if_stmt->get_string_label() + ": ");
  }

  // IF keyword and conditional
  curprint("IF (");
  info.set_inConditional();

  // DQ (8/15/2007): In C the condition is a statement, and in Fortran the
  // condition is an expression! We might want to fix this by having an IR node
  // to represent the Fortran "if" statement.
  SgExprStatement *expressionStatement =
      isSgExprStatement(if_stmt->get_conditional());
  unparseExpression(expressionStatement->get_expression(), info);

  info.unset_inConditional();
  curprint(") ");

  // DQ (12/26/2007): handling cases where endif is not in the source code and
  // not required (stmt vs. construct) This is also (primarily) used to keep
  // else-if-stmt from printing extra END IF.
  bool output_endif = if_stmt->get_has_end_statement();

  SgIfStmt *elseIfStatement = getElseIfStatement(if_stmt);

  // THEN keyword
  // DQ (12/26/2007): If this is an elseif statement then output the "THEN" even
  // though we will not output an "ENDIF"
  if (output_endif) {
    // IF THEN statement branch
    //
    ROSE_ASSERT(if_stmt->get_use_then_keyword());
    // This branch taken for an if-then-stmt.
    // Note that the string label if output before "IF", not after "THEN"
    curprint("THEN");
    unp->cur.insert_newline(1);
    unparseStatement(if_stmt->get_true_body(), info);
  } else {
    if (if_stmt->get_use_then_keyword()) {
      // ELSE IF statement branch (uses if_stmt to unparse the "IF")
      //
      // This branch taken for an else-if-stmt.
      // Note that the string label if output after "THEN"
      curprint("THEN");
      // Output the if-construct-name string after THEN if needed
      if (if_stmt->get_string_label().empty() == false)
        curprint(" " + if_stmt->get_string_label());
      unp->cur.insert_newline(1);
      unparseStatement(if_stmt->get_true_body(), info);
    } else {
      // IF statement branch (not if-construct)
      //
      // "THEN" is not output for the case of "IF (C) B = 0"
      ROSE_ASSERT(isSgBasicBlock(if_stmt->get_true_body()));
      SgStatementPtrList &statementList =
          isSgBasicBlock(if_stmt->get_true_body())->get_statements();
      ROSE_ASSERT(statementList.size() == 1);
      SgStatement *statement = *(statementList.begin());
      ASSERT_not_null(statement);

      // Fixed format code includes a call to insert 6 spaces (or numeric label
      // if available), we want to suppress this.
      SgUnparse_Info info_without_formating(info);
      info_without_formating.set_SkipFormatting();
      unparseStatement(statement, info_without_formating);
    }
  }

  // ELSE and ELSE IF statements
  //
  // Rasmussen(7/23/2018): Added unparsing of C style else-if AST construction.
  // The C AST does not use an SgBasicBlock to precede an SgIfStmt representing
  // the else-if-stmt. Also simplified the logic somewhat, now allowed because
  // the false_body is NULL if no else-stmt (changed in the frontend to follow C
  // AST).
  //
  SgBasicBlock *fbb = isSgBasicBlock(if_stmt->get_false_body());

  if (fbb || hasCStyleElseIfConstruction(if_stmt)) {
    // The else statement might have its own numeric label
    unparseStatementNumbersSupport(if_stmt->get_else_numeric_label(), info);
    curprint("ELSE");

    // However, currently there is no information on else if-construct name in
    // SgIfStmt so we won't try to unparse it.  NOTE, output could be different
    // from input.

    if (elseIfStatement != nullptr) {
      // ELSE IF statement branch
      ROSE_ASSERT(elseIfStatement->get_is_else_if_statement());

      // Call the associated unparse function directly to avoid formatting
      curprint(" ");
      unparseIfStmt(elseIfStatement, info);
    } else {
      // ELSE statement branch
      unparseStatement(if_stmt->get_false_body(), info);
    }
  }

  // END IF statement
  //
  if (output_endif) {
    unparseStatementNumbersSupport(if_stmt->get_end_numeric_label(), info);
    curprint("END IF");
    // Output the if-construct-name string if present
    if (if_stmt->get_string_label().empty() == false)
      curprint(" " + if_stmt->get_string_label());
  }

  ASSERT_not_null(unp);
  unp->cur.insert_newline(1);
}

void FortranCodeGeneration_locatedNode::unparseForAllStatement(
    SgStatement *stmt, SgUnparse_Info &info) {
  SgForAllStatement *forAllStatement = isSgForAllStatement(stmt);
  ASSERT_not_null(forAllStatement);

  // The FORALL statement has been deprecated and replaced by a DO CONCURRENT
  // construct. Since they are very similar they share the same Sage node and
  // are distinguished by an enum.
  //
  if (forAllStatement->get_forall_statement_kind() ==
      SgForAllStatement::e_do_concurrent_statement) {
    unparseDoConcurrentStatement(stmt, info);
    return;
  }

  // Note the return in the preceding for DO CONCURRENT.  What follows unparses
  // a FORALL construct.
  //
  ROSE_ASSERT(forAllStatement->get_forall_statement_kind() ==
              SgForAllStatement::e_forall_statement);

  SgExprListExp *forAllHeader = forAllStatement->get_forall_header();
  ASSERT_not_null(forAllHeader);

  curprint("FORALL ( ");
  unparseExpression(forAllHeader, info);
  curprint(" ) ");

  SgStatement *statement{nullptr};
  if (forAllStatement->get_has_end_statement()) {
    statement = forAllStatement->get_body();
    ASSERT_not_null(statement);

    unparseStatement(statement, info);
  } else {
    SgBasicBlock *body = isSgBasicBlock(forAllStatement->get_body());
    ASSERT_not_null(body);

    SgStatementPtrList &statementList = body->get_statements();
    ROSE_ASSERT(statementList.size() == 1);
    statement = *(statementList.begin());
    ASSERT_not_null(statement);

    unparseLanguageSpecificStatement(statement, info);
  }

  unp->cur.insert_newline(1);

  if (forAllStatement->get_has_end_statement()) {
    unparseStatementNumbersSupport(forAllStatement->get_end_numeric_label(),
                                   info);
    curprint("END FORALL");
  }

  unp->cur.insert_newline(1);
}

void FortranCodeGeneration_locatedNode::unparseDoConcurrentStatement(
    SgStatement *stmt, SgUnparse_Info &info) {
  SgForAllStatement *forAllStatement = isSgForAllStatement(stmt);
  ASSERT_not_null(forAllStatement);

  SgExprListExp *forAllHeader = forAllStatement->get_forall_header();
  ASSERT_not_null(forAllHeader);

  if (forAllStatement->get_string_label().empty() == false) {
    curprint(forAllStatement->get_string_label() + ": ");
  }

  SgExpressionPtrList header = forAllHeader->get_expressions();
  SgExpression *mask = SageInterface::forallMaskExpression(forAllStatement);
  int num_vars = header.size();
  if (mask != nullptr && num_vars > 0) {
    num_vars -= 1;
  }

  curprint("DO ");
  if (forAllStatement->get_end_numeric_label() != nullptr) {
    SgLabelSymbol *endLabelSymbol =
        forAllStatement->get_end_numeric_label()->get_symbol();
    ASSERT_not_null(endLabelSymbol->get_fortran_statement());
    int loopEndLabel = endLabelSymbol->get_numeric_label_value();
    string numeric_label_string = StringUtility::numberToString(loopEndLabel);
    curprint(numeric_label_string + " ");
  }
  curprint("CONCURRENT (");

  for (int i = 0; i < num_vars; i++) {
    if (i != 0)
      curprint(", ");

    SgAssignOp *assignOp = isSgAssignOp(header[i]);
    ROSE_ASSERT(assignOp);

    unparseExpression(assignOp->get_lhs_operand_i(), info);
    curprint("=");
    unparseExpression(assignOp->get_rhs_operand_i(), info);
  }

  if (mask != nullptr) {
    curprint(", ");
    unparseExpression(mask, info);
  }

  curprint(")");
  unp->cur.insert_newline(1);

  // Unparse the body
  SgStatement *statement{nullptr};
  if (forAllStatement->get_has_end_statement()) {
    statement = forAllStatement->get_body();
    ASSERT_not_null(statement);

    unparseStatement(statement, info);
  } else {
    SgBasicBlock *body = isSgBasicBlock(forAllStatement->get_body());
    ASSERT_not_null(body);

    SgStatementPtrList &statementList = body->get_statements();
    ROSE_ASSERT(statementList.size() == 1);
    statement = *(statementList.begin());
    ASSERT_not_null(statement);

    unparseLanguageSpecificStatement(statement, info);
  }

  unp->cur.insert_newline(1);

  // Unparse the end statement
  if (forAllStatement->get_has_end_statement()) {
    unparseStatementNumbersSupport(forAllStatement->get_end_numeric_label(),
                                   info);
    curprint("END DO");
    if (forAllStatement->get_string_label().empty() == false) {
      curprint(" " + forAllStatement->get_string_label());
    }
    unp->cur.insert_newline(1);
  }
}

void FortranCodeGeneration_locatedNode::unparseDoStmt(SgStatement *stmt,
                                                      SgUnparse_Info &info) {
  // Sage node corresponds to Fortran 'do'

  // This is a Fortran specific IR node and it stores its condition and
  // increment differently (since Fortran uses only values to represent the
  // bound and the stride instead of expressions that include the index
  // variable).

  SgFortranDo *doloop = isSgFortranDo(stmt);
  ASSERT_not_null(doloop);

  // NOTE: for now we are responsible for unparsing the
  // initialization, condition and update expressions into a triplet.
  // We assume that these statements are of a very restricted form.
  SgExpression *initExp = doloop->get_initialization();
  ASSERT_not_null(initExp);

  SgExpression *condExp = doloop->get_bound();
  ASSERT_not_null(condExp);

  SgExpression *updateExp = doloop->get_increment();
  ASSERT_not_null(updateExp);

  SgStatement *body = doloop->get_body();
  ASSERT_not_null(body);
  if (!doloop->get_has_end_statement()) {
    if (doloop->get_end_numeric_label() == nullptr) {
      std::cerr << "Error: Fortran DO has neither END DO nor a terminal label"
                << std::endl;
      ROSE_ABORT();
    }
    SgLabelSymbol *endLabelSymbol =
        doloop->get_end_numeric_label()->get_symbol();
    SgStatement *endLabelStmt = endLabelSymbol != nullptr
                                    ? endLabelSymbol->get_fortran_statement()
                                    : nullptr;
    if (endLabelStmt == nullptr || endLabelStmt == doloop ||
        isSgNullStatement(endLabelStmt) != nullptr) {
      std::cerr << "Error: Fortran labeled DO has no resolved terminal "
                   "statement"
                << std::endl;
      ROSE_ABORT();
    }
  }

  if (doloop->get_string_label().empty() == false) {
    // Output the string label
    curprint(doloop->get_string_label() + ": ");
  }

  curprint("DO ");

  if (doloop->get_end_numeric_label() != nullptr) {
    SgLabelSymbol *endLabelSymbol =
        doloop->get_end_numeric_label()->get_symbol();
    ASSERT_not_null(endLabelSymbol);

    ASSERT_not_null(endLabelSymbol->get_fortran_statement());
    int loopEndLabel = endLabelSymbol->get_numeric_label_value();
    string numeric_label_string = StringUtility::numberToString(loopEndLabel);
    curprint(numeric_label_string + " ");
  }

  unparseExpression(initExp, info);
  if (isSgNullExpression(initExp) == nullptr) {
    curprint(", ");
    unparseExpression(condExp, info);
  }

  // If this is NOT a SgNullExpression, then output the "," and the stride
  // expression.
  if (isSgNullExpression(updateExp) == nullptr) {
    curprint(", ");
    unparseExpression(updateExp, info);
  }

  // loop body (must always exist)
  unparseStatement(body, info);

  ASSERT_not_null(unp);
  unp->cur.insert_newline(1);

  // This setting converts all non-block where statements into blocked where
  // statements. So "DO I=1,2 B = 0" becomes: "DO I=1,2
  //     B = 0
  //  END DO"

  // DQ (12/26/2007): handling cases where enddo is not in the source code and
  // not required (stmt vs. construct)
  if (doloop->get_has_end_statement()) {
    unparseStatementNumbersSupport(doloop->get_end_numeric_label(), info);

    curprint("END DO");
    if (doloop->get_string_label().empty() == false) {
      // Output the string label
      curprint(" " + doloop->get_string_label());
    }
  }

  ASSERT_not_null(unp);
  unp->cur.insert_newline(1);
}

void FortranCodeGeneration_locatedNode::unparseWhileStmt(SgStatement *stmt,
                                                         SgUnparse_Info &info) {
  // Sage node corresponds to Fortran 'DO WHILE' (pre-test)

  SgWhileStmt *while_stmt = isSgWhileStmt(stmt);
  ASSERT_not_null(while_stmt);

  if (while_stmt->get_string_label().empty() == false) {
    // Output the string label
    curprint(while_stmt->get_string_label() + ": ");
  }

  curprint_keyword("DO", info);
  curprint(" ");

  if (while_stmt->get_end_numeric_label() != nullptr) {
    SgLabelSymbol *endLabelSymbol =
        while_stmt->get_end_numeric_label()->get_symbol();

    ASSERT_not_null(endLabelSymbol->get_fortran_statement());
    int loopEndLabel = endLabelSymbol->get_numeric_label_value();
    string numeric_label_string = StringUtility::numberToString(loopEndLabel);
    curprint(numeric_label_string + " ");
  }

  curprint_keyword("WHILE", info);
  curprint(" (");
  info.set_inConditional(); // prevent printing line and file info

  SgExprStatement *conditionStatement =
      isSgExprStatement(while_stmt->get_condition());
  ASSERT_not_null(conditionStatement);
  unparseExpression(conditionStatement->get_expression(), info);
  info.unset_inConditional();
  curprint(")");

  // loop body (must always exist)
  unparseStatement(while_stmt->get_body(), info);

  // This setting converts all non-block where statements into blocked where
  // statements. So "DO WHILE (A) B = 0" becomes: "DO WHILE (A)
  //     B = 0
  //  END DO"
  bool output_enddo = while_stmt->get_has_end_statement();
  if (output_enddo == false && while_stmt->get_end_numeric_label() != nullptr) {
    SgLabelSymbol *endLabelSymbol =
        while_stmt->get_end_numeric_label()->get_symbol();
    SgStatement *endLabelStmt = endLabelSymbol != nullptr
                                    ? endLabelSymbol->get_fortran_statement()
                                    : nullptr;
    if (endLabelStmt == nullptr || endLabelStmt == while_stmt ||
        isSgNullStatement(endLabelStmt) != nullptr) {
      output_enddo = true;
    }
  }

  if (output_enddo) {
    unparseStatementNumbersSupport(while_stmt->get_end_numeric_label(), info);
    curprint_keyword("END", info);
    curprint(" ");
    curprint_keyword("DO", info);

    if (while_stmt->get_string_label().empty() == false) {
      // Output the string label
      curprint(" " + while_stmt->get_string_label());
    }
  }

  ASSERT_not_null(unp);
  unp->cur.insert_newline(1);
}

void FortranCodeGeneration_locatedNode::unparseSwitchStmt(
    SgStatement *stmt, SgUnparse_Info &info) {
  // Sage node corresponds to Fortran 'select'
  SgSwitchStatement *switch_stmt = isSgSwitchStatement(stmt);
  ASSERT_not_null(switch_stmt);
  ASSERT_not_null(switch_stmt->get_body());

  if (switch_stmt->get_string_label().empty() == false) {
    // Output the string label
    curprint(switch_stmt->get_string_label() + ": ");
  }

  curprint("SELECT CASE(");

  SgExprStatement *expressionStatement =
      isSgExprStatement(switch_stmt->get_item_selector());
  ASSERT_not_null(expressionStatement);
  unparseExpression(expressionStatement->get_expression(), info);
  curprint(")");

  unparseStatement(switch_stmt->get_body(), info);

  unparseStatementNumbersSupport(switch_stmt->get_end_numeric_label(), info);

  curprint("END SELECT");

  if (switch_stmt->get_string_label().empty() == false) {
    // Output the string label
    curprint(" " + switch_stmt->get_string_label());
  }

  ASSERT_not_null(unp);
  unp->cur.insert_newline(1);
}

void FortranCodeGeneration_locatedNode::unparseCaseStmt(SgStatement *stmt,
                                                        SgUnparse_Info &info) {
  // Sage node corresponds to Fortran 'case'
  SgCaseOptionStmt *case_stmt = isSgCaseOptionStmt(stmt);
  ASSERT_not_null(case_stmt);
  ASSERT_not_null(case_stmt->get_key());
  ASSERT_not_null(case_stmt->get_body());

  curprint("CASE (");
  unparseExpression(case_stmt->get_key(), info);
  curprint(")");

  if (case_stmt->get_case_construct_name().empty() == false) {
    // Output the string case construct name
    curprint(" " + case_stmt->get_case_construct_name());
  }

  unparseStatement(case_stmt->get_body(), info);
}

void FortranCodeGeneration_locatedNode::unparseDefaultStmt(
    SgStatement *stmt, SgUnparse_Info &info) {
  // Sage node corresponds to Fortran 'case default'
  SgDefaultOptionStmt *default_stmt = isSgDefaultOptionStmt(stmt);
  ASSERT_not_null(default_stmt);
  ASSERT_not_null(default_stmt->get_body());

  curprint("CASE DEFAULT");

  if (default_stmt->get_default_construct_name().empty() == false) {
    // Output the string default construct name
    curprint(" " + default_stmt->get_default_construct_name());
  }

  unparseStatement(default_stmt->get_body(), info);
}

void FortranCodeGeneration_locatedNode::unparseBreakStmt(SgStatement *stmt,
                                                         SgUnparse_Info &) {
  // This IR node corresponds to the Fortran 'exit'
  SgBreakStmt *break_stmt = isSgBreakStmt(stmt);
  ASSERT_not_null(break_stmt);
  curprint("EXIT");

  // If this is for a named do loop, this is the optional name.
  if (break_stmt->get_do_string_label().empty() == false) {
    curprint(" " + break_stmt->get_do_string_label());
  }
  unp->cur.insert_newline(1);
}

void FortranCodeGeneration_locatedNode::unparseContinueStmt(
    SgContinueStmt *continueStmt, SgUnparse_Info &info) {
  // This IR node corresponds to a Fortran 'CYCLE' statement,
  // because semantically the same as a C/C++ continue statement.
  curprint_keyword("CYCLE", info);

  // If this is for a named do loop, this is the optional name.
  if (continueStmt->get_do_string_label().empty() == false) {
    curprint(" " + continueStmt->get_do_string_label());
  }

  unp->cur.insert_newline(1);
}

void FortranCodeGeneration_locatedNode::unparseFortranContinueStmt(
    SgFortranContinueStmt *continueStmt, SgUnparse_Info &info) {
  curprint_keyword("CONTINUE", info);
  unp->cur.insert_newline(1);
}

void FortranCodeGeneration_locatedNode::unparseLabelStmt(
    SgLabelStatement *labelStmt, SgUnparse_Info &info) {
  if (flangParser) {
    // The SgLabelStatement is used for the label
    const std::string label = labelStmt->get_label().getString();
    if (!label.empty()) {
      const bool fixedFormat = isFixedFortranOutput(unp);
      if (fixedFormat) {
        std::string labelText = label + " ";
        while (labelText.size() < 6) {
          labelText.insert(labelText.begin(), ' ');
        }
        curprint(labelText);
      } else {
        curprint(label + " ");
      }
    } else {
      if (labelStmt->get_numeric_label() == nullptr) {
        std::cerr << "Error: SgLabelStatement has neither a string nor a "
                     "numeric Fortran label"
                  << std::endl;
        ROSE_ABORT();
      }
      unparseStatementNumbersSupport(labelStmt->get_numeric_label(), info);
    }
    if (labelStmt->get_statement() != nullptr) {
      unparseLanguageSpecificStatement(labelStmt->get_statement(), info);
    } else {
      std::cerr << "Error: SgLabelStatement has no labeled statement"
                << std::endl;
      ROSE_ABORT();
    }
  } else {
    std::cerr << "Error: legacy SgLabelStatement lacks structured Fortran "
                 "label ownership"
              << std::endl;
    ROSE_ABORT();
  }
}

void FortranCodeGeneration_locatedNode::unparseGotoStmt(
    SgGotoStatement *gotoStmt, SgUnparse_Info &info) {
  ASSERT_not_null(gotoStmt);
  curprint_keyword("GOTO", info);
  curprint(" ");

  // The Flang parser uses an SgLabelStatement for a statement label, at this
  // point it has already been printed.  Printing the goto label is simple, just
  // print it and return.
  if (gotoStmt->get_label()) {
    // Flang unparser
    curprint(gotoStmt->get_label()->get_label());
    unp->cur.insert_newline(1);
    return;
  }

  // Old OFP parser uses numeric label handling which is different than C/C++.
  ASSERT_not_null(gotoStmt->get_label_expression());
  SgLabelSymbol *labelSymbol = gotoStmt->get_label_expression()->get_symbol();

  ASSERT_not_null(labelSymbol);

  // Every numeric label should be associated with a statement
  ASSERT_not_null(labelSymbol->get_fortran_statement());
  int numeric_label = labelSymbol->get_numeric_label_value();

  ASSERT_require(numeric_label >= 0);
  string numeric_label_string = StringUtility::numberToString(numeric_label);
  curprint(numeric_label_string);

  unp->cur.insert_newline(1);
}

void FortranCodeGeneration_locatedNode::unparseProcessControlStmt(
    SgStatement *stmt, SgUnparse_Info &info) {
  SgProcessControlStatement *ctrl_stmt = isSgProcessControlStatement(stmt);
  ASSERT_not_null(ctrl_stmt);

  SgExpression *quiet_expr = ctrl_stmt->get_quiet();
  SgProcessControlStatement::control_enum kind = ctrl_stmt->get_control_kind();

  switch (kind) {
  case SgProcessControlStatement::e_stop: {
    curprint_keyword("STOP", info);
    curprint(" ");
    unparseExpression(ctrl_stmt->get_code(), info);
    // F2018 syntax
    if (quiet_expr && !isSgNullExpression(quiet_expr)) {
      curprint(", ");
      curprint_keyword("QUIET", info);
      curprint("=");
      unparseExpression(quiet_expr, info);
    }
    break;
  }
  case SgProcessControlStatement::e_error_stop: {
    curprint_keyword("ERROR", info);
    curprint(" ");
    curprint_keyword("STOP", info);
    curprint(" ");
    unparseExpression(ctrl_stmt->get_code(), info);
    // F2018 syntax
    if (quiet_expr && !isSgNullExpression(quiet_expr)) {
      curprint(", ");
      curprint_keyword("QUIET", info);
      curprint("=");
      unparseExpression(quiet_expr, info);
    }
    break;
  }
  case SgProcessControlStatement::e_fail_image: {
    curprint_keyword("FAIL", info);
    curprint(" ");
    curprint_keyword("IMAGE", info);
    unparseExpression(ctrl_stmt->get_code(), info);
    break;
  }
  case SgProcessControlStatement::e_pause: {
    curprint("PAUSE ");
    unparseExpression(ctrl_stmt->get_code(), info);
    break;
  }
  default: {
    cerr << "error: unparseProcessControlStatement() is unimplemented for enum "
            "value "
         << kind << "\n";
    ROSE_ABORT();
  }
  }

  unp->cur.insert_newline(1);
}

void FortranCodeGeneration_locatedNode::unparseReturnStmt(
    SgStatement *stmt, SgUnparse_Info &info) {
  // This IR node is the same for C and Fortran
  SgReturnStmt *return_stmt = isSgReturnStmt(stmt);
  ASSERT_not_null(return_stmt);

  curprint("RETURN");

  // The expression can only be a scalar integer for an alternate return
  SgExpression *altret = return_stmt->get_expression();
  ASSERT_not_null(altret);

  if (isSgNullExpression(altret) == nullptr) {
    // ROSE_ASSERT(isSgValueExp(altret));
    curprint(" ");
    unparseExpression(altret, info);
  }

  unp->cur.insert_newline(1);
}

//----------------------------------------------------------------------------
//  void FortranCodeGeneration_locatedNode::<executable statements, IO>
//----------------------------------------------------------------------------

void FortranCodeGeneration_locatedNode::unparsePrintStatement(
    SgStatement *stmt, SgUnparse_Info &info) {
  // Sage node corresponds to Fortran input/output statement
  SgPrintStatement *printStatement = isSgPrintStatement(stmt);
  ASSERT_not_null(printStatement);

  curprint("PRINT ");

  SgExprListExp *iolist = printStatement->get_io_stmt_list();
  const bool has_items =
      iolist != nullptr && !iolist->get_expressions().empty();

  SgExpression *fmt = printStatement->get_format();
  if (fmt == nullptr) {
    fprintf(stderr,
            "REX_UNPARSE_INVARIANT[fortran-print-format]: PRINT statement "
            "has no exact format expression\n");
    ROSE_ABORT();
  }
  unparseExpression(fmt, info);
  if (has_items) {
    curprint(", ");
  }

  if (has_items) {
    unparseExprList(iolist, info);
  }

  unp->cur.insert_newline(1);
}

bool FortranCodeGeneration_locatedNode::unparse_IO_Support(
    SgStatement *stmt, bool skipUnit, SgUnparse_Info &info) {
  // Sage node corresponds to Fortran IO control info
  SgIOStatement *io_stmt = isSgIOStatement(stmt);
  ASSERT_not_null(io_stmt);

  bool isLeadingEntry = false;
  if (skipUnit == false) {
    // Some Fortran compilers require omitting the "UNIT=" keyword for WRITE.
    // See test2010_144.f90 for an example of this behavior.
    bool skipOutputOfUnitString = (isSgWriteStatement(stmt) != nullptr);
    if (skipOutputOfUnitString == false) {
      curprint("UNIT=");
    }

    if (io_stmt->get_unit() != nullptr) {
      unparseExpression(io_stmt->get_unit(), info);
    } else {
      curprint("*");
    }
  } else {
    isLeadingEntry = true;
  }

  unparse_IO_Control_Support("IOSTAT", io_stmt->get_iostat(), isLeadingEntry,
                             info);
  isLeadingEntry = isLeadingEntry && (io_stmt->get_iostat() == nullptr);

  unparse_IO_Control_Support("ERR", io_stmt->get_err(), isLeadingEntry, info);
  isLeadingEntry = isLeadingEntry && (io_stmt->get_err() == nullptr);

  unparse_IO_Control_Support("IOMSG", io_stmt->get_iomsg(), isLeadingEntry,
                             info);
  isLeadingEntry = isLeadingEntry && (io_stmt->get_iomsg() == nullptr);

  return isLeadingEntry;
}

void FortranCodeGeneration_locatedNode::unparse_IO_Control_Support(
    string name, SgExpression *expr, bool isLeadingEntry,
    SgUnparse_Info &info) {
  if (expr != nullptr) {
    if (isLeadingEntry == false)
      curprint(", ");

    curprint(name);
    curprint("=");
    unparseExpression(expr, info);
  }
}

void FortranCodeGeneration_locatedNode::unparseReadStatement(
    SgStatement *stmt, SgUnparse_Info &info) {
  // Sage node corresponds to Fortran input/output statement
  SgReadStatement *readStatement = isSgReadStatement(stmt);
  ASSERT_not_null(readStatement);

  curprint("READ ");

  SgExprListExp *iolist = readStatement->get_io_stmt_list();

  // If only "READ 1,A" then this is using the format label "1" which is an
  // alternative form of the read statement. In this case the unit is not
  // specified.
  if (readStatement->get_format() != nullptr &&
      readStatement->get_unit() == nullptr) {
    unparseExpression(readStatement->get_format(), info);
    if (iolist->get_expressions().empty() == false) {
      curprint(",");
    }
  } else {
    curprint("(");
    unparse_IO_Support(readStatement, false, info);

    // Added missing items to the io-control-spec-list [Rasmussen, 2019.05.31]

    unparse_IO_Control_Support("FMT", readStatement->get_format(), false, info);
    if (!readStatement->get_namelist().empty()) {
      curprint(", NML=");
      curprint(readStatement->get_namelist());
    }
    unparse_IO_Control_Support("ADVANCE", readStatement->get_advance(), false,
                               info);
    unparse_IO_Control_Support("ASYNCHRONOUS",
                               readStatement->get_asynchronous(), false, info);
    unparse_IO_Control_Support("BLANK", readStatement->get_blank(), false,
                               info);
    unparse_IO_Control_Support("DECIMAL", readStatement->get_decimal(), false,
                               info);
    unparse_IO_Control_Support("DELIM", readStatement->get_delim(), false,
                               info);
    unparse_IO_Control_Support("END", readStatement->get_end(), false, info);
    unparse_IO_Control_Support("EOR", readStatement->get_eor(), false, info);
    unparse_IO_Control_Support("ID", readStatement->get_id(), false, info);
    unparse_IO_Control_Support("PAD", readStatement->get_pad(), false, info);
    unparse_IO_Control_Support("POS", readStatement->get_pos(), false, info);
    unparse_IO_Control_Support("REC", readStatement->get_rec(), false, info);
    unparse_IO_Control_Support("ROUND", readStatement->get_round(), false,
                               info);
    unparse_IO_Control_Support("SIGN", readStatement->get_sign(), false, info);
    unparse_IO_Control_Support("SIZE", readStatement->get_size(), false, info);

    curprint(") ");
  }

  unparseExprList(iolist, info);

  unp->cur.insert_newline(1);
}

void FortranCodeGeneration_locatedNode::unparseWriteStatement(
    SgStatement *stmt, SgUnparse_Info &info) {
  // Sage node corresponds to Fortran input/output statement
  SgWriteStatement *writeStatement = isSgWriteStatement(stmt);
  ASSERT_not_null(writeStatement);

  curprint("WRITE (");

  unparse_IO_Support(stmt, false, info);

  unparse_IO_Control_Support("FMT", writeStatement->get_format(), false, info);
  if (!writeStatement->get_namelist().empty()) {
    curprint(", NML=");
    curprint(writeStatement->get_namelist());
  }
  unparse_IO_Control_Support("ADVANCE", writeStatement->get_advance(), false,
                             info);
  unparse_IO_Control_Support("ASYNCHRONOUS", writeStatement->get_asynchronous(),
                             false, info);
  unparse_IO_Control_Support("BLANK", writeStatement->get_blank(), false, info);
  unparse_IO_Control_Support("DECIMAL", writeStatement->get_decimal(), false,
                             info);
  unparse_IO_Control_Support("DELIM", writeStatement->get_delim(), false, info);
  unparse_IO_Control_Support("END", writeStatement->get_end(), false, info);
  unparse_IO_Control_Support("EOR", writeStatement->get_eor(), false, info);
  unparse_IO_Control_Support("ID", writeStatement->get_id(), false, info);
  unparse_IO_Control_Support("PAD", writeStatement->get_pad(), false, info);
  unparse_IO_Control_Support("POS", writeStatement->get_pos(), false, info);
  unparse_IO_Control_Support("REC", writeStatement->get_rec(), false, info);
  unparse_IO_Control_Support("ROUND", writeStatement->get_round(), false, info);
  unparse_IO_Control_Support("SIGN", writeStatement->get_sign(), false, info);
  unparse_IO_Control_Support("SIZE", writeStatement->get_size(), false, info);

  curprint(") ");

  SgExprListExp *iolist = writeStatement->get_io_stmt_list();

  unparseExprList(iolist, info);

  unp->cur.insert_newline(1);
}

void FortranCodeGeneration_locatedNode::unparseOpenStatement(
    SgStatement *stmt, SgUnparse_Info &info) {
  // Sage node corresponds to Fortran input/output statement
  SgOpenStatement *openStatement = isSgOpenStatement(stmt);
  ASSERT_not_null(openStatement);

  curprint("OPEN (");

  unparse_IO_Support(stmt, false, info);

  unparse_IO_Control_Support("FILE", openStatement->get_file(), false, info);
  unparse_IO_Control_Support("STATUS", openStatement->get_status(), false,
                             info);
  unparse_IO_Control_Support("ACCESS", openStatement->get_access(), false,
                             info);
  unparse_IO_Control_Support("FORM", openStatement->get_form(), false, info);
  unparse_IO_Control_Support("RECL", openStatement->get_recl(), false, info);
  unparse_IO_Control_Support("BLANK", openStatement->get_blank(), false, info);

  // F90 specific
  unparse_IO_Control_Support("POSITION", openStatement->get_position(), false,
                             info);
  unparse_IO_Control_Support("ACTION", openStatement->get_action(), false,
                             info);
  unparse_IO_Control_Support("DELIM", openStatement->get_delim(), false, info);
  unparse_IO_Control_Support("PAD", openStatement->get_pad(), false, info);

  // F2003 specific
  unparse_IO_Control_Support("ASYNCHRONOUS", openStatement->get_asynchronous(),
                             false, info);

  curprint(") ");

  unp->cur.insert_newline(1);
}

void FortranCodeGeneration_locatedNode::unparseCloseStatement(
    SgStatement *stmt, SgUnparse_Info &info) {
  // Sage node corresponds to Fortran input/output statement
  SgCloseStatement *closeStatement = isSgCloseStatement(stmt);
  ASSERT_not_null(closeStatement);

  curprint("CLOSE (");

  unparse_IO_Support(stmt, false, info);

  unparse_IO_Control_Support("STATUS", closeStatement->get_status(), false,
                             info);

  curprint(") ");

  unp->cur.insert_newline(1);
}

void FortranCodeGeneration_locatedNode::unparseInquireStatement(
    SgStatement *stmt, SgUnparse_Info &info) {
  // Sage node corresponds to Fortran input/output statement
  SgInquireStatement *inquireStatement = isSgInquireStatement(stmt);
  ASSERT_not_null(inquireStatement);

  curprint("INQUIRE (");

  bool isLeadingEntry = true;
  if (inquireStatement->get_iolengthExp() != nullptr) {
    curprint("IOLENGTH=");
    unparseExpression(inquireStatement->get_iolengthExp(), info);
    isLeadingEntry = false;
  } else {
    // This is the "INQUIRE(inquire-spec-list)" case.

    if (inquireStatement->get_unit() != nullptr) {
      // Fortran rules don't allow output if "unit=*"
      isLeadingEntry = unparse_IO_Support(stmt, false, info);
    }

    unparse_IO_Control_Support("FILE", inquireStatement->get_file(),
                               isLeadingEntry, info);

    isLeadingEntry =
        isLeadingEntry && (inquireStatement->get_file() == nullptr);
    ROSE_ASSERT(isLeadingEntry == false);

    unparse_IO_Control_Support("ACCESS", inquireStatement->get_access(), false,
                               info);
    unparse_IO_Control_Support("FORM", inquireStatement->get_form(), false,
                               info);
    unparse_IO_Control_Support("RECL", inquireStatement->get_recl(), false,
                               info);
    unparse_IO_Control_Support("BLANK", inquireStatement->get_blank(), false,
                               info);
    unparse_IO_Control_Support("EXIST", inquireStatement->get_exist(), false,
                               info);
    unparse_IO_Control_Support("OPENED", inquireStatement->get_opened(), false,
                               info);
    unparse_IO_Control_Support("NUMBER", inquireStatement->get_number(), false,
                               info);
    unparse_IO_Control_Support("NAMED", inquireStatement->get_named(), false,
                               info);
    unparse_IO_Control_Support("NAME", inquireStatement->get_name(), false,
                               info);
    unparse_IO_Control_Support("SEQUENTIAL", inquireStatement->get_sequential(),
                               false, info);
    unparse_IO_Control_Support("DIRECT", inquireStatement->get_direct(), false,
                               info);
    unparse_IO_Control_Support("FORMATTED", inquireStatement->get_formatted(),
                               false, info);
    unparse_IO_Control_Support(
        "UNFORMATTED", inquireStatement->get_unformatted(), false, info);
    unparse_IO_Control_Support("NEXTREC", inquireStatement->get_nextrec(),
                               false, info);

    // F90 specific
    unparse_IO_Control_Support("POSITION", inquireStatement->get_position(),
                               false, info);
    unparse_IO_Control_Support("ACTION", inquireStatement->get_action(), false,
                               info);
    unparse_IO_Control_Support("READ", inquireStatement->get_read(), false,
                               info);
    unparse_IO_Control_Support("WRITE", inquireStatement->get_write(), false,
                               info);
    unparse_IO_Control_Support("READWRITE", inquireStatement->get_readwrite(),
                               false, info);
    unparse_IO_Control_Support("DELIM", inquireStatement->get_delim(), false,
                               info);
    unparse_IO_Control_Support("PAD", inquireStatement->get_pad(), false, info);

    // F2003 specific
    unparse_IO_Control_Support(
        "ASYNCHRONOUS", inquireStatement->get_asynchronous(), false, info);
    unparse_IO_Control_Support("DECIMAL", inquireStatement->get_decimal(),
                               false, info);
    unparse_IO_Control_Support("STREAM", inquireStatement->get_stream(), false,
                               info);
    unparse_IO_Control_Support("SIZE", inquireStatement->get_size(), false,
                               info);
    unparse_IO_Control_Support("ID", inquireStatement->get_id(), false, info);
    unparse_IO_Control_Support("PENDING", inquireStatement->get_pending(),
                               false, info);
  }

  curprint(") ");

  SgExprListExp *iolist = inquireStatement->get_io_stmt_list();
  if (iolist != nullptr) {
    // DQ (3/28/2017): Eliminate warning of overloaded virtual function in base
    // class (from Clang). unparseExprList(iolist, info, false /*paren*/);
    unparseExprList(iolist, info);
  }

  unp->cur.insert_newline(1);
}

void FortranCodeGeneration_locatedNode::unparseFlushStatement(
    SgStatement *stmt, SgUnparse_Info &info) {
  // Sage node corresponds to Fortran input/output statement
  SgFlushStatement *flushStatement = isSgFlushStatement(stmt);
  ASSERT_not_null(flushStatement);

  curprint("FLUSH (");

  unparse_IO_Support(stmt, false, info);

  curprint(") ");

  SgExprListExp *iolist = flushStatement->get_io_stmt_list();
  if (iolist != nullptr) {
    unparseExprList(iolist, info);
  }
  unp->cur.insert_newline(1);
}

void FortranCodeGeneration_locatedNode::unparseRewindStatement(
    SgStatement *stmt, SgUnparse_Info &info) {
  // Sage node corresponds to Fortran input/output statement
  SgRewindStatement *rewindStatement = isSgRewindStatement(stmt);
  ASSERT_not_null(rewindStatement);

  curprint("REWIND (");

  unparse_IO_Support(stmt, false, info);

  curprint(") ");

  SgExprListExp *iolist = rewindStatement->get_io_stmt_list();
  if (iolist != nullptr) {
    // DQ (3/28/2017): Eliminate warning of overloaded virtual function in base
    // class (from Clang). unparseExprList(iolist, info, false /*paren*/);
    unparseExprList(iolist, info);
  }

  unp->cur.insert_newline(1);
}

void FortranCodeGeneration_locatedNode::unparseBackspaceStatement(
    SgStatement *stmt, SgUnparse_Info &info) {
  // Sage node corresponds to Fortran input/output statement
  SgBackspaceStatement *backspaceStatement = isSgBackspaceStatement(stmt);
  ASSERT_not_null(backspaceStatement);

  curprint("BACKSPACE (");

  unparse_IO_Support(stmt, false, info);

  curprint(") ");

  SgExprListExp *iolist = backspaceStatement->get_io_stmt_list();
  if (iolist != nullptr) {
    // DQ (3/28/2017): Eliminate warning of overloaded virtual function in base
    // class (from Clang). unparseExprList(iolist, info, false /*paren*/);
    unparseExprList(iolist, info);
  }
  unp->cur.insert_newline(1);
}

void FortranCodeGeneration_locatedNode::unparseEndfileStatement(
    SgStatement *stmt, SgUnparse_Info &info) {
  // Sage node corresponds to Fortran input/output statement
  SgEndfileStatement *endfileStatement = isSgEndfileStatement(stmt);
  ASSERT_not_null(endfileStatement);

  curprint("ENDFILE (");

  unparse_IO_Support(stmt, false, info);

  curprint(") ");

  SgExprListExp *iolist = endfileStatement->get_io_stmt_list();
  if (iolist != nullptr) {
    // DQ (3/28/2017): Eliminate warning of overloaded virtual function in base
    // class (from Clang). unparseExprList(iolist, info, false /*paren*/);
    unparseExprList(iolist, info);
  }

  unp->cur.insert_newline(1);
}

void FortranCodeGeneration_locatedNode::unparseWaitStatement(
    SgStatement *stmt, SgUnparse_Info &info) {
  SgWaitStatement *waitStatement = isSgWaitStatement(stmt);
  ASSERT_not_null(waitStatement);

  auto requireExactChildOwner = [&](SgExpression *expression, const char *name,
                                    bool required) {
    if (expression == nullptr) {
      if (required) {
        std::cerr << "REX_UNPARSE_INVARIANT[fortran-wait-spec]: WAIT spec '"
                  << name << "' is required but absent\n";
        ROSE_ABORT();
      }
      return;
    }
    if (expression->get_parent() != waitStatement) {
      std::cerr << "REX_UNPARSE_INVARIANT[fortran-wait-spec]: WAIT spec '"
                << name << "' does not have exact statement ownership\n";
      ROSE_ABORT();
    }
  };
  requireExactChildOwner(waitStatement->get_unit(), "UNIT", true);
  requireExactChildOwner(waitStatement->get_end(), "END", false);
  requireExactChildOwner(waitStatement->get_eor(), "EOR", false);
  requireExactChildOwner(waitStatement->get_err(), "ERR", false);
  requireExactChildOwner(waitStatement->get_id(), "ID", false);
  requireExactChildOwner(waitStatement->get_iomsg(), "IOMSG", false);
  requireExactChildOwner(waitStatement->get_iostat(), "IOSTAT", false);
  if (waitStatement->get_io_stmt_list() != nullptr) {
    std::cerr << "REX_UNPARSE_INVARIANT[fortran-wait-spec]: WAIT statement "
                 "cannot own an I/O item list\n";
    ROSE_ABORT();
  }

  curprint("WAIT (");

  unparse_IO_Support(stmt, false, info);
  unparse_IO_Control_Support("END", waitStatement->get_end(), false, info);
  unparse_IO_Control_Support("EOR", waitStatement->get_eor(), false, info);
  unparse_IO_Control_Support("ID", waitStatement->get_id(), false, info);

  curprint(") ");

  unp->cur.insert_newline(1);
}

void FortranCodeGeneration_locatedNode::unparse_Image_Ctrl_Stmt_Support(
    SgImageControlStatement *stmt, bool print_comma, SgUnparse_Info &info) {
  ROSE_ASSERT(stmt);

  if (stmt->get_stat()) {
    if (print_comma)
      curprint(", ");
    else
      print_comma = true;
    curprint("STAT=");
    unparseExpression(stmt->get_stat(), info);
  }
  if (stmt->get_err_msg()) {
    if (print_comma)
      curprint(", ");
    else
      print_comma = true;
    curprint("ERRMSG=");
    unparseExpression(stmt->get_err_msg(), info);
  }
}

void FortranCodeGeneration_locatedNode::unparseSyncAllStatement(
    SgStatement *stmt, SgUnparse_Info &info) {
  SgSyncAllStatement *sync_stmt = isSgSyncAllStatement(stmt);
  ROSE_ASSERT(sync_stmt);

  bool print_initial_comma = false;

  curprint("SYNC ALL (");

  unparse_Image_Ctrl_Stmt_Support(sync_stmt, print_initial_comma, info);

  curprint(")");
  unp->cur.insert_newline(1);
}

void FortranCodeGeneration_locatedNode::unparseSyncImagesStatement(
    SgStatement *stmt, SgUnparse_Info &info) {
  SgSyncImagesStatement *sync_stmt = isSgSyncImagesStatement(stmt);
  ROSE_ASSERT(sync_stmt);

  SgExpression *image_set = sync_stmt->get_image_set();
  ROSE_ASSERT(image_set);

  bool print_comma = true;

  curprint("SYNC IMAGES (");

  // unparse the image set
  if (isSgNullExpression(image_set)) {
    // A null expression is used to indicate lack of an actual/"real" expression
    curprint("*");
  } else {
    unparseExpression(sync_stmt->get_image_set(), info);
  }

  if (sync_stmt->get_stat()) {
    if (print_comma)
      curprint(", ");
    else
      print_comma = true;
    curprint("STAT=");
    unparseExpression(sync_stmt->get_stat(), info);
  }
  if (sync_stmt->get_err_msg()) {
    if (print_comma)
      curprint(", ");
    else
      print_comma = true;
    curprint("ERRMSG=");
    unparseExpression(sync_stmt->get_err_msg(), info);
  }

  curprint(")");

  unp->cur.insert_newline(1);
}

void FortranCodeGeneration_locatedNode::unparseSyncMemoryStatement(
    SgStatement *stmt, SgUnparse_Info &info) {
  SgSyncMemoryStatement *sync_stmt = isSgSyncMemoryStatement(stmt);
  ROSE_ASSERT(sync_stmt);

  bool print_comma = false;

  curprint("SYNC MEMORY (");

  if (sync_stmt->get_stat()) {
    if (print_comma)
      curprint(", ");
    else
      print_comma = true;
    curprint("STAT=");
    unparseExpression(sync_stmt->get_stat(), info);
  }
  if (sync_stmt->get_err_msg()) {
    if (print_comma)
      curprint(", ");
    else
      print_comma = true;
    curprint("ERRMSG=");
    unparseExpression(sync_stmt->get_err_msg(), info);
  }

  curprint(")");

  unp->cur.insert_newline(1);
}

void FortranCodeGeneration_locatedNode::unparseSyncTeamStatement(
    SgStatement *stmt, SgUnparse_Info &info) {
  SgSyncTeamStatement *sync_stmt = isSgSyncTeamStatement(stmt);
  ROSE_ASSERT(sync_stmt);

  bool print_comma = true;

  curprint("SYNC TEAM (");

  // unparse the team value
  unparseExpression(sync_stmt->get_team_value(), info);

  if (sync_stmt->get_stat()) {
    if (print_comma)
      curprint(", ");
    else
      print_comma = true;
    curprint("STAT=");
    unparseExpression(sync_stmt->get_stat(), info);
  }
  if (sync_stmt->get_err_msg()) {
    if (print_comma)
      curprint(", ");
    else
      print_comma = true;
    curprint("ERRMSG=");
    unparseExpression(sync_stmt->get_err_msg(), info);
  }

  curprint(")");
  unp->cur.insert_newline(1);
}

void FortranCodeGeneration_locatedNode::unparseLockStatement(
    SgStatement *stmt, SgUnparse_Info &info) {
  SgLockStatement *lock_stmt = isSgLockStatement(stmt);
  ROSE_ASSERT(lock_stmt);

  bool print_initial_comma = true;

  curprint("LOCK (");

  // unparse the lock variable
  unparseExpression(lock_stmt->get_lock_variable(), info);

  if (lock_stmt->get_acquired_lock()) {
    curprint(", ");
    curprint("ACQUIRED_LOCK=");
    unparseExpression(lock_stmt->get_acquired_lock(), info);
  }
  unparse_Image_Ctrl_Stmt_Support(lock_stmt, print_initial_comma, info);

  curprint(")");
  unp->cur.insert_newline(1);
}

void FortranCodeGeneration_locatedNode::unparseUnlockStatement(
    SgStatement *stmt, SgUnparse_Info &info) {
  SgUnlockStatement *unlock_stmt = isSgUnlockStatement(stmt);
  ROSE_ASSERT(unlock_stmt);

  bool print_initial_comma = true;

  curprint("UNLOCK (");

  // unparse the lock variable
  unparseExpression(unlock_stmt->get_lock_variable(), info);

  unparse_Image_Ctrl_Stmt_Support(unlock_stmt, print_initial_comma, info);

  curprint(")");
  unp->cur.insert_newline(1);
}

void FortranCodeGeneration_locatedNode::unparseAssociateStatement(
    SgStatement *stmt, SgUnparse_Info &info) {
  // Sage node corresponds to Fortran input/output statement
  SgAssociateStatement *associateStatement = isSgAssociateStatement(stmt);
  ASSERT_not_null(associateStatement);

  curprint("ASSOCIATE (");

  // Pei-Hung (07/24/2019) unparse SgDeclarationStatementPtrList for multiple
  // associates
  SgDeclarationStatementPtrList::iterator pp =
      associateStatement->get_associates().begin();
  while (pp != associateStatement->get_associates().end()) {
    SgVariableDeclaration *variableDeclaration = isSgVariableDeclaration(*pp);
    ASSERT_not_null(variableDeclaration);

    SgInitializedName *variable =
        *(variableDeclaration->get_variables().begin());
    ASSERT_not_null(variable);

    curprint(variable->get_name());
    curprint(" => ");
    unparseExpression(variable->get_initializer(), info);
    pp++;
    if (pp != associateStatement->get_associates().end())
      curprint(", ");
  }
  curprint(") ");

  ASSERT_not_null(associateStatement->get_body());
  unparseStatement(associateStatement->get_body(), info);

  unparseStatementNumbersSupport(nullptr, info);

  curprint("END ASSOCIATE");

  unp->cur.insert_newline(1);
}

//----------------------------------------------------------------------------
//  void FortranCodeGeneration_locatedNode::<executable statements, other>
//----------------------------------------------------------------------------

void FortranCodeGeneration_locatedNode::unparseExprStmt(SgStatement *stmt,
                                                        SgUnparse_Info &info) {
  // Sage node corresponds to Fortran expression
  SgExprStatement *expr_stmt = isSgExprStatement(stmt);
  ASSERT_not_null(expr_stmt);
  ROSE_ASSERT(expr_stmt->get_expression());

  SgUnparse_Info ninfo(info);

  // Never unparse class definition in expression stmt
  ninfo.set_SkipClassDefinition();

  ninfo.set_SkipEnumDefinition();
  unparseExpression(expr_stmt->get_expression(), ninfo);

  if (ninfo.inVarDecl()) {
    curprint(",");
  }

  unp->u_sage->curprint_newline();
}

//----------------------------------------------------------------------------
//  FortranCodeGeneration_locatedNode::<pragmas>
//----------------------------------------------------------------------------

void FortranCodeGeneration_locatedNode::unparsePragmaDeclStmt(
    SgStatement *stmt, SgUnparse_Info &) {
  // Sage node corresponds to Fortran convention !pragma
  SgPragmaDeclaration *pragmaDeclaration = isSgPragmaDeclaration(stmt);
  ASSERT_not_null(pragmaDeclaration);

  SgPragma *pragma = pragmaDeclaration->get_pragma();
  ASSERT_not_null(pragma);

  switch (pragmaDeclaration->get_fortran_directive_family()) {
  case SgPragmaDeclaration::e_fortran_directive_none:
    curprint("!pragma ");
    break;
  case SgPragmaDeclaration::e_fortran_directive_openmp:
  case SgPragmaDeclaration::e_fortran_directive_ompx:
  case SgPragmaDeclaration::e_fortran_directive_openacc:
  case SgPragmaDeclaration::e_fortran_directive_cuda:
    curprint("!$");
    break;
  default:
    std::cerr << "REX_UNPARSE_INVARIANT[fortran-pragma-family]: pragma has "
                 "invalid family="
              << static_cast<int>(
                     pragmaDeclaration->get_fortran_directive_family())
              << std::endl;
    ROSE_ABORT();
  }
  curprint(pragma->get_pragma());
  curprint("\n");
}

//----------------------------------------------------------------------------
//  Program unit helpers
//----------------------------------------------------------------------------

void FortranCodeGeneration_locatedNode::genPUAutomaticStmts(
    SgStatement *stmt, SgUnparse_Info &info) {
  // For formatting purposes, pretend we have a small basic block
  unp->cur.format(stmt, info, FORMAT_BEFORE_BASIC_BLOCK2);
  curprint("USE ROSE__TYPES");

  unp->cur.format(stmt, info, FORMAT_BEFORE_STMT);

  unp->cur.format(stmt, info, FORMAT_AFTER_BASIC_BLOCK1);
}

void FortranCodeGeneration_locatedNode::unparseFuncArgs(
    SgInitializedNamePtrList *args, SgUnparse_Info &info) {
  unparseInitNamePtrList(args, info);
}

void FortranCodeGeneration_locatedNode::unparseInitNamePtrList(
    SgInitializedNamePtrList *args, SgUnparse_Info &) {
  SgInitializedNamePtrList::iterator it = args->begin();
  while (it != args->end()) {
    SgInitializedName *arg = *it;
    curprint(arg->get_name().str());

    // Move to the next argument
    it++;

    // Check if this is the last argument (output a "," separator if not)
    if (it != args->end()) {
      curprint(", ");
    }
  }
}

//----------------------------------------------------------------------------
//  Declarations helpers
//----------------------------------------------------------------------------
void FortranCodeGeneration_locatedNode::unparseArrayAttr(SgArrayType *type,
                                                         SgUnparse_Info &info,
                                                         bool oneVarOnly) {
  if (!oneVarOnly) {
    ASSERT_not_null(type);
    curprint(type->get_isCoArray() ? "[" : "(");
    unparseExpression(type->get_dim_info(), info);
    curprint(type->get_isCoArray() ? "]" : ")");
  }
}

void FortranCodeGeneration_locatedNode::unparseStringAttr(SgTypeString *type,
                                                          SgUnparse_Info &info,
                                                          bool oneVarOnly) {
  if (!oneVarOnly) {
    curprint("*");
    curprint("(");
    unparseExpression(type->get_lengthExpression(), info);
    curprint(")");
  }
}

void FortranCodeGeneration_locatedNode::unparseEntityTypeAttr(
    SgType *type, SgUnparse_Info &info, bool oneVarOnly,
    bool emitDimensionShape) {
  if (type->get_isCoArray()) {
    SgType *baseType;
    SgArrayType *arrayType = nullptr;
    if (isSgPointerType(type))
      baseType = isSgPointerType(type)->get_base_type();
    else {
      arrayType = isSgArrayType(type);
      baseType = arrayType->get_base_type();
    }
    if (isSgPointerType(type))
      unparseEntityTypeAttr(baseType, info, oneVarOnly, emitDimensionShape);
    else if (isSgTypeString(baseType)) {
      unparseArrayAttr(arrayType, info, oneVarOnly); // print codimension
      unparseStringAttr(isSgTypeString(baseType), info, oneVarOnly);
    } else if (isSgArrayType(baseType)) {
      SgArrayType *arrayBaseType = isSgArrayType(baseType);
      if (arrayBaseType->get_isCoArray() || emitDimensionShape) {
        unparseArrayAttr(arrayBaseType, info, oneVarOnly);
      }
      unparseArrayAttr(arrayType, info, oneVarOnly); // print codimension
      SgTypeString *stringType = isSgTypeString(arrayBaseType->get_base_type());
      if (stringType)
        unparseStringAttr(stringType, info, oneVarOnly);
    } else
      unparseArrayAttr(arrayType, info, oneVarOnly); // print codimension
  } else if (isSgArrayType(type)) {
    SgArrayType *arrayType = isSgArrayType(type);
    if (arrayType->get_isCoArray() || emitDimensionShape) {
      unparseArrayAttr(arrayType, info, oneVarOnly);
    }
    SgTypeString *stringType = isSgTypeString(arrayType->get_base_type());
    if (stringType)
      unparseStringAttr(stringType, info, oneVarOnly);
  } else if (isSgPointerType(type))
    unparseEntityTypeAttr(isSgPointerType(type)->get_base_type(), info,
                          oneVarOnly, emitDimensionShape);
  else if (isSgTypeString(type))
    unparseStringAttr(isSgTypeString(type), info, oneVarOnly);
}

void FortranCodeGeneration_locatedNode::unparseVarDecl(
    SgStatement *stmt, SgInitializedName *initializedName,
    SgUnparse_Info &info) {
  // DQ (9/22/2007): Note that this function does not use its SgStatement* stmt
  // parameter!

  // General format:
  //   <type> <attributes> :: <variable>

  SgName name = initializedName->get_name();
  SgType *semanticType = initializedName->get_type();
  SgType *type = initializedName->get_fortran_source_type();
  SgInitializer *init = initializedName->get_initializer();
  if (semanticType == nullptr || type == nullptr ||
      !fortranSourceTypeMatchesSemanticType(initializedName)) {
    std::cerr << "REX_UNPARSE_INVARIANT[fortran-source-type]: variable '"
              << name
              << "' has no exact compatible semantic/source type pair\n";
    ROSE_ABORT();
  }
  const bool emitDimensionShape =
      !fortranSeparateStatementOwnsArrayShape(initializedName);
  SgUnparse_Info typeInfo(info);
  typeInfo.set_reference_node_for_qualification(initializedName);

  // Find out how many variables are declared in the given stmt:
  SgVariableDeclaration *variableDeclaration = isSgVariableDeclaration(stmt);
  ASSERT_not_null(variableDeclaration);
  int numVar = variableDeclaration->get_variables().size();

  if (info.SkipBaseType() == false) {
    // DXN (08/19/2011): unparse the base type when there are more than one
    // declared variables
    if (numVar > 1) {
      SgType *baseType = type->stripType(SgType::STRIP_ARRAY_TYPE);
      unp->u_fortran_type->unparseType(
          baseType, typeInfo, false); // do not print type attributes such as
                                      // dimension, length on the left of ::
    } else {
      unp->u_fortran_type->unparseType(
          type, typeInfo, true); // print type attribute on the left of ::
    }

    if (variableDeclaration->get_declarationModifier()
            .get_typeModifier()
            .isAllocatable()) {
      curprint(", ALLOCATABLE");
    }

    if (variableDeclaration->get_declarationModifier()
            .get_typeModifier()
            .isAsynchronous()) {
      curprint(", ASYNCHRONOUS");
    }

    if (variableDeclaration->get_declarationModifier()
            .get_typeModifier()
            .isIntent_in()) {
      curprint(", INTENT(IN)");
    }

    if (variableDeclaration->get_declarationModifier()
            .get_typeModifier()
            .isIntent_out()) {
      curprint(", INTENT(OUT)");
    }

    if (variableDeclaration->get_declarationModifier()
            .get_typeModifier()
            .isIntent_inout()) {
      curprint(", INTENT(INOUT)");
    }

    if (variableDeclaration->get_declarationModifier()
            .get_typeModifier()
            .get_constVolatileModifier()
            .isVolatile()) {
      curprint(", VOLATILE");
    }

    if (variableDeclaration->get_declarationModifier()
            .get_storageModifier()
            .isExtern()) {
      if (type->variantT() == V_SgTypeVoid) // FMZ 6/17/2009
        curprint("EXTERNAL");
      else
        curprint(", EXTERNAL");
    }

    // Fortran contiguous array storage attribute
    if (variableDeclaration->get_declarationModifier()
            .get_storageModifier()
            .isContiguous()) {
      curprint(", CONTIGUOUS");
    }

    // Fortran CUDA support
    if (variableDeclaration->get_declarationModifier()
            .get_storageModifier()
            .isCudaDeviceMemory()) {
      curprint(", device");
    }
    if (variableDeclaration->get_declarationModifier()
            .get_storageModifier()
            .isCudaManaged()) {
      curprint(", managed");
    }
    if (variableDeclaration->get_declarationModifier()
            .get_storageModifier()
            .isCudaUnified()) {
      curprint(", unified");
    }
    if (variableDeclaration->get_declarationModifier()
            .get_storageModifier()
            .isCudaConstant()) {
      curprint(", constant");
    }
    if (variableDeclaration->get_declarationModifier()
            .get_storageModifier()
            .isCudaShared()) {
      curprint(", shared");
    }
    if (variableDeclaration->get_declarationModifier()
            .get_storageModifier()
            .isCudaPinned()) {
      curprint(", pinned");
    }
    if (variableDeclaration->get_declarationModifier()
            .get_storageModifier()
            .isCudaTexture()) {
      curprint(", texture");
    }

    if (variableDeclaration->get_declarationModifier()
            .get_typeModifier()
            .get_constVolatileModifier()
            .isConst()) {
      // PARAMETER in Fortran implies const in C/C++
      curprint(", PARAMETER");
    }

    if (variableDeclaration->get_declarationModifier()
            .get_accessModifier()
            .isPublic()) {
      // The PUBLIC keyword is only permitted within Modules
      if (SageInterface::getEnclosingModuleStatement(variableDeclaration) !=
          nullptr) {
        curprint(", PUBLIC");
      } else {
        // Liao 12/14/2010
        // SgAccessModifier::post_construction_initialization() will set the
        // modifier to e_default, which in turn is equal to e_public variable
        // declarations should have public access by default. So I turn off this
        // warning after discussing this issue with Dan
        // printf ("Warning: statement marked as public in non-module scope in
        // FortranCodeGeneration_locatedNode::unparseVarDecl(). \n");
      }
    }

    if (variableDeclaration->get_declarationModifier()
            .get_accessModifier()
            .isPrivate()) {
      // The PRIVATE keyword is only permitted within Modules
      if (SageInterface::getEnclosingModuleStatement(variableDeclaration) !=
          nullptr) {
        curprint(", PRIVATE");
      } else {
        fprintf(stderr,
                "REX_UNPARSE_INVARIANT[fortran-access]: private variable "
                "declaration occurs outside a module\n");
        ROSE_ABORT();
      }
    }

    bool is_protected = true;
    SgInitializedNamePtrList &variableList =
        variableDeclaration->get_variables();
    ROSE_ASSERT(variableList.empty() == false);
    SgInitializedNamePtrList::iterator i = variableList.begin();
    while (i != variableList.end()) {
      if ((*i)->get_protected_declaration() == false)
        is_protected = false;
      i++;
    }
    if (is_protected && (variableList.empty() == false)) {
      curprint(", PROTECTED");
    }

    if (variableDeclaration->get_declarationModifier()
            .get_typeModifier()
            .isIntrinsic()) {
      curprint(", INTRINSIC");
    }

    if (variableDeclaration->get_declarationModifier().isBind()) {
      curprint(", ");

      // This is factored so that it can be called for function declarations,
      // and variable declarations
      unparseBindAttribute(variableDeclaration);
    }

    if (variableDeclaration->get_declarationModifier()
            .get_typeModifier()
            .isOptional()) {
      curprint(", OPTIONAL");
    }

    if (variableDeclaration->get_declarationModifier()
            .get_typeModifier()
            .isSave()) {
      curprint(", SAVE");
    }

    if (variableDeclaration->get_declarationModifier()
            .get_typeModifier()
            .isTarget()) {
      curprint(", TARGET");
    }

    if (variableDeclaration->get_declarationModifier()
            .get_typeModifier()
            .isValue()) {
      curprint(", VALUE");
    }

    // FMZ (4/14/2009): Cray Pointer
    if (isSgTypeCrayPointer(type) == nullptr) {
      curprint(" :: ");
    } else {
      curprint(" (");
    }
  }
  curprint(name.str());

  if (isSgTypeCrayPointer(type) != nullptr) {
    SgInitializedName *pointeeVar = initializedName->get_cray_pointer_pointee();
    SgExprListExp *pointeeShape =
        initializedName->get_fortran_cray_pointer_pointee_shape();
    if (pointeeVar == nullptr ||
        (pointeeShape != nullptr) !=
            (pointeeVar->get_fortran_separate_shape_declaration() ==
             variableDeclaration) ||
        (pointeeShape != nullptr &&
         !fortranCrayPointerStatementOwnsArrayShape(pointeeVar))) {
      std::cerr << "REX_UNPARSE_INVARIANT[fortran-cray-pointer-source]: "
                   "pointer="
                << initializedName->get_name()
                << " has no exact typed scalar or shaped pointee source\n";
      ROSE_ABORT();
    }
    SgName pointeeName = pointeeVar->get_name();
    curprint(",");
    curprint(pointeeName.str());
    if (pointeeShape != nullptr) {
      curprint("(");
      unparseExprList(pointeeShape, info);
      curprint(")");
    }
    curprint(") ");
  } else
    unparseEntityTypeAttr(type, info, numVar == 1, emitDimensionShape);

  // Unparse the initializers if any exist
  // printf ("In
  // FortranCodeGeneration_locatedNode::unparseVarDecl(initializedName=%p):
  // variable initializer = %p \n",initializedName,init);
  if (init != nullptr) {
    SgInitializer *initializer = isSgInitializer(init);
    ASSERT_not_null(initializer);
    if (isSgPointerType(semanticType)) {
      curprint(" => ");
      auto *assignInitializer = isSgAssignInitializer(initializer);
      if (assignInitializer == nullptr ||
          assignInitializer->get_operand() == nullptr) {
        std::cerr << "Error: malformed Fortran pointer initializer"
                  << std::endl;
        ROSE_ABORT();
      }
      if (isSgNullExpression(assignInitializer->get_operand())) {
        curprint("NULL()");
      } else {
        unparseExpression(initializer, info);
      }
    } else {
      curprint(" = ");
      unparseExpression(initializer, info);
    }
  }
}

//----------------------------------------------------------------------------
//  void Unparser::printDeclModifier
//  void Unparser::printAccessModifier
//  void Unparser::printStorageModifier
//
//  The following 2 functions: printAccessModifier and printStorageModifier,
//  are just the two halves from printDeclModifier. These two functions
//  are used in the unparse functions for SgMemberFunctionDeclarations
//  and SgVariableDeclaration.  printAccessModifier is first called before
//  the format function. If "private", "protected", or "public" is to
//  be printed out, it does so here. Then I format which will put me
//  in position to unparse the declaration. Then I call
//  printSpecifer2, which will print out any keywords if the option is
//  turned on.  Then the declaration is printed in the same line. If I
//  didnt do this, the printing of keywords would be done before
//  formatting, and would put the declaration on another line (and
//  would look terribly formatted).
//----------------------------------------------------------------------------

void FortranCodeGeneration_locatedNode::printDeclModifier(
    SgDeclarationStatement *, SgUnparse_Info &) {
  printf("Access modifiers are handled differently for Fortran, this function "
         "printDeclModifier() should not be called! \n");
  ROSE_ABORT();
}

void FortranCodeGeneration_locatedNode::printAccessModifier(
    SgDeclarationStatement *, SgUnparse_Info &) {
  printf("Access modifiers are handled differently for Fortran, this function "
         "printAccessModifier() should not be called! \n");
  ROSE_ABORT();
}

void FortranCodeGeneration_locatedNode::unparseBindAttribute(
    SgDeclarationStatement *declaration) {
  ASSERT_not_null(declaration);
  // Code generation support for "bind" attribute
  if (declaration->get_declarationModifier().isBind()) {
    if (declaration->get_linkage().empty()) {
      std::cerr << "Error: Fortran BIND attribute has no language binding spec"
                << std::endl;
      ROSE_ABORT();
    }
    curprint(" bind(");

    curprint(declaration->get_linkage());

    if (declaration->get_binding_label().empty() == false) {
      curprint(",NAME=\"");
      curprint(declaration->get_binding_label());
      curprint("\"");
    }
    if (declaration->get_binding_cdefined()) {
      curprint(",CDEFINED");
    }
    curprint(")");
  }
}

void FortranCodeGeneration_locatedNode::printStorageModifier(
    SgDeclarationStatement *, SgUnparse_Info &) {
  // FIXME: this will look different for full-featured Fortran
  printf("Access modifiers are handled differently for Fortran, this function "
         "printStorageModifier() should not be called! \n");
  ROSE_ABORT();
}

void FortranCodeGeneration_locatedNode::unparseProcHdrStmt(
    SgStatement *stmt, SgUnparse_Info &info) {
  // Sage node corresponds to Fortran procedure program unit

  SgProcedureHeaderStatement *procedureHeader =
      isSgProcedureHeaderStatement(stmt);
  ASSERT_not_null(procedureHeader);
  const auto sourceForm = procedureHeader->get_fortran_procedure_source_form();
  if (sourceForm ==
      SgProcedureHeaderStatement::e_fortran_procedure_source_form_unknown) {
    std::cerr << "REX_UNPARSE_INVARIANT[fortran-procedure-source-form]: "
                 "procedure '"
              << procedureHeader->get_name()
              << "' has no exact semantic or lexical source form\n";
    ROSE_ABORT();
  }
  if (sourceForm == SgProcedureHeaderStatement::
                        e_fortran_procedure_source_form_semantic_only) {
    std::cerr << "REX_UNPARSE_INVARIANT[fortran-procedure-source-owner]: "
                 "semantic-only procedure '"
              << procedureHeader->get_name()
              << "' reached a source-emission list\n";
    ROSE_ABORT();
  }
  if (sourceForm ==
      SgProcedureHeaderStatement::
          e_fortran_procedure_source_form_compiler_module_header) {
    std::cerr << "REX_UNPARSE_INVARIANT[compiler-module-procedure-output]: "
                 "non-emittable compiler module procedure '"
              << procedureHeader->get_name()
              << "' reached a source-emission list\n";
    ROSE_ABORT();
  }

  const bool typeDeclaration =
      sourceForm == SgProcedureHeaderStatement::
                        e_fortran_procedure_source_form_type_declaration ||
      sourceForm == SgProcedureHeaderStatement::
                        e_fortran_procedure_source_form_type_external;
  if (typeDeclaration) {
    const bool sourceExternal =
        sourceForm == SgProcedureHeaderStatement::
                          e_fortran_procedure_source_form_type_external;
    SgScopeStatement *scope = isSgScopeStatement(procedureHeader->get_parent());
    SgFunctionDeclaration *canonical = isSgFunctionDeclaration(
        procedureHeader->get_firstNondefiningDeclaration());
    SgFunctionSymbol *symbol =
        canonical != nullptr
            ? isSgFunctionSymbol(canonical->get_symbol_from_symbol_table())
            : nullptr;
    SgFunctionType *functionType = procedureHeader->get_type();
    SgType *returnType =
        functionType != nullptr ? functionType->get_return_type() : nullptr;
    SgFunctionType *sourceFunctionType = procedureHeader->get_type_syntax();
    SgType *sourceReturnType = sourceFunctionType != nullptr
                                   ? sourceFunctionType->get_return_type()
                                   : nullptr;
    const bool sourceFunctionMatches =
        SageInterface::fortranSourceFunctionResultMatchesSemanticResult(
            sourceFunctionType, functionType);
    Sg_File_Info *sourceInfo = procedureHeader->get_file_info();
    const bool markedExternal = procedureHeader->get_declarationModifier()
                                    .get_storageModifier()
                                    .isExtern();
    if (!procedureHeader->isForward() ||
        procedureHeader->get_definition() != nullptr ||
        !procedureHeader->isFunction() || scope == nullptr ||
        procedureHeader->get_scope() != scope ||
        !scope->statementExistsInScope(procedureHeader) ||
        canonical == nullptr || symbol == nullptr ||
        symbol->get_declaration() != canonical ||
        symbol->get_scope() != scope || functionType == nullptr ||
        returnType == nullptr || isSgTypeVoid(returnType) != nullptr ||
        !procedureHeader->get_type_syntax_is_available() ||
        sourceFunctionType == nullptr || sourceReturnType == nullptr ||
        isSgTypeVoid(sourceReturnType) != nullptr || !sourceFunctionMatches ||
        procedureHeader->get_orig_return_type() != sourceReturnType ||
        sourceInfo == nullptr || sourceInfo->isCompilerGenerated() ||
        !sourceInfo->isOutputInCodeGeneration() ||
        markedExternal != sourceExternal) {
      std::cerr << "REX_UNPARSE_INVARIANT[fortran-procedure-type-source]: "
                   "typed procedure declaration '"
                << procedureHeader->get_name().getString()
                << "' has malformed lexical ownership, source provenance, "
                   "or canonical function identity\n";
      ROSE_ABORT();
    }

    SgUnparse_Info typeInfo(info);
    typeInfo.set_reference_node_for_qualification(procedureHeader);
    unp->u_fortran_type->unparseType(sourceReturnType, typeInfo, true);
    if (sourceExternal) {
      curprint(", EXTERNAL");
    }
    if (procedureHeader->get_declarationModifier()
            .get_typeModifier()
            .isIntrinsic()) {
      curprint(", INTRINSIC");
    }
    if (procedureHeader->get_declarationModifier()
            .get_accessModifier()
            .isPrivate()) {
      if (SageInterface::getEnclosingModuleStatement(procedureHeader) ==
          nullptr) {
        std::cerr << "REX_UNPARSE_INVARIANT[fortran-procedure-access]: "
                     "private typed procedure declaration occurs outside a "
                     "module\n";
        ROSE_ABORT();
      }
      curprint(", PRIVATE");
    } else if (procedureHeader->get_declarationModifier()
                   .get_accessModifier()
                   .isPublic() &&
               SageInterface::getEnclosingModuleStatement(procedureHeader) !=
                   nullptr) {
      curprint(", PUBLIC");
    }
    if (procedureHeader->get_declarationModifier().isBind()) {
      curprint(", ");
      unparseBindAttribute(procedureHeader);
    }
    curprint(" :: ");
    curprint(procedureHeader->get_name().str());
    unp->cur.insert_newline(1);
    return;
  }

  if (sourceForm !=
      SgProcedureHeaderStatement::e_fortran_procedure_source_form_header) {
    std::cerr << "REX_UNPARSE_INVARIANT[fortran-procedure-source-form]: "
                 "procedure '"
              << procedureHeader->get_name()
              << "' has an invalid source-form value\n";
    ROSE_ABORT();
  }
  if (!procedureHeader->isForward() &&
      procedureHeader->get_definition() == nullptr) {
    std::cerr << "Error: defining Fortran procedure header has no definition"
              << std::endl;
    ROSE_ABORT();
  }
  SageInterface::isFortranProgramUnitWithoutSourceName(procedureHeader);
  if (!procedureHeader->isBlockData() &&
      procedureHeader->get_name().is_null()) {
    std::cerr << "Error: Fortran procedure header has no name" << std::endl;
    ROSE_ABORT();
  }
  if (procedureHeader->isBlockData()) {
    const bool hasBlockDataName = blockDataHasName(procedureHeader);
    if (procedureHeader->isForward()) {
      std::cerr << "Error: BLOCK DATA cannot be a forward declaration"
                << std::endl;
      ROSE_ABORT();
    }
    if (hasBlockDataName && procedureHeader->get_name().is_null()) {
      std::cerr << "Error: named BLOCK DATA has no declaration name"
                << std::endl;
      ROSE_ABORT();
    }
    if (!hasBlockDataName && !procedureHeader->get_name().is_null()) {
      std::cerr << "Error: unnamed BLOCK DATA carries a public declaration "
                   "name"
                << std::endl;
      ROSE_ABORT();
    }
    if (hasBlockDataName &&
        !isValidFortranIdentifier(procedureHeader->get_name())) {
      std::cerr << "Error: named BLOCK DATA has an invalid source name"
                << std::endl;
      ROSE_ABORT();
    }
    if (!hasBlockDataName && procedureHeader->get_named_in_end_statement()) {
      std::cerr << "Error: unnamed BLOCK DATA has a named END BLOCK DATA"
                << std::endl;
      ROSE_ABORT();
    }
  } else {
    if (procedureHeader->get_block_data_name_kind() !=
        SgProcedureHeaderStatement::e_unknown_block_data_name_kind) {
      std::cerr << "Error: non-BLOCK-DATA procedure carries BLOCK DATA name "
                   "metadata"
                << std::endl;
      ROSE_ABORT();
    }
  }

  string typeOfFunction;
  if (procedureHeader->isFunction()) {
    typeOfFunction = " FUNCTION";
  } else {
    if (procedureHeader->isSubroutine()) {
      typeOfFunction = "SUBROUTINE";
    } else {
      ASSERT_require(procedureHeader->isBlockData());
      typeOfFunction = "BLOCK DATA";
    }
  }

  if (!procedureHeader->isForward() &&
      procedureHeader->get_definition() != nullptr &&
      !info.SkipFunctionDefinition()) {
    // Output the function declaration with definition

    // The unparsing of the definition will cause the unparsing of the
    // declaration (with SgUnparse_Info flags set to just unparse a forward
    // declaration!)
    SgUnparse_Info ninfo(info);

    // To avoid end of statement formatting (added CR's) we call the
    // unparseFuncDefnStmt directly
    unparseFuncDefnStmt(procedureHeader->get_definition(), ninfo);

    unp->cur.insert_newline(1);

    // The "END" has just been output by the unparsing of the
    // SgFunctionDefinition so we just want to finish it off with "PROGRAM
    // <name>".

    unparseStatementNumbersSupport(procedureHeader->get_end_numeric_label(),
                                   info);
    curprint("END " + typeOfFunction);
    if (procedureHeader->get_named_in_end_statement()) {
      curprint(" ");
      curprint(procedureHeader->get_name().str());
    }

    // Output 2 new lines to better separate functions visually in the output
    unp->cur.insert_newline(1);
    unp->cur.insert_newline(2); // FMZ
  } else {
    if (procedureHeader->get_functionModifier().isPure()) {
      curprint("PURE ");
    }
    if (procedureHeader->get_functionModifier().isElemental()) {
      curprint("ELEMENTAL ");
    }
    if (procedureHeader->get_functionModifier().isRecursive()) {
      curprint("RECURSIVE ");
    }
    if (procedureHeader->get_functionModifier().isCudaHost()) {
      curprint("attributes(host) ");
    }
    if (procedureHeader->get_functionModifier().isCudaGlobalFunction()) {
      curprint("attributes(global) ");
    }
    if (procedureHeader->get_functionModifier().isCudaDevice()) {
      curprint("attributes(device) ");
    }
    if (procedureHeader->get_functionModifier().isCudaGridGlobal()) {
      curprint("attributes(grid_global) ");
    }

    bool need_type = true;
    if (SgInitializedName *result = procedureHeader->get_result_name()) {
      SgVariableDeclaration *owner =
          isSgVariableDeclaration(result->get_parent());
      if (owner == nullptr || result->get_type() == nullptr) {
        std::cerr << "REX_UNPARSE_INVARIANT[fortran-function-result-owner]: "
                     "function result has no exact variable declaration "
                     "owner\n";
        ROSE_ABORT();
      }
      const bool sourceDeclared = result->get_fortran_source_type() != nullptr;
      if (sourceDeclared) {
        SgScopeStatement *scope = owner->get_scope();
        Sg_File_Info *sourceInfo = owner->get_file_info();
        if (owner->get_fortran_declaration_origin() !=
                SgVariableDeclaration::e_fortran_source_declaration ||
            scope == nullptr || owner->get_parent() != scope ||
            !scope->statementExistsInScope(owner) || sourceInfo == nullptr ||
            sourceInfo->isCompilerGenerated() ||
            !sourceInfo->isOutputInCodeGeneration() ||
            !fortranSourceTypeMatchesSemanticType(result)) {
          std::cerr
              << "REX_UNPARSE_INVARIANT[fortran-function-result-source]: "
              << "procedure=" << procedureHeader->get_name()
              << " result=" << result->get_name() << " owner=" << owner
              << " origin="
              << static_cast<int>(owner->get_fortran_declaration_origin())
              << " parent=" << owner->get_parent() << " scope=" << scope
              << " statement-owned="
              << (scope != nullptr && scope->statementExistsInScope(owner))
              << " source-info=" << sourceInfo << " compiler-generated="
              << (sourceInfo != nullptr && sourceInfo->isCompilerGenerated())
              << " output="
              << (sourceInfo != nullptr &&
                  sourceInfo->isOutputInCodeGeneration())
              << " type-match=" << fortranSourceTypeMatchesSemanticType(result)
              << " parameter-scope="
              << procedureHeader->get_functionParameterScope()
              << " has malformed lexical or type ownership\n";
          ROSE_ABORT();
        }
        need_type = false;
      } else {
        SgAuxiliaryDeclarationList *auxiliary =
            isSgAuxiliaryDeclarationList(owner->get_parent());
        SgScopeStatement *scope =
            auxiliary != nullptr ? isSgScopeStatement(auxiliary->get_parent())
                                 : nullptr;
        if (owner->get_fortran_declaration_origin() !=
                SgVariableDeclaration::e_fortran_semantic_only_declaration ||
            scope == nullptr ||
            scope->get_auxiliary_declarations() != auxiliary) {
          std::cerr
              << "REX_UNPARSE_INVARIANT[fortran-function-result-owner]: "
                 "non-source function result is not semantic-only auxiliary "
                 "state\n";
          ROSE_ABORT();
        }
      }
    }

    if (procedureHeader->isFunction() && need_type) {
      const auto sourceSpec = procedureHeader->get_fortran_result_type_spec();
      if (sourceSpec ==
          SgProcedureHeaderStatement::e_fortran_result_type_spec_unknown) {
        if (procedureHeader->get_type_syntax() != nullptr ||
            procedureHeader->get_type_syntax_is_available()) {
          std::cerr
              << "REX_UNPARSE_INVARIANT[fortran-procedure-prefix-type]: "
                 "untyped function header owns source result type state\n";
          ROSE_ABORT();
        }
      } else {
        SgFunctionType *semanticType = procedureHeader->get_type();
        SgFunctionType *sourceType = procedureHeader->get_type_syntax();
        SgType *sourceReturn =
            sourceType != nullptr ? sourceType->get_return_type() : nullptr;
        if (semanticType == nullptr || sourceType == nullptr ||
            sourceReturn == nullptr ||
            !procedureHeader->get_type_syntax_is_available() ||
            !SageInterface::fortranSourceFunctionResultMatchesSemanticResult(
                sourceType, semanticType) ||
            procedureHeader->get_orig_return_type() != sourceReturn) {
          std::cerr
              << "REX_UNPARSE_INVARIANT[fortran-procedure-prefix-type]: "
                 "typed function header has no exact source/semantic result "
                 "type contract\n";
          ROSE_ABORT();
        }
        SgUnparse_Info returnTypeInfo(info);
        returnTypeInfo.set_reference_node_for_qualification(procedureHeader);
        unp->u_fortran_type->unparseType(sourceReturn, returnTypeInfo);
        curprint(" ");
      }
    }

    if (procedureHeader->isBlockData()) {
      curprint(typeOfFunction);
      if (blockDataHasName(procedureHeader)) {
        curprint(" ");
        curprint(procedureHeader->get_name().str());
      }
    } else {
      curprint(typeOfFunction + " ");
      curprint(procedureHeader->get_name().str());

      SgUnparse_Info ninfo2(info);
      ninfo2.set_inArgList();

      curprint("(");
      unparseFunctionArgs(procedureHeader, ninfo2);
      curprint(")");
    }

    unparseBindAttribute(procedureHeader);

    // Unparse the result(<name>) suffix if present
    if (procedureHeader->get_result_name() != nullptr &&
        procedureHeader->get_name() !=
            procedureHeader->get_result_name()->get_name()) {
      curprint(" result(");
      curprint(procedureHeader->get_result_name()->get_name());
      curprint(")");
    }

    // Output 1 new line so that new statements will appear on their own line
    // after the SgProgramHeaderStatement declaration.
    unp->cur.insert_newline(1);
  }
}

void FortranCodeGeneration_locatedNode::unparseFuncDefnStmt(
    SgStatement *stmt, SgUnparse_Info &info) {
  SgFunctionDefinition *funcdefn_stmt = isSgFunctionDefinition(stmt);
  ASSERT_not_null(funcdefn_stmt);
  if (funcdefn_stmt->get_body() == nullptr) {
    std::cerr << "Error: Fortran function definition has no body" << std::endl;
    ROSE_ABORT();
  }

  // Unparse any comments of directives attached to the SgFunctionParameterList
  ASSERT_not_null(funcdefn_stmt->get_declaration());
  if (funcdefn_stmt->get_declaration()->get_parameterList() != nullptr)
    unparseAttachedPreprocessingInfo(
        funcdefn_stmt->get_declaration()->get_parameterList(), info,
        PreprocessingInfo::before);

  info.set_SkipFunctionDefinition();
  SgStatement *declstmt = funcdefn_stmt->get_declaration();

  // DQ (10/15/2006): Mark that we are unparsing a function declaration (or
  // member function declaration) this will help us know when to trim the "::"
  // prefix from the name qualiciation.  The "::" global scope qualifier is not
  // used in function declarations, but is used for function calls.
  info.set_declstatement_ptr(nullptr);
  info.set_declstatement_ptr(funcdefn_stmt->get_declaration());

  // Emit numeric labels after leading comments for Fortran defining
  // declarations. Leading preprocessing information is emitted by the common
  // statement path before dispatching this defining declaration.
  unparseStatementNumbers(declstmt, info);
  if (isSgProgramHeaderStatement(declstmt) != nullptr) {
    unparseProgHdrStmt(declstmt, info);
  } else {
    ASSERT_not_null(isSgProcedureHeaderStatement(declstmt));
    unparseProcHdrStmt(declstmt, info);
  }

  // Un-mark that we are unparsing a function declaration (or member function
  // declaration)
  info.set_declstatement_ptr(nullptr);

  info.unset_SkipFunctionDefinition();
  SgUnparse_Info ninfo(info);

  // now the body of the function
  unparseStatement(funcdefn_stmt->get_body(), ninfo);

  // Unparse any comments of directives attached to the SgFunctionParameterList
  unparseAttachedPreprocessingInfo(
      funcdefn_stmt->get_declaration()->get_parameterList(), info,
      PreprocessingInfo::after);
}

void FortranCodeGeneration_locatedNode::unparseFunctionParameterDeclaration(
    SgFunctionDeclaration *funcdecl_stmt, SgInitializedName *initializedName,
    bool /*outputParameterDeclaration*/, SgUnparse_Info &) {
  ASSERT_not_null(funcdecl_stmt);
  ASSERT_not_null(initializedName);

  curprint(initializedName->get_name().str());
}

void FortranCodeGeneration_locatedNode::unparseFunctionArgs(
    SgFunctionDeclaration *funcdecl_stmt, SgUnparse_Info &info) {
  ASSERT_not_null(funcdecl_stmt);

  SgInitializedNamePtrList::iterator p = funcdecl_stmt->get_args().begin();
  while (p != funcdecl_stmt->get_args().end()) {
    ASSERT_not_null(*p);
    if ((*p)->get_name().is_null()) {
      std::cerr << "Error: unnamed dummy argument in Fortran procedure"
                << std::endl;
      ROSE_ABORT();
    }
    unparseFunctionParameterDeclaration(funcdecl_stmt, *p, false, info);

    // Move to the next argument
    p++;

    // Check if this is the last argument (output a "," separator if not)
    if (p != funcdecl_stmt->get_args().end()) {
      curprint(",");
    }
  }
}

//-----------------------------------------------------------------------------------
//  void Unparse_ExprStmt::unparse_helper
//
//  prints out the function parameters in a function declaration or function
//  call. For now, all parameters are printed on one line since there is no
//  file information for each parameter.
//-----------------------------------------------------------------------------------
void FortranCodeGeneration_locatedNode::unparse_helper(
    SgFunctionDeclaration *funcdecl_stmt, SgUnparse_Info &info) {
  ASSERT_not_null(funcdecl_stmt);

  // output the function name
  curprint(funcdecl_stmt->get_name().str());

  SgUnparse_Info ninfo2(info);
  ninfo2.set_inArgList();

  // DQ (5/14/2003): Never output the class definition in the argument list.
  // Using this C++ constraint avoids building a more complex mechanism to turn
  // it off.
  ninfo2.set_SkipClassDefinition();

  // DQ (9/9/2016): These should have been setup to be the same.
  ninfo2.set_SkipEnumDefinition();

  curprint("(");
  unparseFunctionArgs(funcdecl_stmt, ninfo2);
  curprint(")");
}

void FortranCodeGeneration_locatedNode::unparseClassDeclStmt_derivedType(
    SgStatement *stmt, SgUnparse_Info &info) {
  SgDerivedTypeStatement *classdecl_stmt = isSgDerivedTypeStatement(stmt);
  ASSERT_not_null(classdecl_stmt);
  if (classdecl_stmt->get_name().is_null()) {
    std::cerr << "Error: Fortran derived type declaration has no name"
              << std::endl;
    ROSE_ABORT();
  }
  if (!classdecl_stmt->isForward() &&
      classdecl_stmt->get_definition() == nullptr) {
    std::cerr << "Error: defining Fortran derived type has no definition"
              << std::endl;
    ROSE_ABORT();
  }

  if (!classdecl_stmt->isForward() && classdecl_stmt->get_definition() &&
      !info.SkipClassDefinition()) {
    SgUnparse_Info ninfox(info);

    ninfox.unset_SkipSemiColon();

    // DQ (6/13/2007): Set to null before resetting to non-null value
    ninfox.set_declstatement_ptr(nullptr);
    ninfox.set_declstatement_ptr(classdecl_stmt);

    unparseStatement(classdecl_stmt->get_definition(), ninfox);
    unparseStatementNumbersSupport(classdecl_stmt->get_end_numeric_label(),
                                   info);

    curprint("END TYPE ");
    curprint(classdecl_stmt->get_name().str());

    ASSERT_not_null(unp);
    unp->cur.insert_newline(1);
  } else {
    if (!info.inEmbeddedDecl()) {
      SgUnparse_Info ninfo(info);
      ASSERT_not_null(classdecl_stmt->get_parent());
      SgClassDefinition *cdefn =
          isSgClassDefinition(classdecl_stmt->get_parent());

      if (cdefn && cdefn->get_declaration()->get_class_type() ==
                       SgClassDeclaration::e_class)
        ninfo.set_CheckAccess();

      unp->u_sage->printSpecifier(classdecl_stmt, ninfo);
      info.set_access_attribute(ninfo.get_access_attribute());
    }

    info.unset_inEmbeddedDecl();

    curprint("TYPE ");

    if (classdecl_stmt->get_declarationModifier()
            .get_accessModifier()
            .isPublic()) {
      // The PUBLIC keyword is only permitted within Modules
      if (SageInterface::getEnclosingModuleStatement(classdecl_stmt) !=
          nullptr) {
        curprint(", PUBLIC");
      } else {
        fprintf(stderr,
                "REX_UNPARSE_INVARIANT[fortran-access]: public derived type "
                "occurs outside a module\n");
        ROSE_ABORT();
      }
    }

    if (classdecl_stmt->get_declarationModifier()
            .get_accessModifier()
            .isPrivate()) {
      // The PRIVATE keyword is only permitted within Modules
      if (SageInterface::getEnclosingModuleStatement(classdecl_stmt) !=
          nullptr) {
        curprint(", PRIVATE");
      } else {
        fprintf(stderr,
                "REX_UNPARSE_INVARIANT[fortran-access]: private derived type "
                "occurs outside a module\n");
        ROSE_ABORT();
      }
    }

    if (classdecl_stmt->get_declarationModifier().get_typeModifier().isBind()) {
      // The BIND keyword is only permitted within Modules
      if (SageInterface::getEnclosingModuleStatement(classdecl_stmt) !=
          nullptr) {
        // I think that bind implies "BIND(C)"
        curprint(", BIND(C)");
      } else {
        fprintf(stderr,
                "REX_UNPARSE_INVARIANT[fortran-bind]: BIND derived type "
                "occurs outside a module\n");
        ROSE_ABORT();
      }
    }

    std::string extendsName;
    if (SgClassDefinition *def = classdecl_stmt->get_definition()) {
      if (!def->get_inheritances().empty()) {
        if (SgBaseClass *base = def->get_inheritances().front()) {
          if (SgClassDeclaration *baseDecl = base->get_base_class()) {
            extendsName = baseDecl->get_name().str();
          }
        }
      }
    }

    if (!extendsName.empty() || classdecl_stmt->get_declarationModifier()
                                    .get_typeModifier()
                                    .isExtends()) {
      // The EXTENDS keyword is only permitted within Modules
      if (SageInterface::getEnclosingModuleStatement(classdecl_stmt) !=
          nullptr) {
        if (extendsName.empty()) {
          std::cerr << "Error: EXTENDS modifier has no base-class edge"
                    << std::endl;
          ROSE_ABORT();
        }
        curprint(", EXTENDS(");
        curprint(extendsName);
        curprint(")");
      } else {
        std::cerr << "Error: EXTENDS modifier occurs outside a module"
                  << std::endl;
        ROSE_ABORT();
      }
    }

    if (classdecl_stmt->get_declarationModifier()
            .get_typeModifier()
            .isAbstract()) {
      curprint(", ABSTRACT");
    }

    // DQ (8/28/2010): I think this is require to separate type attribute
    // specifiers from the name of the type.
    curprint(" :: ");

    curprint(classdecl_stmt->get_name().str());
  }
}

void FortranCodeGeneration_locatedNode::unparseClassDeclStmt_module(
    SgStatement *stmt, SgUnparse_Info &info) {
  SgModuleStatement *classdecl_stmt = isSgModuleStatement(stmt);
  ASSERT_not_null(classdecl_stmt);
  if (classdecl_stmt->get_name().is_null()) {
    std::cerr << "Error: Fortran MODULE declaration has no name" << std::endl;
    ROSE_ABORT();
  }
  if (!classdecl_stmt->isForward() &&
      classdecl_stmt->get_definition() == nullptr) {
    std::cerr << "Error: defining Fortran MODULE declaration has no definition"
              << std::endl;
    ROSE_ABORT();
  }

  if (!classdecl_stmt->isForward() && classdecl_stmt->get_definition() &&
      !info.SkipClassDefinition()) {
    SgUnparse_Info ninfox(info);

    ninfox.unset_SkipSemiColon();

    // DQ (6/13/2007): Set to null before resetting to non-null value
    ninfox.set_declstatement_ptr(nullptr);
    ninfox.set_declstatement_ptr(classdecl_stmt);

    unparseStatement(classdecl_stmt->get_definition(), ninfox);

    unparseStatementNumbersSupport(classdecl_stmt->get_end_numeric_label(),
                                   info);
    curprint("END MODULE ");
    curprint(classdecl_stmt->get_name().str());

    ASSERT_not_null(unp);
    unp->cur.insert_newline(1);
    unp->cur.insert_newline(2); // FMZ
  } else {
    if (!info.inEmbeddedDecl()) {
      SgUnparse_Info ninfo(info);
      ASSERT_not_null(classdecl_stmt->get_parent());
      SgClassDefinition *cdefn =
          isSgClassDefinition(classdecl_stmt->get_parent());

      if (cdefn && cdefn->get_declaration()->get_class_type() ==
                       SgClassDeclaration::e_class)
        ninfo.set_CheckAccess();

      unp->u_sage->printSpecifier(classdecl_stmt, ninfo);
      info.set_access_attribute(ninfo.get_access_attribute());
    }

    info.unset_inEmbeddedDecl();

    curprint("MODULE ");
    curprint(classdecl_stmt->get_name().str());

    SgName nm = classdecl_stmt->get_name();
  }
}

void FortranCodeGeneration_locatedNode::unparseClassDefnStmt(
    SgStatement *stmt, SgUnparse_Info &info) {
  SgClassDefinition *classdefn_stmt = isSgClassDefinition(stmt);
  ASSERT_not_null(classdefn_stmt);
  if (classdefn_stmt->get_packingAlignment() != 0) {
    std::cerr << "Error: C/C++ packing alignment reached a Fortran type "
                 "definition"
              << std::endl;
    ROSE_ABORT();
  }

  SgUnparse_Info ninfo(info);

  ninfo.set_SkipClassDefinition();
  ninfo.set_SkipEnumDefinition();
  ninfo.set_SkipSemiColon();

  ASSERT_not_null(classdefn_stmt->get_declaration());

  if (isSgModuleStatement(classdefn_stmt->get_declaration()) != nullptr) {
    unparseClassDeclStmt_module(classdefn_stmt->get_declaration(), ninfo);
  } else {
    unparseClassDeclStmt_derivedType(classdefn_stmt->get_declaration(), ninfo);
  }

  ninfo.unset_SkipSemiColon();
  ninfo.unset_SkipClassDefinition();
  ninfo.unset_SkipEnumDefinition();

  SgNamedType *saved_context = ninfo.get_current_context();

  ASSERT_not_null(
      classdefn_stmt->get_declaration()->get_firstNondefiningDeclaration());
  SgClassDeclaration *classDeclaration = isSgClassDeclaration(
      classdefn_stmt->get_declaration()->get_firstNondefiningDeclaration());
  ASSERT_not_null(classDeclaration->get_type());

  // DQ (6/13/2007): Set to null before resetting to non-null value
  ninfo.set_current_context(nullptr);
  ninfo.set_current_context(classDeclaration->get_type());

  // Fortran derived types may use EXTENDS to model single inheritance.

  // DQ (9/28/2004): Turn this back on as the only way to prevent this from
  // being unparsed! DQ (11/22/2003): Control unparsing of the {} part of the
  // definition
  if (info.SkipBasicBlock() == false) {
    ninfo.set_isUnsetAccess();
    unp->cur.format(classdefn_stmt, info, FORMAT_BEFORE_BASIC_BLOCK1);
    unp->cur.format(classdefn_stmt, info, FORMAT_AFTER_BASIC_BLOCK1);

    if (classdefn_stmt->get_isSequence()) {
      unparseStatementNumbersSupport(nullptr, info);
      curprint("sequence");
      unp->u_sage->curprint_newline();
    }

    if (classdefn_stmt->get_isPrivate()) {
      unparseStatementNumbersSupport(nullptr, info);
      curprint("private");
      unp->u_sage->curprint_newline();
    }

    SgDeclarationStatementPtrList::iterator pp =
        classdefn_stmt->get_members().begin();

    while (pp != classdefn_stmt->get_members().end()) {
      unparseStatement((*pp), ninfo);
      // curprint("! Comment in unparseClassDefnStmt() (after each member
      // declaration) \n");
      pp++;
    }

    // DQ (3/17/2005): This helps handle cases such as class foo { #include
    // "constant_code.h" }
    ASSERT_not_null(classdefn_stmt->get_startOfConstruct());
    ASSERT_not_null(classdefn_stmt->get_endOfConstruct());

    unparseAttachedPreprocessingInfo(classdefn_stmt, info,
                                     PreprocessingInfo::inside);
  }

  // DQ (6/13/2007): Set to null before resetting to non-null value
  ninfo.set_current_context(nullptr);
  ninfo.set_current_context(saved_context);
}

void FortranCodeGeneration_locatedNode::unparseAllocateStatement(
    SgStatement *stmt, SgUnparse_Info &info) {
  SgAllocateStatement *s = isSgAllocateStatement(stmt);
  ASSERT_not_null(s);
  SgExprListExp *exprList = s->get_expr_list();
  ASSERT_not_null(exprList);

  const SgExpressionPtrList &allocationExpressions =
      exprList->get_expressions();
  if (allocationExpressions.empty() ||
      (allocationExpressions.size() == 1 &&
       isSgTypeExpression(allocationExpressions.front()) != nullptr)) {
    std::cerr << "Error: Fortran ALLOCATE statement has no allocation objects"
              << std::endl;
    ROSE_ABORT();
  }

  curprint("allocate( ");

  SgExpressionPtrList exprs = exprList->get_expressions();
  bool firstIsType = false;
  size_t startIndex = 0;
  if (!exprs.empty()) {
    if (isSgTypeExpression(exprs.front()) != nullptr) {
      firstIsType = true;
      unparseExpression(exprs.front(), info);
      curprint(" :: ");
      startIndex = 1;
    }
  }

  bool needComma = false;
  for (size_t i = startIndex; i < exprs.size(); ++i) {
    if (needComma) {
      curprint(", ");
    }
    unparseExpression(exprs[i], info);
    needComma = true;
  }

  if (s->get_stat_expression() != nullptr) {
    curprint(needComma || firstIsType ? ", STAT = " : "STAT = ");
    unparseExpression(s->get_stat_expression(), info);
    needComma = true;
  }

  if (s->get_errmsg_expression() != nullptr) {
    curprint(needComma || firstIsType ? ", ERRMSG = " : "ERRMSG = ");
    unparseExpression(s->get_errmsg_expression(), info);
    needComma = true;
  }

  if (s->get_source_expression() != nullptr) {
    curprint(needComma || firstIsType ? ", SOURCE = " : "SOURCE = ");
    unparseExpression(s->get_source_expression(), info);
    needComma = true;
  }

  if (s->get_mold_expression() != nullptr) {
    curprint(needComma || firstIsType ? ", MOLD = " : "MOLD = ");
    unparseExpression(s->get_mold_expression(), info);
    needComma = true;
  }

  if (s->get_stream_expression() != nullptr) {
    curprint(needComma || firstIsType ? ", STREAM = " : "STREAM = ");
    unparseExpression(s->get_stream_expression(), info);
    needComma = true;
  }

  if (s->get_pinned_expression() != nullptr) {
    curprint(needComma || firstIsType ? ", PINNED = " : "PINNED = ");
    unparseExpression(s->get_pinned_expression(), info);
  }

  curprint(" )");
  unp->cur.insert_newline(1);
}

void FortranCodeGeneration_locatedNode::unparseDeallocateStatement(
    SgStatement *stmt, SgUnparse_Info &info) {
  SgDeallocateStatement *s = isSgDeallocateStatement(stmt);
  ASSERT_not_null(s);
  SgExprListExp *exprList = s->get_expr_list();
  ASSERT_not_null(exprList);
  if (exprList->get_expressions().empty()) {
    std::cerr << "Error: Fortran DEALLOCATE statement has no objects"
              << std::endl;
    ROSE_ABORT();
  }

  curprint("deallocate( ");

  // DQ (3/28/2017): Eliminate warning of overloaded virtual function in base
  // class (from Clang). unparseExprList(exprList, info, false /*paren*/);
  unparseExprList(exprList, info);

  if (s->get_stat_expression() != nullptr) {
    curprint(", STAT = ");
    unparseExpression(s->get_stat_expression(), info);
  }

  if (s->get_errmsg_expression() != nullptr) {
    curprint(", ERRMSG = ");
    unparseExpression(s->get_errmsg_expression(), info);
  }

  curprint(" )");
  unp->cur.insert_newline(1);
}

void FortranCodeGeneration_locatedNode::unparseWithTeamStatement(
    SgStatement *stmt, SgUnparse_Info &info) {
  SgCAFWithTeamStatement *withTeamStmt = isSgCAFWithTeamStatement(stmt);
  ASSERT_not_null(withTeamStmt);

  curprint("WITHTEAM ");

  SgVarRefExp *teamIdRef = withTeamStmt->get_teamId();

  SgInitializedName *teamDecl = teamIdRef->get_symbol()->get_declaration();

  curprint(teamDecl->get_name().str());

  unp->cur.insert_newline(1);

  // unparse the body
  SgBasicBlock *body = isSgBasicBlock(withTeamStmt->get_body());
  ASSERT_not_null(body);

  unparseBasicBlockStmt(body, info);

  curprint("END WITHTEAM ");

  curprint(teamDecl->get_name().str());
  unp->cur.insert_newline(1);
}

void FortranCodeGeneration_locatedNode::curprint(const std::string &str) const {
#if USE_RICE_FORTRAN_WRAPPING
  unp->emitFortranText(str);

#else // ! USE_RICE_FORTRAN_WRAPPING

  // FMZ (3/22/2010) added fortran continue line support
  bool is_fortran90 = (unp->currentFile != nullptr) &&
                      (unp->currentFile->get_F90_only() ||
                       unp->currentFile->get_CoArrayFortran_only());

  int str_len = str.size();
  int curr_line_len = unp->cur.current_col();

  if (is_fortran90 && curr_line_len != 0 &&
      (str_len + curr_line_len) > MAX_F90_LINE_LEN) {
    unp->u_sage->curprint("&");
    unp->cur.insert_newline(1);
  }

  if (str_len <= MAX_F90_LINE_LEN || str[0] == '#' || str[0] == '!') {
    unp->u_sage->curprint(str);
  } else {
    for (int stridx = 0; stridx < str_len; stridx += MAX_F90_LINE_LEN) {
      std::string substring =
          str.substr(stridx, std::min(MAX_F90_LINE_LEN, str_len - stridx));
      unp->u_sage->curprint(substring);
      if (stridx + MAX_F90_LINE_LEN < str_len) {
        unp->u_sage->curprint("&");
        unp->cur.insert_newline(1);
      }
    }
  }

#endif // USE_RICE_FORTRAN_WRAPPING
}

void FortranCodeGeneration_locatedNode::unparseOmpPrefix(SgUnparse_Info &info) {
  unp->requireFortranDirectiveKind(Unparser::FortranDirectiveKind::openmp);
  if (unp->cur.get_compact_output()) {
    unp->cur.begin_compact_directive();
  }
  curprint(string("!$omp "));
}

static bool fortranOmpDirectiveUsesExplicitEnd(SgStatement *stmt) {
  ASSERT_not_null(stmt);
  switch (stmt->get_directive_end_kind()) {
  case SgStatement::e_directive_end_explicit:
    return true;
  case SgStatement::e_directive_end_implicit:
  case SgStatement::e_directive_end_not_applicable:
    return false;
  default:
    std::cerr << "REX_UNPARSE_INVARIANT[fortran-directive-end-kind]: "
                 "statement="
              << stmt->class_name() << " has invalid value="
              << static_cast<int>(stmt->get_directive_end_kind()) << std::endl;
    ROSE_ABORT();
  }
}

static bool fortranOmpCanEmitEndDirectivePrefix(SgStatement *stmt) {
  ASSERT_not_null(stmt);

  switch (stmt->variantT()) {
  case V_SgOmpParallelStatement:
  case V_SgOmpCriticalStatement:
  case V_SgOmpSectionsStatement:
  case V_SgOmpMasterStatement:
  case V_SgOmpMaskedStatement:
  case V_SgOmpOrderedStatement:
  case V_SgOmpWorkshareStatement:
  case V_SgOmpSingleStatement:
  case V_SgOmpTaskStatement:
  case V_SgOmpTaskgroupStatement:
  case V_SgOmpDoStatement:
  case V_SgOmpForSimdStatement:
  case V_SgOmpAtomicStatement:
  case V_SgOmpTargetStatement:
  case V_SgOmpTargetDataStatement:
  case V_SgOmpTargetParallelStatement:
  case V_SgOmpTargetParallelForStatement:
  case V_SgOmpTargetParallelForSimdStatement:
  case V_SgOmpTargetParallelLoopStatement:
  case V_SgOmpTargetSimdStatement:
  case V_SgOmpTargetTeamsStatement:
  case V_SgOmpTargetTeamsDistributeStatement:
  case V_SgOmpTargetTeamsDistributeSimdStatement:
  case V_SgOmpTargetTeamsWorkdistributeStatement:
  case V_SgOmpTargetTeamsDistributeParallelForStatement:
  case V_SgOmpTargetTeamsDistributeParallelForSimdStatement:
  case V_SgOmpTeamsStatement:
  case V_SgOmpTeamsDistributeStatement:
  case V_SgOmpTeamsDistributeSimdStatement:
  case V_SgOmpTeamsDistributeParallelForStatement:
  case V_SgOmpTeamsDistributeParallelForSimdStatement:
  case V_SgOmpDistributeSimdStatement:
  case V_SgOmpDistributeParallelForStatement:
  case V_SgOmpDistributeParallelForSimdStatement:
  case V_SgOmpParallelMasterStatement:
  case V_SgOmpMasterTaskloopStatement:
  case V_SgOmpMasterTaskloopSimdStatement:
  case V_SgOmpMaskedTaskloopStatement:
  case V_SgOmpMaskedTaskloopSimdStatement:
  case V_SgOmpParallelMasterTaskloopStatement:
  case V_SgOmpParallelMasterTaskloopSimdStatement:
  case V_SgOmpMetadirectiveStatement:
  case V_SgOmpLoopStatement:
  case V_SgOmpTaskloopStatement:
  case V_SgOmpTaskloopSimdStatement:
  case V_SgOmpWorkdistributeStatement:
    return true;
  default:
    return false;
  }
}

static bool fortranOmpMovesClausesToEnd(SgStatement *stmt) {
  ASSERT_not_null(stmt);

  switch (stmt->variantT()) {
  case V_SgOmpSectionsStatement:
  case V_SgOmpSingleStatement:
  case V_SgOmpWorkshareStatement:
  case V_SgOmpDoStatement:
  case V_SgOmpForSimdStatement:
  case V_SgOmpTargetTeamsWorkdistributeStatement:
  case V_SgOmpWorkdistributeStatement:
  case V_SgOmpLoopStatement:
  case V_SgOmpTaskloopStatement:
    return true;
  default:
    return false;
  }
}

static bool fortranOmpUsesDoDirectiveSpelling(SgStatement *stmt) {
  ASSERT_not_null(stmt);
  switch (stmt->get_omp_fortran_spelling()) {
  case SgStatement::e_omp_fortran_spelling_do:
    return true;
  case SgStatement::e_omp_fortran_spelling_not_applicable:
    std::cerr << "REX_UNPARSE_INVARIANT[fortran-openmp-spelling]: statement="
              << stmt->class_name() << " has no exact DO-family source spelling"
              << std::endl;
    ROSE_ABORT();
  default:
    std::cerr << "REX_UNPARSE_INVARIANT[fortran-openmp-spelling]: statement="
              << stmt->class_name() << " has invalid value="
              << static_cast<int>(stmt->get_omp_fortran_spelling())
              << std::endl;
    ROSE_ABORT();
  }
}

static SgStatement *fortranOmpDirectiveLexicalOwner(SgStatement *stmt) {
  ASSERT_not_null(stmt);
  if (SgOmpContextSelector *selector =
          isSgOmpContextSelector(stmt->get_parent())) {
    SgOmpContextSelectorSet *set =
        isSgOmpContextSelectorSet(selector->get_parent());
    SgOmpClause *clause =
        set != nullptr ? isSgOmpClause(set->get_parent()) : nullptr;
    SgStatement *owner =
        clause != nullptr ? isSgStatement(clause->get_parent()) : nullptr;
    if (selector->get_selector_kind() !=
            SgOmpClause::e_omp_context_trait_construct ||
        selector->get_construct_directive() != stmt || set == nullptr ||
        (isSgOmpMatchClause(clause) == nullptr &&
         isSgOmpWhenClause(clause) == nullptr) ||
        owner == nullptr) {
      std::cerr
          << "REX_UNPARSE_INVARIANT[fortran-openmp-nested-directive-owner]: "
             "construct-selector directive has no exact lexical owner"
          << std::endl;
      ROSE_ABORT();
    }
    return owner;
  }
  if (SgOmpWhenClause *when = isSgOmpWhenClause(stmt->get_parent())) {
    SgOmpClauseList *list = isSgOmpClauseList(when->get_parent());
    SgStatement *owner =
        list != nullptr ? isSgStatement(list->get_parent()) : nullptr;
    const SgOmpClausePtrList *clauses =
        list != nullptr ? &list->get_clauses() : nullptr;
    if (when->get_variant_directive() != stmt || list == nullptr ||
        owner == nullptr || OmpSupport::getOmpClauseList(owner) != list ||
        clauses == nullptr ||
        std::find(clauses->begin(), clauses->end(), when) == clauses->end()) {
      std::cerr
          << "REX_UNPARSE_INVARIANT[fortran-openmp-nested-directive-owner]: "
             "WHEN variant directive has no exact lexical owner"
          << std::endl;
      ROSE_ABORT();
    }
    return owner;
  }
  return stmt;
}

// Just skip nowait and copyprivate clauses for Fortran
void FortranCodeGeneration_locatedNode::unparseOmpBeginDirectiveClauses(
    SgStatement *stmt, SgUnparse_Info &info) {
  ASSERT_not_null(stmt);
  unp->requireFortranDirectiveKind(Unparser::FortranDirectiveKind::openmp);
  SgSourceFile *source_file = info.get_current_source_file();
  if (source_file == nullptr || source_file != unp->currentFile ||
      !source_file->get_Fortran_only()) {
    std::cerr << "REX_UNPARSE_INVARIANT[fortran-openmp-clause-context]: "
                 "directive has no exact active Fortran source file"
              << std::endl;
    ROSE_ABORT();
  }
  if (info.get_language() != SgFile::e_Fortran_language) {
    std::cerr << "REX_UNPARSE_INVARIANT[fortran-openmp-clause-context]: "
                 "directive has a non-Fortran language context"
              << std::endl;
    ROSE_ABORT();
  }
  SgScopeStatement *activeScope = info.get_current_scope();
  SgSourceFile *activeScopeSource =
      activeScope != nullptr
          ? SageInterface::getEnclosingSourceFile(activeScope)
          : nullptr;
  if (activeScope == nullptr || activeScopeSource != source_file) {
    std::cerr << "REX_UNPARSE_INVARIANT[fortran-openmp-clause-context]: "
                 "directive has no exact source scope"
              << std::endl;
    std::cerr << "REX_UNPARSE_DETAIL[fortran-openmp-clause-context]: "
                 "active-scope="
              << activeScope << "/"
              << (activeScope != nullptr ? activeScope->class_name() : "<null>")
              << " active-scope-source=" << activeScopeSource << std::endl;
    ROSE_ABORT();
  }
  SgStatement *lexicalOwner = fortranOmpDirectiveLexicalOwner(stmt);
  SgScopeStatement *directiveScope = lexicalOwner->get_scope();
  SgSourceFile *directiveSource =
      directiveScope != nullptr
          ? SageInterface::getEnclosingSourceFile(directiveScope)
          : nullptr;
  if (directiveScope == nullptr || directiveSource != source_file) {
    std::cerr << "REX_UNPARSE_INVARIANT[fortran-openmp-clause-context]: "
                 "directive has no exact source scope"
              << std::endl;
    std::cerr << "REX_UNPARSE_DETAIL[fortran-openmp-clause-context]: directive="
              << stmt << "/" << stmt->class_name()
              << " active-source=" << source_file
              << " directive-scope=" << directiveScope << "/"
              << (directiveScope != nullptr ? directiveScope->class_name()
                                            : "<null>")
              << " directive-source=" << directiveSource << std::endl;
    ROSE_ABORT();
  }
  // optional clauses
  SgOmpDeclareVariantStatement *declare_variant_stmt =
      isSgOmpDeclareVariantStatement(stmt);
  SgOmpBeginDeclareVariantStatement *begin_declare_variant_stmt =
      isSgOmpBeginDeclareVariantStatement(stmt);
  SgOmpDeclareMapperStatement *mapper_stmt =
      isSgOmpDeclareMapperStatement(stmt);
  const SgOmpClausePtrList *clause_ptr_list = nullptr;
  if (SgOmpClauseBodyStatement *body_stmt = isSgOmpClauseBodyStatement(stmt)) {
    clause_ptr_list =
        &requiredFortranOmpClauses(body_stmt, body_stmt->get_clause_list());
  } else if (isSgOmpDeclareSimdStatement(stmt)) {
    clause_ptr_list = &isSgOmpDeclareSimdStatement(stmt)->get_clauses();
  } else if (declare_variant_stmt != nullptr) {
    clause_ptr_list = &declare_variant_stmt->get_clauses();
  } else if (begin_declare_variant_stmt != nullptr) {
    clause_ptr_list = &begin_declare_variant_stmt->get_clauses();
  } else if (mapper_stmt != nullptr) {
    clause_ptr_list = &mapper_stmt->get_clauses();
  } else if (isSgOmpDeclareTargetStatement(stmt)) {
    clause_ptr_list = &isSgOmpDeclareTargetStatement(stmt)->get_clauses();
  } else if (isSgOmpTaskwaitStatement(stmt)) {
    clause_ptr_list = &isSgOmpTaskwaitStatement(stmt)->get_clauses();
  } else if (SgOmpClauseStatement *clause_stmt = isSgOmpClauseStatement(stmt)) {
    clause_ptr_list =
        &requiredFortranOmpClauses(clause_stmt, clause_stmt->get_clause_list());
  } else if (isSgOmpRequiresStatement(stmt)) {
    clause_ptr_list = &isSgOmpRequiresStatement(stmt)->get_clauses();
  } else if (isSgOmpAssumesStatement(stmt)) {
    clause_ptr_list = &isSgOmpAssumesStatement(stmt)->get_clauses();
  } else if (isSgOmpBeginAssumesStatement(stmt)) {
    clause_ptr_list = &isSgOmpBeginAssumesStatement(stmt)->get_clauses();
  } else if (SgOmpGroupprivateStatement *groupprivate_stmt =
                 isSgOmpGroupprivateStatement(stmt)) {
    clause_ptr_list = &requiredFortranOmpClauses(
        groupprivate_stmt, groupprivate_stmt->get_clause_list());
  }

  const bool has_explicit_end = fortranOmpDirectiveUsesExplicitEnd(stmt);
  if (has_explicit_end && !fortranOmpCanEmitEndDirectivePrefix(stmt)) {
    std::cerr << "Error: unsupported explicit END form for "
              << stmt->class_name() << std::endl;
    ROSE_ABORT();
  }

  if (mapper_stmt != nullptr) {
    if (mapper_stmt->get_mapper_type() == nullptr ||
        mapper_stmt->get_mapper_variable() == nullptr) {
      std::cerr << "Error: Fortran DECLARE MAPPER lacks a type or mapper "
                   "variable"
                << std::endl;
      ROSE_ABORT();
    }
    curprint(" (");
    switch (mapper_stmt->get_identifier()) {
    case SgOmpClause::e_omp_declare_mapper_identifier_default:
      if (mapper_stmt->get_identifier_is_explicit()) {
        curprint("default: ");
      }
      break;
    case SgOmpClause::e_omp_declare_mapper_identifier_user:
      if (!mapper_stmt->get_identifier_is_explicit() ||
          mapper_stmt->get_user_defined_identifier() == nullptr) {
        std::cerr << "Error: user-named Fortran DECLARE MAPPER has no "
                     "explicit identifier"
                  << std::endl;
        ROSE_ABORT();
      }
      unparseExpression(mapper_stmt->get_user_defined_identifier(), info);
      curprint(": ");
      break;
    case SgOmpClause::e_omp_declare_mapper_identifier_unspecified:
      break;
    default:
      ROSE_ABORT();
    }
    if (mapper_stmt->get_mapper_type() != nullptr) {
      SgType *mapper_type = nullptr;
      if (SgTypeExpression *type_expr =
              isSgTypeExpression(mapper_stmt->get_mapper_type())) {
        mapper_type = type_expr->get_represented_type()->stripType(
            SgType::STRIP_MODIFIER_TYPE | SgType::STRIP_REFERENCE_TYPE |
            SgType::STRIP_RVALUE_REFERENCE_TYPE | SgType::STRIP_TYPEDEF_TYPE);
      } else {
        mapper_type = mapper_stmt->get_mapper_type()->get_type()->stripType(
            SgType::STRIP_MODIFIER_TYPE | SgType::STRIP_REFERENCE_TYPE |
            SgType::STRIP_RVALUE_REFERENCE_TYPE | SgType::STRIP_TYPEDEF_TYPE);
      }

      // OpenMP Fortran declare mapper syntax uses the bare derived-type name
      // here rather than a full TYPE(...) declaration specifier.
      if (SgClassType *class_type = isSgClassType(mapper_type)) {
        curprint(class_type->get_name().getString());
      } else {
        unparseExpression(mapper_stmt->get_mapper_type(), info);
      }
    }
    if (mapper_stmt->get_mapper_variable() != nullptr) {
      if (mapper_stmt->get_mapper_type() != nullptr) {
        curprint(" :: ");
      }
      unparseExpression(mapper_stmt->get_mapper_variable(), info);
    }
    curprint(")");
  }

  if (clause_ptr_list != nullptr) {
    SgOmpClausePtrList::const_iterator i;
    bool first_clause = true;
    const bool move_nowait_to_end = fortranOmpMovesClausesToEnd(stmt) &&
                                    has_explicit_end &&
                                    fortranOmpCanEmitEndDirectivePrefix(stmt);
    for (i = clause_ptr_list->begin(); i != clause_ptr_list->end(); i++) {
      SgOmpClause *c_clause = *i;
      ASSERT_not_null(c_clause);
      if ((isSgOmpNowaitClause(c_clause) ||
           isSgOmpCopyprivateClause(c_clause)) &&
          move_nowait_to_end) {
        continue;
      }
      SgUnparse_Info clause_info(info);
      if (first_clause) {
        curprint(" ");
        first_clause = false;
      }
      unparseOmpClause(c_clause, clause_info);
    }
  }
  if (isSgOmpDeclareTargetStatement(stmt)) {
    const SgOmpClause::omp_when_context_kind_enum device_type_kind =
        isSgOmpDeclareTargetStatement(stmt)->get_device_type_kind();
    if (device_type_kind != SgOmpClause::e_omp_when_context_kind_unknown) {
      curprint(" ");
      curprint("device_type(");
      switch (device_type_kind) {
      case SgOmpClause::e_omp_when_context_kind_host:
        curprint("host");
        break;
      case SgOmpClause::e_omp_when_context_kind_nohost:
        curprint("nohost");
        break;
      case SgOmpClause::e_omp_when_context_kind_any:
        curprint("any");
        break;
      default:
        ROSE_ABORT();
      }
      curprint(")");
    }
  }
}

// Only unparse nowait or copyprivate clauses here
void FortranCodeGeneration_locatedNode::unparseOmpEndDirectiveClauses(
    SgStatement *stmt, SgUnparse_Info &info) {
  ASSERT_not_null(stmt);
  unp->requireFortranDirectiveKind(Unparser::FortranDirectiveKind::openmp);
  if (!fortranOmpDirectiveUsesExplicitEnd(stmt)) {
    return;
  }
  if (!fortranOmpCanEmitEndDirectivePrefix(stmt)) {
    std::cerr << "Error: unsupported explicit OpenMP END directive for "
              << stmt->class_name() << std::endl;
    ROSE_ABORT();
  }
  // optional clauses
  if (SgOmpClauseBodyStatement *body_stmt = isSgOmpClauseBodyStatement(stmt)) {
    const SgOmpClausePtrList &clause_ptr_list =
        requiredFortranOmpClauses(body_stmt, body_stmt->get_clause_list());
    SgOmpClausePtrList::const_iterator i;
    bool first_clause = true;
    const bool single_space_nowait = isSgOmpSectionsStatement(stmt) != nullptr;
    const bool move_clauses_to_end = fortranOmpMovesClausesToEnd(stmt);
    for (i = clause_ptr_list.begin(); i != clause_ptr_list.end(); i++) {
      SgOmpClause *c_clause = *i;
      if (move_clauses_to_end && (isSgOmpNowaitClause(c_clause) ||
                                  isSgOmpCopyprivateClause(c_clause))) {
        if (first_clause) {
          if (!(isSgOmpNowaitClause(c_clause) && single_space_nowait)) {
            curprint(" ");
          }
          first_clause = false;
        }
        unparseOmpClause(c_clause, info);
      }
    }
  }
  unp->u_sage->curprint_newline();
}

void FortranCodeGeneration_locatedNode::unparseOmpThreadprivateStatement(
    SgStatement *stmt, SgUnparse_Info &info) {
  ASSERT_not_null(stmt);
  SgOmpThreadprivateStatement *s = isSgOmpThreadprivateStatement(stmt);
  ASSERT_not_null(s);
  Unparser::FortranDirectiveContextGuard directiveContext(
      unp, Unparser::FortranDirectiveKind::openmp);
  unparseOmpDirectivePrefixAndName(stmt, info);
  curprint(string(" ("));
  SgExpressionPtrList::iterator p = s->get_variables().begin();
  while (p != s->get_variables().end()) {
    ASSERT_not_null(*p);
    unparseExpression(*p, info);

    ++p;
    if (p != s->get_variables().end()) {
      curprint(",");
    }
  }
  curprint(string(")"));
  unp->u_sage->curprint_newline();
}

void FortranCodeGeneration_locatedNode::unparseOmpFlushStatement(
    SgStatement *stmt, SgUnparse_Info &info) {
  ASSERT_not_null(stmt);
  SgOmpFlushStatement *s = isSgOmpFlushStatement(stmt);
  ASSERT_not_null(s);
  Unparser::FortranDirectiveContextGuard directiveContext(
      unp, Unparser::FortranDirectiveKind::openmp);

  unparseOmpDirectivePrefixAndName(stmt, info);
  if (!requiredFortranOmpClauses(s, s->get_clause_list()).empty()) {
    unparseOmpBeginDirectiveClauses(stmt, info);
  }
  if (s->get_variables().size() > 0) {
    curprint(string(" ("));
  }
  SgExpressionPtrList::const_iterator p = s->get_variables().begin();
  while (p != s->get_variables().end()) {
    ASSERT_not_null(*p);
    unparseExpression(*p, info);

    ++p;
    if (p != s->get_variables().end()) {
      curprint(",");
    }
  }
  if (s->get_variables().size() > 0) {
    curprint(string(")"));
  }
  unp->u_sage->curprint_newline();
}

void FortranCodeGeneration_locatedNode::unparseOmpEndDirectivePrefixAndName(
    SgStatement *stmt, SgUnparse_Info &info) {
  ASSERT_not_null(stmt);
  unp->requireFortranDirectiveKind(Unparser::FortranDirectiveKind::openmp);
  if (!fortranOmpDirectiveUsesExplicitEnd(stmt)) {
    return;
  }
  if (!fortranOmpCanEmitEndDirectivePrefix(stmt)) {
    std::cerr << "Error: unsupported explicit OpenMP END directive for "
              << stmt->class_name() << std::endl;
    ROSE_ABORT();
  }
  unp->u_sage->curprint_newline();
  switch (stmt->variantT()) {
  case V_SgOmpParallelStatement: {
    unparseOmpPrefix(info);
    curprint(string("end parallel"));
    break;
  }
  case V_SgOmpCriticalStatement: {
    unparseOmpPrefix(info);
    curprint(string("end critical"));
    if (isSgOmpCriticalStatement(stmt)->get_name().getString() != "") {
      curprint(string(" "));
      curprint(string("("));
      curprint(isSgOmpCriticalStatement(stmt)->get_name().getString());
      curprint(string(")"));
    }
    break;
  }
  case V_SgOmpSectionsStatement: {
    unparseOmpPrefix(info);
    curprint(string("end sections"));
    break;
  }
  case V_SgOmpMasterStatement: {
    unparseOmpPrefix(info);
    curprint(string("end master"));
    break;
  }
  case V_SgOmpMaskedStatement: {
    unparseOmpPrefix(info);
    curprint(string("end masked"));
    break;
  }
  case V_SgOmpOrderedStatement: {
    unparseOmpPrefix(info);
    curprint(string("end ordered"));
    break;
  }
  case V_SgOmpLoopStatement: {
    unparseOmpPrefix(info);
    curprint(string("end loop"));
    break;
  }
  case V_SgOmpTaskloopStatement: {
    unparseOmpPrefix(info);
    curprint(string("end taskloop"));
    break;
  }
  case V_SgOmpTaskloopSimdStatement: {
    unparseOmpPrefix(info);
    curprint(string("end taskloop simd"));
    break;
  }
  case V_SgOmpTargetDataStatement: {
    unparseOmpPrefix(info);
    curprint(string("end target data"));
    break;
  }
  case V_SgOmpTargetParallelStatement: {
    unparseOmpPrefix(info);
    curprint(string("end target parallel"));
    break;
  }
  case V_SgOmpTargetParallelForStatement: {
    unparseOmpPrefix(info);
    curprint(fortranOmpUsesDoDirectiveSpelling(stmt)
                 ? string("end target parallel do")
                 : string("end target parallel for"));
    break;
  }
  case V_SgOmpTargetParallelForSimdStatement: {
    unparseOmpPrefix(info);
    curprint(fortranOmpUsesDoDirectiveSpelling(stmt)
                 ? string("end target parallel do simd")
                 : string("end target parallel for simd"));
    break;
  }
  case V_SgOmpTargetParallelLoopStatement: {
    unparseOmpPrefix(info);
    curprint(string("end target parallel loop"));
    break;
  }
  case V_SgOmpTargetSimdStatement: {
    unparseOmpPrefix(info);
    curprint(string("end target simd"));
    break;
  }
  case V_SgOmpTargetTeamsStatement: {
    unparseOmpPrefix(info);
    curprint(string("end target teams"));
    break;
  }
  case V_SgOmpTargetTeamsDistributeStatement: {
    unparseOmpPrefix(info);
    curprint(string("end target teams distribute"));
    break;
  }
  case V_SgOmpTargetTeamsDistributeSimdStatement: {
    unparseOmpPrefix(info);
    curprint(string("end target teams distribute simd"));
    break;
  }
  case V_SgOmpTargetTeamsWorkdistributeStatement: {
    unparseOmpPrefix(info);
    curprint(string("end target teams workdistribute"));
    break;
  }
  case V_SgOmpTargetTeamsDistributeParallelForStatement: {
    unparseOmpPrefix(info);
    curprint(fortranOmpUsesDoDirectiveSpelling(stmt)
                 ? string("end target teams distribute parallel do")
                 : string("end target teams distribute parallel for"));
    break;
  }
  case V_SgOmpTargetTeamsDistributeParallelForSimdStatement: {
    unparseOmpPrefix(info);
    curprint(fortranOmpUsesDoDirectiveSpelling(stmt)
                 ? string("end target teams distribute parallel do simd")
                 : string("end target teams distribute parallel for simd"));
    break;
  }
  case V_SgOmpTeamsStatement: {
    unparseOmpPrefix(info);
    curprint(string("end teams"));
    break;
  }
  case V_SgOmpTeamsDistributeStatement: {
    unparseOmpPrefix(info);
    curprint(string("end teams distribute"));
    break;
  }
  case V_SgOmpTeamsDistributeSimdStatement: {
    unparseOmpPrefix(info);
    curprint(string("end teams distribute simd"));
    break;
  }
  case V_SgOmpTeamsDistributeParallelForStatement: {
    unparseOmpPrefix(info);
    curprint(fortranOmpUsesDoDirectiveSpelling(stmt)
                 ? string("end teams distribute parallel do")
                 : string("end teams distribute parallel for"));
    break;
  }
  case V_SgOmpTeamsDistributeParallelForSimdStatement: {
    unparseOmpPrefix(info);
    curprint(fortranOmpUsesDoDirectiveSpelling(stmt)
                 ? string("end teams distribute parallel do simd")
                 : string("end teams distribute parallel for simd"));
    break;
  }
  case V_SgOmpDistributeParallelForStatement: {
    unparseOmpPrefix(info);
    curprint(fortranOmpUsesDoDirectiveSpelling(stmt)
                 ? string("end distribute parallel do")
                 : string("end distribute parallel for"));
    break;
  }
  case V_SgOmpDistributeSimdStatement: {
    unparseOmpPrefix(info);
    curprint(string("end distribute simd"));
    break;
  }
  case V_SgOmpDistributeParallelForSimdStatement: {
    unparseOmpPrefix(info);
    curprint(fortranOmpUsesDoDirectiveSpelling(stmt)
                 ? string("end distribute parallel do simd")
                 : string("end distribute parallel for simd"));
    break;
  }
  case V_SgOmpParallelMasterStatement: {
    unparseOmpPrefix(info);
    curprint(string("end parallel master"));
    break;
  }
  case V_SgOmpMasterTaskloopStatement: {
    unparseOmpPrefix(info);
    curprint(string("end master taskloop"));
    break;
  }
  case V_SgOmpMaskedTaskloopStatement: {
    unparseOmpPrefix(info);
    curprint(string("end masked taskloop"));
    break;
  }
  case V_SgOmpMasterTaskloopSimdStatement: {
    unparseOmpPrefix(info);
    curprint(string("end master taskloop simd"));
    break;
  }
  case V_SgOmpMaskedTaskloopSimdStatement: {
    unparseOmpPrefix(info);
    curprint(string("end masked taskloop simd"));
    break;
  }
  case V_SgOmpParallelMasterTaskloopStatement: {
    unparseOmpPrefix(info);
    curprint(string("end parallel master taskloop"));
    break;
  }
  case V_SgOmpParallelMasterTaskloopSimdStatement: {
    unparseOmpPrefix(info);
    curprint(string("end parallel master taskloop simd"));
    break;
  }
  case V_SgOmpWorkdistributeStatement: {
    unparseOmpPrefix(info);
    curprint(string("end workdistribute"));
    break;
  }
  case V_SgOmpWorkshareStatement: {
    unparseOmpPrefix(info);
    curprint(string("end workshare"));
    break;
  }
  case V_SgOmpSingleStatement: {
    unparseOmpPrefix(info);
    curprint(string("end single"));
    break;
  }
  case V_SgOmpTaskStatement: {
    unparseOmpPrefix(info);
    curprint(string("end task"));
    break;
  }
  case V_SgOmpTaskgroupStatement: {
    unparseOmpPrefix(info);
    curprint(string("end taskgroup"));
    break;
  }
  case V_SgOmpDoStatement: {
    unparseOmpPrefix(info);
    curprint(string("end do"));
    break;
  }
  case V_SgOmpForSimdStatement: {
    unparseOmpPrefix(info);
    curprint(string("end do simd"));
    break;
  }
  case V_SgOmpAtomicStatement: {
    unparseOmpPrefix(info);
    curprint(string("end atomic"));
    break;
  }
  case V_SgOmpTargetStatement: {
    unparseOmpPrefix(info);
    curprint(string("end target"));
    break;
  }
  case V_SgOmpMetadirectiveStatement: {
    unparseOmpPrefix(info);
    curprint(string("end metadirective"));
    break;
  }
  default:
    std::cerr << "Error: missing Fortran spelling for explicit OpenMP END "
                 "directive "
              << stmt->class_name() << std::endl;
    ROSE_ABORT();
  } // end switch
}

// OpenACC support
void FortranCodeGeneration_locatedNode::unparseAccPrefix(SgUnparse_Info &info) {
  unp->requireFortranDirectiveKind(Unparser::FortranDirectiveKind::openacc);
  if (unp->cur.get_compact_output()) {
    unp->cur.begin_compact_directive();
  }
  curprint(string("!$acc "));
}

void FortranCodeGeneration_locatedNode::unparseAccBeginDirectiveClauses(
    SgStatement *stmt, SgUnparse_Info &info) {
  ASSERT_not_null(stmt);
  unp->requireFortranDirectiveKind(Unparser::FortranDirectiveKind::openacc);
  const SgAccClausePtrList *clause_ptr_list = nullptr;
  if (SgAccClauseBodyStatement *bodystmt = isSgAccClauseBodyStatement(stmt)) {
    clause_ptr_list = &bodystmt->get_clauses();
  } else if (SgAccClauseStatement *clausestmt = isSgAccClauseStatement(stmt)) {
    clause_ptr_list = &clausestmt->get_clauses();
  }
  if (clause_ptr_list != nullptr) {
    for (SgAccClause *c_clause : *clause_ptr_list) {
      ASSERT_not_null(c_clause);
      unparseAccClause(c_clause, info);
    }
  }
}

static bool fortranAccRequiresEndProvenance(SgStatement *stmt) {
  ASSERT_not_null(stmt);
  switch (stmt->variantT()) {
  case V_SgAccParallelStatement:
  case V_SgAccParallelLoopStatement:
  case V_SgAccDataStatement:
  case V_SgAccKernelsStatement:
    return true;
  case V_SgAccAtomicStatement: {
    SgAccAtomicStatement *atomic = isSgAccAtomicStatement(stmt);
    ASSERT_not_null(atomic);
    for (SgAccClause *clause : atomic->get_clauses()) {
      if (clause == nullptr || clause->get_parent() != atomic) {
        std::cerr << "REX_UNPARSE_INVARIANT[openacc-clause-owner]: atomic "
                     "directive has a null or misowned clause"
                  << std::endl;
        ROSE_ABORT();
      }
      if (isSgAccCaptureClause(clause) != nullptr) {
        return true;
      }
    }
    return false;
  }
  default:
    return false;
  }
}

void FortranCodeGeneration_locatedNode::unparseAccEndDirectivePrefixAndName(
    SgStatement *stmt, SgUnparse_Info &info) {
  ASSERT_not_null(stmt);
  unp->requireFortranDirectiveKind(Unparser::FortranDirectiveKind::openacc);
  unp->u_sage->curprint_newline();
  unparseAccPrefix(info);
  switch (stmt->variantT()) {
  case V_SgAccParallelStatement:
    curprint(string("end parallel"));
    break;
  case V_SgAccParallelLoopStatement:
    curprint(string("end parallel loop"));
    break;
  case V_SgAccDataStatement:
    curprint(string("end data"));
    break;
  case V_SgAccKernelsStatement:
    curprint(string("end kernels"));
    break;
  case V_SgAccAtomicStatement:
    curprint(string("end atomic"));
    break;
  default:
    std::cerr << "Error: missing Fortran spelling for explicit OpenACC END "
                 "directive "
              << stmt->class_name() << std::endl;
    ROSE_ABORT();
  }
}

static bool fortranAccEmitsExplicitEnd(SgStatement *stmt) {
  ASSERT_not_null(stmt);
  switch (stmt->get_directive_end_kind()) {
  case SgStatement::e_directive_end_explicit:
    return true;
  case SgStatement::e_directive_end_implicit:
    if (isSgAccParallelLoopStatement(stmt) == nullptr) {
      std::cerr << "REX_UNPARSE_INVARIANT[fortran-directive-end-kind]: "
                   "statement="
                << stmt->class_name() << " cannot omit its source END directive"
                << std::endl;
      ROSE_ABORT();
    }
    return false;
  case SgStatement::e_directive_end_not_applicable:
    if (fortranAccRequiresEndProvenance(stmt)) {
      std::cerr << "REX_UNPARSE_INVARIANT[fortran-directive-end-kind]: "
                   "statement="
                << stmt->class_name() << " has no exact source END provenance"
                << std::endl;
      ROSE_ABORT();
    }
    return false;
  default:
    std::cerr << "REX_UNPARSE_INVARIANT[fortran-directive-end-kind]: "
                 "statement="
              << stmt->class_name() << " has invalid value="
              << static_cast<int>(stmt->get_directive_end_kind()) << std::endl;
    ROSE_ABORT();
  }
}

void FortranCodeGeneration_locatedNode::unparseAccGenericStatement(
    SgStatement *stmt, SgUnparse_Info &info) {
  ASSERT_not_null(stmt);
  // Source END provenance is part of the directive's typed identity. Validate
  // it before emitting the begin directive or traversing its body so malformed
  // directives cannot be hidden by an unrelated descendant failure.
  const bool emit_explicit_end = fortranAccEmitsExplicitEnd(stmt);
  {
    Unparser::FortranDirectiveContextGuard directiveContext(
        unp, Unparser::FortranDirectiveKind::openacc);
    unparseAccDirectivePrefixAndName(stmt, info);
    unparseAccBeginDirectiveClauses(stmt, info);
    unp->u_sage->curprint_newline();
  }

  if (SgAccBodyStatement *b_stmt = isSgAccBodyStatement(stmt)) {
    SgUnparse_Info ninfo(info);
    unparseStatement(b_stmt->get_body(), ninfo);
  }

  if (emit_explicit_end) {
    Unparser::FortranDirectiveContextGuard directiveContext(
        unp, Unparser::FortranDirectiveKind::openacc);
    unparseAccEndDirectivePrefixAndName(stmt, info);
    unp->u_sage->curprint_newline();
  }
}

void FortranCodeGeneration_locatedNode::unparseOmpDoStatement(
    SgStatement *stmt, SgUnparse_Info &info) {
  ASSERT_not_null(stmt);
  SgOmpDoStatement *d_stmt = isSgOmpDoStatement(stmt);
  ASSERT_not_null(d_stmt);

  {
    Unparser::FortranDirectiveContextGuard directiveContext(
        unp, Unparser::FortranDirectiveKind::openmp);
    unparseOmpDirectivePrefixAndName(stmt, info);
    unparseOmpBeginDirectiveClauses(stmt, info);
    unp->u_sage->curprint_newline();
  }

  SgUnparse_Info ninfo(info);
  if (d_stmt->get_body()) {
    unparseStatement(d_stmt->get_body(), ninfo);
  } else {
    cerr << "Error: empty body for:" << stmt->class_name() << " is not allowed!"
         << endl;
    ROSE_ABORT();
  }

  {
    Unparser::FortranDirectiveContextGuard directiveContext(
        unp, Unparser::FortranDirectiveKind::openmp);
    // unparse the end directive and name
    unparseOmpEndDirectivePrefixAndName(stmt, info);

    // unparse the end directive's clause
    unparseOmpEndDirectiveClauses(stmt, info);
  }
}
