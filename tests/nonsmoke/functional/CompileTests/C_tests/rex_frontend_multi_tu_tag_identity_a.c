struct rex_c_named_record {
  int member_a;
};

enum rex_c_named_enum { rex_c_named_enum_a = 1 };

typedef struct {
  int anonymous_member_a;
} rex_c_anonymous_record;

typedef enum { rex_c_anonymous_enum_a = 11 } rex_c_anonymous_enum;

int rex_c_tag_identity_a(struct rex_c_named_record *named_record,
                         rex_c_anonymous_record *anonymous_record) {
  return named_record->member_a + anonymous_record->anonymous_member_a +
         rex_c_named_enum_a + rex_c_anonymous_enum_a;
}
