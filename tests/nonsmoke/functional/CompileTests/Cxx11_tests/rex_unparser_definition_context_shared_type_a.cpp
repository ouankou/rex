typedef class RexDefinitionContextClass {
public:
  int rex_definition_context_member_a;
} RexDefinitionContextClassAlias;

typedef enum RexDefinitionContextEnum {
  rex_definition_context_enumerator_a = 101
} RexDefinitionContextEnumAlias;

static RexDefinitionContextClassAlias rex_definition_context_class_a;
static RexDefinitionContextEnumAlias rex_definition_context_enum_a =
    rex_definition_context_enumerator_a;
