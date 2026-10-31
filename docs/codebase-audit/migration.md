# Interface changes under validation

These changes are part of an unaccepted audit batch until the full required
validation matrix passes. No supported language behavior is intentionally removed.

`UnparseQualificationSession` (unparseQualificationSession.h, included by
unparser.h) is an RAII scope in which the AST does not change. While one is
open, every `unparseToString()` request that walks a whole source file reuses
one name-qualification walk of that file instead of repeating it per request;
a standalone type use continues from the frozen end-of-walk state and adds only
its own records. Outside a session, behavior is unchanged. Closing a session,
or using it after any typed AST modification, fails with
`REX_UNPARSE_INVARIANT[qualification-session-mutation]`.
`REX_UNPARSE_QUALIFICATION_CHECK=1` recomputes every reused result per request
and requires identical records and output. CFG DOT output and the
unparseToString test driver open sessions. `SgUnparse_Info` setters no longer
advance `SgNode::get_globalAstModificationSequence()`, since unparser
configuration is not program AST, and destroying an `SgSourceFile` or
`SgProject` advances it. `NameQualificationContext` is non-copyable and can
extend a read-only base context. The unused
`NameQualificationTraversal::get_qualifiedNameMapFor*()` accessors are removed.

C++ class population no longer resolves special-member exception specifications.
[except.spec] evaluates one only when the program needs it; forcing evaluation
instantiated ill-formed default member initializers and rejected valid programs,
such as libstdc++ hash tables over REX's own `hash_Name`. Clang resolves every
needed specification; the rest stay `e_exception_unevaluated` or
`e_exception_uninstantiated` in declared exception signatures and AST JSON.
`HasNoThrowAssign`, `HasNoThrowCopy` and `HasNoThrowConstructor` answer from
evaluated guarantees: a throwing eligible member decides false, and a query that
depends on an unevaluated one fails with
`REX_AST_INVARIANT[unevaluated-exception-specification]`. Declaring implicit
members fails at once if it raises a Clang diagnostic. This supersedes the class
population part of AUD-2255.

A C function's type is Clang's callable prototype. An old-style definition
(C11 6.9.1p7) keeps each parameter object's declared type, while its function
type takes the promoted one, so `int f(c) char c; {}` has type `int (int)`. When
no earlier declaration exists, the semantic nondefining declaration synthesized
for such a definition declares that callable identity: parameters whose
callable type differs carry the callable type.

`SgNode::SourceStateConstruction` keeps its contract but no longer inserts every
constructed node, including `SgName` and modifier values, into a `std::set` per
active scope. Each scope opens a construction epoch and `SgNode` records the
epoch current at its birth in `p_constructionEpoch`, which copies never inherit.
The adjacent flag fields keep `SgNode` at its previous size. `SgTypeTable(int)`
sizes the global type table; scope type tables, which hold no entries, use the
ordinary symbol-table size instead of 1001 buckets each. AST JSON schema251 is
unchanged; rebuild consumers against the generated header.

Generated node classes replace the per-slot `PoolFreeNode` list (`next_node`,
`pool_free_nodes`) with `free_slots`, a stack of slot numbers
(`pool_index * pool_size + slot_index`) whose last entry is allocated next.
`SgNode::PoolEntry` records `pool_index`, so `operator delete` finds a node's
pool through `SgNode::findPoolEntry` instead of scanning its class's pools.
`SgNode::clearMemoryPool` frees this bookkeeping with the pools.

`SgNamespaceDefinitionStatement` link setters are hand-written and, like the
destructor, advance `get_reopening_chain_revision()`. The Clang frontend indexes
each reopening chain's source fragments by opening order and rebuilds an index
only when that revision changes; it audits every current index at the end of
translation. Two fragments of one namespace with overlapping expanded-token
intervals now fail with `REX_FRONTEND_INVARIANT[namespace-fragment-selection]`.

AST JSON checkpoint writes stream to their files, and the round trip releases
each text, parsed document and signature once the next stage has consumed it.
Parsed objects store interned keys in a sorted member vector instead of
`std::map`. Output bytes and schema251 are unchanged.

Schema251 requires `is_braced_initialized` and
`using_assignment_copy_constructor_syntax` on initialized names, including external
encodings. Readers validate both booleans before construction to preserve direct
initialization through JSON. Regenerate older checkpoints and rebuild consumers;
these fields already exist in the AST, so this revision changes no AST-node layout.

Schema250 adds `e_cxx_pragma_source_attribute` to the existing typed pragma payload
kind. C++ OpenMP statement attributes travel through ordinary REX pragma construction
and JSON before the OpenMP constructor resolves them. Serialization and reconstruction
retain the source form; the native OpenMP clause translator is removed. Clang OpenMP mode is hard
rejected. Regenerate checkpoints and rebuild consumers against the generated
header; AST layout is unchanged. Focused build, schema, JSON and runtime controls
pass655/656/658/659; same-name variant handling and full CI remain open.

`generateNameQualificationSupport` accepts an optional exact inline type
definition, selected by `Unparse_Type::inlineDefinitionForType` through existing
typed owner checks. Existing source calls remain valid, but the library symbol
changes: rebuild native and installed consumers. The complete native build and
install pass646; installed-consumer execution remains open. AST layout and
schema249 are unchanged.

Compound literals retain written source type separately from completed object
and initializer value types. `cxxSourceTypeMatchesObjectType` permits only omitted
outer array bound completion for object declarations. The existing strict type
matcher is unchanged. `cxxCompoundLiteralInitializerTypeMatchesObjectType` allows
only top-level CV differences between initializer and object. Both add library
symbols without changing schema249 or AST layout. Rebuild affected consumers.

Schema249 requires `expression_list_source_form` on every `SgExprListExp`.
The typed plain/braced distinction preserves dependent nested initializer syntax
without inventing a semantic value type. Initializer construction transfers its
outer brace form to the owning initializer; nested source lists retain their
braces. Both JSON readers require and validate the field before publication.
Expression-list value-type queries still abort. Regenerate checkpoints and AST
headers, and rebuild all native, installed and WASM consumers for the layout
change. The small native/JSON regression passes; full gates remain open.

`Rose::AstJson::roundTripSourceFile` replaces the complete project file graph and
retires its original nodes. Use the returned file and reacquire other file and
declaration pointers from the project. This replaces the prior detach-only
lifetime contract; the regression checks retirement and shared type identity.

Use `SageInterface::replaceSubexpressionWithStatement` for assignment-result
lowering. The installed inliner header no longer declares the duplicate global
`replaceAssignmentStmtWithStatement`, which passed an attached destination to
the statement generator. Rebuild installed consumers against the retained API.

`OmpSupport::retireConsumedOmpDirective` shares the existing strict retirement
implementation with task lowering through the installed lowering header. This
adds a library symbol without changing AST layout or schema246. The serial
native header-consumer rebuild passes; installed-consumer validation remains open.

`SageBuilder::functionParameterTypeFromDeclaredType` exposes the existing
callable-parameter conversion. Fortran intrinsic calls use it before signature
interning, preserving the actual argument's declared type. This adds a library
symbol without changing AST layout or schema246; rebuild installed consumers.

Function-parameter scopes are constructed before dependent signature types in
JSON readers and copying. `completeDetachedFunctionParameterScopeConstruction`
now attaches the exact scope without changing source provenance; Flang producers
publish generated source explicitly before attachment. Signatures, AST layout
and schema246 are unchanged; rebuild affected native and installed consumers.

Function instantiation builders now adopt a complete detached semantic argument
list when the written list is empty. Callers retain a borrowed list view after
that transfer. Parented arguments and arguments alongside a written list are
still copied; declaration families retain independent owned argument nodes.

Schema246 retains a dependent alias use's exact typed projection in immutable
`SgNonrealType::aliased_type`. Its declaration serializes that projection once;
type references resolve the declaration's existing type. Private projection
occurrences retain their exact owner during copying and deletion. Rebuild
generated ASTs and native/installed consumers for the layout change, and
regenerate older checkpoints. Full copy and memory acceptance remain pending.

AUD2305 adds `AnalyzeConditionRefs` for dependency/effect queries that include a
declaration condition's retained runtime predicate. `AnalyzeStmtRefs` keeps its
subtree-only contract. The new two-argument overload of
`AstInterface::GetRuntimeDeclarationCondition` also returns the exact source
declaration; the existing one-argument signature remains available. The unused
public `ArrayReferences` wrapper is removed after migrating its two callers to
`AnalyzeConditionRefs` with their existing array filters. External users must
migrate and rebuild; installed-consumer validation remains pending. AST layout
and schema243 are unchanged. The focused rebuild and three CTests pass; full
acceptance remains incomplete.

AUD2304 adds `AstInterface::GetRuntimeDeclarationCondition`, a non-owning query
for the separate runtime predicate. Legacy CFG test blocks now contain the source
declaration followed by that value. Existing signatures, AST layout and schema243
are unchanged; rebuild clients instantiating the updated CFG template. Dependency
and effect consumers remain under review.

AUD2303 adds owned `SgExpression *condition_value` to IF, WHILE, FOR and SWITCH.
For a declaration condition, preserve its converted variable read separately from
the source declaration; copying must rebind it to the copied variable. Expression
conditions keep their existing expression-statement edge and no separate value.
`SageInterface::getConditionValue` validates these roles. Runtime CFG executes the
declaration before the predicate; constexpr IF retains its compile-time predicate
without executing that conversion. Control branch indices remain unchanged.
Constructor argument positions are unchanged, but the AST layout changes: rebuild
generated ASTs and every native/installed consumer. Schema243 requires the exact
predicate edge for declaration conditions and rejects older checkpoints; regenerate
them from source. Manual construction and rewriting clients remain under review.

AUD2294 moves the reverse call-target cache from a user attribute to opaque,
project-owned `SgProject::CallGraphState`. Copies start with independent state;
project destruction releases it. This changes project layout and adds runtime
accessors; rebuild native and installed consumers. The derived cache is not
serialized, so schema242 is unchanged. Full acceptance remains pending.

AUD2292 appends `eckCompleteObjectConstruction` and `eckBaseObjectConstruction`
to `VirtualCFG::EdgeConditionKind`, preserving existing numeric values. Constructor
CFG clients must handle the virtual-base execution and skip paths; filtering
retains their guards. AST layout and schema242 are unchanged. Interprocedural
context propagation, installed consumers and full acceptance remain pending.

Schema242 requires `SgFunctionRefExp::c_implicit_declaration` as a node ID,
including zero for absence. Both readers retain this temporal binding and
reject inconsistent dependencies before constructing the reference. Copying
remaps it through the exact node map. This follow-up changes no AST layout.

AUD-2288 adds constructor-owned `SgFunctionType::has_prototype` (default true)
and required `SgFunctionRefExp::function_type`. References preserve each use's
type independently of symbol reanchoring; explicit `set_symbol` rebinding
updates the reference type. Long member-function constructors take prototype
kind before the class type. The symbol-only SageBuilder factories remain
available; overloads accept an exact use-site type. Schema241 requires
`has_prototype` and reconstructs references with their serialized type.
Rebuild generated ASTs and all native/installed consumers for these layout/API
changes. Focused tests pass; full acceptance remains pending.

AUD-2283 implements virtual CFG queries for typed GNU choose expressions. Known
selection yields one unconditional arm; dependent selection yields two constrained
alternatives without executing the condition. Incoming and outgoing boundaries
agree. Signatures, layout and schema240 are unchanged; broader consumers and
full validation remain open.

AUD-2282 adds `SgChooseExpression::selection_kind_enum` and a required selection
argument to `SageBuilder::buildChooseExpression_nfi`; raw construction also requires
classified selection. Supply the exact true/false/dependent fact before the node
owns its operands. The field changes the AST layout and constructor API; schema240
requires regenerating older checkpoints and rebuilding native/installed clients.
The library and four focused clients rebuild. Both readers preserve the field and
reject missing/unclassified values before node construction. Copying retains it;
the native PRE regression passes UBSan/Memcheck. Remaining consumers, general
editing/dependency variants and full acceptance stay open.

AUD-2279 adds `SageInterface::moveVariableDeclarationGroup(group, targetBlock)`.
It accepts a published variable-only source group, validates every member's exact
source symbols and destination conflicts before detachment, and retains symbol
identity while publishing the group in the target block. Mixed declarations and
general type-owned scope transfers are outside this contract; allocation-failure
rollback remains unvalidated. Library, preTest and the existing unit client
rebuild; unit checks and the native diagnostic pass, while three exact PRE
comparisons still fail. Other consumers and full gates remain open. No existing
ABI/layout or schema239 change is introduced.

AUD-2271 makes `legacy::expressionComputedIn` and
`countComputationsOfExpressionIn` report guaranteed runtime computations rather
than count conditional or unevaluated syntax unconditionally. Input writes still
require separate checking. Signatures, ABI/layout and schema239 are unchanged;
full acceptance and remaining PRE output contracts stay open.
AUD-2281 extends these counts to ordinary IF/WHILE/FOR control and existing typed
constexpr/consteval selection. It excludes skipped bodies and compile-time
expression conditions; signatures and layout remain unchanged.

AUD-2269 removes the unused virtual `VisitFriendTemplateDecl` entry from the
installed `clang-frontend-private.hpp`. Stock LLVM22 does not produce that node;
supported friend templates continue through `VisitFriendDecl`. Rebuild translator
subclasses and installed/native clients for the changed virtual interface.
The library and two focused ownership clients rebuild and both controls pass;
remaining consumers/full installation are unvalidated. AST schema239 is unchanged.

AUD-2268 adds `function_declaration_ownership::withFunctionDeclaratorScope`, taking
a detached/provisionally staged declarator scope, optional detached source function
type and written parameter list, and an old-style-definition flag. Supply source
parameter bindings to that exact scope before construction. Both function builders
own these views before publication; scopes owned by another function are rejected.
Nondefining declarations reject the old-style-definition role. The descriptor gains
two pointers and a bool: rebuild native and installed consumers for the C++ ABI
change. The library/dependencies and eight focused clients rebuild;14 focused
controls pass. Other clients and installations remain stale. AST schema239 is
unchanged; general construction and full acceptance remain open.

AUD-2267 extends existing `withSourceParameterList` support to nondefining builders.
Supply separate detached written and native adjusted parameter lists with matching
names, arity and pack roles. The builder validates standard parameter adjustment
and owns the written list/signature before declaration-owner publication. The
defining path shares this construction. No descriptor layout or schema change;
six focused controls pass, while general frontend integration and full gates remain
open. This supersedes AUD-1533's historical nondefining-builder restriction below.

AUD-2264 adds the owned, non-traversed
`SgRequiresExpr::local_parameter_list_syntax` and appends it to the expression
constructor and `SageBuilder::buildRequiresExpr(_nfi)` arguments. The default is
null when the lookup list also represents the spelling; otherwise supply a
distinct list at construction with matching names, arity and scope and no lookup
symbols. Lookup objects retain native adjusted types; written nodes own array
bounds and prototypes. Copying, deletion, qualification and both JSON readers
consume the edge. `SageInterface::cxxParameterTypeMatchesSemanticType` checks the
standard array/function adjustment and exact cv qualification. Schema239 rejects
older checkpoints. Rebuild remaining clients for the changed constructor/layout.
Five guarded focused controls pass;
installation, alternate consumers and full/instrumented acceptance remain open.

AUD-2255 resolves eligible nondependent special-member exception specifications
at native class population and uses existing typed declared exception signatures
for nothrow queries, preserving C++14 guarantees outside canonical types. Own
eligible families replace hierarchy guesses. No public API/layout/schema238 change;
seven focused controls pass, while general variants and full acceptance remain open.

AUD-2253 distinguishes signed floating zeros in PRE using numeric sign and existing
typed extended-value words. No public API/layout/schema238 or output contract change.
The unused PRE type_traits include is removed. Full acceptance remains open.

AUD-2252 adds private PRE storage-reachability proof, with no API/layout/schema238
change. Ordinary scalar stores preserve expressions only when typed whole-function
uses prove their integral inputs unexposed. Unsupported uses and opaque calls
retain barriers. Focused units/generators pass; exact comparisons and gates remain
open. The redundant scope setter after declaration construction is removed.

AUD-2250/2251 add no API/layout/schema238 changes. PRE proves a limited defined
scalar call body read-only; unresolved/weak/member calls and unsupported effects
remain barriers. Atomic-qualified reads cannot be cached through expression
equality. Generated output-only function references accept their exact zero
inclusion occurrence via isGeneratedSource and matching physical ownership;
parsed-source and remaining provenance requirements are unchanged. Five focused
checks pass; three PRE exact comparisons and full acceptance remain open.

AUD-2249 identifies ordinary C/C++ local/value-parameter storage through builtin
lvalue and address arithmetic. Static/external/thread-local storage, unknown
pointers, aliases and calls retain their barriers. This is private PRE precision
work, with no public API/layout/schema238
change. Focused storage controls and generators pass; unchanged exact-output
comparisons, broader alias analysis, installed clients and full gates remain open.

AUD-2247/2248 preserve reference-binding effects and an explicitly supplied physical
output owner. Public APIs/layout/schema238 are unchanged. Source-variable builders
publish exact output identity before returning; physical attachment is separate.
The Fortran producer control now checks this early identity while detached and
retains its post-append provenance checks. Focused validation does not establish
general alias/cleanup correctness, installed consumers or full acceptance.

AUD-2245 makes complete C++ copy-constructibility/assignability queries require
their typed special-member families; missing declarations assert instead of
triggering member/base guessing. Native eligibility filters default/copy/assignment
queries. C record assignment uses exact field qualifications through array layers.
No public API/layout/schema238 change. Native controls pass; PRE failures and
broader semantic/installed/full-gate validation remain open.

AUD-2244 requires a third, explicit is_ineligible argument in
with/setSpecialMemberProperties and adds it to special_member_properties.
isIneligibleSpecialMember reads the native ineligible/not-selected result;
deletion remains separate. Canonical transfers and copies preserve all three
facts. Modifier bit34 uses existing vector capacity; AST storage is unchanged.
Schema238 rejects earlier checkpoints. Rebuild API/ownership-value consumers;
the configured default graph is rebuilt, then one void-domain implementation
fix is compiled with its affected client. Installed/alternate consumers and
full acceptance remain pending.

AUD-2243 supplies implicit lambda special-member declarations before expression
publication. getInternalTypes follows semantic specialization arguments, matching
class identity, rather than its written source arguments. No API/layout/schema237
change. The broad auxiliary-copy-order experiment is reverted, not delivered.

AUD-2242 generalizes constructor-property APIs to special-member properties:
use with/get/has/setSpecialMemberProperties and isTrivialSpecialMember; the
ownership value type becomes special_member_properties. Constructor defaultness
remains isDefaultConstructor. Modifier bits41/42 now carry triviality for
constructors and copy/move assignments; no AST storage/vector-width change.
Schema237 rejects missing/conflicting facts regardless of frontend origin and
does not accept older checkpoints. Rebuild clients of the renamed APIs; no
compatibility shim is supplied. The configured default set is rebuilt, followed
by a reader/client implementation fix; installed/alternate consumers and full
acceptance remain pending.

AUD-2241 makes default-constructor queries require classified constructor families
for complete C++ classes, including auxiliary declarations. Producers must supply
default-constructor, triviality, deletion and access facts before querying. Missing
families assert; parameter shape, POD and base/member reconstruction do not supply
answers. Public signatures, ABI/layout and schema236 remain unchanged.

AUD-2240 makes HasTrivialCopy require typed copy-constructor roles and classified
constructor triviality for complete C++ classes, including auxiliary declarations.
Synthetic IR producers must provide these facts before querying; declaration
absence, POD and parameter shape no longer supply inferred answers. Public
signatures, ABI/layout and schema236 remain unchanged.

AUD-2238 removes unused SgNode createAndParent, initParentPointer and both
changeChildPointer templates, private debugSerializationHelper and protected
virtual destructorHelper. No retained caller/override uses them. Rebuild every
consumer for the changed vtable ABI; the configured default Release set is rebuilt,
but installed/excluded/alternate clients remain unvalidated. Node data layout,
serialized fields and schema236 are unchanged; no compatibility shim is added.

AUD-2192 removes Options frontend-note, frontend-warning and backend-note fields
and get/set APIs. Rebuild clients for the Options layout change. Backend warnings
and exported construction/copy operations remain. Unparsers require typed output
language; source template types remain distinct during JSON construction. Class
parameter lists copy before dependent bodies. AST layout and schema233 are unchanged;
installed-client and full acceptance remain pending.

