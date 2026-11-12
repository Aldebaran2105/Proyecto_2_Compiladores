// Implementación manual en C++ equivalente a square_big.robot
#include <cstdio>

int main() {
    const long W = 10, H = 10;
    long x = 2, y = 2;
    int dir = 0;  // 0 norte, 1 este, 2 sur, 3 oeste
    const int dx[] = {0, 1, 0, -1}, dy[] = {1, 0, -1, 0};
    const char* names[] = {"north", "east", "south", "west"};
    for (long i = 0; i < 1000000; ++i) {
        for (int s = 0; s < 3; ++s) {  // forward 3, casilla por casilla
            long nx = x + dx[dir], ny = y + dy[dir];
            if (nx < 0 || ny < 0 || nx >= W || ny >= H) break;
            x = nx; y = ny;
        }
        dir = (dir + 1) % 4;  // right 90
    }
    std::printf("r1: (%ld, %ld) %s\n", x, y, names[dir]);
    return 0;
}
