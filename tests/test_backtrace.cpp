#include "uvio/common/backtrace.hpp"

#include <iostream>
#include <vector>

auto main() -> int {
    std::vector<int> vec;
    std::cout << "vec[100] = " << vec[100] << '\n';
}
