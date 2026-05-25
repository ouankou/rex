#include "sage3basic.h"

#include "fixupCxxSymbolTablesToSupportAliasingSymbols.h"

#include <string>
#include <unordered_map>
#include <unordered_set>

#define ALIAS_SYMBOL_DEBUGGING 0

#define USING_PERFORMANCE_TRACING 0

namespace {
struct AliasSymbolIdentity {
  std::string name;
  SgNode *basis = NULL;
  int variant = 0;

  bool operator==(const AliasSymbolIdentity &other) const {
    return basis == other.basis && variant == other.variant &&
           name == other.name;
  }
};

struct AliasSymbolIdentityHash {
  size_t operator()(const AliasSymbolIdentity &identity) const {
    size_t seed = std::hash<std::string>()(identity.name);
    seed ^= std::hash<SgNode *>()(identity.basis) + 0x9e3779b9 + (seed << 6) +
            (seed >> 2);
    seed ^= std::hash<int>()(identity.variant) + 0x9e3779b9 + (seed << 6) +
            (seed >> 2);
    return seed;
  }
};

SgSymbol *stripAliasSymbol(SgSymbol *symbol) {
  ROSE_ASSERT(symbol != NULL);

  SgSymbol *non_alias_symbol = symbol;
  while (isSgAliasSymbol(non_alias_symbol) != NULL) {
    non_alias_symbol = isSgAliasSymbol(non_alias_symbol)->get_alias();
    ROSE_ASSERT(non_alias_symbol != NULL);
  }

  return non_alias_symbol;
}

AliasSymbolIdentity buildAliasSymbolIdentity(const SgName &name,
                                             SgSymbol *symbol) {
  ROSE_ASSERT(symbol != NULL);

  SgSymbol *non_alias_symbol = stripAliasSymbol(symbol);
  SgNode *basis = symbol->get_symbol_basis();
  ROSE_ASSERT(basis != NULL);

  AliasSymbolIdentity identity;
  identity.name = name.getString();
  identity.basis = basis;
  identity.variant = static_cast<int>(non_alias_symbol->variantT());
  return identity;
}
} // namespace

struct FixupAstSymbolTablesToSupportAliasedSymbols::CurrentScopeAliasIndex {
  std::unordered_set<AliasSymbolIdentity, AliasSymbolIdentityHash>
      symbolIdentities;
  std::unordered_map<AliasSymbolIdentity, SgAliasSymbol *,
                     AliasSymbolIdentityHash>
      aliasSymbols;

  explicit CurrentScopeAliasIndex(SgScopeStatement *currentScope) {
    ROSE_ASSERT(currentScope != NULL);
    SgSymbolTable *currentSymbolTable = currentScope->get_symbol_table();
    ROSE_ASSERT(currentSymbolTable != NULL);
    SgSymbolTable::BaseHashType *currentInternalTable =
        currentSymbolTable->get_table();
    ROSE_ASSERT(currentInternalTable != NULL);

    symbolIdentities.reserve(currentInternalTable->size() * 2 + 1);
    aliasSymbols.reserve(currentInternalTable->size());
    for (const std::pair<const SgName, SgSymbol *> &entry :
         *currentInternalTable) {
      ROSE_ASSERT(entry.second != NULL);
      AliasSymbolIdentity identity =
          buildAliasSymbolIdentity(entry.first, entry.second);
      symbolIdentities.insert(identity);
      if (SgAliasSymbol *aliasSymbol = isSgAliasSymbol(entry.second)) {
        aliasSymbols.emplace(std::move(identity), aliasSymbol);
      }
    }
  }
};

void fixupAstSymbolTablesToSupportAliasedSymbols(SgNode *node) {
  // DQ (8/16/2020): This function is called for Fortran, and yet it has been
  // modified to be more specific to C++ rules.

  // DQ (4/14/2010): For Cxx only.
  // Adding support for symbol aliasing as a result of using declarations
  // (and other use directives, etc.).

  TimingPerformance timer1("Fixup symbol tables to support aliased symbols:");

  // Now fixup the local symbol tables
  // This simplifies how the traversal is called!
  FixupAstSymbolTablesToSupportAliasedSymbols astFixupTraversal;

  // DQ (4/17/2010): Comment this new option out for now while I focus on
  // getting the language only configure options into place.

#if ALIAS_SYMBOL_DEBUGGING || 0
  printf("########################## Inside of "
         "fixupAstSymbolTablesToSupportAliasedSymbols(node = %p = %s) \n",
         node, node->class_name().c_str());
#endif

  // I think the default should be preorder so that the interfaces would be more
  // uniform
  astFixupTraversal.traverse(node, preorder);

  // DQ (1/23/2019): The set maintained in
  // SgSymbolTable::get_aliasSymbolCausalNodeSet() should likely be cleared
  // after use in this function.
#if ALIAS_SYMBOL_DEBUGGING
  printf("In fixupAstSymbolTablesToSupportAliasedSymbols(): "
         "SgSymbolTable::get_aliasSymbolCausalNodeSet().size() = %zu (should "
         "be cleared) \n",
         SgSymbolTable::get_aliasSymbolCausalNodeSet().size());
#endif
  SgSymbolTable::clear_aliasSymbolCausalNodeSet();
  ROSE_ASSERT(SgSymbolTable::get_aliasSymbolCausalNodeSet().empty() == true);
}

// DQ (1/21/2019): Added to support Cxx_tests/test2019_21.C (symbol aliasing of
// data member in private base class of base class of derived class).
bool FixupAstSymbolTablesToSupportAliasedSymbols::
    isDefinedThroughPrivateBaseClass(SgClassDeclaration *classDeclaration,
                                     SgSymbol *symbol) {
  // DQ (1/22/2019): This function is only posed on a single class declaration,
  // not a chain of base blasses.
#if USING_PERFORMANCE_TRACING
  TimingPerformance timer1(
      "Fixup symbol tables: isDefinedThroughPrivateBaseClass:");
#endif

  bool returnValue = false;

  ROSE_ASSERT(classDeclaration != NULL);
  ROSE_ASSERT(symbol != NULL);

  // The symbol is an element of a base class only if it is an SgAliasSymbol.
  SgAliasSymbol *aliasSymbol = isSgAliasSymbol(symbol);
  if (aliasSymbol != NULL) {

    // SgNodePtrList causal_nodes;
    SgNodePtrList &causalNodeList = aliasSymbol->get_causal_nodes();
    ROSE_ASSERT(causalNodeList.empty() == false);

    SgNode *causalNode = causalNodeList[0];
    ROSE_ASSERT(causalNode != NULL);

    symbol = aliasSymbol->get_base();

    ROSE_ASSERT(isSgAliasSymbol(symbol) == NULL);

    printf("Exiting as a test! \n");
    ROSE_ABORT();
  }

  printf("Exiting as a test in function scope! \n");
  ROSE_ABORT();

  printf("Leaving isDefinedThroughPrivateBaseClass(): returnValue = %s \n",
         returnValue ? "true" : "false");

  return returnValue;
}

// DQ (8/23/2011): Made this a static function so that it can be called in
// additional contexts.
void FixupAstSymbolTablesToSupportAliasedSymbols::
    injectSymbolsFromReferencedScopeIntoCurrentScope(
        SgScopeStatement *referencedScope, SgScopeStatement *currentScope,
        SgNode *causalNode, SgAccessModifier::access_modifier_enum accessLevel,
        bool calledFromUsingDirective) {
  CurrentScopeAliasIndex currentScopeAliasIndex(currentScope);
  injectSymbolsFromReferencedScopeIntoCurrentScope(
      referencedScope, currentScope, causalNode, accessLevel,
      calledFromUsingDirective, currentScopeAliasIndex);
}

