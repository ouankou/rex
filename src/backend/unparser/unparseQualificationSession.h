#ifndef REX_UNPARSE_QUALIFICATION_SESSION_H
#define REX_UNPARSE_QUALIFICATION_SESSION_H

class NameQualificationFileAnalysis;
class SgSourceFile;

// A scope in which the AST does not change, so each source file's name
// qualification is computed once and reused by every unparseToString() call
// it serves instead of being recomputed per call.  Opening a session declares
// that nothing modifies the AST until it closes; a typed AST modification
// observed while it is open is a hard error.  Sessions nest: an inner session
// joins the outermost one.
//
// Set REX_UNPARSE_QUALIFICATION_CHECK=1 to recompute every reused result the
// per-call way and require it to be identical.
class UnparseQualificationSession {
public:
  UnparseQualificationSession();
  ~UnparseQualificationSession();
  UnparseQualificationSession(const UnparseQualificationSession &) = delete;
  UnparseQualificationSession &
  operator=(const UnparseQualificationSession &) = delete;

  //! The analysis of file in the active session, or null when no session is
  //! active or one is computing an analysis.
  static const NameQualificationFileAnalysis *analysisFor(SgSourceFile *file);

  //! Whether reused results must be checked against a fresh computation.
  static bool checkingEnabled();

private:
  bool outermost;
};

#endif
