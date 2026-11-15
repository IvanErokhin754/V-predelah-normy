#include "terminal.h"

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <string>

namespace game {

namespace {

// Сетка экрана: символ 16x32 px (шрифт 8x16 с увеличением вдвое).
const float kScale = 2.0f;
float colX(int col) { return 32.0f + col * 16.0f; }
float rowY(int row) { return 16.0f + row * 32.0f; }

// Зелёный люминофор: #79FF8E и его приглушённый вариант.
const gfx::Rgba kBright{0.475f, 1.0f, 0.557f, 1.0f};
const gfx::Rgba kDim{0.26f, 0.60f, 0.33f, 1.0f};
const gfx::Rgba kFaint{0.14f, 0.34f, 0.19f, 1.0f};
const gfx::Rgba kInk{0.027f, 0.063f, 0.035f, 1.0f}; // цвет фона — для текста на подсветке

const float kCharsPerSecond = 40.0f; // ТЗ: 35–45 символов в секунду
const float kVoiceSeconds = 1.9f;
const float kFlatlineSeconds = 3.0f;

// Первые n символов строки UTF-8.
std::string prefix(const std::string& s, int n) {
    size_t i = 0;
    int count = 0;
    while (i < s.size()) {
        if ((static_cast<unsigned char>(s[i]) & 0xC0) != 0x80) {
            if (count == n) break;
            ++count;
        }
        ++i;
    }
    return s.substr(0, i);
}

// Печатает текст с учётом «бюджета» посимвольной печати: всё, что рисуется
// через put(), появляется на экране по порядку вызовов.
struct Printer {
    gfx::Renderer& r;
    int budget;
    int count = 0;

    bool open() const { return count < budget; }

