#ifndef ROBOTLANG_OPTIMIZER_H
#define ROBOTLANG_OPTIMIZER_H

#include "simulator.h"

namespace robotlang::extension {
struct OptimizerOptions {
    bool mergeMoves = true;
    bool simplifyTurns = true;
};

struct Rewrite {
    std::string rule;
    std::vector<std::uint64_t> inputs;
    std::optional<std::uint64_t> output;
};

struct OptimizationResult {
    Program program;
    std::vector<Rewrite> rewrites;
};

class Optimizer {
public:
    explicit Optimizer(Integer tickLimit = 100000, OptimizerOptions options = {});
    OptimizationResult optimize(const Program& program) const;
private:
    Integer tickLimit_;
    OptimizerOptions options_;
};
}
#endif
