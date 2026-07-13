struct RexStaticMemberOwner {
  struct MemberType {
    int value;
  };

  static MemberType member;
};

RexStaticMemberOwner::MemberType RexStaticMemberOwner::member = {17};

int rex_frontend_static_member_publication() {
  return RexStaticMemberOwner::member.value;
}
