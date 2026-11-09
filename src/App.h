#pragma once

#include "BoardGeometry.h"
#include "BoardView.h"
#include "Camera.h"
#include "Engine.h"
#include "Gl.h"
#include "Go.h"
#include "Renderer.h"
#include "Resources.h"
#include "Settings.h"
#include "Sound.h"
#include "Strings.h"
#include "Text.h"
#include "UiPainter.h"

#include <SDL.h>

#include <array>
#include <chrono>
#include <cstdint>
#include <memory>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

// The game's window: it runs the game, the computer players and the interface, and draws them.
class App {
public:
    App();
    ~App();
    App(const App&) = delete;
    App& operator=(const App&) = delete;

    int Run();

private:
    enum Mode { PlayBlack, PlayWhite, TwoPlayers, Demo };

    // SDL from the first to the last moment of the game; everything else lives within it.
    struct SdlSession {
        SdlSession();
        ~SdlSession();
        SdlSession(const SdlSession&) = delete;
        SdlSession& operator=(const SdlSession&) = delete;
    };

    struct WindowDeleter {
        void operator()(SDL_Window* window) const { SDL_DestroyWindow(window); }
    };
    struct CursorDeleter {
        void operator()(SDL_Cursor* cursor) const { SDL_FreeCursor(cursor); }
    };

    // What the mouse did since the last frame, in pixels.
    struct Pointer {
        float x = -1, y = -1;
        bool leftDown = false, rightDown = false;
        bool leftPressed = false, leftReleased = false, rightPressed = false, rightReleased = false;
        float wheel = 0;
        bool consumed = false; // taken by the interface
    };

    enum class Gesture { None, PressPoint, TurnCamera };

    // The card shown over the game, which takes the mouse while it is open.
    enum class Dialog { None, Help, About };

    // What the pointer does to a widget in this frame.
    struct Interaction {
        bool hover = false, pressed = false, clicked = false;
    };

    struct Toast {
        std::string text;
        float age = 0;
    };

    // The window's size when it is neither maximized nor full screen, in window units.
    struct WindowSize {
        int width = 0, height = 0;
    };

    void CreateMainWindow(const resources::Image& icon);
    void HandleEvent(const SDL_Event& event);
    void HandleWindowEvent(const SDL_WindowEvent& event);
    void UpdateSize();
    void ShowLoading();
    void BuildFonts();
    void Frame();
    void Update(float seconds);
    bool NeedsAnimation() const;
    render::Scene BuildScene() const;

    // The game.
    go::Phase Phase() const { return m_game.GetPhase(); }
    bool IsHuman(go::Color side) const;
    bool HumanToMove() const;
    void NewGame();
    // Sets out a game, with the board of its size.
    void StartGame(go::Game game);
    void PlayMove(go::Point point);
    void Resign(go::Color who);
    void Undo();
    void Redo();
    void Accept();
    void SetMode(int mode);
    void SetLevel(int level);
    void SetBoardSize(int size);
    void SetHandicap(int handicap);
    void SetTheme(int theme);
    void ToggleCoordinates();
    void ToggleSound();
    void ToggleLanguage();
    void StopWorkers();
    void UpdateComputer(float seconds);
    void UpdateScoring(float seconds);
    void UpdateHelpers();
    void RequestHint();
    void RequestEstimate();
    void AfterPositionChange();
    // A toast and a bell for a pass, or the start of the counting after two.
    void AnnouncePass();
    void CopyGame();
    void PasteGame();
    void OpenLink(const char* url);
    void Save();

    // Input.
    void PointerAt(int windowX, int windowY);
    void HandleBoardPointer();
    void HandleKey(const SDL_KeyboardEvent& key);
    go::Point PointUnderPointer() const;
    math::Vec3 PointerOnPlane(float height) const;
    go::Color ViewSide() const { return m_mode == PlayWhite ? go::Color::White : go::Color::Black; }
    // Why a stone may not go on an empty point, for a toast.
    Str IllegalReason(go::Point point) const;

