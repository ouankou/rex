// This file implements the extraction of the attributes (comments and
// preprocessor directives) from the original source file and their insertion
// into the AST data structure.
// The idea is to re-introduce them (as correctly as possible) when the
// transformed AST is unparsed later on.

#include "sage3basic.h"

#include "attachPreprocessingInfo.h"

#include "attachPreprocessingInfoTraversal.h"

#include "rose_config.h"

#include "tokenStreamMapping.h"

#include <set>

using namespace std;

namespace {
void rosePhaseTrace(const char *phase) {
  if (getenv("ROSE_PHASE_TRACE") != nullptr) {
    fprintf(stderr, "ROSE_PHASE %s\n", phase);
    fflush(stderr);
  }
}

bool isCommentPreprocessingInfo(const PreprocessingInfo *info) {
  ASSERT_not_null(info);

  switch (info->getTypeOfDirective()) {
  case PreprocessingInfo::C_StyleComment:
  case PreprocessingInfo::CplusplusStyleComment:
  case PreprocessingInfo::FortranStyleComment:
  case PreprocessingInfo::F90StyleComment:
    return true;

  default:
    return false;
  }
}

bool isSameLineNamespaceClosingComment(SgLocatedNode *locatedNode,
                                       const PreprocessingInfo *info,
                                       int source_file_id) {
  if (!isCommentPreprocessingInfo(info)) {
    return false;
  }

  SgNamespaceDeclarationStatement *namespace_decl =
      isSgNamespaceDeclarationStatement(locatedNode);
  if (namespace_decl == NULL) {
    return false;
  }

  if (!namespace_decl->has_source_fragments()) {
    fprintf(stderr,
            "REX_FRONTEND_INVARIANT[namespace-source-fragment]: namespace "
            "closing-comment anchor has no typed source fragments\n");
    ROSE_ABORT();
  }
  namespace_decl->validate_source_fragments();
  SgNamespaceSourceFragment *closing =
      namespace_decl->get_closing_source_fragment();
  ROSE_ASSERT(closing != NULL);
  closing->validate();
  Sg_File_Info *namespace_end = closing->get_endOfConstruct();
  ROSE_ASSERT(namespace_end != NULL);

  int namespace_end_line = namespace_end->get_physical_line(source_file_id);
  if (namespace_end_line <= 0) {
    fprintf(stderr,
            "REX_FRONTEND_INVARIANT[namespace-source-fragment]: namespace "
            "closing fragment has no exact physical line in output file\n");
    ROSE_ABORT();
  }

  if (namespace_end_line != info->getLineNumber()) {
    return false;
  }

  const int namespace_end_col = namespace_end->get_col();
  const int comment_col = info->getColumnNumber();
  return namespace_end_col <= 0 || comment_col <= 0 ||
         namespace_end_col < comment_col;
}

} // namespace
using namespace Rose;

// It is needed because otherwise, the default destructor breaks something.

AttachPreprocessingInfoTreeTrav::~AttachPreprocessingInfoTreeTrav() {
  // do nothing
}

AttachPreprocessingInfoTreeTrav::AttachPreprocessingInfoTreeTrav(
    SgSourceFile *file, ROSEAttributesList *listOfAttributes) {
  if (file == NULL || file->get_file_info() == NULL ||
      listOfAttributes == NULL) {
    fprintf(stderr,
            "REX_AST_INVARIANT[preprocessing-source]: file=%p attributes=%p "
            "has incomplete typed preprocessing input\n",
            static_cast<void *>(file), static_cast<void *>(listOfAttributes));
    ROSE_ABORT();
  }
  sourceFile = file;
  source_file_id = sourceFile->get_file_info()->get_physical_file_id();
  if (source_file_id < 0) {
    fprintf(stderr,
            "REX_AST_INVARIANT[preprocessing-source]: file=%p has invalid "
            "physical file ID=%d\n",
            static_cast<void *>(file), source_file_id);
    ROSE_ABORT();
  }

  currentListOfAttributes = listOfAttributes;
  start_index = 0;
  previousLocatedNode = NULL;
  target_source_file_id = source_file_id;
}

void AttachPreprocessingInfoTreeTrav::handleBracedScopes(
    SgLocatedNode *previousLocatedNode, SgStatement *bracedScope,
    int lineOfClosingBrace, ROSEAttributesList *currentListOfAttributes) {
  // DQ (2/16/2021): This function supports
  // iterateOverListAndInsertPreviouslyUninsertedElementsAppearingBeforeLineNumber().
  // It separates the case where comments and CPP directives are put in the
  // scope or attached to the bottom of the previous statement.

  SgStatement *previousStatement = isSgStatement(previousLocatedNode);

  if (previousStatement != NULL && previousStatement != bracedScope) {
    bool reset_start_index = false;
    iterateOverListAndInsertPreviouslyUninsertedElementsAppearingBeforeLineNumber(
        previousStatement, lineOfClosingBrace, PreprocessingInfo::after,
        reset_start_index, currentListOfAttributes);
  } else {
    if (previousStatement != NULL && previousStatement == bracedScope) {
      bool reset_start_index = false;
      iterateOverListAndInsertPreviouslyUninsertedElementsAppearingBeforeLineNumber(
          bracedScope, lineOfClosingBrace, PreprocessingInfo::inside,
          reset_start_index, currentListOfAttributes);
    }
  }
}

