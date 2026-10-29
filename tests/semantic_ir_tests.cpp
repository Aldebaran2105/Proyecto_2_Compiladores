#include <functional>
#include <iostream>
#include <sstream>
#include <stdexcept>
#include <string>
#include <type_traits>
#include <utility>
#include <vector>

#include "scanner.h"
#include "semantic.h"

using namespace robotlang;

static_assert(!std::is_convertible_v<Distance, Angle>);
static_assert(!std::is_convertible_v<Count, Coordinate>);
static_assert(!std::is_convertible_v<Dimension, Distance>);
static_assert(!std::is_convertible_v<Integer, Count>);
static_assert(!std::is_same_v<Coordinate, Dimension>);

namespace {

void check(bool condition, const std::string& message) {
    if (!condition) throw std::runtime_error(message);
}

Token token(TokenType type, const std::string& lexeme, int line = 1, int col = 1) {
    return {type, lexeme, line, col};
}

Token num(const std::string& text, int line = 1, int col = 1) {
    return token(TokenType::NUM, text, line, col);
}

Token end() { return token(TokenType::END, "", 20, 1); }

void world(SemanticAnalyzer& analyzer, const std::string& width = "10",
           const std::string& height = "10") {
    analyzer.world(token(TokenType::WORLD, "world"), num(width, 1, 7), num(height, 1, 10));
}

void robot(SemanticAnalyzer& analyzer, const std::string& x = "2",
           const std::string& y = "2", TokenType direction = TokenType::NORTH) {
    analyzer.robot(token(TokenType::ROBOT, "robot", 2, 1), num(x, 2, 10), num(y, 2, 12),
                   token(direction, "north", 2, 21));
}

void prefix(SemanticAnalyzer& analyzer) { world(analyzer); robot(analyzer); }

void forward(SemanticAnalyzer& analyzer, const std::string& value = "1") {
    analyzer.forward(token(TokenType::FORWARD, "forward", 3, 1), num(value, 3, 9));
}

void begin(SemanticAnalyzer& analyzer, const std::string& value = "1", int line = 4) {
    analyzer.beginRepeat(token(TokenType::REPEAT, "repeat", line, 1), num(value, line, 8));
}

void close(SemanticAnalyzer& analyzer, int line = 8) {
    analyzer.endRepeat(token(TokenType::RBRACE, "}", line, 1));
}

const Diagnostic& error(const AnalysisResult& result, DiagnosticCode code) {
    check(!result.ok() && !result.program, "Invalid input must not publish partial IR");
    for (const auto& diagnostic : result.diagnostics) {
        if (diagnostic.code == code) return diagnostic;
    }
    throw std::runtime_error(std::string("Missing diagnostic: ") + diagnosticCodeName(code));
}

void validSquareAndSerialization() {
    // Scanner compatibility only: fixed callbacks, no grammar recognition.
    Scanner scanner("# square\nworld 10 10\nrobot at 2 2 facing north\n"
                    "repeat 4 {\n  forward 3\n  right 90\n}\n");
    const auto t = scanner.scanAll();
    check(t.size() == 18, "Unexpected original scanner output");
    SemanticAnalyzer analyzer;
    analyzer.world(t[0], t[1], t[2]);
    analyzer.robot(t[3], t[5], t[6], t[8]);
    analyzer.beginRepeat(t[9], t[10]);
    analyzer.forward(t[12], t[13]);
    analyzer.right(t[14], t[15]);
    analyzer.endRepeat(t[16]);
    const auto result = analyzer.finish(t[17]);
    check(result.ok(), "Square should be statically valid, regardless of movement bounds");
    const auto& code = result.program->instructions;
    check(code.size() == 3, "Root order/size changed");
    check(std::holds_alternative<ir::WorldInit>(code[0].operation), "Missing WorldInit");
    check(std::holds_alternative<ir::RobotInit>(code[1].operation), "Missing RobotInit");
    const auto& loop = std::get<ir::Repeat>(code[2].operation);
    check(loop.count.value.value() == 4 && loop.body.size() == 2, "Repeat was flattened");
    check(std::get<ir::MoveForward>(loop.body[0].operation).distance.value.value() == 3,
          "Distance was lost");
    check(std::get<ir::Turn>(loop.body[1].operation).sense == ir::TurnSense::Right,
          "Turn sense was lost");
    const std::string expected =
        "RobotIR v1\n"
        "#1 @2:1 WorldInit width=Dimension(10)@2:7 height=Dimension(10)@2:10\n"
        "#2 @3:1 RobotInit x=Coordinate(2)@3:10 y=Coordinate(2)@3:12 facing=Direction(north)@3:21\n"
        "#3 @4:1 Repeat count=Count(4)@4:8 {\n"
        "  #4 @5:3 MoveForward distance=Distance(3)@5:11\n"
        "  #5 @6:3 Turn sense=right angle=Angle(90)@6:9\n"
        "} @7:1\n"
        "End @8:1\n";
    check(ir::toString(*result.program) == expected, "Readable IR lost types/order/source metadata");
}

void allDirectionsAndAngles() {
    const std::vector<std::pair<TokenType, Direction>> directions = {
        {TokenType::NORTH, Direction::North}, {TokenType::EAST, Direction::East},
        {TokenType::SOUTH, Direction::South}, {TokenType::WEST, Direction::West}
    };
    for (const auto& entry : directions) {
        SemanticAnalyzer analyzer;
        world(analyzer, "1", "1");
        robot(analyzer, "0", "0", entry.first);
        for (const auto* angle : {"90", "180", "270"}) {
            analyzer.left(token(TokenType::LEFT, "left"), num(angle));
            analyzer.right(token(TokenType::RIGHT, "right"), num(angle));
        }
        const auto result = analyzer.finish(end());
        check(result.ok(), "Valid orientation/angle rejected");
        const auto& code = result.program->instructions;
        check(std::get<ir::RobotInit>(code[1].operation).facing.value == entry.second,
              "Wrong cardinal direction");
        for (std::size_t i = 2; i < code.size(); ++i) {
            const auto& turn = std::get<ir::Turn>(code[i].operation);
            check(turn.sense == (i % 2 == 0 ? ir::TurnSense::Left : ir::TurnSense::Right),
                  "Left/right collapsed");
        }
    }
}

void positiveDimensionsAndDistance() {
    for (int operand = 0; operand < 3; ++operand) {
        SemanticAnalyzer analyzer;
        world(analyzer, operand == 0 ? "0" : "10", operand == 1 ? "0" : "10");
        robot(analyzer);
        forward(analyzer, operand == 2 ? "0" : "1");
        const auto result = analyzer.finish(end());
        const auto& diagnostic = error(result, DiagnosticCode::NonPositiveValue);
        check(diagnostic.category == DiagnosticCategory::Semantic, "Value error misclassified");
        check(diagnostic.expectedType == (operand == 2 ? DomainType::Distance : DomainType::Dimension),
              "Lost operand domain");
    }
}

void initialBounds() {
    for (const auto& point : std::vector<std::pair<std::string, std::string>>{
             {"0", "0"}, {"9", "9"}, {"0", "9"}, {"9", "0"}}) {
        SemanticAnalyzer analyzer;
        world(analyzer);
        robot(analyzer, point.first, point.second);
        check(analyzer.finish(end()).ok(), "Valid boundary coordinate rejected");
    }
    for (int axis = 0; axis < 2; ++axis) {
        SemanticAnalyzer analyzer;
        world(analyzer);
        robot(analyzer, axis == 0 ? "10" : "0", axis == 1 ? "10" : "0");
        const auto result = analyzer.finish(end());
        const auto& diagnostic = error(result, DiagnosticCode::InitialPositionOutOfBounds);
        check(diagnostic.source.line == 2 && diagnostic.source.col == (axis == 0 ? 10 : 12),
              "Bounds error should point to the offending coordinate");
    }
}

void invalidAngles() {
    for (const auto* angle : {"0", "1", "89", "91", "179", "181", "269", "271", "360"}) {
        for (bool left : {false, true}) {
            SemanticAnalyzer analyzer;
            prefix(analyzer);
            if (left) analyzer.left(token(TokenType::LEFT, "left"), num(angle));
            else analyzer.right(token(TokenType::RIGHT, "right"), num(angle));
            error(analyzer.finish(end()), DiagnosticCode::InvalidAngle);
        }
    }
}

void integerLimits() {
    const std::string maximum = "9007199254740991";
    SemanticAnalyzer analyzer;
    world(analyzer, maximum, maximum);
    robot(analyzer, "9007199254740990", "9007199254740990");
    forward(analyzer, maximum);
    begin(analyzer, maximum);
    close(analyzer);
    check(analyzer.finish(end()).ok(), "Maximum safe integers rejected");

    // Overflow is checked for each operand role, before any arithmetic overflows.
    for (const auto& text : std::vector<std::string>{"9007199254740992", "9223372036854775807",
             "9223372036854775808", "18446744073709551616", std::string(10000, '9')}) {
        for (int operand = 0; operand < 7; ++operand) {
            SemanticAnalyzer candidate;
            world(candidate, operand == 0 ? text : "10", operand == 1 ? text : "10");
            robot(candidate, operand == 2 ? text : "0", operand == 3 ? text : "0");
            forward(candidate, operand == 4 ? text : "1");
            candidate.left(token(TokenType::LEFT, "left"), num(operand == 5 ? text : "90"));
            begin(candidate, operand == 6 ? text : "0");
            close(candidate);
            error(candidate.finish(end()), DiagnosticCode::IntegerOutOfRange);
        }
    }
    SemanticAnalyzer atLimit;
    world(atLimit, maximum, maximum);
    robot(atLimit, maximum, "0");
    error(atLimit.finish(end()), DiagnosticCode::InitialPositionOutOfBounds);
}

void leadingZerosAndMalformedNumbers() {
    SemanticAnalyzer valid;
    world(valid, "00010", "00010");
    robot(valid, "000", "001");
    forward(valid, std::string(10000, '0') + "1");
    begin(valid, "000");
    close(valid);
    check(valid.finish(end()).ok(), "Leading zeros should not cause overflow");

    for (const auto* text : {"", "-1", "+1", "1.0", "1x", " 1", "1 "}) {
        SemanticAnalyzer invalid;
        prefix(invalid);
        forward(invalid, text);
        const auto result = invalid.finish(end());
        check(error(result, DiagnosticCode::InvalidIntegerLiteral).category == DiagnosticCategory::Contract,
              "Malformed NUM is an adapter violation, not supported negative syntax");
    }
    Scanner negative("forward -1");
    const auto tokens = negative.scanAll();
    check(tokens[1].type == TokenType::ERR && tokens[2].type == TokenType::NUM,
          "Scanner negative syntax must remain unchanged");
}

void requiredUniqueOrderedDeclarations() {
    SemanticAnalyzer empty;
    const auto missing = empty.finish(end());
    check(missing.diagnostics.size() == 2, "Missing declarations should both be reported");
    error(missing, DiagnosticCode::MissingWorld);
    check(error(missing, DiagnosticCode::MissingRobot).source.line == 20, "Missing declaration EOF location");

    SemanticAnalyzer onlyWorld;
    world(onlyWorld);
    error(onlyWorld.finish(end()), DiagnosticCode::MissingRobot);
    SemanticAnalyzer onlyRobot;
    robot(onlyRobot);
    error(onlyRobot.finish(end()), DiagnosticCode::MissingWorld);

    SemanticAnalyzer duplicates;
    prefix(duplicates);
    world(duplicates);
    robot(duplicates);
    const auto repeated = duplicates.finish(end());
    error(repeated, DiagnosticCode::DuplicateWorld);
    error(repeated, DiagnosticCode::DuplicateRobot);

    SemanticAnalyzer reversed;
    robot(reversed);
    world(reversed);
    error(reversed.finish(end()), DiagnosticCode::DeclarationOrder);

    SemanticAnalyzer early;
    forward(early);
    prefix(early);
    error(early.finish(end()), DiagnosticCode::DeclarationOrder);

    SemanticAnalyzer between;
    world(between);
    begin(between, "0");
    close(between);
    robot(between);
    error(between.finish(end()), DiagnosticCode::DeclarationOrder);
}

void nestedDeclarationsAndZeroRepeat() {
    SemanticAnalyzer nested;
    prefix(nested);
    begin(nested, "0");
    world(nested);
    robot(nested);
    close(nested);
    const auto result = nested.finish(end());
    error(result, DiagnosticCode::NestedDeclaration);
    check(result.diagnostics.size() == 2, "Both nested declarations must be diagnosed");

    SemanticAnalyzer unreachable;
    prefix(unreachable);
    begin(unreachable, "0");
    forward(unreachable, "0");
    close(unreachable);
    error(unreachable.finish(end()), DiagnosticCode::NonPositiveValue);

    SemanticAnalyzer invalidCounter;
    prefix(invalidCounter);
    begin(invalidCounter, "9007199254740992");
    forward(invalidCounter, "0");
    close(invalidCounter);
    const auto bad = invalidCounter.finish(end());
    error(bad, DiagnosticCode::IntegerOutOfRange);
    error(bad, DiagnosticCode::NonPositiveValue);
}

struct RecordingVisitor : ir::ConstVisitor {
    std::vector<ir::InstructionId> ids;
    std::vector<ir::InstructionId> exits;
    void visit(const ir::Origin& origin, const ir::WorldInit&) override { ids.push_back(origin.id); }
    void visit(const ir::Origin& origin, const ir::RobotInit&) override { ids.push_back(origin.id); }
    void visit(const ir::Origin& origin, const ir::MoveForward&) override { ids.push_back(origin.id); }
    void visit(const ir::Origin& origin, const ir::Turn&) override { ids.push_back(origin.id); }
    void visit(const ir::Origin& origin, const ir::Repeat&) override { ids.push_back(origin.id); }
    void leave(const ir::Origin& origin, const ir::Repeat&) override { exits.push_back(origin.id); }
};

void nestedIrAndVisitor() {
    SemanticAnalyzer analyzer;
    prefix(analyzer);
    begin(analyzer, "0", 3);
    forward(analyzer, "1");
    begin(analyzer, "9007199254740991", 5);
    analyzer.left(token(TokenType::LEFT, "left", 6, 3), num("270", 6, 8));
    close(analyzer, 7);
    close(analyzer, 8);
    forward(analyzer, "2");
    const auto result = analyzer.finish(end());
    check(result.ok(), "Nested/zero/huge repeat should be represented without execution");
    const auto& outer = std::get<ir::Repeat>(result.program->instructions[2].operation);
    check(outer.count.value.value() == 0 && outer.body.size() == 2, "Zero loop erased");
    check(std::get<ir::Repeat>(outer.body[1].operation).closingBrace.line == 7,
          "Nested closing location lost");
    RecordingVisitor visitor;
    ir::walk(*result.program, visitor);
    check(visitor.ids == std::vector<ir::InstructionId>({1, 2, 3, 4, 5, 6, 7}),
          "Structural visitor lost source order or visited dynamic iterations");
    check(visitor.exits == std::vector<ir::InstructionId>({5, 3}), "Repeat exit order wrong");
    const auto copy = *result.program;
    check(ir::toString(copy) == ir::toString(*result.program), "IR copy changed provenance");
}

void movementBoundsRemainDynamic() {
    for (const auto direction : {TokenType::NORTH, TokenType::EAST, TokenType::SOUTH, TokenType::WEST}) {
        SemanticAnalyzer analyzer;
        world(analyzer, "1", "1");
        robot(analyzer, "0", "0", direction);
        forward(analyzer, "9007199254740991");
        check(analyzer.finish(end()).ok(), "Potential out-of-world movement rejected statically");
    }
}

void adapterErrorsAndLifecycle() {
    SemanticAnalyzer wrongOperand;
    prefix(wrongOperand);
    wrongOperand.forward(token(TokenType::FORWARD, "forward"), token(TokenType::NORTH, "north", 7, 9));
    const auto badOperand = wrongOperand.finish(end());
    const auto& diagnostic = error(badOperand, DiagnosticCode::InvalidTokenKind);
    check(diagnostic.category == DiagnosticCategory::Contract && diagnostic.expectedType == DomainType::Distance,
          "Wrong operand kind/domain was not diagnosed");
    check(diagnostic.source.line == 7 && diagnostic.source.col == 9, "Operand diagnostic location lost");
    std::ostringstream diagnosticText;
    diagnosticText << diagnostic;
    check(diagnosticText.str().find("7:9: contract[invalid_token_kind]") == 0,
          "Diagnostic serialization wrong");

    SemanticAnalyzer wrongDirection;
    world(wrongDirection);
    robot(wrongDirection, "0", "0", TokenType::NUM);
    error(wrongDirection.finish(end()), DiagnosticCode::InvalidDirection);

    SemanticAnalyzer wrongKeyword;
    prefix(wrongKeyword);
    wrongKeyword.left(token(TokenType::RIGHT, "right"), num("90"));
    error(wrongKeyword.finish(end()), DiagnosticCode::InvalidTokenKind);

    SemanticAnalyzer unmatched;
    prefix(unmatched);
    close(unmatched);
    error(unmatched.finish(end()), DiagnosticCode::UnmatchedRepeatEnd);

    SemanticAnalyzer unclosed;
    prefix(unclosed);
    begin(unclosed, "1", 4);
    begin(unclosed, "1", 5);
    const auto unclosedResult = unclosed.finish(end());
    check(unclosedResult.diagnostics.size() == 2, "All unclosed repeat callbacks must be reported");
    check(error(unclosedResult, DiagnosticCode::UnclosedRepeat).source.line == 4,
          "Unclosed repeat should point to its opener");

    SemanticAnalyzer wrongEnd;
    prefix(wrongEnd);
    error(wrongEnd.finish(num("1")), DiagnosticCode::InvalidTokenKind);

    SemanticAnalyzer singleUse;
    prefix(singleUse);
    check(singleUse.finish(end()).ok(), "Initial finish failed");
    bool finishThrew = false;
    bool writeThrew = false;
    try { singleUse.finish(end()); } catch (const std::logic_error&) { finishThrew = true; }
    try { forward(singleUse); } catch (const std::logic_error&) { writeThrew = true; }
    check(finishThrew && writeThrew, "Analyzer must reject reuse after finish");
}

void multipleDiagnosticsNoPartialIr() {
    SemanticAnalyzer analyzer;
    world(analyzer, "0", "0");
    robot(analyzer);
    forward(analyzer, "0");
    analyzer.right(token(TokenType::RIGHT, "right", 9, 1), num("45", 9, 7));
    const auto result = analyzer.finish(end());
    check(result.diagnostics.size() == 4 && !result.program,
          "Must accumulate independent errors without publishing partial IR or cascading bounds errors");
    check(result.diagnostics[3].source.line == 9 && result.diagnostics[3].source.col == 7,
          "Last diagnostic source lost");
}

} // namespace

