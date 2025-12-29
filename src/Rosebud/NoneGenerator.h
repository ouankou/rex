#ifndef Rosebud_NoneGenerator_H
#define Rosebud_NoneGenerator_H
#include <Rosebud/Generator.h>

namespace Rosebud {

/** Generator that produces no output. */
class NoneGenerator: public Generator {
public:
    using Ptr = std::shared_ptr<NoneGenerator>;

protected:
    NoneGenerator() {}

public:
    static Ptr instance();
    virtual std::string name() const override;
    virtual std::string purpose() const override;
    virtual void generate(const Ast::ProjectPtr&) override;
};

} // namespace
#endif
