#include "sage3basic.h"

void SgNode::clearMemoryPool() {
  auto& pools = allPools();
  for (const auto& pool : pools) {
    if (pool.base) {
      ROSE_FREE(pool.base);
    }
    if (pool.reset) {
      pool.reset();
    }
  }
  std::vector<SgNode::PoolEntry>().swap(pools);
  releaseAllPools();
}

void SgNode::deleteMemoryPool() {
  clearMemoryPool();
}
