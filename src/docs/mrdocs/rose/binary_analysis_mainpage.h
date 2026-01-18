// -*- c++ -*-

#ifndef ROSE_DOCS_ROSE_BINARY_ANALYSIS_MAINPAGE_H
#define ROSE_DOCS_ROSE_BINARY_ANALYSIS_MAINPAGE_H

/** @brief Binary analysis main page
 *
 * @note This documentation is intentionally incomplete. We plan to extend and
 * improve it as we review and fix existing documentation.
 *
 * @section mainpage_intro The ROSE library
 *
 * The ROSE compiler infrastructure provides an open-source library that users
 * can use to build tools that analyze and transform large-scale programs. These
 * tools use the ROSE library to perform static analysis, program and
 * domain-specific optimizations, arbitrary transformations, cyber-security
 * checks, and more. The library can analyze and transform source code written
 * in C, C++, UPC, Fortran, OpenMP, Java, Python, Ada, Jovial, or PHP, and it
 * can analyze binaries that employ the Intel x86, PowerPC, ARM, MIPS, M68k,
 * JVM, or CIL instruction sets.
 *
 * In prose, one spells ROSE in all capital letters. The ROSE naming conventions
 * dictate the use of PascalCase for namespaces and types, and kebab-case for
 * command lines, thus one spells the main @ref Rose "namespace" "Rose" and
 * command-line tools use "rose".
 *
 * @section mainpage_audience Audience
 *
 * This documentation is intended for users who write ROSE-based analysis or
 * transformation tools that link with an installed version of the ROSE library.
 * This document places an emphasis on parts of the ROSE library particularly
 * suited to binary analysis. The ROSE core developers have other
 * developer-specific documentation available to them.
 *
 * Goals for this documentation:
 * - Document library entities in a way users find useful.
 * - Exclude entities that serve only internal purposes.
 * - Exclude undocumented entities to encourage better documentation.
 * - Exclude entities that do not follow ROSE naming conventions.
 * - Generate documentation without warnings or errors.
 *
 * @section mainpage_naming Naming conventions
 *
 * @subsection mainpage_naming_namespaces The Rose namespace
 *
 * All parts of the ROSE library exist inside the @ref Rose namespace. ROSE
 * includes some third-party libraries that use other top-level namespaces, such
 * as `Sawyer`. Entities used to analyze binaries exist inside the
 * `Rose::BinaryAnalysis` namespace. The naming convention requires namespaces
 * to use PascalCase.
 *
 * @note At this time, many ROSE library symbols pollute the root namespace or
 * appear in other non-Rose namespaces. These violate the naming convention and
 * are excluded from this documentation until fixed.
 *
 * @subsection mainpage_naming_capitalization Capitalization of C++ symbols
 *
 * Namespaces and types in the ROSE public API use PascalCase; macros begin with
 * "ROSE_" and use uppercase with underscores; everything else uses lower camel
 * case. Acronyms are treated as words for capitalization, except API, AST, CFG,
 * and IO when they appear alone.
 *
 * @subsection mainpage_headers Header files
 *
 * The ROSE build system installs header files with path names that mirror the
 * namespace hierarchy. Include these headers using
 * `#include <Rose/....h>`. Base names of public include files match the primary
 * C++ symbol and end with ".h". Each namespace has a header that recursively
 * includes all header files under that namespace and a "BasicTypes.h" header
 * that declares common forward declarations and pointer types.
 *
 * Note: Some documentation refers to headers under an "AstNodes" directory.
 * These headers serve as inputs to the Rosebud code generation system for AST
 * node types and users should include them without the path. For instance, if
 * the documentation mentions "AstNodes/BinaryAnalysis/SgAsmInstruction.h", the
 * user should include "SgAsmInstruction.h" instead.
 *
 * @section mainpage_binaryanalysis Binary analysis
 *
 * Binary analysis progresses by ingesting low-level code like machine code and
 * byte code from executables, parsing and decoding instructions, analyzing
 * them, and unparsing to more human-consumable formats like assembly listings.
 * The ROSE library locates all binary analysis under the `Rose::BinaryAnalysis`
 * namespace.
 *
 * @section mainpage_resources Other resources
 *
 * ROSE comes with a number of binary analysis tools. All tools understand a
 * "--help" command-line switch to show their manpage-style documentation.
 *
 * Source code and installation instructions are hosted at
 * https://github.com/rose-compiler/rose.
 *
 * @section mainpage_generation Documentation generation
 *
 * MrDocs generates this document as part of the ROSE documentation build. You
 * can generate a local copy by running `scripts/generate-api-documentation`
 * from the top of the ROSE source tree.
 */
struct binary_analysis_mainpage {};

#endif
