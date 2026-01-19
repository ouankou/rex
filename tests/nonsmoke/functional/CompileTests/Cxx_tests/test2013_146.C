
namespace ABC
   {
  // This declaration causes the output of that is presented above.
     struct XIncludeHistoryNode;

     typedef struct XIncludeHistoryNode
        {
          int *URI;
          struct XIncludeHistoryNode *next;
        } XIncludeHistoryNode;

     class X
        {
          public: 
               XIncludeHistoryNode* x;
        };
   }



