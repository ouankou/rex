#include "abiStuff.h"

#include "sage3basic.h"

#include "FortranIntegerSelector.h"
#include <limits>

using namespace std;
using namespace SageInterface;

namespace {
SgSourceFile::scalar_abi_layout
requireTargetScalarLayout(SgType *&type, const SgNode *context) {
  ASSERT_not_null(type);
  ASSERT_not_null(context);
  type = type->stripTypedefsAndModifiers();
  if (auto *enum_type = isSgEnumType(type)) {
    auto *declaration = isSgEnumDeclaration(enum_type->get_declaration());
    ASSERT_not_null(declaration);
    if (declaration->get_definingDeclaration() != nullptr)
      declaration = isSgEnumDeclaration(declaration->get_definingDeclaration());
    ASSERT_not_null(declaration);
    type = declaration->get_field_type();
    ASSERT_not_null(type);
    type = type->stripTypedefsAndModifiers();
  }
  ROSE_ASSERT(isSgIntegralType(type) || isSgFloatingPointType(type));
  auto *source = getEnclosingSourceFile(context, true);
  ASSERT_not_null(source);
  const auto &abi = source->get_target_abi();
  ROSE_ASSERT(abi.available);

  VariantT variant = type->variantT();
  switch (variant) {
  case V_SgTypeSignedShort:
    variant = V_SgTypeShort;
    break;
  case V_SgTypeSignedInt:
    variant = V_SgTypeInt;
    break;
  case V_SgTypeSignedLong:
    variant = V_SgTypeLong;
    break;
  case V_SgTypeSignedLongLong:
    variant = V_SgTypeLongLong;
    break;
  default:
    break;
  }
  int kind = 0;
  if (auto *selector = type->get_type_kind()) {
    const auto value = Detail::fortranIntegerSelectorValue(selector);
    ROSE_ASSERT(value && !value->isNegative() && value->words()[1] == 0 &&
                value->words()[0] > 0 &&
                value->words()[0] <= std::numeric_limits<int>::max());
    kind = static_cast<int>(value->words()[0]);
  }
  const auto found = abi.scalars.find({variant, kind});
  ROSE_ASSERT(found != abi.scalars.end());
  ROSE_ASSERT(found->second.size_bits > 0);
  return found->second;
}
} // namespace

unsigned SageInterface::requireTargetScalarSizeBits(SgType *type,
                                                    const SgNode *context) {
  ASSERT_not_null(type);
  type = type->stripTypedefsAndModifiers();
  if (auto *complex = isSgTypeComplex(type)) {
    SgType *component = complex->get_base_type();
    ASSERT_not_null(component);
    component = component->stripTypedefsAndModifiers();
    ROSE_ASSERT(isSgIntegralType(component) ||
                isSgFloatingPointType(component));
    const unsigned component_bits =
        requireTargetScalarSizeBits(component, context);
    ROSE_ASSERT(component_bits <= std::numeric_limits<unsigned>::max() / 2);
    return component_bits * 2;
  }
  if (auto *imaginary = isSgTypeImaginary(type)) {
    SgType *component = imaginary->get_base_type();
    ASSERT_not_null(component);
    ROSE_ASSERT(isSgFloatingPointType(component->stripTypedefsAndModifiers()));
    return requireTargetScalarSizeBits(component, context);
  }
  return requireTargetScalarLayout(type, context).size_bits;
}

unsigned
SageInterface::requireTargetDefaultPointerSizeBits(const SgNode *context) {
  auto *source = getEnclosingSourceFile(context, true);
  ASSERT_not_null(source);
  const auto &abi = source->get_target_abi();
  ROSE_ASSERT(abi.available && abi.pointer_size_bits > 0);
  return abi.pointer_size_bits;
}

void SageInterface::requireTargetIntegerDomain(SgType *type,
                                               const SgNode *context,
                                               bool &is_unsigned,
                                               unsigned &width) {
  const auto layout = requireTargetScalarLayout(type, context);
  ROSE_ASSERT(isSgIntegralType(type));
  // Boolean values have a two-value domain independent of storage size.
  is_unsigned = isSgTypeBool(type) || layout.is_unsigned;
  width = isSgTypeBool(type) ? 1 : layout.size_bits;
}

