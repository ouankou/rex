/****************************************************************************

Remarks on Unparsing and on attaching preprocessing information to AST nodes
----------------------------------------------------------------------------
Markus Kowarschik, 10/2002

The SgFile (always) constructor calls the function
void attachPreprocessingInfo(SgFile *sageFilePtr);
which in turn calls getPreprocessorDirectives (see above) and then
invokes a tree traversal in order to attach the preprocessor directives
(i.e., the preprocessingInfo objects) to located nodes in the AST.
(Currently, we only attach preprocessingInfo objects to SgStatement
objects.)

For this purpose, I added a new data member
attachedPreprocessingInfoType* attachedPreprocessingInfoPtr;
to the SgLocatedNode class. This is done in ROSETTA/src/node.C.

Furthermore, I added the corresponding access functions:
void addToAttachedPreprocessingInfo(preprocessingInfo *prepInfoPtr);
attachedPreprocessingInfoType* getAttachedPreprocessingInfo(void);
to the SgLocatedNode class. This is done in ROSETTA/Grammar/LocatedNode.code.

The tree traversal works as follows: whenever it hits a located node
(currently: a statement), it checks if there is preprocessing info the
line number of which is less or equal than the line number of the current
located node (currently: of the current statement). If this is the case,
the corresponding preprocessing info is attached to the current
located node (currently: before the current statement), unparse flag: "before".
All this is done in the evaluateInheritedAttribute member function of the
derived tree traversal class.

The evaluateSynthesizedAttribute member function deletes the list of
preprocessingInfo objects as soon as the traversal returns to a SgFile
object and attaches trailing preprocessing information to the last located
node (currently to the last statement) that has been visited in
the file (unparse flag: "after").

Node that the preprocessingInfo objects are always attached to AST nodes.
By switching the USE_OLD_MECHANISM_OF_HANDLING_PREPROCESSING_INFO flag,
you only change the mechanism which the unparser is based on!
If USE_OLD_MECHANISM_OF_HANDLING_PREPROCESSING_INFO is set to 1, then
the unparser simply ignores the preprocessingInfo objects that have
been attached to the AST nodes.

Problems with the handling of preprocessing information can be
found in the directory ROSE/TESTS/KnownBugs/AttachPreprocessingInfo.

****************************************************************************/

// This file implements the extraction of the attributes (comments and
// preprocessor directives) from the original source file and their insertion
// into the AST data structure.
// The idea is to re-introduce them (as correctly as possible) when the
// transformed AST is unparsed later on.

// #include "attachPreprocessingInfo.h"
// #include "sage3.h"
// tps (01/14/2010) : Switching from rose.h to sage3.
#include "sage3basic.h"

// DQ (10/14/2010):  This should only be included by source files that require
// it. This fixed a reported bug which caused conflicts with configure-time
// macros (e.g. PACKAGE_BUGREPORT).
#include "attachPreprocessingInfo.h"

#include "attachPreprocessingInfoTraversal.h"

#include "rose_config.h"

// DQ (9/15/2018): Associated header file for the class and member function
// declarations defined in this file. NOTE: this has been moved to be a new ROSE
// IR node. #include "headerFileSupportReport.h"

// DQ (9/26/2018): Added so that we can call the display function for
// TokenStreamSequenceToNodeMapping (for debugging).
#include "tokenStreamMapping.h"

// DQ (12/31/2005): This is OK if not declared in a header file
using namespace std;
using namespace Rose;

// Debug flag
#define DEBUG_ATTACH_PREPROCESSING_INFO 0

// DQ (6/17/2020): This appears to be required to avoid segfaults that will not
// print the failing assertion. #define ROSE_ASSERT assert

// DQ (8/23/2018): Adding function declaration to generate comments, CPP
// directives and the token stream.
void buildTokenStreamMapping(SgSourceFile *sourceFile);

// It is needed because otherwise, the default destructor breaks something.

AttachPreprocessingInfoTreeTrav::~AttachPreprocessingInfoTreeTrav() {
  // do nothing
}

// DQ (11/30/2008): Refactored this code out of the simpler function to isolate
// token stream handling.

// AttachPreprocessingInfoTreeTrav::AttachPreprocessingInfoTreeTrav(
// SgSourceFile* file, bool includeDirectivesAndCommentsFromAllFiles )
AttachPreprocessingInfoTreeTrav::AttachPreprocessingInfoTreeTrav(
    SgSourceFile *file, ROSEAttributesList *listOfAttributes) {
  // DQ (6/5/2020): Adding back the original simile level of support for a
  // single ROSEAttributesList data member.
  start_index = 0;

  // DQ (6/2/2020): This feature is now handled through repeated calls to attach
  // the CPP directives and comments to each file seperately.
  // processAllIncludeFiles = includeDirectivesAndCommentsFromAllFiles;
  processAllIncludeFiles = false;

  // DQ (5/4/2020): This is now handled in a different way.  Each invocation of
  // the AttachPreprocessingInfoTreeTrav traversal will only insert a single
  // file's (header file of source file) comments and CPP directives into the
  // AST.
  ROSE_ASSERT(processAllIncludeFiles == false);

  sourceFile = file;

  // DQ (2/28/2019): We need to return the line that is associated with the
  // source file where this can be a ode shared between multiple ASTs.
  ROSE_ASSERT(sourceFile != NULL);
  ROSE_ASSERT(sourceFile->get_file_info() != NULL);
  source_file_id = sourceFile->get_file_info()->get_physical_file_id();

  // DQ (11/20/2019): Check this.
  // ROSE_ASSERT(sourceFile->get_globalScope() != NULL);

  // ROSEAttributesList* returnListOfAttributes = NULL;
  // ROSEAttributesListContainerPtr filePreprocInfo =
  // sourceFile->get_preprocessorDirectivesAndCommentsList();

  currentListOfAttributes = listOfAttributes;
  ROSE_ASSERT(currentListOfAttributes != NULL);

  // DQ (6/23/2020): Initialize this.
  previousLocatedNode = NULL;

  // DQ (6/23/2020): Initialize this.
  // target_source_file_id =
  // sourceFile->get_file_info()->get_physical_file_id();
  target_source_file_id = sourceFile->get_file_info()->get_physical_file_id();
}

// #ifndef  CXX_IS_ROSE_CODE_GENERATION

// DQ (10/27/2007): Added display function to output information gather durring
// the collection of comments and CPP directives across all files.
void AttachPreprocessingInfoTreeTrav::display(const std::string &label) const {
  // Output internal information

  printf("Inside of AttachPreprocessingInfoTreeTrav::display(%s) \n",
         label.c_str());
  printf("   processAllIncludeFiles        = %s \n",
         processAllIncludeFiles ? "true" : "false");

  // DQ (4/30/2020): Changing the implementation to simplify header file
  // unparsing.
  ROSE_ASSERT(currentListOfAttributes != NULL);
  printf("currentListOfAttributes = %p list size = %d filename = %s \n",
         currentListOfAttributes, currentListOfAttributes->size(),
         currentListOfAttributes->getFileName().c_str());

  printf("previousLocatedNode = %p = %s \n", previousLocatedNode,
         (previousLocatedNode != NULL)
             ? previousLocatedNode->class_name().c_str()
             : "NULL");

  printf("start_index = %d \n", start_index);
}

// DQ (8/6/2012): New copy constructor.
AttachPreprocessingInfoTreeTraversalInheritedAttrribute::
    AttachPreprocessingInfoTreeTraversalInheritedAttrribute(
        const AttachPreprocessingInfoTreeTraversalInheritedAttrribute &X) {
  isPartOfTemplateDeclaration = X.isPartOfTemplateDeclaration;
  isPartOfTemplateInstantiationDeclaration =
      X.isPartOfTemplateInstantiationDeclaration;
  isPartOfFunctionParameterList = X.isPartOfFunctionParameterList;
}

void AttachPreprocessingInfoTreeTrav::handleBracedScopes(
    SgLocatedNode *previousLocatedNode, SgStatement *bracedScope,
    int lineOfClosingBrace, bool reset_start_index,
    ROSEAttributesList *currentListOfAttributes) {
  // DQ (2/16/2021): This function supports
  // iterateOverListAndInsertPreviouslyUninsertedElementsAppearingBeforeLineNumber().
  // It seperates the case where comments and CPP directives are put in the
  // scope or attached to the bottom of the previous statement.

  SgStatement *previousStatement = isSgStatement(previousLocatedNode);

  // if (previousStatement != NULL && previousStatement != basicBlock)
  if (previousStatement != NULL && previousStatement != bracedScope) {
    bool reset_start_index = false;
    iterateOverListAndInsertPreviouslyUninsertedElementsAppearingBeforeLineNumber(
        previousStatement, lineOfClosingBrace, PreprocessingInfo::after,
        reset_start_index, currentListOfAttributes);
  } else {
    // ROSE_ASSERT(previousStatement != NULL);
    // If the previous statement was the current basicBlock, then there were no
    // statements in the SgBasicBlock and we have to add the comments inside the
    // basic block. if (previousStatement == basicBlock) if (previousStatement
    // == bracedScope)
    if (previousStatement != NULL && previousStatement == bracedScope) {
      bool reset_start_index = false;
      // iterateOverListAndInsertPreviouslyUninsertedElementsAppearingBeforeLineNumber
      // ( basicBlock, lineOfClosingBrace, PreprocessingInfo::inside,
      // reset_start_index, currentListOfAttributes );
      iterateOverListAndInsertPreviouslyUninsertedElementsAppearingBeforeLineNumber(
          bracedScope, lineOfClosingBrace, PreprocessingInfo::inside,
          reset_start_index, currentListOfAttributes);
    } else {
    }
  }
}

