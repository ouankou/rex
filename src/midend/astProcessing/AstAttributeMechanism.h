#ifndef ROSE_AstAttributeMechanism_H
#define ROSE_AstAttributeMechanism_H

#include "rosedll.h"

#include <map>

#include <set>

#include <string>

#include <vector>

class SgNode;

/** Base class for all IR node attribute values.
 *
 *  This is the base class for all attribute values stored in the Sage IR node
 * using the @ref AstAttributeMechanism. IR node attributes are polymorphic
 * based on this class, and are always allocated on the heap. Once the
 * attribute value is handed to an attribute container method the container owns
 * the value and is responsible for deleting it.  In the case of methods that
 * only optionally insert a value, the value is deleted immediately if it's not
 * inserted.  But see @ref getOwnershipPolicy for additional information.
 *
 *  The underlying mechanism is designed to store values of any type, including
 * POD, 3rd party types, and pointers. On the other hand, the @ref
 * AstAttributeMechanism interface described here stores only pointers to values
 * allocated on the heap, owns those values, and supports operations that are
 * useful specifically in IR nodes.
 *
 *  Subclasses should each implement a virtual @ref copy constructor, which
 * should allocate a new copy of the value.  The implementation in this base
 * class returns a null pointer, which means that the attribute is not copied
 * into a new AST node when its AST node is copied.  If a subclass fails to
 * implement the virtual copy constructor and a superclass has an implementation
 * that return non-null, then copying the AST node will copy only the superclass
 * part of the attribute and the new attribute will have the dynamic type of the
 * superclass--probably not what you want!
 *
 *  For a more detailed description of using attributes in ROSE (and your own
 * classes) see @ref attributes. */
class ROSE_DLL_API AstAttribute {
public:
  /** Who owns this attribute.
   *
   *  See @ref getOwnershipPolicy. */
  enum OwnershipPolicy {
    CONTAINER_OWNERSHIP, /**< Container owns attribute. New subclasses should
                            use this! */
    NO_OWNERSHIP,        /**< Attributes are always leaked. */
    CUSTOM_OWNERSHIP,    /**< Subclass defines ownership policy. */
    UNKNOWN_OWNERSHIP    /**< Default for old subclasses. */
  };

  /** Who owns this attribute.
   *
   *  @li Attributes allocated on the heap should not be leaked. For instance,
   * if an AST is deleted, then the attributes that were referenced by the AST
   * nodes should also be eventually deleted.
   *  @li The mechanism should not place undue burden on the user.  For
   * instance, if a user copies and later deletes an AST to which some analysis
   * has attached attributes, the user should not need to be concerned with
   * deleting attributes stored in the copy.
   *  @li The mechanism should be able to support either deep or shallow
   * attribute copies as appropriate for the attribute. The deep vs. shallow
   * copying policy is implemented by the virtual @ref copy method, which must
   *      coordinate with the ownership policy to ensure no leaks.
   *
   *  The four ownership policies are:
   *
   *  @li `CONTAINER_OWNERSHIP:` The simple approach to ownership, and the one
   * that we recommend for all new attribute subclasses, is that the attribute
   * container owns the attributes.  When the container is copied (e.g., as part
   * of copying an AST node) then it invokes the @ref copy methods of its
   * attributes, and when the container is deleted (e.g., as part of deleting an
   * AST node) then it explicitly deletes its attributes. This policy is an
   * "allocate and forget" approach: once the creator inserts an attribute into
   * the container it transfers/moves ownership to the container and the creator
   * never needs to invoke `delete.`  This policy also means that users never
   * need to explicitly delete attributes if they copy and then delete an AST.
   * Newly designed attribute subclasses should use this policy unless they have
   * a very good reason to use another policy.
   *
   *  @li `NO_OWNERSHIP:` Another simple approach is that ownership is
   * transfered to the operating system.  In other words, the attribute is
   * *never* deleted by the program and its memory is reclaimed only when the
   * program terminates. Attribute containers that are incorporated into objects
   * that are frequently allocated and/or copied and then deleted will result in
   * a large number of leaked attributes.  This approach is not recommended and
   * is retained for compatibility.
   *
   *  @li `CUSTOM_OWNERSHIP:` A third approach is that the attribute subclass
   * implements its own ownership policy, which ideally should have the
   * properties listed above. An attribute using this policy will never be
   * deleted by an attribute container; the class must implement some other
   * mechanism for tracking which attributes are allocated and whether they can
   * be safely deleted.
   *
   *  @li `UNKNOWN_OWNERSHIP:` This final policy is for subclasses implemented
   * before clear attribute ownership rules were defined.  Due to the ambiguity
   * in the original AstAttributeMechanism implementation and the fact that
   * attributes are used by code outside the ROSE library, this must be the
   * default implementation.
   *
   *  Regardless of the ownership policy, an attribute must not be deleted while
   * it is a member of an AstAttributeMechanism container. This is because in
   * order for the container to decide whether it should delete the attribute,
   * it must first ask the attribute for its ownership policy.  In other words,
   * the following code will likely result in a segmentation fault:
   *
   *  @code
   *   SgDirectedGraphEdge *edge = ...;
   *   delete edge->getAttribute("info");
   *   delete edge; // INVALID ACCESS TO EDGE ATTRIBUTE HERE
   *  @endcode */
  virtual OwnershipPolicy getOwnershipPolicy() const;

