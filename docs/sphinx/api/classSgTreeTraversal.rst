.. _exhale_class_classSgTreeTraversal:

Template Class SgTreeTraversal
==============================

- Defined in :ref:`file_src_midend_astProcessing_AstProcessing.h`


Inheritance Relationships
-------------------------

Derived Types
*************

- ``public AstBottomUpProcessing< std::vector< SynthesizedAttributeType > * >`` (:ref:`exhale_class_classAstBottomUpProcessing`)
- ``public AstBottomUpProcessing< AstNodePtrSynAttr >`` (:ref:`exhale_class_classAstBottomUpProcessing`)
- ``public AstBottomUpProcessing< std::vector< SgInitializedName * > * >`` (:ref:`exhale_class_classAstBottomUpProcessing`)
- ``public AstBottomUpProcessing< ConstantUnFoldingSynthesizedAttribute >`` (:ref:`exhale_class_classAstBottomUpProcessing`)
- ``public AstBottomUpProcessing< DS >`` (:ref:`exhale_class_classAstBottomUpProcessing`)
- ``public AstBottomUpProcessing< TestAstPropertiesSA >`` (:ref:`exhale_class_classAstBottomUpProcessing`)
- ``public AstBottomUpProcessing< ChildUses >`` (:ref:`exhale_class_classAstBottomUpProcessing`)
- ``public AstBottomUpProcessing< VariableRenaming::VarRefSynthAttr >`` (:ref:`exhale_class_classAstBottomUpProcessing`)
- ``public AstTopDownBottomUpProcessing< _DummyAttribute, _DummyAttribute >`` (:ref:`exhale_class_classAstTopDownBottomUpProcessing`)
- ``public AstTopDownBottomUpProcessing< ArtificialFrontier_InheritedAttribute, ArtificialFrontier_SynthesizedAttribute >`` (:ref:`exhale_class_classAstTopDownBottomUpProcessing`)
- ``public AstTopDownBottomUpProcessing< std::vector< InheritedAttributeType > *, std::vector< SynthesizedAttributeType > * >`` (:ref:`exhale_class_classAstTopDownBottomUpProcessing`)
- ``public AstTopDownBottomUpProcessing< DOTInheritedAttribute, DOTSynthesizedAttribute >`` (:ref:`exhale_class_classAstTopDownBottomUpProcessing`)
- ``public AstTopDownBottomUpProcessing< AttachPreprocessingInfoTreeTraversalInheritedAttrribute, AttachPreprocessingInfoTreeTraversalSynthesizedAttribute >`` (:ref:`exhale_class_classAstTopDownBottomUpProcessing`)
- ``public AstTopDownBottomUpProcessing< BooleanQueryInheritedAttributeType, BooleanQuerySynthesizedAttributeType >`` (:ref:`exhale_class_classAstTopDownBottomUpProcessing`)
- ``public AstTopDownBottomUpProcessing< ConstantFoldingInheritedAttribute, ConstantFoldingSynthesizedAttribute >`` (:ref:`exhale_class_classAstTopDownBottomUpProcessing`)
- ``public AstTopDownBottomUpProcessing< BooleanSafeKeeper, BooleanSafeKeeper >`` (:ref:`exhale_class_classAstTopDownBottomUpProcessing`)
- ``public AstTopDownBottomUpProcessing< DetectMacroOrIncludeFileExpansionsInheritedAttribute, DetectMacroOrIncludeFileExpansionsSynthesizedAttribute >`` (:ref:`exhale_class_classAstTopDownBottomUpProcessing`)
- ``public AstTopDownBottomUpProcessing< DI, DS >`` (:ref:`exhale_class_classAstTopDownBottomUpProcessing`)
- ``public AstTopDownBottomUpProcessing< FixupInitializersUsingIncludeFilesInheritedAttribute, FixupInitializersUsingIncludeFilesSynthesizedAttribute >`` (:ref:`exhale_class_classAstTopDownBottomUpProcessing`)
- ``public AstTopDownBottomUpProcessing< FixupSourcePositionInformationInheritedAttribute, FixupSourcePositionInformationSynthesizedAttribute >`` (:ref:`exhale_class_classAstTopDownBottomUpProcessing`)
- ``public AstTopDownBottomUpProcessing< FrontierDetectionForTokenStreamMapping_InheritedAttribute, FrontierDetectionForTokenStreamMapping_SynthesizedAttribute >`` (:ref:`exhale_class_classAstTopDownBottomUpProcessing`)
- ``public AstTopDownBottomUpProcessing< FunctionCallInheritedAttribute, bool >`` (:ref:`exhale_class_classAstTopDownBottomUpProcessing`)
- ``public AstTopDownBottomUpProcessing< HiddenListInheritedAttribute, HiddenListSynthesizedAttribute >`` (:ref:`exhale_class_classAstTopDownBottomUpProcessing`)
- ``public AstTopDownBottomUpProcessing< InheritedAttribute, SynthesizedAttribute >`` (:ref:`exhale_class_classAstTopDownBottomUpProcessing`)
- ``public AstTopDownBottomUpProcessing< MarkTemplateInstantiationsForOutputSupportInheritedAttribute, MarkTemplateInstantiationsForOutputSupportSynthesizedAttribute >`` (:ref:`exhale_class_classAstTopDownBottomUpProcessing`)
- ``public AstTopDownBottomUpProcessing< NameQualificationInheritedAttribute, NameQualificationSynthesizedAttribute >`` (:ref:`exhale_class_classAstTopDownBottomUpProcessing`)
- ``public AstTopDownBottomUpProcessing< SimpleFrontierDetectionForTokenStreamMapping_InheritedAttribute, SimpleFrontierDetectionForTokenStreamMapping_SynthesizedAttribute >`` (:ref:`exhale_class_classAstTopDownBottomUpProcessing`)
- ``public AstTopDownProcessing< AddPrototypesForTemplateInstantiationsInheritedAttribute >`` (:ref:`exhale_class_classAstTopDownProcessing`)
- ``public AstTopDownProcessing< std::vector< InheritedAttributeType > * >`` (:ref:`exhale_class_classAstTopDownProcessing`)
- ``public AstTopDownProcessing< ClangToDotNextPreprocessorToInsert * >`` (:ref:`exhale_class_classAstTopDownProcessing`)
- ``public AstTopDownProcessing< DI >`` (:ref:`exhale_class_classAstTopDownProcessing`)
- ``public AstTopDownProcessing< FixupFunctionDefaultArgumentsInheritedAttribute >`` (:ref:`exhale_class_classAstTopDownProcessing`)
- ``public AstTopDownProcessing< FixupPrettyFunctionVariablesInheritedAttribute >`` (:ref:`exhale_class_classAstTopDownProcessing`)
- ``public AstTopDownProcessing< LinearizeInheritedAttribute >`` (:ref:`exhale_class_classAstTopDownProcessing`)
- ``public AstTopDownProcessing< MarkSharedDeclarationsInheritedAttribute >`` (:ref:`exhale_class_classAstTopDownProcessing`)
- ``public AstTopDownProcessing< MarkTemplateSpecializationsForOutputInheritedAttribute >`` (:ref:`exhale_class_classAstTopDownProcessing`)
- ``public AstTopDownProcessing< MarkTransformationsForOutputInheritedAttribute >`` (:ref:`exhale_class_classAstTopDownProcessing`)
- ``public AstTopDownProcessing< PDFInheritedAttribute >`` (:ref:`exhale_class_classAstTopDownProcessing`)
- ``public AstTopDownProcessing< NextPreprocessorToInsert * >`` (:ref:`exhale_class_classAstTopDownProcessing`)
- ``public AstTopDownProcessing< PropagateHiddenListDataInheritedAttribute >`` (:ref:`exhale_class_classAstTopDownProcessing`)
- ``public AstTopDownProcessing< RemoveInitializedNamePtrInheritedAttribute >`` (:ref:`exhale_class_classAstTopDownProcessing`)
- ``public AstTopDownProcessing< ResetParentPointersInheritedAttribute >`` (:ref:`exhale_class_classAstTopDownProcessing`)
- ``public AstTopDownProcessing< SimpleColorFilesInheritedAttribute >`` (:ref:`exhale_class_classAstTopDownProcessing`)
- ``public AstTopDownProcessing< TestForProperLanguageAndSymbolTableCaseSensitivity_InheritedAttribute >`` (:ref:`exhale_class_classAstTopDownProcessing`)
- ``public AstTopDownBottomUpProcessing< InheritedAttributeType, SynthesizedAttributeType >`` (:ref:`exhale_class_classAstTopDownBottomUpProcessing`)


Class Documentation
-------------------


.. doxygenclass:: SgTreeTraversal
   :project: rex
   :members:
   :undoc-members: