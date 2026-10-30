#ifndef ROBOTLANG_IR_H
#define ROBOTLANG_IR_H

#include "types.h"
#include <optional>
#include <ostream>
#include <string>
#include <vector>

namespace robotlang::extension {

struct Number {
    Integer value;
    SourceLocation source;
};

struct Command {
    enum class Kind { Forward, Left, Right, Repeat };
    Kind kind;
    SourceLocation source;
    Number argument;
    std::vector<Command> body;
    SourceLocation closingBrace;
    std::uint64_t id = 0;
};

struct Wall {
    Number x;
    Number y;
    Number width;
    Number height;
};

struct Hole {
    Number x;
    Number y;
};

struct Robot {
    std::string id;
    SourceLocation idSource;
    Number x;
    Number y;
    Direction facing;
    SourceLocation facingSource;
    std::optional<Number> explicitPriority;
    Integer priority = 0;
    std::vector<Command> commands;
};

struct Run {
    std::string id;
    SourceLocation idSource;
    std::vector<Command> commands;
};

struct Program {
    Number width;
    Number height;
    std::vector<Wall> walls;
    std::vector<Hole> holes;
    std::vector<Robot> robots;
    std::vector<Run> runs;
    SourceLocation end;
};

void printIr(const Program& program, std::ostream& out);

}

#endif
