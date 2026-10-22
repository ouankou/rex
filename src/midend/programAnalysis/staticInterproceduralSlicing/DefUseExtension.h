namespace DUVariableAnalysisExt {
SgNode *getNextParentInterstingNode(SgNode *node);
bool isDef(SgNode *node);
bool isIDef(SgNode *node);
bool isIUse(SgNode *node);

bool isUse(SgNode *node);
} // namespace DUVariableAnalysisExt
