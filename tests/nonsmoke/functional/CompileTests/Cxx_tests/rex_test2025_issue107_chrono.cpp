#include <chrono>

namespace ns {
// This triggers instantiation of std::chrono types
using namespace std::chrono;
} // namespace ns

int main() { return 0; }
