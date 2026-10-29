#include "ir.h"

#include <ostream>
#include <sstream>
#include <type_traits>

namespace robotlang::ir {
namespace {

void walkInstructions(const std::vector<Instruction>& instructions, ConstVisitor& visitor) {
    for (const auto& instruction : instructions) {
        std::visit([&](const auto& operation) {
            visitor.visit(instruction.origin, operation);
            using T = std::decay_t<decltype(operation)>;
            if constexpr (std::is_same_v<T, Repeat>) {
                walkInstructions(operation.body, visitor);
                visitor.leave(instruction.origin, operation);
            }
        }, instruction.operation);
    }
}

void writeLocation(std::ostream& out, SourceLocation source) {
    out << source.line << ':' << source.col;
}

template <DomainType Kind>
void writeOperand(std::ostream& out, const char* name,
                  const Located<DomainInteger<Kind>>& operand) {
    out << ' ' << name << '=' << domainTypeName(Kind) << '(' << operand.value.value() << ")@";
    writeLocation(out, operand.source);
}

void writeInstructions(const std::vector<Instruction>& instructions,
                       std::ostream& out, std::size_t depth) {
    for (const auto& instruction : instructions) {
        const std::string indent(depth * 2, ' ');
        out << indent << '#' << instruction.origin.id << " @";
        writeLocation(out, instruction.origin.source);
        out << ' ';
        std::visit([&](const auto& operation) {
            using T = std::decay_t<decltype(operation)>;
            if constexpr (std::is_same_v<T, WorldInit>) {
                out << "WorldInit";
                writeOperand(out, "width", operation.width);
                writeOperand(out, "height", operation.height);
                out << '\n';
            } else if constexpr (std::is_same_v<T, RobotInit>) {
                out << "RobotInit";
                writeOperand(out, "x", operation.x);
                writeOperand(out, "y", operation.y);
                out << " facing=Direction(" << directionName(operation.facing.value) << ")@";
                writeLocation(out, operation.facing.source);
                out << '\n';
            } else if constexpr (std::is_same_v<T, MoveForward>) {
                out << "MoveForward";
                writeOperand(out, "distance", operation.distance);
                out << '\n';
            } else if constexpr (std::is_same_v<T, Turn>) {
                out << "Turn sense=" << (operation.sense == TurnSense::Left ? "left" : "right");
                writeOperand(out, "angle", operation.angle);
                out << '\n';
            } else if constexpr (std::is_same_v<T, Repeat>) {
                out << "Repeat";
                writeOperand(out, "count", operation.count);
                out << " {\n";
                writeInstructions(operation.body, out, depth + 1);
                out << indent << "} @";
                writeLocation(out, operation.closingBrace);
                out << '\n';
            }
        }, instruction.operation);
    }
}

} // namespace

void walk(const Program& program, ConstVisitor& visitor) {
    walkInstructions(program.instructions, visitor);
}

void write(const Program& program, std::ostream& out) {
    out << "RobotIR v1\n";
    writeInstructions(program.instructions, out, 0);
    out << "End @";
    writeLocation(out, program.end);
    out << '\n';
}

std::string toString(const Program& program) {
    std::ostringstream out;
    write(program, out);
    return out.str();
}

} // namespace robotlang::ir
