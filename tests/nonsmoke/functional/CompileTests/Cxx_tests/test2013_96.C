
class QCString {};

class TagAnchorInfo
   {
     public:
       // The support for matching declarations checks that the line numbers are
       // the same as the declaration and this is false if the thrid parameter
       // is on another line. This version will cause an error.
       TagAnchorInfo(const QCString &t = QCString()) {}
   };

void foobar()
   {
     TagAnchorInfo* tag = new TagAnchorInfo();
   }
