#ifndef _ATTACH_PREPROCESSING_INFO_TRAVERSAL_H_
#define _ATTACH_PREPROCESSING_INFO_TRAVERSAL_H_

#include "AstProcessing.h"

#include <string>

class AttachPreprocessingInfoTreeTraversalInheritedAttrribute {
public:
  bool isPartOfTemplateDeclaration = false;
  bool isPartOfTemplateInstantiationDeclaration = false;
  bool isPartOfFunctionParameterList = false;
  bool isPartOfAuxiliaryDeclaration = false;
  bool isPartOfSemanticDeclarationScope = false;
};

class AttachPreprocessingInfoTreeTraversalSynthesizedAttribute {};

class AttachPreprocessingInfoTreeTrav
    : public SgTopDownBottomUpProcessing<
          AttachPreprocessingInfoTreeTraversalInheritedAttrribute,
          AttachPreprocessingInfoTreeTraversalSynthesizedAttribute> {
protected:
  SgLocatedNode *previousLocatedNode;

  ROSEAttributesList *currentListOfAttributes;

  SgSourceFile *sourceFile;

  // DQ (6/23/2020): We only want to process located nodes that are associated
  // with this file id.
  int target_source_file_id;

  // DQ (2/28/2019): We need to return the line that is associated with the
  // source file where this can be a node shared between multiple ASTs.
  int source_file_id;

  int start_index;

public:
  ~AttachPreprocessingInfoTreeTrav();

  AttachPreprocessingInfoTreeTrav(SgSourceFile *file,
                                  ROSEAttributesList *listOfAttributes);
  void
  iterateOverListAndInsertPreviouslyUninsertedElementsAppearingBeforeLineNumber(
      SgLocatedNode *locatedNode, int lineNumber,
      PreprocessingInfo::RelativePositionType location, bool reset_start_index,
      ROSEAttributesList *currentListOfAttributes);

  // DQ (2/16/2021): This function supports
  // iterateOverListAndInsertPreviouslyUninsertedElementsAppearingBeforeLineNumber().
  // It separates the case where comments and CPP directives are put in the
  // scope or attached to the bottom of the previous statement.
  void handleBracedScopes(SgLocatedNode *previousLocatedNode,
                          SgStatement *bracedScope, int lineOfClosingBrace,
                          ROSEAttributesList *currentListOfAttributes);

  // Member function to be executed on each node of the AST
  // in the course of its traversal
  AttachPreprocessingInfoTreeTraversalInheritedAttrribute
  evaluateInheritedAttribute(
      SgNode *n, AttachPreprocessingInfoTreeTraversalInheritedAttrribute inh);

  AttachPreprocessingInfoTreeTraversalSynthesizedAttribute
  evaluateSynthesizedAttribute(
      SgNode *n, AttachPreprocessingInfoTreeTraversalInheritedAttrribute inh,
      SubTreeSynthesizedAttributes st);

  static ROSEAttributesList *
  buildCommentAndCppDirectiveList(SgSourceFile *sourceFile,
                                  std::string currentFilename,
                                  std::string new_filename);
};

#endif
