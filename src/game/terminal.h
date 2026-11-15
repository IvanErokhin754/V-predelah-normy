// Терминал: 2D-слой на экране ЭЛТ-монитора. Рисуется в текстуру 1024x768,
// которую рендер натягивает на экран в 3D-сцене.
#pragma once

#include "../core/game_state.h"
#include "../engine/audio.h"
#include "../engine/renderer.h"

namespace game {

struct Settings {
    bool ripple = true;      // рябь и бегущие строки на экране терминала
    bool typewriter = true;  // посимвольная печать
    float mouse = 1.0f;      // чувствительность мыши
    float volume = 0.8f;
    float music = 0.6f;      // громкость эмбиента (data/audio/ambient.wav)
    bool fullscreen = false;
};

class Terminal {
public:
    static const int kWidth = 1024;
    static const int kHeight = 768;

    // Сброс состояния экрана (новый день или загрузка).
    void reset();

    // enabled — игрока вызвали на проверку; active — игрок сейчас за монитором.
    void update(float dt, const core::GameState& game, const Settings& settings, bool enabled,
                bool active, audio::Audio& sound);
    void draw(gfx::Renderer& r, const core::GameState& game, const Settings& settings, float time,
              bool enabled, bool active);

    // --- ввод в режиме монитора ---
    void moveCursor(float dx, float dy);
    void pressNumber(int slot, audio::Audio& sound);            // клавиши 1 / 2 / 3
    void pressEnter(core::GameState& game, audio::Audio& sound); // допечатать или подтвердить
    void click(audio::Audio& sound);                             // ЛКМ: выбрать ответ
    void pressReplay(const core::GameState& game, audio::Audio& sound);

    // true один раз после каждого подтверждённого ответа (игра сохраняется и
    // проверяет, не пора ли начинать смену).
    bool takeConfirmEvent();

private:
    bool typingDone() const { return typed_ >= total_; }
    bool acceptsInput(const core::GameState& game) const;

    float typed_ = 0;     // сколько символов уже «напечатано»
    int total_ = 1 << 20; // сколько символов на экране всего (узнаём при отрисовке)
    int selected_ = -1;   // выбранный слот ответа
    int shownTask_ = -1;  // для какого задания идёт печать
    float flash_ = 0;     // пауза «Ответ принят»
    float tick_ = 0;
    float cursorX_ = kWidth * 0.5f, cursorY_ = kHeight * 0.6f;
    float voiceTimer_ = 0; // запись голоса проигрывается
    bool confirmEvent_ = false;
    bool enabled_ = false;
    bool testDoneShown_ = false;
    // Области экрана для мыши; заполняются при отрисовке.
    float optionY_[3] = {0, 0, 0};
    float optionH_ = 0;
    float replayBox_[4] = {0, 0, 0, 0};
    bool replayVisible_ = false;
    bool replayKindVitals_ = false; // кнопка повтора у датчика пульса, а не у записи голоса
};

} // namespace game
