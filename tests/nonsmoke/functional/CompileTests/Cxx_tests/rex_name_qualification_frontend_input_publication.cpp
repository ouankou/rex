#include "rex_name_qualification_frontend_input_publication.hpp"

int rex_frontend_input_publication::published_function(int value) {
  return value + 1;
}

int main() {
  return rex_frontend_input_publication::published_function(1) == 2 ? 0 : 1;
}
