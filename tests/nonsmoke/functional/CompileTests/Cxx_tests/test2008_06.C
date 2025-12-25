// Bug contributed by David Svoboda (CERT)

int puts();

void foo()
   {
     puts();

     // The "(void)" is lost in legacy frontend and so is unavailable in ROSE.
     // Using the "PRESERVE_TOP_LEVEL_CASTS_TO_VOID_IN_IL" switch
     // this should be fixed in legacy frontend 3.10.
     (void) puts();
   }
