#include "test2013_256.h"

struct ProtoEntry
   {
     Protocols   protocol;
     unsigned int        defPort;
   };

// Unparsed as: 
// static struct ProtoEntry gProtoList[Protocols_Count] = 
// {
//    {(0)}, 
// };


static ProtoEntry gProtoList[Protocols_Count] =
   {
     { File , 0  }
   };
