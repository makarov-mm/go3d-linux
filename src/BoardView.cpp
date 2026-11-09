#include "BoardView.h"

#include "StoneModel.h"

#include <algorithm>
#include <cmath>
#include <cstddef>

using namespace math;
using go::Color;
using go::Point;

namespace {

float Ease(float t) { return t * t * t * (t * (t * 6 - 15) + 10); }
float Approach(float value, float target, float step) { return value < target ? std::min(value + step, target) : std::max(value - step, target); }

// A number in [0, 1) that stays the same for the same arguments.
float Hash(int a, int b)
{
    std::uint32_t h = static_cast<std::uint32_t>(a) * 0x9E3779B1u ^ static_cast<std::uint32_t>(b) * 0x85EBCA77u;
    h ^= h >> 15;
    h *= 0x2C1B3C6Du;
    h ^= h >> 12;
    return static_cast<float>(h >> 8) / static_cast<float>(1u << 24);
}

constexpr float FadeTime = 0.45f;
constexpr float HeapLayer = 0.24f; // between the layers of stones in a bowl
constexpr float LidLayer = 0.2f;   // and in a lid
constexpr float Spacing = 0.86f;   // between the stones lying side by side, a little leaning on each other

} // namespace

float BoardView::Random()
{
    m_seed = m_seed * 1664525u + 1013904223u;
    return static_cast<float>(m_seed >> 8) / static_cast<float>(1u << 24);
}

Vec3 BoardView::RandomAxis()
{
    const float angle = Random() * 2 * Pi;
    return {std::cos(angle), 0, std::sin(angle)};
}

float BoardView::GroundAt(Vec3 position) const
{
    return std::abs(position.x) < m_layout.boardHalf && std::abs(position.z) < m_layout.boardHalf ? 0.0f : m_layout.TableHeight();
}

Vec3 BoardView::OnBoard(Point point) const
{
    return m_layout.PointCenter(point) + Vec3{0, stones::HalfHeight, 0};
}

BoardView::Stone& BoardView::Add(Color color, Where where, Vec3 position, bool appear, float delay)
{
    Stone stone;
    stone.color = color;
    stone.where = where;
    stone.position = position;
    stone.yaw = Random() * 2 * Pi;
    stone.seed = Random();
    if (appear) {
        stone.dissolve = 1;
        stone.dissolveDelay = delay;
    }
    m_stones.push_back(stone);
    return m_stones.back();
}

BoardView::Stone* BoardView::OnPoint(Point point)
{
    for (Stone& stone : m_stones)
        if (stone.where == Where::Board && stone.point == point)
            return &stone;
    return nullptr;
}

void BoardView::SetLayout(const board::Layout& layout)
{
    m_layout = layout;
    m_stones.clear();
    m_lidCounts = {};
    BuildHeaps();
}

// Each bowl holds a heap of its stones: a few layers, the stones leaning on each other, higher in the middle.
void BoardView::BuildHeaps()
{
    const float reach = 0.72f * m_layout.bowlRadius - 0.48f;
    for (int bowl = 0; bowl < 2; ++bowl) {
        const Vec3 base = m_layout.bowls[static_cast<std::size_t>(bowl)];
        const float floor = board::HeapHeight(m_layout.bowlHeight) - 0.05f; // above the foot of the bowl
        const Color color = bowl == 0 ? Color::Black : Color::White;
        for (int layer = 0; layer < 3; ++layer) {
            const float layerReach = reach * (1.0f - 0.32f * static_cast<float>(layer));
            const float shift = layer == 1 ? Spacing * 0.5f : 0.0f;
            const int count = static_cast<int>(std::ceil(layerReach / Spacing)) + 1;
            for (int row = -count; row <= count; ++row) {
                for (int column = -count; column <= count; ++column) {
                    float x = (static_cast<float>(column) + 0.5f * static_cast<float>(row & 1)) * Spacing + shift;
                    float z = static_cast<float>(row) * Spacing * 0.866f + shift * 0.577f;
                    x += (Random() - 0.5f) * 0.16f;
                    z += (Random() - 0.5f) * 0.16f;
                    const float r2 = x * x + z * z;
                    if (r2 > layerReach * layerReach)
                        continue;
                    const float dome = 0.12f * (1 - r2 / std::max(reach * reach, 0.01f));
                    const float y = floor + stones::HalfHeight + static_cast<float>(layer) * HeapLayer + dome;
                    Stone& stone = Add(color, Where::Bowl, base + Vec3{x, y, z}, false, 0);
                    stone.tiltAxis = RandomAxis();
                    stone.tilt = 0.1f + 0.3f * Random();
                }
            }
        }
    }
}

