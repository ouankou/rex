// -*- c++ -*-

#ifndef ROSE_DOCS_ATTRIBUTES_H
#define ROSE_DOCS_ATTRIBUTES_H

/** @brief Working with attributes
 *
 *  Attaching user-defined attributes to objects.
 *
 *  Many ROSE classes allow users to define and store their own data in the form of @em attributes. An attribute is a
 *  name/value pair where the name uniquely identifies the attribute within the container object and the value has a
 *  user-defined type.
 *
 *  ROSE supports three interfaces for attributes:
 *
 *  @li The Sage IR nodes derived from @ref SgNode have a built-in interface for storing heap-allocated attributes. The
 *      applicable methods all have "attribute" as part of their names. This interface is built upon @ref
 *      AstAttributeMechanism. This interface provides a way for the user to make multiple passes over the AST and save state
 *      information into the AST for subsequent traversals. The mechanism is different from creation of inherited and
 *      synthesized attributes within the AST processing mechanism (@ref AstProcessingClasses) since those attributes are
 *      allocated and deallocated automatically on the stack.
 *  @li The @ref AstAttributeMechanism provides a mechanism by which heap-allocated attributes derived from @ref AstAttribute
 *      can be stored in an object. Such attributes are copied (by reallocating) whenever their container is copied so that
 *      they are never shared between two containers. The AstAttributeMechanism container owns these attributes and deletes
 *      them when the container is destroyed.
 *  @li A value-based attribute interface provides a mechanism for storing values that are instances of POD types and
 *      3rd-party types that the user cannot edit. It stores attributes by value and uses the value's normal C++
 *      constructors and destructor.
 *
 *  Although there are three interfaces, they really all share the same basic mechanism. SgNode attributes are implemented in
 *  terms of the AstAttributeMechanism, which is implemented in terms of the value-based attribute interface.
 *
 *  @section attribute_comparison Comparison of attribute interfaces
 *
 *  **IR node attributes**
 *  - Applies only to IR nodes.
 *  - Can store multiple attributes with many different value types as long as those types all derive from AstAttribute.
 *  - Requires non-class values to be wrapped in a class derived from @ref AstAttribute.
 *  - User must be able to modify the value type so it inherits from AstAttribute, or wrap the type in a subclass of
 *    AstAttribute, adding an extra level of indirection to access the value.
 *  - No assurance that the same name is not used for two different purposes.
 *  - Requires implementation of virtual @c copy method (non-pure) if copying is intended.
 *  - Errors are not reported.
 *  - Attempting to retrieve a non-existing attribute without providing a default value returns a null attribute pointer.
 *  - Attribute value types are runtime checked. A mismatch is discovered by the user when they perform a @c dynamic_cast from
 *    the AstAttribute base type to their subclass.
 *  - Requires user to use C++ @c dynamic_cast from the AstAttribute pointer to the user's subclass pointer.
 *
 *  **AstAttributeMechanism**
 *  - Class authors can add attribute-storing capability to any class by containing an AstAttributeMechanism object.
 *  - Can store multiple attributes with many different value types as long as those types all derive from AstAttribute.
 *  - Requires non-class values to be wrapped in a class derived from @ref AstAttribute.
 *  - User must be able to modify the value type so it inherits from AstAttribute, or wrap the type in a subclass of
 *    AstAttribute, adding an extra level of indirection to access the value.
 *  - No assurance that the same name is not used for two different purposes.
 *  - Requires implementation of virtual @c copy method (non-pure) if copying is intended.
 *  - Errors are reported by return values.
 *  - Attempting to retrieve a non-existing attribute without providing a default value returns a null attribute pointer.
 *  - Attribute value types are runtime checked. A mismatch is discovered by the user when they perform a @c dynamic_cast from
 *    the AstAttribute base type to their subclass.
 *  - Requires user to use C++ @c dynamic_cast from the AstAttribute pointer to the user's subclass pointer.
 *
 *  **Value-based attributes**
 *  - Class authors can add attribute-storing capability to any class by inheriting this interface.
 *  - Can store multiple attributes with many different value types.
 *  - Can directly store non-class values.
 *  - Can store values whose type is not user-modifiable, such as STL containers.
 *  - Ensures that two users don't declare the same attribute name.
 *  - Uses normal C++ copy constructors and assignment operators for attribute values.
 *  - Errors are reported by dedicated exception types.
 *  - Attempting to retrieve a non-existing attribute without providing a default value throws a dedicated does-not-exist
 *    exception.
 *  - Attribute value types are runtime checked. A mismatch between writing and reading is reported by a dedicated
 *    wrong-query-type exception.
 *  - All casting is hidden behind the API.
 *
 *  Some examples may help illuminate the differences. The examples show three methods of using attributes:
 *
 *  @li **Method 1** uses the value-based attribute interface directly.
 *  @li **Method 2** uses the @ref AstAttributeMechanism interface.
 *  @li **Method 3** uses the @ref SgNode attribute interface.
 *
 *  Let us assume that two types exist in some library header file somewhere and the user wants to store these as attribute
 *  values in some object. The two value types are:
 *
 *  @snippet tutorial/binaryAttribute.C comparison value types
 *
 *  Let us also assume that a ROSE developer has a class and wants the user to be able to store attributes in objects of that
 *  class. The first step is for the ROSE developer to prepare his class for storing attributes:
 *
 *  @snippet tutorial/binaryAttribute.C comparison preparing storage
 *
 *  Method 1 is designed to use inheritance: all of its methods have the word "attribute" in their names. Method 2 could be
 *  used by inheritance, but is more commonly used with containment due to its short, common method names like @c size. Method
 *  3 applies only to Sage IR nodes, but creating a new subclass of SgNode is outside the scope of this document; instead,
 *  we'll just use an existing IR node type.
 *
 *  Now we jump into the user code. The user wants to be able to store two attributes, one of each value type. As mentioned
 *  above, the attribute value types are defined in some library header, and the class of objects in which to store them is
 *  defined in a ROSE header file. Method 1 can store values of any type, but the user has more work to do before he
 *  can use methods 2 or 3:
 *
 *  @snippet tutorial/binaryAttribute.C comparison attribute wrappers
 *
 *  Method 1 requires no additional wrapper code since it can store any value directly. Methods 2 and 3 both require a
 *  substantial amount of boilerplate to store even a simple enum value. The @c copy method's purpose is to allocate a new
 *  copy of an attribute when the object holding the attribute is copied or assigned. The copy method should be implemented in
 *  every @ref AstAttribute subclass, although few do. If it's not implemented then one of two things happen: either the
 *  attribute is not copied, or only a superclass of the attribute is copied. Subclasses must also implement @c
 *  attribute_class_name, although few do. Neither @c copy nor @c attribute_class_name are pure virtual because of limitations
 *  with ROSETTA code generation.
 *
 *  Next, the user will want to use descriptive strings for the attribute so error messages are informative, but shorter names
 *  in C++ code, so we declare the attribute names:
 *
 *  @snippet tutorial/binaryAttribute.C comparison declare 1
 *  @snippet tutorial/binaryAttribute.C comparison declare 2
 *  @snippet tutorial/binaryAttribute.C comparison declare 3
 *
 *  The declarations in methods 2 and 3 are identical. Method 1 differs by using an integral type for attribute IDs, which has
 *  two benefits: (1) it prevents two users from using the same attribute name for different purposes, and (2) it reduces the
 *  size and increases the speed of the underlying storage maps by storing integer keys rather than strings. Method 1 has
 *  functions that convert between identification numbers and strings if necessary (e.g., error messages).
 *
 *  Now, let us see how to insert two attributes into an object assuming that the object came from somewhere far away and we
 *  don't know whether it already contains these attributes. If it does, we want to overwrite their old values with new
 *  values. Overwriting values is likely to be a more common operation than insert-if-nonexistent. After all, languages
 *  generally don't have a dedicated assign-value-if-none-assigned operator (Perl and Bash being exceptions).
 *
 *  @snippet tutorial/binaryAttribute.C comparison insert 1
 *  @snippet tutorial/binaryAttribute.C comparison insert 2
 *  @snippet tutorial/binaryAttribute.C comparison insert 3
 *
 *  Method 1 stores the attribute directly while Methods 2 and 3 require the attribute value to be wrapped in a heap-allocated
 *  object first.
 *
 *  Eventually the user will want to retrieve an attribute's value. Users commonly need to obtain the attribute or a default
 *  value.
 *
 *  @snippet tutorial/binaryAttribute.C comparison retrieve 1
 *  @snippet tutorial/binaryAttribute.C comparison retrieve 2
 *  @snippet tutorial/binaryAttribute.C comparison retrieve 3
 *
 *  Method 1 has a couple functions dedicated to this common scenario. Methods 2 and 3 return a null pointer if the attribute
 *  doesn't exist, but require a dynamic cast to the appropriate type otherwise.
 *
 *  Sooner or later a user will want to erase an attribute. Perhaps the attribute holds the result of some optional analysis
 *  which is no longer valid. The user wants to ensure that the attribute doesn't exist, but isn't sure whether it currently
 *  exists:
 *
 *  @snippet tutorial/binaryAttribute.C comparison erase 1
 *  @snippet tutorial/binaryAttribute.C comparison erase 2
 *  @snippet tutorial/binaryAttribute.C comparison erase 3
 *
 *  If the attribute didn't exist then none of these methods do anything. If it did exist... With Method 1, the value's
 *  destructor is called. Methods 2 and 3 delete the heap-allocated value, which is allowed since the attribute container owns
 *  the object.
 *
 *  Finally, when the object containing the attributes is destroyed the user needs to be able to clean up by destroying the
 *  attributes that are attached:
 *
 *  @snippet tutorial/binaryAttribute.C comparison cleanup 1
 *  @snippet tutorial/binaryAttribute.C comparison cleanup 2
 *  @snippet tutorial/binaryAttribute.C comparison cleanup 3
 *
 *  All three interfaces now properly clean up their attributes, although this wasn't always the case with methods 2 and 3.
 *
 *  See @ref rose_midend.
 */
struct attributes {};

#endif
