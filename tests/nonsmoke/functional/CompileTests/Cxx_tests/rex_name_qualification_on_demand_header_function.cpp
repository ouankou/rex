#include "rex_name_qualification_on_demand_header_function.hpp"

bool rex_use_on_demand_header_function() {
  return rex_on_demand_header_function(
             static_cast<const RexOnDemandFunctionNode *>(nullptr)) != nullptr;
}

RexOnDemandFunctionResult *
rex_on_demand_header_function(RexOnDemandFunctionNode *) {
  return nullptr;
}

const RexOnDemandFunctionResult *
rex_on_demand_header_function(const RexOnDemandFunctionNode *) {
  return nullptr;
}
