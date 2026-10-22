#include "sage3basic.h"

#include "unparser.h"

Unparser_Opt::Unparser_Opt() {
  // default setting: all options are false;
  auto_opt = false;
  linefile_opt = false;
  overload_opt = false;
  boolnum_opt = false;
  this_opt = false;
  debug_opt = false;
  unparse_includes_opt = false;
}

Unparser_Opt::Unparser_Opt(bool _auto, bool linefile,
                           bool useOverloadedOperators, bool num, bool _this,
                           bool _debug, bool _unparse_includes) {
  auto_opt = _auto;
  linefile_opt = linefile;
  overload_opt = useOverloadedOperators;
  boolnum_opt = num;
  this_opt = _this;
  debug_opt = _debug;
  unparse_includes_opt = _unparse_includes;
}

Unparser_Opt::~Unparser_Opt() {}

bool Unparser_Opt::get_auto_opt() { return auto_opt; }

bool Unparser_Opt::get_linefile_opt() { return linefile_opt; }

void Unparser_Opt::set_linefile_opt(bool opt) { linefile_opt = opt; }

bool Unparser_Opt::get_overload_opt() { return overload_opt; }

bool Unparser_Opt::get_num_opt() { return boolnum_opt; }

bool Unparser_Opt::get_this_opt() { return this_opt; }

void Unparser_Opt::set_this_opt(bool opt) { this_opt = opt; }

bool Unparser_Opt::get_debug_opt() { return debug_opt; }

bool Unparser_Opt::get_unparse_includes_opt() { return unparse_includes_opt; }