void AttachPreprocessingInfoTreeTrav::
    iterateOverListAndInsertPreviouslyUninsertedElementsAppearingBeforeLineNumber(
        SgLocatedNode *locatedNode, int lineNumber,
        PreprocessingInfo::RelativePositionType location,
        bool reset_start_index, ROSEAttributesList *currentListOfAttributes) {
  // DQ (11/23/2008): Added comment.
  // This is the main function called to insert all PreprocessingInfo objects
  // into IR nodes.  This function currently adds the PreprocessingInfo objects
  // as attributes, but will be modified to insert the CPP directive specific
  // PreprocessingInfo objects as separate IR nodes and leave PreprocessingInfo
  // objects that are comments inserted as attributes.  Note that attributes
  // imply PreprocessingInfo specific atrributes and not the more general
  // mechanism available in ROSE for user defined attributes to be saved into
  // the AST.

  ROSE_ASSERT(currentListOfAttributes != NULL);

  // DQ (4/29/2020): Introduce test for recursive call.
  static bool isRecursiveCall = false;
  ROSE_ASSERT(isRecursiveCall == false);

  isRecursiveCall = true;

#define DEBUG_IterateOverList 0

#if DEBUG_IterateOverList || 0
  // DQ (8/22/2018): Added debugging information.
  printf("In "
         "AttachPreprocessingInfoTreeTrav::"
         "iterateOverListAndInsertPreviouslyUninsertedElementsAppearingBeforeLi"
         "neNumber(): currentListOfAttributes->size() = %d \n",
         currentListOfAttributes->size());
  printf(" --- locatedNode = %p = %s lineNumber = %d location = %s \n",
         locatedNode, locatedNode->class_name().c_str(), lineNumber,
         PreprocessingInfo::relativePositionName(location).c_str());
#endif
#if DEBUG_IterateOverList
  currentListOfAttributes->display("Top of "
                                   "iterateOverListAndInsertPreviouslyUninserte"
                                   "dElementsAppearingBeforeLineNumber()");
#endif

  // DQ (6/8/2020): Adding assertions to debug segfault below.
  ROSE_ASSERT(locatedNode != NULL);

#if DEBUG_IterateOverList
  // DQ (9/17/2019): Added debugging information.
  printf("In "
         "AttachPreprocessingInfoTreeTrav::"
         "iterateOverListAndInsertPreviouslyUninsertedElementsAppearingBeforeLi"
         "neNumber(): sourceFile = %p = %s \n",
         sourceFile, sourceFile->class_name().c_str());
  printf(" --- currentListOfAttributes->size() = %d \n",
         currentListOfAttributes->size());
  printf(" --- iterateOverListAndInsertPrev... locatedNode = %p = %s "
         "lineNumber = %d location = %s \n",
         locatedNode, locatedNode->class_name().c_str(), lineNumber,
         PreprocessingInfo::relativePositionName(location).c_str());
#endif

  ROSE_ASSERT(locatedNode->get_startOfConstruct() != NULL);

#if DEBUG_ATTACH_PREPROCESSING_INFO
  // Debugging information...
  {
    // int line        = locatedNode->get_startOfConstruct()->get_line();
    int line =
        locatedNode->get_startOfConstruct()->get_physical_line(source_file_id);
    int col = locatedNode->get_startOfConstruct()->get_col();
    // int ending_line = locatedNode->get_endOfConstruct()->get_line();
    int ending_line =
        locatedNode->get_endOfConstruct()->get_physical_line(source_file_id);
    int ending_col = locatedNode->get_endOfConstruct()->get_col();

    // DQ (8/6/2012): Added support for endOfConstruct().
    printf("locatedNode = %p = %s \n", locatedNode,
           locatedNode->class_name().c_str());
    cout << "Visiting SgStatement node (starting: " << line << ":" << col
         << ") (ending " << ending_line << ":" << ending_col << ") -> ";
    cout << getVariantName(locatedNode->variantT()) << endl;
    cout << "-----> Filename: " << locatedNode->get_file_info()->get_filename()
         << endl;
  }
  // cout << "Traversing current list of attributes of length " <<
  // sizeOfCurrentListOfAttributes << endl;
#endif

  // for ( int i = 0; i < sizeOfCurrentListOfAttributes; i++ )
  // AS(09/21/07) Because the AttachAllPreprocessingInfoTreeTrav can call the
  // evaluateInheritedAttribute(..) which calls this function the start_index
  // can not be static for this function. Instead it is made a class member
  // variable for AttachPreprocessingInfoTreeTrav so that it can be reset by
  // AttachAllPreprocessingInfoTreeTrav when processing a new file.

  // static int start_index = 0;
  // int currentFileId = locatedNode->get_startOfConstruct()->get_file_id();
  Sg_File_Info *locatedFileInfo = locatedNode->get_file_info();

  // DQ (12/18/2012): Switch to using the physical file id now that we support
  // this feature.
  int currentFileId =
      (sourceFile->get_requires_C_preprocessor() == true)
          ? Sg_File_Info::getIDFromFilename(
                sourceFile->generate_C_preprocessor_intermediate_filename(
                    sourceFile->get_file_info()->get_filename()))
          : locatedFileInfo->get_physical_file_id(source_file_id);

  // DQ (12/15/2012): Allow equivalent files to be mapped back to the source
  // file.
  if (currentListOfAttributes->get_filenameIdSet().find(currentFileId) !=
      currentListOfAttributes->get_filenameIdSet().end()) {
    // File name that we want all equivalent files to map to...
    // string filename =
    // sourceFile->generate_C_preprocessor_intermediate_filename(sourceFile->get_file_info()->get_filename());
    string filename = sourceFile->get_file_info()->get_filename();

    currentFileId = Sg_File_Info::getIDFromFilename(filename);
    // DQ (12/19/2012): This should map to an existing file.
    ROSE_ASSERT(currentFileId >= 0);
  }

  // DQ (4/30/2020): We no long need this in the new simplified support for CPP
  // directivces and comments and unparsing of header files. int start_index =
  // startIndexMap[currentFileId];
  int sizeOfCurrentListOfAttributes = currentListOfAttributes->size();

#if DEBUG_IterateOverList
  printf("Initial start_index = %d \n", start_index);
#endif

  // Liao 2/1/2010: SgBasicBlock in Fortran should be ignored for attaching a
  // preprocessing info with a 'before' position. The reason is that there is no
  // { ..} in Fortran and the preprocessing information should really be
  // associated with a statement showing up in the source code. However, we
  // allow a preprocessing info. to be attached to be inside of a SgBasicBlock
  // to get the following special case right: end do does not exist in AST. The
  // comment has to be attached inside the do-loop's body to be unparsed right
  // before 'end do'
  //  do i 1, 10
  //
  // ! comment here
  //  end do
  //
  bool isFortranBlockAndBeforePoisition =
      false; // should we skip a Fortran basic block when the position is
             // before?
  if (SageInterface::is_Fortran_language() == true) {
    if (isSgBasicBlock(locatedNode) && (location == PreprocessingInfo::before ||
                                        location == PreprocessingInfo::after)) {
      isFortranBlockAndBeforePoisition = true;
    }
  }

#if DEBUG_IterateOverList
  printf("isFortranBlockAndBeforePoisition = %s \n",
         isFortranBlockAndBeforePoisition ? "true" : "false");
#endif

  // DQ (12/23/2008): Note: I think that this should be turned into a while loop
  // (starting at start_index, to lineNumber when location ==
  // PreprocessingInfo::before, and to the sizeOfCurrentListOfAttributes when
  // location == PreprocessingInfo::after).
  if (!isFortranBlockAndBeforePoisition) {
#if DEBUG_IterateOverList
    printf("start_index = %d sizeOfCurrentListOfAttributes = %d \n",
           start_index, sizeOfCurrentListOfAttributes);
#endif
    list<pair<SgIncludeDirectiveStatement *, SgStatement *>>
        localStatementsToInsertAfter;
    for (int i = start_index; i < sizeOfCurrentListOfAttributes; i++) {
      PreprocessingInfo *currentPreprocessingInfoPtr =
          (*currentListOfAttributes)[i];

      // DQ (6/4/2020): Added test.
      ROSE_ASSERT(currentPreprocessingInfoPtr != NULL);
#if DEBUG_IterateOverList
      printf("TOP OF LOOP: Processing (*currentListOfAttributes)[%3d] = %p "
             "string = %s \n",
             i, currentPreprocessingInfoPtr,
             currentPreprocessingInfoPtr->getString().c_str());
#endif
      // DQ (8/21/2018): I think we can assert these here.
      ROSE_ASSERT(currentPreprocessingInfoPtr->get_file_info() != NULL);
      ROSE_ASSERT(currentPreprocessingInfoPtr != NULL);
      ROSE_ASSERT(currentPreprocessingInfoPtr != NULL);
      int currentPreprocessingInfoLineNumber =
          currentPreprocessingInfoPtr->getLineNumber();

      int currentPreprocessingInfoColumnNumber =
          currentPreprocessingInfoPtr->getColumnNumber();

#if DEBUG_IterateOverList
      // DQ (8/17/2020): Added note detail
      int line = locatedNode->get_startOfConstruct()->get_physical_line(
          source_file_id);
      int col = locatedNode->get_startOfConstruct()->get_col();
      int ending_line =
          locatedNode->get_endOfConstruct()->get_physical_line(source_file_id);
#endif
      int ending_col = locatedNode->get_endOfConstruct()->get_col();

#if DEBUG_IterateOverList
      // std::cerr << "sagenode                             = " <<
      // typeid(*locatedNode).name() << std::endl;
      printf("currentPreprocessingInfoLineNumber   = %d lineNumber = %d \n",
             currentPreprocessingInfoLineNumber, lineNumber);
      printf("currentPreprocessingInfoColumnNumber = %d lineNumber = %d \n",
             currentPreprocessingInfoColumnNumber, lineNumber);
      printf("starting line = %d ending_line = %d starting col = %d ending_col "
             "= %d \n",
             line, ending_line, col, ending_col);
      printf("location = %s \n",
             PreprocessingInfo::relativePositionName(location).c_str());
#endif

      // DQ (12/23/2008): So far this is the most reliable way to break out of
      // the loop.
      ROSE_ASSERT(currentPreprocessingInfoPtr != NULL);

      // bool attachCommentOrDirective = (currentPreprocessingInfoPtr != NULL)
      // && (currentPreprocessingInfoPtr->getLineNumber() <= lineNumber); bool
      // attachCommentOrDirective = (currentPreprocessingInfoLineNumber <=
      // lineNumber);
      bool attachCommentOrDirective =
          (currentPreprocessingInfoLineNumber < lineNumber) ||
          ((currentPreprocessingInfoLineNumber == lineNumber) &&
           (currentPreprocessingInfoColumnNumber < ending_col));

      // DQ (1/7/2019): Supress comments and CPP directives onto member
      // functions of the generated labda function class.
      SgLambdaExp *lambdaExpression = isSgLambdaExp(locatedNode->get_parent());
      if (lambdaExpression != NULL) {
        attachCommentOrDirective = false;
      }

#if DEBUG_IterateOverList || 0
      printf("@@@@@@@@@@@@@@@@@@ attachCommentOrDirective = %s "
             "currentPreprocessingInfoLineNumber = %d lineNumber = %d \n",
             attachCommentOrDirective ? "true" : "false",
             currentPreprocessingInfoLineNumber, lineNumber);
#endif
      if (attachCommentOrDirective == true) {
#if DEBUG_ATTACH_PREPROCESSING_INFO
        printf("Attaching \"%s\" (from file = %s file_id = %d line# %d) to %s "
               "locatedNode = %p = %s = %s at line %d position = %s \n",
               currentPreprocessingInfoPtr->getString().c_str(),
               currentPreprocessingInfoPtr->getFilename().c_str(),
               currentPreprocessingInfoPtr->getFileId(),
               currentPreprocessingInfoPtr->getLineNumber(),
               (locatedNode->get_file_info()->isCompilerGenerated() == true)
                   ? "compiler-generated"
                   : "non-compiler-generated",
               locatedNode, locatedNode->class_name().c_str(),
               SageInterface::get_name(locatedNode).c_str(),
               // (locatedNode->get_file_info()->isCompilerGenerated() == true)
               // ? -1 : locatedNode->get_file_info()->get_line());
               (locatedNode->get_file_info()->isCompilerGenerated() == true)
                   ? -1
                   : locatedNode->get_file_info()->get_physical_line(
                         source_file_id),
               PreprocessingInfo::relativePositionName(location).c_str());
#if DEBUG_IterateOverList
        printf("Attaching to node from "
               "locatedNode->get_file_info()->get_filename() = %s \n",
               locatedNode->get_file_info()->get_filename());
        printf(" --- currentListOfAttributes->getFileName()                    "
               "    = %s \n",
               currentListOfAttributes->getFileName().c_str());
#endif
        // DQ (11/4/2019): If we want this assertion then it likely should be
        // based on physical filenames (derived from physical fid ids). DQ
        // (11/3/2019): Check that the comment or CPP directive is from the same
        // file as the locatedNode. A variation of this test might be required
        // later, though we should only be attacheing comments and CPP
        // directives before possible transformations.
#if DEBUG_IterateOverList
        printf("In "
               "iterateOverListAndInsertPreviouslyUninsertedElementsAppearingBe"
               "foreLineNumber(): locatedNode->get_file_info()->get_filename() "
               "= %s \n",
               locatedNode->get_file_info()->get_filename());
        printf("In "
               "iterateOverListAndInsertPreviouslyUninsertedElementsAppearingBe"
               "foreLineNumber(): currentListOfAttributes->getFileName()       "
               "= %s \n",
               currentListOfAttributes->getFileName().c_str());
#endif
        // ROSE_ASSERT(locatedNode->get_file_info()->get_filename() ==
        // currentListOfAttributes->getFileName());

        SgNode *parentNode = locatedNode->get_parent();
        if (parentNode != NULL) {
          printf("locatedNode->parent = %p = %s \n", parentNode,
                 parentNode->class_name().c_str());
          SgClassDefinition *classDefinition = isSgClassDefinition(parentNode);
          if (classDefinition != NULL) {
            SgClassDeclaration *classDeclaration =
                classDefinition->get_declaration();
            if (classDeclaration != NULL) {
              printf("parent: classDeclaration->get_name() = %s \n",
                     classDeclaration->get_name().str());
            }
          } else {
            SgStatement *associatedStatement = isSgStatement(locatedNode);

            // DQ (11/4/2019): This is not general enough code.
            // ROSE_ASSERT(associatedStatement != NULL);
            if (associatedStatement != NULL) {
              SgScopeStatement *associatedScope =
                  isSgScopeStatement(associatedStatement->get_scope());
              SgClassDefinition *classDefinition =
                  isSgClassDefinition(associatedScope);
              if (classDefinition != NULL) {
                SgClassDeclaration *classDeclaration =
                    classDefinition->get_declaration();
                if (classDeclaration != NULL) {
                  printf(
                      "associatedScope: classDeclaration->get_name() = %s \n",
                      classDeclaration->get_name().str());
                }
              }
            }
          }
        }
        // printf ("locatedNode->unparseToString() = %s
        // \n",locatedNode->unparseToString().c_str());
#endif
        // Mark this PreprocessingInfo object as having been placed into the AST
        // It might make more sense to remove it from the list so it doesn't
        // have to be traversed next time.
        // currentPreprocessingInfoPtr->setHasBeenCopied();

        // negara1 (08/05/2011): Do not set to NULL such that we can reuse it
        // for multiple inclusions of the same header file.
        // currentListOfAttributes->getList()[i] = NULL;

        // DQ (4/30/2020): We no long need this in the new simplified support
        // for CPP directivces and comments and unparsing of header files. DQ
        // (4/13/2007): If we are going to invalidate the list of accumulated
        // attributes then we can start next time at the next index (at least).
        // This removes the order n^2 complexity of traversing over the whole
        // loop. start_index = i+1;
        // ROSE_ASSERT(startIndexMap.find(currentFileId) !=
        // startIndexMap.end()); startIndexMap[currentFileId] = i+1;

        start_index = i + 1;
#if DEBUG_IterateOverList
        // DQ (4/30/2020): We no long need this in the new simplified support
        // for CPP directivces and comments and unparsing of header files.
        // printf ("Incremented start_index to be %d
        // \n",startIndexMap[currentFileId]);
        printf("Incremented start_index to be %d \n", start_index);
#endif
        // Mark the location relative to the current node where the
        // PreprocessingInfo object should be unparsed (before or after)
        // relative to the current locatedNode
        currentPreprocessingInfoPtr->setRelativePosition(location);
#if DEBUG_IterateOverList
        printf("Attaching CPP directives %s to IR nodes as attributes "
               "(location = %s) \n",
               PreprocessingInfo::directiveTypeName(
                   currentPreprocessingInfoPtr->getTypeOfDirective())
                   .c_str(),
               PreprocessingInfo::relativePositionName(
                   currentPreprocessingInfoPtr->getRelativePosition())
                   .c_str());
#endif
        // This uses the old code to attach comments and CPP directives to the
        // AST as attributes.
        locatedNode->addToAttachedPreprocessingInfo(
            currentPreprocessingInfoPtr);
#if DEBUG_IterateOverList
        printf(
            "DONE: Attaching CPP directives %s to IR nodes as attributes. \n",
            PreprocessingInfo::directiveTypeName(
                currentPreprocessingInfoPtr->getTypeOfDirective())
                .c_str());
#endif
        // DQ (12/2/2018): This fails for the C/C++ snippet insertion tests.
        // DQ (12/2/2018): This fails for Fortran.
        // DQ (9/5/2018): We should have already set the
        // preprocessorDirectivesAndCommentsList, checked in getTokenStream().
        // ROSE_ASSERT(sourceFile->get_preprocessorDirectivesAndCommentsList()
        // != NULL); if (SageInterface::is_Fortran_language() == false)
        if (SageInterface::is_C_language() == true ||
            SageInterface::is_Cxx_language() == true) {
          // ROSE_ASSERT(sourceFile->get_preprocessorDirectivesAndCommentsList()
          // != NULL);
        }
#if DEBUG_IterateOverList
        printf("sourceFile->getFileName()                            = %s \n",
               sourceFile->getFileName().c_str());
        printf("sourceFile->get_unparseHeaderFiles()                 = %s \n",
               sourceFile->get_unparseHeaderFiles() ? "true" : "false");
        printf("sourceFile->get_header_file_unparsing_optimization() = %s \n",
               sourceFile->get_header_file_unparsing_optimization() ? "true"
                                                                    : "false");
        printf("currentPreprocessingInfoPtr->getTypeOfDirective() == "
               "PreprocessingInfo::CpreprocessorIncludeDeclaration = %s \n",
               currentPreprocessingInfoPtr->getTypeOfDirective() ==
                       PreprocessingInfo::CpreprocessorIncludeDeclaration
                   ? "true"
                   : "false");
#endif

        // For now leave the lists unmodified so that we can support debugging.
        // delete currentPreprocessingInfoPtr;
        // currentPreprocessingInfoPtr = NULL;

        // debugging info
        // printOutComments(locatedNode);
      }
#if DEBUG_IterateOverList
      printf("BOTTOM OF LOOP: Processing (*currentListOfAttributes)[%3d] = %p "
             "string = %s \n",
             i, currentPreprocessingInfoPtr,
             currentPreprocessingInfoPtr->getString().c_str());
#endif
    }

    // DQ (1/7/2019): This appears to be nearly always an empty list, so we can
    // improve the performance and also simlify the debugging with this test.
    if (localStatementsToInsertAfter.empty() == false) {
#if DEBUG_IterateOverList
      // DQ (1/7/2019): Adding debugging support.
      printf("Calling insert statements: statementsToInsertAfter.size() = %zu "
             "localStatementsToInsertAfter.size() = %zu \n",
             statementsToInsertAfter.size(),
             localStatementsToInsertAfter.size());
#endif
      // negara1 (08/15/2011): After the iteration is over, add local list of
      // statements to "insert after" to the global list. Two lists are used in
      // order to insert in front of the local list and then, insert the local
      // list in front of the global list such that we preserve the relative
      // order of inserted nodes.
      statementsToInsertAfter.insert(statementsToInsertAfter.begin(),
                                     localStatementsToInsertAfter.begin(),
                                     localStatementsToInsertAfter.end());
    }
  }

  // DQ (12/12/2008): We should not need this state, so why support resetting
  // it, unless the traversal needs to be called multiple times. DQ (4/13/2007):
  // The evaluation of the synthesized attribute for a SgFile will trigger the
  // reset of the start index to 0.
  if (reset_start_index == true) {
    // DQ (4/30/2020): We no long need this in the new simplified support for
    // CPP directivces and comments and unparsing of header files. Reset all the
    // start_index data members (for each associated file) start_index = 0; for
    // (StartingIndexAttributeMapType::iterator it = startIndexMap.begin(); it
    // != startIndexMap.end(); it++)
    //    {
    //      it->second = 0;
    //    }
    start_index = 0;
  }

#if DEBUG_IterateOverList
  // DQ (10/27/2019): Added debugging information.
  printf("Leaving "
         "AttachPreprocessingInfoTreeTrav::"
         "iterateOverListAndInsertPreviouslyUninsertedElementsAppearingBeforeLi"
         "neNumber(): currentListOfAttributes->size() = %d \n",
         currentListOfAttributes->size());
#endif

  // DQ (4/29/2020): Introduce test for recursive call.
  isRecursiveCall = false;
}

