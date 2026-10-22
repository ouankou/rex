#ifndef mangling_support_INCLUDED
#define mangling_support_INCLUDED

class SgFunctionDefinition;

namespace MangledNameSupport {
// Track active definitions to detect recursive mangling.
typedef std::set<SgClassDefinition *> setType;

extern setType visitedTemplateDefinitions;

typedef std::set<SgFunctionDefinition *> functionSetType;
extern functionSetType visitedFunctionDefinitions;

} // namespace MangledNameSupport

std::string replaceNonAlphaNum(const std::string &s);

//! Returns the input std::string stripped of leading and trailing spaces.
std::string trimSpaces(const std::string &s);

//! Joins two sets of mangled qualifiers to form a new mangled qualifier.
std::string joinMangledQualifiersToString(const std::string &base,
                                          const std::string &name);

//! Joins two sets of mangled qualifiers to form a new mangled qualifier.
SgName joinMangledQualifiers(const SgName &base, const SgName &name);

//! Returns the function definition containing the specified statement.
const SgFunctionDefinition *findRootFunc(const SgScopeStatement *scope);

//! Produces a mangled name for a scope statement.
std::string mangleLocalScopeToString(const SgScopeStatement *scope);

/*! Mangles a chain of qualifiers followed from a given scope.
 *
 *  This routine starts at a given scope and collects all
 *  mangled scope names going 'up' through to global scope.
 */
std::string mangleQualifiersToString(const SgScopeStatement *scope);

/*! Mangles a chain of qualifiers, returning an SgName object.
 *  \sa mangleQualifiersToStd::String
 */
SgName mangleQualifiers(const SgScopeStatement *scope);

/*! Mangles a container of types, returning an STL std::string object.
 *
 *  \param[in] b  First element in the container to mangle.
 *  \param[in] e  End (last+1) element in the container to mangle.
 *  \returns An STL std::string containing all mangled names in the list,
 *  concatenated by "__sep__".
 */
std::string mangleTypesToString(const SgTypePtrList::const_iterator b,
                                const SgTypePtrList::const_iterator e);

/*! Mangles a list of types, returning an SgName object.
 *  \sa mangleTypesToStd::String
 */
SgName mangleTypes(const SgTypePtrList::const_iterator b,
                   const SgTypePtrList::const_iterator e);

/*! Returns a mangled form of special C++ function names (excl. qualifiers).
 *
 *  Adapted from SgType::mangledNameSupport. This routine considers these
 *  cases:
 *    - An overloaded operator, including 'new/delete' and 'new[]/delete[]'.
 *    - Overloaded casting operators: the caller should specify the optional
 *      return type of the function as a mangled name (see parameters).
 *    - A destructor (signaled by a '~' prefix).
 *
 *  The caller may omit the second parameter to this function, but
 *  that is not recommended.
 *
 *  \param[in] s              Unmangled function name.
 *  \param[in] ret_type_name  Return type of this function, as a mangled name.
 *  \returns A mangled std::string name.
 */
std::string
mangleFunctionNameToString(const std::string &s,
                           const std::string &ret_type_name = std::string(""));

/*! Returns a mangled form of a C++ function name, as an SgName object.
 *  \sa mangleFunctionNameToStd::String
 */
SgName mangleFunctionName(const SgName &n,
                          const SgName &ret_type_name = SgName(""));

/*! Mangles a template argument container, returning an STL std::string.
 *
 *  \param[in] b  First element in the container to mangle.
 *  \param[in] e  End (last+1) in the container to mangle.
 *  \returns The specified arguments mangled and concatenated by "__sep__".
 */
std::string
mangleTemplateArgsToString(const SgTemplateArgumentPtrList::const_iterator b,
                           const SgTemplateArgumentPtrList::const_iterator e);

/*! Mangles an instantiated template.
 *
 *  \param[in] templ_name   Unmangled base name of the template. This
 * std::string should not contain the template arguments (i.e., should be 'foo'
 * and not 'foo < int >').
 *  \param[in] templ_args   Container of template arguments.
 *  \param[in] scope        Scope of the function (e.g., via 'get_scope ()'),
 *                            used to get qualifiers.
 *  \returns Mangled name, including all parameter information.
 */
std::string mangleTemplateToString(const std::string &templ_name,
                                   const SgTemplateArgumentPtrList &templ_args,
                                   const SgScopeStatement *scope);

//! Mangles an instantiated template, returning an SgName object.
SgName mangleTemplate(const SgName &templ_name,
                      const SgTemplateArgumentPtrList &templ_args,
                      const SgScopeStatement *scope);

/*! Mangles an instantiated template function or template member function.
 *
 *  \param[in] templ_name   Unmangled base name of the function. This
 * std::string should not contain the template arguments (i.e., should be 'foo'
 * and not 'foo < int >').
 *  \param[in] templ_args   Container of template arguments.
 *  \param[in] func_type    The function's type (e.g., via 'get_type ()')
 *  \param[in] scope        Scope of the function (e.g., via 'get_scope ()'),
 *                            used to get qualifiers.
 *  \returns Mangled name, including all parameter information.
 */
std::string mangleTemplateFunctionToString(
    const std::string &templ_name, const SgTemplateArgumentPtrList &templ_args,
    const SgFunctionType *func_type, const SgScopeStatement *scope);

/*! Mangles an instantiated template function or member function, returning
 *  a SgName object.
 *  \sa mangleTemplateFunctionToStd::String
 */
SgName mangleTemplateFunction(const std::string &templ_name,
                              const SgTemplateArgumentPtrList &templ_args,
                              const SgFunctionType *func_type,
                              const SgScopeStatement *scope);

/*! Mangles a template parameter container, returning an STL std::string.
 *
 *  \param[in] b  First element in the container to mangle.
 *  \param[in] e  End (last+1) in the container to mangle.
 *  \returns The specified parameters mangled and concatenated by "__sep__".
 */
std::string
mangleTemplateArgsToString(const SgTemplateParameterPtrList::const_iterator b,
                           const SgTemplateParameterPtrList::const_iterator e);

/*! Mangles a template.
 *
 *  \param[in] templ_name   Unmangled base name of the template. This
 * std::string should not contain the template parameter (i.e., should be 'foo'
 * and not 'foo < typename T >').
 *  \param[in] templ_params Container of template parameters.
 *  \param[in] scope        Scope of the function (e.g., via 'get_scope ()'),
 *                            used to get qualifiers.
 *  \returns Mangled name, including all parameter information.
 */
std::string
mangleTemplateToString(const std::string &templ_name,
                       const SgTemplateParameterPtrList &templ_params,
                       const SgScopeStatement *scope);

//! Mangles a template, returning an SgName object.
SgName mangleTemplate(const SgName &templ_name,
                      const SgTemplateParameterPtrList &templ_params,
                      const SgScopeStatement *scope);

//! Mangles a value expression, as used in a template argument.
std::string mangleValueExp(const SgValueExp *expr);

/*! Mangles a constant expression, needed for template arguments.
 *  \todo Find a better way to deal with general template expression
 *  arguments.
 */
std::string mangleExpression(const SgExpression *expr);

/*! If the declaration has translation unit scope
    (as opposed to program scope), mangle the
    translation unit name. */
std::string mangleTranslationUnitQualifiers(const SgDeclarationStatement *decl);

#endif // mangling_support_INCLUDED
