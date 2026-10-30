#include "ast_semantic.h"
#include <utility>

namespace robotlang {
namespace {
extension::Number number(ast::Number value) { return {value.value, value.source}; }
}
extension::AnalysisResult SemanticVisitor::analyze(const ast::Program& program) {
    program_ = {};
    program.accept(*this);
    return extension::SemanticAnalyzer().analyze(std::move(program_));
}
void SemanticVisitor::visit(const ast::Program& node) {
    node.world->accept(*this);
    for (const auto& run : node.runs) run->accept(*this);
    program_.end = node.end;
}
void SemanticVisitor::visit(const ast::World& node) {
    program_.width = number(node.width);
    program_.height = number(node.height);
    for (const auto& item : node.items) item->accept(*this);
}
void SemanticVisitor::visit(const ast::Wall& node) {
    program_.walls.push_back({number(node.x), number(node.y), number(node.width), number(node.height)});
}
void SemanticVisitor::visit(const ast::Obstacle& node) {
    program_.walls.push_back({number(node.x), number(node.y), {1, node.source}, {1, node.source}});
}
void SemanticVisitor::visit(const ast::Hole& node) { program_.holes.push_back({number(node.x), number(node.y)}); }
void SemanticVisitor::visit(const ast::Robot& node) {
    std::optional<extension::Number> priority;
    if (node.priority) priority = number(*node.priority);
    program_.robots.push_back({node.id, node.idSource, number(node.x), number(node.y), node.facing, node.facingSource, priority, 0, {}});
}
void SemanticVisitor::visit(const ast::Run& node) {
    extension::Run run{node.id, node.idSource, {}};
    auto* previous = commands_;
    commands_ = &run.commands;
    for (const auto& cmd : node.commands) cmd->accept(*this);
    commands_ = previous;
    program_.runs.push_back(std::move(run));
}
void SemanticVisitor::visit(const ast::Forward& node) {
    commands_->push_back({extension::Command::Kind::Forward, node.source, number(node.distance), {}, {}, 0});
}
void SemanticVisitor::visit(const ast::Turn& node) {
    commands_->push_back({node.right ? extension::Command::Kind::Right : extension::Command::Kind::Left, node.source, number(node.angle), {}, {}, 0});
}
void SemanticVisitor::visit(const ast::Repeat& node) {
    extension::Command repeat{extension::Command::Kind::Repeat, node.source, number(node.count), {}, node.closingBrace, 0};
    auto* previous = commands_;
    commands_ = &repeat.body;
    for (const auto& cmd : node.commands) cmd->accept(*this);
    commands_ = previous;
    commands_->push_back(std::move(repeat));
}
}
