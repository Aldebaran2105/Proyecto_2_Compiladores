#ifndef ROBOTLANG_SEMANTIC_H
#define ROBOTLANG_SEMANTIC_H

#include <iosfwd>
#include <optional>
#include <string>
#include <vector>

#include "ir.h"
#include "token.h"

namespace robotlang {

enum class DiagnosticCategory { Semantic, Contract };

enum class DiagnosticCode {
    InvalidTokenKind,
    InvalidIntegerLiteral,
    IntegerOutOfRange,
    NonPositiveValue,
    InvalidAngle,
    InvalidDirection,
    DuplicateWorld,
    DuplicateRobot,
    DeclarationOrder,
    NestedDeclaration,
    MissingWorld,
    MissingRobot,
    InitialPositionOutOfBounds,
    UnmatchedRepeatEnd,
    UnclosedRepeat
};

const char* diagnosticCodeName(DiagnosticCode code);

struct Diagnostic {
    DiagnosticCategory category;
    DiagnosticCode code;
    SourceLocation source;
    std::optional<DomainType> expectedType;
    std::string message;
};

std::ostream& operator<<(std::ostream& out, const Diagnostic& diagnostic);

struct AnalysisResult {
    std::optional<ir::Program> program;
    std::vector<Diagnostic> diagnostics;
    bool ok() const { return program.has_value() && diagnostics.empty(); }
};

// Adapter boundary, NOT a Parser or an AST. A future AST visitor invokes these
// callbacks in source order, preserving raw operand tokens and nested blocks.
// One analyzer per program; finish consumes it. Later calls throw logic_error.
class SemanticAnalyzer {
public:
    void world(const Token& keyword, const Token& width, const Token& height);
    void robot(const Token& keyword, const Token& x, const Token& y, const Token& facing);
    void forward(const Token& keyword, const Token& distance);
    void left(const Token& keyword, const Token& angle);
    void right(const Token& keyword, const Token& angle);
    void beginRepeat(const Token& keyword, const Token& count);
    void endRepeat(const Token& closingBrace);
    AnalysisResult finish(const Token& end);

private:
    struct RepeatFrame {
        ir::Origin origin;
        std::optional<Located<Count>> count;
        std::vector<ir::Instruction> body;
    };

    bool finished = false;
    bool seenWorld = false;
    bool seenRobot = false;
    bool seenCommand = false;
    ir::InstructionId nextId = 1;
    std::optional<ir::WorldInit> worldDimensions;
    std::vector<ir::Instruction> instructions;
    std::vector<RepeatFrame> repeats;
    std::vector<Diagnostic> diagnostics;

    void requireActive() const;
    void report(DiagnosticCategory category, DiagnosticCode code, SourceLocation source,
                std::string message, std::optional<DomainType> expectedType = std::nullopt);
    bool requireKind(const Token& token, TokenType expected,
                     std::optional<DomainType> domain = std::nullopt);
    std::optional<Integer> integer(const Token& token, DomainType domain);
    std::optional<Located<Direction>> direction(const Token& token);
    bool declaration(const Token& keyword, bool isWorld);
    void command(const Token& keyword);
    ir::Origin origin(const Token& token);
    void append(ir::Instruction instruction);
    void turn(const Token& keyword, const Token& angle, ir::TurnSense sense);
};

} // namespace robotlang

#endif
