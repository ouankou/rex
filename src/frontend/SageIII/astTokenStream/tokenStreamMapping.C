#include "sage3basic.h"

#include "Rose/StringUtility/FileUtility.h"

#include "general_token_defs.h"
#include "nodeQuery.h"
#include "unparser.h"

#include <algorithm>
#include <limits>
#include <optional>
#include <tuple>
#include <utility>

ROSE_DLL_API bool ROSE_tokenUnparsingTestingMode = false;

#include "tokenStreamMapping.h"

class TokenStreamMappingConstructionAccess {
public:
  static TokenStreamMappingConstructionKey key() {
    return TokenStreamMappingConstructionKey();
  }

  static TokenStreamHalfOpenInterval
  requiredInclusiveInterval(SgNode *node, const char *name, int start,
                            int inclusive_end) {
    return TokenStreamSequenceToNodeMapping::requiredInclusiveDraftInterval(
        node, name, start, inclusive_end);
  }

  static std::optional<TokenStreamHalfOpenInterval>
  optionalInclusiveInterval(SgNode *node, const char *name, int start,
                            int inclusive_end) {
    return TokenStreamSequenceToNodeMapping::optionalInclusiveDraftInterval(
        node, name, start, inclusive_end);
  }

  static TokenStreamSequenceToNodeMapping *construct(
      SgNode *node, const TokenStreamHalfOpenInterval &core,
      std::optional<TokenStreamHalfOpenInterval> leading = std::nullopt,
      std::optional<TokenStreamHalfOpenInterval> trailing = std::nullopt,
      std::optional<TokenStreamHalfOpenInterval> else_interval = std::nullopt) {
    return new TokenStreamSequenceToNodeMapping(node, core, std::move(leading),
                                                std::move(trailing),
                                                std::move(else_interval));
  }

  static TokenStreamSequenceToNodeMapping *createTokenInterval(
      SgSourceFile *source_file, SgNode *node, size_t token_count,
      const TokenStreamHalfOpenInterval &core,
      std::optional<TokenStreamHalfOpenInterval> leading = std::nullopt,
      std::optional<TokenStreamHalfOpenInterval> trailing = std::nullopt,
      std::optional<TokenStreamHalfOpenInterval> else_interval = std::nullopt) {
    return TokenStreamSequenceToNodeMapping::createTokenInterval(
        source_file, node, token_count, core, std::move(leading),
        std::move(trailing), std::move(else_interval));
  }

  static void publish(TokenStreamSequenceToNodeMapping *mapping,
                      size_t token_count) {
    ASSERT_not_null(mapping);
    mapping->publishHalfOpenIntervals(token_count);
  }

  static void requireDirectOwnerInterval(
      SgNode *node, const char *source_file,
      std::optional<TokenStreamHalfOpenInterval> interval) {
    TokenStreamSequenceToNodeMapping::requireDirectOwnerInterval(
        node, source_file, std::move(interval));
  }

  static void retireDetachedMapping(TokenStreamSequenceToNodeMapping *mapping) {
    TokenStreamSequenceToNodeMapping::retireDetachedMapping(mapping);
  }
};

using namespace std;
using namespace Rose;

// namespace for token ID values.
using namespace ROSE_token_ids;

#include "maxExtents.h"

namespace {

int tokenSourceFileId(const SgSourceFile *source_file) {
  ROSE_ASSERT(source_file != nullptr);
  const Sg_File_Info *source_position = source_file->get_file_info();
  ROSE_ASSERT(source_position != nullptr);
  const int source_id = source_position->get_raw_file_id();
  ROSE_ASSERT(source_id >= 0);
  return source_id;
}

bool belongsToTokenSourceFile(const Sg_File_Info *position,
                              const SgSourceFile *source_file) {
  const int source_id = tokenSourceFileId(source_file);
  return position != nullptr && position->get_file_id() == source_id;
}

bool belongsToPhysicalTokenSourceFile(const Sg_File_Info *position,
                                      const SgSourceFile *source_file) {
  const int source_id = tokenSourceFileId(source_file);
  return position != nullptr && position->get_physical_file_id() == source_id;
}

bool isTokenStreamRootNode(SgNode *node) {
  return isSgFile(node) != nullptr || isSgGlobal(node) != nullptr;
}

SgDeclarationGroupStatement *declarationGroupOwner(SgNode *node) {
  SgDeclarationStatement *declaration = isSgDeclarationStatement(node);
  return declaration != nullptr
             ? isSgDeclarationGroupStatement(declaration->get_parent())
             : nullptr;
}

bool declarationRequiresTokenMapping(SgDeclarationStatement *decl,
                                     SgSourceFile *sourceFile);

void detachExactTokenMappingAssociation(
    std::map<SgNode *, TokenStreamSequenceToNodeMapping *> &tokenMap,
    SgNode *node, TokenStreamSequenceToNodeMapping *mapping) {
  if (node == nullptr || mapping == nullptr) {
    fprintf(stderr,
            "REX_TOKEN_INVARIANT[mapping-detachment]: cannot detach a null "
            "node or mapping\n");
    ROSE_ABORT();
  }
  auto direct = tokenMap.find(node);
  const size_t occurrences = static_cast<size_t>(
      std::count(mapping->nodeVector.begin(), mapping->nodeVector.end(), node));
  if (direct == tokenMap.end() || direct->second != mapping ||
      occurrences != 1) {
    fprintf(stderr,
            "REX_TOKEN_INVARIANT[mapping-detachment]: node=%p/%s mapping=%p "
            "has map-entry=%p and %zu reverse associations\n",
            static_cast<void *>(node), node->class_name().c_str(),
            static_cast<void *>(mapping),
            direct != tokenMap.end() ? static_cast<void *>(direct->second)
                                     : nullptr,
            occurrences);
    ROSE_ABORT();
  }

  tokenMap.erase(direct);
  mapping->nodeVector.erase(
      std::find(mapping->nodeVector.begin(), mapping->nodeVector.end(), node));
  mapping->shared = mapping->nodeVector.size() > 1;
  if (!mapping->nodeVector.empty()) {
    if (mapping->node == node) {
      mapping->node = mapping->nodeVector.front();
    }
    return;
  }

  TokenStreamMappingConstructionAccess::retireDetachedMapping(mapping);
}

} // namespace

void detachTokenMappingForRemovedNode(SgSourceFile *sourceFile, SgNode *node) {
  if (sourceFile == nullptr || node == nullptr) {
    fprintf(stderr,
            "REX_TOKEN_INVARIANT[mapping-removal]: source file and removed "
            "node must be nonnull\n");
    ROSE_ABORT();
  }
  std::map<SgNode *, TokenStreamSequenceToNodeMapping *> &tokenMap =
      sourceFile->get_tokenSubsequenceMap();
  const auto mappingEntry = tokenMap.find(node);
  if (mappingEntry == tokenMap.end()) {
    return;
  }
  detachExactTokenMappingAssociation(tokenMap, node, mappingEntry->second);
}

namespace {

enum class TransparentTokenIntervalCarrierRole {
  implicit_conversion,
  semantic_expression_subtree,
  source_syntax_structure,
  catch_sequence_structure,
  implicit_conversion_call_structure,
  gnu_asm_operand_structure
};

const char *transparentTokenIntervalCarrierRoleName(
    TransparentTokenIntervalCarrierRole role) {
  switch (role) {
  case TransparentTokenIntervalCarrierRole::implicit_conversion:
    return "implicit-conversion";
  case TransparentTokenIntervalCarrierRole::semantic_expression_subtree:
    return "semantic-expression-subtree";
  case TransparentTokenIntervalCarrierRole::source_syntax_structure:
    return "source-syntax-structure";
  case TransparentTokenIntervalCarrierRole::catch_sequence_structure:
    return "catch-sequence-structure";
  case TransparentTokenIntervalCarrierRole::implicit_conversion_call_structure:
    return "implicit-conversion-call-structure";
  case TransparentTokenIntervalCarrierRole::gnu_asm_operand_structure:
    return "gnu-asm-operand-structure";
  }
  fprintf(stderr,
          "REX_TOKEN_INVARIANT[transparent-interval-carrier]: invalid carrier "
          "role\n");
  ROSE_ABORT();
}

void requireExactSynthesizedExpressionCarrier(SgExpression *expression,
                                              const char *role) {
  if (expression == nullptr || role == nullptr || *role == '\0') {
    fprintf(stderr,
            "REX_TOKEN_INVARIANT[transparent-interval-carrier]: synthesized "
            "carrier classification has incomplete identity\n");
    ROSE_ABORT();
  }
  for (Sg_File_Info *position :
       {expression->get_file_info(), expression->get_startOfConstruct(),
        expression->get_endOfConstruct(), expression->get_operatorPosition()}) {
    if (position == nullptr || position->get_parent() != expression ||
        position->isShared() || !position->isCompilerGenerated() ||
        !position->isFrontendSpecific() || position->isTransformation() ||
        position->isSourcePositionUnavailableInFrontend() ||
        !position->isOutputInCodeGeneration() ||
        position->get_file_id() != Sg_File_Info::COMPILER_GENERATED_FILE_ID ||
        position->get_physical_file_id() !=
            Sg_File_Info::COMPILER_GENERATED_FILE_ID) {
      fprintf(stderr,
              "REX_TOKEN_INVARIANT[transparent-interval-carrier]: node=%p/%s "
              "role=%s has incomplete synthesized provenance\n",
              static_cast<void *>(expression), expression->class_name().c_str(),
              role);
      ROSE_ABORT();
    }
  }
}

SgFunctionCallExp *exactImplicitConversionCallCarrierOwner(SgNode *node) {
  SgFunctionCallExp *call = isSgFunctionCallExp(node);
  if (call == nullptr) {
    call = isSgFunctionCallExp(node != nullptr ? node->get_parent() : nullptr);
  }
  if (call == nullptr) {
    SgBinaryOp *member_access =
        isSgBinaryOp(node != nullptr ? node->get_parent() : nullptr);
    if (member_access != nullptr && member_access->get_rhs_operand() == node) {
      call = isSgFunctionCallExp(member_access->get_parent());
    }
  }
  if (call == nullptr ||
      call->get_source_syntax() != SgFunctionCallExp::e_implicit_conversion) {
    return nullptr;
  }

  SgExpression *function = call->get_function();
  SgExprListExp *arguments = call->get_args();
  if (function == nullptr || arguments == nullptr ||
      function->get_parent() != call || arguments->get_parent() != call ||
      !arguments->get_expressions().empty()) {
    fprintf(stderr,
            "REX_TOKEN_INVARIANT[transparent-interval-carrier]: implicit "
            "conversion call=%p has no exact function/empty-argument "
            "structure\n",
            static_cast<void *>(call));
    ROSE_ABORT();
  }

  bool exact_role = node == call || node == function || node == arguments;
  if (SgBinaryOp *member_access = isSgBinaryOp(function)) {
    SgExpression *lhs = member_access->get_lhs_operand();
    SgExpression *rhs = member_access->get_rhs_operand();
    if ((isSgDotExp(member_access) == nullptr &&
         isSgArrowExp(member_access) == nullptr) ||
        lhs == nullptr || rhs == nullptr ||
        lhs->get_parent() != member_access ||
        rhs->get_parent() != member_access) {
      fprintf(stderr,
              "REX_TOKEN_INVARIANT[transparent-interval-carrier]: implicit "
              "conversion call=%p has malformed member-access structure\n",
              static_cast<void *>(call));
      ROSE_ABORT();
    }
    exact_role |= node == rhs;
  }
  if (!exact_role) {
    return nullptr;
  }

  SgStatement *source_statement = SageInterface::getEnclosingStatement(call);
  Sg_File_Info *source_position =
      source_statement != nullptr ? source_statement->get_file_info() : nullptr;
  if (source_position == nullptr || source_position->isCompilerGenerated() ||
      source_position->isFrontendSpecific() ||
      source_position->isTransformation() ||
      source_position->isSourcePositionUnavailableInFrontend() ||
      !source_position->isOutputInCodeGeneration()) {
    // A conversion call retained only in a semantic declaration has no source
    // token interval to carry and is outside the lexical mapping role.
    return nullptr;
  }
  return call;
}

std::optional<TransparentTokenIntervalCarrierRole>
transparentTokenIntervalCarrierRole(SgNode *node, SgSourceFile *source_file) {
  if (node == nullptr || source_file == nullptr ||
      source_file->getFileName().empty()) {
    fprintf(stderr,
            "REX_TOKEN_INVARIANT[transparent-interval-carrier]: carrier "
            "classification has incomplete structural identity\n");
    ROSE_ABORT();
  }

  if (auto *expression = isSgExpression(node)) {
    auto *cast = isSgCastExp(expression);
    if ((cast == nullptr ||
         cast->get_cast_type() != SgCastExp::e_implicit_cast) &&
        SageInterface::hasExactSemanticFrontendSourcePosition(
            expression, expression->get_file_info())) {
      requireExactSynthesizedExpressionCarrier(expression,
                                               "semantic-expression");
      if (cast != nullptr)
        cast->validate_semantic_conversion();
      return TransparentTokenIntervalCarrierRole::semantic_expression_subtree;
    }
  }

  SgType *type = isSgType(node);
  SgFunctionType *signature = isSgFunctionType(node);
  SgFunctionParameterTypeList *parameters = isSgFunctionParameterTypeList(node);
  if ((type != nullptr &&
       (type->has_source_prototype() || type->get_fortran_source_syntax() ||
        (signature != nullptr &&
         signature->get_semantic_signature() != nullptr))) ||
      (parameters != nullptr &&
       parameters->get_source_parameters() != nullptr)) {
    SgNode *owner = node->get_parent();
    ASSERT_not_null(owner);
    std::vector<SgNode *> owned;
    owner->appendDestructorOwnedNodes(owned);
    ROSE_ASSERT(std::count(owned.begin(), owned.end(), node) == 1);
    return TransparentTokenIntervalCarrierRole::source_syntax_structure;
  }

  if (SgCatchStatementSeq *sequence = isSgCatchStatementSeq(node)) {
    SgTryStmt *owner = isSgTryStmt(sequence->get_parent());
    if (owner == nullptr || owner->get_catch_statement_seq_root() != sequence) {
      fprintf(stderr,
              "REX_TOKEN_INVARIANT[transparent-interval-carrier]: catch "
              "sequence=%p has no exact try-statement owner\n",
              static_cast<void *>(sequence));
      ROSE_ABORT();
    }
    Sg_File_Info *owner_position = owner->get_file_info();
    const std::array<Sg_File_Info *, 3> sequence_positions = {
        sequence->get_file_info(), sequence->get_startOfConstruct(),
        sequence->get_endOfConstruct()};
    auto validate_handlers = [&]() {
      for (SgStatement *handler : sequence->get_catch_statement_seq()) {
        if (isSgCatchOptionStmt(handler) == nullptr ||
            handler->get_parent() != sequence) {
          fprintf(stderr,
                  "REX_TOKEN_INVARIANT[transparent-interval-carrier]: catch "
                  "sequence=%p owns malformed handler=%p/%s\n",
                  static_cast<void *>(sequence), static_cast<void *>(handler),
                  handler != nullptr ? handler->class_name().c_str()
                                     : "<null>");
          ROSE_ABORT();
        }
      }
    };
    const bool semantic_only_owner =
        owner_position != nullptr && owner_position->get_parent() == owner &&
        !owner_position->isShared() && owner_position->isCompilerGenerated() &&
        owner_position->isFrontendSpecific() &&
        !owner_position->isTransformation() &&
        !owner_position->isSourcePositionUnavailableInFrontend() &&
        owner_position->isOutputInCodeGeneration() &&
        owner_position->get_file_id() ==
            Sg_File_Info::COMPILER_GENERATED_FILE_ID &&
        owner_position->get_physical_file_id() ==
            Sg_File_Info::COMPILER_GENERATED_FILE_ID;
    if (semantic_only_owner) {
      for (std::size_t position_index = 0;
           position_index < sequence_positions.size(); ++position_index) {
        Sg_File_Info *position = sequence_positions[position_index];
        if (position == nullptr || position->get_parent() != sequence ||
            position->isShared() || !position->isCompilerGenerated() ||
            !position->isFrontendSpecific() || position->isTransformation() ||
            position->isSourcePositionUnavailableInFrontend() ||
            !position->isOutputInCodeGeneration() ||
            position->get_file_id() !=
                Sg_File_Info::COMPILER_GENERATED_FILE_ID ||
            position->get_physical_file_id() !=
                Sg_File_Info::COMPILER_GENERATED_FILE_ID) {
          fprintf(stderr,
                  "REX_TOKEN_INVARIANT[transparent-interval-carrier]: "
                  "semantic catch sequence=%p position-index=%zu lacks exact "
                  "semantic structural provenance\n",
                  static_cast<void *>(sequence), position_index);
          ROSE_ABORT();
        }
      }
      validate_handlers();
      return std::nullopt;
    }
    if (owner_position == nullptr || owner_position->get_parent() != owner ||
        owner_position->get_physical_file_id() < 0) {
      fprintf(
          stderr,
          "REX_TOKEN_INVARIANT[transparent-interval-carrier]: catch "
          "sequence=%p owner=%p position=%p parent=%p file=%d "
          "physical=%d compiler=%d frontend=%d transformation=%d "
          "unavailable=%d output=%d has no exact physical source "
          "identity\n",
          static_cast<void *>(sequence), static_cast<void *>(owner),
          static_cast<void *>(owner_position),
          static_cast<void *>(owner_position != nullptr
                                  ? owner_position->get_parent()
                                  : nullptr),
          owner_position != nullptr ? owner_position->get_file_id()
                                    : Sg_File_Info::BAD_FILE_ID,
          owner_position != nullptr ? owner_position->get_physical_file_id()
                                    : Sg_File_Info::BAD_FILE_ID,
          owner_position != nullptr && owner_position->isCompilerGenerated()
              ? 1
              : 0,
          owner_position != nullptr && owner_position->isFrontendSpecific() ? 1
                                                                            : 0,
          owner_position != nullptr && owner_position->isTransformation() ? 1
                                                                          : 0,
          owner_position != nullptr &&
                  owner_position->isSourcePositionUnavailableInFrontend()
              ? 1
              : 0,
          owner_position != nullptr &&
                  owner_position->isOutputInCodeGeneration()
              ? 1
              : 0);
      ROSE_ABORT();
    }
    const int owner_physical_file_id = owner_position->get_physical_file_id();
    for (std::size_t position_index = 0;
         position_index < sequence_positions.size(); ++position_index) {
      Sg_File_Info *position = sequence_positions[position_index];
      const bool has_preassignment_physical_identity =
          position != nullptr && position->get_physical_file_id() ==
                                     Sg_File_Info::COMPILER_GENERATED_FILE_ID;
      const bool has_assigned_physical_identity =
          position != nullptr &&
          position->get_physical_file_id() == owner_physical_file_id;
      if (position == nullptr || position->get_parent() != sequence ||
          position->isShared() || !position->isCompilerGenerated() ||
          !position->isFrontendSpecific() || position->isTransformation() ||
          position->isSourcePositionUnavailableInFrontend() ||
          !position->isOutputInCodeGeneration() ||
          position->get_file_id() != Sg_File_Info::COMPILER_GENERATED_FILE_ID ||
          has_preassignment_physical_identity ==
              has_assigned_physical_identity) {
        fprintf(stderr,
                "REX_TOKEN_INVARIANT[transparent-interval-carrier]: catch "
                "sequence=%p position-index=%zu position=%p parent=%p "
                "owner-physical-file-id=%d file-id=%d physical-file-id=%d "
                "shared=%d compiler-generated=%d frontend-specific=%d "
                "transformation=%d unavailable=%d output=%d has invalid "
                "structural container provenance\n",
                static_cast<void *>(sequence), position_index,
                static_cast<void *>(position),
                static_cast<void *>(position != nullptr ? position->get_parent()
                                                        : nullptr),
                owner_physical_file_id,
                position != nullptr ? position->get_file_id()
                                    : Sg_File_Info::BAD_FILE_ID,
                position != nullptr ? position->get_physical_file_id()
                                    : Sg_File_Info::BAD_FILE_ID,
                position != nullptr && position->isShared() ? 1 : 0,
                position != nullptr && position->isCompilerGenerated() ? 1 : 0,
                position != nullptr && position->isFrontendSpecific() ? 1 : 0,
                position != nullptr && position->isTransformation() ? 1 : 0,
                position != nullptr &&
                        position->isSourcePositionUnavailableInFrontend()
                    ? 1
                    : 0,
                position != nullptr && position->isOutputInCodeGeneration()
                    ? 1
                    : 0);
        ROSE_ABORT();
      }
    }
    validate_handlers();
    return TransparentTokenIntervalCarrierRole::catch_sequence_structure;
  }

  if (SgTemplateArgument *argument = isSgTemplateArgument(node)) {
    SgNode *exact_child = nullptr;
    switch (argument->get_argumentType()) {
    case SgTemplateArgument::nontype_argument:
      if (argument->get_expression() == nullptr) {
        fprintf(stderr,
                "REX_TOKEN_INVARIANT[transparent-interval-carrier]: "
                "non-type template argument=%p has absent "
                "typed payload\n",
                static_cast<void *>(argument));
        ROSE_ABORT();
      }
      exact_child = argument->get_expression();
      break;
    case SgTemplateArgument::type_argument:
    case SgTemplateArgument::template_template_argument:
    case SgTemplateArgument::start_of_pack_expansion_argument:
      break;
    case SgTemplateArgument::argument_undefined:
    default:
      fprintf(stderr,
              "REX_TOKEN_INVARIANT[transparent-interval-carrier]: template "
              "argument=%p has undefined kind=%d\n",
              static_cast<void *>(argument),
              static_cast<int>(argument->get_argumentType()));
      ROSE_ABORT();
    }

    size_t owned_children = 0;
    for (SgNode *child : argument->get_traversalSuccessorContainer()) {
      if (child != nullptr && child->get_parent() == argument) {
        ++owned_children;
        if (child != exact_child) {
          fprintf(stderr,
                  "REX_TOKEN_INVARIANT[transparent-interval-carrier]: "
                  "template argument=%p owns unexpected child=%p/%s\n",
                  static_cast<void *>(argument), static_cast<void *>(child),
                  child->class_name().c_str());
          ROSE_ABORT();
        }
      }
    }
    const size_t expected_children = exact_child != nullptr ? 1 : 0;
    if (owned_children != expected_children ||
        (exact_child != nullptr && exact_child->get_parent() != argument)) {
      fprintf(stderr,
              "REX_TOKEN_INVARIANT[transparent-interval-carrier]: template "
              "argument=%p owns %zu structural children instead of %zu\n",
              static_cast<void *>(argument), owned_children, expected_children);
      ROSE_ABORT();
    }
    return TransparentTokenIntervalCarrierRole::source_syntax_structure;
  }

  if (SgTemplateParameter *parameter = isSgTemplateParameter(node)) {
    std::set<SgNode *> exact_owned_payloads;
    for (SgNode *payload :
         {static_cast<SgNode *>(parameter->get_initializedName()),
          static_cast<SgNode *>(parameter->get_typeConstraint()),
          static_cast<SgNode *>(parameter->get_defaultExpressionParameter()),
          static_cast<SgNode *>(parameter->get_defaultTemplateArgument())}) {
      if (payload != nullptr && payload->get_parent() == parameter) {
        exact_owned_payloads.insert(payload);
      }
    }

    size_t owned_children = 0;
    for (SgNode *child : parameter->get_traversalSuccessorContainer()) {
      if (child == nullptr || child->get_parent() != parameter) {
        continue;
      }
      ++owned_children;
      if (exact_owned_payloads.count(child) != 1) {
        fprintf(stderr,
                "REX_TOKEN_INVARIANT[transparent-interval-carrier]: "
                "template parameter=%p owns unexpected child=%p/%s\n",
                static_cast<void *>(parameter), static_cast<void *>(child),
                child->class_name().c_str());
        ROSE_ABORT();
      }
    }
    if (owned_children != exact_owned_payloads.size()) {
      fprintf(stderr,
              "REX_TOKEN_INVARIANT[transparent-interval-carrier]: template "
              "parameter=%p publishes %zu traversal children for %zu exact "
              "owned payloads\n",
              static_cast<void *>(parameter), owned_children,
              exact_owned_payloads.size());
      ROSE_ABORT();
    }
    return TransparentTokenIntervalCarrierRole::source_syntax_structure;
  }

  if (SgCastExp *cast = isSgCastExp(node)) {
    if (cast->get_cast_type() != SgCastExp::e_implicit_cast) {
      return std::nullopt;
    }
    cast->validate_semantic_conversion();
    SgExpression *operand = cast->get_operand();
    if (operand == nullptr || operand->get_parent() != cast) {
      fprintf(stderr,
              "REX_TOKEN_INVARIANT[transparent-interval-carrier]: implicit "
              "conversion=%p has no exactly owned operand\n",
              static_cast<void *>(cast));
      ROSE_ABORT();
    }

    enum class ImplicitConversionProvenance { Unset, Semantic, Physical };
    ImplicitConversionProvenance provenance =
        ImplicitConversionProvenance::Unset;
    bool belongs_to_source_file = true;
    size_t position_index = 0;
    for (Sg_File_Info *position :
         {cast->get_file_info(), cast->get_startOfConstruct(),
          cast->get_endOfConstruct(), cast->get_operatorPosition()}) {
      if (position == nullptr || position->get_parent() != cast ||
          position->isShared() || !position->isCompilerGenerated() ||
          position->isTransformation() ||
          position->isSourcePositionUnavailableInFrontend() ||
          !position->isOutputInCodeGeneration() ||
          !position->isImplicitCast()) {
        fprintf(stderr,
                "REX_TOKEN_INVARIANT[transparent-interval-carrier]: implicit "
                "conversion=%p position[%zu]=%p parent=%p shared=%d "
                "compiler-generated=%d transformation=%d unavailable=%d "
                "output=%d implicit-cast=%d line=%d file=%d physical=%d "
                "has incomplete physical-owner "
                "provenance\n",
                static_cast<void *>(cast), position_index,
                static_cast<void *>(position),
                static_cast<void *>(position != nullptr ? position->get_parent()
                                                        : nullptr),
                position != nullptr ? position->isShared() : -1,
                position != nullptr ? position->isCompilerGenerated() : -1,
                position != nullptr ? position->isTransformation() : -1,
                position != nullptr
                    ? position->isSourcePositionUnavailableInFrontend()
                    : -1,
                position != nullptr ? position->isOutputInCodeGeneration() : -1,
                position != nullptr ? position->isImplicitCast() : -1,
                position != nullptr ? position->get_line() : -1,
                position != nullptr ? position->get_file_id() : -1,
                position != nullptr ? position->get_physical_file_id() : -1);
        fprintf(stderr,
                "REX_TOKEN_INVARIANT[transparent-interval-carrier]: implicit "
                "conversion=%p kind=%d parent=%p/%s operand=%p/%s "
                "operand-parent=%p\n",
                static_cast<void *>(cast),
                static_cast<int>(cast->get_semantic_conversion_kind()),
                static_cast<void *>(cast->get_parent()),
                cast->get_parent() != nullptr
                    ? cast->get_parent()->class_name().c_str()
                    : "<null>",
                static_cast<void *>(operand), operand->class_name().c_str(),
                static_cast<void *>(operand->get_parent()));
        ROSE_ABORT();
      }

      const bool exact_semantic =
          position->isFrontendSpecific() &&
          position->get_file_id() == Sg_File_Info::COMPILER_GENERATED_FILE_ID &&
          position->get_physical_file_id() ==
              Sg_File_Info::COMPILER_GENERATED_FILE_ID;
      const bool exact_physical = !position->isFrontendSpecific() &&
                                  position->get_physical_file_id() >= 0 &&
                                  !position->get_physical_filename().empty();
      if (exact_semantic == exact_physical) {
        fprintf(stderr,
                "REX_TOKEN_INVARIANT[transparent-interval-carrier]: implicit "
                "conversion=%p has ambiguous semantic/physical provenance\n",
                static_cast<void *>(cast));
        ROSE_ABORT();
      }

      const ImplicitConversionProvenance current =
          exact_semantic ? ImplicitConversionProvenance::Semantic
                         : ImplicitConversionProvenance::Physical;
      if (provenance == ImplicitConversionProvenance::Unset) {
        provenance = current;
      } else if (provenance != current) {
        fprintf(stderr,
                "REX_TOKEN_INVARIANT[transparent-interval-carrier]: implicit "
                "conversion=%p mixes semantic and physical provenance\n",
                static_cast<void *>(cast));
        ROSE_ABORT();
      }
      if (current == ImplicitConversionProvenance::Physical) {
        belongs_to_source_file &=
            belongsToPhysicalTokenSourceFile(position, source_file);
      }
      ++position_index;
    }
    if (provenance == ImplicitConversionProvenance::Semantic) {
      return TransparentTokenIntervalCarrierRole::semantic_expression_subtree;
    }
    if (provenance != ImplicitConversionProvenance::Physical) {
      fprintf(stderr,
              "REX_TOKEN_INVARIANT[transparent-interval-carrier]: implicit "
              "conversion=%p has no exact provenance role\n",
              static_cast<void *>(cast));
      ROSE_ABORT();
    }
    return belongs_to_source_file
               ? std::optional<TransparentTokenIntervalCarrierRole>(
                     TransparentTokenIntervalCarrierRole::implicit_conversion)
               : std::nullopt;
  }

  if (SgFunctionCallExp *call = exactImplicitConversionCallCarrierOwner(node)) {
    Sg_File_Info *source_position =
        SageInterface::getEnclosingStatement(call)->get_file_info();
    if (!belongsToPhysicalTokenSourceFile(source_position, source_file)) {
      return std::nullopt;
    }
    requireExactSynthesizedExpressionCarrier(
        isSgExpression(node), "implicit-conversion-call-structure");
    return TransparentTokenIntervalCarrierRole::
        implicit_conversion_call_structure;
  }

  if (SgAsmOp *asm_operand = isSgAsmOp(node)) {
    SgAsmStmt *asm_statement = isSgAsmStmt(asm_operand->get_parent());
    SgExpression *operand = asm_operand->get_expression();
    Sg_File_Info *statement_position =
        asm_statement != nullptr ? asm_statement->get_file_info() : nullptr;
    if (asm_statement == nullptr || operand == nullptr ||
        operand->get_parent() != asm_operand ||
        asm_operand->get_constraint().spelling.empty() ||
        std::count(asm_statement->get_operands().begin(),
                   asm_statement->get_operands().end(), asm_operand) != 1) {
      fprintf(stderr,
              "REX_TOKEN_INVARIANT[transparent-interval-carrier]: GNU asm "
              "operand=%p has no exact statement/operand ownership\n",
              static_cast<void *>(asm_operand));
      ROSE_ABORT();
    }
    if (!belongsToPhysicalTokenSourceFile(statement_position, source_file)) {
      return std::nullopt;
    }
    requireExactSynthesizedExpressionCarrier(asm_operand,
                                             "gnu-asm-operand-structure");
    return TransparentTokenIntervalCarrierRole::gnu_asm_operand_structure;
  }

  return std::nullopt;
}

SgNode *shareOwnerForTokenInterval(SgNode *node) {
  if (node != NULL && node->get_parent() != NULL) {
    return node->get_parent();
  }
  return node;
}
} // namespace

// TokenStreamSequenceToNodeMapping_key::TokenStreamSequenceToNodeMapping_key(SgNode*
// n, int input_lower_bound, int input_upper_bound)
TokenStreamSequenceToNodeMapping_key::TokenStreamSequenceToNodeMapping_key(
    SgSourceFile *input_sourceFile, SgNode *n, int input_lower_bound,
    int input_upper_bound) {
  // DQ (4/21/2021): We need to include the SgSourceFile to allow header files
  // to be supported.
  sourceFile = input_sourceFile;

  node = n;
  share_owner = shareOwnerForTokenInterval(n);
  lower_bound = input_lower_bound;
  upper_bound = input_upper_bound;
}

TokenStreamSequenceToNodeMapping_key::TokenStreamSequenceToNodeMapping_key(
    const TokenStreamSequenceToNodeMapping_key &X) {
  // DQ (4/21/2021): We need to include the SgSourceFile to allow header files
  // to be supported.
  sourceFile = X.sourceFile;

  node = X.node;
  share_owner = X.share_owner;
  lower_bound = X.lower_bound;
  upper_bound = X.upper_bound;
}

bool TokenStreamSequenceToNodeMapping_key::operator==(
    const TokenStreamSequenceToNodeMapping_key &X) const {

  bool result =
      (X.sourceFile == sourceFile) && (X.lower_bound == lower_bound) &&
      (X.upper_bound == upper_bound) && (X.share_owner == share_owner);

  return result;
}

bool TokenStreamSequenceToNodeMapping_key::operator<(
    const TokenStreamSequenceToNodeMapping_key &X) const {

  bool result = false;
  if (sourceFile != X.sourceFile) {
    result = std::less<SgSourceFile *>()(sourceFile, X.sourceFile);
  } else if (lower_bound != X.lower_bound) {
    result = lower_bound < X.lower_bound;
  } else if (upper_bound != X.upper_bound) {
    result = upper_bound < X.upper_bound;
  } else {
    result = std::less<SgNode *>()(share_owner, X.share_owner);
  }

  return result;
}

map<TokenStreamSequenceToNodeMapping_key, TokenStreamSequenceToNodeMapping *>
    TokenStreamSequenceToNodeMapping::tokenSequencePool;

