namespace rex_alignof_contract {

  template <unsigned Size> struct storage {
    struct type {
      unsigned char data[Size];
    };
  };

  template <unsigned Size,
            unsigned Alignment = alignof(typename storage<Size>::type)>
  struct aligned_storage {
    alignas(Alignment) unsigned char data[Size];
  };

  aligned_storage<8> value;

} // namespace rex_alignof_contract
