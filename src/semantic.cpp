#include "semantic.h"
#include <algorithm>
#include <set>
#include <utility>

namespace robotlang::extension {
namespace {
const char* commandName(Command::Kind kind) {
    switch (kind) {
        case Command::Kind::Forward: return "forward";
        case Command::Kind::Left: return "left";
        case Command::Kind::Right: return "right";
        case Command::Kind::Repeat: return "repeat";
    }
    return "";
}
bool contains(const Wall& wall, Integer x, Integer y) {
    return x >= wall.x.value && x - wall.x.value < wall.width.value &&
           y >= wall.y.value && y - wall.y.value < wall.height.value;
}
void printCommands(const std::vector<Command>& commands, std::ostream& out, int depth) {
    for (const auto& c : commands) {
        out << std::string(static_cast<std::size_t>(depth) * 2, ' ') << '#' << c.id << ' '
            << commandName(c.kind) << ' ' << c.argument.value << " @" << c.source.line << ':' << c.source.col;
        if (c.kind == Command::Kind::Repeat) {
            out << " {\n";
            printCommands(c.body, out, depth + 1);
            out << std::string(static_cast<std::size_t>(depth) * 2, ' ') << "}\n";
        } else out << '\n';
    }
}
}

AnalysisResult SemanticAnalyzer::analyze(Program program) const {
    std::vector<Diagnostic> errors;
    auto report = [&](SourceLocation where, const char* code, const char* message) {
        errors.push_back({where, code, message});
    };
    if (program.width.value == 0) report(program.width.source, "non_positive_value", "ancho debe ser positivo");
    if (program.height.value == 0) report(program.height.source, "non_positive_value", "alto debe ser positivo");
    if (program.robots.empty()) report(program.end, "missing_robot", "se requiere al menos un robot");
    for (const auto& wall : program.walls) {
        if (wall.width.value == 0 || wall.height.value == 0)
            report(wall.width.value == 0 ? wall.width.source : wall.height.source, "invalid_wall", "las dimensiones del muro deben ser positivas");
        if (wall.x.value >= program.width.value || wall.y.value >= program.height.value ||
            wall.width.value > program.width.value - std::min(wall.x.value, program.width.value) ||
            wall.height.value > program.height.value - std::min(wall.y.value, program.height.value))
            report(wall.x.source, "terrain_out_of_bounds", "muro fuera del mundo");
    }
    for (const auto& hole : program.holes) {
        if (hole.x.value >= program.width.value || hole.y.value >= program.height.value)
            report(hole.x.source, "terrain_out_of_bounds", "hueco fuera del mundo");
        for (const auto& wall : program.walls) if (contains(wall, hole.x.value, hole.y.value)) {
            report(hole.x.source, "hole_overlaps_solid", "hueco sobre terreno solido");
            break;
        }
    }
    std::set<std::string> ids;
    std::set<std::pair<Integer, Integer>> cells;
    std::set<Integer> priorities;
    const Integer count = static_cast<Integer>(program.robots.size());
    for (auto& robot : program.robots) {
        if (!ids.insert(robot.id).second) report(robot.idSource, "duplicate_robot_id", "robot duplicado");
        if (!cells.insert({robot.x.value, robot.y.value}).second) report(robot.x.source, "duplicate_start_cell", "celda inicial ocupada");
        if (robot.x.value >= program.width.value || robot.y.value >= program.height.value)
            report(robot.x.value >= program.width.value ? robot.x.source : robot.y.source, "initial_position_out_of_bounds", "robot fuera del mundo");
        bool terrain = false;
        for (const auto& wall : program.walls) terrain = terrain || contains(wall, robot.x.value, robot.y.value);
        for (const auto& hole : program.holes) terrain = terrain || (hole.x.value == robot.x.value && hole.y.value == robot.y.value);
        if (terrain) report(robot.x.source, "robot_on_terrain", "robot sobre terreno");
        if (robot.explicitPriority) {
            Integer p = robot.explicitPriority->value;
            if (p < 1 || p > count) report(robot.explicitPriority->source, "priority_out_of_range", "prioridad fuera de rango");
            else if (!priorities.insert(p).second) report(robot.explicitPriority->source, "duplicate_priority", "prioridad duplicada");
            robot.priority = p;
        }
    }
    Integer next = 1;
    for (auto& robot : program.robots) if (!robot.explicitPriority) {
        while (priorities.count(next)) ++next;
        robot.priority = next;
        priorities.insert(next++);
    }
    std::set<std::string> seenRuns;
    std::uint64_t nextId = 1;
    auto validate = [&](auto&& self, std::vector<Command>& commands) -> void {
        for (auto& command : commands) {
            command.id = nextId++;
            if (command.kind == Command::Kind::Forward || command.kind == Command::Kind::Repeat) {
                if (command.argument.value == 0) report(command.argument.source, "non_positive_value", "el valor debe ser positivo");
            } else if (command.argument.value != 90 && command.argument.value != 180 && command.argument.value != 270)
                report(command.argument.source, "invalid_angle", "el angulo debe ser 90, 180 o 270");
            self(self, command.body);
        }
    };
    for (auto& run : program.runs) {
        validate(validate, run.commands);
        auto robot = std::find_if(program.robots.begin(), program.robots.end(), [&](const Robot& r) { return r.id == run.id; });
        if (robot == program.robots.end()) report(run.idSource, "unknown_run", "robot no declarado");
        else if (!seenRuns.insert(run.id).second) report(run.idSource, "duplicate_run", "run duplicado");
        else robot->commands = run.commands;
    }
    if (!errors.empty()) return {std::nullopt, std::move(errors)};
    program.runs.clear();
    return {std::move(program), {}};
}

void printIr(const Program& program, std::ostream& out) {
    out << "RobotIR v2\nWorld " << program.width.value << ' ' << program.height.value << '\n';
    for (const auto& wall : program.walls) out << "Wall " << wall.x.value << ' ' << wall.y.value << ' ' << wall.width.value << ' ' << wall.height.value << '\n';
    for (const auto& hole : program.holes) out << "Hole " << hole.x.value << ' ' << hole.y.value << '\n';
    for (const auto& robot : program.robots) {
        out << "Robot " << robot.id << ' ' << robot.x.value << ' ' << robot.y.value << ' ' << directionName(robot.facing) << " priority=" << robot.priority << " {\n";
        printCommands(robot.commands, out, 1);
        out << "}\n";
    }
}
}