TokenStreamHalfOpenInterval
TokenStreamSequenceToNodeMapping::requiredInclusiveDraftInterval(
    SgNode *node, const char *name, int start, int inclusive_end) {
  if (node == nullptr || start < 0 || inclusive_end < start ||
      inclusive_end == std::numeric_limits<int>::max()) {
    fprintf(stderr,
            "REX_TOKEN_INVARIANT[draft-interval]: node=%p/%s required %s "
            "inclusive interval [%d,%d] is invalid\n",
            static_cast<void *>(node),
            node != nullptr ? node->class_name().c_str() : "<null>", name,
            start, inclusive_end);
    ROSE_ABORT();
  }
  return TokenStreamHalfOpenInterval(start, inclusive_end + 1);
}

std::optional<TokenStreamHalfOpenInterval>
TokenStreamSequenceToNodeMapping::optionalInclusiveDraftInterval(
    SgNode *node, const char *name, int start, int inclusive_end) {
  const bool start_absent = start == -1;
  const bool end_absent = inclusive_end == -1;
  if (start_absent != end_absent) {
    fprintf(stderr,
            "REX_TOKEN_INVARIANT[draft-interval]: node=%p/%s optional %s "
            "has half-present inclusive endpoints [%d,%d]\n",
            static_cast<void *>(node),
            node != nullptr ? node->class_name().c_str() : "<null>", name,
            start, inclusive_end);
    ROSE_ABORT();
  }
  if (start_absent) {
    return std::nullopt;
  }
  return requiredInclusiveDraftInterval(node, name, start, inclusive_end);
}

void TokenStreamSequenceToNodeMapping::requireDirectOwnerInterval(
    SgNode *node, const char *source_file,
    std::optional<TokenStreamHalfOpenInterval> interval) {
  if (node == nullptr || source_file == nullptr || *source_file == '\0') {
    fprintf(stderr,
            "REX_TOKEN_INVARIANT[direct-owner-interval]: direct token owner "
            "validation requires an exact node and source file\n");
    ROSE_ABORT();
  }
  if (!interval.has_value() || interval->begin < 0 || interval->empty()) {
    fprintf(stderr,
            "REX_TOKEN_INVARIANT[direct-owner-interval]: node=%p/%s file=%s "
            "parent=%p/%s has no exact inherited or cross-file boundary "
            "interval\n",
            static_cast<void *>(node), node->class_name().c_str(), source_file,
            static_cast<void *>(node->get_parent()),
            node->get_parent() != nullptr
                ? node->get_parent()->class_name().c_str()
                : "<null>");
    ROSE_ABORT();
  }
}

TokenStreamSequenceToNodeMapping::TokenMappingDraft::TokenMappingDraft(
    const TokenStreamHalfOpenInterval &token_subsequence,
    std::optional<TokenStreamHalfOpenInterval> leading_whitespace,
    std::optional<TokenStreamHalfOpenInterval> trailing_whitespace,
    std::optional<TokenStreamHalfOpenInterval> else_whitespace)
    : token_subsequence_(token_subsequence),
      leading_whitespace_(std::move(leading_whitespace)),
      trailing_whitespace_(std::move(trailing_whitespace)),
      else_whitespace_(std::move(else_whitespace)) {
  replaceTokenSubsequence(token_subsequence_);
  replaceLeadingWhitespace(leading_whitespace_);
  replaceTrailingWhitespace(trailing_whitespace_);
  replaceElseWhitespace(else_whitespace_);
}

const TokenStreamHalfOpenInterval &
TokenStreamSequenceToNodeMapping::TokenMappingDraft::tokenSubsequence() const {
  return token_subsequence_;
}

const std::optional<TokenStreamHalfOpenInterval> &
TokenStreamSequenceToNodeMapping::TokenMappingDraft::leadingWhitespace() const {
  return leading_whitespace_;
}

const std::optional<TokenStreamHalfOpenInterval> &
TokenStreamSequenceToNodeMapping::TokenMappingDraft::trailingWhitespace()
    const {
  return trailing_whitespace_;
}

const std::optional<TokenStreamHalfOpenInterval> &
TokenStreamSequenceToNodeMapping::TokenMappingDraft::elseWhitespace() const {
  return else_whitespace_;
}

namespace {
void requireDraftInterval(const char *name,
                          const TokenStreamHalfOpenInterval &interval,
                          bool may_be_empty) {
  if (interval.begin < 0 || interval.end < interval.begin ||
      (!may_be_empty && interval.empty())) {
    fprintf(stderr,
            "REX_TOKEN_INVARIANT[draft-interval]: %s half-open interval "
            "[%d,%d) is invalid\n",
            name, interval.begin, interval.end);
    ROSE_ABORT();
  }
}

[[maybe_unused]] std::string describeDraftInterval(
    const std::optional<TokenStreamHalfOpenInterval> &interval) {
  if (!interval.has_value()) {
    return "<absent>";
  }
  return "[" + StringUtility::numberToString(interval->begin) + "," +
         StringUtility::numberToString(interval->end) + ")";
}

template <class Draft>
void replaceDraftLeadingBegin(Draft &draft, int new_begin) {
  if (!draft.leadingWhitespace().has_value()) {
    fprintf(stderr,
            "REX_TOKEN_INVARIANT[draft-interval]: cannot replace the begin "
            "of absent leading whitespace\n");
    ROSE_ABORT();
  }
  draft.replaceLeadingWhitespace(
      TokenStreamHalfOpenInterval(new_begin, draft.leadingWhitespace()->end));
}

template <class Draft>
void replaceDraftTrailingInclusiveEnd(Draft &draft, int new_inclusive_end) {
  if (!draft.trailingWhitespace().has_value() ||
      new_inclusive_end == std::numeric_limits<int>::max()) {
    fprintf(stderr,
            "REX_TOKEN_INVARIANT[draft-interval]: cannot replace the end of "
            "absent or overflowing trailing whitespace\n");
    ROSE_ABORT();
  }
  draft.replaceTrailingWhitespace(TokenStreamHalfOpenInterval(
      draft.trailingWhitespace()->begin, new_inclusive_end + 1));
}
} // namespace

void TokenStreamSequenceToNodeMapping::TokenMappingDraft::
    replaceTokenSubsequence(
        const TokenStreamHalfOpenInterval &token_subsequence) {
  if (!active_) {
    fprintf(stderr,
            "REX_TOKEN_INVARIANT[draft-mutation]: cannot replace a core "
            "interval after publication\n");
    ROSE_ABORT();
  }
  requireDraftInterval("token-subsequence", token_subsequence, true);
  token_subsequence_ = token_subsequence;
}

void TokenStreamSequenceToNodeMapping::TokenMappingDraft::
    replaceLeadingWhitespace(
        std::optional<TokenStreamHalfOpenInterval> leading_whitespace) {
  if (!active_) {
    fprintf(stderr,
            "REX_TOKEN_INVARIANT[draft-mutation]: cannot replace leading "
            "whitespace after publication\n");
    ROSE_ABORT();
  }
  if (leading_whitespace.has_value()) {
    requireDraftInterval("leading-whitespace", *leading_whitespace, false);
  }
  leading_whitespace_ = std::move(leading_whitespace);
}

void TokenStreamSequenceToNodeMapping::TokenMappingDraft::
    replaceTrailingWhitespace(
        std::optional<TokenStreamHalfOpenInterval> trailing_whitespace) {
  if (!active_) {
    fprintf(stderr,
            "REX_TOKEN_INVARIANT[draft-mutation]: cannot replace trailing "
            "whitespace after publication\n");
    ROSE_ABORT();
  }
  if (trailing_whitespace.has_value()) {
    requireDraftInterval("trailing-whitespace", *trailing_whitespace, false);
  }
  trailing_whitespace_ = std::move(trailing_whitespace);
}

void TokenStreamSequenceToNodeMapping::TokenMappingDraft::replaceElseWhitespace(
    std::optional<TokenStreamHalfOpenInterval> else_whitespace) {
  if (!active_) {
    fprintf(stderr, "REX_TOKEN_INVARIANT[draft-mutation]: cannot replace else "
                    "whitespace after publication\n");
    ROSE_ABORT();
  }
  if (else_whitespace.has_value()) {
    requireDraftInterval("else-whitespace", *else_whitespace, false);
  }
  else_whitespace_ = std::move(else_whitespace);
}

bool TokenStreamSequenceToNodeMapping::TokenMappingDraft::active() const {
  return active_;
}

void TokenStreamSequenceToNodeMapping::TokenMappingDraft::finish() {
  if (!active_) {
    fprintf(stderr,
            "REX_TOKEN_INVARIANT[draft-publication]: token mapping draft was "
            "finished more than once\n");
    ROSE_ABORT();
  }
  active_ = false;
}

TokenStreamSequenceToNodeMapping::TokenStreamSequenceToNodeMapping(
    SgNode *n, const TokenStreamHalfOpenInterval &token_subsequence,
    std::optional<TokenStreamHalfOpenInterval> leading_whitespace,
    std::optional<TokenStreamHalfOpenInterval> trailing_whitespace,
    std::optional<TokenStreamHalfOpenInterval> else_whitespace)
    : node(n), shared(false),
      constructedInEvaluationOfSynthesizedAttribute(false),
      construction_(token_subsequence, std::move(leading_whitespace),
                    std::move(trailing_whitespace), std::move(else_whitespace)),
      published_(false) {}

TokenStreamSequenceToNodeMapping::TokenMappingDraft &
TokenStreamSequenceToNodeMapping::constructionState(
    TokenStreamMappingConstructionKey) {
  if (!construction_.active() || published_) {
    fprintf(stderr,
            "REX_TOKEN_INVARIANT[construction-state]: node=%p/%s has no "
            "active token-mapping construction state\n",
            static_cast<void *>(node),
            node != nullptr ? node->class_name().c_str() : "<null>");
    ROSE_ABORT();
  }
  return construction_;
}

const TokenStreamSequenceToNodeMapping::TokenMappingDraft &
TokenStreamSequenceToNodeMapping::constructionState(
    TokenStreamMappingConstructionKey) const {
  if (!construction_.active() || published_) {
    fprintf(stderr,
            "REX_TOKEN_INVARIANT[construction-state]: node=%p/%s has no "
            "active token-mapping construction state\n",
            static_cast<void *>(node),
            node != nullptr ? node->class_name().c_str() : "<null>");
    ROSE_ABORT();
  }
  return construction_;
}

const TokenStreamHalfOpenInterval &
TokenStreamSequenceToNodeMapping::halfOpenInterval(
    TokenStreamIntervalKind kind) const {
  if (!published_ || construction_.active()) {
    fprintf(stderr,
            "REX_TOKEN_INVARIANT[published-interval]: node=%p/%s has no "
            "published token intervals\n",
            static_cast<void *>(node),
            node != nullptr ? node->class_name().c_str() : "<null>");
    ROSE_ABORT();
  }
  switch (kind) {
  case TokenStreamIntervalKind::leading_whitespace:
    return leading_whitespace_interval_;
  case TokenStreamIntervalKind::token_subsequence:
    return token_subsequence_interval_;
  case TokenStreamIntervalKind::trailing_whitespace:
    return trailing_whitespace_interval_;
  case TokenStreamIntervalKind::else_whitespace:
    return else_whitespace_interval_;
  }
  fprintf(stderr,
          "REX_TOKEN_INVARIANT[half-open-interval]: unknown interval kind\n");
  ROSE_ABORT();
}

void TokenStreamSequenceToNodeMapping::publishHalfOpenIntervals(
    size_t token_count) {
  if (published_ || !construction_.active()) {
    fprintf(stderr,
            "REX_TOKEN_INVARIANT[half-open-publication]: node=%p/%s token "
            "intervals were published more than once\n",
            static_cast<void *>(node),
            node != nullptr ? node->class_name().c_str() : "<null>");
    ROSE_ABORT();
  }

  auto require_bounded = [&](const char *name,
                             const TokenStreamHalfOpenInterval &interval) {
    if (interval.begin < 0 || interval.end < interval.begin ||
        static_cast<size_t>(interval.end) > token_count) {
      fprintf(stderr,
              "REX_TOKEN_INVARIANT[half-open-interval]: node=%p/%s %s "
              "interval [%d,%d) is outside [0,%zu)\n",
              static_cast<void *>(node),
              node != nullptr ? node->class_name().c_str() : "<null>", name,
              interval.begin, interval.end, token_count);
      ROSE_ABORT();
    }
  };

  const TokenStreamHalfOpenInterval published_token_subsequence =
      construction_.tokenSubsequence();
  require_bounded("token-subsequence", published_token_subsequence);
  if ((token_count == 0) != published_token_subsequence.empty()) {
    fprintf(stderr,
            "REX_TOKEN_INVARIANT[half-open-interval]: node=%p/%s core "
            "interval [%d,%d) is incompatible with token-count=%zu\n",
            static_cast<void *>(node),
            node != nullptr ? node->class_name().c_str() : "<null>",
            published_token_subsequence.begin, published_token_subsequence.end,
            token_count);
    ROSE_ABORT();
  }
  if (published_token_subsequence.empty() && !isTokenStreamRootNode(node)) {
    fprintf(stderr,
            "REX_TOKEN_INVARIANT[empty-root]: node=%p/%s cannot own the "
            "empty token stream\n",
            static_cast<void *>(node),
            node != nullptr ? node->class_name().c_str() : "<null>");
    ROSE_ABORT();
  }
  const TokenStreamHalfOpenInterval published_leading_whitespace =
      construction_.leadingWhitespace().value_or(
          TokenStreamHalfOpenInterval(published_token_subsequence.begin,
                                      published_token_subsequence.begin));
  const TokenStreamHalfOpenInterval published_trailing_whitespace =
      construction_.trailingWhitespace().value_or(TokenStreamHalfOpenInterval(
          published_token_subsequence.end, published_token_subsequence.end));
  const TokenStreamHalfOpenInterval published_else_whitespace =
      construction_.elseWhitespace().value_or(TokenStreamHalfOpenInterval(
          published_token_subsequence.end, published_token_subsequence.end));
  require_bounded("leading-whitespace", published_leading_whitespace);
  require_bounded("trailing-whitespace", published_trailing_whitespace);
  require_bounded("else-whitespace", published_else_whitespace);

  if (published_leading_whitespace.end != published_token_subsequence.begin) {
    fprintf(stderr,
            "REX_TOKEN_INVARIANT[half-open-interval]: node=%p/%s leading "
            "interval [%d,%d) is not adjacent to core [%d,%d)\n",
            static_cast<void *>(node),
            node != nullptr ? node->class_name().c_str() : "<null>",
            published_leading_whitespace.begin,
            published_leading_whitespace.end, published_token_subsequence.begin,
            published_token_subsequence.end);
    ROSE_ABORT();
  }
  if (published_trailing_whitespace.begin != published_token_subsequence.end) {
    fprintf(stderr,
            "REX_TOKEN_INVARIANT[half-open-interval]: node=%p/%s trailing "
            "interval [%d,%d) is not adjacent to core [%d,%d)\n",
            static_cast<void *>(node),
            node != nullptr ? node->class_name().c_str() : "<null>",
            published_trailing_whitespace.begin,
            published_trailing_whitespace.end,
            published_token_subsequence.begin, published_token_subsequence.end);
    ROSE_ABORT();
  }
  if (!published_else_whitespace.empty() &&
      (published_else_whitespace.begin < published_token_subsequence.begin ||
       published_else_whitespace.end > published_token_subsequence.end)) {
    fprintf(stderr,
            "REX_TOKEN_INVARIANT[half-open-interval]: node=%p/%s else "
            "interval [%d,%d) is outside core [%d,%d)\n",
            static_cast<void *>(node),
            node != nullptr ? node->class_name().c_str() : "<null>",
            published_else_whitespace.begin, published_else_whitespace.end,
            published_token_subsequence.begin, published_token_subsequence.end);
    ROSE_ABORT();
  }

  leading_whitespace_interval_ = published_leading_whitespace;
  token_subsequence_interval_ = published_token_subsequence;
  trailing_whitespace_interval_ = published_trailing_whitespace;
  else_whitespace_interval_ = published_else_whitespace;
  construction_.finish();
  published_ = true;
}

TokenStreamSequenceToNodeMapping *
TokenStreamSequenceToNodeMapping::createPublished(
    SgNode *node, const TokenStreamHalfOpenInterval &leading_whitespace,
    const TokenStreamHalfOpenInterval &token_subsequence,
    const TokenStreamHalfOpenInterval &trailing_whitespace,
    const TokenStreamHalfOpenInterval &else_whitespace, size_t token_count) {
  return createPublished(NodeAssociation{node, {node}, false, false},
                         leading_whitespace, token_subsequence,
                         trailing_whitespace, else_whitespace, token_count);
}

TokenStreamSequenceToNodeMapping *
TokenStreamSequenceToNodeMapping::createPublished(
    const NodeAssociation &association,
    const TokenStreamHalfOpenInterval &leading_whitespace,
    const TokenStreamHalfOpenInterval &token_subsequence,
    const TokenStreamHalfOpenInterval &trailing_whitespace,
    const TokenStreamHalfOpenInterval &else_whitespace, size_t token_count) {
  SgNode *node = association.owner;
  std::set<SgNode *> associated;
  for (SgNode *entry : association.nodes) {
    if (entry == nullptr || !associated.insert(entry).second) {
      fprintf(stderr, "REX_TOKEN_INVARIANT[published-association]: "
                      "token association contains a null or duplicate node\n");
      ROSE_ABORT();
    }
  }
  if (associated.count(node) != 1 ||
      (association.shared && association.nodes.size() < 2)) {
    fprintf(stderr,
            "REX_TOKEN_INVARIANT[published-association]: "
            "token association requires its exact owner and sharing set\n");
    ROSE_ABORT();
  }
  if (node == nullptr ||
      token_count > static_cast<size_t>(std::numeric_limits<int>::max())) {
    fprintf(stderr,
            "REX_TOKEN_INVARIANT[published-interval]: published token mapping "
            "requires a node and an int-sized token stream\n");
    ROSE_ABORT();
  }
  auto require_bounded = [&](const char *name,
                             const TokenStreamHalfOpenInterval &interval) {
    if (interval.begin < 0 || interval.end < interval.begin ||
        static_cast<size_t>(interval.end) > token_count) {
      fprintf(stderr,
              "REX_TOKEN_INVARIANT[published-interval]: node=%p/%s %s "
              "interval [%d,%d) is outside [0,%zu)\n",
              static_cast<void *>(node), node->class_name().c_str(), name,
              interval.begin, interval.end, token_count);
      ROSE_ABORT();
    }
  };
  require_bounded("leading-whitespace", leading_whitespace);
  require_bounded("token-subsequence", token_subsequence);
  require_bounded("trailing-whitespace", trailing_whitespace);
  require_bounded("else-whitespace", else_whitespace);
  if ((token_count != 0 && token_subsequence.empty()) ||
      leading_whitespace.end != token_subsequence.begin ||
      trailing_whitespace.begin != token_subsequence.end ||
      (!else_whitespace.empty() &&
       (else_whitespace.begin < token_subsequence.begin ||
        else_whitespace.end > token_subsequence.end)) ||
      (else_whitespace.empty() &&
       else_whitespace.begin != token_subsequence.end)) {
    fprintf(stderr,
            "REX_TOKEN_INVARIANT[published-interval]: node=%p/%s has "
            "inconsistent intervals leading=[%d,%d) core=[%d,%d) "
            "trailing=[%d,%d) else=[%d,%d)\n",
            static_cast<void *>(node), node->class_name().c_str(),
            leading_whitespace.begin, leading_whitespace.end,
            token_subsequence.begin, token_subsequence.end,
            trailing_whitespace.begin, trailing_whitespace.end,
            else_whitespace.begin, else_whitespace.end);
    ROSE_ABORT();
  }

  auto optional_nonempty = [](const TokenStreamHalfOpenInterval &interval)
      -> std::optional<TokenStreamHalfOpenInterval> {
    return interval.empty()
               ? std::nullopt
               : std::optional<TokenStreamHalfOpenInterval>(interval);
  };
  TokenStreamSequenceToNodeMapping *mapping =
      TokenStreamMappingConstructionAccess::construct(
          node, token_subsequence, optional_nonempty(leading_whitespace),
          optional_nonempty(trailing_whitespace),
          optional_nonempty(else_whitespace));
  ASSERT_not_null(mapping);
  mapping->nodeVector = association.nodes;
  mapping->shared = association.shared;
  mapping->constructedInEvaluationOfSynthesizedAttribute =
      association.constructedInSynthesizedAttribute;
  TokenStreamMappingConstructionAccess::publish(mapping, token_count);
  return mapping;
}

bool isExactEmptyTranslationUnitTokenMapping(
    SgSourceFile *sourceFile, SgStatement *statement,
    TokenStreamSequenceToNodeMapping *mapping) {
  if (sourceFile == nullptr || statement == nullptr || mapping == nullptr ||
      !sourceFile->get_token_list().empty()) {
    return false;
  }

  SgGlobal *globalScope = sourceFile->get_globalScope();
  if (globalScope == nullptr || statement != globalScope ||
      globalScope->get_parent() != sourceFile) {
    return false;
  }

  const auto &tokenMap = sourceFile->get_tokenSubsequenceMap();
  const auto fileEntry = tokenMap.find(sourceFile);
  const auto globalEntry = tokenMap.find(globalScope);
  if (tokenMap.size() != 2 || fileEntry == tokenMap.end() ||
      globalEntry == tokenMap.end() || globalEntry->second != mapping ||
      fileEntry->second == nullptr || fileEntry->second == mapping) {
    return false;
  }

  auto isExactRoot = [](TokenStreamSequenceToNodeMapping *rootMapping,
                        SgNode *root) {
    if (rootMapping == nullptr || rootMapping->node != root ||
        rootMapping->shared || rootMapping->nodeVector.size() != 1 ||
        rootMapping->nodeVector.front() != root) {
      return false;
    }
    for (TokenStreamIntervalKind kind :
         {TokenStreamIntervalKind::leading_whitespace,
          TokenStreamIntervalKind::token_subsequence,
          TokenStreamIntervalKind::trailing_whitespace,
          TokenStreamIntervalKind::else_whitespace}) {
      const TokenStreamHalfOpenInterval &interval =
          rootMapping->halfOpenInterval(kind);
      if (interval.begin != 0 || interval.end != 0) {
        return false;
      }
    }
    return true;
  };

  return isExactRoot(fileEntry->second, sourceFile) &&
         isExactRoot(mapping, globalScope);
}

// Intern a typed draft for one non-empty token interval in this source file.
TokenStreamSequenceToNodeMapping *
TokenStreamSequenceToNodeMapping::createTokenInterval(
    SgSourceFile *sourceFile, SgNode *n, size_t token_count,
    const TokenStreamHalfOpenInterval &token_subsequence,
    std::optional<TokenStreamHalfOpenInterval> leading_whitespace,
    std::optional<TokenStreamHalfOpenInterval> trailing_whitespace,
    std::optional<TokenStreamHalfOpenInterval> else_whitespace) {
  if (sourceFile == nullptr || n == nullptr || token_count == 0) {
    fprintf(stderr,
            "REX_TOKEN_INVARIANT[draft-construction]: token interval "
            "requires a source file, node, and non-empty token stream\n");
    ROSE_ABORT();
  }
  if (SgDeclarationStatement *declaration = isSgDeclarationStatement(n)) {
    if (!declarationRequiresTokenMapping(declaration, sourceFile)) {
      fprintf(stderr,
              "REX_TOKEN_INVARIANT[draft-construction]: nonlexical "
              "declaration=%p/%s cannot acquire a direct token interval\n",
              static_cast<void *>(declaration),
              declaration->class_name().c_str());
      ROSE_ABORT();
    }
  }
  requireDraftInterval("token-subsequence", token_subsequence, false);
  if (static_cast<size_t>(token_subsequence.end) > token_count) {
    fprintf(stderr,
            "REX_TOKEN_INVARIANT[draft-construction]: token interval "
            "[%d,%d) exceeds source stream size %zu\n",
            token_subsequence.begin, token_subsequence.end, token_count);
    ROSE_ABORT();
  }
  if (SgDeclarationGroupStatement *group = declarationGroupOwner(n)) {
    fprintf(stderr,
            "REX_TOKEN_INVARIANT[declaration-group]: member=%p/%s group=%p "
            "cannot own a direct token interval\n",
            static_cast<void *>(n), n->class_name().c_str(),
            static_cast<void *>(group));
    ROSE_ABORT();
  }

  // The token interval is unique and using it we define an interval tree (of
  // tokens) on the AST to separate the token stream over the AST IR nodes. This
  // function defines a set which used a unique key for any possible interval.

  // Generate the key from the node and token subsequence interval.
  // TokenStreamSequenceToNodeMapping_key
  // key(n,input_token_subsequence_start,input_token_subsequence_end);
  TokenStreamSequenceToNodeMapping_key key(
      sourceFile, n, token_subsequence.begin, token_subsequence.end - 1);

  TokenStreamSequenceToNodeMapping *newTokenSequence = NULL;
  // map<size_t,TokenStreamSequenceToNodeMapping*>::iterator iter =
  // tokenSequencePool.find(key);
  map<TokenStreamSequenceToNodeMapping_key,
      TokenStreamSequenceToNodeMapping *>::iterator iter =
      tokenSequencePool.find(key);

  if (iter != tokenSequencePool.end()) {
    // This branch will permit sharing of a previously built
    // TokenStreamSequenceToNodeMapping.
    newTokenSequence = iter->second;
    ROSE_ASSERT(newTokenSequence != NULL);
    const bool nodeAlreadyAssociated =
        std::find(newTokenSequence->nodeVector.begin(),
                  newTokenSequence->nodeVector.end(),
                  n) != newTokenSequence->nodeVector.end();
    if (!nodeAlreadyAssociated) {
      newTokenSequence->nodeVector.push_back(n);
      newTokenSequence->shared = true;
    }
    newTokenSequence->shared = newTokenSequence->nodeVector.size() > 1;
  } else {
    // This branch will force a new TokenStreamSequenceToNodeMapping to be
    // built.
    newTokenSequence = new TokenStreamSequenceToNodeMapping(
        n, token_subsequence, std::move(leading_whitespace),
        std::move(trailing_whitespace), std::move(else_whitespace));
    ROSE_ASSERT(newTokenSequence != NULL);
    newTokenSequence->shared = false;

    ROSE_ASSERT(newTokenSequence->shared == false);

    // Add the input SgNode* to the list of IR nodes sharing this
    // TokenStreamSequenceToNodeMapping.
    newTokenSequence->nodeVector.push_back(n);

    // tokenSequencePool.insert(pair<size_t,TokenStreamSequenceToNodeMapping*>(key,newTokenSequence));
    tokenSequencePool.insert(
        pair<TokenStreamSequenceToNodeMapping_key,
             TokenStreamSequenceToNodeMapping *>(key, newTokenSequence));
  }

  return newTokenSequence;
}

void TokenStreamSequenceToNodeMapping::retireDetachedMapping(
    TokenStreamSequenceToNodeMapping *mapping) {
  if (mapping == nullptr || !mapping->nodeVector.empty()) {
    fprintf(stderr,
            "REX_TOKEN_INVARIANT[mapping-detachment]: retired mapping=%p "
            "retains %zu reverse associations\n",
            static_cast<void *>(mapping),
            mapping != nullptr ? mapping->nodeVector.size() : 0);
    ROSE_ABORT();
  }

  auto ownedEntry = tokenSequencePool.end();
  size_t poolEntries = 0;
  for (auto entry = tokenSequencePool.begin(); entry != tokenSequencePool.end();
       ++entry) {
    if (entry->second == mapping) {
      ownedEntry = entry;
      ++poolEntries;
    }
  }
  if (poolEntries != 1) {
    fprintf(stderr,
            "REX_TOKEN_INVARIANT[mapping-detachment]: retired mapping=%p "
            "owned %zu token-pool entries instead of one\n",
            static_cast<void *>(mapping), poolEntries);
    ROSE_ABORT();
  }

  tokenSequencePool.erase(ownedEntry);
  delete mapping;
}

class InheritedAttribute {
public:
  enum class SubtreeRole { Lexical, Semantic };
  SgSourceFile *sourceFile;
  SubtreeRole subtreeRole;

  // Detect when to stop processing deeper into the AST.
  bool processChildNodes;

  // The traversal either owns one exact half-open token interval or no token
  // interval at all.  Absence is not an interval and must never be promoted to
  // the whole source file by a descendant.
  std::optional<TokenStreamHalfOpenInterval> token_interval;

  SgNode *node;

  InheritedAttribute(
      SgSourceFile *input_sourceFile, SgNode *n,
      std::optional<TokenStreamHalfOpenInterval> input_token_interval,
      bool processed, SubtreeRole role = SubtreeRole::Lexical);

  InheritedAttribute(const InheritedAttribute &X);
};

InheritedAttribute::InheritedAttribute(
    SgSourceFile *input_sourceFile, SgNode *n,
    std::optional<TokenStreamHalfOpenInterval> input_token_interval,
    bool processed, SubtreeRole role)
    : sourceFile(input_sourceFile), subtreeRole(role),
      processChildNodes(processed),
      token_interval(std::move(input_token_interval)), node(n) {
  ROSE_ASSERT(subtreeRole != SubtreeRole::Semantic ||
              (!processChildNodes && !token_interval.has_value()));
  if (token_interval.has_value() &&
      (token_interval->begin < 0 || token_interval->empty())) {
    fprintf(stderr,
            "REX_TOKEN_INVARIANT[inherited-interval]: node=%p/%s has invalid "
            "half-open token interval [%d,%d)\n",
            static_cast<void *>(node),
            node != nullptr ? node->class_name().c_str() : "<null>",
            token_interval->begin, token_interval->end);
    ROSE_ABORT();
  }
}

InheritedAttribute::InheritedAttribute(const InheritedAttribute &X)
    : sourceFile(X.sourceFile), subtreeRole(X.subtreeRole),
      processChildNodes(X.processChildNodes), token_interval(X.token_interval),
      node(X.node) {}

class TokenMappingSynthesizedAttribute {
public:
  SgNode *node = nullptr;

  TokenMappingSynthesizedAttribute() = default;
  explicit TokenMappingSynthesizedAttribute(SgNode *n) : node(n) {
    ROSE_ASSERT(n != nullptr);
  }
};

class TokenMappingTraversal
    : public SgTopDownBottomUpProcessing<InheritedAttribute,
                                         TokenMappingSynthesizedAttribute> {
public:
  vector<stream_element *> &tokenStream;
  SgSourceFile *sourceFile;

  // DQ (1/20/2021): Changed the API to use a reference to the
  // map<SgNode*,TokenStreamSequenceToNodeMapping*>. This is the map of
  // subsequences of the token sequence to the ROSE AST IR nodes.
  // map<SgNode*,pair<int,int> > tokenStreamSequenceMap;
  // map<SgNode*,TokenStreamSequenceToNodeMapping*>* tokenStreamSequenceMap;
  map<SgNode *, TokenStreamSequenceToNodeMapping *> &tokenStreamSequenceMap;

  // We need an ordered sequence to check between the current and last element.
  // vector<pair<SgNode*,pair<int,int> > > tokenStreamSequenceVector;
  vector<TokenStreamSequenceToNodeMapping *> tokenStreamSequenceVector;

  // DQ (11/20/2015): Provide a statement to use as a key in the token sequence
  // map to get representative whitespace. This is required to format
  // transformed statements in scopes (especially required if all statements are
  // transformed as a part of a larger transformation of the file.  The
  // representative statements white space is used to format the code unparsed
  // from the AST.
  map<SgScopeStatement *, SgStatement *> representativeWhitespaceStatementMap;

  // DQ (1/20/2021): Changed the API to add a pointer to the token map.
  TokenMappingTraversal(vector<stream_element *> &tokenStream,
                        SgSourceFile *input_sourceFile,
                        std::map<SgNode *, TokenStreamSequenceToNodeMapping *>
                            *tokenStreamSequenceMapPointer);

  // virtual function must be defined
  InheritedAttribute
  evaluateInheritedAttribute(SgNode *n, InheritedAttribute inheritedAttribute);

  // virtual function must be defined
  TokenMappingSynthesizedAttribute evaluateSynthesizedAttribute(
      SgNode *n, InheritedAttribute inheritedAttribute,
      SubTreeSynthesizedAttributes synthesizedAttributeList);

  void validateConstructionConsistency() const;

  // DQ (12/15/2014): refactoring code for reuse.
  void
  trimLeadingWhiteSpaceFromLeft(TokenStreamSequenceToNodeMapping *mappingInfo,
                                int original_start_of_token_subsequence);
  void
  trimTrailingWhiteSpaceFromRight(TokenStreamSequenceToNodeMapping *mappingInfo,
                                  int original_end_of_token_subsequence);

  // DQ (12/31/2014): Compute the location of the tokens associate with the else
  // syntax.
  void
  discoverElseSyntax(TokenStreamSequenceToNodeMapping *if_statement_mappingInfo,
                     TokenStreamSequenceToNodeMapping *true_body_mappingInfo,
                     TokenStreamSequenceToNodeMapping *false_body_mappingInfo);

  // Centralize the active source file for this traversal.
  SgSourceFile *currentSourceFile() const { return sourceFile; }
};

