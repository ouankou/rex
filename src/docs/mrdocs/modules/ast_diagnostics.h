// -*- c++ -*-

#ifndef ROSE_DOCS_MODULES_AST_DIAGNOSTICS_H
#define ROSE_DOCS_MODULES_AST_DIAGNOSTICS_H

/** @brief Description of AST Diagnostics within ROSE
 *
 * The AST diagnostics support testing and reporting on the AST. Analysis is separated into consistency tests, warnings about
 * questionable AST configurations, and statistical properties of the AST.
 *
 * - Consistency tests are pass/fail and all are required to pass to verify correctness of the AST.
 * - Warning reports describe non-fatal irregularities that may affect future analyses. The unparser will generally generate
 *   good code from an AST which can still contain errors (e.g. the unparser does not require that the symbols in the symbol
 *   table be correct, but many forms of program analysis might use this information).
 * - Statistical properties report coverage and distribution of IR nodes to aid debugging.
 *
 * @section ast_diagnostics_consistency AST Consistency Tests
 *
 * Current tests are pass/fail and test the following properties:
 * - Parent pointers are verified correct.
 * - File info objects are verified to be correct (none are set to default values such as filename \"NULL_FILE\", line 0,
 *   column 0) except for compiler generated IR nodes.
 * - Pointers to file info objects are verified to be valid pointers.
 * - All file info objects are verified to be unique (non-shared).
 * - The type of a SgFunctionRefExp is always a SgFunctionType.
 * - ...
 *
 * @subsection ast_diagnostics_future_tests Future Consistency Tests
 *
 * There are a number of tests planned for the near future:
 * - SgInitializedName diagnostics.
 * - Correct configuration of SgExpressionRoot (SgReturnStmt).
 * - Complete list in ROSE/proposals/TODO.txt.
 *
 * @section ast_diagnostics_warnings AST Warnings
 *
 * There are a number of warnings that are output if certain patterns of IR usage are found:
 * - Uniqueness of file info objects in AST.
 *
 * @subsection ast_diagnostics_future_warnings Future Warnings
 *
 * A number of warnings are planned for the near future:
 * - Correct usage of SgInitializedName.
 * - etc. (see list in TODO.txt).
 *
 * @section ast_diagnostics_stats AST Statistical Properties
 *
 * The AST has about 240 IR nodes and this section is focused on the statistical usage of IR nodes within the AST. Current
 * statistical properties of the AST supported are:
 * - Percentage of use of each IR node in the AST.
 *
 * @subsection ast_diagnostics_future_stats Future Statistical Properties
 *
 * It is easy to imagine additional AST properties to report. Future work will implement a number of tests useful for
 * debugging the AST:
 * - Percentage of AST that passes all tests.
 * - Percentage of AST which generates warnings.
 *
 * Note: This section was previously excluded from generated documentation because it was incomplete.
 */
struct astDiagnostics {};

/** @brief AST Diagnostics
 *
 * This is the AST diagnostics tests/warnings/statistics reporting mechanism.
 *
 * The AST diagnostics mechanism provides simple ways to test and report on the AST using a traversal over the AST. The
 * library provides:
 * - Consistency tests (AST is tested to be a valid AST).
 * - Warning reports (AST non-fatal irregularities are reported, possible failures in future stricter tests).
 * - Statistics reporting (helpful information about the AST).
 *
 * Metadata:
 * - Authors: Schordan and Quinlan
 * - Version: 0.5
 * - Date: $Date: 2006/04/24 00:21:31 $
 * - Bug: No known bugs.
 * - Warning: Documentation is still incomplete.
 * - TODO: Finish documentation.
 *
 * @section ast_diagnostics_classes_consistency AstConsistencyTests
 *
 * This class represents the internal structure of the AST diagnostics mechanism within ROSE.
 *
 * Note: Large parts of documentation contained in ROSE/src/midend/astDiagnostics/astDiagnostics.docs.
 *
 * This class encapsulates the complexities associated with the testing of the AST.
 *
 * Internal: The exact separation between what is a pass/fail test and what is a warning is not established yet.
 *
 * @section ast_diagnostics_classes_warnings AstWarnings
 *
 * This class represents the internal structure of the AST diagnostics mechanism within ROSE.
 *
 * Note: Large parts of documentation contained in ROSE/src/midend/astDiagnostics/astDiagnostics.docs.
 *
 * This class encapsulates the complexities associated with the testing of the AST.
 *
 * Internal: The exact separation between what is a pass/fail test and what is a warning is not established yet.
 *
 * @section ast_diagnostics_classes_statistics AstStatistics
 *
 * This class represents the internal structure of the AST diagnostics mechanism within ROSE.
 *
 * Note: Large parts of documentation contained in ROSE/src/midend/astDiagnostics/astDiagnostics.docs.
 *
 * This class encapsulates the complexities associated with the testing of the AST.
 *
 * Internal: The exact separation between what is a pass/fail test and what is a warning is not established yet.
 *
 * More information is available in @ref astDiagnostics.
 *
 * See @ref rose_midend.
 */
struct AstDiagnosticsClasses {};

#endif
