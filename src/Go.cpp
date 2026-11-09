#include "Go.h"

#include <algorithm>
#include <cctype>
#include <charconv>
#include <chrono>
#include <cmath>
#include <format>
#include <system_error>
#include <type_traits>
#include <utility>

namespace go {

namespace {

constexpr std::string_view Columns = "ABCDEFGHJKLMNOPQRST"; // no I, as on Go boards

constexpr std::uint64_t SplitMix(std::uint64_t& state)
{
    std::uint64_t z = (state += 0x9E3779B97F4A7C15ull);
    z = (z ^ (z >> 30)) * 0xBF58476D1CE4E5B9ull;
    z = (z ^ (z >> 27)) * 0x94D049BB133111EBull;
    return z ^ (z >> 31);
}

// A random number for each color on each point, for the hash of a position.
constexpr auto Zobrist = [] {
    std::array<std::array<std::uint64_t, PointCount>, 2> keys{};
    std::uint64_t state = 0x5EED;
    for (auto& color : keys)
        for (std::uint64_t& key : color)
            key = SplitMix(state);
    return keys;
}();

std::uint64_t KeyOf(Point point, Color color) { return Zobrist[static_cast<std::size_t>(Index(color))][static_cast<std::size_t>(point)]; }

bool IsSpace(char c) { return c == ' ' || c == '\t' || c == '\r' || c == '\n'; }

} // namespace

std::string PointName(Point point, int size)
{
    if (point == Pass)
        return "pass";
    if (point == Resignation)
        return "resign";
    return std::format("{}{}", Columns[static_cast<std::size_t>(XOf(point))], size - YOf(point));
}

std::optional<Point> ParsePoint(std::string_view text, int size)
{
    if (text.size() < 2)
        return std::nullopt;
    const std::size_t column = Columns.find(static_cast<char>(std::toupper(static_cast<unsigned char>(text[0]))));
    int row = 0;
    const auto [end, error] = std::from_chars(text.data() + 1, text.data() + text.size(), row);
    if (column == std::string_view::npos || error != std::errc{} || end != text.data() + text.size())
        return std::nullopt;
    const int x = static_cast<int>(column), y = size - row;
    if (x >= size || y < 0 || y >= size)
        return std::nullopt;
    return PointAt(x, y);
}

std::vector<Point> StarPoints(int size)
{
    const int edge = size >= 13 ? 3 : 2, far = size - 1 - edge, middle = size / 2;
    std::vector<Point> points{PointAt(edge, edge), PointAt(far, edge), PointAt(edge, far), PointAt(far, far)};
    if (size % 2 == 1)
        points.push_back(PointAt(middle, middle));
    if (size >= 19)
        points.insert(points.end(), {PointAt(middle, edge), PointAt(middle, far), PointAt(edge, middle), PointAt(far, middle)});
    return points;
}

int MaxHandicap(int size) { return size >= 13 ? 9 : 5; }

std::vector<Point> HandicapPoints(int size, int stones)
{
    const int edge = size >= 13 ? 3 : 2, far = size - 1 - edge, middle = size / 2;
    // The corners, upper right and lower left first, then the middle and the sides.
    std::vector<Point> points{PointAt(far, edge), PointAt(edge, far), PointAt(far, far), PointAt(edge, edge)};
    if (stones >= 6)
        points.insert(points.end(), {PointAt(edge, middle), PointAt(far, middle)});
    if (stones >= 8)
        points.insert(points.end(), {PointAt(middle, edge), PointAt(middle, far)});
    if (stones % 2 == 1)
        points.push_back(PointAt(middle, middle));
    points.resize(static_cast<std::size_t>(std::min(stones, static_cast<int>(points.size()))));
    return points;
}

// Board ------------------------------------------------------------------------------------------

Board::Board(int size) : m_size(std::clamp(size, 2, MaxSize))
{
    m_cells.fill(Cell::Border);
    for (int y = 0; y < m_size; ++y) {
        for (int x = 0; x < m_size; ++x) {
            m_cells[static_cast<std::size_t>(PointAt(x, y))] = Cell::Empty;
            AddEmpty(PointAt(x, y));
        }
    }
}

void Board::AddEmpty(Point point)
{
    m_emptyIndex[static_cast<std::size_t>(point)] = static_cast<std::int16_t>(m_emptyCount);
    m_empty[static_cast<std::size_t>(m_emptyCount++)] = static_cast<std::int16_t>(point);
}

void Board::RemoveEmpty(Point point)
{
    const int index = m_emptyIndex[static_cast<std::size_t>(point)];
    const std::int16_t last = m_empty[static_cast<std::size_t>(--m_emptyCount)];
    m_empty[static_cast<std::size_t>(index)] = last;
    m_emptyIndex[static_cast<std::size_t>(last)] = static_cast<std::int16_t>(index);
}

void Board::AddLiberty(Point head, Point liberty)
{
    const auto h = static_cast<std::size_t>(head);
    ++m_libs[h];
    m_libSum[h] += liberty;
    m_libSumSquares[h] += liberty * liberty;
}

void Board::RemoveLiberty(Point head, Point liberty)
{
    const auto h = static_cast<std::size_t>(head);
    --m_libs[h];
    m_libSum[h] -= liberty;
    m_libSumSquares[h] -= liberty * liberty;
}

bool Board::InAtari(Point point) const
{
    const auto h = static_cast<std::size_t>(Head(point));
    const std::int64_t libs = m_libs[h];
    return libs > 0 && libs * m_libSumSquares[h] == static_cast<std::int64_t>(m_libSum[h]) * m_libSum[h];
}

Point Board::AtariLiberty(Point point) const
{
    const auto h = static_cast<std::size_t>(Head(point));
    return m_libSum[h] / m_libs[h];
}

int Board::Liberties(Point point, int limit, Point* found) const
{
    int count = 0;
    const Point head = Head(point);
    Point stone = head;
    do {
        for (const int step : Neighbours) {
            const Point next = stone + step;
            if (At(next) == Cell::Empty && std::find(found, found + count, next) == found + count) {
                found[count++] = next;
                if (count >= limit)
                    return count;
            }
        }
        stone = NextStone(stone);
    } while (stone != head);
    return count;
}

bool Board::IsLegal(Point point, Color color) const
{
    if (point == Pass)
        return true;
    if (!IsOnBoard(point) || At(point) != Cell::Empty || point == m_ko)
        return false;
    const Cell own = CellOf(color);
    for (const int step : Neighbours) {
        const Point next = point + step;
        const Cell cell = At(next);
        if (cell == Cell::Empty)
            return true;
        if (cell == Cell::Border)
            continue;
        // A string of one's own with another liberty, or one of the other side's that the move takes.
        if ((cell == own) != InAtari(next))
            return true;
    }
    return false;
}

bool Board::IsEye(Point point, Color color) const
{
    const Cell own = CellOf(color), other = CellOf(Opposite(color));
    bool edge = false;
    for (const int step : Neighbours) {
        const Cell cell = At(point + step);
        if (cell == Cell::Border)
            edge = true;
        else if (cell != own)
            return false;
    }
    int enemies = 0;
    for (const int step : Diagonals)
        enemies += At(point + step) == other ? 1 : 0;
    return enemies < (edge ? 1 : 2);
}

bool Board::IsSelfAtari(Point point, Color color) const
{
    const Cell own = CellOf(color);
    std::array<Point, 2> liberties{};
    int count = 0;
    auto add = [&](Point liberty) {
        if (liberty != point && std::find(liberties.begin(), liberties.begin() + count, liberty) == liberties.begin() + count)
            liberties[static_cast<std::size_t>(count++)] = liberty;
        return count >= 2;
    };
    for (const int step : Neighbours)
        if (At(point + step) == Cell::Empty && add(point + step))
            return false;
    for (const int step : Neighbours) {
        const Point next = point + step;
        const Cell cell = At(next);
        if (cell == Cell::Empty || cell == Cell::Border)
            continue;
        if (cell != own) {
            if (InAtari(next))
                return false; // it takes the string, and gains a liberty where it stood
            continue;
        }
        std::array<Point, 3> found{};
        const int libs = Liberties(next, 3, found.data());
        for (int i = 0; i < libs; ++i)
            if (add(found[static_cast<std::size_t>(i)]))
                return false;
    }
    return true;
}

void Board::Merge(Point a, Point b)
{
    if (m_stones[static_cast<std::size_t>(a)] < m_stones[static_cast<std::size_t>(b)])
        std::swap(a, b);
    Point stone = b;
    do {
        m_head[static_cast<std::size_t>(stone)] = static_cast<std::int16_t>(a);
        stone = NextStone(stone);
    } while (stone != b);
    std::swap(m_next[static_cast<std::size_t>(a)], m_next[static_cast<std::size_t>(b)]);
    const auto to = static_cast<std::size_t>(a), from = static_cast<std::size_t>(b);
    m_stones[to] = static_cast<std::int16_t>(m_stones[to] + m_stones[from]);
    m_libs[to] = static_cast<std::int16_t>(m_libs[to] + m_libs[from]);
    m_libSum[to] += m_libSum[from];
    m_libSumSquares[to] += m_libSumSquares[from];
}

int Board::RemoveString(Point head, std::vector<Point>* captured)
{
    const Color color = At(head) == Cell::Black ? Color::Black : Color::White;
    int count = 0;
    Point stone = head;
    do {
        m_cells[static_cast<std::size_t>(stone)] = Cell::Empty;
        m_hash ^= KeyOf(stone, color);
        AddEmpty(stone);
        if (captured)
            captured->push_back(stone);
        ++count;
        stone = NextStone(stone);
    } while (stone != head);
    // The points freed are liberties again of the strings round them.
    do {
        for (const int step : Neighbours)
            if (IsStone(stone + step))
                AddLiberty(Head(stone + step), stone);
        stone = NextStone(stone);
    } while (stone != head);
    return count;
}

void Board::Play(Point point, Color color, std::vector<Point>* captured)
{
    m_ko = Pass;
    m_toPlay = Opposite(color);
    m_lastMove = point;
    if (point == Pass) {
        ++m_passes;
        return;
    }
    m_passes = 0;

    const Cell own = CellOf(color), other = CellOf(Opposite(color));
    const auto p = static_cast<std::size_t>(point);
    RemoveEmpty(point);
    m_cells[p] = own;
    m_hash ^= KeyOf(point, color);
    m_head[p] = static_cast<std::int16_t>(point);
    m_next[p] = static_cast<std::int16_t>(point);
    m_stones[p] = 1;
    m_libs[p] = 0;
    m_libSum[p] = 0;
    m_libSumSquares[p] = 0;
    for (const int step : Neighbours) {
        const Point next = point + step;
        if (At(next) == Cell::Empty)
            AddLiberty(point, next);
        else if (IsStone(next))
            RemoveLiberty(Head(next), point);
    }
    for (const int step : Neighbours)
        if (At(point + step) == own && Head(point + step) != Head(point))
            Merge(Head(point), Head(point + step));

    int taken = 0;
    Point lastTaken = Pass;
    for (const int step : Neighbours) {
        const Point next = point + step;
        if (At(next) == other && m_libs[static_cast<std::size_t>(Head(next))] == 0) {
            lastTaken = next;
            taken += RemoveString(Head(next), captured);
        }
    }
    m_captures[static_cast<std::size_t>(Index(color))] += taken;
    // One stone taken by a lone stone that is left with that point as its only liberty: a ko.
    const auto head = static_cast<std::size_t>(Head(point));
    if (taken == 1 && m_stones[head] == 1 && m_libs[head] == 1)
        m_ko = lastTaken;
}

void Board::Setup(Point point, Color color)
{
    const Color toPlay = m_toPlay;
    Play(point, color);
    m_toPlay = toPlay;
    m_lastMove = Pass;
    m_ko = Pass;
}

int Board::QuickScore() const
{
    int score = 0;
    for (int y = 0; y < m_size; ++y) {
        for (int x = 0; x < m_size; ++x) {
            const Point point = PointAt(x, y);
            const Cell cell = At(point);
            if (cell == Cell::Black) {
                ++score;
            }
            else if (cell == Cell::White) {
                --score;
            }
            else {
                bool black = false, white = false;
                for (const int step : Neighbours) {
                    black |= At(point + step) == Cell::Black;
                    white |= At(point + step) == Cell::White;
                }
                score += black == white ? 0 : black ? 1 : -1;
            }
        }
    }
    return score;
}

// Game -------------------------------------------------------------------------------------------

void Game::Reset(int size, int handicap)
{
    Board start(size);
    handicap = handicap >= 2 ? std::min(handicap, MaxHandicap(start.Size())) : 0;
    for (const Point point : HandicapPoints(start.Size(), handicap))
        start.Setup(point, Color::Black);
    if (handicap > 0)
        start.SetToPlay(Color::White);
    Begin(start, handicap, handicap > 0 ? 0.5 : 7.5);
}

void Game::Begin(const Board& start, int handicap, double komi)
{
    m_handicap = handicap;
    m_komi = komi;
    m_positions.assign(1, start);
    m_hashes.assign(1, start.Hash());
    m_moves.clear();
    m_redo.clear();
    m_dead = {};
    m_accepted = false;
}

Phase Game::GetPhase() const
{
    if (!m_moves.empty() && m_moves.back().IsResignation())
        return Phase::Finished;
    if (Current().Passes() >= 2)
        return m_accepted ? Phase::Finished : Phase::Scoring;
    return Phase::Playing;
}

bool Game::IsLegal(Point point) const
{
    if (GetPhase() != Phase::Playing)
        return false;
    const Board& board = Current();
    if (point == Pass)
        return true;
    if (!board.IsLegal(point, board.ToPlay()))
        return false;
    Board next = board;
    next.Play(point, board.ToPlay());
    return std::ranges::find(m_hashes, next.Hash()) == m_hashes.end();
}

void Game::Push(Point point, Color color)
{
    MoveRecord record;
    record.point = point;
    record.color = color;
    Board next = Current();
    if (point != Resignation)
        next.Play(point, record.color, &record.captured);
    m_positions.push_back(next);
    m_hashes.push_back(next.Hash());
    m_moves.push_back(std::move(record));
    m_dead = {};
    m_accepted = false;
}

bool Game::Play(Point point)
{
    if (!IsLegal(point))
        return false;
    Push(point, Current().ToPlay());
    m_redo.clear();
    return true;
}

void Game::Resign(Color who)
{
    if (GetPhase() == Phase::Finished)
        return;
    Push(Resignation, who);
    m_redo.clear();
}

bool Game::Undo()
{
    if (m_moves.empty())
        return false;
    m_redo.push_back(std::move(m_moves.back()));
    m_moves.pop_back();
    m_positions.pop_back();
    m_hashes.pop_back();
    m_dead = {};
    m_accepted = false;
    return true;
}

bool Game::Redo()
{
    if (m_redo.empty())
        return false;
    Push(m_redo.back().point, m_redo.back().color);
    m_redo.pop_back();
    return true;
}

void Game::SetDead(const PointSet& dead)
{
    m_dead = {};
    const Board& board = Current();
    for (Point point = 0; point < PointCount; ++point)
        m_dead[static_cast<std::size_t>(point)] = dead[static_cast<std::size_t>(point)] && board.IsStone(point);
}

void Game::ToggleDead(Point point)
{
    const Board& board = Current();
    if (!board.IsStone(point))
        return;
    // The stones of the color clicked in the area they share: reached through their own stones and empty points.
    const Cell color = board.At(point);
    const bool dead = !m_dead[static_cast<std::size_t>(point)];
    PointSet seen{};
    std::vector<Point> stack{point};
    seen[static_cast<std::size_t>(point)] = true;
    while (!stack.empty()) {
        const Point at = stack.back();
        stack.pop_back();
        if (board.At(at) == color)
            m_dead[static_cast<std::size_t>(at)] = dead;
        for (const int step : Neighbours) {
            const Point next = at + step;
            const Cell cell = board.At(next);
            if (!seen[static_cast<std::size_t>(next)] && (cell == color || cell == Cell::Empty)) {
                seen[static_cast<std::size_t>(next)] = true;
                stack.push_back(next);
            }
        }
    }
}

std::array<Cell, PointCount> AreaOwners(const Board& board, const PointSet& dead)
{
    std::array<Cell, PointCount> cells{}, owner{};
    for (Point point = 0; point < PointCount; ++point) {
        cells[static_cast<std::size_t>(point)] = dead[static_cast<std::size_t>(point)] ? Cell::Empty : board.At(point);
        owner[static_cast<std::size_t>(point)] = Cell::Empty;
    }
    PointSet seen{};
    std::vector<Point> region, stack;
    for (Point point = 0; point < PointCount; ++point) {
        const Cell cell = cells[static_cast<std::size_t>(point)];
        if (cell == Cell::Black || cell == Cell::White) {
            owner[static_cast<std::size_t>(point)] = cell;
            continue;
        }
        if (cell != Cell::Empty || seen[static_cast<std::size_t>(point)])
            continue;
        // An empty region: whose stones it touches.
        bool black = false, white = false;
        region.clear();
        stack.assign(1, point);
        seen[static_cast<std::size_t>(point)] = true;
        while (!stack.empty()) {
            const Point at = stack.back();
            stack.pop_back();
            region.push_back(at);
            for (const int step : Neighbours) {
                const Point next = at + step;
                const Cell around = cells[static_cast<std::size_t>(next)];
                black |= around == Cell::Black;
                white |= around == Cell::White;
                if (around == Cell::Empty && !seen[static_cast<std::size_t>(next)]) {
                    seen[static_cast<std::size_t>(next)] = true;
                    stack.push_back(next);
                }
            }
        }
        if (black != white)
            for (const Point at : region)
                owner[static_cast<std::size_t>(at)] = black ? Cell::Black : Cell::White;
    }
    return owner;
}

std::array<Cell, PointCount> Game::Territory() const
{
    return AreaOwners(Current(), m_dead);
}

Score Game::Count() const
{
    Score score{0, 0, m_komi};
    const auto owner = Territory();
    for (Point point = 0; point < PointCount; ++point) {
        score.black += owner[static_cast<std::size_t>(point)] == Cell::Black ? 1 : 0;
        score.white += owner[static_cast<std::size_t>(point)] == Cell::White ? 1 : 0;
    }
    return score;
}

std::optional<double> Game::Margin() const
{
    if (GetPhase() != Phase::Finished)
        return std::nullopt;
    if (m_moves.back().IsResignation())
        return m_moves.back().color == Color::Black ? -1000.0 : 1000.0;
    return Count().Margin();
}

std::optional<Color> Game::Winner() const
{
    const auto margin = Margin();
    if (!margin || *margin == 0)
        return std::nullopt;
    return *margin > 0 ? Color::Black : Color::White;
}

std::string Game::ResultText() const
{
    const auto margin = Margin();
    if (!margin)
        return {};
    if (*margin == 0)
        return "0";
    const char winner = *margin > 0 ? 'B' : 'W';
    if (m_moves.back().IsResignation())
        return std::format("{}+R", winner);
    return std::format("{}+{}", winner, std::abs(*margin));
}

// SGF --------------------------------------------------------------------------------------------

namespace {

// The number at the start of an SGF value, such as the 19 of "19" or "19:19", read the same in any locale; the
// fallback where there is none.
template <typename Number>
Number ReadNumber(std::string_view text, Number fallback)
{
    const std::size_t start = text.find_first_not_of(" \t\r\n+");
    if (start == std::string_view::npos)
        return fallback;
    text.remove_prefix(start);
    Number value{};
    const auto [end, error] = std::from_chars(text.data(), text.data() + text.size(), value);
    if (error != std::errc{})
        return fallback;
    if constexpr (std::is_floating_point_v<Number>) {
        if (!std::isfinite(value))
            return fallback;
    }
    return value;
}

std::string SgfPoint(Point point)
{
    if (point == Pass)
        return {};
    return {static_cast<char>('a' + XOf(point)), static_cast<char>('a' + YOf(point))};
}

std::string Escape(std::string_view text)
{
    std::string escaped;
    for (const char c : text) {
        if (c == ']' || c == '\\')
            escaped += '\\';
        escaped += c;
    }
    return escaped;
}

struct Property {
    std::string name;
    std::vector<std::string> values;
};

using Node = std::vector<Property>;

// The nodes of the main line: at each fork, the first variation.
class SgfReader {
public:
    explicit SgfReader(std::string_view text) : m_text(text) {}

