
#include "parser.h"

#include <utility>

Parser::Parser(std::vector<Token> tokens,
               SourceLocation eof_location)
    : tokens_(std::move(tokens)),
      eof_token_{
          TokenKind::Symbol,
          "<EOF>",
          eof_location.line,
          eof_location.column
      } {}

// 查看当前或后续 Token，不消耗
const Token& Parser::peek(std::size_t offset) const {
    if (pos_ >= tokens_.size() ||
        offset >= tokens_.size() - pos_) {
        return eof_token_;
    }

    return tokens_[pos_ + offset];
}

// 判断是否已经读取到输入末尾
bool Parser::atEnd() const {
    return pos_ >= tokens_.size();
}

// 消耗当前 Token
const Token& Parser::advance() {
    if (atEnd()) {
        return eof_token_;
    }

    return tokens_[pos_++];
}

// 按 Token 文本判断
bool Parser::check(std::string_view lexeme) const {
    return !atEnd() &&
           std::string_view(peek().lexeme) == lexeme;
}

// 按 Token 类型判断
bool Parser::check(TokenKind kind) const {
    return !atEnd() && peek().kind == kind;
}

// 匹配指定文本，成功则消耗
bool Parser::match(std::string_view lexeme) {
    if (!check(lexeme)) {
        return false;
    }

    advance();
    return true;
}

// 匹配指定 Token 类型
bool Parser::match(TokenKind kind) {
    if (!check(kind)) {
        return false;
    }

    advance();
    return true;
}

// 要求当前 Token 必须具有指定文本
const Token& Parser::expect(std::string_view lexeme) {
    if (!check(lexeme)) {
        syntaxError();
    }

    return advance();
}

// 要求当前 Token 必须属于指定类型
const Token& Parser::expect(TokenKind kind) {
    if (!check(kind)) {
        syntaxError();
    }

    return advance();
}


void Parser::recordSyntaxError() {
    const std::size_t line =
        peek().line == 0 ? 0 : peek().line - 1;

    error_lines_.push_back(line);
}

[[noreturn]] void Parser::syntaxError() {
    recordSyntaxError();
    throw SyntaxError(error_lines_.back());
}

// 判断当前位置是否像一个新的函数定义
bool Parser::startsFunctionDefinition() const {
    return (check("int") || check("void")) &&
           peek(1).kind == TokenKind::Identifier &&
           peek(2).lexeme == "(";
}

// 语句级错误恢复
void Parser::synchronizeStatement() {
    while (!atEnd() &&
           !check(";") &&
           !check("}") &&
           !check("{") &&
           !check("if") &&
           !check("while") &&
           !check("return") &&
           !check("break") &&
           !check("continue") &&
           !check("int") &&
           !check("const")) {
        advance();
    }

    // 分号属于刚才出错的语句，可以跳过
    if (check(";")) {
        advance();
    }
}

// 函数级错误恢复
void Parser::synchronizeFunction() {
    while (!atEnd() && !startsFunctionDefinition()) {
        advance();
    }
}



