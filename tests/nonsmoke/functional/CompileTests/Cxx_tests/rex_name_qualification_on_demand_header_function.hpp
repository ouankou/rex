#pragma once

struct RexOnDemandFunctionNode {};
struct RexOnDemandFunctionResult {};

RexOnDemandFunctionResult *
rex_on_demand_header_function(RexOnDemandFunctionNode *);
const RexOnDemandFunctionResult *
rex_on_demand_header_function(const RexOnDemandFunctionNode *);
