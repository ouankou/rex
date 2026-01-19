/*

Hi Dan,

Attached is a code (file main.cpp) that builds an AST from a very simple inputfile (file testfile.cpp).
To this AST I try to add a function prototype declaration of "printf" and then a function call to "printf". There are a couple of problems/difficulties with doing this.

More specific comments and questions are in the file itself (main.cpp).

Please let me know if I should add more comments or specify the difficulties more.

Vera



// test file to demonstrate incorrect string argument into the printf function call
// when building the function call in the AST.
// (the printf call will be appended after the return statement. This is not correct. However, nothing
// is done to fix it since that is not of any concern in this case.)


int main(){

  int a;
  int b = a;  // One SgVarRefExp
  return 0;

}

*/
