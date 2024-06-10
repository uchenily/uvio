#include <functional>
#include <utility>

namespace uvio {

template <typename Func>
struct ScopeGuard final {
    ScopeGuard(Func func)
        : func_{std::move(func)} {}

    ~ScopeGuard() {
        if (active_) {
            std::invoke(func_);
        }
    }

    // No move
    ScopeGuard(ScopeGuard &&) = delete;
    auto operator=(ScopeGuard &&) = delete;
    // No copy
    ScopeGuard(const ScopeGuard &) = delete;
    auto operator=(const ScopeGuard &) = delete;

    void cancel() {
        active_ = false;
    }

private:
    Func func_;
    bool active_{true};
};

#define SCOPE_GUARD_CONCAT_2_IMPL(a, b) a##b
#define SCOPE_GUARD_CONCAT_2(a, b) SCOPE_GUARD_CONCAT_2_IMPL(a, b)

#define AddScopeExitGuard(x)                                                   \
    auto SCOPE_GUARD_CONCAT_2(_deffered_, __COUNTER__) = ScopeGuard(x)

// std::scope_exit ?

} // namespace uvio
