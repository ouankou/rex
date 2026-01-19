// This is a copy of test2013_85.C (so that we can work on a simpler version).
// Note also that test2013_85.C is a copy of test2013_84.C (only much simpler).

struct LayoutDocEntry
   {
  // enum Kind { MemberGroups };
     typedef float Kind;
   };

struct LayoutNavEntry
   {
     public:
       // enum Kind { MainPage };
          typedef int Kind;
   };

// class LayoutParser {
     class StartElementHandlerSection
        {
          private:
            // typedef void (LayoutParser::*Handler)(LayoutDocEntry::Kind kind);
            typedef void (*Handler)(LayoutDocEntry::Kind kind);
        };

     class StartElementHandlerNavEntry
        {
          private:
            // typedef void (LayoutParser::*Handler)(LayoutNavEntry::Kind kind);
            typedef void (*Handler)(LayoutNavEntry::Kind kind);
        };

//   };