  /** Support for attributes to specify edges in the dot graphs. */
  class ROSE_DLL_API AttributeEdgeInfo {
  public:
    SgNode *fromNode;
    SgNode *toNode;
    std::string label;
    std::string options;

    AttributeEdgeInfo(SgNode *fromNode, SgNode *toNode, std::string label,
                      std::string options)
        : fromNode(fromNode), toNode(toNode), label(label), options(options) {}
  };

  /** Support for adding nodes to DOT graphs. */
  class ROSE_DLL_API AttributeNodeInfo {
  public:
    SgNode *nodePtr;
    std::string label;
    std::string options;

    AttributeNodeInfo(SgNode *nodePtr, std::string label, std::string options)
        : nodePtr(nodePtr), label(label), options(options) {}
  };

public:
  AstAttribute() {}
  virtual ~AstAttribute() {}

  /** Virtual copy constructor.
   *
   *  Copy-constructs a new object on the heap and returns its pointer.  All
   * subclasses must implement this in order to instantiate the correct dynamic
   * type, although many don't.  If this @ref copy method returns a null pointer
   * (like the base implementation) then the attribute is not copied as part of
   * copying its container.  E.g., an attribute stored in an AST will not be
   * copied when the AST is copied if that attribute is directly derived from
   * @ref AstAttribute and fails to implement @ref copy.  If a subclass fails to
   * implement @ref copy and inherits from a class that does implement a
   *  @ref copy that returns non-null, then the copied attribute will have an
   * incorrect dynamic type.
   *
   *  It would be nice if we could make this pure virtual, but unfortunately
   * ROSETTA-generated code fails to compile because it generates an
   * instantiation of this interface (whether or not that code is ever executed
   * is unkown). [Robb Matzke 2015-11-10] */
  virtual AstAttribute *copy() const {
    return NULL; // attribute will not be copied when the containing obj is
                 // copied
  }

  // DO NOT DOCUMENT!  The original implementation used a non-const copy
  // constructor and many subclasses that implemented a copy constructor didn't
  // use C++11's "override" word as a defensive measure. Since we don't have
  // access to all those subclasses, we must continue to support the non-const
  // version.  Subclasses should only have to implement one or the other, not
  // both.
  virtual AstAttribute *copy() {
    return const_cast<const AstAttribute *>(this)->copy();
  }

