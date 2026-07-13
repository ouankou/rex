#ifndef REX_FRONTEND_INJECTED_CLASS_NTTP_PROVENANCE_HPP
#define REX_FRONTEND_INJECTED_CLASS_NTTP_PROVENANCE_HPP

namespace RexInjectedClassNttpProvenance {

template <int RexNttpValue> struct RexInjectedClass {
  RexInjectedClass<RexNttpValue> *
  rex_clone(RexInjectedClass<RexNttpValue> *value) {
    return value;
  }
};

} // namespace RexInjectedClassNttpProvenance

#endif
