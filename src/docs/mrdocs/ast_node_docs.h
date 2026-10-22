// -*- c++ -*-

#ifndef ROSE_DOCS_AST_NODE_DOCS_H
#define ROSE_DOCS_AST_NODE_DOCS_H

// Doc-only forward declarations for AST node classes and related types.
// These comments are attached to the classes for MrDocs.

// Flang's public builder interface mentions ENTRY statements before the
// generated Sage declarations are included by its implementation unit.
class SgEntryStatement;

/** @brief Class for traversing the AST.
 *
 */
class AstSimpleProcessing;

/** @brief RHS of an array variable declaration with optional assignment.
 *
 * This class represents the rhs of an array variable declaration which includes
 * an optional assignment (e.g. "int x[2] = {1,2};").  In this case the
 * SgExprListExp ("{1,2}") is wrapped in an SgAggregateInitializer.
 * - See also:
 * Example of using a SgAggregateInitializer object
 *
 * **Data members**
 *
 * #### `SgAggregateInitializer::p_initializers`
 * This points to a SgExprListExp (list of expressions).
 *
 */
class SgAggregateInitializer;

/** @brief This class represents the concept of a C Assembler statement.
 *
 * - Note: This is part of support for embedded programming, however the Linux
 * header files contain numerous example of asm statements demonstrating there
 * somewhat suprising wide-spread use.
 * - Internal: Test code for this include test2006_98.C and test2006_99.C.
 * - See also:
 * Example of using a SgAsmStmt object
 *
 * **Data members**
 *
 * #### `SgExpressionPtrList SgAsmStmt::p_operands`
 * The input and output operands, represented by SgAsmOp expressions.
 * The assembly template is stored separately in p_assemblyCode.
 *
 * #### `SgAsmStmt::AsmRegisterNameList SgAsmStmt::p_clobberRegisterList`
 * This is the clobber list (list of registers where side-effects happen).
 * This is a list of exact target-register strings supplied by the frontend.
 * Exact strings preserve register width, aliases, and non-x86 target names.
 *
 */
class SgAsmStmt;

/** @brief This class represents the rhs of a variable declaration which
 * includes an optional
 *
 * assignment (e.g. "int x = 1;").  In this case the SgValue ("1") is wrapped in
 * an SgAssignInitializer.
 * - See also:
 * Example of using a SgAssignInitializer object
 *
 * **Data members**
 *
 * #### `SgAssignInitializer::p_operand_i`
 * This points to the internal SgExpression (right-hand-side expression).
 *
 * #### `SgAssignInitializer::p_expression_type`
 * This points to the SgType of the rhs expression.
 *
 */
class SgAssignInitializer;

/** @brief This class represents the concept of a block (not a basic block from
 * control flow analysis).
 *
 * SgBasicBlocks are used in other IR nodes where a list of statements is
 * required.  However, if you need a list of statements we suggest you use an
 * STL list instead of borrowing and extending the semantics of SgBasicBlock.
 *
 * **Data members**
 *
 * #### `SgBasicBlock::p_statements`
 * This pointer an STL list of pointers to SgStatement objects.
 *
 */
class SgBasicBlock;

/** @brief This class represents the notion of a binary operator.
 *
 * It is derived from a SgExpression because operators are expressions.
 * - Internal: This is a base class for all binary operators.
 * - See also:
 * Example of using a SgBinaryOp object
 *
 * **Data members**
 *
 * #### `SgExpression* SgBinaryOp::p_lhs_operand_i`
 * This is the operand associated with the lhs of the binary operator.
 * Every binary operator is applied to two operands, this
 * variable stores the lhs operand to which the binary operator is applied
 * (with the rhs).
 *
 * #### `SgExpression* SgBinaryOp::p_rhs_operand_i`
 * This is the operand associated with the rhs of the binary operator.
 * Every binary operator is applied to two operands, this
 * variable stores the rhs operand to which the binary operator is applied
 * (with the lhs).
 *
 * #### `SgBinaryOp::p_expression_type`
 * The semantic result type of the operator.
 *
 */
class SgBinaryOp;

/** @brief This class represents a boolean value (expression value).
 *
 * - See also:
 * Example of using a SgBoolValExp object
 *
 * **Data members**
 *
 * #### `SgBoolValExp::p_value`
 * The boolean literal's value.
 *
 */
class SgBoolValExp;

/** @brief This class represents the notion of a break statement
 *
 * (typically used in a switch statment).
 * This class is used to exit from inside of a jump to the outer
 * block.  It is a specific feature of the C language.  It is typically used to
 * break out of a basic clock or a loop. The break statement is important to
 * the specification of the control flow within a program.
 * The SgBreakStmt is derived from a SgStatement and does not have any
 * data members.
 * - See also:
 * Example of creating a SgBreakStmt object \n
 * Example of using a SgBreakStmt object
 *
 */
class SgBreakStmt;

/** @brief This class represents the concept of a generic call expression.
 *
 * This IR node is intended for use with languages that allow calls of arbitary
 * expressions, even when the called expression does not resolve to a function
 * type. For statically-typed languages, use SgFunctionCallExp. Note that a
 * function call is assembled from a function expression and arguments. The two
 * are bound together in a function call, function expressions are never called
 * directly (the function call argument list can be empty).
 * - Internal: When overloaded operators are used the function name becomes the
 * name of the operator (e.g. "operator+").  In these cases the precedence of
 * the operator is that of the operator being overloaded and is different from a
 * normal function call.  Most of this detail is hidden since operands in the
 * expression tree are represented by functions with parameter lists
 * (represented by a SgExprListExp), and not simple expression trees.
 *
 * **Data members**
 *
 * #### `SgCallExpression::p_function`
 * This pointer points to the expression being assembled with argments for be a
 * function call.
 * Note that this is not always a SgFunctionRefExp or a SgMemberFunctionRefExp
 * and can be quite complex where pointers are referenced and function calls
 * assembled using pointers to functions and member functions. This member is
 * named 'p_function' for historical reasons, but it can point to any arbitrary
 * expression.
 * - Internal: Derived classes may restrict the list of acceptable types of IR
 * nodes here. They are listed and tested in the AST consistency tests (see
 * src/midend/astDiagnostics/AstConsistencyTests.C for details).
 *
 * #### `SgCallExpression::p_args`
 * This is a pointer to a SgExprListExp (list of function arguments)
 *
 * #### `SgCallExpression::p_expression_type`
 * This is the type of the return value of the function.
 *
 * **Member functions**
 *
 * #### `SgCallExpression::get_type()`
 * Returns the exact semantic result type stored by the producer. Missing,
 * placeholder, and callable types are rejected.
 *
 */
class SgCallExpression;

/** @brief This class represents the concept of a C and C++ case option (used
 * within a switch statement).
 *
 * - See also:
 * Example of using a SgCaseOptionStmt object
 *
 * **Data members**
 *
 * #### `SgCaseOptionStmt::p_key_range_end`
 * This pointer points to the last constant in the range when a gnu range
 * case label is used.
 * - Internal: We can't represent the range a ... b as an expression in C or C++
 * so we need to explicitly store the start and end of the range where it is
 * used as a case label. The p_key is the start of the range.
 *
 * #### `SgCaseOptionStmt::p_body`
 * This pointer points to a SgStatement object.
 *
 * **Member functions**
 *
 * #### `SgCaseOptionStmt::get_key() const`
 * Returns the case label expression.
 * - Internal: This should always be a valid pointer.
 * - Return: Returns pointer to SgExpression.
 *
 * #### `SgCaseOptionStmt::set_key(SgExpression* key)`
 * Sets the case label expression.
 * - Return: Returns void.
 *
 * #### `SgCaseOptionStmt::get_body() const`
 * Returns pointer to a SgStatement object.
 * - Internal: This should always be a valid pointer.
 * - Return: Returns pointer to SgStatement.
 *
 * #### `SgCaseOptionStmt::set_body(SgStatement* body)`
 * Access function for p_body.
 * - Return: Returns void.
 */
class SgCaseOptionStmt;

/** @brief This class represents a cast of one type to another.
 *
 * SgCastExp is used across multiple languages. For C++ it represents all casts,
 * including C-style casts: (newtype) and C++ style: reinterpret_cast<>(),
 * static_cast<>(), etc.
 *
 * **Member functions**
 *
 * #### `SgCastExp::get_operand ()`
 * Returns the target of the cast.
 * This functions returns an SgExpression pointer representing the expression
 * being casted.
 * - Return: Returns valid pointer to SgExpression.
 */
class SgCastExp;

/** @brief This class represents the concept of a catch within a try-catch
 * construct used in
 *
 * C++ exception handling.
 * - Internal: try and catch, though linked semanticaly, are seperate statements
 * in the IR.
 *
 * **Data members**
 *
 * #### `SgCatchOptionStmt::p_condition`
 * This pointer to a SgVariableDeclaration.
 *
 * #### `SgCatchOptionStmt::p_body`
 * This pointer to a SgStatement used to hold the statements to be executed
 * when the exception is caught.
 *
 * #### `SgCatchOptionStmt::p_trystmt`
 * This pointer to a SgTryStmt (a declaration) that is associated with this
 * catch option.
 *
 * **Member functions**
 *
 * #### `SgCatchOptionStmt::copy(const SgCopyHelp & help)`
 * Makes a copy (deap of shallow depending on SgCopyHelp).
 * - Return: Returns pointer to copy of SgCatchOptionStmt.
 *
 * #### `SgCatchOptionStmt::get_condition() const`
 * Access function for p_condition.
 * - Return: Returns a pointer to a SgVariableDeclaration.
 *
 * #### `SgCatchOptionStmt::set_condition(SgVariableDeclaration* condition)`
 * Access function for p_condition.
 * - Param `condition`: SgVariableDeclaration pointer
 * - Return: Returns void.
 *
 * #### `SgCatchOptionStmt::get_body() const`
 * Access function for p_body.
 * - Return: Returns a pointer to a SgStatement.
 *
 * #### `SgCatchOptionStmt::set_body(SgStatement* body)`
 * Access function for p_body.
 * - Param `body`: SgStatement pointer
 * - Return: Returns void.
 *
 * #### `SgCatchOptionStmt::get_trystmt() const`
 * Access function for p_trystmt.
 * - Return: Returns a pointer to a SgTryStmt.
 *
 * #### `SgCatchOptionStmt::set_trystmt(SgTryStmt* trystmt)`
 * Access function for p_trystmt.
 * - Param `trystmt`: SgTryStmt pointer
 * - Return: Returns void.
 */
class SgCatchOptionStmt;

/** @brief This class represents the concept of a C++ sequence of catch
 * statements.
 *
 * This is associated with a try statement.
 * - See also:
 * Example of using a SgCatchStatementSeq object
 *
 * **Data members**
 *
 * #### `SgCatchStatementSeq::p_catch_statement_seq`
 * This is an STL list of pointers to SgStatement objects.
 *
 * **Member functions**
 *
 * #### `SgCatchStatementSeq::get_catch_statement_seq() const`
 * Access function for p_catch_statement_seq.
 * - Return: Returns const reference to SgStatementPtrList.
 *
 * #### `SgCatchStatementSeq::get_catch_statement_seq()`
 * Access function for p_catch_statement_seq.
 * - Return: Returns non-const reference to SgStatementPtrList.
 */
class SgCatchStatementSeq;

/** @brief This class represents the concept of a class declaration statement.
 * It includes the
 *
 * concept of an instantiated class template as well.
 * - See also:
 * Example of using a SgClassDeclaration object
 *
 * **Data members**
 *
 * #### `SgClassDeclaration::p_name`
 * This is the name of the class or instantiated class template
 * - Internal: The value held is not shared within the Sage III AST.
 *
 * #### `SgClassDeclaration::p_class_type`
 * Enum value classifying this as a class,struct,or union.
 * - Internal: The enum type is also defined in this class.
 *
 * #### `SgClassDeclaration::p_type`
 * This is the type used in the declaration (SgClassType).
 *
 * #### `SgClassDeclaration::p_definition`
 * This is the class definition (alway a valid pointer, except for explicitly
 * marked forward declarations). When this is a defining or nondefining
 * declartion the p_definition pointer is always valid.  However, as a separate
 * rule, all forward declarations are both makred explicit as forward
 * declarations AND have a NULL value for their p_definition pointer.
 * - Internal: The declaration associated with the definition is the defining
 * declaration and may not be the same as that pointed to by the "this" pointer.
 *
 * #### `SgClassDeclaration::p_from_template`
 * This records if the class declaration is associated with a template.
 * This boolean value is set to true if this is a non-template class nested
 * in a templated class.  We have an explicit IR node for template classes, but
 * this is only flag to indicate that several template specific rules apply to
 * the declaration of this class if the value is true.  For example, the class
 * will not be output unless it is transformed, then the required specialization
 * is required for the parent templated class.
 * - Internal: It is set internally and there is no need for it to be set by the
 * user!
 *
 * **Member functions**
 *
 * #### `SgClassDeclaration::SgClassDeclaration ( Sg_File_Info* startOfConstruct
 * = NULL,` SgName name = SgdefaultName, int class_type = 0, SgClassType
 * *type=NULL, SgClassDefinition *definition=NULL) This is the only constructor.
 * This constructor builds the SgClassDeclaration base class. but has some
 * specific side-effects (some of which have been removed in the latest work).
 * It is however still that case that if the definition is provided then it's
 * declaration will be changed to the declaration being constructed (a warning
 * message it output if this happens and this side-effect will be removed soon).
 * - Param `startOfConstruct`: represents the position in the source code
 * - See also:
 * Example:create an SgClassDeclaration object
 *
 */
class SgClassDeclaration;

/** @brief This class represents the concept of a class definition in C++.
 *
 * Class definitions are distinquished from class declaration by the
 * definition of the member data and specification of inheritance (if any).
 * Where class declarations can appear multiple times withouth a class
 * definition (forward class declarations), the class definition may appear only
 * once in an application. Within C++ there is the "One Time Definition Rule"
 * which requires that within multiple compilation usints (typically separately
 * compiled files), the definition of all classes of the name qualified name
 * must be identical.  Thus the class is considered to be defined only once.
 * - Internal: This is a base class for the SgTemplateInstantiationDefn
 * statements.
 *
 * **Data members**
 *
 * #### `SgClassDefinition::p_members`
 * This the list of member declarations in the class.
 *
 * #### `SgClassDefinition::p_inheritances`
 * This the list of base classes specificed in the class definition.
 * - Internal: The order of base classes can be important.
 *
 * **Member functions**
 *
 * #### `SgClassDefinition::copy(const SgCopyHelp & help)`
 * Makes a copy (deap of shallow depending on SgCopyHelp).
 * - Return: Returns pointer to copy of SgClassDefinition.
 *
 * #### `SgClassDefinition::get_members()`
 * Returns a list to the data member declarations.
 * - Return: Returns an STL list by reference.
 *
 * #### `SgClassDefinition::get_members() const`
 * Returns a const list to the data member declarations.
 * - Return: Returns a const STL list by reference.
 *
 * #### `SgClassDefinition::get_inheritances()`
 * Returns a list to the base classes.
 * - Return: Returns an STL list by reference.
 *
 * #### `SgClassDefinition::get_inheritances() const`
 * Returns a const list to the base classes.
 * - Return: Returns a const STL list by reference.
 *
 * #### `SgClassDefinition::get_qualified_name() const`
 * Returns SgName (a string) representing the name of the current scope.
 * See discussion of qualified names in the documentation.
 * - Return: Returns SgName (a string).
 *
 * #### `SgClassDefinition::get_declaration()`
 * returns the class declaration associated with this class decinition.
 * - Return: Returns SgClassDeclaration pointer.
 */
class SgClassDefinition;

/** @brief This class represents the concept of a C++ expression built from a
 * class name.
 *
 * This legacy reference node is retained for AST compatibility. Template
 * declarations themselves are represented structurally by typed declaration,
 * parameter, argument, constraint, and definition nodes.
 * - Todo: Need to figure out some examples of whare this is used.
 * - See also:
 * Example of using a SgClassNameRefExp object
 *
 * **Data members**
 *
 * #### `SgClassNameRefExp::p_symbol`
 * This pointer points to a SgClassSymbol.
 *
 * **Member functions**
 *
 * #### `SgClassNameRefExp::get_symbol() const`
 * Returns pointer to SgSymbol.
 * - Return: Returns pointer to SgSymbol.
 *
 * #### `SgClassNameRefExp::set_symbol(SgSymbol* symbol)`
 * Access function for p_symbol.
 * - Return: Returns void.
 */
class SgClassNameRefExp;

/** @brief This class represents the concept of a class name within the
 * compiler.
 *
 * Symbols are a simpler way for the compiler to quickly associate types,
 * declarations and names.
 * Indepedent of the different kinds of declarations (declarations are
 * statements), declarations can be considered to be definind and non-defining.
 * See SgDeclarationStatement for details.  where there exist defining and
 * non-defining declarations symbols within Sage III always reference
 * non-defining declarations (only non-defining declarations are shared within
 * the AST).
 * - See also: SgDeclarationStatement
 * - See also:
 * Example of using a SgClassSymbol object
 *
 * **Member functions**
 *
 * #### `SgClassSymbol::get_name() const`
 * Access function for getting name from declarations or types internally.
 * - Internal: This is a virtual function.
 * - Return: Returns SgName.
 *
 * #### `SgClassSymbol::get_type() const`
 * This function returns the type associated with the named entity.
 * - Internal: This is a virtual function.
 * - Return: Returns SgType*.
 *
 */
class SgClassSymbol;

/** @brief Marks the start of a linkage specification region. */
class SgClinkageStartStatement;

/** @brief This class represents the concept of a C trinary conditional
 * expression (e.g. "test ? true : false")
 *
 * - See also:
 * Example of using a SgConditionalExp object
 *
 * **Data members**
 *
 * #### `SgConditionalExp::p_conditional_exp`
 * This pointer points to a SgExpression object.
 *
 * #### `SgConditionalExp::p_true_exp`
 * This pointer points to a SgExpression object.
 *
 * #### `SgConditionalExp::p_false_exp`
 * This pointer points to a SgExpression object.
 *
 * #### `SgConditionalExp::p_expression_type`
 * This pointer points to a SgType object.
 *
 * **Member functions**
 *
 * #### `SgConditionalExp::get_conditional_exp() const`
 * Access function for p_conditional_exp.
 * - Return: Returns pointer to SgExpression.
 *
 * #### `SgConditionalExp::set_conditional_exp(SgExpression* conditional_exp)`
 * Access function for p_conditional_exp.
 * - Param `conditional_exp`: is the conditional test expression
 * - Return: Returns void.
 *
 * #### `SgConditionalExp::get_true_exp() const`
 * Access function for p_true_exp.
 * - Return: Returns pointer to SgExpression.
 *
 * #### `SgConditionalExp::set_true_exp(SgExpression* true_exp)`
 * Access function for p_true_exp.
 * - Param `true_exp`: expression to evaluate if test is true.
 * - Return: Returns void.
 *
 * #### `SgConditionalExp::get_false_exp() const`
 * Access function for p_false_exp.
 * - Return: Returns pointer to SgExpression.
 *
 * #### `SgConditionalExp::set_false_exp(SgExpression* false_exp)`
 * Access function for p_false_exp.
 * - Param `false_exp`: expression to evaluate if test is false.
 * - Return: Returns void.
 *
 * #### `SgConditionalExp::get_expression_type() const`
 * Access function for p_expression_type.
 * - Return: Returns pointer to SgType.
 *
 * #### `SgConditionalExp::set_expression_type(SgType* expression_type)`
 * Access function for p_expression_type.
 * Both the p_true_exp and p_false_exp must have the same type.
 * - Param `expression_type`: type of result.
 * - Return: Returns void.
 */
class SgConditionalExp;

/** @brief This class represents the call of a class constructor to initialize a
 * variable.
 *
 * For example "Foo foo;" or "Bar bar(1,2,3);" to initialize foo and bar
 * respecitively. In both cases it holds the function declaration of the call
 * constructor.  It also holds an SgExprListExp that represents the arguments to
 * the constructor.  The list is empty if there are no arguments.
 * - Internal: This is a base class for all initializers.
 * - See also:
 * Example of using a SgConstructorInitializer object
 *
 * **Data members**
 *
 * #### `SgConstructorInitializer::p_declaration`
 * This points to the associated member function declaration (a constructor).
 * Note that this pointer can be NULL if no such constructor is defined (e.g.
 * compiler generated default constructor).
 * - Internal: Constructors not explicit in the source code are not presently
 * generated in ROSE if there were to be generated in the future they will be
 * marked as compiler generated and likely we can then enforce that this be a
 * valid pointer.  This variable is an exception to the general rule that we
 * have no NULL pointes within the AST.
 *
 * #### `SgConstructorInitializer::p_args`
 * This points to the argument list of the associated constructor call.
 * - Internal: Note that this pointer is always a valid pointer, even if the
 * constructor and class declaration pointers are NULL.  This list can also be
 * empty, but the poitner to the list is always valid.
 *
 * #### `SgConstructorInitializer::p_expression_type`
 * This points to the associated type for this constructor.
 * - Internal: This pointer should always be valid,
 * "p_associated_class_unknown" is true iff "p_expression_type" is not a
 * SgClassType.
 *
 * #### `SgConstructorInitializer::p_need_name`
 * This bool value controls the output of the class name.
 * This might be somewhat redundant with the explicit vs. implement
 * representation in the source code (recorded in another flag of the
 * SgExpression base class).
 *
 * #### `SgConstructorInitializer::p_need_qualifier`
 * This bool value controls the output of the class names qualifier.
 * - Internal: The qualifier is currently always output. This variable is not
 * used.
 *
 * #### `SgConstructorInitializer::p_need_parenthesis_after_name`
 * This bool value controls the output of "()" after the class name.
 * The controls the output at "class X x;" or "class X x();".
 *
 * #### `SgConstructorInitializer::p_associated_class_unknown`
 * This bool value indicates when p_declaration should be a valid pointer.
 * If true, then p_declaration is NULL, else if false, then p_declaration should
 * be a valid pointer.
 * - Todo: Enforce p_declaration is valid pointer when
 * p_associated_class_unknown is false within AST consistency tests.
 *
 * **Member functions**
 *
 * #### `SgConstructorInitializer::get_need_name() const`
 * returns bool value if name is required in code generation then return true.
 * - Return: Returns bool value.
 *
 * #### `SgConstructorInitializer::set_need_name (bool required)`
 * This function allows the p_need_name flag to be set (used internally).
 * - Param `required`: - sets value of internal p_need_name flag.
 * - Return: Returns void.
 *
 * #### `SgConstructorInitializer::set_expression_type (SgType* type)`
 * Access function for p_expression_type.
 * - Param `required`: - sets value of internal p_expression_type.
 * - Return: Returns void.
 *
 * #### `SgConstructorInitializer::get_expression_type() const`
 * Access function for p_expression_type, returns pointer to SgType associated
 * with constructor. This is usually a SgClassType, but it can also be a
 * primative type (typically in association with a new operator).
 * - Return: Returns pointer to SgType.
 */
class SgConstructorInitializer;

