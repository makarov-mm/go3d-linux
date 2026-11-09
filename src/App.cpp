#include "App.h"

#include "Desktop.h"
#include "StoneModel.h"
#include "Themes.h"

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <format>
#include <random>
#include <stdexcept>
#include <thread>
#include <utility>

using namespace math;
using go::Color;
using go::Point;
using ui::FontId;
using ui::Rect;
using ui::Rgba;

namespace {

constexpr float PanelWidth = 344, PanelMargin = 16;

enum Widget {
    HelpButton = 1,
    PassButton,
    ResignButton,
    EstimateButton,
    AcceptButton,
    ResumeButton,
    UndoButton,
    RedoButton,
    HintButton,
    NewGameButton,
    FlipButton,
    ModeSelector,
    LevelSelector = ModeSelector + 2,
    BoardSelector = LevelSelector + 2,
    HandicapSelector = BoardSelector + 2,
    ThemeSelector = HandicapSelector + 2,
    SoundButton = ThemeSelector + 2,
    CoordinatesButton,
    LanguageButton,
    AboutButton,
    ModalWidgets = 100, // the widgets of the cards over the game
    HelpClose,
    AboutClose,
    AboutLinks = 110,
};

constexpr ui::Color TextColor = Rgba(236, 239, 245);
constexpr ui::Color MutedColor = Rgba(150, 158, 176);
constexpr ui::Color BlackStone = Rgba(22, 22, 26);
constexpr ui::Color WhiteStone = Rgba(238, 236, 230);
constexpr ui::Color StoneRim = Rgba(120, 125, 140);
constexpr std::array<int, 3> BoardSizes{9, 13, 19};

// The board size of the choices next to `size` one way or the other, from a size among them or another one, such as
// that of a game pasted; past the end, round to the other end or not.
int NextBoardSize(int size, int step, bool wrap)
{
    if (step > 0) {
        const auto next = std::ranges::upper_bound(BoardSizes, size);
        return next != BoardSizes.end() ? *next : wrap ? BoardSizes.front() : BoardSizes.back();
    }
    const auto next = std::ranges::lower_bound(BoardSizes, size);
    return next != BoardSizes.begin() ? *(next - 1) : wrap ? BoardSizes.back() : BoardSizes.front();
}

int Threads()
{
    return std::clamp(static_cast<int>(std::thread::hardware_concurrency()) - 1, 1, 8);
}

// The playouts, the time and the threads of each level.
go::SearchLimits LevelLimits(int level, std::uint64_t seed)
{
    const int threads = Threads();
    switch (level) {
    case 1: return {300, 1000, 1, true, seed};
    case 2: return {1500, 1500, std::min(threads, 2), true, seed};
    case 3: return {6000, 2000, threads, true, seed};
    case 4: return {30000, 2500, threads, true, seed};
    default: return {200000, 6000, threads, true, seed};
    }
}

// Playouts for an estimate of the score: enough to be steady, and quick on the big board too.
int EstimatePlayouts(int size)
{
    return size <= 9 ? 4000 : size <= 13 ? 3000 : 2400;
}

std::uint64_t NextSeed()
{
    static std::mt19937_64 random{std::random_device{}()};
    return random();
}

const Theme& ThemeAt(int index) { return Themes()[static_cast<std::size_t>(index)]; }

constexpr ui::Color StoneColor(Color side) { return side == Color::Black ? BlackStone : WhiteStone; }

// A little stone for the panel, with a rim and a gleam.
void DrawStone(ui::Painter& painter, float x, float y, float radius, Color side)
{
    painter.Disc(x, y, radius, StoneRim);
    painter.Disc(x, y, radius - std::max(1.0f, radius * 0.1f), StoneColor(side));
    painter.Disc(x - radius * 0.28f, y - radius * 0.3f, radius * 0.38f, Rgba(255, 255, 255, side == Color::Black ? 46 : 120));
}

// Where a point's marks are in the scene.
std::size_t MarkIndex(Point point)
{
    return static_cast<std::size_t>(go::XOf(point) + go::MaxSize * go::YOf(point));
}

// The text on one line, for the settings file.
std::string OneLine(std::string_view text)
{
    std::string line(text.substr(0, text.find_last_not_of('\n') + 1));
    std::ranges::replace(line, '\n', ' ');
    return line;
}

// The letter of a key for the shortcuts: by its label in Latin layouts (Z on a German keyboard is where it is
// marked), and by its place on a US keyboard in the others, such as the Russian one, as Windows does.
char ShortcutLetter(const SDL_Keysym& key)
{
    if (key.sym >= SDLK_a && key.sym <= SDLK_z)
        return static_cast<char>(key.sym);
    if (key.sym >= 0x80 && key.scancode >= SDL_SCANCODE_A && key.scancode <= SDL_SCANCODE_Z)
        return static_cast<char>('a' + (key.scancode - SDL_SCANCODE_A));
    return 0;
}

// The computer level of a digit key, on the main keys or the keypad, or 0.
int LevelKey(const SDL_Keysym& key)
{
    if (key.scancode >= SDL_SCANCODE_1 && key.scancode <= SDL_SCANCODE_5)
        return key.scancode - SDL_SCANCODE_1 + 1;
    if (key.scancode >= SDL_SCANCODE_KP_1 && key.scancode <= SDL_SCANCODE_KP_5)
        return key.scancode - SDL_SCANCODE_KP_1 + 1;
    return 0;
}

} // namespace

// Setup --------------------------------------------------------------------------------------------

App::SdlSession::SdlSession()
{
    SDL_SetHint(SDL_HINT_APP_NAME, "3D Go");
    // A game in a window rather than a full-screen one: the desktop goes on compositing it, the screen saver may
    // start, and leaving full screen for another window does not minimize it.
    SDL_SetHint(SDL_HINT_VIDEO_X11_NET_WM_BYPASS_COMPOSITOR, "0");
    SDL_SetHint(SDL_HINT_VIDEO_ALLOW_SCREENSAVER, "1");
    SDL_SetHint(SDL_HINT_VIDEO_MINIMIZE_ON_FOCUS_LOSS, "0");
    if (SDL_Init(SDL_INIT_VIDEO) != 0)
        throw std::runtime_error(std::format("The display could not be opened: {}", SDL_GetError()));
}

App::SdlSession::~SdlSession() { SDL_Quit(); }

App::App()
{
    m_settings = Settings::Load();
    m_language = m_settings.language;
    m_theme = m_settings.theme;
    m_level = m_settings.level;
    m_mode = m_settings.mode;
    m_boardSize = m_settings.boardSize;
    m_handicap = m_settings.handicap;
    m_coordinates = m_settings.coordinates;
    m_sound.SetEnabled(m_settings.sound);
    m_panelVisible = m_settings.panel;
    m_panelSlide = m_panelVisible ? 1.0f : 0.0f;

    const resources::Image icon = resources::Png("go3d.png");
    CreateMainWindow(icon);
    m_context = std::make_unique<gl::Context>(m_window.get());
    render::Program::SetVersion(m_context->Version());
    m_painter = std::make_unique<ui::Painter>();
    m_painter->SetIcon(icon);
    SDL_ShowWindow(m_window.get());
    if (m_settings.maximized)
        SDL_MaximizeWindow(m_window.get());
    UpdateSize();
    BuildFonts();
    ShowLoading();

    if (auto saved = go::Game::FromSgf(m_settings.game))
        m_game = std::move(*saved);
    else
        m_game = go::Game(m_boardSize, m_handicap);
    m_layout = board::Layout::For(m_game.Size());

    m_renderer = std::make_unique<render::Renderer>(stones::BuildStone(), m_context->Version());
    m_renderer->SetTheme(ThemeAt(m_theme));
    m_renderer->SetLayout(m_layout);
    m_view = std::make_unique<BoardView>();
    m_view->SetLayout(m_layout);
    m_view->Rebuild(m_game, true);
    m_camera.SetReach(m_layout.Extent());
    m_camera.Reset(ViewSide());
    m_announcedEnd = Phase() == go::Phase::Finished;
    m_lastFrame = std::chrono::steady_clock::now();
}

App::~App()
{
    StopWorkers();
}

void App::CreateMainWindow(const resources::Image& icon)
{
    gl::Context::RequestPixelFormat();
    // Hidden until it has its size, which depends on the desktop's scaling, which is known from a window.
    m_window.reset(SDL_CreateWindow("3D Go", SDL_WINDOWPOS_UNDEFINED, SDL_WINDOWPOS_UNDEFINED, 1280, 800,
                                    SDL_WINDOW_OPENGL | SDL_WINDOW_RESIZABLE | SDL_WINDOW_ALLOW_HIGHDPI | SDL_WINDOW_HIDDEN));
    if (!m_window)
        throw std::runtime_error(std::format("The window could not be created: {}", SDL_GetError()));
    SDL_Window* window = m_window.get();
    desktop::SetIcon(window, icon);
    SDL_StopTextInput(); // keys only: no input method in the way
    m_textScale = desktop::TextScale(window);

    // The last size, or most of the screen's work area, centered in it. Room is left for the frame, so that on a small
    // screen with a large scale the window can be seen whole even at its least size.
    const int display = std::max(0, SDL_GetWindowDisplayIndex(window));
    SDL_Rect work{0, 0, 1920, 1080};
    if (SDL_GetDisplayUsableBounds(display, &work) != 0)
        SDL_GetDisplayBounds(display, &work);
    const auto units = [&](int size) { return static_cast<int>(std::lround(static_cast<float>(size) * m_textScale)); };
    const int roomWidth = work.w * 92 / 100, roomHeight = work.h * 92 / 100;
    int width = m_settings.windowWidth, height = m_settings.windowHeight;
    if (width <= 0 || height <= 0) {
        width = std::min(units(1500), roomWidth);
        height = std::min(units(920), roomHeight);
    }
    width = std::min(width, work.w);
    height = std::min(height, work.h);
    SDL_SetWindowMinimumSize(window, std::min(units(900), roomWidth), std::min(units(680), roomHeight));
    SDL_SetWindowSize(window, width, height);
    SDL_SetWindowPosition(window, work.x + (work.w - width) / 2, work.y + (work.h - height) / 2);
    m_normalSize = m_previousNormalSize = {width, height};

    m_arrowCursor.reset(SDL_CreateSystemCursor(SDL_SYSTEM_CURSOR_ARROW));
    m_handCursor.reset(SDL_CreateSystemCursor(SDL_SYSTEM_CURSOR_HAND));
}

