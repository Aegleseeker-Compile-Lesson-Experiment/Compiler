#include "semantic.h"

#include <algorithm>
#include <iostream>
#include <limits>
#include <utility>

namespace {
using I32 = std::int32_t;
using I64 = std::int64_t;
constexpr I64 MIN_I32 = std::numeric_limits<I32>::min();
constexpr I64 MAX_I32 = std::numeric_limits<I32>::max();

// Decimal magnitude, including INT_MAX+1 for the unary-minus special case.
std::optional<I64> magnitude(const std::string& s) {
    I64 value = 0;
    if (s.empty()) return std::nullopt;
    for (char c : s) {
        if (c < '0' || c > '9') return std::nullopt;
        if (value > (MAX_I32 + 1 - (c - '0')) / 10) return std::nullopt;
        value = value * 10 + c - '0';
    }
    return value;
}
std::optional<I32> narrow(I64 v) {
    if (v < MIN_I32 || v > MAX_I32) return std::nullopt;
    return static_cast<I32>(v);
}
std::optional<I32> fold(const std::string& op, I32 a, I32 b) {
    if (op == "+") return narrow(static_cast<I64>(a) + b);
    if (op == "-") return narrow(static_cast<I64>(a) - b);
    if (op == "*") return narrow(static_cast<I64>(a) * b);
    if (op == "/" || op == "%") {
        if (b == 0 || (a == MIN_I32 && b == -1)) return std::nullopt;
        return op == "/" ? a / b : a % b;
    }
    if (op == "<") return a < b;
    if (op == ">") return a > b;
    if (op == "<=") return a <= b;
    if (op == ">=") return a >= b;
    if (op == "==") return a == b;
    if (op == "!=") return a != b;
    if (op == "&&") return a != 0 && b != 0;
    if (op == "||") return a != 0 || b != 0;
    return std::nullopt;
}
}