/** @brief This class represents the concept of a C or C++ continue statement.
 *
 * - See also:
 * Example of using a SgContinueStmt object
 *
 */
class SgContinueStmt;

/** @brief This class represents the concept of a contructor initializer list
 * (used in
 *
 * constructor (member function) definitions).
 * Constructors are simply member functions with a name matching the class name
 * (or instantiated template name).  Constructor initialization lists (also
 * called preinitialization lists) are used only within the member function
 * definition (never in the declaration).
 * - Internal: It is presently used in the SgMemberFunctionDeclaration
 * but it might be that it would be better placed into the SgFunctionDefinition.
 * this would at least be similar to the handling of the base class list (stored
 * in the SgClassDefinition and not the SgClassDeclaration).
 * - See also:
 * Example of using a SgCtorInitializerList object
 *
 * **Data members**
 *
 * #### `SgCtorInitializerList::p_ctors`
 * This is an STL list of pointers to SgInitializedName objects.
 *
 * **Member functions**
 *
 * #### `SgCtorInitializerList::get_ctors()`
 * Returns STL list of pointers to SgInitializedName objects.
 * - Return: Returns SgInitializedNamePtrList (STL list) by reference.
 */
class SgCtorInitializerList;

/** @brief This class represents modifiers for SgDeclaration (declaration
 * statements).
 *
 * - See also:
 * Example of using a SgDeclarationModifier object
 *
 * **Data members**
 *
 * #### `SgBitVector    SgDeclarationModifier::p_modifierVector`
 * Bit vector permitting specification of flags for friend, typedef, export,
 * throw.
 *
 * #### `SgTypeModifier    SgDeclarationModifier::p_typeModifier`
 * Modifier for type information.
 *
 * #### `SgAccessModifier    SgDeclarationModifier::p_accessModifier`
 * Modifier for access information (only set for declarations in class and
 * struct definitions)
 *
 * #### `SgStorageModifier    SgDeclarationModifier::p_storageModifier`
 * Modified for storage information.
 *
 */
class SgDeclarationModifier;

/** @brief This class represents the concept of a declaration statement.
 *
 * Declaration statements are where variables and types are
 * defined and become available for use within a program.  Declarations
 * are strored in symbols (SgSymbol) and used to associate
 * the symbol with a location within a scope within the application
 * source code.  There are many different types of declarations, within Sage III
 * the SgDeclaration forms a base class for numerous IR nodes such as:
 * class declarations (including structs and unions), enum declarations,
 * function declarations, namespace declarations, pragma declarations,
 * template declarations, typedef declarations, using declarations,
 * using directives, variable declarations, etc.
 * A declaration family records its canonical first nondefining declaration
 * and its defining declaration, when one exists. Multiple nondefining
 * declarations can refer to the same entity.
 * - Todo: Current issues to look at:
 * - Current classes derived from SgDeclarationStatement and the
 * SgInitializedName class have ways of generating
 * names, qualified names, and mangled names, but we lack a uniform mechanism to
 * generate mangled qualified names.  The implementation is not uniform within
 * the IR and this is a problem for applications which must generate unique
 * names for C++ constructs.  Specific to functions names:
 * - We have:
 * -# get_name()
 * -# get_qualified_name()
 * -# get_mangled_name()
 * - While we need (on ALL SgDeclarationStatement IR nodes):
 * -# get_name()
 * -# get_qualified_name()
 * -# get_mangled_name()
 * -# get_qualified_mangled_name()
 * - It has been suggested that we eliminate get_qualified_mangled_name() and
 * have get_mangled_name() return a mangled qualified name.  Thus the semantics
 * of get_mangled_name() would be changed to generate a mangled qualified name.
 * The purpose of get_mangled_name is to generate a unique name for all
 * declarations. Note that some declarations should not return a unique name:
 * -# SgAsmStmt
 * -# SgCtorInitializerList
 * -# SgFunctionParameterList
 * -# SgUsingDeclarationStatement
 * -# SgVariableDefinition \\
 * It may be that this should be the same as for the SgVariableDeclaration.
 * - Related to the above problem we wish to provide a definition under which
 * all declarations (SgDeclarationStatement IR nodes) can be defined to be
 * unique. Under One-time Definition Rule (ODR) definitions must be the same
 * within a linked application (in each compilation unit).  We define a unique
 * name for a declaration IR node as:
 * - Any declaration which matches another declaration under ODR must generate
 * the same unique string.
 * - Any two declarations that are different must generate different unique
 * strings.
 * - Important cases are:
 * - The IR nodes for which there is an obvious mangled name (and a
 * get_mangled_name() exists) are:
 * - SgClassDeclaration
 * - SgFunctionDeclaration
 * - SgEnumDeclaration
 * - SgNamespaceDeclarationStatement
 * - SgTypedefDeclaration
 * - IR nodes for which it is less obvious (or for which there is no present
 * get_mangled_name() member function) are:
 * - SgAsmStmt
 * - SgCtorInitializerList
 * - SgFunctionParameterList
 * - SgNamespaceAliasDeclarationStatement
 * - SgPragmaDeclaration
 * - SgTemplateDeclaration (can this have a mangled name?)
 * - SgTemplateInstantiationDirectiveStatement
 * - SgUsingDeclarationStatement
 * - SgUsingDirectiveStatement
 * - SgVariableDeclaration
 * - SgVariableDefinition
 * - Other IR nodes (not derived from SgDeclarationStatement) but for which we
 * need to generate mangled names (and a get_mangled_name() member functions
 * exists) are:
 * - SgTemplateArgument
 * - SgFunctionDefinition
 * - SgNamespaceDefinitionStatement
 * - SgClassDefinition
 * - SgType (and all IR nodes derived from SgType)
 * - Other IR nodes (not derived from SgDeclarationStatement) but for which I
 * think we need to generate mangled names (and a get_mangled_name() member
 * functions does NOT exist) are:
 * - SgInitializedName
 * - SgFile (this might not really be required, but it might make sense)
 * - SgDirectory (this might not really be required, but it might make sense)
 * - SgBaseClass (this is questionable)
 * - Class declarations often point to their definition independently of if they
 * are defining or non-defining declaration (true at least for
 * SgTemplateInstantiationDecl objects).  This is different from how function
 * declarations are handled, where only the defining declaration has a valid
 * pointer to the function definition.  This should be made more consistant.
 * There are more comments about this is the documentation for
 * SgTemplateInstantiationDecl.
 * - The specification of "extern" is not the same as extern "C", one is handled
 * in the SgDeclarationStatement (extern "C") and the other is handled in the
 * SgStorageModifier (extern).  It is confusing that the two are so far a part.
 * Perhaps they could be together so that setting "C" linkage could declare a
 * SgDeclarationStatement as "extern" at the same time.  It makes no sense to
 * have something marked as "C" linkage and have it not be extern (I think); so
 * this could be considered an error (I think).
 * - To support debugging of applications using the mangled names to build
 * unique strings we should provide a global map between the unique names and
 * the associated declarations.  This would avoid the creation of alternative
 * name mangling mechanisms whose sole purpose it to fold into the mangled name
 * debugging information.
 * - Test: To test the mangled name generation we should construct a program
 * using the mangled names and compile it to verify uniquenss of the veriable
 * names.  Since functions of the same name can be repeated, mangled names from
 * functions should generate functions in the test program and mangled names of
 * veriables should generate variables, etc.
 * - See also:
 * Example of using a SgDeclarationStatement object
 *
 * **Data members**
 *
 * #### `SgDeclarationStatement::p_definingDeclaration`
 * This pointer is valid only if a defining declaration exists, and stores the
 * defining declaration where the declaration was defined. It is important to
 * note that this is the only defining declaration as opposed to the
 * non-defining declarations (where there can, depending on IR node, often be
 * many).
 * - Internal: The value held is not shared within the Sage III AST (except
 * though this data member in each defining and non-defining declaration).
 *
 * #### `SgDeclarationStatement::p_firstNondefiningDeclaration`
 * This pointer is valid if there is a non-defining declaration, and stores the
 * first non-defining declaration where a declaration was made explicitly (as in
 * forward declaration) or implicitly as in the case of a few IR nodes (e.g.
 * SgClassDeclaration). See description of SgDeclarationStatement. It is
 * important to note that this is the first non-defining declaration as opposed
 * to the defining declaration.  Thus it is usually the first forward
 * declaration.
 * - Internal: The value held is sharable across many references where as the
 * defining declaration (not held by this variable) should have one unique
 * reference within the AST, except where reference though the declarations
 * definingDeclaration data member pointer.  See declarations for details
 * (SgDeclarationStatements).
 *
 * #### `SgDeclarationStatement::p_linkage`
 * This string stores the value "C", "C++", or "fortran" to define the external
 * linkage for generating linkable names.
 * The only values that make sense here are "C", "C++" is the assumed default
 * and requires a NULL pointer value.  And the use of "fortran" would be backend
 * specific (may be rejected by some backend vendor compilers).
 *
 * #### `SgDeclarationStatement::p_declarationModifier`
 * This is the mechanism to handle declaration modifiers.
 * See Stroustrup 3rd edition, Appendix A, for details of how modifiers are
 * represented in ROSE.
 * - Internal: There is no interface to declaration modifiers implemented
 * directly in this SgDeclarationStatement class.  This was done to simplify the
 * implementation.  Users are expected to access the SgDeclarationmodifier
 * object (returned by reference) directly to get/set its values.
 *
 * #### `SgDeclarationStatement::p_nameOnly`
 * When set on a variable declaration, `get_next` does not traverse its
 * base-type declaration links.
 *
 * #### `SgDeclarationStatement::p_forward`
 * This marks a declaration as being an explicit forward declaration.
 * All non-defining declaations are marked as forward declarations, while
 * a defining declaration is NOT a marked as a forward declaration.
 * - Internal: It is not clear enough to just use defining vs. nondefining
 * declarations to know when something is a forward declaration!
 *
 * #### `SgDeclarationStatement::p_externBrace`
 * This marks that the declaration appeared with "extern".
 * Note that I think this is also set even if it appears between "extern {" and
 * "}". It might be that this is a way to specify the first "extern {", but I
 * don't think it is used interanlly and it is not clear if attaching the
 * "extern {" to a declaration is a great way to handle this problem generally
 * (appears to be overlly sensative to transformations that would add, remove,
 * or move declarations (e.g in inlining and outlining transformations).
 * Currently the lex pass saves all "extern \"C\" {" declarations and treats
 * them as CPP directive (along with comments), they are then woven back into
 * the AST as a separate pass.
 * - Deprecated: This is now redundant with the seperate preprocessing phase.
 * - Internal: Not sure that we need or want this mechanism (or this
 * implementation of it). Not that the mechanism that we have is better and
 * based on the scanning for extern "X" in the source code directly (where "X"
 * is either "C", "C++", or "fortran".
 *
 * #### `SgDeclarationStatement::p_skipElaborateType`
 * This marks "type elaboration" which is the use of the keyword "class" before
 * variables of type class.
 * This is not often required, but is routinely output by the unparser to be
 * conservative.  The support for hidden lists allows the unparser to more
 * precisely output the type elaboration, and so this variable is redundant.
 * - Deprecated: This is now redundant with the hidden list analysis done to
 * support more accurate code generation.
 * - Internal: This field is used in a default mode and could be set to be more
 * accurate.
 *
 * #### `SgDeclarationStatement::p_need_name_qualifier`
 * This marks a declaration are requiring qualification in its output in the
 * unparser.
 * - Deprecated: This is now redundant with the hidden list analysis done to
 * support more accurate code generation.
 * - Internal: Although this variable is specific to unparing, I think it may be
 * required. Although the name qualifications were recently reimplemented to be
 * more uniform and robust I'm not clear if this variable is a part of that new
 * implementation (and thus required).
 *
 * **Member functions**
 *
 * #### `SgDeclarationStatement::get_declarationModifier()`
 * Returns SgDeclarationModifier object by reference (so that it can be set).
 * - Return: Returns SgDeclarationModifier by reference.
 *
 * #### `SgDeclarationStatement::isForward() const`
 * Returns boolean value indicating if this is a forward declaration.
 * - Return: Returns bool.
 *
 * #### `SgDeclarationStatement::setForward()`
 * Marks a declaration as a forward declaration.
 * - Return: Returns void.
 *
 * #### `SgDeclarationStatement::unsetForward()`
 * Marks a forward declaration as a non-forward declaration.
 * - Return: Returns void.
 *
 * #### `SgDeclarationStatement::get_definingDeclaration() const`
 * This is an access function for the
 * SgDeclarationStatement::p_definingDeclaration data member (see that
 * variable's documentation for details).
 *
 * #### `SgDeclarationStatement::set_definingDeclaration(
 * SgDeclarationStatement* definingDeclaration )` This is an access function for
 * the SgDeclarationStatement::p_definingDeclaration data member (see that
 * variable's documentation for details).
 *
 * #### `SgDeclarationStatement::get_firstNondefiningDeclaration() const`
 * This is an access function for the
 * SgDeclarationStatement::p_firstNondefiningDeclaration data member (see that
 * variable's documentation for details).
 *
 * #### `SgDeclarationStatement::set_firstNondefiningDeclaration(
 * SgDeclarationStatement* firstNondefiningDeclaration )` This is an access
 * function for the SgDeclarationStatement::p_firstNondefiningDeclaration data
 * member (see that variable's documentation for details).
 *
 * #### `SgDeclarationStatement::get_symbol_from_symbol_table() const`
 * **FOR** **INTERNAL** **USE** Get the associated symbol from the symbol table
 * in the stored scope. Users should use the
 * SgDeclarationStatement::search_for_symbol_from_symbol_table() instead.
 *
 * #### `SgDeclarationStatement::search_for_symbol_from_symbol_table() const`
 * User interface for retrieving the associated symbol from the declaration.
 * - Return: Returns SgSymbol pointer.
 *
 */
class SgDeclarationStatement;

/** @brief This class represents the concept of a C or C++ default case within a
 * switch
 *
 * statement.
 * - See also:
 * Example of using a SgDefaultOptionStmt object
 *
 * **Data members**
 *
 * #### `SgDefaultOptionStmt::p_body`
 * This pointer points to SgStatement holding the statements executed for the
 * default case of a switch statement.
 *
 * **Member functions**
 *
 * #### `SgDefaultOptionStmt::get_body() const`
 * Returns pointer to SgStatement.
 * - Return: Returns pointer to SgStatement.
 *
 * #### `SgDefaultOptionStmt::set_body(SgStatement* body)`
 * Access function for p_body.
 * - Return: Returns void.
 */
class SgDefaultOptionStmt;

/** @brief This class represents the concept of a C++ call to the delete
 * operator.
 *
 * - Todo: I believe we can associate the destructors from the class with
 * delete operators.
 * - See also:
 * Example of using a SgDeleteExp object
 *
 * **Data members**
 *
 * #### `SgDeleteExp::p_variable`
 * This pointer points to the variable being deleted.
 *
 * #### `SgDeleteExp::p_is_array`
 * This bool value is true only if the delete operator is called on an array
 * (array delete).
 *
 * #### `SgDeleteExp::p_need_global_specifier`
 * This delete operator needs to be output with "::".
 * - Todo: Check if this bool data member is used and/or required.
 * - Internal: Not clear if this is required since Sage III should have the
 * scope sufficent to output any required name qualification.  More generally we
 * should be able to eliminate this once we explicitly store all explicit
 * qualified names.
 *
 * **Member functions**
 *
 * #### `SgDeleteExp::get_variable() const`
 * Access function for p_variable.
 * - Return: Returns pointer to SgExpression.
 *
 * #### `SgDeleteExp::set_variable(SgExpression* variable)`
 * Access function for p_variable.
 * - Param `variable`: is the variable to be deleted.
 * - Return: Returns void.
 *
 * #### `SgDeleteExp::get_is_array() const`
 * Access function for p_is_array.
 * - Return: Returns bool.
 *
 * #### `SgDeleteExp::set_is_array(short is_array)`
 * Access function for p_is_array.
 * - Param `is_array`: flag to record use of array delete.
 * - Return: Returns void.
 *
 * #### `SgDeleteExp::get_need_global_specifier() const`
 * Access function for p_need_global_specifier.
 * - Return: Returns bool.
 *
 * #### `SgDeleteExp::set_need_global_specifier(short
 * need_global_specifier)` Access function for p_need_global_specifier.
 * - Param `need_global_specifier`: flag to record use of array delete.
 * - Return: Returns void.
 */
class SgDeleteExp;

/** @brief This class represents a directory within a projects file structure of
 * files and directories.
 *
 * This IR node is intended to allow the AST at the SgProject level to have the
 * same representation as a project being compiled using ROSE.
 * - Internal: This is not yet used in ROSE and has been added as an experiment
 * to support very large ASTs for a whole project. It is expected a a million
 * lines of code will require a few Gig of storage, but memory is cheap, so we
 * will see what we can accomplish. Note that it is a limitation of ROSETTA that
 * we have to have the file list and the directory list be pointers to separate
 * (new) IR nodes that contain the actually STL lists of files and directories.
 * Note clear if this limitation in ROSETTA is documented anywhere :-).
 * - Todo: Consider adding a SgFileInfo data member to be uniform with SgFile.
 * - See also:
 * Example of using a SgDirectory object
 *
 */
class SgDirectory;

/** @brief This class represents the concept of a do-while statement.
 *
 * - Internal:
 * - Todo: The conditional test should be an expression (different from a
 * SgWhileStmt), see test2005_114.C for examples and details.
 *
 * **Data members**
 *
 * #### `SgDoWhileStmt::p_condition`
 * This pointer a SgStatement, the conditional expression in the loop construct.
 *
 * #### `SgDoWhileStmt::p_body`
 * This pointer a SgStatement, and holds the statements in the body of the
 * loop.
 *
 * **Member functions**
 *
 * #### `SgDoWhileStmt::copy(const SgCopyHelp & help)`
 * Makes a copy (deap of shallow depending on SgCopyHelp).
 * - Return: Returns pointer to copy of SgDoWhileStmt.
 *
 * #### `SgDoWhileStmt::get_body() const`
 * Access function for p_body.
 * - Return: Returns a pointer to a SgStatement.
 *
 * #### `SgDoWhileStmt::set_body(SgStatement* body)`
 * Access function for p_body.
 * - Param `body`: SgStatement pointer
 * - Return: Returns void.
 *
 * #### `SgDoWhileStmt::get_condition() const`
 * Access function for p_condition.
 * - Return: Returns a pointer to a SgStatement.
 *
 * #### `SgDoWhileStmt::set_condition(SgStatement* condition)`
 * Access function for p_condition.
 * - Param `condition`: SgStatement pointer
 * - Return: Returns void.
 */
class SgDoWhileStmt;

/** @brief This class represents the notion of an value (expression value).
 *
 * - See also:
 * Example of using a SgDoubleVal object
 *
 * **Data members**
 *
 * #### `SgDoubleVal::p_value`
 * This value holds the double represented in the source code.
 *
 */
class SgDoubleVal;

/** @brief This class represents the concept of an enum declaration.
 *
 * - See also:
 * Example of using a SgEnumDeclaration object
 *
 * **Data members**
 *
 * #### `SgEnumDeclaration::p_name`
 * Name of enum type (empty if not named).
 *
 * #### `SgEnumDeclaration::p_embedded`
 * Boolean value true if embedded in a typedef declaration (might not be used).
 *
 * #### `SgEnumDeclaration::p_type`
 * SgEnumType generated by this enum declaration.
 *
 * #### `SgEnumDeclaration::p_enumerators`
 * STL list of pointers to SgInitializedName object (used for enum variables).
 *
 * #### `SgEnumDeclaration::p_scope`
 * Scope of enum declaration.
 * Note that the scope of the enum must be stored explicitly since it can be
 * declared in a namespace and defined outside the namespace (note that forward
 * declarations of enum types.
 *
 * **Member functions**
 *
 * #### `SgEnumDeclaration::get_name() const`
 * Access function for p_name.
 * - Return: Returns SgName by value.
 *
 * #### `SgEnumDeclaration::set_name(SgName name)`
 * Access function for p_name.
 * - Return: Returns void.
 *
 * #### `SgEnumDeclaration::get_embedded() const`
 * Access function for p_embedded.
 * - Return: Returns bool by value.
 *
 * #### `SgEnumDeclaration::set_embedded(bool embedded)`
 * Access function for p_embedded.
 * - Return: Returns void.
 *
 * #### `SgEnumDeclaration::get_type() const`
 * Access function for p_type.
 * - Return: Returns pointer to SgType.
 *
 * #### `SgEnumDeclaration::set_type(SgType* type)`
 * Access function for p_type.
 * - Return: Returns void.
 *
 * #### `SgEnumDeclaration::get_enumerators() const`
 * Access function for p_enumerators.
 * - Return: Returns a const reference to SgInitializedNamePtrList.
 *
 * #### `SgEnumDeclaration::get_enumerators()`
 * Access function for p_enumerators.
 * - Return: Returns reference to SgInitializedNamePtrList.
 *
 * #### `SgEnumDeclaration::get_scope() const`
 * Access function for p_scope.
 * - Return: Returns pointer to SgScopeStatement.
 *
 * #### `SgEnumDeclaration::set_scope(SgScopeStatment* scope)`
 * Access function for p_scope.
 * - Return: Returns void.
 */
class SgEnumDeclaration;

/** @brief This class represents the concept of a C and C++ expression list.
 *
 * This is a class used to support multiple IR nodes internally, for example in
 * a function call "foo(exp,exp)" contains and expression list (SgExprListExp).
 * This object supports the following IR nodes:
 * - SgNewExp
 * - SgFunctionCallExp
 * - SgAggregateInitializer
 * - SgConstructorInitializer
 * - See also:
 * Example of using a SgExprListExp object
 *
 */
class SgExprListExp;

/** @brief This class represents the concept of a C or C++ statement which
 * contains a
 *
 * expression.
 * Any expression can be used alone as statement in C and C++, the expression
 * statment provides a mechanism to wrap any SgExpression and make it a
 * SgStaement for use anywhere a SgStatement might be required (e.g. in a list
 * of SgStatement objects in a SgBasicBlock).
 * - See also:
 * Example of using a SgExprStatement object
 *
 */
class SgExprStatement;

