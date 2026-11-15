#include "optimizer.hpp"

// ---------------------------------------------------------------------------
// Rule: predicate pushdown
//
// Before:  Project → Filter(pred) → Scan
// After:   Project → Scan  (filter moved into scan, or kept as Filter below Project)
//
// For now, we push Filter below Project so it runs before projection.
// A later pass can push it into SeqScan to become a pushed-down predicate.
// ---------------------------------------------------------------------------

static PlanPtr push_filters_down(PlanPtr node);

static PlanPtr push_filters_down(PlanPtr node) {
    if (!node) return node;

    return std::visit([&](auto&& n) -> PlanPtr {
        using T = std::decay_t<decltype(n)>;

        // Project(Filter(child)) → Filter(Project(child))  when columns allow it.
        // Simple version: always push filter below project.
        if constexpr (std::is_same_v<T, Project>) {
            if (n.child) {
                if (auto* f = std::get_if<Filter>(&n.child->node)) {
                    // Pull the filter out, recurse on its child, rebuild below project
                    PlanPtr inner = push_filters_down(f->child);
                    PlanPtr new_project = make_project(inner, n.cols, n.star);
                    return make_filter(std::move(new_project), f->predicate);
                }
            }
            PlanPtr child = push_filters_down(n.child);
            return make_project(std::move(child), n.cols, n.star);
        }

        if constexpr (std::is_same_v<T, Filter>) {
            PlanPtr child = push_filters_down(n.child);
            return make_filter(std::move(child), n.predicate);
        }

        if constexpr (std::is_same_v<T, Join>) {
            PlanPtr l = push_filters_down(n.left);
            PlanPtr r = push_filters_down(n.right);
            return make_join(n.kind, std::move(l), std::move(r), n.condition);
        }

        if constexpr (std::is_same_v<T, Limit>) {
            PlanPtr child = push_filters_down(n.child);
            return make_limit(std::move(child), n.count, n.offset);
        }

        // SeqScan — leaf, nothing to push into
        return node;
    }, node->node);
}

// ---------------------------------------------------------------------------
// Rule: constant folding
//
// Evaluates binary expressions whose both sides are literals at plan time.
// e.g.  (1 + 1) = 2  →  2 = 2  →  true (represented as 1 = 1)
// ---------------------------------------------------------------------------

static ExprPtr fold_constants(ExprPtr e) {
    if (!e) return e;

    return std::visit([&](auto&& n) -> ExprPtr {
        using T = std::decay_t<decltype(n)>;

        if constexpr (std::is_same_v<T, BinaryOp>) {
            ExprPtr l = fold_constants(n.left);
            ExprPtr r = fold_constants(n.right);

            auto* ll = std::get_if<Literal>(&l->node);
            auto* rl = std::get_if<Literal>(&r->node);

            if (ll && rl &&
                ll->kind == Literal::Kind::Int &&
                rl->kind == Literal::Kind::Int) {
                int64_t lv = std::get<int64_t>(ll->value);
                int64_t rv = std::get<int64_t>(rl->value);
                switch (n.op) {
                    case BinaryOp::Op::Add: return make_lit_int(lv + rv);
                    case BinaryOp::Op::Sub: return make_lit_int(lv - rv);
                    case BinaryOp::Op::Mul: return make_lit_int(lv * rv);
                    case BinaryOp::Op::Div: return rv != 0 ? make_lit_int(lv / rv) : e;
                    default: break;
                }
            }

            return make_binop(n.op, std::move(l), std::move(r));
        }

        if constexpr (std::is_same_v<T, UnaryOp>) {
            ExprPtr operand = fold_constants(n.operand);
            if (n.op == UnaryOp::Op::Neg) {
                if (auto* lit = std::get_if<Literal>(&operand->node)) {
                    if (lit->kind == Literal::Kind::Int)
                        return make_lit_int(-std::get<int64_t>(lit->value));
                }
            }
            return make_unop(n.op, std::move(operand));
        }

        return e;
    }, e->node);
}

static PlanPtr fold_plan_constants(PlanPtr node) {
    if (!node) return node;

    return std::visit([&](auto&& n) -> PlanPtr {
        using T = std::decay_t<decltype(n)>;

        if constexpr (std::is_same_v<T, Filter>) {
            ExprPtr pred = fold_constants(n.predicate);
            PlanPtr child = fold_plan_constants(n.child);
            return make_filter(std::move(child), std::move(pred));
        }
        if constexpr (std::is_same_v<T, Project>) {
            std::vector<ProjectedCol> cols;
            for (auto& c : n.cols)
                cols.push_back({fold_constants(c.expr), c.alias});
            PlanPtr child = fold_plan_constants(n.child);
            return make_project(std::move(child), std::move(cols), n.star);
        }
        if constexpr (std::is_same_v<T, Join>) {
            ExprPtr cond = fold_constants(n.condition);
            PlanPtr l = fold_plan_constants(n.left);
            PlanPtr r = fold_plan_constants(n.right);
            return make_join(n.kind, std::move(l), std::move(r), std::move(cond));
        }
        if constexpr (std::is_same_v<T, Limit>) {
            return make_limit(fold_plan_constants(n.child), n.count, n.offset);
        }
        return node;
    }, node->node);
}

// ---------------------------------------------------------------------------
// Public entry point — applies all passes in order
// ---------------------------------------------------------------------------

PlanPtr optimize(PlanPtr root) {
    root = fold_plan_constants(std::move(root));
    root = push_filters_down(std::move(root));
    return root;
}
