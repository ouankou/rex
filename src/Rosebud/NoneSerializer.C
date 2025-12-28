#include <Rosebud/NoneSerializer.h>

namespace Rosebud {

NoneSerializer::Ptr
NoneSerializer::instance() {
    return Ptr(new NoneSerializer);
}

std::string
NoneSerializer::name() const {
    return "none";
}

std::string
NoneSerializer::purpose() const {
    return "Generates no serialization code.";
}

bool
NoneSerializer::isSerializable(const Ast::ClassPtr&) const {
    return false;
}

void
NoneSerializer::generate(std::ostream&, std::ostream&, const Ast::ClassPtr&, const Generator&) const {}

} // namespace
