
#if __cplusplus
/* This test code is not defined to work for C++, it is not an error.
   This is a C or C99 code only!
 */
#warning "This is a C++ code!"
#else

#warning "This is not a C++ code!"

/* This is what it takes to make this code compile! */
/* # define __SOCKADDR_ARG		struct sockaddr *__restrict */

typedef union
           {
             struct sockaddr_x25 *__restrict __sockaddr_x25__;
           }
        SOCKADDR_ARG __attribute__ ((__transparent_union__));

extern int accept ( SOCKADDR_ARG __addr);

void pt_accept_cont()
   {
     union { void* buffer; } arg2;
     accept( arg2.buffer );
   }

#endif
