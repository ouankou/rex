

class Complex
{
public:
  Complex (const Complex& y);
};
class MatSimplest_Complex
{
protected:
  Complex** A;
  int nrows;
  int ncolumns;
};

typedef Complex (*Func_Complex)(Complex value);

class Mat_Complex : public MatSimplest_Complex
{
public:
void apply (Func_Complex f);
};
void Mat_Complex:: apply (Func_Complex f)
{
   for (int i = 1; i <= nrows; i++)
     for (int j = 1; j <= ncolumns; j++)
       A[i][j] = f(A[i][j]);

}

