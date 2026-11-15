// Игра целиком: цикл дня, игрок, конвоир, реплики, меню, концовки.
#pragma once

#include <deque>
#include <string>
#include <vector>

#include "../core/game_state.h"
#include "../engine/audio.h"
#include "../engine/renderer.h"
#include "level.h"
#include "terminal.h"

namespace game {

struct Options {
    bool dev = false;          // режим разработчика: отладочные клавиши и табло
    std::string dataDir;       // где лежит balance.ini
    std::string saveDir;       // где хранить сохранение и настройки
    bool noSave = false;       // не читать и не писать файлы (для автотестов)
    uint64_t seed = 0;         // 0 — зерно от времени запуска
};

class Game {
public:
    bool init(const Options& options, std::string* error);
    void shutdown();

    // --- ввод (скан-коды SDL) ---
    void onKey(int scancode, bool down);
    void onMouseMove(float dx, float dy);
    void onMouseButton(bool down);

    void update(float dt);
    void render(int width, int height);

    bool wantsQuit() const { return quit_; }
    const Settings& settings() const { return settings_; }

    // Команды для автотестов и отладки (только с --dev):
    //   new | continue | rating N | release | finish | endday | tp X Z YAW PITCH | state
    void devCommand(const std::string& command);
    bool saveScreenshot(const std::string& path, int w, int h) { return renderer_.saveScreenshot(path, w, h); }

private:
    enum class Screen { MainMenu, SettingsMenu, Playing, Paused, Ending };

    // Этапы дня.
    enum class Phase {
        Dawn,        // затемнение, номер дня
        MorningCall, // команда по громкой связи, ожидание у двери
        EscortOut,   // конвоир ведёт на проверку (или в другую сторону)
        AtTest,      // помещение для тестов: терминал и смена
        EscortBack,  // обратно в камеру
        Evening,     // камера заперта, можно лечь на койку
        Sleep,       // затемнение перед новым днём
        BadEnd,      // дверь закрылась, звук оборвался
        FreeWalk,    // хорошая концовка: путь к дневному свету
        WhiteOut     // выход на свет
    };

    enum class EscortMode { Normal, Watch, Bad, Off };

    struct Escort {
        Vec3 pos;
        float yaw = 0;
        std::vector<Vec3> path;
        size_t next = 0;
        bool leading = false;   // ждёт отставшего игрока
        int waitDoor = -1;      // не идти дальше, пока эта дверь не откроется
        float stepTimer = 0;
        float pause = 0;        // пауза «наблюдения»
        float sincePause = 0;
        float nag = 0;
        float bob = 0;
        bool moving = false;
        int stepCount = 0;
    };

    struct Line {
        std::string speaker;
        std::string text;
        float seconds;
        int sound; // audio::Sound или -1
    };

    // --- жизненный цикл ---
    void startNewGame();
    bool continueGame();
    void startDay();
    void enterPhase(Phase p);
    void updatePlaying(float dt);
    void updatePhase(float dt);
    void updatePlayer(float dt);
    void updateEscort(float dt);
    void updateLights(float dt);
    void updateSound();
    void finishRun(bool good);

    void interact();
    bool canUse(InteractId id) const;
    std::string promptFor(InteractId id) const;
    void doShiftAction(int action);
    void say(const std::string& speaker, const std::string& text, float seconds, int sound = -1);
    void setEscortPath(const std::vector<Vec3>& path, bool leading);
    void setDoor(int id, bool open);
    bool controlLocked() const;
    Vec3 cameraEye() const;

    // --- сохранение ---
    void saveGame();
    void eraseSave();
    bool saveExists() const;
    void loadSettings();
    void storeSettings();

    // --- отрисовка ---
    void drawWorld(int width, int height);
    void drawHud(int width, int height);
    void drawMenu(int width, int height);
    void drawEnding(int width, int height);
    void drawDev(int width, int height);
    void menuKey(int scancode);
    std::vector<std::string> menuItems() const;
    void menuActivate(int index, int direction);

    Options options_;
    Settings settings_;
    core::GameState state_;
    gfx::Renderer renderer_;
    gfx::RenderTarget screenTarget_;
    audio::Audio audio_;
    Level level_;
    Terminal terminal_;

    Screen screen_ = Screen::MainMenu;
    Screen menuReturn_ = Screen::MainMenu;
    int menuIndex_ = 0;
    bool quit_ = false;
    float time_ = 0;

    // игрок
    Vec3 playerPos_;
    float yaw_ = 0, pitch_ = 0;
    float bobPhase_ = 0;
    float stepDist_ = 0;
    bool keys_[512] = {false};
    bool monitorMode_ = false;
    float monitorBlend_ = 0; // 0 — обычный вид, 1 — камера у экрана
    InteractId aimed_ = UseNone;

    // день
    Phase phase_ = Phase::Dawn;
    float phaseTime_ = 0;
    int phaseStep_ = 0;
    Escort escort_;
    EscortMode escortMode_ = EscortMode::Normal;
    bool shiftAnnounced_ = false;
    float tasksDoneTimer_ = 0;
    float knockTimer_ = 0;
    float extraKnock_ = -1;
    float replyKnock_ = -1;
    float fade_ = 1;          // 1 — чёрный экран
    float fadeTarget_ = 0;
    float fadeSpeed_ = 0.6f;
    float white_ = 0;         // белая вспышка хорошей концовки
    float dayCard_ = 0;
    float lampFlicker_ = 1;
    bool humanVisible_ = false;
    Vec3 humanPos_;
    float humanYaw_ = 0;
    bool silent_ = false;     // после обрыва звука в плохой концовке

    std::deque<Line> lines_;
    float lineTime_ = 0;
    float lineVoiceDelay_ = -1; // «речь» после сигнала громкой связи
    uint32_t noise_ = 12345u;   // генератор для мерцания ламп
    float flickerTimer_ = 0;

    // концовка
    bool endingGood_ = false;
    bool endingLow_ = false;
    int endingDay_ = 0;
    float endingTime_ = 0;

    bool devOverlay_ = false;
    std::string notice_; // сообщение в меню (например, об ошибке в balance.ini)
};

} // namespace game
