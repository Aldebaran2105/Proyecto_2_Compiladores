#ifndef ROBOTLANG_PARSER_H
#define ROBOTLANG_PARSER_H

#include "ast.h"
#include "scanner.h"
#include "token.h"

#include <memory>
#include <string>



class Parser {
public:
    explicit Parser(Scanner& scanner);

    std::unique_ptr<Program> parseProgram();

private:
    Scanner& scanner_;
    Token current_;

    std::unique_ptr<WorldDecl> parseWorld();
    std::unique_ptr<RobotDecl> parseRobot();
    std::unique_ptr<Command> parseCommand();
    std::unique_ptr<ForwardCmd> parseForward();
    std::unique_ptr<TurnCmd> parseTurn();
    std::unique_ptr<RepeatCmd> parseRepeat();

    Token expect(TokenType type, const std::string& message);
    void advance();
    int parseNumber(const std::string& message);
    [[noreturn]] void fail(const Token& token, const std::string& message) const;
};

#endif
