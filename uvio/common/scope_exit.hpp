#include <functional>
#include <utility>

namespace uvio {

template <typename Func>
struct ScopeExit final {
    ScopeExit(Func func)
        : func_{std::move(func)} {}

    ~ScopeExit() {
        if (active_) {
            std::invoke(func_);
        }
    }

    // No move
    ScopeExit(ScopeExit &&) = delete;
    auto operator=(ScopeExit &&) = delete;
    // No copy
    ScopeExit(const ScopeExit &) = delete;
    auto operator=(const ScopeExit &) = delete;

    void cancel() {
        active_ = false;
    }

private:
    Func func_;
    bool active_{true};
};

#define SCOPE_EXIT_CONCAT_2_IMPL(a, b) a##b
#define SCOPE_EXIT_CONCAT_2(a, b) SCOPE_EXIT_CONCAT_2_IMPL(a, b)

#define SCOPE_EXIT(x)                                                          \
    auto SCOPE_EXIT_CONCAT_2(_deffered_, __COUNTER__) = ScopeExit(x)

// std::scope_exit ?

} // namespace uvio