void SemanticAnalyzer::enterScope() { scopes_.emplace_back(); }
void SemanticAnalyzer::leaveScope() { scopes_.pop_back(); }
bool SemanticAnalyzer::declare(Symbol symbol) {
    auto& current = scopes_.back();
    if (current.count(symbol.name)) return false;
    std::string name = symbol.name;
    current.emplace(std::move(name), std::move(symbol));
    return true;
}
const Symbol* SemanticAnalyzer::lookup(const std::string& name) const {
    for (auto it = scopes_.rbegin(); it != scopes_.rend(); ++it) {
        auto found = it->find(name);
        if (found != it->end()) return &found->second;
    }
    return nullptr;
}
void SemanticAnalyzer::reportError(SourceLocation loc, const std::string& message) {
    error_lines_.push_back(loc.line ? loc.line - 1 : 0);
    std::cerr << "semantic error at " << loc.line << ':' << loc.column
              << ": " << message << '\n';
}
void SemanticAnalyzer::analyze(const Program& program, SourceLocation last_line) {
    scopes_.clear();
    error_lines_.clear();
    loop_depth_ = 0;
    saw_main_ = false;
    enterScope();
    declare({"getint", SymbolKind::Function, ValueType::Int, false, {}, {}, {}});
    declare({"putint", SymbolKind::Function, ValueType::Void, false, {}, {ValueType::Int}, {}});
    // Register only the current function before analyzing its body.
    for (const auto& fn : program.functions) analyzeFunction(*fn);
    if (!saw_main_) reportError(last_line, "missing main function");
    leaveScope();
    std::sort(error_lines_.begin(), error_lines_.end());
    error_lines_.erase(std::unique(error_lines_.begin(), error_lines_.end()), error_lines_.end());
}
void SemanticAnalyzer::analyzeFunction(const Function& fn) {
    const auto type = fn.return_type == "void" ? ValueType::Void : ValueType::Int;
    if (!declare({fn.name, SymbolKind::Function, type, false, fn.loc,
                  std::vector<ValueType>(fn.params.size(), ValueType::Int), {}})) {
        reportError(fn.loc, "duplicate function: " + fn.name);
    }
    if (fn.name == "main") {
        saw_main_ = true;
        if (type != ValueType::Int || !fn.params.empty())
            reportError(fn.loc, "main must return int and have no parameters");
    }
    return_type_ = type;
    loop_depth_ = 0;
    enterScope();
    for (const auto& p : fn.params) {
        if (!declare({p.name, SymbolKind::Parameter, ValueType::Int, false, p.loc, {}, {}}))
            reportError(p.loc, "duplicate parameter: " + p.name);
    }
    const unsigned flow = analyzeBlock(*fn.body, false); // parameters and outer body share scope
    if (type == ValueType::Int && (flow & Normal))
        reportError(fn.body->end_loc, "int function can fall through without a value");
    leaveScope();
}
void SemanticAnalyzer::analyzeDecl(const Decl& decl) {
    const bool inserted = declare({decl.name, decl.is_const ? SymbolKind::Constant : SymbolKind::Variable,
                                    ValueType::Int, decl.is_const, decl.loc, {}, {}});
    if (!inserted) reportError(decl.loc, "duplicate declaration: " + decl.name);
    if (!decl.initializer) {
        if (decl.is_const) reportError(decl.loc, "const requires initializer");
        return;
    }
    auto result = analyzeExpr(*decl.initializer);
    if (inserted && decl.is_const && result.type == ValueType::Int)
        scopes_.back().at(decl.name).constant = result.constant;
}
unsigned SemanticAnalyzer::analyzeBlock(const Block& block, bool nested) {
    if (nested) enterScope();
    unsigned flow = Normal;
    for (const auto& stmt : block.statements) {
        // Check even unreachable statements for independent semantic errors,
        // but never use unreachable returns to prove a reachable exit.
        const unsigned next = analyzeStmt(*stmt);
        if (flow & Normal) flow = (flow & ~Normal) | next;
    }
    if (nested) leaveScope();
    return flow;
}
unsigned SemanticAnalyzer::analyzeStmt(const Stmt& stmt) {
    if (const auto* d = dynamic_cast<const Decl*>(&stmt)) {
        analyzeDecl(*d);
    } else if (const auto* b = dynamic_cast<const Block*>(&stmt)) {
        return analyzeBlock(*b, true);
    } else if (const auto* a = dynamic_cast<const Assign*>(&stmt)) {
        const Symbol* sym = lookup(a->name);
        if (!sym || sym->kind == SymbolKind::Function)
            reportError(a->loc, "invalid assignment target: " + a->name);
        else if (sym->is_const)
            reportError(a->loc, "assignment to constant: " + a->name);
        analyzeExpr(*a->value);
    } else if (const auto* e = dynamic_cast<const ExprStmt*>(&stmt)) {
        // A standalone call returning void is permitted.
        analyzeExpr(*e->expr, false);
    } else if (const auto* i = dynamic_cast<const If*>(&stmt)) {
        auto cond = analyzeExpr(*i->condition);
        unsigned yes = analyzeStmt(*i->then_stmt);
        unsigned no = i->else_stmt ? analyzeStmt(*i->else_stmt) : static_cast<unsigned>(Normal);
        if (cond.constant) return *cond.constant != 0 ? yes : no;
        return yes | no;
    } else if (const auto* w = dynamic_cast<const While*>(&stmt)) {
        auto cond = analyzeExpr(*w->condition);
        ++loop_depth_;
        unsigned body = analyzeStmt(*w->body);
        --loop_depth_;
        if (cond.constant && *cond.constant == 0) return Normal;
        unsigned exits = body & Returned;
        // The loop absorbs break/continue. A known-true loop only falls
        // through via a reachable break; an unknown loop may not execute.
        if (!cond.constant || (body & Broken)) exits |= Normal;
        return exits;
    } else if (dynamic_cast<const BreakStmt*>(&stmt)) {
        if (loop_depth_) return Broken;
        reportError(stmt.loc, "break outside while");
    } else if (dynamic_cast<const ContinueStmt*>(&stmt)) {
        if (loop_depth_) return Continued;
        reportError(stmt.loc, "continue outside while");
    } else if (const auto* r = dynamic_cast<const Return*>(&stmt)) {
        if (return_type_ == ValueType::Void) {
            if (r->value) {
                analyzeExpr(*r->value, false);
                reportError(r->loc, "void function cannot return a value");
            }
        } else if (!r->value) {
            reportError(r->loc, "int function must return a value");
        } else {
            auto result = analyzeExpr(*r->value, false);
            if (result.type == ValueType::Void)
                reportError(r->loc, "int function cannot return void");
        }
        return Returned; // an invalid return is still a control-flow exit
    }
    return Normal;
}
SemanticAnalyzer::ExprResult SemanticAnalyzer::analyzeExpr(const Expr& expr,
                                                            bool need_int, bool evaluated) {
    if (const auto* n = dynamic_cast<const Number*>(&expr)) {
        auto value = magnitude(n->value);
        if (!value || *value > MAX_I32) {
            reportError(n->loc, "integer literal out of signed 32-bit range");
            return {};
        }
        return {ValueType::Int, narrow(*value)};
    }
    if (const auto* v = dynamic_cast<const Variable*>(&expr)) {
        const Symbol* sym = lookup(v->name);
        if (!sym || sym->kind == SymbolKind::Function) {
            reportError(v->loc, "invalid variable: " + v->name);
            return {};
        }
        return {sym->type, sym->constant};
    }
    if (const auto* call = dynamic_cast<const Call*>(&expr)) {
        const Symbol* sym = lookup(call->name);
        bool valid = sym && sym->kind == SymbolKind::Function;
        if (!valid) reportError(call->loc, "invalid called function: " + call->name);
        else if (sym->param_types.size() != call->args.size()) {
            reportError(call->loc, "incorrect argument count: " + call->name);
            valid = false;
        }
        bool bad_arg = false;
        for (const auto& arg : call->args) {
            auto a = analyzeExpr(*arg, false, evaluated);
            if (a.type == ValueType::Void) {
                reportError(call->loc, "argument must be int: " + call->name);
                bad_arg = true;
            } else if (a.type == ValueType::Error) bad_arg = true;
        }
        if (!valid || bad_arg) return {};
        if (need_int && sym->type == ValueType::Void) {
            reportError(call->loc, "void call used as a value: " + call->name);
            return {};
        }
        return {sym->type, {}};
    }
    if (const auto* u = dynamic_cast<const Unary*>(&expr)) {
        // A positive magnitude 2147483648 is only valid as the operand of minus.
        if (u->op == "-") {
            if (const auto* n = dynamic_cast<const Number*>(u->operand.get())) {
                if (magnitude(n->value) == std::optional<I64>(MAX_I32 + 1))
                    return {ValueType::Int, static_cast<I32>(MIN_I32)};
            }
        }
        auto a = analyzeExpr(*u->operand, true, evaluated);
        if (a.type == ValueType::Error) return {};
        if (!a.constant) return {ValueType::Int, {}};
        if (u->op == "+") return a;
        if (u->op == "!") return {ValueType::Int, static_cast<I32>(*a.constant == 0)};
        return {ValueType::Int, narrow(-static_cast<I64>(*a.constant))};
    }
    if (const auto* b = dynamic_cast<const Binary*>(&expr)) {
        auto l = analyzeExpr(*b->left, true, evaluated);
        const bool shorted = l.constant && ((b->op == "&&" && *l.constant == 0) ||
                                            (b->op == "||" && *l.constant != 0));
        auto r = analyzeExpr(*b->right, true, evaluated && !shorted);
        if (l.type == ValueType::Error || r.type == ValueType::Error) return {};
        if (evaluated && (b->op == "/" || b->op == "%") && r.constant && *r.constant == 0) {
            reportError(b->loc, "division or remainder by constant zero");
            return {};
        }
        if (shorted) return {ValueType::Int, static_cast<I32>(b->op == "||")};
        if (l.constant && r.constant) return {ValueType::Int, fold(b->op, *l.constant, *r.constant)};
        return {ValueType::Int, {}};
    }
    return {};
}
