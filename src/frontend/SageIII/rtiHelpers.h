#ifndef ROSE_RTIHELPERS_H
#define ROSE_RTIHELPERS_H

#include <iomanip>
#include <list>
#include <set>
#include <sstream>
#include <string>
#include <unordered_map>
#include <unordered_set>
#include <vector>

// Helpful functions for Cxx_GrammarRTI.C
// Probably should not be included anywhere else

#if ROSE_USE_VALGRIND
#include <stdio.h>
#include <valgrind/memcheck.h>
#include <valgrind/valgrind.h>
static void doUninitializedFieldCheck(const char *fieldName, void *fieldPointer,
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
  ss << x;
  return ss.str();
}

template <typename T>
static std::string toStringForRTI(const std::vector<T> &x) {
  std::ostringstream ss;
  ss << "[";
  for (typename std::vector<T>::const_iterator i = x.begin(); i != x.end();
       ++i) {
    if (i != x.begin())
      ss << ", ";
    ss << (*i);
  }
  ss << "]";
  return ss.str();
}

template <typename T>
static std::string toStringForRTI(const std::vector<std::pair<T, T>> &x) {
  std::ostringstream ss;
  ss << "[";
  for (typename std::vector<std::pair<T, T>>::const_iterator i = x.begin();
       i != x.end(); ++i) {
    if (i != x.begin())
      ss << ", ";
    ss << i->first << "->" << i->second;
  }
  ss << "]";
  return ss.str();
}

template <typename F, typename S> // First and Second
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

template <typename K, typename V>
static std::string toStringForRTI(const std::map<K, V> &x) {
  std::ostringstream ss;
  ss << "[";
  for (typename std::map<K, V>::const_iterator i = x.begin(); i != x.end();
       ++i) {
    if (i != x.begin())
      ss << ", ";
    ss << i->first << "->" << toStringForRTI(i->second);
  }
  ss << "]";
  return ss.str();
}

template <typename K, typename V>
static std::string toStringForRTI(const std::unordered_map<K, V> &x) {
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

template <typename K>
static std::string
toStringForRTI(const std::map<K, std::set<PreprocessingInfo *>> &x) {
  std::ostringstream ss;
  ss << "[";
  for (typename std::map<K, std::set<PreprocessingInfo *>>::const_iterator i =
           x.begin();
       i != x.end(); ++i) {
    if (i != x.begin())
      ss << ", ";
    ss << i->first << "->" << toStringForRTI(i->second);
  }
  ss << "]";
  return ss.str();
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

#ifdef ROSE_USING_GRAPH_IR_NODES_FOR_BACKWARD_COMPATABILITY
static std::string
toStringForRTI(const rose_graph_node_edge_hash_multimap & /*x*/) {
  std::ostringstream ss;
  ss << "[";
  ss << "]";
  return ss.str();
}
#endif

static std::string
toStringForRTI(const rose_graph_integer_node_hash_map & /*x*/) {
  std::ostringstream ss;
  ss << "[";
  ss << "]";
  return ss.str();
}

static std::string
toStringForRTI(const rose_graph_integer_edge_hash_map & /*x*/) {
  std::ostringstream ss;
  ss << "[";
  ss << "]";
  return ss.str();
}

static std::string
toStringForRTI(const rose_graph_integer_edge_hash_multimap & /*x*/) {
  std::ostringstream ss;
  ss << "[";
  ss << "]";
  return ss.str();
}

static std::string
toStringForRTI(const rose_graph_string_integer_hash_multimap & /*x*/) {
  std::ostringstream ss;
  ss << "[";
  ss << "]";
  return ss.str();
}

static std::string
toStringForRTI(const rose_graph_integerpair_edge_hash_multimap & /*x*/) {
  std::ostringstream ss;
  ss << "[";
  ss << "]";
  return ss.str();
}

#ifdef ROSE_USING_GRAPH_IR_NODES_FOR_BACKWARD_COMPATABILITY
static std::string toStringForRTI(const rose_graph_hash_multimap & /*x*/) {
  std::ostringstream ss;
  ss << "[";
  ss << "]";
  return ss.str();
}
#endif

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

// For scoped enumerations from ROSETTA

void doRTI(const char *fieldNameBase, void *fieldPtr, size_t fieldSize,
           void *thisPtr, const char *className, const char *typeString,
           const char *fieldName, const std::string &fieldContents,
           RTIMemberData &memberData);

#endif // ROSE_RTIHELPERS_H
