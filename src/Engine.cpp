#include "Engine.h"

#include <algorithm>
#include <chrono>
#include <cmath>
#include <string_view>
#include <vector>

namespace go {

namespace {

// A fast generator for the playouts (xorshift64*).
class Random {
public:
    explicit Random(std::uint64_t seed) : m_state(seed ? seed : 0x9E3779B97F4A7C15ull) {}

    std::uint64_t Next()
    {
        m_state ^= m_state >> 12;
        m_state ^= m_state << 25;
        m_state ^= m_state >> 27;
        return m_state * 0x2545F4914F6CDD1Dull;
    }
    // From 0 to n - 1.
    int Below(int n) { return static_cast<int>(((Next() >> 32) * static_cast<std::uint64_t>(n)) >> 32); }

private:
    std::uint64_t m_state;
};

// 3×3 shapes ----------------------------------------------------------------------------------------

// The eight points round a point, in the order of their bits in a shape's code.
constexpr std::array<int, 8> Around{-Stride - 1, -Stride, -Stride + 1, -1, 1, Stride - 1, Stride, Stride + 1};

// Local shapes in which the middle point is a good move (from MoGo and Pachi): X and O are the two colors either
// way round, unless a tenth character says X is to play; x is anything but X, o anything but O, # off the
// board, ? anything.
constexpr std::array<std::string_view, 15> Shapes{
    "XOX...???",  // hane, enclosing
    "XO....?.?",  // hane, not cutting
    "XO?X..x.?",  // hane, bending round
    "XOO...?.?X", // thin hane
    ".O.X.....",  // diagonal attachment
    "XO?O.o?o?",  // cut, unprotected
    "XO?O.X???",  // cut, peeped
    "?X?O.Oooo",  // pushing through
    "OX?o.O???",  // cutting a knight's move
    "X.?O.?##?",  // edge: chasing
    "OX?X.O###",  // edge: blocking a cut
    "?X?x.O###",  // edge: blocking a connection
    "?XOx.x###",  // edge: dropping back
    "?OXo.O?##X", // edge: throwing in
    "?OXX.O###X", // edge: cutting
};

// For each arrangement of the eight points round an empty point (two bits each, as Cell), whether a shape
// matches with Black to play (bit 0) and with White to play (bit 1).
const std::array<std::uint8_t, 65536>& ShapeTable()
{
    static const std::array<std::uint8_t, 65536> table = [] {
        std::array<std::uint8_t, 65536> result{};
        for (const std::string_view shape : Shapes) {
            const bool xToPlay = shape.size() > 9 && shape[9] == 'X';
            for (int symmetry = 0; symmetry < 8; ++symmetry) {
                // The shape's character at each of the eight points, turned and mirrored.
                std::array<char, 8> wanted{};
                for (int row = 0; row < 3; ++row) {
                    for (int column = 0; column < 3; ++column) {
                        int dx = column - 1, dy = row - 1;
                        if (dx == 0 && dy == 0)
                            continue;
                        if (symmetry & 1)
                            dx = -dx;
                        if (symmetry & 2)
                            dy = -dy;
                        if (symmetry & 4)
                            std::swap(dx, dy);
                        const int slot = static_cast<int>(std::ranges::find(Around, dy * Stride + dx) - Around.begin());
                        wanted[static_cast<std::size_t>(slot)] = shape[static_cast<std::size_t>(row * 3 + column)];
                    }
                }
                for (const Cell x : {Cell::Black, Cell::White}) {
                    const Cell o = x == Cell::Black ? Cell::White : Cell::Black;
                    const std::uint8_t players = xToPlay ? (x == Cell::Black ? 1 : 2) : 3;
                    // Every code the shape allows.
                    auto allows = [&](char c, Cell cell) {
                        switch (c) {
                        case 'X': return cell == x;
                        case 'O': return cell == o;
                        case '.': return cell == Cell::Empty;
                        case '#': return cell == Cell::Border;
                        case 'x': return cell != x;
                        case 'o': return cell != o;
                        default: return true;
                        }
                    };
                    for (int code = 0; code < 65536; ++code) {
                        bool match = true;
                        for (int slot = 0; slot < 8 && match; ++slot)
                            match = allows(wanted[static_cast<std::size_t>(slot)], static_cast<Cell>((code >> (2 * slot)) & 3));
                        if (match)
                            result[static_cast<std::size_t>(code)] |= players;
                    }
                }
            }
        }
        return result;
    }();
    return table;
}

bool MatchesShape(const std::array<std::uint8_t, 65536>& table, const Board& board, Point point, Color toPlay)
{
    int code = 0;
    for (int slot = 0; slot < 8; ++slot)
        code |= static_cast<int>(board.At(point + Around[static_cast<std::size_t>(slot)])) << (2 * slot);
    return (table[static_cast<std::size_t>(code)] & (toPlay == Color::Black ? 1 : 2)) != 0;
}

// Playouts --------------------------------------------------------------------------------------------

// Where each point was first played in a simulation, and by whom, for the all-moves-as-first statistics.
struct FirstMoves {
    std::array<std::int16_t, PointCount> ply;
    std::array<Color, PointCount> color;

