#include "checkFortranInterfaces.h"
#include "rose.h"

#include <iostream>
#include <map>
#include <stdexcept>
#include <string>
#include <vector>

namespace {
struct Interface {
  SgFunctionDeclaration *declaration;
  SgSourceFile *file;
};

class UnsupportedAbi : public std::runtime_error {
public:
  using std::runtime_error::runtime_error;
};

SgType *unqualified(SgType *type) {
  ROSE_ASSERT(type != nullptr);
  return type->stripType(SgType::STRIP_TYPEDEF_TYPE |
                         SgType::STRIP_MODIFIER_TYPE);
}

SgSourceFile::target_abi_info target(const Interface &interface) {
  ROSE_ASSERT(interface.file != nullptr);
  const auto &abi = interface.file->get_target_abi();
  if (!abi.available || abi.pointer_size_bits == 0 ||
      abi.pointer_alignment_bits == 0 || abi.scalars.empty()) {
    std::cerr << "REX_AST_INVARIANT[interface-target-abi]: source file has no "
                 "complete frontend target layout\n";
    ROSE_ABORT();
  }
  return abi;
}

const SgSourceFile::scalar_abi_layout &
scalarLayout(const SgSourceFile::target_abi_info &abi, VariantT variant,
             int kind) {
  const auto found = abi.scalars.find({variant, kind});
  if (found == abi.scalars.end()) {
    throw UnsupportedAbi("no frontend layout for scalar variant " +
                         std::to_string(variant) + " and KIND " +
                         std::to_string(kind));
  }
  return found->second;
}

int fortranKind(SgType *type) {
  ROSE_ASSERT(type != nullptr);
  auto *kind = isSgIntVal(type->get_type_kind());
  if (type->get_fortran_source_syntax() || kind == nullptr ||
      kind->get_value() <= 0) {
    std::cerr << "REX_AST_INVARIANT[interface-fortran-kind]: parameter or "
                 "result has no resolved semantic KIND\n";
    ROSE_ABORT();
  }
  return kind->get_value();
}

bool integerType(SgType *type) {
  switch (type->variantT()) {
  case V_SgTypeChar:
  case V_SgTypeSignedChar:
  case V_SgTypeUnsignedChar:
  case V_SgTypeShort:
  case V_SgTypeUnsignedShort:
  case V_SgTypeInt:
  case V_SgTypeUnsignedInt:
  case V_SgTypeLong:
  case V_SgTypeUnsignedLong:
  case V_SgTypeLongLong:
  case V_SgTypeUnsignedLongLong:
  case V_SgTypeSigned128bitInteger:
  case V_SgTypeUnsigned128bitInteger:
    return true;
  default:
    return false;
  }
}

bool floatingType(SgType *type) {
  return isSgTypeFloat(type) || isSgTypeDouble(type) ||
         isSgTypeLongDouble(type) || isSgTypeFloat128(type);
}

class Comparison {
public:
  Comparison(Interface fortran, Interface c)
      : fortran_(fortran), c_(c), fortranAbi_(target(fortran)),
        cAbi_(target(c)) {}

  bool compare() const {
    const auto &fabi = fortranAbi_;
    const auto &cabi = cAbi_;
    if (fabi.big_endian != cabi.big_endian ||
        fabi.pointer_size_bits != cabi.pointer_size_bits ||
        fabi.pointer_alignment_bits != cabi.pointer_alignment_bits) {
      return false;
    }
    const auto &fargs = fortran_.declaration->get_args();
    const auto &cargs = c_.declaration->get_args();
    if (fargs.size() != cargs.size()) {
      return false;
    }
    ROSE_ASSERT(fortran_.declaration->get_type() != nullptr);
    ROSE_ASSERT(c_.declaration->get_type() != nullptr);
    if (!compareObject(fortran_.declaration->get_type()->get_return_type(),
                       c_.declaration->get_type()->get_return_type())) {
      return false;
    }
    for (std::size_t index = 0; index < fargs.size(); ++index) {
      if (!compareArgument(fargs[index], cargs[index])) {
        return false;
      }
    }
    return true;
  }

private:
  bool compareArgument(SgInitializedName *fortran, SgInitializedName *c) const {
    ROSE_ASSERT(fortran != nullptr && c != nullptr);
    SgType *ftype = unqualified(fortran->get_type());
    SgType *ctype = unqualified(c->get_type());
    switch (fortran->get_fortran_c_argument_passing()) {
    case SgInitializedName::e_fortran_c_argument_value:
      return compareObject(ftype, ctype);
    case SgInitializedName::e_fortran_c_argument_reference: {
      auto *pointer = isSgPointerType(ctype);
      // A C array parameter is adjusted to a pointer by the frontend.
      if (pointer == nullptr) {
        return false;
      }
      ctype = unqualified(pointer->get_base_type());
      // Explicit and assumed-size Fortran arrays pass a contiguous base
      // address. C parameter bounds are not part of their ABI signature.
      while (auto *array = isSgArrayType(ftype)) {
        ftype = unqualified(array->get_base_type());
      }
      while (auto *array = isSgArrayType(ctype)) {
        ctype = unqualified(array->get_base_type());
      }
      return compareObject(ftype, ctype);
    }
    case SgInitializedName::e_fortran_c_argument_descriptor:
      throw UnsupportedAbi(
          "descriptor argument requires a typed C descriptor contract");
    case SgInitializedName::e_fortran_c_argument_procedure:
    case SgInitializedName::e_fortran_c_argument_procedure_pointer:
      throw UnsupportedAbi(
          "procedure argument requires its complete typed interface");
    case SgInitializedName::e_fortran_c_argument_none:
      std::cerr << "REX_AST_INVARIANT[interface-fortran-passing]: BIND(C) "
                   "parameter has no frontend passing convention\n";
      ROSE_ABORT();
    }
    ROSE_ABORT();
  }

