// «В пределах нормы» — точка входа: окно, цикл кадра, разбор параметров запуска.
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <fstream>
#include <sstream>
#include <string>
#include <vector>

#include "engine/gl_min.h"
#include "engine/sdl_min.h"
#include "game/game.h"

namespace {

struct Args {
    bool dev = false;
    bool noSave = false;
    bool fullscreen = false;
    bool forceWindowed = false;
    int width = 1280;
    int height = 720;
    unsigned long long seed = 0;
    std::string script;
    std::string dataDir;
};

void printHelp() {
    std::printf(
        "В пределах нормы — прототип 0.1\n\n"
        "  --dev            режим разработчика: отладочные клавиши и табло (Tab)\n"
        "  --fullscreen     запуск на весь экран\n"
        "  --windowed       запуск в окне, даже если в настройках включён полный экран\n"
        "  --size ШxВ       размер окна, например --size 1600x900\n"
        "  --seed N         зерно случайных чисел для новой игры\n"
        "  --data ПУТЬ      папка с balance.ini\n"
        "  --nosave         не читать и не писать сохранение и настройки\n"
        "  --script ФАЙЛ    сценарий автотеста (нужен --dev)\n");
}

bool fileExists(const std::string& path) {
    std::ifstream f(path, std::ios::binary);
    return static_cast<bool>(f);
}

int scancodeByName(const std::string& n) {
    struct Key {
        const char* name;
        int code;
    };
    static const Key keys[] = {
        {"W", SDL_SCANCODE_W},           {"A", SDL_SCANCODE_A},
        {"S", SDL_SCANCODE_S},           {"D", SDL_SCANCODE_D},
        {"E", SDL_SCANCODE_E},           {"R", SDL_SCANCODE_R},
        {"1", SDL_SCANCODE_1},           {"2", SDL_SCANCODE_2},
        {"3", SDL_SCANCODE_3},           {"5", SDL_SCANCODE_5},
        {"6", SDL_SCANCODE_6},           {"7", SDL_SCANCODE_7},
        {"8", SDL_SCANCODE_8},           {"9", SDL_SCANCODE_9},
        {"0", SDL_SCANCODE_0},           {"ENTER", SDL_SCANCODE_RETURN},
        {"ESC", SDL_SCANCODE_ESCAPE},    {"TAB", SDL_SCANCODE_TAB},
        {"SPACE", SDL_SCANCODE_SPACE},   {"MINUS", SDL_SCANCODE_MINUS},
        {"LBRACKET", SDL_SCANCODE_LEFTBRACKET}, {"RBRACKET", SDL_SCANCODE_RIGHTBRACKET},
        {"UP", SDL_SCANCODE_UP},         {"DOWN", SDL_SCANCODE_DOWN},
        {"LEFT", SDL_SCANCODE_LEFT},     {"RIGHT", SDL_SCANCODE_RIGHT},
    };
    for (const Key& k : keys)
        if (n == k.name) return k.code;
    return -1;
}

// Сценарий автотеста: по строке на команду.
//   wait N | key ИМЯ [кадров] | mouse DX DY | click | dev КОМАНДА | shot ФАЙЛ | quit
struct Script {
    std::vector<std::string> lines;
    size_t pc = 0;
    int waitFrames = 0;
    struct Held {
        int code;
        int frames;
    };
    std::vector<Held> held;
    std::string pendingShot;
    bool quit = false;

    bool load(const std::string& path) {
        std::ifstream f(path);
        if (!f) return false;
        std::string line;
        while (std::getline(f, line)) {
            const size_t hash = line.find('#');
            if (hash != std::string::npos) line.erase(hash);
            if (line.find_first_not_of(" \t\r") != std::string::npos) lines.push_back(line);
        }
        return true;
    }

