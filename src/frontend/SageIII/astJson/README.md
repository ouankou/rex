# Sage AST JSON Checkpoints

Schema251 requires initialized names' brace-initialization and assignment-copy
syntax flags. Both readers preserve direct-list versus copy initialization;
missing or malformed flags are rejected. Regenerate older checkpoints.

Schema250 adds the typed C/C++ pragma payload kind for OpenMP attributes.
Attribute directives remain opaque through frontend construction and JSON
reconstruction until the REX OpenMP constructor consumes them.

Schema249 requires the typed `expression_list_source_form` field on expression
lists. Plain lists and dependent braced source lists have no standalone value
type. Both readers preserve their source form; absent or invalid values are
rejected. Regenerate older checkpoints.

Schema 248 retains the typed `source_form_is_attribute` flag on OpenMP
executable directives. Readers require it before construction; the backend
emits attribute syntax from typed directive/clauses instead of saved text.

Schema 247 retains each executable task's typed semantic firstprivate/private
declaration references separately from written clauses. Readers require its
completed environment and preserve exact copied or external declaration identity.

Schema 246 requires `aliased_type` on every `SgNonrealDecl`. Dependent alias
template uses retain their exact computed type alongside the written primary
and arguments; other uses record an absent type. Readers construct this borrowed
projection once, before the declaration-owned type. Type references resolve that
owned type; missing, contradictory or cyclic projections are rejected.

Schema 245 retains the exact origin of member class templates instantiated with
their enclosing class, independently of partial-specialization identity.

Schema 244 retains typed default-argument availability on implicit deduction-guide
parameters. These markers contain no initializer expression; constructors own
the actual default values.

Schema 243 retains the owned converted `condition_value` of declaration
conditions in IF, WHILE, FOR and SWITCH. Readers require that exact expression
and its control-statement parent; expression conditions have no separate value.
CFG execution evaluates the declaration, then this predicate, then branches.

Schema 242 requires the typed `c_implicit_declaration` reference ID (zero when
absent). Both readers preserve the temporal binding selected before a later
declaration; its dependency must exist before constructing the reference.

Schema 241 retains function prototype kind independently of an empty parameter
list, and ordinary function references retain their exact per-use type instead
of following later declaration changes. Readers require `has_prototype` on
function types and reconstruct references with their serialized type before
publication. Unprototyped signatures reject parameters, member qualifiers and
exception metadata; older schemas are unsupported.

Schema 238 preserves Clang's ineligible/not-selected result for constructors and
copy/move assignments in the function modifier vector. Builders require this
fact explicitly; readers reject it on unrelated functions. Schema 237 records
cannot supply this fact and are rejected.

Schema 237 requires typed triviality for copy/move assignments as well as
constructors. Function modifier bits41/42 now classify these special members;
the vector width is unchanged. Both readers reject missing or conflicting facts
before construction. Constructor defaultness remains separate from triviality.

Schema 236 requires `structured_binding_scope` on initialized names. A
decomposition owns this private declaration scope, its aliases and their holding
storage; lexical visibility uses alias symbols. The scope's outer context comes
from its owning initialized name. Ordinary names record zero. Readers reject
conflicting scope, pattern and alias ownership before construction.

Schema 234 stores FORMAT descriptors as typed variants, exact repeat counts and
owned item-list edges. Character bytes and delimiters are distinct from descriptor
syntax; nested and unlimited groups retain their children. Readers require these
fields before constructing FORMAT nodes; the former string payload is unsupported.

Schema 235 preserves structured-binding aliases as typed initialized-name data:
their decomposition owner, binding expression and optional tuple holding variable.
Aliases have no variable initializer; holding variables retain their own initialized
reference storage and lifetime. Anonymous decomposition references name their exact
storage node. Both readers require these relationships during construction and
reject conflicting kinds, identities and ownership before allocation.

Schema 233 requires initialized-name records to include `structured_binding_pattern`:
zero records absence; otherwise it must identify the exact owned expression list.
Both readers reject malformed ownership and binding references before allocation.
Source reconstruction binds each pattern before publishing its initialized name.

Schema 217 requires materialized temporaries to retain `materialized_storage_duration`
and `materialized_lifetime_owner`. Full-expression storage has no extending
declaration; extended lifetimes borrow the exact initialized name. Field defaults
retain containing-object duration until their concrete expansion is constructed.

Schema 216 requires `bound_temporary_destructor` on expressions marked as bound
temporaries. The field borrows Clang's selected destructor declaration; readers
reject missing, non-destructor and incompatible class bindings before constructing
the expression. Ordinary expressions must omit the property.

Schema 210 requires inline OpenMP init modifiers, modifier lists and append_args
operations to retain their typed children. Readers reject missing child fields,
invalid payloads and conflicting ownership before constructing their owners.

Schema 206 represents Fortran BOZ constants as `SgFortranBOZVal` with exact
128-bit `bits` and the distinct `SgTypeFortranBOZ` marker. Both readers reject
signed payloads or numeric types before constructing the literal.

Schema 205 requires `SgTypeRequirement`'s explicit source qualification fields.
Both readers validate them before constructing the requirement. Checkpoints
preserve the written dependent qualifier rather than emitting its semantic owner.

Schema 181 requires `original_expression` on inline expressions and type-owned
expression lists. It stores an exclusively owned typed source tree or an explicit
null reference; missing fields, borrowed owned trees and provenance chains reject.

Schema 177 identifies Fortran I/O operations by their concrete AST node kinds.
The redundant `io_statement` property and native enum/accessors are removed;
rebuild consumers and regenerate older checkpoints.

`astJson` is an opt-in REX tool for serializing one complete `SgSourceFile`
subtree to a deterministic JSON file, reading that file back, reconstructing a
fresh Sage AST, replacing the original file in the active `SgProject`, and
continuing the normal compiler path with the reconstructed file.

The tool is intentionally strict. If a Sage relationship needed by the active
checkpoint cannot be represented or reconstructed, serialization,
deserialization, or round-trip validation fails immediately. Later compiler
passes must not repair incomplete JSON.

Schema 164 retains default uses as `SgCxxDefaultInitExp` nodes. Each
use owns its expansion, references its exact initialized-name declaration and
requires a typed parameter/member kind. Graph declaration
references require matching node IDs and edges; external references require a
complete function header, a parameter-list role and an exact ordinal. Readers
reject conflicting identities, ownership, value categories and types before
constructing the use. Source argument lists omit these typed nodes; CFG retains
their evaluation and bypasses trailing parameter defaults on permitted copy
elision. Member uses require a collected field with an exact class owner;
external field codecs and omitted initializer producers remain under audit.

Schema 162 stores `SgNewExp`'s exact result pointer independently of its
specified source type. Its owned `array_bound` and required
`array_bound_source_form` distinguish scalar allocation, written bounds,
initializer-inferred bounds and bounds supplied by an array type. Both readers
require these construction inputs; older schemas are rejected.

Schema 160 retains C++ array initialization as an owned common-source binding,
an element initializer and references to that exact binding. Graph references
use node IDs; inline references use an enclosing binding depth or an external
graph ID, never both. The unsigned 64-bit element count uses the existing
integer word encoding. Readers reject contradictory counts, types, categories,
ownership and binding scope before committing the completed loop.
Every lambda capture also requires its owned `capture_initializer` and the
typed `is_init_capture` flag, preserving initialization independently of whether
the source writes an init-capture.

Schema 159 requires `is_elidable` and `elision_permitted` on constructor
initializers, preserving eligibility separately from the frontend's elision
policy. Elidable initializers retain their selected constructor, owned argument
and written delimiter form. Readers reject permission without eligibility,
independent constructor names, contradictory delimiters and incomplete elidable
operands before publication.
Source files also require their typed `standard` and `gnu_standard` fields;
replacement files preserve the frontend's language mode independently of the
stored driver arguments.

Schema 158 requires `allocation_implicit_argument_count` and
`array_bound_is_implicit` on `SgNewExp`. Allocation calls retain their exact
operator declaration independently of constructor initialization; placement
arguments follow the exact implicit argument count (1 through 3): the size,
optional type identity and optional alignment arguments.

Schema 156 stores Fortran implicit typing as owned `SgFortranImplicitRange`
children. Each range has integer `first_letter`, integer or null `last_letter`,
typed `type_spec`, exact `type`, and `source_derived_type_symbol` fields. The
statement stores only `implicit_spec` and its ordered `ranges` edges; readers
construct types, symbols and ranges before publishing their statement. The
obsolete `implicit_none`, variable-name range encoding and `decl_attributes`
fields are removed without compatibility reconstruction.
Source-syntax and `SgAutoType` descriptors carry positive `source_type_identity` values:
repeated identities restore the same type and owned selectors, while distinct
identities remain distinct even when their descriptors otherwise match.
Contradictory descriptors for one identity are rejected before publication.

Schema 117 records `label_basis_kind` for label symbols: declaration (0), Fortran
statement (1), or alternate-return parameter (2). Readers validate the role and
node type before construction; symbol spelling does not select the owning field.

Schema 116 stores `SgOmpUsesAllocatorsClause` definitions solely through the
`uses_allocators_defination` edges. The duplicate `uses_allocators_definitions`
property is removed; reconstruction validates the recorded child ownership.

Schema 115 requires `SgTemplateType.is_pack_substitution`, distinguishing an
empty replacement pack from an ordinary parameter use. A substitution has exact
parameter coordinates and source identity and cannot also be a source pack
expansion. `substitution_arguments` records distinct owned type-argument node IDs;
their typed dependencies are reconstructed before the substitution is published.

This is a text interchange format for Sage AST state. It is not a DOT graph
dump, an inspection-only summary, or a best-effort cache.

## Enabling Checkpoints

Use either command-line options or environment variables:

```text
-rex:ast-json-checkpoint=<checkpoint>
-rex:ast-json-dir=<directory>
```

```text
REX_AST_JSON_CHECKPOINT=<checkpoint>
REX_AST_JSON_DIR=<directory>
```

Supported checkpoint names are:

- `pre-omp-construction`
- `post-omp-construction`
- `post-omp-lowering`
- `all`

`all` runs every checkpoint that is reached by the current compiler invocation.
The JSON files are written under the selected directory with names based on the
checkpoint, input basename, source-path hash, compiler process id, and
checkpoint sequence number. Files are first written to a per-process temporary
file and then published with `rename`, so concurrent compiler invocations
sharing one checkpoint directory cannot observe partial JSON or overwrite each
other's active checkpoint files.

## File Contract

Explicit `roundTripSourceFile` calls always reconstruct the graph. Automatic
compiler checkpoints consult `checkpointEnabled` before calling it.

Schema195 preserves the owned include tree, materialized header wrappers and
resolved directive references. Each directive reference names its exact located
owner and attachment index. Missing records, wrong node types and conflicting
ownership are rejected while constructing the private checkpoint graph.
Header wrappers borrow their translation unit's global scope; readers and native
copying preserve that reference without changing the scope's structural owner.

Schema194 requires is_non_odr_use on SgNonrealRefExp. The flag preserves Clang's
per-use storage classification for explicit variable-template references; true
requires an exact resolved variable target. Both readers reject absent, mistyped
or inconsistent state before reference allocation. Regenerate older checkpoints.

Schema 170 preserves FORALL/DO CONCURRENT's typed statement kind,
end-statement flag and construct name. Readers construct the exact header/body
before publishing the statement, then bind its labels to the program unit.
Missing fields or inconsistent ownership are rejected before construction.
Rebuild consumers and regenerate older checkpoints.

Schema 171 requires `SgSwitchStatement.string_label`, preserving Fortran SELECT
construct names before factory allocation. Both readers construct range bounds
from their typed operands, including explicit syntactic-absence expressions.
Older checkpoints must be regenerated.

Schema 172 requires WHERE's end-statement flag and construct name. WHERE and
ELSEWHERE are constructed from their owned typed masks, blocks and clause chain;
an unmasked ELSEWHERE uses an explicit syntactic-absence expression. Clause
labels bind only after the chain has its exact program-unit owner. Missing
fields, invalid ownership and cyclic chains are rejected before construction.
Older checkpoints must be regenerated.

Schema182 replaces the IF `is_if_constexpr_statement` bit with the required
integer `if_statement_kind`: 0 ordinary, 1 constexpr, 2 consteval and 3 negated
consteval. Consteval IF owns compound bodies and no condition statement.
Only constexpr IF may have a known stored constexpr selection. Older
checkpoints must be regenerated.

Schema183 replaces the function `is_constexpr` bit with the required integer
`constexpr_specifier`: 0 unspecified, 1 constexpr and 2 consteval. This field
also applies to external function records. Fortran functions require 0.
Older checkpoints must be regenerated.

Schema188 adds typed value payloads for signed/wide/UTF character literals and
half/bfloat/float32/float64 literals. Character values use range-checked integers;
floating values preserve their native storage bits. Spelling remains a separate
source field. Older checkpoints must be regenerated.

Schema189 widens SgCharVal's value domain to int64_t, preserving multicharacter
and unsigned UTF8 values. Its literal_type remains the exact semantic type.
Older checkpoints must be regenerated.

Schema191 preserves the frontend's typed enum promotion_type separately from
the written/semantic underlying type. Both readers require the property before
enum allocation; null denotes a declaration without a resolved promotion type.
Arithmetic consumers reject missing or scoped enum promotions instead of using
the storage type. Regenerate older checkpoints.

Schema190 widens SgWcharVal's value domain to int64_t and requires is_unsigned
in each target ABI scalar layout. Wide/UTF character and half/bfloat layouts
come from the frontend's target. Source-file construction validates ABI data
before allocation. Older checkpoints must be regenerated.

Checkpoint files have this top-level shape:

```json
{
  "format": "rex-sage-ast-json",
  "schema_version": 248,
  "root_id": 1,
  "project_files_id": 0,
  "root_kind": "SgSourceFile",
  "node_count": 0,
  "metadata": {},
  "nodes": []
}
```

Required top-level fields:

- `format`: format marker. Readers reject anything except
  `rex-sage-ast-json`.
- `schema_version`: integer schema version. Readers reject unsupported
  versions.
- `root_id`: node id of the reconstructed root.
- `root_kind`: must be `SgSourceFile`.
- `node_count`: must equal the number of entries in `nodes`.
- `metadata`: source-file and checkpoint state.
- `nodes`: deterministic node table.

File-info records carry both the stored `raw_file_id` and the classified
`file_id`. The reader requires both and checks them against the registered
filename and classification flags.

`metadata` records the checkpoint name, root source and output file names, and
the dense `Sg_File_Info` file-id table. Each `SgSourceFile` node owns its command
line, include/dependency ownership paths, language and backend settings, OpenMP
and OpenACC flags, preprocessing completion, and token state in `properties`.

Schema 80 includes the exact `frontendExternalFileList` graph of imported module
ASTs, including nested module dependencies. These files remain non-lexical,
non-output children. Native copying preserves their exact source-file ownership
and independent module trees. Component symbols and their class types refer to collected
node identities. Reconstruction creates fresh module ASTs; replacement never
substitutes the old files or summary declarations for the reconstructed graph.
An absent or empty source command line is invalid.

Each `nodes` entry has this required shape:

```json
{
  "id": 1,
  "kind": "SgSourceFile",
  "variant": 0,
  "flags": {},
  "location": {},
  "properties": {},
  "edges": []
}
```

Node ids are stable within one JSON file. When a reconstructed node is
serialized again during validation, preserved ids are reused so the canonical
comparison can detect real AST drift instead of allocator-order drift.

Function declarations require an exact serialized parameter-list edge,
function type, return type, and semantic scope. External redeclaration records
also state whether their scope and parent come from a collected node or from
the in-boundary redeclaration owner; readers reject a missing or contradictory
context instead of attaching a detached peer heuristically. Auxiliary
declarations must have one exact auxiliary-list parent and lexical scope and
must not also occur in the scope's source-emission statement list.

Function type descriptions require `source_argument_list` (zero when absent)
and `source_trailing_return`. Written prototypes retain their argument-list
container as a node record owning the exact parameter list and parameter scope.
Canonical function types cannot own source prototypes or trailing-return syntax.

`SgStringVal` records require the semantic `value` plus the complete literal
surface contract: lexical `literal_encoding` (`ordinary`, `wide`, `utf8`,
`utf16`, or `utf32` by enum value), `cxx_unevaluated`, `string_delimiter`,
`is_raw_string`, `raw_string_delimiter`, and `raw_string_payload`. The lexical
encoding remains exact even when Clang normalizes an unevaluated literal to its
ordinary semantic kind. Raw delimiter and payload fields must be present even
for a non-raw literal, where both must be empty. Readers reject missing fields,
invalid encodings, and inconsistent raw-string state instead of guessing a
source spelling.

## Node Records

`kind` is the Sage class name used by the deserializer to construct the node.
Unsupported node kinds are hard errors.

`variant` stores the Sage variant tag as a consistency check.

`flags` records transformation state and surrounding-whitespace transformation
state.

`location` contains `start` and `end` source positions. A present file-info
object records filename, raw and physical filenames, file ids, line and column
coordinates, source sequence number, physical file occurrence identity,
physical/source file-id relationships, and
the Sage file-info classification flags, including compiler-generated,
transformation, output-in-code-generation, shared, frontend-specific,
source-position-unavailable, comment/directive, token, default-argument, and
implicit-cast state.

`properties` contains node-specific scalar state and structured references,
including:

- source-file language and backend fields;
- declaration modifiers, nested declaration type modifiers such as Fortran
  `TARGET`, linkage, storage, access, GNU extension fields, and
  name-qualification state;
- using-declaration source identity, including the exact terminal token
  sequence and whether the declaration inherits constructors;
- expression values, lvalue/parenthesization state, expression types, symbol
  references, operator syntax, and type syntax;
- `SgSourceFile::token_list` as explicit `SgToken` nodes, including token
  lexeme text and classification codes, because token-list entries are not
  guaranteed to appear through ordinary Sage subtree traversal;
- Fortran control-statement labels and end-statement flags needed for
  reconstructing blocked forms such as `DO WHILE` and `END DO`;
- independent opening and `END PROGRAM` name spellings, with strict
  case-insensitive identity and named-end metadata validation;
- base statement numeric-label references, represented as `SgLabelRefExp`
  nodes with structured `SgLabelSymbol` references;
- shared Fortran nonblocked-do end statements that are intentionally outside
  normal Sage traversal;
- Fortran label statements, label-symbol bases, and numeric label/type
  metadata, including labels whose basis is a regular statement rather than
  `SgLabelStatement`;
- scope symbol tables, including lookup-preferred symbol entries;
- type records for pointer, reference, array, modifier, class, typedef, enum,
  label, function, member-function, template, decltype, and related Sage types;
- AST attributes selected by the serializer;
- preprocessing attachment records, including full structured file-info objects
  for each attached comment/directive;
- typed structural OpenMP/OpenACC payloads for array sections, iterator
  definitions, mapper clauses, uses-allocators clauses, target-data policies,
  and lowering.

Structured references use node ids for in-file Sage nodes. References to
required nodes that were not collected are serialization errors unless the
relationship is represented by one of the explicit external-record contracts
below.

`edges` is the child/reference edge table for the node. Each edge has:

```json
{
  "field": "declarations",
  "index": 0,
  "target": 2
}
```

The deserializer restores these edges by field name and index, then rebuilds
parent links, scopes, declarations, symbol tables, type links, source-file
state, and checkpoint-specific auxiliary state.

## External Records

Some required Sage relationships point to declarations that are intentionally
outside the serialized source-file subtree. These are represented as structured
external records, not pointer text.

External declaration records currently cover runtime functions, modules, and
class declarations used by required Sage state. Function records require a
non-empty `source_file`. Detached external function peers use the active
checkpoint source file as their source domain when no explicit external-source
marker or enclosing `SgSourceFile` exists. Function records include:

- declaration source position;
- parameter-list source position;
- function type and return type;
- Fortran subprogram kind when present;
- nested parameter-scope declarations such as initialized names and
  `SgUseStatement`;
- parameter-scope symbol tables, including variable, alias, and rename symbol
  entries.

Schema 24 added external declaration identity references for uncollected
`firstNondefiningDeclaration` and `definingDeclaration` peers.

Schema 154 preserves external functions' typed CUDA launch-bounds expressions.
The inline reader attaches them before publishing the declaration and rejects
missing fields, unknown types and borrowed operands that already have an owner.
Loaded declarations must retain the exact recorded expression.

Schema 153 preserves GNU asm statements and operands during owned construction.
`asm_constraint` carries Clang-validated capabilities, immediate restrictions,
output bounds and the exact tied-output index. `asm_modifiers` carries the operand
role; `name` and constraint spelling preserve written syntax. Both expression
readers require the complete typed payload before allocating an operand. The graph
reader constructs statements only after their exclusive operands exist, rejecting
invalid output order and ties before statement allocation. No constraint is
inferred from source strings during reconstruction.

Schema 152 represents external function parameter-scope ownership once. A collected
`SgFunctionDeclaration` may have `functionParameterScope` owned by an external
first-nondefining or defining declaration peer. In that case, the generated
`functionParameterScope` edge is omitted only with the checked
`external_function_parameter_scope_owner` enum: 1 selects the first-nondefining
peer and 2 the defining peer. The scope payload belongs to that external
declaration alone. The reader rejects conflicting local/external owners and
the removed duplicate payload/string selector before AST allocation. A private
external scope exists before its parameters are constructed.

Schema 26 makes serialized symbol lookup preference a strict reconstruction
contract for duplicate symbol-table lookup groups. For each supported Sage
lookup category, such as variable, class, typedef, label, namespace, or
function, deserialization must construct the `SgSymbolTable` so that Sage's
own lookup API returns the entry marked `lookup_preferred`. Duplicate groups
are enforced during symbol-table reconstruction and then validated; a mismatch
is a hard deserialization error.

Schema 27 adds explicit source-file token-list serialization and tightens
external function source identity. `SgSourceFile::token_list` entries are
serialized as `SgToken` nodes with stable `token_list` edges from the owning
source file. External function records with an empty `source_file` are rejected
while writing or reading JSON.

Schema 28 adds the source file's exact token-to-node map, including whitespace
intervals, shared mapping identity, and associated-node order. The reader
rejects missing nodes, invalid intervals, duplicate entries, and broken mapping
aliases instead of regenerating this state heuristically.

Schema 29 makes every serialized scalar field mandatory. The reader rejects
missing or null state instead of substituting defaults from constructors or
older schemas.

Schema 30 removes cached unparser spellings from serialized types. Type
identity and reconstruction are defined only by explicit type structure and
declaration edges.

Schema 31 removes diagnostic node spellings and requires canonical round-trip
comparison to include declaration identities, parent and scope edges, and
preprocessing order.

Schema 32 requires `is_braced_initialized` on every `SgInitializer`, preserving
the semantic distinction between brace- and parenthesis-initialization across
checkpoint reconstruction.

Schema 37 adds the required `end_statement_name` property to
`SgProgramHeaderStatement`. The writer and reader reject inconsistent named-end
flags, named implicit programs, invalid identifiers, and `END PROGRAM` names
that do not identify the opening program case-insensitively.

Schema 39 makes comma-separated source-declaration identity and declarator
index first-class required properties of every `SgDeclarationStatement`.
Readers and writers reject an index without a group identity, and checkpoint
round trips preserve the metadata used by token mapping and structural
unparsing.

Schema 40 makes OpenMP clause and variable-list ownership structural.
`SgOmpClauseStatement`, `SgOmpClauseBodyStatement`, and
`SgOmpGroupprivateStatement` records require one `clause_list` edge to an
`SgOmpClauseList`; clause edges belong only to that wrapper.
`SgOmpFlushStatement` and `SgOmpAllocateStatement` records require one
`variable_list` edge to an `SgExprListExp`; expression edges belong only to
that wrapper. Each wrapper's required parent edge must identify the owning
statement, and each contained clause or expression is parented to the wrapper.
Flattened statement-level `clauses` or `variables` edges are not accepted as a
compatibility form.

Schema 41 removes inclusive and sentinel-valued token mapping fields. Every
`token_mappings` record now carries required nonnegative half-open
`*_begin`/`*_end` boundaries; the leading and trailing intervals must be
adjacent to the required core interval, and an empty else interval is anchored
at the core end. It also makes template and OpenMP source identity explicit:

- every `SgTemplateArgument` record requires nullable `source_spelled_type`
  alongside its canonical semantic `type`; a present source type is the exact
  alias/qualification surface emitted by the unparser;
- every type and template-template `SgTemplateParameter` requires an explicit
  `class` or `typename` keyword value, while non-type parameters require the
  no-keyword value;
- source-spelled class and variable template declaration surfaces persist their
  ordered `source_spelled_template_headers`, and variable surfaces additionally
  require nullable `source_spelled_template_owner_type`;
- every `SgRequiresExpr` owns its optional local parameter list and a required
  ordered requirement list containing only typed `SgSimpleRequirement`,
  `SgTypeRequirement`, `SgCompoundRequirement`, or `SgNestedRequirement`
  nodes; no source-text requires-expression representation is accepted;
- every `SgOmpDeclareSimdStatement` requires its exact function-reference edge
  and nonnegative semantic variant ordinal; and
- every `SgOmpDeclareVariantStatement` requires distinct exact base- and
  variant-function-reference edges plus its nonnegative semantic variant
  ordinal.

Readers reject earlier schemas instead of translating any legacy
representation or reconstructing missing source identity.

Schema 45 requires every `SgInitializedName` to carry a nullable
`fortran_source_type` beside its canonical semantic `type`. For a Fortran
source declaration the edge is required and identifies the exact type spelling
surface; semantic-only declarations must not carry it. Reconstruction rejects
missing, contradictory, or pending source-type contracts instead of deriving a
spelling from the semantic type. Procedure parameter-list entries are semantic
signature identities and must not copy source type-spec, derived-type binding,
interface, or dimension syntax from their independently owned declaration
statements.

Schema 46 requires every `SgUsingDeclarationStatement` to carry its exact
`source_terminal_name` and `is_inheriting_constructor` state. The terminal may
be empty only for a synthesized declaration without a source surface; readers
never derive it from a semantic declaration or template-specialization name.

Schema 47 makes function and auxiliary-declaration ownership exact: function
records require their parameter list, function and return types, and semantic
scope; external redeclarations record their collected or in-boundary owner;
and auxiliary declarations have one structural auxiliary-list parent. It also
requires the complete `SgStringVal` encoding, delimiter, raw-delimiter, and
raw-payload surface instead of reconstructing literal spelling.

Schema 48 makes expression emission semantics exact. Every expression requires
`semantic_wrapper_mask`, every `SgArrowExp` requires `arrow_emission_role`, and
every `SgFunctionCallExp` requires `source_syntax` to distinguish a source call
from a semantic implicit-conversion wrapper.

Schema 49 requires each namespace source fragment to identify whether it is
source-spelled or canonical-generated, and every literal
value to identify source-spelled versus canonical-generated spelling. It also
preserves function-name macro parenthesization and typed source/generated C++
pragma payloads. Function calls additionally require the exact operator
surface, member/nonmember callee form, per-argument source/semantic roles, and
user-defined-literal suffix. UDL lexical operands and per-token suffix
occurrences are serialized separately from the literal-operator's semantic
argument list, including concatenated string literals; readers reject missing,
out-of-range, contradictory, or cross-owned operator metadata. Every
`SgEmptyDeclaration` also requires its immutable lexical role: a source-owned
semicolon, a preprocessing-only anchor, or a zero-width source replacement.
Readers never infer that role from output flags, attached directives, or token
mappings.

Schema 50 introduced a namespace-only final-expanded-token ordinal.

Schema 51 replaces that namespace-only property and requires every
`SgDeclarationStatement` to carry `translation_unit_source_order`: a
nonnegative producer-wide final-expanded-token ordinal when the frontend
publishes one, and `-1` for declarations without an ordered token producer.
The field accepts at most one producer initialization and is then immutable; it
is the single ordering authority for mixed declaration lists across physical
files. External function, module, and class objects are semantic surrogate
records rather than collected lexical declarations, so their required order is
exactly `-1` even when the referenced declaration originated in source.

Schema 52 removes `SgOmpAbsentClause` and `SgOmpContainsClause` from the
expression-clause hierarchy. Each now requires a nonempty immutable
`directive_kinds` array containing only typed OpenMP directive grammar
terminals in source order. Readers and writers reject unknown, empty, or
duplicate entries; no expression list or directive-name string compatibility
form is accepted.

Schema 53 removes the `SgFile`
`suppress_variable_declaration_normalization` property. Exact comma-separated
source declarations are represented unconditionally by typed
`SgDeclarationGroupStatement` ownership, so declaration grouping is no longer
a file option or an unparser-formatting mode.

Schema 55 requires every `SgProcedureHeaderStatement` to record its exact
Fortran source form: semantic-only, procedure header, typed procedure
declaration, or typed `EXTERNAL` declaration. This keeps source-written
procedure type declarations attached to their canonical function symbol and
prevents readers or unparsers from reconstructing them as variable
declarations from modifiers or names.

Schema 56 makes the remaining Fortran type-parameter categories explicit.
`TYPE(*)` and `CLASS(*)` use distinct typed nodes rather than
`SgTypeDefault`; nonconstant `CHARACTER(LEN=expr)` retains its exact typed
source/semantic expression pair as a per-entity dependent type; and
runtime-length CHARACTER expression results carry a dedicated semantic category
rather than masquerading as deferred length (`:`). Function prefixes and typed
procedure declarations preserve their exact intrinsic, `TYPE`, `CLASS`,
`TYPE(*)`, or `CLASS(*)` result type-spec plus a non-interned source function
type instead of reconstructing that syntax from the semantic result. COMPLEX
KIND expressions are serialized only through their component type, their sole
structural owner. Explicit Fortran array-constructor type-specs remain attached
to their aggregate source owner independently of the constructor's semantic
array type. Readers reject unresolved dynamic lengths, source-only selector
metadata on semantic canonical types, and contradictory source/use-site
type-spec identities.

Schema 57 requires every `SgSourceFile` to identify the translation unit's
exact canonical target `size_t` type. C and C++ `sizeof` and `alignof`
expressions retain their frontend-provided result types, while detached
transformation builders must receive an explicit result type. Readers reject a
missing or noncanonical target-size contract instead of reconstructing it from
process-global language or address-size modes.

Schema 58 replaces the declaration-order `-1` sentinel with a required nullable
`translation_unit_source_order`: `null` means that no ordered token producer
exists, while a positive integer no greater than `UINT_MAX` is the producer's
exact final-expanded-token ordinal. It also requires every `SgOmpClause` to
carry a nullable
`combined_source_order`. Clauses attached to a combined directive receive one
unique nonnegative ordinal across all constituent directive clause lists;
ordinary clauses use `null`. Readers reject missing, negative, out-of-range,
duplicate, or partially reconstructed ordering state rather than inferring an
order from list grouping. OpenMP declare-simd and declare-variant targets are
reconstructed atomically from their exact declaration references and typed
`size_t` ordinals; unresolved or inconsistent target records are rejected.

Schema 59 requires every `SgInitializedName` to carry a typed
`enum_constant_source_ownership` role. Enum constants identify whether their
spelling belongs to the enum body, an included physical source, or a
semantic-only auxiliary declaration; all other initialized names use the
unclassified role. Every `SgEnumDeclaration` also records independently
whether its semantic `field_type` was written as an enum base. Readers reject
missing, out-of-range, or structurally inconsistent roles and reject a
source-spelled enum base without its exact semantic type.
Schema 114 also requires `enum_constant_value`: a signed 64-bit storage value
for enumerators (unsigned values preserve their bits), and `null` for other
initialized names. It stores the frontend's evaluated value independently of
whether an initializer was written. Readers bind it before publication and
reject disagreement with the enum role.
Schema 143 records each enum declaration's canonical type owner as a node ID.
The reader binds that typed identity before decoding dependent types.
Schema 144 removes the unused label `gnu_extension_unused` property and the
unused `SgVariantStatement` node.
Schema 145 removes the unused `SgBlockDataStatement`, `SgSequenceStatement`,
and `SgParameterStatement` nodes. Their removal changes subsequent numeric AST
variants; older checkpoints must be regenerated.
Schema 146 removes the unused `SgFortranNonblockedDo` node. Flang represents
non-block DO loops with `SgFortranDo` and its `has_end_statement` field. Older
checkpoints must be regenerated because numeric AST variants changed.
Schema 147 removes `SgFortranDo.old_style`, which had no producer or behavior
consumer. The `has_end_statement` and terminal-label edges retain the actual
block versus labeled-loop behavior. Older checkpoints must be regenerated.
Schema 148 removes the unproduced Rice `SgCAFWithTeamStatement` and
`SgTypeCAFTeam` nodes and their numeric AST variants. Older checkpoints must be
regenerated.

Schema 60 replaces the old implicit-cast-only classification with the complete
typed `SgCastExp` conversion contract. Every cast records its exact source cast
form, semantic conversion kind, value category, and ordered base-conversion
path. Readers reject unsupported cast forms, invalid enum values, unknown base
types, and combinations that do not satisfy the cast's semantic invariants
instead of reconstructing conversion meaning during unparsing or analysis.

Schema 61 requires every `SgInitializedName` to carry its typed
`preinitialization` role. Entries structurally owned by an
`SgCtorInitializerList` must identify a virtual base, nonvirtual base, data
member, or delegating constructor; every other initialized name must use the
unknown/non-preinitializer role. Readers reject missing, out-of-range, and
structurally inconsistent roles instead of reconstructing them from names or
types during unparsing.

