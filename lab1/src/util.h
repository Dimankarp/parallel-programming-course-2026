#ifndef PARALLEL_UTIL_H
#define PARALLEL_UTIL_H

#include <atomic>
#include <cstdint>

namespace parallel {

inline uint64_t nextCollectorId() {
  static std::atomic<uint64_t> counter{1};
  return counter.fetch_add(1, std::memory_order_relaxed);
}

} // namespace parallel

#endif // PARALLEL_UTIL_H