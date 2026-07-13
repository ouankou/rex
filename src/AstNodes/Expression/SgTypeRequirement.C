#include "sage3basic.h"

int SgTypeRequirement::get_name_qualification_length() const {
  return p_name_qualification_length;
}

void SgTypeRequirement::set_name_qualification_length(
    int name_qualification_length) {
  p_name_qualification_length = name_qualification_length;
}

bool SgTypeRequirement::get_type_elaboration_required() const {
  return p_type_elaboration_required;
}

void SgTypeRequirement::set_type_elaboration_required(
    bool type_elaboration_required) {
  p_type_elaboration_required = type_elaboration_required;
}

bool SgTypeRequirement::get_global_qualification_required() const {
  return p_global_qualification_required;
}

void SgTypeRequirement::set_global_qualification_required(
    bool global_qualification_required) {
  p_global_qualification_required = global_qualification_required;
}
