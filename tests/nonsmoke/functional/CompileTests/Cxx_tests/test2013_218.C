
#include <string>
using namespace std;

class Message {};
class FieldDescriptor;

class GeneratedMessageReflection
   {
     public:
          string GetString(const Message& message,const FieldDescriptor* field) const;

          template <typename Type> inline const Type& GetField(const Message& message,const FieldDescriptor* field) const;
   };



string GeneratedMessageReflection::GetString(const Message& message, const FieldDescriptor* field) const
   {
     switch (42)
        {
          default:
               return *GetField<const string*>(message, field);
        }
   }

