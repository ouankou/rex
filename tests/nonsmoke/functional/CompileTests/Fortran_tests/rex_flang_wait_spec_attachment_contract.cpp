#include <rose.h>

#include "sage-build.h"

#include <iostream>
#include <string>

namespace {

SgIntVal *integer(int value) {
  SgIntVal *expression = SageBuilder::buildIntVal_nfi(value);
  ROSE_ASSERT(expression != nullptr);
  return expression;
}

void attach(SgWaitStatement *statement,
            Rose::builder::detail::FlangWaitSpecKind kind,
            SgExpression *expression) {
  Rose::builder::detail::AttachFlangWaitSpecExpression(statement, kind,
                                                       expression);
}

} // namespace

int main(int argc, char **argv) {
  if (argc != 2) {
    std::cerr << "usage: " << argv[0] << " MODE\n";
    return 2;
  }

  using Rose::builder::detail::FlangWaitSpecKind;
  const std::string mode = argv[1];
  SgWaitStatement *statement = new SgWaitStatement();
  ROSE_ASSERT(statement != nullptr);

  if (mode == "valid") {
    SgExpression *unit = integer(10);
    SgExpression *end = integer(100);
    SgExpression *eor = integer(200);
    SgExpression *err = integer(300);
    SgExpression *id = integer(1);
    SgExpression *iomsg = SageBuilder::buildStringVal_nfi("message");
    SgExpression *iostat = integer(0);
    attach(statement, FlangWaitSpecKind::unit, unit);
    attach(statement, FlangWaitSpecKind::end, end);
    attach(statement, FlangWaitSpecKind::eor, eor);
    attach(statement, FlangWaitSpecKind::err, err);
    attach(statement, FlangWaitSpecKind::id, id);
    attach(statement, FlangWaitSpecKind::iomsg, iomsg);
    attach(statement, FlangWaitSpecKind::iostat, iostat);
    Rose::builder::detail::ValidateFlangWaitStatement(statement);
    return statement->get_unit() == unit && statement->get_end() == end &&
                   statement->get_eor() == eor && statement->get_err() == err &&
                   statement->get_id() == id &&
                   statement->get_iomsg() == iomsg &&
                   statement->get_iostat() == iostat &&
                   unit->get_parent() == statement &&
                   end->get_parent() == statement &&
                   eor->get_parent() == statement &&
                   err->get_parent() == statement &&
                   id->get_parent() == statement &&
                   iomsg->get_parent() == statement &&
                   iostat->get_parent() == statement
               ? 0
               : 1;
  }
  if (mode == "duplicate") {
    attach(statement, FlangWaitSpecKind::unit, integer(10));
    attach(statement, FlangWaitSpecKind::unit, integer(11));
  } else if (mode == "null-expression") {
    attach(statement, FlangWaitSpecKind::unit, nullptr);
  } else if (mode == "owned-expression") {
    SgExpression *expression = integer(10);
    SgWaitStatement *other = new SgWaitStatement();
    ROSE_ASSERT(other != nullptr);
    expression->set_parent(other);
    attach(statement, FlangWaitSpecKind::unit, expression);
  } else if (mode == "null-statement") {
    attach(nullptr, FlangWaitSpecKind::unit, integer(10));
  } else if (mode == "invalid-kind") {
    attach(statement, static_cast<FlangWaitSpecKind>(99), integer(10));
  } else if (mode == "missing-unit") {
    attach(statement, FlangWaitSpecKind::err, integer(300));
    Rose::builder::detail::ValidateFlangWaitStatement(statement);
  } else {
    std::cerr << "unknown mode: " << mode << '\n';
    return 2;
  }

  std::cerr << "WAIT-spec contract unexpectedly returned\n";
  return 1;
}
