
void foo()
   {
  // Note: this is allowed by legacy frontend, but not by GNU g++ (which
  // requires "loopToHere: ;", the extra ";")
loopToHere: 
   }
