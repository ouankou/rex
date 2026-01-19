
struct process
   {
     int status;
   };

extern int kill (int __pid, int __sig) __attribute__ ((__nothrow__));

static int
handle_sub(int job, int fg)
   {
     if (0)
        {
          struct process *p;

          for (;;)
            if (0) {
              if (0)
                kill(0, (((__extension__({
                           union {
                             __typeof(p->status) __in;
                             int __i;
                           } __u;
                           __u.__in = (p->status);
                           __u.__i;
                         }))) &
                         0x7f));
              else
                kill(0, (((__extension__({
                           union {
                             __typeof(p->status) __in;
                             int __i;
                           } __u;
                           __u.__in = (p->status);
                           __u.__i;
                         }))) &
                         0x7f));
              kill(0, (((__extension__({
                         union {
                           __typeof(p->status) __in;
                           int __i;
                         } __u;
                         __u.__in = (p->status);
                         __u.__i;
                       }))) &
                       0x7f));
              break;
            }
        }

     return 0;
   }

