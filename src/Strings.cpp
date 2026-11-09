#include "Strings.h"

#include <array>
#include <cstddef>
#include <utility>

namespace {

struct Entry {
    Str id;
    std::string_view english;
    std::string_view russian;
};

constexpr std::array<Entry, static_cast<std::size_t>(Str::Count)> Table{{
    {Str::Title, "3D Go", "Го 3D"},
    {Str::BlackToMove, "Black to play", "Ход чёрных"},
    {Str::WhiteToMove, "White to play", "Ход белых"},
    {Str::Thinking, "Computer is thinking…", "Компьютер думает…"},
    {Str::Counting, "Counting", "Подсчёт очков"},
    {Str::CountingHelp, "Click a group: dead or alive", "Щелчок: мёртвая / живая"},
    {Str::Estimating, "Finding the dead stones…", "Ищу мёртвые камни…"},
    {Str::Draw, "Jigo: a draw", "Ничья (дзиго)"},
    {Str::BlackWins, "Black wins", "Чёрные победили"},
    {Str::WhiteWins, "White wins", "Белые победили"},
    {Str::BlackResigned, "Black resigned", "Чёрные сдались"},
    {Str::WhiteResigned, "White resigned", "Белые сдались"},
    {Str::ScoreDetail, "Black {} · White {} + {} komi", "Чёрные {} · белые {} + {} коми"},
    {Str::BlackPasses, "Black passes", "Чёрные пасуют"},
    {Str::WhitePasses, "White passes", "Белые пасуют"},
    {Str::BothPassed, "Both passed: time to count", "Оба спасовали — подсчёт очков"},
    {Str::Pass, "Pass", "Пас"},
    {Str::Resign, "Resign", "Сдаться"},
    {Str::ConfirmResign, "Sure?", "Точно?"},
    {Str::Accept, "Accept", "Принять"},
    {Str::Resume, "Play on", "Доиграть"},
    {Str::Estimate, "Estimate", "Оценка"},
    {Str::EstimateToast, "{} ahead by about {:.1f}", "{} впереди примерно на {:.1f}"},
    {Str::You, "You", "Вы"},
    {Str::Computer, "Computer", "Компьютер"},
    {Str::Player, "Player", "Игрок"},
    {Str::Black, "Black", "Чёрные"},
    {Str::White, "White", "Белые"},
    {Str::Captures, "prisoners", "пленные"},
    {Str::Points, "points", "очки"},
    {Str::Komi, "komi", "коми"},
    {Str::Moves, "Moves", "Ходы"},
    {Str::NoMovesYet, "No moves yet", "Ходов пока нет"},
    {Str::Undo, "Undo", "Назад"},
    {Str::Redo, "Redo", "Вперёд"},
    {Str::Hint, "Hint", "Подсказка"},
    {Str::NewGame, "New game", "Новая партия"},
    {Str::Flip, "Flip board", "Развернуть"},
    {Str::Mode, "Mode", "Режим"},
    {Str::ModeBlack, "You play Black", "Вы за чёрных"},
    {Str::ModeWhite, "You play White", "Вы за белых"},
    {Str::ModeTwoPlayers, "Two players", "Два игрока"},
    {Str::ModeDemo, "Demo game", "Демонстрация"},
    {Str::Level, "Level", "Уровень"},
    {Str::Level1, "Novice", "Новичок"},
    {Str::Level2, "Beginner", "Начинающий"},
    {Str::Level3, "Casual", "Любитель"},
    {Str::Level4, "Strong", "Сильный"},
    {Str::Level5, "Master", "Мастер"},
    {Str::Board, "Board", "Доска"},
    {Str::Handicap, "Handicap", "Фора"},
    {Str::NoHandicap, "None", "Нет"},
    {Str::StonesFew, "{} stones", "{} камня"},
    {Str::StonesMany, "{} stones", "{} камней"},
    {Str::NextGame, "Applies from the next game (N)", "Вступит в силу в новой партии (N)"},
    {Str::Coordinates, "Coordinates", "Координаты"},
    {Str::Theme, "Theme", "Тема"},
    {Str::Sound, "Sound", "Звук"},
    {Str::LanguageName, "English", "Русский"},
    {Str::Footer, "F1 help · Tab panel · F11 full screen", "F1 справка · Tab панель · F11 весь экран"},
    {Str::Controls, "Controls", "Управление"},
    {Str::KeyClick, "Click", "Щелчок"},
    {Str::KeyDrag, "Drag", "Перетаскивание"},
    {Str::KeyWheel, "Wheel", "Колесо мыши"},
    {Str::HelpClick, "Place a stone; mark dead groups", "Поставить камень; отметить мёртвых"},
    {Str::HelpRotate, "Turn the view (either button)", "Повернуть вид (любой кнопкой)"},
    {Str::HelpZoom, "Zoom in and out", "Приблизить или отдалить"},
    {Str::HelpUndo, "Take back", "Отменить ход"},
    {Str::HelpRedo, "Replay", "Вернуть ход"},
    {Str::HelpPass, "Pass", "Пас"},
    {Str::HelpHint, "Suggest a move", "Подсказать ход"},
    {Str::HelpEstimate, "Estimate the score", "Оценить положение"},
    {Str::HelpAccept, "Accept the count", "Принять подсчёт"},
    {Str::HelpNew, "New game", "Новая партия"},
    {Str::HelpFlip, "Flip the board", "Развернуть доску"},
    {Str::HelpTopView, "View from above", "Вид сверху"},
    {Str::HelpLevel, "Computer level", "Уровень компьютера"},
    {Str::HelpMode, "Who plays whom", "Кто с кем играет"},
    {Str::HelpBoard, "Board size", "Размер доски"},
    {Str::HelpCoordinates, "Coordinates", "Координаты"},
    {Str::HelpTheme, "Next theme", "Следующая тема"},
    {Str::HelpSound, "Sound on or off", "Звук вкл/выкл"},
    {Str::HelpLanguage, "English / Russian", "Русский / английский"},
    {Str::HelpPanel, "Show or hide the panel", "Показать/скрыть панель"},
    {Str::HelpFullScreen, "Full screen", "Весь экран"},
    {Str::HelpCopy, "Copy the game (SGF)", "Копировать партию (SGF)"},
    {Str::HelpPaste, "Paste a game (SGF)", "Вставить партию (SGF)"},
    {Str::HelpAbout, "About the game and its author", "Об игре и её авторе"},
    {Str::HelpClose, "Close this help", "Закрыть справку"},
    {Str::Rules, "Rules", "Правила"},
    {Str::RulesNote,
     "Black and White take turns placing stones where the lines cross. Stones of one color next to each other along the lines form a group; a "
     "group with no empty point beside it is captured. A ko may not be retaken at once, and no move may bring back an earlier position. Two "
     "passes in a row end the game: the dead groups are taken off, and each side counts its stones and the empty points they surround. White "
     "gets komi: 7.5 points, or 0.5 with a handicap.",
     "Чёрные и белые по очереди ставят камни на пересечения линий. Камни одного цвета, соседние по линиям, образуют группу; группа без "
     "свободных соседних пунктов снимается с доски. Ко нельзя отбить сразу же, и никакой ход не может повторить прежнюю позицию. Два паса "
     "подряд завершают партию: мёртвые группы снимаются, и каждый считает свои камни и окружённые ими пустые пункты. Белые получают коми: "
     "7,5 очка, а при форе 0,5."},
    {Str::GameCopied, "Game copied as SGF", "Партия скопирована (SGF)"},
    {Str::GameLoaded, "Game loaded", "Партия загружена"},
    {Str::NothingToPaste, "The clipboard has no SGF game", "В буфере нет партии в SGF"},
    {Str::HintToast, "Hint: ", "Подсказка: "},
    {Str::HintPass, "Hint: pass", "Подсказка: пас"},
    {Str::NothingToUndo, "No moves to take back", "Нечего отменять"},
    {Str::Occupied, "That point is taken", "Пункт занят"},
    {Str::KoForbidden, "Ko: it may not be retaken at once", "Ко: сразу отбивать нельзя"},
    {Str::Suicide, "Suicide is not allowed", "Самоубийственный ход запрещён"},
    {Str::Superko, "Not allowed: the position would repeat", "Нельзя: позиция повторится"},
    {Str::LinkCopied, "No browser opened: the link is copied", "Браузер не открылся: ссылка скопирована"},
}};

constexpr bool InOrder()
{
    for (std::size_t i = 0; i < Table.size(); ++i)
        if (std::to_underlying(Table[i].id) != static_cast<int>(i))
            return false;
    return true;
}
static_assert(InOrder(), "the table lists the strings in the order of Str");

} // namespace

std::string_view Tr(Str id, Language language)
{
    const Entry& entry = Table[static_cast<std::size_t>(std::to_underlying(id))];
    return language == Language::Russian ? entry.russian : entry.english;
}
