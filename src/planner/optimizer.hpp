#pragma once
#include "nodes.hpp"

// Apply rule-based rewrites to a logical plan tree.
// Returns the (possibly new) root of the rewritten tree.
PlanPtr optimize(PlanPtr root);
