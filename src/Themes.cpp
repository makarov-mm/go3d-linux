#include "Themes.h"

namespace {

// A kaya board with slate and shell stones, dark mulberry bowls, a dark wooden table.
Theme Kaya()
{
    Theme t{
        .nameEnglish = "Kaya",
        .nameRussian = "Кая",
        .boardColor = {0.64f, 0.39f, 0.13f},
        .boardRoughness = 0.42f,
        .boardClearcoat = 0.3f,
        .boardClearcoatRoughness = 0.12f,
        .boardPattern = 0,
        .reflection = 0.18f,
        .reflectionBlur = 2.6f,
        .gridColor = {0.012f, 0.009f, 0.006f},
        .gridWidth = 0.021f,
        .gridRoughness = 0.55f,
        .starRadius = 0.095f,
        .bowlColor = {0.15f, 0.072f, 0.03f},
        .bowlRoughness = 0.34f,
        .bowlClearcoat = 0.75f,
        .bowlPattern = 0,
        .bowlSeed = 0.37f,
        .white = {.albedo = {0.84f, 0.83f, 0.79f}, .roughness = 0.26f, .clearcoat = 0.55f, .clearcoatRoughness = 0.07f, .subsurface = 0.22f,
                  .pattern = 1, .patternStrength = 1.0f},
        .black = {.albedo = {0.024f, 0.024f, 0.027f}, .roughness = 0.44f, .clearcoat = 0.3f, .clearcoatRoughness = 0.22f, .pattern = 2,
                  .patternStrength = 1.0f},
        .tableColor = {0.055f, 0.032f, 0.018f},
        .tableRoughness = 0.5f,
        .tablePattern = 1,
        .keyDirection = {-0.5f, 0.78f, 0.38f},
        .keyRadiance = {4.2f, 3.9f, 3.4f},
        .keySoftness = 0.045f,
        .ceiling = {0.035f, 0.034f, 0.034f},
        .horizon = {0.1f, 0.085f, 0.07f},
        .floor = {0.02f, 0.016f, 0.012f},
        .diffuseLight = 0.75f,
        .reflectedLight = 1.0f,
        .background = 0.55f,
        .fog = {0.032f, 0.027f, 0.022f},
        .exposure = 1.05f,
        .bloom = 0.035f,
        .vignette = 0.42f,
        .accent = {0.3f, 0.72f, 1.0f},
        .accentWarm = {1.0f, 0.42f, 0.18f},
    };
    t.panels[0] = {.direction = {-0.5f, 0.78f, 0.38f}, .halfSize = {0.42f, 0.3f}, .corner = 0.08f, .softness = 0.06f, .radiance = {7.0f, 6.4f, 5.6f}};
    t.panels[1] = {.direction = {0.85f, 0.3f, 0.3f}, .halfSize = {0.55f, 0.35f}, .corner = 0.1f, .softness = 0.15f, .radiance = {0.7f, 0.78f, 0.9f}};
    t.panels[2] = {.direction = {0.15f, 0.42f, -0.9f}, .halfSize = {1.3f, 0.07f}, .corner = 0.03f, .softness = 0.03f, .radiance = {5.0f, 4.6f, 4.0f}};
    t.panels[3] = {.direction = {0.0f, 1.0f, 0.05f}, .halfSize = {0.7f, 0.7f}, .corner = 0.3f, .softness = 0.3f, .radiance = {0.6f, 0.58f, 0.55f}};
    t.panels[4] = {.direction = {-0.9f, 0.25f, -0.4f}, .halfSize = {0.06f, 0.6f}, .corner = 0.03f, .softness = 0.03f, .radiance = {2.6f, 2.2f, 1.8f}};
    t.panelCount = 5;
    return t;
}

// A board of laminated bamboo with Chinese yunzi stones, red lacquer bowls and green felt.
Theme Bamboo()
{
    Theme t{
        .nameEnglish = "Bamboo · Yunzi",
        .nameRussian = "Бамбук · юньцзы",
        .boardColor = {0.6f, 0.41f, 0.19f},
        .boardRoughness = 0.34f,
        .boardClearcoat = 0.5f,
        .boardClearcoatRoughness = 0.08f,
        .boardPattern = 1,
        .reflection = 0.35f,
        .reflectionBlur = 2.0f,
        .gridColor = {0.022f, 0.012f, 0.006f},
        .gridWidth = 0.019f,
        .gridRoughness = 0.45f,
        .starRadius = 0.09f,
        .bowlColor = {0.3f, 0.025f, 0.018f},
        .bowlRoughness = 0.22f,
        .bowlClearcoat = 0.9f,
        .bowlPattern = 1,
        .bowlSeed = 0.71f,
        .white = {.albedo = {0.78f, 0.77f, 0.68f}, .roughness = 0.48f, .clearcoat = 0.18f, .clearcoatRoughness = 0.18f, .subsurface = 0.4f,
                  .pattern = 4, .patternStrength = 1.0f, .edge = {0.55f, 0.5f, 0.36f}, .edgeStrength = 0.25f},
        .black = {.albedo = {0.02f, 0.025f, 0.021f}, .roughness = 0.46f, .clearcoat = 0.15f, .clearcoatRoughness = 0.2f, .pattern = 4,
                  .patternStrength = 1.0f, .edge = {0.04f, 0.3f, 0.09f}, .edgeStrength = 0.7f},
        .tableColor = {0.016f, 0.045f, 0.036f},
        .tableRoughness = 0.95f,
        .tablePattern = 0,
        .tableSheen = 0.35f,
        .keyDirection = {-0.58f, 0.72f, 0.4f},
        .keyRadiance = {4.4f, 4.0f, 3.4f},
        .keySoftness = 0.05f,
        .ceiling = {0.035f, 0.033f, 0.032f},
        .horizon = {0.1f, 0.085f, 0.065f},
        .floor = {0.018f, 0.016f, 0.012f},
        .diffuseLight = 0.7f,
        .reflectedLight = 1.0f,
        .background = 0.55f,
        .fog = {0.03f, 0.028f, 0.022f},
        .exposure = 1.05f,
        .bloom = 0.04f,
        .vignette = 0.42f,
        .accent = {0.3f, 0.92f, 0.62f},
        .accentWarm = {1.0f, 0.45f, 0.15f},
    };
    t.panels[0] = {.direction = {-0.58f, 0.72f, 0.4f}, .halfSize = {0.42f, 0.3f}, .corner = 0.08f, .softness = 0.06f, .radiance = {7.4f, 6.5f, 5.4f}};
    t.panels[1] = {.direction = {0.85f, 0.3f, 0.3f}, .halfSize = {0.55f, 0.35f}, .corner = 0.1f, .softness = 0.15f, .radiance = {0.65f, 0.8f, 0.75f}};
    t.panels[2] = {.direction = {0.15f, 0.42f, -0.9f}, .halfSize = {1.3f, 0.07f}, .corner = 0.03f, .softness = 0.03f, .radiance = {5.0f, 4.6f, 4.0f}};
    t.panels[3] = {.direction = {0.0f, 1.0f, 0.05f}, .halfSize = {0.7f, 0.7f}, .corner = 0.3f, .softness = 0.3f, .radiance = {0.6f, 0.58f, 0.52f}};
    t.panels[4] = {.direction = {-0.9f, 0.25f, -0.4f}, .halfSize = {0.06f, 0.6f}, .corner = 0.03f, .softness = 0.03f, .radiance = {2.4f, 2.1f, 1.7f}};
    t.panelCount = 5;
    return t;
}

// Smoked glass with silver lines, glass stones, black lacquer, a stone table.
Theme Glass()
{
    Theme t{
        .nameEnglish = "Glass",
        .nameRussian = "Стекло",
        .boardColor = {0.011f, 0.013f, 0.017f},
        .boardRoughness = 0.07f,
        .boardClearcoat = 1.0f,
        .boardClearcoatRoughness = 0.03f,
        .boardPattern = 3,
        .reflection = 1.0f,
        .reflectionBlur = 0.7f,
        .gridColor = {0.78f, 0.8f, 0.84f},
        .gridWidth = 0.015f,
        .gridMetallic = 1,
        .gridRoughness = 0.22f,
        .starRadius = 0.08f,
        .bowlColor = {0.012f, 0.012f, 0.014f},
        .bowlRoughness = 0.1f,
        .bowlClearcoat = 1.0f,
        .bowlPattern = 1,
        .bowlSeed = 0.2f,
        .white = {.albedo = {0.84f, 0.86f, 0.9f}, .roughness = 0.2f, .clearcoat = 0.9f, .clearcoatRoughness = 0.03f, .subsurface = 0.5f,
                  .pattern = 3, .patternStrength = 0.5f, .edge = {0.55f, 0.66f, 0.85f}, .edgeStrength = 0.35f},
        .black = {.albedo = {0.008f, 0.009f, 0.013f}, .roughness = 0.06f, .clearcoat = 1.0f, .clearcoatRoughness = 0.02f, .pattern = 3,
                  .patternStrength = 0.6f, .edge = {0.06f, 0.12f, 0.35f}, .edgeStrength = 0.6f},
        .tableColor = {0.024f, 0.024f, 0.028f},
        .tableRoughness = 0.3f,
        .tablePattern = 3,
        .keyDirection = {-0.42f, 0.85f, 0.32f},
        .keyRadiance = {3.1f, 3.15f, 3.3f},
        .keySoftness = 0.05f,
        .ceiling = {0.05f, 0.055f, 0.065f},
        .horizon = {0.09f, 0.095f, 0.11f},
        .floor = {0.018f, 0.018f, 0.02f},
        .diffuseLight = 1.0f,
        .reflectedLight = 1.0f,
        .background = 0.5f,
        .fog = {0.03f, 0.032f, 0.038f},
        .exposure = 1.0f,
        .bloom = 0.04f,
        .vignette = 0.4f,
        .accent = {0.4f, 0.78f, 1.0f},
        .accentWarm = {0.95f, 0.6f, 0.3f},
    };
    t.panels[0] = {.direction = {-0.42f, 0.85f, 0.32f}, .halfSize = {0.5f, 0.5f}, .corner = 0.25f, .softness = 0.12f, .radiance = {6.0f, 6.1f, 6.4f}};
    t.panels[1] = {.direction = {0.9f, 0.2f, 0.25f}, .halfSize = {0.5f, 0.3f}, .corner = 0.1f, .softness = 0.12f, .radiance = {0.8f, 0.85f, 1.0f}};
    t.panels[2] = {.direction = {-0.1f, 0.35f, -0.92f}, .halfSize = {1.4f, 0.05f}, .corner = 0.02f, .softness = 0.02f, .radiance = {6.0f, 6.2f, 6.6f}};
    t.panels[3] = {.direction = {0.0f, 1.0f, 0.0f}, .halfSize = {0.8f, 0.8f}, .corner = 0.4f, .softness = 0.35f, .radiance = {0.9f, 0.92f, 1.0f}};
    t.panels[4] = {.direction = {0.6f, 0.3f, -0.75f}, .halfSize = {0.05f, 0.7f}, .corner = 0.02f, .softness = 0.02f, .radiance = {2.5f, 2.6f, 3.0f}};
    t.panelCount = 5;
    return t;
}

// A dark board with glowing lines and stones with neon rims.
Theme Neon()
{
    Theme t{
        .nameEnglish = "Neon",
        .nameRussian = "Неон",
        .boardColor = {0.006f, 0.008f, 0.014f},
        .boardRoughness = 0.12f,
        .boardClearcoat = 0.6f,
        .boardClearcoatRoughness = 0.03f,
        .boardPattern = 3,
        .reflection = 0.9f,
        .reflectionBlur = 0.6f,
        .gridColor = {0.2f, 0.8f, 1.0f},
        .gridWidth = 0.013f,
        .gridRoughness = 0.3f,
        .gridGlow = 1.6f,
        .starRadius = 0.075f,
        .bowlColor = {0.05f, 0.055f, 0.07f},
        .bowlRoughness = 0.25f,
        .bowlClearcoat = 0.8f,
        .bowlPattern = 1,
        .bowlSeed = 0.5f,
        .white = {.albedo = {0.82f, 0.86f, 0.9f}, .roughness = 0.2f, .clearcoat = 1.0f, .clearcoatRoughness = 0.03f, .subsurface = 0.5f,
                  .trim = {1.0f, 0.3f, 0.75f}, .trimStrength = 5.0f},
        .black = {.albedo = {0.03f, 0.032f, 0.04f}, .roughness = 0.18f, .clearcoat = 1.0f, .clearcoatRoughness = 0.03f, .pattern = 3,
                  .patternStrength = 0.4f, .trim = {0.2f, 0.8f, 1.0f}, .trimStrength = 5.0f},
        .tableColor = {0.004f, 0.006f, 0.01f},
        .tableRoughness = 0.5f,
        .tablePattern = 2,
        .tableGlow = 0.5f,
        .keyDirection = {-0.35f, 0.8f, 0.48f},
        .keyRadiance = {1.9f, 2.2f, 2.7f},
        .keySoftness = 0.03f,
        .ceiling = {0.01f, 0.012f, 0.025f},
        .horizon = {0.02f, 0.035f, 0.07f},
        .floor = {0.004f, 0.005f, 0.008f},
        .diffuseLight = 1.0f,
        .reflectedLight = 1.2f,
        .background = 0.7f,
        .fog = {0.006f, 0.01f, 0.02f},
        .exposure = 1.1f,
        .bloom = 0.07f,
        .vignette = 0.5f,
        .accent = {0.2f, 0.85f, 1.0f},
        .accentWarm = {1.0f, 0.35f, 0.75f},
    };
    t.panels[0] = {.direction = {-0.35f, 0.8f, 0.48f}, .halfSize = {0.35f, 0.25f}, .corner = 0.05f, .softness = 0.05f, .radiance = {3.5f, 4.2f, 5.0f}};
    t.panels[1] = {.direction = {0.95f, 0.12f, 0.2f}, .halfSize = {1.0f, 0.03f}, .corner = 0.02f, .softness = 0.02f, .radiance = {8.0f, 1.5f, 5.0f}};
    t.panels[2] = {.direction = {-0.95f, 0.12f, -0.2f}, .halfSize = {1.0f, 0.03f}, .corner = 0.02f, .softness = 0.02f, .radiance = {1.0f, 5.0f, 8.0f}};
    t.panels[3] = {.direction = {0.1f, 0.3f, -0.95f}, .halfSize = {1.4f, 0.025f}, .corner = 0.01f, .softness = 0.015f, .radiance = {8.0f, 4.0f, 1.0f}};
    t.panels[4] = {.direction = {0.0f, 1.0f, 0.0f}, .halfSize = {0.6f, 0.6f}, .corner = 0.3f, .softness = 0.3f, .radiance = {0.25f, 0.3f, 0.45f}};
    t.panels[5] = {.direction = {0.3f, 0.25f, 0.95f}, .halfSize = {0.9f, 0.025f}, .corner = 0.01f, .softness = 0.015f, .radiance = {1.0f, 6.0f, 7.0f}};
    t.panelCount = 6;
    return t;
}

} // namespace

const std::array<Theme, ThemeCount>& Themes()
{
    static const std::array<Theme, ThemeCount> themes{Kaya(), Bamboo(), Glass(), Neon()};
    return themes;
}