AUD-2191 changes CompReprBase/CompactRepresentation and IntraProcAliasAnalysis
layouts; rebuild analysis clients. Alias states now carry may/must write effects,
and equality includes them. merge accumulates facts; CFG joins intersect must
effects, and callee entry resets inherited effects. No AST layout or schema change.
Remaining and installed-client validation is pending.

AUD-2190 changes the IntraProcAliasAnalysis C++ layout; rebuild its clients.
getFunctionParametersAliasRelations now takes only the argument-relations output;
return bindings are applied during call transfer. A null CompReprPtr means
pending/unreachable flow, while an allocated empty graph is reachable. Explicitly
seed standalone analyses with setFunctionEntry; VirtualFunctionAnalysis seeds main.
setFunctionExit replaces the current summary. AST layout and schema233 are unchanged;
remaining and installed-client validation is pending.

AUD-2189 removes six unused global symbols with no public declarations:
print_pragma, get_location, get_library, get_libraryName, get_OS_type and
get_distanceFromSourceDirectory. Live filename-classifier APIs remain.
No AST layout or JSON change; installed-consumer validation is pending.

AUD-2187 removes SgProject::binary_only and changes the Project C++ layout/API.
Rebuild all consumers. generateSourceFilenames now takes one const argv reference;
remove its obsolete binary-mode argument. The executable/archive magic readers,
suffix-table globals and initialization APIs are retired. Call isSourceFilename
or isObjectFilename directly; supported suffixes are unchanged. JSON schema233
is unchanged. Remaining and installed-client validation is pending.

AUD-2186 retires the installed CollectionHelper.h printing wrappers. The three
call sites now print their containers directly; preprocessing diagnostics
use recorded AST filenames without filesystem resolution. External clients must
print their own containers. No AST layout or JSON change; installed-client and
relocation checks remain pending.

AUD-2185 removes unused inline FileHelper members makeAbsoluteNormalizedPath,
getIncludedFilePath and endsWith from the installed header. External clients must
use filesystem/string operations appropriate to their path and lookup contracts.
Class layout, exported data and JSON schema are unchanged. Native consumers build;
installed and relocated consumers still require validation.

Schema233 requires `structured_binding_pattern` on initialized-name records: zero
means absence; a nonzero identity must match the owned typed expression-list edge.
Readers validate named binding references. Source reconstruction binds the child
before publishing its owner. Regenerate earlier checkpoints. This changes no AST
layout or public API.

StaticCFG stores CFGNode positions and complete CFGPath values in graph-owned typed
maps instead of `"info"` attributes. Replace CFGNodeAttribute/CFGEdgeAttribute access
with `cfg.toCFGNode(node)` and `cfg.toCFGPath(edge)`. The latter returns a reference
valid until rebuilding or destroying the CFG. Use instance getOutEdges/getInEdges
instead of the removed namespace helpers; getIndex is now an unsigned instance
query, and toCFGNode is const. Derived builders use getOrCreateGraphNode/addEdge;
the protected buildCFG template no longer takes an auxiliary node map. Foreign
nodes, edges and inconsistent endpoints fail before analysis. Rebuild native and
installed CFG consumers: the CFG C++ layout and API/ABI change, while source AST
layout does not, and this CFG change adds no JSON schema revision. Six focused
controls pass; full acceptance is open.

Schema 232 adds `e_fortran_directive_compiler` for complete native compiler-directive
text. Generic `!pragma` payloads and grouped OpenMP/OpenACC/CUDA directives retain
their separate families. Compiler directives cannot carry group ownership metadata.
Regenerate checkpoints and rebuild native/installed consumers. The no-location
`SgPragmaDeclaration` constructor now requires its family argument; pass
`SgPragmaDeclaration::e_fortran_directive_none` for a generic pragma. The existing two-argument
`buildPragmaDeclaration_nfi(text, scope)` remains available; its three-argument
overload accepts an explicit family before construction. Node layout is unchanged,
but constructor ABI changes. Native copy/JSON/emission controls and the updated
required-fields contract pass in staged checks; full acceptance remains pending.

Schema 230 requires complete function-specialization arguments independently of
the written template-id. Function and member-function instantiation constructors
receive both lists; nondefining builders require the semantic list as their last
argument. Lookup and mangling use that complete identity. Readers construct both
owned lists before publishing the declaration. Rebuild native consumers and
regenerate older checkpoints; their missing semantic arguments are not inferred
from source spelling. Focused controls pass; the full matrix remains outstanding.
Implicit/explicit-instantiation semantic declarations have no written argument
list and are not source-explicit. Each written directive owns a separate child
with its own argument spelling; consumers must use the complete semantic list
for identity. This correction does not change schema230 or native APIs.

Schema 229 preserves intermediate alias and rename targets. Visibility targets use
`alias_target_binding` or `original_symbol_binding`, with their exact symbol and
stored name. The location is either a positive `binding_scope` graph ID or
`binding_local: true` for the receiving inline parameter scope; they are exclusive.
Canonical targets retain `alias_target` or `original_symbol`, exclusively of a
binding. Regenerate checkpoints. Native alias construction preserves its supplied
target: `get_alias()` returns that immediate binding and `get_base()` returns the
first non-alias symbol, which may be a Rename. Alias-table queries use the latter.
`set_alias()` now rejects null or cyclic prospective targets before assignment.
`SgRenameSymbol` construction and `set_original_symbol()` validate the complete
mixed chain and its exact function-declaration basis. Accessor signatures
and native layout are unchanged; regenerated declarations move to the validated
implementation. Rebuild affected consumers.

Range-for owned children no longer precede their scope during copying. The original
seven- and eight-argument constructors remain available; minimal scope constructors
let copying register the scope before cloning its contents. This copy fix changes
neither native layout nor serialization. Rebuild consumers of the regenerated header.

Schema 227 replaces READ/WRITE `namelist` strings with exact `SgSymbol*`
bindings to `SgNamelistSymbol` or a standard USE alias. The canonical symbol owns
no declarations; its read-only list references all source group parts. Construct
it from those complete groups and supply the visible symbol to I/O constructors.
The late NAMELIST setters and unused `SgNameGroup::set_group_name` are removed;
create a complete new declaration family when changing group identity. The library
and three consumers build and four focused controls pass; rebuild other consumers
and regenerate checkpoints. Full layout/codec acceptance remains open.

Schema 226 replaces `SgNameGroup::name_list` strings with the required typed
`objects` list of `SgVarRefExp` children. Construct groups with their complete
object references and NAMELIST statements with their complete groups; both lists
are read-only after construction. Update callers, rebuild native consumers and
regenerate checkpoints. Schema 227 adds typed READ/WRITE group bindings.

Remove the unused `builtFromUseOnly` and
`isRepresentingTemplateParameterInTemplateDeclaration` fields and their getter/
setter pairs. Native layout/API changes require consumer rebuilds. Neither field
had a JSON property, so their removal leaves schema 226 unchanged.

AUD-2105 changes the native AST layout and JSON schema to 225. A canonical
`SgLambdaExp` owns `closure_scope`; source projections borrow that canonical
context. Its `SgDeclarationScope::semantic_outer_scope` is an immutable constructor
reference, preserving external capture bindings during partial copying.
`buildDeclarationScope` accepts that optional scope; ordinary no-argument
construction remains available. Native lambda construction takes the owned
context explicitly and adopts it before publication. Copying prepares the context
and function argument types before resolving semantic constructor dependencies.
Rebuild all native consumers and regenerate older checkpoints.

AUD-2108 requires a fresh canonical closure recipe when native lambda construction
adopts its context. That constructor publishes the reciprocal class/lambda link;
callers must not assign it afterward. Reader preflight requires the exact
canonical owner, and restoration validates the constructor's link. Schema 225,
native layout and accessor signatures are unchanged by this contract correction.

AUD-2104 moves statement-expression body adoption and scope-transaction consumption
into its native constructor. Pass a detached basic block with the exact active
construction scope, or its completed copied/reader semantic scope. Callers must
not consume that transaction again after construction. Builder signatures, native
data layout, the generated public header and schema224 are unchanged.

AUD-2103 preserves the return accessor signatures but rejects operand/keyword
mutations that invalidate an existing NRVO candidate. To change that candidate,
construct a new return with its exact typed binding rather than retaining stale
metadata through an operand setter. Native data layout and schema224 are unchanged.

Schema 224 requires `nrvo_candidate` on return statements: the exact initialized
name selected by Clang, or zero. The matching pointer edge must agree. Both
readers validate local ownership before constructing the return; copying preserves
the mapped variable identity. Native return constructors and builders accept this
optional pointer at construction. This records candidate identity, not an executed
optimization: LLVM22 clears ineligible candidates for completed nondependent class
returns; dependent patterns retain provisional candidates until instantiation.
Rebuild all IR consumers and regenerate older checkpoints. Cleanup execution
remains incomplete.

Native copying materializes expression types and maps non-owning scalar constructor
references before construction. References to variables in copied scopes construct
their exact mapped declaration/symbol dependencies first; external scopes remain
shared. General recursive publication and mutation review remain open.

Schema 223 requires `cxx_field_index` on initialized-name descriptors: an unsigned
ordinal for a C++ field, null for names without that fact. Clang supplies it before
field publication; native producers must supply it before constructing member
cleanup or field-owned materialization. Both readers validate its representation
before allocation. Native
epilogues require strictly reversed field ordinals, independently of lexical or
auxiliary ownership. Rebuild IR consumers and regenerate older checkpoints.

`defaultFunctionGenerator` and its installed header are removed. Clang supplies
the selected constructors, initializers and implicit special members during AST
construction. Native producers pass the exact selected declaration to
`buildConstructorInitializer`, then supply that initializer to the variable
builder; no later class-type search selects a constructor. Source variable
builders finish physical ownership before symbol publication. Remove the obsolete
ICG driver option; the remaining transformation options retain their behavior.
Rebuild native/installed consumers. Installation and full acceptance remain open.

The installed analysisUtils.h no longer supplies findIterator; use std::find.

Native destructor epilogue adoption now requires the complete typed inheritance
hierarchy, correct base roles and reverse direct/virtual-base order. Cycles are
rejected; cached indirect entries do not establish inheritance. Prepare these
dependencies before adoption.

Template variable builders accept pending exact-source ownership without publishing
a symbol or lexical child. The frontend completes the source declaration before
publication. Variable-template type queries consume exact semantic bindings;
completed-AST checks retain the full publication contract. The raw
buildTemplateVariableDeclaration_nfi builder now always returns an unpublished
declaration: complete its metadata and explicitly publish ownership and symbols,
or use the high-level ownership builder. Private redeclarations use their typed
canonical family without requiring a public symbol. Public signatures and
schema222 are unchanged; broader semantic properties remain under review.

`SgConstructorInitializer::get_class_decl()` consumes the selected method's typed
associated-class edge and requires an established canonical class declaration.
Synthetic producers must establish that family before querying it; missing family
links no longer return an uncanonicalized declaration.

Schema 222 also requires `is_thread_local` on external parameter-scope variable
descriptors. The reader rejects missing or non-Boolean state before allocation.

Schema 221 requires the existing variable `is_thread_local` flag. Preserve this
standard storage spelling separately from GNU `__thread` storage metadata; both
readers validate it before allocation. Regenerate older checkpoints.

Schema 220 separates semantic function deletion (`isDeleted`) from written
`= delete` (`isMarkedDelete`). Frontend builders supply both facts before
publication through `withDeletionProperties`; redeclarations inherit canonical
facts when omitted. Marking written deletion also marks semantic deletion.
Readers reject written deletion without semantic deletion before allocation.
Rebuild AST/builder consumers and regenerate checkpoints. Trait queries consume
semantic deletion and exact copy/move roles, including auxiliary implicit members.

Schema 219 records exact copy/move constructor and assignment roles in the
function modifier. `withMethodProperties` now carries that typed role before
publication; an omitted method-properties tuple inherits the exact canonical
method's facts. Synthetic builders specify copy/move roles explicitly rather
than relying on parameter or name inference. Rebuild consumers and regenerate
checkpoints. Both readers reject conflicting roles and incompatible owners
before allocation. ICG consumes these facts instead of classifying signatures.

Remove unused `SageInterface::hasTrivialDestructor`. No repository consumer uses
it. The distinct active `HasTrivialDestructor` type-trait API remains under audit;
this cleanup does not introduce an alias or claim a destructor-semantics fix.

Schema 218 retains dependent, unselected, selected-trivial or selected-nontrivial
destructor facts in the function modifier. Destructor builders must supply them
with `withDestructorProperties`, or inherit the exact canonical declaration's
facts. Rebuild IR consumers and regenerate checkpoints; frontend destructor
records without these facts are rejected before allocation.
Function instantiations also require the constraint-satisfaction and SFINAE
results already retained for class instantiations. Trailing requires-clauses
are constructed as owned signature dependencies rather than dropped on reading.

Constructor builders require `withConstructorProperties(is_default, is_trivial)`
or inherit the exact canonical declaration's existing facts. Rebuild builder
consumers. The frontend no longer rewrites these properties after publication.
`withMethodProperties` supplies virtual/pure/final/override facts before method
publication; absence inherits the exact canonical method's flags. The private
Clang visitor dispatches methods directly through function construction.

Remove unused `addDefaultConstructorIfRequired` and `buildDefaultConstructor`.
Use the ordinary member declaration builders with explicit constructor properties;
the audit no longer offers a repair API that invents missing methods. ICG also
consumes frontend constructor initializer lists without synthesizing base calls.
The remaining native constructor construction contracts are still under review.

Schema 217 retains materialization duration and the exact lifetime-extending
initialized name. Rebuild IR consumers and regenerate checkpoints. Native copies
use the existing reference transaction; readers require complete lifetime data
before constructing the expression. Field defaults retain containing-object
duration and structured bindings use their decomposed declaration's storage.

Schema 216 retains the exact selected destructor on a bound temporary expression.
Rebuild IR consumers and regenerate checkpoints. The previous wrapper bit alone
did not retain the callee; readers now require its compatible typed declaration.
Local/temporary cleanup execution remains under review.

Schema 215 requires the existing `source_type_identity` on `SgAutoType`, so
references to one placeholder retain its identity while distinct placeholders
remain distinct. Both readers use the existing typed identity transaction;
regenerate checkpoints. The strengthened required-fields identity and ownership
checks and the lambda checkpoint pass. Full acceptance remains incomplete.

AUD-2090's schema 214 adds SgCxxSubobjectDestruction and owned destructor
epilogues on function definitions. Concrete destructor definitions must provide
typed cleanup; dependent definitions retain validated template context. Rebuild
IR consumers and regenerate checkpoints. Targeted builds and both original ICG
compiler cases and the focused lifetime compile/runtime oracle pass. Generated
destruction calls carry the typed implicit-destruction marker and remain in the
analysis AST. Negative reader coverage, installed/API and full cleanup validation
remain pending.

`SgFunctionDefinition::adopt_destructor_epilogue` now requires the complete physical
field list as its second argument, excluding static members, injected lookup
aliases and union members. Pass exact `SgInitializedName` references, including
trivial and zero-length array fields. All three in-tree callers are migrated.
Rebuild external callers; no compatibility overload is retained. The AST data
layout and JSON schema224 are unchanged by this constructor-contract change.

AUD-1751 removes the project-specific VERBOSE option and custom configuration
summary. Use CMAKE_VERBOSE_MAKEFILE or cmake --build --verbose for build output,
and CMAKE_REQUIRED_QUIET for feature probes. Configuration no longer overrides
those native settings; the configuration-only check is not full acceptance.

Function declarations now own an optional typed explicit-specifier expression,
independent of the resolved function modifier. Construction metadata supplies it
before declaration publication; omission inherits the exact canonical declaration.
Schema204 includes the owned edge and explicit external-function metadata and
rejects older checkpoints. Regenerate checkpoints and rebuild consumers of the
changed IR layout. Native expression, JSON and copying controls pass on87879;
the last reader preflight correction passes required-fields on66271. The revisions
are distinct; full, installed-consumer and memory validation remain pending.

AUD-1690 adds optional typed result storage size to SgPntrArrRefExp construction.
The Clang subscript producer supplies its exact target value. JSON schema202
requires result_storage_size_bytes on array references (number or explicit null),
validates it before construction and rejects older checkpoints. Regenerate
checkpoints from source. Flang's call-parsed array references and character-literal
substrings now supply exact target measurements at construction, including DATA
substrings. General Flang/synthetic producers, physical-stride consumption,
copying/mutation review and validation are still incomplete; this is not an
accepted API/schema migration. General Flang array DataRefs now also supply
the exact semantic result rank and storage at construction, replacing the
scalar-element selection. Both readers' existing required-fields fixture and
the existing Fortran imported-component fixture have additional preservation,
rank and rejection checks; they have not run yet.

AUD-1688 removes unused ASTtools::findFirstFuncDefScope from the library and
installed header. No repository or initialized-submodule callers exist;
rebuild installed clients. No compatibility wrapper is retained.

AUD-1686 removes unused ASTtools::appendCopy, appendStmtsCopy, replaceStatement,
moveStatements and toString(VarSymSet_t) from the library and installed headers.
Repository and initialized-submodule searches find no callers. The copy helper
registers original declarations instead of copied declarations; the replace/move
APIs already abort unconditionally. Use the existing SageInterface replacement
and block-transfer APIs, and explicit typed copy/publication transactions where
copies are needed. No compatibility wrapper is retained. Rebuild installed
clients; final installation and full-suite validation remain pending.

AUD-1681 requires typed causal edges for SgRenameSymbol, using the existing
alias_causal_nodes codec in ordinary and external parameter-scope records.
Schema201 rejects older checkpoints; regenerate them from source. No AST layout
or public API changes. Missing provenance is a reconstruction error, not a
request to recover a binding by name. General rename lists may be empty; a
Fortran USE/generic reference requires its typed producer. Validation remains
incomplete.

Outliner::generateFunction accepts an optional detached leading parameter list
and consumes it during signature planning. OpenMP supplies its runtime IDs there,
before any function declaration or body is published. Existing source callers
omit the argument; rebuild native and installed clients for the changed exported
signature. AST JSON schema200 is unchanged. Validation remains incomplete:
the two reached C++ failures now pass after AUD-1678; full gates remain pending.

AUD-1678 adds a buildSemanticNonrealType overload accepting an exact source
declaration and detached typed arguments. It preserves source metadata before
publication; the existing overload and AST JSON schema200 remain unchanged.
Rebuild consumers of the new overload. General copy/rollback and installation
acceptance remain pending.

Defining function scopes inherit case semantics from their typed owner, matching
prototype parameter scopes; the process-wide builder flag no longer selects them.
Supplied Fortran definitions must already match their owner before adoption.

AUD-1659 removes duplicate chunk-fragment storage from the two OpenMP schedule
classes. Existing chunk setters/getters remain; the expression list owns the
sole fragment and host hooks visit it once. Rebuild parser and installed clients
for the class layout change. AST JSON schema200 is unchanged by this fix.

The object-storage batch adds optional nonnegative storage_size_bytes to
SgInitializedName. Schema200 requires a number or explicit null in internal and
external records; readers reject missing, negative or wrongly typed values before
allocating the name. Clang and Flang supply fixed-object bytes from their semantic
APIs. Changing a name's type clears the value; native copy construction preserves
it while rebinding equivalent types. Null does not authorize a size estimate:
Fortran threadprivate lowering requires exact bytes. Regenerate checkpoints and
rebuild native and installed consumers after the AST layout change. General type
mutation, cross-target copies and complete record/address-space layout remain
under review; this batch has not passed validation.

AUD-1614 removes the unused installed sageFunctors.h and its clone, scope,
reference, initialized-name and insertion helpers. sageGeneric.h retains live
dispatch, ancestor and error APIs; unused type-recovery, pointer/swap,
traversal/link, base-cast utilities and SG_DEREF/SG_ASSERT_TYPE/SG_ERROR_IF macros
are removed. Repository and initialized submodule searches show no consumers.
External users must migrate removed helpers to the typed builders and traversal
APIs they actually need. No compatibility layer is added; final installation
validation remains pending.

AUD-1612 adds optional typed procedure-component PASS bindings to
SgInitializedName. AST JSON schema197 requires
fortran_procedure_component_binding: null for other names, or an object with a
typed kind, optional passed_object_index and written_dummy_name. The position
selects the canonical procedure argument; the name preserves explicitly written
PASS syntax. Both readers reject missing, contradictory or misplaced bindings
before allocation. Regenerate checkpoints and rebuild native/installed consumers.
The Flang tuple Enter overload drops its unused SgType* base-type parameter and
accepts an optional typed binding after the tuple list. Repository callers are
migrated. General procedure-interface identity review and full acceptance remain
open. The native build and three affected regular controls pass with complete
source guards; no full or memory acceptance is claimed.

