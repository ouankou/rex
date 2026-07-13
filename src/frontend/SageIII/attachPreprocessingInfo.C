
// tps (01/14/2010) : Switching from rose.h to sage3.
#include "attachPreprocessingInfo.h"

#include "sage3basic.h"

// DQ (1/7/2021): Added to support testing of the token stream availability.
#include "tokenStreamMapping.h"

// DQ (10/14/2010):  This should only be included by source files that require
// it. This fixed a reported bug which caused conflicts with configure-time
// macros (e.g. PACKAGE_BUGREPORT).
#include "rose_config.h"

#include <algorithm>
#include <unordered_map>

// DQ (12/31/2005): This is OK if not declared in a header file
using namespace std;

namespace {
void rosePhaseTrace(const char *phase) {
  if (getenv("ROSE_PHASE_TRACE") != nullptr) {
    fprintf(stderr, "ROSE_PHASE %s\n", phase);
    fflush(stderr);
  }
}

static SgSourceFile *
getPreprocessingAttachmentTraversalRoot(SgSourceFile *source_file) {
  if (source_file == nullptr || source_file->get_isHeaderFile() == false) {
    return source_file;
  }

  SgIncludeFile *include_file = source_file->get_associated_include_file();
  if (include_file == nullptr) {
    include_file = isSgIncludeFile(source_file->get_parent());
  }
  if (include_file == nullptr) {
    return source_file;
  }

  SgSourceFile *translation_unit =
      include_file->get_source_file_of_translation_unit();
  return translation_unit != nullptr ? translation_unit : source_file;
}

} // namespace

// DQ (11/28/2009): I think this is equivalent to "USE_ROSE"
// DQ (11/28/2008): What does this evaluate to???  Does this mix C++ constants
// with CPP values (does this make sense? Is "true" defined?) #if
// CAN_NOT_COMPILE_WITH_ROSE != true #if !CAN_NOT_COMPILE_WITH_ROSE

// Include files to get the current path
#include <unistd.h>

#include <sys/param.h>

// #include <iostream>
// #include <fstream>
// #include <string>

// DQ (11/11/2018): Added prototype to support debugging.
void generateGraphOfIncludeFiles(SgSourceFile *sourceFile,
                                 std::string filename);

// DQ (5/4/2020): Added directly here because it is required for this function.
typedef std::map<int, ROSEAttributesList *> AttributeMapType;

