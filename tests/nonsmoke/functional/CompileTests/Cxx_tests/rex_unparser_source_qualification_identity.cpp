namespace rex_source_qualification_target {
struct Payload {
  int value;
};

enum Mode : int;

int global_value = 7;
void function();
} // namespace rex_source_qualification_target

namespace rex_source_qualification_alias = rex_source_qualification_target;

using AliasPayload = rex_source_qualification_alias::Payload;
using GlobalPayload = ::rex_source_qualification_target::Payload;

namespace rex_source_qualification_using {
using rex_source_qualification_alias::global_value;
using ::rex_source_qualification_target::global_value;
} // namespace rex_source_qualification_using

enum rex_source_qualification_alias::Mode : int { read };

void rex_source_qualification_alias::function() {}

int main() {
  AliasPayload value{rex_source_qualification_using::global_value};
  GlobalPayload other{value.value};
  rex_source_qualification_target::function();
  return other.value == 7 ? 0 : 1;
}
