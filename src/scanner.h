#ifndef SCANNER_H
#define SCANNER_H

#include <string>
#include <vector>
#include "token.h"


class Scanner {
public:
    explicit Scanner(std::string source);

    Token nextToken();                 // devuelve el siguiente token
    std::vector<Token> scanAll();      // todos los tokens hasta END 

private:
    std::string src;   
    size_t pos = 0;    
    int line = 1;      
    int col = 1;       

    bool isAtEnd() const;              
    char peek() const;                 
    char advance();                   
    void skipSpacesAndComments();      
};

#endif