void TokenMappingTraversal::validateConstructionConsistency() const {
  std::set<TokenStreamSequenceToNodeMapping *> vector_mappings;
  for (size_t index = 0; index < tokenStreamSequenceVector.size(); ++index) {
    TokenStreamSequenceToNodeMapping *mapping =
        tokenStreamSequenceVector[index];
    if (mapping == nullptr || mapping->node == nullptr) {
      fprintf(stderr,
              "REX_TOKEN_INVARIANT[construction-consistency]: file=%s "
              "vector-index=%zu has a null mapping or owner\n",
              sourceFile->getFileName().c_str(), index);
      ROSE_ABORT();
    }
    vector_mappings.insert(mapping);
  }

  std::set<TokenStreamSequenceToNodeMapping *> map_mappings;
  std::map<TokenStreamSequenceToNodeMapping *, std::set<SgNode *>>
      map_keys_by_mapping;
  for (const auto &entry : tokenStreamSequenceMap) {
    SgNode *key = entry.first;
    TokenStreamSequenceToNodeMapping *mapping = entry.second;
    if (key == nullptr || mapping == nullptr || mapping->node == nullptr) {
      fprintf(stderr,
              "REX_TOKEN_INVARIANT[construction-consistency]: file=%s map "
              "contains a null key, mapping, or owner\n",
              sourceFile->getFileName().c_str());
      ROSE_ABORT();
    }
    if (std::find(mapping->nodeVector.begin(), mapping->nodeVector.end(),
                  key) == mapping->nodeVector.end()) {
      fprintf(stderr,
              "REX_TOKEN_INVARIANT[construction-consistency]: file=%s "
              "key=%p/%s is absent from mapping=%p associated nodes\n",
              sourceFile->getFileName().c_str(), static_cast<void *>(key),
              key->class_name().c_str(), static_cast<void *>(mapping));
      ROSE_ABORT();
    }
    map_mappings.insert(mapping);
    map_keys_by_mapping[mapping].insert(key);
  }

  if (map_mappings != vector_mappings) {
    fprintf(stderr,
            "REX_TOKEN_INVARIANT[construction-consistency]: file=%s map has "
            "%zu unique mappings but traversal vector has %zu\n",
            sourceFile->getFileName().c_str(), map_mappings.size(),
            vector_mappings.size());
    ROSE_ABORT();
  }

  for (TokenStreamSequenceToNodeMapping *mapping : map_mappings) {
    std::set<SgNode *> associated_nodes;
    for (SgNode *node : mapping->nodeVector) {
      if (node == nullptr || !associated_nodes.insert(node).second) {
        fprintf(stderr,
                "REX_TOKEN_INVARIANT[construction-consistency]: file=%s "
                "mapping=%p has a null or duplicate associated node\n",
                sourceFile->getFileName().c_str(),
                static_cast<void *>(mapping));
        ROSE_ABORT();
      }
    }
    if (associated_nodes.find(mapping->node) == associated_nodes.end() ||
        mapping->shared != (associated_nodes.size() > 1) ||
        associated_nodes != map_keys_by_mapping.at(mapping)) {
      fprintf(stderr,
              "REX_TOKEN_INVARIANT[construction-consistency]: file=%s "
              "mapping=%p owner=%p/%s has %zu associated nodes, %zu map "
              "keys, and shared=%s\n",
              sourceFile->getFileName().c_str(), static_cast<void *>(mapping),
              static_cast<void *>(mapping->node),
              mapping->node->class_name().c_str(), associated_nodes.size(),
              map_keys_by_mapping.at(mapping).size(),
              mapping->shared ? "true" : "false");
      ROSE_ABORT();
    }
  }
}

void TokenMappingTraversal::trimLeadingWhiteSpaceFromLeft(
    TokenStreamSequenceToNodeMapping *mappingInfo,
    int original_start_of_token_subsequence) {
  // Search backward to the first nontrivia token.

  int leading_whitespace_end =
      mappingInfo
          ->constructionState(TokenStreamMappingConstructionAccess::key())
          .tokenSubsequence()
          .begin -
      1;
  int leading_whitespace_start = leading_whitespace_end;

  // DQ (1/2/2015): There is no leading white space at the start of the token
  // sequence (by definition). This case happends for the trivial case of
  // SgGlobal. if (original_start_of_token_subsequence == 0)
  if (leading_whitespace_start < 0) {
    return;
  }
  ROSE_ASSERT(leading_whitespace_start >= 0);

  // DQ (12/5/2016): Eliminate warning that we want to consider an error:
  // -Wsign-compare ROSE_ASSERT(leading_whitespace_start < tokenStream.size());
  ROSE_ASSERT((size_t)leading_whitespace_start < tokenStream.size());

  if (leading_whitespace_start > 0 &&
      (tokenStream[leading_whitespace_start]->p_tok_elem->token_id ==
           C_CXX_WHITESPACE ||
       tokenStream[leading_whitespace_start]->p_tok_elem->token_id ==
           C_CXX_PREPROCESSING_INFO ||
       tokenStream[leading_whitespace_start]->p_tok_elem->token_id ==
           C_CXX_COMMENTS)) {

    ROSE_ASSERT(leading_whitespace_start > 0);
    ROSE_ASSERT(static_cast<size_t>(leading_whitespace_start - 1) <
                tokenStream.size());

    while ((leading_whitespace_start > 0) &&
           (leading_whitespace_start > original_start_of_token_subsequence) &&
           (tokenStream[leading_whitespace_start - 1]->p_tok_elem->token_id ==
                C_CXX_WHITESPACE ||
            tokenStream[leading_whitespace_start - 1]->p_tok_elem->token_id ==
                C_CXX_PREPROCESSING_INFO ||
            tokenStream[leading_whitespace_start - 1]->p_tok_elem->token_id ==
                C_CXX_COMMENTS)) {
      ROSE_ASSERT(leading_whitespace_start >
                  original_start_of_token_subsequence);

      leading_whitespace_start--;

      ROSE_ASSERT((size_t)leading_whitespace_start < tokenStream.size());
    }

    // If the positions are out of bounds then we don't have any leading
    // whitespace.
    if (leading_whitespace_start < original_start_of_token_subsequence &&
        leading_whitespace_end < original_start_of_token_subsequence) {
      leading_whitespace_start = -1;
      leading_whitespace_end = -1;
    }
  } else {
    // DQ (5/1/2021): test17 of UnparseHeadersUsingTokenStream_tests
    // demonstrates that when the leading_whitespace_start is set to zero, we
    // can't just make it undefined. leading_whitespace_start = -1;
    // leading_whitespace_end   = -1;
    if (leading_whitespace_start == 0) {
      leading_whitespace_start = 0;
      leading_whitespace_end = 0;
    } else {
      leading_whitespace_start = -1;
      leading_whitespace_end = -1;
    }
  }

  mappingInfo->constructionState(TokenStreamMappingConstructionAccess::key())
      .replaceLeadingWhitespace(
          TokenStreamMappingConstructionAccess::optionalInclusiveInterval(
              mappingInfo->node, "leading-whitespace", leading_whitespace_start,
              leading_whitespace_end));

  const auto &leading =
      mappingInfo
          ->constructionState(TokenStreamMappingConstructionAccess::key())
          .leadingWhitespace();
  ROSE_ASSERT(!leading.has_value() || leading->begin < leading->end);
}

void TokenMappingTraversal::trimTrailingWhiteSpaceFromRight(
    TokenStreamSequenceToNodeMapping *mappingInfo,
    int original_end_of_token_subsequence) {
  // Search forward to the first nontrivia token.

  int trailing_whitespace_start =
      mappingInfo
          ->constructionState(TokenStreamMappingConstructionAccess::key())
          .tokenSubsequence()
          .end -
      1 + 1;
  int trailing_whitespace_end = trailing_whitespace_start;

  // DQ (1/2/2015): There is no trailing white space at the end of the token
  // sequence (by definition). This case happends for the trivial case of
  // SgGlobal. if (original_end_of_token_subsequence == tokenStream.size()-1)

  // DQ (12/5/2016): Eliminate warning that we want to consider an error:
  // -Wsign-compare if (trailing_whitespace_end > tokenStream.size()-1)
  if ((size_t)trailing_whitespace_end >= tokenStream.size()) {
    mappingInfo->constructionState(TokenStreamMappingConstructionAccess::key())
        .replaceTrailingWhitespace(std::nullopt);
    return;
  }
  ROSE_ASSERT(trailing_whitespace_end >= 0);

  // DQ (12/5/2016): Eliminate warning that we want to consider an error:
  // -Wsign-compare ROSE_ASSERT(trailing_whitespace_end < tokenStream.size());
  ROSE_ASSERT((size_t)trailing_whitespace_end < tokenStream.size());

  if (tokenStream[trailing_whitespace_end]->p_tok_elem->token_id ==
          C_CXX_WHITESPACE ||
      tokenStream[trailing_whitespace_end]->p_tok_elem->token_id ==
          C_CXX_PREPROCESSING_INFO ||
      tokenStream[trailing_whitespace_end]->p_tok_elem->token_id ==
          C_CXX_COMMENTS) {
    while (trailing_whitespace_end < original_end_of_token_subsequence &&
           (tokenStream[trailing_whitespace_end + 1]->p_tok_elem->token_id ==
                C_CXX_WHITESPACE ||
            tokenStream[trailing_whitespace_end + 1]->p_tok_elem->token_id ==
                C_CXX_PREPROCESSING_INFO ||
            tokenStream[trailing_whitespace_end + 1]->p_tok_elem->token_id ==
                C_CXX_COMMENTS)) {
      ROSE_ASSERT(trailing_whitespace_end < original_end_of_token_subsequence);

      trailing_whitespace_end++;
    }

    // If the positions are out of bounds then we don't have any leading
    // whitespace.
    if (trailing_whitespace_end > original_end_of_token_subsequence &&
        trailing_whitespace_start > original_end_of_token_subsequence) {
      trailing_whitespace_start = -1;
      trailing_whitespace_end = -1;
    }
  } else {
    trailing_whitespace_start = -1;
    trailing_whitespace_end = -1;
  }

  mappingInfo->constructionState(TokenStreamMappingConstructionAccess::key())
      .replaceTrailingWhitespace(
          TokenStreamMappingConstructionAccess::optionalInclusiveInterval(
              mappingInfo->node, "trailing-whitespace",
              trailing_whitespace_start, trailing_whitespace_end));

  const auto &trailing =
      mappingInfo
          ->constructionState(TokenStreamMappingConstructionAccess::key())
          .trailingWhitespace();
  ROSE_ASSERT(!trailing.has_value() || trailing->begin < trailing->end);
}

void TokenMappingTraversal::discoverElseSyntax(
    TokenStreamSequenceToNodeMapping *if_statement_mappingInfo,
    TokenStreamSequenceToNodeMapping *true_body_mappingInfo,
    TokenStreamSequenceToNodeMapping *false_body_mappingInfo) {
  // Search forward to the first nontrivia token.

  int else_whitespace_start =
      true_body_mappingInfo
          ->constructionState(TokenStreamMappingConstructionAccess::key())
          .tokenSubsequence()
          .end -
      1 + 1;
  int else_whitespace_end = else_whitespace_start;

  int original_end_of_token_subsequence =
      false_body_mappingInfo
          ->constructionState(TokenStreamMappingConstructionAccess::key())
          .tokenSubsequence()
          .begin +
      0;

  if (tokenStream[else_whitespace_end]->p_tok_elem->token_id ==
          C_CXX_WHITESPACE ||
      tokenStream[else_whitespace_end]->p_tok_elem->token_id ==
          C_CXX_PREPROCESSING_INFO ||
      tokenStream[else_whitespace_end]->p_tok_elem->token_id ==
          C_CXX_COMMENTS) {
    while (else_whitespace_end < original_end_of_token_subsequence &&
           (tokenStream[else_whitespace_end]->p_tok_elem->token_id ==
                C_CXX_WHITESPACE ||
            tokenStream[else_whitespace_end]->p_tok_elem->token_id ==
                C_CXX_PREPROCESSING_INFO ||
            tokenStream[else_whitespace_end]->p_tok_elem->token_id ==
                C_CXX_COMMENTS)) {
      ROSE_ASSERT(else_whitespace_end < original_end_of_token_subsequence);

      else_whitespace_end++;
    }

    // If the positions are out of bounds then we don't have any leading
    // whitespace.
    if (else_whitespace_end > original_end_of_token_subsequence &&
        else_whitespace_start > original_end_of_token_subsequence) {
      else_whitespace_start = -1;
      else_whitespace_end = -1;
    }
  } else {
    // No leading whitespace; preserve the else token position.
  }

  // Set the else_whitespace_start to the end (since "else" is a single token.
  else_whitespace_start = else_whitespace_end;

  if_statement_mappingInfo
      ->constructionState(TokenStreamMappingConstructionAccess::key())
      .replaceElseWhitespace(
          TokenStreamMappingConstructionAccess::optionalInclusiveInterval(
              if_statement_mappingInfo->node, "else-whitespace",
              else_whitespace_start, else_whitespace_end));

  const auto &else_interval =
      if_statement_mappingInfo
          ->constructionState(TokenStreamMappingConstructionAccess::key())
          .elseWhitespace();
  if (!else_interval.has_value() ||
      else_interval->end != else_interval->begin + 1) {
    fprintf(stderr,
            "REX_TOKEN_INVARIANT[else-interval]: node=%p/%s did not identify "
            "exactly one else token; interval=%s\n",
            static_cast<void *>(if_statement_mappingInfo->node),
            if_statement_mappingInfo->node->class_name().c_str(),
            describeDraftInterval(else_interval).c_str());
    ROSE_ABORT();
  }
}

