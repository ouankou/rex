// Function Try Block
// This construct creates a function body that is a try-block.
// If ROSE traversal returns SgTryStmt instead of SgBasicBlock,
// this should trigger the failure propagation logic in VisitFunctionDecl.
void func() try {
  int x = 0;
} catch(...) {
  int y = 0;
}
