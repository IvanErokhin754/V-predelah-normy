#include "audio.h"

#include <algorithm>
#include <cmath>
#include <cstdint>

namespace audio {

namespace {

const int kRate = 44100;
const float kTau = 6.2831853f;

struct Noise {
    uint32_t s;
    explicit Noise(uint32_t seed) : s(seed) {}
    float next() { // белый шум в [-1, 1]
        s = s * 1664525u + 1013904223u;
        return ((s >> 8) & 0xFFFF) / 32768.0f - 1.0f;
    }
};

std::vector<float> silence(float seconds) {
    return std::vector<float>(static_cast<size_t>(seconds * kRate), 0.0f);
}

float timeOf(size_t i) { return static_cast<float>(i) / kRate; }

// Резонансный фильтр (полосовой): выделяет полосу вокруг freq.
struct Resonator {
    float low = 0, band = 0;
    float process(float in, float freq, float q) {
        const float f = 2.0f * std::sin(3.14159265f * freq / kRate);
        low += f * band;
        const float high = in - low - q * band;
        band += f * high;
        return band;
    }
};

// Делает шумовой буфер бесшовным: хвост плавно перетекает в начало.
void makeSeamless(std::vector<float>& b, size_t fade) {
    const size_t n = b.size() - fade;
    for (size_t i = 0; i < fade; ++i) {
        const float t = static_cast<float>(i) / fade;
        b[i] = b[i] * t + b[n + i] * (1 - t);
    }
    b.resize(n);
}

void addThump(std::vector<float>& b, float start, float freq, float decay, float amp) {
    const size_t i0 = static_cast<size_t>(start * kRate);
    for (size_t i = i0; i < b.size(); ++i) {
        const float t = timeOf(i - i0);
        const float env = std::exp(-t / decay);
        if (env < 0.001f) break;
        b[i] += amp * env * std::sin(kTau * freq * t * (1.0f + 0.4f * env));
    }
}

void addClick(std::vector<float>& b, float start, float decay, float amp, uint32_t seed) {
    Noise n(seed);
    const size_t i0 = static_cast<size_t>(start * kRate);
    for (size_t i = i0; i < b.size(); ++i) {
        const float env = std::exp(-timeOf(i - i0) / decay);
        if (env < 0.001f) break;
        b[i] += amp * env * n.next();
    }
}

void addRing(std::vector<float>& b, float start, float freq, float decay, float amp) {
    const size_t i0 = static_cast<size_t>(start * kRate);
    for (size_t i = i0; i < b.size(); ++i) {
        const float t = timeOf(i - i0);
        const float env = std::exp(-t / decay);
        if (env < 0.001f) break;
        b[i] += amp * env * std::sin(kTau * freq * t);
    }
}

std::vector<float> genLampHum() {
    auto b = silence(2.0f);
    Noise n(1);
    float crackle = 0;
    for (size_t i = 0; i < b.size(); ++i) {
        const float t = timeOf(i);
        const float base = std::sin(kTau * 100 * t);
        float v = 0.20f * base + 0.10f * std::sin(kTau * 200 * t) + 0.05f * std::sin(kTau * 300 * t);
        v += 0.10f * std::tanh(4.0f * base); // жужжание дросселя
        crackle = crackle * 0.9f + 0.1f * n.next();
        v += 0.05f * crackle * (0.5f + 0.5f * std::sin(kTau * 3.5f * t));
        b[i] = v;
    }
    return b;
}

std::vector<float> genMonitorHum() {
    auto b = silence(2.0f);
    for (size_t i = 0; i < b.size(); ++i) {
        const float t = timeOf(i);
        b[i] = 0.10f * std::sin(kTau * 50 * t) + 0.05f * std::sin(kTau * 150 * t) +
               0.030f * std::sin(kTau * 7800 * t) + 0.012f * std::sin(kTau * 11700 * t);
    }
    return b;
}

std::vector<float> genDrone() {
    const float seconds = 8.0f;
    const size_t fade = kRate / 2;
    std::vector<float> b(static_cast<size_t>(seconds * kRate) + fade);
    Noise n(2);
    float lp = 0;
    for (size_t i = 0; i < b.size(); ++i) {
        const float t = timeOf(i);
        const float am = 0.6f + 0.4f * std::sin(kTau * 0.125f * t);
        float v = 0.22f * am * std::sin(kTau * 41.0f * t) + 0.12f * std::sin(kTau * 61.5f * t) +
                  0.05f * std::sin(kTau * 82.25f * t + 1.3f * std::sin(kTau * 0.25f * t));
        lp += 0.004f * (n.next() - lp);
        v += 2.2f * lp;
        b[i] = v;
    }
    makeSeamless(b, fade);
    return b;
}

std::vector<float> genWind() {
    const size_t fade = kRate / 2;
    std::vector<float> b(static_cast<size_t>(6.0f * kRate) + fade);
    Noise n(3);
    Resonator r;
    for (size_t i = 0; i < b.size(); ++i) {
        const float t = timeOf(i);
        const float freq = 420 + 200 * std::sin(kTau * 0.17f * t) + 90 * std::sin(kTau * 0.41f * t);
        b[i] = 0.5f * r.process(n.next(), freq, 0.9f) * (0.6f + 0.4f * std::sin(kTau * 0.11f * t));
    }
    makeSeamless(b, fade);
    return b;
}

std::vector<float> genLockClick() {
    auto b = silence(0.32f);
    addClick(b, 0.0f, 0.003f, 0.6f, 11);
    addRing(b, 0.0f, 1900, 0.02f, 0.2f);
    addClick(b, 0.09f, 0.005f, 0.9f, 12);
    addRing(b, 0.09f, 2600, 0.03f, 0.25f);
    addThump(b, 0.09f, 140, 0.05f, 0.7f);
    return b;
}

std::vector<float> genDoorSlide() {
    auto b = silence(1.2f);
    Noise n(21);
    float lp = 0;
    for (size_t i = 0; i < b.size(); ++i) {
        const float t = timeOf(i);
        const float env = smooth01(t / 0.1f) * (1.0f - smooth01((t - 0.85f) / 0.15f));
        lp += 0.05f * (n.next() - lp);
        const float grind = std::tanh(3.0f * std::sin(kTau * 68 * t)) * 0.12f;
        b[i] = env * (1.6f * lp + grind * (0.6f + 0.4f * std::sin(kTau * 9 * t)));
    }
    addThump(b, 0.93f, 110, 0.07f, 0.9f);
    addClick(b, 0.93f, 0.006f, 0.6f, 22);
    return b;
}

std::vector<float> genHeavyDoor() {
    auto b = silence(2.6f);
    Noise n(31);
    Resonator screech;
    float phase = 0;
    for (size_t i = 0; i < b.size(); ++i) {
        const float t = timeOf(i);
        const float env = smooth01(t / 0.25f) * (1.0f - smooth01((t - 1.7f) / 0.3f));
        const float freq = 92 - 26 * (t / 2.0f) + 5 * std::sin(kTau * 2.3f * t);
        phase += kTau * freq / kRate;
        const float groan = std::tanh(2.5f * std::sin(phase)) * 0.35f;
        const float sweep = 1300 - 500 * (t / 2.0f) + 160 * std::sin(kTau * 5.1f * t);
        const float metal = screech.process(n.next(), sweep, 0.12f) * 0.10f;
        b[i] = env * (groan + metal);
    }
    addThump(b, 1.95f, 48, 0.28f, 1.3f);
    addClick(b, 1.95f, 0.012f, 0.8f, 32);
    addRing(b, 1.95f, 310, 0.25f, 0.18f);
    return b;
}

std::vector<float> genStepRobot(bool heavy) {
    auto b = silence(heavy ? 0.5f : 0.3f);
    addThump(b, 0.0f, heavy ? 52 : 82, heavy ? 0.11f : 0.06f, heavy ? 1.1f : 0.8f);
    addClick(b, 0.0f, 0.004f, heavy ? 0.5f : 0.35f, heavy ? 41 : 42);
    // Короткий визг сервопривода.
    const size_t n = static_cast<size_t>((heavy ? 0.22f : 0.14f) * kRate);
    float phase = 0;
    for (size_t i = 0; i < n && i < b.size(); ++i) {
        const float t = static_cast<float>(i) / n;
        phase += kTau * (heavy ? 380 + 160 * t : 620 + 300 * t) / kRate;
        b[i] += 0.07f * std::sin(kTau * 0.5f * t) * std::sin(phase);
    }
    if (heavy) addRing(b, 0.02f, 740, 0.09f, 0.12f);
    return b;
}

std::vector<float> genStepPlayer() {
    auto b = silence(0.14f);
    Noise n(51);
    float lp = 0;
    for (size_t i = 0; i < b.size(); ++i) {
        lp += 0.12f * (n.next() - lp);
        b[i] = lp * std::exp(-timeOf(i) / 0.03f) * 1.4f;
    }
    addThump(b, 0.0f, 95, 0.03f, 0.35f);
    return b;
}

std::vector<float> genKnock() {
    auto b = silence(1.0f);
    for (float start : {0.0f, 0.34f}) {
        addThump(b, start, 150, 0.045f, 1.0f);
        addRing(b, start, 247, 0.06f, 0.4f);
        addClick(b, start, 0.004f, 0.5f, 61);
    }
    return b;
}

// Импульсы «голосовых связок», пропущенные через два резонатора-форманты.
std::vector<float> genVoice(bool robotic) {
    const float seconds = robotic ? 0.9f : 1.9f;
    auto b = silence(seconds);
    // Пары формант: примерно гласные а, и, о, э, у.
    const float f1[] = {730, 300, 520, 560, 320};
    const float f2[] = {1150, 2250, 900, 1800, 800};
    const int syllables = robotic ? 5 : 7;
    const float syllableLen = seconds / syllables;
    Resonator r1, r2;
    Noise n(robotic ? 71 : 72);
    float phase = 0;
    uint32_t pick = robotic ? 5u : 9u;
    for (size_t i = 0; i < b.size(); ++i) {
        const float t = timeOf(i);
        const int syl = std::min(static_cast<int>(t / syllableLen), syllables - 1);
        const float local = (t - syl * syllableLen) / syllableLen;
        const uint32_t h = (pick + static_cast<uint32_t>(syl) * 2654435761u) >> 8;
        const int vowel = static_cast<int>(h % 5);
        float pitch = robotic ? 96.0f : 118.0f + 22.0f * std::sin(syl * 1.7f) - 14.0f * local;
        phase += pitch / kRate;
        if (phase >= 1.0f) phase -= 1.0f;
        float src = (1.0f - 2.0f * phase);                      // пила
        if (robotic) src = phase < 0.5f ? 0.8f : -0.8f;         // у машины — меандр
        src += 0.08f * n.next();
        const float env = std::sin(3.14159265f * clamp01(local * 1.15f)) * (robotic ? 0.9f : 1.0f);
        const float v = r1.process(src, f1[vowel], 0.18f) * 0.5f + r2.process(src, f2[vowel], 0.14f) * 0.3f;
        b[i] = v * env * 0.5f;
    }
    if (robotic) {
        // Огрубление: «дешёвый динамик».
        for (size_t i = 0; i < b.size(); ++i) b[i] = std::floor(b[i] * 12.0f) / 12.0f;
    }
    return b;
}

std::vector<float> genPaChime() {
    auto b = silence(0.6f);
    for (size_t i = 0; i < b.size(); ++i) {
        const float t = timeOf(i);
        const float f = t < 0.22f ? 880.0f : 660.0f;
        const float local = t < 0.22f ? t : t - 0.22f;
        b[i] = 0.35f * std::exp(-local / 0.16f) * std::tanh(2.0f * std::sin(kTau * f * t));
    }
    return b;
}

std::vector<float> genStatic() {
    auto b = silence(0.7f);
    Noise n(81);
    for (size_t i = 0; i < b.size(); ++i) {
        const float t = timeOf(i);
        const float gate = (std::sin(kTau * 37 * t) > -0.3f) ? 1.0f : 0.2f;
        b[i] = 0.5f * n.next() * gate * (t < 0.62f ? 1.0f : 0.0f); // обрыв без затухания
    }
    return b;
}

std::vector<float> genBlip(float f0, float f1, float seconds, float amp) {
    auto b = silence(seconds);
    for (size_t i = 0; i < b.size(); ++i) {
        const float t = timeOf(i);
        const float f = t < seconds * 0.5f ? f0 : f1;
        const float env = std::min(1.0f, std::min(t, seconds - t) * 400.0f);
        b[i] = amp * env * std::tanh(1.5f * std::sin(kTau * f * t));
    }
    return b;
}

std::vector<float> genKeyTick() {
    auto b = silence(0.025f);
    addClick(b, 0.0f, 0.002f, 0.5f, 91);
    addRing(b, 0.0f, 3200, 0.004f, 0.2f);
    return b;
}

std::vector<float> genButton() {
    auto b = silence(0.2f);
    addClick(b, 0.0f, 0.004f, 0.7f, 101);
    addThump(b, 0.0f, 180, 0.03f, 0.6f);
    addClick(b, 0.07f, 0.003f, 0.4f, 102);
    return b;
}

} // namespace

bool Audio::init() {
    buffers_[LampHum] = genLampHum();
    buffers_[MonitorHum] = genMonitorHum();
    buffers_[Drone] = genDrone();
    buffers_[Wind] = genWind();
    buffers_[LockClick] = genLockClick();
    buffers_[DoorSlide] = genDoorSlide();
    buffers_[HeavyDoor] = genHeavyDoor();
    buffers_[StepRobot] = genStepRobot(false);
    buffers_[StepRobotHeavy] = genStepRobot(true);
    buffers_[StepPlayer] = genStepPlayer();
    buffers_[Knock] = genKnock();
    buffers_[Voice] = genVoice(false);
    buffers_[PaChime] = genPaChime();
    buffers_[PaVoice] = genVoice(true);
    buffers_[Static] = genStatic();
    buffers_[KeyTick] = genKeyTick();
    buffers_[UiSelect] = genBlip(1100, 1100, 0.04f, 0.25f);
    buffers_[UiConfirm] = genBlip(660, 990, 0.14f, 0.3f);
    buffers_[Button] = genButton();
    buffers_[ReaderBeep] = genBlip(1800, 2400, 0.22f, 0.3f);

    SDL_AudioSpec want{};
    want.freq = kRate;
    want.format = AUDIO_S16SYS;
    want.channels = 2;
    want.samples = 1024;
    want.callback = &Audio::callback;
    want.userdata = this;
    SDL_AudioSpec have{};
    device_ = SDL_OpenAudioDevice(nullptr, 0, &want, &have, 0);
    if (device_ == 0) return false;
    SDL_PauseAudioDevice(device_, 0);
    return true;
}

void Audio::shutdown() {
    if (device_ != 0) SDL_CloseAudioDevice(device_);
    device_ = 0;
}

void Audio::setMasterVolume(float v) {
    if (device_ != 0) SDL_LockAudioDevice(device_);
    master_ = std::clamp(v, 0.0f, 1.0f);
    if (device_ != 0) SDL_UnlockAudioDevice(device_);
}

void Audio::setListener(const Vec3& pos, float yaw) {
    listenerPos_ = pos;
    listenerYaw_ = yaw;
}

void Audio::spatial(const Vec3& pos, float gain, float* left, float* right) const {
    const Vec3 d = pos - listenerPos_;
    const float dist = length(d);
    const float att = gain / (1.0f + (dist / 2.5f) * (dist / 2.5f));
    // Панорама: проекция направления на «правое ухо» слушателя.
    const Vec3 rightVec{std::cos(listenerYaw_), 0, std::sin(listenerYaw_)};
    const float pan = dist > 0.01f ? dot(d, rightVec) / dist : 0.0f;
    *left = att * (1.0f - 0.6f * std::max(pan, 0.0f));
    *right = att * (1.0f - 0.6f * std::max(-pan, 0.0f));
}

void Audio::play(Sound s, float gain) {
    if (device_ == 0) return;
    SDL_LockAudioDevice(device_);
    for (Channel& v : voices_) {
        if (v.buf != nullptr) continue;
        v.buf = &buffers_[s];
        v.pos = 0;
        v.left = v.right = gain;
        break;
    }
    SDL_UnlockAudioDevice(device_);
}

void Audio::playAt(Sound s, const Vec3& pos, float gain) {
    if (device_ == 0) return;
    float l = 0, r = 0;
    spatial(pos, gain, &l, &r);
    SDL_LockAudioDevice(device_);
    for (Channel& v : voices_) {
        if (v.buf != nullptr) continue;
        v.buf = &buffers_[s];
        v.pos = 0;
        v.left = l;
        v.right = r;
        break;
    }
    SDL_UnlockAudioDevice(device_);
}

void Audio::loop(Sound s, float gain) {
    if (device_ == 0 || s >= kLoopCount) return;
    SDL_LockAudioDevice(device_);
    loops_[s].targetLeft = loops_[s].targetRight = gain;
    SDL_UnlockAudioDevice(device_);
}

void Audio::loopAt(Sound s, const Vec3& pos, float gain) {
    if (device_ == 0 || s >= kLoopCount) return;
    float l = 0, r = 0;
    spatial(pos, gain, &l, &r);
    SDL_LockAudioDevice(device_);
    loops_[s].targetLeft = l;
    loops_[s].targetRight = r;
    SDL_UnlockAudioDevice(device_);
}

void Audio::cutAll() {
    if (device_ == 0) return;
    SDL_LockAudioDevice(device_);
    for (Channel& v : voices_) v.buf = nullptr;
    for (Loop& l : loops_) l.left = l.right = l.targetLeft = l.targetRight = 0;
    SDL_UnlockAudioDevice(device_);
}

void SDLCALL Audio::callback(void* userdata, Uint8* stream, int len) {
    static_cast<Audio*>(userdata)->mix(reinterpret_cast<Sint16*>(stream), len / 4);
}

void Audio::mix(Sint16* out, int frames) {
    for (int i = 0; i < frames; ++i) {
        float l = 0, r = 0;
        for (int s = 0; s < kLoopCount; ++s) {
            Loop& lp = loops_[s];
            lp.left += (lp.targetLeft - lp.left) * 0.0008f;
            lp.right += (lp.targetRight - lp.right) * 0.0008f;
            const std::vector<float>& b = buffers_[s];
            if (b.empty()) continue;
            const float v = b[lp.pos];
            if (++lp.pos >= b.size()) lp.pos = 0;
            l += v * lp.left;
            r += v * lp.right;
        }
        for (Channel& v : voices_) {
            if (v.buf == nullptr) continue;
            const float sample = (*v.buf)[v.pos];
            l += sample * v.left;
            r += sample * v.right;
            if (++v.pos >= v.buf->size()) v.buf = nullptr;
        }
        // Мягкое ограничение вместо хрипа при перегрузке.
        out[i * 2 + 0] = static_cast<Sint16>(std::tanh(l * master_) * 32000.0f);
        out[i * 2 + 1] = static_cast<Sint16>(std::tanh(r * master_) * 32000.0f);
    }
}

} // namespace audio
