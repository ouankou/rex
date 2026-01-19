
void minBox (int a);

class IntVectSet
   {
     public:
          IntVectSet& operator|=(const IntVectSet& ivs);

          int minBox() const;
};

IntVectSet& IntVectSet::operator|=(const IntVectSet& ivs)
   {
     ::minBox(42);

     return *this;
   }
