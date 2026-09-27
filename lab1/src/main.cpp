#include "basic.h"
#include "gen.h"
#include "metric.h"
#include <algorithm>
#include <atomic>
#include <chrono>
#include <cstdint>
#include <latch>
#include <memory>
#include <numeric>
#include <thread>
#include <vector>

double run(parallel::MetricsCollector &collector,
           const std::vector<uint32_t> &delays, size_t threads_num,
           uint64_t seconds) {

  std::latch latch{static_cast<ssize_t>(threads_num)};
  std::atomic_bool stop{false};
  std::vector<std::thread> threads{};
  std::vector<uint64_t> ops(threads_num, 0);

  for (int i = 0; i < threads_num; i++) {
    threads.push_back(std::thread{[&, i]() {
      uint64_t local_count = 0;
      uint64_t start = i * 1000;
      latch.wait();
      while (!stop) {
        collector.record(delays[start++]);
        local_count++;
        if (start == delays.size())
          start = 0;
      }
      ops[i] = local_count;
    }});
  }

  std::chrono::steady_clock clock{};
  auto t0 = clock.now();
  latch.count_down();
  std::this_thread::sleep_for(std::chrono::seconds{seconds});
  stop = true;
  auto t1 = clock.now();
  for (auto& t : threads) {
    t.join();
  }

  return std::accumulate(ops.begin(), ops.end(), double{0}) /
         std::chrono::duration_cast<std::chrono::milliseconds>((t1 - t0))
             .count();
}

double measurePoint(parallel::MetricsCollector &collector,
                    const std::vector<uint32_t> &delays, size_t threads_num) {
  run(collector, delays, threads_num, 5);
  std::vector<double> results;
  for (int i = 0; i < 5; i++) {
    results.push_back(run(collector, delays, threads_num, 5));
  }
  std::cout << collector.snapshot().count;
  auto median = results.begin() + results.size() / 2;
  std::nth_element(results.begin(), median, results.end());
  return *median;
}

int main() {

  constexpr uint64_t seed = 2026;
  constexpr uint64_t delays_sz = 1 << 20;
  std::mt19937_64 rng(seed);

  parallel::ZipfDistribution zipf{1023, 1.15};
  std::vector<uint32_t> delays(delays_sz, 0);
  for (int i = 0; i < delays.size(); i++) {
    delays[i] = zipf(rng);
  }

  parallel::BasicCollector basic{};
  auto res = measurePoint(basic, delays, 1);
  std::cout << "Result is: " << res << " ops/ms";
}
