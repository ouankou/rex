using rex_size_type = decltype(sizeof(0));
void *operator new(rex_size_type, void *) noexcept;
extern "C" void *malloc(rex_size_type);
extern "C" void free(void *);
rex_size_type rex_allocation_size(rex_size_type);
void *rex_allocation_pointer(void *);

void rex_allocation_effects(int &value, rex_size_type &bytes, void *storage) {
  int *placed = new (storage) int(value++);
  int *heap = new int(value++);
  void *raw = malloc(rex_allocation_size(bytes++));
  free(rex_allocation_pointer(raw));
  delete heap;
}
