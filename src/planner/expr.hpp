#pragma once
#include <memory>
#include <string>
#include <variant>

struct Expr;
using ExprPtr = std::shared_ptr<Expr>;

struct ColumnRef {
    std::string table;   // empty if unqualified
    std::string column;
};

struct Literal {
    enum class Kind { Int, Float, String, Null } kind;
    std::variant<int64_t, double, std::string> value;
};

struct BinaryOp {
    enum class Op {
        Eq, Ne, Lt, Le, Gt, Ge,
        And, Or,
        Add, Sub, Mul, Div
    } op;
    ExprPtr left;
    ExprPtr right;
};

struct UnaryOp {
    enum class Op { Not, Neg } op;
    ExprPtr operand;
};

struct Expr {
    std::variant<ColumnRef, Literal, BinaryOp, UnaryOp> node;
};

ExprPtr make_col(std::string column, std::string table = "");
ExprPtr make_lit_int(int64_t v);
ExprPtr make_lit_float(double v);
ExprPtr make_lit_str(std::string v);
ExprPtr make_lit_null();
ExprPtr make_binop(BinaryOp::Op op, ExprPtr l, ExprPtr r);
ExprPtr make_unop(UnaryOp::Op op, ExprPtr operand);

std::string expr_to_str(const ExprPtr& e);
