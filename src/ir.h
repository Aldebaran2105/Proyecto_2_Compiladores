#ifndef ROBOTLANG_IR_H
#define ROBOTLANG_IR_H

#include <cstdint>
#include <iosfwd>
#include <string>
#include <variant>
#include <vector>

#include "types.h"

namespace robotlang::ir {

using InstructionId = std::uint64_t;

struct Origin {
    InstructionId id;
    SourceLocation source;
};

struct WorldInit {
    Located<Dimension> width;
    Located<Dimension> height;
};

struct RobotInit {
    Located<Coordinate> x;
    Located<Coordinate> y;
    Located<Direction> facing;
};

struct MoveForward {
    Located<Distance> distance;
};

enum class TurnSense { Left, Right };

struct Turn {
    TurnSense sense;
    Located<Angle> angle;
};

struct Instruction;

struct Repeat {
    Located<Count> count;
    std::vector<Instruction> body;
    SourceLocation closingBrace;
};

using Operation = std::variant<WorldInit, RobotInit, MoveForward, Turn, Repeat>;

struct Instruction {
    Origin origin;
    Operation operation;
};

struct Program {
    std::vector<Instruction> instructions;
    SourceLocation end;
};

// A structural visitor: walk visits a Repeat once, then its body once, including
// count=0. This is a compiler traversal, not an execution or an unrolling pass.
class ConstVisitor {
public:
    virtual ~ConstVisitor() = default;
    virtual void visit(const Origin&, const WorldInit&) = 0;
    virtual void visit(const Origin&, const RobotInit&) = 0;
    virtual void visit(const Origin&, const MoveForward&) = 0;
    virtual void visit(const Origin&, const Turn&) = 0;
    virtual void visit(const Origin&, const Repeat&) = 0;
    virtual void leave(const Origin&, const Repeat&) {}
};

void walk(const Program& program, ConstVisitor& visitor);
void write(const Program& program, std::ostream& out);
std::string toString(const Program& program);

} // namespace robotlang::ir

#endif
