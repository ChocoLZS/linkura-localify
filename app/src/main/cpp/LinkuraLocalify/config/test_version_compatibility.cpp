/**
 * @file test_version_compatibility.cpp
 * @brief Comprehensive test suite for the version compatibility parser
 * 
 * This file contains unit tests for all components of the version compatibility
 * system including Version parsing, lexical analysis, expression parsing, and
 * compatibility checking.
 * 
 * Test categories:
 * - Version class functionality
 * - Lexer tokenization
 * - Parser AST generation
 * - VersionChecker compatibility evaluation
 * - Edge cases and error handling
 * 
 * @author LinkuraLocalify Team
 */

#include "version_compatibility.h"
#include <cassert>
#include <iostream>
#include <vector>
#include <stdexcept>

using namespace VersionCompatibility;

// =============================================================================
// Test Framework Utilities
// =============================================================================

class TestFramework {
private:
    static int total_tests_;
    static int passed_tests_;
    static int failed_tests_;

public:
    static void assert_true(bool condition, const std::string& test_name) {
        total_tests_++;
        if (condition) {
            passed_tests_++;
            std::cout << "[PASS] " << test_name << std::endl;
        } else {
            failed_tests_++;
            std::cout << "[FAIL] " << test_name << std::endl;
        }
    }

    static void assert_equal(const std::string& expected, const std::string& actual, const std::string& test_name) {
        assert_true(expected == actual, test_name + " (expected: '" + expected + "', got: '" + actual + "')");
    }

    static void assert_throws(std::function<void()> func, const std::string& test_name) {
        total_tests_++;
        try {
            func();
            failed_tests_++;
            std::cout << "[FAIL] " << test_name << " (expected exception but none was thrown)" << std::endl;
        } catch (...) {
            passed_tests_++;
            std::cout << "[PASS] " << test_name << std::endl;
        }
    }

    static void print_summary() {
        std::cout << "\n=== Test Summary ===" << std::endl;
        std::cout << "Total tests: " << total_tests_ << std::endl;
        std::cout << "Passed: " << passed_tests_ << std::endl;
        std::cout << "Failed: " << failed_tests_ << std::endl;
        if (failed_tests_ == 0) {
            std::cout << "All tests passed!" << std::endl;
        } else {
            std::cout << "Some tests failed!" << std::endl;
        }
    }

    static bool all_passed() {
        return failed_tests_ == 0;
    }
};

int TestFramework::total_tests_ = 0;
int TestFramework::passed_tests_ = 0;
int TestFramework::failed_tests_ = 0;

// =============================================================================
// Version Class Tests
// =============================================================================

void test_version_parsing() {
    std::cout << "\n--- Version Parsing Tests ---" << std::endl;

    // Test basic version parsing
    Version v1("1.2.3");
    TestFramework::assert_true(v1.major == 1 && v1.minor == 2 && v1.patch == 3, "Parse complete version 1.2.3");

    Version v2("2.0");
    TestFramework::assert_true(v2.major == 2 && v2.minor == 0 && v2.patch == 0, "Parse partial version 2.0");

    Version v3("5");
    TestFramework::assert_true(v3.major == 5 && v3.minor == 0 && v3.patch == 0, "Parse major-only version 5");

    // Test version with zero components
    Version v4("0.0.0");
    TestFramework::assert_true(v4.major == 0 && v4.minor == 0 && v4.patch == 0, "Parse zero version 0.0.0");

    // Test large version numbers
    Version v5("99.999.9999");
    TestFramework::assert_true(v5.major == 99 && v5.minor == 999 && v5.patch == 9999, "Parse large version numbers");
}