AUD-1608 removes unused OmpSupport::patchUpSharedVariables and
makeDataSharingExplicit from the installed omp_lowering.h and library, together
with their private helpers. No repository consumer calls either API; live
analyze_omp data-sharing analysis remains. Rebuild native/installed consumers;
there is no compatibility repair. AST layout and JSON schema196 are unchanged.

AUD-1606 removes the unused copy-function, aggregate-size and alignment parameters
from the Fortran xomp_task ABI. Its fixed arguments are now function, if-clause,
untied and triple-count, followed by the unchanged capture triples. Regenerate
lowered Fortran and rebuild the compiler, runtime and installed consumers;
previously lowered calls must not link against the changed wrapper. The C/C++
XOMP_task interface is unchanged. Existing target ABI fields now supply task
capture sizes and default-pointer width; AST layout and schema196 are unchanged.

AUD-1605 adds an optional typed evaluated_value to SgSizeOfOp and SgAlignOfOp,
their constructors and all sizeof/alignof builder overloads. AST JSON schema196
requires the property, including explicit null for an unavailable value. Clang
supplies constant values; dependent and runtime-size expressions have none.
Both readers reject signed values and zero alignment before trait construction.
Regenerate checkpoints and rebuild native/installed consumers. Focused linked
controls pass on recorded source; full acceptance, independent OpenMP producers
and dependency-edit validity remain open.

AUD-1594 removes the unused SageInterface::rebuildSymbolTable public API. Construct
symbols with their declarations or through the existing exact native-copy
transaction; clearing published tables and scanning declarations leaves existing
references stale. No compatibility repair is retained. Rebuild native/installed
consumers; AST layout and JSON schema195 are unchanged.

AUD-1563 makes a named type's declaration a semantic reference rather than a
traversal child. Nonreal declarations construct and own their exact types;
typedef constructors reuse the native mapped type. Type/declaration identity is
unchanged for external borrowed aliases. AST/JSON layouts and schema194 remain
unchanged; rebuild generated native code for the copy/traversal/lifetime changes.

AUD-1558 adds SgNonrealRefExp::is_non_odr_use and advances AST JSON to schema194.
Clang supplies the per-reference classification for resolved variable templates;
native copying preserves it and both readers require an explicit boolean.
True without a resolved variable template is malformed. Regenerate checkpoints
and rebuild native/installed consumers for the layout and constructor change.
Direct NFI construction now requires (symbol, is_non_odr_use). Ordinary unresolved
references use false; resolved variable producers supply Clang's exact use state.

AUD-1557 gives all three buildVariableDeclaration name overloads an optional
final SgType* cxxSourceType argument. Supply a distinct, semantically equivalent
written type at construction instead of setting cxx_source_type after symbol
publication. The non-owning typed edge and AST/JSON layouts are unchanged;
rebuild native and installed consumers for the changed builder ABI.

AUD-1516 adds typed is_non_odr_use to SgVarRefExp and advances AST JSON to
schema193. Clang supplies the classification; both readers require a boolean
before reference construction. Regenerate checkpoints and rebuild native and
installed consumers after the layout change. Preserve the old one-symbol
constructor through delegation. Focused controls pass on recorded source;
generated-program, whole, installed and memory acceptance remain incomplete.

AUD-1510 adds typed explicit/implicit inline state to SgVariableDeclaration
and advances AST JSON to schema192. Native copying keeps the enum; both readers
require a valid value and reject inline-variable state on non-variable
declarations before construction. Only a written keyword is emitted;
template instantiations retain inline semantics without inventing a specifier.
Regenerate checkpoints and rebuild consumers. The final native build, native/JSON
member controls, required fields and original emitted syntax pass on recorded
source. Whole, installed and memory acceptance remain pending.

AUD-1486 adds a deleteAST overload accepting a callback on the complete
unprotected deletion boundary, after reference proof and before destruction.
Frontend indexes use that boundary to retire dependent types and owned syntax
without a separate traversal. Existing overloads and AST/JSON layouts are
unchanged; the callback must not mutate AST references. The native build and
three affected controls pass on the recorded source; full acceptance is pending.

AUD-1473 adds SgEnumDeclaration::promotion_type and advances AST JSON to
schema191. The frontend records Clang's actual canonical promotion type before
publishing the enum family; storage/source underlying types remain separate.
Unresolved declarations may have no promotion type, and dependent types retain
their typed identity. Native copying clones that dependency through SgCopyHelp.
Both readers require the new property and construct it before enum publication.
Regenerate checkpoints and rebuild native/installed consumers for the changed
node layout. Seven affected native/copy/reader/target controls pass on the
AUD-1476 source; whole, installed and memory acceptance remain pending.

AUD-1471 removes the uncalled StructCustomizedSizes and custom/native-system
primitive layout generator APIs from abiStuff.h. ChainableTypeLayoutGenerator
no longer stores or accepts the unused custom-size pointer; its public layout
changes. Rebuild native and installed clients. The existing i386/x86-64/nonpacked
interfaces remain. AUD-1472 corrects character promotions using existing target
ABI metadata without changing Sage node layout or JSON schema190.

AUD-1464 adds strict requireTargetIntegerDomain/requireTargetFloatingDomain
queries in abiStuff.h. They require an owning source file with exact typed ABI
metadata and reject missing layouts; numeric widths no longer come from project
32-bit mode. No schema or node-layout change is required for this batch.

AUD-1463 widens SgWcharVal's value and buildWcharVal APIs to int64_t and removes
the redundant valueUL accessors from wide/UTF character nodes. Use get_value
and set_value. Scalar ABI layouts now carry target signedness; Clang also
provides wide/UTF and half/bfloat layouts. Flang records unsigned KIND layouts
explicitly. Schema190 requires is_unsigned for every scalar ABI layout and
validates ABI data before source-file allocation. Regenerate checkpoints and
rebuild all native/installed consumers. Full acceptance remains open.

AUD-1462 widens SgCharVal's value/constructor/accessor and buildCharVal APIs from
host char to int64_t. Character nodes retain their typed family and literal_type;
Clang's target width/signedness determines the exact value before construction.
Native copying and both JSON readers preserve that domain. Generated emission
uses the typed value/type; source spelling remains separate. Schema189 requires
regenerated checkpoints. Rebuild all native and installed consumers for the
changed node layout and builder ABI; complete acceptance remains open.

AUD-1461 advances AST JSON to schema188. SgSignedCharVal, SgWcharVal, SgChar16Val
and SgChar32Val require value and value_string; SgFloat16Val, SgBFloat16Val,
SgFloat32Val and SgFloat64Val require value_bits and value_string. Floating
values retain their native 32/64-bit storage, including signed zero and NaN
payloads. Both readers use the existing early
metadata validation and range/bit codecs. Regenerate older checkpoints; native
AST fields, layout and public APIs are unchanged.

AUD-1451 advances AST JSON to schema187. Typedef records require
type_elaboration_required_for_base_type; template-argument records require
type_elaboration_required. sizeof/alignof records also require that flag and
source_type_elaboration_kind, including type-owned expression descriptions.
These preserve existing typed AST fields that the writer previously omitted.
Validate presence before allocation and restore fields
in the construction producers before publication. Regenerate older checkpoints;
native AST fields, layout and public APIs are unchanged. Canonical comparison
cannot detect information omitted from both serialized graphs.

AUD-1446 adds source_parameter_scope and source_parameters to
SgFunctionParameterTypeList and source_trailing_return to SgFunctionType.
Schema186 requires source_argument_list and source_trailing_return in function
type descriptions and source_type_identity on C++ pointer/reference/modifier
wrappers containing private prototypes. Prototype containers are node records
with exact parameter-list/scope edges; canonical functions cannot own them.
Existing array/member-pointer/function identities remain; the shared native type
identity uses one allocator. Regenerate older JSON and rebuild consumers.

SgType::has_source_prototype recognizes private prototypes through wrapper bases.
Construction establishes ownership; native copying constructs scopes, parameter
symbols and wrapper bases before dependent references. Argument positions release
their exact type parent on destruction. The handwritten argument-list constructor
initializes both prototype owner pointers. Underlying-type emission substitution
and position-zero-to-this guessing are removed; abstract positional references
cannot supply source spelling.

The nested alias control passes source reconstruction/comparison, copying before
finalization and strict deletion on source-prototype-wrapper-json-identity-20261007.
AST emission with token unparsing disabled passes stock Clang22 syntax checking.
Canonical identity/lifetime and unchanged qualifier rejection controls pass on
their recorded sources. The existing schema186 required-fields driver passes
strict prototype-field and repeated wrapper identity rejection/ownership checks.
AUD-1447 preserves void in empty C prototypes using their typed source owner;
C11 compatibility assertions and adjacent C++ structure pass with checkpoints.
General source ownership, external readers, allocation rollback,
installed consumers and all full acceptance gates remain open.

AUD-1410 adds LoopTreeNode::IsAtomicStatement. Atomic statement emission has no
child body; IF wrappers remain controls. Shadow nodes forward this role and source
identity. Rebuild installed loop-tree consumers for the changed virtual interface.
Sage AST and JSON schema184 are unchanged; installed/full/memory acceptance remains
open.

AUD-1409 adds LoopTreeNode::GetSourceAst for exact dependence construction;
GetOrigStmt retains its existing statement/condition analysis role. Source loop
and IF wrapper layouts change, so rebuild consumers of the installed loop-tree
headers. LoopTreeCreate::CreateIfCond now requires the controlling source
statement and its condition, rather than only the condition. Generated loops
have no source identity. The explicit non-Fortran loop boundary now applies to
both tree and dependence construction. Native Sage AST and JSON schema184 are
unchanged; full/installed/memory acceptance and IF reconstruction remain open.

AUD-1401 adds AstInterface::IsIfWithKnownRuntimeBranch. A true result supplies
the selected runtime body (possibly absent) and the runtime condition declaration
(possibly absent); constexpr expression tests and consteval tests have no runtime
condition evaluation. Legacy CFG and statement-effect collection consume this
typed selection. Native AST layout and schema183 are unchanged. Source-generating
loop-tree consumers still require separate review; focused checks are not full
or installed acceptance.

AUD-1390 advances AST JSON to schema181. Inline type-owned expression and list
records require original_expression, including an explicit null reference when
absent. Written source trees retain exact typed identity and exclusive ownership;
missing, already-owned, chained or operand-aliased sources are rejected. Regenerate
older checkpoints. Native fields, layout and public APIs are unchanged. AUD-1391
constructs imported parameter ownership as its exact graph dependencies become
available. Seven affected controls pass; general and full/memory acceptance are open.
AUD-1392 also rejects null/nonzero expression references carrying inline fields
before lookup or allocation. Reference records contain only node; anonymous
owned expressions use node0 plus owned_kind and their required typed fields.
The schema is unchanged; two affected controls pass on its recorded source.

AUD-1381 constructs explicit-instantiation class/struct/union tags from exact
expanded tokens and validates cached/directive/definition roles without repair.
Existing typed tag fields, public APIs, native layout and schema180 are unchanged.
Seven focused controls pass; broader reader/copy and full/memory gates remain open.

AUD-1379/1380 construct distinct instantiated member class/variable-template
primaries in their exact semantic owners. Consumers must preserve those typed
identities instead of substituting source-template declarations. Existing
ownership/type/canonical fields and schema180 are unchanged. Focused native
and JSON controls pass; installed/full/memory acceptance remains open.

AUD-1378 requires cached specializations to retain their construction-time owner
and canonical symbol. Producers attach ownership before caching; cache hits
validate it. Public APIs, native layout and schema180 are unchanged. Twelve
focused controls pass; general construction and installed/full/memory gates
remain open.

AUD-1377 requires exact Clang declaration contexts and canonical namespace
metadata for type-specialization ownership. Namespace/default-scope recovery is
removed. Public APIs, native layout and schema180 are unchanged; 17 focused
controls pass. Nested-record publication and installed/full/memory gates remain
open.

AUD-1376 shares namespace construction between eager and on-demand translation
and requires exact cached ownership before returning a namespace context.
Public APIs, native layout and schema180 are unchanged. Twelve selected controls
pass; remaining template-scope repairs and installed/full/memory gates are open.

AUD-1375 makes IsMemberAccess return an exact typed selector for ordinary,
dependent and variable-template data members, while callable selectors return
false. Public signatures, native AST layout and schema180 are unchanged. The
strengthened native/copy/JSON control passes after rebuilding AUD-1374's affected
consumers and relinking the library. Installed/full/memory validation remains open.

AUD-1374 removes unused AstInterface block-first/block-last/block-size queries
and read/write/print/null-statement factories, the unused implementation NewClass
factory, and two implementation declarations without definitions. Existing
GetBlockStmtList and SageBuilder statement factories remain available. The
public annotation CreateMethodRef remains. Three affected objects compile and
AUD-1375 subsequently relinks the library; installed/full validation remains
open. Native AST layout and schema180 are unchanged.

AUD-1373 gives static-member accesses their selected declaration's identity
independently of the receiver. Unique keys retain complete template arguments;
static pointer syntax does not imply indirect storage. Static membership comes
from the exact typed canonical source declaration, preserving written storage
syntax. Native/copy/JSON checks pass; regenerate saved analysis keys. Native
layout and schema180 are unchanged. General and full/memory acceptance remain open.

AUD-1372 removes unused CPPAstInterface::IsPointerVariable/GetVarScope/IsPlusPlus.
Member-call queries now preserve static arguments and return the exact callee
expression for qualified and direct calls; selected templates use typed base-name
metadata. The strengthened annotation control passes. Installed and full/memory
validation remain open.

AUD-1371 changes variable-template mangled names and analysis/expression keys to
include complete typed semantic arguments, with a distinct primary-template
identity domain. Regenerate saved analysis signatures. Native layout and schema180
are unchanged. The completed cleanup also removes unused AstNodePtr2Sage and
redundant empty destructors. The native rebuild and six affected controls pass,
including native/copy/JSON/runtime identity. Installed and full/memory validation
remain pending.

AUD-1368 adds owned `semanticTemplateArguments` to variable-template declarations.
Their constructors and builders now require the complete semantic list separately
from written specialization arguments and selected-template deduced bindings.
Primary templates pass an empty list; partials preserve their semantic pattern.
JSON schema180 requires `semantic_template_arguments`, matching typed edges and
owners; missing metadata is rejected. Rebuild all native/installed consumers and
regenerate checkpoints. The native rebuild and ten focused controls pass on the
recorded fixtures manifest. AUD-1369 uses semantic arguments for variable-symbol
lookup and requires matching arguments as well as parameters. Its corrected
witness fails before the fix; the generated-method build and five focused
controls pass on the lookup manifest. Required-fields Memcheck passes; the real
AST fixture fails in two independently reproduced, preexisting Clang evaluator
contexts (AUD-1370). No memory waiver is added. General
template identity, memory and full acceptance remain open.

AUD-1367 requires exact class-specialization primary lookup and semantic context;
alternate-key/symbol/retranslation recovery and scope substitution are removed.
Native layout, public interfaces and schema179 are unchanged; eight focused
controls pass. General construction and full acceptance remain open.

AUD-1365/1366 preserve selected partial constraint ownership and collect complete
Clang instantiation argument levels before evaluating class, variable and
function constraints. Two private evaluator overloads and silent fallbacks are
removed. Native AST layout, public APIs and schema179 are unchanged; eleven
focused controls pass, including strengthened partial and nested cases. General
template construction, memory, installed and full acceptance remain open.

AUD-1363 resolves variable-template specialization scopes directly from Clang's
typed DeclContext before child construction, removing scope-stack/global
substitution. Public interfaces, native layout and schema179 are unchanged;
six focused controls pass. Full semantic-argument identity and acceptance remain
open.

AUD-1364 removes duplicate variable-template visitor normalization; constructor
defaults and exact builder checks remain authoritative. No interface or layout
change; the final six focused controls pass.

AUD-1361 places traversal templates in ProcessAstTree.h, eliminating its missing
installed implementation dependency. The unused SkipOnly and BEFORE_AT API are
removed; rebuild traversal consumers. AUD-1362 retains the base collector's
actual/formal binding contract while excluding expression-binding records from
concrete memory reads. Incoming parameter reads remain. Native AST layout and
schema179 are unchanged. Public-header instantiation and focused controls pass
on their recorded manifests; general effects, installation and full gates remain
open.

AUD-1359 removes member-name substring stripping and returns exact access types;
data member-pointer names now distinguish object, selector and operation.
Regenerate affected saved reference names. AUD-1360 preserves location operand
reads and routes dereferences through memory collection. Public signatures,
native layout and schema179 are unchanged; two focused controls pass. General
identity/effects, installed, memory and full acceptance remain open.

AUD-1356 composes GetGlobalUniqueName from typed semantic scope/function identities
and an unqualified leaf, without detecting qualification in the supplied string.
Global variable/signature output now distinguishes lexical scopes and overloads;
member references qualify their base once. The explicit function callback remains.
AUD-1357 uses exact qualified named types through CV/reference/pointer wrappers,
and native type identities for typed local owners, retaining primary-template
annotation families. Regenerate affected saved
dependence/signature and annotation names. AUD-1358 removes the unused public
ReferenceDimension and ReferenceStride queries and their obsolete implementation
and comments. Native AST layout, public AstInterface signatures and schema179
are unchanged; the affected build and two focused controls pass. Broader identity,
installed, memory and full acceptance remain open.

AUD-1353 keeps GetTypeInfo/GetBaseTypeName interfaces but derives base names
through typed CV/reference/pointer nodes. Names beginning with const remain
intact. AUD-1354 compares converted member references for both dot-star and
arrow-star without dropping conversions. AUD-1355 removes the unused public
AstInterface::IsArrayType query, including its ignored annotation argument.
External callers must use typed array queries. Native layout and schema179 are
unchanged; three focused controls pass, while installed/memory/full acceptance
remains open.

AUD-1352 namespace and using-directive signatures preserve enclosing scope.
Typedef signatures retain the typedef_ prefix followed by the existing semantic
typedef-type identity, including alias instantiations. Template class definitions
use their declaration signature. Regenerate affected saved dependence/annotation
signatures. Native/public interfaces and schema179 are unchanged; two focused
controls pass, while installed, memory and full acceptance remain open.

AUD-1350 class declaration signatures now contain the exact semantic class-type
identity after the existing class_ prefix. Same-named namespace classes and
template instantiations remain distinct. Regenerate saved dependence/annotation
signatures that relied on the old unqualified class name. Public signatures,
native layout and schema179 are unchanged. AUD-1351 constructs known enum
scope/auxiliary ownership before dependent typedefs, without changing interfaces
or serialized data. Three focused controls pass; installed, memory and full
acceptance remain open.

AUD-1349 removes the always-false AstInterface::IsIOInputStmt/IsIOOutputStmt
queries and AstTreeDepGraphAnal::ComputeStmtDep. Use the existing ComputeDataDep
entry for graph dependency analysis; DEPTYPE_DATA includes DEPTYPE_IO. The unused
BuildAstTreeDepGraph input/output stacks are removed, changing that native class
layout. Rebuild its consumers. Active I/O dependence analysis remains unchanged;
installed consumers and full acceptance remain open.

AUD-1348 changes AstNodeVisitMapping::MappingType keys from std::string to
const SgNode pointers and removes its unused MappingPairType alias. Consumers
must pass exact AST identities rather than formatted addresses. PDF navigation
uses typed data-member references. Native AST layout and schema179 are unchanged;
the focused output control passes, but installed/full/memory acceptance is open.

AUD-1333 removes the private scope-only enum reader factory. The existing
NodeMap factory requires constructed canonical/type owners and the underlying
type; graph reading uses the shared declaration worklist. AST layout, public
interfaces and schema178 are unchanged. Nine focused controls and one
required-fields Memcheck pass; full acceptance remains open.

AUD-1330 removes SgClassDeclaration's unused `explicit_annotation_interface`,
`explicit_interface`, `explicit_enum` and `explicit_anonymous` fields and
generated accessors. No source producer or consumer remains. Native metadata
and layout change; rebuild the library and all consumers against regenerated
headers before testing. Neither JSON path encoded these flags, so schema178
is unchanged. Generation passes; AUD-1331 rebuilds the library and five affected
drivers with73 focused passes. Remaining and installed consumers and full
acceptance remain open.