/** @brief This class represents the notion of an expression. Expressions
 *
 * are derived from SgLocatedNodes, since similar to statement, expressions
 * have a concrete location within the user's source code.
 * There are no direct uses of this IR node anywhere.  All expressions
 * are derived from this IR node to build derived classes such as unary and
 * binary operators, conditional expressions, delete expression, variable
 * reference expressions, etc.
 * Value expressions expose their semantic type through get_type(); syntax-only
 * expression nodes reject that query.
 * - Internal: This is a base class for all expressions.
 * As a design point IPR has an empty expression, not an empty statement. And
 * empty statement can be built as an expression statement with an empty
 * expression (which IPR does have). We should consider having such an empty
 * expression.  Currently we have and expression statement with a null pointer
 * (none too elegant). Sage III now has both a SgNullStatement and a
 * SgNullExpression.
 * - Todo: Consider placing the VARARGS expression nodes into a common base
 * class derived from SgExpression.
 * Expression types and declaration references are published by the language
 * frontend. Flang semantic analysis resolves array references and procedure
 * calls before constructing their distinct expression nodes.
 * - See also:
 * Example of using a SgExpression object
 *
 * **Data members**
 *
 * #### `SgExpression::p_lvalue`
 * This boolean variable marks the current expression as a
 * left hand side value (lvalue).
 * - Deprecated: This computation of this value is innacurate.  It identifies
 * expressions which have been assigned to.  This does not include all lvalues
 * as defined by the C++ standard.  For "int* p", "*p" and "&p" are both
 * lvalues, despite not being assigned to (section 3.10).
 *
 * #### `SgExpression::p_need_paren`
 * This boolean value marks the current expression as requiring parenthises.
 * This boolean value marks the current expression as requiring parenthises (the
 * information comes from the frontend's interpretation of the requirement and
 * is almost always overly conservative.  The unparser currently backs out more
 * accurate rules based on operator precedence and removed then where they
 * are not truely required.  Thus the purpose of this variable is to capture the
 * interpritation of the frontend regarding the use of parenthesis.
 *
 * **Member functions**
 *
 * #### `SgExpression::get_need_paren (void) const`
 * returns bool value if front-end considers parenthesis to be required.
 * This function returns a boolean value which is true if parenthesis are
 * required for the proper eveluation of the current expression.  The frontend
 * is however overly conservative and introduces parenthesis too often.  The
 * value as determined by the frontend is saved in the SAGE III AST because it
 * is sometimes required for more complex expressions and contructor calls with
 * initializers.  At present the unparser applies operator precedence rules to
 * eliminate some of the redundent parenthesis within expressions (this makes
 * the output more appealing).
 * - Return: Returns bool value (true if parenthesis are needed around the
 * expression).
 *
 * #### `SgExpression::set_need_paren (bool need_paren)`
 * This function allows the p_need_paren flag to be set (used internally).
 * This function is mostly used internally but would required for the
 * construction of specific expression trees where the operator precedence would
 * be insufficent to generate the correct code (which si the purpose of adding
 * parenthesis in source code).
 * - Param `need_paren`: - sets value of internal p_need_paren flag (true if
 * parenthesis are needed around the expression).
 * - Return: Returns void.
 *
 * #### `SgExpression::get_lvalue (void) const`
 * Returns a bool value if the current expression is assigned to.
 * - Deprecated: The computation of this value is innacurate in terms of the
 * lvalue as defined by the standard.  It identifies expressions which have been
 * assigned to.  This does not include all lvalues as defined by the C++
 * standard.  For "int* p", "*p" and "&p" are both lvalues, despite not being
 * assigned to (section 3.10).  See SgExpression::p_lvalue.
 * - Return: Returns true if the current expression is assigned to.
 *
 * #### `SgExpression::set_lvalue (bool lvalue)`
 * This function allows the p_lvalue flag to be set (used internally).
 * This function is mostly used internally but would required for the
 * construction of specific expression trees. In general it is set by the
 * frontend, but not used elsewhere within SAGE III (will be tested in the
 * future).
 * - Param `lvalue`: - sets value of internal p_lvalue flag (true if the current
 * is to be a left hand side value (lvalue)).
 * - Return: Returns void.
 *
 * #### `SgExpression::isDefinable (void) const`
 * For Fortran.  When called from a node of any other language, behavior is
 * undefined (see SageInterface::is_Fortran_language). Returns a bool value
 * which is true if the current expression is "almost definable". "Almost
 * definable" here means as defined by the Fortran standard (2.5.5), with the
 * caveat that allocatable variables are always called an lvalue. The Fortran
 * standard dictates that an allocatable variable which has not been allocated
 * is not definable. In the general case, it cannot be statically determined
 * whether an allocatable variable has been allocated or not. Note: constants in
 * Fortran are not lvalues, while in C and C++ lvalues may be const.
 * - Return: Returns true if the current expression is almost definable.
 *
 * #### `SgExpression::isUsedAsDefinable (void) const`
 * For Fortran.  When called from a node of any other language, behavior is
 * undefined (see SageInterface::is_Fortran_language). Returns a bool value
 * which is true if the current expression is used as a definable value.  That
 * is, it is definable (assigned to).
 * - Return: Returns true if the current expression is used as a definable
 * value.
 *
 * #### `SgExpression::isLValue (void) const`
 * For C and C++. When called from a node of any other language, behavior is
 * undefined (see SageInterface::is_C*_language). Returns a bool value which is
 * true if the current expression is an lvalue, as defined by the C++ standard.
 * - Return: Returns true if the current expression is an lvalue.
 *
 * #### `SgExpression::isUsedAsLValue (void) const`
 * For C and C++. When called from a node of any other language, behavior is
 * undefined (see SageInterface::is_C*_language). Returns a bool value which is
 * true if the current expression is used as an lvalue.  That is, it is an
 * lvalue and does not get converted to an rvalue by its parent expression.
 * - Return: Returns true if the current expression is used as an lvalue.
 *
 * #### `SgExpression::isChildUsedAsLValue (SGExpression) const`
 * For C and C++. When called from a node of any other language, behavior is
 * undefined (see SageInterface::is_C*_language). Returns a bool value which is
 * true if the specified expression is used as an lvalue.  That is, it is an
 * lvalue and does not get converted to an rvalue by its parent expression.
 * - Internal: For use by children calling their parents.
 * - Return: Returns true if the specified expression is used as an lvalue.
 *
 * #### `SgExpression::get_type() const`
 * Get the type associated with this expression.
 * This function is the correct user function to call to get the
 * type (note that get_expression_type() is for internal use only).
 * - Return: Returns pointer to SgType (base class for all possible types)
 *
 */
class SgExpression;

/** @brief Base class for files in a project.
 * Stores file metadata and command-line options. SgSourceFile owns the global
 * scope of a source file.
 *
 * **Data members**
 *
 * #### `ROSEAttributesListContainerPtr
 * SgFile::p_preprocessorDirectivesAndCommentsList` This is the container of
 * comments and preprocessor control statements that was extracted from the file
 * and which has been woven back into the AST (using heuristics). This
 * information is extracted from the file using a lex based parser.  Within AST
 * post processing the comments and preprocessor control statements are woven
 * back into the AST using simple heuristics.  They are attached to the IR nodes
 * as attributes (see AST attribute Mechanism).  They are unparsed as part of
 * the code generation phase.
 *
 * #### `SgFile::p_originalCommandLineArgumentList`
 * Copy of original argc and argv command line passed to ROSE translator
 * (converted to STL list of strings).
 * - Internal: This is a deep copy.
 *
 * #### `SgFile::p_file_info`
 * This object permits the file name map mechanism to be used on SgFile objects.
 * This Sg_File_Info object encapsulates the file id mechanism which maps file
 * ids (integers) to filenames (strings). This permist us to save significant
 * storage by holding string names only once in memory and associating integers
 * with them saving memory and simplify matching file names.  The memory saving
 * is because there would otherwise be a LOT of redundant filename strings, 2
 * per IR node in many cases. the performance advantage is that integer
 * comparisons are significantly than string comparisions and there are a lot of
 * these (one per IR node) in the traversal mechanisms.
 * - Internal: The filename is not currently correct, I think.
 *
 * **Member functions**
 *
 * #### `list<string> SgFile::get_originalCommandLineArgumentList() const`
 * Returns a list of strings representing the original command-line.
 *
 * #### `SgFile::set_originalCommandLineArgumentList( list<string>
 * originalCommandLineArgumentList )` Sets the list of strings representing the
 * original command-line.
 */
class SgFile;

/** @brief This class represents the location of the code associated with the IR
 * node in the
 *
 * original source code.
 * This object is used to represent the starting and the ending position of the
 * source code associated with some IR nodes (in other cases the endOfConstruct
 * returnes the same data as the startOfConstruct). Classification of the IR
 * nodes is included in this object and applies to:
 * - SgStatements
 * - SgExpression
 * - SgInitializedName
 * - SgFile (and maybe SgDirectory)
 * - Perhaps also SgTemplateArguments, SgTemplateParameters, SgBaseClass
 * classifications are:
 * - e_transformation \n
 * The IR node is part of a transformation.  The transformation may be assigned
 * to a file, in which case the file id references the file (default is the
 * source file, that reachable from the scope where the transformation is done).
 * - e_compiler_generated \n
 * The IR node may be part of a compiler generated IR nodes (or subtree, in
 * which case all IR nodes in the subtree are marked as compiler generated).
 * Compiler generated IR nodes include:
 * -# implicit casts
 * -# instantiated templates (not explicitly specialized)
 * -# non-explicit return statements in **main** function.
 * -# (... complete this list) \n
 * - e_output_in_code_generation \n
 * -# Used to explicitly makr any compiler generated IR nodes for output (forces
 * template instaiations to be output in the generated source if transformed).
 * - e_shared \n
 * Marks IR nodes as shared (set by AST merge mechanism).  Note that SgTypes and
 * SgSymbols don't have a Sg_File_Info object, but are ALWAYS shared. \n
 * (multiple classifications can co-exist).
 * - Internal: A pointer for this object is in the SgNode, but it is only valid
 * for SgLocatedNodes and the SgInitializedName. The get_File_Info() member
 * function (and associated get_startOfConstruct() and get_endOfConstruct()
 * member functions) are implemented in the SgNode, for uniformity, but return
 * NULL for all but the SgLocatedNodes and the SgInitializedName IR nodes.
 * - Todo: isOutputInCodeGeneration() is orthogonal to isCompilerGenerated and
 * isTransformation().  Currently IR nodes that are marked as isTransformation()
 * are output, but these need to be marked as also being
 * isOutputInCodeGeneration() so that orthogonality of the concepts is
 * maintained.
 * - Todo: It is possible to call get_file_info() on a SgFileInfo object and
 * this needs to be fixed because it does not make any sense.  This is because
 * get_file_info is defined as a virtual function on SgNode.  Not sure this is a
 * great design, but maybe it just needs a local implementation of a private
 * get_file_info() member function so that it can't be called (can be hidden).
 * - Todo: Should there be a simpler way to copy a SgFileInfo object than:
 * "new Sg_File_Info(*fileInfo);" or "fileInfo->copy();"; likely not!
 * - Todo: Define the subset of IR nodes which would all have:
 * -# Sg_File_Info
 * -# AstAttributeMechanism
 * -# SgNode (parent pointer) \n
 * This will make the documentation more intuative.  The argument supporting
 * this is that we operate on those IR nodes that correspond to the visual
 * representation of the source code (so we can exclude SgSymbols, and SgTypes).
 * Having the same subset of IR nodes permits a simpler documentation of the API
 * and intuition as to where it applies.  A likely subset would be:
 * -# SgLocatedNodes
 * -# SgFile
 * -# SgDirectory (questionable)
 * -# SgInitializedName
 * -# SgBaseClass
 * -# SgTemplateArgument
 * -# SgTemplateParameter
 * - Todo: Consider putting the endOfConstruct information into the single
 * Sg_File_Info object. Currently the SgLocatedNode stores two Sg_File_Info
 * objects, one for the beginning and the end of each construct.  This would
 * save significant space in the AST. Additional information in the Sg_File_Info
 * could be:
 * - offset from starting line number to the end of construct
 * - flag for if end of construct is located in the same file as the start of
 * construct
 * - Todo: Consider using "short int" instead of "int" for the file_id, line,
 * and col (and maybe the classificationBitField) to reduce the size of the data
 * structure. Padding is not a significant issue since data structures are
 * allocated in contiguious memory (except for padding to at least the nearest
 * byte if bit field widths are used.
 * - See also:
 * Example of using a Sg_File_Info object
 *
 * **Data members**
 *
 * #### `Sg_File_Info::p_file_id`
 * This is a key that maps integers to file names, it prevents redundent storage
 * of filename strings. This is part of a mechanism to reduce the memory
 * requirements of the IR (to support whole program analysis in ROSE).  The
 * values are keys to an STL map which relates file ids to file name strings,
 * allowing a more compressed representation of the filename data in the
 * Sg_File_Info object.  There are two maps, one for finding the filename string
 * from a file id integer (p_fileidtoname_map), and another for the reverse map
 * (p_nametofileid_map).
 * - Internal: We will in the future limit the range of this value (expect a
 * signed short int: 16 bits, 32K = 32768 files plus negative values for special
 * cases).
 *
 * #### `Sg_File_Info::p_line`
 * This is the line number of the location of the source code for this IR node.
 * This information is useful in the heuristics of attaching comments to IR
 * nodes (SgStatement and SgExpression IR nodes). The value of this variable is
 * always greater than or equal to zero. In the case of isCompilerGenerated() ==
 * true or isTransformation() == true then value is INT_MAX-1. This value if
 * define for this case so that all comments will be attached before the
 * associated statement.
 * - Internal: We will in the future limit the range of this value (expect a
 * short int: 20 bits, 1000K = 1,048,576 lines per file).
 *
 * #### `Sg_File_Info::p_col`
 * This is the column number of the location of the source code for this IR
 * node. This information is useful in the heuristics of attaching comments to
 * IR nodes (SgStatement and SgExpression IR nodes). The value of this variable
 * is always greater than or equal to zero. If IR node is compiler generated for
 * a transformation then value is set to INT_MAX-1.
 * - Internal: We will in the future limit the range of this value (expect a
 * short int: 16 bits, 64K = 65536 charaters per line).
 *
 * #### `Sg_File_Info::p_classificationBitField`
 * This is mechanism for classification of the IR node.
 * IR nodes use this mechanism to classify themselves as either part of a
 * transformation or compiler generated (other enum values of
 * Sg_File_Info::classifier are not used).
 * - Internal: We will in the future limit the range of this value (expect an
 * unsigned char: 8 bits, 64K = 65536 charaters per line).
 *
 * #### `Sg_File_Info::p_fileIDsToUnparse`
 * This set contains a list of all file ids for which the accompanying IR node
 * should be unparsed. This is part of the AST Merge mechanism. This set
 * contains a list of file ids. During unparsing, if we encounter a node with
 * this Sg_File_Info object, we only want to unparse this file if the file we
 * are currently unparsing is in this list.
 * - Internal: NOTE: This set should be empty unless the node is marked as
 * shared
 *
 * #### `std::map< int, std::string > Sg_File_Info::p_fileidtoname_map`
 * This is a static STL map of file id (integers) to file names (strings).
 *
 * #### `std::map< std::string, int > Sg_File_Info::p_nametofileid_map`
 * This is a static STL map of file names (strings) to file id (integers).
 *
 * **Member functions**
 *
 * #### `Sg_File_Info::Sg_File_Info()`
 * This is the less useful default constructor.
 * This constructor builds the Sg_File_Info base class.
 * - Internal: This constructor should be deprecated since we don't want it to
 * be used. It's presence is historical, since it was previously used a lot
 * (inappropriately). Before it is removed we likely need a constructor that can
 * take a Sg_File_Info::classifier enum value.
 * - See also:
 * Example:create an Sg_File_Info object
 *
 * #### `Sg_File_Info::Sg_File_Info ( const char *filename, int line=0, int
 * col=0 )` This is the more useful constructor. This constructor builds the
 * Sg_File_Info base class.
 * - See also:
 * Example:create an Sg_File_Info object
 *
 * #### `Sg_File_Info::Sg_File_Info(const Sg_File_Info & X)`
 * This the copy constructor (deep copy made).
 * - Internal: This constructor does a deep copy of all data in the Sg_File_Info
 * object.
 * - See also:
 * Example:create an Sg_File_Info object
 *
 * #### `Sg_File_Info::set_line ( int line )`
 * Sets the line number associated with the source file.
 *
 * #### `Sg_File_Info::get_filename () const`
 * Returns the classified filename as a C string. Transformation positions
 * report "transformation"; compiler-generated positions outside the frontend
 * report "compilerGenerated".
 *
 * #### `Sg_File_Info::get_line () const`
 * Returns the line number of the associated code for this IR node.
 * - Return: Returns integer.
 *
 * #### `Sg_File_Info::get_col () const`
 * Returns the column number of the associated code for this IR node.
 * - Return: Returns integer
 *
 * #### `Sg_File_Info::get_raw_filename () const`
 * Returns the filename mapped from the stored file ID without classification.
 *
 * #### `Sg_File_Info::get_raw_line () const`
 * Returns the line number of the associated code for this IR node.
 * - Return: Returns integer.
 *
 * #### `Sg_File_Info::get_raw_col () const`
 * Returns the column number of the associated code for this IR node.
 * - Return: Returns
 *
 * #### `Sg_File_Info::generateDefaultFileInfo()`
 * Static function to return new Sg_File_Info object set to default values.
 * - Return: Returns pointer to Sg_File_Info.
 *
 * #### `Sg_File_Info::generateDefaultFileInfoForTransformationNode()`
 * Static function to return new Sg_File_Info object set to default values
 * appropriate for transformations.
 * This function calls setOutputInCodeGeneration() and sets the file_id to be
 * TRANSFORMATION_FILE_ID.
 * - Return: Returns pointer to Sg_File_Info.
 *
 * #### `Sg_File_Info::generateFileInfoForTransformationNode( int file_id )`
 * Static function to return new Sg_File_Info object set to default values
 * appropriate for transformations that are not in the source file. Static
 * function which allows specification of transformation with assignement to a
 * specific file (where it would be unparsed). This function will call
 * setOutputInCodeGeneration() and sets the file_id to a non-negative value.
 * This function is useful when transformation to a program happen within a
 * header file (e.g. including a header file as part of a transformation).
 * Setting the file_id to the current source file should have the same result
 * (semantics) as Sg_File_Info::generateDefaultFileInfoForTransformationNode().
 * - Return: Returns pointer to Sg_File_Info.
 *
 * #### `Sg_File_Info::generateFileInfoForTransformationNode( string filename )`
 * Static function to return new Sg_File_Info object set to default values
 * appropriate for transformations that are not in the source file. Function
 * similar to previous function, but takes a filename to support new files that
 * have not ben seen previously (e.g. new header file).
 * - Return: Returns pointer to Sg_File_Info.
 *
 * #### `Sg_File_Info::generateDefaultFileInfoForCompilerGeneratedNode()`
 * Static function to return new Sg_File_Info object set to default values
 * appropriate for compiler generated code.
 * - Return: Returns pointer to Sg_File_Info.
 *
 * #### `Sg_File_Info::hasPositionInSource() const`
 * Abstracts query as to if an IR node maps back to the source code (evolving
 * set of reasons whey this can be false). There are several reasons why an IR
 * node might NOT have a mapping back to the source code:
 * - could be a part of a transformation (isTransformation() == true)
 * - could be a compiler generated statement or expression
 * (isCompilerGenerated() == true)
 * - Internal: This is an evolving set of reasons, so abstracting it as a member
 * function is helpful.
 * - Return: Returns bool.
 *
 * #### `Sg_File_Info::isTransformation() const`
 * Returns true only if part of a transformation.
 * - Return: Returns bool.
 *
 * #### `Sg_File_Info::isCompilerGenerated() const`
 * Returns true only if compiler generated (either by the front-end or by ROSE).
 * Source provenance and output eligibility have separate typed flags; being
 * compiler generated does not by itself require emission by the unparser.
 * - Return: Returns bool.
 *
 * #### `Sg_File_Info::isOutputInCodeGeneration() const`
 * Returns true only if required to be unparsed in generated code.
 * - Internal: Templates instatiated by ROSE are marked as compiler generated,
 * but only those instatiated templates that are used in the source file are
 * required and so only those must be unparsed in the generated code. Note that
 * currently all transformed IR nodes are implicitly considered to be marked as
 * *outputInCodeGeneration,* but this will be made an explicit requirement in
 * the future.
 * - Return: Returns bool.
 *
 * #### `Sg_File_Info::isShared() const`
 * Returns true only if shared internally (either by the front-end or by ROSE).
 * - Internal: This is a new classification supporting the AST merge mechanism.
 * Currently numerous types of IR nodes that are shared (SgType, SgSymbol, many
 * non-defining declarations, etc.) are not explicitly marked as shared.  This
 * may be correct in a future release (requires more thought).
 * - Return: Returns bool.
 *
 * #### `Sg_File_Info::unsetTransformation()`
 * If the IR node is a transformation it marks it false (zeros transformation
 * marker bit internally).
 * - Return: Returns void.
 *
 * #### `Sg_File_Info::setTransformation()`
 * Marks an IR node to be a transformation if it is not one already.
 * - Return: Returns void.
 *
 * #### `Sg_File_Info::unsetCompilerGenerated()`
 * Unmarks IR node as compiler generated.
 * - Return: Returns void.
 *
 * #### `Sg_File_Info::setCompilerGenerated()`
 * Marks IR node as compiler generated.
 * - Return: Returns void.
 *
 * #### `Sg_File_Info::unsetOutputInCodeGeneration()`
 * Unmarks IR node as compiler generated but required in the generated source
 * (e.g. requied templates).
 * - Return: Returns void.
 *
 * #### `Sg_File_Info::setOutputInCodeGeneration()`
 * Marks IR node as compiler generated but required in the generated source
 * (e.g. requied templates).
 * - Return: Returns void.
 *
 * #### `Sg_File_Info::unsetShared()`
 * Unmarks IR node as shared.
 * - Return: Returns void.
 *
 * #### `Sg_File_Info::setShared()`
 * Marks IR node as shared.
 * - Return: Returns void.
 *
 * #### `bool Sg_File_Info::ok()`
 * Checks internal consistancy of data.
 * Checks file ID, line, and column, with explicit allowances for generated,
 * transformed, or unavailable source positions.
 * Defined to be used in tests:
 * ROSE_ASSERT(statement->get_file_info()->ok());
 * - Return: Returns bool.
 *
 * #### `Sg_File_Info::display( const string label )`
 * This function outputs the internal data to stdout, for debugging.
 * - Return: Returns void.
 *
 * #### `bool Sg_File_Info::operator== ( const Sg_File_Info & X, const
 * Sg_File_Info & Y )` This relational operator tests two Sg_File_Info objects
 * for equal position information.
 * - Return: Returns bool.
 *
 * #### `bool Sg_File_Info::operator!= ( const Sg_File_Info & X, const
 * Sg_File_Info & Y )` This relational operator tests two Sg_File_Info objects
 * for unequal position information.
 * - Return: Returns bool.
 *
 * #### `Sg_File_Info::operator> ( const Sg_File_Info & X, const Sg_File_Info &
 * Y )` This relational operator tests two Sg_File_Info objects for X being
 * after Y in the same file.
 * - Return: Returns bool.
 *
 * #### `Sg_File_Info::operator< ( const Sg_File_Info & X, const Sg_File_Info &
 * Y )` This relational operator tests two Sg_File_Info objects for X being
 * before Y in the same file.
 * - Return: Returns bool.
 *
 * #### `Sg_File_Info::operator>= ( const Sg_File_Info & X, const Sg_File_Info &
 * Y )` This relational operator tests two Sg_File_Info objects for X being at
 * the same position or after Y in the same file.
 * - Return: Returns bool.
 *
 * #### `Sg_File_Info::operator<= ( const Sg_File_Info & X, const Sg_File_Info &
 * Y )` This relational operator tests two Sg_File_Info objects for X being at
 * the same position or before Y in the same file.
 * - Return: Returns bool.
 */
