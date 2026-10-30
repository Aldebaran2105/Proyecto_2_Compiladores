#include "semantic.h"
#include "ast_semantic.h"
#include "codegen.h"
#include "optimizer.h"
#include "parser.h"
#include "simulator.h"
#include <iostream>
#include <random>
#include <sstream>
#include <stdexcept>
#include <string>

using namespace robotlang::extension;

void check(bool condition, const std::string& message) {
    if (!condition) throw std::runtime_error(message);
}

AnalysisResult analyze(const std::string& source) {
    Scanner scanner(source);
    robotlang::Parser parser(scanner);
    auto tree = parser.parseProgram();
    return robotlang::SemanticVisitor().analyze(*tree);
}

bool hasError(const AnalysisResult& result, const std::string& code) {
    for (const auto& diagnostic : result.diagnostics) if (diagnostic.code == code) return true;
    return false;
}

int main() {
    try {
        auto patrol = analyze("world 10 8 { wall 4 3 2 2 hole at 7 2 robot r1 at 1 1 facing east priority 1 robot r2 at 8 1 facing west priority 2 } run r1 { forward 3 right 90 forward 2 } run r2 { forward 6 }");
        check(patrol.program.has_value(), "patrol must validate");
        Scanner astScanner("world 4 4 { obstacle at 1 2 robot r1 at 0 0 facing north } run r1 { repeat 2 { forward 1 } }");
        robotlang::Parser astParser(astScanner);
        auto ast = astParser.parseProgram();
        std::ostringstream tree;
        robotlang::ast::PrintVisitor printer(tree);
        ast->accept(printer);
        check(tree.str().find("Obstacle 1 2 @1:13") != std::string::npos && tree.str().find("Repeat 2") != std::string::npos && tree.str().find("Forward 1") != std::string::npos, "AST visitor output");
        std::string originalTree = tree.str();
        auto lowered = robotlang::SemanticVisitor().analyze(*ast);
        check(lowered.program.has_value(), "AST lowers to valid IR");
        Optimizer().optimize(*lowered.program);
        std::ostringstream unchangedTree;
        robotlang::ast::PrintVisitor secondPrinter(unchangedTree);
        ast->accept(secondPrinter);
        check(unchangedTree.str() == originalTree, "lowering and optimization preserve the AST");
        check(tree.str().find("Obstacle") < tree.str().find("Robot"), "world declaration order");
        auto state = Simulator(*patrol.program).run(true);
        check(!state.limitReached, "patrol must finish");
        check(state.robots[0].x == 4 && state.robots[0].y == 0 && state.robots[0].facing == robotlang::Direction::South, "bottom-left coordinate semantics");
        check(state.robots[1].x == 5 && state.robots[1].y == 1, "priority blocking");
        check(state.events.size() == 4, "trace actions");

        auto priorities = analyze("world 5 5 { robot a at 0 0 facing north priority 3 robot b at 1 0 facing north robot c at 2 0 facing north priority 1 } ");
        check(priorities.program.has_value() && priorities.program->robots[1].priority == 2, "priority assignment");
        check(hasError(analyze("world 5 5 { robot a at 0 0 facing north robot a at 1 0 facing north }"), "duplicate_robot_id"), "duplicate robot");
        check(hasError(analyze("world 5 5 { robot a at 0 0 facing north } run missing { forward 1 }"), "unknown_run"), "unknown run");
        check(hasError(analyze("world 5 5 { robot a at 0 0 facing north } run a { repeat 0 { forward 1 } }"), "non_positive_value"), "repeat zero");
        check(hasError(analyze("world 5 5 { wall 1 1 2 2 hole at 1 1 robot a at 0 0 facing north }"), "hole_overlaps_solid"), "terrain overlap");
        check(hasError(analyze("world 5 5 { robot a at 0 0 facing north } run a { right 45 }"), "invalid_angle"), "invalid turn");
        check(hasError(analyze("world 5 5 { robot a at 0 0 facing north } run a {} run a {}"), "duplicate_run"), "duplicate run");
        check(hasError(analyze("world 5 5 { robot a at 0 0 facing north priority 1 robot b at 1 0 facing east priority 1 }"), "duplicate_priority"), "duplicate priority");
        check(hasError(analyze("world 2 2 { robot a at 0 0 facing north } run a { forward 0 }"), "non_positive_value"), "zero forward");
        check(hasError(analyze("world 2 2 { robot a at 0 0 facing north priority 2 }"), "priority_out_of_range"), "priority range");
        check(analyze("world 9007199254740991 1 { robot a at 9007199254740990 0 facing west } run a { forward 0001 }").program.has_value(), "maximum integer and leading zeros");
        auto outOfBounds = analyze("world 5 5 { robot a at 5 0 facing north }");
        check(hasError(outOfBounds, "initial_position_out_of_bounds") && outOfBounds.diagnostics[0].source.col == 24, "operand location");
        try {
            analyze("world 9007199254740992 1 { robot a at 0 0 facing north }");
            check(false, "out-of-range literal accepted");
        } catch (const std::runtime_error& error) {
            check(std::string(error.what()).find("1:7: integer_out_of_range") != std::string::npos, "out-of-range location");
        }
        try {
            analyze("world 2 2 { robot a at 0 0 facing north } run a { repeat 2 { forward 1 }");
            check(false, "unclosed run accepted");
        } catch (const std::runtime_error& error) {
            check(std::string(error.what()).find("cerrar run") != std::string::npos, "missing brace error");
        }
        try {
            analyze("world 2 2 { robot a at 0 0 facing north } run a { for 2 {} }");
            check(false, "for accepted as a command");
        } catch (const std::runtime_error& error) {
            check(std::string(error.what()).find("forward, left, right o repeat") != std::string::npos, "for rejection");
        }

        auto hole = analyze("world 5 5 { hole at 2 0 robot a at 0 0 facing east } run a { forward 4 }");
        check(hole.program.has_value(), "hole program");
        auto fallen = Simulator(*hole.program).run();
        check(fallen.robots[0].status == RobotStatus::Fallen && fallen.robots[0].x == 2, "hole fall");

        auto moves = analyze("world 10 2 { robot a at 0 0 facing east } run a { forward 2 forward 3 right 90 left 90 }");
        check(moves.program.has_value(), "optimizer program");
        auto optimized = Optimizer().optimize(*moves.program);
        check(optimized.rewrites.size() == 2 && optimized.program.robots[0].commands.size() == 1, "optimizer rewrites");
        check(optimized.program.robots[0].commands[0].argument.value == 5, "merged distance");
        auto movesOnly = Optimizer(100000, {true, false}).optimize(*moves.program);
        auto turnsOnly = Optimizer(100000, {false, true}).optimize(*moves.program);
        check(movesOnly.program.robots[0].commands.size() == 3 && turnsOnly.program.robots[0].commands.size() == 2, "optimizer pass toggles");
        std::ostringstream originalAssembly, optimizedAssembly;
        Codegen(originalAssembly).emit(*moves.program);
        Codegen(optimizedAssembly).emit(optimized.program, optimized.rewrites, true);
        check(originalAssembly.str().find("forward 2") != std::string::npos && originalAssembly.str().find("forward 3") != std::string::npos, "original command comments");
        check(optimizedAssembly.str().find("rewrite merge_moves") != std::string::npos && optimizedAssembly.str().find("forward 5") != std::string::npos, "optimized command comments");
        check(optimizedAssembly.str().find(".loc ") == std::string::npos && optimizedAssembly.str().find(".file ") == std::string::npos, "no source mapping directives");
        check(originalAssembly.str() != optimizedAssembly.str(), "assembly differs after optimization");

        auto concurrent = analyze("world 5 1 { robot a at 0 0 facing east priority 1 robot b at 3 0 facing west priority 2 } run a { forward 1 forward 1 } run b { forward 1 }");
        check(concurrent.program.has_value(), "concurrent optimizer program");
        auto rejected = Optimizer().optimize(*concurrent.program);
        check(rejected.rewrites.empty() && rejected.program.robots[0].commands.size() == 2, "unsafe multi-robot optimization rejected");

        auto empty = analyze("world 1 1 { robot a at 0 0 facing north } run a { repeat 9007199254740991 { repeat 9007199254740991 {} } }");
        check(empty.program.has_value() && !Simulator(*empty.program, 2).run().limitReached, "nested empty loops terminate");

        std::mt19937 generator(42);
        for (int trial = 0; trial < 50; ++trial) {
            std::string source = "world 10 10 { robot a at 5 5 facing north } run a { repeat 2 { ";
            for (int i = 0; i < 12; ++i) {
                switch (generator() % 3) {
                    case 0: source += "forward " + std::to_string(generator() % 3 + 1) + " "; break;
                    case 1: source += "left 90 "; break;
                    default: source += "right 90 "; break;
                }
            }
            source += "} }";
            auto input = analyze(source);
            check(input.program.has_value(), "random program validates");
            auto before = Simulator(*input.program).run();
            auto after = Simulator(Optimizer().optimize(*input.program).program).run();
            check(!before.limitReached && !after.limitReached && before.robots[0].x == after.robots[0].x &&
                  before.robots[0].y == after.robots[0].y && before.robots[0].facing == after.robots[0].facing &&
                  before.robots[0].status == after.robots[0].status, "randomized optimizer equivalence");
        }

        auto longRun = analyze("world 1 1 { robot a at 0 0 facing north } run a { repeat 10 { right 90 } }");
        check(longRun.program.has_value() && Simulator(*longRun.program, 3).run().limitReached, "tick limit");
        std::cout << "extension tests passed\n";
        return 0;
    } catch (const std::exception& error) {
        std::cerr << error.what() << '\n';
        return 1;
    }
}