AUD-1328 removes SgProject's unused suppressConstantFoldingPostProcessing field
and generated accessors, plus the ignored CLI option. Native layout changes;
rebuild consumers. Neither JSON path encoded this project field, so schema178
is unchanged. Native regeneration and eight affected CTests pass. The retired
option is rejected by the existing CLI invariant. Installed consumers and full
acceptance remain open.

AUD-1324 extends the existing embedded-tag typedef builders to accept an exact
detached canonical nondefining class/enum introduction as well as a definition.
They adopt it before publication and preserve the definition-only flag and
strict type/symbol/ownership checks. Signatures, AST layout and schema178 remain
unchanged; installed consumers and full acceptance remain open.

AUD-1321 removes the unused template-class owner-scope enum,
`templateClassOwnerScopeKind` and `sourceSpelledTemplateClassOwner`, including
their generated accessors. Written enclosing headers use the existing owned
`sourceSpelledTemplateHeaders` list instead of synthetic class families.
Native AST layout changes; rebuild consumers against regenerated headers.
Neither retired field was encoded by either JSON path, so schema 178 is
unchanged. Regeneration and ten combined focused controls pass; installed
consumers and full acceptance remain open. AUD-1322 makes the existing
function/variable/class source-header lists owned clone trees. Native copies
retain independent headers and reciprocal parents; no additional layout,
signature, traversal visibility or JSON format changes.

AUD-1312 adds declaration_ownership::semanticLexical() for direct publication
inside an exact semantic scope. It preserves compiler-generated provenance and
requires one lexical child edge; source append APIs retain their rejection of
nonautonomous tags. Existing ownership-kind values and descriptor/AST layouts
are unchanged. Thirty-two focused controls pass; installed consumers and full
acceptance remain open.

AUD-1310 adds declaration_ownership::semanticEmbeddedDeclaratorChild() for
detached class/enum definitions with compiler-generated frontend provenance.
Existing embeddedDeclaratorChild() retains transformation provenance; existing
ownership-kind values are unchanged. Typed variable/alias builders adopt the
exact detached definition without intermediate auxiliary publication. No AST
layout or JSON schema change. The strengthened local-tag control and23 related
controls pass; broader and installed-consumer acceptance remain open.

AUD-1302 maps argument reads to the selected callee's defining parameters when
a definition exists, including inherited constructors; external declarations
use their own parameters. Member receiver reads do not consume explicit formals.
Unqualified template operator() uses the same receiver slot as other member
references. AUD-1303 retains explicit cast callee expressions and their typed
conversion edges; declaration lookup no longer invents a direct target by
stripping them. Template-member queries retain semantic specialization identity.
No signature, layout or schema change; installed-consumer acceptance remains open.

AUD-1299 makes AstInterface::IsFunctionCall append exact SgInitializedName
identities for inherited fixed arguments. Ordinary arguments remain expressions;
the returned SgConstructorInitializer retains typed forwarding/variadic context.
Consumers must read incoming values without traversing declarations or defaults.
Argument read collection maps them to the selected constructor's formals;
AUD-1302 uses the exact defining owner where available.
AUD-1300 includes mutable rvalue references in out-arguments, preserving const
exclusion. Signatures, AST layouts and schema178 are unchanged; full variadic,
conditional-execution and installed-consumer acceptance remain open.

AUD-1297 refreshes analysis-owned CFG, alias, hierarchy and call-graph state on
each run. Do not retain internal graph or analysis pointers across runs.
CompReprBase now releases its owned graph nodes and edges as well as the graph;
semantic AST payloads remain borrowed. The shared AliasAnalysisDetail cleanup
supports the inline destructor. No AST layout, schema or callable public
signature changes; installed-consumer acceptance remains open.

AUD-1291 alias analysis uses exact AST identities: `AliasRelationNode::var` and
the identity overloads of `computeAliases` take `SgNode*`, restricted to
`SgInitializedName` and `SgNewExp`. Variable callers pass the symbol's declaration;
allocation callers pass the existing expression. `getAliases` returns types
rather than synthetic symbols. Public signatures/vtables change; rebuild
installed consumers. AST node layouts and schema178 do not change in this
batch. AUD-1293's `getPropertiesForExpression` appends targets using
`canonicalFunctionDeclForCallGraph` identity, preserving existing entries.
AUD-1292 exposes the shared typed nonexecution query in CallTargetSet.
Focused controls pass after the recorded fixes; full acceptance remains open.

AUD-1288 inherited-constructor translation adds the typed
`SgConstructorInitializer::argument_source_enum` constructor argument and
`argument_source` property. Inherited calls use the enclosing constructor's
incoming argument context through their existing base-initializer ownership;
they do not create expression operands or additional copies. The virtual-base
inheritance role preserves Clang's distinct forwarding condition. The `_nfi`
builder accepts the role at construction, with the ordinary-expression role
as the default. Native layout and ABI change; JSON schema 178 requires
regenerated checkpoints. Both readers require the property and reject invalid
roles; inherited calls require a constructor base-initializer context. This
batch compiles; the original compiler failure, imported inherited-constructor
specimen, frontend/roundtrip contracts and required-field checks pass. Focused
required-fields Memcheck reports zero errors, suppressions and lost bytes.
General call/alias/live-variable consumers and linked inherited copying remain
under review; full acceptance remains open.
Direct native constructors without file information must pass the argument
source explicitly; the two existing callers are migrated.

AUD-1261 removes the unsupported SgNonrealType allowance for canonical type
parameters from native and JSON validation. Reject it before construction;
other nonreal-type roles, native layout and schema 177 are unchanged.

AUD-1258 binds canonical template-parameter types before private reader
publication and removes the later repair scan. Native layout and schema 177
are unchanged; defaults, constraints and recursive construction remain open.

AUD-1256 removes `SgIOStatement::io_statement_enum`, its stored field and
get/set accessors. Consumers use the concrete AST node kind. Native I/O layouts
change and schema 177 requires regenerated checkpoints. Serial compilation,
focused identity/rejection and actual roundtrip/runtime controls, and selected
reader Memcheck pass; full acceptance remains open.

AUD-1255 constructs scalar literal semantic types before reader publication and
removes their late recursive repair pass. Schema 176 and native layouts remain
unchanged; recursive dependencies and whole-reader rollback remain open.

AUD-1252/1253 initialize declaration-scope/variable roles and expression emission,
brace and qualification state before reader publication. Expression wrapper masks
must fit the existing native bit range; oversized values fail before conversion.
AUD-1254 carries diagnostic context through the private type decoder and rejects
unresolved results before publication. Schema 176 and public AST layouts remain
unchanged; general construction and full acceptance remain open.

AUD-1250 initializes function source/emission scalar state and validates typed
anonymous-name metadata before graph publication. Native layout and schema 176
are unchanged. Graph dependencies and external-peer metadata remain under review.

AUD-1249 initializes common declaration modifiers and source flags before reader
publication, removing duplicate late restoration. Schema 176 and native layout
are unchanged; incomplete declaration payloads fail during construction.

AUD-1247 makes frontendExternalFileList an owned, independently cloned native
edge while preserving its exclusion from lexical traversal. AUD-1248 constructs
the exact serialized FOR initializer through the existing typed constructor
and initializes that constructor's construction-owner pointer. Native layout,
public signatures and schema176 are unchanged. Focused reconstruction, lifetime
and rejection checks pass; full acceptance remains open.

AUD-1246 removes the unused Flang TypeAttrSpec enum conversion and late
derived-type builder Leave overload. Typed modifiers are set before source
declaration publication; valid local BIND(C) emits directly. Native layout and
schema176 are unchanged. Seven focused controls and one selected Memcheck pass;
full acceptance and imported-file graph ownership remain open.

AUD-1245 adds native `SgBaseClass::fortran_source_derived_type_symbol` and
advances JSON to schema 176. Rebuild consumers and regenerate checkpoints.
Fortran inheritance retains the exact visible class or USE-associated alias
separately from its canonical semantic parent. `SgClassDefinition` now owns
and destroys its cloned inheritance list. Eight focused regular controls and
two selected Memchecks pass; full acceptance remains open.

AUD-1243 adds native `SgIfStmt::else_construct_name` for the written Fortran
ELSE name and advances JSON to schema 175. Both readers require the field;
regenerate older checkpoints. Rebuild consumers for the changed `SgIfStmt`
layout. Four focused reconstruction/copy/emission controls and selected reader
Memcheck pass; full acceptance remains open.

AUD-1241 removes two unused protected, nonvirtual declaration helpers:
`generate_alternative_name_for_unnamed_declaration` and
`generate_alternative_name_for_unnamed_declaration_in_scope`. Retained consumers
have no calls. AST layout, virtual tables and schema are unchanged; generated
outputs pass inspection, while compilation and installed-consumer gates remain open.

AUD-1240 advances AST JSON to schema 174. Basic blocks require the native
`is_implicit_control_flow_scope` boolean; omitted data and conflicting Fortran
roles are rejected before allocation. Regenerate older checkpoints. Range loops
restore exact typed children and variable scopes during private reconstruction.
Native layout and public API are unchanged; full acceptance remains pending.

AUD-1233 makes function-type index retirement native. Types record their exact
table; clearing or destroying a table detaches its borrowed types. Rebuild
consumers for the private layout changes to `SgFunctionType` and
`SgFunctionTypeTable`; public signatures and JSON schema are unchanged.
Eight focused native/copy/reader/runtime checks and two focused Memchecks pass;
full acceptance remains pending.

AUD-1231 adds the GNU selector to `SgGotoStatement` construction. Constructors
without file information require `(label, selector)`; direct/Fortran callers
pass a null selector. The file-information overload has an optional selector.
Native construction owns the supplied expression, and reconstruction rejects
later target changes. Rebuild consumers; AST layout and JSON schema are unchanged.
Focused native, copy, reconstruction and memory controls pass; full acceptance
remains pending.

AUD-1228 makes pointer/reference/modifier cache retirement native and removes
reader-specific cache clearing. Type destruction removes its exact global
symbol index; reverse caches are weak metadata during isolation checks.
Public signatures, AST layout and schema are unchanged. Five focused regular
checks and two focused Memchecks pass; full acceptance remains pending.

AUD-1226 removes `unsetNodesMarkedAsModified`, its `checkIsModifiedFlag.h`
header, and `SageInterface::resetModifiedLocatedNodes`. Postprocessing and
source builders preserve real edits. Frontend and reparsed-file initialization
use `SgNode::SourceStateConstruction` for fresh nodes; it does not suppress
mutations of preexisting nodes. Plugin execution remains outside the frontend
construction scope. Rebuild consumers; no compatibility reset shim is provided.

AUD-1227 preserves real edits during normalization and semantic retirement.
Normalization cleanup consumes its records, moves exact symbols/comments back
and deletes emptied wrappers; repeated cleanup is safe. Modifier-cache setters
preserve borrowed base edit state. API signatures, AST layout and schema are
unchanged; full acceptance remains pending.

Member-pointer cache retirement moves from `deleteAST` to its generated
destructor (AUD-1225). `forgetCanonicalSemanticType` becomes private; no retained
source caller needs explicit retirement. Normal destruction and AST retirement
remove only the matching canonical entry, leaving noncanonical wrappers and
their borrowed owners distinct. Rebuild consumers for the header change.
AST layout and JSON schema are unchanged. Five focused regular checks and
required-fields Memcheck pass; installed consumers and full acceptance remain
pending.

SgTypeComplex and SgTypeImaginary add `findCanonicalSemanticType(base_type)`.
Their existing factories share exact typed cache validation; generated
destructors retire only the matching cache entry. Reader-created fresh COMPLEX
types enter the existing ownership journal (AUD-1224). Rebuild consumers for the
generated header change. Layout and schema are unchanged.
Nine focused regular controls and required-fields Memcheck pass; installed
consumers and full acceptance remain pending.

Reverse pointer/reference cache setters preserve their base type's source edit
flag, consistent with native factories. Semantic intrinsic/COMPLEX reads no
longer write source defaults to borrowed canonical types (AUD-1223). Public
signatures, layout and schema are unchanged; full acceptance remains pending.

SgPointerMemberType adds `findCanonicalSemanticType(base_type, class_type)` for
exact typed cache lookup. The factory and canonical check share its hard cache
invariants; readers distinguish fresh allocations from borrowed representatives
before publication (AUD-1222). No existing signature, AST layout or schema changes.
Generated-header build and five focused linked checks pass; installed-consumer
and full acceptance remain pending.

declarationPreceedsDefinition retains its signature and compares structural
preorder within one lexical AST instead of always returning false. Inputs require
unique traversal ownership paths. The prototype-chain control passes; full
acceptance remains open (AUD-1211).
Valid detached copies use their shared detached root; unrelated roots remain
an error.

Class instantiations retain the canonical declaration of their exact selected
primary or partial template. Construction and cache lookups use that same
typed entity; primary and partial selections remain distinct (AUD-1212).
Public signatures and AST/schema layouts are unchanged.

Inliner cleanup preserves typed volatile loads/stores and value snapshots.
The undeclared implementation helpers `inSimpleContext`,
`doSubexpressionExpansion` and `doSubexpressionExpansionSmart` are removed with
their ineffective caller pass. Supported `inlinerSupport.h` interfaces and
AST/schema layouts are unchanged; installed-consumer validation remains open.

Schema 167 adds `SgInitializedName.ctor_initializer_field_target` and
`ctor_initializer_source_order`. Member initializer entries reference their exact
field; the optional ordinal is present only for written initializers. The owned
list retains semantic execution order, while emission projects written entries
in source order. Both readers require the fields and reject contradictory roles
before allocation. Generated copies bind field references through the existing
identity map. Rebuild consumers because the initialized-name layout changes.
This batch is under validation, not accepted.

Materialized explicitly defaulted functions retain a semantic definition and
body beneath their written declaration. The `= default` declaration remains the
emission surface. Constructor call targets use the shared canonical function
query, which prefers a defining declaration with its body; native identity
checks now use that same contract rather than first-nondefining identity.

Unwritten semantic casts use `e_implicit_cast` for their source surface while
retaining the exact conversion kind, value category and base path. Their target
type is semantic; they own no written TypeLoc or inline-tag definition. Both
cast producers publish canonical implicit-conversion provenance at construction.
No cast schema or reader invariant changes.

Schema 166 appends typed C++ constructor properties to SgFunctionModifier's
serialized vector. The frontend preserves Clang's default-constructor and
triviality facts across declaration families; both readers reject inconsistent
flags before construction. Hand-built constructors used by callgraph filtering
must supply setConstructorProperties; queries assert when facts are absent.
The modifier object layout is unchanged. Rebuild consumers for the new API and
enum bounds; older JSON schemas remain rejected.

Callgraph construction uses exact canonical declaration identities and excludes
other live projects. Function-pointer target queries require their expression's
typed project owner and share one resolver. Public signatures are unchanged;
rebuild consumers instantiating CallGraph.h templates because private recovery
helpers are deleted. Eligible callees are retained before graph emission;
edge emission no longer creates nodes or substitutes template-pattern nodes.
AUD-1167 fixes AUD-1166's constructor-initializer omission;
sixteen affected controls pass on its recorded source. Full acceptance remains open.

AUD-1173 uses shared typed equivalence for callgraph signatures, recognizing
separately owned dependent wrappers bound to the same exact target. No API,
AST layout or JSON schema changes.

AUD-1174/1175 remove return-covariance inference from virtual dispatch and use
typed cast categories. Member-pointer queries require a class hierarchy and
retain exact inherited candidates through pointer conversions. Public signatures
and AST/schema layouts are unchanged.

AUD-1176 replaces ClassHierarchyWrapper::MangledNameToClassDefsMap with
ClassDefMap, keyed by exact SgClassDefinition pointers. The wrapper's layout
changes; rebuild installed consumers. Identical spelling in another project
does not confer class identity. Cross-translation-unit declaration unification
remains a separate frontend task.

Semantic constructor definitions retain written initializer expressions under
their exact definition owner. Constant template-expression arguments use Clang's
complete converted value; source expressions retain their independent typed
spelling. Semantic variable-template initializers retain their original
construction provenance. AUD-1168's parameter worklist accepts an already
supplied typed owner without requiring its serialized record. Public signatures
and schema 165 are unchanged; constructor semantics and reader rollback remain
under review.

Schema 165 requires `SgAggregateInitializer.source_element_roles` in graph and
inline JSON. Empty roles represent ordinary source elements; nonempty roles
retain the producer's exact include-expansion and separator ownership map.
Both readers reject invalid roles and mismatched element counts, and canonical
comparison now includes the field. Earlier schemas cannot establish this
roundtrip contract and remain rejected. No AST layout or public API changes.

Generated copies bind initialized-name declaration/scope references during
private construction and close function-definition ownership before body copies.
`SgCopyHelp` now carries typed pending-reference bindings and requires them to
resolve before root construction ends. Rebuild consumers: its class layout
changes. `COPY_REFERENCE` is ROSETTA metadata, not a serialized AST field;
the copy change itself leaves the serialized shape unchanged. Exact external
references remain borrowed. Signature
dependency construction first collects its typed declaration closure, constructs
class owners ahead of function consumers and processes defining declarations
first. Copy assignments retain an already-bound exact child instead of deleting
it. AUD-1156's final build and twelve focused regular checks pass; recursive
header dependencies, canonical publication and full copy acceptance remain open.

CUDA launch CFGs now evaluate their owned execution configuration before kernel
arguments. Their call phase is index3 and end is index4; ordinary calls retain
indices2 and3. Derive a function call's interprocedural phase from
`cfgIndexForEnd() - 1`; the fixed `SGFUNCTIONCALLEXP_INTERPROCEDURAL_INDEX` macro
is removed. Configuration constructors require grid/block operands and adopt
distinct unowned children. No JSON schema or constructor signature changes.
Focused CUDA/copy/ownership and ordinary CFG/dataflow checks pass (AUD-1155),
as do the three constructor copy/elision controls. Whole interprocedural and
acceptance gates remain open.

Schema 164 replaces `SgCxxDefaultArgumentExp` with `SgCxxDefaultInitExp`.
Construct it from the exact `source_declaration`, owned expansion and explicit
`e_parameter`/`e_member` kind. Member defaults retain field identity and use the
same copy, CFG and emission support. Both readers require the kind and validate
the corresponding declaration ownership before construction. The member
declaration currently requires a collected node identity. Graph variable lists
are adopted in the typed worklist before default-use construction; final edge
linking verifies those lists. Twelve focused checks, original/emitted C++20
runtime checks and required-fields Memcheck pass (AUD-1153). Omitted
aggregate/constructor producers, external field codecs and full acceptance
remain open.

Schema 163 adds `SgCxxDefaultArgumentExp`, owning one expansion and borrowing
its exact parameter identity. Both readers require the typed inputs at
construction. Constructor elision accepts one source operand followed by typed
defaults; its CFG bypass precedes those defaults. Source lists project written
arguments without using file-info default flags. The build and six affected
native checks pass; required-fields Memcheck passes, but constructor Memcheck
fails. Copy ordering, frontend binding families and full acceptance remain open.

`SgNewExp` and `buildNewExp[_nfi]` now take the implicit allocation-parameter
count after the selected allocation declaration. Supply the exact LLVM count
for frontend allocations and one for generated ordinary allocations. The
count is available when construction validates default placement arguments;
both readers use the same binding check before allocation. Schema163 already
contains this count, so its serialized shape is unchanged. Written placement
syntax excludes typed defaults, including allocations with no written list.

Schema 162 replaces `SgNewExp::array_bound_is_implicit` with the required
`array_bound_source_form` enum and owned `array_bound`. `SgNewExp` and
`buildNewExp[_nfi]` take the specified source type, exact result pointer, bound
and source form before the existing placement/initializer/operator arguments.
The result pointer is retained at construction; `get_type()` never creates it
from source syntax. Copying and CFG traverse the bound. Both JSON readers require
the same inputs and reject contradictory roles before construction. This batch
is under validation (AUD-1144).

`SageInterface::deleteAST` adds a forest overload taking
`std::initializer_list<SgNode *>` and the existing deletion mode. CHARACTER
factories use one deletion plan for duplicate LEN/KIND trees, retaining both
caller inputs if planning fails. Single-root calls retain their existing modes.
The JSON reader transfers selector ownership only after successful construction.
Focused native and memory controls pass; full acceptance remains open (AUD-1136).

