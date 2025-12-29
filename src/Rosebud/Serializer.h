#ifndef Rosebud_Serializer_H
#define Rosebud_Serializer_H
#include <Rosebud/Ast.h>

namespace Rosebud {

/** Base class for serialization generators.
 *
 *  A serialization generator is responsible for generating C++ source code to serialize and deserialize instances of an node
 *  class. */
class Serializer {
public:
    using Ptr = SerializerPtr;

private:
    static std::vector<Ptr> registry_;

public:
    virtual ~Serializer() {}

protected:
    Serializer() {}

public:
    /** Register a serializer for use later. */
    static void registerSerializer(const Ptr&);

    /** Return all registered serializers. */
    static const std::vector<Ptr>& registeredSerializers();

    /** Return the registered serializer with the specified name. */
    static Ptr lookup(const std::string&);

    /** Every serializer has a unique name. */
    virtual std::string name() const = 0;

    /** Single-line description for the serializer. */
    virtual std::string purpose() const = 0;

    /** Determines if a class should be serialized. */
    virtual bool isSerializable(const Ast::ClassPtr&) const = 0;

    /** Generate code for the specified class.
     *
     *  The @p header and @p impl streams are output streams for the C++ header file and implementation file, respectively. The
     *  generator is from whence this serializer was called. */
    virtual void generate(std::ostream &header, std::ostream &impl, const Ast::ClassPtr&, const Generator&) const = 0;

private:
    static void initRegistry();
};

} // namespace
#endif