void test_version_comparison() {
    std::cout << "\n--- Version Comparison Tests ---" << std::endl;

    Version v1_0_0("1.0.0");
    Version v1_0_1("1.0.1");
    Version v1_1_0("1.1.0");
    Version v2_0_0("2.0.0");

    // Test equality
    TestFramework::assert_true(v1_0_0 == Version("1.0.0"), "Version equality");
    TestFramework::assert_true(v1_0_0 != v1_0_1, "Version inequality");

    // Test less than
    TestFramework::assert_true(v1_0_0 < v1_0_1, "Version less than (patch)");
    TestFramework::assert_true(v1_0_0 < v1_1_0, "Version less than (minor)");
    TestFramework::assert_true(v1_0_0 < v2_0_0, "Version less than (major)");

    // Test greater than
    TestFramework::assert_true(v2_0_0 > v1_1_0, "Version greater than (major)");
    TestFramework::assert_true(v1_1_0 > v1_0_1, "Version greater than (minor)");
    TestFramework::assert_true(v1_0_1 > v1_0_0, "Version greater than (patch)");

    // Test less than or equal
    TestFramework::assert_true(v1_0_0 <= v1_0_0, "Version less than or equal (equal)");
    TestFramework::assert_true(v1_0_0 <= v1_0_1, "Version less than or equal (less)");

    // Test greater than or equal
    TestFramework::assert_true(v1_0_1 >= v1_0_0, "Version greater than or equal (greater)");
    TestFramework::assert_true(v1_0_0 >= v1_0_0, "Version greater than or equal (equal)");
}

void test_version_string_conversion() {
    std::cout << "\n--- Version String Conversion Tests ---" << std::endl;

    Version v1("1.2.3");
    TestFramework::assert_equal("1.2.3", v1.toString(), "Version toString");

    Version v2("10.0.0");
    TestFramework::assert_equal("10.0.0", v2.toString(), "Version toString with zeros");
}

// =============================================================================
// Lexer Tests
// =============================================================================

void test_lexer_tokenization() {
    std::cout << "\n--- Lexer Tokenization Tests ---" << std::endl;

    // Test simple version tokenization
    Lexer lexer1(">= 1.2.3");
    auto tokens1 = lexer1.tokenize();
    TestFramework::assert_true(tokens1.size() == 3 && 
                              tokens1[0].type == TokenType::OPERATOR && tokens1[0].value == ">=" &&
                              tokens1[1].type == TokenType::VERSION && tokens1[1].value == "1.2.3" &&
                              tokens1[2].type == TokenType::END_OF_INPUT, "Tokenize simple comparison");

    // Test complex expression
    Lexer lexer2(">= 1.0.0 && < 2.0.0");
    auto tokens2 = lexer2.tokenize();
    TestFramework::assert_true(tokens2.size() == 6 &&
                              tokens2[0].type == TokenType::OPERATOR && tokens2[0].value == ">=" &&
                              tokens2[1].type == TokenType::VERSION && tokens2[1].value == "1.0.0" &&
                              tokens2[2].type == TokenType::AND && tokens2[2].value == "&&" &&
                              tokens2[3].type == TokenType::OPERATOR && tokens2[3].value == "<" &&
                              tokens2[4].type == TokenType::VERSION && tokens2[4].value == "2.0.0" &&
                              tokens2[5].type == TokenType::END_OF_INPUT, "Tokenize complex AND expression");

    // Test parentheses
    Lexer lexer3("(>= 1.0.0 || == 2.0.0)");
    auto tokens3 = lexer3.tokenize();
    TestFramework::assert_true(tokens3.size() == 8 &&
                              tokens3[0].type == TokenType::LEFT_PAREN &&
                              tokens3[1].type == TokenType::OPERATOR && tokens3[1].value == ">=" &&
                              tokens3[2].type == TokenType::VERSION && tokens3[2].value == "1.0.0" &&
                              tokens3[3].type == TokenType::OR && tokens3[3].value == "||" &&
                              tokens3[4].type == TokenType::OPERATOR && tokens3[4].value == "==" &&
                              tokens3[5].type == TokenType::VERSION && tokens3[5].value == "2.0.0" &&
                              tokens3[6].type == TokenType::RIGHT_PAREN &&
                              tokens3[7].type == TokenType::END_OF_INPUT, "Tokenize parenthesized OR expression");

    // Test all comparison operators
    Lexer lexer4("== 1.0.0 != 1.1.0 < 1.2.0 <= 1.3.0 > 1.4.0 >= 1.5.0");
    auto tokens4 = lexer4.tokenize();
    TestFramework::assert_true(tokens4.size() == 13, "Tokenize all comparison operators");
}