int main() {
    const std::vector<std::pair<const char*, std::function<void()>>> tests = {
        {"valid square and serialization", validSquareAndSerialization},
        {"all directions and angles", allDirectionsAndAngles},
        {"positive dimensions and distance", positiveDimensionsAndDistance},
        {"initial bounds", initialBounds},
        {"invalid angles", invalidAngles},
        {"integer limits for every operand", integerLimits},
        {"leading zeros and malformed numbers", leadingZerosAndMalformedNumbers},
        {"required unique ordered declarations", requiredUniqueOrderedDeclarations},
        {"nested declarations and zero repeat", nestedDeclarationsAndZeroRepeat},
        {"nested IR and structural visitor", nestedIrAndVisitor},
        {"movement bounds remain dynamic", movementBoundsRemainDynamic},
        {"adapter errors and lifecycle", adapterErrorsAndLifecycle},
        {"multiple diagnostics without partial IR", multipleDiagnosticsNoPartialIr}
    };
    int failed = 0;
    for (const auto& test : tests) {
        try {
            test.second();
            std::cout << "PASS " << test.first << '\n';
        } catch (const std::exception& exception) {
            ++failed;
            std::cerr << "FAIL " << test.first << ": " << exception.what() << '\n';
        }
    }
    std::cout << tests.size() - failed << '/' << tests.size() << " test groups passed\n";
    return failed == 0 ? 0 : 1;
}
