/**
 * @file version_compatibility.cpp
 * @brief Implementation of a version compatibility checking system with expression parsing
 * 
 * This module provides a comprehensive version compatibility checking system that supports
 * complex expressions involving version comparisons and logical operations. It includes:
 * - Semantic version parsing (major.minor.patch format)
 * - Lexical analysis for compatibility rules
 * - Expression parsing with operator precedence
 * - AST-based evaluation of version compatibility rules
 * 
 * Supported operators:
 * - Comparison: ==, !=, <, <=, >, >=
 * - Logical: &&, ||
 * - Grouping: ()
 * 
 * Example usage:
 *   VersionChecker checker(">= 1.2.0 && < 2.0.0");
 *   bool compatible = checker.checkCompatibility("1.5.3"); // returns true
 * 
 * @author LinkuraLocalify Team
 * @version 1.0
 */

#include "version_compatibility.h"
#include <sstream>
#include <stdexcept>
#include <cctype>
#include <algorithm>

namespace VersionCompatibility {

// =============================================================================
// Version Class Implementation
// =============================================================================

/**
 * @brief Constructs a Version object from a semantic version string
 * 
 * Parses a version string in the format "major.minor.patch" and extracts
 * the numeric components. Missing components default to 0.
 * 
 * @param version_str Version string to parse (e.g., "1.2.3")
 * @throws std::invalid_argument if version components are not valid integers
 * 
 * Examples:
 *   Version("1.2.3")    -> major=1, minor=2, patch=3
 *   Version("2.0")      -> major=2, minor=0, patch=0
 *   Version("1")        -> major=1, minor=0, patch=0
 */
Version::Version(const std::string& version_str) {
    std::istringstream ss(version_str);
    std::string token;
    
    // Parse major.minor.patch format
    if (std::getline(ss, token, '.')) {
        major = std::stoi(token);
    }
    if (std::getline(ss, token, '.')) {
        minor = std::stoi(token);
    }
    if (std::getline(ss, token, '.')) {
        patch = std::stoi(token);
    }
}

/**
 * @brief Compares this version with another version
 * 
 * Performs lexicographic comparison of version components in order:
 * major -> minor -> patch
 * 
 * @param other The version to compare against
 * @return Negative if this < other, 0 if equal, positive if this > other
 */
int Version::compare(const Version& other) const {
    if (major != other.major) {
        return major - other.major;
    }
    if (minor != other.minor) {
        return minor - other.minor;
    }
    return patch - other.patch;
}

/**
 * @brief Converts the version to its string representation
 * 
 * @return String representation in "major.minor.patch" format
 */
std::string Version::toString() const {
    return std::to_string(major) + "." + std::to_string(minor) + "." + std::to_string(patch);
}

// =============================================================================
// AST Node Implementations
// =============================================================================

/**
 * @brief Evaluates a version comparison expression
 * 
 * Compares the current version against the target version using the specified
 * comparison operator.
 * 
 * @param current_version The version to evaluate against
 * @return true if the comparison condition is met, false otherwise
 */
bool ComparisonNode::evaluate(const Version& current_version) const {
    if (operator_ == "==") {
        return current_version == target_version_;
    } else if (operator_ == "!=") {
        return current_version != target_version_;
    } else if (operator_ == "<") {
        return current_version < target_version_;
    } else if (operator_ == "<=") {
        return current_version <= target_version_;
    } else if (operator_ == ">") {
        return current_version > target_version_;
    } else if (operator_ == ">=") {
        return current_version >= target_version_;
    }
    return false;
}

/**
 * @brief Evaluates a logical operation between two sub-expressions
 * 
 * Performs logical AND (&&) or OR (||) operations on the results of
 * evaluating the left and right child nodes.
 * 
 * @param current_version The version to evaluate against
 * @return true if the logical condition is met, false otherwise
 */
bool LogicalNode::evaluate(const Version& current_version) const {
    if (operator_ == "&&") {
        return left_->evaluate(current_version) && right_->evaluate(current_version);
    } else if (operator_ == "||") {
        return left_->evaluate(current_version) || right_->evaluate(current_version);
    }
    return false;
}

// =============================================================================
// Lexer Implementation
// =============================================================================

/**
 * @brief Skips whitespace characters in the input stream
 * 
 * Advances the position pointer past any whitespace characters
 * (spaces, tabs, newlines, etc.) in the input string.
 */
void Lexer::skipWhitespace() {
    while (position_ < input_.length() && std::isspace(input_[position_])) {
        position_++;
    }
}

/**
 * @brief Reads a version number from the input stream
 * 
 * Extracts a sequence of digits and dots that form a version number.
 * Stops at the first non-digit, non-dot character.
 * 
 * @return String containing the version number
 */
std::string Lexer::readVersion() {
    std::string version;
    while (position_ < input_.length() && 
           (std::isdigit(input_[position_]) || input_[position_] == '.')) {
        version += input_[position_];
        position_++;
    }
    return version;
}

/**
 * @brief Reads a comparison operator from the input stream
 * 
 * Recognizes both single-character (< >) and double-character (==, !=, <=, >=)
 * comparison operators.
 * 
 * @return String containing the operator
 */
std::string Lexer::readOperator() {
    std::string op;
    char current = input_[position_];
    
    if (current == '=' || current == '!' || current == '<' || current == '>') {
        op += current;
        position_++;
        
        // Check for two-character operators
        if (position_ < input_.length() && input_[position_] == '=') {
            op += '=';
            position_++;
        }
    }
    
    return op;
}

/**
 * @brief Tokenizes the input string into a sequence of tokens
 * 
 * Performs lexical analysis on the input string, breaking it down into
 * meaningful tokens for parsing. Recognizes:
 * - Version numbers (digits and dots)
 * - Comparison operators (==, !=, <, <=, >, >=)
 * - Logical operators (&& ||)
 * - Parentheses for grouping
 * 
 * @return Vector of tokens representing the input expression
 * @throws std::runtime_error for unknown characters
 */
std::vector<Token> Lexer::tokenize() {
    std::vector<Token> tokens;
    
    while (position_ < input_.length()) {
        skipWhitespace();
        
        if (position_ >= input_.length()) {
            break;
        }
        
        char current = input_[position_];
        
        if (std::isdigit(current)) {
            // Version number
            std::string version = readVersion();
            tokens.emplace_back(TokenType::VERSION, version);
        } else if (current == '=' || current == '!' || current == '<' || current == '>') {
            // Comparison operator
            std::string op = readOperator();
            tokens.emplace_back(TokenType::OPERATOR, op);
        } else if (current == '&' && position_ + 1 < input_.length() && input_[position_ + 1] == '&') {
            // Logical AND
            tokens.emplace_back(TokenType::AND, "&&");
            position_ += 2;
        } else if (current == '|' && position_ + 1 < input_.length() && input_[position_ + 1] == '|') {
            // Logical OR
            tokens.emplace_back(TokenType::OR, "||");
            position_ += 2;
        } else if (current == '(') {
            // Left parenthesis
            tokens.emplace_back(TokenType::LEFT_PAREN, "(");
            position_++;
        } else if (current == ')') {
            // Right parenthesis
            tokens.emplace_back(TokenType::RIGHT_PAREN, ")");
            position_++;
        } else {
            // Unknown character
            throw std::runtime_error("Unknown character: " + std::string(1, current));
        }
    }
    
    tokens.emplace_back(TokenType::END_OF_INPUT, "");
    return tokens;
}

// =============================================================================
// Parser Implementation
// =============================================================================

/**
 * @brief Gets the current token without advancing the position
 * 
 * @return Reference to the current token, or END_OF_INPUT if at end
 */
Token& Parser::currentToken() {
    if (position_ >= tokens_.size()) {
        static Token endToken(TokenType::END_OF_INPUT, "");
        return endToken;
    }
    return tokens_[position_];
}

const Token& Parser::currentToken() const {
    if (position_ >= tokens_.size()) {
        static Token endToken(TokenType::END_OF_INPUT, "");
        return endToken;
    }
    return tokens_[position_];
}

/**
 * @brief Advances to the next token and returns it
 * 
 * @return Reference to the next token
 */
Token& Parser::nextToken() {
    if (position_ < tokens_.size() - 1) {
        position_++;
    }
    return currentToken();
}

/**
 * @brief Checks if the parser has reached the end of input
 * 
 * @return true if at end of token stream, false otherwise
 */
bool Parser::isAtEnd() const {
    return position_ >= tokens_.size() || currentToken().type == TokenType::END_OF_INPUT;
}

/**
 * @brief Parses a complete expression (top-level entry point)
 * 
 * @return AST node representing the parsed expression
 */
std::unique_ptr<ASTNode> Parser::parseExpression() {
    return parseOrExpression();
}

/**
 * @brief Parses OR expressions with left-associativity
 * 
 * Handles logical OR operations (||) with proper precedence.
 * OR has lower precedence than AND.
 * 
 * @return AST node representing the OR expression
 */
std::unique_ptr<ASTNode> Parser::parseOrExpression() {
    auto left = parseAndExpression();
    
    while (currentToken().type == TokenType::OR) {
        std::string op = currentToken().value;
        nextToken();
        auto right = parseAndExpression();
        left = std::make_unique<LogicalNode>(op, std::move(left), std::move(right));
    }
    
    return left;
}

/**
 * @brief Parses AND expressions with left-associativity
 * 
 * Handles logical AND operations (&&) with proper precedence.
 * AND has higher precedence than OR.
 * 
 * @return AST node representing the AND expression
 */
std::unique_ptr<ASTNode> Parser::parseAndExpression() {
    auto left = parsePrimaryExpression();
    
    while (currentToken().type == TokenType::AND) {
        std::string op = currentToken().value;
        nextToken();
        auto right = parsePrimaryExpression();
        left = std::make_unique<LogicalNode>(op, std::move(left), std::move(right));
    }
    
    return left;
}

/**
 * @brief Parses primary expressions (comparisons and parenthesized expressions)
 * 
 * Handles the highest precedence expressions:
 * - Parenthesized expressions: (expression)
 * - Version comparisons: operator version
 * 
 * @return AST node representing the primary expression
 * @throws std::runtime_error for syntax errors
 */
std::unique_ptr<ASTNode> Parser::parsePrimaryExpression() {
    if (currentToken().type == TokenType::LEFT_PAREN) {
        nextToken(); // Skip '('
        auto expr = parseExpression();
        if (currentToken().type != TokenType::RIGHT_PAREN) {
            throw std::runtime_error("Expected ')'");
        }
        nextToken(); // Skip ')'
        return expr;
    }
    
    if (currentToken().type == TokenType::OPERATOR) {
        std::string op = currentToken().value;
        nextToken();
        
        if (currentToken().type != TokenType::VERSION) {
            throw std::runtime_error("Expected version after operator");
        }
        
        std::string version_str = currentToken().value;
        nextToken();
        
        Version version(version_str);
        return std::make_unique<ComparisonNode>(op, version);
    }
    
    throw std::runtime_error("Unexpected token: " + currentToken().value);
}

/**
 * @brief Parses the token stream into an Abstract Syntax Tree
 * 
 * Main parsing entry point that builds an AST from the tokenized input.
 * Ensures all tokens are consumed during parsing.
 * 
 * @return Root node of the parsed AST
 * @throws std::runtime_error for syntax errors or unexpected tokens
 */
std::unique_ptr<ASTNode> Parser::parse() {
    auto result = parseExpression();
    if (!isAtEnd()) {
        throw std::runtime_error("Unexpected token after expression");
    }
    return result;
}

// =============================================================================
// VersionChecker Implementation
// =============================================================================

/**
 * @brief Constructs a VersionChecker with the given compatibility rule
 * 
 * Parses the rule string into an AST for efficient evaluation.
 * The rule string should contain a valid version compatibility expression.
 * 
 * @param rule Version compatibility rule string (e.g., ">= 1.0.0 && < 2.0.0")
 * @throws std::runtime_error for invalid rule syntax
 * 
 * Example rules:
 *   ">= 1.2.0"                    - Version must be at least 1.2.0
 *   ">= 1.0.0 && < 2.0.0"        - Version between 1.0.0 and 2.0.0
 *   "== 1.5.2 || == 1.5.3"       - Exactly version 1.5.2 or 1.5.3
 *   "(>= 1.0.0 && < 1.5.0) || >= 2.0.0" - Complex expression with grouping
 */
VersionChecker::VersionChecker(const std::string& rule) {
    Lexer lexer(rule);
    auto tokens = lexer.tokenize();
    Parser parser(std::move(tokens));
    ast_ = parser.parse();
}

/**
 * @brief Checks if a version string is compatible with the rule
 * 
 * @param current_version Version string to check (e.g., "1.2.3")
 * @return true if the version satisfies the compatibility rule, false otherwise
 * @throws std::invalid_argument if version string is malformed
 */
bool VersionChecker::checkCompatibility(const std::string& current_version) const {
    Version version(current_version);
    return checkCompatibility(version);
}

/**
 * @brief Checks if a Version object is compatible with the rule
 * 
 * @param current_version Version object to evaluate
 * @return true if the version satisfies the compatibility rule, false otherwise
 */
bool VersionChecker::checkCompatibility(const Version& current_version) const {
    return ast_->evaluate(current_version);
}

} // namespace VersionCompatibility