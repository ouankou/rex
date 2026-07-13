class stream;

template <class T> class first_box;

template <class T> stream &select(stream &, const first_box<T> &);

template <class T> class first_box {
  friend stream &select<>(stream &, const first_box<T> &);
};

template <class T> class second_box;

template <class T> stream &select(stream &, const second_box<T> &);

template <class T> class second_box {
  friend stream &select<>(stream &, const second_box<T> &);
};
