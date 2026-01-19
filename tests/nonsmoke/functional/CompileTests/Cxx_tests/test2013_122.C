template<class type> class QListIterator
   {
     public:
          type *operator++();
   };

   class DotNode {};

   void NEW_write() {
     // QList<DotNode> *nl = 0L;
     // QListIterator<DotNode>  dnli1(*nl);
     QListIterator<DotNode>  dnli1;
  // QListIterator dnli1;
  // for ( ; ; ++dnli1,++dnli1)
     for ( ; ; ++dnli1,++dnli1)
        {
        }
   }

