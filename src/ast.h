
#pragma once

#include <cstddef>
#include <memory>
#include <string>
#include <utility>
#include <vector>

// 源代码位置
struct SourceLocation {
    std::size_t line = 1;
    std::size_t column = 1;
};

// 所有 AST 节点的基类
struct ASTNode {
    SourceLocation loc;

    explicit ASTNode(SourceLocation location)
        : loc(location) {}

    virtual ~ASTNode() = default;
};

// 表达式基类
struct Expr : ASTNode {
    using ASTNode::ASTNode;
};

// 语句基类
struct Stmt : ASTNode {
    using ASTNode::ASTNode;
};


struct Number : Expr {
    std::string value;

    Number(SourceLocation loc, std::string val)
        : Expr(loc), value(std::move(val)) {}
};

struct Variable : Expr {
    std::string name;

    Variable(SourceLocation loc, std::string n)
        : Expr(loc), name(std::move(n)) {}
};


struct Binary : Expr {
    std::string op;
    std::unique_ptr<Expr> left;
    std::unique_ptr<Expr> right;

    Binary(SourceLocation loc,
           std::string oper,
           std::unique_ptr<Expr> lhs,
           std::unique_ptr<Expr> rhs)
        : Expr(loc),
          op(std::move(oper)),
          left(std::move(lhs)),
          right(std::move(rhs)) {}
};


struct Unary : Expr {
    std::string op;
    std::unique_ptr<Expr> operand;

    Unary(SourceLocation loc,
          std::string oper,
          std::unique_ptr<Expr> expr)
        : Expr(loc),
          op(std::move(oper)),
          operand(std::move(expr)) {}
};

struct Call : Expr {
    std::string name;
    std::vector<std::unique_ptr<Expr>> args;

    Call(SourceLocation loc,
         std::string n,
         std::vector<std::unique_ptr<Expr>> arguments)
        : Expr(loc),
          name(std::move(n)),
          args(std::move(arguments)) {}
};


struct ExprStmt : Stmt {
    std::unique_ptr<Expr> expr;

    ExprStmt(SourceLocation loc,
             std::unique_ptr<Expr> expression)
        : Stmt(loc),
          expr(std::move(expression)) {}
};


struct EmptyStmt : Stmt {
    using Stmt::Stmt;
};

struct BreakStmt : Stmt {
    using Stmt::Stmt;
};

struct ContinueStmt : Stmt {
    using Stmt::Stmt;
};

// 变量声明：int a = 10;
struct Decl : Stmt {
    std::string name;
    std::unique_ptr<Expr> initializer;
    bool is_const;

    Decl(SourceLocation loc,
         std::string n,
         std::unique_ptr<Expr> init,
         bool constant)
        : Stmt(loc),
          name(std::move(n)),
          initializer(std::move(init)),
          is_const(constant) {}
};

// 赋值：a = 10;
struct Assign : Stmt {
    std::string name;
    std::unique_ptr<Expr> value;

    Assign(SourceLocation loc,
           std::string n,
           std::unique_ptr<Expr> val)
        : Stmt(loc),
          name(std::move(n)),
          value(std::move(val)) {}
};

// 返回：return a; 或 return;
struct Return : Stmt {
    std::unique_ptr<Expr> value;

    Return(SourceLocation loc,
           std::unique_ptr<Expr> val = nullptr)
        : Stmt(loc),
          value(std::move(val)) {}
};


// if (condition) then_stmt else else_stmt
struct If : Stmt {
    std::unique_ptr<Expr> condition;
    std::unique_ptr<Stmt> then_stmt;
    std::unique_ptr<Stmt> else_stmt;

    If(SourceLocation loc,
       std::unique_ptr<Expr> cond,
       std::unique_ptr<Stmt> then_branch,
       std::unique_ptr<Stmt> else_branch = nullptr)
        : Stmt(loc),
          condition(std::move(cond)),
          then_stmt(std::move(then_branch)),
          else_stmt(std::move(else_branch)) {}
};

// while (condition) body
struct While : Stmt {
    std::unique_ptr<Expr> condition;
    std::unique_ptr<Stmt> body;

    While(SourceLocation loc,
          std::unique_ptr<Expr> cond,
          std::unique_ptr<Stmt> loop_body)
        : Stmt(loc),
          condition(std::move(cond)),
          body(std::move(loop_body)) {}
};


struct Block : Stmt {
    std::vector<std::unique_ptr<Stmt>> statements;

    Block(SourceLocation loc,
          std::vector<std::unique_ptr<Stmt>> stmts)
        : Stmt(loc),
          statements(std::move(stmts)) {}
};


// 函数形参：int x
struct Param : ASTNode {
    std::string name;

    Param(SourceLocation loc, std::string n)
        : ASTNode(loc), name(std::move(n)) {}
};

// 函数定义
struct Function : ASTNode {
    std::string return_type;
    std::string name;
    std::vector<Param> params;
    std::unique_ptr<Block> body;

    Function(SourceLocation loc,
             std::string type,
             std::string n,
             std::vector<Param> p,
             std::unique_ptr<Block> b)
        : ASTNode(loc),
          return_type(std::move(type)),
          name(std::move(n)),
          params(std::move(p)),
          body(std::move(b)) {}
};

// 整个程序
struct Program : ASTNode {
    std::vector<std::unique_ptr<Function>> functions;

    Program(SourceLocation loc,
            std::vector<std::unique_ptr<Function>> funcs)
        : ASTNode(loc),
          functions(std::move(funcs)) {}
};