TokenMappingSynthesizedAttribute
TokenMappingTraversal::evaluateSynthesizedAttribute(
    SgNode *n, InheritedAttribute inheritedAttribute,
    SynthesizedAttributesList childAttributes) {
  if (inheritedAttribute.subtreeRole ==
      InheritedAttribute::SubtreeRole::Semantic)
    return TokenMappingSynthesizedAttribute(n);
  // This traversal step computes the leading an trailing edges of each node in
  // the child list for the current SgNode. It also builds token subsequence
  // mappings for any interval of child IR nodes of the AST for which they were
  // not computed in the evaluateInheritedAttribute() function (on the way down
  // in the AST traversal).

  const std::optional<TokenStreamHalfOpenInterval> inherited_token_interval =
      inheritedAttribute.token_interval;
  int original_start_of_token_subsequence =
      inherited_token_interval.has_value() ? inherited_token_interval->begin
                                           : -1;
  int original_end_of_token_subsequence =
      inherited_token_interval.has_value() ? inherited_token_interval->end - 1
                                           : -1;

  if (childAttributes.size() > 0) {
    // A block can belong to another file while its children belong to this one.
    if (isSgStatement(n) != NULL) {
      vector<TokenStreamSequenceToNodeMapping *> tokenToNodeVector;

      vector<size_t> childrenWithoutTokenMappings;

      // DQ (12/22/2014): Record the last child that contains token mapping
      // information (e.g. template instantations and normalized template
      // declarations will be excluded).
      int firstChildWithTokenMapping = -1;
      int lastChildWithTokenMapping = -1;

      // DQ (1/24/2015): Handle the case of the null for init and null test
      // statements in "for ( ; ; )".
      SgForStatement *forStatement = isSgForStatement(n);
      if (forStatement != NULL) {
        const size_t forInitTraversalIndex =
            forStatement->get_childIndex(forStatement->get_for_init_stmt());
        const size_t testTraversalIndex =
            forStatement->get_childIndex(forStatement->get_test());
        const size_t incrementTraversalIndex =
            forStatement->get_childIndex(forStatement->get_increment());
        const auto for_mapping = tokenStreamSequenceMap.find(n);
        if (for_mapping != tokenStreamSequenceMap.end()) {
          TokenStreamSequenceToNodeMapping *for_mappingInfo =
              for_mapping->second;
          ROSE_ASSERT(for_mappingInfo != NULL);
          const auto test_mapping = tokenStreamSequenceMap.find(
              childAttributes[testTraversalIndex].node);
          const auto init_mapping = tokenStreamSequenceMap.find(
              childAttributes[forInitTraversalIndex].node);
          if (test_mapping != tokenStreamSequenceMap.end() &&
              init_mapping != tokenStreamSequenceMap.end()) {
            TokenStreamSequenceToNodeMapping *for_init_mappingInfo =
                init_mapping->second;
            TokenStreamSequenceToNodeMapping *for_test_mappingInfo =
                test_mapping->second;
            ROSE_ASSERT(for_init_mappingInfo != nullptr &&
                        for_test_mappingInfo != nullptr);

            SgForInitStatement *previous_for_init_statement =
                isSgForInitStatement(for_init_mappingInfo->node);
            SgNullStatement *null_statement =
                isSgNullStatement(for_test_mappingInfo->node);
            if (previous_for_init_statement != NULL && null_statement != NULL) {
              // This is at least after the for_init_statement, and is a better
              // position to start the direct search within the token stream.
              // This will also avoid the test statement being confused as being
              // shared with the token stream subsequence of the
              // for_init_statement.
              int index = for_init_mappingInfo
                              ->constructionState(
                                  TokenStreamMappingConstructionAccess::key())
                              .tokenSubsequence()
                              .end;
              int upperBound =
                  for_mappingInfo
                      ->constructionState(
                          TokenStreamMappingConstructionAccess::key())
                      .tokenSubsequence()
                      .end -
                  1;

              ROSE_ASSERT(index >= 0);
              ROSE_ASSERT(index <= upperBound);
              ROSE_ASSERT(static_cast<size_t>(upperBound) < tokenStream.size());

              while (index < upperBound &&
                     tokenStream[index]->p_tok_elem->token_lexeme != ";") {
                index++;
              }
              ROSE_ASSERT(tokenStream[index]->p_tok_elem->token_lexeme == ";");

              for_test_mappingInfo
                  ->constructionState(
                      TokenStreamMappingConstructionAccess::key())
                  .replaceTokenSubsequence(
                      TokenStreamHalfOpenInterval(index, index + 1));
            }
          }

          // DQ (1/27/2015): Adjust the the for loop increment expression
          // mapping relative to the end of the for loop test to handle the case
          // where this may be a prefix operator++() and compiler generated and
          // thus not mapped accurately to the token start.
          if ((tokenStreamSequenceMap.find(
                   childAttributes[testTraversalIndex].node) !=
               tokenStreamSequenceMap.end()) &&
              (tokenStreamSequenceMap.find(
                   childAttributes[incrementTraversalIndex].node) !=
               tokenStreamSequenceMap.end())) {
            TokenStreamSequenceToNodeMapping *for_test_mappingInfo =
                tokenStreamSequenceMap[childAttributes[testTraversalIndex]
                                           .node];
            TokenStreamSequenceToNodeMapping *for_increment_mappingInfo =
                tokenStreamSequenceMap[childAttributes[incrementTraversalIndex]
                                           .node];

            ROSE_ASSERT(for_test_mappingInfo != NULL);
            ROSE_ASSERT(for_increment_mappingInfo != NULL);

            // Exact token intervals are interned.  Components produced by one
            // macro invocation therefore deliberately share a mapping object;
            // changing that object to disambiguate the increment would also
            // change the test (and every other component of the expansion).
            // There is no distinct written increment interval to repair in
            // that case.  Preserve the authoritative shared invocation
            // interval and only refine an independently owned increment.
            if (for_test_mappingInfo != for_increment_mappingInfo) {
              int test_end =
                  for_test_mappingInfo
                      ->constructionState(
                          TokenStreamMappingConstructionAccess::key())
                      .tokenSubsequence()
                      .end -
                  1;
              int increment_start =
                  for_increment_mappingInfo
                      ->constructionState(
                          TokenStreamMappingConstructionAccess::key())
                      .tokenSubsequence()
                      .begin;

              int better_start_of_token_subsequence = test_end + 1;

              while (
                  better_start_of_token_subsequence < increment_start &&
                  (tokenStream[better_start_of_token_subsequence]
                           ->p_tok_elem->token_id == C_CXX_PREPROCESSING_INFO ||
                   tokenStream[better_start_of_token_subsequence]
                           ->p_tok_elem->token_id == C_CXX_WHITESPACE ||
                   tokenStream[better_start_of_token_subsequence]
                           ->p_tok_elem->token_id == C_CXX_COMMENTS)) {
                better_start_of_token_subsequence++;
              }
              const int increment_end =
                  for_increment_mappingInfo
                      ->constructionState(
                          TokenStreamMappingConstructionAccess::key())
                      .tokenSubsequence()
                      .end;
              for_increment_mappingInfo
                  ->constructionState(
                      TokenStreamMappingConstructionAccess::key())
                  .replaceTokenSubsequence(TokenStreamHalfOpenInterval(
                      better_start_of_token_subsequence, increment_end));
            }
          }
        }
      }

      for (size_t i = 0; i < childAttributes.size(); i++) {
        SgLocatedNode *childLocatedNode =
            isSgLocatedNode(childAttributes[i].node);

        if (childLocatedNode != nullptr &&
            childLocatedNode->get_file_info() == nullptr) {
          const auto child_names = n->get_traversalSuccessorNamesContainer();
          ROSE_ASSERT(i < child_names.size());
          fprintf(stderr,
                  "REX_TOKEN_INVARIANT[missing-child-provenance]: "
                  "parent=%p/%s child=%p/%s edge=%s is a located node "
                  "without primary file information\n",
                  static_cast<void *>(n),
                  n != nullptr ? n->class_name().c_str() : "<null>",
                  static_cast<void *>(childAttributes[i].node),
                  childAttributes[i].node != nullptr
                      ? childAttributes[i].node->class_name().c_str()
                      : "<null>",
                  child_names[i].c_str());
          ROSE_ABORT();
        }

        const bool from_current_file =
            childLocatedNode != nullptr &&
            belongsToTokenSourceFile(childLocatedNode->get_file_info(),
                                     currentSourceFile());

        const auto child_mapping =
            tokenStreamSequenceMap.find(childAttributes[i].node);
        if (from_current_file &&
            child_mapping != tokenStreamSequenceMap.end()) {
          TokenStreamSequenceToNodeMapping *mappingInfo = child_mapping->second;

          ROSE_ASSERT(mappingInfo != NULL);

          // This is used to know where the compute the leading white space
          // token information (in cases where the leading children do not have
          // a token mapping).
          if (firstChildWithTokenMapping < 0) {
            firstChildWithTokenMapping = (int)i;
          }

          // This is used to know where the compute the trailing white space
          // token information (in cases where the trailing children do not have
          // a token mapping).
          lastChildWithTokenMapping = (int)i;

          TokenStreamSequenceToNodeMapping *mappingInfo_to_add = mappingInfo;

          vector<TokenStreamSequenceToNodeMapping *> tokenToNodeEntriesToRemove;
          bool removeCurrentNestedMapping = false;

          // DQ (10/14/2013): We need to detect cases where a sibling token
          // subsequence range is nested in another token subsequence range of
          // another sibling. These will likely be adjacent IR nodes (see
          // test2013_87.c).
          for (size_t j = 0; j < tokenToNodeVector.size(); j++) {
            // Check to see if this is the superset of any existing subsequence
            // range.
            TokenStreamSequenceToNodeMapping *previous_mappingInfo =
                tokenToNodeVector[j];
            ROSE_ASSERT(previous_mappingInfo != NULL);

            int current_token_sequence_start =
                mappingInfo
                    ->constructionState(
                        TokenStreamMappingConstructionAccess::key())
                    .tokenSubsequence()
                    .begin;
            int current_token_sequence_end =
                mappingInfo
                    ->constructionState(
                        TokenStreamMappingConstructionAccess::key())
                    .tokenSubsequence()
                    .end -
                1;
            int previous_token_sequence_start =
                previous_mappingInfo
                    ->constructionState(
                        TokenStreamMappingConstructionAccess::key())
                    .tokenSubsequence()
                    .begin;
            int previous_token_sequence_end =
                previous_mappingInfo
                    ->constructionState(
                        TokenStreamMappingConstructionAccess::key())
                    .tokenSubsequence()
                    .end -
                1;

            ROSE_ASSERT(current_token_sequence_start >= 0 &&
                        current_token_sequence_end >= 0);
            ROSE_ASSERT(previous_token_sequence_start >= 0 &&
                        previous_token_sequence_end >= 0);
            // We want to only detect proper nesting (not equality).
            // Equality is represented via sharing, while nested subsets will
            // cause the nested subsequence data structure to be removed.
            if (((current_token_sequence_start <
                  previous_token_sequence_start) &&
                 (current_token_sequence_end > previous_token_sequence_end)) ||
                ((current_token_sequence_start ==
                  previous_token_sequence_start) &&
                 (current_token_sequence_end > previous_token_sequence_end)) ||
                ((current_token_sequence_start <
                  previous_token_sequence_start) &&
                 (current_token_sequence_end == previous_token_sequence_end))) {
              // Remove the inner class from the tokenToNodeVector, and the IR
              // node to token subsequence map
              if (find(tokenToNodeEntriesToRemove.begin(),
                       tokenToNodeEntriesToRemove.end(),
                       previous_mappingInfo) ==
                  tokenToNodeEntriesToRemove.end()) {
                tokenToNodeEntriesToRemove.push_back(previous_mappingInfo);
              }

              // A properly nested node does not share the outer node's exact
              // token interval.  Removing its redundant direct mapping must
              // not add it to nodeVector, which is reserved for nodes that map
              // bidirectionally to this one exact interval.

              mappingInfo_to_add = mappingInfo;
            } else {
              if (((current_token_sequence_start >
                    previous_token_sequence_start) &&
                   (current_token_sequence_end <
                    previous_token_sequence_end)) ||
                  ((current_token_sequence_start ==
                    previous_token_sequence_start) &&
                   (current_token_sequence_end <
                    previous_token_sequence_end)) ||
                  ((current_token_sequence_start >
                    previous_token_sequence_start) &&
                   (current_token_sequence_end ==
                    previous_token_sequence_end))) {
                // A child can be nested in more than one previously retained
                // sibling interval.  Record that result while comparing the
                // complete sibling set, then retire the direct association
                // exactly once below.  Detaching inside this loop both
                // double-retired the mapping and reinserted its stale pointer
                // into tokenToNodeVector after the loop.
                removeCurrentNestedMapping = true;
              } else {
                // This is not any kind of nested subsequence.
              }
            }
          }

          ROSE_ASSERT(mappingInfo_to_add != NULL);
          // Remove the entries that we have detected to be nested inside of
          // other sibling IR node subsequences.
          // tokenToNodeVector.erase(tokenToNodeEntriesToRemove.begin(),tokenToNodeEntriesToRemove.end());
          for (size_t index = 0; index < tokenToNodeEntriesToRemove.size();
               index++) {
            vector<TokenStreamSequenceToNodeMapping *>::iterator k1, k2;
            k1 = find(tokenToNodeVector.begin(), tokenToNodeVector.end(),
                      tokenToNodeEntriesToRemove[index]);
            if (k1 != tokenToNodeVector.end()) {
              tokenToNodeVector.erase(k1);
            }

            ROSE_ASSERT(tokenToNodeEntriesToRemove[index]->node != NULL);
            // The current parent node `n` does not always own a direct token
            // sequence entry (for example SgTemplateInstantiationDefn scopes
            // can be represented entirely by their child declarations). The
            // cleanup here is removing the nested child mappings recorded in
            // tokenToNodeEntriesToRemove, so only those entries are required
            // to exist in the token-sequence containers.
            detachExactTokenMappingAssociation(
                tokenStreamSequenceMap, tokenToNodeEntriesToRemove[index]->node,
                tokenToNodeEntriesToRemove[index]);

            k2 = find(tokenStreamSequenceVector.begin(),
                      tokenStreamSequenceVector.end(),
                      tokenToNodeEntriesToRemove[index]);
            if (k2 != tokenStreamSequenceVector.end()) {
              tokenStreamSequenceVector.erase(k2);
            }
          }

          if (removeCurrentNestedMapping) {
            detachExactTokenMappingAssociation(tokenStreamSequenceMap,
                                               mappingInfo->node, mappingInfo);
            vector<TokenStreamSequenceToNodeMapping *>::iterator current =
                find(tokenStreamSequenceVector.begin(),
                     tokenStreamSequenceVector.end(), mappingInfo);
            if (current != tokenStreamSequenceVector.end()) {
              tokenStreamSequenceVector.erase(current);
            }
          }
          // Save the mappingInfo for all children as this IR node in the AST.
          // tokenToNodeVector.push_back(mappingInfo);
          if (!removeCurrentNestedMapping) {
            tokenToNodeVector.push_back(mappingInfo_to_add);
          }
        } else {
          // We need to build a TokenStreamSequenceToNodeMapping for this case
          // (but we currently do this afterward)..

          // We need to ignore NULL pointers.
          // nodesWithoutTokenMappings.push_back(childAttributes[i].node);
          // childrenWithoutTokenMappings.push_back(i);
          if (childAttributes[i].node != NULL) {
            // Also make sure this IR node is associated with the current file.
            SgStatement *statement = isSgStatement(childAttributes[i].node);
            SgDeclarationStatement *declaration =
                isSgDeclarationStatement(statement);
            const bool ownsDirectTokenMapping =
                statement != nullptr && (declaration == nullptr ||
                                         declarationRequiresTokenMapping(
                                             declaration, currentSourceFile()));
            if (ownsDirectTokenMapping) {
              Sg_File_Info *start_pos = statement->get_startOfConstruct();
              ROSE_ASSERT(currentSourceFile() != NULL);

              // Note that this is implemented internally
              // to use the physical file information (not
              // logical file info).
              bool process_node = (start_pos->isSameFile(currentSourceFile()));
              process_node = (process_node == true) &&
                             (inheritedAttribute.processChildNodes == true);
              if (process_node == true) {
                childrenWithoutTokenMappings.push_back(i);
              }
            }
          }
        }
      }

      // DQ (1/3/2014): We need to handle macro expansions that are
      // characterized by having the same start and end source positions (but
      // could also be a single token statement, e.g. ";"). Unfortunately, it
      // can also be a token for a variable reference expression and thus we
      // have to handle this case explicitly.
      for (size_t j = 0; j < tokenToNodeVector.size(); j++) {
        // This can be true for the case of a ";" (SgNullExpression in a
        // SgExprStatement) as well as for where macros are used.
        if (tokenToNodeVector[j]
                ->constructionState(TokenStreamMappingConstructionAccess::key())
                .tokenSubsequence()
                .begin ==
            tokenToNodeVector[j]
                    ->constructionState(
                        TokenStreamMappingConstructionAccess::key())
                    .tokenSubsequence()
                    .end -
                1) {

          // DQ (1/22/2015): Added realization that there are a few more single
          // character statements than I realized previously (these can look
          // like macro expansions). Note: there are a few statements that are a
          // single character and can be tripped up by this test
          // (startOfConstruct() == endOfConstruct()). Examples are: ";" and
          // single character value expressions that are interpreted as
          // SgExprStatement IR nodes (e.g. in "if(0)").

          // SgVarRefExp* varRefExp = isSgVarRefExp(tokenToNodeVector[j]->node);
          // if (varRefExp == NULL)
          SgLocatedNode *tmp_locatedNode =
              isSgLocatedNode(tokenToNodeVector[j]->node);
          SgStatement *candidateStatement =
              isSgStatement(tokenToNodeVector[j]->node);
          const std::map<SgStatement *, MacroExpansion *> &macroExpansionMap =
              currentSourceFile()->get_macroExpansionMap();
          bool processAsMacroExpansion =
              candidateStatement != nullptr &&
              macroExpansionMap.find(candidateStatement) !=
                  macroExpansionMap.end();

          // Only statements recorded in the exact macro-expansion map can use
          // the macro path. Single-token statements such as ";" are ordinary
          // source nodes and retain their own token intervals.
          if (processAsMacroExpansion == true) {
            if (SgStatement *stmt = isSgStatement(tmp_locatedNode)) {
              if (isSgNullStatement(stmt) != NULL) {
                processAsMacroExpansion = false;
              } else if (SgExprStatement *exprStmt = isSgExprStatement(stmt)) {
                if (isSgNullExpression(exprStmt->get_expression()) != NULL) {
                  processAsMacroExpansion = false;
                }
              }
            }
          }
          // DQ (1/22/2015): We need to account for statements that have
          // surrounding syntax, and can be a single character statement.
          // SgIfStmt* ifStatement = isSgIfStmt(n);
          // if (ifStatement != NULL && tokenToNodeVector[j]->node ==
          // ifStatement->get_conditional()) DQ (12/26/2014): Adding support for
          // fixing the leading white space of conditionals statements (in C++
          // most conditional expressions are actually statements).
          SgWhileStmt *whileStatement = isSgWhileStmt(n);
          SgDoWhileStmt *doWhileStatement = isSgDoWhileStmt(n);
          SgSwitchStatement *switchStatement = isSgSwitchStatement(n);
          SgIfStmt *ifStatement = isSgIfStmt(n);
          SgForStatement *forStatement = isSgForStatement(n);
          if (whileStatement != NULL || switchStatement != NULL ||
              ifStatement != NULL || doWhileStatement != NULL ||
              forStatement != NULL) {
            ROSE_ASSERT(tmp_locatedNode != NULL);
            SgStatement *conditionStatement = isSgStatement(tmp_locatedNode);
            if (conditionStatement != NULL) {
              if ((whileStatement != NULL &&
                   conditionStatement == whileStatement->get_condition()) ||
                  (doWhileStatement != NULL &&
                   conditionStatement == doWhileStatement->get_condition()) ||
                  (switchStatement != NULL &&
                   conditionStatement ==
                       switchStatement->get_item_selector()) ||
                  (forStatement != NULL &&
                   conditionStatement == forStatement->get_test()) ||
                  (ifStatement != NULL &&
                   conditionStatement == ifStatement->get_conditional())) {
                // Test inputmove*_test2015_74.C demonstrates this problem where
                // the test in "if(0)" is a 1 token statement. In this case we
                // have to skip over the ")" as well or disqualify an attempt at
                // a better evaluation.
                processAsMacroExpansion = false;
              }
            }
          }

          // Clang macro source locations already identify the exact written
          // macro-call token.  Never widen that interval to the next sibling:
          // doing so steals semicolons, directives, and comments that have
          // independent source owners.
          if (processAsMacroExpansion) {
            continue;
          }
        }
      }

      if (tokenToNodeVector.size() > 0) {
        // We have to process the elements of the tokenToNodeVector.

        // There should be at least one child with token information.
        ROSE_ASSERT(lastChildWithTokenMapping >= 0);

        int current_node_token_subsequence_start = -1;
        int current_node_token_subsequence_end = -1;

        if (tokenStreamSequenceMap.find(n) != tokenStreamSequenceMap.end()) {
          TokenStreamSequenceToNodeMapping *current_node_mappingInfo =
              tokenStreamSequenceMap[n];
          current_node_token_subsequence_start =
              current_node_mappingInfo
                  ->constructionState(
                      TokenStreamMappingConstructionAccess::key())
                  .tokenSubsequence()
                  .begin;
          current_node_token_subsequence_end =
              current_node_mappingInfo
                  ->constructionState(
                      TokenStreamMappingConstructionAccess::key())
                  .tokenSubsequence()
                  .end -
              1;
        }

        bool previous_fixupDarkTokenSubsequencesForTrailingWhitespace = false;

        for (size_t i = 0; i < tokenToNodeVector.size(); i++) {
          TokenStreamSequenceToNodeMapping *mappingInfo = tokenToNodeVector[i];

          // DQ (4/19/2021): Need to check on the files that are being used, so
          // that they are not mixed.
          string mappingInfoFilename;
          if (mappingInfo->node != NULL) {
            mappingInfoFilename =
                mappingInfo->node->get_file_info()->get_filename();
          }
          string nodeFilename = n->get_file_info()->get_filename();
          int token_subsequence_start =
              mappingInfo
                  ->constructionState(
                      TokenStreamMappingConstructionAccess::key())
                  .tokenSubsequence()
                  .begin;
          int token_subsequence_end =
              mappingInfo
                  ->constructionState(
                      TokenStreamMappingConstructionAccess::key())
                  .tokenSubsequence()
                  .end -
              1;

          ROSE_ASSERT(token_subsequence_start >= 0);
          ROSE_ASSERT(token_subsequence_end >= 0);

          // Handle the left edge of the AST subtree: modify the edged (leading
          // whitespace).
          ROSE_ASSERT(firstChildWithTokenMapping <= lastChildWithTokenMapping);

          // DQ (3/22/2021): Added assertion.
          ROSE_ASSERT(mappingInfo->node != NULL);

          // DQ (1/2/2015): New more general mechanism, we now want to
          // uniformally make sure that the leading and trailing whitespace does
          // not include syntax.

          // Trim only mappings in the current source file.
          if (belongsToTokenSourceFile(mappingInfo->node->get_file_info(),
                                       currentSourceFile())) {
            trimLeadingWhiteSpaceFromLeft(mappingInfo,
                                          original_start_of_token_subsequence);
            trimTrailingWhiteSpaceFromRight(mappingInfo,
                                            original_end_of_token_subsequence);
          } else {
            // DQ (4/26/2021): If not calling the trimLeadingWhiteSpaceFromLeft
            // and trimTrailingWhiteSpaceFromRight functions, then set the
            // values accordingly.
            auto retainLastToken =
                [](const std::optional<TokenStreamHalfOpenInterval> &interval) {
                  return interval.has_value()
                             ? std::optional<TokenStreamHalfOpenInterval>(
                                   TokenStreamHalfOpenInterval(
                                       interval->end - 1, interval->end))
                             : std::nullopt;
                };
            auto &draft = mappingInfo->constructionState(
                TokenStreamMappingConstructionAccess::key());
            const auto leading = retainLastToken(draft.leadingWhitespace());
            const auto trailing = retainLastToken(draft.trailingWhitespace());
            const auto else_interval = retainLastToken(draft.elseWhitespace());
            draft.replaceLeadingWhitespace(leading);
            draft.replaceTrailingWhitespace(trailing);
            draft.replaceElseWhitespace(else_interval);
          }
          const auto &validatedDraft = mappingInfo->constructionState(
              TokenStreamMappingConstructionAccess::key());
          ROSE_ASSERT(!validatedDraft.leadingWhitespace().has_value() ||
                      validatedDraft.leadingWhitespace()->begin <
                          validatedDraft.leadingWhitespace()->end);
          ROSE_ASSERT(!validatedDraft.trailingWhitespace().has_value() ||
                      validatedDraft.trailingWhitespace()->begin <
                          validatedDraft.trailingWhitespace()->end);
          ROSE_ASSERT(!validatedDraft.elseWhitespace().has_value() ||
                      validatedDraft.elseWhitespace()->begin <
                          validatedDraft.elseWhitespace()->end);

          // DQ (1/11/2014): Record the dark tokens so that we can add them the
          // the white space range. Dark tokens are defined as those tokens
          // between the previous mappings trailing whitespace end and the token
          // sequence start for the current IR node.

          ROSE_ASSERT(mappingInfo->node != NULL);

          bool fixupDarkTokenSubsequencesForLeadingWhitespace = true;
          bool fixupDarkTokenSubsequencesForTrailingWhitespace = true;

          // This is required to support the dark token sequence support for
          // trailing white space.
          SgFunctionDeclaration *functionDeclaration =
              isSgFunctionDeclaration(n);
          if (functionDeclaration != NULL &&
              mappingInfo->node == functionDeclaration->get_definition()) {
            fixupDarkTokenSubsequencesForLeadingWhitespace = false;
          }
          SgSwitchStatement *switchStatement = isSgSwitchStatement(n);
          SgWhileStmt *whileStatement = isSgWhileStmt(n);
          SgForStatement *forStatement = isSgForStatement(n);
          SgIfStmt *ifStatement = isSgIfStmt(n);
          SgDoWhileStmt *doWhileStatement = isSgDoWhileStmt(n);

          // DQ (6/3/2021): There can be acceptable tokens between the
          // whitespace before "class A" and the whitespace afterward and before
          // the opening "{".
          SgClassDeclaration *classDeclaration = isSgClassDeclaration(n);
          if (classDeclaration != NULL) {
            fixupDarkTokenSubsequencesForLeadingWhitespace = false;
          }

          if (forStatement != NULL &&
              mappingInfo->node == forStatement->get_loop_body()) {
            // fixupDarkTokenSubsequences = true;
            fixupDarkTokenSubsequencesForTrailingWhitespace = false;
          }

          // DQ (1/20/2015): Adding support for the trailing whitespace of the
          // "true" branch of an "if" statement.
          if (ifStatement != NULL &&
              (mappingInfo->node == ifStatement->get_true_body())) {
            fixupDarkTokenSubsequencesForTrailingWhitespace = false;
          }

          // This is required to support the dark token sequence support for
          // leading white space. The problem here is that these statements have
          // syntax that would have to be identified so that we would not
          // inlcude it in the leading whitespace.  For now it would be simpler
          // to avoid processing these cases, however it could be a problem if a
          // dark token subsequence were embedded just right. if (ifStatement !=
          // NULL && mappingInfo->node == ifStatement->get_true_body() )
          if (ifStatement != NULL &&
              (mappingInfo->node == ifStatement->get_true_body() ||
               mappingInfo->node == ifStatement->get_false_body())) {
            fixupDarkTokenSubsequencesForLeadingWhitespace = false;
          }

          // if (forStatement != NULL && mappingInfo->node ==
          // forStatement->get_for_init_stmt() )
          if ((forStatement != NULL &&
               mappingInfo->node == forStatement->get_for_init_stmt()) ||
              (whileStatement != NULL &&
               mappingInfo->node == whileStatement->get_condition()) ||
              (switchStatement != NULL &&
               mappingInfo->node == switchStatement->get_item_selector()) ||
              (doWhileStatement != NULL &&
               mappingInfo->node == doWhileStatement->get_condition())) {
            fixupDarkTokenSubsequencesForLeadingWhitespace = false;
          }
          // DQ (1/25/2015): Test disabling this for symetry with SgIfStmt.
          // DQ (1/21/2015): Turn this off to account for syntax between the
          // condition and the body of a while statement. This might be required
          // for SgIfStmt and other compound statements as well. if (
          // (whileStatement != NULL && mappingInfo->node ==
          // whileStatement->get_condition()) )
          if ((whileStatement != NULL &&
               mappingInfo->node == whileStatement->get_condition()) ||
              (switchStatement != NULL &&
               mappingInfo->node == switchStatement->get_item_selector()) ||
              (doWhileStatement != NULL &&
               mappingInfo->node == doWhileStatement->get_condition())) {
            fixupDarkTokenSubsequencesForTrailingWhitespace = false;
          }
          // DQ (1/22/2015): Turn off processing of the dark tokens in a
          // SgCaseOptionStmt until we can eliminate the compiler generated
          // SgBasicBlock used as the body.  Note that we are currenty forcing
          // the generation of the body from the AST since there is no mapping
          // from the token stream to the compiler-generated basic block body
          // (if it is compiler generated).
          if (isSgCaseOptionStmt(mappingInfo->node) != NULL) {
            fixupDarkTokenSubsequencesForTrailingWhitespace = false;

            // DQ (1/22/2015): test2015_93.C demonstrates that we need to also
            // turn off the processing of the leading white space as well.
            fixupDarkTokenSubsequencesForLeadingWhitespace = false;
          }
          // These statements have syntax that separate the main construct from
          // the construct's associated body (namely the ")" closing
          // parenthesis).
          if ((switchStatement != NULL &&
               mappingInfo->node == switchStatement->get_body()) ||
              (whileStatement != NULL &&
               mappingInfo->node == whileStatement->get_body()) ||
              (forStatement != NULL &&
               mappingInfo->node == forStatement->get_loop_body())) {
            fixupDarkTokenSubsequencesForLeadingWhitespace = false;
          }

          if (fixupDarkTokenSubsequencesForLeadingWhitespace == true) {
            // DQ (1/17/2015): Fixup the leading white space subsequence to
            // include the dark token subsequences.
            if (i == 0) {
              // DQ (1/17/2015): Adding support for
              // tests/nonsmoke/functional/roseTests/astInterface/*_test2015_47.C
              TokenStreamSequenceToNodeMapping *previous_mappingInfo = NULL;
              if (tokenStreamSequenceMap.find(n) !=
                  tokenStreamSequenceMap.end()) {
                previous_mappingInfo = tokenStreamSequenceMap[n];
              }

              if (previous_mappingInfo != NULL) {
                // DQ (4/10/2021): If this is the first of the token sequence,
                // then this should be set to zero. int
                // previous_mappingInfo_leading_whitespace_end =
                // previous_mappingInfo->constructionState(TokenStreamMappingConstructionAccess::key()).tokenSubsequence().begin
                // + 1;
                int previous_mappingInfo_leading_whitespace_end =
                    previous_mappingInfo
                        ->constructionState(
                            TokenStreamMappingConstructionAccess::key())
                        .tokenSubsequence()
                        .begin +
                    1;
                if (previous_mappingInfo
                        ->constructionState(
                            TokenStreamMappingConstructionAccess::key())
                        .tokenSubsequence()
                        .begin == 0) {
                  previous_mappingInfo_leading_whitespace_end = 0;
                }
                ROSE_ASSERT(previous_mappingInfo
                                ->constructionState(
                                    TokenStreamMappingConstructionAccess::key())
                                .tokenSubsequence()
                                .begin >= 0);

                auto &draft = mappingInfo->constructionState(
                    TokenStreamMappingConstructionAccess::key());
                if (draft.leadingWhitespace().has_value() &&
                    draft.leadingWhitespace()->begin >
                        previous_mappingInfo_leading_whitespace_end) {
                  replaceDraftLeadingBegin(
                      draft, previous_mappingInfo_leading_whitespace_end);
                }
              }
            } else {
              TokenStreamSequenceToNodeMapping *previous_mappingInfo =
                  tokenToNodeVector[i - 1];
              ROSE_ASSERT(previous_mappingInfo != NULL);

              // DQ (1/28/2015): Added assertion.
              int temp_i = i;
              if (previous_mappingInfo->node == mappingInfo->node) {
                // This is likely a shared token sequence and we need to go back
                // one more.
                while (temp_i >= 1 &&
                       previous_mappingInfo->node == mappingInfo->node) {
                  previous_mappingInfo = tokenToNodeVector[temp_i - 1];
                  temp_i--;

                  ROSE_ASSERT(previous_mappingInfo != NULL);
                }
              }

              if (previous_mappingInfo != NULL) {
                int previous_mappingInfo_leading_whitespace_end =
                    previous_mappingInfo
                        ->constructionState(
                            TokenStreamMappingConstructionAccess::key())
                        .tokenSubsequence()
                        .end -
                    1 + 1;
                ROSE_ASSERT(previous_mappingInfo
                                ->constructionState(
                                    TokenStreamMappingConstructionAccess::key())
                                .tokenSubsequence()
                                .begin >= 0);

                auto &draft = mappingInfo->constructionState(
                    TokenStreamMappingConstructionAccess::key());
                if (draft.leadingWhitespace().has_value() &&
                    draft.leadingWhitespace()->begin >
                        previous_mappingInfo_leading_whitespace_end) {
                  replaceDraftLeadingBegin(
                      draft, previous_mappingInfo_leading_whitespace_end);
                }
              }
            }
          }

          if (fixupDarkTokenSubsequencesForTrailingWhitespace == true) {
            // Fixup the trailing white space subsequence to include the dark
            // token subsequences.
            if (i == 0) {
              // Dark tokens are defined for the leading token sequence, but we
              // want to work out and debug the case of the trailing dark token
              // sequence first.
              if (tokenToNodeVector.size() == 1) {
                TokenStreamSequenceToNodeMapping *previous_mappingInfo = NULL;
                if (tokenStreamSequenceMap.find(n) !=
                    tokenStreamSequenceMap.end()) {
                  previous_mappingInfo = tokenStreamSequenceMap[n];
                }

                if (previous_mappingInfo != NULL) {
                  // In this case their is only a single child so the first
                  // child has a trailing token sequence to fixup.
                  const int previousCoreEnd =
                      previous_mappingInfo
                          ->constructionState(
                              TokenStreamMappingConstructionAccess::key())
                          .tokenSubsequence()
                          .end;
                  auto &currentDraft = mappingInfo->constructionState(
                      TokenStreamMappingConstructionAccess::key());
                  const auto &currentTrailing =
                      currentDraft.trailingWhitespace();
                  ROSE_ASSERT(previousCoreEnd > 0);
                  if (currentTrailing.has_value() &&
                      currentTrailing->end < previousCoreEnd - 1) {
                    currentDraft.replaceTrailingWhitespace(
                        TokenStreamHalfOpenInterval(currentTrailing->begin,
                                                    previousCoreEnd - 1));
                  }
                }
              }
            } else {
              // This branch can be refactored with the previous branch.
              // This is the first case to fixup.
              TokenStreamSequenceToNodeMapping *previous_mappingInfo =
                  tokenToNodeVector[i - 1];
              ROSE_ASSERT(previous_mappingInfo != NULL);

              // DQ (1/28/2015): Added assertion.
              int temp_i = i;
              if (previous_mappingInfo->node == mappingInfo->node) {
                // This is likely a shared token sequence and we need to go back
                // one more.
                while (temp_i >= 1 &&
                       previous_mappingInfo->node == mappingInfo->node) {
                  previous_mappingInfo = tokenToNodeVector[temp_i - 1];
                  temp_i--;

                  ROSE_ASSERT(previous_mappingInfo != NULL);
                }
              }

              ROSE_ASSERT(previous_mappingInfo != NULL);
              auto &previousDraft = previous_mappingInfo->constructionState(
                  TokenStreamMappingConstructionAccess::key());
              auto &currentDraft = mappingInfo->constructionState(
                  TokenStreamMappingConstructionAccess::key());
              const int currentCoreBegin =
                  currentDraft.tokenSubsequence().begin;
              const int previousCoveredEnd =
                  previousDraft.trailingWhitespace().has_value()
                      ? previousDraft.trailingWhitespace()->end
                      : previousDraft.tokenSubsequence().end;
              const bool crossesElse =
                  ifStatement != nullptr &&
                  previous_mappingInfo->node == ifStatement->get_true_body() &&
                  mappingInfo->node == ifStatement->get_false_body();

              // Dark-token whitespace exists only in a real gap between two
              // disjoint siblings.  An absent trailing interval means the
              // previous core end is the first unowned token; it is not a
              // numeric sentinel.  The `else` region is owned separately by
              // the enclosing if statement and must not be transferred here.
              if (!crossesElse && currentCoreBegin > previousCoveredEnd &&
                  previous_fixupDarkTokenSubsequencesForTrailingWhitespace) {
                const int trailingBegin =
                    previousDraft.trailingWhitespace().has_value()
                        ? previousDraft.trailingWhitespace()->begin
                        : previousDraft.tokenSubsequence().end;
                previousDraft.replaceTrailingWhitespace(
                    TokenStreamHalfOpenInterval(trailingBegin,
                                                currentCoreBegin));
                currentDraft.replaceLeadingWhitespace(
                    previousDraft.trailingWhitespace());
              }

              // This is the same case as i == 0.
              // Handle the trailing space.
              if (i == tokenToNodeVector.size() - 1) {
                TokenStreamSequenceToNodeMapping *previous_mappingInfo = NULL;
                if (tokenStreamSequenceMap.find(n) !=
                    tokenStreamSequenceMap.end()) {
                  previous_mappingInfo = tokenStreamSequenceMap[n];
                }

                if (previous_mappingInfo != NULL) {
                  const int previousCoreEnd =
                      previous_mappingInfo
                          ->constructionState(
                              TokenStreamMappingConstructionAccess::key())
                          .tokenSubsequence()
                          .end;
                  auto &currentDraft = mappingInfo->constructionState(
                      TokenStreamMappingConstructionAccess::key());
                  const auto &currentTrailing =
                      currentDraft.trailingWhitespace();
                  ROSE_ASSERT(previousCoreEnd > 0);
                  if (currentTrailing.has_value() &&
                      currentTrailing->end < previousCoreEnd - 1) {
                    const int newTrailingEnd = isSgGlobal(n) != nullptr
                                                   ? previousCoreEnd
                                                   : previousCoreEnd - 1;
                    currentDraft.replaceTrailingWhitespace(
                        TokenStreamHalfOpenInterval(currentTrailing->begin,
                                                    newTrailingEnd));
                  }
                }
              }
            }
            // end of body for if fixupDarkTokenSubsequences == false
          }

          // DQ (1/21/2015): Added to support to control resetting of
          // previous_mapping trailing token sequence if we was explicitly
          // specified to not be set in the previous iteration. bool
          // previous_fixupDarkTokenSubsequencesForLeadingWhitespace  = false;
          previous_fixupDarkTokenSubsequencesForTrailingWhitespace =
              fixupDarkTokenSubsequencesForTrailingWhitespace;

          // DQ (1/20/2015): Adding support for the trailing whitespace of the
          // "true" branch of an "if" statement.
        }

        // Find the intervals of indexes into the child array of IR nodes that
        // don't have associated token subsequences already defined. Then build
        // a token mapping to represent the token sequence for that interval of
        // IR nodes.  Note that this step fills in the mappings of token stream
        // to IR nodes (or sets of IR nodes) where they could not be computed
        // base on the source position used in the evaluateInheritedAttribute()
        // function (run previous to this evaluateSynthesizedAttribute()
        // function at this point in the AST traversal).
        // for (size_t i = 0; i < childrenWithoutTokenMappings.size(); i++)
        size_t i = 0;
        while (i < childrenWithoutTokenMappings.size()) {

          size_t starting_NodeSequenceWithoutTokenMapping =
              childrenWithoutTokenMappings[i];
          size_t ending_NodeSequenceWithoutTokenMapping =
              starting_NodeSequenceWithoutTokenMapping;

          size_t j = i + 1;
          while (j < childrenWithoutTokenMappings.size() &&
                 childrenWithoutTokenMappings[j] ==
                     ending_NodeSequenceWithoutTokenMapping + 1) {
            ending_NodeSequenceWithoutTokenMapping =
                childrenWithoutTokenMappings[j];
            ++j;
          }

          i = j;

          // At this point we have identified a subsequence of children that
          // don't have an associated token sequence.
          ROSE_ASSERT(starting_NodeSequenceWithoutTokenMapping <=
                      ending_NodeSequenceWithoutTokenMapping);

          // Build a new TokenStreamSequenceToNodeMapping.
          SgNode *starting_node =
              childAttributes[starting_NodeSequenceWithoutTokenMapping].node;
          // SgNode* ending_node   =
          // childAttributes[ending_NodeSequenceWithoutTokenMapping].node;

          int leading_whitespace_start = -1;
          int leading_whitespace_end = -1;

          int start_of_token_subsequence = -1;
          int end_of_token_subsequence = -1;

          int trailing_whitespace_start = -1;
          int trailing_whitespace_end = -1;

          int else_whitespace_start = -1;
          int else_whitespace_end = -1;

          // Find the left and right edges of the token sequences.
          if (starting_NodeSequenceWithoutTokenMapping == 0) {
            // Get the source position or the start of the token stream from the
            // current node.
            start_of_token_subsequence = current_node_token_subsequence_start;

            // DQ (10/29/2013): If this is a SgBasicBlock, then the first token
            // is a "{" and so the first token of this statement must be after
            // that token.
            SgBasicBlock *basicBlock = isSgBasicBlock(n);
            if (basicBlock != NULL) {
              start_of_token_subsequence++;
            }
          } else {
            TokenStreamSequenceToNodeMapping *leftMapping = nullptr;
            for (size_t siblingIndex = starting_NodeSequenceWithoutTokenMapping;
                 siblingIndex-- > 0;) {
              SgNode *sibling = childAttributes[siblingIndex].node;
              if (sibling == nullptr) {
                continue;
              }
              auto mapping = tokenStreamSequenceMap.find(sibling);
              if (mapping == tokenStreamSequenceMap.end()) {
                continue;
              }
              if (mapping->second == nullptr ||
                  mapping->second
                          ->constructionState(
                              TokenStreamMappingConstructionAccess::key())
                          .tokenSubsequence()
                          .begin < 0 ||
                  mapping->second
                              ->constructionState(
                                  TokenStreamMappingConstructionAccess::key())
                              .tokenSubsequence()
                              .end -
                          1 <
                      mapping->second
                          ->constructionState(
                              TokenStreamMappingConstructionAccess::key())
                          .tokenSubsequence()
                          .begin) {
                fprintf(stderr,
                        "REX_TOKEN_INVARIANT[sibling-boundary]: node=%p "
                        "type=%s has an invalid left sibling mapping\n",
                        static_cast<void *>(sibling),
                        sibling->class_name().c_str());
                ROSE_ABORT();
              }
              leftMapping = mapping->second;
              break;
            }

            if (leftMapping != nullptr) {
              if (leftMapping
                          ->constructionState(
                              TokenStreamMappingConstructionAccess::key())
                          .tokenSubsequence()
                          .end -
                      1 >
                  original_end_of_token_subsequence) {
                fprintf(stderr,
                        "REX_TOKEN_INVARIANT[sibling-boundary]: node=%p "
                        "type=%s left sibling ends outside parent interval\n",
                        static_cast<void *>(n), n->class_name().c_str());
                ROSE_ABORT();
              }
              start_of_token_subsequence =
                  leftMapping->constructionState(
                                 TokenStreamMappingConstructionAccess::key())
                                  .tokenSubsequence()
                                  .end -
                              1 ==
                          original_end_of_token_subsequence
                      ? -1
                      : leftMapping
                                ->constructionState(
                                    TokenStreamMappingConstructionAccess::key())
                                .tokenSubsequence()
                                .end -
                            1 + 1;
            } else {
              start_of_token_subsequence = original_start_of_token_subsequence;
              if (isSgBasicBlock(n) != nullptr) {
                if (start_of_token_subsequence < 0 ||
                    start_of_token_subsequence >=
                        static_cast<int>(tokenStream.size()) ||
                    tokenStream[start_of_token_subsequence] == nullptr ||
                    tokenStream[start_of_token_subsequence]->p_tok_elem ==
                        nullptr ||
                    tokenStream[start_of_token_subsequence]
                            ->p_tok_elem->token_lexeme != "{") {
                  fprintf(stderr,
                          "REX_TOKEN_INVARIANT[scope-boundary]: basic block=%p "
                          "does not begin with an exact left-brace token\n",
                          static_cast<void *>(n));
                  ROSE_ABORT();
                }
                ++start_of_token_subsequence;
              }
            }
          }
          // #if 1
          // DQ (10/13/2013): I think that we can assert this.
          ROSE_ASSERT(current_node_token_subsequence_end ==
                      original_end_of_token_subsequence);

          if (ending_NodeSequenceWithoutTokenMapping + 1 ==
              childAttributes.size()) {
            // If we are at the end of the token sequence, then the
            // start_of_token_subsequence was set to -1, if so then set the
            // end_of_token_subsequence to be consistant. if
            // (start_of_token_subsequence == -1) if
            // (mappingInfo->constructionState(TokenStreamMappingConstructionAccess::key()).tokenSubsequence().end
            // - 1
            // == original_end_of_token_subsequence)
            if (start_of_token_subsequence == -1) {
              end_of_token_subsequence = -1;
            } else {
              end_of_token_subsequence = current_node_token_subsequence_end;
              // end_of_token_subsequence = original_end_of_token_subsequence;

              // DQ (10/29/2013): If this is a SgBasicBlock, then the last token
              // is a "}" and so the last token of this statement must be before
              // that token.
              SgBasicBlock *basicBlock = isSgBasicBlock(n);
              if (basicBlock != NULL) {
                printf("Reset the end_of_token_subsequence where the parent is "
                       "a SgBasicBlock: case of last statement \n");
                end_of_token_subsequence--;
              }
            }
            // Note: we can't compare signed to unsigned (else it is always
            // false). ROSE_ASSERT(start_of_token_subsequence >= 0);
            if (end_of_token_subsequence >= 0) {
              ROSE_ASSERT(end_of_token_subsequence < (int)tokenStream.size());
            }
          } else {
            TokenStreamSequenceToNodeMapping *rightMapping = nullptr;
            for (size_t siblingIndex =
                     ending_NodeSequenceWithoutTokenMapping + 1;
                 siblingIndex < childAttributes.size(); ++siblingIndex) {
              SgNode *sibling = childAttributes[siblingIndex].node;
              if (sibling == nullptr) {
                continue;
              }
              auto mapping = tokenStreamSequenceMap.find(sibling);
              if (mapping == tokenStreamSequenceMap.end()) {
                continue;
              }
              if (mapping->second == nullptr ||
                  mapping->second
                          ->constructionState(
                              TokenStreamMappingConstructionAccess::key())
                          .tokenSubsequence()
                          .begin < 0 ||
                  mapping->second
                              ->constructionState(
                                  TokenStreamMappingConstructionAccess::key())
                              .tokenSubsequence()
                              .end -
                          1 <
                      mapping->second
                          ->constructionState(
                              TokenStreamMappingConstructionAccess::key())
                          .tokenSubsequence()
                          .begin) {
                fprintf(stderr,
                        "REX_TOKEN_INVARIANT[sibling-boundary]: node=%p "
                        "type=%s has an invalid right sibling mapping\n",
                        static_cast<void *>(sibling),
                        sibling->class_name().c_str());
                ROSE_ABORT();
              }
              rightMapping = mapping->second;
              break;
            }

            if (start_of_token_subsequence == -1) {
              end_of_token_subsequence = -1;
            } else if (rightMapping != nullptr) {
              if (rightMapping
                      ->constructionState(
                          TokenStreamMappingConstructionAccess::key())
                      .tokenSubsequence()
                      .begin > original_end_of_token_subsequence + 1) {
                fprintf(
                    stderr,
                    "REX_TOKEN_INVARIANT[sibling-boundary]: node=%p "
                    "type=%s right sibling starts outside parent interval\n",
                    static_cast<void *>(n), n->class_name().c_str());
                ROSE_ABORT();
              }
              end_of_token_subsequence =
                  rightMapping
                      ->constructionState(
                          TokenStreamMappingConstructionAccess::key())
                      .tokenSubsequence()
                      .begin -
                  1;
              if (end_of_token_subsequence < start_of_token_subsequence) {
                start_of_token_subsequence = -1;
                end_of_token_subsequence = -1;
              }
            } else {
              end_of_token_subsequence = original_end_of_token_subsequence;
              if (isSgBasicBlock(n) != nullptr) {
                if (end_of_token_subsequence < 0 ||
                    end_of_token_subsequence >=
                        static_cast<int>(tokenStream.size()) ||
                    tokenStream[end_of_token_subsequence] == nullptr ||
                    tokenStream[end_of_token_subsequence]->p_tok_elem ==
                        nullptr ||
                    tokenStream[end_of_token_subsequence]
                            ->p_tok_elem->token_lexeme != "}") {
                  fprintf(stderr,
                          "REX_TOKEN_INVARIANT[scope-boundary]: basic block=%p "
                          "does not end with an exact right-brace token\n",
                          static_cast<void *>(n));
                  ROSE_ABORT();
                }
                --end_of_token_subsequence;
              }
            }

            // Note: we can't compare signed to unsigned (else it is always
            // false). ROSE_ASSERT(end_of_token_subsequence <
            // tokenStream.size());
            if (end_of_token_subsequence >= 0) {
              ROSE_ASSERT(end_of_token_subsequence < (long)tokenStream.size());
            }
          }
          // Error checking for consistancy.
          if (start_of_token_subsequence == -1) {
            ROSE_ASSERT(end_of_token_subsequence == -1);
          }

          // Error checking for consistancy.
          if (end_of_token_subsequence == -1) {
            ROSE_ASSERT(start_of_token_subsequence == -1);
          }

          // Note: we can't compare signed to unsigned (else it is always
          // false). ROSE_ASSERT(end_of_token_subsequence < tokenStream.size());
          if (end_of_token_subsequence >= 0) {
            // DQ (12/6/2016): Eliminate warning that we want to consider an
            // error: -Wsign-compare
            ROSE_ASSERT((size_t)end_of_token_subsequence < tokenStream.size());
          }

          if (start_of_token_subsequence >= 0) {
            // Trim the white space from the leading edge (and assign it to the
            // leading_whitespace_start,leading_whitespace_end values).
            leading_whitespace_start = start_of_token_subsequence;
            leading_whitespace_end = leading_whitespace_start;

            ROSE_ASSERT(leading_whitespace_start >= 0);

            // DQ (12/6/2016): Eliminate warning that we want to consider an
            // error: -Wsign-compare
            ROSE_ASSERT((size_t)leading_whitespace_start < tokenStream.size());

            ROSE_ASSERT(tokenStream[leading_whitespace_start] != NULL);
            if (tokenStream[leading_whitespace_start]->p_tok_elem != NULL) {
              ROSE_ASSERT(tokenStream[leading_whitespace_start]->p_tok_elem !=
                          NULL);
              if (tokenStream[leading_whitespace_start]->p_tok_elem->token_id ==
                  C_CXX_WHITESPACE) {
                // Increment the token subsequence at least once since the
                // current position is C_CXX_WHITESPACE.
                if (leading_whitespace_end < original_end_of_token_subsequence)
                  start_of_token_subsequence++;

                // DQ (10/29/2013): Allow for the "else" keyword to be skipped
                // over in triming tokens from the start of the current
                // statement. while (leading_whitespace_end <
                // original_end_of_token_subsequence &&
                // tokenStream[leading_whitespace_end+1]->p_tok_elem->token_id
                // == C_CXX_WHITESPACE)
                while (leading_whitespace_end <
                           original_end_of_token_subsequence &&
                       (tokenStream[leading_whitespace_end + 1]
                                ->p_tok_elem->token_id == C_CXX_ELSE ||
                        tokenStream[leading_whitespace_end + 1]
                                ->p_tok_elem->token_id == C_CXX_WHITESPACE)) {
                  leading_whitespace_end++;

                  // Increment the token subsequence for the mail token sequence
                  // specification.
                  start_of_token_subsequence++;
                }
              } else {
                // Mark this as an empty subsequence.
                leading_whitespace_start = -1;
                leading_whitespace_end = -1;
              }
            } else {
              printf("Case of CPP directive or comment as token not handled "
                     "(in adjustment of leading_whitespace_end) \n");
            }
          }

          // Note: we can't compare signed to unsigned (else it is always
          // false). ROSE_ASSERT(end_of_token_subsequence < tokenStream.size());
          if (end_of_token_subsequence >= 0) {
            // DQ (12/6/2016): Eliminate warning that we want to consider an
            // error: -Wsign-compare
            ROSE_ASSERT((size_t)end_of_token_subsequence < tokenStream.size());
          }
          if (end_of_token_subsequence >= 0) {
            // Trim the white space from the trailing edge (and assign it to the
            // trailing_whitespace_start,trailing_whitespace_end values).
            // trailing_whitespace_start = end_of_token_subsequence;
            // trailing_whitespace_end   = trailing_whitespace_start;
            trailing_whitespace_end = end_of_token_subsequence;
            trailing_whitespace_start = trailing_whitespace_end;
            // DQ (12/6/2016): Eliminate warning that we want to consider an
            // error: -Wsign-compare
            ROSE_ASSERT((size_t)trailing_whitespace_end < tokenStream.size());

            ROSE_ASSERT(tokenStream[trailing_whitespace_end] != NULL);
            if (tokenStream[trailing_whitespace_end]->p_tok_elem != NULL) {
              ROSE_ASSERT(tokenStream[trailing_whitespace_end]->p_tok_elem !=
                          NULL);
              if (tokenStream[trailing_whitespace_end]->p_tok_elem->token_id ==
                  C_CXX_WHITESPACE) {
                // Back off at least once since the current position is
                // C_CXX_WHITESPACE.
                end_of_token_subsequence--;

                // while
                // (tokenStream[trailing_whitespace_start-1]->p_tok_elem->token_id
                // == C_CXX_WHITESPACE)
                while (trailing_whitespace_start >
                           original_start_of_token_subsequence &&
                       tokenStream[trailing_whitespace_start - 1]
                               ->p_tok_elem->token_id == C_CXX_WHITESPACE) {
                  ROSE_ASSERT(trailing_whitespace_start >
                              original_start_of_token_subsequence);

                  trailing_whitespace_start--;

                  // Back off of the token subsequence for the mail token
                  // sequence specification.
                  end_of_token_subsequence--;
                }
              } else {
                // Mark this as an empty subsequence.
                trailing_whitespace_start = -1;
                trailing_whitespace_end = -1;
              }
            } else {
              printf("Case of CPP directive or comment as token not handled "
                     "(in adjustment of trailing_whitespace_start) \n");
            }
          }

          if (start_of_token_subsequence > end_of_token_subsequence) {
            start_of_token_subsequence = -1;
            end_of_token_subsequence = -1;
          }
          // #if 1
          // #if 1
          if (start_of_token_subsequence >= 0 &&
              end_of_token_subsequence >= 0) {
            size_t sizeBeforeNewTokenStreamSequenceToNodeMapping =
                tokenStreamSequenceVector.size();

            // In this case we should know that this is a new
            // TokenStreamSequenceToNodeMapping, so maybe we should call new
            // directly.
            TokenStreamSequenceToNodeMapping *element =
                TokenStreamMappingConstructionAccess::createTokenInterval(
                    currentSourceFile(), starting_node, tokenStream.size(),
                    TokenStreamMappingConstructionAccess::
                        requiredInclusiveInterval(starting_node,
                                                  "token-subsequence",
                                                  start_of_token_subsequence,
                                                  end_of_token_subsequence),
                    TokenStreamMappingConstructionAccess::
                        optionalInclusiveInterval(
                            starting_node, "leading-whitespace",
                            leading_whitespace_start, leading_whitespace_end),
                    TokenStreamMappingConstructionAccess::
                        optionalInclusiveInterval(
                            starting_node, "trailing-whitespace",
                            trailing_whitespace_start, trailing_whitespace_end),
                    TokenStreamMappingConstructionAccess::
                        optionalInclusiveInterval(
                            starting_node, "else-whitespace",
                            else_whitespace_start, else_whitespace_end));

            element->constructedInEvaluationOfSynthesizedAttribute = true;

            // Add to vector (so that we can be the last element).  Note that we
            // might be able to just lookup the element that we need instead of
            // using the last element in the vector.
            tokenStreamSequenceVector.push_back(element);

            // Add to the map so that we have the final desired data structure
            // (to attach to the SgSourceFile).
            tokenStreamSequenceMap[starting_node] = element;
            size_t sizeAfterNewTokenStreamSequenceToNodeMapping =
                tokenStreamSequenceVector.size();

            // Make sure that this has been added to the collections.
            ROSE_ASSERT(sizeAfterNewTokenStreamSequenceToNodeMapping >
                        sizeBeforeNewTokenStreamSequenceToNodeMapping);

            // for (int k = starting_NodeSequenceWithoutTokenMapping; k <
            // ending_NodeSequenceWithoutTokenMapping; k++) for (int k =
            // starting_NodeSequenceWithoutTokenMapping; k <=
            // ending_NodeSequenceWithoutTokenMapping; k++)

            // DQ (12/6/2016): Eliminate warning that we want to consider an
            // error: -Wsign-compare
            for (size_t k = starting_NodeSequenceWithoutTokenMapping;
                 k < ending_NodeSequenceWithoutTokenMapping; k++) {
              SgNode *associatedNode = childAttributes[k].node;
              if (associatedNode == nullptr) {
                fprintf(stderr,
                        "REX_TOKEN_INVARIANT[shared-interval-publication]: "
                        "owner=%p/%s child-index=%zu is null\n",
                        static_cast<void *>(element->node),
                        element->node->class_name().c_str(), k);
                ROSE_ABORT();
              }
              element->nodeVector.push_back(associatedNode);
              auto published =
                  tokenStreamSequenceMap.emplace(associatedNode, element);
              if (!published.second && published.first->second != element) {
                fprintf(stderr,
                        "REX_TOKEN_INVARIANT[shared-interval-publication]: "
                        "owner=%p/%s child=%p/%s already maps to a different "
                        "token interval\n",
                        static_cast<void *>(element->node),
                        element->node->class_name().c_str(),
                        static_cast<void *>(associatedNode),
                        associatedNode->class_name().c_str());
                ROSE_ABORT();
              }
            }
            element->shared = element->nodeVector.size() > 1;
          }
        }
      }

      // DQ (12/31/2014): Handle the "else" syntax within the SgIfStmt.
      SgIfStmt *ifStatement = isSgIfStmt(n);
      if (ifStatement != NULL) {
        // trimLeadingWhiteSpaceFromLeft(mappingInfo,original_start_of_token_subsequence);
        // trimTrailingWhiteSpaceFromRight(mappingInfo,original_end_of_token_subsequence);

        for (size_t i = 0; i < tokenToNodeVector.size(); i++) {
          TokenStreamSequenceToNodeMapping *mappingInfo = tokenToNodeVector[i];
          ROSE_ASSERT(mappingInfo->node != NULL);

          SgStatement *false_body_statement = isSgStatement(mappingInfo->node);
          if (false_body_statement == ifStatement->get_false_body()) {
            // DQ (1/18/2015): We can't always assume this with the current
            // normalization/denormalization support for SgBasicBlocks in
            // SgIfStmt true and false branches. ROSE_ASSERT(i > 0);
            if (i > 0) {
              TokenStreamSequenceToNodeMapping *previous_mappingInfo =
                  tokenToNodeVector[i - 1];
              ROSE_ASSERT(previous_mappingInfo != NULL);
              ROSE_ASSERT(previous_mappingInfo->node != NULL);

              // Note that if there is a false body, then there must be a true
              // body (so this should be easy to find as the previous child
              // node).
              SgStatement *true_body_statement =
                  isSgStatement(previous_mappingInfo->node);
              if (true_body_statement == ifStatement->get_true_body()) {
                const auto if_mapping =
                    tokenStreamSequenceMap.find(ifStatement);
                ROSE_ASSERT(if_mapping != tokenStreamSequenceMap.end());
                TokenStreamSequenceToNodeMapping *if_statement_mappingInfo =
                    if_mapping->second;

                ROSE_ASSERT(true_body_statement != NULL);
                ROSE_ASSERT(false_body_statement != NULL);
                const auto true_mapping =
                    tokenStreamSequenceMap.find(true_body_statement);
                const auto false_mapping =
                    tokenStreamSequenceMap.find(false_body_statement);
                ROSE_ASSERT(if_statement_mappingInfo != NULL);

                if (true_mapping != tokenStreamSequenceMap.end() &&
                    false_mapping != tokenStreamSequenceMap.end() &&
                    true_mapping->second != nullptr &&
                    false_mapping->second != nullptr) {
                  discoverElseSyntax(if_statement_mappingInfo,
                                     true_mapping->second,
                                     false_mapping->second);
                }
              }
            }
          }
        }
      }
    }
  }

  return TokenMappingSynthesizedAttribute(n);
}

