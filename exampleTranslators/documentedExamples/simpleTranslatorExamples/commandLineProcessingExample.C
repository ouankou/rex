
int
main ( int argc, char* argv[] )
   {
  // Simple command line option --h or --help
  // Use 1 at end of argument list to SLA to force removal of option from argv and decrement of agrc
     int optionCount = sla(&argc, argv, "--", "($)", "(h|help)",1);
     if( optionCount > 0 )
        {
       printf("\nROSE (pre-release alpha version: %s) \n",
              ROSE_PACKAGE_VERSION);
       Rose::usage();
       exit(0);
        }

  // option with parameter to option
     int integerOption = 0;
  // Use 1 at end of argument list to SLA to force removal of option from argv and decrement of agrc
     optionCount =
         sla(&argc, argv, "-rose:", "($)^", "verbose", &integerOption, 1);
     if( optionCount > 0 )
        {
          switch (integerOption)
             {
               case 0 :
                 // do something with "-rose:verbose 0" option
                 break;
               case 1 :
                 // do something with "-rose:verbose 1" option
                 break;
               default:
             }
        }
   }

.
