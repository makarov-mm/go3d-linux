#pragma once

#include <array>
#include <cstddef>
#include <cstdint>
#include <expected>
#include <optional>
#include <span>
#include <string>
#include <string_view>
#include <vector>

// The rules of Go on boards of 9×9 to 19×19: the board with its strings of stones and their liberties, kept up
// to date move by move; legality with ko and superko; area scoring; and a game with its record in SGF.
namespace go {

enum class Color : std::uint8_t { Black, White };

constexpr Color Opposite(Color color) { return color == Color::Black ? Color::White : Color::Black; }
constexpr int Index(Color color) { return color == Color::Black ? 0 : 1; }

// What lies on a point; the border round the board is a point of its own kind.
enum class Cell : std::uint8_t { Empty, Black, White, Border };

constexpr Cell CellOf(Color color) { return color == Color::Black ? Cell::Black : Cell::White; }

constexpr int MaxSize = 19;
constexpr int Stride = MaxSize + 2;
constexpr int PointCount = Stride * Stride;

// A point: (y + 1) * Stride + x + 1, for the column x from the left and the row y from the top, as in SGF.
using Point = int;
constexpr Point Pass = 0;    // a corner of the border, never a point of the board
constexpr Point Resignation = -1; // in a game record

constexpr Point PointAt(int x, int y) { return (y + 1) * Stride + x + 1; }
constexpr int XOf(Point point) { return point % Stride - 1; }
constexpr int YOf(Point point) { return point / Stride - 1; }
constexpr std::array<int, 4> Neighbours{-Stride, -1, 1, Stride};
constexpr std::array<int, 4> Diagonals{-Stride - 1, -Stride + 1, Stride - 1, Stride + 1};

// "D4": the column's letter, I left out, and the row counted from the bottom; "pass".
std::string PointName(Point point, int size);
std::optional<Point> ParsePoint(std::string_view text, int size);

// The hoshi of a board, and the points where handicap stones go, in the order they are placed.
std::vector<Point> StarPoints(int size);
std::vector<Point> HandicapPoints(int size, int stones);
int MaxHandicap(int size);

class Board {
public:
    explicit Board(int size = 19);

    int Size() const { return m_size; }
    Cell At(Point point) const { return m_cells[static_cast<std::size_t>(point)]; }
    bool IsStone(Point point) const { return At(point) == Cell::Black || At(point) == Cell::White; }
    Color ToPlay() const { return m_toPlay; }
    void SetToPlay(Color color) { m_toPlay = color; }
    Point Ko() const { return m_ko; } // the point that simple ko forbids, or Pass
    Point LastMove() const { return m_lastMove; }
    int Passes() const { return m_passes; } // in a row, at the end of the moves so far
    int Captures(Color by) const { return m_captures[static_cast<std::size_t>(Index(by))]; }
    std::uint64_t Hash() const { return m_hash; } // of the stones alone
    std::span<const std::int16_t> EmptyPoints() const { return {m_empty.data(), static_cast<std::size_t>(m_emptyCount)}; }
    bool IsOnBoard(Point point) const { return point > 0 && point < PointCount && At(point) != Cell::Border; }

    // The string of stones through a point: its first stone, its size, and the stones in a ring.
    Point Head(Point point) const { return m_head[static_cast<std::size_t>(point)]; }
    Point NextStone(Point point) const { return m_next[static_cast<std::size_t>(point)]; }
    int StoneCount(Point point) const { return m_stones[static_cast<std::size_t>(Head(point))]; }
    // With one liberty left; and that liberty.
    bool InAtari(Point point) const;
    Point AtariLiberty(Point point) const;
    // The liberties of the string through a point, counted up to a limit; they are written to `found`.
    int Liberties(Point point, int limit, Point* found) const;

    // Legal by the rules but superko: on an empty point, not retaking a ko at once, and not suicide.
    bool IsLegal(Point point, Color color) const;
    // An eye of `color` that playouts leave alone: surrounded by its stones, the corners mostly too.
    bool IsEye(Point point, Color color) const;
    // The move would leave its string with a single liberty, and takes nothing.
    bool IsSelfAtari(Point point, Color color) const;
    // A legal move or a pass; the stones taken are added to `captured` when given.
    void Play(Point point, Color color, std::vector<Point>* captured = nullptr);
    // A stone set on the board before the game, such as a handicap stone.
    void Setup(Point point, Color color);

    // Black's points less White's, counting stones and the empty points next only to stones of one color:
    // the score at the end of a playout, where every empty point is a single eye or a shared liberty.
    int QuickScore() const;

private:
    void AddLiberty(Point head, Point liberty);
    void RemoveLiberty(Point head, Point liberty);
    void Merge(Point a, Point b);
    int RemoveString(Point head, std::vector<Point>* captured);
    void AddEmpty(Point point);
    void RemoveEmpty(Point point);