class Sg_File_Info;

/** @brief This class represents the notion of an value (expression value).
 *
 * - See also:
 * Example of using a SgFloatVal object
 *
 * **Data members**
 *
 * #### `SgFloatVal::p_value`
 * This value holds the float represented in the source code.
 *
 */
class SgFloatVal;

/** @brief This class represents the variable declaration or variable
 * initialization withn a
 *
 * for loop.
 * This class is only used in a SgForStatement; and represents "*" within
 * "for(*; <condition>; <expression>)".
 * - Internal: The design of the ROSE traversal mechanism requires that the
 * child list at any IR node be a list or a collection of data members (but not
 * both, since it would not be clear when the list ended and another list or
 * data member begain).  this class is required so that list of initializers in
 * a for loop (an arbitary length list) can be isolated from the other data
 * members in the SgForStatement.
 * - Todo: Evaluate if this should be derived from SgSupport, like other "list"
 * based IR nodes.
 * - Todo: Evaluate if we should even have this IR node.  If the
 * SgVariableDeclaration were to be fixed to really use the list of
 * SgInitializedName objects where multiple variables are declared in the same
 * variable declaration then we might not need this (I think). And if it didn't
 * exist it would make the use of the SgForStatement a little bit simpler.
 * - See also:
 * Example of using a SgForInitStatement object
 *
 * **Data members**
 *
 * #### `SgForInitStatement::p_init_stmt`
 * This pointer points to list of initializers in a SgForStatement.
 *
 * **Member functions**
 *
 * #### `SgForInitStatement::get_init_stmt() const`
 * Returns const reference to a SgStatementPtrList (typedef to a STL list).
 * - Return: Returns const reference to STL list.
 *
 * #### `SgForInitStatement::get_init_stmt()`
 * Returns non-const reference to a SgStatementPtrList (typedef to a STL list).
 * - Return: Returns non-const reference to STL list.
 */
class SgForInitStatement;

/** @brief This class represents the concept of a for loop.
 *
 * **Data members**
 *
 * #### `SgForStatement::p_for_init_stmt`
 * This pointer a SgForInitStatement (a list of pointers to statements
 * (SgStatement objects) ).
 * - Note: I think this is a poor name for this variable.
 *
 * #### `SgForStatement::p_test`
 * The condition statement, which can be an expression statement or declaration.
 *
 * #### `SgForStatement::p_increment`
 * The increment expression.
 *
 * #### `SgForStatement::p_loop_body`
 * This pointer a SgStatement, and holds the statements in the body of the
 * loop.
 * - Note: I think this is a poor name for this variable (should be "body" to
 * match other IR nodes).
 * - Todo: Change "loop_body" to "body" to be consistant with other scopes that
 * contain a SgStatement.
 * **Member functions**
 *
 * #### `SgForStatement::copy(const SgCopyHelp & help)`
 * Makes a copy (deap of shallow depending on SgCopyHelp).
 * - Return: Returns pointer to copy of SgForStatement.
 *
 * #### `SgForStatement::get_for_init_stmt() const`
 * Access function for p_for_init_stmt.
 * - Return: Returns a pointer to a SgForInitStatement.
 *
 * #### `SgForStatement::set_for_init_stmt(SgForInitStatement* for_init_stmt)`
 * Access function for p_for_init_stmt.
 * - Param `for_init_stmt`: SgForInitStatement pointer
 * - Return: Returns void.
 *
 * #### `SgForStatement::get_test_expr() const`
 * Returns the expression held by an expression-statement test.
 * - Return: Returns a pointer to a SgExpression.
 *
 * #### `SgForStatement::get_loop_body() const`
 * Access function for p_loop_body.
 * - Return: Returns a pointer to a SgStatement.
 *
 * #### `SgForStatement::set_loop_body(SgStatement* loop_body)`
 * Access function for p_loop_body.
 * - Param `loop_body`: SgStatement pointer
 * - Return: Returns void.
 */
class SgForStatement;

/** @brief This class represents the concept of a C++ function call (which is an
 * expression).
 *
 * Note that a function call is assembled from a function expression and
 * arguments. The two are bound together in a function call, function
 * expressions are never called directly (the function call argument list can be
 * empty).
 * - Internal: When overloaded operators are used the function name becomes the
 * name of the operator (e.g. "operator+").  In these cases the precedence of
 * the operator is that of the operator being overloaded and is different from a
 * normal function call.  Most of this detail is hidden since operands in the
 * expression tree are represented by functions with parameter lists
 * (represented by a SgExprListExp), and not simple expression trees.
 * - See also:
 * Example of using a SgFunctionCallExp object
 *
 */
class SgFunctionCallExp;

/** @brief The ordered parameter declarations of a function.
 *
 * - See also:
 * Example of using a SgFunctionParameterList object
 *
 * **Data members**
 *
 * #### `SgFunctionParameterList::p_args`
 * STL list of pointers to SgInitializedName object (used for function parameter
 * declarations).
 *
 * **Member functions**
 *
 * #### `SgFunctionParameterList::get_args() const`
 * Access function for p_args.
 * - Return: Returns a const reference to SgInitializedNamePtrList.
 *
 * #### `SgFunctionParameterList::get_args()`
 * Access function for p_args.
 * - Return: Returns reference to SgInitializedNamePtrList.
 */
class SgFunctionParameterList;

/** @brief This class represents the function being called and must be assembled
 * in the
 *
 * SgFunctionCall with the function arguments.
 * - Internal: When used in a SgFunctionCallExp (most common, or typical case
 * (and maybe the only case)) the SgExpression is one of the following:
 * - SgDotExp
 * - SgDotStarOp
 * - SgArrowExp
 * - SgArrowStarOp
 * - SgPointerDerefExp
 * - SgFunctionRefExp
 * - SgMemberFunctionRefExp
 * - Todo: Figure out why SgMemberFunctionRefExp is required instead of just
 * SgFunctionRefExp.
 * - Todo: Make the use of a SgMemberFunctionSymbol in a SgFunctionRefExp an
 * error. The result will not unparse correctly (suggested by Jeremiah).
 * - See also:
 * Example of using a SgFunctionRefExp object
 *
 * **Data members**
 *
 * #### `SgFunctionRefExp::p_lvalue`
 * This boolean variable marks the current expression as a
 * left hand side value (lvalue).
 *
 * #### `SgFunctionRefExp::p_need_paren`
 * This boolean value marks the current expression as requiring parenthises.
 * This boolean value marks the current expression as requiring parenthises (the
 * information comes from the frontend's interpretation of the requirement and
 * is almost always overly conservative.  The unparser currently backs out more
 * accurate rules based on operator precedence and removed then where they
 * are not truely required.  Thus the purpose of this variable is to capture the
 * interpritation of the frontend regarding the use of parenthesis.
 *
 * **Member functions**
 *
 * #### `SgFunctionRefExp::get_type() const`
 * Returns the exact type associated with the referenced symbol.
 *
 */
class SgFunctionRefExp;

/** @brief This class represents a type for all functions.
 *
 * - Internal: This type is derived from to build member function types
 * (SgMemberFunctionType).
 * - See also:
 * Example of using a SgFunctionType object
 *
 * **Data members**
 *
 * #### `SgFunctionType::p_argument_list`
 * This points to the SgTypes used in the function's parameter list.
 *
 * #### `SgFunctionType::p_return_type`
 * This points to the SgType of the current function type's return type.
 *
 * #### `SgFunctionType::p_has_ellipses`
 * This boolean variable is true if the function use the "..." type
 * (support for variable number of parameters).
 *
 */
class SgFunctionType;

/** @brief This class represents the concept of a name and a type. It may be
 *
 * renamed in the future to SgTypeSymbol (since it is ued for both functions
 * types and more general types).
 * This IR node represents named entries in SgTypeTable. Canonical function
 * types are indexed separately by typed signatures in SgFunctionTypeTable.
 * - Todo: Consider derivation of SgEnumSymbol, SgClassSymbol,
 * SgFunctionTypeSymbol, and SgTypedefSymbol from a common SgTypeSymbol.  Then
 * supporting functions for SgTypeSymbol would lookup any of these type based
 * symbols.
 *
 * **Member functions**
 *
 * #### `SgFunctionTypeSymbol::get_name() const`
 * Access function for getting name from declarations or types internally.
 * - Internal: This is a virtual function.
 * - Return: Returns SgName.
 *
 * #### `SgFunctionTypeSymbol::get_type() const`
 * This function returns the type associated with the named entity.
 * - Internal: This is a virtual function.
 * - Return: Returns SgType*.
 *
 */
class SgFunctionTypeSymbol;

/** @brief Canonical function types indexed by semantic signatures.
 *
 * SgNode owns the global table. A signature contains the return type, argument
 * types, member class type, and member specifiers. Lookup never requires name
 * mangling and is valid while declarations are still being constructed.
 * Entries do not own their shared type nodes. clear() and
 * remove_function_type() invalidate cached identities without deleting types
 * that ASTs still use. get_types() exposes a read-only index for checkpoint
 * invalidation.
 */
class SgFunctionTypeTable;

/** @brief The global scope of a source file.
 *
 * **Data members**
 *
 * #### `SgGlobal::p_declarations`
 * This is an STL list of SgDeclarationStatement objects.
 * - Internal: The name is perhaps all too similar to p_declaration (which
 * exists on many IR nodes).
 *
 * **Member functions**
 *
 * #### `SgGlobal::get_qualified_name() const`
 * Returns SgName (a string) representing the name of the current scope (empty
 * string). See discussion of mangled names in the documentation.
 * - Return: Returns SgName (a string).
 *
 * #### `SgGlobal::copy(const SgCopyHelp & help)`
 * Makes a copy (deap of shallow depending on SgCopyHelp).
 * - Return: Returns pointer to copy of SgGlobal.
 *
 * #### `SgGlobal::get_declarations()`
 * Returns a list to the global scope declarations.
 * - Return: Returns an STL list by reference.
 *
 * #### `SgGlobal::get_declarations() const`
 * Returns a const list to the global scope declarations.
 * - Return: Returns a const STL list by reference.
 */
class SgGlobal;

/** @brief This class represents the concept of a C or C++ goto statement.
 *
 * - See also:
 * Example of using a SgGotoStatement object
 *
 * **Data members**
 *
 * #### `SgGotoStatement::p_label`
 * This pointer points to the SgLabelStatement where control flow will be
 * transfered during execution.
 * - Internal: This is always a valid pointer.
 *
 * **Member functions**
 *
 * #### `SgGotoStatement::get_label() const`
 * Returns pointer to SgLabelStatement where control flow will be transfered
 * during execution.
 * - Return: Returns pointer to SgLabelStatement.
 *
 * #### `SgGotoStatement::set_label(SgLabelStatement* label)`
 * Access function for p_label.
 * - Return: Returns void.
 */
class SgGotoStatement;

/** @brief This class represents the concept of an "if" construct.
 *
 * - Internal:
 * - Todo: The unparse function "unparseIfStmt" associated with this IR node
 * is implemented using a loop.  I think this should be changed to be more
 * conventional and structural (consistant with the design of the rest of
 * the unparsing).
 *
 * **Data members**
 *
 * #### `SgIfStmt::p_conditional`
 * This pointer a SgStatement.
 *
 * #### `SgIfStmt::p_true_body`
 * This pointer a SgStatement, and holds the statements in the "true" body of
 * if statement.
 *
 * #### `SgIfStmt::p_false_body`
 * This pointer a SgStatement, and holds the statements in the "false" body of
 * if statement.
 *
 * **Member functions**
 *
 * #### `SgIfStmt::copy(const SgCopyHelp & help)`
 * Makes a copy (deap of shallow depending on SgCopyHelp).
 * - Return: Returns pointer to copy of SgIfStmt.
 *
 * #### `SgIfStmt::get_conditional() const`
 * Access function for p_conditional.
 * - Return: Returns a pointer to a SgStatement.
 *
 * #### `SgIfStmt::set_conditional(SgStatement* conditional)`
 * Access function for p_conditional.
 * - Param `conditional`: SgStatement pointer
 * - Return: Returns void.
 *
 * #### `SgIfStmt::get_true_body() const`
 * Access function for p_true_body.
 * - Return: Returns a pointer to a SgStatement.
 *
 * #### `SgIfStmt::set_true_body(SgStatement* true_body)`
 * Access function for p_true_body.
 * - Param `true_body`: SgStatement pointer
 * - Return: Returns void.
 *
 * #### `SgIfStmt::get_false_body() const`
 * Access function for p_false_body.
 * - Return: Returns a pointer to a SgStatement.
 *
 * #### `SgIfStmt::set_false_body(SgStatement* false_body)`
 * Access function for p_false_body.
 * - Param `false_body`: SgStatement pointer
 * - Return: Returns void.
 *
 * #### `SgIfStmt::set_else_numeric_label(SgLabelRefExp* label)`
 * Access function for p_else_numeric_label (Fortran only).
 * - Param `lable`: used for end of if statement (Fortran only).
 * - Return: Returns void.
 *
 * #### `SgIfStmt::get_else_numeric_label()`
 * Access function for p_else_numeric_label (Fortran only).
 * - Param `lable`: used for end of if statement (Fortran only).
 * - Return: Returns SgLabelRefExp pointer.
 *
 * #### `SgIfStmt::set_end_numeric_label(SgLabelRefExp* label)`
 * Access function for p_end_numeric_label (Fortran only).
 * - Param `lable`: used for end of if statement (Fortran only).
 * - Return: Returns void.
 *
 * #### `SgIfStmt::get_end_numeric_label()`
 * Access function for p_end_numeric_label (Fortran only).
 * - Param `lable`: used for end of if statement (Fortran only).
 * - Return: Returns SgLabelRefExp pointer.
 *
 * #### `SgIfStmt::set_string_label(std::string label)`
 * Access function for p_string_label (Fortran only; string required to addess
 * generality of Fortran numeric and named labels).
 * - Param `label`: used for Fortran only.
 * - Return: Returns void.
 *
 * #### `SgIfStmt::get_string_label()`
 * Access function for p_string_label (Fortran only; string required to addess
 * generality of Fortran numeric and named labels).
 * - Return: Returns std::string.
 *
 * #### `SgIfStmt::get_has_end_statement()`
 * Fortran specific function to indicate if the Fortran "if" statement has an
 * "end" construct (C/C++ useage always returns false).
 * - Return: Returns bool.
 *
 * #### `SgIfStmt::set_has_end_statement(bool value)`
 * Fortran specific function to indicate if the Fortran "if" statement has an
 * "end" construct (C/C++ useage always returns false).
 * - Param `value`: to use in setting associated (Fortran specific) data member.
 * - Return: Returns void.
 *
 * #### `SgIfStmt::get_use_then_keyword()`
 * Fortran specific function to indicate if the Fortran "if" statement uses the
 * "then" construct (C/C++ useage always returns false).
 * - Return: Returns bool.
 *
 * #### `SgIfStmt::set_use_then_keyword(bool value)`
 * Fortran specific function to indicate if the Fortran "if" statement uses the
 * "then" construct (C/C++ useage always returns false).
 * - Param `value`: to use in setting associated (Fortran specific) data member.
 * - Return: Returns void.
 *
 * #### `SgIfStmt::get_is_else_if_statement()`
 * Fortran-specific function to indicate "else if" form of "if"
 * construct (C/C++ useage always returns false).
 * - Return: Returns bool.
 *
 * #### `SgIfStmt::set_is_else_if_statement(bool value)`
 * Fortran-specific function to indicate "else if" form of "if"
 * construct (C/C++ useage always returns false).
 * - Param `value`: to use in setting associated (Fortran specific) data member.
 * - Return: Returns void.
 */
class SgIfStmt;

/** @brief This class represents the notion of a declared variable.
 *
 * Each variable in the program has a SgInitializedName object which
 * represents its definition. A SgVariableDeclaration for example might
 * contain several SgInitializedName objects, while the
 * SgInitializedName contains the declaration for *one*
 * variable. Each variable use (VarRefExp for example) must have a link
 * to the SgInitializedName object where that specific variable was
 * defined.
 * A valid SgInitializedName records:
 * - the variable name which represents the variable that is being declared in
 * this class.
 * - the variable type
 * - the declaration object which contains the SgInitializedName ( this might be
 * a SgVariableDeclaration, SgFunctionParameterList, SgClassDeclaration,etc)
 * - See also:
 * Example of creating an SgInitializedName object
 * Example of using an SgInitializedName object
 *
 * **Data members**
 *
 * #### `SgInitializedName::p_fileInfo`
 * This pointer is always valid and stores the source position of the start a
 * name. This is an Sg_File_Info object which represents the source position of
 * the starting of the name represented by the SgInitializedName object
 * (variable name, function name, etc.).
 *
 * #### `SgInitializedName::p_name`
 * The variable that is declared in this declaration
 * This is a SgName object which represents the variable that is being declared
 * in this SgInitializedName object. For example, if there is a "int x"
 * declaration in the code, "x" is the variable that will be stored in
 * SgInitializedName::p_name as a SgName object.
 *
 * #### `SgInitializedName::p_typeptr`
 * Pointer to a type object that has been associated with
 * SgInitializedName::p_name This is a pointer that points to an SgType object
 * that represents the type the variable SgInitializedName::p_name declared in
 * this SgInitializedName class (in this declaration). For example, if the
 * variable is of type "int" (as in "int x"), then a SgTypeInt object has to be
 * allocated , and a pointer to this SgTypeInt object has to be stored in
 * SgInitializedName::p_typeptr to represent the type of
 * SgInitializedName::p_name.
 *
 * #### `SgInitializedName::p_initptr`
 * Pointer to an initializer for the variable.
 * In the case that the declaration contains an initializer for the variable,
 * for example "int x=5" or "int x=y" (in these cases the intializers for the
 * declared variable are "5" and "y"), that initializer has to be stored in
 * SgInitializedName::p_initptr as a pointer that points to the SgInitializer
 * object that the SgInitializedName::p_name variable is initialized with.
 *
 * #### `SgInitializedName::p_prev_decl_item`
 * Pointer to the initial uses of this variable previous to its redeclaration
 * declaration.
 * **Dan's** **intuitive** **explanation**
 * This pointer references any initialized name previously built to define a
 * SgVarRefExp, which requires a pointer to a variable declaration (but points
 * to a SgInitializedName since a SgVariableDeclaration could stand for many
 * variables (not just one, e.g. "int x,y,z;"), since uniqueness is required.
 * Although non-intuative, C++ code defined within a class definition can
 * reference variables before they are defined (e.g. "class X { int foo() {
 * return x; } int x; };"), see test2005_67.C (non-static data member) and
 * test2005_68.C (static data member). It is also used by a SgInitializedName in
 * a static declaration outside the class to refer to the preliminary
 * declaration inside the class. (e.g. "class X { static int a;}; int X::a = 0;
 * };")
 *
 * #### `SgInitializedName::p_declptr`
 * Pointer to the declaration object where this SgInitializedName object belongs
 * to.
 * **Alin's** **intuitive** **explanation**
 * Each SgInitializedName object contains the declaration of *one*
 * variable. In the case where the code has more declarations in a
 * single statement, for example "int x=5,y=3;", that declaration is
 * composed of two SgInitializedName objects, one for each variable
 * declared in that statment. So, this SgInitializedName::p_declptr points to
 * the statement (actually the declaration - SgDeclaration) that contains it.
 *
 * #### `SgInitializedName::p_storageModifier`
 * This is the storage modifier (static, auto, register, mutable, asm, etc.).
 * This is the storage modifier (static, auto, register, mutable, asm, etc.),
 * see complete list in source code for more details.  This is an implementation
 * of the modifier system as outlined in appendix A of Bjarne's book.
 * - Internal: Note that isStatic() in the SgInitializedName is always false, is
 * is set in the SgStorageModifier stored in the SgVariableDeclaration (where it
 * is filed of the SgDeclarationModifier).
 *
 * #### `SgInitializedName::p_scope`
 * This pointer is always valid and stores the current scope of the variable.
 * This is the current scope of the variable (required because variables can be
 * defined separately from their declaration).  See test2004_133.C (approx).
 *
 * #### `SgInitializedName::p_preinitialization`
 * This data member stores an enum value.
 * This value is set based on an enum value to indicate the type of use of the
 * initialized name (valid values are: virtual base class, non-virtual base,
 * data member).
 * - Internal: I think that the preinitalization information might be redundant
 * with the SgStorageModifier information.
 *
 * #### `SgInitializedName::p_register_name_string`
 * Exact target spelling of a GNU variable asm label. The spelling is not
 * normalized through an architecture-specific register enum.
 *
 * **Member functions**
 *
 * #### `SgInitializedName::get_storageModifier()`
 * returns a reference to the storage modifier
 *
 * #### `SgInitializedName::post_construction_initialization ()`
 * Allocates a new storage modifier and sets the storage modifier to default
 * values
 *
 * #### `SgNode * SgInitializedName::copy (const SgCopyHelp &help) const`
 * It clones the current SgInitializedName object recursively or not, depending
 * on the argument
 * - Param `help`: - If this argument is of type SgTreeCopy, then the
 * SgInitializedName is cloned recursively. If it's of type SgShallowCopy
 * only the first level of SgInitializedName is copied, everything else
 * pointing to the the original SgInitializedName object's data members.
 * - Return: a pointer to the new clone.
 *
 * #### `SgDeclarationStatement* SgInitializedName::get_declaration() const`
 * Returns the nearest declaration reached through the parent chain.
 * - Return: Pointer to SgDeclarationStatement
 *
 * #### `SgInitializedName::get_symbol_from_symbol_table() const`
 * **FOR** **INTERNAL** **USE** Get the associated symbol from the symbol table
 * in the stored scope. Users should use the
 * SgInitializedName::search_for_symbol_from_symbol_table() instead.
 *
 * #### `SgInitializedName::search_for_symbol_from_symbol_table() const`
 * User interface for retrieving the associated symbol. It searches through the
 * possible chain of prev_decl_item.
 */
class SgInitializedName;

/** @brief This class represents the notion of an initializer for a variable
 * declaration or
 *
 * expression in a function call argument list.
 * There are no direct uses of this IR node anywhere.  All initializers
 * are derived from this IR node to build derived classes such as
 * SgConstructorInitializer, SgAssignInitializer, and SgAggregateInitializer.
 * - Internal: This is a base class for all initializers.
 * - See also:
 * Example of using a SgInitializer object
 *
 * **Data members**
 *
 * #### `SgInitializer::p_is_explicit_cast`
 * This boolean variable marks the initializer ans part of an explicit or
 * implicit cast.  It is used for all of the different types of initalizers.
 * - Internal: This may now be duplicate information with the compiler generated
 * flag in Sg_File_Info.
 *
 * **Member functions**
 *
 * #### `SgInitializer::get_is_explicit_cast (void) const`
 * returns bool value if front-end considers this cast as explicit.
 * - Return: Returns bool value.
 *
 * #### `SgInitializer::set_is_explicit_cast (bool explicit_cast)`
 * This function allows the p_is_explicit_cast flag to be set (used internally).
 * - Param `explicit_cast`: - sets value of internal p_is_explicit_cast flag
 * (true if explicit).
 * - Return: Returns void.
 */
