#include <iostream>
#include <pg_query.h>
#include "planner/builder.hpp"
#include "planner/optimizer.hpp"
#include "planner/nodes.hpp"

int main() {
    const char* sql = "SELECT id, name FROM users WHERE id = 1";

    PgQueryParseResult result = pg_query_parse(sql);
    if (result.error) {
        std::cerr << "parse error: " << result.error->message << "\n";
        pg_query_free_parse_result(result);
        return 1;
    }

    std::cout << "=== Raw parse tree ===\n" << result.parse_tree << "\n\n";

    PlanPtr plan = build_plan(result.parse_tree);
    pg_query_free_parse_result(result);

    std::cout << "=== Logical plan (before optimization) ===\n";
    explain(plan);

    plan = optimize(std::move(plan));

    std::cout << "\n=== Logical plan (after optimization) ===\n";
    explain(plan);

    return 0;
}
