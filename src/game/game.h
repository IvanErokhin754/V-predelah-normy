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
        WhiteOut,    // выход на свет
        Kill         // конвоир расправляется с пленным за неподчинение
    };

    enum class Ending { Good, BadLow, BadHigh, Killed };

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
    void finishRun(Ending ending);

    // --- дисциплина конвоя ---
    // Напоминание не считается нарушением; предупреждение — считается.
    // Последнее предупреждение запускает расправу.
    void updateDiscipline(float dt);
    void remind(const std::string& text);
    void warn(const std::string& text);
    void startKill();
    // Таймер простоя: напоминание после reminder секунд, затем
    // предупреждение каждые strikeEvery секунд.
    void idleRule(float dt, float reminder, const std::string& remindText, const std::string& warnText);
    void updateKill(float dt);

    // --- звук в мире: тише и глуше, если источник за стеной или закрытой дверью ---
    float muffleFor(const Vec3& pos) const;
    void playWorld(int sound, const Vec3& pos, float gain);
    void updateAtmosphere(float dt);

    void interact();
    bool canUse(InteractId id) const;
    std::string promptFor(InteractId id) const;
    void doShiftAction(int action);
    void say(const std::string& speaker, const std::string& text, float seconds, int sound = -1);
    void setEscortPath(const std::vector<Vec3>& path, bool leading);
    void setDoor(int id, bool open, bool sound = true);
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
    bool pushing_ = false; // игрок упирается в конвоира

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

    // дисциплина конвоя
    int strikes_ = 0;           // предупреждения за день
    float idle_ = 0;            // простой в текущем правиле
    int idleStage_ = 0;         // 0 — ещё не напоминали, 1 — напомнили, 2+ — предупреждения
    int idleRuleId_ = -1;       // какое правило сейчас считает простой
    float lagTime_ = 0;         // сколько игрок отстаёт от конвоира
    int lagStage_ = 0;
    bool escortWaiting_ = false; // конвоир стоит и ждёт отставшего
    float zoneTime_ = 0;        // сколько игрок стоит там, где ему быть нельзя
    float bumpTime_ = 0;        // сколько игрок толкает конвоира
    bool bumpReminded_ = false; // первое — замечание, дальше — предупреждения
    float strikeFlash_ = 0;     // красная вспышка на экране после предупреждения
    float eyeFlare_ = 0;        // глаз конвоира разгорается
    float strikeCooldown_ = 0;  // не два предупреждения подряд за одно и то же

    // расправа
    int killStep_ = 0;
    float killTime_ = 0;
    Vec3 killPos_;              // где стоит конвоир во время скримера
    float killYaw_ = 0;
    float killDist_ = 1.6f;
    float shake_ = 0;

    // атмосфера
    float ambientTimer_ = 20;   // до следующего далёкого звука
    float paTimer_ = 50;        // до следующего объявления по громкой связи
    int paIndex_ = 0;
    bool nightLights_ = false;  // после отбоя лампа в камере почти гаснет
    float corridorFlicker_ = 1;
    float lever_ = 0, leverTarget_ = 0;
    float sectorDark_ = 0;      // обесточенный сектор: гаснет свет у двери крыла
    float cutKnock_ = -1;       // стук из обесточенного сектора
    int cutKnocks_ = 0;

    // концовка
    bool endingGood_ = false;
    bool endingLow_ = false;
    bool endingKilled_ = false;
    int endingDay_ = 0;
    float endingTime_ = 0;

    bool devOverlay_ = false;
    std::string notice_; // сообщение в меню (например, об ошибке в balance.ini)
};

} // namespace game
