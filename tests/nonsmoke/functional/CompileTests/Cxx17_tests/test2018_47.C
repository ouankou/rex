// Selection statements with initializer

enum status_code { OK, Bad };

struct Args {};

struct status_info {
  status_code code;

  operator status_code() const { return code; }
  const char *message() const { return "Bad status"; }
};

struct Foo {
  explicit Foo(const Args &) {}

  status_info status() const {
    status_info result{OK};
    return result;
  }
  void zip() const {}
};

struct BadFoo {
  explicit BadFoo(const char *) {}
};

const Args args{};

status_code foo() {
  // Keep the sample self-contained so this stays a positive C++17 compile test.
  switch (Foo gadget{args}; auto s = gadget.status()) {
  case OK:
    gadget.zip();
    return OK;
  case Bad:
    throw BadFoo(s.message());
  }

  return Bad;
}
