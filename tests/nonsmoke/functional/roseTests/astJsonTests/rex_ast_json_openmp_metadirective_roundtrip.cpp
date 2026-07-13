#include "sageAstJsonPrivate.h"

#include <cctype>
#include <iostream>
#include <string>
#include <vector>

namespace {

class MetadirectiveValidator : public AstSimpleProcessing {
public:
  bool valid() const { return valid_; }
  size_t whenClauseCount() const { return when_clause_count_; }
  size_t matchClauseCount() const { return match_clause_count_; }
  size_t beginDeclareVariantCount() const {
    return begin_declare_variant_count_;
  }
  size_t endDeclareVariantCount() const { return end_declare_variant_count_; }

private:
  bool valid_ = true;
  size_t when_clause_count_ = 0;
  size_t match_clause_count_ = 0;
  size_t begin_declare_variant_count_ = 0;
  size_t end_declare_variant_count_ = 0;

  void fail(const std::string &message) {
    std::cerr
        << "invalid OpenMP context selector after AST JSON reconstruction: "
        << message << "\n";
    valid_ = false;
  }

  void checkPropertyOwner(SgOmpContextSelectorProperty *property,
                          SgOmpContextSelector *selector) {
    if (property == nullptr || property->get_parent() != selector) {
      fail("trait property is not owned by its exact selector");
    }
  }

  SgOmpContextSelector *
  selectorAt(SgOmpContextSelectorSet *set, size_t index,
             SgOmpClause::omp_context_trait_selector_kind_enum expected_kind) {
    if (set == nullptr || index >= set->get_selectors().size()) {
      fail("selector set has the wrong selector cardinality");
      return nullptr;
    }
    SgOmpContextSelector *selector = set->get_selectors()[index];
    if (selector == nullptr || selector->get_parent() != set ||
        selector->get_selector_kind() != expected_kind) {
      fail("selector order, kind, or ownership was not preserved");
      return nullptr;
    }
    return selector;
  }

  void checkNameProperty(SgOmpContextSelector *selector, size_t index,
                         const std::string &expected) {
    if (selector == nullptr || index >= selector->get_properties().size()) {
      fail("name-list selector has the wrong property cardinality");
      return;
    }
    SgOmpContextSelectorProperty *property = selector->get_properties()[index];
    checkPropertyOwner(property, selector);
    SgOmpNameExpression *name =
        property != nullptr ? isSgOmpNameExpression(property->get_expression())
                            : nullptr;
    if (name == nullptr || name->get_spelling() != expected ||
        name->get_parent() != property) {
      fail("OpenMP name property lost its exact syntax node");
    }
  }

  void checkStringProperty(SgOmpContextSelector *selector, size_t index,
                           const std::string &expected) {
    if (selector == nullptr || index >= selector->get_properties().size()) {
      fail("string name-list selector has the wrong property cardinality");
      return;
    }
    SgOmpContextSelectorProperty *property = selector->get_properties()[index];
    checkPropertyOwner(property, selector);
    SgOmpSourceExpression *source =
        property != nullptr
            ? isSgOmpSourceExpression(property->get_expression())
            : nullptr;
    if (source == nullptr || source->get_spelling() != "\"" + expected + "\"" ||
        source->get_parent() != property) {
      fail("OpenMP string property lost its exact syntax node");
    }
  }

  void checkKindSelector(SgOmpContextSelector *selector) {
    if (selector == nullptr || selector->get_properties().size() != 2) {
      fail("kind selector did not preserve both properties");
      return;
    }
    const SgOmpClause::omp_when_context_kind_enum expected[] = {
        SgOmpClause::e_omp_when_context_kind_cpu,
        SgOmpClause::e_omp_when_context_kind_gpu};
    for (size_t index = 0; index < 2; ++index) {
      SgOmpContextSelectorProperty *property =
          selector->get_properties()[index];
      checkPropertyOwner(property, selector);
      if (property == nullptr ||
          property->get_context_kind() != expected[index] ||
          property->get_expression() != nullptr) {
        fail("kind selector property lost its typed enum payload");
      }
    }
  }

  void checkDeviceSet(SgOmpContextSelectorSet *set, SgNode *owner) {
    if (set == nullptr || set->get_parent() != owner ||
        set->get_set_kind() != SgOmpClause::e_omp_context_selector_set_device ||
        set->get_selectors().size() != 1) {
      fail("device selector set has the wrong kind, owner, or cardinality");
      return;
    }
    checkKindSelector(
        selectorAt(set, 0, SgOmpClause::e_omp_context_trait_kind));
  }

