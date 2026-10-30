#ifndef ROBOTLANG_SEMANTIC_H
#define ROBOTLANG_SEMANTIC_H

#include "ir.h"

namespace robotlang::extension {
struct Diagnostic {
    SourceLocation source;
    std::string code;
    std::string message;
};

struct AnalysisResult {
    std::optional<Program> program;
    std::vector<Diagnostic> diagnostics;
};

class SemanticAnalyzer {
public:
    AnalysisResult analyze(Program program) const;
};
}
#endif
