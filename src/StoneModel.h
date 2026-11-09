#pragma once

#include "MathTypes.h"

#include <cstdint>
#include <vector>

// The stone: a lens, both faces gently domed and meeting in a rounded edge, as Japanese stones are cut.
// Its middle is the origin; it lies with its axis along y.
namespace stones {

struct Vertex {
    math::Vec3 position;
    math::Vec3 normal;
};

struct Mesh {
    std::vector<Vertex> vertices;
    std::vector<std::uint32_t> indices;
};

// The lines of the board are one unit apart.
constexpr float Radius = 0.475f;
constexpr float HalfHeight = 0.19f;

struct StoneShape {
    Mesh detailed; // for the scene
    Mesh simple;   // for the shadows and the reflections
};

StoneShape BuildStone();

} // namespace stones
