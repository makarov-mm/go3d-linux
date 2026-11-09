#pragma once

#include "BoardGeometry.h"
#include "Gl.h"
#include "MathTypes.h"
#include "StoneModel.h"
#include "Themes.h"

#include <array>
#include <cstdint>
#include <initializer_list>
#include <span>
#include <string>
#include <string_view>
#include <unordered_map>
#include <vector>

namespace render {

// A stone, in the look of its color.
struct StoneDraw {
    math::Mat4 model = math::Mat4::Identity();
    bool white = false;
    float ground = 0; // the height of what it lies on
    math::Vec3 glowColor;
    float glow = 0;
    float dissolve = 0;
    float seed = 0;   // where the pattern falls
    float mark = 0;   // the ring on the stone played last
    float opacity = 1; // below 1, seen through: a dead stone, or where a click would play
};

// What is marked on each point of the board, as bits; see surface.frag.
enum Mark : std::uint8_t {
    BlackArea = 1,
    WhiteArea = 2,
    Hover = 4,
    Hint = 8,
    Ko = 16,
    StoneHere = 32,
};

// A rectangle of the screen behind which the scene is blurred, in pixels from the top left.
struct FrostedPanel {
    float x = 0, y = 0, width = 0, height = 0, radius = 0, opacity = 1;
};

struct Scene {
    math::Mat4 view = math::Mat4::Identity();
    math::Mat4 projection = math::Mat4::Identity();
    math::Vec3 eye;
    float time = 0;
    std::vector<StoneDraw> stones;
    std::array<std::uint8_t, go::MaxSize * go::MaxSize> marks{}; // column + MaxSize * row, rows from the far side
    bool estimate = false; // the territory marked as an estimate, fainter
    bool coordinates = true;
    std::vector<FrostedPanel> panels;
};

// A linked shader program, its sources taken from the files built into the executable.
class Program {
public:
    // A stage from files put together in order, by their names in shaders/.
    struct Stage {
        gl::GLenum type;
        std::initializer_list<std::string_view> sources;
    };

    Program() = default;
    Program(std::string name, std::initializer_list<Stage> stages);
    // The GLSL version the shaders are compiled for, from the context's OpenGL version (46 for 4.6).
    static void SetVersion(int glVersion);

    gl::GLuint Id() const { return m_program.Id(); }
    void Use() const { gl::glUseProgram(m_program.Id()); }
    gl::GLint Location(const char* name) const;

    void Set(const char* name, int value) const;
    void Set(const char* name, float value) const;
    void Set(const char* name, math::Vec2 value) const;
    void Set(const char* name, math::Vec3 value) const;
    void Set(const char* name, math::Vec4 value) const;
    void Set(const char* name, const math::Mat4& value) const;
    void SetArray(const char* name, std::span<const math::Vec4> values) const;

private:
    std::string m_name;
    gl::ProgramObject m_program;
    mutable std::unordered_map<std::string, gl::GLint> m_locations;
};

// Draws the scene into the window: shadows, reflections, the lit scene in high dynamic range with
// multisampling, then bloom and tone mapping. The interface is drawn on top afterwards.
class Renderer {
public:
    Renderer(const stones::StoneShape& stone, int glVersion);
    ~Renderer();
    Renderer(const Renderer&) = delete;
    Renderer& operator=(const Renderer&) = delete;

    void SetTheme(const Theme& theme);
    // The board, the bowls and the lids for a size of board; to be set before the first frame.
    void SetLayout(const board::Layout& layout);
    void Resize(int width, int height);
    void Render(const Scene& scene);

    int Width() const { return m_width; }
    int Height() const { return m_height; }

private:
    struct GpuMesh {
        gl::VertexArray vertexArray;
        gl::Buffer vertices;
        gl::Buffer indices;
        gl::GLsizei count = 0;
        void Draw(int instances = 1) const;
    };

    // A run of stones in the instance buffer drawn with one call.
    struct StoneRange {
        int first = 0, count = 0;
        bool white = false;
    };

    static GpuMesh Upload(const stones::Mesh& mesh);
    static GpuMesh Upload(const board::Mesh& mesh);
    void CreateEnvironmentTextures();
    void BakeEnvironment();
    void UpdateLightView();
    void CreateScreenTargets();
    void UpdateFrameData(const Scene& scene);
    void UploadStones(const Scene& scene);
    void SetPass(math::Vec3 eye, bool shadows, bool mirror);
    void BindLighting(bool reflections) const;
    void DrawShadowMap();
    void DrawReflections(const Scene& scene);
    void DrawMainPass(const Scene& scene);
    void DrawBowls(const Program& program, bool material) const;
    void DrawStones(std::span<const StoneRange> ranges, bool detailed) const;
    void SetStoneMaterial(bool white) const;
    void SetSurfaceMaterial(const Scene& scene) const;
    void PostProcess(const Scene& scene);

    int m_glVersion;
    int m_width = 0, m_height = 0;
    int m_samples = 4;
    Theme m_theme;
    board::Layout m_layout;
    math::Mat4 m_lightViewProjection;
    float m_shadowRadius = 10, m_shadowRange = 30;

    GpuMesh m_stone, m_simpleStone;
    GpuMesh m_boardTop, m_boardSides, m_table, m_bowl, m_lid;
    gl::VertexArray m_emptyVertexArray;

    Program m_stoneProgram, m_surfaceProgram, m_shadowProgram, m_backgroundProgram;
    Program m_resolveProgram, m_bloomDownProgram, m_bloomUpProgram, m_compositeProgram;
    Program m_environmentProgram, m_irradianceProgram, m_prefilterProgram, m_brdfProgram;

    gl::Buffer m_frameData, m_passData;
    gl::Buffer m_stoneBuffer;
    std::size_t m_stoneCapacity = 0;
    std::vector<StoneRange> m_opaque;      // Black's stones, then White's
    std::vector<StoneRange> m_translucent; // one at a time, the furthest first

    gl::Texture m_shadowMap;
    gl::Framebuffer m_shadowFramebuffer;
    gl::Sampler m_shadowCompareSampler, m_shadowDepthSampler;
    gl::Texture m_environment, m_irradiance, m_prefiltered, m_brdf, m_labels, m_marks;
    int m_prefilteredLevels = 6;

    gl::Texture m_sceneColor, m_sceneDepth, m_resolved;
    gl::Framebuffer m_sceneFramebuffer, m_resolvedFramebuffer;
    gl::Texture m_reflectionColor, m_reflectionDepth;
    gl::Framebuffer m_reflectionFramebuffer;
    int m_reflectionLevels = 1;
    static constexpr int BloomLevels = 6;
    std::array<gl::Texture, BloomLevels> m_bloom;
    std::array<gl::Framebuffer, BloomLevels> m_bloomFramebuffers;
    std::array<std::array<int, 2>, BloomLevels> m_bloomSizes{};
};

} // namespace render
