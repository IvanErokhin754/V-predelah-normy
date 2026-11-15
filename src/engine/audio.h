// Звук: все эффекты синтезируются кодом при запуске, микшер работает
// в обратном вызове SDL. Если звукового устройства нет, игра идёт без звука.
#pragma once

#include <cstdint>
#include <string>
#include <vector>

#include "math3d.h"
#include "sdl_min.h"

namespace audio {

enum Sound {
    // зацикленные
    LampHum,    // гул лампы в камере
    MonitorHum, // гул монитора
    Drone,      // низкий гул комплекса
    Wind,       // ветер снаружи (хорошая концовка)
    kLoopCount,
    // разовые
    LockClick = kLoopCount, // щелчок замка
    DoorSlide,              // обычная дверь
    HeavyDoor,              // новый звук двери на развилке (плохая концовка)
    StepRobot,              // шаг конвоира
    StepRobotHeavy,         // тяжёлый шаг конвоира (плохая концовка)
    StepPlayer,
    Knock,      // двойной стук из пустого крыла
    Voice,      // человеческий голос в записи
    PaChime,    // сигнал громкой связи
    PaVoice,    // синтетическая «речь» комплекса
    Static,     // обрыв связи
    KeyTick,    // печать символа на терминале
    UiSelect,
    UiConfirm,
    Button,     // нажатие физической кнопки
    ReaderBeep, // считыватель пломб
    Alarm,      // предупреждение конвоира
    Screech,    // скример
    Clang,      // далёкий удар металла (атмосфера)
    PipeGroan,  // стон труб (атмосфера)
    Lever,      // рубильник
    PowerDown,  // гул обесточенного сектора
    Flatline,   // датчик пульса: три удара и ровный тон
    kSoundCount
};

class Audio {
public:
    bool init();
    void shutdown();

    void setMasterVolume(float v);
    void setListener(const Vec3& pos, float yaw);

    // Разовый звук без позиции (интерфейс, громкая связь).
    void play(Sound s, float gain = 1.0f);
    // Разовый звук в точке сцены: громкость и панорама — по положению слушателя.
    // muffle 0..1 — звук за стеной: тише и глуше (фильтр низких частот).
    void playAt(Sound s, const Vec3& pos, float gain = 1.0f, float muffle = 0.0f);

    // Зацикленные звуки: вызывается каждый кадр, громкость плавно догоняет цель.
    void loop(Sound s, float gain);
    void loopAt(Sound s, const Vec3& pos, float gain, float muffle = 0.0f);

    // Фоновая музыка (эмбиент) из WAV-файла: PCM 8/16/24/32 бит или float,
    // моно или стерео, любая частота. Возвращает false, если файла нет или
    // формат не поддерживается; тогда игра идёт без музыки.
    bool loadMusic(const std::string& path, std::string* error = nullptr);
    bool hasMusic() const { return !music_.empty(); }
    void setMusic(float gain); // громкость плавно догоняет цель

    // «Звук обрывается»: мгновенная тишина.
    void cutAll();

private:
    static void SDLCALL callback(void* userdata, Uint8* stream, int len);
    void mix(Sint16* out, int frames);
    void spatial(const Vec3& pos, float gain, float* left, float* right) const;

    struct Channel {
        const std::vector<float>* buf = nullptr;
        size_t pos = 0;
        float left = 0, right = 0;
        float lowpass = 1; // 1 — без фильтра; меньше — глуше
        float lp = 0;
    };
    struct Loop {
        size_t pos = 0;
        float left = 0, right = 0;             // текущая громкость
        float targetLeft = 0, targetRight = 0; // к чему стремимся
        float lowpass = 1, targetLowpass = 1;
        float lp = 0;
    };

    static const int kVoices = 24;
    SDL_AudioDeviceID device_ = 0;
    std::vector<float> buffers_[kSoundCount];
    Channel voices_[kVoices];
    Loop loops_[kLoopCount];
    float master_ = 0.8f;
    std::vector<int16_t> music_; // стерео, чередование L R, 44100 Гц
    size_t musicPos_ = 0;
    float musicGain_ = 0, musicTarget_ = 0;
    Vec3 listenerPos_;
    float listenerYaw_ = 0;
};

} // namespace audio
