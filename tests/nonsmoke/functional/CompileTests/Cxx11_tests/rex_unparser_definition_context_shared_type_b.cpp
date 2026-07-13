typedef class RexDefinitionContextClass {
public:
  long rex_definition_context_member_b;
} RexDefinitionContextClassAlias;

typedef enum RexDefinitionContextEnum {
  rex_definition_context_enumerator_b = 202
} RexDefinitionContextEnumAlias;

static RexDefinitionContextClassAlias rex_definition_context_class_b;
static RexDefinitionContextEnumAlias rex_definition_context_enum_b =
    rex_definition_context_enumerator_b;
