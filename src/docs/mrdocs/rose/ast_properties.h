// -*- c++ -*-

#ifndef ROSE_DOCS_ROSE_AST_PROPERTIES_H
#define ROSE_DOCS_ROSE_AST_PROPERTIES_H

/** @brief AST Properties (Consistency Tests)
 *
 * This documentation describes the properties of the AST. There are a number of
 * consistency tests of the AST, when all these pass the AST is verified to have
 * specifi properties. This documentation lays out these properties of the AST
 * (e.g. which pointers are always valid, etc.).
 *
 * @section section1 Traversal Tests
 *
 * This test verifies that the different types of traversal work properly on the
 * AST.
 *
 * @section section6 General AST Properties
 *
 * All IR nodes are tests and verified to name a number of specific properties.
 *
 * @subsection subsection6a Source Code Position
 * All SgLocatedNode and SgInitializedName IR nodes (and those derieved from
 * them) are verified to have a valid pointer to a Sg_File_Info object. Each IR
 * nodes has a Sg_File_Info object for the start of the language construct and
 * the end of the language construct. Note that for scope statements the start
 * and end positions identify the opening "{" and closing "}" were appropriate.
 * - All nodes have a vaild Sg_File_Info object for the front and end of the
 * subtree.
 * - All Sg_File_Info objects are uniques (never shared).
 *   This is important to support classification of IR nodes within
 * transformations (marked as transformed, shared, or compiler generated).
 * - Any nodes not passing this tests are recorded internall (output with
 * -rose-verbose >= 2)
 *
 * @subsection subsection6b SgFunctionCallExp Properties
 *
 * The SgFunctionCallExp is tested to verify that the get_function() member
 * function returns only a specific subset of SgExpressions:
 * - SgDotExp
 * - SgDotStarOp
 * - SgArrowExp
 * - SgArrowStarOp
 * - SgPointerDerefExp
 * - SgFunctionRefExp
 * - SgMemberFunctionRefExp
 *
 * The SgType returned from these SgExpression objects is also restricted to the
 * following subset of SgType objects:
 * - SgTypedefType
 *   This is the case of a function call from a pointer to a function
 * "(object->*(mFieldAccessorMethod))();"
 * - SgMemberFunctionType
 * - SgFunctionType
 *   These are the more common cases
 * - SgPointerMemberType
 *   This is an unusual case.
 *
 * Scopes are stored explicitly on some IR nodes (where the parent pointers in
 * the AST could provide incorrect evaluation of the scope. The following IR
 * nodes are tested to verify that their explicitly represented scope is a valid
 * pointer:
 * - SgInitializedName
 * - SgClassDeclaration
 * - SgTemplateInstantiationDecl
 * - SgFunctionDeclaration
 * - SgMemberFunctionDeclaration
 * - SgTemplateInstantiationFunctionDecl
 * - SgTemplateInstantiationMemberFunctionDecl
 * - SgTemplateDeclaration
 * - SgTypedefDeclaration
 *
 * @section section5 Template Properties
 *
 * Template within SAGE III are tested to verify specific properties:
 * - SgTemplateInstantiationDecl
 *   -# get_name() returns a valid C++ string object
 *   -# get_templateName() returns a valid C++ string object
 *   -# get_templateDeclaration() returns a valid pointer
 *   -# All template declarations within template instantiations are never
 * marked as compiler generated.
 * - SgTemplateInstantiationFunctionDecl
 *   -# get_name() returns a valid C++ string object
 *   -# get_templateName() can have an empty string
 *      This should be fixed at some point.
 *   -# get_templateDeclaration() returns a valid pointer
 *   -# All template declarations within template instantiations are never
 * marked as compiler generated.
 * - SgTemplateInstantiationMemberFunctionDecl
 *   -# get_name() returns a valid C++ string object
 *   -# get_templateName() can have an empty string
 *      This should be fixed at some point.
 *   -# get_templateDeclaration() returns a valid pointer
 *   -# Template declarations within template instantiations can be compiler
 * generated. Member functions of templated classes that are declared in the
 * class are represented outside the class as template specializations and are
 * marked as compiler generated.
 * - SgClassDefinition
 * - SgTemplateInstantiationDefn
 *   Base classes within class definitions and template instatiation definitions
 * are searched and verified to have properly reset template names (from
 * original legacy frontend names; see `resetTemplateNameTest`).
 * - All possible template instantiations
 *   -# These are tested to verify that get_specialization() returns a
 * non-default value to verify that some catagory of template specialization has
 * been specified (values verified to have been reset from the defaults).
 *   -# contain a valid pointer to a template declaration.
 * - All non-template instantiations
 *   -# Are maked with default value SgClassDeclaration::e_no_specialization.
 *
 * @section section4 Mangle Name Properties
 *
 * Mangled names within SAGE III follow specific rules and are tested:
 * -# May be used as variable names in C and C++
 *    This implies that they follow all the rules regarding variable naming
 * within C and C++ (not repeated here).
 * -# There are no legacy frontend generated name fragements from template
 * instantiation legacy frontend internally generates unique names (e.g
 * "foo____L1042") for template instatiations, we convert all such names and use
 * the new names of the form "foo<int>" before name mangling. Through name
 * mangling, longer names are generated of the form "foo__tas_int__tae", from
 * which the original template names and arguments can be recognised ("<"
 *    -> "__tas" and ">" -> "__tae", for template argument start (tas) and
 * template argument end (tae).
 *
 * @section section2 Unique Statements in Scope
 *
 * This test verifies each statement in a scope is unique. This catches rewrite
 * and general transformation errors that might insert a statement twice or
 * relocate in by forget to delete it. Since it only works within a single scope
 * it is not very robust.
 *
 * @section section2_unique Unique IR nodes in the AST
 *
 * This test verifies each IR nodes visited in the AST is only visited once.
 * This is a more robust version of the previous test which checked for shared
 * IR nodes in the same scope (which is a more common problem). This test is
 * more expensive in memory since it has to save a reference to ever IR node and
 * test if it has been previously seen.
 *
 * @section section3 Rules for Defining and Nondefining Declarations
 *
 * We separate defining and non-defining declarations so that many aspects of
 * analysis and transformation are simplified, along with code generation. For
 * example, if from any function declaration the function definition is sought,
 * it is available from the defining declaration (this applied uniformly to all
 * declarations). These tests verify that the handling of defining and
 * non-defining declaration follow specific rules (with a goal toward uniformity
 * and intuitive behavior).
 *
 * Nondefining declarations appear as forward declarations and references to
 * declarations within types. These many appear many times within the source
 * code and as a result are non unique within the AST. For example, each forward
 * declaration of a function or class within a source code becomes a
 * non-defining declaration.
 *
 * Defining declarations contain their definition, and function appearing with
 * its body (implementation) is a defined declaration containing a function
 * definition (the scope of the function). The defining declaration should
 * appear only once within the source code, by the One Time Definition rule,
 * (OTD). Each forward declaration, clearly becomes a separate declaration but
 * it may be shared as needed to reduce the total number of non-defining
 * declarations, which are also referenced in types.
 *
 * Within SAGE III, every declaration has a reference to its first non-defining
 * declaration and its defined declaration if it exists (is defined within the
 * current translation unit). If in processing a defining declaration an
 * reference is required, get_declaration() always returns the non-defined
 * declaration. The defined declaration is only available explicitly (via a
 * function call) and is never returned through any other mechanism. Thus
 * non-defining declarations are shared and defining declaration are never
 * shared within the AST.
 *
 * @subsection subsection3a When defining and non-defining declarations are the
 * same SgEnumDeclaration declarations are not allowed to forward reference
 * their definitions, this they are the same and the defining and non-defining
 * declaration for a SgEnumDeclaration are pointer values which are the same.
 *
 * @par Example
 * @code
 * assert(declaration->get_definingDeclaration() ==
 * declaration->get_firstNondefiningDeclaration());
 * @endcode
 *
 * @subsection subsection3b Scopes of defining and nondefining declarations
 * match (same pointer value) For all defining and nondefining declarations the
 * scopes are the same, however for those that are in namespaces the actual
 * SgNamespaceDefinition of a defining and non-defining declaration could be
 * different. To simplify analysis, the namespaces of defining and non-defining
 * declarations are set to the SgNamespaceDefinition of the defined declaration.
 * These test verify the equality of the pointers for all scopes of defining and
 * non-defining declarations.
 *
 * @par Example
 * @code
 * assert(declaration->get_definingDeclaration()->get_scope() ==
 *        declaration->get_firstNondefiningDeclaration()->get_scope());
 * @endcode
 *
 * @subsection subsection3c Defining and nondefining declarations are non-null
 * pointers which never match (different pointer values)
 *
 * The following SgDeclarationStatement IR nodes never share the same
 * declaration and are always valid (non-null) pointers.
 * -# SgAsmStmt
 *    This is a not well tested declaration within Sage III (but I think that
 * any declaration must be a defining declaration)
 * -# SgFunctionParameterList
 * -# SgCtorInitializerList
 *    These are special case declarations.
 * -# SgVariableDefinition
 *    A variable definition appears with a variable declaration, but a variable
 * declaration can be a forward reference to the variable declaration containing
 * the variable definitions (e.g. "extern int x;", is a forward declaration to
 * the declaration of "x").
 * -# SgPragmaDeclaration
 *    A pragam can contain no references to it and so it's declaration is also
 * it's definition
 * -# SgUsingDirectiveStatement
 * -# SgUsingDeclarationStatement
 * -# SgNamespaceAliasDeclarationStatement
 * -# SgTemplateInstantiationDirectiveStatement
 *    These can appear multiple times and are not really associated with
 * definitions (but for consistancy they are consired to be their own defining
 * declaration).
 * -# SgNamespaceDeclarationStatement
 *    Namespaces can't appear without their definitions (or so it seems, and it
 * is tested).
 *
 * @par Example
 * @code
 * assert(declaration->get_definingDeclaration() != NULL);
 * assert(declaration->get_firstNondefiningDeclaration() != NULL);
 * assert(declaration->get_definingDeclaration() !=
 * declaration->get_firstNondefiningDeclaration());
 * @endcode
 *
 * @subsection subsection3d Defining and nondefining declarations which never
 * match (non-defining declaration may be NULL)
 *
 * This case is similar to subsection3c but the non-defining declaration can be
 * a null values pointer. This is because a non-defining declaration may not
 * exist (as in the case of a function defined with its definition and without
 * any function prototype) The following cases are tested for this properly:
 *
 * -# SgVariableDeclaration
 *    This case is a bit special.
 * -# SgTemplateDeclaration
 * -# SgFunctionDeclaration
 *    The non-defining declaration is always a valid pointer.
 * -# SgClassDeclaration
 * -# SgTypedefDeclaration
 * -# SgMemberFunctionDeclaration
 * -# SgTemplateInstantiationFunctionDecl
 * -# SgTemplateInstantiationDecl
 * -# SgTemplateInstantiationMemberFunctionDecl
 *    These can have forward declarations separated from their definitions so a
 * declaration may be either a defining or non-defining declaration. All
 * declarations, except the defining declaration, are the same object as the
 * non-defining declaration if it is non-null.
 *
 * @par Example
 * @code
 * assert(declaration->get_definingDeclaration() != NULL);
 * assert(declaration->get_definingDeclaration() !=
 * declaration->get_firstNondefiningDeclaration());
 * @endcode
 */
struct AstProperties {};

#endif
