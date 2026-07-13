/* Core unparser declarations shared by the backend implementation units. */

#ifndef UNPARSER_H
#define UNPARSER_H

#include <map>

#include "unparse_format.h"

#include "unparser_opt.h"

// #include <algorithm>

#include "name_qualification_support.h"

#include "unparseCxx_types.h"
// #include "unparseCxx_templates.h"

#include "modified_sage.h"

#include "unparseCxx.h"

#include "unparse_debug.h"

#include "unparseFortran.h"

#include "unparseFortran_types.h"

#include "UnparserDelegate.h"

class Unparser_Nameq;
class Unparse_Debug;
class Unparse_MOD_SAGE;
class PreprocessingInfo;
class TokenUnparseFrontierContext;
struct TokenUnparseFrontierFileContext;

using UnparsePreprocessingInfoRewriteMap =
    std::map<const PreprocessingInfo *, std::string>;

// Whether to use Rice's code to wrap long lines in Fortran.
#define USE_RICE_FORTRAN_WRAPPING                                              \
  1 // Enable fixed/free form wrapping to preserve Fortran line-length rules.

// Maximum line lengths for Fortran fixed source form and free source form, per
// the F90 specification.
#if USE_RICE_FORTRAN_WRAPPING
#define MAX_F90_LINE_LEN_FIXED 72
#define MAX_F90_LINE_LEN_FREE 132
#else
#define MAX_F90_LINE_LEN 132
#endif

#define KAI_NONSTD_IOSTREAM 1

#if USE_OLD_MECHANISM_OF_HANDLING_PREPROCESSING_INFO
// I think this is part of the connection to lex support for comments
// extern ROSEAttributesList* getPreprocessorDirectives( char *fileName);
#endif

/*
 * This function is used by the SgNode object to connect the unparser to the
 * AST.
 *
 * This function hides the complexity of generating a string from any subtree
 * of the AST (represented by a SgNode*).
 *
 * This function uses the standard stringstream mechanism in C++ to
 * convert the stream output to a string.
 */
std::string get_output_filename(SgFile &file);

/** @brief Backend C and C++ code generator.
 *
 * This class represents the backend C++ code generator within ROSE. It is
 * separated from the AST IR so that it can be more easily developed as a
 * separate modular piece of ROSE.
 *
 * This is the source code generator. It traverses the AST (not using the newer
 * traversal mechanisms) and generates C++ source code from the constructs
 * within the AST. Special attention is given to formatting the generated code.
 *
 * Note: Large parts of documentation contained in ROSE/src/unparser.docs.
 *
 * Internal: This class could be simplified now that comments and CPP directives
 * are a part of the AST.
 *
 * Metadata:
 * - Authors: Lee, Quinlan, Schordan, ...
 * - Version: 0.5 cvs-version-#: $Name: $
 * - Date: $Date: 2006/04/24 00:21:29 $
 * - Bug: No known bugs.
 * - Warning: Formatting still overly complex (can be simplified now that
 * comments are part of the AST).
 * - TODO: Finish documentation; make formatting easily tailorable to different
 * styles (low priority at the moment).
 */
class Unparser {
public:
  enum class FortranDirectiveKind { none, openmp, openacc };

  class FortranDirectiveContextGuard {
  public:
    FortranDirectiveContextGuard(Unparser *unparser, FortranDirectiveKind kind);
    ~FortranDirectiveContextGuard();

    FortranDirectiveContextGuard(const FortranDirectiveContextGuard &) = delete;
    FortranDirectiveContextGuard &
    operator=(const FortranDirectiveContextGuard &) = delete;

  private:
    Unparser *unparser_ = nullptr;
    FortranDirectiveKind kind_ = FortranDirectiveKind::none;
    FortranDirectiveKind previous_ = FortranDirectiveKind::none;
    bool active_ = false;
  };

  struct FortranLineWrapLayout {
    bool fixedFormat = false;
    int physicalColumns = 0;
    int textColumns = 0;
  };

