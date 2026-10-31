# Native Markdown generator

Based on MrDocs **2026.9.29**'s
[Markdown example](https://github.com/cppalliance/mrdocs/tree/2026.9.29/examples/generators/data-driven/md/addons/generator/md),
under the included Apache-2.0 WITH LLVM-exception license.

MrDocs discovers this data-driven generator through `addons-supplemental`;
Markdown is not a built-in generator. Unmodified rendering falls back to the
upstream HTML/common partials. Local changes provide multipage titles and
explicit symbol/section anchors, escape C++ punctuation in Markdown tables,
and use inline HTML for linked code and signatures (links inside Markdown code
fences would be literal text).

`scripts/prepare-reference.py` creates category navigation, adds compact first
signatures to function tables, sets plain page titles, and resolves inline HTML
source links when renaming pages; Zensical handles directory URLs. Members
literally named `index` use `index-symbol/` to avoid overwriting their owning
class/namespace page. API leaves and code blocks use native search exclusions.
There is no AsciiDoc or format-conversion stage.
When upgrading MrDocs, compare this small template set with its upstream example
and run real generation plus the final-site link/anchor checks.

The build flow configures REX and builds only `rosetta_generated`, which provides
its AST headers; it does not compile `sage3` or the full compiler. `DOCS_JOBS`
and `MRDOCS_CONCURRENCY` each default to two and can be lowered on small machines.
Dependency namespaces remain names in signatures, not API pages. Hidden friends
are referenced rather than copied from dependency classes.
