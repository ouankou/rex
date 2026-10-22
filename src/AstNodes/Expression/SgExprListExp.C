#include "sage3basic.h"

SgExprListExp::source_form_enum SgExprListExp::get_source_form() const {
  return p_source_form;
}

void SgExprListExp::set_source_form(source_form_enum form) {
  ROSE_ASSERT(form == e_expression_list_plain ||
              form == e_expression_list_braced);
  if (p_source_form != form) {
    p_source_form = form;
    markSourceMetadataAsModified();
  }
}

bool SgExprListExp::empty() const { return p_expressions.empty(); }

SgExpressionPtrList &SgExprListExp::get_expressions() { return p_expressions; }

const SgExpressionPtrList &SgExprListExp::get_expressions() const {
  return p_expressions;
}

void SgExprListExp::append_expression(SgExpression *what) {
  what->set_parent(this);
  p_expressions.push_back(what);
}

void SgExprListExp::prepend_expression(SgExpression *what) {
  what->set_parent(this);
  p_expressions.insert(p_expressions.begin(), what);
}

int SgExprListExp::replace_expression(SgExpression *o, SgExpression *n) {
  ROSE_ASSERT(o != NULL);
  ROSE_ASSERT(n != NULL);

  for (SgExpressionPtrList::iterator i = p_expressions.begin();
       i != p_expressions.end(); ++i) {
    if ((*i) == o) {
      ROSE_ASSERT(o->get_parent() == this);
      ROSE_ASSERT(std::count(p_expressions.begin(), p_expressions.end(), o) ==
                  1);
      if (o == n)
        return 1;
      ROSE_ASSERT(n->get_parent() == nullptr);
      ROSE_ASSERT(std::find(p_expressions.begin(), p_expressions.end(), n) ==
                  p_expressions.end());
      *i = n;
      n->set_parent(this);
      o->set_parent(nullptr);
      return 1;
    }
  }
  return 0;
}
