#include "BoardGeometry.h"

#include <algorithm>
#include <array>
#include <cmath>
#include <cstddef>

namespace board {

using namespace math;

namespace {

constexpr float Bevel = 0.06f; // the rounded upper edge of the board

// Adds a quad, wound counterclockwise as seen from the side its normal faces.
void AddQuad(Mesh& mesh, const Vertex& a, const Vertex& b, const Vertex& c, const Vertex& d)
{
    const auto base = static_cast<std::uint32_t>(mesh.vertices.size());
    mesh.vertices.insert(mesh.vertices.end(), {a, b, c, d});
    const Vec3 facing = Cross(b.position - a.position, c.position - a.position);
    const Vec3 normal = a.normal + b.normal + c.normal + d.normal;
    if (Dot(facing, normal) >= 0)
        mesh.indices.insert(mesh.indices.end(), {base, base + 1, base + 2, base, base + 2, base + 3});
    else
        mesh.indices.insert(mesh.indices.end(), {base, base + 2, base + 1, base, base + 3, base + 2});
}

struct ProfilePoint {
    float r, y;
};

// An outline turned round the y axis. Normals come from the neighbouring points, the outline running with
// the outside on its right; uv is the distance along the outline and the turn.
Mesh Lathe(const std::vector<ProfilePoint>& profile, int segments)
{
    Mesh mesh;
    const int count = static_cast<int>(profile.size());
    std::vector<float> along(profile.size(), 0.0f);
    for (int i = 1; i < count; ++i) {
        const ProfilePoint& a = profile[static_cast<std::size_t>(i - 1)];
        const ProfilePoint& b = profile[static_cast<std::size_t>(i)];
        along[static_cast<std::size_t>(i)] = along[static_cast<std::size_t>(i - 1)] + std::hypot(b.r - a.r, b.y - a.y);
    }
    for (int i = 0; i < count; ++i) {
        const ProfilePoint& before = profile[static_cast<std::size_t>(std::max(i - 1, 0))];
        const ProfilePoint& after = profile[static_cast<std::size_t>(std::min(i + 1, count - 1))];
        const float tr = after.r - before.r, ty = after.y - before.y;
        const float length = std::max(std::hypot(tr, ty), 1e-6f);
        const float nr = ty / length, ny = -tr / length;
        const ProfilePoint& point = profile[static_cast<std::size_t>(i)];
        for (int j = 0; j <= segments; ++j) {
            const float t = static_cast<float>(j) / static_cast<float>(segments);
            const float c = std::cos(2 * Pi * t), s = std::sin(2 * Pi * t);
            mesh.vertices.push_back({{point.r * c, point.y, point.r * s}, Normalize(Vec3{nr * c, ny, nr * s}), {along[static_cast<std::size_t>(i)], t}});
        }
    }
    const auto index = [&](int i, int j) { return static_cast<std::uint32_t>(i * (segments + 1) + j); };
    for (int i = 0; i + 1 < count; ++i) {
        for (int j = 0; j < segments; ++j) {
            if (profile[static_cast<std::size_t>(i)].r > 0)
                mesh.indices.insert(mesh.indices.end(), {index(i, j), index(i + 1, j + 1), index(i, j + 1)});
            if (profile[static_cast<std::size_t>(i + 1)].r > 0)
                mesh.indices.insert(mesh.indices.end(), {index(i, j), index(i + 1, j), index(i + 1, j + 1)});
        }
    }
    return mesh;
}

} // namespace

Layout Layout::For(int size)
{
    Layout layout;
    layout.size = size;
    layout.half = static_cast<float>(size - 1) / 2;
    layout.boardHalf = layout.half + 0.95f; // room for the coordinates round the lines
    layout.thickness = 0.9f;
    layout.bowlRadius = std::max(2.2f, 0.28f * layout.boardHalf);
    layout.bowlHeight = layout.bowlRadius * 0.78f;
    // Each player's bowl on the right, toward the player; the lid beside it.
    const float x = layout.boardHalf + 0.8f + layout.bowlRadius;
    const float z = std::min(layout.boardHalf * 0.45f, layout.boardHalf - layout.bowlRadius);
    const float lidZ = z - 2 * layout.bowlRadius - 0.4f;
    const float y = layout.TableHeight();
    layout.bowls = {Vec3{x, y, z}, Vec3{-x, y, -z}};
    layout.lids = {Vec3{x, y, lidZ}, Vec3{-x, y, -lidZ}};
    return layout;
}

Mesh BuildBoardTop(const Layout& layout)
{
    Mesh mesh;
    const float e = layout.boardHalf - Bevel;
    const Vec3 up{0, 1, 0};
    AddQuad(mesh, {{-e, 0, e}, up, {-e, e}}, {{e, 0, e}, up, {e, e}}, {{e, 0, -e}, up, {e, -e}}, {{-e, 0, -e}, up, {-e, -e}});
    return mesh;
}

// The rounded edge and the sides, swept round the top with mitred corners.
Mesh BuildBoardSides(const Layout& layout)
{
    struct SidePoint {
        float out, y;   // outward from the edge of the flat top, and height
        float nOut, nY; // normal
    };
    std::vector<SidePoint> profile;
    for (int i = 0; i <= 4; ++i) {
        const float angle = Pi / 2 * static_cast<float>(i) / 4;
        profile.push_back({Bevel * std::sin(angle), -Bevel * (1 - std::cos(angle)), std::sin(angle), std::cos(angle)});
    }
    profile.push_back({Bevel, -layout.thickness, 1, 0});

    const std::array<Vec3, 4> outward{Vec3{0, 0, 1}, Vec3{1, 0, 0}, Vec3{0, 0, -1}, Vec3{-1, 0, 0}};
    const float inner = layout.boardHalf - Bevel;
    Mesh mesh;
    for (const Vec3& o : outward) {
        const Vec3 along{-o.z, 0, o.x};
        for (std::size_t i = 0; i + 1 < profile.size(); ++i) {
            const SidePoint& p = profile[i];
            const SidePoint& q = profile[i + 1];
            const float pExtent = inner + p.out, qExtent = inner + q.out;
            auto vertex = [&](const SidePoint& point, float extent, float s) {
                return Vertex{o * extent + along * s + Vec3{0, point.y, 0}, Normalize(o * point.nOut + Vec3{0, point.nY, 0}), {s, point.y}};
            };
            AddQuad(mesh, vertex(p, pExtent, -pExtent), vertex(p, pExtent, pExtent), vertex(q, qExtent, qExtent), vertex(q, qExtent, -qExtent));
        }
    }
    return mesh;
}

Mesh BuildTable(float radius, float height)
{
    Mesh mesh;
    const Vec3 up{0, 1, 0};
    constexpr int Segments = 96;
    mesh.vertices.push_back({{0, height, 0}, up, {0.5f, 0.5f}});
    for (int i = 0; i < Segments; ++i) {
        const float angle = 2 * Pi * static_cast<float>(i) / Segments;
        const float x = std::cos(angle) * radius, z = std::sin(angle) * radius;
        mesh.vertices.push_back({{x, height, z}, up, {x / radius * 0.5f + 0.5f, z / radius * 0.5f + 0.5f}});
    }
    for (std::uint32_t i = 0; i < Segments; ++i) {
        // Counterclockwise from above: from +x toward -z.
        const std::uint32_t a = 1 + i, b = 1 + (i + 1) % Segments;
        mesh.indices.insert(mesh.indices.end(), {0, b, a});
    }
    return mesh;
}

Mesh BuildBowl(float radius, float height)
{
    const float r = radius, h = height;
    std::vector<ProfilePoint> profile{{0, 0}, {0.58f * r, 0}, {0.62f * r, 0.03f * h}};
    // The belly: widest a little below the middle, drawing in to the mouth.
    for (int i = 1; i <= 24; ++i) {
        const float t = 0.03f + 0.97f * static_cast<float>(i) / 24;
        const float swell = t < 0.42f ? 0.62f + 0.38f * std::sin(Pi / 2 * t / 0.42f) : 1 - 0.17f * ((t - 0.42f) / 0.58f) * ((t - 0.42f) / 0.58f);
        profile.push_back({swell * r, t * h});
    }
    // The lip, and the inside down to the heap of stones.
    profile.insert(profile.end(), {{0.815f * r, 1.012f * h}, {0.79f * r, 1.012f * h}, {0.77f * r, h}, {0.74f * r, 0.9f * h},
                                   {0.72f * r, HeapHeight(h) - 0.05f}, {0.4f * r, HeapHeight(h) - 0.05f}, {0, HeapHeight(h) - 0.05f}});
    return Lathe(profile, 72);
}

Mesh BuildLid(float radius)
{
    const float r = radius;
    const std::vector<ProfilePoint> profile{{0, 0},           {0.84f * r, 0},     {0.9f * r, 0.03f * r},  {0.96f * r, 0.1f * r},
                                            {r, 0.19f * r},   {0.99f * r, 0.22f * r}, {0.96f * r, 0.225f * r}, {0.935f * r, 0.2f * r},
                                            {0.9f * r, 0.11f * r}, {0.84f * r, LidFloor(r)}, {0.42f * r, LidFloor(r)}, {0, LidFloor(r)}};
    return Lathe(profile, 72);
}

} // namespace board
