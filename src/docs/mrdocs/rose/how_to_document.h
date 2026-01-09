// -*- c++ -*-

#ifndef ROSE_DOCS_ROSE_HOW_TO_DOCUMENT_H
#define ROSE_DOCS_ROSE_HOW_TO_DOCUMENT_H

/** @brief Writing documentation
 *
 * How to write good API and non-API documentation in ROSE.
 *
 * This chapter is mainly for developers working on the ROSE library as opposed to users developing software that uses the
 * library. It specifies how we would like to have the ROSE library source code documented. The style enumerated here does not
 * necessarily need to be used for projects, tests, the tutorial, user-code, etc. Each item is also presented along with our
 * motivation for doing it this way.
 *
 * @todo The documentation style guide has not been reviewed by rose-core yet. [Robb P. Matzke 2015-05-15]
 *
 * ROSE uses MrDocs for two broad categories of documentation:
 *
 * - For documenting the ROSE API. MrDocs generates the structure of the documentation, and authors fill in the descriptions.
 * - For documenting non-API things that are nonetheless tied to a particular version of ROSE. An example is this page itself,
 *   which might change over time as ROSE evolves and which must go through ROSE's continuous integration testing and/or
 *   release testing.
 *
 * @section doc_quick Quick start
 *
 * Here's an example that documents a couple of closely-related class member functions. Things to note:
 *
 * - Use C-style block comments for documentation.
 * - First line (up to punctuation) is a summary -- the autobrief string that shows up in tables of contents.
 * - Use `&#64;ref` when referring to another class and `&#64;p` when mentioning a parameter.
 * - Use `&#64;{` and `&#64;}` to give the same documentation to both member functions.
 * - You can easily insert HTTP links into documentation.
 *
 * ```cpp
 * /** Most basic use of the partitioner.
 *  *
 *  *  This method does everything from parsing the command-line to generating an abstract syntax tree. If all is
 *  *  successful, then an abstract syntax tree is returned. The return value is a @ref SgAsmBlock node that contains all
 *  *  the detected functions. If the specimen consisted of an ELF or PE container then the parent nodes of the returned
 *  *  AST will lead eventually to an @ref SgProject node.
 *  *
 *  *  The command-line can be provided as a typical @c argc and @c argv pair, or as a vector of arguments. In the
 *  *  latter case, the vector should not include `argv[0]` or `argv[argc]` (which is always a null pointer).
 *  *
 *  *  The command-line supports a "--help" or ("-h") switch to describe all other switches and arguments, essentially
 *  *  generating output much like a Unix man(1) page.
 *  *
 *  *  The @p purpose should be a single line string that will be shown in the title of the man page and should not start
 *  *  with an upper-case letter, a hyphen, white space, or the name of the command. E.g., a disassembler tool might
 *  *  specify the purpose as "disassembles a binary specimen".
 *  *
 *  *  The @p description is a full, multi-line description written in the standard ROSE markup language where "@"
 *  *  characters have special meaning.
 *  *
 *  *  @{ *\/
 * SgAsmBlock* frontend(int argc, char *argv[],
 *                      const std::string &purpose, const std::string &description);
 * virtual SgAsmBlock* frontend(const std::vector<std::string> &args,
 *                              const std::string &purpose, const std::string &description);
 * /** @} *\/
 * ```
 *
 * @section doc_general General style
 *
 * Both categories of documentation (API and non-API) are written as comments in C++ source code and follow the same style
 * conventions.
 *
 * - **Comment style:** Whether to use block- or line-style comments is up to the author. However, authors are encouraged to
 *   use block style comments for documentation and line-style comments for non-documentation so that IDEs can easily highlight
 *   them differently. Furthermore, a vertical line of `*` down the left side of block comments has two useful benefits: it
 *   helps those developers that don't use syntax-highlighting IDEs to realize that lines are part of a comment, and it
 *   provides a hint that lines matched by `rg`-style searching are comments rather than code. Both become more important as
 *   the size of the block comment grows, especially if it contains lines that might look like code.
 * - **Use at-sign style:** The at-sign (`@`) style uses `@` rather than the backslash (`\`) to introduce directives. IDEs
 *   tend to have fewer problems recognizing the at-sign style due to its popularity and the fact that `@` is relatively
 *   uncommon in C++ code.
 * - **Explicit references:** Although MrDocs can automatically create cross references to any word that looks like a symbol,
 *   using an explicit `&#64;ref` will cause MrDocs to emit a warning if the referent's name changes and breaks the link. Our
 *   goal is to eventually fix all documentation warnings so that new warnings are easy to spot.
 * - **Capitalization:** Use the Wikipedia style of capitalization for pages, sections, and subsections. Namely, the first word
 *   is capitalized and all other words except proper names and abbreviations are lower-case. Titles do not end with
 *   punctuation.
 * - **"ROSE":** The name of this project is "ROSE", not "Rose" and not "rose". However, within the documentation itself it's
 *   seldom necessary to mention ROSE by name.
 *
 * @section doc_nonapi Documentation for non-API entities
 *
 * As mentioned, one of ROSE's uses of MrDocs is for documentation not related to any specific API element (such as this page
 * itself). This section intends to show how to document such things.
 *
 * **Pages or modules?** Non-API documentation is generally organized into pages and modules. Pages are relatively large
 * chapter-like things, while modules are smaller (usually) and hierarchical. The distinction is blurry though because both
 * support sections and subsections. Use this table to help decide:
 *
 * | Use pages | Use modules |
 * | --- | --- |
 * | Subject is important enough to be a chapter in a book? | Subject would be an appendix in a book? |
 * | Subject should be listed in the top-level table of contents? | Subject should be listed in some broader subject's page? |
 * | User would read the entire subject linearly? | User would jump around in the subject area? |
 * | Subject has two levels of nesting? | Subject has arbitrarily deep hierarchy? |
 * | Subject's sections should appear together in a single HTML page? | Subject's sections should each be on their own HTML page? |
 *
 * Pages and modules are represented by doc-only headers under `src/docs/mrdocs`. Each page or module is a dummy struct with a
 * documentation block. The first sentence is the auto-brief content that shows up in tables of contents. The auto-brief
 * sentence should fit on one line, end with a period, and should not be identical to the title; it should restate the title in
 * different words or else the table of contents looks awkward.
 *
 * Example page:
 *
 * ```cpp
 * /** @brief Getting started with source analysis
 *  *  Overview showing how to write source analysis tools.
 *  *\/
 * struct source_tutorial {};
 * ```
 *
 * Example module:
 *
 * ```cpp
 * /** @brief How to install Zlib
 *  *  Instructions for installing Zlib, a ROSE software dependency.
 *  *\/
 * struct installation_dependencies_zlib {};
 * ```
 *
 * **Location of documentation source?** Regardless of whether one chooses to write a page or a module, the documentation
 * needs to be placed in a C++ header. These files should live under `src/docs/mrdocs` and be added to `src/docs/mrdocs/doc_prelude.h`
 * so they are force-included during documentation builds. The MrDocs configuration file is `docs/mrdocs.yml`.
 *
 * @section doc_api Documentation for API entities
 *
 * The original purpose of API documentation is to describe the files, namespaces, classes, functions, and other types that
 * compose an API. MrDocs automatically generates the document structure from C++ declarations and the API author fills in
 * those things that cannot be done automatically, which is the majority of the text. The bullets below reference this
 * declaration:
 *
 * ```cpp
 * public: std::vector<std::string> splitString(const std::string &inputString, const std::string &separator);
 * ```
 *
 * - **Co-location:** The documentation comment should be adjacent to the thing it documents. Some people claim this
 *   unnecessarily clutters the header file and that the comment should be in a separate file, but the counter argument is that
 *   by having documentation near the declaration it is more likely to be updated if the declaration changes. Also, the
 *   cluttering-up claim is made moot by any reasonably capable IDE, especially if we separate API and implementation
 *   documentation by using C-style block comments for one and C++ line comments for the other.
 * - **Auto brief:** The documentation configuration is set up so that the first sentence of documentation gets used as the
 *   brief value without having to specify `&#64;brief`. The brief content should be concise. In particular, it should not start
 *   with "This function..." (since context provides that), it should easily fit on one line, it should not repeat information
 *   obvious from the declaration, and it should end with a period. Example: "Splits a string into substrings according to
 *   separator strings."
 * - **Public versus private:** Every public and protected part of the API must be documented. Documentation is as important as
 *   implementation; even so-called "self documenting" practices need additional human-written descriptions to make them useful
 *   to users that might not be familiar with a certain technique or algorithm. If some entity is not documented then it is not
 *   worthy of being a member of the API. Eventually we will disable the switches that allow stub documentation for
 *   non-documented parts of the API. The private things should not be documented because if someone's using these they need to
 *   be reading the source anyway.
 * - **Description:** All API entities must have a clear description except if the auto-brief statement together with the
 *   declaration entirely captures all details of interest to a user, which is seldom the case. The description should
 *   describe any pre and post conditions, what happens if an error is detected, and provide an example directly or indirectly
 *   if appropriate. Type information need not be repeated since it's already documented in the declaration. Example: "The
 *   `&#64;p inputString` is scanned to find each non-overlapping occurrence of the `&#64;p separator` string from left to right.
 *   The substrings between the identified separators are returned in the order they occur, including empty substrings. If no
 *   separator is found in the input string then only the input string is returned, even if it is empty."
 * - **Function parameters:** Function parameters need to be documented when their type and/or name is not sufficient. They
 *   can be documented in list format or as part of the function's description. Use the `&#64;p` formatting tag when referring
 *   to a parameter. It may work better to document related parameters in a descriptive paragraph than listing each one
 *   separately. If using a list, there's no need to include a parameter if the parameter is sufficiently documented in the
 *   main description or by the declaration; combine closely related parameters into a single item; attempt to minimize forward
 *   references by rewording or reordering.
 * - **To-do lists:** If you need to mark documentation that should be fixed, use the `&#64;todo` tag and include a
 *   description of what needs to be fixed. Also include your name (i.e., the person who thinks there's a problem) and the
 *   date.
 * - **Author name:** Do not insert your own name as the author. There are a number of reasons: first, we all work on all
 *   parts of ROSE to some degree and most of us would not be willing to remove another author's name if we make edits to the
 *   documentation even if that author is no longer with the ROSE team, which leads to the name eventually becoming inaccurate
 *   and/or misleading. Second, if some names are inaccurate then none of the names can be trusted. Finally, the question of
 *   who wrote what is answered better by the revision control system than by annotations in the source code.
 * - **Proofread:** Proofread your documentation in a web browser after MrDocs runs. One common error is for MrDocs to make a
 *   link to a capitalized word (like Function, at least as this is being written) that happens to also be an entity in the API.
 *   Prefix such words with a percent sign when the link is unintended. Likewise, authors should try to avoid class and
 *   namespace names that are also common words in order to prevent MrDocs from suddenly making those words links throughout
 *   all documentation. The documentation is generated by running `scripts/generate-api-documentation` or by invoking
 *   `scripts/build-docs` to build the full site.
 *
 * @section doc_astnodes Documentation for AST nodes
 *
 * AST nodes (e.g. `SgNode`) are a special case because they are generated by ROSETTA, a code generator. So we can't document
 * them inside the generated code. Instead we maintain documentation for AST nodes in doc-only headers and inject them into
 * the doc build. The source material lives in upstream ROSE and is surfaced through MrDocs via `src/docs/mrdocs/ast_node_docs.h`.
 *
 * @section doc_directives Documentation directives
 *
 * MrDocs understands a subset of HTML, its own command directives, and Markdown. The most useful are:
 *
 * - Sections and subsections are introduced by `&#64;section` or `&#64;subsection` followed by a unique identifier followed by
 *   the name of the section using Title capitalization described above.
 * - Function parameter names are indicated with `&#64;p` followed by the parameter name. This typesets them in a consistent
 *   style.
 * - References to other symbols, pages, and modules are indicated with `&#64;ref` followed by the name or ID. The look-up for
 *   symbols is similar to how the C++ compiler would look up the name, so you might need to qualify it with some namespace or
 *   even place the comment inside a namespace. If you don't want the qualified name cluttering the final HTML, follow the
 *   qualified name with a shorter name in double quotes (e.g., `&#64;ref InstructionSemantics::BaseSemantics::RiscOperators
 *   "RiscOperators"`).
 * - Code snippets are indicated by preceding the word with `&#64;c`. If more than one word or if it contains special
 *   characters, use backticks instead.
 * - To include an entire example source file into the documentation, use the `&#64;snippet` directive and add a snippet tag
 *   around the whole file. The `&#64;snippet` directive takes two arguments: the name of an example source file, and the name
 *   of a snippet. The beginning and end of the snippet is marked in the source file using comments of the form
 *   `//! [snippet name goes in here]`. Snippet names can include space characters.
 * - To document a namespace, class, function, variable, etc., place the documentation immediately prior to the declaration
 *   and use a block style comment that starts with two asterisks: `/**`. Alternatively, for variables and enum constants it's
 *   sometimes more convenient to put the documenting comment *after* the declaration, in which case the comment should start
 *   with `/**<`.
 *
 * @section doc_build Build documentation
 *
 * To generate ROSE's documentation locally, run:
 *
 * - `scripts/generate-api-documentation` (MrDocs only)
 * - `scripts/build-docs` (MrDocs + MkDocs site)
 *
 * @section doc_next Next steps
 *
 * MrDocs is documented at https://github.com/cppalliance/mrdocs.
 *
 * See @ref developer_docs.
 */
struct how_to_document {};

#endif
