#ifndef ROBOTLANG_AST_H
#define ROBOTLANG_AST_H

#include <memory>
#include <ostream>
#include <vector>




struct ASTVisitor;

struct Node {
    int line;
    int col;

    Node(int line, int col) : line(line), col(col) {}
    virtual ~Node() = default;
    virtual void accept(ASTVisitor& visitor) = 0;
};

enum class Direction { North, South, East, West };
enum class TurnSide { Right, Left };

struct WorldDecl final : Node {
    int width;
    int height;

    WorldDecl(int width, int height, int line, int col);
    void accept(ASTVisitor& visitor) override;
};

struct RobotDecl final : Node {
    int x;
    int y;
    Direction facing;

    RobotDecl(int x, int y, Direction facing, int line, int col);
    void accept(ASTVisitor& visitor) override;
};

struct Command : Node {
    using Node::Node;
};

struct ForwardCmd final : Command {
    int distance;

    ForwardCmd(int distance, int line, int col);
    void accept(ASTVisitor& visitor) override;
};

struct TurnCmd final : Command {
    TurnSide side;
    int degrees;

    TurnCmd(TurnSide side, int degrees, int line, int col);
    void accept(ASTVisitor& visitor) override;
};

struct RepeatCmd final : Command {
    int times;
    std::vector<std::unique_ptr<Command>> commands;

    RepeatCmd(int times, std::vector<std::unique_ptr<Command>> commands,
              int line, int col);
    void accept(ASTVisitor& visitor) override;
};

struct Program final : Node {
    std::unique_ptr<WorldDecl> world;
    std::unique_ptr<RobotDecl> robot;
    std::vector<std::unique_ptr<Command>> commands;

    Program(std::unique_ptr<WorldDecl> world,
            std::unique_ptr<RobotDecl> robot,
            std::vector<std::unique_ptr<Command>> commands,
            int line, int col);
    void accept(ASTVisitor& visitor) override;
};


struct ASTVisitor {
    virtual ~ASTVisitor() = default;
    virtual void visit(Program& node) = 0;
    virtual void visit(WorldDecl& node) = 0;
    virtual void visit(RobotDecl& node) = 0;
    virtual void visit(ForwardCmd& node) = 0;
    virtual void visit(TurnCmd& node) = 0;
    virtual void visit(RepeatCmd& node) = 0;
};

class PrintVisitor final : public ASTVisitor {
public:
    explicit PrintVisitor(std::ostream& out);

    void visit(Program& node) override;
    void visit(WorldDecl& node) override;
    void visit(RobotDecl& node) override;
    void visit(ForwardCmd& node) override;
    void visit(TurnCmd& node) override;
    void visit(RepeatCmd& node) override;

private:
    std::ostream& out_;
    int depth_ = 0;

    void indent();
    void position(const Node& node);
};

#endif