    void Clear() { ply.fill(-1); }
    void Record(Point point, Color by, int at)
    {
        if (point != Pass && ply[static_cast<std::size_t>(point)] < 0) {
            ply[static_cast<std::size_t>(point)] = static_cast<std::int16_t>(at);
            color[static_cast<std::size_t>(point)] = by;
        }
    }
};

class Playout {
public:
    Playout(Random& random, const std::array<std::uint8_t, 65536>& shapes) : m_random(random), m_shapes(shapes) {}

    // A move for the side to play: answers to the last move first, then any move that does not fill an eye of
    // one's own or put a string in atari for nothing.
    Point Choose(const Board& board)
    {
        const Color me = board.ToPlay();
        const Point last = board.LastMove();
        if (last != Pass && board.IsStone(last)) {
            if (m_random.Below(10) < 9 && Tactics(board, last, me))
                return Pick();
            if (m_random.Below(10) < 9 && Shapes(board, last, me))
                return Pick();
        }
        const auto empty = board.EmptyPoints();
        const int count = static_cast<int>(empty.size());
        if (count == 0)
            return Pass;
        int index = m_random.Below(count);
        for (int i = 0; i < count; ++i, ++index) {
            if (index == count)
                index = 0;
            const Point point = empty[static_cast<std::size_t>(index)];
            if (!board.IsLegal(point, me) || board.IsEye(point, me))
                continue;
            // Putting a string in atari for nothing is left out; a single stone now and then, as a throw-in.
            if (board.IsSelfAtari(point, me) && (JoinsString(board, point, me) || m_random.Below(10) < 8))
                continue;
            return point;
        }
        return Pass;
    }

    // Plays the position out to the end, both sides passing; Black's score less komi.
    double Run(Board& board, double komi, FirstMoves* first, int ply)
    {
        const int limit = board.Size() * board.Size() * 3;
        int passes = 0;
        for (int moves = 0; moves < limit && passes < 2; ++moves) {
            const Color me = board.ToPlay();
            const Point move = Choose(board);
            if (first)
                first->Record(move, me, ply);
            board.Play(move, me);
            passes = move == Pass ? passes + 1 : 0;
            ++ply;
        }
        return board.QuickScore() - komi;
    }

private:
    static bool JoinsString(const Board& board, Point point, Color me)
    {
        for (const int step : Neighbours)
            if (board.At(point + step) == CellOf(me))
                return true;
        return false;
    }

    void Add(const Board& board, Point point, Color me)
    {
        if (m_count < static_cast<int>(m_moves.size()) && board.IsLegal(point, me))
            m_moves[static_cast<std::size_t>(m_count++)] = point;
    }

