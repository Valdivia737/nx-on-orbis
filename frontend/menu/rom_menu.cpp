// SPDX-License-Identifier: GPL-3.0-or-later
// eden-ps4: the game list shown before the emulator starts. See rom_menu.h.
//
// It draws with the CPU into two linear framebuffers in direct memory and flips them with
// sceVideoOut, then closes the video out so that RADV's WSI can open the display for Eden. The
// framebuffers' 16 MiB are never given back: the display may still be scanning the last one out
// after the close, and freeing memory under it is a risk this console answers with a reboot.
//
// Triangle opens a game's own settings (like Crema's per-game profiles): a few settings.txt keys
// written to juegos/<game>.txt, which the frontend applies after settings.txt.

#include <algorithm>
#include <chrono>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <fstream>
#include <string>
#include <thread>
#include <utility>
#include <vector>

#include <sys/mman.h>
#include <sys/stat.h>
#include <sys/types.h>

#include "menu/menu_font.h"
#include "menu/rom_menu.h"
#include "ps4_platform.h"

extern "C" {
int32_t sceVideoOutOpen(int32_t user, int32_t bus, int32_t index, const void* param);
int32_t sceVideoOutClose(int32_t handle);
void sceVideoOutSetBufferAttribute(void* attribute, uint32_t pixel_format, uint32_t tiling,
                                   uint32_t aspect, uint32_t width, uint32_t height,
                                   uint32_t pitch);
int32_t sceVideoOutRegisterBuffers(int32_t handle, int32_t start, void* const* addresses,
                                   int32_t count, const void* attribute);
int32_t sceVideoOutSubmitFlip(int32_t handle, int32_t index, uint32_t mode, int64_t arg);
int32_t sceVideoOutSetFlipRate(int32_t handle, int32_t rate);
int32_t sceVideoOutIsFlipPending(int32_t handle);
int32_t sceKernelAllocateDirectMemory(off_t start, off_t end, size_t len, size_t align, int type,
                                      off_t* phys);
int32_t sceKernelMapDirectMemory(void** addr, size_t len, int prot, int flags, off_t phys,
                                 size_t align);
int32_t sceKernelReleaseDirectMemory(off_t start, size_t len);
size_t sceKernelGetDirectMemorySize(void);
}

namespace {

constexpr int Width = 1920;
constexpr int Height = 1080;
constexpr size_t BufferBytes = size_t(Width) * Height * 4;
constexpr size_t Align = 2u << 20;
constexpr size_t TotalBytes = (BufferBytes * 2 + Align - 1) & ~(Align - 1);
constexpr uint32_t PixelFormatA8R8G8B8Srgb = 0x80000000u; // pixels are 0xAARRGGBB
constexpr uint32_t TilingLinear = 1;
constexpr int MemoryWcGarlic = 3;
constexpr int ProtCpuRwGpuRw = 0x33;
constexpr uint32_t FlipVsync = 1;

constexpr uint32_t Background = 0xFF141821;
constexpr uint32_t Text = 0xFFE8E8E8;
constexpr uint32_t Dim = 0xFF8A93A6;
constexpr uint32_t Highlight = 0xFF2F5FD0;

struct Canvas {
    uint32_t* pixels;

    void Fill(int x, int y, int w, int h, uint32_t color) {
        for (int j = std::max(y, 0); j < std::min(y + h, Height); ++j) {
            std::fill(pixels + size_t(j) * Width + std::max(x, 0),
                      pixels + size_t(j) * Width + std::min(x + w, Width), color);
        }
    }

