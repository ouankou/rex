static int exact_type(int &) { return 0; }
static int exact_type(short &) { return 0; }
static int exact_type(unsigned int &) { return 0; }
static int exact_type(long long &) { return 0; }
static int exact_type(long &) { return 1; }

int main() {
  int errors = 0;
  int visits = 0;
#pragma omp parallel for reduction(+ : errors, visits)
  for (int i = -4; i < 4; ++i) {
    int &alias = i;
    errors += exact_type(i) + (alias != i);
    ++visits;
  }
#pragma omp parallel for schedule(static, 2) reduction(+ : errors, visits)
  for (short i = 7; i >= 0; --i) {
    errors += exact_type(i);
    ++visits;
  }
#pragma omp parallel for schedule(dynamic, 2) reduction(+ : errors, visits)
  for (unsigned int i = 0; i < 8; ++i) {
    errors += exact_type(i) + (i - i - 1 < 0);
    ++visits;
  }
#pragma omp parallel for schedule(guided, 2) reduction(+ : errors, visits)
  for (long long i = 2147483648LL; i < 2147483656LL; ++i) {
    errors += exact_type(i);
    ++visits;
  }
  return errors != 0 || visits != 32;
}