    Point Pick() { return m_moves[static_cast<std::size_t>(m_random.Below(m_count))]; }

    // Taking a string in atari next to the last move, or saving one's own.
    bool Tactics(const Board& board, Point last, Color me)
    {
        m_count = 0;
        const Cell own = CellOf(me);
        if (board.InAtari(last))
            Add(board, board.AtariLiberty(last), me);
        for (const int step : Neighbours) {
            const Point next = last + step;
            if (!board.IsStone(next) || !board.InAtari(next))
                continue;
            if (board.At(next) != own) {
                Add(board, board.AtariLiberty(next), me);
                continue;
            }
            // Our string in atari: run, if that gives it liberties, or take a string of theirs touching it.
            const Point liberty = board.AtariLiberty(next);
            if (!board.IsSelfAtari(liberty, me))
                Add(board, liberty, me);
            Point stone = board.Head(next);
            for (int i = 0; i < 12; ++i) {
                for (const int around : Neighbours) {
                    const Point enemy = stone + around;
                    if (board.IsStone(enemy) && board.At(enemy) != own && board.InAtari(enemy))
                        Add(board, board.AtariLiberty(enemy), me);
                }
                stone = board.NextStone(stone);
                if (stone == board.Head(next))
                    break;
            }
        }
        return m_count > 0;
    }

    // The shapes round the last move.
    bool Shapes(const Board& board, Point last, Color me)
    {
        m_count = 0;
        for (const int step : Around) {
            const Point point = last + step;
            if (board.At(point) == Cell::Empty && MatchesShape(m_shapes, board, point, me) && !board.IsSelfAtari(point, me))
                Add(board, point, me);
        }
        return m_count > 0;
    }

