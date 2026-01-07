// -*- c++ -*-

#ifndef ROSE_DOCS_AST_NODE_DOCS_H
#define ROSE_DOCS_AST_NODE_DOCS_H

// Doc-only forward declarations for AST node classes and related types.
// These comments are attached to the classes for MrDocs.

/** @brief Class holding static data and functions supporting File I/O.
 *
 * - Todo: Consider making this a namespace.
 * - Todo: For the general file IO we should consider a special file name, similar to "rose_..."
 * - Tests: Test of the AST File I/O mechanism include:
 * - 1) writes the AST out to a source file, the first source file, and compiles it
 * - 2) writes out the AST as a binary file
 * - 3) deletes the AST (clears all the memory pools)
 * - 4) reads in the binary file and runs all AST tests
 * - 5) writes the AST out to a source file, the second source file, and compiles it
 * - 6) runs a diff between the first source file and the second source file
 * - 7) Using a new program that just reads the binary AST file, he reads the binary AST file
 * - 8) writes the AST out to a source file, the first source file, and compiles it
 * - 9) runs a diff between the first source file and the third source file
 */
class AST_FILE_IO;

/** @brief Class for traversing the AST.
 *
 * - Todo: Make options 'preorder' and 'postorder' local to the class (will brake user code).
 * This class allows to traverse the AST in preorder or postorder. A visit function must be implemented
 * by the user which is automaticly called by the provided AST traversal.
 * - Internal: This class is derived from the SgTreeTraversal class.
 *
 * **Member functions**
 *
 * #### `AstSimpleProcessing::traverse(SgNode* node, Order treeTraversalOrder)`
 * Function to start the traversal on the subtree defined by node.
 * This is the call to start the traversal on the subtree defined by node.
 * - Param `node`: represents the root of the subtree in the AST
 * - Param `treeTraversalOrder`: represents the traversal order (preorder or postorder).
 *
 * #### `AstSimpleProcessing::traverseInputFiles(SgProject* projectNode, Order treeTraversalOrder)`
 * Function to start the traversal on the subtree defined by root (skip and nodes from other files).
 * This is the call to start the traversal on the subtree defined by root.
 * This function will cause AST nodes that have a source position in any other
 * files to be skipped (skips all header files for example).
 * - Param `projectNode`: represents the root of the subtree in the AST
 * - Param `treeTraversalOrder`: represents the traversal order (preorder or postorder).
 */
class AstSimpleProcessing;

/** @brief RHS of an array variable declaration with optional assignment.
 *
 * This class represents the rhs of an array variable declaration which includes an optional
 * assignment (e.g. "int x[2] = {1,2};").  In this case the SgExprListExp ("{1,2}") is wrapped in an
 * SgAggregateInitializer.
 * - See also:
 * Example of using a SgAggregateInitializer object
 *
 * **Data members**
 *
 * #### `SgAggregateInitializer::p_initializers`
 * This points to a SgExprListExp (list of expressions).
 *
 * **Member functions**
 *
 * #### `SgAggregateInitializer::SgAggregateInitializer ( Sg_File_Info* startOfConstruct = NULL )`
 * Constructor.
 * This constructor builds the SgAggregateInitializer base class.
 * - Param `startOfConstruct`: represents the position in the source code
 * - See also:
 * Example:create an SgAggregateInitializer object
 *
 * #### `SgAggregateInitializer::~SgAggregateInitializer()`
 * This is the destructor.
 * Only the Sg_File_Info object can be deleted in this object.
 *
 * #### `SgAggregateInitializer::isSgAggregateInitializer (SgNode *s)`
 * Cast function (from derived class to SgAggregateInitializer pointer).
 * This functions returns a SgAggregateInitializer pointer for any input of a
 * pointer to an object derived from a SgAggregateInitializer.
 * - Return: Returns valid pointer to SgAggregateInitializer if input is derived from a SgAggregateInitializer.
 *
 * #### `SgAggregateInitializer::isSgAggregateInitializer (const SgNode *s)`
 * Cast function (from derived class to SgAggregateInitializer pointer).
 * This functions returns a SgAggregateInitializer pointer for any input of a
 * pointer to an object derived from a SgAggregateInitializer.
 * - Return: Returns valid pointer to SgAggregateInitializer if input is derived from a SgAggregateInitializer.
 *
 * #### `SgAggregateInitializer::get_initializers() const`
 * Returns the rhs.
 * - Return: Returns SgExprListExp.
 *
 * #### `SgAggregateInitializer::set_initializers (SgExprListExp initializers)`
 * This sets the rhs expression.
 * - Param `initializersp`: - sets value of rhs.
 * - Return: Returns void.
 */
class SgAggregateInitializer;

/** @brief This class represents the concept of a C Assembler statement.
 *
 * This IR node supports the gnu standard names for registers, and specifially
 * the X86 register set.  Other register sets could be supported, for details contact
 * the ROSE development team.  More information about the use of asm statements can
 * be easily found on the web.
 * - Note: This is part of support for embedded programming, however the Linux header files
 * contain numerous example of asm statements demonstrating there somewhat suprising
 * wide-spread use.
 * - Internal: Test code for this include test2006_98.C and test2006_99.C.
 * - Todo: This should not be a SgDeclaration (should be a regular SgStatement).
 * Where "asm" is used in declarations (e.g. "int x asm("ax") = 0;") this
 * is a SgVariableDeclaration.  A asm statment is really just another kind
 * of statement (e.g "asm ("template",output operand, input operand);").
 * This sort of statement is not a declaration (as I understand it).  This
 * will be fixed in a later release.
 * - See also:
 * Example of using a SgAsmStmt object
 *
 * **Data members**
 *
 * #### `SgExpressionPtrList SgAsmStmt::p_operands`
 * List of expressions.
 * List of expressions: first the asm template, followed by the output
 * operand, and all input operands.
 * Note that the first element of the list if always a SgStringVal IR node containing the
 * asm template string, the second operand is an output operand (SgAsmOp), the rest of the
 * expressions in this list are the input operands (also SgAsmOp).
 *
 * #### `SgAsmStmt::AsmRegisterNameList SgAsmStmt::p_clobberRegisterList`
 * This is the clobber list (list of registers where side-effects happen).
 * This is a list of enum values (type SgInitializedName::asm_register_name_enum)
 * that are standard gnu names for registers.  The only currently supported register
 * set is X86, other register sets could be supported in the future.
 *
 * **Member functions**
 *
 * #### `SgAsmStmt::SgAsmStmt ( Sg_File_Info* startOfConstruct = NULL )`
 * This is the constructor.
 * This constructor builds the SgAsmStmt base class.
 * - Param `startOfConstruct`: represents the position in the source code
 *
 * #### `SgAsmStmt::~SgAsmStmt()`
 * This is the destructor.
 * There are a lot of things to delete, but nothing is deleted in this destructor.
 *
 * #### `SgAsmStmt::isSgAsmStmt (SgNode *s)`
 * Cast function (from derived class to SgAsmStmt pointer).
 * This functions returns a SgAsmStmt pointer for any input of a
 * pointer to an object derived from a SgAsmStmt.
 * - Return: Returns valid pointer to SgAsmStmt if input is derived from a SgLocatedNode.
 *
 * #### `SgAsmStmt::isSgAsmStmt (const SgNode *s)`
 * Cast function (from derived class to SgAsmStmt pointer).
 * This functions returns a SgAsmStmt pointer for any input of a
 * pointer to an object derived from a SgAsmStmt.
 * - Return: Returns valid pointer to SgAsmStmt if input is derived from a SgLocatedNode.
 *
 * #### `SgAsmStmt::get_operands()`
 * Access function for operand list (STL list).
 * - Return: Returns STL list of SgAsmOp by reference.
 *
 * #### `SgAsmStmt::get_operands() const`
 * Access function for operand list (STL list).
 * - Return: Returns STL list of SgAsmOp by const reference.
 *
 * #### `SgAsmStmt::get_clobberRegisterList()`
 * Access function for operand list (STL list).
 * - Return: Returns STL list of SgInitializedName::asm_register_name_enum by reference (SgAsmStmt::AsmRegisterNameList).
 *
 * #### `SgAsmStmt::get_clobberRegisterList() const`
 * Access function for operand list (STL list).
 * - Return: Returns STL list of SgInitializedName::asm_register_name_enum by const reference (SgAsmStmt::AsmRegisterNameList).
 */
class SgAsmStmt;

/** @brief This class represents the rhs of a variable declaration which includes an optional
 *
 * assignment (e.g. "int x = 1;").  In this case the SgValue ("1") is wrapped in an
 * SgAssignInitializer.
 * - Todo: Double check the accuracy of this description.
 * - See also:
 * Example of using a SgAssignInitializer object
 *
 * **Data members**
 *
 * #### `SgAssignInitializer::p_operand_i`
 * This points to the internal SgExpression (right-hand-side expression).
 *
 * #### `SgAssignInitializer::p_expression_type`
 * This points to the SgType of the rhs expression.
 * - Internal: Should this be stored explicitly, it could be obtained from the rhs expression.
 *
 * **Member functions**
 *
 * #### `SgAssignInitializer::SgAssignInitializer ( Sg_File_Info* startOfConstruct = NULL )`
 * This is the constructor.
 * This constructor builds the SgAssignInitializer base class.
 * - Param `startOfConstruct`: represents the position in the source code
 * - See also:
 * Example:create an SgAssignInitializer object
 *
 * #### `SgAssignInitializer::~SgAssignInitializer()`
 * This is the destructor.
 * Only the Sg_File_Info object can be deleted in this object.
 *
 * #### `SgAssignInitializer::isSgAssignInitializer (SgNode *s)`
 * Cast function (from derived class to SgAssignInitializer pointer).
 * This functions returns a SgAssignInitializer pointer for any input of a
 * pointer to an object derived from a SgAssignInitializer.
 * - Return: Returns valid pointer to SgAssignInitializer if input is derived from a SgAssignInitializer.
 *
 * #### `SgAssignInitializer::isSgAssignInitializer (const SgNode *s)`
 * Cast function (from derived class to SgAssignInitializer pointer).
 * This functions returns a SgAssignInitializer pointer for any input of a
 * pointer to an object derived from a SgAssignInitializer.
 * - Return: Returns valid pointer to SgAssignInitializer if input is derived from a SgAssignInitializer.
 *
 * #### `SgAssignInitializer::get_operand() const`
 * Returns the rhs.
 * - Return: Returns SgExpression.
 *
 * #### `SgAssignInitializer::set_operand (SgExpression exp)`
 * This sets the rhs expression.
 * - Param `exp`: - sets value of rhs.
 * - Return: Returns void.
 */
class SgAssignInitializer;

/** @brief This class represents the concept of a block (not a basic block from control flow analysis).
 *
 * SgBasicBlocks are used in other IR nodes where a list of statements is
 * required.  However, if you need a list of statements we suggest you use an
 * STL list instead of borrowing and extending the semantics of SgBasicBlock.
 * - Internal:
 *
 * **Data members**
 *
 * #### `SgBasicBlock::p_statements`
 * This pointer an STL list of pointers to SgStatement objects.
 *
 * **Member functions**
 *
 * #### `SgBasicBlock::SgBasicBlock ( Sg_File_Info* startOfConstruct = NULL )`
 * This is the constructor.
 * This constructor builds the SgBasicBlock base class.
 * - Param `startOfConstruct`: represents the position in the source code
 *
 * #### `SgBasicBlock::~SgBasicBlock()`
 * This is the destructor.
 * There are a lot of things to delete, but nothing is deleted in this destructor.
 *
 * #### `SgBasicBlock::isSgBasicBlock (SgNode *s)`
 * Cast function (from derived class to SgBasicBlock pointer).
 * This functions returns a SgBasicBlock pointer for any input of a
 * pointer to an object derived from a SgBasicBlock.
 * - Return: Returns valid pointer to SgBasicBlock if input is derived from a SgLocatedNode.
 *
 * #### `SgBasicBlock::isSgBasicBlock (const SgNode *s)`
 * Cast function (from derived class to SgBasicBlock pointer).
 * This functions returns a SgBasicBlock pointer for any input of a
 * pointer to an object derived from a SgBasicBlock.
 * - Return: Returns valid pointer to SgBasicBlock if input is derived from a SgLocatedNode.
 *
 * #### `SgBasicBlock::get_symbol_table()`
 * Returns a pointer to the locally strored SgSymbolTable.
 * - Return: Returns a pointer.
 *
 * #### `SgBasicBlock::set_symbol_table(SgSymbolTable *symbolTable)`
 * Sets the pointer to the locally strored SgSymbolTable.
 * - Return: Returns void.
 *
 * #### `SgBasicBlock::get_qualified_name() const`
 * Returns SgName (a string) representing the name of the current scope.
 * See discussion of mangled names in the documentation.
 * - Return: Returns SgName (a string).
 *
 * #### `SgBasicBlock::copy(const SgCopyHelp & help)`
 * Makes a copy (deap of shallow depending on SgCopyHelp).
 * - Return: Returns pointer to copy of SgBasicBlock.
 *
 * #### `SgBasicBlock::get_statements() const`
 * Returns a const STL list by reference.
 * - Return: Returns const SgStatementPtrList (STL list) by reference.
 *
 * #### `SgBasicBlock::get_statements()`
 * Returns a non-const STL list by reference.
 * - Return: Returns non-const SgStatementPtrList (STL list) by reference.
 */
class SgBasicBlock;

/** @brief This class represents the base class for all IR nodes supporting the binary
 *
 * representation of software within Sage III.
 * This class is used as a base class for all IR nodes in Sage III used to support
 * the binary representation of software (supporting binary analysis).
 * - Note: The names of the IR nodes used in this subtree of SageIII are not finalized yet.
 * - See also:
 * Example of using a SgBinaryNode object
 * - See also:
 * Enforced AST Properties
 *
 * **Data members**
 *
 * #### `SgBinaryNode::attribute`
 * Attribute mechanism used to support This is the new attribute mechanism.
 * This is part of a new attribute mechanism. It is difference
 * from the one originally used by Sage II.
 *
 * #### `SgNode::p_parent`
 * This is the pointer to the parent IR node in the AST.
 * This is the pointer to the parent IR node.  It is a valid pointer
 * on all nodes that are traversed (SgExpressions, SgStatements, SgInitializedName, etc.)
 * However it is not set on SgTypes and SgSymbols,both of which are shared internally.
 * This pointer is mostly set in post processing of the Sage III AST, until
 * this point it is not reliable.
 *
 * #### `SgNode::p_isVisited`
 * This the visit flag previously used by the AST traversals.
 * This the visit flag previously used by the AST traversals. It is
 * no longer used in the AST traversals, though the traversals can be
 * set at compile-time to alternatively use this visit flag.  The visit
 * flag is part of an older design of the AST traversal, it was problematic
 * by design, because it had to be reset after each traversal.  It also
 * placed requirements on any newly added IR nodes within the AST (they
 * had to be set so just right so that they could be properly reset).
 * The AST traversals are greatly simplified as a result of no longer
 * requiring this visit flag.  This variable will be removed in the
 * future.
 * - Deprecated: Old traversal supporting mechanism (will be removed).
 *
 * #### `SgNode::p_freepointer`
 * This is the pointer to the chain of previously freed objects.
 * - Internal: This is part of the support for memory pools within ROSE.  The freepointer is
 * only manipulated by the delete operator which constructs a chain of previously freed
 * objects embedded within the memory pools.  The chain of objects link by the freepointer
 * variables are traversed by the new operator to allocate (reuse) previously deleted objects.
 * The new operator does not reset the freepointers since once allocated the freepointer is
 * ignored.
 *
 * #### `SgNode::p_isModified`
 * Records if IR node has been modified (data members reset).
 * This is part of an internal mechanism that records if IR nodes have
 * been modified, either by a transformation or a access function to set
 * a value on the IR node.  All access functions that set IR node data members
 * are automatically generated (except the ones for this data member) and include
 * code to set this boolean flag to true.  This is also part of the
 * support for copy based code generation where source file is copied except
 * where the AST was modified an for these subtrees new code is generated
 * from the AST.
 * - Internal: true if IR node has been modified, else false.
 *
 * #### `SgNode::p_globalFunctionTypeTable`
 * Pointer to symbol table specific to function types.
 * - Internal: Always a valid pointer.
 *
 * **Member functions**
 *
 * #### `SgNode::copy(const SgCopyHelp & help ) const`
 * This function clones the current IR node object recursively or not, depending on the argument
 * This function performs a copy based on the specification of the input parameter.
 * The input parameter is used to determin which data members are copied by reference
 * and which are copied by value.
 * - Param `help`: - If this argument is of type SgTreeCopy, then the
 * IR node is cloned recursively. If its of type SgShallowCopy
 * only the first level of the IR node is copied, everything else is
 * left pointing to the the original IR node's object's data members.
 * - Return: a pointer to the new clone.
 * - Internal: It appears the the copy functions don't set the parents of anything that they do
 * a deep copy of! This can cause AST tests to fail. In particular some functions that
 * require the parent pointers to be valid will return NULL pointers (e.g. SgInitializedName::get_declaration()).
 * It might be that we should allow this to be done as part of the
 * SgCopyHelp::clone function or perhaps another member function of SgCopyHelp would be
 * useful for this support.  It is not serious if the AST post processing is done since
 * that will set any NULL pointers that are found within its traversal.
 * - Exception: none No exceptions are thrown by this function.
 *
 * #### `SgNode::SgNode()`
 * This is the constructor
 * This constructor builds a SgNode, always used as a base class.
 * All Sage III IR nodes are derived from this class.
 * - See also:
 * Example:create an SgNode object
 *
 * #### `SgNode::~SgNode()`
 * This is the destructor.
 * There is nothing to delete in this object.
 *
 * #### `SgNode::sage_class_name() const`
 * generates string representing the class name: (e.g. for SgNode returns "SgNode").
 * This function is useful for debugging and error reporting.  It
 * returns the name of the IR node.
 * - Deprecated: Use class_name() which returns a C++ string object.
 * - Return: a char* pointer to a C style string.
 *
 * #### `string SgNode::class_name() const`
 * generates string representing the class name: (e.g. for SgNode returns "SgNode").
 * This function is useful for debugging and error reporting.  It
 * returns the name of the IR node.
 * - Return: a C++ string object.
 *
 * #### `SgNode::variant() const`
 * Older version function returns enum value "NODE"
 * This function is an older version of the variant function.
 * This function is useful for debugging and error reporting.  It
 * returns the name of the IR node.
 * - Return: an int value.
 * - Deprecated: Use Tvariant() instead.  Older enum values have inconsistant
 * names and are being removed.
 *
 * #### `SgNode::getVariant() const`
 * Older version function returns enum value "NODE"
 * This function is an older version of the variant function.
 * This function is useful for debugging and error reporting.  It
 * returns the name of the IR node.
 * - Return: an int value.
 * - Deprecated: Use Tvariant() instead.  Older enum values have inconsistant
 * names and are being removed.
 *
 * #### `SgNode::variantT() const`
 * returns enum value "V_SgNode"
 * This function is useful for debugging and error reporting.  It
 * returns the name of the IR node.
 * - Return: an enum value (type: VariantT).
 * - Deprecated: Use Tvariant() instead.  Older enum values have inconsistant
 * names and are being removed.
 *
 * #### `SgNode::get_isModified() const`
 * **FOR** **INTERNAL** **USE** All nodes in the AST contain a isModified flag.
 * This flag can be set but this is typically an internal function used to track the updates to AST.
 * - Return: Returns bool; true if IR node has been modified.
 *
 * #### `SgNode::set_isModified(bool isModified)`
 * Acess function for isModified flag.
 * This flag records if the current IR node has been modified. It is set to false after
 * and ROSE front-end processing.
 *
 * #### `SgNode::get_isVisited() const`
 * DOCS IN SgNode.docs: Access function for p_isVisited flag used previously by the AST traversals.
 * - Deprecated: This function is not used and will be removed.
 * - Return: Returns bool; true if previously visited within current AST traversal.
 *
 * #### `SgNode::set_isVisited(bool isVisited)`
 * Access function for p_isVisited flag used previously by the AST traversals.
 * - Deprecated: This function is not used and will be removed.
 *
 * #### `SgNode::isSgNode (SgNode *s)`
 * Cast function (from derived class to SgNode pointer).
 * This functions returns a SgNode pointer for any input of a
 * pointer to an object derived from a SgNode.
 * - Return: Returns valid pointer to SgNode if input is derived from a SgNode.
 *
 * #### `SgNode::isSgNode (const SgNode *s)`
 * Cast function (from derived class to SgNode pointer).
 * This functions returns a SgNode pointer for any input of a
 * pointer to an object derived from a SgNode.
 * - Return: Returns valid pointer to SgNode if input is derived from a SgNode.
 *
 * #### `SgNode::set_parent (SgNode *parent)`
 * Sets parent node for any IR node.
 * The parent node in the AST can be set but this is typically
 * an internal function used to build the AST.
 * - Param `parent`: - Pointer to parent node to store within the current IR node.
 * - Return: returns void.
 *
 * #### `SgNode::get_parent() const`
 * Access function for parent node in AST.
 * The parent node in the AST can be accessed, in general only the project node, symbols and
 * types should be NULL.  Since parent nodes are set within post processing
 * (and using the ROSE AST traveral mechanism) the parents are guarenteed to
 * match the traversal, and no other concept of what could be a parent within
 * the AST (e.g a parent concept based on scope).  Because the traversal is based
 * on the source code layout, what is a parent is similarly based on the source
 * code layout and not any concept of scope.  Note that the scope of relavant
 * IR nodes is stored explicitly in the AST, precisely because it is not always
 * related to the layout of the source code (and thus not related to the concept
 * of parent in the AST).
 * - Return: Returns pointer to SgNode
 *
 * #### `SgNode::unparseToString( SgUnparse_Info* info )`
 * This function unparses the AST node (source code only, excluding comments and white space)
 * - Param `info`: is optional (used only to specify code generation options).
 * This function is useful for converting AST nodes to strings as part of general
 * debugging or the construction of other strings for use as input to the AST rewrite
 * mechansims which accepts source code strings.  See tutorial for examples of this.
 * This function uses the SgUnparse_Info as an inherited attribute internally
 * and using this attribute it will correctly handled many subtle details that
 * will be ignored if the attribute is not provided.  For example, the SgUnparse_Info
 * can record if the statement is in a conditional and if so then the trailing ";"
 * will be omitted in the generated code. See the SgUnparse_Info documentation for
 * the numerous other internal settings that can effect the generated code.  Because
 * of these details, the unparseToString() function can not always be used to generate
 * compiliable code.
 * - Return: Returns std::string
 *
 * #### `SgNode::unparseToCompleteString()`
 * This function unparses the AST node (including comments and white space)
 * This function is a complement to the unparseToString() function and includes
 * any associated comments and preprocessor control directives.  Because C preprocessor
 * control directive can be included string generated using this function may or may
 * not be appropriate for use as input to the AST rewrite mechanism.
 * - Todo: This function needs a better name since it is unclear what the "complete" string is.
 * - Return: Returns std::string
 *
 * #### `SgNode::get_traversalSuccessorContainer()`
 * **FOR** **INTERNAL** **USE** within ROSE traverals mechanism only.
 * This function builds and returns a copy of ordered container
 * holding pointers to children of this node in a traversal. It is
 * associated with the definition of a tree that is travered by the
 * AST traversal mechanism; a tree that is embeded in the AST (which
 * is a more general graph).  This function is used within the implementation
 * of the AST traversal and has a semantics may change in subtle ways
 * that makes it difficult to use in user code.  It can return unexpected
 * data members and thus the order and the number of elements is unpredicable
 * and subject to change.
 * - Warning: This function can return unexpected data members and thus the
 * order and the number of elements is unpredicable and subject
 * to change.
 * - Return: Returns ordered STL Container of pointers to children nodes in AST.
 *
 * #### `SgNode::get_traversalSuccessorNamesContainer()`
 * **FOR** **INTERNAL** **USE** within ROSE traverals mechanism only.
 * This function builds and returns a copy of ordered container
 * holding strings used to name data members that are traversed in the IR
 * node. It is associated with the definition of a tree that is travered by the
 * AST traversal mechanism; a tree that is embeded in the AST (which
 * is a more general graph).  This function is used within the implementation
 * of the AST traversal and has a semantics may change in subtle ways
 * that makes it difficult to use in user code.  It can return unexpected
 * data members and thus the order and the number of elements is unpredicable
 * and subject to change.
 * - Warning: This function can return unexpected data members and thus the
 * order and the number of elements is unpredicable and subject
 * to change.
 * Each string is a name of a member variable holding a pointer to a
 * child in the AST. The names are the same as used in the generated enums for
 * accessing attributes in a traversal. The order is the same in which they are
 * traversed and the same in which the access enums are defined. Therefore this
 * method can be used to get the corresponding name (string) of an access enum
 * which allows to produce more meaningful messages for attribute computations.
 * - Return: Returns ordered STL container of names (strings) of access names to children nodes in AST.
 *
 * #### `SgNode::roseRTI ()`
 * **FOR** **INTERNAL** **USE** Access to Runtime Type Information (RTI) for this IR nodes.
 * This function provides runtime type information for accessing the
 * structure of the current node.  It is useful for generating code which
 * would dump out or rebuild IR nodes.
 * - Return: Returns a RTIReturnType object (runtime type information).
 *
 * **Additional notes**
 */
class SgBinaryNode;

/** @brief This class represents the notion of a binary operator.
 *
 * It is derived from a SgExpression because operators are expressions.
 * There are no uses of this IR node anywhere.  All expressions
 * are derived from this IR node to build derived classes.  Example
 * binary operators include binary minus, binary plus, the address operator,
 * etc.
 * - Internal: This is a base class for all binary operators.
 * - See also:
 * Example of using a SgBinaryOp object
 *
 * **Data members**
 *
 * #### `SgExpression* SgBinaryOp::p_lhs_operand_i`
 * This is the operand associated with the lhs of the binary operator.
 * Every binary operator is applied to two operands, this
 * variable stores the lhs operand to which the binary operator is applied
 * (with the rhs).
 *
 * #### `SgExpression* SgBinaryOp::p_rhs_operand_i`
 * This is the operand associated with the rhs of the binary operator.
 * Every binary operator is applied to two operands, this
 * variable stores the rhs operand to which the binary operator is applied
 * (with the lhs).
 *
 * #### `SgBinaryOp::p_expression_type`
 * This SgType is the type of the operator (function type).
 * This value is the return type of the operator, different from
 * a SgFunctionRefExp, but similar to a SgFunctionCall.  Note
 * that overloaded operators appear in Sage III as functions
 * with a specific name (e.g. operator+, operator-, operator*, etc.).
 * They are not Binary operator, they are SgFunctionRefExp objects.
 * Other mechanisms within ROSE provide support for the classification of
 * overloader operators OverloaderOperatorSupport.
 *
 * **Member functions**
 *
 * #### `SgBinaryOp::SgBinaryOp ( Sg_File_Info* startOfConstruct = NULL, SgExpression* lhs_operand_i = NULL, SgExpression* rhs_operand_i = NULL, SgType* expression_type = NULL )`
 * This is the constructor.
 * This constructor builds the SgBinaryOp base class.
 * - Param `startOfConstruct`: represents the position in the source code
 * - Param `lhs_operand_i`: represents the lhs operand to which the operator is applied
 * - Param `rhs_operand_i`: represents the rhs operand to which the operator is applied
 * - Param `expression_type`: represents the type of the return value of the operator
 * - See also:
 * Example:create an SgBinaryOp object
 *
 * #### `SgBinaryOp::~SgBinaryOp()`
 * This is the destructor.
 * Only the Sg_File_Info object can be deleted in this object.
 *
 * #### `SgBinaryOp::isSgBinaryOp (SgNode *s)`
 * Cast function (from derived class to SgBinaryOp pointer).
 * This functions returns a SgBinaryOp pointer for any input of a
 * pointer to an object derived from a SgBinaryOp.
 * - Return: Returns valid pointer to SgBinaryOp if input is derived from a SgBinaryOp.
 *
 * #### `SgBinaryOp::isSgBinaryOp (const SgNode *s)`
 * Cast function (from derived class to SgBinaryOp pointer).
 * This functions returns a SgBinaryOp pointer for any input of a
 * pointer to an object derived from a SgBinaryOp.
 * - Return: Returns valid pointer to SgBinaryOp if input is derived from a SgBinaryOp.
 *
 * #### `SgBinaryOp::get_lhs_operand() const`
 * returns SgExpression pointer to the lhs operand associated with this binary operator.
 * - Return: Returns SgExpression pointer.
 *
 * #### `SgBinaryOp::set_lhs_operand (SgExpression* operand)`
 * This function allows the p_lhs_operand pointer to be set (used internally).
 * This function is mostly used internally and is only required to support editing
 * of existing SgBinaryOp objects.
 * - Param `operand`: - sets value of internal p_lhs_operand pointer.
 * - Return: Returns void.
 *
 * #### `SgBinaryOp::get_rhs_operand() const`
 * returns SgExpression pointer to the rhs operand associated with this binary operator.
 * - Return: Returns SgExpression pointer.
 *
 * #### `SgBinaryOp::set_rhs_operand (SgExpression* operand)`
 * This function allows the p_rhs_operand pointer to be set (used internally).
 * This function is mostly used internally and is only required to support editing
 * of existing SgBinaryOp objects.
 * - Param `operand`: - sets value of internal p_rhs_operand pointer.
 * - Return: Returns void.
 *
 * #### `SgBinaryOp::get_expression_type (void) const`
 * returns type of operator expression.
 * This function returns the type of the binary operator.
 * - Return: Returns type of operator expression.
 *
 * #### `SgBinaryOp::set_expression_type (SgType* expression_type)`
 * This function allows the p_expression_type pointer to be set (used internally).
 * This function is mostly used internally and is only required to support editing
 * of existing SgBinaryOp objects. In general it is not changed once it is set.
 * - Param `expression_type`: - sets value of internal p_expression_type pointer.
 * - Return: Returns void.
 *
 * #### `SgBinaryOp::length() const`
 * Returns number of operands (virtual function)
 * Returns number of operands (all binary operators return value = 1).
 * This function is not used and is not a defined part a minumal interface
 * for Sage III.
 * - Deprecated: This function is not used.
 * - Return: Returns int
 *
 * #### `SgBinaryOp::empty () const`
 * Returns true if number of operands is zero, else false.
 * This function returns boolean value given by (length() == 0).
 * This function is not used and is not a defined part a minumal interface
 * for Sage III.
 * - Deprecated: This function is not used.
 * - Return: Returns bool
 *
 * #### `SgBinaryOp::get_lhs_operand_i() const`
 * returns SgExpression pointer to the operand associated with this binary operator.
 * - Deprecated: This function is not used.
 * - Return: Returns SgExpression pointer.
 *
 * #### `SgBinaryOp::set_lhs_operand_i (SgExpression* operand)`
 * This function allows the p_lhs_operand_i pointer to be set (used internally).
 * This function is mostly used internally and is only required to support editing
 * of existing SgBinaryOp objects.
 * - Deprecated: This function is not used.
 * - Param `operand`: - sets value of internal p_lhs_operand pointer.
 * - Return: Returns void.
 *
 * #### `SgBinaryOp::get_rhs_operand_i() const`
 * returns SgExpression pointer to the operand associated with this binary operator.
 * - Deprecated: This function is not used.
 * - Return: Returns SgExpression pointer.
 *
 * #### `SgBinaryOp::set_rhs_operand_i (SgExpression* operand)`
 * This function allows the p_rhs_operand_i pointer to be set (used internally).
 * This function is mostly used internally and is only required to support editing
 * of existing SgBinaryOp objects.
 * - Deprecated: This function is not used.
 * - Param `operand`: - sets value of internal p_rhs_operand pointer.
 * - Return: Returns void.
 */
class SgBinaryOp;

/** @brief This class represents a boolean value (expression value).
 *
 * - Internal: This is a base class for all value expressions.
 * - Todo: The name "SgBoolValExp" should be "SgBoolVal" to be consistant with the names of all
 * other classes derived from SgValueExp.
 * - See also:
 * Example of using a SgBoolValExp object
 *
 * **Data members**
 *
 * #### `SgBoolValExp::p_value`
 * This boolean variable marks the current expression as a
 * left hand side value (lvalue).
 *
 * **Member functions**
 *
 * #### `SgBoolValExp::SgBoolValExp ( Sg_File_Info* startOfConstruct = NULL )`
 * This is the constructor.
 * This constructor builds the SgBoolValExp base class.
 * - Param `startOfConstruct`: represents the position in the source code
 * - See also:
 * Example:create an SgBoolValExp object
 *
 * #### `SgBoolValExp::~SgBoolValExp()`
 * This is the destructor.
 * Only the Sg_File_Info object can be deleted in this object.
 */
class SgBoolValExp;

/** @brief This class represents the notion of a break statement
 *
 * (typically used in a switch statment).
 * This class is used to exit from inside of a jump to the outer
 * block.  It is a specific feature of the C language.  It is typically used to
 * break out of a basic clock or a loop. The break statement is important to
 * the specification of the control flow within a program.
 * The SgBreakStmt is derived from a SgStatement and does not have any
 * data members.
 * - Internal: The name of this class will be changed to SgBreakStatement in
 * the future.  See FutureNameChanges for details on proposed future
 * name changes.
 * - See also:
 * Example of creating a SgBreakStmt object \n
 * Example of using a SgBreakStmt object
 *
 * **Member functions**
 *
 * #### `SgBreakStmt::SgBreakStmt ( Sg_File_Info* startOfConstruct = NULL )`
 * This is the constructor
 * This constructor builds a SgBreakStmt used typically to break out of
 * a loop of within a switch statement's case statement.
 * - Param `startOfConstruct`: represents the position in the source code
 * - See also:
 * Example:create an SgBreakStatement object
 *
 * #### `SgBreakStmt::~SgBreakStmt()`
 * This is the destructor.
 * There is nothing to delete in this object.
 *
 * **Additional notes**
 * There are no data members for this class.
 */
class SgBreakStmt;

/** @brief This class represents the concept of a generic call expression.
 *
 * This IR node is intended for use with languages that allow calls of arbitary
 * expressions, even when the called expression does not resolve to a function type. For
 * statically-typed languages, use SgFunctionCallExp.
 * Note that a function call is assembled from a function expression and arguments.
 * The two are bound together in a function call, function expressions are never called
 * directly (the function call argument list can be empty).
 * - Internal: When overloaded operators are used the function name becomes the name of the
 * operator (e.g. "operator+").  In these cases the precedence of the operator is that
 * of the operator being overloaded and is different from a normal function call.  Most
 * of this detail is hidden since operands in the expression tree are represented by functions
 * with parameter lists (represented by a SgExprListExp), and not simple expression trees.
 *
 * **Data members**
 *
 * #### `SgCallExpression::p_function`
 * This pointer points to the expression being assembled with argments for be a
 * function call.
 * Note that this is not always a SgFunctionRefExp or a SgMemberFunctionRefExp
 * and can be quite complex where pointers are referenced and function calls assembled using
 * pointers to functions and member functions.
 * This member is named 'p_function' for historical reasons, but it can point to any
 * arbitrary expression.
 * - Internal: Derived classes may restrict the list of acceptable types of IR nodes here.
 * They are listed and tested in the AST consistency tests
 * (see src/midend/astDiagnostics/AstConsistencyTests.C for details).
 *
 * #### `SgCallExpression::p_args`
 * This is a pointer to a SgExprListExp (list of function arguments)
 *
 * #### `SgCallExpression::p_expression_type`
 * This is the type of the return value of the function.
 *
 * **Member functions**
 *
 * #### `SgCallExpression::get_type()`
 * The type of this call expression.
 * If the 'p_function' data member resolves to a function type, this
 * expression returns the corresponding return type. Otherwise, the
 * return value is undefined.
 *
 * #### `SgCallExpression::SgCallExpression ( Sg_File_Info* startOfConstruct = NULL, SgClassSymbol *symbol = NULL )`
 * This is the constructor.
 * This constructor builds the SgCallExpression base class.
 * - Param `startOfConstruct`: represents the position in the source code
 *
 * #### `SgCallExpression::~SgCallExpression()`
 * This is the destructor.
 * There are a lot of things to delete, but nothing is deleted in this destructor.
 *
 * #### `SgCallExpression::isSgCallExpression (SgNode *s)`
 * Cast function (from derived class to SgCallExpression pointer).
 * This functions returns a SgCallExpression pointer for any input of a
 * pointer to an object derived from a SgCallExpression.
 * - Return: Returns valid pointer to SgCallExpression if input is derived from a SgLocatedNode.
 *
 * #### `SgCallExpression::isSgCallExpression (const SgNode *s)`
 * Cast function (from derived class to SgCallExpression pointer).
 * This functions returns a SgCallExpression pointer for any input of a
 * pointer to an object derived from a SgCallExpression.
 * - Return: Returns valid pointer to SgCallExpression if input is derived from a SgLocatedNode.
 */
class SgCallExpression;

/** @brief This class represents the concept of a C and C++ case option (used within a switch statement).
 *
 * - See also:
 * Example of using a SgCaseOptionStmt object
 *
 * **Data members**
 *
 * #### `SgCaseOptionStmt::p_key_root`
 * This pointer points to a SgExpressionRoot object.
 * - Todo: Evaluate if this should really be a SgExpressionRoot or just a SgExpression.
 * It should perhaps really be a SgValue (but check the C++ grammar to be sure).
 * then make the p_key_range_end the same type to be consistant.
 *
 * #### `SgCaseOptionStmt::p_key_range_end`
 * This pointer points to the last constant in the range when a gnu range
 * case label is used.
 * - Internal: We can't represent the range a ... b as an expression in C or C++ so we need to
 * explicitly store the start and end of the range where it is used as a case label.
 * The p_key_root is the start of the range.
 *
 * #### `SgCaseOptionStmt::p_body`
 * This pointer points to a SgBasicBlock object.
 *
 * **Member functions**
 *
 * #### `SgCaseOptionStmt::SgCaseOptionStmt ( Sg_File_Info* startOfConstruct = NULL )`
 * This is the constructor.
 * This constructor builds the SgCaseOptionStmt base class.
 * - Param `startOfConstruct`: represents the position in the source code
 *
 * #### `SgCaseOptionStmt::~SgCaseOptionStmt()`
 * This is the destructor.
 * There are a lot of things to delete, but nothing is deleted in this destructor.
 *
 * #### `SgCaseOptionStmt::isSgCaseOptionStmt (SgNode *s)`
 * Cast function (from derived class to SgCaseOptionStmt pointer).
 * This functions returns a SgCaseOptionStmt pointer for any input of a
 * pointer to an object derived from a SgCaseOptionStmt.
 * - Return: Returns valid pointer to SgCaseOptionStmt if input is derived from a SgLocatedNode.
 *
 * #### `SgCaseOptionStmt::isSgCaseOptionStmt (const SgNode *s)`
 * Cast function (from derived class to SgCaseOptionStmt pointer).
 * This functions returns a SgCaseOptionStmt pointer for any input of a
 * pointer to an object derived from a SgCaseOptionStmt.
 * - Return: Returns valid pointer to SgCaseOptionStmt if input is derived from a SgLocatedNode.
 *
 * #### `SgCaseOptionStmt::get_key() const`
 * Returns pointer to a SgExpression object wrapped by SgExpressionRoot in p_key_root.
 * - Internal: This should always be a valid pointer.
 * - Return: Returns pointer to SgExpression.
 *
 * #### `SgCaseOptionStmt::set_key(SgExpression* key)`
 * Access function for pointer to SgExpression object wrapped by SgExpressionRoot in p_key_root.
 * - Return: Returns void.
 *
 * #### `SgCaseOptionStmt::get_key_root() const`
 * Returns pointer to a SgExpressionRoot object.
 * - Internal: This should always be a valid pointer.
 * - Return: Returns pointer to SgExpressionRoot.
 *
 * #### `SgCaseOptionStmt::set_key_root(SgExpressionRoot* key_root)`
 * Access function for p_key_root.
 * - Return: Returns void.
 *
 * #### `SgCaseOptionStmt::get_body() const`
 * Returns pointer to a SgBasicBlock object.
 * - Internal: This should always be a valid pointer.
 * - Return: Returns pointer to SgBasicBlock.
 *
 * #### `SgCaseOptionStmt::set_body(SgBasicBlock* body)`
 * Access function for p_body.
 * - Return: Returns void.
 */
class SgCaseOptionStmt;

/** @brief This class represents a cast of one type to another.
 *
 * SgCastExp is used across multiple languages. For C++ it represents all casts, including C-style casts: (newtype) and C++ style: reinterpret_cast<>(), static_cast<>(), etc.
 * This class represents the concept of a tuple object in the input language. Currently, this IR node only works with Python input files.
 *
 * **Member functions**
 *
 * #### `SgCastExp::isSgCastExp (SgNode *s)`
 * Cast function (from derived class to SgCastExp pointer).
 * This functions returns a SgCastExp pointer for any input of a
 * pointer to an object derived from a SgCastExp.
 * - Return: Returns valid pointer to SgCastExp if input is derived from a SgCastExp.
 *
 * #### `SgCastExp::get_operand ()`
 * Returns the target of the cast.
 * This functions returns an SgExpression pointer representing the expression being casted.
 * - Return: Returns valid pointer to SgExpression.
 */
class SgCastExp;

/** @brief This class represents the concept of a catch within a try-catch construct used in
 *
 * C++ exception handling.
 * - Internal: try and catch, though linked semanticaly, are seperate statements in the IR.
 *
 * **Data members**
 *
 * #### `SgCatchOptionStmt::p_condition`
 * This pointer to a SgVariableDeclaration.
 *
 * #### `SgCatchOptionStmt::p_body`
 * This pointer to a SgBasicBlock used to hold the statements to be executed when the
 * exception is caught.
 *
 * #### `SgCatchOptionStmt::p_trystmt`
 * This pointer to a SgTryStmt (a declaration) that is associated with this catch option.
 *
 * **Member functions**
 *
 * #### `SgCatchOptionStmt::SgCatchOptionStmt ( Sg_File_Info* startOfConstruct = NULL )`
 * This is the constructor.
 * This constructor builds the SgCatchOptionStmt base class.
 * - Param `startOfConstruct`: represents the position in the source code
 *
 * #### `SgCatchOptionStmt::~SgCatchOptionStmt()`
 * This is the destructor.
 * There are a lot of things to delete, but nothing is deleted in this destructor.
 *
 * #### `SgCatchOptionStmt::isSgCatchOptionStmt (SgNode *s)`
 * Cast function (from derived class to SgCatchOptionStmt pointer).
 * This functions returns a SgCatchOptionStmt pointer for any input of a
 * pointer to an object derived from a SgCatchOptionStmt.
 * - Return: Returns valid pointer to SgCatchOptionStmt if input is derived from a SgLocatedNode.
 *
 * #### `SgCatchOptionStmt::isSgCatchOptionStmt (const SgNode *s)`
 * Cast function (from derived class to SgCatchOptionStmt pointer).
 * This functions returns a SgCatchOptionStmt pointer for any input of a
 * pointer to an object derived from a SgCatchOptionStmt.
 * - Return: Returns valid pointer to SgCatchOptionStmt if input is derived from a SgLocatedNode.
 *
 * #### `SgCatchOptionStmt::copy(const SgCopyHelp & help)`
 * Makes a copy (deap of shallow depending on SgCopyHelp).
 * - Return: Returns pointer to copy of SgCatchOptionStmt.
 *
 * #### `SgCatchOptionStmt::get_condition() const`
 * Access function for p_condition.
 * - Return: Returns a pointer to a SgVariableDeclaration.
 *
 * #### `SgCatchOptionStmt::set_condition(SgVariableDeclaration* condition)`
 * Access function for p_condition.
 * - Param `condition`: SgVariableDeclaration pointer
 * - Return: Returns void.
 *
 * #### `SgCatchOptionStmt::get_body() const`
 * Access function for p_body.
 * - Return: Returns a pointer to a SgBasicBlock.
 *
 * #### `SgCatchOptionStmt::set_body(SgBasicBlock* body)`
 * Access function for p_body.
 * - Param `body`: SgBasicBlock pointer
 * - Return: Returns void.
 *
 * #### `SgCatchOptionStmt::get_trystmt() const`
 * Access function for p_trystmt.
 * - Return: Returns a pointer to a SgTryStmt.
 *
 * #### `SgCatchOptionStmt::set_trystmt(SgTryStmt* trystmt)`
 * Access function for p_trystmt.
 * - Param `trystmt`: SgTryStmt pointer
 * - Return: Returns void.
 */
class SgCatchOptionStmt;

/** @brief This class represents the concept of a C++ sequence of catch statements.
 *
 * This is associated with a try statement.
 * - See also:
 * Example of using a SgCatchStatementSeq object
 *
 * **Data members**
 *
 * #### `SgCatchStatementSeq::p_catch_statement_seq`
 * This is an STL list of pointers to SgStatement objects.
 *
 * **Member functions**
 *
 * #### `SgCatchStatementSeq::SgCatchStatementSeq ( Sg_File_Info* startOfConstruct = NULL )`
 * This is the constructor.
 * This constructor builds the SgCatchStatementSeq base class.
 * - Param `startOfConstruct`: represents the position in the source code
 *
 * #### `SgCatchStatementSeq::~SgCatchStatementSeq()`
 * This is the destructor.
 * There are a lot of things to delete, but nothing is deleted in this destructor.
 *
 * #### `SgCatchStatementSeq::isSgCatchStatementSeq (SgNode *s)`
 * Cast function (from derived class to SgCatchStatementSeq pointer).
 * This functions returns a SgCatchStatementSeq pointer for any input of a
 * pointer to an object derived from a SgCatchStatementSeq.
 * - Return: Returns valid pointer to SgCatchStatementSeq if input is derived from a SgLocatedNode.
 *
 * #### `SgCatchStatementSeq::isSgCatchStatementSeq (const SgNode *s)`
 * Cast function (from derived class to SgCatchStatementSeq pointer).
 * This functions returns a SgCatchStatementSeq pointer for any input of a
 * pointer to an object derived from a SgCatchStatementSeq.
 * - Return: Returns valid pointer to SgCatchStatementSeq if input is derived from a SgLocatedNode.
 *
 * #### `SgCatchStatementSeq::get_catch_statement_seq() const`
 * Access function for p_catch_statement_seq.
 * - Return: Returns const reference to SgStatementPtrList.
 *
 * #### `SgCatchStatementSeq::get_catch_statement_seq()`
 * Access function for p_catch_statement_seq.
 * - Return: Returns non-const reference to SgStatementPtrList.
 */
class SgCatchStatementSeq;

/** @brief This class represents the concept of a class declaration statement. It includes the
 *
 * concept of an instantiated class template as well.
 * Declaration statements are where variables and types are
 * defined and become available for use within a program.  Declarations
 * are strored in symbols (SgSymbol) and used to associate
 * the symbol with a location within a scope within the application
 * source code.  There are many different types of declarations, within Sage III
 * the SgDeclaration forms a base class for numerous IR nodes such as:
 * class declarations (including structs and unions), enum declarations,
 * function declarations, namespace declarations, pragma declarations,
 * template declarations, typedef declarations, using declarations,
 * using directives, variable declarations, etc.
 * An important concept for a few types fo declarations is that
 * of defining vs. non-defining.  Defining declarations are associated with
 * definitions, for example a class definition or function definition.
 * A class declaration where a class is defined (instead of a forward
 * declaration) is a defining declaration.  A forward declaration is
 * a non-defining declaration.  Some declarations are not explicitly
 * forward declarations, they are non-defining declarations.  Only
 * declarations where there is a definition are defining declarations.
 * Because definitions can at most appear once within a translation unit
 * (a source file and it included header files), there is at most one
 * defining declaration.  The defining declaration is never shared,
 * however the non-defining declaration can be shared and is required
 * to be shared if there is more than one location in the AST where
 * a non-defining declaration is required.  There is at most one
 * defining and one non-defining declaration (at some point they will
 * also share their name, but this is not implemented yet).  If a variable
 * is defined to be extern or only defined through a refererence or pointer,
 * then there would be no defining declaration (only a non-defining declaration).
 * The following code contains no defining declaration for the
 * type "foobar":
 * ```text
 * typedef struct foobar *((FunctionPointer)(void);
 * foobar* foobar_pointer = NULL;
 * ```
 * and yet is vailid code (even though there is no explicit forward declaration
 * of "foobar" as a class (or struct).  In this case foobar is represented
 * using a non-defining declaration internally and that declaration is
 * reference multiple types (once within each statement).  Note that
 * the code abouve is valid C++, and not valid C code (to make it valid C code
 * we would have to add "struct" the pointer declaration, for example:
 * ```text
 * struct foobar* foobar_pointer = NULL;
 * ```
 * So C++ is not always a super set of C :-).
 * Different types of declarations explained:
 * C and C++ declarations can come in several forms, for example:
 * ```text
 * typedef struct { int A; } A; // unnamed class declaration (which gets a name from the typedef)
 * struct A { int i; };         // autonomous declaration
 * struct B { int i; } b;       // non autonomous declaration
 * ```
 * In these cases "struct" and "class" can be used interchangablily. Internally
 * "struct" and "class" are he same (but with a different enum to classify them,
 * see SgNameType and SgClassType).  Clearly most C++ code uses autonomous
 * declarations.
 * There are no uses of this IR node anywhere.  All statements
 * are derived from this SgClassDeclaration class.
 * This is a base class for all statements.
 * - See also:
 * Example of using a SgClassDeclaration object
 *
 * **Data members**
 *
 * #### `SgClassDeclaration::p_name`
 * This is the name of the class or instantiated class template
 * - Internal: The value held is not shared within the Sage III AST.
 *
 * #### `SgClassDeclaration::p_class_type`
 * Enum value classifying this as a class,struct,or union.
 * - Internal: The enum type is also defined in this class.
 *
 * #### `SgClassDeclaration::p_type`
 * This is the type used in the declaration (SgClassType).
 *
 * #### `SgClassDeclaration::p_definition`
 * This is the class definition (alway a valid pointer, except for explicitly marked
 * forward declarations).
 * When this is a defining or nondefining declartion the p_definition pointer is
 * always valid.  However, as a separate rule, all forward declarations are both
 * makred explicit as forward declarations AND have a NULL value for their p_definition
 * pointer.
 * - Internal: The declaration associated with the definition is the defining declaration and
 * may not be the same as that pointed to by the "this" pointer.
 *
 * #### `SgClassDeclaration::p_from_template`
 * This records if the class declaration is associated with a template.
 * This boolean value is set to true if this is a non-template class nested
 * in a templated class.  We have an explicit IR node for template classes, but
 * this is only flag to indicate that several template specific rules apply to
 * the declaration of this class if the value is true.  For example, the class
 * will not be output unless it is transformed, then the required specialization
 * is required for the parent templated class.
 * - Internal: It is set internally and there is no need for it to be set by the user!
 *
 * **Member functions**
 *
 * #### `SgClassDeclaration::SgClassDeclaration ( Sg_File_Info* startOfConstruct = NULL,`
 * SgName name = SgdefaultName, int class_type = 0, SgClassType *type=NULL,
 * SgClassDefinition *definition=NULL)
 * This is the only constructor.
 * This constructor builds the SgClassDeclaration base class. but has some specific
 * side-effects (some of which have been removed in the latest work).  It is however
 * still that case that if the definition is provided then it's declaration will be changed
 * to the declaration being constructed (a warning message it output if this happens and
 * this side-effect will be removed soon).
 * - Param `startOfConstruct`: represents the position in the source code
 * - See also:
 * Example:create an SgClassDeclaration object
 *
 * #### `SgClassDeclaration::~SgClassDeclaration()`
 * This is the destructor.
 * There are a lot of things to delete, but nothing is deleted in this destructor.
 *
 * #### `SgClassDeclaration::isSgClassDeclaration (SgNode *s)`
 * Cast function (from derived class to SgClassDeclaration pointer).
 * This functions returns a SgClassDeclaration pointer for any input of a
 * pointer to an object derived from a SgClassDeclaration.
 * - Return: Returns valid pointer to SgClassDeclaration if input is derived from a SgLocatedNode.
 *
 * #### `SgClassDeclaration::isSgClassDeclaration (const SgNode *s)`
 * Cast function (from derived class to SgClassDeclaration pointer).
 * This functions returns a SgClassDeclaration pointer for any input of a
 * pointer to an object derived from a SgClassDeclaration.
 * - Return: Returns valid pointer to SgClassDeclaration if input is derived from a SgLocatedNode.
 */
class SgClassDeclaration;

/** @brief This class represents the concept of a class definition in C++.
 *
 * Class definitions are distinquished from class declaration by the
 * definition of the member data and specification of inheritance (if any).
 * Where class declarations can appear multiple times withouth a class definition (forward
 * class declarations), the class definition may appear only once in an application.
 * Within C++ there is the "One Time Definition Rule" which requires that within multiple
 * compilation usints (typically separately compiled files), the definition of all
 * classes of the name qualified name must be identical.  Thus the class is considered to
 * be defined only once.
 * - Internal: This is a base class for the SgTemplateInstantiationDefn statements.
 *
 * **Data members**
 *
 * #### `SgClassDefinition::p_members`
 * This the list of member declarations in the class.
 * - Internal: The order of declarations within this list is unimportant in C++.
 *
 * #### `SgClassDefinition::p_inheritances`
 * This the list of base classes specificed in the class definition.
 * - Internal: The order of base classes can be important.
 *
 * **Member functions**
 *
 * #### `SgClassDefinition::SgClassDefinition ( Sg_File_Info* startOfConstruct = NULL )`
 * This is the constructor.
 * This constructor builds the SgClassDefinition base class.
 * - Param `startOfConstruct`: represents the position in the source code
 *
 * #### `SgClassDefinition::~SgClassDefinition()`
 * This is the destructor.
 * There are a lot of things to delete, but nothing is deleted in this destructor.
 *
 * #### `SgClassDefinition::isSgClassDefinition (SgNode *s)`
 * Cast function (from derived class to SgClassDefinition pointer).
 * This functions returns a SgClassDefinition pointer for any input of a
 * pointer to an object derived from a SgClassDefinition.
 * - Return: Returns valid pointer to SgClassDefinition if input is derived from a SgLocatedNode.
 *
 * #### `SgClassDefinition::isSgClassDefinition (const SgNode *s)`
 * Cast function (from derived class to SgClassDefinition pointer).
 * This functions returns a SgClassDefinition pointer for any input of a
 * pointer to an object derived from a SgClassDefinition.
 * - Return: Returns valid pointer to SgClassDefinition if input is derived from a SgLocatedNode.
 *
 * #### `SgClassDefinition::copy(const SgCopyHelp & help)`
 * Makes a copy (deap of shallow depending on SgCopyHelp).
 * - Return: Returns pointer to copy of SgClassDefinition.
 *
 * #### `SgClassDefinition::get_members()`
 * Returns a list to the data member declarations.
 * - Return: Returns an STL list by reference.
 *
 * #### `SgClassDefinition::get_members() const`
 * Returns a const list to the data member declarations.
 * - Return: Returns a const STL list by reference.
 *
 * #### `SgClassDefinition::get_inheritances()`
 * Returns a list to the base classes.
 * - Return: Returns an STL list by reference.
 *
 * #### `SgClassDefinition::get_inheritances() const`
 * Returns a const list to the base classes.
 * - Return: Returns a const STL list by reference.
 *
 * #### `SgClassDefinition::get_qualified_name() const`
 * Returns SgName (a string) representing the name of the current scope.
 * See discussion of qualified names in the documentation.
 * - Return: Returns SgName (a string).
 *
 * #### `SgClassDefinition::get_mangled_qualified_name() const`
 * Returns SgName (a string) representing the mangled name of the current scope.
 * See discussion of mangled names in the documentation.
 * - Return: Returns SgName (a string).
 *
 * #### `SgClassDefinition::get_declaration()`
 * returns the class declaration associated with this class decinition.
 * - Return: Returns SgClassDeclaration pointer.
 */
class SgClassDefinition;

/** @brief This class represents the concept of a C++ expression built from a class name.
 *
 * This IR node does not appear within any AST's that I have built, but it is
 * part of the EDG/Sage III translation, and I recall that it is part of the
 * support for the AST associated with template declarations (SgTemplateDeclaration)
 * but that is currently a string while we debug the support for template declarations
 * as a more meaningful AST.
 * - Todo: Need to figure out some examples of whare this is used.
 * - See also:
 * Example of using a SgClassNameRefExp object
 *
 * **Data members**
 *
 * #### `SgClassNameRefExp::p_symbol`
 * This pointer points to a SgClassSymbol.
 *
 * **Member functions**
 *
 * #### `SgClassNameRefExp::SgClassNameRefExp ( Sg_File_Info* startOfConstruct = NULL, SgClassSymbol *symbol = NULL )`
 * This is the constructor.
 * This constructor builds the SgClassNameRefExp base class.
 * - Param `startOfConstruct`: represents the position in the source code
 *
 * #### `SgClassNameRefExp::~SgClassNameRefExp()`
 * This is the destructor.
 * There are a lot of things to delete, but nothing is deleted in this destructor.
 *
 * #### `SgClassNameRefExp::isSgClassNameRefExp (SgNode *s)`
 * Cast function (from derived class to SgClassNameRefExp pointer).
 * This functions returns a SgClassNameRefExp pointer for any input of a
 * pointer to an object derived from a SgClassNameRefExp.
 * - Return: Returns valid pointer to SgClassNameRefExp if input is derived from a SgLocatedNode.
 *
 * #### `SgClassNameRefExp::isSgClassNameRefExp (const SgNode *s)`
 * Cast function (from derived class to SgClassNameRefExp pointer).
 * This functions returns a SgClassNameRefExp pointer for any input of a
 * pointer to an object derived from a SgClassNameRefExp.
 * - Return: Returns valid pointer to SgClassNameRefExp if input is derived from a SgLocatedNode.
 *
 * #### `SgClassNameRefExp::get_symbol() const`
 * Returns pointer to SgSymbol.
 * - Return: Returns pointer to SgSymbol.
 *
 * #### `SgClassNameRefExp::set_symbol(SgSymbol* symbol)`
 * Access function for p_symbol.
 * - Return: Returns void.
 */
class SgClassNameRefExp;

/** @brief This class represents the concept of a class name within the compiler.
 *
 * Symbols are a simpler way for the compiler to quickly associate types,
 * declarations and names.
 * Indepedent of the different kinds of declarations (declarations are statements),
 * declarations can be considered to be definind and non-defining.  See
 * SgDeclarationStatement for details.  where there exist defining and
 * non-defining declarations symbols within Sage III always reference non-defining
 * declarations (only non-defining declarations are shared within the AST).
 * - See also: SgDeclarationStatement
 * - See also:
 * Example of using a SgClassSymbol object
 *
 * **Member functions**
 *
 * #### `SgClassSymbol::SgClassSymbol( SgClassDeclaration* declaration = NULL )`
 * This is the only constructor.
 * This constructor builds the SgClassSymbol base class.
 * - See also:
 * Example:create an SgClassSymbol object
 *
 * #### `SgClassSymbol::~SgClassSymbol()`
 * This is the destructor.
 *
 * #### `SgClassSymbol::get_name() const`
 * Access function for getting name from declarations or types internally.
 * - Internal: This is a virtual function.
 * - Return: Returns SgName.
 *
 * #### `SgClassSymbol::get_type() const`
 * This function returns the type associated with the named entity.
 * - Internal: This is a virtual function.
 * - Return: Returns SgType*.
 *
 * #### `SgClassSymbol::isSgClassSymbol (SgNode *s)`
 * Cast function (from derived class to SgClassSymbol pointer).
 * This functions returns a SgClassSymbol pointer for any input of a
 * pointer to an object derived from a SgClassSymbol.
 * - Return: Returns valid pointer to SgClassSymbol if input is derived from a SgClassSymbol.
 *
 * #### `SgClassSymbol::isSgClassSymbol (const SgNode *s)`
 * Cast function (from derived class to SgClassSymbol pointer).
 * This functions returns a SgClassSymbol pointer for any input of a
 * pointer to an object derived from a SgClassSymbol.
 * - Return: Returns valid pointer to SgClassSymbol if input is derived from a SgClassSymbol.
 */
class SgClassSymbol;

/** @brief This class represents the concept of a C style `extern "C"` declaration.  But
 *
 * such information (linkage) is stored within linkage modifiers currently.
 * This class is no longer used, see SgLinkageModifier for more details on
 * how this is handled currently.
 * - Deprecated: This class is not used and will be removed in a later release.
 * - See also:
 * Example of using a SgClinkageStartStatement object
 *
 * **Data members**
 *
 * #### `SgClinkageStartStatement::p_dummyString14`
 * This pointer points to C style character string.
 * - Deprecated: This data member (and the whole class) is not used and will be removed in a later release.
 *
 * **Member functions**
 *
 * #### `SgClinkageStartStatement::SgClinkageStartStatement ( Sg_File_Info* startOfConstruct = NULL )`
 * This is the constructor.
 * This constructor builds the SgClinkageStartStatement base class.
 * - Param `startOfConstruct`: represents the position in the source code
 *
 * #### `SgClinkageStartStatement::~SgClinkageStartStatement()`
 * This is the destructor.
 * There are a lot of things to delete, but nothing is deleted in this destructor.
 *
 * #### `SgClinkageStartStatement::isSgClinkageStartStatement (SgNode *s)`
 * Cast function (from derived class to SgClinkageStartStatement pointer).
 * This functions returns a SgClinkageStartStatement pointer for any input of a
 * pointer to an object derived from a SgClinkageStartStatement.
 * - Return: Returns valid pointer to SgClinkageStartStatement if input is derived from a SgLocatedNode.
 *
 * #### `SgClinkageStartStatement::isSgClinkageStartStatement (const SgNode *s)`
 * Cast function (from derived class to SgClinkageStartStatement pointer).
 * This functions returns a SgClinkageStartStatement pointer for any input of a
 * pointer to an object derived from a SgClinkageStartStatement.
 * - Return: Returns valid pointer to SgClinkageStartStatement if input is derived from a SgLocatedNode.
 *
 * #### `SgClinkageStartStatement::get_dummyString14() const`
 * Returns pointer to a char (C style string).
 * - Deprecated: This access function (and the whole class) is not used and will be removed in a later release.
 * - Return: Returns pointer to char.
 *
 * #### `SgClinkageStartStatement::set_dummyString14(char* dummyString14)`
 * Access function for p_dummyString14 (C style string).
 * - Deprecated: This access function (and the whole class) is not used and will be removed in a later release.
 * - Return: Returns void.
 */
class SgClinkageStartStatement;

/** @brief This class represents the concept of a C trinary conditional expression (e.g. "test ? true : false")
 *
 * - See also:
 * Example of using a SgConditionalExp object
 *
 * **Data members**
 *
 * #### `SgConditionalExp::p_conditional_exp`
 * This pointer points to a SgExpression object.
 *
 * #### `SgConditionalExp::p_true_exp`
 * This pointer points to a SgExpression object.
 *
 * #### `SgConditionalExp::p_false_exp`
 * This pointer points to a SgExpression object.
 *
 * #### `SgConditionalExp::p_expression_type`
 * This pointer points to a SgType object.
 *
 * **Member functions**
 *
 * #### `SgConditionalExp::SgConditionalExp ( Sg_File_Info* startOfConstruct = NULL, SgClassSymbol *symbol = NULL )`
 * This is the constructor.
 * This constructor builds the SgConditionalExp base class.
 * - Param `startOfConstruct`: represents the position in the source code
 *
 * #### `SgConditionalExp::~SgConditionalExp()`
 * This is the destructor.
 * There are a lot of things to delete, but nothing is deleted in this destructor.
 *
 * #### `SgConditionalExp::isSgConditionalExp (SgNode *s)`
 * Cast function (from derived class to SgConditionalExp pointer).
 * This functions returns a SgConditionalExp pointer for any input of a
 * pointer to an object derived from a SgConditionalExp.
 * - Return: Returns valid pointer to SgConditionalExp if input is derived from a SgLocatedNode.
 *
 * #### `SgConditionalExp::isSgConditionalExp (const SgNode *s)`
 * Cast function (from derived class to SgConditionalExp pointer).
 * This functions returns a SgConditionalExp pointer for any input of a
 * pointer to an object derived from a SgConditionalExp.
 * - Return: Returns valid pointer to SgConditionalExp if input is derived from a SgLocatedNode.
 *
 * #### `SgConditionalExp::get_conditional_exp() const`
 * Access function for p_conditional_exp.
 * - Return: Returns pointer to SgExpression.
 *
 * #### `SgConditionalExp::set_conditional_exp(SgExpression* conditional_exp)`
 * Access function for p_conditional_exp.
 * - Param `conditional_exp`: is the conditional test expression
 * - Return: Returns void.
 *
 * #### `SgConditionalExp::get_true_exp() const`
 * Access function for p_true_exp.
 * - Return: Returns pointer to SgExpression.
 *
 * #### `SgConditionalExp::set_true_exp(SgExpression* true_exp)`
 * Access function for p_true_exp.
 * - Param `true_exp`: expression to evaluate if test is true.
 * - Return: Returns void.
 *
 * #### `SgConditionalExp::get_false_exp() const`
 * Access function for p_false_exp.
 * - Return: Returns pointer to SgExpression.
 *
 * #### `SgConditionalExp::set_false_exp(SgExpression* false_exp)`
 * Access function for p_false_exp.
 * - Param `false_exp`: expression to evaluate if test is false.
 * - Return: Returns void.
 *
 * #### `SgConditionalExp::get_expression_type() const`
 * Access function for p_expression_type.
 * - Return: Returns pointer to SgExpression.
 *
 * #### `SgConditionalExp::set_expression_type(SgType* expression_type)`
 * Access function for p_expression_type.
 * Both the p_true_exp and p_false_exp must have the same type.
 * - Param `expression_type`: type of result.
 * - Return: Returns void.
 */
class SgConditionalExp;

/** @brief This class represents the call of a class constructor to initialize a variable.
 *
 * For example "Foo foo;" or "Bar bar(1,2,3);" to initialize foo and bar respecitively.
 * In both cases it holds the function declaration of the call constructor.  It also holds an SgExprListExp that represents the arguments to
 * the constructor.  The list is empty if there are no arguments.
 * - Internal: This is a base class for all initializers.
 * - See also:
 * Example of using a SgConstructorInitializer object
 *
 * **Data members**
 *
 * #### `SgConstructorInitializer::p_declaration`
 * This points to the associated member function declaration (a constructor).
 * Note that this pointer can be NULL if no such constructor is defined (e.g. compiler
 * generated default constructor).
 * - Internal: Constructors not explicit in the source code are not presently generated in ROSE
 * if there were to be generated in the future they will be marked as compiler generated
 * and likely we can then enforce that this be a valid pointer.  This variable is an
 * exception to the general rule that we have no NULL pointes within the AST.
 *
 * #### `SgConstructorInitializer::p_args`
 * This points to the argument list of the associated constructor call.
 * - Internal: Note that this pointer is always a valid pointer, even if the constructor and class
 * declaration pointers are NULL.  This list can also be empty, but the poitner to the
 * list is always valid.
 *
 * #### `SgConstructorInitializer::p_expression_type`
 * This points to the associated type for this constructor.
 * - Internal: This pointer should always be valid,
 * "p_associated_class_unknown" is true iff "p_expression_type" is not a SgClassType.
 *
 * #### `SgConstructorInitializer::p_need_name`
 * This bool value controls the output of the class name.
 * This might be somewhat redundant with the explicit vs. implement representation in the
 * source code (recorded in another flag of the SgExpression base class).
 *
 * #### `SgConstructorInitializer::p_need_qualifier`
 * This bool value controls the output of the class names qualifier.
 * - Internal: The qualifier is currently always output. This variable is not used.
 *
 * #### `SgConstructorInitializer::p_need_parenthesis_after_name`
 * This bool value controls the output of "()" after the class name.
 * The controls the output at "class X x;" or "class X x();".
 *
 * #### `SgConstructorInitializer::p_associated_class_unknown`
 * This bool value indicates when p_declaration should be a valid pointer.
 * If true, then p_declaration is NULL, else if false, then p_declaration should
 * be a valid pointer.
 * - Todo: Enforce p_declaration is valid pointer when p_associated_class_unknown is false
 * within AST consistency tests.
 *
 * **Member functions**
 *
 * #### `SgConstructorInitializer::SgConstructorInitializer ( Sg_File_Info* startOfConstruct = NULL )`
 * This is the constructor.
 * This constructor builds the SgConstructorInitializer base class.
 * - Param `startOfConstruct`: represents the position in the source code
 * - See also:
 * Example:create an SgConstructorInitializer object
 *
 * #### `SgConstructorInitializer::~SgConstructorInitializer()`
 * This is the destructor.
 * Only the Sg_File_Info object can be deleted in this object.
 *
 * #### `SgConstructorInitializer::isSgConstructorInitializer (SgNode *s)`
 * Cast function (from derived class to SgConstructorInitializer pointer).
 * This functions returns a SgConstructorInitializer pointer for any input of a
 * pointer to an object derived from a SgConstructorInitializer.
 * - Return: Returns valid pointer to SgConstructorInitializer if input is derived from a SgConstructorInitializer.
 *
 * #### `SgConstructorInitializer::isSgConstructorInitializer (const SgNode *s)`
 * Cast function (from derived class to SgConstructorInitializer pointer).
 * This functions returns a SgConstructorInitializer pointer for any input of a
 * pointer to an object derived from a SgConstructorInitializer.
 * - Return: Returns valid pointer to SgConstructorInitializer if input is derived from a SgConstructorInitializer.
 *
 * #### `SgConstructorInitializer::get_need_name() const`
 * returns bool value if name is required in code generation then return true.
 * - Return: Returns bool value.
 *
 * #### `SgConstructorInitializer::set_need_name (bool required)`
 * This function allows the p_need_name flag to be set (used internally).
 * - Param `required`: - sets value of internal p_need_name flag.
 * - Return: Returns void.
 *
 * #### `SgConstructorInitializer::set_expression_type (SgType* type)`
 * Access function for p_expression_type.
 * - Param `required`: - sets value of internal p_expression_type.
 * - Return: Returns void.
 *
 * #### `SgConstructorInitializer::get_expression_type() const`
 * Access function for p_expression_type, returns pointer to SgType associated with constructor.
 * This is usually a SgClassType, but it can also be a primative type
 * (typically in association with a new operator).
 * - Return: Returns pointer to SgType.
 */
class SgConstructorInitializer;

/** @brief This class represents the concept of a C or C++ continue statement.
 *
 * - See also:
 * Example of using a SgContinueStmt object
 *
 * **Member functions**
 *
 * #### `SgContinueStmt::SgContinueStmt ( Sg_File_Info* startOfConstruct = NULL )`
 * This is the constructor.
 * This constructor builds the SgContinueStmt base class.
 * - Param `startOfConstruct`: represents the position in the source code
 *
 * #### `SgContinueStmt::~SgContinueStmt()`
 * This is the destructor.
 * There are a lot of things to delete, but nothing is deleted in this destructor.
 *
 * #### `SgContinueStmt::isSgContinueStmt (SgNode *s)`
 * Cast function (from derived class to SgContinueStmt pointer).
 * This functions returns a SgContinueStmt pointer for any input of a
 * pointer to an object derived from a SgContinueStmt.
 * - Return: Returns valid pointer to SgContinueStmt if input is derived from a SgLocatedNode.
 *
 * #### `SgContinueStmt::isSgContinueStmt (const SgNode *s)`
 * Cast function (from derived class to SgContinueStmt pointer).
 * This functions returns a SgContinueStmt pointer for any input of a
 * pointer to an object derived from a SgContinueStmt.
 * - Return: Returns valid pointer to SgContinueStmt if input is derived from a SgLocatedNode.
 */
class SgContinueStmt;

/** @brief This class represents the concept of a contructor initializer list (used in
 *
 * constructor (member function) definitions).
 * Constructors are simply member functions with a name matching the class name (or
 * instantiated template name).  Constructor initialization lists (also called
 * preinitialization lists) are used only within the member function definition
 * (never in the declaration).
 * - Internal: It is presently used in the SgMemberFunctionDeclaration
 * but it might be that it would be better placed into the SgFunctionDefinition.
 * this would at least be similar to the handling of the base class list (stored
 * in the SgClassDefinition and not the SgClassDeclaration).
 * - See also:
 * Example of using a SgCtorInitializerList object
 *
 * **Data members**
 *
 * #### `SgCtorInitializerList::p_ctors`
 * This is an STL list of pointers to SgInitializedName objects.
 *
 * **Member functions**
 *
 * #### `SgCtorInitializerList::SgCtorInitializerList ( Sg_File_Info* startOfConstruct = NULL )`
 * This is the constructor.
 * This constructor builds the SgCtorInitializerList base class.
 * - Param `startOfConstruct`: represents the position in the source code
 *
 * #### `SgCtorInitializerList::~SgCtorInitializerList()`
 * This is the destructor.
 * There are a lot of things to delete, but nothing is deleted in this destructor.
 *
 * #### `SgCtorInitializerList::isSgCtorInitializerList (SgNode *s)`
 * Cast function (from derived class to SgCtorInitializerList pointer).
 * This functions returns a SgCtorInitializerList pointer for any input of a
 * pointer to an object derived from a SgCtorInitializerList.
 * - Return: Returns valid pointer to SgCtorInitializerList if input is derived from a SgLocatedNode.
 *
 * #### `SgCtorInitializerList::isSgCtorInitializerList (const SgNode *s)`
 * Cast function (from derived class to SgCtorInitializerList pointer).
 * This functions returns a SgCtorInitializerList pointer for any input of a
 * pointer to an object derived from a SgCtorInitializerList.
 * - Return: Returns valid pointer to SgCtorInitializerList if input is derived from a SgLocatedNode.
 *
 * #### `SgCtorInitializerList::get_ctors()`
 * Returns STL list of pointers to SgInitializedName objects.
 * - Return: Returns SgInitializedNamePtrList (STL list) by reference.
 */
class SgCtorInitializerList;

/** @brief This class represents modifiers for SgDeclaration (declaration statements).
 *
 * - See also:
 * Example of using a SgDeclarationModifier object
 *
 * **Data members**
 *
 * #### `SgBitVector 	SgDeclarationModifier::p_modifierVector`
 * Bit vector permitting specification of flags for friend, typedef, export, throw.
 *
 * #### `SgTypeModifier 	SgDeclarationModifier::p_typeModifier`
 * Modifier for type information.
 *
 * #### `SgAccessModifier 	SgDeclarationModifier::p_accessModifier`
 * Modifier for access information (only set for declarations in class and struct definitions)
 *
 * #### `SgStorageModifier 	SgDeclarationModifier::p_storageModifier`
 * Modified for storage information.
 *
 * **Member functions**
 *
 * #### `SgDeclarationModifier::SgDeclarationModifier()`
 * This is the constructor.
 * This constructor builds the SgDeclarationModifier base class.
 * - See also:
 * Example:create an SgDeclarationModifier object
 *
 * #### `SgDeclarationModifier::~SgDeclarationModifier()`
 * This is the destructor.
 * There is nothing to delete in this object.
 *
 * #### `SgDeclarationModifier::operator=(const SgDeclarationModifier & X)`
 * This is the assignment operator.
 * This is a simple assignment of the SgBitVector from X to the current object.
 *
 * #### `SgDeclarationModifier::isSgDeclarationModifier (SgNode *s)`
 * Cast function (from derived class to SgDeclarationModifier pointer).
 * This functions returns a SgDeclarationModifier pointer for any input of a
 * pointer to an object derived from a SgDeclarationModifier.
 * - Return: Returns valid pointer to SgDeclarationModifier if input is derived from a SgLocatedNode.
 *
 * #### `SgDeclarationModifier::isSgDeclarationModifier (const SgNode *s)`
 * Cast function (from derived class to SgDeclarationModifier pointer).
 * This functions returns a SgDeclarationModifier pointer for any input of a
 * pointer to an object derived from a SgDeclarationModifier.
 * - Return: Returns valid pointer to SgDeclarationModifier if input is derived from a SgLocatedNode.
 *
 * #### `SgDeclarationModifier::isUnknown () const`
 * Declaration modifier is unknown (not set).
 * - Return: Returns bool.
 *
 * #### `SgDeclarationModifier::setUnknown ()`
 * Set bit flag.
 * - Return: Returns void.
 *
 * #### `SgDeclarationModifier::unsetUnknown ()`
 * Clear (unset) bit flag.
 * - Return: Returns void.
 *
 * #### `SgDeclarationModifier::isDefault () const`
 * Declaration modifier is default (default setting).
 * - Return: Returns bool.
 *
 * #### `SgDeclarationModifier::setDefault ()`
 * Set bit flag.
 * - Return: Returns void.
 *
 * #### `SgDeclarationModifier::unsetDefault ()`
 * Clear (unset) bit flag.
 * - Return: Returns void.
 *
 * #### `SgDeclarationModifier::isFriend () const`
 * declaration modifier is friend.
 * - Return: Returns bool.
 *
 * #### `SgDeclarationModifier::setFriend ()`
 * Set bit flag.
 * - Return: Returns void.
 *
 * #### `SgDeclarationModifier::unsetFriend ()`
 * Clear (unset) bit flag.
 * - Return: Returns void.
 *
 * #### `SgDeclarationModifier::isTypedef () const`
 * Declaration modifier is a typedef.
 * - Return: Returns bool.
 *
 * #### `SgDeclarationModifier::setTypedef ()`
 * Set bit flag.
 * - Return: Returns void.
 *
 * #### `SgDeclarationModifier::unsetTypedef ()`
 * Clear (unset) bit flag.
 * - Return: Returns void.
 *
 * #### `SgDeclarationModifier::isExport () const`
 * Declaration modifier is export.
 * - Return: Returns bool.
 *
 * #### `SgDeclarationModifier::setExport ()`
 * Set bit flag.
 * - Return: Returns void.
 *
 * #### `SgDeclarationModifier::unsetExport ()`
 * Clear (unset) bit flag.
 * - Return: Returns void.
 *
 * #### `SgDeclarationModifier::isThrow () const`
 * Declaration modifier is throw.
 * - Return: Returns bool.
 *
 * #### `SgDeclarationModifier::setThrow ()`
 * Set bit flag.
 * - Return: Returns void.
 *
 * #### `SgDeclarationModifier::unsetThrow ()`
 * Clear (unset) bit flag.
 * - Return: Returns void.
 */
class SgDeclarationModifier;

/** @brief This class represents the concept of a declaration statement.
 *
 * Declaration statements are where variables and types are
 * defined and become available for use within a program.  Declarations
 * are strored in symbols (SgSymbol) and used to associate
 * the symbol with a location within a scope within the application
 * source code.  There are many different types of declarations, within Sage III
 * the SgDeclaration forms a base class for numerous IR nodes such as:
 * class declarations (including structs and unions), enum declarations,
 * function declarations, namespace declarations, pragma declarations,
 * template declarations, typedef declarations, using declarations,
 * using directives, variable declarations, etc.
 * An important concept for a few types fo declarations is that
 * of defining vs. non-defining.  Defining declarations are associated with
 * definitions, for example a class definition or function definition.
 * A class declaration where a class is defined (instead of a forward
 * declaration) is a defining declaration.  A forward declaration is
 * a non-defining declaration.  Some declarations are not explicitly
 * forward declarations, they are non-defining declarations.  Only
 * declarations where there is a definition are defining declarations.
 * Because definitions can at most appear once within a translation unit
 * (a source file and it included header files), there is at most one
 * defining declaration.  The defining declaration is never shared,
 * however the non-defining declaration can be shared and is required
 * to be shared if there is more than one location in the AST where
 * a non-defining declaration is required.  There is at most one
 * defining and one non-defining declaration (at some point they will
 * also share their name, but this is not implemented yet).  If a variable
 * is defined to be extern or only defined through a refererence or pointer,
 * then there would be no defining declaration (only a non-defining declaration).
 * The following code contains no defining declaration for the
 * type "foobar":
 * ```text
 * typedef struct foobar *((FunctionPointer)(void);
 * foobar* foobar_pointer = NULL;
 * ```
 * and yet is valid code (even though there is no explicit forward declaration
 * of "foobar" as a class (or struct).  In this case foobar is represented
 * using a non-defining declaration internally and that declaration is
 * reference multiple types (once within each statement).  Note that
 * the code above is valid C++, and not valid C code (to make it valid C code
 * we would have to add "struct" the pointer declaration, for example:
 * ```text
 * struct foobar* foobar_pointer = NULL;
 * ```
 * So C++ is not always a super set of C :-).
 * Different types of declarations explained:
 * C and C++ declarations can come in several forms, for example:
 * ```text
 * typedef struct { int A; } A; // unnamed class declaration (which gets a name from the typedef)
 * struct A { int i; };         // autonomous declaration
 * struct B { int i; } b;       // non autonomous declaration
 * ```
 * In these cases "struct" and "class" can be used interchangablily. Internally
 * "struct" and "class" are he same (but with a different enum to classify them,
 * see SgNameType and SgClassType).  Clearly most C++ code uses autonomous
 * declarations.
 * There are no uses of this IR node anywhere.  All statements
 * are derived from this SgDeclarationStatement class.
 * - Internal: This is a base class for all statements.
 * - Todo: Current issues to look at:
 * - Current classes derived from SgDeclarationStatement and the
 * SgInitializedName class have ways of generating
 * names, qualified names, and mangled names, but we lack a uniform mechanism to
 * generate mangled qualified names.  The implementation is not uniform within
 * the IR and this is a problem for applications which must generate unique names
 * for C++ constructs.  Specific to functions names:
 * - We have:
 * -# get_name()
 * -# get_qualified_name()
 * -# get_mangled_name()
 * - While we need (on ALL SgDeclarationStatement IR nodes):
 * -# get_name()
 * -# get_qualified_name()
 * -# get_mangled_name()
 * -# get_qualified_mangled_name()
 * - It has been suggested that we eliminate get_qualified_mangled_name() and have
 * get_mangled_name() return a mangled qualified name.  Thus the semantics of
 * get_mangled_name() would be changed to generate a mangled qualified name.
 * The purpose of get_mangled_name is to generate a unique name for all declarations.
 * Note that some declarations should not return a unique name:
 * -# SgAsmStmt
 * -# SgCtorInitializerList
 * -# SgFunctionParameterList
 * -# SgUsingDeclarationStatement
 * -# SgVariableDefinition \\
 * It may be that this should be the same as for the SgVariableDeclaration.
 * - Related to the above problem we wish to provide a definition under which all
 * declarations (SgDeclarationStatement IR nodes) can be defined to be unique.
 * Under One-time Definition Rule (ODR) definitions must be the same within
 * a linked application (in each compilation unit).  We define a unique name
 * for a declaration IR node as:
 * - Any declaration which matches another declaration under ODR must generate
 * the same unique string.
 * - Any two declarations that are different must generate different unique strings.
 * - Important cases are:
 * - The IR nodes for which there is an obvious mangled name (and a get_mangled_name()
 * exists) are:
 * - SgClassDeclaration
 * - SgFunctionDeclaration
 * - SgEnumDeclaration
 * - SgNamespaceDeclarationStatement
 * - SgTypedefDeclaration
 * - IR nodes for which it is less obvious (or for which there is no present
 * get_mangled_name() member function) are:
 * - SgAsmStmt
 * - SgCtorInitializerList
 * - SgFunctionParameterList
 * - SgNamespaceAliasDeclarationStatement
 * - SgPragmaDeclaration
 * - SgTemplateDeclaration (can this have a mangled name?)
 * - SgTemplateInstantiationDirectiveStatement
 * - SgUsingDeclarationStatement
 * - SgUsingDirectiveStatement
 * - SgVariableDeclaration
 * - SgVariableDefinition
 * - Other IR nodes (not derived from SgDeclarationStatement) but for which we need to
 * generate mangled names (and a get_mangled_name() member functions exists) are:
 * - SgTemplateArgument
 * - SgFunctionDefinition
 * - SgNamespaceDefinitionStatement
 * - SgClassDefinition
 * - SgType (and all IR nodes derived from SgType)
 * - Other IR nodes (not derived from SgDeclarationStatement) but for which I think we
 * need to generate mangled names (and a get_mangled_name() member functions does NOT
 * exist) are:
 * - SgInitializedName
 * - SgFile (this might not really be required, but it might make sense)
 * - SgDirectory (this might not really be required, but it might make sense)
 * - SgBaseClass (this is questionable)
 * - SgQualifiedName (this is questionable)
 * - Class declarations often point to their definition independently of if they are
 * defining or non-defining declaration (true at least for SgTemplateInstantiationDecl
 * objects).  This is different from how function declarations are handled, where only
 * the defining declaration has a valid pointer to the function definition.  This should
 * be made more consistant.  There are more comments about this is the documentation for
 * SgTemplateInstantiationDecl.
 * - The specification of "extern" is not the same as extern "C", one is handled in the
 * SgDeclarationStatement (extern "C") and the other is handled in the SgStorageModifier
 * (extern).  It is confusing that the two are so far a part.  Perhaps they could be
 * together so that setting "C" linkage could declare a SgDeclarationStatement as
 * "extern" at the same time.  It makes no sense to have something marked as "C" linkage
 * and have it not be extern (I think); so this could be considered an error (I think).
 * - To support debugging of applications using the mangled names to build unique strings
 * we should provide a global map between the unique names and the associated
 * declarations.  This would avoid the creation of alternative name mangling mechanisms
 * whose sole purpose it to fold into the mangled name debugging information.
 * - Test: To test the mangled name generation we should construct a program using the mangled
 * names and compile it to verify uniquenss of the veriable names.  Since functions
 * of the same name can be repeated, mangled names from functions should generate
 * functions in the test program and mangled names of veriables should generate
 * variables, etc.
 * - See also:
 * Example of using a SgDeclarationStatement object
 *
 * **Data members**
 *
 * #### `SgDeclarationStatement::p_definingDeclaration`
 * This pointer is valid only if a defining declaration exists, and stores the defining declaration
 * where the declaration was defined.
 * It is important to note that this is the only defining declaration as
 * opposed to the non-defining declarations (where there can, depending on IR node, often be many).
 * - Internal: The value held is not shared within the Sage III AST (except though this data
 * member in each defining and non-defining declaration).
 *
 * #### `SgDeclarationStatement::p_firstNondefiningDeclaration`
 * This pointer is valid if there is a non-defining declaration, and stores the first non-defining declaration
 * where a declaration was made explicitly (as in forward declaration) or implicitly as in
 * the case of a few IR nodes (e.g. SgClassDeclaration). See description of SgDeclarationStatement.
 * It is important to note that this is the first non-defining declaration as
 * opposed to the defining declaration.  Thus it is usually the first forward declaration.
 * - Internal: The value held is sharable across many references where as the defining
 * declaration (not held by this variable) should have one unique reference
 * within the AST, except where reference though the declarations
 * definingDeclaration data member pointer.  See declarations for details
 * (SgDeclarationStatements).
 *
 * #### `SgDeclarationStatement::p_linkage`
 * This string stores the value "C", "C++", or "fortran" to define the external
 * linkage for generating linkable names.
 * The only values that make sense here are "C", "C++" is the assumed default and requires
 * a NULL pointer value.  And the use of "fortran" would be backend specific (may be
 * rejected by some backend vendor compilers).
 *
 * #### `SgDeclarationStatement::p_decl_attributes`
 * This is old and used to be used to hold modifiers.
 * - Deprecated: This is part of an older modifier interface that was not fully implemented.
 * - Internal: This data member will be removed at some point.
 *
 * #### `SgDeclarationStatement::p_declarationModifier`
 * This is the mechanism to handle declaration modifiers.
 * See Stroustrup 3rd edition, Appendix A, for details of how modifiers are represented
 * in ROSE.
 * - Internal: There is no interface to declaration modifiers implemented directly in this
 * SgDeclarationStatement class.  This was done to simplify the implementation.  Users
 * are expected to access the SgDeclarationmodifier object (returned by reference)
 * directly to get/set its values.
 *
 * #### `SgDeclarationStatement::p_nameOnly`
 * I think this is somewhat redundant with the purpose of specifying a forward
 * declaration.  Either that or it is information used by the unparser (Oh, I hope not!).
 * - Deprecated: This will likely be removed at some point.
 * - Internal: This mechanism is now redundnat with the better support for defining/nondefining
 * declarations.
 *
 * #### `SgDeclarationStatement::p_forward`
 * This marks a declaration as being an explicit forward declaration.
 * All non-defining declaations are marked as forward declarations, while
 * a defining declaration is NOT a marked as a forward declaration.
 * - Internal: It is not clear enough to just use defining vs. nondefining declarations
 * to know when something is a forward declaration!
 *
 * #### `SgDeclarationStatement::p_externBrace`
 * This marks that the declaration appeared with "extern".
 * Note that I think this is also set even if it appears between "extern {" and "}".
 * It might be that this is a way to specify the first "extern {", but I don't think
 * it is used interanlly and it is not clear if attaching the "extern {" to
 * a declaration is a great way to handle this problem generally (appears to be
 * overlly sensative to transformations that would add, remove, or move declarations
 * (e.g in inlining and outlining transformations).
 * Currently the lex pass saves all "extern \"C\" {" declarations and treats them as
 * CPP directive (along with comments), they are then woven back into the AST as
 * a separate pass.
 * - Deprecated: This is now redundant with the seperate preprocessing phase.
 * - Internal: Not sure that we need or want this mechanism (or this implementation of it).
 * Not that the mechanism that we have is better and based on the scanning for extern "X"
 * in the source code directly (where "X" is either "C", "C++", or "fortran".
 *
 * #### `SgDeclarationStatement::p_skipElaborateType`
 * This marks "type elaboration" which is the use of the keyword "class" before
 * variables of type class.
 * This is not often required, but is routinely output by the unparser to be
 * conservative.  The support for hidden lists allows the unparser to more
 * precisely output the type elaboration, and so this variable is redundant.
 * - Deprecated: This is now redundant with the hidden list analysis done to support more
 * accurate code generation.
 * - Internal: This field is used in a default mode and could be set to be more accurate.
 *
 * #### `SgDeclarationStatement::p_need_name_qualifier`
 * This marks a declaration are requiring qualification in its output in the unparser.
 * - Deprecated: This is now redundant with the hidden list analysis done to support more
 * accurate code generation.
 * - Internal: Although this variable is specific to unparing, I think it may be required.
 * Although the name qualifications were recently reimplemented to be more uniform and robust
 * I'm not clear if this variable is a part of that new implementation (and thus required).
 *
 * **Member functions**
 *
 * #### `SgDeclarationStatement::SgDeclarationStatement ( Sg_File_Info* startOfConstruct = NULL )`
 * This is the constructor.
 * This constructor builds the SgDeclarationStatement base class.
 * - Param `startOfConstruct`: represents the position in the source code
 *
 * #### `SgDeclarationStatement::~SgDeclarationStatement()`
 * This is the destructor.
 * There are a lot of things to delete, but nothing is deleted in this destructor.
 *
 * #### `SgDeclarationStatement::isSgDeclarationStatement (SgNode *s)`
 * Cast function (from derived class to SgDeclarationStatement pointer).
 * This functions returns a SgDeclarationStatement pointer for any input of a
 * pointer to an object derived from a SgDeclarationStatement.
 * - Return: Returns valid pointer to SgDeclarationStatement if input is derived from a SgLocatedNode.
 *
 * #### `SgDeclarationStatement::isSgDeclarationStatement (const SgNode *s)`
 * Cast function (from derived class to SgDeclarationStatement pointer).
 * This functions returns a SgDeclarationStatement pointer for any input of a
 * pointer to an object derived from a SgDeclarationStatement.
 * - Return: Returns valid pointer to SgDeclarationStatement if input is derived from a SgLocatedNode.
 *
 * #### `SgDeclarationStatement::get_declarationModifier()`
 * Returns SgDeclarationModifier object by reference (so that it can be set).
 * - Return: Returns SgDeclarationModifier by reference.
 *
 * #### `SgDeclarationStatement::isForward() const`
 * Returns boolean value indicating if this is a forward declaration.
 * - Return: Returns bool.
 *
 * #### `SgDeclarationStatement::setForward()`
 * Marks a declaration as a forward declaration.
 * - Return: Returns void.
 *
 * #### `SgDeclarationStatement::unsetForward()`
 * Marks a forward declaration as a non-forward declaration.
 * - Return: Returns void.
 *
 * #### `SgDeclarationStatement::get_definingDeclaration() const`
 * This is an access function for the SgDeclarationStatement::p_definingDeclaration data member (see
 * that variable's documentation for details).
 *
 * #### `SgDeclarationStatement::set_definingDeclaration( SgDeclarationStatement* definingDeclaration )`
 * This is an access function for the SgDeclarationStatement::p_definingDeclaration data member (see
 * that variable's documentation for details).
 *
 * #### `SgDeclarationStatement::get_firstNondefiningDeclaration() const`
 * This is an access function for the SgDeclarationStatement::p_firstNondefiningDeclaration data member (see
 * that variable's documentation for details).
 *
 * #### `SgDeclarationStatement::set_firstNondefiningDeclaration( SgDeclarationStatement* firstNondefiningDeclaration )`
 * This is an access function for the SgDeclarationStatement::p_firstNondefiningDeclaration data member (see
 * that variable's documentation for details).
 *
 * #### `SgDeclarationStatement::get_symbol_from_symbol_table() const`
 * **FOR** **INTERNAL** **USE** Get the associated symbol from the symbol table in the stored scope.
 * Users should use the SgDeclarationStatement::search_for_symbol_from_symbol_table() instead.
 *
 * #### `SgDeclarationStatement::search_for_symbol_from_symbol_table() const`
 * User interface for retrieving the associated symbol from the declaration.
 * - Return: Returns SgSymbol pointer.
 *
 * #### `SgDeclarationStatement::hasAssociatedSymbol() const`
 * Returns boolean value true of this type of declaration has an associated sysmbol.
 * - Return: Returns bool.
 * - Internal: Only a few types of declarations do not have associated symbols
 * (SgFunctionParameterList, SgCtorInitializerList, SgPragmaDeclaration,
 * SgVariableDeclaration, SgVariableDefinition).
 */
class SgDeclarationStatement;

/** @brief This class represents the concept of a C or C++ default case within a switch
 *
 * statement.
 * The default case in a switch statement is optional within C and C++, it is not
 * currently a part of ROSE AST normalization to build one for each switch, though this might
 * be done at some point (in which case it will be marked as compiler-generated).  At
 * present any statement associated with the defalut case is placed into a compiler generated
 * SgBasicBlock, a current AST normalization.  The ROSE User Manual for details of minor AST
 * normalizations.
 * - See also:
 * Example of using a SgDefaultOptionStmt object
 *
 * **Data members**
 *
 * #### `SgDefaultOptionStmt::p_body`
 * This pointer points to SgBasicBlock holding the statements executed for the default
 * case of a switch statement.
 * - Internal: This should always be a vailid pointer (a current AST normalization).  The list
 * can however be empty.
 *
 * **Member functions**
 *
 * #### `SgDefaultOptionStmt::SgDefaultOptionStmt ( Sg_File_Info* startOfConstruct = NULL )`
 * This is the constructor.
 * This constructor builds the SgDefaultOptionStmt base class.
 * - Param `startOfConstruct`: represents the position in the source code
 *
 * #### `SgDefaultOptionStmt::~SgDefaultOptionStmt()`
 * This is the destructor.
 * There are a lot of things to delete, but nothing is deleted in this destructor.
 *
 * #### `SgDefaultOptionStmt::isSgDefaultOptionStmt (SgNode *s)`
 * Cast function (from derived class to SgDefaultOptionStmt pointer).
 * This functions returns a SgDefaultOptionStmt pointer for any input of a
 * pointer to an object derived from a SgDefaultOptionStmt.
 * - Return: Returns valid pointer to SgDefaultOptionStmt if input is derived from a SgLocatedNode.
 *
 * #### `SgDefaultOptionStmt::isSgDefaultOptionStmt (const SgNode *s)`
 * Cast function (from derived class to SgDefaultOptionStmt pointer).
 * This functions returns a SgDefaultOptionStmt pointer for any input of a
 * pointer to an object derived from a SgDefaultOptionStmt.
 * - Return: Returns valid pointer to SgDefaultOptionStmt if input is derived from a SgLocatedNode.
 *
 * #### `SgDefaultOptionStmt::get_body() const`
 * Returns pointer to SgBasicBlock.
 * - Return: Returns pointer to SgBasicBlock.
 *
 * #### `SgDefaultOptionStmt::set_body(SgBasicBlock* body)`
 * Access function for p_body.
 * - Return: Returns void.
 */
class SgDefaultOptionStmt;

/** @brief This class represents the concept of a C++ call to the delete operator.
 *
 * - Todo: I believe we can associate the destructors from the class with
 * delete operators.
 * - See also:
 * Example of using a SgDeleteExp object
 *
 * **Data members**
 *
 * #### `SgDeleteExp::p_variable`
 * This pointer points to the variable being deleted.
 *
 * #### `SgDeleteExp::p_is_array`
 * This bool value is true only if the delete operator is called on an array (array delete).
 *
 * #### `SgDeleteExp::p_need_global_specifier`
 * This delete operator needs to be output with "::".
 * - Todo: Check if this bool data member is used and/or required.
 * - Internal: Not clear if this is required since Sage III should have the scope sufficent to output
 * any required name qualification.  More generally we should be able to eliminate this
 * once we explicitly store all explicit qualified names.
 *
 * **Member functions**
 *
 * #### `SgDeleteExp::SgDeleteExp ( Sg_File_Info* startOfConstruct = NULL, SgClassSymbol *symbol = NULL )`
 * This is the constructor.
 * This constructor builds the SgDeleteExp base class.
 * - Param `startOfConstruct`: represents the position in the source code
 *
 * #### `SgDeleteExp::~SgDeleteExp()`
 * This is the destructor.
 * There are a lot of things to delete, but nothing is deleted in this destructor.
 *
 * #### `SgDeleteExp::isSgDeleteExp (SgNode *s)`
 * Cast function (from derived class to SgDeleteExp pointer).
 * This functions returns a SgDeleteExp pointer for any input of a
 * pointer to an object derived from a SgDeleteExp.
 * - Return: Returns valid pointer to SgDeleteExp if input is derived from a SgLocatedNode.
 *
 * #### `SgDeleteExp::isSgDeleteExp (const SgNode *s)`
 * Cast function (from derived class to SgDeleteExp pointer).
 * This functions returns a SgDeleteExp pointer for any input of a
 * pointer to an object derived from a SgDeleteExp.
 * - Return: Returns valid pointer to SgDeleteExp if input is derived from a SgLocatedNode.
 *
 * #### `SgDeleteExp::get_variable() const`
 * Access function for p_variable.
 * - Return: Returns pointer to SgExpression.
 *
 * #### `SgDeleteExp::set_variable(SgExpression* variable)`
 * Access function for p_variable.
 * - Param `variable`: is the variable to be deleted.
 * - Return: Returns void.
 *
 * #### `SgDeleteExp::get_is_array() const`
 * Access function for p_is_array.
 * - Return: Returns bool.
 *
 * #### `SgDeleteExp::set_is_array(SgExpression* is_array)`
 * Access function for p_is_array.
 * - Param `is_array`: flag to record use of array delete.
 * - Return: Returns void.
 *
 * #### `SgDeleteExp::get_need_global_specifier() const`
 * Access function for p_need_global_specifier.
 * - Return: Returns bool.
 *
 * #### `SgDeleteExp::set_need_global_specifier(SgExpression* need_global_specifier)`
 * Access function for p_need_global_specifier.
 * - Param `need_global_specifier`: flag to record use of array delete.
 * - Return: Returns void.
 */
class SgDeleteExp;

/** @brief This class represents a directory within a projects file structure of files and directories.
 *
 * This IR node is intended to allow the AST at the SgProject level to have the same
 * representation as a project being compiled using ROSE.
 * - Internal: This is not yet used in ROSE and has been added as an experiment to support very
 * large ASTs for a whole project. It is expected a a million lines of code will require
 * a few Gig of storage, but memory is cheap, so we will see what we can accomplish.
 * Note that it is a limitation of ROSETTA that we have to have the file list and the
 * directory list be pointers to separate (new) IR nodes that contain the actually STL
 * lists of files and directories.  Note clear if this limitation in ROSETTA is
 * documented anywhere :-).
 * - Todo: Consider adding a SgFileInfo data member to be uniform with SgFile.
 * - See also:
 * Example of using a SgDirectory object
 *
 * **Member functions**
 *
 * #### `SgDirectory::SgDirectory()`
 * This is the constructor.
 * This constructor builds the SgDirectory base class.
 * - See also:
 * Example:create an SgDirectory object
 *
 * #### `SgDirectory::~SgDirectory()`
 * This is the destructor.
 * There is nothing to delete in this object.
 *
 * #### `SgDirectory::isSgDirectory (SgNode *s)`
 * Cast function (from derived class to SgDirectory pointer).
 * This functions returns a SgDirectory pointer for any input of a
 * pointer to an object derived from a SgDirectory.
 * - Return: Returns valid pointer to SgDirectory if input is derived from a SgLocatedNode.
 *
 * #### `SgDirectory::isSgDirectory (const SgNode *s)`
 * Cast function (from derived class to SgDirectory pointer).
 * This functions returns a SgDirectory pointer for any input of a
 * pointer to an object derived from a SgDirectory.
 * - Return: Returns valid pointer to SgDirectory if input is derived from a SgLocatedNode.
 */
class SgDirectory;

/** @brief This class represents the concept of a do-while statement.
 *
 * - Internal:
 * - Todo: The conditional test should be an expression (different from a SgWhileStmt),
 * see test2005_114.C for examples and details.
 *
 * **Data members**
 *
 * #### `SgDoWhileStmt::p_condition`
 * This pointer a SgStatement, the conditional expression in the loop construct.
 * - Todo: This should be changed to be a SgExpression (to follow the C++ standard).
 * We also have the condition specified before the body within the ROSETTA
 * specification and this causes the traversal to travers the condition and body in the
 * wrong order (for do-while, the traversal should be body forst and condition second).
 * See test2005_114.C for more details and example code (example of strange loops).
 *
 * #### `SgDoWhileStmt::p_body`
 * This pointer a SgBasicBlock, and holds the statements in the body of the loop.
 *
 * **Member functions**
 *
 * #### `SgDoWhileStmt::SgDoWhileStmt ( Sg_File_Info* startOfConstruct = NULL )`
 * This is the constructor.
 * This constructor builds the SgDoWhileStmt base class.
 * - Param `startOfConstruct`: represents the position in the source code
 *
 * #### `SgDoWhileStmt::~SgDoWhileStmt()`
 * This is the destructor.
 * There are a lot of things to delete, but nothing is deleted in this destructor.
 *
 * #### `SgDoWhileStmt::isSgDoWhileStmt (SgNode *s)`
 * Cast function (from derived class to SgDoWhileStmt pointer).
 * This functions returns a SgDoWhileStmt pointer for any input of a
 * pointer to an object derived from a SgDoWhileStmt.
 * - Return: Returns valid pointer to SgDoWhileStmt if input is derived from a SgLocatedNode.
 *
 * #### `SgDoWhileStmt::isSgDoWhileStmt (const SgNode *s)`
 * Cast function (from derived class to SgDoWhileStmt pointer).
 * This functions returns a SgDoWhileStmt pointer for any input of a
 * pointer to an object derived from a SgDoWhileStmt.
 * - Return: Returns valid pointer to SgDoWhileStmt if input is derived from a SgLocatedNode.
 *
 * #### `SgDoWhileStmt::copy(const SgCopyHelp & help)`
 * Makes a copy (deap of shallow depending on SgCopyHelp).
 * - Return: Returns pointer to copy of SgDoWhileStmt.
 *
 * #### `SgDoWhileStmt::get_body() const`
 * Access function for p_body.
 * - Return: Returns a pointer to a SgBasicBlock.
 *
 * #### `SgDoWhileStmt::set_body(SgBasicBlock* body)`
 * Access function for p_body.
 * - Param `body`: SgBasicBlock pointer
 * - Return: Returns void.
 *
 * #### `SgDoWhileStmt::get_condition() const`
 * Access function for p_condition.
 * - Return: Returns a pointer to a SgStatement.
 *
 * #### `SgDoWhileStmt::set_condition(SgStatement* condition)`
 * Access function for p_condition.
 * - Param `condition`: SgStatement pointer
 * - Return: Returns void.
 */
class SgDoWhileStmt;

/** @brief This class represents the notion of an value (expression value).
 *
 * - Internal: This class will hold a string value so that the exact text for constants can be
 * help in the AST.
 * - Todo: Add string to this class so that the exact value can be held in the AST.
 * - See also:
 * Example of using a SgDoubleVal object
 *
 * **Data members**
 *
 * #### `SgDoubleVal::p_value`
 * This value holds the double represented in the source code.
 *
 * **Member functions**
 *
 * #### `SgDoubleVal::SgDoubleVal ( Sg_File_Info* startOfConstruct = NULL )`
 * This is the constructor.
 * This constructor builds the SgDoubleVal base class.
 * - Param `startOfConstruct`: represents the position in the source code
 * - See also:
 * Example:create an SgDoubleVal object
 *
 * #### `SgDoubleVal::~SgDoubleVal()`
 * This is the destructor.
 * Only the Sg_File_Info object can be deleted in this object.
 */
class SgDoubleVal;

/** @brief This class represents the concept of an enum declaration.
 *
 * - See also:
 * Example of using a SgEnumDeclaration object
 *
 * **Data members**
 *
 * #### `SgEnumDeclaration::p_name`
 * Name of enum type (empty if not named).
 *
 * #### `SgEnumDeclaration::p_embedded`
 * Boolean value true if embedded in a typedef declaration (might not be used).
 *
 * #### `SgEnumDeclaration::p_type`
 * SgEnumType generated by this enum declaration.
 *
 * #### `SgEnumDeclaration::p_enumerators`
 * STL list of pointers to SgInitializedName object (used for enum variables).
 *
 * #### `SgEnumDeclaration::p_scope`
 * Scope of enum declaration.
 * Note that the scope of the enum must be stored explicitly since it can be declared in
 * a namespace and defined outside the namespace (note that forward declarations of
 * enum types.
 *
 * **Member functions**
 *
 * #### `SgEnumDeclaration::SgEnumDeclaration ( Sg_File_Info* startOfConstruct = NULL )`
 * This is the constructor.
 * This constructor builds the SgEnumDeclaration base class.
 * - Param `startOfConstruct`: represents the position in the source code
 *
 * #### `SgEnumDeclaration::~SgEnumDeclaration()`
 * This is the destructor.
 * There are a lot of things to delete, but nothing is deleted in this destructor.
 *
 * #### `SgEnumDeclaration::isSgEnumDeclaration (SgNode *s)`
 * Cast function (from derived class to SgEnumDeclaration pointer).
 * This functions returns a SgEnumDeclaration pointer for any input of a
 * pointer to an object derived from a SgEnumDeclaration.
 * - Return: Returns valid pointer to SgEnumDeclaration if input is derived from a SgLocatedNode.
 *
 * #### `SgEnumDeclaration::isSgEnumDeclaration (const SgNode *s)`
 * Cast function (from derived class to SgEnumDeclaration pointer).
 * This functions returns a SgEnumDeclaration pointer for any input of a
 * pointer to an object derived from a SgEnumDeclaration.
 * - Return: Returns valid pointer to SgEnumDeclaration if input is derived from a SgLocatedNode.
 *
 * #### `SgEnumDeclaration::get_name() const`
 * Access function for p_name.
 * - Return: Returns SgName by value.
 *
 * #### `SgEnumDeclaration::set_name(SgName name)`
 * Access function for p_name.
 * - Return: Returns void.
 *
 * #### `SgEnumDeclaration::get_embedded() const`
 * Access function for p_embedded.
 * - Return: Returns bool by value.
 *
 * #### `SgEnumDeclaration::set_embedded(bool embedded)`
 * Access function for p_embedded.
 * - Return: Returns void.
 *
 * #### `SgEnumDeclaration::get_type() const`
 * Access function for p_type.
 * - Return: Returns pointer to SgType.
 *
 * #### `SgEnumDeclaration::set_type(SgType* type)`
 * Access function for p_type.
 * - Return: Returns void.
 *
 * #### `SgEnumDeclaration::get_enumerators() const`
 * Access function for p_enumerators.
 * - Return: Returns a const reference to SgInitializedNamePtrList.
 *
 * #### `SgEnumDeclaration::get_enumerators()`
 * Access function for p_enumerators.
 * - Return: Returns reference to SgInitializedNamePtrList.
 *
 * #### `SgEnumDeclaration::get_scope() const`
 * Access function for p_scope.
 * - Return: Returns pointer to SgScopeStatement.
 *
 * #### `SgEnumDeclaration::set_scope(SgScopeStatment* scope)`
 * Access function for p_scope.
 * - Return: Returns void.
 */
class SgEnumDeclaration;

/** @brief This class represents the concept of the dynamic execution of a string,
 *
 * file, or code object. This node is intended for use with Python.
 *
 * **Data members**
 *
 * #### `SgExecStatement::p_executable`
 * The object to be executed.
 * This expression is evaluated and executed dynamically.
 *
 * #### `SgExecStatement::p_globals`
 * The global execution environment.
 *
 * #### `SgExecStatement::p_locals`
 * The local execution environment.
 *
 * **Member functions**
 *
 * #### `SgExecStatement::SgExecStatement ( SgExpression* executable, SgExpression* globals, SgExpression* locals)`
 * This is the constructor.
 * This constructor builds the SgExecStatement base class.
 *
 * #### `SgExecStatement::~SgExecStatement()`
 * This is the destructor.
 *
 * #### `SgExecStatement::isSgExecStatement (SgNode *s)`
 * Cast function (from derived class to SgExecStatement pointer).
 * This functions returns a SgExecStatement pointer for any input of a
 * pointer to an object derived from a SgExecStatement.
 * - Return: Returns valid pointer to SgExecStatement if input is derived from a SgExecStatement.
 *
 * #### `SgExecStatement::isSgExecStatement (const SgNode *s)`
 * Cast function (from derived class to SgExecStatement pointer).
 * This functions returns a SgExecStatement pointer for any input of a
 * pointer to an object derived from a SgExecStatement.
 * - Return: Returns valid pointer to SgExecStatement if input is derived from a SgExecStatement.
 */
class SgExecStatement;

/** @brief This class represents the concept of a C and C++ expression list.
 *
 * This is a class used to support multiple IR nodes internally, for example in a
 * function call "foo(exp,exp)" contains and expression list (SgExprListExp).
 * This object supports the following IR nodes:
 * - SgNewExp
 * - SgFunctionCallExp
 * - SgAggregateInitializer
 * - SgConstructorInitializer
 * - Todo: Consider that get_type() returns a SgDefalutType and should return the SgType
 * associated with the last expression in the list.
 * - See also:
 * Example of using a SgExprListExp object
 *
 * **Data members**
 *
 * #### `SgExprListExp::p_conditional_exp`
 * This pointer points to a SgExpression object.
 *
 * #### `SgExprListExp::p_true_exp`
 * This pointer points to a SgExpression object.
 *
 * #### `SgExprListExp::p_false_exp`
 * This pointer points to a SgExpression object.
 *
 * #### `SgExprListExp::p_expression_type`
 * This pointer points to a SgType object.
 *
 * **Member functions**
 *
 * #### `SgExprListExp::SgExprListExp ( Sg_File_Info* startOfConstruct = NULL, SgClassSymbol *symbol = NULL )`
 * This is the constructor.
 * This constructor builds the SgExprListExp base class.
 * - Param `startOfConstruct`: represents the position in the source code
 *
 * #### `SgExprListExp::~SgExprListExp()`
 * This is the destructor.
 * There are a lot of things to delete, but nothing is deleted in this destructor.
 *
 * #### `SgExprListExp::isSgExprListExp (SgNode *s)`
 * Cast function (from derived class to SgExprListExp pointer).
 * This functions returns a SgExprListExp pointer for any input of a
 * pointer to an object derived from a SgExprListExp.
 * - Return: Returns valid pointer to SgExprListExp if input is derived from a SgLocatedNode.
 *
 * #### `SgExprListExp::isSgExprListExp (const SgNode *s)`
 * Cast function (from derived class to SgExprListExp pointer).
 * This functions returns a SgExprListExp pointer for any input of a
 * pointer to an object derived from a SgExprListExp.
 * - Return: Returns valid pointer to SgExprListExp if input is derived from a SgLocatedNode.
 *
 * #### `SgExprListExp::get_conditional_exp() const`
 * Access function for p_conditional_exp.
 * - Return: Returns pointer to SgExpression.
 *
 * #### `SgExprListExp::set_conditional_exp(SgExpression* conditional_exp)`
 * Access function for p_conditional_exp.
 * - Param `conditional_exp`: is the conditional test expression
 * - Return: Returns void.
 *
 * #### `SgExprListExp::get_true_exp() const`
 * Access function for p_true_exp.
 * - Return: Returns pointer to SgExpression.
 *
 * #### `SgExprListExp::set_true_exp(SgExpression* true_exp)`
 * Access function for p_true_exp.
 * - Param `true_exp`: expression to evaluate if test is true.
 * - Return: Returns void.
 *
 * #### `SgExprListExp::get_false_exp() const`
 * Access function for p_false_exp.
 * - Return: Returns pointer to SgExpression.
 *
 * #### `SgExprListExp::set_false_exp(SgExpression* false_exp)`
 * Access function for p_false_exp.
 * - Param `false_exp`: expression to evaluate if test is false.
 * - Return: Returns void.
 *
 * #### `SgExprListExp::get_expression_type() const`
 * Access function for p_expression_type.
 * - Return: Returns pointer to SgExpression.
 *
 * #### `SgExprListExp::set_expression_type(SgType* expression_type)`
 * Access function for p_expression_type.
 * Both the p_true_exp and p_false_exp must have the same type.
 * - Param `expression_type`: type of result.
 * - Return: Returns void.
 */
class SgExprListExp;

/** @brief This class represents the concept of a C or C++ statement which contains a
 *
 * expression.
 * Any expression can be used alone as statement in C and C++, the expression statment
 * provides a mechanism to wrap any SgExpression and make it a SgStaement for use
 * anywhere a SgStatement might be required (e.g. in a list of SgStatement objects in a
 * SgBasicBlock).
 * - See also:
 * Example of using a SgExprStatement object
 *
 * **Data members**
 *
 * #### `SgExprStatement::p_expression_root`
 * This pointer points to the SgExpressionRoot which wraps the SgExpression.
 *
 * **Member functions**
 *
 * #### `SgExprStatement::SgExprStatement ( Sg_File_Info* startOfConstruct = NULL )`
 * This is the constructor.
 * This constructor builds the SgExprStatement base class.
 * - Param `startOfConstruct`: represents the position in the source code
 *
 * #### `SgExprStatement::~SgExprStatement()`
 * This is the destructor.
 * There are a lot of things to delete, but nothing is deleted in this destructor.
 *
 * #### `SgExprStatement::isSgExprStatement (SgNode *s)`
 * Cast function (from derived class to SgExprStatement pointer).
 * This functions returns a SgExprStatement pointer for any input of a
 * pointer to an object derived from a SgExprStatement.
 * - Return: Returns valid pointer to SgExprStatement if input is derived from a SgLocatedNode.
 *
 * #### `SgExprStatement::isSgExprStatement (const SgNode *s)`
 * Cast function (from derived class to SgExprStatement pointer).
 * This functions returns a SgExprStatement pointer for any input of a
 * pointer to an object derived from a SgExprStatement.
 * - Return: Returns valid pointer to SgExprStatement if input is derived from a SgLocatedNode.
 *
 * #### `SgExprStatement::get_the_expr() const`
 * Returns pointer to SgExpression obtained from SgExpressionRoot wrapper for SgExpression.
 * - Return: Returns pointer to SgExpression.
 *
 * #### `SgExprStatement::set_the_expr(SgExpression* the_expr)`
 * Sets pointer in the SgExpressionRoot (constructed as required) to wrap the SgExpression.
 * - Return: Returns void.
 *
 * #### `SgExprStatement::get_expression_root() const`
 * Access function for p_expression_root.
 * - Return: Returns pointer to SgExpressionRoot.
 *
 * #### `SgExprStatement::set_the_expr(SgExpressionRoot* expression_root)`
 * Access function for p_expression_root.
 * - Return: Returns void.
 */
class SgExprStatement;

/** @brief This class represents the notion of an expression. Expressions
 *
 * are derived from SgLocatedNodes, since similar to statement, expressions
 * have a concrete location within the user's source code.
 * There are no direct uses of this IR node anywhere.  All expressions
 * are derived from this IR node to build derived classes such as unary and
 * binary operators, conditional expressions, delete expression, variable
 * reference expressions, etc.
 * - Note: The SgExpression class has a virtual get_type() member function which returns
 * the type associated with any expression.  The get_type() support has the following
 * properties:
 * - The type is not explicitly stored in the IR, except for:
 * -# SgCastExp
 * -# SgNewExp
 * -# SgAssignInitializer
 * -# SgAggregateInitializer
 * -# SgConstructorInitializer
 * -# SgVarArgOp
 * -# SgVarArgStartOp
 * -# SgVarArgEndOp
 * -# SgVarArgCopyOp
 * -# SgVarArgStartOneOperandOp
 * - Binary operators return the type associated with the lhs operand, except for (which
 * return the type associated with the rhs operand):
 * -# SgDotExp
 * -# SgArrowExp
 * -# SgDotStarOp
 * -# SgArrowStarOp
 * - Unary operators return the type of their operand
 * - Value expressions return predefined types (that or their value)
 * - Other special cases include:
 * -# SgPntrArrRefExp, returns the element type of the array being referenced by the
 * lhs (non-trivial implementation), must handle cases of:
 * -# SgArrayType
 * -# SgPointerType
 * -# SgTypedefType
 * -# SgPointerDerefExp, returns the type of the dereferenced pointer (non-trivial
 * implementation), must handle cases of:
 * -# SgArrayType
 * -# SgPointerType
 * -# SgTypedefType
 * -# SgReferenceType
 * -# SgFunctionType
 * -# SgMemberFunctionType
 * -# SgAddressOfOp, creates a pointer to the base type which is stored internally.
 * -# SgFunctionCallExp, returns the return type of the function being called (except
 * in special cases, this might require more work later).  Migration to the correct
 * implementation is more complex due to requirements of backward compatability with
 * older design.
 * -# SgSizeOfOp, returns SgTypeUnsignedInt
 * -# SgTypeIdOp, returns type of stored operand (not clear if this is correct)
 * -# SgConditionalExp, tests for matching of types from true and false branches, but
 * returns type associated with true branch. The types from each branch frequently
 * don't match in application codes.
 * -# SgDeleteExp, returns SgTypeVoid (used to return SgDefaultType)
 * -# SgExprListExp, returns SgDefaultType, but should return type of last expression.
 * -# SgVarRefExp, returns the type associated with the SgSymbol stored internally.
 * -# SgFunctionRefExp, returns the type associated with the SgSymbol stored
 * internally.
 * -# SgMemberFunctionRefExp, returns the type associated with the SgSymbol stored
 * internally.
 * -# SgThisExp, returns a SgPointerType constructed from the type of the class symbol
 * (stored inteernally).
 * -# SgEnumVal, returns a SgTypeInt
 * -# SgLongLongIntVal, returns SgTypeLong (should return SgTypeLongLong)
 * -# SgNewExp, returns a pointer to p_specified_type (explicitly stored there)
 * - Logical operators return SgTypeInt, these include:
 * -# SgAndOp
 * -# SgOrOp
 * -# SgNotOp
 * -# All others operate as a SgBinaryOp (type of lhs operand) or SgUnaryOp (type of
 * operand). not clear if this is the correct behavior.
 * - The following expressions return a SgTypeDefault:
 * -# SgThrowOp
 * -# SgNullExpression
 * -# SgVariantExpression
 * - Note: get_type() vs. get_expression_type(). Note that there is of some IR nodes a
 * get_expression_type() which is always a data access function to a stored type.
 * The get_expression_type() function is for **internal** **use** **only.**
 * Where used, this function can return NULL if the explicitly stored
 * p_expression_type variable is not needed (this is to support it later
 * being removed). Although the type should be computed most of the time, there are
 * some IR nodes where it must be stored explicitly (detailed above; for example where the
 * SgTypeDefault is used and for SgCastExp, etc.). The function get_expression_type()
 * is not a part of the user interface and users should call get_type() instead.
 * - Internal: This is a base class for all expressions.
 * As a design point IPR has an empty expression, not an empty statement. And empty statement can
 * be built as an expression statement with an empty expression (which IPR does have).
 * We should consider having such an empty expression.  Currently we have and
 * expression statement with a null pointer (none too elegant). Sage III now has both a
 * SgNullStatement and a SgNullExpression.
 * - Todo: Consider placing the VARARGS expression nodes into a common base class
 * derived from SgExpression.
 * - Todo: I have removed the access functions from the explicit storage of type information in
 * SgExpression objects as phase 1 of a 2 phase approach to eliminate the storage of the
 * type in the SgExpression IR nodes.  This type should be computed where required.
 * This would avoid it being held redundently.  This mechanism is being redone
 * internally.  Some IR nodes will have likely have to store there type explicaitly
 * (function expressions for example, though it might be better computed through the
 * symbol).  It is not clear it this computing of the type will be better than stroing of
 * the type explicitly.  It might be required for SgBinaryOp IR nodes to store the type
 * if it is not clearly from either the lhs or rhs (if no simple rule exists).
 * - Todo: SgScopeOp is deprecated and will be removed in a future version of ROSE.
 * It is a hold over from support for CC++ which is not supported in SAGE III anymore.
 * - Todo: SgRefExp is deprecated and will be removed in a future version of ROSE.
 * It is not used anywhere within SAGE III and I don't know why it is there.
 * - Todo: Need to find an example of where SgClassNameRefExp is used.  It is build in
 * the EDG/Sage III translation, but not in a way that it is obvious that it is
 * still used within Sage III.  So this may have to be removed at a latr date.
 * - Todo: To support Fortran parser we need an IR node which will represent the
 * ambiguity of an array access or function call expression.  These are
 * then resolved within the AST after parsing (requires AST Fixup rule).
 * - Todo: Fortran support requires support for function call using: "foo(temp=*<label>)"
 * this might force the development of a label expression to support this.
 * Code using this compiles with gfortran, so it appears to be F90.
 * - See also:
 * Example of using a SgExpression object
 *
 * **Data members**
 *
 * #### `SgExpression::p_lvalue`
 * This boolean variable marks the current expression as a
 * left hand side value (lvalue).
 * - Deprecated: This computation of this value is innacurate.  It identifies expressions which have been assigned to.  This does not include all lvalues as defined by the C++ standard.  For "int* p", "*p" and "&p" are both lvalues, despite not being assigned to (section 3.10).
 *
 * #### `SgExpression::p_need_paren`
 * This boolean value marks the current expression as requiring parenthises.
 * This boolean value marks the current expression as requiring parenthises (the
 * information comes from the frontend's interpretation of the requirement and is
 * almost always overly conservative.  The unparser currently backs out more
 * accurate rules based on operator precedence and removed then where they
 * are not truely required.  Thus the purpose of this variable is to capture the
 * interpritation of the frontend regarding the use of parenthesis.
 *
 * **Member functions**
 *
 * #### `SgExpression::SgExpression ( Sg_File_Info* startOfConstruct = NULL )`
 * This is the constructor.
 * This constructor builds the SgExpression base class.
 * - Param `startOfConstruct`: represents the position in the source code
 * - See also:
 * Example:create an SgExpression object
 *
 * #### `SgExpression::~SgExpression()`
 * This is the destructor.
 * Only the Sg_File_Info object can be deleted in this object.
 *
 * #### `SgExpression::isSgExpression (SgNode *s)`
 * Cast function (from derived class to SgExpression pointer).
 * This functions returns a SgExpression pointer for any input of a
 * pointer to an object derived from a SgExpression.
 * - Return: Returns valid pointer to SgExpression if input is derived from a SgExpression.
 *
 * #### `SgExpression::isSgExpression (const SgNode *s)`
 * Cast function (from derived class to SgExpression pointer).
 * This functions returns a SgExpression pointer for any input of a
 * pointer to an object derived from a SgExpression.
 * - Return: Returns valid pointer to SgExpression if input is derived from a SgExpression.
 *
 * #### `SgExpression::get_need_paren (void) const`
 * returns bool value if front-end considers parenthesis to be required.
 * This function returns a boolean value which is true if parenthesis are required
 * for the proper eveluation of the current expression.  The frontend is however overly
 * conservative and introduces parenthesis too often.  The value as determined by the
 * frontend is saved in the SAGE III AST because it is sometimes required for more
 * complex expressions and contructor calls with initializers.  At present the
 * unparser applies operator precedence rules to eliminate some of the redundent
 * parenthesis within expressions (this makes the output more appealing).
 * - Return: Returns bool value (true if parenthesis are needed around the expression).
 *
 * #### `SgExpression::set_need_paren (bool need_paren)`
 * This function allows the p_need_paren flag to be set (used internally).
 * This function is mostly used internally but would required for the construction of
 * specific expression trees where the operator precedence would be insufficent to generate
 * the correct code (which si the purpose of adding parenthesis in source code).
 * - Param `need_paren`: - sets value of internal p_need_paren flag (true if parenthesis are
 * needed around the expression).
 * - Return: Returns void.
 *
 * #### `SgExpression::get_lvalue (void) const`
 * Returns a bool value if the current expression is assigned to.
 * - Deprecated: The computation of this value is innacurate in terms of the lvalue as defined by the standard.  It identifies expressions which have been assigned to.  This does not include all lvalues as defined by the C++ standard.  For "int* p", "*p" and "&p" are both lvalues, despite not being assigned to (section 3.10).  See SgExpression::p_lvalue.
 * - Return: Returns true if the current expression is assigned to.
 *
 * #### `SgExpression::set_lvalue (bool lvalue)`
 * This function allows the p_lvalue flag to be set (used internally).
 * This function is mostly used internally but would required for the construction of
 * specific expression trees. In general it is set by the frontend, but not used elsewhere
 * within SAGE III (will be tested in the future).
 * - Param `lvalue`: - sets value of internal p_lvalue flag (true if the current is to be a left hand side value (lvalue)).
 * - Return: Returns void.
 *
 * #### `SgExpression::isDefinable (void) const`
 * For Fortran.  When called from a node of any other language, behavior is undefined (see SageInterface::is_Fortran_language).
 * Returns a bool value which is true if the current expression is "almost definable".
 * "Almost definable" here means as defined by the Fortran standard (2.5.5), with the caveat that allocatable variables are always called an lvalue.
 * The Fortran standard dictates that an allocatable variable which has not been allocated is not definable.
 * In the general case, it cannot be statically determined whether an allocatable variable has been allocated or not.
 * Note: constants in Fortran are not lvalues, while in C and C++ lvalues may be const.
 * - Return: Returns true if the current expression is almost definable.
 *
 * #### `SgExpression::isUsedAsDefinable (void) const`
 * For Fortran.  When called from a node of any other language, behavior is undefined (see SageInterface::is_Fortran_language).
 * Returns a bool value which is true if the current expression is used as a definable value.  That is, it is definable (assigned to).
 * - Return: Returns true if the current expression is used as a definable value.
 *
 * #### `SgExpression::isLValue (void) const`
 * For C and C++. When called from a node of any other language, behavior is undefined (see SageInterface::is_C*_language).
 * Returns a bool value which is true if the current expression is an lvalue, as defined by the C++ standard.
 * - Return: Returns true if the current expression is an lvalue.
 *
 * #### `SgExpression::isUsedAsLValue (void) const`
 * For C and C++. When called from a node of any other language, behavior is undefined (see SageInterface::is_C*_language).
 * Returns a bool value which is true if the current expression is used as an lvalue.  That is, it is an lvalue and does not get converted to an rvalue by its parent expression.
 * - Return: Returns true if the current expression is used as an lvalue.
 *
 * #### `SgExpression::isChildUsedAsLValue (SGExpression) const`
 * For C and C++. When called from a node of any other language, behavior is undefined (see SageInterface::is_C*_language).
 * Returns a bool value which is true if the specified expression is used as an lvalue.  That is, it is an lvalue and does not get converted to an rvalue by its parent expression.
 * - Internal: For use by children calling their parents.
 * - Return: Returns true if the specified expression is used as an lvalue.
 *
 * #### `SgExpression::get_type() const`
 * Get the type associated with this expression.
 * This function is the correct user function to call to get the
 * type (note that get_expression_type() is for internal use only).
 * - Return: Returns pointer to SgType (base class for all possible types)
 *
 * #### `SgExpression::set_type()`
 * Set the type associated with this expression
 * This is an internally call function, it sets up the type of the expression
 * based upon the types of the subexpressions (if any). Thus it takes no
 * arguments.
 * - Return: Returns void
 *
 * #### `SgExpression::get_expression_type() const`
 * **FOR** **INTERNAL** **USE** Data access function for explicitly stored type.
 * This access function is used internally to access explicitly stored types (where it
 * is required to be stored).  Currently a larger number of IR nodes have this
 * data member than where it is explicitly required, this will be fixed in the
 * future. Where used, this function can return NULL if the explicitly stored
 * p_expression_type variable is not needed (this is to support it later
 * being removed).
 * - Return: Returns pointer to SgType (base class for all possible types)
 *
 * #### `SgExpression::set_expression_type()`
 * **FOR** **INTERNAL** **USE** Data access function for explicitly stored type.
 * This function is only meaningful where the type is explicitly stored.
 * It is an internally called function, it sets up the type of the expression
 * based upon the types of the subexpressions (if any). Thus it takes no
 * arguments.
 * - Return: Returns void
 */
class SgExpression;

/** @brief This class represents a source file for a project (which may contian
 *
 * many source files and or directories).
 * This IR node contains information specific to this file and references the
 * project for more project (global) information.  The dominately useful data in
 * this IR node is the pointer to the global scope for this file (structurally
 * global scope, but actually file scope and global scope by C++ scoping rules).
 * - Internal: This IR node does not appear many times in an AST.  Also it is current
 * rather bizzar that we keep all the file names instead of just the single relavant
 * one!   This should be fixed as it is confusing.
 * - Todo: The ROSEAttributesListContainerPtr p_preprocessorDirectivesAndCommentsList
 * should be implemented a list instead of a pointer to a list.  This might require
 * a list copy in the internal hand,ing, but would simplify the design and there is
 * not the same memory constraint of having a pointer to a list vs. a list here because
 * the list is almost always valid (most source code incluses at least one comment or
 * CPP directive) and there is only one SgFile object per source file (so there are
 * relatively few SgFile nodes in even a very large AST).
 * - Todo: This IR nodes now has a Sg_File_Info pointer, however it needs to be made consistant
 * with the filename that is returned from SgFile::get_fileName().
 * - Todo: The default constructor for SgFile sets the SgGlobal pointer to NULL and perhaps it
 * would be better if it set it to a valid SgGlobal object then we would have a better
 * defined empty list of declarations.
 * - Todo: Yarden has suggested we provide a way to modify the link line that would be
 * generated to support the backend compilation.  I think we should have a list
 * of strings that could be added to the link line (appended to the end would be
 * the simplist).  Else we need a virtual function that could be overloaded to
 * customize the control over the link command generation (however we want to
 * discourage the derivation of user defined IR nodes from existing IR nodes
 * since this would break some of the internal mechanisms that use the memory pools).
 * - See also:
 * Example of using a SgFile object
 *
 * **Data members**
 *
 * #### `SgFile::p_root`
 * This is the global scope.
 * Note that the global scope contains the file scope as the two are not structurally
 * differentiated within the source code.
 * - Internal: This name will be changed to "globalScope" at some point.
 *
 * #### `SgFile::p_no_implicit_templates`
 * This is a reference to the GNU g++ command line option (same name).
 * If the option "-no_implicit_templates" is seen on the command line then this is true.
 * Else it may be manipulated directly within the ROSE command line processing.
 * This option controls if instantiated templates should be output where they are
 * not specified explicitly via explicit template instatiation directives (see
 * SgTemplateInstatiationDirectiveStatement, formally part of C++).
 * - Internal: We need to see this option since it effects how template instantiations are
 * generated. Other backend compilers may force this to be recognized in other ways so that
 * we map other vendor's equivalent compiler options to this one.
 *
 * #### `SgFile::p_no_implicit_inline_templates`
 * This is a reference to the GNU g++ command line option (same name).
 * If the option "-no_implicit_inline_templates" is seen on the command line then this is true.
 * Else it may be manipulated directly within the ROSE command line processing.
 * This option controls if instantiated templates for inline functions should be output where they are
 * not specified explicitly via explicit template instatiation directives (see
 * SgTemplateInstatiationDirectiveStatement, formally part of C++).
 * - Internal: We need to see this option since it effects how template instantiations are
 * generated.  Other backend compilers may force this to be recognized in other ways so that
 * we map other vendor's equivalent compiler options to this one.
 *
 * #### `ROSEAttributesListContainerPtr SgFile::p_preprocessorDirectivesAndCommentsList`
 * This is the container of comments and preprocessor control statements that was
 * extracted from the file and which has been woven back into the AST (using heuristics).
 * This information is extracted from the file using a lex based parser.  Within AST post
 * processing the comments and preprocessor control statements are woven back into the
 * AST using simple heuristics.  They are attached to the IR nodes as attributes
 * (see AST attribute Mechanism).  They are unparsed as part of the code generation
 * phase.
 *
 * #### `SgFile::p_originalCommandLineArgumentList`
 * Copy of original argc and argv command line passed to ROSE translator (converted to
 * STL list of strings).
 * - Internal: This is a deep copy.
 *
 * #### `SgFile::p_file_info`
 * This object permits the file name map mechanism to be used on SgFile objects.
 * This Sg_File_Info object encapsulates the file id mechanism which maps file ids
 * (integers) to filenames (strings). This permist us to save significant storage
 * by holding string names only once in memory and associating integers with them
 * saving memory and simplify matching file names.  The memory saving is because there
 * would otherwise be a LOT of redundant filename strings, 2 per IR node in many cases.
 * the performance advantage is that integer comparisons are significantly than string
 * comparisions and there are a lot of these (one per IR node) in the traversal
 * mechanisms.
 * - Internal: The filename is not currently correct, I think.
 *
 * **Member functions**
 *
 * #### `SgFile::SgFile()`
 * This is the constructor.
 * This constructor builds the SgFile base class.
 * - See also:
 * Example:create an SgFile object
 *
 * #### `SgFile::~SgFile()`
 * This is the destructor.
 * There is nothing to delete in this object.
 *
 * #### `SgFile::isSgFile (SgNode *s)`
 * Cast function (from derived class to SgFile pointer).
 * This functions returns a SgFile pointer for any input of a
 * pointer to an object derived from a SgFile.
 * - Return: Returns valid pointer to SgFile if input is derived from a SgLocatedNode.
 *
 * #### `SgFile::isSgFile (const SgNode *s)`
 * Cast function (from derived class to SgFile pointer).
 * This functions returns a SgFile pointer for any input of a
 * pointer to an object derived from a SgFile.
 * - Return: Returns valid pointer to SgFile if input is derived from a SgLocatedNode.
 *
 * #### `SgFile::get_globalScope()`
 * Function to return global scope (list of declaration statments, SgGlobal).
 * This functions returns a SgGlobal pointer.
 * - Return: Returns valid pointer to SgGlobal.
 * - Internal: This return value is never NULL, but could be a SgGlobal containing
 * an empty list.
 *
 * #### `list<string> SgFile::get_originalCommandLineArgumentList() const`
 * Returns a list of strings representing the original command-line.
 *
 * #### `SgFile::set_originalCommandLineArgumentList( list<string> originalCommandLineArgumentList )`
 * Sets the list of strings representing the original command-line.
 */
class SgFile;

/** @brief This class represents the location of the code associated with the IR node in the
 *
 * original source code.
 * This object is used to represent the starting and the ending position of the source code
 * associated with some IR nodes (in other cases the endOfConstruct returnes the same data
 * as the startOfConstruct).
 * Classification of the IR nodes is included in this object and applies to:
 * - SgStatements
 * - SgExpression
 * - SgInitializedName
 * - SgFile (and maybe SgDirectory)
 * - Perhaps also SgTemplateArguments, SgTemplateParameters, SgBaseClass
 * classifications are:
 * - e_transformation \n
 * The IR node is part of a transformation.  The transformation may be assigned to a
 * file, in which case the file id references the file (default is the source file, that
 * reachable from the scope where the transformation is done).
 * - e_compiler_generated \n
 * The IR node may be part of a compiler generated IR nodes (or subtree, in which
 * case all IR nodes in the subtree are marked as compiler generated).  Compiler
 * generated IR nodes include:
 * -# implicit casts
 * -# instantiated templates (not explicitly specialized)
 * -# non-explicit return statements in **main** function.
 * -# (... complete this list) \n
 * - e_output_in_code_generation \n
 * -# Used to explicitly makr any compiler generated IR nodes for output (forces
 * template instaiations to be output in the generated source if transformed).
 * - e_shared \n
 * Marks IR nodes as shared (set by AST merge mechanism).  Note that SgTypes and
 * SgSymbols don't have a Sg_File_Info object, but are ALWAYS shared. \n
 * (multiple classifications can co-exist).
 * Note that p_filename is correctly initialized in the Sg_File_Info object for a
 * SgGlobal, but the p_line and p_col are set to 0 (zero).  All other IR nodes have valid
 * Sg_File_Info objects with correct p_filename and p_line and p_col set to nonzero
 * values.  A few IR nodes in the AST will have a default value of their Sg_File_Info
 * nodes with p_filename set to "NULL_FILE" and p_line and p_col set to 0 (zero).
 * These are increasingly rare to find in the AST, but since some IR nodes can't
 * be associated with their source position in the AST they persist.
 * - Internal: A pointer for this object is in the SgNode, but it is only valid for
 * SgLocatedNodes and the SgInitializedName. The get_File_Info() member function
 * (and associated get_startOfConstruct() and get_endOfConstruct() member functions)
 * are implemented in the SgNode, for uniformity, but return NULL for all but the
 * SgLocatedNodes and the SgInitializedName IR nodes.
 * - Todo: isOutputInCodeGeneration() is orthogonal to isCompilerGenerated and
 * isTransformation().  Currently IR nodes that are marked as isTransformation()
 * are output, but these need to be marked as also being isOutputInCodeGeneration()
 * so that orthogonality of the concepts is maintained.
 * - Todo: It is possible to call get_file_info() on a SgFileInfo object and this needs to be
 * fixed because it does not make any sense.  This is because get_file_info is defined as
 * a virtual function on SgNode.  Not sure this is a great design, but maybe it just
 * needs a local implementation of a private get_file_info() member function so that it
 * can't be called (can be hidden).
 * - Todo: Should there be a simpler way to copy a SgFileInfo object than:
 * "new Sg_File_Info(*fileInfo);" or "fileInfo->copy();"; likely not!
 * - Todo: Define the subset of IR nodes which would all have:
 * -# Sg_File_Info
 * -# AstAttributeMechanism
 * -# SgNode (parent pointer) \n
 * This will make the documentation more intuative.  The argument supporting this is
 * that we operate on those IR nodes that correspond to the visual representation of the
 * source code (so we can exclude SgSymbols, and SgTypes).  Having the same subset of
 * IR nodes permits a simpler documentation of the API and intuition as to where it
 * applies.  A likely subset would be:
 * -# SgLocatedNodes
 * -# SgFile
 * -# SgDirectory (questionable)
 * -# SgInitializedName
 * -# SgBaseClass
 * -# SgTemplateArgument
 * -# SgTemplateParameter
 * - Todo: Remove the functions: isCompilerGeneratedNodeToBeUnparsed(),
 * setCompilerGeneratedNodeToBeUnparsed(), and unsetCompilerGeneratedNodeToBeUnparsed()
 * from where they are called.
 * - Todo: Consider putting the endOfConstruct information into the single Sg_File_Info object.
 * Currently the SgLocatedNode stores two Sg_File_Info objects, one for the beginning and
 * the end of each construct.  This would save significant space in the AST.
 * Additional information in the Sg_File_Info could be:
 * - offset from starting line number to the end of construct
 * - flag for if end of construct is located in the same file as the start of construct
 * - Todo: Consider using "short int" instead of "int" for the file_id, line, and col
 * (and maybe the classificationBitField) to reduce the size of the data structure.
 * Padding is not a significant issue since data structures are allocated in
 * contiguious memory (except for padding to at least the nearest byte if bit field
 * widths are used.
 * - See also:
 * Example of using a Sg_File_Info object
 *
 * **Data members**
 *
 * #### `Sg_File_Info::p_filename`
 * This is the filename of the location of the source code for this IR node.
 * Note that the filename of the start of a construct CAN be different from the start of
 * the language construct (#include can be used to do this).  The name of the file
 * also the included the full path to the file in the directory structure.
 * The value of this variable is always a valid pointer if the classification is not
 * isTransformed or isCompilerGenerated.
 * If IR node is compiler generated for a transformation then value is set to either
 * "compilerGenerated" or "transformation", but should be considered to be undefined.
 * - Internal: This name is often quite long and likely is a significantly redundent piece of
 * information stored for each IR node (twice, considering a Sg_File_Info object is used
 * for the start and end of each IR node (where it is used, e.g. SgLocatedNodes and the
 * SgInitializedName IR nodes).
 *
 * #### `Sg_File_Info::p_file_id`
 * This is a key that maps integers to file names, it prevents redundent storage of filename strings.
 * This is part of a mechanism to reduce the memory requirements of the IR (to support
 * whole program analysis in ROSE).  The values are keys to an STL map which relates
 * file ids to file name strings, allowing a more compressed representation of the
 * filename data in the Sg_File_Info object.  There are two maps, one for finding
 * the filename string from a file id integer (p_fileidtoname_map), and another for
 * the reverse map (p_nametofileid_map).
 * - Internal: We will in the future limit the range of this value (expect a signed short int:
 * 16 bits, 32K = 32768 files plus negative values for special cases).
 *
 * #### `Sg_File_Info::p_line`
 * This is the line number of the location of the source code for this IR node.
 * This information is useful in the heuristics of attaching comments to IR nodes
 * (SgStatement and SgExpression IR nodes).
 * The value of this variable is always greater than or equal to zero. In the case of
 * isCompilerGenerated() == true or isTransformation() == true then value is INT_MAX-1.
 * This value if define for this case so that all comments will be attached before the
 * associated statement.
 * - Internal: We will in the future limit the range of this value (expect a short int:
 * 20 bits, 1000K = 1,048,576 lines per file).
 *
 * #### `Sg_File_Info::p_col`
 * This is the column number of the location of the source code for this IR node.
 * This information is useful in the heuristics of attaching comments to IR nodes
 * (SgStatement and SgExpression IR nodes).
 * The value of this variable is always greater than or equal to zero.
 * If IR node is compiler generated for a transformation then value is set to INT_MAX-1.
 * - Internal: We will in the future limit the range of this value (expect a short int:
 * 16 bits, 64K = 65536 charaters per line).
 *
 * #### `Sg_File_Info::p_classificationBitField`
 * This is mechanism for classification of the IR node.
 * IR nodes use this mechanism to classify themselves as either part of a transformation
 * or compiler generated (other enum values of Sg_File_Info::classifier are not used).
 * - Internal: We will in the future limit the range of this value (expect an unsigned char:
 * 8 bits, 64K = 65536 charaters per line).
 *
 * #### `Sg_File_Info::p_fileIDsToUnparse`
 * This set contains a list of all file ids for which the accompanying IR node should be unparsed.
 * This is part of the AST Merge mechanism.
 * This set contains a list of file ids. During unparsing, if we encounter
 * a node with this Sg_File_Info object, we only want to unparse this file
 * if the file we are currently unparsing is in this list.
 * - Internal: NOTE: This set should be empty unless the node is marked as shared
 *
 * #### `std::map< int, std::string > Sg_File_Info::p_fileidtoname_map`
 * This is a static STL map of file id (integers) to file names (strings).
 *
 * #### `std::map< std::string, int > Sg_File_Info::p_nametofileid_map`
 * This is a static STL map of file names (strings) to file id (integers).
 *
 * #### `int Sg_File_Info::p_max_file_id`
 * This current number of unique files in use within ROSE.
 * - Internal: Whey do we need this value?
 * - Todo: Verify that we really need this value.
 *
 * #### `int Sg_File_Info::p_cur_file`
 * This is a static variable which used to be used by the unparser.
 * - Internal: This is no longer needed, and not used, I think.
 * - Deprecated: This data member will be removed at some point.
 *
 * #### `int Sg_File_Info::p_cur_line`
 * This is a static variable which used to be used by the unparser.
 * - Internal: This is no longer needed, and not used, I think.
 * - Deprecated: This data member will be removed at some point.
 *
 * #### `Sg_File_Info::p_isPartOfTransformation`
 * This is a depreicated variable, previously used to indicate if an IR node was
 * part of a transformation.
 * - Internal: This variable is redundant with the information stored in the
 * p_classificationBitField.
 * - Deprecated: This data member will be removed at some point.
 *
 * **Member functions**
 *
 * #### `Sg_File_Info::Sg_File_Info()`
 * This is the less useful default constructor.
 * This constructor builds the Sg_File_Info base class.
 * - Internal: This constructor should be deprecated since we don't want it to be used.
 * It's presence is historical, since it was previously used a lot (inappropriately).
 * Before it is removed we likely need a constructor that can take a
 * Sg_File_Info::classifier enum value.
 * - See also:
 * Example:create an Sg_File_Info object
 *
 * #### `Sg_File_Info::Sg_File_Info ( const char *filename, int line=0, int col=0 )`
 * This is the more useful constructor.
 * This constructor builds the Sg_File_Info base class.
 * - See also:
 * Example:create an Sg_File_Info object
 *
 * #### `Sg_File_Info::Sg_File_Info(const Sg_File_Info & X)`
 * This the copy constructor (deep copy made).
 * - Internal: This constructor does a deep copy of all data in the Sg_File_Info object.
 * - See also:
 * Example:create an Sg_File_Info object
 *
 * #### `Sg_File_Info::~Sg_File_Info()`
 * This is the destructor.
 * There is nothing to delete in this object.
 *
 * #### `Sg_File_Info::isSg_File_Info (SgNode *s)`
 * Cast function (from derived class to Sg_File_Info pointer).
 * This functions returns a Sg_File_Info pointer for any input of a
 * pointer to an object derived from a Sg_File_Info.
 * - Return: Returns valid pointer to Sg_File_Info if input is derived from a SgLocatedNode.
 *
 * #### `Sg_File_Info::isSg_File_Info (const SgNode *s)`
 * Cast function (from derived class to Sg_File_Info pointer).
 * This functions returns a Sg_File_Info pointer for any input of a
 * pointer to an object derived from a Sg_File_Info.
 * - Return: Returns valid pointer to Sg_File_Info if input is derived from a SgLocatedNode.
 *
 * #### `Sg_File_Info::set_line ( int line )`
 * This function sets the line number of the construct's association with source code
 * in the source file named by p_filename.
 * - Return: Returns an integer.
 *
 * #### `Sg_File_Info::set_filename (char* filename)`
 * This function sets the filename of the construct's association with source code.
 * - Internal: This function should be updated to take and return a string.  Likely we can
 * implement an overloaded function with takes and resturns a string and preserve this
 * function (deprecated) for a period of time.
 * - Return: Returns char* (will return C++ string at some point later)
 *
 * #### `Sg_File_Info::output ()`
 * Output function, deprecated.
 * - Return: Returns void
 * - Deprecated: This function is replaced by the display(char*) function.
 *
 * #### `Sg_File_Info::set_isPartOfTransformation (bool isPartOfTransformation)`
 * This function marks an IR nodes as being part of a transformation, this function is deprecated.
 * - Return: Returns void.
 * - Deprecated: This function is deprecated.
 *
 * #### `Sg_File_Info::get_isPartOfTransformation ()`
 * This function indicates of the IR nodes is part of a transformation.
 * - Return: Returns bool
 * - Deprecated: This function is deprecated.
 *
 * #### `Sg_File_Info::get_filename () const`
 * Returns filename of source code associated with IR node.
 * The filename returned is manipulated under some contitions:
 * - if isTransformation() is true then "transformation" is returned as the filename.
 * - if isCompilerGenerated() is true then "comilerGenerated" is returned as the filename.
 * - if isCompilerGeneratedNodeToBeUnparsed() is true then "comilerGenerated" is returned as the filename.
 * - Internal: For debugging purposes it is an error to ask for the filename from any IR node
 * for which hasPositionInSource() == false.
 * - Return: Returns char* (will return C++ string in the future).
 *
 * #### `Sg_File_Info::get_line () const`
 * Returns the line number of the associated code for this IR node.
 * - Return: Returns integer.
 *
 * #### `Sg_File_Info::get_col () const`
 * Returns the column number of the associated code for this IR node.
 * - Return: Returns integer
 *
 * #### `Sg_File_Info::get_raw_filename () const`
 * Returns filename of source code associated with IR node.
 * The filename is not manipulated! The value of p_filename is returned whatever the IR
 * node classification.
 * - Internal: This is useful for debugging.
 * - Return: Returns a C++ string object.
 *
 * #### `Sg_File_Info::get_raw_line () const`
 * Returns the line number of the associated code for this IR node.
 * - Return: Returns integer.
 *
 * #### `Sg_File_Info::get_raw_col () const`
 * Returns the column number of the associated code for this IR node.
 * - Return: Returns
 *
 * #### `Sg_File_Info::hasPositionInSource() const`
 * No earthly idea what this function does!
 * - Todo: figure out what this does, it appears to be called in two places (attachment of
 * comments and CPP directives (attachPreprocessingInfo.C) and marking template
 * specializations for output (markTemplateSpecializationsForOutput.C)).
 * - Return: Returns bool.
 *
 * #### `Sg_File_Info::generateDefaultFileInfo()`
 * Static function to return new Sg_File_Info object set to default values.
 * - Return: Returns pointer to Sg_File_Info.
 *
 * #### `Sg_File_Info::generateDefaultFileInfoForTransformationNode()`
 * Static function to return new Sg_File_Info object set to default values
 * appropriate for transformations.
 * This function calls setOutputInCodeGeneration() and sets the file_id to be
 * TRANSFORMATION_FILE_ID.
 * - Return: Returns pointer to Sg_File_Info.
 *
 * #### `Sg_File_Info::generateFileInfoForTransformationNode( int file_id )`
 * Static function to return new Sg_File_Info object set to default values appropriate
 * for transformations that are not in the source file.
 * Static function which allows specification of transformation with assignement to
 * a specific file (where it would be unparsed). This function will call
 * setOutputInCodeGeneration() and sets the file_id to a non-negative value.
 * This function is useful when transformation to a program happen within a
 * header file (e.g. including a header file as part of a transformation).
 * Setting the file_id to the current source file should have the same result (semantics)
 * as Sg_File_Info::generateDefaultFileInfoForTransformationNode().
 * - Return: Returns pointer to Sg_File_Info.
 *
 * #### `Sg_File_Info::generateFileInfoForTransformationNode( string filename )`
 * Static function to return new Sg_File_Info object set to default values appropriate
 * for transformations that are not in the source file.
 * Function similar to previous function, but takes a filename to support new files that
 * have not ben seen previously (e.g. new header file).
 * - Return: Returns pointer to Sg_File_Info.
 *
 * #### `Sg_File_Info::generateDefaultFileInfoForCompilerGeneratedNode()`
 * Static function to return new Sg_File_Info object set to default values appropriate for compiler generated code.
 * - Return: Returns pointer to Sg_File_Info.
 *
 * #### `Sg_File_Info::hasPositionInSource() const`
 * Abstracts query as to if an IR node maps back to the source code (evolving set of reasons whey this can be false).
 * There are several reasons why an IR node might NOT have a mapping back to the source code:
 * - could be a part of a transformation (isTransformation() == true)
 * - could be a compiler generated statement or expression (isCompilerGenerated() == true)
 * - could be compiler generated and marked for output by the unparser
 * (isCompilerGeneratedNodeToBeUnparsed() == true)
 * - Internal: This is an evolving set of reasons, so abstracting it as a member function is helpful.
 * - Return: Returns bool.
 *
 * #### `Sg_File_Info::isTransformation() const`
 * Returns true only if part of a transformation.
 * - Return: Returns bool.
 *
 * #### `Sg_File_Info::isCompilerGenerated() const`
 * Returns true only if compiler generated (either by the front-end or by ROSE).
 * - Internal: We do not presently distinguish between compiler generated code from EDG or by
 * ROSE.  For example template instatiations are not marked as compiler generated by EDG,
 * but if ROSE generates specializations from instatioated templates then they are marked
 * by ROSE as being compiler generated.
 * - Return: Returns bool.
 *
 * #### `Sg_File_Info::isCompilerGeneratedNodeToBeUnparsed() const`
 * Returns true only if compiler generated and required to be unparsed in generated code.
 * - Internal: Templates instatiated by ROSE are marked as compiler generated, but only those
 * instatiated templates that are used in the source file are required and so only
 * those must be unparsed in the generated code.
 * - Deprecated: This now calls isOutputInCodeGeneration(), and will be removed soon.
 * - Return: Returns bool.
 *
 * #### `Sg_File_Info::isOutputInCodeGeneration() const`
 * Returns true only if required to be unparsed in generated code.
 * - Internal: Templates instatiated by ROSE are marked as compiler generated, but only those
 * instatiated templates that are used in the source file are required and so only
 * those must be unparsed in the generated code. Note that currently all transformed IR
 * nodes are implicitly considered to be marked as *outputInCodeGeneration,* but this will
 * be made an explicit requirement in the future.
 * - Return: Returns bool.
 *
 * #### `Sg_File_Info::isShared() const`
 * Returns true only if shared internally (either by the front-end or by ROSE).
 * - Internal: This is a new classification supporting the AST merge mechanism.  Currently
 * numerous types of IR nodes that are shared (SgType, SgSymbol, many non-defining
 * declarations, etc.) are not explicitly marked as shared.  This may be correct in
 * a future release (requires more thought).
 * - Return: Returns bool.
 *
 * #### `Sg_File_Info::unsetTransformation()`
 * If the IR node is a transformation it marks it false (zeros transformation marker bit
 * internally).
 * - Return: Returns void.
 *
 * #### `Sg_File_Info::setTransformation()`
 * Marks an IR node to be a transformation if it is not one already.
 * - Return: Returns void.
 *
 * #### `Sg_File_Info::unsetCompilerGenerated()`
 * Unmarks IR node as compiler generated.
 * - Return: Returns void.
 *
 * #### `Sg_File_Info::setCompilerGenerated()`
 * Marks IR node as compiler generated.
 * - Return: Returns void.
 *
 * #### `Sg_File_Info::unsetCompilerGeneratedNodeToBeUnparsed()`
 * Unmarks IR node as compiler generated but required in the generated source (e.g. requied templates).
 * - Deprecated: This now calls unsetOutputInCodeGeneration(), and will be removed soon.
 * - Return: Returns void.
 *
 * #### `Sg_File_Info::setCompilerGeneratedNodeToBeUnparsed()`
 * Marks IR node as compiler generated but required in the generated source (e.g. requied templates).
 * - Deprecated: This now calls setOutputInCodeGeneration(), and will be removed soon.
 * - Return: Returns void.
 *
 * #### `Sg_File_Info::unsetOutputInCodeGeneration()`
 * Unmarks IR node as compiler generated but required in the generated source (e.g. requied templates).
 * - Return: Returns void.
 *
 * #### `Sg_File_Info::setOutputInCodeGeneration()`
 * Marks IR node as compiler generated but required in the generated source (e.g. requied templates).
 * - Return: Returns void.
 *
 * #### `Sg_File_Info::unsetShared()`
 * Unmarks IR node as shared.
 * - Return: Returns void.
 *
 * #### `Sg_File_Info::setShared()`
 * Marks IR node as shared.
 * - Return: Returns void.
 *
 * #### `bool Sg_File_Info::ok()`
 * Checks internal consistancy of data.
 * This function verifies:
 * -# filename pointer is valid
 * -# if not compiler generaated then line number > 0
 * Defined to be used in tests:
 * ROSE_ASSERT(statement->get_file_info()->ok());
 * - Return: Returns bool.
 *
 * #### `Sg_File_Info::display( const string label )`
 * This function outputs the internal data to stdout, for debugging.
 * - Return: Returns void.
 *
 * #### `bool Sg_File_Info::operator== ( const Sg_File_Info & X, const Sg_File_Info & Y )`
 * This relational operator tests two Sg_File_Info objects for equal position information.
 * - Return: Returns bool.
 *
 * #### `bool Sg_File_Info::operator!= ( const Sg_File_Info & X, const Sg_File_Info & Y )`
 * This relational operator tests two Sg_File_Info objects for unequal position information.
 * - Return: Returns bool.
 *
 * #### `Sg_File_Info::operator> ( const Sg_File_Info & X, const Sg_File_Info & Y )`
 * This relational operator tests two Sg_File_Info objects for X being after Y in the
 * same file.
 * - Return: Returns bool.
 *
 * #### `Sg_File_Info::operator< ( const Sg_File_Info & X, const Sg_File_Info & Y )`
 * This relational operator tests two Sg_File_Info objects for X being before Y in the
 * same file.
 * - Return: Returns bool.
 *
 * #### `Sg_File_Info::operator>= ( const Sg_File_Info & X, const Sg_File_Info & Y )`
 * This relational operator tests two Sg_File_Info objects for X being at the same
 * position or after Y in the same file.
 * - Return: Returns bool.
 *
 * #### `Sg_File_Info::operator<= ( const Sg_File_Info & X, const Sg_File_Info & Y )`
 * This relational operator tests two Sg_File_Info objects for X being at the same
 * position or before Y in the same file.
 * - Return: Returns bool.
 */
class Sg_File_Info;

/** @brief This class represents the notion of an value (expression value).
 *
 * - Internal: This class will hold a string value so that the exact text for constants can be
 * help in the AST.
 * - Todo: Add string to this class so that the exact value can be held in the AST.
 * - See also:
 * Example of using a SgFloatVal object
 *
 * **Data members**
 *
 * #### `SgFloatVal::p_value`
 * This value holds the float represented in the source code.
 *
 * **Member functions**
 *
 * #### `SgFloatVal::SgFloatVal ( Sg_File_Info* startOfConstruct = NULL )`
 * This is the constructor.
 * This constructor builds the SgFloatVal base class.
 * - Param `startOfConstruct`: represents the position in the source code
 * - See also:
 * Example:create an SgFloatVal object
 *
 * #### `SgFloatVal::~SgFloatVal()`
 * This is the destructor.
 * Only the Sg_File_Info object can be deleted in this object.
 */
class SgFloatVal;

/** @brief This class represents the variable declaration or variable initialization withn a
 *
 * for loop.
 * This class is only used in a SgForStatement; and represents "*" within
 * "for(*; <condition>; <expression>)".
 * - Internal: The design of the ROSE traversal mechanism requires that the child list at any
 * IR node be a list or a collection of data members (but not both, since it would not be
 * clear when the list ended and another list or data member begain).  this class is
 * required so that list of initializers in a for loop (an arbitary length list) can
 * be isolated from the other data members in the SgForStatement.
 * - Todo: Evaluate if this should be derived from SgSupport, like other "list" based IR nodes.
 * - Todo: Evaluate if we should even have this IR node.  If the SgVariableDeclaration were to
 * be fixed to really use the list of SgInitializedName objects where multiple variables
 * are declared in the same variable declaration then we might not need this (I think).
 * And if it didn't exist it would make the use of the SgForStatement a little bit
 * simpler.
 * - See also:
 * Example of using a SgForInitStatement object
 *
 * **Data members**
 *
 * #### `SgForInitStatement::p_init_stmt`
 * This pointer points to list of initializers in a SgForStatement.
 *
 * **Member functions**
 *
 * #### `SgForInitStatement::SgForInitStatement ( Sg_File_Info* startOfConstruct = NULL )`
 * This is the constructor.
 * This constructor builds the SgForInitStatement base class.
 * - Param `startOfConstruct`: represents the position in the source code
 *
 * #### `SgForInitStatement::~SgForInitStatement()`
 * This is the destructor.
 * There are a lot of things to delete, but nothing is deleted in this destructor.
 *
 * #### `SgForInitStatement::isSgForInitStatement (SgNode *s)`
 * Cast function (from derived class to SgForInitStatement pointer).
 * This functions returns a SgForInitStatement pointer for any input of a
 * pointer to an object derived from a SgForInitStatement.
 * - Return: Returns valid pointer to SgForInitStatement if input is derived from a SgLocatedNode.
 *
 * #### `SgForInitStatement::isSgForInitStatement (const SgNode *s)`
 * Cast function (from derived class to SgForInitStatement pointer).
 * This functions returns a SgForInitStatement pointer for any input of a
 * pointer to an object derived from a SgForInitStatement.
 * - Return: Returns valid pointer to SgForInitStatement if input is derived from a SgLocatedNode.
 *
 * #### `SgForInitStatement::get_init_stmt() const`
 * Returns const reference to a SgStatementPtrList (typedef to a STL list).
 * - Return: Returns const reference to STL list.
 *
 * #### `SgForInitStatement::get_init_stmt()`
 * Returns non-const reference to a SgStatementPtrList (typedef to a STL list).
 * - Return: Returns non-const reference to STL list.
 */
class SgForInitStatement;

/** @brief This class represents the concept of a for loop.
 *
 * - Internal:
 * - Todo: The conditional in this test is currently an expression, but should be a
 * SgConditional or a SgStatement (e.g. so that it can be a variable declaration).
 * - Todo: Now that the test is a SgStatement, perhaps the name of the field should be
 * "test" instead of "test_expr".
 *
 * **Data members**
 *
 * #### `SgForStatement::p_for_init_stmt`
 * This pointer a SgForInitStatement (a list of pointers to statements (SgStatement objects) ).
 * - Note: I think this is a poor name for this variable.
 *
 * #### `SgForStatement::p_test_expr_root`
 * This pointer a SgExpression (a list of pointers to statements (SgStatement objects) ).
 * - Note: I think this is a poor name for this variable.
 * - Todo: This should be changed to be a SgStatement (to follow the C++ standard).
 * this way it could be an expression (via an expression statement) or a variable
 * declaration with initializer (via a SgVariableDeclaration).
 * We also have the condition specified before the body within the ROSETTA
 * specification and this causes the traversal to travers the condition and body in the
 * wrong order (for do-while, the traversal should be body forst and condition second).
 * See test2005_114.C for more details and example code (example of strange loops).
 *
 * #### `SgForStatement::p_increment_expr_root`
 * This pointer a SgExpression (a list of pointers to statements (SgStatement objects) ).
 * - Note: I think this is a poor name for this variable.
 *
 * #### `SgForStatement::p_loop_body`
 * This pointer a SgBasicBlock, and holds the statements in the body of the loop.
 * - Note: I think this is a poor name for this variable (should be "body" to match other IR
 * nodes).
 * - Todo: Change "loop_body" to "body" to be consistant with other scopes that contain a SgBasicBlock.
 * /*! \var SgForStatement::p_else_body
 * Holds the statements in the else body of the loop. Currently, only Python supports for loops with else bodies.
 *
 * **Member functions**
 *
 * #### `SgForStatement::SgForStatement ( Sg_File_Info* startOfConstruct = NULL )`
 * This is the constructor.
 * This constructor builds the SgForStatement base class.
 * - Param `startOfConstruct`: represents the position in the source code
 *
 * #### `SgForStatement::~SgForStatement()`
 * This is the destructor.
 * There are a lot of things to delete, but nothing is deleted in this destructor.
 *
 * #### `SgForStatement::isSgForStatement (SgNode *s)`
 * Cast function (from derived class to SgForStatement pointer).
 * This functions returns a SgForStatement pointer for any input of a
 * pointer to an object derived from a SgForStatement.
 * - Return: Returns valid pointer to SgForStatement if input is derived from a SgLocatedNode.
 *
 * #### `SgForStatement::isSgForStatement (const SgNode *s)`
 * Cast function (from derived class to SgForStatement pointer).
 * This functions returns a SgForStatement pointer for any input of a
 * pointer to an object derived from a SgForStatement.
 * - Return: Returns valid pointer to SgForStatement if input is derived from a SgLocatedNode.
 *
 * #### `SgForStatement::copy(const SgCopyHelp & help)`
 * Makes a copy (deap of shallow depending on SgCopyHelp).
 * - Return: Returns pointer to copy of SgForStatement.
 *
 * #### `SgForStatement::get_for_init_stmt() const`
 * Access function for p_for_init_stmt.
 * - Return: Returns a pointer to a SgForInitStatement.
 *
 * #### `SgForStatement::set_for_init_stmt(SgForInitStatement* for_init_stmt)`
 * Access function for p_for_init_stmt.
 * - Param `for_init_stmt`: SgForInitStatement pointer
 * - Return: Returns void.
 *
 * #### `SgForStatement::get_test_expr() const`
 * Access function for p_test_expr_root.
 * - Return: Returns a pointer to a SgExpression.
 *
 * #### `SgForStatement::set_test_expr(SgExpression* test_expr)`
 * Access function for p_test_expr_root.
 * - Param `test_expr`: SgExpression pointer
 * - Return: Returns void.
 *
 * #### `SgForStatement::get_increment_expr() const`
 * Access function for p_increment_expr.
 * - Return: Returns a pointer to a SgExpression.
 *
 * #### `SgForStatement::set_increment_expr(SgExpression* increment_expr)`
 * Access function for p_increment_expr.
 * - Param `increment_expr`: SgExpression pointer
 * - Return: Returns void.
 *
 * #### `SgForStatement::get_loop_body() const`
 * Access function for p_loop_body.
 * - Return: Returns a pointer to a SgBasicBlock.
 *
 * #### `SgForStatement::set_loop_body(SgBasicBlock* loop_body)`
 * Access function for p_loop_body.
 * - Param `loop_body`: SgBasicBlock pointer
 * - Return: Returns void.
 */
class SgForStatement;

/** @brief This class represents the concept of a C++ function call (which is an expression).
 *
 * Note that a function call is assembled from a function expression and arguments.
 * The two are bound together in a function call, function expressions are never called
 * directly (the function call argument list can be empty).
 * - Internal: When overloaded operators are used the function name becomes the name of the
 * operator (e.g. "operator+").  In these cases the precedence of the operator is that
 * of the operator being overloaded and is different from a normal function call.  Most
 * of this detail is hidden since operands in the expression tree are represented by functions
 * with parameter lists (represented by a SgExprListExp), and not simple expression trees.
 * - See also:
 * Example of using a SgFunctionCallExp object
 *
 * **Member functions**
 *
 * #### `SgFunctionCallExp::SgFunctionCallExp ( Sg_File_Info* startOfConstruct = NULL, SgClassSymbol *symbol = NULL )`
 * This is the constructor.
 * This constructor builds the SgFunctionCallExp base class.
 * - Param `startOfConstruct`: represents the position in the source code
 *
 * #### `SgFunctionCallExp::~SgFunctionCallExp()`
 * This is the destructor.
 * There are a lot of things to delete, but nothing is deleted in this destructor.
 *
 * #### `SgFunctionCallExp::isSgFunctionCallExp (SgNode *s)`
 * Cast function (from derived class to SgFunctionCallExp pointer).
 * This functions returns a SgFunctionCallExp pointer for any input of a
 * pointer to an object derived from a SgFunctionCallExp.
 * - Return: Returns valid pointer to SgFunctionCallExp if input is derived from a SgLocatedNode.
 *
 * #### `SgFunctionCallExp::isSgFunctionCallExp (const SgNode *s)`
 * Cast function (from derived class to SgFunctionCallExp pointer).
 * This functions returns a SgFunctionCallExp pointer for any input of a
 * pointer to an object derived from a SgFunctionCallExp.
 * - Return: Returns valid pointer to SgFunctionCallExp if input is derived from a SgLocatedNode.
 */
class SgFunctionCallExp;

/** @brief This class represents the concept of a function declaration statement.
 *
 * A function declaration can be either a forward declaration or a defining declaration.
 * If it is a defining declaration then it contains a pointer to the function definition.
 * Future work will allow this class to have a valide defining and non-defining declaration
 * similar to the class declaration, but this is not yet implemented.
 * This class is used as a base class for member function declarations and both template
 * functions and template member function declarations.
 * The name of the function is stored in "p_name". However, if the function is a constructor
 * or destructor the value of "p_name" is ignored (should be unset, perhaps) and the name is
 * generated from the unqualified class name.  This handles the complexity of resetting
 * templated names which likely have been stored in the constructor before the template
 * names have been reset from the form "ABC____L8" to "ABC<int>" as requied to compile the
 * unparsed code.
 * - Internal: This is a base class for all function and member declaration statments
 * (including templated functions and member functions).
 * - Todo: Need to mark function declarations appearing in the file
 * rose_edg_required_macros_and_functions.h as compiler generated
 * since they are either builtin functions for gcc and g++ or those those
 * builtin function that gcc and g++ required and which EDG fails to include
 * as builtin when compiling with EDG's GNU_COMPATABILITY_MODE (current
 * default for ROSE).
 * - Todo: Need to better handle fiend injection rules, currently the SgFunctionSymbol
 * for a friend function is placed into the global scope.  It likely should be
 * the outer scope for a non-defining declaration and the class scope for a defining
 * declaration.  But the exact rules for this are more complex.  So the location
 * of the SgFunctionSymbol in the symbol table of SgGlobal is a poor approximation.
 *
 * **Data members**
 *
 * #### `SgFunctionDeclaration::p_name`
 * This variable stores the string representing the function name.
 * This variable stores the string representing the function name.  The value is
 * ignored in the case of a member function constructor or destructor (where upon the
 * name returned from get_name() is generated from the class name). This handling is
 * important for constructors and constructor initializer handling associated with
 * instantiated template class declarations).
 * The name of the function is stored in "p_name". However, if the function is a constructor
 * or destructor the value of "p_name" is ignored (should be unset, perhaps) and the name is
 * generated from the unqualified class name.  This handles the complexity of resetting
 * templated names which likely have been stored in the constructor before the template
 * names have been reset from the form "ABC____L8" to "ABC<int>" as requied to compile the
 * unparsed code.
 * - Internal: The value held is not shared within the Sage III AST.
 *
 * #### `SgFunctionDeclaration::p_args`
 * This variable stores the function parameters (as declared).
 * This variable stores the function parameters. The names and types are
 * stored as they are declared so different for old K&R C style from newer C
 * style (both of which are acdeptable within ANSI C!).
 *
 * #### `SgFunctionDeclaration::p_functionModifier`
 * This variable stores flags representing use of inline, virtual, etc.
 * This variable stores what the C++ grammar referes to as "function modifiers".
 * Values held here are: inline, virtual, pure virtual, default, explicit.
 *
 * #### `SgFunctionDeclaration::p_specialFunctionModifier`
 * This variable stores flags representing use of constructor, destructor, etc.
 * This variable stores what the C++ grammar referes to as "special function modifiers".
 * Values held here are: default, not special, constructor, destructor, conversion operator,
 * operator.
 *
 * #### `SgFunctionDeclaration::p_type`
 * This variable stores the SgFunctionType.
 * This variable stores the SgFunctionType.  All types are shared within
 * the Sage III AST.
 *
 * #### `SgFunctionDeclaration::p_decoratorList`
 * This variable stores a list of decorators.
 * This variable stores a list of expressions that decorate this function. This member
 * is intended for use with Python, and should be NULL otherwise.
 *
 * #### `SgFunctionDeclaration::p_forwardDefinition`
 * This variable stores the SgFunctionDefinition.
 * This variable stores the SgFunctionDefinition. It is presently NULL if
 * the declaration is a forward declaration.  This will be made more uniform
 * with how defining vs. non-defining declarations are handled in the
 * SgClassDeclaration.
 * - Deprecated: This is now redundant with the more general mechanism of defining and
 * non-defining declarations which is implemented at the SgDeclaration level.
 * - Internal: The handling via defining and non-defining declaration for function declarations
 * is not yet implemented and will be implemented similar to that done for class
 * declarations.
 *
 * #### `SgFunctionDeclaration::p_definition`
 * This variable stores the SgFunctionDefinition.
 * This variable stores the SgFunctionDefinition. It is presently NULL if
 * the declaration is a forward declaration.  This will be made more uniform
 * with how defining vs. non-defining declarations are handled in the
 * SgClassDeclaration.
 * - Deprecated: This is now redundant with the more general mechanism of defining and
 * non-defining declarations which is implemented at the SgDeclaration level.
 * - Internal: The handling via defining and non-defining declaration for function declarations
 * is not yet implemented and will be implemented similar to that done for class
 * declarations.
 *
 * #### `SgFunctionDeclaration::p_definition_ref`
 * This variable stores the SgFunctionDefinition.
 * This variable stores the SgFunctionDefinition. It is presently NULL if
 * the declaration is a forward declaration.  This will be made more uniform
 * with how defining vs. non-defining declarations are handled in the
 * SgClassDeclaration.
 * - Deprecated: This is now redundant with the more general mechanism of defining and
 * non-defining declarations which is implemented at the SgDeclaration level.
 * - Internal: The handling via defining and non-defining declaration for function declarations
 * is not yet implemented and will be implemented similar to that done for class
 * declarations.
 *
 * #### `SgFunctionDeclaration::p_mangled_name`
 * This variable stores the mangled name.
 * This variable stores the string representing the mangled function name.
 * This name is unique within the AST (as required by C and C++ language definition).
 *
 * #### `SgFunctionDeclaration::p_orig_return_type`
 * This variable stores the SgType pointer representing the function return type.
 * This variable stores the function return type (SgType).  All types are shared within
 * the Sage III AST.  Not clear why it is called the "original return type" since functions
 * can't be overloaded upon their return type (virtual or otherwise).
 *
 * #### `SgFunctionDeclaration::p_from_template`
 * This boolean variable records if the function originally came from a template
 * function (now largely redundant information).
 * This boolean variable records if the function originally came from a template
 * function (now largely redundant information).  The IR now has a special type for
 * functions that are instantiated from function templates.
 * - Deprecated: This variable will likely be removed in the future.
 * - Internal: This value is ALWAYS true for a SgTemplateInstantiationFunctionDecl and
 * SgTemplateInstantiationMemberFunctionDecl; otherwise it is ALWAYS false.
 *
 * #### `SgFunctionDeclaration::p_oldStyleDefinition`
 * This boolean variable records if the function uses the old style definition.
 * This boolean variable records if the function was declared using the old style
 * definition.
 * - Internal: This should maybe be moved to the SgFunctionParameterList class.
 *
 * **Member functions**
 *
 * #### `SgFunctionDeclaration::SgFunctionDeclaration ( Sg_File_Info* startOfConstruct = NULL )`
 * This is the constructor.
 * This constructor builds the SgFunctionDeclaration base class.
 * - Param `startOfConstruct`: represents the position in the source code
 *
 * #### `SgFunctionDeclaration::~SgFunctionDeclaration()`
 * This is the destructor.
 * There are a lot of things to delete, but nothing is deleted in this destructor.
 *
 * #### `SgFunctionDeclaration::isSgFunctionDeclaration (SgNode *s)`
 * Cast function (from derived class to SgFunctionDeclaration pointer).
 * This functions returns a SgFunctionDeclaration pointer for any input of a
 * pointer to an object derived from a SgFunctionDeclaration.
 * - Return: Returns valid pointer to SgFunctionDeclaration if input is derived from a SgLocatedNode.
 *
 * #### `SgFunctionDeclaration::isSgFunctionDeclaration (const SgNode *s)`
 * Cast function (from derived class to SgFunctionDeclaration pointer).
 * This functions returns a SgFunctionDeclaration pointer for any input of a
 * pointer to an object derived from a SgFunctionDeclaration.
 * - Return: Returns valid pointer to SgFunctionDeclaration if input is derived from a SgLocatedNode.
 *
 * #### `SgFunctionDeclaration::isTemplateFunction () const`
 * Determines if function is a template or non-template function.
 * This function returns true if the current function is explicitly declared to be a
 * template function. It returns fals if it is a normal function, member function, or
 * member function of a templated class.
 * - Internal: This function is required because member function of template are represented by
 * SgTemplateInstantiationMemberFunction IR nodea the same as explicitly templated
 * member functions.  This function helps distinguish between the two types and is
 * used in determining which functions to output as template specializations (which
 * is now ROSE puts out template instantiations that it generates).
 * - Return: Returns bool value.
 */
class SgFunctionDeclaration;

/** @brief This class represents the concept of a scope in C++ (e.g. global scope, fuction scope, etc.).
 *
 * Scopes are an important aspect of language design. They allow
 * declarations to have a local context and so promote good programming style.
 * Scope statments in C++ include a number of different kinds of statements;
 * the SgFunctionDefinition is a base class for these. Each scope statement contains
 * a symbol table and the SgFunctionDefinitions role is mostly to provide this
 * symbol table and an interface to accessing it.
 * - Internal: This is a base class for scope statements.
 *
 * **Data members**
 *
 * #### `SgFunctionDefinition::p_body`
 * This pointer is always valid and points to a SgBasicBlock holding all the
 * statements in the function.
 * - Internal:
 *
 * #### `SgFunctionDefinition::p_par_flag`
 * This is a bool value left over from CC++.
 * - Internal: This can be removed at some point.
 * - Deprecated: This variable is left over from CC++ and can be removed.
 *
 * **Member functions**
 *
 * #### `SgFunctionDefinition::SgFunctionDefinition ( Sg_File_Info* startOfConstruct = NULL )`
 * This is the constructor.
 * This constructor builds the SgFunctionDefinition base class.
 * - Param `startOfConstruct`: represents the position in the source code
 *
 * #### `SgFunctionDefinition::~SgFunctionDefinition()`
 * This is the destructor.
 * There are a lot of things to delete, but nothing is deleted in this destructor.
 *
 * #### `SgFunctionDefinition::isSgFunctionDefinition (SgNode *s)`
 * Cast function (from derived class to SgFunctionDefinition pointer).
 * This functions returns a SgFunctionDefinition pointer for any input of a
 * pointer to an object derived from a SgFunctionDefinition.
 * - Return: Returns valid pointer to SgFunctionDefinition if input is derived from a SgLocatedNode.
 *
 * #### `SgFunctionDefinition::isSgFunctionDefinition (const SgNode *s)`
 * Cast function (from derived class to SgFunctionDefinition pointer).
 * This functions returns a SgFunctionDefinition pointer for any input of a
 * pointer to an object derived from a SgFunctionDefinition.
 * - Return: Returns valid pointer to SgFunctionDefinition if input is derived from a SgLocatedNode.
 *
 * #### `SgFunctionDefinition::get_qualified_name() const`
 * Returns SgName (a string) representing the name of the current scope.
 * See discussion of mangled names in the documentation.
 * - Return: Returns SgName (a string).
 *
 * #### `SgFunctionDefinition::copy(const SgCopyHelp & help)`
 * Makes a copy (deap of shallow depending on SgCopyHelp).
 * - Return: Returns pointer to copy of SgFunctionDefinition.
 *
 * #### `SgFunctionDefinition::get_body() const`
 * Access function for p_body.
 * - Return: Returns a pointer to a SgBasicBlock.
 *
 * #### `SgFunctionDefinition::set_body(SgBasicBlock* body)`
 * Access function for p_body.
 * - Param `body`: SgBasicBlock pointer
 * - Return: Returns void.
 */
class SgFunctionDefinition;

/** @brief This class represents the concept of a declaration list.
 *
 * This class is used in the function declaration IR node (SgFunctionDeclaration).
 * - Internal: This class is separated as its own IR node so that the traversals can
 * have either a list or a collection on non-list data members.
 * - Todo: Check scopes of variables in function parameter list, should point to function
 * definition, if the function definition exists, else they are undefined. If they
 * are undefined then we still have to have something for them to point to, we could
 * propose that this be the scope of the function declaration (I think this is what is
 * done).  The test in the tutorial tests this and it seems to be correct.
 * - Todo: Not clear if this should be a declaration statement (might make more sense derived
 * from SgSupport, or perhaps from SgLocatedNode (with other IR nodes that are currently
 * derived from SgSupport, see SgLocatedNode for details).
 * - Todo: If this should be a SgDeclarationStatement (and there is a reasonable argument for
 * this) then perhaps the declaration containing any default parameters should be the
 * defining declaration, independent of the defining declaration of the associated
 * function declaration.
 * - See also:
 * Example of using a SgFunctionParameterList object
 *
 * **Data members**
 *
 * #### `SgFunctionParameterList::p_args`
 * STL list of pointers to SgInitializedName object (used for function parameter declarations).
 *
 * **Member functions**
 *
 * #### `SgFunctionParameterList::SgFunctionParameterList ( Sg_File_Info* startOfConstruct = NULL )`
 * This is the constructor.
 * This constructor builds the SgFunctionParameterList base class.
 * - Param `startOfConstruct`: represents the position in the source code
 *
 * #### `SgFunctionParameterList::~SgFunctionParameterList()`
 * This is the destructor.
 * There are a lot of things to delete, but nothing is deleted in this destructor.
 *
 * #### `SgFunctionParameterList::isSgFunctionParameterList (SgNode *s)`
 * Cast function (from derived class to SgFunctionParameterList pointer).
 * This functions returns a SgFunctionParameterList pointer for any input of a
 * pointer to an object derived from a SgFunctionParameterList.
 * - Return: Returns valid pointer to SgFunctionParameterList if input is derived from a SgLocatedNode.
 *
 * #### `SgFunctionParameterList::isSgFunctionParameterList (const SgNode *s)`
 * Cast function (from derived class to SgFunctionParameterList pointer).
 * This functions returns a SgFunctionParameterList pointer for any input of a
 * pointer to an object derived from a SgFunctionParameterList.
 * - Return: Returns valid pointer to SgFunctionParameterList if input is derived from a SgLocatedNode.
 *
 * #### `SgFunctionParameterList::get_args() const`
 * Access function for p_args.
 * - Return: Returns a const reference to SgInitializedNamePtrList.
 *
 * #### `SgFunctionParameterList::get_args()`
 * Access function for p_args.
 * - Return: Returns reference to SgInitializedNamePtrList.
 */
class SgFunctionParameterList;

/** @brief This class represents the function being called and must be assembled in the
 *
 * SgFunctionCall with the function arguments.
 * - Internal: When used in a SgFunctionCallExp (most common, or typical case (and maybe the
 * only case)) the SgExpression is one of the following:
 * - SgDotExp
 * - SgDotStarOp
 * - SgArrowExp
 * - SgArrowStarOp
 * - SgPointerDerefExp
 * - SgFunctionRefExp
 * - SgMemberFunctionRefExp
 * - Todo: Figure out why SgMemberFunctionRefExp is required instead of just SgFunctionRefExp.
 * - Todo: Make the use of a SgMemberFunctionSymbol in a SgFunctionRefExp an error. The result
 * will not unparse correctly (suggested by Jeremiah).
 * - See also:
 * Example of using a SgFunctionRefExp object
 *
 * **Data members**
 *
 * #### `SgFunctionRefExp::p_lvalue`
 * This boolean variable marks the current expression as a
 * left hand side value (lvalue).
 *
 * #### `SgFunctionRefExp::p_need_paren`
 * This boolean value marks the current expression as requiring parenthises.
 * This boolean value marks the current expression as requiring parenthises (the
 * information comes from the frontend's interpretation of the requirement and is
 * almost always overly conservative.  The unparser currently backs out more
 * accurate rules based on operator precedence and removed then where they
 * are not truely required.  Thus the purpose of this variable is to capture the
 * interpritation of the frontend regarding the use of parenthesis.
 *
 * **Member functions**
 *
 * #### `SgFunctionRefExp::SgFunctionRefExp ( Sg_File_Info* startOfConstruct = NULL )`
 * This is the constructor.
 * This constructor builds the SgFunctionRefExp base class.
 * - Param `startOfConstruct`: represents the position in the source code
 * - See also:
 * Example:create an SgFunctionRefExp object
 *
 * #### `SgFunctionRefExp::~SgFunctionRefExp()`
 * This is the destructor.
 * Only the Sg_File_Info object can be deleted in this object.
 *
 * #### `SgFunctionRefExp::isSgFunctionRefExp (SgNode *s)`
 * Cast function (from derived class to SgFunctionRefExp pointer).
 * This functions returns a SgFunctionRefExp pointer for any input of a
 * pointer to an object derived from a SgFunctionRefExp.
 * - Return: Returns valid pointer to SgFunctionRefExp if input is derived from a SgFunctionRefExp.
 *
 * #### `SgFunctionRefExp::isSgFunctionRefExp (const SgNode *s)`
 * Cast function (from derived class to SgFunctionRefExp pointer).
 * This functions returns a SgFunctionRefExp pointer for any input of a
 * pointer to an object derived from a SgFunctionRefExp.
 * - Return: Returns valid pointer to SgFunctionRefExp if input is derived from a SgFunctionRefExp.
 *
 * #### `SgFunctionRefExp::get_type() const`
 * Get the type associated with this expression
 * Note that the return value is either:
 * -# SgFunctionType : normal function call
 * -# SgMemberFunctionType : normal member function call
 * -# SgTypedefType : in teh case of a function call from a pointer
 * It should always be a vailid pointer.  These details are verified in the AST
 * Consistancy Tests.
 * - Return: Returns SgType (but not any SgType).
 *
 * #### `SgFunctionRefExp::set_type()`
 * Set the type associated with this expression
 * This is an internally called function, it sets up the type of the expression
 * based upon the types of the subexpressions (if any). Thus it takes no
 * arguments.
 * - Return: Returns void
 */
class SgFunctionRefExp;

/** @brief This class represents a type for all functions.
 *
 * Note that covariant return types of virtual functions are permited in C++,
 * thus this class has explicit support for the function return type and the
 * original function's return type.
 * - Internal: This type is derived from to build member function types
 * (SgMemberFunctionType), partial function types (SgPartialFunctionType),
 * and also (SgPartialFunctionModifierType and SgUnknownMemberFunctionType).
 * - See also:
 * Example of using a SgFunctionType object
 *
 * **Data members**
 *
 * #### `SgBasicBlock::p_argument_list`
 * This points to the SgTypes used in the function's parameter list.
 *
 * #### `SgBasicBlock::p_orig_return_type`
 * This points to the SgType of the original function type's return type.
 *
 * #### `SgBasicBlock::p_return_type`
 * This points to the SgType of the current function type's return type.
 *
 * #### `SgBasicBlock::p_has_ellipses`
 * This boolean variable is true if the function use the "..." type
 * (support for variable number of parameters).
 *
 * **Member functions**
 *
 * #### `SgFunctionType::SgFunctionType()`
 * This is the constructor.
 * This constructor builds the SgFunctionType base class.
 * - See also:
 * Example:create an SgFunctionType object
 *
 * #### `SgFunctionType::~SgFunctionType()`
 * This is the destructor.
 * There is nothing to delete in this object.
 *
 * #### `SgFunctionType::isSgFunctionType (SgNode *s)`
 * Cast function (from derived class to SgFunctionType pointer).
 * This functions returns a SgFunctionType pointer for any input of a
 * pointer to an object derived from a SgFunctionType.
 * - Return: Returns valid pointer to SgFunctionType if input is derived from a SgLocatedNode.
 *
 * #### `SgFunctionType::isSgFunctionType (const SgNode *s)`
 * Cast function (from derived class to SgFunctionType pointer).
 * This functions returns a SgFunctionType pointer for any input of a
 * pointer to an object derived from a SgFunctionType.
 * - Return: Returns valid pointer to SgFunctionType if input is derived from a SgFunctionType node.
 */
class SgFunctionType;

/** @brief This class represents the concept of a name and a type. It may be
 *
 * renamed in the future to SgTypeSymbol (since it is ued for both functions
 * types and more general types).
 * This IR node is used as both the symbol in the SgFunctionTypeTable
 * and in the SgTypeTable (both IR nodes containing a SgSymbolTable IR node).
 * - Todo: Consider derivation of SgEnumSymbol, SgClassSymbol, SgFunctionTypeSymbol, and
 * SgTypedefSymbol from a common SgTypeSymbol.  Then supporting functions for
 * SgTypeSymbol would lookup any of these type based symbols.
 *
 * **Member functions**
 *
 * #### `SgFunctionTypeSymbol::SgFunctionTypeSymbol()`
 * This is the default constructor.
 * This constructor builds the SgFunctionTypeSymbol base class.
 * - See also:
 * Example:create an SgFunctionTypeSymbol object
 *
 * #### `SgFunctionTypeSymbol::~SgFunctionTypeSymbol()`
 * This is the destructor.
 *
 * #### `SgFunctionTypeSymbol::get_name() const`
 * Access function for getting name from declarations or types internally.
 * - Internal: This is a virtual function.
 * - Return: Returns SgName.
 *
 * #### `SgFunctionTypeSymbol::get_type() const`
 * This function returns the type associated with the named entity.
 * - Internal: This is a virtual function.
 * - Return: Returns SgType*.
 *
 * #### `SgFunctionTypeSymbol::isSgFunctionTypeSymbol (SgNode *s)`
 * Cast function (from derived class to SgFunctionTypeSymbol pointer).
 * This functions returns a SgFunctionTypeSymbol pointer for any input of a
 * pointer to an object derived from a SgFunctionTypeSymbol.
 * - Return: Returns valid pointer to SgFunctionTypeSymbol if input is derived from a SgFunctionTypeSymbol.
 *
 * #### `SgFunctionTypeSymbol::isSgFunctionTypeSymbol (const SgNode *s)`
 * Cast function (from derived class to SgFunctionTypeSymbol pointer).
 * This functions returns a SgFunctionTypeSymbol pointer for any input of a
 * pointer to an object derived from a SgFunctionTypeSymbol.
 * - Return: Returns valid pointer to SgFunctionTypeSymbol if input is derived from a SgFunctionTypeSymbol.
 */
class SgFunctionTypeSymbol;

/** @brief This class represents the function type table (stores all function types so that
 *
 * they can be shared internally).
 * This class is a wrapper for the SgSymbolTable and is resticted to the handling of
 * function type symbols.  There is one (global) object of this type used in the compilation
 * of any project using ROSE.
 * - Internal: The global function type symbol table is a static data member (a pointer to a
 * SgFunctionTypeTable), (i.e. SgFunctionTypeTable* SgNode::p_globalFunctionTypeTable;).
 * The static data member (pointer) is generated by ROSETTA in
 * ROSE/src/frontend/SageIII/Cxx_Grammar.C, but the access function are specially built
 * (not by ROSETTA).
 * - Todo: Evaluate if this should be derived from SgSupport (consistant with SgSymbolTable).
 * - Todo: Evaluate if we might like to have the p_function_type_table be a SgSymbolTable rather than
 * a pointer to a SgSymbolTable (see implementation note).
 * - See also:
 * Example of using a SgFunctionTypeTable object
 *
 * **Data members**
 *
 * #### `SgFunctionTypeTable::p_function_type_table`
 * This pointer points to SgSymbolTable used to store function type symbols only.
 * - Internal: The p_function_type_table could alternatively be a data member of SgSymbolTable rather than
 * a pointer to a SgSymbolTable.  However this would require that the class definition
 * for SgSymbolTable be seen ahead of that for this class and the desing of the code
 * generator for the IR (ROSETTA) tries to remove such ordering dependence.  This iw why
 * it is implemented the current way.  It might at some point be worth changing.
 *
 * **Member functions**
 *
 * #### `SgFunctionTypeTable::SgFunctionTypeTable ( Sg_File_Info* startOfConstruct = NULL )`
 * This is the constructor.
 * This constructor builds the SgFunctionTypeTable base class.
 * - Param `startOfConstruct`: represents the position in the source code
 *
 * #### `SgFunctionTypeTable::~SgFunctionTypeTable()`
 * This is the destructor.
 * There are a lot of things to delete, but nothing is deleted in this destructor.
 *
 * #### `SgFunctionTypeTable::isSgFunctionTypeTable (SgNode *s)`
 * Cast function (from derived class to SgFunctionTypeTable pointer).
 * This functions returns a SgFunctionTypeTable pointer for any input of a
 * pointer to an object derived from a SgFunctionTypeTable.
 * - Return: Returns valid pointer to SgFunctionTypeTable if input is derived from a SgLocatedNode.
 *
 * #### `SgFunctionTypeTable::isSgFunctionTypeTable (const SgNode *s)`
 * Cast function (from derived class to SgFunctionTypeTable pointer).
 * This functions returns a SgFunctionTypeTable pointer for any input of a
 * pointer to an object derived from a SgFunctionTypeTable.
 * - Return: Returns valid pointer to SgFunctionTypeTable if input is derived from a SgLocatedNode.
 *
 * #### `SgFunctionTypeTable::get_function_type_table() const`
 * Returns pointer to SgSymbolTable used for function type symbols only.
 * - Return: Returns pointer to SgSymbolTable.
 *
 * #### `SgFunctionTypeTable::set_function_type_table(SgSymbolTable* function_type_table)`
 * Access function for p_function_type_table.
 * - Return: Returns void.
 */
class SgFunctionTypeTable;

/** @brief This class represents the concept of a namespace definition.
 *
 * Namespace definitions
 * are coupled with namespace declarations to defin the namespace (hold the list of
 * declarations in the namespace.  Within C++ namespaces are "reentrant" and
 * as a result multiple namespace declarations (SgNamespaceDeclarationStatement)
 * and definitions (SgGlobal) may exist for a single namespace.
 * - Note: Note that the namespace "std" is special in C++, such that a program
 * with the statement "using namespace std" can exist all by itself and is a
 * valid program (even though "std" as a namespace is not defined).
 * Scopes are an important aspect of language design. They allow
 * declarations to have a local context and so promote good programming style.
 * Scope statments in C++ include a number of different kinds of statements;
 * the SgGlobal is a base class for these. Each scope statement contains
 * a symbol table and the SgGlobals role is mostly to provide this
 * symbol table and an interface to accessing it.
 * - Internal: This is a base class for scope statements.
 * - Todo: Cleanup interface which presently has multiple append,prepend, insert functions. It
 * might be best to eliminate them and use STL directly.
 *
 * **Data members**
 *
 * #### `SgGlobal::p_declarations`
 * This is an STL list of SgDeclarationStatement objects.
 * - Internal: The name is perhaps all too similar to p_declaration (which exists on many IR nodes).
 *
 * **Member functions**
 *
 * #### `SgGlobal::SgGlobal ( Sg_File_Info* startOfConstruct = NULL )`
 * This is the constructor.
 * This constructor builds the SgGlobal base class.
 * - Param `startOfConstruct`: represents the position in the source code
 *
 * #### `SgGlobal::~SgGlobal()`
 * This is the destructor.
 * There are a lot of things to delete, but nothing is deleted in this destructor.
 *
 * #### `SgGlobal::isSgGlobal (SgNode *s)`
 * Cast function (from derived class to SgGlobal pointer).
 * This functions returns a SgGlobal pointer for any input of a
 * pointer to an object derived from a SgGlobal.
 * - Return: Returns valid pointer to SgGlobal if input is derived from a SgLocatedNode.
 *
 * #### `SgGlobal::isSgGlobal (const SgNode *s)`
 * Cast function (from derived class to SgGlobal pointer).
 * This functions returns a SgGlobal pointer for any input of a
 * pointer to an object derived from a SgGlobal.
 * - Return: Returns valid pointer to SgGlobal if input is derived from a SgLocatedNode.
 *
 * #### `SgGlobal::get_qualified_name() const`
 * Returns SgName (a string) representing the name of the current scope (empty string).
 * See discussion of mangled names in the documentation.
 * - Return: Returns SgName (a string).
 *
 * #### `SgGlobal::copy(const SgCopyHelp & help)`
 * Makes a copy (deap of shallow depending on SgCopyHelp).
 * - Return: Returns pointer to copy of SgGlobal.
 *
 * #### `SgGlobal::get_declarations()`
 * Returns a list to the global scope declarations.
 * - Return: Returns an STL list by reference.
 *
 * #### `SgGlobal::get_declarations() const`
 * Returns a const list to the global scope declarations.
 * - Return: Returns a const STL list by reference.
 */
class SgGlobal;

/** @brief This class represents the concept of a C or C++ goto statement.
 *
 * - See also:
 * Example of using a SgGotoStatement object
 *
 * **Data members**
 *
 * #### `SgGotoStatement::p_label`
 * This pointer points to the SgLabelStatement where control flow will be transfered
 * during execution.
 * - Internal: This is always a valid pointer.
 *
 * **Member functions**
 *
 * #### `SgGotoStatement::SgGotoStatement ( Sg_File_Info* startOfConstruct = NULL )`
 * This is the constructor.
 * This constructor builds the SgGotoStatement base class.
 * - Param `startOfConstruct`: represents the position in the source code
 *
 * #### `SgGotoStatement::~SgGotoStatement()`
 * This is the destructor.
 * There are a lot of things to delete, but nothing is deleted in this destructor.
 *
 * #### `SgGotoStatement::isSgGotoStatement (SgNode *s)`
 * Cast function (from derived class to SgGotoStatement pointer).
 * This functions returns a SgGotoStatement pointer for any input of a
 * pointer to an object derived from a SgGotoStatement.
 * - Return: Returns valid pointer to SgGotoStatement if input is derived from a SgLocatedNode.
 *
 * #### `SgGotoStatement::isSgGotoStatement (const SgNode *s)`
 * Cast function (from derived class to SgGotoStatement pointer).
 * This functions returns a SgGotoStatement pointer for any input of a
 * pointer to an object derived from a SgGotoStatement.
 * - Return: Returns valid pointer to SgGotoStatement if input is derived from a SgLocatedNode.
 *
 * #### `SgGotoStatement::get_label() const`
 * Returns pointer to SgLabelStatement where control flow will be transfered during execution.
 * - Return: Returns pointer to SgLabelStatement.
 *
 * #### `SgGotoStatement::set_label(SgLabelStatement* label)`
 * Access function for p_label.
 * - Return: Returns void.
 */
class SgGotoStatement;

/** @brief This class represents the concept of an "if" construct.
 *
 * - Internal:
 * - Todo: The unparse function "unparseIfStmt" associated with this IR node
 * is implemented using a loop.  I think this should be changed to be more
 * conventional and structural (consistant with the design of the rest of
 * the unparsing).
 *
 * **Data members**
 *
 * #### `SgIfStmt::p_conditional`
 * This pointer a SgStatement.
 *
 * #### `SgIfStmt::p_true_body`
 * This pointer a SgBasicBlock, and holds the statements in the "true" body of if statement.
 *
 * #### `SgIfStmt::p_false_body`
 * This pointer a SgBasicBlock, and holds the statements in the "false" body of if statement.
 *
 * **Member functions**
 *
 * #### `SgIfStmt::SgIfStmt ( Sg_File_Info* startOfConstruct = NULL )`
 * This is the constructor.
 * This constructor builds the SgIfStmt base class.
 * - Param `startOfConstruct`: represents the position in the source code
 *
 * #### `SgIfStmt::~SgIfStmt()`
 * This is the destructor.
 * There are a lot of things to delete, but nothing is deleted in this destructor.
 *
 * #### `SgIfStmt::isSgIfStmt (SgNode *s)`
 * Cast function (from derived class to SgIfStmt pointer).
 * This functions returns a SgIfStmt pointer for any input of a
 * pointer to an object derived from a SgIfStmt.
 * - Return: Returns valid pointer to SgIfStmt if input is derived from a SgLocatedNode.
 *
 * #### `SgIfStmt::isSgIfStmt (const SgNode *s)`
 * Cast function (from derived class to SgIfStmt pointer).
 * This functions returns a SgIfStmt pointer for any input of a
 * pointer to an object derived from a SgIfStmt.
 * - Return: Returns valid pointer to SgIfStmt if input is derived from a SgLocatedNode.
 *
 * #### `SgIfStmt::copy(const SgCopyHelp & help)`
 * Makes a copy (deap of shallow depending on SgCopyHelp).
 * - Return: Returns pointer to copy of SgIfStmt.
 *
 * #### `SgIfStmt::get_conditional() const`
 * Access function for p_conditional.
 * - Return: Returns a pointer to a SgStatement.
 *
 * #### `SgIfStmt::set_conditional(SgStatement* conditional)`
 * Access function for p_conditional.
 * - Param `conditional`: SgStatement pointer
 * - Return: Returns void.
 *
 * #### `SgIfStmt::get_true_body() const`
 * Access function for p_true_body.
 * - Return: Returns a pointer to a SgBasicBlock.
 *
 * #### `SgIfStmt::set_true_body(SgBasicBlock* true_body)`
 * Access function for p_true_body.
 * - Param `true_body`: SgBasicBlock pointer
 * - Return: Returns void.
 *
 * #### `SgIfStmt::get_false_body() const`
 * Access function for p_false_body.
 * - Return: Returns a pointer to a SgBasicBlock.
 *
 * #### `SgIfStmt::set_false_body(SgBasicBlock* false_body)`
 * Access function for p_false_body.
 * - Param `false_body`: SgBasicBlock pointer
 * - Return: Returns void.
 *
 * #### `SgIfStmt::set_else_numeric_label(SgLabelRefExp* label)`
 * Access function for p_else_numeric_label (Fortran only).
 * - Param `lable`: used for end of if statement (Fortran only).
 * - Return: Returns void.
 *
 * #### `SgIfStmt::get_else_numeric_label()`
 * Access function for p_else_numeric_label (Fortran only).
 * - Param `lable`: used for end of if statement (Fortran only).
 * - Return: Returns SgLabelRefExp pointer.
 *
 * #### `SgIfStmt::set_end_numeric_label(SgLabelRefExp* label)`
 * Access function for p_end_numeric_label (Fortran only).
 * - Param `lable`: used for end of if statement (Fortran only).
 * - Return: Returns void.
 *
 * #### `SgIfStmt::get_end_numeric_label()`
 * Access function for p_end_numeric_label (Fortran only).
 * - Param `lable`: used for end of if statement (Fortran only).
 * - Return: Returns SgLabelRefExp pointer.
 *
 * #### `SgIfStmt::set_string_label(std::string label)`
 * Access function for p_string_label (Fortran only; string required to addess generality of Fortran numeric and named labels).
 * - Param `label`: used for Fortran only.
 * - Return: Returns void.
 *
 * #### `SgIfStmt::get_string_label()`
 * Access function for p_string_label (Fortran only; string required to addess generality of Fortran numeric and named labels).
 * - Return: Returns std::string.
 *
 * #### `SgIfStmt::get_has_end_statement()`
 * Fortran specific function to indicate if the Fortran "if" statement has an "end" construct (C/C++ useage always returns false).
 * - Return: Returns bool.
 *
 * #### `SgIfStmt::set_has_end_statement(bool value)`
 * Fortran specific function to indicate if the Fortran "if" statement has an "end" construct (C/C++ useage always returns false).
 * - Param `value`: to use in setting associated (Fortran specific) data member.
 * - Return: Returns void.
 *
 * #### `SgIfStmt::get_use_then_keyword()`
 * Fortran specific function to indicate if the Fortran "if" statement uses the "then" construct (C/C++ useage always returns false).
 * - Return: Returns bool.
 *
 * #### `SgIfStmt::set_use_then_keyword(bool value)`
 * Fortran specific function to indicate if the Fortran "if" statement uses the "then" construct (C/C++ useage always returns false).
 * - Param `value`: to use in setting associated (Fortran specific) data member.
 * - Return: Returns void.
 *
 * #### `SgIfStmt::get_is_else_if_statement()`
 * Fortran and Ada specific function to indicate "else if" form of "if" construct (C/C++ useage always returns false).
 * - Return: Returns bool.
 *
 * #### `SgIfStmt::set_is_else_if_statement(bool value)`
 * Fortran and Ada specific function to indicate "else if" form of "if" construct (C/C++ useage always returns false).
 * - Param `value`: to use in setting associated (Fortran specific) data member.
 * - Return: Returns void.
 */
class SgIfStmt;

/** @brief This class represents the notion of a declared variable.
 *
 * Each variable in the program has a SgInitializedName object which
 * represents its definition. A SgVariableDeclaration for example might
 * contain several SgInitializedName objects, while the
 * SgInitializedName contains the declaration for *one*
 * variable. Each variable use (VarRefExp for example) must have a link
 * to the SgInitializedName object where that specific variable was
 * defined.
 * **What** **really** **happens**
 * Currently, each SgVariableDeclaration contains **only** **one** SgInitializedName
 * In order to have a valid SgInitializedName object, this information must be provided :
 * - the variable name which represents the variable that is being declared in this class.
 * - the variable type
 * - the declaration object which contains the SgInitializedName ( this might be a SgVariableDeclaration, SgFunctionParameterList, SgClassDeclaration,etc)
 * - See also:
 * Example of creating an SgInitializedName object
 * Example of using an SgInitializedName object
 *
 * **Data members**
 *
 * #### `SgInitializedName::p_fileInfo`
 * This pointer is always valid and stores the source position of the start a name.
 * This is an Sg_File_Info object which represents the source position of the starting of
 * the name represented by the SgInitializedName object (variable name, function name, etc.).
 *
 * #### `SgInitializedName::p_name`
 * The variable that is declared in this declaration
 * This is a SgName object which represents the variable that is being declared in this SgInitializedName object.
 * For example, if there is a "int x" declaration in the code, "x" is the variable that will be stored in SgInitializedName::p_name as a SgName object.
 *
 * #### `SgInitializedName::p_typeptr`
 * Pointer to a type object that has been associated with SgInitializedName::p_name
 * This is a pointer that points to an SgType object that represents
 * the type the variable SgInitializedName::p_name declared in this SgInitializedName
 * class (in this declaration). For example, if the variable is of type
 * "int" (as in "int x"), then a SgTypeInt object has to be allocated , and a pointer
 * to this SgTypeInt object has to be stored in SgInitializedName::p_typeptr to represent
 * the type of SgInitializedName::p_name.
 *
 * #### `SgInitializedName::p_initptr`
 * Pointer to an initializer for the variable.
 * In the case that the declaration contains an initializer for the variable, for example "int x=5" or "int x=y" (in these cases the intializers for the declared variable are "5" and "y"), that initializer has to be stored in SgInitializedName::p_initptr as a pointer that points to the SgInitializer object that the SgInitializedName::p_name variable is initialized with.
 *
 * #### `SgInitializedName::p_prev_decl_item`
 * Pointer to the initial uses of this variable previous to its redeclaration declaration.
 * **Dan's** **intuitive** **explanation**
 * This pointer references any initialized name previously built to define a SgVarRefExp,
 * which requires a pointer to a variable declaration (but points to a SgInitializedName
 * since a SgVariableDeclaration could stand for many variables (not just one, e.g.
 * "int x,y,z;"), since uniqueness is required.  Although non-intuative, C++ code defined
 * within a class definition can
 * reference variables before they are defined (e.g. "class X { int foo() { return x; } int x; };"),
 * see test2005_67.C (non-static data member) and test2005_68.C (static data member).
 * It is also used by a SgInitializedName in a static declaration outside the class to
 * refer to the preliminary declaration inside the class.
 * (e.g. "class X { static int a;}; int X::a = 0; };")
 *
 * #### `SgInitializedName::p_is_initializer`
 * flag to determine whether the declaration has an initializer.
 * If the declaration has an initializer, for example "int x=5", then
 * this flag is set to true. The flag is set to false otherwise.
 *
 * #### `SgInitializedName::p_declptr`
 * Pointer to the declaration object where this SgInitializedName object belongs to.
 * **Alin's** **intuitive** **explanation**
 * Each SgInitializedName object contains the declaration of *one*
 * variable. In the case where the code has more declarations in a
 * single statement, for example "int x=5,y=3;", that declaration is
 * composed of two SgInitializedName objects, one for each variable
 * declared in that statment. So, this SgInitializedName::p_declptr points to the
 * statement (actually the declaration - SgDeclaration) that contains it.
 * **What** **really** **happens**
 * For the first SgInitializedName, this points to the the
 * SgDeclarationStatement that contains this SgInitializedName. For the
 * second one, it is set to an unknown SgDeclarationStatement that does
 * not show up in the pdf or the dot files (not traversed).
 * **Dan's** **explaination**
 * This points to the SgVariableDefinition and is the same as get_definition().
 *
 * #### `SgInitializedName::p_itemptr`
 * Pointer to the next SgInitializedName in the declaration.
 * **Alin's** **intuitive** **explanation**
 * This is a pointer to the next SgInitializedName in a declaration
 * statement. For example, if there is a declaration statement of this
 * form "int x=5,y=3", and the current SgInitializedName represents
 * variable "x", the next SgInitializedName in the current declaration
 * is for the variable "y".
 * **What** **really** **happens**
 * This is the pointer that points to a nested SgInitializedName object
 * that has the same variable name, variable type, but it has a
 * different declaration statement and a different SgStorageModifier
 * (this one makes sens). For the nested SgInitalizedName::p_itemptr
 * object, this data member is set to 0 and the SgInitializedName::p_prev_itemptr data
 * member is set to the parent SgInitializedName.
 *
 * #### `SgInitializedName::p_prev_itemptr`
 * Pointer to the previous SgInitializedName in the declaration.
 * **Alin's** **intuitive** **explanation**
 * This is a pointer to the previous SgInitializedName in a declaration
 * statement. For example, if there is a declaration statement of this
 * form "int x=5,y=3", and the current SgInitializedName represents
 * variable "y", the next SgInitializedName in the current declaration
 * is for the variable "x".
 * **What** **really** **happens**
 * For the first level SgInitializedName, this data member is set to 0. For the nested one, this is set to the parent SgInitializedName.
 *
 * #### `SgInitializedName::p_storageModifier`
 * This is the storage modifier (static, auto, register, mutable, asm, etc.).
 * This is the storage modifier (static, auto, register, mutable, asm, etc.), see complete
 * list in source code for more details.  This is an implementation of the modifier system
 * as outlined in appendix A of Bjarne's book.
 * - Internal: Note that isStatic() in the SgInitializedName is always false, is is set in the
 * SgStorageModifier stored in the SgVariableDeclaration (where it is filed of the SgDeclarationModifier).
 *
 * #### `SgInitializedName::p_scope`
 * This pointer is always valid and stores the current scope of the variable.
 * This is the current scope of the variable (required because variables can be defined
 * separately from their declaration).  See test2004_133.C (approx).
 *
 * #### `SgInitializedName::p_preinitialization`
 * This data member stores an enum value.
 * This value is set based on an enum value to indicate the type of use of the initialized
 * name (valid values are: virtual base class, non-virtual base, data member).
 * - Internal: I think that the preinitalization information might be redundant with the
 * SgStorageModifier information.
 *
 * #### `SgInitializedName::p_register_name_code`
 * Code (following GNU standard) for register name.
 * This value is set based on an enum value of GNU standard codes mappings to register names.
 * - Internal: This is a very architecture dependent aspect of the Sage III IR.  We only
 * currently represent code for the Intel X86 processor.
 *
 * #### `SgInitializedName::p_register_name_name`
 * String representing the register name, used when associated GNU code can't be translated.
 * This is a string value representing the name specified (untranslated to the GNU standard
 * register codes). The string is used when the name specified by theused was
 * untranslatable to a more compact GNU code.  This forces the IR to hold an rarely
 * used string object in a frequently used IR node, but I don't think we have any
 * simple way around this detail since we have to support the more general use of
 * the asm options in C and C++.
 * - Internal: This is less architecture dependent than the GNU standard register codes, but
 * takes more storage (though it is not used often).
 *
 * **Member functions**
 *
 * #### `SgInitializedName::get_storageModifier()`
 * returns a reference to the storage modifier
 *
 * #### `SgInitializedName::post_construction_initialization ()`
 * Allocates a new storage modifier and sets the storage modifier to default values
 *
 * #### `SgInitializedName::SgInitializedName (const SgInitializedName &ptr)`
 * This is the copy constructor
 *
 * #### `SgInitializedName::SgInitializedName ( Sg_File_Info* fileInfo, const SgName& name, SgType *typeptr, SgInitializer* iptr,`
 * SgDeclarationStatement *declptr, SgScopeStatement* scope, SgInitializedName *prev_itemptr );
 * This is the constructor
 * - Param `fileInfo`: pointer to source position (also used to mark compiler generated or transformed code)
 * - Param `name`: the variable name
 * - Param `typeptr`: a pointer to the variable's type object
 * - Param `iptr`: pointer to the initializer of the variable ( if any)
 * - Param `declptr`: pointer to the declaration statement to which this SgInitializedName belongs to
 * - Param `scope`: pointer to SgScopeStatement to explicitly represent the scope of the variable (required for ROSE, but mostly useful in C++)
 * - Param `itemptr`: the next SgInitializedName object in the parent declaration statement
 * - Param `prev_itemptr`: the previous SgInitializedName object in the parent declaration statement
 * - See also:
 * Example:create an SgInitializedName object
 *
 * #### `SgInitializedName::SgInitializedName(const SgName &name, SgType *typeptr, SgInitializer *iptr=0, SgDeclarationStatement *declptr=0, SgInitializedName *itemptr=0, SgInitializedName *prev_itemptr=0)`
 * This is the constructor
 * - Deprecated: This is an older constructor which does not include the Sg_File_Info or the
 * SgScopeStatement in its parameter list.  These must be set explicitly using the data
 * member's access functions when using this constructor.
 * - Param `name`: the variable name
 * - Param `typeptr`: a pointer to the variable's type object
 * - Param `iptr`: pointer to the initializer of the variable ( if any)
 * - Param `declptr`: pointer to the declaration statement to which this SgInitializedName belongs to
 * - Param `itemptr`: the next SgInitializedName object in the parent declaration statement
 * - Param `prev_itemptr`: the previous SgInitializedName object in the parent declaration statement
 * - See also:
 * Example:create an SgInitializedName object
 *
 * #### `SgNode * SgInitializedName::copy (const SgCopyHelp &help) const`
 * It clones the current SgInitializedName object recursively or not, depending on the argument
 * - Param `help`: - If this argument is of type SgTreeCopy, then the
 * SgInitializedName is cloned recursively. If it's of type SgShallowCopy
 * only the first level of SgInitializedName is copied, everything else
 * pointing to the the original SgInitializedName object's data members.
 * - Return: a pointer to the new clone.
 *
 * #### `SgInitializedName & SgInitializedName::operator= (const SgInitializedName &ptr)`
 * assignment operator. It copies **everything** (including pointers) from the rhs object to the lhs object
 * It copies all the data members of the rhs SgInitializedName object
 * to the lhs (this) object. The copying is done by value, so all the
 * data member pointers are copied by **value.** After the assignment
 * operator is executed, both operands **share** the same data
 * members. Actually there is a note in the source code (Cxx_Grammar.h)
 * that says that this needs to be executed recursively ( allocating
 * new data members for the newly assigned object).
 *
 * #### `bool SgInitializedName::operator== (const SgInitializedName &) const`
 * Equal operator : it checks if all the data members are the same or point to the same objects
 * **What** **really** **happens**
 * It returns false all the time.
 *
 * #### `SgInitializedName::p_name`
 * variable x
 * detailed variable x
 *
 * #### `bool SgInitializedName::get_declaration() const`
 * Equal operator : it checks if all the data members are the same or point to the same objects
 * - Return: Pointer to SgDeclarationStatement
 *
 * #### `SgInitializedName::get_symbol_from_symbol_table() const`
 * **FOR** **INTERNAL** **USE** Get the associated symbol from the symbol table in the stored scope.
 * Users should use the SgInitializedName::search_for_symbol_from_symbol_table() instead.
 *
 * #### `SgInitializedName::search_for_symbol_from_symbol_table() const`
 * User interface for retrieving the associated symbol. It searches through the possible chain of prev_decl_item.
 */
class SgInitializedName;

/** @brief This class represents the notion of an initializer for a variable declaration or
 *
 * expression in a function call argument list.
 * There are no direct uses of this IR node anywhere.  All initializers
 * are derived from this IR node to build derived classes such as SgConstructorInitializer,
 * SgAssignInitializer, and SgAggregateInitializer.
 * - Internal: This is a base class for all initializers.
 * - See also:
 * Example of using a SgInitializer object
 *
 * **Data members**
 *
 * #### `SgInitializer::p_is_explicit_cast`
 * This boolean variable marks the initializer ans part of an explicit or implicit
 * cast.  It is used for all of the different types of initalizers.
 * - Internal: This may now be duplicate information with the compiler generated flag in Sg_File_Info.
 *
 * **Member functions**
 *
 * #### `SgInitializer::SgInitializer ( Sg_File_Info* startOfConstruct = NULL )`
 * This is the constructor.
 * This constructor builds the SgInitializer base class.
 * - Param `startOfConstruct`: represents the position in the source code
 * - See also:
 * Example:create an SgInitializer object
 *
 * #### `SgInitializer::~SgInitializer()`
 * This is the destructor.
 * Only the Sg_File_Info object can be deleted in this object.
 *
 * #### `SgInitializer::isSgInitializer (SgNode *s)`
 * Cast function (from derived class to SgInitializer pointer).
 * This functions returns a SgInitializer pointer for any input of a
 * pointer to an object derived from a SgInitializer.
 * - Return: Returns valid pointer to SgInitializer if input is derived from a SgInitializer.
 *
 * #### `SgInitializer::isSgInitializer (const SgNode *s)`
 * Cast function (from derived class to SgInitializer pointer).
 * This functions returns a SgInitializer pointer for any input of a
 * pointer to an object derived from a SgInitializer.
 * - Return: Returns valid pointer to SgInitializer if input is derived from a SgInitializer.
 *
 * #### `SgInitializer::get_is_explicit_cast (void) const`
 * returns bool value if front-end considers this cast as explicit.
 * - Return: Returns bool value.
 *
 * #### `SgInitializer::set_is_explicit_cast (bool explicit_cast)`
 * This function allows the p_is_explicit_cast flag to be set (used internally).
 * - Param `explicit_cast`: - sets value of internal p_is_explicit_cast flag (true if explicit).
 * - Return: Returns void.
 */
class SgInitializer;

/** @brief This class represents the physical disequality (often called pointer disequality) operator
 *
 * for languages that also define a content disequality operator.
 * This node is intended for use
 * with Python, where it represents the "is not" token. It should not be confused with the
 * SgNotEqualOp operator, which represents the "!=" token in Python and is used for content disquality.
 *
 * **Data members**
 *
 * #### `SgNode::p_parent`
 * This is the pointer to the parent IR node in the AST.
 * This is the pointer to the parent IR node.  It is a valid pointer
 * on all nodes that are traversed (SgExpressions, SgStatements, SgInitializedName, etc.)
 * However it is not set on SgTypes and SgSymbols,both of which are shared internally.
 * This pointer is mostly set in post processing of the Sage III AST, until
 * this point it is not reliable.
 *
 * **Member functions**
 *
 * #### `SgNode::copy(const SgCopyHelp & help ) const`
 * This function clones the current IR node object recursively or not, depending on the argument
 * This function performs a copy based on the specification of the input parameter.
 * The input parameter is used to determin which data members are copied by reference
 * and which are copied by value.
 * - Param `help`: - If this argument is of type SgTreeCopy, then the
 * IR node is cloned recursively. If its of type SgShallowCopy
 * only the first level of the IR node is copied, everything else is
 * left pointing to the the original IR node's object's data members.
 * - Return: a pointer to the new clone.
 * - Internal: It appears the the copy functions don't set the parents of anything that they do
 * a deep copy of! This can cause AST tests to fail. In particular some functions that
 * require the parent pointers to be valid will return NULL pointers (e.g. SgInitializedName::get_declaration()).
 * It might be that we should allow this to be done as part of the
 * SgCopyHelp::clone function or perhaps another member function of SgCopyHelp would be
 * useful for this support.  It is not serious if the AST post processing is done since
 * that will set any NULL pointers that are found within its traversal.
 * - Exception: none No exceptions are thrown by this function.
 *
 * #### `SgNode::SgNode()`
 * This is the constructor
 * This constructor builds a SgNode, always used as a base class.
 * All Sage III IR nodes are derived from this class.
 * - See also:
 * Example:create an SgNode object
 *
 * #### `SgNode::~SgNode()`
 * This is the destructor.
 * There is nothing to delete in this object.
 *
 * #### `SgNode::sage_class_name() const`
 * generates string representing the class name: (e.g. for SgNode returns "SgNode").
 * This function is useful for debugging and error reporting.  It
 * returns the name of the IR node.
 * - Deprecated: Use class_name() which returns a C++ string object.
 * - Return: a char* pointer to a C style string.
 *
 * #### `string SgNode::class_name() const`
 * generates string representing the class name: (e.g. for SgNode returns "SgNode").
 * This function is useful for debugging and error reporting.  It
 * returns the name of the IR node.
 * - Return: a C++ string object.
 *
 * #### `SgNode::variant() const`
 * Older version function returns enum value "NODE"
 * This function is an older version of the variant function.
 * This function is useful for debugging and error reporting.  It
 * returns the name of the IR node.
 * - Return: an int value.
 * - Deprecated: Use Tvariant() instead.  Older enum values have inconsistant
 * names and are being removed.
 *
 * #### `SgNode::getVariant() const`
 * Older version function returns enum value "NODE"
 * This function is an older version of the variant function.
 * This function is useful for debugging and error reporting.  It
 * returns the name of the IR node.
 * - Return: an int value.
 * - Deprecated: Use Tvariant() instead.  Older enum values have inconsistant
 * names and are being removed.
 *
 * #### `SgNode::variantT() const`
 * returns enum value "V_SgNode"
 * This function is useful for debugging and error reporting.  It
 * returns the name of the IR node.
 * - Return: an enum value (type: VariantT).
 * - Deprecated: Use Tvariant() instead.  Older enum values have inconsistant
 * names and are being removed.
 *
 * #### `SgNode::get_isModified() const`
 * **FOR** **INTERNAL** **USE** All nodes in the AST contain a isModified flag.
 * This flag can be set but this is typically an internal function used to track the updates to AST.
 * - Return: Returns bool; true if IR node has been modified.
 *
 * #### `SgNode::set_isModified(bool isModified)`
 * Acess function for isModified flag.
 * This flag records if the current IR node has been modified. It is set to false after
 * and ROSE front-end processing.
 *
 * #### `SgNode::get_isVisited() const`
 * DOCS IN SgNode.docs: Access function for p_isVisited flag used previously by the AST traversals.
 * - Deprecated: This function is not used and will be removed.
 * - Return: Returns bool; true if previously visited within current AST traversal.
 *
 * #### `SgNode::set_isVisited(bool isVisited)`
 * Access function for p_isVisited flag used previously by the AST traversals.
 * - Deprecated: This function is not used and will be removed.
 *
 * #### `SgNode::isSgNode (SgNode *s)`
 * Cast function (from derived class to SgNode pointer).
 * This functions returns a SgNode pointer for any input of a
 * pointer to an object derived from a SgNode.
 * - Return: Returns valid pointer to SgNode if input is derived from a SgNode.
 *
 * #### `SgNode::isSgNode (const SgNode *s)`
 * Cast function (from derived class to SgNode pointer).
 * This functions returns a SgNode pointer for any input of a
 * pointer to an object derived from a SgNode.
 * - Return: Returns valid pointer to SgNode if input is derived from a SgNode.
 *
 * #### `SgNode::set_parent (SgNode *parent)`
 * Sets parent node for any IR node.
 * The parent node in the AST can be set but this is typically
 * an internal function used to build the AST.
 * - Param `parent`: - Pointer to parent node to store within the current IR node.
 * - Return: returns void.
 *
 * #### `SgNode::get_parent() const`
 * Access function for parent node in AST.
 * The parent node in the AST can be accessed, in general only the project node, symbols and
 * types should be NULL.  Since parent nodes are set within post processing
 * (and using the ROSE AST traveral mechanism) the parents are guarenteed to
 * match the traversal, and no other concept of what could be a parent within
 * the AST (e.g a parent concept based on scope).  Because the traversal is based
 * on the source code layout, what is a parent is similarly based on the source
 * code layout and not any concept of scope.  Note that the scope of relavant
 * IR nodes is stored explicitly in the AST, precisely because it is not always
 * related to the layout of the source code (and thus not related to the concept
 * of parent in the AST).
 * - Return: Returns pointer to SgNode
 *
 * #### `SgNode::unparseToString( SgUnparse_Info* info )`
 * This function unparses the AST node (source code only, excluding comments and white space)
 * - Param `info`: is optional (used only to specify code generation options).
 * This function is useful for converting AST nodes to strings as part of general
 * debugging or the construction of other strings for use as input to the AST rewrite
 * mechansims which accepts source code strings.  See tutorial for examples of this.
 * This function uses the SgUnparse_Info as an inherited attribute internally
 * and using this attribute it will correctly handled many subtle details that
 * will be ignored if the attribute is not provided.  For example, the SgUnparse_Info
 * can record if the statement is in a conditional and if so then the trailing ";"
 * will be omitted in the generated code. See the SgUnparse_Info documentation for
 * the numerous other internal settings that can effect the generated code.  Because
 * of these details, the unparseToString() function can not always be used to generate
 * compiliable code.
 * - Return: Returns std::string
 *
 * #### `SgNode::unparseToCompleteString()`
 * This function unparses the AST node (including comments and white space)
 * This function is a complement to the unparseToString() function and includes
 * any associated comments and preprocessor control directives.  Because C preprocessor
 * control directive can be included string generated using this function may or may
 * not be appropriate for use as input to the AST rewrite mechanism.
 * - Todo: This function needs a better name since it is unclear what the "complete" string is.
 * - Return: Returns std::string
 *
 * #### `SgNode::get_traversalSuccessorContainer()`
 * **FOR** **INTERNAL** **USE** within ROSE traverals mechanism only.
 * This function builds and returns a copy of ordered container
 * holding pointers to children of this node in a traversal. It is
 * associated with the definition of a tree that is travered by the
 * AST traversal mechanism; a tree that is embeded in the AST (which
 * is a more general graph).  This function is used within the implementation
 * of the AST traversal and has a semantics may change in subtle ways
 * that makes it difficult to use in user code.  It can return unexpected
 * data members and thus the order and the number of elements is unpredicable
 * and subject to change.
 * - Warning: This function can return unexpected data members and thus the
 * order and the number of elements is unpredicable and subject
 * to change.
 * - Return: Returns ordered STL Container of pointers to children nodes in AST.
 *
 * #### `SgNode::get_traversalSuccessorNamesContainer()`
 * **FOR** **INTERNAL** **USE** within ROSE traverals mechanism only.
 * This function builds and returns a copy of ordered container
 * holding strings used to name data members that are traversed in the IR
 * node. It is associated with the definition of a tree that is travered by the
 * AST traversal mechanism; a tree that is embeded in the AST (which
 * is a more general graph).  This function is used within the implementation
 * of the AST traversal and has a semantics may change in subtle ways
 * that makes it difficult to use in user code.  It can return unexpected
 * data members and thus the order and the number of elements is unpredicable
 * and subject to change.
 * - Warning: This function can return unexpected data members and thus the
 * order and the number of elements is unpredicable and subject
 * to change.
 * Each string is a name of a member variable holding a pointer to a
 * child in the AST. The names are the same as used in the generated enums for
 * accessing attributes in a traversal. The order is the same in which they are
 * traversed and the same in which the access enums are defined. Therefore this
 * method can be used to get the corresponding name (string) of an access enum
 * which allows to produce more meaningful messages for attribute computations.
 * - Return: Returns ordered STL container of names (strings) of access names to children nodes in AST.
 *
 * #### `SgNode::roseRTI ()`
 * **FOR** **INTERNAL** **USE** Access to Runtime Type Information (RTI) for this IR nodes.
 * This function provides runtime type information for accessing the
 * structure of the current node.  It is useful for generating code which
 * would dump out or rebuild IR nodes.
 * - Return: Returns a RTIReturnType object (runtime type information).
 *
 * **Additional notes**
 */
class SgIsNotOp;

/** @brief This class represents the physical equality (often called pointer equality) operator
 *
 * for languages that also define a content equality operator.
 * This node is intended for use
 * with Python, where it represents the "is" token. It should not be confused with the
 * SgEqualityOp operator, which represents the "==" token in Python and is used for content equality.
 *
 * **Data members**
 *
 * #### `SgNode::p_parent`
 * This is the pointer to the parent IR node in the AST.
 * This is the pointer to the parent IR node.  It is a valid pointer
 * on all nodes that are traversed (SgExpressions, SgStatements, SgInitializedName, etc.)
 * However it is not set on SgTypes and SgSymbols,both of which are shared internally.
 * This pointer is mostly set in post processing of the Sage III AST, until
 * this point it is not reliable.
 *
 * **Member functions**
 *
 * #### `SgNode::copy(const SgCopyHelp & help ) const`
 * This function clones the current IR node object recursively or not, depending on the argument
 * This function performs a copy based on the specification of the input parameter.
 * The input parameter is used to determin which data members are copied by reference
 * and which are copied by value.
 * - Param `help`: - If this argument is of type SgTreeCopy, then the
 * IR node is cloned recursively. If its of type SgShallowCopy
 * only the first level of the IR node is copied, everything else is
 * left pointing to the the original IR node's object's data members.
 * - Return: a pointer to the new clone.
 * - Internal: It appears the the copy functions don't set the parents of anything that they do
 * a deep copy of! This can cause AST tests to fail. In particular some functions that
 * require the parent pointers to be valid will return NULL pointers (e.g. SgInitializedName::get_declaration()).
 * It might be that we should allow this to be done as part of the
 * SgCopyHelp::clone function or perhaps another member function of SgCopyHelp would be
 * useful for this support.  It is not serious if the AST post processing is done since
 * that will set any NULL pointers that are found within its traversal.
 * - Exception: none No exceptions are thrown by this function.
 *
 * #### `SgNode::SgNode()`
 * This is the constructor
 * This constructor builds a SgNode, always used as a base class.
 * All Sage III IR nodes are derived from this class.
 * - See also:
 * Example:create an SgNode object
 *
 * #### `SgNode::~SgNode()`
 * This is the destructor.
 * There is nothing to delete in this object.
 *
 * #### `SgNode::sage_class_name() const`
 * generates string representing the class name: (e.g. for SgNode returns "SgNode").
 * This function is useful for debugging and error reporting.  It
 * returns the name of the IR node.
 * - Deprecated: Use class_name() which returns a C++ string object.
 * - Return: a char* pointer to a C style string.
 *
 * #### `string SgNode::class_name() const`
 * generates string representing the class name: (e.g. for SgNode returns "SgNode").
 * This function is useful for debugging and error reporting.  It
 * returns the name of the IR node.
 * - Return: a C++ string object.
 *
 * #### `SgNode::variant() const`
 * Older version function returns enum value "NODE"
 * This function is an older version of the variant function.
 * This function is useful for debugging and error reporting.  It
 * returns the name of the IR node.
 * - Return: an int value.
 * - Deprecated: Use Tvariant() instead.  Older enum values have inconsistant
 * names and are being removed.
 *
 * #### `SgNode::getVariant() const`
 * Older version function returns enum value "NODE"
 * This function is an older version of the variant function.
 * This function is useful for debugging and error reporting.  It
 * returns the name of the IR node.
 * - Return: an int value.
 * - Deprecated: Use Tvariant() instead.  Older enum values have inconsistant
 * names and are being removed.
 *
 * #### `SgNode::variantT() const`
 * returns enum value "V_SgNode"
 * This function is useful for debugging and error reporting.  It
 * returns the name of the IR node.
 * - Return: an enum value (type: VariantT).
 * - Deprecated: Use Tvariant() instead.  Older enum values have inconsistant
 * names and are being removed.
 *
 * #### `SgNode::get_isModified() const`
 * **FOR** **INTERNAL** **USE** All nodes in the AST contain a isModified flag.
 * This flag can be set but this is typically an internal function used to track the updates to AST.
 * - Return: Returns bool; true if IR node has been modified.
 *
 * #### `SgNode::set_isModified(bool isModified)`
 * Acess function for isModified flag.
 * This flag records if the current IR node has been modified. It is set to false after
 * and ROSE front-end processing.
 *
 * #### `SgNode::get_isVisited() const`
 * DOCS IN SgNode.docs: Access function for p_isVisited flag used previously by the AST traversals.
 * - Deprecated: This function is not used and will be removed.
 * - Return: Returns bool; true if previously visited within current AST traversal.
 *
 * #### `SgNode::set_isVisited(bool isVisited)`
 * Access function for p_isVisited flag used previously by the AST traversals.
 * - Deprecated: This function is not used and will be removed.
 *
 * #### `SgNode::isSgNode (SgNode *s)`
 * Cast function (from derived class to SgNode pointer).
 * This functions returns a SgNode pointer for any input of a
 * pointer to an object derived from a SgNode.
 * - Return: Returns valid pointer to SgNode if input is derived from a SgNode.
 *
 * #### `SgNode::isSgNode (const SgNode *s)`
 * Cast function (from derived class to SgNode pointer).
 * This functions returns a SgNode pointer for any input of a
 * pointer to an object derived from a SgNode.
 * - Return: Returns valid pointer to SgNode if input is derived from a SgNode.
 *
 * #### `SgNode::set_parent (SgNode *parent)`
 * Sets parent node for any IR node.
 * The parent node in the AST can be set but this is typically
 * an internal function used to build the AST.
 * - Param `parent`: - Pointer to parent node to store within the current IR node.
 * - Return: returns void.
 *
 * #### `SgNode::get_parent() const`
 * Access function for parent node in AST.
 * The parent node in the AST can be accessed, in general only the project node, symbols and
 * types should be NULL.  Since parent nodes are set within post processing
 * (and using the ROSE AST traveral mechanism) the parents are guarenteed to
 * match the traversal, and no other concept of what could be a parent within
 * the AST (e.g a parent concept based on scope).  Because the traversal is based
 * on the source code layout, what is a parent is similarly based on the source
 * code layout and not any concept of scope.  Note that the scope of relavant
 * IR nodes is stored explicitly in the AST, precisely because it is not always
 * related to the layout of the source code (and thus not related to the concept
 * of parent in the AST).
 * - Return: Returns pointer to SgNode
 *
 * #### `SgNode::unparseToString( SgUnparse_Info* info )`
 * This function unparses the AST node (source code only, excluding comments and white space)
 * - Param `info`: is optional (used only to specify code generation options).
 * This function is useful for converting AST nodes to strings as part of general
 * debugging or the construction of other strings for use as input to the AST rewrite
 * mechansims which accepts source code strings.  See tutorial for examples of this.
 * This function uses the SgUnparse_Info as an inherited attribute internally
 * and using this attribute it will correctly handled many subtle details that
 * will be ignored if the attribute is not provided.  For example, the SgUnparse_Info
 * can record if the statement is in a conditional and if so then the trailing ";"
 * will be omitted in the generated code. See the SgUnparse_Info documentation for
 * the numerous other internal settings that can effect the generated code.  Because
 * of these details, the unparseToString() function can not always be used to generate
 * compiliable code.
 * - Return: Returns std::string
 *
 * #### `SgNode::unparseToCompleteString()`
 * This function unparses the AST node (including comments and white space)
 * This function is a complement to the unparseToString() function and includes
 * any associated comments and preprocessor control directives.  Because C preprocessor
 * control directive can be included string generated using this function may or may
 * not be appropriate for use as input to the AST rewrite mechanism.
 * - Todo: This function needs a better name since it is unclear what the "complete" string is.
 * - Return: Returns std::string
 *
 * #### `SgNode::get_traversalSuccessorContainer()`
 * **FOR** **INTERNAL** **USE** within ROSE traverals mechanism only.
 * This function builds and returns a copy of ordered container
 * holding pointers to children of this node in a traversal. It is
 * associated with the definition of a tree that is travered by the
 * AST traversal mechanism; a tree that is embeded in the AST (which
 * is a more general graph).  This function is used within the implementation
 * of the AST traversal and has a semantics may change in subtle ways
 * that makes it difficult to use in user code.  It can return unexpected
 * data members and thus the order and the number of elements is unpredicable
 * and subject to change.
 * - Warning: This function can return unexpected data members and thus the
 * order and the number of elements is unpredicable and subject
 * to change.
 * - Return: Returns ordered STL Container of pointers to children nodes in AST.
 *
 * #### `SgNode::get_traversalSuccessorNamesContainer()`
 * **FOR** **INTERNAL** **USE** within ROSE traverals mechanism only.
 * This function builds and returns a copy of ordered container
 * holding strings used to name data members that are traversed in the IR
 * node. It is associated with the definition of a tree that is travered by the
 * AST traversal mechanism; a tree that is embeded in the AST (which
 * is a more general graph).  This function is used within the implementation
 * of the AST traversal and has a semantics may change in subtle ways
 * that makes it difficult to use in user code.  It can return unexpected
 * data members and thus the order and the number of elements is unpredicable
 * and subject to change.
 * - Warning: This function can return unexpected data members and thus the
 * order and the number of elements is unpredicable and subject
 * to change.
 * Each string is a name of a member variable holding a pointer to a
 * child in the AST. The names are the same as used in the generated enums for
 * accessing attributes in a traversal. The order is the same in which they are
 * traversed and the same in which the access enums are defined. Therefore this
 * method can be used to get the corresponding name (string) of an access enum
 * which allows to produce more meaningful messages for attribute computations.
 * - Return: Returns ordered STL container of names (strings) of access names to children nodes in AST.
 *
 * #### `SgNode::roseRTI ()`
 * **FOR** **INTERNAL** **USE** Access to Runtime Type Information (RTI) for this IR nodes.
 * This function provides runtime type information for accessing the
 * structure of the current node.  It is useful for generating code which
 * would dump out or rebuild IR nodes.
 * - Return: Returns a RTIReturnType object (runtime type information).
 *
 * **Additional notes**
 */
class SgIsOp;

/** @brief This class represents the concept of a C or C++ label statement.
 *
 * The label statment is used for labels asociated with goto statements.
 * A label construct in C++ has a statement associated with it,
 * this concept is not represented in ROSE, but is one of the ways
 * that IPR differs from ROSE.  It seems that EDG accepts the representation of
 * a label without a statement as an extension (ref Peter).  An empty
 * statement is an alternative representation:
 * ```text
 * label:;
 * @endicode
 * instead of
 * ```text
 * label:
 * ```
 * To handle the case where a label is used without an associated statement
 * ROSE will unparse: "LABEL:;"
 * to avoid generating cases without an associated statement.  The "empty
 * statement" represented by ";" is ignored and so will not cause an error
 * or change the semantics.
 * - Internal: Since the label statment does not define a scope, it is not clear that
 * we want to attach a sequence of statements to the label which would only be
 * traversed through the label statement.  We could alternatively fixup
 * label statements to internally reference their next statement if it
 * exists, or an empty statement (e.g. ";") if it did not exist.  The problem
 * with a internal reference is that it serves on particular purpose, and represent
 * a muptiple refeence to it's associated statement (which could cause bugs
 * within transformations), so the current design might be best.
 * - Todo: Review if we want to have the label statement reference it's
 * associated statement (it would be a redundent reference, since the next
 * statement is already in the list of statements for the scope).
 * - See also:
 * Example of using a SgLabelStatement object
 *
 * **Data members**
 *
 * #### `SgLabelStatement::p_label`
 * This a SgName object which stores the name of the label.
 *
 * **Member functions**
 *
 * #### `SgLabelStatement::SgLabelStatement ( Sg_File_Info* startOfConstruct = NULL )`
 * This is the constructor.
 * This constructor builds the SgLabelStatement base class.
 * - Param `startOfConstruct`: represents the position in the source code
 *
 * #### `SgLabelStatement::~SgLabelStatement()`
 * This is the destructor.
 * There are a lot of things to delete, but nothing is deleted in this destructor.
 *
 * #### `SgLabelStatement::isSgLabelStatement (SgNode *s)`
 * Cast function (from derived class to SgLabelStatement pointer).
 * This functions returns a SgLabelStatement pointer for any input of a
 * pointer to an object derived from a SgLabelStatement.
 * - Return: Returns valid pointer to SgLabelStatement if input is derived from a SgLocatedNode.
 *
 * #### `SgLabelStatement::isSgLabelStatement (const SgNode *s)`
 * Cast function (from derived class to SgLabelStatement pointer).
 * This functions returns a SgLabelStatement pointer for any input of a
 * pointer to an object derived from a SgLabelStatement.
 * - Return: Returns valid pointer to SgLabelStatement if input is derived from a SgLocatedNode.
 *
 * #### `SgLabelStatement::get_label() const`
 * Returns SgName by value.
 * - Return: Returns SgName.
 *
 * #### `SgLabelStatement::set_label(SgName label)`
 * Access function for p_label.
 * - Return: Returns void.
 */
class SgLabelStatement;

/** @brief This class represents a lambda expression.
 *
 * This class represents a lambda expression in the input language. Currently, this IR node only works with Python input files.
 *
 * **Data members**
 *
 * #### `SgLambdaRefExp::p_functionDeclaration`
 * The implicit function referred to by this lambda.
 *
 * **Member functions**
 *
 * #### `SgLambdaRefExp::SgLambdaRefExp ()`
 * This is the constructor.
 * This constructor builds the SgLambdaRefExp base class.
 *
 * #### `SgLambdaRefExp::isSgLambdaRefExp (SgNode *s)`
 * Cast function (from derived class to SgLambdaRefExp pointer).
 * This functions returns a SgLambdaRefExp pointer for any input of a
 * pointer to an object derived from a SgLambdaRefExp.
 * - Return: Returns valid pointer to SgLambdaRefExp if input is derived from a SgLambdaRefExp.
 *
 * #### `SgLambdaRefExp::isSgLambdaRefExp (const SgNode *s)`
 * Cast function (from derived class to SgLambdaRefExp pointer).
 * This functions returns a SgLambdaRefExp pointer for any input of a
 * pointer to an object derived from a SgLambdaRefExp.
 * - Return: Returns valid pointer to SgLambdaRefExp if input is derived from a SgLambdaRefExp.
 */
class SgLambdaRefExp;

/** @brief This class represents a list display.
 *
 * This class represents the concept of a list object in the input language. Currently, this IR node only works with Python input files. For other languages, see SgExprListExp.
 *
 * **Data members**
 *
 * #### `SgListExp::p_elements`
 * The list of elements contained in this list.
 *
 * **Member functions**
 *
 * #### `SgListExp::SgListExp ()`
 * This is the constructor.
 * This constructor builds the SgListExp base class.
 *
 * #### `SgListExp::isSgListExp (SgNode *s)`
 * Cast function (from derived class to SgListExp pointer).
 * This functions returns a SgListExp pointer for any input of a
 * pointer to an object derived from a SgListExp.
 * - Return: Returns valid pointer to SgListExp if input is derived from a SgListExp.
 *
 * #### `SgListExp::isSgListExp (const SgNode *s)`
 * Cast function (from derived class to SgListExp pointer).
 * This functions returns a SgListExp pointer for any input of a
 * pointer to an object derived from a SgListExp.
 * - Return: Returns valid pointer to SgListExp if input is derived from a SgListExp.
 */
class SgListExp;

/** @brief This class represents the notion of an expression or statement which
 *
 * has a position within the source code.
 * There are no uses of this IR node anywhere.  All expressions and statements
 * are derived from this IR node to build either SgExpression or SgStatement
 * derived classes.
 * - Note: SgLocatedNode objects have a set_startOfConstruct() and a set_endOfConstruct()
 * these must be set explicitly to define a proper AST.  The set_startOfConstruct()
 * will be called if the constructor taking a Sg_File_Info object is called (depreicated)
 * but the set_endOfConstruct() should be called explicitly.
 * - Internal: The AttachedPreprocessingInfoType *p_attachedPreprocessingInfoPtr
 * and the AstAttributeMechanism *p_attribute are implemented as pointers
 * because they would take up 12 bytes each as STL lists and that would be
 * wasteful when we have 20 million IR nodes held in memory.  This is a
 * violation of  general rule in the IR design that we have STL containers as
 * data member instead of pointers to data members (the file I/O handles this
 * as a special case).
 * - Todo: The AstAttributeMechanism type should be handed as other IR nodes with it's own
 * memory pool, except that in all cases where it would be used, it would be a base class
 * to a user-defined derived type and thus would not fix in our memory pool.
 * - Todo: Consider name change of "SgLocatedNode" to "SgSourceNode".
 * - Todo: Consider moving some of the IR nodes currently in SgSupport to this IR node.
 * IR nodes that might be moved would include:
 * -# SgFile
 * -# SgDirectory (questionable)
 * -# SgInitializedName
 * -# SgBaseClass
 * -# SgTemplateArgument
 * -# SgTemplateParameter
 * - Internal: This is a base class for all expressions and statements.
 * - See also:
 * Example of using a SgLocatedNode object
 *
 * **Data members**
 *
 * #### `SgLocatedNode::p_startOfConstruct`
 * This pointer is always valid and stores the source position of the start of the
 * current construct.
 *
 * #### `SgLocatedNode::p_endOfConstruct`
 * This pointer is always valid and stores the source position of the end of the
 * current construct.
 *
 * #### `SgLocatedNode::p_attachedPreprocessingInfoPtr`
 * Holds comments and/or preprocessor directives located before or after the
 * current statement of expression.
 *
 * **Member functions**
 *
 * #### `SgLocatedNode::SgLocatedNode (const SgLocatedNode &X)`
 * Copy constructor (made private to prevent being called by accident).
 *
 * #### `SgLocatedNode::SgLocatedNode ( Sg_File_Info* startOfConstruct = NULL )`
 * This is the constructor.
 * This constructor builds the SgLocatedNode base class.
 * - Param `startOfConstruct`: represents the position in the source code
 * - See also:
 * Example:create an SgLocatedNode object
 *
 * #### `SgLocatedNode::~SgLocatedNode()`
 * This is the destructor.
 * Only the Sg_File_Info object can be deleted in this object.
 *
 * #### `SgLocatedNode::getFileName()`
 * Access function for "get_file_info()->get_filename()".
 * This function is just a simpler access function which retrives the
 * filename from the Sg_File_Info object at the current IR node.  This
 * is only a convience function an as a result not a great idea.
 * - Internal: This function should likely be removed from the SAGEIII interface.
 * - Deprecated: This function should be removed because it is only an interface
 * function of rather minor significance. Alternatively if we want to implement
 * it, we should consider placing it at the SgNode object. If we do preserve the
 * function then we should have it return a string instead of char*.
 *
 * #### `SgLocatedNode::addToAttachedPreprocessingInfo (PreprocessingInfo *prepInfoPtr,`
 * PreprocessingInfo::RelativePositionType locationInList=PreprocessingInfo::after)
 * This function adds comment or CPP directives to the current IR node.
 * - Param `prepInfoPtr`: - This parameter is a pointer to the container for the comment/directive.
 * It is used to specify if the new  new comment/directive is added to the front or back of the current list of comments/directives.
 * It does not change relative position field of the PreprocessingInfo object.
 * - Param `locationInList`: - adds container before or after the current IR node.
 * - Return: Returns void.
 *
 * #### `SgLocatedNode::getAttachedPreprocessingInfo (void)`
 * This function gets the adds comment or CPP directives to the current IR node.
 * - Param `prepInfoPtr`: - This parameter is a pointer to the container for the comment/directive.
 * - Param `locationInList`: - adds container before or after the current IR node.
 * - Return: Returns void.
 *
 * #### `SgLocatedNode::set_startOfConstruct (Sg_File_Info *startOfConstruct)`
 * This function sets the current source location position of the start
 * of the current construct.
 * - Param `startOfConstruct`: - Pointer to Sg_File_Info object containing source location information.
 * - Return: Returns void.
 *
 * #### `SgLocatedNode::set_endOfConstruct (Sg_File_Info *endOfConstruct)`
 * This function sets the current source location position of the end
 * of the current construct.
 * - Param `endOfConstruct`: - Pointer to Sg_File_Info object containing source location information.
 * - Return: Returns void.
 *
 * #### `SgLocatedNode::isSgLocatedNode (SgNode *s)`
 * Cast function (from derived class to SgLocatedNode pointer).
 * This functions returns a SgLocatedNode pointer for any input of a
 * pointer to an object derived from a SgLocatedNode.
 * - Return: Returns valid pointer to SgLocatedNode if input is derived from a SgLocatedNode.
 *
 * #### `SgLocatedNode::isSgLocatedNode (const SgNode *s)`
 * Cast function (from derived class to SgLocatedNode pointer).
 * This functions returns a SgLocatedNode pointer for any input of a
 * pointer to an object derived from a SgLocatedNode.
 * - Return: Returns valid pointer to SgLocatedNode if input is derived from a SgLocatedNode.
 */
class SgLocatedNode;

/** @brief This class represents the notion of an value (expression value).
 *
 * - Internal: This class will hold a string value so that the exact text for constants can be
 * help in the AST.
 * - Todo: Add string to this class so that the exact value can be held in the AST.
 * - See also:
 * Example of using a SgLongDoubleVal object
 *
 * **Data members**
 *
 * #### `SgLongDoubleVal::p_value`
 * This value holds the double represented in the source code.
 *
 * **Member functions**
 *
 * #### `SgLongDoubleVal::SgLongDoubleVal ( Sg_File_Info* startOfConstruct = NULL )`
 * This is the constructor.
 * This constructor builds the SgLongDoubleVal base class.
 * - Param `startOfConstruct`: represents the position in the source code
 * - See also:
 * Example:create an SgLongDoubleVal object
 *
 * #### `SgLongDoubleVal::~SgLongDoubleVal()`
 * This is the destructor.
 * Only the Sg_File_Info object can be deleted in this object.
 */
class SgLongDoubleVal;

/** @brief This class represents the concept of a member function declaration statement.
 *
 * The member function declaration is derived from a function declaration and adds
 * specific data members that are relavant.
 * - Note: Constructors, destructors, and conversion operators are handled as member fucntions
 * with names associated with their classes.
 * - Internal: The scope can at times be that of the global scope, when this happens
 * users should access the scope through get_firstNondefiningDeclaration().
 * This appears to be a bug internally.
 *
 * **Data members**
 *
 * #### `SgMemberFunctionDeclaration::p_CtorInitializerList`
 * This is the constructor preinitialization list (used only for the constructor
 * definitions).
 * - Internal: This data member might make more sense in the SgFunctionDefinition (or we could
 * build a SgMemberFunctionDefinition from the SgFunctionDefinition, and put the
 * constructor initialization list there).
 *
 * **Member functions**
 *
 * #### `SgMemberFunctionDeclaration::SgMemberFunctionDeclaration ( Sg_File_Info* startOfConstruct = NULL )`
 * This is the constructor.
 * This constructor builds the SgMemberFunctionDeclaration base class.
 * - Param `startOfConstruct`: represents the position in the source code
 *
 * #### `SgMemberFunctionDeclaration::~SgMemberFunctionDeclaration()`
 * This is the destructor.
 * There are a lot of things to delete, but nothing is deleted in this destructor.
 *
 * #### `SgMemberFunctionDeclaration::isSgMemberFunctionDeclaration (SgNode *s)`
 * Cast function (from derived class to SgMemberFunctionDeclaration pointer).
 * This functions returns a SgMemberFunctionDeclaration pointer for any input of a
 * pointer to an object derived from a SgMemberFunctionDeclaration.
 * - Return: Returns valid pointer to SgMemberFunctionDeclaration if input is derived from a SgLocatedNode.
 *
 * #### `SgMemberFunctionDeclaration::isSgMemberFunctionDeclaration (const SgNode *s)`
 * Cast function (from derived class to SgMemberFunctionDeclaration pointer).
 * This functions returns a SgMemberFunctionDeclaration pointer for any input of a
 * pointer to an object derived from a SgMemberFunctionDeclaration.
 * - Return: Returns valid pointer to SgMemberFunctionDeclaration if input is derived from a SgLocatedNode.
 *
 * #### `SgMemberFunctionDeclaration::get_CtorInitializerList() const`
 * Access function for p_CtorInitializerList.
 * - Return: Returns pointer to SgCtorInitializerList.
 *
 * #### `SgMemberFunctionDeclaration::set_CtorInitializerList(SgCtorInitializerList* CtorInitializerList)`
 * Access function for p_CtorInitializerList.
 * - Return: Returns void.
 */
class SgMemberFunctionDeclaration;

/** @brief This class represents the member function being called and must be assembled in the
 *
 * SgFunctionCall with the function arguments.
 * - Internal:
 * - See also:
 * Example of using a SgMemberFunctionRefExp object
 *
 * **Data members**
 *
 * #### `SgMemberFunctionRefExp::p_symbol_i`
 * This is the SgMemberFunctionSymbol.
 * This binds the function reference to the member function declaration.
 * Note that all SgSymbol objects are shared (just like SgType objects)
 *
 * #### `SgMemberFunctionRefExp::p_virtual_call`
 * This boolean value marks if the function reference is a virtual function.
 *
 * #### `SgMemberFunctionRefExp::p_function_type`
 * This is a pointer to the SgFunctionType associated with this member function reference.
 * - Internal: Notice that it can be a SgFunctionType or any thing derived from that class.
 *
 * #### `SgMemberFunctionRefExp::p_need_qualifier`
 * This boolean value marks if name qualification is required.
 *
 * **Member functions**
 *
 * #### `SgMemberFunctionRefExp::SgMemberFunctionRefExp ( Sg_File_Info* startOfConstruct = NULL )`
 * This is the constructor.
 * This constructor builds the SgMemberFunctionRefExp base class.
 * - Param `startOfConstruct`: represents the position in the source code
 * - See also:
 * Example:create an SgMemberFunctionRefExp object
 *
 * #### `SgMemberFunctionRefExp::~SgMemberFunctionRefExp()`
 * This is the destructor.
 * Only the Sg_File_Info object can be deleted in this object.
 *
 * #### `SgMemberFunctionRefExp::isSgMemberFunctionRefExp (SgNode *s)`
 * Cast function (from derived class to SgMemberFunctionRefExp pointer).
 * This functions returns a SgMemberFunctionRefExp pointer for any input of a
 * pointer to an object derived from a SgMemberFunctionRefExp.
 * - Return: Returns valid pointer to SgMemberFunctionRefExp if input is derived from a SgMemberFunctionRefExp.
 *
 * #### `SgMemberFunctionRefExp::isSgMemberFunctionRefExp (const SgNode *s)`
 * Cast function (from derived class to SgMemberFunctionRefExp pointer).
 * This functions returns a SgMemberFunctionRefExp pointer for any input of a
 * pointer to an object derived from a SgMemberFunctionRefExp.
 * - Return: Returns valid pointer to SgMemberFunctionRefExp if input is derived from a SgMemberFunctionRefExp.
 *
 * #### `SgMemberFunctionRefExp::get_type() const`
 * Get the type associated with this expression
 * Note that the return value is either:
 * -# SgFunctionType : normal function call
 * -# SgMemberFunctionType : normal member function call
 * -# SgTypedefType : in the case of a function call from a pointer
 * It should always be a vailid pointer.  These details are verified in the AST
 * Consistancy Tests.
 * - Return: Returns SgType (but not any SgType).
 *
 * #### `SgMemberFunctionRefExp::set_type()`
 * Set the type associated with this expression
 * This is an internally called function, it sets up the type of the expression
 * based upon the types of the subexpressions (if any). Thus it takes no
 * arguments.
 * - Return: Returns void
 */
class SgMemberFunctionRefExp;

/** @brief This class represents the numeric negation of a value. Not to
 *
 * be confused with SgSubtractOp
 */
class SgMinusOp;

/** @brief This class represents the base class of a number of IR nodes define modifiers
 *
 * within the C++ grammar.
 * Modifiers are use to add suplimental information to types and declarations and other IR
 * nodes within the C and C++ language (const, volatile, public, protected, private, etc.).
 * There are no uses of this IR node anywhere.  All SgModifier based IR nodes
 * are derived from this SgModifier class.
 * - Internal: This is a base class for all SgModifier objects.
 * - See also:
 * Example of using a SgModifier object
 *
 * **Member functions**
 *
 * #### `SgModifier::SgModifier()`
 * This is the constructor.
 * This constructor builds the SgModifier base class.
 * - See also:
 * Example:create an SgModifier object
 *
 * #### `SgModifier::~SgModifier()`
 * This is the destructor.
 * There is nothing to delete in this object.
 *
 * #### `SgModifier::operator=(const SgModifier & X)`
 * This is the assignment operator.
 * This is a simple assignment of the SgBitVector from X to the current object.
 *
 * #### `SgModifier::isSgModifier (SgNode *s)`
 * Cast function (from derived class to SgModifier pointer).
 * This functions returns a SgModifier pointer for any input of a
 * pointer to an object derived from a SgModifier.
 * - Return: Returns valid pointer to SgModifier if input is derived from a SgLocatedNode.
 *
 * #### `SgModifier::isSgModifier (const SgNode *s)`
 * Cast function (from derived class to SgModifier pointer).
 * This functions returns a SgModifier pointer for any input of a
 * pointer to an object derived from a SgModifier.
 * - Return: Returns valid pointer to SgModifier if input is derived from a SgLocatedNode.
 *
 * #### `SgModifier::setBit(unsigned int bit, SgBitVector &bitVector) const`
 * This function sets the bit in the STL vector<bool> object (sets value to true).
 * - Internal: Note that this function take the SgBitVector as a non-const reference.
 * We modified the original implementation in Sage II to use the STL vector<bool>.
 *
 * #### `SgModifier::unsetBit(unsigned int bit, SgBitVector &bitVector) const`
 * This function clears the bit in the STL vector<bool> object (sets value to false).
 * - Internal: Note that this function take the SgBitVector as a non-const reference.
 * We modified the original implementation in Sage II to use the STL vector<bool>.
 *
 * #### `SgModifier::checkBit(unsigned int bit, const SgBitVector &bitVector) const`
 * This function returns the value of the bit in the STL vector<bool> object.
 * - Internal: Note that this function take the SgBitVector as a const reference.
 */
class SgModifier;

/** @brief This class is not used in ROSE, but is intended to represent a list of SgModifierTypes
 *
 * (similar to the SgTypedefSeq IR node) used for the SgType IR node that points to this SgModifierNodes.
 * In the future, we may either support this concept (similar to SgTypedefSeq) or we may remove
 * the related implementation of SgTypedefSeq to be consistant by design).
 * - Internal: This IR node could likely be removed at some point in the future.
 */
class SgModifierNodes;

/** @brief This class represents strings within the IR nodes.
 *
 * It contains a number of operators that make it similar to the C++ string class.
 * - Internal: This class internally contains a "char*" C style string.
 * - Todo: Define a string conversion operator so that we can handle
 * "SgName name; string s = name;"  This would start the process of
 * internally having SgName contain a C++ style string.
 * - Todo: Change SgName to store a C++ style std::string, instead of a C style char*.
 * - Todo: Some of the member functions defined in this class will be removed
 * (head(), tail(), etc.) because they represent low level string handling which is best
 * done on a C++ style string more directly using C++ string operators.
 * - See also:
 * Example of using a SgName object
 *
 * **Data members**
 *
 * #### `SgName::p_char`
 * This pointer points to an internal C style string.
 *
 * **Member functions**
 *
 * #### `SgName::SgName()`
 * This is the constructor.
 * This constructor builds the SgName base class.
 * - See also:
 * Example:create an SgName object
 *
 * #### `SgName::~SgName()`
 * This is the destructor.
 * There is nothing to delete in this object.
 *
 * #### `SgName::isSgName (SgNode *s)`
 * Cast function (from derived class to SgName pointer).
 * This functions returns a SgName pointer for any input of a
 * pointer to an object derived from a SgName.
 * - Return: Returns valid pointer to SgName if input is derived from a SgLocatedNode.
 *
 * #### `SgName::isSgName (const SgNode *s)`
 * Cast function (from derived class to SgName pointer).
 * This functions returns a SgName pointer for any input of a
 * pointer to an object derived from a SgName.
 * - Return: Returns valid pointer to SgName if input is derived from a SgLocatedNode.
 */
class SgName;

/** @brief This class represents the concept of a C++ namespace alias declaration statement.
 *
 * For a namespace Y, this appears in the code as "namespace X = Y;".  In general
 * "X" is a shorter name for what in "Y" might be unwieldy.
 * - See also:
 * Example of using a SgNamespaceAliasDeclarationStatement object
 *
 * **Data members**
 *
 * #### `SgNamespaceAliasDeclarationStatement::p_name`
 * This the name of the new namespace alias (usually a shorter name).
 *
 * #### `SgNamespaceAliasDeclarationStatement::p_namespaceDeclaration`
 * This the namespace to which the alis references.
 *
 * **Member functions**
 *
 * #### `SgNamespaceAliasDeclarationStatement::SgNamespaceAliasDeclarationStatement ( Sg_File_Info* startOfConstruct = NULL )`
 * This is the constructor.
 * This constructor builds the SgNamespaceAliasDeclarationStatement base class.
 * - Param `startOfConstruct`: represents the position in the source code
 *
 * #### `SgNamespaceAliasDeclarationStatement::~SgNamespaceAliasDeclarationStatement()`
 * This is the destructor.
 * There are a lot of things to delete, but nothing is deleted in this destructor.
 *
 * #### `SgNamespaceAliasDeclarationStatement::isSgNamespaceAliasDeclarationStatement (SgNode *s)`
 * Cast function (from derived class to SgNamespaceAliasDeclarationStatement pointer).
 * This functions returns a SgNamespaceAliasDeclarationStatement pointer for any input of a
 * pointer to an object derived from a SgNamespaceAliasDeclarationStatement.
 * - Return: Returns valid pointer to SgNamespaceAliasDeclarationStatement if input is derived from a SgLocatedNode.
 *
 * #### `SgNamespaceAliasDeclarationStatement::isSgNamespaceAliasDeclarationStatement (const SgNode *s)`
 * Cast function (from derived class to SgNamespaceAliasDeclarationStatement pointer).
 * This functions returns a SgNamespaceAliasDeclarationStatement pointer for any input of a
 * pointer to an object derived from a SgNamespaceAliasDeclarationStatement.
 * - Return: Returns valid pointer to SgNamespaceAliasDeclarationStatement if input is derived from a SgLocatedNode.
 *
 * #### `SgNamespaceAliasDeclarationStatement::get_name() const`
 * Access function for p_name.
 * - Return: Returns SgName.
 *
 * #### `SgNamespaceAliasDeclarationStatement::set_name(SgName name)`
 * Access function for p_name.
 * - Return: Returns void.
 *
 * #### `SgNamespaceAliasDeclarationStatement::get_namespaceDeclaration() const`
 * Access function for p_namespaceDeclaration.
 * - Return: Returns SgNamespaceDeclarationStatement.
 *
 * #### `SgNamespaceAliasDeclarationStatement::set_namespaceDeclaration(SgNamespaceDeclarationStatement* namespaceDeclaration)`
 * Access function for p_namespaceDeclaration.
 * - Return: Returns void.
 */
class SgNamespaceAliasDeclarationStatement;

/** @brief This class represents the concept of a C++ namespace declaration.
 *
 * The namespace declaration is differernt from many other declarations in that
 * there is not concept of a namespace declaration that does not have an associated
 * definition (SgNamespaceDefinitionStatement). In a sense each SgNamespaceDeclarationStatement
 * is a defining declaration, yet there can be more than one of them, so this concept of
 * a defining declaration doesn't fit were we consider that there must be only a single
 * defining declaration under the One-time Definition Rule (ODR).  This we don't consider
 * that SgNamespaceDeclarationStatement as a defining declaration.  As a result:
 * - The defining declaration returned by get_definindDeclaration() is always NULL, and
 * - The get_firstNondefiningDeclaration() member function will:
 * - Always return a valid pointer, and
 * - Will point to the first declaration of SgNamespaceDeclarationStatement
 * The scope of SgNamespaceDeclarationStatement is computed structurally from the
 * parent information.
 * - Todo: Consider having a function which could generate a list of all the
 * SgNamespaceDeclarationStatement IR nodes that match the same namspace.
 * This would make a good first project for a new student.
 * - Todo: Include a graph to show how scopes are handled within the AST.
 * - See also:
 * Example of using a SgNamespaceDeclarationStatement object
 *
 * **Data members**
 *
 * #### `SgNamespaceDeclarationStatement::p_name`
 * This the name of the new namespace alias (usually a shorter name).
 *
 * #### `SgNamespaceDeclarationStatement::p_definition`
 * This pointer points to the SgNamespaceDefinitionStatement, which holds the
 * declarations within the namespace.
 *
 * #### `SgNamespaceDeclarationStatement::p_isUnnamedNamespace`
 * Records special case of an unnamed namespace.
 * - Internal: Marking the IR nodes as unnamed is better than just having an empty string for
 * p_name.  This provides a reasonable mechanism to implement error checking internally.
 *
 * #### `SgNamespaceDeclarationStatement::p_scope`
 * This a pointer to the scope of the first declaration for the namespace.
 *
 * **Member functions**
 *
 * #### `SgNamespaceDeclarationStatement::SgNamespaceDeclarationStatement ( Sg_File_Info* startOfConstruct = NULL )`
 * This is the constructor.
 * This constructor builds the SgNamespaceDeclarationStatement base class.
 * - Param `startOfConstruct`: represents the position in the source code
 *
 * #### `SgNamespaceDeclarationStatement::~SgNamespaceDeclarationStatement()`
 * This is the destructor.
 * There are a lot of things to delete, but nothing is deleted in this destructor.
 *
 * #### `SgNamespaceDeclarationStatement::isSgNamespaceDeclarationStatement (SgNode *s)`
 * Cast function (from derived class to SgNamespaceDeclarationStatement pointer).
 * This functions returns a SgNamespaceDeclarationStatement pointer for any input of a
 * pointer to an object derived from a SgNamespaceDeclarationStatement.
 * - Return: Returns valid pointer to SgNamespaceDeclarationStatement if input is derived from a SgLocatedNode.
 *
 * #### `SgNamespaceDeclarationStatement::isSgNamespaceDeclarationStatement (const SgNode *s)`
 * Cast function (from derived class to SgNamespaceDeclarationStatement pointer).
 * This functions returns a SgNamespaceDeclarationStatement pointer for any input of a
 * pointer to an object derived from a SgNamespaceDeclarationStatement.
 * - Return: Returns valid pointer to SgNamespaceDeclarationStatement if input is derived from a SgLocatedNode.
 *
 * #### `SgNamespaceDeclarationStatement::get_name() const`
 * Access function for p_name.
 * - Return: Returns SgName.
 *
 * #### `SgNamespaceDeclarationStatement::set_name(SgName name)`
 * Access function for p_name.
 * - Return: Returns void.
 *
 * #### `SgNamespaceDeclarationStatement::get_definition() const`
 * Returns pointer to SgNamespaceDefinitionStatement.
 * - Return: Returns pointer to SgNamespaceDefinitionStatement.
 *
 * #### `SgNamespaceDeclarationStatement::set_definition(SgNamespaceDefinitionStatement* definition)`
 * Access function for p_definition.
 * - Return: Returns void.
 *
 * #### `SgNamespaceDeclarationStatement::get_isUnnamedNamespace() const`
 * Access function for p_isUnnamedNamespace.
 * - Return: Returns SgName.
 *
 * #### `SgNamespaceDeclarationStatement::set_isUnnamedNamespace(bool isUnnamedNamespace)`
 * Access function for p_isUnnamedNamespace.
 * - Return: Returns void.
 */
class SgNamespaceDeclarationStatement;

/** @brief This class represents the concept of a namespace definition.
 *
 * Namespace definitions
 * are coupled with namespace declarations to defin the namespace (hold the list of
 * declarations in the namespace.  Within C++ namespaces are "reentrant" and
 * as a result multiple namespace declarations (SgNamespaceDeclarationStatement)
 * and definitions (SgNamespaceDefinitionStatement) may exist for a single namespace.
 * - Note: Note that the namespace "std" is special in C++, such that a program
 * with the statement "using namespace std" can exist all by itself and is a
 * valid program (even though "std" as a namespace is not defined).
 * Scopes are an important aspect of language design. They allow
 * declarations to have a local context and so promote good programming style.
 * Scope statments in C++ include a number of different kinds of statements;
 * the SgNamespaceDefinitionStatement is a base class for these. Each scope statement contains
 * a symbol table and the SgNamespaceDefinitionStatements role is mostly to provide this
 * symbol table and an interface to accessing it.
 * - Internal: This is a base class for scope statements.
 * - Todo: Cleanup interface which presently has multiple append,prepend, insert functions. It
 * might be best to eliminate them and use STL directly.
 *
 * **Data members**
 *
 * #### `SgNamespaceDefinitionStatement::p_declarations`
 * This is an STL list of SgDeclarationStatement objects.
 * - Internal: The name is perhaps all to similar to p_declaration (which exists on many IR nodes).
 *
 * #### `SgNamespaceDefinitionStatement::p_namespaceDeclaration`
 * This is a pointer to the SgNamespaceDeclarationStatement.
 * - Internal: The name would have been p_declaration, except that such a name is too close to
 * p_declarations.  These naming details might be fixed later.
 *
 * **Member functions**
 *
 * #### `SgNamespaceDefinitionStatement::SgNamespaceDefinitionStatement ( Sg_File_Info* startOfConstruct = NULL )`
 * This is the constructor.
 * This constructor builds the SgNamespaceDefinitionStatement base class.
 * - Param `startOfConstruct`: represents the position in the source code
 *
 * #### `SgNamespaceDefinitionStatement::~SgNamespaceDefinitionStatement()`
 * This is the destructor.
 * There are a lot of things to delete, but nothing is deleted in this destructor.
 *
 * #### `SgNamespaceDefinitionStatement::isSgNamespaceDefinitionStatement (SgNode *s)`
 * Cast function (from derived class to SgNamespaceDefinitionStatement pointer).
 * This functions returns a SgNamespaceDefinitionStatement pointer for any input of a
 * pointer to an object derived from a SgNamespaceDefinitionStatement.
 * - Return: Returns valid pointer to SgNamespaceDefinitionStatement if input is derived from a SgLocatedNode.
 *
 * #### `SgNamespaceDefinitionStatement::isSgNamespaceDefinitionStatement (const SgNode *s)`
 * Cast function (from derived class to SgNamespaceDefinitionStatement pointer).
 * This functions returns a SgNamespaceDefinitionStatement pointer for any input of a
 * pointer to an object derived from a SgNamespaceDefinitionStatement.
 * - Return: Returns valid pointer to SgNamespaceDefinitionStatement if input is derived from a SgLocatedNode.
 *
 * #### `SgNamespaceDefinitionStatement::get_qualified_name() const`
 * Returns SgName (a string) representing the name of the current scope.
 * See discussion of mangled names in the documentation.
 * - Return: Returns SgName (a string).
 *
 * #### `SgNamespaceDefinitionStatement::copy(const SgCopyHelp & help)`
 * Makes a copy (deap of shallow depending on SgCopyHelp).
 * - Return: Returns pointer to copy of SgNamespaceDefinitionStatement.
 */
class SgNamespaceDefinitionStatement;

/** @brief This class represents the concept of a namespace name within the compiler.
 *
 * This symbol holds a reference to a SgNamespaceDeclarationStatement and also
 * any of its alias (referencing a SgNamespaceAliasDeclarationStatement).
 * - Internal: The constructor for this symbol takes a name and a declaration.
 * The reason is that it is used to handle the case of a namespace alias which uses
 * the same symbol but uses a different name.  I'm not certain that this is
 * the best implementation since it stores the symbol in the symbol table under
 * two names.  It is not clear if that would be clear if the name were changes and
 * this could cause the symbol table to not be completely removed from the
 * symbol table.
 * - Todo: It might be that we should have a SgNamespaceAliasSymbol so that namespace
 * aliasing can be better supported.  We can consider this for future work.
 * - See also: SgSymbol for more details about symbols and why they are used in ROSE.
 * - See also:
 * Example of using a SgNamespaceSymbol object
 *
 * **Member functions**
 *
 * #### `SgNamespaceSymbol::SgNamespaceSymbol()`
 * This is the default constructor.
 * This constructor builds the SgNamespaceSymbol base class.
 * - See also:
 * Example:create an SgNamespaceSymbol object
 *
 * #### `SgNamespaceSymbol::~SgNamespaceSymbol()`
 * This is the destructor.
 *
 * #### `SgNamespaceSymbol::get_name() const`
 * Access function for getting name stored internally (not the one in the declaration).
 * - Return: Returns SgName.
 *
 * #### `SgNamespaceSymbol::get_type() const`
 * This function returns the type associated with the named entity.
 * - Return: Returns SgType*.
 *
 * #### `SgNamespaceSymbol::get_declaration() const`
 * Access function for getting the declaration of the original namespace.
 * - Return: Returns SgNamespaceDeclarationStatement.
 *
 * #### `SgNamespaceSymbol::set_declaration(SgNamespaceDeclarationStatement *declaration)`
 * Access function for setting the declaration stored internally.
 * - Return: Returns SgNamespaceDeclarationStatement.
 *
 * #### `SgNamespaceSymbol::isSgNamespaceSymbol (SgNode *s)`
 * Cast function (from derived class to SgNamespaceSymbol pointer).
 * This functions returns a SgNamespaceSymbol pointer for any input of a
 * pointer to an object derived from a SgNamespaceSymbol.
 * - Return: Returns valid pointer to SgNamespaceSymbol if input is derived from a SgNamespaceSymbol.
 *
 * #### `SgNamespaceSymbol::isSgNamespaceSymbol (const SgNode *s)`
 * Cast function (from derived class to SgNamespaceSymbol pointer).
 * This functions returns a SgNamespaceSymbol pointer for any input of a
 * pointer to an object derived from a SgNamespaceSymbol.
 * - Return: Returns valid pointer to SgNamespaceSymbol if input is derived from a
 * SgNamespaceSymbol.
 */
class SgNamespaceSymbol;

/** @brief This class represents the notion of an n-ary boolean operation.
 *
 * This node is intended for use with Python.
 * - Internal: Parent class SgNaryOp represents n-ary operations using two
 * lists: one of SgExpressions (the operands) and another of variant
 * enums that represent operations (V_SgAddOp, etc.). Therefore, the
 * operator at index i in the operator list operates on the operands at
 * indices i and i+1 in the operand list.
 */
class SgNaryBooleanOp;

/** @brief This class represents the notion of an n-ary comparison operation.
 *
 * This node is intended for use with Python.
 * - Internal: Parent class SgNaryOp represents n-ary operations using two
 * lists: one of SgExpressions (the operands) and another of variant
 * enums that represent operations (V_SgAddOp, etc.). Therefore, the
 * operator at index i in the operator list operates on the operands at
 * indices i and i+1 in the operand list.
 */
class SgNaryComparisonOp;

/** @brief This class represents the notion of an n-ary operator.
 *
 * This node is intended for use with Python.
 * - Internal: SgNaryOp represents n-ary operations using two lists: one of
 * SgExpressions (the operands) and another of variant enums that
 * represent operations (V_SgAddOp, etc.). Therefore, the operator at
 * index i in the operator list operates on the operands at indices i and
 * i+1 in the operand list.
 *
 * **Data members**
 *
 * #### `SgExpressionPtrList SgNaryOp::p_operands`
 * This is the list of operands associated with this n-ary operator.
 *
 * #### `VariantTList SgNaryOp::p_operators`
 * This is the list of operators associated with this n-ary operator.
 *
 * **Member functions**
 *
 * #### `SgNaryOp::SgNaryOp ( SgExpression* first_operand )`
 * This is the constructor.
 * This constructor builds the SgNaryOp base class.
 * - Param `first_operand`: The first operand in the n-ary operation.
 *
 * #### `SgNaryOp::~SgNaryOp()`
 * This is the destructor.
 *
 * #### `SgNaryOp::isSgNaryOp (SgNode *s)`
 * Cast function (from derived class to SgNaryOp pointer).
 * This functions returns a SgNaryOp pointer for any input of a
 * pointer to an object derived from a SgNaryOp.
 * - Return: Returns valid pointer to SgNaryOp if input is derived from a SgNaryOp.
 *
 * #### `SgNaryOp::isSgNaryOp (const SgNode *s)`
 * Cast function (from derived class to SgNaryOp pointer).
 * This functions returns a SgNaryOp pointer for any input of a
 * pointer to an object derived from a SgNaryOp.
 * - Return: Returns valid pointer to SgNaryOp if input is derived from a SgNaryOp.
 *
 * #### `SgNaryOp::append_operation(VariantT operator, SgExpression* operand)`
 * Adds a new operation to this n-ary operator.
 * - Param `operator`: - the VariantT of the corresponding operation node (V_SgAddOp, V_SgMinusOp, etc)
 * - Param `operand`: - the next expression after the given operator.
 * - Return: Returns void.
 */
class SgNaryOp;

/** @brief This class represents the concept of a C++ call to the new operator.
 *
 * - Todo: Provide some examples to detail the difference between placement, constructor, and
 * builtin arguments.
 * - Todo: I believe we can associate the constructors from the class with
 * new operators.
 * - Internal: This is one of the few SgExpression IR nodes that are required to store
 * the internal type explicitly.  In this case the p_specified_type is used and
 * it is set to the type specified in the new expression (e.g. int *x = new int[1000];,
 * the p_expression_type would be SgTypeInt.  However the return type of the new operator
 * is a pointer to this type so the get_type() member function returns a pointer to this
 * type.
 * - See also:
 * Example of using a SgNewExp object
 *
 * **Data members**
 *
 * #### `SgNewExp::p_expression_type`
 * This pointer points to the type of the variable being allocated.
 *
 * #### `SgNewExp::p_placement_args`
 * This is a pointer to the memory allocation placement arguments for the new
 * operator.
 * An example would be "class X{}; void* p; X* item2 = new (p) X();".  Placement arguments
 * permit the new opperator to allocate space at a specific location (address).
 * - Todo: Provide some examples to detail the difference between placement, constructor, and
 * builtin arguments (these are rarely used in C++).
 *
 * #### `SgNewExp::p_constructor_args`
 * This is a pointer to the constructor initializer (which holds its constructor arguments) for the new operator.
 * - Todo: Provide some examples to detail the difference between placement, constructor, and
 * builtin arguments.
 *
 * #### `SgNewExp::p_builtin_args`
 * This is a pointer to the builtin arguments for the new operator (typically the
 * "this" pointer where specified optionally).
 * - Todo: Provide some examples to detail the difference between placement, constructor, and
 * builtin arguments.
 *
 * #### `SgNewExp::p_need_global_specifier`
 * This new operator needs to be output with "::".
 * - Internal: This should be a bool instead of a short.
 *
 * **Member functions**
 *
 * #### `SgNewExp::SgNewExp ( Sg_File_Info* startOfConstruct = NULL, SgClassSymbol *symbol = NULL )`
 * This is the constructor.
 * This constructor builds the SgNewExp base class.
 * - Param `startOfConstruct`: represents the position in the source code
 *
 * #### `SgNewExp::~SgNewExp()`
 * This is the destructor.
 * There are a lot of things to delete, but nothing is deleted in this destructor.
 *
 * #### `SgNewExp::isSgNewExp (SgNode *s)`
 * Cast function (from derived class to SgNewExp pointer).
 * This functions returns a SgNewExp pointer for any input of a
 * pointer to an object derived from a SgNewExp.
 * - Return: Returns valid pointer to SgNewExp if input is derived from a SgLocatedNode.
 *
 * #### `SgNewExp::isSgNewExp (const SgNode *s)`
 * Cast function (from derived class to SgNewExp pointer).
 * This functions returns a SgNewExp pointer for any input of a
 * pointer to an object derived from a SgNewExp.
 * - Return: Returns valid pointer to SgNewExp if input is derived from a SgLocatedNode.
 *
 * #### `SgNewExp::get_variable() const`
 * Access function for p_variable.
 * - Return: Returns pointer to SgExpression.
 *
 * #### `SgNewExp::set_variable(SgExpression* variable)`
 * Access function for p_variable.
 * - Param `variable`: is the variable to be deleted.
 * - Return: Returns void.
 *
 * #### `SgNewExp::get_is_array() const`
 * Access function for p_is_array.
 * - Return: Returns bool.
 *
 * #### `SgNewExp::set_is_array(SgExpression* is_array)`
 * Access function for p_is_array.
 * - Param `is_array`: flag to record use of array delete.
 * - Return: Returns void.
 *
 * #### `SgNewExp::get_need_global_specifier() const`
 * Access function for p_need_global_specifier.
 * - Return: Returns bool.
 *
 * #### `SgNewExp::set_need_global_specifier(SgExpression* need_global_specifier)`
 * Access function for p_need_global_specifier.
 * - Param `need_global_specifier`: flag to record use of array delete.
 * - Return: Returns void.
 */
class SgNewExp;

/** @brief This class represents the base class for all IR nodes within Sage III.
 *
 * This class is used as a base class for all IR nodes in Sage III.
 * - Note: AST attributes should not be used for the following IR nodes:
 * -# SgName
 * -# SgModifier (and all derived modifier IR nodes)
 * -# SgUnparse_Info
 * -# SgSymbolTable
 * - Internal: Need to remove unused error() and variant() functions.
 * - Todo: The "AstAttributeMechanism attribute;" field should be fixed:
 * - The attribute within this class is not added using ROSETTA and is one of the last
 * (if not the last) variable to NOT be added to the IR nodes using ROSETTA.  Because it is
 * not added using ROSETTA, it is a special case (and we hate special cases) in the file
 * I/O (by Jochen) and perhaps elsewhere.  This variable should be added using ROSETTA!
 * - Access functions need to be built for the "AstAttributeMechanism attribute;"
 * field and it needs to be moved to be a private data member.
 * - It should be moved to only those IR nodes were it makes sense, e.g. excluded from:
 * -# SgName
 * -# SgModifier (and all derived modifier IR nodes)
 * -# SgUnparse_Info
 * -# SgSymbolTable
 * - Todo: Consider eliminating the set_freepointer() function since only the internal memory
 * allocation mechanisms should use it (and they are forced to access the data member
 * directly since they traverse the memory pools directly and member function can onl be
 * called on allocated object initialized vi the new operator (with a proper constructor
 * call, so that the this pointer is set properly)).  Perhaps we don't need access
 * functions for this data member at all.
 * - See also:
 * Example of using a SgNode object
 * - See also:
 * Enforced AST Properties
 *
 * **Data members**
 *
 * #### `SgNode::attribute`
 * This is the new attribute mechanism.
 * This is part of a new attribute mechanism. It is difference
 * from the one originally used by Sage II.
 *
 * #### `SgNode::p_parent`
 * This is the pointer to the parent IR node in the AST.
 * This is the pointer to the parent IR node.  It is a valid pointer
 * on all nodes that are traversed (SgExpressions, SgStatements, SgInitializedName, etc.)
 * However it is not set on SgTypes and SgSymbols,both of which are shared internally.
 * This pointer is mostly set in post processing of the Sage III AST, until
 * this point it is not reliable.
 *
 * #### `SgNode::p_isVisited`
 * This the visit flag previously used by the AST traversals.
 * This the visit flag previously used by the AST traversals. It is
 * no longer used in the AST traversals, though the traversals can be
 * set at compile-time to alternatively use this visit flag.  The visit
 * flag is part of an older design of the AST traversal, it was problematic
 * by design, because it had to be reset after each traversal.  It also
 * placed requirements on any newly added IR nodes within the AST (they
 * had to be set so just right so that they could be properly reset).
 * The AST traversals are greatly simplified as a result of no longer
 * requiring this visit flag.  This variable will be removed in the
 * future.
 * - Deprecated: Old traversal supporting mechanism (will be removed).
 *
 * #### `SgNode::p_freepointer`
 * This is the pointer to the chain of previously freed objects.
 * - Internal: This is part of the support for memory pools within ROSE.  The freepointer is
 * only manipulated by the delete operator which constructs a chain of previously freed
 * objects embedded within the memory pools.  The chain of objects link by the freepointer
 * variables are traversed by the new operator to allocate (reuse) previously deleted objects.
 * The new operator does not reset the freepointers since once allocated the freepointer is
 * ignored.
 *
 * #### `SgNode::p_isModified`
 * Records if IR node has been modified (data members reset).
 * This is part of an internal mechanism that records if IR nodes have
 * been modified, either by a transformation or a access function to set
 * a value on the IR node.  All access functions that set IR node data members
 * are automatically generated (except the ones for this data member) and include
 * code to set this boolean flag to true.  This is also part of the
 * support for copy based code generation where source file is copied except
 * where the AST was modified an for these subtrees new code is generated
 * from the AST.
 * - Internal: true if IR node has been modified, else false.
 *
 * #### `SgNode::p_globalFunctionTypeTable`
 * Pointer to symbol table specific to function types.
 * - Internal: Always a valid pointer.
 *
 * #### `SgNode::p_globalMangledNameMap`
 * Cache of mangled names to avoid regeneration of previously build mangled names or
 * parts of mangled names.  This is a performance optimization.
 * - Internal: This cache should be cleared automatically where required.
 *
 * #### `SgNode::p_shortMangledNameCache`
 * STL map used as a cache to shorten generated mangled names.
 * This is mostly a space optimization ofr mangled names of templates.
 * - Internal: This cache should be cleared before regenerating mangled names.
 *
 * **Member functions**
 *
 * #### `SgNode::copy(const SgCopyHelp & help ) const`
 * This function clones the current IR node object recursively or not, depending on the argument
 * This function performs a copy based on the specification of the input parameter.
 * The input parameter is used to determin which data members are copied by reference
 * and which are copied by value.
 * - Param `help`: - If this argument is of type SgTreeCopy, then the
 * IR node is cloned recursively. If its of type SgShallowCopy
 * only the first level of the IR node is copied, everything else is
 * left pointing to the the original IR node's object's data members.
 * - Return: a pointer to the new clone.
 * - Internal: It appears the the copy functions don't set the parents of anything that they do
 * a deep copy of! This can cause AST tests to fail. In particular some functions that
 * require the parent pointers to be valid will return NULL pointers (e.g. SgInitializedName::get_declaration()).
 * It might be that we should allow this to be done as part of the
 * SgCopyHelp::clone function or perhaps another member function of SgCopyHelp would be
 * useful for this support.  It is not serious if the AST post processing is done since
 * that will set any NULL pointers that are found within its traversal.
 * - Exception: none No exceptions are thrown by this function.
 *
 * #### `SgNode::SgNode()`
 * This is the constructor
 * This constructor builds a SgNode, always used as a base class.
 * All Sage III IR nodes are derived from this class.
 * - See also:
 * Example:create an SgNode object
 *
 * #### `SgNode::~SgNode()`
 * This is the destructor.
 * There is nothing to delete in this object.
 *
 * #### `SgNode::sage_class_name() const`
 * generates string representing the class name: (e.g. for SgNode returns "SgNode").
 * This function is useful for debugging and error reporting.  It
 * returns the name of the IR node.
 * - Deprecated: Use class_name() which returns a C++ string object.
 * - Return: a char* pointer to a C style string.
 *
 * #### `string SgNode::class_name() const`
 * generates string representing the class name: (e.g. for SgNode returns "SgNode").
 * This function is useful for debugging and error reporting.  It
 * returns the name of the IR node.
 * - Return: a C++ string object.
 *
 * #### `SgNode::variant() const`
 * Older version function returns enum value "NODE"
 * This function is an older version of the variant function.
 * This function is useful for debugging and error reporting.  It
 * returns the name of the IR node.
 * - Return: an int value.
 * - Deprecated: Use variantT() instead.  Older enum values have inconsistant
 * names and are being removed.
 *
 * #### `SgNode::getVariant() const`
 * Older version function returns enum value "NODE"
 * This function is an older version of the variant function.
 * This function is useful for debugging and error reporting.  It
 * returns the name of the IR node.
 * - Return: an int value.
 * - Deprecated: Use variantT() instead.  Older enum values have inconsistant
 * names and are being removed.
 *
 * #### `SgNode::variantT() const`
 * returns enum value "V_SgNode"
 * This function is useful for debugging and error reporting.  It
 * returns the name of the IR node.
 * Once the variant() function is removed this function will be
 * deprecated and replaced by a new function named "variant()".  The new "variant()"
 * function will return the new enum type (with consistant naming convention in place).
 * The name of the enum type might be changed to make it more clear.
 * - Return: an enum value (type: VariantT).
 *
 * #### `SgNode::get_isModified() const`
 * **FOR** **INTERNAL** **USE** All nodes in the AST contain a isModified flag.
 * This flag can be set but this is typically an internal function used to track the updates to AST.
 * - Return: Returns bool; true if IR node has been modified.
 *
 * #### `SgNode::set_isModified(bool isModified)`
 * Acess function for isModified flag.
 * This flag records if the current IR node has been modified. It is set to false after
 * and ROSE front-end processing.
 *
 * #### `SgNode::get_isVisited() const`
 * DOCS IN SgNode.docs: Access function for p_isVisited flag used previously by the AST traversals.
 * - Deprecated: This function is not used and will be removed.
 * - Return: Returns bool; true if previously visited within current AST traversal.
 *
 * #### `SgNode::set_isVisited(bool isVisited)`
 * DOCS IN SgNode.docs: Access function for p_isVisited flag used previously by the AST traversals.
 * - Deprecated: This function is not used and will be removed.
 *
 * #### `SgNode::isSgNode (SgNode *s)`
 * Cast function (from derived class to SgNode pointer).
 * This functions returns a SgNode pointer for any input of a
 * pointer to an object derived from a SgNode.
 * - Return: Returns valid pointer to SgNode if input is derived from a SgNode.
 *
 * #### `SgNode::isSgNode (const SgNode *s)`
 * Cast function (from derived class to SgNode pointer).
 * This functions returns a SgNode pointer for any input of a
 * pointer to an object derived from a SgNode.
 * - Return: Returns valid pointer to SgNode if input is derived from a SgNode.
 *
 * #### `SgNode::set_parent (SgNode *parent)`
 * Sets parent node for any IR node.
 * The parent node in the AST can be set but this is typically
 * an internal function used to build the AST.
 * - Param `parent`: - Pointer to parent node to store within the current IR node.
 * - Return: returns void.
 *
 * #### `SgNode::get_parent() const`
 * Access function for parent node in AST.
 * The parent node in the AST can be accessed, in general only the project node, symbols and
 * types should be NULL.  Since parent nodes are set within post processing
 * (and using the ROSE AST traveral mechanism) the parents are guarenteed to
 * match the traversal, and no other concept of what could be a parent within
 * the AST (e.g a parent concept based on scope).  Because the traversal is based
 * on the source code layout, what is a parent is similarly based on the source
 * code layout and not any concept of scope.  Note that the scope of relavant
 * IR nodes is stored explicitly in the AST, precisely because it is not always
 * related to the layout of the source code (and thus not related to the concept
 * of parent in the AST).
 * - Return: Returns pointer to SgNode
 *
 * #### `SgNode::unparseToString( SgUnparse_Info* info )`
 * This function unparses the AST node (source code only, excluding comments and white space)
 * - Param `info`: is optional (used only to specify code generation options).
 * This function is useful for converting AST nodes to strings as part of general
 * debugging or the construction of other strings for use as input to the AST rewrite
 * mechansims which accepts source code strings.  See tutorial for examples of this.
 * This function uses the SgUnparse_Info as an inherited attribute internally
 * and using this attribute it will correctly handled many subtle details that
 * will be ignored if the attribute is not provided.  For example, the SgUnparse_Info
 * can record if the statement is in a conditional and if so then the trailing ";"
 * will be omitted in the generated code. See the SgUnparse_Info documentation for
 * the numerous other internal settings that can effect the generated code.  Because
 * of these details, the unparseToString() function can not always be used to generate
 * compiliable code.
 * - Return: Returns std::string
 *
 * #### `SgNode::unparseToCompleteString()`
 * This function unparses the AST node (including comments and white space)
 * This function is a complement to the unparseToString() function and includes
 * any associated comments and preprocessor control directives.  Because C preprocessor
 * control directive can be included string generated using this function may or may
 * not be appropriate for use as input to the AST rewrite mechanism.
 * - Todo: This function needs a better name since it is unclear what the "complete" string is.
 * - Return: Returns std::string
 *
 * #### `SgNode::get_traversalSuccessorContainer()`
 * **FOR** **INTERNAL** **USE** within ROSE traverals mechanism only.
 * This function builds and returns a copy of ordered container
 * holding pointers to children of this node in a traversal. It is
 * associated with the definition of a tree that is travered by the
 * AST traversal mechanism; a tree that is embeded in the AST (which
 * is a more general graph).  This function is used within the implementation
 * of the AST traversal and has a semantics may change in subtle ways
 * that makes it difficult to use in user code.  It can return unexpected
 * data members and thus the order and the number of elements is unpredicable
 * and subject to change.
 * - Warning: This function can return unexpected data members and thus the
 * order and the number of elements is unpredicable and subject
 * to change.
 * - Return: Returns ordered STL Container of pointers to children nodes in AST.
 *
 * #### `SgNode::get_traversalSuccessorNamesContainer()`
 * **FOR** **INTERNAL** **USE** within ROSE traverals mechanism only.
 * This function builds and returns a copy of ordered container
 * holding strings used to name data members that are traversed in the IR
 * node. It is associated with the definition of a tree that is travered by the
 * AST traversal mechanism; a tree that is embeded in the AST (which
 * is a more general graph).  This function is used within the implementation
 * of the AST traversal and has a semantics may change in subtle ways
 * that makes it difficult to use in user code.  It can return unexpected
 * data members and thus the order and the number of elements is unpredicable
 * and subject to change.
 * - Warning: This function can return unexpected data members and thus the
 * order and the number of elements is unpredicable and subject
 * to change.
 * Each string is a name of a member variable holding a pointer to a
 * child in the AST. The names are the same as used in the generated enums for
 * accessing attributes in a traversal. The order is the same in which they are
 * traversed and the same in which the access enums are defined. Therefore this
 * method can be used to get the corresponding name (string) of an access enum
 * which allows to produce more meaningful messages for attribute computations.
 * - Return: Returns ordered STL container of names (strings) of access names to children nodes in AST.
 *
 * #### `SgNode::roseRTI ()`
 * **FOR** **INTERNAL** **USE** Access to Runtime Type Information (RTI) for this IR nodes.
 * This function provides runtime type information for accessing the
 * structure of the current node.  It is useful for generating code which
 * would dump out or rebuild IR nodes.
 * - Return: Returns a RTIReturnType object (runtime type information).
 */
class SgNode;

/** @brief This class represents an object used to initialize the unparsing.
 *
 * There are no uses of this IR nodes anywhere in the AST.
 * - Internal: This is an object used only internally.
 * - Todo: Review if we really want to keep this class.
 * - See also:
 * Example of using a SgOptions object
 *
 * **Member functions**
 *
 * #### `SgOptions::SgOptions()`
 * This is the constructor.
 * This constructor builds the SgOptions base class.
 * - See also:
 * Example:create an SgOptions object
 *
 * #### `SgOptions::~SgOptions()`
 * This is the destructor.
 * There is nothing to delete in this object.
 *
 * #### `SgOptions::isSgOptions (SgNode *s)`
 * Cast function (from derived class to SgOptions pointer).
 * This functions returns a SgOptions pointer for any input of a
 * pointer to an object derived from a SgOptions.
 * - Return: Returns valid pointer to SgOptions if input is derived from a SgLocatedNode.
 *
 * #### `SgOptions::isSgOptions (const SgNode *s)`
 * Cast function (from derived class to SgOptions pointer).
 * This functions returns a SgOptions pointer for any input of a
 * pointer to an object derived from a SgOptions.
 * - Return: Returns valid pointer to SgOptions if input is derived from a SgLocatedNode.
 */
class SgOptions;

/** @brief This class represents a Fortran pointer assignment. It is not
 *
 * some weird compound assignment operator like C's +=.
 */
class SgPointerAssignOp;

/** @brief This class represents the concept of a C Assembler statement (untested).
 *
 * - See also:
 * Example of using a SgPragmaDeclaration object
 *
 * **Data members**
 *
 * #### `SgPragmaDeclaration::p_expr_root`
 * This pointer points to the assember expression.
 *
 * **Member functions**
 *
 * #### `SgPragmaDeclaration::SgPragmaDeclaration ( Sg_File_Info* startOfConstruct = NULL )`
 * This is the constructor.
 * This constructor builds the SgPragmaDeclaration base class.
 * - Param `startOfConstruct`: represents the position in the source code
 *
 * #### `SgPragmaDeclaration::~SgPragmaDeclaration()`
 * This is the destructor.
 * There are a lot of things to delete, but nothing is deleted in this destructor.
 *
 * #### `SgPragmaDeclaration::isSgPragmaDeclaration (SgNode *s)`
 * Cast function (from derived class to SgPragmaDeclaration pointer).
 * This functions returns a SgPragmaDeclaration pointer for any input of a
 * pointer to an object derived from a SgPragmaDeclaration.
 * - Return: Returns valid pointer to SgPragmaDeclaration if input is derived from a SgLocatedNode.
 *
 * #### `SgPragmaDeclaration::isSgPragmaDeclaration (const SgNode *s)`
 * Cast function (from derived class to SgPragmaDeclaration pointer).
 * This functions returns a SgPragmaDeclaration pointer for any input of a
 * pointer to an object derived from a SgPragmaDeclaration.
 * - Return: Returns valid pointer to SgPragmaDeclaration if input is derived from a SgLocatedNode.
 *
 * #### `SgPragmaDeclaration::get_expr() const`
 * Returns pointer to SgExpression for asm statement.
 * - Return: Returns pointer to SgExpression.
 *
 * #### `SgPragmaDeclaration::set_expr(SgExpression* expr)`
 * Access function for p_expr_root.
 * - Return: Returns void.
 */
class SgPragmaDeclaration;

/** @brief This class represents a source project, with a list of SgFile objects and global
 *
 * information about the project (commandline details, AST merge options, etc.).
 * This IR nodes contains list of files, etc. after internal command-line processing.
 * The original argc and argv are not modified and left in tact for processing by the
 * ROSE translator.  The SgProject node keeps a deep copy of the original argc and
 * argv as an STL list of strings (representing the command-line arguments).
 * Also, if ROSE and/or EDG specific command-line options are not wanted in the ROSE
 * translator, they may be stripped from argc and argv, using the member functions:
 * -# void stripRoseCommandLineOptions (int &argc, char **&argv), and\or
 * -# void stripEdgCommandLineOptions (int &argc, char **&argv).
 * These functions will modify the input argc and argv values.
 * - Internal: This IR node does not appear many times in an AST (typically only once).
 * - Todo: Make the "*PtrList" typed objects non-pointer data members (lists) instead of
 * pointer to lists.
 * - Todo: Implement a "-dumpversion" for compatibility with GNU (icc does the same).
 * - See also:
 * Example of using a SgProject object
 *
 * **Data members**
 *
 * #### `SgProject::p_fileList`
 * This is the list of files within the project.
 * - Internal: This is an STL list, I think that the get_file() member function should be
 * deprecated so that we access the list like all other STL lists.
 *
 * #### `SgProject::project_argc`
 * This is a copy to the argc value (number of command line options specified).
 * - Internal: This is not processed to group multi-argument command line options. This is a static variable.
 *
 * #### `SgProject::project_argv`
 * This is a copy to the argv (pointer) value (array of pointers to C-style strings
 * representing command line options specified).
 * - Internal: This is not processed to group multi-argument command line options. This is a static variable.
 *
 * #### `SgProject::p_verbose`
 * This is the level of verbosity assumed to apply to the whole project (all AST processing).
 * - Internal: This is a static variable, which might make it difficult to multiple SgProject
 * IR nodes to coexist within the same problem.
 *
 * #### `SgProject::p_originalCommandLineArgumentList`
 * Copy of original argc and argv command line passed to ROSE translator (converted to
 * STL list of strings).
 * - Internal: This is a deep copy.
 *
 * #### `SgProject::p_frontendErrorCode`
 * Error code returned from EDG front-end processing.
 * - Internal: Value greater than 3 is an error, else just warnings.
 *
 * #### `SgProject::p_backendErrorCode`
 * Error code returnd from processing of generated source code using vendor compiler.
 * This value should be passed back out of the main() function by the user translator (for
 * support of makefile processing).
 * - Internal: This error code is returned by the ROSE backend() function.
 *
 * #### `SgProject::p_outputFileName`
 * Filename specific using "-o" option on command line.
 * Set as part of normal command line processing within ROSE.
 * - Internal: This is set to a default value of "a.out" if "-o" is not specified.
 *
 * #### `SgProject::p_sourceFileNameList`
 * List of all source file names specified on the command line.
 * - Internal: This can be an empty list (if none are specified). This should be updated by
 * the AST Merge mechanism.
 *
 * #### `SgProject::p_objectFileNameList`
 * List of all object files specified on the command line.
 * - Internal: This can be an empty list (if none are specified).
 *
 * #### `SgProject::p_libraryFileList`
 * List of all libraries specified on command line.
 * List all libraries specified using ".a" or ".so" syntax.
 * - Internal: Required to assemble link lines for backend vendor compiler.
 *
 * #### `SgProject::p_librarySpecifierList`
 * List of libraries specified using "-lxxx" syntax.
 * - Internal: Required to assemble link lines for backend vendor compiler.
 *
 * #### `SgProject::p_libraryDirectorySpecifierList`
 * List of directories specified with "-L" option on command line
 * - Internal:
 *
 * #### `SgProject::p_includeDirectorySpecifierList`
 * List of directories specified with "-I" option on command line
 * - Internal:
 *
 * #### `SgProject::p_compileOnly`
 * This controls if we are to act as a linker (by calling the vendor compiler as a
 * linker/prelinker).
 * The value of p_compileOnly is true if "-c" appears on the command line and is false if not.
 * - Internal: This is a simple pass through to the vendor compiler to do the linking.  If no
 * transformations occured to instantiated templates, then the vendor compiler handles
 * all prelinking template instantiation as well.
 *
 * #### `SgProject::p_prelink`
 * This controls if we are to handle the prelink (not implemented).
 * - Internal: We implemented a prelink mechanism, but it was not very robust, so until we
 * have a better one this should be false.  It is use to control testing of the existing
 * prelink mechanism (which will be rewritten).
 *
 * #### `SgProject::p_template_instantiation_mode`
 * This controls the degree of template instantiation by ROSE.  No template
 * instantiation is required by ROSE if all template instantiation can be handled by the
 * backend vendor C++ compiler.
 * - Internal: The default is currently to output only transformed instantiated templates and
 * to do so as static template functions where they are instantiated template functions
 * because we don't have a sufficiently powerful global prelink mechanism to control the
 * assignment of instantiated template functions and member functions to files (to avoid
 * multiply defined symbols at link time).
 *
 * #### `SgProject::p_astMerge`
 * This controls the merging of AST when multiple files are being processed.
 *
 * #### `SgProject::p_openmp_linking`
 * This flag is used to indicate if OpenMP lowering is requested by the command line so linking to ROSE's OpenMP runtime library is needed.
 * This flag is set to be false by default.
 * "-rose:openmp:lowering" triggers this flag to be set to true.
 * SgFile::p_openmp_lowering will not be sufficient since there is no
 * SgFile in the AST when a ROSE translator is used as a linker wrapper.
 * So we have to have a project level flag to indicate the need.
 * Using a flag like SgProject::p_OpenMP_Only won't be sufficient neither
 * since OpenMP input files can be handled in three different ways in ROSE:
 * 1) parsing only 2) generating dedicated AST 3) actual OpenMP lowering
 * Only the one with lowering will need special linking support to connect to libxomp.a and pthreads.
 *
 * **Member functions**
 *
 * #### `SgProject::SgProject()`
 * This is the constructor.
 * This constructor builds the SgProject base class.
 * - See also:
 * Example:create an SgProject object
 *
 * #### `SgProject::~SgProject()`
 * This is the destructor.
 * There is nothing to delete in this object.
 *
 * #### `SgProject::isSgProject (SgNode *s)`
 * Cast function (from derived class to SgProject pointer).
 * This functions returns a SgProject pointer for any input of a
 * pointer to an object derived from a SgProject.
 * - Return: Returns valid pointer to SgProject if input is derived from a SgLocatedNode.
 *
 * #### `SgProject::isSgProject (const SgNode *s)`
 * Cast function (from derived class to SgProject pointer).
 * This functions returns a SgProject pointer for any input of a
 * pointer to an object derived from a SgProject.
 * - Return: Returns valid pointer to SgProject if input is derived from a SgLocatedNode.
 *
 * #### `list<string> SgProject::get_originalCommandLineArgumentList() const`
 * Returns a list of strings representing the original command-line.
 *
 * #### `SgProject::set_originalCommandLineArgumentList( list<string> originalCommandLineArgumentList )`
 * Sets the list of strings representing the original command-line.
 *
 * #### `bool SgProject::get_C99_only (void) const`
 * This controls the c99 mode in the frontend.
 */
class SgProject;

/** @brief This class represents the concept of a 'global' stmt in Python
 *
 * **Data members**
 *
 * #### `SgPythonGlobalStmt::p_names`
 * This is a list of SgInitializedNames that are imported into the inner scope.
 *
 * **Member functions**
 *
 * #### `SgPythonGlobalStmt::~SgPythonGlobalStmt()`
 * This is the destructor.
 *
 * #### `SgPythonGlobalStmt::isSgPythonGlobalStmt (SgNode *s)`
 * Cast function (from derived class to SgPythonGlobalStmt pointer).
 * This functions returns a SgPythonGlobalStmt pointer for any input of a
 * pointer to an object derived from a SgPythonGlobalStmt.
 * - Return: Returns valid pointer to SgPythonGlobalStmt if input is derived from a SgLocatedNode.
 *
 * #### `SgPythonGlobalStmt::isSgPythonGlobalStmt (const SgNode *s)`
 * Cast function (from derived class to SgPythonGlobalStmt pointer).
 * This functions returns a SgPythonGlobalStmt pointer for any input of a
 * pointer to an object derived from a SgPythonGlobalStmt.
 * - Return: Returns valid pointer to SgPythonGlobalStmt if input is derived from a SgLocatedNode.
 *
 * #### `SgPythonGlobalStmt::copy(const SgCopyHelp & help)`
 * Makes a copy (deap of shallow depending on SgCopyHelp).
 * - Return: Returns pointer to copy of SgPythonGlobalStmt.
 *
 * #### `SgPythonGlobalStmt::append_name(SgIntializedName* name)`
 * Append a name to the list of identifiers imported into the inner scope.
 *
 * #### `SgPythonGlobalStmt::prepend_name(SgIntializedName* name)`
 * Prepend a name to the list of identifiers imported into the inner scope.
 */
class SgPythonGlobalStmt;

/** @brief This class represents a OLD concept of the structure require
 *
 * for qualified names when they were in the AST, they are not used now
 * (name qualification was reimplemented in 2011 as a step before the
 * unparser and is no longer a part of the structure in the AST).
 */
class SgQualifiedName;

/** @brief This class represents a OLD concept of the structure require
 *
 * for qualified names when they were in the AST, they are not used now
 * (name qualification was reimplemented in 2011 as a step before the
 * unparser and is no longer a part of the structure in the AST).
 */
class SgQualifiedNameType;

/** @brief This class represents the "&" operator (applied to any lvalue).
 *
 * - Todo: Double check this description.
 * - Internal:
 * - See also:
 * Example of using a SgRefExp object
 *
 * **Data members**
 *
 * #### `SgRefExp::p_type_name`
 * This a SgType, but I forget its significance.
 *
 * **Member functions**
 *
 * #### `SgRefExp::SgRefExp ( Sg_File_Info* startOfConstruct = NULL )`
 * This is the constructor.
 * This constructor builds the SgRefExp base class.
 * - Param `startOfConstruct`: represents the position in the source code
 * - See also:
 * Example:create an SgRefExp object
 *
 * #### `SgRefExp::~SgRefExp()`
 * This is the destructor.
 * Only the Sg_File_Info object can be deleted in this object.
 *
 * #### `SgRefExp::isSgRefExp (SgNode *s)`
 * Cast function (from derived class to SgRefExp pointer).
 * This functions returns a SgRefExp pointer for any input of a
 * pointer to an object derived from a SgRefExp.
 * - Return: Returns valid pointer to SgRefExp if input is derived from a SgRefExp.
 *
 * #### `SgRefExp::isSgRefExp (const SgNode *s)`
 * Cast function (from derived class to SgRefExp pointer).
 * This functions returns a SgRefExp pointer for any input of a
 * pointer to an object derived from a SgRefExp.
 * - Return: Returns valid pointer to SgRefExp if input is derived from a SgRefExp.
 */
class SgRefExp;

/** @brief This class represents the concept of a C Assembler statement (untested).
 *
 * - Todo: Check that when return (SgReturnStatement) is implicit, it should be marked as
 * compiler generated.  Actually we only include returns in the AST which are explicit, so
 * this is not a problem.
 * - See also:
 * Example of using a SgReturnStmt object
 *
 * **Data members**
 *
 * #### `SgReturnStmt::p_expression_root`
 * This pointer points to the SgExpressionRoot of an expression tree.
 *
 * **Member functions**
 *
 * #### `SgReturnStmt::SgReturnStmt ( Sg_File_Info* startOfConstruct = NULL, SgExpression *return_expr = NULL )`
 * This is the constructor.
 * This constructor builds the SgReturnStmt base class.
 * - Param `startOfConstruct`: represents the position in the source code
 * - See also:
 * Example:create an SgReturnStmt object
 *
 * #### `SgReturnStmt::SgReturnStmt ( Sg_File_Info* startOfConstruct = NULL, SgExpressionRoot *return_expr = NULL )`
 * This is the constructor.
 * This constructor builds the SgReturnStmt base class.
 * - Deprecated: This constructor is likely to be deprecated.
 *
 * #### `SgReturnStmt::~SgReturnStmt()`
 * This is the destructor.
 * There are a lot of things to delete, but nothing is deleted in this destructor.
 *
 * #### `SgReturnStmt::isSgReturnStmt (SgNode *s)`
 * Cast function (from derived class to SgReturnStmt pointer).
 * This functions returns a SgReturnStmt pointer for any input of a
 * pointer to an object derived from a SgReturnStmt.
 * - Return: Returns valid pointer to SgReturnStmt if input is derived from a SgLocatedNode.
 *
 * #### `SgReturnStmt::isSgReturnStmt (const SgNode *s)`
 * Cast function (from derived class to SgReturnStmt pointer).
 * This functions returns a SgReturnStmt pointer for any input of a
 * pointer to an object derived from a SgReturnStmt.
 * - Return: Returns valid pointer to SgReturnStmt if input is derived from a SgLocatedNode.
 *
 * #### `SgReturnStmt::get_expression_root() const`
 * Returns pointer to SgExpressionRoot (which wraps the SgExpresion).
 * - Return: Returns pointer to SgExpressionRoot.
 *
 * #### `SgReturnStmt::set_expression_root(SgExpressionRoot* expr)`
 * Access function for p_expression_root.
 * - Return: Returns void.
 *
 * #### `SgReturnStmt::get_return_expr() const`
 * Returns pointer to SgExpression (which is wrapped by the p_expression_root).
 * - Return: Returns pointer to SgExpression.
 *
 * #### `SgReturnStmt::set_return_expr(SgExpression* expression_root)`
 * Access function for SgExpression hidden behind p_expression_root.
 * - Return: Returns void.
 */
class SgReturnStmt;

/** @brief This class was part of CC++ support from a long time ago.
 *
 * - Deprecated: This IR node is not longer used!
 */
class SgScopeOp;

/** @brief This class represents the concept of a scope in C++ (e.g. global scope, fuction scope, etc.).
 *
 * Scopes are an important aspect of language design. They allow
 * declarations to have a local context and so promote good programming style.
 * Scope statments in C++ include a number of different kinds of statements;
 * the SgScopeStatement is a base class for these. Each scope statement contains
 * a symbol table and the SgScopeStatements role is mostly to provide this
 * symbol table and an interface to accessing it.
 * - Internal: This is a base class for scope statements.
 * - Todo: Where current derived IR nodes from SgScopeStatement contain SgBasicBlock objects,
 * we should consider having them contain SgStatement objects instead of SgBasicBlock.
 * This would avoid current normalizations which are cute, but perhaps unwelcome.  We
 * need to discuss and rule on this detail.
 * An alternative would be to let there be a SgBasicBlock and mark it as
 * compiler-generated if it does not appear in the original source code.  then the
 * unparser (code generator) could be made to avoid output of the SgBasicblock (and
 * enforce that the number of statements in the block is not greater than one).
 * Effected IR nodes include:
 * - SgCatchOptionStmt (should contain SgStatement)
 * - SgDoWhileStmt (should contain SgStatement)
 * - SgForStatement (should contain SgStatement)
 * - SgIfStmt (should contain true and false SgStatement)
 * - SgSwitchStatement (should contain SgStatement)
 * - SgWhileStmt (should contain SgStatement) \n
 * these are currently correct:
 * - SgFunctionDefinition (should contain a SgBasicBlock)
 * - SgGlobal (constains a list of declarations (SgDeclarationStatement objects))
 * - SgNamespaceDefinitionStatement (constains a list of declarations (SgDeclarationStatement objects))
 * - SgClassDefinition (constains a list of declarations (SgDeclarationStatement objects))
 * - Note: Note about "conditions" used in loop constructs. "Conditionals" are either:
 * - an expression, or
 * - a declaration with initializer \n
 * Note also that a SgConditional is different (a ternary operator (<test expr>) ? <true part> : <false part>;)
 * There are a few errors in ROSE, locations in the IR where statements are used instead of expressions and
 * expression used instead of statements. (reference Appendix A.6)
 * - Note: The following should have the "conditional" concept:
 * - (SgIfStmt) if ( condition ) statement     // Currently correct
 * - (SgSwitchStatement) switch ( condition ) statement // Currently uses SgExpression (wrong)
 * - (SgWhileStmt) while ( condition ) statement  // Currently correct
 * - (SgForStatement) for ( for-init-statment; condition; expression ) statement  // Currently uses SgExpression (wrong)
 * - Note: The following should have an expression:
 * - (SgDoWhileStmt) do statement while ( expression );  // currently uses SgStatement instead of SgExpression
 *
 * **Data members**
 *
 * #### `SgScopeStatement::p_symbol_table`
 * This pointer is always valid and stores the symbl table.
 * - Internal: The symbol table implementation was changed in fall of 2004, by Alin.
 * It is not simple and leverages STL and provides more features.
 *
 * **Member functions**
 *
 * #### `SgScopeStatement::SgScopeStatement ( Sg_File_Info* startOfConstruct = NULL )`
 * This is the constructor.
 * This constructor builds the SgScopeStatement base class.
 * - Param `startOfConstruct`: represents the position in the source code
 *
 * #### `SgScopeStatement::~SgScopeStatement()`
 * This is the destructor.
 * There are a lot of things to delete, but nothing is deleted in this destructor.
 *
 * #### `SgScopeStatement::isSgScopeStatement (SgNode *s)`
 * Cast function (from derived class to SgScopeStatement pointer).
 * This functions returns a SgScopeStatement pointer for any input of a
 * pointer to an object derived from a SgScopeStatement.
 * - Return: Returns valid pointer to SgScopeStatement if input is derived from a SgLocatedNode.
 *
 * #### `SgScopeStatement::isSgScopeStatement (const SgNode *s)`
 * Cast function (from derived class to SgScopeStatement pointer).
 * This functions returns a SgScopeStatement pointer for any input of a
 * pointer to an object derived from a SgScopeStatement.
 * - Return: Returns valid pointer to SgScopeStatement if input is derived from a SgLocatedNode.
 *
 * #### `SgScopeStatement::get_symbol_table()`
 * Returns a pointer to the locally strored SgSymbolTable.
 * - Return: Returns a pointer.
 *
 * #### `SgScopeStatement::set_symbol_table(SgSymbolTable *symbolTable)`
 * Sets the pointer to the locally strored SgSymbolTable.
 * - Return: Returns void.
 *
 * #### `SgScopeStatement::get_qualified_name() const`
 * Returns SgName (a string) representing the name of the current scope.
 * See discussion of mangled names in the documentation.
 * - Return: Returns SgName (a string).
 *
 * #### `SgScopeStatement::copy(const SgCopyHelp & help)`
 * Makes a copy (deap of shallow depending on SgCopyHelp).
 * - Return: Returns pointer to copy of SgScopeStatement.
 *
 * #### `SgScopeStatement::insert_symbol(const SgName & name, SgSymbol *symbol)`
 * Puts a SgSymbol object into the local symbol table.
 * - Return: Returns void.
 *
 * #### `SgScopeStatement::lookup_symbol(const SgName & name, SgSymbol *symbol)`
 * returns boolean value if symbol exists.
 * - Return: Returns bool.
 * - Internal: Not clear if this is used!
 *
 * #### `SgScopeStatement::lookup_symbol(const SgName & name)`
 * returns boolean value if symbol exists.
 * - Return: Returns SgSymbol pointer.
 *
 * #### `SgScopeStatement::containsOnlyDeclarations()`
 * boolean member function to distinquish if getStatementList() or getDeclarationList() is
 * appropriate for the SgScopeStatment IR node.
 * - Return: bool.
 *
 * #### `SgScopeStatement::getDeclarationList()`
 * Gets reference to internal STL list of pointers to SgDeclarationStatement objects
 * (only defined for scopes containing declarations, see bool containsOnlyDeclarations()).
 * This function is useful for SgScopeStatement objects that contain declarations, and is
 * not defined (returns an error) if called on a SgClassDefinition,
 * SgTemplateInstantiationDefn, SgGlobal, SgNamespaceDefinitionStatement.
 * To test if a scope contains only declarations use "bool containsOnlyDeclarations()".
 * Because this function returns a reference to the list (instead of a list of pointers
 * to the declarations or statements) any modification of the list is a modification of
 * the AST.
 * - Return: STL list of pointers to SgDeclarationStatement objects.
 *
 * #### `SgScopeStatement::getStatementList()`
 * Gets reference to internal STL list of pointers to SgStatement objects
 * (only defined for scopes containing declarations, see bool containsOnlyDeclarations()).
 * This function is useful for SgScopeStatement objects that contain non-declarations
 * (lists of SgStatement instead of lists of SgDeclarationStatement IR nodes).
 * Because this function returns a reference to the list (instead of a list of pointers
 * to the declarations or statements) any modification of the list is a modification of
 * the AST.
 * - Return: STL list of pointers to SgStatement objects.
 *
 * #### `SgScopeStatement::generateStatementList()`
 * Builds list of pointers to SgStatement objects (useful for all SgScopeStatement objects,
 * independent of whether they contain lists of SgDeclarationStatements or lists of SgStatements).
 * This function is useful for any SgScopeStatement object, but since it returns
 * by value any modification of the list is not reflected in the AST.
 * - Todo: This runtion should return a const value so that it would be more clear that
 * it could not be modified (since such modifications would not be reflected in the AST).
 * - Return: STL list of pointers to SgStatement objects (returns by value).
 */
class SgScopeStatement;

/** @brief This class represents the "sizeof()" operator (applied to any type).
 *
 * - Todo: Need to implement support in EDG/SageIII translation for this operator.
 * - Internal: This operator is only seen in the AST within expression trees
 * which came from constant folded expression.
 * - See also:
 * Example of using a SgSizeOfOp object
 *
 * **Data members**
 *
 * #### `SgSizeOfOp::p_operand_expr`
 * This a pointer to the expression given to the sizeof operator.
 * This is the expression provided as an argument to the sizeof operator
 * (if it is an expression).
 *
 * #### `SgSizeOfOp::p_operand_type`
 * This a pointer to the SgType given to the sizeof operator.
 * This is the type provided as an argument to the sizeof operator
 * (if it is a type).
 *
 * #### `SgSizeOfOp::p_expression_type`
 * This a SgType, the type of the expression.
 * - Internal: Not clear if this is always a valid pointer.
 *
 * **Member functions**
 *
 * #### `SgSizeOfOp::SgSizeOfOp ( Sg_File_Info* startOfConstruct = NULL )`
 * This is the constructor.
 * This constructor builds the SgSizeOfOp base class.
 * - Param `startOfConstruct`: represents the position in the source code
 * - See also:
 * Example:create an SgSizeOfOp object
 *
 * #### `SgSizeOfOp::~SgSizeOfOp()`
 * This is the destructor.
 * Only the Sg_File_Info object can be deleted in this object.
 *
 * #### `SgSizeOfOp::isSgSizeOfOp (SgNode *s)`
 * Cast function (from derived class to SgSizeOfOp pointer).
 * This functions returns a SgSizeOfOp pointer for any input of a
 * pointer to an object derived from a SgSizeOfOp.
 * - Return: Returns valid pointer to SgSizeOfOp if input is derived from a SgSizeOfOp.
 *
 * #### `SgSizeOfOp::isSgSizeOfOp (const SgNode *s)`
 * Cast function (from derived class to SgSizeOfOp pointer).
 * This functions returns a SgSizeOfOp pointer for any input of a
 * pointer to an object derived from a SgSizeOfOp.
 * - Return: Returns valid pointer to SgSizeOfOp if input is derived from a SgSizeOfOp.
 */
class SgSizeOfOp;

/** @brief This class is part of the older CC++ concept. It is not a part of C or C++ (this IR
 *
 * node is not used and will be removed in a future release).
 * - Deprecated: This class is not used and will be removed in a future release.
 * - See also:
 * Example of using a SgSpawnStmt object
 *
 * **Data members**
 *
 * #### `SgSpawnStmt::p_the_func_root`
 * This pointer points to a SgExpressionRoot.
 *
 * **Member functions**
 *
 * #### `SgSpawnStmt::SgSpawnStmt ( Sg_File_Info* startOfConstruct = NULL )`
 * This is the constructor.
 * This constructor builds the SgSpawnStmt base class.
 * - Param `startOfConstruct`: represents the position in the source code
 *
 * #### `SgSpawnStmt::~SgSpawnStmt()`
 * This is the destructor.
 * There are a lot of things to delete, but nothing is deleted in this destructor.
 *
 * #### `SgSpawnStmt::isSgSpawnStmt (SgNode *s)`
 * Cast function (from derived class to SgSpawnStmt pointer).
 * This functions returns a SgSpawnStmt pointer for any input of a
 * pointer to an object derived from a SgSpawnStmt.
 * - Return: Returns valid pointer to SgSpawnStmt if input is derived from a SgLocatedNode.
 *
 * #### `SgSpawnStmt::isSgSpawnStmt (const SgNode *s)`
 * Cast function (from derived class to SgSpawnStmt pointer).
 * This functions returns a SgSpawnStmt pointer for any input of a
 * pointer to an object derived from a SgSpawnStmt.
 * - Return: Returns valid pointer to SgSpawnStmt if input is derived from a SgLocatedNode.
 */
class SgSpawnStmt;

/** @brief This class represents the notion of a statement.
 *
 * There are no uses of this IR node anywhere.  All statements
 * are derived from this SgStatement class.
 * - Internal: This is a base class for all statements.
 * - Todo: There are a number of statements that contain a SgBasicBlock
 * where they should contain a SgStatement.  In each case changing
 * the data member to be a SgStatement will unfortunately change
 * the constructor parameter list and thus the ROSE API.  So these
 * changes have to be organized a a point where it is clear we will
 * be changing some details of the ROSE API (prior to external release).
 * Problem IR nodes are:
 * - SgCaseOptionStmt
 * - SgTryStmt
 * - SgDefaultOptionStmt
 * - SgIfStmt
 * - SgForStatement
 * - SgWhileStmt
 * - SgDoWhileStmt
 * - SgSwitchStatement
 * - Todo: Fortran support for modifiers can be used as statement (must be added to IR):
 * see section 5.2, 5.3 in Fortran 2003 standard. Note that type modifiers can
 * be used as statements.
 * - Todo: Fortran support requires statements in section 6.3.
 * - Todo: Fortran support requires for "where" and "forall" statements.
 * - Todo: Fortran support requires for case statement ranges (gnu extension for C, but
 * standard in Fortran).
 * - Todo: Fortran support requires statements in section 8.1.4, 8.1.5.
 * - Todo: Fortran support requires statements in section 15 (modifiers for ISO_C_BINDING).
 * - See also:
 * Example of using a SgStatement object
 *
 * **Member functions**
 *
 * #### `SgStatement::SgStatement ( Sg_File_Info* startOfConstruct = NULL )`
 * This is the constructor.
 * This constructor builds the SgStatement base class.
 * - Param `startOfConstruct`: represents the position in the source code
 * - See also:
 * Example:create an SgStatement object
 *
 * #### `SgStatement::~SgStatement()`
 * This is the destructor.
 * There is nothing to delete in this object.
 *
 * #### `SgStatement::isSgStatement (SgNode *s)`
 * Cast function (from derived class to SgStatement pointer).
 * This functions returns a SgStatement pointer for any input of a
 * pointer to an object derived from a SgStatement.
 * - Return: Returns valid pointer to SgStatement if input is derived from a SgLocatedNode.
 *
 * #### `SgStatement::isSgStatement (const SgNode *s)`
 * Cast function (from derived class to SgStatement pointer).
 * This functions returns a SgStatement pointer for any input of a
 * pointer to an object derived from a SgStatement.
 * - Return: Returns valid pointer to SgStatement if input is derived from a SgLocatedNode.
 *
 * #### `SgStatement::get_scope() const`
 * Returns scope of current statement.
 * This functions returns a pointer to the inner most SgScopeStatement where the current
 * statement is located.
 * This is a function called after the parents have been set.  It is not called by the
 * EDG/SAGE interface and is only called after the SAGE AST has been built and the parent
 * pointers set within a separate phase.
 * Improtant exception: The SgFunctionParameterList should have as it's scope the
 * SgFunctionDefinition, but it is a sibling of the SgFunctionDefinition (both the
 * SgFunctionParameterList and the SgFunctionDefinition have the SgFunctionDeclaration
 * as a parent.  Thus the scope of the SgFunctionParameterList is computed to be the
 * SgGlobal (global scope) most often; which is an error).  So we handle this case
 * explicitly.
 * - Return: Returns valid pointer to SgScopeStatement.
 *
 * #### `SgStatement::setExtern()`
 * This function sets "extern" status for the current statement.
 * The new modifier mechanism makes this older function interface redundent.
 * - Deprecated: This function should not be called anywhere the new modifier handling make is obsolete.
 *
 * #### `SgStatement::setStatic()`
 * This function sets "static" status for the current statement.
 * The new modifier mechanism makes this older function interface redundent.
 * - Deprecated: This function should not be called anywhere the new modifier handling make is obsolete.
 *
 * #### `SgStatement::replace_child (SgStatement *, SgStatement *, bool extractListFromBasicBlock=false)`
 * Private supporting function for low level SageIII rewrite interface.
 * This function implements special semantics for the insertion of a collection of
 * statements in a block.  If extractListFromBasicBlock is true then the statements in
 * the block are extracts and placed into the same scope as the target position.
 * - Return: Returns bool.
 *
 * #### `SgStatement::insert_child (SgStatement *, SgStatement *, bool inFront=true, bool extractListFromBasicBlock=false)`
 * Private supporting function for low level SageIII rewrite interface.
 * This function implements special semantics for the insertion of a collection of
 * statements in a block.  If extractListFromBasicBlock is true then the statements in
 * the block are extracts and placed into the same scope as the target position.
 * - Return: Returns bool.
 *
 * #### `SgStatement::get_symbol_from_symbol_table()`
 * Virtual function to obtain the SgSymbol from the scope's symbol table.
 * This function permits retrival of the associated SgSymbol, where it exists
 * for that IR node, where is is associated with this exact declaration. If no
 * symbol is found in the symbol table the function returns NULL. If it is not
 * implemented for the associated statement or declaration it is an error
 * (assertion failure).   This fucntion is ONLY defined for those IR nodes for
 * which symbols can be constructed.
 * - Return: Returns SgSymbol pointer.
 *
 * #### `SgStatement::isChildUsedAsLValue(SgExpression *)`
 * Virtual function to determine if the provided expression is used as an l-value within the cotext of this statement.
 * - Return: True if the expression is used within this statement as an l-value.
 *
 * **Additional notes**
 * Low-level Support for mutation of the AST.
 * This is the lowest level of support for the mutation of the AST.
 * - Internal: Need to document this section more fully.
 */
class SgStatement;

/** @brief This class represents the GNU extension "statement expression" (thus is
 *
 * non-standard C and C++).
 * The statement expression is a GNU extension that is helpful in the implementation of
 * macros and expressions.  In most cases an alternative construction of code is possible.
 * More information is at: \n
 * http://developer.apple.com/documentation/developertools/gcc-4.0.1/gcc/Statement-Exprs.html \n
 * In g++, the result value of a statement expression undergoes array and function pointer
 * decay, and is returned by value to the enclosing expression. For instance, if A is a
 * class, then: \n
 * A a; ({a;}).Foo() \n
 * will construct a temporary A object to hold the result of the statement expression, and
 * that will be used to invoke Foo. Therefore the this pointer observed by Foo will not be
 * the address of a.
 * - Internal: This sort of expression can be useful and is important to at least one user so
 * it is implemented in ROSE.
 * - See also:
 * Example of using a SgStatementExpression object
 *
 * **Data members**
 *
 * #### `SgStatementExpression::p_statement`
 * This a pointer to the SgStatement (usually a SgBasicBlock).
 * This is the expression provided as an argument to the sizeof operator
 * (if it is an expression).
 *
 * #### `SgStatementExpression::p_operand_type`
 * This a pointer to the SgType given to the sizeof operator.
 * This is the type provided as an argument to the sizeof operator
 * (if it is a type).
 *
 * #### `SgStatementExpression::p_expression_type`
 * This a SgType, the type of the expression.
 * - Internal: Not clear if this is always a valid pointer.
 *
 * **Member functions**
 *
 * #### `SgStatementExpression::SgStatementExpression ( Sg_File_Info* startOfConstruct = NULL )`
 * This is the constructor.
 * This constructor builds the SgStatementExpression base class.
 * - Param `startOfConstruct`: represents the position in the source code
 * - See also:
 * Example:create an SgStatementExpression object
 *
 * #### `SgStatementExpression::~SgStatementExpression()`
 * This is the destructor.
 * Only the Sg_File_Info object can be deleted in this object.
 *
 * #### `SgStatementExpression::isSgStatementExpression (SgNode *s)`
 * Cast function (from derived class to SgStatementExpression pointer).
 * This functions returns a SgStatementExpression pointer for any input of a
 * pointer to an object derived from a SgStatementExpression.
 * - Return: Returns valid pointer to SgStatementExpression if input is derived from a SgStatementExpression.
 *
 * #### `SgStatementExpression::isSgStatementExpression (const SgNode *s)`
 * Cast function (from derived class to SgStatementExpression pointer).
 * This functions returns a SgStatementExpression pointer for any input of a
 * pointer to an object derived from a SgStatementExpression.
 * - Return: Returns valid pointer to SgStatementExpression if input is derived from a
 * SgStatementExpression.
 */
class SgStatementExpression;

/** @brief This class is intended to be a wrapper around SgStatements, allowing
 *
 * them to exist in scopes that only allow SgDeclarationStatements.
 * Certain languages, such as Python, allow arbitrary statements and expressions
 * in global scope and class definition scopes. However, SageIII requires that all
 * statements in global and class scopes be SgDeclarationStatements (as C/C++ requires).
 * Given these limitations, SgStmtDeclarationStatement represents a compromise between
 * the existing IR implementation and the desire to support a variety of languages.
 * Currently, this node is only intended for use with Python.
 *
 * **Data members**
 *
 * #### `SgStmtDeclarationStatement::p_statement`
 * Holds the wrapped statement.
 *
 * **Member functions**
 *
 * #### `SgStmtDeclarationStatement::SgStmtDeclarationStatement ( SgStatement * stmt )`
 * This is the constructor.
 * This constructor builds the SgStmtDeclarationStatement base class.
 * - Param `stmt`: The statement wrapped by this SgStmtDeclarationStatement.
 *
 * #### `SgStmtDeclarationStatement::~SgStmtDeclarationStatement()`
 * This is the destructor.
 *
 * #### `SgStmtDeclarationStatement::isSgStmtDeclarationStatement (SgNode *s)`
 * Cast function (from derived class to SgStmtDeclarationStatement pointer).
 * This functions returns a SgStmtDeclarationStatement pointer for any input of a
 * pointer to an object derived from a SgStmtDeclarationStatement.
 * - Return: Returns valid pointer to SgStmtDeclarationStatement if input is derived from a SgStmtDeclarationStatement.
 *
 * #### `SgStmtDeclarationStatement::isSgStmtDeclarationStatement (const SgNode *s)`
 * Cast function (from derived class to SgStmtDeclarationStatement pointer).
 * This functions returns a SgStmtDeclarationStatement pointer for any input of a
 * pointer to an object derived from a SgStmtDeclarationStatement.
 * - Return: Returns valid pointer to SgStmtDeclarationStatement if input is derived from a SgStmtDeclarationStatement.
 */
class SgStmtDeclarationStatement;

/** @brief This class represents modifiers specific to storage.
 *
 * This modifier is set to only one of a collection of values. To
 * unset any value (e.g. "extern") just call "setDefault()" to restore
 * the default value.
 * - Note: For more detail see the C++ grammar specification in Bjarne's book, Appendix B.
 * - See also:
 * Example of using a SgStorageModifier object
 *
 * **Data members**
 *
 * #### `SgStorageModifier::storage_modifier_enum p_modifier`
 * enum value (can be set to only one of several values)
 *
 * **Member functions**
 *
 * #### `SgStorageModifier::SgStorageModifier()`
 * This is the constructor.
 * This constructor builds the SgStorageModifier base class.
 * - See also:
 * Example:create an SgStorageModifier object
 *
 * #### `SgStorageModifier::~SgStorageModifier()`
 * This is the destructor.
 * There is nothing to delete in this object.
 *
 * #### `SgStorageModifier::operator=(const SgStorageModifier & X)`
 * This is the assignment operator.
 * This is a simple assignment of the SgBitVector from X to the current object.
 *
 * #### `SgStorageModifier::isSgStorageModifier (SgNode *s)`
 * Cast function (from derived class to SgStorageModifier pointer).
 * This functions returns a SgStorageModifier pointer for any input of a
 * pointer to an object derived from a SgStorageModifier.
 * - Return: Returns valid pointer to SgStorageModifier if input is derived from a SgLocatedNode.
 *
 * #### `SgStorageModifier::isSgStorageModifier (const SgNode *s)`
 * Cast function (from derived class to SgStorageModifier pointer).
 * This functions returns a SgStorageModifier pointer for any input of a
 * pointer to an object derived from a SgStorageModifier.
 * - Return: Returns valid pointer to SgStorageModifier if input is derived from a SgLocatedNode.
 *
 * #### `SgStorageModifier::isUnknown () const`
 * Storage modifier is unknown (not set).
 * - Return: Returns bool.
 *
 * #### `SgStorageModifier::setUnknown ()`
 * Set storage.
 * - Return: Returns bool.
 *
 * #### `SgStorageModifier::isDefault () const`
 * Storage modifier is default (default value).
 * - Return: Returns bool.
 *
 * #### `SgStorageModifier::setDefault ()`
 * Set storage.
 * - Return: Returns bool.
 *
 * #### `SgStorageModifier::isExtern () const`
 * Storage modifier is extern (not the same as extern "C").
 * - Return: Returns bool.
 *
 * #### `SgStorageModifier::setExtern ()`
 * Set storage.
 * - Return: Returns bool.
 *
 * #### `SgStorageModifier::isStatic () const`
 * Storage modifier is static (always false for the SgStorageModifier in the SgInitializedName).
 * - Internal: Note that the isStatic() for variable declarations is stored in the
 * SgStorageModifier of theSgDeclarationModifier in the SgVariableDeclaration,
 * and not in the SgStorageModifier stored in the SgInitializedName.  This is
 * a result of the structure of code, where the static keyword can not be used
 * to selectively apply to individual variables in a list of names in a variable
 * declaration.
 * - Return: Returns bool.
 *
 * #### `SgStorageModifier::setStatic ()`
 * Set storage.
 * - Return: Returns bool.
 *
 * #### `SgStorageModifier::isAuto () const`
 * Storage modifier is auto (most common setting for variables).
 * - Return: Returns bool.
 *
 * #### `SgStorageModifier::setAuto ()`
 * Set storage.
 * - Return: Returns bool.
 *
 * #### `SgStorageModifier::isUnspecified () const`
 * Storage modifier is not specified (I think this is not used).
 * - Return: Returns bool.
 *
 * #### `SgStorageModifier::setUnspecified ()`
 * Set storage.
 * - Return: Returns bool.
 *
 * #### `SgStorageModifier::isRegister () const`
 * Storage modifier is register.
 * - Return: Returns bool.
 *
 * #### `SgStorageModifier::setRegister ()`
 * Set storage.
 * - Return: Returns bool.
 *
 * #### `SgStorageModifier::isMutable () const`
 * Storage modifier is mutable (C++ only).
 * - Return: Returns bool.
 *
 * #### `SgStorageModifier::setMutable ()`
 * Set storage.
 * - Return: Returns bool.
 *
 * #### `SgStorageModifier::isTypedef () const`
 * Storage modifier is a typedef.
 * - Return: Returns bool.
 *
 * #### `SgStorageModifier::setTypedef ()`
 * Set storage.
 * - Return: Returns bool.
 *
 * #### `SgStorageModifier::isAsm () const`
 * Storage modifier is an assembler statement.
 * - Return: Returns bool.
 *
 * #### `SgStorageModifier::setAsm ()`
 * Set storage.
 * - Return: Returns bool.
 */
class SgStorageModifier;

/** @brief This class represents the conversion of an arbitrary expression to
 *
 * a string. This node is intended for use with Python.
 * This node should not be confused with the cast operation found in other
 * languages. See SgCastExp for more info.
 *
 * **Data members**
 *
 * #### `SgStringConversion::p_expression`
 * The expression to be converted to a string.
 *
 * **Member functions**
 *
 * #### `SgStringConversion::SgStringConversion ( SgExpression* exp )`
 * This is the constructor.
 * This constructor builds the SgStringConversion base class.
 * - Param `exp`: The expression to be converted to a string.
 *
 * #### `SgStringConversion::~SgStringConversion()`
 * This is the destructor.
 *
 * #### `SgStringConversion::isSgStringConversion (SgNode *s)`
 * Cast function (from derived class to SgStringConversion pointer).
 * This functions returns a SgStringConversion pointer for any input of a
 * pointer to an object derived from a SgStringConversion.
 * - Return: Returns valid pointer to SgStringConversion if input is derived from a SgStringConversion.
 *
 * #### `SgStringConversion::isSgStringConversion (const SgNode *s)`
 * Cast function (from derived class to SgStringConversion pointer).
 * This functions returns a SgStringConversion pointer for any input of a
 * pointer to an object derived from a SgStringConversion.
 * - Return: Returns valid pointer to SgStringConversion if input is derived from a SgStringConversion.
 */
class SgStringConversion;

/** @brief This class represents the base class of a numbr of IR nodes that don't otherwise fit
 *
 * into the existing hierarchy of statement, expression, types, and symbols.
 * There are no uses of this IR node anywhere.  All SgSupport based IR nodes
 * are derived from this SgSupport class.
 * - Internal: This is a base class for all SgSupport objects.
 * - See also:
 * Example of using a SgSupport object
 *
 * **Member functions**
 *
 * #### `SgSupport::SgSupport()`
 * This is the constructor.
 * This constructor builds the SgSupport base class.
 * - See also:
 * Example:create an SgSupport object
 *
 * #### `SgSupport::~SgSupport()`
 * This is the destructor.
 * There is nothing to delete in this object.
 *
 * #### `SgSupport::isSgSupport (SgNode *s)`
 * Cast function (from derived class to SgSupport pointer).
 * This functions returns a SgSupport pointer for any input of a
 * pointer to an object derived from a SgSupport.
 * - Return: Returns valid pointer to SgSupport if input is derived from a SgLocatedNode.
 *
 * #### `SgSupport::isSgSupport (const SgNode *s)`
 * Cast function (from derived class to SgSupport pointer).
 * This functions returns a SgSupport pointer for any input of a
 * pointer to an object derived from a SgSupport.
 * - Return: Returns valid pointer to SgSupport if input is derived from a SgLocatedNode.
 */
class SgSupport;

/** @brief This class represents the concept of a switch.
 *
 * - Internal: Note that the design or some aspect of the design may be the cause of falure to
 * handle the Duff's Device example code. I think this is not fixed, Oct 2005.
 * - Todo: FIXED: The conditional test should be a SgStatement so that a declaration can
 * be used, it is currently an SgExpression (specifically a SgExpressionRoot).
 * - Todo: The body of the SgSwitch should really be a SgStatement not a SgBasicBlock.
 * DuffsDevice can be modified to should an example of this but there are also
 * much more trivial examples.  See comment about this in the SgStatement todo list.
 *
 * **Data members**
 *
 * #### `SgSwitchStatement::p_item_selector_root`
 * This pointer a SgExpressionRoot.
 * - Note: I think this is a poor name for this variable.
 * - Todo: This should be changed to be a SgStatement (to follow the C++ standard).
 * this way it could be an expression (via an expression statement) or a variable
 * declaration with initializer (via a SgVariableDeclaration).
 * We also have the condition specified before the body within the ROSETTA
 * specification and this causes the traversal to travers the condition and body in the
 * wrong order (for do-while, the traversal should be body forst and condition second).
 * See test2005_114.C for more details and example code (example of strange loops).
 *
 * #### `SgSwitchStatement::p_body`
 * This pointer a SgBasicBlock, and holds the cases in the body of the switch.
 *
 * **Member functions**
 *
 * #### `SgSwitchStatement::SgSwitchStatement ( Sg_File_Info* startOfConstruct = NULL )`
 * This is the constructor.
 * This constructor builds the SgSwitchStatement base class.
 * - Param `startOfConstruct`: represents the position in the source code
 *
 * #### `SgSwitchStatement::~SgSwitchStatement()`
 * This is the destructor.
 * There are a lot of things to delete, but nothing is deleted in this destructor.
 *
 * #### `SgSwitchStatement::isSgSwitchStatement (SgNode *s)`
 * Cast function (from derived class to SgSwitchStatement pointer).
 * This functions returns a SgSwitchStatement pointer for any input of a
 * pointer to an object derived from a SgSwitchStatement.
 * - Return: Returns valid pointer to SgSwitchStatement if input is derived from a SgLocatedNode.
 *
 * #### `SgSwitchStatement::isSgSwitchStatement (const SgNode *s)`
 * Cast function (from derived class to SgSwitchStatement pointer).
 * This functions returns a SgSwitchStatement pointer for any input of a
 * pointer to an object derived from a SgSwitchStatement.
 * - Return: Returns valid pointer to SgSwitchStatement if input is derived from a SgLocatedNode.
 *
 * #### `SgSwitchStatement::copy(const SgCopyHelp & help)`
 * Makes a copy (deap of shallow depending on SgCopyHelp).
 * - Return: Returns pointer to copy of SgSwitchStatement.
 *
 * #### `SgSwitchStatement::get_item_selector() const`
 * Access function for p_item_selector_root.
 * - Return: Returns a pointer to a SgExpression.
 *
 * #### `SgSwitchStatement::set_item_selector(SgExpression* item_selector)`
 * Access function for p_item_selector.
 * - Param `for_init_stmt`: SgExpression pointer
 * - Return: Returns void.
 *
 * #### `SgSwitchStatement::get_body() const`
 * Access function for p_body.
 * - Return: Returns a pointer to a SgBasicBlock.
 *
 * #### `SgSwitchStatement::set_body(SgBasicBlock* body)`
 * Access function for p_body.
 * - Param `loop_body`: SgBasicBlock pointer
 * - Return: Returns void.
 */
class SgSwitchStatement;

/** @brief This class represents the concept of a name within the compiler.
 *
 * Symbols are a simpler way for the compiler to quickly associate types,
 * declarations and names.  It is also more compact than carrying around
 * names as strings.  The SgSymbol can contain many references and is a shared
 * IR node.
 * A "symbol" within the compiler is a concept of a name.  The simplest
 * example is a variable name, the variable most often has a name but internally
 * within the compiler the variable is referenced as a symbol, the symbol data
 * struction contains a name, type, and declaration (where the name was first
 * associated with the type).  Each variable has an associated symbol and each
 * variable reference internally stores a pointer to the single symbol representing
 * that variable.  this avoids the compiler internally relying upon string
 * processing to compile variable (different symbols can be trivially
 * compared to either have the same address on not).  Each named entity
 * within the language's translation within the AST has a unique symbol.
 * Mappings from names to symbols are held in the "Symbol Table" which provides
 * for a quick (hashed) lookup on the symbol for any name.  Mappings from the symbol to
 * the name are held through pointers to the declaration (stored within the symbol).
 * To organize the symbols, they are differentiated by type, each symbol is
 * derived from the SgSymbol class and defines a specialized interface for that
 * specific kind of IR node.  For example, there are symbol objects specific to classes,
 * templates, variable, enums, functions, member functions, lables, function types,
 * namespaces, typedefs, etc.  In each case these symbols contain a name (or method
 * for generating a name when it is located elsewhere), and a declaration, and
 * often a reference to a type (or function to returning the type if not held
 * in the symbol directly).
 * Each SgSymbol has a get_name() member function which gets the associated name
 * of the symbol, usually from the stored declaration. A few symbols have an explicitly
 * stored name (SgName), this is done where the name is derived from a named type (SgNamedType).
 * IMPORTANT: Declarations in Symbols \n
 * Indepedent of the different kinds of declarations (declarations are statements),
 * declarations can be considered to be definind and non-defining.  See
 * SgDeclarationStatement for details.  where there exist defining and
 * non-defining declarations symbols within Sage III always reference non-defining
 * declarations (only non-defining declarations are shared within the AST).
 * There are no uses of this IR node anywhere.  All expressions and statements
 * are derived from this IR node to build either SgExpression or SgStatement
 * derived classes.
 * - Internal: This is a base class for all symbols.
 * - Todo: Consider derivation of SgEnumSymbol, SgClassSymbol, SgFunctionTypeSymbol, and
 * SgTypedefSymbol from a common SgTypeSymbol.  Then supporting functions for
 * SgTypeSymbol would lookup any of these type based symbols.
 * - See also: SgDeclarationStatement
 * - See also:
 * Example of using a SgSymbol object
 *
 * **Member functions**
 *
 * #### `SgSymbol::SgSymbol()`
 * This is the default constructor.
 * This constructor builds the SgSymbol base class.
 * - See also:
 * Example:create an SgSymbol object
 *
 * #### `SgSymbol::~SgSymbol()`
 * This is the destructor.
 *
 * #### `SgSymbol::get_name() const`
 * Access function for getting name from declarations or types internally.
 * - Internal: This is a virtual function.
 * - Return: Returns SgName.
 *
 * #### `SgSymbol::get_type() const`
 * This function returns the type associated with the named entity.
 * - Internal: This is a virtual function.
 * - Return: Returns SgType*.
 *
 * #### `SgSymbol::isSgSymbol (SgNode *s)`
 * Cast function (from derived class to SgSymbol pointer).
 * This functions returns a SgSymbol pointer for any input of a
 * pointer to an object derived from a SgSymbol.
 * - Return: Returns valid pointer to SgSymbol if input is derived from a SgSymbol.
 *
 * #### `SgSymbol::isSgSymbol (const SgNode *s)`
 * Cast function (from derived class to SgSymbol pointer).
 * This functions returns a SgSymbol pointer for any input of a
 * pointer to an object derived from a SgSymbol.
 * - Return: Returns valid pointer to SgSymbol if input is derived from a SgSymbol.
 */
class SgSymbol;

/** @brief This class represents the symbol tables used in both SgScopeStatement and
 *
 * the SgFunctionTypeSymbolTable IR node.
 * - Todo: The rose_hash_multimap should perhaps be included as a data member instead of
 * implemented as a pointer.  We should consider this detail.
 * - Todo: We should decide if we want to give Symbol Tables a name or not, it seems that
 * we rarely if ever do this so perhaps we should not have such a field.
 * - See also:
 * Example of using a SgSymbolTable object
 *
 * **Data members**
 *
 * #### `SgSymbolTable::p_iterator`
 * This iterator is used within some of the deprecated functions.
 * - Deprecated: I think this iterator should be removed.
 *
 * #### `SgSymbolTable::p_name`
 * This is the symbol name (mangled is required).
 *
 * #### `SgSymbolTable::p_no_name`
 * This is a flag to indicate that p_name is empty
 * Some symbols don't have a name.
 * - Todo: generate a list of instances where symbols don't have names.
 * (Namespaces don't have to have a name for example).
 *
 * #### `SgSymbolTable::p_table`
 * This is lower level symbol table implementation (using an STL hash_multimap)
 *
 * **Member functions**
 *
 * #### `SgSymbolTable::SgSymbolTable()`
 * This is the constructor.
 * This constructor builds the SgSymbolTable base class.
 * - See also:
 * Example:create an SgSymbolTable object
 *
 * #### `SgSymbolTable::~SgSymbolTable()`
 * This is the destructor.
 * There is nothing to delete in this object.
 *
 * #### `SgSymbolTable::isSgSymbolTable (SgNode *s)`
 * Cast function (from derived class to SgSymbolTable pointer).
 * This functions returns a SgSymbolTable pointer for any input of a
 * pointer to an object derived from a SgSymbolTable.
 * - Return: Returns valid pointer to SgSymbolTable if input is a SgSymbolTable.
 *
 * #### `SgSymbolTable::isSgSymbolTable (const SgNode *s)`
 * Cast function (from derived class to SgSymbolTable pointer).
 * This functions returns a SgSymbolTable pointer for any input of a
 * pointer to an object derived from a SgSymbolTable.
 * - Return: Returns valid pointer to SgSymbolTable if input is a SgSymbolTable.
 */
class SgSymbolTable;

/** @brief This class represents template argument within the use of a template to build an instantiation.
 *
 * The template argument is used for the instatiation of templates.  They can also have
 * default values and not be specified explicitly.
 * - Internal: Note that this is not the same as a template parameter, which appears in the
 * template declaration as a placeholder for template argument in the declaration of the
 * template.
 * - Todo: Move this be a SgLocatedNode since it should have a position in the source code.
 * - See also:
 * Example of using a SgTemplateArgument object
 *
 * **Data members**
 *
 * #### `SgTemplateArgument::p_arumentType`
 * This is the type of the argument specificed to the template in the instantiation of
 * the template.
 * - Internal:
 *
 * #### `SgTemplateArgument::p_isArrayBoundUnknownType`
 * Template arguments can be used as array bounds, if so this this marks the arguments
 * use as such.
 *
 * #### `SgTemplateArgument::p_type`
 * This is the template argument's reference to a type (if it is a type).
 * This pointer is valid if the argumentType is "type_argument".
 *
 * #### `SgTemplateArgument::p_expression`
 * This is the template argument's reference to an expression (if it is an expression).
 * This pointer is valid if the argumentType is "nontype_argument".
 *
 * #### `SgTemplateArgument::p_templateInstantiation`
 * This is the associated template instantiation.
 *
 * #### `SgTemplateArgument::p_explicitlySpecified`
 * This true only if for a function template the argument is explicit in the source
 * code (it need not always be.)
 * For template<typename T> void foo(T t);
 * we could specify:
 * -# foo(1.0);
 * or:
 * -# foo<float>(1.0);
 * only in the second case is the template argument explicit.  For all instantiated
 * member function templates the value is false.
 * - Internal:
 *
 * **Member functions**
 *
 * #### `SgTemplateArgument::SgTemplateArgument(`
 * SgTemplateArgument::template_argument_enum argumentType=argument_undefined,
 * bool isArrayBoundUnknownType=false,
 * SgType *type=NULL,
 * SgExpression *expression=NULL,
 * SgTemplateInstantiationDecl *templateInstantiation=NULL,
 * bool explicitlySpecified=true)
 * Constructor using all possible parameters.
 * - Internal:
 * - See also:
 * Example:create an SgTemplateArgument object
 *
 * #### `SgTemplateArgument::SgTemplateArgument( SgType* arg, bool isExplicitlySpecified)`
 * Constructor used with types.
 * - Internal:
 * - See also:
 * Example:create an SgTemplateArgument object
 *
 * #### `SgTemplateArgument::SgTemplateArgument( SgExpression* arg, bool isExplicitlySpecified)`
 * Constructor used with expressions.
 * - Internal:
 * - See also:
 * Example:create an SgTemplateArgument object
 *
 * #### `SgTemplateArgument::SgTemplateArgument( SgTemplateInstantiationDecl *arg, bool isExplicitlySpecified)`
 * Constructor used with template instantiations.
 * - Internal:
 * - See also:
 * Example:create an SgTemplateArgument object
 *
 * #### `SgTemplateArgument::SgTemplateArgument(const SgTemplateArgument & X)`
 * This the copy constructor (deep copy made).
 * - Internal: This constructor does a deep copy of all data in the SgTemplateArgument object.
 * - See also:
 * Example:create an SgTemplateArgument object
 *
 * #### `SgTemplateArgument::~SgTemplateArgument()`
 * This is the destructor.
 * There is nothing to delete in this object.
 *
 * #### `SgTemplateArgument::isSgTemplateArgument (SgNode *s)`
 * Cast function (from derived class to SgTemplateArgument pointer).
 * This functions returns a SgTemplateArgument pointer for any input of a
 * pointer to an object derived from a SgTemplateArgument.
 * - Return: Returns valid pointer to SgTemplateArgument if input is derived from a SgLocatedNode.
 *
 * #### `SgTemplateArgument::isSgTemplateArgument (const SgNode *s)`
 * Cast function (from derived class to SgTemplateArgument pointer).
 * This functions returns a SgTemplateArgument pointer for any input of a
 * pointer to an object derived from a SgTemplateArgument.
 * - Return: Returns valid pointer to SgTemplateArgument if input is derived from a SgLocatedNode.
 *
 * #### `SgTemplateArgument::set_argumentType ( SgTemplateArgument::template_argument_enum argumentType )`
 * This function sets argumentType.
 * - Return: Returns void.
 *
 * #### `SgTemplateArgument::template_argument_enum SgTemplateArgument::get_argumentType () const`
 * This function returns argumentType.
 * - Return: Returns SgTemplateArgument::template_argument_enum.
 *
 * #### `SgTemplateArgument::set_isArrayBoundUnknownType ( bool isArrayBoundUnknownType )`
 * This function sets isArrayBoundUnknownType.
 * - Return: Returns void.
 *
 * #### `bool SgTemplateArgument::get_isArrayBoundUnknownType () const`
 * This function returns isArrayBoundUnknownType.
 * - Return: Returns bool.
 *
 * #### `set_type ( SgType *type )`
 * This function sets the argumentType.
 * - Return: Returns void.
 *
 * #### `SgType* SgTemplateArgument::get_type () const`
 * This function returns argumentType.
 * - Return: Returns SgType pointer.
 *
 * #### `set_expression ( SgExpression *exp )`
 * This function sets the argumentExpression.
 * - Return: Returns void.
 *
 * #### `SgExpression* SgTemplateArgument::get_expression () const`
 * This function returns argumentExpression.
 * - Return: Returns SgExpression pointer.
 *
 * #### `set_templateInstantiation ( SgTemplateInstantiationDecl *expression )`
 * This function sets the argumentExpression.
 * - Return: Returns void.
 *
 * #### `SgTemplateInstantiationDecl* SgTemplateArgument::get_templateInstantiation () const`
 * This function returns templateInstantiation.
 * - Return: Returns SgTemplateInstantiationDecl pointer.
 *
 * #### `SgTemplateArgument::set_explicitlySpecified ( bool explicitlySpecified )`
 * This function sets explicitlySpecified.
 * - Return: Returns void.
 *
 * #### `bool SgTemplateArgument::get_explicitlySpecified () const`
 * This function returns explicitlySpecified.
 * - Return: Returns bool.
 */
class SgTemplateArgument;

/** @brief This class represents the concept of a template declaration.
 *
 * This template declaration is internally marked as being either a
 * - class (e_template_class),
 * - member class (e_template_m_class),
 * - function (e_template_function),
 * - member function (e_template_m_function), or
 * - member data (e_template_m_data)
 * The name of the template is available from the get_name() member function.
 * The rest of the template is internally strored as a string (this will be
 * changed very quickly).  The template parameters are available within from
 * the get_templateParameters() member function.  The scope is stored explicitly
 * for this IR node, since name qualification permits it to have a scope
 * independent of its structural location within the source code.
 * - Todo: Template declarations marked as friend don't seem to be marked as friend internally.
 * - Todo: The scope of a SgTemplateDeclaration should be a SgTemplateInstantiationDefn, since
 * it could be associated which more than one definition.  What we need, and don't have
 * yet, is a SgTemplateDefinition to accompany the SgTemplateDeclaration then a
 * SgTemplateDeclaration could have a SgTemplateDefinition for a parent and or scope
 * when it is a member function or namespace or global scope (typically), otherwise.
 * - Todo: Make the "*PtrList" typed objects non-pointer data members (lists) instead of
 * pointer to lists.
 * - Note: Template specializations appear in syntax to be a template declaration (with
 * explicit template arguments), but they are really just an explicit template
 * instantiation. Thus Sage III stores such template specializations as template
 * instantiations and references the associated template declaration
 * (SgTemplateDeclaration) internally.
 * - See also:
 * Example of using a SgTemplateDeclaration object
 *
 * **Data members**
 *
 * #### `SgTemplateDeclaration::p_name`
 * This is the name of the template
 * This is the name of the template (e.g. for "template<class T> class X;",
 * the name is "X".
 *
 * #### `SgTemplateDeclaration::p_string`
 * This is the full template declaration as a string only.
 * This is the template declaration as a string (e.g. for "template<class T> class X {};",
 * the string is "template<class T> class X {};".
 * - Todo: Check the accuracy of this statement in the unparser!
 *
 * #### `SgTemplateDeclaration::p_template_kind`
 * This is the classification of the template declaration
 * This is the classification of the template declaration.
 * This template declaration is internally marked as being either a
 * - class (e_template_class),
 * - member class (e_template_m_class),
 * - function (e_template_function),
 * - member function (e_template_m_function), or
 * - member data (e_template_m_data)
 *
 * #### `SgTemplateDeclaration::p_templateParameters`
 * This is the STL list of pointers template parameters (SgTemplateParameter objects)
 * This is the STL list of pointers to template parameters (SgTemplateParameter objects)
 * For example, for "template<class T> class X;", the STL list would contain a SgTemplateParameter
 * representing T.
 *
 * #### `SgTemplateDeclaration::p_scope`
 * This is the scope of the template declaration.
 * This is stored explicitly since name qualification can be used to
 * place some declarations in positions that are different from their
 * scope if it were computed structureally from the parent pointer data.
 *
 * **Member functions**
 *
 * #### `SgTemplateDeclaration::SgTemplateDeclaration ( Sg_File_Info* startOfConstruct = NULL )`
 * This is the constructor.
 * This constructor builds the SgTemplateDeclaration base class.
 * - Param `startOfConstruct`: represents the position in the source code
 *
 * #### `SgTemplateDeclaration::~SgTemplateDeclaration()`
 * This is the destructor.
 * There are a lot of things to delete, but nothing is deleted in this destructor.
 *
 * #### `SgTemplateDeclaration::isSgTemplateDeclaration (SgNode *s)`
 * Cast function (from derived class to SgTemplateDeclaration pointer).
 * This functions returns a SgTemplateDeclaration pointer for any input of a
 * pointer to an object derived from a SgTemplateDeclaration.
 * - Return: Returns valid pointer to SgTemplateDeclaration if input is derived from a SgLocatedNode.
 *
 * #### `SgTemplateDeclaration::isSgTemplateDeclaration (const SgNode *s)`
 * Cast function (from derived class to SgTemplateDeclaration pointer).
 * This functions returns a SgTemplateDeclaration pointer for any input of a
 * pointer to an object derived from a SgTemplateDeclaration.
 * - Return: Returns valid pointer to SgTemplateDeclaration if input is derived from a SgLocatedNode.
 *
 * #### `SgTemplateDeclaration::get_name()`
 * Returns name of template declaration.
 * - Return: Returns SgName by value.
 *
 * #### `SgTemplateDeclaration::set_name(SgName name)`
 * Access function for p_name.
 * - Return: Returns void.
 *
 * #### `SgTemplateDeclaration::get_string()`
 * Returns stringified template declaration.
 * - Return: Returns SgName by value.
 *
 * #### `SgTemplateDeclaration::set_string(SgName name)`
 * Access function for p_string.
 * - Return: Returns void.
 *
 * #### `SgTemplateDeclaration::get_template_kind()`
 * Returns enum value (function, class, etc.)
 * - Return: Returns enum value
 *
 * #### `SgTemplateDeclaration::set_template_kind(SgName name)`
 * Access function for p_template_kind.
 * - Return: Returns void.
 */
class SgTemplateDeclaration;

/** @brief This class represents the concept of an instantiated class template.
 *
 * - Internal: Objects of this class are marked as compiler-generated if they are instantiated
 * by the compiler, but not if they are generated from a specialization (where the user
 * has in effect generated it).  A few details:
 * - Currently multple declaration have a valid pointer to the
 * SgTemplateInstantiationDefn object even though there is only one defining
 * declaration.  This is different from functions declarations where only one declaration
 * has a valid pointer to the SgFunctionDefinition.  This needs to be made consistant at
 * some point.
 * - Todo: Make the "*PtrList" typed objects non-pointer data members (lists) instead of
 * pointer to lists.
 * - See also:
 * Example of using a SgTemplateInstantiationDecl object
 *
 * **Data members**
 *
 * #### `SgTemplateInstantiationDecl::p_templateName`
 * This is the name of the templated class (excludes template arguments)
 * - Internal: This name is computed internally (in AST post-processing) and stored
 * explicitly.  It can be reset by calling resetTemplateName(SgTemplateInstantiationDecl).
 *
 * #### `SgTemplateInstantiationDecl::p_templateHeader`
 * This data field is not used (or is used internally).
 * I forget what this name includes.
 *
 * #### `SgTemplateInstantiationDecl::p_templateDeclaration`
 * This is the template declarations (SgTemplateDeclaration) from which this template
 * instantiation is constructed.  This can be NULL, as I recall, for nested classes.
 *
 * #### `SgTemplateInstantiationDecl::p_templateArguments`
 * This is a pointer to a list of pointers to SgTemplateArgument objects (used with
 * the SgTemplateDeclaration to construct this instantiation).
 * The template arguments are used to generate the full template name recorded in
 * p_templateName.  All arguments are saved into the argument list, but not all
 * arguments are output in the generated code (see SgTemplateArgument for details).
 * - Note: Template arguments are found in the instantiations, and template parameters are found
 * in the SgTemplateDeclaration (arguments are not the same a parameters, same as for
 * descriptions for functions).
 *
 * #### `SgTemplateInstantiationDecl::p_nameResetFromMangledForm`
 * This bool value is set to false at construction and later reset to true
 * within post-processing as each instantiated template name is reset from EDG
 * names (e.g. "A____L42") to ROSE generated names (e.g. "A<int>").  Either names
 * would work as well for some purposes, but for source to source translation purposes
 * we wanted the original names to be used (also avoids/simplifies linking issues
 * using generated code with non-generated code).
 * - Internal: This variable is set/reset internally and there is no need for it to be set by the user!
 *
 * **Member functions**
 *
 * #### `SgTemplateInstantiationDecl::SgTemplateInstantiationDecl ( Sg_File_Info* startOfConstruct = NULL,`
 * SgName name = SgdefaultName, int class_type = 0, SgClassType *type=NULL,
 * SgClassDefinition *definition=NULL)
 * This is the only constructor.
 * This constructor builds the SgTemplateInstantiationDecl base class. but has some specific
 * side-effects (some of which have been removed in the latest work).  It is however
 * still that case that if the definition is provided then it's declaration will be changed
 * to the declaration being constructed (a warning message it output if this happens and
 * this side-effect will be removed soon).
 * - Param `startOfConstruct`: represents the position in the source code
 * - See also:
 * Example:create an SgTemplateInstantiationDecl object
 *
 * #### `SgTemplateInstantiationDecl::~SgTemplateInstantiationDecl()`
 * This is the destructor.
 * There are a lot of things to delete, but nothing is deleted in this destructor.
 *
 * #### `SgTemplateInstantiationDecl::isSgTemplateInstantiationDecl (SgNode *s)`
 * Cast function (from derived class to SgTemplateInstantiationDecl pointer).
 * This functions returns a SgTemplateInstantiationDecl pointer for any input of a
 * pointer to an object derived from a SgTemplateInstantiationDecl.
 * - Return: Returns valid pointer to SgTemplateInstantiationDecl if input is derived from a SgLocatedNode.
 *
 * #### `SgTemplateInstantiationDecl::isSgTemplateInstantiationDecl (const SgNode *s)`
 * Cast function (from derived class to SgTemplateInstantiationDecl pointer).
 * This functions returns a SgTemplateInstantiationDecl pointer for any input of a
 * pointer to an object derived from a SgTemplateInstantiationDecl.
 * - Return: Returns valid pointer to SgTemplateInstantiationDecl if input is derived from a SgLocatedNode.
 *
 * #### `SgTemplateInstantiationDecl::get_templateName() const`
 * Returns name of class template, the name excludes template arguments.
 * For the template name with arguments, e.g. "class_template<int>" the user should call
 * get_name() (defined in the SgClassDeclaration base class).
 * - Note: The SgClassSymbol used to hold a reference to this declaration in the symbol table
 * is placed into the symbol table using the get_name(); includes template arguments.
 * - Internal: There are cases where the arguments should not be unparse and then
 * this version of the name is required. Also in generating mangled names, this
 * is a more useful name to have separately from the name which inclused arguments.
 * However in these case it still seems that we could get it from the template
 * declaration (if it is available, member fucntions of a templated class may not
 * have an explicit or separate template declaration).
 * - Return: returns SgName by value.
 *
 * #### `SgTemplateInstantiationDecl::set_templateName(SgName name)`
 * sets name of instantiated class template, name excludes template arguments.
 * - Return: returns void.
 *
 * #### `SgTemplateInstantiationDecl::get_templateDeclaration() const`
 * Returns pointer to SgTemplateDeclaration from which instantiation is generated.
 * - Return: returns pointer to SgTemplateDeclaration.
 *
 * #### `SgTemplateInstantiationDecl::set_templateDeclaration(SgTemplateDeclaration* templateDeclaration)`
 * Access function for p_templateDeclaration.
 * - Return: returns void.
 *
 * #### `SgTemplateInstantiationDecl::get_templateArguments() const`
 * Returns pointer to STL list of pointers to SgTemplateArgument objects.
 * - Return: Returns pointer to STL list of pointers to SgTemplateArgument objects.
 *
 * #### `SgTemplateInstantiationDecl::set_templateArguments(SgTemplateArgumentPtrListPtr templateArguments)`
 * Access function for p_templateArguments.
 * - Return: returns void.
 *
 * #### `SgTemplateInstantiationDecl::get_nameResetFromMangledForm() const`
 * Returns pointer to SgTemplateDeclaration from which instantiation is generated.
 * - Return: returns pointer to SgTemplateDeclaration.
 *
 * #### `SgTemplateInstantiationDecl::set_nameResetFromMangledForm(bool nameResetFromMangledForm)`
 * Access function for p_nameResetFromMangledForm.
 * - Return: returns void.
 */
class SgTemplateInstantiationDecl;

/** @brief This class represents the concept of a class definition in C++.
 *
 * Templated class definitions are currently nearly the same as class definitions
 * (SgClassDefinition).  I expect that work in the future will provide more features that
 * are specific to templates.  Currently this version is different from the SgClassDefinition
 * in that its constructor takes a SgTemplateInstantiationDecl instead of a SgClassDeclaration.
 * - Internal: This is not a base class for IR nodes.
 *
 * **Member functions**
 *
 * #### `SgTemplateInstantiationDefn::SgTemplateInstantiationDefn (`
 * Sg_File_Info* startOfConstruct = NULL, SgTemplateInstantiationDecl *decl = NULL )
 * This is the constructor.
 * This constructor builds the SgTemplateInstantiationDefn base class.
 * - Param `startOfConstruct`: represents the position in the source code
 *
 * #### `SgTemplateInstantiationDefn::~SgTemplateInstantiationDefn()`
 * This is the destructor.
 * There are a lot of things to delete, but nothing is deleted in this destructor.
 *
 * #### `SgTemplateInstantiationDefn::isSgTemplateInstantiationDefn (SgNode *s)`
 * Cast function (from derived class to SgTemplateInstantiationDefn pointer).
 * This functions returns a SgTemplateInstantiationDefn pointer for any input of a
 * pointer to an object derived from a SgTemplateInstantiationDefn.
 * - Return: Returns valid pointer to SgTemplateInstantiationDefn if input is derived from a SgLocatedNode.
 *
 * #### `SgTemplateInstantiationDefn::isSgTemplateInstantiationDefn (const SgNode *s)`
 * Cast function (from derived class to SgTemplateInstantiationDefn pointer).
 * This functions returns a SgTemplateInstantiationDefn pointer for any input of a
 * pointer to an object derived from a SgTemplateInstantiationDefn.
 * - Return: Returns valid pointer to SgTemplateInstantiationDefn if input is derived from a SgLocatedNode.
 *
 * #### `SgTemplateInstantiationDefn::copy(const SgCopyHelp & help)`
 * Makes a copy (deap of shallow depending on SgCopyHelp).
 * - Return: Returns pointer to copy of SgTemplateInstantiationDefn.
 */
class SgTemplateInstantiationDefn;

/** @brief This class represents the concept of a C++ template instantiation directive.
 *
 * This statement controls the instantiation of template, forcing their explicit
 * instantiation.  It provides a mechanism to control the instantiation of template,
 * useful in large projects and libraries.
 * - Internal: Currently the declaration representing the class being instantiated is not
 * traversed.  Likely it should be since this is where the instantiated member functions
 * are found.  As an instantiation is should be marked as compiler-generated, however it
 * is explicitly generated by a directive, no information about what forced it to be
 * instantiated is made available in the AST, though it is not clear that this is needed.
 * - Todo: Consider tranversing instantated templates instantiated by an explicit
 * instanntatiation directive.
 * - See also:
 * Example of using a SgTemplateInstantiationDirectiveStatement object
 *
 * **Data members**
 *
 * #### `SgTemplateInstantiationDirectiveStatement::p_declaration`
 * This pointer points to associated template instantiation declaration meant to be
 * explicitly instantiated.
 * Points to either (I think this is a complete list):
 * - SgTemplateInstantiationDecl
 * - SgTemplateInstantiationFunctionDecl
 * - SgTemplateInstantiationMemberFunctionDecl
 * - Internal: This is a SgDeclarationStatement so that it can reference a number of different
 * declaration IR nodes (but not any SgDeclaration, since many don't make sense).
 *
 * **Member functions**
 *
 * #### `SgTemplateInstantiationDirectiveStatement::SgTemplateInstantiationDirectiveStatement ( Sg_File_Info* startOfConstruct = NULL )`
 * This is the constructor.
 * This constructor builds the SgTemplateInstantiationDirectiveStatement base class.
 * - Param `startOfConstruct`: represents the position in the source code
 *
 * #### `SgTemplateInstantiationDirectiveStatement::~SgTemplateInstantiationDirectiveStatement()`
 * This is the destructor.
 * There are a lot of things to delete, but nothing is deleted in this destructor.
 *
 * #### `SgTemplateInstantiationDirectiveStatement::isSgTemplateInstantiationDirectiveStatement (SgNode *s)`
 * Cast function (from derived class to SgTemplateInstantiationDirectiveStatement pointer).
 * This functions returns a SgTemplateInstantiationDirectiveStatement pointer for any input of a
 * pointer to an object derived from a SgTemplateInstantiationDirectiveStatement.
 * - Return: Returns valid pointer to SgTemplateInstantiationDirectiveStatement if input is derived from a SgLocatedNode.
 *
 * #### `SgTemplateInstantiationDirectiveStatement::isSgTemplateInstantiationDirectiveStatement (const SgNode *s)`
 * Cast function (from derived class to SgTemplateInstantiationDirectiveStatement pointer).
 * This functions returns a SgTemplateInstantiationDirectiveStatement pointer for any input of a
 * pointer to an object derived from a SgTemplateInstantiationDirectiveStatement.
 * - Return: Returns valid pointer to SgTemplateInstantiationDirectiveStatement if input is derived from a SgLocatedNode.
 *
 * #### `SgTemplateInstantiationDirectiveStatement::get_declaration() const`
 * Returns pointer to SgDeclarationStatement.
 * - Return: Returns pointer to SgDeclarationStatement.
 *
 * #### `SgTemplateInstantiationDirectiveStatement::set_declaration(SgDeclarationStatement* declaration)`
 * Access function for p_declaration.
 * - Return: Returns void.
 */
class SgTemplateInstantiationDirectiveStatement;

/** @brief This class represents the concept of an instantiation of function template.
 *
 * - Internal:
 * - Todo: Make the "*PtrList" typed objects non-pointer data members (lists) instead of
 * pointer to lists.
 * - See also:
 * Example of using a SgTemplateInstantiationFunctionDecl object
 *
 * **Data members**
 *
 * #### `SgTemplateInstantiationFunctionDecl::p_templateName`
 * This is the name of the templated class (in the form "name<args>")
 * - Internal: This name is computed internally (in AST post-processing) and stored
 * explicitly.  It can be reset by calling resetTemplateName(SgTemplateInstantiationDecl).
 *
 * #### `SgTemplateInstantiationFunctionDecl::p_templateDeclaration`
 * This is the template declarations (SgTemplateDeclaration) from which this template
 * instantiation is constructed.  This can be NULL, as I recall, for nested classes.
 *
 * #### `SgTemplateInstantiationFunctionDecl::p_templateArguments`
 * This is a pointer to a list of pointers to SgTemplateArgument objects (used with
 * the SgTemplateDeclaration to construct this instantiation).
 * The template arguments are used to generate the full template name recorded in
 * p_templateName.  All arguments are saved into the argument list, but not all
 * arguments are output in the generated code (see SgTemplateArgument for details).
 * - Note: Template arguments are found in the instantiations, and template parameters are found
 * in the SgTemplateDeclaration (arguments are not the same a parameters, same as for
 * descriptions for functions).
 *
 * #### `SgTemplateInstantiationFunctionDecl::p_nameResetFromMangledForm`
 * This bool value is set to false at construction and later reset to true
 * within post-processing as each instantiated template name is reset from EDG
 * names (e.g. "A____L42") to ROSE generated names (e.g. "A<int>").  Either names
 * would work as well for some purposes, but for source to source translation purposes
 * we wanted the original names to be used (also avoids/simplifies linking issues
 * using generated code with non-generated code).
 * - Internal: This variable is set/reset internally and there is no need for it to be set by the user!
 *
 * **Member functions**
 *
 * #### `SgTemplateInstantiationFunctionDecl::SgTemplateInstantiationFunctionDecl ( Sg_File_Info* startOfConstruct = NULL )`
 * This is the constructor.
 * This constructor builds the SgTemplateInstantiationFunctionDecl base class.
 * - Param `startOfConstruct`: represents the position in the source code
 *
 * #### `SgTemplateInstantiationFunctionDecl::~SgTemplateInstantiationFunctionDecl()`
 * This is the destructor.
 * There are a lot of things to delete, but nothing is deleted in this destructor.
 *
 * #### `SgTemplateInstantiationFunctionDecl::isSgTemplateInstantiationFunctionDecl (SgNode *s)`
 * Cast function (from derived class to SgTemplateInstantiationFunctionDecl pointer).
 * This functions returns a SgTemplateInstantiationFunctionDecl pointer for any input of a
 * pointer to an object derived from a SgTemplateInstantiationFunctionDecl.
 * - Return: Returns valid pointer to SgTemplateInstantiationFunctionDecl if input is derived from a SgLocatedNode.
 *
 * #### `SgTemplateInstantiationFunctionDecl::isSgTemplateInstantiationFunctionDecl (const SgNode *s)`
 * Cast function (from derived class to SgTemplateInstantiationFunctionDecl pointer).
 * This functions returns a SgTemplateInstantiationFunctionDecl pointer for any input of a
 * pointer to an object derived from a SgTemplateInstantiationFunctionDecl.
 * - Return: Returns valid pointer to SgTemplateInstantiationFunctionDecl if input is derived from a SgLocatedNode.
 *
 * #### `SgTemplateInstantiationFunctionDecl::get_templateName() const`
 * Returns name of instantiated function template, name includes template arguments.
 * - Return: returns SgName by value.
 *
 * #### `SgTemplateInstantiationFunctionDecl::set_templateName(SgName name)`
 * sets name of instantiated function template, name includes template arguments.
 * - Return: returns void.
 *
 * #### `SgTemplateInstantiationFunctionDecl::get_templateDeclaration() const`
 * Returns pointer to SgTemplateDeclaration from which instantiation is generated.
 * - Return: returns pointer to SgTemplateDeclaration.
 *
 * #### `SgTemplateInstantiationFunctionDecl::set_templateDeclaration(SgTemplateDeclaration* templateDeclaration)`
 * Access function for p_templateDeclaration.
 * - Return: returns void.
 *
 * #### `SgTemplateInstantiationFunctionDecl::get_templateArguments() const`
 * Returns pointer to STL list of pointers to SgTemplateArgument objects.
 * - Return: Returns pointer to STL list of pointers to SgTemplateArgument objects.
 *
 * #### `SgTemplateInstantiationFunctionDecl::set_templateArguments(SgTemplateArgumentPtrListPtr templateArguments)`
 * Access function for p_templateArguments.
 * - Return: returns void.
 *
 * #### `SgTemplateInstantiationFunctionDecl::get_nameResetFromMangledForm() const`
 * Returns pointer to SgTemplateDeclaration from which instantiation is generated.
 * - Return: returns pointer to SgTemplateDeclaration.
 *
 * #### `SgTemplateInstantiationFunctionDecl::set_nameResetFromMangledForm(bool nameResetFromMangledForm)`
 * Access function for p_nameResetFromMangledForm.
 * - Return: returns void.
 */
class SgTemplateInstantiationFunctionDecl;

/** @brief This class represents the concept of an instantiation of member function template
 *
 * or a member function of an instantiation of a template class.
 * - Internal:
 * - Todo: Make the "*PtrList" typed objects non-pointer data members (lists) instead of
 * pointer to lists.
 *
 * **Data members**
 *
 * #### `SgTemplateInstantiationMemberFunctionDecl::p_templateName`
 * This is the name of the templated class (in the form "name<args>")
 * - Internal: This name is computed internally (in AST post-processing) and stored
 * explicitly.  It can be reset by calling resetTemplateName(SgTemplateInstantiationDecl).
 *
 * #### `SgTemplateInstantiationMemberFunctionDecl::p_templateDeclaration`
 * This is the template declarations (SgTemplateDeclaration) from which this template
 * instantiation is constructed.  This can be NULL, as I recall, for nested classes.
 *
 * #### `SgTemplateInstantiationMemberFunctionDecl::p_templateArguments`
 * This is a pointer to a list of pointers to SgTemplateArgument objects (used with
 * the SgTemplateDeclaration to construct this instantiation).
 * The template arguments are used to generate the full template name recorded in
 * p_templateName.  All arguments are saved into the argument list, but not all
 * arguments are output in the generated code (see SgTemplateArgument for details).
 * - Note: Template arguments are found in the instantiations, and template parameters are found
 * in the SgTemplateDeclaration (arguments are not the same a parameters, same as for
 * descriptions for functions).
 *
 * #### `SgTemplateInstantiationMemberFunctionDecl::p_nameResetFromMangledForm`
 * This bool value is set to false at construction and later reset to true
 * within post-processing as each instantiated template name is reset from EDG
 * names (e.g. "A____L42") to ROSE generated names (e.g. "A<int>").  Either names
 * would work as well for some purposes, but for source to source translation purposes
 * we wanted the original names to be used (also avoids/simplifies linking issues
 * using generated code with non-generated code).
 * - Internal: This variable is set/reset internally and there is no need for it to be set by the user!
 *
 * **Member functions**
 *
 * #### `SgTemplateInstantiationMemberFunctionDecl::SgTemplateInstantiationMemberFunctionDecl ( Sg_File_Info* startOfConstruct = NULL )`
 * This is the constructor.
 * This constructor builds the SgTemplateInstantiationMemberFunctionDecl base class.
 * - Param `startOfConstruct`: represents the position in the source code
 *
 * #### `SgTemplateInstantiationMemberFunctionDecl::~SgTemplateInstantiationMemberFunctionDecl()`
 * This is the destructor.
 * There are a lot of things to delete, but nothing is deleted in this destructor.
 *
 * #### `SgTemplateInstantiationMemberFunctionDecl::isSgTemplateInstantiationMemberFunctionDecl (SgNode *s)`
 * Cast function (from derived class to SgTemplateInstantiationMemberFunctionDecl pointer).
 * This functions returns a SgTemplateInstantiationMemberFunctionDecl pointer for any input of a
 * pointer to an object derived from a SgTemplateInstantiationMemberFunctionDecl.
 * - Return: Returns valid pointer to SgTemplateInstantiationMemberFunctionDecl if input is derived from a SgLocatedNode.
 *
 * #### `SgTemplateInstantiationMemberFunctionDecl::isSgTemplateInstantiationMemberFunctionDecl (const SgNode *s)`
 * Cast function (from derived class to SgTemplateInstantiationMemberFunctionDecl pointer).
 * This functions returns a SgTemplateInstantiationMemberFunctionDecl pointer for any input of a
 * pointer to an object derived from a SgTemplateInstantiationMemberFunctionDecl.
 * - Return: Returns valid pointer to SgTemplateInstantiationMemberFunctionDecl if input is derived from a SgLocatedNode.
 *
 * #### `SgTemplateInstantiationMemberFunctionDecl::get_templateName() const`
 * Returns name of instantiated function template, name includes template arguments.
 * - Return: returns SgName by value.
 *
 * #### `SgTemplateInstantiationMemberFunctionDecl::set_templateName(SgName name)`
 * sets name of instantiated function template, name includes template arguments.
 * - Return: returns void.
 *
 * #### `SgTemplateInstantiationMemberFunctionDecl::get_templateDeclaration() const`
 * Returns pointer to SgTemplateDeclaration from which instantiation is generated.
 * - Return: returns pointer to SgTemplateDeclaration.
 *
 * #### `SgTemplateInstantiationMemberFunctionDecl::set_templateDeclaration(SgTemplateDeclaration* templateDeclaration)`
 * Access function for p_templateDeclaration.
 * - Return: returns void.
 *
 * #### `SgTemplateInstantiationMemberFunctionDecl::get_templateArguments() const`
 * Returns pointer to STL list of pointers to SgTemplateArgument objects.
 * - Return: Returns pointer to STL list of pointers to SgTemplateArgument objects.
 *
 * #### `SgTemplateInstantiationMemberFunctionDecl::set_templateArguments(SgTemplateArgumentPtrListPtr templateArguments)`
 * Access function for p_templateArguments.
 * - Return: returns void.
 *
 * #### `SgTemplateInstantiationMemberFunctionDecl::get_nameResetFromMangledForm() const`
 * Returns pointer to SgTemplateDeclaration from which instantiation is generated.
 * - Return: returns pointer to SgTemplateDeclaration.
 *
 * #### `SgTemplateInstantiationMemberFunctionDecl::set_nameResetFromMangledForm(bool nameResetFromMangledForm)`
 * Access function for p_nameResetFromMangledForm.
 * - Return: returns void.
 */
class SgTemplateInstantiationMemberFunctionDecl;

/** @brief This class represents the "this" operator (can be applied to any member data).
 *
 * This shows up in the access to member data and is sometimes implicit.
 * Recent changed to the strictness of C++ addressed in several compilers has
 * made this recently more explicitly required.
 * - Internal:
 * - See also:
 * Example of using a SgThisExp object
 *
 * **Data members**
 *
 * #### `SgThisExp::p_class_symbol`
 * This is the symbol of the class to which the "this" operator is applied.
 *
 * #### `SgThisExp::p_pobj_this`
 * This is not used and is related to a flag from CC++.
 * - Deprecated: This will be removed in a future release.
 *
 * **Member functions**
 *
 * #### `SgThisExp::SgThisExp ( Sg_File_Info* startOfConstruct = NULL )`
 * This is the constructor.
 * This constructor builds the SgThisExp base class.
 * - Param `startOfConstruct`: represents the position in the source code
 * - See also:
 * Example:create an SgThisExp object
 *
 * #### `SgThisExp::~SgThisExp()`
 * This is the destructor.
 * Only the Sg_File_Info object can be deleted in this object.
 *
 * #### `SgThisExp::isSgThisExp (SgNode *s)`
 * Cast function (from derived class to SgThisExp pointer).
 * This functions returns a SgThisExp pointer for any input of a
 * pointer to an object derived from a SgThisExp.
 * - Return: Returns valid pointer to SgThisExp if input is derived from a SgThisExp.
 *
 * #### `SgThisExp::isSgThisExp (const SgNode *s)`
 * Cast function (from derived class to SgThisExp pointer).
 * This functions returns a SgThisExp pointer for any input of a
 * pointer to an object derived from a SgThisExp.
 * - Return: Returns valid pointer to SgThisExp if input is derived from a SgThisExp.
 */
class SgThisExp;

/** @brief This class represents the C++ throw expression (handled as a unary operator).
 *
 * This class represents three different approaches to the use of the throw
 * expression within a C++ program.
 * 1) throw expression (with specified function)
 * 2) throw exceoption specification (with list of types to throw)
 * 3) rethrow current exception (specified as simply "throw;")
 * The constructor takes the parameters required to empliment these three kinds of throws.
 * - Internal: We should perhaps define specialized constructors for each kind of throw.
 * - Todo: Make the "*PtrList" typed objects non-pointer data members (lists) instead of
 * pointer to lists.
 * - Todo: Consider that get_type() returns a SgDefalutType and should return the SgType
 * associated with the last expression in the list (research details of the list of
 * pointers in the C++ throw operator).
 * - See also:
 * Example:create an SgThrowOp object
 * Example of using a SgThrowOp object
 * - See also:
 * Example throw in C++ example
 *
 * **Data members**
 *
 * #### `SgThrowOp::p_typeList`
 * This list of types required to support the exception-specification throw.
 * The throw operator can take a list of types, this is called and
 * exception-specification throw (see C++ specification for details).
 *
 * #### `SgThrowOp::p_throwKind`
 * This enum value classifies the throw as either of three different kinds.
 * This variable is used to classify the throw and either:
 * #) throw expression (with specified function)
 * #) throw exceoption specification (with list of types to throw)
 * #) rethrow current exception (specified as simply "throw;")
 * which are used internally to guide the unparsing and interogation of
 * the IR node.
 *
 * **Member functions**
 *
 * #### `SgThrowOp::SgThrowOp ( Sg_File_Info* startOfConstruct = NULL , SgExpression *operand_i=NULL, SgType *expression_type=NULL, SgTypePtrListPtr typeList=NULL, SgThrowOp::e_throw_kind throwKind=SgThrowOp::unknown_throw)`
 * This is the only constructor.
 * This constructor builds the SgThrowOp base class.
 * - Param `startOfConstruct`: represents the position in the source code
 * - See also:
 * Example:create an SgThrowOp object
 * Example of using a SgThrowOp object
 *
 * #### `SgThrowOp::~SgThrowOp()`
 * This is the destructor.
 * Only the Sg_File_Info object can be deleted in this object.
 *
 * #### `SgThrowOp::isSgThrowOp (SgNode *s)`
 * Cast function (from derived class to SgThrowOp pointer).
 * This functions returns a SgThrowOp pointer for any input of a
 * pointer to an object derived from a SgThrowOp.
 * - Return: Returns valid pointer to SgThrowOp if input is derived from a SgThrowOp.
 *
 * #### `SgThrowOp::isSgThrowOp (const SgNode *s)`
 * Cast function (from derived class to SgThrowOp pointer).
 * This functions returns a SgThrowOp pointer for any input of a
 * pointer to an object derived from a SgThrowOp.
 * - Return: Returns valid pointer to SgThrowOp if input is derived from a SgThrowOp.
 *
 * #### `SgThrowOp::get_typeList()`
 * Return internal pointer to SgTypePtrList (STL list of SgType pointers).
 * Return internal pointer to SgTypePtrList (STL list of SgType pointers).
 * - Return: Returns valid pointer to SgTypePtrList.
 */
class SgThrowOp;

/** @brief This class represents the concept of try statement within the try-catch
 *
 * support for exception handling in C++.
 * - See also:
 * Example of using a SgTryStmt object
 *
 * **Data members**
 *
 * #### `SgTryStmt::p_body`
 * This pointer points to a SgBasicBlock containing the statements to be execued by
 * the try block.
 *
 * #### `SgTryStmt::p_catch_statement_seq_root`
 * This pointer points to a SgCatchStatementSeq and connects the try statement to the
 * sequence of catch statements within the support of exception handling.
 *
 * #### `SgTryStmt::p_else_body`
 * This pointer points to a SgBasicBlock containing the statements to be executed when
 * control flows off the end of the try clause. This member is intended for use with Python,
 * and is NULL otherwise.
 * /*! \var SgTryStmt::p_finally_body
 * This pointer points to a SgBasicBlock containing the statements to be executed when
 * an unhandled exception occurs. This member is intended for use with Python, and is NULL
 * otherwise.
 * // Documentation for class SgTryStmt member functions (methods)
 * /*!
 * This is the constructor.
 * This constructor builds the SgTryStmt base class.
 * - Param `startOfConstruct`: represents the position in the source code
 *
 * **Member functions**
 *
 * #### `SgTryStmt::~SgTryStmt()`
 * This is the destructor.
 * There are a lot of things to delete, but nothing is deleted in this destructor.
 *
 * #### `SgTryStmt::isSgTryStmt (SgNode *s)`
 * Cast function (from derived class to SgTryStmt pointer).
 * This functions returns a SgTryStmt pointer for any input of a
 * pointer to an object derived from a SgTryStmt.
 * - Return: Returns valid pointer to SgTryStmt if input is derived from a SgLocatedNode.
 *
 * #### `SgTryStmt::isSgTryStmt (const SgNode *s)`
 * Cast function (from derived class to SgTryStmt pointer).
 * This functions returns a SgTryStmt pointer for any input of a
 * pointer to an object derived from a SgTryStmt.
 * - Return: Returns valid pointer to SgTryStmt if input is derived from a SgLocatedNode.
 *
 * #### `SgTryStmt::get_body() const`
 * Returns pointer to SgBasicBlock.
 * - Return: Returns pointer to SgBasicBlock.
 *
 * #### `SgTryStmt::set_body(SgBasicBlock* body)`
 * Access function for p_body.
 * - Return: Returns void.
 *
 * #### `SgTryStmt::get_catch_statement_seq_root() const`
 * Returns pointer to SgCatchStatementSeq.
 * - Return: Returns pointer to SgCatchStatementSeq.
 *
 * #### `SgTryStmt::set_catch_statement_seq_root(SgCatchStatementSeq* catch_statement_seq_root)`
 * Access function for p_catch_statement_seq_root.
 * - Return: Returns void.
 */
class SgTryStmt;

/** @brief This class represents a tuple display.
 *
 * This class represents the concept of a tuple object in the input language. Currently, this IR node only works with Python input files.
 *
 * **Data members**
 *
 * #### `SgTupleExp::p_elements`
 * The list of elements contained in this tuple.
 *
 * **Member functions**
 *
 * #### `SgTupleExp::SgTupleExp ()`
 * This is the constructor.
 * This constructor builds the SgTupleExp base class.
 *
 * #### `SgTupleExp::isSgTupleExp (SgNode *s)`
 * Cast function (from derived class to SgTupleExp pointer).
 * This functions returns a SgTupleExp pointer for any input of a
 * pointer to an object derived from a SgTupleExp.
 * - Return: Returns valid pointer to SgTupleExp if input is derived from a SgTupleExp.
 *
 * #### `SgTupleExp::isSgTupleExp (const SgNode *s)`
 * Cast function (from derived class to SgTupleExp pointer).
 * This functions returns a SgTupleExp pointer for any input of a
 * pointer to an object derived from a SgTupleExp.
 * - Return: Returns valid pointer to SgTupleExp if input is derived from a SgTupleExp.
 */
class SgTupleExp;

/** @brief This class represents the base class for all types.
 *
 * There are no uses of this IR node anywhere.  All SgType based IR nodes
 * are derived from this SgType class.
 * Discussion of get_base_type() and findBaseType():
 * -# fileBaseType() will recursively strip away all typedefs, reference, pointers, arrays,
 * and modifiers
 * -# get_base_type() is a member function on some IR nodes derived from SgType and
 * returns the non-recursively striped (immediate) type under the typedefs, reference,
 * pointers, arrays, modifiers, etc.
 * Note that the typedefs for which the current SgType is the base type are stored in a list.
 * There is a special IR node (SgTypedefSeq) to maintain this list (which is internally an
 * STL list).  We could also store pointers to other SgType IR nodes where pointers and
 * references have been taken to the current IR node, but currently only typedefs are
 * stored explicitly.  As a result of storing the typedefs, the builtin types store the
 * list for most SgType IR nodes (since ther is only a single SgType IR node for most
 * SgType derived classes).  We could imagine storing the SgTypedefSeq as static data members
 * of the derived SgType classes, but this would not work since then all SgNamedType IR nodes
 * would share the same list of typedefs.  So this is the correct location for this list.
 * - Internal: This is a base class for all SgType objects.
 * - Todo: Several classes derived from SgType are not used and can be removed:
 * -# SgTypeUnknown (Used by SageInterface to specify references to undeclared variables)
 * -# SgUnknownMemberFunctionType
 * - Todo: The signed types (except for signed char) are not used in SAGE III and do not exist
 * in C or C++.  These IR nodes should be removed, specifically SgTypeSignedShort,
 * SgTypeSignedInt, SgTypeSignedLong.
 * - Todo: For Fortran support we need to add the kind, length data member to specify the width.
 * To support handling of kind, length parameters we should use the information about
 * the target backend compiler and map kind information to bit widths (not a high
 * priority).
 * - Todo: Labels appear to be used as types in "foo(*,*)", see example from
 * Chris (LANL, 4/19/2007).
 * - See also:
 * Example of using a SgType object
 *
 * **Data members**
 *
 * #### `int SgType::p_substitutedForTemplateParam`
 * This boolean variable marks if the current type was originally a template
 * parameter.
 * - Internal: This mechanism is not fully implemented and might be modified when it is.
 * Also, this variables type should have been bool.
 *
 * #### `SgReferenceType SgType::p_ref_to`
 * This holds the pointer to a SgReferenceType if this type is a reference to another type.
 *
 * #### `SgPointerType SgType::p_ptr_to`
 * This holds the pointer to a SgPointerType if this type is a pointer to another type.
 *
 * #### `SgReferenceType SgType::p_modifiers`
 * This points to any SgModifierNodes if this type contains type modifiers.
 *
 * #### `SgTypedefSeq SgType::p_typedefs`
 * This points to IR node which holds a list of typedefs where the base_type if this SgType.
 * - See also: SgTypedefSeq
 *
 * **Member functions**
 *
 * #### `SgType::SgType()`
 * This is the constructor.
 * This constructor builds the SgType base class.
 * - See also:
 * Example:create an SgType object
 *
 * #### `SgType::~SgType()`
 * This is the destructor.
 * There is nothing to delete in this object.
 *
 * #### `SgType::isSgType (SgNode *s)`
 * Cast function (from derived class to SgType pointer).
 * This functions returns a SgType pointer for any input of a
 * pointer to an object derived from a SgType.
 * - Return: Returns valid pointer to SgType if input is derived from a SgLocatedNode.
 *
 * #### `SgType::isSgType (const SgNode *s)`
 * Cast function (from derived class to SgType pointer).
 * This functions returns a SgType pointer for any input of a
 * pointer to an object derived from a SgType.
 * - Return: Returns valid pointer to SgType if input is derived from a SgType node.
 */
class SgType;

/** @brief This class represents a C99 complex type.
 *
 * This type can be used in C++ codes by using either the new "_Complex" syntax or the
 * older syntax __complex__ (older syntax may be GNU specific).  Note that C++ codes
 * can alternatively use the complex classes and avoid use of this type. This is here
 * mostly to support C99 extensions to C.
 * - See also:
 * Example of using a SgTypeComplex object
 *
 * **Member functions**
 *
 * #### `SgTypeComplex::SgTypeComplex()`
 * This is the constructor.
 * This constructor builds the SgTypeComplex base class.
 * - See also:
 * Example:create an SgTypeComplex object
 *
 * #### `SgTypeComplex::~SgTypeComplex()`
 * This is the destructor.
 * There is nothing to delete in this object.
 *
 * #### `SgTypeComplex::isSgTypeComplex (SgNode *s)`
 * Cast function (from derived class to SgTypeComplex pointer).
 * This functions returns a SgTypeComplex pointer for any input of a
 * pointer to an object derived from a SgTypeComplex.
 * - Return: Returns valid pointer to SgTypeComplex if input is derived from a SgLocatedNode.
 *
 * #### `SgTypeComplex::isSgTypeComplex (const SgNode *s)`
 * Cast function (from derived class to SgTypeComplex pointer).
 * This functions returns a SgTypeComplex pointer for any input of a
 * pointer to an object derived from a SgTypeComplex.
 * - Return: Returns valid pointer to SgTypeComplex if input is derived from a SgTypeComplex node.
 */
class SgTypeComplex;

/** @brief This class represents a default type used for some IR nodes (see below).
 *
 * - Internal: This type is used internally within some IR nodes:
 * -# SgExprListExp (returned by member function get_type())
 * -# SgThisExp (returned by member function get_type() if SgThisExp::p_pobj_this is NULL)
 * -# SgVarArgStartOp (returned by member function get_type())
 * -# SgVarArgOp (returned by member function get_type())
 * -# SgVarArgEndOp (returned by member function get_type())
 * -# SgVarArgCopyOp (returned by member function get_type())
 * -# SgVarArgStartOneOperandOp (returned by member function get_type())
 * -# SgAggregateInitializer (returned by member function get_type())
 * -# SgConstructorInitializer (returned by member function get_type())
 * -# SgExpressionRoot (returned by member function get_type() if member function
 * get_operand() returns NULL valued pointer)
 * -# SgPointerDerefExp (returned by member function get_type(), if member function
 * get_operand() returns valid valued pointer or operands type is V_SgTypeDefault)
 * -# SgThrowOp (used by member function set_type())
 * - See also:
 * Example of using a SgTypeDefault object
 *
 * **Member functions**
 *
 * #### `SgTypeDefault::SgTypeDefault()`
 * This is the constructor.
 * This constructor builds the SgTypeDefault base class.
 * - See also:
 * Example:create an SgTypeDefault object
 *
 * #### `SgTypeDefault::~SgTypeDefault()`
 * This is the destructor.
 * There is nothing to delete in this object.
 *
 * #### `SgTypeDefault::isSgTypeDefault (SgNode *s)`
 * Cast function (from derived class to SgTypeDefault pointer).
 * This functions returns a SgTypeDefault pointer for any input of a
 * pointer to an object derived from a SgTypeDefault.
 * - Return: Returns valid pointer to SgTypeDefault if input is derived from a SgLocatedNode.
 *
 * #### `SgTypeDefault::isSgTypeDefault (const SgNode *s)`
 * Cast function (from derived class to SgTypeDefault pointer).
 * This functions returns a SgTypeDefault pointer for any input of a
 * pointer to an object derived from a SgTypeDefault.
 * - Return: Returns valid pointer to SgTypeDefault if input is derived from a SgTypeDefault node.
 */
class SgTypeDefault;

/** @brief This class represents a C99 complex type.
 *
 * This type can be used in only C and C99 codes, the syntax is "_Imaginary".
 * This is added to support C and C99 complex support.
 * - See also:
 * Example of using a SgTypeImaginary object
 *
 * **Member functions**
 *
 * #### `SgTypeImaginary::SgTypeImaginary()`
 * This is the constructor.
 * This constructor builds the SgTypeImaginary base class.
 * - See also:
 * Example:create an SgTypeImaginary object
 *
 * #### `SgTypeImaginary::~SgTypeImaginary()`
 * This is the destructor.
 * There is nothing to delete in this object.
 *
 * #### `SgTypeImaginary::isSgTypeImaginary (SgNode *s)`
 * Cast function (from derived class to SgTypeImaginary pointer).
 * This functions returns a SgTypeImaginary pointer for any input of a
 * pointer to an object derived from a SgTypeImaginary.
 * - Return: Returns valid pointer to SgTypeImaginary if input is derived from a SgLocatedNode.
 *
 * #### `SgTypeImaginary::isSgTypeImaginary (const SgNode *s)`
 * Cast function (from derived class to SgTypeImaginary pointer).
 * This functions returns a SgTypeImaginary pointer for any input of a
 * pointer to an object derived from a SgTypeImaginary.
 * - Return: Returns valid pointer to SgTypeImaginary if input is derived from a SgTypeImaginary node.
 */
class SgTypeImaginary;

/** @brief This class represents a string type used for SgStringVal IR node.
 *
 * - Internal: This type is only used for used for the SgStringVal IR node.
 * - See also:
 * Example of using a SgTypeString object
 *
 * **Member functions**
 *
 * #### `SgTypeString::SgTypeString()`
 * This is the constructor.
 * This constructor builds the SgTypeString base class.
 * - See also:
 * Example:create an SgTypeString object
 *
 * #### `SgTypeString::~SgTypeString()`
 * This is the destructor.
 * There is nothing to delete in this object.
 *
 * #### `SgTypeString::isSgTypeString (SgNode *s)`
 * Cast function (from derived class to SgTypeString pointer).
 * This functions returns a SgTypeString pointer for any input of a
 * pointer to an object derived from a SgTypeString.
 * - Return: Returns valid pointer to SgTypeString if input is derived from a SgLocatedNode.
 *
 * #### `SgTypeString::isSgTypeString (const SgNode *s)`
 * Cast function (from derived class to SgTypeString pointer).
 * This functions returns a SgTypeString pointer for any input of a
 * pointer to an object derived from a SgTypeString.
 * - Return: Returns valid pointer to SgTypeString if input is derived from a SgTypeString node.
 */
class SgTypeString;

/** @brief This class represents the notion of a typedef declaration.
 *
 * Typedefs define new types for use in variable declarations, function parameter
 * lists, etc.  Typically the base type is complex and the typedef name allows the
 * more complex types use to be made easier to read.
 * - Todo: There are a few data members in this field that don't appear to be used (should be
 * removed if not required):
 * - p_declaration: have not seen it be used anywhere (I think it is used when a
 * declartion is explicit in the typedef, check this out, might be part of older
 * mechanism before defining and nondefining declarations were developed to provide a
 * uniform mechanism for all declarations)
 * - p_parent_scope: this is a SgSymbol, but I don't know why it is stored explicitly.
 * - See also:
 * Example of using a SgTypedefDeclaration object
 *
 * **Data members**
 *
 * #### `SgName SgTypedefDeclaration::p_name`
 * This is the name of the newly defined type.
 * - Internal: This name carries no qualification.
 *
 * #### `SgType SgTypedefDeclaration::p_base_type`
 * This is the type being given a new name by the typedef declaration.
 * - Internal: This name can be private so using the base type directly can cause access
 * violations within generated code.
 *
 * #### `SgTypedefType SgTypedefDeclaration::p_type`
 * This is the resulting type defined by the typedef declaration.
 * This type can be used where any type can be use, declaration of variables, etc.
 * - Internal: Because of access privileges on the typedef declaration the resulting type can
 * have access restrictions.
 *
 * #### `SgDeclarationStatement SgTypedefDeclaration::p_declaration`
 * pointer to the declaration (typically a SgClassDeclaration).
 * A typedef such as "typedef struct {int __pos; int __state;} _G_fpos64_t;"
 * defines a class as part of its declaration.  p_declaration pointes to the
 * declaration in these cases.
 * - Internal: This is the class declaration in a typedef that defines a class.
 *
 * #### `SgSymbol SgTypedefDeclaration::p_parent_scope`
 * This is the type symbol of the class when it is a member type (redundent with the
 * the explicitly stored scope).
 * This pointer almost always NULL, however test2005_188.C demonstrates a non-NULL value.
 * - Internal: This is redundant with the explicitly stored scope.
 * - Todo: We can remove this.
 * - Deprecated: This should be removed, but and get_scope used instead.
 *
 * #### `bool SgTypedefDeclaration::p_typedefBaseTypeContainsDefiningDeclaration`
 * This flag indicates that the typedef defines a structure
 * typedefExample.C
 * This example show the definition of a struct within the typedef.
 * - Internal: I would like to give this a better name.
 *
 * #### `SgScopeStatement SgTypedefDeclaration::p_scope`
 * This is the scope of the typedef declaration.
 * - Internal: Comment on why we need to store the scope explicitly (resolves name
 * qualification issues).
 *
 * **Member functions**
 *
 * #### `SgTypedefDeclaration::SgTypedefDeclaration ( Sg_File_Info* startOfConstruct = NULL )`
 * This is the constructor.
 * This constructor builds the SgTypedefDeclaration base class.
 * - Param `startOfConstruct`: represents the position in the source code
 * - See also:
 * Example:create an SgTypedefDeclaration object
 *
 * #### `SgTypedefDeclaration::~SgTypedefDeclaration()`
 * This is the destructor.
 * There is nothing to delete in this object.
 *
 * #### `SgTypedefDeclaration::isSgTypedefDeclaration (SgNode *s)`
 * Cast function (from derived class to SgTypedefDeclaration pointer).
 * This functions returns a SgTypedefDeclaration pointer for any input of a
 * pointer to an object derived from a SgTypedefDeclaration.
 * - Return: Returns valid pointer to SgTypedefDeclaration if input is derived from a SgLocatedNode.
 *
 * #### `SgTypedefDeclaration::isSgTypedefDeclaration (const SgNode *s)`
 * Cast function (from derived class to SgTypedefDeclaration pointer).
 * This functions returns a SgTypedefDeclaration pointer for any input of a
 * pointer to an object derived from a SgTypedefDeclaration.
 * - Return: Returns valid pointer to SgTypedefDeclaration if input is derived from a SgLocatedNode.
 *
 * #### `SgTypedefDeclaration::get_scope() const`
 * Returns scope of current statement.
 * This functions returns a pointer to the inner most SgScopeStatement where the current
 * statement is located.
 * This is a function called after the parents have been set.  It is not called by the
 * EDG/SAGE interface and is only called after the SAGE AST has been built and the parent
 * pointers set within a separate phase.
 * Improtant exception: The SgFunctionParameterList should have as it's scope the
 * SgFunctionDefinition, but it is a sibling of the SgFunctionDefinition (both the
 * SgFunctionParameterList and the SgFunctionDefinition have the SgFunctionDeclaration
 * as a parent.  Thus the scope of the SgFunctionParameterList is computed to be the
 * SgGlobal (global scope) most often; which is an error).  So we handle this case
 * explicitly.
 * - Return: Returns valid pointer to SgScopeStatement.
 */
class SgTypedefDeclaration;

/** @brief This class represents a list of associated typedefs for the SgType IR nodes which
 *
 * reference this list.
 * - Internal: This list is not well tested yet.
 * - See also:
 * Example of using a SgTypedefSeq object
 *
 * **Data members**
 *
 * #### `SgTypePtrList SgTypedefSeq::p_typedefs`
 * This holds the STL list of pointers to SgTypes.
 * - Todo: It might be better for this to be a list of SgTypedefTypes
 * - Todo: Think about if we could also store a reference to all pointers and reference types
 * where they share a common base_type.
 *
 * **Member functions**
 *
 * #### `SgTypedefSeq::SgTypedefSeq()`
 * This is the constructor.
 * This constructor builds the SgTypedefSeq base class.
 * - See also:
 * Example:create an SgTypedefSeq object
 *
 * #### `SgTypedefSeq::~SgTypedefSeq()`
 * This is the destructor.
 * There is nothing to delete in this object.
 *
 * #### `SgTypedefSeq::isSgTypedefSeq (SgNode *s)`
 * Cast function (from derived class to SgTypedefSeq pointer).
 * This functions returns a SgTypedefSeq pointer for any input of a
 * pointer to an object derived from a SgTypedefSeq.
 * - Return: Returns valid pointer to SgTypedefSeq if input is derived from a SgSupport.
 *
 * #### `SgTypedefSeq::isSgTypedefSeq (const SgNode *s)`
 * Cast function (from derived class to SgTypedefSeq pointer).
 * This functions returns a SgTypedefSeq pointer for any input of a
 * pointer to an object derived from a SgTypedefSeq.
 * - Return: Returns valid pointer to SgTypedefSeq if input is derived from a SgSupport.
 */
class SgTypedefSeq;

/** @brief This class represents the notion of a unary operator.
 *
 * It is derived from a SgExpression because operators are expressions.
 * There are no uses of this IR node anywhere.  All expressions
 * are derived from this IR node to build derived classes.  Example
 * unary operators include unary minus, unary plus, the address operator,
 * etc.
 * - Internal: This is a base class for all unary operators.
 * - See also:
 * Example of using a SgUnaryOp object
 *
 * **Data members**
 *
 * #### `SgUnaryOp::p_operand_i`
 * This is the operand associated with the unary operator.
 * Every unary operator is applied to a single operand, this
 * variable stores the operand to which the unary operator is applied.
 *
 * #### `SgUnaryOp::p_expression_type`
 * This SgType is the type of the operator (function type).
 * The type is now computed where possible (in all cases except
 * for SgCastExp). This allows us to save space and avoid having
 * explicitly stored values be unset or set incorrectly.
 * - Deprecated: This is no longer used (except for SgCastExp).
 * - Todo: This value is only used for the SgCastExp, we will
 * move it to that IR node when we are ready to change the interface
 * for the SgExpressions (and SgUnaryExp IR nodes).
 *
 * #### `SgUnaryOp::p_mode`
 * This SgType is the type of the operator (function type).
 * This variable records the prefix vs. postfix semantics of the operator
 * since the syntax of "operator++" cannot readily do so.  This approach is
 * simpiler to interogate than the C++ syntax for distingishing prefix vs.
 * postfix.
 *
 * **Member functions**
 *
 * #### `SgUnaryOp::SgUnaryOp ( Sg_File_Info *startOfConstruct=NULL, SgExpression *operand_i=NULL, SgType *expression_type=NULL)`
 * This is the constructor.
 * This constructor builds the SgUnaryOp base class.
 * - Param `startOfConstruct`: represents the position in the source code
 * - Param `operand`: represents the operand to which the operator is applied
 * - Param `expression_type`: represents the type of the return value of the operator
 * - See also:
 * Example:create an SgUnaryOp object
 *
 * #### `SgUnaryOp::~SgUnaryOp()`
 * This is the destructor.
 * Only the Sg_File_Info object can be deleted in this object.
 *
 * #### `SgUnaryOp::isSgUnaryOp (SgNode *s)`
 * Cast function (from derived class to SgUnaryOp pointer).
 * This functions returns a SgUnaryOp pointer for any input of a
 * pointer to an object derived from a SgUnaryOp.
 * - Return: Returns valid pointer to SgUnaryOp if input is derived from a SgUnaryOp.
 *
 * #### `SgUnaryOp::isSgUnaryOp (const SgNode *s)`
 * Cast function (from derived class to SgUnaryOp pointer).
 * This functions returns a SgUnaryOp pointer for any input of a
 * pointer to an object derived from a SgUnaryOp.
 * - Return: Returns valid pointer to SgUnaryOp if input is derived from a SgUnaryOp.
 *
 * #### `SgUnaryOp::get_operand() const`
 * returns SgExpression pointer to the operand associated with this unary operator.
 * - Return: Returns SgExpression pointer.
 *
 * #### `SgUnaryOp::set_operand (SgExpression* operand)`
 * This function allows the p_operand pointer to be set (used internally).
 * This function is mostly used internally and is only required to support editing
 * of existing SgUnaryOp objects.
 * - Param `operand`: - sets value of internal p_operand pointer.
 * - Return: Returns void.
 *
 * #### `SgUnaryOp::get_expression_type (void) const`
 * returns type of operator expression.
 * This function returns the type of the unary operator.
 * - Return: Returns type of operator expression.
 *
 * #### `SgUnaryOp::set_expression_type (SgType* expression_type)`
 * This function allows the p_expression_type pointer to be set (used internally).
 * This function is mostly used internally and is only required to support editing
 * of existing SgUnaryOp objects. In general it is not changed once it is set.
 * - Param `expression_type`: - sets value of internal p_expression_type pointer.
 * - Return: Returns void.
 *
 * #### `SgUnaryOp::get_mode() const`
 * Get the prefix/postfix mode of the operator.
 * Get the prefix/postfix mode of the operator (if applicable to that operator).
 * - Return: Returns SgUnaryOp::Sgop_mode (enum for prefix,postfix values)
 *
 * #### `SgUnaryOp::set_mode ( SgUnaryOp::Sgop_mode mode )`
 * Set the mode (prefix/postfix) associated with this operator
 * This is an internal function, it sets up the prefix/postfix mode of the
 * unary operator.
 * - Param `mode`: - sets value of intermal SgUnaryOp::Sgop_mode mode variable
 * - Return: Returns void
 *
 * #### `SgUnaryOp::length() const`
 * Returns number of operands (virtual function)
 * Returns number of operands (all unary operators return value = 1).
 * This function is not used and is not a defined part a minumal interface
 * for Sage III.
 * - Deprecated: This function is not used.
 * - Return: Returns int
 *
 * #### `SgUnaryOp::empty () const`
 * Returns true if number of operands is zero, else false.
 * This function returns boolean value given by (length() == 0).
 * This function is not used and is not a defined part a minumal interface
 * for Sage III.
 * - Deprecated: This function is not used.
 * - Return: Returns bool
 *
 * #### `SgUnaryOp::get_next(int &n) const`
 * Returns next operand (virtual function)
 * This function returns the next operand and is part of an
 * older iterator interface within Sage II which didn't use STL, but implemented
 * iterators for the operands (since their is only one for a unary operator
 * and two for a binary operator we don't support such an iterator interface
 * within Sage III.  The goal of Sage III is a minimal easily maintained
 * interface.  This function is not used and is not a defined part a minumal
 * interface for Sage III.
 * - Deprecated: This function is not used.
 * - Return: Returns int
 *
 * #### `SgUnaryOp::get_operand_i() const`
 * returns SgExpression pointer to the operand associated with this unary operator.
 * - Deprecated: This function is not used.
 * - Return: Returns SgExpression pointer.
 *
 * #### `SgUnaryOp::set_operand_i (SgExpression* operand)`
 * This function allows the p_operand_i pointer to be set (used internally).
 * This function is mostly used internally and is only required to support editing
 * of existing SgUnaryOp objects.
 * - Deprecated: This function is not used.
 * - Param `operand`: - sets value of internal p_operand pointer.
 * - Return: Returns void.
 */
class SgUnaryOp;

/** @brief This class represents the concept of a C++ using declaration.
 *
 * The using declaration permits named declaration or declarations within the named
 * namespace to be used in the scope of the using declaration without name qualification.
 * - See also:
 * Example of using a SgUsingDeclarationStatement object
 *
 * **Data members**
 *
 * #### `SgUsingDeclarationStatement::p_declaration`
 * This pointer points to a SgDeclarationStatement whose declaration(s) are available
 * for use in the scope containing this using declaration.
 * - Internal: this is a valid pointer if the IR nodes specified using a declaration (else
 * p_initializedName is valid).  Only one of p_declaration and p_initializedName are
 * valid pointers.
 *
 * #### `SgUsingDeclarationStatement::p_initializedName`
 * This pointer points to a SgDeclarationStatement whose declaration(s) are available
 * for use in the scope containing this using declaration.
 *
 * **Member functions**
 *
 * #### `SgUsingDeclarationStatement::SgUsingDeclarationStatement ( Sg_File_Info* startOfConstruct = NULL )`
 * This is the constructor.
 * This constructor builds the SgUsingDeclarationStatement base class.
 * - Param `startOfConstruct`: represents the position in the source code
 *
 * #### `SgUsingDeclarationStatement::~SgUsingDeclarationStatement()`
 * This is the destructor.
 * There are a lot of things to delete, but nothing is deleted in this destructor.
 *
 * #### `SgUsingDeclarationStatement::isSgUsingDeclarationStatement (SgNode *s)`
 * Cast function (from derived class to SgUsingDeclarationStatement pointer).
 * This functions returns a SgUsingDeclarationStatement pointer for any input of a
 * pointer to an object derived from a SgUsingDeclarationStatement.
 * - Return: Returns valid pointer to SgUsingDeclarationStatement if input is derived from a SgLocatedNode.
 *
 * #### `SgUsingDeclarationStatement::isSgUsingDeclarationStatement (const SgNode *s)`
 * Cast function (from derived class to SgUsingDeclarationStatement pointer).
 * This functions returns a SgUsingDeclarationStatement pointer for any input of a
 * pointer to an object derived from a SgUsingDeclarationStatement.
 * - Return: Returns valid pointer to SgUsingDeclarationStatement if input is derived from a SgLocatedNode.
 *
 * #### `SgUsingDeclarationStatement::get_declaration() const`
 * Access function for p_declaration.
 * This is a valid pointer if the using declaration references
 * a declaration, else it is null and p_initializedName is a valid pointer.
 * One or the other is a valid pointer, but not both.
 * - Return: Returns pointer to SgDeclaration.
 *
 * #### `SgUsingDeclarationStatement::set_declaration(SgDeclarationStatement* declaration)`
 * Access function for p_declaration.
 * - Return: Returns void.
 *
 * #### `SgUsingDeclarationStatement::get_initializedName() const`
 * Access function for p_initializedName.
 * This is a valid pointer if the using declaration references
 * a variable name, else it is null and p_declaration is a valid pointer.
 * One or the other is a valid pointer, but not both.
 * - Return: Returns pointer to SgInitializedName.
 *
 * #### `SgUsingDeclarationStatement::set_initializedName(SgInitializedName* initializedName)`
 * Access function for p_initializedName.
 * - Return: Returns void.
 */
class SgUsingDeclarationStatement;

/** @brief This class represents the concept of a C++ using directive.
 *
 * - Todo: Explain difference between using declaration and using directive.
 * - See also:
 * Example of using a SgUsingDirectiveStatement object
 *
 * **Data members**
 *
 * #### `SgUsingDirectiveStatement::p_namespaceDeclaration`
 * This pointer points to namespace declaration being used.
 *
 * **Member functions**
 *
 * #### `SgUsingDirectiveStatement::SgUsingDirectiveStatement ( Sg_File_Info* startOfConstruct = NULL )`
 * This is the constructor.
 * This constructor builds the SgUsingDirectiveStatement base class.
 * - Param `startOfConstruct`: represents the position in the source code
 *
 * #### `SgUsingDirectiveStatement::~SgUsingDirectiveStatement()`
 * This is the destructor.
 * There are a lot of things to delete, but nothing is deleted in this destructor.
 *
 * #### `SgUsingDirectiveStatement::isSgUsingDirectiveStatement (SgNode *s)`
 * Cast function (from derived class to SgUsingDirectiveStatement pointer).
 * This functions returns a SgUsingDirectiveStatement pointer for any input of a
 * pointer to an object derived from a SgUsingDirectiveStatement.
 * - Return: Returns valid pointer to SgUsingDirectiveStatement if input is derived from a SgLocatedNode.
 *
 * #### `SgUsingDirectiveStatement::isSgUsingDirectiveStatement (const SgNode *s)`
 * Cast function (from derived class to SgUsingDirectiveStatement pointer).
 * This functions returns a SgUsingDirectiveStatement pointer for any input of a
 * pointer to an object derived from a SgUsingDirectiveStatement.
 * - Return: Returns valid pointer to SgUsingDirectiveStatement if input is derived from a SgLocatedNode.
 *
 * #### `SgUsingDirectiveStatement::get_namespaceDeclaration() const`
 * Access function for p_namespaceDeclaration.
 * - Return: Returns pointer to SgNamespaceDeclarationStatement.
 *
 * #### `SgUsingDirectiveStatement::set_namespaceDeclaration(SgNamespaceDeclarationStatement* namespaceDeclaration)`
 * Access function for p_namespaceDeclaration.
 * - Return: Returns void.
 */
class SgUsingDirectiveStatement;

/** @brief This class represents the notion of an value (expression value).
 *
 * - Internal: This is a base class for all value expressions.
 * - Todo: We should add a SgComplexValue IR nodes for C99 support of complex constants.
 * At the moment these are internally represented as SgFloatValue, SgDoubleValue,
 * or SgLongDoubleValue IR nodes.
 * - See also:
 * Example of using a SgValueExp object
 *
 * **Data members**
 *
 * #### `SgValueExp::p_valueExpressionTree`
 * This is the root of the expression tree to which the front-end applied constant
 * folding.
 * left hand side value (lvalue).
 *
 * **Member functions**
 *
 * #### `SgValueExp::SgValueExp ( Sg_File_Info* startOfConstruct = NULL )`
 * This is the constructor.
 * This constructor builds the SgValueExp base class.
 * - Param `startOfConstruct`: represents the position in the source code
 * - See also:
 * Example:create an SgValueExp object
 *
 * #### `SgValueExp::~SgValueExp()`
 * This is the destructor.
 * Only the Sg_File_Info object can be deleted in this object.
 */
class SgValueExp;

/** @brief This class represents the variable refernece in expressions.
 *
 * - Todo: Test to verify that each variable reference is associated with the inner
 * most scoped variable with that name, except where name qualified.  Applies most easily
 * to local variables.  The same test could be used for function references, actually
 * all references.
 * - Todo: Make sure that declarations appear before variable references.
 * - See also:
 * Example of using a SgVarRefExp object
 *
 * **Data members**
 *
 * #### `SgVarRefExp::p_lvalue`
 * This boolean variable marks the current expression as a
 * left hand side value (lvalue).
 *
 * #### `SgVarRefExp::p_need_paren`
 * This boolean value marks the current expression as requiring parenthises.
 * This boolean value marks the current expression as requiring parenthises (the
 * information comes from the frontend's interpretation of the requirement and is
 * almost always overly conservative.  The unparser currently backs out more
 * accurate rules based on operator precedence and removed then where they
 * are not truely required.  Thus the purpose of this variable is to capture the
 * interpritation of the frontend regarding the use of parenthesis.
 *
 * **Member functions**
 *
 * #### `SgVarRefExp::SgVarRefExp ( Sg_File_Info* startOfConstruct = NULL )`
 * This is the constructor.
 * This constructor builds the SgVarRefExp base class.
 * - Param `startOfConstruct`: represents the position in the source code
 * - See also:
 * Example:create an SgVarRefExp object
 *
 * #### `SgVarRefExp::~SgVarRefExp()`
 * This is the destructor.
 * Only the Sg_File_Info object can be deleted in this object.
 *
 * #### `SgVarRefExp::isSgVarRefExp (SgNode *s)`
 * Cast function (from derived class to SgVarRefExp pointer).
 * This functions returns a SgVarRefExp pointer for any input of a
 * pointer to an object derived from a SgVarRefExp.
 * - Return: Returns valid pointer to SgVarRefExp if input is derived from a SgVarRefExp.
 *
 * #### `SgVarRefExp::isSgVarRefExp (const SgNode *s)`
 * Cast function (from derived class to SgVarRefExp pointer).
 * This functions returns a SgVarRefExp pointer for any input of a
 * pointer to an object derived from a SgVarRefExp.
 * - Return: Returns valid pointer to SgVarRefExp if input is derived from a SgVarRefExp.
 *
 * #### `SgVarRefExp::get_type() const`
 * Get the type associated with this expression
 * Note that the return value is either:
 * -# SgFunctionType : normal function call
 * -# SgMemberFunctionType : normal member function call
 * -# SgTypedefType : in teh case of a function call from a pointer
 * It should always be a vailid pointer.  These details are verified in the AST
 * Consistancy Tests.
 * - Return: Returns SgType (but not any SgType).
 *
 * #### `SgVarRefExp::set_type()`
 * Set the type associated with this expression
 * This is an internally called function, it sets up the type of the expression
 * based upon the types of the subexpressions (if any). Thus it takes no
 * arguments.
 * - Return: Returns void
 */
class SgVarRefExp;

/** @brief This class represents the concept of a C or C++ variable declaration.
 *
 * A variable declaration can be either a forward declaration (specified as "extern")
 * or a defining declaration (typical case).  Because of this separation (as with classes,
 * functions, etc.) the scope of a variable must be stored explicitly because name
 * qualification can associate a definition in a scope different from the original
 * declaration (see test2005_34.C for examples).
 * - Internal: Since multiple variables may be declared in a single variable declaration,
 * the scope information is held in the SgInitializedName object directly, and not the
 * SgVariableDeclaration.  The scope of a SgVariableDeclaration is ...
 * - Todo: Finish explaination of variable declaration, relationship to variable definition,
 * and the scope issue.
 * - Todo: template static variable declaration are instantiated and this is at least sometimes
 * an error (at least when not part of a transformation). See test2005_69.C for example
 * of this problem.
 * - See also:
 * Example of using a SgVariableDeclaration object
 *
 * **Data members**
 *
 * #### `SgVariableDeclaration::p_baseTypeDefiningDeclaration`
 * This is used to traverse type definitions within variable declarations.
 * In cases where a class or other named type is defined within (and as the
 * base type of) a variable declaration, the traversal code must be able to
 * traverse this definition. The traversal code computes the appropriate value
 * for this pointer when needed.
 *
 * #### `SgVariableDeclaration::p_variables`
 * This is an STL list of pointers to SgInitializedName objects.
 * Each variable is a SgInitializedName object, their can be a list of
 * then, so this list holds that collection of variables.
 *
 * #### `SgVariableDeclaration::p_variableDeclarationContainsBaseTypeDefiningDeclaration`
 * This bool records if the variable declaration has the explicit defining declaration
 * associated with its type.
 * Since types are shared, we can't store such information in the type (else each
 * reference to the type would trigger the output of the full definition of the type).
 * The value of this variable is most typically false.
 * - Todo: Provide an example of where p_variableDeclarationContainsBaseTypeDefiningDeclaration
 * is true and where it is false.
 *
 * #### `SgVariableDeclaration::p_specialization`
 * This is part of template support (variables of templated types).
 * This is most often set to SgDeclarationStatement::e_no_specialization, but where
 * templates are involved can be set to either SgDeclarationStatement::e_specialization or
 * SgDeclarationStatement::e_partial_specialization.
 *
 * **Member functions**
 *
 * #### `SgVariableDeclaration::SgVariableDeclaration ( Sg_File_Info* startOfConstruct = NULL )`
 * This is the constructor.
 * This constructor builds the SgVariableDeclaration base class.
 * - Param `startOfConstruct`: represents the position in the source code
 *
 * #### `SgVariableDeclaration::~SgVariableDeclaration()`
 * This is the destructor.
 * There are a lot of things to delete, but nothing is deleted in this destructor.
 *
 * #### `SgVariableDeclaration::isSgVariableDeclaration (SgNode *s)`
 * Cast function (from derived class to SgVariableDeclaration pointer).
 * This functions returns a SgVariableDeclaration pointer for any input of a
 * pointer to an object derived from a SgVariableDeclaration.
 * - Return: Returns valid pointer to SgVariableDeclaration if input is derived from a SgLocatedNode.
 *
 * #### `SgVariableDeclaration::isSgVariableDeclaration (const SgNode *s)`
 * Cast function (from derived class to SgVariableDeclaration pointer).
 * This functions returns a SgVariableDeclaration pointer for any input of a
 * pointer to an object derived from a SgVariableDeclaration.
 * - Return: Returns valid pointer to SgVariableDeclaration if input is derived from a SgLocatedNode.
 *
 * #### `SgVariableDeclaration::get_variables() const`
 * Access function for p_variables.
 * - Return: Returns a const reference to SgInitializedNamePtrList.
 *
 * #### `SgVariableDeclaration::get_variables()`
 * Access function for p_variables.
 * - Return: Returns a non-const reference to SgInitializedNamePtrList.
 *
 * #### `SgVariableDeclaration::get_variableDeclarationContainsBaseTypeDefiningDeclaration() const`
 * Access function for p_variableDeclarationContainsBaseTypeDefiningDeclaration.
 * - Return: Returns bool.
 *
 * #### `SgVariableDeclaration::set_variableDeclarationContainsBaseTypeDefiningDeclaration(bool variableDeclarationContainsBaseTypeDefiningDeclaration)`
 * Access function for p_variableDeclarationContainsBaseTypeDefiningDeclaration.
 * - Return: Returns void.
 *
 * #### `SgVariableDeclaration::get_specialization() const`
 * Access function for p_specialization.
 * - Return: Returns value of type SgDeclarationStatement::template_specialization_enum.
 *
 * #### `SgVariableDeclaration::set_specialization(SgDeclarationStatement::template_specialization_enum specialization)`
 * Access function for p_specialization.
 * - Return: Returns void.
 */
class SgVariableDeclaration;

/** @brief This class represents the definition (initialization) of a variable.
 *
 * The most common use of the SgVariableDefinition is to store the associated bitfield
 * specifier in declarations such as "int x:4;" which is a four bit integer variable.
 * The range of the value should be (1,32) for 32 bit machines and (1,64) for 64 bit
 * machines.  The SgVariableDefinition is also used as a supporting IR node for variable
 * declarations using the "extern" keyword.
 * The variable definition is separate from the variable declaration when the declaration
 * is using the "extern" keyword.  Thus "extern int i;" is a SgVariableDeclaration, while
 * a subsequent "int i;" or "int i = 0;" would be the variable definition
 * (SgVariableDefinition). Without the "extern" keyword, the two concepts
 * are represented using the single SgVariableDeclaration IR node.
 * - Note: Both SgVariableDeclaration and SgVariableDefinition are both derived from
 * SgDeclaration.  This is because a SgVariableDefinition does not represent a
 * scope like all other definitions (which are derived from SgScopeStatement).
 * - Todo: If a SgVariableDefinition is built internally as part of a SgVariableDeclaration
 * it should be marked as compiler generated if the "extern" keyword was not used in
 * the SgVariableDeclaration.  This needs to be looked into.
 * - Todo: Constant folding happens when the bitfield is a variable and the variable name is
 * lost.  This IR nodes needs to be modified to alternatively store the associated
 * SgExpression (in case it is a root of an expression tree).
 * - See also:
 * Example of using a SgVariableDefinition object
 *
 * **Data members**
 *
 * #### `SgVariableDefinition::p_vardefn`
 * This pointer points to associated SgInitializedName object (the variable).
 *
 * #### `SgVariableDefinition::p_bitfield`
 * This a pointer to a value specifies the bitwidth of the variable (used to control
 * memory layout/padding of data members in data structures).
 * This is a somewhat rare feature of C, and so C++. variables of some types can be
 * specified to have a specific bit width (usually a number of bits smaller than the
 * default size).
 * - Internal: I'm not sure that this has to be a SgUnsignedLongVal, since the value is likely
 * always less than the word length; 32 (typicaly), 64 (common), 128 (in the future).
 * Currently we use the null value of the pointer to indicate when the bitfield is
 * to be relavant; a better implementation would be to have a default value or
 * a second bool variable to indicate clealy when the bitfield is relavant.
 * a pointer to a integer value seems a poor design.
 *
 * **Member functions**
 *
 * #### `SgVariableDefinition::SgVariableDefinition ( Sg_File_Info* startOfConstruct = NULL )`
 * This is the constructor.
 * This constructor builds the SgVariableDefinition base class.
 * - Param `startOfConstruct`: represents the position in the source code
 *
 * #### `SgVariableDefinition::~SgVariableDefinition()`
 * This is the destructor.
 * There are a lot of things to delete, but nothing is deleted in this destructor.
 *
 * #### `SgVariableDefinition::isSgVariableDefinition (SgNode *s)`
 * Cast function (from derived class to SgVariableDefinition pointer).
 * This functions returns a SgVariableDefinition pointer for any input of a
 * pointer to an object derived from a SgVariableDefinition.
 * - Return: Returns valid pointer to SgVariableDefinition if input is derived from a SgLocatedNode.
 *
 * #### `SgVariableDefinition::isSgVariableDefinition (const SgNode *s)`
 * Cast function (from derived class to SgVariableDefinition pointer).
 * This functions returns a SgVariableDefinition pointer for any input of a
 * pointer to an object derived from a SgVariableDefinition.
 * - Return: Returns valid pointer to SgVariableDefinition if input is derived from a SgLocatedNode.
 *
 * #### `SgVariableDefinition::get_vardefn() const`
 * Access function for SgInitializedName stored in p_vardefn.
 * - Return: Returns pointer to SgInitializedName.
 *
 * #### `SgVariableDefinition::set_vardefn(SgInitializedName* expr)`
 * Access function for SgInitializedName in p_vardefn.
 * - Return: Returns void.
 *
 * #### `SgVariableDefinition::get_bitfield() const`
 * Access function for p_bitfield.
 * - Return: Returns pointer to SgUnsignedLongVal.
 *
 * #### `SgVariableDefinition::set_bitfield(SgUnsignedLongVal* bitfield)`
 * Access function for p_bitfield.
 * - Return: Returns void.
 */
class SgVariableDefinition;

/** @brief This class represents the concept of a variable name within the compiler (a
 *
 * shared container for the declaration of a variable (SgInitializedName)).
 * Symbols are a simpler way for the compiler to quickly associate types,
 * declarations and names.  This symbol is specific to holding a variable name.
 * - Internal: Symbols are placed into scopes (more precisely into symbol tables held in
 * each SgScopeStatement).  Most symbols are only in a single symbol table, but the
 * SgVariableSymbol can exist in two scopes if it is a static data member declared both
 * in its class (SgClassDefinition) and outside of the class (in global scope or a
 * namespace scope (SgGlobal or SgNamespaceDefinitionStatement)).
 * - Todo: Need to figure out if it is such a great idea of a single symbol to be in two scopes
 * or if it would be better to use two different symbols (since there are two different
 * SgInitializedName object built (the last one referencing the previous one through
 * the p_prev_decl_item pointer)).
 * - Todo: The get_type() function can return NULL when the get_definition() is NULL.  I think
 * we should have assertiosn to make sure that get_definition is a valid pointer and that
 * get_type() should not return NULL.
 * - See also: SgInitializedName
 * - See also:
 * Example of using a SgVariableSymbol object
 *
 * **Member functions**
 *
 * #### `SgVariableSymbol::SgVariableSymbol( SgClassDeclaration* declaration = NULL )`
 * This is the only constructor.
 * This constructor builds the SgVariableSymbol base class.
 * - See also:
 * Example:create an SgVariableSymbol object
 *
 * #### `SgVariableSymbol::~SgVariableSymbol()`
 * This is the destructor.
 *
 * #### `SgVariableSymbol::get_name() const`
 * Access function for getting name from declarations or types internally.
 * - Internal: This is a virtual function.
 * - Return: Returns SgName.
 *
 * #### `SgVariableSymbol::get_type() const`
 * This function returns the type associated with the named entity.
 * - Internal: This is a virtual function.
 * - Return: Returns SgType*.
 *
 * #### `SgVariableSymbol::isSgVariableSymbol (SgNode *s)`
 * Cast function (from derived class to SgVariableSymbol pointer).
 * This functions returns a SgVariableSymbol pointer for any input of a
 * pointer to an object derived from a SgVariableSymbol.
 * - Return: Returns valid pointer to SgVariableSymbol if input is derived from a SgVariableSymbol.
 *
 * #### `SgVariableSymbol::isSgVariableSymbol (const SgNode *s)`
 * Cast function (from derived class to SgVariableSymbol pointer).
 * This functions returns a SgVariableSymbol pointer for any input of a
 * pointer to an object derived from a SgVariableSymbol.
 * - Return: Returns valid pointer to SgVariableSymbol if input is derived from a SgVariableSymbol.
 */
class SgVariableSymbol;

/** @brief This class represents the concept of a do-while statement.
 *
 * - Internal:
 *
 * **Data members**
 *
 * #### `SgWhileStmt::p_condition`
 * This pointer a SgStatement, the conditional expression in the loop construct.
 *
 * #### `SgWhileStmt::p_body`
 * This pointer a SgBasicBlock, and holds the statements in the body of the loop.
 *
 * #### `SgWhileStmt::p_else_body`
 * This pointer to an SgStatement holds the body of the 'else' block.
 * This member is intended for use with Python, and should be NULL otherwise.
 *
 * **Member functions**
 *
 * #### `SgWhileStmt::SgWhileStmt ( Sg_File_Info* startOfConstruct = NULL )`
 * This is the constructor.
 * This constructor builds the SgWhileStmt base class.
 * - Param `startOfConstruct`: represents the position in the source code
 *
 * #### `SgWhileStmt::~SgWhileStmt()`
 * This is the destructor.
 * There are a lot of things to delete, but nothing is deleted in this destructor.
 *
 * #### `SgWhileStmt::isSgWhileStmt (SgNode *s)`
 * Cast function (from derived class to SgWhileStmt pointer).
 * This functions returns a SgWhileStmt pointer for any input of a
 * pointer to an object derived from a SgWhileStmt.
 * - Return: Returns valid pointer to SgWhileStmt if input is derived from a SgLocatedNode.
 *
 * #### `SgWhileStmt::isSgWhileStmt (const SgNode *s)`
 * Cast function (from derived class to SgWhileStmt pointer).
 * This functions returns a SgWhileStmt pointer for any input of a
 * pointer to an object derived from a SgWhileStmt.
 * - Return: Returns valid pointer to SgWhileStmt if input is derived from a SgLocatedNode.
 *
 * #### `SgWhileStmt::copy(const SgCopyHelp & help)`
 * Makes a copy (deap of shallow depending on SgCopyHelp).
 * - Return: Returns pointer to copy of SgWhileStmt.
 *
 * #### `SgWhileStmt::get_body() const`
 * Access function for p_body.
 * - Return: Returns a pointer to a SgBasicBlock.
 *
 * #### `SgWhileStmt::set_body(SgBasicBlock* body)`
 * Access function for p_body.
 * - Param `body`: SgBasicBlock pointer
 * - Return: Returns void.
 *
 * #### `SgWhileStmt::get_condition() const`
 * Access function for p_condition.
 * - Return: Returns a pointer to a SgStatement.
 *
 * #### `SgWhileStmt::set_condition(SgStatement* condition)`
 * Access function for p_condition.
 * - Param `condition`: SgStatement pointer
 * - Return: Returns void.
 */
class SgWhileStmt;

#endif
