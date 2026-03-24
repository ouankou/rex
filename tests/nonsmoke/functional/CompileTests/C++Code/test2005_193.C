

struct super {
  virtual int operator[](const char *) = 0;
};

struct dummy : public super {
  int operator[](const char *lala) override { return lala != nullptr ? 0 : 1; }
};

int main() { dummy()["Kuh"]; }
