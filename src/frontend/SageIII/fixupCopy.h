namespace SageInterface {
void prepareTemplateParameterTypeCopy(const SgTemplateParameter *parameter,
                                      SgCopyHelp &help);
void prepareTemplateArgumentCopy(const SgTemplateArgument *argument,
                                 SgCopyHelp &help);
SgType *materializeTypeWithCopyMap(SgType *type, SgCopyHelp &help,
                                   bool copyOwnedSyntax = true);
SgType *
getTargetFileTypeWithCopyMap(SgType *type, SgScopeStatement *scope,
                             const SgCopyHelp::copiedNodeMapType &identities);
} // namespace SageInterface