    Random& m_random;
    const std::array<std::uint8_t, 65536>& m_shapes;
    std::array<Point, 48> m_moves{};
    int m_count = 0;
};

// Who owns each point at the end of a playout: its stone's color, or the color of all the stones round an empty point.
void CountOwners(const Board& board, std::array<std::uint32_t, PointCount>& black, std::array<std::uint32_t, PointCount>& white)
{
    const int size = board.Size();
    for (int y = 0; y < size; ++y) {
        for (int x = 0; x < size; ++x) {
            const Point point = PointAt(x, y);
            Cell owner = board.At(point);
            if (owner == Cell::Empty) {
                bool b = false, w = false;
                for (const int step : Neighbours) {
                    b |= board.At(point + step) == Cell::Black;
                    w |= board.At(point + step) == Cell::White;
                }
                owner = b == w ? Cell::Empty : b ? Cell::Black : Cell::White;
            }
            if (owner == Cell::Black)
                ++black[static_cast<std::size_t>(point)];
            else if (owner == Cell::White)
                ++white[static_cast<std::size_t>(point)];
        }
    }
}

// The tree --------------------------------------------------------------------------------------------

struct Node {
    std::int16_t move = Pass;
    std::int16_t childCount = 0;
    std::int32_t firstChild = -1;  // -1 until the node is expanded
    float visits = 0, wins = 0;    // for the side that played the move, the prior knowledge included
    float raveVisits = 0, raveWins = 0;
    std::uint32_t simulations = 0; // through the node itself
};

constexpr float Equivalence = 3000;   // how long the all-moves-as-first statistics weigh in
constexpr std::uint32_t ExpandAfter = 8; // simulations
constexpr std::size_t MaxNodes = 2'000'000;

// Prior knowledge of a move, as visits and wins it starts with: captures, escapes and good shapes are favored, as
// are lines three and four early on; self-ataris and the first line early on are not.
void SetPrior(Node& node, const Board& board, Point point, Color me, const std::array<std::uint8_t, 65536>& shapes)
{
    float visits = 10, wins = 5;
    auto good = [&](float weight) { visits += weight / 2, wins += weight / 2; };
    auto bad = [&](float weight) { visits += weight / 2; };
    if (point == Pass) {
        node.visits = visits;
        node.wins = wins;
        return;
    }
    const Cell own = CellOf(me);
    for (const int step : Neighbours) {
        const Point next = point + step;
        if (board.IsStone(next) && board.InAtari(next)) {
            if (board.At(next) != own)
                good(board.StoneCount(next) > 1 ? 30 : 15);
            else if (!board.IsSelfAtari(point, me))
                good(15);
        }
    }
    if (board.IsSelfAtari(point, me)) {
        bool joins = false;
        for (const int step : Neighbours)
            joins |= board.At(point + step) == own;
        bad(joins ? 30 : 10); // a lone stone may be a throw-in
    }
    const Point last = board.LastMove();
    if (last != Pass && board.IsStone(last)) {
        const int dx = std::abs(XOf(point) - XOf(last)), dy = std::abs(YOf(point) - YOf(last));
        if (dx <= 1 && dy <= 1 && MatchesShape(shapes, board, point, me))
            good(10);
        else if (dx + dy <= 3)
            good(4);
    }
    // Early on, the third and fourth lines, the corners first; not the edges.
    const int size = board.Size();
    const int x = XOf(point), y = YOf(point);
    const int line = 1 + std::min({x, y, size - 1 - x, size - 1 - y});
    if (static_cast<int>(board.EmptyPoints().size()) > size * size * 3 / 4) {
        if (line == 1)
            bad(30);
        else if (line == 2)
            bad(20);
        else if (line == 3 || (line == 4 && size >= 13))
            good(12);
        // A corner point while the corner is empty.
        const int fromSide = std::min(x, size - 1 - x), fromTop = std::min(y, size - 1 - y);
        if (size >= 13 && fromSide >= 2 && fromSide <= 3 && fromTop >= 2 && fromTop <= 3) {
            bool empty = true;
            const int cornerX = x < size / 2 ? 0 : size - 6, cornerY = y < size / 2 ? 0 : size - 6;
            for (int dy = 0; dy < 6 && empty; ++dy)
                for (int dx = 0; dx < 6 && empty; ++dx)
                    empty = board.At(PointAt(cornerX + dx, cornerY + dy)) == Cell::Empty;
            if (empty)
                good(20);
        }
    }
    else if (line == 1) {
        bad(5);
    }
    node.visits = visits;
    node.wins = wins;
}

struct RootStats {
    std::vector<double> visits, wins;
    std::array<std::uint32_t, PointCount> black{}, white{};
    std::uint64_t playouts = 0;
};

class Tree {
public:
    Tree(const Board& root, std::span<const Point> moves, double komi, std::uint64_t seed)
        : m_root(root), m_komi(komi), m_random(seed), m_shapes(ShapeTable()), m_playout(m_random, m_shapes)
    {
        m_nodes.reserve(std::min<std::size_t>(MaxNodes, 4096 + moves.size()));
        m_nodes.push_back(Node{});
        m_nodes.front().firstChild = 1;
        m_nodes.front().childCount = static_cast<std::int16_t>(moves.size());
        for (const Point move : moves) {
            Node child;
            child.move = static_cast<std::int16_t>(move);
            SetPrior(child, root, move, root.ToPlay(), m_shapes);
            m_nodes.push_back(child);
        }
    }

