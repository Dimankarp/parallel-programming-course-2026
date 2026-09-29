#ifndef PARALLEL_DOUBLE_BUFFER_H
#define PARALLEL_DOUBLE_BUFFER_H

#include "metric.h"
#include <algorithm>
#include <array>
#include <atomic>
#include <cstdint>
#include <limits>
#include <memory>
#include <mutex>
#include <thread>
#include <vector>
#include "util.h"

namespace parallel {

enum class BufferState : int {
  NO = -1,
  ZERO = 0,
  ONE = 1,
};

struct Buf {
  std::array<uint64_t, BUCKETS_NUM> buckets{};
  uint64_t count = 0;
  uint64_t sum = 0;
  uint64_t min = std::numeric_limits<uint64_t>::max();
  uint64_t max = 0;

  void clear() {
    buckets.fill(0);
    count = 0;
    sum = 0;
    min = std::numeric_limits<uint64_t>::max();
    max = 0;
  }
};

struct alignas(64) ThreadBuffers {
  std::atomic<BufferState> inside{BufferState::NO};
  Buf buf[2];
};

class DoubleBufferCollector : public MetricsCollector {
public:
  void record(uint64_t value) override {
    ThreadBuffers *my = getBuffers();
    BufferState b;
    while (true) {
      b = _active.load(std::memory_order_seq_cst);
      my->inside.store(b, std::memory_order_seq_cst);
      if (_active.load(std::memory_order_seq_cst) == b) {
        break;
      }
      my->inside.store(BufferState::NO, std::memory_order_seq_cst);
    }

    size_t idx = static_cast<size_t>(b);
    auto bucket = getBucket(value);
    my->buf[idx].buckets[bucket]++;
    my->buf[idx].count++;
    my->buf[idx].sum += value;
    my->buf[idx].min = std::min(my->buf[idx].min, value);
    my->buf[idx].max = std::max(my->buf[idx].max, value);

    my->inside.store(BufferState::NO, std::memory_order_release);
  }

  Snapshot snapshot() const override {
    std::lock_guard<std::mutex> lock{_lock};

    BufferState old = _active.load(std::memory_order_seq_cst);
    BufferState next = (old == BufferState::ZERO) ? BufferState::ONE
                                                     : BufferState::ZERO;
    _active.store(next, std::memory_order_seq_cst);

    for (const auto &tb : _threads) {
      while (tb->inside.load(std::memory_order_seq_cst) == old) {
        std::this_thread::yield();
      }
    }

    size_t old_idx = static_cast<size_t>(old);
    for (const auto &tb : _threads) {
      if (tb->buf[old_idx].count > 0) {
        for (size_t i = 0; i < BUCKETS_NUM; i++) {
          _global.buckets[i] += tb->buf[old_idx].buckets[i];
        }
        _global.count += tb->buf[old_idx].count;
        _global.sum += tb->buf[old_idx].sum;
        _global.min = std::min(_global.min, tb->buf[old_idx].min);
        _global.max = std::max(_global.max, tb->buf[old_idx].max);
      }
      tb->buf[old_idx].clear();
    }

    Snapshot s{};
    s.buckets = _global.buckets;
    s.count = _global.count;
    s.sum = _global.sum;
    s.min = (_global.count > 0) ? _global.min : 0;
    s.max = _global.max;
    calcPercentiles(s);
    return s;
  }

private:
  ThreadBuffers *getBuffers() {
    struct TLSSlot {
      uint64_t id = 0;
      ThreadBuffers *buffers = nullptr;
    };
    static thread_local TLSSlot slot;
    if (slot.id != _id) {
      auto tb = std::make_unique<ThreadBuffers>();
      ThreadBuffers *raw = tb.get();
      {
        std::lock_guard<std::mutex> lock{_lock};
        _threads.push_back(std::move(tb));
      }
      slot.id = _id;
      slot.buffers = raw;
    }
    return slot.buffers;
  }

  const uint64_t _id{nextCollectorId()};
  mutable std::atomic<BufferState> _active{BufferState::ZERO};
  mutable std::mutex _lock;
  std::vector<std::unique_ptr<ThreadBuffers>> _threads;
  mutable Buf _global{};
};

} // namespace parallel

#endif // PARALLEL_DOUBLE_BUFFER_H
