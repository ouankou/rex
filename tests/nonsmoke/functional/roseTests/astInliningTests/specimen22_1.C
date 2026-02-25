const int a=10;
int foo(int);

int main(){
   int x=1;
   foo(x);
   return 0;
}

int foo(int x){
  return x*a;
}
