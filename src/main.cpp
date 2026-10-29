#include "semantic.h"
#include "ast_semantic.h"
#include "codegen.h"
#include "optimizer.h"
#include "parser.h"
#include "simulator.h"
#include <fstream>
#include <iostream>
#include <sstream>
#include <stdexcept>
#include <string>

int main(int argc, char* argv[]) {
    using namespace robotlang::extension;
    if (argc < 2) {
        std::cerr << "Uso: robotc archivo.robot [--tokens|--ast|--ir|--run|--grid|--trace|--emit RUTA] [--optimize] [--max-ticks N]\n";
        return 1;
    }
    std::string path = argv[1], mode, outputPath;
    bool optimize = false;
    robotlang::Integer maxTicks = 100000;
    for (int i = 2; i < argc; ++i) {
        std::string arg = argv[i];
        if (arg == "--optimize") optimize = true;
        else if (arg == "--emit" && i + 1 < argc) {
            if (!mode.empty()) { std::cerr << "se requiere un solo modo\n"; return 1; }
            mode = arg;
            outputPath = argv[++i];
        }
        else if (arg == "--max-ticks" && i + 1 < argc) {
            try {
                std::string text = argv[++i];
                std::size_t used = 0;
                maxTicks = std::stoll(text, &used);
                if (used != text.size() || maxTicks <= 0 || maxTicks > robotlang::MaxInteger) throw std::out_of_range("ticks");
            } catch (const std::exception&) { std::cerr << "limite de ticks invalido\n"; return 1; }
        } else if (arg == "--tokens" || arg == "--ast" || arg == "--ir" || arg == "--run" || arg == "--grid" || arg == "--trace") {
            if (!mode.empty()) { std::cerr << "se requiere un solo modo\n"; return 1; }
            mode = arg;
        } else { std::cerr << "opcion desconocida: " << arg << '\n'; return 1; }
    }
    if (mode.empty()) mode = "--run";
    if (optimize && mode != "--ir" && mode != "--emit") { std::cerr << "--optimize requiere --ir o --emit\n"; return 1; }
    std::ifstream file(path);
    if (!file) { std::cerr << "no se pudo abrir: " << path << '\n'; return 1; }
    std::ostringstream input;
    input << file.rdbuf();
    Scanner scanner(input.str());
    if (mode == "--tokens") {
        bool error = false;
        for (const Token& token : scanner.scanAll()) {
            std::cout << token << '\n';
            if (token.type == TokenType::ERR) { std::cerr << path << ':' << token.line << ':' << token.col << ": error lexico\n"; error = true; }
        }
        return error ? 1 : 0;
    }
    try {
        robotlang::Parser parser(scanner);
        auto tree = parser.parseProgram();
        if (mode == "--ast") { robotlang::ast::PrintVisitor visitor(std::cout); tree->accept(visitor); return 0; }
        AnalysisResult analysis = robotlang::SemanticVisitor().analyze(*tree);
        if (!analysis.program) {
            for (const auto& error : analysis.diagnostics)
                std::cerr << path << ':' << error.source.line << ':' << error.source.col << ": " << error.code << ": " << error.message << '\n';
            return 1;
        }
        Program program = std::move(*analysis.program);
        std::vector<Rewrite> rewrites;
        if (optimize) {
            auto result = Optimizer(maxTicks).optimize(program);
            program = std::move(result.program);
            rewrites = std::move(result.rewrites);
        }
        if (mode == "--ir") { printIr(program, std::cout); return 0; }
        if (mode == "--emit") {
            std::ofstream output(outputPath);
            if (!output) { std::cerr << "no se pudo escribir: " << outputPath << '\n'; return 1; }
            Codegen(output, maxTicks).emit(program, rewrites, optimize);
            if (!output) { std::cerr << "error al escribir: " << outputPath << '\n'; return 1; }
            return 0;
        }
        SimulationResult result = Simulator(program, maxTicks).run(mode == "--trace");
        if (result.limitReached) { std::cerr << path << ": limite de ticks excedido\n"; return 1; }
        if (mode == "--grid") printGrid(program, result, std::cout);
        else if (mode == "--trace") printTrace(result, std::cout);
        else printResult(result, std::cout);
    } catch (const std::exception& error) {
        std::cerr << path << ':' << error.what() << '\n';
        return 1;
    }
    return 0;
}
