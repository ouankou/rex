#ifndef ROSE_RTIHELPERS_H
#define ROSE_RTIHELPERS_H

#include "Cxx_Grammar.h"
#include "rosePublicConfig.h"

#include <iomanip>

#include <list>

#include <map>

#include <optional>

#include <set>

#include <sstream>

#include <string>

#include <type_traits>

#include <unordered_map>

#include <unordered_set>

#include <vector>

// Helpful functions for Cxx_GrammarRTI.C
// Probably should not be included anywhere else

#if ROSE_USE_VALGRIND
#include <stdio.h>

#include <valgrind/memcheck.h>

#include <valgrind/valgrind.h>
static void doUninitializedFieldCheck(const char *fieldName,
                                      const void *fieldPointer,
                                      size_t fieldSize, void *wholeObject,
                                      const char *className) {
  if (VALGRIND_CHECK_MEM_IS_DEFINED(fieldPointer, fieldSize)) {
    fprintf(stderr,
            "Warning: uninitialized field p_%s of object %p of class %s\n",
            fieldName, wholeObject, className);
  }
}
#endif

template <typename T> static std::string toStringForRTI(const T &x) {
  std::ostringstream ss;
  if constexpr (std::is_enum_v<T> && !std::is_convertible_v<T, int>)
    ss << static_cast<std::underlying_type_t<T>>(x);
  else
    ss << x;
  return ss.str();
}

template <typename F, typename S>
static std::string toStringForRTI(const std::pair<F, S> &x) {
  return "(" + toStringForRTI(x.first) + ", " + toStringForRTI(x.second) + ")";
}

template <typename T>
static std::string toStringForRTI(const std::optional<T> &x) {
  if (!x.has_value()) {
    return "absent";
  }
  return "present(" + toStringForRTI(*x) + ")";
}

template <typename T>
static std::string toStringForRTI(const std::vector<T> &x) {
  std::ostringstream ss;
  ss << "[";
  for (typename std::vector<T>::const_iterator i = x.begin(); i != x.end();
       ++i) {
    if (i != x.begin())
      ss << ", ";
    ss << toStringForRTI(*i);
  }
  ss << "]";
  return ss.str();
}

template <typename F, typename S>
static std::string toStringForRTI(const std::vector<std::pair<F, S>> &x) {
  std::ostringstream ss;
  ss << "[";
  for (typename std::vector<std::pair<F, S>>::const_iterator i = x.begin();
       i != x.end(); ++i) {
    if (i != x.begin())
      ss << ", ";
    ss << i->first << "->" << i->second;
  }
  ss << "]";
  return ss.str();
}

static std::string toStringForRTI(const std::vector<bool> &x) {
  std::ostringstream ss;
  ss << "[";
  for (std::vector<bool>::const_iterator i = x.begin(); i != x.end(); ++i) {
    if (i != x.begin())
      ss << ", ";
    ss << (*i ? "T" : "F");
  }
  ss << "]";
  return ss.str();
}

template <typename T> static std::string toStringForRTI(const std::list<T> &x) {
  std::ostringstream ss;
  ss << "[";
  for (typename std::list<T>::const_iterator i = x.begin(); i != x.end(); ++i) {
    if (i != x.begin())
      ss << ", ";
    ss << toStringForRTI(*i);
  }
  ss << "]";
  return ss.str();
}

template <typename T> static std::string toStringForRTI(const std::set<T> &x) {
  std::ostringstream ss;
  ss << "[";
  for (typename std::set<T>::const_iterator i = x.begin(); i != x.end(); ++i) {
    if (i != x.begin())
      ss << ", ";
    ss << (*i);
  }
  ss << "]";
  return ss.str();
}

template <typename T>
static std::string toStringForRTI(const std::unordered_set<T> &x) {
  std::ostringstream ss;
  ss << "[";
  for (auto i = x.begin(); i != x.end(); ++i) {
    if (i != x.begin())
      ss << ", ";
    ss << (*i);
  }
  ss << "]";
  return ss.str();
}

template <typename K, typename V, typename C, typename A>
static std::string toStringForRTI(const std::map<K, V, C, A> &x) {
  std::ostringstream ss;
  ss << "[";
  for (auto i = x.begin(); i != x.end(); ++i) {
    if (i != x.begin())
      ss << ", ";
    ss << i->first << "->" << toStringForRTI(i->second);
  }
  ss << "]";
  return ss.str();
}

