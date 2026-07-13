struct rex_historical_base {
  virtual ~rex_historical_base() = default;
  int inherited() const { return 1; }
};

struct rex_historical_derived : rex_historical_base {};

struct rex_left_base {
  virtual ~rex_left_base() = default;
  int left = 2;
};

struct rex_adjusted_base {
  virtual ~rex_adjusted_base() = default;
  int inherited() const { return 3; }
  int base = 4;
};

struct rex_multiply_derived : rex_left_base, rex_adjusted_base {
  int derived = 5;
};

struct rex_virtual_base {
  virtual ~rex_virtual_base() = default;
  int inherited() const { return 6; }
};

struct rex_virtual_derived : rex_left_base, virtual rex_virtual_base {
  int own = 7;
};

struct rex_conversion_holder {
  rex_historical_derived historical_values[2];

  int exercise(rex_multiply_derived &derived, rex_adjusted_base &base,
               rex_virtual_derived &virtual_derived,
               rex_virtual_base &virtual_base) {
    int result = 0;
#pragma rose_outline
    {
      // This is the exact class-lvalue UncheckedDerivedToBase shape from
      // test_61_2020.cpp. The old late outliner repair deleted this cast node.
      result += historical_values[0].inherited();

      // The adjusted base is deliberately not the first base subobject.
      result += derived.inherited();
      rex_adjusted_base *adjusted_up = &derived;
      result += static_cast<rex_multiply_derived &>(base).derived;

      // Exercise both up- and down-casts through virtual inheritance. A static
      // down-cast through a virtual base is ill-formed, so the checked
      // down-cast is necessarily dynamic.
      rex_virtual_base *virtual_up = &virtual_derived;
      result += virtual_derived.inherited();
      rex_virtual_derived &virtual_down =
          dynamic_cast<rex_virtual_derived &>(virtual_base);
      result += virtual_down.own;

      (void)adjusted_up;
      (void)virtual_up;
    }
    return result;
  }
};