    void Simulate(RootStats& stats)
    {
        Board board = m_root;
        const Color rootColor = m_root.ToPlay();
        m_first.Clear();
        m_path.clear();
        m_path.push_back(0);
        int node = 0, ply = 0;
        while (m_nodes[static_cast<std::size_t>(node)].firstChild >= 0 && board.Passes() < 2) {
            node = Select(node);
            const Point move = m_nodes[static_cast<std::size_t>(node)].move;
            m_first.Record(move, board.ToPlay(), ply++);
            board.Play(move, board.ToPlay());
            m_path.push_back(node);
            // A leaf that has been visited enough grows its children.
            const Node& leaf = m_nodes[static_cast<std::size_t>(node)];
            if (leaf.firstChild < 0 && leaf.simulations >= ExpandAfter && board.Passes() < 2)
                Expand(node, board);
        }
        const double score = board.Passes() >= 2 ? board.QuickScore() - m_komi : m_playout.Run(board, m_komi, &m_first, ply);
        const bool blackWins = score > 0;
        CountOwners(board, stats.black, stats.white);
        ++stats.playouts;

        // Back up the result, and the all-moves-as-first results to the brothers of each node on the way.
        for (std::size_t depth = m_path.size(); depth-- > 0;) {
            Node& current = m_nodes[static_cast<std::size_t>(m_path[depth])];
            const Color mover = depth % 2 == 0 ? Opposite(rootColor) : rootColor;
            current.visits += 1;
            current.wins += (mover == Color::Black) == blackWins ? 1.0f : 0.0f;
            ++current.simulations;
            const Color toPlay = Opposite(mover);
            const float result = (toPlay == Color::Black) == blackWins ? 1.0f : 0.0f;
            for (int i = 0; i < current.childCount; ++i) {
                Node& child = m_nodes[static_cast<std::size_t>(current.firstChild + i)];
                const auto at = static_cast<std::size_t>(child.move);
                if (child.move != Pass && m_first.ply[at] >= static_cast<int>(depth) && m_first.color[at] == toPlay) {
                    child.raveVisits += 1;
                    child.raveWins += result;
                }
            }
        }
    }

    void Collect(RootStats& stats) const
    {
        const Node& root = m_nodes.front();
        for (int i = 0; i < root.childCount; ++i) {
            const Node& child = m_nodes[static_cast<std::size_t>(root.firstChild + i)];
            stats.visits[static_cast<std::size_t>(i)] += child.visits;
            stats.wins[static_cast<std::size_t>(i)] += child.wins;
        }
    }

private:
    int Select(int parent) const
    {
        const Node& node = m_nodes[static_cast<std::size_t>(parent)];
        int best = node.firstChild;
        float bestValue = -1;
        for (int i = 0; i < node.childCount; ++i) {
            const Node& child = m_nodes[static_cast<std::size_t>(node.firstChild + i)];
            float value = child.wins / child.visits;
            if (child.raveVisits > 0) {
                const float beta = child.raveVisits / (child.visits + child.raveVisits + child.visits * child.raveVisits / Equivalence);
                value = (1 - beta) * value + beta * (child.raveWins / child.raveVisits);
            }
            if (value > bestValue) {
                bestValue = value;
                best = node.firstChild + i;
            }
        }
        return best;
    }

    void Expand(int index, const Board& board)
    {
        if (m_nodes.size() + static_cast<std::size_t>(board.EmptyPoints().size()) + 1 > MaxNodes)
            return;
        const Color me = board.ToPlay();
        const auto first = static_cast<std::int32_t>(m_nodes.size());
        for (const std::int16_t point : board.EmptyPoints()) {
            if (!board.IsLegal(point, me) || board.IsEye(point, me))
                continue;
            Node child;
            child.move = point;
            SetPrior(child, board, point, me, m_shapes);
            m_nodes.push_back(child);
        }
        if (static_cast<std::int32_t>(m_nodes.size()) == first) {
            Node pass;
            SetPrior(pass, board, Pass, me, m_shapes);
            m_nodes.push_back(pass);
        }
        Node& node = m_nodes[static_cast<std::size_t>(index)];
        node.firstChild = first;
        node.childCount = static_cast<std::int16_t>(static_cast<std::int32_t>(m_nodes.size()) - first);
    }

