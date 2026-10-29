#include "parser.h"

#include <limits>
#include <stdexcept>
#include <string>
#include <utility>
#include <vector>

Parser::Parser(Scanner& scanner)
    : scanner_(scanner), current_(scanner_.nextToken()) {
    if (current_.type == TokenType::ERR) {
        fail(current_, "error lexico: '" + current_.lexeme + "'");
    }
}

void Parser::advance() {
    current_ = scanner_.nextToken();
    if (current_.type == TokenType::ERR) {
        fail(current_, "error lexico: '" + current_.lexeme + "'");
    }
}

[[noreturn]] void Parser::fail(const Token& token,
                               const std::string& message) const {
    throw std::runtime_error(std::to_string(token.line) + ':' +
                             std::to_string(token.col) + ": " + message);
}

Token Parser::expect(TokenType type, const std::string& message) {
    if (current_.type != type) fail(current_, message);
    Token matched = current_;
    advance();
    return matched;
}

int Parser::parseNumber(const std::string& message) {
    const Token token = expect(TokenType::NUM, message);
    try {
        std::size_t used = 0;
        const long long number = std::stoll(token.lexeme, &used, 10);
        if (used != token.lexeme.size() || number < 0 ||
            number > std::numeric_limits<int>::max()) {
            fail(token, "numero fuera del rango permitido");
        }
        return static_cast<int>(number);
    } catch (const std::invalid_argument&) {
        fail(token, "numero invalido: '" + token.lexeme + "'");
    } catch (const std::out_of_range&) {
        fail(token, "numero fuera del rango permitido");
    }
}

std::unique_ptr<Program> Parser::parseProgram() {
    auto world = parseWorld();
    const int line = world->line;
    const int col = world->col;
    auto robot = parseRobot();
    std::vector<std::unique_ptr<Command>> commands;
    while (current_.type != TokenType::END) {
        commands.push_back(parseCommand());
    }
    return std::make_unique<Program>(std::move(world), std::move(robot),
                                     std::move(commands), line, col);
}

std::unique_ptr<WorldDecl> Parser::parseWorld() {
    const Token start = expect(TokenType::WORLD,
                               "se esperaba 'world' al inicio del programa");
    const Token w = current_;
    const int width = parseNumber("se esperaba NUM para el ancho de 'world'");
    const Token h = current_;
    const int height = parseNumber("se esperaba NUM para el alto de 'world'");
    if (width <= 0) fail(w, "el ancho de 'world' debe ser mayor que 0");
    if (height <= 0) fail(h, "el alto de 'world' debe ser mayor que 0");
    return std::make_unique<WorldDecl>(width, height, start.line, start.col);
}

std::unique_ptr<RobotDecl> Parser::parseRobot() {
    const Token start = expect(TokenType::ROBOT,
                               "se esperaba 'robot' despues de 'world'");
    expect(TokenType::AT, "se esperaba 'at' despues de 'robot'");
    const int x = parseNumber("se esperaba NUM para la coordenada x");
    const int y = parseNumber("se esperaba NUM para la coordenada y");
    expect(TokenType::FACING, "se esperaba 'facing' despues de las coordenadas");

    Direction facing;
    switch (current_.type) {
        case TokenType::NORTH: facing = Direction::North; break;
        case TokenType::SOUTH: facing = Direction::South; break;
        case TokenType::EAST: facing = Direction::East; break;
        case TokenType::WEST: facing = Direction::West; break;
        default: fail(current_, "se esperaba una direccion: north, south, east o west");
    }
    advance();
    return std::make_unique<RobotDecl>(x, y, facing, start.line, start.col);
}

std::unique_ptr<Command> Parser::parseCommand() {
    switch (current_.type) {
        case TokenType::FORWARD: return parseForward();
        case TokenType::RIGHT:
        case TokenType::LEFT: return parseTurn();
        case TokenType::REPEAT: return parseRepeat();
        default: fail(current_, "se esperaba un comando: forward, right, left o repeat");
    }
}

std::unique_ptr<ForwardCmd> Parser::parseForward() {
    const Token start = expect(TokenType::FORWARD, "se esperaba 'forward'");
    const Token numberToken = current_;
    const int distance = parseNumber("se esperaba NUM despues de 'forward'");
    if (distance <= 0) fail(numberToken, "la distancia de 'forward' debe ser mayor que 0");
    return std::make_unique<ForwardCmd>(distance, start.line, start.col);
}

std::unique_ptr<TurnCmd> Parser::parseTurn() {
    const Token start = current_;
    const TurnSide side = current_.type == TokenType::RIGHT
                              ? TurnSide::Right : TurnSide::Left;
    advance();
    const Token numberToken = current_;
    const int degrees = parseNumber("se esperaba NUM despues del giro");
    if (degrees != 90 && degrees != 180 && degrees != 270) {
        fail(numberToken, "el giro debe ser 90, 180 o 270 grados");
    }
    return std::make_unique<TurnCmd>(side, degrees, start.line, start.col);
}

std::unique_ptr<RepeatCmd> Parser::parseRepeat() {
    const Token start = expect(TokenType::REPEAT, "se esperaba 'repeat'");
    const Token numberToken = current_;
    const int times = parseNumber("se esperaba NUM despues de 'repeat'");
    if (times <= 0) fail(numberToken, "'repeat' debe ser mayor que 0");
    expect(TokenType::LBRACE, "se esperaba '{' despues del numero de repeticiones");

    std::vector<std::unique_ptr<Command>> commands;
    while (current_.type != TokenType::RBRACE) {
        if (current_.type == TokenType::END) {
            fail(current_, "se esperaba '}' para cerrar 'repeat'");
        }
        commands.push_back(parseCommand());
    }
    expect(TokenType::RBRACE, "se esperaba '}' para cerrar 'repeat'");
    return std::make_unique<RepeatCmd>(times, std::move(commands),
                                       start.line, start.col);
}
