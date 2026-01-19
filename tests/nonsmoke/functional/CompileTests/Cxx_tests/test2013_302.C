// This code will not compile (since the template functions are equivalent).
// See: http://stackoverflow.com/questions/17313649/how-can-i-distinguish-overloads-of-templates-with-non-type-parameters

template<int module>
    void template_const(int &a,int & b){
            a = a & module;
            b = b % module;
    }

    template<bool x>
    void template_const(int &a,int & b){
            int w;
            if (x){
                    w = 123;
            }
            else w = 512;
            a = a & w;
            b = b % w;
    }

void foo()
   {
     int a,b;

   }

