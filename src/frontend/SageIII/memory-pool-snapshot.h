#ifndef __ROSE_MEMPOOL_SNAPSHOT_H__
#define __ROSE_MEMPOOL_SNAPSHOT_H__

#include <ostream>

#include <string>

namespace Rose {
namespace MemPool {

void snapshot(std::ostream &);
void snapshot(std::string const &);

} // namespace MemPool
} // namespace Rose

#endif /* __ROSE_MEMPOOL_SNAPSHOT_H__ */
