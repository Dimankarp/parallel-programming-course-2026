#ifndef PARALLEL_THREAD_LOCAL_H
#define PARALLEL_THREAD_LOCAL_H

#include "metric.h"
#include "util.h"
#include <algorithm>
#include <array>
#include <atomic>
#include <cstdint>
#include <limits>
#include <memory>
#include <mutex>
#include <vector>

namespace parallel {

struct alignas(64) ThreadState {
  std::array<std::atomic<uint64_t>, BUCKETS_NUM> buckets{};
  std::atomic<uint64_t> count{0};
  std::atomic<uint64_t> sum{0};
  std::atomic<uint64_t> min{UINT64_MAX};
  std::atomic<uint64_t> max{0};
};

inline void relaxed_add(std::atomic<uint64_t> &c, uint64_t delta) {
  c.store(c.load(std::memory_order_relaxed) + delta, std::memory_order_relaxed);
}

class ThreadLocalCollector : public MetricsCollector {
public:
  void record(uint64_t value) override {
    ThreadState *s = get_my_state();
    auto b = getBucket(value);

    relaxed_add(s->buckets[b], 1);
    relaxed_add(s->count, 1);
    relaxed_add(s->sum, value);

    if (value < s->min.load(std::memory_order_relaxed)) {
      s->min.store(value, std::memory_order_relaxed);
    }
    if (value > s->max.load(std::memory_order_relaxed)) {
      s->max.store(value, std::memory_order_relaxed);
    }
  }

  Snapshot snapshot() const override {
    std::vector<ThreadState *> states;
    {
      std::lock_guard<std::mutex> lock{list_lock_};
      states.reserve(all_states_.size());
      for (const auto &st : all_states_) {
        states.push_back(st.get());
      }
    }

    Snapshot s{};
    s.min = std::numeric_limits<uint64_t>::max();
    s.max = 0;

    for (ThreadState *st : states) {
      for (size_t i = 0; i < BUCKETS_NUM; i++) {
        s.buckets[i] += st->buckets[i].load(std::memory_order_relaxed);
      }
      s.count += st->count.load(std::memory_order_relaxed);
      s.sum += st->sum.load(std::memory_order_relaxed);
      if (st->count.load(std::memory_order_relaxed) > 0) {
        s.min = std::min(s.min, st->min.load(std::memory_order_relaxed));
        s.max = std::max(s.max, st->max.load(std::memory_order_relaxed));
      }
    }

    if (s.count == 0) {
      s.min = 0;
    }

    calcPercentiles(s);
    return s;
  }

private:
  ThreadState *get_my_state() {
    struct TLSSlot {
      uint64_t id = 0;
      ThreadState *state = nullptr;
    };
    static thread_local TLSSlot slot;
    if (slot.id != id_) {
      auto s = std::make_unique<ThreadState>();
      ThreadState *raw = s.get();
      {
        std::lock_guard<std::mutex> lock{list_lock_};
        all_states_.push_back(std::move(s));
      }
      slot.id = id_;
      slot.state = raw;
    }
    return slot.state;
  }

  const uint64_t id_{nextCollectorId()};
  mutable std::mutex list_lock_;
  std::vector<std::unique_ptr<ThreadState>> all_states_;
};

} // namespace parallel

#endif // PARALLEL_THREAD_LOCAL_H
