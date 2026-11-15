#pragma once
#include "nodes.hpp"
#include <string>

// Build a logical plan from the JSON string that pg_query_parse() returns.
// Throws std::runtime_error on unsupported or malformed input.
PlanPtr build_plan(const std::string& pg_query_json);