void AttachPreprocessingInfoTreeTrav::
    iterateOverListAndInsertPreviouslyUninsertedElementsAppearingBeforeLineNumber(
        SgLocatedNode *locatedNode, int lineNumber,
        PreprocessingInfo::RelativePositionType location,
        bool reset_start_index, ROSEAttributesList *currentListOfAttributes) {
  static bool isRecursiveCall = false;
  ROSE_ASSERT(!isRecursiveCall);
  ROSE_ASSERT(currentListOfAttributes != NULL);
  ROSE_ASSERT(locatedNode != NULL);
  ROSE_ASSERT(locatedNode->get_startOfConstruct() != NULL);

  isRecursiveCall = true;

  int sizeOfCurrentListOfAttributes = currentListOfAttributes->size();

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
  if (sourceFile->get_Fortran_only()) {
    if (isSgBasicBlock(locatedNode) && (location == PreprocessingInfo::before ||
                                        location == PreprocessingInfo::after)) {
      isFortranBlockAndBeforePoisition = true;
    }
  }

  if (!isFortranBlockAndBeforePoisition) {
    for (int i = start_index; i < sizeOfCurrentListOfAttributes; i++) {
      PreprocessingInfo *currentPreprocessingInfoPtr =
          (*currentListOfAttributes)[i];

      ROSE_ASSERT(currentPreprocessingInfoPtr != NULL);
      ROSE_ASSERT(currentPreprocessingInfoPtr->get_file_info() != NULL);
      int currentPreprocessingInfoLineNumber =
          currentPreprocessingInfoPtr->getLineNumber();

      int currentPreprocessingInfoColumnNumber =
          currentPreprocessingInfoPtr->getColumnNumber();

      Sg_File_Info *end = locatedNode->get_endOfConstruct();
      ROSE_ASSERT(end != NULL);
      int ending_col = end->get_col();

      bool attachCommentOrDirective =
          (currentPreprocessingInfoLineNumber < lineNumber) ||
          ((currentPreprocessingInfoLineNumber == lineNumber) &&
           (currentPreprocessingInfoColumnNumber < ending_col));

      SgFunctionDeclaration *function = isSgFunctionDeclaration(locatedNode);
      SgClassDeclaration *closure = isSgClassDeclaration(locatedNode);
      SgClassDefinition *function_scope =
          function != nullptr ? isSgClassDefinition(function->get_scope())
                              : nullptr;
      if (function_scope != nullptr)
        closure = function_scope->get_declaration();
      if (closure != nullptr &&
          closure->get_class_type() == SgClassDeclaration::e_lambda_closure) {
        attachCommentOrDirective = false;
      }

      if (attachCommentOrDirective == true) {
        start_index = i + 1;
        // Mark the location relative to the current node where the
        // PreprocessingInfo object should be unparsed (before or after)
        // relative to the current locatedNode
        PreprocessingInfo::RelativePositionType effectiveLocation = location;
        if (location == PreprocessingInfo::after &&
            isSameLineNamespaceClosingComment(
                locatedNode, currentPreprocessingInfoPtr, source_file_id)) {
          effectiveLocation = PreprocessingInfo::after_syntax;
        }
        currentPreprocessingInfoPtr->setRelativePosition(effectiveLocation);
        locatedNode->addToAttachedPreprocessingInfo(
            currentPreprocessingInfoPtr);
      }
    }
  }

  // DQ (12/12/2008): We should not need this state, so why support resetting
  // it, unless the traversal needs to be called multiple times. DQ (4/13/2007):
  // The evaluation of the synthesized attribute for a SgFile will trigger the
  // reset of the start index to 0.
  if (reset_start_index == true) {
    start_index = 0;
  }

  isRecursiveCall = false;
}

void buildTokenStreamMapping(SgSourceFile *sourceFile,
                             vector<stream_element *> &tokenVector);

ROSEAttributesList *
AttachPreprocessingInfoTreeTrav::buildCommentAndCppDirectiveList(
    SgSourceFile *sourceFile, std::string fileNameForDirectivesAndComments,
    std::string new_filename) {
  // This function abstracts the collection of comments and CPP directives into
  // a list. The list is then used to draw from as the AST is traversed and the
  // list elements are woven into the AST.

  TimingPerformance timer("AST buildCommentAndCppDirectiveList():");

  static bool isRecursiveCall = false;
  if (isRecursiveCall || sourceFile == NULL ||
      sourceFile->get_file_info() == NULL ||
      sourceFile->get_preprocessorDirectivesAndCommentsList() == NULL) {
    fprintf(stderr,
            "REX_FRONTEND_INVARIANT[preprocessing-collector]: recursive or "
            "incomplete typed source input\n");
    ROSE_ABORT();
  }
  isRecursiveCall = true;

  ROSEAttributesList *returnListOfAttributes = NULL;
  ROSEAttributesListContainerPtr filePreprocInfo =
      sourceFile->get_preprocessorDirectivesAndCommentsList();

  const bool isFortran = sourceFile->get_Fortran_only();

  if (!isFortran) {
    auto &attributeLists = filePreprocInfo->getList();
    const std::string attributes_filename =
        new_filename.empty() ? fileNameForDirectivesAndComments : new_filename;
    auto existing = attributeLists.find(attributes_filename);
    if (existing == attributeLists.end()) {
#ifdef ROSE_BUILD_CPP_LANGUAGE_SUPPORT
      rosePhaseTrace(
          "buildCommentAndCppDirectiveList.getPreprocessorDirectives.begin");
      returnListOfAttributes = getPreprocessorDirectives(
          fileNameForDirectivesAndComments, new_filename);
      rosePhaseTrace(
          "buildCommentAndCppDirectiveList.getPreprocessorDirectives.end");
#endif

      if (returnListOfAttributes == NULL ||
          returnListOfAttributes->get_rawTokenStream() == NULL) {
        fprintf(stderr,
                "REX_FRONTEND_INVARIANT[preprocessing-collector]: source "
                "produced no token stream\n");
        ROSE_ABORT();
      }

      const bool needs_token_mapping = sourceFile->get_unparse_tokens();
      if (needs_token_mapping) {
        rosePhaseTrace(
            "buildCommentAndCppDirectiveList.tokenMappingSetup.begin");
        LexTokenStreamType *tokenStream =
            returnListOfAttributes->get_rawTokenStream();
        // Convert this list to a vectors so that we can use integer indexing
        // instead of iterators into a list.
        vector<stream_element *> tokenVector;
        tokenVector.reserve(tokenStream->size());
        for (LexTokenStreamType::iterator i = tokenStream->begin();
             i != tokenStream->end(); i++) {
          tokenVector.push_back(*i);
        }

        if (tokenVector.empty() == true) {
          printf(
              "Warning: this is an empty file (no tokens found): not even a CR "
              "present! (but not an error using the token stream unparsing) "
              "\n");
        }

        TimingPerformance timer("AST calling buildTokenStreamMapping():");
        buildTokenStreamMapping(sourceFile, tokenVector);
        rosePhaseTrace("buildCommentAndCppDirectiveList.tokenMappingSetup.end");
      }
    } else {
      returnListOfAttributes = existing->second;
      ROSE_ASSERT(returnListOfAttributes != NULL);
      ROSE_ASSERT(returnListOfAttributes->get_rawTokenStream() != NULL);
    }
  }

  TimingPerformance evaluate_timer("AST evaluateInheritedAttribute:");

  if (sourceFile->get_outputLanguage() == SgFile::e_Fortran_language) {
    fileNameForDirectivesAndComments = sourceFile->get_sourceFileNameWithPath();
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

    returnListOfAttributes = new ROSEAttributesList();

#ifdef ROSE_BUILD_FORTRAN_LANGUAGE_SUPPORT
    if (sourceFile->get_inputFormat() == SgFile::e_fixed_form_output_format) {
      if (SgProject::get_verbose() > 1) {
        printf("Fortran code assumed to be in fixed format form \n");
      }

      returnListOfAttributes->collectPreprocessorDirectivesAndCommentsForAST(
          fileNameForDirectivesAndComments,
          ROSEAttributesList::e_Fortran77_language);
    } else {
      returnListOfAttributes->collectPreprocessorDirectivesAndCommentsForAST(
          fileNameForDirectivesAndComments,
          ROSEAttributesList::e_Fortran9x_language);
    }
#endif // for #ifdef ROSE_BUILD_FORTRAN_LANGUAGE_SUPPORT
  }

  ROSE_ASSERT(returnListOfAttributes != NULL);
  returnListOfAttributes->generateFileIdListFromLineDirectives();

  isRecursiveCall = false;

  return returnListOfAttributes;
}

namespace {
void validateSemanticDeclarationScopePreprocessingBoundary(
    SgLocatedNode *node) {
  SgDeclarationScope *scope = isSgDeclarationScope(node);
  SgDeclarationScopeList *container = isSgDeclarationScopeList(node);
  SgFunctionParameterScope *parameterScope = isSgFunctionParameterScope(node);
  if (scope == NULL && container == NULL && parameterScope == NULL) {
    fprintf(stderr,
            "REX_AST_INVARIANT[preprocessing-semantic-scope]: node=%p "
            "is not a typed semantic-scope boundary\n",
            static_cast<void *>(node));
    ROSE_ABORT();
  }

  std::vector<Sg_File_Info *> positions;
  auto addPosition = [&positions](Sg_File_Info *position) {
    if (position != NULL && std::find(positions.begin(), positions.end(),
                                      position) == positions.end()) {
      positions.push_back(position);
    }
  };
  addPosition(node->get_file_info());
  addPosition(node->get_startOfConstruct());
  addPosition(node->get_endOfConstruct());
  auto exactSemanticPosition = [node](Sg_File_Info *position) {
    return position != NULL && position->get_parent() == node &&
           !position->isShared() && position->isCompilerGenerated() &&
           position->isFrontendSpecific() && !position->isTransformation() &&
           !position->isSourcePositionUnavailableInFrontend() &&
           position->isOutputInCodeGeneration() &&
           position->get_file_id() ==
               Sg_File_Info::COMPILER_GENERATED_FILE_ID &&
           position->get_physical_file_id() ==
               Sg_File_Info::COMPILER_GENERATED_FILE_ID;
  };
  AttachedPreprocessingInfoType *records = node->getAttachedPreprocessingInfo();
  if (positions.empty() ||
      !std::all_of(positions.begin(), positions.end(), exactSemanticPosition) ||
      (records != NULL && !records->empty())) {
    fprintf(stderr,
            "REX_AST_INVARIANT[preprocessing-semantic-scope]: node=%p "
            "type=%s is not exact preprocessing-free semantic "
            "infrastructure\n",
            static_cast<void *>(node), node->class_name().c_str());
    ROSE_ABORT();
  }

  if (scope != NULL) {
    if (SgDeclarationScopeList *owner =
            isSgDeclarationScopeList(scope->get_parent())) {
      if (owner->get_parent() == NULL ||
          std::count(owner->get_scopes().begin(), owner->get_scopes().end(),
                     scope) != 1) {
        fprintf(stderr,
                "REX_AST_INVARIANT[preprocessing-semantic-scope]: scope=%p "
                "has no exact declaration-scope-list owner\n",
                static_cast<void *>(scope));
        ROSE_ABORT();
      }
      return;
    }
    SgDeclarationStatement *owner =
        isSgDeclarationStatement(scope->get_parent());
    SgFunctionDeclaration *functionOwner = isSgFunctionDeclaration(owner);
    if (owner == NULL ||
        (owner->get_declarationScope() != scope &&
         owner->get_nonreal_decl_scope() != scope &&
         owner->get_source_declarator_scope() != scope &&
         (functionOwner == NULL ||
          functionOwner->get_function_declarator_scope() != scope))) {
      fprintf(stderr,
              "REX_AST_INVARIANT[preprocessing-semantic-scope]: scope=%p "
              "has no exact declaration owner\n",
              static_cast<void *>(scope));
      ROSE_ABORT();
    }
    return;
  }

  if (parameterScope != NULL) {
    SgFunctionDeclaration *functionOwner =
        isSgFunctionDeclaration(parameterScope->get_parent());
    SgRequiresExpr *requiresOwner =
        isSgRequiresExpr(parameterScope->get_parent());
    const bool exactFunctionRole =
        functionOwner != NULL &&
        functionOwner->get_functionParameterScope() == parameterScope &&
        parameterScope->get_scope() == functionOwner->get_scope();
    const bool exactRequiresRole =
        requiresOwner != NULL &&
        requiresOwner->get_local_parameter_scope() == parameterScope &&
        parameterScope->get_scope() != NULL;
    if (exactFunctionRole == exactRequiresRole ||
        parameterScope->get_construction_physical_output_owner() != NULL ||
        parameterScope->get_construction_semantic_scope() != NULL) {
      fprintf(stderr,
              "REX_AST_INVARIANT[preprocessing-semantic-scope]: parameter-"
              "scope=%p parent=%p function=%p requires=%p semantic=%p "
              "construction-owner=%p construction-scope=%p has no exact "
              "completed structural role\n",
              static_cast<void *>(parameterScope),
              static_cast<void *>(parameterScope->get_parent()),
              static_cast<void *>(functionOwner),
              static_cast<void *>(requiresOwner),
              static_cast<void *>(parameterScope->get_scope()),
              static_cast<void *>(
                  parameterScope->get_construction_physical_output_owner()),
              static_cast<void *>(
                  parameterScope->get_construction_semantic_scope()));
      ROSE_ABORT();
    }
    return;
  }

  SgScopeStatement *owner = isSgScopeStatement(container->get_parent());
  if (owner == NULL || owner->get_auxiliary_declaration_scopes() != container) {
    fprintf(stderr,
            "REX_AST_INVARIANT[preprocessing-semantic-scope]: container=%p "
            "has no exact lexical scope owner\n",
            static_cast<void *>(container));
    ROSE_ABORT();
  }
  for (SgDeclarationScope *ownedScope : container->get_scopes()) {
    if (ownedScope == NULL || ownedScope->get_parent() != container ||
        std::count(container->get_scopes().begin(),
                   container->get_scopes().end(), ownedScope) != 1) {
      fprintf(stderr,
              "REX_AST_INVARIANT[preprocessing-semantic-scope]: "
              "container=%p contains a null, duplicate, or misowned scope\n",
              static_cast<void *>(container));
      ROSE_ABORT();
    }
  }
}

bool isStructuralMemberInitializerPreprocessingBoundary(SgNode *node) {
  SgCtorInitializerList *initializers = isSgCtorInitializerList(node);
  if (initializers == NULL) {
    return false;
  }

  SgMemberFunctionDeclaration *member =
      isSgMemberFunctionDeclaration(initializers->get_parent());
  const bool hasTypedRole =
      member != NULL && member->get_CtorInitializerList() == initializers &&
      !member->get_specialFunctionModifier().isConstructor() &&
      initializers->get_ctors().empty();
  std::vector<Sg_File_Info *> positions;
  auto addPosition = [&positions](Sg_File_Info *position) {
    if (position != NULL && std::find(positions.begin(), positions.end(),
                                      position) == positions.end()) {
      positions.push_back(position);
    }
  };
  addPosition(initializers->get_file_info());
  addPosition(initializers->get_startOfConstruct());
  addPosition(initializers->get_endOfConstruct());
  const bool hasSemanticProvenance =
      std::any_of(positions.begin(), positions.end(), [](Sg_File_Info *info) {
        return info != NULL && info->isCompilerGenerated() &&
               info->isFrontendSpecific() && !info->isTransformation() &&
               info->get_file_id() == Sg_File_Info::COMPILER_GENERATED_FILE_ID;
      });
  if (!hasTypedRole && !hasSemanticProvenance) {
    return false;
  }
  if (!hasTypedRole) {
    fprintf(stderr,
            "REX_AST_INVARIANT[preprocessing-member-initializer]: list=%p "
            "has semantic provenance without one exact empty "
            "non-constructor owner\n",
            static_cast<void *>(initializers));
    ROSE_ABORT();
  }

  auto exactSemanticPosition = [initializers](Sg_File_Info *position) {
    return position != NULL && position->get_parent() == initializers &&
           !position->isShared() && position->isCompilerGenerated() &&
           position->isFrontendSpecific() && !position->isTransformation() &&
           !position->isSourcePositionUnavailableInFrontend() &&
           position->isOutputInCodeGeneration() &&
           position->get_file_id() ==
               Sg_File_Info::COMPILER_GENERATED_FILE_ID &&
           position->get_physical_file_id() ==
               Sg_File_Info::COMPILER_GENERATED_FILE_ID;
  };
  AttachedPreprocessingInfoType *records =
      initializers->getAttachedPreprocessingInfo();
  if (positions.empty() ||
      !std::all_of(positions.begin(), positions.end(), exactSemanticPosition) ||
      (records != NULL && !records->empty())) {
    fprintf(stderr,
            "REX_AST_INVARIANT[preprocessing-member-initializer]: list=%p "
            "is not exact preprocessing-free semantic infrastructure\n",
            static_cast<void *>(initializers));
    ROSE_ABORT();
  }
  return true;
}

bool isSemanticTemplateInstantiationDirectivePayload(SgNode *node) {
  SgDeclarationStatement *declaration = isSgTemplateInstantiationDecl(node);
  if (declaration == NULL) {
    declaration = isSgTemplateInstantiationFunctionDecl(node);
  }
  if (declaration == NULL) {
    declaration = isSgTemplateInstantiationMemberFunctionDecl(node);
  }
  if (declaration == NULL) {
    return false;
  }

  SgTemplateInstantiationDirectiveStatement *directive =
      isSgTemplateInstantiationDirectiveStatement(declaration->get_parent());
  if (directive == NULL) {
    return false;
  }

  const SgNodePtrList successors = directive->get_traversalSuccessorContainer();
  AttachedPreprocessingInfoType *records =
      declaration->getAttachedPreprocessingInfo();
  if (directive->get_declaration() != declaration ||
      declaration->get_parent() != directive || successors.size() != 1 ||
      successors.front() != declaration ||
      !SageInterface::hasSemanticOnlyFrontendSourcePosition(declaration) ||
      (records != NULL && !records->empty())) {
    fprintf(stderr,
            "REX_AST_INVARIANT[preprocessing-template-instantiation-payload]: "
            "directive=%p payload=%p/%s does not have one exact "
            "preprocessing-free semantic owner edge\n",
            static_cast<void *>(directive), static_cast<void *>(declaration),
            declaration->class_name().c_str());
    ROSE_ABORT();
  }
  return true;
}
} // namespace

