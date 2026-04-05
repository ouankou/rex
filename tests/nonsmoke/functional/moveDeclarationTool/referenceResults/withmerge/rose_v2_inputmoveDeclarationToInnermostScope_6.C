// For loop with a single statement as body, no basic block

void foo(int value,int *first,int *last)
{
  const int tmp = value;
  for (; first != last; ++first) 
     *first = tmp;
  if ((*first))
    ;
  // test if false body is not created.
}