  Unparse_Type *u_type;
  Unparser_Nameq *u_name;
  Unparse_Debug *u_debug;
  Unparse_MOD_SAGE *u_sage;
  Unparse_ExprStmt *u_exprStmt;

  // DQ (8/14/2007): I have added this here to be consistant, but I question if
  // this is a good design!
  UnparseFortran_type *u_fortran_type;
  FortranCodeGeneration_locatedNode *u_fortran_locatedNode;

public:
  //! Used to support unparsing of doubles and long double as x.0 instead of
  //! just x if they are whole number values.
  // bool zeroRemainder( long double doubleValue );

  //! holds all desired options for this unparser
  Unparser_Opt opt;

  //! used to index the preprocessor list
  int cur_index;

  //! The previous directive was a CPP statment (otherwise it was a comment)
  bool prevdir_was_cppDeclaration;

  // DQ (8/19/2007): Added simple access to the SgFile so that options specified
  // there are easily available. Using this data member a number of mechanism in
  // the unparser could be simplified to be more efficient (they currently
  // search bacj through the AST to get the SgFile).
  SgFile *currentFile;

  //! This is a cursor mechanism which is not encapsulated into the curprint()
  //! member function.
public:
  UnparseFormat cur;

public:
  //! delegate unparser that can be used to replace the output of this unparser
  UnparseDelegate *delegate;

private:
  // DQ (12/5/2006): Output information that can be used to colorize properties
  // of generated code (useful for debugging).
  int embedColorCodesInGeneratedCode;

  // DQ (12/5/2006): Output separate file containing source position information
  // for highlighting (useful for debugging).
  int generateSourcePositionCodes;

  NameQualificationContext ownedNameQualifications;
  NameQualificationContext *nameQualifications;
  const UnparsePreprocessingInfoRewriteMap *preprocessingInfoRewrites;
  const TokenUnparseFrontierContext *tokenUnparseFrontiers;
  struct PreprocessingInfoReceipt {
    const SgLocatedNode *owner;
    int relativePosition;
  };
  std::map<const PreprocessingInfo *, PreprocessingInfoReceipt>
      preprocessingInfoReceipts;
  FortranDirectiveKind fortranDirectiveKind;
  void emitFortranRawText(const std::string &text);

public:
  //! constructor
  Unparser(std::ostream *localStream, std::string filename, Unparser_Opt info,
           UnparseFormatHelp *h = nullptr, UnparseDelegate *delegate = nullptr,
           const UnparsePreprocessingInfoRewriteMap *rewrites = nullptr,
           NameQualificationContext *nameQualifications = nullptr,
           const TokenUnparseFrontierContext *tokenFrontiers = nullptr);

  //! destructor
  virtual ~Unparser();

  Unparser(const Unparser &) = delete;
  Unparser &operator=(const Unparser &) = delete;

  //! get the output stream wrapper
  UnparseFormat &get_output_stream();

  NameQualificationContext &get_name_qualification_context();

  std::string preprocessingInfoText(const PreprocessingInfo *info) const;
  void claimPreprocessingInfoReceipt(const PreprocessingInfo *record,
                                     const SgLocatedNode *owner,
                                     int relativePosition);
  const TokenUnparseFrontierFileContext &
  tokenUnparseFrontier(SgSourceFile *sourceFile) const;
  const TokenUnparseFrontierContext &tokenUnparseContext() const;

  FortranDirectiveKind getFortranDirectiveKind() const;
  void setFortranDirectiveKind(FortranDirectiveKind kind);
  void requireFortranDirectiveKind(FortranDirectiveKind kind) const;
  const char *fortranDirectiveContinuationPrefix(bool fixedFormat) const;
  FortranLineWrapLayout fortranLineWrapLayout() const;
  void emitFortranText(const std::string &text);
  void emitFortranComment(const std::string &text);
  void emitFortranCharacterLiteral(const std::string &value, char delimiter);

  //! true if SgLocatedNode is part of a transformation on the AST
  bool isPartOfTransformation(SgLocatedNode *n);

  //! true if SgLocatedNode is part of a compiler generated part of the AST (e.g
  //! template instatiation)
  bool isCompilerGenerated(SgLocatedNode *n);