template <typename K, typename V, typename H, typename E, typename A>
static std::string toStringForRTI(const std::unordered_map<K, V, H, E, A> &x) {
  std::ostringstream ss;
  ss << "[";
  for (auto i = x.begin(); i != x.end(); ++i) {
    if (i != x.begin())
      ss << ", ";
    ss << i->first << "->" << toStringForRTI(i->second);
  }
  ss << "]";
  return ss.str();
}

template <typename K, typename V, typename H, typename E, typename A>
static std::string
toStringForRTI(const std::unordered_multimap<K, V, H, E, A> &x) {
  std::ostringstream ss;
  ss << "[";
  for (auto i = x.begin(); i != x.end(); ++i) {
    if (i != x.begin())
      ss << ", ";
    ss << toStringForRTI(i->first) << "->" << toStringForRTI(i->second);
  }
  ss << "]";
  return ss.str();
}

static std::string toStringForRTI(const rose_graph_integer_node_hash_map &x) {
  return toStringForRTI(
      static_cast<const rose_graph_integer_node_hash_map_base_type &>(x));
}

static std::string toStringForRTI(const rose_graph_integer_edge_hash_map &x) {
  return toStringForRTI(
      static_cast<const rose_graph_integer_edge_hash_map_base_type &>(x));
}

static std::string
toStringForRTI(const rose_graph_integer_edge_hash_multimap &x) {
  return toStringForRTI(
      static_cast<const rose_graph_integer_edge_hash_multimap_base_type &>(x));
}

static std::string
toStringForRTI(const rose_graph_string_integer_hash_multimap &x) {
  return toStringForRTI(
      static_cast<const rose_graph_string_integer_hash_multimap_base_type &>(
          x));
}

static std::string
toStringForRTI(const rose_graph_integerpair_edge_hash_multimap &x) {
  return toStringForRTI(
      static_cast<const rose_graph_integerpair_edge_hash_multimap_base_type &>(
          x));
}

template <typename K, typename V>
static std::string toStringForRTI(const std::multimap<K, V> &x) {
  std::ostringstream ss;
  ss << "[";
  for (typename std::multimap<K, V>::const_iterator i = x.begin(); i != x.end();
       ++i) {
    if (i != x.begin())
      ss << ", ";
    ss << i->first << "->" << i->second;
  }
  ss << "]";
  return ss.str();
}

static std::string toStringForRTI(const SgAccessModifier &m) {
  return m.displayString();
}

static std::string toStringForRTI(const SgConstVolatileModifier &m) {
  return m.displayString();
}

static std::string toStringForRTI(const SgElaboratedTypeModifier &m) {
  return m.displayString();
}

static std::string toStringForRTI(const SgTypeModifier &m) {
  return m.displayString();
}

static std::string toStringForRTI(const SgStorageModifier &m) {
  return m.displayString();
}

static std::string toStringForRTI(const SgDeclarationModifier &m) {
  return m.displayString();
}

static std::string toStringForRTI(const SgFunctionModifier &m) {
  return m.displayString();
}

static std::string toStringForRTI(const SgSpecialFunctionModifier &m) {
  return m.displayString();
}

static std::string toStringForRTI(const SgName &n) { return n.getString(); }

static std::string
toStringForRTI(const SgFunctionModifier::opencl_work_group_size_t &x) {
  std::ostringstream os;
  os << " ( " << x.x << ", " << x.y << ", " << x.z << " ) ";
  return os.str();
}

template <typename T> static std::string formatRTIField(const void *fieldPtr) {
  return toStringForRTI(*static_cast<const T *>(fieldPtr));
}

template <typename T>
static RTIValueDescriptor makeRTIValueDescriptor(const T &field) {
  return RTIValueDescriptor(static_cast<const void *>(&field),
                            &formatRTIField<T>);
}

template <typename T>
void doRTI(const char *fieldNameBase, T *fieldPtr, void *thisPtr,
           const char *className, const char *typeString, const char *fieldName,
           const RTIValueDescriptor &fieldContents, RTIMemberData &memberData) {
#if ROSE_USE_VALGRIND
  if constexpr (std::is_scalar_v<T>) {
    const void *address =
        const_cast<const void *>(static_cast<const volatile void *>(fieldPtr));
    doUninitializedFieldCheck(fieldNameBase, address, sizeof(T), thisPtr,
                              className);
  }
#else
  (void)fieldNameBase;
  (void)fieldPtr;
  (void)thisPtr;
  (void)className;
#endif
  memberData = RTIMemberData(typeString, fieldName, fieldContents);
}

#endif // ROSE_RTIHELPERS_H
