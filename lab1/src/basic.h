#ifndef PARALLEL_BASIC_H
#define PARALLEL_BASIC_H

#include "metric.h"
namespace parallel {
class BasicCollector : public MetricsCollector {

public:
  void record(uint64_t value) override {

    _snap.count++;
    _snap.sum += value;
    _snap.min = std::min(_snap.min, value);
    _snap.max = std::max(_snap.max, value);
    _snap.buckets[getBucket(value)]++;
  }

  Snapshot snapshot() const override {
    Snapshot s = _snap;
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
    return s;
  }

private:
  Snapshot _snap{};
};
} // namespace parallel

#endif // PARALLEL_BASIC_H