class SgInitializer;

/** @brief This class represents the concept of a C or C++ label statement.
 *
 * A C or C++ label owns its associated statement through its statement edge.
 * The enclosing scope owns the label, rather than also owning that statement
 * as a sibling. Use a null statement when the source requires an empty labeled
 * statement. Transformations must preserve this ownership relationship.
 *
 * **Data members**
 *
 * #### `SgLabelStatement::p_label`
 * This a SgName object which stores the name of the label.
 *
 * **Member functions**
 *
 * #### `SgLabelStatement::get_label() const`
 * Returns SgName by value.
 * - Return: Returns SgName.
 *
 * #### `SgLabelStatement::set_label(SgName label)`
 * Access function for p_label.
 * - Return: Returns void.
 */
class SgLabelStatement;

/** @brief This class represents the notion of an expression or statement which
 *
 * has a position within the source code.
 * There are no uses of this IR node anywhere.  All expressions and statements
 * are derived from this IR node to build either SgExpression or SgStatement
 * derived classes.
 * - Note: SgLocatedNode objects have a set_startOfConstruct() and a
 * set_endOfConstruct() these must be set explicitly to define a proper AST. The
 * set_startOfConstruct() will be called if the constructor taking a
 * Sg_File_Info object is called (depreicated) but the set_endOfConstruct()
 * should be called explicitly.
 * - Internal: The AttachedPreprocessingInfoType *p_attachedPreprocessingInfoPtr
 * and the AstAttributeMechanism *p_attribute are implemented as pointers
 * because they would take up 12 bytes each as STL lists and that would be
 * wasteful when we have 20 million IR nodes held in memory.  This is a
 * violation of  general rule in the IR design that we have STL containers as
 * data member instead of pointers to data members (the file I/O handles this
 * as a special case).
 * - Todo: The AstAttributeMechanism type should be handed as other IR nodes
 * with it's own memory pool, except that in all cases where it would be used,
 * it would be a base class to a user-defined derived type and thus would not
 * fix in our memory pool.
 * - Todo: Consider name change of "SgLocatedNode" to "SgSourceNode".
 * - Todo: Consider moving some of the IR nodes currently in SgSupport to this
 * IR node. IR nodes that might be moved would include:
 * -# SgFile
 * -# SgDirectory (questionable)
 * -# SgInitializedName
 * -# SgBaseClass
 * -# SgTemplateArgument
 * -# SgTemplateParameter
 * - Internal: This is a base class for all expressions and statements.
 * - See also:
 * Example of using a SgLocatedNode object
 *
 * **Data members**
 *
 * #### `SgLocatedNode::p_startOfConstruct`
 * This pointer is always valid and stores the source position of the start of
 * the current construct.
 *
 * #### `SgLocatedNode::p_endOfConstruct`
 * This pointer is always valid and stores the source position of the end of the
 * current construct.
 *
 * #### `SgLocatedNode::p_attachedPreprocessingInfoPtr`
 * Holds comments and/or preprocessor directives located before or after the
 * current statement of expression.
 *
 * **Member functions**
 *
 * #### `SgLocatedNode::SgLocatedNode (const SgLocatedNode &X)`
 * Copy constructor (made private to prevent being called by accident).
 *
 * #### `SgLocatedNode::getFileName()`
 * Access function for "get_file_info()->get_filename()".
 * This function is just a simpler access function which retrives the
 * filename from the Sg_File_Info object at the current IR node.  This
 * is only a convience function an as a result not a great idea.
 * - Internal: This function should likely be removed from the SAGEIII
 * interface.
 * - Deprecated: This function should be removed because it is only an interface
 * function of rather minor significance. Alternatively if we want to implement
 * it, we should consider placing it at the SgNode object. If we do preserve the
 * function then we should have it return a string instead of char*.
 *
 * #### `SgLocatedNode::addToAttachedPreprocessingInfo (PreprocessingInfo
 * *prepInfoPtr,` PreprocessingInfo::RelativePositionType
 * locationInList=PreprocessingInfo::after) This function adds comment or CPP
 * directives to the current IR node.
 * - Param `prepInfoPtr`: - This parameter is a pointer to the container for the
 * comment/directive. It is used to specify if the new  new comment/directive is
 * added to the front or back of the current list of comments/directives. It
 * does not change relative position field of the PreprocessingInfo object.
 * - Param `locationInList`: - adds container before or after the current IR
 * node.
 * - Return: Returns void.
 *
 * #### `SgLocatedNode::getAttachedPreprocessingInfo (void)`
 * Returns the attached comment and preprocessing-directive container by
 * reference to its pointer.
 *
 * #### `SgLocatedNode::set_startOfConstruct (Sg_File_Info *startOfConstruct)`
 * This function sets the current source location position of the start
 * of the current construct.
 * - Param `startOfConstruct`: - Pointer to Sg_File_Info object containing
 * source location information.
 * - Return: Returns void.
 *
 * #### `SgLocatedNode::set_endOfConstruct (Sg_File_Info *endOfConstruct)`
 * This function sets the current source location position of the end
 * of the current construct.
 * - Param `endOfConstruct`: - Pointer to Sg_File_Info object containing source
 * location information.
 * - Return: Returns void.
 *
 */
class SgLocatedNode;

/** @brief This class represents the notion of an value (expression value).
 *
 * - See also:
 * Example of using a SgLongDoubleVal object
 *
 * **Data members**
 *
 * #### `SgLongDoubleVal::p_value`
 * This value holds the double represented in the source code.
 *
 */
class SgLongDoubleVal;

/** @brief This class represents the concept of a member function declaration
 * statement.
 *
 * The member function declaration is derived from a function declaration and
 * adds specific data members that are relavant.
 * - Note: Constructors, destructors, and conversion operators are handled as
 * member fucntions with names associated with their classes.
 * - Internal: The scope can at times be that of the global scope, when this
 * happens users should access the scope through
 * get_firstNondefiningDeclaration(). This appears to be a bug internally.
 *
 * **Data members**
 *
 * #### `SgMemberFunctionDeclaration::p_CtorInitializerList`
 * This is the constructor preinitialization list (used only for the constructor
 * definitions).
 * - Internal: This data member might make more sense in the
 * SgFunctionDefinition (or we could build a SgMemberFunctionDefinition from the
 * SgFunctionDefinition, and put the constructor initialization list there).
 *
 * **Member functions**
 *
 * #### `SgMemberFunctionDeclaration::get_CtorInitializerList() const`
 * Access function for p_CtorInitializerList.
 * - Return: Returns pointer to SgCtorInitializerList.
 *
 * ####
 * `SgMemberFunctionDeclaration::set_CtorInitializerList(SgCtorInitializerList*
 * CtorInitializerList)` Access function for p_CtorInitializerList.
 * - Return: Returns void.
 */
class SgMemberFunctionDeclaration;

/** @brief This class represents the member function being called and must be
 * assembled in the
 *
 * SgFunctionCall with the function arguments.
 * - Internal:
 * - See also:
 * Example of using a SgMemberFunctionRefExp object
 *
 * **Data members**
 *
 * #### `SgMemberFunctionRefExp::p_symbol_i`
 * This is the SgMemberFunctionSymbol.
 * This binds the function reference to the member function declaration.
 * Note that all SgSymbol objects are shared (just like SgType objects)
 *
 * #### `SgMemberFunctionRefExp::p_virtual_call`
 * This boolean value marks if the function reference is a virtual function.
 *
 * The semantic function type is obtained from the exact member symbol.
 *
 * #### `SgMemberFunctionRefExp::p_need_qualifier`
 * This boolean value marks if name qualification is required.
 *
 * **Member functions**
 *
 * #### `SgMemberFunctionRefExp::get_type() const`
 * Returns the exact type associated with the referenced symbol.
 *
 */
class SgMemberFunctionRefExp;

/** @brief This class represents the numeric negation of a value. Not to
 *
 * be confused with SgSubtractOp
 */
class SgMinusOp;

/** @brief This class represents the base class of a number of IR nodes define
 * modifiers
 *
 * within the C++ grammar.
 * Modifiers are use to add suplimental information to types and declarations
 * and other IR nodes within the C and C++ language (const, volatile, public,
 * protected, private, etc.). There are no uses of this IR node anywhere.  All
 * SgModifier based IR nodes are derived from this SgModifier class.
 * - Internal: This is a base class for all SgModifier objects.
 * - See also:
 * Example of using a SgModifier object
 *
 */
class SgModifier;

/** @brief Holds the modifier-type cache buckets for a base type. */
class SgModifierNodes;

/** @brief String-backed name value used by AST nodes.
 *
 * Stores a `std::string` and provides conversions to `std::string` and a
 * null-terminated view through `str()`.
 */
class SgName;

/** @brief This class represents the concept of a C++ namespace alias
 * declaration statement.
 *
 * For a namespace Y, this appears in the code as "namespace X = Y;".  In
 * general "X" is a shorter name for what in "Y" might be unwieldy.
 * - See also:
 * Example of using a SgNamespaceAliasDeclarationStatement object
 *
 * **Data members**
 *
 * #### `SgNamespaceAliasDeclarationStatement::p_name`
 * This the name of the new namespace alias (usually a shorter name).
 *
 * #### `SgNamespaceAliasDeclarationStatement::p_namespaceDeclaration`
 * This the namespace to which the alis references.
 *
 * **Member functions**
 *
 * #### `SgNamespaceAliasDeclarationStatement::get_name() const`
 * Access function for p_name.
 * - Return: Returns SgName.
 *
 * #### `SgNamespaceAliasDeclarationStatement::set_name(SgName name)`
 * Access function for p_name.
 * - Return: Returns void.
 *
 * #### `SgNamespaceAliasDeclarationStatement::get_namespaceDeclaration() const`
 * Access function for p_namespaceDeclaration.
 * - Return: Returns SgNamespaceDeclarationStatement.
 *
 * ####
 * `SgNamespaceAliasDeclarationStatement::set_namespaceDeclaration(SgNamespaceDeclarationStatement*
 * namespaceDeclaration)` Access function for p_namespaceDeclaration.
 * - Return: Returns void.
 */
class SgNamespaceAliasDeclarationStatement;

/** @brief This class represents the concept of a C++ namespace declaration.
 *
 * The namespace declaration is differernt from many other declarations in that
 * there is not concept of a namespace declaration that does not have an
 * associated definition (SgNamespaceDefinitionStatement). In a sense each
 * SgNamespaceDeclarationStatement is a defining declaration, yet there can be
 * more than one of them, so this concept of a defining declaration doesn't fit
 * were we consider that there must be only a single defining declaration under
 * the One-time Definition Rule (ODR).  This we don't consider that
 * SgNamespaceDeclarationStatement as a defining declaration.  As a result:
 * - The defining declaration returned by get_definindDeclaration() is always
 * NULL, and
 * - The get_firstNondefiningDeclaration() member function will:
 * - Always return a valid pointer, and
 * - Will point to the first declaration of SgNamespaceDeclarationStatement
 * The scope of SgNamespaceDeclarationStatement is computed structurally from
 * the parent information.
 * - Todo: Consider having a function which could generate a list of all the
 * SgNamespaceDeclarationStatement IR nodes that match the same namspace.
 * This would make a good first project for a new student.
 * - Todo: Include a graph to show how scopes are handled within the AST.
 * - See also:
 * Example of using a SgNamespaceDeclarationStatement object
 *
 * **Data members**
 *
 * #### `SgNamespaceDeclarationStatement::p_name`
 * This the name of the new namespace alias (usually a shorter name).
 *
 * #### `SgNamespaceDeclarationStatement::p_definition`
 * This pointer points to the SgNamespaceDefinitionStatement, which holds the
 * declarations within the namespace.
 *
 * #### `SgNamespaceDeclarationStatement::p_isUnnamedNamespace`
 * Records special case of an unnamed namespace.
 * - Internal: Marking the IR nodes as unnamed is better than just having an
 * empty string for p_name.  This provides a reasonable mechanism to implement
 * error checking internally.
 *
 * #### `SgNamespaceDeclarationStatement::p_scope`
 * This a pointer to the scope of the first declaration for the namespace.
 *
 * **Member functions**
 *
 * #### `SgNamespaceDeclarationStatement::get_name() const`
 * Access function for p_name.
 * - Return: Returns SgName.
 *
 * #### `SgNamespaceDeclarationStatement::set_name(SgName name)`
 * Access function for p_name.
 * - Return: Returns void.
 *
 * #### `SgNamespaceDeclarationStatement::get_definition() const`
 * Returns pointer to SgNamespaceDefinitionStatement.
 * - Return: Returns pointer to SgNamespaceDefinitionStatement.
 *
 * ####
 * `SgNamespaceDeclarationStatement::set_definition(SgNamespaceDefinitionStatement*
 * definition)` Access function for p_definition.
 * - Return: Returns void.
 *
 * #### `SgNamespaceDeclarationStatement::get_isUnnamedNamespace() const`
 * Access function for p_isUnnamedNamespace.
 * - Return: Returns SgName.
 *
 * #### `SgNamespaceDeclarationStatement::set_isUnnamedNamespace(bool
 * isUnnamedNamespace)` Access function for p_isUnnamedNamespace.
 * - Return: Returns void.
 */
class SgNamespaceDeclarationStatement;

/** @brief This class represents the concept of a namespace definition.
 *
 * Namespace definitions
 * are coupled with namespace declarations to defin the namespace (hold the list
 * of declarations in the namespace.  Within C++ namespaces are "reentrant" and
 * as a result multiple namespace declarations (SgNamespaceDeclarationStatement)
 * and definitions (SgNamespaceDefinitionStatement) may exist for a single
 * namespace.
 * - Note: Note that the namespace "std" is special in C++, such that a program
 * with the statement "using namespace std" can exist all by itself and is a
 * valid program (even though "std" as a namespace is not defined).
 *
 * **Data members**
 *
 * #### `SgNamespaceDefinitionStatement::p_declarations`
 * This is an STL list of SgDeclarationStatement objects.
 * - Internal: The name is perhaps all to similar to p_declaration (which exists
 * on many IR nodes).
 *
 * #### `SgNamespaceDefinitionStatement::p_namespaceDeclaration`
 * This is a pointer to the SgNamespaceDeclarationStatement.
 * - Internal: The name would have been p_declaration, except that such a name
 * is too close to p_declarations.  These naming details might be fixed later.
 *
 * **Member functions**
 *
 * #### `SgNamespaceDefinitionStatement::get_qualified_name() const`
 * Returns SgName (a string) representing the name of the current scope.
 * See discussion of mangled names in the documentation.
 * - Return: Returns SgName (a string).
 *
 * #### `SgNamespaceDefinitionStatement::copy(const SgCopyHelp & help)`
 * Makes a copy (deap of shallow depending on SgCopyHelp).
 * - Return: Returns pointer to copy of SgNamespaceDefinitionStatement.
 */
class SgNamespaceDefinitionStatement;

/** @brief This class represents the concept of a namespace name within the
 * compiler.
 *
 * This symbol holds a reference to a SgNamespaceDeclarationStatement and also
 * any of its alias (referencing a SgNamespaceAliasDeclarationStatement).
 * - Internal: The constructor for this symbol takes a name and a declaration.
 * The reason is that it is used to handle the case of a namespace alias which
 * uses the same symbol but uses a different name.  I'm not certain that this is
 * the best implementation since it stores the symbol in the symbol table under
 * two names.  It is not clear if that would be clear if the name were changes
 * and this could cause the symbol table to not be completely removed from the
 * symbol table.
 * - Todo: It might be that we should have a SgNamespaceAliasSymbol so that
 * namespace aliasing can be better supported.  We can consider this for future
 * work.
 * - See also: SgSymbol for more details about symbols and why they are used in
 * ROSE.
 * - See also:
 * Example of using a SgNamespaceSymbol object
 *
 * **Member functions**
 *
 * #### `SgNamespaceSymbol::get_name() const`
 * Access function for getting name stored internally (not the one in the
 * declaration).
 * - Return: Returns SgName.
 *
 * #### `SgNamespaceSymbol::get_type() const`
 * This function returns the type associated with the named entity.
 * - Return: Returns SgType*.
 *
 * #### `SgNamespaceSymbol::get_declaration() const`
 * Access function for getting the declaration of the original namespace.
 * - Return: Returns SgNamespaceDeclarationStatement.
 *
 * #### `SgNamespaceSymbol::set_declaration(SgNamespaceDeclarationStatement
 * *declaration)` Access function for setting the declaration stored internally.
 * - Return: Returns SgNamespaceDeclarationStatement.
 *
 */
class SgNamespaceSymbol;

/** @brief This class represents the concept of a C++ call to the new operator.
 *
 * - Todo: Provide some examples to detail the difference between placement,
 * constructor, and builtin arguments.
 * - Todo: I believe we can associate the constructors from the class with
 * new operators.
 * - Internal: This is one of the few SgExpression IR nodes that are required to
 * store the internal type explicitly.  In this case the p_specified_type is
 * used and it is set to the type specified in the new expression (e.g. int *x =
 * new int[1000];, the p_specified_type would be SgTypeInt.  However the return
 * type of the new operator is a pointer to this type so the get_type() member
 * function returns a pointer to this type.
 * - See also:
 * Example of using a SgNewExp object
 *
 * **Data members**
 *
 * #### `SgNewExp::p_specified_type`
 * This pointer points to the type of the variable being allocated.
 *
 * #### `SgNewExp::p_placement_args`
 * This is a pointer to the memory allocation placement arguments for the new
 * operator.
 * An example would be "class X{}; void* p; X* item2 = new (p) X();".  Placement
 * arguments permit the new opperator to allocate space at a specific location
 * (address).
 * - Todo: Provide some examples to detail the difference between placement,
 * constructor, and builtin arguments (these are rarely used in C++).
 *
 * #### `SgNewExp::p_constructor_args`
 * This is a pointer to the constructor initializer (which holds its constructor
 * arguments) for the new operator.
 * - Todo: Provide some examples to detail the difference between placement,
 * constructor, and builtin arguments.
 *
 * #### `SgNewExp::p_builtin_args`
 * This is a pointer to the builtin arguments for the new operator (typically
 * the "this" pointer where specified optionally).
 * - Todo: Provide some examples to detail the difference between placement,
 * constructor, and builtin arguments.
 *
 * #### `SgNewExp::p_need_global_specifier`
 * This new operator needs to be output with "::".
 * - Internal: This should be a bool instead of a short.
 *
 * **Member functions**
 *
 * #### `SgNewExp::get_need_global_specifier() const`
 * Access function for p_need_global_specifier.
 * - Return: Returns bool.
 *
 * #### `SgNewExp::set_need_global_specifier(short
 * need_global_specifier)` Access function for p_need_global_specifier.
 * - Param `need_global_specifier`: flag to record use of array delete.
 * - Return: Returns void.
 */
class SgNewExp;