    void put(int col, int row, const std::string& s, const gfx::Rgba& c) {
        const int n = font::length(s);
        const int visible = std::clamp(budget - count, 0, n);
        if (visible > 0) r.text(colX(col), rowY(row), kScale, c, visible == n ? s : prefix(s, visible));
        count += n;
    }
};

void separator(gfx::Renderer& r, int row) { r.rect(32, rowY(row) + 15, 960, 2, kFaint); }

void drawCards(gfx::Renderer& r, Printer& p, const core::Task& t) {
    p.put(0, 5, "В двух секторах обнаружены карточки с одним номером.", kDim);
    for (int i = 0; i < 2; ++i) {
        const int col = 1 + i * 30;
        if (p.open()) r.frame(colX(col) - 12, rowY(7) - 8, 28 * 16 + 8, 6 * 32 + 16, 2, kDim);
        p.put(col, 7, i == 0 ? "КАРТОЧКА ПЛЕННОГО · А" : "КАРТОЧКА ПЛЕННОГО · Б", kDim);
        p.put(col, 9, "НОМЕР   " + t.cardNumber, kBright);
        p.put(col, 10, "СЕКТОР  " + t.cardSector[i], kBright);
        p.put(col, 11, "ЗАПИСЬ  " + t.cardDate[i], kBright);
        p.put(col, 12, "СТАТУС  АКТИВНА", kBright);
    }
}

void drawGauge(gfx::Renderer& r, Printer& p, const core::Task& t) {
    p.put(0, 5, "Фото прибора и запись в журнале смены.", kDim);

    // «Фотография» прибора: шкала 0–100 на 270 градусов и стрелка.
    if (p.open()) {
        const float cx = 224, cy = 356, radius = 128;
        r.rect(64, 200, 320, 292, gfx::Rgba{0.05f, 0.11f, 0.065f, 1});
        r.frame(64, 200, 320, 292, 2, kDim);
        r.text(72, 204, 1, kDim, "ФОТО ПРИБОРА");
        auto point = [&](float value, float rad, float* x, float* y) {
            const float angle = (225.0f - value * 2.7f) * kPi / 180.0f;
            *x = cx + rad * std::cos(angle);
            *y = cy - rad * std::sin(angle);
        };
        for (int v = 0; v < 100; v += 2) { // дуга шкалы
            float x0, y0, x1, y1;
            point(static_cast<float>(v), radius, &x0, &y0);
            point(static_cast<float>(v + 2), radius, &x1, &y1);
            r.line(x0, y0, x1, y1, 3, kDim);
        }
        for (int v = 0; v <= 100; v += 5) { // деления
            float x0, y0, x1, y1;
            const bool major = v % 10 == 0;
            point(static_cast<float>(v), radius, &x0, &y0);
            point(static_cast<float>(v), radius * (major ? 0.82f : 0.90f), &x1, &y1);
            r.line(x0, y0, x1, y1, major ? 3.0f : 2.0f, major ? kBright : kDim);
            if (v % 20 == 0) {
                float tx, ty;
                point(static_cast<float>(v), radius * 0.64f, &tx, &ty);
                r.textCentered(tx, ty - 8, 1, kBright, std::to_string(v));
            }
        }
        float nx, ny;
        point(static_cast<float>(t.gaugeNeedle), radius * 0.78f, &nx, &ny);
        r.line(cx, cy, nx, ny, 5, kBright);
        r.rect(cx - 7, cy - 7, 14, 14, kBright);
        r.textCentered(cx, cy + 62, 1, kDim, t.gaugeUnit);
    }

    p.put(24, 7, t.gaugeDevice, kBright);
    p.put(24, 8, t.gaugeArea, kDim);
    if (p.open()) r.frame(colX(24) - 12, rowY(10) - 8, 34 * 16, 2 * 32 + 20, 2, kDim);
    p.put(24, 10, "ЖУРНАЛ СМЕНЫ", kDim);
    p.put(24, 11, "Показание: " + std::to_string(t.gaugeJournal) + " " + t.gaugeUnit, kBright);
    p.put(24, 13, "Запись внесена оператором.", kDim);
}

// Г. Журнал выдачи пайков и фото лотка у двери камеры.
void drawRations(gfx::Renderer& r, Printer& p, const core::Task& t) {
    p.put(0, 5, "КАМЕРА " + std::to_string(t.rationCell) + " · ЖУРНАЛ ВЫДАЧИ ПАЙКОВ", kDim);
    for (int d = 0; d < t.rationDays; ++d) {
        const std::string day = "ДЕНЬ -" + std::to_string(t.rationDays - d);
        p.put(0, 7 + d, day + "   ВЫДАН 06:00", kBright);
    }
    p.put(0, 7 + t.rationDays + 1, "Посл. открытие двери:", kDim);
    p.put(0, 8 + t.rationDays + 1, t.rationLastDoor, kDim);
    if (p.open()) {
        // «Фото» лотка: все пайки на месте, обёртки целы.
        const float x0 = 560, y0 = 200, w = 400, h = 270;
        r.rect(x0, y0, w, h, gfx::Rgba{0.05f, 0.11f, 0.065f, 1});
        r.frame(x0, y0, w, h, 2, kDim);
        r.text(x0 + 8, y0 + 4, 1, kDim, "ФОТО ЛОТКА · КАМЕРА " + std::to_string(t.rationCell));
        r.frame(x0 + 40, y0 + 60, w - 80, h - 90, 3, kDim); // лоток
        for (int i = 0; i < t.rationDays; ++i) {
            const float bx = x0 + 60 + (i % 3) * 100, by = y0 + 80 + (i / 3) * 80;
            r.rect(bx, by, 80, 56, kDim);
            r.frame(bx, by, 80, 56, 2, kBright);
            r.line(bx, by + 28, bx + 80, by + 28, 2, kFaint); // целая обёртка
        }
    }
    p.put(36, 14, "Лоток: не тронут", kBright);
}

// Д. Снимок камеры наблюдения: людей на одного больше, чем в списке.
void drawPhoto(gfx::Renderer& r, Printer& p, const core::Task& t) {
    p.put(0, 5, t.photoCamera + " · " + t.photoTime + " · СПИСОК: " + std::to_string(t.photoListed) + " ЧЕЛ.", kDim);
    if (p.open()) {
        const float x0 = 48, y0 = 218, w = 640, h = 258;
        r.rect(x0, y0, w, h, gfx::Rgba{0.04f, 0.09f, 0.05f, 1});
        r.frame(x0, y0, w, h, 2, kDim);
        const float wallY = y0 + 140; // линия, где стена встречается с полом
        r.rect(x0 + 2, y0 + 2, w - 4, wallY - y0 - 2, gfx::Rgba{0.06f, 0.13f, 0.075f, 1});
        r.line(x0, wallY, x0 + w, wallY, 2, kFaint);
        for (int i = 0; i < 6; ++i) r.line(x0 + 60 + i * 104, wallY, x0 + 20 + i * 120, y0 + h, 1, kFaint);
        r.text(x0 + 8, y0 + 6, 1, kDim, "REC " + t.photoTime);
        const float gap = (w - 80) / static_cast<float>(t.photoPeople);
        for (int i = 0; i < t.photoPeople; ++i) {
            const bool extra = i == t.photoExtra;
            const float cx = x0 + 40 + gap * (i + 0.5f);
            // Лишний стоит вплотную к стене, спиной к камере: мельче и без лица.
            const float k = extra ? 0.72f : 1.0f;
            const float feet = extra ? wallY + 6 : y0 + h - 30 - (i % 2) * 14;
            const gfx::Rgba body = extra ? kFaint : kDim;
            r.rect(cx - 16 * k, feet - 70 * k, 32 * k, 48 * k, body);           // туловище
            r.rect(cx - 13 * k, feet - 22 * k, 10 * k, 22 * k, body);           // ноги
            r.rect(cx + 3 * k, feet - 22 * k, 10 * k, 22 * k, body);
            r.rect(cx - 11 * k, feet - 94 * k, 22 * k, 22 * k, extra ? kDim : kBright); // голова
            if (!extra) {
                r.rect(cx - 6, feet - 87, 3, 3, kInk); // глаза
                r.rect(cx + 3, feet - 87, 3, 3, kInk);
            }
        }
    }
    if (p.open()) r.frame(colX(44) - 8, rowY(6) - 8, 15 * 16 + 16, 8 * 32 + 8, 2, kDim);
    p.put(44, 6, "СПИСОК", kDim);
    // Номера в списке — из времени снимка, чтобы не менялись между кадрами.
    int seed = 0;
    for (char c : t.photoTime + t.photoCamera) seed = seed * 31 + static_cast<unsigned char>(c);
    for (int i = 0; i < t.photoListed; ++i) {
        seed = seed * 1103515245 + 12345;
        const int number = 1000 + ((seed >> 8) & 0x7FFF) % 9000;
        p.put(44, 8 + i, std::to_string(i + 1) + ". № " + std::to_string(number), kBright);
    }
}

// Е. Лента датчика пульса: удары, потом прямая.
void drawVitals(gfx::Renderer& r, Printer& p, const core::Task& t, float replay) {
    p.put(0, 5, "ПЛЕННЫЙ № " + std::to_string(t.vitalsPrisoner) + " · ДАТЧИК ПУЛЬСА", kDim);
    if (p.open()) {
        const float x0 = 48, y0 = 208, w = 928, h = 150, mid = y0 + 90;
        r.rect(x0, y0, w, h, gfx::Rgba{0.04f, 0.09f, 0.05f, 1});
        r.frame(x0, y0, w, h, 2, kFaint);
        for (int gx = 1; gx < 16; ++gx) r.line(x0 + gx * 58, y0, x0 + gx * 58, y0 + h, 1, kFaint);
        const float flatX = x0 + w * t.vitalsFlatAt / 100.0f;
        float px = x0, py = mid;
        for (float x = x0; x < x0 + w; x += 4) {
            float y = mid;
            if (x < flatX) {
                const float local = std::fmod(x - x0, 72.0f);
                if (local > 30 && local < 34) y = mid - 58;
                else if (local >= 34 && local < 38) y = mid + 26;
                else if (local > 46 && local < 56) y = mid - 10;
            }
            const bool lit = replay >= 0 && (x - x0) / w <= replay;
            r.line(px, py, x, y, 3, lit ? kBright : kDim);
            px = x;
            py = y;
        }
        r.line(flatX, y0 + 6, flatX, y0 + h - 6, 1, kDim);
        r.text(flatX + 6, y0 + 6, 1, kBright, t.vitalsFlatTime);
    }
    if (p.open()) r.frame(colX(0) - 12, rowY(11) - 8, 44 * 16, 2 * 32 + 20, 2, kDim);
    p.put(0, 11, "ЖУРНАЛ " + t.vitalsJournalTime + "  Состояние: стабильно", kBright);
    p.put(0, 12, "Запись внесена автоматически.", kDim);
}

// Ж. Записка из камеры: листок, неровные строки.
void drawNote(gfx::Renderer& r, Printer& p, const core::Task& t) {
    p.put(0, 5, "КАМЕРА " + std::to_string(t.noteCell) + " · НАЙДЕНО ПРИ ОБЫСКЕ", kDim);
    if (p.open()) {
        const float x0 = 112, y0 = 212, w = 800, h = 236;
        r.rect(x0, y0, w, h, gfx::Rgba{0.06f, 0.13f, 0.075f, 1});
        // Рваный нижний край.
        for (int i = 0; i < 20; ++i) {
            const float x = x0 + i * 40;
            r.line(x, y0 + h, x + 20, y0 + h - 10, 2, kFaint);
            r.line(x + 20, y0 + h - 10, x + 40, y0 + h, 2, kFaint);
        }
        r.frame(x0, y0, w, h, 2, kFaint);
        r.text(x0 + w - 150, y0 + 8, 1, kFaint, "ДЕНЬ " + std::to_string(t.noteDay));
    }
    const std::vector<std::string> lines = font::wrap(t.noteText, 42);
    for (size_t i = 0; i < lines.size() && i < 4; ++i)
        p.put(9 + static_cast<int>(i % 2), 8 + static_cast<int>(i) * 2 - static_cast<int>(i / 2), lines[i], kBright);
}

} // namespace

void Terminal::reset() {
    typed_ = 0;
    total_ = 1 << 20;
    selected_ = -1;
    shownTask_ = -1;
    flash_ = 0;
    voiceTimer_ = 0;
    confirmEvent_ = false;
    testDoneShown_ = false;
}

bool Terminal::acceptsInput(const core::GameState& game) const {
    return enabled_ && flash_ <= 0 && !game.testDone() && shownTask_ == game.currentTask();
}

void Terminal::update(float dt, const core::GameState& game, const Settings& settings, bool enabled,
                      bool active, audio::Audio& sound) {
    enabled_ = enabled;
    if (flash_ > 0) flash_ -= dt;
    if (voiceTimer_ > 0) voiceTimer_ -= dt;

    // Сменилось задание — начинаем печать нового экрана.
    if (flash_ <= 0 && shownTask_ != game.currentTask()) {
        shownTask_ = game.currentTask();
        typed_ = 0;
        total_ = 1 << 20;
        selected_ = -1;
    }

    if (!enabled || !active || flash_ > 0 || typingDone()) return;
    if (!settings.typewriter) {
        typed_ = static_cast<float>(total_);
        return;
    }
    typed_ += kCharsPerSecond * dt;
    tick_ -= dt;
    if (tick_ <= 0) {
        sound.play(audio::KeyTick, 0.25f);
        tick_ = 0.075f;
    }
}

void Terminal::moveCursor(float dx, float dy) {
    cursorX_ = std::clamp(cursorX_ + dx, 8.0f, kWidth - 8.0f);
    cursorY_ = std::clamp(cursorY_ + dy, 8.0f, kHeight - 8.0f);
}

void Terminal::pressNumber(int slot, audio::Audio& sound) {
    if (slot < 0 || slot > 2 || !enabled_ || flash_ > 0 || shownTask_ >= core::GameState::kTaskCount) return;
    if (!typingDone()) return; // варианты ещё не напечатаны
    if (selected_ != slot) sound.play(audio::UiSelect, 0.6f);
    selected_ = slot;
}

void Terminal::pressEnter(core::GameState& game, audio::Audio& sound) {
    if (!enabled_ || flash_ > 0) return;
    if (!typingDone()) { // первое нажатие показывает текст целиком
        typed_ = static_cast<float>(total_);
        return;
    }
    if (!acceptsInput(game) || selected_ < 0) return;
    // Только подтверждение меняет итог; ядро не примет ответ дважды.
    if (game.confirmAnswer(shownTask_, selected_)) {
        sound.play(audio::UiConfirm, 0.7f);
        flash_ = 0.9f;
        confirmEvent_ = true;
        selected_ = -1;
    }
}

void Terminal::click(audio::Audio& sound) {
    if (!enabled_ || flash_ > 0 || !typingDone()) return;
    if (replayVisible_ && cursorX_ >= replayBox_[0] && cursorX_ <= replayBox_[0] + replayBox_[2] &&
        cursorY_ >= replayBox_[1] && cursorY_ <= replayBox_[1] + replayBox_[3]) {
        replayKindVitals_ ? sound.play(audio::Flatline, 0.8f) : sound.play(audio::Voice, 0.9f);
        voiceTimer_ = replayKindVitals_ ? kFlatlineSeconds : kVoiceSeconds;
        return;
    }
    if (shownTask_ >= core::GameState::kTaskCount) return;
    for (int i = 0; i < 3; ++i) {
        if (cursorY_ >= optionY_[i] && cursorY_ <= optionY_[i] + optionH_) {
            if (selected_ != i) sound.play(audio::UiSelect, 0.6f);
            selected_ = i;
            return;
        }
    }
}

void Terminal::pressReplay(const core::GameState& game, audio::Audio& sound) {
    if (!enabled_ || flash_ > 0 || !typingDone() || game.testDone()) return;
    const core::TaskKind kind = game.task(game.currentTask()).kind;
    if (kind == core::TaskKind::Sector) {
        sound.play(audio::Voice, 0.9f);
        voiceTimer_ = kVoiceSeconds;
    } else if (kind == core::TaskKind::Vitals) {
        sound.play(audio::Flatline, 0.8f);
        voiceTimer_ = kFlatlineSeconds;
    }
}

bool Terminal::takeConfirmEvent() {
    const bool e = confirmEvent_;
    confirmEvent_ = false;
    return e;
}

void Terminal::draw(gfx::Renderer& r, const core::GameState& game, const Settings& settings,
                    float time, bool enabled, bool active) {
    r.crtBackground(time, settings.ripple);
    r.begin2D(kWidth, kHeight);
    replayVisible_ = false;

    // Заголовок виден всегда.
    char head[64];
    std::snprintf(head, sizeof(head), "ДЕНЬ %03d", game.day());
    r.text(colX(0), rowY(0), kScale, kDim, "КОМПЛЕКС-7 · СВЕРКА ЗАПИСЕЙ");
    r.text(colX(60 - font::length(head)), rowY(0), kScale, kDim, head);
    separator(r, 1);

    Printer p{r, static_cast<int>(typed_)};
    const int taskIndex = shownTask_;

    if (!enabled) {
        // Терминал не ждёт игрока: дежурный экран.
        r.textCentered(kWidth * 0.5f, rowY(9), kScale, kDim, "ТЕРМИНАЛ ЗАБЛОКИРОВАН");
        r.textCentered(kWidth * 0.5f, rowY(11), kScale, kFaint, "Ожидайте вызова");
    } else if (flash_ > 0) {
        r.frame(kWidth * 0.5f - 200, rowY(9) - 16, 400, 64, 2, kBright);
        r.textCentered(kWidth * 0.5f, rowY(9), kScale, kBright, "ОТВЕТ ПРИНЯТ");
    } else if (taskIndex >= core::GameState::kTaskCount || taskIndex < 0) {
        p.put(0, 8, "Ответы приняты.", kBright);
        p.put(0, 10, "Ожидайте следующего вызова.", kBright);
        total_ = p.count;
        if (typingDone() && std::fmod(time, 1.0f) < 0.5f) r.rect(colX(27), rowY(10) + 4, 14, 26, kBright);
        if (active) r.text(colX(0), rowY(22), kScale, kFaint, "[Esc] отойти от терминала");
    } else {
        const core::Task& t = game.task(taskIndex);
        char num[32];
        std::snprintf(num, sizeof(num), "ЗАДАНИЕ %d/%d", taskIndex + 1, core::GameState::kTaskCount);
        r.text(colX(60 - font::length(num)), rowY(2), kScale, kDim, num);

        // Приказ виден над материалами до подтверждения ответа.
        p.put(0, 2, "РАСПОРЯЖЕНИЕ:", kDim);
        p.put(0, 3, "> " + t.order, kBright);
        separator(r, 4);

        auto replayButton = [&](int col, int row, const std::string& label, bool vitals) {
            if (p.open()) {
                replayBox_[0] = colX(col) - 10;
                replayBox_[1] = rowY(row) - 4;
                replayBox_[2] = font::length(label) * 16 + 20;
                replayBox_[3] = 40;
                replayVisible_ = true;
                replayKindVitals_ = vitals;
                const bool hover = active && cursorX_ >= replayBox_[0] && cursorX_ <= replayBox_[0] + replayBox_[2] &&
                                   cursorY_ >= replayBox_[1] && cursorY_ <= replayBox_[1] + replayBox_[3];
                r.frame(replayBox_[0], replayBox_[1], replayBox_[2], replayBox_[3], 2, hover ? kBright : kDim);
            }
            p.put(col, row, label, voiceTimer_ > 0 ? kBright : kDim);
        };
        if (t.kind == core::TaskKind::Cards) {
            drawCards(r, p, t);
        } else if (t.kind == core::TaskKind::Gauge) {
            drawGauge(r, p, t);
        } else if (t.kind == core::TaskKind::Rations) {
            drawRations(r, p, t);
        } else if (t.kind == core::TaskKind::Headcount) {
            drawPhoto(r, p, t);
        } else if (t.kind == core::TaskKind::Vitals) {
            const float progress = voiceTimer_ > 0 ? 1.0f - voiceTimer_ / kFlatlineSeconds : -1.0f;
            drawVitals(r, p, t, progress);
            replayButton(38, 14, "[R] ЗВУК ДАТЧИКА", true);
        } else if (t.kind == core::TaskKind::Note) {
            drawNote(r, p, t);
        } else {
            p.put(0, 5, "СЕКТОР " + t.sectorCode, kBright);
            p.put(22, 5, "СТАТУС: ПУСТ", kBright);
            p.put(0, 7, "ЖУРНАЛ ДВЕРЕЙ    " + t.sectorDoorTime + "  дверь открыта", kDim);
            p.put(0, 8, "ЗАПИСЬ ИЗ СЕКТОРА " + t.sectorRecTime + "  длительность 00:02", kDim);
            if (p.open()) {
                // Осциллограмма записи. Высота столбиков — псевдослучайная, но
                // «слоги» читаются: это речь, а не ровный шум.
                const float x0 = 64, y0 = 384;
                r.frame(48, 316, 688, 136, 2, kFaint);
                const float progress = voiceTimer_ > 0 ? 1.0f - voiceTimer_ / kVoiceSeconds : -1.0f;
                for (int i = 0; i < 82; ++i) {
                    const float u = i / 82.0f;
                    const float syllable = std::fabs(std::sin(u * kPi * 7.0f));
                    const float jitter = 0.55f + 0.45f * std::sin(i * 12.9898f) * std::sin(i * 4.1f);
                    const float h = 6 + 52 * syllable * std::fabs(jitter);
                    const bool played = progress >= 0 && u <= progress;
                    r.rect(x0 + i * 8, y0 - h, 5, h * 2, played ? kBright : kDim);
                }
            }
            p.put(0, 14, "«Разборчивый голос»", kBright);
            if (p.open()) {
                replayBox_[0] = colX(25) - 10;
                replayBox_[1] = rowY(14) - 4;
                replayBox_[2] = 18 * 16 + 20;
                replayBox_[3] = 40;
                replayVisible_ = true;
                replayKindVitals_ = false;
                const bool hover = active && cursorX_ >= replayBox_[0] &&
                                   cursorX_ <= replayBox_[0] + replayBox_[2] &&
                                   cursorY_ >= replayBox_[1] && cursorY_ <= replayBox_[1] + replayBox_[3];
                r.frame(replayBox_[0], replayBox_[1], replayBox_[2], replayBox_[3], 2, hover ? kBright : kDim);
            }
            p.put(25, 14, "[R] ПОВТОР ЗАПИСИ", voiceTimer_ > 0 ? kBright : kDim);
        }
        separator(r, 15);

        // Три варианта ответа. Выбранный отмечается сплошным фоном.
        optionH_ = 44;
        for (int i = 0; i < 3; ++i) {
            const int row = 16 + i * 2;
            optionY_[i] = rowY(row) - 6;
            const bool chosen = selected_ == i;
            const bool hover = active && typingDone() && cursorY_ >= optionY_[i] &&
                               cursorY_ <= optionY_[i] + optionH_;
            if (chosen) r.rect(24, optionY_[i], 976, optionH_, kBright);
            else if (hover && p.open() == false) r.frame(24, optionY_[i], 976, optionH_, 2, kFaint);
            const std::string line = "[" + std::to_string(i + 1) + "] " + t.answers[i].text;
            p.put(0, row, line, chosen ? kInk : kBright);
        }
        total_ = p.count;

        if (typingDone()) {
            r.text(colX(0), rowY(22), kScale, kFaint,
                   selected_ < 0 ? "[1][2][3] выбор   [Esc] отойти"
                                 : "[Enter] подтвердить   [1][2][3] выбор   [Esc] отойти");
        } else {
            r.text(colX(0), rowY(22), kScale, kFaint, "[Enter] показать текст целиком");
        }
    }

    // Курсор мыши — только когда игрок за монитором.
    if (active && enabled) {
        const float x = cursorX_, y = cursorY_;
        r.line(x, y, x, y + 20, 3, kBright);
        r.line(x, y, x + 14, y + 14, 3, kBright);
        r.line(x, y + 20, x + 6, y + 14, 3, kBright);
        r.line(x + 6, y + 14, x + 14, y + 14, 3, kBright);
    }
    r.end2D();
}

} // namespace game
