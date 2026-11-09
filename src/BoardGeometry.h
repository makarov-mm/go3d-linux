#pragma once

#include "Go.h"
#include "MathTypes.h"

#include <array>
#include <cmath>
#include <cstdint>
#include <vector>

// The board, the table, and the bowls with their lids. The lines of the board are one unit apart and its
// surface is the plane y = 0; Black looks from +z, where row 1 is.
namespace board {

struct Vertex {
    math::Vec3 position;
    math::Vec3 normal;
    math::Vec2 uv;
};

struct Mesh {
    std::vector<Vertex> vertices;
    std::vector<std::uint32_t> indices;
};

// Where everything stands for a size of board.
struct Layout {
    int size = 19;
    float half = 9;        // from the middle to the outer lines
    float boardHalf = 10;  // to the edge of the board
    float thickness = 0.9f;
    float bowlRadius = 2.5f;
    float bowlHeight = 2;
    std::array<math::Vec3, 2> bowls{}; // Black's and White's, on the table
    std::array<math::Vec3, 2> lids{};  // turned over beside them, for the stones taken

    static Layout For(int size);

    float TableHeight() const { return -thickness; }
    math::Vec3 PointCenter(go::Point point) const
    {
        return {static_cast<float>(go::XOf(point)) - half, 0, static_cast<float>(go::YOf(point)) - half};
    }
    // The point nearest a point of the surface, or Pass off the board.
    go::Point PointNear(math::Vec3 position) const
    {
        const int x = static_cast<int>(std::lround(position.x + half)), y = static_cast<int>(std::lround(position.z + half));
        return x >= 0 && y >= 0 && x < size && y < size ? go::PointAt(x, y) : go::Pass;
    }
    // How far the scene reaches from the middle, for the camera.
    float Extent() const { return boardHalf + bowlRadius * 1.2f; }
};

Mesh BuildBoardTop(const Layout& layout);
Mesh BuildBoardSides(const Layout& layout);
Mesh BuildTable(float radius, float height);
// A bowl and a lid, standing on y = 0 round the y axis; uv runs along the outline and round.
Mesh BuildBowl(float radius, float height);
Mesh BuildLid(float radius);
// The height of the floor of a lid of this radius, where the stones taken lie.
constexpr float LidFloor(float radius) { return radius * 0.07f; }
// The height of the top of the heap of stones in a bowl.
constexpr float HeapHeight(float height) { return height * 0.78f; }

} // namespace board
