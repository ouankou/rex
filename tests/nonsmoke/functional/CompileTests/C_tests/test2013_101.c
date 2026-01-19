

enum XXXYYYZZZ 
   {
     LSA_LEN_SIZE = 0, // offsetof(len_and_sockaddr, u),
     LSA_SIZEOF_SA = sizeof(
          union
             {
               int x;
               long y;
             } ),
     LAST_ENUM = 42
   } XXX;


void foo()
   {
     enum XXXYYYZZZ value = LAST_ENUM;
   }