    const Board& m_root;
    double m_komi;
    Random m_random;
    const std::array<std::uint8_t, 65536>& m_shapes;
    Playout m_playout;
    std::vector<Node> m_nodes;
    std::vector<int> m_path;
    FirstMoves m_first{};
};

// A 4-4, 3-4 or 3-3 point in an empty corner, while there are only a few stones on a board of 13 or more.
Point OpeningMove(const Board& board, std::uint64_t seed)
{
    const int size = board.Size();
    if (size < 13 || static_cast<int>(board.EmptyPoints().size()) < size * size - 3)
        return Pass;
    struct Choice {
        Point point;
        int weight;
    };
    std::vector<Choice> choices;
    for (const bool right : {false, true}) {
        for (const bool bottom : {false, true}) {
            const int cornerX = right ? size - 7 : 0, cornerY = bottom ? size - 7 : 0;
            bool empty = true;
            for (int dy = 0; dy < 7 && empty; ++dy)
                for (int dx = 0; dx < 7 && empty; ++dx)
                    empty = board.At(PointAt(cornerX + dx, cornerY + dy)) == Cell::Empty;
            if (!empty)
                continue;
            auto at = [&](int x, int y) { return PointAt(right ? size - 1 - x : x, bottom ? size - 1 - y : y); };
            choices.insert(choices.end(), {{at(3, 3), 4}, {at(2, 3), 2}, {at(3, 2), 2}, {at(2, 2), 1}});
        }
    }
    int total = 0;
    for (const Choice& choice : choices)
        total += choice.weight;
    if (total == 0)
        return Pass;
    Random random(seed ^ board.Hash());
    int pick = random.Below(total);
    for (const Choice& choice : choices) {
        if (pick < choice.weight)
            return choice.point;
        pick -= choice.weight;
    }
    return Pass;
}

} // namespace

PointSet Ownership::Dead(const Board& board) const
{
    PointSet dead{};
    for (Point point = 0; point < PointCount; ++point) {
        const float share = black[static_cast<std::size_t>(point)];
        if ((board.At(point) == Cell::Black && share < 0.5f) || (board.At(point) == Cell::White && share > 0.5f))
            dead[static_cast<std::size_t>(point)] = true;
    }
    return dead;
}

SearchResult Engine::Search(const Board& board, std::span<const std::uint64_t> history, double komi, const SearchLimits& limits,
                            std::stop_token stop)
{
    const auto deadline = std::chrono::steady_clock::now() + std::chrono::milliseconds(limits.milliseconds);
    const Color me = board.ToPlay();

    // The moves at the root: legal, superko included, and not into one's own eyes.
    std::vector<Point> moves;
    for (const std::int16_t point : board.EmptyPoints()) {
        if (!board.IsLegal(point, me) || board.IsEye(point, me))
            continue;
        Board next = board;
        next.Play(point, me);
        if (std::ranges::find(history, next.Hash()) == history.end())
            moves.push_back(point);
    }
    SearchResult result;
    if (moves.empty())
        return result; // a pass

    // On the big boards the first stones go to the empty corners, which the playouts do not see the worth of.
    if (const Point opening = OpeningMove(board, limits.seed); opening != Pass && std::ranges::find(moves, opening) != moves.end()) {
        result.move = opening;
        return result;
    }

    // Independent trees, one for each thread, their visits added up at the end.
    const int threads = std::clamp(limits.threads, 1, 16);
    std::vector<RootStats> stats(static_cast<std::size_t>(threads));
    for (RootStats& s : stats) {
        s.visits.assign(moves.size(), 0.0);
        s.wins.assign(moves.size(), 0.0);
    }
    const int share = std::max(1, limits.playouts / threads);
    auto work = [&](int index) {
        RootStats& own = stats[static_cast<std::size_t>(index)];
        Tree tree(board, moves, komi, limits.seed * 7919 + static_cast<std::uint64_t>(index) * 104729 + 1);
        for (int n = 0; n < share; ++n) {
            if ((n & 15) == 0 && (stop.stop_requested() || std::chrono::steady_clock::now() >= deadline))
                break;
            tree.Simulate(own);
        }
        tree.Collect(own);
    };
    {
        std::vector<std::jthread> helpers;
        for (int i = 1; i < threads; ++i)
            helpers.emplace_back(work, i);
        work(0);
    }

    RootStats total;
    total.visits.assign(moves.size(), 0.0);
    total.wins.assign(moves.size(), 0.0);
    for (const RootStats& s : stats) {
        for (std::size_t i = 0; i < moves.size(); ++i) {
            total.visits[i] += s.visits[i];
            total.wins[i] += s.wins[i];
        }
        for (Point point = 0; point < PointCount; ++point) {
            total.black[static_cast<std::size_t>(point)] += s.black[static_cast<std::size_t>(point)];
            total.white[static_cast<std::size_t>(point)] += s.white[static_cast<std::size_t>(point)];
        }
        total.playouts += s.playouts;
    }
    const auto best = static_cast<std::size_t>(std::ranges::max_element(total.visits) - total.visits.begin());
    result.move = moves[best];
    result.winRate = total.wins[best] / std::max(1.0, total.visits[best]);
    result.playouts = static_cast<int>(total.playouts);

    // With every point settled, one way or the other or for neither, there is nothing left to play for: pass.
    if (total.playouts > 0) {
        const int size = board.Size();
        const double n = static_cast<double>(total.playouts);
        bool settled = true;
        for (int y = 0; y < size && settled; ++y) {
            for (int x = 0; x < size && settled; ++x) {
                const auto at = static_cast<std::size_t>(PointAt(x, y));
                const double black = total.black[at] / n, white = total.white[at] / n;
                settled = black > 0.85 || white > 0.85 || black + white < 0.15;
            }
        }
        if (settled)
            result.move = Pass;

        // The other side has passed: if the game ending now would be won, with the stones that the playouts
        // mostly lose taken off as dead, pass too.
        if (board.Passes() > 0) {
            PointSet dead{};
            for (Point point = 0; point < PointCount; ++point) {
                const auto at = static_cast<std::size_t>(point);
                dead[at] = (board.At(point) == Cell::Black && total.white[at] > n / 2) || (board.At(point) == Cell::White && total.black[at] > n / 2);
            }
            int margin = 0;
            for (const Cell owner : AreaOwners(board, dead))
                margin += owner == Cell::Black ? 1 : owner == Cell::White ? -1 : 0;
            if ((me == Color::Black) == (margin - komi > 0))
                result.move = Pass;
        }
    }
    // Hopeless, with enough of the game played to be sure.
    const int area = board.Size() * board.Size();
    if (limits.mayResign && total.playouts >= 2000 && result.winRate < 0.06 && static_cast<int>(board.EmptyPoints().size()) < area * 2 / 3)
        result.resign = true;
    return result;
}

Ownership Engine::Estimate(const Board& board, double komi, int playouts, std::uint64_t seed, std::stop_token stop)
{
    Random random(seed);
    Playout playout(random, ShapeTable());
    std::array<std::uint32_t, PointCount> black{}, white{};
    double margin = 0;
    int done = 0;
    for (; done < playouts && !stop.stop_requested(); ++done) {
        Board copy = board;
        copy.SetToPlay(board.ToPlay());
        margin += playout.Run(copy, komi, nullptr, 0);
        CountOwners(copy, black, white);
    }
    Ownership ownership;
    const float n = static_cast<float>(std::max(done, 1));
    for (Point point = 0; point < PointCount; ++point) {
        const auto at = static_cast<std::size_t>(point);
        // The share of Black's ownership, with the points neither owned counted half.
        ownership.black[at] = (static_cast<float>(black[at]) + 0.5f * static_cast<float>(done - static_cast<int>(black[at] + white[at]))) / n;
    }
    ownership.margin = margin / std::max(done, 1);
    return ownership;
}

} // namespace go
