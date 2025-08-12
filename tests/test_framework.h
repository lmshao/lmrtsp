/**
 * @author SHAO Liming <lmshao@163.com>
 * @copyright Copyright (c) 2025 SHAO Liming
 * @license MIT
 *
 * SPDX-License-Identifier: MIT
 */

#ifndef TEST_FRAMEWORK_H
#define TEST_FRAMEWORK_H

#include <exception>
#include <functional>
#include <iostream>
#include <string>
#include <vector>

namespace test_framework {

class TestSuite {
public:
    struct TestCase {
        std::string name;
        std::function<void()> test_func;
        bool passed = false;
        std::string error_message;
    };

private:
    std::vector<TestCase> test_cases;
    std::string suite_name;
    int passed_count = 0;
    int failed_count = 0;

public:
    explicit TestSuite(const std::string &name) : suite_name(name) {}

    void AddTest(const std::string &test_name, std::function<void()> test_func)
    {
        test_cases.push_back({test_name, std::move(test_func), false, ""});
    }

    bool RunAll()
    {
        std::cout << "=== Running " << suite_name << " ===" << std::endl;

        for (auto &test_case : test_cases) {
            std::cout << "Running " << test_case.name << "... ";

            try {
                test_case.test_func();
                test_case.passed = true;
                passed_count++;
                std::cout << "PASS" << std::endl;
            } catch (const std::exception &e) {
                test_case.passed = false;
                test_case.error_message = e.what();
                failed_count++;
                std::cout << "FAIL (" << e.what() << ")" << std::endl;
            } catch (...) {
                test_case.passed = false;
                test_case.error_message = "Unknown exception";
                failed_count++;
                std::cout << "FAIL (Unknown exception)" << std::endl;
            }
        }

        std::cout << std::endl;
        std::cout << "=== " << suite_name << " Results ===" << std::endl;
        std::cout << "Total: " << test_cases.size() << ", ";
        std::cout << "Passed: " << passed_count << ", ";
        std::cout << "Failed: " << failed_count << std::endl;

        return failed_count == 0;
    }

    int GetFailedCount() const { return failed_count; }
    int GetPassedCount() const { return passed_count; }
};

// Assertion macros
#define ASSERT_TRUE(condition)                                                                                         \
    do {                                                                                                               \
        if (!(condition)) {                                                                                            \
            throw std::runtime_error("Assertion failed: " #condition);                                                 \
        }                                                                                                              \
    } while (0)

#define ASSERT_FALSE(condition)                                                                                        \
    do {                                                                                                               \
        if (condition) {                                                                                               \
            throw std::runtime_error("Assertion failed: " #condition " should be false");                              \
        }                                                                                                              \
    } while (0)

#define ASSERT_EQ(expected, actual)                                                                                    \
    do {                                                                                                               \
        if ((expected) != (actual)) {                                                                                  \
            throw std::runtime_error("Assertion failed: expected " + std::to_string(expected) + ", got " +             \
                                     std::to_string(actual));                                                          \
        }                                                                                                              \
    } while (0)

#define ASSERT_STR_EQ(expected, actual)                                                                                \
    do {                                                                                                               \
        if ((expected) != (actual)) {                                                                                  \
            throw std::runtime_error("Assertion failed: expected '" + std::string(expected) + "', got '" +             \
                                     std::string(actual) + "'");                                                       \
        }                                                                                                              \
    } while (0)

#define ASSERT_STR_CONTAINS(haystack, needle)                                                                          \
    do {                                                                                                               \
        std::string h(haystack);                                                                                       \
        std::string n(needle);                                                                                         \
        if (h.find(n) == std::string::npos) {                                                                          \
            throw std::runtime_error("Assertion failed: '" + h + "' does not contain '" + n + "'");                    \
        }                                                                                                              \
    } while (0)

#define ASSERT_STR_NOT_CONTAINS(haystack, needle)                                                                      \
    do {                                                                                                               \
        std::string h(haystack);                                                                                       \
        std::string n(needle);                                                                                         \
        if (h.find(n) != std::string::npos) {                                                                          \
            throw std::runtime_error("Assertion failed: '" + h + "' should not contain '" + n + "'");                  \
        }                                                                                                              \
    } while (0)

} // namespace test_framework

#endif // TEST_FRAMEWORK_H
