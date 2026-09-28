#ifndef PARALLEL_SHARD_H
#define PARALLEL_SHARD_H

#include "metric.h"
#include <atomic>
#include <cstdint>
#include <limits>
#include <mutex>

#define SHARD_LOCKS 16

namespace parallel {
class ShardCollector : public MetricsCollector {

public:
  void record(uint64_t value) override {
    uint64_t temp = _count;
    while (!_count.compare_exchange_weak(temp, temp + 1)) {
    }
    temp = _sum;
    while (!_sum.compare_exchange_weak(temp, temp + value)) {
    }
    temp = _min;
    while (!_min.compare_exchange_weak(temp, std::min(temp, value))) {
    }
    temp = _max;
    while (!_max.compare_exchange_weak(temp, std::max(temp, value))) {
    }

    auto b = getBucket(value);
    auto lock_index = b % 16;
    std::scoped_lock lock{_mutexes[lock_index]};
    _buckets[b]++;
  }

  Snapshot snapshot() const override {
    Snapshot s;
    for (int i = 0; i < SHARD_LOCKS; i++) {
      std::scoped_lock lock{_mutexes[i]};
      for (int j = i; j < _buckets.size(); j += SHARD_LOCKS) {
        s.buckets[j] = _buckets[j];
      }
    }
    s.count = _count;
    s.max = _max;
    s.min = _min;
    s.sum = _sum;
    calcPercentiles(s);
    return s;
  }

private:
  std::atomic<uint64_t> _count;
  std::atomic<uint64_t> _sum;
  std::atomic<uint64_t> _min = std::numeric_limits<uint64_t>::max();
  std::atomic<uint64_t> _max;
  std::array<uint64_t, BUCKETS_NUM> _buckets;
  mutable std::array<std::mutex, SHARD_LOCKS> _mutexes;
};
} // namespace parallel

#endif // PARALLEL_SHARD_H