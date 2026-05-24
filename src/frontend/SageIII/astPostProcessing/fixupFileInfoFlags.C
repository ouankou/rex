#include "fixupFileInfoFlags.h"

#include "sage3basic.h"

using namespace Rose;

// documented in header file
size_t fixupFileInfoInconsistanties(SgNode *ast) {
  // DQ (11/14/2015): Added support to fixup the Sg_File_Info objects where they
  // are detected to be inconsistant. This consistancy test is a precursor to
  // possibily moving the API for specification of transformation to the
  // SgLocatedNode since it is redundant and error prown to have it in each of
  // multiple Sg_File_Info objects. Note that the OpenMP transformations are at
  // least one place where this is not set consistantly, so it is more helpful
  // to fix it to be consistant and issue a warning as a part of the process of
  // moving the API and communicating the deprication of the API in the
  // Sg_File_Info over that in the SgLocatedNode.  Most other transformations
  // mechanism now use the API in the SgLocatedNode (but this test will help
  // test that).

  // Note that there may be other flags in the Sg_File_Info objects that shuld
  // be consistant (these can be added to this fixup function).  Other flags to
  // test include:
  //    isCompilerGenerated
  //    isOutputInCodeGeneration
  //    isShared
  //    isFrontendSpecific
  //    isSourcePositionUnavailableInFrontend
  //    isCommentOrDirective
  //    isToken
  //    isDefaultArgument
  //    isImplicitCast
  //
  // Or a less precise mechanism might be to just set the
  // classificationBitField. Note also that not all of these have been or should
  // be moved to the SgLocatedNode API (though this is a subject up for
  // discussion).

  struct T1 : public AstSimpleProcessing {
    size_t nviolations;
    T1() : nviolations(0) {}

    void visit(SgNode *node) {
      SgLocatedNode *located = isSgLocatedNode(node);
      if (located) {
        auto has_real_source = [](const Sg_File_Info *fi) {
          if (fi == NULL) {
            return false;
          }

          const std::string &filename = fi->get_filenameString();
          return fi->get_line() > 0 && !filename.empty() &&
                 filename != "compilerGenerated" && filename != "NULL_FILE" &&
                 fi->isCompilerGenerated() == false &&
                 fi->isFrontendSpecific() == false &&
                 fi->isSourcePositionUnavailableInFrontend() == false;
        };

        auto restore_source_backed_file_info = [&](Sg_File_Info *target,
                                                   const Sg_File_Info *source,
                                                   SgNode *parent) {
          if (target == NULL || source == NULL) {
            return;
          }

          *target = *source;
          target->set_parent(parent);
        };

        auto repair_missing_physical_source = [](Sg_File_Info *fi) {
          if (fi == NULL) {
            return;
          }

          int *physical_file_id = fi->get_physical_file_id_reference();
          if (physical_file_id != NULL && *physical_file_id >= 0) {
            return;
          }

          if (fi->isTransformation() || fi->isCompilerGenerated() ||
              fi->isFrontendSpecific() ||
              fi->isSourcePositionUnavailableInFrontend()) {
            return;
          }

          const int file_id = fi->get_file_id();
          if (file_id < 0) {
            return;
          }

          fi->set_physical_file_id(file_id);
          if (fi->get_physical_line() <= 0 && fi->get_line() > 0) {
            fi->set_physical_line(fi->get_line());
          }
        };

        auto synchronize_with_anchor = [&](Sg_File_Info *target,
                                           const Sg_File_Info *anchor,
                                           SgNode *parent) {
          if (target == NULL || anchor == NULL) {
            return;
          }

          const bool anchor_has_real_source = has_real_source(anchor);
          const std::string &target_filename = target->get_filenameString();
          const bool target_missing_real_source =
              target->get_line() <= 0 || target_filename.empty() ||
              target_filename == "compilerGenerated" ||
              target_filename == "NULL_FILE" ||
              target->isCompilerGenerated() == true ||
              target->isFrontendSpecific() == true ||
              target->isSourcePositionUnavailableInFrontend() == true;

          if (anchor_has_real_source && target_missing_real_source) {
            restore_source_backed_file_info(target, anchor, parent);
            return;
          }

          if (anchor->isCompilerGenerated() != target->isCompilerGenerated()) {
            if (anchor->isCompilerGenerated()) {
              target->setCompilerGenerated();
            } else {
              if (target->get_file_id() ==
                      Sg_File_Info::COMPILER_GENERATED_FILE_ID ||
                  target->get_file_id() ==
                      Sg_File_Info::TRANSFORMATION_FILE_ID ||
                  target->get_file_id() == Sg_File_Info::NULL_FILE_ID) {
                target->set_file_id(anchor->get_file_id());
                target->set_physical_file_id(anchor->get_physical_file_id());
              }
              target->unsetCompilerGenerated();
            }
          }

          if (anchor->isFrontendSpecific() != target->isFrontendSpecific()) {
            if (anchor->isFrontendSpecific()) {
              target->setFrontendSpecific();
            } else {
              if (target->get_file_id() ==
                      Sg_File_Info::COMPILER_GENERATED_FILE_ID ||
                  target->get_file_id() == Sg_File_Info::NULL_FILE_ID) {
                target->set_file_id(anchor->get_file_id());
                target->set_physical_file_id(anchor->get_physical_file_id());
              }
              target->unsetFrontendSpecific();
            }
          }

          if (anchor->isOutputInCodeGeneration() !=
              target->isOutputInCodeGeneration()) {
            if (anchor->isOutputInCodeGeneration()) {
              target->setOutputInCodeGeneration();
            } else {
              target->unsetOutputInCodeGeneration();
            }
          }
        };

        // This test is only looking at the consistancy of the setting of
        // transforamtions across all of the Sg_File_Info objects in a
        // SgLocatedNode (and the extra one in a SgExpression).
        repair_missing_physical_source(located->get_file_info());
        repair_missing_physical_source(located->get_startOfConstruct());
        repair_missing_physical_source(located->get_endOfConstruct());
        if (SgExpression *expression = isSgExpression(located)) {
          repair_missing_physical_source(expression->get_operatorPosition());
        }

        bool result = located->get_startOfConstruct()->isTransformation();

        ROSE_ASSERT(located->get_startOfConstruct() != NULL);
        if (located->get_endOfConstruct() != NULL) {
          if (result != located->get_endOfConstruct()->isTransformation()) {
            // FIX: The Clang frontend may have set p_file_id to
            // TRANSFORMATION_FILE_ID when the node was originally created. We
            // need to restore it to a valid file_id before changing the flag.
            // Use startOfConstruct's file_id as the source of truth for the
            // real file location.
            if (result == false) {
              // We're about to unset transformation - need to restore p_file_id
              // from startOfConstruct
              located->get_endOfConstruct()->set_file_id(
                  located->get_startOfConstruct()->get_file_id());
            }

            if (result == true)
              located->get_endOfConstruct()->setTransformation();
            else
              located->get_endOfConstruct()->unsetTransformation();

            // After changing transformation flag, sync physical_file_id to
            // match
            located->get_endOfConstruct()->set_physical_file_id(
                located->get_endOfConstruct()->get_file_id());

            printf("WARNING: In fixupFileInfoInconsistanties(): located = %p = "
                   "%s testing: get_endOfConstruct()->isTransformation() "
                   "inconsistantly set (set to match startOfConstruct) \n",
                   located, located->class_name().c_str());
            located->get_startOfConstruct()->display(
                "fixupFileInfoInconsistanties()");
          }
          ROSE_ASSERT(located->get_startOfConstruct()->isTransformation() ==
                      located->get_endOfConstruct()->isTransformation());

          synchronize_with_anchor(located->get_endOfConstruct(),
                                  located->get_startOfConstruct(), located);
        } else {
          printf("WARNING: In fixupFileInfoInconsistanties(): located = %p = "
                 "%s testing: get_endOfConstruct() != NULL (failed) \n",
                 located, located->class_name().c_str());
          located->get_startOfConstruct()->display(
              "fixupFileInfoInconsistanties()");
        }

        const SgExpression *expression = isSgExpression(located);
        if (expression != NULL && expression->get_operatorPosition() != NULL) {
          if (result !=
              expression->get_operatorPosition()->isTransformation()) {
            // FIX: The Clang frontend may have set p_file_id to
            // TRANSFORMATION_FILE_ID when the node was originally created. We
            // need to restore it to a valid file_id before changing the flag.
            // Use startOfConstruct's file_id as the source of truth for the
            // real file location.
            if (result == false) {
              // We're about to unset transformation - need to restore p_file_id
              // from startOfConstruct
              expression->get_operatorPosition()->set_file_id(
                  expression->get_startOfConstruct()->get_file_id());
            }

            if (result == true)
              expression->get_operatorPosition()->setTransformation();
            else
              expression->get_operatorPosition()->unsetTransformation();

            // After changing transformation flag, sync physical_file_id to
            // match
            expression->get_operatorPosition()->set_physical_file_id(
                expression->get_operatorPosition()->get_file_id());

            printf("WARNING: In fixupFileInfoInconsistanties(): expression "
                   "located = %p = %s testing: "
                   "get_operatorPosition()->isTransformation() inconsistantly "
                   "set (set to match startOfConstruct) \n",
                   expression, expression->class_name().c_str());
          }
          ROSE_ASSERT(expression->get_startOfConstruct()->isTransformation() ==
                      expression->get_operatorPosition()->isTransformation());

          synchronize_with_anchor(expression->get_operatorPosition(),
                                  expression->get_startOfConstruct(),
                                  const_cast<SgExpression *>(expression));
        }
      }
    }

  } t1;

  t1.traverse(ast, preorder);
  return t1.nviolations;
}
