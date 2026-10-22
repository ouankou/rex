#include "rose.h"

namespace {

class OwnedAttribute : public AstIntAttribute {
public:
  explicit OwnedAttribute(int &live) : AstIntAttribute(7), live_(live) {
    ++live_;
  }
  OwnedAttribute(const OwnedAttribute &other)
      : AstIntAttribute(other), live_(other.live_) {
    ++live_;
  }
  ~OwnedAttribute() override { --live_; }
  AstAttribute *copy() const override { return new OwnedAttribute(*this); }

private:
  int &live_;
};

void requireIndependentAttributes() {
  int live = 0;
  auto *source = new SgTypeInt();
  source->setAttribute("rex_copy_payload", new OwnedAttribute(live));
  auto *copy = new SgTypeInt(*source);
  ROSE_ASSERT(copy->get_attributeMechanism() !=
              source->get_attributeMechanism());
  auto *copied =
      dynamic_cast<AstIntAttribute *>(copy->getAttribute("rex_copy_payload"));
  ROSE_ASSERT(copied != nullptr && copied->getValue() == 7);
  ROSE_ASSERT(live == 2);
  delete copy;
  ROSE_ASSERT(live == 1);
  auto *original =
      dynamic_cast<AstIntAttribute *>(source->getAttribute("rex_copy_payload"));
  ROSE_ASSERT(original != nullptr && original->getValue() == 7);
  delete source;
  ROSE_ASSERT(live == 0);
}

void requireIndependentKindSyntax() {
  auto *source = new SgTypeInt();
  source->set_fortran_source_syntax(true);
  source->set_isCoArray(true);
  source->set_hasTypeKindStar(true);
  auto *kind = SageBuilder::buildIntVal(8);
  source->set_type_kind(kind);
  kind->set_parent(source);
  auto *copy = new SgTypeInt(*source);
  ROSE_ASSERT(copy->get_type_kind() != kind);
  ROSE_ASSERT(copy->get_type_kind()->get_parent() == copy);
  ROSE_ASSERT(isSgIntVal(copy->get_type_kind())->get_value() == 8);
  ROSE_ASSERT(copy->get_isCoArray() && copy->get_hasTypeKindStar());
  ROSE_ASSERT(copy->get_fortran_source_syntax());
  delete copy;
  ROSE_ASSERT(SgNode::isLiveNode(kind));
  ROSE_ASSERT(kind->get_parent() == source && kind->get_value() == 8);
  delete source;
}

void requireEmptyReverseCaches(SgType *type) {
  ROSE_ASSERT(type->get_ptr_to() == nullptr);
  ROSE_ASSERT(type->get_ref_to() == nullptr);
  ROSE_ASSERT(type->get_rvalue_ref_to() == nullptr);
  ROSE_ASSERT(type->get_modifiers() == nullptr);
}

void requireIndependentWrappers(SgType *source, SgType *copy) {
  ROSE_ASSERT(copy != nullptr && copy != source);
  requireEmptyReverseCaches(copy);

  SgPointerType *pointer = SgPointerType::createType(copy);
  SgReferenceType *reference = SgReferenceType::createType(copy);
  SgRvalueReferenceType *rvalue = SgRvalueReferenceType::createType(copy);
  ROSE_ASSERT(pointer != source->get_ptr_to());
  ROSE_ASSERT(reference != source->get_ref_to());
  ROSE_ASSERT(rvalue != source->get_rvalue_ref_to());
  ROSE_ASSERT(pointer->get_base_type() == copy);
  ROSE_ASSERT(reference->get_base_type() == copy);
  ROSE_ASSERT(rvalue->get_base_type() == copy);
  ROSE_ASSERT(SgPointerType::createType(copy) == pointer);
  ROSE_ASSERT(SgReferenceType::createType(copy) == reference);
  ROSE_ASSERT(SgRvalueReferenceType::createType(copy) == rvalue);
}

} // namespace

int main() {
  requireIndependentAttributes();
  requireIndependentKindSyntax();
  SgTypeInt *source = new SgTypeInt();
  SgPointerType *pointer = SgPointerType::createType(source);
  SgReferenceType *reference = SgReferenceType::createType(source);
  SgRvalueReferenceType *rvalue = SgRvalueReferenceType::createType(source);

  // Exercise both the generated AST copy and the C++ copy constructor. Both
  // must be correct before a caller creates any wrapper around the new type.
  SgTreeCopy help;
  requireIndependentWrappers(source, isSgType(source->copy(help)));
  requireIndependentWrappers(source, new SgTypeInt(*source));
  ROSE_ASSERT(source->get_ptr_to() == pointer);
  ROSE_ASSERT(source->get_ref_to() == reference);
  ROSE_ASSERT(source->get_rvalue_ref_to() == rvalue);
  return 0;
}
