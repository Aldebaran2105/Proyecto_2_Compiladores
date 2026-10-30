#ifndef ROBOTLANG_AST_H
#define ROBOTLANG_AST_H

#include "types.h"
#include <memory>
#include <optional>
#include <ostream>
#include <string>
#include <vector>

namespace robotlang::ast {

struct Number { Integer value; SourceLocation source; };
class Visitor;
class Program;
class World;
class Wall;
class Obstacle;
class Hole;
class Robot;
class Run;
class Forward;
class Turn;
class Repeat;

class Node {
public:
    explicit Node(SourceLocation location) : source(location) {}
    virtual ~Node() = default;
    virtual void accept(Visitor& visitor) const = 0;
    SourceLocation source;
};

class WorldItem : public Node { public: using Node::Node; };
class Command : public Node { public: using Node::Node; };

class World final : public Node {
public:
    World(SourceLocation source, Number width, Number height);
    void accept(Visitor& visitor) const override;
    Number width, height;
    std::vector<std::unique_ptr<WorldItem>> items;
};

class Wall final : public WorldItem {
public:
    Wall(SourceLocation source, Number x, Number y, Number width, Number height);
    void accept(Visitor& visitor) const override;
    Number x, y, width, height;
};

class Obstacle final : public WorldItem {
public:
    Obstacle(SourceLocation source, Number x, Number y);
    void accept(Visitor& visitor) const override;
    Number x, y;
};

class Hole final : public WorldItem {
public:
    Hole(SourceLocation source, Number x, Number y);
    void accept(Visitor& visitor) const override;
    Number x, y;
};

class Robot final : public WorldItem {
public:
    Robot(SourceLocation source, std::string id, SourceLocation idSource, Number x, Number y,
          Direction facing, SourceLocation facingSource, std::optional<Number> priority);
    void accept(Visitor& visitor) const override;
    std::string id;
    SourceLocation idSource;
    Number x, y;
    Direction facing;
    SourceLocation facingSource;
    std::optional<Number> priority;
};

class Forward final : public Command {
public:
    Forward(SourceLocation source, Number distance);
    void accept(Visitor& visitor) const override;
    Number distance;
};

class Turn final : public Command {
public:
    Turn(SourceLocation source, bool right, Number angle);
    void accept(Visitor& visitor) const override;
    bool right;
    Number angle;
};

class Repeat final : public Command {
public:
    Repeat(SourceLocation source, Number count);
    void accept(Visitor& visitor) const override;
    Number count;
    SourceLocation closingBrace;
    std::vector<std::unique_ptr<Command>> commands;
};

class Run final : public Node {
public:
    Run(SourceLocation source, std::string id, SourceLocation idSource);
    void accept(Visitor& visitor) const override;
    std::string id;
    SourceLocation idSource, closingBrace;
    std::vector<std::unique_ptr<Command>> commands;
};

class Program final : public Node {
public:
    explicit Program(std::unique_ptr<World> world);
    void accept(Visitor& visitor) const override;
    std::unique_ptr<World> world;
    std::vector<std::unique_ptr<Run>> runs;
    SourceLocation end;
};

class Visitor {
public:
    virtual ~Visitor() = default;
    virtual void visit(const Program&) = 0;
    virtual void visit(const World&) = 0;
    virtual void visit(const Wall&) = 0;
    virtual void visit(const Obstacle&) = 0;
    virtual void visit(const Hole&) = 0;
    virtual void visit(const Robot&) = 0;
    virtual void visit(const Run&) = 0;
    virtual void visit(const Forward&) = 0;
    virtual void visit(const Turn&) = 0;
    virtual void visit(const Repeat&) = 0;
};

class PrintVisitor final : public Visitor {
public:
    explicit PrintVisitor(std::ostream& out);
    void visit(const Program&) override;
    void visit(const World&) override;
    void visit(const Wall&) override;
    void visit(const Obstacle&) override;
    void visit(const Hole&) override;
    void visit(const Robot&) override;
    void visit(const Run&) override;
    void visit(const Forward&) override;
    void visit(const Turn&) override;
    void visit(const Repeat&) override;
private:
    std::ostream& out_;
    int depth_ = 0;
    void line(const Node& node, const std::string& text);
};
}
#endif
