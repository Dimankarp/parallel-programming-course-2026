#ifndef PARALLEL_METRIC_H
#define PARALLEL_METRIC_H

#include <array>
#include <cstddef>
#include <cstdint>
#include <limits>

#define BUCKETS_NUM 256
#define BUCKETS_STEP_MS 4

namespace parallel {

inline size_t getBucket(uint64_t value) {
  constexpr auto MAX_VALUE =
      static_cast<uint64_t>(BUCKETS_NUM * BUCKETS_STEP_MS - 1);
  auto normal = std::min(value, MAX_VALUE);
  return normal / BUCKETS_STEP_MS;
}

struct Snapshot {
  std::array<uint64_t, BUCKETS_NUM> buckets;
  uint64_t count;
  uint64_t sum;
  uint64_t min = std::numeric_limits<uint64_t>::max();
  uint64_t max;
  uint64_t p50;
  uint64_t p99;
};

inline void calcPercentiles(Snapshot &s) {
  size_t threshold_99 = s.count * 0.99;
  size_t threshold_50 = s.count * 0.5;
  s.p50 = 0;
  s.p99 = 0;

  size_t acc = 0;
  size_t i = 0;
  for (; i < BUCKETS_NUM; i++) {
    acc += s.buckets[i];
    if (acc > threshold_50 && s.p50 == 0) {
      s.p50 = i * 4;
    }
    if (acc > threshold_99 && s.p99 == 0) {
      s.p99 = i * 4;
    }
  }
}

class MetricsCollector {
public:
  virtual ~MetricsCollector() = default;
  virtual void record(uint64_t value) = 0;
  virtual Snapshot snapshot() const = 0;
};

} // namespace parallel

#endif // PARALLEL_METRIC_H