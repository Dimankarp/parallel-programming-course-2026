#ifndef PARALLEL_METRIC_H
#define PARALLEL_METRIC_H

#include <array>
#include <cstddef>
#include <cstdint>

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
  uint64_t min;
  uint64_t max;
  uint64_t p50;
  uint64_t p99;
};

class MetricsCollector {
public:
  virtual ~MetricsCollector() = default;
  virtual void record(uint64_t value) = 0;
  virtual Snapshot snapshot() const = 0;
};

} // namespace parallel

#endif // PARALLEL_METRIC_H