`SgFunctionTypeTable::TypeIndex` uses `SignatureLess` for transparent lookup
with a borrowed `SignatureView`. Published keys remain owned `Signature`
values. Lookup validates typed argument positions without allocating temporary
keys or sets. Native retirement uses the stored key (AUD-1233).
Rebuild consumers of this public index type; no JSON
schema changes (AUD-1121).

`NameQualificationTraversal::nameQualificationDepthForType` and
`setNameQualificationOnType` now take the selected `SgType*` as their second
argument. Pass the actual source or semantic projection for that use; neither
helper derives it from the initialized-name's semantic type.

Schema 160 preserves C++ array initialization with `SgCxxArrayInitLoop`, its
owned `SgCxxArrayInitBinding` and borrowed `SgCxxArrayInitReference` nodes.
The common source runs once; the element initializer remains executable for
each element. Counts, value categories, types and lexical binding identities
are required in both readers. Complete copies remap bindings during
construction; detached fragments explicitly borrow their enclosing binding.
`replace_common_source` requires an explicit preserved category. Generic
expression replacement accepts only categories proved by existing typed nodes
or an exact lvalue; it cannot infer prvalue from a false lvalue flag.
Focused regular and required-fields memory checks pass; full acceptance remains
open (AUD-1090/1093).

Schema 160 also replaces `SgLambdaCapture::source_closure_variable` with the
required owned `capture_initializer` for every capture. The typed
`is_init_capture` flag selects written `=initializer` syntax; ordinary captures
retain their semantic copy/reference initialization without emitting it as an
init-capture. `buildLambdaCapture_nfi` takes the three expressions followed by
`capture_by_reference`, `implicit`, `pack_expansion` and `is_init_capture`.
Readers require the initializer and flag before construction. CFG traverses the
 initializer rather than the lexical captured name. These changes are unaccepted.

Schema 159 requires `SgConstructorInitializer::is_elidable` and
`elision_permitted` in both JSON readers. Eligible construction retains its
selected declaration and argument instead of being replaced by that argument.
The policy records Clang's `ElideConstructors`; CFG retains the call and permits
a bypass only when enabled. Source output preserves the exact delimiter form.
Source-file records also require the exact `standard` enum and `gnu_standard`
flag; both are validated before allocating the reconstructed file.
Complete implicit arguments and constructor preinitialization remain open.

Schema 158 requires `SgNewExp::allocation_implicit_argument_count` and
`array_bound_is_implicit` in both graph and embedded-expression JSON. The count
comes directly from LLVM 22 and includes size plus optional type-identity and
alignment arguments. Placement consumers use that exact count for formal
parameter alignment. The selected allocation declaration is separate from the
constructor declaration; readers reject missing fields and invalid counts
before construction. Older schemas are rejected by the existing version policy.

`checkTransformedFlagsVisitor` is removed: its visitor performed no checks.
Remove calls; use the existing `AstTests::runAllTests` for AST validation.

## Enclosing file ownership

`SageInterface::getEnclosingFileNode` follows the typed parent chain to the
nearest `SgFile`; a detached node still returns null. The unproduced legacy
`"sourcefile"` attribute override on class declarations is removed. Client
metadata must not redirect structural AST ownership. Construct the correct
parent edges when introducing declarations into a source file.

## Copy transaction identity maps

`SgCopyHelp::get_copiedNodeMap()` now returns a const map, and
`copiedNodeMapTypeIterator` is a const iterator. Replace direct map insertion
with `insertCopiedNodePair` (preserves an existing mapping), direct assignment
with `assignCopiedNodePair` (inserts or replaces), and erasure with
`eraseCopiedNodePair` (returns the number erased). `clearState` resets both
directions of the transaction. Standalone `copiedNodeMapType` output maps remain
ordinary mutable maps.

Use `get_originalNodesForCopy` to inspect all sources mapped to a target. A
canonical shared target may have multiple sources; consumers requiring an exact
one-to-one role must retain their existing rejection of ambiguous mappings.
All mutations update this reverse index immediately, including replacements
that leave the forward map's size unchanged.

## Compiler configuration

REX no longer installs its unused bundled GNU `shtool` copy. Install that
standalone utility separately if an external workflow needs it. REX's CMake
build, installation and retained scripts have no dependency on it.

Compiler identity and version come from CMake's selected, enabled compiler.
Independent `BACKEND_C_COMPILER`, `BACKEND_CXX_COMPILER`, and
`BACKEND_FORTRAN_COMPILER` values may no longer name a different executable while
inheriting another compiler's metadata. Select compilers with
`CMAKE_C_COMPILER`, `CMAKE_CXX_COMPILER`, and `CMAKE_Fortran_COMPILER`. Aliases
resolving to the selected executable remain valid.

The old compiler-header overlay is removed: configuration no longer rewrites
system headers, substitutes copied standard-library/intrinsic headers, generates
a legacy builtin-declaration preinclude, or installs those artifacts. The active
Clang builtin headers, OpenMP compatibility headers, selected LLVM 22 resource
directory, and their build/install lookup remain in place. Clang AST graph output
uses the primary frontend's compiler instance, including its exact include paths
and ordering. The unconsumed C_INCLUDE_STRING/CXX_INCLUDE_STRING configuration
macros and their CMake serialization are removed.

`Rose::Cmdline::makeSysIncludeList` is removed. It had no in-repository callers
and constructed the retired header-overlay arguments. Pass normal compiler
include options through the frontend interface instead. The unused
`fixupUsingNamespaceDeclarations` placeholder is also removed.

The public install lists now include `unparseFormatTypes.h` and the template
header `AstDOTGenerationImpl.C`, both required transitively by `rose.h`.

## Documentation and logging

The JavaCodeGenerator documentation placeholder and unused OFP logging categories
are removed because those frontends/backends are dropped. Parent-ownership
validation documentation now describes validation; these functions do not repair
published ASTs.

`SgProject::Fortran_ofp_jvm_options` and `Fortran_ofp_classpath`, including their
generated accessors, are removed. Neither field has an active consumer; Flang
uses the ordinary frontend arguments and module-search configuration.

## Frontend and AST interfaces

Schema 157 represents standalone DATA groups and declaration slash initialization
with owned typed values. `SgDataStatementValue` takes a constant expression and
optional repeat expression; repetition uses the existing folded integer metadata.
The obsolete value-format enum and initializer list are removed. Construct
`SgDataStatementObject(expressions)`, `SgDataStatementGroup(objects, values)` and
`SgFortranDataInitializer(values, exactDestinationType)` with complete children.
Attribute-statement builders accept the optional completed DATA groups. JSON
construction validates ownership and destination type before publishing these
nodes. This batch is building; runtime/full acceptance remains open.

Transformation tracking clears live node mappings at destruction while retaining
registered source snapshots and history IDs. Historical reporting uses
`getFileInfo(id)`; `getNode(id)` returns null for a deleted node. Pool reuse is
supported without retaining deleted nodes or reusing their tracking identities.

The schema-156 batch replaces `SgImplicitStatement::variables` and
`implicit_none` with an explicit implicit-spec enum and owned typed
`SgFortranImplicitRange` children. Ranges retain character endpoints, the
optional written endpoint, exact type/source form and source-visible symbol.
Construct the completed statement directly; do not encode or parse ranges as
initialized-name strings. Focused schema-156 construction, copy, JSON and
Memcheck contracts pass; complete acceptance gates remain open.

The unused `SageBuilder::buildFortranImplicitType` fallback API and obsolete
`SgDeclarationStatement::decl_attributes` field/accessors are removed.
Use Flang semantic types and the existing typed declaration modifiers.
Schema 156 drops the obsolete field and requires the typed range representation;
older schemas are rejected without compatibility reconstruction.

Source-syntax JSON type descriptors also require positive `source_type_identity`
values. Repeated IDs preserve the same type and selector ownership; contradictory
descriptors are rejected. Separate IDs retain separate source types even when
their values agree. Source-type copies receive new identities.

The unused `Unique.h`/`Rose::Unique::genUniqueID` API and empty `general.h`
are removed, along with unused `ROSE_GFORTRAN_PATH` and the never-defined
`ROSE_NUMERIC_VERSION` declaration. Active Clang staging-path constants remain.

The Clang boundary no longer disables Memcheck reporting or marks complete
objects, pointer arrays, strings, or API results defined. The internal
`clang-frontend-valgrind.cpp` publication session and its helpers are removed;
callers use Clang's typed public APIs directly. The existing source-location
contract keeps its assertions and no longer opens an annotation session.

`ResetTypes`, `resetTypesInAST`, and `fixupTypes.h` are removed. There are no
production callers. Frontends and AST deserialization must publish shared type
identity during construction. The existing detached-file regression now checks
that identity before and after postprocessing of the replacement graph.

Template parameter coordinates are no longer parsed from identifiers or printed
conversion names. Legal names such as `type_parameter_0_0` remain identifiers;
they must not resolve to a different parameter merely because of their spelling.
The new regression checks the conversion target's typed identity immediately
after construction and compiles/runs both original and emitted programs.

The subsequent frontend iteration removes template-name quality ranking and
synthetic-prefix normalization. Implicit auto parameters are classified through
Clang's declaration flags and have no invented template-header spelling.
Substituted parameter packs preserve typed declaration coordinates, canonical
source identity, and argument nodes, including when their identifier is empty.
Qualified types receive separate base identifiers, typed qualifiers, and typed
template arguments; printed type strings are no longer split to obtain them.

AST JSON schema 79 adds `SgNonrealRefExp::name_kind` and the owned
`conversion_type_id` edge for dependent conversion-function-ids. A conversion
name carries an `SgTypeExpression` constructed from Clang's exact TypeSourceInfo;
ordinary names have no such child. Copying preserves the type identity and
creates a distinct owned expression. Both ordinary and type-owned JSON readers
validate the name kind and child agreement. Older schema versions are rejected.

Schema 79 also requires `is_parameter_pack` and `is_pack_element` on every
serialized `SgInitializedName`, including external declaration parameters.
Readers restore these typed roles during construction and reject omitted or
non-boolean values. Regenerate older files from source; do not infer pack roles
from names or insert defaults into existing JSON.
Variable declarations also require their `is_constexpr` bit, which is independent
of the source type's explicit const qualification.
Resolved `SgMemberFunctionRefExp` and `SgTemplateMemberFunctionRefExp` nodes also
own a `conversion_type_id` when the source explicitly writes the conversion
name. Implicit conversion calls retain their resolved declaration identity and
do not invent a written type-id.
Conversion references and declarations emit their typed targets rather than
interpreting the function's display name.

`SgFile::unparse` keeps its public interface and delegates through `unparser.h`.
Its stale private declaration of an obsolete `unparseFile` overload is removed,
so the installed shared library no longer references that undefined symbol.

## Slicing and token metadata lifetime

Slicing cleanup now tests an entry's own edges before pruning it. Removing an
isolated formal-return node does not remove a connected entry or access the
freed return node. The unused `DependenceGraph::edgeNameArray` and protected
`sgNodeToDepNodeMap` are removed; use `getEdgeName(EdgeType)` and the typed
`getNode`/`getExistingNode` interfaces. The unbuilt, uninstalled `NEW_*` alternate
slicing implementations are removed.

A `MacroExpansion` remains owned while any statement association survives.
Statement removal and subtree deletion release it after the last reverse
association is erased. Consumers must not retain a raw expansion pointer after
removing its last associated statement.

WASM native LLVM validation evaluates `LLVMConfigVersion.cmake` in an isolated
CMake function and requires that file alongside `LLVMConfig.cmake`. Native and
WASM package versions must still match exactly.

## Clang AST graph generation

`clang_to_dot_main` is replaced by `writeClangAstGraph(CompilerInstance&, filename)`.
The graph consumes the completed AST from the primary frontend. Its duplicate
command-line parsing, compiler initialization, unused preprocessing recorder and
inserter, global output stream, and abandoned debug exit mode are removed.
The private `clang_graph.h` global-stream declaration is removed. Graph consumers
must provide an already parsed compiler instance without diagnostic errors.

`ClangToDotTranslator` and its installed `clang-to-dot-private.hpp` are removed.
Use the public graph function. LLVM 22's native typed traversal supplies graph
nodes and display labels; clients must not depend on the old node labels or IDs.
The graph retains type identities and specialization references without reparsing
display text. Valid null Clang expression types remain explicitly null.

Bison discovery now uses CMake's `FindBISON` module. `BISON_ROOT` still selects
an explicit installation prefix, and `BISON_EXECUTABLE` can select the executable
directly. `BISON_ROOT=no` is rejected during root configuration: the embedded
OpenMP parser already required Bison, so this setting never disabled it
successfully. CMake's standard `BISON_VERSION_STRING` replaces the unconsumed
custom `BISON_VERSION` value.

Changing `BISON_ROOT` or `ZLIB_ROOT` revalidates the requested installation even
in an existing build. Missing prefixes and incomplete Zlib header/library pairs
are errors. Native Zlib consumers use `ZLIB::ZLIB`; WASM does not discover the
native PDF dependency. The copied `FindZlib` macro interface is removed.

The private configuration header no longer publishes unused platform probes,
obsolete dependency switches, or autotools replacement-keyword scaffolding.
The active `HAVE_DLADDR`, `HAVE_PTHREAD_H`, and `HAVE_SYS_TIME_H` checks and public
`ROSE_SIZEOF_INT`/`ROSE_SIZEOF_LONG` values remain. External projects needing
other feature checks should perform them in their own configuration.

## Filesystem utilities

`Rose::FileSystem::baseNameMatches` owns its regular expression; rebuild clients
against its changed class layout. `readFile` and `writeFile` throw
`std::ios_base::failure` on I/O failure. `copyFile` uses the standard filesystem
copy operation and reports failures as `std::filesystem::filesystem_error`.
Empty files copy successfully, failed source lookup preserves an existing
destination, and recursive copies retain all selected regular files.
`makeNormal` follows `path::lexically_normal`, including leading parent paths.
The unconsumed `tempNamePattern` variable is removed; temporary-directory names
remain unique with the documented `rose-` prefix.

The unused `Rose::ModuleBuilder` compool API and its factory are removed.
Fortran continues to use the active Flang module loader and semantic import
publication. Include paths preserve literal quotes and backslashes.

## Type-copy reverse caches

A copied `SgType` begins with empty pointer, reference, rvalue-reference,
modifier, decltype, and typeof reverse caches. This holds for generated AST
copy functions and C++ copy constructors. Use the typed wrapper factories for
the new identity; source wrappers remain attached to their original base type.
Type graph materialization preserves caches populated during the copy
transaction. Clearing a published cache can create two canonical wrappers for
one base and is no longer part of copy processing.


## Final-review driver, node, and tool cleanup

The in-process Fortran frontend no longer launches the demo driver's compiler,
consults or changes F18_FC, or copies input into synthetic source filenames. The
existing REX backend still compiles and links the emitted program. Source file
selection and language form come from the project's selected input. Trailing
preprocessor tokens follow Flang's original diagnostics and semantics.

SgSuperExp, SgClassExp, and SgUnknownArrayOrFunctionReference are removed, including
the super/class expression builders. They have no supported frontend/emission
path. Fortran array and procedure references must be resolved as typed nodes at
construction. Their historical variant IDs remain reserved; other nodes retain
their IDs.

Rose::rose now advertises the C++17 minimum already required by public headers.
Consumers should link the imported CMake target to inherit that requirement.
The featureVector --debug option had no effect and is removed. Textual inventory
output remains the default; --verbose also prints it. Output I/O failures now
fail the command.

## Imported module checkpoint ownership

AST JSON schema 80 serializes the exact frontend-owned imported module graph,
including nested file lists, component declarations, symbol tables, and types.
File-specific command arguments, ownership paths, and frontend flags move from
the root metadata to each source node's properties. Empty or absent command
arguments are rejected. Consumers must regenerate earlier checkpoints.

## Project-wide checkpoint replacement (schema 81)

A source-file checkpoint can affect declarations shared with other input files.
Its snapshot now records the complete project input graph and the exact
`project_files_id`. Every reconstructed input is published in one transaction;
the project object and its file-list storage remain stable, but clients must
reacquire source-file and descendant pointers afterwards. Standalone export
records use zero for `project_files_id`. Flang module registries follow the
explicit old-to-new node map. This change is under validation for AUD-061.

## Process execution and allocation diagnostics

`systemFromVector` now returns the child's exit code, or 128 plus its terminating
signal. Callers must not decode the result as a packed POSIX wait status. Missing
executables return 127. Invalid argv and process-management failures abort.
Unreferenced `popenReadFromVector`/`pcloseFromVector` and the unused global
Stratego `mutex` class are removed; they had no active repository consumers.

`ROSE_ALLOC_MEMSET` accepts 0, 2, and 3. Mode 1 never implemented its promised
command-line behavior and is now rejected. Diagnostic fills preserve the
configured pointer-word prefix and cover every remaining object byte.
Mixed Fortran/C++ backend links use the configured Fortran driver plus CMake's
C++ runtime metadata; compiler identity still comes from CMake discovery.

## Import roles and schema 82

`SgImportStatement::fortran_import_kind` preserves Default, Only, None, and All
as an enum. Source construction validates the role and its name list before
publication. JSON schema 82 records the role and all C++ module/header-unit and
Fortran import fields; regenerate older checkpoints. Copies retain the role.
Checkpoint semantic function types use the normal canonical factories, so later
transformations receive the same identity. Declaration-owned function syntax
remains independently owned.

`FileHelper::normalizePath` still permits nonexistent output paths, but now
propagates filesystem errors instead of substituting a lexical path. Optional
JSON values require an object with a boolean `present` field; malformed scalar,
array, or null values are no longer interpreted as absent records.

`SgNode` now carries its AST JSON ID directly. New and copied nodes start with
zero, and setting that bookkeeping ID does not mark a source transformation.
This changes the generated node ABI; rebuild tools against matching headers.

## Type metadata and copy ownership

Embedded array and pointer-member checkpoint identities now use a typed
`SgType::ast_json_type_identity` field. New and copied types start at zero;
bookkeeping setters do not mark source transformations. C++ type copies now
clone their owned attribute container, matching generated AST copying.
Deserialization caches own JSON comparison keys for the full transaction,
including keys supplied through temporary records. These changes are under
validation for AUD-074 through AUD-076.

External AST JSON declaration provenance now lives in typed
`SgDeclarationStatement` fields. `get_ast_json_external_role()` distinguishes
local, external function, external module, and external class records;
`get_ast_json_external_source_file()` provides the exact source identity.
Initialize a detached declaration once through
`initialize_ast_json_external_provenance()`. Invalid roles, empty/NUL paths, and
repeated initialization are invariant violations. Copies retain provenance.
The four former `rex_ast_json_external_*` attribute keys no longer have reserved
semantic meaning. This changes generated node ABI but preserves the existing
external-record JSON encoding.

`FileHelper::pickMoreGeneralPath` is removed after checking its installed inline
API and finding no repository consumers. Active include-path helpers now treat
include spellings lexically, preserving leading parent components and literal
directory names ending in dots. Filesystem canonicalization remains a separate,
checked operation. The unexported and unused mlog `getDateTime` helper is removed.

Direct C++ type copies preserve coarray state and independently own KIND syntax.
Generated destructors now honor `DEF_DELETE` for ordinary owned C++ objects as
well as pooled AST nodes; attribute containers are released by direct deletion,
including when no project teardown follows.

The unused installed `LoopTreeLoopNode::GetOrigLoop()` accessor and its `orig`
field are removed. Loop reconstruction and cloning continue through `LoopInfo`;
there are no repository consumers of the removed accessor. Rebuild clients of
this installed class after its layout change.

Function type interning now uses typed semantic signatures. `SgFunctionTypeTable`
provides `size()`, `clear()`, read-only `get_types()`, and exact typed insertion,
lookup, and removal. Its former mutable `SgSymbolTable` accessor and name-based
entry APIs are removed, as is the unused `SgFunctionType` type-symbol lookup.
Canonical function types are shared AST nodes, not named symbols. Cache removal
does not delete a type still referenced by an AST; mutation requires removing
the old cache entry before changing its signature. This changes the generated
class layout and requires rebuilding consumers. Existing size/reuse tests keep
their expectations while using the typed table API.

Function-construction tutorials now use source lexical ownership at the intended
insertion point. Semantic auxiliary ownership is reserved for declarations that
have semantic provenance; transformation source nodes must not be reclassified
as semantic nodes after construction.

Canonical class and enum types are obtained from their first nondefining
declaration or the respective `SgClassType::createType`/`SgEnumType::createType`
factory. Creation no longer duplicates that identity in the lexical scope's
string-keyed type table. Name mangling is a consumer of a completed declaration
graph and is not required to reserve a canonical tag
identity during frontend or JSON construction. Declaration-chain validation
remains mandatory.

