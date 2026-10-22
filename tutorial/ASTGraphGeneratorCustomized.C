// Example ROSE Translator: used within ROSE/tutorial

#include "rose.h"

#include "AstDOTGeneration.h"

#include "utility_functions.h"

struct ExtraNodeInfo {
  std::string operator()(SgNode *node) {
    AstDOTGenerationExtended_Defaults::NamedExtraNodeInfo name;
    AstDOTGenerationExtended_Defaults::AddressExtraNodeInfo add;
    AstDOTGenerationExtended_Defaults::TypeExtraNodeInfo ty;
    AstDOTGenerationExtended_Defaults::LValueExtraNodeInfo lv;
    std::string extra;
    if (SgExpression *expr = isSgExpression(node)) {
      extra += std::string("Mods: ") +
               (SageInterface::isConstType(expr->get_type()) ? "c" : "") +
               (SageInterface::isVolatileType(expr->get_type()) ? "v" : "") +
               (SageInterface::isReferenceType(expr->get_type()) ? "&" : "") +
               "\\n";
      extra += (expr->isDefinable() ? "Definable\\n" : "!Definable\\n");
      extra += (expr->isUsedAsLValue() ? "Is Used As LValue\\n"
                                       : "Is Not Used As LValue\\n");
      if (SgCastExp *cast = isSgCastExp(expr)) {
        switch (cast->cast_type()) {
        case SgCastExp::e_C_style_cast:
          extra += "C-style\\n";
          break;
        case SgCastExp::e_const_cast:
          extra += "const_cast\\n";
          break;
        case SgCastExp::e_static_cast:
          extra += "static_cast\\n";
          break;
        case SgCastExp::e_dynamic_cast:
          extra += "dynamic_cast\\n";
          break;
        case SgCastExp::e_reinterpret_cast:
          extra += "reinterpret_cast\\n";
          break;
        default:
          break;
        }
      }
    } else if (SgInitializedName *initialized = isSgInitializedName(node)) {
      extra += initialized->get_type()->unparseToString() + "\\n";
    }
    return std::string("\\n") + name(node) + ty(node) + lv(node) + extra +
           add(node);
  }
};

struct ExtraNodeOptions {
  std::string operator()(SgNode *node) {
    AstDOTGenerationExtended_Defaults::DefaultExtraNodeOptions eno;
    return eno(node);
  }
};

int main(int argc, char *argv[]) {
  SgProject *project = frontend(argc, argv);

  generateDOTExtended<ExtraNodeInfo, ExtraNodeOptions>(*project, std::string());

  return 0;
}
