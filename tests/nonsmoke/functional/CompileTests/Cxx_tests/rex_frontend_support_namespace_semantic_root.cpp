namespace std {
struct rex_frontend_support_namespace_member {
  int value;
};
} // namespace std

int main() {
  std::rex_frontend_support_namespace_member member{7};
  return member.value == 7 ? 0 : 1;
}
