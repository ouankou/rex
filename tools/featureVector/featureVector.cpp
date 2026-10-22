#include "rose.h"
#include "rose_test_output_path.h"

#include <iterator>

using namespace std;

bool enable_verbose = false;

static void printHelp(const char *program_name) {
  std::cout
      << "Feature Vector: This program prints out the feature\n"
      << "vector of a program to help verifying ROSE output correctness.\n"
      << "\n"
      << "Usage: " << program_name << " [options] <file>\n"
      << "\n"
      << "Options:\n"
      << "  --help            Show this help.\n"
      << "  --rose:help       Show ROSE frontend help.\n"
      << "  --verbose         Enable verbose output.\n";
}

class nodeTraversal : public AstSimpleProcessing {
private:
  string sourcefilename;
  vector<string> debugVector;

public:
  virtual void visit(SgNode *n);

  void setSourceFile(string);

  const vector<string> &getDebugVector() const;

  void getNamedTypeInfo(SgNamedType *namedType);
};

void nodeTraversal::setSourceFile(string filename) {
  this->sourcefilename = filename;
}

const vector<string> &nodeTraversal::getDebugVector() const {
  return debugVector;
}

void nodeTraversal::getNamedTypeInfo(SgNamedType *namedType) {
  SgTemplateInstantiationDecl *templateInstDecl =
      isSgTemplateInstantiationDecl(namedType->get_declaration());
  if (templateInstDecl) {
    SgTemplateArgumentPtrList argPtrList =
        templateInstDecl->get_templateArguments();
    for (SgTemplateArgumentPtrList::iterator tempArgId = argPtrList.begin();
         tempArgId != argPtrList.end(); ++tempArgId) {
      SgTemplateArgument *tempArg = *tempArgId;
      if (tempArg->get_argumentType() == SgTemplateArgument::type_argument) {
        SgType *tempArgType = tempArg->get_type();
        {
          std::ostringstream o;
          o << tempArgType->class_name();
          if (enable_verbose)
            cout << o.str() << endl;
          debugVector.push_back(o.str());
        }
        tempArgType = (isSgTypedefType(tempArgType) != nullptr)
                          ? isSgTypedefType(tempArgType)->get_base_type()
                          : tempArgType;
        SgNamedType *newNamedType = isSgNamedType(tempArgType);
        if (newNamedType != nullptr) {
          getNamedTypeInfo(newNamedType);
        }
      } else {
        SgName mangledName = tempArg->get_mangled_name();
        string mangledNameString = mangledName.getString();
        std::size_t hashStringVal = std::hash<std::string>{}(mangledNameString);
        {
          std::ostringstream o;
          o << "template argument mangled name:" << hashStringVal << ":"
            << mangledNameString;
          if (enable_verbose)
            cout << o.str() << endl;
          debugVector.push_back(o.str());
        }
      }
    }
  }
}

