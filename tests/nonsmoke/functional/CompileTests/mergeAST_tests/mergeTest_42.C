struct wait
   {
     struct {} __wait_terminated;
     struct { int x; };
  };

  struct wait_container {
    struct {
    } __wait_terminated;
    struct {
      int x;
    };
  };

struct wait_alt
   {
     enum { X };
     enum { Y };
};