/** @brief This class represents the base class for all IR nodes within Sage
 * III.
 *
 * This class is used as a base class for all IR nodes in Sage III.
 * - Note: AST attributes should not be used for the following IR nodes:
 * -# SgName
 * -# SgModifier (and all derived modifier IR nodes)
 * -# SgUnparse_Info
 * -# SgSymbolTable
 * - Internal: Need to remove unused error() and variant() functions.
 * - Todo: Consider eliminating the set_freepointer() function since only the
 * internal memory allocation mechanisms should use it (and they are forced to
 * access the data member directly since they traverse the memory pools directly
 * and member function can onl be called on allocated object initialized vi the
 * new operator (with a proper constructor call, so that the this pointer is set
 * properly)).  Perhaps we don't need access functions for this data member at
 * all.
 * - See also:
 * Example of using a SgNode object
 * - See also:
 * Enforced AST Properties
 *
 * **Data members**
 *
 * #### `SgNode::p_parent`
 * This is the pointer to the parent IR node in the AST.
 * Owning AST edges have reciprocal parent links when the producer publishes
 * the node. Semantic references to shared types and declarations are distinct
 * from owning edges; a reference does not reparent its target. Symbols belong
 * to their symbol tables.
 *
 * #### `SgNode::p_freepointer`
 * This is the pointer to the chain of previously freed objects.
 * - Internal: This is part of the support for memory pools within ROSE.  The
 * freepointer is only manipulated by the delete operator which constructs a
 * chain of previously freed objects embedded within the memory pools.  The
 * chain of objects link by the freepointer variables are traversed by the new
 * operator to allocate (reuse) previously deleted objects. The new operator
 * does not reset the freepointers since once allocated the freepointer is
 * ignored.
 *
 * #### `SgNode::p_isModified`
 * Records if IR node has been modified (data members reset).
 * This is part of an internal mechanism that records if IR nodes have
 * been modified, either by a transformation or a access function to set
 * a value on the IR node.  All access functions that set IR node data members
 * are automatically generated (except the ones for this data member) and
 * include code to set this boolean flag to true.  This is also part of the
 * support for copy based code generation where source file is copied except
 * where the AST was modified an for these subtrees new code is generated
 * from the AST.
 * - Internal: true if IR node has been modified, else false.
 *
 * #### `SgNode::p_globalFunctionTypeTable`
 * Pointer to symbol table specific to function types.
 * - Internal: Always a valid pointer.
 *
 * #### `SgNode::p_globalMangledNameMap`
 * Cache of mangled names to avoid regeneration of previously build mangled
 * names or parts of mangled names.  This is a performance optimization.
 * - Internal: This cache should be cleared automatically where required.
 *
 * #### `SgNode::p_shortMangledNameCache`
 * STL map used as a cache to shorten generated mangled names.
 * This is mostly a space optimization ofr mangled names of templates.
 * - Internal: This cache should be cleared before regenerating mangled names.
 *
 * **Member functions**
 *
 * #### `SgNode::copy(const SgCopyHelp & help ) const`
 * This function clones the current IR node object recursively or not, depending
 * on the argument This function performs a copy based on the specification of
 * the input parameter. The input parameter is used to determin which data
 * members are copied by reference and which are copied by value.
 * - Param `help`: - If this argument is of type SgTreeCopy, then the
 * IR node is cloned recursively. If its of type SgShallowCopy
 * only the first level of the IR node is copied, everything else is
 * left pointing to the the original IR node's object's data members.
 * - Return: a pointer to the new clone.
 * Copy construction establishes the copied owning edges before publication.
 * Shared semantic identities remain references according to SgCopyHelp's copy
 * policy. AstPostProcessing does not repair missing ownership in a copy.
 *
 * #### `SgNode::SgNode()`
 * This is the constructor
 * This constructor builds a SgNode, always used as a base class.
 * All Sage III IR nodes are derived from this class.
 * - See also:
 * Example:create an SgNode object
 *
 * #### `SgNode::sage_class_name() const`
 * generates string representing the class name: (e.g. for SgNode returns
 * "SgNode"). This function is useful for debugging and error reporting.  It
 * returns the name of the IR node.
 * - Deprecated: Use class_name() which returns a C++ string object.
 * - Return: a char* pointer to a C style string.
 *
 * #### `string SgNode::class_name() const`
 * generates string representing the class name: (e.g. for SgNode returns
 * "SgNode"). This function is useful for debugging and error reporting.  It
 * returns the name of the IR node.
 * - Return: a C++ string object.
 *
 * #### `SgNode::variant() const`
 * Older version function returns enum value "NODE"
 * This function is an older version of the variant function.
 * This function is useful for debugging and error reporting.  It
 * returns the name of the IR node.
 * - Return: an int value.
 * - Deprecated: Use variantT() instead.  Older enum values have inconsistant
 * names and are being removed.
 *
 * #### `SgNode::variantT() const`
 * returns enum value "V_SgNode"
 * This function is useful for debugging and error reporting.  It
 * returns the name of the IR node.
 * Once the variant() function is removed this function will be
 * deprecated and replaced by a new function named "variant()".  The new
 * "variant()" function will return the new enum type (with consistant naming
 * convention in place). The name of the enum type might be changed to make it
 * more clear.
 * - Return: an enum value (type: VariantT).
 *
 * #### `SgNode::get_isModified() const`
 * **FOR** **INTERNAL** **USE** All nodes in the AST contain a isModified flag.
 * This flag can be set but this is typically an internal function used to track
 * the updates to AST.
 * - Return: Returns bool; true if IR node has been modified.
 *
 * #### `SgNode::set_isModified(bool isModified)`
 * Acess function for isModified flag.
 * This flag records if the current IR node has been modified. It is set to
 * false after and ROSE front-end processing.
 *
 * #### `SgNode::set_parent (SgNode *parent)`
 * Sets parent node for any IR node.
 * The parent node in the AST can be set but this is typically
 * an internal function used to build the AST.
 * - Param `parent`: - Pointer to parent node to store within the current IR
 * node.
 * - Return: returns void.
 *
 * #### `SgNode::get_parent() const`
 * Access function for parent node in AST.
 * Returns the structural owner established during construction. Shared
 * semantic references do not change the referenced node's parent. Semantic
 * scope is represented separately where it differs from structural ownership.
 * - Return: Returns pointer to SgNode
 *
 * #### `SgNode::unparseToString( SgUnparse_Info* info )`
 * This function unparses the AST node (source code only, excluding comments and
 * white space)
 * - Param `info`: is optional (used only to specify code generation options).
 * This function is useful for debugging. It uses SgUnparse_Info as an inherited
 * attribute internally and using this attribute it will correctly handled many
 * subtle details that will be ignored if the attribute is not provided.  For
 * example, the SgUnparse_Info can record if the statement is in a conditional
 * and if so then the trailing ";" will be omitted in the generated code. See
 * the SgUnparse_Info documentation for the numerous other internal settings
 * that can effect the generated code.  Because of these details, the
 * unparseToString() function can not always be used to generate compiliable
 * code.
 * - Return: Returns std::string
 *
 * #### `SgNode::unparseToCompleteString()`
 * This function unparses the AST node (including comments and white space)
 * This function is a complement to the unparseToString() function and includes
 * any associated comments and preprocessor control directives.  Because C
 * preprocessor control directives can be included, the resulting string may
 * not be a complete translation unit.
 * - Todo: This function needs a better name since it is unclear what the
 * "complete" string is.
 * - Return: Returns std::string
 *
 * #### `SgNode::get_traversalSuccessorContainer()`
 * **FOR** **INTERNAL** **USE** within ROSE traverals mechanism only.
 * This function builds and returns a copy of ordered container
 * holding pointers to children of this node in a traversal. It is
 * associated with the definition of a tree that is travered by the
 * AST traversal mechanism; a tree that is embeded in the AST (which
 * is a more general graph).  This function is used within the implementation
 * of the AST traversal and has a semantics may change in subtle ways
 * that makes it difficult to use in user code.  It can return unexpected
 * data members and thus the order and the number of elements is unpredicable
 * and subject to change.
 * - Warning: This function can return unexpected data members and thus the
 * order and the number of elements is unpredicable and subject
 * to change.
 * - Return: Returns ordered STL Container of pointers to children nodes in AST.
 *
 * #### `SgNode::get_traversalSuccessorNamesContainer()`
 * **FOR** **INTERNAL** **USE** within ROSE traverals mechanism only.
 * This function builds and returns a copy of ordered container
 * holding strings used to name data members that are traversed in the IR
 * node. It is associated with the definition of a tree that is travered by the
 * AST traversal mechanism; a tree that is embeded in the AST (which
 * is a more general graph).  This function is used within the implementation
 * of the AST traversal and has a semantics may change in subtle ways
 * that makes it difficult to use in user code.  It can return unexpected
 * data members and thus the order and the number of elements is unpredicable
 * and subject to change.
 * - Warning: This function can return unexpected data members and thus the
 * order and the number of elements is unpredicable and subject
 * to change.
 * Each string is a name of a member variable holding a pointer to a
 * child in the AST. The names are the same as used in the generated enums for
 * accessing attributes in a traversal. The order is the same in which they are
 * traversed and the same in which the access enums are defined. Therefore this
 * method can be used to get the corresponding name (string) of an access enum
 * which allows to produce more meaningful messages for attribute computations.
 * - Return: Returns ordered STL container of names (strings) of access names to
 * children nodes in AST.
 *
 * #### `SgNode::roseRTI ()`
 * **FOR** **INTERNAL** **USE** Access to Runtime Type Information (RTI) for
 * this IR nodes. This function provides runtime type information for accessing
 * the structure of the current node.  It is useful for generating code which
 * would dump out or rebuild IR nodes.
 * - Return: Returns a RTIReturnType object (runtime type information).
 */
class SgNode;

/** @brief This class represents an object used to initialize the unparsing.
 *
 * There are no uses of this IR nodes anywhere in the AST.
 * - Internal: This is an object used only internally.
 * - Todo: Review if we really want to keep this class.
 * - See also:
 * Example of using a SgOptions object
 *
 */
class SgOptions;

/** @brief This class represents a Fortran pointer assignment. It is not
 *
 * some weird compound assignment operator like C's +=.
 */
class SgPointerAssignOp;

/** @brief A pragma declaration containing an SgPragma node.
 *
 * - See also:
 * Example of using a SgPragmaDeclaration object
 *
 */
class SgPragmaDeclaration;

/** @brief This class represents a source project, with a list of SgFile objects
 * and global
 *
 * information about the project (commandline details, AST merge options, etc.).
 * This IR nodes contains list of files, etc. after internal command-line
 * processing. The original argc and argv are not modified and left in tact for
 * processing by the ROSE translator.  The SgProject node keeps a deep copy of
 * the original argc and argv as an STL list of strings (representing the
 * command-line arguments). SgFile::stripRoseCommandLineOptions operates on a
 * vector of argument strings to remove REX options before backend invocation.
 * - Internal: This IR node does not appear many times in an AST (typically only
 * once).
 * - Todo: Make the "*PtrList" typed objects non-pointer data members (lists)
 * instead of pointer to lists.
 * - Todo: Implement a "-dumpversion" for compatibility with GNU (icc does the
 * same).
 * - See also:
 * Example of using a SgProject object
 *
 * **Data members**
 *
 * #### `SgProject::p_fileList_ptr`
 * Points to the SgFileList holding the project's files.
 *
 * #### `SgProject::p_verbose`
 * This is the level of verbosity assumed to apply to the whole project (all AST
 * processing).
 * - Internal: This is a static variable, which might make it difficult to
 * multiple SgProject IR nodes to coexist within the same problem.
 *
 * #### `SgProject::p_originalCommandLineArgumentList`
 * Copy of original argc and argv command line passed to ROSE translator
 * (converted to STL list of strings).
 * - Internal: This is a deep copy.
 *
 * #### `SgProject::p_frontendErrorCode`
 * Error status from frontend processing; successful processing returns zero.
 *
 * #### `SgProject::p_backendErrorCode`
 * Error code returnd from processing of generated source code using vendor
 * compiler. This value should be passed back out of the main() function by the
 * user translator (for support of makefile processing).
 * - Internal: This error code is returned by the ROSE backend() function.
 *
 * #### `SgProject::p_outputFileName`
 * Filename specific using "-o" option on command line.
 * Set as part of normal command line processing within ROSE.
 * - Internal: This is set to a default value of "a.out" if "-o" is not
 * specified.
 *
 * #### `SgProject::p_sourceFileNameList`
 * List of all source file names specified on the command line.
 * - Internal: This can be an empty list (if none are specified). This should be
 * updated by the AST Merge mechanism.
 *
 * #### `SgProject::p_objectFileNameList`
 * List of all object files specified on the command line.
 * - Internal: This can be an empty list (if none are specified).
 *
 * #### `SgProject::p_libraryFileList`
 * List of all libraries specified on command line.
 * List all libraries specified using ".a" or ".so" syntax.
 * - Internal: Required to assemble link lines for backend vendor compiler.
 *
 * #### `SgProject::p_librarySpecifierList`
 * List of libraries specified using "-lxxx" syntax.
 * - Internal: Required to assemble link lines for backend vendor compiler.
 *
 * #### `SgProject::p_libraryDirectorySpecifierList`
 * List of directories specified with "-L" option on command line
 * - Internal:
 *
 * #### `SgProject::p_includeDirectorySpecifierList`
 * List of directories specified with "-I" option on command line
 * - Internal:
 *
 * #### `SgProject::p_compileOnly`
 * This is true when "-c" requests compilation without linking.
 *
 * #### `SgProject::p_openmp_linking`
 * This flag is used to indicate if OpenMP lowering is requested by the command
 * line so linking to ROSE's OpenMP runtime library is needed. This flag is set
 * to be false by default.
 * "-rose:openmp:lowering" triggers this flag to be set to true.
 * SgFile::p_openmp_lowering will not be sufficient since there is no
 * SgFile in the AST when a ROSE translator is used as a linker wrapper.
 * So we have to have a project level flag to indicate the need.
 * Using a flag like SgProject::p_OpenMP_Only won't be sufficient neither
 * since OpenMP input files can be handled in three different ways in ROSE:
 * 1) parsing only 2) generating dedicated AST 3) actual OpenMP lowering
 * Only the one with lowering will need special linking support to connect to
 * libxomp.a and pthreads.
 *
 * **Member functions**
 *
 * #### `list<string> SgProject::get_originalCommandLineArgumentList() const`
 * Returns a list of strings representing the original command-line.
 *
 * #### `SgProject::set_originalCommandLineArgumentList( list<string>
 * originalCommandLineArgumentList )` Sets the list of strings representing the
 * original command-line.
 *
 * #### `bool SgProject::get_C99_only (void) const`
 * This controls the c99 mode in the frontend.
 */
class SgProject;

/** @brief This class represents the "&" operator (applied to any lvalue).
 *
 * - Todo: Double check this description.
 * - Internal:
 * - See also:
 * Example of using a SgRefExp object
 *
 * **Data members**
 *
 * #### `SgRefExp::p_type_name`
 * This a SgType, but I forget its significance.
 *
 */
class SgRefExp;

/** @brief A return statement with an optional expression.
 *
 * - See also:
 * Example of using a SgReturnStmt object
 *
 */
class SgReturnStmt;

/** @brief Represents the qualified name in a C++ pseudo-destructor call. */
class SgScopeOp;

/** @brief This class represents the concept of a scope in C++ (e.g. global
 * scope, fuction scope, etc.).
 *
 * Scopes are an important aspect of language design. They allow
 * declarations to have a local context and so promote good programming style.
 * Scope statments in C++ include a number of different kinds of statements;
 * the SgScopeStatement is a base class for these. Each scope statement contains
 * a symbol table and the SgScopeStatements role is mostly to provide this
 * symbol table and an interface to accessing it.
 * - Internal: This is a base class for scope statements.
 * Control-flow bodies use SgStatement edges. Conditions may be expressions
 * wrapped in expression statements or declaration statements.
 *
 * **Data members**
 *
 * #### `SgScopeStatement::p_symbol_table`
 * This pointer is always valid and stores the symbl table.
 * - Internal: The symbol table implementation was changed in fall of 2004, by
 * Alin. It is not simple and leverages STL and provides more features.
 *
 * **Member functions**
 *
 * #### `SgScopeStatement::get_symbol_table()`
 * Returns a pointer to the locally strored SgSymbolTable.
 * - Return: Returns a pointer.
 *
 * #### `SgScopeStatement::set_symbol_table(SgSymbolTable *symbolTable)`
 * Sets the pointer to the locally strored SgSymbolTable.
 * - Return: Returns void.
 *
 * #### `SgScopeStatement::get_qualified_name() const`
 * Returns SgName (a string) representing the name of the current scope.
 * See discussion of mangled names in the documentation.
 * - Return: Returns SgName (a string).
 *
 * #### `SgScopeStatement::copy(const SgCopyHelp & help)`
 * Makes a copy (deap of shallow depending on SgCopyHelp).
 * - Return: Returns pointer to copy of SgScopeStatement.
 *
 * #### `SgScopeStatement::insert_symbol(const SgName & name, SgSymbol *symbol)`
 * Puts a SgSymbol object into the local symbol table.
 * - Return: Returns void.
 *
 * #### `SgScopeStatement::lookup_symbol(const SgName & name)`
 * returns boolean value if symbol exists.
 * - Return: Returns SgSymbol pointer.
 *
 * #### `SgScopeStatement::containsOnlyDeclarations()`
 * boolean member function to distinquish if getStatementList() or
 * getDeclarationList() is appropriate for the SgScopeStatment IR node.
 * - Return: bool.
 *
 * #### `SgScopeStatement::getDeclarationList()`
 * Gets reference to internal STL list of pointers to SgDeclarationStatement
 * objects (only defined for scopes containing declarations, see bool
 * containsOnlyDeclarations()). This function is useful for SgScopeStatement
 * objects that contain declarations, including SgClassDefinition,
 * SgTemplateInstantiationDefn, SgGlobal, and SgNamespaceDefinitionStatement.
 * To test if a scope contains only declarations
 * use "bool containsOnlyDeclarations()". Because this function returns a
 * reference to the list (instead of a list of pointers to the declarations or
 * statements) any modification of the list is a modification of the AST.
 * - Return: STL list of pointers to SgDeclarationStatement objects.
 *
 * #### `SgScopeStatement::getStatementList()`
 * Gets reference to internal STL list of pointers to SgStatement objects
 * (only defined for scopes containing statements, see bool
 * containsOnlyDeclarations()). This function is useful for SgScopeStatement
 * objects that contain non-declarations (lists of SgStatement instead of lists
 * of SgDeclarationStatement IR nodes). Because this function returns a
 * reference to the list (instead of a list of pointers to the declarations or
 * statements) any modification of the list is a modification of the AST.
 * - Return: STL list of pointers to SgStatement objects.
 *
 * #### `SgScopeStatement::generateStatementList()`
 * Builds list of pointers to SgStatement objects (useful for all
 * SgScopeStatement objects, independent of whether they contain lists of
 * SgDeclarationStatements or lists of SgStatements). This function is useful
 * for any SgScopeStatement object, but since it returns by value any
 * modification of the list is not reflected in the AST.
 * - Todo: This runtion should return a const value so that it would be more
 * clear that it could not be modified (since such modifications would not be
 * reflected in the AST).
 * - Return: STL list of pointers to SgStatement objects (returns by value).
 */
class SgScopeStatement;

/** @brief This class represents the "sizeof()" operator (applied to any type).
 *
 * The operand is represented as either a type or an unevaluated expression.
 * The frontend also records the result type for the target ABI.
 * - See also:
 * Example of using a SgSizeOfOp object
 *
 * **Data members**
 *
 * #### `SgSizeOfOp::p_operand_expr`
 * This a pointer to the expression given to the sizeof operator.
 * This is the expression provided as an argument to the sizeof operator
 * (if it is an expression).
 *
 * #### `SgSizeOfOp::p_operand_type`
 * This a pointer to the SgType given to the sizeof operator.
 * This is the type provided as an argument to the sizeof operator
 * (if it is a type).
 *
 * #### `SgSizeOfOp::p_expression_type`
 * This a SgType, the type of the expression.
 *
 */
class SgSizeOfOp;

/** @brief This class represents the notion of a statement.
 *
 * There are no uses of this IR node anywhere.  All statements
 * are derived from this SgStatement class.
 * - Internal: This is a base class for all statements.
 * - Todo: Fortran support for modifiers can be used as statement (must be added
 * to IR): see section 5.2, 5.3 in Fortran 2003 standard. Note that type
 * modifiers can be used as statements.
 * - Todo: Fortran support requires statements in section 6.3.
 * - Todo: Fortran support requires for case statement ranges (gnu extension for
 * C, but standard in Fortran).
 * - Todo: Fortran support requires statements in section 8.1.4, 8.1.5.
 * - Todo: Fortran support requires statements in section 15 (modifiers for
 * ISO_C_BINDING).
 * - See also:
 * Example of using a SgStatement object
 *
 * **Member functions**
 *
 * #### `SgStatement::get_scope() const`
 * Returns scope of current statement.
 * Returns the explicit semantic scope for declarations that store one, or the
 * scope defined by the owning AST structure for other statements. Producers
 * establish those identities during construction. Function parameter scopes
 * follow their owning declaration's signature or definition role.
 * - Return: Returns valid pointer to SgScopeStatement.
 *
 * #### `SgStatement::replace_child (SgStatement *, SgStatement *, bool
 * extractListFromBasicBlock=false)` Private supporting function for low level
 * SageIII rewrite interface. This function implements special semantics for the
 * insertion of a collection of statements in a block.  If
 * extractListFromBasicBlock is true then the statements in the block are
 * extracts and placed into the same scope as the target position.
 * - Return: Returns bool.
 *
 * #### `SgStatement::insert_child (SgStatement *, SgStatement *, bool
 * inFront=true, bool extractListFromBasicBlock=false)` Private supporting
 * function for low level SageIII rewrite interface. This function implements
 * special semantics for the insertion of a collection of statements in a block.
 * If extractListFromBasicBlock is true then the statements in the block are
 * extracts and placed into the same scope as the target position.
 * - Return: Returns bool.
 *
 * #### `SgStatement::get_symbol_from_symbol_table()`
 * Virtual function to obtain the SgSymbol from the scope's symbol table.
 * This function permits retrival of the associated SgSymbol, where it exists
 * for that IR node, where is is associated with this exact declaration. If no
 * symbol is found in the symbol table the function returns NULL. If it is not
 * implemented for the associated statement or declaration it is an error
 * (assertion failure).   This fucntion is ONLY defined for those IR nodes for
 * which symbols can be constructed.
 * - Return: Returns SgSymbol pointer.
 *
 * #### `SgStatement::isChildUsedAsLValue(SgExpression *)`
 * Virtual function to determine if the provided expression is used as an
 * l-value within the cotext of this statement.
 * - Return: True if the expression is used within this statement as an l-value.
 *
 * **Additional notes**
 * Low-level Support for mutation of the AST.
 * This is the lowest level of support for the mutation of the AST.
 * - Internal: Need to document this section more fully.
 */
class SgStatement;

/** @brief This class represents the GNU extension "statement expression" (thus
 * is
 *
 * non-standard C and C++).
 * The statement expression is a GNU extension that is helpful in the
 * implementation of macros and expressions.  In most cases an alternative
 * construction of code is possible. More information is at: \n
 * http://developer.apple.com/documentation/developertools/gcc-4.0.1/gcc/Statement-Exprs.html
 * \n In g++, the result value of a statement expression undergoes array and
 * function pointer decay, and is returned by value to the enclosing expression.
 * For instance, if A is a class, then: \n A a; ({a;}).Foo() \n will construct a
 * temporary A object to hold the result of the statement expression, and that
 * will be used to invoke Foo. Therefore the this pointer observed by Foo will
 * not be the address of a.
 * - Internal: This sort of expression can be useful and is important to at
 * least one user so it is implemented in ROSE.
 * - See also:
 * Example of using a SgStatementExpression object
 *
 * **Data members**
 *
 * #### `SgStatementExpression::p_statement`
 * This a pointer to the SgStatement (usually a SgBasicBlock).
 *
 * #### `SgStatementExpression::p_expression_type`
 * This a SgType, the type of the expression.
 *
 */
class SgStatementExpression;

/** @brief This class represents modifiers specific to storage.
 *
 * This modifier is set to only one of a collection of values. To
 * unset any value (e.g. "extern") just call "setDefault()" to restore
 * the default value.
 * - Note: For more detail see the C++ grammar specification in Bjarne's book,
 * Appendix B.
 * - See also:
 * Example of using a SgStorageModifier object
 *
 * **Data members**
 *
 * #### `SgStorageModifier::storage_modifier_enum p_modifier`
 * enum value (can be set to only one of several values)
 *
 * **Member functions**
 *
 * #### `SgStorageModifier::isStatic () const`
 * Storage modifier is static (always false for the SgStorageModifier in the
 * SgInitializedName).
 * - Internal: Note that the isStatic() for variable declarations is stored in
 * the SgStorageModifier of theSgDeclarationModifier in the
 * SgVariableDeclaration, and not in the SgStorageModifier stored in the
 * SgInitializedName.  This is a result of the structure of code, where the
 * static keyword can not be used to selectively apply to individual variables
 * in a list of names in a variable declaration.
 * - Return: Returns bool.
 *
 */
class SgStorageModifier;

/** @brief This class represents the base class of a numbr of IR nodes that
 * don't otherwise fit
 *
 * into the existing hierarchy of statement, expression, types, and symbols.
 * There are no uses of this IR node anywhere.  All SgSupport based IR nodes
 * are derived from this SgSupport class.
 * - Internal: This is a base class for all SgSupport objects.
 * - See also:
 * Example of using a SgSupport object
 *
 */
class SgSupport;

/** @brief This class represents the concept of a switch.
 *
 * - Internal: Note that the design or some aspect of the design may be the
 * cause of falure to handle the Duff's Device example code. I think this is not
 * fixed, Oct 2005.
 *
 * **Data members**
 *
 * #### `SgSwitchStatement::p_item_selector`
 * The selector statement, which can be an expression statement or declaration.
 *
 * #### `SgSwitchStatement::p_body`
 * This pointer a SgStatement, and holds the cases in the body of the switch.
 *
 * **Member functions**
 *
 * #### `SgSwitchStatement::copy(const SgCopyHelp & help)`
 * Makes a copy (deap of shallow depending on SgCopyHelp).
 * - Return: Returns pointer to copy of SgSwitchStatement.
 *
 * #### `SgSwitchStatement::get_item_selector() const`
 * Access function for p_item_selector_root.
 * - Return: Returns a pointer to a SgExpression.
 *
 * #### `SgSwitchStatement::set_item_selector(SgExpression* item_selector)`
 * Access function for p_item_selector.
 * - Param `for_init_stmt`: SgExpression pointer
 * - Return: Returns void.
 *
 * #### `SgSwitchStatement::get_body() const`
 * Access function for p_body.
 * - Return: Returns a pointer to a SgStatement.
 *
 * #### `SgSwitchStatement::set_body(SgStatement* body)`
 * Access function for p_body.
 * - Param `loop_body`: SgStatement pointer
 * - Return: Returns void.
 */
class SgSwitchStatement;

