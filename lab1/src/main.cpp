#include "gen.h"
#include <cstdint>
int main() {

  constexpr uint64_t seed = 2026;
  constexpr uint64_t delays_sz = 1 << 20;
  std::mt19937_64 rng(seed);

  parallel::ZipfDistribution zipf{1023, 1.15};
  std::vector<uint32_t> delays(delays_sz, 0);
  for (int i = 0; i < delays.size(); i++) {
    delays[i] = zipf(rng);
  }

  printf("%d %d %d", delays[0], delays[1], delays[2]);
}