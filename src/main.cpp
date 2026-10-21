#include <fstream>
#include <iostream>
#include <sstream>
#include <string>

#include "scanner.h"

using namespace std;


static bool readFile(const string& path, string& out) {
    ifstream file(path);
    if (!file) return false;
    stringstream buffer;
    buffer << file.rdbuf();
    out = buffer.str();
    return true;
}

int main(int argc, char* argv[]) {
    if (argc < 2) {
        cerr << "Uso: robotc <archivo.robot> [--tokens]\n";
        return 1;
    }

    string path = argv[1];
    bool showTokens = false;
    for (int i = 2; i < argc; i++) {
        if (string(argv[i]) == "--tokens") showTokens = true;
    }

    string source;
    if (!readFile(path, source)) {
        cerr << "No se pudo abrir el archivo: " << path << "\n";
        return 1;
    }

    Scanner scanner(source);
    bool hasErrors = false;

    for (const Token& tok : scanner.scanAll()) {
        if (showTokens) cout << tok << "\n";
        if (tok.type == TokenType::ERR) {
            cerr << path << ":" << tok.line << ":" << tok.col
                 << ": error léxico: '" << tok.lexeme
                 << "' no pertenece a RobotLang\n";
            hasErrors = true;
        }
    }

    return hasErrors ? 1 : 0;
}