InheritedAttribute TokenMappingTraversal::evaluateInheritedAttribute(
    SgNode *n, InheritedAttribute inheritedAttribute) {
  ROSE_ASSERT(inheritedAttribute.sourceFile == sourceFile);
  if (inheritedAttribute.subtreeRole ==
      InheritedAttribute::SubtreeRole::Semantic)
    return InheritedAttribute(sourceFile, n, std::nullopt, false,
                              InheritedAttribute::SubtreeRole::Semantic);
  // These are the bounds that we will increment and decrement to trim the size
  // of the token subsequence of leading and trailing white space.
  int start_of_token_subsequence =
      inheritedAttribute.token_interval.has_value()
          ? inheritedAttribute.token_interval->begin
          : -1;
  int end_of_token_subsequence =
      inheritedAttribute.token_interval.has_value()
          ? inheritedAttribute.token_interval->end - 1
          : -1;

  // Save the upper and lower bounds
  int original_start_of_token_subsequence = start_of_token_subsequence;
  int original_end_of_token_subsequence = end_of_token_subsequence;

  bool process_node = inheritedAttribute.processChildNodes;

  if (isSgProject(n) != NULL) {
    printf("Error: an SgProject is an inappropriate input for the "
           "TokenMappingTraversal traversal (this traversal only makes sense "
           "for a SgSourceFile) \n");
    ROSE_ABORT();
  }

  if (!inheritedAttribute.processChildNodes) {
    // Set to clear default values.
    start_of_token_subsequence = -1;
    end_of_token_subsequence = -1;
  }

  if (isSgFile(n) != NULL || isSgGlobal(n) != NULL) {
    // This is where the token stream is attached, and by definition the whole
    // token sequence represents the file. The subsequence iterators are set
    // above and should not be changed.

    int leading_whitespace_start = -1;
    int leading_whitespace_end = -1;

    // int token_subsequence_start   = start_of_token_subsequence;
    // int token_subsequence_end     = end_of_token_subsequence;
    int token_subsequence_start = 0;
    int token_subsequence_end = ((int)tokenStream.size()) - 1;
    int trailing_whitespace_start = -1;
    int trailing_whitespace_end = -1;

    int else_whitespace_start = -1;
    int else_whitespace_end = -1;

    start_of_token_subsequence = token_subsequence_start;
    end_of_token_subsequence = token_subsequence_end;

    // Generate a unique TokenStreamSequenceToNodeMapping for each interval
    // defined by (start_of_token_subsequence,end_of_token_subsequence).
    // TokenStreamSequenceToNodeMapping* element = new
    // TokenStreamSequenceToNodeMapping(n,leading_whitespace_start,leading_whitespace_end,start_of_token_subsequence,end_of_token_subsequence,trailing_whitespace_start,trailing_whitespace_end);
    TokenStreamSequenceToNodeMapping *element =
        TokenStreamMappingConstructionAccess::createTokenInterval(
            currentSourceFile(), n, tokenStream.size(),
            TokenStreamMappingConstructionAccess::requiredInclusiveInterval(
                n, "token-subsequence", start_of_token_subsequence,
                end_of_token_subsequence),
            TokenStreamMappingConstructionAccess::optionalInclusiveInterval(
                n, "leading-whitespace", leading_whitespace_start,
                leading_whitespace_end),
            TokenStreamMappingConstructionAccess::optionalInclusiveInterval(
                n, "trailing-whitespace", trailing_whitespace_start,
                trailing_whitespace_end),
            TokenStreamMappingConstructionAccess::optionalInclusiveInterval(
                n, "else-whitespace", else_whitespace_start,
                else_whitespace_end));

    // Add to vector (so that we can be the last element).  Note that we might
    // be able to just lookup the element that we need instead of using the last
    // element in the vector.
    tokenStreamSequenceVector.push_back(element);

    // Add to the map so that we have the final desired data structure (to
    // attach to the SgSourceFile).
    tokenStreamSequenceMap[n] = element;

  } else {
    // SgStatement* statement = isSgStatement(n);
    // SgStatement* locatedNode = isSgStatement(n);
    SgLocatedNode *locatedNode = isSgLocatedNode(n);

    const std::optional<TransparentTokenIntervalCarrierRole> carrier_role =
        transparentTokenIntervalCarrierRole(n, currentSourceFile());
    if (carrier_role.has_value()) {
      if (*carrier_role ==
          TransparentTokenIntervalCarrierRole::semantic_expression_subtree) {
        return InheritedAttribute(inheritedAttribute.sourceFile, n,
                                  std::nullopt, false,
                                  InheritedAttribute::SubtreeRole::Semantic);
      }
      if (*carrier_role ==
              TransparentTokenIntervalCarrierRole::source_syntax_structure ||
          *carrier_role ==
              TransparentTokenIntervalCarrierRole::catch_sequence_structure) {
        return InheritedAttribute(inheritedAttribute.sourceFile,
                                  inheritedAttribute.node,
                                  inheritedAttribute.token_interval,
                                  inheritedAttribute.processChildNodes);
      }
      if (!inheritedAttribute.token_interval.has_value()) {
        if (*carrier_role ==
            TransparentTokenIntervalCarrierRole::implicit_conversion) {
          // An implicit conversion immediately below a source expression
          // wrapper such as SgAssignInitializer has no syntax of its own and
          // that wrapper does not necessarily own a token mapping.  Continue
          // at the exact physical operand, which computes its interval from
          // its own frontend source coordinates.
          return InheritedAttribute(inheritedAttribute.sourceFile, n,
                                    std::nullopt, false);
        }
        fprintf(
            stderr,
            "REX_TOKEN_INVARIANT[transparent-interval-carrier]: node=%p/%s "
            "role=%s parent=%p/%s inherited-node=%p/%s inherited-children=%d "
            "has no exact owner interval to preserve\n",
            static_cast<void *>(n), n->class_name().c_str(),
            transparentTokenIntervalCarrierRoleName(*carrier_role),
            static_cast<void *>(n->get_parent()),
            n->get_parent() != nullptr ? n->get_parent()->class_name().c_str()
                                       : "<null>",
            static_cast<void *>(inheritedAttribute.node),
            inheritedAttribute.node != nullptr
                ? inheritedAttribute.node->class_name().c_str()
                : "<null>",
            inheritedAttribute.processChildNodes ? 1 : 0);
        ROSE_ABORT();
      }
      return InheritedAttribute(inheritedAttribute.sourceFile, n,
                                inheritedAttribute.token_interval, true);
    }

    Sg_File_Info *locatedFileInfo =
        locatedNode != nullptr ? locatedNode->get_file_info() : nullptr;
    if (locatedNode != nullptr && locatedFileInfo == nullptr) {
      fprintf(stderr,
              "REX_TOKEN_INVARIANT[located-node-source-provenance]: "
              "node=%p/%s has no primary source record\n",
              static_cast<void *>(locatedNode),
              locatedNode->class_name().c_str());
      ROSE_ABORT();
    }
    bool nodeInSourceFile =
        belongsToTokenSourceFile(locatedFileInfo, currentSourceFile());
    ROSE_ASSERT(isSgGlobal(locatedNode) == NULL);

    if (nodeInSourceFile == false) {
      start_of_token_subsequence = -1;
      end_of_token_subsequence = -1;
    }

    if ((locatedNode != NULL) && (isSgInitializedName(n) == NULL)) {
      Sg_File_Info *start_pos = locatedNode->get_startOfConstruct();
      Sg_File_Info *end_pos = locatedNode->get_endOfConstruct();
      ROSE_ASSERT(inheritedAttribute.node != NULL);
      Sg_File_Info *inheritedFileInfo =
          inheritedAttribute.node->get_file_info();
      if (inheritedFileInfo == nullptr) {
        fprintf(stderr,
                "REX_TOKEN_INVARIANT[inherited-source-provenance]: node=%p/%s "
                "has source child=%p/%s but no primary source record\n",
                static_cast<void *>(inheritedAttribute.node),
                inheritedAttribute.node->class_name().c_str(),
                static_cast<void *>(locatedNode),
                locatedNode->class_name().c_str());
        ROSE_ABORT();
      }
      bool nodeIsFromSameFileAsInheritedAttributeNode =
          inheritedFileInfo->isSameFile(locatedFileInfo);

      if (nodeIsFromSameFileAsInheritedAttributeNode == false) {
        start_of_token_subsequence = 0;
        ROSE_ASSERT(tokenStream.size() > 0);
        end_of_token_subsequence = tokenStream.size() - 1;

        original_start_of_token_subsequence = start_of_token_subsequence;
        original_end_of_token_subsequence = end_of_token_subsequence;
      }

      ROSE_ASSERT(start_pos != NULL);
      ROSE_ASSERT(end_pos != NULL);

      if (start_pos->isFrontendSpecific() == false) {
        ROSE_ASSERT(end_pos->isFrontendSpecific() == false);

        // Direct token mappings are built for statements and the one
        // expression role whose spelling belongs to a statement header.  Other
        // expressions/support nodes still carry the inherited interval so
        // source-backed declarations nested behind typed structural edges can
        // derive their own exact intervals.
        SgExpression *expression = isSgExpression(locatedNode);
        SgExpression *forStatementIncrementExpression = NULL;
        if (expression != NULL) {
          SgForStatement *forStatement =
              isSgForStatement(locatedNode->get_parent());
          if (forStatement != NULL &&
              forStatement->get_increment() == expression) {
            forStatementIncrementExpression = expression;
          }
        }
        SgDeclarationStatement *declaration =
            isSgDeclarationStatement(locatedNode);
        const bool directTokenMappingCategory =
            (isSgStatement(locatedNode) != nullptr &&
             (declaration == nullptr ||
              declarationRequiresTokenMapping(declaration,
                                              currentSourceFile()))) ||
            forStatementIncrementExpression != nullptr;
        if (directTokenMappingCategory && nodeInSourceFile &&
            !start_pos->isCompilerGenerated()) {
          std::optional<TokenStreamHalfOpenInterval> direct_owner_interval;
          if (start_of_token_subsequence >= 0 &&
              end_of_token_subsequence >= start_of_token_subsequence &&
              end_of_token_subsequence != std::numeric_limits<int>::max()) {
            direct_owner_interval = TokenStreamHalfOpenInterval(
                start_of_token_subsequence, end_of_token_subsequence + 1);
          }
          TokenStreamMappingConstructionAccess::requireDirectOwnerInterval(
              n, currentSourceFile()->getFileName().c_str(),
              std::move(direct_owner_interval));
        }
        process_node = directTokenMappingCategory;

        int starting_line = -1;
        int starting_column = -1;
        int ending_line = -1;
        int ending_column = -1;

        SgFunctionDeclaration *functionDeclaration = isSgFunctionDeclaration(n);

        if (process_node && nodeInSourceFile) {
          starting_line = start_pos->get_physical_line();
          starting_column = start_pos->get_col();
          ending_line = end_pos->get_physical_line();
          ending_column = end_pos->get_col();

          bool subtreeHasValidSourcePosition = true;
          bool evaluateForLoopIncrementExpression = false;
          SgForStatement *forStatement = isSgForStatement(n->get_parent());
          if (forStatement != NULL) {
            if (n == forStatement->get_increment()) {
              evaluateForLoopIncrementExpression = true;
            }
          }
          if (start_pos->isCompilerGenerated() == true &&
              (isSgExprStatement(n) != NULL ||
               evaluateForLoopIncrementExpression == true) &&
              (inheritedAttribute.processChildNodes == true)) {
            // These are passed by reference to computeMaxSourceExtents().
            int computed_start_line = 0;
            int computed_start_column = 0;
            int computed_end_line = 0;
            int computed_end_column = 0;

            MaxSourceExtents::computeMaxSourceExtents(
                currentSourceFile(), n, computed_start_line,
                computed_start_column, computed_end_line, computed_end_column);
            if (computed_start_line == computed_end_line &&
                computed_start_column == computed_end_column) {
              // DQ (1/26/2015): I think we would have to apply this to
              // non-compiler generated statements to detect macros expansions
              // (might be expensive).
              printf(
                  "Detected macro after evaluate of sub-tree source positions: "
                  "computed_start_line = %d computed_start_column = %d "
                  "computed_end_line = %d computed_end_column = %d \n",
                  computed_start_line, computed_start_column, computed_end_line,
                  computed_end_column);

              subtreeHasValidSourcePosition = false;
            } else {
              subtreeHasValidSourcePosition = true;
            }

            starting_line = computed_start_line;
            starting_column = computed_start_column;
            ending_line = computed_end_line;
            ending_column = computed_end_column;
          }

          process_node = directTokenMappingCategory &&
                         (start_pos->isCompilerGenerated() == false ||
                          subtreeHasValidSourcePosition == true) &&
                         inheritedAttribute.processChildNodes;

          ROSE_ASSERT(currentSourceFile() != NULL);
          process_node =
              process_node && (start_pos->isSameFile(currentSourceFile()) ||
                               subtreeHasValidSourcePosition == true);
          process_node = process_node && ((starting_line < ending_line) ||
                                          ((starting_line == ending_line) &&
                                           (starting_column <= ending_column)));
        }

        if (process_node && nodeInSourceFile) {
          if (start_of_token_subsequence < 0 ||
              end_of_token_subsequence < start_of_token_subsequence ||
              static_cast<size_t>(end_of_token_subsequence) >=
                  tokenStream.size()) {
            fprintf(stderr,
                    "REX_TOKEN_INVARIANT[source-interval]: node=%p/%s has "
                    "invalid token interval [%d, %d] in %zu tokens\n",
                    static_cast<void *>(n), n->class_name().c_str(),
                    start_of_token_subsequence, end_of_token_subsequence,
                    tokenStream.size());
            ROSE_ABORT();
          }
          bool isNullForInitStatement = false;
          SgForInitStatement *forInitStatement = isSgForInitStatement(n);
          if (forInitStatement != NULL) {
            ROSE_ASSERT(!forInitStatement->get_init_stmt().empty());
            isNullForInitStatement =
                isSgNullStatement(forInitStatement->get_init_stmt().front()) !=
                nullptr;
          }

          if (isNullForInitStatement) {
            while (start_of_token_subsequence < end_of_token_subsequence &&
                   tokenStream[start_of_token_subsequence]
                           ->p_tok_elem->token_lexeme != ";") {
              start_of_token_subsequence++;
            }
            if (tokenStream[start_of_token_subsequence]
                    ->p_tok_elem->token_lexeme != ";") {
              fprintf(stderr,
                      "REX_TOKEN_INVARIANT[for-init-semicolon]: node=%p/%s "
                      "has no semicolon in its token interval\n",
                      static_cast<void *>(n), n->class_name().c_str());
              ROSE_ABORT();
            }

            end_of_token_subsequence = start_of_token_subsequence;
          } else {
            while (start_of_token_subsequence < end_of_token_subsequence &&
                   (tokenStream[start_of_token_subsequence]
                            ->beginning_fpi.line_num < starting_line ||
                    (tokenStream[start_of_token_subsequence]
                             ->beginning_fpi.line_num == starting_line &&
                     tokenStream[start_of_token_subsequence]
                             ->beginning_fpi.column_num < starting_column))) {
              start_of_token_subsequence++;
            }

            while (
                end_of_token_subsequence > start_of_token_subsequence &&
                (tokenStream[end_of_token_subsequence]->ending_fpi.line_num >
                     ending_line ||
                 (tokenStream[end_of_token_subsequence]->ending_fpi.line_num ==
                      ending_line &&
                  tokenStream[end_of_token_subsequence]->ending_fpi.column_num >
                      ending_column))) {
              end_of_token_subsequence--;
            }
          }

          if (isSgClassDefinition(n) != nullptr ||
              isSgNamespaceDefinitionStatement(n) != nullptr) {
            const int saved_start_of_token_subsequence =
                start_of_token_subsequence;
            const int saved_end_of_token_subsequence = end_of_token_subsequence;
            const int token_stream_size = static_cast<int>(tokenStream.size());
            const int safe_upper_bound =
                std::min(original_end_of_token_subsequence, token_stream_size);

            if (saved_start_of_token_subsequence >= 0 &&
                saved_start_of_token_subsequence < token_stream_size &&
                saved_end_of_token_subsequence >= 0 &&
                saved_end_of_token_subsequence < token_stream_size) {
              while ((start_of_token_subsequence < safe_upper_bound) &&
                     (tokenStream[start_of_token_subsequence]
                          ->p_tok_elem->token_lexeme != "{")) {
                start_of_token_subsequence++;
              }

              if (start_of_token_subsequence >= token_stream_size ||
                  tokenStream[start_of_token_subsequence]
                          ->p_tok_elem->token_lexeme != "{") {
                start_of_token_subsequence = saved_start_of_token_subsequence;
              }

              while ((end_of_token_subsequence >
                      original_start_of_token_subsequence) &&
                     (tokenStream[end_of_token_subsequence]
                          ->p_tok_elem->token_lexeme != "}")) {
                end_of_token_subsequence--;
              }

              if (tokenStream[end_of_token_subsequence]
                      ->p_tok_elem->token_lexeme != "}") {
                end_of_token_subsequence = saved_end_of_token_subsequence;
              }
            }
          }

          SgForStatement *parent_is_forStatement =
              isSgForStatement(n->get_parent());
          if (parent_is_forStatement != NULL &&
              n == parent_is_forStatement->get_test()) {
            while ((end_of_token_subsequence <
                    original_end_of_token_subsequence) &&
                   (tokenStream[end_of_token_subsequence]
                        ->p_tok_elem->token_lexeme != ";")) {
              end_of_token_subsequence++;
            }
          }

          if (isSgBasicBlock(locatedNode) != nullptr &&
              isSgSwitchStatement(locatedNode->get_parent()) != nullptr) {
            while ((start_of_token_subsequence >
                    original_start_of_token_subsequence) &&
                   (tokenStream[start_of_token_subsequence]
                        ->p_tok_elem->token_lexeme != "{")) {
              start_of_token_subsequence--;
            }
          }

          if (isSgCaseOptionStmt(locatedNode) != nullptr) {
            while ((start_of_token_subsequence >
                    original_start_of_token_subsequence) &&
                   (tokenStream[start_of_token_subsequence]
                        ->p_tok_elem->token_lexeme != "case")) {
              start_of_token_subsequence--;
            }
          }
        } else if (!nodeInSourceFile || directTokenMappingCategory) {
          // A directly mappable node that cannot establish an interval must
          // not lend an unrelated ancestor interval to its children.  A node
          // from another file likewise carries no interval in this file's
          // token stream.  Descendant traversal still continues so a typed
          // source-file boundary can establish a new exact interval.
          start_of_token_subsequence = -1;
          end_of_token_subsequence = -1;
        } else {
          // This source-backed structural carrier has no direct token mapping.
          // Preserve the inherited half-open interval exactly for descendants.
          process_node = false;
        }

        if (nodeInSourceFile && directTokenMappingCategory &&
            declarationGroupOwner(n) == nullptr) {
          // Disallow the default value: -1
          if (start_of_token_subsequence >= 0 &&
              end_of_token_subsequence >= 0) {
            int leading_whitespace_start = -1;
            int leading_whitespace_end = -1;
            int trailing_whitespace_start = -1;
            int trailing_whitespace_end = -1;
            int else_whitespace_start = -1;
            int else_whitespace_end = -1;

            ROSE_ASSERT(static_cast<size_t>(end_of_token_subsequence) <
                        tokenStream.size());

            TokenStreamSequenceToNodeMapping *element =
                TokenStreamMappingConstructionAccess::createTokenInterval(
                    currentSourceFile(), n, tokenStream.size(),
                    TokenStreamMappingConstructionAccess::
                        requiredInclusiveInterval(n, "token-subsequence",
                                                  start_of_token_subsequence,
                                                  end_of_token_subsequence),
                    TokenStreamMappingConstructionAccess::
                        optionalInclusiveInterval(n, "leading-whitespace",
                                                  leading_whitespace_start,
                                                  leading_whitespace_end),
                    TokenStreamMappingConstructionAccess::
                        optionalInclusiveInterval(n, "trailing-whitespace",
                                                  trailing_whitespace_start,
                                                  trailing_whitespace_end),
                    TokenStreamMappingConstructionAccess::
                        optionalInclusiveInterval(n, "else-whitespace",
                                                  else_whitespace_start,
                                                  else_whitespace_end));
            // One located node has one structural token owner. ROSE ASTs can
            // expose the same node through multiple traversal edges, so an
            // identical interned mapping is an idempotent revisit. A distinct
            // interval for the same node remains a hard ownership error.
            auto existingMapping = tokenStreamSequenceMap.find(n);
            if (existingMapping != tokenStreamSequenceMap.end()) {
              TokenStreamSequenceToNodeMapping *existing =
                  existingMapping->second;
              if (existing == nullptr) {
                fprintf(stderr,
                        "REX_TOKEN_INVARIANT[duplicate-node-mapping]: "
                        "node=%p type=%s has a null existing mapping\n",
                        static_cast<void *>(n), n->class_name().c_str());
                ROSE_ABORT();
              }
              if (existing != element ||
                  existing->constructionState(
                              TokenStreamMappingConstructionAccess::key())
                          .tokenSubsequence()
                          .begin != start_of_token_subsequence ||
                  existing->constructionState(
                              TokenStreamMappingConstructionAccess::key())
                              .tokenSubsequence()
                              .end -
                          1 !=
                      end_of_token_subsequence) {
                fprintf(
                    stderr,
                    "REX_TOKEN_INVARIANT[duplicate-node-mapping]: "
                    "node=%p type=%s parent=%p(%s) "
                    "existing=%p interval=[%d,%d] new=%p interval=[%d,%d]\n",
                    static_cast<void *>(n), n->class_name().c_str(),
                    static_cast<void *>(n->get_parent()),
                    n->get_parent() != nullptr
                        ? n->get_parent()->class_name().c_str()
                        : "<null>",
                    static_cast<void *>(existing),
                    existing
                        ->constructionState(
                            TokenStreamMappingConstructionAccess::key())
                        .tokenSubsequence()
                        .begin,
                    existing->constructionState(
                                TokenStreamMappingConstructionAccess::key())
                            .tokenSubsequence()
                            .end -
                        1,
                    static_cast<void *>(element),
                    element
                        ->constructionState(
                            TokenStreamMappingConstructionAccess::key())
                        .tokenSubsequence()
                        .begin,
                    element->constructionState(
                               TokenStreamMappingConstructionAccess::key())
                            .tokenSubsequence()
                            .end -
                        1);
                ROSE_ABORT();
              }
            } else {
              // Add to vector (so that we can be the last element). Note that
              // we might be able to just look up the element instead of using
              // the last element in the vector.
              tokenStreamSequenceVector.push_back(element);

              // Add to the map so that we have the final desired data
              // structure (to attach to the SgSourceFile).
              tokenStreamSequenceMap[n] = element;
            }
          }
        }
      } else {
        // DQ (8/1/2018): This fails for the combination of token based
        // unparsing and unparse headers option.
        ROSE_ASSERT(end_pos != NULL);

        ROSE_ASSERT(end_pos->isFrontendSpecific() == true);
      }
    }
  }

  if (start_of_token_subsequence > end_of_token_subsequence) {
    printf("Error: evaluateInheritedAttribute(): n = %p = %s \n", n,
           n->class_name().c_str());
    SgLocatedNode *locatedNode = isSgLocatedNode(n);
    if (locatedNode != NULL) {
      locatedNode->get_startOfConstruct()->display(
          "Error: evaluateInheritedAttribute(): start");
      locatedNode->get_endOfConstruct()->display(
          "Error: evaluateInheritedAttribute(): end");
    }
  }

  // DQ (11/24/2018): Error that I need to debug.
  if (start_of_token_subsequence > end_of_token_subsequence) {
    printf("ERROR: Failing test: (start_of_token_subsequence <= "
           "end_of_token_subsequence): \n");
    printf("   --- start_of_token_subsequence = %d \n",
           start_of_token_subsequence);
    printf("   --- end_of_token_subsequence   = %d \n",
           end_of_token_subsequence);
  }

  ROSE_ASSERT(start_of_token_subsequence <= end_of_token_subsequence);

  // DQ (12/6/2016): Added assertion as part of fix for warning now considered
  // to be an error.
  ROSE_ASSERT(end_of_token_subsequence >= -1);
  ROSE_ASSERT(end_of_token_subsequence == -1 ||
              (size_t)end_of_token_subsequence < tokenStream.size());

  ROSE_ASSERT(currentSourceFile() != NULL);

  // Select the representative statement to use in formatting transformations in
  // the token based unparsing.
  SgScopeStatement *scopeStatement = isSgScopeStatement(n);
  if (scopeStatement != NULL) {
    // Save a statement from each scope.
    SgGlobal *globalScope = isSgGlobal(scopeStatement);
    SgBasicBlock *basicBlock = isSgBasicBlock(scopeStatement);
    SgClassDefinition *classDefinition = isSgClassDefinition(scopeStatement);
    SgNamespaceDefinitionStatement *namespaceDefinition =
        isSgNamespaceDefinitionStatement(scopeStatement);

    if (globalScope != NULL || basicBlock != NULL || classDefinition != NULL ||
        namespaceDefinition != NULL) {
      SgStatement *firstStatement = NULL;

      // Note that this is the efficent way to access the first statement in any
      // scope containing a list of statements or declarations.
      if (scopeStatement->containsOnlyDeclarations() == true) {
        if (scopeStatement->getDeclarationList().empty() == false) {
          SgDeclarationStatement *firstDeclaration =
              *(scopeStatement->getDeclarationList().begin());
          firstStatement = firstDeclaration;
        } else {
          // Not clear what to do here.
        }
      } else {
        if (scopeStatement->getStatementList().empty() == false) {
          firstStatement = *(scopeStatement->getStatementList().begin());
        } else {
          // Not clear what to do here.
        }
      }

      if (firstStatement != NULL) {
        ROSE_ASSERT(scopeStatement != NULL);
        if (representativeWhitespaceStatementMap.find(scopeStatement) !=
            representativeWhitespaceStatementMap.end()) {
          // Revisited scopes are expected here for some large inputs; avoid
          // unconditional hot-path stdout churn.
          if (SgProject::get_verbose() > 0) {
            printf("NOTE: "
                   "(representativeWhitespaceStatementMap.find(scopeStatement) "
                   "!= representativeWhitespaceStatementMap.end()): scope "
                   "revisited \n");
          }
        }
        // Allow this case while we debug this.
        if (representativeWhitespaceStatementMap.find(scopeStatement) ==
            representativeWhitespaceStatementMap.end()) {
          representativeWhitespaceStatementMap[scopeStatement] = firstStatement;
        }
      }
    }
  }

  const bool start_absent = start_of_token_subsequence == -1;
  const bool end_absent = end_of_token_subsequence == -1;
  if (start_absent != end_absent ||
      (!start_absent &&
       (start_of_token_subsequence < 0 ||
        end_of_token_subsequence < start_of_token_subsequence ||
        end_of_token_subsequence == std::numeric_limits<int>::max()))) {
    fprintf(stderr,
            "REX_TOKEN_INVARIANT[inherited-interval]: node=%p/%s produced "
            "invalid inclusive token endpoints [%d,%d]\n",
            static_cast<void *>(n), n->class_name().c_str(),
            start_of_token_subsequence, end_of_token_subsequence);
    ROSE_ABORT();
  }
  std::optional<TokenStreamHalfOpenInterval> returned_interval;
  if (!start_absent) {
    returned_interval = TokenStreamHalfOpenInterval(
        start_of_token_subsequence, end_of_token_subsequence + 1);
  }
  // Direct-node mappability and descendant discovery are independent.  Every
  // structural edge remains traversable; absence of an interval remains
  // explicit and is rejected when a same-file source child requires one.
  return InheritedAttribute(inheritedAttribute.sourceFile, n,
                            std::move(returned_interval), true);
}