void SageInterface::requireTargetFloatingDomain(SgType *type,
                                                const SgNode *context,
                                                unsigned &precision,
                                                int &max_exponent) {
  const auto layout = requireTargetScalarLayout(type, context);
  ROSE_ASSERT(isSgFloatingPointType(type));
  ROSE_ASSERT(layout.precision > 0 && layout.max_exponent > 0);
  precision = layout.precision;
  max_exponent = layout.max_exponent;
}

StructLayoutInfo ChainableTypeLayoutGenerator::layoutType(SgType *t) const {
  // Default implementation just passes everything (except typedefs and
  // modifiers) on to next
  switch (t->variantT()) {
  case V_SgTypedefType: {
    SgType *t2 = isSgTypedefType(t)->get_base_type();
    return this->beginning->layoutType(t2);
  }
  case V_SgModifierType: {
    SgType *t2 = isSgModifierType(t)->get_base_type();
    return this->beginning->layoutType(t2);
  }
  default: {
    if (this->next)
      return this->next->layoutType(t);
    std::cerr << "Reached end of chain when trying to lay out a "
              << t->class_name() << std::endl;
    abort();
  }
  }
}

void NonpackedTypeLayoutGenerator::layoutOneField(
    SgType *fieldType, SgNode *decl, bool isUnion, size_t &currentOffset,
    StructLayoutInfo &layout) const {
  StructLayoutInfo fieldInfo = this->beginning->layoutType(fieldType);
  ROSE_ASSERT(fieldInfo.alignment != 0);
  if (fieldInfo.alignment > layout.alignment) {
    layout.alignment = fieldInfo.alignment;
  }
  if (currentOffset % fieldInfo.alignment != 0) {
    size_t paddingNeeded =
        fieldInfo.alignment - (currentOffset % fieldInfo.alignment);
    if (!isUnion) {
      layout.fields.push_back(
          StructLayoutEntry(NULL, currentOffset, paddingNeeded));
    }
    currentOffset += paddingNeeded;
  }
  layout.fields.push_back(
      StructLayoutEntry(decl, currentOffset, fieldInfo.size));
  currentOffset +=
      fieldInfo
          .size; // We need to do this even for unions to set the object size
  if (currentOffset > layout.size) {
    layout.size = currentOffset;
  }
  if (isUnion) {
    ROSE_ASSERT(currentOffset == fieldInfo.size);
    currentOffset = 0;
  }
}

StructLayoutInfo NonpackedTypeLayoutGenerator::layoutType(SgType *t) const {
  switch (t->variantT()) {
  case V_SgClassType: { // Also covers structs and unions
    SgClassDeclaration *decl =
        isSgClassDeclaration(isSgClassType(t)->get_declaration());
    ROSE_ASSERT(decl);
    decl = isSgClassDeclaration(decl->get_definingDeclaration());
    ROSE_ASSERT(decl);
    SgClassDefinition *def = decl->get_definition();
    ROSE_ASSERT(def);
    StructLayoutInfo layout;
    size_t currentOffset = 0;
    const SgBaseClassPtrList &bases = def->get_inheritances();
    for (SgBaseClassPtrList::const_iterator i = bases.begin(); i != bases.end();
         ++i) {
      SgBaseClass *base = *i;
      SgClassDeclaration *basecls = base->get_base_class();
      layoutOneField(basecls->get_type(), base, false, currentOffset, layout);
    }
    const SgDeclarationStatementPtrList &body = def->get_members();
    bool isUnion = (decl->get_class_type() == SgClassDeclaration::e_union);
    for (SgDeclarationStatementPtrList::const_iterator i = body.begin();
         i != body.end(); ++i) {
      SgDeclarationStatement *mem = *i;
      SgVariableDeclaration *vardecl = isSgVariableDeclaration(mem);
      SgClassDeclaration *classdecl = isSgClassDeclaration(mem);
      bool isUnnamedUnion = classdecl ? classdecl->get_isUnNamed() : false;
      if (vardecl) {
        if (!vardecl->get_declarationModifier().isDefault())
          continue; // Static fields and friends
        ROSE_ASSERT(!vardecl->get_bitfield());
        const SgInitializedNamePtrList &vars =
            isSgVariableDeclaration(mem)->get_variables();
        for (SgInitializedNamePtrList::const_iterator j = vars.begin();
             j != vars.end(); ++j) {
          SgInitializedName *var = *j;
          layoutOneField(var->get_type(), var, isUnion, currentOffset, layout);
        }
      } else if (isUnnamedUnion) {
        layoutOneField(classdecl->get_type(), classdecl, isUnion, currentOffset,
                       layout);
      } // else continue;
    }
    if (layout.alignment != 0 && layout.size % layout.alignment != 0) {
      size_t paddingNeeded =
          layout.alignment - (layout.size % layout.alignment);
      if (!isUnion) {
        layout.fields.push_back(
            StructLayoutEntry(NULL, layout.size, paddingNeeded));
      }
      layout.size += paddingNeeded;
    }
    return layout;
  }
  case V_SgArrayType: {
    StructLayoutInfo layout =
        this->beginning->layoutType(isSgArrayType(t)->get_base_type());
    layout.fields.clear();
    SgExpression *numElements = isSgArrayType(t)->get_index();

    SgValueExp *valuexpr = isSgValueExp(numElements);
    if (valuexpr == NULL) {
      cerr << "Error: trying to compute static size of an array with "
              "non-constant size"
           << endl;
      abort();
    }
    layout.size *= SageInterface::getIntegerConstantValue(valuexpr);
    return layout;
  }
  case V_SgTypeComplex: {
    //"Each complex type has the same representation and alignment requirements
    // as an array type containing exactly two elements of the corresponding
    // real
    // type"
    StructLayoutInfo layout =
        this->beginning->layoutType(isSgTypeComplex(t)->get_base_type());
    layout.size *= 2;
    return layout;
  }
  case V_SgTypeImaginary: {
    StructLayoutInfo layout =
        this->beginning->layoutType(isSgTypeImaginary(t)->get_base_type());
    return layout;
  }

  default:
    return ChainableTypeLayoutGenerator::layoutType(t);
  }
}

StructLayoutInfo I386PrimitiveTypeLayoutGenerator::layoutType(SgType *t) const {
  StructLayoutInfo layout;
  switch (t->variantT()) {
  case V_SgTypeBool: {
    layout.size = 1;
    layout.alignment = 1;
    break;
  }

  case V_SgTypeChar: {
    layout.size = 1;
    layout.alignment = 1;
    break;
  }
  case V_SgTypeSignedChar: {
    layout.size = 1;
    layout.alignment = 1;
    break;
  }
  case V_SgTypeUnsignedChar: {
    layout.size = 1;
    layout.alignment = 1;
    break;
  }

  case V_SgTypeShort: {
    layout.size = 2;
    layout.alignment = 2;
    break;
  }
  case V_SgTypeSignedShort: {
    layout.size = 2;
    layout.alignment = 2;
    break;
  }
  case V_SgTypeUnsignedShort: {
    layout.size = 2;
    layout.alignment = 2;
    break;
  }

  case V_SgTypeInt: {
    layout.size = 4;
    layout.alignment = 4;
    break;
  }
  case V_SgEnumType: {
    layout.size = 4;
    layout.alignment = 4;
    break;
  }
  case V_SgTypeSignedInt: {
    layout.size = 4;
    layout.alignment = 4;
    break;
  }
  case V_SgTypeUnsignedInt: {
    layout.size = 4;
    layout.alignment = 4;
    break;
  }

  case V_SgTypeLong: {
    layout.size = 4;
    layout.alignment = 4;
    break;
  }
  case V_SgTypeSignedLong: {
    layout.size = 4;
    layout.alignment = 4;
    break;
  }
  case V_SgTypeUnsignedLong: {
    layout.size = 4;
    layout.alignment = 4;
    break;
  }

  case V_SgTypeLongLong: {
    layout.size = 8;
    layout.alignment = 4;
    break;
  }
  case V_SgTypeUnsignedLongLong: {
    layout.size = 8;
    layout.alignment = 4;
    break;
  }

  case V_SgTypeFloat16: {
    layout.size = 2;
    layout.alignment = 2;
    break;
  }
  case V_SgTypeFloat: {
    layout.size = 4;
    layout.alignment = 4;
    break;
  }
  case V_SgTypeDouble: {
    layout.size = 8;
    layout.alignment = 4;
    break;
  }
  case V_SgTypeLongDouble: {
    layout.size = 12;
    layout.alignment = 4;
    break;
  }

  case V_SgPointerType: {
    layout.size = 4;
    layout.alignment = 4;
    break;
  }
  case V_SgReferenceType: {
    layout.size = 4;
    layout.alignment = 4;
    break;
  }
  default:
    return ChainableTypeLayoutGenerator::layoutType(t);
  }
  return layout;
}