void App::UpdateSize()
{
    SDL_GetWindowSize(m_window.get(), &m_windowWidth, &m_windowHeight);
    SDL_GL_GetDrawableSize(m_window.get(), &m_width, &m_height);
    const float pixelsPerUnit = m_windowWidth > 0 && m_width > 0 ? static_cast<float>(m_width) / static_cast<float>(m_windowWidth) : 1.0f;
    m_scale = std::clamp(m_textScale * pixelsPerUnit, 0.5f, 4.0f);
}

void App::BuildFonts()
{
    const float scale = Scale();
    m_fonts[0] = std::make_unique<text::Font>(15 * scale, text::Weight::Regular);
    m_fonts[1] = std::make_unique<text::Font>(19 * scale, text::Weight::SemiBold);
    m_fonts[2] = std::make_unique<text::Font>(31 * scale, text::Weight::SemiBold);
    m_painter->SetFonts(m_fonts[0].get(), m_fonts[1].get(), m_fonts[2].get());
    m_fontScale = scale;
}

void App::ShowLoading()
{
    gl::glBindFramebuffer(gl::GL_FRAMEBUFFER, 0);
    gl::glViewport(0, 0, m_width, m_height);
    gl::glClearColor(0.016f, 0.017f, 0.022f, 1);
    gl::glClear(gl::GL_COLOR_BUFFER_BIT);
    m_painter->Begin(m_width, m_height);
    const Rect screen{0, 0, static_cast<float>(m_width), static_cast<float>(m_height)};
    m_painter->TextCentered(FontId::Display, screen, T(Str::Title), TextColor);
    m_painter->Flush();
    m_context->SwapBuffers();
}

int App::Run()
{
    SDL_Event event;
    while (m_running) {
        // Nothing moving and nothing new, or nothing to be seen: wait for input instead of drawing the same picture.
        const bool hidden = (SDL_GetWindowFlags(m_window.get()) & (SDL_WINDOW_MINIMIZED | SDL_WINDOW_HIDDEN)) != 0;
        if (hidden || (!m_dirty && !NeedsAnimation())) {
            if (SDL_WaitEvent(&event))
                HandleEvent(event);
            m_lastFrame = std::chrono::steady_clock::now(); // the idle time is not animation time
        }
        while (SDL_PollEvent(&event))
            HandleEvent(event);
        if (!m_running || hidden || (!m_dirty && !NeedsAnimation()))
            continue;

        m_dirty = false;
        const auto start = std::chrono::steady_clock::now();
        Frame();
        // Swapping the buffers waits for the screen; where it does not, the loop sleeps a little.
        if (std::chrono::steady_clock::now() - start < std::chrono::milliseconds(2))
            SDL_Delay(1);
    }
    Save();
    return 0;
}

void App::HandleEvent(const SDL_Event& event)
{
    m_dirty = true;
    switch (event.type) {
    case SDL_QUIT: m_running = false; break;
    case SDL_WINDOWEVENT: HandleWindowEvent(event.window); break;
    case SDL_MOUSEMOTION: PointerAt(event.motion.x, event.motion.y); break;
    case SDL_MOUSEBUTTONDOWN:
    case SDL_MOUSEBUTTONUP: {
        PointerAt(event.button.x, event.button.y);
        const bool down = event.type == SDL_MOUSEBUTTONDOWN;
        if (event.button.button == SDL_BUTTON_LEFT) {
            m_pointer.leftDown = down;
            (down ? m_pointer.leftPressed : m_pointer.leftReleased) = true;
        }
        else if (event.button.button == SDL_BUTTON_RIGHT) {
            m_pointer.rightDown = down;
            (down ? m_pointer.rightPressed : m_pointer.rightReleased) = true;
        }
        break;
    }
    case SDL_MOUSEWHEEL:
        // Away from the user is positive, in notches; touchpads scroll in fractions of one.
        m_pointer.wheel += event.wheel.preciseY;
        break;
    case SDL_KEYDOWN: HandleKey(event.key); break;
    default: break;
    }
}

void App::HandleWindowEvent(const SDL_WindowEvent& event)
{
    switch (event.event) {
    case SDL_WINDOWEVENT_SIZE_CHANGED:
        UpdateSize();
        if (!m_fullScreen && (SDL_GetWindowFlags(m_window.get()) & (SDL_WINDOW_MAXIMIZED | SDL_WINDOW_FULLSCREEN)) == 0) {
            m_previousNormalSize = m_normalSize;
            m_normalSize = {m_windowWidth, m_windowHeight};
            m_normalSizeTime = event.timestamp;
        }
        break;
    case SDL_WINDOWEVENT_MAXIMIZED:
        // The new size may come a moment before the word that the window is maximized: it was not a size of its own.
        if (event.timestamp - m_normalSizeTime < 250)
            m_normalSize = m_previousNormalSize;
        break;
    case SDL_WINDOWEVENT_DISPLAY_CHANGED:
        m_textScale = desktop::TextScale(m_window.get());
        UpdateSize();
        break;
    case SDL_WINDOWEVENT_LEAVE:
        // So that nothing stays highlighted under a pointer that has gone.
        if (!m_pointer.leftDown && !m_pointer.rightDown)
            m_pointer.x = m_pointer.y = -1;
        break;
    case SDL_WINDOWEVENT_FOCUS_LOST:
        // A button let go of in another window is not heard here.
        if (m_pointer.leftDown)
            m_pointer.leftReleased = true;
        if (m_pointer.rightDown)
            m_pointer.rightReleased = true;
        m_pointer.leftDown = m_pointer.rightDown = false;
        break;
    default: break;
    }
}

void App::PointerAt(int windowX, int windowY)
{
    const float pixelsPerUnit = m_windowWidth > 0 ? static_cast<float>(m_width) / static_cast<float>(m_windowWidth) : 1.0f;
    m_pointer.x = static_cast<float>(windowX) * pixelsPerUnit;
    m_pointer.y = static_cast<float>(windowY) * pixelsPerUnit;
}

// The frame ----------------------------------------------------------------------------------------

void App::Frame()
{
    const auto now = std::chrono::steady_clock::now();
    const float seconds = std::min(std::chrono::duration<float>(now - m_lastFrame).count(), 0.1f);
    m_lastFrame = now;
    m_time += seconds;
    if (m_width <= 0 || m_height <= 0)
        return;
    if (m_scale != m_fontScale)
        BuildFonts();
    m_renderer->Resize(m_width, m_height);

    Update(seconds);
    m_renderer->Render(BuildScene());

    m_cursorHand = false;
    m_painter->Begin(m_width, m_height);
    DrawInterface();
    m_painter->Flush();
    HandleBoardPointer();

    // The widgets are drawn as they were before they heard the click: what it changed shows in the next frame.
    if (m_pointer.leftPressed || m_pointer.leftReleased || m_pointer.rightPressed || m_pointer.rightReleased)
        m_dirty = true;
    m_dialogOpening = false;
    m_pointer.leftPressed = m_pointer.leftReleased = m_pointer.rightPressed = m_pointer.rightReleased = false;
    m_pointer.wheel = 0;
    m_pointer.consumed = false;
    if (m_cursorHand != m_cursorShownHand) {
        SDL_SetCursor(m_cursorHand ? m_handCursor.get() : m_arrowCursor.get());
        m_cursorShownHand = m_cursorHand;
    }
    m_context->SwapBuffers();
}

render::Scene App::BuildScene() const
{
    const Theme& theme = ThemeAt(m_theme);
    const go::Board& board = m_game.Current();
    const go::Phase phase = Phase();
    const int size = board.Size();

    render::Scene scene;
    scene.view = m_camera.View();
    scene.projection = m_camera.Projection(static_cast<float>(m_width) / static_cast<float>(m_height));
    scene.eye = m_camera.Eye();
    scene.time = m_time;
    scene.coordinates = m_coordinates;

    // The stone played last carries a ring; after a pass there is none.
    Point lastMove = go::Pass;
    if (!m_game.Moves().empty() && m_game.Moves().back().point > go::Pass)
        lastMove = m_game.Moves().back().point;

    // The territory, as counted at the end or estimated on request; the stones that are dead in it are seen through.
    go::PointSet seeThrough{};
    bool marked = false;
    const bool counted = phase == go::Phase::Scoring || (phase == go::Phase::Finished && !m_game.Moves().back().IsResignation());
    if (counted) {
        const auto owner = m_game.Territory();
        const go::PointSet& dead = m_game.Dead();
        for (int y = 0; y < size; ++y) {
            for (int x = 0; x < size; ++x) {
                const Point point = go::PointAt(x, y);
                const auto at = static_cast<std::size_t>(point);
                if (board.IsStone(point) && !dead[at])
                    continue;
                if (owner[at] == go::Cell::Black)
                    scene.marks[MarkIndex(point)] |= render::BlackArea;
                else if (owner[at] == go::Cell::White)
                    scene.marks[MarkIndex(point)] |= render::WhiteArea;
            }
        }
        seeThrough = dead;
        marked = true;
    }
    else if (m_estimate) {
        scene.estimate = true;
        for (int y = 0; y < size; ++y) {
            for (int x = 0; x < size; ++x) {
                const Point point = go::PointAt(x, y);
                const auto at = static_cast<std::size_t>(point);
                const float share = m_estimate->black[at];
                const go::Cell cell = board.At(point);
                if (share > 0.7f && cell != go::Cell::Black) {
                    scene.marks[MarkIndex(point)] |= render::BlackArea;
                    seeThrough[at] = cell == go::Cell::White;
                }
                else if (share < 0.3f && cell != go::Cell::White) {
                    scene.marks[MarkIndex(point)] |= render::WhiteArea;
                    seeThrough[at] = cell == go::Cell::Black;
                }
            }
        }
        marked = true;
    }
    m_view->Draws(scene.stones, lastMove, marked ? &seeThrough : nullptr, theme.accentWarm, m_time);
    m_view->MarkStones(scene.marks);

    // The stone that a click would place, seen through; while counting, the group under the pointer.
    const bool ready = phase == go::Phase::Playing && HumanToMove() && m_view->BoardReady();
    if (ready && m_hoverPoint != go::Pass) {
        render::StoneDraw ghost;
        ghost.model = Mat4::Translation(m_layout.PointCenter(m_hoverPoint) + Vec3{0, stones::HalfHeight + 0.015f, 0}) * Mat4::RotationY(0.7f);
        ghost.white = board.ToPlay() == Color::White;
        ghost.seed = 0.37f;
        ghost.opacity = 0.5f;
        scene.stones.push_back(ghost);
    }
    if (phase == go::Phase::Scoring && m_hoverPoint != go::Pass)
        scene.marks[MarkIndex(m_hoverPoint)] |= render::Hover;
    if (m_hint && m_hintGlow > 0.01f)
        scene.marks[MarkIndex(*m_hint)] |= render::Hint;
    if (ready && board.Ko() != go::Pass)
        scene.marks[MarkIndex(board.Ko())] |= render::Ko;
    scene.panels = FrostedPanels();
    return scene;
}