  //! friend string globalUnparseToString ( SgNode* astNode );

  void unparseFile(SgSourceFile *file, SgUnparse_Info &info,
                   SgScopeStatement *unparseScope = nullptr);
  //! remove unneccessary white space to build a condensed string

  // DQ (12/5/2006): Output separate file containing source position information
  // for highlighting (useful for debugging).
  int get_embedColorCodesInGeneratedCode();
  int get_generateSourcePositionCodes();
  void set_embedColorCodesInGeneratedCode(int x);
  void set_generateSourcePositionCodes(int x);

  // DQ (8/7/2018): Refactored code for name qualification (so that we can call
  // it once before all files are unparsed (where we unparse multiple files
  // because fo the use of header file unparsing)).
  static void
  computeNameQualification(SgSourceFile *file,
                           NameQualificationContext &nameQualifications);
};

//! User callable function available if compilation using the backend compiler
//! is not required.
ROSE_DLL_API void unparseFile(
    SgFile *file, UnparseFormatHelp *unparseHelp = nullptr,
    UnparseDelegate *delegate = nullptr,
    SgScopeStatement *unparseScope = nullptr,
    const UnparsePreprocessingInfoRewriteMap *preprocessingInfoRewrites =
        nullptr,
    const std::string *outputFilenameOverride = nullptr,
    NameQualificationContext *nameQualifications = nullptr,
    TokenUnparseFrontierContext *tokenFrontiers = nullptr,
    unsigned int physicalFileOccurrence = 0);

//! User callable function available if compilation using the backend compiler
//! is not required.
ROSE_DLL_API void unparseIncludedFiles(
    SgProject *project, UnparseFormatHelp *unparseHelp = nullptr,
    UnparseDelegate *delegate = nullptr,
    UnparsePreprocessingInfoRewriteMap *preprocessingInfoRewrites = nullptr,
    NameQualificationContext *nameQualifications = nullptr,
    TokenUnparseFrontierContext *tokenFrontiers = nullptr);

//! User callable function available if compilation using the backend compiler
//! is not required.
ROSE_DLL_API void unparseProject(SgProject *project,
                                 UnparseFormatHelp *unparseHelp = nullptr,
                                 UnparseDelegate *delegate = nullptr);

//! Support for handling directories of files in ROSE (useful for code
//! generation).
void unparseDirectory(SgDirectory *directory,
                      UnparseFormatHelp *unparseHelp = nullptr,
                      UnparseDelegate *delegate = nullptr,
                      const UnparsePreprocessingInfoRewriteMap
                          *preprocessingInfoRewrites = nullptr,
                      NameQualificationContext *nameQualifications = nullptr,
                      TokenUnparseFrontierContext *tokenFrontiers = nullptr);

// DQ (1/19/2010): Added support for refactored handling directories of files.
//! Support for refactored handling directories of files.
void unparseFileList(SgFileList *fileList,
                     UnparseFormatHelp *unparseFormatHelp = nullptr,
                     UnparseDelegate *unparseDelegate = nullptr,
                     const UnparsePreprocessingInfoRewriteMap
                         *preprocessingInfoRewrites = nullptr,
                     NameQualificationContext *nameQualifications = nullptr,
                     const std::string *outputDirectoryOverride = nullptr,
                     TokenUnparseFrontierContext *tokenFrontiers = nullptr);

// DQ (10/1/2019): Adding support to generate SgSourceFile for individual header
// files on demand. This is required for the optimization of the header files
// because in this optimization all the header files will not be processed at
// one time in the operation to attach the CPP and comments to the AST. Instead
// we defer the transformations on the header files and make a note of what
// header files will be transformed, and then prepare the individual header
// files that we will transform by collecint CPP directives and comments and
// weaving them into those subsequences of the AST and then perform the defered
// transforamtion, and then unparse the header files.  This is a moderately
// complex operation.
SgSourceFile *buildSourceFileForHeaderFile(SgProject *project,
                                           std::string originalFileName);

#endif
