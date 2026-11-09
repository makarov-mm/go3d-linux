#pragma once

#include "Go.h"

#include <array>
#include <atomic>
#include <cstdint>
#include <mutex>
#include <optional>
#include <span>
#include <stop_token>
#include <thread>
#include <utility>

namespace go {

struct SearchLimits {
    int playouts = 10000;     // in all, over the threads
    int milliseconds = 2000;
    int threads = 1;
    bool mayResign = true;
    std::uint64_t seed = 0;
};

struct SearchResult {
    Point move = Pass;
    bool resign = false;
    double winRate = 0.5; // for the side to move
    int playouts = 0;
};

// Who owns each point at the end of the game, as the share of playouts in which Black did, and the mean score.
struct Ownership {
    std::array<float, PointCount> black{};
    double margin = 0; // Black's points less White's and komi
    // The stones that are probably dead, for scoring.
    PointSet Dead(const Board& board) const;
};

// Monte Carlo tree search: playouts with the usual heuristics of Go programs (answering ataris, taking stones,
// 3×3 shapes near the last move, no filling of eyes or putting big strings in atari), a tree with priors and
// all-moves-as-first (RAVE) statistics, and independent trees on several threads whose results are added up.
class Engine {
public:
    // The move for the side to move; `history` holds the earlier positions, for superko.
    static SearchResult Search(const Board& board, std::span<const std::uint64_t> history, double komi, const SearchLimits& limits,
                               std::stop_token stop);
    // The position played out many times.
    static Ownership Estimate(const Board& board, double komi, int playouts, std::uint64_t seed, std::stop_token stop);
};

// Runs a computation on a thread of its own; the window polls for the result.
template <class Result>
class Worker {
public:
    Worker() = default;
    ~Worker() { Cancel(); }
    Worker(const Worker&) = delete;
    Worker& operator=(const Worker&) = delete;

    template <class Function>
    void Start(Function function)
    {
        Cancel();
        m_busy = true;
        m_thread = std::jthread([this, function = std::move(function)](std::stop_token stop) {
            Result result = function(stop);
            if (!stop.stop_requested()) {
                std::scoped_lock lock(m_mutex);
                m_result = std::move(result);
            }
            m_busy = false;
        });
    }

    void Cancel()
    {
        if (m_thread.joinable()) {
            m_thread.request_stop();
            m_thread.join();
        }
        m_busy = false;
        std::scoped_lock lock(m_mutex);
        m_result.reset();
    }

    bool Busy() const { return m_busy; }

    std::optional<Result> Take()
    {
        std::scoped_lock lock(m_mutex);
        return std::exchange(m_result, std::nullopt);
    }

private:
    std::jthread m_thread;
    std::mutex m_mutex;
    std::optional<Result> m_result;
    std::atomic<bool> m_busy = false;
};

} // namespace go
