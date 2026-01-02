.. _exhale_class_classSymbolicBoundAnalysis:

Template Class SymbolicBoundAnalysis
====================================

- Defined in :ref:`file_src_midend_astUtil_symbolicVal_SymbolicBound.h`


Inheritance Relationships
-------------------------

Base Types
**********

- ``public MapObject< SymbolicVal, SymbolicBound >`` (:ref:`exhale_class_classMapObject`)
- ``private SymbolicVisitor`` (:ref:`exhale_class_classSymbolicVisitor`)


Derived Types
*************

- ``public SymbolicConstBoundAnalysis< LoopTreeNode *, LoopTreeInterface >`` (:ref:`exhale_class_classSymbolicConstBoundAnalysis`)
- ``public SymbolicConstBoundAnalysis< AstNodePtr, DepInfoAnalInterface >`` (:ref:`exhale_class_classSymbolicConstBoundAnalysis`)
- ``public SymbolicConstBoundAnalysis< Stmt, Interface >`` (:ref:`exhale_class_classSymbolicConstBoundAnalysis`)


Class Documentation
-------------------


.. doxygenclass:: SymbolicBoundAnalysis
   :project: rex
   :members:
   :undoc-members: