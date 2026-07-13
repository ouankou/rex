# Sage AST JSON Checkpoints

`astJson` is an opt-in REX tool for serializing one complete `SgSourceFile`
subtree to a deterministic JSON file, reading that file back, reconstructing a
fresh Sage AST, replacing the original file in the active `SgProject`, and
continuing the normal compiler path with the reconstructed file.

The tool is intentionally strict. If a Sage relationship needed by the active
checkpoint cannot be represented or reconstructed, serialization,
deserialization, or round-trip validation fails immediately. Later compiler
passes must not repair incomplete JSON.

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

Current files use schema version 74 and this top-level shape:

```json
{
  "format": "rex-sage-ast-json",
  "schema_version": 74,
  "root_id": 1,
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

`metadata` records the checkpoint name, source and output file names, command
line, the dense `Sg_File_Info` file-id table, and active source-file flags such
as OpenMP, OpenACC, skip-final-compile, language mode, backend format, and token
unparsing state.

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
coordinates, source sequence number, physical/source file-id relationships, and
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

Schema 25 added external function parameter-scope identity. A collected
`SgFunctionDeclaration` may have `functionParameterScope` owned by an external
first-nondefining or defining declaration peer. In that case, the generated
`functionParameterScope` edge is omitted only when an
`external_function_parameter_scope` record is present and validated. During
deserialization, the collected declaration's `functionParameterScope` is
restored from the named external peer.

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

## Validation Commands

Focused tests:

```text
ctest --test-dir build-ast-json -R '^ASTJSON_' --output-on-failure -j$(nproc)
```

Broad OpenMP/OpenACC/OpenMP-lowering gate with every reached checkpoint enabled:

```text
rm -rf build-ast-json/test-output/ast-json-broad &&
REX_AST_JSON_CHECKPOINT=all \
REX_AST_JSON_DIR=$PWD/build-ast-json/test-output/ast-json-broad \
ctest --test-dir build-ast-json \
  -L 'OMPTEST|OMPACCTEST|OMPLOWERING|ASTJSON' \
  --output-on-failure -j$(nproc)
```
