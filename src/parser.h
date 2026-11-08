
#pragma once

#include "ast.h"
#include "lexer.h"

#include <cstddef>
#include <memory>
#include <stdexcept>
#include <string_view>
#include <vector>

// 语法错误，行号使用评测要求的 0-based 编号
struct SyntaxError : std::runtime_error {
    std::size_t line;

    explicit SyntaxError(std::size_t zero_based_line)
        : std::runtime_error("syntax error"),
          line(zero_based_line) {}
};

class Parser {
public:
    // 接收 Lexer 生成的 Token 和源文件结束位置
    Parser(std::vector<Token> tokens,
           SourceLocation eof_location);

    // 解析完整程序（下一阶段实现）
    std::unique_ptr<Program> parseProgram();

    const std::vector<std::size_t>& errorLines() const {
        return error_lines_;
    }

private:
    // Token 序列
    std::vector<Token> tokens_;

    // 输入结束时使用的特殊 Token
    Token eof_token_;

    // 当前 Token 下标
    std::size_t pos_ = 0;

    // 已发现的语法错误行号
    std::vector<std::size_t> error_lines_;

    // 基础 Token 操作
    const Token& peek(std::size_t offset = 0) const;
    bool atEnd() const;
    const Token& advance();

    bool check(std::string_view lexeme) const;
    bool match(std::string_view lexeme);

    bool check(TokenKind kind) const;
    bool match(TokenKind kind);

    const Token& expect(std::string_view lexeme);
    const Token& expect(TokenKind kind);

    // 报告错误并抛出异常
    [[noreturn]] void syntaxError();
    // 表达式解析（从低优先级到高优先级）
    std::unique_ptr<Expr> parseExpr();

    std::unique_ptr<Expr> parseLOrExpr();
    std::unique_ptr<Expr> parseLAndExpr();
    std::unique_ptr<Expr> parseRelExpr();

    std::unique_ptr<Expr> parseAddExpr();
    std::unique_ptr<Expr> parseMulExpr();

    std::unique_ptr<Expr> parseUnaryExpr();
    std::unique_ptr<Expr> parsePrimaryExpr();
    // 语句解析
    std::unique_ptr<Stmt> parseStmt();

    // 语句块解析
    std::unique_ptr<Block> parseBlock();

    // 函数解析
    std::unique_ptr<Function> parseFunction();
    void recordSyntaxError();
    void synchronizeStatement();
    void synchronizeFunction();
    bool startsFunctionDefinition() const;

};
