#pragma once

#include "Strings.h"

#include <string>

// The choices and the game in progress, kept between sessions in ~/.config/go3d/go3d.ini.
struct Settings {
    int theme = 0;
    int level = 2;
    int mode = 0;
    int boardSize = 9; // for new games
    int handicap = 0;  // for new games
    bool coordinates = true;
    bool sound = true;
    bool panel = true;
    Language language = Language::English;
    int windowWidth = 0, windowHeight = 0; // when it is neither maximized nor full screen; 0 before the first run
    bool maximized = false;
    std::string game; // in SGF, on one line

    static Settings Load();
    void Save() const;
};
