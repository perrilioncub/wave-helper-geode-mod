// Native unit tests for the mirror geometry. Build and run with:
//   cmake -S tests -B build-tests && cmake --build build-tests && ./build-tests/mirror-math-test

#include "../src/MirrorMath.hpp"

#include <cstdio>
#include <cstdlib>
#include <string>

using namespace wave_helper;

static int g_failures = 0;
static int g_checks = 0;

static void expectNear(double actual, double expected, std::string const& what) {
    g_checks++;
    if (std::abs(actual - expected) > 1e-6) {
        g_failures++;
        std::printf("FAIL: %s: expected %.4f, got %.4f\n", what.c_str(), expected, actual);
    }
}

static void expectTrue(bool value, std::string const& what) {
    g_checks++;
    if (!value) {
        g_failures++;
        std::printf("FAIL: %s\n", what.c_str());
    }
}

// A GD block is 30 units. Helpers build shapes from a cell's bottom-left corner.
static Shape block(double x, double y, double w = 30, double h = 30) {
    return { x, y, x + w, y + h, Corner::None };
}
static Shape slope(double x, double y, double w, double h, Corner corner) {
    return { x, y, x + w, y + h, corner };
}
static double cx(Shape const& s) { return (s.minX + s.maxX) / 2; }
static double cy(Shape const& s) { return (s.minY + s.maxY) / 2; }

// What rotating an object 180 degrees in place does to a slope's solid corner.
static Corner rotated180(Corner c) {
    switch (c) {
        case Corner::BottomLeft: return Corner::TopRight;
        case Corner::BottomRight: return Corner::TopLeft;
        case Corner::TopRight: return Corner::BottomLeft;
        case Corner::TopLeft: return Corner::BottomRight;
        default: return Corner::None;
    }
}

// Builds the ceiling the mod would create, as shapes.
static std::vector<Shape> buildCeiling(std::vector<Shape> const& floor, SurfaceMirror const& mirror) {
    std::vector<Shape> out;
    for (auto const& s : floor) {
        double newY = mirror.mapY(cx(s), cy(s));
        double h = s.height() / 2;
        out.push_back({ s.minX, newY - h, s.maxX, newY + h, rotated180(s.corner) });
    }
    return out;
}

static void testFlatFloor() {
    std::vector<Shape> floor = { block(0, 0), block(0, -30), block(30, 0) };
    SurfaceMirror m(floor);
    expectNear(m.mapY(15, 15), 45, "flat: top block mirrors across its top edge");
    expectNear(m.mapY(15, -15), 75, "flat: block below goes one further up");
    expectNear(m.mapY(45, 15), 45, "flat: neighbour block");
    expectNear(m.mapY(15, 40), 20, "flat: decoration above surface goes below it");
}

static void testRisingStaircase1x1() {
    // Two 1x1 slopes rising to the right, each with a fill block underneath the second.
    std::vector<Shape> floor = {
        slope(0, 0, 30, 30, Corner::BottomRight),
        slope(30, 30, 30, 30, Corner::BottomRight),
        block(30, 0),
    };
    SurfaceMirror m(floor);
    expectNear(m.mapY(15, 15), 15, "1x1: slope stays in place");
    expectNear(m.mapY(45, 45), 45, "1x1: second slope stays in place");
    expectNear(m.mapY(45, 15), 75, "1x1: fill block goes directly above the slope");
    expectNear(m.mapY(15, 25), 5, "1x1: decoration 10 above the line goes 10 below it");
}

static void testFalling2x1() {
    // A 2x1 slope falling to the right, with two fill blocks below it.
    std::vector<Shape> floor = {
        slope(0, 30, 60, 30, Corner::BottomLeft),
        block(0, 0),
        block(30, 0),
    };
    SurfaceMirror m(floor);
    expectNear(m.mapY(30, 45), 45, "2x1: slope stays in place");
    expectNear(m.mapY(15, 15), 75, "2x1: left fill block lands on the grid above the slope");
    expectNear(m.mapY(45, 15), 75, "2x1: right fill block lands on the grid above the slope");
    // Line at x=15 is at 52.5. A decoration 5 above it should end up 5 below.
    expectNear(m.mapY(15, 57.5), 47.5, "2x1: decoration mirrors across the diagonal");
}

