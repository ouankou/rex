#include <sage3basic.h>

int
SgNonrealRefExp::get_name_qualification_length () const
   {
     ROSE_ASSERT (this != NULL);
     return p_name_qualification_length;
   }

void
SgNonrealRefExp::set_name_qualification_length ( int name_qualification_length )
   {
     ROSE_ASSERT (this != NULL);
     p_name_qualification_length = name_qualification_length;
   }

bool
SgNonrealRefExp::get_type_elaboration_required () const
   {
     ROSE_ASSERT (this != NULL);
     return p_type_elaboration_required;
   }

void
SgNonrealRefExp::set_type_elaboration_required ( bool type_elaboration_required )
   {
     ROSE_ASSERT (this != NULL);
     p_type_elaboration_required = type_elaboration_required;
   }

bool
SgNonrealRefExp::get_global_qualification_required () const
   {
     ROSE_ASSERT (this != NULL);
     return p_global_qualification_required;
   }

void
SgNonrealRefExp::set_global_qualification_required ( bool global_qualification_required )
   {
     ROSE_ASSERT (this != NULL);

     p_global_qualification_required = global_qualification_required;
   }

int
SgNonrealRefExp::get_explicit_name_qualification_length() const
   {
     ROSE_ASSERT(this != NULL);
     return p_explicit_name_qualification_length;
   }

void
SgNonrealRefExp::set_explicit_name_qualification_length(
    int explicit_name_qualification_length)
   {
     ROSE_ASSERT(this != NULL);
     p_explicit_name_qualification_length = explicit_name_qualification_length;
   }

bool
SgNonrealRefExp::get_explicit_global_qualification() const
   {
     ROSE_ASSERT(this != NULL);
     return p_explicit_global_qualification;
   }

void
SgNonrealRefExp::set_explicit_global_qualification(
    bool explicit_global_qualification)
   {
     ROSE_ASSERT(this != NULL);
     p_explicit_global_qualification = explicit_global_qualification;
   }

const SgStringList &
SgNonrealRefExp::get_explicit_name_qualification_tokens() const
   {
     ROSE_ASSERT(this != NULL);
     return p_explicit_name_qualification_tokens;
   }

SgStringList &
SgNonrealRefExp::get_explicit_name_qualification_tokens()
   {
     ROSE_ASSERT(this != NULL);
     return p_explicit_name_qualification_tokens;
   }

void
SgNonrealRefExp::set_explicit_name_qualification_tokens(
    const SgStringList &explicit_name_qualification_tokens)
   {
     ROSE_ASSERT(this != NULL);
     p_explicit_name_qualification_tokens = explicit_name_qualification_tokens;
   }
