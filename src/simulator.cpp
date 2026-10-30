#include "simulator.h"
#include <algorithm>
#include <numeric>
#include <sstream>

namespace robotlang::extension {
namespace {
struct Frame {
    const std::vector<Command>* body;
    std::size_t index;
    Integer remaining;
};

bool hasAction(const std::vector<Command>& commands) {
    for (const auto& command : commands)
        if (command.kind != Command::Kind::Repeat || hasAction(command.body)) return true;
    return false;
}

const Command* next(std::vector<Frame>& stack) {
    while (!stack.empty()) {
        auto& frame = stack.back();
        if (frame.index == frame.body->size()) {
            if (--frame.remaining > 0) frame.index = 0;
            else stack.pop_back();
            continue;
        }
        const Command& command = (*frame.body)[frame.index++];
        if (command.kind != Command::Kind::Repeat) return &command;
        if (hasAction(command.body)) stack.push_back({&command.body, 0, command.argument.value});
    }
    return nullptr;
}

bool solid(const Program& program, Integer x, Integer y) {
    for (const auto& w : program.walls)
        if (x >= w.x.value && x - w.x.value < w.width.value && y >= w.y.value && y - w.y.value < w.height.value) return true;
    return false;
}

bool hole(const Program& program, Integer x, Integer y) {
    for (const auto& h : program.holes) if (x == h.x.value && y == h.y.value) return true;
    return false;
}

int index(Direction d) {
    switch (d) {
        case Direction::North: return 0;
        case Direction::East: return 1;
        case Direction::South: return 2;
        case Direction::West: return 3;
    }
    return 0;
}

Direction direction(int n) {
    constexpr Direction directions[] = {Direction::North, Direction::East, Direction::South, Direction::West};
    return directions[n & 3];
}
}

Simulator::Simulator(const Program& program, Integer tickLimit) : program_(program), tickLimit_(tickLimit) {}

SimulationResult Simulator::run(bool trace) const {
    SimulationResult result;
    std::vector<std::vector<Frame>> stacks;
    std::vector<std::size_t> order(program_.robots.size());
    std::iota(order.begin(), order.end(), 0);
    std::sort(order.begin(), order.end(), [&](std::size_t a, std::size_t b) { return program_.robots[a].priority < program_.robots[b].priority; });
    for (const auto& robot : program_.robots) {
        result.robots.push_back({robot.id, robot.priority, robot.x.value, robot.y.value, robot.facing,
                                 robot.commands.empty() ? RobotStatus::Finished : RobotStatus::Active});
        stacks.push_back({Frame{&robot.commands, 0, 1}});
    }
    while (std::any_of(result.robots.begin(), result.robots.end(), [](const RobotState& r) { return r.status == RobotStatus::Active; })) {
        if (result.ticks == tickLimit_) { result.limitReached = true; break; }
        ++result.ticks;
        for (std::size_t i : order) {
            auto& state = result.robots[i];
            if (state.status != RobotStatus::Active) continue;
            const Command* cmd = next(stacks[i]);
            if (!cmd) { state.status = RobotStatus::Finished; continue; }
            std::ostringstream event;
            if (cmd->kind == Command::Kind::Forward) {
                Integer x0 = state.x, y0 = state.y;
                bool blocked = false;
                for (Integer step = 0; step < cmd->argument.value; ++step) {
                    Integer x = state.x, y = state.y;
                    switch (state.facing) {
                        case Direction::North: ++y; break;
                        case Direction::East: ++x; break;
                        case Direction::South: --y; break;
                        case Direction::West: --x; break;
                    }
                    bool occupied = false;
                    for (std::size_t j = 0; j < result.robots.size(); ++j)
                        if (i != j && result.robots[j].status != RobotStatus::Fallen && result.robots[j].x == x && result.robots[j].y == y) occupied = true;
                    if (x < 0 || y < 0 || x >= program_.width.value || y >= program_.height.value || solid(program_, x, y) || occupied) { blocked = true; break; }
                    state.x = x; state.y = y;
                    if (hole(program_, x, y)) { state.status = RobotStatus::Fallen; break; }
                }
                event << "forward " << cmd->argument.value << " (" << x0 << ',' << y0 << ")->(" << state.x << ',' << state.y << ')';
                if (blocked) event << " blocked";
                if (state.status == RobotStatus::Fallen) event << " fallen";
            } else {
                int delta = static_cast<int>(cmd->argument.value / 90);
                if (cmd->kind == Command::Kind::Left) delta = -delta;
                state.facing = direction(index(state.facing) + delta);
                event << (cmd->kind == Command::Kind::Left ? "left " : "right ") << cmd->argument.value << " facing " << directionName(state.facing);
            }
            if (trace) result.events.push_back({result.ticks, state.id, event.str()});
        }
    }
    return result;
}

void printResult(const SimulationResult& result, std::ostream& out) {
    for (const auto& robot : result.robots) {
        out << robot.id << ": ";
        if (robot.status == RobotStatus::Fallen) out << "fallen at (" << robot.x << ", " << robot.y << ")\n";
        else out << '(' << robot.x << ", " << robot.y << ") " << directionName(robot.facing) << '\n';
    }
}

void printGrid(const Program& program, const SimulationResult& result, std::ostream& out) {
    for (Integer y = program.height.value; y-- > 0;) {
        out << y << ' ';
        for (Integer x = 0; x < program.width.value; ++x) {
            char c = solid(program, x, y) ? '#' : hole(program, x, y) ? 'O' : '.';
            for (const auto& robot : result.robots) if (robot.status != RobotStatus::Fallen && robot.x == x && robot.y == y) c = 'R';
            out << c;
        }
        out << '\n';
    }
    printResult(result, out);
}

void printTrace(const SimulationResult& result, std::ostream& out) {
    Integer tick = 0;
    for (const auto& event : result.events) {
        if (tick != event.tick) { tick = event.tick; out << "tick " << tick << '\n'; }
        out << "  " << event.robot << ' ' << event.text << '\n';
    }
}
}
