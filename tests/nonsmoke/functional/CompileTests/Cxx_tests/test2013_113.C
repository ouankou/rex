
template<class type> class  QList
   {
     public:
          type *at( int i );
   };

template<class T> class LockingPtr
   {
     public:
          T* operator-> () const;
   };

class GroupList;

class ClassDef
   {
public:
  int getOutputFileBase() const;
  LockingPtr<GroupList> partOfGroups() const;
   };

class GroupList : public QList<ClassDef> {};

int ClassDef::getOutputFileBase() const 
   {
     return partOfGroups()->at(0)->getOutputFileBase();
}
