// Implementación manual en C++ equivalente a examples/patrol.robot:
// mundo con muro y hueco, dos robots con prioridad que se mueven por ticks.
#include <cstdio>
#include <string>
#include <vector>

struct Cmd { char kind; long arg; };  // 'F' avanzar, 'R' derecha, 'L' izquierda
struct Robot {
    std::string id;
    long x, y;
    int dir;  // 0 norte, 1 este, 2 sur, 3 oeste
    std::vector<Cmd> program;
    size_t pc = 0;
    bool fallen = false;
};

const long W = 10, H = 8;
const int dx[] = {0, 1, 0, -1}, dy[] = {1, 0, -1, 0};
const char* names[] = {"north", "east", "south", "west"};

bool wall(long x, long y) { return x >= 4 && x < 6 && y >= 3 && y < 5; }
bool hole(long x, long y) { return x == 7 && y == 2; }

bool occupied(const std::vector<Robot>& robots, size_t self, long x, long y) {
    for (size_t j = 0; j < robots.size(); ++j)
        if (j != self && !robots[j].fallen && robots[j].x == x && robots[j].y == y) return true;
    return false;
}

void forward(std::vector<Robot>& robots, size_t i, long n) {
    Robot& r = robots[i];
    for (long s = 0; s < n; ++s) {
        long nx = r.x + dx[r.dir], ny = r.y + dy[r.dir];
        if (nx < 0 || ny < 0 || nx >= W || ny >= H || wall(nx, ny) || occupied(robots, i, nx, ny)) return;
        r.x = nx; r.y = ny;
        if (hole(nx, ny)) { r.fallen = true; return; }
    }
}

int main() {
    // Ordenados por prioridad: r1 (1) y luego r2 (2)
    std::vector<Robot> robots = {
        {"r1", 1, 1, 1, {{'F', 3}, {'R', 90}, {'F', 2}}},
        {"r2", 8, 1, 3, {{'F', 6}}},
    };
    bool active = true;
    while (active) {
        active = false;
        for (size_t i = 0; i < robots.size(); ++i) {
            Robot& r = robots[i];
            if (r.fallen || r.pc == r.program.size()) continue;
            active = true;
            Cmd c = r.program[r.pc++];
            if (c.kind == 'F') forward(robots, i, c.arg);
            else if (c.kind == 'R') r.dir = (r.dir + c.arg / 90) % 4;
            else r.dir = ((r.dir - c.arg / 90) % 4 + 4) % 4;
        }
    }
    for (const Robot& r : robots) {
        if (r.fallen) std::printf("%s: fallen at (%ld, %ld)\n", r.id.c_str(), r.x, r.y);
        else std::printf("%s: (%ld, %ld) %s\n", r.id.c_str(), r.x, r.y, names[r.dir]);
    }
    return 0;
}