void FixupAstSymbolTablesToSupportAliasedSymbols::
    injectSymbolsFromReferencedScopeIntoCurrentScope(
        SgScopeStatement *referencedScope, SgScopeStatement *currentScope,
        SgNode *causalNode, SgAccessModifier::access_modifier_enum accessLevel,
        bool calledFromUsingDirective,
        CurrentScopeAliasIndex &currentScopeAliasIndex) {
#if USING_PERFORMANCE_TRACING
  TimingPerformance timer1(
      "Fixup symbol tables: injectSymbolsFromReferencedScopeIntoCurrentScope: "
      "whole function");
#endif

  ROSE_ASSERT(referencedScope != NULL);
  ROSE_ASSERT(currentScope != NULL);

#if ALIAS_SYMBOL_DEBUGGING || 0
  printf("*********************************************************************"
         "********** \n");
  printf("In injectSymbolsFromReferencedScopeIntoCurrentScope(): "
         "referencedScope = %p = %s currentScope = %p = %s accessLevel = %d \n",
         referencedScope, referencedScope->class_name().c_str(), currentScope,
         currentScope->class_name().c_str(), accessLevel);
  printf("   --- referencedScope = %s \n",
         SageInterface::get_name(referencedScope).c_str());
  printf("   --- currentScope = %s \n",
         SageInterface::get_name(currentScope).c_str());
  printf("   --- causalNode = %s \n",
         SageInterface::get_name(causalNode).c_str());
  printf("*********************************************************************"
         "********** \n");
#endif

  SgSymbolTable *symbolTable = referencedScope->get_symbol_table();
  ROSE_ASSERT(symbolTable != NULL);

  SgClassDefinition *classDefinition = isSgClassDefinition(referencedScope);
  if (classDefinition != NULL) {
    // If this is a class definition, then we need to make sure that we only for
    // alias symbols for those declarations.
#if ALIAS_SYMBOL_DEBUGGING
    printf("Injection of symbols from a class definition needs to respect "
           "access priviledge (private, protected, public) declarations \n");
#endif
  }

#if ALIAS_SYMBOL_DEBUGGING
  printf("Adding causalNode = %p = %s name = %s to "
         "SgSymbolTable::get_aliasSymbolCausalNodeSet() \n",
         causalNode, causalNode->class_name().c_str(),
         SageInterface::get_name(causalNode).c_str());
#endif

  // DQ (7/14/2025): Adding timers to support Matt's tool.
  {
#if USING_PERFORMANCE_TRACING
    TimingPerformance timer1(
        "Fixup symbol tables: "
        "injectSymbolsFromReferencedScopeIntoCurrentScope: "
        "get_aliasSymbolCausalNodeSet().find(causalNode):");
#endif

    SgSymbolTable::insert_aliasSymbolCausalNodeSet(causalNode);

#if OBSOLETE_1
    // DQ (1/23/2019): Also need to add this to the aliasSymbolCausalNodeSet.
    if (SgSymbolTable::get_aliasSymbolCausalNodeSet().find(causalNode) ==
        SgSymbolTable::get_aliasSymbolCausalNodeSet().end()) {
      SgSymbolTable::insert_aliasSymbolCausalNodeSet(causalNode);

#if ALIAS_SYMBOL_DEBUGGING
      printf("@@@@@@@@@@@@ Inserted causalNode = %p into "
             "SgSymbolTable::get_aliasSymbolCausalNodeSet().size() = %zu \n",
             causalNode, SgSymbolTable::get_aliasSymbolCausalNodeSet().size());
      SgBaseClass *baseClass = isSgBaseClass(causalNode);
      if (baseClass != NULL) {
        SgClassDeclaration *baseClassDeclaration = baseClass->get_base_class();
        ROSE_ASSERT(baseClassDeclaration != NULL);

        SgClassDefinition *derivedClassDefinition =
            isSgClassDefinition(currentScope);
        ROSE_ASSERT(derivedClassDefinition != NULL);
        SgClassDeclaration *derivedClassDeclaration =
            derivedClassDefinition->get_declaration();
        ROSE_ASSERT(derivedClassDeclaration != NULL);

        printf(" --- Adding base class %s to derived class %s \n",
               baseClassDeclaration->get_name().str(),
               derivedClassDeclaration->get_name().str());
      }
#endif
    }
#endif /* OBSOLETE_1 */

    // DQ (7/14/2025): Adding timers to support Matt's tool.
  }

  SgSymbolTable::BaseHashType *internalTable = symbolTable->get_table();
  ROSE_ASSERT(internalTable != NULL);

  SgBaseClass *causalBaseClass = isSgBaseClass(causalNode);
  SgScopeStatement *causalBaseScope = NULL;
  if (causalBaseClass != NULL) {
    SgNonrealBaseClass *nrBaseClass = isSgNonrealBaseClass(causalBaseClass);
    if (nrBaseClass != NULL) {
      SgNonrealDecl *baseNonrealDeclaration =
          nrBaseClass->get_base_class_nonreal();
      ROSE_ASSERT(baseNonrealDeclaration != NULL);
      causalBaseScope = baseNonrealDeclaration->get_nonreal_decl_scope();
    } else {
      SgClassDeclaration *baseClassDeclaration =
          causalBaseClass->get_base_class();
      ROSE_ASSERT(baseClassDeclaration != NULL);
      SgClassDeclaration *definingBaseClassDeclaration =
          isSgClassDeclaration(baseClassDeclaration->get_definingDeclaration());
      ROSE_ASSERT(definingBaseClassDeclaration != NULL);
      causalBaseScope = definingBaseClassDeclaration->get_definition();
    }
    ROSE_ASSERT(causalBaseScope != NULL);
  }

  // DQ (7/14/2025): Adding timers to support Matt's tool.
  {
#if USING_PERFORMANCE_TRACING
    TimingPerformance timer1(
        "Fixup symbol tables: "
        "injectSymbolsFromReferencedScopeIntoCurrentScope: total for loop:");
#endif

    // DQ (7/16/2025): Calling performance counters in AstPerformance (static
    // data members).
    AstPerformance::
        numberOfCallsToInjectSymbolsFromReferencedScopeIntoCurrentScope++;

    SgSymbolTable::hash_iterator i = internalTable->begin();
    while (i != internalTable->end()) {
      // DQ (7/16/2025): Calling performance counters in AstPerformance (static
      // data members).
      AstPerformance::numberOfSymbolsCopiedIntoAliasSymbols++;

      ROSE_ASSERT((*i).first.str() != NULL);
      ROSE_ASSERT(isSgSymbol((*i).second) != NULL);

#if ALIAS_SYMBOL_DEBUGGING
      printf("Symbol number: %d (pair.first (SgName) = %s) pair.second "
             "(SgSymbol) class_name() = %s \n",
             counter, (*i).first.str(), (*i).second->class_name().c_str());
#endif
      SgName name = (*i).first;
      SgSymbol *symbol = (*i).second;

      ROSE_ASSERT(symbol != NULL);

      // DQ (1/22/2019): Save a copy of the symbol before we resolved it to the
      // unaliased base.
      SgSymbol *original_symbol = symbol;

      // Make sure that this is not a SgLabelSymbol, I think these should not be
      // aliased (if only because I don't think that C++ support name
      // qualification for labels).
      ROSE_ASSERT(isSgLabelSymbol(symbol) == NULL);

      // DQ (7/19/2025): This shold better capture what we expect is true, that
      // there are no chains of SgAlias symbols.
      ROSE_ASSERT(
          (isSgAliasSymbol(symbol) == NULL) ||
          (isSgAliasSymbol(isSgAliasSymbol(symbol)->get_alias()) == NULL));

      // DQ (6/22/2011): For now skip the handling of alias symbol from other
      // scopes.
      if (isSgAliasSymbol(symbol) != NULL) {
        // DQ (7/14/2025): Adding timers to support Matt's tool.
        {
          // TimingPerformance timer1 ("Fixup symbol tables:
          // injectSymbolsFromReferencedScopeIntoCurrentScope: process alias
          // symbols:");

#if ALIAS_SYMBOL_DEBUGGING
          printf("WARNING: Not clear if we want to nest SgAliasSymbol inside "
                 "of SgAliasSymbol \n");
#endif

          // DQ (7/18/2025): Test if these chains can exist. If not then we can
          // have an assertion about this and simplify this code.
          size_t counter = 0;
          // DQ (9/22/2012): We need to avoid building chains of SgAliasSymbol
          // (to simplify the representation in the AST).
          while (isSgAliasSymbol(symbol) != NULL) {
#if ALIAS_SYMBOL_DEBUGGING
            printf(" --- Iterating to root of alias: symbol = %p = %s \n",
                   symbol, symbol->class_name().c_str());
#endif
            symbol = isSgAliasSymbol(symbol)->get_alias();
            ROSE_ASSERT(symbol != NULL);

            counter++;
          }

#if ALIAS_SYMBOL_DEBUGGING
          printf("Resolved aliased symbol to root symbol: symbol = %p = %s \n",
                 symbol, symbol->class_name().c_str());
#endif
          // DQ (7/18/2025): Test if these chains can exist. If not then we can
          // have an assertion about this and simplify this code.
          if (counter > 1) {
            printf("Found case of a chain of SgAliasSymbol IR nodes, which "
                   "should not be allowed \n");
            ROSE_ASSERT(false);
          }

          // DQ (7/14/2025): Adding timers to support Matt's tool.
        }
      }

      // DQ (7/15/2025): variables moved outside  of the performance monitoring.
      SgNode *symbolBasis = NULL;
      SgAccessModifier::access_modifier_enum declarationAccessLevel =
          SgAccessModifier::e_unknown;
      SgDeclarationStatement *declarationFromSymbol = NULL;

      // DQ (7/14/2025): Adding timers to support Matt's tool.
      {
        // TimingPerformance timer1 ("Fixup symbol tables:
        // injectSymbolsFromReferencedScopeIntoCurrentScope:
        // get_symbol_basis:");

        // SgNode* symbolBasis = symbol->get_symbol_basis();
        symbolBasis = symbol->get_symbol_basis();
        ROSE_ASSERT(symbolBasis != NULL);
#if ALIAS_SYMBOL_DEBUGGING
        printf("symbolBasis = %p = %s \n", symbolBasis,
               symbolBasis->class_name().c_str());
#endif
        // DQ (7/15/2025): this had to be moved as a result of putting in the
        // performance monitoring. SgDeclarationStatement* declarationFromSymbol
        // = isSgDeclarationStatement(symbolBasis);
        declarationFromSymbol = isSgDeclarationStatement(symbolBasis);

        // DQ (7/15/2025): this had to be moved as a result of putting in the
        // performance monitoring. SgAccessModifier::access_modifier_enum
        // declarationAccessLevel = SgAccessModifier::e_unknown;

        // ROSE_ASSERT(declarationFromSymbol != NULL);
        if (declarationFromSymbol != NULL) {
          // DQ (6/22/2011): Can I, or should I, do relational operations on
          // enum values (note that the values are designed to allow this).
          declarationAccessLevel =
              declarationFromSymbol->get_declarationModifier()
                  .get_accessModifier()
                  .get_modifier();
        }

        else {
          SgInitializedName *initializedNameFromSymbol =
              isSgInitializedName(symbolBasis);
          ROSE_ASSERT(initializedNameFromSymbol != NULL);

          // DQ (9/8/2014): This fails for test2013_234, 235, 240, 241, 242,
          // 246.C. ROSE_ASSERT(initializedNameFromSymbol->get_declptr() !=
          // NULL); declarationAccessLevel =
          // initializedNameFromSymbol->get_declptr()->get_declarationModifier().get_accessModifier().get_modifier();
          if (initializedNameFromSymbol->get_declptr() != NULL) {
            declarationAccessLevel = initializedNameFromSymbol->get_declptr()
                                         ->get_declarationModifier()
                                         .get_accessModifier()
                                         .get_modifier();
          } else {
            // MLOG_WARN_C("astPostProcessing", "In
            // injectSymbolsFromReferencedScopeIntoCurrentScope():
            // initializedNameFromSymbol->get_declptr() == NULL:
            // initializedNameFromSymbol->get_name() = %s
            // \n",initializedNameFromSymbol->get_name().str());
            MLOG_INFO_C(
                "astPostProcessing",
                "In injectSymbolsFromReferencedScopeIntoCurrentScope(): "
                "initializedNameFromSymbol->get_declptr() == NULL: "
                "initializedNameFromSymbol->get_name() = %s \n",
                initializedNameFromSymbol->get_name().str());
          }
        }
        // DQ (7/14/2025): Adding timers to support Matt's tool.
      }

#if ALIAS_SYMBOL_DEBUGGING || 0
      printf("declarationAccessLevel = %d accessLevel = %d \n",
             declarationAccessLevel, accessLevel);
#endif

      // DQ (1/21/2019): The test2019_21.C demonstrates that while a private
      // base class is visible in the derived class, it an subsequent derived
      // class it should not be visible.  So we can't just copy all of the
      // symbols from one class into another.  We need to check the causal node
      // and see if it is associated with a private base class within the class
      // whose nodes were are copying.

#if ALIAS_SYMBOL_DEBUGGING || 0
      printf("  --- accessLevel            = %d \n", accessLevel);
      printf("  --- declarationAccessLevel = %d \n", declarationAccessLevel);
      printf("  --- causalNode             = %p = %s \n", causalNode,
             (causalNode != NULL) ? causalNode->class_name().c_str() : "null");
      printf("  --- symbol table name: name = %s \n", name.str());
      printf("  --- symbol          = %p = %s \n", symbol,
             symbol->class_name().c_str());
      printf("  --- original_symbol = %p = %s \n", original_symbol,
             original_symbol->class_name().c_str());
#endif

#define DEBUG_PRIVATE_BASE_CLASS_ALIAS_SYMBOL_SUPPORT 0

      // DQ (1/25/2019): This code should be refactored to a seperate function
      // since it is just about the private base class support.  An alternative
      // implementation could use a isInherited flag or
      // isInheritedThroughPrivateBaseClass flag that might simplify the
      // implementation (suggested by Tristan). The flag would be added to the
      // SgAliasSymbol, unclear if this is redundant with the causal node list.

      bool definedThroughPrivateBaseClass = false;

      SgBaseClass *baseClass = causalBaseClass;
      if (baseClass != NULL) {
        // DQ (7/14/2025): Adding timers to support Matt's tool.
        {
          // TimingPerformance timer1 ("Fixup symbol tables:
          // injectSymbolsFromReferencedScopeIntoCurrentScope: handle
          // baseClass:");

          // DQ (7/19/2025): Adding debugging information.
          AstPerformance::
              injectSymbolsFromReferencedScopeIntoCurrentScope_numberOfBaseClass++;

          SgBaseClassModifier *baseClassModifier =
              baseClass->get_baseClassModifier();
          ROSE_ASSERT(baseClassModifier != NULL);

#if DEBUG_PRIVATE_BASE_CLASS_ALIAS_SYMBOL_SUPPORT
          SgAccessModifier &accessModifier =
              baseClassModifier->get_accessModifier();
          printf("  --- baseClass = %p name = %s \n", baseClass,
                 SageInterface::get_name(baseClass).c_str());
          printf("  --- accessModifier = %s \n",
                 accessModifier.displayString().c_str());
#endif
          // Check if the symbol here is from another base class and if is a
          // private base class. Iterate for each symbol through the associated
          // base classes until we reach a non-alias symbol. Note that the alias
          // symbols do no form an alias chain, so we have to go through the
          // causal nodes within each iteration.

          SgClassDefinition *derivedClassDefinition =
              isSgClassDefinition(baseClass->get_parent());
          ROSE_ASSERT(derivedClassDefinition != NULL);
          SgClassDeclaration *derivedClassDeclaration =
              derivedClassDefinition->get_declaration();
          ROSE_ASSERT(derivedClassDeclaration != NULL);

#if DEBUG_PRIVATE_BASE_CLASS_ALIAS_SYMBOL_SUPPORT
          printf("derivedClassDeclaration = %p = %s name = %s \n",
                 derivedClassDeclaration,
                 derivedClassDeclaration->class_name().c_str(),
                 derivedClassDeclaration->get_name().str());
#endif
          SgScopeStatement *baseScope = causalBaseScope;
          ROSE_ASSERT(baseScope != NULL);

#if DEBUG_PRIVATE_BASE_CLASS_ALIAS_SYMBOL_SUPPORT
          // DQ (8/9/2020): The variable baseClassDeclaration is not defined.
          // printf ("baseClassDeclaration = %p = %s name = %s
          // \n",baseClassDeclaration,baseClassDeclaration->class_name().c_str(),baseClassDeclaration->get_name().str());
          printf("baseScope = %p = %s \n", baseScope,
                 baseScope->class_name().c_str());
#endif
          // SgClassDefinition* classDefinition =
          // classDeclaration->get_definition(); ROSE_ASSERT(classDefinition !=
          // NULL);

          if (baseScope->symbol_exists(original_symbol) == true) {
            // Sense we found the original symbol in the base class, we know
            // that the symbol would visible in the derivedClassDeclaration only
            // because of the derivation from the base class.  Then the only
            // point is if the base class is a private base class or not. If it
            // is a private base class then we don't want to build the alias
            // symbol, but if it is public or protected then we do want to
            // insert the symbol (through a SgAliasSymbol).

            // DQ (7/19/2025): Adding debugging info.
            AstPerformance::
                injectSymbolsFromReferencedScopeIntoCurrentScope_numberOfTimes_symbolExistsInBaseScope++;

#if DEBUG_PRIVATE_BASE_CLASS_ALIAS_SYMBOL_SUPPORT
            // printf ("FOUND original_symbol in baseClassDeclaration = %s
            // \n",baseClassDeclaration->get_name().str());

            // SgAliasSymbol* aliasSymbol = isSgAliasSymbol(original_symbol);
            // size_t count_alias_symbol (const SgName &n);

            printf("baseScope->count_alias_symbol(): name = %s count = %zu \n",
                   name.str(), baseScope->count_alias_symbol(name));
            // baseScope->print_symboltable("isDefinedThroughPrivateBaseClass");
#endif

            SgSymbol *baseClassSymbol = NULL;
            switch (symbol->variantT()) {
            case V_SgVariableSymbol: {
#if USING_PERFORMANCE_TRACING
              TimingPerformance timer1(
                  "Fixup symbol tables: "
                  "injectSymbolsFromReferencedScopeIntoCurrentScope: handle "
                  "baseClass: handle variableSymbol:");
#endif
#if DEBUG_PRIVATE_BASE_CLASS_ALIAS_SYMBOL_SUPPORT
              printf("case V_SgVariableSymbol: Process symbol lookup as a "
                     "variable \n");
#endif
              // DQ (1/23/2019): We want this to return a SgAliasSymbol
              // associated with the name if it exists, and maybe if there is
              // also not a non-alias symbol available.  Teh get_symbol function
              // will return the non-aliased version of the symbol even when an
              // aliased version of the symbol exists. So we need an additional
              // API function to support this.

              // DQ (7/19/2025): Adding debugging info.
              AstPerformance::
                  injectSymbolsFromReferencedScopeIntoCurrentScope_numberOfTimes_symbolExistsInBaseScope_SgVariableSymbol++;

              // SgSymbol* baseClassVariableSymbol =
              // baseScope->lookup_variable_symbol(name); SgSymbol*
              // baseClassVariableSymbol = baseScope->lookup_symbol(name);
              SgSymbol *baseClassVariableSymbol =
                  baseScope->lookup_alias_symbol(name, symbol);
              if (baseClassVariableSymbol == NULL) {
#if DEBUG_PRIVATE_BASE_CLASS_ALIAS_SYMBOL_SUPPORT
                printf("NO SgAliasSymbol was found: search for non-alias "
                       "symbol \n");
#endif
                baseClassVariableSymbol =
                    baseScope->lookup_variable_symbol(name);
              }
              ROSE_ASSERT(baseClassVariableSymbol != NULL);
              baseClassSymbol = baseClassVariableSymbol;
              break;
            }

            case V_SgTypedefSymbol: {
#if DEBUG_PRIVATE_BASE_CLASS_ALIAS_SYMBOL_SUPPORT
              printf("case V_SgTypedefSymbol: Not supported in private base "
                     "class symbol alias handling \n");
#endif
              break;
            }

            case V_SgEnumSymbol:
            case V_SgEnumFieldSymbol: {
#if DEBUG_PRIVATE_BASE_CLASS_ALIAS_SYMBOL_SUPPORT
              printf("case V_SgEnumFieldSymbol: Not supported in private base "
                     "class symbol alias handling \n");
#endif
              break;
            }

            default: {
#if DEBUG_PRIVATE_BASE_CLASS_ALIAS_SYMBOL_SUPPORT
              printf("Default reached in switch: Symbol = %p = %s name = %s is "
                     "not handled in switch \n",
                     symbol, symbol->class_name().c_str(), name.str());
              // ROSE_ABORT();
#endif
            }
            }

            // SgSymbol* baseClassSymbol =
            // baseScope->lookup_class_symbol(name,NULL);
            // ROSE_ASSERT(baseClassSymbol != NULL);

            if (baseClassSymbol != NULL) {
#if DEBUG_PRIVATE_BASE_CLASS_ALIAS_SYMBOL_SUPPORT
              printf("baseClassSymbol = %p = %s \n", baseClassSymbol,
                     baseClassSymbol->class_name().c_str());
#endif
            }

            // ROSE_ASSERT(baseScope->count_alias_symbol(name) > 0);
            // SgSymbol* baseClassSymbol = baseScope->get_symbol(name);
            // ROSE_ASSERT(baseClassSymbol != NULL);
            SgAliasSymbol *baseClassAliasSymbol =
                isSgAliasSymbol(baseClassSymbol);

            if (baseClassAliasSymbol != NULL) {
              // DQ (7/14/2025): Adding timers to support Matt's tool.
#if USING_PERFORMANCE_TRACING
              TimingPerformance timer1(
                  "Fixup symbol tables: "
                  "injectSymbolsFromReferencedScopeIntoCurrentScope: handle "
                  "baseClass: handle variableSymbol:");
#endif
#if DEBUG_PRIVATE_BASE_CLASS_ALIAS_SYMBOL_SUPPORT
              printf("baseClassAliasSymbol = %p = %s \n", baseClassAliasSymbol,
                     baseClassAliasSymbol->class_name().c_str());
              printf("In xxxisDefinedThroughPrivateBaseClass(): FOUND "
                     "SgAliasSymbol! \n");
#endif

              // Look up the causal node for the symbol found in the base class.
              SgNodePtrList &causalNodeList =
                  baseClassAliasSymbol->get_causal_nodes();
              ROSE_ASSERT(causalNodeList.empty() == false);

              SgNode *base_class_causal_node = causalNodeList[0];
              ROSE_ASSERT(base_class_causal_node != NULL);

              SgBaseClass *basebaseClass =
                  isSgBaseClass(base_class_causal_node);

              // Need to iterate through base classes chains. iterate past the
              // first layer.
              SgClassDeclaration *basebaseClassDeclaration =
                  baseClass->get_base_class();
              ROSE_ASSERT(basebaseClassDeclaration != NULL);
              SgClassDeclaration *definingBaseBaseClassDeclaration =
                  isSgClassDeclaration(
                      basebaseClassDeclaration->get_definingDeclaration());
              ROSE_ASSERT(definingBaseBaseClassDeclaration != NULL);
#if DEBUG_PRIVATE_BASE_CLASS_ALIAS_SYMBOL_SUPPORT
              printf("definingBaseBaseClassDeclaration = %p = %s \n",
                     definingBaseBaseClassDeclaration,
                     definingBaseBaseClassDeclaration->get_name().str());
#endif
              SgBaseClassModifier *basebaseClassModifier =
                  basebaseClass->get_baseClassModifier();
              ROSE_ASSERT(basebaseClassModifier != NULL);
              SgAccessModifier &basebaseClassAccessModifier =
                  basebaseClassModifier->get_accessModifier();
#if DEBUG_PRIVATE_BASE_CLASS_ALIAS_SYMBOL_SUPPORT
              printf("  --- basebaseClass = %p name = %s \n", basebaseClass,
                     SageInterface::get_name(basebaseClass).c_str());
              printf("  --- basebaseClassAccessModifier = %s \n",
                     basebaseClassAccessModifier.displayString().c_str());
#endif
              if (basebaseClassAccessModifier.isPrivate() == true) {
#if DEBUG_PRIVATE_BASE_CLASS_ALIAS_SYMBOL_SUPPORT
                printf("Private base class derivation used: so we don't alias "
                       "this symbol in the derived class \n");
#endif
                // DQ (1/24/2019): Added to support Cxx_tests/test2019_21.C
                // (symbol aliasing of data member in private base class of base
                // class of derived class).
                definedThroughPrivateBaseClass = true;

                // DQ (1/24/2019): We need to also set the declaration level as
                // private (for the logic below).
                declarationAccessLevel = SgAccessModifier::e_private;
              }
            } else {
              // This is not a symbol aliased from an other scope through class
              // derivation.
#if DEBUG_PRIVATE_BASE_CLASS_ALIAS_SYMBOL_SUPPORT
              printf("NOT a SgAliasSymbol: baseClassSymbol = %p = %s \n",
                     baseClassSymbol, baseClassSymbol->class_name().c_str());
#endif
            }
          } else {
#if DEBUG_PRIVATE_BASE_CLASS_ALIAS_SYMBOL_SUPPORT
            // printf ("NOT found original_symbol in baseClassDeclaration = %s
            // \n",baseClassDeclaration->get_name().str());
            printf("NOT found original_symbol baseClassAliasSymbol == NULL: "
                   "baseScope = %p = %s \n",
                   baseScope, baseScope->class_name().c_str());
#endif
          }

#if DEBUG_PRIVATE_BASE_CLASS_ALIAS_SYMBOL_SUPPORT
          printf("definedThroughPrivateBaseClass = %s \n",
                 definedThroughPrivateBaseClass ? "true" : "false");
          if (definedThroughPrivateBaseClass == true) {
            printf("symbol is associated with a base class that is private, so "
                   "it should not be aliased \n");
          }
#endif
          // DQ (7/14/2025): Adding timers to support Matt's tool.
        }
      }

#if ALIAS_SYMBOL_DEBUGGING || 0
      printf("declarationAccessLevel         = %d \n", declarationAccessLevel);
      printf("accessLevel                    = %d \n", accessLevel);
      printf("causalNode                     = %p = %s \n", causalNode,
             causalNode->class_name().c_str());
      printf("definedThroughPrivateBaseClass = %s \n",
             definedThroughPrivateBaseClass ? "true" : "false");
#endif

      // DQ (8/15/2020): We need to account for the access permission being a
      // new value SgAccessModifier::e_default. DQ (1/24/2019): Added support
      // for where we detect that a symbol should not be aliased because it
      // comes from a pribvate base class derivation in a nested derivation.
      // DQ (12/23/2015): See test2015_140.C for where even private base classes
      // will require representations of it's symbols in the derived class (to
      // support correct name qualification). if (declarationAccessLevel >=
      // accessLevel) if ( (declarationAccessLevel >= accessLevel) ||
      // isSgBaseClass(causalNode) != NULL) DQ (8/15/2020): This original code
      // is fine as long as the accessLevel is set properly as function input.
      if ((declarationAccessLevel >= accessLevel) ||
          (isSgBaseClass(causalNode) != NULL &&
           definedThroughPrivateBaseClass == false)) {
        // This declaration is visible, so build an alias.

        // DQ (7/14/2025): Adding timers to support Matt's tool.
#if USING_PERFORMANCE_TRACING
        TimingPerformance timer1("Fixup symbol tables: "
                                 "injectSymbolsFromReferencedScopeIntoCurrentSc"
                                 "ope: handle baseClass: build an alias:");
#endif

        // DQ (8/9/2020): Check and see if the name is visible without
        // referencing SgAliasSymbols. SgDeclarationStatement*
        // declarationFromSymbol = isSgDeclarationStatement(symbolBasis);
        SgTemplateParameterPtrList *templateParameterList = NULL;
        SgTemplateArgumentPtrList *templateArgumentList = NULL;
        if (declarationFromSymbol != NULL) {
          templateParameterList =
              SageBuilder::getTemplateParameterList(declarationFromSymbol);
          templateArgumentList =
              SageBuilder::getTemplateArgumentList(declarationFromSymbol);
        }

#if ALIAS_SYMBOL_DEBUGGING || 0
        printf("Calling "
               "SageInterface::lookupSymbolInParentScopesIgnoringAliasSymbols()"
               " \n");
        printf(" --- name = %s \n", name.str());
        printf(" --- currentScope = %p = %s \n", currentScope,
               currentScope->class_name().c_str());
        printf(" --- currentScope name = %s \n",
               SageInterface::get_name(currentScope).c_str());
        printf(" --- calledFromUsingDirective = %s \n",
               calledFromUsingDirective ? "true" : "false");
#endif
        // DQ (8/15/2020): This code may be inapproriate for Fortran rules.

        // DQ (8/14/2020): Activated this code back to what it was.
        // DQ (8/14/2020): Commented out this fix to test
        // Cxx_tests/test2020_33.C. DQ (8/9/2020): I think this should only be
        // called from the case of a SgUsingDirectiveStatement.
        SgSymbol *trial_lookup_symbol = NULL;
        if (calledFromUsingDirective == true) {
          // DQ (7/14/2025): Adding timers to support Matt's tool.
#if USING_PERFORMANCE_TRACING
          TimingPerformance timer1(
              "Fixup symbol tables: "
              "injectSymbolsFromReferencedScopeIntoCurrentScope: handle "
              "baseClass: build an alias: calledFromUsingDirective == true:");
#endif
          // DQ (7/19/2025): Adding debugging info.
          AstPerformance::
              injectSymbolsFromReferencedScopeIntoCurrentScope_numberOfTimes_calledFromUsingDirective++;

          trial_lookup_symbol =
              SageInterface::lookupSymbolInParentScopesIgnoringAliasSymbols(
                  name, currentScope, templateParameterList,
                  templateArgumentList);
          // DQ (8/9/2020): Ignore case of SgEnumSymbol symbols (see
          // Cxx11_tests/test2019_448.C).
          SgEnumSymbol *enumSymbol = isSgEnumSymbol(trial_lookup_symbol);
          if (enumSymbol != NULL) {
#if ALIAS_SYMBOL_DEBUGGING || 0
            printf("Detected a trial_lookup_symbol == SgEnumSymbol: reset "
                   "trial_lookup_symbol = NULL \n");
#endif
            trial_lookup_symbol = NULL;
          }
          // DQ (8/9/2020): Ignore case of SgEnumSymbol symbols (see
          // Cxx11_tests/test2019_448.C).
          SgNamespaceSymbol *namespaceSymbol =
              isSgNamespaceSymbol(trial_lookup_symbol);
          if (namespaceSymbol != NULL) {
#if ALIAS_SYMBOL_DEBUGGING || 0
            printf("Detected a trial_lookup_symbol == SgNamespaceSymbol: reset "
                   "trial_lookup_symbol = NULL \n");
#endif
            trial_lookup_symbol = NULL;
          }
        }

#if ALIAS_SYMBOL_DEBUGGING || 0
        printf("DONE: Calling "
               "SageInterface::lookupSymbolInParentScopesIgnoringAliasSymbols()"
               " \n");
        printf(" --- trial_lookup_symbol = %p \n", trial_lookup_symbol);
        if (trial_lookup_symbol != NULL) {
          printf(" --- trial_lookup_symbol = %p = %s \n", trial_lookup_symbol,
                 trial_lookup_symbol->class_name().c_str());
        }
#endif
        // DQ (8/9/2020): start of case to exclude symbols that can be found
        // based on name only and not using SgAliasSymbols.
        if (trial_lookup_symbol != NULL) {
#if ALIAS_SYMBOL_DEBUGGING || 0
          printf("trial_lookup_symbol != NULL: a symbol with this name is "
                 "visible in paranet scopes: need to exclude this symbol \n");
#endif
        } else
        // DQ (8/14/2020): Commented out this fix to test
        // Cxx_tests/test2020_33.C.
        {
          // DQ (8/9/2020): Original code before supporting exclusion of symbols
          // in parent scopes.

#if ALIAS_SYMBOL_DEBUGGING || 0
          printf("This declaration is visible, so build an alias \n");
#endif

          // DQ (8/14/2020): The problem here (demonstrated in test code:
          // Cxx_tests/test2020_33.C) is that there could be two or more symbols
          // with the same alias and so we can't just check the name.  Cache the
          // exact identity checked by the old equal_range(name) scan:
          // same visible name, same symbol basis, and same non-alias symbol
          // kind.
          AliasSymbolIdentity symbolIdentity;
          symbolIdentity.name = name.getString();
          symbolIdentity.basis = symbolBasis;
          symbolIdentity.variant = static_cast<int>(symbol->variantT());
          bool alreadyExists =
              currentScopeAliasIndex.symbolIdentities.find(symbolIdentity) !=
              currentScopeAliasIndex.symbolIdentities.end();

          if (alreadyExists == true) {
            AstPerformance::
                injectSymbolsFromReferencedScopeIntoCurrentScope_numberOfTimes_alreadyExistsAndIsInterestingCase++;
          }

          // DQ (2/15/2019): Assume it does not already exist, because we want
          // multiple base classes to represent it with multiple (different)
          // SgAliasSymbols.

          if (alreadyExists == false) {
            // DQ (7/14/2025): Adding timers to support Matt's tool.
            // TimingPerformance timer1 ("Fixup symbol tables:
            // injectSymbolsFromReferencedScopeIntoCurrentScope: handle
            // baseClass: build an alias: after resetting alreadyExists:
            // alreadyExists == false:");

#if ALIAS_SYMBOL_DEBUGGING || 0
            printf("Building a new SgAliasSymbol: causalNode = %p = %s \n",
                   causalNode, causalNode->class_name().c_str());
#endif
            // DQ (7/19/2025): Adding performance debugging support.
            AstPerformance::
                injectSymbolsFromReferencedScopeIntoCurrentScope_alreadyExists_false_addingNewSgAliasSymbol++;

            // DQ: The parameter to a SgAliasSymbol is a SgSymbol (but should
            // not be another SgAliasSymbol).
            SgAliasSymbol *aliasSymbol = new SgAliasSymbol(symbol);
            ROSE_ASSERT(aliasSymbol != NULL);

            // DQ (7/12/2014): Added support to trace back the SgAliasSymbol to
            // the declarations that caused it to be added.
            ROSE_ASSERT(causalNode != NULL);

            // DQ (12/26/2020): Since this is a new SgAliasSymbol, it should
            // have an empty causal node list.
            ROSE_ASSERT(aliasSymbol->get_causal_nodes().empty() == true);

            aliasSymbol->get_causal_nodes().push_back(causalNode);

#if ALIAS_SYMBOL_DEBUGGING || 0
            // printf ("In injectSymbolsFromReferencedScopeIntoCurrentScope():
            // Adding symbol to new scope as a SgAliasSymbol = %p causalNode =
            // %p = %s
            // \n",aliasSymbol,causalNode,causalNode->class_name().c_str());
            printf("In injectSymbolsFromReferencedScopeIntoCurrentScope(): "
                   "Adding symbol to new scope (currentScope = %p = %s) as a "
                   "SgAliasSymbol = %p causalNode = %p = %s \n",
                   currentScope, currentScope->class_name().c_str(),
                   aliasSymbol, causalNode, causalNode->class_name().c_str());
#endif
            // Use the current name and the alias to the symbol
            currentScope->insert_symbol(name, aliasSymbol);
            currentScopeAliasIndex.symbolIdentities.insert(symbolIdentity);
            currentScopeAliasIndex.aliasSymbols[symbolIdentity] = aliasSymbol;

#if ALIAS_SYMBOL_DEBUGGING || 0
            printf("In injectSymbolsFromReferencedScopeIntoCurrentScope(): "
                   "DONE: Adding symbol to new scope (currentScope = %p = %s) "
                   "as a SgAliasSymbol = %p causalNode = %p = %s \n",
                   currentScope, currentScope->class_name().c_str(),
                   aliasSymbol, causalNode, causalNode->class_name().c_str());
#endif
          } else {
            // DQ (7/14/2025): Adding timers to support Matt's tool.
#if USING_PERFORMANCE_TRACING
            TimingPerformance timer1(
                "Fixup symbol tables: "
                "injectSymbolsFromReferencedScopeIntoCurrentScope: handle "
                "baseClass: build an alias: after resetting alreadyExists: "
                "alreadyExists == true:");
#endif

#if ALIAS_SYMBOL_DEBUGGING || 0
            printf("An alias symbol for the same kind of symbol already "
                   "exists, so add to the existing SgAliasSymbol \n");
            printf("  --- symbol          = %p = %s \n", symbol,
                   symbol->class_name().c_str());
            printf("  --- original_symbol = %p = %s \n", original_symbol,
                   original_symbol->class_name().c_str());
            SgNode *symbolTableNode = original_symbol->get_parent();
            ROSE_ASSERT(symbolTableNode != NULL);
            SgScopeStatement *scope =
                isSgScopeStatement(symbolTableNode->get_parent());
            ROSE_ASSERT(scope != NULL);
            SgClassDefinition *classDefinition = isSgClassDefinition(scope);
            ROSE_ASSERT(classDefinition != NULL);
            SgClassDeclaration *classDeclaration =
                classDefinition->get_declaration();
            ROSE_ASSERT(classDeclaration != NULL);
            printf("  --- original_symbol from class = %p = %s name = %s \n",
                   classDeclaration, classDeclaration->class_name().c_str(),
                   classDeclaration->get_name().str());
#endif

            auto alias_it =
                currentScopeAliasIndex.aliasSymbols.find(symbolIdentity);
            SgAliasSymbol *aliasSymbol =
                alias_it != currentScopeAliasIndex.aliasSymbols.end()
                    ? alias_it->second
                    : NULL;

            // If lookup by (name, symbol) misses, fall back to creating the
            // alias now so injected members remain visible in the current
            // scope.
            if (aliasSymbol == NULL) {
              ROSE_ASSERT(causalNode != NULL);

#if ALIAS_SYMBOL_DEBUGGING || 0
              printf("In injectSymbolsFromReferencedScopeIntoCurrentScope(): "
                     "lookup_alias_symbol() returned NULL, creating alias "
                     "for symbol = %p = %s name = %s\n",
                     symbol, symbol->class_name().c_str(), name.str());
#endif

              aliasSymbol = new SgAliasSymbol(symbol);
              ROSE_ASSERT(aliasSymbol != NULL);
              ROSE_ASSERT(aliasSymbol->get_causal_nodes().empty() == true);
              aliasSymbol->get_causal_nodes().push_back(causalNode);
              currentScope->insert_symbol(name, aliasSymbol);
              currentScopeAliasIndex.symbolIdentities.insert(symbolIdentity);
              currentScopeAliasIndex.aliasSymbols[symbolIdentity] = aliasSymbol;
            }

            ROSE_ASSERT(aliasSymbol != NULL);

            // DQ (7/12/2014): Added support to trace back the SgAliasSymbol
            // to the declarations that caused it to be added.
            ROSE_ASSERT(causalNode != NULL);

            // DQ (7/19/2025): Adding performance debugging support.
            AstPerformance::
                injectSymbolsFromReferencedScopeIntoCurrentScope_alreadyExists_true_addingCausalNode++;

            // DQ (12/26/2020): check if this is already a causal node.
            // Debugging test_122.cpp in codeSegregation.
            // aliasSymbol->get_causal_nodes().push_back(causalNode);
            SgNodePtrList &causal_nodes_list = aliasSymbol->get_causal_nodes();
            if (std::find(causal_nodes_list.begin(), causal_nodes_list.end(),
                          causalNode) != causal_nodes_list.end()) {
#if ALIAS_SYMBOL_DEBUGGING || 0
              printf("In injectSymbolsFromReferencedScopeIntoCurrentScope(): "
                     "This causal node is already present in the "
                     "causal_nodes_list: causalNode = %p = %s \n",
                     causalNode, causalNode->class_name().c_str());
              printf("Skipping insertion of causalNode into causal_nodes_list: "
                     "causal_nodes_list.size() = %zu \n",
                     causal_nodes_list.size());
#endif
            } else {
#if ALIAS_SYMBOL_DEBUGGING || 0
              // DQ (7/18/2025): Adding debugging information.
              printf("In injectSymbolsFromReferencedScopeIntoCurrentScope(): "
                     "Adding a causal node to the causal_nodes_list (size = "
                     "%zu): causalNode = %p = %s \n",
                     causal_nodes_list.size(), causalNode,
                     causalNode->class_name().c_str());
#endif
              aliasSymbol->get_causal_nodes().push_back(causalNode);
            }

#if ALIAS_SYMBOL_DEBUGGING || 0
            printf("aliasSymbol->get_causal_nodes().size() = %zu \n",
                   aliasSymbol->get_causal_nodes().size());
            for (size_t i = 0; i < aliasSymbol->get_causal_nodes().size();
                 i++) {
              SgNode *causalNode = aliasSymbol->get_causal_nodes()[i];
              printf(" --- causal node #%zu = %p = %s \n", i, causalNode,
                     causalNode->class_name().c_str());
            }

#endif
          }

          // DQ (8/9/2020): end of case to exclude symbols that can be found
          // based on name only and not using SgAliasSymbols.
        }
      } else {
#if ALIAS_SYMBOL_DEBUGGING
        printf("NO SgAliasSymbol ADDED (wrong permissions): "
               "declarationFromSymbol = %p \n",
               declarationFromSymbol);
#endif
      }
      // Increment iterator
      i++;
    }

#if ALIAS_SYMBOL_DEBUGGING
    printf(
        "In injectSymbolsFromReferencedScopeIntoCurrentScope(): "
        "referencedScope = %p = %s currentScope = %p = %s accessLevel = %d \n",
        referencedScope, referencedScope->class_name().c_str(), currentScope,
        currentScope->class_name().c_str(), accessLevel);
#endif

    // DQ (7/14/2025): Closing brace of added timing support for Matt's
    // performance tracing tool.
  }
}

