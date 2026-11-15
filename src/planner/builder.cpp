#include "builder.hpp"
#include "../../third_party/json.hpp"
#include <stdexcept>

using json = nlohmann::json;

// ---------------------------------------------------------------------------
// Forward declarations
// ---------------------------------------------------------------------------
static ExprPtr build_expr(const json& node);
static PlanPtr build_select(const json& stmt);
static PlanPtr build_from(const json& from_clause);

// ---------------------------------------------------------------------------
// Expression builder
// ---------------------------------------------------------------------------

static BinaryOp::Op parse_op(const std::string& s) {
    if (s == "=")  return BinaryOp::Op::Eq;
    if (s == "<>") return BinaryOp::Op::Ne;
    if (s == "<")  return BinaryOp::Op::Lt;
    if (s == "<=") return BinaryOp::Op::Le;
    if (s == ">")  return BinaryOp::Op::Gt;
    if (s == ">=") return BinaryOp::Op::Ge;
    if (s == "+")  return BinaryOp::Op::Add;
    if (s == "-")  return BinaryOp::Op::Sub;
    if (s == "*")  return BinaryOp::Op::Mul;
    if (s == "/")  return BinaryOp::Op::Div;
    throw std::runtime_error("unsupported operator: " + s);
}

static ExprPtr build_expr(const json& node) {
    if (node.contains("ColumnRef")) {
        const auto& fields = node["ColumnRef"]["fields"];
        if (fields.size() == 1) {
            return make_col(fields[0]["String"]["sval"].get<std::string>());
        }
        // table.column
        return make_col(
            fields[1]["String"]["sval"].get<std::string>(),
            fields[0]["String"]["sval"].get<std::string>()
        );
    }

    if (node.contains("A_Const")) {
        const auto& c = node["A_Const"];
        if (c.contains("ival"))  return make_lit_int(c["ival"]["ival"].get<int64_t>());
        if (c.contains("fval"))  return make_lit_float(std::stod(c["fval"]["fval"].get<std::string>()));
        if (c.contains("sval"))  return make_lit_str(c["sval"]["sval"].get<std::string>());
        if (c.contains("isnull")) return make_lit_null();
        return make_lit_null();
    }

    if (node.contains("A_Expr")) {
        const auto& e = node["A_Expr"];
        const std::string op_name = e["name"][0]["String"]["sval"].get<std::string>();
        ExprPtr l = build_expr(e["lexpr"]);
        ExprPtr r = build_expr(e["rexpr"]);
        return make_binop(parse_op(op_name), std::move(l), std::move(r));
    }

    if (node.contains("BoolExpr")) {
        const auto& b = node["BoolExpr"];
        const std::string kind = b["boolop"].get<std::string>();
        const auto& args = b["args"];

        if (kind == "AND_EXPR" || kind == "OR_EXPR") {
            BinaryOp::Op op = (kind == "AND_EXPR") ? BinaryOp::Op::And : BinaryOp::Op::Or;
            ExprPtr acc = build_expr(args[0]);
            for (size_t i = 1; i < args.size(); ++i)
                acc = make_binop(op, acc, build_expr(args[i]));
            return acc;
        }
        if (kind == "NOT_EXPR") {
            return make_unop(UnaryOp::Op::Not, build_expr(args[0]));
        }
    }

    throw std::runtime_error("unsupported expression node: " + node.dump());
}

// ---------------------------------------------------------------------------
// FROM clause builder (handles single tables and cross joins)
// ---------------------------------------------------------------------------

static PlanPtr build_from_item(const json& item) {
    if (item.contains("RangeVar")) {
        return make_scan(item["RangeVar"]["relname"].get<std::string>());
    }
    if (item.contains("JoinExpr")) {
        const auto& j = item["JoinExpr"];
        int jtype = j["jointype"].get<int>();
        Join::Kind kind;
        switch (jtype) {
            case 0: kind = Join::Kind::Inner; break;
            case 1: kind = Join::Kind::Left;  break;
            case 2: kind = Join::Kind::Right; break;
            case 3: kind = Join::Kind::Full;  break;
            default: kind = Join::Kind::Inner;
        }
        ExprPtr cond = j.contains("quals") ? build_expr(j["quals"]) : nullptr;
        return make_join(kind, build_from_item(j["larg"]), build_from_item(j["rarg"]), std::move(cond));
    }
    throw std::runtime_error("unsupported FROM item: " + item.dump());
}

static PlanPtr build_from(const json& from_clause) {
    if (from_clause.empty())
        throw std::runtime_error("SELECT with no FROM clause");

    PlanPtr root = build_from_item(from_clause[0]);
    // Multiple items in FROM = implicit cross join
    for (size_t i = 1; i < from_clause.size(); ++i)
        root = make_join(Join::Kind::Cross, std::move(root), build_from_item(from_clause[i]));
    return root;
}

// ---------------------------------------------------------------------------
// SELECT statement builder
// ---------------------------------------------------------------------------

static PlanPtr build_select(const json& stmt) {
    PlanPtr plan = build_from(stmt["fromClause"]);

    // WHERE
    if (stmt.contains("whereClause")) {
        ExprPtr pred = build_expr(stmt["whereClause"]);
        plan = make_filter(std::move(plan), std::move(pred));
    }

    // SELECT list
    const auto& targets = stmt["targetList"];
    bool star = false;
    std::vector<ProjectedCol> cols;

    for (const auto& t : targets) {
        const auto& res = t["ResTarget"];
        const auto& val = res["val"];
        if (val.contains("ColumnRef")) {
            const auto& fields = val["ColumnRef"]["fields"];
            if (fields.size() == 1 && fields[0].contains("A_Star")) {
                star = true;
                break;
            }
        }
        std::string alias = res.contains("name") ? res["name"].get<std::string>() : "";
        cols.push_back({build_expr(val), std::move(alias)});
    }

    plan = make_project(std::move(plan), std::move(cols), star);

    // LIMIT / OFFSET
    if (stmt.contains("limitCount")) {
        int64_t count = stmt["limitCount"]["A_Const"]["ival"]["ival"].get<int64_t>();
        int64_t offset = 0;
        if (stmt.contains("limitOffset"))
            offset = stmt["limitOffset"]["A_Const"]["ival"]["ival"].get<int64_t>();
        plan = make_limit(std::move(plan), count, offset);
    }

    return plan;
}

// ---------------------------------------------------------------------------
// Public entry point
// ---------------------------------------------------------------------------

PlanPtr build_plan(const std::string& pg_query_json) {
    json root = json::parse(pg_query_json);
    const auto& stmt = root["stmts"][0]["stmt"];

    if (stmt.contains("SelectStmt"))
        return build_select(stmt["SelectStmt"]);

    throw std::runtime_error("only SELECT is supported");
}
