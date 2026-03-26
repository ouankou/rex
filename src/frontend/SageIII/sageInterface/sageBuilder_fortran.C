// tps (01/14/2010) : Switching from rose.h to sage3
// test cases are put into tests/nonsmoke/functional/roseTests/astInterfaceTests
#include "sage3basic.h"

#ifndef ROSE_USE_INTERNAL_FRONTEND_DEVELOPMENT
#include "Outliner.hh"

#include "markLhsValues.h"

#include "sageBuilder.h"

#include <fstream>
#endif

using namespace std;
using namespace SageInterface;

namespace {
SgScopeStatement *findImplicitScope(SgScopeStatement *scope) {
  if (scope == nullptr) {
    return nullptr;
  }

  if (SgFunctionDefinition *funcDef =
          SageInterface::getEnclosingFunctionDefinition(scope, true)) {
    if (SgBasicBlock *body = funcDef->get_body()) {
      return body;
    }
  }

  if (SgBasicBlock *block = isSgBasicBlock(scope)) {
    SgNode *parent = block->get_parent();
    if (parent != nullptr && isSgScopeStatement(parent) != nullptr &&
        isSgFunctionDefinition(parent) == nullptr) {
      return block;
    }
  }

  if (SgModuleStatement *moduleStmt =
          SageInterface::getEnclosingModuleStatement(scope, true)) {
    if (SgClassDefinition *moduleDef = moduleStmt->get_definition()) {
      return moduleDef;
    }
  }

  return scope;
}

bool matchesImplicitRange(const std::string &range, char letter) {
  if (range.empty()) {
    return false;
  }
  char start = static_cast<char>(tolower(range[0]));
  char end = start;
  if (range.size() >= 3 && range[1] == '-') {
    end = static_cast<char>(tolower(range[2]));
  }
  if (start > end) {
    std::swap(start, end);
  }
  return letter >= start && letter <= end;
}
} // namespace

//! Put Fortran-specific builders here
// Liao 12/6/2010

// Rasmussen (8/07/2018): created a function to build an implicit type

//! Build a type based on Fortran's implicit typing rules.
//! Currently this interface does not take into account possible implicit
//! statements that change the rules.
SgType *SageBuilder::buildFortranImplicitType(SgName sg_name) {
  // The DEFAULT implicit typing is based on the first letter of the variable
  // name A to H     REAL I to N     INTEGER O to Z     REAL

  SgType *returnType = NULL;
  std::string name = sg_name;

  ROSE_ASSERT(tolower(name[0]) >= 'a');
  ROSE_ASSERT(tolower(name[0]) <= 'z');

  if (SgScopeStatement *scope = SageBuilder::topScopeStack()) {
    SgScopeStatement *implicitScope = findImplicitScope(scope);
    if (implicitScope != nullptr) {
      SgStatementPtrList stmts = implicitScope->generateStatementList();
      const char first = static_cast<char>(tolower(name[0]));
      for (auto it = stmts.rbegin(); it != stmts.rend(); ++it) {
        SgImplicitStatement *implicitStmt = isSgImplicitStatement(*it);
        if (implicitStmt == nullptr) {
          continue;
        }
        if (implicitStmt->get_implicit_spec() !=
            SgImplicitStatement::e_has_implicit_spec_list) {
          continue;
        }
        for (SgInitializedName *init : implicitStmt->get_variables()) {
          if (init == nullptr) {
            continue;
          }
          const std::string range = init->get_name().str();
          if (matchesImplicitRange(range, first)) {
            SgType *implicitType = init->get_type();
            if (implicitType != nullptr) {
              return implicitType;
            }
          }
        }
      }
    }
  }

  if (tolower(name[0]) < 'i') {
    returnType = buildFloatType();
  } else {
    if (tolower(name[0]) < 'o') {
      returnType = buildIntType();
    } else {
      returnType = buildFloatType();
    }
  }

  ROSE_ASSERT(returnType != NULL);
  return returnType;
}

SgAttributeSpecificationStatement *
SageBuilder::buildAttributeSpecificationStatement(
    SgAttributeSpecificationStatement::attribute_spec_enum kind) {
  SgAttributeSpecificationStatement *attributeSpecificationStatement =
      new SgAttributeSpecificationStatement();
  ROSE_ASSERT(attributeSpecificationStatement != NULL);

  attributeSpecificationStatement->set_definingDeclaration(
      attributeSpecificationStatement);
  attributeSpecificationStatement->set_firstNondefiningDeclaration(
      attributeSpecificationStatement);

  attributeSpecificationStatement->set_attribute_kind(kind);

  switch (kind) {
  case SgAttributeSpecificationStatement::e_accessStatement_private:
  case SgAttributeSpecificationStatement::e_accessStatement_public:
  case SgAttributeSpecificationStatement::e_parameterStatement:
  case SgAttributeSpecificationStatement::e_externalStatement:
  case SgAttributeSpecificationStatement::e_dimensionStatement:
  case SgAttributeSpecificationStatement::e_allocatableStatement: {
    SgExprListExp *parameterList{SageBuilder::buildExprListExp_nfi()};
    attributeSpecificationStatement->set_parameter_list(parameterList);
    parameterList->set_parent(attributeSpecificationStatement);
    setSourcePositionForTransformation(parameterList);
    break;
  }
  case SgAttributeSpecificationStatement::e_dataStatement:
    break;
  default:
    cerr << "SageBuilder::buildAttributeSpecificationStatement(), unhandled "
            "attribute specification kind:"
         << kind << endl;
    ROSE_ABORT();
    break;
  }
  setSourcePositionForTransformation(attributeSpecificationStatement);
  return attributeSpecificationStatement;
}

//! Build Fortran include line
SgFortranIncludeLine *
SageBuilder::buildFortranIncludeLine(std::string filename) {
  SgFortranIncludeLine *result = new SgFortranIncludeLine(filename);
  ;
  ROSE_ASSERT(result != NULL);
  result->set_definingDeclaration(result);
  result->set_firstNondefiningDeclaration(result);
  setSourcePositionForTransformation(result);
  return result;
}
//! Build a Fortran common block, possibly with a name
SgCommonBlockObject *
SageBuilder::buildCommonBlockObject(std::string name /*="" */,
                                    SgExprListExp *exp_list /*=NULL*/) {
  SgCommonBlockObject *result = new SgCommonBlockObject();
  ROSE_ASSERT(result != NULL);

  result->set_block_name(name);

  if (exp_list != NULL) {
    result->set_variable_reference_list(exp_list);
    exp_list->set_parent(result);
  }
  setSourcePositionForTransformation(result);
  return result;
}

//! Build a Fortran Common statement
SgCommonBlock *
SageBuilder::buildCommonBlock(SgCommonBlockObject *first_block /*=NULL*/) {
  SgCommonBlock *result = new SgCommonBlock();
  ROSE_ASSERT(result != NULL);

  if (first_block != NULL) {
    result->get_block_list().push_back(first_block);
    first_block->set_parent(result);
  }

  result->set_definingDeclaration(result);
  result->set_firstNondefiningDeclaration(result);

  setSourcePositionForTransformation(result);
  return result;
}