    std::expected<std::vector<Node>, std::string> MainLine()
    {
        SkipSpace();
        if (!Accept('('))
            return std::unexpected("not an SGF game");
        std::vector<Node> nodes;
        if (auto error = Sequence(nodes); !error.empty())
            return std::unexpected(error);
        return nodes;
    }

private:
    std::string Sequence(std::vector<Node>& nodes)
    {
        for (;;) {
            SkipSpace();
            if (m_at >= m_text.size())
                return {}; // a game cut short is read as far as it goes
            const char c = m_text[m_at];
            if (c == ';') {
                ++m_at;
                nodes.push_back(ReadNode());
            }
            else if (c == '(') {
                // The first variation is followed; the others are skipped.
                ++m_at;
                if (auto error = Sequence(nodes); !error.empty())
                    return error;
                SkipVariations();
                return {};
            }
            else if (c == ')') {
                ++m_at;
                return {};
            }
            else {
                return std::format("unexpected '{}'", c);
            }
        }
    }

    Node ReadNode()
    {
        Node node;
        for (;;) {
            SkipSpace();
            std::string name;
            while (m_at < m_text.size() && std::isalpha(static_cast<unsigned char>(m_text[m_at]))) {
                if (std::isupper(static_cast<unsigned char>(m_text[m_at])))
                    name += m_text[m_at];
                ++m_at;
            }
            if (name.empty())
                return node;
            Property property{name, {}};
            SkipSpace();
            while (m_at < m_text.size() && m_text[m_at] == '[') {
                ++m_at;
                std::string value;
                while (m_at < m_text.size() && m_text[m_at] != ']') {
                    if (m_text[m_at] == '\\' && m_at + 1 < m_text.size())
                        ++m_at;
                    value += m_text[m_at++];
                }
                ++m_at;
                property.values.push_back(std::move(value));
                SkipSpace();
            }
            node.push_back(std::move(property));
        }
    }

