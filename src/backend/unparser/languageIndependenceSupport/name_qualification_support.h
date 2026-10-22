
/* Name-qualification unparser declarations. */

#ifndef UNPARSER_NAME_QUALIFICATION
#define UNPARSER_NAME_QUALIFICATION

#include "Cxx_Grammar.h"

#include <map>
#include <string>
#include <utility>

class SgType;
class SgDeclarationStatement;
class SgName;
class SgInitializedName;
class SgNode;
class SgStatement;
class SgTemplateArgument;

struct NameQualificationResult {
  std::string qualifier;
  int length;
  bool global;
  bool typeElaboration;
};

// Resolve the statement whose lexical environment governs one emitted
// qualification use.  Syntax owned by semantic-only declaration
// infrastructure (for example a per-use dependent SgNonrealDecl surface) is
// emitted by the explicitly supplied source statement, whereas ordinary
// source syntax is governed by its structural statement.  Malformed or
// ambiguous ownership is a hard error.
SgStatement *
exactQualificationUseSiteForEmission(const SgNode *node,
                                     SgStatement *emissionStatement);

class NameQualificationContext {
private:
  using Key = std::pair<const SgNode *, const SgStatement *>;
  std::map<Key, NameQualificationResult> qualifications;
  std::map<Key, NameQualificationResult> nameChannelQualifications;
  std::map<Key, NameQualificationResult> typeChannelQualifications;
  std::map<Key, NameQualificationResult> pointerMemberBaseQualifications;

  void recordChannel(std::map<Key, NameQualificationResult> &channel,
                     const char *channelName, const SgNode *node,
                     const SgStatement *useSiteStatement,
                     const NameQualificationResult &result);
  NameQualificationResult
  lookupChannel(const std::map<Key, NameQualificationResult> &channel,
                const char *channelName, const SgNode *node,
                const SgStatement *useSiteStatement) const;

public:
  void clear();
  bool contains(const SgNode *node, const SgStatement *useSiteStatement) const;
  bool containsName(const SgNode *node,
                    const SgStatement *useSiteStatement) const;
  bool containsType(const SgNode *node,
                    const SgStatement *useSiteStatement) const;
  bool containsPointerMemberBase(const SgNode *node,
                                 const SgStatement *useSiteStatement) const;
  void record(const SgNode *node, const SgStatement *useSiteStatement,
              const NameQualificationResult &result);
  NameQualificationResult lookup(const SgNode *node,
                                 const SgStatement *useSiteStatement) const;
  void recordName(const SgNode *node, const SgStatement *useSiteStatement,
                  const NameQualificationResult &result);
  NameQualificationResult lookupName(const SgNode *node,
                                     const SgStatement *useSiteStatement) const;
  void recordType(const SgNode *node, const SgStatement *useSiteStatement,
                  const NameQualificationResult &result);
  NameQualificationResult lookupType(const SgNode *node,
                                     const SgStatement *useSiteStatement) const;
  void recordPointerMemberBase(const SgNode *node,
                               const SgStatement *useSiteStatement,
                               const NameQualificationResult &result);
  NameQualificationResult
  lookupPointerMemberBase(const SgNode *node,
                          const SgStatement *useSiteStatement) const;
};

class Unparser;

class Unparser_Nameq {
private:
  NameQualificationContext &nameQualifications;

public:
  Unparser_Nameq(Unparser *, NameQualificationContext &qualifications)
      : nameQualifications(qualifications) {};
  virtual ~Unparser_Nameq() {};

  NameQualificationResult
  lookup_qualification(const SgNode *node,
                       const SgStatement *useSiteStatement) const;
  NameQualificationResult
  lookup_name_qualification(const SgNode *node,
                            const SgStatement *useSiteStatement) const;
  NameQualificationResult
  lookup_type_qualification(const SgNode *node,
                            const SgStatement *useSiteStatement) const;
  NameQualificationResult
  lookup_type_qualification_for_output(const SgNode *node,
                                       const SgStatement *useSiteStatement,
                                       bool skipGeneratedQualification) const;
  NameQualificationResult lookup_pointer_member_base_qualification(
      const SgNode *node, const SgStatement *useSiteStatement) const;
};

#endif
