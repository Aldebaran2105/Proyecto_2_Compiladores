// Herramienta de medición: aplica cada optimización por separado y reporta
// instrucciones de la IR, ticks simulados y (opcional) emite el ensamblador.
// Uso: opt_stats archivo.robot {none|merge|turns|all} [salida.s] [max_ticks]
#include "ast_semantic.h"
#include "codegen.h"
#include "optimizer.h"
#include "parser.h"
#include "simulator.h"
#include <fstream>
#include <iostream>
#include <sstream>

using namespace robotlang::extension;

static long countCommands(const std::vector<Command>& commands) {
    long n = 0;
    for (const auto& c : commands) n += 1 + countCommands(c.body);
    return n;
}
static long countProgram(const Program& p) {
    long n = 0;
    for (const auto& r : p.robots) n += countCommands(r.commands);
    return n;
}

int main(int argc, char* argv[]) {
    if (argc < 3) { std::cerr << "uso: opt_stats archivo {none|merge|turns|all} [salida.s] [max_ticks]\n"; return 1; }
    std::ifstream file(argv[1]);
    std::stringstream buffer; buffer << file.rdbuf();
    std::string mode = argv[2];
    robotlang::Integer maxTicks = argc > 4 ? std::stoll(argv[4]) : 100000;
    Scanner scanner(buffer.str());
    robotlang::Parser parser(scanner);
    auto tree = parser.parseProgram();
    AnalysisResult analysis = robotlang::SemanticVisitor().analyze(*tree);
    if (!analysis.program) { std::cerr << "programa invalido\n"; return 1; }
    Program original = *analysis.program;
    Program result = original;
    std::vector<Rewrite> rewrites;
    if (mode != "none") {
        OptimizerOptions options;
        options.mergeMoves = (mode == "merge" || mode == "all");
        options.simplifyTurns = (mode == "turns" || mode == "all");
        auto optimized = Optimizer(maxTicks, options).optimize(original);
        result = optimized.program;
        rewrites = optimized.rewrites;
    }
    auto before = Simulator(original, maxTicks).run();
    auto after = Simulator(result, maxTicks).run();
    std::cout << "instr_before=" << countProgram(original) << " instr_after=" << countProgram(result)
              << " ticks_before=" << before.ticks << " ticks_after=" << after.ticks
              << " rewrites=" << rewrites.size() << '\n';
    if (argc > 3) {
        std::ofstream out(argv[3]);
        Codegen(out, maxTicks).emit(result, rewrites, mode != "none");
    }
    return 0;
}
