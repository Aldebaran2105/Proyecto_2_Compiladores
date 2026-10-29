#ifndef ROBOTLANG_PARSER_H
#define ROBOTLANG_PARSER_H

#include "ast.h"
#include "scanner.h"
#include <memory>
#include <string>

namespace robotlang {
class Parser {
public:
    explicit Parser(Scanner& scanner);
    std::unique_ptr<ast::Program> parseProgram();
private:
    Scanner& scanner_;
    Token current_;
    void advance();
    Token expect(TokenType type, const std::string& message);
    ast::Number number();
    Direction direction();
    std::unique_ptr<ast::World> world();
    std::unique_ptr<ast::WorldItem> worldItem();
    std::unique_ptr<ast::Run> run();
    std::unique_ptr<ast::Command> command();
    [[noreturn]] void fail(const std::string& message) const;
};
}
#endif
