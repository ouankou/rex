// Original Author (AstProcessing classes): Markus Schordan
// Rewritten by: Gergo Barany

#include "sage3basic.h"

#include "AstProcessing.h"

#include "AstSimpleProcessing.h"

DummyAttribute AstSimpleProcessing::evaluateInheritedAttribute(
    SgNode *astNode, DummyAttribute /*inheritedValue*/) {
  visit(astNode);
  return defaultDummyAttribute;
}

DummyAttribute AstSimpleProcessing::evaluateSynthesizedAttribute(
    SgNode *astNode, DummyAttribute, SynthesizedAttributesList) {
  visit(astNode);
  return defaultDummyAttribute;
}

DummyAttribute AstPrePostProcessing::evaluateInheritedAttribute(
    SgNode *astNode, DummyAttribute /*inheritedValue*/) {
  preOrderVisit(astNode);
  return defaultDummyAttribute;
}

DummyAttribute AstPrePostProcessing::evaluateSynthesizedAttribute(
    SgNode *astNode, DummyAttribute /*inheritedValue*/,
    SynthesizedAttributesList) {
  postOrderVisit(astNode);
  return defaultDummyAttribute;
}

DummyAttribute AstSimpleProcessing::defaultSynthesizedAttribute(
    DummyAttribute /*inheritedValue*/) {
  return defaultDummyAttribute;
}

DummyAttribute AstPrePostProcessing::defaultSynthesizedAttribute(
    DummyAttribute /*inheritedValue*/) {
  return defaultDummyAttribute;
}

void AstPrePostProcessing::traverse(SgNode *node) {
  SgTreeTraversal<DummyAttribute, DummyAttribute>::traverse(
      node, defaultDummyAttribute, preandpostorder);
}

void AstSimpleProcessing::traverse(SgNode *node,
                                   t_traverseOrder treeTraversalOrder) {
  SgTreeTraversal<DummyAttribute, DummyAttribute>::traverse(
      node, defaultDummyAttribute, treeTraversalOrder);
}

void AstPrePostProcessing::traverseWithinFile(SgNode *node) {
  SgTreeTraversal<DummyAttribute, DummyAttribute>::traverseWithinFile(
      node, defaultDummyAttribute, preandpostorder);
}

void AstSimpleProcessing::traverseWithinFile(
    SgNode *node, t_traverseOrder treeTraversalOrder) {
  SgTreeTraversal<DummyAttribute, DummyAttribute>::traverseWithinFile(
      node, defaultDummyAttribute, treeTraversalOrder);
}

void AstPrePostProcessing::traverseInputFiles(SgProject *projectNode) {
  SgTreeTraversal<DummyAttribute, DummyAttribute>::traverseInputFiles(
      projectNode, defaultDummyAttribute, preandpostorder);
}

void AstSimpleProcessing::traverseInputFiles(
    SgProject *projectNode, t_traverseOrder treeTraversalOrder) {
  SgTreeTraversal<DummyAttribute, DummyAttribute>::traverseInputFiles(
      projectNode, defaultDummyAttribute, treeTraversalOrder);
}

void AstPrePostProcessing::atTraversalStart() {}

void AstPrePostProcessing::atTraversalEnd() {}

void AstSimpleProcessing::atTraversalStart() {}

void AstSimpleProcessing::atTraversalEnd() {}