    /// Draws ASCII text (anything else as '?'), clipped at max_chars, blending the glyph
    /// coverage over the background color behind it.
    void Print(int x, int y, const std::string& text, uint32_t color, uint32_t behind,
               size_t max_chars = 200) {
        for (size_t i = 0; i < text.size() && i < max_chars; ++i) {
            unsigned char c = static_cast<unsigned char>(text[i]);
            if (c < 32 || c > 126) {
                c = '?';
            }
            const std::uint8_t* glyph = MenuFont::Glyphs[c - 32];
            const int gx = x + int(i) * MenuFont::Width;
            for (int row = 0; row < MenuFont::Height; ++row) {
                const int py = y + row;
                if (py < 0 || py >= Height) {
                    continue;
                }
                for (int col = 0; col < MenuFont::Width; ++col) {
                    const int px = gx + col;
                    const unsigned a = glyph[row * MenuFont::Width + col];
                    if (a == 0 || px < 0 || px >= Width) {
                        continue;
                    }
                    const auto mix = [a](uint32_t fg, uint32_t bg, int shift) {
                        const unsigned f = (fg >> shift) & 0xFF;
                        const unsigned b = (bg >> shift) & 0xFF;
                        return ((f * a + b * (255 - a)) / 255) << shift;
                    };
                    pixels[size_t(py) * Width + px] = 0xFF000000u | mix(color, behind, 16) |
                                                      mix(color, behind, 8) |
                                                      mix(color, behind, 0);
                }
            }
        }
    }
};

std::string DisplayName(const std::string& file) {
    std::string name = file;
    if (const auto dot = name.find_last_of('.'); dot != std::string::npos && dot > 0) {
        name.erase(dot);
    }
    return name;
}

// ---- per-game settings ------------------------------------------------------------------------

struct GameOption {
    const char* key;   // settings.txt key
    const char* label; // shown (ASCII only: the font has no accents)
    std::vector<std::pair<const char*, const char*>> values; // written value, shown text; [0] = Global
};

const std::vector<GameOption> GameOptions = {
    {"fastmem", "Fastmem (memoria rapida)", {{"", "Global"}, {"on", "Si"}, {"off", "No"}}},
    {"fps", "Limite de FPS", {{"", "Global"}, {"30", "30 fijos"}, {"auto", "Sin limite"}, {"60", "60"}}},
    {"regalloc", "Registros de Eden4", {{"", "Global"}, {"on", "Si"}, {"off", "No"}}},
    {"language", "Idioma",
     {{"", "Global"}, {"es-419", "Espanol latino"}, {"es", "Espanol (Espana)"}, {"en", "Ingles"}}},
};

struct GameSettings {
    std::vector<size_t> choice;     // index into GameOptions[i].values
    std::vector<std::string> other; // lines of the file this menu does not manage, kept as they are

    bool Custom() const {
        return std::any_of(choice.begin(), choice.end(), [](size_t c) { return c != 0; });
    }
};

GameSettings LoadGameSettings(const std::string& path) {
    GameSettings s{std::vector<size_t>(GameOptions.size(), 0), {}};
    std::ifstream in{path};
    for (std::string line; std::getline(in, line);) {
        line.erase(std::remove(line.begin(), line.end(), '\r'), line.end());
        std::string bare = line.substr(0, line.find('#'));
        bare.erase(std::remove(bare.begin(), bare.end(), ' '), bare.end());
        const auto eq = bare.find('=');
        bool managed = false;
        if (eq != std::string::npos) {
            const std::string key = bare.substr(0, eq), value = bare.substr(eq + 1);
            for (size_t i = 0; i < GameOptions.size() && !managed; ++i) {
                if (key != GameOptions[i].key) {
                    continue;
                }
                for (size_t v = 1; v < GameOptions[i].values.size(); ++v) {
                    if (value == GameOptions[i].values[v].first) {
                        s.choice[i] = v;
                        managed = true;
                    }
                }
            }
        }
        if (!managed && !line.empty() && line.rfind("# Ajustes de ", 0) != 0) {
            s.other.push_back(line);
        }
    }
    return s;
}

void SaveGameSettings(const std::string& settings_dir, const std::string& game,
                      const GameSettings& s) {
    mkdir((settings_dir + "/juegos").c_str(), 0777);
    const std::string path = GameSettingsPath(settings_dir, game);
    std::ofstream out{path, std::ios::trunc};
    if (!out) {
        Ps4::Log("menu: could not write %s", path.c_str());
        return;
    }
    out << "# Ajustes de " << DisplayName(game) << " (menu de Eden, Triangulo); van despues de settings.txt\n";
    for (const std::string& line : s.other) {
        out << line << '\n';
    }
    for (size_t i = 0; i < GameOptions.size(); ++i) {
        if (s.choice[i] != 0) {
            out << GameOptions[i].key << '=' << GameOptions[i].values[s.choice[i]].first << '\n';
        }
    }
    Ps4::Log("menu: settings of '%s' saved (%s)", game.c_str(), s.Custom() ? "own" : "all global");
}

void DrawGameSettings(Canvas& c, const std::string& game, const GameSettings& s, int row_selected) {
    constexpr int Left = 120;
    constexpr int RowHeight = 70;
    constexpr int ListTop = 260;
    constexpr int ValueX = 1100;
    c.Fill(0, 0, Width, Height, Background);
    c.Print(Left, 90, "Ajustes de este juego", Text, Background);
    c.Print(Left, 150, DisplayName(game), Dim, Background, (Width - 2 * Left) / MenuFont::Width);
    c.Fill(Left, 200, Width - 2 * Left, 3, Dim);
    for (size_t i = 0; i < GameOptions.size(); ++i) {
        const int y = ListTop + int(i) * RowHeight;
        const bool on = int(i) == row_selected;
        const uint32_t behind = on ? Highlight : Background;
        if (on) {
            c.Fill(Left, y - 8, Width - 2 * Left, RowHeight - 14, Highlight);
        }
        c.Print(Left + 20, y, GameOptions[i].label, on ? 0xFFFFFFFF : Text, behind);
        const std::string value = std::string("<  ") + GameOptions[i].values[s.choice[i]].second + "  >";
        c.Print(ValueX, y, value, on ? 0xFFFFFFFF : (s.choice[i] != 0 ? Text : Dim), behind);
    }
    c.Print(Left, ListTop + int(GameOptions.size()) * RowHeight + 30,
            "Global = lo que diga settings.txt. Se aplica la proxima vez que abras el juego.", Dim,
            Background);
    c.Fill(Left, Height - 130, Width - 2 * Left, 3, Dim);
    c.Print(Left, Height - 100, "Arriba/Abajo: elegir    Izquierda/Derecha: cambiar    Circulo: guardar y volver",
            Dim, Background);
}

void Draw(Canvas& c, const std::vector<std::string>& names, const std::vector<bool>& custom,
          int selected) {
    constexpr int Left = 120;
    constexpr int RowHeight = 56;
    constexpr int ListTop = 220;
    constexpr int Rows = 13;
    constexpr size_t MaxChars = (Width - 2 * Left - 40) / MenuFont::Width;
    c.Fill(0, 0, Width, Height, Background);
    c.Print(Left, 90, "Eden PS4 - elegi un juego", Text, Background);
    c.Fill(Left, 150, Width - 2 * Left, 3, Dim);

    const int count = int(names.size());
    const int first = std::clamp(selected - Rows / 2, 0, std::max(count - Rows, 0));
    for (int row = 0; row < Rows && first + row < count; ++row) {
        const int index = first + row;
        const int y = ListTop + row * RowHeight;
        const bool on = index == selected;
        if (on) {
            c.Fill(Left, y - 8, Width - 2 * Left, RowHeight - 4, Highlight);
        }
        // A game with its own settings gets a mark at the end of its row.
        constexpr char Mark[] = "[ajustes propios]";
        const size_t mark_chars = sizeof(Mark) - 1;
        const bool marked = custom[index];
        c.Print(Left + 20, y, DisplayName(names[index]), on ? 0xFFFFFFFF : Text,
                on ? Highlight : Background, marked ? MaxChars - mark_chars - 2 : MaxChars);
        if (marked) {
            c.Print(Width - Left - 20 - int(mark_chars) * MenuFont::Width, y, Mark,
                    on ? 0xFFFFFFFF : Dim, on ? Highlight : Background);
        }
    }
    if (first > 0) {
        c.Print(Width - Left - MenuFont::Width, ListTop - 50, "^", Dim, Background);
    }
    if (first + Rows < count) {
        c.Print(Width - Left - MenuFont::Width, ListTop + Rows * RowHeight, "v", Dim, Background);
    }
    c.Fill(Left, Height - 130, Width - 2 * Left, 3, Dim);
    c.Print(Left, Height - 100, "Arriba/Abajo: elegir    X: jugar    Triangulo: ajustes del juego", Dim,
            Background);
    char position[32];
    std::snprintf(position, sizeof(position), "%d / %d", selected + 1, count);
    c.Print(Width - Left - int(std::strlen(position)) * MenuFont::Width, Height - 100, position,
            Dim, Background);
}

} // Anonymous namespace