    // The interface.
    float Scale() const { return m_scale; }
    ui::Rect PanelRect() const;
    ui::Rect FreeArea() const;
    std::vector<render::FrostedPanel> FrostedPanels() const;
    // The text in lines no wider than `width`, broken at the spaces.
    std::vector<std::string> WrapText(ui::FontId font, std::string_view text, float width) const;
    // The rules note in lines that fit the help.
    std::vector<std::string> RulesLines() const;
    float HelpRowHeight() const;
    // A card of the size over the game: beside the panel, or over it where the window is too narrow for that.
    ui::Rect CenteredCard(float width, float height) const;
    ui::Rect HelpCard() const;
    ui::Rect AboutCard() const;
    ui::Rect BannerCard() const;
    void DrawInterface();
    void DrawPanel();
    void DrawScore(ui::Rect area);
    void DrawMoveList(ui::Rect area);
    void DrawStatus(ui::Rect area);
    void DrawBanner();
    // The background of a card over the game.
    void DrawCard(ui::Rect card);
    void DrawHelp();
    void DrawAbout();
    void DrawToasts();
    void ToggleDialog(Dialog dialog);
    // A press outside the open card closes it, unless it is the click that opened it.
    bool PressedOutside(ui::Rect card) const;
    // Hovering, pressing and clicking; a click sounds.
    Interaction Interact(int id, ui::Rect rect, bool enabled);
    bool Button(int id, ui::Rect rect, std::string_view label, bool enabled = true, bool accent = false);
    // A label with a value stepped by two arrows, past the width of the longest label of the column; returns -1 or 1
    // when an arrow is clicked, else 0.
    int Selector(int id, ui::Rect rect, std::string_view label, std::string_view value, float labelWidth);
    void ShowToast(std::string text);
    std::string_view T(Str id) const { return Tr(id, m_language); }
    std::string PlayerName(go::Color side) const;
    std::string HandicapText(int stones) const;
    // The winner and how, once the game is over.
    std::string ResultTitle() const;
    std::string ResultDetail() const;

    void ToggleFullScreen();

    // Declared in the order of their making; destroyed the other way round, the window after what draws in it.
    SdlSession m_sdl;
    SoundBank m_sound;
    std::unique_ptr<SDL_Window, WindowDeleter> m_window;
    std::unique_ptr<SDL_Cursor, CursorDeleter> m_arrowCursor, m_handCursor;
    std::unique_ptr<gl::Context> m_context;
    std::unique_ptr<render::Renderer> m_renderer;
    std::unique_ptr<ui::Painter> m_painter;
    std::array<std::unique_ptr<text::Font>, 3> m_fonts;
    std::unique_ptr<BoardView> m_view;

    Settings m_settings;
    Language m_language = Language::English;
    int m_width = 0, m_height = 0;             // of the picture, in pixels
    int m_windowWidth = 0, m_windowHeight = 0; // in window units, which the mouse reports
    float m_textScale = 1;                     // the desktop's
    float m_scale = 1;                         // of the interface: the desktop's, in pixels
    float m_fontScale = 0;                     // that the fonts were made for
    bool m_running = true;
    bool m_dirty = true; // something to draw since the last frame
    bool m_fullScreen = false;
    bool m_maximizedBeforeFullScreen = false;
    WindowSize m_normalSize, m_previousNormalSize;
    std::uint32_t m_normalSizeTime = 0; // when the window last took a size of its own, in SDL's milliseconds
    bool m_cursorHand = false, m_cursorShownHand = false;
    std::chrono::steady_clock::time_point m_lastFrame;
    float m_time = 0;

    go::Game m_game;
    board::Layout m_layout;
    OrbitCamera m_camera;
    int m_mode = PlayBlack;
    int m_level = 2;
    int m_theme = 0;
    int m_boardSize = 9; // for new games
    int m_handicap = 0;  // for new games
    bool m_coordinates = true;

    go::Worker<go::SearchResult> m_computer;
    bool m_computerThinking = false;
    float m_computerClock = 0;
    std::optional<go::SearchResult> m_computerResult;
    go::Worker<go::SearchResult> m_hinter;
    bool m_hintThinking = false;
    std::optional<go::Point> m_hint;
    float m_hintGlow = 0;
    go::Worker<go::Ownership> m_estimator;
    bool m_estimating = false;
    std::optional<go::Ownership> m_estimate; // shown over the board until the position changes
    go::Worker<go::Ownership> m_deadFinder;  // at the start of the counting
    bool m_findingDead = false;
    int m_deadFoundPly = -1;
    float m_scoringClock = 0;
    float m_resignConfirm = 0; // while above zero, a second click on Resign resigns
    bool m_announcedEnd = false;
    std::vector<BoardView::Event> m_events;

    Pointer m_pointer;
    Gesture m_gesture = Gesture::None;
    float m_pressX = 0, m_pressY = 0;
    float m_dragX = 0, m_dragY = 0; // the pointer when the camera last turned
    go::Point m_pressPoint = go::Pass;
    go::Point m_hoverPoint = go::Pass; // a legal point under the pointer, or a stone while counting

    int m_activeWidget = 0;
    bool m_panelVisible = true;
    float m_panelSlide = 1;
    Dialog m_dialog = Dialog::None;
    bool m_dialogOpening = false; // opened in this frame
    float m_moveScroll = 0;
    bool m_followMoves = true;
    float m_bannerFade = 0;
    std::string m_bannerTitle, m_bannerDetail; // kept while the banner fades after a new game
    std::vector<Toast> m_toasts;
};