void App::Update(float seconds)
{
    const float target = m_panelVisible ? 1.0f : 0.0f;
    m_panelSlide += (target - m_panelSlide) * (1 - std::exp(-seconds * 12));
    if (std::abs(target - m_panelSlide) < 0.001f)
        m_panelSlide = target;
    const float panelSpace = (PanelWidth + PanelMargin) * Scale() * m_panelSlide;
    m_camera.SetSpace(1 - panelSpace / static_cast<float>(std::max(m_width, 1)));
    m_camera.Update(seconds);

    // The stones that the move under the pointer would take glow and rise, about to go.
    std::vector<Point> taking;
    if (m_hoverPoint != go::Pass && Phase() == go::Phase::Playing && HumanToMove() && m_view->BoardReady()) {
        go::Board next = m_game.Current();
        next.Play(m_hoverPoint, next.ToPlay(), &taking);
    }
    m_view->Preview(taking);

    // One sound of each kind a frame: the stones taken land together.
    m_events.clear();
    m_view->Update(seconds, m_events);
    std::ranges::sort(m_events);
    const auto [first, last] = std::ranges::unique(m_events);
    m_events.erase(first, last);
    for (const BoardView::Event event : m_events) {
        switch (event) {
        case BoardView::Event::Place: m_sound.Play(SoundBank::Effect::Place); break;
        case BoardView::Event::Capture: m_sound.Play(SoundBank::Effect::Capture); break;
        case BoardView::Event::Return: m_sound.Play(SoundBank::Effect::Return); break;
        }
    }

    const bool over = Phase() == go::Phase::Finished && m_view->Settled();
    if (over) {
        m_bannerTitle = ResultTitle();
        m_bannerDetail = ResultDetail();
    }
    m_bannerFade = std::clamp(m_bannerFade + (over ? seconds : -seconds) * 3, 0.0f, 1.0f);
    m_hintGlow = std::clamp(m_hintGlow + (m_hint ? seconds : -seconds) * 4, 0.0f, 1.0f);
    m_resignConfirm = std::max(0.0f, m_resignConfirm - seconds);
    for (Toast& toast : m_toasts)
        toast.age += seconds;
    std::erase_if(m_toasts, [](const Toast& toast) { return toast.age > 3.2f; });

    UpdateHelpers();
    UpdateComputer(seconds);
    UpdateScoring(seconds);

    if (over && !m_announcedEnd) {
        m_announcedEnd = true;
        m_sound.Play(SoundBank::Effect::GameOver);
    }
    if (Phase() != go::Phase::Finished)
        m_announcedEnd = false;
}

bool App::NeedsAnimation() const
{
    const go::Phase phase = Phase();
    return m_camera.Moving() || m_view->Animating() || m_computerThinking || m_computerResult || m_hintThinking || m_estimating || m_findingDead ||
           !m_toasts.empty() || (m_panelSlide > 0 && m_panelSlide < 1) || (m_bannerFade > 0 && m_bannerFade < 1) || m_hintGlow > 0 ||
           m_resignConfirm > 0 || m_gesture != Gesture::None || m_pointer.leftDown || m_pointer.rightDown ||
           (phase == go::Phase::Playing && !IsHuman(m_game.Current().ToPlay())) || (phase == go::Phase::Scoring && m_mode == Demo);
}

// The game -----------------------------------------------------------------------------------------

bool App::IsHuman(Color side) const
{
    switch (m_mode) {
    case PlayBlack: return side == Color::Black;
    case PlayWhite: return side == Color::White;
    case TwoPlayers: return true;
    default: return false;
    }
}

bool App::HumanToMove() const
{
    return Phase() == go::Phase::Playing && IsHuman(m_game.Current().ToPlay());
}

void App::StopWorkers()
{
    m_computer.Cancel();
    m_computerThinking = false;
    m_computerResult.reset();
    m_hinter.Cancel();
    m_hintThinking = false;
    m_estimator.Cancel();
    m_estimating = false;
    m_deadFinder.Cancel();
    m_findingDead = false;
}

void App::UpdateComputer(float seconds)
{
    if (Phase() != go::Phase::Playing || IsHuman(m_game.Current().ToPlay()))
        return;
    m_computerClock += seconds;
    if (!m_computerThinking && !m_computerResult) {
        const go::Board board = m_game.Current();
        std::vector<std::uint64_t> history(m_game.History().begin(), m_game.History().end());
        const double komi = m_game.Komi();
        const go::SearchLimits limits = LevelLimits(m_level, NextSeed());
        m_computer.Start([board, history = std::move(history), komi, limits](std::stop_token stop) {
            return go::Engine::Search(board, history, komi, limits, stop);
        });
        m_computerThinking = true;
        m_computerClock = 0;
    }
    if (m_computerThinking) {
        const bool busy = m_computer.Busy();
        if (auto result = m_computer.Take()) {
            m_computerResult = result;
            m_computerThinking = false;
        }
        else if (!busy) {
            m_computerThinking = false; // cancelled; a new search starts next frame
        }
    }
    // The computer waits for the stones to settle, and a moment more, so that its move can be followed.
    const float pause = m_mode == Demo ? 0.6f : 0.25f;
    if (m_computerResult && m_view->Settled() && m_computerClock >= pause && m_gesture == Gesture::None) {
        const go::SearchResult result = *m_computerResult;
        m_computerResult.reset();
        if (result.resign)
            Resign(m_game.Current().ToPlay());
        else
            PlayMove(result.move);
    }
}

// After two passes the dead stones are found, to be corrected by a click; a demonstration accepts the count by itself.
void App::UpdateScoring(float seconds)
{
    if (Phase() != go::Phase::Scoring)
        return;
    if (m_deadFoundPly != m_game.Ply() && !m_findingDead) {
        const go::Board board = m_game.Current();
        const double komi = m_game.Komi();
        const int playouts = EstimatePlayouts(board.Size());
        const std::uint64_t seed = NextSeed();
        m_deadFinder.Start([board, komi, playouts, seed](std::stop_token stop) { return go::Engine::Estimate(board, komi, playouts, seed, stop); });
        m_findingDead = true;
    }
    if (m_findingDead) {
        const bool busy = m_deadFinder.Busy();
        if (auto result = m_deadFinder.Take()) {
            m_findingDead = false;
            m_game.SetDead(result->Dead(m_game.Current()));
            m_deadFoundPly = m_game.Ply();
            m_scoringClock = 0;
        }
        else if (!busy) {
            m_findingDead = false;
        }
    }
    if (m_mode == Demo && m_deadFoundPly == m_game.Ply() && !m_findingDead) {
        m_scoringClock += seconds;
        if (m_scoringClock > 1.6f)
            Accept();
    }
}

void App::UpdateHelpers()
{
    if (m_hintThinking) {
        const bool busy = m_hinter.Busy();
        if (auto result = m_hinter.Take()) {
            m_hintThinking = false;
            if (HumanToMove()) {
                if (result->move == go::Pass) {
                    ShowToast(std::string(T(Str::HintPass)));
                }
                else {
                    m_hint = result->move;
                    ShowToast(std::string(T(Str::HintToast)) + go::PointName(result->move, m_game.Size()));
                }
            }
        }
        else if (!busy) {
            m_hintThinking = false;
        }
    }
    if (m_estimating) {
        const bool busy = m_estimator.Busy();
        if (auto result = m_estimator.Take()) {
            m_estimating = false;
            m_estimate = *result;
            const std::string_view leader = T(result->margin >= 0 ? Str::Black : Str::White);
            const double margin = std::abs(result->margin);
            ShowToast(std::vformat(T(Str::EstimateToast), std::make_format_args(leader, margin)));
        }
        else if (!busy) {
            m_estimating = false;
        }
    }
}

void App::NewGame()
{
    StartGame(go::Game(m_boardSize, m_handicap));
}

void App::StartGame(go::Game game)
{
    StopWorkers();
    const bool resize = game.Size() != m_layout.size;
    m_game = std::move(game);
    if (resize) {
        m_layout = board::Layout::For(m_game.Size());
        m_renderer->SetLayout(m_layout);
        m_view->SetLayout(m_layout);
        m_camera.SetReach(m_layout.Extent());
    }
    m_view->Rebuild(m_game, true);
    m_camera.Reset(ViewSide());
    m_announcedEnd = Phase() == go::Phase::Finished;
    m_deadFoundPly = -1;
    AfterPositionChange();
}

void App::PlayMove(Point point)
{
    if (!m_game.Play(point))
        return;
    m_view->PlayMove(m_game.Moves().back(), true);
    if (point == go::Pass)
        AnnouncePass();
    AfterPositionChange();
}

void App::AnnouncePass()
{
    if (m_game.Moves().empty() || !m_game.Moves().back().IsPass())
        return;
    if (Phase() == go::Phase::Scoring)
        ShowToast(std::string(T(Str::BothPassed)));
    else
        ShowToast(std::string(T(m_game.Moves().back().color == Color::Black ? Str::BlackPasses : Str::WhitePasses)));
    m_sound.Play(SoundBank::Effect::Pass);
}

void App::Resign(Color who)
{
    if (Phase() == go::Phase::Finished)
        return;
    StopWorkers();
    m_game.Resign(who);
    AfterPositionChange();
}

void App::Accept()
{
    if (Phase() != go::Phase::Scoring || m_findingDead)
        return;
    m_deadFinder.Cancel();
    m_findingDead = false;
    m_game.Accept();
    m_hoverPoint = go::Pass;
    Save();
}

void App::Undo()
{
    StopWorkers();
    if (!m_game.CanUndo()) {
        ShowToast(std::string(T(Str::NothingToUndo)));
        return;
    }
    auto undoOne = [&] {
        const go::MoveRecord record = m_game.Moves().back();
        m_game.Undo();
        m_view->PlayMove(record, false);
    };
    undoOne();
    // Against the computer, back to the player's own turn.
    while ((m_mode == PlayBlack || m_mode == PlayWhite) && m_game.CanUndo() && !HumanToMove())
        undoOne();
    AfterPositionChange();
}

void App::Redo()
{
    StopWorkers();
    if (!m_game.CanRedo())
        return;
    auto redoOne = [&] {
        m_game.Redo();
        m_view->PlayMove(m_game.Moves().back(), true);
    };
    redoOne();
    while ((m_mode == PlayBlack || m_mode == PlayWhite) && m_game.CanRedo() && Phase() == go::Phase::Playing && !HumanToMove())
        redoOne();
    AfterPositionChange();
}

void App::SetMode(int mode)
{
    StopWorkers();
    m_mode = (mode % 4 + 4) % 4;
    if (m_mode == PlayBlack || m_mode == PlayWhite)
        m_camera.FaceSide(ViewSide());
    m_hint.reset();
    Save();
}

void App::SetLevel(int level)
{
    level = std::clamp(level, 1, 5);
    if (level == m_level)
        return;
    m_level = level;
    if (m_computerThinking) {
        m_computer.Cancel(); // starts again at the new level
        m_computerThinking = false;
    }
    Save();
}

// The size and the handicap are for the next game; a game not yet begun or already over is set out again at once.
void App::SetBoardSize(int size)
{
    if (size == m_boardSize)
        return;
    m_boardSize = size;
    m_handicap = std::min(m_handicap, go::MaxHandicap(size));
    if (m_game.Ply() == 0 || Phase() == go::Phase::Finished)
        NewGame();
    else
        ShowToast(std::string(T(Str::NextGame)));
    Save();
}

void App::SetHandicap(int handicap)
{
    handicap = std::clamp(handicap, 0, go::MaxHandicap(m_boardSize));
    if (handicap == 1)
        handicap = 0;
    if (handicap == m_handicap)
        return;
    m_handicap = handicap;
    if (m_game.Ply() == 0 || Phase() == go::Phase::Finished)
        NewGame();
    else
        ShowToast(std::string(T(Str::NextGame)));
    Save();
}

void App::SetTheme(int theme)
{
    m_theme = (theme % ThemeCount + ThemeCount) % ThemeCount;
    m_renderer->SetTheme(ThemeAt(m_theme));
    Save();
}

void App::ToggleCoordinates()
{
    m_coordinates = !m_coordinates;
    Save();
}

void App::ToggleSound()
{
    m_sound.SetEnabled(!m_sound.Enabled());
    Save();
}

void App::ToggleLanguage()
{
    m_language = m_language == Language::English ? Language::Russian : Language::English;
    Save();
}

void App::RequestHint()
{
    if (!HumanToMove() || m_hintThinking)
        return;
    m_hint.reset();
    const go::Board board = m_game.Current();
    std::vector<std::uint64_t> history(m_game.History().begin(), m_game.History().end());
    const double komi = m_game.Komi();
    const go::SearchLimits limits{60000, 1500, Threads(), false, NextSeed()};
    m_hinter.Start([board, history = std::move(history), komi, limits](std::stop_token stop) {
        return go::Engine::Search(board, history, komi, limits, stop);
    });
    m_hintThinking = true;
}

// The territory as it stands, shown until the next move; a second request hides it.
void App::RequestEstimate()
{
    if (Phase() != go::Phase::Playing)
        return;
    if (m_estimate) {
        m_estimate.reset();
        return;
    }
    if (m_estimating)
        return;
    const go::Board board = m_game.Current();
    const double komi = m_game.Komi();
    const int playouts = EstimatePlayouts(board.Size());
    const std::uint64_t seed = NextSeed();
    m_estimator.Start([board, komi, playouts, seed](std::stop_token stop) { return go::Engine::Estimate(board, komi, playouts, seed, stop); });
    m_estimating = true;
}

void App::AfterPositionChange()
{
    m_hinter.Cancel();
    m_hintThinking = false;
    m_hint.reset();
    m_estimator.Cancel();
    m_estimating = false;
    m_estimate.reset();
    m_deadFinder.Cancel();
    m_findingDead = false;
    m_resignConfirm = 0;
    m_hoverPoint = go::Pass;
    m_followMoves = true;
    Save();
}

void App::CopyGame()
{
    const std::string sgf = m_game.ToSgf(PlayerName(Color::Black), PlayerName(Color::White));
    if (SDL_SetClipboardText(sgf.c_str()) != 0)
        return;
    ShowToast(std::string(T(Str::GameCopied)));
}

void App::PasteGame()
{
    std::string text;
    if (char* clipboard = SDL_GetClipboardText()) {
        text = clipboard;
        SDL_free(clipboard);
    }
    auto game = go::Game::FromSgf(text);
    if (!game) {
        ShowToast(std::string(T(Str::NothingToPaste)));
        return;
    }
    m_boardSize = game->Size(); // new games go on with the size of the one loaded
    m_handicap = std::min(m_handicap, go::MaxHandicap(m_boardSize));
    StartGame(std::move(*game));
    ShowToast(std::string(T(Str::GameLoaded)));
}

void App::OpenLink(const char* url)
{
    if (SDL_OpenURL(url) == 0)
        return;
    // Where no browser could be started, the link goes to the clipboard instead.
    SDL_SetClipboardText(url);
    ShowToast(std::string(T(Str::LinkCopied)));
}

void App::Save()
{
    m_settings.theme = m_theme;
    m_settings.level = m_level;
    m_settings.mode = m_mode;
    m_settings.boardSize = m_boardSize;
    m_settings.handicap = m_handicap;
    m_settings.coordinates = m_coordinates;
    m_settings.sound = m_sound.Enabled();
    m_settings.panel = m_panelVisible;
    m_settings.language = m_language;
    m_settings.game = OneLine(m_game.ToSgf("", "")); // without names, which the settings file might not hold
    if (m_window) {
        const std::uint32_t flags = SDL_GetWindowFlags(m_window.get());
        if (!m_fullScreen && (flags & (SDL_WINDOW_MAXIMIZED | SDL_WINDOW_MINIMIZED | SDL_WINDOW_FULLSCREEN)) == 0)
            SDL_GetWindowSize(m_window.get(), &m_normalSize.width, &m_normalSize.height);
        m_settings.windowWidth = m_normalSize.width;
        m_settings.windowHeight = m_normalSize.height;
        m_settings.maximized = m_fullScreen ? m_maximizedBeforeFullScreen : (flags & SDL_WINDOW_MAXIMIZED) != 0;
    }
    m_settings.Save();
}

void App::ToggleFullScreen()
{
    const bool fullScreen = !m_fullScreen;
    if (fullScreen)
        m_maximizedBeforeFullScreen = (SDL_GetWindowFlags(m_window.get()) & SDL_WINDOW_MAXIMIZED) != 0;
    // Borderless over the whole screen, at its own resolution.
    if (SDL_SetWindowFullscreen(m_window.get(), fullScreen ? SDL_WINDOW_FULLSCREEN_DESKTOP : 0) == 0)
        m_fullScreen = fullScreen;
}

// Input --------------------------------------------------------------------------------------------

Vec3 App::PointerOnPlane(float height) const
{
    Vec3 origin, direction;
    m_camera.Ray(m_pointer.x, m_pointer.y, static_cast<float>(m_width), static_cast<float>(m_height), origin, direction);
    if (std::abs(direction.y) < 1e-5f)
        return origin;
    const float t = (height - origin.y) / direction.y;
    return origin + direction * std::max(t, 0.0f);
}

// The point nearest the pointer, aimed at a little above the board, between its surface and the tops of the stones.
Point App::PointUnderPointer() const
{
    return m_layout.PointNear(PointerOnPlane(0.1f));
}

Str App::IllegalReason(Point point) const
{
    const go::Board& board = m_game.Current();
    if (board.IsStone(point))
        return Str::Occupied;
    if (point == board.Ko())
        return Str::KoForbidden;
    if (!board.IsLegal(point, board.ToPlay()))
        return Str::Suicide;
    return Str::Superko;
}

void App::HandleBoardPointer()
{
    const bool blocked = m_pointer.consumed || m_dialog != Dialog::None;
    const float scale = Scale();
    const bool playing = HumanToMove() && m_view->BoardReady();
    const bool counting = Phase() == go::Phase::Scoring && m_mode != Demo && !m_findingDead;
    const bool insideWindow = m_pointer.x >= 0 && m_pointer.y >= 0 && m_pointer.x < static_cast<float>(m_width) && m_pointer.y < static_cast<float>(m_height);
    const Point point = insideWindow && !blocked ? PointUnderPointer() : go::Pass;

    // A legal point under the pointer, where a stone is shown seen through; while counting, a stone to mark.
    const Point hoverBefore = m_hoverPoint;
    m_hoverPoint = go::Pass;
    if (point != go::Pass && (m_gesture == Gesture::None || m_gesture == Gesture::PressPoint)) {
        if (playing && m_game.IsLegal(point))
            m_hoverPoint = point;
        else if (counting && m_game.Current().IsStone(point))
            m_hoverPoint = point;
    }
    if (m_hoverPoint != go::Pass)
        m_cursorHand = true;
    if (m_hoverPoint != hoverBefore)
        m_dirty = true; // drawn again with the new point and the stones it would take

    if (!blocked && m_pointer.leftPressed) {
        m_pressX = m_dragX = m_pointer.x;
        m_pressY = m_dragY = m_pointer.y;
        m_pressPoint = point;
        m_gesture = m_hoverPoint != go::Pass ? Gesture::PressPoint : Gesture::TurnCamera;
    }
    if (!m_pointer.consumed && m_pointer.rightPressed && m_gesture == Gesture::None) {
        m_gesture = Gesture::TurnCamera;
        m_pressPoint = go::Pass;
        m_pressX = m_dragX = m_pointer.x;
        m_pressY = m_dragY = m_pointer.y;
    }

    // A press on a point that moves on turns the view instead.
    const float travel = std::abs(m_pointer.x - m_pressX) + std::abs(m_pointer.y - m_pressY);
    if (m_gesture == Gesture::PressPoint && travel > 6 * scale && m_pointer.leftDown)
        m_gesture = Gesture::TurnCamera;
    if (m_gesture == Gesture::TurnCamera) {
        m_camera.Rotate(-(m_pointer.x - m_dragX) * 0.0055f / scale, (m_pointer.y - m_dragY) * 0.0045f / scale);
        m_dragX = m_pointer.x;
        m_dragY = m_pointer.y;
    }

    if (m_pointer.leftReleased || m_pointer.rightReleased) {
        if (m_gesture == Gesture::PressPoint && m_pointer.leftReleased && point == m_pressPoint) {
            if (playing && m_game.IsLegal(point)) {
                PlayMove(point);
            }
            else if (counting && m_game.Current().IsStone(point)) {
                m_game.ToggleDead(point);
                m_sound.Play(SoundBank::Effect::Click);
            }
        }
        else if (m_gesture == Gesture::TurnCamera && m_pointer.leftReleased && travel < 4 * scale && playing && point != go::Pass &&
                 !m_game.IsLegal(point)) {
            ShowToast(std::string(T(IllegalReason(point)))); // a click where a stone may not go
        }
        if ((m_pointer.leftReleased && m_gesture != Gesture::None && !(m_gesture == Gesture::TurnCamera && m_pointer.rightDown)) ||
            (m_pointer.rightReleased && m_gesture == Gesture::TurnCamera && !m_pointer.leftDown))
            m_gesture = Gesture::None;
    }

    if (!blocked && m_pointer.wheel != 0)
        m_camera.Zoom(m_pointer.wheel);
}

