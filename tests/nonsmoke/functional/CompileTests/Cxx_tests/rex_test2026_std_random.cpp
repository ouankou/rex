#include <array>
#include <random>

namespace vendor {
namespace sampling {

int drawBinomial(unsigned seed) {
  std::mt19937 generator(seed);
  std::binomial_distribution<int> distribution(12, 0.35);
  return distribution(generator);
}

double drawNormal(unsigned seed) {
  std::minstd_rand generator(seed);
  std::normal_distribution<double> distribution(0.0, 1.0);
  return distribution(generator);
}

std::array<int, 3> fillHistogram(unsigned seed) {
  std::array<int, 3> histogram = {{0, 0, 0}};

  std::mt19937 generator(seed);
  std::uniform_int_distribution<int> bucket(0, 2);
  for (int i = 0; i < 24; ++i) {
    ++histogram[bucket(generator)];
  }

  return histogram;
}

} // namespace sampling
} // namespace vendor

int sampleSummary() {
  std::array<int, 3> histogram = vendor::sampling::fillHistogram(99);
  return vendor::sampling::drawBinomial(7) + histogram[0] +
         static_cast<int>(vendor::sampling::drawNormal(42));
}