Block flattening retains blocks containing lexical declarations or semantic
auxiliary owners, preserving their scope and lifetime boundaries. Other blocks
can flatten without rebuilding symbols by name. Explicit scope moves still
reject name conflicts before changing ownership. The undefined
`rebindVariableAndLabelReferences` and `fixReturnStatements` declarations are
removed from the installed inliner support header; there are no definitions or
callers to migrate.

AST JSON schema 83 separates Fortran source NAME presence
(`binding_label_present`) from the semantic external procedure identity
(`fortran_binding_name`). NAME="" remains present and has no external identity;
consumers must use the semantic identity, not derive one from source text.
Schema 84 additionally serializes SgSourceFile's target ABI layouts and
SgInitializedName's typed BIND(C) parameter passing convention. Generated source
files may have explicitly unavailable target ABI metadata; a consumer requiring
an ABI must reject that state. Previously serialized ASTs must be regenerated.

AstRestructure and its installed header have been removed. Its string replacement operation only aborted after the rewrite subsystem was dropped. Use complete typed SageBuilder nodes and SageInterface insertion/replacement operations for AST transformations.

AST nodes now expose appendDestructorOwnedNodes, generated from their DEF_DELETE
fields and inherited ownership. Subtree deletion uses these exact edges for the
objects released by destructors; parent pointers alone never establish ownership.
Rebuild all client executables after regenerating the AST: the added virtual API
changes its C++ ABI. Repeated transformation-position setup replaces and retires
its owned locations and publishes complete ranges, including global scopes.

AST JSON schema 85 adds SgFormTeamStatement. The frontend constructs FORM TEAM
with required team-number/team-variable expressions and optional NEW_INDEX, STAT,
and ERRMSG expressions. Its image-control kind is e_form_team. Copying preserves
independent owned operands; readers reject absent required operands and invalid
image-control roles. No attribute string or post-construction repair supplies
these semantics.

`Outliner::saved_source_file_for_dynamic_library` is removed. Use the returned
`Outliner::Result::file_` or the declaration's enclosing source file.
`Result` also removes unused `target_class_member` and `new_function_prototype`
members; deferred header work remains in `deferredTransformation`. Rebuild
outlining clients for the changed C++ layout. Default results initialize all
pointer fields, and copies preserve the exact file identity.

`Outliner::generateFunction` now requires an initially empty
`OutlinedFunctionParameterBindings` output map. It returns exact captured source
symbols and their defining formal parameters; transformations must use these
identities instead of matching generated parameter names.

`legacy::PRE::partialRedundancyEliminationOne` now takes a
`std::vector<SgExpression *> &retired_expressions` ownership collection. The
caller must delete these detached roots after all worklist/equality scans that
can reference them have completed. Function-level PRE manages that transaction.

Outliner::collectVars now accepts (const SgNode *root, const SgStatement
*outline_site, VarSymSet_t &). Ordinary statement callers pass the statement in
both positions. Type-owned expressions supply their actual outline site without
reparenting the expression. ASTtools::collectRefdVarSyms and collectDefdVarSyms
accept SgNode roots accordingly; consumers must rebuild for the changed symbols.

FunctionDeclaration.parameterList_syntax is an owned source edge. Generated copy
construction clones it using the existing identity map before copy validation;
the syntax list may alias the primary list within the same declaration. Detached
subtree deletion includes a distinct syntax list's parameters and default
arguments. The serialized field is unchanged.

Clang constructs adjusted lookup parameters before symbol publication. When the
written type differs, its syntax parameter owns the source qualifiers and
prototype; consumers inspecting spelling must use parameterList_syntax. The
primary list supplies the written view when no distinct syntax list is needed.
No IR layout or serialized-field change is introduced by this construction fix.

Source files own their token-subsequence maps from every constructor, together
with their token nodes and independent preprocessing/token-stream containers.
The source-file registry is a nonowning index; the unused filename-keyed token
map registry is removed. get_tokenSubsequenceMap never allocates. Explicit
replacement transfers a complete map to a cleared owner; call
clear_tokenSubsequenceMap before starting a new mapping construction.
SgSourceFile's layout and generated ownership API require a complete rebuild.

Published token maps may take a typed NodeAssociation containing the exact
owner, associated nodes and construction flags. Copy and JSON restoration
provide it before publication rather than appending to an already published
single-owner mapping. The JSON field format is unchanged; duplicate/missing
owner associations are invalid and are rejected at restoration.

Copy construction preserves the source edit-state flags and uses an explicit
SgCopyHelp::ConstructionScope during internal identity rebinding. Automatic
mutator notifications still advance the structural cache sequence. Calls on
original/shared nodes and changes after the copy transaction remain ordinary
modifications. There is no subsequent flag-reset traversal.


Scalar literal value setters now discard the previous spelling and select
`e_literal_canonical_generated`. This applies to all numeric and character
scalar kinds, including both wide-character setter APIs. Constructors still
accept an exact source spelling together with its value. Clients changing a
literal value no longer need to clear its old spelling separately.

AST JSON schema 86 requires a boolean `is_modified` in every node's flags,
including inline expressions owned by types. Regenerate older checkpoints;
missing or incorrectly typed flags are rejected before node construction.
Reconstruction keeps `SgNode::SourceStateConstruction` active for newly allocated
nodes through graph linking and validation, preserving recorded source edit
state. Existing nodes remain tracked, structural cache invalidation continues, and edits after
construction remain ordinary source modifications. Rebuild consumers for the
changed generated AST implementation.

The `astCopyReplTest` example now copies and replaces the owning `SgProject`,
including every input file and its metadata. It compares emitted bytes after
copying and again after destroying the original project. The former operation
that interleaved declarations between unrelated global symbol tables is removed.
Temporary outputs use filesystem and stream APIs and are retained on failure.


AST JSON schema 87 replaces float/double's spelling-only `value` with mandatory
`value_bits` and `value_string`. `value_bits` is an array of unsigned 32-bit
numeric words in least-significant-first order: one for IEEE binary32 and two
for binary64. It represents the typed AST value independently of source text
and host byte order, including signed zero and NaN payloads. `value_string`
retains the exact optional spelling. Older checkpoints must be regenerated;
readers never parse a source literal to recover a missing numeric value.


AST JSON schema 88 removes `SgComplexVal.value_string`, and the generated AST
and builder APIs no longer accept a composite spelling. Pass exact optional
source spelling to the scalar operand constructors. Complex expressions always
emit their typed operands; editing a component cannot leave a stale composite
string. Clang's imaginary-literal producer separates the validated imaginary
suffix before scalar construction. Regenerate checkpoints and rebuild consumers
for this AST layout and API change.


Macro invocation spelling remains valid only while its typed expansion is
unchanged. Editing an owned expression now emits that expansion with explicit
parentheses. Copy and JSON preservation use the existing typed edit-state flags;
no reparsing or post-construction repair supplies the edited semantics.


CMake now requires version 3.19, matching the existing `file(REAL_PATH)` calls
and `CheckLinkerFlag` capability checks. `REX_LINKER` selects a compiler-driver
policy, tested with all enabled languages. `auto` tries lld, mold and bfd in
that order, then uses the configured toolchain. `system` adds no project linker
selection option. Explicit unsupported selections fail configuration, including
non-system linker requests under Emscripten.

Linker options belong to the build directory and no longer overwrite
`CMAKE_EXE_LINKER_FLAGS`, `CMAKE_SHARED_LINKER_FLAGS` or
`CMAKE_MODULE_LINKER_FLAGS`. Reconfiguration therefore removes the previous
project choice while preserving caller-owned flags. `CMAKE_LINKER` retains
CMake's native toolchain semantics; REX no longer guesses driver flags from its
filename. Old caches marked `_ROSE_CONFIGURED_ONCE` require a fresh build tree
and the caller's intended flags because the old code irreversibly rewrote those
cache values. REX rejects those caches instead of guessing their original values.

## Exact extended floating values (schema 89)

`SgLongDoubleVal`, `SgFloat80Val` and `SgFloat128Val` now store
`Rose::FloatingValue`, consisting of a typed binary format and exact value bits.
Their value accessors and corresponding builders use that type. Constructing a
value from a native `long double` preserves the native representation; frontend
translation imports the Clang value bits and target format directly. There is
no implicit conversion back to a host floating scalar. Numeric comparison and
arithmetic use the represented formats. Literal setters retain the literal's
established target format and invalidate the old source spelling.

`SgFloat80Val` and `SgFloat128Val` constructors convert inputs to their fixed
format. `SgLongDoubleVal` retains the supplied target format, which can differ
from host `long double`. JSON schema 89 requires that format and four numerical
32-bit words independently of spelling; older schemas are rejected explicitly.

## Macro expansion edit state (schema 90)

`SgMacroExpansionExp` retains its original invocation until its bound semantic
expansion changes. Construction-time `isModified` and transformation-position
flags are not edits to that binding. The node records its construction sequence;
actual setter mutations in the expansion carry a later node sequence. Copies
preserve both. JSON requires the boolean `expansion_is_modified` and establishes
a fresh local construction baseline when restoring the typed expansion.
Replacing the expansion explicitly invalidates the original invocation surface.
Missing or malformed edit-state properties are rejected before node allocation.

Source-position, macro-range provenance and literal-spelling-role setters still
mark source state and invalidate structural caches. They do not advance the
semantic mutation sequence used by macro expansion tracking. Changing a literal
value or replacing an operand continues to advance that sequence. Relocating an
unchanged subtree therefore preserves its valid macro invocation.

## User-defined literal lexical tokens (schema 91)

SgFunctionCallExp::source_user_defined_literal_operands now owns an
SgCxxLiteralTokenList. Its get_tokens() entries are SgCxxLiteralToken nodes with
an explicit numeric/character/string token category and exact source spelling.
They are located syntax nodes, not expressions or fabricated literal values.
The existing per-token suffix roles and exact common suffix retain concatenated
string spelling. Read semantic values and types from the call's actual argument
and template argument graph. Do not call get_type() or value accessors on tokens.
Copies and JSON retain exclusive token ownership; token JSON requires token_kind
and a nonempty spelling. Remove uses of SgUnparse_Info::user_defined_literal:
literal token emission no longer changes numeric suffixes in the backend.

## User-defined literal semantic edits (schema 92)

UDL lexical ownership records the semantic AST revision at construction.
get_source_user_defined_literal_is_modified() reports later edits to the call,
its arguments/reference, or its exact template argument graph. Source-position
and source-form setters retain cache invalidation without marking semantic
values changed. The JSON UDL call property source_user_defined_literal_is_modified
is required and boolean; it survives copying/restoration independently of the
new construction's revision. Edited calls emit their actual operator function
and arguments, including a template specialization's complete typed argument
list, so old source tokens cannot override transformed semantics.

## Decoded string values (schema 93)

SgStringVal::value is now SgStringCodeUnitList, a sequence of uint32_t target
code units excluding the implicit terminator. CFE takes these directly from
Clang. get_value()/set_value() operate on that typed sequence; get_byte_value()
explicitly converts units in 0..255 and rejects wider values. Byte-oriented
builders accept decoded bytes and use code_units_from_bytes(). No API decodes
escaped source text into semantic values. Emission escapes the stored units;
changing a value invalidates retained raw-token spelling and updates its literal
array/CHARACTER length when needed.

JSON requires code_units as an integer array, including for inline expressions,
and rejects missing, fractional, negative and out-of-range entries. The old
escaped value string is removed. Source flags use one shared writer/reader for
ordinary graph nodes and inline lexical/expression nodes.

## CHARACTER type construction

The zero-argument `SageBuilder::buildStringType()` declaration and implementation
are removed. The implementation has always assigned null and asserted, so it never
constructed a usable type. Pass the typed Fortran length expression to
`SageBuilder::buildStringType(SgExpression *)`, for example an integer-value node
for an explicit constant length. No implicit length or placeholder type is inferred.

## Exact 128-bit integer values (schema 94)

Clang 128-bit integral template values and instantiated literals now use
`SgInteger128Val`. Its `Rose::Integer128Value` stores two least-significant-first
64-bit words and signedness, without relying on host `__int128` support or source
spelling. Consumers must handle this node when inspecting integer values. Its
value setter preserves signedness and marks semantic edits; copied values own
independent storage. JSON schema 94 requires four numeric 32-bit words and a
boolean signedness field. Older snapshots are rejected and must be regenerated.
Emission constructs an exact target constant expression because C/C++ has no
128-bit literal token suffix.

## Atomic value types (schema 95)

Clang atomic types now produce an interned `SgModifierType` whose
`SgTypeModifier::isAtomic()` is true, with the exact value type as its base.
Atomic values, atomic pointers and pointers to atomic values retain distinct
semantic identities. Use the typed modifier when inspecting atomic state;
stripping every modifier intentionally removes this information.

Schema 95 required `modifier_atomic` on every modifier-type record. Missing or
non-Boolean values fail before allocation. Copying preserves the modifier and
source emission uses `_Atomic(type-name)`, including complete pointer type names.

## Complete modifier checkpoints (schema 96)

Modifier-type records now use the complete typed modifier payload:
`type_modifier_vector`, `type_const_volatile_modifier`,
`type_elaborated_modifier`, `type_gnu_attribute_alignment`,
`type_gnu_attribute_sentinel`, `type_address_space_value` and `type_vector_size`.
The four partial `modifier_*` Boolean fields from schema 95 are replaced by
the exact flag vector and qualifier enum. Declaration modifiers retain their
existing field names and share the same codec. Every field is required; invalid
vector lengths, enum values and numeric ranges are rejected before type
allocation. Regenerate earlier JSON checkpoints.

The inaccessible `SgTypeModifier::gnu_extension_machine_mode_enum` placeholder
and its permanently unspecified private field are removed. This does not remove
Clang's resolved semantic types for machine-mode source attributes.

## Block pointer rejection

Clang block pointers have no lossless Sage representation. Their declarations
now fail at frontend type construction, including external callbacks with no
block-expression initializer. They previously became ordinary function pointers
with a different ABI. Block expressions remain unsupported; source text is not
used to conceal the missing typed representation.

Unevaluated GNU/Clang vector dimensions are also rejected at type construction.
The previous numeric modifier silently replaced a dependent dimension with one
element. Concrete vector types remain supported, including copying, checkpoint
round trips and runtime operations. Supporting dependent dimensions requires a
typed expression representation; retaining their spelling or supplying a numeric
default is not a valid substitute.

## Typedef identity and lifetime

`SgTypedefDeclaration::get_type()` is the canonical identity for that exact
source declaration. Typedef types are no longer indexed by a mangled name in
the enclosing type table or registered in a reverse list on their base type.
Use the declaration's typed edge and ordinary declaration/symbol lookup.

The unused `SgTypedefSeq` node and `SgType` typedef-list field/accessors are
removed. Regenerate ROSETTA outputs and rebuild all clients because the
in-memory AST layout changes. AST JSON never serialized that reverse cache,
so this removal does not add a JSON compatibility representation.

`SageInterface::deleteAST` retires exact declaration-owned typedef types and
symbols in hidden project-owned tables. Borrowed semantic types and genuinely
external references remain subject to the existing isolation proof.

## Typed OpenMP runtime source context (schema 97)

`SgOmpExecStatement` now owns the runtime source context needed by lowering and
outlining. Use `initialize_runtime_source_context` with the function spelling,
physical file ID, line and typed origin. Ordinary AST copying retains this
provenance and marks the copied context. The internal
`rex:omp:kmpc-source-context` attribute and its class are removed.

JSON records require `runtime_source_origin`, `runtime_source_function_name`,
`runtime_source_file`, `runtime_source_line` and `runtime_source_is_copied`.
The filename is serialized instead of a process-local file ID. An unavailable
context requires an empty payload; available contexts require complete source
information. Regenerate older checkpoints and rebuild generated AST clients.

The installed Clang translator header no longer declares the unused generic
`VisitTemplateDecl` and `VisitRedeclarableTemplateDecl` entry points. REX's actual
Clang declaration dispatcher calls the concrete class/function/alias/variable,
concept and builtin-template visitors directly. The removed generic path could
manufacture a second template declaration and substitute global scope. Consumers
of the translator header must rebuild; custom integrations should use `Traverse`
for typed declaration dispatch or the corresponding concrete visitor.

## WASM source filename contract (AUD-285)

The WASM API rejects empty or unsupported source filenames and embedded NULs
before parsing, with `ok: false`, `exitCode: 2`, no output files, and an
`unsupported source filename` diagnostic. Callers must provide a supported C or
C++ suffix. `.c++` is preserved and parsed as C++; supported filenames containing
spaces or leading dashes remain usable. No default `.c` extension is appended.

## Member-address semantic types (AUD-286)

`SageInterface::isAddressOfCurrentObjectDataMemberReference` now reads the exact
address expression's result type. Transformations constructing member-address
expressions must provide the correct ordinary-pointer or pointer-to-member type.
Enclosing-class membership does not determine that distinction. Anonymous
template parameters retain an empty source name; internal symbol identities are
not emitted as template arguments.

## Typed address operand form (schema 98, AUD-288)

`SgAddressOfOp` records `address_operand_form`, distinguishing an object operand
from an unparenthesized qualified-id. Result types can remain dependent while
that distinction is already known. Clang supplies the form during construction;
copying and JSON preserve it. JSON requires a valid form and rejects absent,
unclassified or out-of-domain values. Schema 98 does not infer this information
from older checkpoints.

All address builders require an exact
`SgAddressOfOp::address_operand_form_enum` (AUD-2163). Generated constructors
require both unary mode and operand form; the two-argument builders and custom
three-argument constructor are removed. Rebuild native and installed clients.
Transformations replacing an address expression transfer its typed form with
its type and symbol; those constructing object addresses supply the object form.
The pragma lexer/parser carries qualification as a typed semantic value and
parentheses remove qualified-id status before address construction. A result
type alone cannot distinguish `&N::object` from an ordinary object address.
Node layout and the serialized schema are unchanged.

## Template-parameter builder ownership (AUD-289)

For type and template-template parameters, `buildTemplateParameter` requires an
unbound `SgTemplateType` and sets its exact back-reference and matching pack
state before returning. A different declaration surface needs its own type;
reusing an already-bound parameter type is rejected. Callers no longer supply
a missing binding after construction.

Template-template parameters also require the exact `SgTemplateDeclaration *`
as the final builder argument (AUD-877). Type and non-type parameters require
that argument to be null. Both overloads validate the role before constructing
the parameter; setting a missing declaration after the builder returns is no
longer supported. Clones pass their resolved declaration to the constructor.

Defining function-instantiation builders retain the canonical declaration's
typed primary or dependent candidate set before publication (AUD-882). The
canonical input must already have matching primary/specialized edges; a free
function requires one primary or at least two dependent candidates. Frontend
callers supply the exact member pattern before instantiation construction.

`buildNondefiningClassDeclaration_nfi` now requires the selected
`SgTemplateClassDeclaration *` immediately after the primary template argument
(AUD-885). Instantiations require both; ordinary classes pass null for both.
The primary is canonical identity, while the selected pointer preserves Clang's
exact primary or partial declaration. Partial defining builders require the
canonical input's primary link to be complete before construction (AUD-886).

## Shared object-address result builder (AUD-292/293)

Use `SageBuilder::buildAddressOfResultType(SgType *)` instead of the removed
`ASTtools::buildAddressOfResultType` declaration. The implementation is shared
with AstInterface's object-address producer and retains the exact pointee surface
while resolving reference aliases. An optional `SgType::get_ptr_to()` cache is
not a constructed result type. Qualified pointer-to-member expressions continue
to require their exact pointer-member result type and operand form.

AstInterface unary construction now uses SageBuilder to establish complete
transformation positions and operand ownership before returning. Copying these
expressions preserves exact result types and prefix/postfix modes.

## Single typed typeof operand (schema 99, AUD-295)

AST JSON now represents `SgTypeOfType` with exactly one of `base_expression`
or `base_type`. Regenerate schema-98 files; duplicate or missing operands are
errors. Every expression, including `SgFunctionParameterRefExp`, supplies its
exact result type. `SageBuilder::buildTypeOfType` requires exactly one non-null
operand and gives each expression occurrence its own lexical type owner.

## Process-wide type roots and collected typeof operands (AUD-297–301)

