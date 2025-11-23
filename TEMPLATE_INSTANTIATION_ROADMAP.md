# Template Instantiation Implementation Roadmap

**Document Version**: 1.3  
**Last Updated**: 2025-11-23  
**Maintainer**: REX Development Team  

## Current Status
- Template declarations/instantiations are created in their namespace scopes (Option A) and reuse cached decl/inst nodes via `p_template_decl_cache`/`p_template_inst_cache`.
- Template-template parameters and packs are anchored to synthetic nonreal template decls; packs unparse as `template<class...> class C` and arguments expand as `Args...`.
- Alias templates now translate via `VisitTypeAliasTemplateDecl`, preserving template parameters and underlying types.
- Dependent template names keep qualified spelling in `globalQualifiedNameMapForTypes`, and unparsing uses that map for entries like `typename T::template rebind<int>`.
- The new `tests/nonsmoke/functional/CompileTests/Cxx_tests/rex_template_instantiation.C` is enabled in CMake/ctest and currently passes.
- Additional coverage in `ctest -R template`: `rex_nested_alias_template.C` and `rex_std_template_template_param.C` both pass in the current build.

## Recently Resolved
- Fixed duplicate `template<...>` headers in unparsed output by deduplicating template-class emission before printing headers.
- Normalized instantiation scopes and symbols to avoid global-scope anchors and mangling assertions.
- Improved mangling/unparsing for template-template arguments, parameter packs, and dependent names; ellipsis placement is now correct.
- Prevented duplicate top-of-file include emission by detecting include directives attached to declaration preprocessing info before running the fallback include replay.

## Remaining Gaps / Risks
- Need broader coverage: nested-namespace alias templates, partial specializations, non-type template-template arguments, and std headers interacting with cached decls.
- Decl registration from type-only translation relies on specialization visits; audit cases where specialization decls are skipped so `p_decl_translation_map` stays complete.
- Unparser still prefixes some instantiations with `class` (e.g., `class std::array`); compiles but is not a perfect round-trip.
- Fallback include replay now skips when includes are attached to declarations; still need to confirm behavior on files that truly lost preprocessing info so headers are preserved without duplication.
- Current rose output for `rex_template_instantiation.C` still diverges from the source: `using` aliases are rewritten as `typedef`, a placeholder comment precedes `rebind`, some spaces are missing (`rebind<int>rebound`, `C<Args...>value`), extra `class`/`::` prefixes appear on instantiations, member accesses gain spaces (`dep . data`), and literals are reformatted (`1.5` → `1.50000`). Round-trip parity remains the target.

## Next Actions
1. Add targeted CompileTests for nested-namespace alias templates, partial specializations, and std template-template params; run under `ctest -R template`.
2. Confirm `p_decl_translation_map` coverage for instantiations built from types without later specialization visits; backfill registration if missing.
3. Review fallback include replay on files that lack attached preprocessing info to ensure headers are restored exactly once.
4. Preserve source spelling in unparse for template-heavy cases:
   - Keep `using` aliases (avoid typedef downgrades) and drop placeholder comments.
   - Avoid injecting `class`/leading `::` on instantiations unless required for disambiguation.
   - Normalize spacing for dependent names, template args, and member access (`dep.data`, `C<Args...> value;`).
   - Keep literal text/precision as written (e.g., `1.5`).
5. Re-run `ctest -R template` and re-diff `rose_rex_template_instantiation.C` until differences are limited to acceptable formatting only.

## Success Criteria
- Namespace-qualified instantiations unparse in correct scopes without duplicated headers or stray template shells.
- Template-template parameters/arguments retain correct pack markers and anchored decls; mangling/unparse stays assertion-free.
- Dependent names (`typename T::template ...`) round-trip with correct `typename`/`template` tokens and without fallback concatenation.
- Added template-focused tests pass under `ctest -R template` without regressing existing suites.

## Options for Namespace Qualification
- **Option A: Build template decls in their natural namespace scopes (chosen)**  
  - Pros: Matches symbol tables and mangling; unparser naturally applies the right qualification.  
  - Cons: Requires careful scope construction for inferred decls and fixes to builder assertions.
- **Option B: Synthesize globals and force qualification in the unparser**  
  - Pros: Simplifies construction and avoids namespace scope churn.  
  - Cons: Diverges from real scopes, needs custom unparser hooks, and risks symbol mismatches.

Decision: Continue with Option A; keep Option B only as a fallback if namespace-scope construction blocks progress.