void test_lexer_whitespace_handling() {
    std::cout << "\n--- Lexer Whitespace Handling Tests ---" << std::endl;

    Lexer lexer1("  >=   1.2.3  ");
    auto tokens1 = lexer1.tokenize();
    TestFramework::assert_true(tokens1.size() == 3 &&
                              tokens1[0].type == TokenType::OPERATOR && tokens1[0].value == ">=" &&
                              tokens1[1].type == TokenType::VERSION && tokens1[1].value == "1.2.3",
                              "Handle extra whitespace");

    Lexer lexer2(">=1.2.3");
    auto tokens2 = lexer2.tokenize();
    TestFramework::assert_true(tokens2.size() == 3 &&
                              tokens2[0].type == TokenType::OPERATOR && tokens2[0].value == ">=" &&
                              tokens2[1].type == TokenType::VERSION && tokens2[1].value == "1.2.3",
                              "Handle no whitespace");
}

void test_lexer_error_handling() {
    std::cout << "\n--- Lexer Error Handling Tests ---" << std::endl;

    TestFramework::assert_throws([]() {
        Lexer lexer("@invalid");
        lexer.tokenize();
    }, "Lexer throws on invalid character");
}

// =============================================================================
// Parser Tests
// =============================================================================

void test_parser_simple_expressions() {
    std::cout << "\n--- Parser Simple Expression Tests ---" << std::endl;

    // Test simple comparison
    Lexer lexer1(">= 1.2.3");
    auto tokens1 = lexer1.tokenize();
    Parser parser1(std::move(tokens1));
    auto ast1 = parser1.parse();
    TestFramework::assert_true(ast1 != nullptr, "Parse simple comparison expression");

    // Test equality comparison
    Lexer lexer2("== 1.0.0");
    auto tokens2 = lexer2.tokenize();
    Parser parser2(std::move(tokens2));
    auto ast2 = parser2.parse();
    TestFramework::assert_true(ast2 != nullptr, "Parse equality expression");
}

void test_parser_complex_expressions() {
    std::cout << "\n--- Parser Complex Expression Tests ---" << std::endl;

    // Test AND expression
    Lexer lexer1(">= 1.0.0 && < 2.0.0");
    auto tokens1 = lexer1.tokenize();
    Parser parser1(std::move(tokens1));
    auto ast1 = parser1.parse();
    TestFramework::assert_true(ast1 != nullptr, "Parse AND expression");

    // Test OR expression
    Lexer lexer2("== 1.5.2 || == 1.5.3");
    auto tokens2 = lexer2.tokenize();
    Parser parser2(std::move(tokens2));
    auto ast2 = parser2.parse();
    TestFramework::assert_true(ast2 != nullptr, "Parse OR expression");

    // Test parenthesized expression
    Lexer lexer3("(>= 1.0.0 && < 1.5.0) || >= 2.0.0");
    auto tokens3 = lexer3.tokenize();
    Parser parser3(std::move(tokens3));
    auto ast3 = parser3.parse();
    TestFramework::assert_true(ast3 != nullptr, "Parse parenthesized expression");
}

void test_parser_error_handling() {
    std::cout << "\n--- Parser Error Handling Tests ---" << std::endl;

    TestFramework::assert_throws([]() {
        Lexer lexer(">= 1.0.0 &&");  // Missing right operand
        auto tokens = lexer.tokenize();
        Parser parser(std::move(tokens));
        parser.parse();
    }, "Parser throws on incomplete AND expression");

    TestFramework::assert_throws([]() {
        Lexer lexer("(>= 1.0.0");  // Missing closing parenthesis
        auto tokens = lexer.tokenize();
        Parser parser(std::move(tokens));
        parser.parse();
    }, "Parser throws on missing closing parenthesis");

    TestFramework::assert_throws([]() {
        Lexer lexer(">= 1.0.0 unexpected");  // Unexpected token
        auto tokens = lexer.tokenize();
        Parser parser(std::move(tokens));
        parser.parse();
    }, "Parser throws on unexpected token");
}

