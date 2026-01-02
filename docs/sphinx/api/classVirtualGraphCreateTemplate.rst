.. _exhale_class_classVirtualGraphCreateTemplate:

Template Class VirtualGraphCreateTemplate
=========================================

- Defined in :ref:`file_src_util_graphs_VirtualGraphCreate.h`


Nested Relationships
--------------------


Nested Types
************

- :ref:`exhale_class_classVirtualGraphCreateTemplate_1_1EdgeIteratorImpl`
- :ref:`exhale_class_classVirtualGraphCreateTemplate_1_1NodeIteratorImpl`


Inheritance Relationships
-------------------------

Base Types
**********

- ``public MultiGraphCreate`` (:ref:`exhale_class_classMultiGraphCreate`)
- ``public GraphAccessTemplate< NodeImpl, EdgeImpl >`` (:ref:`exhale_class_classGraphAccessTemplate`)


Derived Types
*************

- ``public CFGImplTemplate< Node, CFGEdgeImpl >`` (:ref:`exhale_class_classCFGImplTemplate`)
- ``public CFGImplTemplate< CFGNodeImpl, CFGEdgeImpl >`` (:ref:`exhale_class_classCFGImplTemplate`)
- ``public CFGImplTemplate< ReachingDefNode, CFGEdgeImpl >`` (:ref:`exhale_class_classCFGImplTemplate`)
- ``public DefUseChain< DefUseChainNode >`` (:ref:`exhale_class_classDefUseChain`)
- ``public DefUseChain< ValuePropagateNode >`` (:ref:`exhale_class_classDefUseChain`)
- ``public DepInfoGraphCreate< CompSliceDepGraphNode >`` (:ref:`exhale_class_classDepInfoGraphCreate`)
- ``public DepInfoGraphCreate< DepCompAstRefGraphNode >`` (:ref:`exhale_class_classDepInfoGraphCreate`)
- ``public DepInfoGraphCreate< LoopTreeDepGraphNode >`` (:ref:`exhale_class_classDepInfoGraphCreate`)


Class Documentation
-------------------


.. doxygenclass:: VirtualGraphCreateTemplate
   :project: rex
   :members:
   :undoc-members: