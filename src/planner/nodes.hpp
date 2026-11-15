#pragma once
#include "expr.hpp"
#include <memory>
#include <string>
#include <vector>

struct PlanNode;
using PlanPtr = std::shared_ptr<PlanNode>;

struct ProjectedCol {
    ExprPtr expr;
    std::string alias;
};

struct SeqScan {
    std::string table;
};

struct Filter {
    PlanPtr child;
    ExprPtr predicate;
};

struct Project {
    PlanPtr child;
    std::vector<ProjectedCol> cols;
    bool star = false;
};

struct Join {
    enum class Kind { Inner, Left, Right, Full, Cross } kind;
    PlanPtr left;
    PlanPtr right;
    ExprPtr condition;
};

struct Limit {
    PlanPtr child;
    int64_t count  = -1;
    int64_t offset =  0;
};

struct PlanNode {
    std::variant<SeqScan, Filter, Project, Join, Limit> node;
};

PlanPtr make_scan(std::string table);
PlanPtr make_filter(PlanPtr child, ExprPtr pred);
PlanPtr make_project(PlanPtr child, std::vector<ProjectedCol> cols, bool star = false);
PlanPtr make_join(Join::Kind kind, PlanPtr l, PlanPtr r, ExprPtr cond = nullptr);
PlanPtr make_limit(PlanPtr child, int64_t count, int64_t offset = 0);

void explain(const PlanPtr& node, int indent = 0);
