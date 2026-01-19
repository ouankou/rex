// This test code does NOT compile wth GNU 6.1

template<class...Ty> 
int f_(Ty...a) 
   {
     auto d = [&, a...]() mutable [[]] -> int 
        {
          return sizeof...(a); 
	};
     return d();
   }

void foobar()
   {
     int i;
     i = f_();
     i = f_(1);
     i = f_(2, 2.0);
   }

