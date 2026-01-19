void foo() {
  if (int i = 0)
    ;

  for (; int b = 1;)
    ;

  for (int b = 1;;)
    ;

  do
    ;
  while (0); // this should work (and does)

  switch (int y = 1)
    ; // { default: ; }

  while (int y = 1)
    ; // { ; }
}
