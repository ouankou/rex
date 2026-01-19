// Source code position tests.

typedef struct ngx_listening_s ngx_listening_t;

struct sockaddr
  {
//  sa_family_t sa_family;
    char sa_data[14];
  };


typedef int socklen_t;

struct ngx_listening_s {
  // int* sockaddr;
  struct sockaddr *sockaddr;
  // int socklen;
  socklen_t socklen;
};

typedef union { __const struct sockaddr *__restrict __sockaddr__; __const struct sockaddr_at *__restrict __sockaddr_at__; __const struct sockaddr_ax25 *__restrict __sockaddr_ax25__; __const struct sockaddr_dl *__restrict __sockaddr_dl__; __const struct sockaddr_eon *__restrict __sockaddr_eon__; __const struct sockaddr_in *__restrict __sockaddr_in__; __const struct sockaddr_in6 *__restrict __sockaddr_in6__; __const struct sockaddr_inarp *__restrict __sockaddr_inarp__; __const struct sockaddr_ipx *__restrict __sockaddr_ipx__; __const struct sockaddr_iso *__restrict __sockaddr_iso__; __const struct sockaddr_ns *__restrict __sockaddr_ns__; __const struct sockaddr_un *__restrict __sockaddr_un__; __const struct sockaddr_x25 *__restrict __sockaddr_x25__;
} __CONST_SOCKADDR_ARG __attribute__((__transparent_union__));

// extern int bind (int __fd, __CONST_SOCKADDR_ARG __addr, socklen_t __len) __attribute__ ((__nothrow__));
// extern int bind (int __fd, int* __addr, int __len) __attribute__ ((__nothrow__));
extern int bind (int __fd, __CONST_SOCKADDR_ARG __addr, int __len) __attribute__ ((__nothrow__));

void foobar()
   {
     int s;
     int i;
     ngx_listening_t *ls;

  // if (bind(s, ls[i].sockaddr, ls[i].socklen) == -1) 
     if (bind(s, ls[i].sockaddr, 42) == -1) 
        {
        }
   }
