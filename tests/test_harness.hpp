#pragma once
#include <functional>
#include <iostream>
#include <stdexcept>
#include <string>
#include <utility>
#include <vector>
namespace aethon::tests {
struct TestCase { std::string name; std::function<void()> fn; };
inline std::vector<TestCase>& registry() { static std::vector<TestCase> tests; return tests; }
struct Registrar { Registrar(std::string name, std::function<void()> fn) { registry().push_back({std::move(name), std::move(fn)}); } };
inline void require(bool value, const char* expr, const char* file, int line) { if (!value) throw std::runtime_error(std::string(file) + ":" + std::to_string(line) + " failed: " + expr); }
} // namespace aethon::tests
#define AETHON_TEST(name) static void name(); static ::aethon::tests::Registrar name##_registrar(#name, name); static void name()
#define AETHON_REQUIRE(expr) ::aethon::tests::require((expr), #expr, __FILE__, __LINE__)
