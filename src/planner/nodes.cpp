#include "nodes.hpp"
#include <iostream>

PlanPtr make_scan(std::string table) {
    return std::make_shared<PlanNode>(PlanNode{SeqScan{std::move(table)}});
}
PlanPtr make_filter(PlanPtr child, ExprPtr pred) {
    return std::make_shared<PlanNode>(PlanNode{Filter{std::move(child), std::move(pred)}});
}
PlanPtr make_project(PlanPtr child, std::vector<ProjectedCol> cols, bool star) {
    return std::make_shared<PlanNode>(PlanNode{Project{std::move(child), std::move(cols), star}});
}
PlanPtr make_join(Join::Kind kind, PlanPtr l, PlanPtr r, ExprPtr cond) {
    return std::make_shared<PlanNode>(PlanNode{Join{kind, std::move(l), std::move(r), std::move(cond)}});
}
PlanPtr make_limit(PlanPtr child, int64_t count, int64_t offset) {
    return std::make_shared<PlanNode>(PlanNode{Limit{std::move(child), count, offset}});
}

void explain(const PlanPtr& node, int indent) {
    if (!node) return;
    std::string pad(indent * 2, ' ');
    std::visit([&](auto&& n) {
        using T = std::decay_t<decltype(n)>;
        if constexpr (std::is_same_v<T, SeqScan>) {
            std::cout << pad << "SeqScan(" << n.table << ")\n";
        } else if constexpr (std::is_same_v<T, Filter>) {
            std::cout << pad << "Filter(" << expr_to_str(n.predicate) << ")\n";
            explain(n.child, indent + 1);
        } else if constexpr (std::is_same_v<T, Project>) {
            if (n.star) {
                std::cout << pad << "Project(*)\n";
            } else {
                std::cout << pad << "Project(";
                for (size_t i = 0; i < n.cols.size(); ++i) {
                    std::cout << expr_to_str(n.cols[i].expr);
                    if (!n.cols[i].alias.empty()) std::cout << " AS " << n.cols[i].alias;
                    if (i + 1 < n.cols.size()) std::cout << ", ";
                }
                std::cout << ")\n";
            }
            explain(n.child, indent + 1);
        } else if constexpr (std::is_same_v<T, Join>) {
            static const char* kinds[] = {"INNER","LEFT","RIGHT","FULL","CROSS"};
            std::cout << pad << kinds[(int)n.kind] << " JOIN";
            if (n.condition) std::cout << " ON " << expr_to_str(n.condition);
            std::cout << "\n";
            explain(n.left,  indent + 1);
            explain(n.right, indent + 1);
        } else if constexpr (std::is_same_v<T, Limit>) {
            std::cout << pad << "Limit(count=" << n.count << " offset=" << n.offset << ")\n";
            explain(n.child, indent + 1);
        }
    }, node->node);
}
