
#include <vector>

class dsl_attribute 
   {
     public:
          dsl_attribute();
          dsl_attribute(const dsl_attribute & X);
   };

// std::vector<dsl_attribute> abc = { dsl_attribute };  // Error in frontend
// std::vector<dsl_attribute> abc = { dsl_attribute() }; // Correct code
// std::vector<dsl_attribute> abc = { dsl_attribute{} }; // Correct code, try to figure out the difference.
std::vector<dsl_attribute> abc = { dsl_attribute{dsl_attribute{}} }; // Strange but correct.
