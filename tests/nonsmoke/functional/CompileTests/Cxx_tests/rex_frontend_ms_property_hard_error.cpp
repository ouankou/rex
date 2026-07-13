struct rex_ms_property_owner {
  int read_value() const;
  void write_value(int);

  __declspec(property(get = read_value, put = write_value)) int value;
};

int rex_read_ms_property(rex_ms_property_owner &owner) { return owner.value; }

void rex_write_ms_property(rex_ms_property_owner &owner, int value) {
  owner.value = value;
}
