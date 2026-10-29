#include "optimizer.h"

namespace robotlang::extension {
namespace {
bool transform(std::vector<Command>& commands, std::vector<Rewrite>& rewrites, const OptimizerOptions& options) {
    bool changed = false;
    for (auto& command : commands) if (command.kind == Command::Kind::Repeat)
        changed = transform(command.body, rewrites, options) || changed;
    for (std::size_t i = 0; i < commands.size();) {
        if (commands[i].kind == Command::Kind::Repeat && commands[i].body.empty()) {
            rewrites.push_back({"drop_empty_repeat", {commands[i].id}, std::nullopt});
            commands.erase(commands.begin() + static_cast<std::ptrdiff_t>(i));
            changed = true;
            continue;
        }
        if (i + 1 == commands.size()) break;
        auto& a = commands[i];
        auto& b = commands[i + 1];
        if (options.mergeMoves && a.kind == Command::Kind::Forward && b.kind == Command::Kind::Forward && a.argument.value <= MaxInteger - b.argument.value) {
            rewrites.push_back({"merge_moves", {a.id, b.id}, a.id});
            a.argument.value += b.argument.value;
            commands.erase(commands.begin() + static_cast<std::ptrdiff_t>(i + 1));
            changed = true;
            continue;
        }
        bool firstTurn = a.kind == Command::Kind::Left || a.kind == Command::Kind::Right;
        bool secondTurn = b.kind == Command::Kind::Left || b.kind == Command::Kind::Right;
        if (options.simplifyTurns && firstTurn && secondTurn) {
            Integer angle = (a.kind == Command::Kind::Right ? a.argument.value : -a.argument.value) +
                            (b.kind == Command::Kind::Right ? b.argument.value : -b.argument.value);
            angle = (angle % 360 + 360) % 360;
            if (angle == 0) {
                rewrites.push_back({"simplify_turns", {a.id, b.id}, std::nullopt});
                commands.erase(commands.begin() + static_cast<std::ptrdiff_t>(i), commands.begin() + static_cast<std::ptrdiff_t>(i + 2));
            } else {
                rewrites.push_back({"simplify_turns", {a.id, b.id}, a.id});
                a.kind = Command::Kind::Right;
                a.argument.value = angle;
                commands.erase(commands.begin() + static_cast<std::ptrdiff_t>(i + 1));
            }
            changed = true;
            continue;
        }
        ++i;
    }
    return changed;
}

bool equivalent(const SimulationResult& a, const SimulationResult& b) {
    if (a.limitReached || b.limitReached || a.robots.size() != b.robots.size()) return false;
    for (std::size_t i = 0; i < a.robots.size(); ++i) {
        const auto& x = a.robots[i];
        const auto& y = b.robots[i];
        if (x.id != y.id || x.x != y.x || x.y != y.y || x.facing != y.facing || x.status != y.status) return false;
    }
    return true;
}
}

Optimizer::Optimizer(Integer tickLimit, OptimizerOptions options) : tickLimit_(tickLimit), options_(options) {}

OptimizationResult Optimizer::optimize(const Program& program) const {
    OptimizationResult result{program, {}};
    for (auto& robot : result.program.robots) while (transform(robot.commands, result.rewrites, options_)) {}
    if (!result.rewrites.empty() && !equivalent(Simulator(program, tickLimit_).run(), Simulator(result.program, tickLimit_).run()))
        return {program, {}};
    return result;
}
}