// DQ (1/20/2021): Changed the API to add a pointer to the
// map<SgNode*,TokenStreamSequenceToNodeMapping*>.
TokenMappingTraversal::TokenMappingTraversal(
    vector<stream_element *> &ts, SgSourceFile *input_sourceFile,
    map<SgNode *, TokenStreamSequenceToNodeMapping *>
        *tokenStreamSequenceMapPointer)
    : tokenStream(ts), sourceFile(input_sourceFile),
      tokenStreamSequenceMap(*tokenStreamSequenceMapPointer) {

  ROSE_ASSERT(tokenStream.empty() == false);
  ROSE_ASSERT(sourceFile != NULL);
}

vector<stream_element *> getTokenStream(SgSourceFile *file) {
  string fileNameForTokenStream = file->getFileName();

  ROSE_ASSERT(file->get_preprocessorDirectivesAndCommentsList() != NULL);
  ROSEAttributesListContainerPtr filePreprocInfo =
      file->get_preprocessorDirectivesAndCommentsList();

  // We should at least have the current files CPP/Comment/Token information
  // (even if it is an empty file).
  ROSE_ASSERT(filePreprocInfo->getList().size() > 0);

  std::map<std::string, ROSEAttributesList *>::iterator currentFileItr =
      filePreprocInfo->getList().find(fileNameForTokenStream);
  ROSE_ASSERT(currentFileItr != filePreprocInfo->getList().end());

  // If there already exists a list for the current file then get that list.
  ROSE_ASSERT(currentFileItr->second != NULL);

  ROSEAttributesList *existingListOfAttributes = currentFileItr->second;

  LexTokenStreamType *tokenStream =
      existingListOfAttributes->get_rawTokenStream();
  ROSE_ASSERT(tokenStream != NULL);

  // Convert this list to a vectors so that we can use integer indexing instead
  // of iterators into a list.
  vector<stream_element *> tokenVector;
  for (LexTokenStreamType::iterator i = tokenStream->begin();
       i != tokenStream->end(); i++) {
    tokenVector.push_back(*i);
  }

  // A physically empty source file has an exact empty token sequence.
  return tokenVector;
}

