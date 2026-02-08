
struct __NSConstantString_tag {
  const int *isa;
  int flags;
  const char *str;
  long length;
};

struct __va_list_tag {
  unsigned int gp_offset;
  unsigned int fp_offset;
  void *overflow_arg_area;
  void *reg_save_area;
};

struct LayoutDocEntry {
  enum Kind { MemberGroups };
};

struct LayoutNavEntry {
  enum Kind { MainPage };
};

class LayoutParser {

private:
  class StartElementHandlerSection {
  private:
    typedef void (LayoutParser::*Handler)(::LayoutDocEntry::Kind);

    void foobar_A() { (m_parent->*m_handler)(m_kind); }
    class LayoutParser *m_parent;
    Handler m_handler;
    enum LayoutDocEntry::Kind m_kind;
  };

private:
  class StartElementHandlerNavEntry {
  private:
    typedef void (LayoutParser::*Handler)(::LayoutNavEntry::Kind);

    void foobar_B() { (m_parent->*m_handler)(m_kind); }
    class LayoutParser *m_parent;
    Handler m_handler;
    enum LayoutNavEntry::Kind m_kind;
  };
};
