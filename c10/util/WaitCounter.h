#pragma once

#include <chrono>
#include <memory>
#include <string>
#include <string_view>

#include <c10/macros/Macros.h>
#include <c10/util/ScopeExit.h>

namespace c10::monitor {
namespace detail {
class WaitCounterImpl;

class WaitCounterBackendIf {
 public:
  virtual ~WaitCounterBackendIf() = default;

  virtual intptr_t start(
      std::chrono::steady_clock::time_point now) noexcept = 0;
  virtual void stop(
      std::chrono::steady_clock::time_point now,
      intptr_t ctx) noexcept = 0;
};

class WaitCounterBackendFactoryIf {
 public:
  virtual ~WaitCounterBackendFactoryIf() = default;

  // May return nullptr.
  // In this case the counter will be ignored by the given backend.
  virtual std::unique_ptr<WaitCounterBackendIf> create(
      std::string_view key) noexcept = 0;
};

C10_API void registerWaitCounterBackend(
    std::unique_ptr<WaitCounterBackendFactoryIf>);
} // namespace detail

// A handle to a wait counter.
class C10_API WaitCounterHandle {
 public:
  explicit WaitCounterHandle(std::string_view key);

  class WaitGuard {
   public:
    ~WaitGuard() {
      stop();
    }

    void stop() {
      if (auto handle = std::exchange(handle_, nullptr)) {
        handle->stop(std::move(ctxs_));
      }
    }

   private:
    WaitGuard(WaitCounterHandle& handle, SmallVector<intptr_t>&& ctxs)
        : handle_{&handle}, ctxs_{std::move(ctxs)} {}

    friend class WaitCounterHandle;

    WaitCounterHandle* handle_;
    SmallVector<intptr_t> ctxs_;
  };

  // Starts a waiter
  void start(
      std::chrono::steady_clock::time_point now =
          std::chrono::steady_clock::now());
  // Stops the waiter. Each start() call should be matched by exactly one stop()
  // call.
  void stop(SmallVector<intptr_t>&& ctxs);

 private:
  detail::WaitCounterImpl& impl_;
};
} // namespace c10::monitor

#define STATIC_WAIT_COUNTER(_key)                           \
  []() -> ::c10::monitor::WaitCounterHandle& {              \
    static ::c10::monitor::WaitCounterHandle handle(#_key); \
    return handle;                                          \
  }()

#define STATIC_SCOPED_WAIT_COUNTER(_name)    \
  STATIC_WAIT_COUNTER(_name).start();        \
  auto C10_ANONYMOUS_VARIABLE(SCOPE_GUARD) = \
      c10::make_scope_exit([&]() { STATIC_WAIT_COUNTER(_name).stop(); });
