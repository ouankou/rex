#ifndef ROSE_FORTRAN_SOURCE_CONTEXT_H
#define ROSE_FORTRAN_SOURCE_CONTEXT_H

#include <memory>
#include <vector>

class PreprocessingInfo;
class Sg_File_Info;
class SgFortranIncludeLine;
class SgFunctionDeclaration;
class SgEmptyDeclaration;
class SgLocatedNode;
class SgPragmaDeclaration;
class SgSourceFile;
class SgStatement;
class SgNode;

namespace SageInterface {
void publishGeneratedSubtreeOutputOwner(SgNode *, SgLocatedNode *);
}

namespace Rose::FortranSource {

// Validate persisted lexical ownership. Invalid AST-JSON input is reported as
// std::runtime_error; production callers decide their diagnostic boundary.
void validate(const SgSourceFile *);
void validateSelectedPragma(const SgPragmaDeclaration *);

enum class EditReason {
  paired_boundary_absorption,
  directive_lowering,
  body_outlining,
  associated_statement_rewrite,
  loop_transformation,
  source_file_relocation
};

enum class ProgramUnitBoundary { header, closing };

// One invocation-owned immutable index. It retains no observers into an AST
// attachment list, so moving statements and preprocessing records is safe.
class Context {
public:
  // Only the canonical generated-subtree publisher can prepare this bounded
  // proof. It does not own AST nodes or allow structural mutation; source
  // output placement may advance while original provenance stays unchanged.
  class PendingSubtreePublication {
  public:
    ~PendingSubtreePublication();
    PendingSubtreePublication(const PendingSubtreePublication &) = delete;
    PendingSubtreePublication &
    operator=(const PendingSubtreePublication &) = delete;
    bool canAttachSourcePreprocessing(const PreprocessingInfo *,
                                      const SgLocatedNode *) const;
    void complete();

  private:
    friend class Context;
    PendingSubtreePublication(const Context &, const SgNode *,
                              const SgLocatedNode *);
    struct Impl;
    std::unique_ptr<Impl> impl_;
  };

  explicit Context(const SgSourceFile *);
  ~Context();
  Context(const Context &) = delete;
  Context &operator=(const Context &) = delete;

  bool selectedOutput() const;
  // Generated/manual ASTs without a lexical occurrence tree return false.
  bool hasLexicalSource() const;
  bool ownsSourcePoint(const Sg_File_Info *) const;
  bool ownsOutputBoundary(const SgLocatedNode *) const;
  // Program units and their exact definition/body are structural containers;
  // their individual syntax surfaces retain their physical occurrence.
  bool ownsProgramUnitContainer(const SgLocatedNode *) const;
  bool emitProgramUnitBoundary(const SgFunctionDeclaration *,
                               ProgramUnitBoundary,
                               unsigned activeOccurrence) const;
  bool canAttachSourcePreprocessing(const PreprocessingInfo *,
                                    const SgLocatedNode *) const;
  void validateSourcePreprocessingAnchor(const SgEmptyDeclaration *) const;
  bool emitPreprocessing(const PreprocessingInfo *) const;
  bool emitInclude(const SgFortranIncludeLine *) const;

private:
  friend void validate(const SgSourceFile *);
  friend void
  SageInterface::publishGeneratedSubtreeOutputOwner(SgNode *, SgLocatedNode *);
  std::unique_ptr<PendingSubtreePublication>
  prepareSubtreePublication(const SgNode *, const SgLocatedNode *) const;
  bool
  canAttachSourcePreprocessingImpl(const PreprocessingInfo *,
                                   const SgLocatedNode *,
                                   const PendingSubtreePublication *) const;
  bool ownsOutputBoundaryAnchors(const SgLocatedNode *) const;
  struct Impl;
  std::unique_ptr<Impl> impl_;
};

bool selectedOutput(const SgSourceFile *);

// The inputs are the exact source operands an operation is about to change.
// A consumed body includes its source-owned inclusion surfaces, whereas a
// syntax operand excludes unrelated before/after preprocessing attachments.
void prepareEdit(SgSourceFile *, EditReason,
                 const std::vector<const SgLocatedNode *> &syntax,
                 const std::vector<const SgStatement *> &consumedBodies = {});
void preparePairedBoundary(SgLocatedNode *begin, SgLocatedNode *end);

} // namespace Rose::FortranSource

#endif