//! Use parent as the previous node to attach preprocessing info since a current
//! node is not unparsed.
void AttachPreprocessingInfoTreeTrav::setupPointerToPreviousNode(
    SgLocatedNode *currentLocNodePtr) {
  // If we are at a SgCtorInitializerList IR nodes (and a few others)
  // then since it is visited last (after the definition) leave the
  // previousLocNodePtr referenced to the function definition.

  // Supports assertions at end of function
  SgLocatedNode *previousLocNodePtr = NULL;

  if ((dynamic_cast<SgForInitStatement *>(currentLocNodePtr) == NULL) &&
      (dynamic_cast<SgTypedefSeq *>(currentLocNodePtr) == NULL) &&
      (dynamic_cast<SgCatchStatementSeq *>(currentLocNodePtr) == NULL) &&
      (dynamic_cast<SgFunctionParameterList *>(currentLocNodePtr) == NULL) &&
      (dynamic_cast<SgCtorInitializerList *>(currentLocNodePtr) == NULL)) {
    // DQ (6/9/2020): Modified to point to currentLocNodePtr.
    // SgLocatedNode* previousLocatedNode;
    // previousLocNodePtr = previousLocatedNode;
    previousLocNodePtr = currentLocNodePtr;
  } else {
    SgStatement *currentStatement =
        dynamic_cast<SgStatement *>(currentLocNodePtr);
    ROSE_ASSERT(currentStatement != NULL);
    SgStatement *parentStatement =
        isSgStatement(currentStatement->get_parent());

    // We can't enforce this since currentStatement may be SgGlobal and the
    // parent is SgSourceFile (which is not a SgStatement). ROSE_ASSERT
    // (parentStatement != NULL);
    ROSE_ASSERT((parentStatement != NULL) ||
                (isSgGlobal(currentStatement) != NULL));

    // Supports assertions at end of function
    previousLocNodePtr = parentStatement;
  }

  // Liao 6/10/2020, special handling for Fortran subroutine init-name, which is
  // compiler generated and cannot be unparsed directly. It cannot be used as an
  // anchor node for preprocessing information. In this case, we use
  // SgBasicBlock of the SgFunctionDefinition as the previous located node. This
  // is to address the lost comment problem as shown in test2020_comment_1.f90 .
  if (SageInterface::is_Fortran_language()) {
    if (SgInitializedName *init_name = isSgInitializedName(currentLocNodePtr)) {
      if (isSgProcedureHeaderStatement(init_name->get_parent())) {
        previousLocNodePtr = init_name->get_scope();

        // DQ (7/3/2020): We no longer support this map (in the new design for
        // comment and CPP directive handling).
        // previousLocatedNodeMap[currentFileId] = previousLocNodePtr;
      }
    }
  }

  // Nodes that should not have comments attached (since they are not unparsed
  // directly within the generation of the source code by the unparser (no
  // associated unparse functions))
  ROSE_ASSERT(dynamic_cast<SgForInitStatement *>(previousLocNodePtr) == NULL);
  ROSE_ASSERT(dynamic_cast<SgTypedefSeq *>(previousLocNodePtr) == NULL);
  ROSE_ASSERT(dynamic_cast<SgCatchStatementSeq *>(previousLocNodePtr) == NULL);
  ROSE_ASSERT(dynamic_cast<SgFunctionParameterList *>(previousLocNodePtr) ==
              NULL);
  ROSE_ASSERT(dynamic_cast<SgCtorInitializerList *>(previousLocNodePtr) ==
              NULL);
}

// DQ (1/18/2021): Adding call to buildTokenStreamMapping.
void buildTokenStreamMapping(SgSourceFile *sourceFile,
                             vector<stream_element *> &tokenVector);