Vec3 BoardView::HeapTop(int bowl)
{
    const Vec3 base = m_layout.bowls[static_cast<std::size_t>(bowl)];
    const float angle = Random() * 2 * Pi, radius = 0.35f * std::sqrt(Random());
    const float top = board::HeapHeight(m_layout.bowlHeight) - 0.05f + stones::HalfHeight + 2 * HeapLayer + 0.12f;
    return base + Vec3{std::cos(angle) * radius, top, std::sin(angle) * radius};
}

// The stones taken lie in the lid in a spiral, as many as fit on its floor, and then in smaller layers on top.
Vec3 BoardView::LidSlot(int lid, int slot, float& tilt, Vec3& axis)
{
    const float reach = 0.84f * m_layout.bowlRadius - 0.5f;
    int index = slot, layer = 0;
    float layerReach = reach;
    int capacity = 0;
    for (;;) {
        layerReach = reach * std::max(0.3f, 1.0f - 0.2f * static_cast<float>(layer));
        capacity = std::max(3, static_cast<int>(0.85f * (layerReach / 0.5f) * (layerReach / 0.5f)));
        if (index < capacity)
            break;
        index -= capacity;
        ++layer;
    }
    const float radius = layerReach * std::sqrt((static_cast<float>(index) + 0.5f) / static_cast<float>(capacity));
    const float angle = static_cast<float>(index) * 2.39996f + static_cast<float>(layer) * 1.3f;
    const float jitter = 0.08f * (Hash(lid * 977 + 1, slot) - 0.5f);
    const Vec3 base = m_layout.lids[static_cast<std::size_t>(lid)];
    tilt = 0.05f + 0.15f * Hash(lid * 977 + 2, slot) + (layer > 0 ? 0.1f : 0.0f);
    const float axisAngle = 2 * Pi * Hash(lid * 977 + 3, slot);
    axis = {std::cos(axisAngle), 0, std::sin(axisAngle)};
    const float y = board::LidFloor(m_layout.bowlRadius) + stones::HalfHeight + static_cast<float>(layer) * LidLayer;
    return base + Vec3{std::cos(angle) * (radius + jitter), y, std::sin(angle) * (radius + jitter)};
}

void BoardView::ToLid(Stone& stone, int lid, float delay)
{
    const int slot = m_lidCounts[static_cast<std::size_t>(lid)]++;
    float tilt = 0;
    Vec3 axis{1, 0, 0};
    const Vec3 place = LidSlot(lid, slot, tilt, axis);
    stone.where = Where::Lid;
    stone.point = go::Pass;
    stone.lid = lid;
    stone.slot = slot;
    Fly(stone, place, tilt, axis, 0.62f, 1.5f, delay, Event::Capture);
}

void BoardView::Fly(Stone& stone, Vec3 to, float tilt, Vec3 axis, float duration, float arc, float delay, Event landing)
{
    // A stone already on its way starts again from where it is, with the lean of the end it is nearer.
    const bool nearerStart = stone.flying && (stone.flightDelay > 0 || stone.flightTime < stone.flightDuration * 0.5f);
    if (!nearerStart) {
        stone.fromAxis = stone.tiltAxis;
        stone.fromTilt = stone.tilt;
    }
    stone.from = stone.position;
    stone.to = to;
    stone.tiltAxis = axis;
    stone.tilt = tilt;
    stone.flightTime = 0;
    stone.flightDuration = duration;
    stone.flightDelay = delay;
    stone.arc = arc;
    stone.flying = true;
    stone.landing = landing;
}