void App::HandleKey(const SDL_KeyboardEvent& event)
{
    const SDL_Keysym& key = event.keysym;
    const bool control = (key.mod & KMOD_CTRL) != 0;
    const bool shift = (key.mod & KMOD_SHIFT) != 0;
    const bool alt = (key.mod & KMOD_ALT) != 0;
    const bool repeat = event.repeat != 0;
    const char letter = ShortcutLetter(key);

    if (alt) {
        if ((key.sym == SDLK_RETURN || key.sym == SDLK_KP_ENTER) && !repeat)
            ToggleFullScreen();
        return; // the rest of Alt is the desktop's
    }
    if (m_dialog != Dialog::None && key.sym == SDLK_ESCAPE) {
        if (!repeat)
            m_dialog = Dialog::None;
        return;
    }

    // Taking back and replaying go on while the key is held; the other keys act once.
    if (control) {
        switch (letter) {
        case 'z': shift ? Redo() : Undo(); return;
        case 'y': Redo(); return;
        default: break;
        }
        if (repeat)
            return;
        switch (letter) {
        case 'c': CopyGame(); return;
        case 'v': PasteGame(); return;
        case 'n': NewGame(); return;
        default: return;
        }
    }
    switch (key.sym) {
    case SDLK_BACKSPACE:
    case SDLK_LEFT: Undo(); return;
    case SDLK_RIGHT: Redo(); return;
    default: break;
    }
    if (repeat)
        return;

    switch (key.sym) {
    case SDLK_RETURN:
    case SDLK_KP_ENTER: Accept(); return;
    case SDLK_HOME: m_camera.Reset(ViewSide()); return;
    case SDLK_TAB:
        m_panelVisible = !m_panelVisible;
        Save();
        return;
    case SDLK_F1: ToggleDialog(Dialog::Help); return;
    case SDLK_F11: ToggleFullScreen(); return;
    case SDLK_ESCAPE:
        if (m_fullScreen)
            ToggleFullScreen();
        return;
    default: break;
    }
    switch (letter) {
    case 'a': ToggleDialog(Dialog::About); return;
    case 'p':
        if (HumanToMove())
            PlayMove(go::Pass);
        return;
    case 'n': NewGame(); return;
    case 'h': RequestHint(); return;
    case 'e': RequestEstimate(); return;
    case 'f': m_camera.Flip(); return;
    case 'v': m_camera.ToggleTopView(); return;
    case 'm': SetMode(m_mode + (shift ? -1 : 1)); return;
    case 'b': {
        SetBoardSize(NextBoardSize(m_boardSize, shift ? -1 : 1, true));
        return;
    }
    case 'c': ToggleCoordinates(); return;
    case 't': SetTheme(m_theme + (shift ? -1 : 1)); return;
    case 's': ToggleSound(); return;
    case 'l': ToggleLanguage(); return;
    default: break;
    }
    if (const int level = LevelKey(key))
        SetLevel(level);
}

// The interface ------------------------------------------------------------------------------------

Rect App::PanelRect() const
{
    const float scale = Scale();
    const float width = PanelWidth * scale, margin = PanelMargin * scale;
    return {static_cast<float>(m_width) - (width + margin) * m_panelSlide, margin, width, static_cast<float>(m_height) - 2 * margin};
}

Rect App::FreeArea() const
{
    const Rect panel = PanelRect();
    return {0, 0, std::min(panel.x, static_cast<float>(m_width)), static_cast<float>(m_height)};
}

namespace {

constexpr std::size_t HelpLineCount = 25;
constexpr int HelpRows = static_cast<int>(HelpLineCount + 1) / 2; // in the first of the two columns
constexpr float HelpWidth = 880, HelpRow = 26, HelpPad = 28;
constexpr float AboutWidth = 520, AboutPad = 28, AboutIcon = 76, LinkRow = 36, LinkGap = 6;

// The about card is in English, whatever the language of the interface.
constexpr std::string_view AboutDescription =
    "A 3D game of Go written in modern C++ on OpenGL 4.6, with a computer opponent of five levels, from Novice to Master. "
    "Everything you see and hear is made in code: there is no model, texture or sound file.";

struct Link {
    std::string_view name;
    std::string_view address; // as shown
    const char* url;
};

constexpr std::array<Link, 5> AuthorLinks{{
    {"GitHub", "github.com/makarov-mm", "https://github.com/makarov-mm"},
    {"GitHub Sponsors", "github.com/sponsors/makarov-mm", "https://github.com/sponsors/makarov-mm"},
    {"LinkedIn", "linkedin.com/in/makarov-mm", "https://www.linkedin.com/in/makarov-mm/"},
    {"Threads", "threads.net/@m.m.makarov", "https://www.threads.net/@m.m.makarov"},
    {"Instagram", "instagram.com/m.m.makarov", "https://www.instagram.com/m.m.makarov/"},
}};

} // namespace

std::vector<std::string> App::WrapText(FontId font, std::string_view words, float width) const
{
    std::vector<std::string> lines;
    std::string line;
    while (!words.empty()) {
        const std::size_t space = std::min(words.find(' '), words.size());
        const std::string_view word = words.substr(0, space);
        words.remove_prefix(std::min(space + 1, words.size()));
        std::string longer = line.empty() ? std::string(word) : std::format("{} {}", line, word);
        if (!line.empty() && m_painter->Measure(font, longer) > width) {
            lines.push_back(std::move(line));
            line = word;
        }
        else {
            line = std::move(longer);
        }
    }
    if (!line.empty())
        lines.push_back(std::move(line));
    return lines;
}

std::vector<std::string> App::RulesLines() const { return WrapText(FontId::Body, T(Str::RulesNote), (HelpWidth - 2 * HelpPad) * Scale()); }

// The rows of the help close up, down to the height of a line, where the window is too low for them.
float App::HelpRowHeight() const
{
    const float scale = Scale();
    const float rows = static_cast<float>(HelpRows + 1) + static_cast<float>(RulesLines().size());
    const float room = static_cast<float>(m_height) - (20 + 2 * HelpPad + 44 + 12) * scale;
    return std::max(m_painter->LineHeight(FontId::Body), std::min(room / rows, HelpRow * scale));
}

Rect App::CenteredCard(float width, float height) const
{
    const Rect free = FreeArea();
    const float x = width <= free.width ? free.x + (free.width - width) / 2 : std::max(0.0f, (static_cast<float>(m_width) - width) / 2);
    return {x, std::max(10 * Scale(), (static_cast<float>(m_height) - height) / 2), width, height};
}

Rect App::HelpCard() const
{
    const float scale = Scale();
    const float rows = static_cast<float>(HelpRows + 1) + static_cast<float>(RulesLines().size());
    return CenteredCard(HelpWidth * scale, (2 * HelpPad + 44 + 12) * scale + rows * HelpRowHeight());
}

Rect App::AboutCard() const
{
    const float scale = Scale();
    const ui::Painter& p = *m_painter;
    const auto lines = static_cast<float>(WrapText(FontId::Body, AboutDescription, (AboutWidth - 2 * AboutPad) * scale).size());
    const auto links = static_cast<float>(AuthorLinks.size());
    const float height = (2 * AboutPad + AboutIcon + 20 + 16 + 2 + 20 + links * (LinkRow + LinkGap) - LinkGap) * scale +
                         lines * (p.LineHeight(FontId::Body) + 3 * scale) + p.LineHeight(FontId::Heading) + p.LineHeight(FontId::Body);
    return CenteredCard(AboutWidth * scale, height);
}

Rect App::BannerCard() const
{
    const float scale = Scale();
    const float width = std::max({m_painter->Measure(FontId::Display, m_bannerTitle) + 80 * scale, m_painter->Measure(FontId::Body, m_bannerDetail) + 60 * scale,
                                  320 * scale});
    const Rect free = FreeArea();
    return {free.x + (free.width - width) / 2, 26 * scale - (1 - m_bannerFade) * 20 * scale, width, 92 * scale};
}

std::vector<render::FrostedPanel> App::FrostedPanels() const
{
    std::vector<render::FrostedPanel> panels;
    const float scale = Scale();
    auto add = [&](Rect rect, float radius, float opacity) { panels.push_back({rect.x, rect.y, rect.width, rect.height, radius * scale, opacity}); };
    if (m_panelSlide > 0.001f)
        add(PanelRect(), 18, 1);
    if (m_bannerFade > 0.001f)
        add(BannerCard(), 18, m_bannerFade);
    if (m_dialog == Dialog::Help)
        add(HelpCard(), 20, 1);
    else if (m_dialog == Dialog::About)
        add(AboutCard(), 20, 1);
    return panels;
}

void App::ShowToast(std::string text)
{
    if (m_toasts.size() >= 3)
        m_toasts.erase(m_toasts.begin());
    m_toasts.push_back({std::move(text), 0});
}

std::string App::PlayerName(Color side) const
{
    if (m_mode == TwoPlayers)
        return std::string(T(Str::Player));
    if (IsHuman(side))
        return std::string(T(Str::You));
    return std::format("{} · {}", T(Str::Computer), T(static_cast<Str>(std::to_underlying(Str::Level1) + m_level - 1)));
}

