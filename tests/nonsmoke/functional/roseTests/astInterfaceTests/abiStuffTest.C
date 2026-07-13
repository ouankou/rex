#include "rose.h"

#include <iostream>

using namespace std;

int main(int argc, char **argv) {
  SgProject *proj = frontend(argc, argv);
  // Set up the struct layout chain
  I386PrimitiveTypeLayoutGenerator gen1_i386(NULL);
  NonpackedTypeLayoutGenerator gen_i386(&gen1_i386);

  X86_64PrimitiveTypeLayoutGenerator gen1_x86_64(NULL);
  NonpackedTypeLayoutGenerator gen_x86_64(&gen1_x86_64);
  // Process every type used in a variable or parameter declaration
  vector<SgNode *> initNames =
      NodeQuery::querySubTree(proj, V_SgInitializedName);
  for (size_t i = 0; i < initNames.size(); ++i) {
    SgInitializedName *in = isSgInitializedName(initNames[i]);
    SgType *t = in->get_type();
    if (isSgTypeEllipse(t))
      continue;
    if (isSgTypeDefault(t->findBaseType()))
      continue;
    SgSourceFile *source_file = SageInterface::getEnclosingSourceFile(in);
    SgScopeStatement *scope = in->get_scope();
    SgDeclarationStatement *declaration = in->get_declptr();
    ASSERT_not_null(source_file);
    ASSERT_not_null(scope);
    ASSERT_not_null(declaration);
    SgUnparse_Info type_info;
    type_info.set_current_source_file(source_file);
    type_info.set_current_scope(scope);
    type_info.set_template_argument_qualification_context(declaration);
    type_info.set_reference_node_for_qualification(in);
    type_info.set_language(source_file->get_outputLanguage());
    SgType *base_type = t->stripTypedefsAndModifiers();
    SgClassType *class_type = isSgClassType(base_type);
    SgClassDeclaration *class_declaration =
        class_type != nullptr
            ? isSgClassDeclaration(class_type->get_declaration())
            : nullptr;
    cout << in->get_name().getString() << " has type ";
    if (class_declaration != nullptr && class_declaration->get_isUnNamed()) {
      cout << "<anonymous " << class_declaration->class_name() << ">";
    } else {
      cout << t->unparseToString(&type_info);
    }
    cout << ":\n";
    cout << "On i386:\n";
    cout << gen_i386.layoutType(t) << "\n";
    cout << "On x86-64:\n";
    cout << gen_x86_64.layoutType(t) << "\n";
    if (isSgArrayType(t))
      cout << "Array element count is:"
           << SageInterface::getArrayElementCount(isSgArrayType(t)) << endl;
  }
  return 0;
}
