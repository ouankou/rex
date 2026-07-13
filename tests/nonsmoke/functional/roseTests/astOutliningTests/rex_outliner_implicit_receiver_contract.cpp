struct rex_outliner_receiver {
  int value;

  int helper() const { return value; }

  int exercise() const {
    int result = 0;
#pragma rose_outline
    {
      result += helper();
      result += value;
    }
    return result;
  }
};

template <typename T> struct rex_outliner_current_instantiation {
  T value;

  T helper() const { return value; }

  T implicit_receiver() const {
    T result = T();
#pragma rose_outline
    {
      result += helper();
      result += value;
    }
    return result;
  }

  T explicit_receiver() const {
    T result = T();
#pragma rose_outline
    {
      result += this->helper();
      result += this->value;
    }
    return result;
  }
};

template <typename T> struct rex_outliner_receiver_transform {
  static T apply(T value) { return value; }
};

template <typename T, int Extent, template <typename> class Transform>
struct rex_outliner_mixed_current_instantiation {
  T value;

  T helper() const { return Transform<T>::apply(value) + Extent; }

  T implicit_receiver() const {
    T result = T();
#pragma rose_outline
    {
      result += helper();
      result += value;
    }
    return result;
  }
};

template <typename... Types>
struct rex_outliner_type_pack_current_instantiation {
  int value;

  int helper() const { return value; }

  int implicit_receiver() const {
    int result = 0;
#pragma rose_outline
    {
      result += helper();
      result += value;
    }
    return result;
  }
};

template <int... Values>
struct rex_outliner_nontype_pack_current_instantiation {
  int value;

  int helper() const { return value; }

  int implicit_receiver() const {
    int result = 0;
#pragma rose_outline
    {
      result += helper();
      result += value;
    }
    return result;
  }
};

template <template <typename> class... Transforms>
struct rex_outliner_template_pack_current_instantiation {
  int value;

  int helper() const { return value; }

  int implicit_receiver() const {
    int result = 0;
#pragma rose_outline
    {
      result += helper();
      result += value;
    }
    return result;
  }
};

template struct rex_outliner_current_instantiation<int>;
template struct rex_outliner_mixed_current_instantiation<
    int, 2, rex_outliner_receiver_transform>;
template struct rex_outliner_type_pack_current_instantiation<int, short>;
template struct rex_outliner_nontype_pack_current_instantiation<1, 2>;
template struct rex_outliner_template_pack_current_instantiation<
    rex_outliner_receiver_transform, rex_outliner_receiver_transform>;