void BoardView::Rebuild(const go::Game& game, bool animate)
{
    // The stones on the board and in the lids go; the heaps in the bowls stay.
    for (Stone& stone : m_stones) {
        if (stone.where == Where::Bowl)
            continue;
        stone.where = Where::Leaving;
        stone.point = go::Pass;
        stone.flying = false;
        stone.removeWhenLanded = false;
        stone.dissolveTarget = 1;
        stone.dissolveDelay = 0;
        stone.removeWhenGone = true;
    }
    if (!animate)
        std::erase_if(m_stones, [](const Stone& stone) { return stone.where == Where::Leaving; });

    const go::Board& position = game.Current();
    const int size = position.Size();
    for (int y = 0; y < size; ++y) {
        for (int x = 0; x < size; ++x) {
            const Point point = go::PointAt(x, y);
            if (!position.IsStone(point))
                continue;
            const Color color = position.At(point) == go::Cell::Black ? Color::Black : Color::White;
            Stone& stone = Add(color, Where::Board, OnBoard(point), animate, 0.25f + 0.0035f * static_cast<float>(x + y * size));
            stone.point = point;
        }
    }
    for (int lid = 0; lid < 2; ++lid) {
        const Color owner = lid == 0 ? Color::Black : Color::White;
        const int count = position.Captures(owner);
        for (int slot = 0; slot < count; ++slot) {
            float tilt = 0;
            Vec3 axis{1, 0, 0};
            const Vec3 place = LidSlot(lid, slot, tilt, axis);
            Stone& stone = Add(go::Opposite(owner), Where::Lid, place, animate, 0.4f + 0.01f * static_cast<float>(slot));
            stone.lid = lid;
            stone.slot = slot;
            stone.tilt = tilt;
            stone.tiltAxis = axis;
        }
        m_lidCounts[static_cast<std::size_t>(lid)] = count;
    }
}

void BoardView::PlayMove(const go::MoveRecord& record, bool forward)
{
    if (record.IsPass() || record.IsResignation())
        return;
    const int bowl = go::Index(record.color);
    const int lid = bowl; // a player keeps the stones taken in the lid of their own bowl
    const Vec3 target = OnBoard(record.point);
    auto nearestFirst = [&](std::vector<Point> points) {
        std::ranges::sort(points, [&](Point a, Point b) { return Length(OnBoard(a) - target) < Length(OnBoard(b) - target); });
        return points;
    };

    if (forward) {
        // A stone from the player's bowl flies to the point.
        const Vec3 start = HeapTop(bowl);
        Stone& stone = Add(record.color, Where::Board, start, false, 0);
        stone.point = record.point;
        stone.tiltAxis = RandomAxis();
        stone.tilt = 0.3f;
        const float distance = Length(target - start);
        const float duration = std::min(0.4f + 0.012f * distance, 0.68f);
        Fly(stone, target, 0, {1, 0, 0}, duration, 0.9f + 0.035f * distance, 0, Event::Place);

        // Then the stones it takes fly into the lid, the nearest first.
        float delay = duration + 0.08f;
        for (const Point point : nearestFirst(record.captured)) {
            if (Stone* taken = OnPoint(point)) {
                ToLid(*taken, lid, delay);
                delay += 0.045f;
            }
        }
        return;
    }

    // Back: the stone returns to its bowl, and the stones it took come back from the lid, the last put there first.
    if (Stone* stone = OnPoint(record.point)) {
        stone->where = Where::Leaving;
        stone->point = go::Pass;
        stone->removeWhenLanded = true;
        Fly(*stone, HeapTop(bowl), 0.3f, RandomAxis(), 0.45f, 1.0f, 0, Event::Return);
    }
    std::vector<std::size_t> inLid;
    for (std::size_t i = 0; i < m_stones.size(); ++i)
        if (m_stones[i].where == Where::Lid && m_stones[i].lid == lid)
            inLid.push_back(i);
    std::ranges::sort(inLid, [&](std::size_t a, std::size_t b) { return m_stones[a].slot > m_stones[b].slot; });
    const std::vector<Point> returning = nearestFirst(record.captured);
    std::vector<std::size_t> movers;
    for (std::size_t i = 0; i < returning.size(); ++i) {
        if (i < inLid.size()) {
            movers.push_back(inLid[i]);
        }
        else {
            float tilt = 0;
            Vec3 axis{1, 0, 0};
            const Vec3 place = LidSlot(lid, 0, tilt, axis);
            Add(go::Opposite(record.color), Where::Lid, place, false, 0);
            movers.push_back(m_stones.size() - 1);
        }
    }
    float delay = 0.08f;
    for (std::size_t i = 0; i < returning.size(); ++i) {
        Stone& stone = m_stones[movers[i]];
        stone.where = Where::Board;
        stone.point = returning[i];
        stone.lid = -1;
        Fly(stone, OnBoard(returning[i]), 0, {1, 0, 0}, 0.55f, 1.3f, delay, Event::Place);
        delay += 0.04f;
    }
    m_lidCounts[static_cast<std::size_t>(lid)] = std::max(0, m_lidCounts[static_cast<std::size_t>(lid)] - static_cast<int>(returning.size()));
}