/** @brief This class represents the concept of a name within the compiler.
 *
 * Symbols are a simpler way for the compiler to quickly associate types,
 * declarations and names.  It is also more compact than carrying around
 * names as strings.  The SgSymbol can contain many references and is a shared
 * IR node.
 * A "symbol" within the compiler is a concept of a name.  The simplest
 * example is a variable name, the variable most often has a name but internally
 * within the compiler the variable is referenced as a symbol, the symbol data
 * struction contains a name, type, and declaration (where the name was first
 * associated with the type).  Each variable has an associated symbol and each
 * variable reference internally stores a pointer to the single symbol
 * representing that variable.  this avoids the compiler internally relying upon
 * string processing to compile variable (different symbols can be trivially
 * compared to either have the same address on not).  Each named entity
 * within the language's translation within the AST has a unique symbol.
 * Mappings from names to symbols are held in the "Symbol Table" which provides
 * for a quick (hashed) lookup on the symbol for any name.  Mappings from the
 * symbol to the name are held through pointers to the declaration (stored
 * within the symbol). To organize the symbols, they are differentiated by type,
 * each symbol is derived from the SgSymbol class and defines a specialized
 * interface for that specific kind of IR node.  For example, there are symbol
 * objects specific to classes, templates, variable, enums, functions, member
 * functions, lables, function types, namespaces, typedefs, etc.  In each case
 * these symbols contain a name (or method for generating a name when it is
 * located elsewhere), and a declaration, and often a reference to a type (or
 * function to returning the type if not held in the symbol directly). Each
 * SgSymbol has a get_name() member function which gets the associated name of
 * the symbol, usually from the stored declaration. A few symbols have an
 * explicitly stored name (SgName), this is done where the name is derived from
 * a named type (SgNamedType). IMPORTANT: Declarations in Symbols \n Indepedent
 * of the different kinds of declarations (declarations are statements),
 * declarations can be considered to be definind and non-defining.  See
 * SgDeclarationStatement for details.  where there exist defining and
 * non-defining declarations symbols within Sage III always reference
 * non-defining declarations (only non-defining declarations are shared within
 * the AST).
 * - Internal: This is a base class for all symbols.
 * - Todo: Consider derivation of SgEnumSymbol, SgClassSymbol,
 * SgFunctionTypeSymbol, and SgTypedefSymbol from a common SgTypeSymbol.  Then
 * supporting functions for SgTypeSymbol would lookup any of these type based
 * symbols.
 * - See also: SgDeclarationStatement
 * - See also:
 * Example of using a SgSymbol object
 *
 * **Member functions**
 *
 * #### `SgSymbol::get_name() const`
 * Access function for getting name from declarations or types internally.
 * - Internal: This is a virtual function.
 * - Return: Returns SgName.
 *
 * #### `SgSymbol::get_type() const`
 * This function returns the type associated with the named entity.
 * - Internal: This is a virtual function.
 * - Return: Returns SgType*.
 *
 */
class SgSymbol;

/** @brief This class represents the symbol tables used in both SgScopeStatement
 * and
 *
 * the SgFunctionTypeSymbolTable IR node.
 * - Todo: The rose_hash_multimap should perhaps be included as a data member
 * instead of implemented as a pointer.  We should consider this detail.
 * - Todo: We should decide if we want to give Symbol Tables a name or not, it
 * seems that we rarely if ever do this so perhaps we should not have such a
 * field.
 * - See also:
 * Example of using a SgSymbolTable object
 *
 * **Data members**
 *
 * #### `SgSymbolTable::p_iterator`
 * This iterator is used within some of the deprecated functions.
 * - Deprecated: I think this iterator should be removed.
 *
 * #### `SgSymbolTable::p_name`
 * This is the symbol name (mangled is required).
 *
 * #### `SgSymbolTable::p_no_name`
 * This is a flag to indicate that p_name is empty
 * Some symbols don't have a name.
 * - Todo: generate a list of instances where symbols don't have names.
 * (Namespaces don't have to have a name for example).
 *
 * #### `SgSymbolTable::p_table`
 * This is lower level symbol table implementation (using an STL hash_multimap)
 *
 */
class SgSymbolTable;

/** @brief This class represents template argument within the use of a template
 * to build an instantiation.
 *
 * The template argument is used for the instatiation of templates.  They can
 * also have default values and not be specified explicitly.
 * - Internal: Note that this is not the same as a template parameter, which
 * appears in the template declaration as a placeholder for template argument in
 * the declaration of the template.
 * - Todo: Move this be a SgLocatedNode since it should have a position in the
 * source code.
 * - See also:
 * Example of using a SgTemplateArgument object
 *
 * **Data members**
 *
 * #### `SgTemplateArgument::p_argumentType`
 * This is the type of the argument specificed to the template in the
 * instantiation of the template.
 * - Internal:
 *
 * #### `SgTemplateArgument::p_isArrayBoundUnknownType`
 * Template arguments can be used as array bounds, if so this this marks the
 * arguments use as such.
 *
 * #### `SgTemplateArgument::p_type`
 * This is the template argument's reference to a type (if it is a type).
 * This pointer is valid if the argumentType is "type_argument".
 *
 * #### `SgTemplateArgument::p_expression`
 * This is the template argument's reference to an expression (if it is an
 * expression). This pointer is valid if the argumentType is "nontype_argument".
 *
 * #### `SgTemplateArgument::p_templateDeclaration`
 * The exact semantic template declaration referenced by the argument.
 *
 * #### `SgTemplateArgument::p_explicitlySpecified`
 * This true only if for a function template the argument is explicit in the
 * source code (it need not always be.) For template<typename T> void foo(T t);
 * we could specify:
 * -# foo(1.0);
 * or:
 * -# foo<float>(1.0);
 * only in the second case is the template argument explicit.  For all
 * instantiated member function templates the value is false.
 * - Internal:
 *
 * **Member functions**
 *
 * #### `SgTemplateArgument::SgTemplateArgument( SgType* arg, bool
 * isExplicitlySpecified)` Constructor used with types.
 * - Internal:
 * - See also:
 * Example:create an SgTemplateArgument object
 *
 * #### `SgTemplateArgument::SgTemplateArgument( SgExpression* arg, bool
 * isExplicitlySpecified)` Constructor used with expressions.
 * - Internal:
 * - See also:
 * Example:create an SgTemplateArgument object
 *
 * #### `SgTemplateArgument::SgTemplateArgument(const SgTemplateArgument & X)`
 * This the copy constructor (deep copy made).
 * - Internal: This constructor does a deep copy of all data in the
 * SgTemplateArgument object.
 * - See also:
 * Example:create an SgTemplateArgument object
 *
 * #### `SgTemplateArgument::set_argumentType (
 * SgTemplateArgument::template_argument_enum argumentType )` This function sets
 * argumentType.
 * - Return: Returns void.
 *
 * #### `SgTemplateArgument::template_argument_enum
 * SgTemplateArgument::get_argumentType () const` This function returns
 * argumentType.
 * - Return: Returns SgTemplateArgument::template_argument_enum.
 *
 * #### `SgTemplateArgument::set_isArrayBoundUnknownType ( bool
 * isArrayBoundUnknownType )` This function sets isArrayBoundUnknownType.
 * - Return: Returns void.
 *
 * #### `bool SgTemplateArgument::get_isArrayBoundUnknownType () const`
 * This function returns isArrayBoundUnknownType.
 * - Return: Returns bool.
 *
 * #### `set_type ( SgType *type )`
 * This function sets the argumentType.
 * - Return: Returns void.
 *
 * #### `SgType* SgTemplateArgument::get_type () const`
 * This function returns argumentType.
 * - Return: Returns SgType pointer.
 *
 * #### `set_expression ( SgExpression *exp )`
 * This function sets the argumentExpression.
 * - Return: Returns void.
 *
 * #### `SgExpression* SgTemplateArgument::get_expression () const`
 * This function returns argumentExpression.
 * - Return: Returns SgExpression pointer.
 *
 * #### `SgTemplateArgument::set_explicitlySpecified ( bool explicitlySpecified
 * )` This function sets explicitlySpecified.
 * - Return: Returns void.
 *
 * #### `bool SgTemplateArgument::get_explicitlySpecified () const`
 * This function returns explicitlySpecified.
 * - Return: Returns bool.
 */
class SgTemplateArgument;

/** @brief This class represents the concept of a template declaration.
 *
 * This template declaration is internally marked as being either a
 * - class (e_template_class),
 * - member class (e_template_m_class),
 * - function (e_template_function),
 * - member function (e_template_m_function), or
 * - member data (e_template_m_data)
 * The template name, parameter list, optional requires-clause, classification,
 * and semantic scope are represented by typed fields. Source declarations and
 * definitions are represented by the corresponding typed class, function,
 * variable, or typedef declaration nodes; no source-text payload is stored on
 * this semantic template identity node.
 * - Todo: Template declarations marked as friend don't seem to be marked as
 * friend internally.
 * - Todo: The scope of a SgTemplateDeclaration should be a
 * SgTemplateInstantiationDefn, since it could be associated which more than one
 * definition.  What we need, and don't have yet, is a SgTemplateDefinition to
 * accompany the SgTemplateDeclaration then a SgTemplateDeclaration could have a
 * SgTemplateDefinition for a parent and or scope when it is a member function
 * or namespace or global scope (typically), otherwise.
 * - Todo: Make the "*PtrList" typed objects non-pointer data members (lists)
 * instead of pointer to lists.
 * - Note: Template specializations appear in syntax to be a template
 * declaration (with explicit template arguments), but they are really just an
 * explicit template instantiation. Thus Sage III stores such template
 * specializations as template instantiations and references the associated
 * template declaration (SgTemplateDeclaration) internally.
 * - See also:
 * Example of using a SgTemplateDeclaration object
 *
 * **Data members**
 *
 * #### `SgTemplateDeclaration::p_name`
 * This is the name of the template
 * This is the name of the template (e.g. for "template<class T> class X;",
 * the name is "X".
 *
 * #### `SgTemplateDeclaration::p_template_kind`
 * This is the classification of the template declaration
 * This is the classification of the template declaration.
 * This template declaration is internally marked as being either a
 * - class (e_template_class),
 * - member class (e_template_m_class),
 * - function (e_template_function),
 * - member function (e_template_m_function), or
 * - member data (e_template_m_data)
 *
 * #### `SgTemplateDeclaration::p_templateParameters`
 * This is the STL list of pointers template parameters (SgTemplateParameter
 * objects) This is the STL list of pointers to template parameters
 * (SgTemplateParameter objects) For example, for "template<class T> class X;",
 * the STL list would contain a SgTemplateParameter representing T.
 *
 * #### `SgTemplateDeclaration::p_scope`
 * This is the scope of the template declaration.
 * This is stored explicitly since name qualification can be used to
 * place some declarations in positions that are different from their
 * scope if it were computed structureally from the parent pointer data.
 *
 * **Member functions**
 *
 * #### `SgTemplateDeclaration::get_name()`
 * Returns name of template declaration.
 * - Return: Returns SgName by value.
 *
 * #### `SgTemplateDeclaration::set_name(SgName name)`
 * Access function for p_name.
 * - Return: Returns void.
 *
 * #### `SgTemplateDeclaration::get_template_kind()`
 * Returns enum value (function, class, etc.)
 * - Return: Returns enum value
 *
 * #### `SgTemplateDeclaration::set_template_kind(SgName name)`
 * Access function for p_template_kind.
 * - Return: Returns void.
 */
class SgTemplateDeclaration;

/** @brief This class represents the concept of an instantiated class template.
 *
 * - Internal: Objects of this class are marked as compiler-generated if they
 * are instantiated by the compiler, but not if they are generated from a
 * specialization (where the user has in effect generated it).  A few details:
 * - Currently multple declaration have a valid pointer to the
 * SgTemplateInstantiationDefn object even though there is only one defining
 * declaration.  This is different from functions declarations where only one
 * declaration has a valid pointer to the SgFunctionDefinition.  This needs to
 * be made consistant at some point.
 * - Todo: Make the "*PtrList" typed objects non-pointer data members (lists)
 * instead of pointer to lists.
 * - See also:
 * Example of using a SgTemplateInstantiationDecl object
 *
 * **Data members**
 *
 * #### `SgTemplateInstantiationDecl::p_templateName`
 * This is the name of the templated class (excludes template arguments)
 * The frontend publishes the template's identifier and its typed argument list
 * during construction. Unparsing renders the arguments from their AST nodes.
 *
 * #### `SgTemplateInstantiationDecl::p_templateHeader`
 * This data field is not used (or is used internally).
 * I forget what this name includes.
 *
 * #### `SgTemplateInstantiationDecl::p_templateDeclaration`
 * This is the template declarations (SgTemplateDeclaration) from which this
 * template instantiation is constructed.  This can be NULL, as I recall, for
 * nested classes.
 *
 * #### `SgTemplateInstantiationDecl::p_templateArguments`
 * This is a pointer to a list of pointers to SgTemplateArgument objects (used
 * with the SgTemplateDeclaration to construct this instantiation). Not all
 * arguments are output in the generated code (see SgTemplateArgument for
 * details).
 * - Note: Template arguments are found in the instantiations, and template
 * parameters are found in the SgTemplateDeclaration (arguments are not the same
 * a parameters, same as for descriptions for functions).
 *
 * **Member functions**
 *
 * #### `SgTemplateInstantiationDecl::SgTemplateInstantiationDecl (
 * Sg_File_Info* startOfConstruct = NULL,` SgName name = SgdefaultName, int
 * class_type = 0, SgClassType *type=NULL, SgClassDefinition *definition=NULL)
 * This is the only constructor.
 * This constructor builds the SgTemplateInstantiationDecl base class. but has
 * some specific side-effects (some of which have been removed in the latest
 * work).  It is however still that case that if the definition is provided then
 * it's declaration will be changed to the declaration being constructed (a
 * warning message it output if this happens and this side-effect will be
 * removed soon).
 * - Param `startOfConstruct`: represents the position in the source code
 * - See also:
 * Example:create an SgTemplateInstantiationDecl object
 *
 * #### `SgTemplateInstantiationDecl::get_templateName() const`
 * Returns name of class template, the name excludes template arguments.
 * For the template name with arguments, e.g. "class_template<int>" the user
 * should call get_name() (defined in the SgClassDeclaration base class).
 * - Note: The SgClassSymbol used to hold a reference to this declaration in the
 * symbol table is placed into the symbol table using the get_name(); includes
 * template arguments.
 * - Internal: There are cases where the arguments should not be unparse and
 * then this version of the name is required. Also in generating mangled names,
 * this is a more useful name to have separately from the name which inclused
 * arguments. However in these case it still seems that we could get it from the
 * template declaration (if it is available, member fucntions of a templated
 * class may not have an explicit or separate template declaration).
 * - Return: returns SgName by value.
 *
 * #### `SgTemplateInstantiationDecl::set_templateName(SgName name)`
 * sets name of instantiated class template, name excludes template arguments.
 * - Return: returns void.
 *
 * #### `SgTemplateInstantiationDecl::get_templateDeclaration() const`
 * Returns pointer to SgTemplateDeclaration from which instantiation is
 * generated.
 * - Return: returns pointer to SgTemplateDeclaration.
 *
 * ####
 * `SgTemplateInstantiationDecl::set_templateDeclaration(SgTemplateDeclaration*
 * templateDeclaration)` Access function for p_templateDeclaration.
 * - Return: returns void.
 *
 * #### `SgTemplateInstantiationDecl::get_templateArguments() const`
 * Returns the declaration's list of template arguments by reference.
 *
 */
class SgTemplateInstantiationDecl;

/** @brief This class represents the concept of a class definition in C++.
 *
 * Templated class definitions are currently nearly the same as class
 * definitions (SgClassDefinition).  I expect that work in the future will
 * provide more features that are specific to templates.  Currently this version
 * is different from the SgClassDefinition in that its constructor takes a
 * SgTemplateInstantiationDecl instead of a SgClassDeclaration.
 * - Internal: This is not a base class for IR nodes.
 *
 * **Member functions**
 *
 * #### `SgTemplateInstantiationDefn::SgTemplateInstantiationDefn (`
 * Sg_File_Info* startOfConstruct = NULL, SgTemplateInstantiationDecl *decl =
 * NULL ) This is the constructor. This constructor builds the
 * SgTemplateInstantiationDefn base class.
 * - Param `startOfConstruct`: represents the position in the source code
 *
 * #### `SgTemplateInstantiationDefn::copy(const SgCopyHelp & help)`
 * Makes a copy (deap of shallow depending on SgCopyHelp).
 * - Return: Returns pointer to copy of SgTemplateInstantiationDefn.
 */
class SgTemplateInstantiationDefn;

/** @brief This class represents the concept of a C++ template instantiation
 * directive.
 *
 * This statement controls the instantiation of template, forcing their explicit
 * instantiation.  It provides a mechanism to control the instantiation of
 * template, useful in large projects and libraries.
 * The directive owns and traverses its associated declaration.
 * - See also:
 * Example of using a SgTemplateInstantiationDirectiveStatement object
 *
 * **Data members**
 *
 * #### `SgTemplateInstantiationDirectiveStatement::p_declaration`
 * This pointer points to associated template instantiation declaration meant to
 * be explicitly instantiated. Points to either (I think this is a complete
 * list):
 * - SgTemplateInstantiationDecl
 * - SgTemplateInstantiationFunctionDecl
 * - SgTemplateInstantiationMemberFunctionDecl
 * - Internal: This is a SgDeclarationStatement so that it can reference a
 * number of different declaration IR nodes (but not any SgDeclaration, since
 * many don't make sense).
 *
 * **Member functions**
 *
 * ####
 * `SgTemplateInstantiationDirectiveStatement::SgTemplateInstantiationDirectiveStatement
 * ( Sg_File_Info* startOfConstruct = NULL )` This is the constructor. This
 * constructor builds the SgTemplateInstantiationDirectiveStatement base class.
 * - Param `startOfConstruct`: represents the position in the source code
 *
 * #### `SgTemplateInstantiationDirectiveStatement::get_declaration() const`
 * Returns pointer to SgDeclarationStatement.
 * - Return: Returns pointer to SgDeclarationStatement.
 *
 * ####
 * `SgTemplateInstantiationDirectiveStatement::set_declaration(SgDeclarationStatement*
 * declaration)` Access function for p_declaration.
 * - Return: Returns void.
 */
class SgTemplateInstantiationDirectiveStatement;

/** @brief This class represents the concept of an instantiation of function
 * template.
 *
 * - Internal:
 * - Todo: Make the "*PtrList" typed objects non-pointer data members (lists)
 * instead of pointer to lists.
 * - See also:
 * Example of using a SgTemplateInstantiationFunctionDecl object
 *
 * **Data members**
 *
 * #### `SgTemplateInstantiationFunctionDecl::p_templateName`
 * The frontend publishes the template's identifier and its typed argument list
 * during construction. Unparsing renders the arguments from their AST nodes.
 *
 * #### `SgTemplateInstantiationFunctionDecl::p_templateDeclaration`
 * This is the template declarations (SgTemplateDeclaration) from which this
 * template instantiation is constructed.  This can be NULL, as I recall, for
 * nested classes.
 *
 * #### `SgTemplateInstantiationFunctionDecl::p_templateArguments`
 * This is a pointer to a list of pointers to SgTemplateArgument objects (used
 * with the SgTemplateDeclaration to construct this instantiation). Not all
 * arguments are output in the generated code (see SgTemplateArgument for
 * details).
 * - Note: Template arguments are found in the instantiations, and template
 * parameters are found in the SgTemplateDeclaration (arguments are not the same
 * a parameters, same as for descriptions for functions).
 *
 * **Member functions**
 *
 * #### `SgTemplateInstantiationFunctionDecl::get_templateName() const`
 * Returns name of instantiated function template, name excludes template
 * arguments.
 * - Return: returns SgName by value.
 *
 * #### `SgTemplateInstantiationFunctionDecl::set_templateName(SgName name)`
 * sets name of instantiated function template, name excludes template
 * arguments.
 * - Return: returns void.
 *
 * #### `SgTemplateInstantiationFunctionDecl::get_templateDeclaration() const`
 * Returns pointer to SgTemplateDeclaration from which instantiation is
 * generated.
 * - Return: returns pointer to SgTemplateDeclaration.
 *
 * ####
 * `SgTemplateInstantiationFunctionDecl::set_templateDeclaration(SgTemplateDeclaration*
 * templateDeclaration)` Access function for p_templateDeclaration.
 * - Return: returns void.
 *
 * #### `SgTemplateInstantiationFunctionDecl::get_templateArguments() const`
 * Returns the declaration's list of template arguments by reference.
 *
 */
class SgTemplateInstantiationFunctionDecl;

/** @brief This class represents the concept of an instantiation of member
 * function template
 *
 * or a member function of an instantiation of a template class.
 * - Internal:
 * - Todo: Make the "*PtrList" typed objects non-pointer data members (lists)
 * instead of pointer to lists.
 *
 * **Data members**
 *
 * #### `SgTemplateInstantiationMemberFunctionDecl::p_templateName`
 * The frontend publishes the template's identifier and its typed argument list
 * during construction. Unparsing renders the arguments from their AST nodes.
 *
 * #### `SgTemplateInstantiationMemberFunctionDecl::p_templateDeclaration`
 * This is the template declarations (SgTemplateDeclaration) from which this
 * template instantiation is constructed.  This can be NULL, as I recall, for
 * nested classes.
 *
 * #### `SgTemplateInstantiationMemberFunctionDecl::p_templateArguments`
 * This is a pointer to a list of pointers to SgTemplateArgument objects (used
 * with the SgTemplateDeclaration to construct this instantiation). Not all
 * arguments are output in the generated code (see SgTemplateArgument for
 * details).
 * - Note: Template arguments are found in the instantiations, and template
 * parameters are found in the SgTemplateDeclaration (arguments are not the same
 * a parameters, same as for descriptions for functions).
 *
 * **Member functions**
 *
 * ####
 * `SgTemplateInstantiationMemberFunctionDecl::SgTemplateInstantiationMemberFunctionDecl
 * ( Sg_File_Info* startOfConstruct = NULL )` This is the constructor. This
 * constructor builds the SgTemplateInstantiationMemberFunctionDecl base class.
 * - Param `startOfConstruct`: represents the position in the source code
 *
 * #### `SgTemplateInstantiationMemberFunctionDecl::get_templateName() const`
 * Returns name of instantiated function template, name excludes template
 * arguments.
 * - Return: returns SgName by value.
 *
 * #### `SgTemplateInstantiationMemberFunctionDecl::set_templateName(SgName
 * name)` sets name of instantiated function template, name excludes template
 * arguments.
 * - Return: returns void.
 *
 * #### `SgTemplateInstantiationMemberFunctionDecl::get_templateDeclaration()
 * const` Returns pointer to SgTemplateDeclaration from which instantiation is
 * generated.
 * - Return: returns pointer to SgTemplateDeclaration.
 *
 * ####
 * `SgTemplateInstantiationMemberFunctionDecl::set_templateDeclaration(SgTemplateDeclaration*
 * templateDeclaration)` Access function for p_templateDeclaration.
 * - Return: returns void.
 *
 * #### `SgTemplateInstantiationMemberFunctionDecl::get_templateArguments()
 * const` Returns the declaration's list of template arguments by reference.
 *
 */
