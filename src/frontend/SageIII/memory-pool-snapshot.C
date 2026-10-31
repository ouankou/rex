
#include "memory-pool-snapshot.h"

#include "sage3basic.h"

#include <unordered_map>

namespace Rose {

namespace AST {

namespace utils {
template <typename... Ts> struct NodeList;

template <typename N, typename... Ns> struct NodeList<N, Ns...> {
  template <typename Functor> static inline void apply(Functor &f) {
    NodeList<N>::template apply<Functor>(f);
    NodeList<Ns...>::template apply<Functor>(f);
  }
};

template <typename Node> struct NodeList<Node> {
  template <typename Functor> static inline void apply(Functor &f) {
    f.template apply<Node>();
  }
};
} // namespace utils
} // namespace AST

namespace MemPool {

struct DumpPoolInfo {
  std::ostream &out;

  explicit DumpPoolInfo(std::ostream &out_) : out(out_) {
    out << "kind,pool,offset,address,allocated,next_free\n";
  }

  template <typename N> void apply() {
    ROSE_ASSERT(N::pool_live_flags.size() == N::pools.size());
    auto slot_address = [](size_t slot) -> const void * {
      return N::pools[slot / N::pool_size] + slot % N::pool_size * sizeof(N);
    };
    // A free slot's successor is the one allocated after it.
    std::unordered_map<size_t, const void *> next_free_slots;
    for (size_t i = 0; i < N::free_slots.size(); i++)
      next_free_slots.emplace(N::free_slots[i],
                              i == 0 ? nullptr
                                     : slot_address(N::free_slots[i - 1]));
    for (size_t p = 0; p < N::pools.size(); p++) {
      ROSE_ASSERT(N::pool_live_flags[p].size() == N::pool_size);
      for (size_t o = 0; o < N::pool_size; o++) {
        const void *slot = N::pools[p] + o * sizeof(N);
        const bool allocated = N::pool_live_flags[p][o] != 0;
        const auto free = next_free_slots.find(p * N::pool_size + o);
        ROSE_ASSERT(allocated == (free == next_free_slots.end()));
        const void *next_free = allocated ? nullptr : free->second;
        out << std::dec << N::static_variant << ',' << p << ',' << o << ','
            << slot << ',' << static_cast<unsigned>(allocated) << ','
            << next_free << '\n';
      }
    }
  }
};

using Nodes = AST::utils::NodeList<
    SgAliasSymbol, SgDirectoryList, Sg_File_Info, SgFileList,
    SgFunctionTypeSymbol, SgFunctionTypeTable, SgGlobal, SgInitializedName,
    SgProject, SgSourceFile, SgStorageModifier, SgSupport, SgSymbolTable,
    SgTypeDefault, SgTypeInt, SgTypeTable, SgVariableDeclaration,
    SgVariableDefinition, SgVariableSymbol>;

void snapshot(std::ostream &out) {
  DumpPoolInfo f(out);
  Nodes::apply(f);
}

void snapshot(std::string const &outfilename) {
  std::ofstream ofile(outfilename);
  snapshot(ofile);
}

} // namespace MemPool
} // namespace Rose
