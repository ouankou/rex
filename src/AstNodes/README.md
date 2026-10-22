# AST node implementations

`Expression/` contains hand-written C++ methods for Sage expression classes.
Its CMake source list compiles these methods into the REX library. These files
are ordinary C++; no Rosebud generator is used.

Class declarations, data members, traversal roles, and generated methods are
specified in `src/ROSETTA/src/` and `src/ROSETTA/Grammar/`. Edit those inputs when
changing the AST structure. The normal CMake build runs `rosetta_generated` and
compiles its output under the build tree's `src/frontend/SageIII/` directory.
Do not edit generated files in the build tree.

`src/ROSETTA/astNodeList` reserves stable numeric variant IDs. Historical names
for removed classes remain reserved there so removing an implementation does
not renumber unrelated nodes. A reserved name does not register a class or
provide its API; active classes are declared by the ROSETTA grammar.