  /** Attribute class name.
   *
   *  Returns the name of the dynamic type. All subclasses must implement this
   * in order to return the correct type name, although many don't.  If a
   * subclass fails to implement this then it will return an incorrect class
   * name.
   *
   *  It would be nice if this could be pure virtual, but unfortunately
   * ROSETTA-generated code fails to compile because it generates an
   * instantiation of this interface (whether or not that code is ever executed
   * is unknown). [Robb Matzke 2015-11-10] */
  virtual std::string attribute_class_name() const {
    return "AstAttribute"; // almost certainly not the right dynamic type name!
  }

  /** Convert an attribute to a string.
   *
   *  This is used by other components to print the value of an attribute. For
   * example the pdf generation calls this function to print the value of an
   * attribute in the pdf file. The default implementation is to return the
   * attribute address in the heap as a string. */
  virtual std::string toString();

  /** DOT support.
   *
   *  @{ */
  virtual std::string additionalNodeOptions();
  virtual std::vector<AttributeEdgeInfo> additionalEdgeInfo();
  virtual std::vector<AttributeNodeInfo> additionalNodeInfo();
  /** @} */

  /** Eliminate IR nodes in DOT graphs.
   *
   *  Or to tailor the presentation of information about ASTs. */
  virtual bool commentOutNodeInGraph();
};

/** Stores named attributes in Sage IR nodes.
 *
 *  This attribute container stores one non-null, heap-allocated attribute per
 * user-specified name.  All values are derived from @ref AstAttribute.  Any
 * object can have an attribute container data member. For example, each AST
 * node (@ref SgNode) contains one attribute container accessible via
 * `SgNode::get_attributeMechanism`.
 *
 *  The value class's @ref AstAttribute::getOwnershipPolicy "getOwnershipPolicy"
 * method indicates whether the container owns the heap-allocated attribute and
 * therefore whether the container is responsible for invoking `delete` on the
 * attribute when the container is destroyed.  New attribute subclasses should
 * use the @ref AstAttribute::CONTAINER_OWNERSHIP policy if possible.
 *
 *  IR node attribute values are always on the heap. Whenever an attribute
 * container is copied, the container invokes the @ref AstAttribute::copy "copy"
 * method on all its attributes. The attributes' `copy` should either allocate a
 * new copy of the attribute or return a null pointer. Since these values must
 * be derived from @ref AstAttribute, it is not possible to directly store
 * values whose type the user cannot modify to inherit from @ref AstAttribute.
 * This includes POD types and classes defined in 3rd party libraries (e.g.,
 * `std::vector).` To store such values, the user must wrap them in another
 *  class that does inherit from @ref AstAttribute and which implements the
 * necessary virtual functions.
 *
 *  The names of attributes are strings and the container does not check whether
 * the string supplied to various container methods is spelled correctly.  Using
 * a misspelled attribute name is the same as using a different value name--in
 * effect, operating on a completely different, unintended attribute.
 *
 *  The @ref AstAttributeMechanism is used by AST nodes (@ref SgNode) and is
 * available via `SgNode::get_attributeMechanism`, although that is not the
 * preferred API.  Instead, @ref SgNode provides an additional methods that
 * contain "attribute" as part of their name. These "attribute" methods are
 * mostly just wrappers around `SgNode::get_attributeMechanism`.
 *
 *  For additional information, including examples, see @ref attributes. */
class ROSE_DLL_API AstAttributeMechanism {
  typedef std::map<std::string, AstAttribute *> attributeMap_;
  attributeMap_ attributes_;

public:
  /** Default constructor.
   *
   *  Constructs an attribute mechanism that holds no attributes. */
  AstAttributeMechanism() {}

  /** Copy constructor.
   *
   *  Copying an attribute container invokes each value's `copy` method. If it
   * returns null, the new container does not have that attribute. */
  AstAttributeMechanism(const AstAttributeMechanism &other) {
    assignFrom(other);
  }

  /** Assignment operator.
   *
   *  Assigning one attribute container to another will cause the destination
   * container's attributes to be erased and deleted, and the source container's
   * attributes to be copied.  The assignment operator is exception safe: it
   * will either successfully copy and assign all attributes or not assign any
   * attributes. However, if an attribute uses anything other than the @ref
   * AstAttribute::CONTAINER_OWNERSHIP ownership policy then it is up to the
   * attribute type's designer to handle deletion of attributes that were copied
   * before the exception occurred. */
  AstAttributeMechanism &operator=(const AstAttributeMechanism &other);

