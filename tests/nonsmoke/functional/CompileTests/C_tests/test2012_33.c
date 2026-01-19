void foobar()
   {
  // Unparses to: static char buf[sizeof(char [])];
  static char buf[sizeof(".xxx.xxx.xxx.xxx")];
  // int constantFoldedValue = 1 + 2;
   }