std::unique_ptr<Expr> Parser::parsePrimaryExpr() {

    // 1. 整数常量，例如 123
    if (check(TokenKind::IntConst)) {
        Token token = advance();

        return std::make_unique<Number>(
            SourceLocation{token.line, token.column},
            token.lexeme
        );
    }

    // 2. 标识符：变量或函数调用
    if (check(TokenKind::Identifier)) {
        Token token = advance();

        // 如果标识符后面是 '('，则是函数调用
        if (match("(")) {
            std::vector<std::unique_ptr<Expr>> args;

            // 解析函数实参
            if (!check(")")) {
                do {
                    args.push_back(parseExpr());
                } while (match(","));
            }

            expect(")");

            return std::make_unique<Call>(
                SourceLocation{token.line, token.column},
                token.lexeme,
                std::move(args)
            );
        }

        // 否则是普通变量
        return std::make_unique<Variable>(
            SourceLocation{token.line, token.column},
            token.lexeme
        );
    }

    // 3. 括号表达式，例如 (a + b)
    if (match("(")) {
        auto expr = parseExpr();
        expect(")");
        return expr;
    }

    // 4. 以上都不符合：语法错误
    syntaxError();
}
std::unique_ptr<Expr> Parser::parseUnaryExpr() {

    if (check("+") || check("-") || check("!")) {
        Token op = advance();

        auto operand = parseUnaryExpr();

        return std::make_unique<Unary>(
            SourceLocation{op.line, op.column},
            op.lexeme,
            std::move(operand)
        );
    }

    return parsePrimaryExpr();
}std::unique_ptr<Expr> Parser::parseMulExpr() {

    auto left = parseUnaryExpr();

    while (check("*") || check("/") || check("%")) {
        Token op = advance();

        auto right = parseUnaryExpr();

        left = std::make_unique<Binary>(
            SourceLocation{op.line, op.column},
            op.lexeme,
            std::move(left),
            std::move(right)
        );
    }

    return left;
}std::unique_ptr<Expr> Parser::parseAddExpr() {

    auto left = parseMulExpr();

    while (check("+") || check("-")) {
        Token op = advance();

        auto right = parseMulExpr();

        left = std::make_unique<Binary>(
            SourceLocation{op.line, op.column},
            op.lexeme,
            std::move(left),
            std::move(right)
        );
    }

    return left;
}std::unique_ptr<Expr> Parser::parseRelExpr() {

    auto left = parseAddExpr();

    while (check("<")  || check(">")  ||
           check("<=") || check(">=") ||
           check("==") || check("!=")) {

        Token op = advance();
        auto right = parseAddExpr();

        left = std::make_unique<Binary>(
            SourceLocation{op.line, op.column},
            op.lexeme,
            std::move(left),
            std::move(right)
        );
    }

    return left;
}std::unique_ptr<Expr> Parser::parseLAndExpr() {

    auto left = parseRelExpr();

    while (check("&&")) {
        Token op = advance();
        auto right = parseRelExpr();

        left = std::make_unique<Binary>(
            SourceLocation{op.line, op.column},
            op.lexeme,
            std::move(left),
            std::move(right)
        );
    }

    return left;
}std::unique_ptr<Expr> Parser::parseLOrExpr() {

    auto left = parseLAndExpr();

    while (check("||")) {
        Token op = advance();
        auto right = parseLAndExpr();

        left = std::make_unique<Binary>(
            SourceLocation{op.line, op.column},
            op.lexeme,
            std::move(left),
            std::move(right)
        );
    }

    return left;
}std::unique_ptr<Expr> Parser::parseExpr() {
    return parseLOrExpr();
}
std::unique_ptr<Stmt> Parser::parseStmt() {
    Token start = peek();
    SourceLocation loc{start.line, start.column};

    // 1. 语句块 { ... }
    if (check("{")) {
        return parseBlock();
    }

    // 2. 空语句 ;
    if (match(";")) {
        return std::make_unique<EmptyStmt>(loc);
    }

    // 3. 变量声明
    if (check("int") || check("const")) {
        bool is_const = match("const");

        expect("int");
        Token name = expect(TokenKind::Identifier);

        std::unique_ptr<Expr> initializer;

        if (match("=")) {
            initializer = parseExpr();
        } else if (is_const) {
            // const int 必须初始化
            syntaxError();
        }

        expect(";");

        return std::make_unique<Decl>(
            loc,
            name.lexeme,
            std::move(initializer),
            is_const
        );
    }

    // 4. if 语句
    if (match("if")) {
        if (!match("(")) {
            recordSyntaxError();
        }
        auto condition = parseExpr();
        expect(")");

        auto then_stmt = parseStmt();

        std::unique_ptr<Stmt> else_stmt;
        if (match("else")) {
            else_stmt = parseStmt();
        }

        return std::make_unique<If>(
            loc,
            std::move(condition),
            std::move(then_stmt),
            std::move(else_stmt)
        );
    }

    // 5. while 语句
    if (match("while")) {
        if (!match("(")) {
            recordSyntaxError();
        }

        std::unique_ptr<Expr> condition;

        // 空条件：while ()
        if (check(")")) {
            recordSyntaxError();
        } else {
            condition = parseExpr();
        }

        expect(")");

        // 即使条件缺失，也继续解析循环体
        auto body = parseStmt();

        return std::make_unique<While>(
            loc,
            std::move(condition),
            std::move(body)
        );
    }

    // 6. break
    if (match("break")) {
        expect(";");
        return std::make_unique<BreakStmt>(loc);
    }

    // 7. continue
    if (match("continue")) {
        expect(";");
        return std::make_unique<ContinueStmt>(loc);
    }

    // 8. return
    if (match("return")) {
        std::unique_ptr<Expr> value;

        if (!check(";")) {
            value = parseExpr();
        }

        expect(";");

        return std::make_unique<Return>(
            loc, std::move(value)
        );
    }

    // 9. 赋值语句：ID = Expr ;
    if (check(TokenKind::Identifier) &&
        peek(1).lexeme == "=") {

        Token name = advance();
        expect("=");

        auto value = parseExpr();
        expect(";");

        return std::make_unique<Assign>(
            loc,
            name.lexeme,
            std::move(value)
        );
    }

    // 10. 表达式语句：Expr ;
    auto expr = parseExpr();
    expect(";");

    return std::make_unique<ExprStmt>(
        loc, std::move(expr)
    );
}


