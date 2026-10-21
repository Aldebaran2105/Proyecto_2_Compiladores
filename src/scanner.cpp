#include "scanner.h"

#include <cctype>
#include <unordered_map>
#include <utility>

using namespace std;

// Palabras reservadas: texto -> tipo de token
static const unordered_map<string, TokenType> KEYWORDS = {
    {"world",   TokenType::WORLD},
    {"robot",   TokenType::ROBOT},
    {"at",      TokenType::AT},
    {"facing",  TokenType::FACING},
    {"forward", TokenType::FORWARD},
    {"right",   TokenType::RIGHT},
    {"left",    TokenType::LEFT},
    {"repeat",  TokenType::REPEAT},
    {"north",   TokenType::NORTH},
    {"south",   TokenType::SOUTH},
    {"east",    TokenType::EAST},
    {"west",    TokenType::WEST},
};

Scanner::Scanner(string source) : src(move(source)) {}

bool Scanner::isAtEnd() const {
    return pos >= src.size();
}

char Scanner::peek() const {
    if (isAtEnd()) return '\0';
    return src[pos];
}

// Avanza un carácter y actualiza línea y columna
char Scanner::advance() {
    char c = src[pos];
    pos++;
    if (c == '\n') {
        line++;
        col = 1;
    } else {
        col++;
    }
    return c;
}


void Scanner::skipSpacesAndComments() {
    while (!isAtEnd()) {
        char c = peek();
        if (c == ' ' || c == '\t' || c == '\r' || c == '\n') {
            advance();
        } else if (c == '#') {
            while (!isAtEnd() && peek() != '\n') advance();
        } else {
            break;
        }
    }
}

Token Scanner::nextToken() {
    skipSpacesAndComments();

    
    int startLine = line;
    int startCol = col;

    if (isAtEnd()) {
        return Token{TokenType::END, "", startLine, startCol};
    }

    char c = advance();

    
    if (isdigit(static_cast<unsigned char>(c))) {
        string text(1, c);
        while (isdigit(static_cast<unsigned char>(peek()))) {
            text += advance();
        }
        return Token{TokenType::NUM, text, startLine, startCol};
    }

    
    if (isalpha(static_cast<unsigned char>(c))) {
        string text(1, c);
        while (isalnum(static_cast<unsigned char>(peek())) || peek() == '_') {
            text += advance();
        }
        auto it = KEYWORDS.find(text);
        if (it != KEYWORDS.end()) {
            return Token{it->second, text, startLine, startCol};
        }
        
        return Token{TokenType::ERR, text, startLine, startCol};
    }

    
    if (c == '{') return Token{TokenType::LBRACE, "{", startLine, startCol};
    if (c == '}') return Token{TokenType::RBRACE, "}", startLine, startCol};

    
    return Token{TokenType::ERR, string(1, c), startLine, startCol};
}

vector<Token> Scanner::scanAll() {
    vector<Token> tokens;
    while (true) {
        Token t = nextToken();
        tokens.push_back(t);
        if (t.type == TokenType::END) break;
    }
    return tokens;
}