  void checkTargetDeviceSet(SgOmpContextSelectorSet *set, SgNode *owner,
                            bool expect_device_num_variable) {
    if (set == nullptr || set->get_parent() != owner ||
        set->get_set_kind() !=
            SgOmpClause::e_omp_context_selector_set_target_device ||
        set->get_selectors().size() != 5) {
      fail("target_device set has the wrong kind, owner, or cardinality");
      return;
    }

    checkKindSelector(
        selectorAt(set, 0, SgOmpClause::e_omp_context_trait_kind));

    SgOmpContextSelector *arch =
        selectorAt(set, 1, SgOmpClause::e_omp_context_trait_arch);
    if (arch == nullptr || arch->get_properties().size() != 2) {
      fail("arch selector did not preserve both properties");
    } else {
      checkStringProperty(arch, 0, "nvptx");
      checkStringProperty(arch, 1, "amdgcn");
    }

    SgOmpContextSelector *isa =
        selectorAt(set, 2, SgOmpClause::e_omp_context_trait_isa);
    if (isa == nullptr || isa->get_properties().size() != 2) {
      fail("isa selector did not preserve both properties");
    } else {
      checkStringProperty(isa, 0, "sse4");
      checkStringProperty(isa, 1, "avx2");
    }

    SgOmpContextSelector *device_num =
        selectorAt(set, 3, SgOmpClause::e_omp_context_trait_device_num);
    if (device_num == nullptr || device_num->get_properties().size() != 1) {
      fail("device_num selector did not preserve its single property");
    } else {
      SgOmpContextSelectorProperty *property =
          device_num->get_properties().front();
      checkPropertyOwner(property, device_num);
      SgExpression *expression =
          property != nullptr ? property->get_expression() : nullptr;
      bool matches = false;
      if (expect_device_num_variable) {
        SgVarRefExp *reference = isSgVarRefExp(expression);
        matches =
            reference != nullptr && reference->get_symbol() != nullptr &&
            reference->get_symbol()->get_name().getString() == "device_id";
      } else {
        SgIntVal *value = isSgIntVal(expression);
        matches = value != nullptr && value->get_value() == 0;
      }
      if (!matches || expression->get_parent() != property) {
        fail("device_num selector lost its semantic expression property");
      }
    }

    SgOmpContextSelector *uid =
        selectorAt(set, 4, SgOmpClause::e_omp_context_trait_uid);
    if (uid == nullptr || uid->get_properties().size() != 1) {
      fail("uid selector did not preserve its single property");
    } else {
      checkStringProperty(uid, 0, "rex-device");
    }
  }

