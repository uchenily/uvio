#include "backward.hpp"

#include <iostream>
#include <vector>

namespace backward {
backward::SignalHandling sh;
} // namespace backward

auto main() -> int {
    std::vector<int> vec;
    std::cout << "vec[100] = " << vec[100] << '\n';
}
