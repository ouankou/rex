#include <rex_frontend_requires_local_parameter_owner.hpp>

int rex_frontend_requires_local_parameter_owner() {
  int result = 0;
  rex_write(&result, 17);
  return result;
}