  void checkImplementationSet(
      SgOmpContextSelectorSet *set, SgNode *owner,
      bool expect_requires_variable,
      SgOmpClause::omp_atomic_default_mem_order_kind_enum expected_order) {
    if (set == nullptr || set->get_parent() != owner ||
        set->get_set_kind() !=
            SgOmpClause::e_omp_context_selector_set_implementation ||
        set->get_selectors().size() != 5) {
      fail("implementation set has the wrong kind, owner, or cardinality");
      return;
    }

    SgOmpContextSelector *vendor =
        selectorAt(set, 0, SgOmpClause::e_omp_context_trait_vendor);
    if (vendor == nullptr || vendor->get_properties().size() != 2) {
      fail("vendor selector did not preserve both properties");
    } else {
      const SgOmpClause::omp_when_context_vendor_enum expected[] = {
          SgOmpClause::e_omp_when_context_vendor_gnu,
          SgOmpClause::e_omp_when_context_vendor_llvm};
      for (size_t index = 0; index < 2; ++index) {
        SgOmpContextSelectorProperty *property =
            vendor->get_properties()[index];
        checkPropertyOwner(property, vendor);
        if (property == nullptr ||
            property->get_context_vendor() != expected[index]) {
          fail("vendor selector property lost its typed enum payload");
        }
      }
      SgIntVal *score = isSgIntVal(vendor->get_score());
      if (score == nullptr || score->get_value() != 7 ||
          score->get_parent() != vendor) {
        fail("vendor selector lost its typed score expression");
      }
    }

    SgOmpContextSelector *extension =
        selectorAt(set, 1, SgOmpClause::e_omp_context_trait_extension);
    if (extension == nullptr || extension->get_properties().size() != 2) {
      fail("extension selector did not preserve both properties");
    } else {
      checkNameProperty(extension, 0, "rex_ext_a");
      checkNameProperty(extension, 1, "rex_ext_b");
    }

    SgOmpContextSelector *
      requires
    = selectorAt(set, 2, SgOmpClause::e_omp_context_trait_requires);
    if (requires == nullptr || requires->get_properties().size() != 3) {
      fail("requires selector did not preserve all typed clause properties");
    } else {
      const SgOmpClause::omp_requires_property_kind_enum expected[] = {
          SgOmpClause::e_omp_requires_property_unified_shared_memory,
          SgOmpClause::e_omp_requires_property_reverse_offload,
          SgOmpClause::e_omp_requires_property_dynamic_allocators};
      for (size_t index = 0; index < 3; ++index) {
        SgOmpContextSelectorProperty *property =
          requires
            ->get_properties()[index];
        checkPropertyOwner(property, requires);
        if (property == nullptr ||
            property->get_requires_kind() != expected[index] ||
            property->get_expression() != nullptr ||
            property->get_context_kind() !=
                SgOmpClause::e_omp_when_context_kind_unknown ||
            property->get_context_vendor() !=
                SgOmpClause::e_omp_when_context_vendor_unspecified ||
            property->get_atomic_default_mem_order() !=
                SgOmpClause::e_omp_atomic_default_mem_order_kind_unspecified) {
          fail("requires property lost its distinct typed payload");
        }
      }
      SgOmpContextSelectorProperty *dynamic =
        requires
          ->get_properties()[2];
      SgExpression *expression = dynamic->get_requires_expression();
      bool matches = false;
      if (expect_requires_variable) {
        SgGreaterThanOp *greater = isSgGreaterThanOp(expression);
        SgVarRefExp *reference = greater != nullptr
                                     ? isSgVarRefExp(greater->get_lhs_operand())
                                     : nullptr;
        SgIntVal *zero = greater != nullptr
                             ? isSgIntVal(greater->get_rhs_operand())
                             : nullptr;
        matches =
            reference != nullptr && reference->get_symbol() != nullptr &&
            reference->get_symbol()->get_name().getString() == "device_id" &&
            zero != nullptr && zero->get_value() == 0;
      } else {
        SgIntVal *one = isSgIntVal(expression);
        matches = one != nullptr && one->get_value() == 1;
      }
      if (!matches || expression == nullptr ||
          expression->get_parent() != dynamic) {
        fail("requires property lost its optional semantic expression");
      }
    }

    SgOmpContextSelector *atomic = selectorAt(
        set, 3, SgOmpClause::e_omp_context_trait_atomic_default_mem_order);
    if (atomic == nullptr || atomic->get_properties().size() != 1) {
      fail("atomic_default_mem_order lost its single property");
    } else {
      SgOmpContextSelectorProperty *property = atomic->get_properties().front();
      checkPropertyOwner(property, atomic);
      if (property == nullptr ||
          property->get_atomic_default_mem_order() != expected_order) {
        fail("atomic_default_mem_order lost its typed enum payload");
      }
    }

    SgOmpContextSelector *custom = selectorAt(
        set, 4, SgOmpClause::e_omp_context_trait_implementation_user);
    if (custom == nullptr ||
        custom->get_implementation_defined_name().getString() != "rex_fast" ||
        custom->get_properties().size() != 2) {
      fail("implementation-defined selector lost its explicit identity");
    } else {
      checkNameProperty(custom, 0, "rex_prop");
      SgOmpContextSelectorProperty *nested = custom->get_properties()[1];
      checkPropertyOwner(nested, custom);
      SgOmpSourceExpression *source =
          nested != nullptr ? isSgOmpSourceExpression(nested->get_expression())
                            : nullptr;
      if (source == nullptr || source->get_spelling() != "nested(7)" ||
          source->get_parent() != nested) {
        fail("nested extension property lost its exact source syntax node");
      }
    }
  }

  void checkSelectorSets(
      const SgOmpContextSelectorSetPtrList &sets, SgNode *owner,
      bool expect_device_num_variable,
      SgOmpClause::omp_atomic_default_mem_order_kind_enum expected_order) {
    if (sets.size() != 3) {
      fail("variant clause does not own exactly three selector sets");
      return;
    }
    checkDeviceSet(sets[0], owner);
    checkTargetDeviceSet(sets[1], owner, expect_device_num_variable);
    checkImplementationSet(sets[2], owner, expect_device_num_variable,
                           expected_order);
  }

