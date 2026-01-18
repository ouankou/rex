#ifndef ROSE_As_H
#define ROSE_As_H

#include <memory>

namespace Rose {

/** Dynamic pointer down-cast.
 *
 *  Down-casts the first argument from a pointer to `U` to a pointer to `T`
 * where `T` is a subclass of `U`. If the cast is possible, then this function
 * returns the new pointer, otherwise it returns a null pointer. Since pointers
 * can be used in Boolean contexts, this function can also be used to test
 * whether the cast is possible.
 *
 *  This function handles `std::shared_ptr` and raw pointers, including Sage AST
 * node pointers.
 *
 *  @code
 *  #include <Rose/As.h>
 *
 *  SgType *type = ...;
 *  if (auto array = Rose::as<SgArrayType>(type)) {         // equivalent to
 * `isSgArrayType(node)` if (Rose::as<SgTypeBool>(array->get_base_type())) { //
 * equivalent to `isSgTypeBool(node)` std::cout <<"the type is an array of
 * Boolean\n";
 *      }
 *  }
 *  @endcode
 *
 *  @{ */
/** Cast a shared pointer to a derived type.
 *
 * @param p Pointer to cast.
 * @return Cast pointer or null if the dynamic cast fails. */
template <class T, class U> std::shared_ptr<T> as(const std::shared_ptr<U> &p) {
  return std::dynamic_pointer_cast<T>(p);
}

/** Cast a raw pointer to a derived type.
 *
 * @param p Pointer to cast.
 * @return Cast pointer or null if the dynamic cast fails. */
template <class T, class U> T *as(U *p) { return dynamic_cast<T *>(p); }
/** @} */

} // namespace Rose

#endif
