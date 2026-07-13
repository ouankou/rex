struct rex_c_named_record {
  long member_b;
};

enum rex_c_named_enum { rex_c_named_enum_b = 2 };

typedef struct {
  long anonymous_member_b;
} rex_c_anonymous_record;

typedef enum { rex_c_anonymous_enum_b = 22 } rex_c_anonymous_enum;

int rex_c_tag_identity_b(struct rex_c_named_record *named_record,
                         rex_c_anonymous_record *anonymous_record) {
  return (int)(named_record->member_b + anonymous_record->anonymous_member_b) +
         rex_c_named_enum_b + rex_c_anonymous_enum_b;
}
