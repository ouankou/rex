void write_int(int *p) { *p = 1; }

void read_int(const int *p) { (void)*p; }

void write_ref(int &r) { r = 2; }

void test_address_of_casts() {
  int a = 0;
  int b = 0;
  int c = 0;

  write_int((int *)&a);
  write_int((int *)((void *)&b));
  read_int((const int *)&c);
  write_ref(c);
}