namespace {

bool tokenPrecedesSourcePosition(stream_element *token, int line, int column) {
  if (token == NULL) {
    fprintf(stderr,
            "REX_TOKEN_INVARIANT[source-position]: null token while locating "
            "a source range\n");
    ROSE_ABORT();
  }
  if (token->ending_fpi.line_num < line) {
    return true;
  }
  if (token->ending_fpi.line_num == line &&
      token->ending_fpi.column_num < column) {
    return true;
  }
  return false;
}

bool tokenFollowsSourcePosition(stream_element *token, int line, int column) {
  if (token == NULL) {
    fprintf(stderr,
            "REX_TOKEN_INVARIANT[source-position]: null token while locating "
            "a source range\n");
    ROSE_ABORT();
  }
  if (token->beginning_fpi.line_num > line) {
    return true;
  }
  if (token->beginning_fpi.line_num == line &&
      token->beginning_fpi.column_num > column) {
    return true;
  }
  return false;
}

bool isIgnorableToken(stream_element *token) {
  if (token == nullptr || token->p_tok_elem == nullptr) {
    fprintf(stderr,
            "REX_TOKEN_INVARIANT[source-position]: token stream contains an "
            "incomplete token\n");
    ROSE_ABORT();
  }
  return token->p_tok_elem->token_id == C_CXX_WHITESPACE ||
         token->p_tok_elem->token_id == C_CXX_PREPROCESSING_INFO ||
         token->p_tok_elem->token_id == C_CXX_COMMENTS;
}

int findFirstTokenAtOrAfterSourcePosition(
    const std::vector<stream_element *> &tokenVector, int lowerBound,
    int upperBound, int line, int column) {
  lowerBound = std::max(0, lowerBound);
  upperBound = std::min(static_cast<int>(tokenVector.size()) - 1, upperBound);
  for (int i = lowerBound; i <= upperBound; ++i) {
    if (isIgnorableToken(tokenVector[i])) {
      continue;
    }
    if (!tokenPrecedesSourcePosition(tokenVector[i], line, column)) {
      return i;
    }
  }
  return -1;
}

int findLastTokenAtOrBeforeSourcePosition(
    const std::vector<stream_element *> &tokenVector, int lowerBound,
    int upperBound, int line, int column) {
  lowerBound = std::max(0, lowerBound);
  upperBound = std::min(static_cast<int>(tokenVector.size()) - 1, upperBound);
  for (int i = upperBound; i >= lowerBound; --i) {
    if (isIgnorableToken(tokenVector[i])) {
      continue;
    }
    if (!tokenFollowsSourcePosition(tokenVector[i], line, column)) {
      return i;
    }
  }
  return -1;
}

int findTrailingSemicolonTokenIndex(
    const std::vector<stream_element *> &tokenVector, int lowerBound,
    int upperBound) {
  lowerBound = std::max(0, lowerBound);
  upperBound = std::min(static_cast<int>(tokenVector.size()) - 1, upperBound);
  for (int i = lowerBound; i <= upperBound; ++i) {
    if (tokenVector[i] == nullptr || tokenVector[i]->p_tok_elem == nullptr) {
      fprintf(stderr,
              "REX_TOKEN_INVARIANT[source-position]: token stream contains "
              "an incomplete token\n");
      ROSE_ABORT();
    }
    if (tokenVector[i]->p_tok_elem->token_lexeme == ";") {
      return i;
    }
  }
  return -1;
}

bool isTypedEmbeddedDeclaratorTag(SgDeclarationStatement *declaration) {
  ASSERT_not_null(declaration);

  SgNode *owner = declaration->get_parent();
  bool exactTypedOwner = false;
  if (SgTypedefDeclaration *typedefOwner = isSgTypedefDeclaration(owner)) {
    const bool exactNondefiningEdge =
        typedefOwner->get_baseTypeNondefiningDeclaration() == declaration;
    const bool exactDefiningEdge =
        typedefOwner->get_baseTypeDefiningDeclaration() == declaration;
    exactTypedOwner = exactNondefiningEdge != exactDefiningEdge;
  } else if (SgVariableDeclaration *variableOwner =
                 isSgVariableDeclaration(owner)) {
    exactTypedOwner =
        variableOwner->get_baseTypeNondefiningDeclaration() == declaration ||
        variableOwner->get_baseTypeDefiningDeclaration() == declaration;
  } else {
    return false;
  }

  SgClassDeclaration *classDeclaration = isSgClassDeclaration(declaration);
  SgEnumDeclaration *enumDeclaration = isSgEnumDeclaration(declaration);
  const bool nonAutonomousTag =
      (classDeclaration != nullptr &&
       !classDeclaration->get_isAutonomousDeclaration()) ||
      (enumDeclaration != nullptr &&
       !enumDeclaration->get_isAutonomousDeclaration());
  if (!exactTypedOwner || !nonAutonomousTag) {
    fprintf(stderr,
            "REX_TOKEN_INVARIANT[embedded-declarator-owner]: declaration=%p/%s "
            "parent=%p/%s is not one exact non-autonomous typed tag child\n",
            static_cast<void *>(declaration), declaration->class_name().c_str(),
            static_cast<void *>(owner), owner->class_name().c_str());
    ROSE_ABORT();
  }

  if (declaration->get_file_info() == nullptr ||
      declaration->get_startOfConstruct() == nullptr ||
      declaration->get_endOfConstruct() == nullptr) {
    fprintf(stderr,
            "REX_TOKEN_INVARIANT[embedded-declarator-source]: "
            "declaration=%p/%s parent=%p/%s is an exact typed child without "
            "complete source provenance\n",
            static_cast<void *>(declaration), declaration->class_name().c_str(),
            static_cast<void *>(owner), owner->class_name().c_str());
    ROSE_ABORT();
  }

  return true;
}

bool declarationRequiresTokenMapping(SgDeclarationStatement *decl,
                                     SgSourceFile *sourceFile) {
  ASSERT_not_null(decl);
  ASSERT_not_null(sourceFile);
  // SgVariableDefinition is the typed definition edge of one
  // SgInitializedName.  It inherits declaration ancestry for semantic APIs,
  // but it is not a lexical declaration surface: the enclosing
  // SgVariableDeclaration owns every source token for the declarator.  Verify
  // the exact bidirectional edge before excluding it from declaration-sibling
  // token grouping.
  if (SgVariableDefinition *definition = isSgVariableDefinition(decl)) {
    SgInitializedName *initialized_name = definition->get_vardefn();
    const std::vector<SgNode *> successors =
        initialized_name != nullptr
            ? initialized_name->get_traversalSuccessorContainer()
            : std::vector<SgNode *>();
    if (initialized_name == nullptr ||
        definition->get_parent() != initialized_name ||
        initialized_name->get_definition() != definition ||
        std::count(successors.begin(), successors.end(), definition) != 1) {
      fprintf(stderr,
              "REX_TOKEN_INVARIANT[variable-definition-owner]: "
              "definition=%p initialized-name=%p parent=%p does not form one "
              "exact typed definition edge\n",
              static_cast<void *>(definition),
              static_cast<void *>(initialized_name),
              static_cast<void *>(definition->get_parent()));
      ROSE_ABORT();
    }
    return false;
  }
  if (decl->get_source_range_is_macro_expansion_fragment()) {
    // The frontend proved that this node is a semantic component inside a
    // larger macro replacement.  The enclosing macro surface owns the physical
    // invocation tokens; assigning those same coordinates to this declaration
    // would fabricate an independent lexical declaration.
    return false;
  }
  if (SgDeclarationGroupStatement *group = declarationGroupOwner(decl)) {
    const size_t occurrences =
        static_cast<size_t>(std::count(group->get_declarations().begin(),
                                       group->get_declarations().end(), decl));
    if (occurrences != 1) {
      fprintf(stderr,
              "REX_TOKEN_INVARIANT[declaration-group]: member=%p/%s occurs "
              "%zu times in group=%p\n",
              static_cast<void *>(decl), decl->class_name().c_str(),
              occurrences, static_cast<void *>(group));
      ROSE_ABORT();
    }
    return false;
  }
  // These declaration nodes are structural components of a function
  // declaration, not independently emitted declaration statements.  Their
  // source tokens belong to the enclosing function declaration.
  if (isSgFunctionParameterList(decl) != nullptr ||
      isSgCtorInitializerList(decl) != nullptr) {
    return false;
  }
  if (SgTemplateInstantiationDirectiveStatement *directive =
          isSgTemplateInstantiationDirectiveStatement(decl->get_parent())) {
    const std::vector<SgNode *> successors =
        directive->get_traversalSuccessorContainer();
    if (directive->get_declaration() != decl ||
        directive->get_scope() == nullptr || decl->get_scope() == nullptr ||
        std::count(successors.begin(), successors.end(), decl) != 1) {
      fprintf(stderr,
              "REX_TOKEN_INVARIANT[instantiation-directive-owner]: "
              "directive=%p declaration=%p/%s lacks one exact typed child "
              "edge\n",
              static_cast<void *>(directive), static_cast<void *>(decl),
              decl->class_name().c_str());
      ROSE_ABORT();
    }
    // The directive is the written declaration surface; its declaration child
    // carries template identity and signature semantics but is not a sibling in
    // the containing lexical declaration sequence.
    return false;
  }
  // SgAsmStmt retains declaration ancestry even though its lexical role is a
  // statement.  It is nevertheless an autonomous written surface at both file
  // and block scope, so it must pass through the normal physical-source checks
  // below and own one direct mapping.  Excluding it here also excludes it from
  // the statement-mapping path because the traversal deliberately consults
  // this predicate for every statement with declaration ancestry.
  // A tag definition owned through the typedef/variable declarator edge is a
  // semantic component of that enclosing declaration.  Its exact source range
  // remains available for provenance, but its tokens are owned by the enclosing
  // lexical surface (or by that surface's declaration group).  It must never be
  // treated as a direct declaration sibling of its typed declaration parent.
  if (isTypedEmbeddedDeclaratorTag(decl)) {
    return false;
  }
  if (SgEmptyDeclaration *empty = isSgEmptyDeclaration(decl)) {
    empty->validate_lexical_role();
    if (empty->get_lexical_role() !=
        SgEmptyDeclaration::e_empty_declaration_source_semicolon) {
      return false;
    }
  }
  Sg_File_Info *fileInfo = decl->get_file_info();
  if (fileInfo == nullptr) {
    fprintf(stderr,
            "REX_TOKEN_INVARIANT[declaration-mapping]: declaration=%p "
            "type=%s has no file information\n",
            static_cast<void *>(decl), decl->class_name().c_str());
    ROSE_ABORT();
  }
  if (!fileInfo->isOutputInCodeGeneration() ||
      fileInfo->isCompilerGenerated() || fileInfo->isFrontendSpecific() ||
      fileInfo->isTransformation()) {
    return false;
  }
  if (fileInfo->get_physical_file_id() < 0) {
    fprintf(stderr,
            "REX_TOKEN_INVARIANT[declaration-mapping]: declaration=%p "
            "type=%s has no physical file ownership\n",
            static_cast<void *>(decl), decl->class_name().c_str());
    ROSE_ABORT();
  }
  return fileInfo->isSameFile(sourceFile);
}

using DirectDeclarationSequence = std::vector<SgDeclarationStatement *>;

DirectDeclarationSequence directDeclarationSequence(SgNode *parent) {
  ASSERT_not_null(parent);
  DirectDeclarationSequence result;

  auto appendStatement = [&](SgStatement *statement) {
    if (statement == nullptr) {
      fprintf(stderr,
              "REX_TOKEN_INVARIANT[structural-sibling]: parent=%p/%s owns a "
              "null statement\n",
              static_cast<void *>(parent), parent->class_name().c_str());
      ROSE_ABORT();
    }
    // Keep a null declaration sentinel for a non-declaration statement.  A
    // statement between two declarations is a real structural group barrier.
    result.push_back(isSgDeclarationStatement(statement));
  };

  if (SgForInitStatement *forInit = isSgForInitStatement(parent)) {
    for (SgStatement *statement : forInit->get_init_stmt()) {
      appendStatement(statement);
    }
    return result;
  }

  // Condition declarations are owned through a typed statement edge rather
  // than through an SgScopeStatement statement list.  Treat that edge as the
  // complete direct declaration sequence for its owner.  In particular,
  // calling SgScopeStatement::getStatementList() on these statement kinds is
  // not a valid way to recover their structural children.
  if (SgIfStmt *ifStatement = isSgIfStmt(parent)) {
    if (ifStatement->get_is_if_consteval_statement())
      ROSE_ASSERT(ifStatement->get_conditional() == nullptr);
    else
      appendStatement(ifStatement->get_conditional());
    appendStatement(ifStatement->get_true_body());
    if (ifStatement->get_false_body() != nullptr) {
      appendStatement(ifStatement->get_false_body());
    }
    return result;
  }
  if (SgWhileStmt *whileStatement = isSgWhileStmt(parent)) {
    appendStatement(whileStatement->get_condition());
    appendStatement(whileStatement->get_body());
    return result;
  }
  if (SgSwitchStatement *switchStatement = isSgSwitchStatement(parent)) {
    appendStatement(switchStatement->get_item_selector());
    appendStatement(switchStatement->get_body());
    return result;
  }
  if (SgForStatement *forStatement = isSgForStatement(parent)) {
    appendStatement(forStatement->get_test());
    appendStatement(forStatement->get_loop_body());
    return result;
  }
  if (SgCatchOptionStmt *catchStatement = isSgCatchOptionStmt(parent)) {
    appendStatement(catchStatement->get_condition());
    appendStatement(catchStatement->get_body());
    return result;
  }
  if (SgRangeBasedForStatement *rangeFor = isSgRangeBasedForStatement(parent)) {
    // A range-for is a semantic scope, but it does not own an ordinary
    // statement list.  Its source iterator and semantic lowering declarations
    // are distinct typed children separated by expression/body roles; preserve
    // that exact traversal order instead of treating them as lexical siblings.
    const SgNodePtrList successors =
        rangeFor->get_traversalSuccessorContainer();
    for (SgVariableDeclaration *declaration :
         {rangeFor->get_iterator_declaration(),
          rangeFor->get_range_declaration(), rangeFor->get_begin_declaration(),
          rangeFor->get_end_declaration()}) {
      if (declaration == nullptr) {
        continue;
      }
      if (declaration->get_parent() != rangeFor ||
          declaration->get_scope() != rangeFor ||
          std::count(successors.begin(), successors.end(), declaration) != 1) {
        fprintf(stderr,
                "REX_TOKEN_INVARIANT[structural-sibling]: range-for=%p typed "
                "declaration=%p has no exact parent, scope, and traversal "
                "edge\n",
                static_cast<void *>(rangeFor),
                static_cast<void *>(declaration));
        ROSE_ABORT();
      }
    }
    if (rangeFor->get_iterator_declaration() == nullptr ||
        rangeFor->get_range_declaration() == nullptr) {
      fprintf(stderr,
              "REX_TOKEN_INVARIANT[structural-sibling]: range-for=%p lacks "
              "its required iterator or range declaration\n",
              static_cast<void *>(rangeFor));
      ROSE_ABORT();
    }
    for (SgNode *successor : successors) {
      result.push_back(isSgDeclarationStatement(successor));
    }
    return result;
  }

  auto appendTypedExpressionTag =
      [&](SgDeclarationStatement *declaration,
          const char *ownerType) -> DirectDeclarationSequence {
    const std::vector<SgNode *> successors =
        parent->get_traversalSuccessorContainer();
    const size_t occurrences = static_cast<size_t>(
        std::count(successors.begin(), successors.end(), declaration));
    if (declaration == nullptr || declaration->get_parent() != parent ||
        occurrences != 1) {
      fprintf(stderr,
              "REX_TOKEN_INVARIANT[structural-sibling]: parent=%p/%s typed "
              "tag=%p occurs %zu times and has parent=%p\n",
              static_cast<void *>(parent), ownerType,
              static_cast<void *>(declaration), occurrences,
              static_cast<void *>(declaration != nullptr
                                      ? declaration->get_parent()
                                      : nullptr));
      ROSE_ABORT();
    }
    result.push_back(declaration);
    return result;
  };

  // A tag spelled directly in a cast/sizeof/alignof type-id is owned by the
  // expression's typed declaration edge, not by a lexical scope list.  That
  // exact singleton edge is the complete declaration sequence for token
  // grouping under the expression.
  if (SgCastExp *cast = isSgCastExp(parent)) {
    return appendTypedExpressionTag(cast->get_type_defining_declaration(),
                                    "SgCastExp");
  }
  if (SgSizeOfOp *sizeOf = isSgSizeOfOp(parent)) {
    return appendTypedExpressionTag(sizeOf->get_type_defining_declaration(),
                                    "SgSizeOfOp");
  }
  if (SgAlignOfOp *alignOf = isSgAlignOfOp(parent)) {
    return appendTypedExpressionTag(alignOf->get_type_defining_declaration(),
                                    "SgAlignOfOp");
  }

  SgScopeStatement *scope = isSgScopeStatement(parent);
  if (scope == nullptr) {
    if (SgAuxiliaryDeclarationList *auxiliary =
            isSgAuxiliaryDeclarationList(parent)) {
      SgScopeStatement *owner = isSgScopeStatement(auxiliary->get_parent());
      fprintf(stderr,
              "REX_TOKEN_INVARIANT[structural-sibling-detail]: auxiliary=%p "
              "owner=%p/%s owner-edge=%d declarations=%zu\n",
              static_cast<void *>(auxiliary), static_cast<void *>(owner),
              owner != nullptr ? owner->class_name().c_str() : "<null>",
              owner != nullptr &&
                  owner->get_auxiliary_declarations() == auxiliary,
              auxiliary->get_declarations().size());
      size_t physicalOutputCount = 0;
      SgDeclarationStatement *firstPhysicalOutput = nullptr;
      for (SgDeclarationStatement *declaration :
           auxiliary->get_declarations()) {
        Sg_File_Info *info =
            declaration != nullptr ? declaration->get_file_info() : nullptr;
        if (info != nullptr && info->isOutputInCodeGeneration() &&
            !info->isCompilerGenerated() && !info->isFrontendSpecific() &&
            !info->isTransformation() && info->get_physical_file_id() >= 0) {
          ++physicalOutputCount;
          if (firstPhysicalOutput == nullptr) {
            firstPhysicalOutput = declaration;
          }
        }
      }
      Sg_File_Info *firstInfo = firstPhysicalOutput != nullptr
                                    ? firstPhysicalOutput->get_file_info()
                                    : nullptr;
      fprintf(stderr,
              "  physical-output-declarations=%zu first=%p/%s "
              "file/physical=%d/%d\n",
              physicalOutputCount, static_cast<void *>(firstPhysicalOutput),
              firstPhysicalOutput != nullptr
                  ? firstPhysicalOutput->class_name().c_str()
                  : "<null>",
              firstInfo != nullptr ? firstInfo->get_file_id()
                                   : Sg_File_Info::BAD_FILE_ID,
              firstInfo != nullptr ? firstInfo->get_physical_file_id()
                                   : Sg_File_Info::BAD_FILE_ID);
    }
    fprintf(stderr,
            "REX_TOKEN_INVARIANT[structural-sibling]: declaration sequence "
            "parent=%p/%s is neither an SgScopeStatement nor an "
            "SgForInitStatement\n",
            static_cast<void *>(parent), parent->class_name().c_str());
    ROSE_ABORT();
  }

  if (scope->containsOnlyDeclarations() || isSgDeclarationScope(scope)) {
    for (SgDeclarationStatement *declaration : scope->getDeclarationList()) {
      if (declaration == nullptr) {
        fprintf(stderr,
                "REX_TOKEN_INVARIANT[structural-sibling]: parent=%p/%s owns a "
                "null declaration\n",
                static_cast<void *>(parent), parent->class_name().c_str());
        ROSE_ABORT();
      }
      result.push_back(declaration);
    }
  } else {
    for (SgStatement *statement : scope->getStatementList()) {
      appendStatement(statement);
    }
  }
  return result;
}

std::vector<DirectDeclarationSequence>
sourceDeclarationSequences(SgSourceFile *sourceFile) {
  ASSERT_not_null(sourceFile);
  SgGlobal *global = sourceFile->get_globalScope();
  if (global == nullptr) {
    fprintf(stderr,
            "REX_TOKEN_INVARIANT[declaration-mapping]: file=%s has no global "
            "scope\n",
            sourceFile->getFileName().c_str());
    ROSE_ABORT();
  }

  std::map<SgNode *, DirectDeclarationSequence> sequencesByParent;
  for (SgNode *node :
       NodeQuery::querySubTree(global, V_SgDeclarationStatement)) {
    SgDeclarationStatement *declaration = isSgDeclarationStatement(node);
    ASSERT_not_null(declaration);
    if (!declarationRequiresTokenMapping(declaration, sourceFile)) {
      continue;
    }

    SgNode *parent = declaration->get_parent();
    if (parent == nullptr) {
      fprintf(stderr,
              "REX_TOKEN_INVARIANT[structural-sibling]: declaration=%p/%s "
              "has no structural parent\n",
              static_cast<void *>(declaration),
              declaration->class_name().c_str());
      ROSE_ABORT();
    }
    auto inserted =
        sequencesByParent.emplace(parent, DirectDeclarationSequence());
    if (inserted.second) {
      inserted.first->second = directDeclarationSequence(parent);
    }
    const size_t occurrences = static_cast<size_t>(
        std::count(inserted.first->second.begin(), inserted.first->second.end(),
                   declaration));
    if (occurrences != 1) {
      fprintf(stderr,
              "REX_TOKEN_INVARIANT[structural-sibling]: declaration=%p/%s "
              "occurs %zu times in parent=%p/%s direct sequence\n",
              static_cast<void *>(declaration),
              declaration->class_name().c_str(), occurrences,
              static_cast<void *>(parent), parent->class_name().c_str());
      ROSE_ABORT();
    }
  }

  std::vector<DirectDeclarationSequence> result;
  result.reserve(sequencesByParent.size());
  for (auto &entry : sequencesByParent) {
    result.push_back(std::move(entry.second));
  }
  return result;
}

bool statementTokenIntervalShouldClaimTrailingSemicolon(SgStatement *stmt) {
  ASSERT_not_null(stmt);
  if (SgEmptyDeclaration *empty = isSgEmptyDeclaration(stmt)) {
    empty->validate_lexical_role();
    return empty->get_lexical_role() ==
           SgEmptyDeclaration::e_empty_declaration_source_semicolon;
  }
  if (isSgClassDeclaration(stmt) != nullptr ||
      isSgEnumDeclaration(stmt) != nullptr ||
      isSgVariableDeclaration(stmt) != nullptr ||
      isSgTypedefDeclaration(stmt) != nullptr ||
      isSgExprStatement(stmt) != nullptr || isSgReturnStmt(stmt) != nullptr ||
      isSgBreakStmt(stmt) != nullptr || isSgContinueStmt(stmt) != nullptr ||
      isSgGotoStatement(stmt) != nullptr || isSgAsmStmt(stmt) != nullptr) {
    return true;
  }
  if (SgFunctionDeclaration *functionDecl = isSgFunctionDeclaration(stmt)) {
    return functionDecl->get_definition() == nullptr;
  }
  return false;
}

bool followingEmptyDeclarationOwnsSourcePosition(SgStatement *statement,
                                                 int line, int column) {
  ASSERT_not_null(statement);
  SgStatement *next = nullptr;
  SgNode *parent = statement->get_parent();
  if (SgForInitStatement *forInit = isSgForInitStatement(parent)) {
    const SgStatementPtrList &statements = forInit->get_init_stmt();
    auto current = std::find(statements.begin(), statements.end(), statement);
    if (current == statements.end()) {
      fprintf(stderr,
              "REX_TOKEN_INVARIANT[structural-sibling]: statement=%p "
              "type=%s is not owned by its SgForInitStatement parent=%p\n",
              static_cast<void *>(statement), statement->class_name().c_str(),
              static_cast<void *>(forInit));
      ROSE_ABORT();
    }
    ++current;
    next = current != statements.end() ? *current : nullptr;
  } else if (SgScopeStatement *scope = isSgScopeStatement(parent)) {
    if (scope->containsOnlyDeclarations() || isSgDeclarationScope(scope)) {
      const SgDeclarationStatementPtrList &declarations =
          scope->getDeclarationList();
      auto current = std::find(declarations.begin(), declarations.end(),
                               isSgDeclarationStatement(statement));
      if (current == declarations.end()) {
        fprintf(stderr,
                "REX_TOKEN_INVARIANT[structural-sibling]: statement=%p "
                "type=%s is not owned by its declaration-list parent=%p/%s\n",
                static_cast<void *>(statement), statement->class_name().c_str(),
                static_cast<void *>(scope), scope->class_name().c_str());
        ROSE_ABORT();
      }
      ++current;
      next = current != declarations.end() ? *current : nullptr;
    } else {
      const SgStatementPtrList &statements = scope->getStatementList();
      auto current = std::find(statements.begin(), statements.end(), statement);
      if (current == statements.end()) {
        fprintf(stderr,
                "REX_TOKEN_INVARIANT[structural-sibling]: statement=%p "
                "type=%s is not owned by its statement-list parent=%p/%s\n",
                static_cast<void *>(statement), statement->class_name().c_str(),
                static_cast<void *>(scope), scope->class_name().c_str());
        ROSE_ABORT();
      }
      ++current;
      next = current != statements.end() ? *current : nullptr;
    }
  }
  SgEmptyDeclaration *empty = isSgEmptyDeclaration(next);
  if (empty == nullptr || empty->get_parent() != statement->get_parent()) {
    return false;
  }
  empty->validate_lexical_role();
  if (empty->get_lexical_role() !=
      SgEmptyDeclaration::e_empty_declaration_source_semicolon) {
    return false;
  }

  Sg_File_Info *start = empty->get_startOfConstruct();
  if (start == nullptr || start->get_physical_line() <= 0 ||
      start->get_col() < 0) {
    fprintf(stderr,
            "REX_TOKEN_INVARIANT[empty-declaration]: declaration=%p has no "
            "exact physical source position\n",
            static_cast<void *>(empty));
    ROSE_ABORT();
  }
  return start->get_physical_line() == line && start->get_col() == column;
}

void finalizeDeclarationTokenMappingsForSequence(
    SgSourceFile *sourceFile, const std::vector<stream_element *> &tokenVector,
    const DirectDeclarationSequence &declarations,
    std::map<SgNode *, TokenStreamSequenceToNodeMapping *> &tokenMap) {
  ASSERT_not_null(sourceFile);
  for (size_t index = 0; index < declarations.size(); ++index) {
    SgDeclarationStatement *decl = declarations[index];
    if (decl == nullptr) {
      continue;
    }
    if (!declarationRequiresTokenMapping(decl, sourceFile)) {
      continue;
    }

    auto existing = tokenMap.find(decl);
    if (existing != tokenMap.end()) {
      if (existing->second == nullptr) {
        fprintf(stderr,
                "REX_TOKEN_INVARIANT[declaration-mapping]: declaration=%p "
                "type=%s has a null token mapping\n",
                static_cast<void *>(decl), decl->class_name().c_str());
        ROSE_ABORT();
      }
      continue;
    }

    if (tokenVector.empty()) {
      fprintf(stderr,
              "REX_TOKEN_INVARIANT[declaration-mapping]: declaration=%p "
              "type=%s requires tokens in empty file=%s\n",
              static_cast<void *>(decl), decl->class_name().c_str(),
              sourceFile->getFileName().c_str());
      ROSE_ABORT();
    }
    Sg_File_Info *start = decl->get_startOfConstruct();
    Sg_File_Info *end = decl->get_endOfConstruct();
    if (start == nullptr || end == nullptr || start->isCompilerGenerated() ||
        end->isCompilerGenerated() || start->isFrontendSpecific() ||
        end->isFrontendSpecific() || start->isTransformation() ||
        end->isTransformation() || !start->isSameFile(sourceFile) ||
        !end->isSameFile(sourceFile) || start->get_physical_line() <= 0 ||
        end->get_physical_line() <= 0 || start->get_col() < 0 ||
        end->get_col() < 0) {
      fprintf(stderr,
              "REX_TOKEN_INVARIANT[declaration-mapping]: declaration=%p "
              "type=%s has an incomplete physical source range\n",
              static_cast<void *>(decl), decl->class_name().c_str());
      ROSE_ABORT();
    }

    int lowerBound = 0;
    SgDeclarationStatement *lowerBoundOwner = nullptr;
    for (size_t previous = index; previous-- > 0;) {
      auto previousMapping = tokenMap.find(declarations[previous]);
      if (previousMapping != tokenMap.end() &&
          previousMapping->second != nullptr &&
          previousMapping->second
                      ->constructionState(
                          TokenStreamMappingConstructionAccess::key())
                      .tokenSubsequence()
                      .end -
                  1 >=
              0) {
        lowerBound =
            previousMapping->second
                ->constructionState(TokenStreamMappingConstructionAccess::key())
                .tokenSubsequence()
                .end -
            1 + 1;
        lowerBoundOwner = declarations[previous];
        break;
      }
    }
    int upperBound = static_cast<int>(tokenVector.size()) - 1;
    SgDeclarationStatement *upperBoundOwner = nullptr;
    for (size_t next = index + 1; next < declarations.size(); ++next) {
      auto nextMapping = tokenMap.find(declarations[next]);
      if (nextMapping != tokenMap.end() && nextMapping->second != nullptr &&
          nextMapping->second
                  ->constructionState(
                      TokenStreamMappingConstructionAccess::key())
                  .tokenSubsequence()
                  .begin >= 0) {
        upperBound =
            nextMapping->second
                ->constructionState(TokenStreamMappingConstructionAccess::key())
                .tokenSubsequence()
                .begin -
            1;
        upperBoundOwner = declarations[next];
        break;
      }
    }
    if (lowerBound > upperBound) {
      fprintf(stderr,
              "REX_TOKEN_INVARIANT[declaration-mapping]: declaration=%p "
              "type=%s name=%s source=%d:%d-%d:%d has inverted neighboring "
              "token bounds [%d,%d] lower-owner=%p/%s upper-owner=%p/%s\n",
              static_cast<void *>(decl), decl->class_name().c_str(),
              SageInterface::get_name(decl).c_str(), start->get_physical_line(),
              start->get_col(), end->get_physical_line(), end->get_col(),
              lowerBound, upperBound, static_cast<void *>(lowerBoundOwner),
              lowerBoundOwner != nullptr ? lowerBoundOwner->class_name().c_str()
                                         : "<none>",
              static_cast<void *>(upperBoundOwner),
              upperBoundOwner != nullptr ? upperBoundOwner->class_name().c_str()
                                         : "<none>");
      ROSE_ABORT();
    }

    const int startIndex = findFirstTokenAtOrAfterSourcePosition(
        tokenVector, lowerBound, upperBound, start->get_physical_line(),
        start->get_col());
    int endIndex = findLastTokenAtOrBeforeSourcePosition(
        tokenVector, std::max(startIndex, lowerBound), upperBound,
        end->get_physical_line(), end->get_col());
    if (startIndex < 0 || endIndex < startIndex) {
      const char *autonomousState = "not-applicable";
      if (SgClassDeclaration *classDeclaration = isSgClassDeclaration(decl)) {
        autonomousState =
            classDeclaration->get_isAutonomousDeclaration() ? "true" : "false";
      } else if (SgEnumDeclaration *enumDeclaration =
                     isSgEnumDeclaration(decl)) {
        autonomousState =
            enumDeclaration->get_isAutonomousDeclaration() ? "true" : "false";
      }
      fprintf(stderr,
              "REX_TOKEN_INVARIANT[declaration-mapping]: declaration=%p "
              "type=%s name=%s parent=%p/%s scope=%p/%s autonomous=%s "
              "source=%d:%d-%d:%d bounds=[%d,%d] "
              "start-index=%d end-index=%d source range has no exact token "
              "interval\n",
              static_cast<void *>(decl), decl->class_name().c_str(),
              SageInterface::get_name(decl).c_str(),
              static_cast<void *>(decl->get_parent()),
              decl->get_parent() != nullptr
                  ? decl->get_parent()->class_name().c_str()
                  : "<null>",
              static_cast<void *>(decl->get_scope()),
              decl->get_scope() != nullptr
                  ? decl->get_scope()->class_name().c_str()
                  : "<null>",
              autonomousState, start->get_physical_line(), start->get_col(),
              end->get_physical_line(), end->get_col(), lowerBound, upperBound,
              startIndex, endIndex);
      for (int tokenIndex = lowerBound; tokenIndex <= upperBound;
           ++tokenIndex) {
        stream_element *token = tokenVector[tokenIndex];
        if (token == nullptr || token->p_tok_elem == nullptr) {
          fprintf(stderr, "  token[%d]=<incomplete>\n", tokenIndex);
          continue;
        }
        fprintf(stderr, "  token[%d] id=%d begin=%d:%d end=%d:%d lexeme=%s\n",
                tokenIndex, token->p_tok_elem->token_id,
                token->beginning_fpi.line_num, token->beginning_fpi.column_num,
                token->ending_fpi.line_num, token->ending_fpi.column_num,
                token->p_tok_elem->token_lexeme.c_str());
      }
      auto reportNeighbor = [&](const char *label,
                                SgDeclarationStatement *neighbor) {
        if (neighbor == nullptr) {
          fprintf(stderr, "  %s=<none>\n", label);
          return;
        }
        Sg_File_Info *neighborStart = neighbor->get_startOfConstruct();
        Sg_File_Info *neighborEnd = neighbor->get_endOfConstruct();
        auto neighborMapping = tokenMap.find(neighbor);
        if (neighborStart == nullptr || neighborEnd == nullptr) {
          fprintf(stderr, "  %s=%p/%s source=<incomplete>\n", label,
                  static_cast<void *>(neighbor),
                  neighbor->class_name().c_str());
          return;
        }
        if (neighborMapping == tokenMap.end() ||
            neighborMapping->second == nullptr) {
          fprintf(stderr, "  %s=%p/%s source=%d:%d-%d:%d mapping=<absent>\n",
                  label, static_cast<void *>(neighbor),
                  neighbor->class_name().c_str(),
                  neighborStart->get_physical_line(), neighborStart->get_col(),
                  neighborEnd->get_physical_line(), neighborEnd->get_col());
          return;
        }
        const auto &neighborCore =
            neighborMapping->second
                ->constructionState(TokenStreamMappingConstructionAccess::key())
                .tokenSubsequence();
        fprintf(stderr, "  %s=%p/%s source=%d:%d-%d:%d mapping=[%d,%d]\n",
                label, static_cast<void *>(neighbor),
                neighbor->class_name().c_str(),
                neighborStart->get_physical_line(), neighborStart->get_col(),
                neighborEnd->get_physical_line(), neighborEnd->get_col(),
                neighborCore.begin, neighborCore.end - 1);
        for (int tokenIndex = neighborCore.begin;
             tokenIndex >= 0 && tokenIndex < neighborCore.end &&
             tokenIndex < static_cast<int>(tokenVector.size());
             ++tokenIndex) {
          stream_element *token = tokenVector[tokenIndex];
          if (token == nullptr || token->p_tok_elem == nullptr) {
            continue;
          }
          fprintf(
              stderr, "    token[%d] id=%d begin=%d:%d end=%d:%d lexeme=%s\n",
              tokenIndex, token->p_tok_elem->token_id,
              token->beginning_fpi.line_num, token->beginning_fpi.column_num,
              token->ending_fpi.line_num, token->ending_fpi.column_num,
              token->p_tok_elem->token_lexeme.c_str());
        }
      };
      reportNeighbor("lower-owner", lowerBoundOwner);
      reportNeighbor("upper-owner", upperBoundOwner);
      ROSE_ABORT();
    }

    const bool macroEndedDeclaration =
        isSgDeclarationGroupStatement(decl) == nullptr &&
        decl->get_source_range_ends_in_macro_expansion();
    bool followingEmptyDeclarationOwnsSemicolon = false;
    if (macroEndedDeclaration && index + 1 < declarations.size()) {
      SgEmptyDeclaration *emptyDecl =
          isSgEmptyDeclaration(declarations[index + 1]);
      if (emptyDecl != nullptr) {
        emptyDecl->validate_lexical_role();
      }
      if (emptyDecl != nullptr &&
          emptyDecl->get_lexical_role() ==
              SgEmptyDeclaration::e_empty_declaration_source_semicolon &&
          emptyDecl->get_startOfConstruct() != nullptr) {
        const int candidateSemicolon = findTrailingSemicolonTokenIndex(
            tokenVector, endIndex + 1, upperBound);
        if (candidateSemicolon >= 0) {
          stream_element *semicolonToken = tokenVector[candidateSemicolon];
          Sg_File_Info *emptyStart = emptyDecl->get_startOfConstruct();
          followingEmptyDeclarationOwnsSemicolon =
              semicolonToken != nullptr &&
              semicolonToken->beginning_fpi.line_num ==
                  emptyStart->get_physical_line() &&
              semicolonToken->beginning_fpi.column_num == emptyStart->get_col();
        }
      }
    }

    if (statementTokenIntervalShouldClaimTrailingSemicolon(decl) &&
        !followingEmptyDeclarationOwnsSemicolon &&
        (tokenVector[endIndex] == nullptr ||
         tokenVector[endIndex]->p_tok_elem == nullptr ||
         tokenVector[endIndex]->p_tok_elem->token_lexeme != ";")) {
      const int semicolonIndex = findTrailingSemicolonTokenIndex(
          tokenVector, endIndex + 1, upperBound);
      if (semicolonIndex < 0) {
        if (!macroEndedDeclaration) {
          fprintf(stderr,
                  "REX_TOKEN_INVARIANT[declaration-mapping]: declaration=%p "
                  "type=%s has no required trailing semicolon token\n",
                  static_cast<void *>(decl), decl->class_name().c_str());
          ROSE_ABORT();
        }
      } else {
        endIndex = semicolonIndex;
      }
    }

    TokenStreamSequenceToNodeMapping *mapping =
        TokenStreamMappingConstructionAccess::createTokenInterval(
            sourceFile, decl, tokenVector.size(),
            TokenStreamMappingConstructionAccess::requiredInclusiveInterval(
                decl, "token-subsequence", startIndex, endIndex));
    if (mapping == nullptr || !tokenMap.insert({decl, mapping}).second) {
      fprintf(stderr,
              "REX_TOKEN_INVARIANT[declaration-mapping]: declaration=%p "
              "type=%s could not install its token mapping\n",
              static_cast<void *>(decl), decl->class_name().c_str());
      ROSE_ABORT();
    }
  }
}

void finalizeDeclarationTokenMappings(
    SgSourceFile *sourceFile, const std::vector<stream_element *> &tokenVector,
    std::map<SgNode *, TokenStreamSequenceToNodeMapping *> &tokenMap) {
  ASSERT_not_null(sourceFile);
  // Construction classifies declaration-derived semantic and structural nodes
  // before publishing token mappings.  Reaching this boundary with one of
  // those nodes in the map is an internal ownership error, not state to repair.
  for (SgNode *node : NodeQuery::querySubTree(sourceFile->get_globalScope(),
                                              V_SgDeclarationStatement)) {
    SgDeclarationStatement *declaration = isSgDeclarationStatement(node);
    ASSERT_not_null(declaration);
    if (!declarationRequiresTokenMapping(declaration, sourceFile)) {
      auto unexpected = tokenMap.find(declaration);
      if (unexpected != tokenMap.end()) {
        fprintf(stderr,
                "REX_TOKEN_INVARIANT[nonlexical-declaration-mapping]: "
                "declaration=%p/%s file=%s acquired a direct token mapping\n",
                static_cast<void *>(declaration),
                declaration->class_name().c_str(),
                sourceFile->getFileName().c_str());
        ROSE_ABORT();
      }
    }
  }
  for (const DirectDeclarationSequence &sequence :
       sourceDeclarationSequences(sourceFile)) {
    finalizeDeclarationTokenMappingsForSequence(sourceFile, tokenVector,
                                                sequence, tokenMap);
  }
}

bool tokenIsTrivia(const SgToken *token) {
  if (token == nullptr) {
    return false;
  }
  const int classification = token->get_classification_code();
  return classification == ROSE_token_ids::C_CXX_WHITESPACE ||
         classification == ROSE_token_ids::C_CXX_PREPROCESSING_INFO ||
         classification == ROSE_token_ids::C_CXX_COMMENTS;
}

void finalizeStatementTokenMappingBoundaries(
    SgSourceFile *sourceFile,
    std::map<SgNode *, TokenStreamSequenceToNodeMapping *> &tokenMap) {
  ASSERT_not_null(sourceFile);
  SgTokenPtrList &tokens = sourceFile->get_token_list();
  const int tokenCount = static_cast<int>(tokens.size());
  std::set<TokenStreamSequenceToNodeMapping *> finalizedMappings;

  for (const auto &entry : tokenMap) {
    SgStatement *statement = isSgStatement(entry.first);
    TokenStreamSequenceToNodeMapping *mapping = entry.second;
    if (statement == nullptr || mapping == nullptr ||
        !finalizedMappings.insert(mapping).second ||
        mapping->constructionState(TokenStreamMappingConstructionAccess::key())
                .tokenSubsequence()
                .begin < 0 ||
        mapping->constructionState(TokenStreamMappingConstructionAccess::key())
                    .tokenSubsequence()
                    .end -
                1 <
            mapping
                ->constructionState(TokenStreamMappingConstructionAccess::key())
                .tokenSubsequence()
                .begin ||
        mapping->constructionState(TokenStreamMappingConstructionAccess::key())
                    .tokenSubsequence()
                    .end -
                1 >=
            tokenCount) {
      continue;
    }

    const bool macroEndedDeclaration =
        isSgDeclarationGroupStatement(statement) == nullptr &&
        statement->get_source_range_ends_in_macro_expansion();
    if (macroEndedDeclaration &&
        tokens[mapping
                   ->constructionState(
                       TokenStreamMappingConstructionAccess::key())
                   .tokenSubsequence()
                   .end -
               1] != nullptr &&
        tokens[mapping
                   ->constructionState(
                       TokenStreamMappingConstructionAccess::key())
                   .tokenSubsequence()
                   .end -
               1]
                ->get_lexeme_string() == ";") {
      Sg_File_Info *semicolonStart =
          tokens[mapping
                     ->constructionState(
                         TokenStreamMappingConstructionAccess::key())
                     .tokenSubsequence()
                     .end -
                 1]
              ->get_startOfConstruct();
      if (semicolonStart == nullptr) {
        fprintf(stderr,
                "REX_TOKEN_INVARIANT[semicolon-owner]: mapped semicolon has "
                "no source position\n");
        ROSE_ABORT();
      }
      if (followingEmptyDeclarationOwnsSourcePosition(
              statement, semicolonStart->get_line(),
              semicolonStart->get_col())) {
        Sg_File_Info *statementEnd = statement->get_endOfConstruct();
        if (statementEnd == nullptr) {
          fprintf(stderr,
                  "REX_TOKEN_INVARIANT[semicolon-owner]: macro-ended "
                  "statement=%p has no source end\n",
                  static_cast<void *>(statement));
          ROSE_ABORT();
        }
        int exactEnd =
            mapping
                ->constructionState(TokenStreamMappingConstructionAccess::key())
                .tokenSubsequence()
                .end -
            1 - 1;
        while (exactEnd >= mapping
                               ->constructionState(
                                   TokenStreamMappingConstructionAccess::key())
                               .tokenSubsequence()
                               .begin &&
               tokens[exactEnd] != nullptr &&
               (tokens[exactEnd]->get_classification_code() ==
                    ROSE_token_ids::C_CXX_WHITESPACE ||
                tokens[exactEnd]->get_classification_code() ==
                    ROSE_token_ids::C_CXX_COMMENTS)) {
          --exactEnd;
        }
        if (exactEnd < mapping
                           ->constructionState(
                               TokenStreamMappingConstructionAccess::key())
                           .tokenSubsequence()
                           .begin ||
            tokens[exactEnd] == nullptr ||
            tokens[exactEnd]->get_endOfConstruct() == nullptr ||
            tokens[exactEnd]->get_endOfConstruct()->get_line() !=
                statementEnd->get_line() ||
            tokens[exactEnd]->get_endOfConstruct()->get_col() !=
                statementEnd->get_col()) {
          fprintf(stderr,
                  "REX_TOKEN_INVARIANT[semicolon-owner]: macro-ended "
                  "statement=%p cannot relinquish the distinct empty "
                  "declaration's semicolon at token=%d\n",
                  static_cast<void *>(statement),
                  mapping->constructionState(
                             TokenStreamMappingConstructionAccess::key())
                          .tokenSubsequence()
                          .end -
                      1);
          ROSE_ABORT();
        }
        const TokenStreamHalfOpenInterval current =
            mapping
                ->constructionState(TokenStreamMappingConstructionAccess::key())
                .tokenSubsequence();
        mapping->constructionState(TokenStreamMappingConstructionAccess::key())
            .replaceTokenSubsequence(
                TokenStreamHalfOpenInterval(current.begin, exactEnd + 1));
      }
    }

    if (statementTokenIntervalShouldClaimTrailingSemicolon(statement) &&
        tokens[mapping
                   ->constructionState(
                       TokenStreamMappingConstructionAccess::key())
                   .tokenSubsequence()
                   .end -
               1] != nullptr &&
        tokens[mapping
                   ->constructionState(
                       TokenStreamMappingConstructionAccess::key())
                   .tokenSubsequence()
                   .end -
               1]
                ->get_lexeme_string() != ";") {
      int semicolon =
          mapping
              ->constructionState(TokenStreamMappingConstructionAccess::key())
              .tokenSubsequence()
              .end -
          1 + 1;
      while (semicolon < tokenCount && tokens[semicolon] != nullptr &&
             (tokens[semicolon]->get_classification_code() ==
                  ROSE_token_ids::C_CXX_WHITESPACE ||
              tokens[semicolon]->get_classification_code() ==
                  ROSE_token_ids::C_CXX_COMMENTS)) {
        ++semicolon;
      }
      if (semicolon < tokenCount && tokens[semicolon] != nullptr &&
          tokens[semicolon]->get_lexeme_string() == ";") {
        Sg_File_Info *semicolonStart =
            tokens[semicolon]->get_startOfConstruct();
        if (semicolonStart == nullptr) {
          fprintf(stderr,
                  "REX_TOKEN_INVARIANT[semicolon-owner]: token=%d has no "
                  "source position\n",
                  semicolon);
          ROSE_ABORT();
        }
        if (!macroEndedDeclaration ||
            !followingEmptyDeclarationOwnsSourcePosition(
                statement, semicolonStart->get_line(),
                semicolonStart->get_col())) {
          const TokenStreamHalfOpenInterval current =
              mapping
                  ->constructionState(
                      TokenStreamMappingConstructionAccess::key())
                  .tokenSubsequence();
          mapping
              ->constructionState(TokenStreamMappingConstructionAccess::key())
              .replaceTokenSubsequence(
                  TokenStreamHalfOpenInterval(current.begin, semicolon + 1));
        }
      }
    }

    auto &draft =
        mapping->constructionState(TokenStreamMappingConstructionAccess::key());
    draft.replaceTrailingWhitespace(std::nullopt);
    int trailing = draft.tokenSubsequence().end;
    if (trailing < tokenCount && tokenIsTrivia(tokens[trailing])) {
      int trailingEnd = trailing;
      while (trailingEnd + 1 < tokenCount &&
             tokenIsTrivia(tokens[trailingEnd + 1])) {
        ++trailingEnd;
      }
      draft.replaceTrailingWhitespace(
          TokenStreamHalfOpenInterval(trailing, trailingEnd + 1));
    }
  }

  // Core intervals are now final.  Remove any leading interval that still
  // overlaps the exact previous sibling's core.  This matters for constructs
  // such as pragmas whose initial source range names only the introducer and
  // whose core is extended above through the end of its physical line.
  for (const auto &entry : tokenMap) {
    SgStatement *statement = isSgStatement(entry.first);
    TokenStreamSequenceToNodeMapping *mapping = entry.second;
    if (statement == nullptr || mapping == nullptr ||
        !mapping->constructionState(TokenStreamMappingConstructionAccess::key())
             .leadingWhitespace()
             .has_value()) {
      continue;
    }

    SgScopeStatement *parentScope = isSgScopeStatement(statement->get_parent());
    if (parentScope == nullptr) {
      continue;
    }

    SgStatement *previous = nullptr;
    if (parentScope->containsOnlyDeclarations()) {
      SgDeclarationStatement *declaration = isSgDeclarationStatement(statement);
      if (declaration == nullptr) {
        continue;
      }
      SgDeclarationStatementPtrList &declarations =
          parentScope->getDeclarationList();
      auto position =
          std::find(declarations.begin(), declarations.end(), declaration);
      if (position == declarations.end() || position == declarations.begin()) {
        continue;
      }
      previous = *std::prev(position);
    } else if (SgBasicBlock *parentBlock = isSgBasicBlock(parentScope)) {
      SgStatementPtrList &statements = parentBlock->get_statements();
      auto position =
          std::find(statements.begin(), statements.end(), statement);
      if (position == statements.end() || position == statements.begin()) {
        continue;
      }
      previous = *std::prev(position);
    } else {
      // Control statements are scopes in Sage, but their condition and body
      // children are distinct grammar fields rather than members of one
      // lexical statement sequence.  They therefore have no previous sibling
      // whose core interval can constrain this leading interval.
      continue;
    }
    ASSERT_not_null(previous);

    auto previousEntry = tokenMap.find(previous);
    if (previousEntry == tokenMap.end() || previousEntry->second == nullptr ||
        previousEntry->second == mapping) {
      continue;
    }

    const int firstUnownedToken =
        previousEntry->second
            ->constructionState(TokenStreamMappingConstructionAccess::key())
            .tokenSubsequence()
            .end -
        1 + 1;
    auto &draft =
        mapping->constructionState(TokenStreamMappingConstructionAccess::key());
    if (draft.leadingWhitespace()->begin < firstUnownedToken) {
      if (firstUnownedToken >= draft.leadingWhitespace()->end) {
        draft.replaceLeadingWhitespace(std::nullopt);
      } else {
        draft.replaceLeadingWhitespace(TokenStreamHalfOpenInterval(
            firstUnownedToken, draft.leadingWhitespace()->end));
      }
    }
  }
}

void validateDeclarationGroupTokenMappings(
    SgSourceFile *sourceFile,
    std::map<SgNode *, TokenStreamSequenceToNodeMapping *> &tokenMap,
    bool intervalsPublished) {
  ASSERT_not_null(sourceFile);
  SgGlobal *global = sourceFile->get_globalScope();
  if (global == nullptr) {
    fprintf(stderr,
            "REX_TOKEN_INVARIANT[declaration-group]: file=%s has no global "
            "scope\n",
            sourceFile->getFileName().c_str());
    ROSE_ABORT();
  }

  std::set<SgDeclarationStatement *> groupMembers;
  std::vector<SgDeclarationGroupStatement *> requiredGroups;
  for (SgNode *node :
       NodeQuery::querySubTree(global, V_SgDeclarationGroupStatement)) {
    SgDeclarationGroupStatement *group = isSgDeclarationGroupStatement(node);
    ASSERT_not_null(group);
    group->validate();
    for (SgDeclarationStatement *member : group->get_declarations()) {
      ASSERT_not_null(member);
      if (member->get_parent() != group ||
          !groupMembers.insert(member).second) {
        fprintf(stderr,
                "REX_TOKEN_INVARIANT[declaration-group]: group=%p member=%p/%s "
                "does not have one exclusive structural owner\n",
                static_cast<void *>(group), static_cast<void *>(member),
                member->class_name().c_str());
        ROSE_ABORT();
      }
    }
    if (declarationRequiresTokenMapping(group, sourceFile)) {
      requiredGroups.push_back(group);
    }
  }

  for (SgDeclarationStatement *member : groupMembers) {
    auto direct = tokenMap.find(member);
    if (direct != tokenMap.end()) {
      SgDeclarationGroupStatement *group = declarationGroupOwner(member);
      ASSERT_not_null(group);
      fprintf(stderr,
              "REX_TOKEN_INVARIANT[declaration-group]: member=%p/%s group=%p "
              "has a forbidden direct token mapping\n",
              static_cast<void *>(member), member->class_name().c_str(),
              static_cast<void *>(group));
      ROSE_ABORT();
    }
  }

  std::set<TokenStreamSequenceToNodeMapping *> inspectedMappings;
  for (const auto &entry : tokenMap) {
    TokenStreamSequenceToNodeMapping *mapping = entry.second;
    if (mapping == nullptr || !inspectedMappings.insert(mapping).second) {
      continue;
    }
    if (SgDeclarationStatement *owner =
            isSgDeclarationStatement(mapping->node)) {
      if (groupMembers.find(owner) != groupMembers.end()) {
        SgDeclarationGroupStatement *group = declarationGroupOwner(owner);
        ASSERT_not_null(group);
        fprintf(stderr,
                "REX_TOKEN_INVARIANT[declaration-group]: mapping=%p uses "
                "member=%p/%s group=%p as its token-interval owner\n",
                static_cast<void *>(mapping), static_cast<void *>(owner),
                owner->class_name().c_str(), static_cast<void *>(group));
        ROSE_ABORT();
      }
    }
    for (SgNode *associatedNode : mapping->nodeVector) {
      SgDeclarationStatement *associated =
          isSgDeclarationStatement(associatedNode);
      if (associated == nullptr ||
          groupMembers.find(associated) == groupMembers.end()) {
        continue;
      }
      SgDeclarationGroupStatement *group = declarationGroupOwner(associated);
      ASSERT_not_null(group);
      fprintf(stderr,
              "REX_TOKEN_INVARIANT[declaration-group]: mapping=%p owner=%p/%s "
              "associates member=%p/%s structurally owned by group=%p\n",
              static_cast<void *>(mapping), static_cast<void *>(mapping->node),
              mapping->node != nullptr ? mapping->node->class_name().c_str()
                                       : "<null>",
              static_cast<void *>(associated), associated->class_name().c_str(),
              static_cast<void *>(group));
      ROSE_ABORT();
    }
  }

  const SgTokenPtrList &tokens = sourceFile->get_token_list();
  for (SgDeclarationGroupStatement *group : requiredGroups) {
    auto direct = tokenMap.find(group);
    if (direct == tokenMap.end() || direct->second == nullptr) {
      fprintf(stderr,
              "REX_TOKEN_INVARIANT[declaration-mapping]: declaration=%p/%s "
              "has no direct token mapping in file=%s\n",
              static_cast<void *>(group), group->class_name().c_str(),
              sourceFile->getFileName().c_str());
      ROSE_ABORT();
    }
    TokenStreamSequenceToNodeMapping *mapping = direct->second;
    if (mapping->node != group) {
      fprintf(stderr,
              "REX_TOKEN_INVARIANT[declaration-group]: group=%p direct token "
              "mapping owner=%p/%s is not the wrapper\n",
              static_cast<void *>(group), static_cast<void *>(mapping->node),
              mapping->node != nullptr ? mapping->node->class_name().c_str()
                                       : "<null>");
      ROSE_ABORT();
    }
    const size_t occurrences = static_cast<size_t>(std::count(
        mapping->nodeVector.begin(), mapping->nodeVector.end(), group));
    if (occurrences != 1) {
      fprintf(stderr,
              "REX_TOKEN_INVARIANT[declaration-mapping]: declaration=%p/%s "
              "occurs %zu times in its direct token mapping node vector\n",
              static_cast<void *>(group), group->class_name().c_str(),
              occurrences);
      ROSE_ABORT();
    }
    const TokenStreamHalfOpenInterval &core =
        intervalsPublished
            ? mapping->halfOpenInterval(
                  TokenStreamIntervalKind::token_subsequence)
            : mapping
                  ->constructionState(
                      TokenStreamMappingConstructionAccess::key())
                  .tokenSubsequence();
    const int start = core.begin;
    const int end = core.end - 1;
    if (start < 0 || end < start || static_cast<size_t>(end) >= tokens.size() ||
        tokens[start] == nullptr || tokens[end] == nullptr) {
      fprintf(stderr,
              "REX_TOKEN_INVARIANT[declaration-group]: group=%p token mapping "
              "has invalid exact-source interval=[%d,%d]\n",
              static_cast<void *>(group), start, end);
      ROSE_ABORT();
    }
    Sg_File_Info *groupStart = group->get_startOfConstruct();
    Sg_File_Info *groupEnd = group->get_endOfConstruct();
    Sg_File_Info *tokenStart = tokens[start]->get_startOfConstruct();
    Sg_File_Info *tokenEnd = tokens[end]->get_endOfConstruct();
    if (groupStart == nullptr || groupEnd == nullptr || tokenStart == nullptr ||
        tokenEnd == nullptr) {
      fprintf(stderr,
              "REX_TOKEN_INVARIANT[declaration-group]: group=%p has an "
              "incomplete source or mapped-token range\n",
              static_cast<void *>(group));
      ROSE_ABORT();
    }
    const bool exactStart =
        groupStart->isSameFile(tokenStart) &&
        groupStart->get_physical_line() == tokenStart->get_physical_line() &&
        groupStart->get_col() == tokenStart->get_col();
    const bool exactEnd =
        groupEnd->isSameFile(tokenEnd) &&
        groupEnd->get_physical_line() == tokenEnd->get_physical_line() &&
        groupEnd->get_col() == tokenEnd->get_col();
    if (!exactStart || !exactEnd) {
      fprintf(stderr,
              "REX_TOKEN_INVARIANT[declaration-group]: group=%p source="
              "%d:%d-%d:%d does not exactly match mapped tokens="
              "%d:%d-%d:%d\n",
              static_cast<void *>(group), groupStart->get_physical_line(),
              groupStart->get_col(), groupEnd->get_physical_line(),
              groupEnd->get_col(), tokenStart->get_physical_line(),
              tokenStart->get_col(), tokenEnd->get_physical_line(),
              tokenEnd->get_col());
      ROSE_ABORT();
    }

    if (group->get_source_terminator() ==
        SgDeclarationGroupStatement::e_source_terminator_macro_semicolon) {
      if (tokens[end]->get_lexeme_string() == ";") {
        fprintf(stderr,
                "REX_TOKEN_INVARIANT[declaration-group]: macro-terminated "
                "group=%p incorrectly owns a file semicolon token=%d\n",
                static_cast<void *>(group), end);
        ROSE_ABORT();
      }
      continue;
    }

    if (tokens[end]->get_lexeme_string() != ";") {
      fprintf(stderr,
              "REX_TOKEN_INVARIANT[declaration-group]: group=%p token mapping "
              "interval=[%d,%d] does not end at its required trailing "
              "semicolon\n",
              static_cast<void *>(group), start, end);
      ROSE_ABORT();
    }
    Sg_File_Info *semicolonStart = tokens[end]->get_startOfConstruct();
    ASSERT_not_null(semicolonStart);
    for (SgDeclarationStatement *member : group->get_declarations()) {
      Sg_File_Info *memberEnd = member->get_endOfConstruct();
      if (memberEnd == nullptr) {
        fprintf(stderr,
                "REX_TOKEN_INVARIANT[declaration-group]: group=%p "
                "member=%p/%s has no exact source end\n",
                static_cast<void *>(group), static_cast<void *>(member),
                member->class_name().c_str());
        ROSE_ABORT();
      }
      if (!memberEnd->isSameFile(semicolonStart) ||
          memberEnd->get_physical_line() >
              semicolonStart->get_physical_line() ||
          (memberEnd->get_physical_line() ==
               semicolonStart->get_physical_line() &&
           memberEnd->get_col() >= semicolonStart->get_col())) {
        fprintf(stderr,
                "REX_TOKEN_INVARIANT[declaration-group]: group=%p member=%p/%s "
                "source end=%d:%d claims the wrapper's trailing semicolon at "
                "%d:%d\n",
                static_cast<void *>(group), static_cast<void *>(member),
                member->class_name().c_str(), memberEnd->get_physical_line(),
                memberEnd->get_col(), semicolonStart->get_physical_line(),
                semicolonStart->get_col());
        ROSE_ABORT();
      }
    }
  }
}

void validateDeclarationTokenMappings(
    SgSourceFile *sourceFile,
    std::map<SgNode *, TokenStreamSequenceToNodeMapping *> &tokenMap,
    bool intervalsPublished = false) {
  ASSERT_not_null(sourceFile);
  validateDeclarationGroupTokenMappings(sourceFile, tokenMap,
                                        intervalsPublished);
  const std::vector<DirectDeclarationSequence> sequences =
      sourceDeclarationSequences(sourceFile);
  const SgTokenPtrList &tokens = sourceFile->get_token_list();

  // A declaration's map identity is direct and bidirectional.  Finding a
  // declaration in some mapping's nodeVector is not a substitute for the
  // declaration being a key in the source file's map.
  for (const DirectDeclarationSequence &sequence : sequences) {
    for (SgDeclarationStatement *declaration : sequence) {
      if (declaration == nullptr ||
          !declarationRequiresTokenMapping(declaration, sourceFile)) {
        continue;
      }
      auto direct = tokenMap.find(declaration);
      if (direct == tokenMap.end() || direct->second == nullptr) {
        fprintf(stderr,
                "REX_TOKEN_INVARIANT[declaration-mapping]: declaration=%p/%s "
                "has no direct token mapping in file=%s\n",
                static_cast<void *>(declaration),
                declaration->class_name().c_str(),
                sourceFile->getFileName().c_str());
        ROSE_ABORT();
      }

      SgFunctionDeclaration *function = isSgFunctionDeclaration(declaration);
      if (function != nullptr && function->get_definition() == nullptr) {
        Sg_File_Info *sourceStart = function->get_startOfConstruct();
        Sg_File_Info *sourceEnd = function->get_endOfConstruct();
        if (sourceStart == nullptr || sourceEnd == nullptr) {
          fprintf(stderr,
                  "REX_TOKEN_INVARIANT[function-prototype-boundary]: "
                  "declaration=%p name=%s has no exact source range\n",
                  static_cast<void *>(function),
                  function->get_name().getString().c_str());
          ROSE_ABORT();
        }
        const TokenStreamHalfOpenInterval &core =
            intervalsPublished
                ? direct->second->halfOpenInterval(
                      TokenStreamIntervalKind::token_subsequence)
                : direct->second
                      ->constructionState(
                          TokenStreamMappingConstructionAccess::key())
                      .tokenSubsequence();
        const int tokenStart = core.begin;
        const int tokenEnd = core.end - 1;
        if (tokenStart < 0 ||
            static_cast<size_t>(tokenStart) >= tokens.size() ||
            tokens[tokenStart] == nullptr ||
            tokens[tokenStart]->get_startOfConstruct() == nullptr ||
            tokenEnd < tokenStart ||
            static_cast<size_t>(tokenEnd) >= tokens.size() ||
            tokens[tokenEnd] == nullptr ||
            tokens[tokenEnd]->get_endOfConstruct() == nullptr) {
          fprintf(stderr,
                  "REX_TOKEN_INVARIANT[function-prototype-boundary]: "
                  "declaration=%p name=%s has an incomplete source or mapped "
                  "token range source=%d:%d-%d:%d tokens=[%d,%d]\n",
                  static_cast<void *>(function),
                  function->get_name().getString().c_str(),
                  sourceStart->get_physical_line(), sourceStart->get_col(),
                  sourceEnd->get_physical_line(), sourceEnd->get_col(),
                  tokenStart, tokenEnd);
          ROSE_ABORT();
        }

        Sg_File_Info *mappedStart = tokens[tokenStart]->get_startOfConstruct();
        Sg_File_Info *mappedEnd = tokens[tokenEnd]->get_endOfConstruct();
        const bool exactStart =
            sourceStart->isSameFile(mappedStart) &&
            sourceStart->get_physical_line() ==
                mappedStart->get_physical_line() &&
            sourceStart->get_col() == mappedStart->get_col();
        const bool exactEnd =
            sourceEnd->isSameFile(mappedEnd) &&
            sourceEnd->get_physical_line() == mappedEnd->get_physical_line() &&
            sourceEnd->get_col() == mappedEnd->get_col();
        if (!exactStart || !exactEnd) {
          fprintf(stderr,
                  "REX_TOKEN_INVARIANT[function-prototype-boundary]: "
                  "declaration=%p name=%s source=%d:%d-%d:%d does not "
                  "exactly match mapped tokens=[%d,%d] range=%d:%d-%d:%d\n",
                  static_cast<void *>(function),
                  function->get_name().getString().c_str(),
                  sourceStart->get_physical_line(), sourceStart->get_col(),
                  sourceEnd->get_physical_line(), sourceEnd->get_col(),
                  tokenStart, tokenEnd, mappedStart->get_physical_line(),
                  mappedStart->get_col(), mappedEnd->get_physical_line(),
                  mappedEnd->get_col());
          ROSE_ABORT();
        }
      }

      const size_t occurrences = static_cast<size_t>(
          std::count(direct->second->nodeVector.begin(),
                     direct->second->nodeVector.end(), declaration));
      if (occurrences != 1) {
        fprintf(stderr,
                "REX_TOKEN_INVARIANT[declaration-mapping]: declaration=%p/%s "
                "occurs %zu times in its direct token mapping node vector\n",
                static_cast<void *>(declaration),
                declaration->class_name().c_str(), occurrences);
        ROSE_ABORT();
      }

      for (SgNode *associatedNode : direct->second->nodeVector) {
        SgDeclarationStatement *associated =
            isSgDeclarationStatement(associatedNode);
        if (associated == nullptr || associated == declaration ||
            !declarationRequiresTokenMapping(associated, sourceFile)) {
          continue;
        }
        fprintf(
            stderr,
            "REX_TOKEN_INVARIANT[declaration-mapping]: declaration=%p/%s "
            "token mapping borrows distinct declaration=%p/%s\n",
            static_cast<void *>(declaration), declaration->class_name().c_str(),
            static_cast<void *>(associated), associated->class_name().c_str());
        ROSE_ABORT();
      }
    }
  }
}

void publishTokenMappingIntervals(
    SgSourceFile *sourceFile,
    std::map<SgNode *, TokenStreamSequenceToNodeMapping *> &tokenMap) {
  ASSERT_not_null(sourceFile);
  validateDeclarationTokenMappings(sourceFile, tokenMap);
  const size_t tokenCount = sourceFile->get_token_list().size();
  if (tokenMap.empty()) {
    fprintf(stderr,
            "REX_TOKEN_INVARIANT[mapping-interval]: file=%s has no token "
            "mappings to publish\n",
            sourceFile->getFileName().c_str());
    ROSE_ABORT();
  }
  std::set<TokenStreamSequenceToNodeMapping *> validatedMappings;

  auto validateInterval = [&](SgNode *node, const char *name,
                              const TokenStreamHalfOpenInterval &interval,
                              bool allow_empty) {
    if (interval.begin < 0 || interval.end < interval.begin ||
        (!allow_empty && interval.empty()) ||
        static_cast<size_t>(interval.end) > tokenCount) {
      fprintf(stderr,
              "REX_TOKEN_INVARIANT[mapping-interval]: file=%s node=%p "
              "type=%s line=%d column=%d parent=%p parent-type=%s "
              "%s=[%d,%d) token-count=%zu\n",
              sourceFile->getFileName().c_str(), static_cast<void *>(node),
              node != nullptr ? node->class_name().c_str() : "<null>",
              isSgLocatedNode(node) != nullptr &&
                      isSgLocatedNode(node)->get_startOfConstruct() != nullptr
                  ? isSgLocatedNode(node)->get_startOfConstruct()->get_line()
                  : -1,
              isSgLocatedNode(node) != nullptr &&
                      isSgLocatedNode(node)->get_startOfConstruct() != nullptr
                  ? isSgLocatedNode(node)->get_startOfConstruct()->get_col()
                  : -1,
              node != nullptr ? static_cast<void *>(node->get_parent())
                              : nullptr,
              node != nullptr && node->get_parent() != nullptr
                  ? node->get_parent()->class_name().c_str()
                  : "<null>",
              name, interval.begin, interval.end, tokenCount);
      ROSE_ABORT();
    }
  };

  auto validateOptionalInterval =
      [&](SgNode *node, const char *name,
          const std::optional<TokenStreamHalfOpenInterval> &interval) {
        if (interval.has_value()) {
          validateInterval(node, name, *interval, false);
        }
      };

  for (const auto &entry : tokenMap) {
    if (entry.first == nullptr || entry.second == nullptr ||
        entry.second->node == nullptr) {
      fprintf(stderr,
              "REX_TOKEN_INVARIANT[mapping-interval]: file=%s contains a "
              "null map key, mapping, or mapping owner\n",
              sourceFile->getFileName().c_str());
      ROSE_ABORT();
    }
    TokenStreamSequenceToNodeMapping *mapping = entry.second;
    if (entry.first != mapping->node &&
        std::find(mapping->nodeVector.begin(), mapping->nodeVector.end(),
                  entry.first) == mapping->nodeVector.end()) {
      fprintf(stderr,
              "REX_TOKEN_INVARIANT[mapping-alias]: file=%s key=%p/%s is not "
              "owned by mapping=%p owner=%p/%s\n",
              sourceFile->getFileName().c_str(),
              static_cast<void *>(entry.first),
              entry.first->class_name().c_str(), static_cast<void *>(mapping),
              static_cast<void *>(mapping->node),
              mapping->node->class_name().c_str());
      ROSE_ABORT();
    }
    if (!validatedMappings.insert(mapping).second) {
      continue;
    }
    const auto &draft =
        mapping->constructionState(TokenStreamMappingConstructionAccess::key());
    validateOptionalInterval(mapping->node, "leading-whitespace",
                             draft.leadingWhitespace());
    validateInterval(mapping->node, "token-subsequence",
                     draft.tokenSubsequence(), tokenCount == 0);
    validateOptionalInterval(mapping->node, "trailing-whitespace",
                             draft.trailingWhitespace());
    validateOptionalInterval(mapping->node, "else-whitespace",
                             draft.elseWhitespace());
    if (mapping->shared != (mapping->nodeVector.size() > 1)) {
      fprintf(stderr,
              "REX_TOKEN_INVARIANT[mapping-alias]: file=%s node=%p type=%s "
              "has inconsistent shared state for %zu associated nodes\n",
              sourceFile->getFileName().c_str(),
              static_cast<void *>(mapping->node),
              mapping->node->class_name().c_str(), mapping->nodeVector.size());
      ROSE_ABORT();
    }
    std::set<SgNode *> associatedNodes;
    for (SgNode *node : mapping->nodeVector) {
      if (node == nullptr || !associatedNodes.insert(node).second) {
        fprintf(stderr,
                "REX_TOKEN_INVARIANT[mapping-alias]: file=%s node=%p "
                "type=%s has a null or duplicate associated node\n",
                sourceFile->getFileName().c_str(),
                static_cast<void *>(mapping->node),
                mapping->node->class_name().c_str());
        ROSE_ABORT();
      }
    }
    if (!mapping->nodeVector.empty() &&
        associatedNodes.find(mapping->node) == associatedNodes.end()) {
      fprintf(stderr,
              "REX_TOKEN_INVARIANT[mapping-alias]: file=%s node=%p type=%s "
              "owner is absent from its associated-node list\n",
              sourceFile->getFileName().c_str(),
              static_cast<void *>(mapping->node),
              mapping->node->class_name().c_str());
      ROSE_ABORT();
    }
  }

  for (TokenStreamSequenceToNodeMapping *mapping : validatedMappings) {
    TokenStreamMappingConstructionAccess::publish(mapping, tokenCount);
  }
}

void validatePublishedTokenMappingIntervals(SgSourceFile *sourceFile) {
  ASSERT_not_null(sourceFile);
  const size_t token_count = sourceFile->get_token_list().size();
  auto &token_map = sourceFile->get_tokenSubsequenceMap();
  if (token_map.empty()) {
    fprintf(stderr,
            "REX_TOKEN_INVARIANT[published-map]: file=%s has no published "
            "token mappings\n",
            sourceFile->getFileName().c_str());
    ROSE_ABORT();
  }

  validateDeclarationTokenMappings(sourceFile, token_map,
                                   /*intervalsPublished=*/true);

  std::set<TokenStreamSequenceToNodeMapping *> validated;
  for (const auto &entry : token_map) {
    TokenStreamSequenceToNodeMapping *mapping = entry.second;
    if (entry.first == nullptr || mapping == nullptr ||
        mapping->node == nullptr) {
      fprintf(stderr,
              "REX_TOKEN_INVARIANT[published-map]: file=%s contains a null "
              "key, mapping, or owner\n",
              sourceFile->getFileName().c_str());
      ROSE_ABORT();
    }
    if (entry.first != mapping->node &&
        std::find(mapping->nodeVector.begin(), mapping->nodeVector.end(),
                  entry.first) == mapping->nodeVector.end()) {
      fprintf(stderr,
              "REX_TOKEN_INVARIANT[published-map]: file=%s key=%p/%s is not "
              "owned by mapping=%p owner=%p/%s\n",
              sourceFile->getFileName().c_str(),
              static_cast<void *>(entry.first),
              entry.first->class_name().c_str(), static_cast<void *>(mapping),
              static_cast<void *>(mapping->node),
              mapping->node->class_name().c_str());
      ROSE_ABORT();
    }
    if (!validated.insert(mapping).second) {
      continue;
    }

    const TokenStreamHalfOpenInterval &leading =
        mapping->halfOpenInterval(TokenStreamIntervalKind::leading_whitespace);
    const TokenStreamHalfOpenInterval &core =
        mapping->halfOpenInterval(TokenStreamIntervalKind::token_subsequence);
    const TokenStreamHalfOpenInterval &trailing =
        mapping->halfOpenInterval(TokenStreamIntervalKind::trailing_whitespace);
    const TokenStreamHalfOpenInterval &else_interval =
        mapping->halfOpenInterval(TokenStreamIntervalKind::else_whitespace);
    auto require_bounded = [&](const char *name,
                               const TokenStreamHalfOpenInterval &interval) {
      if (interval.begin < 0 || interval.end < interval.begin ||
          static_cast<size_t>(interval.end) > token_count) {
        fprintf(stderr,
                "REX_TOKEN_INVARIANT[published-map]: file=%s node=%p/%s %s "
                "interval [%d,%d) is outside [0,%zu)\n",
                sourceFile->getFileName().c_str(),
                static_cast<void *>(mapping->node),
                mapping->node->class_name().c_str(), name, interval.begin,
                interval.end, token_count);
        ROSE_ABORT();
      }
    };
    require_bounded("leading-whitespace", leading);
    require_bounded("token-subsequence", core);
    require_bounded("trailing-whitespace", trailing);
    require_bounded("else-whitespace", else_interval);
    if ((token_count != 0 && core.empty()) || leading.end != core.begin ||
        trailing.begin != core.end ||
        (!else_interval.empty() &&
         (else_interval.begin < core.begin || else_interval.end > core.end)) ||
        (else_interval.empty() && else_interval.begin != core.end)) {
      fprintf(stderr,
              "REX_TOKEN_INVARIANT[published-map]: file=%s node=%p/%s has "
              "inconsistent intervals leading=[%d,%d) core=[%d,%d) "
              "trailing=[%d,%d) else=[%d,%d)\n",
              sourceFile->getFileName().c_str(),
              static_cast<void *>(mapping->node),
              mapping->node->class_name().c_str(), leading.begin, leading.end,
              core.begin, core.end, trailing.begin, trailing.end,
              else_interval.begin, else_interval.end);
      ROSE_ABORT();
    }

    std::set<SgNode *> associated;
    for (SgNode *node : mapping->nodeVector) {
      if (node == nullptr || !associated.insert(node).second) {
        fprintf(stderr,
                "REX_TOKEN_INVARIANT[published-map]: file=%s mapping=%p has "
                "a null or duplicate associated node\n",
                sourceFile->getFileName().c_str(),
                static_cast<void *>(mapping));
        ROSE_ABORT();
      }
    }
    if (associated.find(mapping->node) == associated.end() ||
        mapping->shared != (mapping->nodeVector.size() > 1)) {
      fprintf(stderr,
              "REX_TOKEN_INVARIANT[published-map]: file=%s mapping=%p has "
              "inconsistent owner/shared metadata\n",
              sourceFile->getFileName().c_str(), static_cast<void *>(mapping));
      ROSE_ABORT();
    }
  }
}

} // namespace

