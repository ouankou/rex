#ifndef ROSE_BitOps_H
#define ROSE_BitOps_H
#include "mlog.h"

#include <cstddef>

#include <cstdint>

namespace Rose {

/** Bit operations on unsigned integers.
 *
 * This namespace provides functions that operate at the bit level on unsigned
 * integer types and avoid C/C++ undefined behavior. */
namespace BitOps {

/** Number of bits in a type or value.
 *
 * @param x Sample value used to deduce the type.
 * @return Number of bits in the unsigned type. */
template <typename Unsigned> inline size_t nBits(Unsigned x = Unsigned(0)) {
  return 8 * sizeof(Unsigned);
}

/** Generate a value with all bits set or cleared.
 *
 * @param b If true, set all bits; otherwise clear them.
 * @return Value with all bits set or cleared. */
template <typename Unsigned> inline Unsigned all(bool b = true) {
  return Unsigned(0) - Unsigned(b ? 1 : 0);
}

/** Generate a value with low order bits set.
 *
 *  Returns a value whose `n` low-order bits are set and all the other higher
 * order bits are cleared. If `n` is greater than or equal to the size of the
 * unsigned type then the returned value has all bits set. If `n` is zero then
 * no bits are set in the return value.
 *
 * @param n Number of low-order bits to set.
 * @return Mask value with low bits set. */
template <typename Unsigned> inline Unsigned lowMask(size_t n) {
  return n >= nBits<Unsigned>() ? all<Unsigned>(true)
                                : (Unsigned(1) << n) - Unsigned(1);
}

/** Set or clear the low-order @n bits.
 *
 *  Sets or clears the low order bits of the input value without affecting any
 * of the other bits.
 *
 * @param src Input value.
 * @param w Width of the low-order region.
 * @param b Bit value to write.
 * @return Updated value. */
template <typename Unsigned>
inline Unsigned allLsb(Unsigned src, size_t w, bool b = true) {
  ASSERT_require(w <= nBits(src));
  if (b) {
    return src | lowMask<Unsigned>(w);
  } else {
    return src & ~lowMask<Unsigned>(w);
  }
}

/** Generate a value with high order bits set.
 *
 *  Returns a value whose `n` high-order bits are set and the other low order
 * bits are cleared.  If `n` is greater than or equal to the size of the
 * unsigned type then the returned value has all bits set.
 *
 * @param n Number of high-order bits to set.
 * @return Mask value with high bits set. */
template <typename Unsigned> inline Unsigned highMask(size_t n) {
  if (n == 0) {
    return Unsigned(0);
  }
  if (n >= nBits<Unsigned>()) {
    return all<Unsigned>(true);
  }
  return lowMask<Unsigned>(n) << (nBits<Unsigned>() - n);
}

/** Combine two values based on a bit mask.
 *
 *  The return value has bits from `a` and `b` depending on the mask `cond.`  If
 * the mask bit `i` is set, then the return value bit `i` comes from `a,`
 * otherwise it comes from `b.`
 *
 * @param cond Mask selecting between `a` and `b`.
 * @param a Value used when mask bit is 1.
 * @param b Value used when mask bit is 0.
 * @return Combined value. */
template <typename Unsigned>
inline Unsigned select(Unsigned cond, Unsigned a, Unsigned b) {
  return (a & cond) | (b & ~cond);
}

/** Left shift a value.
 *
 *  The value `src` has its bits shifted `n` places toward higher order. The @n
 * highest order bits are discarded and the `n` new low-order bits are either
 * set or cleared depending on the value of `b.` If `n` is greater than or equal
 * to the number of bits in `src` then the return value has either all bits set
 * or all bits cleared depending on `b.`
 *
 * @param src Input value.
 * @param n Shift count.
 * @param b Fill bit value.
 * @return Shifted value. */
template <typename Unsigned>
inline Unsigned shiftLeft(Unsigned src, size_t n, bool b = false) {
  if (n >= nBits(src)) {
    return all<Unsigned>(b);
  } else {
    return Unsigned(src << n) | (all<Unsigned>(b) & lowMask<Unsigned>(n));
  }
}

/** Left shift part of a value without affecting the rest.
 *
 *  The value `src` has its low-order `w` bits shifted `n` places toward higher
 * order.  The @n highest bits are discarded and the `n` new lower order bits
 * are either set or cleared depending on the value of `b.` If `n` is greater
 * than or equal to `w` then all affected bits are set or cleared, depending on
 * `b.`  The bits not in the affected range are not affected and are returned.
 *
 * @param src Input value.
 * @param w Width of the low-order region.
 * @param n Shift count.
 * @param b Fill bit value.
 * @return Updated value. */
template <typename Unsigned>
inline Unsigned shiftLeftLsb(Unsigned src, size_t w, size_t n, bool b = false) {
  ASSERT_require(w <= nBits(src));
  if (n >= w) {
    return allLsb(src, w, b);
  } else {
    return select(lowMask<Unsigned>(w), shiftLeft(src, n, b), src);
  }
}

/** Right shift a value.
 *
 *  Shift all bits of the value right (to lower indices) by `n.` The `n`
 * low-order bits are discarded and the new `n` high-order bits are set or
 * cleared depending on `b.`  If `n` is greater than or equal to the size of
 * `src` then the return value has either all bits set or all bits cleared
 * depending on `b.`
 *
 * @param src Input value.
 * @param n Shift count.
 * @param b Fill bit value.
 * @return Shifted value. */
template <typename Unsigned>
inline Unsigned shiftRight(Unsigned src, size_t n, bool b = false) {
  if (n >= nBits(src)) {
    return all<Unsigned>(b);
  } else {
    return Unsigned(src >> n) | (all<Unsigned>(b) & highMask<Unsigned>(n));
  }
}

/** Right shift part of a value without affecting the rest.
 *
 *  The value `src` has its low-order `w` bits shifted right toward lower order.
 * The `n` lowest bits are discarded and the `n` new higher order bits are
 * either set or cleared depending on `b.` If `n` is greater than or equal to
 * `w` then all affected bits are set or cleared, depending on `b.`  The bits
 * not in the affected range are not affected and are returned.
 *
 * @param src Input value.
 * @param w Width of the low-order region.
 * @param n Shift count.
 * @param b Fill bit value.
 * @return Updated value. */
template <typename Unsigned>
inline Unsigned shiftRightLsb(Unsigned src, size_t w, size_t n,
                              bool b = false) {
  ASSERT_require(w <= nBits(src));
  if (n >= w) {
    return allLsb(src, w, b);
  } else {
    return select(lowMask<Unsigned>(w),
                  shiftRight(src & lowMask<Unsigned>(w), n, b), src);
  }
}

/** Generate a single-bit mask.
 *
 *  Returns a value that has all bit cleared except the bit at position `i.` If
 * `i` is outside the valid range of bit positions for the unsigned type, then
 * zero is returned.
 *
 * @param i Bit index to set.
 * @return Single-bit mask. */
template <typename Unsigned> inline Unsigned position(size_t i) {
  return i < nBits<Unsigned>() ? shiftLeft(Unsigned(1), i) : Unsigned(0);
}

/** Generate a single-bit mask without affecting the high-order bits.
 *
 *  The low order `w` bits of src are cleared except bit `i` is set, and other
 * bits are not affected.
 *
 * @param src Input value.
 * @param w Width of the low-order region.
 * @param i Bit index to set.
 * @return Updated value. */
template <typename Unsigned>
inline Unsigned positionLsb(Unsigned src, size_t w, size_t i) {
  ASSERT_require(w <= nBits(src));
  return select(lowMask<Unsigned>(w), position<Unsigned>(i), src);
}

/** Generate a mask.
 *
 *  Returns a value where bits `least` through `greatest` (inclusive) are set
 * and all other bits are cleared. The specified indexes must be valid for the
 * type of `x.` In other words, `greatest` must be less than the number of bits
 * in `x` and `greatest` must be greater than or equal to `least.`
 *
 * @param least Least significant bit index.
 * @param greatest Most significant bit index.
 * @return Mask value. */
template <typename Unsigned>
inline Unsigned mask(size_t least, size_t greatest) {
  ASSERT_require(greatest < nBits<Unsigned>());
  ASSERT_require(greatest >= least);
  return shiftLeft(lowMask<Unsigned>(greatest - least + 1), least);
}

/** Generate a mask without affecting other bits.
 *
 *  Generates a mask limited to the low order `w` bits without affecting the
 * other bits of src.
 *
 * @param src Input value.
 * @param w Width of the low-order region.
 * @param least Least significant bit index.
 * @param greatest Most significant bit index.
 * @return Updated value. */
template <typename Unsigned>
inline Unsigned maskLsb(Unsigned src, size_t w, size_t least, size_t greatest) {
  ASSERT_require(w <= nBits(src));
  return select(lowMask<Unsigned>(w), mask<Unsigned>(least, greatest), src);
}

/** Extract a single bit.
 *
 *  The bit at position `i` of the value `src` is returned. If `i` is out of
 * range for `src` then zero is returned.
 *
 * @param src Input value.
 * @param i Bit index.
 * @return True if the bit is set. */
template <typename Unsigned> inline bool bit(Unsigned src, size_t i) {
  return i < nBits(src) ? (src & position<Unsigned>(i)) != 0 : false;
}

/** Extract a single bit.
 *
 *  The bit at position `i` of value `src` is returned. If `i` is out of range
 * for the specified value width then zero is returned.
 *
 * @param src Input value.
 * @param w Width of the low-order region.
 * @param i Bit index.
 * @return True if the bit is set. */
template <typename Unsigned>
inline bool bitLsb(Unsigned src, size_t w, size_t i) {
  return i < w ? (src & position<Unsigned>(i)) != 0 : false;
}

/** Most significant bit.
 *
 *  Returns the most significant bit. This is the sign bit for two's complement
 * values.
 *
 * @param src Input value.
 * @return Most significant bit. */
template <typename Unsigned> inline bool msb(Unsigned src) {
  return bit(src, nBits(src) - 1);
}

/** Most significant bit within lsb region.
 *
 *  Returns the value of the most significant bit within the region of `w`
 * low-order bits. If `w` is zero then returns false.
 *
 * @param src Input value.
 * @param w Width of the low-order region.
 * @return Most significant bit within the region. */
template <typename Unsigned> inline bool msbLsb(Unsigned src, size_t w) {
  ASSERT_require(w <= nBits(src));
  return w > 0 ? bit(src, w - 1) : false;
}

/** Right shift replicating MSB.
 *
 *  Shift all bits of the value right (to lower indices) by `n.` The `n`
 * low-order bits are discarded and the new `n` high-order bits are set or
 * cleared depending on the original most significant bit.  If `n` is greater
 * than or equal to the size of `src` then the return value has either all bits
 * set or all bits cleared depending on its original most significant bit.
 *
 * @param src Input value.
 * @param n Shift count.
 * @return Shifted value. */
template <typename Unsigned>
inline Unsigned shiftRightSigned(Unsigned src, size_t n) {
  return shiftRight(src, n, msb(src));
}

/** Right shift low bits without affecting other bits.
 *
 *  Right shift the low-order `w` bits by `n` without affecting higher order
 * bits. The width, `w,` must not be larger than the `src` width. If @n is
 * greater than or equal to `w` then the `w` low order bits are set or cleared
 * depending on bit `w-1`. Otherwise, the `n` low order bits are discarded and
 * the `n` new bits introduced at index `w-1` are either zero or one depending
 * on bit `w-1`.
 *
 * @param src Input value.
 * @param w Width of the low-order region.
 * @param n Shift count.
 * @return Updated value. */
template <typename Unsigned>
inline Unsigned shiftRightSigned(Unsigned src, size_t w, size_t n) {
  return shiftRightLsb(src, n, w, msbLsb(src, w));
}

/** Extract part of a value.
 *
 *  Extracts the bits in the range `least` through @greatest (inclusive) and
 * shifts them right by `least` bits. The `least` and `greatest` indices must be
 * valid for `src` as defined by @ref mask.
 *
 * @param src Input value.
 * @param least Least significant bit index.
 * @param greatest Most significant bit index.
 * @return Extracted bits. */
template <typename Unsigned>
inline Unsigned bits(Unsigned src, size_t least, size_t greatest) {
  return shiftRight(src & mask<Unsigned>(least, greatest), least);
}

/** Extract part of a value limited by width.
 *
 *  Extracts the bits in the range `least` through @greatest (inclusive) and
 * shifts them right by `least` bits. Any bits of `src` at index `w` or greater
 * are treated as zeros.
 *
 * @param src Input value.
 * @param w Width of the low-order region.
 * @param least Least significant bit index.
 * @param greatest Most significant bit index.
 * @return Extracted bits. */
template <typename Unsigned>
inline Unsigned bitsLsb(Unsigned src, size_t w, size_t least, size_t greatest) {
  return shiftRight(
      src & mask<Unsigned>(least, greatest) & lowMask<Unsigned>(w), least);
}

/** Extend or truncate a value.
 *
 *  When the destination type is smaller than the source type, the most
 * significant bits of the source value are discarded, otherwise the most
 * significant bits of the destination type are set to `b.`
 *
 * @param x Input value.
 * @param b Fill bit value when extending.
 * @return Converted value. */
template <typename UnsignedTarget, typename UnsignedSource>
inline UnsignedTarget convert(UnsignedSource x, bool b = false) {
  if (nBits(x) < nBits<UnsignedTarget>()) {
    // extending
    return UnsignedTarget(x) |
           (all<UnsignedTarget>(b) & ~lowMask<UnsignedTarget>(nBits(x)));
  } else {
    // truncating
    return UnsignedTarget(x & lowMask<UnsignedSource>(nBits<UnsignedTarget>()));
  }
}

/** Sign extend or truncate a value.
 *
 *  This is identical to @ref convert except when the target value is wider than
 * the source value the new bits of the return value are all set to the most
 * significant bit of the source value.
 *
 * @param x Input value.
 * @return Converted value with sign extension when widening. */
template <typename UnsignedTarget, typename UnsignedSource>
inline UnsignedTarget convertSigned(UnsignedSource x) {
  return convert<UnsignedTarget>(x, msb(x));
}

/** Sign extend part of a value to the full width of the src type.
 *
 *  The low order `n` bits are treated as a signed integer and sign extended to
 * fill the entire width of the return value.
 *
 * @param src Input value.
 * @param n Width of the signed region.
 * @return Sign-extended value. */
template <typename Unsigned>
inline Unsigned signExtend(Unsigned src, size_t n) {
  if (n < nBits(src)) {
    if (msbLsb(src, n)) {
      src |= mask<Unsigned>(n, nBits(src) - 1);
    } else {
      src &= ~mask<Unsigned>(n, nBits(src) - 1);
    }
  }
  return src;
}

/** Sign extend part of value without affecting other bits.
 *
 *  Sign extends the low-order `n` bits of the input value to occupy the lower
 * order `m` bits of the output, where `m` is greater than or equal to `n` and
 * less than or equal to the number of bits in the `src` value.
 *
 * @param src Input value.
 * @param n Width of the signed region.
 * @param m Width of the output region.
 * @return Updated value. */
template <typename Unsigned>
inline Unsigned signExtendLsb(Unsigned src, size_t n, size_t m) {
  ASSERT_require(n > 0);
  ASSERT_require(m >= n);
  ASSERT_require(m <= nBits(src));
  if (m == n) {
    return src;
  } else {
    Unsigned newBitsMask = mask<Unsigned>(n, m - 1);
    if (bit(src, n - 1)) {
      return src | newBitsMask;
    } else {
      return src & ~newBitsMask;
    }
  }
}

/** Rotate bits left.
 *
 *  Rotates the bits of `src` left (toward higher indices) by `n` bits. This is
 * similar to @ref shiftLeft except the high order bits that would normally be
 * discarded are reintroduced in the low order positions. If `n` is zero then
 * this is a no-op. The rotation amount is calculated modulo the width of `src`.
 *
 * @param src Input value.
 * @param n Rotation count.
 * @return Rotated value. */
template <typename Unsigned>
inline Unsigned rotateLeft(Unsigned src, size_t n) {
  n %= nBits(src);
  return shiftLeft(src, n) | shiftRight(src, nBits(src) - n);
}

/** Rotate low-order bits left without affecting others.
 *
 *  Rotates the low-order `w` bits of `src` left by `n` bits without affecting
 * the other bits, and returns the result. The rotation amount is modulo `w.` If
 * `w` is zero then the original value is returned.
 *
 * @param src Input value.
 * @param w Width of the low-order region.
 * @param n Rotation count.
 * @return Updated value. */
template <typename Unsigned>
inline Unsigned rotateLeftLsb(Unsigned src, size_t w, size_t n) {
  ASSERT_require(w <= nBits(src));
  n = w ? n % w : 0;
  return select(lowMask<Unsigned>(w),
                shiftLeftLsb(src, w, n) | shiftRightLsb(src, w - n), src);
}

/** Rotate bits right.
 *
 *  Rotates the bits of `src` right (toward lower indices) by `n` bits. This is
 * similar to @ref shiftRight except the low order bits that would normally be
 * discarded are reintroduced in the high order positions. If `n` is zero then
 * this is a no-op. The rotation amount is calculated modulo the width of `src.`
 *
 * @param src Input value.
 * @param n Rotation count.
 * @return Rotated value. */
template <typename Unsigned>
inline Unsigned rotateRight(Unsigned src, size_t n) {
  n %= nBits(src);
  return shiftRight(src, n) | shiftLeft(src, nBits(src) - n);
}

/** Rotate low-order bits right without affecting others.
 *
 *  Rotates the low-order `w` bits of `src` right by `n` bits without affecting
 * the higher-order bits, and returns the result.  The rotation amount is modulo
 * `w.` If `w` is zero then the original value is returned.
 *
 * @param src Input value.
 * @param w Width of the low-order region.
 * @param n Rotation count.
 * @return Updated value. */
template <typename Unsigned>
inline Unsigned rotateRightLsb(Unsigned src, size_t w, size_t n) {
  ASSERT_require(w <= nBits(src));
  n = w ? n % w : 0;
  return select(lowMask<Unsigned>(w),
                shiftRightLsb(src, w, n) | shiftLeftLsb(src, w, w - n), src);
}

/** Replicate low-order bits to fill return value.
 *
 *  The `n` low-order bits of `src` are repeated as a group as many times as
 * necessary to fill the entire return value. For instance, if `src` contains
 * 0xabcdef and `n` is 8 and the return type is a 32-bit unsigned integer, then
 * the return value will be 0xefefefef.  If the width of the return value is not
 * an integer multiple of `n,` then the high order bits of the return value will
 * contain only some of the lowest order bits of the `src.` The value of `n`
 * cannot be zero.
 *
 * @param src Input value.
 * @param n Width of the pattern to replicate.
 * @return Replicated value. */
template <typename Unsigned> inline Unsigned replicate(Unsigned src, size_t n) {
  ASSERT_require(n != 0);
  if (n >= nBits(src)) {
    return src;
  } else {
    size_t ngroups = (nBits(src) + 1) / n;
    Unsigned retval = 0;
    for (size_t i = 0; i < ngroups; ++i)
      retval |= shiftLeft(src & lowMask<Unsigned>(n), i * n);
    return retval;
  }
}

/** Replicate low-order bits to fill region without affecting other bits.
 *
 *  This is identical to @ref replicate except that instead of filling the
 * entire return value with the replicated bits, at most `w` low-order bits of
 * the return value are filled with replicated bits and the remaining high order
 * bits are copied from `src.`
 *
 * @param src Input value.
 * @param w Width of the low-order region to fill.
 * @param n Width of the pattern to replicate.
 * @return Updated value. */
template <typename Unsigned>
inline Unsigned replicateLsb(Unsigned src, size_t w, size_t n) {
  ASSERT_require(w <= nBits(src));
  return select(lowMask<Unsigned>(w), replicate(src, n), src);
}

template <typename Unsigned> inline size_t nSet(Unsigned src) {
  size_t retval = 0;
  while (src != 0) {
    if ((src & 1) != 0)
      ++retval;
    src >>= 1;
  }
  return retval;
}

} // namespace BitOps
} // namespace Rose
#endif
