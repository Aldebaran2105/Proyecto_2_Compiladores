#include "ast.h"

#include <string>
#include <utility>

namespace {
const char* directionName(Direction direction) {
    switch (direction) {
        case Direction::North: return "north";
        case Direction::South: return "south";
        case Direction::East: return "east";
        case Direction::West: return "west";
    }
    return "?";
}

const char* turnName(TurnSide side) {
    return side == TurnSide::Right ? "right" : "left";
}
}

WorldDecl::WorldDecl(int width, int height, int line, int col)
    : Node(line, col), width(width), height(height) {}
void WorldDecl::accept(ASTVisitor& visitor) { visitor.visit(*this); }

RobotDecl::RobotDecl(int x, int y, Direction facing, int line, int col)
    : Node(line, col), x(x), y(y), facing(facing) {}
void RobotDecl::accept(ASTVisitor& visitor) { visitor.visit(*this); }

ForwardCmd::ForwardCmd(int distance, int line, int col)
    : Command(line, col), distance(distance) {}
void ForwardCmd::accept(ASTVisitor& visitor) { visitor.visit(*this); }

TurnCmd::TurnCmd(TurnSide side, int degrees, int line, int col)
    : Command(line, col), side(side), degrees(degrees) {}
void TurnCmd::accept(ASTVisitor& visitor) { visitor.visit(*this); }

RepeatCmd::RepeatCmd(int times, std::vector<std::unique_ptr<Command>> commands,
                     int line, int col)
    : Command(line, col), times(times), commands(std::move(commands)) {}
void RepeatCmd::accept(ASTVisitor& visitor) { visitor.visit(*this); }

Program::Program(std::unique_ptr<WorldDecl> world,
                 std::unique_ptr<RobotDecl> robot,
                 std::vector<std::unique_ptr<Command>> commands,
                 int line, int col)
    : Node(line, col), world(std::move(world)), robot(std::move(robot)),
      commands(std::move(commands)) {}
void Program::accept(ASTVisitor& visitor) { visitor.visit(*this); }

PrintVisitor::PrintVisitor(std::ostream& out) : out_(out) {}

void PrintVisitor::indent() { out_ << std::string(depth_ * 2, ' '); }
void PrintVisitor::position(const Node& node) {
    out_ << " [" << node.line << ':' << node.col << "]\n";
}

void PrintVisitor::visit(Program& node) {
    indent(); out_ << "Program"; position(node);
    ++depth_;
    node.world->accept(*this);
    node.robot->accept(*this);
    for (auto& command : node.commands) command->accept(*this);
    --depth_;
}

void PrintVisitor::visit(WorldDecl& node) {
    indent(); out_ << "WorldDecl width=" << node.width
                   << " height=" << node.height;
    position(node);
}

void PrintVisitor::visit(RobotDecl& node) {
    indent(); out_ << "RobotDecl x=" << node.x << " y=" << node.y
                   << " facing=" << directionName(node.facing);
    position(node);
}

void PrintVisitor::visit(ForwardCmd& node) {
    indent(); out_ << "ForwardCmd distance=" << node.distance;
    position(node);
}

void PrintVisitor::visit(TurnCmd& node) {
    indent(); out_ << "TurnCmd side=" << turnName(node.side)
                   << " degrees=" << node.degrees;
    position(node);
}

void PrintVisitor::visit(RepeatCmd& node) {
    indent(); out_ << "RepeatCmd times=" << node.times; position(node);
    ++depth_;
    for (auto& command : node.commands) command->accept(*this);
    --depth_;
}
