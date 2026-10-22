#ifndef PTRMAP_H
#define PTRMAP_H

#include <map>

template <class T1, class T2> class PtrMapWrap {
  using MapType = std::map<const T1 *, T2 *>;
  MapType impl;
  PtrMapWrap(const PtrMapWrap &) = delete;
  PtrMapWrap &operator=(const PtrMapWrap &) = delete;

public:
  PtrMapWrap() = default;
  T2 *Map(const T1 *t) const {
    const auto entry = impl.find(t);
    return entry == impl.end() ? nullptr : entry->second;
  }
  void InsertMapping(const T1 *t1, T2 *t2) {
    if (t2 != nullptr)
      impl.insert_or_assign(t1, t2);
    else
      impl.erase(t1);
  }
  void RemoveMapping(const T1 *t) { impl.erase(t); }

  class Iterator {
    const MapType *impl;
    typename MapType::const_iterator p;
    Iterator(const MapType *_impl) : impl(_impl) { Reset(); }

  public:
    ~Iterator() {}
    Iterator(const Iterator &that) : impl(that.impl), p(that.p) {}
    Iterator &operator=(const Iterator &that) {
      impl = that.impl;
      p = that.p;
      return *this;
    }

    const T1 *Current() const {
      return (p == impl->end()) ? nullptr : p->first;
    }

    T2 *Current2() const { return (p == impl->end()) ? nullptr : p->second; }
    const T1 *operator*() const { return Current(); }
    void Reset() { p = impl->begin(); }
    void Advance() {
      if (p != impl->end())
        ++p;
    }
    void operator++() { Advance(); }
    void operator++(int) { Advance(); }
    bool ReachEnd() const { return p == impl->end(); }
    friend class PtrMapWrap<T1, T2>;
  };

  Iterator GetIterator() const { return Iterator(&impl); }
};

#endif