StructLayoutInfo
X86_64PrimitiveTypeLayoutGenerator::layoutType(SgType *t) const {
  StructLayoutInfo layout;
  switch (t->variantT()) {
  case V_SgTypeBool: {
    layout.size = 1;
    layout.alignment = 1;
    break;
  }

  case V_SgTypeChar: {
    layout.size = 1;
    layout.alignment = 1;
    break;
  }
  case V_SgTypeSignedChar: {
    layout.size = 1;
    layout.alignment = 1;
    break;
  }
  case V_SgTypeUnsignedChar: {
    layout.size = 1;
    layout.alignment = 1;
    break;
  }

  case V_SgTypeShort: {
    layout.size = 2;
    layout.alignment = 2;
    break;
  }
  case V_SgTypeSignedShort: {
    layout.size = 2;
    layout.alignment = 2;
    break;
  }
  case V_SgTypeUnsignedShort: {
    layout.size = 2;
    layout.alignment = 2;
    break;
  }

  case V_SgTypeInt: {
    layout.size = 4;
    layout.alignment = 4;
    break;
  }
  case V_SgEnumType: {
    layout.size = 4;
    layout.alignment = 4;
    break;
  }
  case V_SgTypeSignedInt: {
    layout.size = 4;
    layout.alignment = 4;
    break;
  }
  case V_SgTypeUnsignedInt: {
    layout.size = 4;
    layout.alignment = 4;
    break;
  }

  case V_SgTypeLong: {
    layout.size = 8;
    layout.alignment = 8;
    break;
  }
  case V_SgTypeSignedLong: {
    layout.size = 8;
    layout.alignment = 8;
    break;
  }
  case V_SgTypeUnsignedLong: {
    layout.size = 8;
    layout.alignment = 8;
    break;
  }

  case V_SgTypeLongLong: {
    layout.size = 8;
    layout.alignment = 8;
    break;
  }
  case V_SgTypeUnsignedLongLong: {
    layout.size = 8;
    layout.alignment = 8;
    break;
  }

  case V_SgTypeFloat16: {
    layout.size = 2;
    layout.alignment = 2;
    break;
  }
  case V_SgTypeFloat: {
    layout.size = 4;
    layout.alignment = 4;
    break;
  }
  case V_SgTypeDouble: {
    layout.size = 8;
    layout.alignment = 8;
    break;
  }
  case V_SgTypeLongDouble: {
    layout.size = 16;
    layout.alignment = 16;
    break;
  }

  case V_SgPointerType: {
    layout.size = 8;
    layout.alignment = 8;
    break;
  }
  case V_SgReferenceType: {
    layout.size = 8;
    layout.alignment = 8;
    break;
  }
  default:
    return ChainableTypeLayoutGenerator::layoutType(t);
  }
  return layout;
}

ostream &operator<<(ostream &o, const StructLayoutEntry &e) {
  string label;
  if (isSgInitializedName(e.decl)) {
    label = "Field " + isSgInitializedName(e.decl)->get_name().getString();
  } else if (isSgClassDeclaration(e.decl)) {
    label = "Anonymous union";
  } else if (!e.decl) {
    label = "Padding";
  } else {
    cerr << "Bad decl kind " << e.decl->class_name() << endl;
    abort();
  }
  o << label << " at " << e.byteOffset << " size " << e.fieldSize;
  if (e.bitFieldContainerSize != 0) {
    o << " (bit field at bit " << e.bitOffset << " of word of size "
      << e.bitFieldContainerSize << ")";
  }
  return o;
}

ostream &operator<<(ostream &o, const StructLayoutInfo &i) {
  o << "Type with size " << i.size << " alignment " << i.alignment;
  if (!i.fields.empty()) {
    o << " fields: {\n";
    for (size_t j = 0; j < i.fields.size(); ++j) {
      o << "  " << i.fields[j] << "\n";
    }
    o << "}";
  }
  return o;
}