  /** Destructor.
   *
   *  Destroying the attribute container should cause unused attributes to be
   * destroyed.  If an attribute implements the @ref
   *  AstAttribute::CONTAINER_OWNERSHIP policy then the container explicitly
   * deletes the attribute, otherwise it is up to the attribute class's designer
   * to implement a deletion policy that prevents leaks. */
  ~AstAttributeMechanism();

  /** Clears all attributes from the container. */
  void clearAttributes();

  /** Test for attribute existence.
   *
   *  Test whether this container holds an attribute with the specified name.
   * This predicate returns true only if the name exists and points to a
   * non-null attribute value.  The name need not be declared in the attribute
   * system. */
  bool exists(const std::string &name) const;

  /** Insert an attribute.
   *
   *  Inserts the specified heap-allocated value for the given attribute name,
   * replacing any previous value stored for that same name.
   *
   *  If the new value uses the @ref AstAttribute::CONTAINER_OWNERSHIP policy
   * then ownership is immediately transferred/moved to this container, which
   * becomes responsible for deleting the attribute. Otherwise, the attribute's
   * designer is responsible for preventing leaks.
   *
   *  If the old value (if present) uses the @ref
   * AstAttribute::CONTAINER_OWNERSHIP policy then it is deleted. Otherwise the
   *  attribute's designer is responsible for implementing an ownership policy
   * that safely prevents leaks, and the attribute must not be deleted until
   * after this method returns. */
  void set(const std::string &name, AstAttribute *value);

  /** Insert a new value if the attribute doesn't already exist.
   *
   *  Tests whether an attribute with the specified name @ref exists and if not,
   * invokes @ref set.  See @ref set for details about ownership of the new
   * attribute. Returns true if the value was inserted. An uninserted value is
   * deleted according to its ownership policy. */
  bool add(const std::string &name, AstAttribute *value);

  /** Insert a new value if the attribute already exists.
   *
   *  Tests whether the specified attribute exists, and if so, invokes @ref set.
   * See @ref set for details about ownership of the old and new attributes.
   * Returns true if an attribute with the specified name already existed. An
   * uninserted value is deleted according to its ownership policy. */
  bool replace(const std::string &name, AstAttribute *value);

  /** Get an attribute value.
   *
   *  Returns the value associated with the given attribute, or null if the
   * attribute does not exist.  This method does not copy the attribute before
   * returning it, therefore the caller should not delete the attribute. Erasing
   * the attribute from the container may cause the pointer to become invalid,
   * depending on the attribute's ownership policy. If the attribute uses the
   * @ref AstAttribute::CONTAINER_OWNERSHIP method then the attribute can be
   * modified through its pointer without affecting attributes in other
   * containers, otherwise the behavior is up to the attribute's designer.
   *
   *  The caller will need to `dynamic_cast` the returned pointer to the
   * appropriate subclass of @ref AstAttribute. */
  AstAttribute *operator[](const std::string &name) const;

  /** Erases the specified attribute.
   *
   *  If an attribute with the specified name exists then it is removed from the
   * container. If the attribute implements the
   *  @ref AstAttribute::CONTAINER_OWNERSHIP policy then this container deletes
   * the attribute, otherwise it is up to the attribute's designer to implement
   * a safe way to prevent attribute leaks, and the attribute must not be
   * deleted until after this method returns. */
  void remove(const std::string &name);

  /** Set of attribute names. */
  typedef std::set<std::string> AttributeIdentifiers;

  /** Names of the attributes currently stored in this container.
   *
   *  Iterate over the returned set and retrieve each value with operator[].
   *  The set is a snapshot; modifying it does not change the container. */
  AttributeIdentifiers getAttributeIdentifiers() const;

  /** Number of attributes stored. */
  size_t size() const;

private:
  // Called by copy constructor and assignment.
  void assignFrom(const AstAttributeMechanism &other);
};

