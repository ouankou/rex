#include "sage3basic.h"

int SgNonrealRefExp::get_name_qualification_length() const {
  return p_name_qualification_length;
}

void SgNonrealRefExp::set_name_qualification_length(
    int name_qualification_length) {
  p_name_qualification_length = name_qualification_length;
}

bool SgNonrealRefExp::get_type_elaboration_required() const {
  return p_type_elaboration_required;
}

void SgNonrealRefExp::set_type_elaboration_required(
    bool type_elaboration_required) {
  p_type_elaboration_required = type_elaboration_required;
}

bool SgNonrealRefExp::get_global_qualification_required() const {
  return p_global_qualification_required;
}

void SgNonrealRefExp::set_global_qualification_required(
    bool global_qualification_required) {

  p_global_qualification_required = global_qualification_required;
}

int SgNonrealRefExp::get_explicit_name_qualification_length() const {
  return p_explicit_name_qualification_length;
}

void SgNonrealRefExp::set_explicit_name_qualification_length(
    int explicit_name_qualification_length) {
  p_explicit_name_qualification_length = explicit_name_qualification_length;
}

bool SgNonrealRefExp::get_explicit_global_qualification() const {
  return p_explicit_global_qualification;
}

void SgNonrealRefExp::set_explicit_global_qualification(
    bool explicit_global_qualification) {
  p_explicit_global_qualification = explicit_global_qualification;
}

const SgStringList &
SgNonrealRefExp::get_explicit_name_qualification_tokens() const {
  return p_explicit_name_qualification_tokens;
}

SgStringList &SgNonrealRefExp::get_explicit_name_qualification_tokens() {
  return p_explicit_name_qualification_tokens;
}

void SgNonrealRefExp::set_explicit_name_qualification_tokens(
    const SgStringList &explicit_name_qualification_tokens) {
  p_explicit_name_qualification_tokens = explicit_name_qualification_tokens;
}