// DQ (1/4/2021): Adding support for comments and CPP directives and tokens to
// use new_filename. ROSEAttributesList*
// AttachPreprocessingInfoTreeTrav::buildCommentAndCppDirectiveList (
// std::string fileNameForDirectivesAndComments )
ROSEAttributesList *
AttachPreprocessingInfoTreeTrav::buildCommentAndCppDirectiveList(
    SgSourceFile *sourceFile, std::string fileNameForDirectivesAndComments,
    std::string new_filename) {
  // This function abstracts the collection of comments and CPP directives into
  // a list. The list is then used to draw from as the AST is traversed and the
  // list elements are woven into the AST.

  // DQ (02/20/2021): Using the performance tracking within ROSE.
  TimingPerformance timer("AST buildCommentAndCppDirectiveList():");

#define DEBUG_BUILD_COMMENT_AND_CPP_DIRECTIVE_LIST 0

#if DEBUG_BUILD_COMMENT_AND_CPP_DIRECTIVE_LIST || 0
  // DQ (1/4/2021): adding debugging support.
  printf("Inside of "
         "AttachPreprocessingInfoTreeTrav::buildCommentAndCppDirectiveList "
         "file = %s \n",
         fileNameForDirectivesAndComments.c_str());
  printf(" --- sourceFile->getFileName() = %s \n",
         sourceFile->getFileName().c_str());
  printf(" --- new_filename = %s \n", new_filename.c_str());
#endif

  // DQ (4/29/2020): Introduce test for recursive call.
  static bool isRecursiveCall = false;
  ROSE_ASSERT(isRecursiveCall == false);

  isRecursiveCall = true;

  // Liao 4/26/2010 support --enable-only-c
  ROSE_ASSERT(sourceFile != NULL);
  string fileNameForTokenStream = fileNameForDirectivesAndComments;

  // DQ (11/2/2019): Avoid redundant calls to getListOfAttributes().
  // ROSEAttributesList* returnListOfAttributes = new ROSEAttributesList();
  ROSEAttributesList *returnListOfAttributes = NULL;
  ROSEAttributesListContainerPtr filePreprocInfo =
      sourceFile->get_preprocessorDirectivesAndCommentsList();

#if DEBUG_BUILD_COMMENT_AND_CPP_DIRECTIVE_LIST || 0
  printf("filePreprocInfo = %p \n", filePreprocInfo);
  printf("sourceFile = %p \n", sourceFile);
  printf("sourceFile->get_file_info() = %p \n", sourceFile->get_file_info());
  printf("sourceFile->get_file_info()->get_filename() = %s \n",
         sourceFile->get_file_info()->get_filename());
  printf("sourceFile->get_Fortran_only() = %s \n",
         sourceFile->get_Fortran_only() ? "true" : "false");
#endif

  // DQ (1/9/2021): Cleaned up the logic here.
  // DQ (12/3/2019): Need to test for filePreprocInfo != NULL when compiling
  // Fortran code. if (sourceFile->get_Fortran_only() == false)
  //    {
  // if (filePreprocInfo != NULL)

  // PP (04/13/21) limit preprocessing for non-Fortran sources
  const bool isFortran = sourceFile->get_Fortran_only();

  if ((isFortran == false) && (filePreprocInfo != NULL)) {
#if DEBUG_BUILD_COMMENT_AND_CPP_DIRECTIVE_LIST
    printf("filePreprocInfo->getList().find(sourceFile->get_file_info()->get_"
           "filename()) == filePreprocInfo->getList().end() = %s \n",
           filePreprocInfo->getList().find(
               sourceFile->get_file_info()->get_filename()) ==
                   filePreprocInfo->getList().end()
               ? "true"
               : "false");
#endif
    if (filePreprocInfo->getList().find(
            sourceFile->get_file_info()->get_filename()) ==
        filePreprocInfo->getList().end()) {
#if DEBUG_BUILD_COMMENT_AND_CPP_DIRECTIVE_LIST
      int currentFileNameId = sourceFile->get_file_info()->get_file_id();
      printf("Generating a new ROSEAttributesList: currentFileNameId = %d \n",
             currentFileNameId);
#endif

#ifdef ROSE_BUILD_CPP_LANGUAGE_SUPPORT
      // DQ (1/4/2021): Adding support for comments and CPP directives and
      // tokens to use new_filename. DQ (11/2/2019): A call to
      // getListOfAttributes() will generate infinite recursion.
      // returnListOfAttributes = getListOfAttributes(currentFileNameId);
      // returnListOfAttributes =
      // getPreprocessorDirectives(fileNameForDirectivesAndComments);
      returnListOfAttributes = getPreprocessorDirectives(
          fileNameForDirectivesAndComments, new_filename);
#endif
#if DEBUG_BUILD_COMMENT_AND_CPP_DIRECTIVE_LIST
      printf("DONE: Generating a new ROSEAttributesList: currentFileNameId = "
             "%d \n",
             currentFileNameId);
#endif

      // DQ (1/17/2020): Get the token list from the
      // LexTokenStreamTypePointer token_list_pointer =
      // returnListOfAttributes->get_rawTokenStream();
      // ROSE_ASSERT(token_list_pointer != NULL);
      ROSE_ASSERT(returnListOfAttributes->get_rawTokenStream() != NULL);

      // DQ (1/18/2021): This is useless code, except that it converts the
      // list<stream_element*> type to a vector<stream_element*> type. Obviously
      // we should change the handling in the lexing step to generate a
      // vector<stream_element*> type directly so that we can avoid this silly
      // translations. The reason we need it is because the processing of the
      // mapping of the toke stream to the AST is using the
      // vector<stream_element*> type (which is likely best for being the most
      // efficient, and allows for integer indexing of the vector).  Then the
      // token sequence mapping is just the the lists of index values into the
      // vector of tokens.  However, I don't see where the list or vector if
      // SgTokens* is used or located.

      // LexTokenStreamType* tokenStream = getTokenStream(sourceFile);
      LexTokenStreamType *tokenStream =
          returnListOfAttributes->get_rawTokenStream();
      ROSE_ASSERT(tokenStream != NULL);
      // Set this value so that we can generate unique keys for any interval.
      // I think that a better mehcanism for generating unique keys would be
      // possible (but this is simple).
      TokenStreamSequenceToNodeMapping::tokenStreamSize = tokenStream->size();

      // Convert this list to a vectors so that we can use integer indexing
      // instead of iterators into a list.
      vector<stream_element *> tokenVector;
      for (LexTokenStreamType::iterator i = tokenStream->begin();
           i != tokenStream->end(); i++) {
        tokenVector.push_back(*i);
      }

      // DQ (1/30/2014): I have added the corner case for an empty file, with
      // zero tokens to find. We need to make sure this is not an error (OK it
      // issue a warning). ROSE_ASSERT(tokenVector.empty() == false);
      if (tokenVector.empty() == true) {
        printf(
            "Warning: this is an empty file (no tokens found): not even a CR "
            "present! (but not an error using the token stream unparsing) \n");
      }

      // return tokenVector;

      // typedef std::list<stream_element*> LexTokenStreamType;

      // TokenStreamSequenceToNodeMapping* tokenStreamSequenceToNodeMapping =
      // token_list_pointer;

      // std::map<SgNode*,TokenStreamSequenceToNodeMapping*> &
      // tokenStreamSequenceMap = *token_list_pointer;
      // std::map<SgNode*,TokenStreamSequenceToNodeMapping*> &
      // tokenStreamSequenceMap = *token_list_pointer;
      // sourceFile->set_tokenSubsequenceMap(tokenStreamSequenceMap);
      // DQ (1/18/2021): Only in the most trivial of empty files should the
      // vector of tokens be empty. ROSE_ASSERT(tokenVector.size() > 0);

      ROSE_ASSERT(filePreprocInfo != NULL);
      // ROSEAttributesListContainerPtr filePreprocInfo =
      // file->get_preprocessorDirectivesAndCommentsList();

      // DQ (2/20/2021): This conditional fixes the performance problem with the
      // use of ROSE without the token-based unparsing. The token based
      // unparsing is more expensive mostly because of the call to
      // buildTokenStreamMapping().  We might have to look into that seperately.
      // Since it is about as expensive as the cost of the frontend with
      // token-based unparsing. DQ (2/18/2021): We only want to process the
      // token stream if sourceFile->get_unparse_tokens() is true (specified on
      // the command line). Currently we have to call this else we get an error
      // in the unparser.  This should be fixed for performance reasons. We
      // currently output a message in buildTokenStreamMapping() when
      // sourceFile->get_unparse_tokens() == false. This code now works and
      // solves the perfoermance problem that was present for ROSE when used
      // without the token-based unparsing.
      if (sourceFile->get_unparse_tokens() == true) {
        // DQ (02/20/2021): Using the performance tracking within ROSE.
        TimingPerformance timer("AST calling buildTokenStreamMapping():");
        // DQ (2/20/2021): This is a pretty expensive operation, about the same
        // cost of the frontend (without the call to this function).
        buildTokenStreamMapping(sourceFile, tokenVector);
      } else {
      }

      // DQ (1/9/2021): This adds the token list and the comments and CPP
      // directives to the list of such things. DQ (11/2/2019): Add the new
      // attributes to the list.
      auto &filePreprocInfoList = filePreprocInfo->getList();
      filePreprocInfoList[sourceFile->get_file_info()->get_filename()] =
          returnListOfAttributes;

    } else {
      returnListOfAttributes =
          filePreprocInfo
              ->getList()[sourceFile->get_file_info()->get_filename()];
      // DQ (1/9/2021): Debugging.
      printf("Exiting as a test! \n");
      ROSE_ABORT();
    }
  } else {
  }
  // }

  // Build an empty list while we skip the translation of tokens
  // returnListOfAttributes = new ROSEAttributesList();

  // If this is a CPP processed file then modify the name to reflect that the
  // CPP output is to be process and it was assigned a different file name (with
  // "_preprocessed" suffix).

  // currentFileNameId = currentFilePtr->get_file_info()->get_file_id();
  // ROSE_ASSERT(currentFileNameId >= 0);

  // Note that we need the SgSourceFile so that we get information about what
  // language type this is to support. SgSourceFile* currentFilePtr =
  // sourceFile;

  // DQ (4/12/2007): Introduce tracking of performance of ROSE.
  TimingPerformance evaluate_timer("AST evaluateInheritedAttribute:");

  // AS(4/3/09): FIXME: We are doing this quick fix because the
  // fileNameForDirectivesAndComments is incorrect for Fortran
  // PC(08/17/2009): Now conditional on the output language, otherwise
  // breaks -rose:collectAllCommentsAndDirectives
  if (sourceFile->get_outputLanguage() == SgFile::e_Fortran_language) {
    fileNameForDirectivesAndComments = sourceFile->get_sourceFileNameWithPath();
    fileNameForTokenStream = fileNameForDirectivesAndComments;
  }

  if (sourceFile->get_Fortran_only() == true) {
    // For Fortran CPP code you need to preprocess the code into an intermediate
    // file in order to pass it through the Fortran frontend. This is because
    // for Fortan everything is ONE file.
    if (sourceFile->get_requires_C_preprocessor() == true) {
      fileNameForDirectivesAndComments =
          sourceFile->generate_C_preprocessor_intermediate_filename(
              fileNameForDirectivesAndComments);
    }

    // DQ (12/3/2019): I think this is required for Fortran support.
    // ROSE_ASSERT(returnListOfAttributes == NULL);
    // returnListOfAttributes = new ROSEAttributesList();
    if (returnListOfAttributes == NULL) {
      returnListOfAttributes = new ROSEAttributesList();
    }
    ROSE_ASSERT(returnListOfAttributes != NULL);

#ifdef ROSE_BUILD_FORTRAN_LANGUAGE_SUPPORT
    // This is either of two different kinds of Fortran programs: fixed format
    // or free format
    //    * fix format is generally used for older Fortran code, F77 and
    //    earlier, and
    //    * free format is generall used for newer codes, F90 and later
    //    * however this is a general rule, specifically a F03 code can use
    //    fixed format.

    // If it is not explicitly fixed form, then assume it is free form input.
    // if (currentFilePtr->get_fixedFormat() == true)
    if (sourceFile->get_inputFormat() == SgFile::e_fixed_form_output_format) {
      if (SgProject::get_verbose() > 1) {
        printf("Fortran code assumed to be in fixed format form (skipping "
               "translation of tokens) \n");
      }

      // For now we call the lexical pass on the fortran file, but we don't yet
      // translate the tokens. returnListOfAttributes       =
      // getPreprocessorDirectives(
      // Sg_File_Info::getFilenameFromID(currentFileNameId) );
      // getFortranFixedFormatPreprocessorDirectives(
      // Sg_File_Info::getFilenameFromID(currentFileNameId) );
      // LexTokenStreamTypePointer lex_token_stream =
      // getFortranFixedFormatPreprocessorDirectives(
      // Sg_File_Info::getFilenameFromID(currentFileNameId) );
      LexTokenStreamTypePointer lex_token_stream = new LexTokenStreamType();
      ROSE_ASSERT(lex_token_stream != NULL);

      // Attach the token stream to the AST
      returnListOfAttributes->set_rawTokenStream(lex_token_stream);
      // DQ (11/23/2008): This is the new support to collect CPP directives and
      // comments from Fortran applications. printf ("Calling
      // collectPreprocessorDirectivesAndCommentsForAST() to collect CPP
      // directives for fileNameForDirectivesAndComments = %s
      // \n",fileNameForDirectivesAndComments.c_str());
      returnListOfAttributes->collectPreprocessorDirectivesAndCommentsForAST(
          fileNameForDirectivesAndComments,
          ROSEAttributesList::e_Fortran77_language);
      // printf ("DONE: Calling collectPreprocessorDirectivesAndCommentsForAST()
      // to collect CPP directives for fileNameForDirectivesAndComments = %s
      // \n",fileNameForDirectivesAndComments.c_str());
    } else {
      // int currentFileNameId = currentFilePtr->get_file_info()->get_file_id();
      // For now we call the lexical pass on the fortran file, but we don't yet
      // translate the tokens. returnListOfAttributes       =
      // getPreprocessorDirectives(
      // Sg_File_Info::getFilenameFromID(currentFileNameId) );
      // getFortranFreeFormatPreprocessorDirectives(
      // Sg_File_Info::getFilenameFromID(currentFileNameId) ); string
      // fileNameForTokenStream =
      // Sg_File_Info::getFilenameFromID(currentFileNameId);

      LexTokenStreamTypePointer lex_token_stream = new LexTokenStreamType();
      ROSE_ASSERT(lex_token_stream != NULL);

      // DQ (12/3/2019): Added test to support debugging Fortran support.
      ROSE_ASSERT(returnListOfAttributes != NULL);

      // Attach the token stream to the AST
      returnListOfAttributes->set_rawTokenStream(lex_token_stream);
      ROSE_ASSERT(returnListOfAttributes->get_rawTokenStream() != NULL);
      // DQ (11/23/2008): This is the new support to collect CPP directives and
      // comments from Fortran applications. printf ("Calling
      // collectPreprocessorDirectivesAndCommentsForAST() to collect CPP
      // directives for fileNameForDirectivesAndComments = %s
      // \n",fileNameForDirectivesAndComments.c_str());
      returnListOfAttributes->collectPreprocessorDirectivesAndCommentsForAST(
          fileNameForDirectivesAndComments,
          ROSEAttributesList::e_Fortran9x_language);
    }
#endif // for #ifdef ROSE_BUILD_FORTRAN_LANGUAGE_SUPPORT
  } else {
    // Else we assume this is a C or C++ program (for which the lexical analysis
    // is identical) The lex token stream is now returned in the
    // ROSEAttributesList object.

    // DQ (8/19/2019): comment this redundant call out!  We are trying to
    // optimize the performance of the header file unparsing. DQ (8/23/2018):
    // The token stream has not been collected yet (verify). ROSE_ASSERT
    // (returnListOfAttributes->get_rawTokenStream() == NULL);
    if (returnListOfAttributes == NULL) {
#ifdef ROSE_BUILD_CPP_LANGUAGE_SUPPORT
      returnListOfAttributes =
          getPreprocessorDirectives(fileNameForDirectivesAndComments);
#endif
    }
  }
  ROSE_ASSERT(returnListOfAttributes != NULL);

  // DQ (12/15/2012): Generate the list of file ids to be considered equivalent
  // to the input source file's filename.
  returnListOfAttributes->generateFileIdListFromLineDirectives();

  // DQ (9/29/2013): Check the generated returnListOfAttributes for tokens.
  if (returnListOfAttributes->get_rawTokenStream() != NULL) {
#if DEBUG_BUILD_COMMENT_AND_CPP_DIRECTIVE_LIST
    printf("Found the raw token stream in ROSE! "
           "returnListOfAttributes->get_rawTokenStream() = %p \n",
           returnListOfAttributes->get_rawTokenStream());
#endif
  }

#if DEBUG_BUILD_COMMENT_AND_CPP_DIRECTIVE_LIST
  printf("sourceFile->getFileName() = %s \n",
         sourceFile->getFileName().c_str());
  printf("new_filename              = %s \n", new_filename.c_str());
  printf("tokenVector.size()        = %zu \n",
         getTokenStream(sourceFile).size());
#endif

#if DEBUG_BUILD_COMMENT_AND_CPP_DIRECTIVE_LIST
  printf("Leaving "
         "AttachPreprocessingInfoTreeTrav::buildCommentAndCppDirectiveList "
         "file = %s \n",
         fileNameForDirectivesAndComments.c_str());
#endif

  // DQ (4/29/2020): Introduce test for recursive call.
  isRecursiveCall = false;

  return returnListOfAttributes;
}

ROSEAttributesList *
AttachPreprocessingInfoTreeTrav::getListOfAttributes(int currentFileNameId) {
  // This function will get the list of CPP directives and comments if it
  // exists, or build it if required.  The function is called each time we come
  // to a IR node as part of the traversal. If it is a new IR node (from a file
  // not previously visited) then the associated file will be read to gather its
  // CPP directives and comments.

  // DQ (5/1/2020): This is now a data member.
  // ROSEAttributesList* currentListOfAttributes = NULL;

  // DQ (4/29/2020): Introduce test for recursive call.
  static bool isRecursiveCall = false;
  ROSE_ASSERT(isRecursiveCall == false);

  isRecursiveCall = true;

  // DQ (6/12/2020): This should always be true in the new design. This is not
  // always true. ROSE_ASSERT(currentFileNameId >= 0);

  // DQ (5/19/2013): Added test... only valid for specific test codes with
  // appropriate CPP directives. ROSE_ASSERT(currentListOfAttributes != NULL);

  // DQ (4/29/2020): Introduce test for recursive call.
  isRecursiveCall = false;

  return currentListOfAttributes;
}

