#pragma once
#include <string>
#include <vector>
#include <memory>

/**
 *
 * >=1.2.0 && <2.0.0\n
 * ==1.5.2 || ==1.5.3\n
 * (>=1.0.0 && <1.5.0) || >=2.0.0\n
 * ==, !=, >=, <=, >, <, &&, ||, ()\n
 */
namespace VersionCompatibility {

// 版本结构体
struct Version {
    int major = 0;
    int minor = 0;
    int patch = 0;
    
    Version() = default;
    Version(int maj, int min, int pat) : major(maj), minor(min), patch(pat) {}
    explicit Version(const std::string& version_str);
    
    int compare(const Version& other) const;
    bool operator==(const Version& other) const { return compare(other) == 0; }
    bool operator!=(const Version& other) const { return compare(other) != 0; }
    bool operator<(const Version& other) const { return compare(other) < 0; }
    bool operator<=(const Version& other) const { return compare(other) <= 0; }
    bool operator>(const Version& other) const { return compare(other) > 0; }
    bool operator>=(const Version& other) const { return compare(other) >= 0; }
    
    std::string toString() const;
};

// Token类型
enum class TokenType {
    VERSION,
    OPERATOR,
    AND,
    OR,
    LEFT_PAREN,
    RIGHT_PAREN,
    END_OF_INPUT
};

// Token结构
struct Token {
    TokenType type;
    std::string value;
    
    Token(TokenType t, const std::string& v) : type(t), value(v) {}
};

// 抽象语法树节点
class ASTNode {
public:
    virtual ~ASTNode() = default;
    virtual bool evaluate(const Version& current_version) const = 0;
    virtual std::string toHumanReadable() const = 0;
    virtual std::string getRecommendVersion() const = 0;
};

// 版本比较节点
class ComparisonNode : public ASTNode {
private:
    std::string operator_;
    Version target_version_;
    
public:
    ComparisonNode(const std::string& op, const Version& version) 
        : operator_(op), target_version_(version) {}
    
    bool evaluate(const Version& current_version) const override;
    std::string toHumanReadable() const override;
    std::string getRecommendVersion() const override;
};

// 逻辑操作节点
class LogicalNode : public ASTNode {
private:
    std::string operator_;
    std::unique_ptr<ASTNode> left_;
    std::unique_ptr<ASTNode> right_;
    
public:
    LogicalNode(const std::string& op, std::unique_ptr<ASTNode> left, std::unique_ptr<ASTNode> right)
        : operator_(op), left_(std::move(left)), right_(std::move(right)) {}
    
    bool evaluate(const Version& current_version) const override;
    std::string toHumanReadable() const override;
    std::string getRecommendVersion() const override;
};

// 词法分析器
class Lexer {
private:
    std::string input_;
    size_t position_;
    
    void skipWhitespace();
    std::string readVersion();
    std::string readOperator();
    
public:
    explicit Lexer(const std::string& input) : input_(input), position_(0) {}
    
    std::vector<Token> tokenize();
};

// 语法分析器
class Parser {
private:
    std::vector<Token> tokens_;
    size_t position_;
    
    Token& currentToken();
    const Token& currentToken() const;
    Token& nextToken();
    bool isAtEnd() const;
    
    std::unique_ptr<ASTNode> parseExpression();
    std::unique_ptr<ASTNode> parseOrExpression();
    std::unique_ptr<ASTNode> parseAndExpression();
    std::unique_ptr<ASTNode> parsePrimaryExpression();
    
public:
    explicit Parser(std::vector<Token> tokens) : tokens_(std::move(tokens)), position_(0) {}
    
    std::unique_ptr<ASTNode> parse();
};

// 版本兼容性检查器
class VersionChecker {
private:
    std::unique_ptr<ASTNode> ast_;
    std::string original_rule_;
    
public:
    explicit VersionChecker(const std::string& rule);
    
    bool checkCompatibility(const std::string& current_version) const;
    bool checkCompatibility(const Version& current_version) const;
    
    // Convert rule to human readable format
    std::string toHumanReadable() const;
    
    // Get recommended version (first exact match found, empty if range)
    std::string getRecommendVersion() const;
};

} // namespace VersionCompatibility