The global type and function-type interning tables are process roots with null
parents. Their lifetime and canonical type identities span files and projects;
clients must not attach them to a project or infer lexical ownership from them.
A collected typeof operand has one exact lexical owner. Repeated JSON references
to that operand reuse its owner; separately owned equal operands remain distinct.
Statement-expression reconstruction requires its complete block, semantic scope
and result type before construction.

## typeof copying and source context (AUD-303–305)

Deep copying creates an independent lexical typeof occurrence, including its
owned expression graph. Source-file lookup uses the statement-expression block's
explicit semantic scope across a type-owned operand boundary. Both source-file
queries retain hard owner/scope/cycle checks; no filename or attribute override
supplies missing context. Inserting a function also preserves the null parent of
the process-wide function-type table.

## typeof factory ownership and retired reverse cache (AUD-307–310)

Both `SgTypeOfType::createType` overloads now construct lexical occurrences through
`SageBuilder::buildTypeOfType`. Expression operands must be unowned and have exact
types; a type operand does not populate a cache for later expression operands.
Malformed base identities are rejected instead of receiving an invented name.

The unused `SgType::typeof_ref_to` member and generated `get_typeof_ref_to` /
`set_typeof_ref_to` accessors are removed. Rebuild REX and all clients against the
new AST layout. Use the factory or builder to create an occurrence; a reverse
cache is not a lexical typeof operand. Unique names for function-type tables now
use the table's own identity, independent of files and scopes.

## Exact decltype construction and lexical copying (AUD-311–314)

`SgDeclType::createType` now requires both the unowned operand expression and
its exact semantic result type, as `SageBuilder::buildDeclType` already does.
The frontend must supply reference semantics: an expression's own type cannot
distinguish `decltype(value)` from `decltype((value))`. The factory delegates to
the checked builder, and both reject an expression already owned by another node.
JSON references preserve the exact existing owner only when operand identity,
semantic result and GNU dialect agree.

Deep copying creates independent decltype/typeof syntax even when no named type
occurs in the project. Original type/operand identities remain unchanged.
The unused `SgType::decltype_ref_to` member and its generated getter/setter are
removed. Rebuild all clients against the new layout and pass the explicit semantic
result to the lexical factory; no reverse cache replaces that construction.


## Fortran module registry and derived-type identities (AUD-315–318)

`FlangModuleInfo` now represents one noncopyable project-parse session. The project
parser owns its lifetime; callers must not retain or set a process-wide current
project. Module declarations remain owned by the AST; registry entries borrow them
only while parsing. Replace explicit `setCurrentProject`/`set_inputDirs` calls with
the scoped session when implementing a project parser.

Fortran derived-type mangling encodes the canonical declaration's process-local
identity. Defining and nondefining declarations in one family share that identity;
distinct types and independent project copies do not. Do not persist these names
as cross-process identifiers. AST JSON uses its explicit identity graph.


## Semantic identity and structural ownership (AUD-319–324)

Derived-type identity is independent of source-name visibility. Imported procedure
results and arguments retain their producer's typed signature positions, including
nested procedure interfaces. ASSOCIATE declarations and selectors are now normal
owned traversal children; visitors and copies include them before the body. JSON
requires their typed body and nonempty declaration list.

Non-type template arguments retain separate declared-parameter and expression
payload types. Copy each from its corresponding original edge; reference and
nullptr conversions do not make those two roles identical.


## Exact module producers and traversal lifetime (AUD-326–330)

Generic re-export keys identify overload sets. Resolve a specific through its exact
semantic producer module and declaration scope before constructing dependent typed
signatures. A visible derived-type alias must also publish the canonical producer
binding used by dependency checks. External JSON function declarations receive
complete decoded semantic types directly in their constructors; private graph-node
construction ordering does not apply to this separate reader.

Frontier and range-for classification retain checked structural facts only during
their traversal. They do not add AST attributes, change structural edges, relax
positive ownership checks or increase test deadlines. Reused frontier visitors clear
their indices at both traversal boundaries.


## Function-reference constructor API (AUD-331–332)

SgFunctionRefExp and SgMemberFunctionRefExp no longer store or accept a separate
function-type pointer. Their semantic type is obtained through the exact bound
symbol. Use SgFunctionRefExp(symbol), or its source-position overload, and
SgMemberFunctionRefExp(symbol, virtual_call, need_qualifier), or its source-position
overload. SageBuilder remains the normal construction interface. Rebuild every
consumer and generated AST object; the old constructor signatures and class layouts
are not binary compatible. AST JSON still serializes the computed semantic type.


## Unsupported packaging removal (AUD-333)

Remove the unreachable non-Unix CPack/bundle integration. Supported native and
WebAssembly builds never entered it. Standard CMake installation remains the
supported install path. The audit-added mock packaging test and its now-unused
ldd tracing boundary are retired; no inherited regular test is removed.


## Traversal diagnostics and enclosing-node queries (AUD-338–339)

AstTests::validateTraversalSuccessors checks actual counts and successor identities.
The removed isProblematic, numSuccContainers, numSingleSuccs, isPrefix and numPrefix
APIs classified AST structure through generated display names. Mixed scalar and
dynamic-container traversal is supported; static traversal enums identify only the
fixed required prefix. Diagnostic callers must use the actual successor API.

getEnclosingNode preserves existing defining-declaration redirects and its nearest
matching-node behavior. Cycles in the visited ancestry are hard errors; valid
ancestry has no arbitrary depth limit. Queries use constant auxiliary storage.


## Retired diagnostic demonstrations (AUD-341)

The unused Dummy*TestQuery classes, DI/DS demonstration attributes and dormant
TestAstNullPointers visitor are removed from AstConsistencyTests.h. Use the
standard AstProcessing and AstReverseProcessing traversal interfaces directly;
rex_ast_traversal_attributes now exercises their contracts in CTest. Include
AstReverseProcessing.h explicitly when using its templates.

## Explicit parsed and generated source construction

`SgSymbolTable` no longer exposes `FunctionSymbolExactKey`,
`FunctionSymbolExactIndex`, or `get_functionSymbolExactIndex()`. Typed function
lookup uses the name table directly. Replace `clear_functionSymbolExactIndex()`
with `clear_symbol_indexes()` when clearing auxiliary indexes during table
reconstruction. Declaration-basis replacements still require
`reindex_symbol_basis()`; changing a function type needs no signature-cache repair.

`SageBuilder::buildFile` and `buildSourceFile` require an existing readable input.
They no longer create a dummy C++ input after an open failure. Call
`buildGeneratedSourceFile(outputFileName, project)` for a new output-only
translation unit; it constructs an empty typed global scope without filesystem
writes or frontend invocation. This distinction also applies when the project
argument is omitted. Parsed-input callers retain `buildFile`; generation callers
in the tutorial and builder examples now use the explicit generated-source API.

AST JSON schema 102 requires conditional-expression operand records in type-owned
expressions, including an explicitly absent true operand for GNU binary conditionals.
Regenerate older checkpoints from source. The reader rejects older versions and
missing fields; it does not synthesize operands or reconstruct a placeholder tree.

The argv-taking `SgSourceFile` constructor requires an explicit project. File
setup also requires its physical `SgFileList` owner; it never temporarily parents
the file to the project. Checkpoint readers construct lists before files and use
`createSourceFileFromRecord(record, project, owner)` rather than the generic
node factory. The delegating `SgSourceFile::doSetupForConstructor` override is
removed. Single-file checkpoints explicitly borrow the existing physical list;
complete-project checkpoints construct their recorded list.

Requires-expression builders consume the local parameter scope's exact pending
construction transaction before returning. Callers no longer complete it after
receiving the expression. JSON reconstruction uses complete typed requirements
and local parameters before publishing their owning requires expression;
declarations with derived scope validate their owner instead of assigning a
scope field. No schema change is needed.

AST JSON schema 149 removes `SgNonrealDecl.semantic_name`. Nonreal template
identity consists of its base name, typed arguments, and exact template target.
`buildSemanticNonrealType` and the Clang qualifier/reference helpers no longer
accept a duplicate string identity. Template-id builders reject a missing typed
target before publishing a declaration. The malformed-template death contract
now rejects this structural defect instead of inspecting angle brackets in a
removed string input. String-shape assertions now check typed edges and argument
ownership. Outliner producers pass their exact primary at construction.

Graph JSON now schedules nonreal template targets as construction dependencies.
Named-type dependencies include `SgNonrealType`; its declaration receives the
exact target and available physical owner before publication. The old late
target attachment phase is deleted. Requires and initializer children remain
unowned until their typed constructors adopt them.

Resolved explicit declaration/member template references also supply the selected
typed function or variable before their nonreal spelling declaration is built.
The producer no longer replaces a provisional qualifier-base target afterward.
Deferred class-template completion reuses its owned parameter surface, and
operator direct-symbol checks query the exact table basis rather than the
canonical function-symbol accessor.

Dependent function template-ids may retain multiple canonical typed candidates
until substitution resolves the overload. JSON accepts exactly one primary or
at least two distinct canonical candidates with the exact name and scope; it
rejects missing, duplicate, or conflicting identities before allocation. Candidate
lists are construction inputs and are checked without replacing them later.

Class instantiations keep complete semantic identity arguments in
`semanticTemplateArguments`; `templateArguments` contains only the declaration's
written source surface. Consumers of canonical type identity must use the
semantic accessor. An empty source-owned `SgTemplateParameterList` represents
`template<>` and is preserved by JSON, including its typed separator and owner.

Implicit function instantiations retain canonical substitution arguments in
`deducedTemplateArguments`; their `templateArguments` written list is empty.
Written call arguments belong to the per-use reference. AUD2298 migrates the
stale reference-NTTP consumer while preserving binding checks; do not populate
written fields with semantic arguments to satisfy an old consumer.

AUD2299 includes canonical substitution arguments in free/member function-scope
mangled identities. Regenerate persisted analysis identities derived from these
scopes; distinct same-signature specializations previously collided. Class source
matching also uses complete semantic arguments. AST layout and schema stay unchanged.

AUD2300 also uses canonical substitution arguments for edited literal-call
invalidation, explicit emission and contextual qualification. Per-use written
arguments and semantic declaration arguments retain independent owners.

Class-symbol argument matching and filtered class lookup now require complete
semantic arguments, including defaults (AUD2301). Member `isTemplateFunction`
queries the source pattern's own template parameters; an empty argument pack
remains a function-template specialization. ABI and schema remain unchanged.

`buildNondefiningClassDeclaration_nfi` requires explicit complete semantic and
selected-template deduction lists for instantiations, including when empty
(AUD2302). Written arguments never supply missing semantic inputs.

Generated variable builders leave physical output ownership unbound while the
declaration is detached. Their scope argument supplies semantic scope, not an
insertion target's physical file. Publication uses the actual lexical attachment:
relative insertion uses its target, and direct scope append/prepend publishes a
fresh detached transformation into that scope. Previously the variable builder
prematurely bound the scope's file, preventing insertion beside a header node.
The physical-conflict assertion remains strict. Regenerate/rebuild the IR
implementation and library; AST layout and JSON schema are unchanged.

Pointer and reference factories use their existing exact base-type caches;
they no longer register wrappers by mangled name in the global type table.
The unused `SgType::handledUsingLocalTable` API is removed. Rebuild consumers
against matching generated headers and library. AST layout and schema are unchanged.

Schema 247 adds task-local semantic firstprivate/private declaration lists and
their construction-availability bit to `SgOmpTaskStatement`. The frontend finishes
these typed bindings before returning the OpenMP construction result. Written
clauses remain source-owned; lowering consumes the semantic lists without adding
implicit source clauses. Variable substitution and type-preserving outlining
carry the exact declaration bindings. Copying binds each borrowed declaration
edge through the exact copy transaction. JSON readers require the availability
flag, validate the declaration references, and resolve their dependencies before
initializing the task environment during construction, before publication.
Rebuild all native and installed consumers for the changed node layout. The old
`patchUpFirstprivateVariables` API and eight unused UPIR enum groups are removed.

Schema248 adds `SgOmpExecStatement::source_form_is_attribute`. Native Clang
parallel/worksharing-for attribute directives and supported clauses construct
typed REX nodes directly. Sequence syntax emits an equivalent ordered series of
directive attributes; no source text is replayed. Readers require the source-form
flag before constructing executable directives. Rebuild generated ASTs and all
native/installed consumers for the layout change; regenerate older checkpoints.
Captured declaration contexts retain their semantic-only role while source-body
ownership follows their exact parents to the enclosing source function. C/C++
for and for-simd output now uses the shared strict directive unparser.

`setTemplateArgumentsInDeclaration` changes only the written list; semantic
identity and deduction remain independent. Defining builders copy their exact
canonical lists before constructing the definition.

Unused `SageBuilder::buildClassTemplateType` is removed, together with its three
private helpers. Downstream callers must use the typed class builders with exact
primary/selected-template identities and argument lists. Rebuild/relink against
matching headers and library; the removed symbol is a public API break. No AST
layout or serialized-schema change accompanies it.

Both variable-template builders replace `template_variable_entity_kind` with a
mandatory `SgTemplateVariableDeclaration*` argument after the semantic scope.
Pass null for a primary template and the exact selected primary or partial for
a specialization. The builder retains that input before any publication and
requires redeclarations to retain the same selected identity.

AST JSON schema 150 requires the existing directive's `do_not_instantiate`
boolean. Earlier schemas omitted it and incorrectly reconstructed `extern
template` declarations as definitions. The reader requires the boolean before
allocation, preserves it before publication, and never defaults a missing role.

`determineFileType` now requires the exact `SgFileList*` owner after the project
argument. Normal project callers pass `get_fileList_ptr()`; external module
callers pass the importing source file's external list. Ownership is retained
during construction and before frontend parsing, with no later transfer.

AST JSON schema 152 removes the duplicate `external_function_parameter_scope`
payload and string `external_function_parameter_scope_source`. The checked
`external_function_parameter_scope_owner` enum selects the first-nondefining
peer (1) or defining peer (2); its declaration owns the sole scope payload.
Earlier schemas and removed fields are rejected. External scope construction
now precedes its parameter construction; private symbol population consumes
that exact scope. Cross-file closure transactions remain under review.

`RewriteRuleCombiner` and `PatternActionRule` are noncopyable because they own
their polymorphic children. Construct independent rule sets and retain one owner;
copying them previously aliased child ownership. Private binary patterns follow
the same rule. No clone or compatibility layer is provided.

`SgAsmOp` now takes `constraint_info`, modifiers and its expression. The value
contains the original assembly constraint spelling and Clang-validated memory,
register, immediate, matching-input and tied-output information. The old
single-character constraint enum, raw/normalized output selector, duplicate
constraint string and output-role setter are removed. Output role comes from
typed modifier bits. Rebuild clients against the regenerated AST. Schema 153
preserves the typed constraint value, operand role and name, assembly text,
clobbers and volatility. Both readers bind the owned expression during operand
construction; the graph reader constructs the statement from its complete
operand list. Earlier schemas are rejected. Remaining assembly variants and
full acceptance still require review.

Schema 155 requires `expressions` for anonymous `SgExprListExp` references;
earlier schemas omitted their children and are rejected. Both list readers
construct distinct unowned children before attachment. CUDA launch bounds may
contain one scalar or a list of one to three exact typed operands. Use
`SgFunctionDeclaration::cuda_launch_bounds_operands_are_valid` for private
construction checks and `validate_cuda_launch_bounds_expression` after binding
the declaration owner. Neither operation queries a syntax list's value type.
Native CUDA producer coverage and full acceptance remain open.

The 46 unused private `p_builtin_type` globals are removed from generated AST
classes. Type factories retain their live canonical tables; no public accessor
or JSON field is removed. Rebuild against the regenerated headers. Linked
validation passes; full acceptance remains pending (AUD-1111).

Schema 161 removes `SgNamedType::autonomous_declaration`, its accessors and the
redundant named-type JSON field. Use declaration-owned source ownership and
`isAutonomousDeclaration` for tag declarations; canonical types do not own a
source declaration surface. Older schemas and the retired type field are
rejected. Rebuild all consumers against the regenerated layout. Linked build,
24 focused controls and targeted required-fields Memcheck pass; full acceptance
remains pending.

KIND/LEN construction now rejects non-integer selectors and contradictory
literal/folded metadata before publication (AUD-1137). KIND must be a positive
integer constant; LEN retains dynamic integer expressions, length shapes and
exact wide values. The JSON schema and installed API are unchanged. Twelve
focused native controls and two targeted Memchecks pass; full acceptance remains
open.

CHARACTER and pointer/reference interning now preserve exact expression/base
identity when different bindings have equal mangled names (AUD-1139/1141).
Names remain index labels; they do not distinguish those bindings. Checkpoint
reconstruction preserves live general cache entries (AUD-1140). These changes do
not alter the installed API or schema. Public name-only queries still need review.

AUD-1169–1172 leave schema 165 and public layouts unchanged. Callgraph
canonicalization consumes exact declaration links without block-prototype
name/type recovery. Materialized extern-template bodies remain semantic analysis
definitions; declaration-only extern templates do not invent bodies. Simple
nonreal uses sharing a name remain distinct when their exact targets differ,
and named TypeLocs retain known declaration targets during construction. The
local-pattern control requires complete typed ownership through both sides
of reconstruction. Whole-reader/template-family acceptance remains open.

Schema168 adds required `SgTypeIdOp::operand_is_evaluated` (AUD-1184). Its
constructor and both `buildTypeIdOp` builders require the exact result type and
evaluation boolean; there is no default or inferred reader value. Clang supplies
`isPotentiallyEvaluated`. Older schemas are rejected. Regenerate and rebuild
consumers. Native, reader and actual JSON controls pass; full acceptance is open.

The unused `SageInterface::getDefaultConstructor` API is removed. Use the exact
constructor declaration retained by `SgConstructorInitializer`; the deleted
member-list scan selected the last constructor regardless of its signature.

`SgSizeOfOp::expression_operand_is_evaluated()` queries existing typed VLA
metadata (AUD-1185); it does not infer values from spelling or mutate the AST.
Schema168 and node layout remain unchanged. It describes expression operands;
fresh type-operand bound execution still needs explicit ownership. Focused
native/reader/analysis and actual JSON controls pass; full acceptance remains open.

The unregistered, noninstalled `taintedFlowAnalysisTest` prototype and its private
analysis files are removed (AUD-1187). Use the library `TaintAnalysis`; its
`taintAnalysisTest` driver and seven registered tests remain.

Schema169 removes `SgFunctionDeclaration::template_instantiation_pattern_is_unpublished`
and its accessors (AUD-1189). Exact typed pattern dependencies are constructed
before function-context capture and linked before cache publication or body
construction. Both record and graph readers reject the retired property
before allocating a function. Rebuild against regenerated headers and regenerate
older checkpoints. The linked build and nine focused controls pass; general body
completion, template publication and full acceptance remain open.

AUD-1190 uses the existing typed dependency argument to construct reader function
patterns before publication, rather than repairing them during edge linking.
Direct factories require a typed primary; graph preflight rejects self, chained,
wrong-kind and narrow-instantiation misuse before allocation. Eight focused
controls pass. Schema169 and public APIs are unchanged; general body/signature
completion and full acceptance remain open.

BLOCK's existing role/name fields and END BLOCK label aliases are validated
before publication; the label is constructed with its exact block owner
(AUD-1198). Fortran statement-label copies retain lexical target scope and bind
to the enclosing program-unit symbol table. Schema169 and public APIs are
unchanged for this batch.

Schema170 adds FORALL/DO CONCURRENT's required typed statement kind,
end-statement flag and construct name (AUD-1199). Readers reject missing fields,
invalid enum values and conflicting child ownership before construction;
header controls and body precede publication, and exact label symbols precede
their references. Regenerate older checkpoints; no backward-default recovery.
Focused FORALL/DO CONCURRENT reconstruction and FORALL/reader Memcheck pass;
remaining construct families and full gates stay open.

IF/WHILE names and source roles, and namespace inline status, are now required
before factory allocation (AUD-1201); late restoration setters are removed.
Existing schema170 fields and public APIs are unchanged. Negative fixtures for
later invariants must supply these prerequisites. General child construction
remains under review. Flang now retains the
parser's written IF/ELSE IF construct names before publication (AUD-1202), using
the existing fields. Written plain ELSE names remain unrepresented.

