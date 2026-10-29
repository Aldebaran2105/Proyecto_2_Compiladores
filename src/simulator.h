#ifndef ROBOTLANG_SIMULATOR_H
#define ROBOTLANG_SIMULATOR_H

#include "ir.h"
#include <string>
#include <vector>

namespace robotlang::extension {

enum class RobotStatus { Active, Finished, Fallen };

struct RobotState {
    std::string id;
    Integer priority;
    Integer x;
    Integer y;
    Direction facing;
    RobotStatus status;
};

struct Event {
    Integer tick;
    std::string robot;
    std::string text;
};

struct SimulationResult {
    std::vector<RobotState> robots;
    std::vector<Event> events;
    Integer ticks = 0;
    bool limitReached = false;
};

class Simulator {
public:
    explicit Simulator(const Program& program, Integer tickLimit = 100000);
    SimulationResult run(bool trace = false) const;

private:
    const Program& program_;
    Integer tickLimit_;
};

void printResult(const SimulationResult& result, std::ostream& out);
void printGrid(const Program& program, const SimulationResult& result, std::ostream& out);
void printTrace(const SimulationResult& result, std::ostream& out);

}
#endif
