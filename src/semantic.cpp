#include "semantic.h"

#include <ostream>
#include <stdexcept>
#include <utility>

namespace robotlang {
namespace {

SourceLocation location(const Token& token) {
    return {token.line, token.col};
}

template <typename T>
Located<T> located(Integer value, const Token& token) {
    return {T(value), location(token)};
}

} // namespace

const char* diagnosticCodeName(DiagnosticCode code) {
    switch (code) {
        case DiagnosticCode::InvalidTokenKind: return "invalid_token_kind";
        case DiagnosticCode::InvalidIntegerLiteral: return "invalid_integer_literal";
        case DiagnosticCode::IntegerOutOfRange: return "integer_out_of_range";
        case DiagnosticCode::NonPositiveValue: return "non_positive_value";
        case DiagnosticCode::InvalidAngle: return "invalid_angle";
        case DiagnosticCode::InvalidDirection: return "invalid_direction";
        case DiagnosticCode::DuplicateWorld: return "duplicate_world";
        case DiagnosticCode::DuplicateRobot: return "duplicate_robot";
        case DiagnosticCode::DeclarationOrder: return "declaration_order";
        case DiagnosticCode::NestedDeclaration: return "nested_declaration";
        case DiagnosticCode::MissingWorld: return "missing_world";
        case DiagnosticCode::MissingRobot: return "missing_robot";
        case DiagnosticCode::InitialPositionOutOfBounds: return "initial_position_out_of_bounds";
        case DiagnosticCode::UnmatchedRepeatEnd: return "unmatched_repeat_end";
        case DiagnosticCode::UnclosedRepeat: return "unclosed_repeat";
    }
    return "?";
}

std::ostream& operator<<(std::ostream& out, const Diagnostic& diagnostic) {
    out << diagnostic.source.line << ':' << diagnostic.source.col << ": "
        << (diagnostic.category == DiagnosticCategory::Semantic ? "semantic" : "contract")
        << '[' << diagnosticCodeName(diagnostic.code) << "] " << diagnostic.message;
    if (diagnostic.expectedType) out << " (" << domainTypeName(*diagnostic.expectedType) << ')';
    return out;
}

void SemanticAnalyzer::requireActive() const {
    if (finished) throw std::logic_error("SemanticAnalyzer has already been finished");
}

void SemanticAnalyzer::report(DiagnosticCategory category, DiagnosticCode code,
                              SourceLocation source, std::string message,
                              std::optional<DomainType> expectedType) {
    diagnostics.push_back({category, code, source, expectedType, std::move(message)});
}

bool SemanticAnalyzer::requireKind(const Token& token, TokenType expected,
                                   std::optional<DomainType> domain) {
    if (token.type == expected) return true;
    report(DiagnosticCategory::Contract, DiagnosticCode::InvalidTokenKind, location(token),
           "Adapter supplied an unexpected token kind", domain);
    return false;
}

std::optional<Integer> SemanticAnalyzer::integer(const Token& token, DomainType domain) {
    if (!requireKind(token, TokenType::NUM, domain)) return std::nullopt;
    if (token.lexeme.empty() || token.lexeme.find_first_not_of("0123456789") != std::string::npos) {
        report(DiagnosticCategory::Contract, DiagnosticCode::InvalidIntegerLiteral, location(token),
               "NUM must preserve a nonempty unsigned decimal lexeme", domain);
        return std::nullopt;
    }

    Integer value = 0;
    for (char character : token.lexeme) {
        const Integer digit = character - '0';
        // Check before multiplication or addition; even arbitrarily long input
        // cannot overflow the C++ accumulator.
        if (value > (MaxInteger - digit) / 10) {
            report(DiagnosticCategory::Semantic, DiagnosticCode::IntegerOutOfRange, location(token),
                   "Integer exceeds the MVP maximum 9007199254740991", domain);
            return std::nullopt;
        }
        value = value * 10 + digit;
    }

    if ((domain == DomainType::Dimension || domain == DomainType::Distance) && value == 0) {
        report(DiagnosticCategory::Semantic, DiagnosticCode::NonPositiveValue, location(token),
               "Value must be greater than zero", domain);
        return std::nullopt;
    }
    if (domain == DomainType::Angle && value != 90 && value != 180 && value != 270) {
        report(DiagnosticCategory::Semantic, DiagnosticCode::InvalidAngle, location(token),
               "Angle must be 90, 180 or 270", domain);
        return std::nullopt;
    }
    return value;
}

std::optional<Located<Direction>> SemanticAnalyzer::direction(const Token& token) {
    switch (token.type) {
        case TokenType::NORTH: return Located<Direction>{Direction::North, location(token)};
        case TokenType::EAST: return Located<Direction>{Direction::East, location(token)};
        case TokenType::SOUTH: return Located<Direction>{Direction::South, location(token)};
        case TokenType::WEST: return Located<Direction>{Direction::West, location(token)};
        default:
            report(DiagnosticCategory::Contract, DiagnosticCode::InvalidDirection, location(token),
                   "Direction must be a NORTH, EAST, SOUTH or WEST token", DomainType::Direction);
            return std::nullopt;
    }
}

bool SemanticAnalyzer::declaration(const Token& keyword, bool isWorld) {
    if (!repeats.empty()) {
        report(DiagnosticCategory::Semantic, DiagnosticCode::NestedDeclaration, location(keyword),
               "world and robot are only allowed at program scope");
        return false;
    }
    bool& seen = isWorld ? seenWorld : seenRobot;
    if (seen) {
        report(DiagnosticCategory::Semantic,
               isWorld ? DiagnosticCode::DuplicateWorld : DiagnosticCode::DuplicateRobot,
               location(keyword), "Declaration must be unique");
        return false;
    }
    const bool ordered = isWorld ? (!seenRobot && !seenCommand) : (seenWorld && !seenCommand);
    seen = true;
    if (!ordered) {
        report(DiagnosticCategory::Semantic, DiagnosticCode::DeclarationOrder, location(keyword),
               "Required order is world, robot, then commands");
    }
    return ordered;
}

void SemanticAnalyzer::command(const Token& keyword) {
    if (repeats.empty()) {
        seenCommand = true;
        if (!seenWorld || !seenRobot) {
            report(DiagnosticCategory::Semantic, DiagnosticCode::DeclarationOrder, location(keyword),
                   "Commands require preceding world and robot declarations");
        }
    }
}

ir::Origin SemanticAnalyzer::origin(const Token& token) {
    return {nextId++, location(token)};
}

void SemanticAnalyzer::append(ir::Instruction instruction) {
    auto& destination = repeats.empty() ? instructions : repeats.back().body;
    destination.push_back(std::move(instruction));
}

void SemanticAnalyzer::world(const Token& keyword, const Token& width, const Token& height) {
    requireActive();
    const bool kind = requireKind(keyword, TokenType::WORLD);
    const bool placement = declaration(keyword, true);
    const auto source = origin(keyword);
    const auto w = integer(width, DomainType::Dimension);
    const auto h = integer(height, DomainType::Dimension);
    if (kind && placement && w && h) {
        ir::WorldInit operation{located<Dimension>(*w, width), located<Dimension>(*h, height)};
        worldDimensions = operation;
        append({source, operation});
    }
}

void SemanticAnalyzer::robot(const Token& keyword, const Token& x,
                             const Token& y, const Token& facing) {
    requireActive();
    const bool kind = requireKind(keyword, TokenType::ROBOT);
    const bool placement = declaration(keyword, false);
    const auto source = origin(keyword);
    const auto xValue = integer(x, DomainType::Coordinate);
    const auto yValue = integer(y, DomainType::Coordinate);
    const auto orientation = direction(facing);
    bool inBounds = true;
    if (worldDimensions) {
        if (xValue && *xValue >= worldDimensions->width.value.value()) {
            report(DiagnosticCategory::Semantic, DiagnosticCode::InitialPositionOutOfBounds,
                   location(x), "Initial x must be less than world width", DomainType::Coordinate);
            inBounds = false;
        }
        if (yValue && *yValue >= worldDimensions->height.value.value()) {
            report(DiagnosticCategory::Semantic, DiagnosticCode::InitialPositionOutOfBounds,
                   location(y), "Initial y must be less than world height", DomainType::Coordinate);
            inBounds = false;
        }
    }
    if (kind && placement && xValue && yValue && orientation && inBounds) {
        append({source, ir::RobotInit{located<Coordinate>(*xValue, x),
                                     located<Coordinate>(*yValue, y), *orientation}});
    }
}

void SemanticAnalyzer::forward(const Token& keyword, const Token& distance) {
    requireActive();
    const bool kind = requireKind(keyword, TokenType::FORWARD);
    command(keyword);
    const auto source = origin(keyword);
    const auto value = integer(distance, DomainType::Distance);
    if (kind && value) append({source, ir::MoveForward{located<Distance>(*value, distance)}});
}

void SemanticAnalyzer::turn(const Token& keyword, const Token& angle, ir::TurnSense sense) {
    requireActive();
    const bool kind = requireKind(keyword, sense == ir::TurnSense::Left ? TokenType::LEFT : TokenType::RIGHT);
    command(keyword);
    const auto source = origin(keyword);
    const auto value = integer(angle, DomainType::Angle);
    if (kind && value) append({source, ir::Turn{sense, located<Angle>(*value, angle)}});
}

void SemanticAnalyzer::left(const Token& keyword, const Token& angle) {
    turn(keyword, angle, ir::TurnSense::Left);
}

void SemanticAnalyzer::right(const Token& keyword, const Token& angle) {
    turn(keyword, angle, ir::TurnSense::Right);
}

void SemanticAnalyzer::beginRepeat(const Token& keyword, const Token& count) {
    requireActive();
    const bool kind = requireKind(keyword, TokenType::REPEAT);
    command(keyword);
    const auto source = origin(keyword);
    const auto value = integer(count, DomainType::Count);
    std::optional<Located<Count>> typed;
    if (kind && value) typed = located<Count>(*value, count);
    // Keep the frame even on error, to check the body and balance callbacks.
    repeats.push_back({source, typed, {}});
}

void SemanticAnalyzer::endRepeat(const Token& closingBrace) {
    requireActive();
    requireKind(closingBrace, TokenType::RBRACE);
    if (repeats.empty()) {
        report(DiagnosticCategory::Contract, DiagnosticCode::UnmatchedRepeatEnd,
               location(closingBrace), "endRepeat has no matching beginRepeat");
        return;
    }
    auto frame = std::move(repeats.back());
    repeats.pop_back();
    if (frame.count) {
        append({frame.origin, ir::Repeat{*frame.count, std::move(frame.body), location(closingBrace)}});
    }
}

AnalysisResult SemanticAnalyzer::finish(const Token& end) {
    requireActive();
    requireKind(end, TokenType::END);
    for (const auto& frame : repeats) {
        report(DiagnosticCategory::Contract, DiagnosticCode::UnclosedRepeat,
               frame.origin.source, "beginRepeat was not followed by endRepeat");
    }
    if (!seenWorld) report(DiagnosticCategory::Semantic, DiagnosticCode::MissingWorld,
                            location(end), "Program requires one world declaration");
    if (!seenRobot) report(DiagnosticCategory::Semantic, DiagnosticCode::MissingRobot,
                            location(end), "Program requires one robot declaration");
    finished = true;
    AnalysisResult result;
    if (diagnostics.empty()) result.program = ir::Program{std::move(instructions), location(end)};
    result.diagnostics = std::move(diagnostics);
    return result;
}

} // namespace robotlang