std::string App::HandicapText(int stones) const
{
    if (stones < 2)
        return std::string(T(Str::NoHandicap));
    return std::vformat(T(stones <= 4 ? Str::StonesFew : Str::StonesMany), std::make_format_args(stones));
}

std::string App::ResultTitle() const
{
    const auto winner = m_game.Winner();
    if (!winner)
        return std::string(T(Str::Draw));
    return std::string(T(*winner == Color::Black ? Str::BlackWins : Str::WhiteWins));
}

std::string App::ResultDetail() const
{
    if (m_game.Moves().empty())
        return {};
    const go::MoveRecord& last = m_game.Moves().back();
    if (last.IsResignation())
        return std::string(T(last.color == Color::Black ? Str::BlackResigned : Str::WhiteResigned));
    const go::Score score = m_game.Count();
    const double komi = score.komi;
    return std::vformat(T(Str::ScoreDetail), std::make_format_args(score.black, score.white, komi)) + "  ·  " + m_game.ResultText();
}

void App::ToggleDialog(Dialog dialog)
{
    m_dialog = m_dialog == dialog ? Dialog::None : dialog;
    m_dialogOpening = m_dialog != Dialog::None;
}

bool App::PressedOutside(Rect card) const
{
    // A quick click, such as a tap on a touchpad, presses and lets go within one frame: when it opens a card, its
    // press must not then close it.
    return m_pointer.leftPressed && !m_dialogOpening && !card.Contains(m_pointer.x, m_pointer.y);
}

App::Interaction App::Interact(int id, Rect rect, bool enabled)
{
    // While a card is open, only its own widgets answer, from the frame after the one that opened it.
    const bool interactive = enabled && (m_dialog == Dialog::None || (id > ModalWidgets && !m_dialogOpening));
    Interaction result;
    result.hover = interactive && rect.Contains(m_pointer.x, m_pointer.y);
    if (result.hover)
        m_cursorHand = true;
    if (result.hover && m_pointer.leftPressed) {
        m_activeWidget = id;
        m_pointer.consumed = true;
    }
    if (m_pointer.leftReleased && m_activeWidget == id) {
        result.clicked = result.hover;
        m_activeWidget = 0;
        m_pointer.consumed = true;
    }
    result.pressed = result.hover && m_activeWidget == id && m_pointer.leftDown;
    if (result.clicked)
        m_sound.Play(SoundBank::Effect::Click);
    return result;
}

bool App::Button(int id, Rect rect, std::string_view label, bool enabled, bool accent)
{
    const Interaction state = Interact(id, rect, enabled);
    ui::Color fill = !enabled        ? Rgba(255, 255, 255, 7)
                     : state.pressed ? Rgba(255, 255, 255, 52)
                     : state.hover   ? Rgba(255, 255, 255, 34)
                                     : Rgba(255, 255, 255, 17);
    if (accent && enabled)
        fill = ui::Mix(fill, ui::FromLinear(ThemeAt(m_theme).accent, static_cast<float>(fill.a) / 255.0f * 2.2f), 0.6f);
    m_painter->Fill(rect, 9 * Scale(), fill);
    m_painter->TextCentered(FontId::Body, rect, label, enabled ? TextColor : ui::Fade(TextColor, 0.35f));
    return state.clicked;
}

int App::Selector(int id, Rect rect, std::string_view label, std::string_view value, float labelWidth)
{
    const float scale = Scale();
    m_painter->Text(FontId::Body, rect.x, rect.y + (rect.height - m_painter->LineHeight(FontId::Body)) / 2, label, MutedColor);
    const float arrow = rect.height;
    const Rect right{rect.Right() - arrow, rect.y, arrow, rect.height};
    // The value from 42% of the row, or further where a label is longer: fonts differ in width from one desktop to another.
    const Rect left{std::max(rect.x + rect.width * 0.42f - arrow - 4 * scale, rect.x + labelWidth + 8 * scale), rect.y, arrow, rect.height};
    m_painter->Fill({left.Right() + 2 * scale, rect.y, right.x - left.Right() - 4 * scale, rect.height}, 9 * scale, Rgba(255, 255, 255, 9));
    m_painter->TextCentered(FontId::Body, {left.Right(), rect.y, right.x - left.Right(), rect.height}, value, TextColor);
    int change = 0;
    if (Button(id, left, ""))
        change = -1;
    if (Button(id + 1, right, ""))
        change = 1;
    const float size = 5 * scale;
    m_painter->Arrow(left.x + left.width / 2, left.y + left.height / 2, size * 0.8f, size, -1, TextColor);
    m_painter->Arrow(right.x + right.width / 2, right.y + right.height / 2, size * 0.8f, size, 1, TextColor);
    return change;
}

void App::DrawInterface()
{
    DrawPanel();
    DrawBanner();
    DrawToasts();
    if (m_dialog == Dialog::Help)
        DrawHelp();
    else if (m_dialog == Dialog::About)
        DrawAbout();
}

void App::DrawStatus(Rect area)
{
    const float scale = Scale();
    ui::Painter& p = *m_painter;
    const go::Phase phase = Phase();
    const Color side = m_game.Current().ToPlay();
    const ui::Color accent = ui::FromLinear(ThemeAt(m_theme).accent);
    p.Fill(area, 12 * scale, Rgba(255, 255, 255, 14));

    // A stone of the side to move, or of the winner.
    const float stoneX = area.x + 22 * scale, stoneY = area.y + area.height / 2;
    const auto winner = phase == go::Phase::Finished ? m_game.Winner() : std::nullopt;
    DrawStone(p, stoneX, stoneY, 10 * scale, winner.value_or(side));

    std::string title, detail;
    ui::Color titleColor = TextColor;
    if (phase == go::Phase::Finished) {
        title = ResultTitle();
        detail = m_game.Moves().back().IsResignation() ? ResultDetail() : m_game.ResultText();
        titleColor = accent;
    }
    else if (phase == go::Phase::Scoring) {
        title = T(Str::Counting);
        detail = T(m_findingDead ? Str::Estimating : Str::CountingHelp);
    }
    else {
        title = T(side == Color::Black ? Str::BlackToMove : Str::WhiteToMove);
        detail = m_computerThinking || m_computerResult ? std::string(T(Str::Thinking)) : PlayerName(side);
    }
    const bool busy = m_computerThinking || m_computerResult || m_hintThinking || m_estimating || m_findingDead;
    const float textX = area.x + 44 * scale, textWidth = area.Right() - (busy ? 40 : 10) * scale - textX;
    const float lineHeight = p.LineHeight(FontId::Heading);
    p.Text(FontId::Heading, textX, area.y + area.height / 2 - lineHeight + 3 * scale, title, titleColor);
    p.Text(FontId::Body, textX, area.y + area.height / 2 + 2 * scale, p.Elided(FontId::Body, detail, textWidth), MutedColor);
    if (busy)
        p.Spinner(area.Right() - 22 * scale, area.y + area.height / 2, 8 * scale, 2.2f * scale, m_time * 5.0f, accent);
}

// The players with their prisoners while the game goes on; their points when it is counted.
void App::DrawScore(Rect area)
{
    const float scale = Scale();
    ui::Painter& p = *m_painter;
    const go::Phase phase = Phase();
    const bool counted = phase == go::Phase::Scoring || (phase == go::Phase::Finished && !m_game.Moves().back().IsResignation());
    const go::Score score = counted ? m_game.Count() : go::Score{0, 0, m_game.Komi()};
    const float rowHeight = 28 * scale;
    // What the numbers are, over them.
    const std::string_view caption = T(counted ? Str::Points : Str::Captures);
    p.Text(FontId::Body, area.Right() - p.Measure(FontId::Body, caption), area.y, caption, ui::Fade(MutedColor, 0.8f));
    float y = area.y + p.LineHeight(FontId::Body) + 2 * scale;
    for (const Color side : {Color::Black, Color::White}) {
        const float textY = y + (rowHeight - p.LineHeight(FontId::Body)) / 2;
        const bool toMove = phase == go::Phase::Playing && m_game.Current().ToPlay() == side;
        DrawStone(p, area.x + 9 * scale, y + rowHeight / 2, 7 * scale, side);
        std::string value;
        if (counted)
            value = side == Color::Black ? std::to_string(score.black) : std::format("{} + {}", score.white, score.komi);
        else
            value = std::to_string(m_game.Current().Captures(side));
        const float valueWidth = p.Measure(FontId::Heading, value), nameX = area.x + 26 * scale;
        const Str who = m_mode == TwoPlayers ? Str::Player : IsHuman(side) ? Str::You : Str::Computer;
        const std::string name = std::format("{}  ·  {}", T(side == Color::Black ? Str::Black : Str::White), T(who));
        p.Text(FontId::Body, nameX, textY, p.Elided(FontId::Body, name, area.Right() - valueWidth - 8 * scale - nameX),
               toMove ? TextColor : ui::Fade(TextColor, 0.75f));
        p.Text(FontId::Heading, area.Right() - valueWidth, y + (rowHeight - p.LineHeight(FontId::Heading)) / 2, value, TextColor);
        y += rowHeight;
    }
    y += 8 * scale;
    if (counted) {
        // A bar shared out between the colors, komi with White.
        const Rect bar{area.x, y, area.width, 6 * scale};
        const double total = std::max(1.0, score.black + score.white + score.komi);
        const float share = static_cast<float>(score.black / total);
        p.Fill(bar, 3 * scale, Rgba(238, 236, 230, 220));
        p.Fill({bar.x, bar.y, std::max(bar.height, bar.width * share), bar.height}, 3 * scale, Rgba(10, 10, 12, 235));
        p.Fill({bar.x + bar.width / 2 - 0.5f * scale, bar.y - 2 * scale, 1 * scale, bar.height + 4 * scale}, 0, Rgba(255, 255, 255, 90));
    }
    else {
        std::string line = std::format("{} × {}  ·  {} {}", m_game.Size(), m_game.Size(), T(Str::Komi), m_game.Komi());
        if (m_game.Handicap() >= 2)
            line += std::format("  ·  {}: {}", T(Str::Handicap), m_game.Handicap());
        p.Text(FontId::Body, area.x, y - 4 * scale, line, MutedColor);
    }
}