    int m_size = 19;
    std::array<Cell, PointCount> m_cells{};
    std::array<std::int16_t, PointCount> m_head{};
    std::array<std::int16_t, PointCount> m_next{};
    // For each string, kept at its head: its stones and its pseudo-liberties, an empty neighbour counted once
    // for every stone it touches, with their sum and the sum of their squares: when all are the same point,
    // the string is in atari.
    std::array<std::int16_t, PointCount> m_stones{};
    std::array<std::int16_t, PointCount> m_libs{};
    std::array<std::int32_t, PointCount> m_libSum{};
    std::array<std::int32_t, PointCount> m_libSumSquares{};
    std::array<std::int16_t, MaxSize * MaxSize> m_empty{};
    std::array<std::int16_t, PointCount> m_emptyIndex{};
    int m_emptyCount = 0;
    Color m_toPlay = Color::Black;
    Point m_ko = Pass;
    Point m_lastMove = Pass;
    int m_passes = 0;
    std::array<int, 2> m_captures{};
    std::uint64_t m_hash = 0;
};

using PointSet = std::array<bool, PointCount>;

// The owner of every point with the dead stones taken off: the stones' color, the color that alone surrounds an
// empty region, or Empty for neither.
std::array<Cell, PointCount> AreaOwners(const Board& board, const PointSet& dead);

// Area scoring: stones and the territory they surround; komi goes to White.
struct Score {
    int black = 0;
    int white = 0;
    double komi = 0;

    double Margin() const { return black - white - komi; } // above zero, Black wins
};

enum class Phase : std::uint8_t { Playing, Scoring, Finished };

struct MoveRecord {
    Point point = Pass; // a stone, Pass or Resignation
    Color color = Color::Black;
    std::vector<Point> captured;

    bool IsPass() const { return point == Pass; }
    bool IsResignation() const { return point == Resignation; }
};

// A game: the size, the handicap and komi, the moves played and those taken back, and at the end the dead
// stones the players agree on.
class Game {
public:
    explicit Game(int size = 9, int handicap = 0) { Reset(size, handicap); }

    // Komi is 7.5, or 0.5 with a handicap.
    void Reset(int size, int handicap);
    int Size() const { return Start().Size(); }
    int Handicap() const { return m_handicap; }
    double Komi() const { return m_komi; }

    const Board& Current() const { return m_positions.back(); }
    const Board& Start() const { return m_positions.front(); }
    const Board& PositionAt(int ply) const { return m_positions[static_cast<std::size_t>(ply)]; }
    std::span<const MoveRecord> Moves() const { return m_moves; }
    int Ply() const { return static_cast<int>(m_moves.size()); }
    Phase GetPhase() const;
    // The earlier positions, for superko.
    std::span<const std::uint64_t> History() const { return m_hashes; }

    // Legal with positional superko: no move may bring back an earlier position.
    bool IsLegal(Point point) const;
    // A stone or a pass; after two passes the game goes to scoring.
    bool Play(Point point);
    void Resign(Color who);
    bool CanUndo() const { return !m_moves.empty(); }
    bool CanRedo() const { return !m_redo.empty(); }
    bool Undo();
    bool Redo();

    // Scoring: the dead stones, marked string by string, and the score with them taken off.
    const PointSet& Dead() const { return m_dead; }
    void SetDead(const PointSet& dead);
    void ToggleDead(Point point);
    Score Count() const;
    // The owner of every point once the dead stones are off: Black, White or Empty for neither.
    std::array<Cell, PointCount> Territory() const;
    void Accept() { m_accepted = true; }
    // Black's margin after scoring or resignation: positive for Black; nothing while the game goes on.
    std::optional<double> Margin() const;
    std::optional<Color> Winner() const;
    std::string ResultText() const; // "B+3.5", "W+R"

    std::string ToSgf(std::string_view blackName, std::string_view whiteName) const;
    // An SGF game: its main line, with the size, komi, handicap and setup stones.
    static std::expected<Game, std::string> FromSgf(std::string_view text);

private:
    void Begin(const Board& start, int handicap, double komi);
    void Push(Point point, Color color);

    int m_handicap = 0;
    double m_komi = 7.5;
    std::vector<Board> m_positions;
    std::vector<std::uint64_t> m_hashes;
    std::vector<MoveRecord> m_moves;
    std::vector<MoveRecord> m_redo; // the next to replay at the end
    PointSet m_dead{};
    bool m_accepted = false;
};

} // namespace go
