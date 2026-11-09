#pragma once

#include "BoardGeometry.h"
#include "Go.h"
#include "MathTypes.h"
#include "Renderer.h"

#include <array>
#include <cstdint>
#include <span>
#include <vector>

// The stones as they lie and move: heaped in the bowls, flying from a bowl to the point played, the stones
// taken flying into the lid of the player who took them, all of it in reverse when a move is taken back,
// and dissolving in when a game is set out.
class BoardView {
public:
    // What happened in a frame, for the sounds.
    enum class Event { Place, Capture, Return };

    // The places of the bowls and the lids for a size of board; the heaps in the bowls are made again.
    void SetLayout(const board::Layout& layout);
    // Sets out the game's position on the board, and the stones taken in the lids.
    void Rebuild(const go::Game& game, bool animate);
    // A move played (forward) or taken back.
    void PlayMove(const go::MoveRecord& record, bool forward);
    // The stones that a stone placed under the pointer would take glow and rise a little, about to go.
    void Preview(std::span<const go::Point> taking);
    void Update(float seconds, std::vector<Event>& events);
    // Nothing flying or dissolving: the position shown is the game's.
    bool Settled() const;
    // The stones of the board in their places, though stones taken may still be on their way to a lid.
    bool BoardReady() const;
    bool Animating() const;

    // `dead` stones are drawn seen through; stones about to be taken glow in `warning`.
    void Draws(std::vector<render::StoneDraw>& draws, go::Point lastMove, const go::PointSet* dead, math::Vec3 warning, float time) const;
    // The points with a stone lying on them, for the shade round the stones.
    void MarkStones(std::array<std::uint8_t, go::MaxSize * go::MaxSize>& marks) const;

private:
    enum class Where { Board, Bowl, Lid, Leaving };

    struct Stone {
        go::Color color = go::Color::Black;
        Where where = Where::Bowl;
        go::Point point = go::Pass; // on the board
        int lid = -1, slot = 0;     // in a lid: whose, and its place there
        math::Vec3 position;        // the middle of the stone
        float yaw = 0, seed = 0;
        math::Vec3 tiltAxis{1, 0, 0}; // how it lies, leaning on the stones under it
        float tilt = 0;
        // A flight from one place to another, in an arc, turning from one tilt to another.
        math::Vec3 from, to;
        math::Vec3 fromAxis{1, 0, 0};
        float fromTilt = 0;
        float flightTime = 0, flightDuration = 0, flightDelay = 0, arc = 0;
        bool flying = false;
        bool removeWhenLanded = false;
        Event landing = Event::Place;
        float dissolve = 0, dissolveTarget = 0, dissolveDelay = 0;
        bool removeWhenGone = false;
        float lift = 0, liftTarget = 0; // in the preview of a capture
    };

    Stone& Add(go::Color color, Where where, math::Vec3 position, bool appear, float delay);
    Stone* OnPoint(go::Point point);
    math::Vec3 OnBoard(go::Point point) const;
    // Where a stone is taken from a bowl and put back into it: the top of the heap.
    math::Vec3 HeapTop(int bowl);
    math::Vec3 LidSlot(int lid, int slot, float& tilt, math::Vec3& axis);
    void ToLid(Stone& stone, int lid, float delay);
    void BuildHeaps();
    static void Fly(Stone& stone, math::Vec3 to, float tilt, math::Vec3 axis, float duration, float arc, float delay, Event landing);
    float GroundAt(math::Vec3 position) const;
    float Random();
    math::Vec3 RandomAxis();

    board::Layout m_layout;
    std::vector<Stone> m_stones;
    std::array<int, 2> m_lidCounts{};
    std::uint32_t m_seed = 1;
};
