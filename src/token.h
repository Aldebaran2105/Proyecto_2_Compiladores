#ifndef TOKEN_H
#define TOKEN_H

#include <string>
#include <ostream>

enum class TokenType {
    WORLD, ROBOT, AT, FACING,
    FORWARD, RIGHT, LEFT, REPEAT,
    NORTH, SOUTH, EAST, WEST,
    OBSTACLE, HOLE, WALL, PRIORITY, RUN,
    ID,
    NUM, LBRACE, RBRACE,
    ERR, END
};

struct Token {
    TokenType type;
    std::string lexeme; 
    int line;
    int col;
};

std::string tokenTypeName(TokenType t);

std::ostream& operator<<(std::ostream& out, const Token& tok);

#endif
