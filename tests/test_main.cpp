#include "test_harness.hpp"
int main() {
    int failed = 0;
    for (const auto& test : aethon::tests::registry()) {
        try {
            test.fn();
            std::cout << "[pass] " << test.name << '\n';
        } catch (const std::exception& e) {
            ++failed;
            std::cerr << "[fail] " << test.name << ": " << e.what() << '\n';
        }
    }
    return failed == 0 ? 0 : 1;
}
