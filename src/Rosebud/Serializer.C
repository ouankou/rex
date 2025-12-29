#include <Rosebud/Serializer.h>

#include <Rosebud/NoneSerializer.h>
#include <Rosebud/Utility.h>

namespace Rosebud {

std::vector<Serializer::Ptr>
Serializer::registry_;

void
Serializer::initRegistry() {
    static bool initialized = false;
    if (!initialized) {
        registry_.push_back(NoneSerializer::instance());
        initialized = true;
    }
}

void
Serializer::registerSerializer(const Ptr &serializer) {
    ASSERT_not_null(serializer);
    initRegistry();
    registry_.push_back(serializer);
}

const std::vector<Serializer::Ptr>&
Serializer::registeredSerializers() {
    initRegistry();
    return registry_;
}

Serializer::Ptr
Serializer::lookup(const std::string &name) {
    initRegistry();
    for (auto it = registry_.rbegin(); it != registry_.rend(); ++it) {
        if ((*it)->name() == name)
            return *it;
    }
    return {};
}

} // namespace
