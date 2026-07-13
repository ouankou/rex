#pragma once

class RexHeaderMemberInitializerOwnership {
public:
  RexHeaderMemberInitializerOwnership();

  const char *value() const;
  unsigned size() const;

private:
  const char *p_char;
  unsigned p_size;
};