std::string GameSettingsPath(const std::string& settings_dir, const std::string& game_file) {
    return settings_dir + "/juegos/" + DisplayName(game_file) + ".txt";
}

int RunRomMenu(const std::vector<std::string>& names, int initial, const std::string& settings_dir) {
    if (names.empty()) {
        return -1;
    }
    off_t phys = 0;
    if (int rc = sceKernelAllocateDirectMemory(0, off_t(sceKernelGetDirectMemorySize()),
                                               TotalBytes, Align, MemoryWcGarlic, &phys);
        rc != 0) {
        Ps4::Log("menu: direct memory for the framebuffers refused (0x%x)", unsigned(rc));
        return -1;
    }
    void* memory = nullptr;
    if (int rc = sceKernelMapDirectMemory(&memory, TotalBytes, ProtCpuRwGpuRw, 0, phys, Align);
        rc != 0) {
        Ps4::Log("menu: mapping the framebuffers failed (0x%x)", unsigned(rc));
        sceKernelReleaseDirectMemory(phys, TotalBytes);
        return -1;
    }
    const int handle = sceVideoOutOpen(255, 0, 0, nullptr);
    if (handle < 0) {
        Ps4::Log("menu: sceVideoOutOpen failed (0x%x)", unsigned(handle));
        munmap(memory, TotalBytes);
        sceKernelReleaseDirectMemory(phys, TotalBytes);
        return -1;
    }
    void* buffers[2] = {memory, static_cast<std::uint8_t*>(memory) + BufferBytes};
    alignas(16) std::uint8_t attribute[64] = {};
    sceVideoOutSetBufferAttribute(attribute, PixelFormatA8R8G8B8Srgb, TilingLinear, 0, Width,
                                  Height, Width);
    sceVideoOutSetFlipRate(handle, 0);
    if (int rc = sceVideoOutRegisterBuffers(handle, 0, buffers, 2, attribute); rc < 0) {
        Ps4::Log("menu: sceVideoOutRegisterBuffers failed (0x%x)", unsigned(rc));
        sceVideoOutClose(handle);
        munmap(memory, TotalBytes);
        sceKernelReleaseDirectMemory(phys, TotalBytes);
        return -1;
    }
    Ps4::Log("menu: %zu games listed", names.size());

    std::vector<GameSettings> settings;
    std::vector<bool> custom;
    for (const std::string& name : names) {
        settings.push_back(LoadGameSettings(GameSettingsPath(settings_dir, name)));
        custom.push_back(settings.back().Custom());
    }

    int selected = std::clamp(initial, 0, int(names.size()) - 1);
    bool editing = false; // the game's settings page instead of the list
    int option = 0;
    int back = 0;
    const auto present = [&] {
        while (sceVideoOutIsFlipPending(handle) > 0) {
            std::this_thread::sleep_for(std::chrono::milliseconds(1));
        }
        Canvas canvas{static_cast<uint32_t*>(buffers[back])};
        if (editing) {
            DrawGameSettings(canvas, names[selected], settings[selected], option);
        } else {
            Draw(canvas, names, custom, selected);
        }
        sceVideoOutSubmitFlip(handle, back, FlipVsync, 0);
        back ^= 1;
    };
    present();

    constexpr std::uint32_t Intercepted = 0x80000000u;
    std::uint32_t previous = Ps4::ReadPad().buttons;
    std::uint64_t repeat_at = 0;
    for (;;) {
        const std::uint32_t now = Ps4::ReadPad().buttons;
        if (now & Intercepted) {
            previous = 0;
            std::this_thread::sleep_for(std::chrono::milliseconds(16));
            continue;
        }
        const std::uint32_t pressed = now & ~previous;
        previous = now;
        if (editing) {
            if (pressed & (Ps4::Button::Circle | Ps4::Button::Triangle | Ps4::Button::Cross)) {
                SaveGameSettings(settings_dir, names[selected], settings[selected]);
                custom[selected] = settings[selected].Custom();
                editing = false;
                present();
            } else if (pressed & (Ps4::Button::Left | Ps4::Button::Right)) {
                const size_t n = GameOptions[option].values.size();
                size_t& c = settings[selected].choice[option];
                c = (pressed & Ps4::Button::Left) ? (c + n - 1) % n : (c + 1) % n;
                present();
            } else if (pressed & (Ps4::Button::Up | Ps4::Button::Down)) {
                const int n = int(GameOptions.size());
                option = (option + ((pressed & Ps4::Button::Up) ? -1 : 1) + n) % n;
                present();
            }
            std::this_thread::sleep_for(std::chrono::milliseconds(16));
            continue;
        }
        if (pressed & Ps4::Button::Cross) {
            break;
        }
        if (pressed & Ps4::Button::Triangle) {
            editing = true;
            option = 0;
            present();
            std::this_thread::sleep_for(std::chrono::milliseconds(16));
            continue;
        }
        int step = 0;
        const std::uint32_t vertical = now & (Ps4::Button::Up | Ps4::Button::Down);
        if (pressed & (Ps4::Button::Up | Ps4::Button::Down)) {
            step = (pressed & Ps4::Button::Up) ? -1 : 1;
            repeat_at = Ps4::NowUs() + 400000; // held: repeat after 0.4 s
        } else if (vertical != 0 && Ps4::NowUs() >= repeat_at) {
            step = (vertical & Ps4::Button::Up) ? -1 : 1;
            repeat_at = Ps4::NowUs() + 110000;
        }
        if (step != 0) {
            const int count = int(names.size());
            selected = (selected + step + count) % count;
            present();
        }
        std::this_thread::sleep_for(std::chrono::milliseconds(16));
    }

    // Leave the display as Eden's WSI expects to find it: closed.
    {
        Canvas canvas{static_cast<uint32_t*>(buffers[back])};
        canvas.Fill(0, 0, Width, Height, 0xFF000000);
        while (sceVideoOutIsFlipPending(handle) > 0) {
            std::this_thread::sleep_for(std::chrono::milliseconds(1));
        }
        sceVideoOutSubmitFlip(handle, back, FlipVsync, 0);
        while (sceVideoOutIsFlipPending(handle) > 0) {
            std::this_thread::sleep_for(std::chrono::milliseconds(1));
        }
    }
    const int closed = sceVideoOutClose(handle);
    Ps4::Log("menu: game %d of %zu chosen, display closed (0x%x); framebuffers kept",
             selected + 1, names.size(), unsigned(closed));
    return selected;
}
