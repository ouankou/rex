struct rex_output_leaf {
  bool enabled;
};

class rex_output_iterator {
public:
  rex_output_leaf *operator->() const;
};

class rex_friend_owner {
public:
  friend rex_friend_owner &operator<<(rex_friend_owner &left,
                                      const rex_friend_owner &) {
    return left;
  }
};

template <class T> class rex_output_stream {};

template <class T>
rex_output_stream<T> &operator<<(rex_output_stream<T> &stream, const T &) {
  return stream;
}

void rex_outline_generated_output() {
#pragma rose_outline
  {
    rex_output_iterator iterator;
    bool enabled = iterator->enabled;
    (void)enabled;
  }
  rex_output_stream<int> stream;
  stream << 1;
}
