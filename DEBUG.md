# Debug Notes

## Coupled Failure Group

- Reference investigation: `../ctest-failure-investigation-2026-04-17.md`
- The active bounce group is the coupled Clang frontend/template/unparser and
  OpenMP mapper shard, not isolated one-off tests. The fixes were reviewed
  together because earlier attempts bounced between issue160 explicit
  instantiations, issue203 hidden template surfaces, STL/astInterface template
  visibility, and mapper unparse spelling.

## Root Causes Fixed

- Application headers marked with `#pragma clang system_header` caused Clang to
  report explicit function-instantiation specializations at the header template
  pattern instead of the source statement. The frontend now keeps such
  application-header explicit instantiations source-relevant, recovers the main
  file `template ...;` statement when Clang does not expose it directly, and
  prevents namespace traversal from moving the recovered directive back under a
  hidden header namespace fragment.
- Semantic-only system template instantiations such as
  `std::allocator_traits<...>` were translated but structurally attached only
  under a canonical semantic namespace fragment that was not reachable by
  project subtree queries. The frontend now keeps the semantic scope while
  attaching the hidden instantiation through the reachable lexical namespace
  fragment, so AST queries see it without emitting stray system-header code.
- Same-source class-template redeclarations and hidden nested template surfaces
  could alternate between duplicate output and missing source-owned templates.
  The class-template path now distinguishes distinct source redeclarations from
  same-source placeholders and suppresses only the synthetic/duplicate surfaces.
- Explicit class-template instantiation directives are kept attached even during
  on-demand translation, while non-output nested/system specializations remain
  suppressed. This avoids losing real explicit-instantiation directives without
  reintroducing hidden `template<> class ...;` output.
- On-demand sibling materialization for application headers marked as system
  headers was incorrectly gated on `-rose:unparseHeaderFiles`. That let
  transformation tests translate only the referenced declaration from a user
  header and miss same-header record siblings that still need to remain
  mutable. The frontend now materializes application-header leaf siblings
  whenever one declaration from that header is pulled in on demand, including
  namespace/function-template/tag leaves without forcing whole-header unparse.
- Standalone template-variable declarations could end up without a reachable
  local symbol because same-name lookup was treated as sufficient even when it
  resolved to a different declaration. Template-variable registration now only
  accepts an existing symbol when it is already bound to the current
  `SgInitializedName`, so each translated template variable keeps its own
  symbol-table entry.
- OpenMP mapper dynamic loops were generating semantically correct but
  reference-unstable cast spelling. Mapper index and dynamic-loop conditions now
  preserve the expected opaque cast spelling while leaving generated artifacts
  in the build tree.

## Verification

- `cmake --build build -j$(nproc)` passed after cleanup.
- Focused unresolved pair passed:
  `ctest --test-dir build --output-on-failure -R '^(Cxx_tests_rex_test2025_issue160_system_header_instantiation_cpp|astInterface_rex_test2026_stl_vector)$'`
- Full coupled shard passed, 13/13:
  `ctest --test-dir build --output-on-failure -R '^(Cxx_tests_rex_test2026_template_features_cpp|Cxx_tests_rex_test2026_stl_map_cpp|Cxx_tests_rex_test2026_deduction_guide_cpp|Cxx_tests_rex_test2026_dependent_qualification_cpp|Cxx_tests_rex_test2025_issue160_explicit_class_instantiation_cpp|Cxx_tests_rex_test2025_issue160_explicit_struct_instantiation_cpp|Cxx_tests_rex_test2025_issue160_extern_class_instantiation_cpp|Cxx_tests_rex_test2025_issue160_system_header_instantiation_cpp|Cxx_tests_rex_test2026_issue203_template_qualified_private_typedef_cpp|astInterface_buildTemplateClass|astInterface_rex_test2026_stl_vector|astInterface_insertStatementBeforeFunction|omp_lowering_mapper_semantic_declare_mapper_target_update)$'`
- Final residual failures passed in isolation:
  `ctest --test-dir build --output-on-failure -R '^(Cxx_tests_rex_test2025_issue148_system_header_mutation|rex_test2026_astSymbolTable_template_symbols)$'`
- Related mirror set passed, 8/8:
  `ctest --test-dir build --output-on-failure -R '^(Cxx_tests_rex_test2025_issue148_system_header_mutation_cpp|Cxx_tests_rex_test2025_issue148_system_header_mutation|uninit_fields_cxx_Cxx_tests_rex_test2025_issue148_system_header_mutation_cpp|rex_test2026_astSymbolTable_template_symbols|normalizationTranslator_rex_test2025_issue148_system_header_mutation.cpp|singleStatementToBlockNormalization_rex_test2025_issue148_system_header_mutation.cpp|astInterface_rex_test2026_template_symbol_table|compiler_options_collect_comments_Cxx_tests_rex_test2025_issue148_system_header_mutation_cpp)$'`
- Expanded coupled shard passed, 15/15:
  `ctest --test-dir build --output-on-failure -R '^(Cxx_tests_rex_test2026_template_features_cpp|Cxx_tests_rex_test2026_stl_map_cpp|Cxx_tests_rex_test2026_deduction_guide_cpp|Cxx_tests_rex_test2026_dependent_qualification_cpp|Cxx_tests_rex_test2025_issue160_explicit_class_instantiation_cpp|Cxx_tests_rex_test2025_issue160_explicit_struct_instantiation_cpp|Cxx_tests_rex_test2025_issue160_extern_class_instantiation_cpp|Cxx_tests_rex_test2025_issue160_system_header_instantiation_cpp|Cxx_tests_rex_test2026_issue203_template_qualified_private_typedef_cpp|Cxx_tests_rex_test2025_issue148_system_header_mutation|rex_test2026_astSymbolTable_template_symbols|astInterface_buildTemplateClass|astInterface_rex_test2026_stl_vector|astInterface_insertStatementBeforeFunction|omp_lowering_mapper_semantic_declare_mapper_target_update)$'`
- Broad regression slice passed, 7137/7137:
  `ctest --test-dir build -R "rex|astInterface|testQuery|fortran|f90|f03|gfortran|OMPTEST_|OMPACCTEST_|OMPFORTRAN_|OMPANALYZE_|OMPVV_5_0_|OMPVV_4_5_|OMPVV_5_1_|OMPVV_5_2_|OMPVV_6_0_|omp_lowering_|OMPLOWERING_CPU_|OMPLOWERING_RODINIA_|^Cxx_tests_test2013_69_C$|^Cxx_tests_test2013_198_C$" -j$(nproc) --output-on-failure`
- Source-tree artifact check was clean:
  `git ls-files --others --exclude-standard src tests`
- Temporary debug scan was clean for the instrumentation used during this work:
  `rg -n "REX_DEBUG_EXPLICIT_FUNC_INST|REX_DEBUG_TEMPLATE_DUP|REX_DEBUG_TEMPLATE_DECLS|REX_TEMPLATE_DECL|\\[rex-return-range\\]" src DEBUG.md`

## Follow-Up Risk

- The validated risk now shifts back to future frontend work that touches
  on-demand application-header materialization, template-variable symbol
  registration, or explicit-instantiation scope repair. Re-run the broad slice
  above when those areas change so the coupled group does not start bouncing
  again.
