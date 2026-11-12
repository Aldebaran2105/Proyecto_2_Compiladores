// Implementación manual en C++ equivalente a redundant.robot
#include <cstdio>

static const long W = 20, H = 20;
static long x = 0, y = 0;
static int dir = 1;  // este
static const int dx[] = {0, 1, 0, -1}, dy[] = {1, 0, -1, 0};

static void forward(long n) {
    for (long s = 0; s < n; ++s) {
        long nx = x + dx[dir], ny = y + dy[dir];
        if (nx < 0 || ny < 0 || nx >= W || ny >= H) return;
        x = nx; y = ny;
    }
}
static void turn(int degrees) { dir = ((dir + degrees / 90) % 4 + 4) % 4; }

int main() {
    const char* names[] = {"north", "east", "south", "west"};
    for (long i = 0; i < 250000; ++i) {
        forward(1); forward(1); forward(1);
        turn(90); turn(90);
        forward(1); forward(1); forward(1);
        turn(90); turn(-90); turn(90); turn(90);
    }
    std::printf("r1: (%ld, %ld) %s\n", x, y, names[dir]);
    return 0;
}