void nodeTraversal::visit(SgNode *n) {
  SgLocatedNode *locatedNode = isSgLocatedNode(n);
  if (locatedNode)
    if (sourcefilename.compare(locatedNode->getFilenameString()) == 0) {
      {
        std::ostringstream o;
        o << n->class_name();
        if (isSgValueExp(n) != nullptr) {
          SgValueExp *valueExp = isSgValueExp(n);
          o << ":" << valueExp->get_constant_folded_value_as_string();
        }

        if (enable_verbose)
          cout << o.str() << endl;
        debugVector.push_back(o.str());
      }

      // retrieve preprocessing information
      AttachedPreprocessingInfoType *info =
          locatedNode->getAttachedPreprocessingInfo();
      if (info) {
        std::ostringstream o;
        for (AttachedPreprocessingInfoType::iterator i = info->begin();
             i != info->end(); i++) {
          PreprocessingInfo::DirectiveType directive =
              (*i)->getTypeOfDirective();
          o << n->class_name() << " ("
            << PreprocessingInfo::relativePositionName(
                   (*i)->getRelativePosition())
            << "):" << PreprocessingInfo::directiveTypeName(directive) << ":"
            << (*i)->getString();
        }
        if (!o.str().empty()) {
          if (enable_verbose)
            cout << o.str() << endl;
          debugVector.push_back(o.str());
        }
      }

      // get type info from SgVariableDeclaration
      SgVariableDeclaration *varDecl = isSgVariableDeclaration(n);
      if (varDecl) {
        SgInitializedNamePtrList nameList = varDecl->get_variables();
        for (SgInitializedNamePtrList::iterator nameId = nameList.begin();
             nameId != nameList.end(); ++nameId) {
          SgInitializedName *initNameId = *nameId;
          SgType *varType = initNameId->get_type()->findBaseType();
          {
            std::ostringstream o;
            o << varType->class_name();
            if (enable_verbose)
              cout << o.str() << endl;
            debugVector.push_back(o.str());
          }
          // get int class type declaration
          SgNamedType *namedType = isSgNamedType(varType);
          if (namedType) {
            getNamedTypeInfo(namedType);
          }
        }
      }
      // get type info from SgTypedefDeclaration
      SgTypedefDeclaration *typedefDecl = isSgTypedefDeclaration(n);
      if (typedefDecl) {
        SgType *basetype = typedefDecl->get_type()->findBaseType();
        {
          std::ostringstream o;
          o << basetype->class_name();
          if (enable_verbose)
            cout << o.str() << endl;
          debugVector.push_back(o.str());
        }
        // get int class type declaration
        SgNamedType *namedType = isSgNamedType(basetype);
        if (namedType) {
          getNamedTypeInfo(namedType);
        }
      }
      SgFunctionDeclaration *funcDecl = isSgFunctionDeclaration(n);
      if (funcDecl) {
        SgInitializedNamePtrList funcArgList = funcDecl->get_args();
        for (SgInitializedNamePtrList::iterator argId = funcArgList.begin();
             argId != funcArgList.end(); ++argId) {
          SgInitializedName *initNameId = *argId;
          SgType *varType = initNameId->get_type()->findBaseType();
          {
            std::ostringstream o;
            o << "funcArgType:" << varType->class_name();
            if (enable_verbose)
              cout << o.str() << endl;
            debugVector.push_back(o.str());
          }
        }
      }
    }
}

int main(int argc, char *argv[]) {
  std::vector<std::string> frontend_args;
  frontend_args.reserve(argc);
  frontend_args.push_back(argv[0]);

  for (int i = 1; i < argc; ++i) {
    std::string arg(argv[i]);
    if (arg == "--help") {
      printHelp(argv[0]);
      return 0;
    }
    if (arg == "--verbose") {
      enable_verbose = true;
      continue;
    }
    if (arg == "--rose:help") {
      frontend_args.push_back("-rose:help");
      continue;
    }
    frontend_args.push_back(arg);
  }

  SgProject *project = frontend(frontend_args);
  ASSERT_not_null(project);
  if (project->get_frontendErrorCode() != 0) {
    const int status = frontendExitStatus(project);
    SageInterface::deleteAST(project,
                             SageInterface::DeleteAstMode::kRequireIsolated);
    return status;
  }

  Rose_STL_Container<std::string> filenames =
      project->getAbsolutePathFileNames();
  for (Rose_STL_Container<std::string>::iterator it = filenames.begin();
       it != filenames.end(); it++)
    cout << "filename: " << *it << endl;
  ROSE_ASSERT(filenames.size() == 1);
  string inputfilename = filenames[0];

  nodeTraversal travese;
  travese.setSourceFile(inputfilename);
  travese.traverseInputFiles(project, preorder);
  std::string outfilename = Rose::TestOutput::resolvePath(
      SageInterface::generateProjectName(project) + ".FV.txt");
  ofstream output_file;
  output_file.exceptions(ios::failbit | ios::badbit);
  output_file.open(outfilename);

  const vector<string> &debugFeatureVector = travese.getDebugVector();
  ostream_iterator<string> debug_iterator(output_file, "\n");
  copy(debugFeatureVector.begin(), debugFeatureVector.end(), debug_iterator);
  output_file.close();
  SageInterface::deleteAST(project,
                           SageInterface::DeleteAstMode::kRequireIsolated);
  return 0;
}
