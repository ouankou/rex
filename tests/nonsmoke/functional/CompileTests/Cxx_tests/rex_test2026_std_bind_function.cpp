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

  CostFunction getCostFunction() const {
    if (cost_function_) {
      return cost_function_;
    }

    return std::bind(&scaledPartitionCost, 1.0, std::placeholders::_1);
  }

private:
  CostFunction cost_function_;
};

} // namespace partition
} // namespace vendor

double computePartitionCost() {
  vendor::partition::UnstructuredBlockPartitioner partitioner;
  return partitioner.getCostFunction()(20);
}
