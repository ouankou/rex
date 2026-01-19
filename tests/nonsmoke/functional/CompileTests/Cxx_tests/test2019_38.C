struct line 
   {
  // union xxxxxx
     union
        {
          unsigned serial;
        };
   };

void foo()
   {
     struct line *b;
     b->serial;
}
