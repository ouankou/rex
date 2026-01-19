//--------------- input code
//cat /home/liao6/workspace/raja/templateInstantiation/template1.cpp

template<typename T> 
T GetMax (T a, T b) {
  T result;
  result = (a>b)? a : b;
  return (result);
}

void foo(int i, int j)
{
 int  k=GetMax<int>(i,j);
}
