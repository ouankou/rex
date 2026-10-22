#ifdef TEMPLATE_IMPLEMENTATIONS

#ifndef UTILITY_FUNCTION_TEMPLATES_C
#define UTILITY_FUNCTION_TEMPLATES_C

#include "AstDOTGeneration.h"

#include "wholeAST_API.h"

template <typename ExtraNodeInfo_t, typename ExtraNodeOptions_t,
          typename ExtraEdgeInfo_t, typename ExtraEdgeOptions_t>
struct generateDOTExtended {
  generateDOTExtended(const SgProject &project,
                      std::string filenamePostfix = "",
                      ExtraNodeInfo_t eni = ExtraNodeInfo_t(),
                      ExtraNodeOptions_t eno = ExtraNodeOptions_t(),
                      ExtraEdgeInfo_t eei = ExtraEdgeInfo_t(),
                      ExtraEdgeOptions_t eeo = ExtraEdgeOptions_t()) {
    AstDOTGenerationExtended<ExtraNodeInfo_t, ExtraNodeOptions_t,
                             ExtraEdgeInfo_t, ExtraEdgeOptions_t>
        astdotgen(eni, eno, eei, eeo);
    SgProject &nonconstProject = (SgProject &)project;

    astdotgen.generateInputFiles(&nonconstProject,
                                 DOTGeneration<SgNode *>::TOPDOWNBOTTOMUP,
                                 filenamePostfix);
  }
};

template <typename ExtraNodeInfo_t, typename ExtraNodeOptions_t,
          typename ExtraEdgeInfo_t, typename ExtraEdgeOptions_t>
struct generateDOTExtended_withIncludes {
  generateDOTExtended_withIncludes(
      const SgProject &project, std::string filenamePostfix = "",
      ExtraNodeInfo_t eni =
          AstDOTGenerationExtended_Defaults::DefaultExtraNodeInfo(),
      ExtraNodeOptions_t eno =
          AstDOTGenerationExtended_Defaults::DefaultExtraNodeOptions(),
      ExtraEdgeInfo_t eei =
          AstDOTGenerationExtended_Defaults::DefaultExtraEdgeInfo(),
      ExtraEdgeOptions_t eeo =
          AstDOTGenerationExtended_Defaults::DefaultExtraEdgeOptions()) {
    AstDOTGenerationExtended<ExtraNodeInfo_t, ExtraNodeOptions_t,
                             ExtraEdgeInfo_t, ExtraEdgeOptions_t>
        astdotgen(eni, eno, eei, eeo);
    SgProject &nonconstProject = (SgProject &)project;

    astdotgen.generate(&nonconstProject,
                       DOTGeneration<SgNode *>::TOPDOWNBOTTOMUP,
                       filenamePostfix);
  }
};

template <typename ExtraNodeInfo_t, typename ExtraNodeOptions_t,
          typename ExtraEdgeInfo_t, typename ExtraEdgeOptions_t>
struct generateDOTExtendedforMultipleFile {
  generateDOTExtendedforMultipleFile(
      const SgProject &project, std::string filenamePostfix,
      ExtraNodeInfo_t eni =
          AstDOTGenerationExtended_Defaults::DefaultExtraNodeInfo(),
      ExtraNodeOptions_t eno =
          AstDOTGenerationExtended_Defaults::DefaultExtraNodeOptions(),
      ExtraEdgeInfo_t eei =
          AstDOTGenerationExtended_Defaults::DefaultExtraEdgeInfo(),
      ExtraEdgeOptions_t eeo =
          AstDOTGenerationExtended_Defaults::DefaultExtraEdgeOptions()) {
    generateDOTExtended_withIncludes<ExtraNodeInfo_t, ExtraNodeOptions_t,
                                     ExtraEdgeInfo_t, ExtraEdgeOptions_t>(
        project, filenamePostfix, eni, eno, eei, eeo);
  }
};

#endif // UTILITY_FUNCTION_TEMPLATES_C

#else
#error Must include template implementation files after all header files.  If you have, make sure you define TEMPLATE_IMPLEMENTATIONS
#endif // TEMPLATE_IMPLEMENTATIONS
