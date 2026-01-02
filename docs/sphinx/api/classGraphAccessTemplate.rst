.. _exhale_class_classGraphAccessTemplate:

Template Class GraphAccessTemplate
==================================

- Defined in :ref:`file_src_util_graphs_GraphAccess.h`


Inheritance Relationships
-------------------------

Base Type
*********

- ``public GraphAccess`` (:ref:`exhale_class_classGraphAccess`)


Derived Types
*************

- ``public GraphAccessWrapTemplate< GraphAccessInterface::Node, GraphAccessInterface::Edge, GraphAccessTemplate< Node, DepInfoEdge > >`` (:ref:`exhale_class_classGraphAccessWrapTemplate`)
- ``public VirtualGraphCreateTemplate< Node, Edge >`` (:ref:`exhale_class_classVirtualGraphCreateTemplate`)
- ``public VirtualGraphCreateTemplate< CompSliceDepGraphNode, DepInfoEdge >`` (:ref:`exhale_class_classVirtualGraphCreateTemplate`)
- ``public VirtualGraphCreateTemplate< MultiGraphElemTemplate< SelfInfo >, MultiGraphElemTemplate< RelInfo > >`` (:ref:`exhale_class_classVirtualGraphCreateTemplate`)
- ``public VirtualGraphCreateTemplate< Node, CFGEdgeImpl >`` (:ref:`exhale_class_classVirtualGraphCreateTemplate`)
- ``public VirtualGraphCreateTemplate< Node, MultiGraphElem >`` (:ref:`exhale_class_classVirtualGraphCreateTemplate`)
- ``public VirtualGraphCreateTemplate< CFGNodeImpl, CFGEdgeImpl >`` (:ref:`exhale_class_classVirtualGraphCreateTemplate`)
- ``public VirtualGraphCreateTemplate< DefUseChainNode, MultiGraphElem >`` (:ref:`exhale_class_classVirtualGraphCreateTemplate`)
- ``public VirtualGraphCreateTemplate< DepCompAstRefGraphNode, DepInfoEdge >`` (:ref:`exhale_class_classVirtualGraphCreateTemplate`)
- ``public VirtualGraphCreateTemplate< Node, DepInfoEdge >`` (:ref:`exhale_class_classVirtualGraphCreateTemplate`)
- ``public VirtualGraphCreateTemplate< Node, DepInfoSetEdge >`` (:ref:`exhale_class_classVirtualGraphCreateTemplate`)
- ``public VirtualGraphCreateTemplate< GroupGraphNode, GroupGraphEdge >`` (:ref:`exhale_class_classVirtualGraphCreateTemplate`)
- ``public VirtualGraphCreateTemplate< LoopTreeDepGraphNode, DepInfoEdge >`` (:ref:`exhale_class_classVirtualGraphCreateTemplate`)
- ``public VirtualGraphCreateTemplate< ReachingDefNode, CFGEdgeImpl >`` (:ref:`exhale_class_classVirtualGraphCreateTemplate`)
- ``public VirtualGraphCreateTemplate< ValuePropagateNode, MultiGraphElem >`` (:ref:`exhale_class_classVirtualGraphCreateTemplate`)
- ``public VirtualGraphCreateTemplate< NodeImpl, EdgeImpl >`` (:ref:`exhale_class_classVirtualGraphCreateTemplate`)


Class Documentation
-------------------


.. doxygenclass:: GraphAccessTemplate
   :project: rex
   :members:
   :undoc-members: