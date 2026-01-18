#ifndef AST_POST_PROCESSING_H
#define AST_POST_PROCESSING_H

#define AST_POST_PROCESSING_VERBOSE_LEVEL DIAGNOSTICS_VERBOSE_LEVEL + 1

#include "AstFixup.h"

#include "fixupSymbolTables.h"
#include "processTemplateHandlingOptions.h"
#include "resetParentPointers.h"

// DQ (12/29/2011): This header file requires rose_config.h and we don't want
// all of ROSE to require this. #include "markCompilerGenerated.h"

#include "checkIsCompilerGeneratedFlag.h"
#include "checkIsFrontendSpecificFlag.h"
#include "checkIsModifiedFlag.h"
#include "fixupConstructorPreinitializationLists.h"
#include "fixupDeclarations.h"
#include "fixupDefiningAndNondefiningDeclarations.h"
#include "fixupNames.h"
#include "fixupNullPointers.h"
#include "fixupTemplateInstantiations.h"
#include "fixupTypes.h"
#include "initializeExplicitScopeData.h"
#include "markBackendCompilerSpecificFunctions.h"
#include "markForOutputInCodeGeneration.h"
#include "markLhsValues.h"
#include "markOverloadedTemplateInstantiations.h"
#include "markTemplateInstantiationsForOutput.h"
#include "markTemplateSpecializationsForOutput.h"
#include "markTransformationsForOutput.h"
#include "propagateHiddenListData.h"
#include "resetTemplateNames.h"

// DQ (11/24/2007): Fortran support to resolution of array references vs.
// function references.
#include "insertFortranContainsStatement.h"
#include "resolveFortranReferences.h"

// DQ (9/28/2008): This is for Fortran and eventually maybe C++ support.
#include "fixupUseAndUsingDeclarations.h"

// DQ (4/14/2010): This is the C++ specific support for symbol aliasing (to
// support better name qualification).
#include "fixupCxxSymbolTablesToSupportAliasingSymbols.h"

// DQ (6/24/2010): Fixup the SgTypedefSeq lists to support the AST merge
// mechanism.
#include "normalizeTypedefSequenceLists.h"

// DQ (9/14/2011): Added support to make the AST consistant with respect to
// constant folded values. We can either leave the constant folded values or
// replace the constant folded values with the original expression trees. The
// default will be to replace the constant folded values with the original
// expression trees.
#include "fixupConstantFoldedValues.h"

// DQ (5/1/2012): Added testing for marked transformations in the AST (should be
// none after legacy frontend/ROSE translation).
#include "detectTransformations.h"

// DQ (8/12/2012): Fixup the SgModifiers used to hold type reference in legacy
// frontend to types in instantiated templates that had not yet been seen). This
// post processing effects test2012_190.C and test2007_141.C.
#include "fixupTypeReferences.h"

// DQ (10/5/2012): Fixup known macros that might expand into a recursive mess in
// the unparsed code.
#include "fixupSelfReferentialMacros.h"

// DQ (4/24/2013): Detect the correct function declaration to declare the use of
// default arguments. This can only be a single function and it can't be any
// function (this is a moderately complex issue).
#include "fixupFunctionDefaultArguments.h"

// DQ (12/20/2012): Added support for testing the physical source position
// information.
#include "checkPhysicalSourcePosition.h"

// DQ (6/11/2013): This corrects where legacy frontend can set the scope of a
// friend declaration to be different from the defining declaration.
#include "fixupDeclarationScope.h"

// DQ (11/14/2015): This corrects inconstancies in the setting of flags in the
// Sg_File_Info objects.
#include "fixupFileInfoFlags.h"

// DQ (11/27/2016): Provide alternative typedef type that when unparsed will not
// contain private types).
#include "fixupTemplateArguments.h"

// DQ (5/18/2017): Added support to insert template instantiation prototypes.
#include "addPrototypesForTemplateInstantiations.h"

// DQ (2/25/2019): Adding support for marking shared defining declarations
// across multiple files.
#include "markSharedDeclarationsForOutputInCodeGeneration.h"

// DQ (5/7/2020): Added support to interleaving include directives into the
// code.

// DQ (8/25/2020): Added support to remove redundant include files used to
// initialize variable declarations.
#include "fixupInitializers.h"

/*! \brief Postprocessing that is not likely to be handled in the legacy
 * frontend/Sage III translation.
 */
void postProcessingSupport(SgNode *node);

/*! \brief This does all post-processing fixup and translation of the Sage III
   AST.

    This function was added to provide a more representative name, the post
   processing does not just include temporary fixes.  We might make if nore
   explicit what function are representative of temporary fixes later (at the
   moment I think there are none that are temporary). This does all
   post-processing fixup of the AST including: 1) initialization of the parent
   pointers 2) fixup of instantiated template names 3) fixup of ... 4) ...

 */
ROSE_DLL_API void AstPostProcessing(SgNode *node);

#endif
