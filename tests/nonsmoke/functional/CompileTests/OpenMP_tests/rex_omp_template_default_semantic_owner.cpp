#include <iostream>

void rex_omp_template_default_semantic_owner() {
#pragma omp parallel
  {
    std::cout << 1;
  }
}