/** Attribute corresponding to a metric.
 *
 *  A metric attribute stores an observed or derived numeric value. The
 * attribute container holds its name separately. */
class ROSE_DLL_API MetricAttribute : public AstAttribute {
protected:
  bool is_derived_;
  double value_;

public:
  MetricAttribute() : is_derived_(false), value_(0) {}

  MetricAttribute(double value, bool is_derived = false)
      : is_derived_(is_derived), value_(value) {}

  virtual AstAttribute *copy() const override;
  virtual std::string attribute_class_name() const override;

  virtual bool isDerived() const { return is_derived_; }
  virtual double getValue() const { return value_; }
  virtual void setValue(double newVal) { value_ = newVal; }

  virtual std::string toString() override;

  MetricAttribute &operator+=(const MetricAttribute &other);
  MetricAttribute &operator-=(const MetricAttribute &other);
  MetricAttribute &operator*=(const MetricAttribute &other);
  MetricAttribute &operator/=(const MetricAttribute &other);
};

/** IR node attribute that stores a copyable value.
 *
 *  Since IR node attributes must all inherit from @ref AstAttribute we need to
 * write wrappers around POD types and 3rd party types when we want to store
 * them as IR attributes. We've ended up with a proliferation of such attributes
 * that all do almost the same thing but have different names to access their
 * data members. Hopefully by providing one class template we can avoid further
 * proliferation. */
template <class T> class ROSE_DLL_API AstValueAttribute : public AstAttribute {
public:
  /** Type of value this wrapper holds. */
  typedef T Value;

private:
  Value value_;

public:
  /** Default constructor. Constructs an attribute wrapper having a
   * default-constructed value. */
  AstValueAttribute() {}

  /** Constructs an attribute containing a specified value. */
  explicit AstValueAttribute(const T &value) : value_(value) {}

  /** Copy constructor. Invokes the copy-constructor of the contained value. */
  AstValueAttribute(const AstValueAttribute &other) : value_(other.value_) {}

  virtual AstAttribute *copy() const override {
    return new AstValueAttribute(*this);
  }
  virtual std::string attribute_class_name() const override {
    return "AstValueAttribute";
  }

  virtual AstAttribute::OwnershipPolicy getOwnershipPolicy() const override {
    return CONTAINER_OWNERSHIP;
  }

  /** Return the stored value by reference.
   *
   * @{ */
  const T &get() const { return value_; }
  T &get() { return value_; }
  /** @} */

  /** Assign a new value. */
  void set(const T &value) { value_ = value; }
};

/** Attribute storing an SgNode.
 *
 *  A SgNode attribute may be used to "dynamically add" an SgNode field to an
 * AST node. */
class ROSE_DLL_API AstSgNodeAttribute : public AstValueAttribute<SgNode *> {
public:
  typedef AstValueAttribute<SgNode *> Super;
  AstSgNodeAttribute() : Super(NULL) {}
  explicit AstSgNodeAttribute(SgNode *value) : Super(value) {}
  AstSgNodeAttribute(const AstSgNodeAttribute &other) : Super(other) {}
  virtual AstAttribute *copy() const override {
    return new AstSgNodeAttribute(*this);
  }
  virtual std::string attribute_class_name() const override {
    return "AstSgNodeAttribute";
  }
  SgNode *getNode() { return get(); }
  void setNode(SgNode *node) { set(node); }
};

class ROSE_DLL_API AstIntAttribute : public AstValueAttribute<int> {
public:
  typedef AstValueAttribute<int> Super;
  AstIntAttribute() : Super(0) {}
  explicit AstIntAttribute(int value) : Super(value) {}
  AstIntAttribute(const AstIntAttribute &other) : Super(other) {}
  virtual AstAttribute *copy() const override {
    return new AstIntAttribute(*this);
  }
  virtual std::string attribute_class_name() const override {
    return "AstIntAttribute";
  }
  int getValue() { return get(); }
};

#endif
