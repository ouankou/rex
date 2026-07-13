#ifndef UNPARSER_DELEGATE_H
#define UNPARSER_DELEGATE_H

// This is the base class for the support or alternative code generation
// mechanisms (by Qing Yi) and is the basis of the copy based unparsing that
// unparses the code by copying parts of the AST not transformed directly from
// the source file (character by character, to preserve absolutely ALL
// formatting).  Patch files can then be generated from such files, where the
// patches represent only the transformations introduced.
class UnparseFormat;
class UnparseDelegate {
public:
  enum class StatementCoreEmission { declined, emitted };

  virtual ~UnparseDelegate() = default;

  // A delegate emits only the statement's core AST surface.  The owning
  // unparser retains leading/trailing preprocessing, line directives, numeric
  // labels, formatting, token-routing, and session-state cleanup.

  virtual StatementCoreEmission unparse_statement(SgStatement *stmt,
                                                  SgUnparse_Info &info,
                                                  UnparseFormat &out) = 0;
};

#endif
