typedef struct NamedRecord {
  int value;
} *NamedRecordPointer;

typedef struct {
  int value;
} AnonymousRecord;

typedef struct RexTypedefIntroducedRecord *RexTypedefIntroducedPointer;
struct RexTypedefIntroducedRecord {
  int value;
};

typedef enum RexTypedefIntroducedEnum RexTypedefIntroducedEnumAlias;
enum RexTypedefIntroducedEnum { rex_typedef_introduced_zero };

typedef struct RexTypedefReturnTag *RexTypedefFunctionReturn(void);
typedef int (*RexTypedefFunctionParameter)(struct RexTypedefParameterTag *);
typedef int (*RexTypedefEnumParameter)(enum RexTypedefParameterEnum value);

typedef struct RexTypedefAliasRecord {
  int value;
} RexTypedefAliasRecord;

struct RexTypedefAliasFieldOwner {
  RexTypedefAliasRecord rex_direct_alias;
  RexTypedefAliasRecord rex_alias_array[2];
};

union RecordHolder {
  struct NestedRecord {
    int value;
  } nested;
};

struct StandaloneRecord {
  int value;
};

enum StandaloneEnum {
  standalone_zero,
  standalone_one,
};

enum RexIncompleteReturnEnum rex_incomplete_enum_return(void);

#define REX_RETURN_TAG_REFERENCE                                               \
  struct rex_return_tag_reference {                                            \
    int value;                                                                 \
  };                                                                           \
  struct rex_return_tag_reference *rex_return_tag_reference_fn(void);

REX_RETURN_TAG_REFERENCE

struct RexReturnRecord;

struct RexReturnRecord {
  int value;
} *rex_return_owned_record(void);

void rex_parameter_owned_tag(struct RexParameterRecord {
  int value;
} parameter);

int main(void) {
  NamedRecordPointer pointer = 0;
  AnonymousRecord record = {0};
  union RecordHolder holder = {{0}};
  void *raw = 0;
  struct StandaloneRecord *standalone = (struct StandaloneRecord *)raw;
  enum StandaloneEnum value = (enum StandaloneEnum)0;
  return pointer != 0 || record.value != 0 || holder.nested.value != 0 ||
         standalone != 0 || value != standalone_zero;
}