    // After the first variation, the rest up to the end of the enclosing sequence.
    void SkipVariations()
    {
        int depth = 0;
        bool inValue = false;
        while (m_at < m_text.size()) {
            const char c = m_text[m_at++];
            if (inValue) {
                if (c == '\\')
                    ++m_at;
                else if (c == ']')
                    inValue = false;
            }
            else if (c == '[') {
                inValue = true;
            }
            else if (c == '(') {
                ++depth;
            }
            else if (c == ')') {
                if (depth-- == 0)
                    return;
            }
        }
    }

    void SkipSpace()
    {
        while (m_at < m_text.size() && IsSpace(m_text[m_at]))
            ++m_at;
    }

    bool Accept(char c)
    {
        if (m_at < m_text.size() && m_text[m_at] == c) {
            ++m_at;
            return true;
        }
        return false;
    }

    std::string_view m_text;
    std::size_t m_at = 0;
};

const Property* Find(const Node& node, std::string_view name)
{
    for (const Property& property : node)
        if (property.name == name)
            return &property;
    return nullptr;
}

// "dd", or a rectangle "aa:cc" in a list of setup stones; "" and "tt" are a pass.
std::vector<Point> ReadPoints(std::string_view value, int size)
{
    auto one = [&](std::string_view text) -> std::optional<std::pair<int, int>> {
        if (text.size() != 2)
            return std::nullopt;
        const int x = text[0] - 'a', y = text[1] - 'a';
        if (x < 0 || y < 0 || x >= size || y >= size)
            return std::nullopt;
        return std::pair{x, y};
    };
    const std::size_t colon = value.find(':');
    const auto first = one(value.substr(0, colon));
    if (!first)
        return {};
    auto last = colon == std::string_view::npos ? first : one(value.substr(colon + 1));
    if (!last)
        last = first;
    std::vector<Point> points;
    for (int y = std::min(first->second, last->second); y <= std::max(first->second, last->second); ++y)
        for (int x = std::min(first->first, last->first); x <= std::max(first->first, last->first); ++x)
            points.push_back(PointAt(x, y));
    return points;
}

} // namespace

std::string Game::ToSgf(std::string_view blackName, std::string_view whiteName) const
{
    const auto today = std::chrono::year_month_day{std::chrono::floor<std::chrono::days>(std::chrono::system_clock::now())};
    std::string sgf = std::format("(;GM[1]FF[4]CA[UTF-8]AP[3D Go:1.0]SZ[{}]KM[{}]RU[Chinese]DT[{:04}-{:02}-{:02}]", Size(), m_komi,
                                  static_cast<int>(today.year()), static_cast<unsigned>(today.month()), static_cast<unsigned>(today.day()));
    if (!blackName.empty())
        sgf += std::format("PB[{}]", Escape(blackName));
    if (!whiteName.empty())
        sgf += std::format("PW[{}]", Escape(whiteName));
    if (m_handicap > 0)
        sgf += std::format("HA[{}]", m_handicap);
    // The stones on the board at the start.
    std::string black, white;
    for (Point point = 0; point < PointCount; ++point) {
        if (Start().At(point) == Cell::Black)
            black += '[' + SgfPoint(point) + ']';
        else if (Start().At(point) == Cell::White)
            white += '[' + SgfPoint(point) + ']';
    }
    if (!black.empty())
        sgf += "AB" + black;
    if (!white.empty())
        sgf += "AW" + white;
    if (Start().ToPlay() == Color::White)
        sgf += "PL[W]";
    if (const std::string result = ResultText(); !result.empty())
        sgf += std::format("RE[{}]", result);
    int count = 0;
    for (const MoveRecord& record : m_moves) {
        if (record.IsResignation())
            continue;
        sgf += std::format("{};{}[{}]", ++count % 10 == 0 ? "\n" : "", record.color == Color::Black ? 'B' : 'W', SgfPoint(record.point));
    }
    return sgf + ")\n";
}

std::expected<Game, std::string> Game::FromSgf(std::string_view text)
{
    // Whatever comes before the game, such as a message it was pasted from, is skipped.
    const std::size_t begin = text.find("(;");
    if (begin == std::string_view::npos)
        return std::unexpected("no SGF game");
    auto nodes = SgfReader(text.substr(begin)).MainLine();
    if (!nodes)
        return std::unexpected(nodes.error());
    if (nodes->empty())
        return std::unexpected("an empty game");

    const Node& root = nodes->front();
    int size = 19;
    if (const Property* property = Find(root, "SZ"); property && !property->values.empty())
        size = ReadNumber(property->values.front(), 0);
    if (size < 2 || size > MaxSize)
        return std::unexpected(std::format("a board of {} is not supported", size));
    int handicap = 0;
    if (const Property* property = Find(root, "HA"); property && !property->values.empty())
        handicap = std::max(0, ReadNumber(property->values.front(), 0));
    double komi = handicap >= 2 ? 0.5 : 7.5;
    if (const Property* property = Find(root, "KM"); property && !property->values.empty())
        komi = ReadNumber(property->values.front(), komi);

    // The setup stones, or the usual handicap when none are given.
    Board start(size);
    bool setup = false;
    for (const auto& [name, color] : {std::pair{"AB", Color::Black}, std::pair{"AW", Color::White}}) {
        if (const Property* property = Find(root, name)) {
            for (const std::string& value : property->values) {
                for (const Point point : ReadPoints(value, size)) {
                    if (start.At(point) == Cell::Empty && start.IsLegal(point, color)) {
                        start.Setup(point, color);
                        setup = true;
                    }
                }
            }
        }
    }
    if (!setup && handicap >= 2) {
        for (const Point point : HandicapPoints(size, handicap))
            start.Setup(point, Color::Black);
    }
    if (handicap >= 2)
        start.SetToPlay(Color::White);
    if (const Property* property = Find(root, "PL"); property && !property->values.empty())
        start.SetToPlay(property->values.front() == "W" || property->values.front() == "w" ? Color::White : Color::Black);

    Game game;
    game.Begin(start, handicap, komi);
    int number = 0;
    for (const Node& node : *nodes) {
        for (const auto& [name, color] : {std::pair{"B", Color::Black}, std::pair{"W", Color::White}}) {
            const Property* property = Find(node, name);
            if (!property || property->values.empty())
                continue;
            ++number;
            if (game.GetPhase() != Phase::Playing)
                return std::unexpected(std::format("move {} comes after the end", number));
            // A move out of turn: the other side passed.
            if (game.Current().ToPlay() != color)
                game.Push(Pass, Opposite(color));
            const std::string& value = property->values.front();
            Point point = Pass;
            if (!value.empty() && !(value == "tt" && size <= 19)) {
                const auto points = ReadPoints(value, size);
                if (points.size() != 1)
                    return std::unexpected(std::format("move {} is not a point", number));
                point = points.front();
            }
            if (!game.IsLegal(point))
                return std::unexpected(std::format("move {} ({}) is not legal", number, PointName(point, size)));
            game.Push(point, color);
        }
    }
    if (const Property* property = Find(root, "RE"); property && !property->values.empty()) {
        const std::string& result = property->values.front();
        if (result.size() >= 3 && result.substr(1, 2) == "+R" && game.GetPhase() == Phase::Playing) {
            // The side that lost resigned.
            game.Push(Resignation, std::toupper(static_cast<unsigned char>(result[0])) == 'B' ? Color::White : Color::Black);
        }
    }
    game.m_redo.clear();
    return game;
}

} // namespace go
