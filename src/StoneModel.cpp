#include "StoneModel.h"

#include <algorithm>
#include <cmath>
#include <cstddef>

namespace stones {

using namespace math;

namespace {

// The height of the face at a distance from the axis: flat in the middle, falling steeply at the edge.
float FaceHeight(float r)
{
    const float t = std::min(r / Radius, 1.0f);
    return HalfHeight * std::pow(1 - t * t, 0.75f);
}

// The outline from the middle of the lower face round the edge to the middle of the upper face, its points
// crowded at the edge where it turns fastest, turned round the axis.
Mesh Build(int rings, int segments)
{
    struct ProfilePoint {
        float r, y;
        Vec2 normal;
    };
    std::vector<ProfilePoint> profile;
    // A point of a face, sign 1 for the upper and -1 for the lower, with its outward normal from the slope.
    auto face = [](float r, float sign) {
        const float d = 0.0005f;
        const float a = std::max(r - d, 0.0f), b = std::min(r + d, Radius);
        const float slope = (FaceHeight(b) - FaceHeight(a)) / (b - a);
        const float length = std::sqrt(1 + slope * slope);
        return ProfilePoint{r, sign * FaceHeight(r), Vec2{-slope / length, sign / length}};
    };
    for (int i = 0; i < rings; ++i)
        profile.push_back(face(Radius * std::sin(Pi / 2 * static_cast<float>(i) / static_cast<float>(rings)), -1));
    profile.push_back({Radius, 0, {1, 0}});
    for (int i = rings - 1; i >= 0; --i) {
        const float r = Radius * std::sin(Pi / 2 * static_cast<float>(i) / static_cast<float>(rings));
        profile.push_back(face(r, 1));
    }

    Mesh mesh;
    const int count = static_cast<int>(profile.size());
    for (const ProfilePoint& point : profile) {
        for (int j = 0; j <= segments; ++j) {
            const float angle = 2 * Pi * static_cast<float>(j) / static_cast<float>(segments);
            const float c = std::cos(angle), s = std::sin(angle);
            mesh.vertices.push_back({{point.r * c, point.y, point.r * s}, Normalize(Vec3{point.normal.x * c, point.normal.y, point.normal.x * s})});
        }
    }
    const auto index = [&](int i, int j) { return static_cast<std::uint32_t>(i * (segments + 1) + j); };
    for (int i = 0; i + 1 < count; ++i) {
        for (int j = 0; j < segments; ++j) {
            // Next to the axis a quad narrows to a triangle.
            if (profile[static_cast<std::size_t>(i)].r > 0)
                mesh.indices.insert(mesh.indices.end(), {index(i, j), index(i + 1, j + 1), index(i, j + 1)});
            if (profile[static_cast<std::size_t>(i + 1)].r > 0)
                mesh.indices.insert(mesh.indices.end(), {index(i, j), index(i + 1, j), index(i + 1, j + 1)});
        }
    }
    return mesh;
}

} // namespace

StoneShape BuildStone()
{
    return {Build(14, 48), Build(5, 20)};
}

} // namespace stones
