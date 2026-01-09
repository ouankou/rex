int classify(int x) {
  if (x == 0)
    x = 1;
  else if (x == 1) {
    x = 2;
  } else
    x = 3;

  return x;
}
