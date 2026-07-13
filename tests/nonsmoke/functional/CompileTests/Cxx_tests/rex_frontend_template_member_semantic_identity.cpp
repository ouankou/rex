struct RexTemplateMemberSemanticOwner {
  template <class T> RexTemplateMemberSemanticOwner &operator<<(T value);
};

int rex_template_member_semantic_identity() {
  RexTemplateMemberSemanticOwner owner;
  owner.operator<<(1);
  owner << 2;
  return 0;
}
