#include <rex_frontend_generated_typedef_dependency.h>

struct rex_hidden_socket_address rex_hidden_address;

int rex_use_hidden_local(void) {
  rex_hidden_local_word_t value = 1;
  return value;
}

rex_hidden_signature_word_t
rex_hidden_identity(rex_hidden_signature_word_t value) {
  return value;
}

int main(void) {
  return rex_hidden_address.family != 0 || rex_use_hidden_local() != 1 ||
         rex_hidden_identity(2) != 2;
}
