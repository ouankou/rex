// This is not legal C++ code (fails for GNU (for good reason), but passes for
// legacy frontend (which is usually more strict).
void foobar() 
   {
     switch (int i = 42)
        {
          default:
        }

     7;
   }
