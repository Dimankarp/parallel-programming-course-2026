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
    calcPercentiles(s);
    return s;
  }

private:
  Snapshot _snap{};
};
} // namespace parallel

#endif // PARALLEL_BASIC_H