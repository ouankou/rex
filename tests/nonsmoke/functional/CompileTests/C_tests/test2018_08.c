
void foobar() {
#pragma STDC FENV_ACCESS ON
#pragma STDC FP_CONTRACT ON
#pragma STDC FP_CONTRACT OFF
  int n = 0;
  int x = 0;
  {
#pragma STDC FP_CONTRACT OFF
    int y = 0;
  }
}
