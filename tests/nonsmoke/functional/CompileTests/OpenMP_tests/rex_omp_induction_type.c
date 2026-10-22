/* Runtime scheduler bounds must not change the source induction type. */
int main(void) {
  int errors = 0;
  int visits = 0;
  typedef int rex_index;
#pragma omp parallel for reduction(+ : errors, visits)
  for (rex_index i = -4; i < 4; ++i) {
    errors += sizeof(i) != sizeof(rex_index);
    ++visits;
  }
#pragma omp parallel for schedule(static, 2) reduction(+ : errors, visits)
  for (short i = 7; i >= 0; --i) {
    errors += sizeof(i) != sizeof(short);
    ++visits;
  }
#pragma omp parallel for schedule(dynamic, 2) reduction(+ : errors, visits)
  for (unsigned int i = 0; i < 8; ++i) {
    errors += sizeof(i) != sizeof(unsigned int) || (i - i - 1 < 0);
    ++visits;
  }
#pragma omp parallel for schedule(guided, 2) reduction(+ : errors, visits)
  for (long long i = 2147483648LL; i < 2147483656LL; ++i) {
    errors += sizeof(i) != sizeof(long long);
    ++visits;
  }
  return errors != 0 || visits != 32;
}