class SgTemplateInstantiationMemberFunctionDecl;

/** @brief This class represents the "this" operator (can be applied to any
 * member data).
 *
 * This shows up in the access to member data and is sometimes implicit.
 * Recent changed to the strictness of C++ addressed in several compilers has
 * made this recently more explicitly required.
 * - Internal:
 * - See also:
 * Example of using a SgThisExp object
 *
 * **Data members**
 *
 * #### `SgThisExp::p_class_symbol`
 * This is the symbol of the class to which the "this" operator is applied.
 *
 */
class SgThisExp;

/** @brief C++ throw expression or rethrow. */
class SgThrowOp;

/** @brief This class represents the concept of try statement within the
 * try-catch
 *
 * support for exception handling in C++.
 * - See also:
 * Example of using a SgTryStmt object
 *
 * **Data members**
 *
 * #### `SgTryStmt::p_body`
 * This pointer points to a SgBasicBlock containing the statements to be execued
 * by the try block.
 *
 * #### `SgTryStmt::p_catch_statement_seq_root`
 * This pointer points to a SgCatchStatementSeq and connects the try statement
 * to the sequence of catch statements within the support of exception handling.
 *
 * **Member functions**
 *
 * #### `SgTryStmt::get_body() const`
 * Returns pointer to SgBasicBlock.
 * - Return: Returns pointer to SgBasicBlock.
 *
 * #### `SgTryStmt::set_body(SgBasicBlock* body)`
 * Access function for p_body.
 * - Return: Returns void.
 *
 * #### `SgTryStmt::get_catch_statement_seq_root() const`
 * Returns pointer to SgCatchStatementSeq.
 * - Return: Returns pointer to SgCatchStatementSeq.
 *
 * #### `SgTryStmt::set_catch_statement_seq_root(SgCatchStatementSeq*
 * catch_statement_seq_root)` Access function for p_catch_statement_seq_root.
 * - Return: Returns void.
 */
class SgTryStmt;

/** @brief This class represents the base class for all types.
 *
 * There are no uses of this IR node anywhere.  All SgType based IR nodes
 * are derived from this SgType class.
 * Discussion of get_base_type() and findBaseType():
 * -# fileBaseType() will recursively strip away all typedefs, reference,
 * pointers, arrays, and modifiers
 * -# get_base_type() is a member function on some IR nodes derived from SgType
 * and returns the non-recursively striped (immediate) type under the typedefs,
 * reference, pointers, arrays, modifiers, etc.
 * - Internal: This is a base class for all SgType objects.
 * - Todo: Several classes derived from SgType are not used and can be removed:
 * -# SgTypeUnknown (Used by SageInterface to specify references to undeclared
 * variables)
 * -# SgUnknownMemberFunctionType
 * - Todo: Labels appear to be used as types in "foo(*,*)", see example from
 * Chris (LANL, 4/19/2007).
 * - See also:
 * Example of using a SgType object
 *
 * **Data members**
 *
 * #### `SgReferenceType SgType::p_ref_to`
 * This holds the pointer to a SgReferenceType if this type is a reference to
 * another type.
 *
 * #### `SgPointerType SgType::p_ptr_to`
 * This holds the pointer to a SgPointerType if this type is a pointer to
 * another type.
 *
 * #### `SgReferenceType SgType::p_modifiers`
 * This points to any SgModifierNodes if this type contains type modifiers.
 *
 */
class SgType;

/** @brief This class represents a C99 complex type.
 *
 * This type can be used in C++ codes by using either the new "_Complex" syntax
 * or the older syntax __complex__ (older syntax may be GNU specific).  Note
 * that C++ codes can alternatively use the complex classes and avoid use of
 * this type. This is here mostly to support C99 extensions to C.
 * - See also:
 * Example of using a SgTypeComplex object
 *
 */
class SgTypeComplex;

/** @brief A placeholder type.
 *
 * - See also:
 * Example of using a SgTypeDefault object
 *
 */
class SgTypeDefault;

/** @brief This class represents a C99 complex type.
 *
 * This is a legacy ROSE IR type node for an imaginary scalar base type.
 * It is retained for IR/API compatibility.
 * - See also:
 * Example of using a SgTypeImaginary object
 *
 */
class SgTypeImaginary;

/** @brief This class represents a string type used for SgStringVal IR node.
 *
 * - See also:
 * Example of using a SgTypeString object
 *
 */
class SgTypeString;

/** @brief This class represents the notion of a typedef declaration.
 *
 * Typedefs define new types for use in variable declarations, function
 * parameter lists, etc.  Typically the base type is complex and the typedef
 * name allows the more complex types use to be made easier to read.
 * - Todo: There are a few data members in this field that don't appear to be
 * used (should be removed if not required):
 * - p_declaration: have not seen it be used anywhere (I think it is used when a
 * declartion is explicit in the typedef, check this out, might be part of older
 * mechanism before defining and nondefining declarations were developed to
 * provide a uniform mechanism for all declarations)
 * - See also:
 * Example of using a SgTypedefDeclaration object
 *
 * **Data members**
 *
 * #### `SgName SgTypedefDeclaration::p_name`
 * This is the name of the newly defined type.
 * - Internal: This name carries no qualification.
 *
 * #### `SgType SgTypedefDeclaration::p_base_type`
 * This is the type being given a new name by the typedef declaration.
 * - Internal: This name can be private so using the base type directly can
 * cause access violations within generated code.
 *
 * #### `SgTypedefType SgTypedefDeclaration::p_type`
 * This is the resulting type defined by the typedef declaration.
 * This type can be used where any type can be use, declaration of variables,
 * etc.
 * - Internal: Because of access privileges on the typedef declaration the
 * resulting type can have access restrictions.
 *
 * #### `SgDeclarationStatement SgTypedefDeclaration::p_declaration`
 * pointer to the declaration (typically a SgClassDeclaration).
 * A typedef such as "typedef struct {int __pos; int __state;} _G_fpos64_t;"
 * defines a class as part of its declaration.  p_declaration pointes to the
 * declaration in these cases.
 * - Internal: This is the class declaration in a typedef that defines a class.
 *
 * #### `bool
 * SgTypedefDeclaration::p_typedefBaseTypeContainsDefiningDeclaration` This flag
 * indicates that the typedef defines a structure typedefExample.C This example
 * show the definition of a struct within the typedef.
 * - Internal: I would like to give this a better name.
 *
 * #### `SgScopeStatement SgTypedefDeclaration::p_scope`
 * This is the scope of the typedef declaration.
 * - Internal: Comment on why we need to store the scope explicitly (resolves
 * name qualification issues).
 *
 * **Member functions**
 *
 * #### `SgTypedefDeclaration::get_scope() const`
 * Returns scope of current statement.
 * Returns the explicit semantic scope for declarations that store one, or the
 * scope defined by the owning AST structure for other statements. Producers
 * establish those identities during construction. Function parameter scopes
 * follow their owning declaration's signature or definition role.
 * - Return: Returns valid pointer to SgScopeStatement.
 */
class SgTypedefDeclaration;

/** @brief This class represents the notion of a unary operator.
 *
 * It is derived from a SgExpression because operators are expressions.
 * - Internal: This is a base class for all unary operators.
 * - See also:
 * Example of using a SgUnaryOp object
 *
 * **Data members**
 *
 * #### `SgUnaryOp::p_operand_i`
 * This is the operand associated with the unary operator.
 * Every unary operator is applied to a single operand, this
 * variable stores the operand to which the unary operator is applied.
 *
 * #### `SgUnaryOp::p_expression_type`
 * The exact semantic result type established by the producer.
 *
 * #### `SgUnaryOp::p_mode`
 * This variable records the prefix vs. postfix semantics of the operator
 * since the syntax of "operator++" cannot readily do so.  This approach is
 * simpiler to interogate than the C++ syntax for distingishing prefix vs.
 * postfix.
 *
 * **Member functions**
 *
 * #### `SgUnaryOp::SgUnaryOp ( Sg_File_Info *startOfConstruct=NULL,
 * SgExpression *operand_i=NULL, SgType *expression_type=NULL)` This is the
 * constructor. This constructor builds the SgUnaryOp base class.
 * - Param `startOfConstruct`: represents the position in the source code
 * - Param `operand`: represents the operand to which the operator is applied
 * - Param `expression_type`: represents the type of the return value of the
 * operator
 * - See also:
 * Example:create an SgUnaryOp object
 *
 * #### `SgUnaryOp::get_operand() const`
 * returns SgExpression pointer to the operand associated with this unary
 * operator.
 * - Return: Returns SgExpression pointer.
 *
 * #### `SgUnaryOp::set_operand (SgExpression* operand)`
 * This function allows the p_operand pointer to be set (used internally).
 * This function is mostly used internally and is only required to support
 * editing of existing SgUnaryOp objects.
 * - Param `operand`: - sets value of internal p_operand pointer.
 * - Return: Returns void.
 *
 * #### `SgUnaryOp::get_expression_type (void) const`
 * returns type of operator expression.
 * This function returns the type of the unary operator.
 * - Return: Returns type of operator expression.
 *
 * #### `SgUnaryOp::set_expression_type (SgType* expression_type)`
 * This function allows the p_expression_type pointer to be set (used
 * internally). This function is mostly used internally and is only required to
 * support editing of existing SgUnaryOp objects. In general it is not changed
 * once it is set.
 * - Param `expression_type`: - sets value of internal p_expression_type
 * pointer.
 * - Return: Returns void.
 *
 * #### `SgUnaryOp::get_mode() const`
 * Get the prefix/postfix mode of the operator.
 * Get the prefix/postfix mode of the operator (if applicable to that operator).
 * - Return: Returns SgUnaryOp::Sgop_mode (enum for prefix,postfix values)
 *
 * #### `SgUnaryOp::set_mode ( SgUnaryOp::Sgop_mode mode )`
 * Set the mode (prefix/postfix) associated with this operator
 * This is an internal function, it sets up the prefix/postfix mode of the
 * unary operator.
 * - Param `mode`: - sets value of intermal SgUnaryOp::Sgop_mode mode variable
 * - Return: Returns void
 *
 * #### `SgUnaryOp::length() const`
 * Returns the number of operands (1).
 * - Deprecated.
 * - Return: Returns int
 *
 * #### `SgUnaryOp::empty () const`
 * Returns false.
 * - Deprecated.
 * - Return: Returns bool
 *
 * #### `SgUnaryOp::get_next(int &n) const`
 * Returns the operand and increments n if n is zero; otherwise returns null.
 * - Deprecated.
 * - Return: Returns SgExpression pointer.
 *
 * #### `SgUnaryOp::get_operand_i() const`
 * returns SgExpression pointer to the operand associated with this unary
 * operator.
 * - Return: Returns SgExpression pointer.
 *
 * #### `SgUnaryOp::set_operand_i (SgExpression* operand)`
 * This function allows the p_operand_i pointer to be set (used internally).
 * This function is mostly used internally and is only required to support
 * editing of existing SgUnaryOp objects.
 * - Param `operand`: - sets value of internal p_operand pointer.
 * - Return: Returns void.
 */
class SgUnaryOp;

/** @brief This class represents the concept of a C++ using declaration.
 *
 * The using declaration permits named declaration or declarations within the
 * named namespace to be used in the scope of the using declaration without name
 * qualification.
 * - See also:
 * Example of using a SgUsingDeclarationStatement object
 *
 * **Data members**
 *
 * #### `SgUsingDeclarationStatement::p_declaration`
 * This pointer points to a SgDeclarationStatement whose declaration(s) are
 * available for use in the scope containing this using declaration.
 * - Internal: this is a valid pointer if the IR nodes specified using a
 * declaration (else p_initializedName is valid).  Only one of p_declaration and
 * p_initializedName are valid pointers.
 *
 * #### `SgUsingDeclarationStatement::p_initializedName`
 * This pointer points to a SgInitializedName whose declaration is
 * available for use in the scope containing this using declaration.
 *
 * **Member functions**
 *
 * #### `SgUsingDeclarationStatement::get_declaration() const`
 * Access function for p_declaration.
 * This is a valid pointer if the using declaration references
 * a declaration, else it is null and p_initializedName is a valid pointer.
 * One or the other is a valid pointer, but not both.
 * - Return: Returns pointer to SgDeclaration.
 *
 * #### `SgUsingDeclarationStatement::set_declaration(SgDeclarationStatement*
 * declaration)` Access function for p_declaration.
 * - Return: Returns void.
 *
 * #### `SgUsingDeclarationStatement::get_initializedName() const`
 * Access function for p_initializedName.
 * This is a valid pointer if the using declaration references
 * a variable name, else it is null and p_declaration is a valid pointer.
 * One or the other is a valid pointer, but not both.
 * - Return: Returns pointer to SgInitializedName.
 *
 * #### `SgUsingDeclarationStatement::set_initializedName(SgInitializedName*
 * initializedName)` Access function for p_initializedName.
 * - Return: Returns void.
 */
class SgUsingDeclarationStatement;

/** @brief This class represents the concept of a C++ using directive.
 *
 * - Todo: Explain difference between using declaration and using directive.
 * - See also:
 * Example of using a SgUsingDirectiveStatement object
 *
 * **Data members**
 *
 * #### `SgUsingDirectiveStatement::p_namespaceDeclaration`
 * This pointer points to namespace declaration being used.
 *
 * **Member functions**
 *
 * #### `SgUsingDirectiveStatement::get_namespaceDeclaration() const`
 * Access function for p_namespaceDeclaration.
 * - Return: Returns pointer to SgNamespaceDeclarationStatement.
 *
 * ####
 * `SgUsingDirectiveStatement::set_namespaceDeclaration(SgNamespaceDeclarationStatement*
 * namespaceDeclaration)` Access function for p_namespaceDeclaration.
 * - Return: Returns void.
 */
class SgUsingDirectiveStatement;

/** @brief This class represents the notion of an value (expression value).
 *
 * - Internal: This is a base class for all value expressions.
 * - See also:
 * Example of using a SgValueExp object
 *
 */
class SgValueExp;

/** @brief This class represents the variable refernece in expressions.
 *
 * - Todo: Test to verify that each variable reference is associated with the
 * inner most scoped variable with that name, except where name qualified.
 * Applies most easily to local variables.  The same test could be used for
 * function references, actually all references.
 * - Todo: Make sure that declarations appear before variable references.
 * - See also:
 * Example of using a SgVarRefExp object
 *
 * **Data members**
 *
 * #### `SgVarRefExp::p_lvalue`
 * This boolean variable marks the current expression as a
 * left hand side value (lvalue).
 *
 * #### `SgVarRefExp::p_need_paren`
 * This boolean value marks the current expression as requiring parenthises.
 * This boolean value marks the current expression as requiring parenthises (the
 * information comes from the frontend's interpretation of the requirement and
 * is almost always overly conservative.  The unparser currently backs out more
 * accurate rules based on operator precedence and removed then where they
 * are not truely required.  Thus the purpose of this variable is to capture the
 * interpritation of the frontend regarding the use of parenthesis.
 *
 * **Member functions**
 *
 * #### `SgVarRefExp::get_type() const`
 * Returns the exact type associated with the referenced symbol.
 *
 */
class SgVarRefExp;

/** @brief This class represents the concept of a C or C++ variable declaration.
 *
 * A variable declaration can be either a forward declaration (specified as
 * "extern") or a defining declaration (typical case).  Because of this
 * separation (as with classes, functions, etc.) the scope of a variable must be
 * stored explicitly because name qualification can associate a definition in a
 * scope different from the original declaration (see test2005_34.C for
 * examples).
 * - Internal: Since multiple variables may be declared in a single variable
 * declaration, the scope information is held in the SgInitializedName object
 * directly, and not the SgVariableDeclaration.  The scope of a
 * SgVariableDeclaration is ...
 * - Todo: Finish explaination of variable declaration, relationship to variable
 * definition, and the scope issue.
 * - Todo: template static variable declaration are instantiated and this is at
 * least sometimes an error (at least when not part of a transformation). See
 * test2005_69.C for example of this problem.
 * - See also:
 * Example of using a SgVariableDeclaration object
 *
 * **Data members**
 *
 * #### `SgVariableDeclaration::p_baseTypeDefiningDeclaration`
 * This is used to traverse type definitions within variable declarations.
 * In cases where a class or other named type is defined within (and as the
 * base type of) a variable declaration, the traversal code must be able to
 * traverse this definition. The traversal code computes the appropriate value
 * for this pointer when needed.
 *
 * #### `SgVariableDeclaration::p_variables`
 * This is an STL list of pointers to SgInitializedName objects.
 * Each variable is a SgInitializedName object, their can be a list of
 * then, so this list holds that collection of variables.
 *
 * #### `SgVariableDeclaration::p_specialization`
 * This is part of template support (variables of templated types).
 * This is most often set to SgDeclarationStatement::e_no_specialization, but
 * where templates are involved can be set to either
 * SgDeclarationStatement::e_specialization or
 * SgDeclarationStatement::e_partial_specialization.
 *
 * **Member functions**
 *
 * #### `SgVariableDeclaration::get_variables() const`
 * Access function for p_variables.
 * - Return: Returns a const reference to SgInitializedNamePtrList.
 *
 * #### `SgVariableDeclaration::get_variables()`
 * Access function for p_variables.
 * - Return: Returns a non-const reference to SgInitializedNamePtrList.
 *
 * #### `SgVariableDeclaration::get_baseTypeDefiningDeclaration() const`
 * Returns the exact class or enum definition structurally owned by this
 * declaration, or null when the declaration only references its base type.
 *
 * ####
 * `SgVariableDeclaration::set_baseTypeDefiningDeclaration(SgDeclarationStatement*)`
 * Sets the exact owned, non-autonomous class or enum definition. Null is not a
 * valid setter argument; absence is the construction-time default. The
 * definition must already have this variable declaration as its parent and
 * must be the base definition named by every declarator type.
 *
 * #### `SgVariableDeclaration::get_specialization() const`
 * Access function for p_specialization.
 * - Return: Returns value of type
 * SgDeclarationStatement::template_specialization_enum.
 *
 * ####
 * `SgVariableDeclaration::set_specialization(SgDeclarationStatement::template_specialization_enum
 * specialization)` Access function for p_specialization.
 * - Return: Returns void.
 */
class SgVariableDeclaration;

/** @brief This class represents the definition (initialization) of a variable.
 *
 * The most common use of the SgVariableDefinition is to store the associated
 * bitfield specifier in declarations such as "int x:4;" which is a four bit
 * integer variable. Each definition has an exact owning SgInitializedName.
 * - Note: Both SgVariableDeclaration and SgVariableDefinition are both derived
 * from SgDeclaration.  This is because a SgVariableDefinition does not
 * represent a scope like all other definitions (which are derived from
 * SgScopeStatement).
 * - Todo: If a SgVariableDefinition is built internally as part of a
 * SgVariableDeclaration it should be marked as compiler generated if the
 * "extern" keyword was not used in the SgVariableDeclaration.  This needs to be
 * looked into.
 * - See also:
 * Example of using a SgVariableDefinition object
 *
 * **Data members**
 *
 * #### `SgVariableDefinition::p_vardefn`
 * This pointer points to associated SgInitializedName object (the variable).
 *
 * #### `SgVariableDefinition::p_bitfield`
 * This a pointer to a value specifies the bitwidth of the variable (used to
 * control memory layout/padding of data members in data structures). This is a
 * somewhat rare feature of C, and so C++. variables of some types can be
 * specified to have a specific bit width (usually a number of bits smaller than
 * the default size).
 *
 * **Member functions**
 *
 * #### `SgVariableDefinition::get_vardefn() const`
 * Access function for SgInitializedName stored in p_vardefn.
 * - Return: Returns pointer to SgInitializedName.
 *
 * #### `SgVariableDefinition::set_vardefn(SgInitializedName* expr)`
 * Access function for SgInitializedName in p_vardefn.
 * - Return: Returns void.
 *
 * #### `SgVariableDefinition::get_bitfield() const`
 * Access function for p_bitfield.
 * - Return: Returns pointer to SgExpression.
 *
 * #### `SgVariableDefinition::set_bitfield(SgExpression* bitfield)`
 * Access function for p_bitfield.
 * - Return: Returns void.
 */
class SgVariableDefinition;

/** @brief This class represents the concept of a variable name within the
 * compiler (a
 *
 * shared container for the declaration of a variable (SgInitializedName)).
 * Symbols are a simpler way for the compiler to quickly associate types,
 * declarations and names.  This symbol is specific to holding a variable name.
 * - Internal: Symbols are placed into scopes (more precisely into symbol tables
 * held in each SgScopeStatement).  Most symbols are only in a single symbol
 * table, but the SgVariableSymbol can exist in two scopes if it is a static
 * data member declared both in its class (SgClassDefinition) and outside of the
 * class (in global scope or a namespace scope (SgGlobal or
 * SgNamespaceDefinitionStatement)).
 * - Todo: Need to figure out if it is such a great idea of a single symbol to
 * be in two scopes or if it would be better to use two different symbols (since
 * there are two different SgInitializedName object built (the last one
 * referencing the previous one through the p_prev_decl_item pointer)).
 * - Todo: The get_type() function can return NULL when the get_definition() is
 * NULL.  I think we should have assertiosn to make sure that get_definition is
 * a valid pointer and that get_type() should not return NULL.
 * - See also: SgInitializedName
 * - See also:
 * Example of using a SgVariableSymbol object
 *
 * **Member functions**
 *
 * #### `SgVariableSymbol::get_name() const`
 * Access function for getting name from declarations or types internally.
 * - Internal: This is a virtual function.
 * - Return: Returns SgName.
 *
 * #### `SgVariableSymbol::get_type() const`
 * This function returns the type associated with the named entity.
 * - Internal: This is a virtual function.
 * - Return: Returns SgType*.
 *
 */
class SgVariableSymbol;

/** @brief This class represents a while statement.
 *
 * - Internal:
 *
 * **Data members**
 *
 * #### `SgWhileStmt::p_condition`
 * This pointer a SgStatement, the conditional expression in the loop construct.
 *
 * #### `SgWhileStmt::p_body`
 * This pointer a SgStatement, and holds the statements in the body of the
 * loop.
 *
 * **Member functions**
 *
 * #### `SgWhileStmt::copy(const SgCopyHelp & help)`
 * Makes a copy (deap of shallow depending on SgCopyHelp).
 * - Return: Returns pointer to copy of SgWhileStmt.
 *
 * #### `SgWhileStmt::get_body() const`
 * Access function for p_body.
 * - Return: Returns a pointer to a SgStatement.
 *
 * #### `SgWhileStmt::set_body(SgStatement* body)`
 * Access function for p_body.
 * - Param `body`: SgStatement pointer
 * - Return: Returns void.
 *
 * #### `SgWhileStmt::get_condition() const`
 * Access function for p_condition.
 * - Return: Returns a pointer to a SgStatement.
 *
 * #### `SgWhileStmt::set_condition(SgStatement* condition)`
 * Access function for p_condition.
 * - Param `condition`: SgStatement pointer
 * - Return: Returns void.
 */
class SgWhileStmt;

#endif
