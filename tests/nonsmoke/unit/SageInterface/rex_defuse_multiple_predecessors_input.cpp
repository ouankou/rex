int rex_join(int selector, int incoming) {
  int result = 0;
  if (selector == 0) {
    result = incoming + 1;
    goto join;
  }
  if (selector == 1) {
    result = incoming + 2;
    goto join;
  }
  result = incoming + 3;
join:
  return result;
}

void rex_update(int *, int *);

int rex_argument_updates() {
  int first = 0;
  int second = 0;
  rex_update(&first, &second);
  return first + second;
}
