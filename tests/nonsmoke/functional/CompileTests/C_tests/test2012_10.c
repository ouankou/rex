
// Note that this must be marked as __transparent_union__
union ZZZ
   {
     int *__restrict __sockaddr__;
   } __attribute__ ((__transparent_union__));

   typedef union ZZZ __SOCKADDR_ARG;

   extern int getsockname(__SOCKADDR_ARG __addr);

   int ngx_set_inherited_sockets() {
     // This will force the cast operation (demonstrated bug).
     int *functionArg;

     // Error: unparses as: "getsockname({.__sockaddr__ = (sockaddr)});"
     // But should be" "getsockname(sockaddr);"
     getsockname(functionArg);

     return 0;
   }