void BoardView::Preview(std::span<const Point> taking)
{
    for (Stone& stone : m_stones)
        stone.liftTarget = stone.where == Where::Board && !stone.flying && std::ranges::find(taking, stone.point) != taking.end() ? 1.0f : 0.0f;
}

void BoardView::Update(float seconds, std::vector<Event>& events)
{
    for (Stone& stone : m_stones) {
        stone.lift = Approach(stone.lift, stone.liftTarget, seconds / 0.15f);
        if (stone.dissolveDelay > 0)
            stone.dissolveDelay -= seconds;
        else
            stone.dissolve = Approach(stone.dissolve, stone.dissolveTarget, seconds / FadeTime);

        if (!stone.flying)
            continue;
        if (stone.flightDelay > 0) {
            stone.flightDelay -= seconds;
            continue;
        }
        stone.flightTime = std::min(stone.flightTime + seconds, stone.flightDuration);
        const float t = stone.flightTime / stone.flightDuration;
        stone.position = Lerp(stone.from, stone.to, Ease(t)) + Vec3{0, stone.arc * std::sin(Pi * t), 0};
        if (t >= 1) {
            stone.position = stone.to;
            stone.flying = false;
            events.push_back(stone.landing);
        }
    }
    std::erase_if(m_stones, [](const Stone& stone) { return (stone.removeWhenGone && stone.dissolve >= 1) || (stone.removeWhenLanded && !stone.flying); });
}

bool BoardView::Settled() const
{
    return std::ranges::none_of(m_stones, [](const Stone& stone) { return stone.flying || stone.dissolve != stone.dissolveTarget; });
}

bool BoardView::BoardReady() const
{
    return std::ranges::none_of(m_stones, [](const Stone& stone) {
        return stone.where == Where::Board && (stone.flying || stone.dissolve != stone.dissolveTarget);
    });
}

bool BoardView::Animating() const
{
    // Stones about to be taken pulse while they are lifted.
    return !Settled() || std::ranges::any_of(m_stones, [](const Stone& stone) { return stone.lift != stone.liftTarget || stone.lift > 0; });
}

void BoardView::Draws(std::vector<render::StoneDraw>& draws, Point lastMove, const go::PointSet* dead, Vec3 warning, float time) const
{
    draws.reserve(draws.size() + m_stones.size());
    for (const Stone& stone : m_stones) {
        Mat4 orientation = Mat4::Rotation(stone.tiltAxis, stone.tilt);
        if (stone.flying && stone.flightDelay > 0) {
            orientation = Mat4::Rotation(stone.fromAxis, stone.fromTilt);
        }
        else if (stone.flying) {
            const float e = Ease(stone.flightTime / stone.flightDuration);
            orientation = Mat4::Rotation(stone.tiltAxis, e * stone.tilt) * Mat4::Rotation(stone.fromAxis, (1 - e) * stone.fromTilt);
        }
        const bool resting = stone.where == Where::Board && !stone.flying;
        const float lift = Ease(stone.lift) * (0.8f + 0.2f * std::sin(time * 6.0f));
        render::StoneDraw draw;
        draw.model = Mat4::Translation(stone.position + Vec3{0, 0.12f * lift, 0}) * orientation * Mat4::RotationY(stone.yaw);
        draw.white = stone.color == Color::White;
        draw.ground = stone.flying ? GroundAt(stone.position) : stone.position.y - stones::HalfHeight;
        draw.dissolve = stone.dissolve;
        draw.seed = stone.seed;
        draw.glowColor = warning;
        draw.glow = 1.5f * lift;
        if (resting && stone.point == lastMove)
            draw.mark = 1;
        if (resting && dead && (*dead)[static_cast<std::size_t>(stone.point)])
            draw.opacity = 0.42f;
        draws.push_back(draw);
    }
}

void BoardView::MarkStones(std::array<std::uint8_t, go::MaxSize * go::MaxSize>& marks) const
{
    for (const Stone& stone : m_stones)
        if (stone.where == Where::Board && !stone.flying && stone.dissolve < 0.5f)
            marks[static_cast<std::size_t>(go::XOf(stone.point) + go::MaxSize * go::YOf(stone.point))] |= render::StoneHere;
}