  bool compareObject(SgType *fortran, SgType *c) const {
    fortran = unqualified(fortran);
    c = unqualified(c);
    if (isSgTypeVoid(fortran) || isSgTypeVoid(c)) {
      return isSgTypeVoid(fortran) && isSgTypeVoid(c);
    }
    if (auto *complex = isSgTypeComplex(fortran)) {
      auto *other = isSgTypeComplex(c);
      return other != nullptr &&
             compareObject(complex->get_base_type(), other->get_base_type());
    }
    if (isSgClassType(fortran) || isSgClassType(c)) {
      throw UnsupportedAbi(
          "derived or record type requires complete frontend record layouts");
    }
    if (auto *character = isSgTypeString(fortran)) {
      auto *length = isSgIntVal(character->get_lengthExpression());
      if (length == nullptr || length->get_value() != 1) {
        throw UnsupportedAbi(
            "character ABI requires a resolved interoperable length");
      }
      if (!isSgTypeChar(c)) {
        return false;
      }
    } else if (integerType(fortran)) {
      if (!integerType(c)) {
        return false;
      }
    } else if (floatingType(fortran)) {
      if (!floatingType(c)) {
        return false;
      }
    } else if (isSgTypeBool(fortran)) {
      if (!isSgTypeBool(c)) {
        return false;
      }
    } else {
      throw UnsupportedAbi("unsupported semantic object type " +
                           fortran->class_name());
    }
    const auto &fortranLayout =
        scalarLayout(fortranAbi_, fortran->variantT(), fortranKind(fortran));
    const auto &cLayout = scalarLayout(cAbi_, c->variantT(), 0);
    // Representation compatibility does not require equal signed value ranges.
    return fortranLayout.size_bits == cLayout.size_bits &&
           fortranLayout.alignment_bits == cLayout.alignment_bits &&
           fortranLayout.precision == cLayout.precision &&
           fortranLayout.max_exponent == cLayout.max_exponent;
  }

  Interface fortran_;
  Interface c_;
  SgSourceFile::target_abi_info fortranAbi_;
  SgSourceFile::target_abi_info cAbi_;
};
} // namespace

int checkFortranInterfaces(SgProject *project) {
  ROSE_ASSERT(project != nullptr);
  std::map<std::string, std::vector<Interface>> fortranInterfaces;
  std::map<std::string, std::vector<Interface>> cInterfaces;
  for (SgFile *input : project->get_fileList()) {
    auto *file = isSgSourceFile(input);
    ROSE_ASSERT(file != nullptr);
    const auto language = file->get_inputLanguage();
    for (SgNode *node :
         NodeQuery::querySubTree(file, V_SgFunctionDeclaration)) {
      auto *declaration = isSgFunctionDeclaration(node);
      ROSE_ASSERT(declaration != nullptr);
      if (declaration->get_firstNondefiningDeclaration() != declaration) {
        continue;
      }
      if (language == SgFile::e_Fortran_language) {
        auto *procedure = isSgProcedureHeaderStatement(declaration);
        if (procedure == nullptr ||
            !procedure->get_declarationModifier().isBind() ||
            procedure->get_fortran_binding_name().is_null()) {
          continue;
        }
        fortranInterfaces[procedure->get_fortran_binding_name().getString()]
            .push_back({procedure, file});
      } else if ((language == SgFile::e_C_language ||
                  (language == SgFile::e_Cxx_language &&
                   declaration->get_linkage() == "C")) &&
                 !isSgMemberFunctionDeclaration(declaration) &&
                 !declaration->get_declarationModifier()
                      .get_storageModifier()
                      .isStatic()) {
        cInterfaces[declaration->get_name().getString()].push_back(
            {declaration, file});
      }
    }
  }
  if (fortranInterfaces.empty()) {
    std::cerr << "No externally bound Fortran procedure interface was found.\n";
    return 2;
  }
  int result = 0;
  for (const auto &[name, declarations] : fortranInterfaces) {
    const auto found = cInterfaces.find(name);
    if (found == cInterfaces.end()) {
      std::cerr << "Missing C interface for binding '" << name << "'.\n";
      result = 1;
      continue;
    }
    for (const Interface &fortran : declarations) {
      for (const Interface &c : found->second) {
        try {
          if (Comparison(fortran, c).compare()) {
            std::cout << "Compatible ABI for binding '" << name << "'.\n";
          } else {
            std::cerr << "Incompatible ABI for binding '" << name << "'.\n";
            result = 1;
          }
        } catch (const UnsupportedAbi &error) {
          std::cerr << "Cannot verify binding '" << name
                    << "': " << error.what() << ".\n";
          if (result == 0) {
            result = 2;
          }
        }
      }
    }
  }
  return result;
}
