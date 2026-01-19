void
ngx_process_get_status(void)
   {

  // Use of "status" as an initializer is a bug.
     int status = 42;
     if ( (((union { __typeof(status) __in; int __i; }) { .__in = (status) }).__i) == 0)
        {
     }
   }
