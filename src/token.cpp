#include "token.h"
using namespace std;


string tokenTypeName(TokenType t){
    switch (t) {
        case TokenType::WORLD:   return "WORLD";
        case TokenType::ROBOT:   return "ROBOT";
        case TokenType::AT:      return "AT";
        case TokenType::FACING:  return "FACING";
        case TokenType::FORWARD: return "FORWARD";
        case TokenType::RIGHT:   return "RIGHT";
        case TokenType::LEFT:    return "LEFT";
        case TokenType::REPEAT:  return "REPEAT";
        case TokenType::NORTH:   return "NORTH";
        case TokenType::SOUTH:   return "SOUTH";
        case TokenType::EAST:    return "EAST";
        case TokenType::WEST:    return "WEST";
        case TokenType::OBSTACLE: return "OBSTACLE";
        case TokenType::HOLE:    return "HOLE";
        case TokenType::WALL:    return "WALL";
        case TokenType::PRIORITY: return "PRIORITY";
        case TokenType::RUN:     return "RUN";
        case TokenType::ID:      return "ID";
        case TokenType::NUM:     return "NUM";
        case TokenType::LBRACE:  return "LBRACE";
        case TokenType::RBRACE:  return "RBRACE";
        case TokenType::ERR:     return "ERR";
        case TokenType::END:     return "END";
    }
    return "?";
}

ostream& operator<<(ostream& out, const Token& tok){
    out << tok.line << ":" << tok.col << "\t"
        << tokenTypeName(tok.type) << "\t\"" << tok.lexeme << "\"";
    return out;
}