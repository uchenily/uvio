#include "uvio/common/scope_exit.hpp"
#include "uvio/debug.hpp"

using namespace uvio;

auto main() -> int {
    LOG_INFO("allocate resource");
    AddScopeExitGuard([]() {
        LOG_INFO("recovery resource");
    });

    LOG_INFO("program running ...");
}
