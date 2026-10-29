#include <iostream>

#include "scanner.h"
#include "semantic.h"

int main() {
    // Fixed contract fixture matching square.robot. Indexing known tokens here
    // demonstrates adapter callbacks; it does NOT parse arbitrary RobotLang.
    Scanner scanner(
        "# Recorre un cuadrado de lado 3\n"
        "world 10 10\n"
        "robot at 2 2 facing north\n"
        "repeat 4 {\n"
        "  forward 3\n"
        "  right 90\n"
        "}\n");
    const auto tokens = scanner.scanAll();
    robotlang::SemanticAnalyzer analyzer;
    analyzer.world(tokens.at(0), tokens.at(1), tokens.at(2));
    analyzer.robot(tokens.at(3), tokens.at(5), tokens.at(6), tokens.at(8));
    analyzer.beginRepeat(tokens.at(9), tokens.at(10));
    analyzer.forward(tokens.at(12), tokens.at(13));
    analyzer.right(tokens.at(14), tokens.at(15));
    analyzer.endRepeat(tokens.at(16));
    const auto result = analyzer.finish(tokens.at(17));
    if (!result.ok()) {
        for (const auto& diagnostic : result.diagnostics) std::cerr << diagnostic << '\n';
        return 1;
    }
    std::cout << "Contract callbacks -> validated IR (AST adapter pending)\n";
    robotlang::ir::write(*result.program, std::cout);
    return 0;
}