// =============================================================================
// VersionChecker Integration Tests
// =============================================================================

void test_version_checker_basic() {
    std::cout << "\n--- VersionChecker Basic Tests ---" << std::endl;

    // Test simple comparisons
    VersionChecker checker1(">= 1.2.0");
    TestFramework::assert_true(checker1.checkCompatibility("1.2.0"), "VersionChecker >= exact match");
    TestFramework::assert_true(checker1.checkCompatibility("1.3.0"), "VersionChecker >= greater");
    TestFramework::assert_true(!checker1.checkCompatibility("1.1.9"), "VersionChecker >= less");

    VersionChecker checker2("== 1.5.2");
    TestFramework::assert_true(checker2.checkCompatibility("1.5.2"), "VersionChecker == match");
    TestFramework::assert_true(!checker2.checkCompatibility("1.5.3"), "VersionChecker == no match");

    VersionChecker checker3("< 2.0.0");
    TestFramework::assert_true(checker3.checkCompatibility("1.9.9"), "VersionChecker < less");
    TestFramework::assert_true(!checker3.checkCompatibility("2.0.0"), "VersionChecker < equal");
    TestFramework::assert_true(!checker3.checkCompatibility("2.0.1"), "VersionChecker < greater");
}

void test_version_checker_complex() {
    std::cout << "\n--- VersionChecker Complex Tests ---" << std::endl;

    // Test AND expression
    VersionChecker checker1(">= 1.0.0 && < 2.0.0");
    TestFramework::assert_true(checker1.checkCompatibility("1.0.0"), "VersionChecker AND - lower bound");
    TestFramework::assert_true(checker1.checkCompatibility("1.5.0"), "VersionChecker AND - middle");
    TestFramework::assert_true(checker1.checkCompatibility("1.9.9"), "VersionChecker AND - upper bound");
    TestFramework::assert_true(!checker1.checkCompatibility("0.9.9"), "VersionChecker AND - below range");
    TestFramework::assert_true(!checker1.checkCompatibility("2.0.0"), "VersionChecker AND - above range");

    // Test OR expression
    VersionChecker checker2("== 1.5.2 || == 1.5.3");
    TestFramework::assert_true(checker2.checkCompatibility("1.5.2"), "VersionChecker OR - first match");
    TestFramework::assert_true(checker2.checkCompatibility("1.5.3"), "VersionChecker OR - second match");
    TestFramework::assert_true(!checker2.checkCompatibility("1.5.1"), "VersionChecker OR - no match");

    // Test complex parenthesized expression
    VersionChecker checker3("(>= 1.0.0 && < 1.5.0) || >= 2.0.0");
    TestFramework::assert_true(checker3.checkCompatibility("1.0.0"), "VersionChecker complex - first range start");
    TestFramework::assert_true(checker3.checkCompatibility("1.4.9"), "VersionChecker complex - first range end");
    TestFramework::assert_true(!checker3.checkCompatibility("1.5.0"), "VersionChecker complex - between ranges");
    TestFramework::assert_true(!checker3.checkCompatibility("1.9.9"), "VersionChecker complex - between ranges");
    TestFramework::assert_true(checker3.checkCompatibility("2.0.0"), "VersionChecker complex - second range start");
    TestFramework::assert_true(checker3.checkCompatibility("3.0.0"), "VersionChecker complex - second range");
}

void test_version_checker_edge_cases() {
    std::cout << "\n--- VersionChecker Edge Cases Tests ---" << std::endl;

    // Test with zero versions
    VersionChecker checker1(">= 0.0.0");
    TestFramework::assert_true(checker1.checkCompatibility("0.0.0"), "VersionChecker zero version");

    // Test with large version numbers
    VersionChecker checker2("< 999.999.999");
    TestFramework::assert_true(checker2.checkCompatibility("999.999.998"), "VersionChecker large version");

    // Test not equal
    VersionChecker checker3("!= 1.0.0");
    TestFramework::assert_true(checker3.checkCompatibility("1.0.1"), "VersionChecker != true");
    TestFramework::assert_true(!checker3.checkCompatibility("1.0.0"), "VersionChecker != false");

    // Test <= and >=
    VersionChecker checker4("<= 1.5.0");
    TestFramework::assert_true(checker4.checkCompatibility("1.5.0"), "VersionChecker <= equal");
    TestFramework::assert_true(checker4.checkCompatibility("1.4.9"), "VersionChecker <= less");

    VersionChecker checker5(">= 1.5.0");
    TestFramework::assert_true(checker5.checkCompatibility("1.5.0"), "VersionChecker >= equal");
    TestFramework::assert_true(checker5.checkCompatibility("1.5.1"), "VersionChecker >= greater");
}