Schema 62 preserves the complete typed Cray pointer relationship. Every
`SgInitializedName` records its nullable exact `cray_pointer_pointee` edge and
its nullable owned `fortran_cray_pointer_pointee_shape`. A shaped pointee must
link back to that exact source `SgVariableDeclaration` through
`fortran_separate_shape_declaration`, and its source rank must equal the
semantic array rank. Readers reject missing, crossed, or partially restored
pointer pairs instead of dropping or reconstructing pointee bounds during
unparsing. The schema also preserves the required `shape_deferred` bit for
source constructs whose shape is owned outside the ordinary type declaration.

Schema 79 requires both `is_parameter_pack` and `is_pack_element` booleans on
ordinary and external `SgInitializedName` records. Construction restores the
exact pack roles used by C++ emission. The parameter's display name and enclosing
template parameter record are not substitutes for either required field.
Variable declarations also record the required `is_constexpr` bit independently
of the explicit source type, including external variable declarations.

Schema 66 requires every `SgInitializedName` to carry its nullable exact
`fortran_separate_pointer_declaration` edge. An object whose semantic type has
a `POINTER` wrapper while its independently owned `fortran_source_type` does
not must link to the source `POINTER` attribute statement that owns exactly one
typed reference to that object. A deferred-shape pointer also links that same
statement through `fortran_separate_shape_declaration`. Readers reject missing,
crossed, duplicated, or semantic-only pointer ownership instead of recovering
the attribute or shape during unparsing.

Schema 67 requires every `SgSourceFile` to preserve `skip_unparse`. Compiler
module and other semantic input files therefore remain non-emitting after
checkpoint reconstruction instead of entering the backend file list as source
outputs.

Schema 68 preserves the exact source qualification of every base-type-specifier
through required `SgBaseClass` provenance, global-qualification, and ordered
qualifier-token fields. Reconstruction rejects qualifier components without
source provenance instead of recomputing a different name from auxiliary
semantic scopes during unparsing.

Schema 69 requires every class, function, and variable declaration to preserve
its specialization category. Reconstruction validates the complete
redeclaration family before creating declaration-owned types and again after
identity restoration, so a malformed specialization cannot collide with an
unrelated canonical type or be repaired during unparsing.

Schema 73 requires every coordinate-identified `SgTemplateType` to preserve
one canonical source identity.  The identity records the exact macro-aware
expansion and spelling coordinates of the owning template parameter; the
reader rejects a missing, invalid, or conflicting identity instead of merging
unrelated parameters that happen to share a name or semantic position.

External function declaration symbol bases belong to an isolated semantic
scope through `SgAuxiliaryDeclarationList`; they are never attached to a live
source scope. This keeps lookup ownership exact without making an external
declaration a structural child of a user source subtree.

## Determinism And Validation

Serialization is deterministic for the supported checkpoint surface. The
writer orders ordinary AST nodes by traversal and auxiliary nodes by stable
semantic keys. Symbol-table entries carry `lookup_preferred` state for Sage
lookup categories where duplicate names can exist. During reconstruction, the
deserializer groups entries by symbol table, entry name, and lookup category,
then orders each duplicate group so the serialized preferred entry is the one
returned by Sage lookup. The final validator re-runs the lookup checks and
fails hard if any group cannot be reconstructed exactly.

Every checkpoint performs this sequence:

1. serialize the active `SgSourceFile` to JSON;
2. parse the JSON and validate the format, schema version, checkpoint name,
   root kind, node table, and required fields;
3. reconstruct a fresh `SgSourceFile`;
4. replace the old file in the owning `SgProject`;
5. serialize the replacement and compare canonical semantic signatures.

Round-trip signatures compare the reconstructed semantic contract rather than
raw pointer values. They include node class, edge order, properties, symbol
targets, type shape, source positions, attributes, and OpenMP/OpenACC side
tables.

If validation fails, the tool writes diagnostic artifacts next to the checkpoint
file:

- the original JSON;
- the reconstructed JSON;
- the original canonical signature;
- the reconstructed canonical signature.

The compiler then fails hard.

## Ownership Notes

JSON parser state is ordinary local C++ data. `JsonValue` trees, node records,
edge records, and temporary lookup maps are owned by automatic objects or
standard containers and are destroyed when checkpoint processing exits.

Reconstructed Sage nodes are owned by normal Sage relationships, not by JSON
records. The deserializer allocates nodes, restores their child edges, parent
links, scopes, symbol tables, type links, and source-file lists, then installs
the reconstructed `SgSourceFile` into the existing `SgProject`. After
replacement, the old source file is detached from the active project file list
and later project traversals must operate on the replacement.

Symbols created while rebuilding symbol tables are owned by their
`SgSymbolTable`. The deserializer restores duplicate lookup preference by
constructing each duplicate group in deterministic lookup order and then
validating Sage's lookup API against the serialized `lookup_preferred` entry.

External declaration records restore references to runtime declarations,
modules, classes, and declaration peers that are required by the serialized
source file but live outside its structural subtree. These records are
structured identity contracts. External function symbol bases restore lookup
scope, but runtime placeholder declarations are not parented into active source
scopes, because doing so would make them structural children of the user source
and can make later lowering or copy code emit invalid declarations.

Checkpoint files are written to per-process temporary paths and published with
`rename`. On validation failure, diagnostic JSON and signature files are written
next to the checkpoint file; these artifacts are filesystem outputs, not AST
owners.

## Unsupported State

Unsupported required state must be added to the schema or rejected with a
diagnostic that names the node class and missing relationship. Do not serialize
required Sage relationships as unstructured text with the expectation that a
later pass will infer or repair them.

DOT graph output elsewhere in REX is useful for visual AST inspection, traversal
debugging, and parent/child-shape comparison. It is not this interchange format.
DOT output is diagnostic, address-oriented, filtered in several modes, has no
stable schema, and is not deserializable into a valid Sage AST.