    void step(game::Game& g) {
        for (size_t i = 0; i < held.size();) {
            if (--held[i].frames <= 0) {
                g.onKey(held[i].code, false);
                held.erase(held.begin() + static_cast<long>(i));
            } else {
                ++i;
            }
        }
        if (waitFrames > 0) {
            --waitFrames;
            return;
        }
        while (pc < lines.size() && waitFrames == 0 && pendingShot.empty()) {
            std::istringstream in(lines[pc++]);
            std::string cmd;
            in >> cmd;
            if (cmd == "wait") {
                in >> waitFrames;
            } else if (cmd == "key") {
                std::string name;
                int frames = 2;
                in >> name >> frames;
                const int code = scancodeByName(name);
                if (code < 0) {
                    std::fprintf(stderr, "сценарий: неизвестная клавиша %s\n", name.c_str());
                    continue;
                }
                g.onKey(code, true);
                held.push_back({code, frames});
            } else if (cmd == "mouse") {
                float dx = 0, dy = 0;
                in >> dx >> dy;
                g.onMouseMove(dx, dy);
            } else if (cmd == "click") {
                g.onMouseButton(true);
                g.onMouseButton(false);
            } else if (cmd == "dev") {
                std::string rest;
                std::getline(in, rest);
                const size_t start = rest.find_first_not_of(' ');
                g.devCommand(start == std::string::npos ? "" : rest.substr(start));
            } else if (cmd == "shot") {
                in >> pendingShot; // снимок делается после отрисовки кадра
            } else if (cmd == "quit") {
                quit = true;
                return;
            } else {
                std::fprintf(stderr, "сценарий: неизвестная команда %s\n", cmd.c_str());
            }
        }
        if (pc >= lines.size() && waitFrames == 0 && pendingShot.empty() && held.empty()) quit = true;
    }
};

} // namespace

