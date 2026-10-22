#ifndef UNPARSER_OPT_H
#define UNPARSER_OPT_H

/*!  unparser_opt.h
 *  This class contains all options we want to have available when
 *  generating C++ code
 *
 *  Controls the printing of:
 *  auto_opt - "auto" in front of variable declarations
 *  linefile_opt - emission of #line directives
 *  overload_opt - overloaded operators for non-primitive types. For example,
 *                prints out "+" instead of "operator+"
 *  boolnum_opt - booleans as numbers (0 and 1) to designate false and true or
 *               as words ("false" and "true") themselves
 *  this_opt - "this" in from of data or function members
 *  debug_opt - debugging information to standard output
!*/

class Unparser_Opt {
  //! This class controls the final unparsing of the AST
public:
  // constructors
  Unparser_Opt(); // all options are set to false by default
  Unparser_Opt(bool, bool, bool, bool, bool, bool, bool);

  // destructor
  ~Unparser_Opt();

  // access functions
  bool get_auto_opt();
  bool get_linefile_opt();
  void set_linefile_opt(bool opt);
  bool get_overload_opt();
  bool get_num_opt();
  bool get_this_opt();
  void set_this_opt(bool opt);
  bool get_debug_opt();
  bool get_unparse_includes_opt();

private:
  bool auto_opt;
  bool linefile_opt;
  bool overload_opt;
  bool boolnum_opt;
  bool this_opt;
  bool debug_opt;
  bool unparse_includes_opt;
};

#endif
