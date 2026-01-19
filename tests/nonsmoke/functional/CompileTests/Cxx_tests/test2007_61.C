
  // defineing DEFINE_INLINE leads to a ROSE bug:  a SgFunctionDeclaration
  // for spx_alloc with a NULL SgFileInfo.
#define DEFINE_INLINE

template <class T>
void spx_alloc(T& p, int n)
{

}

class SSVector 
{
public:

#ifdef DEFINE_INLINE
  int func() 
  {
      int len = 1;
      int *idx;
      spx_alloc(idx, len);
  }
#else
  func();
#endif

};

#ifndef DEFINE_INLINE
int SSVector::
func() 
{
      int len = 1;
      int *idx;
      spx_alloc(idx, len);
}
#endif
