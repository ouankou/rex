/**
 *  \file NonLocalDecls.cc
 *
 *  \brief This preprocessing phase collects local function
 *  declarations into a to-be-outlined basic block.
 */
#include "sage3basic.h"

#include <map>

#include "Preprocess.hh"

void Outliner::Preprocess::gatherNonLocalDecls(SgBasicBlock *b) {
  ROSE_ASSERT(b != nullptr);
  std::map<SgFunctionSymbol *, SgFunctionSymbol *> bindings;
  for (SgNode *node : NodeQuery::querySubTree(b, V_SgFunctionRefExp)) {
    SgFunctionRefExp *ref = isSgFunctionRefExp(node);
    ROSE_ASSERT(ref);
    SgFunctionSymbol *sym = ref->get_symbol();
    ROSE_ASSERT(sym);
    SgFunctionDeclaration *decl = sym->get_declaration();
    ROSE_ASSERT(decl);

    SgBasicBlock *owner = isSgBasicBlock(decl->get_parent());
    if (owner == nullptr || owner == b)
      continue;
    auto binding = bindings.find(sym);
    if (binding == bindings.end()) {
      auto *anchor =
          b->get_statements().empty() ? nullptr : b->get_statements().front();
      auto *copy = copyNonLocalFunctionDeclaration(decl, b, anchor);
      auto *copiedSymbol =
          isSgFunctionSymbol(copy->get_symbol_from_symbol_table());
      ROSE_ASSERT(copiedSymbol != nullptr && copiedSymbol != sym &&
                  copiedSymbol->get_declaration() == copy &&
                  copiedSymbol->get_parent() == b->get_symbol_table());
      binding = bindings.emplace(sym, copiedSymbol).first;
    }
    ref->set_symbol(binding->second);
  }
}
