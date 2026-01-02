
.. _namespace_Rosebud__Ast:

Namespace Rosebud::Ast
======================


.. contents:: Contents
   :local:
   :backlinks: none




Detailed Description
--------------------

Abstract syntax tree.

This is the tree representation of the abstract syntactic structure of the :ref:`namespace_Rosebud` input. Each node of the tree denotes a construct appearing in an input file.

The root of the tree is a :ref:`exhale_class_classRosebud_1_1Ast_1_1Project` node, which points to a list of :ref:`exhale_class_classRosebud_1_1Ast_1_1File` nodes. Each file points to a list of (usually one) :ref:`exhale_class_classRosebud_1_1Ast_1_1Class` nodes. A class is a type of (inherits from) :ref:`exhale_class_classRosebud_1_1Ast_1_1Definition` and points to a list of :ref:`exhale_class_classRosebud_1_1Ast_1_1Property` nodes. Each property has a data type, initial value, and points to a list of :ref:`exhale_class_classRosebud_1_1Ast_1_1Attribute` nodes. Each attribute has a list of arguments that are of each a :ref:`exhale_class_classRosebud_1_1Ast_1_1TokenList` node. A token list is simply a list of locations in the input file.

All nodes of the AST are derived from the :ref:`exhale_class_classRosebud_1_1Ast_1_1Node` type which, among other things, provides a parent pointer. The parent pointer in a node is adjusted automatically when the node is assigned as a child of another node. All nodes are reference counted and are deleted automatically when they're no longer referenced.

Since nodes are always constructed in the heap and reference counted, their normal C++ constructors are protected. Instead, use the static ``instance`` member functions to construct nodes. Every node also has a ``Ptr`` type which is a ``std::shared_ptr<T>`` for that class of node.

For examples showing how nodes and pointers to children and parents work, see the unit :ref:`namespace_Rosebud` AST unit tests. Here's a quick preview:

autofileList=FileList::instance();

autofoo=File::instance("foo.h");
autobar=File::instance("bar.h");

fileList->push_back(foo);
:ref:`exhale_define_mlog_8h_1a78e8f09ab5fb0c4aa9ed20991ed93a3d`(fileList->at(0)==foo);
:ref:`exhale_define_mlog_8h_1a78e8f09ab5fb0c4aa9ed20991ed93a3d`(foo->parent==fileList);

fileList->at(0)==bar;
:ref:`exhale_define_mlog_8h_1a78e8f09ab5fb0c4aa9ed20991ed93a3d`(foo->parent==nullptr);
:ref:`exhale_define_mlog_8h_1a78e8f09ab5fb0c4aa9ed20991ed93a3d`(bar->parent==fileList);

fileList->pop_back();
:ref:`exhale_define_mlog_8h_1a78e8f09ab5fb0c4aa9ed20991ed93a3d`(bar->parent==nullptr);
 





Classes
-------


- :ref:`exhale_struct_structRosebud_1_1Ast_1_1CppStack_1_1Directive`

- :ref:`exhale_class_classRosebud_1_1Ast_1_1AttachmentError`

- :ref:`exhale_class_classRosebud_1_1Ast_1_1Attribute`

- :ref:`exhale_class_classRosebud_1_1Ast_1_1ChildEdge`

- :ref:`exhale_class_classRosebud_1_1Ast_1_1Class`

- :ref:`exhale_class_classRosebud_1_1Ast_1_1CppStack`

- :ref:`exhale_class_classRosebud_1_1Ast_1_1CycleError`

- :ref:`exhale_class_classRosebud_1_1Ast_1_1Definition`

- :ref:`exhale_class_classRosebud_1_1Ast_1_1Error`

- :ref:`exhale_class_classRosebud_1_1Ast_1_1File`

- :ref:`exhale_class_classRosebud_1_1Ast_1_1ListNode`

- :ref:`exhale_class_classRosebud_1_1Ast_1_1ListNode_1_1iterator`

- :ref:`exhale_class_classRosebud_1_1Ast_1_1Node`

- :ref:`exhale_class_classRosebud_1_1Ast_1_1ParentEdge`

- :ref:`exhale_class_classRosebud_1_1Ast_1_1ParentEdgeAccess`

- :ref:`exhale_class_classRosebud_1_1Ast_1_1Project`

- :ref:`exhale_class_classRosebud_1_1Ast_1_1Property`

- :ref:`exhale_class_classRosebud_1_1Ast_1_1TokenList`


Typedefs
--------


- :ref:`exhale_typedef_namespaceRosebud_1_1Ast_1afb3e77d335f3ec123ddb149b2dafad15`

- :ref:`exhale_typedef_namespaceRosebud_1_1Ast_1a503691c5f66fccf2542ee5edebdbf152`

- :ref:`exhale_typedef_namespaceRosebud_1_1Ast_1a7a916d0e3f90534e9d6d52b50c2dc6ad`

- :ref:`exhale_typedef_namespaceRosebud_1_1Ast_1ab45927fb0e7a15ecf6be56e96785b01e`

- :ref:`exhale_typedef_namespaceRosebud_1_1Ast_1a96db7abdd61fc4b028d97b0a6cda8f69`

- :ref:`exhale_typedef_namespaceRosebud_1_1Ast_1a1fcb7cbf14cf4fbca6c31ffa34ba49ea`

- :ref:`exhale_typedef_namespaceRosebud_1_1Ast_1a5f82282e5ef2243670d11d4684b74386`

- :ref:`exhale_typedef_namespaceRosebud_1_1Ast_1a3038427b9ff48105242804d352d3a9d5`

- :ref:`exhale_typedef_namespaceRosebud_1_1Ast_1ad432a33f7d572f22cd3cf098537b41dd`

- :ref:`exhale_typedef_namespaceRosebud_1_1Ast_1a96c9439b4fb17b8293edfaa255b3e384`

- :ref:`exhale_typedef_namespaceRosebud_1_1Ast_1a2cef00e0d1f76a5aa03fa106f4ac7aba`

- :ref:`exhale_typedef_namespaceRosebud_1_1Ast_1a57fe717f46987258a714cfea3f821b91`

- :ref:`exhale_typedef_namespaceRosebud_1_1Ast_1a5d40b10d0b68fbfe7ed3d278a51c10cf`

- :ref:`exhale_typedef_namespaceRosebud_1_1Ast_1a232ed0fcb99b4d53dd7bdf9660df0e51`

- :ref:`exhale_typedef_namespaceRosebud_1_1Ast_1aa72153dad1ac56e1b33fcf9e6e210ee2`
