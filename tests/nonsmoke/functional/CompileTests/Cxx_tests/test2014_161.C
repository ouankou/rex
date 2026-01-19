template <typename T>
class HistVarSet 
   {
public:
  // HistVarSet (int x) {}
  HistVarSet(int x);
  void foo(HistVarSet<bool> flags);
  // HistVarSet<bool> flags;
   };

HistVarSet<bool> log_flags(7);

void foobar() { HistVarSet<bool> log_flags(7); }
