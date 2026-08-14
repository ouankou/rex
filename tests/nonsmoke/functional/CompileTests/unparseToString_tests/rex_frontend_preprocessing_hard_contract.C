#include "rose.h"

#include "IncludeDirective.h"
#include "attachPreprocessingInfo.h"

#include <cstdio>
#include <map>
#include <set>
#include <string>

namespace {

constexpr char kFilename[] = "rex_frontend_preprocessing_contract.c";

PreprocessingInfo makeDirective(PreprocessingInfo::DirectiveType type,
                                const std::string &spelling) {
  return PreprocessingInfo(type, spelling, kFilename, 1, 1, 1,
                           PreprocessingInfo::before);
}

} // namespace

int main(int argc, char **argv) {
  if (argc == 1) {
    {
      IncludeDirective angled(
          "#include <bits/stl_function.h> // less<void>, equal_to<void>\n");
      ROSE_ASSERT(angled.getIncludedPath() == "bits/stl_function.h");
      ROSE_ASSERT(!angled.isQuotedInclude());
    }
    {
      IncludeDirective quoted(
          "  # include_next \"rex/path.hpp\" // \"not-the-target\"\n");
      ROSE_ASSERT(quoted.getIncludedPath() == "rex/path.hpp");
      ROSE_ASSERT(quoted.isQuotedInclude());
    }
    {
      IncludeDirective macro(
          "#include REX_HEADER(rex, path) /* not part of the target */\n");
      ROSE_ASSERT(macro.getIncludedPath() == "REX_HEADER(rex, path)");
      ROSE_ASSERT(!macro.isQuotedInclude());
      ROSE_ASSERT(macro.getTargetLength() == macro.getIncludedPath().size());
    }
    {
      const std::string spelling =
          "#include REX_HEADER(/* exact internal trivia */ rex, \\\n"
          "path) /* trailing trivia */\n";
      IncludeDirective macro(spelling);
      ROSE_ASSERT(macro.getIncludedPath() ==
                  "REX_HEADER(/* exact internal trivia */ rex, path)");
      ROSE_ASSERT(!macro.isQuotedInclude());
      ROSE_ASSERT(
          spelling.substr(macro.getTargetStartPos(), macro.getTargetLength()) ==
          "REX_HEADER(/* exact internal trivia */ rex, \\\npath)");
    }
    {
      const std::string spelling = "#inclu\\\nde_\\\nnext \"rex/path.hpp\"";
      IncludeDirective splicedKeyword(spelling);
      ROSE_ASSERT(splicedKeyword.getIncludedPath() == "rex/path.hpp");
      ROSE_ASSERT(splicedKeyword.isQuotedInclude());
      ROSE_ASSERT(spelling.substr(splicedKeyword.getTargetStartPos(),
                                  splicedKeyword.getTargetLength()) ==
                  "\"rex/path.hpp\"");
    }
    {
      const std::string spelling = "#include_next <rex/long\\\r\n/path.hpp>";
      IncludeDirective splicedTarget(spelling);
      ROSE_ASSERT(splicedTarget.getIncludedPath() == "rex/long/path.hpp");
      ROSE_ASSERT(!splicedTarget.isQuotedInclude());
      ROSE_ASSERT(spelling.substr(splicedTarget.getTargetStartPos(),
                                  splicedTarget.getTargetLength()) ==
                  "<rex/long\\\r\n/path.hpp>");
    }
    PreprocessingInfo functionLike = makeDirective(
        PreprocessingInfo::CpreprocessorDefineDeclaration,
        "  # define /* exact trivia */ REX_FUNCTION(value) ((value) + 1)");
    ROSE_ASSERT(functionLike.getMacroName() == "REX_FUNCTION");

    PreprocessingInfo lfSplices =
        makeDirective(PreprocessingInfo::CpreprocessorDefineDeclaration,
                      "#def\\"
                      "\n"
                      "ine RE\\"
                      "\n"
                      "X_SPLICE(value) (value)");
    ROSE_ASSERT(lfSplices.getMacroName() == "REX_SPLICE");

    PreprocessingInfo crlfSplices = makeDirective(
        PreprocessingInfo::CpreprocessorDefineDeclaration, "#def\\"
                                                           "\r\n"
                                                           "ine REX_\\"
                                                           "\r\n"
                                                           "CRLF 1");
    ROSE_ASSERT(crlfSplices.getMacroName() == "REX_CRLF");

    PreprocessingInfo separatorAfterSplice =
        makeDirective(PreprocessingInfo::CpreprocessorDefineDeclaration,
                      "#define\\"
                      "\n"
                      " REX_SEPARATOR_SPLICE 1");
    ROSE_ASSERT(separatorAfterSplice.getMacroName() == "REX_SEPARATOR_SPLICE");

    ROSEAttributesList attributes;
    PreprocessingInfo::DirectiveType kind =
        PreprocessingInfo::CpreprocessorUnknownDeclaration;
    std::string rest;
    ROSE_ASSERT(
        attributes.isCppDirective("  #pragma rex collector", kind, rest));
    ROSE_ASSERT(kind == PreprocessingInfo::CpreprocessorPragmaDeclaration);
    ROSE_ASSERT(rest == " rex collector");
    ROSE_ASSERT(!attributes.isFortran90Comment(""));
    ROSE_ASSERT(attributes.isFortran77Comment(""));
    return 0;
  }

  const std::string mode = argv[1];
  if (mode == "missing-define-keyword") {
    (void)makeDirective(PreprocessingInfo::CpreprocessorDefineDeclaration,
                        "#undef REX_BROKEN");
  } else if (mode == "missing-macro-name") {
    (void)makeDirective(PreprocessingInfo::CpreprocessorDefineDeclaration,
                        "#define /* nothing */");
  } else if (mode == "wrong-directive-kind") {
    PreprocessingInfo comment =
        makeDirective(PreprocessingInfo::CplusplusStyleComment, "// comment");
    (void)comment.getMacroName();
  } else if (mode == "unterminated-angled-include") {
    (void)IncludeDirective("#include <rex/missing.hpp");
  } else if (mode == "tokens-after-header-name") {
    (void)IncludeDirective("#include <rex/path.hpp> REX_EXTRA");
  } else if (mode == "missing-include-target") {
    (void)IncludeDirective("#include");
  } else if (mode == "missing-separator-after-splice") {
    (void)makeDirective(PreprocessingInfo::CpreprocessorDefineDeclaration,
                        "#define\\"
                        "\n"
                        "REX_BROKEN 1");
  } else if (mode == "unknown-hash-directive") {
    ROSEAttributesList attributes;
    PreprocessingInfo::DirectiveType kind;
    std::string rest;
    (void)attributes.isCppDirective("#rex_unknown payload", kind, rest);
  } else if (mode == "empty-hash-directive") {
    ROSEAttributesList attributes;
    PreprocessingInfo::DirectiveType kind;
    std::string rest;
    (void)attributes.isCppDirective(" # \t", kind, rest);
  } else if (mode == "invalid-hash-identifier") {
    ROSEAttributesList attributes;
    PreprocessingInfo::DirectiveType kind;
    std::string rest;
    (void)attributes.isCppDirective("#@invalid", kind, rest);
  } else if (mode == "null-list-entry") {
    ROSEAttributesList attributes;
    attributes.getList().push_back(NULL);
    attributes.display("hard-contract");
  } else if (mode == "invalid-fortran-fixed-byte") {
    ROSEAttributesList attributes;
    const std::string sourceLine(1, static_cast<char>(0xff));
    (void)attributes.isFortran77Comment(sourceLine);
  } else if (mode == "scan-valid-source") {
    ROSE_ASSERT(argc == 3);
    ROSEAttributesList *attributes =
        getPreprocessorDirectives(argv[2], argv[2]);
    ROSE_ASSERT(attributes != NULL);
    const std::set<std::string> expected{
        "REX_COMMENT_PREFIX", "REX_HASH_SPLICE", "REX_LEADING_COMMENT",
        "REX_SEPARATOR_SPLICE", "REX_SPLICED"};
    std::set<std::string> actual;
    bool sawRawLineSplice = false;
    for (PreprocessingInfo *entry : attributes->getList()) {
      ROSE_ASSERT(entry != NULL);
      if (entry->getTypeOfDirective() !=
          PreprocessingInfo::CpreprocessorDefineDeclaration) {
        continue;
      }
      actual.insert(entry->getMacroName());
      sawRawLineSplice = sawRawLineSplice ||
                         entry->getString().find("\\\n") != std::string::npos;
    }
    ROSE_ASSERT(actual == expected);
    ROSE_ASSERT(sawRawLineSplice);
    delete attributes;
    return 0;
  } else if (mode == "scan-include-directives") {
    ROSE_ASSERT(argc == 3);
    ROSEAttributesList *attributes =
        getPreprocessorDirectives(argv[2], argv[2]);
    ROSE_ASSERT(attributes != NULL);
    const std::map<std::string, PreprocessingInfo::DirectiveType> expected{
        {"rex_regular.hpp", PreprocessingInfo::CpreprocessorIncludeDeclaration},
        {"rex_next.hpp",
         PreprocessingInfo::CpreprocessorIncludeNextDeclaration},
        {"rex_spliced_next.hpp",
         PreprocessingInfo::CpreprocessorIncludeNextDeclaration}};
    std::map<std::string, PreprocessingInfo::DirectiveType> actual;
    for (PreprocessingInfo *entry : attributes->getList()) {
      ROSE_ASSERT(entry != NULL);
      const PreprocessingInfo::DirectiveType type = entry->getTypeOfDirective();
      ROSE_ASSERT(type == PreprocessingInfo::CpreprocessorIncludeDeclaration ||
                  type ==
                      PreprocessingInfo::CpreprocessorIncludeNextDeclaration);
      IncludeDirective directive(entry->getString());
      ROSE_ASSERT(actual.emplace(directive.getIncludedPath(), type).second);
    }
    ROSE_ASSERT(actual == expected);
    delete attributes;
    return 0;
  } else if (mode == "scan-fortran-source") {
    ROSE_ASSERT(argc == 3);
    ROSEAttributesList attributes;
    attributes.collectPreprocessorDirectivesAndCommentsForAST(
        argv[2], ROSEAttributesList::e_Fortran9x_language);

    bool sawPragma = false;
    bool sawSplitKeywordDefine = false;
    bool sawMultilineDefine = false;
    bool sawFollowingDefine = false;
    bool sawLinemarker = false;
    for (PreprocessingInfo *entry : attributes.getList()) {
      ROSE_ASSERT(entry != NULL);
      if (entry->getTypeOfDirective() ==
          PreprocessingInfo::CpreprocessorPragmaDeclaration) {
        ROSE_ASSERT(!sawPragma);
        ROSE_ASSERT(entry->getLineNumber() == 1);
        ROSE_ASSERT(entry->getColumnNumber() == 1);
        ROSE_ASSERT(entry->getString() == "#pragma rex collector");
        sawPragma = true;
      } else if (entry->getTypeOfDirective() ==
                 PreprocessingInfo::CpreprocessorDefineDeclaration) {
        if (entry->getMacroName() == "REX_SPLIT") {
          ROSE_ASSERT(!sawSplitKeywordDefine);
          ROSE_ASSERT(entry->getLineNumber() == 2);
          ROSE_ASSERT(entry->getString() == "#def\\\nine REX_SPLIT 1");
          sawSplitKeywordDefine = true;
        } else if (entry->getMacroName() == "REX_MULTI") {
          ROSE_ASSERT(!sawMultilineDefine);
          ROSE_ASSERT(entry->getLineNumber() == 4);
          ROSE_ASSERT(entry->getString() == "#define REX_MULTI(value) \\\n");
          sawMultilineDefine = true;
        } else {
          ROSE_ASSERT(entry->getMacroName() == "REX_AFTER");
          ROSE_ASSERT(!sawFollowingDefine);
          ROSE_ASSERT(entry->getLineNumber() == 6);
          sawFollowingDefine = true;
        }
      } else if (entry->getTypeOfDirective() ==
                 PreprocessingInfo::CpreprocessorCompilerGeneratedLinemarker) {
        ROSE_ASSERT(!sawLinemarker);
        ROSE_ASSERT(entry->getLineNumber() == 7);
        ROSE_ASSERT(entry->get_lineNumberForCompilerGeneratedLinemarker() ==
                    42);
        ROSE_ASSERT(entry->get_filenameForCompilerGeneratedLinemarker() ==
                    "\"generated source.f90\"");
        ROSE_ASSERT(entry->get_optionalflagsForCompilerGeneratedLinemarker() ==
                    "1 3");
        sawLinemarker = true;
      } else {
        ROSE_ABORT();
      }
    }
    ROSE_ASSERT(attributes.getList().size() == 5);
    ROSE_ASSERT(sawPragma && sawSplitKeywordDefine && sawMultilineDefine &&
                sawFollowingDefine && sawLinemarker);
    return 0;
  } else if (mode == "scan-invalid-source") {
    ROSE_ASSERT(argc == 3);
    ROSEAttributesList *attributes =
        getPreprocessorDirectives(argv[2], argv[2]);
    delete attributes;
  } else if (mode == "scan-invalid-fortran-source") {
    ROSE_ASSERT(argc == 3);
    ROSEAttributesList attributes;
    attributes.collectPreprocessorDirectivesAndCommentsForAST(
        argv[2], ROSEAttributesList::e_Fortran9x_language);
  } else if (mode == "missing-source-file") {
    ROSE_ASSERT(argc == 3);
    std::remove(argv[2]);
    ROSEAttributesList attributes;
    attributes.collectPreprocessorDirectivesAndCommentsForAST(
        argv[2], ROSEAttributesList::e_C_language);
  } else {
    fprintf(stderr, "unknown preprocessing contract mode: %s\n", mode.c_str());
    return 2;
  }

  fprintf(stderr, "preprocessing contract mode unexpectedly returned: %s\n",
          mode.c_str());
  return 3;
}
