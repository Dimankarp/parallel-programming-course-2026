
#ifndef PARALLEL_NAIVE_H
#define PARALLEL_NAIVE_H

#include "basic.h"
#include "metric.h"
#include <mutex>
namespace parallel {
class NaiveCollector : public MetricsCollector {

public:
  void record(uint64_t value) override {
    std::scoped_lock lock{_mtx};
    _basic.record(value);
  }
  Snapshot snapshot() const override {
    std::scoped_lock lock{_mtx};
    return _basic.snapshot();
  }

private:
  mutable std::mutex _mtx;
  BasicCollector _basic{};
};

class NaiveEmptyCollector : public MetricsCollector {

public:
  void record(uint64_t value) override { std::scoped_lock lock{_mtx}; }
  Snapshot snapshot() const override {
    std::scoped_lock lock{_mtx};
    return _basic.snapshot();
  }

private:
  mutable std::mutex _mtx;
  BasicCollector _basic{};
};

} // namespace parallel

#endif // PARALLEL_NAIVE_H
