#pragma once

#include "ast.h"
#include <cstdint>
#include <optional>
#include <string>
#include <unordered_map>
#include <vector>

enum class SymbolKind { Variable, Constant, Parameter, Function };
enum class ValueType { Int, Void, Error };

struct Symbol {
    std::string name;
    SymbolKind kind;
    ValueType type;
    bool is_const;
    SourceLocation loc;
    std::vector<ValueType> param_types;
    std::optional<std::int32_t> constant;
};
using Scope = std::unordered_map<std::string, Symbol>;

class SemanticAnalyzer {
public:
    void analyze(const Program& program, SourceLocation last_line);
    const std::vector<std::size_t>& errorLines() const { return error_lines_; }

private:
    struct ExprResult {
        ValueType type = ValueType::Error;
        std::optional<std::int32_t> constant;
    };
    // A set of possible exits from a statement; zero means no reachable exit.
    enum Flow : unsigned { Normal = 1, Returned = 2, Broken = 4, Continued = 8 };
    std::vector<Scope> scopes_;
    std::vector<std::size_t> error_lines_;
    ValueType return_type_ = ValueType::Int;
    unsigned loop_depth_ = 0;
    bool saw_main_ = false;

    void enterScope();
    void leaveScope();
    bool declare(Symbol symbol);
    const Symbol* lookup(const std::string& name) const;
    void reportError(SourceLocation loc, const std::string& message);
    void analyzeFunction(const Function& fn);
    unsigned analyzeBlock(const Block& block, bool nested);
    unsigned analyzeStmt(const Stmt& stmt);
    void analyzeDecl(const Decl& decl);
    ExprResult analyzeExpr(const Expr& expr, bool need_int = true,
                           bool evaluated = true);
};