std::unique_ptr<Block> Parser::parseBlock() {
    Token start = expect("{");

    std::vector<std::unique_ptr<Stmt>> statements;

    while (!check("}") && !atEnd()) {

        // 如果在函数体中遇到新的函数定义，
        // 很可能前一个函数漏写了 '}'
        if (startsFunctionDefinition()) {
            recordSyntaxError();

            return std::make_unique<Block>(
                SourceLocation{start.line, start.column},
                std::move(statements)
            );
        }

        std::size_t before = pos_;

        try {
            statements.push_back(parseStmt());
        } catch (const SyntaxError&) {
            // 已经记录过错误，恢复到下一个语句
            synchronizeStatement();
        }

        // 防止解析器卡在同一个 Token 上
        if (pos_ == before &&
            !check("}") && !atEnd()) {
            advance();
        }
    }

    // EOF 时仍未遇到右花括号
    if (atEnd()) {
        recordSyntaxError();

        return std::make_unique<Block>(
            SourceLocation{start.line, start.column},
            std::move(statements)
        );
    }

    expect("}");

    return std::make_unique<Block>(
        SourceLocation{start.line, start.column},
        std::move(statements)
    );
}


std::unique_ptr<Function> Parser::parseFunction() {
    // 返回类型必须是 int 或 void
    if (!check("int") && !check("void")) {
        syntaxError();
    }

    Token type = advance();
    Token name = expect(TokenKind::Identifier);

    expect("(");

    std::vector<Param> params;

    // 解析形参列表
    if (!check(")")) {
        do {
            expect("int");

            Token param =
                expect(TokenKind::Identifier);

            params.emplace_back(
                SourceLocation{param.line, param.column},
                param.lexeme
            );

        } while (match(","));
    }

    if (!match(")")) {
        if (check("{")) {
           // 已经遇到函数体，推测漏写了 ')'
           recordSyntaxError();
     } else {
         syntaxError();
      }
    }

    auto body = parseBlock();

    return std::make_unique<Function>(
        SourceLocation{type.line, type.column},
        type.lexeme,
        name.lexeme,
        std::move(params),
        std::move(body)
    );
}

std::unique_ptr<Program> Parser::parseProgram() {
    Token start = peek();

    std::vector<std::unique_ptr<Function>> functions;

    if (atEnd()) {
        recordSyntaxError();

        return std::make_unique<Program>(
            SourceLocation{start.line, start.column},
            std::move(functions)
        );
    }

    while (!atEnd()) {
        std::size_t before = pos_;

        try {
            functions.push_back(parseFunction());
        } catch (const SyntaxError&) {
            synchronizeFunction();
        }

        if (pos_ == before && !atEnd()) {
            advance();
        }
    }

    return std::make_unique<Program>(
        SourceLocation{start.line, start.column},
        std::move(functions)
    );
}