void App::DrawMoveList(Rect area)
{
    const float scale = Scale();
    ui::Painter& p = *m_painter;
    p.Fill(area, 12 * scale, Rgba(0, 0, 0, 40));
    const auto moves = m_game.Moves();
    if (moves.empty()) {
        p.TextCentered(FontId::Body, area, T(Str::NoMovesYet), MutedColor);
        return;
    }

    const float rowHeight = std::round(25 * scale);
    const Rect inner = area.Inset(8 * scale);
    const int visible = std::max(1, static_cast<int>(inner.height / rowHeight));
    // Each row starts with Black's move; with a handicap White begins, in the second column.
    const int offset = m_game.Start().ToPlay() == Color::White ? 1 : 0;
    const int rows = (static_cast<int>(moves.size()) + offset + 1) / 2;
    const float maxScroll = static_cast<float>(std::max(0, rows - visible));

    if (area.Contains(m_pointer.x, m_pointer.y) && m_pointer.wheel != 0 && m_dialog == Dialog::None) {
        m_moveScroll = std::clamp(m_moveScroll - m_pointer.wheel * 3, 0.0f, maxScroll);
        m_followMoves = m_moveScroll >= maxScroll;
        m_pointer.wheel = 0;
    }
    if (m_followMoves)
        m_moveScroll = maxScroll;
    m_moveScroll = std::clamp(m_moveScroll, 0.0f, maxScroll);

    const int first = static_cast<int>(m_moveScroll);
    const float numberWidth = 40 * scale, columnWidth = (inner.width - numberWidth) / 2;
    const ui::Color accent = ui::FromLinear(ThemeAt(m_theme).accent);
    for (int row = first; row < std::min(rows, first + visible); ++row) {
        const float y = inner.y + static_cast<float>(row - first) * rowHeight;
        const std::string number = std::format("{}.", row + 1);
        p.Text(FontId::Body, inner.x + numberWidth - 10 * scale - p.Measure(FontId::Body, number), y + 3 * scale, number, MutedColor);
        for (int column = 0; column < 2; ++column) {
            const int ply = row * 2 + column - offset;
            if (ply < 0 || ply >= static_cast<int>(moves.size()))
                continue;
            const float x = inner.x + numberWidth + static_cast<float>(column) * columnWidth;
            if (ply == static_cast<int>(moves.size()) - 1)
                p.Fill({x - 6 * scale, y + 1 * scale, columnWidth - 6 * scale, rowHeight - 2 * scale}, 7 * scale, ui::Fade(accent, 0.28f));
            const go::MoveRecord& record = moves[static_cast<std::size_t>(ply)];
            if (record.IsPass()) {
                p.Text(FontId::Body, x, y + 3 * scale, T(Str::Pass), MutedColor);
            }
            else if (record.IsResignation()) {
                p.Text(FontId::Body, x, y + 3 * scale, T(Str::Resign), MutedColor);
            }
            else {
                // The point, and the stones it took.
                const float width = p.Text(FontId::Body, x, y + 3 * scale, go::PointName(record.point, m_game.Size()), TextColor);
                if (!record.captured.empty())
                    p.Text(FontId::Body, x + width + 8 * scale, y + 3 * scale, std::format("×{}", record.captured.size()), ui::Fade(MutedColor, 0.85f));
            }
        }
    }
    if (maxScroll > 0) {
        // A thin scroll bar.
        const float barHeight = inner.height * static_cast<float>(visible) / static_cast<float>(rows);
        const float barY = inner.y + (inner.height - barHeight) * (m_moveScroll / maxScroll);
        p.Fill({area.Right() - 5 * scale, barY, 3 * scale, barHeight}, 1.5f * scale, Rgba(255, 255, 255, 50));
    }
}

void App::DrawPanel()
{
    if (m_panelSlide <= 0.001f)
        return;
    const float scale = Scale();
    ui::Painter& p = *m_painter;
    const Rect panel = PanelRect();
    if (panel.Contains(m_pointer.x, m_pointer.y) && (m_pointer.leftPressed || m_pointer.rightPressed))
        m_pointer.consumed = true;
    p.Shadow(panel, 22 * scale, Rgba(0, 0, 0, 70));
    p.Fill(panel, 18 * scale, Rgba(16, 18, 26, 120));
    p.Outline(panel, 18 * scale, Rgba(255, 255, 255, 24));

    // In a low window everything is packed closer, and the footer goes.
    const bool compact = panel.height < 760 * scale;
    const float pad = (compact ? 16 : 20) * scale;
    const float x = panel.x + pad, width = panel.width - 2 * pad;
    float y = panel.y + pad;

    p.Text(FontId::Heading, x, y, T(Str::Title), TextColor);
    const float small = 30 * scale;
    if (Button(HelpButton, {panel.Right() - pad - small, y - 2 * scale, small, small}, "?"))
        ToggleDialog(Dialog::Help);
    if (Button(AboutButton, {panel.Right() - pad - 2 * small - 6 * scale, y - 2 * scale, small, small}, "i"))
        ToggleDialog(Dialog::About);
    y += (compact ? 38 : 44) * scale;

    const float statusHeight = (compact ? 54 : 62) * scale;
    DrawStatus({x, y, width, statusHeight});
    y += statusHeight + (compact ? 8 : 14) * scale;
    const float scoreHeight = 2 * p.LineHeight(FontId::Body) + 66 * scale;
    DrawScore({x, y, width, scoreHeight});
    y += scoreHeight + (compact ? 4 : 10) * scale;

    // From the bottom up: the footer, the switches, the settings and the buttons; the moves take the space between.
    float bottom = panel.Bottom() - pad;
    if (!compact) {
        const float footerHeight = p.LineHeight(FontId::Body);
        p.TextCentered(FontId::Body, {x, bottom - footerHeight, width, footerHeight}, T(Str::Footer), ui::Fade(MutedColor, 0.8f));
        bottom -= footerHeight + 14 * scale;
    }

    const float rowHeight = (compact ? 29 : 34) * scale, gap = (compact ? 5 : 8) * scale;
    const float half = (width - gap) / 2, third = (width - 2 * gap) / 3;
    bottom -= rowHeight;
    if (Button(SoundButton, {x, bottom, third, rowHeight}, T(Str::Sound), true, m_sound.Enabled()))
        ToggleSound();
    if (Button(CoordinatesButton, {x + third + gap, bottom, third, rowHeight}, T(Str::Coordinates), true, m_coordinates))
        ToggleCoordinates();
    if (Button(LanguageButton, {x + 2 * (third + gap), bottom, third, rowHeight}, T(Str::LanguageName)))
        ToggleLanguage();
    bottom -= gap;

    const std::array<std::string_view, 4> modes{T(Str::ModeBlack), T(Str::ModeWhite), T(Str::ModeTwoPlayers), T(Str::ModeDemo)};
    const Theme& theme = ThemeAt(m_theme);
    struct Choice {
        int id;
        Str label;
        std::string value;
    };
    const std::array<Choice, 5> choices{{
        {ThemeSelector, Str::Theme, std::string(m_language == Language::Russian ? theme.nameRussian : theme.nameEnglish)},
        {HandicapSelector, Str::Handicap, HandicapText(m_handicap)},
        {BoardSelector, Str::Board, std::format("{0} × {0}", m_boardSize)},
        {LevelSelector, Str::Level, std::format("{} · {}", m_level, T(static_cast<Str>(std::to_underlying(Str::Level1) + m_level - 1)))},
        {ModeSelector, Str::Mode, std::string(modes[static_cast<std::size_t>(m_mode)])},
    }};
    float labelWidth = 0;
    for (const Choice& choice : choices)
        labelWidth = std::max(labelWidth, p.Measure(FontId::Body, T(choice.label)));
    for (const Choice& choice : choices) {
        bottom -= rowHeight;
        if (const int change = Selector(choice.id, {x, bottom, width, rowHeight}, T(choice.label), choice.value, labelWidth); change != 0) {
            if (choice.id == ThemeSelector) {
                SetTheme(m_theme + change);
            }
            else if (choice.id == HandicapSelector) {
                const int next = m_handicap + change;
                SetHandicap(next == 1 ? (change > 0 ? 2 : 0) : next);
            }
            else if (choice.id == BoardSelector) {
                SetBoardSize(NextBoardSize(m_boardSize, change, false));
            }
            else if (choice.id == LevelSelector) {
                SetLevel(m_level + change);
            }
            else {
                SetMode(m_mode + change);
            }
        }
        bottom -= (compact ? 3 : 6) * scale;
    }
    bottom -= (compact ? 4 : 8) * scale;

    const float buttonHeight = (compact ? 31 : 38) * scale;
    bottom -= buttonHeight;
    if (Button(NewGameButton, {x, bottom, half, buttonHeight}, T(Str::NewGame)))
        NewGame();
    if (Button(FlipButton, {x + half + gap, bottom, half, buttonHeight}, T(Str::Flip)))
        m_camera.Flip();
    bottom -= gap + buttonHeight;
    if (Button(UndoButton, {x, bottom, third, buttonHeight}, T(Str::Undo), m_game.CanUndo()))
        Undo();
    if (Button(RedoButton, {x + third + gap, bottom, third, buttonHeight}, T(Str::Redo), m_game.CanRedo()))
        Redo();
    if (Button(HintButton, {x + 2 * (third + gap), bottom, third, buttonHeight}, T(Str::Hint), HumanToMove() && !m_hintThinking, true))
        RequestHint();
    bottom -= gap + buttonHeight;
    const go::Phase phase = Phase();
    if (phase == go::Phase::Scoring) {
        // The count: accepted as marked, or the game played on.
        if (Button(AcceptButton, {x, bottom, half, buttonHeight}, T(Str::Accept), m_mode != Demo && !m_findingDead, true))
            Accept();
        if (Button(ResumeButton, {x + half + gap, bottom, half, buttonHeight}, T(Str::Resume), m_mode != Demo))
            Undo();
    }
    else {
        const bool playing = HumanToMove();
        if (Button(PassButton, {x, bottom, third, buttonHeight}, T(Str::Pass), playing))
            PlayMove(go::Pass);
        // Resigning takes a second click.
        const bool canResign = phase == go::Phase::Playing && m_mode != Demo;
        if (Button(ResignButton, {x + third + gap, bottom, third, buttonHeight}, T(m_resignConfirm > 0 ? Str::ConfirmResign : Str::Resign), canResign,
                   m_resignConfirm > 0)) {
            if (m_resignConfirm > 0)
                Resign(m_mode == TwoPlayers ? m_game.Current().ToPlay() : ViewSide());
            else
                m_resignConfirm = 3;
        }
        if (Button(EstimateButton, {x + 2 * (third + gap), bottom, third, buttonHeight}, T(Str::Estimate), phase == go::Phase::Playing && !m_estimating,
                   m_estimate.has_value()))
            RequestEstimate();
    }
    bottom -= (compact ? 10 : 16) * scale;

    // The moves, where there is room for them.
    const float labelHeight = p.LineHeight(FontId::Body) + 6 * scale;
    if (bottom - y - labelHeight > 40 * scale) {
        p.Text(FontId::Body, x, y, T(Str::Moves), MutedColor);
        DrawMoveList({x, y + labelHeight, width, bottom - y - labelHeight});
    }
}