void buildTokenStreamFrontier(SgSourceFile *sourceFile,
                              bool traverseHeaderFiles,
                              TokenUnparseFrontierContext &context,
                              SgNode *traversalRoot) {

  ASSERT_not_null(sourceFile);
  validatePublishedTokenMappingIntervals(sourceFile);
  TokenUnparseFrontierFileContext &fileContext = context.beginFile(sourceFile);

  // DQ (4/1/2021): Added assertion to debug tests in
  // UnparseHeadersUsingTokenStream_tests.
  ROSE_ASSERT(Rose::tokenSubsequenceMapOfMapsBySourceFile.find(sourceFile) !=
              Rose::tokenSubsequenceMapOfMapsBySourceFile.end());

  // DQ (5/31/2021): Adding testing support for token-based unparsing.
  if (ROSE_tokenUnparsingTestingMode == true) {
    buildArtificialFrontier(sourceFile, traverseHeaderFiles, fileContext);
  }

  // Macro expansions must be classified as one unit. Record that choice in the
  // invocation context before the final bottom-up transformation analysis.
  detectMacroExpansionsToBeUnparsedAsAstTransformations(sourceFile,
                                                        fileContext);

  // Convert modified descendants and the choices above into a complete,
  // invocation-owned statement classification. This analysis must not rewrite
  // AST modification, transformation, or output flags.
  simpleFrontierDetectionForTokenStreamMapping(sourceFile, traverseHeaderFiles,
                                               fileContext, traversalRoot);

  // DQ (12/6/2014): I think we need the frontier mechanism, and then the
  // partial use of token streams on nodes containing transformations is
  // required to provide a more precise generated code (precise representation
  // with minimal diff).

  // Note that we first detect the frontier.
  // DQ (5/10/2021): Activate this code.
  frontierDetectionForTokenStreamMapping(sourceFile, traverseHeaderFiles,
                                         context, traversalRoot);

  // A frontier statement may replay tokens only when it is the exact owner of
  // one unique source surface.  Legacy code grouped statements by their last
  // token and then silently selected whichever statement traversal reached
  // first.  That made output depend on traversal history and also conflated
  // nested intervals that merely shared an end token.
  const auto &tokenStreamSequenceMap = sourceFile->get_tokenSubsequenceMap();
  std::map<SgStatement *, FrontierNode *> &token_unparse_frontier_map =
      fileContext.frontierNodes;
  std::map<TokenStreamSequenceToNodeMapping *, SgStatement *>
      frontierMappingOwners;
  std::map<std::pair<int, int>, SgStatement *> tokenSurfaceOwners;
  for (const auto &entry : token_unparse_frontier_map) {
    SgStatement *statement = entry.first;
    FrontierNode *frontierNode = entry.second;
    if (statement == nullptr || frontierNode == nullptr ||
        frontierNode->node != statement) {
      fprintf(stderr,
              "REX_TOKEN_INVARIANT[frontier-owner]: file=%s has an "
              "incomplete or mismatched frontier entry\n",
              sourceFile->getFileName().c_str());
      ROSE_ABORT();
    }

    const auto mappingEntry = tokenStreamSequenceMap.find(statement);
    if (mappingEntry == tokenStreamSequenceMap.end()) {
      continue;
    }
    TokenStreamSequenceToNodeMapping *mapping = mappingEntry->second;
    if (mapping == nullptr) {
      fprintf(stderr,
              "REX_TOKEN_INVARIANT[frontier-owner]: file=%s "
              "statement-type=%s has a null token mapping\n",
              sourceFile->getFileName().c_str(),
              statement->class_name().c_str());
      ROSE_ABORT();
    }
    const TokenStreamHalfOpenInterval &core =
        mapping->halfOpenInterval(TokenStreamIntervalKind::token_subsequence);
    if (core.empty()) {
      // Empty translation units have two distinct zero-length root mappings.
      // Their ordinary forward and reverse ownership edges remain complete
      // even though there is no physical token interval.
      if (!isExactEmptyTranslationUnitTokenMapping(sourceFile, statement,
                                                   mapping)) {
        fprintf(stderr,
                "REX_TOKEN_INVARIANT[frontier-owner]: file=%s "
                "statement-type=%s has an empty token surface outside the "
                "exact empty-translation-unit root contract\n",
                sourceFile->getFileName().c_str(),
                statement->class_name().c_str());
        ROSE_ABORT();
      }
      continue;
    }
    const size_t reverseAssociations = static_cast<size_t>(std::count(
        mapping->nodeVector.begin(), mapping->nodeVector.end(), statement));
    if (reverseAssociations != 1) {
      fprintf(stderr,
              "REX_TOKEN_INVARIANT[frontier-owner]: file=%s "
              "statement-type=%s has %zu reverse token associations\n",
              sourceFile->getFileName().c_str(),
              statement->class_name().c_str(), reverseAssociations);
      ROSE_ABORT();
    }
    if (mapping->node != statement) {
      if (!mapping->shared || mapping->node == nullptr ||
          std::count(mapping->nodeVector.begin(), mapping->nodeVector.end(),
                     mapping->node) != 1) {
        fprintf(stderr,
                "REX_TOKEN_INVARIANT[frontier-owner]: file=%s "
                "statement-type=%s is a nonowner alias of mapping-owner=%s "
                "with an invalid shared ownership relation\n",
                sourceFile->getFileName().c_str(),
                statement->class_name().c_str(),
                mapping->node != nullptr ? mapping->node->class_name().c_str()
                                         : "<null>");
        ROSE_ABORT();
      }
      // Exact-interval aliases are valid members of nodeVector, but only the
      // canonical mapping->node owner may replay that interval.  Resolve the
      // preliminary frontier decision to the exact physical owner here, where
      // both forward and reverse mapping identities are available.
      frontierNode->unparseUsingTokenStream = false;
      continue;
    }
    if (!frontierNode->unparseUsingTokenStream) {
      continue;
    }
    const auto mappingOwner = frontierMappingOwners.emplace(mapping, statement);
    if (!mappingOwner.second && mappingOwner.first->second != statement) {
      fprintf(stderr,
              "REX_TOKEN_INVARIANT[frontier-owner]: file=%s "
              "statement-types=%s/%s claim one token mapping\n",
              sourceFile->getFileName().c_str(),
              mappingOwner.first->second->class_name().c_str(),
              statement->class_name().c_str());
      ROSE_ABORT();
    }
    const auto surfaceOwner = tokenSurfaceOwners.emplace(
        std::make_pair(core.begin, core.end), statement);
    if (!surfaceOwner.second && surfaceOwner.first->second != statement) {
      SgStatement *existingOwner = surfaceOwner.first->second;
      auto describePosition = [](SgStatement *owner, bool start) {
        Sg_File_Info *position =
            start ? owner->get_startOfConstruct() : owner->get_endOfConstruct();
        return std::make_tuple(
            position != nullptr ? position->get_raw_line() : 0,
            position != nullptr ? position->get_raw_col() : 0,
            position != nullptr ? position->get_file_id() : -1,
            position != nullptr && position->isCompilerGenerated() ? 1 : 0,
            position != nullptr && position->isFrontendSpecific() ? 1 : 0);
      };
      const auto existingStart = describePosition(existingOwner, true);
      const auto existingEnd = describePosition(existingOwner, false);
      const auto statementStart = describePosition(statement, true);
      const auto statementEnd = describePosition(statement, false);
      const SgTokenPtrList &tokens = sourceFile->get_token_list();
      const std::string beginLexeme =
          core.begin >= 0 && static_cast<size_t>(core.begin) < tokens.size()
              ? tokens[core.begin]->get_lexeme_string()
              : "<out-of-range>";
      const std::string endLexeme =
          core.end > core.begin &&
                  static_cast<size_t>(core.end - 1) < tokens.size()
              ? tokens[core.end - 1]->get_lexeme_string()
              : "<out-of-range>";
      fprintf(stderr,
              "REX_TOKEN_INVARIANT[frontier-owner]: file=%s "
              "token-surface=[%d,%d) lexemes=%s..%s has duplicate owners "
              "%p/%s parent=%p/%s source=[%d:%d,%d:%d] file=%d/%d "
              "compiler=%d/%d frontend=%d/%d and %p/%s parent=%p/%s "
              "source=[%d:%d,%d:%d] file=%d/%d compiler=%d/%d "
              "frontend=%d/%d\n",
              sourceFile->getFileName().c_str(), core.begin, core.end,
              beginLexeme.c_str(), endLexeme.c_str(),
              static_cast<void *>(existingOwner),
              existingOwner->class_name().c_str(),
              static_cast<void *>(existingOwner->get_parent()),
              existingOwner->get_parent() != nullptr
                  ? existingOwner->get_parent()->class_name().c_str()
                  : "<null>",
              std::get<0>(existingStart), std::get<1>(existingStart),
              std::get<0>(existingEnd), std::get<1>(existingEnd),
              std::get<2>(existingStart), std::get<2>(existingEnd),
              std::get<3>(existingStart), std::get<3>(existingEnd),
              std::get<4>(existingStart), std::get<4>(existingEnd),
              static_cast<void *>(statement), statement->class_name().c_str(),
              static_cast<void *>(statement->get_parent()),
              statement->get_parent() != nullptr
                  ? statement->get_parent()->class_name().c_str()
                  : "<null>",
              std::get<0>(statementStart), std::get<1>(statementStart),
              std::get<0>(statementEnd), std::get<1>(statementEnd),
              std::get<2>(statementStart), std::get<2>(statementEnd),
              std::get<3>(statementStart), std::get<3>(statementEnd),
              std::get<4>(statementStart), std::get<4>(statementEnd));
      const int diagnosticBegin = std::max(0, core.begin - 4);
      const int diagnosticEnd =
          std::min(static_cast<int>(tokens.size()),
                   std::max(core.end + 4, core.begin + 1));
      for (int tokenIndex = diagnosticBegin; tokenIndex < diagnosticEnd;
           ++tokenIndex) {
        SgToken *token = tokens[tokenIndex];
        Sg_File_Info *tokenStart =
            token != nullptr ? token->get_startOfConstruct() : nullptr;
        Sg_File_Info *tokenEnd =
            token != nullptr ? token->get_endOfConstruct() : nullptr;
        fprintf(stderr,
                "REX_TOKEN_INVARIANT[frontier-owner-token]: index=%d "
                "lexeme=%s source=[%d:%d,%d:%d]\n",
                tokenIndex,
                token != nullptr ? token->get_lexeme_string().c_str()
                                 : "<null>",
                tokenStart != nullptr ? tokenStart->get_raw_line() : 0,
                tokenStart != nullptr ? tokenStart->get_raw_col() : 0,
                tokenEnd != nullptr ? tokenEnd->get_raw_line() : 0,
                tokenEnd != nullptr ? tokenEnd->get_raw_col() : 0);
      }
      ROSE_ABORT();
    }
  }

  std::vector<SgStatement *> orderedFrontierStatements;
  orderedFrontierStatements.reserve(token_unparse_frontier_map.size());
  for (const auto &entry : token_unparse_frontier_map) {
    ASSERT_not_null(entry.first);
    orderedFrontierStatements.push_back(entry.first);
  }
  std::sort(orderedFrontierStatements.begin(), orderedFrontierStatements.end(),
            [](SgStatement *lhs, SgStatement *rhs) {
              ASSERT_not_null(lhs);
              ASSERT_not_null(rhs);
              ASSERT_not_null(lhs->get_file_info());
              ASSERT_not_null(rhs->get_file_info());
              Sg_File_Info *lhsInfo = lhs->get_file_info();
              Sg_File_Info *rhsInfo = rhs->get_file_info();
              return std::make_tuple(lhsInfo->get_physical_file_id(),
                                     lhsInfo->get_raw_line(),
                                     lhsInfo->get_raw_col(), lhs) <
                     std::make_tuple(rhsInfo->get_physical_file_id(),
                                     rhsInfo->get_raw_line(),
                                     rhsInfo->get_raw_col(), rhs);
            });
  SgGlobal *globalScope = sourceFile->get_globalScope();
  ASSERT_not_null(globalScope);
  for (size_t index = 0; index < orderedFrontierStatements.size(); ++index) {
    SgNode *previous = index == 0 ? static_cast<SgNode *>(globalScope)
                                  : orderedFrontierStatements[index - 1];
    SgNode *next = index + 1 == orderedFrontierStatements.size()
                       ? static_cast<SgNode *>(globalScope)
                       : orderedFrontierStatements[index + 1];
    fileContext.frontierAdjacency.emplace(orderedFrontierStatements[index],
                                          std::make_pair(previous, next));
  }
}

void buildTokenStreamMapping(SgSourceFile *sourceFile,
                             vector<stream_element *> &tokenVector) {
  buildTokenStreamMappingForRoot(sourceFile, sourceFile, tokenVector);
}

void buildTokenStreamMappingForRoot(SgSourceFile *sourceFile,
                                    SgNode *traversalRoot,
                                    vector<stream_element *> &tokenVector) {
  // DQ (12/6/2014): This function separates the initial generation of the token
  // stream and it's mapping to the AST from the assocaited connection to the
  // computed frontier after transformations have been done to define where the
  // AST should be using the token stream unparsing and where it should be using
  // the AST unparsing.

  // DQ (02/20/2021): Using the performance tracking within ROSE.
  TimingPerformance timer("AST Token Stream Mapping:");

  if (sourceFile == nullptr || traversalRoot == nullptr ||
      tokenVector.size() >
          static_cast<size_t>(std::numeric_limits<int>::max())) {
    fprintf(stderr,
            "REX_TOKEN_INVARIANT[mapping-build]: source-file=%p "
            "traversal-root=%p token-count=%zu requires non-null roots and an "
            "int-sized token stream\n",
            static_cast<void *>(sourceFile), static_cast<void *>(traversalRoot),
            tokenVector.size());
    ROSE_ABORT();
  }

  for (const stream_element *token : tokenVector) {
    if (token == nullptr || token->p_tok_elem == nullptr) {
      fprintf(stderr,
              "REX_TOKEN_INVARIANT[mapping-build]: token stream contains a "
              "missing token or classification\n");
      ROSE_ABORT();
    }
    const file_pos_info &begin = token->beginning_fpi;
    const file_pos_info &end = token->ending_fpi;
    if (begin.line_num <= 0 || begin.column_num < 0 || end.line_num <= 0 ||
        end.column_num < 0 || end.line_num < begin.line_num ||
        (end.line_num == begin.line_num && end.column_num < begin.column_num)) {
      fprintf(stderr,
              "REX_TOKEN_INVARIANT[source-position]: token lexeme=%s has "
              "invalid physical source range=%d:%d-%d:%d\n",
              token->p_tok_elem->token_lexeme.c_str(), begin.line_num,
              begin.column_num, end.line_num, end.column_num);
      ROSE_ABORT();
    }
    if (token->p_tok_elem->token_id < 0) {
      fprintf(stderr,
              "REX_TOKEN_INVARIANT[classification]: token lexeme=%s has "
              "negative classification=%d\n",
              token->p_tok_elem->token_lexeme.c_str(),
              token->p_tok_elem->token_id);
      ROSE_ABORT();
    }
  }

  // DQ (9/5/2018): We should have already set the
  // preprocessorDirectivesAndCommentsList, checked in getTokenStream().
  ROSE_ASSERT(sourceFile->get_preprocessorDirectivesAndCommentsList() != NULL);

  // DQ (2/18/2021): Warn that the buildTokenStreamMapping() is always called.
  if (sourceFile->get_unparse_tokens() == false) {
    printf("*** Note that the buildTokenStreamMapping() has been called even "
           "if sourceFile->get_unparse_tokens() == false: source file = %s \n",
           sourceFile->getFileName().c_str());
  }

  // DQ (11/29/2018): Debugging the token stream (for form-feeds).

  // Retire the prior publication before constructing this file's new pool.
  sourceFile->clear_tokenSubsequenceMap();
  auto *tokenStreamSequenceMapPointer =
      new std::map<SgNode *, TokenStreamSequenceToNodeMapping *>();
  auto installPublishedMap = [&]() {
    sourceFile->set_tokenSubsequenceMap(tokenStreamSequenceMapPointer);
  };

  if (tokenVector.empty() == true) {
    printf("In buildTokenStreamMapping(): No tokens found in file \n");

    // DQ (1/30/2021): If there are no tokens or AST nodes, then at least build
    // the empty map to avoid errors later. This is the trivial case of an empty
    // file.
    SgGlobal *globalScope = sourceFile->get_globalScope();
    ROSE_ASSERT(globalScope != NULL);

    const TokenStreamHalfOpenInterval emptyInterval(0, 0);
    TokenStreamSequenceToNodeMapping *sourceFileMapping =
        TokenStreamSequenceToNodeMapping::createPublished(
            sourceFile, emptyInterval, emptyInterval, emptyInterval,
            emptyInterval, 0);
    TokenStreamSequenceToNodeMapping *globalScopeMapping =
        TokenStreamSequenceToNodeMapping::createPublished(
            globalScope, emptyInterval, emptyInterval, emptyInterval,
            emptyInterval, 0);
    (*tokenStreamSequenceMapPointer)[sourceFile] = sourceFileMapping;
    (*tokenStreamSequenceMapPointer)[globalScope] = globalScopeMapping;

    ROSE_ASSERT(tokenStreamSequenceMapPointer != NULL);
    installPublishedMap();
    validatePublishedTokenMappingIntervals(sourceFile);

    return;
  }

  InheritedAttribute inheritedAttribute(
      sourceFile, NULL,
      TokenStreamHalfOpenInterval(0, static_cast<int>(tokenVector.size())),
      true);

  TokenMappingTraversal tokenMappingTraversal(tokenVector, sourceFile,
                                              tokenStreamSequenceMapPointer);

  const TokenStreamHalfOpenInterval wholeFileInterval(
      0, static_cast<int>(tokenVector.size()));
  auto ensureWholeFileMapping = [&](SgNode *node) {
    if (node == NULL) {
      return;
    }

    std::map<SgNode *, TokenStreamSequenceToNodeMapping *>::iterator mapIt =
        tokenStreamSequenceMapPointer->find(node);
    if (mapIt != tokenStreamSequenceMapPointer->end() &&
        mapIt->second != NULL) {
      return;
    }

    (*tokenStreamSequenceMapPointer)[node] =
        TokenStreamMappingConstructionAccess::construct(node,
                                                        wholeFileInterval);
  };

  {
    TimingPerformance timer("AST tokenMappingTraversal:");

    SgNode *traversalRootNode =
        traversalRoot != NULL ? traversalRoot : sourceFile;
    tokenMappingTraversal.traverse(traversalRootNode, inheritedAttribute);
  }
  tokenMappingTraversal.validateConstructionConsistency();

  ensureWholeFileMapping(sourceFile);

  // DQ (10/27/2013): Build the SgToken IR nodes and the vector of them into the
  // SgSourceFile IR node.
  SgTokenPtrList &roseTokenList = sourceFile->get_token_list();

  // DQ (11/29/2013): I think this should be empty at this point.
  ROSE_ASSERT(roseTokenList.empty() == true);

  // Setup the current file ID from the name in the source file.
  ROSE_ASSERT(sourceFile->get_file_info() != NULL);
  int currentFileId = sourceFile->get_file_info()->get_file_id();

  // DQ (1/7/2021): Output info on this converstion from the data structure used
  // in the lex file to the data structure used in the rest of ROSE.
  if (SgProject::get_unparseHeaderFilesDebug() >= 4) {
    printf("In buildTokenStreamMapping(): building the SgToken objects from "
           "the tokens collected by the lex file: size = %zu \n",
           roseTokenList.size());
  }

  {
    // DQ (02/20/2021): Using the performance tracking within ROSE.
    TimingPerformance timer("AST build the SgToken vector:");

    // This should now include all of the CPP directives and C/C++ style
    // comments as tokens.
    for (vector<stream_element *>::iterator i = tokenVector.begin();
         i != tokenVector.end(); i++) {
      const file_pos_info &begin = (*i)->beginning_fpi;
      const file_pos_info &end = (*i)->ending_fpi;

      SgToken *roseToken = new SgToken((*i)->p_tok_elem->token_lexeme,
                                       (*i)->p_tok_elem->token_id);
      ROSE_ASSERT(roseToken != NULL);

      Sg_File_Info *start =
          new Sg_File_Info(currentFileId, begin.line_num, begin.column_num);
      Sg_File_Info *finish =
          new Sg_File_Info(currentFileId, end.line_num, end.column_num);
      start->set_physical_line(begin.line_num);
      finish->set_physical_line(end.line_num);
      roseToken->set_startOfConstruct(start);
      roseToken->set_endOfConstruct(finish);
      roseToken->set_parent(sourceFile);

      roseTokenList.push_back(roseToken);
    }
  }

  for (const auto &entry : tokenMappingTraversal.tokenStreamSequenceMap) {
    ROSE_ASSERT(entry.second != NULL && entry.second->node != NULL);
  }

  finalizeDeclarationTokenMappings(sourceFile, tokenVector,
                                   *tokenStreamSequenceMapPointer);
  finalizeStatementTokenMappingBoundaries(sourceFile,
                                          *tokenStreamSequenceMapPointer);
  publishTokenMappingIntervals(sourceFile, *tokenStreamSequenceMapPointer);
  installPublishedMap();
  validatePublishedTokenMappingIntervals(sourceFile);

  {
    TimingPerformance timer("AST set_representativeWhitespaceStatementMap:");

    sourceFile->set_representativeWhitespaceStatementMap(
        tokenMappingTraversal.representativeWhitespaceStatementMap);
  }
}
