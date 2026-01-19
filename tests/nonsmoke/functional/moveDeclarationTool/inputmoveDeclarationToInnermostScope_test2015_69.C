
// This works
#define MY_MACRO_2(c)                                                          \
  c;                                                                           \
  c;                                                                           \
  c;

void foobar()
   {
     double cc;

        {
       // Either of these will work if the macro is defined with a ending ";".
       // MY_MACRO_2(cc)
          MY_MACRO_2(cc);
        }
   }

