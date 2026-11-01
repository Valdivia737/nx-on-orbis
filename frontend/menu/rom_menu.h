// SPDX-License-Identifier: GPL-3.0-or-later
// eden-ps4: the game list shown before the emulator starts.

#pragma once

#include <string>
#include <vector>

/// Shows `names` (file names) with `initial` selected: Up/Down move (held: repeat), Cross picks,
/// Triangle opens the game's own settings (Left/Right change them, Circle saves and goes back),
/// kept in GameSettingsPath(settings_dir, name). Returns the chosen index, or -1 if the display
/// could not be opened (nothing was shown).
int RunRomMenu(const std::vector<std::string>& names, int initial, const std::string& settings_dir);

/// The per-game settings file for a game file name: same syntax as settings.txt, applied after it.
std::string GameSettingsPath(const std::string& settings_dir, const std::string& game_file);