// =============================================================================
// Real-world Usage Tests
// =============================================================================

void test_real_world_scenarios() {
    std::cout << "\n--- Real-world Scenario Tests ---" << std::endl;

    // Test common semver patterns
    struct TestCase {
        std::string rule;
        std::string version;
        bool expected;
        std::string description;
    };

    std::vector<TestCase> test_cases = {
        // Patch version compatibility
        {">= 1.2.0 && < 1.3.0", "1.2.5", true, "Patch version in range"},
        {">= 1.2.0 && < 1.3.0", "1.3.0", false, "Patch version out of range"},
        
        // Minor version compatibility
        {">= 1.0.0 && < 2.0.0", "1.9.9", true, "Minor version in range"},
        {">= 1.0.0 && < 2.0.0", "2.0.0", false, "Minor version out of range"},
        
        // Multiple allowed versions
        {"== 1.0.0 || == 1.1.0 || == 1.2.0", "1.1.0", true, "Multiple versions allowed"},
        {"== 1.0.0 || == 1.1.0 || == 1.2.0", "1.3.0", false, "Multiple versions not allowed"},
        
        // Complex compatibility rules
        {"(>= 1.0.0 && < 2.0.0) || (>= 3.0.0 && < 4.0.0)", "1.5.0", true, "Complex rule first range"},
        {"(>= 1.0.0 && < 2.0.0) || (>= 3.0.0 && < 4.0.0)", "3.5.0", true, "Complex rule second range"},
        {"(>= 1.0.0 && < 2.0.0) || (>= 3.0.0 && < 4.0.0)", "2.5.0", false, "Complex rule excluded range"},
        
        // Security update patterns
        {">= 1.2.5", "1.2.5", true, "Security update minimum"},
        {">= 1.2.5", "1.2.4", false, "Below security minimum"},
        
        // Beta/RC exclusion
        {"!= 2.0.0", "2.0.1", true, "Not beta version"},
        {"!= 2.0.0", "2.0.0", false, "Is beta version"},
    };

    for (const auto& test_case : test_cases) {
        try {
            VersionChecker checker(test_case.rule);
            bool result = checker.checkCompatibility(test_case.version);
            TestFramework::assert_true(result == test_case.expected, test_case.description);
        } catch (const std::exception& e) {
            TestFramework::assert_true(false, test_case.description + " (exception: " + e.what() + ")");
        }
    }
}

// =============================================================================
// Main Test Runner
// =============================================================================

int main() {
    std::cout << "=== Version Compatibility Parser Test Suite ===" << std::endl;
    std::cout << "Testing comprehensive functionality of the version compatibility system\n" << std::endl;

    try {
        // Version class tests
        test_version_parsing();
        test_version_comparison();
        test_version_string_conversion();

        // Lexer tests
        test_lexer_tokenization();
        test_lexer_whitespace_handling();
        test_lexer_error_handling();

        // Parser tests
        test_parser_simple_expressions();
        test_parser_complex_expressions();
        test_parser_error_handling();

        // VersionChecker integration tests
        test_version_checker_basic();
        test_version_checker_complex();
        test_version_checker_edge_cases();

        // Real-world scenario tests
        test_real_world_scenarios();

        TestFramework::print_summary();
        return TestFramework::all_passed() ? 0 : 1;

    } catch (const std::exception& e) {
        std::cerr << "Test suite failed with exception: " << e.what() << std::endl;
        return 1;
    } catch (...) {
        std::cerr << "Test suite failed with unknown exception" << std::endl;
        return 1;
    }
}