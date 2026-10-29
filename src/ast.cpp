#include "ast.h"
#include <utility>

namespace robotlang::ast {
World::World(SourceLocation s, Number w, Number h) : Node(s), width(w), height(h) {}
Wall::Wall(SourceLocation s, Number a, Number b, Number c, Number d) : WorldItem(s), x(a), y(b), width(c), height(d) {}
Obstacle::Obstacle(SourceLocation s, Number a, Number b) : WorldItem(s), x(a), y(b) {}
Hole::Hole(SourceLocation s, Number a, Number b) : WorldItem(s), x(a), y(b) {}
Robot::Robot(SourceLocation s, std::string name, SourceLocation idLoc, Number a, Number b, Direction d, SourceLocation facingLoc, std::optional<Number> p)
    : WorldItem(s), id(std::move(name)), idSource(idLoc), x(a), y(b), facing(d), facingSource(facingLoc), priority(p) {}
Forward::Forward(SourceLocation s, Number n) : Command(s), distance(n) {}
Turn::Turn(SourceLocation s, bool r, Number n) : Command(s), right(r), angle(n) {}
Repeat::Repeat(SourceLocation s, Number n) : Command(s), count(n) {}
Run::Run(SourceLocation s, std::string name, SourceLocation idLoc) : Node(s), id(std::move(name)), idSource(idLoc) {}
Program::Program(std::unique_ptr<World> w) : Node(w->source), world(std::move(w)) {}
void Program::accept(Visitor& v) const { v.visit(*this); }
void World::accept(Visitor& v) const { v.visit(*this); }
void Wall::accept(Visitor& v) const { v.visit(*this); }
void Obstacle::accept(Visitor& v) const { v.visit(*this); }
void Hole::accept(Visitor& v) const { v.visit(*this); }
void Robot::accept(Visitor& v) const { v.visit(*this); }
void Run::accept(Visitor& v) const { v.visit(*this); }
void Forward::accept(Visitor& v) const { v.visit(*this); }
void Turn::accept(Visitor& v) const { v.visit(*this); }
void Repeat::accept(Visitor& v) const { v.visit(*this); }

PrintVisitor::PrintVisitor(std::ostream& out) : out_(out) {}
void PrintVisitor::line(const Node& node, const std::string& text) {
    out_ << std::string(static_cast<std::size_t>(depth_) * 2, ' ') << text << " @" << node.source.line << ':' << node.source.col << '\n';
}
void PrintVisitor::visit(const Program& n) {
    line(n, "Program"); ++depth_; n.world->accept(*this); for (const auto& run : n.runs) run->accept(*this); --depth_;
}
void PrintVisitor::visit(const World& n) {
    line(n, "World " + std::to_string(n.width.value) + " " + std::to_string(n.height.value));
    ++depth_; for (const auto& item : n.items) item->accept(*this); --depth_;
}
void PrintVisitor::visit(const Wall& n) { line(n, "Wall " + std::to_string(n.x.value) + " " + std::to_string(n.y.value) + " " + std::to_string(n.width.value) + " " + std::to_string(n.height.value)); }
void PrintVisitor::visit(const Obstacle& n) { line(n, "Obstacle " + std::to_string(n.x.value) + " " + std::to_string(n.y.value)); }
void PrintVisitor::visit(const Hole& n) { line(n, "Hole " + std::to_string(n.x.value) + " " + std::to_string(n.y.value)); }
void PrintVisitor::visit(const Robot& n) {
    std::string text = "Robot " + n.id + " " + std::to_string(n.x.value) + " " + std::to_string(n.y.value) + " " + directionName(n.facing);
    if (n.priority) text += " priority " + std::to_string(n.priority->value);
    line(n, text);
}
void PrintVisitor::visit(const Run& n) { line(n, "Run " + n.id); ++depth_; for (const auto& cmd : n.commands) cmd->accept(*this); --depth_; }
void PrintVisitor::visit(const Forward& n) { line(n, "Forward " + std::to_string(n.distance.value)); }
void PrintVisitor::visit(const Turn& n) { line(n, std::string(n.right ? "Right " : "Left ") + std::to_string(n.angle.value)); }
void PrintVisitor::visit(const Repeat& n) { line(n, "Repeat " + std::to_string(n.count.value)); ++depth_; for (const auto& cmd : n.commands) cmd->accept(*this); --depth_; }
}
