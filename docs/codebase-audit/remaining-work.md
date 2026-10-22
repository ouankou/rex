# Remaining audit and cleanup work

Acceptance is incomplete. PR861 publishes the checkpoint and CI repairs before
whole-audit completion. [Coverage](coverage.json) has
38 scopes in progress and three retained. Full regular, sanitizer, Memcheck,
relocated-consumer and portability acceptance remain open. Required PR CI passes
locally on frozen source794: core794 runs all10,763 selected tests, fragile795 runs
all584, and WASM797 builds, runs all eight smoke cases and assembles the site.
All three act jobs exit0; exact inventories and source/tool/dependency guards
pass. Final packaging changes only status documents, touched-line formatting and
the formatting ignore entry preserving the diagnostic specimen's exact bytes.
Three user-approved exact negative contracts retain unresolved defects in issues
[862](https://github.com/ouankou/rex/issues/862),
[863](https://github.com/ouankou/rex/issues/863) and
[864](https://github.com/ouankou/rex/issues/864); these conversions are not compiler
fixes.

The verified batch preserves exact source ownership, initializes JSON brace/copy
flags, and establishes isolated semantic visibility at its publication invariant.
Unchanged declaration-moving goldens, native syntax and runtime checks pass. The
regex control passes783 in51.06 seconds versus its prior51.56 seconds. See
[final review](final-review.md) for the distinction between PR CI and full audit
acceptance.

Source733 folds typed character LEN expressions before constructing semantic types
and emits character literals with their typed KIND selector. The negative contract
and26 adjacent checks pass. Source734 adds a positive Unicode runtime specimen;
its original Flang execution and all four REX stages pass. These31 distinct focused
checks preserve existing AST/copy/JSON assertions and rejection contracts. No
current-source full regular, sanitizer or Memcheck pass is claimed.

OpenMP source655 fixes attribute execution with REX-owned token capture, a typed
pragma source kind and normal OpenMP construction. All10 focused checks656,
three pre-OpenMP JSON/runtime controls658 and eight adjacent controls659 pass.
Source685 fixes both existing C same-name variant failures using typed function
lookup frames before definition construction. Base references keep their exact
source entity; pragmas remain opaque to Clang. All19 affected CTests pass687.
Eight permanent source/JSON/native-runtime and strict rejection controls pass689.
Source701 completes diagnostic initialization from parsed driver/frontend options
and registers opaque OpenMP attributes without enabling native OpenMP semantics.
Warnings retain their requested severity; verified expected errors still forbid
AST construction. All 34 focused checks703 and three opaque source/compile/runtime
checks705 pass, with exact inventories and input guards. Existing native/JSON
attribute checks now require `-Werror=unknown-attributes`; unrelated attributes
still hard-fail. No existing expectation is weakened.
C++ variant declaration/default/constexpr handling and effective-selector identity
still need review; the C-specific result does not close those obligations.
The metadirective failure was a PR integration regression, not an external
limitation. Broader audit acceptance remains open.

The template negative first requires stock Clang to accept the unchanged fixture,
then requires abort134 with its exact source-binding diagnostic. The unchanged
contract translator's positive assertions remain available for restoring that
test. Image-control AST, translation, graph and token checks stay positive; native
compilation is a separate exact negative for original and emitted source with
-fcoarray. All22 focused tests pass741 without skips. That run also catches and
fixes redundant default-kind prefixes from733; existing source-format expectations
remain unchanged. Stock LLVM22 is unchanged. Full regular and memory gates remain due.

This file lists remaining obligations. Completed-batch history, witnesses,
failures and source-specific manifests remain in [findings.md](findings.md);
public API/layout/schema changes remain in [migration.md](migration.md).
Focused results, compilation and interrupted jobs do not close whole scopes.
AUD2290 removes the build script's substring-based path deduplication with two
passing focused contracts. The24 retained `cmake/` files have a recorded source
review; configuration/platform integration and full gates remain pending.
The frozen CI inventory794 has21,419 entries, including tracked/untracked
initialized-submodule inputs and ignored build/test inputs. Each job retains its
exact source, runtime and toolchain manifests. Evidence798 changes only two status
documents; packaging799 additionally records token-preserving formatting and the
diagnostic specimen's formatting ignore entry.
Current serialized schema is 251; historical schema numbers in batch records
identify those checkpoints, not current acceptance.

Schema249 adds typed expression-list source form; its nested-brace native/JSON
regression and exact required-fields guard pass. The approved native contract
batch521 and semantic-reference contract537 are applied. The anonymous-storage
extension556 is also applied under the existing source-view contract, with exact
typed ownership and direct/parent rejection checks; eight focused controls pass594.
Source617 preserves written lambda parameter types during frontend construction
and retains exact qualification contexts for template scopes and lambda defaults.
Inherited-constructor forwarding keeps its typed semantic role and hard source
rejection. All22 focused CTests618 and both original regex checks619 pass; three
lambda checks also pass through real JSON reconstruction620. Source, runtime and
toolchain guards match; no skips are added. Interrupted selection610 is not a pass.
The later original-failure reconciliation finds two additional REX defects:
standalone type queries omitted the owned inline definition from qualification
analysis, and a late name scan rejected valid variable-template specializations.
Source634 prepares the exact typed definition and removes that invalid scan.
All23 affected checks635 and three real JSON reconstruction checks636 pass,
including the six older failure candidates and unchanged declaration/cycle
rejection controls. Actual WASM CI644 passes433 build steps, all eight declared
smoke cases and site assembly on source637. Actual fragile CI646 rebuilds and
installs native consumers and passes all584 tests on the same source. Exact
executed inventories and final source/tool/submodule guards pass. Core CI and full regular, sanitizer,
Memcheck and installed-consumer gates remain open, including the documented stock
LLVM/Flang blockers below.

The latest native batch corrects Fortran INCLUDE comment boundaries and
contained-procedure declaration order, collapsed-loop temporary ownership,
detached mapper/reduction loop construction, and consumed task retirement.
Source-specific focused checks pass, including all eight remaining Rodinia
checkpoint cases and the deterministic task checkpoint. These are not full gates.
Fortran task capture now preserves typed declaration bindings through private
substitution and outlining. The unchanged scalar oracle produces1..10, and its
complete AST JSON checkpoint passes. Readers validate typed capture references
and resolve their dependencies before constructing a task. Single lowering now
retires its consumed directive instead of leaving clause references behind.
Nine focused schema/cache/single/task checks pass (`build-audit/pr-round172-*`);
four C checkpoint/copy/rejection controls pass (`pr-round175-*`). Broader task
sharing/storage variants and instrumented acceptance remain open. The active
Fortran pipeline projects OpenMP out before Flang semantic analysis; the new
construction uses exact typed REX clauses and declaration ownership.

Predefined-name sharing decisions now use the frontend's typed identifier flag;
the redundant spelling/provenance bypass is removed. Sharing queries read typed
clauses directly, without a string-keyed cache, clone hook or mutation timestamps.
Fourteen focused checks, including five unchanged golden comparisons, pass
(`build-audit/pr-round155-*`). These historical checks do not establish a full gate.

The approved521 batch applies the copied-function ownership/type/retirement
contract178. Source and copy each own their primary parameter list.

The schedule failure was a deterministic reader dependency cycle, not an
intermittent baseline abort:
the DO waited for its own OpenMP owner. Removing premature parent dependency
and adoption passes both schedule and exampleA41f checkpoints plus three strict
reader/copy/runtime controls. Evidence: `build-audit/pr-round88-*` through
`pr-round90-*`. Full gates remain open.

Clang initializer construction now retains native aggregate types through
temporary binding, with distinct typed return/new initializers and a typed
`std::initializer_list` object around its backing array. The obsolete
functional-cast repair is removed; 21 initializer/source/copy/JSON controls pass.
Pragma dispatch now inspects the active native lexer buffer without consuming
or backtracking Clang's tokens; seven opaque/macro/variant/rejection controls
pass. Evidence: `build-audit/pr-round103-*` and `pr-round111-*`.
All 512 historical OpenMPVV failures have been revisited; 511 have passing
observations across different frozen batches, not one current-source gate.
Native C++ parallel/for attributes now construct typed directives and clauses.
The original sequence-attribute case passes; both its original and emitted files
compile and execute with stock Clang22. Pragma-only text extraction cannot prove
valid attribute output, so native and JSON-restored attribute regressions also
compile and execute the emitted source, asserting real parallel/private behavior.
The duplicate for/for-simd output paths are removed. Thirty-nine focused CTests
pass (`build-audit/pr-round192-*`, `pr-round195-*`), including unchanged pragma
and SIMD golden comparisons. Other native directive/clause kinds remain explicit
hard errors; broader attribute coverage and full acceptance remain open.
The new-file outliner now copies its exact call prototype and existing typed
class forwards in source declaration order, before copied class bodies use them.
The outliner batch passes 54 focused CTests (`build-audit/pr-round257-*`).
These include all 28 selected original outliner failures, six unchanged
hard-rejection controls, native/JSON
copying, genuine local-type capture, friend linkage and compile/runtime checks.
Independent copy roots distinguish semantic scope plans from structural owners.
Hidden-type queries follow the receiver's typed template binding; friend families
enter dependency collection before copying. Definition relocation preserves the
destination family, and written prototypes bind its exact definition during
construction. The late declaration-chain rewrite and invalid-definition-to-null
substitution are removed. Static-member checks retain separate written and
semantic views, and the approved schema-248 required-fields guard passes.
A separate native compile found that copied-library C prototypes followed class
friends, causing four linkage and two protected-access errors despite successful
generation. Namespace prototypes now precede the exact friend class boundary,
reusing the existing class-forward and preprocessing-prefix routines. Stock
Clang 22 compiles both emitted files, and a new two-test CTest fixture generates
and compiles the actual library (`pr-round258-*`, `pr-round261-*`).
Variable-renaming traversal now stops at nested function declarations: local-class
methods are analysed separately instead of recording each definition twice.
All 19 outstanding cases and five existing lambda/basic controls pass
(`pr-round267-*`); the duplicate-definition assertion remains unchanged.
Fortran comment emission and wrapping no longer reclassify every dollar-prefixed
comment as a directive. Stock Flang 22 accepts the unchanged free/fixed annotations,
unknown sentinel and inactive OpenMP controls. All eight outstanding Fortran
outlining cases, 34 directive/comment controls and four fixed-form outlining
controls pass (`pr-round271-*`, `pr-round275-*`, `pr-round276-*`). Typed directive
contexts and all 25 selected hard-rejection controls remain intact. The approved521
batch applies the lexical-helper contract272. No whole-suite pass is claimed.
The latest frozen Fortran batch passes 86/86 (`pr-round319-*`), including all 59
selected original imported-suite failures and the schema-248 corruption contract.
Statement functions preserve exact implicit canonical ownership; intrinsic
interface references use Flang's characterized signature and actual symbol
identities. Procedure pointers stay out of the implicit direct-function producer.
JSON collection and reading share exact borrowed POINTER-owner validation, and
typed EXTERNAL declarations compare their written result with the semantic
function result. Unchanged original, emitted and JSON-restored programs pass
stock Flang 22 execution checks (`pr-round295-*`, `pr-round303-*`, `pr-round308-*`,
`pr-round318-*`). The wrong output-path attempt306 and denied directory setup313
remain invalid evidence; the genuine collection/reader failures314/316 are fixed.
The approved521 batch removes the duplicate semantic registration284; the dedicated
wide-literal contract remains registered. The MPI module closure passes 12/12.
The unchanged wide-integer native/JSON runtime oracle passed in round199.
The AST-interface batch fixes modifier-cache bucket identity, indexed partial
specialization parameter-list retrieval through Clang's typed API, and anonymous
Fortran label registration. Unchanged unique-name, tuple/vector/variant,
function-chain and C/Fortran label checks pass (`pr-round327-*` through
`pr-round349-*`); named-label collision and null-child rejection remain strict.
The approved521 batch applies class argument identity330, typed deduction336,
cast rejection337, dependent-alias ownership346, trait/callable contracts359 and
the verified inlining count369. Their source-specific checks do not close the
broader API, transformation or memory gates.
All nine STL argument-normalization cases pass361. Range-for insertion uses the
existing exact body-owner transaction, default-use references qualify in their
use-site scope, and trivial-argument classification reads the typed default-use
value. Eight source-qualification/default-owner controls, including JSON, pass372.
The duplicate assignment-lowering implementation and declaration are removed;
the sole caller uses the retained checked ownership boundary. These batches have
different frozen sources and are regular checks, including the sanitizer-labelled
control; instrumented acceptance remains open.
Range-for body detection and single-statement normalization now use the existing
checked body-owner transaction. The duplicate classic-for implementation is
removed. A strengthened existing fixture fails before the fix and passes after
it; all eleven focused controls pass377. Original and emitted programs compile
with stock Clang22 and both return the expected19 in378. Twelve OpenMP controls
pass381: two compare native CPU behavior; ten check generated Rodinia source
shapes. These are regular checks, not instrumented or GPU acceptance.
The three approved early-rejection migrations pass396. Four further diagnostic
mismatches are verified as hard aborts400 and corrected without changing fixtures
or compiler guards; eleven nearby controls pass401. The approved default-argument
fixture migration uses a typed default, proves its binding to its declaring
function, and requires construction to reject binding it to another function.
It and four unchanged UDL siblings pass403; the obsolete file-info flag setup is
removed. Every negative still requires status134 and one matching diagnostic.
Labelled WHERE/ELSEWHERE and assignment CFG children now use exact typed execution
roles rather than metadata traversal positions. Incoming computed-GOTO edges now
include main programs; one function-local typed query replaces the global
memory-pool workaround and separate branch scans. Original fixtures and edge
assertions stay unchanged, with the existing labelled WHERE-chain fixture added
to both drivers. All252 registered Fortran-90 static/virtual CFG checks pass on
the same frozen source and compiler bytes414/415;416 verifies exact registry
coverage. Eighteen comment-collection compiler cases pass406 on the preceding
frozen source. Image-control translation407 still fails at the documented
stock-Flang coarray gate; its graph/token stages did not execute. No exclusion,
flag substitution or native-lowering waiver is added.
VLA parameter bounds now retain Clang's typed lvalue conversion during frontend
construction; the strict source/semantic parameter validator is unchanged.
Fifty-three of54 selected OpenMP parsing/output checks pass422/425; the ordered
reference omitted four directives present in the unchanged source. The approved521
batch applies proposal424. Stock Clang confirms all six directives and identical
130-line original/emitted output with two and four threads423.
Normalized blocks now retire their pending registry entry in their destructor.
Trace427 proves the old entry survived destruction and pooled-address reuse.
The strengthened existing lifetime test fails before the fix428, then it and
fourteen C lowering checks pass431. Four include native compile/runtime comparisons;
ten legacy checks verify translation only, despite their compile-only names.
A separate native check432 exposed guard deletion in full_verify.c: disabled
SERIAL_SORT code became active, while an endif appeared in the outlined library.
The stripping helpers, string-based directive guesses and unbalanced-directive
repair pass are removed (242 lines). Existing typed preprocessing transfers now
retain the complete surface. Two direct native CTests require host and outlined
compilation after the existing producer; all three stages pass437, and seventeen
disjoint nearby controls pass438 on the same frozen source/compiler bytes.
Native439 rejects both archived pre-fix outputs and compiles the original/host/
outlined forms of endif.c and endif3.c. Existing specimens, references and
assertions are unchanged; no replacement repair or script is added. Full
acceptance remains open.
Seven C++ lowering cases pass442; four emitted CPU files also compile with stock
Clang22 in443. The mapper CUDA output still needs device-toolchain validation.
Unused reduction-size expressions retained references into retired ASTs; removing
their construction fixes the unchanged reduction JSON checkpoint445/446.
The frontend now constructs omitted DO steps with its existing typed default-
integer helper450. Scheduler increments and chunks use exact runtime-width
storage453; GDB452 proved the old 64-bit wrapper read 32-bit actual arguments as
146028888065 and4294967297. Taskgroup bodies detach before transfer456, and
ordered CYCLE insertion promotes a single-line IF to its typed block form458.
All24 selected Fortran lowering cases now have passing observations across
453/456/458/465. The outliner validates declared parameters against the signature
builder's exact canonical parameter types465; shared.f and the final three cases
pass466. Eight C/C++ outlining controls pass467. Twenty C/C++ cross-checks pass460
on source458; these are different frozen compiler revisions.
Temporary frontend parameter lists now retire through the existing checked
transient-AST transaction470 instead of raw deletion, which left stale source
provenance at reused pool addresses469. All six declaration-moving producers
pass471/473. Both inputs, six emitted files and six references compile474; each
emitted/reference IR pair differs only in its two filename headers475. This does
not instantiate every library template. The approved521 batch applies the six
golden corrections475; all18 selected producer/diff/syntax checks pass536.
Seven frontend ownership/JSON controls pass476/478; the eighth remains at the
documented stock-LLVM22 instantiation source-binding blocker, unchanged.
The six remaining Fortran dataflow inputs are already classified as invalid by
the compile suite. Its six strict rejection tests pass479, and stock Flang rejects
all six481. The approved521 batch applies proposal480, reusing the existing strict
negative inventory/diagnostic helper. All six dataflow checks pass536; no compiler
rejection or assertion is weakened.
Virtual/member-pointer lookup now consumes typed auxiliary class methods as well
as lexical members. Sixteen compiler/callgraph cases pass486. The conversion
control caught duplicate canonical targets487/489; the shared inventory now
returns each canonical method once490. The original failure and both conversion
controls pass491. Five legacy dispatch controls passed487 before that correction.
Existing fixtures and assertions are unchanged; full acceptance remains open.
DO-WHILE CFG children now use their exact body/condition roles instead of the
generic scope-child ordering495. The unchanged random-library fixture and its
static control pass496; the other forty selected CFG cases pass497. All51 have
passing observations across492/495, not one frozen full gate. Interprocedural
cases without `main` exercise frontend processing only.
The shared Fortran specimen-flags helper now supplies the unsigned fixture's
required `-funsigned`; its duplicate literal-runtime flag selection is removed505.
All ten affected positive/native/runtime/strict-negative and remaining Fortran
checks pass. Generated commands differ only by that flag on the failing positive
field-traversal check. The18 C++ field-traversal cases passed501; these ordinary
CTest results do not establish Memcheck coverage.
The approved521 batch applies proposal509's exact unnamed-template-parameter
ownership checks and strict negative contract. Regex `--all` advances to the
separate anonymous-storage source request recorded above.
The existing stock-Flang KIND4 substring blocker reproduces498 before REX runs.
The six deferred explicit-instantiation consumers use a different fixture from
AUD2145; grouping them with its alias-rebinding blocker was incorrect. All six
pass unchanged512. Ordinary regex translation also passes514; its separate `--all`
failure remains open. The superseded static-member source-projection proposal44
is already implemented and tested249. The approved521 batch applies the retired-
file lifetime proposal518.

The approved521 batch applies the taskgraph diagnostic migration; its strict
early-rejection comparison passes532.

Template forwards now emit typed written arguments, and obsolete template-name
rewriting is removed. Token mapping preserves the enclosing source anchor through
owned source-type containers. Detached generated variables acquire their physical
owner at actual attachment; the header-mutation and approved Fortran lifecycle
checks pass. Borrowed typedef targets bind during copy construction instead of
being cloned by late fixup. Pointer/reference factories use exact base-type caches
without redundant mangled-name registration; seven copy/cache/lifetime controls,
including the original namespace-copy failure, pass. These are source-specific
focused results. The approved521 batch applies the interface coverage caller
correction. Its complete focused run passes538 in150.56seconds; earlier interrupted
attempts remain incomplete evidence.
Evidence: `build-audit/pr-round116-*` through `pr-round150-*`.

PRE's three current generators pass, but all three saved-output comparisons
still fail. Stock LLVM22 diagnoses an uninitialized cache read in pass3's saved
reference. Current output retains all10 caches and passes the existing native
five-dimension oracle under Memcheck. The concrete three-reference correction
is awaiting explicit user review; references remain unchanged. Finish general
placement, target arithmetic, floating evaluation and output contracts without
masking failures. Normal-completion counts do not establish safe speculation
before a call, and callee names do not establish purity.
FD test3 still differs in three parentheses despite byte-identical native IR;
its reference change has no approval. Aligned(0), AUD2145 and memory failures
remain open below. Completed-batch history and failed/invalid attempts belong
in findings.md, not current acceptance.

## Frontend, readers and copying

- Finish canonical template-consumer and builder review. Preserve independent
  written, complete semantic and selected-specialization deduction owners.
  Include nested/source-forward placeholder variants, duplicate-input and
  allocation-failure transactions; focused builder checks do not close them.
- AUD2288's C11 prototype-refinement abort is fixed with typed prototype kind
  and per-use reference types; late decay-type reconciliation is removed.
  Focused construction/copy/reader, C11 checkpoint/runtime and lifecycle memory
  checks pass. Schema242 also preserves implicit-declaration identity through
  readers/copying, and the GNU89 implicit-call JSON/runtime witness passes.
  Complete broader temporal C bindings and remaining function-family variants.
  These focused results do not establish full acceptance.
- Complete recursive declaration/type/symbol/scope/template/expression
  construction in Clang, Flang and both JSON readers. Validate dependencies
  before allocation/publication; retain hard canonical and ownership checks.
  Remove string/name recovery and late body/signature/parent repair through
  typed construction. Do not substitute Unknown/default types or invent scopes.
  Finish source-position classification transitions and publication; literal
  spelling/target domains, unsigned warnings, GNU backend execution and nonempty
  compiler arguments; pragma scalar-statement/declarator/control/type-owned roles.
  AUD2298 shares early typed declaration-argument conversions and removes missing-
  type address inference. Its canonical consumer now uses substitution arguments;
  complete remaining written/semantic consumer and construction review.
- Finish canonical parameter/prototype identity, lexical/source-type ownership,
  TypeLoc classification, Cray type/rank validation, alias redeclarations,
  hidden friends, specialization families, external-peer metadata and
  cross-translation-unit identities. Review source-to-semantic suppression,
  provenance selection, source/semantic parameter adjustment, deferred bodies,
  reparsing, recursive transfers and public synthetic factories.
  AUD2261's shared parameter-count preflight passes focused checks; complete the
  remaining recursive ownership contracts. Do not repair alignment in the unparser.
  Review the separate friend and prototype parameter producers, remaining
  local-requires variants, copying and rollback; AUD2263 covers the general
  signature path and AUD2264 covers four local-requires adjustment cases.
- Preserve typed exception specifications: written operands, deferred
  source/pattern bindings, exception-pack positions and canonical distinctions.
  Finish parameter/body/prototype-copy ordering, dependent structural identity,
  non-prototype exception-type ownership and nested class/lambda construction.
  Preserve borrowed semantic signatures and owned operands through interning,
  copying and deletion. LLVM's canonical None projection does not prove throwing
  behavior; unfinished nodes must remain private.
- Finish value-sensitive template-argument conversion and representability,
  canonical declaration dependencies, borrowed/unpublished operand validation,
  recursive constraints and conditional explicit expressions. Review nested,
  dependent, pack/default expansion, variable-template, static-member and
  immediate-function contexts, semantic argument levels and written header
  pairing across redeclarations. Review semantic-to-written conversion when
  native source argument records are absent; never replace a wrong written
  argument with a semantic one.
- **AUD-2145 remains blocked on stock LLVM22.** EOF replay can bind a later
  alias, producing written `char` versus semantic `long`. AUD-2173 rejects this
  before malformed REX arguments are built, but the valid input is still rejected.
  Top-level parser callbacks occur too late for declarations nested in a
  namespace. The user approved an exact negative contract, tracked in
  [issue863](https://github.com/ouankou/rex/issues/863). Keep the fixture, original
  structural assertions and early invariant; restore the positive invocation when
  declaration-time typed capture works. The LLVM callback draft stays unapplied.
- Complete function/signature scope publication, external-peer attachment,
  recursive construction groups, native-builder metadata and allocation-failure
  transactions. Review source-owned types, per-position reference APIs, repeated
  wrapper identities, auxiliary-to-body transfers and generic external codecs.
- Complete lambda editing/deletion, shared/external partial-copy recipes,
  canonical auxiliary-owner publication, project/cache retirement and borrowed
  edit state. Review source/pattern/header queries, terminators, macro/tag/template
  owners, include occurrences/materialization, spelling rewriting and recorded
  versus native identities. Preserve exact deletion boundaries and snapshot
  timing; uniform weak canonicalization must not collapse invalid paths.
- Finish exact symbol lookup keys, case-insensitive equivalence, preference order,
  ordinary-symbol/alias bases and source-order template roots. Remove preference
  retries and reconstructed names by fixing construction. Retain the separate
  installed source-line query API and its live project-isolation/invalid-input
  contracts. Review coroutine owners, token/suffix bindings and initializer roles.
- Finish whole-reader allocation/publication/rollback transactions. Distinguish
  borrowed declarations from fresh roots, restore displaced canonical entries
  after rejection and retire fresh indexes. Cover independent registries,
  comparison/replacement, array shapes, cleanup allocation and token/preprocessing
  faults. Validate graph and borrowed dependencies before metadata allocation;
  metadata preflight alone does not establish complete operands.
- Retain decoded typed fields through construction rather than decoding again.
  Review implicit name conversions, external module/class/function identities,
  inline type-owned/nonreal/reference paths, generic child construction,
  branch-specific fields, constructor-only publication, nested operands,
  clause/list families, property-value identity, scalar verification,
  array-initialization groups, symbol-basis indexes and cross-table aliases.
  Complete 32-bit index/span runtime validation and allocation-fault coverage.
- Complete copy ordering: declaration/signature before defaults/expansions;
  owner before parameter/field/prototype references; source syntax/scope before
  bodies. Dependent character-pointer signatures now pass reader, project-copy
  and emitted-runtime checks using early typed parameter scopes; broader copy
  and memory acceptance remain open. Review direct destruction, imported shared
  identities, cross-project caches, strict checkpoint retirement, low-level
  frontend entries and recorded
  block lifetimes. Complete broader dlopen/dlopen2/outlining copying beyond the
  focused triangle/polygon passes; retain canonical/ownership assertions.
- Finish structured-binding dependent/pack/nested variants, mutation, positive
  partial-reader contracts, shared/external ownership and cleanup. Typed values,
  aliases, holding storage and temporary owners now exist (AUD-2203–2215);
  they do not establish runtime cleanup or general copying/rollback correctness.
  Review general bit-field, pragma and other child/source-role mutation,
  constructor adoption, replacement, copy/publication and rejection transactions.
- Complete Flang DATA/slash initializers, DO/BLOCK DATA/FORALL and retired-variant
  consumers, including nested/masked/statement/named forms. Finish label-basis,
  CFG/clause/header/boundary dependencies, body construction and rollback;
  public name-only type queries, source-type factories, derived-modifier ordering
  and derived-body completion in both readers. Complete outlining result-type
  dependencies, source bindings, metadirective transfers, nested/empty INCLUDE
  tokens and comment placement/lifetimes. Stock Flang22 KIND4 substring fails
  before REX; dynamic descriptor/section/indirect layouts and typed substring/
  inquiry construction remain open, with no waiver.

## Control flow, analyses and transformations

- Complete constant-folding metadata/edit/copy contracts, target arithmetic and
  source provenance. AUD2285 removes redundant retirement scans; AUD2286 handles
  statement/expression consumers and root-result ownership. Five consumer/API
  forms, targeted Memcheck and two changed-program UBSan passes do not close
  the folding or broader memory scope.
- Complete PRE recursive/floating/constructor effects, GNU selection consumers and
  statement-expression/early-exit evaluation, other loop families and general
  declaration-group/type-owned transfers,
  path initialization and output contracts. AUD2278–2280 cover typed intrinsic
  operand counts, single/variable-group for-initializer placement and complete
  replacement absence-node publication. AUD2281 covers ordinary IF/WHILE/FOR
  counts and typed constexpr/consteval arm selection. Focused
  operand-prefix, zero-trip and nonreturning controls do not close these scopes.
  AUD2282 retains GNU choose selection through frontend construction, copying
  and both readers and fixes unselected-arm hoisting. Complete dependent/edited/
  type-owned variants and other effect/analysis consumers. AUD2283 covers virtual
  CFG entry/exit, reciprocal arm edges and condition isolation for known and
  dependent selections; remaining ABI consumers
  and whole gates are unvalidated.
  AUD2284 also preserves choose conditions and discarded arms during PRE
  replacement. Both known selections pass through the public single-expression
  entry; broader unevaluated/replacement contexts and native gates remain open.
- Complete potentially throwing call edges, ordered handlers/rethrows,
  constructor/function-try completion, cleanup execution and scope/lifetime
  effects. Explicit-throw and catch-scope fixes do not establish general
  exceptional analysis. CFG lexical exit lists and graph path maps do not
  supply complete runtime destruction sites or continuations.
- Finish IF/WHILE condition-declaration scopes, FOR dependencies, range loops,
  control-body publication, typed runtime arms and source-only consumers across
  CFG, reference effects, dependence, locality and emission. AUD2296 removes
  redundant IF condition splitting; focused binding/lifetime/path checks pass.
  AUD2297 preserves ordinary expression-IF body identity. Declaration-condition
  continuation/static/label identity and general declaration/loop lowering remain
  open. Cover parameter-entry
  and new-array bounds, sizeof/VLA runtime roles, unevaluated noexcept/alignof/
  dynamic typeid, aligned allocation, argument evaluation alternatives, computed
  goto, continue, const/reference initialization and literal/type/operand domains.
- Finish observer/borrowed-registry/iterator lifetimes, cloned subscriptions,
  private-root allocation-failure cleanup, supported/nonnumeric loop domains and
  split-domain refinement. Verify actual fusion/distribution/slicing/hoisting
  across retained blocks and control boundaries, generic/Fortran body wrapping,
  lexical groups, multiple declaration roots, synthetic exits, nested/reversed
  domains, relational runtime, dynamic strides and overflow boundaries.
- Complete physical access layouts, target address spaces, field identities and
  offsets through frontend construction, copying and both readers. Whole-object
  pointer size is not pointee/element layout; mode, host sizeof and guessed width
  tables are insufficient. Preserve exact native Clang/Flang storage facts before
  Sage construction. Finish dynamic/indirect layouts, value invalidation and the
  independent C/C++ OpenMP value producer; OpenACC captures do not establish it.
- Finish VFA/call-graph reference/temporary/subobject/context precision,
  conversion provenance and base adjustments, implicit constructor scheduling,
  variadics, receiver bindings, opaque/external/exceptional effects and edited-
  project reruns. Preserve may/must-write distinctions and weak updates for
  ambiguous storage. AUD2294 gives the reverse-call cache typed project ownership
  and fresh copy state; broader index validity and analysis acceptance remain.
  Complete general def-use and liveness review.
- Complete persisted call-graph legacy-format/publication contracts, negative
  edge-flag evidence, process-local internal-linkage IDs, record bounds, writer
  transactions, cross-TU provenance and saved compile-context coherence. Fresh
  records still lack saved directory/command values. Rebuild installed clients
  of changed edge maps, alias/binding APIs and analysis layouts.
- Finish outliner provenance clearing and late AstPostProcessing, inliner effects,
  lifetimes and borrowed/extern identities, declaration/typedef publication,
  token/declaration groups, detached/SIMD copies, executable SIMD emission and
  deletion performance. Cover generated peers, mixed groups, independent cross-
  file correspondence, Fortran exports, conditional annotations and multi-output
  inline-definition equivalence. AUD2295 preserves the original body scope when
  lowering a short-circuit do/while condition; its focused runtime, Memcheck and
  UBSan checks pass. Complete direct public DO-WHILE rewriting and
  finite-differencing eligibility, alias/volatile/lifetime proofs, control flow,
  cache/rule ownership and installed APIs. Generated-program memory passes do
  not establish compiler memory correctness.

## Construction and executable semantics

- AUD2303 now preserves declaration-condition predicates in typed IF/WHILE/FOR/
  SWITCH owners, CFG, copying and both readers (schema243). Scalar/class, JSON,
  token and corruption-rejection witnesses pass; the native required-fields
  Memcheck has zero errors/lost/suppressed. Audit manual builders, mutation and
  rewriting clients against the separate source/predicate edges, plus dependent
  variants and installed consumers. This does not resolve cleanup execution or
  establish full regular/sanitizer/Memcheck acceptance.
- AUD2304 adds declaration-then-predicate execution to legacy IF/WHILE/FOR CFG
  blocks. AUD2305 migrates dependency/locality consumers to explicit complete-
  condition queries while preserving generic subtree-only analysis. Scalar/class,
  compile-time, phase-separation and unknown-effect controls pass focused checks.
  Review remaining clients, manual mutations and installed consumers.
  The short-circuit pass still splits initialization into assignment and clones
  declaration-IF bodies; preserve const/reference/class initialization, full-
  expression lifetimes, static/label identity and incoming jumps when replacing it.
- Finish constructor field targets, ordinals/order, source projection,
  materialized defaulted bodies, linked inherited-owner copying, fixed/variadic
  parameter identity, complete/base-object context propagation and interprocedural
  scheduling. AUD2292 adds raw/filtered virtual-base construction guards; complete
  lifetime execution remains open. Do not invent flattened targets. Complete aggregate
  entries, array fillers, initialized-union identities, per-use defaults and
  external-header codecs; elision must bypass trailing default arguments too.
- Replace the installed lexical destructor annotator with typed cleanup execution.
  AUD2291 removes inactive prototype parameters from CFG scope exits; actual
  procedure parameters and local exits pass the focused contract and Memcheck.
  Generated destruction calls carry the typed implicit-destruction marker and
  remain available to AST analysis. The unchanged lifetime compile/runtime and
  `icg_2` controls pass. Preserve the trace oracle and installed API while
  completing cleanup execution. Cover exact local,
  parameter/result/temporary bindings, full-expression groups, lifetime extension,
  conditional creation, arrays, exception paths and complete/base-object roles.
  Body-local cleanup must precede member/base cleanup. NRVO candidate identity
  and selected destructor metadata do not establish execution sites.
  Structured-binding aliases are not separately constructed objects; cleanup
  must use typed holding storage, binding values and temporary owners.
  A trigger statement cannot distinguish while-loop exit from iteration cleanup;
  preserve both paths and continuations. Do not freeze a native CFG snapshot that
  becomes stale when the REX AST is transformed.
- Finish cleanup completeness, physical field mutation, fresh-reader rejection
  cleanup and general ownership/copy/deletion transactions. AUD2293 rejects
  foreign-owner signatures in temporary and subobject bindings without borrowing
  a base-object role. Preserve selected nontrivial destructor completeness,
  exact trivial-base classification, reverse
  member/direct/virtual-base order, union rules and typed storage duration.
  Explicit destructor calls remain ordinary expressions. Review return mutation,
  copy-reference publication, special-member availability, cv/constraint/abstract/
  incomplete/dependent constructibility and semantic deletion versus syntax.
- Complete template storage, attributes/access, constexpr written versus semantic
  qualifiers, recursive publication and canonical/source ownership. Review live
  outliner/PRE consumers, GNU cleanup-attribute semantics and remaining
  special-member inference.
- Finish property/pseudo-object getter/setter identity, evaluation order,
  opaque-value bindings, transformations and installed consumers. Review CUDA
  asynchronous/device-clause evaluation and operand alternatives: imported
  `launchBounds.cu` retains a builtin-property failure and host third-operand loss;
  a device-only SM90 pass does not establish host fidelity.
- Resolve the supported-language boundary for inherited `aligned(0)`, rejected by
  stock LLVM22. Keep the removed zero-to-one substitution and OpenCL aliases
  removed; do not rewrite input or mask the failure.

## Whole-codebase review

- Close each coverage scope through source/dependency review, including unbuilt
  files, disabled branches, generated references, virtual dispatch, installed APIs,
  initialized submodules and alternate configurations. Prove callers absent
  before deletion. Complete ROSETTA constructor/copy/scalar/list metadata review;
  removed helpers do not establish whole-grammar correctness.
- Review CMake scans, namespace-policy enforcement, package/install metadata,
  build helpers, CI, utilities, tools, examples, tutorials, documentation, browser
  support and runtime fixtures. Ignore legacy autotools inputs as instructed.
  Complete quoted exported flags, absolute metadata rebasing, alternate prefixes/
  toolchains, generated script/template contexts and full installed/relocated REX.
  Existing small lib/lib64 fixtures do not establish a full installation pass.
- Validate all remaining native, installed, excluded and alternate-configuration
  consumers after API/layout/virtual-hook removals and constructor changes;
  [migration.md](migration.md) records the retired interfaces. Validate semantics
  beyond compilation: setters/copies/replacements, provenance/preprocessing,
  qualification/symbols, serialization, analyses, transformations and live tools.
  Tutorials are disabled in the current build; check their claimed behavior,
  standalone generators, temporary lifetimes and test-driver integrity.
- Finish libharu installed/relocated/alternate-prefix acceptance and conflicting
  CMake/pkg-config selection after AUD-1404; prior configuration/object checks
  do not establish runtime or memory acceptance. Finish public configuration
  macros, annotation/reference/requires-expression/local-type owners, allocation-site
  identity, filename-policy configurations and general file/string/path rendering.
  Keep recorded identities distinct from native physical lookup and command paths.
- EXT-CLANG-FILE-MACRO-CR remains unresolved fidelity: stock LLVM22 normalizes CR
  in `__FILE__` stringification (AUD-2236). EXT-FLANG-FRONTEND-EXPORTS documents
  missing MLIR export closure and the existing narrow archive-dependency check.
  See [external-exceptions.md](external-exceptions.md) for mechanisms, inline
  comments, exact versions and recheck triggers; neither is a test/memory waiver.

## Memory defects and external exceptions

Fix REX initialization, ownership, lifetime and API misuse directly. Prefer sound
integration through documented typed dependency APIs; carrying a fork is not
required. External exceptions need exact dependency versions, object/write or
allocation/lifetime origins, semantic effects, narrow comments and removal
triggers. One external stack does not classify other contexts or permit an
incorrect AST to be masked.

Flang consumer retirement still needs runtime validation after the LLVM22 KIND4
blocker. Earlier focused memory passes do not establish current full acceptance.

- AUD2243's recorded Memcheck gate remains failed. AUD2287 reproduces all1000 primary use
  sites and available shared stack prefixes in stock LLVM22 on the exact historical
  fixture. The uncapped native report has6,677 contexts; matching use paths do
  not prove identical objects/write chains, harmlessness or the capped tail.
  Keep the failure visible and finish exact-origin/semantic classification.
- AUD2277 remains open: stock LLVM22.1.8 at `-O0 -fsanitize=undefined
  -fno-sanitize-recover=all` reports division by zero in an unevaluated
  `__builtin_constant_p` operand, both before and after REX. The unchanged plain
  original completes0 and transformed Memcheck completes0; no sanitizer waiver
  follows. Retain the exact source/logs and recheck when the stock compiler
  changes. Do not rewrite the builtin, annotate no_sanitize, suppress the report
  or change optimization to hide it.

Unclassified reports remain under AUD-861/868/924/945/974/993/998/1037/1051/1079:

- Copied include ownership (AUD-1581): the two strengthened cases report903/909
  uninitialized contexts despite zero lost bytes. Reports lack origins; exact
  initialization/write chains and later REX effects remain unclassified.
- Default-template parameter ownership (AUD-1492): two conditions in Clang
  reserveInits/FillInEmptyInitializations before REX translation; origin-tracked
  native Memcheck exits16. Stock Clang reproduces both and separately reports96
  possibly lost bytes. Exact object/write chains, semantic effects and existing
  suppressions remain to classify; no exception or policy change is accepted.
- Strengthened C tag ownership (AUD-1499): native Memcheck reports1,658 errors
  in1,000 contexts and exits16. Stock/native first-error diagnostics reproduce
  BuildArrayType with arena-allocation origins through diagnostic typedef-type
  creation. Exact objects/writes, semantic effects, later contexts and existing
  suppressions remain unclassified; zero lost bytes is not acceptance.
- Strengthened partial-member ownership (AUD-1507): native Memcheck reports1,341
  errors in1,000 contexts and exits16. Stock first-error with the unchanged native
  policy reproduces reserveInits and its canonical-template-specialization arena
  origin. Exact objects/writes, remaining contexts and later REX effects stay
  unclassified; the first stock reproduction does not waive this failed gate.
- Finalization (AUD-1124): 1,014 errors in 470 contexts. The 111 REX-first contexts
  have Clang stack-allocation origins; three Clang contexts lack origins. This
  overlaps AUD-974 and is not an accepted exception.
  AUD-1895 resolves both unnamed stack origins with matching debug symbols:
  exception-type rewriting's callback and ParentMap statement traversal. Trace
  their exact undefined members and writes; symbolization and avoiding nonlocal
  statement-parent queries do not prove these reports harmless.
- Dependence: 1,596 errors in 1,000 contexts, including two originally unnamed
  leaves. AUD-1257/1260 prove packed-byte increment precision loss for only the
  first stock/current-REX initializer report. Remaining contexts and later REX
  behavior remain unclassified; do not repeat that first trace as substitute proof.
- Expression-tag checkpoint (AUD-1344): 1,549 errors in 1,000 contexts, beginning
  in Clang compound-literal initialization. Origins and behavior after the cutoff
  remain unclassified; zero lost bytes are not acceptance.
- Constructor roundtrip: current AUD-1277 reports 14 errors in three contexts
  (earlier AUD-1147: six). The first path traces two empty-base constructors and a
  carry-free increment: the semantic flag stays false while shadow becomes
  undefined and counts remain zero. Repeated reports, register-shadow propagation
  and existing suppressions remain unclassified; this is not an exception.
- Interface (AUD-1216): 1,141 errors reaching 1,000 contexts. Stock LLVM22 reproduces
  selected base/initializer-list paths with 16,338 errors in 5,562 contexts and
  96 possibly lost bytes. Complete REX origins, harmlessness and behavior after
  cutoff remain unresolved; no new exception is accepted.
- Variable-template evaluation (AUD-1370): six errors in two Clang evaluator
  contexts reproduce in stock LLVM on current and prior specimens, with 96 other
  possibly lost bytes. Mechanisms and feasible integration fixes remain open;
  stock reproduction is not a waiver.
- Native CUDA and strengthened copying reach reporting limits. Later REX behavior,
  other contexts and stock lost bytes remain unresolved. Removing awk does not
  classify anonymous-record reports; generated RTI scalar selection lacks linked
  Memcheck acceptance.
- Project replacement (AUD-1408): 1,210 errors in 1,000 reported contexts, beginning
  in Clang InitListExpr::reserveInits before copying. The strengthened destruction
  assertions pass, with zero lost bytes and 40 reachable bytes; the reporting
  cutoff, 32 suppressed reports in 15 contexts, exact origins and later REX
  behavior remain unclassified. No external exception is accepted.

VFA's seven unsuppressed reports are now traced from each exact constructor
through completion to all three field/four base reads; all are packed-byte
tracking precision loss (AUD-1292). The driver finishes with zero lost bytes,
but strict Memcheck still fails. The only used rule is the unchanged
function-prototype constructor rule (199 reports/16 contexts). A safe reporting
boundary remains open; do not widen parser rules or repeat the completed
write-chain investigation. See [external exceptions](external-exceptions.md).

Verified LLVM/Clang/Flang: 22.1.8 (`1:22.1.8-1+b2`); Clang debug Build ID:
`1234ae7a7b9f9ec56546d67ee10c8c617c378680`. AUD-1079's leaves resolve to
IntExprEvaluator::VisitBinaryOperator at ExprConstant.cpp:18304/18305. Precise
write origins, other callers, complete contexts and harmlessness remain open;
see `build-audit/container-clang22-` evidence (AUD-1251).

## Acceptance and delivery

After root fixes and source review, freeze source and run the complete
[README](README.md) gates serially: full Debug and optimized regular CTest,
including negative/death and uninitialized-value contracts; full eligible Debug
ASan/LSan/UBSan and Memcheck registries with reviewed exclusions and required
child tracing; installation and relocated installed consumers; affected
supported configurations/architectures. PR WASM797 passes its build, exact eight
smoke cases and site assembly on source794 with Emscripten4.0.22; broader
configuration gates remain open.

Use clean acceptance builds, exact source/toolchain identities, complete CTest
registries/dependencies/fixtures, raw logs and real exits. Incremental focused
builds and interrupted runs are not acceptance. Never weaken assertions or alter
tests/frameworks to mask failures. Repeat gates only for relevant fixes, specific
investigations or demonstrably invalid evidence.
Input manifests must cover initialized submodules and explicitly contain every
changed build/test input; superproject-only hashes cannot prove whole-tree identity.

Current-source remote CI remains open. Nightly 36921106337 uses baseline image
`eceaca2`, excluding the dirty audit, and fails amd64
`outline_dlopen2_triangle_cpp` at the unchanged 900-second limit (AUD-1035).
Local focused passes do not clear that selection. The older RISC-V failure lacks
its diagnostic log. Verify the final workflow/image revision and remote results.
Weekly memory jobs also remain name-filtered selections with blanket death-label
exclusions; they cannot establish full instrumented acceptance. Review exact
eligibility and scheduling before broadening them, preserving dependency/fixture
closure and the report checks that reject incomplete runs.

The published checkpoint is [PR861](https://github.com/ouankou/rex/pull/861).
Required local PR CI gates pass; full audit acceptance still requires the gates
above. Keep the PR description aligned with final behavior, deletions, API/schema
migration, evidence and accepted limitations.

## Resource boundary

Read `build-audit/RESOURCE_STOP.json` before resuming. The authorized existing
act runner has 24,000,000,000 bytes RAM, zero extra swap, four CPUs and 512 processes;
the idle `rex-audit-bounded` container has 16,000,000,000 bytes RAM and zero extra
swap. Combined container RAM caps are 40,000,000,000 bytes. Verify limits before
restarting; run one build or plain CTest job with one worker. No new containers
or custom runners. Freeze source during jobs. For runtime controls in the bounded
container, use `TMPDIR=/mnt/current-release/Testing/Temporary` because its `/tmp`
is noexec.

Collective RAM stays below 48,000,000,000 bytes and disk below 200,000,000,000 bytes,
including images, layers, sparse filesystems, host artifacts and temporary files.
Preserve source and diagnostics; reclaim obsolete artifacts only after checking
consumers. Do not repeat full suites while known blockers remain.
