#ifndef __GRAMMARSTRING_H__
#define __GRAMMARSTRING_H__

#include "Rose/StringUtility/FileUtility.h"

#include "Rose/StringUtility.h"

#include "grammar.h"

#include <string>

#include <optional>
#include <utility>

class AstNodeClass;

class GrammarString {
public:
  std::string functionNameString;
  std::pair<std::string, std::string> sourceFragment;

  std::string typeNameString;
  std::string variableNameString;
  std::string defaultInitializerString;

private:
  ConstructParamEnum p_isInConstructorParameterList;
  CopyConstructionOrderEnum p_copyConstructionOrder =
      DEFAULT_COPY_CONSTRUCTION_ORDER;
  std::optional<TraversalCardinalityEnum> p_traversalCardinality;
  TraversalAccessorEnum p_traversalAccessor;
  TraversalStorageEnum p_traversalStorage;
  std::string p_traversalElementTypeName;
  SchemaStorageEnum p_schemaStorage;
  SchemaElementEnum p_schemaElement;
  std::string p_schemaElementTypeName;
  bool p_includeInRTI = true;

  void initializeSchemaMetadata();
  void initializeTraversalMetadata();

public:
  CopyConfigEnum toBeCopied; // used to guide cloning of AST nodes

  TraversalEnum toBeTraversed;

  BuildAccessEnum automaticGenerationOfDataAccessFunctions;

  DeleteEnum toBeDeleted;

  virtual ~GrammarString();
  GrammarString();
  GrammarString(const std::string &inputFunctionNameString,
                std::pair<std::string, std::string> sourceFragment = {});
  GrammarString(const std::string &inputTypeNameString,
                const std::string &inputVariableNameString,
                const std::string &defaultInitializer,
                const ConstructParamEnum &isConstructorParameter,
                const BuildAccessEnum &buildAccessFunctions,
                const TraversalEnum &toBeTraversedDuringTreeTraversal,
                const DeleteEnum &delete_flag,
                const CopyConfigEnum &toBeCopied);
  GrammarString(
      const std::string &inputTypeNameString,
      const std::string &inputVariableNameString,
      const std::string &defaultInitializer,
      const ConstructParamEnum &isConstructorParameter,
      const BuildAccessEnum &buildAccessFunctions,
      const TraversalEnum &toBeTraversedDuringTreeTraversal,
      const DeleteEnum &delete_flag, const CopyConfigEnum &toBeCopied,
      std::optional<TraversalCardinalityEnum> traversalCardinality,
      const TraversalAccessorEnum &traversalAccessor = DIRECT_TRAVERSAL_ACCESS,
      CopyConstructionOrderEnum copyConstructionOrder =
          DEFAULT_COPY_CONSTRUCTION_ORDER);
  GrammarString(const GrammarString &X);
  GrammarString &operator=(const GrammarString &X);

  std::string getConstructorPrototypeParameterString();
  std::string getConstructorSourceParameterString();
  std::string getBaseClassConstructorSourceParameterString();

  std::string getRawString() const;
  std::string getDataPrototypeString() const;
  std::string getDataAccessFunctionPrototypeString() const;

  DeleteEnum getToBeDeleted() const;
  SchemaStorageEnum getSchemaStorage() const { return p_schemaStorage; }
  SchemaElementEnum getSchemaElement() const { return p_schemaElement; }
  const std::string &getSchemaElementTypeName() const {
    return p_schemaElementTypeName;
  }

  friend bool operator!=(const GrammarString &X, const GrammarString &Y);
  friend bool operator==(const GrammarString &X, const GrammarString &Y);

  // Access functions
  const std::string &getTypeNameString() const;
  const std::string &getVariableNameString() const;
  const std::string &getDefaultInitializerString() const;
  void setIsInConstructorParameterList(ConstructParamEnum X);
  ConstructParamEnum getIsInConstructorParameterList() const;

  TraversalEnum getToBeTraversed() const;
  TraversalCardinalityEnum getTraversalCardinality() const;
  TraversalAccessorEnum getTraversalAccessor() const;
  TraversalStorageEnum getTraversalStorage() const;

  CopyConfigEnum getToBeCopied() const { return toBeCopied; }
  CopyConstructionOrderEnum getCopyConstructionOrder() const {
    return p_copyConstructionOrder;
  }

  void setIncludeInRTI(bool include) { p_includeInRTI = include; }
  bool includeInRTI() const { return p_includeInRTI; }

  void setAutomaticGenerationOfDataAccessFunctions(const BuildAccessEnum &X);
  BuildAccessEnum generateDataAccessFunctions() const;

  std::string buildCopyMemberFunctionSource(bool buildConstructorArgument);

  std::string buildCopyMemberFunctionSetParentSource();

  void consistencyCheck() const;

  static bool isContainedIn(const std::string &longString,
                            const std::string &shortString);

  bool generate_override_keyword(std::string variableNameString) const;
};

#endif
