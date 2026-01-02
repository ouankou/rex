
.. _file_src_frontend_SageIII_astPostProcessing_astPostProcessing.h:

File astPostProcessing.h
========================

|exhale_lsh| :ref:`Parent directory <dir_src_frontend_SageIII_astPostProcessing>` (``src/frontend/SageIII/astPostProcessing``)

.. |exhale_lsh| unicode:: U+021B0 .. UPWARDS ARROW WITH TIP LEFTWARDS


.. contents:: Contents
   :local:
   :backlinks: none

Definition (``src/frontend/SageIII/astPostProcessing/astPostProcessing.h``)
---------------------------------------------------------------------------






Includes
--------


- ``AstFixup.h`` (:ref:`file_src_frontend_SageIII_astFixup_AstFixup.h`)

- ``addPrototypesForTemplateInstantiations.h`` (:ref:`file_src_frontend_SageIII_astPostProcessing_addPrototypesForTemplateInstantiations.h`)

- ``checkIsCompilerGeneratedFlag.h`` (:ref:`file_src_frontend_SageIII_astPostProcessing_checkIsCompilerGeneratedFlag.h`)

- ``checkIsFrontendSpecificFlag.h`` (:ref:`file_src_frontend_SageIII_astPostProcessing_checkIsFrontendSpecificFlag.h`)

- ``checkIsModifiedFlag.h`` (:ref:`file_src_frontend_SageIII_astPostProcessing_checkIsModifiedFlag.h`)

- ``checkPhysicalSourcePosition.h`` (:ref:`file_src_frontend_SageIII_astPostProcessing_checkPhysicalSourcePosition.h`)

- ``detectTransformations.h`` (:ref:`file_src_frontend_SageIII_astPostProcessing_detectTransformations.h`)

- ``fixupConstantFoldedValues.h`` (:ref:`file_src_frontend_SageIII_astPostProcessing_fixupConstantFoldedValues.h`)

- ``fixupConstructorPreinitializationLists.h`` (:ref:`file_src_frontend_SageIII_astPostProcessing_fixupConstructorPreinitializationLists.h`)

- ``fixupCxxSymbolTablesToSupportAliasingSymbols.h`` (:ref:`file_src_frontend_SageIII_astPostProcessing_fixupCxxSymbolTablesToSupportAliasingSymbols.h`)

- ``fixupDeclarationScope.h`` (:ref:`file_src_frontend_SageIII_astPostProcessing_fixupDeclarationScope.h`)

- ``fixupDeclarations.h`` (:ref:`file_src_frontend_SageIII_astPostProcessing_fixupDeclarations.h`)

- ``fixupDefiningAndNondefiningDeclarations.h`` (:ref:`file_src_frontend_SageIII_astPostProcessing_fixupDefiningAndNondefiningDeclarations.h`)

- ``fixupFileInfoFlags.h`` (:ref:`file_src_frontend_SageIII_astPostProcessing_fixupFileInfoFlags.h`)

- ``fixupFunctionDefaultArguments.h`` (:ref:`file_src_frontend_SageIII_astPostProcessing_fixupFunctionDefaultArguments.h`)

- ``fixupInitializers.h`` (:ref:`file_src_frontend_SageIII_astPostProcessing_fixupInitializers.h`)

- ``fixupNames.h`` (:ref:`file_src_frontend_SageIII_astPostProcessing_fixupNames.h`)

- ``fixupNullPointers.h`` (:ref:`file_src_frontend_SageIII_astPostProcessing_fixupNullPointers.h`)

- ``fixupSelfReferentialMacros.h`` (:ref:`file_src_frontend_SageIII_astPostProcessing_fixupSelfReferentialMacros.h`)

- ``fixupSymbolTables.h`` (:ref:`file_src_frontend_SageIII_astPostProcessing_fixupSymbolTables.h`)

- ``fixupTemplateArguments.h`` (:ref:`file_src_frontend_SageIII_astPostProcessing_fixupTemplateArguments.h`)

- ``fixupTemplateInstantiations.h`` (:ref:`file_src_frontend_SageIII_astPostProcessing_fixupTemplateInstantiations.h`)

- ``fixupTypeReferences.h`` (:ref:`file_src_frontend_SageIII_astPostProcessing_fixupTypeReferences.h`)

- ``fixupTypes.h`` (:ref:`file_src_frontend_SageIII_astPostProcessing_fixupTypes.h`)

- ``fixupUseAndUsingDeclarations.h`` (:ref:`file_src_frontend_SageIII_astPostProcessing_fixupUseAndUsingDeclarations.h`)

- ``initializeExplicitScopeData.h`` (:ref:`file_src_frontend_SageIII_astPostProcessing_initializeExplicitScopeData.h`)

- ``insertFortranContainsStatement.h`` (:ref:`file_src_frontend_SageIII_astPostProcessing_insertFortranContainsStatement.h`)

- ``markBackendCompilerSpecificFunctions.h`` (:ref:`file_src_frontend_SageIII_astPostProcessing_markBackendCompilerSpecificFunctions.h`)

- ``markForOutputInCodeGeneration.h`` (:ref:`file_src_frontend_SageIII_astPostProcessing_markForOutputInCodeGeneration.h`)

- ``markLhsValues.h`` (:ref:`file_src_frontend_SageIII_astPostProcessing_markLhsValues.h`)

- ``markOverloadedTemplateInstantiations.h`` (:ref:`file_src_frontend_SageIII_astPostProcessing_markOverloadedTemplateInstantiations.h`)

- ``markSharedDeclarationsForOutputInCodeGeneration.h`` (:ref:`file_src_frontend_SageIII_astPostProcessing_markSharedDeclarationsForOutputInCodeGeneration.h`)

- ``markTemplateInstantiationsForOutput.h`` (:ref:`file_src_frontend_SageIII_astPostProcessing_markTemplateInstantiationsForOutput.h`)

- ``markTemplateSpecializationsForOutput.h`` (:ref:`file_src_frontend_SageIII_astPostProcessing_markTemplateSpecializationsForOutput.h`)

- ``markTransformationsForOutput.h`` (:ref:`file_src_frontend_SageIII_astPostProcessing_markTransformationsForOutput.h`)

- ``normalizeTypedefSequenceLists.h`` (:ref:`file_src_frontend_SageIII_astPostProcessing_normalizeTypedefSequenceLists.h`)

- ``processTemplateHandlingOptions.h`` (:ref:`file_src_frontend_SageIII_astPostProcessing_processTemplateHandlingOptions.h`)

- ``propagateHiddenListData.h`` (:ref:`file_src_frontend_SageIII_astPostProcessing_propagateHiddenListData.h`)

- ``resetParentPointers.h`` (:ref:`file_src_frontend_SageIII_astPostProcessing_resetParentPointers.h`)

- ``resetTemplateNames.h`` (:ref:`file_src_frontend_SageIII_astPostProcessing_resetTemplateNames.h`)

- ``resolveFortranReferences.h`` (:ref:`file_src_frontend_SageIII_astPostProcessing_resolveFortranReferences.h`)



Included By
-----------


- :ref:`file_src_frontend_SageIII_sage3.h`

- :ref:`file_src_frontend_SageIII_sage_support_sage_support.h`




Functions
---------


- :ref:`exhale_function_astPostProcessing_8h_1abfb503342d8968e2389a6a17bea69e6c`

- :ref:`exhale_function_astPostProcessing_8h_1a38150c0c439080c0a13519bbbb18a17b`


Defines
-------


- :ref:`exhale_define_astPostProcessing_8h_1a3d991ee07d314fc3ed38ce272020aef2`