static void testHalfSlope() {
    // A 1x0.5 slope sitting on a block.
    std::vector<Shape> floor = {
        slope(0, 30, 30, 15, Corner::BottomRight),
        block(0, 0),
    };
    SurfaceMirror m(floor);
    expectNear(m.mapY(15, 37.5), 37.5, "half: slope stays in place");
    expectNear(m.mapY(15, 15), 60, "half: block goes directly above the half slope's box");
}

static void testOffGrid() {
    double dx = 7.3, dy = 11.1;
    std::vector<Shape> floor = {
        slope(0 + dx, 30 + dy, 60, 30, Corner::BottomLeft),
        block(0 + dx, 0 + dy),
        block(30 + dx, 0 + dy),
    };
    SurfaceMirror m(floor);
    expectNear(m.mapY(30 + dx, 45 + dy), 45 + dy, "off-grid: slope stays in place");
    expectNear(m.mapY(15 + dx, 15 + dy), 75 + dy, "off-grid: fill block keeps the same offset");
}

static void testOverhangUsesNearestShape() {
    std::vector<Shape> floor = { block(0, 0) };
    SurfaceMirror m(floor);
    expectNear(m.mapY(50, 40), 20, "overhang: object past the edge uses the nearest block");
}

static void testDetectDirection() {
    std::vector<Shape> floor = { slope(0, 0, 30, 30, Corner::BottomRight), block(30, 0) };
    std::vector<Shape> ceiling = { slope(0, 0, 30, 30, Corner::TopLeft), slope(30, 0, 30, 30, Corner::TopRight) };
    std::vector<Shape> flat = { block(0, 0) };
    expectTrue(SurfaceMirror::detectDirection(floor) == Direction::FloorToCeiling, "detect: floor");
    expectTrue(SurfaceMirror::detectDirection(ceiling) == Direction::CeilingToFloor, "detect: ceiling");
    expectTrue(SurfaceMirror::detectDirection(flat) == Direction::FloorToCeiling, "detect: flat defaults to floor");
}

// Mirroring a floor into a ceiling and then mirroring that ceiling back into a
// floor must land every object exactly where it started.
static void testRoundTrip() {
    std::vector<Shape> floor = {
        slope(0, 0, 30, 30, Corner::BottomRight),
        slope(30, 30, 60, 30, Corner::BottomRight),
        block(30, 0), block(60, 0),
        block(90, 30), block(90, 0),
        slope(120, 0, 30, 30, Corner::BottomLeft),
        slope(150, -15, 60, 15, Corner::BottomLeft),
        block(150, -45), block(180, -45),
    };
    SurfaceMirror up(floor, Direction::FloorToCeiling);
    auto ceiling = buildCeiling(floor, up);

    expectTrue(SurfaceMirror::detectDirection(ceiling) == Direction::CeilingToFloor, "round trip: ceiling detected");
    SurfaceMirror down(ceiling, Direction::CeilingToFloor);
    for (size_t i = 0; i < floor.size(); i++) {
        auto const& c = ceiling[i];
        expectNear(down.mapY(cx(c), cy(c)), cy(floor[i]), "round trip: object " + std::to_string(i));
    }

    // The ceiling's surface must follow the floor's exactly: the bottom of
    // the ceiling at each x equals the top of the floor there.
    for (double x = 1; x < 210; x += 3) {
        double floorTop = -1e9, ceilBottom = 1e9;
        for (auto const& s : floor) {
            if (x < s.minX || x > s.maxX) continue;
            floorTop = std::max(floorTop, s.topAt(x));
        }
        for (auto const& s : ceiling) {
            if (x < s.minX || x > s.maxX) continue;
            double bottom = (s.corner == Corner::TopLeft || s.corner == Corner::TopRight) ? s.diagonalAt(x) : s.minY;
            ceilBottom = std::min(ceilBottom, bottom);
        }
        expectNear(ceilBottom, floorTop, "round trip: ceiling hugs floor at x=" + std::to_string(x));
    }
}

int main() {
    testFlatFloor();
    testRisingStaircase1x1();
    testFalling2x1();
    testHalfSlope();
    testOffGrid();
    testOverhangUsesNearestShape();
    testDetectDirection();
    testRoundTrip();
    std::printf("%d/%d checks passed\n", g_checks - g_failures, g_checks);
    return g_failures == 0 ? EXIT_SUCCESS : EXIT_FAILURE;
}
