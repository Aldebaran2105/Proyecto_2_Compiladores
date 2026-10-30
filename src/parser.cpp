#include "parser.h"
#include <stdexcept>
#include <utility>

namespace robotlang {
namespace {
SourceLocation location(const Token& token) { return {token.line, token.col}; }
}
Parser::Parser(Scanner& scanner) : scanner_(scanner), current_(scanner_.nextToken()) {}
void Parser::advance() { current_ = scanner_.nextToken(); }
[[noreturn]] void Parser::fail(const std::string& message) const {
    throw std::runtime_error(std::to_string(current_.line) + ':' + std::to_string(current_.col) + ": " + message);
}
Token Parser::expect(TokenType type, const std::string& message) {
    if (current_.type != type) fail(message);
    Token token = current_;
    advance();
    return token;
}
ast::Number Parser::number() {
    Token token = expect(TokenType::NUM, "se esperaba un numero");
    Integer value = 0;
    for (char c : token.lexeme) {
        Integer digit = c - '0';
        if (value > (MaxInteger - digit) / 10)
            throw std::runtime_error(std::to_string(token.line) + ':' + std::to_string(token.col) + ": integer_out_of_range");
        value = value * 10 + digit;
    }
    return {value, location(token)};
}
Direction Parser::direction() {
    Direction value;
    switch (current_.type) {
        case TokenType::NORTH: value = Direction::North; break;
        case TokenType::EAST: value = Direction::East; break;
        case TokenType::SOUTH: value = Direction::South; break;
        case TokenType::WEST: value = Direction::West; break;
        default: fail("se esperaba una direccion");
    }
    advance();
    return value;
}
std::unique_ptr<ast::Command> Parser::command() {
    Token start = current_;
    if (start.type == TokenType::FORWARD) { advance(); return std::make_unique<ast::Forward>(location(start), number()); }
    if (start.type == TokenType::LEFT || start.type == TokenType::RIGHT) {
        advance(); return std::make_unique<ast::Turn>(location(start), start.type == TokenType::RIGHT, number());
    }
    if (start.type != TokenType::REPEAT) fail("se esperaba forward, left, right o repeat");
    advance();
    auto result = std::make_unique<ast::Repeat>(location(start), number());
    expect(TokenType::LBRACE, "se esperaba '{' despues de repeat");
    while (current_.type != TokenType::RBRACE) {
        if (current_.type == TokenType::END) fail("se esperaba '}' para cerrar repeat");
        result->commands.push_back(command());
    }
    result->closingBrace = location(expect(TokenType::RBRACE, "se esperaba '}'"));
    return result;
}
std::unique_ptr<ast::Run> Parser::run() {
    Token start = expect(TokenType::RUN, "se esperaba run");
    Token id = expect(TokenType::ID, "se esperaba el nombre del robot");
    auto result = std::make_unique<ast::Run>(location(start), id.lexeme, location(id));
    expect(TokenType::LBRACE, "se esperaba '{' despues de run");
    while (current_.type != TokenType::RBRACE) {
        if (current_.type == TokenType::END) fail("se esperaba '}' para cerrar run");
        result->commands.push_back(command());
    }
    result->closingBrace = location(expect(TokenType::RBRACE, "se esperaba '}'"));
    return result;
}
std::unique_ptr<ast::WorldItem> Parser::worldItem() {
    Token start = current_;
    if (start.type == TokenType::WALL) {
        advance(); auto x = number(), y = number(), w = number(), h = number();
        return std::make_unique<ast::Wall>(location(start), x, y, w, h);
    }
    if (start.type == TokenType::OBSTACLE || start.type == TokenType::HOLE) {
        advance(); expect(TokenType::AT, "se esperaba at"); auto x = number(), y = number();
        if (start.type == TokenType::OBSTACLE) return std::make_unique<ast::Obstacle>(location(start), x, y);
        return std::make_unique<ast::Hole>(location(start), x, y);
    }
    if (start.type != TokenType::ROBOT) fail("se esperaba terreno, robot o '}'");
    advance();
    Token id = expect(TokenType::ID, "se esperaba el nombre del robot");
    expect(TokenType::AT, "se esperaba at");
    auto x = number(), y = number();
    expect(TokenType::FACING, "se esperaba facing");
    SourceLocation facingSource = location(current_);
    Direction facing = direction();
    std::optional<ast::Number> priority;
    if (current_.type == TokenType::PRIORITY) { advance(); priority = number(); }
    return std::make_unique<ast::Robot>(location(start), id.lexeme, location(id), x, y, facing, facingSource, priority);
}
std::unique_ptr<ast::World> Parser::world() {
    Token start = expect(TokenType::WORLD, "se esperaba world");
    auto width = number(), height = number();
    auto result = std::make_unique<ast::World>(location(start), width, height);
    expect(TokenType::LBRACE, "se esperaba '{' despues de world");
    while (current_.type != TokenType::RBRACE) {
        if (current_.type == TokenType::END) fail("se esperaba '}' para cerrar world");
        result->items.push_back(worldItem());
    }
    expect(TokenType::RBRACE, "se esperaba '}'");
    return result;
}
std::unique_ptr<ast::Program> Parser::parseProgram() {
    auto result = std::make_unique<ast::Program>(world());
    while (current_.type == TokenType::RUN) result->runs.push_back(run());
    if (current_.type != TokenType::END) fail("se esperaba run o fin de archivo");
    result->end = location(current_);
    return result;
}
}
