#ifndef ROBOTLANG_AST_SEMANTIC_H
#define ROBOTLANG_AST_SEMANTIC_H

#include "ast.h"
#include "semantic.h"

namespace robotlang {
class SemanticVisitor final : public ast::Visitor {
public:
    extension::AnalysisResult analyze(const ast::Program& program);
    void visit(const ast::Program&) override;
    void visit(const ast::World&) override;
    void visit(const ast::Wall&) override;
    void visit(const ast::Obstacle&) override;
    void visit(const ast::Hole&) override;
    void visit(const ast::Robot&) override;
    void visit(const ast::Run&) override;
    void visit(const ast::Forward&) override;
    void visit(const ast::Turn&) override;
    void visit(const ast::Repeat&) override;
private:
    extension::Program program_{};
    std::vector<extension::Command>* commands_ = nullptr;
};
}
#endif