// Member function: evaluateInheritedAttribute
AttachPreprocessingInfoTreeTraversalInheritedAttrribute
AttachPreprocessingInfoTreeTrav::evaluateInheritedAttribute(
    SgNode *n, AttachPreprocessingInfoTreeTraversalInheritedAttrribute
                   inheritedAttribute) {
  // This is this inherited attribute evaluation.  It is executed as a preorder
  // traversal of the AST.  We don't use anything in the inherited attribute at
  // present, however, some actions have to be executed as we first visit an IR
  // node and some have to be executed as we last vist an IR node (post-order;
  // see the evaluateSynthezidedAttribute() member function).

  // DQ (11/20/2019): Check this (should be set in constructor).
  ROSE_ASSERT(sourceFile != NULL);

  ROSE_ASSERT(n != NULL);
  // printf ("In AttachPreprocessingInfoTreeTrav::evaluateInheritedAttribute():
  // n = %p = %s \n",n,n->class_name().c_str()); SgTemplateFunctionDeclaration*
  // templateDeclaration = isSgTemplateFunctionDeclaration(n);
  SgDeclarationStatement *templateDeclaration =
      isSgTemplateFunctionDeclaration(n);
  SgDeclarationStatement *templateInstantiationDeclaration =
      isSgTemplateInstantiationFunctionDecl(n);

  if (templateDeclaration == NULL)
    templateDeclaration = isSgTemplateMemberFunctionDeclaration(n);
  if (templateDeclaration == NULL)
    templateDeclaration = isSgTemplateClassDeclaration(n);
  if (templateDeclaration == NULL)
    templateDeclaration = isSgTemplateVariableDeclaration(n);

  if (templateDeclaration != NULL) {
    // Set the flag in the inherited attribute.
    // printf ("Set the flag for this to be in a template declaration n = %p =
    // %s \n",n,n->class_name().c_str());
    inheritedAttribute.isPartOfTemplateDeclaration = true;
  } else {
    // DQ (7/1/2014): Added support for detecting when we are in a template
    // instantation.
    if (templateInstantiationDeclaration == NULL)
      templateInstantiationDeclaration =
          isSgTemplateInstantiationMemberFunctionDecl(n);
    if (templateInstantiationDeclaration == NULL)
      templateInstantiationDeclaration = isSgTemplateInstantiationDecl(n);
    // if (templateInstantiationDeclaration == NULL)
    // templateInstantiationDeclaration =
    // isSgTemplateInstantiationVariableDecl(n);
    if (templateInstantiationDeclaration != NULL) {
      inheritedAttribute.isPartOfTemplateInstantiationDeclaration = true;
    }
  }

  // Pei-Hung(9/17/2020): Check if the AST node is SgFunctionParameterList for
  // Fortran input
  if (sourceFile->get_Fortran_only() == true) {
    SgFunctionParameterList *functionParameterList =
        isSgFunctionParameterList(n);
    if (functionParameterList != NULL) {
      inheritedAttribute.isPartOfFunctionParameterList = true;
    }
  }

  // DQ (8/6/2012): Allow those associated with the declaration and not inside
  // of the template declaration. if
  // (inheritedAttribute.isPartOfTemplateDeclaration == true &&
  // templateDeclaration == NULL)
  if ((inheritedAttribute.isPartOfTemplateDeclaration == true &&
       templateDeclaration == NULL) ||
      (inheritedAttribute.isPartOfTemplateInstantiationDeclaration == true &&
       templateInstantiationDeclaration == NULL)) {
    // #if DEBUG_ATTACH_PREPROCESSING_INFO
    return inheritedAttribute;
  }

  // Check if current AST node is an SgFile object
  SgFile *currentFilePtr = isSgFile(n);
  if (currentFilePtr != NULL) {
    // Current AST node is an SgFile object, generate the corresponding list of
    // attributes

#if DEBUG_ATTACH_PREPROCESSING_INFO
    printf("=== Visiting SgSourceFile node and building current list of "
           "attributes === \n");
#endif

    // This entry should not be present, so generate the list.
    // If this is a preprocessed file then change the name so that we generate
    // the correct list for the correct file. int currentFileNameId =
    // currentFilePtr->get_file_info()->get_file_id();
    Sg_File_Info *currentFileInfo = currentFilePtr->get_file_info();
    ROSE_ASSERT(currentFileInfo != NULL);

    // DQ (11/2/2019): Commenting out this assertion so that we can support
    // attaching comments to header files. ROSE_ASSERT(sourceFile ==
    // currentFilePtr);
    if (sourceFile != currentFilePtr) {
      printf("NOTE: sourceFile = %p currentFilePtr = %p \n", sourceFile,
             currentFilePtr);
    }

    // DQ (10/25/2019): This is not a correct assertion for Fortran code.
    // DQ (9/23/2019): For C/C++ code this should be false (including all
    // headers). ROSE_ASSERT(sourceFile->get_requires_C_preprocessor() ==
    // false);

    // DQ (12/2/2018): This fails for the C/C++ snippet insertion tests.
    // DQ (12/2/2018): This fails for Fortran.
    // DQ (9/5/2018): We should have already set the
    // preprocessorDirectivesAndCommentsList, checked in getTokenStream().
    // ROSE_ASSERT(currentFilePtr->get_preprocessorDirectivesAndCommentsList()
    // != NULL); if (SageInterface::is_Fortran_language() == false)
    if (SageInterface::is_C_language() == true ||
        SageInterface::is_Cxx_language() == true) {
      // ROSE_ASSERT(currentFilePtr->get_preprocessorDirectivesAndCommentsList()
      // != NULL);
    }

    // DQ (9/7/2018): Actually the default for C/C++ code should be that
    // get_requires_C_preprocessor() == false, the other case is for C
    // preprocessed fortran code.
    int currentFileNameId =
        (currentFilePtr->get_requires_C_preprocessor() == true)
            ?
            // Sg_File_Info::getIDFromFilename(sourceFile->get_file_info()->get_filenameString())
            // :
            Sg_File_Info::getIDFromFilename(
                currentFilePtr->generate_C_preprocessor_intermediate_filename(
                    sourceFile->get_file_info()->get_filename()))
            : currentFileInfo->get_physical_file_id(source_file_id);

    // DQ (6/29/2020): We should not be adding comments and/or CPP directives to
    // IR nodes that don't have a source position.
    ROSE_ASSERT(currentFileNameId >= 0);

    // Pei-Hung (09/23/2020) For Fortran code,  target_source_file_id should be
    // same as currentFileNameId when preprocessing is required
    if (SageInterface::is_Fortran_language() == true) {
      target_source_file_id =
          (currentFilePtr->get_requires_C_preprocessor() == true)
              ?
              // Sg_File_Info::getIDFromFilename(sourceFile->get_file_info()->get_filenameString())
              // :
              Sg_File_Info::getIDFromFilename(
                  currentFilePtr->generate_C_preprocessor_intermediate_filename(
                      sourceFile->get_file_info()->get_filename()))
              : currentFileInfo->get_physical_file_id(source_file_id);
    }

    // std::cerr << "The filename " <<
    // sourceFile->get_file_info()->get_filename() << std::endl;
    // ROSE_ASSERT(attributeMapForAllFiles.find(currentFileNameId) ==
    // attributeMapForAllFiles.end());

    if (currentFileNameId != target_source_file_id) {
      return inheritedAttribute;
    }

    // DQ (6/3/2020): We now handle one file at a time and the
    // currentListOfAttributes is a member of the traversal. DQ (8/19/2019):
    // Avoid processing this if we are optimizing the header file unparsing and
    // only processing the header files. ROSEAttributesList*
    // currentListOfAttributes = NULL;
  }

  // Move attributes from the list of attributes into the collection of the
  // current AST nodes, we only consider statements for the moment, but this
  // needs to be refined further on. Probably we will have to consider each
  // SgLocatedNode IR node within the AST. if (dynamic_cast<SgStatement*>(n) !=
  // NULL)
  SgStatement *statement = isSgStatement(n);
  // Liao 11/2/2010, Ideally we should put all SgLocatedNode here,
  // But we start with statements and initialized names first
  SgInitializedName *i_name = isSgInitializedName(n);
  SgAggregateInitializer *a_initor = isSgAggregateInitializer(n);

  // Pei-Hung (9/17/2020): comment and preprocess information will not be
  // attached to SgInitializedName that is part of the SgFunctionParameterList

  if (statement != NULL ||
      (i_name != NULL &&
       inheritedAttribute.isPartOfFunctionParameterList == false) ||
      a_initor != NULL) {
    SgLocatedNode *currentLocNodePtr = NULL;
    int line = 0;

    // DQ (12/9/2016): Eliminating a warning that we want to be an error:
    // -Werror=unused-but-set-variable. int col  = 0;

    // The following should always work since each statement is a located node
    // currentLocNodePtr = dynamic_cast<SgLocatedNode*>(n);
    currentLocNodePtr = isSgLocatedNode(n);
    ROSE_ASSERT(currentLocNodePtr != NULL);

    // Attach the comments only to nodes from the same file
    ROSE_ASSERT(currentLocNodePtr->get_file_info() != NULL);
    // int currentFileNameId =
    // currentLocNodePtr->get_file_info()->get_file_id();
    Sg_File_Info *currentFileInfo = currentLocNodePtr->get_file_info();
    ROSE_ASSERT(currentFileInfo != NULL);

    // DQ (12/2/2018): Oddly enough, this case does not appear to fail in the
    // C/C++ snippet insertion tests. DQ (12/2/2018): This fails for Fortran. DQ
    // (9/7/2018): Assert this as default for C/C++ file processing tests only
    // (remove later). ROSE_ASSERT(sourceFile->get_requires_C_preprocessor() ==
    // false); if (SageInterface::is_Fortran_language() == false)
    if (SageInterface::is_C_language() == true ||
        SageInterface::is_Cxx_language() == true) {
      ROSE_ASSERT(sourceFile->get_requires_C_preprocessor() == false);
    }

    int currentFileNameId =
        (sourceFile->get_requires_C_preprocessor() == true)
            ? Sg_File_Info::getIDFromFilename(
                  sourceFile->generate_C_preprocessor_intermediate_filename(
                      sourceFile->get_file_info()->get_filename()))
            : currentFileInfo->get_physical_file_id(source_file_id);

    // DQ (11/2/2019): Adding debugging code.
    // if (currentFileNameId == 0)
    // DQ (4/29/2020): This is a redundnat call but it.
    // DQ (11/2/2019): This is the call that can be redundant.
    ROSEAttributesList *currentListOfAttributes =
        getListOfAttributes(currentFileNameId);

    // DQ (8/22/2018): This can be NULL!
    // ROSE_ASSERT(currentListOfAttributes != NULL);

    // If currentListOfAttributes == NULL then this was not an IR node from a
    // file where we wanted to include CPP directives and comments.
    if (currentListOfAttributes != NULL) {
      // DQ (6/20/2005): Compiler generated is not enough, it must be marked for
      // output explicitly bool isCompilerGenerated =
      // currentLocNodePtr->get_file_info()->isCompilerGenerated();
      bool isCompilerGenerated = currentLocNodePtr->get_file_info()
                                     ->isCompilerGeneratedNodeToBeUnparsed();

      // JJW (6/25/2008): These are always flagged as "to be unparsed", even if
      // they are not unparsed because their corresponding declarations aren't
      // unparsed
      if (isSgClassDefinition(currentLocNodePtr) ||
          isSgFunctionDefinition(currentLocNodePtr)) {
        SgLocatedNode *ln = isSgLocatedNode(currentLocNodePtr->get_parent());
        Sg_File_Info *parentFi = ln ? ln->get_file_info() : NULL;
        if (parentFi && parentFi->isCompilerGenerated() &&
            !parentFi->isCompilerGeneratedNodeToBeUnparsed()) {
          isCompilerGenerated = false;
        }
      }

      bool isTransformation =
          currentLocNodePtr->get_file_info()->isTransformation();

      // Try to not call get_filename() if it would be inappropriate (either
      // when isCompilerGenerated || isTransformation)

      int currentLocNode_physical_file_id =
          currentLocNodePtr->get_file_info()->get_physical_file_id();
      string currentLocNode_physical_filename_from_id =
          Sg_File_Info::getFilenameFromID(currentLocNode_physical_file_id);

      // Pei-Hung (2/25/2020): If CPP is required, then we should use
      // currentFileNameId here to use the preprocessed input file.  Otherwise,
      // all the preprocessed information is not attached to AST.  Comments and
      // directives will not be unparsed. DQ (11/3/2019): I think we want the
      // source_file_id below, since they used to be that currentFileNameId and
      // source_file_id had the same value, but this didn't allow us to support
      // the header file unparsing. Or perhaps it didn't allow the support of
      // the optimization of the header file unparsing. DQ (5/24/2005): Relaxed
      // to handle compiler generated and transformed IR nodes if (
      // isCompilerGenerated || isTransformation || currentFileNameId ==
      // fileIdForOriginOfCurrentLocatedNode ) if ( isCompilerGenerated ||
      // isTransformation || source_file_id ==
      // fileIdForOriginOfCurrentLocatedNode ) if ( source_file_id ==
      // fileIdForOriginOfCurrentLocatedNode ) DQ (4/16/2020): This is the cause
      // of a redundant inclusion of a CPP directive and comment in test8.
      // Basically, the issue is that the evaluation of the inherited attribute
      // is causing it to be attached and the evaluation of the synthesized
      // attribute is also causing it to be attached. If this is a fix then I
      // need to work with Pei-Hung. Or the issue is that the Preprocessor list
      // iterator is not being properly increments, and so this is why both
      // attribute evaluation functions are adding the include directive in
      // test8. if ( ((sourceFile->get_requires_C_preprocessor() == true) ?
      // currentFileNameId : source_file_id) == currentLocNode_physical_file_id
      // ) if ( source_file_id == fileIdForOriginOfCurrentLocatedNode )
      if (((sourceFile->get_requires_C_preprocessor() == true)
               ? currentFileNameId
               : source_file_id) == currentLocNode_physical_file_id) {
        // DQ (11/3/2019): Check that the comment or CPP directive is from the
        // same file as the locatedNode. A variation of this test might be
        // required later, though we should only be attacheing comments and CPP
        // directives before possible transformations. if
        // (currentLocNodePtr->get_file_info()->get_filename() !=
        // currentListOfAttributes->getFileName())

        // int currentLocNode_physical_file_id =
        // currentLocNodePtr->get_file_info()->get_physical_file_id(); string
        // currentLocNode_physical_filename_from_id =
        // Sg_File_Info::getFilenameFromID(currentLocNode_physical_file_id);
        // ROSE_ASSERT(currentLocNodePtr->get_file_info()->get_filename() ==
        // currentListOfAttributes->getFileName());
        // ROSE_ASSERT(currentLocNodePtr->get_file_info()->get_physical_filename()
        // == currentListOfAttributes->getFileName()); DQ (11/4/2019): if we
        // allow isCompilerGenerated || isTransformation above, then we need to
        // uncomment this if statement.
        if (!isCompilerGenerated && !isTransformation) {
          // DQ (12/4/2019): This fails for Fortran code so skip the test when
          // using Fortran. ROSE_ASSERT(currentLocNode_physical_filename_from_id
          // == currentListOfAttributes->getFileName());
          if ((sourceFile->get_Fortran_only() == false)) {
            // DQ (1/5/2021): Adding debugging code now that the filename of the
            // token stream and the comments and CPP directives is computed
            // based on the output filename for the source file. Relevant when a
            // single source file is read twice to build two different files.
            if (currentLocNode_physical_filename_from_id !=
                currentListOfAttributes->getFileName()) {
              printf("Error: currentLocNode_physical_filename_from_id = %s \n",
                     currentLocNode_physical_filename_from_id.c_str());
              printf(" ----- currentListOfAttributes->getFileName()   = %s \n",
                     currentListOfAttributes->getFileName().c_str());
            }
          } else {
          }
        }

        // DQ (2/28/2019): We need to return the line that is associated with
        // the source file where this can be a node shared between multiple
        // ASTs. Current node belongs to the file the name of which has been
        // specified on the command line line =
        // currentLocNodePtr->get_file_info()->get_line(); line =
        // currentLocNodePtr->get_file_info()->get_physical_line();
        line = currentLocNodePtr->get_file_info()->get_physical_line(
            source_file_id);

        // DQ (8/19/2019): This is not the best place to isolate the two phases
        // of processing the source file from the processin og the headers. DQ
        // (8/19/2019): If we want to defer the insertion of CPP directives from
        // header files into the AST then we need to be able to call this
        // function later. But since this is a recursive function maybe we could
        // just call the whole process to insert comments and CPP directives
        // into ROSE later, however, that would mark the nodes as transformed.

        // Or maybe we could do it once where the comments and CPP directives
        // are inserted into the main source file, and then later when they are
        // inserted into all of the header files. Iterate over the list of
        // comments and directives and add them to the AST
        bool reset_start_index = false;
        iterateOverListAndInsertPreviouslyUninsertedElementsAppearingBeforeLineNumber(
            currentLocNodePtr, line, PreprocessingInfo::before,
            reset_start_index, currentListOfAttributes);
        // save the previous node (in an accumulator attribute), but handle some
        // nodes differently to avoid having comments attached to them since
        // they are not unparsed directly. printf ("currentLocNodePtr = %p = %s
        // \n",currentLocNodePtr,currentLocNodePtr->class_name().c_str());
        // setupPointerToPreviousNode(currentLocNodePtr);
        // DQ (6/26/2020): Avoid setting the previousLocatedNode to a
        // SgInitializedName in a variable declaration.
        SgInitializedName *initializedName =
            isSgInitializedName(currentLocNodePtr);
        if (initializedName != NULL) {
        } else {
          // DQ (6/17/2020): Set the previousLocatedNode
          previousLocatedNode = currentLocNodePtr;
          // DQ (4/28/2021): Adding assertion.
          ROSE_ASSERT(previousLocatedNode != NULL);
        }

      }
      // Debugging output
      else {
      }
    } // end if current list of attribute is not empty

  } // end if statement or init name

  return inheritedAttribute;
}

