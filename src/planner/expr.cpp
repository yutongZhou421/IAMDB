#include "expr.hpp"
#include <stdexcept>

ExprPtr make_col(std::string column, std::string table) {
    return std::make_shared<Expr>(Expr{ColumnRef{std::move(table), std::move(column)}});
}
ExprPtr make_lit_int(int64_t v) {
    return std::make_shared<Expr>(Expr{Literal{Literal::Kind::Int, v}});
}
ExprPtr make_lit_float(double v) {
    return std::make_shared<Expr>(Expr{Literal{Literal::Kind::Float, v}});
}
ExprPtr make_lit_str(std::string v) {
    return std::make_shared<Expr>(Expr{Literal{Literal::Kind::String, std::move(v)}});
}
ExprPtr make_lit_null() {
    return std::make_shared<Expr>(Expr{Literal{Literal::Kind::Null, int64_t(0)}});
}
ExprPtr make_binop(BinaryOp::Op op, ExprPtr l, ExprPtr r) {
    return std::make_shared<Expr>(Expr{BinaryOp{op, std::move(l), std::move(r)}});
}
ExprPtr make_unop(UnaryOp::Op op, ExprPtr operand) {
    return std::make_shared<Expr>(Expr{UnaryOp{op, std::move(operand)}});
}

static const char* op_str(BinaryOp::Op o) {
    switch (o) {
        case BinaryOp::Op::Eq:  return "=";   case BinaryOp::Op::Ne:  return "<>";
        case BinaryOp::Op::Lt:  return "<";   case BinaryOp::Op::Le:  return "<=";
        case BinaryOp::Op::Gt:  return ">";   case BinaryOp::Op::Ge:  return ">=";
        case BinaryOp::Op::And: return "AND"; case BinaryOp::Op::Or:  return "OR";
        case BinaryOp::Op::Add: return "+";   case BinaryOp::Op::Sub: return "-";
        case BinaryOp::Op::Mul: return "*";   case BinaryOp::Op::Div: return "/";
    }
    return "?";
}

std::string expr_to_str(const ExprPtr& e) {
    if (!e) return "null";
    return std::visit([](auto&& n) -> std::string {
        using T = std::decay_t<decltype(n)>;
        if constexpr (std::is_same_v<T, ColumnRef>) {
            return n.table.empty() ? n.column : n.table + "." + n.column;
        } else if constexpr (std::is_same_v<T, Literal>) {
            switch (n.kind) {
                case Literal::Kind::Int:    return std::to_string(std::get<int64_t>(n.value));
                case Literal::Kind::Float:  return std::to_string(std::get<double>(n.value));
                case Literal::Kind::String: return "'" + std::get<std::string>(n.value) + "'";
                case Literal::Kind::Null:   return "NULL";
            }
        } else if constexpr (std::is_same_v<T, BinaryOp>) {
            return "(" + expr_to_str(n.left) + " " + op_str(n.op) + " " + expr_to_str(n.right) + ")";
        } else if constexpr (std::is_same_v<T, UnaryOp>) {
            return n.op == UnaryOp::Op::Not
                ? "NOT " + expr_to_str(n.operand)
                : "-"   + expr_to_str(n.operand);
        }
        return "?";
    }, e->node);
}