void App::DrawBanner()
{
    if (m_bannerFade <= 0.001f)
        return;
    const float scale = Scale();
    ui::Painter& p = *m_painter;
    const Rect card = BannerCard();
    p.Shadow(card, 24 * scale, ui::Fade(Rgba(0, 0, 0, 120), m_bannerFade));
    p.Fill(card, 18 * scale, ui::Fade(Rgba(14, 16, 24, 150), m_bannerFade));
    p.Outline(card, 18 * scale, ui::Fade(ui::FromLinear(ThemeAt(m_theme).accent), 0.6f * m_bannerFade));
    p.TextCentered(FontId::Display, {card.x, card.y + 8 * scale, card.width, 48 * scale}, m_bannerTitle, ui::Fade(TextColor, m_bannerFade));
    p.TextCentered(FontId::Body, {card.x, card.y + 54 * scale, card.width, 26 * scale}, m_bannerDetail, ui::Fade(MutedColor, m_bannerFade));
}

// The card's glass is frosted over the scene; over the panel, where there is nothing to frost, it is darker instead.
void App::DrawCard(Rect card)
{
    const float scale = Scale();
    ui::Painter& p = *m_painter;
    const bool overPanel = m_panelSlide > 0.001f && card.Right() > PanelRect().x;
    p.Shadow(card, 26 * scale, Rgba(0, 0, 0, 140));
    p.Fill(card, 20 * scale, Rgba(18, 20, 28, overPanel ? 235 : 175));
    p.Outline(card, 20 * scale, Rgba(255, 255, 255, 30));
}

void App::DrawHelp()
{
    const float scale = Scale();
    ui::Painter& p = *m_painter;
    p.Fill({0, 0, static_cast<float>(m_width), static_cast<float>(m_height)}, 0, Rgba(0, 0, 0, 110));

    struct Line {
        std::string_view keys;
        Str action;
    };
    const std::array<Line, HelpLineCount> lines{{
        {T(Str::KeyClick), Str::HelpClick},
        {T(Str::KeyDrag), Str::HelpRotate},
        {T(Str::KeyWheel), Str::HelpZoom},
        {"Ctrl+Z  ←  Backspace", Str::HelpUndo},
        {"Ctrl+Y  →", Str::HelpRedo},
        {"P", Str::HelpPass},
        {"H", Str::HelpHint},
        {"E", Str::HelpEstimate},
        {"Enter", Str::HelpAccept},
        {"N", Str::HelpNew},
        {"Ctrl+C", Str::HelpCopy},
        {"Ctrl+V", Str::HelpPaste},
        {"F", Str::HelpFlip},
        {"V", Str::HelpTopView},
        {"1 – 5", Str::HelpLevel},
        {"M", Str::HelpMode},
        {"B", Str::HelpBoard},
        {"C", Str::HelpCoordinates},
        {"T", Str::HelpTheme},
        {"S", Str::HelpSound},
        {"L", Str::HelpLanguage},
        {"Tab", Str::HelpPanel},
        {"F11  Alt+Enter", Str::HelpFullScreen},
        {"A", Str::HelpAbout},
        {"Esc  F1", Str::HelpClose},
    }};
    const float rowHeight = HelpRowHeight(), pad = HelpPad * scale;
    const Rect card = HelpCard();
    const ui::Color accent = ui::FromLinear(ThemeAt(m_theme).accent);
    DrawCard(card);
    p.Text(FontId::Heading, card.x + pad, card.y + pad - 4 * scale, T(Str::Controls), TextColor);
    // Two columns, made wider where the desktop's font is wider than they were laid out for.
    std::array<float, 2> keysWidth{165 * scale, 125 * scale};
    float actionsWidth = 0;
    for (std::size_t i = 0; i < lines.size(); ++i) {
        const std::size_t column = static_cast<int>(i) < HelpRows ? 0 : 1;
        keysWidth[column] = std::max(keysWidth[column], p.Measure(FontId::Body, lines[i].keys) + 16 * scale);
        if (column == 0)
            actionsWidth = std::max(actionsWidth, p.Measure(FontId::Body, T(lines[i].action)));
    }
    const float top = card.y + pad + 44 * scale;
    const float leftWidth = std::max((card.width - 2 * pad) * 0.56f, keysWidth[0] + actionsWidth + 20 * scale);
    for (std::size_t i = 0; i < lines.size(); ++i) {
        const std::size_t column = static_cast<int>(i) < HelpRows ? 0 : 1;
        const float columnX = card.x + pad + (column == 0 ? 0.0f : leftWidth);
        const float y = top + static_cast<float>(static_cast<int>(i) % HelpRows) * rowHeight;
        p.Text(FontId::Body, columnX, y, lines[i].keys, accent);
        p.Text(FontId::Body, columnX + keysWidth[column], y, T(lines[i].action), TextColor);
    }
    // The rules, in a few sentences.
    float y = top + static_cast<float>(HelpRows) * rowHeight + 12 * scale;
    p.Text(FontId::Body, card.x + pad, y, T(Str::Rules), accent);
    for (const std::string& line : RulesLines()) {
        y += rowHeight;
        p.Text(FontId::Body, card.x + pad, y, line, MutedColor);
    }
    if (Button(HelpClose, {card.Right() - pad - 30 * scale, card.y + pad - 8 * scale, 30 * scale, 30 * scale}, "×") || PressedOutside(card))
        m_dialog = Dialog::None;
    if (m_pointer.leftPressed || m_pointer.rightPressed)
        m_pointer.consumed = true;
}

void App::DrawAbout()
{
    const float scale = Scale();
    ui::Painter& p = *m_painter;
    p.Fill({0, 0, static_cast<float>(m_width), static_cast<float>(m_height)}, 0, Rgba(0, 0, 0, 110));
    const Rect card = AboutCard();
    const float pad = AboutPad * scale;
    const ui::Color accent = ui::FromLinear(ThemeAt(m_theme).accent);
    DrawCard(card);

    // The icon, and beside it the name and the version.
    const float x = card.x + pad, width = card.width - 2 * pad, icon = AboutIcon * scale;
    float y = card.y + pad;
    p.Icon({x, y, icon, icon});
    const float titleHeight = p.LineHeight(FontId::Display), versionHeight = p.LineHeight(FontId::Body);
    const float titleY = y + (icon - titleHeight - versionHeight) / 2;
    p.Text(FontId::Display, x + icon + 18 * scale, titleY, "3D Go", TextColor);
    p.Text(FontId::Body, x + icon + 18 * scale, titleY + titleHeight, "Version " GO3D_VERSION " for Linux", MutedColor);
    y += icon + 20 * scale;

    for (const std::string& line : WrapText(FontId::Body, AboutDescription, width)) {
        p.Text(FontId::Body, x, y, line, TextColor);
        y += p.LineHeight(FontId::Body) + 3 * scale;
    }
    y += 16 * scale;
    p.Text(FontId::Heading, x, y, "Created by Mykhailo Makarov", TextColor);
    y += p.LineHeight(FontId::Heading) + 2 * scale;
    p.Text(FontId::Body, x, y, "© 2026 · MIT License", MutedColor);
    y += p.LineHeight(FontId::Body) + 20 * scale;

    // The author's pages, opened in the browser.
    float nameWidth = 0;
    for (const Link& link : AuthorLinks)
        nameWidth = std::max(nameWidth, p.Measure(FontId::Body, link.name));
    for (std::size_t i = 0; i < AuthorLinks.size(); ++i) {
        const Link& link = AuthorLinks[i];
        const Rect row{x, y, width, LinkRow * scale};
        const Interaction state = Interact(AboutLinks + static_cast<int>(i), row, true);
        p.Fill(row, 9 * scale, state.pressed ? Rgba(255, 255, 255, 52) : state.hover ? Rgba(255, 255, 255, 34) : Rgba(255, 255, 255, 12));
        const float textY = row.y + (row.height - p.LineHeight(FontId::Body)) / 2;
        p.Text(FontId::Body, row.x + 14 * scale, textY, link.name, TextColor);
        p.Text(FontId::Body, row.x + 32 * scale + nameWidth, textY, link.address, accent);
        p.Text(FontId::Body, row.Right() - 14 * scale - p.Measure(FontId::Body, "↗"), textY, "↗", state.hover ? TextColor : MutedColor);
        if (state.clicked)
            OpenLink(link.url);
        y += (LinkRow + LinkGap) * scale;
    }

    if (Button(AboutClose, {card.Right() - pad - 30 * scale, card.y + pad - 8 * scale, 30 * scale, 30 * scale}, "×") || PressedOutside(card))
        m_dialog = Dialog::None;
    if (m_pointer.leftPressed || m_pointer.rightPressed)
        m_pointer.consumed = true;
}

void App::DrawToasts()
{
    const float scale = Scale();
    ui::Painter& p = *m_painter;
    const Rect free = FreeArea();
    float y = static_cast<float>(m_height) - 40 * scale;
    for (auto toast = m_toasts.rbegin(); toast != m_toasts.rend(); ++toast) {
        const float fade = std::min({1.0f, toast->age * 6, (2.8f - toast->age) * 3});
        const float width = p.Measure(FontId::Body, toast->text) + 40 * scale, height = 36 * scale;
        const Rect pill{free.x + (free.width - width) / 2, y - height, width, height};
        p.Shadow(pill, 14 * scale, ui::Fade(Rgba(0, 0, 0, 110), fade));
        p.Fill(pill, height / 2, ui::Fade(Rgba(20, 22, 30, 225), fade));
        p.TextCentered(FontId::Body, pill, toast->text, ui::Fade(TextColor, fade));
        y -= height + 8 * scale;
    }
}
