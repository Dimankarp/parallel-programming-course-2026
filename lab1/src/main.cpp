#include "basic.h"
#include "gen.h"
#include "metric.h"
#include "naive.h"
#include "shard.h"
#include <algorithm>
#include <atomic>
#include <chrono>
#include <cstdint>
#include <iostream>
#include <latch>
#include <numeric>
#include <string>
#include <string_view>
#include <thread>
#include <vector>

double run(parallel::MetricsCollector &collector,
           const std::vector<uint32_t> &delays, size_t threads_num,
           uint64_t seconds) {

  std::latch ready_latch{static_cast<ssize_t>(threads_num)};
  std::latch start_latch{1};

  std::atomic_bool stop{false};
  std::vector<std::thread> threads{};

  std::vector<uint64_t> ops(threads_num, 0);

  for (size_t i = 0; i < threads_num; i++) {
    threads.push_back(std::thread{[&, i]() {
      uint64_t local_count = 0;
      uint64_t start = i * 1000;
      ready_latch.count_down();
      start_latch.wait();
      while (!stop.load(std::memory_order_relaxed)) {
        collector.record(delays[start++]);
        local_count++;
        if (start == delays.size())
          start = 0;
      }
      ops[i] = local_count;
    }});
  }

  ready_latch.wait();

  std::chrono::steady_clock clock{};
  auto t0 = clock.now();
  start_latch.count_down();
  std::this_thread::sleep_for(std::chrono::seconds{seconds});
  stop = true;
  auto t1 = clock.now();
  for (auto &t : threads) {
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
  std::cout << "Snapshot count: " << collector.snapshot().count << "\n";
  auto median = results.begin() + results.size() / 2;
  std::nth_element(results.begin(), median, results.end());
  return *median;
}

void printUsage(const char *prog) {
  std::cerr << "Usage: " << prog << " <test_number> [-n <threads>]\n";
}

int main(int argc, char *argv[]) {
  if (argc < 2) {
    printUsage(argv[0]);
    return 1;
  }

  int test_id = 0;
  try {
    test_id = std::stoi(argv[1]);
  } catch (const std::exception &e) {
    std::cerr << "Error: Test number must be an int\n";
    printUsage(argv[0]);
    return 1;
  }

  if (test_id < 1) {
    std::cerr << "Error: Unknown test number: " << test_id << "\n";
    printUsage(argv[0]);
    return 1;
  }

  size_t threads_num = (test_id == 1) ? 1 : 4;

  for (int i = 2; i < argc; ++i) {
    std::string_view arg = argv[i];
    if (arg == "-n") {
      if (i + 1 < argc) {
        try {
          long val = std::stol(argv[++i]);
          if (val <= 0) {
            std::cerr << "Error: Number of threads must be positive.\n";
            return 1;
          }
          threads_num = static_cast<size_t>(val);
        } catch (const std::exception &e) {
          std::cerr << "Error: Invalid thread number: " << argv[i] << "\n";
          return 1;
        }
      } else {
        std::cerr << "Error: -n requires a thread count argument.\n";
        return 1;
      }
    } else {
      std::cerr << "Error: Unrecognized option: " << arg << "\n";
      printUsage(argv[0]);
      return 1;
    }
  }

  constexpr uint64_t seed = 2026;
  constexpr uint64_t delays_sz = 1 << 20;
  std::mt19937_64 rng(seed);

  parallel::ZipfDistribution zipf{1023, 1.15};
  std::vector<uint32_t> delays(delays_sz, 0);
  for (size_t i = 0; i < delays.size(); i++) {
    delays[i] = zipf(rng);
  }

  std::cout << "Running Benchmark " << test_id
            << " with threds: " << threads_num << "\n";
  std::unique_ptr<parallel::MetricsCollector> collector_ptr;
  switch (test_id) {
  case 1:
    collector_ptr.reset(new parallel::BasicCollector());
    break;
  case 2:
    collector_ptr.reset(new parallel::NaiveCollector());
    break;
  case 3:
    collector_ptr.reset(new parallel::NaiveEmptyCollector());
    break;
  case 4:
    collector_ptr.reset(new parallel::ShardCollector());
    break;
  }
  double res = measurePoint(*collector_ptr, delays, threads_num);
  std::cout << "Result is: " << res << " ops/ms\n";
  return 0;
}
