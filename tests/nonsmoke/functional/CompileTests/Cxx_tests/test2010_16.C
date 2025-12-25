// Interesting bug report (same as C_tests test2010_02.c).
// This is a strictness of legacy frontend whereas GNU allows this (at least for
// C++).
int main()
{
  return main();
}