void FixupAstSymbolTablesToSupportAliasedSymbols::visit(SgNode *node) {
  // DQ (11/24/2007): Output the current IR node for debugging the traversal of
  // the Fortran AST.
#if ALIAS_SYMBOL_DEBUGGING || 0
  printf("In FixupAstSymbolTablesToSupportAliasedSymbols::visit() (preorder "
         "AST traversal) node = %p = %s \n",
         node, node->class_name().c_str());
#endif

  SgUseStatement *useDeclaration = isSgUseStatement(node);
  if (useDeclaration != NULL) {
    // This must be done in the Fortran AST construction since aliased symbols
    // must be inserted before they are looked up as part of name resolution of
    // variable, functions, and types. For C++ we can be more flexible and
    // support the construction of symbol aliases within post-processing.
  }

  // DQ (4/14/2010): Added this C++ specific support.
  // In the future we may want to support the injection of alias symbols for C++
  // "using" directives and "using" declarations.
  SgUsingDeclarationStatement *usingDeclarationStatement =
      isSgUsingDeclarationStatement(node);
  if (usingDeclarationStatement != NULL) {
#if USING_PERFORMANCE_TRACING || 0
    TimingPerformance timer1("Fixup symbol tables: "
                             "FixupAstSymbolTablesToSupportAliasedSymbols::"
                             "visit: usingDeclarationStatement:");
#endif

#if ALIAS_SYMBOL_DEBUGGING
    printf("Found the SgUsingDeclarationStatement \n");
#endif
    SgScopeStatement *currentScope = usingDeclarationStatement->get_scope();
    ROSE_ASSERT(currentScope != NULL);

    SgDeclarationStatement *declaration =
        usingDeclarationStatement->get_declaration();
    SgInitializedName *initializedName =
        usingDeclarationStatement->get_initializedName();

    // Only one of these can be non-null.
    ROSE_ASSERT(initializedName != NULL || declaration != NULL);
    ROSE_ASSERT((initializedName != NULL && declaration != NULL) == false);

    if (declaration != NULL) {
#if ALIAS_SYMBOL_DEBUGGING
      printf("In FixupAstSymbolTablesToSupportAliasedSymbols::visit(): "
             "declaration = %p = %s \n",
             declaration, declaration->class_name().c_str());
#endif
    } else {
      if (initializedName != NULL) {
#if ALIAS_SYMBOL_DEBUGGING
        printf("In FixupAstSymbolTablesToSupportAliasedSymbols::visit(): "
               "initializedName = %s \n",
               initializedName->get_name().str());
#endif
      } else {
        printf("Error: both declaration and initializedName in "
               "SgUsingDeclarationStatement are NULL \n");
        ROSE_ABORT();
      }
    }
  }

  SgUsingDirectiveStatement *usingDirectiveStatement =
      isSgUsingDirectiveStatement(node);
  if (usingDirectiveStatement != NULL) {
#if USING_PERFORMANCE_TRACING || 0
    TimingPerformance timer1("Fixup symbol tables: "
                             "FixupAstSymbolTablesToSupportAliasedSymbols::"
                             "visit: usingDirectiveStatement:");
#endif

#if ALIAS_SYMBOL_DEBUGGING
    printf("Found the SgUsingDirectiveStatement \n");
#endif
    SgNamespaceDeclarationStatement *namespaceDeclaration =
        usingDirectiveStatement->get_namespaceDeclaration();
    ROSE_ASSERT(namespaceDeclaration != NULL);

    SgScopeStatement *currentScope = usingDirectiveStatement->get_scope();

    // To be more specific this is really a SgNamespaceDefinitionStatement
    SgScopeStatement *referencedScope = namespaceDeclaration->get_definition();

    if (referencedScope == NULL) {
      // DQ (5/21/2010): Handle case of using "std" (predefined namespace in
      // C++), but it not having been explicitly defined (see test2005_57.C).
      if (namespaceDeclaration->get_name() != "std") {
        printf("ERROR: namespaceDeclaration has no valid definition \n");
        namespaceDeclaration->get_startOfConstruct()->display(
            "ERROR: namespaceDeclaration has no valid definition");

        // DQ (5/20/2010): Added assertion to trap this case.
        printf("Exiting because referencedScope could not be identified.\n");
        ROSE_ABORT();
      }
    }

    // Note that "std", as a predefined namespace, can have a null definition,
    // so we can't insist that we inject all symbols in namespaces that we can't
    // see explicitly.
    if (referencedScope != NULL) {
#if USING_PERFORMANCE_TRACING || 1
      TimingPerformance timer1("Fixup symbol tables: "
                               "FixupAstSymbolTablesToSupportAliasedSymbols::"
                               "visit: using directives");
#endif
      // DQ (7/16/2025): Calling performance counters in AstPerformance (static
      // data members).
      AstPerformance::numberOfUsingDirectivesProcessingAliasSymbols++;
      ROSE_ASSERT(referencedScope != NULL);
      ROSE_ASSERT(currentScope != NULL);
      // DQ (8/15/2020): This code may be inapproriate for Fortran rules (but
      // required for Cxx_tests/test2004_79.C).

      // DQ (8/9/2020): We need to define a mode so that within
      // injectSymbolsFromReferencedScopeIntoCurrentScope() we can handle this
      // as a special case.
      bool calledFromUsingDirective = true;
      // injectSymbolsFromReferencedScopeIntoCurrentScope(referencedScope,currentScope,usingDirectiveStatement,SgAccessModifier::e_default,calledFromUsingDirective);
      injectSymbolsFromReferencedScopeIntoCurrentScope(
          referencedScope, currentScope, usingDirectiveStatement,
          SgAccessModifier::e_public, calledFromUsingDirective);
    }
  }

  // DQ (5/6/2011): Added support to build SgAliasSymbols in derived class
  // scopes that reference the symbols of the base classes associated with
  // protected and public declarations.
  SgClassDefinition *classDefinition = isSgClassDefinition(node);
  if (classDefinition != NULL) {
#if USING_PERFORMANCE_TRACING || 0
    TimingPerformance timer1(
        "Fixup symbol tables: "
        "FixupAstSymbolTablesToSupportAliasedSymbols::visit: classDefinition:");
#endif

    if (classDefinition->get_inheritances().size() > 0) {
#if USING_PERFORMANCE_TRACING || 0
      TimingPerformance timer1("Fixup symbol tables: "
                               "FixupAstSymbolTablesToSupportAliasedSymbols::"
                               "visit: classDefinition: with base classes");
#endif

      // Handle any derived classes.
      SgBaseClassPtrList &baseClassList = classDefinition->get_inheritances();
      CurrentScopeAliasIndex currentScopeAliasIndex(classDefinition);
      SgBaseClassPtrList::iterator i = baseClassList.begin();
      for (; i != baseClassList.end(); ++i) {
        // Check each base class.
        SgBaseClass *baseClass = *i;
        ROSE_ASSERT(baseClass != NULL);

        /* skip processing for SgExpBaseClasses (which don't have to define
         * p_base_class) */
        if (baseClass->variantT() == V_SgExpBaseClass) {
          continue;
        }

        // printf ("baseClass->get_baseClassModifier().displayString() = %s
        // \n",baseClass->get_baseClassModifier().displayString().c_str());
        // printf
        // ("baseClass->get_baseClassModifier().get_accessModifier().displayString()
        // = %s
        // \n",baseClass->get_baseClassModifier().get_accessModifier().displayString().c_str());
        // DQ (1/21/2019): get_baseClassModifier() returns a pointer instead of
        // a value. DQ (6/22/2011): Define the access level for alias symbol's
        // declarations to be included. SgAccessModifier::access_modifier_enum
        // accessLevel =
        // baseClass->get_baseClassModifier().get_accessModifier().get_modifier();
        ROSE_ASSERT(baseClass->get_baseClassModifier() != NULL);
        SgAccessModifier::access_modifier_enum accessLevel =
            baseClass->get_baseClassModifier()
                ->get_accessModifier()
                .get_modifier();

        SgScopeStatement *referencedScope = NULL;
        if (baseClass->variantT() == V_SgBaseClass) {
          SgClassDeclaration *tmpClassDeclaration =
              isSgClassDeclaration(baseClass->get_base_class());
          ROSE_ASSERT(tmpClassDeclaration != NULL);
          if (tmpClassDeclaration->get_definingDeclaration() != NULL) {
            SgClassDeclaration *targetDeclaration = isSgClassDeclaration(
                tmpClassDeclaration->get_definingDeclaration());
            ROSE_ASSERT(targetDeclaration != NULL);
            referencedScope = targetDeclaration->get_definition();
          } else {
            if (SgProject::get_verbose() > 0) {
              MLOG_WARN_C(
                  "astPostProcessing",
                  "In FixupAstSymbolTablesToSupportAliasedSymbols::visit(): "
                  "Not really clear how to handle this case where "
                  "tmpClassDeclaration->get_definingDeclaration() == NULL! \n");
            }
          }
        } else if (baseClass->variantT() == V_SgNonrealBaseClass) {
          SgNonrealDecl *nrdecl =
              isSgNonrealBaseClass(baseClass)->get_base_class_nonreal();
          ROSE_ASSERT(nrdecl != NULL);

          referencedScope = nrdecl->get_nonreal_decl_scope();
          ROSE_ASSERT(referencedScope != NULL);
        } else {
          ROSE_ABORT();
        }

        if (referencedScope != NULL) {
          // DQ (7/16/2025): Calling performance counters in AstPerformance
          // (static data members).
          AstPerformance::numberOfUsingBaseClassesProcessingAliasSymbols++;

          bool calledFromUsingDirective = false;
          injectSymbolsFromReferencedScopeIntoCurrentScope(
              referencedScope, classDefinition, baseClass, accessLevel,
              calledFromUsingDirective, currentScopeAliasIndex);
        }
      }
    }
  }

  SgFunctionDeclaration *functionDeclaration = isSgFunctionDeclaration(node);
  if (functionDeclaration != NULL) {
#if USING_PERFORMANCE_TRACING
    TimingPerformance timer1("Fixup symbol tables: "
                             "FixupAstSymbolTablesToSupportAliasedSymbols::"
                             "visit: functionDeclaration:");
#endif
#if ALIAS_SYMBOL_DEBUGGING
    printf("Found a the SgFunctionDeclaration \n");
#endif
    // SgScopeStatement*  functionScope   = functionDeclaration->get_scope();
    SgScopeStatement *currentScope =
        isSgScopeStatement(functionDeclaration->get_parent());
    SgClassDefinition *classDefinition = isSgClassDefinition(currentScope);

    if (classDefinition != NULL) {
      // This is a function declared in a class definition, test of friend
      // (forget why it is important to test for isOperator().
      if (functionDeclaration->get_declarationModifier().isFriend() == true ||
          functionDeclaration->get_specialFunctionModifier().isOperator() ==
              true) {
        // printf ("Process all friend function with a SgAliasSymbol to where
        // they are declared in another scope (usually global scope) \n");
      }
    }
  }

#if ALIAS_SYMBOL_DEBUGGING
  printf("Leaving FixupAstSymbolTablesToSupportAliasedSymbols::visit() "
         "(preorder AST traversal) node = %p = %s \n",
         node, node->class_name().c_str());
#endif
}
