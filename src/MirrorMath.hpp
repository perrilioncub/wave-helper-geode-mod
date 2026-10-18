#pragma once

// Pure geometry for "Mirror to Ceiling". Nothing in here touches Geometry Dash,
// so it can be unit tested on any machine (see tests/MirrorMathTest.cpp).
//
// The idea: a wave floor is a set of solid shapes (blocks and slopes). Its top
// outline is the "surface". To build the matching ceiling we keep every
// object's X position and remap its Y so that whatever was *below* the surface
// ends up *above* it, and vice versa. Slopes are rotated 180 degrees in place,
// which keeps the diagonal exactly where it was but moves the solid half to
// the other side of it.

#include <algorithm>
#include <cmath>
#include <limits>
#include <optional>
#include <vector>

namespace wave_helper {

// Which corner of a slope's bounding box holds the right angle (the solid
// corner). `None` means the shape is a plain rectangle.
enum class Corner { BottomLeft, BottomRight, TopRight, TopLeft, None };

inline bool isBottomCorner(Corner c) {
    return c == Corner::BottomLeft || c == Corner::BottomRight;
}

inline Corner flipCornerVertically(Corner c) {
    switch (c) {
        case Corner::BottomLeft: return Corner::TopLeft;
        case Corner::BottomRight: return Corner::TopRight;
        case Corner::TopRight: return Corner::BottomRight;
        case Corner::TopLeft: return Corner::BottomLeft;
        default: return Corner::None;
    }
}

// An axis-aligned hitbox. Slopes use their bounding box plus the solid corner,
// which is exactly how GD itself collides with them.
struct Shape {
    double minX = 0, minY = 0, maxX = 0, maxY = 0;
    Corner corner = Corner::None;

    double width() const { return maxX - minX; }
    double height() const { return maxY - minY; }
    bool isSlope() const { return corner != Corner::None; }

    // Y of the slope's diagonal at `x` (clamped to the box).
    double diagonalAt(double x) const {
        double w = this->width();
        double t = w > 0 ? std::clamp((x - minX) / w, 0.0, 1.0) : 0.5;
        // Right angle bottom-right or top-left: the diagonal rises left to right.
        bool rising = corner == Corner::BottomRight || corner == Corner::TopLeft;
        return rising ? minY + t * this->height() : maxY - t * this->height();
    }

    // Highest solid point of the shape at `x`.
    double topAt(double x) const {
        if (isBottomCorner(corner)) return this->diagonalAt(x);
        return maxY;
    }

    Shape flippedVertically() const {
        return { minX, -maxY, maxX, -minY, flipCornerVertically(corner) };
    }
};

enum class Direction { FloorToCeiling, CeilingToFloor };

class SurfaceMirror {
public:
    explicit SurfaceMirror(std::vector<Shape> solids, Direction direction = Direction::FloorToCeiling)
      : m_direction(direction) {
        // Ceiling to floor is the same problem upside down, so flip the
        // world vertically, solve it as a floor, and flip the answer back.
        for (auto& s : solids) {
            m_solids.push_back(direction == Direction::CeilingToFloor ? s.flippedVertically() : s);
        }
    }

    bool empty() const { return m_solids.empty(); }
    Direction direction() const { return m_direction; }

    // Picks the more likely direction from the slopes in the selection: a
    // floor's slopes have their solid corner at the bottom, a ceiling's at the
    // top. With no slopes to go on, assume it's a floor.
    static Direction detectDirection(std::vector<Shape> const& solids) {
        int floorVotes = 0, ceilingVotes = 0;
        for (auto const& s : solids) {
            if (!s.isSlope()) continue;
            if (isBottomCorner(s.corner)) floorVotes++;
            else ceilingVotes++;
        }
        return ceilingVotes > floorVotes ? Direction::CeilingToFloor : Direction::FloorToCeiling;
    }

    // New Y for an object whose position is (x, y). X never changes.
    double mapY(double x, double y) const {
        if (m_direction == Direction::CeilingToFloor) {
            return -this->mapFloorY(x, -y);
        }
        return this->mapFloorY(x, y);
    }

private:
    static constexpr double EPSILON = 0.01;

    std::vector<Shape> m_solids;
    Direction m_direction;

    // Finds the shape that forms the surface at `x`: the one whose top is
    // highest there. Objects hanging past the edge of the floor use the
    // nearest shape instead. Returns the shape and the X to sample it at.
    std::optional<std::pair<Shape const*, double>> surfaceAt(double x) const {
        Shape const* best = nullptr;
        double bestTop = -std::numeric_limits<double>::infinity();
        for (auto const& s : m_solids) {
            if (x < s.minX - EPSILON || x > s.maxX + EPSILON) continue;
            double top = s.topAt(x);
            // On a tie (the seam between two shapes), prefer a slope over a
            // block, since its diagonal is the visible surface.
            if (top > bestTop + EPSILON || (std::abs(top - bestTop) <= EPSILON && best && !best->isSlope() && s.isSlope())) {
                best = &s;
                bestTop = top;
            }
        }
        if (best) return std::make_pair(best, x);

        double bestDist = std::numeric_limits<double>::infinity();
        for (auto const& s : m_solids) {
            double dist = x < s.minX ? s.minX - x : x - s.maxX;
            if (dist < bestDist - EPSILON || (std::abs(dist - bestDist) <= EPSILON && best && s.maxY > best->maxY)) {
                best = &s;
                bestDist = dist;
            }
        }
        if (!best) return std::nullopt;
        return std::make_pair(best, std::clamp(x, best->minX, best->maxX));
    }

    // In each column the floor splits into three bands, and each band is
    // mirrored onto the matching band of the ceiling:
    //
    //   above the surface line      -> the same distance below the line
    //   inside the slope's box      -> stretched across the flipped slope's box
    //   below the slope's box       -> the same distance above the slope's box
    //
    // For a flat block all three collapse into a plain mirror across its top
    // edge. Measuring fill from the slope's box instead of the line is what
    // keeps blocks under 2x1 and half slopes on the grid.
    double mapFloorY(double x, double y) const {
        auto found = this->surfaceAt(x);
        if (!found) return y;
        auto [surface, sampleX] = *found;

        double line, bottom, top;
        if (isBottomCorner(surface->corner)) {
            line = surface->diagonalAt(sampleX);
            bottom = surface->minY;
            top = surface->maxY;
        }
        else {
            line = bottom = top = surface->maxY;
        }

        if (y >= line) {
            return 2 * line - y;
        }
        if (y >= bottom) {
            double below = line - bottom;
            if (below <= EPSILON) return top;
            return line + (line - y) * (top - line) / below;
        }
        return top + (bottom - y);
    }
};

}
