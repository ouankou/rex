class QList 
   {
     public:
          QList & operator=(const QList &list);
   };

class QGDItList : public QList
   {
     public:
          QGDItList &operator=(const QList &list)
	          {
            // This unparses to: "return  = list;"
            // Original code: "return (QGDItList&) QList::operator= ( list );"
               return (QGDItList&) QList::operator= ( list );
             }
   };

void foobar()
   {
     QGDItList x,y;
     x = y;
}
