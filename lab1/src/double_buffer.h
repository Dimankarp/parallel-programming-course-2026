#ifndef PARALLEL_DOUBLE_BUFFER_H
#define PARALLEL_DOUBLE_BUFFER_H

#include "metric.h"
#include <algorithm>
#include <array>
#include <atomic>
#include <cstdint>
#include <memory>
#include <mutex>
#include <thread>
#include <vector>

namespace parallel {

#ifndef PARALLEL_NEXT_COLLECTOR_ID_DEFINED
#define PARALLEL_NEXT_COLLECTOR_ID_DEFINED
inline uint64_t next_collector_id() {
  static std::atomic<uint64_t> counter{1};
  return counter.fetch_add(1, std::memory_order_relaxed);
}
#endif

enum class BufferState : int {
  Nowhere = -1,
  Buffer0 = 0,
  Buffer1 = 1,
};

struct Buf {
  std::array<uint64_t, BUCKETS_NUM> buckets{};
  uint64_t count = 0;
  uint64_t sum = 0;
  uint64_t min = UINT64_MAX;
  uint64_t max = 0;

  void clear() {
    buckets.fill(0);
    count = 0;
    sum = 0;
    min = UINT64_MAX;
    max = 0;
  }
};

struct alignas(64) ThreadBuffers {
  std::atomic<BufferState> inside{BufferState::Nowhere};
  Buf buf[2];
};

class DoubleBufferCollector : public MetricsCollector {
public:
  void record(uint64_t value) override {
    ThreadBuffers *my = get_my_buffers();
    BufferState b;
    while (true) {
      b = active_.load(std::memory_order_seq_cst);
      my->inside.store(b, std::memory_order_seq_cst);
      if (active_.load(std::memory_order_seq_cst) == b) {
        break;
      }
      my->inside.store(BufferState::Nowhere, std::memory_order_seq_cst);
    }

    size_t idx = static_cast<size_t>(b);
    auto bucket = getBucket(value);
    my->buf[idx].buckets[bucket]++;
    my->buf[idx].count++;
    my->buf[idx].sum += value;
    my->buf[idx].min = std::min(my->buf[idx].min, value);
    my->buf[idx].max = std::max(my->buf[idx].max, value);

    my->inside.store(BufferState::Nowhere, std::memory_order_release);
  }

  Snapshot snapshot() const override {
    std::lock_guard<std::mutex> lock{snap_lock_};

    BufferState old = active_.load(std::memory_order_seq_cst);
    BufferState next = (old == BufferState::Buffer0) ? BufferState::Buffer1 : BufferState::Buffer0;
    active_.store(next, std::memory_order_seq_cst);

    for (const auto &tb : all_threads_) {
      while (tb->inside.load(std::memory_order_seq_cst) == old) {
        std::this_thread::yield();
      }
    }

    size_t old_idx = static_cast<size_t>(old);
    for (const auto &tb : all_threads_) {
      if (tb->buf[old_idx].count > 0) {
        for (size_t i = 0; i < BUCKETS_NUM; i++) {
          global_.buckets[i] += tb->buf[old_idx].buckets[i];
        }
        global_.count += tb->buf[old_idx].count;
        global_.sum += tb->buf[old_idx].sum;
        global_.min = std::min(global_.min, tb->buf[old_idx].min);
        global_.max = std::max(global_.max, tb->buf[old_idx].max);
      }
      tb->buf[old_idx].clear();
    }

    Snapshot s{};
    s.buckets = global_.buckets;
    s.count = global_.count;
    s.sum = global_.sum;
    s.min = (global_.count > 0) ? global_.min : 0;
    s.max = global_.max;
    calcPercentiles(s);
    return s;
  }

private:
  ThreadBuffers *get_my_buffers() {
    struct TLSSlot {
      uint64_t id = 0;
      ThreadBuffers *buffers = nullptr;
    };
    static thread_local TLSSlot slot;
    if (slot.id != id_) {
      auto tb = std::make_unique<ThreadBuffers>();
      ThreadBuffers *raw = tb.get();
      {
        std::lock_guard<std::mutex> lock{snap_lock_};
        all_threads_.push_back(std::move(tb));
      }
      slot.id = id_;
      slot.buffers = raw;
    }
    return slot.buffers;
  }

  const uint64_t id_{next_collector_id()};
  mutable std::atomic<BufferState> active_{BufferState::Buffer0};
  mutable std::mutex snap_lock_;
  std::vector<std::unique_ptr<ThreadBuffers>> all_threads_;
  mutable Buf global_{};
};

} // namespace parallel

#endif // PARALLEL_DOUBLE_BUFFER_H