// Member function: evaluateInheritedAttribute
AttachPreprocessingInfoTreeTraversalInheritedAttrribute
AttachPreprocessingInfoTreeTrav::evaluateInheritedAttribute(
    SgNode *n, AttachPreprocessingInfoTreeTraversalInheritedAttrribute
                   inheritedAttribute) {
  ROSE_ASSERT(sourceFile != NULL);

  ROSE_ASSERT(n != NULL);
  if (inheritedAttribute.isPartOfAuxiliaryDeclaration ||
      inheritedAttribute.isPartOfSemanticDeclarationScope) {
    return inheritedAttribute;
  }
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
    inheritedAttribute.isPartOfTemplateDeclaration = true;
  } else {
    if (templateInstantiationDeclaration == NULL)
      templateInstantiationDeclaration =
          isSgTemplateInstantiationMemberFunctionDecl(n);
    if (templateInstantiationDeclaration == NULL)
      templateInstantiationDeclaration = isSgTemplateInstantiationDecl(n);
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

  if ((inheritedAttribute.isPartOfTemplateDeclaration == true &&
       templateDeclaration == NULL) ||
      (inheritedAttribute.isPartOfTemplateInstantiationDeclaration == true &&
       templateInstantiationDeclaration == NULL)) {
    return inheritedAttribute;
  }

  if (SgLocatedNode *semanticScope = isSgLocatedNode(isSgDeclarationScope(n))) {
    validateSemanticDeclarationScopePreprocessingBoundary(semanticScope);
    inheritedAttribute.isPartOfSemanticDeclarationScope = true;
    return inheritedAttribute;
  }
  if (SgLocatedNode *semanticScopeList =
          isSgLocatedNode(isSgDeclarationScopeList(n))) {
    validateSemanticDeclarationScopePreprocessingBoundary(semanticScopeList);
    inheritedAttribute.isPartOfSemanticDeclarationScope = true;
    return inheritedAttribute;
  }
  if (SgLocatedNode *parameterScope =
          isSgLocatedNode(isSgFunctionParameterScope(n))) {
    validateSemanticDeclarationScopePreprocessingBoundary(parameterScope);
    inheritedAttribute.isPartOfSemanticDeclarationScope = true;
    return inheritedAttribute;
  }
  if (isStructuralMemberInitializerPreprocessingBoundary(n)) {
    return inheritedAttribute;
  }
  // An explicit-instantiation directive is the only lexical surface.  Its
  // wrapped declaration is typed semantic payload consumed by the directive
  // unparser; it cannot independently own comments or directives from the
  // source file.  Validate that producer contract before excluding the complete
  // payload subtree from preprocessing attachment.
  if (isSemanticTemplateInstantiationDirectivePayload(n)) {
    inheritedAttribute.isPartOfTemplateInstantiationDeclaration = true;
    return inheritedAttribute;
  }

  // Auxiliary declarations are typed semantic targets for a separate lexical
  // source surface.  Neither the declaration nor its descendants are legal
  // anchors for source comments or directives.  Validate that ownership
  // transaction before excluding the semantic subtree from attachment.
  SgAuxiliaryDeclarationList *auxiliaryOwner = NULL;
  SgNode *auxiliaryChild = NULL;
  std::set<SgNode *> ownershipPath;
  for (SgNode *current = n; current != NULL; current = current->get_parent()) {
    if (!ownershipPath.insert(current).second) {
      fprintf(stderr,
              "REX_AST_INVARIANT[preprocessing-auxiliary-owner]: node=%p "
              "has a cycle in its ownership path\n",
              static_cast<void *>(n));
      ROSE_ABORT();
    }
    if (SgAuxiliaryDeclarationList *candidate =
            isSgAuxiliaryDeclarationList(current)) {
      auxiliaryOwner = candidate;
      break;
    }
    auxiliaryChild = current;
  }
  if (auxiliaryOwner != NULL) {
    SgScopeStatement *semanticScope =
        isSgScopeStatement(auxiliaryOwner->get_parent());
    const SgDeclarationStatementPtrList &declarations =
        auxiliaryOwner->get_declarations();
    if (semanticScope == NULL) {
      fprintf(stderr,
              "REX_AST_INVARIANT[preprocessing-auxiliary-owner]: node=%p "
              "auxiliary=%p has no exact semantic scope\n",
              static_cast<void *>(n), static_cast<void *>(auxiliaryOwner));
      ROSE_ABORT();
    }

    // The traversal visits the typed container itself before any declaration
    // child.  Validate every direct edge at that point; there is deliberately
    // no declaration-shaped child to recover from the parent walk.
    if (n == auxiliaryOwner) {
      for (SgDeclarationStatement *declaration : declarations) {
        if (declaration == NULL ||
            declaration->get_parent() != auxiliaryOwner ||
            declaration->get_scope() != semanticScope ||
            std::count(declarations.begin(), declarations.end(), declaration) !=
                1) {
          fprintf(stderr,
                  "REX_AST_INVARIANT[preprocessing-auxiliary-owner]: "
                  "auxiliary=%p declaration=%p scope=%p has no exact direct "
                  "semantic ownership\n",
                  static_cast<void *>(auxiliaryOwner),
                  static_cast<void *>(declaration),
                  static_cast<void *>(semanticScope));
          ROSE_ABORT();
        }
      }
      inheritedAttribute.isPartOfAuxiliaryDeclaration = true;
      return inheritedAttribute;
    }

    SgDeclarationStatement *semanticDeclaration =
        isSgDeclarationStatement(auxiliaryChild);
    if (semanticDeclaration == NULL ||
        semanticDeclaration->get_parent() != auxiliaryOwner ||
        semanticDeclaration->get_scope() != semanticScope ||
        std::count(declarations.begin(), declarations.end(),
                   semanticDeclaration) != 1) {
      fprintf(stderr,
              "REX_AST_INVARIANT[preprocessing-auxiliary-owner]: node=%p "
              "auxiliary=%p declaration=%p scope=%p has no exact semantic "
              "ownership\n",
              static_cast<void *>(n), static_cast<void *>(auxiliaryOwner),
              static_cast<void *>(semanticDeclaration),
              static_cast<void *>(semanticScope));
      ROSE_ABORT();
    }
    inheritedAttribute.isPartOfAuxiliaryDeclaration = true;
    return inheritedAttribute;
  }

  SgFile *currentFilePtr = isSgFile(n);
  if (currentFilePtr != NULL) {
    Sg_File_Info *currentFileInfo = currentFilePtr->get_file_info();
    Sg_File_Info *sourceFileInfo = sourceFile->get_file_info();
    if (currentFileInfo == NULL) {
      fprintf(stderr,
              "REX_AST_INVARIANT[preprocessing-file-position]: file=%p has "
              "no typed file info\n",
              static_cast<void *>(currentFilePtr));
      ROSE_ABORT();
    }

    int currentFileNameId =
        (currentFilePtr->get_requires_C_preprocessor() == true)
            ? Sg_File_Info::getIDFromFilename(
                  currentFilePtr->generate_C_preprocessor_intermediate_filename(
                      sourceFileInfo->get_filename()))
            : currentFileInfo->get_physical_file_id(source_file_id);

    if (currentFileNameId < 0) {
      fprintf(stderr,
              "REX_AST_INVARIANT[preprocessing-file-position]: file=%p "
              "has invalid physical file ID=%d\n",
              static_cast<void *>(currentFilePtr), currentFileNameId);
      ROSE_ABORT();
    }

    if (sourceFile->get_Fortran_only()) {
      target_source_file_id = currentFileNameId;
    }

    if (currentFileNameId != target_source_file_id) {
      return inheritedAttribute;
    }
  }

  SgStatement *statement = isSgStatement(n);
  SgInitializedName *i_name = isSgInitializedName(n);
  SgAggregateInitializer *a_initor = isSgAggregateInitializer(n);

  // Pei-Hung (9/17/2020): comment and preprocess information will not be
  // attached to SgInitializedName that is part of the SgFunctionParameterList

  if (statement != NULL ||
      (i_name != NULL &&
       inheritedAttribute.isPartOfFunctionParameterList == false) ||
      a_initor != NULL) {
    SgLocatedNode *currentLocNodePtr = isSgLocatedNode(n);
    ROSE_ASSERT(currentLocNodePtr != NULL);

    // Some helper IR nodes participate in traversal but are not valid
    // preprocessing anchors because they are not unparsed as standalone syntax.
    // Normalize them to the enclosing statement so directives that precede a
    // declaration stay attached to that declaration instead of migrating into
    // internal nodes like SgFunctionParameterList.
    if (isSgForInitStatement(currentLocNodePtr) != NULL ||
        isSgCatchStatementSeq(currentLocNodePtr) != NULL ||
        isSgFunctionParameterList(currentLocNodePtr) != NULL ||
        isSgCtorInitializerList(currentLocNodePtr) != NULL) {
      SgStatement *currentStatement = isSgStatement(currentLocNodePtr);
      ROSE_ASSERT(currentStatement != NULL);
      SgStatement *parentStatement =
          isSgStatement(currentStatement->get_parent());
      ROSE_ASSERT((parentStatement != NULL) ||
                  (isSgGlobal(currentStatement) != NULL));
      if (parentStatement != NULL) {
        currentLocNodePtr = parentStatement;
      }
    }

    // Attach the comments only to nodes from the same file
    Sg_File_Info *currentFileInfo = currentLocNodePtr->get_file_info();
    ROSE_ASSERT(currentFileInfo != NULL);

    if (!sourceFile->get_Fortran_only()) {
      ROSE_ASSERT(!sourceFile->get_requires_C_preprocessor());
    }

    int currentFileNameId =
        (sourceFile->get_requires_C_preprocessor() == true)
            ? Sg_File_Info::getIDFromFilename(
                  sourceFile->generate_C_preprocessor_intermediate_filename(
                      sourceFile->get_file_info()->get_filename()))
            : currentFileInfo->get_physical_file_id(source_file_id);

    ROSEAttributesList *currentListOfAttributes = this->currentListOfAttributes;
    ROSE_ASSERT(currentListOfAttributes != NULL);

    const int currentLocNode_physical_file_id =
        currentFileInfo->get_physical_file_id();

    // Match the preprocessed file when CPP ran; otherwise match the source.
    if (((sourceFile->get_requires_C_preprocessor() == true)
             ? currentFileNameId
             : source_file_id) == currentLocNode_physical_file_id) {
      // Shared nodes need the physical line for this source file.
      const int line = currentFileInfo->get_physical_line(source_file_id);

      bool reset_start_index = false;
      iterateOverListAndInsertPreviouslyUninsertedElementsAppearingBeforeLineNumber(
          currentLocNodePtr, line, PreprocessingInfo::before, reset_start_index,
          currentListOfAttributes);
      // Initialized names are not unparsed directly, so they cannot anchor
      // attached preprocessing information.
      if (isSgInitializedName(currentLocNodePtr) == NULL) {
        previousLocatedNode = currentLocNodePtr;
      }
    }
  } // end if statement or init name

  return inheritedAttribute;
}