AUD-1203 constructs numeric IF/start/ELSE/end label references with their exact
typed owner before edge linking. The existing ELSE role is valid only for IF.
Label dependencies and lookup distinguish label symbols from ordinary symbols
on the same declaration; serialization uses the native label name. Boundary
labels use their exact program-unit scope. Four redundant frontend wrappers
and late boundary-label setters are removed. Native copy assertions are retained.
Fortran enumerators retain written initializer expressions separately from the
typed evaluated integer; implicit enumerators have no fabricated initializer.
The synthetic `SageTreeBuilder::Enter(SgEnumVal*...)` helper is removed.
Ordinary numeric enum emission now reads the typed value directly.

Fortran module boundary labels use the module definition's symbol table in
native copying and both readers (AUD-1205). The reader no longer substitutes a
nearest lexical scope. Function labels and alternate-return parameter scopes
retain their typed ownership. No schema or public API change.

Schema171 requires SELECT/switch construct names before factory allocation
(AUD-1204). Both readers construct range bounds from their three exact typed
children; explicit syntactic-absence nodes represent open endpoints and missing
stride. Range/subscript construction shares the existing bounds path. Regenerate
older checkpoints; no missing-name default or late restoration is provided.
Flang retains CASE header numeric labels before clause publication, using the
existing statement-label fields and program-unit symbol identity.

Schema172 requires WHERE's existing has_end_statement and string_label fields
(AUD-1209). Regenerate older checkpoints. WHERE/ELSEWHERE reconstruct from typed
owned conditions, blocks and clause chains; unmasked ELSEWHERE retains explicit
syntactic absence. Clause labels bind after the enclosing construct supplies
their exact program-unit owner. Missing fields, malformed ownership and cyclic
chains fail before allocation; no name/form defaults or late child replacement.

CASE/DEFAULT decoding now requires existing names and exact owned keys/bodies
before clause allocation (AUD-1210). The worklist assembles Fortran key lists,
including already exact-owned literal children, and retains GNU range-end
operands before publication. Edge linking verifies those identities rather than
replacing fields. The schema and public API are unchanged.

ASSOCIATE, BLOCK, SELECT CASE, WHERE and FORALL frontend producers now insert
the enclosing statement after body/clause construction, including single-line
WHERE/FORALL (AUD-1208). Typed parent links are available during construction.
The duplicate WHERE label wrapper and uncalled EndsWith helper are removed;
shared boundary-label handling preserves exact program-unit identity.

DO controls/body/name/end role now construct in the dependency worklist
(AUD-1200). Label symbols follow their exact DO or terminal-statement basis;
end labels attach during reference construction. The late construction loop
and setters are removed. Existing schema170 fields and public APIs are unchanged.
Labeled/named DO reconstruction and focused Memcheck pass; general dependency
ordering and rollback remain open.

Explicit END DO labels now retain their typed loop owner and end role for
counted DO, DO WHILE and DO CONCURRENT (AUD-1206). Emission uses the existing
has_end_statement field to distinguish block endings from legacy termination
labels. The WHILE unparser no longer manufactures END DO for an invalid target.
IF/DO statements enter their enclosing statement list after body construction.
No schema or public API change.

End-label CFG resolution now distinguishes physical statement entry from block
loop continuation using typed owner/reference identity (AUD-1207). Counted DO
and WHILE use their existing child-continuation indices; legacy terminal
assignments execute before iteration continues. No AST or codec change.

Public configuration now comes directly from
`cmake/rosePublicConfig.h.in.cmake` (AUD-1191), retaining exported macro names and
values. Extend that template for public metadata. The installed
`publicConfiguration.pl` conversion utility and `generate_rosePublicConfig`
build target are removed; CMake produces the header during configuration.
Private configuration includes the shared public header. Native macro/registry
equivalence passes; alternate configurations and installed consumers remain open.

The installed Clang translator header removes the unused virtual
`VisitOverloadExpr` hook (AUD-1339). Rebuild translator subclasses against the
new library; use the concrete `VisitUnresolvedLookupExpr` and
`VisitUnresolvedMemberExpr` hooks for those expressions. The native frontend
rebuild and focused controls pass; installed-consumer validation remains open.
AST layouts and schema178 are unchanged.

Both JSON readers now construct the existing SgVarArgOp kind through its exact
result type and exclusively owned operand (AUD-1341). Inline records require
`operand_expr`; regenerate any old inline vararg records that omitted it.
Existing graph record fields and schema178 are unchanged. The Clang frontend
preserves the written TypeLoc for va_arg and rejects invalid pseudo-destructor
source ranges before reference allocation (AUD-1340).

Both JSON readers now construct SgCompoundLiteralExp from its exact variable
symbol, known type and owned compound-literal aggregate initializer (AUD-1344).
Graph and inline properties require symbol and symbol_declaration; regenerate
older compound-literal checkpoints that omitted them. Schema178 and native
layouts are unchanged; this kind previously could not be reconstructed.

Typeid type operands retain their written TypeLoc, including reference, cv and
typedef structure (AUD-1342). Its existing result type and operand evaluation
role remain unchanged. Consumers needing the adjusted RTTI type must apply
language semantics to the typed operand; they must not discard the source type.
Inexact translated result types are rejected instead of acquiring synthesized
const qualification. Schema178 and public/native layouts are unchanged.

SizeOfOp and AlignOfOp now carry SgExpression::unary_type_trait_kind_enum at
construction (AUD-1345). Pass the operator kind as the fourth native constructor
argument or third SageBuilder argument. Ordinary sizeof defaults to e_sizeof;
the existing __alignof__ builders default to e_preferred_alignof. The frontend
passes explicit kinds, including e_alignof and e_datasizeof. Pack size uses
e_sizeof_pack; its separate field/accessors are removed. Rebuild every native
consumer against the changed layout and ABI. Schema179 requires operator_kind
on both node families; regenerate older checkpoints. The readers reject missing,
wrong-family and retired flag metadata. Focused native, reader, copying and
callgraph controls pass on their recorded source manifests. The pack JSON
checkpoint passes after the canonical-identity reader fix (AUD-1347).
Installed-consumer and full validation remain open.

Readers preserve detached canonical non-type parameters and template-template
identities without inventing lexical scopes (AUD-1347). Canonical self identity,
reciprocal typed declarator ownership and nested declaration scope are validated
before construction. Schema179 and native/public layouts are unchanged by this
reader fix.

Schema173 requires `is_if_constexpr_statement`, `if_constexpr_value_known` and
`if_constexpr_value` on every IF record (AUD-1218). These are existing typed AST
fields, with no public API or layout change. Missing or contradictory states
are rejected before allocation; regenerate older checkpoints. Frontend,
copying, reconstruction, emission and CFG retain constexpr branch selection;
condition declarations keep their executable declaration edges. General
scope construction and C++23 IF CONSTEVAL remain under review.

Unary-transform types now expose `findCanonicalSemanticType(operand, operation)`
for exact, read-only cache lookup. Native destruction retires their weak operand
cache entry, and reader rollback owns fresh results. AST layouts and schema173
are unchanged; installed-consumer acceptance remains open (AUD-1236).

IsConstant no longer extracts sizeof/data-size/pack operators as integer literal
values. They retain their typed expression representation. Structural expression
keys now distinguish local declarations and use positions/types for bound function
parameters; regenerate persisted analysis keys made with the previous mangler.
Native layouts and schema180 are unchanged (AUD-1385–1387).

Explicit constant array bounds retain their numeric extent and exclusively own
their written expression through originalExpressionTree. Type-owned source trees
use graph records; layouts, APIs and schema180 are unchanged (AUD-1388/1389).

The obsolete getFiniteDifferencingRules factory is removed (AUD-1396). It
interpreted ordinary comma expressions as a synthetic old/new-value protocol.
Use simpleIndexFiniteDifferencing for source-AST finite differencing;
getAlgebraicRules remains available, with distribution/reassociation limited
to unsigned arithmetic. Native AST layouts and schema181 are unchanged.
Rebuild installed consumers; their final acceptance remains open.

SgIfStmt now stores if_statement_kind_enum instead of the constexpr boolean
(AUD-1399). Use set_if_statement_kind with e_if_ordinary, e_if_constexpr,
e_if_consteval or e_if_not_consteval; the constexpr query remains derived from
that kind. Consteval IF has no condition statement and requires compound bodies.
Runtime CFG selects its runtime arm without inventing an expression. Existing
constexpr selection bits retain their constexpr-only meaning. Schema182 requires
the integer if_statement_kind property instead of is_if_constexpr_statement.
Regenerate older checkpoints and rebuild all native and installed consumers;
the native node layout changes. The native library and three affected drivers
rebuild; reached language/checkpoint and copy/CFG/reader controls pass. General
analysis and installed consumers, and all full/memory gates, remain pending.

AUD-1400 replaces SgFunctionDeclaration's stored is_constexpr bit and setter
with constexpr_specifier_enum: e_constexpr_unspecified, e_constexpr and
e_consteval. get_is_constexpr remains a derived semantic query, true for both
constant-evaluation specifiers. Native builders accept withConstexprSpecifier
on their construction ownership; redeclarations inherit their exact canonical
specifier when no explicit value is supplied, and disagreement fails hard.
Schema183 requires constexpr_specifier for function and external-function
records; variable is_constexpr is unchanged. Regenerate older checkpoints and
rebuild all native/installed consumers. The native library and three drivers
rebuild; seven language controls and the corrected reader/copy/rejection control
pass on their recorded manifests. Other native/installed consumers and general
publication/semantic immediate escalation remain open. No full or memory gate
is accepted.

AUD-1407 introduces SgClassDeclaration::e_lambda_closure and the typed
lambda_expression construction-recipe reference. LambdaExp owns only its capture
list; lambda_closure_class and lambda_function are semantic references to complete
canonical declarations under auxiliary owners. Consumers must use those typed
edges and roles rather than expression-parent tests. Schema184 rejects earlier
ownership records. Regenerate checkpoints and rebuild native/installed consumers
because class layout changes. The required-fields test pins184; malformed closure
and call-operator parents are now rejected by the auxiliary-owner preflight
before the lambda validator. Existing rejection and rollback assertions remain.
ROSETTA generation, native rebuilding, eleven source-projection controls, two
lambda-containing frontend/reader controls and the preflight control pass on
their recorded sources. General copy/lifetime and full acceptance remain pending.

AUD-1411 adds LoopTreeNode::GetRuntimeAst, forwarded by shadow nodes. IF controls
return only their runtime evaluation; consteval and known constexpr expression
tests have none. GetSourceAst and GetOrigStmt retain source roles. Dependence
reference collection now requires the computation: construct DepCompAstRefAnal
and DepCompCopyArrayCollect with (comp, root). Native loop-processing consumers
and two focused drivers rebuild and pass; rebuild installed consumers for the
changed virtual interface and constructor signatures. AST schema184 is unchanged.
Transformation observers, whole-loop correctness and full gates remain pending.

AUD-1412 adds LoopTreeObserver::ObservesNode to select subscriptions. Its default
retains ordinary observer behavior; dependence graphs subscribe to structural
containers and their own statement nodes. IsSimpleStmt is now a typed function
using source identity and loop role, including expressionless consteval controls.
LoopTreeDepGraphSubtree now takes (root, graph), dropping the unused computation
argument. Rebuild native/installed consumers for the observer virtual interface
and subtree constructor. Native consumers and two focused drivers rebuild/pass;
installed, transformation lifetime and whole-project acceptance remain open.
AST schema184 is unchanged.

AUD-1430 adds an optional exact source/clone pair output to LoopTreeNode::CloneTree;
its symbol signature changes. AUD-1432 removes LoopTreeGetClassName and its virtual
slot. Use typed node roles/casts for dispatch and toString for diagnostics.
Native loop-processing consumers rebuild and the scoped splitting control passes;
rebuild installed consumers before acceptance. AST schema184 is unchanged.

AUD-1428 removes SplitStmtInfo2's unused loop/cut fields and accessors; its
constructor now takes only original and cloned statement nodes. Scalar bounds
stay in typed range nodes. The unused UpdateSplitNodeEdge template is removed.
Rebuild native and installed consumers for the event layout and constructor.
Native consumers and the affected dependence control pass; installed and full
acceptance remain open. AST schema184 is unchanged.

AUD-1427 removes DepCompDistributeLoop's unused three-argument selective overload
and Result type. Its retained two-argument SCC operation now returns void; all
four retained callers already discard the result. Rebuild native and installed
consumers for the changed ABI. Retained native callers rebuild; installed and
whole transformation acceptance remain open. AST schema184 is unchanged.

AUD-1424 removes the symbolic LoopTransformInterface::IsLoop helper and the
condition-only AstInterface::CreateLoop overload. Use AstInterface::IsLoop for
typed native header/body access; retain the source owner through reconstruction.
Generic LoopInfo no longer encodes init/condition/increment as numeric bounds
and stride. Read those headers from GetSourceAst and edit typed AST nodes;
numeric mutation is reserved for counted loops. Rebuild native and installed
consumers for the removed methods. Native consumers and the two affected regular
controls pass; installed and whole-project acceptance remain open. Schema184
and the six-argument counted-loop factory are unchanged.

AUD-1420 adds AstInterface::CopyScope and an optional typed mapping output to
CopyAstTree. Rebuild consumers for that signature and the changed AstInterface
and LoopTreeIfCond layouts. Symbolic variables use the same typed copy bindings;
LoopTreeReplAst applies replacement to emitted copies instead of mutating source
ASTs. AUD-1421 retires exact declaration indexes without weakening isolated
deletion. Focused controls pass; schema184 is unchanged. General loop emission,
allocation failure, installed and whole acceptance remain open.

AUD-1437 removes SgFunctionDeclaration::hasEllipses. Supply a complete parameter
list with a trailing SgTypeEllipse before function construction; do not mutate
shared function types afterward. Build22977 regenerates and compiles/links the
affected native consumers; three focused controls pass. Rebuild installed
consumers during acceptance. AST layout and schema184 are unchanged.

AUD-1419 makes native variable-reference copies honor exact external declaration
plans and hard-reject inconsistent bindings. Public APIs and schema184 are
unchanged. Early construction, emission and whole acceptance remain open.

AUD-1413 removes LoopTreeDepCompCreate's unregistered MultiGraphObserver base and
callback. Shared dependence nodes now retain the exact source registry until
their final release; rebuilt graphs register reused nodes. ObserveObject dispatch
uses a registration snapshot and honors cancellation during callbacks. Rebuild
native/installed consumers for the changed computation/node layouts. The affected
native consumers and two focused drivers rebuild/pass; general observer/lifetime,
installed and full acceptance remain open. AST schema184 is unchanged.

AUD-1414 stores PtrMapWrap's typed std::map directly and removes PointerHolder
and VoidPtrMap. Missing-key Map queries do not insert or allocate. RemoveMapping
and null-value insertion erase entries, invalidating only the erased iterator;
remaining entries retain ordinary std::map iteration behavior. DepCompAstRef
compares typed statement/AstNodePtr identities and no longer exposes ToHandle.
Rebuild consumers for changed containing layouts and the removed handle API.
Affected native consumers rebuild; focused regular and ownership Memcheck
controls pass. Installed and whole-scope acceptance remain open; schema184 is
unchanged.

AUD-1415 changes SelfSpatialReuse's induction argument from a string to
const SymbolicVar&, requiring an exact declaration. Rebuild callers and pass
typed loop metadata; no name-only overload remains. Native locality/slicing
consumers rebuild and two focused controls pass. The AST schema stays184;
installed APIs, target sizes and whole-project acceptance remain open.

AUD-1416 constructs copied IF owners and ordinary nontraversed state before
owned children. Regenerate and rebuild copy consumers. AST fields, constructor
APIs and schema184 are unchanged. Focused regular and synthetic Memcheck controls
pass; loop-tree reconstruction, generic copying and full acceptance remain open.

AUD-1417 adds typed loop-tree arm/block containers and retains declarations.
Use GetBodyAst/GetBodyRole for structural source boundaries; GetSourceAst remains
reserved for executable dependence endpoints. Rebuild consumers for the new
virtuals and tree hierarchy. Remove callers of the unreferenced
LoopTreeShadowSelect; no compatibility wrapper remains. AUD-1418 corrects typed
dependence products, including zero dimensions and self-multiplication. Native
focused controls pass; emission, installed and full acceptance remain open.
AST schema184 is unchanged.

AUD-1498 changes TestAstProperties' duplicate-source index to a typed position
key including expanded-token start/end identities. Rebuild consumers of the
validator header for its changed containing layout. Affected native consumers
rebuild; installed acceptance remains open. Public Sg layouts and schema191 are
unchanged.

AUD-1525 changes appendStatementWithDependentDeclaration: supply the original
typed dependency vector and a mandatory source file; the unused original
statement and physical-file arguments are removed. collectDependentDeclarations
accepts an original statement, header policy and physical source identity and
returns the unsorted recursive closure; getDependentDeclarations retains source
sorting. Rebuild native and installed consumers. Object compilation passes;
linked/installed acceptance remains open. AST schema193 is unchanged.

AUD-1533 adds withSourceParameterList to function_declaration_ownership. Supply
an independent detached written list to a defining builder; it validates and
owns the syntax signature before publication. Nondefining builders reject this
property. Rebuild native and installed clients for the descriptor's changed
C++ layout. The native rebuild and nine focused controls pass; installed and
full acceptance remain open. AST schema193 is unchanged.

AUD-1537 adds constructsIndependentClassForward to SgCopyHelp. Its default
preserves the existing external-family copy contract. Explicit independent
forwards require exact projected auxiliary ownership and acquire their own
canonical type without borrowing a definition. Rebuild all native and installed
consumers, including copy-helper subclasses, for the changed virtual interface.
The native rebuild passes; eight of nine focused controls pass and structural
lambda copying aborts. AST schema193 is unchanged.

AUD-1538 changes native copy policy for declaration-family and explicit
declaration scope edges to COPY_REFERENCE. Rebuild the generated native copy implementation;
the field layout, public interface and AST schema193 are unchanged. Typed copy
bindings now establish these identities before dependent constructors consume
them. Native rebuild and all eleven affected controls pass with complete source
guards; full and installed acceptance remain open.

AUD-1540 adds a SgNode overload of findAssociatedSymbolInTargetAST for initialized
name symbol bases; the existing declaration overload and ABI remain available.
The shared implementation uses exact destination tables and projected function
types. Rebuild consumers of sageBuilder.h. Native rebuild passes; broader focused
validation has five passes and four failures. Installed acceptance remains open;
AST schema193 is unchanged.

AUD-1541 corrects semantic retained-definition and parameter-scope ownership
classification without changing public interfaces, native layout or schema193.
Native rebuild passes. The initial affected run has five passes and two failures;
the revised ownership classification passes all three copied-header controls.
Full and installed acceptance remain open.

AUD-1542 adds defining-record selection to the Clang parameter-context collector.
Rebuild its utility-header/PCH consumers; native rebuild and all five focused
controls pass with complete source guards. AST layout and schema193 are unchanged.
Full, installed and memory acceptance remain open.

AUD-1543 shares and strengthens native canonical namespace ownership validation.
Public interfaces, native layout and schema193 are unchanged. Native rebuild
and all eight affected controls pass; installed and full acceptance remain open.

AUD-1544 publishes ordinary class symbols in the namespace's canonical table,
while declarations retain their exact semantic fragments. Class builders query
that table directly instead of borrowing project-wide symbols. Defining and
forward fragments must belong to the same exact canonical family and translation
unit. Interfaces, native layout and schema193 are unchanged. Native rebuild
and all ten focused controls pass; installed and full acceptance remain open.

AUD-1545/1546 extend canonical namespace symbol ownership to typedefs, enums and
variables, preserving declaration fragments and requiring exact canonical
predecessors within one translation unit. Enum definition symbol validation now
precedes allocation. Publication and target projection query direct canonical
tables. Interfaces, native layout and schema193 are unchanged. The eleven-control
typedef/enum batch and final eight-control variable batch pass with source guards;
installed, full and memory acceptance remain open.

AUD-1667 adds identity-map and pending-definition overloads of
SageInterface::deepCopySemanticSubtree and Outliner::getLibSourceFile, preserving
their existing entry points. AUD-1668 removes the uncalled
OmpSupport::transOmpTargetLoop and transOmpTargetLoop_RoundRobin interfaces;
OpenMP lowering enters through OmpSupport::lower_omp. The affected rebuild and
three focused controls pass; installed consumer acceptance remains pending.
Neither change alters AST layout or schema200.

AUD-1669 constructs generated Fortran IF statements as complete block forms in
SageBuilder::buildIfStmt. Parsed logical IF and ELSE IF retain their frontend
forms without a lowering repair. Interfaces and schema200 are unchanged; the
affected rebuild and eight focused controls pass. Installed and full gates remain open.
