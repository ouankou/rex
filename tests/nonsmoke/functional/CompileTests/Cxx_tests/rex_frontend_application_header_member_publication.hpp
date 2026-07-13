#pragma once

template <typename T> class RexApplicationHeaderMemberPublication {
public:
  T first(T value) const { return later(value); }
  T later(T value) const {
    typedef T RexLocalTypedef;
    using RexLocalAlias = RexLocalTypedef;
    struct RexLocalRecord {
      RexLocalAlias field;
    };
    enum RexLocalEnum { RexLocalEnumerator };
    RexLocalRecord rexLocalRecord{value};
    return rexLocalRecord.field;
  }
};