// Member function: evaluateSynthesizedAttribute
AttachPreprocessingInfoTreeTraversalSynthesizedAttribute
AttachPreprocessingInfoTreeTrav::evaluateSynthesizedAttribute(
    SgNode *n,
    AttachPreprocessingInfoTreeTraversalInheritedAttrribute inheritedAttribute,
    SubTreeSynthesizedAttributes /*synthiziedAttributeList*/) {
  AttachPreprocessingInfoTreeTraversalSynthesizedAttribute
      returnSynthesizeAttribute;

  ROSE_ASSERT(n != NULL);
  if (inheritedAttribute.isPartOfAuxiliaryDeclaration ||
      inheritedAttribute.isPartOfSemanticDeclarationScope) {
    return returnSynthesizeAttribute;
  }
  if (isStructuralMemberInitializerPreprocessingBoundary(n)) {
    return returnSynthesizeAttribute;
  }
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
  // Attach directives at the declaration boundary, not within its template.
  if ((inheritedAttribute.isPartOfTemplateDeclaration == true &&
       templateDeclaration == NULL) ||
      (inheritedAttribute.isPartOfTemplateInstantiationDeclaration == true &&
       templateInstantiationDeclaration == NULL)) {
    return returnSynthesizeAttribute;
  }

  SgClassDeclaration *classDeclaration = isSgClassDeclaration(n);
  ROSE_ASSERT(classDeclaration == NULL ||
              classDeclaration->get_endOfConstruct() != NULL);

  SgFile *fileNode = isSgFile(n);

  SgLocatedNode *locatedNode = isSgLocatedNode(n);

  if ((locatedNode != NULL) || (fileNode != NULL)) {
    int currentFileNameId = -9;
    if (locatedNode != NULL) {
      ROSE_ASSERT(locatedNode->get_file_info() != NULL);
      // Preprocessing information is attached by exact physical ownership.
      // Semantic-only compiler-generated and transformation nodes have no
      // physical file in which a comment or directive could occur.
      if (locatedNode->get_file_info()->get_physical_file_id() < 0) {
        return returnSynthesizeAttribute;
      }
      currentFileNameId =
          locatedNode->get_file_info()->get_physical_file_id(source_file_id);
    } else {
      Sg_File_Info *currentFileInfo = sourceFile->get_file_info();
      ROSE_ASSERT(currentFileInfo != NULL);
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

    bool isOutputOrTransformation = false;
    int lineOfClosingBrace = 0;
    if (locatedNode != NULL) {
      ROSE_ASSERT(locatedNode->get_file_info());
      isOutputOrTransformation =
          locatedNode->get_file_info()->isOutputInCodeGeneration() ||
          locatedNode->get_file_info()->isTransformation();

      if (locatedNode->get_file_info()->hasPositionInSource() == true) {
        fileIdForOriginOfCurrentLocatedNode =
            locatedNode->get_file_info()->get_physical_file_id(source_file_id);
      }

      if (locatedNode->get_endOfConstruct() != NULL) {
        lineOfClosingBrace =
            locatedNode->get_endOfConstruct()->get_physical_line(
                source_file_id);
      }
    } else {
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
    // directives.
    if ((isOutputOrTransformation == true) ||
        (currentFileNameId == fileIdForOriginOfCurrentLocatedNode)) {

      ROSEAttributesList *currentListOfAttributes =
          this->currentListOfAttributes;
      ROSE_ASSERT(currentListOfAttributes != NULL);

      SgLocatedNode *previousLocNodePtr = previousLocatedNode;
      if (previousLocNodePtr == NULL) {
        previousLocNodePtr = locatedNode;
      }

      if (isSgSourceFile(n) == NULL) {
        ROSE_ASSERT(previousLocNodePtr != NULL);
      }

      switch (n->variantT()) {
      case V_SgSourceFile: {
        // DQ (4/28/2021): Consider this as a work around.
        if (previousLocatedNode == NULL) {
          return returnSynthesizeAttribute;
        }
        ROSE_ASSERT(previousLocatedNode != NULL);
        SgLocatedNode *targetNode = previousLocatedNode;
        ROSE_ASSERT(targetNode != NULL);
        if (isSgBasicBlock(targetNode) != NULL) {
          while ((targetNode != NULL) &&
                 (isSgGlobal(targetNode->get_parent()) == NULL)) {
            targetNode =
                dynamic_cast<SgLocatedNode *>(targetNode->get_parent());
          }
        }

        ROSE_ASSERT(targetNode != NULL);
        Sg_File_Info *nodeFileInfo = targetNode->get_file_info();
        ROSE_ASSERT(nodeFileInfo != NULL);
        // A generated program header has no source position for comments.
        if (nodeFileInfo->get_physical_file_id(source_file_id) < 0) {
          break;
        }
        // Iterate over the list of comments and directives and add them to the
        // AST negara1 (07/28/2011): Changed to false, since we might need to
        // re-visit some header files.
        bool reset_start_index = false;
        iterateOverListAndInsertPreviouslyUninsertedElementsAppearingBeforeLineNumber(
            targetNode, lineOfClosingBrace, PreprocessingInfo::after,
            reset_start_index, currentListOfAttributes);

        break;
      }

        // This case helps place the comment or directive relative to the
        // closing brace of a SgBasicBlock.
      case V_SgBasicBlock: {
        ROSE_ASSERT(locatedNode != NULL);
        ROSE_ASSERT(locatedNode->get_endOfConstruct() != NULL);

        SgBasicBlock *basicBlock = isSgBasicBlock(n);
        ROSE_ASSERT(basicBlock != NULL);

        SgStatementExpression *statementExpression = NULL;
        // DQ (2/15/2021): I would like to insert comments and CPP directives
        // after the last statement, where it exists, instead of inside the
        // block. Only when there are no statements in the SgBasciBlock should
        // we add the comments and CPP directives inside the SgBasicBlock.
        SgStatement *previousStatement = isSgStatement(previousLocatedNode);

        if (previousStatement != NULL) {
          SgLocatedNode *parentLocatedNode =
              isSgLocatedNode(previousStatement->get_parent());
          statementExpression = isSgStatementExpression(parentLocatedNode);
          SgStatement *enclosingStatement = NULL;
          if (statementExpression != NULL) {
            bool includingSelf = false;
            enclosingStatement = SageInterface::getEnclosingNode<SgStatement>(
                statementExpression, includingSelf);
            ROSE_ASSERT(enclosingStatement != NULL);

            previousStatement = enclosingStatement;
          } else {
            SgFunctionDeclaration *function =
                isSgFunctionDeclaration(previousStatement);
            SgClassDefinition *closure =
                function != nullptr ? isSgClassDefinition(function->get_scope())
                                    : nullptr;
            SgLambdaExp *lambdaExpression =
                closure != nullptr
                    ? closure->get_declaration()->get_lambda_expression()
                    : nullptr;
            if (lambdaExpression != NULL) {
              bool includingSelf = false;
              enclosingStatement = SageInterface::getEnclosingNode<SgStatement>(
                  lambdaExpression, includingSelf);
              ROSE_ASSERT(enclosingStatement != NULL);
              previousStatement = enclosingStatement;
            }
          }
        }

        if (previousStatement != NULL) {
          bool reset_start_index = false;
          const auto position = previousStatement == basicBlock
                                    ? PreprocessingInfo::inside
                                    : PreprocessingInfo::after;
          iterateOverListAndInsertPreviouslyUninsertedElementsAppearingBeforeLineNumber(
              previousStatement, lineOfClosingBrace, position,
              reset_start_index, currentListOfAttributes);
        }
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
        ROSE_ASSERT(target != NULL);

        bool reset_start_index = false;
        iterateOverListAndInsertPreviouslyUninsertedElementsAppearingBeforeLineNumber(
            target, lineOfClosingBrace, PreprocessingInfo::inside,
            reset_start_index, currentListOfAttributes);

        previousLocatedNode = target;
        break;
      }

      case V_SgTemplateClassDeclaration:

      case V_SgClassDeclaration: {
        ROSE_ASSERT(locatedNode != NULL);
        ROSE_ASSERT(locatedNode->get_endOfConstruct() != NULL);

        SgClassDeclaration *classDeclaration =
            dynamic_cast<SgClassDeclaration *>(n);
        ROSE_ASSERT(classDeclaration != NULL);
        ROSE_ASSERT(previousLocNodePtr != NULL);

        handleBracedScopes(previousLocNodePtr, classDeclaration,
                           lineOfClosingBrace, currentListOfAttributes);
        previousLocatedNode = classDeclaration;
        break;
      }

      case V_SgTypedefDeclaration: {
        ROSE_ASSERT(locatedNode != NULL);
        ROSE_ASSERT(locatedNode->get_endOfConstruct() != NULL);

        SgTypedefDeclaration *typedefDeclaration = isSgTypedefDeclaration(n);
        ROSE_ASSERT(typedefDeclaration != NULL);
        ROSE_ASSERT(previousLocNodePtr != NULL);

        bool reset_start_index = false;
        iterateOverListAndInsertPreviouslyUninsertedElementsAppearingBeforeLineNumber(
            previousLocNodePtr, lineOfClosingBrace, PreprocessingInfo::after,
            reset_start_index, currentListOfAttributes);

        previousLocatedNode = typedefDeclaration;
        break;
      }

      case V_SgTemplateVariableDeclaration:

      case V_SgVariableDeclaration: {
        ROSE_ASSERT(locatedNode != NULL);
        ROSE_ASSERT(locatedNode->get_endOfConstruct() != NULL);

        SgVariableDeclaration *variableDeclaration = isSgVariableDeclaration(n);
        ROSE_ASSERT(variableDeclaration != NULL);
        ROSE_ASSERT(previousLocNodePtr != NULL);

        bool reset_start_index = false;
        iterateOverListAndInsertPreviouslyUninsertedElementsAppearingBeforeLineNumber(
            previousLocNodePtr, lineOfClosingBrace, PreprocessingInfo::after,
            reset_start_index, currentListOfAttributes);

        previousLocatedNode = variableDeclaration;
        break;
      }

      case V_SgInitializedName: {
        ROSE_ASSERT(locatedNode != NULL);
        ROSE_ASSERT(locatedNode->get_endOfConstruct() != NULL);

        SgInitializedName *initializedName = isSgInitializedName(n);
        ROSE_ASSERT(initializedName != NULL);
        ROSE_ASSERT(previousLocNodePtr != NULL);
        if (!sourceFile->get_Fortran_only()) {
          bool reset_start_index = false;
          iterateOverListAndInsertPreviouslyUninsertedElementsAppearingBeforeLineNumber(
              previousLocNodePtr, lineOfClosingBrace, PreprocessingInfo::after,
              reset_start_index, currentListOfAttributes);

          previousLocatedNode = initializedName;
        }
        break;
      }

      case V_SgClassDefinition: {
        ROSE_ASSERT(locatedNode != NULL);
        ROSE_ASSERT(locatedNode->get_endOfConstruct() != NULL);

        ROSE_ASSERT(locatedNode != NULL);
        SgClassDefinition *classDefinition =
            dynamic_cast<SgClassDefinition *>(n);
        ROSE_ASSERT(classDefinition != NULL);
        handleBracedScopes(previousLocNodePtr, classDefinition,
                           lineOfClosingBrace, currentListOfAttributes);
        break;
      }

      case V_SgEnumDeclaration: {
        ROSE_ASSERT(locatedNode != NULL);
        ROSE_ASSERT(locatedNode->get_endOfConstruct() != NULL);

        SgEnumDeclaration *enumDeclaration =
            dynamic_cast<SgEnumDeclaration *>(n);
        ROSE_ASSERT(enumDeclaration != NULL);
        ROSE_ASSERT(locatedNode != NULL);

        bool reset_start_index = false;
        iterateOverListAndInsertPreviouslyUninsertedElementsAppearingBeforeLineNumber(
            locatedNode, lineOfClosingBrace, PreprocessingInfo::inside,
            reset_start_index, currentListOfAttributes);

        previousLocatedNode = enumDeclaration;
        break;
      }

      case V_SgNamespaceDeclarationStatement: {
        ROSE_ASSERT(locatedNode != NULL);
        ROSE_ASSERT(locatedNode->get_endOfConstruct() != NULL);

        SgNamespaceDeclarationStatement *namespaceDeclaration =
            dynamic_cast<SgNamespaceDeclarationStatement *>(n);
        ROSE_ASSERT(namespaceDeclaration != NULL);

        ROSE_ASSERT(previousLocNodePtr != NULL);
        bool reset_start_index = false;
        iterateOverListAndInsertPreviouslyUninsertedElementsAppearingBeforeLineNumber(
            previousLocNodePtr, lineOfClosingBrace, PreprocessingInfo::after,
            reset_start_index, currentListOfAttributes);

        previousLocatedNode = namespaceDeclaration;
        break;
      }

      case V_SgNamespaceDefinitionStatement: {
        ROSE_ASSERT(locatedNode != NULL);
        ROSE_ASSERT(locatedNode->get_endOfConstruct() != NULL);

        SgNamespaceDefinitionStatement *namespaceDefinition =
            dynamic_cast<SgNamespaceDefinitionStatement *>(n);
        ROSE_ASSERT(namespaceDefinition != NULL);

        ROSE_ASSERT(previousLocNodePtr != NULL);

        bool reset_start_index = false;
        iterateOverListAndInsertPreviouslyUninsertedElementsAppearingBeforeLineNumber(
            locatedNode, lineOfClosingBrace, PreprocessingInfo::inside,
            reset_start_index, currentListOfAttributes);

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
        bool reset_start_index = false;
        iterateOverListAndInsertPreviouslyUninsertedElementsAppearingBeforeLineNumber(
            locatedNode, lineOfClosingBrace, PreprocessingInfo::inside,
            reset_start_index, currentListOfAttributes);

        previousLocatedNode = locatedNode;
        // DQ (3/11/2012): Added break statement to prevent fall through, I
        // think this fixes a bug.
        break;
      }

      case V_SgTemplateClassDefinition:
      case V_SgTemplateFunctionDefinition:
      case V_SgFunctionDefinition:
      case V_SgTemplateFunctionDeclaration:
      case V_SgTemplateMemberFunctionDeclaration:
      case V_SgTemplateInstantiationDirectiveStatement:
      case V_SgFunctionDeclaration:
      case V_SgMemberFunctionDeclaration:
      case V_SgTemplateInstantiationFunctionDecl: {
        ROSE_ASSERT(locatedNode != NULL);
        ROSE_ASSERT(locatedNode->get_endOfConstruct() != NULL);

        ROSE_ASSERT(locatedNode != NULL);
        bool reset_start_index = false;
        iterateOverListAndInsertPreviouslyUninsertedElementsAppearingBeforeLineNumber(
            locatedNode, lineOfClosingBrace, PreprocessingInfo::inside,
            reset_start_index, currentListOfAttributes);

        previousLocatedNode = locatedNode;
        break;
      }

        // Statements with nested blocks need to become the previous anchor
        // after their children are processed. This preserves directives that
        // follow an inner block but still belong to the surrounding statement,
        // such as a conditionally present block inside a case label:
        //   case 1:
        // #if X
        //   {
        // #endif
        //     stmt;
        // #if X
        //   }
        // #endif
        // Without this, trailing directives drift to the next sibling
        // statement instead of remaining attached to the case/if/loop/switch.
      case V_SgIfStmt: {
        previousLocatedNode = locatedNode;
        break;
      }

      case V_SgForStatement:
      case V_SgWhileStmt:
      case V_SgDoWhileStmt: {
        previousLocatedNode = locatedNode;
        break;
      }

      case V_SgSwitchStatement:
      case V_SgCaseOptionStmt:
      case V_SgDefaultOptionStmt: {
        previousLocatedNode = locatedNode;
        break;
      }

      case V_SgFortranDo: {
        previousLocatedNode = locatedNode;
        break;
      }

      default: {
        ROSE_ASSERT(n != NULL);
      }
      }

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

  if (previousLocatedNode == NULL) {
    previousLocatedNode = isSgLocatedNode(n);
  }

  return returnSynthesizeAttribute;
}
