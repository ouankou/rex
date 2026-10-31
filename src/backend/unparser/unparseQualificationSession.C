#include "sage3basic.h"

#include "nameQualificationSupport.h"
#include "unparseQualificationSession.h"

#include <cstdlib>
#include <cstring>
#include <memory>
#include <unordered_map>

namespace {

struct SessionState {
  uint64_t sequence = 0;
  bool computing = false;
  std::unordered_map<SgSourceFile *,
                     std::unique_ptr<NameQualificationFileAnalysis>>
      analyses;
};

thread_local std::unique_ptr<SessionState> activeSession;

void requireUnchanged(const SessionState &session, const char *event) {
  const uint64_t current = SgNode::get_globalAstModificationSequence();
  if (current != session.sequence) {
    fprintf(stderr,
            "REX_UNPARSE_INVARIANT[qualification-session-mutation]: %s "
            "observed AST modification sequence=%llu after the session "
            "opened at sequence=%llu; the AST must not change while a "
            "qualification session is open\n",
            event, static_cast<unsigned long long>(current),
            static_cast<unsigned long long>(session.sequence));
    ROSE_ABORT();
  }
}

// Requests made while an analysis is being computed use the per-call path,
// exactly as they would outside a session.
class ComputingScope {
public:
  explicit ComputingScope(SessionState &session) : session_(session) {
    session_.computing = true;
  }
  ~ComputingScope() { session_.computing = false; }
  ComputingScope(const ComputingScope &) = delete;
  ComputingScope &operator=(const ComputingScope &) = delete;

private:
  SessionState &session_;
};

} // namespace

UnparseQualificationSession::UnparseQualificationSession()
    : outermost(activeSession == nullptr) {
  if (outermost) {
    activeSession = std::make_unique<SessionState>();
    activeSession->sequence = SgNode::get_globalAstModificationSequence();
  } else {
    requireUnchanged(*activeSession, "nested session");
  }
}

UnparseQualificationSession::~UnparseQualificationSession() {
  if (outermost) {
    requireUnchanged(*activeSession, "session close");
    activeSession.reset();
  }
}

const NameQualificationFileAnalysis *
UnparseQualificationSession::analysisFor(SgSourceFile *file) {
  ASSERT_not_null(file);
  SessionState *session = activeSession.get();
  if (session == nullptr || session->computing) {
    return nullptr;
  }
  requireUnchanged(*session, "qualification request");
  auto found = session->analyses.find(file);
  if (found != session->analyses.end()) {
    return found->second.get();
  }
  std::unique_ptr<NameQualificationFileAnalysis> analysis;
  {
    ComputingScope computing(*session);
    analysis = std::make_unique<NameQualificationFileAnalysis>(file);
  }
  requireUnchanged(*session, "file analysis");
  return session->analyses.emplace(file, std::move(analysis))
      .first->second.get();
}

bool UnparseQualificationSession::checkingEnabled() {
  static const bool enabled = [] {
    const char *value = std::getenv("REX_UNPARSE_QUALIFICATION_CHECK");
    if (value == nullptr) {
      return false;
    }
    if (std::strcmp(value, "1") != 0) {
      fprintf(stderr,
              "REX_UNPARSE_INVARIANT[qualification-check-option]: "
              "REX_UNPARSE_QUALIFICATION_CHECK='%s' must be unset or 1\n",
              value);
      ROSE_ABORT();
    }
    return true;
  }();
  return enabled;
}