// DQ (12/3/2020): We sometimes want to read a file twice, and gather the
// comments and CPP directives twice, but the second time the file is read it is
// read so that it can build a file with a different name. So we need to specify
// the name of the file that we want the comments and CPP directives to
// eventually be attached to and not the one from which they were take.  This
// technique is used to support building a second file to be a dynamic library
// within the codeSegregation tool. DQ (4/5/2006): Older version not using the
// current preprocessing pipeline. This is the function to be called from the
// main function DQ: Now called by the SgFile constructor body (I think) void
// attachPreprocessingInfo(SgSourceFile *sageFilePtr)
void attachPreprocessingInfo(SgSourceFile *sageFilePtr,
                             const std::string &new_filename,
                             bool attach_to_ast) {
  ROSE_ASSERT(sageFilePtr != NULL);

  // DQ (02/20/2021): Using the performance tracking within ROSE.
  TimingPerformance timer_1("AST attachPreprocessingInfo:");

#define DEBUG_ATTACH_PREPROCESSOR_INFO 0

#if DEBUG_ATTACH_PREPROCESSOR_INFO
  printf("################################################################ \n");
  printf("################################################################ \n");
  printf("In attachPreprocessingInfo(): file    = %p = %s \n", sageFilePtr,
         sageFilePtr->get_sourceFileNameWithPath().c_str());
  printf(" --- unparse output filename                    = %s \n",
         sageFilePtr->get_unparse_output_filename().c_str());
  printf(" --- sageFilePtr->getFileName()                 = %s \n",
         sageFilePtr->getFileName().c_str());
  printf(" --- sageFilePtr->get_globalScope()             = %p \n",
         sageFilePtr->get_globalScope());
  printf(" --- sageFilePtr->get_unparse_output_filename() = %s \n",
         sageFilePtr->get_unparse_output_filename().c_str());
  printf(" --- new_filename                               = %s \n",
         new_filename.c_str());
  printf("################################################################ \n");
  printf("################################################################ \n");
#endif

  // DQ (11/18/2019): Check the flag that indicates that this SgSourceFile has
  // NOT yet had its CPP directives and comments added.
  ROSE_ASSERT(sageFilePtr->get_processedToIncludeCppDirectivesAndComments() ==
              false);

  // ROSEAttributesList* headerAttributes = getListOfAttributes(fileNameId);
  string filename = sageFilePtr->get_sourceFileNameWithPath();
  ROSEAttributesList *commentAndCppDirectiveList = NULL;

#if DEBUG_ATTACH_PREPROCESSOR_INFO
  printf(
      "Calling "
      "AttachPreprocessingInfoTreeTrav::buildCommentAndCppDirectiveList(): \n");
  printf("sageFilePtr->getFileName() = %s \n",
         sageFilePtr->getFileName().c_str());
  printf("filename                   = %s \n", filename.c_str());
  printf("new_filename               = %s \n", new_filename.c_str());
  // printf ("tokenVector.size() = %zu using filename     = %s
  // \n",getTokenStream(sageFilePtr).size(),filename.c_str());
#endif

  // DQ (1/4/2021): Adding support for comments and CPP directives and tokens
  // to use new_filename. DQ (7/4/2020): This function should be called only
  // for C/C++ source code. commentAndCppDirectiveList =
  // getPreprocessorDirectives(filename);
  // commentAndCppDirectiveList =
  // AttachPreprocessingInfoTreeTrav::buildCommentAndCppDirectiveList(filename);
  // commentAndCppDirectiveList =
  // AttachPreprocessingInfoTreeTrav::buildCommentAndCppDirectiveList(sageFilePtr,filename);
  rosePhaseTrace("attachPreprocessingInfo.buildList.begin");
  commentAndCppDirectiveList =
      AttachPreprocessingInfoTreeTrav::buildCommentAndCppDirectiveList(
          sageFilePtr, filename, new_filename);
  rosePhaseTrace("attachPreprocessingInfo.buildList.end");

  ROSE_ASSERT(commentAndCppDirectiveList != NULL);
  // sageFilePtr->get_preprocessorDirectivesAndCommentsList().insert()

#if DEBUG_ATTACH_PREPROCESSOR_INFO
  printf("Test after buildCommentAndCppDirectiveList(): "
         "sageFilePtr->getFileName() = %s tokenVector.size() = %zu \n",
         sageFilePtr->getFileName().c_str(),
         getTokenStream(sageFilePtr).size());
  printf("tokenVector.size() = %zu using filename     = %s \n",
         getTokenStream(sageFilePtr).size(), filename.c_str());
#endif

  // DQ (7/2/2020): Added assertion (fails for snippet tests).
  ROSE_ASSERT(sageFilePtr->get_preprocessorDirectivesAndCommentsList() != NULL);

  const std::string attributes_filename =
      new_filename.empty() ? filename : new_filename;
  sageFilePtr->get_preprocessorDirectivesAndCommentsList()->addList(
      attributes_filename, commentAndCppDirectiveList);

  // DQ (6/30/2020): Testing for token-based unparsing.
  ROSE_ASSERT(sageFilePtr->get_preprocessorDirectivesAndCommentsList() != NULL);
  ROSEAttributesListContainerPtr filePreprocInfo =
      sageFilePtr->get_preprocessorDirectivesAndCommentsList();

#if DEBUG_ATTACH_PREPROCESSOR_INFO
  printf("filePreprocInfo->getList().size() = %zu \n",
         filePreprocInfo->getList().size());
#endif

  // We should at least have the current files CPP/Comment/Token information
  // (even if it is an empty file).
  ROSE_ASSERT(filePreprocInfo->getList().size() > 0);

#if DEBUG_ATTACH_PREPROCESSOR_INFO
  printf("sageFilePtr->get_token_list().size()                                 "
         "      = %zu \n",
         sageFilePtr->get_token_list().size());
  printf("commentAndCppDirectiveList->get_rawTokenStream()->size()             "
         "      = %zu \n",
         commentAndCppDirectiveList->get_rawTokenStream()->size());
  printf("sageFilePtr->get_preprocessorDirectivesAndCommentsList()->getList()."
         "size() = %zu \n",
         sageFilePtr->get_preprocessorDirectivesAndCommentsList()
             ->getList()
             .size());
#endif
#if DEBUG_ATTACH_PREPROCESSOR_INFO
  printf("sageFilePtr->getFileName() = %s \n",
         sageFilePtr->getFileName().c_str());
  printf("tokenVector.size() = %zu using filename     = %s \n",
         getTokenStream(sageFilePtr).size(), filename.c_str());
  printf("tokenVector.size() = %zu using new_filename = %s \n",
         getTokenStream(sageFilePtr).size(), new_filename.c_str());
#endif

#ifndef CXX_IS_ROSE_CODE_GENERATION
  // DQ (7/6/2005): Introduce tracking of performance of ROSE.
  TimingPerformance timer_2("AST Comment and CPP Directive Processing:");

  if (attach_to_ast) {
    // Dummy attribute (nothing is done here since this is an empty class)
    AttachPreprocessingInfoTreeTraversalInheritedAttrribute inh;

    // DQ (4/19/2006): Now supporting either the collection or ALL comments and
    // CPP directives into header file AST nodes or just the collection of the
    // comments and CPP directives into the source file. printf
    // ("sageFilePtr->get_collectAllCommentsAndDirectives() = %s
    // \n",sageFilePtr->get_collectAllCommentsAndDirectives() ? "true" :
    // "false");

    // bool processAllFiles =
    // sageFilePtr->get_collectAllCommentsAndDirectives();

#if DEBUG_ATTACH_PREPROCESSOR_INFO
    // DQ (4/24/2021): Trying to debug the header file optimization support.
    printf("In attachPreprocessingInfo(): Skipping "
           "header_file_unparsing_optimization preamble \n");
#endif

    // DQ (6/2/2020): Change the API to pass in the CPP directives and comments
    // list. Also disable boolean processAllFiles since these are no longer
    // processed in the traversal (adding CPP directives and comments from each
    // file is a separate). AttachPreprocessingInfoTreeTrav
    // tt(sageFilePtr,processAllFiles);
    AttachPreprocessingInfoTreeTrav tt(sageFilePtr, commentAndCppDirectiveList);
    SgSourceFile *attachment_traversal_root =
        getPreprocessingAttachmentTraversalRoot(sageFilePtr);

    // DQ (12/19/2008): Added support for Fortran CPP files.
    // If this is a Fortran file requiring CPP processing then we want to call
    // traverse, instead of traverseWithinFile, so that the whole AST will be
    // processed (which is in a SgSourceFile using a name without the
    // "_preprocessed" suffix, though the statements in the file are marked with
    // a source position from the filename with the "_preprocessed" suffix).

    // DQ (4/24/2021): This is not used and generates a compiler warning.
    // bool requiresCPP = sageFilePtr->get_requires_C_preprocessor();

    // DQ (6/29/2020): This is now a simple traversal over the whole of the AST.
    rosePhaseTrace("attachPreprocessingInfo.attachTraversal.begin");
    tt.traverse(attachment_traversal_root, inh);
    rosePhaseTrace("attachPreprocessingInfo.attachTraversal.end");
  }

  // endif for ifndef  CXX_IS_ROSE_CODE_GENERATION
#endif

  // DQ (11/18/2019): Set the flag that indicates that this SgSourceFile has had
  // its CPP directives and comments added.
  sageFilePtr->set_processedToIncludeCppDirectivesAndComments(true);

  // DQ (1/7/2021): Get the token vector using the mechanism used in
  // buildTokenStreamMapping(). vector<stream_element*> tokenVector =
  // getTokenStream(sageFilePtr);

#if DEBUG_ATTACH_PREPROCESSOR_INFO
  // printf ("tokenVector.size() = %zu \n",tokenVector.size());
  printf("tokenVector.size() = %zu \n", getTokenStream(sageFilePtr).size());
#endif
}

// EOF
