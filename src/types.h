#ifndef ROBOTLANG_TYPES_H
#define ROBOTLANG_TYPES_H

#include <cstdint>

namespace robotlang {

using Integer = std::int64_t;
inline constexpr Integer MaxInteger = 9007199254740991LL;

struct SourceLocation {
    int line = 1;
    int col = 1;
};

enum class DomainType { Distance, Angle, Count, Coordinate, Dimension, Direction };

constexpr const char* domainTypeName(DomainType type) {
    switch (type) {
        case DomainType::Distance: return "Distance";
        case DomainType::Angle: return "Angle";
        case DomainType::Count: return "Count";
        case DomainType::Coordinate: return "Coordinate";
        case DomainType::Dimension: return "Dimension";
        case DomainType::Direction: return "Direction";
    }
    return "?";
}

template <DomainType Kind>
class DomainInteger {
public:
    explicit constexpr DomainInteger(Integer value) : number(value) {}
    constexpr Integer value() const { return number; }
    static constexpr DomainType type = Kind;

private:
    Integer number;
};

using Distance = DomainInteger<DomainType::Distance>;
using Angle = DomainInteger<DomainType::Angle>;
using Count = DomainInteger<DomainType::Count>;
using Coordinate = DomainInteger<DomainType::Coordinate>;
using Dimension = DomainInteger<DomainType::Dimension>;

enum class Direction { North, East, South, West };

constexpr const char* directionName(Direction direction) {
    switch (direction) {
        case Direction::North: return "north";
        case Direction::East: return "east";
        case Direction::South: return "south";
        case Direction::West: return "west";
    }
    return "?";
}

template <typename T>
struct Located {
    T value;
    SourceLocation source;
};

}

#endif