int main(int argc, char** argv) {
    Args args;
    for (int i = 1; i < argc; ++i) {
        const std::string a = argv[i];
        auto value = [&](const char* name) -> const char* {
            if (i + 1 >= argc) {
                std::fprintf(stderr, "после %s нужно значение\n", name);
                std::exit(2);
            }
            return argv[++i];
        };
        if (a == "--dev") args.dev = true;
        else if (a == "--nosave") args.noSave = true;
        else if (a == "--fullscreen") args.fullscreen = true;
        else if (a == "--windowed") args.forceWindowed = true;
        else if (a == "--size") {
            if (std::sscanf(value("--size"), "%dx%d", &args.width, &args.height) != 2) {
                std::fprintf(stderr, "размер задаётся как ШxВ, например 1280x720\n");
                return 2;
            }
        } else if (a == "--seed") args.seed = std::strtoull(value("--seed"), nullptr, 10);
        else if (a == "--script") args.script = value("--script");
        else if (a == "--data") args.dataDir = value("--data");
        else if (a == "--help" || a == "-h") {
            printHelp();
            return 0;
        } else {
            std::fprintf(stderr, "неизвестный параметр: %s\n\n", a.c_str());
            printHelp();
            return 2;
        }
    }

    if (SDL_Init(SDL_INIT_VIDEO | SDL_INIT_AUDIO) != 0) {
        std::fprintf(stderr, "SDL не запустился: %s\n", SDL_GetError());
        return 1;
    }

    // OpenGL 3.3 core. Флаг совместимости «вперёд» обязателен на macOS.
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_MAJOR_VERSION, 3);
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_MINOR_VERSION, 3);
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_PROFILE_MASK, SDL_GL_CONTEXT_PROFILE_CORE);
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_FLAGS, SDL_GL_CONTEXT_FORWARD_COMPATIBLE_FLAG);
    SDL_GL_SetAttribute(SDL_GL_DOUBLEBUFFER, 1);
    SDL_GL_SetAttribute(SDL_GL_DEPTH_SIZE, 24);

    SDL_Window* window =
        SDL_CreateWindow("В пределах нормы", SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED, args.width,
                         args.height, SDL_WINDOW_OPENGL | SDL_WINDOW_RESIZABLE | SDL_WINDOW_ALLOW_HIGHDPI);
    if (!window) {
        std::fprintf(stderr, "не удалось создать окно: %s\n", SDL_GetError());
        SDL_Quit();
        return 1;
    }
    SDL_GLContext context = SDL_GL_CreateContext(window);
    if (!context) {
        std::fprintf(stderr, "не удалось создать контекст OpenGL 3.3: %s\n", SDL_GetError());
        SDL_DestroyWindow(window);
        SDL_Quit();
        return 1;
    }
    if (const char* missing = loadGL(SDL_GL_GetProcAddress)) {
        std::fprintf(stderr, "в драйвере нет функции OpenGL %s\n", missing);
        return 1;
    }

    Script script;
    const bool scripted = !args.script.empty();
    if (scripted) {
        if (!args.dev) {
            std::fprintf(stderr, "--script работает только вместе с --dev\n");
            return 2;
        }
        if (!script.load(args.script)) {
            std::fprintf(stderr, "не удалось открыть сценарий %s\n", args.script.c_str());
            return 2;
        }
    }
    SDL_GL_SetSwapInterval(scripted ? 0 : 1);

    // Папка с данными: рядом с исполняемым файлом, иначе — в текущей папке.
    game::Options options;
    options.dev = args.dev;
    options.noSave = args.noSave;
    options.seed = args.seed;
    if (!args.dataDir.empty()) {
        options.dataDir = args.dataDir;
        if (options.dataDir.back() != '/' && options.dataDir.back() != '\\') options.dataDir += '/';
    } else {
        options.dataDir = "data/";
        if (char* base = SDL_GetBasePath()) {
            const std::string candidate = std::string(base) + "data/";
            if (fileExists(candidate + "balance.ini")) options.dataDir = candidate;
            SDL_free(base);
        }
    }
    if (char* pref = SDL_GetPrefPath("norma", "v-predelah-normy")) {
        options.saveDir = pref;
        SDL_free(pref);
    }

    game::Game game;
    std::string error;
    if (!game.init(options, &error)) {
        std::fprintf(stderr, "игра не запустилась: %s\n", error.c_str());
        return 1;
    }

    bool fullscreen = false;
    auto applyFullscreen = [&](bool want) {
        if (want == fullscreen) return;
        SDL_SetWindowFullscreen(window, want ? static_cast<Uint32>(SDL_WINDOW_FULLSCREEN_DESKTOP) : 0u);
        fullscreen = want;
    };
    bool lastSetting = game.settings().fullscreen;
    applyFullscreen(!args.forceWindowed && (args.fullscreen || lastSetting));

    SDL_SetRelativeMouseMode(SDL_TRUE);

    const double frequency = static_cast<double>(SDL_GetPerformanceFrequency());
    Uint64 last = SDL_GetPerformanceCounter();
    bool running = true;
    while (running && !game.wantsQuit()) {
        SDL_Event ev;
        while (SDL_PollEvent(&ev)) {
            switch (ev.type) {
                case SDL_QUIT: running = false; break;
                case SDL_KEYDOWN:
                    if (ev.key.repeat == 0) game.onKey(static_cast<int>(ev.key.keysym.scancode), true);
                    break;
                case SDL_KEYUP: game.onKey(static_cast<int>(ev.key.keysym.scancode), false); break;
                case SDL_MOUSEMOTION:
                    game.onMouseMove(static_cast<float>(ev.motion.xrel), static_cast<float>(ev.motion.yrel));
                    break;
                case SDL_MOUSEBUTTONDOWN:
                    if (ev.button.button == SDL_BUTTON_LEFT) game.onMouseButton(true);
                    break;
                case SDL_MOUSEBUTTONUP:
                    if (ev.button.button == SDL_BUTTON_LEFT) game.onMouseButton(false);
                    break;
                default: break;
            }
        }

        // Переключение полного экрана из меню настроек.
        if (game.settings().fullscreen != lastSetting) {
            lastSetting = game.settings().fullscreen;
            applyFullscreen(lastSetting);
        }

        const Uint64 now = SDL_GetPerformanceCounter();
        double dt = static_cast<double>(now - last) / frequency;
        last = now;
        if (dt > 0.05) dt = 0.05; // после паузы или зависания не «прыгаем»
        if (scripted) {
            dt = 1.0 / 60.0; // сценарий воспроизводится одинаково на любой машине
            script.step(game);
            if (script.quit) break;
        }

        game.update(static_cast<float>(dt));

        int width = 0, height = 0;
        SDL_GL_GetDrawableSize(window, &width, &height);
        if (width < 1 || height < 1) {
            SDL_Delay(16);
            continue;
        }
        game.render(width, height);
        if (!script.pendingShot.empty()) {
            if (!game.saveScreenshot(script.pendingShot, width, height))
                std::fprintf(stderr, "не удалось сохранить снимок %s\n", script.pendingShot.c_str());
            script.pendingShot.clear();
        }
        SDL_GL_SwapWindow(window);
    }

    game.shutdown();
    SDL_GL_DeleteContext(context);
    SDL_DestroyWindow(window);
    SDL_Quit();
    return 0;
}
