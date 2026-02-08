#include <functional>

namespace vendor {
namespace partition {

double scaledPartitionCost(double factor, int partitions) {
  return factor * static_cast<double>(partitions);
}

class UnstructuredBlockPartitioner {
public:
  using CostFunction = std::function<double(int)>;

  UnstructuredBlockPartitioner()
      : cost_function_(
            std::bind(&scaledPartitionCost, 0.5, std::placeholders::_1)) {}

  CostFunction getCostFunction() const { return cost_function_; }

private:
  CostFunction cost_function_;
};

} // namespace partition
} // namespace vendor

double computePartitionCost() {
  vendor::partition::UnstructuredBlockPartitioner partitioner;
  return partitioner.getCostFunction()(20);
}