// Member function: evaluateSynthesizedAttribute
AttachPreprocessingInfoTreeTraversalSynthesizedAttribute
AttachPreprocessingInfoTreeTrav::evaluateSynthesizedAttribute(
    SgNode *n,
    AttachPreprocessingInfoTreeTraversalInheritedAttrribute inheritedAttribute,
    SubTreeSynthesizedAttributes synthiziedAttributeList) {
  // DQ (11/29/2008): FIXME: Note that this traversal does not use its
  // inheritedAttribute or synthiziedAttributeList attributes, so it could be
  // expressed as a much simpler visit traversal.  We might do that later, if we
  // decide that we REALLY don't require inheritedAttribute or
  // synthiziedAttributeList attributes.

  AttachPreprocessingInfoTreeTraversalSynthesizedAttribute
      returnSynthesizeAttribute;

  // DQ (8/6/2012): Allow those associated with the declaration and not inside
  // of the template declaration.
  ROSE_ASSERT(n != NULL);
  // printf ("In
  // AttachPreprocessingInfoTreeTrav::evaluateSynthesizedAttribute(): n = %p =
  // %s \n",n,n->class_name().c_str());
  SgDeclarationStatement *templateDeclaration =
      isSgTemplateFunctionDeclaration(n);

  if (templateDeclaration == NULL)
    templateDeclaration = isSgTemplateMemberFunctionDeclaration(n);
  if (templateDeclaration == NULL)
    templateDeclaration = isSgTemplateClassDeclaration(n);
  if (templateDeclaration == NULL)
    templateDeclaration = isSgTemplateVariableDeclaration(n);

  SgDeclarationStatement *templateInstantiationDeclaration =
      isSgTemplateInstantiationFunctionDecl(n);
  if (templateInstantiationDeclaration == NULL)
    templateInstantiationDeclaration =
        isSgTemplateInstantiationMemberFunctionDecl(n);
  if (templateInstantiationDeclaration == NULL)
    templateInstantiationDeclaration = isSgTemplateInstantiationDecl(n);
  // if (templateInstantiationDeclaration == NULL)
  // templateInstantiationDeclaration =
  // isSgTemplateInstantiationVariableDecl(n);

  // DQ (7/1/2014): Modify to avoid use of CPP directives in both template
  // declarations and template instantiations (which might not be unparsed). if
  // (inheritedAttribute.isPartOfTemplateDeclaration == true ) if
  // (inheritedAttribute.isPartOfTemplateDeclaration == true &&
  // templateDeclaration == NULL)
  if ((inheritedAttribute.isPartOfTemplateDeclaration == true &&
       templateDeclaration == NULL) ||
      (inheritedAttribute.isPartOfTemplateInstantiationDeclaration == true &&
       templateInstantiationDeclaration == NULL)) {
    // #if DEBUG_ATTACH_PREPROCESSING_INFO
    return returnSynthesizeAttribute;
  }

  // DQ (3/4/2016): Klocworks reports a problem with
  // "isSgClassDeclaration(n)->get_endOfConstruct() != NULL". These used to be a
  // problem, so we can continue to test these specific cases. ROSE_ASSERT
  // (isSgCaseOptionStmt(n)   == NULL || isSgCaseOptionStmt(n)->get_body() !=
  // NULL);

  // DQ (3/4/2016): Klocworks reports a problem with
  // "isSgClassDeclaration(n)->get_endOfConstruct() != NULL". ROSE_ASSERT
  // (isSgClassDeclaration(n) == NULL ||
  // isSgClassDeclaration(n)->get_endOfConstruct() != NULL); ROSE_ASSERT
  // (isSgClassDeclaration(n) == NULL || (isSgClassDeclaration(n) != NULL &&
  // isSgClassDeclaration(n)->get_endOfConstruct() != NULL) );
  SgClassDeclaration *classDeclaration = isSgClassDeclaration(n);
  ROSE_ASSERT(classDeclaration == NULL ||
              classDeclaration->get_endOfConstruct() != NULL);

  // Only process SgLocatedNode object and the SgFile object
  // SgFile* fileNode           = dynamic_cast<SgFile*>(n);
  // SgLocatedNode* locatedNode = dynamic_cast<SgLocatedNode*>(n);
  SgFile *fileNode = isSgFile(n);

  SgLocatedNode *locatedNode = isSgLocatedNode(n);

  // DQ (6/10/2020): We only care is the locatedNode is non-null now.
  // if ( (locatedNode != NULL) || (fileNode != NULL) )
  // if (locatedNode != NULL)
  if ((locatedNode != NULL) || (fileNode != NULL)) {
    // Attach the comments only to nodes from the same file
    // int fileNameId = currentFileNameId;
    // ROSE_ASSERT(locatedNode->get_file_info() != NULL);
    int currentFileNameId = -9;
    if (locatedNode != NULL) {
      ROSE_ASSERT(locatedNode->get_file_info() != NULL);
      // currentFileNameId = locatedNode->get_file_info()->get_file_id();
      currentFileNameId =
          locatedNode->get_file_info()->get_physical_file_id(source_file_id);
    } else {
      // ROSE_ASSERT(fileNode->get_file_info() != NULL);
      // currentFileNameId = fileNode->get_file_info()->get_file_id();
      Sg_File_Info *currentFileInfo = sourceFile->get_file_info();
      ROSE_ASSERT(currentFileInfo != NULL);
      // Newer version of code using the physical source code position.
      currentFileNameId =
          (sourceFile->get_requires_C_preprocessor() == true)
              ? Sg_File_Info::getIDFromFilename(
                    sourceFile->generate_C_preprocessor_intermediate_filename(
                        sourceFile->get_file_info()->get_filename()))
              : currentFileInfo->get_physical_file_id(source_file_id);
    }

    // DQ (6/25/2020): If this is not a node associated with the collect
    // comments and CPP directives for the associated file then ignore this IR
    // node.
    if (currentFileNameId != target_source_file_id) {
      return returnSynthesizeAttribute;
    }

    // DQ (12/20/2012): Adding support for physical source position.
    if (locatedNode != NULL) {
      ROSE_ASSERT(locatedNode->get_file_info()->get_physical_file_id(
                      source_file_id) == currentFileNameId);
    }

    // DQ (10/27/2007): This is a valgrind error: use of uninitialized variable
    // below! Initialized with a value that could not match a valid file_id.
    int fileIdForOriginOfCurrentLocatedNode = -99;

    bool isCompilerGeneratedOrTransformation = false;
    int lineOfClosingBrace = 0;
    if (locatedNode != NULL) {
      ROSE_ASSERT(locatedNode->get_file_info());
      // printf ("Calling locatedNode->get_file_info()->get_filename() \n");

      // DQ (6/20/2005): Compiler generated IR nodes to be output are now marked
      // explicitly! isCompilerGeneratedOrTransformation =
      // locatedNode->get_file_info()->isCompilerGenerated() ||
      //                                       locatedNode->get_file_info()->isTransformation()
      //                                       ||
      isCompilerGeneratedOrTransformation =
          locatedNode->get_file_info()->isCompilerGeneratedNodeToBeUnparsed() ||
          locatedNode->get_file_info()->isTransformation();

      // bool isCompilerGenerated =
      // currentLocNodePtr->get_file_info()->isCompilerGeneratedNodeToBeUnparsed();
      // bool isTransformation    =
      // currentLocNodePtr->get_file_info()->isTransformation();

      // DQ (6/20/2005): Notice that we use the new hasPositionInSource() member
      // function if ( isCompilerGeneratedOrTransformation == false )
      if (locatedNode->get_file_info()->hasPositionInSource() == true) {
        // fileIdForOriginOfCurrentLocatedNode =
        // locatedNode->get_file_info()->get_file_id();
        fileIdForOriginOfCurrentLocatedNode =
            locatedNode->get_file_info()->get_physical_file_id(source_file_id);
      }

      if (locatedNode->get_endOfConstruct() != NULL) {
        ROSE_ASSERT(locatedNode->get_endOfConstruct() != NULL);
        // lineOfClosingBrace = locatedNode->get_endOfConstruct()->get_line();
        lineOfClosingBrace =
            locatedNode->get_endOfConstruct()->get_physical_line(
                source_file_id);
      }
    } else {
      // handle the trivial case of a SgFile node being from it's own file

      // fileIdForOriginOfCurrentLocatedNode = currentFileNameId;
      // fileIdForOriginOfCurrentLocatedNode =
      // sourceFile->get_file_info()->get_file_id();

      Sg_File_Info *currentFileInfo = sourceFile->get_file_info();
      ROSE_ASSERT(currentFileInfo != NULL);
      fileIdForOriginOfCurrentLocatedNode =
          (sourceFile->get_requires_C_preprocessor() == true)
              ? Sg_File_Info::getIDFromFilename(
                    sourceFile->generate_C_preprocessor_intermediate_filename(
                        sourceFile->get_file_info()->get_filename()))
              : currentFileInfo->get_physical_file_id(source_file_id);

      // Use one billion as the max number of lines in a file
      const int OneBillion = 1000000000;

      lineOfClosingBrace = OneBillion;
    }

    // Make sure the astNode matches the current file's list of comments and CPP
    // directives. DQ (5/24/2005): Handle cases of isCompilerGenerated or
    // isTransformation
    if ((isCompilerGeneratedOrTransformation == true) ||
        (currentFileNameId == fileIdForOriginOfCurrentLocatedNode)) {

      // DQ (9/22/2013): This fails for the projects/haskellport tests (does not
      // appear to be related to the move to physical source position
      // information, but I can't be certain).
      // ROSE_ASSERT(processAllIncludeFiles == false || ((currentFileNameId < 0)
      // || (attributeMapForAllFiles.find(currentFileNameId) !=
      // attributeMapForAllFiles.end())));

      // ROSEAttributesList* currentListOfAttributes =
      // attributeMapForAllFiles[currentFileNameId];
      ROSEAttributesList *currentListOfAttributes =
          getListOfAttributes(currentFileNameId);
      // ROSE_ASSERT(currentListOfAttributes != NULL);
      if (currentListOfAttributes == NULL) {
        // This case is used to handle the case of the currentFileNameId being
        // negative (not a real file).
        printf("Not supporting gathering of CPP directives and comments for "
               "this file currentFileNameId = %d \n",
               currentFileNameId);
        return returnSynthesizeAttribute;
      }

      // DQ (6/10/2020): Set the previousLocNodePtr to the locatedNode.
      // SgLocatedNode* previousLocNodePtr = previousLocatedNode;
      // SgLocatedNode* previousLocNodePtr = locatedNode;
      // DQ (6/11/2020): We want to use previousLocatedNode, but it seems to
      // sometimes be NULL (need to isolate this case).
      SgLocatedNode *previousLocNodePtr = previousLocatedNode;
      if (previousLocNodePtr == NULL) {
        previousLocNodePtr = locatedNode;

        // DQ (6/12/2020): Debug where this is still NULL.
        // ROSE_ASSERT(previousLocNodePtr != NULL);
        if (previousLocNodePtr == NULL) {
          ROSE_ASSERT(n != NULL);
        }
        if (isSgSourceFile(n) == NULL) {
          ROSE_ASSERT(previousLocNodePtr != NULL);
        }
      }

      // ROSE_ASSERT(previousLocNodePtr != NULL);
      if (isSgSourceFile(n) == NULL) {
        ROSE_ASSERT(previousLocNodePtr != NULL);
      }

      switch (n->variantT()) {
        // I wanted to leave the SgFile case in the switch statement rather
        // than separating it out in a conditional statement at the top of the
        // file. case V_SgFile:
      case V_SgSourceFile: {
        // DQ (4/28/2021): Consider this as a work around.
        if (previousLocatedNode == NULL) {
          return returnSynthesizeAttribute;
        }
        ROSE_ASSERT(previousLocatedNode != NULL);
        SgLocatedNode *targetNode = previousLocatedNode;
        ROSE_ASSERT(targetNode != NULL);
        // printf ("In SgFile: previousLocNodePtr = %s
        // \n",previousLocNodePtr->sage_class_name()); printf ("In SgSourceFile:
        // initial value of targetNode = %p = %s
        // \n",targetNode,targetNode->class_name().c_str());

        // If the target is a SgBasicBlock then try to find its parent in the
        // global scope if (isSgBasicBlock(previousLocNodePtr) != NULL)
        if (isSgBasicBlock(targetNode) != NULL) {
          while ((targetNode != NULL) &&
                 (isSgGlobal(targetNode->get_parent()) == NULL)) {
            targetNode =
                dynamic_cast<SgLocatedNode *>(targetNode->get_parent());
            // printf ("loop: targetNode = %s
            // \n",targetNode->sage_class_name());
          }
        }

        ROSE_ASSERT(targetNode != NULL);
        Sg_File_Info *nodeFileInfo = targetNode->get_file_info();
        ROSE_ASSERT(nodeFileInfo != NULL);
        // This case appears for test2008_08.f90: the SgProgramHeaderStatement
        // is not present in the source code so we can't attach a comment to it.
        // if (nodeFileInfo->get_file_id() < 0)
        if (nodeFileInfo->get_physical_file_id(source_file_id) < 0) {
          // DQ (9/12/2010): This is something caught in compiling the Fortran
          // LLNL_POP code file: prognostic.F90 ROSE_ABORT(); printf ("Skipping
          // abort in processing a Fortran LLNL_POP code file: prognostic.F90
          // (unclear how to handle this error, if it is an error) \n");

          // DQ (9/25/2013): FIXME: I don't like this design using a break
          // statement at this specific location (in the middle of the case
          // implementation) in this case.
          break;

          // return returnSynthesizeAttribute;
        }
        // Iterate over the list of comments and directives and add them to the
        // AST negara1 (07/28/2011): Changed to false, since we might need to
        // re-visit some header files.
        bool reset_start_index = false;
        iterateOverListAndInsertPreviouslyUninsertedElementsAppearingBeforeLineNumber(
            targetNode, lineOfClosingBrace, PreprocessingInfo::after,
            reset_start_index, currentListOfAttributes);

        // DQ (12/19/2008): Output debugging information (needs to be output
        // before we reset the attributeMapForAllFiles map entries
        if (SgProject::get_verbose() >= 3) {
          bool processAllFiles =
              sourceFile->get_collectAllCommentsAndDirectives();
          if (processAllFiles == true)
            display("Output from collecting ALL comments and CPP directives "
                    "(across source and header files)");
          else
            display("Output from collecting comments and CPP directives in "
                    "source file only");
        }

        // DQ (1/21/2008): Original code
        // printf ("Delete Fortran Token List Size:
        // currentListOfAttributes->get_rawTokenStream()->size() = %" PRIuPTR "
        // \n",currentListOfAttributes->get_rawTokenStream()->size()); delete
        // inheritedAttribute.currentListOfAttributes; delete
        // currentListOfAttributes;

        // For now just reset the pointer to NULL, but later we might want to
        // delete the lists (to avoid a memory leak). delete
        // attributeMapForAllFiles[currentFileNameId];
        currentListOfAttributes = NULL;

        if (statementsToInsertBefore.size() > 0) {
          printf("In "
                 "AttachPreprocessingInfoTreeTrav::"
                 "evaluateSynthesizedAttribute(): case V_SgSourceFile: process "
                 "statementsToInsertBefore (size = %zu) \n",
                 statementsToInsertBefore.size());
        }
        // negara1 (08/12/2011): We reached the last AST node, so its safe to
        // insert nodes for header files bodies.
        for (list<pair<SgIncludeDirectiveStatement *, SgStatement *>>::
                 const_iterator it = statementsToInsertBefore.begin();
             it != statementsToInsertBefore.end(); it++) {
          ROSE_ASSERT(it->second != NULL);
          ROSE_ASSERT(it->first != NULL);
          SageInterface::insertStatementBefore(it->second, it->first, false);
        }

        if (statementsToInsertAfter.size() > 0) {
          printf("In "
                 "AttachPreprocessingInfoTreeTrav::"
                 "evaluateSynthesizedAttribute(): case V_SgSourceFile: process "
                 "statementsToInsertAfter (size = %zu) \n",
                 statementsToInsertAfter.size());
        }
        for (list<pair<SgIncludeDirectiveStatement *, SgStatement *>>::
                 const_iterator it = statementsToInsertAfter.begin();
             it != statementsToInsertAfter.end(); it++) {
          SgClassDefinition *classDefinition = isSgClassDefinition(it->second);
          if (classDefinition != NULL) {
            // Since the parent of SgClassDefinition is SgClassDeclaration,
            // whose implementation for child insertion is not provided, insert
            // after the last statement of SgClassDefinition instead.
            SgDeclarationStatement *lastMember =
                (classDefinition->get_members()).back();
            SageInterface::insertStatementAfter(lastMember, it->first, false);
          } else {
            SgBasicBlock *basicBlock = isSgBasicBlock(it->second);
            if (basicBlock != NULL) {
              // Do not insert after a basic block, but rather insert as the
              // last statement of the basic block.
              SageInterface::insertStatementAfter(basicBlock->lastStatement(),
                                                  it->first, false);
            } else {
              ROSE_ASSERT(it->second != NULL);
              ROSE_ASSERT(it->first != NULL);
              // Handle other scopes.
              // SgScopeStatement* scope = isSgScopeStatement(it->second);
              SgGlobal *globalScope = isSgGlobal(it->second);
              if (globalScope != NULL) {
                printf("globalScope->get_declarations().size() = %zu \n",
                       globalScope->get_declarations().size());
                if (globalScope->get_declarations().empty() == false) {
                  // When there is no statement
                  // outside of the frontend
                  // (rose_required_macros_and_functions.h),
                  // we want to put this after
                  // the last statement from
                  // rose_required_macros_and_functions.h.
                  SgStatement *firstStatement =
                      globalScope->get_declarations()[0];
                  printf("Addressing insertion "
                         "into globa scope: "
                         "firstStatement = %p "
                         "= %s \n",
                         firstStatement, firstStatement->class_name().c_str());
                  ROSE_ASSERT(firstStatement != NULL);

                  SgStatement *firstStatementAfterPreincludeStatements =
                      SageInterface::lastFrontEndSpecificStatement(globalScope);
                  ROSE_ASSERT(firstStatementAfterPreincludeStatements != NULL);
                  SageInterface::insertStatementAfter(firstStatement, it->first,
                                                      false);
                } else {
                  // DQ (11/21/2018): Adding.
                  printf("Global scope is empty! \n");
                  ROSE_ABORT();
                }
              } else {
                SageInterface::insertStatementAfter(it->second, it->first,
                                                    false);
              }
            }
          }
        }

        break;
      }

        // This case helps place the comment or directive relative to the
        // closing brace of a SgBasicBlock.
      case V_SgBasicBlock: {
        ROSE_ASSERT(locatedNode != NULL);
        ROSE_ASSERT(locatedNode->get_endOfConstruct() != NULL);

        // The following should always work since each statement is a located
        // node SgBasicBlock* basicBlock = dynamic_cast<SgBasicBlock*>(n);
        SgBasicBlock *basicBlock = isSgBasicBlock(n);
        ROSE_ASSERT(basicBlock != NULL);
        // DQ (6/8/2020): This appear to be NULL in some cases I am debugging
        // currently.
        ROSE_ASSERT(basicBlock != NULL);

        // DQ (3/23/2021): Testing the case of a lambda function with
        // Cxx11_tests/test2018_30.C.

        SgStatementExpression *statementExpression = NULL;
        // DQ (2/15/2021): I would like to insert comments and CPP directives
        // after the last statement, where it exists, instead of inside the
        // block. Only when there are no statements in the SgBasciBlock should
        // we add the comments and CPP directives inside the SgBasicBlock.
        SgStatement *previousStatement = isSgStatement(previousLocatedNode);

        // ROSE_ASSERT(previousStatement != NULL);
        if (previousStatement != NULL) {
          // SgLocatedNode* parentLocatedNode =
          // isSgLocatedNode(basicBlock->get_parent());
          SgLocatedNode *parentLocatedNode =
              isSgLocatedNode(previousStatement->get_parent());
          // SgStatementExpression* statementExpression =
          // isSgStatementExpression(parentLocatedNode);
          statementExpression = isSgStatementExpression(parentLocatedNode);
          SgStatement *enclosingStatement = NULL;
          if (statementExpression != NULL) {
            // enclosingStatement =
            // SageInterface::getEnclosingStatement(statementExpression);
            bool includingSelf = false;
            enclosingStatement = SageInterface::getEnclosingNode<SgStatement>(
                statementExpression, includingSelf);
            ROSE_ASSERT(enclosingStatement != NULL);

            previousStatement = enclosingStatement;
            // ROSE_ASSERT(enclosingStatement != statementExpression);
          } else {
            SgLambdaExp *lambdaExpression = isSgLambdaExp(parentLocatedNode);
            if (lambdaExpression != NULL) {
              bool includingSelf = false;
              enclosingStatement = SageInterface::getEnclosingNode<SgStatement>(
                  lambdaExpression, includingSelf);
              ROSE_ASSERT(enclosingStatement != NULL);
              previousStatement = enclosingStatement;
            }
          }
        }

        // if (previousStatement != NULL && previousStatement != basicBlock)
        if (previousStatement != NULL && previousStatement != basicBlock &&
            statementExpression == NULL) {
          bool reset_start_index = false;
          iterateOverListAndInsertPreviouslyUninsertedElementsAppearingBeforeLineNumber(
              previousStatement, lineOfClosingBrace, PreprocessingInfo::after,
              reset_start_index, currentListOfAttributes);
        } else {
          // ROSE_ASSERT(previousStatement != NULL);
          if (previousStatement != NULL) {
            // If the previous statement was the current basicBlock, then there
            // were no statements in the SgBasicBlock and we have to add the
            // comments inside the basic block.
            if (previousStatement == basicBlock) {
              bool reset_start_index = false;
              iterateOverListAndInsertPreviouslyUninsertedElementsAppearingBeforeLineNumber(
                  basicBlock, lineOfClosingBrace, PreprocessingInfo::inside,
                  reset_start_index, currentListOfAttributes);
            } else {
              // Use the basicBlock and mark the comments to be inside.
              bool reset_start_index = false;
              iterateOverListAndInsertPreviouslyUninsertedElementsAppearingBeforeLineNumber(
                  previousStatement, lineOfClosingBrace,
                  PreprocessingInfo::after, reset_start_index,
                  currentListOfAttributes);
            }

            // DQ (4/28/2021): Set the previousLocatedNode.
            previousLocatedNode = basicBlock;
          } else {
          }
        }
        // DQ (4/9/2005): We need to point to the SgBasicBlock and not the last
        // return statement (I think) Reset the previousLocNodePtr to the
        // current node so that all PreprocessingInfo objects will be inserted
        // relative to the current node next time. previousLocNodePtr =
        // basicBlock; DQ (6/24/2020): Set this only in the
        // evaluateInheritedAttribute() function.

        // DQ (2/18/2021): If this is a block from a gnu SgStatementExpression,
        // then don't record it as a previousLocatedNode. Original code.
        previousLocatedNode = basicBlock;

        break;
      }
        // Liao 11/2/2010, support #include within SgAggregateInitializer { }
        // e.g.
        /*
             static const char c_tree_code_type[] = {
                 'x',
                 #include "c-common.def"
             };
        */

      case V_SgAggregateInitializer: {
        ROSE_ASSERT(locatedNode != NULL);
        ROSE_ASSERT(locatedNode->get_endOfConstruct() != NULL);

        SgAggregateInitializer *target =
            dynamic_cast<SgAggregateInitializer *>(n);
        // DQ (6/8/2020): This appear to be NULL in some cases I am debugging
        // currently.
        ROSE_ASSERT(target != NULL);

        bool reset_start_index = false;
        iterateOverListAndInsertPreviouslyUninsertedElementsAppearingBeforeLineNumber(
            target, lineOfClosingBrace, PreprocessingInfo::inside,
            reset_start_index, currentListOfAttributes);

        // DQ (6/24/2020): Set this only in the evaluateInheritedAttribute()
        // function.
        previousLocatedNode = target;
        break;
      }

        // DQ (12/29/2011): Adding support for template class declarations.
      case V_SgTemplateClassDeclaration:

      case V_SgClassDeclaration: {
        ROSE_ASSERT(locatedNode != NULL);
        ROSE_ASSERT(locatedNode->get_endOfConstruct() != NULL);

        // The following should always work since each statement is a located
        // node
        SgClassDeclaration *classDeclaration =
            dynamic_cast<SgClassDeclaration *>(n);
        ROSE_ASSERT(classDeclaration != NULL);
        // DQ (6/8/2020): This appear to be NULL in some cases I am debugging
        // currently.
        ROSE_ASSERT(previousLocNodePtr != NULL);

        // DQ (2/16/2021): Refactored code so that we can support comments at
        // the end of a block being attached to the bottom (after) the last
        // statement.
        bool reset_start_index = false;
        handleBracedScopes(previousLocNodePtr, classDeclaration,
                           lineOfClosingBrace, reset_start_index,
                           currentListOfAttributes);
        // printf ("Adding comment/directive to base of class declaration \n");
        // iterateOverListAndInsertPreviouslyUninsertedElementsAppearingBeforeLineNumber
        //    ( locatedNode, lineOfClosingBrace, PreprocessingInfo::inside );

        // previousLocNodePtr = classDeclaration;
        // DQ (6/24/2020): Set this only in the evaluateInheritedAttribute()
        // function.
        previousLocatedNode = classDeclaration;
        break;
      }

        // GB (09/18/2007): Added support for preprocessing info inside typedef
        // declarations (e.g. after the base type, which is what the
        // previousLocNodePtr might point to).
      case V_SgTypedefDeclaration: {
        ROSE_ASSERT(locatedNode != NULL);
        ROSE_ASSERT(locatedNode->get_endOfConstruct() != NULL);

        SgTypedefDeclaration *typedefDeclaration = isSgTypedefDeclaration(n);
        ROSE_ASSERT(typedefDeclaration != NULL);
        // DQ (6/8/2020): This appear to be NULL in some cases I am debugging
        // currently.
        ROSE_ASSERT(previousLocNodePtr != NULL);

        bool reset_start_index = false;
        iterateOverListAndInsertPreviouslyUninsertedElementsAppearingBeforeLineNumber(
            previousLocNodePtr, lineOfClosingBrace, PreprocessingInfo::after,
            reset_start_index, currentListOfAttributes);

        // previousLocNodePtr = typedefDeclaration;
        // DQ (6/24/2020): Set this only in the evaluateInheritedAttribute()
        // function.
        previousLocatedNode = typedefDeclaration;
        break;
      }

        // DQ (12/29/2011): Adding support for template variable declarations.
      case V_SgTemplateVariableDeclaration:

        // GB (09/19/2007): Added support for preprocessing info inside variable
        // declarations (e.g. after the base type, which is what the
        // previousLocNodePtr might point to).
      case V_SgVariableDeclaration: {
        ROSE_ASSERT(locatedNode != NULL);
        ROSE_ASSERT(locatedNode->get_endOfConstruct() != NULL);

        SgVariableDeclaration *variableDeclaration = isSgVariableDeclaration(n);
        ROSE_ASSERT(variableDeclaration != NULL);
        // DQ (6/9/2020): This appear to be NULL in some cases I am debugging
        // currently.
        ROSE_ASSERT(previousLocNodePtr != NULL);

        bool reset_start_index = false;
        iterateOverListAndInsertPreviouslyUninsertedElementsAppearingBeforeLineNumber(
            previousLocNodePtr, lineOfClosingBrace, PreprocessingInfo::after,
            reset_start_index, currentListOfAttributes);

        // previousLocNodePtr = variableDeclaration;
        // DQ (6/24/2020): Set this only in the evaluateInheritedAttribute()
        // function.
        previousLocatedNode = variableDeclaration;
        break;
      }

        // DQ (10/25/2012): Added new case.  I expect this might be important
        // for test2012_78.c
      case V_SgInitializedName: {
        ROSE_ASSERT(locatedNode != NULL);
        ROSE_ASSERT(locatedNode->get_endOfConstruct() != NULL);

        SgInitializedName *initializedName = isSgInitializedName(n);
        ROSE_ASSERT(initializedName != NULL);
        // DQ (6/9/2020): This appears to be NULL in some cases I am debugging
        // currently.
        ROSE_ASSERT(previousLocNodePtr != NULL);
        // Liao 6/10/2020, Fortran subroutine will have a SgInitializedName
        // generated in AST to represent the subroutine name. It is
        // compiler-generated and has no appearance in the original source code.
        // We should not attach comments to it.
        if (SageInterface::is_Fortran_language()) {
          if (isSgProcedureHeaderStatement(initializedName->get_parent())) {
            // cout<<"Found Fortran subroutine init name, skipping attaching
            // comments to it..."<< initializedName <<endl;
          }
        } else {
          bool reset_start_index = false;
          iterateOverListAndInsertPreviouslyUninsertedElementsAppearingBeforeLineNumber(
              previousLocNodePtr, lineOfClosingBrace, PreprocessingInfo::after,
              reset_start_index, currentListOfAttributes);

          // DQ (6/24/2020): Set this only in the evaluateInheritedAttribute()
          // function.
          previousLocatedNode = initializedName;
        }
        break;
      }

      case V_SgClassDefinition: {
        ROSE_ASSERT(locatedNode != NULL);
        ROSE_ASSERT(locatedNode->get_endOfConstruct() != NULL);

        // DQ (3/19/2005): This is a more robust process (although it introduces
        // a new location for a comment/directive)
        // iterateOverListAndInsertPreviouslyUninsertedElementsAppearingBeforeLineNumber
        //    ( previousLocNodePtr, lineOfClosingBrace, PreprocessingInfo::after
        //    );
        // printf ("Adding comment/directive to base of class definition \n");
        // DQ (6/9/2020): This appear to be NULL in some cases I am debugging
        // currently.
        ROSE_ASSERT(locatedNode != NULL);

        // The following should always work since each statement is a located
        // node
        SgClassDefinition *classDefinition =
            dynamic_cast<SgClassDefinition *>(n);
        ROSE_ASSERT(classDefinition != NULL);
        // DQ (2/16/2021): Refactored code so that we can support comments at
        // the end of a block being attached to the bottom (after) the last
        // statement.
        bool reset_start_index = false;
        handleBracedScopes(previousLocNodePtr, classDefinition,
                           lineOfClosingBrace, reset_start_index,
                           currentListOfAttributes);
        // previousLocNodePtr = locatedNode;
        // previousLocatedNodeMap[currentFileNameId] = locatedNode;
        break;
      }

      case V_SgEnumDeclaration: {
        ROSE_ASSERT(locatedNode != NULL);
        ROSE_ASSERT(locatedNode->get_endOfConstruct() != NULL);

        // The following should always work since each statement is a located
        // node
        SgEnumDeclaration *enumDeclaration =
            dynamic_cast<SgEnumDeclaration *>(n);
        ROSE_ASSERT(enumDeclaration != NULL);
        // DQ (3/18/2005): This is a more robust process (although it introduces
        // a new location for a comment/directive)
        // iterateOverListAndInsertPreviouslyUninsertedElementsAppearingBeforeLineNumber
        //    ( previousLocNodePtr, lineOfClosingBrace, PreprocessingInfo::after
        //    );
        // printf ("Adding comment/directive to base of enum declaration \n");

        // DQ (6/9/2020): This appear to be NULL in some cases I am debugging
        // currently.
        ROSE_ASSERT(locatedNode != NULL);

        bool reset_start_index = false;
        iterateOverListAndInsertPreviouslyUninsertedElementsAppearingBeforeLineNumber(
            locatedNode, lineOfClosingBrace, PreprocessingInfo::inside,
            reset_start_index, currentListOfAttributes);

        // previousLocNodePtr = enumDeclaration;
        // DQ (6/24/2020): Set this only in the evaluateInheritedAttribute()
        // function.
        previousLocatedNode = enumDeclaration;
        break;
      }

      // DQ (5/3/2004): Added support for namespaces
      case V_SgNamespaceDeclarationStatement: {
        ROSE_ASSERT(locatedNode != NULL);
        ROSE_ASSERT(locatedNode->get_endOfConstruct() != NULL);

        // The following should always work since each statement is a located
        // node SgNamespaceDeclarationStatement* namespaceDeclaration =
        // dynamic_cast<SgNamespaceDeclarationStatement*>(n);
        // SgNamespaceDeclarationStatement* namespaceDeclaration =
        // isSgNamespaceDeclarationStatement(n);
        SgNamespaceDeclarationStatement *namespaceDeclaration =
            dynamic_cast<SgNamespaceDeclarationStatement *>(n);
        ROSE_ASSERT(namespaceDeclaration != NULL);

        // DQ (6/9/2020): This appear to be NULL in some cases I am debugging
        // currently.
        ROSE_ASSERT(previousLocNodePtr != NULL);
        bool reset_start_index = false;
        iterateOverListAndInsertPreviouslyUninsertedElementsAppearingBeforeLineNumber(
            previousLocNodePtr, lineOfClosingBrace, PreprocessingInfo::after,
            reset_start_index, currentListOfAttributes);

        // previousLocNodePtr = namespaceDeclaration;
        // DQ (6/24/2020): Set this only in the evaluateInheritedAttribute()
        // function.
        previousLocatedNode = namespaceDeclaration;
        break;
      }

      // DQ (5/3/2004): Added support for namespaces
      case V_SgNamespaceDefinitionStatement: {
        ROSE_ASSERT(locatedNode != NULL);
        ROSE_ASSERT(locatedNode->get_endOfConstruct() != NULL);

        // The following should always work since each statement is a located
        // node
        SgNamespaceDefinitionStatement *namespaceDefinition =
            dynamic_cast<SgNamespaceDefinitionStatement *>(n);
        ROSE_ASSERT(namespaceDefinition != NULL);

        // DQ (3/18/2005): This is a more robust process (although it introduces
        // a new location for a comment/directive)
        // iterateOverListAndInsertPreviouslyUninsertedElementsAppearingBeforeLineNumber
        //    ( previousLocNodePtr, lineOfClosingBrace, PreprocessingInfo::after
        //    );
        // printf ("Adding comment/directive to base of namespace definition
        // \n");

        // DQ (6/9/2020): This appear to be NULL in some cases I am debugging
        // currently.
        ROSE_ASSERT(previousLocNodePtr != NULL);

        bool reset_start_index = false;
        iterateOverListAndInsertPreviouslyUninsertedElementsAppearingBeforeLineNumber(
            locatedNode, lineOfClosingBrace, PreprocessingInfo::inside,
            reset_start_index, currentListOfAttributes);

        // previousLocNodePtr = namespaceDefinition;
        // DQ (6/24/2020): Set this only in the evaluateInheritedAttribute()
        // function.
        previousLocatedNode = namespaceDefinition;
        break;
      }

      // DQ (4/9/2005): Added support for templates instaiations which are
      // compiler generated
      //                but OK to attach comments to them (just not inside
      //                them!).
      case V_SgTemplateInstantiationMemberFunctionDecl: {
        ROSE_ASSERT(locatedNode != NULL);
        ROSE_ASSERT(locatedNode->get_endOfConstruct() != NULL);
        // printf ("Found a SgTemplateInstantiationMemberFunctionDecl but only
        // record it as a previousLocNodePtr \n");

        // DQ (6/9/2020): This appear to be NULL in some cases I am debugging
        // currently. DQ (3/11/2012): Added recursive call to insert comments.
        bool reset_start_index = false;
        iterateOverListAndInsertPreviouslyUninsertedElementsAppearingBeforeLineNumber(
            locatedNode, lineOfClosingBrace, PreprocessingInfo::inside,
            reset_start_index, currentListOfAttributes);

        // previousLocNodePtr = locatedNode;
        // DQ (6/24/2020): Set this only in the evaluateInheritedAttribute()
        // function.
        previousLocatedNode = locatedNode;
        // DQ (3/11/2012): Added break statement to prevent fall through, I
        // think this fixes a bug.
        break;
      }

        // DQ (5/13/2012): Added case.
      case V_SgTemplateClassDefinition:

        // DQ (3/11/2012): Added case.
      case V_SgTemplateFunctionDefinition:

        // DQ (8/12/2012): Added support for attaching comments after a
        // SgFunctionDefinition.
      case V_SgFunctionDefinition:

        // DQ (12/29/2011): Adding support for template function and member
        // function declarations.
      case V_SgTemplateFunctionDeclaration:
      case V_SgTemplateMemberFunctionDeclaration:

        // DQ (4/21/2005): this can be the last statement and if it is we have
        // to record it as such so that directives/comments can be attached
        // after it.
      case V_SgTemplateInstantiationDirectiveStatement:
        // case V_SgFunctionParameterList:
      case V_SgFunctionDeclaration: // Liao 11/8/2010, this is necessary since
                                    // SgInitializedName might be a previous
                                    // located node. we don't want to attach
                                    // anything after an ending initialized
                                    // name, So we give a chance to the init
                                    // name's ancestor a chance. For
                                    // preprocessing info appearing after a last
                                    // init name, we attach it inside the
                                    // ancestor.

      case V_SgMemberFunctionDeclaration:
      case V_SgTemplateInstantiationFunctionDecl: {
        ROSE_ASSERT(locatedNode != NULL);
        ROSE_ASSERT(locatedNode->get_endOfConstruct() != NULL);

        // DQ (6/9/2020): This appear to be NULL in some cases I am debugging
        // currently.
        ROSE_ASSERT(locatedNode != NULL);

        // DQ (3/11/2012): Added recursive call to insert comments.
        bool reset_start_index = false;
        iterateOverListAndInsertPreviouslyUninsertedElementsAppearingBeforeLineNumber(
            locatedNode, lineOfClosingBrace, PreprocessingInfo::inside,
            reset_start_index, currentListOfAttributes);

        // previousLocNodePtr = locatedNode;
        // DQ (6/24/2020): Set this only in the evaluateInheritedAttribute()
        // function.
        previousLocatedNode = locatedNode;
        // DQ (3/11/2012): Added break statement to prevent fall through, I
        // think this fixes a bug.
        break;
      }

        // The following cases are required because the fortran blocks can be
        // nest in syntax, so a comment after the block should be after the
        // closing syntax for the consruct containing the block. DQ (3/30/2021):
        // Adding to support comments after statements which contain
        // SgBasicBlock nodes.
      case V_SgIfStmt: {
        // I might need an example of this.
        if (SageInterface::is_Fortran_language() == true) {
          previousLocatedNode = locatedNode;
        }
      }

        // DQ (3/30/2021): Adding to support comments after statements which
        // contain SgBasicBlock nodes.
      case V_SgDoWhileStmt: {
        // I might need an example of this.
        if (SageInterface::is_Fortran_language() == true) {
          previousLocatedNode = locatedNode;
        }
      }

        // DQ (3/30/2021): Adding to support comments after statements which
        // contain SgBasicBlock nodes.
      case V_SgFortranDo: {
        // test2021_01.f90 through test2021_04.f90 are examples of this issue,
        // but it fails in the OpenMP tests for Fortran. Note: Craign things
        // that the case of a loop ending on a label might be an issue.
        previousLocatedNode = locatedNode;
      }

      default: {
        // DQ (11/11/2012): Added assertion.
        ROSE_ASSERT(n != NULL);

#if DEBUG_ATTACH_PREPROCESSING_INFO
        ROSE_ASSERT(n->get_file_info() != NULL);
        n->get_file_info()->display(
            "Skipping any possability of attaching a comment/directive: debug");
#endif
      }
      }

      // DQ (6/17/2020): Need to check for null pointer.
      ROSE_ASSERT(previousLocatedNode != NULL);

      ROSE_ASSERT(previousLocatedNode->get_file_info() != NULL);
      if (previousLocatedNode->get_file_info()->get_physical_file_id() !=
          target_source_file_id) {
        printf("Error: previousLocatedNode->get_file_info()->get_file_id() != "
               "target_source_file_id \n");
        previousLocatedNode->get_file_info()->display(
            "Error: previousLocatedNode->get_file_info()->get_file_id() != "
            "target_source_file_id");
        printf(
            " --- previousLocatedNode->get_file_info()->get_file_id() = %d \n",
            previousLocatedNode->get_file_info()->get_file_id());
        printf(
            " --- target_source_file_id                               = %d \n",
            target_source_file_id);
      }
      ROSE_ASSERT(
          previousLocatedNode->get_file_info()->get_physical_file_id() ==
          target_source_file_id);

    } // if compiler generated or match current file

  } // end if ((locatedNode != NULL) || (fileNode != NULL))

  // DQ (6/15/2020): Set the previous node to be the current node as we leave
  // evaluateSynthesizedAttribute().
  if (previousLocatedNode == NULL) {
    previousLocatedNode = isSgLocatedNode(n);
  }

  return returnSynthesizeAttribute;
}

// ifndef  CXX_IS_ROSE_CODE_GENERATION
// #endif
