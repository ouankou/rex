
#include <vector>

class dsl_attribute 
   {
     public:
          dsl_attribute();
          dsl_attribute(const dsl_attribute & X);
   };

// std::vector<dsl_attribute> abc1 = { dsl_attribute };  // Error in frontend
std::vector<dsl_attribute> abc2 = { dsl_attribute() }; // Correct code
std::vector<dsl_attribute> abc3 = { dsl_attribute{} }; // Correct code, try to figure out the difference.
std::vector<dsl_attribute> abc4 = { dsl_attribute{dsl_attribute{}} }; // Strange but correct.

std::vector<dsl_attribute> abc5 = { dsl_attribute() }; // Correct code
std::vector<dsl_attribute> abc6 = {}; // Correct code
