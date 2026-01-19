

struct sigaction
  {
    union
      {
	    int sa_handler;
      }
    __sigaction_handler;
    # define sa_handler	__sigaction_handler.sa_handler
};

void _PR_UnixInit(void)
{
   struct sigaction sigact;
   sigact.sa_handler;
}