  void visit(SgNode *node) override {
    if (isSgOmpBeginDeclareVariantStatement(node) != nullptr) {
      ++begin_declare_variant_count_;
    }
    if (isSgOmpEndDeclareVariantStatement(node) != nullptr) {
      ++end_declare_variant_count_;
    }
    if (SgOmpWhenClause *clause = isSgOmpWhenClause(node)) {
      ++when_clause_count_;
      checkSelectorSets(
          clause->get_context_selector_sets(), clause, true,
          SgOmpClause::e_omp_atomic_default_mem_order_kind_acquire);
      if (clause->get_variant_directive() == nullptr ||
          clause->get_variant_directive()->get_parent() != clause ||
          isSgOmpParallelStatement(clause->get_variant_directive()) ==
              nullptr) {
        fail("when clause lost its typed parallel variant directive");
      }
    }
    if (SgOmpMatchClause *clause = isSgOmpMatchClause(node)) {
      ++match_clause_count_;
      checkSelectorSets(
          clause->get_context_selector_sets(), clause, false,
          SgOmpClause::e_omp_atomic_default_mem_order_kind_release);
    }
  }
};

std::string withoutWhitespace(const std::string &text) {
  std::string result;
  result.reserve(text.size());
  for (unsigned char ch : text) {
    if (!std::isspace(ch)) {
      result += static_cast<char>(ch);
    }
  }
  return result;
}

bool syntaxExpressionsOmitJsonType(SgSourceFile *file) {
  const std::string json = Rose::AstJson::buildJson(
      file, Rose::AstJson::Checkpoint::PostOmpConstruction, file);
  const Rose::AstJson::AstFileRecord ast = Rose::AstJson::parseAstFileJson(
      json, Rose::AstJson::checkpointName(
                Rose::AstJson::Checkpoint::PostOmpConstruction));
  size_t name_count = 0;
  size_t source_count = 0;
  for (const Rose::AstJson::NodeRecord &record : ast.nodes) {
    if (record.kind != "SgOmpNameExpression" &&
        record.kind != "SgOmpSourceExpression") {
      continue;
    }
    if (record.properties.find("type") != nullptr) {
      std::cerr << record.kind
                << " serialized a fabricated semantic type property\n";
      return false;
    }
    name_count += record.kind == "SgOmpNameExpression";
    source_count += record.kind == "SgOmpSourceExpression";
  }
  if (name_count == 0 || source_count == 0) {
    std::cerr << "OpenMP syntax-type fixture did not produce both exact "
                 "syntax expression kinds\n";
    return false;
  }
  return true;
}

SgSourceFile *sourceFile(SgProject *project) {
  if (project == nullptr || project->numberOfFiles() != 1) {
    return nullptr;
  }
  return isSgSourceFile(&project->get_file(0));
}

} // namespace

int main(int argc, char **argv) {
  SgProject *project = frontend(argc, argv);
  SgSourceFile *file = sourceFile(project);
  if (file == nullptr || frontendExitStatus(project) != 0) {
    std::cerr << "OpenMP metadirective AST JSON fixture failed in the "
                 "frontend\n";
    return 1;
  }

  MetadirectiveValidator validator;
  validator.traverse(project, preorder);
  if (!validator.valid() || validator.whenClauseCount() != 1 ||
      validator.matchClauseCount() != 1 ||
      validator.beginDeclareVariantCount() != 1 ||
      validator.endDeclareVariantCount() != 1 ||
      !syntaxExpressionsOmitJsonType(file)) {
    std::cerr << "OpenMP context selector AST JSON round trip reconstructed "
              << validator.whenClauseCount() << " when clauses and "
              << validator.matchClauseCount()
              << " match clauses instead of one each\n";
    return 1;
  }

  const std::string unparsed = withoutWhitespace(project->unparseToString());
  const std::string shared_prefix =
      "device={kind(cpu,gpu)},target_device={kind(cpu,gpu),"
      "arch(\"nvptx\",\"amdgcn\"),isa(\"sse4\",\"avx2\"),";
  const std::string shared_suffix =
      ",uid(\"rex-device\")},implementation={vendor(score(7):gnu,llvm),"
      "extension(rex_ext_a,rex_ext_b),requires(unified_shared_memory,"
      "reverse_offload,";
  const std::string expected_when =
      "when(" + shared_prefix + "device_num(device_id)" + shared_suffix +
      "dynamic_allocators(device_id>0)),atomic_default_mem_order(acquire),"
      "rex_fast(rex_prop,nested(7))}:"
      "parallel)";
  const std::string expected_match =
      "match(" + shared_prefix + "device_num(0)" + shared_suffix +
      "dynamic_allocators(1)),atomic_default_mem_order(release),"
      "rex_fast(rex_prop,nested(7))})";
  for (const std::string &expected : {expected_when, expected_match}) {
    if (unparsed.find(expected) == std::string::npos) {
      std::cerr << "OpenMP context selector round trip lost ordered typed "
                   "properties\nexpected: "
                << expected << "\nunparsed project: " << unparsed << "\n";
      return 1;
    }
  }
  if (unparsed.find("otherwise(nothing)") == std::string::npos) {
    std::cerr << "OpenMP metadirective AST JSON round trip lost its otherwise "
                 "variant\n";
    return 1;
  }

  project->skipfinalCompileStep(true);
  AstTests::runAllTests(project);
  return backend(project);
}
