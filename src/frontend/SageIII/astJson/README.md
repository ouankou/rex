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

Current files use schema version 27 and this top-level shape:

```json
{
  "format": "rex-sage-ast-json",
  "schema_version": 27,
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
- expression values, lvalue/parenthesization state, expression types, symbol
  references, operator syntax, and type syntax;
- `SgSourceFile::token_list` as explicit `SgToken` nodes, including token
  lexeme text and classification codes, because token-list entries are not
  guaranteed to appear through ordinary Sage subtree traversal;
- Fortran control-statement labels and end-statement flags needed for
  reconstructing blocked forms such as `DO WHILE` and `END DO`;
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
- OpenMP/OpenACC clause side tables needed by array sections, iterator clauses,
  mapper clauses, uses-allocators clauses, target-data policies, and lowering.

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

External function declaration symbol bases restore lookup `scope` but are not
parented into live source scopes. Parent assignment would make runtime
placeholder declarations structural children and can cause later lowering or
copy code to materialize invalid source declarations.

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
