enum values { x,y };

// Note that this second declaration of an enum (which is meaningless, is
// ignored and not produced in the legacy frontend AST and so not in ROSE
// either).
enum values;

int main()
   {
     values* selectionPtr = 0;

  // Call a vacuious (meaningless) destructor
     selectionPtr->~values();
   }
