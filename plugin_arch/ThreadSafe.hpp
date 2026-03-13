// Generic thread-safe wrapper using locked proxy objects.
//
// Zero-cost for single-threaded hosts — they use the originals unchanged.
// Multi-threaded hosts wrap the component they need:
//
//   ThreadSafe<ServiceLocator> locator;
//   locator.write()->add(service);
//   auto svc = locator.read()->get<T>("type");
//
// For EventBus (needs re-entrant locking), use recursive_mutex:
//   ThreadSafe<EventBus, std::recursive_mutex> bus;
//   bus.write()->subscribe("topic", handler);
//
// write() returns exclusive access (unique_lock).
// read() returns shared access (shared_lock) — only for shared_mutex.

#pragma once

#include <mutex>
#include <shared_mutex>
#include <type_traits>

namespace plugin_arch {

template <typename T, typename Mutex = std::shared_mutex>
class ThreadSafe {
 public:
  template <typename... Args>
  explicit ThreadSafe(Args&&... args) : inner_(std::forward<Args>(args)...) {}

  // Default constructor when T is default-constructible.
  ThreadSafe() requires std::is_default_constructible_v<T> = default;

  ~ThreadSafe() = default;

  ThreadSafe(const ThreadSafe&) = delete;
  ThreadSafe& operator=(const ThreadSafe&) = delete;
  ThreadSafe(ThreadSafe&&) = delete;
  ThreadSafe& operator=(ThreadSafe&&) = delete;

  // Exclusive access proxy — returned by write().
  class WriteGuard {
   public:
    WriteGuard(T& ref, Mutex& m) : lock_(m), ref_(ref) {}
    T* operator->() { return &ref_; }
    T& operator*() { return ref_; }

   private:
    std::unique_lock<Mutex> lock_;
    T& ref_;
  };

  // Shared (read-only) access proxy — returned by read().
  // Only available when Mutex supports shared locking (e.g. std::shared_mutex).
  class ReadGuard {
   public:
    ReadGuard(const T& ref, std::shared_mutex& m) : lock_(m), ref_(ref) {}
    const T* operator->() const { return &ref_; }
    const T& operator*() const { return ref_; }

   private:
    std::shared_lock<std::shared_mutex> lock_;
    const T& ref_;
  };

  // Get exclusive (write) access. Works with any mutex type.
  WriteGuard write() { return {inner_, mutex_}; }

  // Get shared (read) access. Only available with std::shared_mutex.
  ReadGuard read() const
      requires std::same_as<Mutex, std::shared_mutex>
  {
    return {inner_, mutex_};
  }

 private:
  T inner_;
  mutable Mutex mutex_;
};

}  // namespace plugin_arch