## Schema history

Schema 81 records `project_files_id` explicitly (zero for standalone exports).
A source-file checkpoint snapshots every project input and its shared module
and type graph. Reconstruction replaces all input files together while keeping
the project and its file-list storage stable. Callers must reacquire other file
pointers after a checkpoint. The Flang registry is updated through exact node
identities; serialized module names do not select replacement declarations.

Schema 85 adds typed FORM TEAM statements, including required team number and
team variable edges and optional NEW_INDEX, STAT, and ERRMSG operands. Missing
required operands, duplicate scalar edges, and ACQUIRED_LOCK are rejected.

Schema 86 requires `is_modified` in node flags, including inline type-owned
expressions. Reconstruction publishes each node's recorded edit state at its
construction boundary. An explicit construction scope prevents fresh bindings
from becoming additional source edits; mutations of existing nodes and all
structural cache invalidations remain active. Older schemas are rejected.


Schema 87 stores SgFloatVal/SgDoubleVal numeric values in mandatory `value_bits`:
one/two unsigned 32-bit words, respectively, least-significant first. This is
IEEE binary32/binary64 data independent of host byte order. The separate
`value_string` preserves source spelling, including Fortran exponents and C++
digit separators, and may be empty for generated or edited values. Decoding
validates every word and never derives semantics from that spelling. Signed
zero, subnormals, infinities and NaN payloads survive checkpointing. Older
checkpoints must be regenerated.


Schema 88 removes the redundant composite spelling of `SgComplexVal`. Its
precision type and owned real/imaginary expressions carry its semantics; scalar
operands retain their own spellings. A component edit survives copying and JSON
restoration independently of the original literal text.

Schema 89 stores SgLongDoubleVal, SgFloat80Val and SgFloat128Val numeric values
as a required typed format and four least-significant-first 32-bit words in
`value_bits`. The exact source target format is independent of the host's long
double representation. Source spelling is optional syntax, never numeric input.
Readers reject unknown formats, malformed words, excess bits, noncanonical x87
encodings and mismatched fixed-width literal formats before constructing a node.

Schema 90 requires `expansion_is_modified` on macro expansion expressions.
This boolean preserves whether the typed expansion has changed since its source
spelling was bound. Construction flags are not used to infer this state. The
reader rejects missing or non-boolean values before allocating the macro node.

Schema 91 replaces UDL lexical value-expression lists with SgCxxLiteralTokenList
and SgCxxLiteralToken nodes. Tokens require token_kind (numeric, character, or
string) and nonempty spelling. Semantic call arguments remain independent.

Schema 92 requires source_user_defined_literal_is_modified on UDL calls so
restoring a transformed call cannot revive stale lexical tokens.

Schema 212 represents pseudo-objects with an owned written expression, ordered
semantic evaluations, an explicit result and typed opaque-value bindings and
references. Nested pseudo-objects explicitly capture enclosing bindings. Readers
reject references before their binding and inconsistent ownership. Schema 211
records must be regenerated with the current frontend.

Schema 211 stores each complete UDL lexical token, including its written suffix,
in SgCxxLiteralToken.spelling. The call's suffix identity remains normalized;
it must not be appended to lexical tokens during emission. Per-token suffix
roles remain explicit. This preserves UCN and UTF-8 suffix forms independently,
including mixed forms in concatenated strings. Schema 210 records must be
regenerated; readers do not reconstruct or migrate stripped token spellings.

### GNU typeof operand identity (schema 99)

`SgTypeOfType` stores exactly one JSON operand: `base_expression` for an
expression operand, or `base_type` for a type operand. Both fields together,
neither field, and an absent operand are rejected. The expression supplies its
exact semantic result type, including typed function-parameter references. No
second serialized type is reconstructed or compared after AST construction.
Schema 98 records must be regenerated; readers do not infer or discard operands.

### Typed fold operators (schema 100)

Fold expressions store `SgFoldExpression::operator_kind_enum`. JSON records require
`operator_kind`, a closed integer enum covering the C++ fold operators; unspecified
and out-of-range values are rejected before construction. `operator_token` string
payloads are no longer accepted. Operator spelling is derived from the enum for
unparsing and mangling. Files from previous schemas require regeneration.

### Removed CC++ this flag (schema 101)

The unused CC++ `pobj_this` field is removed from `SgThisExp`, its constructors,
and JSON records. Class/nonreal symbol identity and exact pointer type remain
required. Earlier-schema files require regeneration; the removed flag is not
translated into another attribute.

Schema 103 includes the initializer list in type-owned aggregate expressions.
Aggregate readers construct exact semantic/source types and owned children before
publishing the node; regenerate checkpoints written with earlier schemas.

Schema 104 includes exact operands for type-owned simple, compound and nested
requirements. Both readers construct these nodes only after their exclusive
operands are available. Older type-owned records omitted these dependencies and
must be regenerated; they are not filled in during restoration.

Schema 130 records zero-width bitfield and `[[no_unique_address]]` facts as
required typed `SgVariableDefinition` booleans.

Schema 131 removes `source_file_type_is_unknown`. No producer or consumer used
the corresponding `SgFile` field; older checkpoints must be regenerated.

Schema 132 removes the unused `fortran_implicit_none` file property. Older
checkpoints must be regenerated.

Schema 133 preserves the typed END ASSOCIATE label edge. Older checkpoints must
be regenerated.

Schema 134 removes redundant namespace-symbol name, alias-role, and alias-ID
metadata. The typed symbol basis and symbol-table entry key carry that
information; older checkpoints must be regenerated.

Schema 135 removes the unused Python-only `SgStmtDeclarationStatement` node.
Its removal changes subsequent numeric AST variants; older checkpoints must
be regenerated.
