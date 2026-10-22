
#ifndef OBSERVE_OBJECT
#define OBSERVE_OBJECT

#include "PtrSet.h"

#include <assert.h>

#include <string>

template <class Observer> class ObserveInfo {
public:
  virtual void UpdateObserver(Observer &o) const = 0;
  virtual std::string toString() { return ""; };
  virtual ~ObserveInfo() {}
};

template <class Observer> class ObserveObject {
  typedef PtrSetWrap<Observer> ObSetType;
  ObSetType obList;

public:
  typedef typename PtrSetWrap<Observer>::const_iterator Iterator;
  virtual ~ObserveObject() {}
  void AttachObserver(Observer *o) {
    assert(!obList.IsMember(o));
    obList.insert(o);
  }
  void DetachObserver(Observer *o) { obList.erase(o); }

  Iterator GetObserverIterator() const { return obList.begin(); }
  void Notify(const ObserveInfo<Observer> &info) {
    const ObSetType pending(obList);
    for (Iterator p = pending.begin(); !p.ReachEnd(); ++p) {
      Observer *o = *p;
      if (obList.IsMember(o))
        info.UpdateObserver(*o);
    }
  }
  unsigned NumberOfObservers() const { return obList.NumberOfEntries(); }
};

#endif
