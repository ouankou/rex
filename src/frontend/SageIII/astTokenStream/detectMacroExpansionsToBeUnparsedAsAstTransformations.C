#include "sage3basic.h"
#include "tokenStreamMapping.h"
#include "previousAndNextNode.h"

#include <set>

using namespace std;

namespace {
class TransformationFinder : public SgSimpleProcessing
   {
     public:
      bool found;
      TransformationFinder() : found(false) {}

      void visit(SgNode* n) override
         {
           if (found || n == NULL)
              {
                return;
              }
           if (n->get_containsTransformation())
              {
                found = true;
                return;
              }
           SgLocatedNode* located = isSgLocatedNode(n);
           if (located != NULL && located->isTransformation())
              {
                found = true;
              }
         }
   };

static bool subtreeHasTransformation(SgStatement* statement)
   {
     if (statement == NULL)
        {
          return false;
        }
     if (statement->get_containsTransformation() || statement->isTransformation())
        {
          return true;
        }

     TransformationFinder finder;
     finder.traverse(statement, preorder);
     return finder.found;
   }

static void markStatementsForMacro(MacroExpansion* macroExpansion,
                                   const std::vector<SgStatement*>& statements)
   {
     if (macroExpansion == NULL)
        {
          return;
        }
     for (SgStatement* statement : statements)
        {
          if (statement == NULL)
             {
               continue;
             }
          if (statement->get_file_info() == NULL)
             {
               continue;
             }
          statement->setTransformation();
          statement->setOutputInCodeGeneration();
        }
     macroExpansion->isTransformed = true;
   }
} // namespace

void
detectMacroExpansionsToBeUnparsedAsAstTransformations(SgSourceFile* sourceFile)
   {
  // If a statement associated with a macro expansion contains a transformation,
  // mark all statements for that macro expansion as transformations so that the
  // unparser falls back to AST unparsing for the expanded macro region.
     if (sourceFile == NULL)
        {
          return;
        }

     std::map<SgStatement*, MacroExpansion*>& macroExpansionMap =
         sourceFile->get_macroExpansionMap();
     if (macroExpansionMap.empty())
        {
          return;
        }

     std::set<MacroExpansion*> processed;
     for (std::map<SgStatement*, MacroExpansion*>::const_iterator it =
              macroExpansionMap.begin();
          it != macroExpansionMap.end(); ++it)
        {
          MacroExpansion* macroExpansion = it->second;
          if (macroExpansion == NULL)
             {
               continue;
             }
          if (!processed.insert(macroExpansion).second)
             {
               continue;
             }

          bool needsTransformation = macroExpansion->isTransformed;
          const std::vector<SgStatement*>& statements =
              macroExpansion->associatedStatementVector;

          if (!needsTransformation)
             {
               for (SgStatement* statement : statements)
                  {
                    if (subtreeHasTransformation(statement))
                       {
                         needsTransformation = true;
                         break;
                       }
                  }
             }

          if (!needsTransformation && statements.empty())
             {
               // Fallback: check the map entries if the association list is empty.
               for (std::map<SgStatement*, MacroExpansion*>::const_iterator mapIt =
                        macroExpansionMap.begin();
                    mapIt != macroExpansionMap.end(); ++mapIt)
                  {
                    if (mapIt->second != macroExpansion)
                       {
                         continue;
                       }
                    if (subtreeHasTransformation(mapIt->first))
                       {
                         needsTransformation = true;
                         break;
                       }
                  }
             }

          if (!needsTransformation)
             {
               continue;
             }

          if (!statements.empty())
             {
               markStatementsForMacro(macroExpansion, statements);
             }
          else
             {
               std::vector<SgStatement*> fallbackStatements;
               for (std::map<SgStatement*, MacroExpansion*>::const_iterator mapIt =
                        macroExpansionMap.begin();
                    mapIt != macroExpansionMap.end(); ++mapIt)
                  {
                    if (mapIt->second == macroExpansion)
                       {
                         fallbackStatements.push_back(mapIt->first);
                       }
                  }
               markStatementsForMacro(macroExpansion, fallbackStatements);
             }
        }
   }
